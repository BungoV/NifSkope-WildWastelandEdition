#!/usr/bin/env python3
"""corrupt_lodl.py -- break ONE named thing in a version-3 .lodl, so the
reader's refusal for it can be provoked and read.

Gate G4 wants each refusal to produce its OWN sentence, not a shared "bad
file". Each corruption below touches exactly one field, and the harness runs
the unbroken file through the same route first, so a refusal is never mistaken
for a broken route.

    version4  the version word -> 4                 (refusal 1)
    norate    the body-ID plane's samples-per-cell -> 0, bit still set
    stride    the body record stride -> 32, SHORTER than the 48 this reader
              knows.  A record LONGER than the reader's is the forward-compatible
              case and must NOT refuse -- the reader strides by the file's value
              and reads the prefix it knows, which is the rule the .lodm
              sidecars already use.  Only a record that is missing fields the
              reader needs is a refusal.
    badid     record 0's `id` -> 7, which is not its index + 1
    planeid   the body COUNT -> 1, so the plane names bodies past the table
"""

import shutil
import struct
import sys


def main():
    src, dst, what = sys.argv[1], sys.argv[2], sys.argv[3]
    shutil.copyfile(src, dst)
    head = open(src, 'rb').read(0xF8)
    with open(dst, 'r+b') as f:
        if what == 'version4':
            f.seek(4)
            f.write(struct.pack('<I', 4))
        elif what == 'norate':
            f.seek(0xBC)
            f.write(struct.pack('<I', 0))
        elif what == 'stride':
            f.seek(0xAC)
            f.write(struct.pack('<I', 32))
        elif what == 'badid':
            o = struct.unpack_from('<Q', head, 0xA0)[0]
            f.seek(o)
            f.write(struct.pack('<H', 7))
        elif what == 'planeid':
            f.seek(0xA8)
            f.write(struct.pack('<I', 1))
        else:
            raise SystemExit('unknown corruption %r' % what)
    print('%s: %s' % (dst, what))


if __name__ == '__main__':
    main()
