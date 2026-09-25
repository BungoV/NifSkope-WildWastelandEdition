"""Side count: shapes with a colour channel but NO SLSF2 Vertex_Colors (the writer leaves them out, as the game
does per SEAM1's rule). Are the tinted buildings here?  Reuses census.py's resolver by exec of its first part."""
import sys
src = open(sys.argv[0].replace('census_noflag.py', 'census.py')).read().split('# ---- per mesh')[0]
exec(src)
import collections, numpy as np
lib = collections.Counter(); per = []
place = collections.Counter(r['baseId'] for r in Ti['instances'])
pm = collections.Counter()
for b, c in place.items():
    B = L['bases'][b]; ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
    if ms: pm[ms[0]] += c
for i, nm in enumerate(names):
    data, s = getnif(nm)
    if data is None: continue
    N = nifwind.Nif(data)
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES: continue
        sh = N.shape(k)
        f2 = 0
        if 0 <= sh['shader'] < len(N.blocks): f2 = N.shader_flags(sh['shader'])[1] or 0
        if sh['cols'] is None: lib['noChannel'] += 1; continue
        C = np.asarray(sh['cols'])[:, :3].astype(int)
        hue = bool(np.any((C[:, 0] != C[:, 1]) | (C[:, 1] != C[:, 2]))); shade = (not hue) and bool(np.any(C != 255))
        key = ('flag' if f2 & 0x20 else 'noflag') + ('_hue' if hue else '_shade' if shade else '_white')
        lib[key] += 1; lib[key + '_placements'] += pm[i]
        if key == 'noflag_hue': per.append((pm[i], nm))
print(dict(lib))
per.sort(reverse=True); print('noflag hue top:', [(c, n.split(chr(92))[-1]) for c, n in per[:12]])
