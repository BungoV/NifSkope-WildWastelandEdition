"""Ruling 07:3x, the picture-level proof: what colour is the LIT glyph really?

The mean over "everything that is not the :checked plate" is a blend -- a 16 px
glyph is mostly antialiased edge over a blue plate, which is why the accent
build measured #61584b and not #f0a54a.  So take the glyph's CORE as well: the
pixels furthest from the plate.  White ink gives a core near textBright
(#f2f3f5); accent ink gave a core near accent (#f0a54a).

Run:  py scratchpad/uinotes2_20260912/measure_lit_core.py
"""
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
AFT = os.path.join(HERE, "images", "after")
BTN_DOWN = np.array([53, 95, 134])    # bgBtnDown, the :checked plate
BTN_PLAIN = np.array([58, 61, 66])    # bgBtn, the unchecked plate


def runs(a):
    """the two toggles: the last two lumps of ink in the bar, by column."""
    h, w, _ = a.shape
    cols, counts = np.unique(a.reshape(-1, 3), axis=0, return_counts=True)
    bg = cols[counts.argmax()]
    ink = (np.abs(a - bg).sum(axis=2) > 60)
    colhas = ink.any(axis=0)
    out, x = [], 0
    while x < w:
        if colhas[x]:
            x0 = x1 = x
            gap = 0
            while x < w and (colhas[x] or gap < 6):
                if colhas[x]:
                    gap, x1 = 0, x
                else:
                    gap += 1
                x += 1
            out.append((x0, x1))
        else:
            x += 1
    return out


def core(sub, plate):
    d = np.abs(sub - plate).sum(axis=2)
    if not (d > 60).any():
        return "-", 0
    thr = max(150, int(np.percentile(d[d > 60], 90)))
    m = d >= thr
    if not m.any():
        m = d >= d.max() - 1
    return "#%02x%02x%02x" % tuple(int(round(v)) for v in sub[m].mean(axis=0)), int(m.sum())


for name, plate in (("transport_2x_on.png", BTN_DOWN), ("transport_2x_off.png", BTN_PLAIN)):
    p = os.path.join(AFT, name)
    a = np.asarray(Image.open(p).convert("RGB")).astype(int)
    r = runs(a)
    # the same identification compose_icons.py uses: seven transport glyphs,
    # then loop, then the two toggles
    loopRight = r[7][1]
    tog = [t for t in r if t[0] > loopRight][:2]
    print(name)
    for label, (x0, x1) in zip(("pose", "auto-key"), tog):
        sub = a[:, x0:x1 + 1]
        c, n = core(sub, plate)
        allm = (np.abs(sub - plate).sum(axis=2) > 60)
        blend = "#%02x%02x%02x" % tuple(int(round(v)) for v in sub[allm].mean(axis=0))
        print("  %-9s cols %d..%d  glyph CORE %s (%d px)   everything-off-the-plate %s"
              % (label, x0, x1, c, n, blend))
