"""AUDIT1: every material row of a .lodo with its string, alpha threshold and
TREE flag -- so the audit can ask WHICH texture our LOD trees actually name."""
import sys, os
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from lodgen_native_decode import read_lodo
L = read_lodo(sys.argv[1])
sa = L['string_at']
rows = []
for i, m in enumerate(L['materials']):
    s = sa(m['lodmStringOffset'])
    rows.append((s, m['alphaThreshold'], m['flags'], m['family'], m['arrayClass'], m['layer']))
print('%d material row(s)' % len(rows))
tree = [r for r in rows if r[2] & 2]
print('%d with the TREE flag, %d alpha-tested' % (len(tree), sum(1 for r in rows if r[1])))
for r in sorted(set(tree)):
    print('  TREE  a=%-3d flags=%d fam=%d cls=%d layer=%d  %s' % (r[1], r[2], r[3], r[4], r[5], r[0]))
