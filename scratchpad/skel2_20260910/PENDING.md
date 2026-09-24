# Lane SKEL2 -- RESUME (insurance copy, written mid-lane 2026-09-11 ~08:0x)

**SUPERSEDED 2026-09-11 08:07 -- THE LANE FINISHED.** `DONE` is in,
`BUILDING` is gone, every step below was run and every document was written.
Read `scratchpad/lane_skel2_report.md` and
`scratchpad/skel2_20260910/HANDOFF_BLOCK.md` instead. This page is kept only
because it records the baselines as they were measured, before any code.

If this lane had died, everything below is on disk and nothing needs
re-deriving.

## What is built and gated

* `release/NifSkope.exe` **07:58:59, 20,927,488 B** -- ONE build (07:55:14,
  20,926,976 B) plus ONE counted relink (07:58:59) for the armature-membership
  fix. Rollback rung `release/NifSkope.before_skel2.exe` = the 07:06:04 bytes,
  written once.
* Marker `scratchpad/skel2_20260910/BUILDING` is UP. Replace it with `DONE`
  once the gates are read.
* `tasklist` printed `rc=1` before the build and before the relink; bungo had no
  window open at any point and nothing of his was touched.

## The code, all applied

| file | how |
|---|---|
| `src/glview.h` | Edit tool (LF, CR 0) |
| `src/glview.cpp` | `scratchpad/skel2_20260910/patch_glview.py --apply` (CRLF binary splice; dCR = dLF = +467, then +14 for the fix) |
| `src/skeletontools.h` / `.cpp` | Edit tool (LF, CR 0) |
| `src/skeloverlaytest.cpp` | `scratchpad/skel2_20260910/patch_test.py --apply` (LF) |
| `src/nifskope_ui.cpp` | `scratchpad/skel2_20260910/hookup.py --apply` -- 2 anchors, CR 0 -> 0, marker x2 |
| `tests/spells/skeleton_overlay.sh` | rewritten |
| `tests/spells/skeleton_overlay_coverage.py`, `skeleton_overlay_dots.py` | NEW |

`g++ -fsyntax-only` with `Makefile.Release`'s own flags: RC=0 on `glview.cpp`,
`skeletontools.cpp`, `skeloverlaytest.cpp`, `nifskope_ui.cpp`.

## What is left, in order

1. `bash tests/spells/skeleton_overlay.sh` -- the whole chain (S1 source gate,
   the in-app gates, the legacy-column floor run, eleven renders, the two mask
   gates, the dots, the coverage gate). Run 1 on the 07:55:14 exe:
   **46 checks, 0 failures** in-app, one red -- S2 coverage `only-new 782`,
   which the relink fixes. Log
   `scratchpad/skel2_20260910/logs/skeleton_overlay_run1.txt`.
2. `bash scratchpad/skel2_20260910/neighbours.sh` -- the two pose harnesses
   (both RED ON THE RUNG with one named failure each; the gate is the SAME
   failure, not green) and the neighbour spells animws / hkxanim_ui / water_ui /
   ui_align / loaded_nifs / top_bar / files_tab.
3. `python scratchpad/skel2_20260910/compose.py` -- the composite pictures.
4. Documents: `WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md` (both owed),
   `MISTAKES_ENTRIES.md` (WRITTEN, 3 entries), `scratchpad/lane_skel2_report.md`
   sections 4-8 (0-3 written).
5. Replace `BUILDING` with `DONE`.

## The baselines, measured on the rung BEFORE any code

| harness | rung |
|---|---|
| `WW_SKELOVERLAY_TEST` | 27 checks, 2 failures (five identical runs) |
| `WW_POSEDRAW_TEST` | RED: *clicking a bone did not make it the active object* |
| `WW_POSEEXTRAS_TEST` | RED: *weight overlay found no influenced vertices* |
| `WW_SKELETON_TEST` | PASS, All 130 / Bones 93 / Deforming 93 / Unused 0 |
| (c) over five runs | 16 / 13 / 14 / 9 / 1 |
| (e) over five runs | 12 / 22 / 19 / 15 / 21 |
| (j) on the rung's frame-46 pair | 5 checks, 1 failure, 56 stray px |

## The answer bungo is owed

The two grey dots above the back at frame 46 are **`Camera` (block 148)** and
**`CamTarget` (block 159)**, both marker-only nodes, named from the overlay's own
per-frame dump and matched 0.9 and 0.8 px from the stray clusters' centres.
Under the Bones chip they are not listed and the frame has **zero** stray pixels.
