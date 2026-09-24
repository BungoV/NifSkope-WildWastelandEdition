#!/usr/bin/env python3
"""check_v2_intact.py -- nothing a version-2 reader addresses CHANGED.

The version-3 discipline is the version-2 one, and it means exactly what the
version-1 -> version-2 gate in tests/spells/lodl_write.sh means: the header
grows at the end, so every section SLIDES by exactly the number of bytes
appended, and nothing else moves at all. Here that is 0xF8 - 0xA0 = 88 bytes.

  * 0x08..0x44 identical (everything but the version word and the section-flag
    word, and the section word may only GAIN bits 4..7);
  * every one of the ten section offsets is exactly +88;
  * every block payload offset in the directory is +88 and no block's sizes
    changed -- compared as NUMBERS, because the directory stores absolute
    offsets and a raw memcmp of it could only pass by accident;
  * the two spans either side of the directory are byte-identical;
  * the version-3 file is longer only by its own appended sections.

Also: the stroke store is present, four bytes, and holds a count of zero. That
is not the same as absent, and the difference is the point -- a panel can write
into a store that exists.
"""

import struct
import sys

GROWTH = 0xF8 - 0xA0


def main():
    a = open(sys.argv[1], 'rb').read()      # module off  -> version 2
    b = open(sys.argv[2], 'rb').read()      # module on   -> version 3
    fails = []

    def check(name, cond):
        print('  %s %s' % ('ok  ' if cond else 'FAIL', name))
        if not cond:
            fails.append(name)

    check('the module-off file is version 2', struct.unpack_from('<I', a, 4)[0] == 2)
    check('the module-on file is version 3', struct.unpack_from('<I', b, 4)[0] == 3)
    check('0x08..0x44 is identical (every field but the version and the section bits)',
          a[0x08:0x44] == b[0x08:0x44])
    sa = struct.unpack_from('<I', a, 0x44)[0]
    sb = struct.unpack_from('<I', b, 0x44)[0]
    check('the section word gained only bits 4..7 (0x%x -> 0x%x)' % (sa, sb),
          (sb & 0x0F) == sa and (sb & 0xF0) == 0xF0)

    oa = struct.unpack_from('<QQQQQQQQQQ', a, 0x48)
    ob = struct.unpack_from('<QQQQQQQQQQ', b, 0x48)
    moved = [k for k in range(9) if ob[k] != oa[k] + GROWTH]
    check('every version-2 section offset moved by exactly the %d bytes the '
          'header grew (%d did not)' % (GROWTH, len(moved)), not moved)
    check('the first section starts at 0xF8, the version-3 header size',
          ob[0] == 0xF8)

    d2s, d2e = ob[7], ob[8]
    d1s, d1e = oa[7], oa[8]
    nblk = (d1e - d1s) // 16
    badoff = badsz = 0
    for k in range(nblk):
        a2, c2, u2 = struct.unpack_from('<QII', b, d2s + k * 16)
        a1, c1, u1 = struct.unpack_from('<QII', a, d1s + k * 16)
        if a2 != a1 + GROWTH:
            badoff += 1
        if (c2, u2) != (c1, u1):
            badsz += 1
    print('  %d directory entries, %d offsets not +%d, %d sizes changed'
          % (nblk, badoff, GROWTH, badsz))
    check('the directory has entries at all, so this can fail', nblk > 0)
    check('every block payload offset moved by exactly %d and no size moved' % GROWTH,
          badoff == 0 and badsz == 0)
    check('and NOTHING ELSE moved: identical either side of the directory',
          b[0xF8:d2s] == a[0xA0:d1s] and b[d2e:d2e + (len(a) - d1e)] == a[d1e:])

    check('the version-3 file is longer, and only at the end (%d bytes of water '
          'sections)' % (len(b) - len(a) - GROWTH), len(b) > len(a))
    check('the size field is the file size', struct.unpack_from('<Q', b, 0x90)[0] == len(b))

    o = struct.unpack_from('<Q', b, 0xE8)[0]
    n = struct.unpack_from('<I', b, 0xF0)[0]
    check('the stroke store is present, four bytes, and holds a count of zero',
          bool(sb & (1 << 7)) and n == 4 and struct.unpack_from('<I', b, o)[0] == 0)
    check('the reserved word at 0xF4 is zero', struct.unpack_from('<I', b, 0xF4)[0] == 0)

    print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
