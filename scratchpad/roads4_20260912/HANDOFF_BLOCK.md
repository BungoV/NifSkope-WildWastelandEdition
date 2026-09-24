# 2026-09-12 06:5x -- lane ROADS4: road detail on by default, and the terrain inside the road models located and sized

`release/NifSkope.exe` built **06:31:05**, **21,819,904 bytes**, one build, zero
relinks. Rung kept as `release/NifSkope.before_roads4.exe` (05:48:33, md5
`980e64c1aa4e5478b5833d83ebea9655`). Nothing committed. bungo's open NifSkope
window needs a restart to pick the new exe up.

## What landed

* **`--road-detail` now defaults to 1.0** (`src/lodgen.h`), bungo's ruling of
  05:0x: *"--road-detail 1 is always on, do not ever use road detail 0, that
  looks terrible"*. `--road-detail 0` reproduces the old default bake **byte for
  byte** -- 9 files on chunk (-20,20), 10 on (-8,8), zero differing, `bake.log`
  excluded because it records the command line.
* **New CLI question `--road-ground-paint 0..1`** (`src/lodgen.cpp`,
  `src/nifcli.cpp`): the coverage multiplier for a road shape whose material
  lives under `materials/Landscape/Ground/`. **Defaults to 1.0 = existing
  behaviour.** It ships as the instrument that refuted its own candidate, with
  the measured numbers in its `--help` text.
* **The road census counts them**: `ground_shapes` / `ground_texels` on the
  census line, `roadGroundPaint` / `roadGroundShapes` / `roadGroundTexels` in the
  meta report.

## The finding, which is bungo's own observation with numbers on it

He said *"these meshes have some terrain included there, you can see the sharp
mesh terrain being included into the chunk's bake"*. Fallout 4's road NIFs
(`Landscape\Roads\Sanctuary\SancRoadStr01.nif` and siblings) carry shapes
materialled from `materials\Landscape\Ground\` -- `CommonwealthDefault01`,
`DirtGravel01` -- the verge and the junction fill. They win **36.1 per cent** of
the road plane on (-20,20) and **24.9 per cent** on (-8,8). The two-tone step
where they meet the asphalt is **25.28** and **13.13** levels against vanilla's
**2.53** and **1.76**; the boundary gradient is **15.387** and **8.010** against
vanilla's **5.362** and **5.138**, with a displaced floor of 6.077 and 4.805.

**The obvious fix is refuted by baking it.** Fading those shapes out makes the
seam monotonically worse (15.387 -> 33.352 as the knob goes 1 -> 0) because the
patch darkens toward our ground while the asphalt beside it does not move, and
R5 falls from 0.3078 to 0.2783. The default stays 1.0.

**What the numbers point at is road opacity, and the two chunks disagree**, which
is lane ROADS3's refusal a second time. `--road-opacity 0.326` gives vanilla's
own seam number on both tiles (5.304 vs 5.362; 4.242 vs 5.138) and destroys R5
(0.1576, both bars fail, centreline colour error 23.64 -> 30.87).
`--road-opacity 0.83` is the only value that passes both R5 bars (0.3545 >=
0.3271) while improving the seam to 12.630. **Nothing about opacity was
changed. This is the open decision and it is bungo's.** The picture that prices
it is `scratchpad/roads4_20260912/images/road_ground_look.png`.

## Red, and why

* **`tests/spells/lodgen_roads.sh` is 11 checks / 1 failure** on the new exe.
  R5 `after` 0.3078 against bar 2 = 0.3223, short by 0.0145. **Caused by the
  detail flip alone**, measured on the rung before the build: detail 0 reads
  0.3435 and passes, detail 1 reads 0.3078 and does not. R5's bar was calibrated
  when detail 0 was the default. bungo's eye outranks it; the bar needs
  re-stating by whoever owns R5, not meeting.
* **`tests/spells/lodl_open.sh` is 23 checks / 2 failures with SIX segmentation
  faults** in the headless render path (`WW_RENDER_VIEW=1`, `0 planes rendered,
  0 distinct pictures`). It was 23/0 on the 04:10:38 exe, so the crash arrived
  with one of the nine UI rulings in the 05:48:33 build -- **before this lane
  touched anything**. Reported to lane UINOTES1b in
  `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md`, not fixed here.
* Green on the new exe: `lodgen_terrain.sh` 26/0, `lod_generation.sh` 116/0.

## Refused, with numbers

* **G1 (vertex-alpha correlation) is refused as an instrument artefact.**
  ROADS3's -0.792 came from `seam.py` allocating its alpha buffer with
  `np.zeros` and never filling in the ones for shapes that carry no vertex
  alpha. Re-derived with ones as the default: ours -0.0345 against vanilla
  -0.0359 on (-20,20), +0.0404 against +0.0766 on (-8,8). Indistinguishable.
* **The brief's premise is refuted.** "The road meshes carry terrain-shaped
  skirt geometry" -- **skirt-only texels are 0** on both chunks. A skirt
  triangle is the max-z winner on 455 of 23,116 texels and 556 of 11,069, and
  never alone. There is nothing to suppress by vertex alpha.
* **G3** (feathered boundary not worse than 3.979) reads 5.080, and the rung
  read 3.988 -- so the detail flip is what made it red, not any candidate.

## Where things are

* Lane report: `scratchpad/lane_roads4_report.md` (items -1 through 8).
* Pictures: `scratchpad/roads4_20260912/images/` -- `road_ground_look.png`,
  `road_ground_where.png`, `road_ground_profile.png`. Every panel a real bake.
* Numbers: `scratchpad/roads4_20260912/logs/` -- `gp.json` (the variant table),
  `gates.json`, `skirt.json`, `material.json`, `r5_variants.txt`,
  `after_*.txt`.
* Bakes: `scratchpad/roads4_20260912/out/` (11 variants x 2 chunks, region bakes
  only; bungo's installed `Data\Terrain` untouched).
* Changelog text: `scratchpad/roads4_20260912/WW_CHANGES_ENTRY.md`.
  Mistakes: `MISTAKES_ENTRIES.md`. Doc amendment:
  `LODGEN_TERRAIN_VT_1a5.md`. New skill:
  `.claude/skills/ww-material-folder-classify/SKILL.md`.

## What the next lane should NOT re-derive

The skirt hypothesis (dead, 0 texels), the vertex-alpha correlation (dead,
instrument), and the ground-paint knob as a fix (dead, baked and worse). The
live question is one sentence: **our asphalt sits at luminance 110.82 where
vanilla's is 93.54, and every seam number in this lane is that one tone.**
