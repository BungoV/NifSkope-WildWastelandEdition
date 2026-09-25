"""TOWER1 step 1: the tallest placements in the Boston window (cells -5,-10..2,-3), from the INSTALLED library's
dim-4 manifests (world x y z, height) + .lodo bases (formId -> slot meshes). Read only."""
import sys, glob, collections
import numpy as np
sys.path.insert(0, r'E:/Projects/NifskopeWWE-tower1/tests/spells')
import lodgen_native_decode as dec
D = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth'
L = dec.read_lodo(D + '/Commonwealth.lodo')
names = [L['string_at'](m['modelStringOffset']) for m in L['meshes']]
byFid = {B['formId']: B for B in L['bases']}
rows = []
for f in glob.glob(D + '/Commonwealth.4.*.BTO.manifest.txt'):
    for ln in open(f):
        p = ln.split()
        if ln.startswith('#') or len(p) != 11: continue
        x, y, z = float(p[3]), float(p[4]), float(p[5])
        cx, cy = int(np.floor(x / 4096)), int(np.floor(y / 4096))
        if not (-5 <= cx <= 2 and -10 <= cy <= -3): continue
        B = byFid.get(int(p[1], 16))
        ms = [names[B['rep%d' % k]] for k in range(4) if B and B['rep%d' % k] != 0xFFFF]
        rows.append((float(p[8]), z, x, y, p[1], p[9], float(p[6]), p[7], ms))
rows.sort(reverse=True)
print(len(rows), 'placements in window')
for r in rows[:int(sys.argv[1]) if len(sys.argv) > 1 else 30]:
    print('h %7.0f z %6.0f  (%8.0f,%8.0f) cell %d,%d base %s ref %s s %.2f %s  %s' % (r[0], r[1], r[2], r[3], np.floor(r[2]/4096), np.floor(r[3]/4096), r[4], r[5], r[6], r[7], ' | '.join(m.split(chr(92))[-1] for m in r[8])))
print('---- by top (z + height), not trees')
top = sorted((r for r in rows if r[7] != 'tree'), key=lambda r: -(r[0] + r[1]))
for r in top[:40]:
    print('top %6.0f h %5.0f (%7.0f,%7.0f,%5.0f) base %s ref %s %s  %s' % (r[0] + r[1], r[0], r[2], r[3], r[1], r[4], r[5], r[7], r[8][0].split(chr(92))[-1] if r[8] else '-'))
print('---- clusters of tall non-tree placements (top > 4500), 1024-unit linking')
tall = [r for r in rows if r[7] != 'tree' and r[0] + r[1] > 4500]
lab = list(range(len(tall)))
def fnd(i):
    while lab[i] != i: lab[i] = lab[lab[i]]; i = lab[i]
    return i
for i in range(len(tall)):
    for j in range(i):
        if abs(tall[i][2] - tall[j][2]) < 1024 and abs(tall[i][3] - tall[j][3]) < 1024:
            lab[fnd(i)] = fnd(j)
cl = collections.defaultdict(list)
for i, r in enumerate(tall): cl[fnd(i)].append(r)
import pickle
out = []
for k, v in sorted(cl.items(), key=lambda kv: -max(r[0] + r[1] for r in kv[1])):
    xs = [r[2] for r in v]; ys = [r[3] for r in v]; tp = max(r[0] + r[1] for r in v); lo = min(r[1] for r in v)
    fam = collections.Counter(r[8][0].split(chr(92))[-1][:10] if r[8] else '-' for r in v)
    print('cluster n %3d top %6.0f zmin %5.0f x %7.0f..%7.0f y %7.0f..%7.0f  %s' % (len(v), tp, lo, min(xs), max(xs), min(ys), max(ys), fam.most_common(5)))
    out.append(v)
pickle.dump(out, open('clusters.pkl', 'wb'))
