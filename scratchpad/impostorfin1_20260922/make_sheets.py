# IMPOSTORFIN1 -- make_A_sheets.py (IMPOSTORLOOK1) parameterised: one "every
# bake angle" contact sheet per subject, MESH | CARD as drawn | RAW FRAME, for
# any set root and any grab root.
#   python make_sheets.py SETROOT GRABROOT OUTDIR LABEL
import sys, os, json, glob, math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = 'E:/Projects/NifskopeWildWastelandEdition'
FX, GR, OUT, LABEL = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
sys.path.insert(0, REPO + '/tests/spells')
import impostor_bc_decode as bc

BG = np.array([43, 45, 49], float)      # the viewport background in every grab

GRABS = {t: (GR + '/' + t, LABEL) for t in ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')}
MESHOF = {
  'blast_n4': 'Trees/TreeMapleblasted05.nif',
  'blast_n8': 'Trees/TreeMapleblasted05.nif',
  'maple_n4': 'Trees/TreeMapleForest2.nif',
  'dead_n4' : 'Trees/BlastedForestDestroyedTreeUpright01.nif',
  'rock_n4' : 'Rocks/RockCliff02_Alt.nif',
}

def font(sz):
    for p in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(p):
            try: return ImageFont.truetype(p, sz)
            except Exception: pass
    return ImageFont.load_default()

def views(N):
    """The bake directions, from the .lodm's own grid, in frame order j*N+i.
       Byte-for-byte the map in tests/spells/impostor_bake_views.py."""
    out = []
    for j in range(N):
        for i in range(N):
            u = i/float(N-1)*2.0-1.0; v = j/float(N-1)*2.0-1.0
            x = (u+v)*0.5; y = (u-v)*0.5; z = 1.0-abs(x)-abs(y)
            n = math.sqrt(x*x+y*y+z*z); x,y,z = x/n, y/n, z/n
            el = math.degrees(math.asin(max(-1.0, min(1.0, z))))
            az = math.degrees(math.atan2(y, x)) % 360.0
            out.append((i, j, az, el))
    return out

def card_meta(tag):
    p = glob.glob('%s/%s/cards/*_oct.lodm' % (FX, tag))[0]
    raw = open(p, 'rb').read()
    return json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card'], p

def coverage_of(a, floor, base):
    """The drawer's coverage decode (res/shaders/impostor_oct.frag, and
       refcard.py's coverageOf): the sheet stores coverage remapped into
       [base..255]; below base is empty."""
    f = floor/255.0; b = base/255.0
    if b <= 0: return np.where(a < 16/255.0, 0.0, a)
    out = np.clip(f + (a-b)*(1.0-f)/(1.0-b), f, 1.0)
    return np.where(a < b, 0.0, out)

def load_grab(folder, az, el, kind):
    """Grab filenames are the view list truncated: az153.4349 -> v_az153_el00."""
    p = '%s/v_az%03d_el%02d_%s.png' % (folder, int(az), int(abs(el)), kind)
    return p if os.path.exists(p) else None

def ink_bbox(paths):
    X0, X1, Y0, Y1 = 10**9, -1, 10**9, -1
    for p in paths:
        a = np.asarray(Image.open(p).convert('RGB')).astype(float)
        m = np.abs(a-BG).sum(-1) > 12
        if not m.any(): continue
        ys, xs = np.nonzero(m)
        X0 = min(X0, xs.min()); X1 = max(X1, xs.max())
        Y0 = min(Y0, ys.min()); Y1 = max(Y1, ys.max())
    if X1 < 0: return None
    return int(X0), int(X1), int(Y0), int(Y1)

def frame_panel(alb, N, i, j, floor, base, K):
    """Frame (i,j) cut from the sheet -- sheet cell = (W//N, H//N), the layout
       refcard.py's uv=(i+u)/N, (j+v)/N implies and the DDS dimensions confirm
       (192x512 at N=4 is exactly 4x 48x128). Coverage decoded, composited on
       the grabs' own background, enlarged K x NEAREST."""
    H, W = alb.shape[:2]; fw = W//N; fh = H//N
    f = alb[j*fh:(j+1)*fh, i*fw:(i+1)*fw]
    cov = coverage_of(f[..., 3], floor, base)[..., None]
    rgb = np.clip(f[..., :3], 0, 1)*255.0
    out = rgb*cov + BG[None, None, :]*(1.0-cov)
    im = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))
    return im.resize((fw*K, fh*K), Image.NEAREST), fw, fh

def build(tag, maxdim=int(os.environ.get('SHEET_MAXDIM', '5400'))):
    meta, lodm = card_meta(tag)
    N = int(meta['oct']); floor = meta['coverage']['floor']; base = meta['coverage']['base']
    dds = glob.glob('%s/%s/cards/*_oct_d.DDS' % (FX, tag))[0]
    alb, dim, fourcc = bc.load_dds(dds)
    vs = views(N)
    gdir, gwhat = GRABS[tag]

    # the crop shared by every grab of this subject -- same crop, so same scale
    crop = None
    if gdir:
        crop = ink_bbox(sorted(glob.glob(gdir + '/*.png')))
    if crop:
        x0, x1, y0, y1 = crop
        pad = 6
        x0 = max(0, x0-pad); y0 = max(0, y0-pad)
        gw = x1-x0+1+pad; gh = y1-y0+1+pad
        fh0 = alb.shape[0]//N
        K = max(1, int(gh//fh0))            # integer NEAREST enlargement only
    else:
        # No grab exists anywhere for this subject. The MISSING placeholder
        # takes the FRAME panel's own shape, so the cell still reads as three
        # views of one object and the frame is never squeezed.
        fh0 = alb.shape[0]//N
        K = 4
        gh = fh0*K; gw = max((alb.shape[1]//N)*K, 150)
    fpanel_h = fh0*K; fpanel_w = (alb.shape[1]//N)*K

    cap = 26; sub = 18; gap = 10; margin = 16
    cellw = gw + gap + gw + gap + fpanel_w
    cellh = cap + max(gh, fpanel_h) + sub
    Wt = margin*2 + N*cellw + (N-1)*gap
    Ht = margin*2 + 54 + N*cellh + (N-1)*gap

    sheet = Image.new('RGB', (Wt, Ht), (24, 25, 28))
    dr = ImageDraw.Draw(sheet)
    f1 = font(20); f2 = font(15); f3 = font(13)
    dr.text((margin, margin), '%s -- %s -- every bake direction (%d frames), model | impostor'
            % (tag, MESHOF[tag], N*N), font=f1, fill=(235, 235, 235))
    dr.text((margin, margin+26),
            'left = original mesh   middle = impostor card as the viewer draws it   right = the raw baked frame out of %s, %dx%d texels, coverage decoded, %dx nearest   %s'
            % (os.path.basename(dds), alb.shape[1]//N, fh0, K,
               ('grabs: ' + gwhat) if gwhat else 'NO mesh/card grabs exist at this subject\'s bake directions'),
            font=f3, fill=(170, 172, 178))

    y0s = margin+54
    for idx, (i, j, az, el) in enumerate(vs):
        cx = margin + i*(cellw+gap)
        cy = y0s + j*(cellh+gap)
        dr.rectangle([cx-2, cy-2, cx+cellw+1, cy+cellh+1], outline=(60, 62, 68))
        dr.text((cx+2, cy+4), 'f%02d  i=%d j=%d   az %6.2f  el %5.2f' % (idx, i, j, az, el),
                font=f2, fill=(220, 222, 228))
        py = cy + cap
        for k, kind in enumerate(('mesh', 'card')):
            px = cx + k*(gw+gap)
            p = load_grab(gdir, az, el, kind) if gdir else None
            if p:
                im = Image.open(p).convert('RGB').crop((x0, y0, x0+gw, y0+gh))
                sheet.paste(im, (px, py))
                dr.text((px+2, py+gh+1), kind + ('  (original model)' if kind == 'mesh' else '  (impostor)'),
                        font=f3, fill=(150, 200, 150) if kind == 'mesh' else (200, 190, 140))
            else:
                dr.rectangle([px, py, px+gw-1, py+gh-1], fill=(48, 30, 30), outline=(150, 70, 70))
                dr.text((px+8, py+gh//2-8), 'MISSING', font=f1, fill=(235, 130, 130))
                dr.text((px+8, py+gh//2+12), 'no grab exists', font=f3, fill=(200, 150, 150))
                dr.text((px+2, py+gh+1), kind + ' -> shoot_missing.sh', font=f3, fill=(200, 120, 120))
        px = cx + 2*(gw+gap)
        fp, fw0, fh0b = frame_panel(alb, N, i, j, floor, base, K)
        sheet.paste(fp, (px, py))
        lbl = 'baked frame %dx%d x%d' % (fw0, fh0b, K)
        if dr.textlength(lbl, font=f3) > fpanel_w:
            lbl = '%dx%d x%d' % (fw0, fh0b, K)
        dr.text((px+2, py+fpanel_h+1), lbl, font=f3, fill=(150, 175, 215))

    if max(sheet.size) > maxdim:
        s = maxdim/float(max(sheet.size))
        # the frame panel must stay AT OR ABOVE its own texel count
        assert fpanel_h*s >= fh0, 'downscale would put the frame below native'
        sheet = sheet.resize((int(sheet.size[0]*s), int(sheet.size[1]*s)), Image.LANCZOS)
    p = '%s/%s_every_bake_angle.png' % (OUT, tag)
    sheet.save(p)
    ng = sum(1 for (i, j, az, el) in vs if gdir and load_grab(gdir, az, el, 'mesh'))
    print('%-9s %-28s frames=%-3d grabs=%-3d missing=%-3d  %dx%d' %
          (tag, os.path.basename(p), N*N, ng, N*N-ng, sheet.size[0], sheet.size[1]))
    return p, N*N, ng

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for t in ('blast_n4', 'maple_n4', 'blast_n8', 'dead_n4', 'rock_n4'):
        if glob.glob('%s/%s/cards/*_oct.lodm' % (FX, t)):
            build(t)
