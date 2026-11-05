"""lane FIX1101A (LATE ITEM 14, bungo's "banding on shadows"): the unit twin of the near contact march's reach.
A synthetic wall + a power-line wire in front of it (BlackWireSpline XBSD thickness 2.0-2.76 u), ray cast into a linear
depth / normal g-buffer (1920x1045, bottom row first); the cell view's NEAR contact tier on it (res/shaders/
ww_contact.frag under SHADOWPAR1: half-res mask, 192 samples, 0.5 % thickness, 1.5 u floor, 4 lanes, contrast 4; the
march of notes/shadowband1/tools/contact_twin.py in numpy) with the reach the tree ships (kContactReach, read from
src/features/shadows/shadowset.cpp; the shader's reach break must be there), the cascade factor (ussVogel on the Ultra + PAR numbers:
c0 1.239 u texel to 1750 u, c1 3.561 u, world radius 2.8 / 10.5 u), composed by min, along the wire's true shadow.
Gates (--gate far, a wire 85 u off the wall at ~2700 u): the composed line's stair (darkness centroid across the line,
high-passed, half p2p) < 1 px and its darkness CV < 0.1. (--gate near, a wire 6 u off): the contact tier still marks
>= 90 % of the line (contact shadows keep their job). --red: today's march (no reach) -- the far gate must FAIL.
usage: python -I contactreach_twin.py <out prefix> --root <tree> --gate far|near [--red] [--png]"""
import argparse
import math
import os
import re
import sys

import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument('out')
ap.add_argument('--root', required=True)
ap.add_argument('--gate', choices=('far', 'near'), default='far')
ap.add_argument('--red', action='store_true')
ap.add_argument('--png', action='store_true')
ap.add_argument('--dist', type=float, default=2000.0)	# wall distance along the view (u)
ap.add_argument('--yaw', type=float, default=50.0)	# the wall's normal turned from the view axis (deg)
ap.add_argument('--off', type=float, default=85.0)	# the wire in front of the wall (u)
ap.add_argument('--r', type=float, default=1.2)		# wire radius (XBSD thickness / 2, MEASURED 1.0 .. 1.38)
ap.add_argument('--tilt', type=float, default=-12.0)	# the wire's slope along the wall (deg)
ap.add_argument('--elev', type=float, default=55.0)	# sun elevation (deg)
ap.add_argument('--saz', type=float, default=160.0)	# sun azimuth from the view axis (deg): n.L 0.196 on the wall
ap.add_argument('--S', type=int, default=2)		# mask scale
ap.add_argument('--n', type=int, default=192)		# samples
ap.add_argument('--fov', type=float, default=50.0)
ap.add_argument('--thick', type=float, default=0.005)
ap.add_argument('--floor', type=float, default=1.5)
ap.add_argument('--reach', type=float, default=0.0)	# the fix: the near march's world reach inside the cascades (0 = today)
ap.add_argument('--reachk', type=float, default=0.0)	# ... plus this share of the receiver's depth
ap.add_argument('--D', type=float, default=14000.0)	# the cascades' reach (the hold's PAR distance, MEASURED in the path logs)
a = ap.parse_args()
if a.gate == 'near':
	a.off = 6.0
# the reach the tree ships, and the shader's use of it
src = open(os.path.join(a.root, 'src', 'features', 'shadows', 'shadowset.cpp'), encoding='utf-8', errors='replace').read()
mk = re.search(r'constexpr float kContactReach = ([0-9.]+)f;', src)
frag = open(os.path.join(a.root, 'res', 'shaders', 'features', 'shadows', 'ww_contact.frag'), encoding='utf-8', errors='replace').read()
shaderReach = 'sssReach.x > 0.0 && distance( sssViewPos( q, lin0 * dist0 / dist ), pv0 ) > reachEff' in frag
if not a.red:
	if not mk or not shaderReach:
		print('FAIL the tree has no near-march reach (kContactReach %s, shader break %s)' % (bool(mk), shaderReach))
		sys.exit(1)
	a.reach = float(mk.group(1))
else:
	a.reach = 0.0
print('reach %.1f u (%s)' % (a.reach, 'red: the whole march' if a.red else 'src/features/shadows/shadowset.cpp kContactReach'))

W, H = 1920, 1045
ty = math.tan(math.radians(a.fov / 2))
tx = ty * W / H
fwd = np.array([0.0, 1.0, 0.0])
right = np.array([1.0, 0.0, 0.0])
up = np.array([0.0, 0.0, 1.0])
# the wall: through C = fwd * dist, normal toward the camera turned by yaw about up
yaw = math.radians(a.yaw)
nW = np.array([-math.sin(yaw), -math.cos(yaw), 0.0])
C = fwd * a.dist + up * 300.0
uW = np.cross(up, nW)
uW /= np.linalg.norm(uW)	# along the wall, horizontal
tl = math.radians(a.tilt)
wdir = uW * math.cos(tl) + up * math.sin(tl)
P0 = C + nW * a.off		# the wire's axis point
# the sun (toward it)
el, az = math.radians(a.elev), math.radians(a.saz)
Ls = np.array([math.sin(az) * math.cos(el), math.cos(az) * math.cos(el), math.sin(el)])
print('n.L of the wall %.3f (stretch %.1f)' % (nW @ Ls, 1 / max(abs(nW @ Ls), 1e-3)))

py, px = np.mgrid[0:H, 0:W].astype(np.float64) + 0.5
nx = px / W * 2 - 1
ny = py / H * 2 - 1	# bottom row first
D = (fwd[None, None, :] + nx[..., None] * tx * right + ny[..., None] * ty * up)
# wall
t_w = (C @ nW) / (D @ nW)
t_w = np.where(t_w > 0, t_w, np.inf)
# wire (infinite cylinder): |(t D - P0) x wdir| = r
dperp = D - (D @ wdir)[..., None] * wdir
p0perp = -P0 + (P0 @ wdir) * wdir	# (0 - P0) perp
A = (dperp * dperp).sum(-1)
B = 2 * (dperp * p0perp).sum(-1)
Cc = (p0perp @ p0perp) - a.r * a.r
disc = B * B - 4 * A * Cc
t_c = np.where(disc >= 0, (-B - np.sqrt(np.maximum(disc, 0))) / (2 * A), np.inf)
t_c = np.where(t_c > 0, t_c, np.inf)
t = np.minimum(t_w, t_c)
hitc = t_c < t_w
lin = np.where(np.isfinite(t), t * (D @ fwd), 1e6).astype(np.float32)
Pt = D * t[..., None]
nrm_c = Pt - P0 - ((Pt - P0) @ wdir)[..., None] * wdir
nrm_c /= np.maximum(np.linalg.norm(nrm_c, axis=-1, keepdims=True), 1e-9)
nrm = np.where(hitc[..., None], nrm_c, nW[None, None, :])
# view-space normals as the g-buffer holds them (x right, y up, z toward the eye)
nrmv = np.stack([nrm @ right, nrm @ up, -(nrm @ fwd)], -1).astype(np.float32)

lv = Ls
sunView = np.array([lv @ right, lv @ up, -(lv @ fwd)])
cw = lv @ fwd
ndcX, ndcY = (lv @ right) / tx / cw, (lv @ up) / ty / cw


def march(lin, nrm, S, lx, ly, n, floor_u, thickness=0.005, bilin=0.02, contrast=4.0, planeBias=12.0, rampMin=0.125,
		  evidence=4.0, edgeBand=16.0, normalReject=0.1):
	"""notes/shadowband1/tools/contact_twin.py march(), verbatim but for the reach cap (0 there)"""
	Hf, Wf = lin.shape
	MW, MH = (Wf + S - 1) // S, (Hf + S - 1) // S

	def at(qx, qy):
		fx = np.floor(qx * S).astype(np.int64)
		fy = np.floor(qy * S).astype(np.int64)
		ok = (fx >= 0) & (fy >= 0) & (fx < Wf) & (fy < Hf)
		l = np.full(qx.shape, 1e6, np.float32)
		l[ok] = lin[fy[ok], fx[ok]]
		nn = np.zeros(qx.shape + (3,), np.float32)
		nn[ok] = nrm[fy[ok], fx[ok]]
		return l, ok & (l < 9e5), nn
	py, px = np.mgrid[0:MH, 0:MW].astype(np.float64)
	px += 0.5
	py += 0.5
	lin0, v0, _ = at(px, py)
	dx, dy = px - lx, py - ly
	majorX = np.abs(dx) > np.abs(dy)
	dist0 = np.where(majorX, np.abs(dx), np.abs(dy))
	act = v0 & (lin0 > 0) & (dist0 >= 1)
	direction = -1.0 if cw > 0 else 1.0
	sx = dx / np.maximum(dist0, 1e-9) * direction
	sy = dy / np.maximum(dist0, 1e-9) * direction
	lanes = np.ones((4,) + px.shape)
	la4, va4, _ = at(px + sx * 4, py + sy * 4)
	lr4, vb4, _ = at(px - sx * 4, py - sy * 4)
	plane = va4 & vb4 & (np.abs(la4 - lr4) < 0.25 * lin0)
	invSlope = np.where(plane, (1.0 / la4 - 1.0 / lr4) / 8.0, 0.0)
	hard = np.ones(px.shape)
	support = np.zeros(px.shape)
	exitK = np.full(px.shape, n, np.int64)
	alive = act.copy()

	def viewpos(qx, qy, z):
		nx = qx * S / Wf * 2 - 1
		ny = qy * S / Hf * 2 - 1
		return np.stack([nx * tx * z, ny * ty * z, z], -1)
	P0v = viewpos(px, py, lin0)
	# the fix: inside the cascades' reach the march stops past reach + reachk x depth (faded out toward D as the far
	# tier's farcomp fades: weight 1 - s^4, s = (|P| / D)^2)
	reachEff = None
	if a.reach > 0:
		s_ = np.clip((P0v ** 2).sum(-1) / (a.D * a.D), 0, 1)
		wgt = 1.0 - s_ ** 4
		cap = a.reach + a.reachk * lin0
		reachEff = np.where(wgt > 0, cap / np.maximum(wgt, 1e-6), np.inf)
	for k in range(1, n + 1):
		dist = dist0 + k * direction
		alive &= dist >= 1
		qx, qy = px + sx * k, py + sy * k
		rayLin = lin0 * dist0 / np.maximum(dist, 1e-9)
		if reachEff is not None:
			alive &= np.linalg.norm(viewpos(qx, qy, rayLin) - P0v, axis=-1) <= reachEff
		off = (qx * S < 0) | (qy * S < 0) | (qx * S >= Wf) | (qy * S >= Hf)
		exitK = np.where((exitK == n) & off & alive, k, exitK)
		minor = np.where(majorX, qy, qx)
		f = minor - np.floor(minor) - 0.5
		ox = np.where(majorX, 0.0, np.where(f > 0, 1.0, -1.0))
		oy = np.where(majorX, np.where(f > 0, 1.0, -1.0), 0.0)
		la, va, na = at(qx, qy)
		lb, vb, _ = at(qx + ox, qy + oy)
		use = alive & va
		edge = (~vb) | (np.abs(la - lb) > la * bilin)
		ls = np.where(edge, la, la + (lb - la) * np.abs(f))
		thick = np.maximum(floor_u, thickness * rayLin)
		dd = np.abs((rayLin - ls) / thick - 1.0)
		planeLin = 1.0 / np.maximum(1.0 / lin0 + invSlope * k, 1e-9)
		dd = np.where(plane & (ls > planeLin - planeBias * np.clip(k / 32.0, rampMin, 1.0)), 1e10, dd)
		dd = np.where((dd < 1) & (np.abs(na @ sunView) <= normalReject), 1e10, dd)
		fadeOut = (np.clip(1.0 - (n - k) / 16.0, 0, 1) * 0.75) if k > n - 16 else 0.0
		if k <= 4:
			hard = np.where(use, np.minimum(hard, dd), hard)
			support = np.where(use, support + np.clip((1.0 - dd) * contrast, 0, 1), support)
		dd = dd + fadeOut
		lanes[k & 3] = np.where(use, np.minimum(lanes[k & 3], dd), lanes[k & 3])
	m = np.clip(lanes * contrast + (1 - contrast), 0, 1).mean(axis=0)
	h = 1.0 + (np.clip(hard * contrast + (1 - contrast), 0, 1) - 1.0) * np.clip(support / evidence, 0, 1)
	m = np.minimum(h, m)
	e = np.clip(exitK * S / edgeBand, 0, 1)
	m = 1.0 + (m - 1.0) * e * e * (3 - 2 * e)
	return np.where(act, m, 1.0)


S = a.S
MW, MH = (W + S - 1) // S, (H + S - 1) // S
mask = march(lin, nrmv, S, (ndcX * 0.5 + 0.5) * MW, (ndcY * 0.5 + 0.5) * MH, a.n, a.floor, thickness=a.thick)
# upsample (GL_LINEAR) to full res
yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
mx, my = (xx + 0.5) / S - 0.5, (yy + 0.5) / S - 0.5
i0 = np.clip(np.floor(mx).astype(int), 0, MW - 2)
j0 = np.clip(np.floor(my).astype(int), 0, MH - 2)
fx, fy = np.clip(mx - i0, 0, 1), np.clip(my - j0, 0, 1)
full = (mask[j0, i0] * (1 - fx) * (1 - fy) + mask[j0, i0 + 1] * fx * (1 - fy) + mask[j0 + 1, i0] * (1 - fx) * fy
		+ mask[j0 + 1, i0 + 1] * fx * fy)
# the analytic shadow line: wire points projected along -Ls onto the wall, then to the screen
ts = np.linspace(-3000, 3000, 6001)
Wp = P0[None, :] + ts[:, None] * wdir[None, :]
s_ = ((C - Wp) @ nW) / (-Ls @ nW)
Sp = Wp - Ls[None, :] * s_[:, None]
z = Sp @ fwd
sxp = ((Sp @ right) / z / tx * 0.5 + 0.5) * W
syp = ((Sp @ up) / z / ty * 0.5 + 0.5) * H
okp = (sxp > 20) & (sxp < W - 20) & (syp > 20) & (syp < H - 20)
sxp, syp = sxp[okp], syp[okp]
# the cascade factor in a window round the line's middle (ww_sunshadow.glsl ussVogel on the hold's Ultra + PAR numbers,
# MEASURED in the path-log echo: c0 texel 1.239 to 1750 u, c1 3.561 to 5687.5, world radius 2.8 / 10.5 u; taps
# 32 (R/3)^2), the wire rasterised the GL way into the light-space grid
xc, yc = int(np.median(sxp)), int(np.median(syp))
X0, X1, Y0, Y1 = max(xc - 200, 0), min(xc + 200, W), max(yc - 120, 0), min(yc + 120, H)
Lt = -Ls
e1 = np.cross(Lt, np.array([0.0, 1.0, 0.0]))
e1 /= np.linalg.norm(e1)
e2 = np.cross(e1, Lt)
Pw = Pt[Y0:Y1, X0:X1]
linw = lin[Y0:Y1, X0:X1].astype(np.float64)
casc = np.ones(Pw.shape[:2])
wallpix = ~hitc[Y0:Y1, X0:X1]
for (T, Rw, lo, hi) in ((1.239, 2.8, 0.0, 1750.0), (3.561, 10.5, 1750.0, 1e9)):
	m = wallpix & (linw >= lo) & (linw < hi)
	if not m.any():
		continue
	R = Rw / T
	n = int(np.clip(np.floor(32.0 * (R / 3.0) ** 2 + 0.5), 8, 48))
	q = np.stack([Pw[m] @ e1, Pw[m] @ e2], -1) / T
	c0 = np.array([P0 @ e1, P0 @ e2]) / T
	dl = np.array([wdir @ e1, wdir @ e2])
	dl /= np.linalg.norm(dl)
	nl = np.array([-dl[1], dl[0]])
	half = a.r / T

	def lit_bilinear(qq):
		f = qq - 0.5
		i0 = np.floor(f)
		t = f - i0
		out = 0.0
		for di in (0, 1):
			for dj in (0, 1):
				wgt = (t[:, 0] if di else 1 - t[:, 0]) * (t[:, 1] if dj else 1 - t[:, 1])
				cx = i0[:, 0] + di + 0.5 - c0[0]
				cy = i0[:, 1] + dj + 0.5 - c0[1]
				out = out + wgt * (np.abs(cx * nl[0] + cy * nl[1]) > half)
		return out
	cell = np.mod(np.floor(q * 4.0), 4096.0)
	rot = 2 * math.pi * np.mod(52.9829189 * np.mod(cell[:, 0] * 0.06711056 + cell[:, 1] * 0.00583715, 1.0), 1.0)
	acc = np.zeros(len(q))
	for t_ in range(n):
		r_ = math.sqrt((t_ + 0.5) / n) * R
		th = t_ * 2.39996323 + rot
		acc += lit_bilinear(q + np.stack([np.cos(th) * r_, np.sin(th) * r_], -1))
	casc[m] = acc / n
cont = full[Y0:Y1, X0:X1]
comp = np.minimum(casc, cont)


def line_stats(fac, tag):
	vals = []
	for x, y in zip(sxp, syp):
		xi, yi = int(x) - X0, int(y) - Y0
		if not (3 <= xi < fac.shape[1] - 4 and 3 <= yi < fac.shape[0] - 4) or hitc[int(y), int(x)]:
			continue
		vals.append(1.0 - fac[yi - 3:yi + 4, xi - 3:xi + 4].min())
	vals = np.array(vals)
	# the stair amplitude: the darkness centroid across the line (+-14 px), high-passed along it (a 24-px moving mean
	# is the line), half p2p (p2..p98), px
	dsx, dsy = np.diff(sxp).mean(), np.diff(syp).mean()
	dl_ = np.hypot(dsx, dsy)
	nx_, ny_ = -dsy / dl_, dsx / dl_
	cen = []
	for x, y in zip(sxp[::5], syp[::5]):
		ms = np.arange(-14, 14.01, 0.5)
		xs_ = np.round(x + nx_ * ms).astype(int) - X0
		ys_ = np.round(y + ny_ * ms).astype(int) - Y0
		if xs_.min() < 0 or ys_.min() < 0 or xs_.max() >= fac.shape[1] or ys_.max() >= fac.shape[0]:
			continue
		dk = 1.0 - fac[ys_, xs_]
		cen.append((dk * ms).sum() / max(dk.sum(), 1e-9) if dk.sum() > 1e-6 else np.nan)
	cen = np.array(cen)
	cen = cen[np.isfinite(cen)]
	step = 5 * dl_
	win = max(int(24 / step), 3)
	amp = float('nan')
	if len(cen) > 3 * win:
		hp = cen - np.convolve(cen, np.ones(win) / win, mode='same')
		hp = hp[win:-win]
		amp = (np.percentile(hp, 98) - np.percentile(hp, 2)) / 2
	print('%-8s along the wire shadow: %d stations, darkness mean %.3f min %.3f max %.3f CV %.3f, darker than 0.05: '
		  '%.1f %% | stair (centroid) half p2p %.2f px' % (tag, len(vals), vals.mean(), vals.min(), vals.max(),
		  vals.std() / max(vals.mean(), 1e-9), 100.0 * (vals > 0.05).mean(), amp))
	return vals.std() / max(vals.mean(), 1e-9), amp, 100.0 * (vals > 0.05).mean()


print('window x %d..%d y %d..%d (bottom-up), wall depth there %.0f u, wire %.0f u off the wall' % (
	X0, X1, Y0, Y1, np.median(linw[wallpix]), a.off))
cvC, ampC, darkC = line_stats(cont, 'contact')
line_stats(casc, 'cascade')
cv, amp, _ = line_stats(comp, 'composed')
if a.png:
	from PIL import Image
	for nm, img in (('contact', cont), ('cascade', casc), ('composed', comp)):
		im = np.flipud(np.clip(img, 0, 1))
		Image.fromarray((im * 255).astype(np.uint8)).resize((im.shape[1] * 2, im.shape[0] * 2), Image.NEAREST).save(
			a.out + '_' + nm + '.png')
fails = []
if a.gate == 'far':
	if not amp < 1.0:
		fails.append('the composed wire shadow stairs %.2f px (bar < 1 px)' % amp)
	if not cv < 0.1:
		fails.append('the composed wire shadow darkness CV %.3f (bar < 0.1: dashes)' % cv)
else:
	if not darkC >= 90.0:
		fails.append('the contact tier marks only %.1f %% of a wire 6 u off the wall (bar >= 90 %%)' % darkC)
for f in fails:
	print('FAIL ' + f)
print('CONTACTREACH1 %s %s: %s' % (a.gate, 'red' if a.red else 'green', 'PASS' if not fails else 'FAIL'))
sys.exit(1 if fails else 0)
