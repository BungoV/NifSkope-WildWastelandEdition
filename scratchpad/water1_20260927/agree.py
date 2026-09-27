#!/usr/bin/env python3
"""WATER1 follow-up 1: does the viewer's water-depth view agree with the chunk bake's
"Water depth (R)" vertex tint (lodgen.cpp: R = clamp((water - terrain) / 2048, 0, 1))?

  agree.py pick <btr dir> <out.json> [n]   water vertices from every .btr (world x,y + R byte), a spread
                                           sample of n with 0 < R < 255 (unsaturated), and the probe string
  agree.py score <in.json> <render log>    matches each vertex to the viewer's
                                           "water depth probe x,y: ... depth D units" line and prints numbers

The .btr is read with tests/spells/gltf_nifread.py (a reader that shares nothing with lodgen.cpp); the
colour bytes are read here from the same vertex walk. Positions are shape-local and taken to world
through the node chain (translation + uniform scale; a rotation is refused, not guessed).
"""
import glob
import json
import os
import re
import struct
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWWE-water1/tests/spells')
import gltf_nifread as G  # noqa: E402


def colour_offset(va):
    o = 0
    if va & G.VA_VERTEX:
        o += 16 if va & G.VA_FULLPREC else 8
    for flag in (G.VA_UV, G.VA_UV2, G.VA_NORMALS, G.VA_TANGENTS):
        if va & flag:
            o += 4
    return o


def shape_base(nif, sh):
    name, t, r, s, o = nif._avobject(nif.start[sh['block']])
    return o + 16 + 4 + 4 + 4 + 8 + 4 + 2 + 4


def to_world(nif, sh, v):
    x, y, z = v
    t, r, s = sh['t'], sh['r'], sh['s']
    node = sh['parent']
    chain = [(t, r, s)]
    while node is not None:
        n = nif.nodes[node]
        chain.append((n['t'], n['r'], n['s']))
        node = n['parent']
    for t, r, s in chain:
        if max(abs(r[0] - 1), abs(r[4] - 1), abs(r[8] - 1), abs(r[1]), abs(r[2]), abs(r[3]),
               abs(r[5]), abs(r[6]), abs(r[7])) > 1e-5:
            raise SystemExit('rotated node on a water shape: refused')
        x, y, z = t[0] + s * x, t[1] + s * y, t[2] + s * z
    return x, y, z


def pick(d, out, n):
    rows, shapes, files, sat, zero = [], 0, 0, 0, 0
    gba = {}   # the other three colour bytes: constant or not decides reclaim vs keep
    for p in sorted(glob.glob(os.path.join(d, '**', '*.btr'), recursive=True)):
        nif = G.Nif(p)
        files += 1
        for sh in nif.shapes.values():
            if not (sh['va'] & G.VA_COLORS):
                continue
            shapes += 1
            base, co = shape_base(nif, sh), colour_offset(sh['va'])
            for k, v in enumerate(sh['verts']):
                rb = nif.data[base + k * sh['stride'] + co]
                key = tuple(nif.data[base + k * sh['stride'] + co + 1:base + k * sh['stride'] + co + 4])
                gba[key] = gba.get(key, 0) + 1
                x, y, z = to_world(nif, sh, v)
                # a .btr's shapes are chunk-local: the chunk's SW cell is in its name (<ws>.<dim>.<x>.<y>.BTR)
                parts = os.path.basename(p).split('.')
                x, y = x + int(parts[2]) * 4096.0, y + int(parts[3]) * 4096.0
                if rb >= 255:
                    sat += 1
                elif rb == 0:
                    zero += 1
                rows.append((round(x, 2), round(y, 2), round(z, 2), rb, os.path.basename(p)))
    live = [r for r in rows if 0 < r[3] < 255]
    step = max(1, len(live) // n)
    sample = live[::step][:n]
    probe = ';'.join('%g,%g' % (r[0], r[1]) for r in sample)
    json.dump(dict(files=files, colour_shapes=shapes, vertices=len(rows), saturated=sat, zero=zero,
                   sample=sample, probe=probe), open(out, 'w'))
    print('files %d, water shapes with colours %d, vertices %d (R=0: %d, R=255: %d), sampled %d unsaturated'
          % (files, shapes, len(rows), zero, sat, len(sample)))
    print('distinct G,B,A byte triples over every water vertex: %s' % {str(k): v for k, v in gba.items()})


def score(js, log):
    J = json.load(open(js))
    got = {}
    pat = re.compile(r'water depth probe ([-\d.e+]+),([-\d.e+]+): body (\d+) water ([-\d.]+), ground ([-\d.]+).*?'
                     r'depth ([-\d.]+) units')
    for line in open(log, encoding='utf-8', errors='replace'):
        m = pat.search(line)
        if m:
            got[(float(m.group(1)), float(m.group(2)))] = (int(m.group(3)), float(m.group(4)), float(m.group(6)))
    diffs, nob, samewater = [], 0, []
    for x, y, z, rb, f in J['sample']:
        g = got.get((float('%g' % x), float('%g' % y)))
        if g is None:
            nob += 1
            continue
        bake = rb / 255.0 * 2048.0
        view = min(2048.0, max(0.0, g[2]))
        diffs.append(view - bake)
        samewater.append(abs(g[1] - z))
    if not diffs:
        print('no probe lines matched: REFUSED')
        return
    a = sorted(abs(d) for d in diffs)

    def q(p):
        return a[min(len(a) - 1, int(p * len(a)))]
    within = lambda t: sum(1 for d in a if d <= t) / len(a)
    print('vertices compared %d (no body at %d); |view - bake| median %.1f, 90%% %.1f, max %.1f units; '
          'within 8 (one R step) %.1f%%, within 64 %.1f%%; mean signed %.1f' %
          (len(a), nob, q(0.5), q(0.9), a[-1], 100 * within(8.1), 100 * within(64), sum(diffs) / len(diffs)))
    print('bake water height vs body water height: equal (<0.5) at %.1f%% of vertices, max apart %.1f' %
          (100 * sum(1 for s in samewater if s < 0.5) / len(samewater), max(samewater)))


if __name__ == '__main__':
    if sys.argv[1] == 'pick':
        pick(sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 300)
    else:
        score(sys.argv[2], sys.argv[3])
