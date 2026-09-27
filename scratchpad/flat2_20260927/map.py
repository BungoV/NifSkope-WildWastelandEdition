#!/usr/bin/env python
"""FLAT2 picture 1: which Commonwealth tiles store a sheet as one value, by sheet kind.

usage: map.py <installed.npz> <any Commonwealth .VT.<dim>.lodt> <dim> <out.png>
The npz is measure.py's (per tile: -1 absent, 0 not one value, 1 within 1 level, 2 one value).
One palette dict paints the tiles AND the legend; after drawing, every legend colour's pixel count
in the map area is measured back from the image and must equal its tile count x cell area.
"""
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

npzp, lodtp, dim, out = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
Z = np.load(npzp)
kinds = ['colour', 'msn', 'mask', 'height']
cls = {k: Z['Commonwealth.%d.%s' % (dim, k)] for k in kinds if 'Commonwealth.%d.%s' % (dim, k) in Z}
ty_, tx_ = next(iter(cls.values())).shape
with open(lodtp, 'rb') as f:
	h = f.read(256)
south, west, north, east = struct.unpack_from('<4h', h, 0x58)
tilesX, tilesY = struct.unpack_from('<2H', h, 0x6E)
assert (tilesY, tilesX) == (ty_, tx_), (tilesY, tilesX, ty_, tx_)

# ONE palette: tiles and legend both read it
PAL = {
	'absent': (24, 24, 28),
	'no sheet stored as one value': (92, 92, 92),
	'normal (msn) only': (70, 150, 235),
	'height only': (235, 170, 40),
	'normal and height': (215, 60, 190),
	'colour or mask': (40, 220, 90),
}
present = cls['colour'] >= 0
one = {k: cls[k] == 2 for k in cls}
cat = np.full((tilesY, tilesX), 'absent', dtype=object)
cat[present] = 'no sheet stored as one value'
cm = one.get('colour', False) | one.get('mask', False)
cat[present & one['msn'] & ~one['height']] = 'normal (msn) only'
cat[present & one['height'] & ~one['msn']] = 'height only'
cat[present & one['msn'] & one['height']] = 'normal and height'
cat[present & cm] = 'colour or mask'

C = 9                      # pixels a tile
BAR = 60
LEG = 330
W = tilesX * C + LEG + 30
H = BAR + tilesY * C + 20
im = Image.new('RGB', (W, H), (16, 16, 18))
d = ImageDraw.Draw(im)
X0, Y0 = 10, BAR + 10
for y in range(tilesY):
	for x in range(tilesX):
		d.rectangle([X0 + x * C, Y0 + y * C, X0 + x * C + C - 1, Y0 + y * C + C - 1], fill=PAL[cat[y, x]])
# the Boston test box (cells -8 -12 .. 3 -1), outlined
bx0 = (-8 - west) // dim
bx1 = (3 - west) // dim
by0 = (north - (-1)) // dim
by1 = (north - (-12)) // dim
d.rectangle([X0 + bx0 * C - 1, Y0 + by0 * C - 1, X0 + (bx1 + 1) * C, Y0 + (by1 + 1) * C], outline=(255, 255, 255), width=2)
try:
	f1 = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
	f2 = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 17)
except OSError:
	f1 = f2 = ImageFont.load_default()
d.rectangle([0, 0, W, BAR - 1], fill=(0, 0, 0))
d.text((12, 4), 'Commonwealth terrain tiles whose sheets are one value (stored as 16 bytes)', font=f1, fill=(255, 255, 255))
d.text((12, 36), 'finest level (%d x %d cells a tile, %d x %d tiles), installed 09-25 bake, measured read-only; north up'
	   % (dim, dim, tilesX, tilesY), font=f2, fill=(200, 200, 200))
lx = X0 + tilesX * C + 20
ly = Y0
counts = {k: int((cat == k).sum()) for k in PAL}
for k, rgb in PAL.items():
	d.rectangle([lx, ly, lx + 22, ly + 22], fill=rgb)
	d.text((lx + 30, ly), '%s: %d' % (k, counts[k]), font=f2, fill=(230, 230, 230))
	ly += 32
d.rectangle([lx, ly + 4, lx + 22, ly + 26], outline=(255, 255, 255), width=2)
d.text((lx + 30, ly + 4), 'the Boston test box (-8,-12 .. 3,-1)', font=f2, fill=(230, 230, 230))
ly += 44
for k in kinds:
	d.text((lx, ly), '%s sheets one value: %d of %d' % (k, int(one[k].sum()), int(present.sum())), font=f2, fill=(200, 200, 200))
	ly += 24
im.save(out)
# measured back from the saved picture: each legend colour's pixels inside the map area
a = np.asarray(Image.open(out).convert('RGB'))[Y0:Y0 + tilesY * C, X0:X0 + tilesX * C].reshape(-1, 3)
bad = 0
for k, rgb in PAL.items():
	n = int((a == np.array(rgb)).all(1).sum())
	# the box outline covers some tile pixels, so the picture holds AT MOST the tiles' area
	ok = n <= counts[k] * C * C and (counts[k] == 0) == (n == 0)
	bad += not ok
	print('legend %-30s rgb %-15s tiles %5d  pixels %7d  %s' % (k, rgb, counts[k], n, 'ok' if ok else 'MISMATCH'))
print('legend check: %s' % ('PASS' if bad == 0 else 'FAIL'))
