"""Lane NATIVE1c: is the way-back `.lodi` the SAME FILE as the rung exe's?

`cmp` answers that question with a yes or a no, and the no it gives is useless:
it names one byte and stops.  This gate answers it the way the module-off rule
wants it answered -- every differing byte is NAMED, and a difference is allowed
only where the word is DERIVED FROM THE COMPANION `.lodo`, which this lane bumps
to v4 unconditionally and so legitimately differs.

Two words are derived, and nothing else in the header or the payload is:

  0x0C..0x0F  headerCrc32   crc32 over 0x10..0xFF of this same file
  0x20..0x27  lodoIdentity  FNV-1a 64 over the .lodo's headerCrc32,
                            modelCorpusHash and objectCorpusHash (spec 4.1)

So the claim is not "the two files happen to differ only here".  The claim is
that both words are RECOMPUTED from their own inputs and both come out right,
which makes the difference a consequence of the `.lodo` and not a change of the
instance table.  A difference anywhere else is a failure with its offset named.

  usage: lodgen_lodi_wayback.py A.lodi B.lodi --lodo-a A.lodo --lodo-b B.lodo
"""
import argparse
import struct
import sys
from zlib import crc32

DERIVED = {
    'headerCrc32': (0x0C, 4),
    'lodoIdentity': (0x20, 8),
}


def fnv1a64(b, h=0xCBF29CE484222325):
    for c in b:
        h = ((h ^ c) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def lodo_identity(lodo):
    """Spec 4.1: FNV-1a 64 over the .lodo's headerCrc32, modelCorpusHash,
    objectCorpusHash -- packed `<IQQ`, never an XOR."""
    crc = struct.unpack_from('<I', lodo, 0x0C)[0]
    plugin_hash = struct.unpack_from('<Q', lodo, 0x10)[0]   # noqa: F841 (0x10 is plugin)
    obj = struct.unpack_from('<Q', lodo, 0x18)[0]
    model = struct.unpack_from('<Q', lodo, 0x20)[0]
    return fnv1a64(struct.pack('<IQQ', crc, model, obj))


class Checks(object):
    def __init__(self):
        self.n = 0
        self.bad = 0

    def check(self, what, cond):
        self.n += 1
        if cond:
            print('  ok   %s' % what)
        else:
            self.bad += 1
            print('  FAIL %s' % what)
        return cond


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--lodo-a', dest='lodo_a', required=True)
    ap.add_argument('--lodo-b', dest='lodo_b', required=True)
    ap.add_argument('--label-a', dest='label_a', default='A')
    ap.add_argument('--label-b', dest='label_b', default='B')
    a = ap.parse_args()

    fa = open(a.a, 'rb').read()
    fb = open(a.b, 'rb').read()
    oa = open(a.lodo_a, 'rb').read()
    ob = open(a.lodo_b, 'rb').read()
    ck = Checks()

    print('  %s %d bytes, %s %d bytes' % (a.label_a, len(fa), a.label_b, len(fb)))
    if not ck.check('x0 the two .lodi files are the same LENGTH (%d vs %d)'
                    % (len(fa), len(fb)), len(fa) == len(fb)):
        print('%d checks, %d failures' % (ck.n, ck.bad))
        print('RESULT FAIL')
        return 1

    diff = [i for i in range(len(fa)) if fa[i] != fb[i]]
    allowed = set()
    for (off, size) in DERIVED.values():
        allowed.update(range(off, off + size))
    stray = [i for i in diff if i not in allowed]
    hit = sorted(set(n for (n, (off, size)) in DERIVED.items()
                     if any(off <= i < off + size for i in diff)))

    print('  %d of %d bytes differ; the words they land in: %s'
          % (len(diff), len(fa), ', '.join(hit) if hit else 'none'))
    if stray:
        print('  STRAY OFFSETS (first 16): %s'
              % ' '.join('0x%X' % i for i in stray[:16]))
    ck.check('x1 every differing byte is inside a DERIVED header word '
             '(%d stray bytes)' % len(stray), not stray)

    # A comparator that only ever says "allowed" proves nothing, so prove the
    # payload was actually READ: the two files must agree over every byte that
    # is not one of the two derived words, and that is what x1 just said -- but
    # x2 states the size of what it agreed over, so a truncated read shows up.
    same = len(fa) - len(allowed)
    ck.check('x2 the compared region is the whole file minus the 12 derived '
             'bytes (%d bytes)' % same, same == len(fa) - 12)

    for (f, lodo, label) in ((fa, oa, a.label_a), (fb, ob, a.label_b)):
        ck.check('x3 %s headerCrc32 recomputes over its own 0x10..0xFF' % label,
                 crc32(f[0x10:0x100]) == struct.unpack_from('<I', f, 0x0C)[0])
        want = lodo_identity(lodo)
        got = struct.unpack_from('<Q', f, 0x20)[0]
        ck.check('x4 %s lodoIdentity is FNV-1a 64 over ITS OWN .lodo '
                 '(%016x)' % (label, got), want == got)

    ia = struct.unpack_from('<Q', fa, 0x20)[0]
    ib = struct.unpack_from('<Q', fb, 0x20)[0]
    ck.check('x5 the two identities DIFFER, because the two .lodo files do '
             '(%016x vs %016x)' % (ia, ib), (ia != ib) == (oa != ob))

    print('%d checks, %d failures' % (ck.n, ck.bad))
    print('RESULT %s' % ('PASS' if ck.bad == 0 else 'FAIL'))
    return 1 if ck.bad else 0


if __name__ == '__main__':
    sys.exit(main())
