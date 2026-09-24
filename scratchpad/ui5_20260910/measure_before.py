"""Lane UI5 -- where the menu bar's items actually sit in the SHIPPED window.

The 20:45:47 exe cannot be asked (bungo has it open, one instance ever), but it
already answered once: lane UI4's in-application grab of the top of the window,
scratchpad/ui4_20260910/images/strip_after.png, was taken from that exe by
WW_WATERUI_SHOT and starts at the window's own (0,0) -- so its first 35 rows ARE
the menu row bungo photographed.

This reads the text ink out of those rows: a pixel is ink when it is much
brighter than the bar's background (the bar is #25272a, the text #e6e8eb), and
the columns of ink are grouped into words by their gaps.

Run: python scratchpad/ui5_20260910/measure_before.py [image] [rowheight]
"""

import sys
from PIL import Image

path = sys.argv[1] if len(sys.argv) > 1 else "scratchpad/ui4_20260910/images/strip_after.png"
row = int(sys.argv[2]) if len(sys.argv) > 2 else 35

img = Image.open(path).convert("RGB")
W, H = img.size
px = img.load()
print(f"{path}  {W}x{H}, menu row taken as y 0..{row - 1}")


def lum(p):
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]


# the bar's own background = the most common colour in the row
hist = {}
for y in range(row):
    for x in range(W):
        hist[px[x, y]] = hist.get(px[x, y], 0) + 1
bg = max(hist.items(), key=lambda kv: kv[1])[0]
print(f"  background {bg} ({hist[bg]} px of {W * row}), luminance {lum(bg):.1f}")

THRESH = lum(bg) + 40.0
cols = []
for x in range(W):
    ys = [y for y in range(row) if lum(px[x, y]) > THRESH]
    cols.append(ys)

# group columns with ink into words, splitting on >= 5 empty columns
words = []
run = None
gap = 0
for x in range(W):
    if cols[x]:
        if run is None:
            run = [x, x]
        else:
            run[1] = x
        gap = 0
    elif run is not None:
        gap += 1
        if gap >= 5:
            words.append(tuple(run))
            run = None
if run is not None:
    words.append(tuple(run))

centre = (row - 1) / 2.0
print(f"  ink threshold luminance > {THRESH:.1f}; row centre {centre:.1f}")
print(f"  {'item':>10}  {'x0':>4} {'x1':>4}  {'top':>3} {'bot':>3}  {'inkc':>5}  {'offset':>6}")
names = ["File", "View", "Spells", "Options", "Help"]
out = []
for i, (x0, x1) in enumerate(words):
    ys = [y for x in range(x0, x1 + 1) for y in cols[x]]
    if not ys:
        continue
    top, bot = min(ys), max(ys)
    c = (top + bot) / 2.0
    name = names[i] if i < len(names) else f"#{i}"
    out.append((name, x0, x1, top, bot, c, c - centre))
    print(f"  {name:>10}  {x0:4d} {x1:4d}  {top:3d} {bot:3d}  {c:5.1f}  {c - centre:+6.1f}")

if out:
    offs = [o[6] for o in out]
    print(f"  worst offset {max(offs, key=abs):+.1f} px, spread {max(offs) - min(offs):.1f} px")
