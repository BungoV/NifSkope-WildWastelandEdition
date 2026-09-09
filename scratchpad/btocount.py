"""Count triangles/vertices per BSSubIndexTriShape in a FO4 .BTO/.BTR, offline.

No exe needed.  The parse is self-checked: for every shape it recomputes
Data Size = (VertexDesc & 0xF) * 4 * NumVertices + NumTriangles * 6 and refuses
the file if any shape disagrees, so a wrong field offset cannot pass as a count.
"""
import struct
import sys
import os


class R:
    def __init__(self, b):
        self.b = b
        self.o = 0

    def u8(self):
        v = self.b[self.o]
        self.o += 1
        return v

    def u16(self):
        v = struct.unpack_from('<H', self.b, self.o)[0]
        self.o += 2
        return v

    def u32(self):
        v = struct.unpack_from('<I', self.b, self.o)[0]
        self.o += 4
        return v

    def u64(self):
        v = struct.unpack_from('<Q', self.b, self.o)[0]
        self.o += 8
        return v

    def i32(self):
        v = struct.unpack_from('<i', self.b, self.o)[0]
        self.o += 4
        return v

    def expstr(self):
        n = self.u8()
        s = self.b[self.o:self.o + n]
        self.o += n
        return s

    def sizedstr(self):
        n = self.u32()
        s = self.b[self.o:self.o + n]
        self.o += n
        return s.decode('latin-1')


def parse(path):
    b = open(path, 'rb').read()
    r = R(b)
    nl = b.index(b'\n')
    r.o = nl + 1
    ver = r.u32()
    endian = r.u8()
    user = r.u32()
    nblocks = r.u32()
    bsver = r.u32()
    r.expstr()                       # Author
    if bsver >= 131:
        r.u32()
    r.expstr()                       # Process Script
    r.expstr()                       # Export Script
    if bsver == 130:
        r.expstr()                   # Max Filepath
    ntypes = r.u16()
    types = [r.sizedstr() for _ in range(ntypes)]
    tidx = [r.u16() for _ in range(nblocks)]
    sizes = [r.u32() for _ in range(nblocks)]
    nstr = r.u32()
    r.u32()
    strings = [r.sizedstr() for _ in range(nstr)]
    ngroups = r.u32()
    for _ in range(ngroups):
        r.u32()
    base = r.o
    shapes = []
    off = base
    for i in range(nblocks):
        t = types[tidx[i] & 0x7FFF]
        if t in ('BSSubIndexTriShape', 'BSTriShape', 'BSMeshLODTriShape'):
            q = R(b)
            q.o = off
            nameIdx = q.u32()
            nx = q.u32()
            q.o += 4 * nx
            q.i32()                  # Controller
            q.u32()                  # Flags
            q.o += 12 + 36 + 4       # Translation, Rotation, Scale
            q.i32()                  # Collision Object
            q.o += 16                # Bounding Sphere
            q.i32()                  # Skin
            q.i32()                  # Shader Property
            q.i32()                  # Alpha Property
            desc = q.u64()
            ntri = q.u32()
            nvert = q.u16()
            dsize = q.u32()
            want = (desc & 0xF) * 4 * nvert + ntri * 6
            if want != dsize:
                raise SystemExit('%s: block %d (%s): Data Size %d != %d — parse is wrong'
                                 % (path, i, t, dsize, want))
            name = strings[nameIdx] if nameIdx < len(strings) else '?'
            nseg = -1
            if t == 'BSSubIndexTriShape':
                q.o += nvert * ((desc & 0xF) * 4) + ntri * 6
                q.u32()                  # Num Primitives
                nseg = q.u32()           # Num Segments
            shapes.append((i, t, name, nvert, ntri, (desc & 0xF) * 4, nseg))
        off += sizes[i]
    if off != len(b) - 4:            # footer: Num Roots + roots
        pass
    return shapes


if __name__ == '__main__':
    total_t = total_v = 0
    for p in sys.argv[1:]:
        sh = parse(p)
        t = sum(s[4] for s in sh)
        v = sum(s[3] for s in sh)
        total_t += t
        total_v += v
        print('%-40s shapes %3d verts %7d tris %7d' % (os.path.basename(p), len(sh), v, t))
        if os.environ.get('PERSHAPE'):
            for s in sh:
                print('    block %3d %-20s v %6d t %6d stride %d  %s' % (s[0], s[1], s[3], s[4], s[5], s[2]))
    if len(sys.argv) > 2:
        print('TOTAL files %d verts %d tris %d' % (len(sys.argv) - 1, total_v, total_t))
