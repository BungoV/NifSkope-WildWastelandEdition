"""TOWER1 step 1b: per tower (clusters.pkl from towers.py), the slot-0 LOD models, their shapes' materials and the
diffuse texture each resolves to through his MO2 stack (GREY1 atlas_vs_full.py resolver, imported). Read only."""
import sys, pickle, collections
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
import nifwind
CL = pickle.load(open('clusters.pkl', 'rb'))[:3]
tags = ['A', 'B', 'C']
allm = {}
for tag, v in zip(tags, CL):
    mc = collections.Counter(r[8][0] for r in v if r[8])
    print('tower %s: %d placements, %d distinct slot-0 models; top: %s' % (tag, len(v), len(mc), [(m.split(A.BS)[-1], c) for m, c in mc.most_common(6)]))
    for m in mc: allm[m] = allm.get(m, 0) + mc[m]
mats = collections.Counter(); texsrc = collections.Counter(); missing = collections.Counter()
for m, c in allm.items():
    data, src = A.getfile(m, 'meshes')
    if data is None:
        missing['nif:' + m] += c; continue
    N = nifwind.Nif(data)
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES: continue
        sh = N.shape(k)
        f1, f2, mat = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (None, None, '')
        M = A.bgsm(mat) if mat else None
        tex = M['tex'][0] if M and M.get('tex') else ''
        img, why = A.gettex(tex) if tex else (None, 'notex')
        mats[(mat.lower(), tex.lower(), why if img is None else 'OK ' + why, (M or {}).get('src'))] += c
for k, c in mats.most_common():
    print('%5d  %s' % (c, k))
print('missing:', dict(missing))
