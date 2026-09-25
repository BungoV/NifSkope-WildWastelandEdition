"""TOWER1 discriminator: for each tower placement, the material swap vanilla Fallout4.esm puts on it (REFR XMSP, else
base MODS) and whether that MSWP has an entry for the LOD material our LOD model uses. Read only."""
import sys, pickle, collections
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
import nifwind
E = pickle.load(open('esm_refswaps.pkl', 'rb'))
CL = pickle.load(open('clusters.pkl', 'rb'))[:3]
def nm(p): return A.norm(p, 'materials')
lodmats = {}
def mats_of(m):
    if m not in lodmats:
        data, src = A.getfile(m, 'meshes'); N = nifwind.Nif(data); s = set()
        for k, (t, o, sz) in enumerate(N.blocks):
            if t in nifwind.SHAPES:
                sh = N.shape(k)
                if 0 <= sh['shader'] < len(N.blocks):
                    mt = N.shader_flags(sh['shader'])[2]
                    if mt: s.add(nm(mt))
        lodmats[m] = s
    return lodmats[m]
for tag, v in zip('ABC', CL):
    kinds = collections.Counter(); sw = collections.Counter(); hit = collections.Counter()
    for r in v:
        ref = int(r[5], 16)
        if ref >> 24: kinds['ref not in Fallout4.esm'] += 1; continue
        if ref not in E['refs']: kinds['ref not found'] += 1; continue
        b, x = E['refs'][ref]
        src = 'XMSP' if x else ('MODS' if E['base'].get(b, {}).get('mods') else None)
        x = x or E['base'].get(b, {}).get('mods', 0)
        if not x: kinds['no swap'] += 1; continue
        kinds['swap ' + src] += 1
        M = E['mswp'].get(x)
        sw[(x, M['edid'] if M else '?')] += 1
        lm = mats_of(r[8][0]) if r[8] else set()
        for o, rep, c in (M['subs'] if M else []):
            if nm(o) in lm: hit['%s -> %s' % (nm(o).split(A.BS)[-1], nm(rep).split(A.BS)[-1])] += 1
    print('tower %s: %d placements; %s' % (tag, len(v), dict(kinds)))
    print('   swaps:', [('%08x %s' % k, c) for k, c in sw.most_common(6)])
    print('   swap entries that name OUR LOD model material:', hit.most_common(8))
