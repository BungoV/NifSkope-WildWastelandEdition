# Lane GATEFIX2 -- `native_lighting` has two standing reds from stale calibration (owns exe slot; build only if needed)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/gatefix2_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; fallback name `DELIVERABLE_TEXT.md`; `PENDING.md` past
  half context). Pictures only under that folder, NEVER repo-root `images/`.
- You OWN the exe slot (and the build slot, though this lane should not need a build). `tasklist | grep -i -E
  "Fallout4|NifSkope"; echo rc=$?` as ITS OWN command before every exe run. Fallout4 up = write `BUILD PENDING` and
  END; never wait-loop; NEVER end your turn waiting on your own batch -- wait in the foreground, then continue. ONE
  NifSkope, `--port <unused>` + `WW_WINDOW_AT=1960,40`, absolute paths. A NifSkope without `--port` is bungo's: never
  kill it. NEVER run any `release/NifSkope.before_*.exe` with a GUI.
- Exe at launch: 23,625,728 B, 2026-09-19 19:49:03, sha1 c529e3c12fe4216a3c33cc14631e3c811169fbb0.
- ANOTHER LANE (CELLVIEW4) is live and CODE-ONLY. Do not touch `src/cell*`.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/gatefix1_20260919/` (all text files) and the GATEFIX1
  entry in `WW_CHANGES.md`; `tests/spells/native_lighting.sh`, `native_lighting_check.py`,
  `native_lighting_fixtures.py`; skills `ww-gate-owns-its-fixtures`, `ww-control-calibration`, `ww-spec-gate-audit`,
  `ww-analytic-fixture-gate`.

## The work
1. Reproduce today's state: `native_lighting.sh` 19 pass / 2 red (IoU 0.850 vs bar 0.800 direction as GATEFIX1
   recorded it; blockSD 2.28 vs floor 3.50). State exactly what each red row measures and why GATEFIX1 called the
   calibration stale.
2. Re-calibrate on a NAMED, HASHED container the gate owns (always `--road-detail 1`, authored LOD models only):
   bake it, record sha1s, measure the two statistics on the known-good state AND on a deliberately broken state (the
   defect each row exists to catch -- read the row's history in WW_CHANGES/MISTAKES to find it). A bar is set between
   the two populations with the margin stated; a bar that the broken state passes is not a bar. If the two states do
   not separate, the statistic is wrong: replace it, do not tune it.
3. This is NOT "lower the floor until green". If the honest reading is that the lighting output regressed, say so
   with the pictures and leave the row red.
4. Re-run `native_lighting.sh` twice (determinism), plus neighbours `render_shot.sh`, `native_open.sh`.

## Rules
No "fixed/final/true" -- mechanism + refuter. Plain words. Masters ship OFF. Never decimate.

## Report
Container name + sha1s; the two-population table per row; before -> after gate counts; WW_CHANGES + HANDOFF text;
MISTAKES appended to root MISTAKES.md (top CRLF, byte splice, CR before/after); skill update to both trees if earned.
Final message under 200 words.
