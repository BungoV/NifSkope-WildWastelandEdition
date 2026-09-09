"""The picture: recovered material map over the whole 192x192 extent.

Painted cells are drawn in their TRUE dominant material and unpainted cells in
the recovered one, in the same colour scheme, so the two can be compared
directly -- if the recovery were sound the painted blob would not stand out as a
differently-textured island.

4 px a cell, so each quadrant is a 2x2 block and per-quadrant structure is
visible.  The painted region's boundary is outlined in white.  Colours are
assigned by golden-ratio hue spacing within a per-family base hue, so materials
of the same family look related and different families do not collide.

Also writes recovered_conf.png -- the calibrated confidence, because a map of
the assignment without a map of how much to believe it would be misleading.
"""
import os
import struct
import sys
import zlib
from collections import Counter

import numpy as np

from rec_common import HERE, MINX, MINY, N, NULL, parse_layers, ltex_names
from rec_alt import family_of

PER = 4
FAMHUE = {'ocean': 0.55, 'coast-sand': 0.12, 'coast-rock': 0.08,
          'riverbed-rock': 0.62, 'riverbed-silt': 0.48, 'marsh': 0.30,
          'glowingsea': 0.85, 'blastedforest': 0.05, 'nfoothills': 0.72,
          'nf-farharbor': 0.42, 'grass-dry': 0.18, 'forest-floor': 0.25,
          'rubble-debris': 0.00, 'dirt-gravel': 0.09, 'none': 0.0,
          'other': 0.78}


def hsv(h, s, v):
    i = int(h * 6) % 6
    f = h * 6 - int(h * 6)
    p, q, t = v * (1 - s), v * (1 - f * s), v * (1 - (1 - f) * s)
    r, g, b = [(v, t, p), (q, v, p), (p, v, t),
               (p, q, v), (t, p, v), (v, p, q)][i]
    return int(r * 255), int(g * 255), int(b * 255)


def write_png(path, w, h, rgb):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgb[y * w * 3:(y + 1) * w * 3]

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 6))
    png += chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def main():
    names = ltex_names()
    painted, blends = parse_layers()
    z = np.load(os.path.join(HERE, 'recovered.npz'))
    grid, conf, tgt = z['grid'], z['conf'], z['tgt']

    # true dominant material per painted quadrant
    truth = np.full((N, N, 4), -1, dtype=np.int64)
    tz = np.load(os.path.join(HERE, 'training.npz'))
    Wt, XYt, lt = tz['W'], tz['XY'], tz['ltex']
    domt = lt[np.argmax(Wt, axis=1)]
    for i, (cx, cy, q) in enumerate(XYt):
        truth[cy - MINY, cx - MINX, q] = domt[i]

    allm = sorted(set(np.unique(grid[grid >= 0]).tolist())
                  | set(np.unique(truth[truth >= 0]).tolist()))
    byfam = {}
    for m in allm:
        byfam.setdefault(family_of(names.get(int(m), ('',))[0]), []).append(m)
    colour = {}
    for fm, ms in byfam.items():
        base = FAMHUE.get(fm, 0.78)
        for i, m in enumerate(sorted(ms)):
            colour[m] = hsv((base + 0.037 * i) % 1.0,
                            0.45 + 0.12 * (i % 4), 0.55 + 0.10 * (i % 3))
    colour[NULL] = (40, 40, 40)

    W = H = N * PER
    img = bytearray(b'\x00' * (W * H * 3))
    # quadrant -> (row offset, col offset) in a 2x2 layout, north up
    QO = {2: (0, 0), 3: (0, 1), 0: (1, 0), 1: (1, 1)}
    for r in range(N):
        for c in range(N):
            for q in range(4):
                m = truth[r, c, q] if painted[r, c] else grid[r, c, q]
                if m < 0:
                    m = NULL if painted[r, c] else -1
                col = colour.get(int(m), (25, 25, 25)) if m >= 0 else (25, 25, 25)
                qr, qc = QO[q]
                # world row 0 = SOUTH in our arrays; draw +y NORTH up
                y0 = (N - 1 - r) * PER + qr * (PER // 2)
                x0 = c * PER + qc * (PER // 2)
                for yy in range(PER // 2):
                    o = ((y0 + yy) * W + x0) * 3
                    for xx in range(PER // 2):
                        img[o + xx * 3:o + xx * 3 + 3] = bytes(col)

    # outline the painted region in white
    for r in range(N):
        for c in range(N):
            if not painted[r, c]:
                continue
            edge = False
            for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                rr, cc = r + dr, c + dc
                if not (0 <= rr < N and 0 <= cc < N) or not painted[rr, cc]:
                    edge = True
            if not edge:
                continue
            y0 = (N - 1 - r) * PER
            x0 = c * PER
            for yy in range(PER):
                for xx in range(PER):
                    if yy in (0, PER - 1) or xx in (0, PER - 1):
                        o = ((y0 + yy) * W + x0 + xx) * 3
                        img[o:o + 3] = b'\xff\xff\xff'

    write_png(os.path.join(HERE, 'recovered_map.png'), W, H, img)
    print('wrote recovered_map.png  %dx%d  (%d materials drawn)' % (W, H, len(colour)))

    # confidence map -- uses the JOINT (distance, vote share) calibration from
    # rec_calib.py when it exists, because the vote-share-only number massively
    # overstates reliability far from painted terrain
    cp = os.path.join(HERE, 'recovered_conf.npz')
    if os.path.exists(cp):
        conf = np.load(cp)['conf']
        print('confidence map uses the joint calibration (rec_calib.py)')
    cg = np.zeros((N, N), dtype=np.float32)
    for i, (rr, cc, q) in enumerate(tgt):
        cg[rr, cc] += conf[i] / 4.0
    img2 = bytearray(b'\x00' * (W * H * 3))
    for r in range(N):
        for c in range(N):
            if painted[r, c]:
                col = (255, 255, 255)
            else:
                v = float(np.clip(cg[r, c] / 0.35, 0, 1))
                col = hsv(0.0 + 0.33 * v, 0.85, 0.35 + 0.55 * v)
            y0 = (N - 1 - r) * PER
            x0 = c * PER
            for yy in range(PER):
                o = ((y0 + yy) * W + x0) * 3
                for xx in range(PER):
                    img2[o + xx * 3:o + xx * 3 + 3] = bytes(col)
    write_png(os.path.join(HERE, 'recovered_conf.png'), W, H, img2)
    print('wrote recovered_conf.png (white = painted; red low -> green high, '
          'full green = 0.35 calibrated accuracy)')

    cnt = Counter()
    for i, (rr, cc, q) in enumerate(tgt):
        cnt[int(grid[rr, cc, q])] += 1
    tot = sum(cnt.values())
    with open(os.path.join(HERE, 'recovered_legend.txt'), 'w') as f:
        f.write('# material assigned out of bounds, by area\n')
        f.write('# formid   quadrants   share   rgb        EDID / texture\n')
        for m, n in cnt.most_common():
            ed, tx = names.get(int(m), ('?', None))
            f.write('%08x %9d %7.2f%%  %-14s %-34s %s\n'
                    % (m, n, 100.0 * n / tot, str(colour.get(int(m))), ed, tx))
    print('wrote recovered_legend.txt')
    print('\ntop assigned materials out of bounds:')
    for m, n in cnt.most_common(12):
        ed, tx = names.get(int(m), ('?', None))
        print('  %08x %-34s %7.2f%%  %s' % (m, ed, 100.0 * n / tot, tx))


if __name__ == '__main__':
    sys.exit(main())
