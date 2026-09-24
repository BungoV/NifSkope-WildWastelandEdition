# IMPOSTORLOOK1 -- the brief's items 1, 2 and 3.
#   B pictures : side-by-side pairs at the grabs' native size, azimuths
#                0/90/180/270 (never the flattering view), two subjects,
#                plus x3 NEAREST crops of the trunk and of one fork.
#   TRANSFER   : the card's luma against the mesh's, inside the intersection
#                of the two silhouettes, as a CURVE -- a pure gamma/tonemap
#                difference is a smooth monotone curve, a missing highlight
#                diverges at the bright end.
#   OUTLINE    : how rough the trunk edge is, mesh vs card, and whether its
#                steps are 1 sheet texel (frame resolution) or 4 (a BC block).
# OFFLINE: existing grabs only.
import os, sys, glob, json, math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = 'E:/Projects/NifskopeWildWastelandEdition'
SC   = REPO + '/scratchpad'
MINE = SC + '/impostorlook1_20260919'
OUT  = MINE + '/images'
BG   = np.array([43, 45, 49], float)
ORB  = SC + '/impostorfix5_20260919/control'
BAKE = {'blast_n4': SC + '/impostorfix5_20260919/control/blast_n4_bake_ctl',
        'maple_n4': SC + '/impostorfix5_20260919/control/maple_n4_bake_on'}

def font(s):
    for p in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(p):
            try: return ImageFont.truetype(p, s)
            except Exception: pass
    return ImageFont.load_default()

def load(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    return a, (np.abs(a-BG).sum(-1) > 12)

def srgb2lin(x):
    x = x/255.0
    return np.where(x <= 0.04045, x/12.92, ((x+0.055)/1.055)**2.4)

def luma_lin(a):
    l = srgb2lin(a)
    return 0.2126*l[..., 0]+0.7152*l[..., 1]+0.0722*l[..., 2]

# ---------------------------------------------------------------- B pictures
def pairs(tag, folder, el, azs, name, note):
    ims = []
    for az in azs:
        m = '%s/v_az%03d_el%02d_mesh.png' % (folder, az, el)
        c = '%s/v_az%03d_el%02d_card.png' % (folder, az, el)
        if not (os.path.exists(m) and os.path.exists(c)): return None
        ims.append((az, m, c))
    X0, X1, Y0, Y1 = 10**9, -1, 10**9, -1
    for az, m, c in ims:
        for p in (m, c):
            _, k = load(p)
            if not k.any(): continue
            ys, xs = np.nonzero(k)
            X0 = min(X0, xs.min()); X1 = max(X1, xs.max())
            Y0 = min(Y0, ys.min()); Y1 = max(Y1, ys.max())
    pad = 8
    x0 = max(0, int(X0)-pad); y0 = max(0, int(Y0)-pad)
    w = int(X1)-x0+pad; h = int(Y1)-y0+pad
    cap, sub, gap, mar = 52, 34, 14, 16
    sheet = Image.new('RGB', (mar*2+len(ims)*(2*w+gap)+(len(ims)-1)*gap*2, mar+cap+h+sub+mar), (24, 25, 28))
    dr = ImageDraw.Draw(sheet); f1 = font(19); f2 = font(15); f3 = font(13)
    dr.text((mar, mar-2), '%s -- mesh | card, at the grabs\' own size, azimuths %s, elevation %d.  %s'
            % (tag, '/'.join(str(a) for a in azs), el, note), font=f1, fill=(235, 235, 235))
    for n, (az, m, c) in enumerate(ims):
        bx = mar + n*(2*w+3*gap)
        for k, p in enumerate((m, c)):
            im = Image.open(p).convert('RGB').crop((x0, y0, x0+w, y0+h))
            sheet.paste(im, (bx+k*(w+gap), mar+cap))
            dr.text((bx+k*(w+gap)+2, mar+cap-20), 'az %d  %s' % (az, 'MESH' if k == 0 else 'CARD'),
                    font=f2, fill=(150, 200, 150) if k == 0 else (210, 195, 140))
    p = '%s/%s' % (OUT, name)
    sheet.save(p); print('  ', name, sheet.size)
    return p, (x0, y0, w, h)

def crop3x(tag, folder, az, el, frac_lo, frac_hi, name, title):
    m = '%s/v_az%03d_el%02d_mesh.png' % (folder, az, el)
    c = '%s/v_az%03d_el%02d_card.png' % (folder, az, el)
    mi, mk = load(m); ci, ck = load(c)
    U = mk | ck
    ys, xs = np.nonzero(U)
    y0, y1 = ys.min(), ys.max(); x0, x1 = xs.min(), xs.max()
    a = int(y0+(y1-y0)*frac_lo); b = int(y0+(y1-y0)*frac_hi)
    cx0 = max(0, x0-10); cx1 = min(mi.shape[1], x1+10)
    K = 3
    panels = []
    for img in (mi, ci):
        im = Image.fromarray(img[a:b, cx0:cx1].astype(np.uint8))
        panels.append(im.resize((im.size[0]*K, im.size[1]*K), Image.NEAREST))
    w, h = panels[0].size
    cap, gap, mar = 66, 14, 16
    sheet = Image.new('RGB', (mar*2+2*w+gap, mar+cap+h+mar), (24, 25, 28))
    dr = ImageDraw.Draw(sheet); f1 = font(19); f2 = font(15)
    dr.text((mar, mar), title, font=f1, fill=(235, 235, 235))
    dr.text((mar, mar+22), 'az %d el %d, x3 nearest -- every square is one screen pixel of the original grab'
            % (az, el), font=f2, fill=(170, 172, 178))
    for k, im in enumerate(panels):
        sheet.paste(im, (mar+k*(w+gap), mar+cap))
        dr.text((mar+k*(w+gap)+2, mar+cap-17), 'MESH' if k == 0 else 'CARD', font=f2,
                fill=(150, 200, 150) if k == 0 else (210, 195, 140))
    p = '%s/%s' % (OUT, name); sheet.save(p); print('  ', name, sheet.size)
    return p

# ---------------------------------------------------------- transfer + outline
def transfer(tag):
    f = '%s/%s_after_b1' % (ORB, tag)
    xs, ys = [], []
    for p in sorted(glob.glob(f+'/v_az*_mesh.png')):
        c = p.replace('_mesh.png', '_card.png')
        mi, mk = load(p); ci, ck = load(c)
        I = mk & ck
        if I.sum() < 500: continue
        xs.append(luma_lin(mi)[I]); ys.append(luma_lin(ci)[I])
    x = np.concatenate(xs); y = np.concatenate(ys)
    edges = np.array([0.0, .04, .08, .12, .16, .20, .25, .30, .40, .55, 1.01])
    out = []
    for i in range(len(edges)-1):
        m = (x >= edges[i]) & (x < edges[i+1])
        if m.sum() < 200: out.append(None); continue
        out.append((float(x[m].mean()), float(y[m].mean()), int(m.sum())))
    return out, float(x.mean()), float(y.mean()), len(x)

def edge_rough(path_mesh, path_card, texels_across):
    """std of the trunk's LEFT edge x, about a straight-line fit, over the
       bottom third -- in screen pixels and in SHEET TEXELS. A texel-shaped
       edge has steps of about one texel; a BC-block one, four."""
    res = {}
    for kind, p in (('mesh', path_mesh), ('card', path_card)):
        a, k = load(p)
        ys, xs = np.nonzero(k)
        y0, y1 = ys.min(), ys.max()
        lo = int(y0+(y1-y0)*0.66); hi = int(y0+(y1-y0)*0.97)
        ex, ey = [], []
        for y in range(lo, hi):
            r = np.nonzero(k[y])[0]
            if len(r) < 3: continue
            ex.append(r.min()); ey.append(y)
        if len(ex) < 30: res[kind] = None; continue
        ex = np.array(ex, float); ey = np.array(ey, float)
        A = np.polyfit(ey, ex, 1)
        r = ex-np.polyval(A, ey)
        w = float(np.median([np.nonzero(k[int(y)])[0].max()-np.nonzero(k[int(y)])[0].min()+1 for y in ey]))
        res[kind] = dict(sd_px=float(r.std()), width_px=w,
                         px_per_texel=w/float(texels_across),
                         sd_texel=float(r.std())/(w/float(texels_across)),
                         n=len(ex))
    return res

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    print('B pictures:')
    pairs('blast_n8 (the bare blasted maple, N=8)', ORB+'/blast_n8_after_b1', 15, [0, 90, 180, 270],
          'B_pairs_blast_n8.png', 'the 24-view orbit run, shipped sheets, parallax + blend ON')
    pairs('maple_n4 (the fine-twig maple)', ORB+'/maple_n4_after_b1', 15, [0, 90, 180, 270],
          'B_pairs_maple_n4.png', 'the 24-view orbit run, shipped sheets, parallax + blend ON')
    crop3x('blast_n4', BAKE['blast_n4'], 180, 0, 0.62, 0.99, 'B_crop_trunk_blast_n4_x3.png',
           'blast_n4 TRUNK, at a BAKE direction (parallax is provably a no-op here)')
    crop3x('blast_n4', BAKE['blast_n4'], 180, 0, 0.02, 0.34, 'B_crop_fork_blast_n4_x3.png',
           'blast_n4 the FORK, at a BAKE direction (parallax is provably a no-op here)')
    crop3x('maple_n4', BAKE['maple_n4'], 180, 0, 0.02, 0.34, 'B_crop_crown_maple_n4_x3.png',
           'maple_n4 the CROWN, at a BAKE direction (parallax is provably a no-op here)')

    print('\nTRANSFER: card luma against mesh luma, both linear, inside the intersection')
    print('%-9s %s' % ('subject', '  '.join('%6.3f' % v for v in
          [.02, .06, .10, .14, .18, .22, .27, .35, .47, .78])))
    T = {}
    for tag in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4'):
        rows, mx, my, n = transfer(tag)
        T[tag] = dict(rows=rows, mean_mesh=mx, mean_card=my, n=int(n))
        print('%-9s %s   n=%d' % (tag, '  '.join(('%6.3f' % r[1]) if r else '     -' for r in rows), n))

    print('\nOUTLINE: trunk left-edge roughness about a straight fit, bottom third')
    E = {}
    for tag, folder, tex in (('blast_n4', BAKE['blast_n4'], 48), ('maple_n4', BAKE['maple_n4'], 32)):
        r = edge_rough('%s/v_az180_el00_mesh.png' % folder, '%s/v_az180_el00_card.png' % folder, tex)
        E[tag] = r
        for k in ('mesh', 'card'):
            d = r[k]
            print('  %-9s %-5s sd %6.3f px = %5.3f sheet texels   (trunk %5.1f px wide, %5.2f px per texel, %d rows)'
                  % (tag, k, d['sd_px'], d['sd_texel'], d['width_px'], d['px_per_texel'], d['n']))
    json.dump(dict(transfer=T, edge=E), open(MINE+'/look.json', 'w'), indent=1)
