"""The handoff contact sheet: every delivered picture as a captioned thumbnail.

  python contact_sheet.py <out.png> <cols> <thumbw> <png>|<caption> ...

Each argument after the width is `path|caption`. A caption wraps inside its own
column, so nothing is written outside the frame, and every thumbnail keeps its
aspect (letterboxed on the sheet's ground, never stretched). The file name is
printed under the caption, because the sheet is what the FO4CS handoff quotes
paths from.
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

BG = (18, 18, 20)
CELL = (28, 28, 32)
FG = (238, 238, 238)
MUTED = (150, 150, 155)
PATHC = (120, 170, 210)
PAD = 18
GAP = 16


def font(px, bold=False):
    for name in (('segoeuib.ttf', 'arialbd.ttf') if bold else ('segoeui.ttf', 'arial.ttf')):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def wrap(text, f, width):
    lines, cur = [], ''
    for word in text.split(' '):
        t = word if not cur else cur + ' ' + word
        if f.getlength(t) <= width or not cur:
            cur = t
        else:
            lines.append(cur); cur = word
    lines.append(cur)
    return lines


def main():
    outp, cols, tw = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    items = []
    for a in sys.argv[4:]:
        path, cap = a.split('|', 1)
        items.append((path, cap))

    fT, fC, fP = font(30, True), font(16), font(14)
    thumbs = []
    for path, cap in items:
        im = Image.open(path).convert('RGB')
        th = max(1, round(im.height * tw / im.width))
        thumbs.append((im.resize((tw, th), Image.LANCZOS), cap, os.path.basename(path)))

    # rows are as tall as their tallest thumbnail plus their tallest caption
    rows = []
    for r in range(0, len(thumbs), cols):
        chunk = thumbs[r:r + cols]
        ih = max(t[0].height for t in chunk)
        ch = max(len(wrap(t[1], fC, tw)) for t in chunk) * 20 + 22
        rows.append((chunk, ih, ch))

    hdr = 96
    W = PAD * 2 + cols * tw + (cols - 1) * GAP
    H = PAD * 2 + hdr + sum(ih + ch + GAP for _, ih, ch in rows)
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    d.text((PAD, PAD), 'NifSkope Wild Wasteland — what the LOD generator makes today',
           font=fT, fill=FG)
    for i, ln in enumerate(wrap(
            'Terrain chunks with geometry at all four far levels, object chunks at '
            'ring 0 and a far ring, and the octahedral impostor cards. Vanilla left / '
            'ours right wherever vanilla ships the file. Exe release/NifSkope.exe '
            '2026-09-09 19:35:14; every picture regenerates from a command in '
            'scratchpad/lane_images_handoff_report.md.', fP, W - PAD * 2)):
        d.text((PAD, PAD + 40 + i * 19), ln, font=fP, fill=MUTED)

    y = PAD + hdr
    for chunk, ih, ch in rows:
        x = PAD
        for im, cap, name in chunk:
            d.rectangle([x, y, x + tw - 1, y + ih - 1], fill=CELL)
            img.paste(im, (x, y + (ih - im.height) // 2))
            cy = y + ih + 4
            for ln in wrap(cap, fC, tw):
                d.text((x, cy), ln, font=fC, fill=FG); cy += 20
            d.text((x, cy), name, font=fP, fill=PATHC)
            x += tw + GAP
        y += ih + ch + GAP
    img.save(outp, optimize=True)
    print('%s  %dx%d  tiles %d' % (outp, W, H, len(thumbs)))


if __name__ == '__main__':
    main()
