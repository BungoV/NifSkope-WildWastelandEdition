"""decallit1_check.py <dir> <lighttag> <gain>: judges the DECALLIT1 gate pictures (tests/spells/decallit1_cell.sh).

  <lighttag>.png  probe 6 x gain with every effect decal drawn white and opaque: the decal's light alone
  wall.png        probe 6 x gain with the effect decals hidden: the wall's lit colour at the same pixels
  alb.png         probe 81 with the effect decals hidden: the wall's albedo as drawn (gamma); its linear albedo is alb^2

The region is where the decals drew: the light picture differs from the wall picture (geometry, not a hand mask).
Per pixel the wall's light is wall / alb^2; the decal's light over it must be the same: median ratio within
0.8..1.25, under 1 % of the readable region clipped where the wall's light is below white, and a region of at least 5000 pixels. Exit 0 on pass, 1 on fail."""
import sys
import numpy as np
from PIL import Image

D, TAG, GAIN = sys.argv[1], sys.argv[2], float(sys.argv[3])


def ld(n):
    return np.array(Image.open(f'{D}/{n}.png').convert('RGB')).astype(np.float64) / 255.0


light, wall, alb = ld(TAG), ld('wall'), ld('alb')
H = min(a.shape[0] for a in (light, wall, alb))
W = min(a.shape[1] for a in (light, wall, alb))
light, wall, alb = light[:H, :W], wall[:H, :W], alb[:H, :W]
lum = lambda a: a @ np.array([0.2126, 0.7152, 0.0722])

Ll, Lw, La = lum(light), lum(wall), lum(alb ** 2)
region = np.abs(light - wall).max(2) > 3.0 / 255
# a ratio needs a readable wall: albedo and wall light above the 8-bit floor
ok = region & (La > 0.01) & (Lw > 3.0 / 255) & (wall.max(2) < 254.0 / 255)
# clipped: the decal light hit 8-bit white where the wall's own light (wall / alb^2, per channel) stays below it; where
# the wall's light x gain is itself past white (the Third Rail neon, lamp-lit sills) the decal clips by right
expect = (wall / np.maximum(alb ** 2, 0.01)).max(2)
clip = ok & (light.max(2) >= 254.0 / 255) & (expect < 0.9)
n, nok = int(region.sum()), int(ok.sum())
fails = 0


def check(msg, good):
    global fails
    print(('  PASS  ' if good else '  FAIL  ') + msg)
    fails += 0 if good else 1


check(f'decal region {n} px ({nok} readable) >= 5000', nok >= 5000)
if nok:
    wl = Lw[ok] / La[ok]
    ratio = Ll[ok] / np.maximum(wl, 1e-6)
    med = float(np.median(ratio))
    q1, q3 = np.percentile(ratio, [25, 75])
    ys, xs = np.nonzero(ok)
    print(f'  region bbox x {xs.min()}..{xs.max()} y {ys.min()}..{ys.max()}')
    print(f'  wall light (linear, /gain) median {np.median(wl) / GAIN:.4f}   decal light median {np.median(Ll[ok]) / GAIN:.4f}')
    print(f'  ratio decal / wall light: median {med:.3f}  quartiles {q1:.3f} .. {q3:.3f}')
    check(f'decal light / wall light median {med:.3f} within 0.8 .. 1.25', 0.8 <= med <= 1.25)
frac = clip.sum() / max(nok, 1)
check(f'clipped {100 * frac:.2f} % of the readable region (wall light below white) < 1 %', frac < 0.01)
m = np.zeros((H, W, 3), np.uint8)
m[region] = (90, 90, 0)
m[ok] = (255, 255, 0)
m[clip] = (255, 0, 0)
Image.fromarray(m).save(f'{D}/{TAG}_region.png')
print(f'decallit1_check: {fails} failure(s)')
sys.exit(1 if fails else 0)
