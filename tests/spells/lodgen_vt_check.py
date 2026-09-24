#!/usr/bin/env python
"""The measurements behind tests/spells/lodgen_terrain_vt.sh.

This re-implements the .lodt terrain-texture LAYOUT from
docs/LODGEN_TERRAIN_VT.md (the container was .lodv until 2026-09-09) -- header,
tile table, payload order -- and nothing else. It does NOT import the writer's
arithmetic: the box filter is re-typed from the document, so a wrong filter in
the C++ cannot agree with itself here.

Sub-commands:
  header <file...>              V1/V3/V4/V18: every header field, printed
  tiles <file>                  V5/V6/V7: sizes, offsets, pad bytes, CRCs
  filter <fine> <coarse>        V8/V10: a parent against the box filter of its
                                children, on the EXACT R16 height sheet and on
                                the BC1 colour, content and each border strip
  border <file>                 V11: a tile's border against its neighbour's
                                content, both axes
  georef <file>                 V20: north-up, and two tiles are not one tile
  ladder <file...>              the clipmap requirement: one aligned grid, each
                                coarse tile covering exactly four finer ones
"""

import json
import os
import struct
import sys
import zlib

HDR = 256
STRIDE = 24


def crc32(b):
	return zlib.crc32(b) & 0xFFFFFFFF


class Lodv(object):
	def __init__(self, path):
		self.path = path
		with open(path, 'rb') as f:
			self.b = f.read()
		b = self.b
		self.magic = b[0:4]
		(self.version, self.headerBytes, self.flags) = struct.unpack_from('<III', b, 4)
		(self.fileBytes, self.tableOffset, self.payloadOffset) = struct.unpack_from('<QQQ', b, 0x10)
		(self.vhgt, self.paint) = struct.unpack_from('<QQ', b, 0x28)
		self.edid = b[0x38:0x58].split(b'\0')[0].decode('latin-1')
		(self.south, self.west, self.north, self.east,
		 self.wSouth, self.wWest, self.wNorth, self.wEast) = struct.unpack_from('<8h', b, 0x58)
		(self.levelDim, self.levelIndex, self.levelCount, self.tilesX, self.tilesY,
		 self.content, self.border, self.stored) = struct.unpack_from('<8H', b, 0x68)
		(self.mips, self.sheetCount, self.aniso, self.compression) = struct.unpack_from('<4B', b, 0x78)
		self.tileCount = struct.unpack_from('<I', b, 0x7C)[0]
		(self.coverNorm, self.tintStrength) = struct.unpack_from('<2f', b, 0x80)
		self.levelDims = list(struct.unpack_from('<8H', b, 0x88))
		self.indexCrc = struct.unpack_from('<I', b, 0x98)[0]
		self.sheets = []
		# TEN descriptors since 2026-09-18 (0xA0..0xEF); v2 held six at
		# 0xA0..0xCF and v1 four. Nothing before 0xA0 moved and the stride is
		# still 8, so a reader that stops at six reads an OLD container
		# correctly and simply cannot see the horizon sheets.
		for i in range(10):
			f0, f1, role, space, skip = struct.unpack_from('<HHBBB', b, 0xA0 + i * 8)
			# byte 6 is mipSkip (lane VTNORMAL1): 1 on a half-resolution sheet,
			# which stores the full sheet's mips 1.. and no mip 0
			self.sheets.append({'dxgi': f0, 'dxgiCover': f1, 'role': role, 'space': space,
								'skip': skip})
		self.table = []
		for i in range(self.tileCount):
			o = self.tableOffset + i * STRIDE
			off, stored, raw, crc, flags, res = struct.unpack_from('<QIIIHH', b, o)
			self.table.append({'offset': off, 'stored': stored, 'raw': raw, 'crc': crc,
							   'flags': flags, 'reserved': res})

	def sheetMipBytes(self, s, mip, cover):
		skip = self.sheets[s]['skip']
		if mip + skip >= self.mips:
			return 0
		side = self.stored >> (mip + skip)
		if self.sheets[s]['role'] == 4:
			return side * side * 2
		# role 7 is the HORIZON sheet: uncompressed R8G8B8A8, four azimuth bins
		# one byte each, so it is the one role with no block compression at all
		if self.sheets[s]['role'] == 7:
			return side * side * 4
		# the cover carrier is whichever sheet declares TWO formats -- the mask
		# by default, the colour sheet under --vt-cover-in-color
		sd = self.sheets[s]
		fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
		bb = 16 if fmt in (77, 78) else 8
		return (side // 4) * (side // 4) * bb

	def rawBytes(self, cover):
		return sum(self.sheetMipBytes(s, m, cover)
				   for s in range(self.sheetCount) for m in range(self.mips))

	def payload(self, index):
		e = self.table[index]
		if not (e['flags'] & 1):
			return None
		d = self.b[e['offset']:e['offset'] + e['stored']]
		if self.compression == 1:
			d = zlib.decompress(d)
		return d

	def sheetOffset(self, cover, sheet, mip):
		o = 0
		for s in range(self.sheetCount):
			for m in range(self.mips):
				if s == sheet and m == mip:
					return o
				o += self.sheetMipBytes(s, m, cover)
		return o

	def heights(self, index):
		"""The R16 height sheet of one tile, mip 0, as a list of rows. It is
		UNCOMPRESSED, which is what lets the filter law be checked exactly."""
		e = self.table[index]
		p = self.payload(index)
		hs = None
		for s in range(self.sheetCount):
			if self.sheets[s]['role'] == 4:
				hs = s
		if hs is None or p is None:
			return None
		o = self.sheetOffset(bool(e['flags'] & 2), hs, 0)
		n = self.stored >> self.sheets[hs]['skip']
		return [list(struct.unpack_from('<%dH' % n, p, o + y * n * 2)) for y in range(n)]

	def colour(self, index, mip=0):
		"""The BC1 colour sheet decoded to RGB rows."""
		e = self.table[index]
		p = self.payload(index)
		if p is None:
			return None
		o = self.sheetOffset(bool(e['flags'] & 2), 0, mip)
		side = self.stored >> mip
		return decode_bc1(p, o, side, side)


def rgb565(c):
	return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)


def decode_bc1(b, off, w, h):
	out = [[(0, 0, 0)] * w for _ in range(h)]
	bw, bh = w // 4, h // 4
	for by in range(bh):
		for bx in range(bw):
			o = off + (by * bw + bx) * 8
			c0, c1, bits = struct.unpack_from('<HHI', b, o)
			e0, e1 = rgb565(c0), rgb565(c1)
			if c0 > c1:
				pal = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
					   tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
			else:
				pal = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
			for i in range(16):
				out[by * 4 + (i >> 2)][bx * 4 + (i & 3)] = pal[(bits >> (2 * i)) & 3]
	return out


FAILS = [0]


def check(name, ok, detail=''):
	print('  %s %s%s' % ('ok  ' if ok else 'FAIL', name, (' -- ' + detail) if detail else ''))
	if not ok:
		FAILS[0] += 1


def cmd_header(paths):
	for p in paths:
		v = Lodv(p)
		print('  %s' % os.path.basename(p))
		print('    magic %s version %d headerBytes %d flags %d compression %d'
			  % (v.magic.decode('latin-1'), v.version, v.headerBytes, v.flags, v.compression))
		print('    worldspace %s  padded S/W/N/E %d %d %d %d  world %d %d %d %d'
			  % (v.edid, v.south, v.west, v.north, v.east,
				 v.wSouth, v.wWest, v.wNorth, v.wEast))
		print('    levelDim %d index %d count %d tiles %dx%d = %d'
			  % (v.levelDim, v.levelIndex, v.levelCount, v.tilesX, v.tilesY, v.tileCount))
		print('    content %d border %d stored %d mips %d sheets %d aniso %d'
			  % (v.content, v.border, v.stored, v.mips, v.sheetCount, v.aniso))
		print('    levelDims %s  coverNorm %g tint %g' % (v.levelDims, v.coverNorm, v.tintStrength))
		print('    vhgt 0x%016X  paint 0x%016X' % (v.vhgt, v.paint))
		for i, s in enumerate(v.sheets[:v.sheetCount]):
			print('    sheet %d role %d dxgi %d dxgiCover %d space %d'
				  % (i, s['role'], s['dxgi'], s['dxgiCover'], s['space']))
		check('V1 %s magic, version and header size' % os.path.basename(p),
			  v.magic == b'LDTX' and v.version == 2 and v.headerBytes == HDR,
			  'version %d' % v.version)
		check('V1 %s the geometry is the one the document fixes' % os.path.basename(p),
			  v.content == 256 and v.border == 8 and v.stored == 272 and v.mips == 2
			  and v.aniso == 8)
		roles = [x['role'] for x in v.sheets[:v.sheetCount]]
		# version 2 (docs/LODGEN_TERRAIN_VT.md 2.2): colour, msn, MASK, then
		# height if it was asked for, then emissive if any layer supplies one.
		# Role 3 `data` is RETIRED and must appear nowhere.
		check('V1 %s the object texture family: colour, msn and MASK, in that order'
			  % os.path.basename(p),
			  roles[:3] == [1, 2, 5] and 3 not in roles,
			  'roles %s' % roles)
		check('V1 %s the HEIGHT sheet is role 4 and R16' % os.path.basename(p),
			  4 in roles and v.sheets[roles.index(4)]['dxgi'] == 56,
			  'roles %s' % roles)
		# exactly ONE sheet may declare two formats, and it is the cover carrier
		carriers = [i for i, x in enumerate(v.sheets[:v.sheetCount])
					if x['dxgiCover'] != x['dxgi']]
		check('V1 %s exactly one sheet carries the ground-cover alpha'
			  % os.path.basename(p),
			  len(carriers) == 1 and v.sheets[carriers[0]]['role'] in (1, 5),
			  'carriers %s' % carriers)
		check('V1 %s the reserved tail is zero' % os.path.basename(p),
			  v.b[0xD0:0x100] == b'\0' * 48)
		check('V1 %s row order is north-up and it is a refusal to be otherwise'
			  % os.path.basename(p), (v.flags & 1) == 1)
		check('V3 %s fileBytes matches the file' % os.path.basename(p),
			  v.fileBytes == os.path.getsize(p),
			  '%d vs %d' % (v.fileBytes, os.path.getsize(p)))
		spanX, spanY = v.east - v.west + 1, v.north - v.south + 1
		check('V4 %s tilesX/tilesY are EXACT divides of the padded rectangle'
			  % os.path.basename(p),
			  spanX % v.levelDim == 0 and spanY % v.levelDim == 0
			  and v.tilesX == spanX // v.levelDim and v.tilesY == spanY // v.levelDim
			  and v.tileCount == v.tilesX * v.tilesY)
		check('V4 %s the padded rectangle contains the world rectangle' % os.path.basename(p),
			  v.west <= v.wWest and v.south <= v.wSouth
			  and v.east >= v.wEast and v.north >= v.wNorth)
		# V18: the size, exactly, not "within a kilobyte"
		want = v.payloadOffset
		for i, e in enumerate(v.table):
			if e['flags'] & 1:
				want = e['offset'] + e['stored']
		check('V18 %s the file is exactly header + table + aligned payloads'
			  % os.path.basename(p), want == v.fileBytes, '%d vs %d' % (want, v.fileBytes))
		# V7's other half, the one per-payload CRCs cannot see
		hz = bytearray(v.b[:HDR])
		hz[0x98:0x9C] = b'\0\0\0\0'
		c = crc32(bytes(hz))
		c = zlib.crc32(v.b[v.tableOffset:v.tableOffset + STRIDE * v.tileCount], c) & 0xFFFFFFFF
		check('V7 %s indexCrc32 covers the header and the whole table'
			  % os.path.basename(p), c == v.indexCrc, '0x%08X vs 0x%08X' % (c, v.indexCrc))


def cmd_tiles(path):
	v = Lodv(path)
	present = [i for i, e in enumerate(v.table) if e['flags'] & 1]
	absent = [i for i, e in enumerate(v.table) if not (e['flags'] & 1)]
	print('  %s: %d present, %d absent' % (os.path.basename(path), len(present), len(absent)))
	bad = 0
	for i in absent:
		o = v.tableOffset + i * STRIDE
		if v.b[o:o + STRIDE] != b'\0' * STRIDE:
			bad += 1
	check('V5 an absent tile is 24 ZERO bytes, not just a zero offset', bad == 0)
	sizes = set()
	rawBad = 0
	for i in present:
		e = v.table[i]
		want = v.rawBytes(bool(e['flags'] & 2))
		sizes.add(e['raw'])
		if e['raw'] != want:
			rawBad += 1
		if v.compression == 0 and e['stored'] != e['raw']:
			rawBad += 1
	print('  V5 distinct rawBytes: %s (the header implies %d without cover, %d with)'
		  % (sorted(sizes), v.rawBytes(False), v.rawBytes(True)))
	check('V5 every payload is exactly the size the header implies', rawBad == 0)
	inflBad = 0
	for i in present[:64]:
		d = v.payload(i)
		if d is None or len(d) != v.table[i]['raw']:
			inflBad += 1
	check('V5 the payloads really are that long when read', inflBad == 0,
		  'first 64 present tiles')
	# V6: ordering, alignment, no overlap, and every pad byte zero
	prevEnd = v.payloadOffset
	orderBad = alignBad = overlapBad = padBad = 0
	for i in present:
		e = v.table[i]
		if e['offset'] % 4096:
			alignBad += 1
		if e['offset'] < prevEnd:
			overlapBad += 1
		if v.b[prevEnd:e['offset']].strip(b'\0'):
			padBad += 1
		prevEnd = e['offset'] + e['stored']
	for a, b_ in zip(present, present[1:]):
		if v.table[a]['offset'] >= v.table[b_]['offset']:
			orderBad += 1
	check('V6 payloads are 4,096-aligned', alignBad == 0)
	check('V6 payloads do not overlap and are in table-index order',
		  overlapBad == 0 and orderBad == 0)
	check('V6 every alignment pad byte is zero (this is what makes two runs match)',
		  padBad == 0)
	crcBad = 0
	for i in present:
		e = v.table[i]
		if crc32(v.b[e['offset']:e['offset'] + e['stored']]) != e['crc']:
			crcBad += 1
	check('V7 every tile CRC recomputes', crcBad == 0)
	if v.compression == 1:
		zbad = 0
		for i in present:
			e = v.table[i]
			b0, b1 = v.b[e['offset']], v.b[e['offset'] + 1]
			if (b0 & 15) != 8 or (b0 >> 4) > 7 or (b1 & 0x20) or ((b0 << 8 | b1) % 31):
				zbad += 1
		check('V21 every stream is CM=8 CINFO<=7 FDICT-clear, which is all the '
			  'consumer inflater accepts', zbad == 0)


def cmd_filter(finePath, coarsePath):
	"""V8/V10. The HEIGHT sheet is uncompressed R16, so the filter law is
	checked EXACTLY there -- zero violations, not a bar. The colour sheet is
	BC1 both sides, so it gets the document's bars."""
	fine = Lodv(finePath)
	coarse = Lodv(coarsePath)
	# a half-resolution height sheet (--vt-half-aux, descriptor byte 6) stores
	# no mip 0, so the exact per-texel law has nothing to read at the fine side
	if any(x['role'] == 4 and x['skip'] for x in fine.sheets + coarse.sheets):
		print('  skip V8/V10: half-resolution height sheet (mipSkip 1); the law is checked on the full bake')
		return
	check('V8 the two levels share one origin (a coarse tile covers exactly four fine)',
		  fine.west == coarse.west and fine.north == coarse.north
		  and coarse.levelDim == fine.levelDim * 2)
	C, B, S = coarse.content, coarse.border, coarse.stored
	mosW, mosH = fine.tilesX * C, fine.tilesY * C

	def fineTexel(u, v_, kind):
		u = min(max(u, 0), mosW - 1)
		v_ = min(max(v_, 0), mosH - 1)
		tx, ty = u // C, v_ // C
		idx = ty * fine.tilesX + tx
		key = (idx, kind)
		if key not in cache:
			cache[key] = fine.heights(idx) if kind == 'h' else fine.colour(idx)
		img = cache[key]
		return img[B + v_ % C][B + u % C]

	cache = {}
	# the parent nearest the middle of the grid, so all four children exist
	ptx, pty = coarse.tilesX // 2, coarse.tilesY // 2
	pIdx = pty * coarse.tilesX + ptx
	ph = coarse.heights(pIdx)
	pc = coarse.colour(pIdx)
	regions = {'content': [(i, j) for j in range(B, B + C) for i in range(B, B + C)],
			   'north': [(i, j) for j in range(0, B) for i in range(S)],
			   'south': [(i, j) for j in range(S - B, S) for i in range(S)],
			   'west': [(i, j) for j in range(S) for i in range(0, B)],
			   'east': [(i, j) for j in range(S) for i in range(S - B, S)]}
	# BC1 carries four interpolated levels between 5:6:5 endpoints, so decode(coarse)
	# and box(decode(fine)) differ by the codec whichever order they are taken in.
	# The property that is true of a BOX FILTER and false of almost any other rule is
	# that the parent lies inside its four children's range; BC1_SLOP widens it by one
	# endpoint quantum a channel (255/31 = 8.2).
	# BC1: four levels between 5:6:5 endpoints, so a texel's worst error is about a
	# sixth of its own block's range plus the endpoint quantum (255/31 = 8.2 on red and
	# blue). A constant cannot bound that - it is too loose on a flat block and too
	# tight on a contrasty one - so the slop is computed per block, per channel.
	BC1_ENDPOINT = 8

	def fineSlop(u, v_, k):
		"""The child texel's own BC1 block error, in the child's own tile."""
		u = min(max(u, 0), mosW - 1)
		v_ = min(max(v_, 0), mosH - 1)
		tx, ty = u // C, v_ // C
		idx = ty * fine.tilesX + tx
		key = (idx, 'c')
		if key not in cache:
			cache[key] = fine.colour(idx)
		return blockSlop(cache[key], B + u % C, B + v_ % C, k)

	def blockSlop(img, i, j, k):
		bi, bj = (i // 4) * 4, (j // 4) * 4
		lo, hi = 255, 0
		for jj in range(bj, min(bj + 4, len(img))):
			row = img[jj]
			for ii in range(bi, min(bi + 4, len(row))):
				v = row[ii][k]
				lo = min(lo, v)
				hi = max(hi, v)
		return (hi - lo) // 6 + BC1_ENDPOINT

	for name, pts in regions.items():
		step = max(1, len(pts) // 4000)
		hBad = 0
		cDiffs = []
		cOut = 0
		for (i, j) in pts[::step]:
			u0, v0 = 2 * (ptx * C + i - B), 2 * (pty * C + j - B)
			acc = (fineTexel(u0, v0, 'h') + fineTexel(u0 + 1, v0, 'h')
				   + fineTexel(u0, v0 + 1, 'h') + fineTexel(u0 + 1, v0 + 1, 'h'))
			if ph[j][i] != (acc + 2) >> 2:
				hBad += 1
			kids = [fineTexel(u0, v0, 'c'), fineTexel(u0 + 1, v0, 'c'),
					fineTexel(u0, v0 + 1, 'c'), fineTexel(u0 + 1, v0 + 1, 'c')]
			for k in range(3):
				vals = [kid[k] for kid in kids]
				a = (sum(vals) + 2) >> 2
				cDiffs.append(abs(pc[j][i][k] - a))
				# both sides are BC1: the parent's own block error, plus the worst of
				# the four children's, since their decoded range is not the range the
				# filter actually averaged
				slop = blockSlop(pc, i, j, k) + max(
					fineSlop(u0, v0, k), fineSlop(u0 + 1, v0, k),
					fineSlop(u0, v0 + 1, k), fineSlop(u0 + 1, v0 + 1, k))
				if not (min(vals) - slop <= pc[j][i][k] <= max(vals) + slop):
					cOut += 1
		cDiffs.sort()
		med = cDiffs[len(cDiffs) // 2] if cDiffs else 0
		mx = cDiffs[-1] if cDiffs else 0
		print('  V8 %-8s height violations %d (bar 0); colour outside its children\'s range '
			  'widened by both sides\' BC1 blocks %d (bar 0); colour vs box median %d max %d, '
			  'for information' % (name, hBad, cOut, med, mx))
		check('V8 %s obeys (a+b+c+d+2)>>2 exactly on the R16 height sheet' % name, hBad == 0)
		check('V8 %s colour stays inside the range of the four texels it averaged' % name,
			  cOut == 0)
	# V10: the CONTROL, and it has to measure its own power first. Only a texel whose
	# four children DIFFER can tell a box filter from a nearest resample; a Commonwealth
	# pyramid is interpolated up from land samples 128 units apart, so most texels cannot.
	# Search every parent tile for the one with the most relief and use that.
	best = (-1, 0, 0, 0)          # discriminating, agreeWithBox, agreeWithNearest, tile
	for cand in range(coarse.tilesX * coarse.tilesY):
		ctx, cty = cand % coarse.tilesX, cand // coarse.tilesX
		try:
			ch = coarse.heights(cand)
		except Exception:
			continue
		disc = box = near = 0
		for j in range(B, B + C, 7):
			for i in range(B, B + C, 7):
				u0, v0 = 2 * (ctx * C + i - B), 2 * (cty * C + j - B)
				kids = [fineTexel(u0, v0, 'h'), fineTexel(u0 + 1, v0, 'h'),
						fineTexel(u0, v0 + 1, 'h'), fineTexel(u0 + 1, v0 + 1, 'h')]
				if min(kids) == max(kids):
					continue          # box and nearest agree here by construction
				disc += 1
				if ch[j][i] == (sum(kids) + 2) >> 2:
					box += 1
				if ch[j][i] == kids[0]:
					near += 1
		if disc > best[0]:
			best = (disc, box, near, cand)
	disc, box, near, cand = best
	print('  V10 best parent tile %d: %d texels can tell box from nearest; the file matches '
		  'box on %d of them and nearest on %d' % (cand, disc, box, near))
	check('V10 the region has texels that can discriminate at all (bar 20)', disc >= 20)
	check('V10 the coarse level is a box filter, not a nearest resample',
		  disc >= 20 and box == disc and near < disc)


def cmd_border(path):
	"""V11: a tile's border is its NEIGHBOUR's content, not a clamp of its own
	edge. Both axes, and the second half of the check is the one that matters:
	the border must actually DIFFER from the tile's own edge column, or a
	clamped border would pass the first half every time."""
	v = Lodv(path)
	C, B, S = v.content, v.border, v.stored
	tx, ty = v.tilesX // 2, v.tilesY // 2
	me = v.heights(ty * v.tilesX + tx)
	east = v.heights(ty * v.tilesX + tx + 1)
	south = v.heights((ty + 1) * v.tilesX + tx)
	for name, other, mine, theirs in (
			('horizontal', east,
			 [(B + C + k, B + j) for j in range(0, C, 3) for k in range(B)],
			 [(B + k, B + j) for j in range(0, C, 3) for k in range(B)]),
			('vertical', south,
			 [(B + i, B + C + k) for i in range(0, C, 3) for k in range(B)],
			 [(B + i, B + k) for i in range(0, C, 3) for k in range(B)])):
		bad = 0
		clampLike = 0
		n = 0
		for (mx, my), (ox, oy) in zip(mine, theirs):
			n += 1
			if me[my][mx] != other[oy][ox]:
				bad += 1
			# a border filled by CLAMPING would repeat this tile's own last
			# content texel along the same row (or column)
			edge = me[my][B + C - 1] if name == 'horizontal' else me[B + C - 1][mx]
			if me[my][mx] == edge:
				clampLike += 1
		print('  V11 %s: border texels %d, disagreeing with the neighbour %d (bar 0), '
			  'equal to this tile\'s own edge %d (bar < 75%%)' % (name, n, bad, clampLike))
		check('V11 %s the border IS the neighbour\'s content' % name, n > 0 and bad == 0)
		check('V11 %s and it is not a clamp of this tile\'s edge' % name,
			  n > 0 and clampLike < n * 0.75)


def cmd_georef(path):
	"""V20. Row 0 is NORTH, and two tiles are not the same tile."""
	v = Lodv(path)
	a = v.heights(0)
	b = v.heights(1)
	same = sum(1 for j in range(0, v.stored, 5) for i in range(0, v.stored, 5)
			   if a[j][i] == b[j][i])
	tot = len(range(0, v.stored, 5)) ** 2
	print('  V20 tile (0,0) vs tile (1,0): identical samples %d of %d (bar < 90%%)'
		  % (same, tot))
	check('V20 two tiles are not one tile', same < tot * 0.9)
	# the north row's cells, derived from the document's tile -> cells line
	northCellY = v.north - 0 * v.levelDim
	southCellY = v.north - (v.tilesY - 1 + 1) * v.levelDim + 1
	print('  V20 tile row 0 covers cells y %d..%d, row %d covers y %d..%d (north %d, south %d)'
		  % (v.north - v.levelDim + 1, northCellY, v.tilesY - 1, southCellY,
			 v.north - (v.tilesY - 1) * v.levelDim, v.north, v.south))
	check('V20 row 0 is the NORTH row of the padded rectangle',
		  northCellY == v.north and southCellY == v.south)


def cmd_ladder(paths):
	"""The clipmap requirement, checked rather than asserted: one aligned grid,
	each coarser tile covering exactly four finer ones, and the index stating
	the world span and the texel count per level rather than implying them."""
	vs = [Lodv(p) for p in paths]
	vs.sort(key=lambda v: v.levelDim)
	dims = [v.levelDim for v in vs]
	print('  ladder: %s' % dims)
	check('the ladder doubles from the finest level up',
		  all(dims[i + 1] == dims[i] * 2 for i in range(len(dims) - 1)))
	check('every level names the same ladder in its own header',
		  all(v.levelDims[:len(dims)] == dims and v.levelCount == len(dims) for v in vs))
	check('every level is anchored to ONE north-west origin',
		  len(set(v.west for v in vs)) == 1 and len(set(v.north for v in vs)) == 1)
	ok = True
	for i in range(len(vs) - 1):
		f, c = vs[i], vs[i + 1]
		if c.tilesX != (f.tilesX + 1) // 2 or c.tilesY != (f.tilesY + 1) // 2:
			ok = False
	check('a coarse tile covers exactly four finer ones', ok)
	check('every container agrees on the world rectangle and both corpus hashes',
		  len(set((v.wWest, v.wSouth, v.wEast, v.wNorth, v.vhgt, v.paint) for v in vs)) == 1)
	# The msn stores UP in GREEN at EVERY level. The parent-tile filter passed its
	# channels to the pixel function in the wrong order until 2026-09-18, so every
	# other level came out with up in BLUE and lit sideways.
	ups = []
	for v in vs:
		img = decode_bc1(v.payload(0), v.sheetOffset(False, 1, 0), v.stored, v.stored)
		flat = [q for row in img[::4] for q in row[::4]]
		g = sum(q[1] for q in flat) / float(len(flat))
		b = sum(q[2] for q in flat) / float(len(flat))
		ups.append((v.levelDim, int(g), int(b)))
	print('  msn mean (dim, green, blue): %s' % ups)
	check('every level of the ladder stores the normal with UP in green',
		  len(ups) > 0 and all(g > 200 and b < 160 for _, g, b in ups), str(ups))
	for v in vs:
		print('    dim %2d: %d x %d tiles, %d world units a tile, %d texels a tile edge, '
			  '%d units a texel' % (v.levelDim, v.tilesX, v.tilesY, v.levelDim * 4096,
									v.content, v.levelDim * 4096 // v.content))


if __name__ == '__main__':
	cmd = sys.argv[1]
	if cmd == 'header':
		cmd_header(sys.argv[2:])
	elif cmd == 'tiles':
		cmd_tiles(sys.argv[2])
	elif cmd == 'filter':
		cmd_filter(sys.argv[2], sys.argv[3])
	elif cmd == 'border':
		cmd_border(sys.argv[2])
	elif cmd == 'georef':
		cmd_georef(sys.argv[2])
	elif cmd == 'ladder':
		cmd_ladder(sys.argv[2:])
	else:
		print(__doc__)
		sys.exit(2)
	sys.exit(1 if FAILS[0] else 0)
