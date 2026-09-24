- TILING2 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  21:52:22, 21,466,624 B**, sha1 `9492e5a60deaf9aab19bf6269c14bf6caed01989`
  (ROADS2's was 20:46:44, 21,458,944). Rollback rung
  `release/NifSkope.before_tiling2.exe` (21:20:07, 21,458,944 B — sha1
  `955b0952a5f7ab62d4dcfef7852a923c5a6d3f22`, the launch exe byte for byte,
  untouched). Markers: `scratchpad/tiling2_20260911/DONE` in, `BUILDING` gone.
  Report `scratchpad/lane_tiling2_report.md` (all ten sections 0..9); entry text
  `scratchpad/tiling2_20260911/WW_CHANGES_ENTRY.md`; **six MISTAKES entries NOT
  appended by the lane** — `scratchpad/tiling2_20260911/MISTAKES_ENTRIES.md`,
  the director splices. **The contract IS amended in place** because the brief
  asked for it: `docs/LODGEN_TERRAIN_VT.md` **§2.5a** (new) plus a TILING2
  provenance block at the end, LF-only, anchors re-found. ONE build (21:47:49,
  21,465,600 B) plus **ONE counted relink** (21:52:22, the shipped exe) — the
  relink's reason is in the report's section 5 and in MISTAKES entry 1.
  **bungo's open NifSkope window needs a restart.** No NifSkope GUI was launched
  outside the harness chain; `Fallout4.exe` was checked down before every link
  and every bake; every bake went into `scratchpad/tiling2_20260911/out/` only,
  two 4x4-cell regions, never his installed `Data\Terrain`.

  **NO DEFAULT CHANGED.** A bake with this exe and no new flags is the rung's
  bytes: 18 files over both test tiles, **0 differ**, and the same with
  `--land-sample footprint --blend-edges off` typed out. The compare is shown
  able to fail (each switch moves exactly 6 of the 18 — the chunk colour DDS and
  the two `.lodt` containers, 3 a tile). `_msn`, `_data`, `.lodm`, BTO, BTR and
  the manifest are byte-identical at EVERY setting including `--land-tiling
  2048`. `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` is deliberately
  untouched: nothing bungo bakes moves.

  **FOUR NEW FLAGS, all opt-in.** `--land-sample footprint|average` (default
  `footprint`) — `average` reads the landscape diffuse's 1x1 mip, which IS the
  exact average over one repeat (a landscape texture ships a full mip chain to
  1x1 and one repeat is the whole texture). `--land-detail 0..1` (default 0)
  lerps back k of the footprint sample's departure from that average.
  `--blend-edges off|quadrant` (default `off`) cross-fades the neighbouring
  quadrant's composite over `--blend-margin` world units either side of every
  2,048-unit line, quintic, exactly 0.5 ON the line. `--blend-margin` (default
  128 = 4 texels, the 17x17 opacity grid's own spacing).

  **VANILLA'S LAW, measured over the 22 shipped dim-4 sheets BEFORE any code**
  (`scratchpad/tiling2_20260911/logs/t3_laws.txt`, `t3b_seam.txt`; the
  population rule — mip-3 SD >= 5.25 — was written down first). Repeat: **0 of
  22** above their own null floor, ceiling 0.264 absolute / 0.448 over the
  sheet's own floor. Edges: w50 median 4.12 over 2.50..6.50, hard-edge median
  523 of 6,000, quadrant-seam median 1.041 with a worst of 1.100. Detail:
  22.6% / 21.5% of variance finer than 4 texels, and the ceiling for "how
  different two real sheets are" is **0.670**, measured vanilla against vanilla
  on neighbouring tiles.

  **WHAT THE SWITCHES DO, on both tiles.** Repeat 1.037 -> **0.092** on
  (-20,24) (vanilla's ceiling 0.264). Quadrant seam 1.236 -> **0.955** and
  1.065 -> **0.847**, below vanilla's own median on both, at no measurable cost
  (local variance -0.9%, spectrum distance 1.227 -> 1.226, repeat +1.3%).
  `--land-sample average` ALONE makes the grid WORSE (1.236 -> 1.710): with each
  quadrant flat the only edges left ARE the lines, so the two switches are not
  independent. The 0.564 that (-20,20) still reads at that bin is proved NOT to
  be a texture repeat — with `average` the sheet is byte-identical at
  `--land-tiling 341.3333` and `2048`, and in the largest road-free window
  vanilla reads 0.245 where we read 0.376 (dimensionless 0.255 vs **0.131**).

  **THE BLUR IS REFUSED, WITH THE TABLE, AND NOTHING ROUTES TO GRADE1.** Ten
  candidates against vanilla's high-pass residual, each with its own phase twin
  as the floor: the land diffuse at the footprint mip and at mip-1, -2, +1, the
  exact footprint box mean, the fully averaged texture, VCLR, the slope from the
  shipped `_msn`, the best of eight directional shadings, and our own sheet —
  **every one |r| <= 0.006** against floors of the same size
  (`logs/t4_corr.txt`). The control that makes it mean something
  (`logs/t4b_align.txt`): the same model against OUR OWN bake reads **r =
  +0.7948 and +0.7040 with the best shift at exactly (0,0)**, row-flip control
  0.0501 / 0.0137. Vanilla's far-terrain colour was NOT produced by resampling
  the landscape textures this composite composites. VCLR reads -0.004 / -0.001
  and the best lighting azimuth 0.006, so there is nothing for a grading lane
  either. **Still red: the blur.** Local variance 4.84 against vanilla's 19.81
  at the recommended setting, 12.29 at the rung.

  **THE DETAIL TRADE, PRICED.** k = 0/0.15/0.25/0.35/0.50 gives a repeat of
  0.092/0.175/0.268/0.366/0.532 for a local variance of
  4.84/5.00/5.22/5.58/6.45. Vanilla's ceiling is crossed at **k = 0.246**,
  having recovered 0.37 of the 14.97 missing levels. The repeat and the
  texture's detail are the same signal.

  **BUNGO'S CALL, BLOCKING.** Which defaults? The recommendation with its costs
  named is `--land-sample average --land-detail 0.20 --blend-edges quadrant`
  (repeat and grid inside vanilla's law; blurrier than vanilla, which is the
  complaint that is not fixed). The alternative is `--blend-edges quadrant`
  alone: the grid goes, the repeat stays. No lane changes the look of every
  future bake on its own.

  **CHAIN, all on the new exe** (22:05..22:10, `scratchpad/tiling2_20260911/
  chain.sh`, logs `logs/c_*.log`): `lodl_open.sh` **23/0**, `lodgen_terrain.sh`
  **26/0**, `lodgen_terrain_vt.sh` **41/1** (V9b, red on the rung too),
  `lodgen_roads.sh` **11/0**, `lodgen_ground_cover.sh` **29/5** (the same five
  by name), `lodgen_terrain_pbrm.sh` **14/0**, `lodgen_native.sh` **18/0**,
  `lodgen_panel_run.sh` **125/0**, `lod_generation.sh` **116/0**,
  `ui_align.sh` **11/0**, `water_ui.sh` **82/0**. Every count is ROADS2's
  baseline exactly; nothing moved, nothing to explain.

  **FOR ROADS3** (the director's addendum, measurement only — no road code was
  touched): the road edge on (-20,20) is a **contrast** defect, not a width
  defect. Our road crosses in **6.00 texels** (10-90%), vanilla's in 6.00, w90
  12 against 11 — but ours stands **+29.45** luminance levels over its surround
  where vanilla's stands **+3.92**: vanilla's far-LOD road is a ~2-level rise
  over twelve texels, and its width row is read off a 23.26-level profile that
  is the terrain's own texture under the mask, not a road edge. **Do not chase
  all 25 levels with the road pass**: our non-road surround is 69.41 where
  vanilla's is 88.57, so 19 of them are a whole-sheet tone offset that belongs
  to GRADE1. Mask and profiles: `logs/t3c_road.txt`, `road_mask_t2020.npy`.
  **ROADS2's standing note still stands**: if a default ever changes, the ground
  moves, so `lodgen_roads.sh` R5 bar 2 and the four-variant composite ranking
  need re-running. They are green on the defaults as they are.

  **KNOWN LIMITATION, named not hidden**: a quadrant border lying on the chunk's
  own edge is not cross-faded (the neighbouring cell's paint is not loaded in the
  tile baker). The adjacent chunk does not blend it from its side either, so no
  new seam is created — but that one line stays as hard as it is today. All
  seven interior lines per axis are blended.

  **SKILLS AMENDED IN BOTH TREES** (`E:\Projects\NifskopeWildWastelandEdition\
  .claude\skills` and the live mirror `E:\Projects\Claude\.claude\skills`):
  the pyramid owns the colour sheet, so a colour change has two sites and the
  one that ships is the pyramid; the phase twin is a floor for a STRUCTURE
  statistic only and never for a periodicity or a spectrum; a zero correlation
  proves nothing without an alignment control that reads ~0.8 against a field
  the model does reproduce; and a crop's own null floor is higher than a
  sheet's, so a picture's verdict number must be the whole sheet's.
