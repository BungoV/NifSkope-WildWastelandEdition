- ROADS2 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  20:46:44, 21,458,944 B** (RESUME3's was 19:08:42, 21,435,904). Rollback rung
  `release/NifSkope.before_roads2.exe` (19:08:42, 21,435,904 — sha1
  `89065512abfd1fed11ab4f934c943f5972bd106d`, equal to the launch exe byte for
  byte, untouched). Markers: `scratchpad/roads2_20260911/DONE` in, `BUILDING`
  gone. Report `scratchpad/lane_roads2_report.md` (all nine sections, verdicts
  in 0.1); entry text `scratchpad/roads2_20260911/WW_CHANGES_ENTRY.md`; **five
  MISTAKES entries NOT appended by the lane** —
  `scratchpad/roads2_20260911/MISTAKES_ENTRIES.md`, the director splices. **The
  contract IS amended in place** because the brief asked for it:
  `docs/LODGEN_TERRAIN_VT.md` section 1a, six amendments, LF-only. ONE build
  (20:13) plus **TWO counted relinks** (one failed to compile and wrote no exe —
  `lodgen.cpp:9037` used `coverOpts` where the telemetry block has `opts.cover`;
  the second, 20:46:44, is the shipped exe), declared in the report's 4.1.
  **bungo's open NifSkope window needs a restart.** No NifSkope GUI was launched
  by this lane at any point; `Fallout4.exe` was checked before every link and
  was not running.

  **THE INHERITED RED IS CLOSED.** `lodgen_roads.sh` **11/0** (RESUME3 left it
  11/1): R5 reads floor 0.1347, after **0.3431**, reference 0.4038, so bar 1
  (0.3431 >= 0.2694) and bar 2 (0.3431 >= 0.3231) both clear, and the mean
  colour error against vanilla on the road centreline falls from 38.11 with no
  road to **22.37** with one, the whole tile from 22.45 to 20.80. Adding our
  road now makes the sheet MORE like vanilla on both, which was not true before.

  **WHAT THE SEAM ACTUALLY WAS.** Measured on chunk (-20,20) before any code —
  vanilla's own `Commonwealth.4.-20.20.DDS` grid, 512 texels at 32 world units,
  no resampling. Not a piece-join or alpha-compositing bug, which was the
  obvious guess: it is **the road diffuse's own texture pattern printed at
  footprint scale**. A 256-world-unit UV repeat lands every **8.01 bake texels**,
  so the material's own stripes go straight into the sheet. Within-material
  luminance correlation along the road **0.852 ours / 0.016 vanilla**; phase-fit
  R² **0.140 / 0.013** against a 0.022 floor; local 5×5 SD on the road **10.33 /
  6.62**, and off it 5.52 / 5.38 (the control agrees). Three rival mechanisms
  refuted with numbers: **0 of 474** road materials set `bAlphaBlend` (6 of 512
  SHAPES do blend, via `NiAlphaProperty`); median covering-Z spread at a road
  texel 12.299 units, so no z-fighting; **0 of 474** shapes mip-clamped.

  **WHAT SHIPPED, five flags, all on the exe's own usage page.**
  `--road-detail 0..1` (**default 0**) lerps the diffuse sample to the texture's
  own average, which is what vanilla's far road measures as; detail 1 is what
  banded it. `--road-composite max-z|blend` (**default max-z**) — the topmost
  triangle wins, or paint in ascending mean world Z with `dst = lerp(dst, src,
  srcAlpha)`. `--road-raised` / `--no-road-raised` (**default no**) refuses a
  road base that carries its own Distant LOD mesh plus anything under
  `Landscape\Roads\HighwayOverpass\` or `…\Bridge\`. `--road-sidewalks` /
  `--no-road-sidewalks` (**default no**) refuses `Landscape\Sidewalks\`.
  **`--roads-legacy` is the one-token way back and means all four.**

  **THE COMPOSITE DEFAULT WENT AGAINST THE PLAN, and the harness decided it.**
  Blend was built to fix the seam and lost to the max-z path it was meant to
  replace. On `lodgen_roads_metric.py`: old pipeline 0.3061 (bar 2 fails),
  **max-z + detail 0 = 0.3404 (both bars clear)**, blend + detail 1 = 0.2481,
  blend + detail 0 = 0.2669 (both fail), bars 0.2694 / 0.3228. Max-z is also
  better at the piece boundaries (5.996 vs blend's 6.204, vanilla 5.271). Blend
  does win local 5×5 SD (6.542 vs 6.820), phase R² (0.033 vs 0.052) and road
  colour error (12.50 vs 14.09), so it is **kept behind the flag, not deleted**,
  with all five measures in the contract. **If a later lane changes the road
  colour or the grading, re-run all four variants — the ranking is not safe.**

  **TWO FAMILIES REFUSED, each on its own number, chunk (-8,8) downtown.**
  Clearance above the top of the same mask displaced five ways, tie-averaged
  brightness. *Raised roads*: vanilla **−0.009**, before **+0.314**, after
  **+0.001**; flat road still clears (+0.068 vs vanilla's +0.011); non-road
  control −0.028 / −0.005 / −0.094. Census `roadRefusedRaised 110`,
  `roadRaisedBases 35`; downtown road texels **163,586 → 40,436**.
  *Pavements*: 17,801 kerb texels, 15,696 of them >2 texels from any flat road;
  vanilla's clearance there **−0.102** (it paints nothing), ours +0.284, mean
  luminance **86.7 vanilla / 128.1 before / 102.1 after**, while the flat road
  reproduced vanilla's own clearance to **0.001**. Census
  `roadRefusedSidewalk 414`, `roadSidewalkBases 64`. Every refusal is NAMED in
  `roadRefusals`.

  **THE TREE CLAUSE IS NARROWER.** `lodgenIsTreeModel()` matched a `trees`
  component anywhere, catching `SetDressing\TreeSwing01.nif`,
  `TreeNoose01_Branch.nif` and five siblings — swings and gallows props. Now
  scoped to a `landscape` first component. Over every placed base in the
  Commonwealth census: **137 tree bases under both rules** (135
  `Landscape\Trees`, 2 `Landscape\Plants`), **7 flip out** (82 placements,
  **0 with a distant LOD mesh**, so no card or impostor ever came of them),
  **0 flip in**; the 36 of 137 kept bases that carry a distant LOD mesh are
  exactly the 36 lines `--list-impostor-candidates --candidates trees` prints
  for the whole worldspace, so the re-typed classifier and the shipped C++ agree
  base for base.

  **GATE S2, THREE WAYS, 9 of 9 FILES EACH, cells -20..-17 x 20..23.**
  `--roads --roads-legacy` on the new exe = the rung's `--roads` bake byte for
  byte (content hash `32f88dc8df56092a` both sides); `--no-roads` = the rung's
  `--no-roads` bake (`72cc5e098dfa7d4e`); and `--roads` with nothing else named
  = `--roads` with all four knobs spelled out at their defaults, so the defaults
  are exactly those four and not a fifth unstated one. Region bakes only, all
  under `scratchpad/roads2_20260911/out/`; **his installed `Data\Terrain` was
  never touched**.

  **GATE S4 WAS REFUSED AS WRITTEN and substituted.** The brief's gate — the
  four `SetDressing\Tree*` props absent from the Sanctuary candidate list — is
  green on the RUNG too: Sanctuary lists 20 lines, the whole Commonwealth 36, no
  `SetDressing` in either on either exe, because the lister returns early on
  `!b.hasLod` (`nifcli.cpp:3350`) and all seven props have no MNAM and bit 15
  clear. The gate could not fail. The flip table above is the substitute and it
  fails in both directions. Found by running the gate on the OLD exe first.

  **THE HARNESS CHAIN, 20:51→20:55, every count at RESUME3's baseline:**
  `lodgen_roads.sh` **11/0** (was 11/1 — closed), `lodgen_terrain.sh` 26/0,
  `lodgen_terrain_vt.sh` 41/1 (`V9b`, red on the rung too),
  `lodgen_ground_cover.sh` 29/5 (red on the rung too), `lodgen_terrain_pbrm.sh`
  14/0, `lodgen_native.sh` 18/0, `lodgen_panel_run.sh` 125/0,
  `lod_generation.sh` 116/0, `ui_align.sh` 11/0, `water_ui.sh` 82/0. 383 checks,
  6 failures, all six red on the rung as well.

  **PICTURES, all four looked at before being cited**, same grid, same mip, no
  resampling, numbers burned in: `scratchpad/roads2_20260911/images/cmp_seam.png`
  (the banding is plainly there in the BEFORE zoom and plainly gone in the
  AFTER: boundary gradient 5.302 vanilla / 9.752 / **5.712**, local SD 6.653 /
  10.375 / **6.678**), `cmp_highway.png` (vanilla's downtown sheet has **no
  highway in it at all**; ours painted a bright interchange and now does not),
  `cmp_sidewalk.png` (**the change bungo has not seen** — a bright cream kerb
  band that vanilla has nothing of, gone), `cmp_sanctuary_road_v2.png` (ROADS1's
  crop re-taken).

  **STILL RED, WITH THE NUMBER.** Ours: the road interior is now **too smooth**
  — local 5×5 SD 4.369 against vanilla's 5.533, having been 10.375, the known
  cost of detail 0; mean road luminance **101.59 against vanilla's 92.18**; and
  laid beside vanilla at Sanctuary, **vanilla's cul-de-sac is a soft desaturated
  blue-grey blob and ours a crisp warm pale-grey ribbon with a visible kerb
  line** — the geometry is right, the hue and the crispness are not, and neither
  has a number yet. That chroma/edge measurement is **owed to a later lane**.
  The skirt still darkens with the mesh's vertex alpha where vanilla's does not
  (correlation −0.791 vs +0.001) but the size is small: 3.7 luminance units over
  676 of 262,144 texels. **Not ours:** the off-road ground is ~16 luminance units
  too dark (whole sheet 71.16 against vanilla's 83.84; off-road 68.69 against
  83.52) — TILING2 and GRADE1's.

  **FOR LANE TILING2, ONE THING.** Every road number in this lane was measured
  on top of the current ground. When TILING2 changes the ground, `lodgen_roads.sh`
  R5 (bar 2 is 0.8× the ground's own agreement with vanilla) **and** the
  four-variant composite ranking both need re-running before anyone assumes
  max-z still wins.

  **TWO SKILLS AMENDED**, both trees (`…NifskopeWildWastelandEdition\.claude\skills`
  and the live mirror `E:\Projects\Claude\.claude\skills`), LF-only, verified by
  byte count on both copies. `ww-spec-gate-audit` gained *"Run the gate on the
  OLD binary FIRST"* — a gate that is already green is measuring something else
  — with the five-row flip table to substitute when one turns out vacuous.
  `ww-control-calibration` gained *"A rank statistic with a dominant tie block is
  not reproducible"*: FLAGSCAN1's grey AUC of **0.716 re-measures as 0.757 from
  its own script**, because one saturation value covers **53.5 percent** of that
  tile and `np.argsort` breaks ties by raster index; tie-averaged it is 0.731. It
  carries a fifteen-line `rankdata`/`auc_tie` (no `scipy` here) and four
  reporting rules, including **prefer a clearance over a raw score**, which is
  how every family gate in this lane was read.
