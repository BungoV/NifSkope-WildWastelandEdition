"""What does the CMSD's triangleIsInterior bit mean? Measure, don't guess.

    python tools/hkinterior.py <dir-or-nif> [--limit=N] [--rules]

Reads each compiled mesh the way the engine does -- sections, quantized
vertices, quad primitives -- then reads the shape's triangleIsInterior hkBitField
and reports what the set bits have in common.

THE INDEXING (this is the part that was wrong before 2026-08-20): the bit field
is in KEY SPACE, not triangle order.

    bit = (section << primBits) | (2 * localPrimitive + half)

with half 0 for (a,b,c) and half 1 for (a,c,d), and
primBits = shape+0x12 (bitsPerKey) - bitlen(sections - 1). That is the same
indexing quadIsFlat uses at half the resolution, which is why the field is
numKeys bits where quadIsFlat is numKeys/2. Under it the necessary condition
below is exact; read in triangle order it is only approximate, which is what the
2026-08-16 "767 of 867" measurement was actually seeing.

WHAT IS ESTABLISHED (400 meshes, 20,354 triangles, 874 set bits):

  * EVERY set bit sits on a triangle whose three edges are each shared with
    another triangle. Zero exceptions. It is a necessary condition.
  * It is nowhere near sufficient: 9,728 fully-shared triangles carry only 479
    bits in the same sample.
  * Small closed meshes -- the 12- and 16-triangle boxes and duct sections that
    make up most of the corpus, every edge shared -- are ALL ZERO. So the bit
    does not mean "closed surface", and zero is what vanilla itself writes for
    the shapes our compiler mostly produces.

WHAT IT IS NOT (each measured, with `--rules` reproducing the table):
  all-three-edges-shared alone, the second half of a quad, buried inside another
  connected component (0 hits), an inward-facing normal (15 of 50 on Toilet01),
  all-convex / all-concave / n-concave neighbours (best F 0.30), a max-dihedral
  threshold (best F 0.38), a coincident twin triangle (0 hits), or a junction
  vertex (precision 6.7%).

So it is not a local geometric predicate, and the writer keeps writing zero --
which is the safe direction: a set bit is a strict subset of fully-shared
triangles, so under-flagging can only ask the engine to do more work, never to
skip a collidable edge. Settling it wants a controlled Elric pair (recompile one
mesh with a face removed and diff the bits), and Elric is not on this machine.
"""
import collections
import math
import os
import struct
import sys


# --- packfile reading (same shape as tools/hkmatrun.py) ---------------------

def nif_blobs(path):
    data = open(path, 'rb').read()
    pos = data.index(b'\x0a') + 1
    pos += 4 + 1 + 4
    nb, = struct.unpack_from('<I', data, pos); pos += 4
    bsver, = struct.unpack_from('<I', data, pos); pos += 4
    for _ in range(4 if bsver == 130 else 3):
        pos += 1 + data[pos]
    ntypes = struct.unpack_from('<H', data, pos)[0]; pos += 2
    types = []
    for _ in range(ntypes):
        n = struct.unpack_from('<I', data, pos)[0]; pos += 4
        types.append(data[pos:pos + n].decode('latin-1')); pos += n
    tidx = struct.unpack_from('<%dH' % nb, data, pos); pos += nb * 2
    bsize = struct.unpack_from('<%dI' % nb, data, pos); pos += nb * 4
    nstr = struct.unpack_from('<I', data, pos)[0]; pos += 8
    for _ in range(nstr):
        n = struct.unpack_from('<I', data, pos)[0]; pos += 4 + n
    ngrp = struct.unpack_from('<I', data, pos)[0]; pos += 4 + ngrp * 4
    out, off = [], pos
    for i in range(nb):
        if types[tidx[i]] in ('bhkPhysicsSystem', 'bhkRagdollSystem'):
            n, = struct.unpack_from('<I', data, off)
            out.append(data[off + 4:off + 4 + n])
        off += bsize[i]
    return out


class Pack(object):
    def __init__(self, blob):
        if struct.unpack_from('<II', blob, 0) != (0x57E0E057, 0x10C0C010):
            raise ValueError('not a Havok packfile')
        nsec, = struct.unpack_from('<i', blob, 20)
        secs = []
        for s in range(nsec):
            o = 0x40 + s * 0x40
            secs.append((blob[o:o + 19].split(b'\x00')[0].decode('latin-1'),)
                        + struct.unpack_from('<7i', blob, o + 20))
        names, cn = {}, secs[0]
        p, end = cn[1], cn[1] + cn[2]
        while p < end - 5 and blob[p + 4] == 0x09:
            e = blob.index(b'\x00', p + 5)
            names[p + 5 - cn[1]] = blob[p + 5:e].decode('latin-1')
            p = e + 1
        dt = [s for s in secs if s[0] == '__data__'][0]
        self.data = blob[dt[1]:dt[1] + dt[2]]
        self.objects = []
        p = dt[1] + dt[4]
        while p + 12 <= dt[1] + dt[5]:
            src, sec, cno = struct.unpack_from('<iii', blob, p); p += 12
            if src != -1:
                self.objects.append((src, names.get(cno, '?')))
        self.objects.sort()
        self.local = {}
        p = dt[1] + dt[2]
        while p + 8 <= dt[1] + dt[3]:
            src, dst = struct.unpack_from('<ii', blob, p); p += 8
            if src != -1:
                self.local[src] = dst

    def u32(self, off):
        return struct.unpack_from('<I', self.data, off)[0]

    def of_class(self, name):
        return [o for o, c in self.objects if c == name]


def bitlen(v):
    n = 0
    while v:
        n += 1
        v >>= 1
    return n


def shapes(pk):
    """(shapeOffset, cmsdOffset) for every compressed mesh."""
    for sh in pk.of_class('hknpCompressedMeshShape'):
        for off, cls in pk.objects:
            if cls == 'hknpCompressedMeshShapeData' and off > sh:
                yield sh, off
                break


def triangles(pk, sh, cm):
    """[(section, primitive, half, (v0,v1,v2), interiorBit)], vertices dequantized.

    None if the mesh uses the shared-vertex table, which this does not follow.
    """
    nsec = pk.u32(cm + 0x58)
    secAt = pk.local.get(cm + 0x50)
    primAt = pk.local.get(cm + 0x60)
    packAt = pk.local.get(cm + 0x80)
    intAt = pk.local.get(sh + 0x80)
    numKeys = pk.u32(sh + 0x90)
    if None in (secAt, primAt, packAt, intAt) or not numKeys:
        return None
    secBits = bitlen(nsec - 1) if nsec > 1 else 0
    primBits = pk.data[sh + 0x12] - secBits
    bits = pk.data[intAt:intAt + (numKeys + 7) // 8]

    def bit(i):
        return (bits[i // 8] >> (i % 8)) & 1 if i // 8 < len(bits) else 0

    out = []
    for s in range(nsec):
        so = secAt + s * 0x60
        off = struct.unpack_from('<3f', pk.data, so + 0x30)
        stp = struct.unpack_from('<3f', pk.data, so + 0x3c)
        firstPacked = pk.u32(so + 0x48)
        pf = pk.u32(so + 0x50)
        firstPrim, numPrim = pf >> 8, pf & 0xff
        numPacked = pk.data[so + 0x58]

        def vert(idx):
            if idx >= numPacked:
                return None
            v = pk.u32(packAt + (firstPacked + idx) * 4)
            return (round(off[0] + (v & 0x7FF) * stp[0], 4),
                    round(off[1] + ((v >> 11) & 0x7FF) * stp[1], 4),
                    round(off[2] + ((v >> 22) & 0x3FF) * stp[2], 4))

        for p in range(numPrim):
            P = primAt + (firstPrim + p) * 4
            a, b, c, d = pk.data[P], pk.data[P + 1], pk.data[P + 2], pk.data[P + 3]
            va, vb, vc = vert(a), vert(b), vert(c)
            if None in (va, vb, vc):
                return None
            key = (s << primBits) | (2 * p)
            out.append((s, p, 0, (va, vb, vc), bit(key)))
            if d != c:
                vd = vert(d)
                if vd is None:
                    return None
                out.append((s, p, 1, (va, vc, vd), bit(key + 1)))
    return out


# --- geometry ---------------------------------------------------------------

def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _cross(a, b): return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])
def _dot(a, b): return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]


def _unit(a):
    l = math.sqrt(_dot(a, a)) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)


def neighbours(geo):
    """Per triangle, per edge: (dihedralAngle, 'convex'|'concave'|'flat') or None."""
    byedge = collections.defaultdict(list)
    for i, g in enumerate(geo):
        for k in range(3):
            byedge[tuple(sorted((g[k], g[(k + 1) % 3])))].append(i)
    out = []
    for i, g in enumerate(geo):
        n1 = _unit(_cross(_sub(g[1], g[0]), _sub(g[2], g[0])))
        edges = []
        for k in range(3):
            a, b = g[k], g[(k + 1) % 3]
            others = [j for j in byedge[tuple(sorted((a, b)))] if j != i]
            if not others:
                edges.append(None)
                continue
            j = others[0]
            n2 = _unit(_cross(_sub(geo[j][1], geo[j][0]), _sub(geo[j][2], geo[j][0])))
            opp = [v for v in geo[j] if v != a and v != b]
            side = _dot(n1, _sub(opp[0], g[0])) if opp else 0.0
            ang = math.degrees(math.acos(max(-1.0, min(1.0, _dot(n1, n2)))))
            edges.append((ang, 'concave' if side > 1e-6 else ('convex' if side < -1e-6 else 'flat')))
        out.append(edges)
    return out


RULES = collections.OrderedDict((
    ('all three edges shared',   lambda e: True),
    ('  + >=1 concave edge',     lambda e: sum(1 for x in e if x[1] == 'concave') >= 1),
    ('  + >=2 concave edges',    lambda e: sum(1 for x in e if x[1] == 'concave') >= 2),
    ('  + all three concave',    lambda e: all(x[1] == 'concave' for x in e)),
    ('  + all three convex',     lambda e: all(x[1] == 'convex' for x in e)),
    ('  + no edge under 1 deg',  lambda e: all(x[0] >= 1.0 for x in e)),
    ('  + max dihedral < 60',    lambda e: max(x[0] for x in e) < 60.0),
    ('  + max dihedral < 89',    lambda e: max(x[0] for x in e) < 89.0),
))


def walk(target, limit=0):
    if os.path.isfile(target):
        yield target
        return
    seen = 0
    for dp, dn, fn in os.walk(target):
        for f in sorted(fn):
            if f.lower().endswith('.nif'):
                yield os.path.join(dp, f)
                seen += 1
                if limit and seen >= limit:
                    return


def main(target, limit=0, rules=False):
    meshes = tris_total = bits_total = with_bits = 0
    violations = []
    tp = collections.Counter(); fp = collections.Counter()
    candidates = flagged = 0
    for path in walk(target, limit):
        try:
            blobs = nif_blobs(path)
        except Exception:
            continue
        for blob in blobs:
            try:
                pk = Pack(blob)
            except Exception:
                continue
            for sh, cm in shapes(pk):
                tris = triangles(pk, sh, cm)
                if not tris:
                    continue
                meshes += 1
                geo = [t[3] for t in tris]
                bits = [t[4] for t in tris]
                tris_total += len(tris); bits_total += sum(bits)
                with_bits += 1 if sum(bits) else 0
                info = neighbours(geo)
                for i in range(len(geo)):
                    full = all(x is not None for x in info[i])
                    if bits[i] and not full:
                        violations.append((os.path.basename(path), i))
                    if not full:
                        continue
                    candidates += 1
                    b = bool(bits[i]); flagged += b
                    if rules:
                        for name, rule in RULES.items():
                            if rule(info[i]):
                                (tp if b else fp)[name] += 1
    print('%d meshes, %d triangles, %d interior bits set (%.2f%%)'
          % (meshes, tris_total, bits_total, 100.0 * bits_total / max(tris_total, 1)))
    print('meshes with any bit set: %d of %d' % (with_bits, meshes))
    print('set bits on a triangle with an unshared edge: %d   <- the necessary condition'
          % len(violations))
    for v in violations[:5]:
        print('   violation:', v)
    if rules:
        print('%d fully-shared triangles, %d of them flagged' % (candidates, flagged))
        print('%-28s %8s %8s %8s %6s' % ('rule', 'hits', 'recall', 'precis', 'F'))
        for name in RULES:
            h = tp[name] + fp[name]
            r = tp[name] / max(flagged, 1); p = tp[name] / max(h, 1)
            print('%-28s %8d %7.1f%% %7.1f%% %6.2f'
                  % (name, h, 100 * r, 100 * p, 2 * r * p / max(r + p, 1e-9)))
    return 0


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    lim = 0
    for a in sys.argv[1:]:
        if a.startswith('--limit='):
            lim = int(a.split('=', 1)[1])
    sys.exit(main(args[0], lim, '--rules' in sys.argv))
