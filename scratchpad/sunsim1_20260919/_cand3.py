"""Scratch: street-camera candidates.  The shipped 'street' camera (D) looks
along azimuth 120, i.e. straight INTO four of the five suns, so at elevation 5
and 15 everything is a silhouette and no cast shadow is legible.  These
candidates keep the near-ground eye but turn the bearing toward the ANTI-sun
direction (az 300) so the long shadows rake into the frame across lit ground."""
import numpy as np, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scene import Terrain, Objects
from render import Camera, GBuffer, SunShadow
import shade as SH
from PIL import Image

ter = Terrain(); ob = Objects(verbose=False)
def g(x, y): return float(ter.atf(np.array([x]), np.array([y]))[0])

CAND = {
 'D_old': ((26500., -39500., 105.), (31700., -42500., 400.), 62.),
 'J':     ((27200., -40600., 120.), (20800., -38400., 250.), 62.),
 'K':     ((26200., -42400., 130.), (20600., -40400., 250.), 62.),
 'L':     ((25800., -38600., 110.), (20600., -40800., 200.), 62.),
}
W, H = 560, 315
ims = []
for k, (e, t, f) in CAND.items():
    cam = Camera((e[0], e[1], g(e[0], e[1]) + e[2]),
                 (t[0], t[1], g(t[0], t[1]) + t[2]), f, W, H, name=k)
    gb = GBuffer(cam, ter, ob, verbose=False)
    row = []
    for az, el in ((120., 5.), (120., 15.)):
        ss = SunShadow(ter, ob, az, el)
        lit = np.zeros(len(gb.kind)); m = gb.kind != 0
        lit[m] = ss.lit(gb.pos[m]).astype(float)
        img = SH.to8(SH.shade(gb, lit, el, az), W, H)
        row.append(SH.stamp(img, '%s az%.0f el%.0f' % (k, az, el), (6, 4), 16))
    ims.append(np.concatenate(row, axis=1))
    print(k, 'sky %.0f%% ter %.0f%% obj %.0f%%  lit(el15) %.0f%%'
          % (100 * (gb.kind == 0).mean(), 100 * gb.terfirst.mean(),
             100 * gb.objfirst.mean(), 100 * lit[m].mean()))
Image.fromarray(np.concatenate(ims, axis=0)).save(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), 'images/_cand3.png'))
