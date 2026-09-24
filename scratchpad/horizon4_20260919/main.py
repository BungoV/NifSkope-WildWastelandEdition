#!/usr/bin/env python3
"""HORIZON4 -- assemble the ceiling table and write the picture pairs."""
import json
import numpy as np
import os
import sys
import time

import h4core as H
import rows as R
import shade as SH

LANE = H.LANE
IMG = LANE + '/images'


def main():
	os.makedirs(IMG, exist_ok=True)
	log = open(LANE + '/rows.log', 'w')

	def p(*a):
		s = ' '.join(str(x) for x in a)
		print(s)
		log.write(s + '\n')
		log.flush()

	T0 = time.time()
	ter, ob, sh = H.load_scene()
	gbs = H.gbuffers(ter, ob)
	z = np.load(LANE + '/ceiling.npz')
	hv = z['h_vert']
	hvP = z['h_vertP']
	hvM = z['h_vertM']
	hE = z['h_edge']
	edges = z['edges']
	elen = z['elen']
	ht32 = z['h_ter32']
	ht64 = z['h_ter64']
	x0, y0, x1, y1 = H.CHUNK
	ny32 = nx32 = int(round((x1 - x0) / 32.0))
	ny64 = nx64 = int(round((x1 - x0) / 64.0))
	p('ceiling: vert %s  edge %s  ter32 %s -> %dx%d  ter64 %s -> %dx%d'
	  % (hv.shape, hE.shape, ht32.shape, ny32, nx32, ht64.shape, ny64, nx64))

	def plane(h, n, upt, A, sec=False):
		b = R.sector_max(h, A) if sec else R.sub_bins(h, A)
		return H.Plane(b.reshape(n, n, A), x0, y0, upt, A)

	# ---------------------------------------------------------------- the rows
	O = {}
	O['O1'] = ('bins', ob.bins, 16, 'lerp')
	O['O2'] = ('bins', R.sub_bins(hv, 16), 16, 'lerp')
	O['O2p'] = ('bins', R.sub_bins(hvP, 16), 16, 'lerp')
	O['O2m'] = ('bins', R.sub_bins(np.minimum(hvP, hvM), 16), 16, 'lerp')
	O['O5a'] = ('bins', R.sub_bins(hv, 32), 32, 'lerp')
	O['O5b'] = ('bins', hv, 64, 'lerp')
	O['O5n'] = ('bins', R.sub_bins(hv, 16), 16, 'nearest')
	O['O5m'] = ('bins', R.sub_bins(hv, 16), 16, 'max')
	T = {}
	T['T1'] = ('plane', H.Plane(sh.plane, sh.ox, sh.oy, sh.upt, 16), 'lerp', 'bilinear')
	T['T2a'] = ('plane', plane(ht32, nx32, 32.0, 16), 'lerp', 'bilinear')
	T['T2b'] = ('plane', plane(ht64, nx64, 64.0, 16), 'lerp', 'bilinear')
	T['T3a'] = ('plane', plane(ht32, nx32, 32.0, 32), 'lerp', 'bilinear')
	T['T3b'] = ('plane', plane(ht32, nx32, 32.0, 64), 'lerp', 'bilinear')
	T['T4'] = ('plane', plane(ht32, nx32, 32.0, 16, sec=True), 'lerp', 'bilinear')
	T['T5'] = ('plane', plane(ht32, nx32, 32.0, 16), 'lerp', 'nearest')
	c2 = LANE + '/ceiling2.npz'
	if os.path.exists(c2):
		z2 = np.load(c2)
		for k, nm in (('h_ter32_r0', 'T6a'), ('h_ter32_r48', 'T6c')):
			if k in z2:
				b = z2[k]
				if b.shape[1] == 64:
					b = R.sub_bins(b, 16)
				T[nm] = ('plane', H.Plane(b.reshape(ny32, nx32, 16),
										  x0, y0, 32.0, 16), 'lerp', 'bilinear')
	else:
		z2 = None

	ekey, eorder = R.edge_lookup(edges)
	p('edges over 256 u: %d' % len(edges))

	SUNS = R.SUNS
	table = []
	pics = {(c, a, e) for c, a, e in R.PIC}

	def run(rowname, objkey, terkey, title, obj_override=None):
		for cam_nm in R.CAMS:
			cam, gb = gbs[cam_nm]
			for (az, el) in SUNS:
				truth = H.truth_lit(ter, ob, gb, az, el)
				obj = O[objkey] if obj_override is None else obj_override(gb, az)
				hz = R.right_hz(gb, ob, az, obj=obj, ter=T[terkey])
				st, lit = R.stats(gb, truth, hz, el)
				table.append(dict(row=rowname, cam=cam_nm, az=az, el=el,
								  all=st['all'][0], terrain=st['terrain'][0],
								  objects=st['objects'][0], terpx=st['terrain'][1],
								  objpx=st['objects'][1], nodata=st['nodata'][0]))
				p('%-5s %-7s az%3.0f el%2.0f   ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%'
				  % (rowname, cam_nm, az, el, st['all'][0], st['terrain'][0], st['objects'][0]))
				if (cam_nm, az, el) in pics:
					R.picture(gb, cam, truth, hz, lit, az, el,
							  'row_%s_%s_az%03d_el%02d' % (rowname, cam_nm, az, el),
							  title, 'ROW %s -- %s' % (rowname, title))

	# object rows against the shipped terrain sheet
	for k, ttl in (('O1', 'the stored .lodi v8 horizon bytes (baseline)'),
				   ('O2', 'ceiling per vertex, no down-normal zero rule'),
				   ('O2p', 'ceiling per vertex, +16 u along the normal'),
				   ('O2m', 'ceiling per vertex, two-sided, the LOWER skyline'),
				   ('O5a', 'ceiling per vertex, 32 azimuth bins'),
				   ('O5b', 'ceiling per vertex, 64 azimuth bins'),
				   ('O5n', 'ceiling per vertex, 16 bins read NEAREST'),
				   ('O5m', 'ceiling per vertex, 16 bins read as the pair MAX')):
		run(k, k, 'T1', ttl)

	# O3: edge insertion, simulated on top of O2
	hzv_cache = {}

	def o3(Tthr):
		def f(gb, az):
			key = (id(gb), az)
			if key not in hzv_cache:
				hzv_cache[key] = H.read_bins(R.sub_bins(hv, 16), 16, az)
			hzv = hzv_cache[key]
			hzE = H.read_bins(R.sub_bins(hE, 16), 16, az)
			out, nsp = R.refine_hz(ob, gb, hzv, hzE, ekey, eorder, edges, elen, Tthr)
			return ('pix', out)
		return f
	run('O3a', 'O2', 'T1', 'ceiling + a vertex on every edge over 512 u', o3(512.0))
	run('O3b', 'O2', 'T1', 'ceiling + a vertex on every edge over 256 u', o3(256.0))

	# O4: the tier-3 ceiling -- a horizon per FACE TEXEL instead of per vertex
	if z2 is not None:
		import sweep2 as S2

		def o4(keyname, hname, snap):
			uk = z2[keyname]
			hb = z2[hname]

			def f(gb, az):
				e = H.read_bins(hb, 16, az)
				oi = gb.objfirst
				k = S2.pack_key(gb.tri[oi], gb.pos[oi], snap)
				i = np.searchsorted(uk, k)
				return ('pix', e[np.clip(i, 0, len(uk) - 1)])
			return f
		run('O4', 'O2', 'T1', 'tier-3 ceiling: a horizon per 64 u face texel', o4('k64', 'h_pix64', 64.0))
		run('O4x', 'O2', 'T1', 'tier-3 ceiling at 16 u texels (the instrument floor)',
			o4('k16', 'h_pix16', 16.0))

	# terrain rows against the stored object stream
	for k, ttl in (('T1', 'the stored role-7 terrain sheet (baseline)'),
				   ('T2a', 'ceiling terrain sheet, 32 u a texel (shipped size)'),
				   ('T2b', 'ceiling terrain sheet, 64 u a texel'),
				   ('T3a', 'ceiling terrain sheet, 32 bins'),
				   ('T3b', 'ceiling terrain sheet, 64 bins'),
				   ('T4', 'ceiling terrain sheet, bin = MAX over its 22.5 deg sector'),
				   ('T5', 'ceiling terrain sheet, NO spatial interpolation at read'),
				   ('T6a', 'ceiling terrain sheet, ray starts AT the ground'),
				   ('T6c', 'ceiling terrain sheet, ray starts 48 u up')):
		if k in T:
			run(k, 'O1', k, ttl)

	json.dump(table, open(LANE + '/rows.json', 'w'), indent=1)
	p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
	log.close()


if __name__ == '__main__':
	main()
