# GPU1: decoded difference between the CPU and GPU card normal arrays (layer 0, top mip, as Pillow reads it).
# usage: python dds_diff.py <bake A> <bake B>
import glob, os, sys
from PIL import Image, ImageChops

a_root, b_root = sys.argv[1], sys.argv[2]
rel = 'mod/FO4CSLOD/Commonwealth/Objects'
tot_px = 0
tot_same = 0
worst = 0
for fa in sorted(glob.glob(os.path.join(a_root, rel, '*Cards*_n.DDS'))):
    fb = os.path.join(b_root, rel, os.path.basename(fa))
    ia = Image.open(fa).convert('RGBA')
    ib = Image.open(fb).convert('RGBA')
    d = ImageChops.difference(ia, ib)
    px = ia.size[0] * ia.size[1]
    hist = d.convert('L').histogram()
    same = hist[0]
    mx = max(ch[1] for ch in d.getextrema())
    mean = [sum(i * n for i, n in enumerate(d.getchannel(c).histogram())) / px for c in range(4)]
    worst = max(worst, mx)
    tot_px += px
    tot_same += same
    print('%-50s %9d px  identical %6.2f%%  max |d| %3d  mean |d| R %.4f G %.4f B %.4f A %.4f' % (
        os.path.basename(fa), px, 100.0 * same / px, mx, *mean))
print('ALL: %d px, identical %.2f%%, worst |d| %d (8-bit levels)' % (tot_px, 100.0 * tot_same / tot_px, worst))
