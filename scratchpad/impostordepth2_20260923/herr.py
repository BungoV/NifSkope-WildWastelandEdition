# herr.py -- height (B of _n) decode error of a card `_n` DDS against the bake's own PNG.
# Lane IMPOSTORDEPTH2. The decoder is Pillow's (independent of the tree's encoder).
# usage: python herr.py <cards dir> [<id>]   -> one line per sheet
# Covered = the bake's albedo alpha >= 16 (the coverage floor, raw fraction in the PNG),
# which is the 1,037,765-texel set IMPOSTORDEPTH1 measured on.
import sys, os, numpy as np
from PIL import Image

d = sys.argv[1]
fid = sys.argv[2] if len(sys.argv) > 2 else '000531b3'
png = np.asarray(Image.open(os.environ.get('REF') or os.path.join(d, fid + '_oct_normal.png')).convert('RGBA')).astype(np.int32)
alb = np.asarray(Image.open(os.path.join(d, fid + '_oct_albedo.png')).convert('RGBA')).astype(np.int32)
dds_path = os.path.join(d, fid + '_oct_n.DDS')
im = Image.open(dds_path)
dec = np.asarray(im.convert('RGBA')).astype(np.int32)
with open(dds_path, 'rb') as f:
    hdr = f.read(148)
four = hdr[84:88]
fmt = four.decode('latin1')
if four == b'DX10':
    fmt = 'DX10 dxgi %d' % int.from_bytes(hdr[128:132], 'little')
cov = alb[..., 3] >= 16
ph, dh = png[..., 2][cov], dec[..., 2][cov]
e = np.abs(dh - ph)
pv = set(np.unique(ph).tolist())
dv = set(np.unique(dh).tolist())
surv = len(pv & dv)
def ch(c):
    ee = np.abs(dec[..., c][cov] - png[..., c][cov])
    return '%.2f/%d' % (ee.mean(), int(np.percentile(ee, 95)))
print('%s %s: covered %d  height |err| mean %.2f levels (%.1f units) p95 %d max %d  heights surviving %d of %d (decoded distinct %d)  R %s G %s A %s  file %d B' % (
    os.path.basename(d.rstrip('/\\')), fmt, int(cov.sum()), e.mean(), e.mean() * 12.0,
    int(np.percentile(e, 95)), int(e.max()), surv, len(pv), len(dv),
    ch(0), ch(1), ch(3), os.path.getsize(dds_path)))
