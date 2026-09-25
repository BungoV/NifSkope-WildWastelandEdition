"""The BNS tree picture: full 3D render vs its 8x8 octahedral card (N8, the shipped default).
usage: pic_tree.py <3d.png> <card albedo.png> <out.png> <title>"""
import sys
from PIL import Image, ImageDraw, ImageFont

three, sheet_path, out, title = sys.argv[1:5]
BG = (43, 45, 48)
font = ImageFont.truetype('arial.ttf', 22)
small = ImageFont.truetype('arial.ttf', 18)

im3 = Image.open(three).convert('RGB')
# crop the 3D render to the tree: pixels that differ from the background
px = im3.load()
w, h = im3.size
xs, ys = [], []
for y in range(0, h, 2):
    for x in range(0, w, 2):
        r, g, b = px[x, y]
        if abs(r - BG[0]) + abs(g - BG[1]) + abs(b - BG[2]) > 24:
            xs.append(x); ys.append(y)
pad = 20
box = (max(min(xs) - pad, 0), max(min(ys) - pad, 0), min(max(xs) + pad, w), min(max(ys) + pad, h))
tree = im3.crop(box)

sheet = Image.open(sheet_path).convert('RGBA')
fw, fh = sheet.size[0] // 8, sheet.size[1] // 8
flat = Image.new('RGBA', sheet.size, BG + (255,))
flat.alpha_composite(sheet)
flat = flat.convert('RGB')
# one horizon-edge frame (bottom row), magnified nearest-neighbour to the 3D tree's height
col, row = 3, 7
frame = flat.crop((col * fw, row * fh, (col + 1) * fw, (row + 1) * fh))
scale = tree.size[1] / float(fh)
frame_big = frame.resize((int(fw * scale), tree.size[1]), Image.NEAREST)
sheet_show = flat.resize((int(flat.size[0] * tree.size[1] / flat.size[1]), tree.size[1]), Image.LANCZOS)

gap = 30
top = 70
bottom = 80
W = tree.size[0] + frame_big.size[0] + sheet_show.size[0] + gap * 4
H = tree.size[1] + top + bottom
canvas = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(canvas)
d.text((gap, 15), title, fill=(230, 230, 230), font=font)
x = gap
NL = chr(10)
for img, cap in ((tree, 'full 3D model' + NL + '(lit render, his MO2 textures)'),
                 (frame_big, ('one card frame, %dx%d px' + NL + '(shown %.1fx, nearest)') % (fw, fh, scale)),
                 (sheet_show, ('the whole 8x8 card sheet' + NL + '(N8: 64 views, %dx%d)') % sheet.size)):
    canvas.paste(img, (x, top))
    d.multiline_text((x, top + img.size[1] + 10), cap, fill=(200, 200, 200), font=small)
    x += img.size[0] + gap
canvas.save(out)
print(out, canvas.size, 'tree crop', tree.size, 'frame', (fw, fh))
