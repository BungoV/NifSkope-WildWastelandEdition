"""VTNORMAL1: the ceiling under lodgen's OWN BC1 encoder (lodgenEncodeBC1Block,
4-colour path: endpoints = the block's min- and max-LUMINANCE texels, 565 by
truncation, nearest of four). Luminance weights B at 0.114, and B is NORTH in
the msn: the encoder picks endpoints almost blind to north, which a PCA fit
(measure.py's bc1_sim) does not. Prints:
  ceiling  : lodgen-BC1(his downsampled) vs his downsampled
  transfer : the lodt's L02 normal vs lodgen-BC1(his downsampled) -- near 1.0
             when the pyramid carries exactly his sheet into the encoder."""
import sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923')
import measure as M


def enc8(v):
	# lodgenTerrainMsnPixel: R east, G up, B north, v*0.5+0.5 to 8 bit
	rgb = np.stack([v[..., 0], v[..., 2], v[..., 1]], -1)
	return np.clip(np.round((rgb * 0.5 + 0.5) * 255.0), 0, 255)


def bc1_lodgen(v):
	c = enc8(v)
	h, w, _ = c.shape
	blk = c.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h // 4, w // 4, 16, 3)
	lum = 0.299 * blk[..., 0] + 0.587 * blk[..., 1] + 0.114 * blk[..., 2]
	hi = np.take_along_axis(blk, lum.argmax(2)[..., None, None].repeat(3, -1), 2)[:, :, 0]
	lo = np.take_along_axis(blk, lum.argmin(2)[..., None, None].repeat(3, -1), 2)[:, :, 0]

	def q(x):
		x = x.astype(np.int64)
		return np.stack([(x[..., 0] >> 3) * 255.0 / 31, (x[..., 1] >> 2) * 255.0 / 63, (x[..., 2] >> 3) * 255.0 / 31], -1)
	p0, p1 = q(hi), q(lo)
	pal = np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 2)
	d = ((blk[:, :, :, None, :] - pal[:, :, None, :, :]) ** 2).sum(-1)
	res = np.take_along_axis(pal, d.argmin(-1)[..., None].repeat(3, -1), 2)
	res = res.reshape(h // 4, w // 4, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)
	res = np.round(res) / 255.0 * 2 - 1
	o = np.stack([res[..., 0], res[..., 2], res[..., 1]], -1)
	return o / np.maximum(np.linalg.norm(o, axis=-1, keepdims=True), 1e-9)


pyr, L = M.lodt_mosaic(sys.argv[1])
his = M.box(M.load_his(), 2048 // pyr.shape[0])
sim = bc1_lodgen(his)
print('ceiling (lodgen BC1 of his downsampled vs his) east %.4f north %.4f' % (M.r(sim[..., 0], his[..., 0]), M.r(sim[..., 1], his[..., 1])))
print('transfer (lodt L02 vs lodgen BC1 of his downsampled) east %.4f north %.4f up %.4f' % (
	M.r(pyr[..., 0], sim[..., 0]), M.r(pyr[..., 1], sim[..., 1]), M.r(pyr[..., 2], sim[..., 2])))
d = np.abs(pyr - sim).max(-1)
print('transfer texels within 1/64 of the simulation: %.4f' % (d < 1.0 / 64).mean())
