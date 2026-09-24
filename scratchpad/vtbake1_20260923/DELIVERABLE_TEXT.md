# VTBAKE1 -- text for the overseer to splice (lane edits none of these files)

## HANDOFF (one entry)

VTBAKE1 2026-09-23: first whole-Commonwealth terrain texture pyramid baked from the CLI
(`lodgen Fallout4.esm --worldspace 3C --vt <scratch> --vt-height --data-root <unpacked>`), exe
release/NifSkope.exe 2026-09-23 07:54 sha1 8d87c155. 569 s wall, rc 0. 5 levels (dim 2/4/8/16/32,
9216/2304/576/144/36 tiles), 12,276 tiles all present, 4,022,886,880 B of containers + a 2,505 B
index at FO4CSLOD\Commonwealth\Commonwealth.VT.lodm. No cover (not asked, off by default).
Every container passes `--lodt-check` and an independent re-typed rule checker (VT 3.4 rules 1-22,
all 12,276 CRCs); three corrupted copies were each refused by name by both. The index matches the
containers on 24 fields per level. One red: the maskRules sum invariant (100 vs distinctLtex 101),
a census defect, see MISTAKES. Pictures + R2 numbers in scratchpad/vtbake1_20260923
(images/, r2numbers.md). Texel size: 0.457 / 0.914 / 1.829 / 3.658 / 7.315 m. Scratch tree only;
nothing deployed, nothing committed.

## WW_CHANGES (none)

No source changed. If a line is wanted: "Measured: the first whole-worldspace .lodt pyramid
(Commonwealth, 5 levels, 4.02 GB, 569 s) validates clean on every container rule; tools
scratchpad/vtbake1_20260923/vtread.py (mmap reader) + vtcheck.py (independent rule and index checker)."

## MISTAKES candidates (root MISTAKES.md, newest at top)

1. **The VT mask census counts one land texture it never classifies.** On the whole Commonwealth
   the `.lodm` says maskPbrm 0 + maskLegacyInverted 99 + maskNoneDefault 1 = 100 but
   maskDistinctLtex = 101; the VT s4 invariant (the three add up to the distinct count) is red. It was
   green on the Sanctuary region (14 = 14), so a region gate never saw it. Candidate mechanism, from
   reading the code only (not yet proven by a count): `LodgenVtMaskCache::resolve` in src/lodgen.cpp
   (~lines 11138-11171) stores form 0 (a quadrant with no texture) in `byForm` without bumping any
   rule counter, and distinctLtex is `byForm.size()` (~12571, ~12651). Refuter: print whether form 0
   is in `byForm` on the Commonwealth bake; if it is not, the mechanism is wrong. Containers are not
   affected (every tile's bytes validate). Lesson: run the index invariants on a WHOLE-worldspace bake,
   not only a region.
2. **Doc drift: VT s4 still gives the index path as `Data\Terrain\<EDID>.VT.lodm`.** The writer
   puts it at `FO4CSLOD\<ws>\<ws>.VT.lodm` since LAYOUT1. A reader following the doc finds nothing.
   Fix the doc line in docs/LODGEN_TERRAIN_VT.md s4.

## Observations for R2 / the owner (not defects)

* The playable land fills only the middle of the 192 x 192-cell worldspace; the rest bakes as flat
  grey tiles. void_census.txt: at dim 2, 4,561 of 9,216 tiles (49.5%, 1.48 GB) have zero height
  range and uniform colour; at dim 8, 258 of 576 (44.8%). Land spans cells about -78..77 each way.
  Candidate saving: mark them absent or shared, rule-permitting.
* Sanctuary at dim 2 vs vanilla's chunk sheet: orientation agrees (normal east channel r 0.66,
  0.16 flipped); colour averages agree (ours 91/79/66 vs 88/80/69) but the look differs -- ours is a
  per-texel blend of land textures showing their tiling, vanilla's is a smooth painted sheet
  (luminance r 0.01). Vanilla's normal carries fine erosion detail ours does not.
* 99 "pbrm not found in archives" log lines are the PBRM discovery miss on vanilla (ships no .pbrm);
  harmless but noisy -- one summary line would do.
