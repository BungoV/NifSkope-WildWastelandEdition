"""Numpy DDS reader for lane TERRAINFMT1: headers, BC1 colour, BC3 alpha.

Written because the tree's own reader (tests/spells/lodgen_terrain_model.py) is
pure Python and this lane decodes whole 512x512 and 2048x2048 sheets by the
hundred.  Its numbers are cross-checked against that reader in f1_corpus.py,
which is the point of having two.
"""
import struct
import numpy as np


def header(path):
	with open(path, 'rb') as f:
		b = f.read(148)
	if b[:4] != b'DDS ':
		raise ValueError('not a DDS: %s' % path)
	h = struct.unpack_from('<31I', b, 4)
	return {
		'flags': h[1], 'height': h[2], 'width': h[3], 'pitch': h[4],
		'mips': h[6], 'reserved1': h[7:18], 'fourcc': b[84:88],
		'caps': h[26],
	}


def _levels(b, fourcc, w, h, mips):
	bb = 8 if fourcc == b'DXT1' else 16
	off, out = 128, []
	for _ in range(max(1, mips)):
		bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
		size = bw * bh * bb
		if off + size > len(b):
			break
		out.append((off, w, h, bw, bh))
		off += size
		w, h = max(1, w // 2), max(1, h // 2)
	return out, bb


class Dds(object):
	def __init__(self, path):
		with open(path, 'rb') as f:
			self.raw = f.read()
		b = self.raw
		if b[:4] != b'DDS ':
			raise ValueError('not a DDS: %s' % path)
		hh = struct.unpack_from('<31I', b, 4)
		self.height, self.width = hh[2], hh[3]
		self.declaredMips = hh[6]
		self.reserved1 = hh[7:18]
		self.fourcc = b[84:88]
		if self.fourcc not in (b'DXT1', b'DXT3', b'DXT5'):
			raise ValueError('unsupported fourCC %r in %s' % (self.fourcc, path))
		self.levels, self.blockBytes = _levels(b, self.fourcc, self.width,
											   self.height, self.declaredMips)
		self.mips = len(self.levels)

	# ---- BC3 alpha ---------------------------------------------------------
	def alpha(self, m=0):
		"""Alpha plane of mip m as uint8 (h, w).  DXT1 -> all 255."""
		off, w, h, bw, bh = self.levels[m]
		if self.fourcc == b'DXT1':
			return np.full((h, w), 255, np.uint8)
		blk = np.frombuffer(self.raw, np.uint8, bw * bh * 16, off).reshape(bh, bw, 16)
		a0 = blk[:, :, 0].astype(np.int32)
		a1 = blk[:, :, 1].astype(np.int32)
		bits = np.zeros((bh, bw), np.uint64)
		for k in range(6):
			bits |= blk[:, :, 2 + k].astype(np.uint64) << np.uint64(8 * k)
		idx = np.zeros((bh, bw, 16), np.uint8)
		for i in range(16):
			idx[:, :, i] = ((bits >> np.uint64(3 * i)) & np.uint64(7)).astype(np.uint8)
		pal = np.zeros((bh, bw, 8), np.int32)
		pal[:, :, 0], pal[:, :, 1] = a0, a1
		gt = a0 > a1
		for k in range(1, 7):
			pal[:, :, k + 1] = np.where(gt, ((7 - k) * a0 + k * a1) // 7,
										0)
		for k in range(1, 5):
			pal[:, :, k + 1] = np.where(gt, pal[:, :, k + 1],
										((5 - k) * a0 + k * a1) // 5)
		pal[:, :, 6] = np.where(gt, pal[:, :, 6], 0)
		pal[:, :, 7] = np.where(gt, pal[:, :, 7], 255)
		out = np.take_along_axis(pal, idx.astype(np.int64), axis=2).astype(np.uint8)
		out = out.reshape(bh, bw, 4, 4).transpose(0, 2, 1, 3).reshape(bh * 4, bw * 4)
		return out[:h, :w]

	# ---- BC1 colour --------------------------------------------------------
	def rgb(self, m=0):
		"""RGB of mip m as uint8 (h, w, 3)."""
		off, w, h, bw, bh = self.levels[m]
		cOff = off + (8 if self.fourcc in (b'DXT3', b'DXT5') else 0)
		st = self.blockBytes
		flat = np.frombuffer(self.raw, np.uint8, bw * bh * st, off).reshape(bh, bw, st)
		blk = flat[:, :, (cOff - off):(cOff - off) + 8]
		c0 = blk[:, :, 0].astype(np.uint16) | (blk[:, :, 1].astype(np.uint16) << 8)
		c1 = blk[:, :, 2].astype(np.uint16) | (blk[:, :, 3].astype(np.uint16) << 8)

		def unpack(c):
			r = ((c >> 11) & 0x1F).astype(np.int32)
			g = ((c >> 5) & 0x3F).astype(np.int32)
			b = (c & 0x1F).astype(np.int32)
			return np.stack([(r * 255 + 15) // 31, (g * 255 + 31) // 63,
							 (b * 255 + 15) // 31], -1)

		p0, p1 = unpack(c0), unpack(c1)
		gt = (c0 > c1)[:, :, None]
		p2 = np.where(gt, (2 * p0 + p1 + 1) // 3, (p0 + p1) // 2)
		p3 = np.where(gt, (p0 + 2 * p1 + 1) // 3, np.zeros_like(p0))
		pal = np.stack([p0, p1, p2, p3], 2)          # (bh, bw, 4, 3)
		bits = np.zeros((bh, bw), np.uint32)
		for k in range(4):
			bits |= blk[:, :, 4 + k].astype(np.uint32) << np.uint32(8 * k)
		idx = np.zeros((bh, bw, 16), np.int64)
		for i in range(16):
			idx[:, :, i] = ((bits >> np.uint32(2 * i)) & np.uint32(3)).astype(np.int64)
		out = np.take_along_axis(pal, idx[:, :, :, None], axis=2).astype(np.uint8)
		out = out.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4)
		out = out.reshape(bh * 4, bw * 4, 3)
		return out[:h, :w]
