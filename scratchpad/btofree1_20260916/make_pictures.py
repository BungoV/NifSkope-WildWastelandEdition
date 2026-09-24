# Lane BTOFREE1, 2026-09-16 -- the pictures, with their labels BURNED IN.
#
# A picture whose caption lives in a report is a picture that will be looked at
# without the report. Everything a reader needs to place these frames is drawn
# into the pixels: which library, which file is which colour, and the three
# numbers the gate reads.
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/'
OUT = L + 'pictures/'
os.makedirs(OUT, exist_ok=True)

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from native_open_authority import load_png, mask_of


def font(size):
    for name in ('arial.ttf', 'segoeui.ttf', 'DejaVuSans.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except Exception:
            pass
    return ImageFont.load_default()


def panel(lodi_png, bto_png, title, lines, width=760):
    a = load_png(lodi_png)
    b = load_png(bto_png)
    ma, _ = mask_of(a)
    mb, _ = mask_of(b)
    inter = ma & mb
    img = np.full(ma.shape + (3,), 18, dtype=np.uint8)
    img[inter] = (150, 150, 150)
    img[ma & ~mb] = (225, 70, 70)
    img[mb & ~ma] = (70, 130, 235)
    pic = Image.fromarray(img).resize((width, int(width * ma.shape[0] / ma.shape[1])),
                                      Image.NEAREST)
    head = 34 + 20 * len(lines) + 12
    out = Image.new('RGB', (width, pic.height + head), (12, 12, 14))
    out.paste(pic, (0, head))
    d = ImageDraw.Draw(out)
    d.text((10, 8), title, font=font(20), fill=(245, 245, 245))
    y = 34
    for text, colour in lines:
        d.text((10, y), text, font=font(15), fill=colour)
        y += 20
    return out


GREY = (170, 170, 170)
RED = (235, 110, 110)
BLUE = (110, 160, 245)
WHITE = (240, 240, 240)

legend = [('grey: both draw it    red: only the .lodi scene    blue: only the .BTO', GREY)]

p1 = panel(L + 'iou_mnam/lodi.png', L + 'iou_mnam/bto.png',
           'library mnam  (the pre-2026-09-16 default)',
           legend + [('covered 0.9874   area 1.195 x   IoU 0.8179', WHITE),
                     ('the excess is 17.4 % of the scene; every pixel of it within 16 px of a shared one', RED),
                     ('largest excess blob 143 px, none over 200 px', RED)])
p2 = panel(L + 'iou/lodi.png', L + 'iou/bto.png',
           'library near  (NATIVE1c default, today)',
           legend + [('covered 0.9604   area 1.512 x   IoU 0.6190', WHITE),
                     ('same objects, drawn from the full near model instead of the LOD mesh', RED),
                     ('the check passes on coverage and area; the IoU bar never passed on either', BLUE)])

w = max(p1.width, p2.width)
top = Image.new('RGB', (w, 30), (12, 12, 14))
ImageDraw.Draw(top).text(
    (10, 6),
    'native_open.sh check (c): the .lodi scene against the same chunk .BTO, one camera, chunk (-20,24) dim 4',
    font=font(15), fill=(255, 220, 130))
sheet = Image.new('RGB', (w * 2 + 12, top.height + max(p1.height, p2.height)), (12, 12, 14))
sheet.paste(top, (0, 0))
sheet.paste(p1, (0, top.height))
sheet.paste(p2, (w + 12, top.height))
sheet.save(OUT + 'check_c_libraries.png')
print('wrote check_c_libraries.png', sheet.size)

# the refuter sheet: what a failing input looks like
r = panel(L + 'iou/solid.png', L + 'iou/bto.png',
          'REFUTER: a solid frame in place of the .lodi scene',
          legend + [('covered 1.0000 -- a perfect score on coverage alone', WHITE),
                    ('area 5.5119 x the .BTO -- the area bar (2.00) catches it', RED),
                    ('this is why coverage is never gated on its own', RED)])
r.save(OUT + 'check_c_refuter_solid.png')
print('wrote check_c_refuter_solid.png', r.size)
