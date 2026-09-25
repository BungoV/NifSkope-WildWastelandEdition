"""Replay the §2.5 colour law (tests/spells/lodgen_terrain_model.composite) across the Sanctuary block's west and
north edges under two base-fallback rules, no build:
  A  dominant base of the enclosing dim-4 chunk (the shipped law)
  B  the engine's own default land texture (INI sDefaultLandDiffuseTexture:Landscape, default
     Ground\CommonwealthDefault01_d.dds, read from the exe's string table), one texture world-wide.
Also serves NULL-LTEX layers the same way (the engine's GetDefaultTexture). Step = |mean lum 1 cell inside - 1 cell
outside| over a band, and the step right AT the border (last 64 u vs first 64 u)."""
import sys, pickle, math, numpy as np
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/tests/spells')
import lodgen_terrain_model as tm
from lodgen_cover_model import dominant_base
DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
class E: pass
e = E(); d = pickle.load(open('fo4esm_cw.pkl', 'rb')); e.lands = d['lands']; e.ltex = d['ltex']; e.txst = d['txst']
cache = {}
DEF = 0xDEFA0001
class DefLayer(tm.Layer):
    def __init__(s):
        s.form = DEF; s.diffuse = tm.Dds(DATA + '/Textures/Landscape/Ground/CommonwealthDefault01_d.DDS'); s.spec = None
        s.gateable = True; s.smoothness = 1.0; s.roughConst = 1.0; s.edid = 'ENGINE DEFAULT'; s.rule = 'none-default'
cache[DEF] = DefLayer()
def dom(cx, cy):
    return dominant_base(e, cx - ((cx + 96) % 4), cy - ((cy + 96) % 4), 4)
def lum(c): return 255 * (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2])
def sample(wx, wy, rule):
    cx, cy = int(math.floor(wx / 4096)), int(math.floor(wy / 4096))
    db = dom(cx, cy) if rule == 'A' else DEF
    r = tm.composite(e, DATA, cache, cx, cy, 4, db, wx, wy, 32.0)
    return lum(r['colour']) if r else float('nan')
S = 64.0
def edge(axis, at_cells, band_cells, rule):
    """profile across the line `axis`=at (x or y, cells); band = the perpendicular extent (cells)"""
    a0 = at_cells * 4096; ts = np.arange(a0 - 4096 + S / 2, a0 + 4096, S)
    bs = np.arange(band_cells[0] * 4096 + S / 2, band_cells[1] * 4096, 256.0)
    prof = []
    for t in ts:
        vals = [sample(t, b, rule) if axis == 'x' else sample(b, t, rule) for b in bs]
        prof.append(np.nanmean(vals))
    prof = np.array(prof); h = len(prof) // 2
    return abs(prof[h:].mean() - prof[:h].mean()), abs(prof[h] - prof[h - 1]), float(np.median(np.abs(np.diff(prof))))
if __name__ == "__main__":
 for rule in "AB":
     for name, axis, at, band in (('west x=-20', 'x', -20, (20, 24)), ('north y=24', 'y', 24, (-20, -16)),
                                  ('south y=20', 'y', 20, (-20, -16)), ('east x=-16', 'x', -16, (20, 24))):
         st, at_b, med = edge(axis, at, band, rule)
         print('rule %s %-11s cell-mean step %5.2f  border step %5.2f  median neighbour step %4.2f' % (rule, name, st, at_b, med), flush=True)
