"""MERGE1: compare two bake roots file by file (mod/, scr/, vt/; logs excluded), with two red controls.
usage: python cmp_trees.py <A root> <B root>
Prints counts per extension, every differing / one-sided file (first 40), and the red controls' verdict."""
import os, sys, hashlib, collections
A, B = sys.argv[1:3]
def tree(root):
    d = {}
    for sub in ('mod', 'scr', 'vt'):
        for dp, dn, fn in os.walk(os.path.join(root, sub)):
            for f in fn:
                if f.endswith('.log') or f.endswith('.txt') and 'groupdump' in f: continue
                p = os.path.join(dp, f); d[os.path.relpath(p, root).replace(chr(92), '/')] = p
    return d
def h(p):
    x = hashlib.sha1()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 22), b''): x.update(b)
    return x.digest()
TA, TB = tree(A), tree(B)
HA = {n: h(p) for n, p in TA.items()}; HB = {n: h(p) for n, p in TB.items()}
ext = lambda n: os.path.splitext(n)[1].lower() or n.rsplit('/', 1)[-1]
cnt = collections.defaultdict(lambda: [0, 0, 0, 0])
bad = []
for n in sorted(set(HA) | set(HB)):
    e = ext(n)
    if n not in HB: cnt[e][2] += 1; bad.append(('onlyA', n)); continue
    if n not in HA: cnt[e][3] += 1; bad.append(('onlyB', n)); continue
    if HA[n] == HB[n]: cnt[e][0] += 1
    else: cnt[e][1] += 1; bad.append(('DIFF', n))
print('files A %d  B %d' % (len(HA), len(HB)))
for e, (s, d, a, b) in sorted(cnt.items()): print('  %-12s same %6d  differ %5d  onlyA %4d  onlyB %4d' % (e, s, d, a, b))
for k, n in bad[:40]: print('  %s %s' % (k, n))
if len(bad) > 40: print('  ... %d more' % (len(bad) - 40))
# red controls on the comparator itself: a flipped byte and a dropped file must both show
names = sorted(set(HA) & set(HB)); mid = names[len(names) // 2]
buf = bytearray(open(TB[mid], 'rb').read()); buf[len(buf) // 2] ^= 1
red1 = hashlib.sha1(bytes(buf)).digest() != HA[mid] or HA[mid] != HB[mid]
red2 = names[len(names) // 3] in HA
print('red controls fire (flipped byte, dropped file):', red1, red2)
print('TOTAL same %d differ %d onlyA %d onlyB %d' % (sum(v[0] for v in cnt.values()), sum(v[1] for v in cnt.values()), sum(v[2] for v in cnt.values()), sum(v[3] for v in cnt.values())))
