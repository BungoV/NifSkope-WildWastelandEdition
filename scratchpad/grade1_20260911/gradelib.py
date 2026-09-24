"""Lane GRADE1 -- the tone instruments.

Everything here reads FILES: vanilla's shipped dim-4 chunk sheets and this
lane's own region bakes.  Nothing is imported from the generator; the only
things shared with earlier lanes are SPLAT1's DDS decoder (`splatlib.Dds`, a
file reader, not a model of the bake) and the independent plugin walk in
`tests/spells/lodgen_cover_model.py` (which parses Fallout4.esm itself).

The fields a residual is correlated against:

  height   VHGT, parsed here (cell offset + 33x33 signed byte deltas x 8)
  slope    from the chunk's `_msn` UP channel -- at the shipped default that
           sheet IS vanilla's own bytes, so the slope is vanilla's, not ours
  AO       the R channel of our `_data.DDS` (horizon-from-height visibility)
  VCLR     the 33x33x3 landscape vertex colour, bilinear, from the plugin

Controls: every correlation is reported beside a PHASE TWIN of the same field
(same amplitude spectrum, phase randomised), which is a floor for a structure
statistic and never for a periodicity (TILING2's ruling).
"""

import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))

from splatlib import Dds, lum, local_var, phase_twin      # noqa: E402,F401
from lodgen_cover_model import (Esm, GRUP, REC_HDR,       # noqa: E402
                               read_fields, record_data)

VAN = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'
ESM = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm'
CELL = 4096.0

TILES = {(-20, 24): 't2024', (-20, 20): 't2020'}


# ------------------------------------------------------------------ sheet I/O

def van(cx, cy, suffix=''):
    return os.path.join(VAN, 'Commonwealth.4.%d.%d%s.DDS' % (cx, cy, suffix))


def ours(variant, cx, cy, suffix=''):
    tag = TILES.get((cx, cy), 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3))
    return os.path.join(HERE, 'out', variant, tag, 'tex',
                        'Commonwealth.4.%d.%d%s.DDS' % (cx, cy, suffix))


def rgb(path, mip=0):
    """mip 0 as float32 HxWx3 in 0..255."""
    return Dds(path).level(mip)[:, :, :3].astype(np.float32)


def alpha(path, mip=0):
    return Dds(path).level(mip)[:, :, 3].astype(np.float32)


def cover_plane(path):
    """The cover byte plane: alpha of `_data.DDS`, and ONLY when the sheet is
    DXT5 and its dwReserved1 carries 'WWCV' (src/lodgen.cpp:7070-7095).  A DXT1
    `_data` means no cover plane was written at all, so the plane is zeros and
    the second return value says so -- a refusal in words, not a silent zero."""
    with open(path, 'rb') as f:
        head = f.read(128)
    fourcc = head[84:88]
    stamp = head[112:116]
    d = Dds(path)
    if fourcc != b'DXT5' or stamp != b'WWCV':
        return np.zeros((d.height, d.width), np.float32), False
    return alpha(path), True


# ------------------------------------------------------------- the plugin side

def read_lands(esm_path):
    """(paint, heights): paint is lodgen_cover_model's own walk (VCLR lives
    there); heights is {(cx,cy): 33x33 float world units} from VHGT, parsed
    here because that walk deliberately skips it."""
    e = Esm(esm_path)
    e.walk(0x0000003C)
    heights = _walk_vhgt(e.buf, 0x0000003C)
    return e, heights


def _walk_vhgt(buf, world_target):
    """VHGT: float32 offset, then 33x33 signed byte deltas, row-cumulative from
    the offset, column-cumulative down the first column, x 8 world units."""
    out = {}
    n = len(buf)

    def walk(start, end, world, cell):
        i = start
        while i + REC_HDR <= end:
            t = buf[i:i + 4]
            size = struct.unpack_from('<I', buf, i + 4)[0]
            if t == GRUP:
                label = struct.unpack_from('<I', buf, i + 8)[0]
                gtype = struct.unpack_from('<I', buf, i + 12)[0]
                cell = walk(i + REC_HDR, i + size,
                            label if gtype == 1 else world, cell)
                i += size
                continue
            flags = struct.unpack_from('<I', buf, i + 8)[0]
            body = record_data(buf, i + REC_HDR, size, flags)
            if t == b'CELL':
                cell = None
                for ft, fd in read_fields(body):
                    if ft == b'XCLC' and len(fd) >= 8:
                        cell = struct.unpack_from('<ii', fd, 0)
            elif t == b'LAND' and cell is not None and world == world_target:
                for ft, fd in read_fields(body):
                    if ft == b'VHGT' and len(fd) >= 4 + 33 * 33:
                        off = struct.unpack_from('<f', fd, 0)[0]
                        d = np.frombuffer(fd, np.int8, 33 * 33, 4)
                        d = d.reshape(33, 33).astype(np.float64)
                        h = np.empty((33, 33), np.float64)
                        col = off + np.cumsum(d[:, 0])
                        h[:, 0] = col
                        h[:, 1:] = col[:, None] + np.cumsum(d[:, 1:], axis=1)
                        out[cell] = (h * 8.0).astype(np.float32)
            i += REC_HDR + size
        return cell

    size = struct.unpack_from('<I', buf, 4)[0]
    walk(REC_HDR + size, n, 0, None)
    return out


def bilinear_grid(grid, fx, fy):
    """grid[33,33] or [33,33,c] sampled at fractional cell coords 0..1."""
    g = grid[:, :, None] if grid.ndim == 2 else grid
    n = g.shape[0] - 1
    gx = np.clip(fx * n, 0, n - 1e-4)
    gy = np.clip(fy * n, 0, n - 1e-4)
    ix, iy = gx.astype(np.int32), gy.astype(np.int32)
    tx, ty = (gx - ix)[..., None], (gy - iy)[..., None]
    v = ((g[iy, ix] * (1 - tx) + g[iy, ix + 1] * tx) * (1 - ty)
         + (g[iy + 1, ix] * (1 - tx) + g[iy + 1, ix + 1] * tx) * ty)
    return v[..., 0] if grid.ndim == 2 else v


# -------------------------------------------------------------------- geometry

def texel_world(cx, cy, res=512, dim=4):
    """World (wx, wy) at each texel centre of the dim-4 chunk sheet whose
    south-west cell is (cx, cy).  Row 0 of the sheet is NORTH."""
    span = dim * CELL
    i = np.arange(res, dtype=np.float64)
    u = (i + 0.5) / res
    wx = cx * CELL + u * span
    wy = (cy + dim) * CELL - u * span
    return np.meshgrid(wx, wy)


def vclr_field(cx, cy, esm, res=512, dim=4):
    """Per-texel VCLR (res,res,3) in 0..255 and a 'this cell carries one' mask.
    Cells with no VCLR record read 255 -- which is what the bake does (it
    simply skips the multiply), so the field is the MULTIPLIER, not the record."""
    WX, WY = texel_world(cx, cy, res, dim)
    ccx = np.floor(WX / CELL).astype(np.int32)
    ccy = np.floor(WY / CELL).astype(np.int32)
    fx, fy = WX / CELL - ccx, WY / CELL - ccy
    out = np.full((res, res, 3), 255.0, np.float32)
    have = np.zeros((res, res), bool)
    for gy in range(dim):
        for gx in range(dim):
            key = (cx + gx, cy + gy)
            rec = esm.lands.get(key)
            if rec is None or not rec.get('vclr'):
                continue
            grid = np.array(rec['vclr'], np.float32).reshape(33, 33, 3)
            m = (ccx == key[0]) & (ccy == key[1])
            if not m.any():
                continue
            out[m] = bilinear_grid(grid, fx[m], fy[m])
            have[m] = True
    return out, have


def height_field(cx, cy, heights, res=512, dim=4):
    """Per-texel height in world units, and a 'have' mask."""
    WX, WY = texel_world(cx, cy, res, dim)
    ccx = np.floor(WX / CELL).astype(np.int32)
    ccy = np.floor(WY / CELL).astype(np.int32)
    fx, fy = WX / CELL - ccx, WY / CELL - ccy
    out = np.zeros((res, res), np.float32)
    have = np.zeros((res, res), bool)
    for gy in range(dim):
        for gx in range(dim):
            key = (cx + gx, cy + gy)
            grid = heights.get(key)
            if grid is None:
                continue
            m = (ccx == key[0]) & (ccy == key[1])
            if not m.any():
                continue
            # VHGT row 0 is the SOUTH edge of the cell; the sheet's row 0 is
            # north, so the cell-local v runs from the north edge downward.
            out[m] = bilinear_grid(grid, fx[m], fy[m])
            have[m] = True
    return out, have


def slope_from_msn(path):
    """Slope in degrees from the `_msn` UP channel (G), and the decoded normal.
    The sheet stores (east, up, north) as signed bytes about 128."""
    im = rgb(path) / 255.0 * 2.0 - 1.0
    e, up, nn = im[:, :, 0], im[:, :, 1], im[:, :, 2]
    L = np.sqrt(e * e + up * up + nn * nn)
    up = np.clip(up / np.maximum(L, 1e-6), -1.0, 1.0)
    return np.degrees(np.arccos(up)).astype(np.float32)


# ---------------------------------------------------------------------- colour

def srgb_to_linear(c):
    c = np.asarray(c, np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(np.asarray(c, np.float64), 0.0, None)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1.0 / 2.4) - 0.055)


def saturation(img):
    mx, mn = img.max(2), img.min(2)
    return np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0.0)


# ------------------------------------------------------------------- the fits

def fit_gain(o, v):
    o = np.asarray(o, np.float64).ravel()
    v = np.asarray(v, np.float64).ravel()
    return {'k': float(np.sum(o * v) / np.sum(o * o))}


def apply_gain(o, p):
    return o * p['k']


def fit_affine(o, v):
    o = np.asarray(o, np.float64).ravel()
    v = np.asarray(v, np.float64).ravel()
    A = np.stack([o, np.ones_like(o)], 1)
    sol = np.linalg.lstsq(A, v, rcond=None)[0]
    return {'k': float(sol[0]), 'c': float(sol[1])}


def apply_affine(o, p):
    return o * p['k'] + p['c']


def fit_gamma(o, v, scale=255.0):
    """v = a * o^g, fitted as a straight line in log-log on the normalised
    values.  Texels at or below zero on either side are dropped BY THE FITTER
    and the count is returned, because log(0) is not a small number."""
    on = np.asarray(o, np.float64).ravel() / scale
    vn = np.asarray(v, np.float64).ravel() / scale
    m = (on > 1e-4) & (vn > 1e-4)
    lo, lv = np.log(on[m]), np.log(vn[m])
    A = np.stack([lo, np.ones_like(lo)], 1)
    sol = np.linalg.lstsq(A, lv, rcond=None)[0]
    return {'a': float(np.exp(sol[1])), 'g': float(sol[0]),
            'dropped': int((~m).sum()), 'scale': scale}


def apply_gamma(o, p):
    on = np.clip(np.asarray(o, np.float64) / p['scale'], 0.0, None)
    return p['a'] * on ** p['g'] * p['scale']


def apply_slip_encode(o, p=None):
    """OURS read as LINEAR and written as sRGB -- the BRIGHTER slip."""
    return linear_to_srgb(np.asarray(o, np.float64) / 255.0) * 255.0


def apply_slip_decode(o, p=None):
    """OURS read as sRGB and written as LINEAR -- the DARKER slip."""
    return srgb_to_linear(np.asarray(o, np.float64) / 255.0) * 255.0


def resid(pred, v):
    d = np.asarray(pred, np.float64) - np.asarray(v, np.float64)
    return {'rms': float(np.sqrt(np.mean(d * d))),
            'mae': float(np.mean(np.abs(d))),
            'bias': float(np.mean(d)),
            'n': int(d.size)}


# --------------------------------------------------------------- correlations

def pearson(a, b):
    a = np.asarray(a, np.float64); a = a - a.mean()
    b = np.asarray(b, np.float64); b = b - b.mean()
    d = float(np.sqrt(np.sum(a * a) * np.sum(b * b)))
    return float(np.sum(a * b) / d) if d > 0 else 0.0


def corr_with_floor(res_map, field, mask, seed=1):
    """r of the residual against a field, and against a phase twin of the same
    field as the floor."""
    f = np.asarray(field, np.float64)
    f = np.nan_to_num(f, nan=float(np.nanmean(f)))
    tw = phase_twin(f, seed=seed)
    return {'r': pearson(res_map[mask], f[mask]),
            'floor': pearson(res_map[mask], tw[mask])}
