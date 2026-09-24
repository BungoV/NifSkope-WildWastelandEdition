import sys, collections

man = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/gate_bake/obj/Commonwealth.4.-20.24.BTO.manifest.txt'
aut = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/work/gate_auth.txt'

# manifest: index base type x y z scale class height ref part
M = {}
mclass = collections.Counter()
for line in open(man, encoding='utf-8', errors='replace'):
    line = line.strip()
    if not line or line.startswith('#'):
        continue
    t = line.split()
    if len(t) < 11:
        continue
    key = (t[9].lower(), t[10])
    M[key] = (float(t[3]), float(t[4]), float(t[5]), t[1].lower(), t[7])
    mclass[t[7]] += 1

A = {}
for line in open(aut, encoding='utf-8', errors='replace'):
    line = line.strip()
    if not line or line.startswith('chunk.'):
        continue
    t = line.split()
    if len(t) < 7:
        continue
    A[(t[0].lower(), t[1])] = (float(t[3]), float(t[4]), float(t[5]), t[2].lower())

print('manifest rows %d  unique keys %d' % (sum(mclass.values()), len(M)))
print('lodi  rows %d  unique keys %d' % (len(A), len(A)))
onlyM = set(M) - set(A)
onlyA = set(A) - set(M)
print('only in manifest: %d   only in lodi: %d   shared: %d'
      % (len(onlyM), len(onlyA), len(set(M) & set(A))))
cm = collections.Counter(M[k][4] for k in onlyM)
print('classes of the manifest-only rows:', dict(cm))
print('classes of all manifest rows     :', dict(mclass))
for k in list(onlyM)[:8]:
    print('  M-only', k, M[k])
for k in list(onlyA)[:8]:
    print('  L-only', k, A[k])
worst = 0.0
wk = None
for k in set(M) & set(A):
    d = max(abs(M[k][i] - A[k][i]) for i in range(3))
    if d > worst:
        worst, wk = d, k
print('worst |dpos| over shared keys: %.4f at %s' % (worst, wk))
