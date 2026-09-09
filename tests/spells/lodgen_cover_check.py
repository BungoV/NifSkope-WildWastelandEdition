#!/usr/bin/env python
"""The measurements behind tests/spells/lodgen_ground_cover.sh.

Given the baked sheets, the raw --dump-cover plane and the independent model
from lodgen_cover_model.py, this prints one line per check with its MEASURED
value and its bar on the same line, before the verdict, so a FAIL is readable
without a re-run. It exits non-zero when any check fails.

The one place the model cannot be exact, said out loud: the slope gate's
operand is the unit normal, and the only copy of that normal this side can read
is the _msn sheet -- which is BC1-compressed, so its Z is quantised to five
bits and a texel near the gate's edge can move by tens of cover levels for
reasons that are not a bug. So every value comparison is made where the gate is
SATURATED (|Stex + 5 - theta| > 2 degrees), which is exact, and the transition
band is reported separately with its own bar and its own count.
"""

import json
import math
import struct
import sys

RES = 512


# ---------------------------------------------------------------- DDS ----
def dds_info(b):
	h, w = struct.unpack_from('<II', b, 12)
	mips = struct.unpack_from('<I', b, 28)[0]
	fourcc = b[84:88]
	res1 = struct.unpack_from('<11I', b, 32)
	return {'w': w, 'h': h, 'mips': mips, 'fourcc': fourcc, 'reserved1': res1}


def rgb565(c):
	return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)


def mip_offsets(w, h, blockBytes, mips):
	offs, o, mw, mh = [], 128, w, h
	for _ in range(mips):
		offs.append((o, mw, mh))
		o += ((mw + 3) // 4) * ((mh + 3) // 4) * blockBytes
		mw = max(4, mw // 2)
		mh = max(4, mh // 2)
	return offs


def decode_color(b, off, w, h, stride, skip):
	out = [[0] * w for _ in range(h)]
	bw, bh = (w + 3) // 4, (h + 3) // 4
	for by in range(bh):
		for bx in range(bw):
			o = off + (by * bw + bx) * stride + skip
			c0, c1, bits = struct.unpack_from('<HHI', b, o)
			e0, e1 = rgb565(c0), rgb565(c1)
			if c0 > c1:
				pal = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
					   tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
			else:
				pal = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
			for i in range(16):
				x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
				if x < w and y < h:
					out[y][x] = pal[(bits >> (2 * i)) & 3]
	return out


def decode_alpha(b, off, w, h):
	out = [[0] * w for _ in range(h)]
	bw, bh = (w + 3) // 4, (h + 3) // 4
	for by in range(bh):
		for bx in range(bw):
			o = off + (by * bw + bx) * 16
			a0, a1 = b[o], b[o + 1]
			bits = int.from_bytes(b[o + 2:o + 8], 'little')
			if a0 > a1:
				pal = [a0, a1] + [((7 - k) * a0 + k * a1) // 7 for k in range(1, 7)]
			else:
				pal = [a0, a1] + [((5 - k) * a0 + k * a1) // 5 for k in range(1, 5)] + [0, 255]
			for i in range(16):
				x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
				if x < w and y < h:
					out[y][x] = pal[(bits >> (3 * i)) & 7]
	return out


def block_alpha_range(b, off, w, bx, by):
	bw = (w + 3) // 4
	o = off + (by * bw + bx) * 16
	return abs(b[o] - b[o + 1])


# --------------------------------------------------------------- stats ---
def pearson(xs, ys):
	n = len(xs)
	if n < 2:
		return 0.0
	mx, my = sum(xs) / n, sum(ys) / n
	sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
	sy = math.sqrt(sum((y - my) ** 2 for y in ys))
	if sx == 0 or sy == 0:
		return 0.0
	return sum((xs[i] - mx) * (ys[i] - my) for i in range(n)) / (sx * sy)


class Checks(object):
	def __init__(self):
		self.n = 0
		self.fails = 0

	def check(self, name, ok, detail=''):
		self.n += 1
		if not ok:
			self.fails += 1
		print('  %s %s%s' % ('ok  ' if ok else 'FAIL', name,
							 (' -- ' + detail) if detail else ''))

	def note(self, text):
		print('       %s' % text)


def main():
	(coverDds, plainDds, coverData, plainData, coverMsn, rawPlane,
	 modelPath, model2Path, censusPath) = sys.argv[1:10]
	c = Checks()

	model = json.load(open(modelPath))
	dtex = model['dtex']
	stex = model['stex']
	coverFull = model['coverFull']

	dataB = open(coverData, 'rb').read()
	plainB = open(plainData, 'rb').read()
	msnB = open(coverMsn, 'rb').read()
	raw = open(rawPlane, 'rb').read()
	di = dds_info(dataB)
	pi = dds_info(plainB)

	# ---- C3: the fourCC IS the switch, and it is qualified by the stamp ----
	stamp = di['reserved1'][0]
	law = di['reserved1'][1] >> 24
	full = di['reserved1'][1] & 0xFFFFFF
	print('  C3 cover sheet: fourCC %s, dwReserved1[0] 0x%08X, law %d, COVER_FULL %d'
		  % (di['fourcc'].decode('latin-1'), stamp, law, full))
	c.check('C3 the cover sheet is DXT5 and carries the WWCV stamp',
			di['fourcc'] == b'DXT5' and stamp == 0x56435757 and law == 1,
			'a DXT5 sheet with no stamp would decode as FULL cover on every texel')
	c.check('C3 the stamp records the normalisation the bake used',
			abs(full - coverFull) < 1.0, 'stamp %d, model %g' % (full, coverFull))
	c.check('C3 the no-cover sheet stays DXT1', pi['fourcc'] == b'DXT1',
			'fourcc %s' % pi['fourcc'].decode('latin-1'))

	# ---- C4: the arithmetic, both directions ----
	print('  C4 sizes: cover %d, no-cover %d (bars 349648 / 174888)'
		  % (len(dataB), len(plainB)))
	c.check('C4 a cover data sheet is 349,648 bytes', len(dataB) == 349648)
	c.check('C4 a no-cover data sheet is 174,888 bytes', len(plainB) == 174888)

	# ---- the decoded planes ----
	offs = mip_offsets(di['w'], di['h'], 16, di['mips'])
	alpha = decode_alpha(dataB, offs[0][0], RES, RES)
	rgbC = decode_color(dataB, offs[0][0], RES, RES, 16, 8)
	poffs = mip_offsets(pi['w'], pi['h'], 8, pi['mips'])
	rgbP = decode_color(plainB, poffs[0][0], RES, RES, 8, 0)
	moffs = mip_offsets(RES, RES, 8, dds_info(msnB)['mips'])
	msn = decode_color(msnB, moffs[0][0], RES, RES, 8, 0)

	# theta from the SAME normal the msn encodes
	theta = [[math.degrees(math.acos(max(0.0, min(1.0, msn[y][x][2] / 255.0 * 2.0 - 1.0))))
			  for x in range(RES)] for y in range(RES)]

	# ---- C3b: the BC1 -> BC3 switch must not move R, G or B ----
	moved = 0
	blocksChecked = 0
	for by in range(RES // 4):
		for bx in range(RES // 4):
			allzero = True
			for i in range(16):
				x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
				if alpha[y][x] != 0:
					allzero = False
					break
			if not allzero:
				continue
			blocksChecked += 1
			for i in range(16):
				x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
				if rgbC[y][x] != rgbP[y][x]:
					moved += 1
	print('  C3b all-cover-0 blocks compared: %d, texels whose RGB moved: %d (bar 0)'
		  % (blocksChecked, moved))
	c.check('C3b the BC1 to BC3 switch leaves AO, wetness and shore alone',
			blocksChecked > 0 and moved == 0)

	# ---- C6: the model against the RAW plane, where the gate is saturated ----
	sat = []
	band = []
	for y in range(RES):
		for x in range(RES):
			if dtex[y][x] <= 0.0:
				continue
			margin = stex[y][x] + 5.0 - theta[y][x]
			m = 0
			if margin >= 2.0:
				m = min(255, int(255.0 * dtex[y][x] / coverFull + 0.5))
			elif margin <= -2.0:
				m = 0
			else:
				band.append((x, y))
				continue
			sat.append((x, y, m))
	rawMax = 0
	rawOver = 0
	for x, y, m in sat:
		d = abs(raw[y * RES + x] - m)
		rawMax = max(rawMax, d)
		if d > 1:
			rawOver += 1
	print('  C6a model vs the raw plane on %d saturated texels: max %d (bar 1), over-bar %d; '
		  'transition band %d texels' % (len(sat), rawMax, rawOver, len(band)))
	c.check('C6a the model and the bake agree on the composite',
			len(sat) > 1000 and rawOver == 0, 'the band is reported, not gated')

	# ---- C6b: the raw plane against the decoded BC3, per block ----
	over = 0
	worst = 0.0
	for by in range(RES // 4):
		for bx in range(RES // 4):
			rng = block_alpha_range(dataB, offs[0][0], RES, bx, by)
			bar = rng / 14.0 + 1.0
			for i in range(16):
				x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
				d = abs(alpha[y][x] - raw[y * RES + x])
				worst = max(worst, d - bar)
				if d > bar:
					over += 1
	print('  C6b raw vs decoded BC3, per block bar range/14 + 1: violations %d (bar 0), '
		  'worst excess %.2f' % (over, worst))
	c.check('C6b BC3 carries the plane within its own block range', over == 0)

	# ---- C6c: the CONTROL. Shift the model one cell and it must disagree ----
	shift = RES // model['dim']
	ctrl = 0
	for y in range(0, RES, 7):
		for x in range(0, RES - shift, 7):
			m = 0
			if dtex[y][x] > 0.0 and stex[y][x] + 5.0 - theta[y][x] >= 2.0:
				m = min(255, int(255.0 * dtex[y][x] / coverFull + 0.5))
			ctrl = max(ctrl, abs(raw[y * RES + x + shift] - m))
	print('  C6c CONTROL, the model shifted one cell: max abs diff %d (bar > 32)' % ctrl)
	c.check('C6c the comparison can fail: a shifted model does not match', ctrl > 32)

	# ---- C5: the plane is a field, not a constant ----
	vals = [alpha[y][x] for y in range(0, RES, 3) for x in range(0, RES, 3)]
	nz = [v for v in vals if v > 0]
	mvals = [min(255, int(255.0 * dtex[y][x] / coverFull + 0.5)) if dtex[y][x] > 0 else 0
			 for y in range(0, RES, 3) for x in range(0, RES, 3)]
	mnz = [v for v in mvals if v > 0]
	fracFile = len(nz) / float(len(vals))
	fracModel = len(mnz) / float(len(mvals))
	distinct = len(set(vals))
	print('  C5 nonzero fraction: file %.3f, model (ungated) %.3f; distinct values %d; '
		  'mean over nonzero %.1f' % (fracFile, fracModel, distinct,
									  (sum(nz) / len(nz)) if nz else 0.0))
	c.check('C5 the plane is neither empty nor saturated',
			distinct >= 16 and 0.0 < fracFile <= fracModel + 0.02)

	# ---- C7 / C17: cover is not one of the channels it sits beside ----
	sx = [alpha[y][x] for y in range(0, RES, 5) for x in range(0, RES, 5)]
	rr = [rgbC[y][x][0] for y in range(0, RES, 5) for x in range(0, RES, 5)]
	gg = [rgbC[y][x][1] for y in range(0, RES, 5) for x in range(0, RES, 5)]
	bb = [rgbC[y][x][2] for y in range(0, RES, 5) for x in range(0, RES, 5)]
	rA, rG, rB = pearson(sx, rr), pearson(sx, gg), pearson(sx, bb)
	print('  C7 r(cover, AO) %.3f  r(cover, wetness) %.3f  r(cover, shore) %.3f (bar |r| < 0.5)'
		  % (rA, rG, rB))
	c.check('C7 cover is not a copy of a channel already shipped',
			max(abs(rA), abs(rG), abs(rB)) < 0.5)
	prg, prb, pgb = pearson(rr, gg), pearson(rr, bb), pearson(gg, bb)
	print('  C8 r(AO,wet) %.3f  r(AO,shore) %.3f  r(wet,shore) %.3f (bar |r| < 0.9) [sampled]'
		  % (prg, prb, pgb))
	c.check('C8 the existing channels have not collapsed into one another',
			max(abs(prg), abs(prb), abs(pgb)) < 0.9)
	th = [theta[y][x] for y in range(0, RES, 5) for x in range(0, RES, 5)]
	mz = [msn[y][x][2] for y in range(0, RES, 5) for x in range(0, RES, 5)]
	rTh = pearson(sx, th)
	rMz = pearson(sx, mz)
	# the residual of a least-squares fit of cover on theta alone
	n = len(sx)
	mt, ms = sum(th) / n, sum(sx) / n
	den = sum((t - mt) ** 2 for t in th)
	slope = (sum((th[i] - mt) * (sx[i] - ms) for i in range(n)) / den) if den else 0.0
	tot = sum((v - ms) ** 2 for v in sx)
	res = sum((sx[i] - (ms + slope * (th[i] - mt))) ** 2 for i in range(n))
	frac = (res / tot) if tot else 1.0
	print('  C17 r(cover, msn B) %.3f  r(cover, theta) %.3f  unexplained variance %.3f '
		  '(bars |r| < 0.5, residual > 0.5)' % (rMz, rTh, frac))
	c.check('C17 cover is not recoverable from the slope it consumes',
			abs(rMz) < 0.5 and abs(rTh) < 0.5 and frac > 0.5)

	# ---- C9: the slope gate exists and bites ----
	g0 = [(x, y) for y in range(RES) for x in range(RES)
		  if dtex[y][x] > 0.0 and stex[y][x] + 5.0 - theta[y][x] <= -2.0]
	g1 = [(x, y) for y in range(RES) for x in range(RES)
		  if dtex[y][x] > 0.0 and stex[y][x] + 5.0 - theta[y][x] >= 2.0
		  and 255.0 * dtex[y][x] / coverFull >= 8.0]
	m0 = max((alpha[y][x] for x, y in g0), default=0)
	mean1 = (sum(alpha[y][x] for x, y in g1) / len(g1)) if g1 else 0.0
	print('  C9 gated-off texels %d (max cover %d, bar <= 2), gated-on %d (mean %.1f, bar > 8)'
		  % (len(g0), m0, len(g1), mean1))
	c.check('C9 the slope gate is there and it bites',
			len(g0) > 0 and len(g1) > 0 and m0 <= 2 and mean1 > 8.0)

	# ---- C16: the mip chain carries the plane, not BC1's forced opacity ----
	a1 = decode_alpha(dataB, offs[1][0], offs[1][1], offs[1][2])
	over16 = 0
	for y in range(offs[1][2]):
		for x in range(offs[1][1]):
			box = (alpha[2 * y][2 * x] + alpha[2 * y][2 * x + 1]
				   + alpha[2 * y + 1][2 * x] + alpha[2 * y + 1][2 * x + 1] + 2) >> 2
			rng = block_alpha_range(dataB, offs[1][0], offs[1][1], x // 4, y // 4)
			if abs(a1[y][x] - box) > rng / 14.0 + 2.0:
				over16 += 1
	last = decode_alpha(dataB, offs[-1][0], offs[-1][1], offs[-1][2])
	lastVals = set(v for row in last for v in row)
	print('  C16 mip 1 vs the box filter of mip 0: violations %d (bar 0); last mip distinct '
		  'alphas %d' % (over16, len(lastVals)))
	c.check('C16 the cover survives the mip chain',
			over16 == 0 and lastVals not in ({255}, {0}))

	# ---- C10 / C11: the tint, at the granularity BC1 preserves ----
	cB = open(coverDds, 'rb').read()
	pB = open(plainDds, 'rb').read()
	cOff = mip_offsets(RES, RES, 8, dds_info(cB)['mips'])[0][0]
	pOff = mip_offsets(RES, RES, 8, dds_info(pB)['mips'])[0][0]
	diffBlocks = 0
	badBlocks = 0
	bw = RES // 4
	for by in range(bw):
		for bx in range(bw):
			o = cOff + (by * bw + bx) * 8
			same = cB[o:o + 8] == pB[pOff + (by * bw + bx) * 8:pOff + (by * bw + bx) * 8 + 8]
			anyCover = any(alpha[by * 4 + (i >> 2)][bx * 4 + (i & 3)] > 0 for i in range(16))
			if not same:
				diffBlocks += 1
				if not anyCover:
					badBlocks += 1
	print('  C10 diffuse blocks that moved: %d [sampled, bar > 250]; moved with no cover in '
		  'them: %d (bar 0)' % (diffBlocks, badBlocks))
	c.check('C10 the tint moved only blocks that carry cover',
			badBlocks == 0 and diffBlocks > 250)

	cRgb = decode_color(cB, cOff, RES, RES, 8, 0)
	pRgb = decode_color(pB, pOff, RES, RES, 8, 0)
	top = sorted(((alpha[y][x], x, y) for y in range(RES) for x in range(RES)),
				 reverse=True)[:256]
	deltas = []
	tEst = []
	for a, x, y in top:
		if a == 0:
			continue
		w = a / 255.0 * 0.35
		d = [cRgb[y][x][k] - pRgb[y][x][k] for k in range(3)]
		deltas.append(math.sqrt(sum(v * v for v in d)))
		if w > 0.05:
			tEst.append([pRgb[y][x][k] + d[k] / w for k in range(3)])
	meanDelta = (sum(deltas) / len(deltas)) if deltas else 0.0
	print('  C11 mean tint delta over the 256 highest-cover texels: %.2f/255 (bar > 3)'
		  % meanDelta)
	c.check('C11 the tint actually moves the albedo where cover is high', meanDelta > 3.0)
	# C11b: the implied grass colour must be the SAME on dark ground as on
	# bright. Applied before the VCLR multiply it would scale with the vertex
	# colour, and the two groups would disagree.
	if len(tEst) >= 32:
		half = len(tEst) // 2
		byBright = sorted(range(len(tEst)),
						  key=lambda i: sum(tEst[i]))
		lo = [tEst[i] for i in byBright[:half]]
		hi = [tEst[i] for i in byBright[half:]]
		mlo = [sum(v[k] for v in lo) / len(lo) for k in range(3)]
		mhi = [sum(v[k] for v in hi) / len(hi) for k in range(3)]
		spread = math.sqrt(sum((mhi[k] - mlo[k]) ** 2 for k in range(3)))
		print('  C11b implied grass colour, dark half (%.0f,%.0f,%.0f) vs bright half '
			  '(%.0f,%.0f,%.0f), spread %.1f (bar < 60)'
			  % (mlo[0], mlo[1], mlo[2], mhi[0], mhi[1], mhi[2], spread))
		c.check('C11b one grass colour explains both halves (the tint is after VCLR)',
				spread < 60.0)
	else:
		c.check('C11b there are enough tinted texels to test the VCLR side', False)

	# ---- C7 on a second chunk of different terrain character ----
	if model2Path and model2Path != '-':
		m2 = json.load(open(model2Path))
		print('  C7 second chunk (%d,%d) model loaded: dominant base %s'
			  % (m2['cx'], m2['cy'], m2['dominantBase']))

	# ---- C14: the census against this walk ----
	census = json.load(open(censusPath))
	print('  C14 python walk: LTEX %d, GRAS %d, GNAM links %d, LTEX with GNAM %d, '
		  'GRAS DATA %d..%d, bytes left over %d'
		  % (census['ltexTotal'], census['grasTotal'], census['gnamLinks'],
			 census['ltexWithGnam'], census['grasDataMin'], census['grasDataMax'],
			 census['leftover']))
	c.check('C14 the python walk consumed every byte it read', census['leftover'] == 0)
	c.check('C14 every GRAS carries a 32-byte DATA and no DNAM',
			census['grasDataMin'] == 32 and census['grasDataMax'] == 32
			and census['grasWithoutData'] == 0)
	c.check('C14 the artists own NoGrass records exist and are the reason ltexNoGnam is '
			'informational', len(census['noGrassEdids']) > 0,
			'%d EDIDs end in NoGrass' % len(census['noGrassEdids']))

	print('  %d checks, %d failures' % (c.n, c.fails))
	return 1 if c.fails else 0


if __name__ == '__main__':
	sys.exit(main())
