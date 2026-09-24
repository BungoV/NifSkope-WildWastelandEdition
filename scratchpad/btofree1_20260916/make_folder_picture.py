# Lane BTOFREE1, 2026-09-16 -- the mod-folder picture, labels burned in.
#
# The two trees are the gate's own: `rung_native` is the previous exe's FO4CS
# bake and `drop` is this exe's, same region, same switches, same dim. Nothing
# here is drawn by hand: every row is read off disk at the moment the picture is
# made, and the bytes are `os.path.getsize`.
import os
from PIL import Image, ImageDraw, ImageFont

W = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/btofree_work/'
OUT = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/pictures/'
os.makedirs(OUT, exist_ok=True)


def font(size, mono=False):
    names = ('consola.ttf', 'DejaVuSansMono.ttf') if mono else ('arial.ttf', 'segoeui.ttf')
    for n in names:
        try:
            return ImageFont.truetype(n, size)
        except Exception:
            pass
    return ImageFont.load_default()


def listing(root):
    rows = []
    for dirpath, _dirs, files in os.walk(root):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            rel = os.path.relpath(p, root).replace(chr(92), '/')
            rows.append((rel, os.path.getsize(p)))
    return sorted(rows)


GOLD = (255, 220, 130)
WHITE = (238, 238, 238)
GREY = (150, 150, 150)
RED = (235, 110, 110)
GREEN = (120, 215, 140)
BG = (12, 12, 14)


def column(title, subtitle, rows, width, highlight):
    fh = font(17)
    fm = font(14, True)
    fs = font(14)
    head = 62
    line = 19
    img = Image.new('RGB', (width, head + line * (len(rows) + 3) + 16), BG)
    d = ImageDraw.Draw(img)
    d.text((12, 10), title, font=fh, fill=GOLD)
    d.text((12, 34), subtitle, font=fs, fill=GREY)
    y = head
    total = 0
    bto = 0
    for rel, n in rows:
        hot = rel.upper().endswith('.BTO')
        total += n
        if hot:
            bto += n
        d.text((12, y), rel, font=fm, fill=(highlight if hot else WHITE))
        d.text((width - 130, y), '{:>12,}'.format(n), font=fm, fill=(highlight if hot else GREY))
        y += line
    y += line
    d.text((12, y), '%d files, %s bytes' % (len(rows), '{:,}'.format(total)), font=fm, fill=WHITE)
    y += line
    if bto:
        d.text((12, y), '%s of that is .BTO chunk files' % '{:,}'.format(bto), font=fm, fill=RED)
    else:
        d.text((12, y), 'no .BTO chunk file anywhere in the folder', font=fm, fill=GREEN)
    return img


a = listing(W + 'rung_native')
b = listing(W + 'drop')
cw = 560
ca = column('BEFORE  (the previous program)', 'FO4CS target, chunk (-20,24) dim 4, same switches', a, cw, RED)
cb = column('AFTER  (this build)', 'the same bake, nothing else changed', b, cw, RED)

top = 34
sheet = Image.new('RGB', (cw * 2 + 14, top + max(ca.height, cb.height)), BG)
ImageDraw.Draw(sheet).text(
    (12, 8),
    'The mod folder after one FO4 Community Shaders bake -- read off disk, not typed',
    font=font(16), fill=GOLD)
sheet.paste(ca, (0, top))
sheet.paste(cb, (cw + 14, top))
sheet.save(OUT + 'mod_folder_before_after.png')
print('wrote mod_folder_before_after.png', sheet.size, len(a), 'vs', len(b), 'files')
