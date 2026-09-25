"""The three SEAM1 control bakes side by side, perspective (01_sanctuary_oblique camera), one caption each."""
from PIL import Image, ImageDraw, ImageFont
H = 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/pics/'
cap = [('a  his load order, BAKE1 settings', 'ctl_a_oblique.png'),
       ('b  Fallout4.esm alone', 'ctl_b_oblique.png'),
       ('c  as a, --cover off', 'ctl_c_oblique.png')]
f = ImageFont.truetype('arial.ttf', 22); fb = ImageFont.truetype('arialbd.ttf', 24)
ims = [Image.open(H + 'ctl/' + p).convert('RGB') for _, p in cap]
w = 800; h = int(ims[0].height * w / ims[0].width)
page = Image.new('RGB', (3 * w + 40, h + 130), (24, 24, 24)); d = ImageDraw.Draw(page)
d.text((10, 8), 'SEAM1 controls, same exe, same camera (view 8, cells -23,18..-16,25, look-at Z 6690). The edge, measured on the cell grid, is in all three bakes:',
       fill=(235, 235, 235), font=f)
d.text((10, 36), 'not a plugin (b has it with Fallout4.esm alone), not ground cover (c has it with --cover off). a is byte-identical to the shipped render.',
       fill=(235, 235, 235), font=f)
for i, ((c, _), im) in enumerate(zip(cap, ims)):
    x = 10 + i * (w + 10)
    page.paste(im.resize((w, h), Image.LANCZOS), (x, 100))
    d.text((x + 6, 70), c, fill=(255, 220, 90), font=fb)
page.save(H + 'controls_side_by_side_oblique.png'); print(page.size)
