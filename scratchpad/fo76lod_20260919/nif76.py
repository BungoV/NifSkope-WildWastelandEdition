#!/usr/bin/env python3
"""Independent Fallout 76 (NIF 20.2.0.7, BS version 155) reader.

Field orders taken from E:/Projects/NifskopeWildWastelandEdition/release/nif.xml:
  * Header/BSStreamHeader: BS Version, Author(ExportString), Unknown Int(uint,
    only when BS Version > 130), Export Script(ExportString),
    Max Filepath(ExportString). Process Script exists only below 131.
  * NiObjectNET: the leading `Shader Type` uint is `#NI_BS_LTE_FO4#`, so it is
    ABSENT at 155 -- the opposite of the FO4 reader.
  * BSTriShape at 155 has a `Bounding Box` (BSBoundingBox, 24 bytes) right
    after `Bounding Sphere` (`vercond="#BS_GTE_F76#"`).
  * BSVertexData is the same bitfield layout as FO4.

Invariants that fail on a broken walk, same as the FO4 reader:
  * Data Size == (Vertex Desc & 0xF)*4*NumVertices + NumTriangles*6;
  * every walked block ends exactly on its Block Sizes entry;
  * block table + footer root refs end at the last byte of the file.
"""
import os
import struct
import sys

NODE_TYPES = ('NiNode', 'BSFadeNode', 'BSLeafAnimNode', 'BSTreeNode',
              'BSOrderedNode', 'BSMultiBoundNode', 'BSValueNode')
SHAPE_TYPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape',
               'BSDynamicTriShape')

VA_VERTEX = 0x001
VA_UV = 0x002
VA_UV2 = 0x004
VA_NORMALS = 0x008
VA_TANGENTS = 0x010
VA_COLORS = 0x020
VA_SKINNED = 0x040
VA_EYEDATA = 0x100
VA_FULLPREC = 0x400


class NifError(Exception):
    pass


def half_to_float(h):
    s = (h >> 15) & 1
    e = (h >> 10) & 0x1F
    m = h & 0x3FF
    if e == 0:
        v = (m / 1024.0) * (2.0 ** -14)
    elif e == 31:
        v = float('inf') if m == 0 else float('nan')
    else:
        v = (1.0 + m / 1024.0) * (2.0 ** (e - 15))
    return -v if s else v


class Nif76:
    def __init__(self, path, verts=True):
        self.path = path
        self.data = open(path, 'rb').read()
        self.problems = []
        self.want_verts = verts
        self._header()
        self.nodes = {}
        self.shapes = {}
        self.texsets = {}
        self.shaderprops = {}
        self.extradata = {}
        self._read_all()
        self._parent_map()

    def _u32(self, o):
        return struct.unpack_from('<I', self.data, o)[0]

    def _i32(self, o):
        return struct.unpack_from('<i', self.data, o)[0]

    def _str(self, idx):
        return self.strings[idx] if 0 <= idx < len(self.strings) else ''

    def _sized(self, o):
        n = self._u32(o)
        return self.data[o + 4:o + 4 + n].decode('latin-1'), o + 4 + n

    def _header(self):
        d = self.data
        o = d.index(b'\n') + 1
        ver, = struct.unpack_from('<I', d, o); o += 4
        o += 1                                     # Endian Type
        self.userver, = struct.unpack_from('<I', d, o); o += 4
        nblocks, = struct.unpack_from('<I', d, o); o += 4
        bsver, = struct.unpack_from('<I', d, o); o += 4
        if ver != 0x14020007 or bsver != 155:
            raise NifError('%s: NIF %08x BS %d, not a Fallout 76 file'
                           % (os.path.basename(self.path), ver, bsver))
        self.bsver = bsver
        n = d[o]; o += 1 + n                       # Author
        o += 4                                     # Unknown Int (BS > 130)
        n = d[o]; o += 1 + n                       # Export Script
        n = d[o]; o += 1 + n                       # Max Filepath
        ntypes, = struct.unpack_from('<H', d, o); o += 2
        types = []
        for _ in range(ntypes):
            s, o = self._sized(o)
            types.append(s)
        tidx = struct.unpack_from('<%dH' % nblocks, d, o); o += 2 * nblocks
        bsize = struct.unpack_from('<%dI' % nblocks, d, o); o += 4 * nblocks
        nstr, = struct.unpack_from('<I', d, o); o += 4
        o += 4
        self.strings = []
        for _ in range(nstr):
            s, o = self._sized(o)
            self.strings.append(s)
        ngroups, = struct.unpack_from('<I', d, o); o += 4 + 4 * ngroups
        self.type, self.start, self.size = {}, {}, {}
        for i in range(nblocks):
            self.type[i] = types[tidx[i]]
            self.start[i] = o
            self.size[i] = bsize[i]
            o += bsize[i]
        nroots, = struct.unpack_from('<I', d, o)
        end = o + 4 + 4 * nroots
        if end != len(d):
            raise NifError('%s: block table ends at %d, %d roots -> %d, file is %d'
                           % (os.path.basename(self.path), o, nroots, end, len(d)))
        self.numBlocks = nblocks
        self.blockTypes = types

    def _avobject(self, o):
        """NiObjectNET + NiAVObject prefix at BS 155 (no leading Shader Type)."""
        name = self._str(self._u32(o)); o += 4
        ne = self._u32(o)
        extras = list(struct.unpack_from('<%di' % ne, self.data, o + 4)) if ne else []
        o += 4 + 4 * ne
        o += 4                                     # Controller
        o += 4                                     # Flags (uint, BSVER > 26)
        t = struct.unpack_from('<3f', self.data, o); o += 12
        r = struct.unpack_from('<9f', self.data, o); o += 36
        s = struct.unpack_from('<f', self.data, o)[0]; o += 4
        o += 4                                     # Collision Object
        return name, t, r, s, extras, o

    def _read_all(self):
        for i in range(self.numBlocks):
            tname, start, size = self.type[i], self.start[i], self.size[i]
            if tname in NODE_TYPES:
                name, t, r, s, ex, o = self._avobject(start)
                nc = self._u32(o); o += 4
                ch = list(struct.unpack_from('<%di' % nc, self.data, o)) if nc else []
                o += 4 * nc
                self.nodes[i] = dict(name=name, t=t, r=r, s=s, children=ch,
                                     extras=ex, parent=None, block=i, type=tname)
                if tname == 'NiNode' and o - start != size:
                    self.problems.append('block %d NiNode parsed %d of %d'
                                         % (i, o - start, size))
            elif tname in SHAPE_TYPES:
                self.shapes[i] = self._read_shape(i, start, size, tname)
            elif tname == 'BSShaderTextureSet':
                o = start
                n = self._u32(o); o += 4
                tex = []
                for _ in range(n):
                    s, o = self._sized(o)
                    tex.append(s)
                self.texsets[i] = tex
                if o - start != size:
                    self.problems.append('block %d BSShaderTextureSet parsed %d of %d'
                                         % (i, o - start, size))
            elif tname in ('BSLightingShaderProperty', 'BSEffectShaderProperty'):
                o = start
                name = self._str(self._u32(o)); o += 4
                ne = self._u32(o); o += 4 + 4 * ne
                o += 4                             # Controller
                self.shaderprops[i] = dict(name=name, body=start + 0, bodyStart=o,
                                           end=start + size, type=tname)
            elif tname.startswith('BS') and 'ExtraData' in tname:
                o = start
                nm = self._str(self._u32(o)); o += 4
                rest = self.data[o:start + size]
                self.extradata[i] = dict(name=nm, type=tname, raw=rest)

    def _read_shape(self, i, start, size, tname):
        name, t, r, s, ex, o = self._avobject(start)
        o += 16                                    # Bounding Sphere
        bbox = struct.unpack_from('<6f', self.data, o); o += 24   # F76 Bounding Box
        skin = self._i32(o); o += 4
        shader = self._i32(o); o += 4
        o += 4                                     # Alpha Property
        desc = struct.unpack_from('<Q', self.data, o)[0]; o += 8
        ntri = self._u32(o); o += 4
        nv = struct.unpack_from('<H', self.data, o)[0]; o += 2
        dsize = self._u32(o); o += 4
        stride = (desc & 0xF) * 4
        va = (desc >> 44) & 0xFFF
        want = nv * stride + ntri * 6
        if dsize != want:
            self.problems.append("shape %d '%s': Data Size %d != %d*%d + %d*6 = %d"
                                 % (i, name, dsize, nv, stride, ntri, want))
        verts, norms, uvs, cols = [], [], [], []
        base = o
        if self.want_verts:
            for k in range(nv):
                p = base + k * stride
                if va & VA_VERTEX:
                    if va & VA_FULLPREC:
                        verts.append(struct.unpack_from('<3f', self.data, p)); p += 12 + 4
                    else:
                        h = struct.unpack_from('<3H', self.data, p); p += 6
                        verts.append(tuple(half_to_float(x) for x in h))
                        p += 2
                if va & VA_UV:
                    h = struct.unpack_from('<2H', self.data, p); p += 4
                    uvs.append(tuple(half_to_float(x) for x in h))
                if va & VA_UV2:
                    p += 4
                if va & VA_NORMALS:
                    b = struct.unpack_from('<3B', self.data, p); p += 3
                    norms.append(tuple(x / 255.0 * 2.0 - 1.0 for x in b))
                    p += 1
                if va & VA_TANGENTS:
                    p += 4
                if va & VA_COLORS:
                    cols.append(struct.unpack_from('<4B', self.data, p)); p += 4
                if va & VA_SKINNED:
                    p += 12
                if va & VA_EYEDATA:
                    p += 4
                if p - (base + k * stride) != stride:
                    self.problems.append("shape %d '%s': vertex %d walked %d of %d"
                                         % (i, name, k, p - (base + k * stride), stride))
                    break
        o = base + nv * stride
        tris = list(struct.unpack_from('<%dH' % (ntri * 3), self.data, o)) if ntri else []
        o += ntri * 6
        segs = None
        if tname == 'BSSubIndexTriShape' and dsize > 0:
            o += 4                                 # Num Primitives
            nseg = self._u32(o); o += 4
            tot = self._u32(o); o += 4
            segs = []
            for _ in range(nseg):
                si = self._u32(o); o += 4
                np_ = self._u32(o); o += 4
                o += 4                             # Parent Array Index
                nss = self._u32(o); o += 4
                o += 12 * nss
                segs.append((si, np_, nss))
            if nseg < tot:
                o = start + size                   # shared data: not walked
        if tname in ('BSTriShape',) and o - start != size:
            self.problems.append('block %d %s parsed %d of %d'
                                 % (i, tname, o - start, size))
        return dict(block=i, name=name, t=t, r=r, s=s, bbox=bbox, skin=skin,
                    shader=shader, va=va, stride=stride, numVerts=nv,
                    numTris=ntri, dataSize=dsize, verts=verts, norms=norms,
                    uvs=uvs, colors=cols, tris=tris, parent=None, segs=segs,
                    type=tname, extras=ex)

    def _parent_map(self):
        for i, n in self.nodes.items():
            for c in n['children']:
                if c in self.nodes:
                    self.nodes[c]['parent'] = i
                elif c in self.shapes:
                    self.shapes[c]['parent'] = i

    def shader_strings(self, blk):
        """Every plausible SizedString inside a shader block's raw bytes."""
        sp = self.shaderprops.get(blk)
        if not sp:
            return []
        d = self.data[sp['bodyStart']:sp['end']]
        out = []
        o = 0
        while o + 4 <= len(d):
            n = struct.unpack_from('<I', d, o)[0]
            if 4 <= n <= 300 and o + 4 + n <= len(d):
                s = d[o + 4:o + 4 + n]
                try:
                    t = s.decode('ascii')
                except UnicodeDecodeError:
                    o += 1
                    continue
                if all(32 <= c < 127 for c in s) and ('\\' in t or '/' in t or '.' in t):
                    out.append(t)
                    o += 4 + n
                    continue
            o += 1
        return out

    def material_of(self, shape):
        sp = self.shaderprops.get(shape['shader'])
        return sp['name'] if sp else ''


def main(argv):
    n = Nif76(argv[1])
    print('%s: %d blocks, %d nodes, %d shapes' %
          (os.path.basename(n.path), n.numBlocks, len(n.nodes), len(n.shapes)))
    print('  block types: %s' % ', '.join(n.blockTypes))
    for p in n.problems[:20]:
        print('  PROBLEM ' + p)
    for i in sorted(n.shapes):
        sh = n.shapes[i]
        print("  shape[%d] %s '%s' nv=%d nt=%d stride=%d va=%#x mat=%s"
              % (i, sh['type'], sh['name'], sh['numVerts'], sh['numTris'],
                 sh['stride'], sh['va'], n.material_of(sh)))
    return 1 if n.problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
