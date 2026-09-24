"""Where does the LARGEST group stand, and who else stands in the same cells?

A picture of "the largest group on its own" has to be made with a knob that
already ships, never with a doctored file: `WW_LODI_REGION="x0,y0,x1,y1"` in
CELLS (src/lodinative.cpp, lodiSpecFromEnv). So this answers one factual
question -- which cells the group occupies, and how many placements that are
NOT in it fall inside the same rectangle, because that number is what the
caption has to admit.

usage: biggest_group_cells.py <Commonwealth.lodo> <Commonwealth.lodi>
"""
import os
import sys
import collections

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
import lodgen_native_decode as D  # noqa: E402

BS = chr(92)
L = D.read_lodo(sys.argv[1])
T = D.read_lodi(sys.argv[2])
h = T['header']
S = L['string_at']
inst = T['instances']
cold = T['cold']
grp = T['group']
n = h['instanceCount']
base_model = [S(b['modelStringOffset']) for b in L['bases']]

pos = [None] * n
chunk_of = [0] * n
w = h['chunkEast'] - h['chunkWest'] + 1
for ci, c in enumerate(T['chunks']):
    if c['instanceCount'] == 0:
        continue
    cx = h['chunkWest'] + (ci % w)
    cy = h['chunkNorth'] - (ci // w)
    for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
        r = inst[i]
        chunk_of[i] = ci
        pos[i] = (cx * 16384.0 + r['px'] / 65535.0 * 16384.0,
                  cy * 16384.0 + r['py'] / 65535.0 * 16384.0)

groups = collections.defaultdict(list)
for i in range(n):
    groups[(chunk_of[i], grp[i])].append(i)

key, mem = max(groups.items(), key=lambda kv: len(kv[1]))
xs = [pos[i][0] for i in mem]
ys = [pos[i][1] for i in mem]
refs = {cold[i]['refFormId'] for i in mem}
models = {base_model[inst[i]['baseId']] for i in mem}
CELL = 4096.0
cx0, cx1 = int(min(xs) // CELL), int(max(xs) // CELL)
cy0, cy1 = int(min(ys) // CELL), int(max(ys) // CELL)

others = [i for i in range(n)
          if i not in set(mem)
          and cx0 <= pos[i][0] // CELL <= cx1
          and cy0 <= pos[i][1] // CELL <= cy1]

print('largest group  (chunk %d, id %d)  %d placements' % (key[0], key[1], len(mem)))
print('  refFormIds   %s' % ', '.join('0x%08X' % r for r in sorted(refs)))
print('  models       %s' % ', '.join(sorted(m.replace('/', BS).split(BS)[-1] for m in models)))
print('  world box    x %.1f..%.1f   y %.1f..%.1f  (%.0f x %.0f units)'
      % (min(xs), max(xs), min(ys), max(ys), max(xs) - min(xs), max(ys) - min(ys)))
print('  centre       %.1f, %.1f' % ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2))
print('  CELLS        x %d..%d  y %d..%d   ->  WW_LODI_REGION="%d,%d,%d,%d"'
      % (cx0, cx1, cy0, cy1, cx0, cy0, cx1, cy1))
print('  placements in those cells that are NOT in the group: %d' % len(others))
byg = collections.Counter((chunk_of[i], grp[i]) for i in others)
print('  they belong to %d other groups; largest of those has %d'
      % (len(byg), max(byg.values()) if byg else 0))
