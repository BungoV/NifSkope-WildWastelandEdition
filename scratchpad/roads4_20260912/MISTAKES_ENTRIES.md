Three entries for `MISTAKES.md` at the repo root, newest first. Written by lane
ROADS4, 2026-09-12.

---

## 2026-09-12 -- An inherited chain was run AFTER the lane's own source edits landed on disk

**Lane ROADS4.** Item -1 of the brief was lane UINOTES1b's unfinished business:
run `lodgen_chain.sh` and `ui_chain.sh after` on the 05:48:33 exe and report the
rows. I edited `src/lodgen.h`, `src/lodgen.cpp` and `src/nifcli.cpp` at
06:07-06:08 and started the chains at 06:10. Two harnesses --
`lodgen_terrain_vt` and `lodgen_ground_cover` -- answer "the exe is newer than
every source this answer depends on", and it no longer was, so both reported a
failure whose only cause was my own uncommitted edits. Their rows had to be
reported as **unreadable** rather than as the pass/fail the inheriting lane
needed.

**The rule.** An inherited chain measures the tree it was written for. It runs
**before any of the new lane's edits touch disk**, or its staleness checks are
measuring the new lane instead of the old exe. If edits are already down, say so
in the row rather than reporting the number as a regression.

---

## 2026-09-12 -- A brief's premise was built on for an hour before the two-line count that refutes it

**Lane ROADS4.** The brief's title was *"the road meshes carry terrain-shaped
skirt geometry; the far bake must not paint it as road"*, and item 1 asked for
trunk-vs-skirt triangles by vertex alpha. A classifier was written, a projection
library extended, and per-material tables produced -- and then the count that
decides the whole question came out:

```
SKIRT ONLY (a skirt triangle is the only cover)   0 texels   on (-20,20)
SKIRT ONLY                                        0 texels   on (-8,8)
max-z winner IS a skirt triangle                455 of 23,116 / 556 of 11,069
vertex alpha on the road mask, mean               0.9887 / 0.9756
```

There is no skirt-only paint anywhere on either chunk, so there is nothing for a
vertex-alpha rule to suppress. Two lines of numpy, and they belong at line one
of the lane, not after the instrument that assumes the premise.

**The rule.** A brief's premise is a hypothesis, not a finding. The FIRST thing
a lane writes is the count that would refute it -- and if that count is
available offline (it was: a projection and a mask), it is written before any
code that depends on the premise being true.

---

## 2026-09-12 -- A zero-initialised buffer produced a finding that survived into another lane's gate

**Lane ROADS3 made it, lane ROADS4 found it.**
`scratchpad/roads2_20260911/seam.py` allocated its per-texel vertex-alpha buffer
as `abuf = [np.zeros(...), np.zeros(...)]` and wrote into it only for shapes
that actually carry a vertex-alpha channel. Every texel won by a shape with no
alpha channel therefore read **0.0** -- meaning "fully transparent" -- when the
correct default is **1.0**. The reported result, *"luminance correlates -0.792
with vertex alpha"*, was a correlation with **which shapes happen to have an
alpha channel**, and it became gate G1 in the next lane's brief.

Re-derived with the buffer initialised to ones: the correlation is **-0.0345**
against vanilla's **-0.0359** on chunk (-20,20), and **+0.0404** against
**+0.0766** on (-8,8) -- no signal on either tile, on ours or on Bethesda's.

**The rule, and it is the general one.** A buffer's fill value is a claim about
the world. `np.zeros` says "absent means zero"; for a coverage, an alpha or a
weight, absent almost always means **one**. Three tests, all cheap:

1. name the default out loud before allocating ("a shape with no vertex alpha is
   fully opaque") and make the allocation say it (`np.ones`);
2. **run the instrument on VANILLA as well as on ours.** Bethesda's sheet read
   the same -0.79, which should have ended the finding on the spot: a defect
   this lane introduced cannot be present in a sheet shipped in 2015;
3. count how many texels took the default. 440 of 23,116 texels here have alpha
   below 0.99 -- so the "correlation" was carried by the other 22,676, all of
   which were being reported as transparent.

`ww-spec-gate-audit` already says to ask whether an approximation is one-sided.
A wrong fill value is the purest case of that: it can only ever push one way.
