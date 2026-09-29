#!/usr/bin/env python
"""The identity join, read against the CREATION KIT's own layers.

The `.lodi` group table says which placements share one identity. Whether that
is the RIGHT set is not a question the bake can answer about itself, so this
script asks the plugin instead: every REFR carries an `XLYR` layer, and a
Creation Kit layer is the closest thing Bethesda ships to "this is one
building". It reads the layer of every placement out of `Fallout4.esm`, the
group of every placement out of the `.lodi`, and prints the two crossed.

It shares no code with the bake and never calls NifSkope.

USAGE
  python tests/spells/lodgen_identjoin_layers.py <a.lodi> [<b.lodi>...]
        [--esm PATH] [--layer NAME] [--top 8]

  Each `.lodi` is named on its own line, so a legacy bake and a proximity bake
  can be compared in one run.

WHAT IT PRINTS, per file
  groups              distinct group ids, and how many placements are alone
  layer <NAME>        that layer's placements, and how many groups they fall in
  spanning            groups that hold placements of MORE THAN ONE layer, worst
                      first -- an identity that spans two layers is an identity
                      that may be welding two buildings

EXIT 0 always: this is a measurement, and the gate that calls it owns the
floors.
"""
import os
import struct
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))

from lod_emission_probe import walk, subrecords, recordData  # noqa: E402
import lodgen_native_decode as D  # noqa: E402


def argval(name, default=None):
	if name in sys.argv:
		return sys.argv[sys.argv.index(name) + 1]
	return default


def main():
	opts = ('--esm', '--layer', '--top')
	skip = set()
	for n in opts:
		if n in sys.argv:
			skip.add(sys.argv[sys.argv.index(n) + 1])
	files = [a for a in sys.argv[1:] if not a.startswith('--') and a not in skip]
	if not files:
		sys.stderr.write(__doc__)
		return 2
	esm = argval('--esm', 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm')
	want = argval('--layer', 'DN135_GwinnettExt')
	top = int(argval('--top', '8'))
	if not os.path.isfile(esm):
		print('SKIP: missing %s' % esm)
		return 2

	buf = open(esm, 'rb').read()
	recs = list(walk(buf))

	def subs(r):
		return list(subrecords(recordData(buf, r[3], r[4], r[2])))

	edid = {}
	for r in recs:
		for st, p in subs(r):
			if st == b'EDID':
				edid[r[1]] = p.split(b'\0')[0].decode('latin-1')
			break
	layerOf = {}
	for r in recs:
		if r[0] != b'REFR':
			continue
		for st, p in subs(r):
			if st == b'XLYR' and len(p) >= 4:
				layerOf[r[1]] = struct.unpack_from('<I', p, 0)[0]
				break
	print('plugin: %d records, %d REFRs carry a layer' % (len(recs), len(layerOf)))

	for path in files:
		if not os.path.isfile(path):
			print('SKIP: missing %s' % path)
			continue
		T = D.read_lodi(path)
		h = T['header']
		grp = T.get('group') or []
		print('')
		print('== %s (version %d)' % (path, h['version']))
		if not grp:
			print('  no group table (version %d); nothing to cross' % h['version'])
			continue
		# THE IDS ARE FILE-WIDE AS THE DECODER HANDS THEM (IDENT2, 2026-09-29).
		# A v13 file stores one u32 id a placement, dense over the whole file; a
		# v7..v12 file stores a u16 dense per CHUNK, which the decoder offsets by
		# the earlier chunks' counts (lodgen_native_decode.py). Either way the id
		# alone is the identity, and a group crossing a chunk line (v13) is ONE.
		key = list(grp)
		members = Counter(key)
		print('  placements %d, groups %d, singletons %d, largest %d'
			  % (len(grp), len(members), sum(1 for v in members.values() if v == 1),
				 max(members.values())))
		if len(members) != h['groupCount']:
			print('  NOTE: the header says groupCount %d and the table holds %d '
				  'id(s) -- one of the two is wrong'
				  % (h['groupCount'], len(members)))

		names = []
		for c in T['cold']:
			lf = layerOf.get(c['refFormId'], 0)
			names.append(edid.get(lf, '(no layer)') if lf else '(no layer)')

		sel = [i for i, n in enumerate(names) if n == want]
		if sel:
			g = set(key[i] for i in sel)
			print('  layer %-28s %4d placements in %d group(s)' % (want, len(sel), len(g)))
		else:
			print('  layer %-28s not in this file' % want)

		byGroup = defaultdict(set)
		for i, k in enumerate(key):
			byGroup[k].add(names[i])
		spanning = [(len(v), k, sorted(v)) for k, v in byGroup.items() if len(v) > 1]
		spanning.sort(key=lambda t: (t[0], t[1]), reverse=True)
		print('  groups spanning more than one layer: %d of %d'
			  % (len(spanning), len(members)))
		for n, k, v in spanning[:top]:
			print('    group %-6d %2d layers, %4d placements: %s'
				  % (k, n, members[k], ', '.join(x[:26] for x in v[:6])))
	return 0


if __name__ == '__main__':
	sys.exit(main())
