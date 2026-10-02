#!/usr/bin/env python3
"""Placed decals in the cell view (lane PLACED1, 2026-10-02; tests/spells/cell_decal.sh).

  cell_decal_check.py <Fallout4.esm> <data root> <interior EDID> <shot dir> <look-at x,y,z> [stages, default KGNC]
  cell_decal_check.py <Fallout4.esm> <data root> <interior EDID> --walk      the walk alone, no shots

The shot dir holds off.png (WW_CELL_DECAL_RED=none: no decal drawn, the reference), on.png (the run under test),
on.notes (its census), on.decals.txt (WW_CELL_DECAL_DUMP) and cam.txt (WW_CELL_CAM_DUMP: "cam=x,y,z").
Everything else is this file's OWN read of the plugin and the loose meshes (nothing shared with src/):

  THE DECALS  every REFR of the cell whose base is a TXST carrying DODT, not deleted, shown (the initially
              disabled flag, turned round by an opposite-state enable parent, as the cell view shows refs).
  THE BOX     the reference's frame, world = R * local with R = euler(-rx, -ry, -rz): projection axis local +Y,
              width axis local +X, height axis local -Z.
              With an XPRM box: centre = the reference position, width = 2 * bounds.x, height = 2 * bounds.z,
              depth = 2 * bounds.y (the bounds are half extents).
              Without: this file's own ray along +Y, 1000 units, from 1 unit behind the reference position (the
              view's named allowance for a decal placed exactly on its surface), against the opaque triangles
              of every placed model (below); centre = the nearest hit, width and height = DODT's
              sizes times XPDD's pair, depth = DODT's depth.
  THE RULE    a triangle inside the box takes the decal when its face looks back along the axis (dot > 0) and,
              under 0.3, when one of its vertex normals still passes (dot(N, -Y) - 0.3) / 0.25 > 0.
  EXCEPTIONS, each one named and counted:
    dice      DODT min and max sizes differ with no box, or the base does not take the whole texture: the
              game rolls for it, the cell view refuses it ("random size or sheet quarter")
    not a box an XPRM of another kind ("primitive is not a box")
    no surface the ray meets nothing, or no triangle in the box passes the rule
  THE RECEIVERS  every shown REFR whose base names a model (collections through their parts), the loose mesh
              under <data root>/Meshes, every triangle shape with a lighting shader that is neither blended
              nor cut out (its material file when one is on disk, else its alpha property), editor markers
              skipped.
Stages:
  K  counts: the census reads as many decals as the walk finds, drawn + refused = read, the dice and not-a-box
     refusals equal the walk's, and drawn = the walk's expected (read less the named exceptions) within
     KTOL decals (the walk's ray meets a few surfaces the weld does not carry and the other way round; the
     differing forms are printed)
  G  geometry: every decal both sides draw has its box centre within 1 unit and its sizes within 0.5% of the
     walk's, on >= 98% of them (the rest printed)
  N  nothing else moves: outside the walk's boxes on screen (+ 3 px), |on - off| <= 3/255 on >= 99.5%
  C  the decals show: of the decals the camera SEES (box centre on screen, within 900 units, turned toward the
     eye, nothing opaque in front by this file's own ray), >= 80% change 30 or more pixels (> 3/255) inside
     their own projected box; fewer than 3 seen decals is nothing to judge
A stage the shot does not name is measured and printed "n/a" and does not count. A named stage with nothing to
judge FAILS. Prints one line per stage, then "decal PASS" or "decal FAIL".
"""
import math
import os
import re
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import euler, record, walk  # noqa: E402  (the plugin walk only)

RAY = 1000.0
SLACK = 1.0     # the ray starts this far behind the reference (a decal placed ON its surface still meets it)
KTOL = 0
MODEL_TYPES = (b'STAT', b'SCOL', b'MSTT', b'ACTI', b'FURN', b'DOOR', b'MISC', b'CONT', b'FLOR', b'TERM',
               b'LIGH', b'TREE', b'ALCH', b'AMMO', b'WEAP', b'BOOK', b'KEYM', b'NOTE', b'TACT', b'ARMO')
NODES = ('NiNode', 'BSFadeNode', 'BSLeafAnimNode', 'BSOrderedNode', 'BSMultiBoundNode', 'BSValueNode',
         'NiBillboardNode', 'NiSwitchNode', 'NiLODNode', 'BSBlastNode', 'BSDamageStage', 'BSTreeNode',
         'NiSortAdjustNode', 'BSRangeNode', 'BSDebrisNode')
SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape')


def z(b):
    return b.split(b'\0')[0].decode('cp1252', 'replace')


# ---- the loose meshes: header, node tree, triangle shapes (NIF 20.2.0.7, BS version 130)
def nif_blocks(b):
    off = b.index(b'\n') + 1 + 4 + 1
    _user, nblocks, bsver = struct.unpack_from('<III', b, off)
    off += 12
    for k in range(4 if bsver >= 103 else 3):
        off += 1 + b[off]
        if k == 0 and bsver > 130:
            off += 4
    ntypes, = struct.unpack_from('<H', b, off)
    off += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from('<I', b, off)
        types.append(b[off + 4:off + 4 + n].decode('latin1'))
        off += 4 + n
    tidx = struct.unpack_from('<%dH' % nblocks, b, off)
    off += 2 * nblocks
    sizes = struct.unpack_from('<%dI' % nblocks, b, off)
    off += 4 * nblocks
    nstr, _maxlen = struct.unpack_from('<II', b, off)
    off += 8
    strings = []
    for _ in range(nstr):
        n, = struct.unpack_from('<I', b, off)
        strings.append(b[off + 4:off + 4 + n].decode('latin1'))
        off += 4 + n
    ngroups, = struct.unpack_from('<I', b, off)
    off += 4 + 4 * ngroups
    blocks = []
    for i in range(nblocks):
        blocks.append((types[tidx[i] & 0x7FFF], off, sizes[i]))
        off += sizes[i]
    return blocks, strings


def av_object(b, off, strings):
    name_i, nextra = struct.unpack_from('<iI', b, off)
    off += 8 + 4 * nextra + 4
    flags, = struct.unpack_from('<I', b, off)
    off += 4
    t = np.array(struct.unpack_from('<3f', b, off), np.float64)
    r = np.array(struct.unpack_from('<9f', b, off + 12), np.float64).reshape(3, 3)
    s, = struct.unpack_from('<f', b, off + 48)
    off += 52 + 4
    return (strings[name_i] if 0 <= name_i < len(strings) else ''), flags, (r, t, s), off


def material_alpha(data, name):
    """(blend, test) from the material file a shader names, or None when there is none on disk."""
    n = name.replace('\\', '/').lower()
    if not n.endswith('.bgsm'):
        return None
    if not n.startswith('materials/'):
        n = 'materials/' + (n.split('/materials/', 1)[1] if '/materials/' in n else n)
    p = os.path.join(data, n)
    if not os.path.isfile(p):
        return None
    m = open(p, 'rb').read(64)
    if m[:4] != b'BGSM' or len(m) < 43:
        return None
    return m[32] != 0, m[42] != 0


_models = {}


def model_shapes(data, model):
    """[(positions Nx3, normals Nx3, triangles Mx3)] in model space: the opaque lighting shapes of one mesh."""
    key = model.lower()
    if key in _models:
        return _models[key]
    out = []
    _models[key] = out
    p = os.path.join(data, 'Meshes', model.replace('\\', '/'))
    if not os.path.isfile(p):
        return out
    b = open(p, 'rb').read()
    try:
        blocks, strings = nif_blocks(b)
    except (struct.error, IndexError, ValueError):
        return out
    info, parent = {}, {}
    for i, (t, off, _size) in enumerate(blocks):
        if t in NODES or t in SHAPES:
            name, flags, xf, o = av_object(b, off, strings)
            info[i] = dict(type=t, name=name, flags=flags, xf=xf, o=o)
            if t in NODES:
                n, = struct.unpack_from('<I', b, o)
                for k in struct.unpack_from('<%di' % n, b, o + 4):
                    if k >= 0:
                        parent[k] = i
    for i, rec in info.items():
        if rec['type'] not in SHAPES:
            continue
        o = rec['o'] + 16
        _skin, shader, alpha = struct.unpack_from('<3i', b, o)
        desc, ntri, nv, _dsize = struct.unpack_from('<QIHI', b, o + 12)
        o += 12 + 18
        stride, va = (desc & 0xF) * 4, (desc >> 44) & 0xFFF
        if not (va & 1) or nv == 0 or ntri == 0 or stride == 0:
            continue
        if not (0 <= shader < len(blocks)) or blocks[shader][0] != 'BSLightingShaderProperty':
            continue
        so = blocks[shader][1] + 4
        name_i, = struct.unpack_from('<i', b, so)
        mat = material_alpha(data, strings[name_i] if 0 <= name_i < len(strings) else '')
        if mat is None:
            mat = (False, False)
            if 0 <= alpha < len(blocks) and blocks[alpha][0] == 'NiAlphaProperty':
                ao = blocks[alpha][1]
                nex, = struct.unpack_from('<I', b, ao + 4)
                fl, = struct.unpack_from('<H', b, ao + 8 + 4 * nex + 4)
                mat = (bool(fl & 1), bool(fl & 0x200))
        if mat[0] or mat[1]:
            continue
        # the chain to the root: hidden nodes and editor markers carry nothing
        xf, k, skip = rec['xf'], parent.get(i), rec['name'].lower().startswith('editormarker') or rec['flags'] & 1
        while k is not None and not skip:
            n = info[k]
            skip = n['name'].lower().startswith('editormarker') or n['flags'] & 1
            ra, ta, sa = n['xf']
            xf = (ra @ xf[0], ra @ xf[1] * sa + ta, sa * xf[2])
            k = parent.get(k)
        if skip:
            continue
        full = bool(va & 0x400)
        pos = np.ndarray((nv, 3), '<f4' if full else '<f2', b, o, (stride, 4 if full else 2)).astype(np.float64)
        no = o + (16 if full else 8) + (4 if va & 2 else 0) + (4 if va & 4 else 0)
        if va & 8:
            nrm = np.ndarray((nv, 3), 'u1', b, no, (stride, 1)).astype(np.float64) / 255.0 * 2.0 - 1.0
        else:
            nrm = np.zeros((nv, 3))
        tris = np.frombuffer(b, '<u2', ntri * 3, o + nv * stride).reshape(-1, 3).astype(np.int64)
        if tris.max() >= nv:
            continue
        r, t, s = xf
        out.append((pos @ r.T * s + t, nrm @ r.T, tris))
    return out


# ---- the plugin walk
def cell_walk(esm, cell_edid):
    buf = open(esm, 'rb').read()
    cell_form, txst, base_at, refs = None, {}, {}, []
    for t, form, off, stack in walk(buf):
        if t == b'REFR':
            if cell_form is not None and any(g[1] == cell_form and g[2] in (6, 8, 9) for g in stack):
                flags, f = record(buf, off)
                refs.append((form, flags, f))
        elif t == b'CELL':
            if cell_form is None and all(g[2] != 1 for g in stack):
                _, f = record(buf, off)
                if z(f.get(b'EDID', b'')) == cell_edid:
                    cell_form = form
        elif t == b'TXST':
            _, f = record(buf, off)
            if b'DODT' in f and len(f[b'DODT']) >= 36:
                txst[form] = f
        elif t in MODEL_TYPES:
            base_at[form] = (t, off)
    if cell_form is None:
        raise SystemExit(f'no interior named {cell_edid}')
    return buf, txst, base_at, refs


def shown(flags, f):
    if flags & 0x20 or b'NAME' not in f or b'DATA' not in f:
        return False
    off = bool(flags & 0x800)
    if b'XESP' in f and len(f[b'XESP']) >= 8 and struct.unpack_from('<I', f[b'XESP'], 4)[0] & 1:
        off = not off
    return not off


def is_marker(model):
    m = model.replace('\\', '/').lower()
    return m.startswith('markers/') or '/markers/' in m or 'marker' in os.path.basename(m)


def receivers_of(buf, data, base_at, refs):
    """World triangles (T x 3 x 3), their vertex normals (T x 3 x 3)."""
    models, scols = {}, {}

    def base(form):
        if form in models or form in scols or form not in base_at:
            return
        t, off = base_at[form]
        _, f = record(buf, off)
        if t == b'SCOL':
            parts, data_fields = [], []
            flds = buf_fields(buf, off)
            cur = None
            for name, payload in flds:
                if name == b'ONAM':
                    cur = struct.unpack_from('<I', payload)[0]
                elif name == b'DATA' and cur is not None:
                    parts.append((cur, np.frombuffer(payload, '<f4').reshape(-1, 7)))
                    cur = None
            scols[form] = parts
            for part, _ in parts:
                base(part)
        else:
            models[form] = z(f.get(b'MODL', b''))

    tri_p, tri_n = [], []

    def place(form, R, pos, scale):
        m = models.get(form, '')
        if not m or is_marker(m):
            return
        for p, n, tris in model_shapes(data, m):
            w = p @ R.T * scale + pos
            tri_p.append(w[tris])
            tri_n.append((n @ R.T)[tris])

    for _form, flags, f in refs:
        if not shown(flags, f):
            continue
        b = struct.unpack_from('<I', f[b'NAME'])[0]
        base(b)
        pos = np.array(struct.unpack_from('<3f', f[b'DATA'], 0), np.float64)
        rot = struct.unpack_from('<3f', f[b'DATA'], 12)
        scale = struct.unpack_from('<f', f[b'XSCL'])[0] if b'XSCL' in f else 1.0
        R = euler(-rot[0], -rot[1], -rot[2])
        if b in scols:
            for part, rows in scols[b]:
                for px, py, pz, rx, ry, rz, ps in rows:
                    place(part, R @ euler(-rx, -ry, -rz), pos + R @ np.array([px, py, pz]) * scale, scale * ps)
        else:
            place(b, R, pos, scale)
    if not tri_p:
        return np.zeros((0, 3, 3)), np.zeros((0, 3, 3))
    return np.concatenate(tri_p), np.concatenate(tri_n)


def buf_fields(buf, off):
    """The fields of one record in file order (repeats kept)."""
    import zlib
    size, flags = struct.unpack_from('<II', buf, off + 4)
    d = buf[off + 24:off + 24 + size]
    if flags & 0x00040000:
        d = zlib.decompress(d[4:])
    out, o, big = [], 0, 0
    while o + 6 <= len(d):
        name = d[o:o + 4]
        n, = struct.unpack_from('<H', d, o + 4)
        if name == b'XXXX':
            big, = struct.unpack_from('<I', d, o + 6)
            o += 6 + n
            continue
        if big:
            n, big = big, 0
        out.append((name, d[o + 6:o + 6 + n]))
        o += 6 + n
    return out


class Soup:
    """The receivers, with a ray and a box query."""

    def __init__(self, tp, tn):
        self.p, self.n = tp, tn
        self.lo, self.hi = tp.min(axis=1), tp.max(axis=1)
        e1, e2 = tp[:, 1] - tp[:, 0], tp[:, 2] - tp[:, 0]
        g = np.cross(e1, e2)
        ln = np.linalg.norm(g, axis=1)
        self.ok = ln > 1.0e-9
        self.g = g / np.where(self.ok, ln, 1.0)[:, None]
        # a mirrored node winds the other way: the shading normals say which side is the front
        flip = (self.g * tn.sum(axis=1)).sum(axis=1) < 0.0
        self.g[flip] *= -1.0

    def near(self, lo, hi):
        return np.nonzero(self.ok & (self.lo <= hi).all(axis=1) & (self.hi >= lo).all(axis=1))[0]

    def ray(self, o, d):
        """The nearest hit along o + t d, 0 <= t <= RAY, either side; -1 when none."""
        for reach in (64.0, 256.0, RAY + SLACK):
            e = o + d * reach
            idx = self.near(np.minimum(o, e), np.maximum(o, e))
            if len(idx):
                p0 = self.p[idx, 0]
                e1, e2 = self.p[idx, 1] - p0, self.p[idx, 2] - p0
                h = np.cross(d, e2)
                a = (e1 * h).sum(axis=1)
                good = np.abs(a) > 1.0e-12
                f = 1.0 / np.where(good, a, 1.0)
                s = o - p0
                u = f * (s * h).sum(axis=1)
                q = np.cross(s, e1)
                v = f * (q @ d)
                t = f * (e2 * q).sum(axis=1)
                hit = good & (u >= 0.0) & (v >= 0.0) & (u + v <= 1.0) & (t >= 0.0) & (t <= reach)
                if hit.any():
                    return float(t[hit].min())
        return -1.0

    def takes(self, c, axes, half):
        """Does one triangle inside the box take the decal (the rule in the docstring)?"""
        aw, ah, ad = axes
        ext = np.abs(aw) * half[0] + np.abs(ah) * half[1] + np.abs(ad) * half[2]
        A = np.stack([aw, ah, ad])
        for i in self.near(c - ext, c + ext):
            gd = -float(self.g[i] @ ad)
            if gd <= 0.0:
                continue
            if gd < 0.3 and not ((-(self.n[i] / np.maximum(np.linalg.norm(self.n[i], axis=1), 1e-9)[:, None]) @ ad
                                  - 0.3) / 0.25 > 0.0).any():
                continue
            poly = [(self.p[i, k] - c) @ A.T for k in range(3)]
            for ax in range(3):
                for sign in (1.0, -1.0):
                    nxt = []
                    for k in range(len(poly)):
                        a, b = poly[k], poly[(k + 1) % len(poly)]
                        da, db = half[ax] - sign * a[ax], half[ax] - sign * b[ax]
                        if da >= 0.0:
                            nxt.append(a)
                        if (da >= 0.0) != (db >= 0.0):
                            nxt.append(a + (b - a) * (da / (da - db)))
                    poly = nxt
                    if len(poly) < 3:
                        break
                if len(poly) < 3:
                    break
            if len(poly) >= 3:
                return True
        return False


def decals_of(esm, data, cell):
    """[dict(form, fate, centre, axes, size)] by the docstring's rule; fate in drawn/dice/notbox/nosurface."""
    buf, txst, base_at, refs = cell_walk(esm, cell)
    tp, tn = receivers_of(buf, data, base_at, refs)
    soup = Soup(tp, tn)
    out = []
    for form, flags, f in refs:
        if not shown(flags, f):
            continue
        b = struct.unpack_from('<I', f[b'NAME'])[0]
        if b not in txst:
            continue
        d = txst[b][b'DODT']
        minw, maxw, minh, maxh, depth = struct.unpack_from('<5f', d)
        whole = bool(d[29] & 0x08)
        prim = f.get(b'XPRM')
        rec = dict(form=form, base=z(txst[b].get(b'EDID', b'')), box=prim is not None)
        out.append(rec)
        if not whole or (prim is None and (minw != maxw or minh != maxh)):
            rec['fate'] = 'dice'
            continue
        if prim is not None and (len(prim) < 32 or struct.unpack_from('<I', prim, 28)[0] != 1):
            rec['fate'] = 'notbox'
            continue
        pos = np.array(struct.unpack_from('<3f', f[b'DATA'], 0), np.float64)
        rot = struct.unpack_from('<3f', f[b'DATA'], 12)
        R = euler(-rot[0], -rot[1], -rot[2])
        axes = (R[:, 0], -R[:, 2], R[:, 1])
        if prim is not None:
            hx, hy, hz = struct.unpack_from('<3f', prim)
            size, centre, dist = (2.0 * hx, 2.0 * hz, 2.0 * hy), pos, -1.0
        else:
            sw, sh = struct.unpack_from('<2f', f[b'XPDD']) if b'XPDD' in f and len(f[b'XPDD']) >= 8 else (1.0, 1.0)
            dist = soup.ray(pos - axes[2] * SLACK, axes[2])
            if dist < 0.0:
                rec['fate'] = 'nosurface'
                rec['why'] = 'the ray meets nothing'
                continue
            dist -= SLACK
            size, centre = (minw * sw, minh * sh, depth), pos + axes[2] * dist
        rec.update(centre=centre, axes=axes, size=size, dist=dist)
        if min(size) <= 0.0 or not soup.takes(centre, axes, np.array(size) * 0.5):
            rec['fate'] = 'nosurface'
            rec['why'] = 'no triangle in the box passes the rule'
            continue
        rec['fate'] = 'drawn'
    return out, soup


def clip_near(poly, near):
    out = []
    for k in range(len(poly)):
        a, b = poly[k], poly[(k + 1) % len(poly)]
        if a[2] >= near:
            out.append(a)
        if (a[2] >= near) != (b[2] >= near):
            out.append(a + (b - a) * ((near - a[2]) / (b[2] - a[2])))
    return out


def main():
    esm, data, cell = sys.argv[1:4]
    decals, soup = decals_of(esm, data, cell)
    ntris = len(soup.p)
    want = {k: [d for d in decals if d['fate'] == k] for k in ('drawn', 'dice', 'notbox', 'nosurface')}
    print(f"walk: {len(decals)} placed decals ({sum(d['box'] for d in decals)} with a box), expected drawn "
          f"{len(want['drawn'])}; named exceptions: dice {len(want['dice'])}, not a box {len(want['notbox'])}, "
          f"no surface {len(want['nosurface'])}; {ntris} opaque triangles read")
    if sys.argv[4] == '--walk':
        dist = np.array([d['dist'] for d in want['drawn'] if d.get('dist', -1.0) >= 0.0])
        if len(dist):
            print('ray distance: median %.2f, 90%% %.2f, max %.2f; over 64: %d' % (
                np.median(dist), np.percentile(dist, 90), dist.max(), int((dist > 64).sum())))
        for d in want['nosurface']:
            print('  no surface %08x %s: %s' % (d['form'], d['base'], d['why']))
        if len(sys.argv) > 5:
            with open(sys.argv[5], 'w') as fh:
                for d in decals:
                    c = d.get('centre')
                    fh.write('%08x %s %s\n' % (d['form'], d['fate'], '' if c is None else '%.3f %.3f %.3f %.3f' % (
                        c[0], c[1], c[2], d['dist'])))
        return 0
    from PIL import Image, ImageDraw
    run, at = sys.argv[4:6]
    stages = (sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else 'KGNC').upper()
    word = lambda st, good: ('PASS' if good else 'FAIL') if st in stages else 'n/a '
    ok = True

    # ---- K: the census against the walk
    notes = open(f'{run}/on.notes', errors='replace').read()
    m = re.search(r'placed decals: (\d+) read, drawn (\d+) \((\d+) triangles; (\d+) by their box, (\d+) by a ray; '
                  r'(\d+) opaque triangles offered\), refused (\d+)([^\n\[]*)(\[RED CONTROL (\w+)\])?', notes)
    if not m:
        print('K FAIL  no "placed decals" line in the census')
        k_ok, drawn = False, -1
    else:
        read, drawn, _tris, _bybox, _byray, _offered, refused = (int(m.group(k)) for k in range(1, 8))
        why = {a.strip(): int(b) for a, b in re.findall(r'([a-z][a-z ]+?) (\d+)(?:,|$|\s*$)', m.group(8).lstrip(': '))}
        dice, notbox = why.get('random size or sheet quarter', 0), why.get('primitive is not a box', 0)
        nosurf = why.get('no surface within reach', 0) + why.get('nothing opaque in the box', 0)
        k_ok = (read == len(decals) and drawn + refused == read and dice == len(want['dice'])
                and notbox == len(want['notbox']) and abs(drawn - len(want['drawn'])) <= KTOL
                and drawn > 0 and not m.group(9))
        print(f"K {word('K', k_ok)}  census: read {read}, drawn {drawn}, refused {refused} (dice {dice}, not a box "
              f"{notbox}, no surface {nosurf}{', RED CONTROL ' + m.group(10) if m.group(9) else ''}); the walk: read "
              f"{len(decals)}, expected drawn {len(want['drawn'])} = read less dice {len(want['dice'])}, not a box "
              f"{len(want['notbox'])}, no surface {len(want['nosurface'])} (drawn within {KTOL})")
    ok &= k_ok or 'K' not in stages

    # ---- G: each drawn box against the walk's
    theirs = {}
    dump = f'{run}/on.decals.txt'
    if os.path.isfile(dump):
        for line in open(dump, errors='replace'):
            part = [p.split() for p in line.split('|')]
            if len(part) >= 6 and part[0][1] == '0':
                theirs[int(part[0][0], 16)] = (np.array([float(v) for v in part[1]]),
                                               np.array([float(v) for v in part[2]]))
    both = [d for d in want['drawn'] if d['form'] in theirs]
    only_walk = [d for d in want['drawn'] if d['form'] not in theirs]
    only_view = sorted(set(theirs) - {d['form'] for d in want['drawn']})
    good, bad = 0, []
    for d in both:
        c, s = theirs[d['form']]
        if np.linalg.norm(c - d['centre']) <= 1.0 and np.allclose(s, d['size'], rtol=0.005, atol=0.01):
            good += 1
        else:
            bad.append('%08x off by %.1f' % (d['form'], np.linalg.norm(c - d['centre'])))
    g_ok = len(both) > 0 and good >= 0.98 * len(both) and len(only_walk) + len(only_view) <= KTOL
    print(f"G {word('G', g_ok)}  {good} of {len(both)} boxes drawn by both sit within 1 unit and 0.5% of the walk's "
          f"(>= 98%); drawn by the walk alone {len(only_walk)}, by the view alone {len(only_view)} (<= {KTOL})")
    for t in bad[:8]:
        print('     ' + t)
    for d in only_walk[:8]:
        print('     walk alone %08x %s' % (d['form'], d['base']))
    for f in only_view[:8]:
        print('     view alone %08x' % f)
    ok &= g_ok or 'G' not in stages

    # ---- N and C: the pixels
    off = np.asarray(Image.open(f'{run}/off.png').convert('RGB')).astype(np.int32)
    on = np.asarray(Image.open(f'{run}/on.png').convert('RGB')).astype(np.int32)
    H, W = on.shape[:2]
    cam = np.array([float(v) for v in re.search(r'cam=([-\d.]+),([-\d.]+),([-\d.]+)',
                                                 open(f'{run}/cam.txt').read()).groups()])
    look = np.array([float(v) for v in at.split(',')])
    fwd = look - cam
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, [0.0, 0.0, 1.0])
    right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    focal = (H / 2.0) / math.tan(math.radians(35.0))
    view = np.stack([right, up, fwd])
    mask_img = Image.new('L', (W, H), 0)
    draw = ImageDraw.Draw(mask_img)
    faces = ((0, 1, 3, 2), (4, 5, 7, 6), (0, 1, 5, 4), (2, 3, 7, 6), (0, 2, 6, 4), (1, 3, 7, 5))
    on_screen = 0
    own = []
    for d in want['drawn']:
        polys = []
        aw, ah, ad = d['axes']
        hw, hh, hd = (v * 0.5 for v in d['size'])
        corners = [(d['centre'] + sx * hw * aw + sy * hh * ah + sz * hd * ad - cam) @ view.T
                   for sz in (-1, 1) for sy in (-1, 1) for sx in (-1, 1)]
        seen = False
        for face in faces:
            poly = clip_near([corners[k] for k in face], 2.0)
            if len(poly) < 3:
                continue
            pts = [(W / 2.0 + focal * p[0] / p[2], H / 2.0 - focal * p[1] / p[2]) for p in poly]
            if max(x for x, _ in pts) < 0 or min(x for x, _ in pts) >= W or \
                    max(y for _, y in pts) < 0 or min(y for _, y in pts) >= H:
                continue
            pts = [(min(max(x, -4.0 * W), 5.0 * W), min(max(y, -4.0 * H), 5.0 * H)) for x, y in pts]
            draw.polygon(pts, fill=255, outline=255)
            polys.append(pts)
            seen = True
        on_screen += seen
        if seen:
            own.append((d, polys))
    inside = np.asarray(mask_img) > 0
    grown = inside.copy()
    for _ in range(3):
        g = grown.copy()
        g[1:] |= grown[:-1]
        g[:-1] |= grown[1:]
        g[:, 1:] |= grown[:, :-1]
        g[:, :-1] |= grown[:, 1:]
        grown = g
    diff = np.abs(on - off).max(axis=2)
    outside = ~grown
    n_out, n_in = int(outside.sum()), int(inside.sum())
    print(f"boxes on screen {on_screen} of {len(want['drawn'])}; {n_in} pixels inside them, {n_out} outside")
    if n_out < 1000:
        n_ok = False
        print(f"N {word('N', False)}  only {n_out} pixels outside the boxes: nothing to judge")
    else:
        still = float((diff[outside] <= 3).mean())
        n_ok = still >= 0.995
        print(f"N {word('N', n_ok)}  {100.0 * still:.3f}% of {n_out} pixels outside the boxes equal the decal-less "
              f"shot (>= 99.5%)")
    ok &= n_ok or 'N' not in stages
    # ---- C: each decal the camera SEES changes pixels inside its own box (boxes reach through walls, so a
    # share of all pixels inside all boxes says nothing: it is 1.5% on a correct picture)
    moved_all = int((diff > 3).sum())
    seen_n, shown_n, quiet = 0, 0, []
    for d, polys in own:
        to = d['centre'] - cam
        far = float(np.linalg.norm(to))
        cv = to @ view.T
        if cv[2] < 8.0 or far > SEEFAR or float(d['axes'][2] @ to) / far < 0.2:
            continue
        px, py = W / 2.0 + focal * cv[0] / cv[2], H / 2.0 - focal * cv[1] / cv[2]
        if not (0 <= px < W and 0 <= py < H):
            continue
        hit = soup.ray(cam, to / far)
        if 0.0 <= hit < far - 4.0:
            continue
        seen_n += 1
        one = Image.new('L', (W, H), 0)
        dr1 = ImageDraw.Draw(one)
        for pts in polys:
            dr1.polygon(pts, fill=255, outline=255)
        n = int((diff[np.asarray(one) > 0] > 3).sum())
        if n >= CPIX:
            shown_n += 1
        else:
            quiet.append('%08x %s (%d changed pixels)' % (d['form'], d['base'], n))
    c_ok = seen_n >= CSEEN and shown_n >= CSHARE * seen_n
    print(f"C {word('C', c_ok)}  {shown_n} of {seen_n} decals the camera sees (centre on screen, within {SEEFAR:.0f} "
          f"units, nothing opaque in front) change {CPIX} or more pixels inside their own box (>= {100.0 * CSHARE:.0f}% "
          f"of at least {CSEEN}); {moved_all} pixels changed in the whole frame")
    for line in quiet[:8]:
        print('     quiet: ' + line)
    ok &= c_ok or 'C' not in stages
    print('decal PASS' if ok else 'decal FAIL')
    return 0 if ok else 1


CSHARE = 0.8    # of the decals the camera sees
CPIX = 30       # changed pixels inside one decal's own box
CSEEN = 3       # fewer seen decals than this: nothing to judge, a FAIL
SEEFAR = 900.0  # the checker's ray reaches RAY + SLACK

if __name__ == '__main__':
    sys.exit(main())
