#!/usr/bin/env python
"""The workshop-scrappable rule, read out of Fallout4.esm and out of the .lodi,
by two routes that share no code with the bake.

Lane HORIZON3 measured the rule (`scratchpad/horizon3_20260919/scrap_rule.py`);
lane HORIZONOUT kept the bit when the rest of that lane's baked-horizon route
was dropped, and this is the gate's copy of the measurement. It does NOT call
NifSkope: clause 1 and clause 3 are read from the plugin's COBJ/FLST/KWDA
records, clause 2 from the XPRM box primitives, and the ANSWER the bake wrote is
read back from bit 6 of the instance flags word in the `.lodi`. The two must
agree placement for placement, not only in total.

  three clauses, each read out of the plugin and none from memory:

  1. the placement's base form is the CNAM of a COBJ whose FNAM category array
     contains KYWD 00106D8F WorkshopRecipeFilterScrap (FormList targets are
     expanded transitively); AND
  2. the placement's REFR position lies inside at least one XPRM Box primitive
     carried by a REFR that links, by KYWD 000B91E6 WorkshopLinkedPrimitive, to
     a workshop workbench REFR -- that box IS the settlement build area, and it
     is rotated by its own REFR DATA rotation; AND
  3. the placement's base does NOT carry KYWD 001CC46A UnscrappableObject.

USAGE
  python tests/spells/lodgen_scrappable_rule.py <native-prefix> [--esm PATH]
        [--expect N] [--examples 5] [--quiet]

  <native-prefix> is the path without the extension, e.g.
  .../FO4CSLOD/Commonwealth/Commonwealth -- both `.lodi` and `.lodo` sit there.

EXIT
  0  the plugin rule and the file's bits agree (and match --expect if given)
  1  they disagree, and every disagreement is named
  2  an input is missing
"""
import math
import os
import struct
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from lod_emission_probe import walk, subrecords, recordData  # noqa: E402
import lodgen_native_decode as D  # noqa: E402

KW_SCRAPFILTER = 0x00106D8F
KW_UNSCRAPPABLE = 0x001CC46A
KW_LINKEDPRIM = 0x000B91E6

LODI_INST_SCRAPPABLE = 64       # bit 6, src/lodifile.h


def argval(name, default=None):
	if name in sys.argv:
		return sys.argv[sys.argv.index(name) + 1]
	return default


def main():
	args = [a for a in sys.argv[1:] if not a.startswith('--')]
	skip = set()
	for n in ('--esm', '--expect', '--examples'):
		if n in sys.argv:
			skip.add(sys.argv[sys.argv.index(n) + 1])
	args = [a for a in args if a not in skip]
	if not args:
		sys.stderr.write(__doc__)
		return 2
	nat = args[0]
	esm = argval('--esm', 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm')
	expect = argval('--expect')
	nex = int(argval('--examples', '5'))
	quiet = '--quiet' in sys.argv

	for p in (esm, nat + '.lodi'):
		if not os.path.isfile(p):
			print('SKIP: missing %s' % p)
			return 2

	buf = open(esm, 'rb').read()
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
	if not quiet:
		print('plugin records %d, EDIDs %d' % (len(recs), len(edid)))

	# ---- clause 1
	scrap, recipe = set(), {}
	for r in recs:
		if r[0] != b'COBJ':
			continue
		cnam, fnam, ed = None, (), ''
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
	cobjTargets = len(scrap)

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
	scrap = expanded

	# ---- clause 3
	unscrap = set()
	for r in recs:
		for st, p in subs(r):
			if st == b'KWDA' and len(p) >= 4:
				if KW_UNSCRAPPABLE in struct.unpack_from('<%dI' % (len(p) // 4), p, 0):
					unscrap.add(r[1])

	# ---- clause 2
	wbase = set(f for f, e in edid.items()
				if 'workshop' in e.lower() and 'workbench' in e.lower())
	wrefs, refbase, prims = set(), {}, []
	for r in recs:
		if r[0] != b'REFR':
			continue
		base = pos = prm = None
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
		if base in wbase:
			wrefs.add(r[1])
		if prm is not None:
			prims.append((r[1], pos, prm, links))
	boxes = []
	for (form, pos, (half, ptype), links) in prims:
		if ptype != 1 or pos is None:
			continue
		if not any(k == KW_LINKEDPRIM and t in wrefs for k, t in links):
			continue
		boxes.append((pos[0], pos[1], pos[2], half[0], half[1], half[2],
					  pos[3], pos[4], pos[5], form))
	if not quiet:
		print('clause 1: %d COBJ scrap targets -> %d bases after %d FormLists'
			  % (cobjTargets, len(scrap), len(flst)))
		print('clause 3: %d bases carry UnscrappableObject, %d of them in clause 1'
			  % (len(unscrap), len(unscrap & scrap)))
		print('clause 2: %d workshop REFRs, %d build areas' % (len(wrefs), len(boxes)))

	def inside(x, y, z):
		for (cx, cy, cz, hx, hy, hz, rx, ry, rz, form) in boxes:
			dx, dy, dz = x - cx, y - cy, z - cz
			cr, sr = math.cos(-rz), math.sin(-rz)
			ux = dx * cr - dy * sr
			uy = dx * sr + dy * cr
			if abs(ux) <= hx and abs(uy) <= hy and abs(dz) <= hz:
				return form
		return 0

	# ---- the region, and the answer the bake wrote
	T = D.read_lodi(nat + '.lodi')
	inst, cold = T['instances'], T['cold']
	print('region: %d placements, %s' % (len(inst), nat + '.lodi'))

	rule, fileBit, yes, no = set(), set(), [], []
	c1 = c2 = 0
	for i, (r, c) in enumerate(zip(inst, cold)):
		ref = c['refFormId']
		base = refbase.get(ref)
		ok1 = base in scrap and base not in unscrap
		w = inside(r['x'], r['y'], r['z'])
		if ok1:
			c1 += 1
		if w:
			c2 += 1
		if ok1 and w:
			rule.add(i)
			yes.append((i, ref, base, r['x'], r['y'], r['z'], w))
		else:
			no.append((i, ref, base, r['x'], r['y'], r['z'], w))
		if int(r['flags']) & LODI_INST_SCRAPPABLE:
			fileBit.add(i)

	print('  clause 1 alone (base has a scrap recipe)      : %d of %d' % (c1, len(inst)))
	print('  clause 2 alone (inside a workshop build area) : %d of %d' % (c2, len(inst)))
	print('  RULE  (the plugin, this script)               : %d' % len(rule))
	print('  FILE  (instance flags bit 6, the bake)        : %d' % len(fileBit))

	print('--- %d that ARE scrappable ---' % min(nex, len(yes)))
	for (i, ref, base, x, y, z, w) in yes[:nex]:
		print('  #%-6d REFR %08X base %08X %-30s at %.0f %.0f %.0f  area %08X  %s'
			  % (i, ref, base or 0, edid.get(base, '?')[:30], x, y, z, w,
				 recipe.get(base, (0, ''))[1]))
	def clause(base):
		return ('no scrap recipe for the base' if base not in scrap
				else 'base is UnscrappableObject' if base in unscrap
				else 'outside every workshop build area')

	# EVERY clause that the region actually refuses on gets named first, and only
	# then is the list filled up to `nex`. A fixed quota per clause silently
	# printed FOUR examples when a region happened to have no placement refused
	# by the third clause, and a gate asking for five then failed on the gate's
	# own arithmetic rather than on anything about the bake.
	first, rest, seen = [], [], set()
	for row in no:
		why = clause(row[2])
		if why not in seen:
			seen.add(why)
			first.append((row, why))
		else:
			rest.append((row, why))
	picked = first + rest[:max(0, nex - len(first))]
	print('--- %d that are NOT, with the clause that failed (%d clause(s) refuse here) ---'
		  % (len(picked), len(seen)))
	for ((i, ref, base, x, y, z, w), why) in picked:
		print('  #%-6d REFR %08X base %08X %-30s at %.0f %.0f %.0f  -- %s'
			  % (i, ref, base or 0, edid.get(base, '?')[:30], x, y, z, why))

	bad = 0
	for i in sorted(rule ^ fileBit):
		bad += 1
		print('  DISAGREE instance %d: rule says %s, file says %s'
			  % (i, i in rule, i in fileBit))
		if bad >= 20:
			print('  ... and more')
			break
	if rule != fileBit:
		print('FAIL: the plugin rule and the file disagree on %d placement(s)'
			  % len(rule ^ fileBit))
		return 1
	print('OK: the plugin rule and the file agree on every one of %d placements'
		  % len(inst))
	if expect is not None and len(rule) != int(expect):
		print('FAIL: expected %s scrappable, measured %d' % (expect, len(rule)))
		return 1
	if expect is not None:
		print('OK: the count is the expected %s' % expect)
	return 0


if __name__ == '__main__':
	sys.exit(main())
