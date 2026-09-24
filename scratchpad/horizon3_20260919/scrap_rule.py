#!/usr/bin/env python
"""HORIZON3 step 4 -- the workshop-scrappable rule, measured on the region.

The rule, in three clauses, each one read out of Fallout4.esm and not from
memory:

  1. the placement's base form is the CNAM (created object) of a COBJ whose FNAM
     category array contains KYWD 00106D8F WorkshopRecipeFilterScrap; AND
  2. the placement's REFR position lies inside at least one XPRM Box primitive
     carried by a REFR that links to a workshop workbench REFR with keyword
     KYWD 000B91E6 WorkshopLinkedPrimitive (that box IS the settlement build
     area; the box is rotated by its own REFR DATA rotation); AND
  3. the placement's base does NOT carry KYWD 001CC46A UnscrappableObject.

Clause 3 is a no-op on Fallout4.esm (the overlap with clause 1 is empty, see
scrap_rule_stage2.log) and is in the rule so a DLC or a mod that sets it is
honoured.
"""
import math
import struct
import sys
from collections import Counter

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, ROOT + '/tools')
sys.path.insert(0, ROOT + '/tests/spells')
from lod_emission_probe import walk, subrecords, recordData  # noqa: E402
import lodgen_native_decode as D  # noqa: E402

ESM = 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
NAT = (ROOT + '/scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/'
	   'Commonwealth/Commonwealth')

KW_SCRAPFILTER = 0x00106D8F
KW_UNSCRAPPABLE = 0x001CC46A
KW_LINKEDPRIM = 0x000B91E6


def main():
	buf = open(ESM, 'rb').read()
	recs = list(walk(buf))

	def subs(r):
		return list(subrecords(recordData(buf, r[3], r[4], r[2])))

	edid, typ = {}, {}
	for r in recs:
		typ[r[1]] = r[0].decode()
		for st, p in subs(r):
			if st == b'EDID':
				edid[r[1]] = p.split(b'\0')[0].decode('latin-1')
			break
	print('plugin records %d, EDIDs %d' % (len(recs), len(edid)))

	# ---- clause 1
	scrap, recipe = set(), {}
	for r in recs:
		if r[0] != b'COBJ':
			continue
		cnam = None
		fnam = ()
		ed = ''
		for st, p in subs(r):
			if st == b'EDID':
				ed = p.split(b'\0')[0].decode('latin-1')
			elif st == b'CNAM' and len(p) >= 4:
				cnam = struct.unpack_from('<I', p, 0)[0]
			elif st == b'FNAM':
				fnam = struct.unpack_from('<%dI' % (len(p) // 4), p, 0)
		if KW_SCRAPFILTER in fnam and cnam:
			scrap.add(cnam)
			recipe[cnam] = (r[1], ed)
	print('clause 1: %d distinct CNAM targets on scrap COBJ records' % len(scrap))
	print('          by record type: %s'
		  % dict(Counter(typ.get(f, '?') for f in scrap)))

	# 87 of those CNAM targets are FormLists, not bases: a scrap recipe may name
	# a whole family at once. Expand FLST members (LNAM) transitively.
	flst = {}
	for r in recs:
		if r[0] != b'FLST':
			continue
		mem = []
		for st, p in subs(r):
			if st == b'LNAM' and len(p) >= 4:
				mem.append(struct.unpack_from('<I', p, 0)[0])
		flst[r[1]] = mem
	expanded, seen, stack = set(), set(), list(scrap)
	while stack:
		f = stack.pop()
		if f in seen:
			continue
		seen.add(f)
		if f in flst:
			for m in flst[f]:
				stack.append(m)
				if m not in recipe:
					recipe[m] = recipe.get(f, (0, ''))
		else:
			expanded.add(f)
	print('clause 1: %d FLST records in the plugin; after transitive expansion '
		  '%d distinct BASE forms are scrappable'
		  % (len(flst), len(expanded)))
	print('          by record type: %s'
		  % dict(Counter(typ.get(f, '?') for f in expanded)))
	scrap = expanded

	# ---- clause 3
	unscrap = set()
	for r in recs:
		for st, p in subs(r):
			if st == b'KWDA' and len(p) >= 4:
				if KW_UNSCRAPPABLE in struct.unpack_from('<%dI' % (len(p) // 4), p, 0):
					unscrap.add(r[1])
	print('clause 3: %d bases carry UnscrappableObject; %d of them are in clause 1'
		  % (len(unscrap), len(unscrap & scrap)))

	# ---- clause 2: the build boxes
	wbase = set(f for f, e in edid.items()
				if 'workshop' in e.lower() and 'workbench' in e.lower())
	wrefs = set()
	refbase, refpos = {}, {}
	for r in recs:
		if r[0] != b'REFR':
			continue
		base = pos = None
		prm = None
		links = []
		for st, p in subs(r):
			if st == b'NAME' and len(p) >= 4:
				base = struct.unpack_from('<I', p, 0)[0]
			elif st == b'DATA' and len(p) >= 24:
				pos = struct.unpack_from('<6f', p, 0)
			elif st == b'XPRM' and len(p) >= 32:
				prm = (struct.unpack_from('<3f', p, 0),
					   struct.unpack_from('<I', p, 28)[0])
			elif st == b'XLKR' and len(p) >= 8:
				links.append(struct.unpack_from('<2I', p, 0))
		if base is not None:
			refbase[r[1]] = base
		if pos is not None:
			refpos[r[1]] = pos
		if base in wbase:
			wrefs.add(r[1])
		r_extra = (prm, links)
		if prm is not None:
			PRIMS.append((r[1], pos, prm, links))
	boxes = []
	for (form, pos, (half, ptype), links) in PRIMS:
		if ptype != 1 or pos is None:
			continue
		if not any(k == KW_LINKEDPRIM and t in wrefs for k, t in links):
			continue
		boxes.append((pos[0], pos[1], pos[2], half[0], half[1], half[2],
					  pos[3], pos[4], pos[5], form))
	print('clause 2: %d workshop REFRs, %d WorkshopLinkedPrimitive Box build areas'
		  % (len(wrefs), len(boxes)))

	def inside(x, y, z):
		for (cx, cy, cz, hx, hy, hz, rx, ry, rz, form) in boxes:
			dx, dy, dz = x - cx, y - cy, z - cz
			# the CK writes the primitive in the ref's own rotated frame
			cr, sr = math.cos(-rz), math.sin(-rz)
			ux = dx * cr - dy * sr
			uy = dx * sr + dy * cr
			if abs(ux) <= hx and abs(uy) <= hy and abs(dz) <= hz:
				return form
		return 0

	# ---- the region
	T = D.read_lodi(NAT + '.lodi')
	inst, cold = T['instances'], T['cold']
	print('region: %d placements (%s)' % (len(inst), NAT + '.lodi'))
	if inst:
		print('instance keys: %s' % sorted(inst[0].keys()))

	yes, no = [], []
	c1 = c2 = 0
	for r, c in zip(inst, cold):
		ref = c['refFormId']
		base = refbase.get(ref)
		ok1 = base in scrap and base not in unscrap
		x, y, z = r['x'], r['y'], r['z']
		w = inside(x, y, z)
		if ok1:
			c1 += 1
		if w:
			c2 += 1
		(yes if (ok1 and w) else no).append((ref, base, x, y, z, w))
	print('--- the count on the region ---')
	print('  clause 1 alone (base has a scrap recipe)      : %d of %d' % (c1, len(inst)))
	print('  clause 2 alone (inside a workshop build area) : %d of %d' % (c2, len(inst)))
	print('  RULE (1 and 2 and 3) scrappablePlacements     : %d of %d (%.2f %%)'
		  % (len(yes), len(inst), 100.0 * len(yes) / max(1, len(inst))))
	print('--- five that ARE scrappable ---')
	for (ref, base, x, y, z, w) in yes[:5]:
		print('  REFR %08X base %08X %-34s at %.0f %.0f %.0f  build area REFR %08X, recipe COBJ %08X %s'
			  % (ref, base or 0, edid.get(base, '?')[:34], x, y, z, w,
				 recipe.get(base, (0, ''))[0], recipe.get(base, (0, ''))[1]))
	print('--- that are NOT, five per failing clause ---')
	shown = Counter()
	for (ref, base, x, y, z, w) in no:
		why = ('no scrap recipe for the base' if base not in scrap
			   else 'base is UnscrappableObject' if base in unscrap
			   else 'outside every workshop build area')
		if shown[why] >= 5:
			continue
		shown[why] += 1
		print('  REFR %08X base %08X %-34s at %.0f %.0f %.0f  -- %s'
			  % (ref, base or 0, edid.get(base, '?')[:34], x, y, z, why))
	print('--- the five build areas that touch this region at all ---')
	seen = Counter()
	for (ref, base, x, y, z, w) in yes + no:
		if w:
			seen[w] += 1
	for f, n in seen.most_common():
		print('  build area REFR %08X: %d placements of the region inside it' % (f, n))
	return 0


PRIMS = []

if __name__ == '__main__':
	sys.exit(main())
