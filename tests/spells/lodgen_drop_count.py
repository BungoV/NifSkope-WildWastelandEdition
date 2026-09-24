#!/usr/bin/env python3
"""lodgen_drop_count.py -- how many of a chunk's placements have NO geometry in its .BTO.

Plan section 5 row 13 (lane INCRGATE1, 2026-09-24). The stock chunk writer stitches
every shape of a bucket into one 16-bit index domain; when a bucket reaches 65,535
vertices the next placement's geometry is not written, and nothing says so. The
manifest sidecar still lists that placement, so the drop is countable from the two
files alone:

    placements  = the manifest's rows (one per ref and SCOL part; column 0 = the
                  identity index the chunk's vertex colours carry)
    with geometry = the identity indices that appear on any `i` line of
                  `lodgen --dump-geometry <chunk>` (needs a bake with --identity)
    dropped     = placements whose index is on no `i` line

Usage: lodgen_drop_count.py <manifest.txt> <dump-geometry output file>
Prints three `drop.*` lines and the first few dropped rows; exit 0 unless an input
is unreadable or empty (exit 2) -- the verdict is the caller's.
"""
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    rows = {}
    for line in open(sys.argv[1], encoding='utf-8', errors='replace'):
        t = line.split()
        if len(t) >= 11 and t[0].isdigit():
            rows[int(t[0])] = t
    seen = set()
    shapes = 0
    for line in open(sys.argv[2], encoding='utf-8', errors='replace'):
        t = line.split()
        if len(t) >= 2 and t[0] == 'i':
            shapes += 1
            seen.update(int(x) for x in t[2:] if x.isdigit())
    if not rows or not shapes:
        print('drop.REFUSED %d manifest rows, %d identity lines: nothing to count' % (len(rows), shapes))
        return 2
    dropped = sorted(k for k in rows if k not in seen)
    stray = sorted(k for k in seen if k not in rows)
    print('drop.placements %d' % len(rows))
    print('drop.withGeometry %d (over %d shape identity lines; %d id(s) not in the manifest)'
          % (len(rows) - len(dropped), shapes, len(stray)))
    print('drop.dropped %d (%.2f%%)' % (len(dropped), 100.0 * len(dropped) / len(rows)))
    for k in dropped[:5]:
        t = rows[k]
        print('drop.row %s base %s class %s ref %s part %s' % (t[0], t[1], t[7], t[9], t[10]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
