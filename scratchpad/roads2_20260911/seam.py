"""Work item 1: THE SEAM, MEASURED -- before a line of rasteriser code changes.

On chunk (-20,20) (cells -20..-17 x 20..23, 512 texels, 32 world units a
texel -- vanilla's own grid, no resampling on either side):

  * project every placed Landscape\\Roads / Landscape\\Sidewalks piece top-down
    with a maximum-z buffer, keeping per texel WHICH PIECE won and the
    interpolated VERTEX ALPHA of the winning triangle;
  * piece boundaries = road texels whose 4-neighbour is a road texel belonging
    to a DIFFERENT piece;
  * FEATHERED boundaries = a boundary where at least one of the two winners is
    a ramped skirt shape (vertex alpha reaching below 0.9); SOLID boundaries =
    both winners unramped.  The solid set is the control: nothing in this lane
    changes how two solid asphalt pieces meet, so ours and vanilla must sit in
    the same relation before and after;
  * the metric is the mean luminance GRADIENT MAGNITUDE at those texels
    (central differences), in vanilla's shipped sheet and in ours;
  * the FLOOR is the same boundary set displaced five ways -- area, shape and
    spatial spectrum preserved, registration with the road destroyed
    (ww-control-calibration).

Second measurement, the compositing rule: bin the skirt-covered texels by the
mesh's own interpolated vertex alpha and read vanilla's sheet, our --roads bake
and our --no-roads bake in each bin.  If vanilla feathers by vertex alpha its
luminance walks from the ground's to the asphalt's across the bins; if it cuts
out, it jumps.

usage: seam.py <ourColour.DDS> <ourNoRoadColour.DDS> [tag]
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds, vertex_colors, alpha_property, \
    shape_vertex_data_offset, read_material_full, Nif       # noqa: E402
import matinfo                                              # noqa: E402
from rasterlib import MeshCache, Grid, rasterise, local_to_model  # noqa: E402
import placements                                           # noqa: E402

DATA = r'E:\Tools\Fallout 4\DataUnpacked\Data'
VAN = os.path.join(DATA, 'Textures', 'Terrain', 'Commonwealth',
                   'Commonwealth.4.-20.20.DDS')
REFS = os.path.join(HERE, '..', 'roads1_20260911', 'sanctuary_refs.json')
SEP = chr(92)
CELL = 4096.0
N = 512
CX0, CY0 = -20, 20          # the chunk's SW cell
DISPLACE = [(37, 0), (0, 37), (-29, 23), (23, -29), (53, 53)]


def is_roadish(sig, modl):
    c = placements.norm_components(modl)
    return (sig == 'STAT' and len(c) >= 3 and c[0] == 'landscape'
            and c[1] in ('roads', 'sidewalks'))


def shape_records(nif, sh):
    """(ramped, decal, alphaTest, vertexAlpha array or None) for one shape."""
    info = matinfo.shader_info(nif, sh)
    matname = info[0] if info else ''
    f1 = info[1] if info else 0
    mpath = matinfo.find_material(DATA, matname)
    mat = read_material_full(mpath) if mpath else None
    _, apref = shape_vertex_data_offset(nif, sh)
    ap = alpha_property(nif, apref)
    col = vertex_colors(nif, sh)
    a = col[:, 3] if col is not None else None
    ramped = a is not None and float(a.min()) < 0.9
    decal = bool((mat and mat['decal']) or (f1 & (1 << 26)))
    atest = bool((mat and mat['alphaTest']) or (ap and (ap[0] & 0x0200)))
    return ramped, decal, atest, a


def project():
    """Per-texel: piece id, vertex alpha, ramped flag of the winning shape."""
    wx0, wy1 = CX0 * CELL, (CY0 + 4) * CELL
    grid = Grid(wx0, wy1 - 4 * CELL, wx0 + 4 * CELL, wy1, N)
    pl = [p for p in placements.load(REFS) if is_roadish(p['sig'], p['modl'])]
    mc = MeshCache(DATA)
    nifcache = {}

    zbuf = np.full((N, N), -1e30)
    idbuf = np.full((N, N), -1, dtype=np.int32)
    abuf = [np.zeros((N, N)), np.zeros((N, N))]      # vertex alpha, ramped 0/1
    pieces = []
    for pi, p in enumerate(pl):
        full = mc.path_for(p['modl'])
        if full is None:
            continue
        key = full.lower()
        if key not in nifcache:
            try:
                nif = Nif(full)
            except Exception:
                nifcache[key] = None
            else:
                recs = {}
                for idx, sh in nif.shapes.items():
                    if not sh['verts'] or not sh['tris']:
                        continue
                    recs[idx] = shape_records(nif, sh)
                nifcache[key] = (nif, recs)
        got = nifcache[key]
        if got is None:
            continue
        nif, recs = got
        pid = len(pieces)
        pieces.append(dict(model=p['modl'], edid=p.get('edid', '')))
        for idx, sh in nif.shapes.items():
            if idx not in recs:
                continue
            ramped, decal, atest, a = recs[idx]
            R, t, s = local_to_model(nif, sh)
            v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t
            v = (v * p['scale']).dot(p['R'].T) + p['pos']
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            txy = grid.to_texel(v)
            pts = txy[tri]                            # (T,3,2)
            zz = v[tri][:, :, 2]                      # (T,3)
            if a is None:
                av = np.ones((tri.shape[0], 3))
            else:
                av = a[tri]
            attrs = np.stack([av, np.full_like(av, 1.0 if ramped else 0.0)],
                             axis=2)
            rasterise(grid, pts, zz, zbuf, idbuf, pid, attrs=attrs, abuf=abuf)
    return idbuf, abuf[0], abuf[1], pieces


def grad_mag(L):
    gy, gx = np.gradient(L)
    return np.hypot(gx, gy)


def boundaries(idbuf, rampedbuf):
    road = idbuf >= 0
    diff = np.zeros_like(road)
    nb_ramped = np.zeros_like(road)
    for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        sh_id = np.roll(np.roll(idbuf, dj, axis=0), di, axis=1)
        sh_rp = np.roll(np.roll(rampedbuf, dj, axis=0), di, axis=1)
        both = road & (sh_id >= 0)
        d = both & (sh_id != idbuf)
        diff |= d
        nb_ramped |= d & (sh_rp > 0.5)
    feath = diff & ((rampedbuf > 0.5) | nb_ramped)
    solid = diff & ~feath
    return diff, feath, solid


def report(mask, name, sheets):
    row = {'set': name, 'texels': int(mask.sum())}
    for k, L in sheets.items():
        row[k] = float(grad_mag(L)[mask].mean()) if mask.sum() else float('nan')
    return row


def main():
    ours = Dds(sys.argv[1]).lum()
    noroad = Dds(sys.argv[2]).lum()
    tag = sys.argv[3] if len(sys.argv) > 3 else 'ours'
    van = Dds(VAN).lum()
    print('vanilla %s  ours %s  no-roads %s' % (van.shape, ours.shape, noroad.shape))

    cache = os.path.join(HERE, 'proj_m20_20.npz')
    if os.path.isfile(cache):
        z = np.load(cache, allow_pickle=True)
        idbuf, alph, ramp = z['idbuf'], z['alpha'], z['ramped']
        print('projection loaded from %s' % cache)
    else:
        idbuf, alph, ramp, pieces = project()
        np.savez_compressed(cache, idbuf=idbuf, alpha=alph, ramped=ramp)
        json.dump(pieces, open(os.path.join(HERE, 'proj_pieces.json'), 'w'))
        print('projected %d pieces, %d road texels'
              % (len(pieces), int((idbuf >= 0).sum())))

    sheets = {'vanilla': van, tag: ours, 'no-roads': noroad}
    diff, feath, solid = boundaries(idbuf, ramp)
    rows = [report(diff, 'all piece boundaries', sheets),
            report(feath, 'FEATHERED boundaries', sheets),
            report(solid, 'SOLID boundaries (control)', sheets)]
    road = idbuf >= 0
    rows.append(report(road, 'all road texels', sheets))
    rows.append(report(~road, 'off-road ground', sheets))
    for k, (dj, di) in enumerate(DISPLACE):
        m = np.roll(np.roll(feath, dj, axis=0), di, axis=1)
        rows.append(report(m, 'floor: feathered displaced %+d%+d' % (dj, di), sheets))

    print('')
    print('%-44s %8s %9s %9s %9s' % ('set', 'texels', 'vanilla', tag, 'no-roads'))
    for r in rows:
        print('%-44s %8d %9.3f %9.3f %9.3f'
              % (r['set'], r['texels'], r['vanilla'], r[tag], r['no-roads']))

    # ---- the compositing rule: does vanilla follow the vertex-alpha ramp? ---
    print('')
    print('SKIRT-COVERED TEXELS BINNED BY THE MESH VERTEX ALPHA')
    skirt = road & (ramp > 0.5)
    print('skirt-covered road texels: %d' % int(skirt.sum()))
    edges = [0.0, 0.1, 0.25, 0.4, 0.55, 0.7, 0.85, 1.01]
    print('%-14s %8s %9s %9s %9s %9s' % ('alpha bin', 'texels', 'vanilla',
                                         tag, 'no-roads', 'van norm'))
    gl = float(van[~road].mean())
    core = road & (ramp < 0.5)
    rl = float(van[core].mean()) if core.sum() else float('nan')
    bins = []
    for k in range(len(edges) - 1):
        m = skirt & (alph >= edges[k]) & (alph < edges[k + 1])
        if m.sum() < 20:
            continue
        vv, oo, nn = float(van[m].mean()), float(ours[m].mean()), float(noroad[m].mean())
        nrm = (vv - gl) / (rl - gl) if rl == rl else float('nan')
        bins.append((edges[k], edges[k + 1], int(m.sum()), vv, oo, nn, nrm))
        print('%-14s %8d %9.2f %9.2f %9.2f %9.3f'
              % ('%.2f-%.2f' % (edges[k], edges[k + 1]), int(m.sum()), vv, oo, nn, nrm))
    print('reference: vanilla off-road ground luminance %.2f, '
          'vanilla unramped road core %.2f' % (gl, rl))
    if len(bins) >= 3:
        a = np.array([0.5 * (b[0] + b[1]) for b in bins])
        print('correlation of vanilla luminance with the mesh alpha: %.3f'
              % float(np.corrcoef(a, [b[3] for b in bins])[0, 1]))
        print('correlation of OURS    luminance with the mesh alpha: %.3f'
              % float(np.corrcoef(a, [b[4] for b in bins])[0, 1]))
        print('correlation of no-roads luminance with the mesh alpha: %.3f '
              '(the floor: the ground knows nothing of the mesh)'
              % float(np.corrcoef(a, [b[5] for b in bins])[0, 1]))

    out = os.path.join(HERE, 'seam_%s.json' % tag)
    json.dump(dict(rows=rows, bins=bins, ground=gl, roadcore=rl),
              open(out, 'w'), indent=1)
    print('wrote %s' % out)


if __name__ == '__main__':
    main()
