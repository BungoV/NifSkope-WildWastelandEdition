# IMPOSTORLOOK1 section 3 -- WHAT SHAPE ARE THE LUMPS?
#
# "The card's outline is LUMPY where the mesh's is smooth." Three candidates,
# told apart by the SIZE OF ONE STEP and not by how rough the edge is:
#
#   frame resolution  the edge only ever lands on SHEET TEXEL boundaries
#   a BC block        it only lands on every FOURTH one (DXT blocks are 4x4 and
#                     the alpha inside a block is one interpolated ramp)
#   parallax tearing  it lands anywhere, and it vanishes at a bake direction
#                     where the parallax step is a provable no-op
#
# THE INSTRUMENT. Take the trunk's left/right edge column for each row of the
# bottom third, divide by the width of one sheet texel on screen, and measure
# how far it sits from the nearest WHOLE texel -- then from the nearest FOURTH.
# The texel grid's phase on screen is not known, so the phase is searched and
# the BEST it can do is reported. The MESH grab is the control: it goes through
# the identical procedure, and a mesh silhouette has no sheet texels in it, so
# whatever the mesh scores is what "not quantised" looks like.
#   perfectly quantised -> mean distance 0.00      (0.25 = uniform, i.e. none)
#
# WHY NOT A STANDARD DEVIATION. It is the WRONG instrument and gives the
# opposite verdict on blast_n4: the card's edge sd there is 1.84 texels against
# the mesh's 2.33, i.e. the card measures SMOOTHER -- because the mesh's
# deviation is real bark and real taper. Roughness is not the question.
#
# These grabs are at BAKE directions, so any step seen here is not tearing.
# OFFLINE.
import numpy as np
from PIL import Image
SC = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad'
BG = np.array([43, 45, 49], float)

def edge(p, texels_across, side):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    k = np.abs(a-BG).sum(-1) > 12
    ys, xs = np.nonzero(k)
    y0, y1 = ys.min(), ys.max()
    lo = int(y0+(y1-y0)*0.66); hi = int(y0+(y1-y0)*0.97)
    ex, wid = [], []
    for y in range(lo, hi):
        r = np.nonzero(k[y])[0]
        if len(r) < 3: continue
        ex.append(r.min() if side == 'L' else r.max())
        wid.append(r.max()-r.min()+1)
    return np.array(ex, float), float(np.median(wid))/texels_across

def quant(ex, ppt, mult):
    """best mean distance to a grid of `mult` sheet texels, phase searched"""
    u = ex/(ppt*mult)
    best = 1.0
    for ph in np.linspace(0, 1, 101):
        d = np.abs((u+ph) - np.round(u+ph)).mean()
        best = min(best, d)
    return best

BAKE = {'blast_n4': (SC+'/impostorfix5_20260919/control/blast_n4_bake_ctl', 48),
        'maple_n4': (SC+'/impostorfix5_20260919/control/maple_n4_bake_on', 32)}
print('distance of the trunk edge from a texel grid (0.00 = locked to it, 0.25 = ignores it)')
print('%-9s %-4s %-5s %9s %9s %9s %7s' % ('subject', 'edge', 'what', 'to 1 texel', 'to 4 texels', 'to 1 px', 'px/tex'))
for tag, (folder, tex) in BAKE.items():
    for side in ('L', 'R'):
        for kind in ('mesh', 'card'):
            ex, ppt = edge('%s/v_az180_el00_%s.png' % (folder, kind), tex, side)
            print('%-9s %-4s %-5s %9.3f %11.3f %9.3f %7.2f' %
                  (tag, side, kind, quant(ex, ppt, 1), quant(ex, ppt, 4), quant(ex, 1.0, 1), ppt))
