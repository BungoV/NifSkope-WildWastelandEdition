"""TINT1 side question: which materials do the placed LOD shapes name (slot-0 placements weighted)? Read only."""
exec(open('census.py').read().split('# ---- per mesh')[0])
import collections
place = collections.Counter(r['baseId'] for r in Ti['instances'])
per = collections.Counter()
for bid, c in place.items():
    B = L['bases'][bid]; ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
    if ms: per[ms[0]] += c
mats = collections.Counter(); mm = collections.Counter()
for i, nm in enumerate(names):
    if not per[i]: continue
    data, src = getnif(nm)
    if data is None: continue
    N = nifwind.Nif(data)
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES: continue
        sh = N.shape(k)
        if not (0 <= sh['shader'] < len(N.blocks)): continue
        f1, f2, mat = N.shader_flags(sh['shader'])
        mats[(mat or '').lower()] += per[i]; mm[(mat or '').lower()] += 1
print('distinct materials', len(mats))
for m, c in mats.most_common(25): print('  %7d %4d  %s' % (c, mm[m], m))
