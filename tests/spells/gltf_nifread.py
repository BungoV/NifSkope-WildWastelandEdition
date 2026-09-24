#!/usr/bin/env python3
"""Independent FO4 (NIF 20.2.0.7, BS version 130) reader for the glTF gates.

It reads exactly what src/gltfexport.cpp claims to export -- the NiNode tree
(name, parent, local translation / row-major 3x3 rotation / uniform scale),
the BSTriShape-family shapes (positions, normals, UVs, triangles, the four
skin influences), the BSSkin::Instance / BSSkin::BoneData skin (bone node refs
plus the stored bone transform, which is FO4's INVERSE bind matrix) and the
shader property's diffuse path -- and nothing else.

INDEPENDENCE. Nothing here is shared with src/gltfexport.cpp,
tests/gltfexport_dump.cpp or tests/spells/gltf_check.py: the header walk, the
block walk and every field offset are written here from release/nif.xml's own
field lists. Its own arithmetic is the invariant that fails on a broken walk:
  * Data Size == Num Vertices * stride + Num Triangles * 6, per shape;
  * every block parses to exactly its Block Sizes entry;
  * the block table plus the footer's root refs end at the last byte.

Provenance for the two field orders that are easy to get wrong (both read from
release/nif.xml, 2026-09-10, lane HKX4b):
  * `NiObjectNET` carries a LEADING `Shader Type` uint that exists only on
    BSLightingShaderProperty (`onlyT="BSLightingShaderProperty"`, vercond
    `#BS_GTE_SKY# #AND# #NI_BS_LTE_FO4#`). Missing it shifts the whole shader
    block by four bytes -- which is what the first draft of this file did, and
    it made the reader read a 4-billion-entry extra-data list.
  * `BSSkinBoneTrans` = NiBound (Vector3 centre + float radius) 16 B,
    Matrix33 36 B, Vector3 translation 12 B, float scale 4 B = 68 B, and
    Matrix33 is stored ROW-major (nif.xml: "Stored in row-major format").

Usage:  gltf_nifread.py FILE.nif        prints the summary the gates quote.
"""
import os
import struct
import sys

NODE_TYPES = ('NiNode', 'BSFadeNode', 'BSLeafAnimNode', 'BSTreeNode',
              'BSOrderedNode', 'BSMultiBoundNode', 'BSValueNode')
SHAPE_TYPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape',
               'BSDynamicTriShape')

# Vertex Desc >> 44 : the 12 Vertex Attributes bits (nif.xml bitfield
# BSVertexDesc, member "Vertex Attributes" pos=44 width=12).
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


class Nif:
    def __init__(self, path):
        self.path = path
        self.data = open(path, 'rb').read()
        self.problems = []
        self._header()
        self.nodes = {}
        self.shapes = {}
        self.skins = {}
        self.bonedata = {}
        self.texsets = {}
        self.shaderprops = {}
        self._read_all()
        self._parent_map()

    # ---- primitives -------------------------------------------------
    def _u32(self, o):
        return struct.unpack_from('<I', self.data, o)[0]

    def _i32(self, o):
        return struct.unpack_from('<i', self.data, o)[0]

    def _str(self, idx):
        return self.strings[idx] if 0 <= idx < len(self.strings) else ''

    def _sized(self, o):
        n = self._u32(o)
        return self.data[o + 4:o + 4 + n].decode('latin-1'), o + 4 + n

    # ---- header -----------------------------------------------------
    def _header(self):
        d = self.data
        o = d.index(b'\n') + 1
        ver, = struct.unpack_from('<I', d, o); o += 4
        o += 1                                    # Endian Type
        o += 4                                    # User Version
        nblocks, = struct.unpack_from('<I', d, o); o += 4
        bsver, = struct.unpack_from('<I', d, o); o += 4
        if ver != 0x14020007 or bsver != 130:
            raise NifError('%s: NIF version %08x BS version %d, not a Fallout 4 file'
                           % (os.path.basename(self.path), ver, bsver))
        for _ in range(4):                        # Author, Process, Export, Max Filepath
            n = d[o]; o += 1 + n
        ntypes, = struct.unpack_from('<H', d, o); o += 2
        types = []
        for _ in range(ntypes):
            s, o = self._sized(o)
            types.append(s)
        tidx = struct.unpack_from('<%dH' % nblocks, d, o); o += 2 * nblocks
        bsize = struct.unpack_from('<%dI' % nblocks, d, o); o += 4 * nblocks
        nstr, = struct.unpack_from('<I', d, o); o += 4
        o += 4                                    # Max String Length
        self.strings = []
        for _ in range(nstr):
            s, o = self._sized(o)
            self.strings.append(s)
        ngroups, = struct.unpack_from('<I', d, o); o += 4 + 4 * ngroups
        self.type = {}
        self.start = {}
        self.size = {}
        for i in range(nblocks):
            self.type[i] = types[tidx[i]]
            self.start[i] = o
            self.size[i] = bsize[i]
            o += bsize[i]
        nroots, = struct.unpack_from('<I', d, o)
        end = o + 4 + 4 * nroots
        if end != len(d):
            raise NifError('%s: the block table ends at %d, %d root refs take it to %d, the file is %d bytes'
                           % (os.path.basename(self.path), o, nroots, end, len(d)))
        self.numBlocks = nblocks

    # ---- the NiObjectNET / NiAVObject prefix ------------------------
    def _avobject(self, o):
        name = self._str(self._u32(o)); o += 4
        ne = self._u32(o); o += 4 + 4 * ne         # Num Extra Data List + refs
        o += 4                                     # Controller
        o += 4                                     # Flags (NiAVObject, uint at BS >= 130)
        t = struct.unpack_from('<3f', self.data, o); o += 12
        r = struct.unpack_from('<9f', self.data, o); o += 36
        s = struct.unpack_from('<f', self.data, o)[0]; o += 4
        o += 4                                     # Collision Object
        return name, t, r, s, o

    # ---- blocks -----------------------------------------------------
    def _read_all(self):
        for i in range(self.numBlocks):
            tname, start, size = self.type[i], self.start[i], self.size[i]
            if tname in NODE_TYPES:
                name, t, r, s, o = self._avobject(start)
                nc = self._u32(o); o += 4
                ch = list(struct.unpack_from('<%di' % nc, self.data, o)) if nc else []
                o += 4 * nc
                self.nodes[i] = dict(name=name, t=t, r=r, s=s, children=ch,
                                     parent=None, block=i)
                if tname == 'NiNode' and o - start != size:
                    self.problems.append('block %d NiNode parsed %d of %d bytes' % (i, o - start, size))
            elif tname in SHAPE_TYPES:
                self.shapes[i] = self._read_shape(i, start, size)
            elif tname == 'BSSkin::Instance':
                o = start
                skelroot = self._i32(o); o += 4
                dataref = self._i32(o); o += 4
                nb = self._u32(o); o += 4
                bones = list(struct.unpack_from('<%di' % nb, self.data, o)) if nb else []
                o += 4 * nb
                ns = self._u32(o); o += 4 + 12 * ns
                self.skins[i] = dict(skeletonRoot=skelroot, data=dataref, bones=bones, numScales=ns)
                if o - start != size:
                    self.problems.append('block %d BSSkin::Instance parsed %d of %d bytes' % (i, o - start, size))
            elif tname == 'BSSkin::BoneData':
                o = start
                nb = self._u32(o); o += 4
                lst = []
                for _ in range(nb):
                    v = struct.unpack_from('<17f', self.data, o); o += 68
                    lst.append(dict(sphere=v[0:4], r=v[4:13], t=v[13:16], s=v[16]))
                self.bonedata[i] = lst
                if o - start != size:
                    self.problems.append('block %d BSSkin::BoneData parsed %d of %d bytes' % (i, o - start, size))
            elif tname == 'BSShaderTextureSet':
                o = start
                n = self._u32(o); o += 4
                tex = []
                for _ in range(n):
                    s, o = self._sized(o)
                    tex.append(s)
                self.texsets[i] = tex
                if o - start != size:
                    self.problems.append('block %d BSShaderTextureSet parsed %d of %d bytes' % (i, o - start, size))
            elif tname in ('BSLightingShaderProperty', 'BSEffectShaderProperty'):
                o = start
                if tname == 'BSLightingShaderProperty':
                    o += 4                         # Shader Type (onlyT, see the module docstring)
                o += 4                             # Name
                ne = self._u32(o); o += 4 + 4 * ne  # Num Extra Data List + refs
                o += 4                             # Controller
                o += 8                             # Shader Flags 1, 2
                o += 16                            # UV Offset, UV Scale
                if tname == 'BSLightingShaderProperty':
                    self.shaderprops[i] = dict(texset=self._i32(o), source=None)
                else:
                    src, o = self._sized(o)
                    self.shaderprops[i] = dict(texset=-1, source=src)

    def _read_shape(self, i, start, size):
        name, t, r, s, o = self._avobject(start)
        o += 16                                    # Bounding Sphere
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
        verts, norms, uvs, weights, bidx = [], [], [], [], []
        base = o
        for k in range(nv):
            p = base + k * stride
            if va & VA_VERTEX:
                if va & VA_FULLPREC:
                    verts.append(struct.unpack_from('<3f', self.data, p)); p += 12 + 4
                else:
                    h = struct.unpack_from('<3H', self.data, p); p += 6
                    verts.append(tuple(half_to_float(x) for x in h))
                    p += 2                         # Bitangent X / unused W
            if va & VA_UV:
                h = struct.unpack_from('<2H', self.data, p); p += 4
                uvs.append(tuple(half_to_float(x) for x in h))
            if va & VA_UV2:
                p += 4
            if va & VA_NORMALS:
                b = struct.unpack_from('<3B', self.data, p); p += 3
                norms.append(tuple(x / 255.0 * 2.0 - 1.0 for x in b))
                p += 1                             # Bitangent Y
            if va & VA_TANGENTS:
                p += 4
            if va & VA_COLORS:
                p += 4
            if va & VA_SKINNED:
                h = struct.unpack_from('<4H', self.data, p); p += 8
                weights.append(tuple(half_to_float(x) for x in h))
                bidx.append(struct.unpack_from('<4B', self.data, p)); p += 4
            if va & VA_EYEDATA:
                p += 4
            if p - (base + k * stride) != stride:
                self.problems.append("shape %d '%s': vertex %d walked %d of %d bytes"
                                     % (i, name, k, p - (base + k * stride), stride))
                break
        o = base + nv * stride
        tris = list(struct.unpack_from('<%dH' % (ntri * 3), self.data, o))
        return dict(block=i, name=name, t=t, r=r, s=s, skin=skin, shader=shader,
                    va=va, stride=stride, numVerts=nv, numTris=ntri, dataSize=dsize,
                    verts=verts, norms=norms, uvs=uvs, weights=weights,
                    boneIndices=bidx, tris=tris, parent=None)

    def _parent_map(self):
        for i, n in self.nodes.items():
            for c in n['children']:
                if c in self.nodes:
                    self.nodes[c]['parent'] = i
                elif c in self.shapes:
                    self.shapes[c]['parent'] = i

    # ---- helpers ----------------------------------------------------
    def roots(self):
        return [i for i, n in self.nodes.items() if n['parent'] is None]

    def diffuse_for(self, shape):
        sp = self.shaderprops.get(shape['shader'])
        if not sp:
            return ''
        if sp['source'] is not None:
            return sp['source']
        tex = self.texsets.get(sp['texset'])
        return tex[0] if tex else ''

    def skin_bones(self, shape):
        """[(bone node name, the stored 68-byte transform)] in skin order."""
        sk = self.skins.get(shape['skin'])
        if not sk:
            return []
        bd = self.bonedata.get(sk['data'], [])
        out = []
        for k, blk in enumerate(sk['bones']):
            nm = self.nodes[blk]['name'] if blk in self.nodes else '?block%d' % blk
            out.append((nm, bd[k] if k < len(bd) else None))
        return out


def main(argv):
    n = Nif(argv[1])
    print('%s: %d blocks, %d nodes, %d shapes, %d skins'
          % (os.path.basename(n.path), n.numBlocks, len(n.nodes), len(n.shapes), len(n.skins)))
    for p in n.problems:
        print('  PROBLEM ' + p)
    for i in sorted(n.shapes):
        sh = n.shapes[i]
        wsum = [sum(w) for w in sh['weights']]
        print("  shape[%d] '%s' nv=%d nt=%d stride=%d va=%#x bones=%d tex0=%s"
              % (i, sh['name'], sh['numVerts'], sh['numTris'], sh['stride'], sh['va'],
                 len(n.skin_bones(sh)), n.diffuse_for(sh)))
        if wsum:
            print('     weight sums: min %.6f max %.6f' % (min(wsum), max(wsum)))
    return 1 if n.problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
