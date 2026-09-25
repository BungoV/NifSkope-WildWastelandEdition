"""GRASSCOL: alpha-weighted mean at the first mip <= LIM on its long side (what the fix reads) vs mip 0 (truth)."""
import sys, numpy as np
sys.argv = ['x', 'NUL']
src = open('grass_census.py').read(); src = src[:src.index('# ------------------------------------------------------------------ report')]
exec(src)
LIM = int(__import__('os').environ.get('LIM', 128)); worst = 0
for rel in [r'textures\landscape\grass\DriedGrassObj01_D.dds', r'textures\Landscape\Grass\PreWarLawnGrassObj01_d.dds',
            'textures\landscape\grass\driedgrassobj02_d.dds', r'textures\Landscape\Grass\TG_MutatedGrass_d.dds',
            'textures\bns\landscape\grass\wildgrassatlas01_d.dds', r'textures\landscape\grass\meadowgrass01_d.dds',
            'textures\Landscape\Grass\TG_MutatedPlant02_D.dds']:
    lab, x = resolve(rel.lower()); w, h, mips, fmt, data = dds_parse(x); ml = mips_of(w, h, mips, fmt, data)
    def aw(m):
        mw, mh, o = ml[m]; im = bc_decode(data[o:], mw, mh, fmt).astype(float) / 255
        a = im[..., 3]; return (im[..., :3] * a[..., None]).sum((0, 1)) / a.sum(), a.mean()
    k = next(i for i, (mw, mh, o) in enumerate(ml) if max(mw, mh) <= LIM)
    t, a0 = aw(0); f, ak = aw(k); d = np.abs(t - f).max() * 255; worst = max(worst, d)
    print('%-52s mip0 %s a %.3f | mip%d %dx%d %s a %.3f | max diff %.1f' % (rel[-40:], (t * 255).round(), a0, k, ml[k][0], ml[k][1], (f * 255).round(), ak, d))
print('worst', round(worst, 1))
