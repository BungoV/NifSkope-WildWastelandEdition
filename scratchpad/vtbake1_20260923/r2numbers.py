#!/usr/bin/env python
"""Lane VTBAKE1 job 4: the numbers R2's clipmap design needs, read from the
baked containers' own headers (never typed): texel size per level in metres,
and the resident cost of an N-ring stack at 1024 and 2048 windows, per sheet
format. No ring count is chosen here.

usage: python r2numbers.py <dir with Commonwealth.VT.<dim>.lodt>
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vtread import Lodt

UNIT_M = 0.9144 / 64.0
MIB = 1024.0 * 1024.0
BPP = [('BC1 (dxgi 71/72), colour / msn / mask without cover', 0.5),
	   ('BC3 (dxgi 77/78), mask WITH cover', 1.0),
	   ('R16_UNORM (dxgi 56), height', 2.0),
	   ('R8G8B8A8 uncompressed (if a ring is kept decoded)', 4.0)]


def main():
	d = sys.argv[1]
	levels = []
	for dim in (2, 4, 8, 16, 32):
		v = Lodt(os.path.join(d, 'Commonwealth.VT.%d.lodt' % dim))
		upt = v.levelDim * 4096 / v.content
		span = (v.wEast - v.wWest + 1) * 4096
		levels.append((dim, upt, v.content, v.tilesX, v.tilesY, span,
					   [(v.sheets[s]['role'], v.sheets[s]['dxgi'], v.sheets[s]['dxgiCover']) for s in range(v.sheetCount)],
					   v.rawBytes(False), v.rawBytes(True)))
	print('## Texel size per level (from each container header)')
	print('| level dim | units/texel | metres/texel | tile content | tile covers | world span in texels |')
	print('|---|---|---|---|---|---|')
	for (dim, upt, c, tx, ty, span, sh, rb0, rb1) in levels:
		print('| %d | %g | %.4f | %d px | %d u = %.1f m | %d |' % (dim, upt, upt * UNIT_M, c, dim * 4096, dim * 4096 * UNIT_M, span / upt))
	print()
	print('sheets in the bake: %s' % levels[0][6])
	print('tile raw bytes: no cover %d, cover %d' % (levels[0][7], levels[0][8]))
	print()
	print('## One ring, one sheet: a W x W window, mip 0 only')
	print('| format | bytes/texel | W=1024 | W=2048 |')
	print('|---|---|---|---|')
	for name, b in BPP:
		print('| %s | %g | %.2f MiB | %.2f MiB |' % (name, b, 1024 * 1024 * b / MIB, 2048 * 2048 * b / MIB))
	print()
	print('A ring with one extra mip for trilinear costs x1.25 of the figure above.')
	print()
	stacks = [('colour+msn+mask BC1 (the bake as written, no height)', 1.5),
			  ('colour+msn+mask BC1 + height R16 (this bake: --vt-height, no cover)', 3.5),
			  ('colour+msn BC1 + mask BC3 + height R16 (a --cover bake)', 4.0),
			  ('colour+msn+mask BC1 + height R16, rings kept as RGBA8 except height', 14.0)]
	print('## N-ring stack, per-ring sheet set, mip 0 only, MiB')
	hdr = '| sheet set | B/texel/ring | ' + ' | '.join('N=%d W=%d' % (n, w) for w in (1024, 2048) for n in range(1, 6)) + ' |'
	print(hdr)
	print('|' + '---|' * (2 + 10))
	for name, b in stacks:
		cells = []
		for w in (1024, 2048):
			for n in range(1, 6):
				cells.append('%.1f' % (n * w * w * b / MIB))
		print('| %s | %g | %s |' % (name, b, ' | '.join(cells)))
	print()
	print('## Window reach per level (a ring window centred on the camera)')
	print('| level dim | W=1024 covers | half-width (m) | W=2048 covers | half-width (m) | whole world fits at W=2048? |')
	print('|---|---|---|---|---|---|')
	for (dim, upt, c, tx, ty, span, sh, rb0, rb1) in levels:
		for_w = []
		for w in (1024, 2048):
			ext = w * upt
			for_w.append('%d u = %.0f m' % (ext, ext * UNIT_M))
			for_w.append('%.0f' % (ext * UNIT_M / 2))
		fits = 'yes' if 2048 * upt >= span else 'no (%.0f%% of span)' % (100.0 * 2048 * upt / span)
		print('| %d | %s | %s | %s | %s | %s |' % (dim, for_w[0], for_w[1], for_w[2], for_w[3], fits))
	print()
	print('## Tile cache needed to FILL one ring from the pyramid (worst case, window not tile-aligned)')
	print('| W | tiles per ring (ceil(W/content)+1)^2 | bytes, no cover | bytes, cover |')
	print('|---|---|---|---|')
	c = levels[0][2]
	for w in (1024, 2048):
		n = (-(-w // c) + 1) ** 2
		print('| %d | %d | %.1f MiB | %.1f MiB |' % (w, n, n * levels[0][7] / MIB, n * levels[0][8] / MIB))


if __name__ == '__main__':
	main()
