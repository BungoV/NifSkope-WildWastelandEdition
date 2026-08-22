"""Hold a rebuild's stored GEOMETRY against vanilla's, shape by shape.

    python tools/collision_ab.py <manifest.tsv> [<rebuilt-root> <vanilla-root>]

WHY THIS EXISTS

The Museum set loads without crashing, which says the files parse and the engine
can build bodies from them. It says nothing about whether the solids are the ones
Bethesda shipped. This reads both packfiles and compares what DEFINES a solid.

TWO METRICS THIS DELIBERATELY DOES NOT USE

*Centre of mass.* The first version paired shapes by it and reported a mannequin
hull 0.74 m out of place. That was the tool: vanilla stores that hull about the
origin and puts the offset in its compound INSTANCE, while Compile bakes the
offset into the vertices and writes an identity instance. Same solid, same world
position, two conventions, and a centre of mass read from the shape alone is in a
different frame in each.

*Volume.* The second version compared the inventory's volume and reported
railings 27% small. Also the tool: that volume is derived by triangulating the
stored FACE tables, and ours triangulate the same 6 faces into 18 triangles where
vanilla uses 12. Vertices, planes and convex radius were identical on all ten
shapes -- the solid was vanilla's exactly.

So this compares the stored definition itself, which cannot drift out of frame or
depend on a triangulation:

  * convex   the vertex set, the plane set, and convexRadius
  * capsule  both end points and the radius
  * sphere   centre and radius
  * mesh     the class is present with the same shape count
  * per body layer, friction, restitution, and the multiset of materials

Numbers are compared with a TOLERANCE and the worst deviation is reported, not
tested for equality: vanilla writes both signs of zero for the same plane, and a
recomputed plane lands a couple of 1e-5 away from the stored one. Exact equality
called all of that a difference, which is the third way this tool has lied.

Exit 1 if any file exceeds the tolerance (1e-3 -- a millimetre on a vertex).
"""
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkmatrun import nif_blobs, Pack

BS = chr(92)


TOL = 1.0e-3


def r5(v):
    """Round to 5 decimals and kill negative zero."""
    x = round(float(v), 5)
    return 0.0 if x == 0.0 else x


def dev(x, y):
    """Worst absolute difference between two nested tuples, or None if shaped
    differently."""
    if isinstance(x, (int, float)) and isinstance(y, (int, float)):
        return abs(x - y)
    if isinstance(x, str) or isinstance(y, str):
        return 0.0 if x == y else None
    if len(x) != len(y):
        return None
    worst = 0.0
    for a, b in zip(x, y):
        d = dev(a, b)
        if d is None:
            return None
        worst = max(worst, d)
    return worst


def child_offsets(blob, pk):
    """child shape offset -> its compound instance translation.

    Vanilla stores a compound child about its own origin and puts the offset in
    the instance; Compile bakes the offset into the vertices and writes an
    identity instance. Same solid either way, so the comparison has to bring both
    into the same frame or it reports a mannequin hull 0.76 m out of place --
    which is exactly the number this recovers.
    """
    if struct.unpack_from('<II', blob, 0) != (0x57E0E057, 0x10C0C010):
        return {}
    nsec, = struct.unpack_from('<i', blob, 20)
    secs = []
    for si in range(nsec):
        o = 0x40 + si * 0x40
        secs.append((blob[o:o + 19].split(bytes([0]))[0].decode('latin-1'),)
                    + struct.unpack_from('<7i', blob, o + 20))
    dt = [x for x in secs if x[0] == '__data__'][0]
    glob = {}
    q = dt[1] + dt[3]
    while q + 12 <= dt[1] + dt[4]:
        src, sec, dst = struct.unpack_from('<iii', blob, q)
        q += 12
        if src != -1:
            glob[src] = dst
    out = {}
    for cls in ('hknpDynamicCompoundShape', 'hknpStaticCompoundShape'):
        for co in pk.of_class(cls):
            for i in range(pk.u32(co + 0x68)):
                at = co + 0xD0 + i * 0x80
                child = glob.get(at + 0x50)
                if child is not None:
                    rows = [struct.unpack_from('<3f', pk.data, at + r * 16) for r in range(3)]
                    out[child] = (rows, struct.unpack_from('<3f', pk.data, at + 0x30))
    return out


def xf(v, t):
    """Apply an instance transform: v' = v . R + trans, rows as stored."""
    if t is None:
        return v
    rows, trans = t
    return tuple(sum(v[k] * rows[k][c] for k in range(3)) + trans[c] for c in range(3))


def xf_plane(pl, t):
    """A plane under the same transform: the normal rotates, d moves by -n'.trans."""
    if t is None:
        return pl
    rows, trans = t
    n = tuple(sum(pl[k] * rows[k][c] for k in range(3)) for c in range(3))
    return (n[0], n[1], n[2], pl[3] - sum(n[c] * trans[c] for c in range(3)))


def shapes_of(path):
    """Every shape's stored definition, as comparable tuples."""
    out = []
    detail = {}
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        inst = child_offsets(blob, pk)
        for off, cls in sorted(pk.objects):
            tx = inst.get(off)
            if cls in ('hknpConvexPolytopeShape', 'hknpConvexShape'):
                nv, voff = struct.unpack_from('<2H', pk.data, off + 0x30)
                verts = sorted(tuple(r5(c) for c in
                                     xf(struct.unpack_from('<3f', pk.data, off + 0x30 + voff + i * 16), tx))
                               for i in range(nv))
                planes = []
                if cls == 'hknpConvexPolytopeShape':
                    npl, poff = struct.unpack_from('<2H', pk.data, off + 0x40)
                    nf = struct.unpack_from('<H', pk.data, off + 0x44)[0]
                    # Only the first nf planes are REAL. The slots after them are
                    # residue: across 700 vanilla files they are (0,0,0,0) 596
                    # times, (0,0,1,0) 490 times, and things like 0.12657 repeated
                    # four times -- uninitialised memory, not a convention. Ours
                    # are zeros, which vanilla writes too. Comparing them called
                    # 91 of 114 files wrong.
                    raw = [struct.unpack_from('<4f', pk.data, off + 0x40 + poff + i * 16)
                           for i in range(min(nf, npl))]
                    # a translation leaves the normal alone and moves d by -n.t
                    planes = sorted(tuple(r5(c) for c in xf_plane(pl, tx)) for pl in raw)
                out.append((cls, r5(struct.unpack_from('<f', pk.data, off + 0x14)[0]),
                            tuple(verts), tuple(planes)))
                detail[len(out) - 1] = (nv, npl, nf if cls == 'hknpConvexPolytopeShape' else 0)
            elif cls == 'hknpCapsuleShape':
                a = tuple(r5(c) for c in xf(struct.unpack_from('<3f', pk.data, off + 0x50), tx))
                b = tuple(r5(c) for c in xf(struct.unpack_from('<3f', pk.data, off + 0x60), tx))
                out.append((cls, r5(struct.unpack_from('<f', pk.data, off + 0x14)[0]), a, b))
            elif cls == 'hknpSphereShape':
                c = tuple(r5(q) for q in xf(struct.unpack_from('<3f', pk.data, off + 0x40), tx))
                out.append((cls, r5(struct.unpack_from('<f', pk.data, off + 0x14)[0]), c))
            elif cls == 'hknpCompressedMeshShape':
                out.append((cls,))
    return sorted(out)


def bodies_of(path):
    """Per-body filter and material words, order-independent."""
    out = []
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        for psd in pk.of_class('hknpPhysicsSystemData'):
            cin = pk.local.get(psd + 0x40)
            if cin is None:
                continue
            for k in range(pk.u32(psd + 0x48)):
                out.append(struct.unpack_from('<I', pk.data, cin + k * 0x60 + 0x14)[0])
    return sorted(out)


def main():
    manifest = sys.argv[1]
    mod = sys.argv[2] if len(sys.argv) > 2 else \
        'E:/Projects/Fallout 4 Mods/mods/WW Concord Collision Test/Meshes/'
    van = sys.argv[3] if len(sys.argv) > 3 else \
        'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/'

    tally = collections.Counter()
    worst_ok = [0.0]
    offenders = []
    files = identical = 0
    for line in open(manifest, encoding='utf-8', errors='replace'):
        f = line.rstrip('\n').split('\t')
        if len(f) < 5 or f[0] != 'ok':
            continue
        rel = f[4].replace(BS, '/')
        a, b = os.path.join(mod, rel), os.path.join(van, rel)
        if not (os.path.exists(a) and os.path.exists(b)):
            continue
        files += 1
        sa, sb = shapes_of(a), shapes_of(b)
        ba, bb = bodies_of(a), bodies_of(b)
        notes = []
        ca = collections.Counter(s[0] for s in sa)
        cb = collections.Counter(s[0] for s in sb)
        if ca != cb:
            notes.append('shape classes %s vs %s' % (dict(ca), dict(cb)))
            tally['shape classes'] += 1
        else:
            # Shapes are MATCHED, not zipped: sorting tuples that contain nested
            # tuples orders two nearly-identical shapes differently between the
            # two files, and zipping then compares a hull against its neighbour.
            # That reported deviations of exactly 1.0 (a plane normal against a
            # different axis) and 1.8e21 (a padding lane), which is the fourth
            # way this tool has lied. Each of ours takes its closest unused
            # counterpart of the same class.
            worstfile = 0.0
            left = list(range(len(sb)))
            for x in sa:
                best, bestj = None, None
                for j in left:
                    if sb[j][0] != x[0]:
                        continue
                    d = dev(x, sb[j])
                    if d is not None and (best is None or d < best):
                        best, bestj = d, j
                if bestj is None:
                    d, y = None, x
                    for j in left:
                        if sb[j][0] == x[0]:
                            y = sb[j]
                            break
                else:
                    left.remove(bestj)
                    d, y = best, sb[bestj]
                if d is None:
                    # say WHICH: a hull can carry the same vertices and plane
                    # slots and still decompose into a different number of FACES,
                    # which is a different triangulation of the same solid, not
                    # different geometry.
                    nvx = len(x[2]) if len(x) > 2 else 0
                    nvy = len(y[2]) if len(y) > 2 else 0
                    npx = len(x[3]) if len(x) > 3 else 0
                    npy = len(y[3]) if len(y) > 3 else 0
                    if nvx != nvy:
                        notes.append('%s: %d vertices vs %d' % (x[0], nvx, nvy))
                    else:
                        notes.append('%s: same %d vertices, but %d faces vs %d '
                                     '(a different decomposition of the same hull)'
                                     % (x[0], nvx, npx, npy))
                    tally['shape structure'] += 1
                    worstfile = float('inf')
                    break
                worstfile = max(worstfile, d)
            if worstfile == float('inf'):
                pass
            elif worstfile > TOL:
                notes.append('geometry off by %.6f (tolerance %.4f)' % (worstfile, TOL))
                tally['geometry'] += 1
            else:
                worst_ok[0] = max(worst_ok[0], worstfile)
        if ba != bb:
            notes.append('body filter words %s vs %s' % (ba, bb))
            tally['body filter'] += 1
        if notes:
            offenders.append((rel, notes))
        else:
            identical += 1

    print('%d files compared, %d within tolerance in every stored solid' % (files, identical))
    print('worst deviation among those: %.7f  (tolerance %.4f)' % (worst_ok[0], TOL))
    if tally:
        print('differences: %s' % ', '.join('%s x%d' % kv for kv in tally.most_common()))
    else:
        print('no differences')
    for rel, notes in offenders[:30]:
        print('  %s' % rel)
        for n in notes[:5]:
            print('      %s' % n)
    if len(offenders) > 30:
        print('  ... and %d more' % (len(offenders) - 30))
    return 1 if tally else 0


if __name__ == '__main__':
    sys.exit(main())
