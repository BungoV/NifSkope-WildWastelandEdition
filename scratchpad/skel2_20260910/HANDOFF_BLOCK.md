# HANDOFF block -- lane SKEL2 (verbatim for the director to splice)

**SKEL2 LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 07:58:59**, **20,927,488 bytes** (UI6's was 07:06:04, 20,867,584).
ONE build (07:55:14) plus ONE counted relink (07:58:59, the armature-membership
fix). Markers: `scratchpad/skel2_20260910/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_skel2.exe` = the 07:06:04 bytes exactly, written once.
Report `scratchpad/lane_skel2_report.md`; entry text
`scratchpad/skel2_20260910/WW_CHANGES_ENTRY.md`; **three MISTAKES entries NOT
appended by the lane** -- `scratchpad/skel2_20260910/MISTAKES_ENTRIES.md`.

## What bungo gets

* *"shouldn't we improve both views (that will now be shared)?"* -- Pose Mode and
  the Overlays armature are **one drawing routine** now, `GLView::drawArmature()`,
  fed by each caller with a bone list and per-bone state. Pose Mode keeps
  everything it had. A source gate counts the bone-drawing calls inside both old
  functions (0) so they cannot drift apart again, and ONE palette function
  answers for the viewport and the dock's rows.
* *"what about the bone shape? Shouldn't it be something like in Blender?"* --
  the bone is Blender's **solid octahedron**: eight flat-lit facets, the ring at
  one tenth of the length, a ball at the head and a smaller one at the tail.
  **Overlays > Bone Display** has Octahedral, Stick and Wire, plus X-ray and
  Names. B-Bone and Envelope are refused, with the reason (a NIF node carries
  none of the five numbers they need).
* *"Just keep the color of the bones blue"* -- **#4772b3**, the palette's
  `toggle`, which is Blender's own option blue and the only blue in `skinVars[]`.
  Unselected is that blue dimmed to 0.78, selected is the blue, active is lighter
  still, hovered is a rim.
* *"for that bone view toggle, shouldn't it mirror the skeleton manager view?"* --
  it does, through ONE shared function that decides which rows are listed. Under
  all four chips and under a search the overlay draws exactly the dock's rows,
  checked BY NAME. Click a bone and its row goes current and scrolls into view;
  click a row and the bone lights; double-click a row and the viewport frames
  that bone. Names are on the hovered and selected bones only.
* **The Skeleton Manager's Bone column never elides to nothing.** At a forced
  400 px, 95 rows sit at depth 6 or deeper and **0** of them are empty or
  elided; the tightest (`RArm_Finger13`, depth 14) has 59 px to spare. Under the
  shipped law that same row had **-71 px** and 50 rows were elided.
* **The two grey dots are named**: `Camera` (block 148) and `CamTarget`
  (block 159).

Pictures, `scratchpad/skel2_20260910/images/`: **`cmp_bones_before_after.png`**,
**`bones_stick_vs_octa.png`**, **`cmp_manager_names.png`**, `bones_xray.png`,
`bones_bones_chip.png`, `bones_all_chip.png`, `manager_after.png`.

## Gates (all on the 07:58:59 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `skeleton_overlay.sh` in-app | **46 checks, 0 failures, PASS** | 27 / **2** on the rung |
| `skeleton_overlay.sh` whole spell | **rc=0** | rc=1 |
| (j) under the Bones chip | 5 / 0, **0 stray pixels** | newly registered |
| S2 `Wire` coverage vs the rung | **only-rung 2, only-new 0**, bar 64 | -- |
| S2 control, `Octahedral` | FAILS the same test, as it must | -- |
| S6 the dots | 2 clusters, 0 unnamed | -- |
| `WW_SKELETON_TEST` | PASS, 130 / 93 / 93 / 0 | same |
| `WW_POSEDRAW_TEST` | RED, the SAME one failure | RED, same failure |
| `WW_POSEEXTRAS_TEST` | RED, the SAME one failure | RED, same failure |
| `animws.sh` | 72 / 0, 1 skip, PASS | 72 / 0, 1 skip |
| `water_ui.sh` | 86 / 0, PASS | 86 / 0 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `hkxanim_ui.sh` | 48 / **1**, the same | 48 / 1 |
| `loaded_nifs.sh` | 166 / **3**, the same three | 166 / 3 |
| `top_bar.sh` | 43 / **5**, the same five | 43 / 5 |
| `files_tab.sh` | 28 / **2**, the same two | 28 / 2 |

Consistency: **122 of 122** changed paths older than the exe; **38 of 38**
objects of `glview.h` and **9 of 9** of `skeletontools.h` newer than their
header; `res/style.qss` and `release/style.qss` byte-identical.

The one count that moved and its NAME: `water_ui.sh` first read 82 / 0 because
four `(shot)` checks arm only when the spell is given `SHOT=` / `TABSHOT=` /
`STRIPSHOT=` / `LODSHOT=`. Re-run with them: 86 / 0, PASS.

## Four things for the director

1. **The Bones chip draws long reach-through bodies.** An FO4 body weights the
   `*_skin` helpers, so under that chip the dock lists 93 helpers and none of
   the chain nodes between them, and each listed bone reaches to its nearest
   LISTED ancestor -- across the torso in places. It is the mirror taken
   literally and it passes every gate (0 pixels off the character); it does not
   look like a skeleton. bungo's call: leave it, draw each listed bone as its
   own short stub under a chip (closer to Blender), or let a chip keep the
   connecting nodes as muted context (which breaks the "lists exactly the dock's
   rows" gate as written). Picture: `bones_bones_chip.png`.
2. **17 of the 172 drawn bodies are muted grey, not blue** -- `Pelvis`, both
   `*_UpperArm`, `*_ForeArm1..3`, `*_UpperTwist1/2`, `*_Thigh`, `*_Calf`. The
   Skeleton Manager classes them "not a bone" because no skin lists them, which
   is true of this file. 155 bodies and 93 of the 130 joint balls are blue. One
   line makes the whole armature blue, at the cost of the dock's colours no
   longer agreeing with its own Bones filter.
3. **Both Pose harnesses were RED ON THE RUNG** and still are, each with one
   named failure, measured before this lane wrote a line.
   `WW_POSEDRAW_TEST`: *clicking a bone did not make it the active object* -- a
   real application defect (a click at bone 0's screen position selects block 5).
   `WW_POSEEXTRAS_TEST`: *weight overlay found no influenced vertices* -- a
   harness defect (it inspects bone 0, the root, which drives no vertex). Two
   small lanes, neither this one's.
4. **NOBODY CLICKS THE NEW MENU ROWS.** Every gate drives the armature through
   the API or through the render hook's environment switches. Overlays > Bone
   Display is proved only by its strings being in the binary (`Bone Display` x1,
   `GLView/ArmatureDisplay` x1, `BoneDisplay%1` x1, read back out of the exe)
   and by the code compiling. A harness that opens the dropdown and clicks the
   three rows is owed.
5. **`WW_RENDER_SIZE`'s width floor moved again**: a request of 1000x1000 comes
   back **1524x941** on this build (it was 1293 on 17:08:39 and 1437 on
   15:52:46). Height is still `requested - 59`. The skill's table is amended
   with the exe each row was measured on.

## Skills

**NEW:** `<repo>/.claude/skills/ww-way-back-by-coverage/SKILL.md` -- proving an
exact way back when the look was ALSO ordered to change, and the shape of
merging two drawing routines into one.
**AMENDED, both need mirroring to `E:\Projects\Claude\.claude\skills`:**
`ww-test-harness-add` (new 5c, the measured framebuffer tolerance and its own
floor; new 5d, force the states the lane itself just made persistent) and
`nifskope-ww-render-shot` (the `WW_RENDER_SIZE` table gains the exe per row and
the 1524 measurement, plus the rule that a screen COORDINATE is never carried
between builds).

## Restart

**YES.** Whatever bungo opens next must be launched after **07:58:59**. No
NifSkope was running at any point in this lane (`rc=1` at every check, including
immediately before both links), so nothing of his was touched and no exe had to
be renamed aside.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane:
`src/glview.h`, `src/glview.cpp` (binary CRLF splice,
`scratchpad/skel2_20260910/patch_glview.py`), `src/skeletontools.h`,
`src/skeletontools.cpp`, `src/skeloverlaytest.cpp` (`patch_test.py`),
`src/nifskope_ui.cpp` (`hookup.py`, 2 anchors, CR 0 -> 0, marker x2),
`tests/spells/skeleton_overlay.sh`; NEW
`tests/spells/skeleton_overlay_coverage.py`,
`tests/spells/skeleton_overlay_dots.py`. Game down at every check; no harness
instance was left running.
