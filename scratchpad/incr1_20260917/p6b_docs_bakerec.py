"""INCR1 step 6b -- docs/LODGEN_BAKE_RECORD.md.

Two edits, both because lane INCR1 changed what the record CARRIES:

  * section 2.6: the `out` rows of a `--native` bake now include one `.lodj`
    per chunk, and the ledger is what heals a lost one. A reader of that
    section would otherwise not know why an FO4CS record lists a file with an
    extension nothing else in the tree mentions.
  * section 7 is addressed "For lane INCR1" and is now a note to the lane
    AFTER it, so it says what INCR1 actually did with each bullet and what the
    next lane inherits.

Anchors asserted count == 1. LF preserved.
"""
import io
import os
import sys

NL = chr(10)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'docs/LODGEN_BAKE_RECORD.md'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')
if '.lodj' in t:
    raise SystemExit(rel + ' already mentions the chunk cache')


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


# ---- 2.6: the .lodj is an output like any other ------------------------------
t = sub1(t,
         NL.join([
             '`--keep-bto` chunk that stays at the out-dir root is therefore spelled',
             '`../../<name>.BTO`, and that is correct: the row names the file, and the whole',
             'mod folder can be moved without invalidating a row.',
         ]),
         NL.join([
             '`--keep-bto` chunk that stays at the out-dir root is therefore spelled',
             '`../../<name>.BTO`, and that is correct: the row names the file, and the whole',
             'mod folder can be moved without invalidating a row.',
             '',
             '**On a `--native` bake each chunk also has an `out` row for its**',
             '**`<ws>.<dim>.<cx>.<cy>.lodj`** — the per-chunk cache lane INCR1 added so a',
             'skipped chunk can still contribute to the one `.lodo`/`.lodi` pair',
             '(`docs/LODGEN_LEDGER_FORMAT.md` §4.1). It is listed like any other output,',
             'and being listed is what makes it self-healing: a `.lodj` that is missing or',
             'edited marks its chunk dirty, and the rebake writes it again.',
             '',
             'It is the one output whose loss does **not** drag the neighbours in. The',
             'reader in `src/nifcli.cpp` therefore keeps two dirty sets rather than one, and',
             'seeds the one-cell widening from the smaller: see §4.2 of the ledger page for',
             'the measurement that forced that, in which deleting a single cache file',
             'rebaked an entire region.',
         ]),
         'out rows paragraph')

# ---- 7: it is no longer INCR1's page -----------------------------------------
t = sub1(t,
         '## 7. For lane INCR1',
         '## 7. For the lane after INCR1',
         'section 7 heading')

t = sub1(t,
         NL.join([
             '* The `out` rows resolve against the record\'s own folder (§2.6), not the',
             '  out-dir. `lodbFindRecord()` in `src/nifcli.cpp` looks for the FO4CS spot first',
             '  and the stock spot second, because the previous bake may have been either.',
         ]),
         NL.join([
             '* The `out` rows resolve against the record\'s own folder (§2.6), not the',
             '  out-dir. `lodbFindRecord()` in `src/nifcli.cpp` looks for the FO4CS spot first',
             '  and the stock spot second, because the previous bake may have been either.',
             '',
             'What INCR1 (2026-09-17) did with that list, and what it left:',
             '',
             '* Every bullet above held. The record needed **no format change**: the `.lodj`',
             '  cache is carried as ordinary `out` rows, which is why a lost cache heals',
             '  itself for free.',
             '* `--incremental` no longer refuses `--native`, so the record is now read on',
             '  the ruled FO4CS pipeline and not only on the stock target.',
             '* **The wall clock did not move, and the record says why.** On a four-chunk',
             '  FO4CS region a null incremental — no chunk work at all — saved 2 s of 72.',
             '  `lodgenNativeWrite()` builds the object library from the worldspace\'s FULL',
             '  base census and is chunk-independent by design, so roughly 80 % of an FO4CS',
             '  bake is fixed cost that skipping chunks cannot touch.',
             '* The prize the next lane inherits is therefore **reusing the `.lodo`**, not',
             '  skipping more chunks. The library is a pure function of the base census and',
             '  the three corpus hashes **this record already stores** (§2.2), so the record',
             '  is already sufficient to decide whether the previous `.lodo` may be kept.',
             '  INCR1 deliberately did not attempt it: correctness first, and a lane that',
             '  reuses the library must prove bit-identity against a full bake the same way',
             '  §4.1 proved the chunk cache.',
         ]),
         'section 7 tail')

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
