"""Survey vanilla Commonwealth .BTR terrain LOD meshes: vertex attributes,
shader flags, triangle counts per LOD level, inside vs outside the textured
region measured by esmland.py."""
import os, re, sys, pickle, subprocess, collections, io
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import nifpeek

M = r'E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Terrain\Commonwealth'
cells = pickle.load(open(os.path.join(HERE, 'landinfo.pkl'), 'rb'))
textured = set(k for k, v in cells.items() if v['BTXT'] or v['ATXT'])

NAME = re.compile(r'^Commonwealth\.(\d+)\.(-?\d+)\.(-?\d+)\.BTR$', re.I)
files = [f for f in os.listdir(M) if NAME.match(f)]
print('BTR files:', len(files))
perlevel = collections.Counter()
coords = collections.defaultdict(set)
for f in files:
    m = NAME.match(f)
    lv, x, y = int(m.group(1)), int(m.group(2)), int(m.group(3))
    perlevel[lv] += 1
    coords[lv].add((x, y))
print('per level:', dict(sorted(perlevel.items())))
for lv in sorted(coords):
    xs = [c[0] for c in coords[lv]]
    ys = [c[1] for c in coords[lv]]
    span = (max(xs) - min(xs)) // lv + 1
    print('  level %2d: %4d tiles, x %4d..%4d y %4d..%4d, full grid would be %d' %
          (lv, len(coords[lv]), min(xs), max(xs), min(ys), max(ys), span * span))

# capture nifpeek output for a sample
import contextlib


def peek(path):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        try:
            nifpeek.main([path])
        except Exception as e:
            print('ERROR', e)
    return buf.getvalue()


ATTR = re.compile(r'attrs=(\S+)')
TRIS = re.compile(r'tris=(\d+) verts=(\d+)')
FL = re.compile(r'flags1=(0x[0-9A-Fa-f]+) flags2=(0x[0-9A-Fa-f]+)')

import random
random.seed(3)
print()
print('=== attribute / flag survey ===')
seenattr = collections.Counter()
seenflag = collections.Counter()
tri = collections.defaultdict(list)
for lv in sorted(coords):
    samp = random.sample(sorted(coords[lv]), min(25, len(coords[lv])))
    for (x, y) in samp:
        p = os.path.join(M, 'Commonwealth.%d.%d.%d.BTR' % (lv, x, y))
        out = peek(p)
        for a in ATTR.findall(out):
            seenattr[a] += 1
        for f in FL.findall(out):
            seenflag[f] += 1
        t = sum(int(a) for a, b in TRIS.findall(out))
        v = sum(int(b) for a, b in TRIS.findall(out))
        cellsInTile = lv * lv
        tri[lv].append((t, v, t / float(cellsInTile)))
print('vertex attribute sets seen:', dict(seenattr))
print('shader flag pairs seen    :', dict(seenflag))
print()
print('=== triangles per LOD level (25 tiles each) ===')
print('%-8s %-10s %-10s %-14s' % ('level', 'mean tris', 'mean verts', 'tris per CELL'))
for lv in sorted(tri):
    v = tri[lv]
    print('%-8d %-10.0f %-10.0f %-14.2f' % (lv, sum(a for a, b, c in v) / len(v),
                                            sum(b for a, b, c in v) / len(v),
                                            sum(c for a, b, c in v) / len(v)))

print()
print('=== level-4 BTR: inside vs outside the textured region ===')
lv = 4
IN = [c for c in coords[lv] if all((c[0] + dx, c[1] + dy) in textured
                                   for dx in range(4) for dy in range(4))]
OUT = [c for c in coords[lv] if not any((c[0] + dx, c[1] + dy) in textured
                                        for dx in range(4) for dy in range(4))]
print('fully textured tiles with a BTR: %d ; fully untextured tiles with a BTR: %d' % (len(IN), len(OUT)))
for nm, grp in (('TEXTURED', IN), ('UNTEXTURED', OUT)):
    samp = random.sample(sorted(grp), min(30, len(grp)))
    ts, vs = [], []
    attrs = collections.Counter()
    for (x, y) in samp:
        out = peek(os.path.join(M, 'Commonwealth.%d.%d.%d.BTR' % (lv, x, y)))
        ts.append(sum(int(a) for a, b in TRIS.findall(out)))
        vs.append(sum(int(b) for a, b in TRIS.findall(out)))
        for a in ATTR.findall(out):
            attrs[a] += 1
    print('%-12s n=%2d  mean tris %7.0f  mean verts %7.0f  attrs %s'
          % (nm, len(ts), sum(ts) / len(ts), sum(vs) / len(vs), dict(attrs)))
