"""Lane LAYOUT1 (2026-09-16): the three pictures, all read off disk.

Nothing here is drawn from a claim: every path printed is walked at the moment
the picture is made, the .lodt tiles are decoded from the container's own bytes
by tests/spells/lodgen_vt_check.py (which re-types the layout from the document
rather than importing the writer), and the two screenshots are frames NifSkope
itself wrote.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
OUT = ROOT + 'scratchpad/layout1_20260916/images/'
os.makedirs(OUT, exist_ok=True)
sys.path.insert(0, ROOT + 'tests/spells')

BG = (12, 12, 14)
WHITE = (238, 238, 238)
GOLD = (255, 220, 130)
GREY = (150, 150, 150)
GREEN = (120, 215, 140)


def font(size, mono=True):
    names = ('consola.ttf', 'DejaVuSansMono.ttf') if mono else ('segoeui.ttf', 'arial.ttf')
    for n in names:
        try:
            return ImageFont.truetype(n, size)
        except Exception:
            pass
    return ImageFont.load_default()


def banner(img, lines, pad=10):
    """Put a caption strip under an image and return the new image."""
    f = font(15)
    h = pad * 2 + len(lines) * 20
    out = Image.new('RGB', (img.width, img.height + h), BG)
    out.paste(img, (0, 0))
    d = ImageDraw.Draw(out)
    y = img.height + pad
    for text, col in lines:
        d.text((pad, y), text, font=f, fill=col)
        y += 20
    return out


def shot_caption(src, dst, path_on_disk, title, extra=()):
    im = Image.open(src).convert('RGB')
    lines = [(title, GOLD),
             ('opened from: ' + path_on_disk, WHITE),
             ('%d bytes on disk, frame %dx%d' % (os.path.getsize(path_on_disk),
                                                 im.width, im.height), GREY)]
    lines += [(t, GREEN) for t in extra]
    banner(im, lines).save(dst)
    print('wrote', dst)


def lodt_mosaic(lodt, dst, cols=8):
    import lodgen_vt_check as vt
    c = vt.Lodv(lodt)
    n = min(cols * cols, c.tileCount)
    tiles = []
    for i in range(n):
        try:
            tiles.append(c.colour(i))
        except Exception:
            break
    if not tiles:
        print('no tiles decoded from', lodt)
        return False
    tw, th = len(tiles[0][0]), len(tiles[0])
    rows = (len(tiles) + cols - 1) // cols
    img = Image.new('RGB', (cols * tw, rows * th), BG)
    for i, t in enumerate(tiles):
        tile = Image.new('RGB', (tw, th))
        tile.putdata([tuple(px) for row in t for px in row])
        img.paste(tile, ((i % cols) * tw, (i // cols) * th))
    if img.width < 640:
        s = max(1, 640 // img.width)
        img = img.resize((img.width * s, img.height * s), Image.NEAREST)
    rel = os.path.relpath(lodt, os.path.dirname(os.path.dirname(os.path.dirname(lodt))))
    img = banner(img, [
        ('the .lodt level rendered from its NEW path', GOLD),
        ('read from: ' + rel.replace(chr(92), '/'), WHITE),
        ('%d tiles decoded, %d x %d each, %d bytes in the container'
         % (len(tiles), tw, th, os.path.getsize(lodt)), GREY),
    ])
    img.save(dst)
    print('wrote', dst)
    return True


if __name__ == '__main__':
    print('this module is driven by the lane; import and call the pieces')
