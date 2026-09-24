"""IMPOSTORFIX3 -- four entries spliced into the top of the root MISTAKES.md.

The file is CRLF. This is a BYTE splice: the new text is encoded with CRLF
explicitly, the CR count is printed before and after, and the difference must
equal the number of lines added or the script refuses to write.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
raw = open(P, 'rb').read()
cr0 = raw.count(b'\r')
lf0 = raw.count(b'\n')

ANCHOR = b'Newest at the top.\r\n\r\n'
if raw.count(ANCHOR) != 1:
    print('ANCHOR COUNT %d (want 1)' % raw.count(ANCHOR)); sys.exit(2)

NEW = """## 2026-09-19 -- IMPOSTORFIX3 -- a new gate clause that could not fail on the sheets it was written to convict

`impostor_sheet_check.py` clause (b) was rewritten to follow the 8-ring height
fill as: "the first 8 rings outside the silhouette must lie in the band the
frame's own whole texels occupy". That reads like a strict test and it is not
one. Measured against exe af457755's own sheets, where every texel out there is
the card plane, it failed 0 frames of 16 on FOUR of the five fixture subjects.
The band is the object's FULL depth range and an object centred on its own card
straddles the plane by construction, so the plane is inside the band for almost
every frame.

Found by writing a throwaway probe (`probe_cliff.py`) that scored the proposed
clause and one alternative side by side on both sheet sets BEFORE the clause
was trusted -- the same discipline the gate's own red controls use, applied to
the check instead of to the code. The clause that does separate them is
CONTINUITY ACROSS THE SILHOUETTE: an uncovered texel touching an inked one must
carry a height within SLACK of the mean of its inked neighbours. That fails on
all five subjects before the repair (4/16, 17/64, 3/16, 6/16, 16/16) and on two
after.

THE RULE: CONSTITUTION 4 is not satisfied by a check that sounds strict. Run
the new clause against the OLD state and watch the number go red, and if it
does not, the clause is wrong -- not the input. A check that cannot fail on the
input it names is not a check, and a gate full of them passes a broken build
with a clean log.

## 2026-09-19 -- IMPOSTORFIX3 -- a known-answer control whose endpoint hid half the defect

The BC3 six-level alpha ramp had TWO defects: the coefficient was `(4-k)` where
D3D says `(5-k)`, and index 5 was never written. The first known-answer block
built for it used a0 = 0, a1 = 200, and reported 3 wrong entries -- all of them
the missing index. The arithmetic error was invisible, because `(4-k)*a0` and
`(5-k)*a0` are both zero when a0 is zero.

Found by asking why a control with an obvious defect in it convicted only one
of the two. Rebuilt with a0 = 40, a1 = 200: 10 of 16 entries wrong across both
modes, 0 of 16 after the repair.

THE RULE: a known-answer input must exercise every coefficient, not just every
index. Zero and one are the two values that hide multiplication errors, so a
control built from them is the weakest one available. Pick endpoints that make
every term of the formula contribute.

## 2026-09-19 -- IMPOSTORFIX3 -- a rebake wrapper whose extra flag never reached the tool, and which deleted the output before failing

The rock is not a tree, and the impostor bake defaults to trees-only. The
rebake was launched as `EXTRA="--no-trees-only" sh rebake_dds.sh rock_n4`; that
wrapper does not forward `EXTRA`, so lodgen ran with its default and stopped
with `no LOD-bearing refs in chunk (-32,16)x16 (0 placed, 7630 without a usable
LOD model)`. The run had already deleted the rock's existing `.DDS` files by
then. It was recoverable only because the old sheets had been copied to
`cards_r3/` minutes earlier for an unrelated reason.

THE RULE: before passing a variable to a script someone else wrote, grep that
script for the variable's name. And a wrapper that deletes its output before
the tool it wraps has succeeded is a wrapper that loses work on every failure
-- write the new files beside the old ones and swap, or copy first.

## 2026-09-19 -- IMPOSTORFIX3 -- two sheet sets in one fixture resolve to the same sheets

The A/B for the height fill was going to be two `.lodm` files side by side in
one fixture directory, `cards/` and `cards_r3/`. That measures nothing:
`registerLooseSheets` walks UP from the `.lodm` looking for the nearest
ancestor holding a `textures/` tree, so BOTH `.lodm` files would have resolved
the same `_oct_n.DDS` and the two runs would have differed by nothing at all --
and would have printed two plausible, slightly different numbers, because the
harness's own IoU has run-to-run jitter.

Caught before it produced a number, by asking how the sheet is found rather
than assuming the `.lodm` names it. The fix is a wholly separate fixture root
with its own `textures/` tree; the check that it worked is that the two
`_oct_n.DDS` files differ AND that the old root reproduces the previous lane's
published IoU to four digits (0.5038 / 0.6754 / 0.3545).

THE RULE: when an A/B turns on which FILE is loaded, prove the two arms load
different bytes before believing either number. Two runs that differ by 0.004
and should have differed by 0.07 look exactly like a repair that did not help.

"""

out = raw.replace(ANCHOR, ANCHOR + NEW.replace('\n', '\r\n').encode('utf-8'))
cr1 = out.count(b'\r')
lf1 = out.count(b'\n')
added = NEW.count('\n')
print('CR before %d after %d (+%d)   LF before %d after %d (+%d)   lines added %d'
      % (cr0, cr1, cr1 - cr0, lf0, lf1, lf1 - lf0, added))
if cr1 - cr0 != added or lf1 - lf0 != added or cr1 != lf1:
    print('REFUSED: the line endings do not balance.'); sys.exit(2)
open(P, 'wb').write(out)
print('written %s: %d -> %d bytes' % (P, len(raw), len(out)))
