"""BAKE2 halo gate (bungo 2026-09-25 on prewar_topdown.png: "What's that glow around the playable area?").
Pre-registered before the fix. RED on the current code's pre-war fill-ON bake, GREEN after.

Cause measured by halo.py: a no-LAND cell has no colour of ours, only the generator's flat placeholder grey
(lum 129.6, chroma 2). The --vt-fill-vanilla law blends FROM the texel's own colour with
w = smoothstep(0, band, d), d = distance to the nearest painted cell, so the first cells off the LAND edge keep
most of the placeholder: a pale ring 1..3 cells wide fading into vanilla.

Checks on the no-LAND cells, grouped by Chebyshev distance d to the nearest LAND cell, against the far field
(d >= 6, where w = 1 on today's band of 4):
  H1  for d = 1, 2, 3: mean(lum ON - lum VAN) within 2.0 of the far field's        (today +38.1/+30.5/+19.0 vs +10.8)
  H2  for d = 1, 2, 3: mean(chroma ON - chroma VAN) within 3.0 of the far field's
  H3  no no-LAND cell at d <= 3 whose lum exceeds VAN + far offset by more than 8   (the halo, cell by cell)
  H4  the far field itself is filled: |mean lum ON - (VAN + far offset)| trivially 0, and its chroma > 10
      (a fill that did nothing would pass H1..H3 as all-grey; this catches it)
usage: python halo_gate.py <land dump> <ON mod ws dir> <world> <x0> <y0> <x1> <y1>"""
import sys, struct, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread, vanilla_tiles as VT

dump, on_dir, world = sys.argv[1:4]
x0, y0, x1, y1 = map(int, sys.argv[4:8])
b = open(dump, 'rb').read()
mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
flags = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
W = x1 - x0 + 1; H = y1 - y0 + 1
land = np.zeros((H, W), bool)
for r in range(H):
    for c in range(W):
        i, j = x0 + c - mnx, y1 - r - mny
        land[r, c] = 0 <= i < cw and 0 <= j < ch and flags[j, i] != 0
dist = np.full((H, W), 99, np.int32); dist[land] = 0; cur = land.copy()
for d in range(1, 60):
    p = np.pad(cur, 1); nb = np.zeros_like(cur)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            nb |= p[1 + dy:1 + dy + H, 1 + dx:1 + dx + W]
    dist[nb & ~cur] = d; cur = nb
    if cur.all(): break

def cells_from(mos, wW, nN, per):
    out = np.zeros((H, W, 3), np.float32)
    for r in range(H):
        for c in range(W):
            rr = (nN - 1 - (y1 - r)) * per; cc = (x0 + c - wW) * per
            out[r, c] = mos[rr:rr + per, cc:cc + per, :3].reshape(-1, 3).mean(0)
    return out
v = vtread.Vt('%s/%s.VT.4.lodt' % (on_dir, world))
m, wW, nN = v.mosaic(x0, y0, x1, y1)
on = cells_from(m, wW, nN, v.content // v.levelDim)
import os   # PHASE=px,py: the worldspace's dim-4 grid phase (LODSettings SW cell mod 4); Far Harbor 3,1
px, py = map(int, os.environ.get('PHASE', '0,0').split(','))
ax = lambda z: z - ((z - px) % 4); ay = lambda z: z - ((z - py) % 4)
vm, n = VT.mosaic(world, 4, ax(x0), ay(y0), ax(x1), ay(y1))
van = cells_from(vm, ax(x0), ay(y1) + 4, n // 4)
K = np.array([0.2126, 0.7152, 0.0722], np.float32)
lo, lv = on @ K, van @ K
co, cv = on.max(2) - on.min(2), van.max(2) - van.min(2)
ok = True
def line(nm, good, d):
    global ok; ok &= bool(good); print('%s %s: %s' % ('PASS' if good else 'FAIL', nm, d))
far = dist >= 6
fo = (lo - lv)[far].mean(); fc = (co - cv)[far].mean()
print('no-LAND cells %d (LAND %d); far field d>=6: %d cells, lum ON-VAN %+.2f, chroma ON-VAN %+.2f'
      % ((~land).sum(), land.sum(), far.sum(), fo, fc))
for d in (1, 2, 3):
    s = dist == d
    dl = (lo - lv)[s].mean(); dc = (co - cv)[s].mean()
    line('H1 d=%d lum' % d, abs(dl - fo) <= 2.0, 'ON-VAN %+.2f vs far %+.2f (%d cells)' % (dl, fo, s.sum()))
    line('H2 d=%d chroma' % d, abs(dc - fc) <= 3.0, 'ON-VAN %+.2f vs far %+.2f' % (dc, fc))
near = (dist >= 1) & (dist <= 3)
hot = near & (lo - (lv + fo) > 8.0)
line('H3 halo cells', hot.sum() == 0, '%d of %d no-LAND cells at d<=3 above VAN+offset+8; max excess %+.1f'
     % (hot.sum(), near.sum(), (lo - (lv + fo))[near].max()))
line('H4 far field filled', far.sum() > 0 and co[far].mean() > 10.0,
     'chroma ON %.1f (placeholder grey = 2)' % co[far].mean())
print('HALO GATE %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
