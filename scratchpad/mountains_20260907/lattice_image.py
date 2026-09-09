"""Lane LATTICE -- what PERIOD is the square pattern bungo saw, in the picture?

The sheet is 512 texels over a 16384-unit chunk (32 units a texel).  The height
grid is 129 samples (128 units apart = 4 texels).  The mesh is its own grid.
Before chasing a period in the sheet, measure the one in the RENDER, so the
number chased is the number he is looking at.

Method: take a patch of the rendered frame, subtract a local blur, window it,
2-D FFT, and report the strongest non-DC peak and its period in screen pixels.
Vanilla's frame, same camera and same patch, is the control -- if both show the
same peak it is the renderer or the mesh, not our sheet.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, 'images')


def lum(path, box):
    a = np.asarray(Image.open(path).convert('RGB'), dtype=np.float64)
    x0, y0, x1, y1 = box
    a = a[y0:y1, x0:x1]
    return 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]


def boxmean(a, k):
    r = k // 2
    p = np.pad(a, r, mode='edge')
    cs = np.cumsum(p, axis=0)
    cs = np.vstack([np.zeros((1, cs.shape[1])), cs])
    s = cs[k:] - cs[:-k]
    cs = np.cumsum(s, axis=1)
    cs = np.hstack([np.zeros((cs.shape[0], 1)), cs])
    return (cs[:, k:] - cs[:, :-k]) / float(k * k)


def peaks(f, label, top=6):
    n0, n1 = f.shape
    w = np.outer(np.hanning(n0), np.hanning(n1))
    F = np.fft.fftshift(np.abs(np.fft.fft2((f - f.mean()) * w)) ** 2)
    cy, cx = n0 // 2, n1 // 2
    F[cy - 2:cy + 3, cx - 2:cx + 3] = 0          # kill DC and its skirt
    idx = np.argsort(F.ravel())[::-1]
    print('  %s   (patch %dx%d, rms %.3f)' % (label, n0, n1, f.std()))
    seen = []
    for i in idx:
        y, x = divmod(int(i), n1)
        ky, kx = y - cy, x - cx
        if any((ky - a) ** 2 + (kx - b) ** 2 < 9 for a, b in seen):
            continue
        seen.append((ky, kx))
        seen.append((-ky, -kx))
        py = n0 / abs(ky) if ky else float('inf')
        px = n1 / abs(kx) if kx else float('inf')
        r = np.hypot(ky / float(n0), kx / float(n1))
        print('     k=(%+3d,%+3d)  power %.3g  period x %6.1f px  y %6.1f px  '
              'along-k %5.1f px' % (ky, kx, F[y, x], px, py, 1.0 / r if r else 0))
        if len(seen) >= 2 * top:
            break


def main(argv):
    # patch: a quiet stretch of the mountain face, the same box in both frames.
    # shot_*4.png are 1507x841; the delivered crop was (180,350)-(900,841).
    boxes = {
        'right smooth face': (620, 400, 876, 528),
        'upper right': (560, 393, 816, 521),
        'mid face': (330, 430, 586, 558),
    }
    for name, box in boxes.items():
        print('\n=== patch %s  %s ===' % (name, box))
        for side, png in (('ours', 'shot_ours4.png'), ('vanilla', 'shot_van4.png')):
            f = lum(os.path.join(IMG, png), box)
            hp = f - boxmean(f, 15)
            peaks(hp, '%-8s' % side)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
