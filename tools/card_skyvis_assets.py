#!/usr/bin/env python3
"""Assets for the card sky-visibility pass (lane SKYVIS1127): a Fallout 4 BA2
reader (GNRL + DX10), a BGSM header reader, and the source model's triangle
soup with its alpha-test masks, built the way the card bake photographed it
(shapes named `_L1`..`_L9` hidden, a BSMeshLODTriShape cut to its first range).

The NIF walk is tests/spells/gltf_nifread.py's, extended here with the shape's
alpha property, its shader's material name and the LOD ranges.
"""
import io
import os
import re
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tests', 'spells'))
import gltf_nifread  # noqa: E402


def norm(p):
    p = p.replace('/', '\\').lower().lstrip('\\')
    if p.startswith('data\\'):
        p = p[5:]
    return p


class Ba2:
    def __init__(self, path):
        self.path = path
        self.entries = {}
        with open(path, 'rb') as f:
            hdr = f.read(24)
            magic, ver, kind, count, names = struct.unpack('<4sI4sIQ', hdr)
            if magic != b'BTDX':
                raise ValueError('not a BA2: ' + path)
            self.kind = kind
            recs = []
            if kind == b'GNRL':
                for _ in range(count):
                    r = struct.unpack('<I4sIIQIII', f.read(36))
                    recs.append(('g', r[4], r[5], r[6]))
            else:
                for _ in range(count):
                    h = struct.unpack('<I4sIBBHHHBBBB', f.read(24))
                    nch = h[4]
                    chunks = [struct.unpack('<QIIHHI', f.read(24)) for _ in range(nch)]
                    recs.append(('t', h[6], h[7], h[8], h[9], chunks))
            f.seek(names)
            for r in recs:
                n = struct.unpack('<H', f.read(2))[0]
                name = f.read(n).decode('latin-1')
                self.entries[norm(name)] = r

    def read(self, name):
        r = self.entries[norm(name)]
        with open(self.path, 'rb') as f:
            if r[0] == 'g':
                _, off, packed, size = r
                f.seek(off)
                b = f.read(packed if packed else size)
                return zlib.decompress(b) if packed else b
            _, h, w, mips, fmt, chunks = r
            data = b''
            for off, packed, size, m0, m1, _a in chunks:
                f.seek(off)
                b = f.read(packed if packed else size)
                data += zlib.decompress(b) if packed else b
            return ('tex', w, h, mips, fmt, data)


class Store:
    """Loose folders first (the LAST given wins, MO2's order), then archives."""

    def __init__(self, data_dir, loose=(), mod_archives=()):
        """mod_archives: highest priority FIRST (they win over the game's own)."""
        self.loose = list(loose)
        self.archives = [Ba2(p) for p in mod_archives]
        for fn in sorted(os.listdir(data_dir)):
            if fn.lower().endswith('.ba2'):
                try:
                    self.archives.append(Ba2(os.path.join(data_dir, fn)))
                except Exception as e:  # noqa: BLE001
                    print('skip archive', fn, e)
        self.data_dir = data_dir

    def find_by_basename(self, base, folder='meshes\\'):
        base = base.lower()
        hits = []
        for a in self.archives:
            for k in a.entries:
                if k.startswith(folder) and k.rsplit('\\', 1)[-1] == base:
                    hits.append((k, a.path))
        return hits

    def read(self, name):
        n = norm(name)
        for root in reversed(self.loose):
            p = os.path.join(root, n)
            if os.path.isfile(p):
                return open(p, 'rb').read()
        for a in self.archives:
            if n in a.entries:
                return a.read(n)
        return None


def bgsm(b):
    """(alphaTest, alphaTestRef, diffuse) of a BGSM (versions 1..2, Fallout 4)."""
    if not b or b[:4] != b'BGSM':
        return None
    ver = struct.unpack_from('<I', b, 4)[0]
    o = 8 + 4 + 16 + 4          # version, tile flags, uv offset/scale, alpha
    o += 1 + 4 + 4              # alpha blend mode
    ref = b[o]; o += 1
    test = b[o] != 0; o += 1
    o += 10                     # zwrite ztest ssr wetssr decal twosided decalnofade nonoccluder refraction refrfalloff
    o += 4                      # refraction power
    if ver < 10:
        o += 1 + 4              # environment mapping, mask scale
    else:
        o += 1
    o += 1                      # grayscale to palette colour
    if ver >= 6:
        o += 1
    n = struct.unpack_from('<I', b, o)[0]; o += 4
    diffuse = b[o:o + n].rstrip(b'\x00').decode('latin-1')
    return test, ref, diffuse


class NifX(gltf_nifread.Nif):
    """gltf_nifread's walk plus the alpha property, the shader name and LOD ranges."""

    def _read_all(self):
        super()._read_all()
        self.alphas = {}
        self.shadername = {}
        for i in range(self.numBlocks):
            t, s = self.type[i], self.start[i]
            if t == 'NiAlphaProperty':
                o = s + 4
                ne = self._u32(o); o += 4 + 4 * ne + 4
                flags = struct.unpack_from('<H', self.data, o)[0]
                thr = self.data[o + 2]
                self.alphas[i] = (flags, thr)
            elif t == 'BSLightingShaderProperty':
                self.shadername[i] = self._str(self._u32(s + 4))
            elif t == 'BSEffectShaderProperty':
                self.shadername[i] = self._str(self._u32(s))
        for i, sh in self.shapes.items():
            o = self.start[i]
            _n, _t, _r, _s, o = self._avobject(o)
            o += 16 + 4 + 4
            sh['alpha'] = self._i32(o)
            sh['lods'] = None
            if self.type[i] == 'BSMeshLODTriShape':
                e = self.start[i] + self.size[i]
                sh['lods'] = (self._u32(e - 12), self._u32(e - 8), self._u32(e - 4))


def _mat(r):
    return [r[0:3], r[3:6], r[6:9]]


def _world(nif, blk, is_shape):
    """The block's model-space transform: (R 3x3 row-major, t, s), root included."""
    chain = []
    cur = nif.shapes[blk] if is_shape else nif.nodes[blk]
    while cur is not None:
        chain.append(cur)
        p = cur['parent']
        cur = nif.nodes.get(p) if p is not None else None
    R = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]; T = [0.0, 0.0, 0.0]; S = 1.0
    for c in chain:                      # child first: x_parent = R_c x s_c + t_c
        r = _mat(c['r'])
        # compose: new(x) = r*(s_c * (R x S + T)) + t_c
        R = [[sum(r[a][k] * R[k][b] for k in range(3)) for b in range(3)] for a in range(3)]
        T = [c['s'] * sum(r[a][k] * T[k] for k in range(3)) + c['t'][a] for a in range(3)]
        S = S * c['s']
    return R, T, S


LEVEL = re.compile(r'_L[1-9]$', re.I)


def decode_alpha(tex):
    """Alpha plane (w, h, bytes) of mip 0 of a DX10 BA2 texture; None = no alpha."""
    from PIL import Image
    _k, w, h, mips, fmt, data = tex
    bc = {71: 1, 72: 1, 74: 2, 75: 2, 77: 3, 78: 3, 98: 7, 99: 7}.get(fmt)
    if bc is None or bc == 1 and False:
        return None
    blocks = ((w + 3) // 4) * ((h + 3) // 4)
    size = blocks * (8 if bc == 1 else 16)
    raw = data[:size]
    if bc == 1:
        img = Image.frombytes('RGBA', (w, h), raw, 'bcn', 1)
    elif bc == 2:
        img = Image.frombytes('RGBA', (w, h), raw, 'bcn', 2)
    elif bc == 3:
        img = Image.frombytes('RGBA', (w, h), raw, 'bcn', 3)
    else:
        img = Image.frombytes('RGBA', (w, h), raw, 'bcn', 7)
    return w, h, img.getchannel('A').tobytes()


def soup(store, nif_path, log=print):
    """[(9 floats, mapIndex, thr, 6 uv floats)], [(w, h, alpha bytes)], notes"""
    b = store.read(nif_path)
    if b is None:
        raise FileNotFoundError(nif_path)
    tmp = os.path.join(os.environ.get('TEMP', '.'), 'skyvis_tmp.nif')
    open(tmp, 'wb').write(b)
    nif = NifX(tmp)
    nif._parent_map()
    tris, maps, mapidx, notes = [], [], {}, []
    # the bake's mesh-LOD rule (nifskope_ui.cpp, IMPOSTORSHRUB1): ONE level for the model, the lowest slot any
    # BSMeshLODTriShape fills; a shape with an L1/L2 range draws that level's count from triangle 0
    kept = None
    for sh in nif.shapes.values():
        if sh['lods']:
            k = next((k for k in range(3) if sh['lods'][k]), None)
            if k is not None:
                kept = k if kept is None else min(kept, k)
    for blk, sh in sorted(nif.shapes.items()):
        if LEVEL.search(sh['name'] or ''):
            notes.append('hidden ' + sh['name'])
            continue
        R, T, S = _world(nif, blk, True)
        V = [[S * sum(R[a][k] * v[k] for k in range(3)) + T[a] for a in range(3)] for v in sh['verts']]
        idx = sh['tris']
        if sh['lods'] is not None:
            l0, l1, l2 = sh['lods']
            n = l0
            if (l1 or l2) and kept:
                n = l1 if kept == 1 else l2
            idx = idx[:n * 3]
            notes.append('range %s %d of %d (%d/%d/%d kept L%s)' % (sh['name'], n, sh['numTris'], l0, l1, l2, kept))
        m, thr = -1, 128
        ap = nif.alphas.get(sh['alpha'])
        test = bool(ap and (ap[0] & 0x200))
        if ap:
            thr = ap[1]
        diffuse = nif.diffuse_for(sh)
        mat = nif.shadername.get(sh['shader'], '')
        if mat.lower().endswith(('.bgsm', '.bgem')):
            mb = store.read('materials\\' + norm(mat) if not norm(mat).startswith('materials\\') else mat)
            g = bgsm(mb)
            if g:
                test = test or g[0]
                if g[0]:
                    thr = g[1]
                if g[2]:
                    diffuse = g[2]
        if test and diffuse:
            key = norm(diffuse)
            if key not in mapidx:
                tp = key if key.startswith('textures\\') else 'textures\\' + key
                tex = store.read(tp)
                a = decode_alpha(tex) if isinstance(tex, tuple) else None
                mapidx[key] = len(maps) if a else -1
                if a:
                    maps.append(a)
                else:
                    notes.append('no alpha map ' + key)
            m = mapidx[key]
        notes.append('shape %s tris %d test %s thr %d map %d' % (sh['name'], len(idx) // 3, test, thr, m))
        uv = sh['uvs']
        for k in range(0, len(idx), 3):
            a, b2, c = idx[k], idx[k + 1], idx[k + 2]
            p = V[a] + V[b2] + V[c]
            u = (list(uv[a]) + list(uv[b2]) + list(uv[c])) if uv else [0.0] * 6
            tris.append((p, m if test else -1, thr, u))
    return tris, maps, notes
