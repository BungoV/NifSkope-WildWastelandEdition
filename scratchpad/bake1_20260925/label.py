"""Put a title bar over a render (the place name, cells, view) and a scale bar of one cell.
usage: label.py <in.png> <out.png> <title> <cells across the frame>"""
import sys
from PIL import Image, ImageDraw, ImageFont

src, out, title, across = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
im = Image.open(src).convert('RGB')
w, h = im.size
bar = 48
canvas = Image.new('RGB', (w, h + bar), (30, 31, 34))
canvas.paste(im, (0, bar))
d = ImageDraw.Draw(canvas)
d.text((14, 11), title, fill=(235, 235, 235), font=ImageFont.truetype('arial.ttf', 24))
cell_px = w / across
x1 = w - 20
x0 = int(x1 - cell_px)
y = h + bar - 24
d.rectangle((x0 - 6, y - 26, x1 + 6, y + 10), fill=(30, 31, 34))
d.line((x0, y, x1, y), fill=(255, 255, 255), width=4)
d.text((x0, y - 24), '1 cell = 4096 units', fill=(255, 255, 255), font=ImageFont.truetype('arial.ttf', 16))
canvas.save(out)
print(out, canvas.size)
