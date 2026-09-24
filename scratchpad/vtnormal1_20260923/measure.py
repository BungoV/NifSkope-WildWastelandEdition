"""VTNORMAL1 pre-code measurement: orientation of his _msn sheet and the r of
the rung's L02 pyramid normal against it, over the Sanctuary dim-4 chunk
(-20,24). Prints verdict lines only.
usage: measure.py <lodt VT.2 file> [label]
"""
import sys, struct
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923')
import vtread

HIS = 'E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS'
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS'
CX, CY = -20, 24


def load_his(path=HIS):
	b = open(path, 'rb').read()
	assert b[:4] == b'DDS ' and b[84:88] == b'DX10'
	h, w = struct.unpack_from('<II', b, 12)
	dxgi = struct.unpack_from('<I', b, 128)[0]
	assert dxgi == 28, dxgi
	a = np.frombuffer(b, np.uint8, count=w * h * 4, offset=148).reshape(h, w, 4).astype(np.float64)
	v = a[..., :3] / 255.0 * 2.0 - 1.0          # R east, G up, B north
	v = np.stack([v[..., 0], v[..., 2], v[..., 1]], -1)  # -> (east, north, up)
	v /= np.linalg.norm(v, axis=-1, keepdims=True)
	return v


def box(v, f):
	h, w, _ = v.shape
	s = v.reshape(h // f, f, w // f, f, 3).sum(axis=(1, 3))
	return s / np.linalg.norm(s, axis=-1, keepdims=True)


def load_vanilla(path=VAN):
	b = open(path, 'rb').read()
	fourcc = b[84:88]
	h, w = struct.unpack_from('<II', b, 12)
	if fourcc == b'DX10':
		dxgi = struct.unpack_from('<I', b, 128)[0]; off = 148
	else:
		dxgi = {b'DXT1': 71, b'ATI2': 83, b'BC5U': 83, b'DXT5': 77}.get(fourcc, -1); off = 128
	if dxgi in (71, 72):
		blk = np.frombuffer(b, np.uint8, count=(w // 4) * (h // 4) * 8, offset=off).reshape(h // 4, w // 4, 8)
		rgb = vtread.decode_bc1_blocks(blk, four=False).astype(np.float64)
	elif dxgi in (77, 78):
		blk = np.frombuffer(b, np.uint8, count=(w // 4) * (h // 4) * 16, offset=off).reshape(h // 4, w // 4, 16)
		rgb = vtread.decode_bc1_blocks(blk[..., 8:16], four=True).astype(np.float64)
	elif dxgi in (28, 87):
		a = np.frombuffer(b, np.uint8, count=w * h * 4, offset=off).reshape(h, w, 4).astype(np.float64)
		rgb = a[..., :3] if dxgi == 28 else a[..., [2, 1, 0]]
	else:
		raise SystemExit('vanilla msn format %r dxgi %d' % (fourcc, dxgi))
	v = rgb / 255.0 * 2.0 - 1.0
	v = np.stack([v[..., 0], v[..., 2], v[..., 1]], -1)
	v /= np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-9)
	return v, (w, h, dxgi)


def lodt_mosaic(path):
	L = vtread.Lodt(path)
	s = L.sheetIndex(2)
	c, bd = L.content, L.border
	n = 4 // L.levelDim
	out = np.zeros((c * n, c * n, 3))
	for j in range(n):          # j = 0 north
		for i in range(n):
			cx = CX + i * L.levelDim
			cy = CY + 4 - (j + 1) * L.levelDim
			tx, ty = L.tileOfCell(cx, cy)
			rgb, _ = L.rgb(L.tileIndex(tx, ty), s, 0)
			t = rgb[bd:bd + c, bd:bd + c].astype(np.float64) / 255.0 * 2.0 - 1.0
			out[j * c:(j + 1) * c, i * c:(i + 1) * c] = np.stack([t[..., 0], t[..., 2], t[..., 1]], -1)
	out /= np.maximum(np.linalg.norm(out, axis=-1, keepdims=True), 1e-9)
	return out, L


def r(a, b):
	return float(np.corrcoef(a.ravel(), b.ravel())[0, 1])


def bc1_sim(v):
	"""Crude BC1 round trip of an encoded (east,north,up) field: range fit on the
	principal axis, 565 endpoints, 4-colour palette. Predicts the codec's r loss."""
	rgb = np.stack([v[..., 0], v[..., 2], v[..., 1]], -1) * 0.5 + 0.5  # R east G up B north
	h, w, _ = rgb.shape
	blk = rgb.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h // 4, w // 4, 16, 3)
	mu = blk.mean(2, keepdims=True)
	d = blk - mu
	cov = np.einsum('abki,abkj->abij', d, d)
	_, vec = np.linalg.eigh(cov)
	ax = vec[..., :, -1][:, :, None, :]
	t = (d * ax).sum(-1)
	lo = mu[:, :, 0] + ax[:, :, 0] * t.min(2)[..., None]
	hi = mu[:, :, 0] + ax[:, :, 0] * t.max(2)[..., None]
	q = np.array([31, 63, 31], np.float64)
	lo = np.round(np.clip(lo, 0, 1) * q) / q
	hi = np.round(np.clip(hi, 0, 1) * q) / q
	pal = np.stack([lo, hi, (2 * lo + hi) / 3, (lo + 2 * hi) / 3], 2)
	dist = ((blk[:, :, :, None, :] - pal[:, :, None, :, :]) ** 2).sum(-1)
	pick = dist.argmin(-1)
	res = np.take_along_axis(pal, pick[..., None].repeat(3, -1), 2)
	res = res.reshape(h // 4, w // 4, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)
	res = np.round(res * 255) / 255 * 2 - 1
	o = np.stack([res[..., 0], res[..., 2], res[..., 1]], -1)
	return o / np.maximum(np.linalg.norm(o, axis=-1, keepdims=True), 1e-9)


def main():
	path = sys.argv[1]
	label = sys.argv[2] if len(sys.argv) > 2 else 'lodt'
	his8 = load_his()
	pyr, L = lodt_mosaic(path)
	f = 2048 // pyr.shape[0]
	his = box(his8, f)
	his_flip = his[::-1, :, :] * np.array([1, -1, 1])
	van, vinfo = load_vanilla()
	vanb = box(van, van.shape[0] // pyr.shape[0]) if van.shape[0] > pyr.shape[0] else van
	print('grid %s: level dim %d content %d -> %d px, his box %dx%d (%d u/texel)'
		  % (label, L.levelDim, L.content, pyr.shape[0], f, f, 8 * f))
	print('his downsampled SD east %.4f north %.4f; vanilla msn %dx%d dxgi %d'
		  % (his[..., 0].std(), his[..., 1].std(), vinfo[0], vinfo[1], vinfo[2]))
	for nm, ref in (('his(row0=north)', his), ('his(row0=south)', his_flip), ('vanilla', vanb)):
		print('r %s vs %-16s east %.4f north %.4f' % (label, nm, r(pyr[..., 0], ref[..., 0]), r(pyr[..., 1], ref[..., 1])))
	print('r vanilla vs his(row0=north) east %.4f north %.4f; vs his(row0=south) east %.4f north %.4f'
		  % (r(vanb[..., 0], his[..., 0]), r(vanb[..., 1], his[..., 1]),
			 r(vanb[..., 0], his_flip[..., 0]), r(vanb[..., 1], his_flip[..., 1])))
	sim = bc1_sim(his)
	print('ceiling: his downsampled through a simulated BC1 vs itself east %.4f north %.4f'
		  % (r(sim[..., 0], his[..., 0]), r(sim[..., 1], his[..., 1])))
	np.save(path + '.mosaic.npy', pyr) if False else None


if __name__ == '__main__':
	main()
