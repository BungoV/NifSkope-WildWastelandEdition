"""Pick a far mountain tile OUTSIDE the painted box, from VANILLA's shipped
far LOD only (never our own output -- that would be circular).

For each tile of the requested dim whose cell block lies wholly outside the
painted box x -36..32 / y -41..32, measure two things on vanilla's own sheets:

  colour  = mean over texels of (max(RGB) - min(RGB)), mip 2, on the diffuse.
            A blank/grey tile scores ~0; painted rock/dirt scores high.
  relief  = mean over texels of the model-space normal's tilt away from up,
            in degrees, from the _msn (up is GREEN in FO4).

Prints every tile sorted by colour*relief so one tile has BOTH.

  python pick_region.py [dim] [x0 y0 x1 y1]
"""
import os, sys, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
from dds import DDS
from bcnp import decode_rgb

TEX = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'

# the painted box, from lodgen --dump-layers (HANDOFF): x -36..32, y -41..32
PX0, PX1, PY0, PY1 = -36, 32, -41, 32


def outside(x, y, dim):
    """True when the whole block [x,x+dim-1] x [y,y+dim-1] misses the painted box."""
    return (x + dim - 1 < PX0 or x > PX1 or y + dim - 1 < PY0 or y > PY1)


def tiles(dim):
    for n in os.listdir(TEX):
        if not n.upper().endswith('.DDS') or '_msn' in n or '_data' in n:
            continue
        p = n.split('.')
        if len(p) < 5 or p[1] != str(dim):
            continue
        yield int(p[2]), int(p[3]), n[:-4]


def measure(stem):
    dp = os.path.join(TEX, stem + '.DDS')
    mp = os.path.join(TEX, stem + '_msn.DDS')
    if not (os.path.exists(dp) and os.path.exists(mp)):
        return None
    d = DDS(dp)
    rgb = decode_rgb(d, min(2, d.mips - 1)).astype(np.int16)
    colour = float((rgb.max(2) - rgb.min(2)).mean())
    lum = float(rgb.mean())
    sd = float(rgb.reshape(-1, 3).mean(1).std())
    m = DDS(mp)
    n = decode_rgb(m, min(2, m.mips - 1)).astype(np.float32) / 255.0 * 2.0 - 1.0
    up = np.clip(n[:, :, 1], -1.0, 1.0)          # FO4 terrain _msn: up is GREEN
    relief = float(np.degrees(np.arccos(up)).mean())
    return colour, sd, lum, relief


def main():
    dim = int(sys.argv[1]) if len(sys.argv) > 1 else 16
    box = [int(v) for v in sys.argv[2:6]] if len(sys.argv) > 5 else None
    rows = []
    for x, y, stem in tiles(dim):
        if box:
            if not (box[0] <= x <= box[2] and box[1] <= y <= box[3]):
                continue
        elif not outside(x, y, dim):
            continue
        r = measure(stem)
        if r is None:
            continue
        colour, sd, lum, relief = r
        rows.append((colour * relief, colour, sd, lum, relief, stem))
        sys.stderr.write('.')
        sys.stderr.flush()
    sys.stderr.write('\n')
    rows.sort(reverse=True)
    print('%-30s %8s %7s %7s %7s %7s' % ('tile', 'score', 'colour', 'lumSD', 'lum', 'tiltDeg'))
    for r in rows:
        print('%-30s %8.1f %7.2f %7.2f %7.1f %7.2f' % (r[5], r[0], r[1], r[2], r[3], r[4]))
    print('...%d tiles at dim %d measured' % (len(rows), dim))


if __name__ == '__main__':
    main()
