"""TINT1 side question: is the building tint a greyscale-to-palette material rather than vertex colour?
Counts LOD shapes whose NIF shader has SLSF1 bit 4 (Greyscale_To_PaletteColor) and whose material is a .bgsm,
weighted by slot-0 placements (installed library). Read only."""
exec(open('census.py').read().split('# ---- per mesh')[0])
import collections
g2p = {}; bgsm = collections.Counter(); ex = []
for i, nm in enumerate(names):
    data, src = getnif(nm)
    if data is None: continue
    N = nifwind.Nif(data); a = b = 0
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES: continue
        sh = N.shape(k)
        if not (0 <= sh['shader'] < len(N.blocks)): continue
        f1, f2, mat = N.shader_flags(sh['shader'])
        if f1 is not None and f1 & 0x10: a += 1
        if mat and mat.lower().endswith('.bgsm'): b += 1
    g2p[i] = a; bgsm['meshes_with_bgsm'] += bool(b)
place = collections.Counter(r['baseId'] for r in Ti['instances'])
per = collections.Counter()
for bid, c in place.items():
    B = L['bases'][bid]; ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
    if ms: per[ms[0]] += c
print('meshes with a G2P shape: %d of %d; slot-0 placements %d; meshes with any .bgsm: %d' % (
    sum(1 for v in g2p.values() if v), len(names), sum(per[m] for m, v in g2p.items() if v), bgsm['meshes_with_bgsm']))
for m in sorted((m for m, v in g2p.items() if v), key=lambda m: -per[m])[:12]:
    print('  %6d  %s' % (per[m], names[m][-70:]))
