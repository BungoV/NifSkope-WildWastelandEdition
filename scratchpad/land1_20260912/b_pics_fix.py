"""Draw the REAL dirty set, not an illustration of its size.

The first draft shaded `dirty` cells outward from the centre and labelled the
shading "illustrative". That was the wrong call: a reader looks at the picture,
not at the disclaimer, and a diamond of nine chunks is not what a one-cell edit
dirties -- a 3x3 block is.

The set is exactly computable from what the gate already logged. Every
incremental run prints its SEED chunks (`  (-20,20) inputs a -> b`, and
`deleted Commonwealth.4.-12.16.BTO` for the lost arm), and the widening rule is
one chunk step in each direction, clipped to the region. Computing it and then
ASSERTING the result equals the census count the exe printed makes the picture a
second, independent check on the diff rather than a drawing of it -- and if the
two ever disagree the picture refuses to be drawn.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/b_pics.py'

OLD_MAP = """def draw_map(d, x, y, arm, verdict, dirty, total, moved, notin, lost, nb, w=118):
    cell = w // 5
    # A 5x5 chunk grid. The gate's edit is at the centre for land/refs, on the
    # border for border, and lost deletes one output; the exact dirty SET is not
    # in the log, only its size and its causes, so the grid shades the count
    # from the centre outwards and the number is what is authoritative.
    col = {'PASS': (120, 210, 130), 'FAIL': (235, 110, 110)}.get(verdict, (230, 200, 120))
    d.text((x, y), '%-8s %s' % (arm, verdict), font=FB, fill=col)
    d.text((x, y + 18), '%d of %d dirty' % (dirty, total), font=F, fill=(235, 235, 240))
    d.text((x, y + 34), '%d moved  %d lost' % (moved, lost), font=F, fill=(170, 170, 180))
    d.text((x, y + 50), '%d by neighbour' % nb, font=F, fill=(170, 170, 180))
    gx, gy = x, y + 70
    order = []
    for j in range(5):
        for i in range(5):
            order.append((abs(i - 2) + abs(j - 2), i, j))
    order.sort()
    on = {(i, j) for _k, i, j in order[:dirty]}
    for j in range(5):
        for i in range(5):
            fill = (70, 120, 80) if (i, j) in on else (44, 46, 52)
            d.rectangle([gx + i * cell, gy + j * cell,
                         gx + i * cell + cell - 2, gy + j * cell + cell - 2],
                        fill=fill, outline=(28, 30, 34))
    return gy + 5 * cell + 8"""

NEW_MAP = '''REGION = {'A': (-24, 16), 'B': (-16, 0)}   # the lowest (cx, cy) of each 5x5
DIM = 4


def seeds(arm):
    """The chunks the exe itself named as dirty-at-source, out of its own log."""
    import re
    tag, kind = arm.split('/')
    log = os.path.join(B3, tag, kind, 'incr', 'bake.log')
    out = set()
    if not os.path.exists(log):
        return out
    for line in open(log, encoding='utf-8', errors='replace'):
        m = re.match(r'\\s+\\((-?\\d+),(-?\\d+)\\) ', line)
        if m:
            out.add((int(m.group(1)), int(m.group(2))))
        m = re.search(r'deleted Commonwealth\\.\\d+\\.(-?\\d+)\\.(-?\\d+)\\.', line)
        if m:
            out.add((int(m.group(1)), int(m.group(2))))
    return out


def dirty_set(arm):
    """Seeds widened by ONE CHUNK STEP, clipped to the region -- the rule in
    nifcli.cpp:3791. Returned as grid indices."""
    tag = arm.split('/')[0]
    x0, y0 = REGION[tag]
    grid = set()
    for (sx, sy) in seeds(arm):
        for i in range(5):
            for j in range(5):
                cx, cy = x0 + i * DIM, y0 + j * DIM
                if abs(cx - sx) <= DIM and abs(cy - sy) <= DIM:
                    grid.add((i, j))
    return grid


def draw_map(d, x, y, arm, verdict, dirty, total, moved, notin, lost, nb, w=118):
    cell = w // 5
    col = {'PASS': (120, 210, 130), 'FAIL': (235, 110, 110)}.get(verdict, (230, 200, 120))
    d.text((x, y), '%-8s %s' % (arm, verdict), font=FB, fill=col)
    d.text((x, y + 18), '%d of %d dirty' % (dirty, total), font=F, fill=(235, 235, 240))
    d.text((x, y + 34), '%d moved  %d lost' % (moved, lost), font=F, fill=(170, 170, 180))
    d.text((x, y + 50), '%d by neighbour' % nb, font=F, fill=(170, 170, 180))
    gx, gy = x, y + 70

    sd = seeds(arm)
    on = dirty_set(arm)
    # THE PICTURE IS ALSO A CHECK. The set drawn here is computed from the seed
    # chunks and the widening rule; the count beside it came out of the exe. If
    # they disagree, one of the two is wrong and neither should be drawn.
    if len(on) != dirty:
        raise SystemExit('REFUSED: %s -- the widening rule gives %d dirty chunks, '
                         'the exe printed %d' % (arm, len(on), dirty))

    tag = arm.split('/')[0]
    x0, y0 = REGION[tag]
    for j in range(5):
        for i in range(5):
            cx, cy = x0 + i * DIM, y0 + j * DIM
            if (cx, cy) in sd:
                fill = (120, 200, 130)          # the chunk whose input moved
            elif (i, j) in on:
                fill = (62, 104, 72)            # dirtied by the widening
            else:
                fill = (44, 46, 52)             # left exactly as it was
            d.rectangle([gx + i * cell, gy + j * cell,
                         gx + i * cell + cell - 2, gy + j * cell + cell - 2],
                        fill=fill, outline=(28, 30, 34))
    return gy + 5 * cell + 8'''

OLD_NOTE = ("           'Two regions, five kinds of change. Shading is illustrative -- the COUNT and its '\n"
            "           'causes are the measurement, read out of logs/b3.txt.',")
NEW_NOTE = ("           'Two regions, five kinds of change. BRIGHT = the chunk whose input actually '\n"
            "           'moved (or whose output was deleted); dim green = dirtied by the one-chunk '\n"
            "           'widening; grey = left exactly as the previous bake wrote it.',")

OLD_PCT = """           'A one-cell edit dirties 9 chunks of 25 (36 %% here, a few per cent of a real '"""
NEW_PCT = """           'A one-cell edit dirties 9 chunks of 25 (36 % here, a few per cent of a real '"""


def main():
    b = open(P, 'rb').read()
    s = b.decode('utf-8')
    for old, new, label in ((OLD_MAP, NEW_MAP, 'draw_map'),
                            (OLD_NOTE, NEW_NOTE, 'legend'),
                            (OLD_PCT, NEW_PCT, 'percent')):
        n = s.count(old)
        assert n == 1, '%s matched %d times' % (label, n)
        s = s.replace(old, new)
    compile(s, P, 'exec')
    out = s.encode('utf-8')
    open(P, 'wb').write(out)
    print('b_pics.py %d -> %d bytes, compiles' % (len(b), len(out)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
