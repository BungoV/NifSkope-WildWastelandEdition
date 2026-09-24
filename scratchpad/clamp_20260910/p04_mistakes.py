# LANE CLAMP: two entries at the top of MISTAKES.md (CONSTITUTION rule 2).
import sys

PATH = 'MISTAKES.md'
raw = open(PATH, 'rb').read()
cr_before = raw.count(b'\r')
if cr_before != 0:
    print('MISTAKES.md is not LF-only (%d CR); refusing' % cr_before); sys.exit(1)
s = raw.decode('utf-8')

ANCHOR = '## 2026-09-10 -- lane HOOKCAM wrote into a file another lane owned'
if s.count(ANCHOR) != 1:
    print('anchor count %d' % s.count(ANCHOR)); sys.exit(1)

new = '\n'.join([
    '## 2026-09-10 -- lane CLAMP re-derived a line number from the WRONG END of a range',
    '',
    '- **What was done:** the provenance pass over `docs/LODGEN_TERRAIN_VT.md`',
    '  rewrote the row `per-level worldUnitsPerTile / unitsPerTexel` from',
    '  `lodgen.cpp:6831-6833` to `7083-7085`, by finding the row\'s anchor text and',
    '  treating its line as the range START.',
    '- **What was true instead:** that row\'s anchor quotes `unitsPerTexel`, which is',
    '  the range\'s LAST line, not its first. The range is 7081-7083. The script had',
    '  produced a plausible wrong number -- two lines past the end of the block, into',
    '  code that has nothing to do with the claim.',
    '- **How it was found:** the grep that located every anchor was read beside the',
    '  script\'s output, and `worldUnitsPerTile` sat at 7081 while the row now said',
    '  7083. Nothing in the script could have caught it: it never looked at the other',
    '  end of the range.',
    '- **The rule:** a RANGE row is only re-derivable when its anchor is the range\'s',
    '  FIRST line. The `ww-contract-provenance` skill already says a range anchors its',
    '  start and marks the rest with an ellipsis; the repair is to make the row obey',
    '  that -- the anchor now quotes `worldUnitsPerTile` first and `unitsPerTexel`',
    '  after an ellipsis -- and for a script to REFUSE a range row whose anchor is not',
    '  its first line rather than guess. Same family as the entry the skill records',
    '  from 2026-09-09: a stale number announces itself, a plausible wrong one does',
    '  not.',
    '',
    '## 2026-09-10 -- lane CLAMP hit the relative `--out-dir` trap that is already in this file',
    '',
    '- **What was done:** the "before" bakes were run from a scratch copy of',
    '  `release/` with `--tex-dir before/cover/tex`, and the follow-up `find before',
    '  -name "*.DDS"` returned nothing. The bake had exited 0 and printed its census',
    '  rows, so for a moment the reading was that the bake wrote no sheets.',
    '- **What was true instead:** every sheet was there, under',
    '  `ns_before/before/cover/tex` -- a RELATIVE output path resolves against the',
    '  EXE\'s folder, not the working directory. That is already an entry in this file',
    '  (lane IMAGES, 2026-09-09) and a line in the memory index.',
    '- **How it was found:** `find` over the copy of `release/` instead of over the',
    '  intended directory.',
    '- **The rule, and the reason this repeat is its own entry (CONSTITUTION 2):**',
    '  every lodgen CLI path -- `--out-dir`, `--tex-dir`, `--lodt`, `-o` -- is passed',
    '  ABSOLUTE. Knowing a trap is in the ledger is not the same as applying it; the',
    '  cheap defence is a rule about the argument, not a memory about the tool.',
    '',
])

s = s.replace(ANCHOR, new + ANCHOR)
out = s.encode('utf-8')
if out.count(b'\r') != 0:
    print('CR appeared'); sys.exit(1)
open(PATH, 'wb').write(out)
print('ok  CR 0, lines %d' % len(s.split('\n')))
