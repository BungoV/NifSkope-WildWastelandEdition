"""Minimal FO4 NIF header walker (lane IDENTITY, read-only).

Lists blocks with file offsets; decodes BSLightingShaderProperty Shader Flags
1/2 and BS*TriShape Vertex Desc / counts, straight from the bytes, against the
field order in build/nif.xml. No NifSkope needed.
"""
import struct, sys
from collections import Counter

def u8(b, o):  return b[o], o + 1
def u16(b, o): return struct.unpack_from('<H', b, o)[0], o + 2
def u32(b, o): return struct.unpack_from('<I', b, o)[0], o + 4
def u64(b, o): return struct.unpack_from('<Q', b, o)[0], o + 8
def sstr(b, o):
    n, o = u32(b, o)
    return b[o:o + n].decode('latin1'), o + n
def estr(b, o):                      # ExportString: byte length prefix
    n, o = u8(b, o)
    return b[o:o + n].decode('latin1').rstrip('\0'), o + n

def parse(path):
    b = open(path, 'rb').read()
    e = b.index(b'\n')
    hdr = b[:e].decode('latin1')
    o = e + 1
    ver, o = u32(b, o)
    endian, o = u8(b, o)
    user, o = u32(b, o)
    numblocks, o = u32(b, o)
    bsver, o = u32(b, o)
    author, o = estr(b, o)
    if bsver > 130:
        unk, o = u32(b, o)
    if bsver < 131:
        proc, o = estr(b, o)
    expo, o = estr(b, o)
    if 103 <= bsver < 170:
        maxpath, o = estr(b, o)
    numbt, o = u16(b, o)
    btypes = []
    for _ in range(numbt):
        s, o = sstr(b, o); btypes.append(s)
    btidx = []
    for _ in range(numblocks):
        v, o = u16(b, o); btidx.append(v & 0x7FFF)
    bsize = []
    for _ in range(numblocks):
        v, o = u32(b, o); bsize.append(v)
    numstr, o = u32(b, o)
    maxstr, o = u32(b, o)
    strings = []
    for _ in range(numstr):
        s, o = sstr(b, o); strings.append(s)
    numgroups, o = u32(b, o)
    o += 4 * numgroups
    blocks = []
    off = o
    for i in range(numblocks):
        blocks.append((i, btypes[btidx[i]], off, bsize[i]))
        off += bsize[i]
    return hdr, ver, user, bsver, strings, blocks, b, off

VDESC_FLAGS = [
    (0x0001, 'VERTEX'), (0x0002, 'UVs'), (0x0004, 'UV_2'), (0x0008, 'NORMALS'),
    (0x0010, 'TANGENTS'), (0x0020, 'COLORS'), (0x0040, 'SKINNED'),
    (0x0080, 'LANDDATA'), (0x0100, 'EYEDATA'), (0x0200, 'FULLPREC'),
]

def main(paths):
    for path in paths:
        hdr, ver, user, bsver, strings, blocks, b, endoff = parse(path)
        print('== %s' % path)
        print('   %s ver=%08x user=%d bsver=%d blocks=%d size=%d (walk ends %d)'
              % (hdr.strip(), ver, user, bsver, len(blocks), len(b), endoff))
        print('   types: %s' % dict(Counter(t for _, t, _, _ in blocks)))
        for i, t, off, sz in blocks:
            if t.endswith('ShaderProperty'):
                o = off
                shtype, o = u32(b, o)                 # NiObjectNET Shader Type (FO4 BSLSP only)
                if t != 'BSLightingShaderProperty':
                    o = off
                    shtype = None
                nameidx, o = u32(b, o)
                nex, o = u32(b, o); o += 4 * nex
                ctrl, o = u32(b, o)
                f1, o = u32(b, o)
                f2, o = u32(b, o)
                nm = strings[nameidx] if 0 <= nameidx < len(strings) else ('#%d' % nameidx)
                print('   #%-3d %-26s type=%s name=%r flags1=0x%08X flags2=0x%08X'
                      % (i, t, shtype, nm, f1, f2))
                print('        flags1 bits: %s' % ','.join(
                    str(k) for k in range(32) if f1 & (1 << k)))
                print('        flags2 bits: %s' % ','.join(
                    str(k) for k in range(32) if f2 & (1 << k)))
            if t in ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape',
                     'BSDynamicTriShape'):
                o = off
                nameidx, o = u32(b, o)
                nex, o = u32(b, o); o += 4 * nex
                ctrl, o = u32(b, o)
                flags, o = u32(b, o)
                o += 12 + 36 + 4        # Translation, Rotation, Scale
                coll, o = u32(b, o)     # Collision Object
                o += 16                 # Bounding Sphere (NiBound)
                skin, o = u32(b, o)
                shader, o = u32(b, o)
                alpha, o = u32(b, o)
                desc, o = u64(b, o)
                nt, o = u32(b, o)       # uint for BS >= 130
                nv, o = u16(b, o)
                ds, o = u32(b, o)
                vflags = (desc >> 44) & 0x3FF
                names = [n for m, n in VDESC_FLAGS if vflags & m]
                nm = strings[nameidx] if 0 <= nameidx < len(strings) else ('#%d' % nameidx)
                print('   #%-3d %-26s name=%r desc=0x%X stride=%d tris=%d verts=%d '
                      'datasize=%d(calc %d) attrs=%s'
                      % (i, t, nm, desc, desc & 0xF, nt, nv, ds,
                         (desc & 0xF) * 4 * nv + nt * 6, '+'.join(names)))

if __name__ == '__main__':
    main(sys.argv[1:])
