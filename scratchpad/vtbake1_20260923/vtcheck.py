#!/usr/bin/env python
"""Lane VTBAKE1 job 2, the SECOND instrument beside `lodgen --lodt-check`:
the file-local refusal rules of docs/LODGEN_TERRAIN_VT.md s3.4 (every rule but
18, which needs the plugin and is checked against `lodgen --corpus-hash`),
re-typed from the document, plus the index (.lodm, s4) cross-checked field by
field against every container it names.

usage: python vtcheck.py rules <file.lodt>...
       python vtcheck.py index <file.VT.lodm> <Data root the containers are relative to>
Exit 1 on any failure. Every failure line starts FAIL and names the rule.
"""
import json
import os
import struct
import sys
import zlib
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vtread import Lodt

FAILS = []


def need(rule, ok, what):
	if not ok:
		FAILS.append('FAIL rule %s: %s' % (rule, what))
		print(FAILS[-1])
	return ok


def rules(path):
	sz = os.path.getsize(path)
	if not need(1, sz >= 256, 'file %d bytes' % sz):
		return
	v = Lodt(path)
	need(2, v.magic == b'LDTX', 'magic %r' % v.magic)
	need(3, v.version == 2, 'version %d' % v.version)
	need(4, v.headerBytes == 256, 'headerBytes %d' % v.headerBytes)
	need(5, v.fileBytes == sz, 'fileBytes %d != size %d' % (v.fileBytes, sz))
	e = v.edidRaw
	nul = e.find(b'\0')
	need(6, 0 < nul < 32 and all(0x20 <= c <= 0x7E for c in e[:nul]) and all(c == 0 for c in e[nul:]),
		 'edid %r' % e)
	need(7, v.north >= v.south and v.east >= v.west and v.wNorth >= v.wSouth and v.wEast >= v.wWest
		 and v.west <= v.wWest and v.south <= v.wSouth and v.east >= v.wEast and v.north >= v.wNorth,
		 'rectangles %s' % ((v.south, v.west, v.north, v.east, v.wSouth, v.wWest, v.wNorth, v.wEast),))
	d = v.levelDim
	need(8, d in (1, 2, 4, 8, 16, 32) and v.west % d == 0 and v.south % d == 0, 'levelDim %d west %d south %d' % (d, v.west, v.south))
	sx, sy = v.east - v.west + 1, v.north - v.south + 1
	need(9, sx % d == 0 and sy % d == 0 and v.tilesX == sx // d and v.tilesY == sy // d and v.tilesX >= 1 and v.tilesY >= 1,
		 'tiles %dx%d spans %dx%d' % (v.tilesX, v.tilesY, sx, sy))
	need(10, v.tileCount == v.tilesX * v.tilesY, 'tileCount %d' % v.tileCount)
	c = v.content
	need(11, c in (128, 256, 512, 1024) and v.border % 4 == 0 and v.stored == c + 2 * v.border,
		 'content %d border %d stored %d' % (c, v.border, v.stored))
	m = v.mips
	need(12, m >= 1 and (v.border >> (m - 1)) % 4 == 0 and ((v.border >> (m - 1)) << (m - 1)) == v.border
		 and (c >> (m - 1)) >= 4, 'mips %d' % m)
	roles = [v.sheets[s]['role'] for s in range(v.sheetCount)]
	ok13 = 1 <= v.sheetCount <= 6 and 3 not in roles and all(1 <= r <= 6 for r in roles) \
		and len(set(roles)) == len(roles) and all(r in roles for r in (1, 2, 5))
	pairs = 0
	for s in range(v.sheetCount):
		sd = v.sheets[s]
		if sd['role'] in (1, 2, 5, 6):
			ok13 &= sd['dxgi'] in (71, 72, 77, 78) and sd['dxgiCover'] in (71, 72, 77, 78)
		if sd['role'] == 4:
			ok13 &= sd['dxgi'] == 56 and sd['dxgiCover'] == 56
		ok13 &= sd['space'] <= 1 and sd['zero'] == (0, 0)
		if sd['dxgiCover'] != sd['dxgi']:
			pairs += 1
			ok13 &= sd['role'] in (1, 5)
	ok13 &= pairs <= 1
	for s in range(v.sheetCount, 10):
		ok13 &= v.hdr[0xA0 + s * 8:0xA8 + s * 8] == b'\0' * 8
	need(13, ok13, 'sheets %s' % [(v.sheets[s]['role'], v.sheets[s]['dxgi'], v.sheets[s]['dxgiCover']) for s in range(v.sheetCount)])
	need(14, v.compression in (0, 1), 'compression %d' % v.compression)
	need(15, v.tableOffset >= 256 and v.tableOffset % 8 == 0 and v.tableOffset + 24 * v.tileCount <= v.payloadOffset
		 and v.payloadOffset <= v.fileBytes and v.payloadOffset % 4096 == 0,
		 'table %d payload %d' % (v.tableOffset, v.payloadOffset))
	t = v.table
	present = (t['flags'] & 1) != 0
	need(16, np.all(present == (t['offset'] != 0)), 'PRESENT disagrees with offset')
	raw = np.frombuffer(v.m, np.uint8, 24 * v.tileCount, v.tableOffset).reshape(-1, 24)
	need(16, np.all(raw[~present] == 0), 'absent entry not all zero')
	tp = t[present]
	cov = (tp['flags'] & 2) != 0
	need(16, np.all((tp['flags'] & ~np.uint16(3)) == 0) and np.all(tp['reserved'] == 0), 'unknown flag / reserved')
	need(16, np.all(tp['offset'] >= v.payloadOffset) and np.all(tp['offset'] % 4096 == 0)
		 and np.all(tp['offset'] + tp['stored'] <= v.fileBytes) and np.all(tp['stored'] > 0), 'offset/stored bounds')
	want = np.where(cov, v.rawBytes(True), v.rawBytes(False))
	need(16, np.all(tp['raw'] == want), 'rawBytes != implied size')
	if v.compression == 0:
		need(16, np.all(tp['stored'] == tp['raw']), 'stored != raw uncompressed')
	need(17, present.any(), 'no tile present')
	need(19, (v.flags & 1) == 1 and (v.flags & ~3) == 0, 'flags 0x%x' % v.flags)
	h = bytearray(v.hdr)
	h[0x98:0x9C] = b'\0\0\0\0'
	crc = zlib.crc32(bytes(h))
	crc = zlib.crc32(v.m[v.tableOffset:v.tableOffset + 24 * v.tileCount], crc) & 0xFFFFFFFF
	need(20, crc == v.indexCrc, 'indexCrc32 0x%08x recomputed 0x%08x' % (v.indexCrc, crc))
	need(21, v.aniso <= 2 * (v.border >> (m - 1)), 'aniso %d' % v.aniso)
	ld = v.levelDims
	nz = [x for x in ld if x]
	prefix_ok = all(x != 0 for x in ld[:len(nz)]) and all(x == 0 for x in ld[len(nz):])
	need(22, v.levelIndex < v.levelCount and ld[v.levelIndex] == d and len(nz) == v.levelCount and prefix_ok
		 and all(nz[i] < nz[i + 1] for i in range(len(nz) - 1)), 'levelDims %s index %d count %d' % (ld, v.levelIndex, v.levelCount))
	need('3.3', v.hdr[0xF0:0x100] == b'\0' * 16 and v.reserved0 == 0, 'reserved tail not zero')
	# payload order + zero padding + per-tile CRC (s3.3, and s3.4's load-time crc32)
	bad_crc = 0
	order_ok = True
	pad_ok = True
	prev_end = v.payloadOffset
	idx = np.nonzero(present)[0]
	for i in idx:
		e = t[i]
		o, n = int(e['offset']), int(e['stored'])
		if o < prev_end:
			order_ok = False
		gap = v.m[prev_end:o]
		if gap.count(0) != len(gap):
			pad_ok = False
		prev_end = o + n
		if (zlib.crc32(v.m[o:o + n]) & 0xFFFFFFFF) != int(e['crc']):
			bad_crc += 1
	tail = v.m[prev_end:v.fileBytes]
	need('3.3', order_ok, 'payloads not in table order')
	need('3.3', pad_ok and tail.count(0) == len(tail), 'non-zero alignment pad')
	need('crc', bad_crc == 0, '%d tile CRC mismatches' % bad_crc)
	print('rules %s: dim %d tiles %d present %d cover %d file %d B, %d tile CRCs checked, bad %d, indexCrc 0x%08x' % (
		os.path.basename(path), d, v.tileCount, int(present.sum()), int(cov.sum()), sz, len(idx), bad_crc, v.indexCrc))


def index(lodm, root):
	b = open(lodm, 'rb').read()
	# LODM envelope + compact JSON (src/io/lodmfile.h); find the JSON object
	j0 = b.find(b'{')
	doc = json.loads(b[j0:b.rfind(b'}') + 1].decode('utf-8'))
	need('4', doc.get('kind') == 'terrainVT' and doc.get('lodm') == 1 and doc.get('family') == 'pbr',
		 'kind/lodm/family %s' % ((doc.get('kind'), doc.get('lodm'), doc.get('family')),))
	T = doc['terrain']
	print('index: worldspace %s extent %s partial %s rowOrder %s compression %s emissive %s' % (
		T.get('worldspace'), T.get('extent'), T.get('partial'), T.get('rowOrder'), T.get('compression'), T.get('emissive')))
	need('4', T.get('rowOrder') == 'northUp' and T.get('alignedToWorldOrigin') is True
		 and T.get('coarseLevelsAreDownsamples') is True and not T.get('partial', False), 'flags')
	mr = T.get('maskRules', {})
	need('4', mr.get('pbrm', 0) + mr.get('legacyInverted', 0) + mr.get('noneDefault', 0) == mr.get('distinctLtex', -1),
		 'maskRules sum %s' % mr)
	print('index: maskRules %s' % mr)
	print('index: cover %s' % T.get('cover'))
	levels = T['levels']
	dims = [L['dim'] for L in levels]
	for L in levels:
		p = os.path.join(root, L['container'].replace('\\', '/'))
		if not need('4', os.path.exists(p), 'container missing %s' % p):
			continue
		v = Lodt(p)
		pres = int(((v.table['flags'] & 1) != 0).sum())
		checks = [
			('index', L['index'], v.levelIndex), ('dim', L['dim'], v.levelDim),
			('tilesX', L['tilesX'], v.tilesX), ('tilesY', L['tilesY'], v.tilesY),
			('tiles', L['tiles'], v.tileCount), ('present', L['present'], pres),
			('contentTexels', L['contentTexels'], v.content),
			('worldUnitsPerTile', L['worldUnitsPerTile'], v.levelDim * T['cellUnits']),
			('unitsPerTexel', L['unitsPerTexel'], v.levelDim * T['cellUnits'] // v.content),
			('vhgtCorpusHash', int(T['vhgtCorpusHash'], 16), v.vhgt),
			('paintCorpusHash', int(T['paintCorpusHash'], 16), v.paint),
			('extent.south', T['extent']['south'], v.wSouth), ('extent.west', T['extent']['west'], v.wWest),
			('extent.north', T['extent']['north'], v.wNorth), ('extent.east', T['extent']['east'], v.wEast),
			('content', T['content'], v.content), ('border', T['border'], v.border),
			('stored', T['stored'], v.stored), ('mips', T['mips'], v.mips), ('aniso', T['aniso'], v.aniso),
			('worldspace', T['worldspace'], v.edid), ('levelCount', len(levels), v.levelCount),
			('levelDims', dims, [x for x in v.levelDims if x]),
			('sheets', [(s['dxgi'], s['dxgiWithCover']) for s in T['sheets']],
			 [(v.sheets[i]['dxgi'], v.sheets[i]['dxgiCover']) for i in range(v.sheetCount)]),
		]
		bad = [(k, a, bb) for (k, a, bb) in checks if a != bb]
		need('4', not bad, 'level %d mismatches %s' % (L['dim'], bad))
		print('index level dim %2d: %d fields vs container, %d differ; %s' % (L['dim'], len(checks), len(bad), L['container']))
	print('index sheets: %s' % [(s['role'], s['dxgi'], s['dxgiWithCover'], s['colorSpace']) for s in T['sheets']])


if __name__ == '__main__':
	cmd = sys.argv[1]
	if cmd == 'rules':
		for p in sys.argv[2:]:
			rules(p)
	elif cmd == 'index':
		index(sys.argv[2], sys.argv[3])
	print('RESULT %s (%d failures)' % ('FAIL' if FAILS else 'PASS', len(FAILS)))
	sys.exit(1 if FAILS else 0)
