"""INCR1 step 6d -- the two mistake ledgers.

`docs/MISTAKES.md` takes the lodgen one: a refusal aimed at a corner case that
landed on the only pipeline anybody bakes, and the cost model that went with it.
Root `MISTAKES.md` takes mine.

Both files are newest-at-top, so both inserts go in front of the current first
entry. Anchors asserted count == 1. LF preserved.
"""
import io
import os
import sys

NL = chr(10)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]


def patch(rel, anchor, added, guard):
    with io.open(os.path.join(ROOT, rel), 'rb') as fh:
        b = fh.read()
    assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
    t = b.decode('utf-8')
    if guard in t:
        print('  %s already has it' % rel)
        return
    n = t.count(anchor)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (rel, n))
    t = t.replace(anchor, added + anchor, 1)
    if CHECK:
        print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
    else:
        with io.open(os.path.join(ROOT, rel), 'wb') as fh:
            fh.write(t.encode('utf-8'))
        print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))


DOCS = NL.join([
    '## 2026-09-17 — A refusal aimed at a corner case landed on the only pipeline anybody bakes',
    '',
    '**What:** `--incremental` refused `--native`. bungo\'s ruled FO4CS command is',
    '`--native <dir>`, everything under `Data/FO4CSLOD/`, and it is the default',
    'pipeline in this tree — so `--incremental` was usable only on the stock engine',
    'target, which is the target nobody bakes. The feature shipped, was gated, was',
    'documented, and refused every real command for five days.',
    '',
    '**Why it got through:** the refusal is *correct about its mechanism*. `--native`',
    'builds ONE `.lodo`/`.lodi` pair for the whole region out of the placements the',
    'chunk pass hands it, so an incremental run would have written a pair covering',
    'only the rebaked chunks and said nothing about the rest — and it would have',
    'loaded, passed `--native-verify --native-verify-corpus`, matched its own three',
    'staleness hashes, and simply been missing most of the worldspace. That argument',
    'was reviewed on its own terms and it survives on its own terms. What nobody',
    'asked was **what fraction of the commands an operator actually types it**',
    '**refused**. The answer was all of them.',
    '',
    'The gate made it invisible. `--incremental`\'s identity gate baked the STOCK',
    'target, because the stock target was the only one `--incremental` would run on.',
    'A gate shaped by the defect it is meant to catch reports green forever.',
    '',
    '**How it was found:** it was written down as a gap in a lane brief, not found',
    'by anything in the tree.',
    '',
    '**The rule:** a change that adds a refusal states, in the same change, which of',
    'the project\'s RULED default commands it now refuses, **by name**. A refusal',
    'that covers the default pipeline is a missing feature wearing a refusal\'s',
    'clothes. And a gate that can only run in the configuration the defect permits',
    'is not a gate for that defect: say so in the gate, or make it fail.',
    '',
    '### The second half: the cost model was never measured either',
    '',
    '**What:** with the refusal removed and the per-chunk cache proved bit-identical,',
    'a null incremental — no chunk work at all — on a four-chunk FO4CS region saved',
    '**2 seconds of 72**.',
    '',
    '**Why:** `lodgenNativeWrite()` builds the object library from the worldspace\'s',
    'FULL base census. That is chunk-independent by design (spec 8 / 9.3): the',
    '`.lodo` is an append-only library of every base, not of every placement. Against',
    'a nine-chunk bake of the same tree a chunk is worth about 1.6 s, so the FIXED',
    'cost of an FO4CS bake is around 58 s — roughly **80 %**. "Incremental" was',
    'assumed to mean "the work is per chunk" because that is what it means on the',
    'stock target, where it is true.',
    '',
    '**The rule:** before building a skip, measure what fraction of the wall clock',
    'the skipped thing is, on the target that matters. The lane still shipped,',
    'because incremental-and-correct is the precondition for reusing the library at',
    'all — but it shipped saying 2 of 72 in its own report, not implying more.',
    '',
    '---',
    '',
    '',
])

MINE = NL.join([
    '## 2026-09-17 06:xx -- lane INCR1 (the FO4CS chunk cache and its gate)',
    '',
    '1. **Edited a harness while it was running.** A gate was mid-bake and I patched',
    '   the same `.sh` file; bash had already read part of it and re-read the changed',
    '   bytes at a byte offset that no longer meant what it had meant. The run\'s',
    '   verdicts were garbage and, worse, plausible. I had to throw away a BEFORE',
    '   table and cite ARCHLOCK1\'s recorded one instead. **Rule:** a harness that is',
    '   running is read-only; queue the edit, or copy the harness aside and edit the',
    '   copy.',
    '',
    '2. **A proof leg that grepped the census WORDING and passed on a run that',
    '   proved the opposite.** `s2_proof.sh` leg C was meant to prove "one chunk',
    '   rebuilt beside three cached ones". It grepped for `"no native chunk cache"`,',
    '   which that line always prints, so it passed while the census actually said',
    '   `4 of 4 chunks dirty` and nothing whatever had been cached -- the mixed path',
    '   never ran. Fixed by reading the two NUMBERS out and asserting them',
    '   (`CDIRTY -eq 1`, `CREP -ge 1`). **Rule:** never grep a census line\'s prose.',
    '   Parse the numbers and assert them, and give every leg a floor that makes the',
    '   vacuous case fail. (This is the same shape as "a check that passed because',
    '   both sides were empty", and it caught a REAL defect the moment it was fixed:',
    '   a single lost cache file was rebaking the whole region.)',
    '',
    '3. **Hashed a pipe on Windows and called the product red.** Gate arm (f) holds',
    '   `lodbNormalise()` (C++) against `lodb_read.normalise_file()` (Python) on one',
    '   file. It reported two different sha1s over the same 77 lines. The C++ was',
    '   right and so was the Python: `lodb_read.py --normalise` used',
    '   `sys.stdout.write()`, and a text-mode stdout on Windows turns every LF into',
    '   CRLF, so the gate hashed a byte stream neither normaliser ever produced.',
    '   Fixed in two places -- the CLI writes `sys.stdout.buffer.write(...encode())`,',
    '   and the gate takes the digest INSIDE python. **Rule:** a digest is taken over',
    '   bytes, never over a pipe, and a cross-check that goes red starts by',
    '   suspecting the harness, not the product. (Memory rule "line endings measured',
    '   with PYTHON BYTE COUNTS ONLY" already said this; I applied it to files and',
    '   not to stdout.)',
    '',
    '4. **Handed a Windows python a Git-Bash path.** The replacement for (3) spliced',
    '   `r\'/e/Projects/...\'` into a shell-quoted `python -c "..."`. Python died,',
    '   printed nothing, and the arm compared a real digest against an empty string',
    '   and reported "the two normalisers disagree". Fixed by converting with the',
    '   harness\'s own `win()` helper and passing the paths as `sys.argv`, not inside',
    '   the quoted program. **Rule:** paths cross into a Windows tool converted and',
    '   as arguments; a tool that prints nothing is a failure, so a harness that can',
    '   read an empty result must say `?` and go red as vacuous, not as a mismatch.',
    '',
    '5. **A byte gate that was measuring the linker.** `lodgen_layout.sh` leg (c)',
    '   byte-compares the whole stock tree against a rung exe\'s bake. Four of its',
    '   five failures were `DIFFERS: Commonwealth.lodb` at four dims -- because the',
    '   record\'s header line stamps the exe\'s version AND ITS SIZE IN BYTES. Two',
    '   different exes can never write the same record bytes, so that leg goes red on',
    '   any lane that adds a line of code, and it had nothing to do with the LOD. The',
    '   record is now excepted with its reason printed and compared on its substance',
    '   instead. **Rule:** before adding a file to a byte-identity gate, ask what in',
    '   it is a function of the BUILD rather than of the bake -- and when a file is',
    '   excepted, print where it IS still measured, so the exception cannot quietly',
    '   become "nobody checks it".',
    '',
    '6. **Three small ones, one line each.** Wrote a field count from the struct I',
    '   remembered rather than from the row I had (25, not 22) -- count the fields in',
    '   a real line. Missed two `count == 1` anchors because I typed the indentation',
    '   from the rendered view instead of measuring it with `cat -A` (7 tabs, not 8).',
    '   Left two non-compiling lines in a patch script I had written by hand --',
    '   run the patch and compile before writing the next one.',
    '',
    '---',
    '',
    '',
])

patch('docs/MISTAKES.md',
      '## 2026-09-17 — Every resource gate opened a file BY PATH,',
      DOCS,
      'landed on the only pipeline anybody bakes')

patch('MISTAKES.md',
      '## 2026-09-17 05:3x -- director, OPENHERE1 out-of-tree build',
      MINE,
      'lane INCR1 (the FO4CS chunk cache and its gate)')
