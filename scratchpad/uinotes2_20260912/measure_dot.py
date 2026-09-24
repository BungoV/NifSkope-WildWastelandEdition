"""Measure the transport row's glyph ink, one button at a time, out of the
2:1 picture the (q) gate saves.  Purpose: bungo said the auto-key dot is "not
centered".  Centred against WHAT is the question, so this prints, for every
lump of ink in the row, its bounding box, its centre, and the row's own
vertical centre line.  No claim is made here that the picture does not carry.
"""
import sys
from PIL import Image
import numpy as np

src = sys.argv[1] if len(sys.argv) > 1 else "scratchpad/uinotes2_20260912/images/before/transport_2x.png"
im = Image.open(src).convert("RGB")
a = np.asarray(im).astype(int)
h, w, _ = a.shape
print("picture %dx%d  (2:1, so logical %dx%d)" % (w, h, w // 2, h // 2))

# the row's own background is the most common colour
cols, counts = np.unique(a.reshape(-1, 3), axis=0, return_counts=True)
bg = cols[counts.argmax()]
print("background rgb", tuple(bg), "=", counts.max(), "px of", w * h)

d = np.abs(a - bg).sum(axis=2)
ink = d > 60           # anything clearly not the background

# split into columns of ink separated by >= 6 blank columns
colhas = ink.any(axis=0)
runs = []
x = 0
while x < w:
    if colhas[x]:
        x0 = x
        gap = 0
        while x < w and (colhas[x] or gap < 6):
            if colhas[x]:
                gap = 0
                x1 = x
            else:
                gap += 1
            x += 1
        runs.append((x0, x1))
    else:
        x += 1

print("\n%-4s %-12s %-14s %-10s %s" % ("#", "x range", "y range", "centre y", "h/w"))
for i, (x0, x1) in enumerate(runs):
    sub = ink[:, x0:x1 + 1]
    ys = np.where(sub.any(axis=1))[0]
    y0, y1 = ys.min(), ys.max()
    print("%-4d %-12s %-14s %-10.1f %dx%d" % (
        i, "%d..%d" % (x0, x1), "%d..%d" % (y0, y1), (y0 + y1) / 2.0, y1 - y0 + 1, x1 - x0 + 1))
print("\nrow centre line y = %.1f" % ((h - 1) / 2.0))
