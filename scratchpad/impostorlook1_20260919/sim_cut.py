# IMPOSTORLOOK1 -- the SIMULATION for defect 1, and its known-answer control.
#
# THE CLAIM UNDER TEST: the sheet stores coverage as a FRACTION, the drawer
# tests it (`colour.a < alphaThreshold` -> discard) and then writes alpha 1.0,
# so a texel with 7 per cent coverage paints as solid as one with 100. On the
# fine-twig maple 94 per cent of the silhouette is partial coverage, so the
# crown paints as its own solid hull.
#
# HOW THIS IS NOT CIRCULAR. At a BAKE direction the blend weights are (1,0,0)
# and the parallax step is a provable no-op, so the card the viewer draws is
# exactly one frame of the sheet, thresholded. So:
#
#   1. Take the frame out of the sheet, threshold it the way the drawer does,
#      and FIT a scale+offset that puts it on the card grab's pixel grid.
#   2. The fit is scored against the REAL card grab. That is the known-answer
#      control: if the reproduction does not land near the grab, the model of
#      the draw is wrong and nothing below it may be believed. The brief's
#      pre-registered bar is blast N=4, single frame, its own bake direction.
#   3. Then apply the SAME transform to the SAME frame with coverage used as
#      OPACITY instead of as a test. That is the "after". Nothing is refitted.
#
# OFFLINE. No exe, no build.
import sys, os, glob, json
import numpy as np
from PIL import Image, ImageDraw, ImageFont
REPO = 'E:/Projects/NifskopeWildWastelandEdition'
SC = REPO + '/scratchpad'; MINE = SC + '/impostorlook1_20260919'
sys.path.insert(0, REPO + '/tests/spells')
import impostor_bc_decode as bc
BG = np.array([43, 45, 49], float)

def load(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(float)
    return a, (np.abs(a-BG).sum(-1) > 12)

def frame_of(tag, i, j):
    d = '%s/impostorfix5_20260919/fixture/%s/cards' % (SC, tag)
    raw = open(glob.glob(d+'/*_oct.lodm')[0], 'rb').read()
    m = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
    N = int(m['oct']); fl = m['coverage']['floor']/255.; ba = m['coverage']['base']/255.
    a, _, _ = bc.load_dds(glob.glob(d+'/*_oct_d.DDS')[0])
    H, W = a.shape[:2]; fw, fh = W//N, H//N
    f = a[j*fh:(j+1)*fh, i*fw:(i+1)*fw]
    al = f[..., 3]
    cov = np.where(al < ba, 0.0, np.clip(fl+(al-ba)*(1-fl)/(1-ba), fl, 1.0))
    return f[..., :3], cov, fl, (fw, fh)

def place(mask_or_val, box, shape, nearest=True):
    x0, y0, w, h = box
    im = Image.fromarray((np.clip(mask_or_val, 0, 1)*255).astype(np.uint8))
    im = im.resize((w, h), Image.NEAREST if nearest else Image.BILINEAR)
    out = np.zeros(shape, float)
    src = np.asarray(im).astype(float)/255.0
    sy0 = max(0, -y0); sx0 = max(0, -x0)
    dy0 = max(0, y0); dx0 = max(0, x0)
    hh = min(h-sy0, shape[0]-dy0); ww = min(w-sx0, shape[1]-dx0)
    if hh > 0 and ww > 0:
        out[dy0:dy0+hh, dx0:dx0+ww] = src[sy0:sy0+hh, sx0:sx0+ww]
    return out

def fit_box(binary_frame, card_mask):
    """The scale+offset is READ OFF THE CARD GRAB'S OWN INK BOX, not searched:
       the drawn card quad is the frame stretched over the quad, so the ink of
       the thresholded frame and the ink of the grab bound the same rectangle."""
    ys, xs = np.nonzero(card_mask)
    if len(ys) < 50: return None
    fy, fx = np.nonzero(binary_frame)
    if len(fy) < 10: return None
    # the frame's ink box in frame texels, mapped onto the grab's ink box
    fw = binary_frame.shape[1]; fh = binary_frame.shape[0]
    sx = (xs.max()-xs.min()+1)/float(fx.max()-fx.min()+1)
    sy = (ys.max()-ys.min()+1)/float(fy.max()-fy.min()+1)
    W = int(round(fw*sx)); H = int(round(fh*sy))
    X0 = int(round(xs.min()-fx.min()*sx)); Y0 = int(round(ys.min()-fy.min()*sy))
    return (X0, Y0, W, H)

def iou(a, b):
    u = (a | b).sum()
    return float((a & b).sum())/u if u else 0.0

def run(tag, i, j, az, el, folder, name, title):
    rgb, cov, fl, (fw, fh) = frame_of(tag, i, j)
    mi, mk = load('%s/v_az%03d_el%02d_mesh.png' % (folder, az, el))
    ci, ck = load('%s/v_az%03d_el%02d_card.png' % (folder, az, el))
    binf = (cov >= fl)
    box = fit_box(binf, ck)
    if box is None: return None
    B = place(binf.astype(float), box, ck.shape) > 0.5          # the draw, reproduced
    ctl = iou(B, ck)
    A = place(cov, box, ck.shape, nearest=True)                 # coverage as OPACITY
    col = np.stack([place(rgb[..., c], box, ck.shape) for c in range(3)], -1)*255.0
    after = col*A[..., None] + BG[None, None, :]*(1.0-A[..., None])
    before = ci
    # crop all three to the union, x2 nearest
    U = mk | ck | (A > 0.02)
    ys, xs = np.nonzero(U); y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    K = 2
    def pan(img):
        im = Image.fromarray(np.clip(img[y0:y1+1, x0:x1+1], 0, 255).astype(np.uint8))
        return im.resize((im.size[0]*K, im.size[1]*K), Image.NEAREST)
    ps = [pan(mi), pan(before), pan(after)]
    w, h = ps[0].size; cap, gap, mar = 74, 14, 16
    sheet = Image.new('RGB', (mar*2+3*w+2*gap, mar+cap+h+mar), (24, 25, 28))
    dr = ImageDraw.Draw(sheet)
    def F(s):
        for p in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
            if os.path.exists(p):
                try: return ImageFont.truetype(p, s)
                except Exception: pass
        return ImageFont.load_default()
    f1, f2, f3 = F(19), F(15), F(13)
    dr.text((mar, mar), title, font=f1, fill=(235, 235, 235))
    dr.text((mar, mar+23), 'SIMULATION. Right panel is not a render -- it is the same sheet frame with its coverage used as OPACITY', font=f3, fill=(215, 175, 120))
    dr.text((mar, mar+40), 'known-answer control: reproducing the SHIPPED draw from the sheet scores IoU %.4f against the real card grab' % ctl, font=f3, fill=(170, 172, 178))
    for k, (im, lab, cl) in enumerate(zip(ps, ('MESH (the model)', 'CARD as shipped (real grab)', 'SIMULATED: coverage as opacity'),
                                          ((150, 200, 150), (210, 195, 140), (150, 180, 220)))):
        sheet.paste(im, (mar+k*(w+gap), mar+cap)); dr.text((mar+k*(w+gap)+2, mar+cap-18), lab, font=f2, fill=cl)
    p = '%s/images/%s' % (MINE, name); sheet.save(p)
    print('%-9s control IoU(reproduced draw, real card grab) = %.4f   ink card %.4f  ink sim %.4f  ink mesh %.4f   -> %s'
          % (tag, ctl, ck.mean(), float(A.mean()), mk.mean(), name))
    return ctl

if __name__ == '__main__':
    B = SC+'/impostorfix5_20260919/control/blast_n4_bake_ctl'
    M = SC+'/impostorfix5_20260919/control/maple_n4_bake_on'
    # frame index 0 is (i=0,j=0) = az 180 el 0 for both N=4 subjects
    c1 = run('blast_n4', 0, 0, 180, 0, B, 'C_sim_cut_blast_n4.png',
             'blast_n4 -- the coverage cut, at a bake direction (the control subject)')
    c2 = run('maple_n4', 0, 0, 180, 0, M, 'C_sim_cut_maple_n4.png',
             'maple_n4 -- the coverage cut, at a bake direction (94% of this silhouette is partial coverage)')
