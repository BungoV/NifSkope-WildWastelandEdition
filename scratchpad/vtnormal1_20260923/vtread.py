#!/usr/bin/env python
"""Lane VTBAKE1: a memory-mapped, numpy .lodt v2 reader for WHOLE-WORLDSPACE
containers (tests/spells/lodgen_vt_check.py reads the whole file into a bytes
object, which is fine for a region and not for a 3 GB level).

Layout re-typed from docs/LODGEN_TERRAIN_VT.md s3 -- nothing imported from the
writer. The codec is chosen PER TILE from the COVER bit against the header's
format pair (skill nifskope-ww-lodgen, lane GROUND1's trap).
"""
import mmap
import struct
import numpy as np

HDR = 256
STRIDE = 24
ROLE_NAMES = {1: 'color', 2: 'msn', 4: 'height', 5: 'mask', 6: 'emissive', 7: 'horizon'}


class Lodt(object):
	def __init__(self, path):
		self.path = path
		self.f = open(path, 'rb')
		self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
		b = self.m[:HDR]
		self.hdr = b
		self.magic = b[0:4]
		(self.version, self.headerBytes, self.flags) = struct.unpack_from('<III', b, 4)
		(self.fileBytes, self.tableOffset, self.payloadOffset) = struct.unpack_from('<QQQ', b, 0x10)
		(self.vhgt, self.paint) = struct.unpack_from('<QQ', b, 0x28)
		self.edidRaw = b[0x38:0x58]
		self.edid = self.edidRaw.split(b'\0')[0].decode('latin-1')
		(self.south, self.west, self.north, self.east,
		 self.wSouth, self.wWest, self.wNorth, self.wEast) = struct.unpack_from('<8h', b, 0x58)
		(self.levelDim, self.levelIndex, self.levelCount, self.tilesX, self.tilesY,
		 self.content, self.border, self.stored) = struct.unpack_from('<8H', b, 0x68)
		(self.mips, self.sheetCount, self.aniso, self.compression) = struct.unpack_from('<4B', b, 0x78)
		self.tileCount = struct.unpack_from('<I', b, 0x7C)[0]
		(self.coverNorm, self.tintStrength) = struct.unpack_from('<2f', b, 0x80)
		self.levelDims = list(struct.unpack_from('<8H', b, 0x88))
		self.indexCrc = struct.unpack_from('<I', b, 0x98)[0]
		self.reserved0 = struct.unpack_from('<I', b, 0x9C)[0]
		self.sheets = []
		for i in range(10):
			f0, f1, role, space, z0, z1 = struct.unpack_from('<HHBBBB', b, 0xA0 + i * 8)
			self.sheets.append({'dxgi': f0, 'dxgiCover': f1, 'role': role, 'space': space,
								'zero': (z0, z1)})
		self.tail = b[0xF0:0x100]
		dt = np.dtype([('offset', '<u8'), ('stored', '<u4'), ('raw', '<u4'), ('crc', '<u4'),
					   ('flags', '<u2'), ('reserved', '<u2')])
		self.table = np.frombuffer(self.m, dtype=dt, count=self.tileCount, offset=self.tableOffset)

	def sheetIndex(self, role):
		for s in range(self.sheetCount):
			if self.sheets[s]['role'] == role:
				return s
		return None

	def fmt(self, s, cover):
		sd = self.sheets[s]
		return sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']

	def sheetMipBytes(self, s, mip, cover):
		side = self.stored >> mip
		role = self.sheets[s]['role']
		if role == 4:
			return side * side * 2
		if role == 7:
			return side * side * 4
		bb = 16 if self.fmt(s, cover) in (77, 78) else 8
		return (side // 4) * (side // 4) * bb

	def rawBytes(self, cover):
		return sum(self.sheetMipBytes(s, m, cover)
				   for s in range(self.sheetCount) for m in range(self.mips))

	def sheetOffset(self, cover, sheet, mip):
		o = 0
		for s in range(self.sheetCount):
			for m in range(self.mips):
				if s == sheet and m == mip:
					return o
				o += self.sheetMipBytes(s, m, cover)
		raise ValueError('no such sheet/mip')

	def tileIndex(self, tx, ty):
		return ty * self.tilesX + tx

	def tileOfCell(self, cx, cy):
		tx = (cx - self.west) // self.levelDim
		ty = (self.north - cy) // self.levelDim
		return tx, ty

	def blocks(self, index, sheet, mip):
		"""Raw block bytes of one sheet mip of one tile, as a uint8 array
		(nBlocksY, nBlocksX, blockBytes), plus the dxgi format used."""
		e = self.table[index]
		if not (int(e['flags']) & 1):
			return None, None
		if self.compression != 0:
			raise NotImplementedError('zlib payloads not needed by this lane')
		cover = bool(int(e['flags']) & 2)
		fmt = self.fmt(sheet, cover)
		side = self.stored >> mip
		o = int(e['offset']) + self.sheetOffset(cover, sheet, mip)
		n = self.sheetMipBytes(sheet, mip, cover)
		a = np.frombuffer(self.m, dtype=np.uint8, count=n, offset=o)
		if self.sheets[sheet]['role'] == 4:
			return a.view('<u2').reshape(side, side), fmt
		bb = 16 if fmt in (77, 78) else 8
		return a.reshape(side // 4, side // 4, bb), fmt

	def rgb(self, index, sheet, mip):
		"""Decoded (side, side, 3) uint8 RGB, plus alpha (side, side) for BC3."""
		blk, fmt = self.blocks(index, sheet, mip)
		if blk is None:
			return None, None
		if fmt in (77, 78):
			col = decode_bc1_blocks(blk[..., 8:16], four=True)
			alp = decode_bc3_alpha(blk[..., 0:8])
			return col, alp
		return decode_bc1_blocks(blk, four=False), None


def _565(c):
	r = ((c >> 11) & 31).astype(np.int32)
	g = ((c >> 5) & 63).astype(np.int32)
	b = (c & 31).astype(np.int32)
	return np.stack([(r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (b * 527 + 23) >> 6], -1)


def decode_bc1_blocks(blk, four):
	"""blk: (by, bx, 8) uint8. Returns (by*4, bx*4, 3) uint8. four=True forces
	the 4-colour palette (the colour half of a BC3 block)."""
	by, bx = blk.shape[:2]
	c0 = blk[..., 0].astype(np.uint16) | (blk[..., 1].astype(np.uint16) << 8)
	c1 = blk[..., 2].astype(np.uint16) | (blk[..., 3].astype(np.uint16) << 8)
	bits = (blk[..., 4].astype(np.uint32) | (blk[..., 5].astype(np.uint32) << 8)
			| (blk[..., 6].astype(np.uint32) << 16) | (blk[..., 7].astype(np.uint32) << 24))
	e0, e1 = _565(c0), _565(c1)
	p2a = (2 * e0 + e1) // 3
	p3a = (e0 + 2 * e1) // 3
	p2b = (e0 + e1) // 2
	p3b = np.zeros_like(e0)
	mode4 = (c0 > c1)[..., None] | four
	p2 = np.where(mode4, p2a, p2b)
	p3 = np.where(mode4, p3a, p3b)
	pal = np.stack([e0, e1, p2, p3], -2)  # (by,bx,4,3)
	idx = np.stack([(bits >> (2 * i)) & 3 for i in range(16)], -1).astype(np.int64)  # (by,bx,16)
	px = np.take_along_axis(pal, idx[..., None].repeat(3, -1), axis=-2)  # (by,bx,16,3)
	px = px.reshape(by, bx, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(by * 4, bx * 4, 3)
	return px.astype(np.uint8)


def decode_bc3_alpha(blk):
	by, bx = blk.shape[:2]
	a0 = blk[..., 0].astype(np.int32)
	a1 = blk[..., 1].astype(np.int32)
	bits = np.zeros((by, bx), dtype=np.uint64)
	for i in range(6):
		bits |= blk[..., 2 + i].astype(np.uint64) << np.uint64(8 * i)
	pal = [a0, a1]
	big = a0 > a1
	for k in range(1, 7):
		pal.append(np.where(big, ((7 - k) * a0 + k * a1) // 7, 0))
	for k in range(1, 5):
		pal[1 + k] = np.where(big, pal[1 + k], ((5 - k) * a0 + k * a1) // 5)
	pal[6] = np.where(big, pal[6], 0)
	pal[7] = np.where(big, pal[7], 255)
	pal = np.stack(pal, -1)
	idx = np.stack([((bits >> np.uint64(3 * i)) & np.uint64(7)).astype(np.int64) for i in range(16)], -1)
	px = np.take_along_axis(pal, idx, axis=-1)
	return px.reshape(by, bx, 4, 4).transpose(0, 2, 1, 3).reshape(by * 4, bx * 4).astype(np.uint8)
