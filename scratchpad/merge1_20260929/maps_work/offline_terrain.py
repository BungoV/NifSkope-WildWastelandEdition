# MERGE1 copy of MAPS1 offline_terrain.py: input paths come from env MAPS_CW (and MAPS_V2); nothing else changed.
"""offline_terrain.py -- MAPS1: the VT sheets decoded by a reader that shares no code with the viewer
(numpy BC1/BC3 from vtbake1 vtread.py, container parse from tests/spells/lodgen_vt_check.Lodv).

* census of msn R/G/B over the 20 tiles the `normal` view binds (tx 1..5, ty 1..4, ty 0 = NORTH,
  docs/LODGEN_TERRAIN_VT.md:2378) and of mask R/G/B/A over the 25 tiles the mask views read (plus ty 0,
  the row the top-edge vertices fall in) -- the right-hand side of each view refuter;
* offline top-down panels over cells x -5..2, y -10..-3 (1600 px, north up): the height sheet (role 4, R16),
  the msn sheet re-encoded into the same world XYZ colours as the object panel (R east, G north, B up),
  and the colour sheet (role 1);
* the known-slope refuter: msn against the height sheet's own gradient."""
import sys, os, json, struct
import numpy as np
from PIL import Image

sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtbake1_20260923')
import lodgen_vt_check as V
import vtread as VR

HERE = os.path.dirname(os.path.abspath(__file__))
SHEET = os.environ['MAPS_CW'] + '/Commonwealth.VT.2.lodt'
v = V.Lodv(SHEET)
ROLE = {v.sheets[i]['role']: i for i in range(v.sheetCount)}
D, B, C = v.stored, v.border, v.content


def tile(tx, ty, role):
    """content texels of one tile, one sheet, mip 0: (C, C, 4) uint8 or (C, C) uint16 for height."""
    idx = ty * v.tilesX + tx
    e = v.table[idx]
    p = v.payload(idx)
    si = ROLE[role]
    sd = v.sheets[si]
    cover = bool(e['flags'] & 2)
    o = v.sheetOffset(cover, si, 0)
    if role == 4:
        a = np.frombuffer(p, dtype='<u2', count=D * D, offset=o).reshape(D, D)
        return a[B:B + C, B:B + C].astype(np.int32)
    fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
    bb = 16 if fmt in (77, 78) else 8
    nb = D // 4
    blk = np.frombuffer(p, dtype=np.uint8, count=nb * nb * bb, offset=o).reshape(nb, nb, bb)
    rgb = VR.decode_bc1_blocks(blk[..., bb - 8:], bb == 16)
    if bb == 16:
        a = VR.decode_bc3_alpha(blk[..., :8])
    else:
        a = np.full(rgb.shape[:2], -1, dtype=np.int32)
    out = np.concatenate([rgb.astype(np.int32), a[..., None].astype(np.int32)], axis=2)
    return out[B:B + C, B:B + C], fmt


def mosaic(role, txs, tys):
    rows = []
    for ty in tys:
        row = []
        for tx in txs:
            t = tile(tx, ty, role)
            row.append(t if role == 4 else t[0])
        rows.append(np.concatenate(row, axis=1))
    return np.concatenate(rows, axis=0)


out = {}
# ---- censuses (the refuters' right-hand side)
msn20 = mosaic(2, range(1, 6), range(1, 5))
out['msn_20tiles'] = {'texels': int(msn20.shape[0] * msn20.shape[1]),
                      'R': float(msn20[..., 0].mean()), 'G': float(msn20[..., 1].mean()), 'B': float(msn20[..., 2].mean()),
                      'Rmin': int(msn20[..., 0].min()), 'Rmax': int(msn20[..., 0].max()),
                      'Gmin': int(msn20[..., 1].min())}
mask25 = mosaic(5, range(1, 6), range(0, 5))
out['mask_25tiles'] = {'texels': int(mask25.shape[0] * mask25.shape[1])}
for k, n in enumerate('RGBA'):
    ch = mask25[..., k]
    out['mask_25tiles'][n] = float(ch.mean()) if ch.min() >= 0 else 'BC1 somewhere'
    out['mask_25tiles'][n + 'range'] = [int(ch.min()), int(ch.max())]
fmts = sorted(set(tile(tx, ty, 5)[1] for ty in range(0, 5) for tx in range(1, 6)))
out['mask_25tiles']['formats'] = fmts

# ---- top-down panels over cells x -5..2, y -10..-3: the widened mosaic is x -6..3, y -10..-3 (tx 1..5, ty 1..4)
def crop(m):
    cpc = C // v.levelDim            # texels a cell (256)
    return m[:, cpc:cpc + 8 * cpc]   # drop cell -6 on the west and cell 3 on the east

h = crop(mosaic(4, range(1, 6), range(1, 5))).astype(np.float64)
msn = crop(msn20)
col = crop(mosaic(1, range(1, 6), range(1, 5)))
print('crop', h.shape, msn.shape)
lo, hi = np.percentile(h, 0.5), np.percentile(h, 99.5)
out['height_raw'] = {'min': int(h.min()), 'max': int(h.max()), 'p0.5': float(lo), 'p99.5': float(hi)}
g = np.clip((h - lo) / max(1.0, hi - lo) * 255, 0, 255).astype(np.uint8)
Image.fromarray(g).resize((1600, 1600), Image.BOX).save(os.path.join(HERE, 'pics', 'T_height_sheet.png'))
# msn stored: R +east, G up, B +north (VT.md:909) -> world XYZ colours: R east, G north, B up
xyz = np.stack([msn[..., 0], msn[..., 2], msn[..., 1]], axis=2).astype(np.uint8)
Image.fromarray(xyz).resize((1600, 1600), Image.BOX).save(os.path.join(HERE, 'pics', 'T_msn_worldXYZ.png'))
Image.fromarray(msn[..., :3].astype(np.uint8)).resize((1600, 1600), Image.BOX).save(os.path.join(HERE, 'pics', 'T_msn_stored.png'))
Image.fromarray(col[..., :3].astype(np.uint8)).resize((1600, 1600), Image.BOX).save(os.path.join(HERE, 'pics', 'T_colour_sheet.png'))

# ---- known-slope refuter: the height sheet's own gradient vs the msn
# rows are north-up: +row = south. dh/dx along columns (east), dh/dnorth = -d/drow
hs = h
gx = np.zeros_like(hs); gy = np.zeros_like(hs)
gx[:, 1:-1] = (hs[:, 2:] - hs[:, :-2]) / 2.0
gy[1:-1, :] = -(hs[2:, :] - hs[:-2, :]) / 2.0
nx = (msn[..., 0] - 127.5) / 127.5
nn = (msn[..., 2] - 127.5) / 127.5
m = np.zeros_like(hs, dtype=bool); m[2:-2, 2:-2] = True
out['corr_R_vs_minus_dhdx'] = float(np.corrcoef(nx[m], -gx[m])[0, 1])
out['corr_B_vs_minus_dhdnorth'] = float(np.corrcoef(nn[m], -gy[m])[0, 1])
out['corr_R_vs_minus_dhdnorth_wrongaxis'] = float(np.corrcoef(nx[m], -gy[m])[0, 1])
# the steepest east-facing and west-facing texels (top 1% of |dh/dx|, sign picks the facing)
q = np.percentile(np.abs(gx[m]), 99)
east = m & (gx < -q)     # height falls toward the east = the slope faces east = expected R > 128
west = m & (gx > q)
out['steep_east_facing'] = {'texels': int(east.sum()), 'meanR': float(msn[..., 0][east].mean())}
out['steep_west_facing'] = {'texels': int(west.sum()), 'meanR': float(msn[..., 0][west].mean())}
qn = np.percentile(np.abs(gy[m]), 99)
north = m & (gy < -qn)
south = m & (gy > qn)
out['steep_north_facing'] = {'texels': int(north.sum()), 'meanB': float(msn[..., 2][north].mean())}
out['steep_south_facing'] = {'texels': int(south.sum()), 'meanB': float(msn[..., 2][south].mean())}
flat = m & (np.abs(gx) < 1) & (np.abs(gy) < 1)
out['flat_texels'] = {'texels': int(flat.sum()), 'meanRGB': [float(msn[..., k][flat].mean()) for k in range(3)]}
json.dump(out, open(os.path.join(HERE, 'offline_terrain.json'), 'w'), indent=1)
print(json.dumps(out, indent=1))
