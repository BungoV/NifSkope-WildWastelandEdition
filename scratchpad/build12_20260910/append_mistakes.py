"""Append lane BUILD12's MISTAKES.md entries. APPEND-ONLY: the original bytes
must be a prefix of the result, the CR count must not move (the file is
LF-only), and the file must grow by exactly the text appended."""
import io, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(ROOT)  # scratchpad/build12_20260910 -> repo root
P = os.path.join(ROOT, "MISTAKES.md")

TEXT = """
## 2026-09-10, lane BUILD12: a resume's "corrected" marker count was corrected in the WRONG DIRECTION

**What was done.** `scratchpad/water7_20260910/PENDING.md` tabulates the marker
strings a resume reads the applied state from, with the count each reaches once
`hookup.py --apply` has run. Two of its six rows say 2:
`wwBarRowButtonQss` and `UI/CompactTopBars`. Lane WATER7 wrote a MISTAKES entry
the same hour ("marker counts typed instead of derived, again") saying it had
listed `wwBarRowButtonQss` at 3 "where the truth is 2", and made
`hookup.py` DERIVE every expectation from its own EDITS table so the two could
not drift.

**What was true instead.** The derivation says **3**, and after `--apply` the
file holds **3** (`src/nifskope_ui.cpp`: the definition at line 631, the call
at 657, and a comment at 29784 that names it). `UI/CompactTopBars` is 3 as
well, not 2. So the fix -- deriving the number -- was right and landed, and the
PROSE correction beside it went the other way: the typed number was replaced
with a second typed number, in a table the script does not read, and the entry
recording the mistake states the wrong value as the truth.

**How it was found.** By running `--check`, then `--apply`, then `--check`
again, and reading the script's own two columns side by side: it prints
`0 (applied = 3)` before and `3 (applied = 3)` after. The table in the resume
was never consulted for the decision, which is why the apply was not affected.

**The rule.** When a number is moved into a derivation, DELETE the typed copy
rather than correcting it -- a prose table and a MISTAKES entry that quote a
derived number are two more places for it to be wrong, and nothing checks
them. If a document must show the number, show the command that prints it.

## 2026-09-10, lane BUILD12: a gate whose comment promises 1 px and whose assertion allows 8

**What was done.** `tests/spells/water_ui.sh` gate R3 is the half of bungo's
ruling that says *"compact these vertically like this, the top bar and the
buttons"*. Its header states it as: "every button in those bars takes the
row's height and agrees with its neighbours within 1 px (floor: at least 4
buttons found)". It ran green on the first build.

**What was true instead.** The check that actually executes is
`every button in the row is within **8** px of the row height`, and the measured
buttons are **39 px inside a 35 px row** -- 4 px taller than the bar that holds
them. The 1 px is enforced only BETWEEN the buttons (`39..39`), which is a
different claim: four buttons that are all equally wrong pass it. So the gate
is green while the number it exists to constrain is out by 4, and the report
would have said "the buttons are compact" on its word.

**How it was found.** By reading the log's R block rather than the spell's
verdict: the line prints `R3: 4 bar buttons, heights 39..39, row 35` beside its
own `ok`, which is the only reason the mismatch was visible at all.

**The rule.** `ww-test-harness-add`'s "floors that cannot fire" has a twin: a
tolerance that cannot fail on the defect the gate was written for. A gate's
header comment and its assertion are checked against each other in the same
sitting, and a check that quotes a tolerance PRINTS the measured value beside
it -- as this one does -- so the first run can be read for the number and not
just for the colour. Not fixed here: a build lane's product is a verdict
(`nifskope-ww-resume-pending` s6), and the arithmetic that produces 39
(`min-height: rowHeight - 4` plus `(rowHeight - 18) / 2` of padding, in
`wwBarRowButtonQss`) is a design decision for the director.
"""


def main():
    before = open(P, "rb").read()
    cr, lf, n = before.count(b"\r"), before.count(b"\n"), len(before)
    add = TEXT.encode("utf-8")
    assert b"\r" not in add, "the appended text must be LF-only"
    assert before.endswith(b"\n"), "file does not end with a newline"
    open(P, "wb").write(before + add)
    after = open(P, "rb").read()
    assert after.startswith(before), "NOT append-only"
    assert len(after) == n + len(add), (len(after), n + len(add))
    assert after.count(b"\r") == cr, (after.count(b"\r"), cr)
    print("MISTAKES.md %d -> %d bytes, CR %d -> %d, LF %d -> %d"
          % (n, len(after), cr, after.count(b"\r"), lf, after.count(b"\n")))


main()
