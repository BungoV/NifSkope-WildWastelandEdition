"""Label the three AO panels of one place and put them side by side (half scale) on one sheet.
usage: ao_label.py <tag> <place title>"""
import sys
from PIL import Image, ImageDraw, ImageFont, ImageStat
W = r'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/pics/ao'
tag, place = sys.argv[1], sys.argv[2]
F = ImageFont.truetype('arial.ttf', 22); f2 = ImageFont.truetype('arial.ttf', 16)
panels = [
 ('ao_x_diffuse', 'AO over diffuse (WW_LODL_AO=1)',
  'terrain: VT colour x .lodt mask-sheet B (sky AO, Commonwealth.VT.2.lodt). objects: diffuse x .lodo selfAO x .lodi v6 per-vertex scene AO'),
 ('diffuse', 'diffuse only (default lit view, = BAKE1 picture)', 'terrain: VT colour sheet; objects: their LOD textures, no AO'),
 ('ao_only', 'AO only (WW_LODL_CHANNEL=ao, flat, greyscale)', 'terrain: .lodt mask B; objects: .lodo selfAO x .lodi v6 scene AO; white = open, black = occluded'),
]
outs = []
for key, title, src in panels:
    im = Image.open('%s/work/%s_%s.png' % (W, tag, key)).convert('RGB')
    s = ImageStat.Stat(im.convert('L'))
    ncol = len(set(im.resize((200, 200)).getdata()))
    w, h = im.size; bar = 76
    c = Image.new('RGB', (w, h + bar), (30, 31, 34)); c.paste(im, (0, bar))
    d = ImageDraw.Draw(c)
    d.text((14, 8), '%s -- %s' % (place, title), fill=(235, 235, 235), font=F)
    d.text((14, 44), '%s   [lum mean %.1f sd %.1f, colours %d]' % (src, s.mean[0], s.stddev[0], ncol), fill=(200, 200, 200), font=f2)
    p = '%s/%s_%s.png' % (W, tag, key); c.save(p); outs.append(c)
    print(p, c.size, 'lum mean %.1f sd %.1f colours %d' % (s.mean[0], s.stddev[0], ncol))
# SHEET_ORDER (director 2026-09-25): diffuse | AO only | AO x diffuse for the oblique sets
import os
if os.environ.get('SHEET_ORDER') == 'dia':
    outs = [outs[1], outs[2], outs[0]]
hw = outs[0].size[0] // 2; hh = outs[0].size[1] // 2
sheet = Image.new('RGB', (hw * 3, hh), (30, 31, 34))
for i, c in enumerate(outs):
    sheet.paste(c.resize((hw, hh), Image.LANCZOS), (i * hw, 0))
sheet.save('%s/%s_sheet.png' % (W, tag)); print('%s/%s_sheet.png' % (W, tag), sheet.size)
