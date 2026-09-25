"""GRASSCOL picture: left = the grass colour the bake used per GRAS (old: smallest mip / alpha) beside the alpha-weighted
mean the fix uses; right = the Sanctuary block's colour texels (dim-4 chunk -20..-17, 20..23), old exe vs new exe, default
tint 0.35, his load order (ctl/rung/a vs ctl/new/a; the new one also carries the edge fix)."""
import sys, os, pickle, numpy as np
from PIL import Image, ImageDraw
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import vtread
d = pickle.load(open(HERE + '/grass_census.pkl', 'rb'))
rows = [r for r in d['rows'] if r[2] and 'bake' in r[2] and r[2]['meanA'] < 0.9][:8]
W = 1400; H = 640; img = Image.new('RGB', (W, H), (24, 24, 24)); dr = ImageDraw.Draw(img)
dr.text((20, 10), 'grass colour per GRAS: OLD bake (smallest mip / alpha)  vs  NEW (alpha-weighted mean)', fill=(230, 230, 230))
for i, (name, n, t0, bg) in enumerate(rows):
    y = 40 + i * 72
    old = tuple(int(v * 255) for v in t0['bake']) if t0['bake'] is not None else (0, 0, 0)
    new = tuple(int(round(v * 255)) for v in t0['aweighted'])
    dr.rectangle([20, y, 110, y + 60], fill=old); dr.rectangle([120, y, 210, y + 60], fill=new)
    dr.text((220, y + 8), '%s  (n=%d)' % (name, n), fill=(230, 230, 230))
    dr.text((220, y + 30), 'old %s   new %s   mean alpha %.2f' % (old, new, t0['meanA']), fill=(180, 180, 180))
V = 'vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
def crop(tag):
    v = vtread.Vt(HERE + '/ctl/%s/a/%s' % (tag, V)); m, wW, nN = v.mosaic(-20, 20, -17, 23, 1)
    tpc = v.content // v.levelDim; x0 = (-20 - wW) * tpc; y0 = (nN - 24) * tpc
    return Image.fromarray(m[y0:y0 + 4 * tpc, x0:x0 + 4 * tpc, :3]).resize((300, 300), Image.LANCZOS)
for j, (tag, lab) in enumerate((('rung', 'OLD exe (shipped law)'), ('new', 'NEW exe'))):
    x = 760 + j * 320; img.paste(crop(tag), (x, 60)); dr.text((x, 40), lab + ', Sanctuary -20..-17 x 20..23', fill=(230, 230, 230))
img.save(HERE + '/pics/grass_green_vs_sandy.png'); print('pics/grass_green_vs_sandy.png')
