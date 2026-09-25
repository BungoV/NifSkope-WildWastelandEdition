"""SEAM1 gate G1 (pre-registered): the colour step across the Sanctuary block's four chunk borders, read from a VT.2
container's own texels. Bar = max(3.0, the p99 of every other texel-to-texel step in the same profiles).
usage: edge_gate.py <Commonwealth.VT.2.lodt> [label] [png]"""
import sys, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread
def measure(path, png=None):
    v = vtread.Vt(path)
    m, wW, nN = v.mosaic(-24, 16, -13, 27, 1)
    if png:
        from PIL import Image; Image.fromarray(m[..., :3]).save(png)
    lum = m[..., :3].astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    tpc = v.content // v.levelDim            # texels per cell
    X = lambda cx: (cx - wW) * tpc; Y = lambda cy: (nN - cy) * tpc
    colp = lum[Y(24):Y(20)].mean(0); rowp = lum[:, X(-20):X(-16)].mean(1)
    dc = np.abs(np.diff(colp)); dr = np.abs(np.diff(rowp))
    steps = {'west x=-20': dc[X(-20) - 1], 'east x=-16': dc[X(-16) - 1], 'north y=24': dr[Y(24) - 1], 'south y=20': dr[Y(20) - 1]}
    mask_c = np.ones_like(dc, bool); mask_c[[X(-20) - 1, X(-16) - 1]] = False
    mask_r = np.ones_like(dr, bool); mask_r[[Y(24) - 1, Y(20) - 1]] = False
    bar = max(3.0, float(np.percentile(np.concatenate([dc[mask_c], dr[mask_r]]), 99)))
    return steps, bar
if __name__ == '__main__':
    steps, bar = measure(sys.argv[1], sys.argv[3] if len(sys.argv) > 3 else None)
    worst = max(steps.values())
    print('%s: %s | bar %.2f | %s' % (sys.argv[2] if len(sys.argv) > 2 else sys.argv[1],
          ' '.join('%s %.2f' % (k, v) for k, v in steps.items()), bar, 'GREEN' if worst <= bar else 'RED'))
