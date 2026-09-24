"""IMPOSTOR16 N8 addendum -- captioned orbit GIFs from several run folders.

    python n8/gifs8.py <outdir> <gif name> <panel H> <caption>=<run dir> [...]

Each <run dir> holds v_az%03d_el00_{mesh,card}.png from orb.sh at 3-degree steps
(the lit look test, WW_IMPOSTOR_CHANNEL=20, light 45,45); "<run dir>:mesh" takes
that folder's MESH picture instead of its card. Each run's camera is at ortho
half-width 1.02 x max(set half) (aimCamera), read from the run's own log
("half: W x H"); every frame is rescaled about the frame centre to the N16 set's
356.831 so all panels share one scale. The scale is proved per panel: that run's
mesh picture, rescaled, against the N16 reference run's mesh picture (IoU).
Crop = union of every panel's coverage, bottom cut at the tightest-zoomed run's
frame edge so all panels end on one line. One 128-colour palette, unchanged
pixels written transparent (disposal 1), every frame read back and compared.
"""
import sys, os, re, numpy as np
from PIL import Image, ImageDraw, ImageFont

od, name, H = sys.argv[1], sys.argv[2], int(sys.argv[3])
specs = [a.split('=', 1) for a in sys.argv[4:]]
os.makedirs(od, exist_ok=True)
REF = 356.831
NCOL = int(os.environ.get('NCOL', '128'))
FONT_PATH = 'C:/Windows/Fonts/segoeuib.ttf'
STRIP = 44


def half(run):
    t = open(run.rstrip('/\\') + '.log', encoding='utf-8', errors='replace').read()
    m = re.search(r'half: ([\d.]+) x ([\d.]+)', t)
    return max(float(m.group(1)), float(m.group(2)))


panels = []
for cap, src in specs:
    kind = 'card'
    if src.endswith(':mesh'):
        src, kind = src[:-5], 'mesh'
    panels.append((cap, src, kind, half(src) / REF))

az = sorted(int(f[4:7]) for f in os.listdir(panels[0][1]) if f.endswith('_card.png'))


def load(src, a, k, s):
    im = Image.open(os.path.join(src, 'v_az%03d_el00_%s.png' % (a, k))).convert('RGB')
    if abs(s - 1) > 1e-4:
        bg = im.getpixel((0, 0)); w, h = im.size
        sm = im.resize((round(w * s), round(h * s)), Image.LANCZOS)
        can = Image.new('RGB', (w, h), bg)
        can.paste(sm, ((w - sm.size[0]) // 2, (h - sm.size[1]) // 2))
        im = can
    return np.asarray(im)


F = {(i, a): load(p[1], a, p[2], p[3]) for i, p in enumerate(panels) for a in az}
bg = F[(0, az[0])][0, 0]

# scale proof: each run's own mesh, rescaled, vs the reference-scale mesh
refmesh = [p for p in panels if abs(p[3] - 1) < 1e-4]
for i, p in enumerate(panels):
    if not refmesh or abs(p[3] - 1) < 1e-4:
        continue
    ious = []
    for a in az[::10]:
        m1 = (load(refmesh[0][1], a, 'mesh', 1.0) != bg).any(-1)
        m2 = (np.abs(load(p[1], a, 'mesh', p[3]).astype(int) - bg.astype(int)).max(-1) > 6)
        ious.append((m1 & m2).sum() / max(1, (m1 | m2).sum()))
    print('scale proof %-34s x%.4f  mesh IoU vs reference %.4f' % (p[0], p[3], np.mean(ious)))

shape = F[(0, az[0])].shape[:2]
cov = np.zeros(shape, bool)
for v in F.values():
    cov |= (np.abs(v.astype(int) - bg.astype(int)).max(-1) > 6)
ys, xs = np.nonzero(cov)
pad = 14
y0, y1 = max(0, ys.min() - pad), ys.max() + pad + 1
x0, x1 = max(0, xs.min() - pad), xs.max() + pad + 1
smin = min(p[3] for p in panels)
y1 = min(y1, shape[0] // 2 + int((shape[0] // 2 - 1) * smin) - 1)
print('crop box', x0, y0, x1, y1)


def panel(arr, caption):
    im = Image.fromarray(arr[y0:y1, x0:x1])
    s = H / im.size[1]
    im = im.resize((round(im.size[0] * s), H), Image.LANCZOS)
    out = Image.new('RGB', (im.size[0], H + STRIP), (18, 18, 20))
    out.paste(im, (0, STRIP))
    d = ImageDraw.Draw(out)
    sz = 26
    fnt = ImageFont.truetype(FONT_PATH, sz)
    while d.textlength(caption, font=fnt) > im.size[0] - 12 and sz > 12:
        sz -= 1
        fnt = ImageFont.truetype(FONT_PATH, sz)
    tw = d.textlength(caption, font=fnt)
    d.text(((im.size[0] - tw) / 2, 7 + (26 - sz) // 2), caption, fill=(236, 236, 236), font=fnt)
    return out, sz


frames = []
minsz = 99
for a in az:
    ps = []
    for i, p in enumerate(panels):
        pi, sz = panel(F[(i, a)], p[0]); ps.append(pi); minsz = min(minsz, sz)
    W = sum(q.size[0] for q in ps) + 6 * (len(ps) - 1)
    row = Image.new('RGB', (W, ps[0].size[1]), (18, 18, 20))
    x = 0
    for q in ps:
        row.paste(q, (x, 0)); x += q.size[0] + 6
    frames.append(row)
frames[az.index(30) if 30 in az else 0].save(os.path.join(od, os.path.splitext(name)[0] + '_az030.png'))

pick = frames[::max(1, len(frames) // 6)][:6]
mont = Image.new('RGB', (frames[0].size[0], frames[0].size[1] * len(pick)))
for i, f in enumerate(pick):
    mont.paste(f, (0, i * f.size[1]))
pal = mont.quantize(colors=NCOL, method=Image.MEDIANCUT)
q = [np.asarray(f.quantize(palette=pal, dither=Image.NONE)) for f in frames]
out = [q[0]]
for i in range(1, len(q)):
    d = q[i].copy(); d[q[i] == q[i - 1]] = 255; out.append(d)
pl = pal.getpalette()[:768]; pl += [0] * (768 - len(pl))
ims = []
for a in out:
    im = Image.fromarray(a.astype(np.uint8), 'P'); im.putpalette(pl); ims.append(im)
p = os.path.join(od, name)
ims[0].save(p, save_all=True, append_images=ims[1:], duration=40, loop=0, optimize=False,
            disposal=1, transparency=255)
rgbpal = np.array(pl, np.uint8).reshape(256, 3)
g = Image.open(p); bad = 0
for i in range(len(q)):
    g.seek(i); bad = max(bad, int((np.asarray(g.convert('RGB')) != rgbpal[q[i]]).any(-1).sum()))
print('%s %d frames %dx%d %.2f MB; smallest caption %d pt; read-back worst frame differs in %d px' % (
    p, len(q), ims[0].size[0], ims[0].size[1], os.path.getsize(p) / 1e6, minsz, bad))
