"""TILING5: burn a plain-words title into a 60 px bar on top of one picture (maps1 label_full.py's bar).
    python t5_label.py IN.png OUT.png "title"
"""
import sys
from PIL import Image, ImageDraw, ImageFont

src, dst, title = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    F = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 34)
except OSError:
    F = ImageFont.load_default()
im = Image.open(src).convert("RGB")
bar = 60
out = Image.new("RGB", (im.width, im.height + bar), (20, 20, 24))
out.paste(im, (0, bar))
ImageDraw.Draw(out).text((16, 10), title, fill=(240, 240, 240), font=F)
out.save(dst)
print("labelled", dst, out.size)
