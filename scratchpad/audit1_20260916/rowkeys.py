# AUDIT1: dump a .lodb's recorded output rows, canonicalised the way
# lodgen_btofree_ledger.canon() does, so two records can be compared as SETS.
#   usage: python rowkeys.py <a.lodb> [<b.lodb>]
import os
import sys
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodb_read
import lodgen_btofree_ledger as L


def keys(p):
    d = lodb_read.read(p)
    out = []
    for c in d.get('chunks', []):
        for o in c.get('out', []):
            out.append(L.canon(o).split(' ')[0])
    return out


a = keys(sys.argv[1])
print('%s: %d row(s)' % (os.path.basename(sys.argv[1]), len(a)))
if len(sys.argv) < 3:
    for k in sorted(set(a)):
        print('   %s' % k)
    raise SystemExit(0)
b = keys(sys.argv[2])
print('%s: %d row(s)' % (os.path.basename(sys.argv[2]), len(b)))
sa, sb = set(a), set(b)
for k in sorted(sa - sb):
    print('  only in A: %s' % k)
for k in sorted(sb - sa):
    print('  only in B: %s' % k)
print('  shared: %d' % len(sa & sb))
