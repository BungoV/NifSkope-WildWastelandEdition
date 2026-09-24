#!/usr/bin/env python
"""PIC-CHUNK: ours vs vanilla, one chunk's sheet, two panels side by side.

Read-only. Writes only under scratchpad/pic_chunk_20260911/.
DDS reading is tests/spells/lodgen_terrain_model.py's Dds (the lanes' own).
"""
import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from lodgen_terrain_model import Dds

REPO = 'E:/Projects/NifskopeWildWastelandEdition'
OURS = REPO + '/scratchpad/roads1_20260911/out/after/tex/'
VAN  = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/'
OUT  = REPO + '/scratchpad/pic_chunk_20260911/images/'
CHUNK = 'Commonwealth.4.-20.20'

FMT = {b'DXT1': 'BC1', b'DXT3': 'BC2', b'DXT5': 'BC3', b'BC5U': 'BC5', b'ATI2': 'BC5'}


def load(path):
	"""mip 0 as uint8 HxWx4, plus the facts the caption quotes."""
	d = Dds(path)
	px, w, h = d._level(0)
	a = np.asarray(px, dtype=np.float32).reshape(h, w, 4)
	img = np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)
	return img, dict(fmt=FMT.get(d.fourcc, repr(d.fourcc)), bytes=os.path.getsize(path),
	                 w=w, h=h, mips=len(d.mips), path=path)


def font(sz, bold=False):
	for n in (('arialbd.ttf' if bold else 'arial.ttf'), 'segoeui.ttf'):
		try:
			return ImageFont.truetype('C:/Windows/Fonts/' + n, sz)
		except OSError:
			pass
	return ImageFont.load_default()


def panel_page(left, lmeta, lcap, right, rmeta, rcap, title, sub, red, out):
	F_T, F_S, F_L, F_R = font(21, True), font(14), font(14), font(15, True)
	M, GAP = 26, 26
	W = M + 512 + GAP + 512 + M
	y_title = 14
	y_sub = y_title + 30
	y_tag = y_sub + 19 * len(sub) + 10
	y_img = y_tag + 22
	y_lab = y_img + 512 + 10
	y_red = y_lab + 44
	H = y_red + 26 + 14
	page = Image.new('RGB', (W, H), (24, 24, 26))
	dr = ImageDraw.Draw(page)
	dr.text((M, y_title), title, font=F_T, fill=(238, 238, 240))
	for i, line in enumerate(sub):
		dr.text((M, y_sub + 19 * i), line, font=F_S, fill=(160, 160, 166))
		assert dr.textlength(line, font=F_S) <= W - 2 * M, 'subtitle line %d overflows the page' % i
	assert dr.textlength(title, font=F_T) <= W - 2 * M, 'title overflows the page'
	for x, im, cap, meta, tag, col in ((M, left, lcap, lmeta, 'OURS', (120, 210, 130)),
	                                   (M + 512 + GAP, right, rcap, rmeta, 'VANILLA', (150, 175, 235))):
		page.paste(Image.fromarray(im[:, :, :3], 'RGB'), (x, y_img))
		dr.rectangle([x - 1, y_img - 1, x + 512, y_img + 512], outline=(90, 90, 96))
		dr.text((x, y_tag), tag, font=F_R, fill=col)
		dr.text((x, y_lab), cap, font=F_L, fill=(222, 222, 226))
		dr.text((x, y_lab + 19), '%s  %s  %d bytes  %dx%d  %d mips' % (
			os.path.basename(meta['path']), meta['fmt'], meta['bytes'], meta['w'], meta['h'], meta['mips']),
			font=F_S, fill=(150, 150, 156))
	dr.text((M, y_red), red, font=F_R, fill=(236, 70, 70))
	page.save(out)
	return page.size


def mean_rgb(a):
	return tuple(float(a[:, :, i].mean()) for i in range(3))


def main():
	os.makedirs(OUT, exist_ok=True)
	facts = {}
	pairs = [('', 'chunk_vs_vanilla.png', 'colour sheet'),
	         ('_msn', 'chunk_vs_vanilla_msn.png', 'normal sheet')]
	for suffix, outname, what in pairs:
		o, om = load(OURS + CHUNK + suffix + '.DDS')
		v, vm = load(VAN + CHUNK + suffix + '.DDS')
		mo, mv = mean_rgb(o), mean_rgb(v)
		d = np.abs(o[:, :, :3].astype(np.int16) - v[:, :, :3].astype(np.int16))
		facts[outname] = dict(ours=om, vanilla=vm, mean_ours=mo, mean_vanilla=mv,
		                      mean_abs_diff=float(d.mean()),
		                      mean_abs_diff_ch=[float(d[:, :, i].mean()) for i in range(3)])
		size = panel_page(
			o, om, 'mean RGB %.1f, %.1f, %.1f' % mo,
			v, vm, 'mean RGB %.1f, %.1f, %.1f' % mv,
			'Far-terrain chunk %s -- our bake beside Bethesda\'s, %s' % (CHUNK, what),
			['ROADS1 bake  scratchpad/roads1_20260911/out/after  (--roads, the shipped default), written 2026-09-11 12:19:52',
			 'by release/NifSkope.exe 2026-09-11 12:19:06, 21,180,928 B.   Chunk (-20,20) = cells -20..-17 x 20..23, the Sanctuary loop road.',
			 'Both panels 512 x 512 texels at 32 world units a texel, the same grid, drawn 1:1 with no resampling on either side.']
			+ ([] if not suffix else ['The normal sheet is model-space: R = east, G = up, B = north.']),
			'colour sheet baked with TILE=2048 (the 6x tiling bug, SPLAT1 16:1x); fix pending in RESUME3',
			OUT + outname)
		facts[outname]['page'] = size
		print(outname, size, 'ours', om['fmt'], om['bytes'], 'vanilla', vm['fmt'], vm['bytes'],
		      'meanOurs %.2f %.2f %.2f' % mo, 'meanVan %.2f %.2f %.2f' % mv,
		      'meanAbsDiff %.2f' % facts[outname]['mean_abs_diff'])
	with open(REPO + '/scratchpad/pic_chunk_20260911/facts.json', 'w') as f:
		json.dump(facts, f, indent=1)


if __name__ == '__main__':
	main()
