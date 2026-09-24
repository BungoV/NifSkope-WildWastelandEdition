#!/usr/bin/env python3
"""SUNSIM1 -- the driver.  `python run.py [--w 1600] [--h 900] [--quick]`."""
import argparse
import json
import os
import sys
import time
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scene import Terrain, Objects, Sheet, CHUNK
from render import GBuffer, SunShadow, baked_lit
import cams as CAMS
import shade as SH

LANE = os.path.dirname(os.path.abspath(__file__))
IMG = LANE + '/images'

SUNS = [(120.0, 5.0), (120.0, 15.0), (120.0, 30.0), (120.0, 50.0), (240.0, 15.0)]


def stats(gb, truth, baked):
	"""% of decided pixels where the two panels disagree, split by surface."""
	dec = (gb.kind != 0) & np.isfinite(baked)
	tb = baked > 0.5
	out = {}
	for name, m in (('all', dec), ('terrain', dec & gb.terfirst), ('objects', dec & gb.objfirst)):
		k = int(m.sum())
		out[name] = (100.0 * float((truth[m] != tb[m]).sum()) / k if k else float('nan'), k)
	nod = int(((gb.kind != 0) & ~np.isfinite(baked)).sum())
	out['nodata'] = (100.0 * nod / max(1, int((gb.kind != 0).sum())), nod)
	return out


def one_pair(gb, ob, sheet, ter, az, el, cam, tag, note='', rot180=False, save=True):
	t0 = time.time()
	ss = SunShadow(ter, ob, az, el)
	lit_t = np.zeros(len(gb.kind))
	m = gb.kind != 0
	lit_t[m] = ss.lit(gb.pos[m]).astype(float)
	az_r = (az + 180.0) % 360.0 if rot180 else az
	lit_b, hz = baked_lit(gb, ob, sheet, az_r, el)
	st = stats(gb, lit_t > 0.5, lit_b)
	# outside the baked data's reach the right panel has nothing to say: it is
	# drawn LIT and those pixels are excluded from the disagreement above
	nod = (gb.kind != 0) & ~np.isfinite(lit_b)
	tb = np.where(np.isfinite(lit_b), lit_b, 1.0)
	L = SH.shade(gb, lit_t, el, az)
	R = SH.shade(gb, tb, el, az, nodata=nod)
	li = SH.to8(L, cam.w, cam.h)
	ri = SH.to8(R, cam.w, cam.h)
	dt = time.time() - t0
	if save:
		nd = ('' if st['nodata'][1] == 0 else
			  ('\n' if note else '') +
			  'teal wash on the RIGHT = %.1f%% of its surface pixels lie outside the baked '
			  'data (drawn lit, excluded from the figures above)' % st['nodata'][0])
		cap = ('sun azimuth %.0f deg, elevation %.0f deg   |   camera "%s" %dx%d, %.0f deg vfov\n'
			   'disagree lit/shadow  ALL %.2f%%  (terrain %.2f%% of %s px, objects %.2f%% of %s px)\n'
			   '%s%s'
			   % (az, el, cam.name, cam.w, cam.h, np.degrees(cam.vfov),
				  st['all'][0], st['terrain'][0], '{:,}'.format(st['terrain'][1]),
				  st['objects'][0], '{:,}'.format(st['objects'][1]), note, nd))
		SH.pair(li, ri, cam.w, cam.h,
				'LEFT  ray-cast sun (truth)',
				'RIGHT  baked horizon data (what FO4CS will get)',
				cap, '%s/%s.png' % (IMG, tag),
				sub_l='heightmap + 29,587 .lodo triangles; shadow ray with a 16 u footprint (86% of brute force)',
				sub_r='role-7 terrain sheet + .lodi v8 per-vertex horizon bytes only')
	return st, li, ri, dt


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('--w', type=int, default=1600)
	ap.add_argument('--h', type=int, default=900)
	ap.add_argument('--only', default='')
	ap.add_argument('--skip-top', action='store_true')
	ap.add_argument('--sweep', type=int, default=14)
	ap.add_argument('--suns', default='')
	ap.add_argument('--tag', default='persp')
	a = ap.parse_args()
	global SUNS
	if a.suns:
		SUNS = [tuple(float(x) for x in s.split(':')) for s in a.suns.split(',')]
	os.makedirs(IMG, exist_ok=True)
	T0 = time.time()
	ter = Terrain()
	ob = Objects()
	sheet = Sheet()
	print('scene loaded %.1fs' % (time.time() - T0))
	cs = CAMS.build(ter, a.w, a.h)
	want = [x for x in (a.only.split(',') if a.only else ['close', 'east', 'full', 'street']) if x]
	rows = []
	times = {}
	gbs = {}
	for nm in want:
		cam = cs[nm]
		print('camera %s  eye %s -> %s' % (nm, np.round(cam.eye), np.round(cam.target)))
		t0 = time.time()
		gb = GBuffer(cam, ter, ob)
		times['gbuffer_' + nm] = time.time() - t0
		gbs[nm] = (cam, gb)
		for (az, el) in SUNS:
			tag = '%s_%s_az%03d_el%02d' % (a.tag, nm, az, el)
			st, li, ri, dt = one_pair(gb, ob, sheet, ter, az, el, cam, tag)
			rows.append((nm, az, el, st, tag))
			print('  %-28s all %.2f%%  ter %.2f%%  obj %.2f%%  nodata %.1f%%  %.1fs'
				  % (tag, st['all'][0], st['terrain'][0], st['objects'][0], st['nodata'][0], dt))
			times[tag] = dt
	# ---- the sensitivity control: RIGHT panel read with the azimuth turned 180
	if 'east' in gbs:
		cam, gb = gbs['east']
		az, el = 120.0, 15.0
		tag = 'control_%s_az%03d_el%02d_RIGHT_ROT180' % ('east', az, el)
		st, _, _, dt = one_pair(gb, ob, sheet, ter, az, el, cam, tag, rot180=True,
								note='SANITY CONTROL: the RIGHT panel reads the baked bins at azimuth '
									 '%.0f (the sun turned 180 deg); the LEFT panel is unchanged.'
									 % ((az + 180) % 360))
		rows.append(('east_ROT180', az, el, st, tag))
		print('  %-28s all %.2f%%  ter %.2f%%  obj %.2f%%   (control)'
			  % (tag, st['all'][0], st['terrain'][0], st['objects'][0]))
	# ---- top-down, secondary
	if not a.skip_top:
		for crop in (False, True):
			cam = CAMS.topdown(2048, 2048, crop)
			cam.tstart = 9.0e5
			cam.tmax = 1.1e6
			print('camera %s' % cam.name)
			gb = GBuffer(cam, ter, ob)
			gb.t = gb.t - 1.0e6 + 1500.0           # haze measured from the ground, not the lens
			for (az, el) in [(120.0, 15.0)]:
				tag = 'top_%s_az%03d_el%02d' % ('crop' if crop else 'full', az, el)
				st, _, _, dt = one_pair(gb, ob, sheet, ter, az, el, cam, tag,
										note='SECONDARY: top-down, north up, a 1,000,000-unit lens '
											 '(orthographic to within a few pixels).')
				rows.append(('top_crop' if crop else 'top_full', az, el, st, tag))
				print('  %-28s all %.2f%%  ter %.2f%%  obj %.2f%%' % (tag, st['all'][0],
																	  st['terrain'][0], st['objects'][0]))
	# ---- the sweep, on camera (1)
	if 'close' in gbs and a.sweep:
		cam, gb = gbs['close']
		sw = []
		n = a.sweep
		frames = []
		for i in range(n):
			az = 95.0 + i * (170.0 / (n - 1))          # east-ish to west-ish
			el = 3.0 + 42.0 * np.sin(np.pi * (i + 0.5) / n)
			tag = 'sweep_%02d' % i
			st, li, ri, dt = one_pair(gb, ob, sheet, ter, az, el, cam, tag, save=False)
			lab = 'az %.0f  el %.0f   disagree %.1f%%' % (az, el, st['all'][0])
			half = np.concatenate([SH.stamp(li, 'TRUTH', (8, 6), 18),
								   np.zeros((cam.h, 6, 3), np.uint8),
								   SH.stamp(ri, 'BAKED', (8, 6), 18)], axis=1)
			half = SH.stamp(half, lab, (8, cam.h - 30), 18, (255, 220, 170))
			sc = Image.fromarray(half).resize((half.shape[1] // 2, half.shape[0] // 2), Image.LANCZOS)
			frames.append(sc)
			sw.append((az, el, st['all'][0], st['terrain'][0], st['objects'][0]))
			print('  sweep %02d az %6.1f el %5.1f  all %.2f%%' % (i, az, el, st['all'][0]))
		frames[0].save(IMG + '/sweep_close.gif', save_all=True, append_images=frames[1:],
					   duration=420, loop=0, optimize=True)
		strip_w = frames[0].width // 2
		strip_h = frames[0].height // 2
		cols = 4
		rowsn = (len(frames) + cols - 1) // cols
		st_img = Image.new('RGB', (cols * strip_w, rowsn * strip_h), (17, 18, 20))
		for i, f in enumerate(frames):
			st_img.paste(f.resize((strip_w, strip_h), Image.LANCZOS),
						 ((i % cols) * strip_w, (i // cols) * strip_h))
		st_img.save(IMG + '/sweep_close_filmstrip.png')
		json.dump(sw, open(LANE + '/sweep.json', 'w'), indent=1)
	json.dump([[r[0], r[1], r[2], {k: list(v) for k, v in r[3].items()}, r[4]] for r in rows],
			  open(LANE + '/disagreement.json', 'w'), indent=1)
	json.dump(times, open(LANE + '/times.json', 'w'), indent=1)
	print('TOTAL %.1f min' % ((time.time() - T0) / 60.0))


if __name__ == '__main__':
	main()
