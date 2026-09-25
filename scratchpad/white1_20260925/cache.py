"""WHITE1 cache: every whole-map field at 32 texels/samples per cell (6144 x 6144, cells -96..95), row 0 = NORTH.
  h.npy        .lodl level-0 heights (world units), installed Commonwealth.lodl (the authority decoder + lodl_bulk scatter)
  on.npy       installed VT.16 colour sheets (fill ON, SEAM1 16:12)            RGB uint8
  off.npy      SEAM1's fill-OFF VT.16 (whole/off)                               RGB uint8
  pre.npy      SEAM1 replaced/ VT.16 (pre-fill install)                         RGB uint8
  van.npy      Bethesda's dim-4 terrain LOD diffuse, 128/cell box-averaged to 32/cell   RGB uint8
Cached raw inputs only; everything derived is recomputed by the analysis scripts."""
import os, sys, hashlib
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/water_20260909')
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import lodl_bulk as L
import vtread, vanilla_tiles
INST = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/'
SEAM = r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/'
SRC = {'on': INST + 'Commonwealth.VT.16.lodt',
       'off': SEAM + 'whole/off/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.16.lodt',
       'pre': SEAM + 'replaced/Commonwealth.VT.16.lodt'}
N = 6144

def out(n): return os.path.join(HERE, n + '.npy')

if not os.path.exists(out('h')):
    d = L.open_lodl(INST + 'Commonwealth.lodl')
    assert (d.minX, d.minY, d.cellsX, d.cellsY, d.spc) == (-96, -96, 192, 192, 32), (d.minX, d.minY)
    h = L.heights(d)[::-1].copy()                  # file row 0 = south -> flip to north-up
    rng = np.random.default_rng(1); bad = 0
    for _ in range(300):                           # the scatter checked against the authority's scalar reader
        gx, gy = int(rng.integers(0, N)), int(rng.integers(0, N))
        w = d.plane_word(gx, gy, 0)
        if w is not None and abs((w - 32767.0) * d.quantum - h[N - 1 - gy, gx]) > 1e-3: bad += 1
    print('lodl heights: scatter vs plane_word mismatches %d of 300' % bad)
    np.save(out('h'), h.astype(np.float32))

for k, p in SRC.items():
    if os.path.exists(out(k)): continue
    v = vtread.Vt(p)
    assert (v.levelDim, v.west, v.north, v.content) == (16, -96, 95, 512), (v.levelDim, v.west, v.north, v.content)
    m, wW, nN = v.mosaic(-96, -96, 95, 95, 1)
    assert m.shape[:2] == (N, N) and wW == -96 and nN == 96, (m.shape, wW, nN)
    np.save(out(k), m[..., :3].copy())
    print(k, p, 'sha1', hashlib.sha1(open(p, 'rb').read(1 << 20)).hexdigest()[:12], '(first MB)')

if not os.path.exists(out('van')):
    van = np.zeros((N, N, 3), np.uint8)
    miss = 0
    for y in range(-96, 96, 4):
        for x in range(-96, 96, 4):
            t = vanilla_tiles.tile('Commonwealth', 4, x, y)
            if t is None: miss += 1; continue
            t = t[..., :3].astype(np.float32).reshape(128, 4, 128, 4, 3).mean((1, 3))
            r0 = (95 - (y + 3)) * 32; c0 = (x + 96) * 32
            van[r0:r0 + 128, c0:c0 + 128] = np.round(t).astype(np.uint8)
    print('vanilla dim-4 tiles missing', miss, 'of', 48 * 48)
    np.save(out('van'), van)
