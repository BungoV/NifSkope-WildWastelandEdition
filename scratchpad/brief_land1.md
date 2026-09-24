# Lane LAND1 -- TILING5 + INCR1 folded into one lane (bungo 2026-09-12 05:2x: "fold small ones together")

This lane does BOTH `brief_tiling5.md` (terrain-guided land sampling) and `brief_incr1.md` (incremental regeneration),
in that order, as one lane. Read both briefs in full; this file only says what changes when they are one lane.

## Header (overrides the two briefs' headers)
- Exe at launch: `release/NifSkope.exe` -- 2026-09-12 06:31:05, 21,819,904 B (ROADS4's DONE exe: --road-detail default is now 1.0, --road-ground-paint knob at 1.0; the road-detail flip is DONE, so INCR1 item 0 is void) . ONE rung before the first build:
  `release/NifSkope.before_land1.exe` (not before_tiling5, not before_incr1).
- Markers `scratchpad/land1_20260912/BUILDING` / `DONE`; report `scratchpad/lane_land1_report.md` with a
  `# Part A -- TILING5` and a `# Part B -- INCR1` section, each carrying that brief's required sections; PENDING.md
  past half context, in the same folder, saying which part is finished.
- One set of documents in `scratchpad/land1_20260912/`: `WW_CHANGES_ENTRY.md` (ONE entry, `## 2026-09-12 — <title>`,
  with a subsection per part), `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`, `CHANGED_FILES.txt`.
- Pictures under `scratchpad/land1_20260912/images/`, named `a_*.png` and `b_*.png`.
- Builds: Part A builds and gates once; Part B builds and gates once; INCR1's byte-identity gate (dirty rebake == full
  bake) runs on the exe that has BOTH changes, and with `--road-detail 1` and with Part A's winning switch ON as one
  of its arms -- an incremental rebake must be byte-identical under every sampling switch.
- INCR1 item 0 is VOID: ROADS4 landed the `--road-detail 1` default (gate G0 green). Skip it.
- INHERITED RED (from ROADS4, yours because it is in tests/spells/lodgen_*): `tests/spells/lodgen_roads.sh` reads 11 checks / 1 failure
  on the 06:31:05 exe -- R5 0.3078 against a bar of 0.3223, short by 0.0145, caused by the road-detail default flip alone
  (detail 0 = 0.3435 passes, detail 1 = 0.3078 fails, both measured on the rung). The bar was calibrated when detail 0 was
  the default. Recalibrate R5 to the detail-1 default with the SAME method that set 0.3223 (find it in WW_CHANGES.md /
  the harness comments; ROADS2 or ROADS3 set it); if the method is not recorded, say so and set the bar from vanilla's
  own value on the same chunks with the margin the other rows use. State the old and new bar in the report. Expected
  lodgen chain on the 06:31:05 exe otherwise: lodgen_terrain 26/0, lod_generation 116/0, lodgen_terrain_vt 41/1 (V9c),
  lodgen_ground_cover 29/5, lodgen_terrain_pbrm 14/0, lodgen_native green; lodl_open 23/2 with six segfaults is UINOTES1's
  (lane UINOTES2 owns it, not you -- report the row, do not fix).
- Game check before every build and exe launch: count Fallout4.exe SEPARATELY from NifSkope.exe.
- A UI lane (UINOTES2) is building in the COPY tree E:/Projects/NifskopeWWE_ui at the same time; it never touches the
  main tree, and you never touch the copy. Before every GUI launch check `tasklist` for a NifSkope with `--port`; wait if
  one is up; a NifSkope without `--port` is bungo's.
- Everything else (skills, gates, rules, "stay out of the UI files", game-down, one NifSkope, region bakes only) as
  the two briefs say. Where they disagree, TILING5's brief is newer and wins.
