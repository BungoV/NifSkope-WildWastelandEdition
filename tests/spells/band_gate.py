"""FOGGLOW1 banding gate on an 8-bit picture (the staircase an undithered 8-bit write leaves on a smooth gradient).
python -I bandgate.py <png> [--bar RUN] [--hdr <the row's .hdr>] [--label X]
Regions (1920 x ~1045 Memory Den still_memden camera): left wall, the air right of the red neon, the air round the pole
lamp, the dark sky band. Per region and per axis (y, x): along each line, runs of one 8-bit value (G channel, the
luminance carrier); report the mean run length over runs whose neighbours differ by exactly 1 level (the staircase
steps), and P8 = the share of pixels on a run of >= 8 px bounded by +-1 steps.
A smooth region's plateaus are long (a staircase); a dithered write breaks them (runs ~1-3 px).
Texture is excluded by the HDR (optional): pixels whose sqrt-luminance d2 > 4x the region's median are masked out.
Verdict: PASS when every region's max(mean run y, mean run x) <= bar (default 4.0)."""
import sys
import numpy as np
from PIL import Image

REG = {	# x0 y0 x1 y1 (top-left origin)
    'left_wall': (20, 380, 330, 640),
    'red_air': (1150, 300, 1330, 560),
    'lamp_air': (1380, 220, 1800, 360),
    'top_dark': (1150, 20, 1450, 180),
}


def runs_1d(v):
    """runs of equal values in one line: (start, length, value)"""
    d = np.flatnonzero(np.diff(v) != 0) + 1
    st = np.concatenate(([0], d)); en = np.concatenate((d, [len(v)]))
    return st, en - st, v[st]


def stair(G, mask):
    lens = []
    p8 = 0
    tot = 0
    for line, m in zip(G, mask):
        st, ln, val = runs_1d(line.astype(int))
        for k in range(1, len(st) - 1):
            if abs(val[k] - val[k - 1]) == 1 and abs(val[k + 1] - val[k]) == 1 and m[st[k]:st[k] + ln[k]].all():
                lens.append(ln[k])
                if ln[k] >= 8:
                    p8 += ln[k]
        tot += int(m.sum())
    return (float(np.mean(lens)) if lens else 0.0), len(lens), p8 / max(tot, 1)


def main():
    a = sys.argv[1:]
    png = a[0]
    bar = float(a[a.index('--bar') + 1]) if '--bar' in a else 4.0
    hdr = a[a.index('--hdr') + 1] if '--hdr' in a else None
    lab = a[a.index('--label') + 1] if '--label' in a else png.split('/')[-1]
    G = np.asarray(Image.open(png).convert('RGB'))[..., 1]
    F = None
    if hdr:
        raw = open(hdr, 'rb').read()
        w, h = np.frombuffer(raw[:8], dtype=np.int32)
        px = np.frombuffer(raw[8:8 + int(w) * int(h) * 16], dtype=np.float32).reshape(int(h), int(w), 4)[::-1]
        F = np.sqrt(np.maximum(0.2126 * px[..., 0] + 0.7152 * px[..., 1] + 0.0722 * px[..., 2], 0)).astype(np.float64)
    worst = 0.0
    for name, (x0, y0, x1, y1) in REG.items():
        g = G[y0:y1, x0:x1]
        m = np.ones_like(g, dtype=bool)
        if F is not None and F.shape == G.shape:
            f = F[y0:y1, x0:x1]
            d2 = np.zeros_like(f)
            d2[1:-1, 1:-1] = np.abs(4 * f[1:-1, 1:-1] - f[2:, 1:-1] - f[:-2, 1:-1] - f[1:-1, 2:] - f[1:-1, :-2])
            m = d2 <= 4.0 * np.median(d2[1:-1, 1:-1]) + 1e-9
        ry, ny, p8y = stair(g.T, m.T)	# along y: lines are columns
        rx, nx, p8x = stair(g, m)
        worst = max(worst, ry, rx)
        print('%-14s %-10s run y %5.2f (n %5d, P8 %.3f)  run x %5.2f (n %5d, P8 %.3f)  mask %.2f  mean %.1f'
              % (lab, name, ry, ny, p8y, rx, nx, p8x, m.mean(), g.mean()))
    v = 'PASS' if worst <= bar else 'FAIL'
    print('%-14s worst run %.2f bar %.2f %s' % (lab, worst, bar, v))
    sys.exit(0 if v == 'PASS' else 1)


if __name__ == '__main__':
    main()
