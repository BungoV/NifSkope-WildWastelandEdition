"""INCR1 step 6c -- docs/LODGEN_CENSUS.md 6.1 gains the two incremental lines.

The inventory in 6.1 is meant to be every bake census line as it exists today.
`incremental:` was never in it, and this lane both added a field to it and put
a second line beside it. Each row is written in the page's own house style: what
it carries, HOW IT MOVES, and the gate that reads it back -- because a census
field nothing measures is a field that goes stale the next lane.

Anchor asserted count == 1. LF preserved.
"""
import io
import os
import sys

NL = chr(10)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'docs/LODGEN_CENSUS.md'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')
if '`native cache:`' in t:
    raise SystemExit(rel + ' already carries the incremental rows')

anchor = '| the manifest per chunk | one row per placement,'
n = t.count(anchor)
if n != 1:
    raise SystemExit('ANCHOR manifest row: found %d times, wanted exactly 1' % n)

rows = NL.join([
    '| `incremental:` | **The INCREMENTAL line (lane LAND1, 2026-09-12; a fifth field from lane INCR1, 2026-09-17).** `N of M chunks dirty`, then the reason each dirty chunk is dirty, as a partition inside one bracket: `inputs moved`, `not in the ledger`, `output lost`, `by neighbour` (the one-cell widening), and `with no native chunk cache`. The last is chunks clean by every other measure whose `.lodj` is simply not on disk -- the first `--incremental` after an older bake -- and it is the field that tells a full-looking run apart from a genuinely dirty one. Up to eight chunks are then named on their own lines with the digest that moved. HOW IT MOVES: edit one cell of the plugin and `inputs moved` becomes 1 and `by neighbour` rises to the ring around it; delete one output and `output lost` becomes 1; delete one `.lodj` and `with no native chunk cache` becomes 1 **and `by neighbour` stays where it was**, which is the whole point of the two dirty lists (`docs/LODGEN_LEDGER_FORMAT.md` §4.2). Gate: `tests/spells/lodgen_incremental.sh` arms (a) and (b) read the numbers out of this line rather than grepping its wording -- grepping the wording is how the first version of that leg passed while nothing at all was cached. |',
    '| `native cache:` | **The CHUNK CACHE line (lane INCR1, 2026-09-17),** printed only on a `--native` bake with the cache on. Chunks written to `.lodj`; chunks replayed from cache and the placements those carried; failures; and arrivals lit by more than one chunk. The last two are not decoration: a non-zero failure count fails the bake, because the pair it would write is missing whole chunks, and a non-zero shared-arrival count on a run that replayed anything REFUSES, because `(prev + a1) + a2` is not `prev + (a1 + a2)` in floating point and the pair would be nearly right. HOW IT MOVES: a null incremental prints `0 written, <all> replayed`; deleting one `.lodj` prints `1 written, <rest> replayed`; `--no-native-cache` removes the line entirely and the incremental run refuses instead. Gate: `tests/spells/lodgen_incremental.sh` arms (a), (b) and (e). |',
    '',
])
t = t.replace(anchor, rows + anchor, 1)

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
