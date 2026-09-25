"""TINT1 side question, part 2: bGrayscaleToPaletteColor read from each placed LOD shape's BGSM (the material
wins over the NIF flag). Layout: NifSkope src/io/materialfile.cpp Material::readFile. Materials resolved through
his MO2 stack (loose highest first, then GNRL BA2s), same resolver as census.py. Read only."""
exec(open('census.py').read().split('# ---- per mesh')[0])
import collections, struct
def getmat(m):
    m = m.lower().replace('/', BS)
    j = m.find('materials' + BS)
    if j < 0: m = 'materials' + BS + m
    else: m = m[j:]
    for r in reversed(RES):
        p = r + '/' + m.replace(BS, '/')
        if os.path.isfile(p): return open(p, 'rb').read()
    for an, a in arcs:
        if m in a['recs']: return ba2lib.get(a, m)
    return None
def g2p(b):
    if b[:4] != b'BGSM': return None
    v, = struct.unpack_from('<I', b, 4); o = 8 + 4 + 16 + 4 + 1 + 4 + 4 + 1 + 11 + 4 + 1
    if v < 10: o += 4
    x = b[o]; assert x in (0, 1), (v, x); return x
place = collections.Counter(r['baseId'] for r in Ti['instances'])
per = collections.Counter()
for bid, c in place.items():
    B = L['bases'][bid]; ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
    if ms: per[ms[0]] += c
res = {}; w = collections.Counter(); cnt = collections.Counter()
for i, nm in enumerate(names):
    if not per[i]: continue
    data, src = getnif(nm)
    if data is None: continue
    N = nifwind.Nif(data); hit = False
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES: continue
        sh = N.shape(k)
        if not (0 <= sh['shader'] < len(N.blocks)): continue
        f1, f2, mat = N.shader_flags(sh['shader'])
        if not mat or not mat.lower().endswith('.bgsm'): continue
        if mat not in res:
            b = getmat(mat); res[mat] = None if b is None else g2p(b)
        cnt[res[mat]] += 1
        hit |= res[mat] == 1
    if hit: w['placements'] += per[i]; w['meshes'] += 1
print('bgsm shapes by G2P value (None = unresolved):', dict(cnt), '; materials', len(res), 'unresolved', sum(v is None for v in res.values()))
print('meshes/placements with a G2P material:', dict(w))
print('G2P materials:', sorted(m for m, v in res.items() if v == 1)[:20])
