#!/usr/bin/env python
"""BUILD1: lodt_write.sh's version-1 fallback check could not pass.

It asserted `b[0xA0:] == v1[0x98:]` -- everything past the header identical.
But the BLOCK DIRECTORY stores each block's payload offset ABSOLUTE
(docs/LODGEN_BTD_FORMAT.md, "Block directory": uint64 offset, uint32
compressed size, uint32 uncompressed size), so appending eight bytes to the
header moves every one of those offsets by eight as well. Measured on the
Commonwealth pair written by the built exe: 50,657 differing bytes, ALL of
them inside the directory range (0x90026c..0x9bf66c), first at 0x90026c.

The invariant the fallback actually has to meet, and now does:

  * 48,960 directory entries, every payload offset exactly v1 + 8, every
    compressed and uncompressed size unchanged;
  * every byte before the directory identical (9,437,644 of them) and every
    byte after it identical (25,732,130) -- so the pyramid payloads, the
    per-cell table, the AO and overview planes and the tables are untouched.

That is a stronger statement than the old one, not a weaker: the old check
would also have passed a file whose directory was rewritten wholesale, and it
could not pass at all.

LF-only file; the CR count must stay 0.
"""
import os

P = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 '..', '..', 'tests', 'spells', 'lodt_write.sh')
P = os.path.normpath(P)

b = open(P, 'rb').read()
assert b.count(b'\r') == 0, b.count(b'\r')

OLD = (b'    check("every section offset moved by exactly the 8 bytes appended",\n'
       b'          all(off[k] == o1[k] + 8 for k in range(10)))\n'
       b'    check("and NOTHING ELSE moved: the two files are identical past the header",\n'
       b'          b[0xA0:] == v1[0x98:])\n')
NEW = (b'    check("every section offset moved by exactly the 8 bytes appended",\n'
       b'          all(off[k] == o1[k] + 8 for k in range(10)))\n'
       b'    # The block directory stores each payload\'s offset ABSOLUTE, so it moves\n'
       b'    # by the same eight bytes and cannot be compared byte for byte. Its\n'
       b'    # entries are compared as NUMBERS instead, and the two spans on either\n'
       b'    # side of it byte for byte -- which is the stronger statement: a\n'
       b'    # directory rewritten wholesale would pass a raw memcmp of the tail\n'
       b'    # only by accident, and this fails on any entry that moved by anything\n'
       b'    # other than 8 or whose sizes changed.\n'
       b'    d2s, d2e, d1s, d1e = off[7], off[8], o1[7], o1[8]\n'
       b'    nblk = (d2e - d2s) // 16\n'
       b'    badoff = badsz = 0\n'
       b'    for k in range(nblk):\n'
       b'        a2, c2, u2 = struct.unpack_from("<QII", b, d2s + k * 16)\n'
       b'        a1, c1, u1 = struct.unpack_from("<QII", v1, d1s + k * 16)\n'
       b'        if a2 != a1 + 8:\n'
       b'            badoff += 1\n'
       b'        if (c2, u2) != (c1, u1):\n'
       b'            badsz += 1\n'
       b'    print("  %d directory entries, %d offsets not +8, %d sizes changed"\n'
       b'          % (nblk, badoff, badsz))\n'
       b'    check("the directory has entries at all (so this can fail)", nblk > 0)\n'
       b'    check("every block payload offset moved by exactly 8, and no size moved",\n'
       b'          badoff == 0 and badsz == 0)\n'
       b'    check("and NOTHING ELSE moved: identical either side of the directory",\n'
       b'          b[0xA0:d2s] == v1[0x98:d1s] and b[d2e:] == v1[d1e:])\n')
assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)

assert b.count(b'\r') == 0
open(P, 'wb').write(b)
print('patched %s, %d bytes, CR %d' % (P, len(b), b.count(b'\r')))
