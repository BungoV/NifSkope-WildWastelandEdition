# Lane UI5 -- File / View / Spells / Options / Help vertically centred in the 35-px menu row

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree. Nothing is committed (CONSTITUTION 8).
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the UI RULING paragraphs of 2026-09-10 20:1x, 20:2x, 20:3x, 21:0x, 21:1x); `scratchpad/ui3_20260910/HANDOFF_BLOCK.md` and `scratchpad/ui4_20260910/HANDOFF_BLOCK.md` (how the row, the buttons and the strip were sized through the shared skin: `wwAlignBarRow`, `wwBarRowButtonQss`, `wwBarRowBoxQss`, `UI/CompactTopBars`); `MISTAKES.md` tail (four UI4 entries: the unsubstituted sheet in a probe, the guard four minutes before the link, tabRect() including the margin, the "separated segments" misread); `scratchpad/ui5_20260910/probe_out.txt` and `probe.cpp` (a dead UI5 instance's probe: `QMenuBar::item { margin-top: N }` sweep, ink centre vs row centre 17.0 -- margin-top 6 -> ink offset -0.5, 8 -> +1.5; the marker sheet moved no rect, so the gate must read PIXELS); `scratchpad/ui5_20260910/measure_before.py` (reads the ink rows out of UI4's `strip_after.png`, which IS the shipped menu row).
- Skills you MUST invoke (repo tree `.claude/skills/`): `ww-qss-geometry-probe` (case 0 reproduces the SHIPPED number in the application's own substituted sheet before anything is believed), `ww-anchored-hookup` (every edit to a file another lane owns goes through a refusing script with `--check`), `ww-test-harness-add` (the new checks, their floors, the one floor proved to fire live), `nifskope-ww-build-verify` (the build chain; the exe under bungo's window renamed aside immediately before the link; `cmp res/style.qss release/style.qss`), `nifskope-ww-panel-style` for the skin helpers' names and the no-text rule.
- Build rules: you may build ONLY when (a) `scratchpad/water8_20260910/DONE` exists and no `scratchpad/*/BUILDING` exists, (b) `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` prints rc=1 immediately before the link (inside the chain, not at the top of the lane), (c) you have put your own `scratchpad/ui5_20260910/BUILDING` marker up first and remove it when the exe is gated (then write `DONE`). Until (a) holds you are CODE-ONLY: write the change, the hook-up script, the harness additions, syntax-check them (`sx_UI5.sh`, build-verify "When you CANNOT build", delete it after), and wait by polling for DONE (sleep 60 between polls, at most 40 polls; if DONE never comes, end BUILD PENDING with `scratchpad/ui5_20260910/PENDING.md` per `nifskope-ww-resume-pending`).

## bungo's words, verbatim
- 2026-09-10 20:2x: **"Also, please center file / view / spells / options / help buttons, top left"** -- his screenshot shows the menu-bar items sitting at the TOP of the 35-px row; they are to be vertically centred in it.
- 20:1x: the row height STAYS as it is. Nothing else moves.
- 21:0x (about the tab strip, lane UI6's, NOT yours): "Why are they separated?" -- so do not touch the strip.

## The work
1. Case 0 (probe, no build slot): reproduce the SHIPPED offset in a 40-line Qt program against `res/style.qss` with the `${...}` skin tokens substituted the way `src/nifskope_ui.cpp` does it (~line 30199-30211; read it, do not assume), a 35-px `QMenuBar` with the five titles, and print: the row centre, each item's painted-box centre and ink centre. The number must match `measure_before.py`'s reading of `scratchpad/ui4_20260910/images/strip_after.png` (run it; quote both). Reuse `scratchpad/ui5_20260910/probe.cpp` if it already does this -- check that it substituted the tokens (MISTAKES entry 1 of UI4).
2. Sweep the candidate mechanisms in the probe and pick the one whose ink centre lands within 0.5 px of 17.0 AND does not change the menu bar's height (`wwAlignBarRow` takes the TALLEST bar: if the menu bar's natural height rises above 35 the whole row grows -- the refuter, print the bar's `sizeHint().height()` in every case). Candidates: `QMenuBar::item { margin-top / padding-top }` (probe says ~7 px), a `QMenuBar` content margin, or centring through `wwBarRowButtonQss`-style arithmetic derived from the row height rather than a literal. Prefer the skin-derived form (the row height is `wwBarRowHeight()`, and UI3 states button air from the row once; the menu items should be stated the same way, not as a magic 7) -- if the literal is the only thing that works, say why.
3. Implement through the shared skin: the QSS rule goes where `wwBarRowButtonQss()` / `wwBarRowBoxQss()` live (find them; `grep -n wwBarRow src/*.cpp src/*.h`), applied to the menu bar inside `wwAlignBarRow` (which already includes the menu bar since WATER7). `UI/CompactTopBars=false` must remain the exact way back (off value = pre-UI3 behaviour, pinned by the existing check). The file that holds those helpers is owned by NO live lane at the time of writing (UI4 DONE, WATER8 gate-only, UI6 not launched) -- confirm by `ls scratchpad/*/BUILDING` and the HANDOFF block; if another lane holds it, hook-up script only.
4. Gate additions in `src/wateruitest.cpp` group M (the file is free once WATER8's DONE exists; until then write them in a new TU `src/wateruitest_menu.cpp` called from group S's runner via the refusing hook-up, following what WATER8 did with `wateruitest_lod.cpp`): M1 menu bar height == `wwBarRowHeight()` (35) unchanged; M2 for each of the 5 items, ink centre (from the in-app grab, brighter-than-background rows, same method as `measure_before.py`) within 1 px of the row centre; M3 the row's other bars (tab strip, tMode, tRender tops/heights from `ui_align.sh`'s dump) unmoved from UI4's numbers; M4 with `UI/CompactTopBars=false` the items sit where they did before (top-aligned, the old offset) -- the way back is exact; M5 a floor that fires: run M2 against the OLD sheet's offset (the pre-change number from case 0) and show it red in the same log. Count floor: water_ui.sh currently 48 (UI4) plus whatever WATER8 lands; the spell refuses below the larger of those.
5. Build (when allowed): `qmake` before `make` if `NifSkope.pro` changed (a new TU), objects of every TU including a header you changed deleted first if that header changed, the rename-aside immediately before the link (`tools/ww_build.sh` or WATER8's `build.sh` pattern), `cmp res/style.qss release/style.qss`, exe-newer sweep over the whole `git status` working set, then the chain: `water_ui.sh` (`env SHOT=scratchpad/ui5_20260910/images/toprow_after.png`), `ui_align.sh`, `top_bar.sh`, `files_tab.sh`, `animws.sh`. Baselines: see the HANDOFF block (UI4: 48/0, 11/0, 43/5, 28/2, 57/0) plus WATER8's landed counts in its DONE line.
6. Pictures: BEFORE = `scratchpad/ui4_20260910/images/strip_after.png` (the shipped row, name it as the before); AFTER = the in-app grab from step 5, same crop; plus a 4x zoom pair of the File..Help region (`cmp_menu_zoom.png`) made by a script under your scratchpad. Describe each in two sentences before citing.
7. Documents: `scratchpad/ui5_20260910/WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md` (style of UI4's), `MISTAKES_ENTRIES.md`, and the report. You do NOT edit `WW_CHANGES.md`, `HANDOFF.md`, `MISTAKES.md`.

## Gates (for this lane)
- U1 case 0 reproduces the shipped ink offset within 1 px of `measure_before.py`'s number, in a sheet with tokens substituted; both numbers quoted.
- U2 after: all five ink centres within 1 px of the row centre; menu bar height 35; every other bar's top/height equal to UI4's dump.
- U3 `UI/CompactTopBars=false` reproduces the before offset (exact way back).
- U4 one floor shown red live in the same log (M5).
- U5 exe newer than every file in `git status --porcelain -- src res tools tests`; `cmp res/style.qss release/style.qss` silent.
- U6 no NifSkope left running by you (harness instances carry `--port`; a window without it is bungo's -- never touch it).

## Rules
- Code-only until WATER8's DONE exists and no BUILDING marker exists. One build.
- Touch nothing of the tab strip, the buttons, the toolbars (that is UI6 / already landed). Only the menu bar's items move.
- Files another lane could own go through the refusing hook-up script with `--check`, exact-once anchors carrying the file's real line ending, CR byte assert.
- If the only working mechanism changes the row height or any other bar: do not ship it; report the number (CONSTITUTION 7: refused-with-numbers is a deliverable).
- Never `git stash`, never commit. Plain language.

## Report
Path: `scratchpad/lane_ui5_report.md`, written incrementally:
- `## 0. Pre-registered gates` (before code: the numbers you predict for U1..U4)
- `## 1. Case 0` (shipped offset, both instruments)
- `## 2. The mechanism` (sweep table, the refuter's column = bar sizeHint height)
- `## 3. The change` (files, the skin helper, the hook-up script's `--check` output)
- `## 4. Build and gates` (mtime table, gate table with baselines and log paths, skipped harnesses with reasons)
- `## 5. Pictures` (described, then cited)
- `## 6. Owed / red` 
- `## 7. Mistakes`
- `## 8. Finished-work skill review` (loaded, missing, written/amended -- name any skill file you amended so the director mirrors it to the live tree)
