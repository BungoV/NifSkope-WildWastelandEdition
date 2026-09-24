"""INCR1 step 6a -- docs/LODGEN_LEDGER_FORMAT.md catches up with the code.

Three things in that page are now false:

  * section 4 says `--native` is on the whole-region refusal list. It is not:
    that refusal is what this lane was opened to remove, and it now fires only
    with `--no-native-cache`.
  * section 5's census line has four fields inside the bracket. It has five,
    and there is a second line beside it.
  * section 6 names a scratchpad script from lane LAND1 as the gate. There is
    a gate in `tests/spells/` now.

A doc that describes a refusal the code no longer makes is worse than no doc:
it is a claim about disk (root MISTAKES.md, 2026-09-1x) and it goes stale.

Anchors asserted count == 1. LF preserved.
"""
import io
import os
import sys

NL = chr(10)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'docs/LODGEN_LEDGER_FORMAT.md'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')
if '.lodj' in t:
    raise SystemExit(rel + ' already knows about the chunk cache')


def sub1(text, old, new, what):
    n = text.count(old)
    if n != 1:
        raise SystemExit('ANCHOR %s: found %d times, wanted exactly 1' % (what, n))
    return text.replace(old, new, 1)


# ---- 4. the refusal table ----------------------------------------------------
t = sub1(t,
         '| `LODGEN_INCR_WHOLE_REGION` | `--atlas`, `--arrays`, `--impostors` or `--native` is asked for | do those in a separate full pass over the finished chunks (section 2). The merge and the far-ring simplify are **not** on this list |',
         NL.join([
             '| `LODGEN_INCR_WHOLE_REGION` | `--atlas`, `--arrays` or `--impostors` is asked for, or `--native` **together with `--no-native-cache`** | do those in a separate full pass over the finished chunks (section 2). The merge and the far-ring simplify are **not** on this list |',
             '| *(no enum, printed inline)* | a `.lodj` chunk cache could not be written, or could not be replayed | bake without `--incremental`: the pair this run would write is missing whole chunks |',
             '| *(no enum, printed inline)* | a replay ran **and** some placement was lit by more than one chunk | bake without `--incremental`: a full bake adds those sums in the one order there is |',
         ]),
         'whole-region row')

t = sub1(t,
         NL.join([
             '`--native` was added to the whole-region refusal on 2026-09-12 and it is the',
             'one arm of that refusal whose absence would not have shown in the output tree.',
         ]),
         NL.join([
             '`--native` was on the whole-region refusal from 2026-09-12 until 2026-09-17,',
             'and it is the one arm of that refusal whose absence would not have shown in',
             'the output tree.',
         ]),
         'native paragraph head')

t = sub1(t,
         NL.join([
             '`--native-verify --native-verify-corpus`, matches its own three staleness',
             'hashes, and is simply missing most of the worldspace. The pass collects one',
             '`NativePlacement` per drawn reference and one lighting sample per vertex',
             '**inside the chunk pass** (`src/lodgen.cpp:3784` and `:4069`), so it can only',
             'ever see the chunks that were rebaked.',
             '',
             '---',
         ]),
         NL.join([
             '`--native-verify --native-verify-corpus`, matches its own three staleness',
             'hashes, and is simply missing most of the worldspace. The pass collects one',
             '`NativePlacement` per drawn reference and one lighting sample per vertex',
             '**inside the chunk pass** (`src/lodgen.cpp:3784` and `:4069`), so it can only',
             'ever see the chunks that were rebaked.',
             '',
             '### 4.1 The `.lodj` chunk cache, and why the refusal is gone',
             '',
             'That refusal read, in practice, "refuse the only target anybody bakes".',
             'bungo\'s ruled FO4CS command is `--native <dir>`, everything under',
             '`Data/FO4CSLOD/`; `--incremental` therefore refused the default pipeline and',
             'was usable only on the stock engine target. The gap was the point of lane',
             'INCR1 (2026-09-17).',
             '',
             'A skipped chunk now **speaks from a file** instead of being silently missing',
             'from the pair. Every `--native` bake writes, beside the pair,',
             '',
             '    FO4CSLOD/<ws>/<ws>.<dim>.<cx>.<cy>.lodj',
             '',
             "one a chunk: that chunk's whole contribution to the region's `.lodo`/`.lodi`",
             '-- every placement it emitted, **in emission order**, and the exact lighting',
             'and placement-AO sums it accumulated per object index. On an incremental run',
             'the chunk pass replays each cached chunk **in that chunk\'s own queue',
             'position**, so the arrival order -- which the library\'s mesh ids depend on --',
             'is the order a full bake would have produced. Proved, on a real bake of four',
             'chunks: a null incremental and a mixed one (one chunk rebuilt beside three',
             'cached) each rewrote the `.lodo` and the `.lodi` **byte for byte**',
             '(`scratchpad/incr1_20260917/s2_proof.txt`).',
             '',
             'Two rules the file obeys, both for the same reason -- the pair must come out',
             'bit-identical, not nearly identical:',
             '',
             '* **every number is hex.** Positions, rotations and scale are `%08x` IEEE-754',
             '  bit patterns and the lighting sums are `%016x` doubles. A decimal round',
             '  trip of a float is not an identity.',
             '* **a placement lit by more than one chunk is refused, not hoped through.** An',
             '  arrival is deduped on `(refForm, scolPart)` **across** chunks, so a',
             '  placement two chunks both light has sums built from both, and',
             '  `(prev + a1) + a2` is not `prev + (a1 + a2)` in floating point. Zero is the',
             '  ordinary answer, because lighting is keyed on the chunk\'s own identity',
             '  index; anything else means the pair would be NEARLY right.',
             '',
             'The exact way back is `--no-native-cache`: no `.lodj` is written, and an',
             '`--incremental --native` run then refuses with exactly the words it used',
             'before this lane. It is there so the cost of the cache can be measured, and',
             'so the old behaviour is reachable rather than merely remembered. Measured on',
             'that four-chunk bake: 658,970 bytes for 2,243 placements = **294 bytes a',
             'placement**, 0.29 % of the 225,399,755-byte `.lodo`.',
             '',
             '### 4.2 The two dirty lists, and why they are two',
             '',
             'A dirty chunk goes on `dirty`. Only some dirty chunks go on `dirtyWide`, and',
             '**the widening is seeded from `dirtyWide` alone**.',
             '',
             'The widening exists because the terrain ring and the AO skirt each reach one',
             'cell, so a chunk whose **inputs** moved changes what its neighbours draw. A',
             '`.lodj` is not an input to anything: it is this tree\'s record of what a chunk',
             'once emitted, and losing it changes exactly one chunk\'s work, because',
             'rebaking that chunk writes the same bytes again.',
             '',
             'This was measured the wrong way round first. Deleting **one** cache file took',
             'the ordinary "an output is missing or edited" path, which seeds the widening,',
             'and the census said `4 of 4 chunks dirty` -- one lost cache rebaked the whole',
             'region, and the harness leg that was meant to prove "one chunk rebuilt beside',
             'three cached ones" had no cached ones in it at all. So the output loop now',
             'walks **every** output rather than breaking on the first lost one, and a',
             'chunk whose only lost output ends in `.lodj` goes on `dirty` and not on',
             '`dirtyWide`. After: `1 of 4 dirty, 3 replayed from cache`.',
             '',
             '---',
         ]),
         'native paragraph tail')

# ---- 5. the census line ------------------------------------------------------
t = sub1(t,
         NL.join([
             '```',
             'incremental: 4 of 9 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 3 by neighbour)',
             '  (-20,20) inputs 3f2a1c9d0e11 -> 9b70c4a2f1de',
             '```',
             '',
             'Read it every run. `9 of 9` means the diff found nothing to skip and the run is',
             'a full bake with extra bookkeeping — true and safe, but not what you asked for,',
             'and the line says so instead of letting a wall-clock reading imply it.',
         ]),
         NL.join([
             '```',
             'incremental: 4 of 9 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 3 by neighbour, 0 with no native chunk cache)',
             '  (-20,20) inputs 3f2a1c9d0e11 -> 9b70c4a2f1de',
             'native cache: 1 chunk(s) written to .lodj, 3 replayed from cache (1791 placement(s)), 0 failure(s), 0 arrival(s) lit by more than one chunk',
             '```',
             '',
             'Read them every run. `9 of 9` means the diff found nothing to skip and the run',
             'is a full bake with extra bookkeeping — true and safe, but not what you asked',
             'for, and the line says so instead of letting a wall-clock reading imply it.',
             '',
             'The bracket\'s **fifth** field, `with no native chunk cache`, counts chunks that',
             'were clean by every other measure and are being rebaked only because their',
             '`.lodj` is not on disk — the first `--incremental` after an older bake, or',
             'after somebody cleaned the folder. It is the field that tells a full-looking',
             'run apart from a genuinely dirty one.',
             '',
             'The `native cache:` line appears only on a `--native` run with the cache on.',
             '`0 replayed from cache` on a run that reported skipped chunks is a defect, not',
             'a performance note: it means the pair was assembled from the rebaked chunks',
             'alone.',
         ]),
         'census section')

# ---- 6. the gate -------------------------------------------------------------
t = sub1(t,
         NL.join([
             '`dirty rebake == full bake`, byte for byte, every file, the `.lodb` included.',
             'The measurement is `scratchpad/land1_20260912/b3_identity.sh` and its verdicts',
             'are in `scratchpad/lane_land1_report.md` section B3.',
         ]),
         NL.join([
             '`dirty rebake == full bake`, byte for byte, every file, the `.lodb` included.',
             '',
             'The standing gate is **`tests/spells/lodgen_incremental.sh`** (lane INCR1),',
             'six arms, each with its own floor:',
             '',
             '| arm | what it measures | floor |',
             '|---|---|---|',
             '| (a) | a null `--incremental --native` rewrites the SAME `.lodo`/`.lodi` | at least one `.lodj` on disk AND at least one chunk replayed |',
             '| (b) | one chunk rebuilt beside cached ones writes the same pair, and a deleted `.lodj` heals itself | at least one chunk rebaked AND at least one replayed |',
             '| (c) | a null `--incremental` on the STOCK target rewrites every file byte for byte | at least one file compared |',
             '| (d) | the record\'s SUBSTANCE after an incremental equals the full bake\'s | at least one `chunk` row AND one `out` row; both refuters (a deleted chunk row, a zeroed `out` digest) must fire |',
             '| (e) | `--no-native-cache` writes no `.lodj`, and the incremental then refuses naming the flag | the refusal must name the flag |',
             '| (f) | `lodbNormalise()` and `lodb_read.normalise_file()` agree on the same file, text and line count | the exe must have printed a digest |',
             '',
             'The earlier measurement, from lane LAND1, is',
             '`scratchpad/land1_20260912/b3_identity.sh`, with its verdicts in',
             '`scratchpad/lane_land1_report.md` section B3.',
         ]),
         'gate section')

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
