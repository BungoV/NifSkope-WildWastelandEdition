"""IMPOSTOR16 job 3 -- the orbit GIFs, captions burned in.

    python gifs.py <lit root> <outdir>

<lit root>/<tag>/v_az%03d_el00_{mesh,card}.png, 2-degree steps, el 0, one
world-fixed light (WW_IMPOSTOR_LIGHT=45,45), card drawn with the full material
(WW_IMPOSTOR_CHANNEL=20 in run_mat). The N4 run's camera is at ortho half-width
1.02 x 345.305, the N16 runs' at 1.02 x 356.831, so N4 frames are scaled by
345.305/356.831 about the frame centre (the look-at, = the set centre, the
same point for all three sets) to put every panel at one scale. The scale is
PROVED, not assumed: the N4 run's own mesh picture, rescaled, is compared to the
N16 run's mesh picture (IoU printed).
"""
import sys, os, numpy as np
from PIL import Image, ImageDraw, ImageFont

root, od = sys.argv[1], sys.argv[2]
os.makedirs(od, exist_ok=True)
S4 = 345.305 / 356.831
FPS_MS = 40                     # 25 fps
NCOL = int(os.environ.get("NCOL", "254"))
try:
    FONT = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
except Exception:
    FONT = ImageFont.load_default()


def load(tag, a, k):
    im = Image.open(os.path.join(root, tag, 'v_az%03d_el00_%s.png' % (a, k))).convert('RGB')
    if tag == 'n4_512':
        bg = im.getpixel((0, 0))
        w, h = im.size
        sm = im.resize((round(w * S4), round(h * S4)), Image.LANCZOS)
        can = Image.new('RGB', (w, h), bg)
        can.paste(sm, ((w - sm.size[0]) // 2, (h - sm.size[1]) // 2))
        im = can
    return np.asarray(im)


az = sorted(int(f[4:7]) for f in os.listdir(os.path.join(root, 'n16_2k')) if f.endswith('_card.png'))
tags = ['n16_2k', 'n4_512']
F = {(t, a, k): load(t, a, k) for t in tags for a in az for k in ('mesh', 'card')}
bg = F[('n16_2k', 0, 'mesh')][0, 0]

# the scale proof: N4-run mesh (rescaled) vs N16-run mesh, same view
ious = []
for a in az:
    m1 = (F[('n16_2k', a, 'mesh')] != bg).any(-1)
    m2 = (np.abs(F[('n4_512', a, 'mesh')].astype(int) - bg.astype(int)).max(-1) > 6)
    ious.append((m1 & m2).sum() / max(1, (m1 | m2).sum()))
print('scale proof: N4-run mesh rescaled vs N16-run mesh IoU mean %.4f min %.4f' % (np.mean(ious), np.min(ious)))

cov = np.zeros(bg.shape if False else F[('n16_2k', 0, 'mesh')].shape[:2], bool)
for v in F.values():
    cov |= (np.abs(v.astype(int) - bg.astype(int)).max(-1) > 6)
ys, xs = np.nonzero(cov)
pad = 14
y0, y1 = max(0, ys.min() - pad), ys.max() + pad + 1
x0, x1 = max(0, xs.min() - pad), xs.max() + pad + 1
# the tree runs off the bottom of the 1000 px frame in every run (the trunk base);
# the N4 frames are shrunk about the centre, so their cut sits higher -- crop every
# panel at the N4 cut so all three end on the same line
y1 = min(y1, 500 + int(499 * S4) - 1)
print('crop box', x0, y0, x1, y1)
STRIP = 44


def panel(arr, caption, H):
    im = Image.fromarray(arr[y0:y1, x0:x1])
    s = H / im.size[1]
    im = im.resize((round(im.size[0] * s), H), Image.LANCZOS)
    out = Image.new('RGB', (im.size[0], H + STRIP), (18, 18, 20))
    out.paste(im, (0, STRIP))
    d = ImageDraw.Draw(out)
    fnt, sz = FONT, 26
    while d.textlength(caption, font=fnt) > im.size[0] - 12 and sz > 12:
        sz -= 1
        fnt = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', sz)
    tw = d.textlength(caption, font=fnt)
    d.text(((im.size[0] - tw) / 2, 7 + (26 - sz) // 2), caption, fill=(236, 236, 236), font=fnt)
    return out


def row(panels):
    W = sum(p.size[0] for p in panels) + 6 * (len(panels) - 1)
    out = Image.new('RGB', (W, panels[0].size[1]), (18, 18, 20))
    x = 0
    for p in panels:
        out.paste(p, (x, 0)); x += p.size[0] + 6
    return out


def gif(name, frames):
    # one palette for the whole loop (built from a spread of frames), so the
    # colours do not re-quantise per frame and shimmer
    pick = frames[::max(1, len(frames) // 6)][:6]
    mont = Image.new('RGB', (frames[0].size[0], frames[0].size[1] * len(pick)))
    for i, f in enumerate(pick):
        mont.paste(f, (0, i * f.size[1]))
    pal = mont.quantize(colors=NCOL, method=Image.MEDIANCUT)
    q = [np.asarray(f.quantize(palette=pal, dither=Image.NONE)) for f in frames]
    # a pixel that keeps its palette index from the previous frame is written as
    # the transparent index (255, unused by the palette) over the kept frame
    # (disposal 1): same picture on screen, long runs for LZW
    out = [q[0]]
    for i in range(1, len(q)):
        d = q[i].copy(); d[q[i] == q[i - 1]] = 255; out.append(d)
    pl = pal.getpalette()[:768]; pl += [0] * (768 - len(pl))
    ims = []
    for a in out:
        im = Image.fromarray(a.astype(np.uint8), 'P'); im.putpalette(pl); ims.append(im)
    p = os.path.join(od, name)
    ims[0].save(p, save_all=True, append_images=ims[1:], duration=FPS_MS, loop=0, optimize=False,
                disposal=1, transparency=255)
    # read back: every decoded frame must equal the quantised source frame
    rgbpal = np.array(pl, np.uint8).reshape(256, 3)
    g = Image.open(p); bad = 0; n = 0
    for i in range(len(q)):
        g.seek(i); got = np.asarray(g.convert('RGB'))
        bad = max(bad, int((got != rgbpal[q[i]]).any(-1).sum())); n += 1
    print('%s %d frames %dx%d %.2f MB; read-back %d frames, worst frame differs in %d px' % (
        p, len(q), ims[0].size[0], ims[0].size[1], os.path.getsize(p) / 1e6, n, bad))


H1 = int(sys.argv[3]) if len(sys.argv) > 3 else 640
H2 = int(sys.argv[4]) if len(sys.argv) > 4 else 520
H3 = int(sys.argv[5]) if len(sys.argv) > 5 else 440
gif('maple_impostor_16x16_2k_orbit.gif',
    [panel(F[('n16_2k', a, 'card')], 'Octahedral impostor 16x16 (2k)', H1) for a in az])
gif('maple_3dmodel_vs_impostor_16x16_2k.gif',
    [row([panel(F[('n16_2k', a, 'mesh')], '3D model', H2),
          panel(F[('n16_2k', a, 'card')], 'Octahedral impostor 16x16 (2k)', H2)]) for a in az])
gif('maple_3dmodel_vs_4x4_512_vs_16x16_2k.gif',
    [row([panel(F[('n16_2k', a, 'mesh')], '3D model', H3),
          panel(F[('n4_512', a, 'card')], 'Octahedral impostor 4x4 (512)', H3),
          panel(F[('n16_2k', a, 'card')], 'Octahedral impostor 16x16 (2k)', H3)]) for a in az])
# one still of the three-panel GIF at az 30 (between N4 frames) for a quick look
row([panel(F[('n16_2k', 30, 'mesh')], '3D model', 520),
     panel(F[('n4_512', 30, 'card')], 'Octahedral impostor 4x4 (512)', 520),
     panel(F[('n16_2k', 30, 'card')], 'Octahedral impostor 16x16 (2k)', 520)]).save(os.path.join(od, 'still_az030.png'))
