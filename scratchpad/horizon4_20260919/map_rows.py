#!/usr/bin/env python3
"""HORIZON4 rows M1-M3 -- the runtime far shadow map, scored like every other row."""
import json
import numpy as np
import os
import sys
import time

import h4core as H
import h4map as M
import rows as R
import shade as SH

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                          # noqa: E402

LANE = H.LANE
IMG = LANE + '/images'
BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
		'/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')


def main():
	os.makedirs(IMG, exist_ok=True)
	log = open(LANE + '/map_rows.log', 'w')

	def p(*a):
		s = ' '.join(str(x) for x in a)
		print(s)
		log.write(s + '\n')
		log.flush()

	T0 = time.time()
	ter, ob, sh = H.load_scene()
	gbs = H.gbuffers(ter, ob)
	T = ND.read_lodi(BAKE + '.lodi')
	gvert = M.group_of_vertex(ob, T)
	gtri = gvert[ob.tri[:, 0]]
	p('shadow identity: %d distinct groups over %d placements, %d drawn triangles'
	  % (len(np.unique(gtri)), len(T['instances']), len(ob.tri)))

	table = []
	for texel, name in ((64.0, 'M1'), (32.0, 'M2'), (16.0, 'M3')):
		for (az, el) in R.SUNS:
			mp = M.ShadowMap(ter, ob, gtri, az, el, texel, verbose=True)
			p('  %s az%3.0f el%2.0f  map %d x %d = %s texels, %.2f MB at 4 B depth + 2 B id'
			  % (name, az, el, mp.nv, mp.ns, '{:,}'.format(mp.nv * mp.ns),
				 mp.nv * mp.ns * 6 / 1e6))
			for cam_nm in R.CAMS:
				cam, gb = gbs[cam_nm]
				truth = H.truth_lit(ter, ob, gb, az, el)
				m = gb.kind != 0
				ident = np.full(len(gb.kind), M.TERRAIN_ID, dtype=np.int64)
				oi = gb.objfirst
				ident[oi] = gvert[ob.tri[gb.tri[oi], 0]]
				dark, mi, nearer, has = mp.query(gb.pos[m], gb.nrm[m], ident[m])
				darkNI, _, _, _ = mp.query(gb.pos[m], gb.nrm[m], ident[m], use_identity=False)
				lit = np.ones(len(gb.kind), dtype=bool)
				lit[m] = ~dark
				litNI = np.ones(len(gb.kind), dtype=bool)
				litNI[m] = ~darkNI
				nod = np.zeros(len(gb.kind), dtype=bool)
				nod[m] = ~has
				st = {}
				dec = m & ~nod
				for nm2, mm in (('all', dec), ('terrain', dec & gb.terfirst),
								('objects', dec & gb.objfirst)):
					k = int(mm.sum())
					st[nm2] = (100.0 * float((truth[mm] != lit[mm]).sum()) / k if k else float('nan'), k)
					st[nm2 + '_noid'] = (100.0 * float((truth[mm] != litNI[mm]).sum()) / k
										 if k else float('nan'))
				# what excluding self-shadow costs: truth-dark object pixels that
				# the DEPTH test darkens but the IDENTITY test lets back to lit
				od = dec & gb.objfirst & ~truth
				n_od = int(od.sum())
				lost = int((od & ~litNI & lit).sum())
				st['selfloss'] = (100.0 * lost / max(1, n_od), n_od, lost)
				st['nodata'] = (100.0 * int(nod.sum()) / max(1, int(m.sum())), int(nod.sum()))
				p('%-3s %-7s az%3.0f el%2.0f  ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%'
				  '   (no identity: obj %6.2f%%)  self-shadow refused on %5.2f%% of %s truth-dark obj px'
				  % (name, cam_nm, az, el, st['all'][0], st['terrain'][0], st['objects'][0],
					 st['objects_noid'], st['selfloss'][0], '{:,}'.format(n_od)))
				table.append(dict(row=name, texel=texel, cam=cam_nm, az=az, el=el,
								  all=st['all'][0], terrain=st['terrain'][0],
								  objects=st['objects'][0], objects_noid=st['objects_noid'],
								  selfloss=st['selfloss'][0], selfloss_px=lost,
								  truthdark_obj=n_od, nodata=st['nodata'][0],
								  texels=int(mp.nv * mp.ns)))
				if (cam_nm, az, el) in {(c, a, e) for c, a, e in R.PIC}:
					hz = np.where(m, np.where(lit, -1.0, 90.0), np.nan)
					hz[nod] = np.nan
					R.picture(gb, cam, truth, hz, lit, az, el,
							  'row_%s_%s_az%03d_el%02d' % (name, cam_nm, az, el),
							  'runtime far shadow map, %.0f u a texel, identity = the v7 group'
							  % texel,
							  'ROW %s -- ortho shadow map at %.0f u a texel; a receiver is dark when '
							  'the map is nearer AND its identity differs' % (name, texel))
			del mp
	json.dump(table, open(LANE + '/map_rows.json', 'w'), indent=1)
	p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
	log.close()


if __name__ == '__main__':
	main()
