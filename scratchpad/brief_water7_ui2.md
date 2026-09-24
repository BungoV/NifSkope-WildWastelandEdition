# LANE WATER7/UI2 -- the water tool becomes a tab of the LOD Generation workspace; every top bar one compact height

Header: tree E:\Projects\NifskopeWildWastelandEdition, main, NO commits, ~200
uncommitted paths. Read CONSTITUTION.md, HANDOFF.md top block (the 2026-09-10
UI INTAKE + UI RULING paragraphs, BUILD9's alignment block, BUILD10's block),
scratchpad/build10_20260910/HANDOFF_BLOCK.md (the five reds), WW_CHANGES.md
entries of WATER5/WATER6/BUILD9. Skills via the Skill tool BEFORE any route:
`nifskope-ww-panel-style`, `ww-anchored-hookup`, `ww-test-harness-add`,
`nifskope-ww-build-verify`, `nifskope-ww-render-shot`, `nifskope-ww-resume-pending`.
Model Opus 5. Files you own: src/watermarkpanel.{h,cpp}, src/waterwindow.{h,cpp},
src/watercurves.{h,cpp}, src/watermark.{h,cpp}, src/wwskin.{h,cpp} (if the .cpp
exists; where the skin functions live in src/nifskope_ui.cpp the wwBarRowHeight /
wwAlignBarRow / wwSegmentedTabBarQss edits go through a REFUSING anchored
script, NOT a direct edit), res/style.qss, tests/spells/water_*.sh,
tests/spells/lodl_*.sh, the water test .cpp files, new files. src/nifskope_ui.cpp,
src/nifskope.cpp, src/nifskope.h, NifSkope.pro: ONLY through
scratchpad/water7_20260910/hookup.py (ww-anchored-hookup: --check, exact-once
anchors, CR/LF byte assert). MISTAKES.md: append your entries yourself
(append-only, LF). WW_CHANGES.md: do NOT edit; write your entry text to
scratchpad/water7_20260910/WW_CHANGES_ENTRY.md (the director splices).

## bungo's words, verbatim, in order
1. (Workspaces menu screenshot) "Two issues with water window and water marking
   appearing here" -> two entries for one tool, and a pop-up window is not a
   workspace.
2. "They should be in the LOD gen workspace".
3. (screenshot of the Header | Blocks | Files strip) "You'd access them like this"
   -> in the LOD Generation workspace the water tool is a TAB in that left-dock
   segmented tab strip: Header | Blocks | Files | Water. Same strip, same bar
   height. The tab holds the marking rows (body select, per-body rows, curve
   tools, Solve, Save/Load curves, Export/Import PNG) and the button that opens
   the full-screen flow window (src/waterwindow.*, unchanged in behaviour).
4. (screenshot of the aligned Header | Blocks | Files + Object Mode row)
   "compact these vertically like this, the top bar and the buttons" -> that
   row's height is THE bar height for the whole top of the window: the menu row
   (File View Spells Options Help), the Workspaces / LOD / Animation / Collision
   row, and every button in them take the same compact height and vertical
   padding THROUGH THE SHARED SKIN (wwBarRowHeight / wwAlignBarRow), no
   per-widget heights.

## The work
A. Water tab. Find how the Files tab (src/filestab.*, lane FILESTAB, BUILD9)
   joined the left-dock strip; add "Water" the same way, shown when the LOD
   Generation workspace is active (say in the report how the strip's tabs are
   scoped per workspace today, file:line, and follow that). Move the marking
   panel's rows into the tab (panel-style: flat Name|Value rows, skin palette
   only). Remove 'Water window' and 'Water Marking' from the Workspaces menu and
   retire the old dock (its harness gates move to the tab; the count may not
   drop below WATER5's/WATER6's floors). The window button opens the existing
   WaterWindow.
B. Compact bars. One number through the skin: the aligned strip's height (the
   35 px row of BUILD9, re-measured) becomes the height of the menu bar, the
   workspace row and their tool buttons, with the same vertical padding, by
   changing wwAlignBarRow / the QSS the skin emits, never a per-widget
   setFixedHeight. Gate: geometry read-back (a WW_*_TEST harness, or extend the
   alignment spell BUILD9 left) of the menu row, the workspace row and the dock
   strip: heights equal within 1 px, top edges consistent, the search row's
   content top unchanged or stated; before/after grabs through the render hook
   (never a desktop capture), opened and looked at.
C. BUILD10's five reds (HANDOFF_BLOCK.md "The five reds"): fix 2 (water_flow.sh
   grep takes the informational line), 3 (floor 18 vs two registered reds ->
   floor 17, say so in the spell), 4 (water_mark.sh pins WW_WATER_MARK_BODY=3
   or the dye gates report n/a by name), 5 (dye pin weight written; first named
   body's name offset 0 -> spelled correctly, plus a gate that writes a named
   first body and reads the name back), and 1 (replace X2b's radial cosine with
   the net-flux ring instrument named in lane_water6_report.md s6; register the
   floor before coding).
D. Documents: WW_CHANGES_ENTRY.md (measured numbers only), the panel-style
   skill's finished-work review, a docs/ paragraph where a contract changed
   (none expected: the .lodl and the json are untouched -- if you must touch
   either, stop and say so in the report).

## Build rule
Write ALL code first, syntax-check with sx_WATER7.sh (copy the pattern from
sx_HKXEDIT2.sh; the real Makefile.Release flags; with and without your
compile-time switch). Then check ONCE: scratchpad/build11_20260910/DONE exists
AND `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` prints rc=1. If
either fails: report, END BUILD PENDING with scratchpad/water7_20260910/PENDING.md
per nifskope-ww-resume-pending. Never poll. If both hold: create
scratchpad/water7_20260910/BUILDING, apply hookup.py, qmake (if the .pro
changed) then make through nifskope-ww-build-verify, delete stale .o for any
file whose defines changed (MISTAKES BUILD9 DEFINES trap), run the gates,
create DONE. One NifSkope instance ever, second monitor, opacity 0, absolute
paths, bungo's own window renamed aside never killed; report "restart".

## Report
scratchpad/lane_water7_report.md, incremental: 0. Pre-registered gates (before
code). 1. How the strip scopes tabs (file:line) and what was built. 2. The bar
numbers before/after. 3. The five reds, each: cause, change, number. 4. Gates
run / skipped and why, exe vs source mtimes. 5. Mistakes (also MISTAKES.md).
6. Finished-work skill review. 7. HANDOFF text (the director splices). Final
message under 25 lines, BUILD PENDING first if so, picture paths next.
