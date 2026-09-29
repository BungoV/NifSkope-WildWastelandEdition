"""IDENT2 follow-up: top-down map of one landmark's footprint: named pieces grey, would-join blue, refused red,
other pieces light grey, vegetation green dots, hull black. usage: python fp_map.py <dump> <list> <rule> <out.png> [margin]"""
import sys, math
from PIL import Image, ImageDraw
import groups, fp_hull as FH
h, P, C = groups.load(sys.argv[1]); M = float(sys.argv[5]) if len(sys.argv) > 5 else 128
rule = [r for r in FH.load_rules(sys.argv[2]) if r[0] == sys.argv[3]][0]
mem, H, rows = FH.classify(P, rule, M)
xs = [x for x, _ in H]; ys = [y for _, y in H]
pad = 1500; x0, x1, y0, y1 = min(xs)-pad, max(xs)+pad, min(ys)-pad, max(ys)+pad
S = 1400 / max(x1-x0, y1-y0); W, Hh = int((x1-x0)*S), int((y1-y0)*S)
im = Image.new('RGB', (W, Hh), (255, 255, 255)); d = ImageDraw.Draw(im)
T = lambda x, y: ((x-x0)*S, (y1-y)*S)
def box(p, col, w=0):
    a = T(p['lo'][0], p['hi'][1]); b = T(p['hi'][0], p['lo'][1])
    d.rectangle([a, b], outline=col, fill=None if w else col)
for p in P:
    if p['tris'] and not p['tree'] and p['hi'][0] > x0 and p['lo'][0] < x1 and p['hi'][1] > y0 and p['lo'][1] < y1:
        box(p, (215, 215, 215))
for p in mem: box(p, (120, 120, 120))
for w, rt, dd, k in rows:
    for p in dd:
        box(p, (40, 90, 230) if k == 'join' else (230, 30, 30) if k == 'refuse' else (30, 160, 30))
d.polygon([T(x, y) for x, y in H], outline=(0, 0, 0))
im.save(sys.argv[4]); print(sys.argv[4], im.size)
