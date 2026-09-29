"""IDENT2: which bytes of a .lodi differ between two bakes, by header word and by payload table.
usage: python lodidiff.py <a.lodi> <b.lodi>"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
import lodgen_native_decode as D
pa, pb = sys.argv[1], sys.argv[2]
A, B = open(pa, 'rb').read(), open(pb, 'rb').read()
TA, TB = D.read_lodi(pa), D.read_lodi(pb)
ha, hb = TA['header'], TB['header']
print('sizes %d -> %d' % (len(A), len(B)))
for k in sorted(set(ha) | set(hb)):
    if ha.get(k) != hb.get(k):
        print('header %-22s %s -> %s' % (k, ha.get(k), hb.get(k)))
# raw header bytes outside the decoded words
hd = [i for i in range(256) if A[i] != B[i]]
print('header byte offsets that differ: %s' % ', '.join('0x%X' % i for i in hd))
# each table's payload bytes, a vs b, by the ranges the decoder used for the index CRC plus the rest
names = ['chunks', 'cells', 'occ', 'occRanges', 'agg', 'cov', 'pao', 'vao', 'group', 'vsky', 'vhor', 'vgnd']
ra, rb = TA['indexRanges'], TB['indexRanges']
print('index ranges: %d / %d' % (len(ra), len(rb)))
for i, ((oa, sa), (ob, sb)) in enumerate(zip(ra, rb)):
    same = A[oa:oa + sa] == B[ob:ob + sb]
    print('  range %d: a @%d %d bytes, b @%d %d bytes: %s' % (i, oa, sa, ob, sb, 'SAME' if same else 'DIFFERENT'))
# instance + cold tables
for k, sz in (('offInstances', 24), ('offCold', 8)):
    oa, ob = ha[k], hb[k]; n = ha['instanceCount'] * sz
    print('  %s: %s' % (k, 'SAME' if A[oa:oa + n] == B[ob:ob + n] else 'DIFFERENT'))
ga, gb = TA['group'], TB['group']
# is b's file-wide id a pure renumbering of a's (file-wide offset) ids? (same partition)
m1, m2, ok = {}, {}, True
for x, y in zip(ga, gb):
    if m1.setdefault(x, y) != y or m2.setdefault(y, x) != x:
        ok = False; break
print('group partition: a %d ids, b %d ids; b is a renaming of a: %s' % (len(set(ga)), len(set(gb)), ok))
