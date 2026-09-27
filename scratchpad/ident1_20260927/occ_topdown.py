"""IDENT1: the maps1 O_occluders picture for any bake -- top-down, north up, 1600 px over cells x -5..2, y -10..-3
(20.48 u a pixel), the placed level-0 triangles as a grey silhouette (slot 0, level 0, the viewer's population via
lodl_channels_table.drawn), every occluder box whose centre is in the frame as an orange outline.
Code copied from maps1/offline_objects.py (population, transform, colours); nothing else drawn.
usage: python occ_topdown.py <lod base path without extension> <out.png>"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident1/tests/spells')
import lodgen_native_decode as ND
import lodl_channels_table as CT
from lodi_occluder_building import placed_tris

X0, Y0, X1, Y1 = -5, -10, 2, -3
WX0, WY1 = X0 * 4096.0, (Y1 + 1) * 4096.0
N = 1600
UPP = (X1 - X0 + 1) * 4096.0 / N
BG = (40, 40, 44)


def px(x, y):
    return ((x - WX0) / UPP, (WY1 - y) / UPP)


def main():
    base, out = sys.argv[1], sys.argv[2]
    L = ND.read_lodo(base + '.lodo')
    T = ND.read_lodi(base + '.lodi')
    sel = CT.drawn(L, T, X0, Y0, X1, Y1, 0, 0)
    im = Image.new('RGB', (N, N), BG)
    d = ImageDraw.Draw(im)
    ntri = 0
    for ii, inst, mesh in sel:
        for t in placed_tris(L, inst, mesh):
            d.polygon([px(v[0], v[1]) for v in t], fill=(78, 78, 84))
            ntri += 1
    nocc = 0
    for o in T['occluders']:
        if not (X0 * 4096 <= o['x'] < (X1 + 1) * 4096 and Y0 * 4096 <= o['y'] < (Y1 + 1) * 4096):
            continue
        mm = ND.unpack_rotation(o['r0'], o['r1'], o['r2'])
        cs = []
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            lx, ly = sx * o['hx'], sy * o['hy']
            cs.append(px(o['x'] + mm[0] * lx + mm[1] * ly, o['y'] + mm[3] * lx + mm[4] * ly))
        d.line(cs + [cs[0]], fill=(255, 170, 40), width=2)
        nocc += 1
    nh = 0
    if len(sys.argv) > 3:                      # hills.py --json: proposed hill boxes, drawn cyan
        import json, math
        for b in json.load(open(sys.argv[3]))['rows']:
            a = math.radians(b['yawDeg']); c, s = math.cos(a), math.sin(a)
            cs = [px(b['centre'][0] + c * sx * b['half'][0] - s * sy * b['half'][1],
                     b['centre'][1] + s * sx * b['half'][0] + c * sy * b['half'][1])
                  for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            d.line(cs + [cs[0]], fill=(60, 200, 230), width=2)
            nh += 1
    im.save(out)
    print('placements', len(sel), 'triangles', ntri, 'boxes in frame', nocc, 'of', len(T['occluders']), 'hill boxes', nh)


if __name__ == '__main__':
    main()
