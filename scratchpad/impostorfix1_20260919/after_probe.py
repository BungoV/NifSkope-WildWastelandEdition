# Two questions about the repaired sheets, both offline:
#  1. how much of the residual parallax error is BC3 quantisation of the height,
#     i.e. what the `_n` channel-swap proposal would be worth;
#  2. why maple_n4 barely moved, and whether relaxing the "fully covered" seed
#     threshold would move it.
import sys, math, glob
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919')
import numpy as np
import refcard
from bcdec import load_dds
from PIL import Image
from scipy.ndimage import distance_transform_edt

MINE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919/fixture/'
OLD = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture/'

print('== 1. the height channel after the repair: what BC3 still costs ==')
for tag, span in (('blast_n4', 3072.0), ('maple_n4', 3072.0), ('dead_n4', 3072.0)):
    d = glob.glob(MINE + tag + '/cards/*_oct_d.DDS')[0]
    n = glob.glob(MINE + tag + '/cards/*_oct_n.DDS')[0]
    alb = load_dds(d)[0]
    nrm = load_dds(n)[0]
    png = np.asarray(Image.open(glob.glob(MINE + tag + '/cards/*_oct_normal.png')[0])
                     .convert('RGBA')).astype(float)
    a = alb[..., 3] * 255.0
    h = nrm[..., 2] * 255.0
    full = a >= 250
    if full.sum() == 0:
        print('%-10s no fully covered texel' % tag)
        continue
    # the sheet the repair produced, before BC3, is not on disk -- but on FULLY
    # COVERED texels the repair leaves the height alone, so the PNG is its twin
    # there and the difference is BC3 and nothing else.
    err = np.abs(h - png[..., 2])[full]
    print('%-10s BC3 height error on whole texels: mean %5.1f lv = %4.0f u, p95 %5.1f lv = %4.0f u, max %3.0f lv = %4.0f u'
          % (tag, err.mean(), err.mean() / 255 * span, np.percentile(err, 95),
             np.percentile(err, 95) / 255 * span, err.max(), err.max() / 255 * span))

print()
print('== 2. maple_n4: how many texels the seed threshold actually finds ==')
for tag in ('blast_n4', 'maple_n4', 'dead_n4', 'rock_n4'):
    d = glob.glob(MINE + tag + '/cards/*_oct_d.DDS')[0]
    a = load_dds(d)[0][..., 3] * 255.0
    tot = a.size
    for thr in (250, 224, 192, 160):
        pass
    print('%-10s texels a>=250 %6d   a>=224 %6d   a>=192 %6d   a>=160(any cov) %6d   of %d'
          % (tag, (a >= 250).sum(), (a >= 224).sum(), (a >= 192).sum(), (a >= 160).sum(), tot))

print()
print('== 3. maple_n4: does a relaxed seed threshold help? (numpy reference, 24 views) ==')
refcard.USE_DDS = True
refcard.FX = MINE
from refcard import CardSet, render, truth, iou, coverageOf
cs = CardSet('maple_n4')
refcard.FX = OLD
cs12 = CardSet('blast_n12')          # the subject differs; only used as a stand-in? no --
refcard.FX = MINE
# maple's own truth: there is no N=12 maple set, so score against the N=4 set's
# OWN nearest frame is meaningless. Use the SHEET-LEVEL statistic instead.
cov = coverageOf(cs.alb[..., 3], cs)
fw, fh, N = cs.fw, cs.fh, cs.N
for thr in (250, 224, 192, 160):
    seeds = 0
    for j in range(N):
        for i in range(N):
            c = cov[j * fh:(j + 1) * fh, i * fw:(i + 1) * fw]
            seeds += (c >= thr / 255.0).sum()
    print('  seed threshold %3d -> %6d seed texels over the whole sheet' % (thr, seeds))
