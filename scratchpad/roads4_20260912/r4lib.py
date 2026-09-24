"""ROADS4 -- the road mesh's SKIRT, projected onto the bake grid.

Nothing here imports the generator.  The NIF reader, the material reader and
the max-z scan conversion are lane ROADS1's and ROADS2's, re-used unchanged so
that this lane's numbers sit on instruments that were already gated
(`rasterlib`, `roads2lib`, `matinfo`, `placements`).

WHAT A SKIRT IS, stated as a rule before any number is read:

    a road TRIANGLE is SKIRT when any of its three vertices carries an alpha
    below 1, and TRUNK when all three are 1.

That is the finest grain the mesh offers.  ROADS2 classified whole SHAPES
(`ramped` = the shape's minimum vertex alpha < 0.9), which is coarser: a shape
that is mostly asphalt with a feathered lip is wholly "ramped" under that rule
and the lip is not separable from the asphalt.  The two readings are both
reported, and the difference between them is the point of this lane.

THE FAMILY FILTER matches what the generator actually paints at its shipped
defaults (`lodgen.cpp:7172`, `lodgenIsRaisedRoadModel` at 6719):

  * STAT, model components `landscape` / `roads` / ... (component equality, a
    leading `meshes` dropped, never a substring -- MISTAKES.md's "sTREEt");
  * the RAISED families out: third component `highwayoverpass` or `bridge`;
  * `landscape/sidewalks` out (it is a different second component, so the road
    test already excludes it).

The `hasLod` arm of the generator's raised test is NOT reproduced here -- this
has no MNAM data -- and the folder arm is used alone.  `docs/LODGEN_TERRAIN_VT.md`
1a.3 records the two arms disagreeing on ZERO of the 151 road bases / 759
placements audited on these tiles, and the projected mask is checked against
the generator's own painted mask in every run, so a disagreement would show up
as unclassified texels rather than as a silent wrong answer.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(SCRATCH, 'roads1_20260911'))
sys.path.insert(0, os.path.join(SCRATCH, 'roads2_20260911'))
sys.path.insert(0, os.path.join(SCRATCH, 'splat1_20260911'))

from rasterlib import MeshCache, Grid, rasterise, local_to_model   # noqa: E402
from roads2lib import vertex_colors, alpha_property, \
    shape_vertex_data_offset, read_material_full, Nif              # noqa: E402
import matinfo                                                     # noqa: E402
import placements                                                  # noqa: E402

DATA = r'E:\Tools\Fallout 4\DataUnpacked\Data'
VANDIR = os.path.join(DATA, 'Textures', 'Terrain', 'Commonwealth')
R3OUT = os.path.join(SCRATCH, 'roads3_20260911', 'out')
CELL = 4096.0
N = 512
SEP = chr(92)

TILES = {
    't2020': dict(cx=-20, cy=20,
                  refs=os.path.join(SCRATCH, 'roads2_20260911',
                                    'sanc_wide_refs.json')),
    't0808': dict(cx=-8, cy=8,
                  refs=os.path.join(HERE, 'dt_wide_refs.json')),
}


# --------------------------------------------------------------- the sheets

def sheet(variant, tile, suffix=''):
    t = TILES[tile]
    p = os.path.join(R3OUT, variant, tile, 'tex',
                     'Commonwealth.4.%d.%d%s.DDS' % (t['cx'], t['cy'], suffix))
    import splatlib as S
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def vanilla_sheet(tile, suffix=''):
    t = TILES[tile]
    import splatlib as S
    p = os.path.join(VANDIR, 'Commonwealth.4.%d.%d%s.DDS'
                     % (t['cx'], t['cy'], suffix))
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def lum(rgb):
    return 0.299 * rgb[:, :, 0] + 0.587 * rgb[:, :, 1] + 0.114 * rgb[:, :, 2]


def painted_mask(a_roads, a_noroads, thr=1.0):
    """The texels the generator's road pass actually changed."""
    return np.abs(a_roads - a_noroads).max(2) >= thr


# ------------------------------------------------------------- the families

def is_flat_road(sig, modl):
    c = placements.norm_components(modl)
    if not (sig == 'STAT' and len(c) >= 3 and c[0] == 'landscape'
            and c[1] == 'roads'):
        return False
    return c[2] not in ('highwayoverpass', 'bridge')


def is_raised_road(sig, modl):
    c = placements.norm_components(modl)
    return (sig == 'STAT' and len(c) >= 3 and c[0] == 'landscape'
            and c[1] == 'roads' and c[2] in ('highwayoverpass', 'bridge'))


# -------------------------------------------------------------- the meshes

def shape_record(nif, sh):
    """(material path or '', alphaTest, alphaBlend, vertex alpha array|None)."""
    info = matinfo.shader_info(nif, sh)
    matname = info[0] if info else ''
    f1 = info[1] if info else 0
    mpath = matinfo.find_material(DATA, matname)
    mat = read_material_full(mpath) if mpath else None
    _, apref = shape_vertex_data_offset(nif, sh)
    ap = alpha_property(nif, apref)
    col = vertex_colors(nif, sh)
    a = col[:, 3] if col is not None else None
    atest = bool((mat and mat['alphaTest']) or (ap and (ap[0] & 0x0200)))
    ablend = bool((mat and mat.get('alphaBlend')) or (ap and (ap[0] & 0x0001)))
    return (matname or '', atest, ablend, a, bool(f1 & (1 << 26)))


def project(tile, cache=True):
    """Per-texel max-z projection of the flat-road family.

    Returns a dict of (N,N) arrays:
      shp   winning shape index, -1 where nothing covers
      cls   1 where the WINNING triangle is skirt, 0 trunk, -1 nothing
      alp   the winning triangle's interpolated vertex alpha (1 where opaque)
      trunk 1 where ANY trunk triangle covers the texel at all (max-z ignored)
      skirt 1 where ANY skirt triangle covers the texel at all
    plus `shapes`, the per-shape table, and `tris`, the per-triangle census.
    """
    cpath = os.path.join(HERE, 'proj_%s.npz' % tile)
    jpath = os.path.join(HERE, 'proj_%s.json' % tile)
    if cache and os.path.isfile(cpath) and os.path.isfile(jpath):
        z = np.load(cpath, allow_pickle=True)
        meta = json.load(open(jpath))
        out = dict((k, z[k]) for k in z.files)
        out['shapes'] = meta['shapes']
        out['tris'] = meta['tris']
        return out

    t = TILES[tile]
    wx0, wy1 = t['cx'] * CELL, (t['cy'] + 4) * CELL
    grid = Grid(wx0, wy1 - 4 * CELL, wx0 + 4 * CELL, wy1, N)
    pl = [p for p in placements.load(t['refs'])
          if is_flat_road(p['sig'], p['modl'])]

    mc = MeshCache(DATA)
    nifcache = {}
    zbuf = np.full((N, N), -1e30)
    shp = np.full((N, N), -1, dtype=np.int32)
    abuf = [np.ones((N, N)), np.zeros((N, N))]        # alpha, isSkirt
    # "any triangle of this class covers this texel", max-z independent: one
    # shared buffer pair per class for the whole run, because the FIRST
    # triangle to reach a texel already sets the id and no later triangle can
    # unset it.
    zbT = np.full((N, N), -1e30); ibT = np.full((N, N), -1, dtype=np.int32)
    zbS = np.full((N, N), -1e30); ibS = np.full((N, N), -1, dtype=np.int32)
    shapes = []
    tri_trunk = tri_skirt = 0
    per_mat = {}

    for p in pl:
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
                    recs[idx] = shape_record(nif, sh)
                nifcache[key] = (nif, recs)
        got = nifcache[key]
        if got is None:
            continue
        nif, recs = got
        for idx, sh in nif.shapes.items():
            if idx not in recs:
                continue
            matname, atest, ablend, a, decal = recs[idx]
            R, t3, s = local_to_model(nif, sh)
            v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t3
            v = (v * p['scale']).dot(p['R'].T) + p['pos']
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            if a is None:
                av = np.ones((tri.shape[0], 3))
            else:
                av = a[tri]
            isSkirt = (av.min(1) < 1.0)
            tri_trunk += int((~isSkirt).sum())
            tri_skirt += int(isSkirt.sum())
            mk = matname.lower()
            m = per_mat.setdefault(mk, dict(trunk=0, skirt=0, shapes=0,
                                            alphaTest=atest, alphaBlend=ablend))
            m['trunk'] += int((~isSkirt).sum())
            m['skirt'] += int(isSkirt.sum())
            m['shapes'] += 1
            sid = len(shapes)
            shapes.append(dict(model=p['modl'], mat=matname, alphaTest=atest,
                               alphaBlend=ablend, decal=decal,
                               tris=int(tri.shape[0]),
                               skirtTris=int(isSkirt.sum()),
                               minAlpha=float(a.min()) if a is not None else 1.0))
            txy = grid.to_texel(v)
            pts = txy[tri]
            zz = v[tri][:, :, 2]
            attrs = np.stack([av, np.repeat(isSkirt.astype(float)[:, None], 3,
                                            axis=1)], axis=2)
            rasterise(grid, pts, zz, zbuf, shp, sid, attrs=attrs, abuf=abuf)
            # coverage-only passes: which texels are touched at all, by class
            if (~isSkirt).any():
                rasterise(grid, pts[~isSkirt], zz[~isSkirt], zbT, ibT, 1)
            if isSkirt.any():
                rasterise(grid, pts[isSkirt], zz[isSkirt], zbS, ibS, 1)

    anyTrunk = ibT >= 0
    anySkirt = ibS >= 0
    cls = np.where(shp >= 0, (abuf[1] > 0.5).astype(np.int8), -1).astype(np.int8)
    out = dict(shp=shp, cls=cls, alp=abuf[0],
               trunk=anyTrunk.astype(np.int8), skirt=anySkirt.astype(np.int8))
    np.savez_compressed(cpath, **out)
    meta = dict(shapes=shapes,
                tris=dict(trunk=tri_trunk, skirt=tri_skirt, byMaterial=per_mat))
    json.dump(meta, open(jpath, 'w'))
    out['shapes'] = shapes
    out['tris'] = meta['tris']
    return out
