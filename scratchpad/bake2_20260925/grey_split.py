"""BAKE2: where are a VT's flat-grey cells? Split by inside/outside the .lodl header box (the only cells the terrain
mesh can sample) and LAND/no-LAND (land dump), and say whether vanilla ships a dim-4 sheet over them.
usage: python grey_split.py <mod ws dir> <world> <land dump> [PHASE env = grid phase]"""
import sys, os, struct, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread
d, world, dump = sys.argv[1:4]
v = vtread.Vt('%s/%s.VT.4.lodt' % (d, world))
m, wW, nN = v.mosaic(v.west, v.south, v.east, v.north); per = v.content // v.levelDim
H, W = m.shape[0] // per, m.shape[1] // per
c = m[..., :3].astype(np.float32).reshape(H, per, W, per, 3).mean((1, 3))
lum = c @ np.array([0.2126, 0.7152, 0.0722], np.float32); chroma = c.max(2) - c.min(2)
g = (np.abs(lum - 129.6) < 0.5) & (chroma <= 3)
hb = open('%s/%s.lodl' % (d, world), 'rb').read(0x18); hx0, hy0, hx1, hy1 = struct.unpack_from('<4i', hb, 8)
b = open(dump, 'rb').read(); mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
fl = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
px, py = map(int, os.environ.get('PHASE', '0,0').split(','))
root = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/%s/' % world
cnt = {}
for r in range(H):
    for q in range(W):
        if not g[r, q]: continue
        x, y = wW + q, nN - 1 - r
        inside = hx0 <= x <= hx1 and hy0 <= y <= hy1
        land = 0 <= x - mnx < cw and 0 <= y - mny < ch and fl[y - mny, x - mnx] != 0
        sx, sy = x - ((x - px) % 4), y - ((y - py) % 4)
        van = os.path.exists(root + '%s.4.%d.%d.dds' % (world, sx, sy))
        k = ('inside header' if inside else 'outside header', 'LAND' if land else 'no-LAND', 'vanilla sheet' if van else 'no vanilla sheet')
        cnt[k] = cnt.get(k, 0) + 1
print('%s: VT.4 box %d..%d x %d..%d, .lodl header %d,%d..%d,%d, flat-grey cells %d' % (world, wW, wW + W - 1, nN - H, nN - 1, hx0, hy0, hx1, hy1, g.sum()))
for k in sorted(cnt): print('  %-15s %-8s %-17s %6d' % (k + (cnt[k],)))
