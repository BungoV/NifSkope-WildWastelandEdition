**Lane PANEL1, 2026-09-12 — every bake setting is now a row in the LOD Generation panel.**
`src/lodgenmanager.cpp` gained **57 rows** covering 56 `lodgen` switches (audit: 133 settings; 93
bake settings, 37 of which already had a row; 31 diagnostics and 3 paths stay on the command
line). **No default moved.** Exe `release/NifSkope.exe` 2026-09-12 **18:07:58**, **22,154,240 B**, rc 0,
one TU relinked; rung `NifSkope.before_panel1.exe` (15:49:03). **His open NifSkope needs a restart
to see this. Nothing was committed.**
Proof — `tests/spells/lodgen_byte_gate.sh` (new), Sanctuary (-20,24) at dim 4, failures 0:
* (a) rung vs new exe, every new row at its default: 11 files, 47,645,750 B, same SHA1 — not one byte moved (re-run on 18:07:58).
* (b) each row alone moved, 49 bakes: 32 move the bake, 2 leave it alone (the thread counts, the control), 23 this chunk cannot exercise, 0 refused, 0 unmovable.
* (c) the panel's bake vs the command line's, same settings: **15 files, 0 differ, 0 missing**.
The picture: `shots/panel_full_after.png`, **483x2897 px**, the whole settings column with every
folding section open — 55 of the 57 new rows (the other two are hidden under the FO4CS target by
the panel's own rule). New harness knob `WW_LODGEN_SHOT_FULL=<png>`; the old dock grab is
byte-identical to before. `lod_generation.sh` 121 checks 0 failures on the new exe.
The gate was wrong five times and the pictures once (three grabs that showed none of the new
rows); five entries in `MISTAKES.md`, three skills gained a section.
Owed: `--incremental` (its leg lives in the CLI driver), `--water-channels` / `--water-cull-buried`
(no switch, no row), the 20 rows one chunk cannot exercise, a full-column picture of the STOCK
target, `vanillaLodRoot`'s hardcoded data root, `--road-ground-paint`'s default (lane DEFAULTS1).
Full report: `scratchpad/panel1_20260912/lane_panel1_report.md` — §0 audit, §3 picture, §4 owed.
