"""Lane NATIVE0 audit: the spec's numbers against the tree as it is today.

Measures, without the exe:
  1. bytes per placement per level on the FO4CS sample set (today's emitter:
     merge, far-ring simplify, cards, arrays) against the spec's 682 B anchor;
  2. the identity drop: distinct identity indices in the .BTO vertex colours
     against manifest rows, on the dim-32 sample (the bucket-cap case);
  3. manifest scale range and distinct bases (the two u16 ceilings);
  4. Monte Carlo on the record's precision claims (quaternion 2+3x15, oct 12:12);
  5. the spec's src/lodgen.cpp line anchors re-grepped.

Run from the repo root:  python scratchpad/native0_20260910/audit.py
"""
import glob
import math
import os
import random
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SAMPLES = os.path.join(ROOT, 'scratchpad', 'handoff_fo4cs', 'samples')


def manifest_rows(path):
    rows = []
    for line in open(path, encoding='utf-8'):
        t = line.split()
        if t and t[0].isdigit():
            rows.append(t)
    return rows


# ---------------------------------------------------------------- NIF reader
def read_nif_blocks(path):
    b = open(path, 'rb').read()
    o = b.index(b'\n') + 1
    ver, endian, uver, nblocks, bsver = struct.unpack_from('<IBIII', b, o)
    o += 17
    assert ver == 0x14020007 and bsver == 130, (hex(ver), bsver)

    def short_string():
        nonlocal o
        n = b[o]
        o += 1 + n

    short_string(); short_string(); short_string()
    if bsver >= 130:
        short_string()
    ntypes = struct.unpack_from('<H', b, o)[0]
    o += 2
    types = []
    for _ in range(ntypes):
        n = struct.unpack_from('<I', b, o)[0]
        types.append(b[o + 4:o + 4 + n].decode())
        o += 4 + n
    tidx = struct.unpack_from('<%dH' % nblocks, b, o)
    o += 2 * nblocks
    sizes = struct.unpack_from('<%dI' % nblocks, b, o)
    o += 4 * nblocks
    nstr, _maxlen = struct.unpack_from('<II', b, o)
    o += 8
    for _ in range(nstr):
        n = struct.unpack_from('<I', b, o)[0]
        o += 4 + n
    ngroups = struct.unpack_from('<I', b, o)[0]
    o += 4 + 4 * ngroups
    blocks = []
    for i in range(nblocks):
        blocks.append((types[tidx[i]], o, sizes[i]))
        o += sizes[i]
    return b, blocks


def trishape_identity_ids(path):
    """Distinct identity indices (R + G*256) over every BSTriShape-derived
    block, read through the vertex descriptor, never a remembered offset."""
    b, blocks = read_nif_blocks(path)
    ids = set()
    verts = tris = 0
    shapes = 0
    for typ, off, size in blocks:
        if typ not in ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape'):
            continue
        shapes += 1
        o = off + 4                                   # name
        nextra = struct.unpack_from('<I', b, o)[0]
        o += 4 + 4 * nextra + 4                       # extra refs, controller
        o += 4 + 12 + 36 + 4 + 4                      # flags, T, R, scale, collision
        o += 16                                       # bound sphere
        o += 12                                       # skin, shader, alpha
        desc = struct.unpack_from('<Q', b, o)[0]
        o += 8
        ntri, nv, dsize = struct.unpack_from('<IHI', b, o)
        o += 10
        stride = (desc & 0xF) * 4
        flags = desc >> 44
        assert dsize == nv * stride + ntri * 6, (dsize, nv, stride, ntri)
        if flags & 0x20:
            coff = (desc >> 22) & 0x3C
            for v in range(nv):
                p = o + v * stride + coff
                ids.add(b[p] + b[p + 1] * 256)
        verts += nv
        tris += ntri
    return ids, shapes, verts, tris


# ------------------------------------------------------------- precision MC
def quat_from_axis_angle(ax, ang):
    s = math.sin(ang / 2)
    n = math.sqrt(sum(a * a for a in ax))
    return (math.cos(ang / 2), ax[0] / n * s, ax[1] / n * s, ax[2] / n * s)


def pack_quat(q, bits):
    """Smallest-three: drop the largest |component|, keep its sign positive,
    map the rest from [-1/sqrt2, 1/sqrt2] onto `bits` bits. Returns decoded q."""
    i = max(range(4), key=lambda k: abs(q[k]))
    if q[i] < 0:
        q = tuple(-c for c in q)
    steps = (1 << bits) - 1
    r = math.sqrt(0.5)
    out = [0.0] * 4
    ss = 0.0
    for k in range(4):
        if k == i:
            continue
        u = round((q[k] / r + 1.0) * 0.5 * steps)
        u = max(0, min(steps, u))
        c = (u / steps * 2.0 - 1.0) * r
        out[k] = c
        ss += c * c
    out[i] = math.sqrt(max(0.0, 1.0 - ss))
    return tuple(out)


def quat_angle(a, b):
    d = abs(sum(x * y for x, y in zip(a, b)))
    return 2.0 * math.acos(min(1.0, d))


def oct_encode(n, bits):
    x, y, z = n
    s = abs(x) + abs(y) + abs(z)
    u, v = x / s, y / s
    if z < 0:
        u, v = (1 - abs(v)) * (1 if u >= 0 else -1), (1 - abs(u)) * (1 if v >= 0 else -1)
    steps = (1 << bits) - 1
    qu = round((u + 1) * 0.5 * steps)
    qv = round((v + 1) * 0.5 * steps)
    u = qu / steps * 2 - 1
    v = qv / steps * 2 - 1
    z = 1 - abs(u) - abs(v)
    if z < 0:
        u, v = (1 - abs(v)) * (1 if u >= 0 else -1), (1 - abs(u)) * (1 if v >= 0 else -1)
    l = math.sqrt(u * u + v * v + z * z)
    return (u / l, v / l, z / l)


def main():
    print('== 1. bytes per placement, sample set (0,0) region, today\'s emitter')
    total_bto = total_rows = 0
    scales = []
    bases = set()
    scol = plain = 0
    for d in (4, 8, 16, 32):
        bto = os.path.join(SAMPLES, 'L%d' % d, 'Commonwealth.%d.0.0.BTO' % d)
        man = bto + '.manifest.txt'
        if not os.path.exists(bto):
            print('  L%d: missing' % d)
            continue
        rows = manifest_rows(man)
        nb = os.path.getsize(bto)
        print('  dim %2d: %10d B / %6d rows = %7.1f B a placement' % (d, nb, len(rows), nb / max(1, len(rows))))
        total_bto += nb
        total_rows += len(rows)
        for t in rows:
            scales.append(float(t[6]))
            bases.add(t[1])
            if int(t[10]) >= 0:
                scol += 1
            else:
                plain += 1
    print('  four rings: %d B / %d rows = %.1f B a placement; spec anchor 682 B'
          % (total_bto, total_rows, total_bto / max(1, total_rows)))
    print('  manifest scale range %.4f .. %.4f (spec [pri] corpus 0.010 .. 4.970; u16/8192 ceiling 7.99988)'
          % (min(scales), max(scales)))
    print('  distinct bases in the four samples: %d (spec: 3,400 worldspace; u16 ceiling 65,535)' % len(bases))
    print('  SCOL parts %d, plain refs %d (%.1f%% SCOL)' % (scol, plain, 100.0 * scol / max(1, scol + plain)))

    print('== 2. identity drop: distinct ids in vertex colours vs manifest rows')
    for d in (16, 32):
        bto = os.path.join(SAMPLES, 'L%d' % d, 'Commonwealth.%d.0.0.BTO' % d)
        if not os.path.exists(bto):
            continue
        ids, shapes, verts, tris = trishape_identity_ids(bto)
        rows = manifest_rows(bto + '.manifest.txt')
        rowids = set(int(t[0]) for t in rows)
        missing = rowids - ids
        print('  dim %d (0,0): %d shapes, %d verts, %d tris; %d ids in colours, %d rows, %d rows with no geometry (%.2f%%)'
              % (d, shapes, verts, tris, len(ids), len(rows), len(missing), 100.0 * len(missing) / max(1, len(rows))))
        if missing:
            print('    first missing index %d, last %d' % (min(missing), max(missing)))
    print('  (spec: (-32,0) dim 32 = 2,628 of 42,560 = 6.17%; (0,-32) = 882 of 42,641 = 2.07%; this sample is (0,0))')

    print('== 3. record precision, Monte Carlo, seed 20260910')
    random.seed(20260910)
    N = 100000
    worst15 = mean15 = worst10 = mean10 = 0.0
    for _ in range(N):
        ax = (random.gauss(0, 1), random.gauss(0, 1), random.gauss(0, 1))
        q = quat_from_axis_angle(ax, random.uniform(0, math.pi))
        e15 = quat_angle(q, pack_quat(q, 15))
        e10 = quat_angle(q, pack_quat(q, 10))
        worst15 = max(worst15, e15); mean15 += e15
        worst10 = max(worst10, e10); mean10 += e10
    print('  quaternion 2+3x15: worst %.4f deg, mean %.5f deg (spec worst 0.0146 deg)'
          % (math.degrees(worst15), math.degrees(mean15 / N)))
    print('  quaternion 2+3x10: worst %.4f deg, mean %.5f deg (spec worst 0.471 deg)'
          % (math.degrees(worst10), math.degrees(mean10 / N)))
    worst12 = mean12 = worst8 = mean8 = 0.0
    for _ in range(N):
        v = (random.gauss(0, 1), random.gauss(0, 1), random.gauss(0, 1))
        l = math.sqrt(sum(c * c for c in v))
        n = tuple(c / l for c in v)
        for bits, acc in ((12, 'w12'), (8, 'w8')):
            m = oct_encode(n, bits)
            e = math.acos(min(1.0, sum(a * b for a, b in zip(n, m))))
            if bits == 12:
                worst12 = max(worst12, e); mean12 += e
            else:
                worst8 = max(worst8, e); mean8 += e
    print('  oct 12:12: worst %.4f deg, mean %.4f deg (spec 0.0591 / 0.0209)' % (math.degrees(worst12), math.degrees(mean12 / N)))
    print('  oct  8:8 : worst %.4f deg, mean %.4f deg (spec 0.946 / 0.335)' % (math.degrees(worst8), math.degrees(mean8 / N)))
    print('  position: 16384 / 65535 = %.5f u step (spec 0.250); scale 1/8192 = %.4e (spec 1.221e-4)' % (16384 / 65535, 1 / 8192))
    print('  seed: treeHash %% 360 needs %d bits, mirror 1 bit; the u8 seed holds 8' % math.ceil(math.log2(360)))

    print('== 4. spec anchors in src/lodgen.cpp today')
    src = open(os.path.join(ROOT, 'src', 'lodgen.cpp'), encoding='utf-8', errors='replace').read().split('\n')
    anchors = [
        ('3144', 'bucket full', 'continue;   // bucket full'),
        ('2955-2964', 'treeHash', 'treeHash = ( quint32( qRound( r.pos[0] ) ) * 2654435761U )'),
        ('~2981', 'localMaxDist', 'localMaxDist = qMax( localMaxDist, lp.length() );'),
        ('2915', 'slot fallback', 'if ( model.isEmpty() && opts.slotFallback ) {'),
        ('3027', 'manifest row', 'manifest.append( QString( "%1 %2 %3 %4 %5 %6 %7 %8 %9" )'),
        ('3033', 'column 9 = localMaxDist * scale', '.arg( double( localMaxDist * r.scale ) )'),
        ('3481', 'manifest header', '# lodgen manifest 2 ws %1 dim %2 chunk %3 %4'),
        ('2881', 'buckets map', 'QMap<QString, ObjBucket> buckets;'),
        ('4237', 'UV2.y layer write', 'HalfVector2( Vector2( uv2[0], float( lit.value().second ) ) )'),
    ]
    for spec_line, what, text in anchors:
        hits = [i + 1 for i, l in enumerate(src) if text in l]
        print('  spec :%-10s %-32s -> %s' % (spec_line, what, ','.join(map(str, hits)) if hits else 'MISSING'))
    print('  src/lodgen.cpp lines today: %d (spec read 8286 on 2026-09-09)' % len(src))


if __name__ == '__main__':
    main()
