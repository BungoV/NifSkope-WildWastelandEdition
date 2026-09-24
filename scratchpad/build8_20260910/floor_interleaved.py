#!/usr/bin/env python3
"""Lane BUILD8: THE FLOOR under the new interleaved read path in src/hkxanim.cpp.

A reader that returned zeros, the bind pose, or frame 0 for every frame would
make every comparison in this lane come out PASS if the oracle shared the
misreading.  So change ONE float of the transform payload by a KNOWN amount and
require the exe's reader to report that amount, in that row and nowhere else.

The row is not guessed: a translation value is taken from the exe's own TSV of
the unmutated file, its float32 bytes are found in the file, and the row is
used only if those bytes occur EXACTLY ONCE in the whole file -- so the byte
nudged is provably the byte that row was read from.

  python floor_interleaved.py IN.hkx IN.tsv OUT.hkx
prints `MUTANT <offset> <old> -> <new>` and the (frame, track, component) it
belongs to.  Exit 2 if no unambiguous row exists.
"""
import struct
import sys


def main():
    src, tsv, dst = sys.argv[1], sys.argv[2], sys.argv[3]
    raw = open(src, "rb").read()

    rows = []
    for line in open(tsv):
        if line.startswith("#") or not line.strip():
            continue
        p = line.rstrip("\n").split("\t")
        if int(p[1]) < 0:
            continue
        rows.append((int(p[0]), int(p[1]), [float(x) for x in p[3:6]]))

    for f, t, tr in rows:
        for c in range(3):
            v = tr[c]
            if abs(v) < 0.5 or abs(v) > 500.0:
                continue
            pat = struct.pack("<f", v)
            if raw.count(pat) != 1:
                continue
            off = raw.find(pat)
            new = v + 1.0
            out = bytearray(raw)
            out[off:off + 4] = struct.pack("<f", new)
            open(dst, "wb").write(bytes(out))
            print("MUTANT %s: frame %d track %d component %d, offset 0x%x, "
                  "%.6f -> %.6f (delta exactly 1.0), the bytes occur once in the file"
                  % (dst, f, t, c, off, v, new))
            return 0
    print("FAIL: no translation value whose bytes occur exactly once")
    return 2


if __name__ == "__main__":
    sys.exit(main())
