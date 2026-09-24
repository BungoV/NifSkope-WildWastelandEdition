#!/usr/bin/env python3
"""HORIZON4 -- the second, small sweep: 16 azimuths only.

  * T6, the receiver height offset: the terrain sheet's 32-u texels cast again
    with the ray starting AT the ground (rise 0) and 48 u above it, against the
    12 u the main sweep used (which is `SunShadow.lit`'s own bias, so the
    ceiling and the truth panel start from the same place).
  * O4, the tier-3 ceiling: receivers on the FACE rather than on the vertices.
    A receiver is one (triangle, lattice cell) pair taken from the four camera
    G-buffers, at 64 u -- the texel size HORIZON3 s1(b) priced -- and at 16 u,
    which is the finest the instrument can mean anything at, because the LEFT
    panel's own shadow ray has a 16-unit footprint.  Asking for a finer texel
    would be measuring the simulator, not the representation.
"""
import numpy as np
import os
import time

import h4core as H

A16 = 16
OUT = H.LANE + '/ceiling2.npz'


BIAS = np.int64(8192)


def pack_key(tri, pos, snap):
	"""(triangle, lattice cell) as ONE int64, so a reader can `searchsorted`
	the same key without carrying a 4-column table about."""
	c = np.floor(np.asarray(pos) / snap).astype(np.int64) + BIAS
	return ((np.asarray(tri, dtype=np.int64) << np.int64(45))
			| (c[:, 0] << np.int64(30)) | (c[:, 1] << np.int64(15)) | c[:, 2])


def pixel_receivers(gbs, snap):
	"""One receiver per (triangle, lattice cell), at the mean hit position."""
	keys, pos = [], []
	for nm, (cam, gb) in gbs.items():
		oi = gb.objfirst
		if not oi.any():
			continue
		keys.append(pack_key(gb.tri[oi], gb.pos[oi], snap))
		pos.append(gb.pos[oi])
	K = np.concatenate(keys)
	P = np.concatenate(pos)
	uk, inv = np.unique(K, return_inverse=True)
	acc = np.zeros((len(uk), 3))
	cnt = np.zeros(len(uk))
	np.add.at(acc, inv, P)
	np.add.at(cnt, inv, 1.0)
	return acc / cnt[:, None], uk, inv


def main():
	log = open(H.LANE + '/sweep2.log', 'w')

	def p(*a):
		s = ' '.join(str(x) for x in a)
		print(s)
		log.write(s + '\n')
		log.flush()

	T0 = time.time()
	ter, ob, sh = H.load_scene()
	gbs = H.gbuffers(ter, ob)
	X, Y, nx, ny, ox, oy = H.texel_grid(32.0)
	ter32 = np.stack([X.ravel(), Y.ravel(), ter.atf(X, Y).ravel()], axis=1)
	r64, k64, i64 = pixel_receivers(gbs, 64.0)
	r16, k16, i16 = pixel_receivers(gbs, 16.0)
	p('receivers: ter32 %d  pix64 %d  pix16 %d' % (len(ter32), len(r64), len(r16)))
	out = dict(h_ter32_r0=np.zeros((len(ter32), A16), np.uint8),
			   h_ter32_r48=np.zeros((len(ter32), A16), np.uint8),
			   h_pix64=np.zeros((len(r64), A16), np.uint8),
			   h_pix16=np.zeros((len(r16), A16), np.uint8))
	for j in range(A16):
		az = j * (360.0 / A16)
		t0 = time.time()
		sg = H.ShearGrid(ter, ob, az)
		a, b = sg.horizon_multi(sg.index(ter32), [0.0, 48.0])
		out['h_ter32_r0'][:, j] = H.quantise(a)
		out['h_ter32_r48'][:, j] = H.quantise(b)
		out['h_pix64'][:, j] = H.quantise(sg.horizon(sg.index(r64)))
		out['h_pix16'][:, j] = H.quantise(sg.horizon(sg.index(r16)))
		p('az %6.3f  %5.1fs  elapsed %.1f min' % (az, time.time() - t0, (time.time() - T0) / 60.0))
		del sg
	np.savez_compressed(OUT, k64=k64, k16=k16, **out)
	p('wrote %s %.1f MB  TOTAL %.1f min'
	  % (OUT, os.path.getsize(OUT) / 1e6, (time.time() - T0) / 60.0))
	log.close()


if __name__ == '__main__':
	main()
