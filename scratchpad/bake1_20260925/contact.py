"""Contact sheet of the lane's pictures, each labelled with its file name and caption.
usage: contact.py <out.png> <cols> <thumb px> <png>=<caption> [...]"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

out, cols, tw = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
items = [a.split('=', 1) for a in sys.argv[4:]]
BG = (30, 31, 34)
font = ImageFont.truetype('arial.ttf', 18)
cap_h = 50
rows = (len(items) + cols - 1) // cols
pad = 12
W = cols * (tw + pad) + pad
H = rows * (tw + cap_h + pad) + pad
sheet = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(sheet)
for i, (path, cap) in enumerate(items):
    im = Image.open(path).convert('RGB')
    im.thumbnail((tw, tw), Image.LANCZOS)
    x = pad + (i % cols) * (tw + pad)
    y = pad + (i // cols) * (tw + cap_h + pad)
    sheet.paste(im, (x + (tw - im.size[0]) // 2, y + (tw - im.size[1]) // 2))
    d.multiline_text((x, y + tw + 4), os.path.basename(path) + chr(10) + cap, fill=(220, 220, 220), font=font)
sheet.save(out)
print(out, sheet.size, len(items), 'pictures')
