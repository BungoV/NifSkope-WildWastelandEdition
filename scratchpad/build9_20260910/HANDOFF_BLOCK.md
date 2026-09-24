<!-- Lane BUILD9, 2026-09-10. TEXT ONLY for the HANDOFF.md top block; the
     director splices it (CONSTITUTION 8). -->

**BUILD9 DONE 16:0x -- the three pending lanes are built, gated and ledgered,
and the bars share one row.** Deployed exe `release/NifSkope.exe` **15:52:46**,
`release/style.qss` equal to `res/style.qss`, exe newer than every changed
source. **His open window needs a restart.** Nothing committed (193 uncommitted
paths).

* **FILESTAB** applied (71 anchors) and gated: `files_tab.sh` **29 checks,
  2 failures**. Green: 0 "NIF" strings on the page with the seeded-offender
  floor, the tree lists `.nif 1 / .bto 1 / .btr 1 / .hkx 14939` over FORCED
  roots, opening a clip from a real row gives 78 / 17 / 4 and becomes the
  playing sequence, and an `.hkx` with nothing open refuses in words. The two
  reds are measured and deliberately NOT amended: the "every tool button
  explains itself" count includes Qt's own `QLineEditIconButton` clear buttons
  (2 of 6), and "unload restores the bind pose" reports `PipboyBone`, which the
  fixture drives with its own `NiTransformController` while the gate compares
  across two different scene times. **Question for bungo: unloading a clip
  leaves the scene at the clip's time.** Pictures
  `scratchpad/filestab_20260910/dock_before.png` / `dock_after.png`.
* **HKX3** applied (9 anchors, `WW_HKXANIM_UI`) and gated: `hkxanim_ui.sh`
  **48 checks, 1 failure, 0 skips**; gates a-f all green, including 0 of 139
  nodes differing after unload and the drop accepted with the same 93 @ 60. The
  one red is gate (g)'s wheel floor, which CANNOT fire: a WW harness window is
  never activated, so `hasFocus()` is false and the guard blocks the wheel
  exactly as specified (the harness now prints
  `hasFocus no, focusWidget <none>, window active no`). Picture
  `scratchpad/hkx3_20260910/dock_clip_midclip.png`. `hkxanim_play.sh` still
  27 / 0.
* **SKELOVERLAY** applied (5 anchors) and gated: `skeleton_overlay.sh`
  **17 checks, 0 failures, PASS** -- the overlay's census equals the Skeleton
  Manager's (130 / 93 / 93 / 0), 0 changed pixels outside its own reported mask,
  byte-identical restore when toggled off, and every joint on the animated node
  at frame 46. Pictures `off.png` / `on.png` / `on_frame46.png` plus
  `gates/gate_mask.png`.
  **OWED:** `WW_POSEDRAW_TEST` FAILS at "clicking a bone did not make it the
  active object" on BOTH fixtures here; the pose-size/tail factoring is
  arithmetically identical by diff, and this harness was last recorded green on
  a FACIAL rig of 70 bones that neither fixture is. One run on that rig settles
  it.
* **ALIGNMENT (UIALIGN) LANDED.** Measured before: main toolbars 35 px, viewport
  toolbar 33, dock tab strip 26, search row starting 3 px above the viewport's
  content. Now one row: tab strip and viewport toolbar both top 35 height 35,
  search row top 70 = viewport content top 70. Stated once in the shared skin
  helpers (`wwAlignBarRow`, `wwBarRowHeight`, `wwStartContentBelowBar`,
  `wwSegmentedTabBarQss( rowHeight )`), never per-widget; the row is the tallest
  natural height so no toolbar is squeezed into its chevron. New gate
  `tests/spells/ui_align.sh` / `src/uialigntest.cpp`: **11 checks, 0 failures**,
  with the floor that a 4 px difference goes red. Seam pictures
  `scratchpad/build9_20260910/seam_before.png` / `seam_after.png`.
* **Neighbours after everything:** `loaded_nifs.sh` 166 / 3 (the same three as
  before this session; six expectations that quoted the renamed labels were
  repaired, literals only), `top_bar.sh` 43 / 5 -- all five pre-existing, it
  expects a View menu listing six docks that were merged into "Left Editor"
  before this session.
* **Documents:** WW_CHANGES.md gained one entry carrying all three lanes plus
  the alignment (CR 19,020 unchanged); MISTAKES.md gained five entries (my
  DEFINES-staleness mistake, the four never-executed gates, the two wrong resume
  figures, and FILESTAB's and SKELOVERLAY's own sections, which had never been
  spliced); each lane report gained a `## Build (BUILD9)` section. The owed skill
  `ww-test-harness-add` is WRITTEN in both trees, and `nifskope-ww-build-verify`,
  `nifskope-ww-resume-pending` and `nifskope-ww-panel-style` were amended in both
  (panel-style did not exist in the repo tree and now does).
* **Not mine, still in the tree:** `src/hkxmodel.{h,cpp}` (untracked, not in
  `NifSkope.pro`, written 15:54-15:56 -- lane HKXEDIT1) and `sx_tmp.sh` at the
  root from 02:30, another lane's throwaway. Neither was touched.
