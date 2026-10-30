"""SKYSHARDS2 step 2 gate: what the cell view BUILT over the Concord sky shards' footprints (a streamwalk --place dump with
WW_PLACE_VBOX, tests/skyshards/run.sh) holds no vertex where the shards are drawn.

python -I tests/skyshards/place_gate.py <place.txt> <land.pkl | -> [--red]
The shards, measured by parallax (notes/code1030/moveframes): S1 (-61515, 64794, 8594) and S3 (-65090, 59181, 8130),
from the c3 still_close5 eye. Checks:
  G1 no built vertex (W shape lines, J far-row lines, V far terrain lines) within 900 u of S1 or S3;
  G2 far terrain over the footprint box: every V vertex within 1024 u of the LAND height (when a land pickle is given;
     '-' skips it);
  G3 the dump has the vertex lines at all (> 0 W lines: the box was read).
--red: plants one W vertex at S1 (what a lifted copy would leave) -- G1 must FAIL.
"""
import math, pickle, sys, os
S1, S3 = (-61515.0, 64794.0, 8594.0), (-65090.0, 59181.0, 8130.0)
args = [a for a in sys.argv[1:] if not a.startswith('--')]
red = '--red' in sys.argv
land = None
if len(args) > 1 and args[1] != '-':
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'lodplace'))
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'spells'))
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
    import bake_census as B
    land = pickle.load(open(args[1], 'rb'))
box = None
near, nW, nJ, nV, vOff, vMax = [], 0, 0, 0, 0, 0.0
lines = open(args[0], encoding='utf-8', errors='replace')
extra = ['W planted_red 0 %.1f %.1f %.1f\n' % S1] if red else []
for line in list(extra) + list(lines):
    if line.startswith('# vbox'):
        f = line.split()
        box = [float(v) for v in f[2:6]]
        continue
    t = line[:2]
    if t not in ('W ', 'J ', 'V '):
        continue
    f = line.split()
    if t == 'W ':
        nW += 1; p = tuple(float(v) for v in f[3:6])
    elif t == 'J ':
        nJ += 1; p = tuple(float(v) for v in f[5:8])
    else:
        p = tuple(float(v) for v in f[2:5])
        if not box or not (box[0] <= p[0] <= box[2] and box[1] <= p[1] <= box[3]):
            continue
        nV += 1
        if land is not None:
            g = B.height_at(land, p[0], p[1])
            if g is not None:
                d = abs(p[2] - g); vMax = max(vMax, d); vOff += d > 1024
    for k, s in (('S1', S1), ('S3', S3)):
        d = math.dist(p, s)
        if d < 900:
            near.append((k, d, line.strip()[:120]))
print('footprint %s: W %d, J %d, far terrain V %d' % (box, nW, nJ, nV))
g1 = not near
print('G1 built vertices within 900 u of S1/S3: %d %s' % (len(near), 'PASS' if g1 else 'FAIL'))
for k, d, s in sorted(near, key=lambda r: r[1])[:10]:
    print('   %s %.0f u: %s' % (k, d, s))
g2 = land is None or vOff == 0
print('G2 far terrain off its LAND > 1024 u: %s (max %.0f) %s' % ('skipped' if land is None else vOff, vMax, 'PASS' if g2 else 'FAIL'))
g3 = nW > 0
print('G3 the box was read (W lines > 0): %s' % ('PASS' if g3 else 'FAIL'))
ok = g1 and g2 and g3
print('PLACE GATE %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
