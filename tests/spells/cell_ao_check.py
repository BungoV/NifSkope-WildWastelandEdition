#!/usr/bin/env python3
"""lane AO1: the independent check of the cell view's ambient obscurance (tests/spells/cell_ao.sh).

It rebuilds the game's obscurance (docs/PRTP_PLAN.md "ambient obscurance") in numpy from NifSkope's opaque-pass dump (the view
normal and the linear depth per pixel) with its OWN constants, the game's INI defaults, and float32 arithmetic:
  E  the dump is a real surface: the dumped normal (mirrored to the game's axes) vs the normal of the surface
     rebuilt from the dumped depth, where the depth is smooth                       >= 80% with a dot over 0.8
     (and the same with the normal's x left unmirrored must NOT reach that: the stage can fail)
  A  the raw obscurance and its depth key, per half-size pixel, vs the dumped raw   >= 98% within 0.01
  R  the game's history rule (a frame at 0.95 or more replaces a history under 0.7): where it can fire, the
     dumped raw vs this checker's frame-by-frame run of that history      mean |d| <= 0.03, >= 85% within 0.08
  B  the bilateral blur of the DUMPED raw vs the dumped final                        >= 98% within 0.01
  C  what the picture got: the measure pass's linear light with the obscurance over the same pass without it
     (WW_CELL_AO_RED=off), on opaque cell-lit pixels, vs the bilinear upsample of the checker's OWN final
     (its own raw, its own blur)      >= 97% within 0.02, over >= 50% of the geometry and 20,000 px
  D  the obscurance is there at all: >= 2% of the geometry pixels under 0.9

USAGE  cell_ao_check.py <run dir>   (ao.bin + ao.bin.txt, on.hdr, off.hdr from cell_ao.sh)
"""

import os
import re
import sys

import numpy as np

F = np.float32
RADIUS, BIAS, INTENSITY = F(108.2), F(0.6), F(7.1)	# the game's INI defaults, the same for every cell
ANGLES = 8	# a still view: the per-frame angle in [0, pi) taken at 8 even steps, as the viewer approximates it
W_BLUR = (F(0.15317), F(0.444893), F(0.422649), F(0.392902))


def read_ao(path):
	raw = np.fromfile(path, dtype=np.uint8)
	W, H, hw, hh = np.frombuffer(raw[:16].tobytes(), dtype=np.int32)
	o = 16
	gb = np.frombuffer(raw[o:o + W * H * 16].tobytes(), dtype=np.float32).reshape(H, W, 4)
	o += W * H * 16
	rw = np.frombuffer(raw[o:o + hw * hh * 8].tobytes(), dtype=np.float32).reshape(hh, hw, 2)
	o += hw * hh * 8
	fn = np.frombuffer(raw[o:o + hw * hh * 4].tobytes(), dtype=np.float32).reshape(hh, hw)
	return int(W), int(H), int(hw), int(hh), gb[::-1].copy(), rw, fn	# the opaque pass top row first


def read_is(path):
	raw = np.fromfile(path, dtype=np.uint8)
	w, h = np.frombuffer(raw[:8].tobytes(), dtype=np.int32)
	px = np.frombuffer(raw[8:8 + w * h * 16].tobytes(), dtype=np.float32).reshape(h, w, 4)
	st = raw[8 + w * h * 16:8 + w * h * 17].reshape(h, w)
	return int(w), int(h), px[::-1], st[::-1]	# top row first


def echo_num(txt, key):
	m = re.search(r'\b' + key + r' (-?[0-9.]+)', txt)
	return float(m.group(1)) if m else None


def mips(z0):
	out = [z0]
	for m in range(1, 5):
		a = out[-1]
		h, w = a.shape
		nh, nw = max(h // 2, 1), max(w // 2, 1)
		ys = np.arange(nh) * 2
		xs = np.arange(nw) * 2
		y1 = np.minimum(ys + 1, h - 1)
		x1 = np.minimum(xs + 1, w - 1)
		ys = np.minimum(ys, h - 1)
		xs = np.minimum(xs, w - 1)
		q = np.minimum(np.minimum(a[np.ix_(ys, xs)], a[np.ix_(ys, x1)]), np.minimum(a[np.ix_(y1, xs)], a[np.ix_(y1, x1)]))
		out.append(q.astype(F))
	return out


def texel_of(n2q1, size, half):
	"""the texel a point sample at the centre of half-size pixel q reads: floor((2q+1) size / (2 half))"""
	t = (np.maximum(n2q1, 0).astype(np.int64) * size) // (2 * half)
	return np.clip(np.where(n2q1 < 0, -1, t), 0, size - 1)


def raw_ao(z0, nrm, hw, hh, p00, p11, radius):
	H, W = z0.shape
	zm = mips(z0)
	py, px = np.mgrid[0:hh, 0:hw]
	hs = np.array([hw, hh], dtype=F)
	pi = np.array([F(-2.0) / (F(hw) * F(p00)), F(-2.0) / (F(hh) * F(p11)), F(1.0) / F(p00), F(1.0) / F(p11)], dtype=F)
	z = z0[texel_of(2 * py + 1, H, hh), texel_of(2 * px + 1, W, hw)]
	cx = ((px.astype(F) + F(0.5)) * pi[0] + pi[2]) * z
	cy = ((py.astype(F) + F(0.5)) * pi[1] + pi[3]) * z
	cz = z
	ny_, nx_ = np.minimum(2 * py, H - 1), np.minimum(2 * px, W - 1)
	g = nrm[ny_, nx_]
	n = (-g[..., 0], g[..., 1], -g[..., 2])	# the game's axes: x left, y up, z forward
	d = np.clip(z * F(1.42857141e-4), F(0), F(1))
	hx, hy = px.astype(np.uint32), py.astype(np.uint32)
	hash_ang = (((hx * np.uint32(3)) ^ (hx * hy + hy)) * np.uint32(10)).astype(F)
	ndcx = (px.astype(F) + F(0.5)) / hs[0] * F(2) - F(1)
	ndcy = (py.astype(F) + F(0.5)) / hs[1] * F(2) - F(1)
	biasp = F(BIAS) + F(10) * np.maximum(d - F(0.3), F(0)) + F(5) * (ndcx * ndcx + ndcy * ndcy)
	r2 = F(radius) * F(radius)
	r6 = r2 * F(radius)
	r6 = r6 * r6
	with np.errstate(divide='ignore', invalid='ignore', over='ignore'):
		ssdisk = F(radius) * F(100) / z
	near = d <= F(0.5)
	per = []
	for k in range(ANGLES):
		rnd = np.where(near, F((k + 0.5) * np.pi / ANGLES), F(0)).astype(F)
		rot = (rnd + hash_ang).astype(F)
		s = np.zeros_like(z)
		for i in range(5):
			ssr = ((ssdisk * F(i + 0.5)).astype(F) * F(0.2)).astype(F)
			ang = (np.float64(F(i + 0.5)) * np.float64(F(2.512)) + rot.astype(np.float64)).astype(F)
			ad = ang.astype(np.float64)
			a = (ad - 2.0 * np.pi * np.floor(ad / (2.0 * np.pi))).astype(F)
			with np.errstate(divide='ignore', invalid='ignore'):
				m = np.clip(np.floor(np.log2(ssr)) - 3, 0, 4)
			m = np.where(np.isfinite(m), m, 0).astype(np.int64)
			ox = np.trunc(ssr * np.cos(a).astype(F))
			oy = np.trunc(ssr * np.sin(a).astype(F))
			ox = np.where(np.isfinite(ox), np.clip(ox, -1e6, 1e6), 0).astype(np.int64)
			oy = np.where(np.isfinite(oy), np.clip(oy, -1e6, 1e6), 0).astype(np.int64)
			qx, qy = px + ox, py + oy
			qz = np.empty_like(z)
			for lv in range(5):
				sel = m == lv
				if not sel.any():
					continue
				mh, mw = zm[lv].shape
				qz[sel] = zm[lv][texel_of(2 * qy[sel] + 1, mh, hh), texel_of(2 * qx[sel] + 1, mw, hw)]
			vx = ((qx.astype(F) + F(0.5)) * pi[0] + pi[2]) * qz - cx
			vy = ((qy.astype(F) + F(0.5)) * pi[1] + pi[3]) * qz - cy
			vz = qz - cz
			vv = vx * vx + vy * vy + vz * vz
			f = np.maximum(r2 - vv, F(0))
			with np.errstate(invalid='ignore', over='ignore'):
				t = f * f * f * np.maximum((vx * n[0] + vy * n[1] + vz * n[2] - biasp) / (vv + F(0.01)), F(0))
			s = s + np.where(np.isfinite(t), t, 0).astype(F)
		per.append(np.maximum(F(1) - s * F(INTENSITY) / r6, F(0)).astype(F))
	per = np.stack(per)	# one frame's value per angle (past depth 3500 the angle is fixed: all the same)
	hi = np.floor(d * F(256))
	key = (hi * F(0.00390625) * F(0.996108949) + (d * F(256) - hi) * F(0.00389105058)).astype(F)
	return per, key, z


def still_view(per):
	"""what the game's history holds on a still view: each frame one of the angles, h = A + 0.99 (h - A), and
	h = A when A >= 0.95 and h < 0.7. Run frame by frame where that rule can fire; the plain mean elsewhere."""
	mean = (per.sum(0, dtype=F) / F(per.shape[0])).astype(F)
	fires = (mean < F(0.7)) & (per >= F(0.95)).any(0)
	out = mean.copy()
	idx = np.argwhere(fires)
	if len(idx):
		a = per[:, idx[:, 0], idx[:, 1]].astype(np.float64)
		cols = np.arange(a.shape[1])
		rng = np.random.default_rng(20261001)
		h = a.mean(0)
		acc = np.zeros_like(h)
		for f in range(7000):
			A = a[rng.integers(0, a.shape[0], size=a.shape[1]), cols]
			h = np.where((A >= 0.95) & (h < 0.7), A, A + 0.99 * (h - A))
			if f >= 1000:
				acc += h
		out[idx[:, 0], idx[:, 1]] = (acc / 6000.0).astype(F)
	return out, fires


def surface_normals(gb, p00, p11):
	"""the dot of the dumped normal (GL view axes) with the normal of the surface the dumped depth describes
	(the game's axes: x left, y up, z forward; a visible surface faces the eye), on smooth-depth pixels;
	returned for the mirrored normal (-x, y, -z) and for the control with x left as it is (x, y, -z)"""
	H, W = gb.shape[:2]
	z = gb[..., 3].astype(np.float64)
	py, px = np.mgrid[0:H, 0:W]
	P = np.stack([(1 - 2 * (px + 0.5) / W) / p00 * z, (1 - 2 * (py + 0.5) / H) / p11 * z, z], -1)
	zc = z[1:-1, 1:-1]
	smooth = zc < 1e5
	for nb in (z[1:-1, 2:], z[1:-1, :-2], z[2:, 1:-1], z[:-2, 1:-1]):
		smooth &= np.abs(nb - zc) < 0.02 * zc
	g = np.cross(P[1:-1, 2:] - P[1:-1, :-2], P[2:, 1:-1] - P[:-2, 1:-1])
	g /= np.maximum(np.linalg.norm(g, axis=-1, keepdims=True), 1e-20)
	g *= -np.sign((g * P[1:-1, 1:-1]).sum(-1, keepdims=True))
	n = gb[1:-1, 1:-1, :3].astype(np.float64)
	real = (g * (n * np.array([-1.0, 1.0, -1.0]))).sum(-1)[smooth]
	ctrl = (g * (n * np.array([1.0, 1.0, -1.0]))).sum(-1)[smooth]
	return real, ctrl


def blur1(a, key, axis):
	hh, hw = a.shape
	s = a * W_BLUR[0]
	ws = np.full_like(a, W_BLUR[0])
	for j in range(1, 4):
		for sg in (-1, 1):
			sh = 2 * j * sg
			ta = np.zeros_like(a)
			tk = np.zeros_like(key)
			if axis == 1:
				if sh > 0:
					ta[:, :-sh], tk[:, :-sh] = a[:, sh:], key[:, sh:]
				else:
					ta[:, -sh:], tk[:, -sh:] = a[:, :sh], key[:, :sh]
			else:
				if sh > 0:
					ta[:-sh], tk[:-sh] = a[sh:], key[sh:]
				else:
					ta[-sh:], tk[-sh:] = a[:sh], key[:sh]
			w = np.maximum(F(1) - np.abs(tk - key) * F(2000), F(0)) * W_BLUR[j]
			s = s + ta * w
			ws = ws + w
	out = s / (ws + F(0.0001))
	return np.where(key == F(1), F(0), out).astype(F)


def blur(a, key):
	return blur1(blur1(a, key, 1), key, 0)


def bilinear(t, W, H):
	hh, hw = t.shape
	sx = (np.arange(W, dtype=np.float64) + 0.5) / W * hw - 0.5
	sy = (np.arange(H, dtype=np.float64) + 0.5) / H * hh - 0.5
	x0, y0 = np.floor(sx).astype(int), np.floor(sy).astype(int)
	fx, fy = sx - x0, sy - y0
	x1, y1 = np.clip(x0 + 1, 0, hw - 1), np.clip(y0 + 1, 0, hh - 1)
	x0, y0 = np.clip(x0, 0, hw - 1), np.clip(y0, 0, hh - 1)
	top = t[np.ix_(y0, x0)] * (1 - fx) + t[np.ix_(y0, x1)] * fx
	bot = t[np.ix_(y1, x0)] * (1 - fx) + t[np.ix_(y1, x1)] * fx
	return top * (1 - fy)[:, None] + bot * fy[:, None]


def main():
	run = sys.argv[1]
	txt = open(os.path.join(run, 'ao.bin.txt')).read()
	print('echo ' + txt.strip())
	p00, p11 = echo_num(txt, 'p00'), echo_num(txt, 'p11')
	p20, p21 = echo_num(txt, 'p20'), echo_num(txt, 'p21')
	if p00 is None or p11 is None:
		print('ao FAIL: no projection in the echo')
		return 1
	ok = True
	sym = abs(p20 or 0) < 1e-4 and abs(p21 or 0) < 1e-4
	print('frustum symmetric (the game\'s reconstruction assumes it): %s  p20 %.6f p21 %.6f' % ('yes' if sym else 'NO', p20 or 0, p21 or 0))
	ok &= sym
	W, H, hw, hh, gb, draw_, dfin = read_ao(os.path.join(run, 'ao.bin'))
	z0 = gb[..., 3].astype(F)
	per, key, zc = raw_ao(z0, gb[..., :3].astype(F), hw, hh, p00, p11, RADIUS)
	A, fires = still_view(per)
	geo = zc < F(1e5)
	ng = int(geo.sum())
	print('geometry half-size pixels: %d of %d' % (ng, hw * hh))
	if ng < 2000:
		print('ao FAIL: too few geometry pixels')
		return 1

	# E: the dumped normals belong to the surface the dumped depth describes
	real, ctrl = surface_normals(gb, p00, p11)
	fe = float((real > 0.8).mean()) if real.size else 0.0
	fx = float((ctrl > 0.8).mean()) if ctrl.size else 1.0
	pe = real.size >= 20000 and fe >= 0.80 and fx < 0.80
	print('E %s  dumped normal vs the depth\'s own surface, dot > 0.8: %.1f%% of %d smooth px (median dot %.3f); '
		'control with x unmirrored: %.1f%% (must stay under 80)' % ('PASS' if pe else 'FAIL', 100 * fe, real.size,
		float(np.median(real)) if real.size else 0, 100 * fx))
	ok &= pe

	# A: the raw obscurance and its key (where the history rule cannot fire)
	plain = geo & ~fires
	ea = np.abs(draw_[..., 0] - A)[plain]
	ek = np.abs(draw_[..., 1] - key)[geo]
	fa = float((ea <= 0.01).mean())
	fk = float((ek <= 1e-5).mean())
	pa = fa >= 0.98 and fk >= 0.98
	print('A %s  raw obscurance within 0.01: %.2f%% (mean |d| %.4f, max %.3f); key within 1e-5: %.2f%%'
		% ('PASS' if pa else 'FAIL', 100 * fa, float(ea.mean()), float(ea.max()), 100 * fk))
	ok &= pa

	# R: the history rule, where it can fire
	nr = int((geo & fires).sum())
	if nr >= 100:
		er = np.abs(draw_[..., 0] - A)[geo & fires]
		mean_plain = (per.sum(0, dtype=F) / F(per.shape[0]))[geo & fires]
		pr = float(er.mean()) <= 0.03 and float((er <= 0.08).mean()) >= 0.85
		print('R %s  history rule on %d px (%.2f%%): dumped raw vs the frame-by-frame run, mean |d| %.4f, within 0.08: '
			'%.1f%%; the run sits %.3f above the plain mean there' % ('PASS' if pr else 'FAIL', nr, 100.0 * nr / ng,
			float(er.mean()), 100 * float((er <= 0.08).mean()), float((A[geo & fires] - mean_plain).mean())))
		ok &= pr
	else:
		print('R n/a   the history rule can fire on only %d px here (needs 100 to judge)' % nr)

	# B: the blur of the dumped raw
	bfin = blur(draw_[..., 0].astype(F), draw_[..., 1].astype(F))
	eb = np.abs(bfin - dfin)[geo]
	fb = float((eb <= 0.01).mean())
	moved = float((np.abs(bfin - draw_[..., 0])[geo] > 0.01).mean())
	pb = fb >= 0.98
	print('B %s  blur within 0.01: %.2f%% (mean |d| %.4f); the blur moves %.1f%% of the raw by > 0.01'
		% ('PASS' if pb else 'FAIL', 100 * fb, float(eb.mean()), 100 * moved))
	ok &= pb

	# D: the obscurance is there
	own = blur(A, key)
	dark = float((own[geo] < 0.9).mean())
	pd = dark >= 0.02
	print('D %s  own final: mean %.4f over geometry, %.1f%% under 0.9, %.1f%% under 0.7'
		% ('PASS' if pd else 'FAIL', float(own[geo].mean()), 100 * dark, 100 * float((own[geo] < 0.7).mean())))
	ok &= pd

	# C: what the picture got
	w1, h1, on, st = read_is(os.path.join(run, 'on.hdr'))
	w2, h2, off, st2 = read_is(os.path.join(run, 'off.hdr'))
	if (w1, h1) != (W, H) or (w2, h2) != (W, H):
		print('C FAIL: the measure dumps are %dx%d / %dx%d, the obscurance %dx%d' % (w1, h1, w2, h2, W, H))
		return 1
	up = bilinear(own.astype(np.float64), W, H)
	lon = on[..., :3].astype(np.float64).sum(-1)
	loff = off[..., :3].astype(np.float64).sum(-1)
	sel = (st == 1) & (st2 == 1) & (loff > 1e-4) & np.isfinite(lon) & np.isfinite(loff) & (z0 < F(1e5))
	nsel = int(sel.sum())
	ratio = lon[sel] / loff[sel]
	ec = np.abs(ratio - up[sel])
	fc = float((ec <= 0.02).mean()) if nsel else 0.0
	darker = float((up[sel] < 0.98).mean()) if nsel else 0.0
	share = nsel / float(max(int((z0 < F(1e5)).sum()), 1))	# of the geometry: a view may be mostly void
	pc = nsel >= 20000 and share >= 0.50 and fc >= 0.97
	print('C %s  applied (with / without) vs own final upsampled, within 0.02: %.2f%% of %d px (%.0f%% of the geometry, '
		'needs 50; mean |d| %.4f); %.1f%% of them expect < 0.98' % ('PASS' if pc else 'FAIL', 100 * fc, nsel, 100 * share,
		float(ec.mean()) if nsel else 0, 100 * darker))
	ok &= pc
	print('ao %s' % ('PASS' if ok else 'FAIL'))
	return 0 if ok else 1


if __name__ == '__main__':
	sys.exit(main())
