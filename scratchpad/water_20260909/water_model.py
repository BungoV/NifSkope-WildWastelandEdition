#!/usr/bin/env python3
"""water_model.py -- the water-body model lane WATER1 measures with.

Definitions, fixed here so the census and the control run the SAME code:

  wet texel      a texel of the level-0 height grid whose terrain height is
                 below the water plane of the cell it lies in, in a cell whose
                 CELL DATA carries Has Water.  (docs/LODGEN_BTD_FORMAT.md: the
                 file stores a RESOLVED per-cell water height; vanilla draws a
                 LOD quad only where the water is exposed above the cell's
                 terrain minimum, and this is the per-texel form of that rule.)

  surface        a 4-connected set of wet texels whose cell water HEIGHT is
                 equal (quantised to 1/8 unit).  A pond is one surface; a
                 stepped river is several.

  body           a 4-connected set of wet texels whose resolved water TYPE is
                 equal (the cell's XCWT, or the worldspace default when the
                 cell inherits).  Heights may step inside a body.

  class          sea    the body reaches the worldspace edge, or its texels are
                        majority at the worldspace default water height AND it
                        reaches the edge of the generated extent;
                 river  more than one surface (it steps), or one surface but
                        long and thin (elongation gate, stated in the census);
                 lake   one surface, does not reach the edge, does not step.

Everything here is pure numpy plus ccl.py.  Nothing reads our own writer's
output to judge our own writer's output: the inputs are the .lodl (measured
against the authority decoder) and Fallout4.esm.
"""

import numpy as np

import ccl

TEXEL_UNITS = 128.0          # 4096 world units per cell / 32 samples per edge
CELL_UNITS = 4096.0


def expand_cells(a, spc):
    """(cellsY, cellsX) -> (cellsY*spc, cellsX*spc), each cell a solid block."""
    return np.repeat(np.repeat(a, spc, axis=0), spc, axis=1)


def wet_mask(heights, cell_waterH, cell_hasWater, spc):
    wh = expand_cells(cell_waterH, spc)
    hw = expand_cells(cell_hasWater, spc)
    return (hw & (heights < wh)), wh


def segment(heights, cell_waterH, cell_hasWater, cell_type, spc):
    """Returns (wet, waterH_texel, surf_lab, n_surf, body_lab, n_body)."""
    wet, wh = wet_mask(heights, cell_waterH, cell_hasWater, spc)
    hkey = np.round(wh * 8.0).astype(np.int32)
    tkey = expand_cells(cell_type.astype(np.int32), spc)
    surf_lab, n_surf = ccl.label(wet, hkey)
    body_lab, n_body = ccl.label(wet, tkey)
    return wet, wh, surf_lab, n_surf, body_lab, n_body


def body_stats(wet, wh, body_lab, n_body, surf_lab, tkey_texel, default_h):
    """Per-body: area in texels, bbox, distinct surface heights, edge contact."""
    H, W = wet.shape
    flat = body_lab.ravel()
    area = np.bincount(flat, minlength=n_body + 1)
    ys, xs = np.nonzero(wet)
    lb = body_lab[ys, xs]
    out = {}
    order = np.argsort(lb, kind='stable')
    lb_s = lb[order]
    ys_s, xs_s = ys[order], xs[order]
    bounds = np.searchsorted(lb_s, np.arange(1, n_body + 1), 'left')
    bounds2 = np.searchsorted(lb_s, np.arange(1, n_body + 1), 'right')
    whf = wh
    for b in range(1, n_body + 1):
        i0, i1 = bounds[b - 1], bounds2[b - 1]
        if i1 <= i0:
            continue
        by, bx = ys_s[i0:i1], xs_s[i0:i1]
        hs = whf[by, bx]
        uh, cnt = np.unique(np.round(hs * 8.0).astype(np.int64), return_counts=True)
        surfs = np.unique(surf_lab[by, bx])
        out[b] = {
            'area': int(area[b]),
            'x0': int(bx.min()), 'x1': int(bx.max()),
            'y0': int(by.min()), 'y1': int(by.max()),
            'heights': [(h / 8.0, int(c)) for h, c in zip(uh, cnt)],
            'nsurf': int(surfs.size),
            'type': int(tkey_texel[by[0], bx[0]]),
            'edge': bool(bx.min() == 0 or by.min() == 0 or bx.max() == W - 1 or by.max() == H - 1),
        }
    return out


def elongation(rec):
    w = rec['x1'] - rec['x0'] + 1
    h = rec['y1'] - rec['y0'] + 1
    long_side = max(w, h)
    return long_side * long_side / float(max(rec['area'], 1))


def classify(rec, default_h, elong_gate=6.0):
    """sea / river / lake / puddle, from the rules stated at the top."""
    if rec['edge']:
        return 'sea'
    if rec['nsurf'] > 1:
        return 'river'
    if elongation(rec) >= elong_gate and rec['area'] >= 16:
        return 'river'
    return 'lake'


# ---------------------------------------------------------------- geometry

def chamfer_distance(mask):
    """Distance (in texels, 3-4 chamfer / 3) from every True texel to the
    nearest False texel.  Two sequential passes; no scipy on this machine."""
    H, W = mask.shape
    BIG = np.int32(1 << 24)
    d = np.where(mask, BIG, 0).astype(np.int32)
    for y in range(H):
        row = d[y]
        if y > 0:
            up = d[y - 1]
            np.minimum(row, up + 3, out=row)
            np.minimum(row[1:], up[:-1] + 4, out=row[1:])
            np.minimum(row[:-1], up[1:] + 4, out=row[:-1])
        for x in range(1, W):
            if row[x] > row[x - 1] + 3:
                row[x] = row[x - 1] + 3
    for y in range(H - 1, -1, -1):
        row = d[y]
        if y < H - 1:
            dn = d[y + 1]
            np.minimum(row, dn + 3, out=row)
            np.minimum(row[1:], dn[:-1] + 4, out=row[1:])
            np.minimum(row[:-1], dn[1:] + 4, out=row[:-1])
        for x in range(W - 2, -1, -1):
            if row[x] > row[x + 1] + 3:
                row[x] = row[x + 1] + 3
    return d.astype(np.float32) / 3.0


def neighbour_bodies(body_lab, n_body):
    """{label: set(neighbouring labels)} across 4-adjacency, background excluded."""
    out = {}

    def note(a, b):
        m = (a != b) & (a != 0) & (b != 0)
        for u, v in zip(a[m].tolist(), b[m].tolist()):
            out.setdefault(u, set()).add(v)
            out.setdefault(v, set()).add(u)
    note(body_lab[:, :-1], body_lab[:, 1:])
    note(body_lab[:-1, :], body_lab[1:, :])
    return out


def principal_axis(ys, xs):
    """(unit vector along the longest extent, anisotropy 0..1)."""
    if ys.size < 3:
        return np.array([1.0, 0.0]), 0.0
    p = np.stack([xs.astype(np.float64), ys.astype(np.float64)])
    p -= p.mean(axis=1, keepdims=True)
    c = np.cov(p)
    w, v = np.linalg.eigh(c)
    order = np.argsort(w)[::-1]
    w = w[order]
    v = v[:, order]
    aniso = 0.0 if w[0] <= 0 else float(1.0 - w[1] / w[0])
    return v[:, 0], aniso


def height_monotonicity(ys, xs, hs):
    """Pearson r between the water-surface height and the position along the
    body's principal axis.  |r| near 1 means the steps march one way, which is
    what makes a gradient rule confident."""
    ax, aniso = principal_axis(ys, xs)
    t = xs * ax[0] + ys * ax[1]
    if hs.std() == 0 or t.std() == 0:
        return 0.0, aniso
    return float(np.corrcoef(t, hs)[0, 1]), aniso
