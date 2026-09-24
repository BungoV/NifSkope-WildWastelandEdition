import sys
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from lodgen_native_decode import read_lodo
L = read_lodo(sys.argv[1]); sa = L['string_at']
rows = set()
for m in L['materials']:
    rows.add((sa(m['lodmStringOffset']), m['alphaThreshold'], m['flags']))
at = sorted(r for r in rows if r[1])
print('%d distinct material strings, %d distinct alpha-tested' % (len(rows), len(at)))
import re
for s, a, f in at:
    if re.search(r'tree|maple|elm|forest|branch|leaf|shrub|bush|foliage|hedge|grass|ivy|vine', s, re.I):
        print('  a=%-3d f=%d  %s' % (a, f, s))
print('--- flags histogram:', sorted(set(r[2] for r in rows)))
