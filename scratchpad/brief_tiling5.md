# Lane TILING5 -- terrain-guided land sampling: the warp steered by the terrain's own normal/slope (bungo's ruling 2026-09-12 05:1x)

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. NOTHING is committed. Never bungo's installed
  Data\Terrain; never the whole Commonwealth; region bakes only (`t4_bake.sh` below). Game (Fallout4.exe) must be
  down for every build and every exe launch; if it is up, the lane ends BUILD PENDING.
- Exe at launch: `release/NifSkope.exe` -- director fills: ______ (the DONE exe of the lane before you). Rung ONCE before
  your first build: `release/NifSkope.before_tiling5.exe`. Markers `scratchpad/tiling5_20260912/BUILDING` / `DONE`.
  Report `scratchpad/lane_tiling5_report.md`, incremental, PENDING.md past half context.
- `--road-detail 1` on EVERY bake and EVERY picture (his ruling 05:0x; the default flip may already be in the tree --
  check `src/lodgen.h` `roadDetail`; pass the flag regardless).
- Stay OUT of the UI files (`src/nifskope*.cpp`, `src/anim*`, `src/hkx*`, `src/ui/*`, `res/style.qss`). Yours are
  `src/lodgen.cpp`, `src/lodgen.h`, `src/nifcli.cpp`, `docs/LODGEN_TERRAIN_VT.md`, `tests/spells/lodgen_*`.
- Read first: `CONSTITUTION.md`, HANDOFF.md top block, `scratchpad/lane_tiling4_report.md` (the swirl/repeat instrument,
  the frozen fourteen-sheet split, gate F3 and why it was NOT met), `scratchpad/lane_tiling3_report.md` (the warp),
  `src/lodgen.cpp` 5920-6200 (warp + hex + the single hash) and the two sampling sites (~7652 and ~9014: the
  heightmap normal `nrm` is computed in the SAME iteration a few lines above the land lookup -- that is your input),
  `docs/LODGEN_TERRAIN_VT.md` the land-sampling section.
- Skills: `nifskope-ww-lodgen`, `nifskope-ww-build-verify`, `ww-prototype-is-not-the-product` (the real exe scores
  the fourteen sheets, same scorer `tiling4_20260912/f3_full.py` imported unchanged), `ww-control-calibration`,
  `ww-spec-gate-audit`, `ww-texel-picture`.

## His words, verbatim
- 05:1x, after the warp sweep picture (vanilla | no warp | warp 170 | warp 340 | hex): "what is used for the land
  sample warp? the normal or slope map?" -- answer given: neither, a hashed value-noise lattice on world X/Y.
- 05:1x: "since we're reusing vanilla terain normals and slope maps, might as well use them to guide this a bit"
- earlier, 05:1x: "the warp is too strong, what is it set to?" (683 units = two repeats; he finds that too strong).

## What is on disk already
- The hash warp: `lodgenLandWarp` (amp `--land-warp`, lattice `--land-warp-lattice`, octaves), off by default.
- The hex tiling: `lodgenLandHexTap` (`--land-hex`, `--land-sample stochastic`), off by default.
- The mip bias `--land-mip-bias`; the preset `--land-sample warp` = amp 683, lattice 1024, 1 octave, bias -1.0.
- Warp sweep bakes with textured roads: `scratchpad/tiling4_20260912/out/dw_0|dw_170|dw_340|dw_hex/t2020/tex/
  Commonwealth.4.-20.20.DDS` (05:14, on the 04:10:38 exe), picture `scratchpad/warp_sweep.png`, script
  `scratchpad/warp_sweep.py`. Local 5x5 SD on that sheet: vanilla 5.48, plain 5.96, warp170/340 7.04 (the 7.04 is the
  bias -1.0, not the warp), hex 6.19.
- At the sampling site the exact heightmap normal `nrm` (from `lodgenTerrainHeightAt`, VHGT spacing) is in scope;
  vanilla's `_msn` sheets are loaded in `vanilla-blend` mode (~6512) -- a second, finer (32 u/texel) source.

## The work
1. Macro normal. A 32-unit texel normal is noise for this purpose; build the terrain's LOW-PASS normal at a scale the
   lane chooses by measurement (try 256, 512, 1024 world units; the lattice the warp already uses is 1024). It must
   come from the global heightmap, not from the sheet, so it is continuous across chunk and region borders (gate:
   bake two adjacent chunks separately and together, byte-identical). Say whether the vanilla `_msn` adds anything
   the heightmap cannot give; if not, do not depend on it.
2. Three candidate rules, each a switch, each measured on the fourteen sheets with the REAL exe and TILING4's scorer:
   (a) DOWNHILL DRAG: sample offset = k * (macro nx, ny) -- the texture slides downhill by an amount proportional to
       slope (k in world units per unit of tangent; sweep). Flat ground gets no offset, so alone it cannot hide the
       repeat on flat ground -- measure that, do not assume it.
   (b) ASPECT ROTATION: rotate the sampling frame by the downhill azimuth of the macro normal, weighted by slope
       (flat = no rotation, steep = full), so the tiling grid's axes follow the terrain instead of the world axes.
   (c) SLOPE-MODULATED HASH WARP: the existing hash warp with its amplitude scaled by a function of macro slope
       (both directions tried: more warp on slopes, more warp on flats), so the hash warp can be much weaker than 683
       where he sees it as too strong.
   Plus the combinations the numbers suggest, and each with/without the hex tiling. Every candidate off = the rung's
   bytes (gate F2 byte identity, `tiling4_20260912/f2_gate.py`).
3. Gates, registered BEFORE any candidate is scored: F3 (repeat, fourteen sheets, per sheet vs its own vanilla;
   report selection and validation sevens separately), G1/G2 grain as TILING4 froze them, the border-continuity gate
   from item 1, F2 byte identity, the swirl instrument's known-answer controls run first. A candidate that trades
   repeat for swirl or blur is refused by its own numbers, as TILING4 did.
4. Ship: the winning rule behind `--land-guide <rule>[:k]` (name the switch better if the code says so) and, if it
   beats the shipped `--land-sample warp` preset on F3 without losing on grain, replace that preset's contents with it
   and say so in the flag table. Default stays plain footprint sampling unless bungo rules (he has not).
5. Pictures (`ww-texel-picture`): `scratchpad/warp_sweep.py`'s window on (-20,20) at 3:1 + full sheet, panels
   vanilla | plain | best of (a) | best of (b) | best of (c) | shipped warp 683, ALL with `--road-detail 1`; one
   second sheet from the validation seven, a sloped one, same layout. `scratchpad/tiling5_20260912/images/`.
6. Documents in `scratchpad/tiling5_20260912/`: `WW_CHANGES_ENTRY.md` (`## 2026-09-12 — <title>`, em dash),
   `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`; `docs/LODGEN_TERRAIN_VT.md` land-sampling section + flag table amended by
   you; `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` gets one line if a flag becomes recommended; skills you add or
   amend under `.claude/skills` (listed in the report's finished-work skill review).

## Bake helper
`bash scratchpad/tiling4_20260912/t4_bake.sh <exe> <variant> <tile> --road-detail 1 [args]` writes to
`tiling4_20260912/out/<variant>/...` -- COPY it to `scratchpad/tiling5_20260912/t5_bake.sh` with the out-dir changed
and use yours. `f3_full.sh` shows how the fourteen sheets are baked and scored.

## Rules
Plain language; every number beside its floor; no "final/true" claims; incremental writes; build chain per
`nifskope-ww-build-verify` (game check inside the chain, rename-aside, exe-newer sweep, stale-object sweep, one build +
counted relinks); one NifSkope instance at a time (`--port`, second monitor); never touch a NifSkope without `--port`.
