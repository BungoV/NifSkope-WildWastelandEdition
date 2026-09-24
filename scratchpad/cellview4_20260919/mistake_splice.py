#!/usr/bin/env python3
"""Splice lane CELLVIEW4's entry into the ROOT MISTAKES.md.

MISTAKES.md is CRLF throughout (measured: 10517 CR to 10517 LF before this
runs), newest at the top, entries starting at the first `## `.  A text-mode
write would convert the whole file, so this is a BYTE splice and the CR count
is printed before and after -- it must go up by exactly the lines added.
"""
import io
import sys

PATH = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'

ENTRY = """## 2026-09-19 -- a byte scan was used as evidence about a format this tree has a reader for (CELLVIEW4)

To answer "why is that shape solid black", lane CELLVIEW4 needed the vertex
descriptor of every BSTriShape in a cell.  Instead of walking the block with
the field order `build/nif.xml` states, the lane's `tangent_census.py` SCANNED
each block for a u64 that looked like a `BSVertexDesc` -- a plausible flags
nibble, a vertex size that divided the data size -- and took the first hit.

It reported `markerxheading.nif` as flags 0x02F, vertex size 40.  The real
descriptor is `0x0002900003020004`: flags 0x29, vertex size 16.  The scan had
locked onto a bounding-sphere float pair.  A whole theory -- "the arrow is
black because it has no tangents" -- was built on that number before anyone
checked it, and it survived a second script because the second script imported
the first one's parser.

It was found by giving the scanner a file whose answer was already known from
NifSkope's own tables and seeing the two disagree, then rebuilding the walk
field by field until it reproduced the measured 118-byte header exactly.  With
the real descriptors the theory died anyway: 18 tangent-less shapes were on
screen in the same picture and every one of them rendered with normal shading.

THE RULE.  If this tree contains a reader for the format -- and for NIF it
contains `build/nif.xml` and the model code that consumes it -- a scan-and-
guess parser is not evidence, it is a hypothesis about a parser.  Write the
walk from the declared field order, and make its first act a self-check against
a file whose numbers are known independently.  A parser that cannot fail loudly
on a file it has misread will hand you confident wrong numbers all day.

SECOND, SMALLER: the same lane owed a `report.md` inside its first ten tool
calls and did not write one until the end of the lane, so a crash any time
before that would have left nothing behind but scripts.  The incremental report
is not paperwork; it is the only part of a lane that survives the lane.

"""


def main():
    raw = io.open(PATH, 'rb').read()
    crBefore = raw.count(b'\r')
    lfBefore = raw.count(b'\n')
    head = raw.find(b'## ')
    if head < 0:
        print('REFUSED: no `## ` entry heading found')
        return 1
    if b'CELLVIEW4' in raw[:4000]:
        print('REFUSED: the top of the file already names CELLVIEW4')
        return 1
    body = ENTRY.replace('\n', '\r\n').encode('utf-8')
    out = raw[:head] + body + raw[head:]
    crAfter = out.count(b'\r')
    lfAfter = out.count(b'\n')
    added = body.count(b'\n')
    print('CR before %d, after %d (+%d); LF before %d, after %d (+%d)'
          % (crBefore, crAfter, crAfter - crBefore,
             lfBefore, lfAfter, lfAfter - lfBefore))
    assert crAfter - crBefore == added, 'CR delta is not the lines added'
    assert crAfter == lfAfter, 'the file is no longer wholly CRLF'
    if '--apply' not in sys.argv:
        print('NOTHING WAS WRITTEN (pass --apply).')
        return 0
    with io.open(PATH, 'wb') as fh:
        fh.write(out)
    print('spliced %d bytes at offset %d' % (len(body), head))
    return 0


if __name__ == '__main__':
    sys.exit(main())
