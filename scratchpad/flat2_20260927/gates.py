#!/usr/bin/env python
"""Lane FLAT2 gates, on three bake trees of the same command line:
  RUNG = release/NifSkope.before_flat2.exe (night-trial @ 5b338d39)
  OFF  = the new exe with --no-collapse-uniform
  ON   = the new exe, default (one-value sheets collapsed)

G1  OFF == RUNG, every DATA file byte for byte; the three provenance files (chunk .key,
    .lodb bake record, flat_objects_report) differ only in their named words (see
    provenance_same). The comparator is shown RED first on a flipped data byte, a
    flipped chunk-key 'bto' byte and a removed file.
G2  ON vs OFF: every non-.lodt file identical; per .lodt the header differs only
    in fileBytes and indexCrc32 (both recomputed); per present tile:
      - flags: ON = OFF | one-value bits, nothing else
      - every NON-collapsed sheet: ON's stored bytes == OFF's bytes
      - every collapsed sheet: OFF's sheet, decoded over every texel of every
        mip, is ONE value (range 0 per channel = case a), and that value is the
        decode of ON's 16-byte record; the record's pad is zero
      - the ON payload EXPANDED == OFF's payload, byte for byte
      - tile crc32 recomputed over ON's stored bytes
    plus: case-a sheets in OFF that were NOT collapsed (counted, with why).
R   Refuters, each must come back RED from the same functions:
      R1 a NON-uniform sheet forced to collapse (flag set, record = its first block)
      R2 a collapsed record's block swapped for a two-colour block
      R3 a collapsed record's pad byte set
    and --make-doctored DIR writes R2/R3 (with every CRC fixed up so the
    in-tree validator reaches rule 16c) for `--lodt-check`.

usage: gates.py RUNG OFF ON [--make-doctored DIR]
Prints one verdict line per gate and the counts table; exit 1 on any failure.
"""
import os
import re
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
sys.path.insert(0, os.path.join(HERE, 'work'))
import lodgen_vt_check as vc  # noqa: E402  (the in-tree checker, uniform-aware)
import vtread  # noqa: E402

ROLE = {1: 'colour', 2: 'msn', 4: 'height', 5: 'mask', 6: 'emissive', 7: 'horizon'}
FAILS = []


def verdict(name, ok, detail=''):
	print('%s  %s%s' % ('PASS' if ok else 'FAIL', name, ('  -- ' + detail) if detail else ''))
	if not ok:
		FAILS.append(name)


# ------------------------------------------------------------------ trees --
def tree(root):
	out = {}
	for d, _, fs in os.walk(root):
		for f in fs:
			p = os.path.join(d, f)
			rel = os.path.relpath(p, root).replace('\\', '/')
			if rel == 'bake.log':
				continue          # the run's own log: timings and paths, not output
			out[rel] = p
	return out


# Provenance files: words that are FUNCTIONS of the run, not of the data (skill ww-module-off-is-identical 4b).
#   <chunk>.key                 : 'inputs' (hashes the generator exe) and 'switches' (hashes the command line);
#                                 its 'bto' and 'manifest' lines are the chunk's DATA and must be equal
#   *.flat_objects_report.txt   : names the bake's own folders; equal once the root name is masked
#   *.lodb (the bake record)    : paths, clock, exe/switch/input hashes, timings; every printable run must
#                                 cancel once paths, digits and 40-hex hashes are masked, except the switch
#                                 token --no-collapse-uniform and this lane's census words on the 'vt:' line
HEX40 = re.compile(rb'\b[0-9a-f]{40}\b')
FLAT2_VT_WORDS = re.compile(rb' collapseUniform # uniformColour # uniformMsn # uniformMask # uniformHeight #'
							rb'( uniformEmissive #)? uniformBytesSaved #')


def _mask_root(b, root):
	name = os.path.basename(os.path.normpath(root)).encode()
	for sep in (b'/', b'\\'):
		b = b.replace(b'bakes' + sep + name + sep, b'bakes' + sep + b'X' + sep)
	return b.replace(b'run_rung', b'run_X').replace(b'run_new', b'run_X')


def _norm_run(s, root):
	s = HEX40.sub(b'H', _mask_root(s, root))
	s = re.sub(rb'[0-9]+', b'#', s)
	if s.startswith(b'vt: '):
		s = FLAT2_VT_WORDS.sub(b'', s)
	return s


def provenance_same(rel, ba, bb, ra, rb_):
	"""(True, why) when two differing files differ only in their named provenance words."""
	if rel.endswith('.key'):
		la = [l for l in ba.splitlines() if not l.startswith((b'inputs ', b'switches '))]
		lb = [l for l in bb.splitlines() if not l.startswith((b'inputs ', b'switches '))]
		return (la == lb and len(la) >= 2), 'chunk key: bto + manifest lines equal'
	if rel.endswith('flat_objects_report.txt'):
		return _mask_root(ba, ra) == _mask_root(bb, rb_), 'report: equal once the bake root is masked'
	if rel.endswith('.lodb'):
		from collections import Counter
		na = Counter(_norm_run(s, ra) for s in re.findall(rb'[\x20-\x7e]{6,}', ba))
		nb = Counter(_norm_run(s, rb_) for s in re.findall(rb'[\x20-\x7e]{6,}', bb))
		left = (na - nb) + (nb - na)
		# the one extra switch token is recorded as a 'switch' entry holding '--no-collapse-uniform'
		if left.get(b'--no-collapse-uniform') == 1 and left.get(b'switch') == 1:
			left = {k: v for k, v in left.items() if k not in (b'--no-collapse-uniform', b'switch')}
		return (not left), 'bake record: residual runs %s' % (list(left)[:3] if left else 'none')
	return False, 'not a provenance file'


def cmp_trees(a, b, doctor=None):
	"""-> (files in both, data files that differ, provenance files that differ only in provenance words,
	only in a, only in b). A doctored control flips a byte in the middle DATA file ('flip'), in a chunk
	key's 'bto' line ('keyflip'), or removes the middle file ('remove')."""
	ta, tb = tree(a), tree(b)
	if doctor == 'remove':
		k = sorted(ta)[len(ta) // 2]
		del ta[k]
	both = sorted(set(ta) & set(tb))
	data = [k for k in both if not k.endswith(('.key', '.lodb', 'flat_objects_report.txt'))]
	keys = [k for k in both if k.endswith('.key')]
	diff, prov = [], []
	for k in both:
		ba = open(ta[k], 'rb').read()
		bb = open(tb[k], 'rb').read()
		if doctor == 'flip' and data and k == data[len(data) // 2]:
			ba = bytearray(ba)
			ba[len(ba) // 2] ^= 0xFF
			ba = bytes(ba)
		if doctor == 'keyflip' and keys and k == keys[0]:
			i = ba.index(b'bto ') + 4
			ba = ba[:i] + (b'0' if ba[i:i + 1] != b'0' else b'1') + ba[i + 1:]
		if doctor == 'lodbflip' and k.endswith('.lodb'):
			assert b'Fallout4.esm' in ba
			ba = ba.replace(b'Fallout4.esm', b'Gallout4.esm', 1)
		if ba == bb:
			continue
		ok, _ = provenance_same(k, ba, bb, a, b)
		(prov if ok else diff).append(k)
	onlyA = sorted(set(ta) - set(tb))
	onlyB = sorted(set(tb) - set(ta))
	return len(both), diff, prov, onlyA, onlyB


def g1(rung, off):
	for doc in ('flip', 'keyflip', 'lodbflip', 'remove'):
		n, d, p, oa, ob = cmp_trees(rung, off, doc)
		red = bool(d or oa or ob)
		verdict('G1 control (%s) is red' % doc, red,
				'%d files in both, %d differ, %d only in rung, %d only in off' % (n, len(d), len(oa), len(ob)))
	n, d, p, oa, ob = cmp_trees(rung, off)
	verdict('G1 OFF == RUNG: every data file byte for byte, provenance files differ only in provenance words',
			n > 0 and not d and not oa and not ob,
			'%d files in both, %d data files differ, %d provenance files differ only in their named words '
			'(%d .key, %d .lodb, %d report), %d only in rung, %d only in off%s' % (
				n, len(d), len(p), sum(k.endswith('.key') for k in p), sum(k.endswith('.lodb') for k in p),
				sum(k.endswith('report.txt') for k in p), len(oa), len(ob), ('; first: ' + d[0]) if d else ''))
	lodt = [k for k in tree(rung) if k.endswith('.lodt')]
	verdict('G1 every .lodt OFF == RUNG byte for byte', len(lodt) > 0 and not [k for k in d if k.endswith('.lodt')],
			'%d .lodt files' % len(lodt))


# ------------------------------------------------------------------ sheets --
def sheet_full_bytes(L, s, cover):
	return sum(L.sheetMipBytes(s, m, cover) for m in range(L.mips))


def decode_sheet(L, s, cover, raw):
	"""Every texel of every mip of one sheet's FULL bytes -> (N, channels) int32."""
	role = L.sheets[s]['role']
	unit = L.unitBytes(s, cover)
	parts = []
	o = 0
	for m in range(L.mips):
		n = L.sheetMipBytes(s, m, cover)
		seg = np.frombuffer(raw[o:o + n], np.uint8)
		o += n
		if n == 0:
			continue
		if role == 4:
			parts.append(seg.view('<u2').astype(np.int32)[:, None])
			continue
		if role == 7:
			parts.append(seg.reshape(-1, 4).astype(np.int32))
			continue
		side = L.stored >> (m + L.sheets[s]['skip'])
		nb = side // 4
		blk = seg.reshape(nb, nb, unit)
		if unit == 16:
			col = vtread.decode_bc1_blocks(blk[..., 8:16], four=True)
			alp = vtread.decode_bc3_alpha(blk[..., 0:8])
		else:
			col = vtread.decode_bc1_blocks(blk, four=False)
			c0 = blk[..., 0].astype(np.uint16) | (blk[..., 1].astype(np.uint16) << 8)
			c1 = blk[..., 2].astype(np.uint16) | (blk[..., 3].astype(np.uint16) << 8)
			bits = (blk[..., 4].astype(np.uint32) | (blk[..., 5].astype(np.uint32) << 8)
					| (blk[..., 6].astype(np.uint32) << 16) | (blk[..., 7].astype(np.uint32) << 24))
			idx = np.stack([(bits >> (2 * i)) & 3 for i in range(16)], -1)
			a = np.where(((c0 <= c1)[..., None]) & (idx == 3), 0, 255).astype(np.uint8)
			alp = a.reshape(nb, nb, 4, 4).transpose(0, 2, 1, 3).reshape(nb * 4, nb * 4)
		parts.append(np.concatenate([col, alp[..., None]], -1).reshape(-1, 4).astype(np.int32))
	return np.concatenate(parts, 0)


def tile_check(offL, onL, i, on_stored=None, on_flags=None):
	"""Every G2 per-tile claim. Returns (failures, collapsed sheet list)."""
	fails = []
	eo = offL.table[i]
	en = dict(onL.table[i])
	if on_flags is not None:
		en['flags'] = on_flags
	if not (eo['flags'] & 1):
		if en['flags'] != 0:
			fails.append('absent in OFF, not in ON')
		return fails, []
	cover = bool(eo['flags'] & 2)
	mask = onL.uniformMask(en)
	if (en['flags'] & 3) != eo['flags'] or (en['flags'] >> (2 + onL.sheetCount)):
		fails.append('flags %x vs OFF %x' % (en['flags'], eo['flags']))
	offRaw = offL.b[eo['offset']:eo['offset'] + eo['stored']]
	if offL.compression:
		offRaw = zlib.decompress(offRaw)
	onSt = on_stored if on_stored is not None else onL.b[en['offset']:en['offset'] + en['stored']]
	if onL.compression:
		onSt = zlib.decompress(onSt)
	if len(onSt) != onL.rawBytesFor(en):
		fails.append('stored %d != implied %d' % (len(onSt), onL.rawBytesFor(en)))
		return fails, []
	collapsed = []
	oOff = 0
	oOn = 0
	for s in range(offL.sheetCount):
		full = sheet_full_bytes(offL, s, cover)
		offSeg = offRaw[oOff:oOff + full]
		if mask & (1 << s):
			rec = onSt[oOn:oOn + 16]
			unit = onL.unitBytes(s, cover)
			if rec[unit:] != b'\0' * (16 - unit):
				fails.append('sheet %d record pad not zero' % s)
			px = decode_sheet(offL, s, cover, offSeg)
			rng = int((px.max(0) - px.min(0)).max())
			if rng != 0:
				fails.append('sheet %d collapsed but OFF sheet spans %d levels' % (s, rng))
			# the record, repeated, decoded: every texel must be the OFF sheet's one value
			rpx = decode_sheet(offL, s, cover, rec[:unit] * (full // unit))
			if rpx.shape != px.shape or not np.array_equal(rpx, px):
				fails.append('sheet %d record decodes to %s, OFF sheet is %s' % (
					s, rpx.min(0).tolist(), px.min(0).tolist()))
			collapsed.append((s, full, px[0].tolist()))
			oOn += 16
		else:
			if onSt[oOn:oOn + full] != offSeg:
				fails.append('sheet %d not collapsed and not byte-identical' % s)
			oOn += full
		oOff += full
	exp = onL.expand(en, bytes(onSt))
	if exp != offRaw:
		fails.append('expanded ON payload != OFF payload')
	if on_stored is None and zlib.crc32(onL.b[en['offset']:en['offset'] + en['stored']]) & 0xFFFFFFFF != en['crc']:
		fails.append('tile crc32')
	return fails, collapsed


def case_a_sheets(offL, i):
	"""Independent classification of OFF's sheets: which are one value by decode."""
	eo = offL.table[i]
	cover = bool(eo['flags'] & 2)
	raw = offL.payload(i)
	out = []
	o = 0
	for s in range(offL.sheetCount):
		full = sheet_full_bytes(offL, s, cover)
		px = decode_sheet(offL, s, cover, raw[o:o + full])
		if int((px.max(0) - px.min(0)).max()) == 0:
			unit = offL.unitBytes(s, cover)
			seg = raw[o:o + full]
			same = seg == seg[:unit] * (full // unit)
			out.append((s, same))
		o += full
	return out


def g2(off, on, doctored_dir=None):
	to, tn = tree(off), tree(on)
	nonl = sorted(k for k in set(to) | set(tn) if not k.endswith('.lodt'))
	bad, prov = [], []
	for k in nonl:
		if k not in to or k not in tn:
			bad.append(k)
			continue
		ba, bb = open(to[k], 'rb').read(), open(tn[k], 'rb').read()
		if ba != bb:
			(prov if provenance_same(k, ba, bb, off, on)[0] else bad).append(k)
	verdict('G2 every non-.lodt file ON == OFF (provenance files: only their named words)', len(nonl) > 0 and not bad,
			'%d files, %d data files differ, %d provenance files differ only in their named words%s' % (
				len(nonl), len(bad), len(prov), ('; first: ' + bad[0]) if bad else ''))
	lodts = sorted(k for k in to if k.endswith('.lodt'))
	verdict('G2 the same .lodt set', lodts == sorted(k for k in tn if k.endswith('.lodt')) and len(lodts) > 0,
			'%d files' % len(lodts))
	stats = {}
	allFails = []
	sizeOff = sizeOn = 0
	missed = {}
	refuted = {}
	for k in lodts:
		offL, onL = vc.Lodv(to[k]), vc.Lodv(tn[k])
		sizeOff += len(offL.b)
		sizeOn += len(onL.b)
		ho, hn = bytearray(offL.b[:256]), bytearray(onL.b[:256])
		for h in (ho, hn):
			h[0x10:0x18] = b'\0' * 8
			h[0x98:0x9C] = b'\0' * 4
		if ho != hn:
			allFails.append('%s header differs outside fileBytes/indexCrc32' % k)
		if struct.unpack_from('<Q', onL.b, 0x10)[0] != len(onL.b):
			allFails.append('%s fileBytes != length' % k)
		hz = bytearray(onL.b[:256])
		hz[0x98:0x9C] = b'\0' * 4
		crc = zlib.crc32(onL.b[onL.tableOffset:onL.tableOffset + 24 * onL.tileCount], zlib.crc32(bytes(hz))) & 0xFFFFFFFF
		if crc != onL.indexCrc:
			allFails.append('%s indexCrc32 recomputed %08x != %08x' % (k, crc, onL.indexCrc))
		for i in range(offL.tileCount):
			f, col = tile_check(offL, onL, i)
			allFails += ['%s tile %d: %s' % (k, i, x) for x in f]
			if not (offL.table[i]['flags'] & 1):
				continue
			for (s, full, val) in col:
				key = (offL.levelDim, ROLE.get(offL.sheets[s]['role']))
				st = stats.setdefault(key, [0, 0, 0])
				st[1] += 1
				st[2] += full - 16
			for s in range(offL.sheetCount):
				stats.setdefault((offL.levelDim, ROLE.get(offL.sheets[s]['role'])), [0, 0, 0])[0] += 1
			colset = {c[0] for c in col}
			for (s, same) in case_a_sheets(offL, i):
				if s not in colset:
					why = 'blocks differ' if not same else 'SHOULD HAVE COLLAPSED'
					missed[why] = missed.get(why, 0) + 1
			# refuters, once per file on the first tile that allows each
			if 'R1' not in refuted:
				e = offL.table[i]
				cover = bool(e['flags'] & 2)
				for s in range(offL.sheetCount):
					if s in colset:
						continue
					# R1: force a non-uniform sheet to collapse
					en = onL.table[i]
					st_ = bytearray(onL.b[en['offset']:en['offset'] + en['stored']])
					mask = onL.uniformMask(en)
					at = sum(onL.sheetStoredBytes(t, cover, mask) for t in range(s))
					full = sheet_full_bytes(onL, s, cover)
					unit = onL.unitBytes(s, cover)
					rec = bytes(st_[at:at + unit]) + b'\0' * (16 - unit)
					forged = bytes(st_[:at]) + rec + bytes(st_[at + full:])
					f1, _ = tile_check(offL, onL, i, on_stored=forged, on_flags=en['flags'] | (1 << (2 + s)))
					refuted['R1'] = (len(f1) > 0, '%s tile %d sheet %d (%s): %s' % (
						k, i, s, ROLE.get(onL.sheets[s]['role']), f1[0] if f1 else 'NOT caught'))
					break
			bcCol = [c for c in col if onL.sheets[c[0]]['role'] not in (4, 7)]
			if bcCol and 'R2' not in refuted:
				# a BLOCK sheet, so the doctored file also tests the validator's block rule
				e = onL.table[i]
				cover = bool(e['flags'] & 2)
				mask = onL.uniformMask(e)
				s = bcCol[0][0]
				at = sum(onL.sheetStoredBytes(t, cover, mask) for t in range(s))
				st_ = bytearray(onL.b[e['offset']:e['offset'] + e['stored']])
				unit = onL.unitBytes(s, cover)
				r2 = bytearray(st_)
				if onL.sheets[s]['role'] in (4, 7):
					r2[at] ^= 0x01       # a different texel value
				else:
					b0 = at + unit - 8   # the BC colour half: two colours, indices 0 and 1
					r2[b0:b0 + 8] = bytes([0x00, 0xF8, 0x1F, 0x00, 0x44, 0x44, 0x44, 0x44])
				f2, _ = tile_check(offL, onL, i, on_stored=bytes(r2))
				refuted['R2'] = (len(f2) > 0, '%s tile %d sheet %d: %s' % (k, i, s, f2[0] if f2 else 'NOT caught'))
				r3 = bytearray(st_)
				r3[at + 15] = 0x5A if unit < 16 else r3[at + 15]
				if unit < 16:
					f3, _ = tile_check(offL, onL, i, on_stored=bytes(r3))
					refuted['R3'] = (len(f3) > 0, '%s tile %d sheet %d: %s' % (k, i, s, f3[0] if f3 else 'NOT caught'))
				if doctored_dir:
					write_doctored(onL, i, bytes(r2), os.path.join(doctored_dir, 'R2_block.lodt'))
					if unit < 16:
						write_doctored(onL, i, bytes(r3), os.path.join(doctored_dir, 'R3_pad.lodt'))
					open(os.path.join(doctored_dir, 'control.lodt'), 'wb').write(onL.b)
	verdict('G2 every tile: flags, identical non-collapsed sheets, collapsed = one value = its record, '
			'expanded == OFF, crc', not allFails, '%d failures%s' % (len(allFails), ('; first: ' + allFails[0]) if allFails else ''))
	for r in ('R1', 'R2', 'R3'):
		if r in refuted:
			verdict('%s refuter comes back red' % r, refuted[r][0], refuted[r][1])
		else:
			verdict('%s refuter ran' % r, False, 'no tile allowed it')
	print('case-a sheets in OFF not collapsed: %s' % (missed or 'none'))
	verdict('G2 no case-a sheet with identical blocks was left uncollapsed', 'SHOULD HAVE COLLAPSED' not in missed)
	print('%-6s %-8s %8s %9s %14s' % ('dim', 'sheet', 'sheets', 'collapsed', 'raw B saved'))
	tot = [0, 0, 0]
	for key in sorted(stats):
		st = stats[key]
		tot = [a + b for a, b in zip(tot, st)]
		print('%-6d %-8s %8d %9d %14d' % (key[0], key[1], st[0], st[1], st[2]))
	print('total  %8d sheets, %d collapsed, %d raw B saved; files %d -> %d B (%d saved, %.2f %%)' % (
		tot[0], tot[1], tot[2], sizeOff, sizeOn, sizeOff - sizeOn, 100.0 * (sizeOff - sizeOn) / max(1, sizeOff)))


def write_doctored(L, i, stored, path):
	"""A copy of L with tile i's payload replaced by `stored` (same length) and
	every CRC fixed, so a validator reaches the record rule (16c)."""
	b = bytearray(L.b)
	e = L.table[i]
	assert len(stored) == e['stored']
	b[e['offset']:e['offset'] + e['stored']] = stored
	o = L.tableOffset + i * 24
	struct.pack_into('<I', b, o + 0x10, zlib.crc32(stored) & 0xFFFFFFFF)
	hz = bytearray(b[:256])
	hz[0x98:0x9C] = b'\0' * 4
	crc = zlib.crc32(bytes(b[L.tableOffset:L.tableOffset + 24 * L.tileCount]), zlib.crc32(bytes(hz))) & 0xFFFFFFFF
	struct.pack_into('<I', b, 0x98, crc)
	open(path, 'wb').write(b)


def main():
	rung, off, on = sys.argv[1:4]
	doctored = None
	if '--make-doctored' in sys.argv:
		doctored = sys.argv[sys.argv.index('--make-doctored') + 1]
		os.makedirs(doctored, exist_ok=True)
	vc.check = lambda *a, **k: None
	g1(rung, off)
	g2(off, on, doctored)
	print('RESULT %s (%d failures)' % ('PASS' if not FAILS else 'FAIL', len(FAILS)))
	sys.exit(1 if FAILS else 0)


if __name__ == '__main__':
	main()
