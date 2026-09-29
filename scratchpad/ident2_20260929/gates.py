"""IDENT1 after-gates from an emitter group dump, using the EMITTER's own roots (the dump's root column).
usage: python gates.py <dump> <tol> <cap> [landmarks.txt  (IDENT2: pre-join its landmarks like the emitter)]
Prints: partition equal to the offline re-run (groups.py) at tol/cap; placements / eligible / groups; every
placement has a valid root; histogram; widest multi-piece group vs the cap; single pieces over the cap;
landmarks; row houses; groups crossing a dim-4 chunk line (16,384 u)."""
import sys, collections
import groups
import sweep

h, P, C = groups.load(sys.argv[1])
tol, cap = float(sys.argv[2]), float(sys.argv[3])
n = len(P)
root = [p['root'] for p in P]
bad = sum(1 for r in root if r < 0 or r >= n or root[r] != r)
print('dump', h)
print('placements %d, eligible (non-tree, drawn) %d, bad roots %d' % (n, sum(1 for p in P if p['tris'] and not p['tree']), bad))
# the offline re-run must give the same partition
PRE = groups.landmark_sets(P, sys.argv[4]) if len(sys.argv) > 4 else ()
print('offline landmark pre-join: %s' % (', '.join(str(len(v)) for v in PRE) if PRE else 'none'))
r2, gb2, refused = groups.run(P, C, tol, cap, PRE)
canon = lambda rs: sorted(tuple(sorted(v)) for v in _grp(rs).values())


def _grp(rs):
    d = collections.defaultdict(list)
    for i, r in enumerate(rs):
        d[r].append(i)
    return d


same = canon(root) == canon(r2)
e2 = collections.defaultdict(set)
for i in range(n):
    e2[r2[i]].add(root[i])
diff = sum(1 for v in e2.values() if len(v) > 1)
edge = sum(1 for d, i, j in C if abs(d - tol) < 0.0006)
print('partition == groups.py(tol %g, cap %g): %s (offline groups the emitter keeps apart %d; pairs printed at the tolerance itself %d -- the dump rounds to 0.001; offline refusals %d)' % (tol, cap, 'YES' if same else 'NO', diff, edge, refused))
# group boxes from the pieces' own boxes
gb = [[0, 0, 0, 0] for _ in range(n)]
mem = collections.defaultdict(list)
for p in P:
    mem[root[p['i']]].append(p)
for r, v in mem.items():
    gb[r] = [min(p['lo'][0] for p in v), min(p['lo'][1] for p in v), max(p['hi'][0] for p in v), max(p['hi'][1] for p in v)]
_, hist = groups.summary(P, root, gb)
print('groups %d; histogram %s' % (len(mem), ' '.join('%s:%d' % kv for kv in hist)))
multi = [(r, v) for r, v in mem.items() if len(v) > 1]
wid = lambda r: max(gb[r][2] - gb[r][0], gb[r][3] - gb[r][1])
w = max(multi, key=lambda kv: wid(kv[0]))
over = [r for r, v in multi if cap > 0 and wid(r) > cap + 0.5]
single_over = sum(1 for r, v in mem.items() if len(v) == 1 and cap > 0 and wid(r) > cap)
big = max(mem.values(), key=len)
print('widest multi-piece group %d pieces %.0f x %.0f u; multi-piece groups over the cap %d; single pieces wider than the cap %d; largest group %d pieces' % (
    len(w[1]), gb[w[0]][2] - gb[w[0]][0], gb[w[0]][3] - gb[w[0]][1], len(over), single_over, len(big)))
lm = groups.landmarks(P, root, mem, gb)
for k, d in lm.items():
    t = d['top'][0]
    print('landmark %-15s %4d pieces in %3d groups; top %d (%d landmark + %d other), %d x %d u' % (
        k, d['pieces'], d['groups'], t['n'], t['landmarkPieces'], t['otherPieces'], t['spanX'], t['spanY']))
print('row houses (pieces, groups, top n, X x Y): %d, %d, %d, %d x %d' % sweep.rowhouses(P, root, gb))
cross = [(r, v) for r, v in multi if int(gb[r][0] // 16384) != int(gb[r][2] // 16384) or int(gb[r][1] // 16384) != int(gb[r][3] // 16384)]
print('multi-piece groups crossing a 16,384 u chunk line: %d holding %d pieces' % (len(cross), sum(len(v) for r, v in cross)))
