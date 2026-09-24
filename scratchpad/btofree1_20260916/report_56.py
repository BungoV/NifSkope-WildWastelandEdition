# Lane BTOFREE1, 2026-09-16 -- fills report sections 5 and 6.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'

S5 = '''## 5. Mistakes

Four, all written into `MISTAKES_ENTRIES.md` the moment each was recognised and spliced by me to
the top of the root `MISTAKES.md` (which is now 458,995 B, LF 7,741, CR 0 -- unchanged ending).
Short form:

1. **The backslash trap, on the third day it is in that ledger.** I reached for `python -c` with a
   `replace` of a path separator in it; the shell ate the escape and Python died on
   `EOL while scanning string literal`. It cost seconds only by luck -- the same collapse inside a
   patch anchor is silent. Every edit in this lane after that went through a file written with the
   Write tool, with `assert count == 1` on every anchor.
2. **Wrote the same teardown loop twice** -- once in `src/nifcli.cpp`, once headed for
   `src/lodgenmanager.cpp` -- and only then re-read `tests/spells/lodgen_byte_gate.sh` phase (c),
   which exists to catch the two front ends drifting apart. Fixed by extracting
   `lodgenDropBtoScratch()` into `src/lodgenchunkpass.cpp` and having both call it, which is why
   phase (c) has nothing to find.
3. **Counted tab stops by eye** into a 1.6 MB file; `wwNewRows[]` is seven tabs deep and I wrote
   six. The assert fired. The fix is structural: the patch script now stores its blocks unindented
   and applies depth with a helper, so the depth is a number I can check with one `sed | cat -A`.
4. **Read 0.8179 out of the brief as a current measurement** and spent the first minutes of the
   diagnosis looking for what had broken. Nothing had; the number was measured on the old library
   default. A discriminator bake reproduced it to four decimals. A number in a brief is a number
   from an earlier exe.

'''

S6 = '''## 6. Skill review

`nifskope-ww-lodgen` was read before I chose a single input or route, and again before the panel
row. What it was right about, and what it could not tell me:

**Load-bearing, used as written.**
* `bash tools/ww_build.sh <sources>` -- the only build route an account-B lane can take, since such
  a lane cannot invoke `/c/msys64/usr/bin/bash` itself. It also did the thing the skill warns about
  two sections later: a NifSkope window was holding `release/NifSkope.exe`, and the script renamed
  it aside as `NifSkope_inuse_2000.exe` rather than failing at the link.
* "a spell that shells out to `python` measures whichever `python` is on the PATH, and MSYS2's has
  no numpy" -- every measurement in this lane went through
  `/c/Users/bungo/AppData/Local/Programs/Python/Python39/python` by name, and
  `tests/spells/lodgen_btofree.sh` now names its interpreter in a `PY=` line for the same reason.
* The editing traps. All four mistakes above are traps the skill already names; three of them it
  names in the exact words that would have prevented them. That is not a gap in the skill.
* The panel house rules -- `xB(...)` with a tooltip that ends in the command-line switch, the row
  shown by `showExtra` under one target only, and the `wwNewRows[]` self-test row with its default
  -- are what made "Keep legacy .BTO chunks" indistinguishable from a row that shipped a month ago.

**What the skill did not say, and now does.** It described the two bake targets by what they *turn
on* and never by **what each one leaves on disk**, which is the whole question this lane was asked.
So the skill gained a section before "## The gates":
`## THE TWO BAKE TARGETS, AND WHAT EACH ONE LEAVES ON DISK (2026-09-16, lane BTOFREE1)` --
the FO4CS file list, the stock file list, the scratch folder and its name, `--keep-bto` /
"Keep legacy .BTO chunks" as the way back, the five post-passes that force a chunk to exist at all
(with their `lodgen.cpp` line numbers, so the next lane can check they are still five), and the
rule that the stock target's bytes are a hard gate. Both copies were updated:
`/.claude/skills/nifskope-ww-lodgen/SKILL.md` in the repo and
`E:/Projects/Claude/.claude/skills/nifskope-ww-lodgen/SKILL.md`, 38,639 -> 40,786 B, LF 517 -> 546,
CR 0 both.

**One thing I would still add, and did not, because it is the director's call.** The skill lists the
gates but not their *floors*; a lane learns that `lodgen_native.sh` is 120/0/2 only from a brief.
A table of "gate | today's count | what a drop means" in the skill would make a count regression
self-evident to a lane that never got a brief. I did not add numbers I cannot keep current from
inside one lane.
'''

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
for old, new in ((u'## 5. Mistakes\n\n(in progress)\n\n', S5),
                 (u'## 6. Skill review\n\n(in progress)\n', S6)):
    assert s.count(old) == 1, 'anchor count %d' % s.count(old)
    s = s.replace(old, new)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('report 5+6 written: %d -> %d bytes, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))
