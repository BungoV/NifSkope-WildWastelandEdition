#!/usr/bin/env python3
"""SUNSIM1 -- shading, sky, haze, and the labelled side-by-side pair.

Grey albedo, no textures.  Terrain is deliberately a little warmer than the
objects so the two read apart in a picture that is otherwise all grey.
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ALB_TER = np.array([0.50, 0.44, 0.36])
ALB_OBJ = np.array([0.36, 0.37, 0.40])
SUN_COL = np.array([1.00, 0.91, 0.76])
AMB_COL = np.array([0.34, 0.40, 0.52])
AMB_K = 0.34
HAZE = np.array([0.58, 0.67, 0.80])
HAZE_D = 95000.0
NODATA = np.array([0.20, 0.55, 0.62])   # the wash the RIGHT panel wears where the bake says nothing

F_TITLE = 'C:/Windows/Fonts/arialbd.ttf'
F_BODY = 'C:/Windows/Fonts/consola.ttf'


def sky(dirs, el):
	"""A cheap two-band sky so up is not flat."""
	z = np.clip(dirs[:, 2], -1.0, 1.0)
	up = np.clip(z, 0.0, 1.0) ** 0.6
	top = np.array([0.24, 0.40, 0.72])
	hor = np.array([0.72, 0.80, 0.88])
	c = hor[None, :] * (1 - up[:, None]) + top[None, :] * up[:, None]
	glow = np.clip(1.0 - np.abs(z - np.sin(np.radians(el))) * 6.0, 0.0, 1.0) ** 3
	return c + glow[:, None] * np.array([0.30, 0.20, 0.06])[None, :]


def shade(gb, lit, el, az, sunmul=1.0, nodata=None):
	"""lit is 0..1 a pixel.  Returns an (N,3) linear-light image."""
	n = len(gb.kind)
	sd = np.array([np.sin(np.radians(az)) * np.cos(np.radians(el)),
				   np.cos(np.radians(az)) * np.cos(np.radians(el)),
				   np.sin(np.radians(el))])
	ndl = np.clip(gb.nrm @ sd, 0.0, 1.0)
	alb = np.where(gb.kind[:, None] == 2, ALB_OBJ[None, :], ALB_TER[None, :])
	# a low sun is redder and weaker
	warm = np.clip(el / 25.0, 0.0, 1.0)
	sc = SUN_COL * (0.55 + 0.45 * warm) + np.array([0.18, 0.02, -0.06]) * (1 - warm)
	col = alb * (sc[None, :] * (ndl * lit)[:, None] * 2.15 * sunmul
				 + AMB_COL[None, :] * AMB_K * (0.55 + 0.45 * np.clip(gb.nrm[:, 2:3], 0, 1)))
	if nodata is not None and nodata.any():
		col[nodata] = col[nodata] * 0.62 + NODATA[None, :] * 0.30
	s = sky(gb.dir, el)
	out = np.where(gb.kind[:, None] == 0, s, col)
	t = np.where(np.isfinite(gb.t), gb.t, 0.0)
	f = 1.0 - np.exp(-t / HAZE_D)
	f = np.where(gb.kind == 0, 0.0, f)[:, None]
	hz = HAZE * (0.75 + 0.35 * warm)
	return out * (1 - f) + hz[None, :] * f


def to8(lin, w, h):
	img = np.clip(lin, 0.0, None).reshape(h, w, 3)
	img = img / (1.0 + img * 0.28)           # gentle shoulder, no clipping to white
	img = np.clip(img, 0.0, 1.0) ** (1 / 2.2)
	return (img * 255.0 + 0.5).astype(np.uint8)


def pair(left, right, w, h, ltitle, rtitle, caption, path, sub_l='', sub_r=''):
	"""Two panels side by side with a title bar and a caption strip."""
	GAP, TOP, BOT, PAD = 10, 58, 92, 12
	W = PAD * 2 + w * 2 + GAP
	H = TOP + h + BOT
	im = Image.new('RGB', (W, H), (17, 18, 20))
	im.paste(Image.fromarray(left), (PAD, TOP))
	im.paste(Image.fromarray(right), (PAD + w + GAP, TOP))
	d = ImageDraw.Draw(im)
	ft = ImageFont.truetype(F_TITLE, 26)
	fs = ImageFont.truetype(F_BODY, 16)
	fc = ImageFont.truetype(F_BODY, 17)
	d.text((PAD, 10), ltitle, font=ft, fill=(236, 232, 224))
	d.text((PAD + w + GAP, 10), rtitle, font=ft, fill=(236, 232, 224))
	if sub_l:
		d.text((PAD, 38), sub_l, font=fs, fill=(150, 156, 166))
	if sub_r:
		d.text((PAD + w + GAP, 38), sub_r, font=fs, fill=(150, 156, 166))
	y = TOP + h + 10
	for line in caption.split('\n'):
		d.text((PAD, y), line, font=fc, fill=(198, 202, 210))
		y += 21
	d.rectangle([PAD - 1, TOP - 1, PAD + w, TOP + h], outline=(70, 72, 78))
	d.rectangle([PAD + w + GAP - 1, TOP - 1, PAD + w * 2 + GAP, TOP + h], outline=(70, 72, 78))
	im.save(path)
	return im


def stamp(img, text, xy=(10, 10), size=20, fill=(255, 240, 210)):
	im = Image.fromarray(img)
	d = ImageDraw.Draw(im)
	f = ImageFont.truetype(F_TITLE, size)
	d.text((xy[0] + 1, xy[1] + 1), text, font=f, fill=(0, 0, 0))
	d.text(xy, text, font=f, fill=fill)
	return np.asarray(im)
