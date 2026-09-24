"""SPLAT1 -- the chunk colour bake, reproduced OFFLINE, with switches.

An independent re-implementation of `lodgenBakeTerrainTextures`'s colour
composite (src/lodgen.cpp:6413-6591), vectorised per quadrant, so that one
term at a time can be turned off and the variance it owns read straight off
the sheet. Nothing is imported from the generator; the ESM walk is
`tests/spells/lodgen_cover_model.py`'s, already independent and already gated.

The switches:

  mip = 'code'   the mip `src/lodgen.cpp:6551` picks
      = 'box'    the EXACT box mean of mip 0 over the texel's world footprint
      = 'mean'   the texture's global mean (the coarsest mip)
      = float    a fixed bias added to the code's mip
  layers = True/False       the ATXT blend, or the base alone
  vclr   = True/False       step 6
  tile   = the world units per texture repeat (the code's TILE)
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
import splatlib as S                                          # noqa: E402
from lodgen_cover_model import Esm, dominant_base             # noqa: E402
from lodgen_terrain_model import find_asset, find_material    # noqa: E402

ESM = r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
DATA = r'E:/Tools/Fallout 4/DataUnpacked/Data'
CELL = 4096.0
RES = 512

_esm = None
_tex = {}


def esm():
    global _esm
    if _esm is None:
        _esm = Esm(ESM)
        _esm.walk(0x0000003C)
    return _esm


def _bgsm_first_dds(path):
    """The diffuse of a BGSM, without re-deriving the whole layout: the first
    length-prefixed ASCII string in the file that ends in `.dds`. Checked in
    s2 against the texture the bake's own census names."""
    b = open(path, 'rb').read()
    i = 0
    while i + 4 <= len(b):
        n = int.from_bytes(b[i:i + 4], 'little')
        if 4 < n < 260 and i + 4 + n <= len(b):
            s = b[i + 4:i + 4 + n]
            try:
                t = s.decode('latin-1')
            except Exception:
                t = ''
            if t.lower().endswith('.dds') and all(32 <= c < 127 for c in s):
                return t
        i += 1
    return None


def diffuse_of(form):
    """The DDS a landscape texture's LTEX resolves to, cached."""
    if form in _tex:
        return _tex[form]
    e = esm()
    out = None
    name = ''
    rec = e.ltex.get(form)
    if rec:
        ts = e.txst.get(rec['tnam'])
        if ts:
            p = None
            if ts['tx00']:
                p = find_asset(DATA, ts['tx00'])
                name = ts['tx00']
            elif ts['mnam']:
                m = find_material(DATA, ts['mnam'])
                if m:
                    d = _bgsm_first_dds(m)
                    if d:
                        p = find_asset(DATA, d)
                        name = d
            if p:
                try:
                    out = S.Dds(p)
                except Exception:
                    out = None
    _tex[form] = (out, name)
    return _tex[form]


def _tap(dds, wx, wy, tile, upt, mip):
    u = np.mod(wx / tile, 1.0)
    v = np.mod(wy / tile, 1.0)
    if mip == 'box':
        return S.footprint_box(dds, wx, wy, tile, upt)[:, :3]
    if mip == 'mean':
        m = float(dds.maxMip)
    else:
        m = S.bake_mip(dds, tile, upt)
        if isinstance(mip, (int, float)):
            m = min(max(m + float(mip), 0.0), float(dds.maxMip))
    return S.sample_trilinear(dds, u, v, m)[..., :3]


def code_mip(form, tile=2048.0, upt=32.0):
    dds, name = diffuse_of(form)
    if dds is None:
        return None
    return dict(name=name, width=dds.width, maxMip=dds.maxMip,
                texelWorld=tile / dds.width,
                mip=S.bake_mip(dds, tile, upt),
                mipSide=dds.width >> int(S.bake_mip(dds, tile, upt)),
                mipTexelWorld=tile / float(dds.width >> int(S.bake_mip(dds, tile, upt))))


def bake(cx0, cy0, dim=4, mip='code', layers=True, vclr=True, tile=2048.0,
         res=RES):
    """A (res,res,3) sheet in 0..255, row 0 = the chunk's NORTH edge."""
    e = esm()
    span = float(dim) * CELL
    upt = span / float(res)
    dom = dominant_base(e, (cx0 // 4) * 4, (cy0 // 4) * 4, 4)
    cwX, cwY = cx0 * CELL, cy0 * CELL
    py, px = np.mgrid[0:res, 0:res]
    wy = cwY + (1.0 - (py + 0.5) / res) * span
    wx = cwX + ((px + 0.5) / res) * span
    out = np.full((res, res, 3), 127.5, np.float64)
    for ci in range(dim * dim):
        cx, cy = cx0 + ci % dim, cy0 + ci // dim
        land = e.lands.get((cx, cy))
        if not land:
            continue
        for q in range(4):
            x0 = cx * CELL + (2048.0 if q & 1 else 0.0)
            y0 = cy * CELL + (2048.0 if q & 2 else 0.0)
            m = ((wx >= x0) & (wx < x0 + 2048.0) & (wy >= y0) & (wy < y0 + 2048.0))
            if not m.any():
                continue
            qwx, qwy = wx[m], wy[m]
            qx = (qwx - x0) / 2048.0
            qy = (qwy - y0) / 2048.0
            base = land['base'][q] or dom
            bd, _ = diffuse_of(base) if base else (None, '')
            col = (_tap(bd, qwx, qwy, tile, upt, mip) if bd is not None
                   else np.full((qwx.size, 3), 127.5))
            if layers:
                fx = np.clip(qx * 16.0, 0.0, 15.999)
                fy = np.clip(qy * 16.0, 0.0, 15.999)
                ix, iy = fx.astype(np.int64), fy.astype(np.int64)
                tx, ty = fx - ix, fy - iy
                for lay in land['layers'][q]:
                    op = np.asarray(lay['op'], np.float64)     # 17x17
                    a = ((op[iy, ix] * (1 - tx) + op[iy, ix + 1] * tx) * (1 - ty)
                         + (op[iy + 1, ix] * (1 - tx) + op[iy + 1, ix + 1] * tx) * ty)
                    a = np.clip(a, 0.0, 1.0)
                    use = a > 0.001
                    if not use.any():
                        continue
                    f = lay['ltex'] or dom
                    ld, _ = diffuse_of(f) if f else (None, '')
                    if ld is None:
                        continue
                    lc = _tap(ld, qwx, qwy, tile, upt, mip)
                    col = col + (lc - col) * np.where(use, a, 0.0)[:, None]
            if vclr and land.get('vclr'):
                clx, cly = qwx - cx * CELL, qwy - cy * CELL
                gx = np.clip(clx / CELL * 32.0, 0.0, 31.999)
                gy = np.clip(cly / CELL * 32.0, 0.0, 31.999)
                ix, iy = gx.astype(np.int64), gy.astype(np.int64)
                tx, ty = gx - ix, gy - iy
                vc = np.asarray(land['vclr'], np.float64).reshape(33, 33, 3)
                c = ((vc[iy, ix] * (1 - tx)[:, None] + vc[iy, ix + 1] * tx[:, None])
                     * (1 - ty)[:, None]
                     + (vc[iy + 1, ix] * (1 - tx)[:, None]
                        + vc[iy + 1, ix + 1] * tx[:, None]) * ty[:, None])
                col = col * (c / 255.0)
            out[m] = col
    return out


def vclr_map(cx0, cy0, dim=4):
    """Which of the 16 cells carries a VCLR at all, and its range."""
    e = esm()
    rows = []
    for ci in range(dim * dim):
        cx, cy = cx0 + ci % dim, cy0 + ci // dim
        land = e.lands.get((cx, cy))
        v = land.get('vclr') if land else None
        if not v:
            rows.append((cx, cy, False, None, None))
        else:
            a = np.asarray(v, np.float64)
            rows.append((cx, cy, True, float(a.min()), float(a.max())))
    return rows
