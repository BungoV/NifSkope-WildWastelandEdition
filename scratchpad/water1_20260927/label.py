"""label.py <raw.png> <render log> <out.png> <title> [flow]

A 60 px plain-words title bar on top, the render at full size, and under it a legend strip built from the
render's own "water legend (<view>):" note line (parsed, never typed). With "flow", the strip also carries a
colour wheel at the legend's own brightness, with the compass points marked.
"""
import sys, re, math
from PIL import Image, ImageDraw, ImageFont

src, logp, dst, title = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
flow = len(sys.argv) > 5 and sys.argv[5] == 'flow'
im = Image.open(src).convert('RGB')
W, H = im.size
txt = open(logp, encoding='utf-8', errors='replace').read()
m = re.search(r'water legend \(([^)]*)\): (.*)', txt)
items = []
if m:
    for part in m.group(2).split('; '):
        mm = re.match(r'\s*(.*?) = (\d+),(\d+),(\d+)', part)
        if mm:
            items.append((mm.group(1), (int(mm.group(2)), int(mm.group(3)), int(mm.group(4)))))
try:
    f1 = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 28)
    f2 = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
except OSError:
    f1 = f2 = ImageFont.load_default()
BAR = 60
colw = 390
cols = max(1, (W - 20) // colw)
rows = (len(items) + cols - 1) // cols
wheel = 180 if flow else 0
LEG = 20 + max(rows * 30, wheel) + 16 if items else 0
out = Image.new('RGB', (W, BAR + H + LEG), (22, 22, 28))
out.paste(im, (0, BAR))
d = ImageDraw.Draw(out)
d.text((14, 12), title, fill=(240, 240, 240), font=f1)
y0 = BAR + H + 12
x_off = 14
if flow and items:
    moving = [c for l, c in items if not l.startswith('still')]
    k = max(max(c) for c in moving) / 255.0 if moving else 1.0
    R = 80
    cx, cy = x_off + R + 10, y0 + R + 4
    for yy in range(-R, R + 1):
        for xx in range(-R, R + 1):
            rr = math.hypot(xx, yy)
            if rr > R or rr < 26:
                continue
            ang = math.atan2(-yy, xx) / (2 * math.pi)   # east = 0, counter-clockwise, north up
            ang = ang % 1.0
            h6 = ang * 6
            s = int(h6) % 6
            fr = h6 - int(h6)
            r, g, b = [(1, fr, 0), (1 - fr, 1, 0), (0, 1, fr), (0, 1 - fr, 1), (fr, 0, 1), (1, 0, 1 - fr)][s]
            out.putpixel((cx + xx, cy + yy), (int(r * k * 255 + .5), int(g * k * 255 + .5), int(b * k * 255 + .5)))
    for lab, a in (('E', 0), ('N', 90), ('W', 180), ('S', 270)):
        t = math.radians(a)
        d.text((cx + int(math.cos(t) * (R + 4)) - 6 + (6 if lab == 'E' else 0) - (6 if lab == 'W' else 0),
                cy - int(math.sin(t) * (R + 4)) - 12 - (8 if lab == 'N' else 0) + (6 if lab == 'S' else 0)),
               lab, fill=(230, 230, 230), font=f2)
    d.text((cx - 22, cy - 12), 'flows\ntoward', fill=(200, 200, 210), font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 12) if f2 is not None else None)
    x_off += 2 * R + 40
    cols = max(1, (W - x_off - 10) // colw)
for i, (lab, c) in enumerate(items):
    col, row = i // max(rows, 1) if False else i % cols, i // cols
    x = x_off + col * colw
    y = y0 + row * 30
    d.rectangle([x, y + 3, x + 22, y + 25], fill=c, outline=(200, 200, 200))
    d.text((x + 30, y + 1), lab, fill=(225, 225, 230), font=f2)
out.save(dst)
print('labelled', dst, out.size, len(items), 'swatches')
