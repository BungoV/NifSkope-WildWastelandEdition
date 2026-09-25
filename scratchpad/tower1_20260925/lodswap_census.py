"""TOWER1: whole Commonwealth -- how many placements of our installed library (dim-4 manifests, slot-0 LOD mesh) carry
a vanilla Fallout4.esm material swap (REFR XMSP, else base MODS) whose MSWP has an entry naming a material of that
LOD mesh, i.e. a swap the CK applies to vanilla's LOD and our bake does not. Read only."""
import sys, glob, pickle, collections
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
import nifwind
E = pickle.load(open('esm_refswaps.pkl', 'rb'))
L = A.L; names = A.names
byFid = {B['formId']: B for B in L['bases']}
def nm(p): return A.norm(p, 'materials')
lm = {}
def mats(m):
    if m not in lm:
        data, src = A.getfile(names[m], 'meshes'); s = set()
        if data:
            N = nifwind.Nif(data)
            for k, (t, o, sz) in enumerate(N.blocks):
                if t in nifwind.SHAPES:
                    sh = N.shape(k)
                    if 0 <= sh['shader'] < len(N.blocks):
                        mt = N.shader_flags(sh['shader'])[2]
                        if mt: s.add(nm(mt))
        lm[m] = s
    return lm[m]
swl = {x: {nm(o): nm(r) for o, r, c in M['subs']} for x, M in E['mswp'].items()}
tot = collections.Counter(); fam = collections.Counter(); pairs = collections.Counter()
for f in glob.glob(A.D + '/Commonwealth.4.*.BTO.manifest.txt'):
    for ln in open(f):
        p = ln.split()
        if ln.startswith('#') or len(p) != 11: continue
        tot['placements'] += 1
        ref = int(p[9], 16); B = byFid.get(int(p[1], 16))
        if ref >> 24: tot['ref not Fallout4.esm'] += 1; continue
        if not B or B['rep0'] == 0xFFFF: tot['no slot-0 mesh'] += 1; continue
        b, x = E['refs'].get(ref, (0, 0))
        x = x or E['base'].get(b, {}).get('mods', 0)
        if not x: continue
        tot['swapped (XMSP or MODS)'] += 1
        S = swl.get(x, {})
        hit = [(o, S[o]) for o in mats(B['rep0']) if o in S]
        if hit:
            tot['swap names the LOD material'] += 1
            fam[p[7]] += 1
            for o, r in hit: pairs['%s -> %s' % (o.split(A.BS)[-1], r.split(A.BS)[-1])] += 1
print(dict(tot)); print('by class:', dict(fam))
print('top LOD material swaps:'); [print('  %6d %s' % (c, k)) for k, c in pairs.most_common(15)]
print('distinct LOD swap pairs:', len(pairs))
