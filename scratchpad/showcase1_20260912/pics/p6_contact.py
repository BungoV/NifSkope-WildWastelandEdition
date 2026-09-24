"""Lane SHOWCASE1 picture 6 -- one contact sheet of everything else.

Every picture also stands alone in the same folder at full size; this is only
the index, so bungo can see the set in one frame and then open the one he wants.
"""
import os
import numpy as np
from PIL import Image, ImageDraw
from common import L, font, save

SHEET = [
    ('1a_sheets_colour.png', '1a  colour sheet: ours ON / ours OFF / vanilla'),
    ('1b_sheets_msn.png', '1b  normal sheet (_msn): the 2K cache'),
    ('1c_sheets_mask.png', '1c  mask sheet (_data) and its four channels'),
    ('2_terrain_and_objects.png', '2  terrain + objects, 3 views x 3 identity states'),
    ('3_far_rings_and_cards.png', '3  the far rings and the impostor cards'),
    ('4_objects_only.png', '4  the near .BTO alone'),
    ('5_ao_greyscale.png', '5  the baked AO in greyscale'),
]

CELL = 470
CAP = 26
cols = 4
have = [(n, t) for n, t in SHEET if os.path.exists(L + '/images/' + n)]
rows = (len(have) + cols - 1) // cols
W = 12 + cols * (CELL + 12)
H = 64 + rows * (CELL + CAP + 12)
out = Image.new('RGB', (W, H), (16, 16, 18))
d = ImageDraw.Draw(out)
d.text((12, 10), '6  Lane SHOWCASE1 contact sheet -- Sanctuary, cells -20 24 -9 35, dim 4',
       font=font(26), fill=(240, 240, 240))
d.text((12, 42), 'Every one of these is also a full-size .png beside this file. %d of %d built.'
       % (len(have), len(SHEET)), font=font(15), fill=(170, 170, 175))
for i, (n, t) in enumerate(have):
    r, c = divmod(i, cols)
    x = 12 + c * (CELL + 12)
    y = 64 + r * (CELL + CAP + 12)
    im = Image.open(L + '/images/' + n).convert('RGB')
    im.thumbnail((CELL, CELL), Image.LANCZOS)
    out.paste(im, (x + (CELL - im.size[0]) // 2, y + (CELL - im.size[1]) // 2))
    d.rectangle([x, y, x + CELL - 1, y + CELL - 1], outline=(70, 70, 76))
    d.text((x + 2, y + CELL + 4), t, font=font(15), fill=(225, 225, 225))
save(out, '6_contact_sheet.png')
