# Offline: vertices the native viewer would emit for the whole map per WW_LODI_SLOT (sum of cluster vertex counts of the
# drawn mesh over every placement; the viewer's cap is 9,500,000). Decoder = tests/spells/lodgen_native_decode.py.
import sys, collections
sys.path.insert(0, r'E:/Projects/NifskopeWWE-extent1/tests/spells')
import lodgen_native_decode as D
F = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/'
L = D.read_lodo(F + 'Commonwealth.lodo'); T = D.read_lodi(F + 'Commonwealth.lodi')
NO = 0xFFFF
mv = [0] * len(L['meshes'])
for c in L['clusters']: mv[c['meshId']] += c['vertexCount']
nb = len(L['bases'])
for slot in (None, 0, 1, 2, 3):
    v = 0; drawn = 0; nomesh = 0
    for r in T['instances']:
        if r['baseId'] >= nb: nomesh += 1; continue
        B = L['bases'][r['baseId']]; reps = [B['rep0'], B['rep1'], B['rep2'], B['rep3']]
        m = NO
        if slot is None:
            for x in reps:
                if x != NO and x < len(mv): m = x; break
        elif reps[slot] != NO and reps[slot] < len(mv): m = reps[slot]
        if m == NO: nomesh += 1; continue
        v += mv[m]; drawn += 1
    print('slot %s: %d placements drawn, %d without a mesh at that slot, ~%d vertices (cap 9,500,000)' % (slot, drawn, nomesh, v))

# split: slot-0 vertices on each side of a world-x boundary (placement cell = floor(x/4096))
import math
xs = []
for r in T['instances']:
    if r['baseId'] >= nb: continue
    B = L['bases'][r['baseId']]
    if B['rep0'] == NO or B['rep0'] >= len(mv): continue
    xs.append((math.floor(r['x'] / 4096), mv[B['rep0']]))
for bx in (-16, -12, -8, -4, 0):
    w = sum(v for c, v in xs if c < bx); e = sum(v for c, v in xs if c >= bx)
    print('slot 0 split at cell x %d: west %d, east %d vertices' % (bx, w, e))
