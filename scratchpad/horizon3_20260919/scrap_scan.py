#!/usr/bin/env python
"""HORIZON3 step 4 -- what makes a REFR workshop-scrappable, read out of the
plugin rather than remembered.

Nothing here is asserted from memory: every keyword, every record type and
every subrecord is printed with its own EDID and formID so the rule in the
report can be checked against this log.

Stage 1 (--survey): the COBJ population by workbench keyword (BNAM), the KYWD
records whose EDID names a workshop or a scrap, and the workshop workbench
bases and how their REFRs carry a build area.
"""
import argparse
import os
import struct
import sys
from collections import Counter, defaultdict

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tools')
from lod_emission_probe import walk, subrecords, recordData  # noqa: E402

ESM = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'


def load(path):
	buf = open(path, 'rb').read()
	recs = []
	for typ, form, flags, doff, dsize, stack in walk(buf):
		recs.append((typ, form, flags, doff, dsize, stack))
	return buf, recs


def edid_of(buf, r):
	typ, form, flags, doff, dsize, stack = r
	for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
		if st == b'EDID':
			return payload.split(b'\0')[0].decode('latin-1')
	return ''


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('--esm', default=ESM)
	ap.add_argument('--survey', action='store_true')
	a = ap.parse_args()

	buf, recs = load(a.esm)
	print('plugin %s  %d bytes  %d records' % (os.path.basename(a.esm), len(buf), len(recs)))

	by_type = Counter(r[0] for r in recs)
	for t in (b'COBJ', b'KYWD', b'FLST', b'LCTN', b'FURN', b'MISC', b'STAT', b'SCOL'):
		print('  %s %d' % (t.decode(), by_type.get(t, 0)))

	# ---- every EDID, for naming
	names = {}
	for r in recs:
		if r[0] in (b'KYWD', b'COBJ', b'FLST', b'LCTN', b'FURN', b'CMPO', b'MISC',
					b'STAT', b'ACTI', b'SCOL', b'MSTT', b'CONT', b'FLOR',
					b'TREE', b'DOOR', b'LIGH', b'ALCH', b'WEAP', b'ARMO',
					b'NPC_', b'TERM', b'BOOK', b'AMMO', b'KEYM'):
			e = edid_of(buf, r)
			if e:
				names[r[1]] = (r[0].decode(), e)

	# ---- KYWD whose name says workshop / scrap / recipe filter
	print('--- KYWD records naming a workshop or a scrap ---')
	kw = []
	for r in recs:
		if r[0] != b'KYWD':
			continue
		e = edid_of(buf, r)
		el = e.lower()
		if 'workshop' in el or 'scrap' in el or 'recipefilter' in el:
			kw.append((r[1], e))
	for form, e in sorted(kw, key=lambda x: x[1]):
		print('  %08X  %s' % (form, e))
	print('  (%d)' % len(kw))

	# ---- COBJ: workbench keyword distribution
	print('--- COBJ by BNAM (workbench keyword) ---')
	cobj = []
	for r in recs:
		if r[0] != b'COBJ':
			continue
		ed = cnam = bnam = None
		fnam = []
		for st, payload in subrecords(recordData(buf, r[3], r[4], r[2])):
			if st == b'EDID':
				ed = payload.split(b'\0')[0].decode('latin-1')
			elif st == b'CNAM' and len(payload) >= 4:
				cnam = struct.unpack_from('<I', payload, 0)[0]
			elif st == b'BNAM' and len(payload) >= 4:
				bnam = struct.unpack_from('<I', payload, 0)[0]
			elif st == b'FNAM':
				fnam = list(struct.unpack_from('<%dI' % (len(payload) // 4), payload, 0))
		cobj.append((r[1], ed, cnam, bnam, tuple(fnam)))
	c = Counter(x[3] for x in cobj)
	for form, n in c.most_common(20):
		print('  BNAM %s  %-44s %6d' % ('%08X' % form if form else '(none)  ',
									   names.get(form, ('', '(unnamed)'))[1], n))
	print('--- COBJ by FNAM category keyword ---')
	cf = Counter()
	for x in cobj:
		for k in x[4]:
			cf[k] += 1
	for form, n in cf.most_common(25):
		print('  FNAM %08X  %-44s %6d' % (form, names.get(form, ('', '(unnamed)'))[1], n))

	# ---- five COBJ whose EDID says scrap, printed whole
	print('--- COBJ whose EDID names a scrap (first 8) ---')
	shown = 0
	for (form, ed, cnam, bnam, fnam) in cobj:
		if not ed or 'scrap' not in ed.lower():
			continue
		print('  %08X %-40s CNAM %s %-34s BNAM %s %s' % (
			form, ed, '%08X' % cnam if cnam else '--------',
			names.get(cnam, ('', '?'))[1][:34],
			'%08X' % bnam if bnam else '--------',
			names.get(bnam, ('', '?'))[1]))
		shown += 1
		if shown >= 8:
			break
	print('  scrap-named COBJ total: %d'
		  % sum(1 for x in cobj if x[1] and 'scrap' in x[1].lower()))

	# ---- the workshop workbench base, and what a workshop REFR carries
	print('--- bases whose EDID names a workshop workbench ---')
	wbases = [(f, n[1]) for f, n in names.items()
			  if 'workshop' in n[1].lower() and 'workbench' in n[1].lower()]
	for f, e in sorted(wbases, key=lambda x: x[1])[:15]:
		print('  %08X %s (%s)' % (f, e, names[f][0]))
	wset = set(f for f, _ in wbases)

	print('--- the subrecords a workshop REFR actually carries ---')
	sub = Counter()
	nref = 0
	first = []
	for r in recs:
		if r[0] != b'REFR':
			continue
		base = None
		sts = []
		for st, payload in subrecords(recordData(buf, r[3], r[4], r[2])):
			sts.append((st, len(payload), payload))
			if st == b'NAME' and len(payload) >= 4:
				base = struct.unpack_from('<I', payload, 0)[0]
		if base not in wset:
			continue
		nref += 1
		for st, n, _ in sts:
			sub[st.decode()] += 1
		if len(first) < 3:
			first.append((r[1], base, sts))
	print('  workshop REFRs: %d' % nref)
	for st, n in sub.most_common(30):
		print('    %-6s %5d' % (st, n))
	for (form, base, sts) in first:
		print('  REFR %08X base %08X %s' % (form, base, names.get(base, ('', '?'))[1]))
		for st, n, payload in sts:
			extra = ''
			if st in (b'XLRL', b'XLRT', b'XLKR', b'NAME') and n >= 4:
				ids = struct.unpack_from('<%dI' % (n // 4), payload, 0)
				extra = ' -> ' + ', '.join('%08X %s' % (i, names.get(i, ('', '?'))[1])
										   for i in ids[:4])
			if st == b'XPRM' and n >= 12:
				extra = ' bounds %.0f %.0f %.0f' % struct.unpack_from('<3f', payload, 0)
			print('     %-6s %4d%s' % (st.decode(), n, extra))
	return 0


if __name__ == '__main__':
	sys.exit(main())
