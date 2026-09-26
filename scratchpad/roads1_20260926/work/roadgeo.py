"""ROADS1 (2026-09-26): an INDEPENDENT reading of the road and pavement placements and their in-game surface.

Nothing here links NifSkope or reads a NifSkope output. Plugins come off his MO2 profile (merged the game's way,
nlc.World), NIFs / BGSMs / DDSs off the same resource stack (nlc.Stack), and the surface rule is the game's:
  diffuse = the BGSM's texture 0 (else the NIF texture set's slot 0), sampled at uv * uvScale + uvOffset,
  x vertex colour ONLY where the shape's vertex descriptor carries colours AND SLSF2 bit 5 (Vertex_Colors) is set.
The texel footprint picks the mip: 0.5 * log2(texture texels / sheet texels over the triangle).
"""
import math
import os
import pickle
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import nlc  # noqa: E402

PROFILE = 'E:/Projects/Fallout 4 Mods/profiles/Default'
MODS = 'E:/Projects/Fallout 4 Mods/mods'
DATA = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data'
NIFXML = 'E:/Projects/NifskopeWWE-roads1/build/nif.xml'
BS = chr(92)


def comps(model):
    c = [x for x in model.lower().replace(BS, '/').split('/') if x]
    if c and c[0] == 'meshes':
        c = c[1:]
    return c


def is_road(model):
    c = comps(model)
    return len(c) >= 3 and c[0] == 'landscape' and c[1] in ('roads', 'sidewalks')


def is_sidewalk(model):
    c = comps(model)
    return len(c) >= 3 and c[0] == 'landscape' and c[1] == 'sidewalks'


def is_raised_folder(model):
    c = comps(model)
    return len(c) >= 4 and c[0] == 'landscape' and c[1] == 'roads' and c[2] in ('highwayoverpass', 'bridge')


def rot_matrix(rx, ry, rz):
    """NifSkope Matrix::fromEuler(-rx, -ry, -rz) = Rx(-rx) Ry(-ry) Rz(-rz), applied as M * v."""
    x, y, z = -rx, -ry, -rz
    sx, cx, sy, cy, sz, cz = math.sin(x), math.cos(x), math.sin(y), math.cos(y), math.sin(z), math.cos(z)
    return np.array([[cy * cz, -cy * sz, sy],
                     [sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
                     [sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy]])


# ------------------------------------------------------------------------------------------ BGSM

def bgsm_read(b):
    """dict(tex0, uvOff, uvScale, alphaTest, alphaRef, blend, decal) or None."""
    try:
        if b[:4] != b'BGSM':
            return None
        o = 4
        ver, = struct.unpack_from('<I', b, o); o += 4
        tile, = struct.unpack_from('<I', b, o); o += 4
        uo = struct.unpack_from('<2f', b, o); o += 8
        us = struct.unpack_from('<2f', b, o); o += 8
        o += 4                                            # alpha
        blend = b[o] != 0; o += 1 + 8
        aref = b[o]; atest = b[o + 1] != 0
        o += 6
        decal = b[o] != 0; o += 1
        o += 3 + 1 + 1 + 4 + 1
        if ver < 10:
            o += 4
        o += 1
        if ver >= 6:
            o += 1
        tex = []
        for _ in range(10 if ver >= 17 else 9):
            n, = struct.unpack_from('<I', b, o); o += 4
            tex.append(b[o:o + n].split(b'\0')[0].decode('latin-1')); o += n
        return {'tex0': tex[0], 'uvOff': uo, 'uvScale': us, 'alphaTest': atest, 'alphaRef': aref,
                'blend': blend, 'decal': decal, 'ver': ver, 'tile': tile}
    except (struct.error, IndexError):
        return None


def mat_path(name):
    p = name.replace(BS, '/')
    i = p.lower().rfind('materials/')
    if i > 0:
        p = p[i:]
    elif i < 0:
        p = 'materials/' + p
    return p


# ------------------------------------------------------------------------------------------ NIF

def nif_shapes(data, T):
    """Every drawn BSTriShape: model-space pos (N,3), uv (N,2), col (N,4) or None, tris (M,3), facts."""
    bsv, strings, blocks = nlc.nif_header(data)

    def name_of(i):
        if i < 0 or i >= len(blocks):
            return ''
        s = struct.unpack_from('<i', data, blocks[i][1])[0]
        return strings[s] if 0 <= s < len(strings) else ''

    def avobject(o):
        ne, = struct.unpack_from('<I', data, o + 4)
        o += 8 + 4 * ne + 4 + 4
        t = struct.unpack_from('<3f', data, o)
        r = struct.unpack_from('<9f', data, o + 12)
        s, = struct.unpack_from('<f', data, o + 48)
        return o + 52 + 4, (np.array(t), np.array(r).reshape(3, 3), s)

    parent, xform = {}, {}
    for i, (tn, st, sz) in enumerate(blocks):
        if T.inherits(tn, 'NiNode'):
            o, x = avobject(st)
            xform[i] = x
            nc, = struct.unpack_from('<I', data, o)
            for k in struct.unpack_from('<%di' % nc, data, o + 4):
                if 0 <= k < len(blocks) and k not in parent:
                    parent[k] = i
    out = []
    for i, (tn, st, sz) in enumerate(blocks):
        if not T.inherits(tn, 'BSTriShape'):
            continue
        o, own = avobject(st)
        o += 16 + 4
        shader, alpha = struct.unpack_from('<ii', data, o); o += 8
        vdesc, = struct.unpack_from('<Q', data, o); o += 8
        ntri, = struct.unpack_from('<I', data, o); o += 4
        nv, = struct.unpack_from('<H', data, o); o += 2
        dsize, = struct.unpack_from('<I', data, o); o += 4
        if not nv or not dsize or not ntri:
            continue
        b, marker = i, False
        for _ in range(64):
            if name_of(b).lower().startswith('editormarker'):
                marker = True
                break
            if b not in parent:
                break
            b = parent[b]
        if marker:
            continue
        stride = (vdesc & 0xF) * 4
        uvo = ((vdesc >> 8) & 0xF) * 4
        colo = ((vdesc >> 24) & 0xF) * 4
        vflags = (vdesc >> 44) & 0xFFFF
        full = bool(vflags & 0x400)
        hascol = bool(vflags & 0x20)
        raw = np.frombuffer(data, dtype=np.uint8, count=nv * stride, offset=o).reshape(nv, stride)
        if full:
            pos = raw[:, 0:12].copy().view('<f4').reshape(nv, 3).astype(np.float64)
        else:
            pos = raw[:, 0:6].copy().view('<f2').reshape(nv, 3).astype(np.float64)
        uv = raw[:, uvo:uvo + 4].copy().view('<f2').reshape(nv, 2).astype(np.float64)
        col = raw[:, colo:colo + 4].astype(np.float64) / 255.0 if hascol else None
        tris = np.frombuffer(data, dtype='<u2', count=3 * ntri, offset=o + nv * stride).reshape(ntri, 3).astype(np.int64)
        # transform: own, then the NiNode chain
        t0, r0, s0 = own
        pos = (pos @ r0.T) * s0 + t0
        b = i
        seen = set()
        while b in parent and b not in seen:
            seen.add(b)
            b = parent[b]
            if b in xform:
                t, r, s = xform[b]
                pos = (pos @ r.T) * s + t
        f = {'block': i, 'name': name_of(i), 'type': tn, 'pos': pos, 'uv': uv, 'col': col, 'tris': tris,
             'hascol': hascol, 'vc': False, 'va': False, 'mat': '', 'tex0': '', 'uvOff': (0.0, 0.0),
             'uvScale': (1.0, 1.0), 'alphaFlags': 0, 'alphaThr': 0, 'hasAlpha': False, 'effect': False,
             'sf1': 0, 'sf2': 0}
        if 0 <= shader < len(blocks):
            stn, sst, _ = blocks[shader]
            f['effect'] = T.inherits(stn, 'BSEffectShaderProperty')
            so = sst + (4 if stn == 'BSLightingShaderProperty' else 0)
            sidx, = struct.unpack_from('<i', data, so)
            f['mat'] = strings[sidx] if 0 <= sidx < len(strings) else ''
            ne, = struct.unpack_from('<I', data, so + 4)
            so += 8 + 4 * ne + 4
            sf1, sf2 = struct.unpack_from('<II', data, so)
            f['sf1'], f['sf2'] = sf1, sf2
            f['vc'] = bool(sf2 & (1 << 5))
            f['va'] = bool(sf1 & (1 << 3))
            if stn == 'BSLightingShaderProperty':
                f['uvOff'] = struct.unpack_from('<2f', data, so + 8)
                f['uvScale'] = struct.unpack_from('<2f', data, so + 16)
                ts, = struct.unpack_from('<i', data, so + 24)
                if 0 <= ts < len(blocks) and blocks[ts][0] == 'BSShaderTextureSet':
                    to = blocks[ts][1]
                    n, = struct.unpack_from('<I', data, to)
                    if n:
                        ln, = struct.unpack_from('<I', data, to + 4)
                        f['tex0'] = data[to + 8:to + 8 + ln].split(b'\0')[0].decode('latin-1')
        if 0 <= alpha < len(blocks) and blocks[alpha][0] == 'NiAlphaProperty':
            ast = blocks[alpha][1]
            ne, = struct.unpack_from('<I', data, ast + 4)
            ao = ast + 8 + 4 * ne + 4
            f['alphaFlags'], = struct.unpack_from('<H', data, ao)
            f['alphaThr'] = data[ao + 2]
            f['hasAlpha'] = True
        out.append(f)
    return out


# ------------------------------------------------------------------------------------------ DDS

def legacy_dds(data):
    """Pillow 10.0.1 reads BC1/BC3/BC5 only behind a legacy FourCC: rewrite a DX10 header of those formats."""
    if data[84:88] != b'DX10':
        return data
    fmt, = struct.unpack_from('<I', data, 128)
    four = {71: b'DXT1', 72: b'DXT1', 74: b'DXT3', 75: b'DXT3', 77: b'DXT5', 78: b'DXT5', 83: b'ATI2', 84: b'ATI2'}.get(fmt)
    if not four:
        return data
    return data[:84] + four + data[88:128] + data[148:]


class Tex:
    """Top mip decoded by Pillow, the chain box-filtered in the file's own (gamma) encoding, float 0..1 RGBA."""

    def __init__(self, data):
        from PIL import Image
        import io
        data = legacy_dds(data)
        im = Image.open(io.BytesIO(data))
        im = im.convert('RGBA')
        a = np.asarray(im).astype(np.float64) / 255.0
        self.w, self.h = im.size
        self.mips = [a]
        while a.shape[0] > 1 or a.shape[1] > 1:
            h2, w2 = max(1, a.shape[0] // 2), max(1, a.shape[1] // 2)
            if a.shape[0] >= 2 and a.shape[1] >= 2:
                a = a[:h2 * 2, :w2 * 2].reshape(h2, 2, w2, 2, 4).mean((1, 3))
            elif a.shape[0] >= 2:
                a = a[:h2 * 2].reshape(h2, 2, a.shape[1], 4).mean(1)
            else:
                a = a[:, :w2 * 2].reshape(a.shape[0], w2, 2, 4).mean(2)
            self.mips.append(a)
        self.maxmip = len(self.mips) - 1
        self.mean = self.mips[-1][0, 0]

    def _bil(self, m, u, v):
        a = self.mips[m]
        h, w = a.shape[:2]
        x = u * w - 0.5
        y = v * h - 0.5
        x0 = math.floor(x); y0 = math.floor(y)
        fx = x - x0; fy = y - y0
        x0 %= w; y0 %= h
        x1 = (x0 + 1) % w; y1 = (y0 + 1) % h
        return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x1] * fx * (1 - fy)
                + a[y1, x0] * (1 - fx) * fy + a[y1, x1] * fx * fy)

    def sample(self, u, v, mip):
        mip = min(max(mip, 0.0), float(self.maxmip))
        m0 = int(math.floor(mip)); t = mip - m0
        c = self._bil(m0, u, v)
        if t > 0 and m0 < self.maxmip:
            c = c * (1 - t) + self._bil(m0 + 1, u, v) * t
        return c


# ------------------------------------------------------------------------------------------ the world

class Reader:
    def __init__(self):
        plugins, stack, _e, _d, _s, _m = nlc.lo.expected(PROFILE, MODS, DATA)
        self.plugins = plugins
        self.S = nlc.Stack(stack, plugins)
        self.W = nlc.World(plugins, 0x3C)
        self.T = nlc.NifTypes(NIFXML)
        self.models = {}
        self.mats = {}
        self.texs = {}

    def plugin_name(self, pi):
        return os.path.basename(self.plugins[pi])

    def base_info(self, form):
        b = self.W.base(form)
        if not b:
            return None
        t, flags, fl, pi, masters = b
        info = {'type': t, 'flags': flags, 'modl': '', 'edid': '', 'hasLod': False, 'parts': [], 'plugin': pi,
                'mods': 0}
        for ft, pl in fl:
            if ft == b'EDID':
                info['edid'] = pl.split(b'\0')[0].decode('latin-1')
            elif ft == b'MODL' and not info['modl']:
                info['modl'] = pl.split(b'\0')[0].decode('latin-1')
            elif ft == b'MODS' and len(pl) >= 4:
                info['mods'] = self.W._gmap(pi, masters, struct.unpack_from('<I', pl, 0)[0])
            elif ft == b'MNAM' and t == 'STAT':
                info['hasLod'] = any(pl[k * 260:k * 260 + 1] not in (b'\0', b'') for k in range(len(pl) // 260))
            elif ft == b'ONAM' and t == 'SCOL' and len(pl) >= 4:
                info['parts'].append([self.W._gmap(pi, masters, struct.unpack_from('<I', pl, 0)[0]), []])
            elif ft == b'DATA' and t == 'SCOL' and info['parts']:
                for k in range(len(pl) // 28):
                    info['parts'][-1][1].append(struct.unpack_from('<7f', pl, k * 28))
        # header bit 15 = Has Distant LOD
        if t == 'STAT' and (flags & 0x8000):
            info['hasLod'] = True
        return info

    def refs_in(self, x0, y0, x1, y1):
        """(form, ref dict, cell x, cell y) for every REFR placed in cells x0..x1, y0..y1 (persistent by position)."""
        out = []
        for form, r in self.W.refs.items():
            c = self.W.cells.get(r['cell'])
            if c is None:
                continue
            if c['persistent'] or c['x'] is None:
                cx, cy = int(r['pos'][0] // 4096), int(r['pos'][1] // 4096)
            else:
                cx, cy = c['x'], c['y']
            if x0 <= cx <= x1 and y0 <= cy <= y1:
                out.append((form, r, cx, cy))
        return out

    def model(self, modl):
        key = modl.lower().replace('/', BS)
        if key in self.models:
            return self.models[key]
        rel = key if key.startswith('meshes' + BS) else 'meshes' + BS + key
        data = self.S.read(rel)
        shapes = None
        if data is not None:
            try:
                shapes = nif_shapes(data, self.T)
            except Exception as e:  # noqa: BLE001
                shapes = []
                print('NIF parse failed', modl, e)
        self.models[key] = shapes
        return shapes

    def material(self, name):
        if not name.lower().endswith('.bgsm'):
            return None
        p = mat_path(name).lower()
        if p not in self.mats:
            b = self.S.read(p)
            self.mats[p] = bgsm_read(b) if b else None
        return self.mats[p]

    def texture(self, path):
        if not path:
            return None
        p = path.replace('/', BS).lower()
        if not p.startswith('textures' + BS):
            p = 'textures' + BS + p
        if p not in self.texs:
            b = self.S.read(p)
            t = None
            if b:
                try:
                    t = Tex(b)
                except Exception as e:  # noqa: BLE001
                    print('DDS decode failed', p, e)
            self.texs[p] = t
        return self.texs[p]


def placements(R, x0, y0, x1, y1):
    """Every STAT placement (SCOL parts expanded) whose REFR sits in the cells. Yields dicts."""
    for form, r, cx, cy in R.refs_in(x0, y0, x1, y1):
        bi = R.base_info(r['base'])
        if not bi:
            continue
        m = rot_matrix(*r['rot'])
        p = np.array(r['pos'])
        common = {'ref': form, 'refFlags': r['flags'], 'plugin': R.plugin_name(r['plugin']),
                  'origin': R.plugin_name(r['origin']), 'xmsp': r.get('xmsp', 0), 'cx': cx, 'cy': cy,
                  'refBase': r['base'], 'refType': bi['type']}
        if bi['type'] == 'SCOL':
            ordinal = 0
            for pbase, pls in bi['parts']:
                pb = R.base_info(pbase)
                for pl in pls:
                    pm = rot_matrix(*pl[3:6])
                    d = dict(common)
                    d.update({'part': ordinal, 'base': pbase, 'info': pb,
                              'pos': p + m @ (np.array(pl[:3]) * r['scale']), 'rot': m @ pm,
                              'scale': r['scale'] * pl[6]})
                    ordinal += 1
                    yield d
            continue
        d = dict(common)
        d.update({'part': -1, 'base': r['base'], 'info': bi, 'pos': p, 'rot': m, 'scale': r['scale']})
        yield d


def road_decision(d, sidewalks=True):
    """What the lodgen road gather does with this placement (LodgenRoadSet::gather/addPlacement), in words."""
    if d['refFlags'] & 0x20:
        return 'deleted'
    if d['refFlags'] & 0x800:
        return 'initially-disabled'
    bi = d['info']
    if not bi:
        return 'no-base'
    if bi['type'] != 'STAT':
        return 'not-a-road-model (%s)' % bi['type']
    if not is_road(bi['modl']):
        return 'not-a-road-model'
    if bi['hasLod'] or is_raised_folder(bi['modl']):
        return 'refused raised-haslod' if bi['hasLod'] else 'refused raised-folder'
    if not sidewalks and is_sidewalk(bi['modl']):
        return 'refused sidewalk'
    return 'stamped'
