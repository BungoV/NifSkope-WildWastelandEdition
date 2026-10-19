"""Judges tests/spells/volair1_fogblend.sh (lane VOLAIR1, option B = WW_CELL_VOLAIR_FOGBLEND=1, test only).

  E  b0 (FOGBLEND=0) is a byte for byte (option B off is option A)
  D  b1 differs from a only where the geo probe's d >= the fog start - 64 (fogK0 z / x from b1's dump), and in at
     least 500 pixels past it
A red control (VOLAIR1_RED set) passes only when a stage FAILS."""
import os
import sys

import numpy as np
from PIL import Image

out = sys.argv[1]
red = os.environ.get('VOLAIR1_RED', '')


def img(n):
	return np.asarray(Image.open(os.path.join(out, n)).convert('RGB')).astype(np.int32)


ok = True
a, b0, b1, geo = img('a.png'), img('b0.png'), img('b1.png'), img('geo.png')
same = int((a != b0).any(axis=2).sum())
e = same == 0
print('E  b0 vs a: %d px differ  %s' % (same, 'PASS' if e else 'FAIL'))
ok &= e
k = {}
for line in open(os.path.join(out, 'b1.bin.txt')):
	p = line.split()
	if p and p[0] == 'fogK0':
		k = [float(x) for x in p[1:]]
start = k[2] / k[0] if k and k[0] > 0 else float('nan')
fogged = (geo[..., 2] == 0) & (geo[..., 0] > 0)
d = geo[..., 0] / 255.0 * 16384.0
diff = (np.abs(a - b1) > 1).any(axis=2)
near = int((diff & fogged & (d < start - 64)).sum())
far = int((diff & fogged & (d >= start - 64)).sum())
unk = int((diff & ~fogged).sum())
dd = near == 0 and far >= 500
print('D  fog start %.1f: b1 differs near %d px, past it %d px (of %d fogged; %d off the probe)  %s'
	% (start, near, far, int(fogged.sum()), unk, 'PASS' if dd else 'FAIL'))
ok &= dd
if red:
	print('VERDICT red %s: %s' % (red, 'FAILED as it must' if not ok else 'PASSED (the gate is blind)'))
	sys.exit(0 if not ok else 1)
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
