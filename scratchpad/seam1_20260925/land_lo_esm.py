"""The composite's ESM over his LOAD ORDER (coordinator order 16:2x, north-east gate, part 2): the colour model B
read Fallout4.esm's LAND everywhere (fo4esm_cw.pkl), so on the 253 cells where DLCCoast.esm's LAND wins it modelled
ground the game never draws. This builds the same {lands, ltex, txst} pickle from the load order:
  lands  per Commonwealth cell, the LAND of the LAST plugin that carries one (the whole record overrides)
  ltex / txst  every plugin's records, later overrides earlier
Form ids are made global: a plugin-local id whose index byte i < len(masters) belongs to masters[i], else to the
plugin itself; global = (load index of the owner << 24) | low 24 bits. Fallout4.esm is load index 0, so its ids are
unchanged and Fallout4.esm itself is read from fo4esm_cw.pkl (the same reader, already run).
Plugins = BAKE1's esm_list.txt. Writes fo4lo_cw.pkl. usage: land_lo_esm.py"""
import sys, os, struct, pickle, time, copy
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/tests/spells')
import lodgen_cover_model as lcm
PLUGINS = open('E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/esm_list.txt').read().strip().split(',')
NAMES = [p.split('/')[-1].lower() for p in PLUGINS]
assert NAMES[0] == 'fallout4.esm'


def masters(buf):
    size = struct.unpack_from('<I', buf, 4)[0]
    return [fd.rstrip(b'\0').decode('latin-1').lower() for t, fd in lcm.read_fields(buf[24:24 + size]) if t == b'MAST']


base = pickle.load(open(HERE + '/fo4esm_cw.pkl', 'rb'))
lands, ltex, txst = dict(base['lands']), dict(base['ltex']), dict(base['txst'])
# ltex_off / txst_off: the same merge over Fallout4.esm + the DLC masters only. The model reads textures from the
# UNPACKED vanilla Data, so a texture mod's LTEX/TXST override (BNS Landscape re-points 26 LTEX at its own materials)
# would resolve to nothing there; the model keeps Bethesda's records and says so.
ltex_off, txst_off = dict(base['ltex']), dict(base['txst'])
won = {}
t0 = time.time()
for li, path in enumerate(PLUGINS[1:], 1):
    e = lcm.Esm(path); ms = masters(e.buf)
    owner = [NAMES.index(m) for m in ms] + [li]

    def g(f):
        if f == 0:
            return 0
        i = f >> 24
        return (owner[min(i, len(ms))] << 24) | (f & 0xFFFFFF)
    e.walk(0x3C)
    off = NAMES[li].startswith('dlc')          # Bethesda's own masters: their assets are in the unpacked Data
    for f, r in e.txst.items():
        txst[g(f)] = r
        if off: txst_off[g(f)] = r
    for f, r in e.ltex.items():
        r = dict(r); r['tnam'] = g(r['tnam']); ltex[g(f)] = r
        if off: ltex_off[g(f)] = r
    for c, land in e.lands.items():
        land = copy.deepcopy(land)
        land['base'] = [g(b) for b in land['base']]
        for q in range(4):
            for lay in land['layers'][q]:
                lay['ltex'] = g(lay['ltex'])
        lands[c] = land; won[c] = NAMES[li]
    print('%-34s masters %d  LAND %4d LTEX %3d TXST %4d  (%.0f s)' % (
        NAMES[li], len(ms), len(e.lands), len(e.ltex), len(e.txst), time.time() - t0), flush=True)
lo = pickle.load(open(HERE + '/land_lo.pkl', 'rb'))['winner']
bad = [c for c, p in lo.items() if p.lower() != won.get(c, 'fallout4.esm')]
print('winner agreement with land_lo.pkl: %d cells, %d disagree %s' % (len(lo), len(bad), bad[:5]))
pickle.dump({'lands': lands, 'ltex': ltex, 'txst': txst, 'ltex_off': ltex_off, 'txst_off': txst_off, 'winner': won}, open(HERE + '/fo4lo_cw.pkl', 'wb'))
