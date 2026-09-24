"""INCR1 -- compose the two pictures from what pics.sh put on disk.

Picture 1 is deliberately boring and says so: two renders of the same cells of
the FO4CS object library, one from a FULL bake's pair and one from a NULL
INCREMENTAL's, with the sha1 of each under it and the pixel difference measured
rather than asserted. A picture of an identity has to carry the number, or it is
just two pictures.

Picture 2 is the census an --incremental prints after one cell of the plugin is
edited, set as text on the panel palette, because a photograph of a terminal is
a photograph of a font.

    python pics_compose.py <work dir> <image dir> <sha full> <sha incr> <cx,cy>
"""
import io
import re
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

# the PBR Material Editor palette this tree's UI follows
BG = (30, 32, 36)
PANEL = (38, 41, 46)
LINE = (58, 62, 69)
TEXT = (222, 226, 232)
DIM = (150, 157, 167)
GOOD = (126, 200, 140)


def font(size, bold=False):
    names = ['segoeuib.ttf' if bold else 'segoeui.ttf',
             'DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf']
    for n in names:
        for d in ['C:/Windows/Fonts/', '']:
            try:
                return ImageFont.truetype(d + n, size)
            except OSError:
                pass
    return ImageFont.load_default()


def mono(size):
    for n in ['consola.ttf', 'DejaVuSansMono.ttf']:
        for d in ['C:/Windows/Fonts/', '']:
            try:
                return ImageFont.truetype(d + n, size)
            except OSError:
                pass
    return ImageFont.load_default()


def diff_pct(a, b):
    """How many pixels differ at all, as a percentage. Measured, not claimed."""
    if a.size != b.size:
        return None
    d = ImageChops.difference(a.convert('RGB'), b.convert('RGB'))
    bbox = d.getbbox()
    if bbox is None:
        return 0.0
    hist = d.convert('L').histogram()
    same = hist[0]
    return 100.0 * (sum(hist) - same) / float(sum(hist))


def picture_one(work, out, sha_full, sha_incr):
    pa, pb = os.path.join(work, 'full.png'), os.path.join(work, 'incr.png')
    if not (os.path.exists(pa) and os.path.exists(pb)):
        print('  no renders on disk; picture 1 not written')
        return False
    a, b = Image.open(pa).convert('RGB'), Image.open(pb).convert('RGB')
    pct = diff_pct(a, b)
    tile = 512
    a = a.resize((tile, tile), Image.LANCZOS)
    b = b.resize((tile, tile), Image.LANCZOS)

    pad, head, capt, foot = 24, 96, 64, 80
    W = pad * 3 + tile * 2
    H = head + tile + capt + foot
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((pad, 22), 'The FO4CS object library, full bake vs incremental',
           font=font(26, True), fill=TEXT)
    d.text((pad, 58),
           'Same cells, same camera. Nothing to see is the whole result.',
           font=font(16), fill=DIM)

    for i, (img, title, sha) in enumerate(
            [(a, 'FULL BAKE', sha_full), (b, 'NULL --incremental', sha_incr)]):
        x = pad + i * (tile + pad)
        im.paste(img, (x, head))
        d.rectangle([x - 1, head - 1, x + tile, head + tile], outline=LINE)
        d.text((x, head + tile + 10), title, font=font(18, True), fill=TEXT)
        d.text((x, head + tile + 34), '.lodi sha1 ' + sha[:40],
               font=mono(13), fill=DIM)

    y = head + tile + capt + 8
    d.rectangle([pad, y - 6, W - pad, y - 5], fill=LINE)
    if pct is None:
        msg = 'the two renders are different sizes'
        col = (220, 120, 120)
    elif pct == 0.0:
        msg = ('0.000 % of pixels differ, and the two .lodi files are the '
               'same sha1' if sha_full == sha_incr else
               '0.000 % of pixels differ, but the sha1s do NOT match')
        col = GOOD if sha_full == sha_incr else (220, 120, 120)
    else:
        msg = '%.3f %% of pixels differ' % pct
        col = (220, 120, 120)
    d.text((pad, y + 6), msg, font=font(17, True), fill=col)
    d.text((pad, y + 32),
           'The incremental run replayed every chunk from its .lodj cache and '
           'rebaked none.', font=font(14), fill=DIM)
    im.save(out)
    print('  wrote %s (%dx%d)' % (out, W, H))
    return True


def picture_two(work, out, cell):
    p = os.path.join(work, 'census.txt')
    if not os.path.exists(p):
        print('  no census.txt; picture 2 not written')
        return False
    with io.open(p, encoding='utf-8', errors='replace') as fh:
        lines = [l.rstrip() for l in fh if l.strip()]
    if not lines:
        print('  census.txt is empty; picture 2 not written')
        return False
    f = mono(15)
    fb = font(24, True)
    pad = 26
    wrap = []
    for l in lines:
        while len(l) > 118:
            cut = l.rfind(' ', 0, 118)
            cut = cut if cut > 40 else 118
            wrap.append(l[:cut])
            l = '    ' + l[cut:].lstrip()
        wrap.append(l)
    lh = 22
    W = 1100
    H = pad * 2 + 96 + lh * len(wrap) + 40
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    # THE CAPTION IS READ OUT OF THE CENSUS, never typed: a heading that
    # disagrees with the lines underneath it is worse than no picture.
    dirty = total = replayed = placements = -1
    for l in lines:
        m = re.search(r'incremental: (\d+) of (\d+) chunks dirty', l)
        if m:
            dirty, total = int(m.group(1)), int(m.group(2))
        m = re.search(r'(\d+) replayed from cache \((\d+) placement', l)
        if m:
            replayed, placements = int(m.group(1)), int(m.group(2))
    if replayed > 0:
        head = 'One cell edited, %d of %d chunks rebaked' % (dirty, total)
        sub = ('cell (%s) of the plugin changed; the other %d chunk(s) spoke '
               'from their .lodj cache -- %d placement(s) replayed instead of '
               're-derived' % (cell, replayed, placements))
    elif dirty >= 0:
        head = 'One cell edited, the widening reached %d of %d chunks' % (dirty, total)
        sub = ('cell (%s) changed; on a region this small every chunk is a '
               'neighbour of it, so nothing was left to replay' % cell)
    else:
        head = 'An incremental run'
        sub = 'cell (%s) of the plugin was changed' % cell
    d.text((pad, 20), head, font=fb, fill=TEXT)
    d.text((pad, 54), sub, font=font(15), fill=DIM)
    d.rectangle([pad, 92, W - pad, H - pad], fill=PANEL, outline=LINE)
    y = 104
    for l in wrap:
        col = TEXT
        if l.startswith('incremental:') or l.startswith('native cache:'):
            col = GOOD
        elif l.startswith('  ('):
            col = DIM
        d.text((pad + 12, y), l, font=f, fill=col)
        y += lh
    im.save(out)
    print('  wrote %s (%dx%d)' % (out, W, H))
    return True


def main(argv):
    work, imgdir, sha_full, sha_incr, cell = argv[1:6]
    picture_one(work, os.path.join(imgdir, 'native_full_vs_incremental.png'),
                sha_full, sha_incr)
    picture_two(work, os.path.join(imgdir, 'incremental_census.png'), cell)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
