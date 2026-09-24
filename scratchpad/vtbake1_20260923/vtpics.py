#!/usr/bin/env python
"""Lane VTBAKE1 job 3: every level's colour, normal and mask as one NORTH-UP
overview each, labelled with units-per-texel, plus the Sanctuary crop at the
finest level beside vanilla's shipped dim-4 chunk sheet.

usage: python vtpics.py <dir with Commonwealth.VT.<dim>.lodt> <out dir> <vanilla Textures/Terrain/Commonwealth dir>
"""
import os
import sys
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vtread import Lodt, decode_bc1_blocks, decode_bc3_alpha

UNIT_M = 0.9144 / 64.0   # docs/GLTF_INTERCHANGE.md: 1 NIF unit = 0.0142875 m
TARGET = 1536            # every overview is this many pixels across


def font(sz):
	for p in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
		if os.path.exists(p):
			return ImageFont.truetype(p, sz)
	return ImageFont.load_default()


def boxdown(a, k):
	if k == 1:
		return a
	h, w = a.shape[:2]
	a = a[:h - h % k, :w - w % k].astype(np.float32)
	if a.ndim == 3:
		return a.reshape(h // k, k, w // k, k, a.shape[2]).mean((1, 3))
	return a.reshape(h // k, k, w // k, k).mean((1, 3))


def mosaic(v, role, want):
	"""Content-only mosaic of one sheet, north-up, at `want` px per tile."""
	s = v.sheetIndex(role)
	# pick the mip whose content is >= want and closest
	mip = 0
	while mip + 1 < v.mips and (v.content >> (mip + 1)) >= want:
		mip += 1
	c = v.content >> mip
	b = v.border >> mip
	k = c // want
	ch = 3 if role != 99 else 4
	out = np.zeros((v.tilesY * want, v.tilesX * want, 4), np.float32)
	for ty in range(v.tilesY):
		for tx in range(v.tilesX):
			i = v.tileIndex(tx, ty)
			col, alp = v.rgb(i, s, mip)
			if col is None:
				continue
			col = col[b:b + c, b:b + c]
			rgba = np.zeros((c, c, 4), np.float32)
			rgba[..., :3] = col
			rgba[..., 3] = alp[b:b + c, b:b + c] if alp is not None else 255
			out[ty * want:(ty + 1) * want, tx * want:(tx + 1) * want] = boxdown(rgba, k)
	return out, mip, k


def label(img, lines, fsz=22):
	d = ImageDraw.Draw(img)
	f = font(fsz)
	y = 8
	for ln in lines:
		w = d.textlength(ln, font=f)
		d.rectangle([6, y - 2, 14 + w, y + fsz + 4], fill=(0, 0, 0))
		d.text((10, y), ln, fill=(255, 255, 255), font=f)
		y += fsz + 8
	# north arrow, top right
	W = img.size[0]
	d.polygon([(W - 30, 10), (W - 42, 40), (W - 18, 40)], fill=(255, 255, 255), outline=(0, 0, 0))
	d.text((W - 38, 44), 'N', fill=(255, 255, 255), font=font(20), stroke_width=2, stroke_fill=(0, 0, 0))


def sanctuary_box(v, img, want):
	"""Outline the Sanctuary chunk (cells -20..-17 x 24..27) on an overview."""
	d = ImageDraw.Draw(img)
	ppc = want / v.levelDim  # px per cell
	x0 = (-20 - v.west) * ppc
	x1 = (-17 + 1 - v.west) * ppc
	y0 = (v.north - 27) * ppc
	y1 = (v.north + 1 - 24) * ppc
	d.rectangle([x0, y0, x1, y1], outline=(255, 40, 40), width=2)


def main():
	src, outd, vdir = sys.argv[1], sys.argv[2], sys.argv[3]
	os.makedirs(outd, exist_ok=True)
	rows = []
	for dim in (2, 4, 8, 16, 32):
		p = os.path.join(src, 'Commonwealth.VT.%d.lodt' % dim)
		v = Lodt(p)
		upt = v.levelDim * 4096 // v.content
		want = TARGET // v.tilesX
		for role, name in ((1, 'colour'), (2, 'normal'), (5, 'mask')):
			m, mip, k = mosaic(v, role, want)
			rgb = np.clip(m[..., :3] + 0.5, 0, 255).astype(np.uint8)
			img = Image.fromarray(rgb, 'RGB')
			sanctuary_box(v, img, want)
			shown = upt * (1 << mip) * k
			lines = ['Commonwealth .lodt level dim %d  --  %s sheet' % (dim, name),
					 'stored: %d units/texel = %.3f m/texel' % (upt, upt * UNIT_M),
					 'this picture: %d units/px (mip %d, box %dx%d)' % (shown, mip, k, k),
					 'north up; red box = Sanctuary chunk 4.-20.24']
			if name == 'normal':
				lines.append('RGB as stored: R east, G up, B north')
			if name == 'mask':
				lines.append('RGB = R roughness, G metallic, B AO (A cover: not baked)')
			label(img, lines)
			fn = os.path.join(outd, 'L%02d_%s.png' % (dim, name))
			img.save(fn)
			rows.append((dim, name, upt, shown, fn, float(m[..., :3].mean()),
						 float(m[..., :3].reshape(-1, 3).std(0).mean())))
			print('wrote %s  upt %d  shown %d  mean %.2f  sd %.2f' % (fn, upt, shown, rows[-1][5], rows[-1][6]))
			sys.stdout.flush()
	sanctuary(src, outd, vdir)


def dds_bc(path):
	"""Minimal DDS reader: DXT1/DXT5 mip 0 through our own decoder AND PIL
	(the known-answer control for the decoder)."""
	b = open(path, 'rb').read()
	h, w = struct.unpack_from('<II', b, 12)
	four = b[84:88]
	off = 128
	bw, bh = w // 4, h // 4
	if four == b'DXT5':
		blk = np.frombuffer(b, np.uint8, bw * bh * 16, off).reshape(bh, bw, 16)
		col = decode_bc1_blocks(blk[..., 8:16], four=True)
	elif four == b'DXT1':
		blk = np.frombuffer(b, np.uint8, bw * bh * 8, off).reshape(bh, bw, 8)
		col = decode_bc1_blocks(blk, four=False)
	else:
		raise ValueError('unhandled fourcc %r' % four)
	pil = np.asarray(Image.open(path).convert('RGB'))
	return col, pil, four.decode()


def corr(a, b):
	a = a.astype(np.float64).ravel(); b = b.astype(np.float64).ravel()
	a -= a.mean(); b -= b.mean()
	return float((a * b).sum() / np.sqrt((a * a).sum() * (b * b).sum()))


def sanctuary(src, outd, vdir):
	v = Lodt(os.path.join(src, 'Commonwealth.VT.2.lodt'))
	# chunk 4.-20.24 = cells -20..-17 x 24..27 = 2x2 tiles at dim 2
	txw, tyn = v.tileOfCell(-20, 27)
	txe, tys = v.tileOfCell(-17, 24)
	c, b = v.content, v.border
	out = {}
	for role in (1, 2):
		s = v.sheetIndex(role)
		a = np.zeros(((tys - tyn + 1) * c, (txe - txw + 1) * c, 3), np.uint8)
		for ty in range(tyn, tys + 1):
			for tx in range(txw, txe + 1):
				col, _ = v.rgb(v.tileIndex(tx, ty), s, 0)
				a[(ty - tyn) * c:(ty - tyn + 1) * c, (tx - txw) * c:(tx - txw + 1) * c] = col[b:b + c, b:b + c]
		out[role] = a
	vc, vpil, vfour = dds_bc(os.path.join(vdir, 'Commonwealth.4.-20.24.DDS'))
	vn, vnpil, vnfour = dds_bc(os.path.join(vdir, 'Commonwealth.4.-20.24_msn.DDS'))
	dec_ctrl = int(np.abs(vc.astype(int) - vpil.astype(int)).max())
	dec_ctrl_n = int(np.abs(vn.astype(int) - vnpil.astype(int)).max())
	ours = out[1]
	lum = lambda x: x.astype(np.float64) @ np.array([0.299, 0.587, 0.114])
	r_up = corr(lum(ours), lum(vc))
	r_flipv = corr(lum(ours), lum(vc[::-1]))
	r_fliph = corr(lum(ours), lum(vc[:, ::-1]))
	mad = float(np.abs(ours.astype(int) - vc.astype(int)).mean())
	mean_ours = ours.reshape(-1, 3).mean(0)
	mean_van = vc.reshape(-1, 3).mean(0)
	# normals: our G is up, vanilla's msn channel order measured by NATIVEVIEW2:
	# R east, G up, B north as well -- compare R channel (east) directly
	rn_up = corr(out[2][..., 0], vn[..., 0])
	rn_flip = corr(out[2][..., 0], vn[::-1, :, 0])
	gap = 16
	W = 512 * 2 + gap
	img = Image.new('RGB', (W, 512 * 2 + gap + 150), (20, 20, 20))
	img.paste(Image.fromarray(ours), (0, 150))
	img.paste(Image.fromarray(vc), (512 + gap, 150))
	img.paste(Image.fromarray(out[2]), (0, 150 + 512 + gap))
	img.paste(Image.fromarray(vn), (512 + gap, 150 + 512 + gap))
	d = ImageDraw.Draw(img)
	f = font(18)
	txt = ['Sanctuary, chunk 4.-20.24 (cells -20..-17 x 24..27), 16,384 units square, north up',
		   'LEFT: our .lodt level dim 2 (32 units/texel, 0.457 m), 2x2 tiles, content only',
		   "RIGHT: vanilla's shipped Commonwealth.4.-20.24.DDS / _msn.DDS (%s, 512 px = 32 units/px)" % vfour,
		   'top: colour; bottom: normal as stored. colour luminance r = %.3f (vanilla flipped N-S: %.3f)' % (r_up, r_flipv),
		   'mean |diff| %.1f/255; mean RGB ours %s vs vanilla %s' % (
			   mad, '/'.join('%.0f' % x for x in mean_ours), '/'.join('%.0f' % x for x in mean_van))]
	y = 6
	for t in txt:
		d.text((8, y), t, fill=(255, 255, 255), font=f)
		y += 26
	fn = os.path.join(outd, 'sanctuary_L02_vs_vanilla_chunk.png')
	img.save(fn)
	Image.fromarray(ours).save(os.path.join(outd, 'sanctuary_L02_colour_raw.png'))
	print('SANCTUARY tiles tx %d..%d ty %d..%d' % (txw, txe, tyn, tys))
	print('SANCTUARY decoder control: our BC3 decode vs PIL, max abs diff colour %d normal %d (%s/%s)' % (
		dec_ctrl, dec_ctrl_n, vfour, vnfour))
	print('SANCTUARY colour lum corr as-is %.4f, vanilla flipped N-S %.4f, flipped E-W %.4f' % (r_up, r_flipv, r_fliph))
	print('SANCTUARY normal R(east) corr as-is %.4f, vanilla flipped N-S %.4f' % (rn_up, rn_flip))
	print('SANCTUARY mean |diff| %.2f; mean RGB ours %s vanilla %s' % (mad, mean_ours.round(1), mean_van.round(1)))
	print('wrote', fn)


if __name__ == '__main__':
	main()
