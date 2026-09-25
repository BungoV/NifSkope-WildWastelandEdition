"""TOWER1 refuter (offline): ours per tower again, but with vanilla's material swap applied to the LOD materials
(the MSWP entry whose original is the LOD model's material -> its replacement). If the swap is the cause, this
must land on vanilla's atlas-sampled colour. Read only."""
import sys, pickle
import numpy as np
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
_src = open('van_tower.py').read()
exec(_src[_src.index('def stats'):_src.index('def van')])
CL = pickle.load(open('clusters.pkl', 'rb'))[:3]
E = pickle.load(open('esm_refswaps.pkl', 'rb'))
VT = pickle.load(open('van_tower.pkl', 'rb'))
def nm(p): return A.norm(p, 'materials')
swl = {x: {nm(o): nm(r) for o, r, c in M['subs']} for x, M in E['mswp'].items()}
orig_bgsm = A.bgsm
SUB = {}
def bgsm_sw(name):
    r = SUB.get(nm(name)) if name else None
    return orig_bgsm(r if r else name)
A.bgsm = bgsm_sw
def ours_sw(v, apply):
    RGB, WG = [], []; cache = {}
    for r in v:
        if not r[8]: continue
        ref = int(r[5], 16); x = 0
        if apply and not ref >> 24:
            b, x = E['refs'].get(ref, (0, 0)); x = x or E['base'].get(b, {}).get('mods', 0)
        key = (r[8][0], x)
        if key not in cache:
            SUB.clear(); SUB.update(swl.get(x, {}))
            data, src = A.getfile(r[8][0], 'meshes'); keep = []
            A.measure(data, K=8, keep=keep); cache[key] = keep
        for rgb, w in cache[key]:
            RGB.append(rgb); WG.append(w * r[6] ** 2)
    return stats(np.concatenate(RGB), np.concatenate(WG))
fmt = lambda s: 'sRGB %s S_of_mean %.3f mean_S %.3f Y %.4f' % (np.round(s['srgb'], 3), s['S_of_mean'], s['mean_S'], s['Y'])
for tag, v in zip('ABC', CL):
    print('tower', tag)
    print('  vanilla atlas       ', fmt(VT[tag][0]))
    print('  ours as baked       ', fmt(ours_sw(v, False)))
    print('  ours + vanilla swap ', fmt(ours_sw(v, True)))
