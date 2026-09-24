"""The before/after picture for bungo's 06:1x ruling ("Both icons").

CONSTITUTION 5: same framing, before and after, taken by us. The three bars are
the SAME widget grabbed by the SAME gate, at 2:1 with nearest-neighbour
magnification (Qt::FastTransformation), so no resample invents a pixel:

  1  BEFORE   the rung exe 05:48:33 -- "pose" a word, the auto-key dot at r=4
  2  AFTER    both toggles off
  3  AFTER    both toggles on (the lit state)

The caption on each row carries the number the report quotes: the y centre of
the ink in that row's own toggle area, measured here with the same arithmetic as
measure_dot.py, so the picture and the report cannot disagree.

A second picture crops the three bars to the toggles themselves at 4:1.
"""
import os, sys
from PIL import Image, ImageDraw
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "images")

BEFORE = os.path.join(IMG, "before", "transport_2x.png")
AFTER_OFF = os.path.join(IMG, "after", "transport_2x_off.png")
AFTER_ON = os.path.join(IMG, "after", "transport_2x_on.png")

BG = (48, 50, 54)
INK = (230, 232, 235)
# the QSS ":checked" plate a lit toggle sits on (wwBoxedButtonQss -> bgBtnDown)
BTN_DOWN = (53, 95, 134)
# the plain button plate an unchecked toggle sits on (bgBtn)
BTN_PLAIN = (58, 61, 66)
CAPTION_H = 26
PAD = 10


def ink_runs(im):
    """Every lump of ink in the bar, as (x0, x1, ycentre)."""
    a = np.asarray(im.convert("RGB")).astype(int)
    h, w, _ = a.shape
    cols, counts = np.unique(a.reshape(-1, 3), axis=0, return_counts=True)
    bg = cols[counts.argmax()]
    ink = (np.abs(a - bg).sum(axis=2) > 60)
    colhas = ink.any(axis=0)
    runs = []
    x = 0
    while x < w:
        if colhas[x]:
            x0 = x
            x1 = x
            gap = 0
            while x < w and (colhas[x] or gap < 6):
                if colhas[x]:
                    gap = 0
                    x1 = x
                else:
                    gap += 1
                x += 1
            ys = np.where(ink[:, x0:x1 + 1].any(axis=1))[0]
            runs.append((x0, x1, (ys.min() + ys.max()) / 2.0))
        else:
            x += 1
    return runs


def glyph_line(im):
    """The y the first seven transport glyphs are centred on."""
    r = ink_runs(im)
    return sum(t[2] for t in r[:7]) / 7.0 if len(r) >= 7 else -1.0


def toggle_line(im, after_loop_x):
    """The y centre of everything to the right of the loop button and left of
    the Start field -- i.e. the two toggles."""
    r = [t for t in ink_runs(im) if t[0] > after_loop_x]
    return r[:2]


def lit_ink(im, tog, plate=BTN_DOWN):
    """The mean colour of what is DRAWN inside each checked button's box.

    The gate can pass on an icon the button never asks for (2026-09-12 06:35,
    the loop button), so the lit colour is read back off the picture: inside the
    toggle's own columns, every pixel far enough from the ":checked" plate is
    ink, and their mean is the colour the eye sees.
    """
    a = np.asarray(im.convert("RGB")).astype(int)
    out = []
    for x0, x1, _ in tog:
        sub = a[:, x0:x1 + 1]
        d = np.abs(sub - np.array(plate)).sum(axis=2)
        if not (d > 60).any():
            out.append("-")
            continue
        # the GLYPH CORE, not every pixel off the plate: a 16 px drawing is
        # mostly antialiased edge, and the mean over the edge is a blend of ink
        # and plate (that is why the accent build read #61584b, not #f0a54a).
        thr = max(150, int(np.percentile(d[d > 60], 90)))
        m = d >= thr
        if not m.any():
            m = d >= d.max() - 1
        out.append("#%02x%02x%02x" % tuple(int(round(v)) for v in sub[m].mean(axis=0)))
    return out


def stack(rows, out, title):
    w = max(im.width for im, _ in rows)
    h = sum(im.height + CAPTION_H for im, _ in rows) + PAD * 2 + CAPTION_H
    page = Image.new("RGB", (w + PAD * 2, h), BG)
    d = ImageDraw.Draw(page)
    d.text((PAD, PAD // 2), title, fill=INK)
    y = PAD + CAPTION_H
    for im, cap in rows:
        d.text((PAD, y), cap, fill=INK)
        page.paste(im, (PAD, y + CAPTION_H - 6))
        y += im.height + CAPTION_H
    page.save(out)
    return page.size


def main():
    for p in (BEFORE, AFTER_OFF, AFTER_ON):
        if not os.path.exists(p):
            sys.exit("missing %s -- the after pictures come from the built exe's gate run" % p)
    b = Image.open(BEFORE)
    ao = Image.open(AFTER_OFF)
    an = Image.open(AFTER_ON)

    lb, lo, ln = glyph_line(b), glyph_line(ao), glyph_line(an)
    # the loop button's right edge, so "the toggles" can be found without
    # hard-coding an x: the 8th lump of ink in the bar is the loop glyph
    def loop_right(im):
        r = ink_runs(im)
        return r[7][1] if len(r) > 7 else 500
    tb = toggle_line(b, loop_right(b))
    to = toggle_line(ao, loop_right(ao))
    tn = toggle_line(an, loop_right(an))

    def cap(name, line, toggles):
        bits = ", ".join("%.1f" % t[2] for t in toggles)
        return "%s  --  transport glyphs centred on y %.1f;  the two toggles' ink centred on y %s" % (name, line, bits)

    size = stack(
        [
            (b, cap("BEFORE  (rung exe 05:48:33)", lb, tb)),
            (ao, cap("AFTER   both toggles OFF", lo, to)),
            (an, cap("AFTER   both toggles ON (lit)", ln, tn)),
        ],
        os.path.join(IMG, "icons_before_after.png"),
        "bungo 06:1x \"Both icons\" + 07:3x \"keep them white\" -- the transport row at 2:1, nearest-neighbour, same widget, same gate",
    )
    print("icons_before_after.png %dx%d" % size)
    print("  glyph line: before %.1f  after-off %.1f  after-on %.1f" % (lb, lo, ln))
    print("  toggles   : before %s" % [("%d..%d" % (t[0], t[1]), round(t[2], 1)) for t in tb])
    print("            : after  %s" % [("%d..%d" % (t[0], t[1]), round(t[2], 1)) for t in to])

    # ---- the 4:1 crop of the toggles themselves
    def crop4(im, tog):
        if not tog:
            return None
        x0 = max(0, min(t[0] for t in tog) - 40)
        x1 = min(im.width, max(t[1] for t in tog) + 40)
        c = im.crop((x0, 0, x1, im.height))
        return c.resize((c.width * 2, c.height * 2), Image.NEAREST)

    litOn = lit_ink(an, tn)
    litOff = lit_ink(ao, to, BTN_PLAIN)
    print("  glyph core read back off the picture: ON %s (textBright #f2f3f5)  OFF %s (text #e6e8eb)" % (litOn, litOff))

    rows = []
    for name, im, tog in (("BEFORE  a word and a speck", b, tb),
                          ("AFTER   OFF", ao, to),
                          ("AFTER   ON  (lit: glyph core measures %s -- textBright #f2f3f5, NOT the accent)"
                           % " and ".join(litOn), an, tn)):
        c = crop4(im, tog)
        if c:
            rows.append((c, name))
    if rows:
        size = stack(rows, os.path.join(IMG, "icons_zoom_4x.png"),
                     "the two gizmo toggles at 4:1 (nearest neighbour) -- loop | pose | auto-key")
        print("icons_zoom_4x.png %dx%d" % size)


if __name__ == "__main__":
    main()
