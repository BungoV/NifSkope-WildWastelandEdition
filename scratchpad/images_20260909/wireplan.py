"""Draw a terrain chunk's TRIANGULATION in plan, straight from the .BTR's own
bytes -- the renderer has no wireframe mode reachable headlessly (checked in
source: the only wire path is the selection outline, src/glview.cpp:7245), and
channel 8 photographs black on terrain because no .BTR ships vertex normals.

So the triangulation is drawn offline, from the same reader that measured far
terrain vertex spacing (scratchpad/btr_spacing_20260909/btrparse.py): every
triangle edge of the LAND shape, orthographic from above, both sides on the
SAME extent box so the two pictures are the same ground at the same scale.

  python wireplan.py <in.BTR> <out.png> <dim> <label>
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'btr_spacing_20260909'))
from btrparse import find_shapes, read_shape

SIZE = 900
BG = (18, 18, 20)
LINE = (206, 212, 220)


def font(px):
    for name in ('segoeuib.ttf', 'arialbd.ttf'):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    src, out, dim, label = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    data = open(src, 'rb').read()
    shapes = find_shapes(data)
    if not shapes:
        raise SystemExit('no BSTriShape found in %s' % src)
    # the LAND shape is the one with the most triangles; the other is WATER
    s = max(shapes, key=lambda z: z['numTris'])
    pos, tri = read_shape(data, s)
    world = pos * float(dim)                      # miniature space x the shape scale
    span = dim * 4096.0
    img = Image.new('RGB', (SIZE, SIZE), BG)
    d = ImageDraw.Draw(img)
    sx = (world[:, 0] / span) * (SIZE - 2) + 1
    sy = (SIZE - 2) - (world[:, 1] / span) * (SIZE - 2) + 1   # y up
    segs = set()
    for a, b, c in tri:
        for u, v in ((a, b), (b, c), (c, a)):
            segs.add((u, v) if u < v else (v, u))
    for u, v in segs:
        d.line([sx[u], sy[u], sx[v], sy[v]], fill=LINE, width=1)
    f = font(22)
    d.text((10, 8), '%s   %d verts  %d tris  %d edges' %
           (label, s['numVerts'], s['numTris'], len(segs)), font=f, fill=(255, 255, 255))
    img.save(out, optimize=True)
    print('%s  %dx%d  verts %d tris %d edges %d  span %.0f units' %
          (out, SIZE, SIZE, s['numVerts'], s['numTris'], len(segs), span))


if __name__ == '__main__':
    main()
