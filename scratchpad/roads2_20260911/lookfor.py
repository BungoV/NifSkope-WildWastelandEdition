"""Find the crop where the road seam is worst, and dump a quick side-by-side so
the defect is SEEN before it is named (CONSTITUTION 5).

usage: lookfor.py <ourColour.DDS> <out.png>
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds                                   # noqa: E402
import seam                                                 # noqa: E402

try:
    from PIL import Image, ImageDraw
except ImportError:
    Image = None


def main():
    ourp = sys.argv[1]
    outp = sys.argv[2]
    van = Dds(seam.VAN)
    ours = Dds(ourp)
    pz = np.load(os.path.join(HERE, 'proj_m20_20.npz'))
    idbuf, alph, ramp = pz['idbuf'], pz['alpha'], pz['ramped']
    diff, feath, solid = seam.boundaries(idbuf, ramp)
    road = idbuf >= 0
    gv, go = seam.grad_mag(van.lum()), seam.grad_mag(ours.lum())
    exc = np.where(diff, go - gv, 0.0)

    # the 48x48 window with the largest total excess on boundaries
    W = 48
    ii = np.cumsum(np.cumsum(exc, axis=0), axis=1)
    best, bj, bi = -1e30, 0, 0
    for j in range(0, 512 - W):
        for i in range(0, 512 - W):
            s = ii[j + W - 1, i + W - 1]
            if j: s -= ii[j - 1, i + W - 1]
            if i: s -= ii[j + W - 1, i - 1]
            if j and i: s += ii[j - 1, i - 1]
            if s > best:
                best, bj, bi = s, j, i
    print('worst %dx%d window at texel (x=%d, y=%d), boundary excess sum %.1f'
          % (W, W, bi, bj, best))
    print('  in it: road texels %d, boundary texels %d'
          % (int(road[bj:bj + W, bi:bi + W].sum()),
             int(diff[bj:bj + W, bi:bi + W].sum())))
    print('  gradient there: vanilla %.3f  ours %.3f'
          % (gv[bj:bj + W, bi:bi + W][diff[bj:bj + W, bi:bi + W]].mean(),
             go[bj:bj + W, bi:bi + W][diff[bj:bj + W, bi:bi + W]].mean()))
    if Image is None:
        print('PIL missing; no picture')
        return
    Z = 8
    panels = []
    for name, d in (('VANILLA', van), ('OURS', ours)):
        a = np.clip(d.rgb[bj:bj + W, bi:bi + W], 0, 255).astype(np.uint8)
        panels.append((name, Image.fromarray(a).resize((W * Z, W * Z),
                                                       Image.NEAREST)))
    # a third panel: the piece-id map, so the joints are visible as such
    pid = idbuf[bj:bj + W, bi:bi + W]
    rng = np.random.RandomState(7)
    lut = rng.randint(40, 255, size=(int(idbuf.max()) + 2, 3)).astype(np.uint8)
    img = np.zeros((W, W, 3), dtype=np.uint8)
    for y in range(W):
        for x in range(W):
            img[y, x] = lut[pid[y, x] + 1] if pid[y, x] >= 0 else (20, 20, 20)
    panels.append(('PIECES', Image.fromarray(img).resize((W * Z, W * Z),
                                                         Image.NEAREST)))
    pad, top = 8, 22
    out = Image.new('RGB', (len(panels) * (W * Z + pad) + pad,
                            W * Z + top + pad), (16, 16, 16))
    dr = ImageDraw.Draw(out)
    for k, (name, p) in enumerate(panels):
        x = pad + k * (W * Z + pad)
        out.paste(p, (x, top))
        dr.text((x, 6), name, fill=(235, 235, 235))
    out.save(outp)
    print('wrote %s (%dx%d)' % (outp, out.size[0], out.size[1]))


if __name__ == '__main__':
    main()
