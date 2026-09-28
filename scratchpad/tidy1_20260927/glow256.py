"""TIDY1 continuation: why the 256x512 legacy card glow group was not dropped.

Reads the six card sets that make Commonwealth.LodgenCards.legacy.256x512_g.DDS
(bake1 cards/) and measures their emissive PNG (the file the card-array writer
decodes, not the DDS): size, count of texels with RGB != 0, max channel, and how
many of those texels survive a 5:6:5 quantisation (the BC1 end-point precision).
    python glow256.py [cards dir]
"""
import os, sys
from PIL import Image

CARDS = sys.argv[1] if len(sys.argv) > 1 else r"E:\Projects\NifskopeWWE-bake1\scratchpad\bake1_20260925\cards"
IDS = ["000531b3", "000a7208", "000a7209", "000f4791", "00121550", "2c550e59"]


def q565(r, g, b):
    # round to the nearest 5/6/5 level, as a BC1 end point stores it
    return round(r * 31 / 255), round(g * 63 / 255), round(b * 31 / 255)


for i in IDS:
    names = sorted(n for n in os.listdir(CARDS) if n.lower().startswith(i))
    print(i, " ".join(n for n in names if "_g" in n.lower() or n.lower().endswith((".lodm", ".txt", ".json"))))
    png = os.path.join(CARDS, i + "_oct_g.png")
    if not os.path.exists(png):
        print("   no _oct_g.png")
        continue
    im = Image.open(png)
    mode = im.mode
    im = im.convert("RGBA")
    px = im.getdata()
    lit = [p for p in px if p[0] | p[1] | p[2]]
    mx = max((max(p[:3]) for p in lit), default=0)
    s565 = sum(1 for p in lit if any(q565(*p[:3])))
    hist = {}
    for p in lit:
        hist[max(p[:3])] = hist.get(max(p[:3]), 0) + 1
    print("   png %dx%d %s: %d of %d texels RGB!=0, max %d, %d survive 565; max-channel histogram %s"
          % (im.width, im.height, mode, len(lit), len(px), mx, s565, dict(sorted(hist.items()))))
