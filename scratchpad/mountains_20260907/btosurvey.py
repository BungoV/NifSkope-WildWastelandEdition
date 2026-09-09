"""How big does a vanilla Fallout 4 object-LOD chunk actually get? Vertex and
triangle totals per chunk, per LOD level, over the whole Commonwealth corpus —
used as a proxy for how many PLACED OBJECTS the worst chunk holds, which is
what the 16-bit identity index has to survive. Read-only. Lane IDENTITY."""
import os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nifpeek

SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape')

def shape_counts(path):
    hdr, ver, user, bsver, strings, blocks, b, endoff = nifpeek.parse(path)
    verts = tris = 0
    n = 0
    for i, t, off, sz in blocks:
        if t not in SHAPES:
            continue
        o = off
        nameidx, o = nifpeek.u32(b, o)
        nex, o = nifpeek.u32(b, o); o += 4 * nex
        ctrl, o = nifpeek.u32(b, o)
        flags, o = nifpeek.u32(b, o)
        o += 12 + 36 + 4
        coll, o = nifpeek.u32(b, o)
        o += 16
        skin, o = nifpeek.u32(b, o)
        shader, o = nifpeek.u32(b, o)
        alpha, o = nifpeek.u32(b, o)
        desc, o = nifpeek.u64(b, o)
        nt, o = nifpeek.u32(b, o)
        nv, o = nifpeek.u16(b, o)
        ds, o = nifpeek.u32(b, o)
        if ds != (desc & 0xF) * 4 * nv + nt * 6:
            raise ValueError('layout mismatch in %s' % path)
        verts += nv
        tris += nt
        n += 1
    return n, verts, tris

def main(root):
    per = collections.defaultdict(list)
    bad = 0
    for f in sorted(os.listdir(root)):
        if not f.lower().endswith('.bto'):
            continue
        parts = f.split('.')
        try:
            lvl = int(parts[1])
        except (IndexError, ValueError):
            continue
        p = os.path.join(root, f)
        try:
            n, v, t = shape_counts(p)
        except Exception:
            bad += 1
            continue
        per[lvl].append((v, t, n, os.path.getsize(p), f))
    print('unparsed: %d' % bad)
    print('%-5s %6s %10s %10s %10s %10s   %s' %
          ('lvl', 'chunks', 'maxVerts', 'meanVerts', 'maxTris', 'totBytes', 'worst chunk'))
    for lvl in sorted(per):
        rows = per[lvl]
        rows.sort(reverse=True)
        v = [r[0] for r in rows]
        t = [r[1] for r in rows]
        tot = sum(r[3] for r in rows)
        print('%-5d %6d %10d %10d %10d %10d   %s (shapes %d, %d bytes)' %
              (lvl, len(rows), max(v), sum(v) // len(v), max(t), tot,
               rows[0][4], rows[0][2], rows[0][3]))
    print()
    print('top 10 chunks by vertex count, all levels:')
    allrows = [(r[0], r[1], r[2], r[3], r[4], lvl) for lvl in per for r in per[lvl]]
    allrows.sort(reverse=True)
    for v, t, n, sz, f, lvl in allrows[:10]:
        print('   %7d verts %7d tris %2d shapes %9d bytes  %s' % (v, t, n, sz, f))

if __name__ == '__main__':
    main(sys.argv[1])
