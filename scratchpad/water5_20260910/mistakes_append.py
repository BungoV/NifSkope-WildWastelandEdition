#!/usr/bin/env python3
"""Splice lane WATER5's entries into MISTAKES.md at the top (newest first).
Anchor must match exactly once; the CR count (0) must not move."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PATH = os.path.join(ROOT, "MISTAKES.md")
ANCHOR = b"Newest at the top.\n\n"
MARK = b"(lane WATER5)"

ENTRY = """## 2026-09-10 -- two undo conventions in one stack (lane WATER5)

**What was done.** The water window's undo stack was written in one place as
"push the state AFTER an edit" (`edited()`, called by every map gesture after
it has changed the model) and in another as "push the state BEFORE"
(`loadCurves()` pushed the old document, then replaced it).

**What was true.** With both in one stack, an undo after a load would have
restored the pre-load state to the REDO side and left the loaded curves in
place -- the button would have done nothing visible, then done the wrong thing
on the next press.

**How it was found.** Re-reading the file before its syntax pass, listing
every writer of the stack and checking each against the comment on the stack.
Fixed to one convention (post-edit snapshots; undo pops and restores the one
beneath, or re-reads the file's own strokes).

**The rule.** A stack has ONE convention, written as a comment AT the stack,
and every pusher is checked against that comment before the pass -- the same
reading that caught WATER3's placeholder check.

## 2026-09-10 -- the anchored hook-up script was written for the fourth time (lane WATER5)

**What was done.** `scratchpad/water5_20260910/hookup.py` re-implements
"anchor matches exactly once, CR count unchanged, --check before --apply" --
the function lanes WATER2, WATER3 (`hookup.py`), WATER4 (`splice.py`) and
BUILD5b each wrote for themselves.

**What was true.** CONSTITUTION 1a: a procedure done twice becomes a skill
the same session.  WATER4's report asked the director to lift `splice.py`
into `tools/`; nobody had, and this lane typed it again before noticing.

**How it was found.** The finished-work skill review (section 8 of the lane
report) -- which is late: the rule says BEFORE the second use.

**The rule.** When a brief says "write the hook-up edits, unapplied", the
lane's FIRST act on that item is to look for the skill that shapes such a
script; it is now `ww-anchored-hookup` (written by this lane, repo skill
tree), and the script it describes should be lifted to `tools/` by the
director so the next lane imports rather than re-types.

"""


def main():
    b = open(PATH, "rb").read()
    cr = b.count(b"\r")
    assert b.count(ANCHOR) == 1, "anchor count %d" % b.count(ANCHOR)
    if b.count(MARK):
        print("already spliced (%d marks); nothing written" % b.count(MARK))
        return 0
    e = ENTRY.encode("utf-8")
    assert b"\r" not in e
    out = b.replace(ANCHOR, ANCHOR + e)
    assert out.count(b"\r") == cr
    open(PATH, "wb").write(out)
    print("MISTAKES.md %d -> %d bytes, CR %d -> %d" % (len(b), len(out), cr, out.count(b"\r")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
