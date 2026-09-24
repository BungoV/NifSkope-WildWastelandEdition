"""Section 1c: the striping, measured.

The picture (images/diag_worst.png) shows our road carrying evenly spaced
parallel bands where vanilla's carries smooth mottle.  Two candidates:

  S-A  THE ROAD TEXTURE'S OWN PERIOD, printed at full contrast because the
       bake samples it too sharply -- the same class of defect SPLAT1 found for
       the land textures.  Discriminator: the dominant spatial period of our
       road luminance, in WORLD UNITS, against the world distance of one UV
       repeat measured from the road meshes themselves.  If they agree, the
       bands are the texture's tiling.
  S-B  A PER-PIECE STEP (a compositing or a per-piece mip/colour difference).
       Discriminator: the bands would land on piece boundaries.  Measured as
       the fraction of band crests that fall within one texel of a boundary.

usage: stripes.py <ourColour.DDS> [tag]
"""

import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds, Nif                              # noqa: E402
from rasterlib import MeshCache, local_to_model             # noqa: E402
import placements                                           # noqa: E402
import seam                                                 # noqa: E402

UPT = 32.0                                     # world units a bake texel


def uv_world_period():
    """World distance of one UV repeat, per road shape, from the meshes."""
    pl = [p for p in placements.load(seam.REFS)
          if seam.is_roadish(p['sig'], p['modl'])]
    mc = MeshCache(seam.DATA)
    seen = set()
    out = []
    for p in pl:
        key = p['modl'].lower()
        if key in seen:
            continue
        seen.add(key)
        full = mc.path_for(p['modl'])
        if full is None:
            continue
        try:
            nif = Nif(full)
        except Exception:
            continue
        for idx, sh in nif.shapes.items():
            if not sh['verts'] or not sh['tris'] or not sh['uvs']:
                continue
            R, t, s = local_to_model(nif, sh)
            v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t
            v = v * p['scale']
            uv = np.array(sh['uvs'], dtype=np.float64)
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            if tri.shape[0] == 0:
                continue
            # world area / uv area over the whole shape, via triangles
            wa = 0.0; ua = 0.0
            for k in range(min(tri.shape[0], 400)):
                a, b, c = v[tri[k]]
                e1, e2 = b - a, c - a
                wa += 0.5 * np.linalg.norm(np.cross(e1, e2))
                ua0, ub, uc = uv[tri[k]]
                ua += 0.5 * abs((ub[0] - ua0[0]) * (uc[1] - ua0[1])
                                - (uc[0] - ua0[0]) * (ub[1] - ua0[1]))
            if ua <= 1e-9 or wa <= 1e-9:
                continue
            out.append(dict(model=p['modl'], shape=sh['name'],
                            world_per_repeat=math.sqrt(wa / ua)))
    return out


def dominant_period(L, mask):
    """Radially binned power of the masked, mean-removed field; the period in
    TEXELS at the peak, ignoring the lowest three bins (the tile's own shape)."""
    f = np.where(mask, L - L[mask].mean(), 0.0)
    P = np.abs(np.fft.fftshift(np.fft.fft2(f))) ** 2
    n = L.shape[0]
    cy = cx = n // 2
    yy, xx = np.mgrid[0:n, 0:n]
    r = np.hypot(yy - cy, xx - cx)
    nb = n // 2
    prof = np.zeros(nb)
    for k in range(nb):
        m = (r >= k) & (r < k + 1)
        prof[k] = P[m].mean() if m.any() else 0.0
    k = int(np.argmax(prof[3:]) + 3)
    return float(n) / k, prof, k


def main():
    ours = Dds(sys.argv[1])
    tag = sys.argv[2] if len(sys.argv) > 2 else 'maxz'
    van = Dds(seam.VAN)
    pz = np.load(os.path.join(HERE, 'proj_m20_20.npz'))
    idbuf, ramp = pz['idbuf'], pz['ramped']
    road = idbuf >= 0
    diff, feath, solid = seam.boundaries(idbuf, ramp)

    cache = os.path.join(HERE, 'uvperiod.json')
    if os.path.isfile(cache):
        per = json.load(open(cache))
    else:
        per = uv_world_period()
        json.dump(per, open(cache, 'w'), indent=1)
    wp = np.array([p['world_per_repeat'] for p in per])
    print('UV repeat measured on %d road shapes: median %.1f world units, '
          'quartiles %.1f / %.1f' % (len(wp), np.median(wp),
                                     np.percentile(wp, 25), np.percentile(wp, 75)))
    print('   = %.2f bake texels a repeat at %.0f units a texel'
          % (np.median(wp) / UPT, UPT))

    print('')
    print('DOMINANT PERIOD of the road region (radial power peak)')
    for name, d in (('vanilla', van), (tag, ours)):
        p, prof, k = dominant_period(d.lum(), road)
        print('   %-8s peak at radial bin %3d -> period %6.2f texels = %7.1f world units'
              % (name, k, p, p * UPT))

    print('')
    print('S-B: do the bands land on piece boundaries?')
    # crests = local maxima of our luminance along the road, high-pass filtered
    L = ours.lum()
    hp = L - np.array([[L[max(0, j - 2):j + 3, max(0, i - 2):i + 3].mean()
                        for i in range(512)] for j in range(512)])
    crest = road & (hp > np.percentile(hp[road], 90))
    near = np.zeros_like(crest)
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            near |= np.roll(np.roll(diff, dj, axis=0), di, axis=1)
    print('   crest texels %d; within one texel of a piece boundary: %d (%.1f%%)'
          % (int(crest.sum()), int((crest & near).sum()),
             100.0 * (crest & near).sum() / max(crest.sum(), 1)))
    base = road & ~crest
    print('   control -- the same fraction for NON-crest road texels: %.1f%%'
          % (100.0 * (base & near).sum() / max(base.sum(), 1)))

    print('')
    print('band amplitude, ours vs vanilla, on the road only')
    for name, d in (('vanilla', van), (tag, ours)):
        LL = d.lum()
        loc = np.array([[LL[max(0, j - 2):j + 3, max(0, i - 2):i + 3].std()
                         for i in range(512)] for j in range(512)])
        print('   %-8s local 5x5 SD on the road %6.3f ; off the road %6.3f'
              % (name, loc[road].mean(), loc[~road].mean()))


if __name__ == '__main__':
    main()
