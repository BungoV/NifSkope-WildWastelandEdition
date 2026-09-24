- UI6 block, spliced 2026-09-11 (lane text verbatim):

**UI6 LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 07:06:04**, **20,867,584 bytes** (UI5's was 05:58:21,
20,855,296). Markers: `scratchpad/ui6_20260910/DONE` in, `BUILDING` gone.
Report `scratchpad/lane_ui6_report.md`; entry text
`scratchpad/ui6_20260910/WW_CHANGES_ENTRY.md`; **six MISTAKES entries NOT
appended by the lane** -- `scratchpad/ui6_20260910/MISTAKES_ENTRIES.md`.
**The rollback rung is real this time**: `release/NifSkope.before_ui6.exe` is
the 05:58:21 bytes exactly (20,855,296), written once and guarded against a
second write.

## What bungo gets

* *"Why are they separated?"* -- **both** segmented strips are one joined box
  again. Header | Blocks | Files on the left and LOD | Water in the right-hand
  panel: segments touching, one shared seam, square inner corners, the two
  outer corners rounded, and UI4's 4 px still between the strip and the row's
  top and bottom, the window's left edge and the toolbar. Segments still 27 px
  in a 35 px row; nothing else moved. `UI/SegmentedStripAir = 0` still emits
  the 18:25:20 sheet byte for byte.
* *"That's fine, as long as the dropdown arrows do not intersect with the text
  / icons"* -- every button in the top row that has a menu now keeps a column
  for its arrow. Thirteen of them, worst **2 px clear** (it was **-3** on the
  narrow icon buttons, i.e. the arrow drawn inside the glyph). Nothing vertical
  changed; the menu buttons are 4 px wider.
* *"old and outdated"*, *"Do you see it?"*, *"we have a new standard for those
  sliders"*, *"look at all this text clutter"* -- **the Animation Manager dock
  is gone**, and with it the interim Havok clip strip inside it. The Animation
  dock takes its seat in **Workspaces > Animation**, which it never had. All
  eleven of its number fields are scrub fields and **none of them draws a
  native stepper any more** (two still did). The bone-binding report is
  **"78 of 95 bones"**, 14 characters, with the whole 456-character sentence in
  its tooltip.

Pictures, `scratchpad/ui6_20260910/images/`: **`cmp_strip_left.png`**,
**`cmp_strip_right.png`**, **`cmp_arrows.png`** (before/after, in-application
grabs, same spell, same arguments, each labelled with its exe) and
`animdock_after.png`.

## Gates (all on the 07:06:04 exe, sequential, one instance, `.gatelock`)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **86 / 0 / 0, PASS** (floor 72) | 75 / 0 asked the same way (76 with a second LOD picture), floor 62 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `top_bar.sh` | 43 / **5** -- the same five | 43 / 5 |
| `files_tab.sh` | 28 / **2** -- the same two | 28 / 2 |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 57 / 0, 1 skip |
| `hkxanim_ui.sh` | 48 / **1** -- the same unfirable wheel floor | 48 / 1 |
| `loaded_nifs.sh` | 166 / **3** | 166 / 0 quoted, but the RUNG reads 166 / **1** |

Consistency: 118 of 118 changed paths older than the exe; every object of all
six changed headers newer than its header; `res/style.qss` and
`release/style.qss` byte-identical.

## Four things for the director

1. **`animws.sh` gate (j) had NEVER RUN.** The whole panel-style group --
   including the scrub-field count bungo ruled on -- lived inside the
   sequence-NIF branch, and the fixture (`10mmPistol.nif`) has no
   `NiControllerSequence`, so every 57 / 0 since HKXEDIT2 finished without
   asking a panel-style question. It is a lambda now and both branches call it;
   that is 9 of the 15 checks `animws.sh` gained. On its first execution it
   found a real defect: Speed and Frame were stamped as scrub fields and still
   drew Qt's up/down arrows (`wwMakeScrubField` only removes them when its
   `chrome` flag is on). Fixed in the same wave. **A fixture NIF that HAS a
   NiControllerSequence would light nine more checks and nobody has named one.**
2. **`loaded_nifs.sh` 166 / 3, and two of the three are this lane's, measured.**
   Thirteen menu buttons 4 px wider raise the viewport header's minimum width,
   and in the harness's window QMainWindow takes it out of the LEFT DOCK: `dock
   tab strip w 174 -> 164`, `viewport header w 833 -> 857`. Two checks probe the
   Loaded Files name column at a fixed `nameArea.left() + 60` and that point now
   lands on a glyph. **Not repaired** -- it is another lane's gate and the fix is
   to take the probe point from the column's own rect. The THIRD red ("the three
   editor modes are equal-width joined segments") is red on the rung too, twice,
   so it predates this lane and nobody has noticed since.
3. **What the retired dock could do and the new one cannot** -- section 5.1 of
   the report, sixteen rows. Nothing was ported; this is bungo's call. The
   largest by far is **editing a NIF's own animation keys**: the Animation dock
   is a Havok clip editor and a NIF sequence is read-only in it. Also gone: the
   graph/F-curve view and key inspector, per-interpolator lanes for
   non-transform controllers, channel copy/paste, CSV import/export, the lint
   scan, the lane filter, snap steps, normalise and follow-playhead.
4. **FOUR LINKS, not one**, and all four are stated in the report's 3.1. One
   carried application code; the other three were single harness translation
   units, each forced by a gate going red on its FIRST EXECUTION. Two harnesses
   (`WW_ANIMPLAY_TEST`, `WW_ROTKEY_TEST`) are retired in place and write a line
   saying so; their coverage is gone until somebody re-aims them.
   `TimelineWidget` is still compiled but never constructed -- deleting the
   class is a separate lane, because its file also owns the icon set the whole
   window draws with.

## Restart

**YES.** Whatever bungo opens next must be launched after **07:06:04**. No
NifSkope was running at any point in this lane (`rc=1` at every check,
including immediately before each of the four links), so nothing of his was
touched and no exe had to be renamed aside.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane:
`src/nifskope_ui.cpp`, `src/nifskope.h`, `src/wateruitest.cpp`,
`tests/spells/water_ui.sh` (all four through the two refusing scripts
`scratchpad/ui6_20260910/hookup.py` and `hookup_gates.py`, 24 + 13 edits, every
anchor once, CR 0 -> 0), and directly `src/wateruitest_lod.cpp`,
`src/hkxplayback.{h,cpp}`, `src/hkxanimui.{h,cpp}`,
`src/animworkspace.{h,cpp}`, `src/animworkspacetest.cpp`,
`src/hkxanimuitest.cpp`, `src/ui/widgets/timeline.{cpp,h}`,
`src/spells/animationsetup.cpp`, `src/ui/widgets/physicspanel.cpp`,
`tests/spells/hkxanim_ui.sh`. Game down at every check.

**Skills: one amended and one written, in BOTH trees, byte-identical.**
`ww-qss-geometry-probe` 14,244 -> 17,337 B (md5 d4c15a63) gains sections 3c
(a widget that paints no background grabs onto WHITE, and an icon copied while
its painter is still attached) and 3d (isolating a subcontrol by two renders at
one pinned geometry; pin the whole row, not one widget at a time). **NEW:
`ww-retire-a-surface`** 6,918 B (md5 cfff4228) -- the inventory that comes
first, the seven places a dock is still reachable from, the workspace INDEX
stored in QSettings that shifts when a list loses an element, the harnesses'
three honest futures, and the control run of the kept rung.
