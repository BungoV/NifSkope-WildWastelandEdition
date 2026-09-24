# Lane SLAB1 -- the pictures (step 5, gate G6).
#
# Two kinds of page, and the rules of `ww-texel-picture` for both:
#
#   RENDER PAGES  before | after | the difference, from the SAME framing, with
#                 the measured pixel difference in every label. The viewer read
#                 the same .lodi/.lodl/.lodo in both arms and only the mask
#                 SHEETS differ, so anything visible is the sheet.
#   TEXEL PAGE    the mask sheet's B under the deck at 1 texel = 8 world units,
#                 nearest-neighbour magnified, with the grid drawn and the
#                 rectangle's own mean in the caption beside the whole-sheet
#                 mean the report gates on (skill section 7: print both, say
#                 which is the verdict).
#
# THE THIRD READING (skill section 6): a change of LAW wants the old artefact
# under the old law, the old artefact under the new law, and the new artefact.
# Here the first and second are the same file: `oldlaw/` (new exe, slab 0) is
# byte-identical to `before/` (the rung) on all ten output files, section 4.2.
# The page says so instead of drawing a third identical panel.
import os, sys
from PIL import Image, ImageDraw, ImageFont
import mask_b_mean as M

def _font(sz):
    # A page 4,240 px wide with 11-px labels is unreadable at 1:1; the labels
    # carry the numbers the report gates on, so they are sized to the page.
    for n in ('DejaVuSansMono.ttf', 'DejaVuSans.ttf', 'consola.ttf', 'cour.ttf'):
        try:
            return ImageFont.truetype(n, sz)
        except Exception:
            pass
    return ImageFont.load_default()

F_CAP = _font(20)
F_LAB = _font(26)

S = os.path.dirname(os.path.abspath(__file__))
IM = os.path.join(S, 'images')
PAD = 12
CAP = 34


def load(name):
    return Image.open(os.path.join(IM, name)).convert('RGB')


def diff_stats(a, b):
    """(% of pixels that differ, mean |difference| over the channels that did)."""
    pa, pb = a.load(), b.load()
    w, h = a.size
    n = 0
    s = 0
    for y in range(h):
        for x in range(w):
            p, q = pa[x, y], pb[x, y]
            if p != q:
                n += 1
                s += abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])
    tot = w * h
    return 100.0 * n / tot, (s / (3.0 * n) if n else 0.0)


def diff_image(a, b, gain=4):
    """|after - before| per channel, gain x, so a small move is visible."""
    out = Image.new('RGB', a.size)
    pa, pb, po = a.load(), b.load(), out.load()
    w, h = a.size
    for y in range(h):
        for x in range(w):
            p, q = pa[x, y], pb[x, y]
            po[x, y] = tuple(min(255, abs(p[i] - q[i]) * gain) for i in range(3))
    return out


def wrap(draw, text, width, font=None):
    words = text.split(' ')
    lines, cur = [], ''
    for wd in words:
        t = (cur + ' ' + wd).strip()
        if draw.textlength(t, font=font) <= width or not cur:
            cur = t
        else:
            lines.append(cur); cur = wd
    if cur:
        lines.append(cur)
    return lines


def page(tag, before, after, caption, out):
    a, b = load(before), load(after)
    assert a.size == b.size, (a.size, b.size)
    pct, mag = diff_stats(a, b)
    d = diff_image(a, b)
    labels = ['BEFORE (rung, max-Z reading)',
              'AFTER (slab law) -- %.2f %% of pixels differ' % pct,
              '|AFTER - BEFORE| x 4 -- mean move %.1f of 255 where it moved' % mag]
    w, h = a.size
    tmp = Image.new('RGB', (10, 10))
    dr0 = ImageDraw.Draw(tmp)
    cw = max(w, *[int(dr0.textlength(l, font=F_LAB)) for l in labels]) + PAD
    ch = h + CAP + PAD
    head = wrap(dr0, caption, 3 * cw - 2 * PAD, F_CAP)
    hh = PAD + len(head) * 26 + PAD
    pg = Image.new('RGB', (3 * cw + PAD, hh + ch + PAD), (24, 24, 24))
    dr = ImageDraw.Draw(pg)
    for i, ln in enumerate(head):
        dr.text((PAD, PAD + i * 26), ln, fill=(230, 230, 230), font=F_CAP)
    for i, (img, lab) in enumerate(zip((a, b, d), labels)):
        x = PAD + i * cw
        dr.text((x, hh), lab, fill=(255, 220, 120) if i == 1 else (200, 200, 200), font=F_LAB)
        pg.paste(img, (x, hh + CAP))
    pg.save(os.path.join(IM, out))
    print('%-22s %s  %.2f %% of pixels differ, mean move %.1f  -> %s'
          % (tag, a.size, pct, mag, out))
    return pct, mag


# ---------------------------------------------------------------- the texel page
def texel_rect(sheet, x0, y0, x1, y1):
    """The mask sheet's B over a world rectangle as rows of ints, north-up."""
    v, u = sheet.v, sheet.u
    cols = {}
    for (tx, ty), rows in sheet.tiles.items():
        cellX = v.west + tx * v.levelDim
        cellY = v.north - ty * v.levelDim
        for j in range(v.border, v.border + v.content):
            wy = (cellY + 1) * 4096.0 - (j - v.border + 0.5) * u
            if not (y0 <= wy < y1):
                continue
            row = rows[j]
            for i in range(v.border, v.border + v.content):
                wx = cellX * 4096.0 + (i - v.border + 0.5) * u
                if not (x0 <= wx < x1):
                    continue
                cols[(wx, wy)] = row[i]
    xs = sorted(set(k[0] for k in cols))
    ys = sorted(set(k[1] for k in cols), reverse=True)   # north first
    return xs, ys, [[cols[(x, y)] for x in xs] for y in ys]


def texel_page(out, x0, y0, x1, y1, whole_before, whole_after):
    sb = M.Sheet(os.path.join(S, 'before', 'vt', 'FO4CSLOD', 'Commonwealth',
                              'Commonwealth.VT.1.lodt'))
    sa = M.Sheet(os.path.join(S, 'after', 'vt', 'FO4CSLOD', 'Commonwealth',
                              'Commonwealth.VT.1.lodt'))
    xs, ys, A = texel_rect(sb, x0, y0, x1, y1)
    xs2, ys2, B = texel_rect(sa, x0, y0, x1, y1)
    assert xs == xs2 and ys == ys2, 'the two panels must be the SAME texels'
    w, h = len(xs), len(ys)
    mag = max(1, min(512 // w, 512 // h))
    ma = sum(sum(r) for r in A) / float(w * h)
    mb = sum(sum(r) for r in B) / float(w * h)

    def panel(grid):
        im = Image.new('RGB', (w * mag, h * mag))
        d = ImageDraw.Draw(im)
        for j in range(h):
            for i in range(w):
                b = grid[j][i]
                d.rectangle([i * mag, j * mag, (i + 1) * mag - 1, (j + 1) * mag - 1],
                            fill=(b, b, b))
        if mag >= 4:          # the texel grid, only where a texel is visible
            for i in range(w + 1):
                d.line([(i * mag, 0), (i * mag, h * mag)], fill=(0, 38, 86))
            for j in range(h + 1):
                d.line([(0, j * mag), (w * mag, j * mag)], fill=(0, 38, 86))
        return im

    PA, PB = panel(A), panel(B)
    PD = Image.new('RGB', (w * mag, h * mag))
    d = ImageDraw.Draw(PD)
    for j in range(h):
        for i in range(w):
            v = min(255, abs(B[j][i] - A[j][i]) * 2)
            d.rectangle([i * mag, j * mag, (i + 1) * mag - 1, (j + 1) * mag - 1],
                        fill=(v, v // 3, 0))
    tmp = Image.new('RGB', (10, 10)); dr0 = ImageDraw.Draw(tmp)
    ch = h * mag + CAP + PAD
    cap = ('MASK SHEET role 5, channel B = the far terrain sky AO, mip 0 of '
           'Commonwealth.VT.1.lodt, 1 texel = 8 world units, nearest-neighbour x%d, '
           'grid drawn. SAME %d x %d texels in both panels: world x %.0f..%.0f y %.0f..%.0f '
           '(under the elevated highway deck, whose lattice squares all hold max Z 2415.9). '
           'CROP mean %.2f -> %.2f of 255. THE VERDICT NUMBER IS THE WHOLE SHEET: %.3f -> %.3f '
           '(a crop has fewer texels and a coarser floor; B is BC1, so a single texel is one of '
           '32 steps). Third reading: the old artefact under the NEW law is the file oldlaw/, '
           'byte-identical to BEFORE on all ten output files (report 4.2), so it is not drawn twice.'
           % (mag, w, h, min(xs), max(xs), min(ys), max(ys), ma, mb,
              whole_before, whole_after))
    labs = ['BEFORE  B mean %.2f  (min %d max %d)' % (ma, min(min(r) for r in A), max(max(r) for r in A)),
            'AFTER  B mean %.2f  (min %d max %d)' % (mb, min(min(r) for r in B), max(max(r) for r in B)),
            '|move| x 2, orange  max %d' % max(abs(B[j][i] - A[j][i])
                                               for j in range(h) for i in range(w))]
    cw = max(w * mag, *[int(dr0.textlength(l, font=F_LAB)) for l in labs]) + PAD
    head = wrap(dr0, cap, 3 * cw - 2 * PAD, F_CAP)
    hh = PAD + len(head) * 26 + PAD
    pg = Image.new('RGB', (3 * cw + PAD, hh + ch + PAD), (24, 24, 24))
    dr = ImageDraw.Draw(pg)
    for i, ln in enumerate(head):
        dr.text((PAD, PAD + i * 26), ln, fill=(230, 230, 230), font=F_CAP)
    for i, (img, lab) in enumerate(zip((PA, PB, PD), labs)):
        x = PAD + i * cw
        dr.text((x, hh), lab, fill=(255, 220, 120) if i == 1 else (200, 200, 200), font=F_LAB)
        pg.paste(img, (x, hh + CAP))
    pg.save(os.path.join(IM, out))
    print('texel page %d x %d texels, x%d, crop %.2f -> %.2f  -> %s'
          % (w, h, mag, ma, mb, out))


def main():
    page('close 24900,-41300', 'flat_close_before.png', 'flat_close_after.png',
         'FRAMING close: WW_RENDER_CENTER 24900,-41300,450  ORTHO 2600  VIEW 8, '
         'WW_LODL_AO=1 WW_RENDER_FLAT=1, level 0, region cells [4,-12]..[7,-9]. '
         'The brief\'s coordinate. Viewer note line, BEFORE: "terrain AO from the MASK '
         'SHEET\'S B (the texture), 512 texels a cell, bilinear a vertex; 16 tiles read, '
         '257 vertices without a tile (drawn open); values 16..255, mean 134.1"; AFTER the '
         'same line reads "values 33..255, mean 158.8".',
         'page_close.png')
    page('full 24576,-40960', 'flat_full_before.png', 'flat_full_after.png',
         'FRAMING full: WW_RENDER_CENTER 24576,-40960,0  ORTHO 8192  VIEW 1 -- the whole '
         'baked region, cells [4,-12]..[7,-9]. The hotfix 7c logs do not record their camera '
         'arguments, so this framing is RECONSTRUCTED from the region itself (centre of '
         'x 16384..32768, y -49152..-32768; half-extent 8192) and its arguments are printed '
         'here rather than claimed to be the same ones.',
         'page_full.png')
    page('deck flat 20352,-41216', 'flat_deck_before.png', 'flat_deck_after.png',
         'FRAMING deck: WW_RENDER_CENTER 20352,-41216,700  ORTHO 2600  VIEW 8 -- the centre '
         'of the actual elevated-highway rectangle (x 19712..20992, y -41856..-40576), which '
         'is where the 57.3 -> 83.9 move is. WW_RENDER_FLAT=1, so the terrain carries the mask '
         'sheet\'s AO and nothing else.',
         'page_deck_flat.png')
    page('deck lit 20352,-41216', 'lit_deck_before.png', 'lit_deck_after.png',
         'FRAMING deck, LIT (WW_RENDER_FLAT=0): the same camera as the flat pair, with the '
         'objects and the diffuse drawn -- the picture as bungo sees it rather than as the '
         'mask alone.',
         'page_deck_lit.png')
    texel_page('page_texels_deck.png', 20224, -41344, 20480, -41088,
               134.689, 158.619)


if __name__ == '__main__':
    main()
