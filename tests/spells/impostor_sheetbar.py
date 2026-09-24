# impostor_sheetbar.py -- the depth sheet bar of tests/spells/impostor_trunk.sh.
# Lane IMPOSTORDEPTH2, 2026-09-23.
#
# usage: python impostor_sheetbar.py <cards dir>        (one line, first word SHEET PASS|FAIL)
#
# Reads <id>_oct_n.DDS with Pillow (a decoder independent of the tree's encoder) and
# measures its HEIGHT (B) against the image lodgen was given to encode: the bake's
# <id>_oct_normal.png after lodgenRepairOctHeight, reconstructed by
# impostor_height_ref.py's port, which is believed only when its four-integer census
# equals the one the bake printed into a log beside the sheets (else: SHEET FAIL
# REFUSED). The raw PNG numbers are printed beside them; the gap between the two is
# the repair, not the codec.
#
# THE BAR, over the covered texels (albedo alpha >= 16, the coverage floor):
#   the sheet is DX10 BC7 (DXGI 98); height |err| mean < 1.0 level and p95 <= 3 levels;
#   at least 90% of the heights the repaired image holds survive the decode.
# Measured 2026-09-23 on the 8x8 maple (000531b3, 1920x2048): BC7 0.57 / 2 / 57 of 58;
# the DXT5 sheet of the same bake 2.64 / 7 / 27 of 58 (the red control).
import sys, os, glob, json, numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from impostor_height_ref import _repair, _census_from_logs


def stats(ref, dec, cov):
    ph, dh = ref[cov], dec[cov]
    e = np.abs(dh - ph)
    pv = set(np.unique(ph).tolist())
    return float(e.mean()), int(np.percentile(e, 95)), int(e.max()), len(pv & set(np.unique(dh).tolist())), len(pv)


d = os.path.abspath(sys.argv[1])
ns = glob.glob(os.path.join(d, '*_oct_n.DDS')) + glob.glob(os.path.join(d, '*_oct_n.dds'))
if not ns:
    print('SHEET FAIL no _oct_n.DDS in %s' % d); sys.exit()
dds = ns[0]; fid = os.path.basename(dds)[:-len('_oct_n.DDS')]
hdr = open(dds, 'rb').read(148)
fmt = 'DX10 dxgi %d' % int.from_bytes(hdr[128:132], 'little') if hdr[84:88] == b'DX10' else hdr[84:88].decode('latin1')
raw = open(os.path.join(d, fid + '_oct.lodm'), 'rb').read()
fw, fh = json.JSONDecoder().raw_decode(raw[raw.index(b'{'):].decode('utf-8', 'replace'))[0]['card']['frame']
nrm = np.asarray(Image.open(os.path.join(d, fid + '_oct_normal.png')).convert('RGBA'))
alb = np.asarray(Image.open(os.path.join(d, fid + '_oct_albedo.png')).convert('RGBA'))
cens = _census_from_logs(d)
ref, got = _repair(nrm[..., 2], alb[..., 3], fw, fh, 16)
if got not in cens:
    print('SHEET FAIL REFUSED: the repair port gives census %s, the bake logs beside %s say %s' % (got, d, cens)); sys.exit()
dec = np.asarray(Image.open(dds).convert('RGBA'))[..., 2].astype(np.int32)
cov = alb[..., 3] >= 16
m, p95, mx, sv, nv = stats(ref.astype(np.int32), dec, cov)
rm, rp, _, rs, rn = stats(nrm[..., 2].astype(np.int32), dec, cov)
ok = fmt == 'DX10 dxgi 98' and m < 1.0 and p95 <= 3 and sv >= 0.9 * nv
print('SHEET %s %s: height |err| vs the repaired input mean %.2f levels p95 %d max %d, heights surviving %d of %d'
      ' | vs the raw bake PNG mean %.2f p95 %d, %d of %d | repair census %s matches the bake | file %d B' % (
      'PASS' if ok else 'FAIL', fmt, m, p95, mx, sv, nv, rm, rp, rs, rn, '/'.join(map(str, got)), os.path.getsize(dds)))
