#!/usr/bin/env python
"""An INDEPENDENT reader of the landscape paint and the grass records, and the
ground-cover model of docs/LODGEN_TERRAIN_VT.md computed from it.

Why this exists rather than a --dump-paint on the CLI: a round-trip test that
shares one table with the code it tests proves nothing (docs/MISTAKES.md: a
manifest round trip passed 38 of 38 with the mapping deliberately wrong). So
this parses the ESM itself -- GRUP walk, zlib-decompressed records, BTXT/ATXT/
VTXT, LTEX GNAM, GRAS DATA -- and re-types the composite, the slope gate and
the quantisation from the document rather than importing anything.

Sub-commands:

  census <esm>                     LTEX/GRAS totals, for the cross-check
  cover <esm> <cx> <cy> <dim> <out.json>
                                   the model's cover plane for one chunk, plus
                                   the paint facts the fixture premises assert
  renorm <esm>                     find the cell whose paint has the most grid
                                   points where the alpha layers sum past 1

Everything is cached: pass --cache <dir> and a parse is reused.
"""

import json
import os
import struct
import sys
import zlib

GRUP = b'GRUP'
REC_HDR = 24


def read_fields(data):
	"""(type, payload) pairs, honouring the XXXX big-field override."""
	out = []
	i = 0
	big = 0
	n = len(data)
	while i + 6 <= n:
		t = data[i:i + 4]
		sz = struct.unpack_from('<H', data, i + 4)[0]
		i += 6
		if t == b'XXXX':
			big = struct.unpack_from('<I', data, i)[0]
			i += sz
			continue
		if big:
			sz = big
			big = 0
		out.append((t, data[i:i + sz]))
		i += sz
	return out


def record_data(buf, off, size, flags):
	raw = buf[off:off + size]
	if flags & 0x00040000:
		if len(raw) < 4:
			return b''
		return zlib.decompress(raw[4:])
	return raw


class Esm(object):
	def __init__(self, path):
		with open(path, 'rb') as f:
			self.buf = f.read()
		self.lands = {}          # (cx, cy) -> parsed paint
		self.ltex = {}           # formid -> {'gnam': [...], 'edid': str}
		self.gras = {}           # formid -> {'density':, 'maxSlope':, 'modl':, 'dataSize':}
		self.ltexOrder = []
		self.grasOrder = []
		self.leftover = 0

	def walk(self, worldTarget):
		buf = self.buf
		n = len(buf)

		def walkRange(start, end, world, cell):
			i = start
			while i + REC_HDR <= end:
				t = buf[i:i + 4]
				size = struct.unpack_from('<I', buf, i + 4)[0]
				if t == GRUP:
					label = struct.unpack_from('<I', buf, i + 8)[0]
					gtype = struct.unpack_from('<I', buf, i + 12)[0]
					sub = (label if gtype == 1 else world)
					walkRange(i + REC_HDR, i + size, sub, cell)
					i += size
					continue
				flags = struct.unpack_from('<I', buf, i + 8)[0]
				formid = struct.unpack_from('<I', buf, i + 12)[0]
				body = record_data(buf, i + REC_HDR, size, flags)
				if t == b'CELL':
					cell = None
					for ft, fd in read_fields(body):
						if ft == b'XCLC' and len(fd) >= 8:
							cell = struct.unpack_from('<ii', fd, 0)
				elif t == b'LAND' and cell is not None and world == worldTarget:
					self.lands[cell] = self.parse_land(body)
				elif t == b'LTEX':
					rec = {'gnam': [], 'edid': ''}
					for ft, fd in read_fields(body):
						if ft == b'EDID':
							rec['edid'] = fd.rstrip(b'\0').decode('latin-1')
						elif ft == b'GNAM' and len(fd) >= 4:
							rec['gnam'].append(struct.unpack_from('<I', fd, 0)[0])
					self.ltex[formid] = rec
					self.ltexOrder.append(formid)
				elif t == b'GRAS':
					rec = {'density': 0, 'minSlope': 0, 'maxSlope': 0, 'modl': '',
						   'dataSize': -1, 'edid': ''}
					for ft, fd in read_fields(body):
						if ft == b'EDID':
							rec['edid'] = fd.rstrip(b'\0').decode('latin-1')
						elif ft == b'MODL':
							rec['modl'] = fd.rstrip(b'\0').decode('latin-1')
						elif ft == b'DATA':
							rec['dataSize'] = len(fd)
							if len(fd) >= 3:
								rec['density'] = fd[0]
								rec['minSlope'] = fd[1]
								rec['maxSlope'] = fd[2]
					self.gras[formid] = rec
					self.grasOrder.append(formid)
				i += REC_HDR + size
			if i != end:
				self.leftover += end - i

		i = 0
		# the TES4 header record first, then the top-level groups
		size = struct.unpack_from('<I', buf, 4)[0]
		i = REC_HDR + size
		walkRange(i, n, 0, None)

	@staticmethod
	def parse_land(body):
		land = {'base': [0, 0, 0, 0], 'layers': [[], [], [], []], 'vclr': None}
		pending = -1
		for t, fd in read_fields(body):
			if t == b'BTXT' and len(fd) >= 8:
				form, quad = struct.unpack_from('<IB', fd, 0)
				if 0 <= quad < 4:
					land['base'][quad] = form
			elif t == b'ATXT' and len(fd) >= 8:
				form, quad = struct.unpack_from('<IB', fd, 0)
				if 0 <= quad < 4:
					land['layers'][quad].append({'ltex': form,
						'op': [[0.0] * 17 for _ in range(17)]})
					pending = quad
				else:
					pending = -1
			elif t == b'VTXT' and pending >= 0 and land['layers'][pending]:
				layer = land['layers'][pending][-1]
				for e in range(len(fd) // 8):
					posn, _pad, op = struct.unpack_from('<HHf', fd, e * 8)
					if posn <= 288:
						layer['op'][posn // 17][posn % 17] = op
				pending = -1
			elif t == b'VCLR' and len(fd) >= 33 * 33 * 3:
				land['vclr'] = list(fd[:33 * 33 * 3])
		return land


def ltex_cover(esm, form):
	"""D and S for one LTEX form: the density SUM over its grasses and the
	density-weighted Max Slope. The tint is NOT modelled here -- it needs the
	grass mesh and its texture, which this reader deliberately does not open."""
	rec = esm.ltex.get(form)
	if rec is None:
		return None                      # dangling: a data error, contributes nothing
	d = 0.0
	s = 0.0
	for g in rec['gnam']:
		gr = esm.gras.get(g)
		if gr is None:
			continue
		d += gr['density']
		s += gr['density'] * gr['maxSlope']
	return (d, (s / d) if d > 0 else 0.0)


def bilinear(op, qx, qy):
	fx = min(max(qx * 16.0, 0.0), 15.999)
	fy = min(max(qy * 16.0, 0.0), 15.999)
	ix, iy = int(fx), int(fy)
	tx, ty = fx - ix, fy - iy
	return ((op[iy][ix] * (1 - tx) + op[iy][ix + 1] * tx) * (1 - ty)
			+ (op[iy + 1][ix] * (1 - tx) + op[iy + 1][ix + 1] * tx) * ty)


def dominant_base(esm, cx0, cy0, dim):
	counts = {}
	for cy in range(dim):
		for cx in range(dim):
			land = esm.lands.get((cx0 + cx, cy0 + cy))
			if not land:
				continue
			for q in range(4):
				if land['base'][q]:
					counts[land['base'][q]] = counts.get(land['base'][q], 0) + 1
	best, form = 0, 0
	# the C++ walks a QMap, i.e. ascending key order, and keeps the first
	# strict maximum; matched here so a tie resolves the same way
	for k in sorted(counts):
		if counts[k] > best:
			best, form = counts[k], k
	return form


def cover_plane(esm, cx0, cy0, dim, coverFull, res=512):
	"""The model's cover plane, north-up, plus the operands a check needs.

	The slope gate wants the SAME unit normal the msn write encodes, which is
	central differences over the chunk's own 33x33-per-cell heightfield -- and
	that heightfield is not parsed here (VHGT is heights, and this model is
	about the paint). So the gate is returned SEPARATELY as its two operands:
	Stex per texel and the per-texel A/Dtex composite. The shell script feeds
	theta in from the bake's own _msn sheet, which is the operand the law
	names, and the gate is applied here. That keeps the composite independent
	and does not pretend to re-derive the heightfield."""
	span = dim * 4096.0
	cwx, cwy = cx0 * 4096.0, cy0 * 4096.0
	dom = dominant_base(esm, cx0, cy0, dim)
	dtex = [[0.0] * res for _ in range(res)]
	stex = [[0.0] * res for _ in range(res)]
	renorm = 0
	cache = {}

	def dsOf(form, allowNull):
		if form == 0:
			form = dom if allowNull else 0
		if form == 0:
			return (0.0, 0.0)
		if form not in cache:
			v = ltex_cover(esm, form)
			cache[form] = (0.0, 0.0) if v is None else v
		return cache[form]

	for py in range(res):
		wy = cwy + (1.0 - (py + 0.5) / res) * span
		for px in range(res):
			wx = cwx + ((px + 0.5) / res) * span
			cx = min(max(int((wx - cwx) / 4096.0), 0), dim - 1)
			cy = min(max(int((wy - cwy) / 4096.0), 0), dim - 1)
			land = esm.lands.get((cx0 + cx, cy0 + cy))
			if not land:
				continue
			lx = (wx - cwx) - cx * 4096.0
			ly = (wy - cwy) - cy * 4096.0
			q = (2 if ly >= 2048.0 else 0) + (1 if lx >= 2048.0 else 0)
			qx = (lx - (2048.0 if (q & 1) else 0.0)) / 2048.0
			qy = (ly - (2048.0 if (q & 2) else 0.0)) / 2048.0
			base = land['base'][q] or dom
			bd, bs = dsOf(base, True)
			a = []
			for layer in land['layers'][q]:
				a.append(min(max(bilinear(layer['op'], qx, qy), 0.0), 1.0))
			A = sum(a)
			if A > 1.0:
				a = [v / A for v in a]
				A = 1.0
				renorm += 1
			wbase = 1.0 - A
			d = wbase * bd
			s = wbase * bd * bs
			for k, layer in enumerate(land['layers'][q]):
				ld, ls = dsOf(layer['ltex'], True) if layer['ltex'] == 0 \
					else dsOf(layer['ltex'], False)
				d += a[k] * ld
				s += a[k] * ld * ls
			dtex[py][px] = d
			stex[py][px] = (s / d) if d > 0 else 0.0
	return {'dtex': dtex, 'stex': stex, 'renorm': renorm, 'dominantBase': dom,
			'coverFull': coverFull}


def cmd_census(esm_path):
	esm = Esm(esm_path)
	esm.walk(0)
	links = sum(len(r['gnam']) for r in esm.ltex.values())
	withg = sum(1 for r in esm.ltex.values() if r['gnam'])
	sizes = [r['dataSize'] for r in esm.gras.values() if r['dataSize'] >= 0]
	print(json.dumps({
		'ltexTotal': len(esm.ltex), 'grasTotal': len(esm.gras),
		'gnamLinks': links, 'ltexWithGnam': withg,
		'grasDataMin': min(sizes) if sizes else -1,
		'grasDataMax': max(sizes) if sizes else -1,
		'grasWithoutData': sum(1 for r in esm.gras.values() if r['dataSize'] < 0),
		'leftover': esm.leftover,
		'noGrassEdids': sorted(r['edid'] for r in esm.ltex.values()
							   if r['edid'].endswith('NoGrass')),
	}))


def cmd_cover(esm_path, cx, cy, dim, coverFull, out):
	esm = Esm(esm_path)
	esm.walk(0x3C)
	m = cover_plane(esm, cx, cy, dim, coverFull)
	# the fixture premises, asserted rather than described
	cells = 0
	ltexIds = set()
	baseD = []
	nullLayers = 0
	alphaLayers = 0
	for y in range(dim):
		for x in range(dim):
			land = esm.lands.get((cx + x, cy + y))
			if not land:
				continue
			cells += 1
			for q in range(4):
				if land['base'][q]:
					ltexIds.add(land['base'][q])
					v = ltex_cover(esm, land['base'][q])
					baseD.append(0.0 if v is None else v[0])
				for layer in land['layers'][q]:
					alphaLayers += 1
					if layer['ltex']:
						ltexIds.add(layer['ltex'])
					else:
						nullLayers += 1
	edids = sorted(esm.ltex[f]['edid'] for f in ltexIds if f in esm.ltex)
	dangling = sorted('%X' % f for f in ltexIds if f not in esm.ltex)
	json.dump({
		'cells': cells, 'dim': dim, 'cx': cx, 'cy': cy,
		'ltexEdids': edids, 'danglingLtex': dangling,
		'nullLayers': nullLayers, 'alphaLayers': alphaLayers,
		'maxBaseD': max(baseD) if baseD else 0.0,
		'dominantBase': '%X' % m['dominantBase'],
		'renorm': m['renorm'], 'coverFull': coverFull,
		'dtex': m['dtex'], 'stex': m['stex'],
	}, open(out, 'w'))
	print('  model: %d cells, %d LTEX, %d alpha layers (%d NULL), renorm %d, maxBaseD %.1f'
		  % (cells, len(edids), alphaLayers, nullLayers, m['renorm'],
			 max(baseD) if baseD else 0.0))


def cmd_renorm(esm_path):
	"""The cell whose paint has the most grid points where the alpha layers
	sum past 1 -- the one place a cover-side renormalisation can leak into the
	colour composite, and a fixture that has none cannot expose it."""
	esm = Esm(esm_path)
	esm.walk(0x3C)
	best = None
	total = 0
	for (cx, cy), land in esm.lands.items():
		n = 0
		for q in range(4):
			if len(land['layers'][q]) < 2:
				continue
			for r in range(17):
				for c in range(17):
					if sum(l['op'][r][c] for l in land['layers'][q]) > 1.0:
						n += 1
		total += n
		if n and (best is None or n > best[2]):
			best = (cx, cy, n)
	print(json.dumps({'cell': list(best[:2]) if best else None,
					  'points': best[2] if best else 0,
					  'totalPoints': total,
					  'cells': len(esm.lands)}))


if __name__ == '__main__':
	if len(sys.argv) < 3:
		print(__doc__)
		sys.exit(2)
	cmd = sys.argv[1]
	if cmd == 'census':
		cmd_census(sys.argv[2])
	elif cmd == 'cover':
		cmd_cover(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]),
				  float(sys.argv[6]), sys.argv[7])
	elif cmd == 'renorm':
		cmd_renorm(sys.argv[2])
	else:
		print('unknown sub-command %s' % cmd)
		sys.exit(2)
