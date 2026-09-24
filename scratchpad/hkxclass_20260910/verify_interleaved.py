import struct, sys
blob = open(sys.argv[1] if len(sys.argv)>1 else "interleaved.hkx", 'rb').read()
n, = struct.unpack_from('<i', blob, 20)
sechdr = 0x40 + struct.unpack_from('<H', blob, 0x3e)[0]
secs = {}
for s in range(n):
    off = sechdr + s*0x40
    tag = blob[off:off+19].split(b'\0')[0].decode()
    secs[tag] = struct.unpack_from('<7i', blob, off+20)
cn = secs['__classnames__']; dt = secs['__data__']
cnames = {}
p, end = cn[0], cn[0]+cn[1]
while p+5 < end:
    if blob[p+4] != 0x09: break
    sig, = struct.unpack_from('<I', blob, p)
    e = blob.index(b'\0', p+5)
    cnames[p+5-cn[0]] = (sig, blob[p+5:e].decode('latin-1'))
    p = e+1
base = dt[0]
local = {}
for p in range(base+dt[1], min(base+dt[2], len(blob)-7), 8):
    src, dst = struct.unpack_from('<ii', blob, p)
    if src != -1: local[base+src] = base+dst
objs = []
for p in range(base+dt[3], min(base+dt[4], len(blob)-11), 12):
    src, sec, cno = struct.unpack_from('<iii', blob, p)
    if src != -1: objs.append((base+src, cnames.get(cno,(0,'?'))[1]))
print("objects:", [(hex(o), c) for o, c in objs])
for o, c in objs:
    if c == 'hkaInterleavedUncompressedAnimation':
        atype, dur, nT, nF = struct.unpack_from('<ifii', blob, o+0x10)
        tp = local.get(o+0x38); tn, = struct.unpack_from('<i', blob, o+0x40)
        fp = local.get(o+0x48); fn, = struct.unpack_from('<i', blob, o+0x50)
        print("OBJECT @0x%x type=%d duration=%.6f nTransformTracks=%d nFloatTracks=%d" % (o, atype, dur, nT, nF))
        print("transforms[] +0x38 payload=%s size=%d ; floats[] +0x48 payload=%s size=%d" %
              (hex(tp) if tp else None, tn, hex(fp) if fp else None, fn))
        import math
        for f in (0, 4, 9):
            t = struct.unpack_from('<12f', blob, tp + f*48)
            ang = 2*math.degrees(math.asin(min(1.0, math.hypot(t[4],t[5],t[6]))))
            print("  frame %d T=(%.3f %.3f %.3f) Q=(%.6f %.6f %.6f %.6f) S=(%.3f %.3f %.3f) yaw=%.4f deg"
                  % ((f,)+t[0:3]+t[4:8]+t[8:11]+(ang,)))
