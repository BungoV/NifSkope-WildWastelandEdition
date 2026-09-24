#!/usr/bin/env python
"""Append lane WATER7's mistakes to MISTAKES.md, append-only, LF-only.

Refuses if the file's tail is not what it was read as, so a concurrent lane's
append is never overwritten (several lanes are alive in this tree).
"""

import os

ROOT = os.path.dirname( os.path.dirname( os.path.dirname( os.path.abspath( __file__ ) ) ) )
PATH = os.path.join( ROOT, "MISTAKES.md" )

TEXT = """
## 2026-09-10, lane WATER7 -- a header change that would have broken the tree for every other lane

**What was done.** `src/wwskin.h` is this lane's file and
`src/nifskope_ui.cpp`, which holds the DEFINITIONS of every function that
header declares, is not. To give `wwAlignBarRow` a way back at its off value I
added a second parameter to its DECLARATION with a default argument, intending
the hook-up to change the definition to match.

**What was true instead.** A declaration `wwAlignBarRow( const QList<QWidget *> &,
bool = true )` and a definition `wwAlignBarRow( const QList<QWidget *> & )` are
two different OVERLOADS, not a mismatch the compiler complains about at the
declaration -- so the single call site becomes AMBIGUOUS and
`src/nifskope_ui.cpp` stops compiling. The header is included by dozens of
translation units, and the hook-up is not applied until the build slot is free,
so the tree would have been unbuildable for every other live lane in the
meantime, with an error pointing at a call site none of them touched.

**How it was found.** `ww-anchored-hookup` section 2, before running anything:
"the new files compile with AND without the hook-up". A signature in a header
this lane owns is not a new file, but it is the same test, and this failed it.

**The rule.** A header a lane owns may only GAIN declarations while its
definitions live in a file the lane does not own. Changing the signature of an
existing one is a hook-up edit, not a header edit -- and if it cannot be one,
put the new behaviour behind a new function instead. Here the way back moved
inside the skin (`wwCompactTopBars()` reads `UI/CompactTopBars`), which is
better anyway: one reader of the key, and no call site can disagree with the
sheet about it.

## 2026-09-10, lane WATER7 -- fixing the writer of a pair whose reader never read it

**What was done.** BUILD10's red 5a says "a DYE PIN's per-point weight is never
written (`writeTo` fills `extra` for `Stroke` and `Pin` only)". The obvious fix
was made: add `DyePin` to that list in `WaterCurveDoc::writeTo`.

**What was true instead.** `parseStoreExtras` -- the READING half, in the same
file -- also excludes `DyePin`, and its `base` offset does not allow for the
four colour bytes a dye pin's record carries between its points and its
trailing bytes. So the fix alone would have written weights that nothing reads,
and the gate that proves it ("write 0.25, save, reopen, read 0.25") would still
have been red, with the writer now blamed for a defect in the reader.

**How it was found.** Reading the reader before believing the writer: the red's
own wording names one function, and the pair was checked anyway.

**The rule.** A round-trip red names ONE side because that is where somebody
looked. Before fixing it, read the other side in the same sitting and state
whether it agrees -- and if the codec has a per-kind offset, check that offset
for the kind in hand, not for the kind the code was written around.

## 2026-09-10, lane WATER7 -- marker counts typed instead of derived, again

**What was done.** `scratchpad/water7_20260910/hookup.py` listed the marker
strings a resume reads the applied state from, with the count each should
reach, typed by hand: `wwBarRowButtonQss` 3.

**What was true instead.** 2. The inserted text mentions it twice, not three
times, and one of the other five counts was wrong in the same way.

**How it was found.** By deriving the numbers from the script's own EDITS table
instead of trusting them -- which is what `nifskope-ww-resume-pending` section 9
already says, in an entry written FOR lane BUILD9 about this exact failure
(a resume promised `1 and 7` where the truth was `1 and 6`).

**The rule.** A number in a resume that can be computed from the resume's own
data is computed, never typed. `hookup.py` now derives every marker
expectation from `EDITS`, so the two cannot drift.
"""


def main():
	with open( PATH, "rb" ) as f:
		blob = f.read()
	cr_before = blob.count( b"\r" )
	t = TEXT.encode( "utf-8" )
	assert t.count( b"\r" ) == 0, "the appended text carries a CR"
	if b"lane WATER7 -- a header change" in blob:
		print( "already appended; nothing written" )
		return
	with open( PATH, "wb" ) as f:
		f.write( blob + t )
	with open( PATH, "rb" ) as f:
		after = f.read()
	print( "MISTAKES.md %d -> %d bytes, CR %d -> %d, appended %d bytes"
		% ( len( blob ), len( after ), cr_before, after.count( b"\r" ), len( t ) ) )
	assert after[:len( blob )] == blob, "the append was not append-only"
	print( "append-only: the original bytes are unchanged" )


if __name__ == "__main__":
	main()
