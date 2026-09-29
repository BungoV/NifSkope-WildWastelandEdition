"""GROUND1 picture finisher: optional 2x crop, then a 60 px title bar (maps1 label_full.py style).
usage: python label.py <in.png> <out.png> "<title>" [x y w h]
The render's own 59 px top strip (WW_RENDER_SIZE H+59) is cut first, so crop coordinates are in the 1600x1600 map.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

src, dst, title = sys.argv[1], sys.argv[2], sys.argv[3]
im = Image.open(src).convert("RGB")
if im.height == im.width + 59:
    im = im.crop((0, 59, im.width, im.height))
if len(sys.argv) == 8:
    x, y, w, h = (int(v) for v in sys.argv[4:8])
    im = im.crop((x, y, x + w, y + h)).resize((2 * w, 2 * h), Image.NEAREST)
try:
    F = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 34)
except OSError:
    F = ImageFont.load_default()
bar = 60
out = Image.new("RGB", (im.width, im.height + bar), (20, 20, 24))
out.paste(im, (0, bar))
ImageDraw.Draw(out).text((16, 10), title, fill=(240, 240, 240), font=F)
out.save(dst)
print("wrote %s %dx%d" % (dst, out.width, out.height))
