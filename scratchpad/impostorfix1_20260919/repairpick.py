# Rank the candidate BAKE repairs for the height channel, in the numpy reference,
# on the same 24 orbit views the viewer measures. The sheet under test is the
# shipped BC3 .DDS -- the bytes the viewer actually samples -- with the repair
# applied to its height channel, so the ranking is of the REPAIR and not of an
# imagined uncompressed sheet.
import sys, math
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919')
import numpy as np
import refcard
refcard.USE_DDS = True
from refcard import CardSet, render, truth, iou, coverageOf
from scipy.ndimage import distance_transform_edt

cs = CardSet('blast_n4')
cs12 = CardSet('blast_n12')

fw, fh, N = cs.fw, cs.fh, cs.N
cov = coverageOf(cs.alb[..., 3], cs)


def perframe(fn):
    """Apply fn(hFrame, covFrame) -> hFrame over each frame of the sheet."""
    h = cs.nrm[..., 2].copy()
    for j in range(N):
        for i in range(N):
            ys, xs = slice(j * fh, (j + 1) * fh), slice(i * fw, (i + 1) * fw)
            h[ys, xs] = fn(h[ys, xs], cov[ys, xs])
    return h


def plane_everywhere_but_full(h, c):
    return np.where(c >= 250 / 255.0, h, 0.5)


def dilate_from_full(h, c):
    full = c >= 250 / 255.0
    if not full.any():
        return np.full_like(h, 0.5)
    _, idx = distance_transform_edt(~full, return_indices=True)
    return h[idx[0], idx[1]]


def dilate_then_plane_bg(h, c):
    out = dilate_from_full(h, c)
    return np.where(c > 0.0, out, 0.5)


def plane_bg_only(h, c):
    return np.where(c > 0.0, h, 0.5)


CANDS = [
    ('as shipped', None, None),
    ('R1 card plane on every non-full texel', plane_everywhere_but_full, None),
    ('R2 dilate height from fully covered', dilate_from_full, None),
    ('R3 dilate, then card plane outside coverage', dilate_then_plane_bg, None),
    ('R4 card plane OUTSIDE COVERAGE only', plane_bg_only, None),
]
# the depthSpan fit, applied on top of the winner: the reference is told a smaller
# span, which is what a re-bake with a fitted span would encode.
FITS = [None, 1024.0, 590.0]

RES = (192, 512)
views = [(az, el) for el in (15.0, 45.0) for az in range(0, 360, 30)]
dirs = []
for az, el in views:
    a, e = math.radians(az), math.radians(el)
    dirs.append(np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)]))

T = [truth(cs12, d, RES, cs.half)[0] for d in dirs]
nrm0 = cs.nrm.copy()
span0 = cs.span

print('%-44s %8s %8s %8s' % ('candidate', 'span3072', 'span1024', 'span590'))
for name, fn, _ in CANDS:
    cs.nrm = nrm0.copy()
    if fn is not None:
        cs.nrm[..., 2] = perframe(fn)
    row = []
    for fit in FITS:
        cs.span = span0 if fit is None else fit
        if fit is not None and fn is None and fit != span0:
            pass
        s = 0.0
        for d, t in zip(dirs, T):
            m, _ = render(cs, d, RES, parallax=True, nframes=3)
            s += iou(m, t)
        row.append(s / len(dirs))
    print('%-44s %8.4f %8.4f %8.4f' % (name, row[0], row[1], row[2]))
cs.nrm = nrm0
cs.span = span0
