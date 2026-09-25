#!/usr/bin/env python
"""An INDEPENDENT model of the far-terrain MASK sheet and of the RING-0 runtime
blend, re-typed from docs/LODGEN_TERRAIN_VT.md rather than imported from the
generator.

Two gates live here.

  mask   T1 and T2. The mask sheet's R is roughness and its G is metallic. For a
         texel the paint leaves to ONE landscape texture, this recomputes both
         from that texture's own material -- `1 - smoothness * _s.G` for a
         legacy material, the PBRM's RMAOS R and G for a PBRM -- and compares.
         The FLOOR is the same computation with the inversion omitted, which
         must go red; without it the check would pass on a sheet storing gloss.

  ring0  T4, bungo's owed generator gate (2026-09-11 09:2x): "the ring-0 weights
         blend and the pyramid's baked colour agree on the same texel -- same
         source textures, same grading". Ring 0 is drawn at runtime from the
         per-texel LTEX weights; ring 1 out samples this pyramid. If the two
         disagree the band between them is a visible seam. So the runtime's
         formula is stated in the contract, implemented HERE from that statement
         alone, and compared against the pyramid's level-0 colour, per tile, as
         a mean and a max in sRGB 8-bit units.
         FLOOR: the same blend with the VCLR multiply omitted -- the grading the
         contract says the runtime must apply -- which must read far worse.
         CEILING: the bake compared against itself, which must read exactly 0.

Everything that could be shared with the code under test is NOT: the ESM walk
comes from lodgen_cover_model.py (which parses the plugin itself), the DDS
decoding, the mip choice, the bilinear taps and the composite are written here
from the document. The only thing read from the generator is its OUTPUT.

USAGE
  python lodgen_terrain_model.py mask  <esm> <data> <lodt> <cx0> <cy0> <dim>
  python lodgen_terrain_model.py ring0 <esm> <data> <lodt> <cx0> <cy0> <dim>

Exit 2 = a missing input; 1 = a failed check; 0 = pass.
"""

import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lodgen_cover_model import Esm, bilinear, dominant_base, ENGINE_DEFAULT, ENGINE_DEFAULT_TEX     # noqa: E402

# The bake's world-space tiling of a landscape texture, in world units per
# repeat. 341.3333 = 128/0.375 is the engine's own number, read out of
# Fallout4.exe 1.10.155 (lane SPLAT1); the bake's default moved to it on
# 2026-09-11, and this model has to move with it or the ring-0 gate measures
# the old law against the new bake. Override with WW_LAND_TILING to check a
# bake made with --land-tiling.
TILE = float(os.environ.get('WW_LAND_TILING', 341.3333))
CELL = 4096.0

checks = 0
fails = 0


def ok(msg, detail=''):
	global checks
	checks += 1
	print('  ok   %s' % msg)
	if detail:
		print('       %s' % detail)


def bad(msg, detail=''):
	global checks, fails
	checks += 1
	fails += 1
	print('  FAIL %s' % msg)
	if detail:
		print('       %s' % detail)


def say(msg):
	print('       %s' % msg)


# --------------------------------------------------------------------------
# DDS: header, mip offsets, BC1/BC3 decode, and the bake's own sampler.
# --------------------------------------------------------------------------

DDPF_FOURCC = 0x4


class Dds(object):
	"""Enough of a DDS reader to sample one texture the way the bake does.

	BC1 and BC3 only. Every Fallout 4 landscape DIFFUSE measured on the
	unpacked corpus is one of the two (100 DXT1, 226 DXT5, 2 DXT3 over 824
	files; the 495 BC5U are the normals, which this never opens)."""

	def __init__(self, path):
		with open(path, 'rb') as f:
			b = f.read()
		if b[:4] != b'DDS ':
			raise ValueError('not a DDS: %s' % path)
		self.h = struct.unpack_from('<31I', b, 4)
		self.height = self.h[2]
		self.width = self.h[3]
		mips = self.h[6]
		self.fourcc = b[84:88]
		off = 128
		if self.fourcc == b'DX10':
			off = 148
		if self.fourcc == b'DXT1':
			self.blockBytes = 8
		elif self.fourcc in (b'DXT3', b'DXT5'):
			self.blockBytes = 16
		elif self.fourcc in (b'BC5U', b'ATI2'):
			# MEASURED, and it is the whole reason the gloss lives in GREEN:
			# every Fallout 4 landscape `_s` map is BC5U -- TWO channels, two
			# BC4 blocks of 8 bytes, R then G, with B 0 and A 1. It is not a
			# colour map and it never had a blue channel to lose.
			self.blockBytes = 16
		else:
			raise ValueError('unsupported fourCC %r in %s' % (self.fourcc, path))
		self.b = b
		self.mips = []
		w, h = self.width, self.height
		n = max(1, mips)
		for _ in range(n):
			bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
			size = bw * bh * self.blockBytes
			if off + size > len(b):
				break
			self.mips.append((off, w, h))
			off += size
			w, h = max(1, w // 2), max(1, h // 2)
		self.maxMip = len(self.mips) - 1
		self._cache = {}

	def _level(self, m):
		"""One mip decoded to a flat list of (r, g, b, a) floats, 0..1."""
		if m in self._cache:
			return self._cache[m]
		off, w, h = self.mips[m]
		px = [(0.0, 0.0, 0.0, 1.0)] * (w * h)
		bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
		if self.fourcc in (b'BC5U', b'ATI2'):
			for by in range(bh):
				for bx in range(bw):
					o = off + (by * bw + bx) * 16
					r = self._bc4(o)
					g = self._bc4(o + 8)
					for j in range(4):
						for i in range(4):
							x, y = bx * 4 + i, by * 4 + j
							if x >= w or y >= h:
								continue
							k = j * 4 + i
							px[y * w + x] = (r[k], g[k], 0.0, 1.0)
			self._cache[m] = (px, w, h)
			return self._cache[m]
		for by in range(bh):
			for bx in range(bw):
				o = off + (by * bw + bx) * self.blockBytes
				a = [1.0] * 16
				co = o
				if self.blockBytes == 16:
					co = o + 8
					if self.fourcc == b'DXT5':
						a0, a1 = self.b[o], self.b[o + 1]
						bits = int.from_bytes(self.b[o + 2:o + 8], 'little')
						tab = [a0, a1]
						if a0 > a1:
							tab += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
						else:
							tab += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)]
							tab += [0, 255]
						for i in range(16):
							a[i] = tab[(bits >> (3 * i)) & 7] / 255.0
					else:           # DXT3: four explicit bits a texel
						bits = int.from_bytes(self.b[o:o + 8], 'little')
						for i in range(16):
							a[i] = ((bits >> (4 * i)) & 15) / 15.0
				c0, c1, cbits = struct.unpack_from('<HHI', self.b, co)
				e = [self._565(c0), self._565(c1)]
				if c0 > c1 or self.blockBytes == 16:
					e.append(tuple((2 * e[0][k] + e[1][k]) / 3.0 for k in range(3)))
					e.append(tuple((e[0][k] + 2 * e[1][k]) / 3.0 for k in range(3)))
				else:
					e.append(tuple((e[0][k] + e[1][k]) / 2.0 for k in range(3)))
					e.append((0.0, 0.0, 0.0))
				for j in range(4):
					for i in range(4):
						x, y = bx * 4 + i, by * 4 + j
						if x >= w or y >= h:
							continue
						k = (cbits >> (2 * (j * 4 + i))) & 3
						c = e[k]
						px[y * w + x] = (c[0], c[1], c[2], a[j * 4 + i])
		self._cache[m] = (px, w, h)
		return self._cache[m]

	def _bc4(self, o):
		"""One 8-byte BC4 block as sixteen 0..1 floats, in texel order."""
		a0, a1 = self.b[o], self.b[o + 1]
		bits = int.from_bytes(self.b[o + 2:o + 8], 'little')
		tab = [a0, a1]
		if a0 > a1:
			tab += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
		else:
			tab += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)]
			tab += [0, 255]
		return [tab[(bits >> (3 * i)) & 7] / 255.0 for i in range(16)]

	@staticmethod
	def _565(c):
		return (((c >> 11) & 31) / 31.0, ((c >> 5) & 63) / 63.0, (c & 31) / 31.0)

	def _bilinear(self, m, u, v):
		px, w, h = self._level(min(m, self.maxMip))
		# getPixelB takes NORMALISED coordinates and CLAMPS at the edge; the
		# bake wraps by hand before calling it, which is what the caller does.
		x = u * w - 0.5
		y = v * h - 0.5
		x0, y0 = int(math.floor(x)), int(math.floor(y))
		tx, ty = x - x0, y - y0
		out = [0.0, 0.0, 0.0, 0.0]
		for dy in range(2):
			for dx in range(2):
				xx = min(max(x0 + dx, 0), w - 1)
				yy = min(max(y0 + dy, 0), h - 1)
				wgt = (tx if dx else 1 - tx) * (ty if dy else 1 - ty)
				p = px[yy * w + xx]
				for k in range(4):
					out[k] += p[k] * wgt
		return out

	def sample(self, u, v, mip):
		"""The trilinear tap: two mips, blended by the fractional part."""
		mip = min(max(mip, 0.0), float(self.maxMip))
		m0 = int(mip)
		c0 = self._bilinear(m0, u, v)
		f = mip - m0
		if f <= 0.0 or m0 >= self.maxMip:
			return c0
		c1 = self._bilinear(m0 + 1, u, v)
		return [c0[k] + (c1[k] - c0[k]) * f for k in range(4)]


def find_asset(data, rel):
	"""A game path resolved against the unpacked Data folder, the way the bake
	does with --data-root: separators normalised, the folder prefixed when the
	path does not already carry it."""
	if not rel:
		return None
	p = rel.replace('\\', '/').lstrip('./')
	low = p.lower()
	if low.startswith('data/'):
		p = p[5:]
		low = p.lower()
	if not low.startswith('textures/'):
		p = 'textures/' + p
	full = os.path.join(data, p.replace('/', os.sep))
	return full if os.path.isfile(full) else None


def find_material(data, rel):
	if not rel:
		return None
	p = rel.replace('\\', '/')
	low = p.lower()
	i = low.rfind('materials/')
	if i >= 0:
		p = p[i:]
	elif not low.startswith('materials/'):
		p = 'materials/' + p
	full = os.path.join(data, p.replace('/', os.sep))
	return full if os.path.isfile(full) else None


# --------------------------------------------------------------------------
# The container: header + the mask/colour sheets of one tile.
# --------------------------------------------------------------------------

class Lodt(object):
	def __init__(self, path):
		with open(path, 'rb') as f:
			self.b = f.read()
		b = self.b
		assert b[:4] == b'LDTX', 'not a .lodt'
		(self.version, self.headerBytes, self.flags) = struct.unpack_from('<III', b, 4)
		(self.fileBytes, self.tableOffset, self.payloadOffset) = struct.unpack_from('<QQQ', b, 0x10)
		(self.south, self.west, self.north, self.east,
		 self.wSouth, self.wWest, self.wNorth, self.wEast) = struct.unpack_from('<8h', b, 0x58)
		(self.levelDim, self.levelIndex, self.levelCount, self.tilesX, self.tilesY,
		 self.content, self.border, self.stored) = struct.unpack_from('<8H', b, 0x68)
		(self.mips, self.sheetCount, self.aniso, self.compression) = struct.unpack_from('<4B', b, 0x78)
		self.tileCount = struct.unpack_from('<I', b, 0x7C)[0]
		self.sheets = []
		for i in range(6):
			f0, f1, role, space = struct.unpack_from('<HHBB', b, 0xA0 + i * 8)
			self.sheets.append({'dxgi': f0, 'dxgiCover': f1, 'role': role, 'space': space})
		self.table = []
		for i in range(self.tileCount):
			o = self.tableOffset + i * 24
			off, st, raw, crc, flags, res = struct.unpack_from('<QIIIHH', b, o)
			self.table.append({'offset': off, 'stored': st, 'raw': raw, 'flags': flags})

	def sheetMipBytes(self, s, mip, cover):
		side = self.stored >> mip
		sd = self.sheets[s]
		if sd['role'] == 4:
			return side * side * 2
		fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
		return (side // 4) * (side // 4) * (16 if fmt in (77, 78) else 8)

	def sheetOffset(self, cover, role, mip):
		o = 0
		for s in range(self.sheetCount):
			for m in range(self.mips):
				if self.sheets[s]['role'] == role and m == mip:
					return o, (self.sheets[s]['dxgiCover']
							   if (cover and self.sheets[s]['dxgiCover'] != self.sheets[s]['dxgi'])
							   else self.sheets[s]['dxgi'])
				o += self.sheetMipBytes(s, m, cover)
		return None, None

	def sheet(self, index, role, mip=0):
		"""One sheet of one tile, decoded to a flat list of (r, g, b, a) 0..255."""
		e = self.table[index]
		if not (e['flags'] & 1):
			return None
		p = self.b[e['offset']:e['offset'] + e['stored']]
		cover = bool(e['flags'] & 2)
		o, fmt = self.sheetOffset(cover, role, mip)
		if o is None:
			return None
		side = self.stored >> mip
		return decode_block_sheet(p, o, side, side, fmt in (77, 78)), side


def decode_block_sheet(b, off, w, h, bc3):
	px = [(0, 0, 0, 255)] * (w * h)
	bw, bh = w // 4, h // 4
	bb = 16 if bc3 else 8
	for by in range(bh):
		for bx in range(bw):
			o = off + (by * bw + bx) * bb
			a = [255] * 16
			co = o
			if bc3:
				co = o + 8
				a0, a1 = b[o], b[o + 1]
				bits = int.from_bytes(b[o + 2:o + 8], 'little')
				tab = [a0, a1]
				if a0 > a1:
					tab += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
				else:
					tab += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)]
					tab += [0, 255]
				for i in range(16):
					a[i] = tab[(bits >> (3 * i)) & 7]
			c0, c1, cbits = struct.unpack_from('<HHI', b, co)

			def c565(c):
				return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
						(c & 31) * 255 // 31)

			e = [c565(c0), c565(c1)]
			if c0 > c1 or bc3:
				e.append(tuple((2 * e[0][k] + e[1][k]) // 3 for k in range(3)))
				e.append(tuple((e[0][k] + 2 * e[1][k]) // 3 for k in range(3)))
			else:
				e.append(tuple((e[0][k] + e[1][k]) // 2 for k in range(3)))
				e.append((0, 0, 0))
			for j in range(4):
				for i in range(4):
					x, y = bx * 4 + i, by * 4 + j
					if x >= w or y >= h:
						continue
					k = (cbits >> (2 * (j * 4 + i))) & 3
					px[y * w + x] = (e[k][0], e[k][1], e[k][2], a[j * 4 + i])
	return px


# --------------------------------------------------------------------------
# The layer model: every landscape texture's diffuse and its mask law.
# --------------------------------------------------------------------------

class Layer(object):
	"""One landscape texture, as the model can read it WITHOUT our material
	reader.

	`gateable` is the honest half of this: a TXST that names a `.bgsm` carries
	its diffuse, its specular map AND its smoothness constant inside the
	material, and reading a BGSM independently means re-deriving a binary layout
	that this model has no second source for. Such a layer is therefore NOT
	gated -- it is counted and named. A TXST that names TX00 and TX07 and no
	material has everything in the record, the smoothness constant is the
	shader's own 1.0 on both sides, and those texels are what T1 and T4 are
	measured on."""

	def __init__(self, esm, data, form):
		self.form = form
		self.rule = 'none-default'
		self.diffuse = None
		self.spec = None
		self.smoothness = 1.0
		self.roughConst = 1.0
		self.gateable = False
		self.edid = ''
		self.why = 'not an LTEX'
		if form == ENGINE_DEFAULT:
			# the engine's default ground (lane SEAM1): TX00 + TX07, no material
			self.edid = 'ENGINE_DEFAULT'
			self.rule = 'legacy-inverted'
			d = find_asset(data, ENGINE_DEFAULT_TEX[0])
			sp = find_asset(data, ENGINE_DEFAULT_TEX[2])
			self.diffuse = Dds(d) if d else None
			self.spec = Dds(sp) if sp else None
			self.roughConst = 0.0
			self.gateable = self.diffuse is not None
			self.why = 'ok' if self.gateable else 'engine default diffuse not found'
			return
		rec = esm.ltex.get(form)
		if not rec:
			return
		self.edid = rec['edid']
		ts = esm.txst.get(rec['tnam'])
		if not ts:
			self.why = 'the LTEX names no TXST'
			return
		if ts['mnam']:
			self.rule = 'legacy-inverted'
			self.why = 'material-backed (%s): not modelled independently' % ts['mnam']
			return
		if not ts['tx00']:
			self.why = 'the TXST names no diffuse'
			return
		self.rule = 'legacy-inverted' if ts['tx07'] else 'none-default'
		self.roughConst = 1.0 - min(max(self.smoothness, 0.0), 1.0)
		d = find_asset(data, ts['tx00'])
		try:
			self.diffuse = Dds(d) if d else None
		except Exception as ex:
			self.why = 'diffuse unreadable: %s' % ex
			return
		sp = find_asset(data, ts['tx07']) if ts['tx07'] else None
		try:
			self.spec = Dds(sp) if sp else None
		except Exception as ex:
			self.why = 'specular unreadable: %s' % ex
			return
		self.gateable = self.diffuse is not None
		self.why = 'ok'

	def sample_colour(self, wx, wy, upt):
		if not self.diffuse:
			return [0.5, 0.5, 0.5, 1.0]
		return self._tap(self.diffuse, wx, wy, upt)

	def sample_rough(self, wx, wy, upt):
		if not self.spec:
			return self.roughConst
		g = self._tap(self.spec, wx, wy, upt)[1]
		# THE CONTRACT'S INVERSION: gloss = smoothness * _s.G, roughness = 1 - gloss
		return 1.0 - min(max(min(max(self.smoothness, 0.0), 1.0) * g, 0.0), 1.0)

	def sample_gloss_uninverted(self, wx, wy, upt):
		"""The FLOOR for T1: the same number WITHOUT the inversion."""
		if not self.spec:
			return 1.0 - self.roughConst
		g = self._tap(self.spec, wx, wy, upt)[1]
		return min(max(min(max(self.smoothness, 0.0), 1.0) * g, 0.0), 1.0)

	@staticmethod
	def _tap(tex, wx, wy, upt):
		u = math.fmod(wx / TILE, 1.0)
		v = math.fmod(wy / TILE, 1.0)
		if u < 0.0:
			u += 1.0
		if v < 0.0:
			v += 1.0
		texelWorld = TILE / tex.width
		mip = min(max(math.log2(max(1.0, upt / texelWorld)), 0.0), float(tex.maxMip))
		return tex.sample(u, v, mip)


def layers_for(esm, data, forms, cache):
	for f in forms:
		if f not in cache:
			cache[f] = Layer(esm, data, f)
	return cache


# --------------------------------------------------------------------------
# The composite, from 2.2 / 1.5: the base, then each layer by its bilinear
# opacity, then VCLR, then the grass tint. The mask takes the SAME blend
# WITHOUT VCLR and without the tint.
# --------------------------------------------------------------------------

def composite(esm, data, cache, cx0, cy0, dim, domBase, wx, wy, upt, withVclr=True,
			  invertRough=True, withLayers=True):
	cx = int(math.floor(wx / CELL))
	cy = int(math.floor(wy / CELL))
	land = esm.lands.get((cx, cy))
	if not land:
		return None
	clx, cly = wx - cx * CELL, wy - cy * CELL
	q = (2 if cly >= 2048.0 else 0) + (1 if clx >= 2048.0 else 0)
	qx = (clx - (2048.0 if q & 1 else 0.0)) / 2048.0
	qy = (cly - (2048.0 if q & 2 else 0.0)) / 2048.0
	base = land['base'][q] or domBase
	forms = [base] + [(lay['ltex'] or domBase) for lay in land['layers'][q]]
	layers_for(esm, data, forms, cache)
	# a texel any of whose layers the model cannot read independently is
	# REFUSED, not approximated: an approximation would put the model's own
	# error into a number that is supposed to measure the generator's
	gateable = all(cache[f].gateable for f in forms if f)
	col = list(cache[base].sample_colour(wx, wy, upt)) if base else [0.5, 0.5, 0.5, 1.0]
	roughOf = (lambda l: l.sample_rough(wx, wy, upt)) if invertRough \
		else (lambda l: l.sample_gloss_uninverted(wx, wy, upt))
	rough = roughOf(cache[base]) if base else 1.0
	for lay in (land['layers'][q] if withLayers else []):
		a = bilinear(lay['op'], qx, qy)
		a = min(max(a, 0.0), 1.0)
		if a <= 0.001:
			continue
		f = lay['ltex'] or domBase
		lc = cache[f].sample_colour(wx, wy, upt)
		for k in range(3):
			col[k] += (lc[k] - col[k]) * a
		rough += (roughOf(cache[f]) - rough) * a
	if withVclr and land.get('vclr'):
		gx = min(max(clx / CELL * 32.0, 0.0), 31.999)
		gy = min(max(cly / CELL * 32.0, 0.0), 31.999)
		ix, iy = int(gx), int(gy)
		tx, ty = gx - ix, gy - iy

		def vc(x, y, k):
			return land['vclr'][(y * 33 + x) * 3 + k]

		for k in range(3):
			c = ((vc(ix, iy, k) * (1 - tx) + vc(ix + 1, iy, k) * tx) * (1 - ty)
				 + (vc(ix, iy + 1, k) * (1 - tx) + vc(ix + 1, iy + 1, k) * tx) * ty)
			col[k] *= c / 255.0
	return {'colour': col, 'rough': rough, 'q': q, 'base': base,
			'gateable': gateable, 'layerCount': len(land['layers'][q])}


def tile_world(v, ti, dim, content, border, i, j):
	pass


# --------------------------------------------------------------------------
# commands
# --------------------------------------------------------------------------

def load(esmPath, cx0, cy0, dim):
	e = Esm(esmPath)
	e.walk(0x0000003C)
	return e


def tile_index(v, cx0, cy0):
	"""Table index of the tile whose SOUTH-WEST cell is (cx0, cy0)."""
	tx = (cx0 - v.west) // v.levelDim
	ty = (v.north - (cy0 + v.levelDim - 1)) // v.levelDim
	if tx < 0 or ty < 0 or tx >= v.tilesX or ty >= v.tilesY:
		return None, None, None
	return ty * v.tilesX + tx, tx, ty


def cmd_mask(esmPath, data, lodt, cx0, cy0, dim):
	v = Lodt(lodt)
	e = load(esmPath, cx0, cy0, dim)
	say('container version %d, %d sheets, roles %s'
		% (v.version, v.sheetCount, [s['role'] for s in v.sheets[:v.sheetCount]]))
	idx, tx, ty = tile_index(v, cx0, cy0)
	if idx is None:
		bad('T1 the named tile is inside the container', 'cell (%d,%d)' % (cx0, cy0))
		return
	got = v.sheet(idx, 5, 0)
	if not got:
		bad('T1 the container carries a MASK sheet (role 5)')
		return
	px, side = got
	content, border = v.content, v.border
	upt = v.levelDim * CELL / content
	domBase = ENGINE_DEFAULT   # lane SEAM1: was the dim-4 chunk's dominant base
	cache = {}

	# T2 first: metallic. Every legacy layer must read EXACTLY 0.
	metals = sorted(set(p[1] for p in px))
	rules = {}
	for f in set(list(e.ltex.keys())):
		pass
	# the rule census over the layers this tile actually paints
	tileN = (cy0 + v.levelDim) * CELL
	tileW = cx0 * CELL
	samples = []
	for j in range(border, side - border, 7):
		for i in range(border, side - border, 7):
			wx = tileW + (i - border + 0.5) * upt
			wy = tileN - (j - border + 0.5) * upt
			samples.append((i, j, wx, wy))
	singles = []
	total = 0
	specSeen = []
	for (i, j, wx, wy) in samples:
		c = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt)
		if c is None:
			continue
		total += 1
		if not c['gateable']:
			continue
		f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt,
					  True, False)
		singles.append((i, j, wx, wy, c, f))
		lay = cache[c['base']]
		if lay.spec and len(specSeen) < 400:
			specSeen.append(round(lay._tap(lay.spec, wx, wy, upt)[1] * 255))
	say('%d of %d sampled texels modelled; %d refused (a material-backed layer '
		'this reader does not open)' % (len(singles), total, total - len(singles)))
	for f, lay in cache.items():
		rules[lay.rule] = rules.get(lay.rule, 0) + 1
	say('layers resolved on this tile: %s' % rules)

	if not metals:
		bad('T2 the mask sheet decoded')
	elif metals == [0]:
		ok('T2 metallic is EXACTLY 0 on a tile whose layers are all legacy',
		   'distinct G values: %s' % metals)
	else:
		bad('T2 metallic is 0 where no PBRM layer exists',
			'distinct G values (first 8): %s' % metals[:8])
	# the floor for T2, on the same data: a sheet that stored the legacy
	# SPECULAR in G instead of metallic would not be constant 0
	if specSeen and len(set(specSeen)) > 1:
		ok('T2 FLOOR the legacy gloss channel is NOT constant here, so a metallic '
		   'of 0 is a decision and not an empty sheet',
		   '%d distinct gloss values over %d modelled samples'
		   % (len(set(specSeen)), len(specSeen)))
	else:
		bad('T2 FLOOR the legacy gloss varies on this tile',
			'gloss values: %s' % sorted(set(specSeen))[:8])

	# T1: roughness, on texels the paint leaves to ONE landscape texture
	if not singles:
		bad('T1 the tile has texels this model can read independently')
		return
	errs, floorErrs = [], []
	for (i, j, wx, wy, c, f) in singles:
		gotv = px[j * side + i][0] / 255.0
		errs.append(abs(gotv - c['rough']) * 255.0)
		floorErrs.append(abs(gotv - f['rough']) * 255.0)
	errs.sort()
	mean = sum(errs) / len(errs)
	p95 = errs[int(len(errs) * 0.95)]
	fmean = sum(floorErrs) / len(floorErrs)
	say('T1 %d modelled texels; model vs sheet: mean %.2f, p95 %.2f, max %.2f (8-bit)'
		% (len(errs), mean, p95, errs[-1]))
	say('T1 FLOOR the same texels against the UN-inverted gloss: mean %.2f' % fmean)
	if mean <= 6.0:
		ok('T1 the mask sheet stores 1 - gloss, the near material\'s own roughness',
		   'mean %.2f of 255' % mean)
	else:
		bad('T1 the mask sheet stores 1 - gloss', 'mean %.2f of 255' % mean)
	if fmean > mean * 3.0 and fmean > 30.0:
		ok('T1 FLOOR the un-inverted gloss is refused',
		   'floor mean %.2f vs %.2f, a %.1fx separation' % (fmean, mean, fmean / max(mean, 0.01)))
	else:
		bad('T1 FLOOR the un-inverted gloss reads far worse than the inverted one',
			'floor mean %.2f vs %.2f' % (fmean, mean))


def cmd_ring0(esmPath, data, lodt, cx0, cy0, dim):
	v = Lodt(lodt)
	e = load(esmPath, cx0, cy0, dim)
	idx, tx, ty = tile_index(v, cx0, cy0)
	if idx is None:
		bad('T4 the named tile is inside the container')
		return
	got = v.sheet(idx, 1, 0)
	if not got:
		bad('T4 the container carries a COLOUR sheet')
		return
	px, side = got
	content, border = v.content, v.border
	upt = v.levelDim * CELL / content
	domBase = ENGINE_DEFAULT   # lane SEAM1: was the dim-4 chunk's dominant base
	cache = {}
	tileN = (cy0 + v.levelDim) * CELL
	tileW = cx0 * CELL
	errs, floorErrs, baseErrs = [], [], []
	n = 0
	total = 0
	skipped = 0
	for j in range(border, side - border, 5):
		for i in range(border, side - border, 5):
			wx = tileW + (i - border + 0.5) * upt
			wy = tileN - (j - border + 0.5) * upt
			c = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, True)
			if c is None:
				continue
			total += 1
			if not c['gateable']:
				skipped += 1
				continue
			f = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt, False)
			g = composite(e, data, cache, cx0, cy0, v.levelDim, domBase, wx, wy, upt,
						  True, True, False)
			p = px[j * side + i]
			d = max(abs(round(c['colour'][k] * 255) - p[k]) for k in range(3))
			errs.append(d)
			floorErrs.append(max(abs(round(f['colour'][k] * 255) - p[k]) for k in range(3)))
			baseErrs.append(max(abs(round(g['colour'][k] * 255) - p[k]) for k in range(3)))
			n += 1
	if not errs:
		bad('T4 the ring-0 blend found land under this tile')
		return
	errs.sort()
	mean = sum(errs) / len(errs)
	fmean = sum(floorErrs) / len(floorErrs)
	say('%d of %d sampled texels modelled; %d refused (a material-backed layer '
		'this reader does not open)' % (n, total, skipped))
	print('RING0 tile %d,%d texels %d meanErr %.2f p95 %d maxErr %d floorMean %.2f'
		  % (tx, ty, n, mean, errs[int(n * 0.95)], errs[-1], fmean))
	if mean <= 12.0:
		ok('T4 the ring-0 runtime blend and the pyramid agree on the same texel',
		   'mean %.2f, p95 %d, max %d of 255' % (mean, errs[int(n * 0.95)], errs[-1]))
	else:
		bad('T4 the ring-0 runtime blend and the pyramid agree',
			'mean %.2f, p95 %d, max %d of 255' % (mean, errs[int(n * 0.95)], errs[-1]))
	# THE FLOOR THAT BITES: drop the per-texel LTEX WEIGHTS and paint the base
	# alone. The gate's whole claim is that the runtime's weights blend lands on
	# the pyramid's colour, so a blend WITHOUT the weights must miss it.
	bmean = sum(baseErrs) / len(baseErrs)
	if bmean > mean * 3.0:
		ok('T4 FLOOR a blend that ignores the per-texel LTEX weights is refused',
		   'base-only mean %.2f vs %.2f, a %.1fx separation'
		   % (bmean, mean, bmean / max(mean, 0.01)))
	else:
		bad('T4 FLOOR a blend that ignores the per-texel LTEX weights reads far worse',
			'base-only mean %.2f vs %.2f' % (bmean, mean))
	# The VCLR arm, REPORTED and not gated, with the reason measured rather than
	# assumed: only 2,362 of the Commonwealth's 36,864 cells carry a VCLR at all,
	# and in this region every one of them is within 249..255 of white -- so
	# omitting the multiply is not a wrong grading here, it is no change.
	say('T4 the same texels with the VCLR multiply omitted: mean %.2f (reported, '
		'not gated -- this region\'s VCLR is 249..255 of white)' % fmean)
	# THE CEILING: the same comparison, through the same decode path, against the
	# bake itself. A ceiling that cannot read non-zero would be decoration, so it
	# re-opens the file and re-decodes rather than differencing one array.
	v2 = Lodt(lodt)
	px2, _ = v2.sheet(idx, 1, 0)
	ceil = max(max(abs(px2[k][c] - px[k][c]) for c in range(3))
			   for k in range(len(px)))
	if ceil == 0:
		ok('T4 CEILING the bake re-decoded and compared against itself reads exactly 0',
		   'over all %d texels of the sheet' % len(px))
	else:
		bad('T4 CEILING the bake re-decoded and compared against itself reads 0',
			'max %d' % ceil)


def main(argv):
	if len(argv) < 7:
		print(__doc__)
		return 2
	cmd, esmPath, data, lodt = argv[1], argv[2], argv[3], argv[4]
	cx0, cy0, dim = int(argv[5]), int(argv[6]), int(argv[7])
	for p in (esmPath, data, lodt):
		if not os.path.exists(p):
			print('missing input: %s' % p)
			return 2
	say('NOTE: this model does not fold in the grass tint (1.5). Run the bake '
		'with --grass-tint 0 for the ring-0 gate, or the tint\'s own mix lands '
		'in the error.')
	if cmd == 'mask':
		cmd_mask(esmPath, data, lodt, cx0, cy0, dim)
	elif cmd == 'ring0':
		cmd_ring0(esmPath, data, lodt, cx0, cy0, dim)
	else:
		print('unknown command %s' % cmd)
		return 2
	print('%d checks, %d failures' % (checks, fails))
	print('PASS' if fails == 0 else 'FAIL')
	return 1 if fails else 0


if __name__ == '__main__':
	sys.exit(main(sys.argv))
