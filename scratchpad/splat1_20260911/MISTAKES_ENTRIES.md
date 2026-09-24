## 2026-09-11, lane SPLAT1 -- a working spectrum instrument reported as broken

**What was done.** The known-answer control for the radially averaged spectrum
planted a 6-texel CHECKER and then looked for the peak at radius `1/6` cycles a
texel. It read 0.2x and the self-test went red, on an instrument that was
working.

**What was true instead.** A checker of period *p* has its fundamental at
`(1/p, 1/p)`, radius `sqrt(2)/p`. At the right radius the same run reads the
planted period at 6.87 power against a local median of 0.00.

**How it was found.** The other eleven self-test lines passed, including one
that says the metric separates smooth from speckled by 25.5x -- so the metric
could not be blind and the LOOKUP had to be wrong.

**The rule.** A known-answer control states WHERE the answer will appear, in the
units the instrument reports, and the script computes that place rather than the
author typing it. `splatlib.checker_radius()` now does it.

## 2026-09-11, lane SPLAT1 -- a constant whose own comment said it was a guess, shipped and cited

**What was done.** `src/lodgen.cpp:6198` has carried
`constexpr float TILE = 2048.0f` since the terrain bake was written, with a
comment saying "calibration against vanilla bakes is an open refinement -- the
constant only affects apparent texel density". `docs/LODGEN_TERRAIN_VT.md:734`
then wrote the same number into the CONTRACT as `u = frac(wx/2048)`, "the bake's
world-space tiling", citing the bake. A guess became law by being restated.

**What was true instead.** The engine's own number is 341.3333 world units a
repeat -- `fLandTextureTilingMult` 1.5 in Fallout4.exe 1.10.155, one code
reference, `uv = vertexIndex * mult/4` over the 17x17 quadrant grid. The bake is
6.0000x too coarse, and the comment's "only affects apparent texel density" was
wrong twice over: it is the whole of the speckle bungo saw, and it also moves the
roughness, metallic and emissive sheets.

**How it was found.** bungo asked whether the terrain textures use their correct
scale. Nobody had ever looked for the number outside our own tree.

**The rule (ww-contract-provenance).** A contract page may not cite the code it
is the contract FOR. A constant with no source outside the tree is written into
the page as UNSOURCED, by name, with what would source it -- and a comment
admitting a value is uncalibrated is a lane, not a footnote.

## 2026-09-11, lane SPLAT1 -- a contract number that does not reproduce (VCLR)

**What was done.** `docs/LODGEN_TERRAIN_VT.md` 2.5 states that over the
Sanctuary region, cells -20..-17 x 24..27, "every byte of every VCLR present is
in 249..255 -- white to within 6/255", and 2,362 of 36,864 cells carry a VCLR.

**What was true instead.** Over exactly those 16 cells, 11 carry a VCLR and the
byte range is **203..255**. Over cells -20..-17 x 20..23, 16 of 16 carry one,
range **170..255**.

**How it was found.** This lane needed the VCLR-flat cells to isolate VCLR from
the mip and printed the range rather than trusting the page.

**The rule.** A range quoted in a contract carries the script that produced it,
and a later lane re-runs that script rather than the sentence. The conclusion
here survived (VCLR moves the sheet by 0.02 of a 52-unit excess) but it survived
by luck.

## 2026-09-11, lane SPLAT1 -- line numbers quoted from a file another lane was editing

**What was done.** Sections 2 and 5 of the report were written citing
`src/lodgen.cpp` 6195/6198/6543/6551/7486/7661/7665/7699/7703/7718/7722 and
6933-6959, read at about 15:3x.

**What was true instead.** Lane CARDS-AGG wrote to the same file at 15:44. At
16:2x it is 10,275 lines (it was 10,137), sha1
`d81bfcc94016956a185867ca5f594fee46f7313c`, and every one of those numbers had
moved by about 137: `constexpr float TILE` is at 6335 and 7623, the mip block at
6688-6692 / 7802 / 7840 / 7859, the cover-stamp comment at 7070-7095. Fourteen
`TILE` sites, not eleven.

**How it was found.** The anchor pass of `ww-contract-provenance` was run as the
LAST step -- re-deriving every number from the live file with its anchor text
beside it -- instead of being taken as done because the numbers were read
carefully the first time.

**The rule.** In a tree with live lanes, a line number has a shelf life of
minutes. Cite by ANCHOR TEXT, keep the numbers in a generated `anchors.txt` with
the file's size, line count and sha1 beside them, re-derive at the end, and say
in the document that the file is live and the pass must be re-run.

## 2026-09-11, lane SPLAT1 -- another lane declared dead from a directory listing

**What was done.** SPLAT1's `PENDING.md` and `HANDOFF_BLOCK.md` said CARDS-AGG's
"newest file is `aggpicture.py` at 16:02 and nothing has changed since", and
concluded it had ENDED BUILD PENDING with a stale `BUILDING` marker, "exactly as
WATER8 did on 2026-09-10". The phase-B gate was then closed on that reasoning.

**What was true instead.** CARDS-AGG was alive and working: `lodm_3a.md` 16:16,
`lodi_4_6.md` 16:17, `gate/` 16:20. Its build genuinely had not run, and the gate
genuinely was shut -- but "it will never write a DONE" was not supported by
anything measured.

**How it was found.** A stale background waiter drained and its listing showed
the 16:16/16:17 files. Nothing in the lane's own polling would have shown it:
the poll only tested for `DONE` and `BUILDING`, never for ACTIVITY.

**The rule.** "Lane X is dead" is a claim about a PROCESS and needs a measurement
of that process -- the newest mtime in its directory at the moment of writing,
re-read in the same step as the sentence -- not one listing taken minutes
earlier. A gate-poll that watches only for markers must print the blocking
lane's newest mtime beside every poll, or it cannot tell "still working" from
"ended with the marker up". SPLAT1's real reason for ending BUILD PENDING is
that its own session ended; that is what the documents now say.
