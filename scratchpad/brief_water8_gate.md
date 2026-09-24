# Lane WATER8-GATE -- finish lane WATER8: gate the built exe, pictures, documents, DONE

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree (no worktree; you are the only lane that runs the exe). Nothing is committed (CONSTITUTION 8).
- Read first, in this order: `CONSTITUTION.md`; `HANDOFF.md` top block ("SESSION HANDOFF, written 2026-09-10 17:18"); `scratchpad/lane_water8_report.md` (sections 0..0.3 = the pre-registered gates); `scratchpad/water8_20260910/build.sh`, `chain.log`; `MISTAKES.md` tail (the four 2026-09-10 UI entries).
- Skills you MUST invoke (repo tree `.claude/skills/`): `nifskope-ww-resume-pending` (this is a resume: sections 4, 5, 6, 7, 9, 11), `nifskope-ww-build-verify` (the exe-newer sweep, "whose NifSkope is that", the harness rules), `ww-test-harness-add` (if a gate is itself defective), `nifskope-ww-render-shot` only for the one-instance / second-monitor / invisible-headless rules.
- Build rules: **you do NOT build unless a gate is defective in a way that a gate-only fix cures** (section 6 of resume-pending: measure the cause and STOP; the director decides). Game check `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` must print rc=1 before every exe launch. One NifSkope instance ever; `WW_WINDOW_AT=1960,40`; never a desktop capture.

## What happened (facts, read them back before believing them)
- Lane WATER8 wrote the code (Water tool = second tab of a segmented strip in the LOD Generation panel on the RIGHT, left strip back to Header | Blocks | Files, old Water dock + Workspaces entries removed), applied its hook-up (`hookup.py --check` now reports E1/E2 REFUSED because their markers are already present x1 -- that is "applied", not "failed"), ran `build.sh`: `QMAKE-RC=0 BUILD-RC=0 CHAIN-RC=0`, `release/NifSkope.exe` 21:02:12, 20,830,208 B, `release/style.qss` 21:02:12.
- `find src res tests NifSkope.pro -newer release/NifSkope.exe` prints nothing: the exe is newer than every source.
- The lane then died (account meter hit 100). `scratchpad/water8_20260910/BUILDING` is still up, no `DONE`, no report section after 0.3, no `WW_CHANGES_ENTRY.md`, no `HANDOFF_BLOCK.md`, `images/` empty.
- bungo's words that rule this work, verbatim: **"What? I wanted it in that right panel though"**, earlier "They should be in the LOD gen workspace", "You'd access them like this" (over a screenshot of the Header | Blocks | Files strip). His later ruling on the strip (21:0x): **"Why are they separated?"** -> the SEGMENTS of a strip must touch; only the strip's outer box keeps 4 px of air. That ruling is lane UI6's to implement, NOT yours -- but your gate L8 (registered by WATER8 as "4 px ... of one another") measures the OLD reading. See gate note below.

## The work
1. Verify on disk before anything: exe mtime/size as above; `hookup.py --check` output; the WATER8 markers in `src/nifskope_ui.cpp`, `src/nifskope.h`, `NifSkope.pro` (`grep -c "lane WATER8"` per file -- counts are re-derived, not accepted); `src/wateruitest_lod.cpp` present and named in `NifSkope.pro`; `grep -c wateruitest_lod Makefile.Release` > 0; `GeneratedFiles/.obj/wateruitest_lod.o` newer than its source. Put every mtime in ONE table.
2. Header staleness (build-verify "A successful build is not a consistent one"): `src/nifskope.h` changed at 20:59:18; list every TU that includes it whose `.o` is OLDER than it. If any is stale, STOP and report (do not relink on your own -- the director decides; the risk is the class-layout segfault the skill describes).
3. Run the gates WATER8 pre-registered (report section 0.1, group L, L1..L8), as ONE sequential chain script under `scratchpad/water8_20260910/gates.sh`, logs per harness under `scratchpad/water8_20260910/logs/`, env assignments on the CHILD (`env SHOT=...`), in this order: `tests/spells/water_ui.sh` (with `env WW_WATERUI_LODSHOT=scratchpad/water8_20260910/images/lodtab_lod.png WW_WATERUI_LODSHOT2=scratchpad/water8_20260910/images/lodtab_water.png` -- read the spell and `src/wateruitest_lod.cpp` first to learn the REAL env names; the names above are WATER8's registered intent), then `ui_align.sh`, `top_bar.sh`, `files_tab.sh`, `animws.sh`, `water_mark.sh`, `water_window.sh`, `lodl_water.sh`, `loaded_nifs` (via its spell). Baselines to compare against (BUILD/UI4, same day): water_ui 48/0 (floor 41), ui_align 11/0, top_bar 43/5, files_tab 28/2, animws 57/0, water_mark dock 20/0 body 3, water_window 46/0, lodl_water 33/0, loaded_nifs 166/2. A count that moved from baseline is explained by name, never waved through.
4. Pictures: `lodtab_lod.png` and `lodtab_water.png` (in-app grabs of `LodGenerationDock`, LOD tab then Water tab, same crop). Open each and describe it in two sentences in the report BEFORE stating anything about it. Also one grab of the top-left of the window (`ui_align.sh` `env SHOT=scratchpad/water8_20260910/images/toprow_after.png`) so the director can show bungo the left strip is back to three tabs.
5. Gate L8 note: L8 as registered asserts 4 px BETWEEN the two segments. bungo has since ruled segments touch (UI6). Run L8 as written and REPORT its number; do not change the gate and do not change the QSS. If it is green with 4 px between segments, that is the expected state for this exe and UI6 changes it.
6. Documents (resume-pending section 7): write `scratchpad/water8_20260910/WW_CHANGES_ENTRY.md` (entry text for the director to splice: measured numbers, exe time/size, every gate with counts, what is red, what was not measured, the refuter L4's actual row height); write `scratchpad/water8_20260910/HANDOFF_BLOCK.md` (the paragraph for HANDOFF.md in the style of `scratchpad/ui4_20260910/HANDOFF_BLOCK.md`); append `## 1..4` sections to `scratchpad/lane_water8_report.md` (do not rewrite 0..0.3); `scratchpad/water8_20260910/MISTAKES_ENTRIES.md` for anything found (including the dead lane's own: a BUILDING marker left up past a finished build with no DONE). You do NOT edit `WW_CHANGES.md`, `HANDOFF.md`, `MISTAKES.md` -- the director splices.
7. Last: `rm scratchpad/water8_20260910/BUILDING`, write `scratchpad/water8_20260910/DONE` (one line: timestamp + exe time/size + water_ui count). Do this ONLY after the gates ran and the documents are on disk, whatever the gate colours are -- a red gate is a valid verdict; a missing DONE blocks lanes UI5 and UI6.
8. Cleanup: `sx_WATER8.sh` at the repo root belongs to the dead lane; delete it (build-verify says a lane deletes its own; you are WATER8's continuation). Do NOT touch `sx_HKXEDIT2.sh` or `sx_tmp.sh`.

## Gates (for THIS lane)
- G1 every number in the report comes from a log file under `scratchpad/water8_20260910/logs/` that exists on disk; the report quotes the log path beside each count.
- G2 `water_ui.sh` count >= 41 (UI4's floor) and group L present in the log (L1..L8 lines); if the spell refuses or crashes, the report says at which check and why, with the log's own line.
- G3 the two LOD-tab pictures exist, are non-empty PNGs, and were described before being cited.
- G4 exe-newer sweep over `git status --porcelain -- src res tools tests` prints no STALE line (resume-pending section 4); the object-vs-header table for `src/nifskope.h` is in the report.
- G5 `tasklist` shows no NifSkope when you finish (harness instances only -- if a `--port`-less window appears it is bungo's; never touch it).

## Rules
- Touch NO source, QSS or `.pro` file. A defect in a GATE (not in the feature) may be repaired in the harness source only if it is a literal/path fix per resume-pending section 10, written as a refusing script, and then the exe must be rebuilt before the gate counts -- that is a build, which needs the director: STOP and report instead.
- Never `git stash`, never commit.
- Plain language in the report; bungo reads the director's relay, not yours.

## Report
Path: `scratchpad/lane_water8_report.md`, appended sections, written incrementally (each section as it finishes):
- `## 1. On disk before gating` (mtime table, marker counts, object staleness table)
- `## 2. Gates` (table: harness, count/failures, baseline, log path, delta explained)
- `## 3. Pictures` (each described in two sentences, then what it proves/does not prove)
- `## 4. Verdict and what is owed` (what is green, what is red with the number, what was not measured and why, what UI6 changes next)
- `## 5. Mistakes` (also copied to `scratchpad/water8_20260910/MISTAKES_ENTRIES.md`)
- `## 6. Finished-work skill review` (skills loaded, skills that should have existed, skills written or amended -- CONSTITUTION 1a; if you amend a skill in the repo tree, name the file so the director mirrors it)
