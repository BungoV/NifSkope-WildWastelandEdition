# IMPOSTORLOOK1 section 2 -- SHADING, measured for the first time.
#
# Everything that has ever been scored on these cards is SILHOUETTE overlap.
# bungo's "impostors look off" is partly a shading complaint -- the mesh has a
# bright side that turns with the camera and the card looks evenly lit -- and
# nothing has ever put a number on it. This does.
#
# Per subject, per view, inside the INTERSECTION of the two silhouettes (so a
# coverage difference cannot masquerade as a brightness difference):
#
#   mean luma        mesh vs card, linear, and as displayed
#   trunk L-R        the lit-side signal: mean luma of the left half of the
#                    TRUNK band minus the right half. A lit object's value
#                    swings as the camera goes round it; an unlit one's does
#                    not. This is the number the complaint is about.
#   Lab dE           mean colour difference, CIE76, inside the intersection
#
# OFFLINE: reads existing grabs only.
import os, glob, json, math, sys
import numpy as np
from PIL import Image

REPO = 'E:/Projects/NifskopeWildWastelandEdition'
SC = REPO + '/scratchpad'
BG = np.array([43, 45, 49], float)

def srgb2lin(x):
    x = x/255.0
    return np.where(x <= 0.04045, x/12.92, ((x+0.055)/1.055)**2.4)

def lab(rgb_lin):
    M = np.array([[0.4124, 0.3576, 0.1805],
                  [0.2126, 0.7152, 0.0722],
                  [0.0193, 0.1192, 0.9505]])
    xyz = rgb_lin @ M.T
    wp = np.array([0.95047, 1.0, 1.08883])
    t = xyz/wp
    f = np.where(t > 0.008856, np.cbrt(t), 7.787*t + 16.0/116.0)
    L = 116*f[..., 1]-16; a = 500*(f[..., 0]-f[..., 1]); b = 200*(f[..., 1]-f[..., 2])
    return np.stack([L, a, b], -1)

def load(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    ink = np.abs(a-BG).sum(-1) > 12
    return a, ink

def band_lr(img, ink, mask, lo, hi):
    """mean luma of the left and right halves of the ink in a horizontal band
       between lo..hi of the ink's own height. Halves are split at the band's
       own ink centroid, so a leaning trunk does not bias the split."""
    ys, xs = np.nonzero(ink)
    if len(ys) < 50: return None
    y0, y1 = ys.min(), ys.max()
    a = int(y0 + (y1-y0)*lo); b = int(y0 + (y1-y0)*hi)
    sel = np.zeros_like(ink); sel[a:b+1, :] = True
    m = sel & mask
    if m.sum() < 60: return None
    yy, xx = np.nonzero(m)
    cx = xx.mean()
    lin = srgb2lin(img)
    lum = 0.2126*lin[..., 0]+0.7152*lin[..., 1]+0.0722*lin[..., 2]
    L = m & (np.arange(ink.shape[1])[None, :] < cx)
    R = m & (np.arange(ink.shape[1])[None, :] >= cx)
    if L.sum() < 20 or R.sum() < 20: return None
    return float(lum[L].mean()), float(lum[R].mean()), int(m.sum())

def run(folder, tag):
    rows = []
    for p in sorted(glob.glob(folder + '/v_az*_el*_mesh.png')):
        c = p.replace('_mesh.png', '_card.png')
        if not os.path.exists(c): continue
        name = os.path.basename(p)
        az = int(name[4:7]); el = int(name[10:12])
        mi, mk = load(p); ci, ck = load(c)
        I = mk & ck
        if I.sum() < 200: continue
        ml = srgb2lin(mi); cl = srgb2lin(ci)
        lum = lambda x: 0.2126*x[..., 0]+0.7152*x[..., 1]+0.0722*x[..., 2]
        mlum = float(lum(ml)[I].mean()); clum = float(lum(cl)[I].mean())
        dE = float(np.linalg.norm(lab(ml[I])-lab(cl[I]), axis=-1).mean())
        # the lit-side signal, measured on the TRUNK: the bottom quarter of the
        # object, where both mesh and card are solid wood and neither is twigs.
        bm = band_lr(mi, mk, I, 0.72, 0.97)
        bc = band_lr(ci, ck, I, 0.72, 0.97)
        rows.append(dict(az=az, el=el, cov_mesh=int(mk.sum()), cov_card=int(ck.sum()),
                         inter=int(I.sum()), lum_mesh=mlum, lum_card=clum, dE=dE,
                         lr_mesh=(bm[0]-bm[1]) if bm else None,
                         lr_card=(bc[0]-bc[1]) if bc else None,
                         n_band=(bm[2] if bm else 0)))
    return rows

def summarise(tag, rows):
    if not rows: return None
    lm = np.array([r['lum_mesh'] for r in rows]); lc = np.array([r['lum_card'] for r in rows])
    dE = np.array([r['dE'] for r in rows])
    bm = np.array([r['lr_mesh'] for r in rows if r['lr_mesh'] is not None])
    bc = np.array([r['lr_card'] for r in rows if r['lr_card'] is not None])
    out = dict(tag=tag, n=len(rows),
               lum_mesh=float(lm.mean()), lum_card=float(lc.mean()),
               lum_ratio=float(lc.mean()/max(lm.mean(), 1e-9)),
               # how much the mean brightness itself MOVES as the camera goes round
               swing_lum_mesh=float(lm.max()-lm.min()), swing_lum_card=float(lc.max()-lc.min()),
               dE=float(dE.mean()),
               lr_mesh_sd=float(bm.std()) if len(bm) else None,
               lr_card_sd=float(bc.std()) if len(bc) else None,
               lr_mesh_range=float(bm.max()-bm.min()) if len(bm) else None,
               lr_card_range=float(bc.max()-bc.min()) if len(bc) else None,
               nband=len(bm))
    return out

if __name__ == '__main__':
    W = SC + '/impostorfix5_20260919/control'
    res = {}
    for tag in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
        rows = run('%s/%s_after_b1' % (W, tag), tag)
        res[tag] = dict(summary=summarise(tag, rows), rows=rows)
        s = res[tag]['summary']
        print('%-9s n=%-3d lum mesh %.4f card %.4f (card/mesh %.3f)  dE %.2f' %
              (tag, s['n'], s['lum_mesh'], s['lum_card'], s['lum_ratio'], s['dE']))
        print('          brightness swing over the orbit:  mesh %.4f   card %.4f' %
              (s['swing_lum_mesh'], s['swing_lum_card']))
        print('          trunk left-minus-right luma, spread over the orbit:')
        print('              mesh  sd %.5f  range %.5f' % (s['lr_mesh_sd'], s['lr_mesh_range']))
        print('              card  sd %.5f  range %.5f   (%d views in the band)' %
              (s['lr_card_sd'], s['lr_card_range'], s['nband']))
    json.dump(res, open(os.path.dirname(os.path.abspath(__file__)) + '/shade.json', 'w'), indent=1)
