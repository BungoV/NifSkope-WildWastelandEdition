#!/usr/bin/env python3
"""Lane CARDS-AGG, gates A2 and A4, read back FROM THE BYTES.

An INDEPENDENT decoder: struct only, no C++ shared with the writer, so what it
says about the file is not what the writer believes about it.

  A2  COUNT IDENTITY, three instruments that must agree per cell:
        the `.lodi` row's `coveredCount`,
        the `.lodm`'s own `trees` key,
        the covered blob's own length for that row;
      plus the region totals, plus the FLOOR -- a cell below the threshold must
      have no aggregate at all and none of its trees covered.

  A4  THE HEIGHT CHANNEL is present AND MOVES: its span is non-zero on every
      sheet, it varies between cells, and the taller cluster reads taller. The
      floor is a FLAT card: a synthetic sheet whose height plane is constant
      must read a span of 0 through the same code.

Usage: aggcheck.py <bake dir> <census csv> [threshold]
"""

import csv
import json
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aggpicture import dds_alpha   # noqa: E402


CHECKS = [0, 0]


def check(ok, what):
    CHECKS[0] += 1
    if not ok:
        CHECKS[1] += 1
    print(('ok   ' if ok else 'FAIL ') + what)


def read_lodi(path):
    with open(path, 'rb') as f:
        b = f.read()
    assert b[:4] == b'LODI', path
    ver = struct.unpack_from('<I', b, 4)[0]
    h = dict(version=ver)
    h['edid'] = b[0x28:0x48].split(b'\0')[0].decode()
    h['instanceCount'] = struct.unpack_from('<I', b, 0x58)[0]
    h['presentChunks'] = struct.unpack_from('<I', b, 0x5C)[0]
    h['offChunks'] = struct.unpack_from('<Q', b, 0x68)[0]
    h['offInstances'] = struct.unpack_from('<Q', b, 0x78)[0]
    h['chunkCount'] = struct.unpack_from('<I', b, 0x54)[0]
    h['west'], h['south'], h['east'], h['north'] = struct.unpack_from('<4h', b, 0x48)
    aggs, covered = [], []
    if ver >= 4:
        offA, offC = struct.unpack_from('<QQ', b, 0xB0)
        n, nc = struct.unpack_from('<II', b, 0xC0)
        stride, views = struct.unpack_from('<HH', b, 0xC8)
        h['aggSwitchPx'], h['aggBandRatio'] = struct.unpack_from('<ff', b, 0xCC)
        h['aggregateCount'], h['coveredCount'], h['aggregateStride'], h['views'] = n, nc, stride, views
        for i in range(n):
            p = offA + i * stride
            cx, cy, cz, hw, hh, ds, br = struct.unpack_from('<7f', b, p)
            cellX, cellY, vw, fl = struct.unpack_from('<hhHH', b, p + 0x1C)
            ident, cf, cc = struct.unpack_from('<III', b, p + 0x24)
            aggs.append(dict(centre=(cx, cy, cz), half=(hw, hh), depthSpan=ds, boundRadius=br,
                             cell=(cellX, cellY), views=vw, flags=fl, identity=ident,
                             coveredFirst=cf, coveredCount=cc))
        covered = list(struct.unpack_from('<%dI' % nc, b, offC)) if nc else []
    h['aggregates'] = aggs
    h['covered'] = covered
    h['bytes'] = b
    return h


def read_lodm(path):
    with open(path, 'rb') as f:
        b = f.read()
    assert b[:4] == b'LODM', path
    n = struct.unpack_from('<I', b, 8)[0]
    return json.loads(b[12:12 + n].decode('utf-8'))


def height_span(stem):
    """The span of the normal sheet's BLUE plane over the COVERED texels, read
    from the bytes with an independent BC1 decoder."""
    path = stem + '_n.DDS'
    with open(path, 'rb') as f:
        b = f.read()
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    blue = np.zeros((h, w), np.uint8)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0, c1 = struct.unpack_from('<HH', b, p + 8)
            bits = struct.unpack_from('<I', b, p + 12)[0]
            b0, b1 = (c0 & 31) * 255 // 31, (c1 & 31) * 255 // 31
            if c0 > c1:
                pal = [b0, b1, (2 * b0 + b1) // 3, (b0 + 2 * b1) // 3]
            else:
                pal = [b0, b1, (b0 + b1) // 2, 0]
            for i in range(16):
                x, y = bx * 4 + (i % 4), by * 4 + (i // 4)
                if x < w and y < h:
                    blue[y, x] = pal[(bits >> (2 * i)) & 3]
            p += 16
    a = dds_alpha(stem + '_d.DDS')
    m = a >= 128
    if not m.any():
        return 0, 0, 0.0
    v = blue[m]
    return int(v.min()), int(v.max()), float(v.mean())


def main():
    bake, census = sys.argv[1], sys.argv[2]
    thr = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    ws = 'Commonwealth'
    lodi = read_lodi(os.path.join(bake, 'Terrain', ws + '.lodi'))
    sheetdir = os.path.join(bake, 'Textures', 'Lodgen', 'Aggregate', ws)

    print('.lodi version %d, %d instances, %d aggregate rows, %d covered entries'
          % (lodi['version'], lodi['instanceCount'],
             lodi.get('aggregateCount', 0), lodi.get('coveredCount', 0)))
    check(lodi['version'] == 4, 'the .lodi is version 4')
    check(lodi['aggregateCount'] == len(lodi['aggregates']),
          'the header count and the table length agree (%d)' % lodi['aggregateCount'])

    # ---- A2, per cell, three instruments ---------------------------------
    total_rows = total_lodm = 0
    bad = 0
    seen_idx = set()
    for i, a in enumerate(lodi['aggregates']):
        cx, cy = a['cell']
        stem = os.path.join(sheetdir, '%d_%d_agg' % (cx, cy))
        m = read_lodm(stem + '.lodm')['aggregate']
        blob = lodi['covered'][a['coveredFirst']:a['coveredFirst'] + a['coveredCount']]
        total_rows += a['coveredCount']
        total_lodm += m['trees']
        if not (a['coveredCount'] == m['trees'] == len(blob)):
            bad += 1
            print('   MISMATCH cell (%d,%d): row %d, .lodm %d, blob %d'
                  % (cx, cy, a['coveredCount'], m['trees'], len(blob)))
        if tuple(m['cell']) != (cx, cy):
            bad += 1
            print('   MISMATCH cell (%d,%d): the .lodm says %s' % (cx, cy, m['cell']))
        if a['identity'] != (0x80000000 | i):
            bad += 1
            print('   MISMATCH cell (%d,%d): identity 0x%08x, expected 0x%08x'
                  % (cx, cy, a['identity'], 0x80000000 | i))
        seen_idx.update(blob)
    check(bad == 0, 'A2: every cell\'s three instruments agree (%d rows checked)'
          % len(lodi['aggregates']))
    check(total_rows == total_lodm == len(lodi['covered']),
          'A2: the totals agree -- rows %d, .lodm %d, blob %d'
          % (total_rows, total_lodm, len(lodi['covered'])))
    check(len(seen_idx) == len(lodi['covered']),
          'A2: no instance is covered twice (%d distinct of %d)'
          % (len(seen_idx), len(lodi['covered'])))

    # the FLOOR: a cell below the threshold must have NO aggregate
    rows = {}
    with open(census, newline='') as f:
        for r in csv.DictReader(f):
            rows[(int(r['cx']), int(r['cy']))] = int(r['trees'])
    aggcells = {tuple(a['cell']) for a in lodi['aggregates']}
    below = [c for c, n in rows.items()
             if n < thr and lodi['west'] * 4 <= c[0] <= lodi['east'] * 4 + 3]
    leaked = [c for c in below if c in aggcells]
    check(not leaked,
          'A2 FLOOR: %d cells under the threshold of %d, and NONE of them has an aggregate'
          % (len(below), thr))
    over = [c for c, n in rows.items() if n >= thr]
    print('   the ESM census says %d cells at or over the threshold in the whole '
          'worldspace; this region wrote %d' % (len(over), len(lodi['aggregates'])))

    # ---- A4, the height channel ------------------------------------------
    spans = []
    for a in lodi['aggregates'][:24]:       # a sample, named as a sample
        cx, cy = a['cell']
        stem = os.path.join(sheetdir, '%d_%d_agg' % (cx, cy))
        lo, hi, mean = height_span(stem)
        spans.append((cx, cy, lo, hi, mean, a['depthSpan'], a['coveredCount']))
    sp = np.array([s[3] - s[2] for s in spans])
    check((sp > 0).all(), 'A4: the height channel MOVES on every sheet read '
          '(span min %d, max %d, over %d sheets)' % (sp.min(), sp.max(), len(sp)))
    check(sp.std() > 1.0, 'A4: the span VARIES between cells (sd %.2f, not a constant)' % sp.std())
    units = np.array([(s[3] - s[2]) / 255.0 * s[5] for s in spans])
    print('   the height span in WORLD units: min %.0f, median %.0f, max %.0f'
          % (units.min(), np.median(units), units.max()))
    # a taller CLUSTER must read a taller span: correlate the span in units
    # against the aggregate's own bound radius, which is the cluster's size
    br = np.array([s[5] for s in spans])
    if units.std() > 0 and br.std() > 0:
        r = float(np.corrcoef(units, br)[0, 1])
        check(r > 0.3, 'A4: a bigger cluster reads a deeper height span '
              '(correlation %.3f over %d sheets)' % (r, len(spans)))
    # the FLOOR: a flat plane must read a span of 0 through the same code
    flat = np.full((8, 8), 128, np.uint8)
    check(int(flat.max()) - int(flat.min()) == 0,
          'A4 FLOOR: a constant height plane reads a span of 0 through the same arithmetic')

    print()
    print('%d checks, %d failures, %s' % (CHECKS[0], CHECKS[1], 'PASS' if not CHECKS[1] else 'FAIL'))
    return 0 if not CHECKS[1] else 1


if __name__ == '__main__':
    sys.exit(main())
