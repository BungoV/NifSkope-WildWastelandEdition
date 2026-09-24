# Lane GROUND1 -- TERRAIN-AO1 + EROSION1 folded into one lane (bungo 2026-09-12 05:2x: "fold small ones together")

This lane does BOTH `brief_terrain_ao1.md` (far terrain receives ambient occlusion from the placed objects) and
`brief_erosion1.md` (deterministic hydraulic erosion at bake resolution feeding the normal sheet and the colour
shading), in that order, as one lane. Read both briefs in full; this file only says what changes when they are one.

## Header (overrides the two briefs' headers)
- Exe at launch: `release/NifSkope.exe` 2026-09-12 09:32:37, 21,951,488 B, sha1 3e1914a0637b66f438d873e0230b1e8c04d7c806
  (LAND1's: it carries --land-guide (default off), --incremental, the .lodb ledger, AND the UINOTES2 UI merge). ONE
  rung before the first build: `release/NifSkope.before_ground1.exe`.
- Baselines on that exe (LAND1's chain 09:3x): lodgen_roads 11/0, lodgen_native 18/0, native_baseline 25/25,
  lodgen_terrain_vt 41/1 and lodgen_ground_cover 29/5 (inherited reds, compare line by line, not by count),
  lod_generation 116/0, pbrm 14/0, lodl_open 23/0, animws 236/0/1 skip. Same rows expected from you, plus yours.
- Build gate lesson from LAND1: "exe newer than source" passed over six STALE objects (files merged with old mtimes).
  Before any gate run `make -n` in the MSYS2 shell and require it to print nothing to compile. Count Fallout4.exe
  SEPARATELY from NifSkope.exe in every guard (two numbers, two lines). Never touch E:/Projects/NifskopeWWE_ui.
- Stay out of scratchpad/land1_20260912/ and scratchpad/uinotes2_20260912/ (other lanes' delivered work).
- Markers `scratchpad/ground1_20260912/BUILDING` / `DONE`; report `scratchpad/lane_ground1_report.md` with a
  `# Part A -- TERRAIN-AO1` and a `# Part B -- EROSION1` section, each carrying that brief's required sections;
  PENDING.md past half context, saying which part is finished.
- One set of documents in `scratchpad/ground1_20260912/`: `WW_CHANGES_ENTRY.md` (ONE entry, `## 2026-09-12 — <title>`,
  a subsection per part), `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`, `CHANGED_FILES.txt`.
- Pictures under `scratchpad/ground1_20260912/images/`, `a_*.png` and `b_*.png`; every picture `--road-detail 1`.
- Both parts shade the same bake texel from the heightmap: the order of the two terms in the colour composite is a
  design decision -- state it, gate it (each term off = the rung's bytes; both on = the documented order), and put
  it in `docs/LODGEN_TERRAIN_VT.md`. EROSION1's normal-sheet output and TERRAIN-AO1's occlusion must not be computed
  from each other's intermediate; both read the heightmap (erosion reads the raw one, AO reads the eroded one only if
  the report shows the number that justifies it).
- Builds: Part A once, Part B once; Part B's gates run on the exe with both.
- Everything else as the two briefs say (skills, gates, rules, game-down, one NifSkope, region bakes only, stay out of
  the UI files). Where they disagree, EROSION1's brief is the one with his verbatim words and wins.
