# IMPOSTORLIGHT1 -- montage the pics.sh grabs.
#   python compose.py            -> pics/headline_before_after.png, pics/sidelight_<subj>.png
# Every tile is cropped to the UNION bounding box of the non-background pixels
# of the images in its group (so mesh / before / after share one crop and one
# scale), padded, and scaled to a common height.
import glob, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

BG = np.array([43, 45, 49])
P = 'pics'
SUBJ = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')
NAME = {'blast_n4': 'blasted maple N4', 'blast_n8': 'blasted maple N8', 'maple_n4': 'forest maple N4',
        'dead_n4': 'destroyed tree N4', 'rock_n4': 'cliff rock N4'}
H = 360
try:
    FONT = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
    SMALL = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)
except OSError:
    FONT = SMALL = ImageFont.load_default()


def one(pattern):
    g = sorted(glob.glob(pattern))
    return Image.open(g[0]).convert('RGB') if g else None


def bbox(imgs, pad=12):
    m = None
    for im in imgs:
        k = (np.abs(np.asarray(im).astype(int) - BG) > 12).any(-1)
        m = k if m is None else (m | k)
    ys, xs = np.nonzero(m)
    if not len(xs):
        return (0, 0) + imgs[0].size
    w, h = imgs[0].size
    return (max(0, xs.min() - pad), max(0, ys.min() - pad), min(w, xs.max() + pad + 1), min(h, ys.max() + pad + 1))


def tiles(imgs):
    b = bbox(imgs)
    out = []
    for im in imgs:
        c = im.crop(b)
        out.append(c.resize((max(1, round(c.width * H / c.height)), H), Image.LANCZOS))
    return out


def label(im, text, font=FONT):
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, im.width, 28), fill=(20, 21, 24))
    d.text((8, 3), text, fill=(235, 235, 235), font=font)
    return im


def grid(rows, head, path):
    # rows: list of (row label, [tile images]); head: column labels
    cw = [max(r[1][i].width for r in rows) for i in range(len(head))]
    lw = 190
    W = lw + sum(cw) + 6 * len(cw)
    Ht = 34 + len(rows) * (H + 6)
    out = Image.new('RGB', (W, Ht), (20, 21, 24))
    d = ImageDraw.Draw(out)
    x = lw
    for i, h in enumerate(head):
        d.text((x + 6, 6), h, fill=(235, 235, 235), font=FONT); x += cw[i] + 6
    y = 34
    for name, ts in rows:
        d.text((8, y + H // 2 - 12), name, fill=(235, 235, 235), font=SMALL)
        x = lw
        for i, t in enumerate(ts):
            out.paste(t, (x + (cw[i] - t.width) // 2, y)); x += cw[i] + 6
        y += H + 6
    out.save(path)
    print('wrote', path, out.size)


rows = []
for t in SUBJ:
    m = one('%s/head_after/%s/*_mesh.png' % (P, t))
    b = one('%s/head_before/%s/*_card.png' % (P, t))
    a = one('%s/head_after/%s/*_card.png' % (P, t))
    if m is None or b is None or a is None:
        print('missing', t); continue
    rows.append((NAME[t], tiles([m, b, a])))
grid(rows, ['mesh', 'card BEFORE (bf6aa749)', 'card AFTER (this lane)'], '%s/headline_before_after.png' % P)

for t in ('blast_n4', 'dead_n4', 'rock_n4'):
    angles = (0, 90, 180, 270)
    ims = {}
    for pl in angles:
        ims[('mesh', pl)] = one('%s/side_after_p%d/%s/*_mesh.png' % (P, pl, t))
        ims[('before', pl)] = one('%s/side_before_p%d/%s/*_card.png' % (P, pl, t))
        ims[('after', pl)] = one('%s/side_after_p%d/%s/*_card.png' % (P, pl, t))
    if any(v is None for v in ims.values()):
        print('missing side', t); continue
    allt = tiles(list(ims.values()))
    tl = dict(zip(ims.keys(), allt))
    rws = [(r, [tl[(k, pl)] for pl in angles]) for r, k in
           (('mesh', 'mesh'), ('card BEFORE', 'before'), ('card AFTER', 'after'))]
    grid(rws, ['light planar %d deg' % pl for pl in angles], '%s/sidelight_%s.png' % (P, t))
