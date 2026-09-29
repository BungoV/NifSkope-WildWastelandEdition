# TIDY1: burn a 60 px plain-words title bar on top of one full-size render (MAPS1 label_full.py's bar and font).
#   python title.py <in.png> <out.png> "<title>"
import sys
from PIL import Image, ImageDraw, ImageFont

src, dst, title = sys.argv[1], sys.argv[2], sys.argv[3]
im = Image.open(src).convert("RGB")
bar = 60
out = Image.new("RGB", (im.width, im.height + bar), (20, 20, 24))
out.paste(im, (0, bar))
dr = ImageDraw.Draw(out)
font = None
for size in range(34, 18, -1):  # a long caption shrinks to fit rather than run off the edge
    font = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", size)
    if dr.textlength(title, font=font) <= im.width - 32:
        break
dr.text((16, 10 + (34 - font.size) // 2), title, fill=(240, 240, 240), font=font)
out.save(dst)
print("%s %dx%d" % (dst, out.width, out.height))
