"""IMPOSTOR16 job 2 -- every baked sheet as a PNG he can look at, plus an index.

    python sheets16.py <bakes root> <outdir>

Reads the bake's own PNGs (what the bake wrote, before BC compression):
<root>/<tag>/bake/treemapleinstitute06green_oct_{albedo,normal,gsaos,g}.png
Channel meanings are the bake's (src/nifskope_ui.cpp ~23500):
  albedo  RGB unlit colour, A coverage (coverage-encoded)
  normal  R nX, G nY (frame space, half-packed), B height (window z), A sway
  gsaos   R gloss, G specular, B AO (height-neighbourhood x map AO), A subsurface mask
  g       RGB emissive
"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

root, od = sys.argv[1], sys.argv[2]
os.makedirs(od, exist_ok=True)
B = 'treemapleinstitute06green_oct_'
GREY = np.array([40, 42, 46], np.float32)
try:
    F = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
    FB = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
except Exception:
    F = FB = ImageFont.load_default()


def ld(tag, k):
    return np.asarray(Image.open(os.path.join(root, tag, 'bake', B + k + '.png')).convert('RGBA')).astype(np.float32)


def save(arr, name):
    im = Image.fromarray(np.clip(arr + 0.5, 0, 255).astype(np.uint8))
    im.save(os.path.join(od, name))
    return name


def grey(ch):
    return np.repeat(ch[..., None], 3, -1)


out = []   # (file, caption)
SETS = (('n16_2k', True), ('n16_1k', False), ('n4_512', False))
if len(sys.argv) > 3:                      # e.g. n8_2k -> that set only, every channel
    SETS = tuple((t, True) for t in sys.argv[3:])
for tag, full in SETS:
    lab = {'n16_2k': '16x16 frames, 2048 sheet (128 px frames)',
           'n16_1k': '16x16 frames, 1024 sheet (64 px frames)',
           'n4_512': '4x4 frames, 1920x2048 sheet (current, 512 px frames)',
           'n8_2k': '8x8 frames, 1920x2048 sheet (256 px frames)'}[tag]
    d = ld(tag, 'albedo')
    a = d[..., 3:4] / 255.0
    over = d[..., :3] * a + GREY * (1 - a)
    out.append((save(over, f'{tag}_d_colour_over_grey.png'), f'{lab}: _d colour with alpha applied over dark grey'))
    if not full:
        continue
    out.append((save(d[..., :3], f'{tag}_d_colour_raw.png'), f'{lab}: _d raw colour (dilated past the edge)'))
    out.append((save(grey(d[..., 3]), f'{tag}_d_alpha.png'), f'{lab}: _d alpha (coverage)'))
    n = ld(tag, 'normal')
    x = n[..., 0] / 127.5 - 1; y = n[..., 1] / 127.5 - 1
    z = np.sqrt(np.clip(1 - x * x - y * y, 0, 1))
    nc = np.stack([n[..., 0], n[..., 1], (z * 0.5 + 0.5) * 255], -1)
    out.append((save(nc, f'{tag}_n_normal_as_colour.png'), f'{lab}: _n normal as colour (R x, G y, B z rebuilt)'))
    out.append((save(grey(n[..., 2]), f'{tag}_n_height.png'), f'{lab}: _n height (blue)'))
    out.append((save(grey(n[..., 3]), f'{tag}_n_sway.png'), f'{lab}: _n sway (alpha)'))
    g = ld(tag, 'gsaos')
    out.append((save(g[..., :3], f'{tag}_gsaos_rgb.png'), f'{lab}: _gsaos as stored (R gloss, G specular, B AO)'))
    for i, nm in enumerate(('gloss', 'specular', 'ao', 'subsurface')):
        out.append((save(grey(g[..., i]), f'{tag}_gsaos_{nm}.png'), f'{lab}: _gsaos {nm}'))
    e = ld(tag, 'g')
    out.append((save(e[..., :3], f'{tag}_g_emissive.png'), f'{lab}: _g emissive colour'))

# index: thumbnails 384 px, 4 across, caption under each
T, C, PAD, CAP = 384, 4, 16, 84
rows = (len(out) + C - 1) // C
idx = Image.new('RGB', (C * (T + PAD) + PAD, 60 + rows * (T + CAP + PAD)), (24, 25, 28))
dr = ImageDraw.Draw(idx)
dr.text((PAD, 14), 'TreeMapleInstitute06Green - octahedral impostor bake, every sheet (full size files beside this index)',
        fill=(235, 235, 235), font=FB)
for k, (fn, cap) in enumerate(out):
    r, c = divmod(k, C)
    x0, y0 = PAD + c * (T + PAD), 60 + r * (T + CAP + PAD)
    im = Image.open(os.path.join(od, fn)).convert('RGB')
    im.thumbnail((T, T), Image.LANCZOS)
    idx.paste(im, (x0, y0))
    words, lines, cur = cap.split(' '), [], ''
    for w in words:
        if len(cur) + len(w) > 40:
            lines.append(cur); cur = w
        else:
            cur = (cur + ' ' + w).strip()
    lines.append(cur)
    for i, ln in enumerate(lines[:3]):
        dr.text((x0, y0 + T + 4 + i * 24), ln, fill=(210, 210, 210), font=F)
idx.save(os.path.join(od, '00_index.png'))
for fn, cap in out:
    print(fn, '|', cap)
print('index', os.path.join(od, '00_index.png'))
