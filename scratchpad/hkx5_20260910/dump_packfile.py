#!/usr/bin/env python3
"""Full structural dump of a Havok 2014 packfile: header, sections, the three
fixup tables and every object's first bytes.  Written for lane HKX5b to mirror
HKXPACK's own output byte for byte in the direct packfile emitter (route B).

usage: python dump_packfile.py FILE.hkx [--bytes N]
"""
import struct, sys


def dump(path, nbytes=64):
    blob = open(path, 'rb').read()
    print("file %s  %d bytes" % (path, len(blob)))
    magic = blob[0:8]
    print("magic          %s" % magic.hex())
    (userTag, fileVersion) = struct.unpack_from('<ii', blob, 8)
    layout = blob[0x10:0x14]
    (numSections,) = struct.unpack_from('<i', blob, 0x14)
    (csi, cso, ccnsi, ccnso) = struct.unpack_from('<iiii', blob, 0x18)
    ver = blob[0x28:0x38]
    (flags,) = struct.unpack_from('<i', blob, 0x38)
    (maxpred, predpad) = struct.unpack_from('<HH', blob, 0x3c)
    print("userTag %d fileVersion %d layout %s numSections %d" % (userTag, fileVersion, layout.hex(), numSections))
    print("contentsSection %d off %d  contentsClassNameSection %d off %d" % (csi, cso, ccnsi, ccnso))
    print("contentsVersion %r flags %d maxpredicate 0x%x predicateArraySizePlusPadding 0x%x" % (ver, flags, maxpred, predpad))
    sechdr = 0x40 + predpad
    print("section headers start 0x%x" % sechdr)
    secs = {}
    order = []
    for s in range(numSections):
        off = sechdr + s * 0x40
        tag = blob[off:off + 19].split(b'\0')[0].decode()
        vals = struct.unpack_from('<7i', blob, off + 20)
        pad = blob[off + 48:off + 64]
        secs[tag] = vals
        order.append(tag)
        print("  [%d] %-16s abs=0x%-6x local=%-6d global=%-6d virtual=%-6d exports=%-6d imports=%-6d end=%-6d pad=%s"
              % ((s, tag) + vals + (pad.hex(),)))

    cn = secs['__classnames__']
    print("\n__classnames__ (abs 0x%x .. 0x%x):" % (cn[0], cn[0] + cn[1]))
    cnames = {}
    p, end = cn[0], cn[0] + cn[1]
    while p + 5 < end:
        if blob[p + 4] != 0x09:
            print("  stop at 0x%x (no 0x09)" % p)
            break
        sig, = struct.unpack_from('<I', blob, p)
        e = blob.index(b'\0', p + 5)
        nm = blob[p + 5:e].decode('latin-1')
        cnames[p + 5 - cn[0]] = (sig, nm)
        print("  rel 0x%-4x sig 0x%08x  %s" % (p + 5 - cn[0], sig, nm))
        p = e + 1
    tail = blob[p:end]
    print("  tail %d bytes: %s" % (len(tail), tail.hex()))

    dt = secs['__data__']
    base = dt[0]
    print("\n__data__ abs 0x%x, payload 0 .. %d, local %d, global %d, virtual %d, end %d"
          % (base, dt[1], dt[1], dt[2], dt[3], dt[6]))
    print("local fixups (src -> dst, both relative to the section):")
    loc = []
    for p in range(base + dt[1], base + dt[2], 8):
        src, dst = struct.unpack_from('<ii', blob, p)
        if src == -1:
            continue
        loc.append((src, dst))
        print("  0x%-6x -> 0x%-6x" % (src, dst))
    print("global fixups (src, sectionIndex, dst):")
    for p in range(base + dt[2], base + dt[3], 12):
        src, sec, dst = struct.unpack_from('<iii', blob, p)
        if src == -1:
            continue
        print("  0x%-6x sec %d -> 0x%-6x" % (src, sec, dst))
    print("virtual fixups (objOffset, sectionIndex, classNameOffset):")
    objs = []
    for p in range(base + dt[3], min(base + dt[4], len(blob) - 11), 12):
        src, sec, cno = struct.unpack_from('<iii', blob, p)
        if src == -1:
            continue
        nm = cnames.get(cno, (0, '?'))[1]
        objs.append((src, nm))
        print("  0x%-6x sec %d class rel 0x%-4x  %s" % (src, sec, cno, nm))

    print("\nobject bytes:")
    for i, (off, nm) in enumerate(objs):
        nxt = objs[i + 1][0] if i + 1 < len(objs) else dt[1]
        n = min(nxt - off, nbytes)
        print("  %s @0x%x (size to next 0x%x)" % (nm, off, nxt - off))
        for r in range(0, n, 16):
            row = blob[base + off + r: base + off + r + 16]
            print("    +0x%03x %s" % (r, ' '.join('%02x' % b for b in row)))


if __name__ == '__main__':
    nb = 64
    args = [a for a in sys.argv[1:]]
    if '--bytes' in args:
        i = args.index('--bytes')
        nb = int(args[i + 1])
        del args[i:i + 2]
    dump(args[0], nb)
