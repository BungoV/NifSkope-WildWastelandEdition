"""The third picture: the region seen from above through NifSkope's own render
hook, vanilla's chunk beside ours without and with roads.

Each side is staged as its own miniature data root -- the `.BTR` with
`textures/terrain/commonwealth/` beside it -- so `NifModel::load`'s
`addNIFResourcePath` serves OUR sheets to the renderer and neither side can
borrow the other's files (`nifskope-ww-vanilla-compare` section 2). Nothing in
the game folder is touched. The camera is the hook's Top view, the same for all
three, at the same size.

  python make_topview.py <imagesDir>
"""

import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

CROP = (505, 205, 1015, 715)


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


F, FS, FT = font(15), font(13), font(21)


def main(argv):
    d = argv[0]
    pics, stats = [], []
    for s in ('van', 'before', 'after'):
        im = Image.open(os.path.join(d, 'top_%s.png' % s)).convert('RGB')
        a = np.asarray(im).astype(float)
        stats.append((im.size, a.mean(), a.std()))
        pics.append(im.crop(CROP).resize((420, 420), Image.LANCZOS))
    pad, top, capH = 18, 92, 92
    W = pad + 3 * (420 + pad)
    H = top + 420 + capH
    out = Image.new('RGB', (W, H), (22, 22, 24))
    dr = ImageDraw.Draw(out)
    dr.text((pad, 12), 'The same ground from above, through the render hook',
            font=FT, fill=(235, 235, 238))
    dr.text((pad, 40),
            'Commonwealth.4.-20.20.BTR at the hook\'s Top view, %dx%d, each side '
            'staged as its own data root so the renderer serves that side\'s own '
            'sheets.' % stats[0][0], font=FS, fill=(150, 152, 158))
    dr.text((pad, 57),
            'Our .BTR is byte-identical between the two right-hand panels -- roads '
            'touch the COLOUR SHEET and no geometry -- so the only difference '
            'between them is the sheet.', font=FS, fill=(150, 152, 158))
    dr.text((pad, 74),
            'These are the viewport\'s own lighting and normal mapping, not the '
            'sheet\'s colours; the sheets themselves are cmp_sanctuary_road.png.',
            font=FS, fill=(150, 152, 158))
    caps = [
        ('VANILLA mesh + VANILLA sheets',
         ['Bethesda\'s shipped chunk and its shipped textures',
          'frame mean %.1f, SD %.2f' % (stats[0][1], stats[0][2])]),
        ('OUR mesh + OUR sheets, --no-roads',
         ['the rung: no road anywhere on the ground',
          'frame mean %.1f, SD %.2f' % (stats[1][1], stats[1][2])]),
        ('OUR mesh + OUR sheets, --roads',
         ['the loop road, its cul-de-sac and the driveways',
          'frame mean %.1f, SD %.2f' % (stats[2][1], stats[2][2])]),
    ]
    for k, p in enumerate(pics):
        x = pad + k * (420 + pad)
        out.paste(p, (x, top))
        dr.rectangle([x, top, x + 419, top + 419], outline=(70, 70, 76))
        ty = top + 425
        dr.text((x + 2, ty), caps[k][0], font=F, fill=(120, 190, 255))
        ty += 18
        for ln in caps[k][1]:
            dr.text((x + 2, ty), ln, font=FS, fill=(150, 152, 158))
            ty += 15
    p = os.path.join(d, 'top_region.png')
    out.save(p)
    print('wrote %s  %dx%d' % (p, W, H))


if __name__ == '__main__':
    main(sys.argv[1:])
