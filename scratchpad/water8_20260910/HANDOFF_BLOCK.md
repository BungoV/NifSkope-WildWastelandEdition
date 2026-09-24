<!-- Lanes WATER8 + WATER8-GATE, 2026-09-10/11. TEXT ONLY for the HANDOFF.md top
     block; the director splices it (CONSTITUTION 8). -->

**WATER8 LANDED AND IS NOW GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-10 21:02:12**, **20,830,208 bytes** (UI4's was 20:45:47,
20,798,976). Markers: `scratchpad/water8_20260910/DONE` in, `BUILDING` gone --
**lanes UI5 and UI6 are unblocked**. Report `scratchpad/lane_water8_report.md`
(sections 0..0.3 are WATER8's pre-registered gates, 1..6 are the gating run);
entry text `scratchpad/water8_20260910/WW_CHANGES_ENTRY.md`; mistakes text
`scratchpad/water8_20260910/MISTAKES_ENTRIES.md` (TWO entries, not yet in
`MISTAKES.md`). No rollback rung was taken for this build; the nearest is
`release/NifSkope.before_ui4.exe` (18:25:20).

**Lane WATER8 itself died at its account limit right after its chain returned
`CHAIN-RC=0`**, leaving `BUILDING` up, no `DONE`, no documents and an empty
`images/`. It had built correctly; nothing was wrong with the exe. Lane
WATER8-GATE (2026-09-11 05:2x) ran the gates, took the pictures and wrote the
documents. It built nothing and touched no source, QSS or `.pro` file.

## What bungo gets

*"What? I wanted it in that right panel though"* -- done. The LOD Generation
dock on the RIGHT now opens with its own two-segment strip under the title:
**LOD** (the generator, unchanged) and **Water** (the whole marking tool,
including the full-screen flow window's button). The LEFT strip is back to
**Header | Blocks | Files**, the standalone Water Marking dock is gone, and so
are both Workspaces-menu entries. Both strips carry the SAME stylesheet, byte
for byte, in the same 35 px row.

Pictures, `scratchpad/water8_20260910/images/`: **`lodtab_water.png`** and
**`lodtab_lod.png`** (499x741, the same dock, same crop, the two tabs) and
`toprow_after.png` (602x82, the three-tab left strip). In-app grabs.

## Gates (all on the 21:02:12 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **59 checks, 0 failures, 0 skips, PASS** (floor 48) | 48 / 0, floor 41 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `top_bar.sh` | 43 / **5** -- the same five `Panels lists the ... dock` | 43 / 5 |
| `files_tab.sh` | 28 / **2** -- BUILD9's same two | 28 / 2 |
| `animws.sh` | 57 / 0, 1 skip (the 10mmPistol has no sequence), PASS | 57 / 0, 1 skip |
| `water_mark.sh` | dock 20 / 0 PASS; self-test FAIL on X2b **0.407** (gate > 0.5), body 3 | same red, same number as BUILD12 |
| `water_window.sh` | 46 / 0, PASS | 46 / 0 |
| `lodl_water.sh` | 33 ok, 0 FAIL, RESULT PASS (prints no count) | 33 / 0 |
| `loaded_nifs.sh` | **166 / 0, PASS** | 166 / **2** |

Consistency, not just success: `src/nifskope.h` (20:59:18) is older than **35
of 35** objects that include it (20:59:57..21:02:08); **116 of 116** changed
files under `src res tools tests` are older than the exe; `res/style.qss` and
`release/style.qss` are byte-identical.

Skipped with the reason: every suite the change does not reach (lodgen,
terrain, impostor, gltf, hkx*, collision, block, water solve / flow / weights),
`skeleton_overlay.sh` (flaky by BUILD11's own measurement) and `lodl_open.sh`
(its fixture is bungo's installed `Commonwealth.lodl`, which nothing here
rewrites).

## Three things for the director

1. **`loaded_nifs.sh` 166 / 2 -> 166 / 0, and the two are finally NAMED.**
   BUILD12's handoff said nobody could say which check turned green because only
   a count had been recorded. A control run of the kept rung
   `release/NifSkope.before_ui4.exe` (18:25:20, WATER7's fourth tab still in)
   answers it: `the top selector orders Header, Blocks and NIFs without
   remapping modes` and `NIF Browser is above Loaded NIFs in its own mode`.
   Removing the fourth left tab cured both. Log
   `scratchpad/water8_20260910/logs/loaded_nifs_CONTROL_ui3exe.log`.
2. **Gate L8 is HALF the gate it was registered as, and lane UI6 needs to know.**
   Report 0.1 registers it as 4 px clear of the row *"and of one another"*; as
   shipped (`src/wateruitest_lod.cpp:485-517`) it measures only the row's top
   and bottom, and **nothing measures the gap between the LOD strip's two
   segments**. The inter-segment gap is measured only on the LEFT strip, by
   UI4's `(S5) every pair of segments is 4..4 px apart` -- and that 4 is exactly
   what bungo's *"Why are they separated?"* sends to 0. UI6 must add L8's
   missing half, or the right-hand strip keeps its gap silently. Nothing was
   changed here: a gate repair needs a build, and the brief forbids one.
3. **X2b is still 0.407** on body 3 against a gate of > 0.5 (0.595 on body 2).
   Unchanged since BUILD12 to three digits, so it is the instrument, not a
   regression -- and still owed to bungo as a decision.

## Restart

**NO -- he already has it.** bungo launched `release\NifSkope.exe` himself at
**2026-09-11 05:32:30** (pid 8428, no `--port`, so it is his interactive window
and not a harness), which is after the 21:02:12 link. His open window IS this
build: the Water tab is in the right-hand panel in front of him now.

**That window HOLDS `release/NifSkope.exe`.** The next build must rename the
running copy aside immediately before the link (`NifSkope_inuse_8428.exe`),
never kill it -- and lanes UI5/UI6, which this DONE just unblocked, are the ones
that will hit it.

## State

Nothing committed (CONSTITUTION 8). Changed by lane WATER8:
`src/lodgenmanager.cpp`, `src/nifskope_ui.cpp`, `src/nifskope.h`,
`src/wateruitest.cpp`, `NifSkope.pro` (all through
`scratchpad/water8_20260910/hookup.py`, CR 0 in all three shared files) and the
new `src/wateruitest_lod.cpp`. Changed by WATER8-GATE: nothing outside
`scratchpad/water8_20260910/`. Game down (`rc=1`) at every check; no harness
instance was left running, and the only NifSkope on the machine at the end is
bungo's own window (pid 8428).

**Lane UI5 is ALIVE in the tree** (`scratchpad/ui5_20260910/probe.cpp` and
`release/ui5_probe.exe` both stamped 2026-09-11 05:22, a standalone Qt probe,
not NifSkope). It has touched no file under `src/`, `res/`, `tests/` or
`tools/`, so the exe-newer sweep above still holds -- but the next build must
account for whatever it lands.
