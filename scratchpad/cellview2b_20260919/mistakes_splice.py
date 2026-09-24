#!/usr/bin/env python3
"""CELLVIEW2B: splice three entries into the root MISTAKES.md.

The file is PURE CRLF. It is opened in binary, the new text is built with
explicit \\r\\n, and the CR count is printed before and after -- the rule from
`feedback_crlf_python_edits` and from this file's own top entry. Nothing is
normalised and nothing else in the file is touched: the entries go immediately
after the six-line preamble, which ends at the first blank line before the
first `## ` heading.
"""
import sys

P = r"E:\Projects\NifskopeWildWastelandEdition\MISTAKES.md"

b = open(P, "rb").read()
cr_before, lf_before, len_before = b.count(b"\r"), b.count(b"\n"), len(b)
print("before: %d bytes, CR %d, LF %d, bare LF %d"
      % (len_before, cr_before, lf_before, lf_before - cr_before))

marker = b"\r\n## "
i = b.find(marker)
assert i > 0, "no heading found"
head, tail = b[:i + 2], b[i + 2:]

ENTRIES = """## 2026-09-19 -- CELLVIEW2B -- a handoff quirk that the header disproves in one line

What was done: lane CELLVIEW1's handoff says, as a standing quirk of the render
hook, "WW_RENDER_VIEW cannot select ViewTop, because glview.cpp:6450 maps v == 0
to ViewFront while ViewTop IS 0." A lane reading that would take its cell
pictures from the side, or write a hook-up to repair a defect that is not there.

What was true: `src/glview.h:452` is the enum, and it reads ViewDefault = 0,
**ViewTop = 1**, ViewBottom = 2, ViewLeft = 3, ViewRight = 4, ViewFront = 5,
ViewBack = 6, ViewWalk = 7, ViewUser = 8. So `WW_RENDER_VIEW=1` IS ViewTop, and
the v == 0 -> ViewFront line is the hook giving a sensible default to the
UNSET case, not a mis-map. Every picture this lane took used
`WW_RENDER_VIEW=1` and every one of them is top-down.

How it was found: by reading the enum before believing a sentence about it --
five seconds, against a hook-up that would have been written from the sentence.

The rule: a handoff bullet that names a line number is a claim about a file, and
the file is cheaper to read than the claim is to act on. A quirk is quoted with
the DECLARATION that makes it true (`grep -n "ViewTop" src/glview.h`), never
with the consumer that is alleged to mishandle it.

## 2026-09-19 -- CELLVIEW2B -- a gate row whose floor is its own whole population

What was done: `tests/spells/cell_pick.sh` row 7 was written by lane CELLVIEW2 as
"most quads resolved a texture", with the floor `textured >= 1024`.

What was true: the subject is a 1x1 cell block, and a cell's painted ground is
exactly 32 x 32 = **1024** quads. The floor is therefore not "most", it is ALL:
the row can only pass at one hundred per cent, and it fails at 891 textured /
133 bare -- a number that would satisfy any reading of the row's own name. The
measured cause of the 133 is separate and real (those quads' quadrant carries no
BTXT, no ATXT layer clears `CELL_GROUND_LAYER_MIN` at their SW corner, and
`src/cellground.cpp` has no worldspace-default landscape texture to fall back
on), and it is reported as this lane's one red -- but the row as written could
not have passed even with a cure that left a single quad bare.

How it was found: by working out the row's population before reading its verdict
-- 32 x 32 from `cellground.h`'s own grid constant, against the literal in the
floor.

The rule: a floor is written against the POPULATION, and the population is
computed and stated in the row's detail line ("891 of 1024"), never typed as a
bare literal that happens to equal it. A row named with a quantifier ("most",
"nearly all") whose floor equals the total is a naming defect in the gate, and
it is fixed in the gate -- not by moving the floor to whatever today's run
scored, which is the other half of the same mistake.

## 2026-09-19 -- CELLVIEW2B -- guessing a binary header's offsets twice over

What was done: to find whether any `.lodi` in the tree covers Sanctuary cell
-20,7, I read the chunk bounds out of the header by inference -- first assuming
a fixed-size buffer for the worldspace EDID, then, when the numbers came back
absurd, assuming a different order for the four bounds. Both readings were
wrong, and both produced plausible-looking cell rectangles.

What was true: `src/lodifile.cpp:30` names the four offsets outright --
`H_WEST = 0x48, H_SOUTH = 0x4A, H_EAST = 0x4C, H_NORTH = 0x4E` -- and reading
them gave the answer immediately: horizonout's bakes cover x 0..11 / y -12..-9
and x -4..11 / y -16..3, defaults1's covers x -20..-13 / y 24..31, and NONE of
them reaches -20,7, so a bake was owed.

How it was found: by stopping after the second wrong answer and opening the
writer, which is the file that decides what the offsets are.

The rule: this tree writes every binary format it reads, so the layout is never
a guess -- the constants are in the `*file.cpp` that writes them. Read the
writer FIRST, and treat a probe script built from inference as evidence of
nothing, however confident its output looks.

"""

ins = ENTRIES.replace("\n", "\r\n").encode("utf-8")
assert b"\n" not in ins.replace(b"\r\n", b""), "a bare LF got into the insert"
assert ins.count(b"\r") == ins.count(b"\n")

out = head + ins + tail
cr_after, lf_after = out.count(b"\r"), out.count(b"\n")
print("insert: %d bytes, CR %d" % (len(ins), ins.count(b"\r")))
print("after : %d bytes, CR %d, LF %d, bare LF %d"
      % (len(out), cr_after, lf_after, lf_after - cr_after))
assert cr_after == lf_after, "the file stopped being pure CRLF"
assert len(out) == len_before + len(ins)

if "--apply" in sys.argv:
    open(P, "wb").write(out)
    print("WROTE")
else:
    print("CHECK ONLY -- nothing written")
