"""LAND=lo for the colour model (fill_model.py): the composite reads each cell's LAND from the plugin that wins it in
his load order (fo4lo_cw.pkl, land_lo_esm.py), LTEX/TXST from Fallout4.esm + the DLC masters, and a MATERIAL-BACKED
layer's diffuse from its .bgsm (the first string of a BGSM v2 is DiffuseTexture). Before this the model painted a
material-backed layer flat grey 0.5 -- which on DLCCoast's LDriedGrass01 cells would have been a made-up colour."""
import os, struct, pickle
HERE = os.path.dirname(os.path.abspath(__file__))


def bgsm_diffuse(path):
    b = open(path, 'rb').read()
    if b[:4] != b'BGSM':
        return None
    for i in range(8, min(len(b) - 8, 256)):
        n = struct.unpack_from('<I', b, i)[0]
        if 5 <= n <= 260 and i + 4 + n <= len(b) and b[i + 3 + n] == 0:
            t = b[i + 4:i + 3 + n]
            if all(32 <= c < 127 for c in t) and t.lower().endswith(b'.dds'):
                return t.decode('latin-1')
    return None


def setup(lp, tm):
    d = pickle.load(open(HERE + '/fo4lo_cw.pkl', 'rb'))
    lp.e.lands, lp.e.ltex, lp.e.txst = d['lands'], d['ltex_off'], d['txst_off']
    for k in [k for k in lp.cache if k != lp.DEF]:
        del lp.cache[k]
    Base = tm.Layer
    stats = {'bgsm': 0, 'unresolved': []}

    class Layer(Base):
        def __init__(s, esm, data, form):
            Base.__init__(s, esm, data, form)
            rec = esm.ltex.get(form)
            ts = esm.txst.get(rec['tnam']) if rec else None
            if s.diffuse is None and ts and ts['mnam']:
                m = tm.find_material(data, ts['mnam'])
                dd = bgsm_diffuse(m) if m else None
                a = tm.find_asset(data, dd) if dd else None
                if a:
                    s.diffuse = tm.Dds(a); stats['bgsm'] += 1; s.why = 'bgsm diffuse ' + dd
                else:
                    stats['unresolved'].append((rec['edid'], ts['mnam'], dd))
    tm.Layer = Layer
    return stats
