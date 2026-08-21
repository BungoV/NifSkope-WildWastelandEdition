"""Check an hknpCompoundShapeData the way the ENGINE reads it, not the way we wrote it.

    python tools/hkcompound.py <file.nif> [--expect-compounds=N] [--quiet]

WHY THIS EXISTS

A compound written from scratch shipped with its BVH intact and no POINTER to it:
the local fixup at +0x10 was never emitted, so Fallout 4 dereferenced null inside
hknpDynamicCompoundShape::updateAabb the first time it loaded one. Every check we
had passed. `--roundtrip` passed too, because our own decoder carries an
unreadable array through verbatim and re-encodes it byte for byte — a round trip
cannot see a pointer neither end needs.

So this reads the object the way the engine does: follow the pointer or fail.

WHAT IS CHECKED, all measured on vanilla (86 of 86 compounds fit it):

  * the node array pointer exists at +0x10, and lands where the header says
  * the array holds 2n+1 records of 32 bytes, and the object is that size
  * index 0 is a NULL SENTINEL, all zero -- which is why a link of 0 means "none"
  * the last record is zero too, leaving 2n-1 real nodes for n leaves
  * every parent tag is 0x3f000000 | parentIndex, and names the node that
    actually points at it
  * every child link is in range, and the leaves are a permutation of 0..n-1
  * the compound's own AABB at +0x80/+0x90 equals the root node's box

Exit 0 if it holds up, 1 if it does not, 2 if the file holds no compound.
"""
import struct
import sys

sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from hkmatrun import nif_blobs, Pack       # same packfile reader


def size_of(pk, off):
    """Distance to the next object; objects sit back to back, padded to 16."""
    later = [o for o, c in pk.objects if o > off]
    return (min(later) if later else len(pk.data)) - off


def check(path, expect=None, quiet=False):
    def say(msg):
        if not quiet:
            print(msg)

    bad = []
    seen = 0
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        datas = pk.of_class('hknpDynamicCompoundShapeData') + pk.of_class('hknpStaticCompoundShapeData')
        comps = pk.of_class('hknpDynamicCompoundShape') + pk.of_class('hknpStaticCompoundShape')
        for k, da in enumerate(datas):
            seen += 1
            where = 'block %d compound %d' % (bi, k)
            size = size_of(pk, da)
            n = pk.u32(da + 0x28)
            nodes = pk.u32(da + 0x18)
            ptr = pk.local.get(da + 0x10)
            if ptr is None:
                bad.append('%s: NO POINTER to the node array (+0x10) — the engine reads null' % where)
                continue
            if ptr - da != 0x40:
                bad.append('%s: array pointer lands at +0x%x, not +0x40' % (where, ptr - da))
            if nodes != 2 * n + 1:
                bad.append('%s: header says %d nodes, expected 2n+1 = %d' % (where, nodes, 2 * n + 1))
            if size != 0x40 + nodes * 32:
                bad.append('%s: object is %d bytes, expected 0x40 + %d*32 = %d'
                           % (where, size, nodes, 0x40 + nodes * 32))
                continue
            rec = []
            for i in range(nodes):
                o = ptr + i * 32
                w = struct.unpack_from('<I', pk.data, o + 12)[0]
                a, b = struct.unpack_from('<2H', pk.data, o + 28)
                rec.append((w, a, b, pk.data[o:o + 32]))
            if any(rec[0][3]):
                bad.append('%s: node 0 is not the null sentinel' % where)
            if any(rec[-1][3]):
                bad.append('%s: the last node is not zero' % where)
            leaves, parent = [], {}
            for i in range(1, nodes - 1):
                w, a, b, raw = rec[i]
                if (w & 0xffffff00) != 0x3f000000:
                    bad.append('%s: node %d parent tag %08x is not 0x3f0000xx' % (where, i, w))
                    continue
                parent[i] = w & 0xff
                if a == 0:
                    leaves.append(b)
                else:
                    for c in (a, b):
                        if not (1 <= c < nodes - 1):
                            bad.append('%s: node %d links to %d, outside 1..%d' % (where, i, c, nodes - 2))
            for i in range(1, nodes - 1):
                w, a, b, raw = rec[i]
                if a:
                    for c in (a, b):
                        if 1 <= c < nodes - 1 and parent.get(c) != i:
                            bad.append('%s: node %d claims child %d, whose parent tag says %s'
                                       % (where, i, c, parent.get(c)))
            if parent.get(1) not in (0, None):
                bad.append('%s: the root (node 1) has parent %s, expected 0' % (where, parent.get(1)))
            if sorted(leaves) != list(range(n)):
                bad.append('%s: leaves are %s, not a permutation of 0..%d' % (where, sorted(leaves), n - 1))
            if k < len(comps):
                co = comps[k]
                amin = struct.unpack_from('<3f', pk.data, co + 0x80)
                amax = struct.unpack_from('<3f', pk.data, co + 0x90)
                rmin = struct.unpack_from('<3f', pk.data, ptr + 32)
                rmax = struct.unpack_from('<3f', pk.data, ptr + 32 + 16)
                if amin != rmin or amax != rmax:
                    bad.append('%s: the compound AABB is not the root node box' % where)
            say('%s: %d children, %d nodes, pointer +0x40, leaves %s'
                % (where, n, nodes, sorted(leaves)))
    if not seen:
        say('%s holds no compound' % path)
        return 2
    for b in bad:
        print('FAIL %s' % b)
    say('%d compound(s) checked, %d problems' % (seen, len(bad)))
    if expect is not None and seen != expect:
        print('FAIL expected %d compound(s), found %d' % (expect, seen))
        return 1
    return 1 if bad else 0


def damage(path, out):
    """Write a copy whose node-array pointer is misfiled, reproducing the crash.

    The fixup is not deleted -- that would resize the table -- its SOURCE is moved
    to +0x14, so nothing points at +0x10 any more and the engine reads null. This
    is what shipped, and what the check above has to reject.
    """
    data = bytearray(open(path, 'rb').read())
    moved = 0
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        for da in pk.of_class('hknpDynamicCompoundShapeData') + pk.of_class('hknpStaticCompoundShapeData'):
            want = da + 0x10
            # the local fixup table sits after the data; find the entry and bend it
            base = at + pk.base
            for off in range(at, len(data) - 8):
                src, dst = struct.unpack_from('<ii', data, off)
                if src == want and (dst - da) == 0x40:
                    struct.pack_into('<i', data, off, want + 4)
                    moved += 1
                    break
    open(out, 'wb').write(bytes(data))
    print('misfiled %d node-array pointer(s) -> %s' % (moved, out))
    return 0 if moved else 1


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    want, out = None, None
    for a in sys.argv[1:]:
        if a.startswith('--expect-compounds='):
            want = int(a.split('=', 1)[1])
        if a.startswith('--damage='):
            out = a.split('=', 1)[1]
    if out:
        sys.exit(damage(args[0], out))
    sys.exit(check(args[0], want, '--quiet' in sys.argv))
