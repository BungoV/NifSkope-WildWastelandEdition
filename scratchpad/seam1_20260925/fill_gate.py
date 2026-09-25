"""SEAM1 fill gates, whole map (pre-registered 2026-09-25 before the build; docs/LODGEN_TERRAIN_VT.md 2.6).
Inputs: two VT.2 pyramids from the SAME exe, the same load order and settings, --vt-fill-vanilla off (before) and on
(after). Painted = w2_cells.pkl 'painted' (a LAND quadrant with BTXT or any ATXT layer, Fallout4.esm).
  FG1 flat chunks: dim-4 chunks whose colour SD (mean over RGB, mip-1 content) < 2. RED on the shipped (pre-SEAM1)
      bake (the floor: > 0 there), GREEN when the after bake has 0. SD < 4.5 reported beside it.
  FG2 his paint wins: every tile whose cells AND one-cell ring are all painted is byte-identical in EVERY role; every
      other tile is byte-identical in every role except colour (the fill writes colour only).
  FG3 the fill is wired: colour moved on > 0 tiles, and on 0 fully-painted tiles (FG2 says the second).
The border-step gate is fill_model.py per region with VT2=<after> (A = the file): at-line steps under the line bar,
cell-mean steps no worse than the model's F and the engine-default B.
usage: fill_gate.py <before VT.2.lodt> <after VT.2.lodt> [<shipped VT.2.lodt, the FG1 floor>]"""
import sys, pickle
import numpy as np
HERE = 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925'
sys.path.insert(0, HERE)
import vtread

A, B = vtread.Vt(sys.argv[1]), vtread.Vt(sys.argv[2])
P = pickle.load(open(HERE + '/w2_cells.pkl', 'rb'))['painted']
assert (A.tileCount, A.levelDim, A.content, A.border) == (B.tileCount, B.levelDim, B.content, B.border)
roles = {s: A.sheets[s]['role'] for s in range(A.sheetCount)}


def tile_cells(i, v):
    ty, tx = divmod(i, v.tilesX)
    return v.west + tx * v.levelDim, v.north - (ty + 1) * v.levelDim + 1


def all_painted(cx0, cy0, D):
    return all((x, y) in P for x in range(cx0 - 1, cx0 + D + 1) for y in range(cy0 - 1, cy0 + D + 1))


# FG2 / FG3
bad_full, bad_other, moved, nfull, n = [], [], 0, 0, 0
for i in range(A.tileCount):
    pa, pb = A.payload(i), B.payload(i)
    if pa is None and pb is None:
        continue
    n += 1
    cx0, cy0 = tile_cells(i, A)
    full = all_painted(cx0, cy0, A.levelDim)
    nfull += full
    if (pa is None) != (pb is None):
        (bad_full if full else bad_other).append((cx0, cy0, 'presence'))
        continue
    ca, cb = bool(A.tFlags[i] & 2), bool(B.tFlags[i] & 2)
    for s, r in roles.items():
        oa, ob = A.sheetOffset(ca, s, 0), B.sheetOffset(cb, s, 0)
        na = sum(A.sheetMipBytes(s, m, ca) for m in range(A.mips))
        nb = sum(B.sheetMipBytes(s, m, cb) for m in range(B.mips))
        same = na == nb and pa[oa:oa + na] == pb[ob:ob + nb]
        if same:
            continue
        if full:
            bad_full.append((cx0, cy0, r))
        elif r != 1:
            bad_other.append((cx0, cy0, r))
        else:
            moved += 1
print('tiles %d, fully painted (tile + ring) %d' % (n, nfull))
print('FG2 fully painted tiles identical in every role: %s (%d differ %s) | other tiles identical except colour: %s (%d differ %s)' % (
    'GREEN' if not bad_full else 'RED', len(bad_full), bad_full[:5],
    'GREEN' if not bad_other else 'RED', len(bad_other), bad_other[:5]))
print('FG3 colour moved on %d tiles: %s' % (moved, 'GREEN' if moved > 0 else 'RED'))


# FG1
def chunk_sd(v):
    C, Bd = v.content >> 1, v.border >> 1
    s1 = [k for k in range(v.sheetCount) if v.sheets[k]['role'] == 1][0]
    out = {}
    for cy0 in range(-96, 96, 4):
        for cx0 in range(-96, 96, 4):
            px = []
            for dy in (0, 2):
                for dx in (0, 2):
                    i, tx, ty = v.index(cx0 + dx, cy0 + dy)
                    if not (0 <= tx < v.tilesX and 0 <= ty < v.tilesY):
                        continue
                    p = v.payload(i)
                    if p is None:
                        continue
                    cover = bool(v.tFlags[i] & 2)
                    sd = v.sheets[s1]
                    fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
                    o = v.sheetOffset(cover, s1, 1)
                    px.append(vtread.decode(p, o, v.stored >> 1, fmt)[Bd:Bd + C, Bd:Bd + C, :3]
                              .reshape(-1, 3).astype(np.float32))
            if px:
                out[(cx0, cy0)] = float(np.concatenate(px).std(0).mean())
    return out


sa, sb = chunk_sd(A), chunk_sd(B)
# the RED floor is the pre-SEAM1 code: the shipped bake (old law, grey fallback), third argument
SHIP = sys.argv[3] if len(sys.argv) > 3 else 'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
ss = chunk_sd(vtread.Vt(SHIP))
fs2 = sorted(k for k, s in ss.items() if s < 2)
print('FG1 shipped (pre-SEAM1 code) flat chunks SD<2: %d, SD<4.5: %d' % (len(fs2), sum(1 for s in ss.values() if s < 4.5)))
fa2 = sorted(k for k, s in sa.items() if s < 2); fb2 = sorted(k for k, s in sb.items() if s < 2)
fa4 = sum(1 for s in sa.values() if s < 4.5); fb4 = sum(1 for s in sb.values() if s < 4.5)
print('FG1 flat dim-4 chunks (SD<2): before %d, after %d %s | SD<4.5: before %d, after %d | chunks %d' % (
    len(fa2), len(fb2), fb2[:10], fa4, fb4, len(sb)))
print('FG1 floor (shipped > 0): %s | FG1 after == 0: %s' % (
    'GREEN' if fs2 else 'RED', 'GREEN' if not fb2 else 'RED'))
pickle.dump({'before': sa, 'after': sb}, open(HERE + '/fill_gate_chunks.pkl', 'wb'))
