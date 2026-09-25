"""WHITE1 pictures, all in the frame of EXTENT1's whole_top_after.png (3200x3224, view 1, ortho half-width 400000,
look-at 0,0 -> upp 250, north up; read from pics/top/T.cam.log). Texel fields are sampled nearest into that frame.
usage: python pictures.py            (needs cache.py's .npy and, for the render panels, renders.sh's PNGs)"""
import os, numpy as np
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__)); os.chdir(HERE)
EXT = r'E:/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925/pics/'
Wd, Hd, UPP = 3200, 3224, 250.0
u = np.arange(Wd); v = np.arange(Hd)
x = (u + 0.5 - Wd / 2) * UPP; y = (Hd / 2 - (v + 0.5)) * UPP
col = np.floor(x / 128.0 + 3072).astype(int); row = np.floor(3072 - y / 128.0).astype(int)
okc = (col >= 0) & (col < 6144); okr = (row >= 0) & (row < 6144)
BG = np.array([38, 40, 46], np.uint8)
def frame(a):
    out = np.empty((Hd, Wd) + a.shape[2:], a.dtype); out[...] = BG if (a.ndim == 3 and a.shape[2] == 3) else 0
    rr, cc = np.meshgrid(row[okr], col[okc], indexing='ij')
    out[np.ix_(okr, okc)] = a[rr, cc]
    return out
try: font = ImageFont.truetype('arial.ttf', 22); big = ImageFont.truetype('arial.ttf', 30)
except Exception: font = big = ImageFont.load_default()

h = np.load('h.npy'); blk = np.load('blk.npy'); ring = np.load('ring.npy')
gy, gx = np.gradient(h, 128.0); slope = np.degrees(np.arctan(np.hypot(gx, gy)))
F = {k: np.load(k + '.npy') for k in ('on', 'off', 'van')}
Wl = np.array([0.2126, 0.7152, 0.0722], np.float32)
fr = {k: frame(a) for k, a in F.items()}
for k, a in fr.items(): Image.fromarray(a).save('pics/tex_%s_frame.png' % k)

# slope map (grey = degrees/90) and the pale mask over the western block (same definition as localise.py)
lum = F['on'].astype(np.float32) @ Wl; chn = F['on'].max(-1).astype(np.int16) - F['on'].min(-1)
pale = blk & (lum >= np.percentile(lum[blk], 85)) & (chn <= np.median(chn[blk]))
steep = blk & (slope > 30)
ov = np.zeros((6144, 6144, 3), np.uint8); ov[blk] = 40
ov[steep] = (40, 90, 200); ov[pale & ~steep] = (230, 60, 50); ov[pale & steep] = (250, 250, 250)
fr['ov'] = frame(ov)
sl = (np.clip(slope / 90.0, 0, 1) * 255).astype(np.uint8); fr['slope'] = frame(np.repeat(sl[..., None], 3, -1))

# renders (if present)
R = {}
for name, p in (('render ON (whole_top_after.png, his picture)', EXT + 'whole_top_after.png'),
                ('render fill OFF (T_off.png)', 'pics/T_off.png'),
                ('render ON unlit base colour (T_on_c12.png)', 'pics/T_on_c12.png')):
    if os.path.exists(p):
        im = np.asarray(Image.open(p).convert('RGB'))
        if im.shape[0] == 3248: im = im[12:3236]      # same look-at and upp; 3248 rows = 12 more above and below (T_on rows 12..3235 == EXTENT1 T.png, 0 px differ)
        assert im.shape[:2] == (Hd, Wd), (p, im.shape); R[name] = im
# C5: the band in the rendered pixels vs in the texels (ring = outer 4 cells of the western block, core = >8 cells in)
rf, cf = frame(ring.astype(np.uint8)[..., None])[..., 0] > 0, None
cf = frame((blk & ~ring).astype(np.uint8)[..., None])[..., 0] > 0
import localise_masks
core_f = frame(localise_masks.core().astype(np.uint8)[..., None])[..., 0] > 0
for name, im in list(R.items()) + [('texels ON in frame', fr['on']), ('texels OFF in frame', fr['off']), ('texels VANILLA in frame', fr['van'])]:
    L_ = im.astype(np.float32) @ Wl
    print('C5 %-48s lum ring %6.1f core %6.1f step %+6.1f' % (name, L_[rf].mean(), L_[core_f].mean(), L_[rf].mean() - L_[core_f].mean()))

def page(panels, box, zoom, out, title):
    x0, y0, x1, y1 = box; w, hh = (x1 - x0) * zoom, (y1 - y0) * zoom
    cols = 3; rows = (len(panels) + cols - 1) // cols
    P = Image.new('RGB', (cols * (w + 10) + 10, rows * (hh + 44) + 60), (20, 20, 24)); d = ImageDraw.Draw(P)
    d.text((10, 12), title, fill=(235, 235, 235), font=big)
    for i, (cap, a) in enumerate(panels):
        c = a[y0:y1, x0:x1]; im = Image.fromarray(c).resize((w, hh), Image.NEAREST)
        px, py = 10 + (i % cols) * (w + 10), 60 + (i // cols) * (hh + 44)
        P.paste(im, (px, py + 34)); d.text((px, py + 6), cap, fill=(235, 235, 235), font=font)
    P.save(out); print('wrote', out, P.size)

# frame pixel box of the NW corner of the land block: cells x -78..-68, y 68..78
def px_of(cx, cy): return int(round(Wd / 2 + cx * 4096 / UPP)), int(round(Hd / 2 - cy * 4096 / UPP))
(a0, b0), (a1, b1) = px_of(-79, 79), px_of(-67, 67)
pan = [(k, a) for k, a in R.items()] + [('vanilla dim-4 LOD diffuse texels', fr['van']),
       ('installed VT.16 texels (fill ON)', fr['on']), ('fill-OFF VT.16 texels', fr['off']),
       ('.lodl slope, black 0 / white 90 deg', fr['slope']),
       ('pale (red) / slope>30 (blue) / both (white)', fr['ov'])]
page(pan, (a0, b0, a1, b1), 4, 'pics/band_nw_zoom4x.png',
     'NW corner of the western block, cells x -79..-67, y 67..79, frame pixels %d..%d x %d..%d, nearest x4' % (a0, a1, b0, b1))
(a0, b0), (a1, b1) = px_of(-79, -66), px_of(-67, -78)
page(pan, (a0, b0, a1, b1), 4, 'pics/band_sw_zoom4x.png',
     'SW corner of the western block, cells x -79..-67, y -78..-66, frame pixels %d..%d x %d..%d, nearest x4' % (a0, a1, b0, b1))
# whole-map page, 1/3 scale
whole = [(k, a) for k, a in R.items()] + [('vanilla dim-4 LOD diffuse texels', fr['van']),
         ('installed VT.16 texels (fill ON)', fr['on']), ('fill-OFF VT.16 texels', fr['off']),
         ('.lodl slope', fr['slope']), ('pale / steep>30 / both', fr['ov'])]
cols = 3; S = 3; w, hh = Wd // S, Hd // S
P = Image.new('RGB', (cols * (w + 10) + 10, ((len(whole) + 2) // 3) * (hh + 44) + 60), (20, 20, 24)); d = ImageDraw.Draw(P)
d.text((10, 12), 'Whole map, the frame of whole_top_after.png (upp 250, cells -96..95), 1/3 scale', fill=(235, 235, 235), font=big)
for i, (cap, a) in enumerate(whole):
    px, py = 10 + (i % cols) * (w + 10), 60 + (i // cols) * (hh + 44)
    P.paste(Image.fromarray(a).resize((w, hh), Image.BOX), (px, py + 34)); d.text((px, py + 6), cap, fill=(235, 235, 235), font=font)
P.save('pics/whole_compare.png'); print('wrote pics/whole_compare.png', P.size)
