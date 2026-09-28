#!/usr/bin/env python
"""FLAT2 offline before/after of the sea-edge region (cells 32 -12 .. 43 -1), with no NifSkope run.

  before = the RUNG exe's .lodt (every sheet stored in full)
  after  = the new exe's default .lodt (one-value sheets stored as 16-byte records), read through the
           in-tree checker's reader, which expands a record back to the full sheet

usage: sea_pics.py <rung vt dir> <on vt dir> <out dir>

1. Decode check, every tile x sheet x mip x texel of both levels: the after bytes, expanded, equal the
   before bytes, AND the decoded texels equal (BC1/BC3 through vtread's decoder, R16 as is).
   A control first: one byte of one collapsed record flipped must make the decode check red.
2. Pictures (full size, 60 px title bar, one per file), finest level (dim 2, 6 x 6 tiles, the
   512-texel content of each tile, border cropped), north up:
     F2 which sheets are stored as one value, per tile (legend measured back from the picture)
     F3/F4 normal (msn) sheet before / after      F5/F6 height sheet before / after
     F7 texels that differ between before and after (black = equal; any difference painted red)
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
sys.path.insert(0, os.path.join(HERE, 'work'))
import lodgen_vt_check as vc  # noqa: E402
import vtread  # noqa: E402

rungDir, onDir, outDir = sys.argv[1:4]
os.makedirs(outDir, exist_ok=True)
EDID = os.environ.get('EDID', 'Commonwealth')  # env EDID=NukaWorld; env COMPARE_ONLY=1 skips the pictures
ROLE = {1: 'colour', 2: 'msn', 4: 'height', 5: 'mask'}
BAR = 60
try:
	F1 = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
	F2 = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 17)
except OSError:
	F1 = F2 = ImageFont.load_default()


def decode(L, s, mip, cover, raw):
	"""One sheet mip of one tile's FULL payload -> (side, side, ch) int32."""
	o = L.sheetOffset(cover, s, mip)
	n = L.sheetMipBytes(s, mip, cover)
	seg = np.frombuffer(raw[o:o + n], np.uint8)
	side = L.stored >> mip
	role = L.sheets[s]['role']
	if role == 4:
		return seg.view('<u2').reshape(side, side, 1).astype(np.int32)
	unit = L.unitBytes(s, cover)
	blk = seg.reshape(side // 4, side // 4, unit)
	if unit == 16:
		col = vtread.decode_bc1_blocks(blk[..., 8:16], four=True)
		alp = vtread.decode_bc3_alpha(blk[..., 0:8])
		return np.concatenate([col, alp[..., None]], -1).astype(np.int32)
	return vtread.decode_bc1_blocks(blk, four=False).astype(np.int32)


def compare(Lb, La, doctor=False):
	"""-> (tiles, sheet-mips, texels compared, byte diffs, texel diffs, per-tile collapsed masks, diff map)"""
	nT = nSM = nTex = dBytes = dTex = 0
	masks = {}
	diffmap = {}
	for i in range(Lb.tileCount):
		eb, ea = Lb.table[i], dict(La.table[i])
		if not (eb['flags'] & 1):
			continue
		nT += 1
		rb = Lb.payload(i)
		d = La.b[ea['offset']:ea['offset'] + ea['stored']]
		mask = La.uniformMask(ea)
		if doctor and mask:
			s0 = [s for s in range(La.sheetCount) if mask & (1 << s)][0]
			cover = bool(ea['flags'] & 2)
			at = sum(La.sheetStoredBytes(t, cover, mask) for t in range(s0))
			d = bytearray(d)
			d[at] ^= 0x01
			d = bytes(d)
			doctor = False
		ra = La.expand(ea, d)
		masks[i] = mask
		if ra != rb:
			dBytes += 1
		cover = bool(eb['flags'] & 2)
		dm = np.zeros((Lb.stored, Lb.stored), bool)
		for s in range(Lb.sheetCount):
			for m in range(Lb.mips):
				xb = decode(Lb, s, m, cover, rb)
				xa = decode(La, s, m, cover, ra)
				nSM += 1
				nTex += xb.shape[0] * xb.shape[1]
				ne = (xb != xa).any(-1)
				dTex += int(ne.sum())
				if m == 0:
					dm |= ne
		diffmap[i] = dm
	return nT, nSM, nTex, dBytes, dTex, masks, diffmap


def titled(img, title, sub):
	w, h = img.size
	out = Image.new('RGB', (w, h + BAR), (0, 0, 0))
	out.paste(img, (0, BAR))
	d = ImageDraw.Draw(out)
	d.text((12, 4), title, font=F1, fill=(255, 255, 255))
	d.text((12, 36), sub, font=F2, fill=(200, 200, 200))
	return out


def mosaic(L, s, full_of):
	"""Finest-level content mosaic of sheet s, mip 0, border cropped, north up (row 0 = north)."""
	c, b = L.content, L.border
	ch = 1 if L.sheets[s]['role'] == 4 else None
	out = None
	for ty in range(L.tilesY):
		for tx in range(L.tilesX):
			i = L.tileIndex(tx, ty) if hasattr(L, 'tileIndex') else ty * L.tilesX + tx
			e = L.table[i]
			if not (e['flags'] & 1):
				continue
			x = decode(L, s, 0, bool(e['flags'] & 2), full_of(i))[b:b + c, b:b + c]
			if out is None:
				out = np.zeros((L.tilesY * c, L.tilesX * c, x.shape[-1]), np.int32)
			out[ty * c:(ty + 1) * c, tx * c:(tx + 1) * c] = x
	return out


def main():
	res = []
	for dim in (2, 4):
		Lb = vc.Lodv(os.path.join(rungDir, EDID + '.VT.%d.lodt' % dim))
		La = vc.Lodv(os.path.join(onDir, EDID + '.VT.%d.lodt' % dim))
		c = compare(Lb, La, doctor=True)
		print('control dim %d (one collapsed record byte flipped): %d tile payloads differ, %d texels differ -> %s'
			  % (dim, c[3], c[4], 'RED (good)' if c[3] and c[4] else 'NOT RED'))
		r = compare(Lb, La)
		print('dim %d: %d tiles, %d sheet-mips, %d texels compared; payload bytes differ in %d tiles; texels differ %d; '
			  'tiles with a collapsed sheet %d' % (dim, r[0], r[1], r[2], r[3], r[4], sum(1 for m in r[5].values() if m)))
		res.append((dim, Lb, La, r, c))
		print('file bytes: before %d, after %d' % (len(Lb.b), len(La.b)))
	if os.environ.get('COMPARE_ONLY'):
		return

	dim, Lb, La, r, _ = res[0]
	masks, diffmap = r[5], r[6]
	west, north = Lb.west, Lb.north
	sub0 = 'Commonwealth cells %d,%d .. %d,%d, finest level (%d x %d cells a tile), north up, %d texels a side' % (
		Lb.west, Lb.south, Lb.east, Lb.north, dim, dim, Lb.tilesX * Lb.content)
	idx = {ROLE[Lb.sheets[s]['role']]: s for s in range(Lb.sheetCount)}

	# F2: which sheets are one value, per tile -- ONE palette for tiles and legend
	PAL = {
		'no sheet stored as one value': (92, 92, 92),
		'normal (msn) only': (70, 150, 235),
		'height only': (235, 170, 40),
		'normal and height': (215, 60, 190),
		'colour or mask': (40, 220, 90),
	}
	cat = {}
	for i, m in masks.items():
		one = {k: bool(m & (1 << s)) for k, s in idx.items()}
		k = 'no sheet stored as one value'
		if one['msn'] and not one['height']:
			k = 'normal (msn) only'
		if one['height'] and not one['msn']:
			k = 'height only'
		if one['msn'] and one['height']:
			k = 'normal and height'
		if one['colour'] or one['mask']:
			k = 'colour or mask'
		cat[i] = k
	C = 96
	LEG = 380
	img = Image.new('RGB', (Lb.tilesX * C + LEG + 40, Lb.tilesY * C + 20), (16, 16, 18))
	d = ImageDraw.Draw(img)
	for i, k in cat.items():
		tx, ty = i % Lb.tilesX, i // Lb.tilesX
		d.rectangle([10 + tx * C, 10 + ty * C, 10 + tx * C + C - 2, 10 + ty * C + C - 2], fill=PAL[k])
	lx, ly = 30 + Lb.tilesX * C, 10
	counts = {k: sum(1 for v in cat.values() if v == k) for k in PAL}
	for k, rgb in PAL.items():
		d.rectangle([lx, ly, lx + 22, ly + 22], fill=rgb)
		d.text((lx + 30, ly), '%s: %d tiles' % (k, counts[k]), font=F2, fill=(230, 230, 230))
		ly += 32
	f2 = titled(img, 'Sea edge: which terrain sheets the new writer stores as one 16-byte record',
				'cells %d,%d .. %d,%d, %d x %d tiles of %d x %d cells, north up; from the new exe\'s default bake'
				% (Lb.west, Lb.south, Lb.east, Lb.north, Lb.tilesX, Lb.tilesY, dim, dim))
	p2 = os.path.join(outDir, 'F2_sea_edge_one_value_sheets_map.png')
	f2.save(p2)
	a = np.asarray(Image.open(p2).convert('RGB'))[BAR + 10:BAR + 10 + Lb.tilesY * C, 10:10 + Lb.tilesX * C].reshape(-1, 3)
	bad = 0
	for k, rgb in PAL.items():
		n = int((a == np.array(rgb)).all(1).sum())
		ok = n == counts[k] * (C - 1) * (C - 1)
		bad += not ok
		print('legend %-30s tiles %3d pixels %7d %s' % (k, counts[k], n, 'ok' if ok else 'MISMATCH'))
	print('F2 legend check: %s' % ('PASS' if not bad else 'FAIL'))

	def full_b(i):
		return Lb.payload(i)

	def full_a(i):
		return La.payload(i)

	# F3/F4: normal sheet, RGB as stored (x red, UP green)
	for (tag, L, fo, when) in (('F3', Lb, full_b, 'BEFORE: old exe, every sheet stored in full'),
							   ('F4', La, full_a, 'AFTER: new exe, one-value sheets stored as 16-byte records, expanded')):
		m = mosaic(L, idx['msn'], fo)
		im = Image.fromarray(m[..., :3].astype(np.uint8), 'RGB')
		titled(im, 'Sea edge normal (msn) sheet -- ' + when, sub0 + '; the sheet\'s RGB as decoded').save(
			os.path.join(outDir, '%s_sea_edge_normal_%s.png' % (tag, 'before' if tag == 'F3' else 'after')))
	# F5/F6: height, one grey ramp fixed from the BEFORE sheet's range
	hb = mosaic(Lb, idx['height'], full_b)[..., 0]
	lo, hi = int(hb.min()), int(hb.max())
	for (tag, L, fo, when) in (('F5', Lb, full_b, 'BEFORE: old exe, every sheet stored in full'),
							   ('F6', La, full_a, 'AFTER: new exe, one-value sheets stored as 16-byte records, expanded')):
		h = mosaic(L, idx['height'], fo)[..., 0]
		g = np.clip((h - lo) * 255.0 / max(1, hi - lo), 0, 255).astype(np.uint8)
		titled(Image.fromarray(g, 'L').convert('RGB'), 'Sea edge height sheet -- ' + when,
			   sub0 + '; grey = R16 %d (black) .. %d (white), same ramp both pictures' % (lo, hi)).save(
			os.path.join(outDir, '%s_sea_edge_height_%s.png' % (tag, 'before' if tag == 'F5' else 'after')))
	# F7: texel differences, any sheet, mip 0 (black = equal)
	c, b = Lb.content, Lb.border
	dm = np.zeros((Lb.tilesY * c, Lb.tilesX * c), bool)
	for i, m in diffmap.items():
		tx, ty = i % Lb.tilesX, i // Lb.tilesX
		dm[ty * c:(ty + 1) * c, tx * c:(tx + 1) * c] = m[b:b + c, b:b + c]
	rgb = np.zeros(dm.shape + (3,), np.uint8)
	rgb[dm] = (255, 40, 40)
	nd = int(dm.sum())
	titled(Image.fromarray(rgb, 'RGB'), 'Sea edge: texels that differ between before and after (red), any of the 4 sheets: %d' % nd,
		   sub0 + '; black = the after sheet decodes to exactly the before texel').save(
		os.path.join(outDir, 'F7_sea_edge_before_after_difference.png'))
	ok = all(rr[3][3] == 0 and rr[3][4] == 0 and rr[4][3] and rr[4][4] for rr in res) and not bad
	print('RESULT %s' % ('PASS' if ok else 'FAIL'))
	sys.exit(0 if ok else 1)


main()
