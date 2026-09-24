- GRADE1 DONE 2026-09-12 03:0x, EXE FREE (release/NifSkope.exe 03:06:21,
  21,489,152 B, md5 6af74b4b4667ce50c4506a2d42a04fdf; rung
  release/NifSkope.before_grade1.exe 02:08:57, 21,487,616 B, md5
  8a1a1e718c6d6822ad0d60b90803fd69 = TILING4's DONE exe, intact;
  scratchpad/lane_grade1_report.md, HANDOFF_BLOCK + WW_CHANGES_ENTRY +
  MISTAKES_ENTRIES in scratchpad/grade1_20260911/). THE ANSWER IS THAT THERE
  IS NO TONE CURVE. The transfer ours -> vanilla has no constant: the best
  gain is 0.8916 on (-20,24) (we are too BRIGHT there, +6.79 levels) and
  1.1612 on (-20,20)'s ground (too dark, -14.70) -- opposite signs one chunk
  apart. 25-tile census k = 0.615..1.241 mean 0.892 sd 0.144; 96 land cells
  0.699..1.388 mean 1.004. Both fixed sRGB slips refuted by 2 orders (RMS
  56.6/78.5 and 68.6/59.7 vs identity 19.9/22.7) and the colour path has no
  conversion to slip. Affine/gamma "win" degenerately (slopes 0.019/0.185,
  their RMS = vanilla's own sd) because our sheet and vanilla's barely
  correlate texel to texel (r 0.034 / 0.314). VCLR is dead for tone (mean
  multiplier 254.9/255 on six tiles; removing it moves k by 0.0003).
  FINGERPRINT: a per-cell CONTENT difference with a near-zero mean -- not an
  exposure, a gamma, a colour-space slip or a lighting term; the residual's
  only sign-consistent partner is our OWN luminance (r +0.835/+0.827 vs phase
  twins -0.056/+0.012), height sits AT its twin floor on both tiles.
  fo4-engine-constant-from-ini-setting NOT invoked: its precondition
  (position-independent gain) fails. SHIPPED: `--grade K`, both writers,
  after road+tint before the crevice term, clamped 0..4, default 1.0 with the
  multiply BRANCHED OVER -- off == the rung's bytes by construction and
  measured (24/24 files identical, no-flag and --grade 1.0). GATE G3's
  "reduced on BOTH tiles" REFUSED WITH ARITHMETIC per the skill (the error in
  k is a parabola whose vertex is that tile's own k_opt; the two straddle 1),
  the refusal asserted against the BINARY: pooled k=0.8403 gives (-20,24)
  20.127 -> 18.108 and (-20,20) 21.897 -> 28.488. Own-optimum k does reduce
  each (-12.8 % and -6.2 %); binary vs prediction within +0.031..+0.173 of a
  1.0-level tolerance; rung exe exits 2 on --grade. Gates: G1 controls 24/0,
  ship gates S1-S5 8/0. HARNESSES at ROADS2's baselines exactly: lodgen_terrain
  26/0, lodgen_terrain_vt 41/1, lodgen_roads 11/0, lodgen_ground_cover 29/5,
  lodl_open 23/0, lod_generation 116/0, lodgen_native 18/0,
  lodgen_terrain_pbrm 14/0. Contract amended: new 2.5f, step 8 of the 2.5
  ring-0 formula (so FO4CS blends the same tone; at 1.0 the step does not
  exist), --grade in 5, GRADE1 provenance re-stamped. Pictures
  scratchpad/grade1_20260911/images/{cmp_tone,curve}.png. RESTART: yes (any
  open window predates 03:06:21).
  REDS FOR BUNGO / OTHER LANES: (1) the ground-cover plane is EMPTY on all 25
  census tiles including Sanctuary -- every _data sheet carries dwReserved1 = 0
  (set exactly when coverMax == 0) and its decoded alpha is 0 on 100 % of
  texels, so the grass tint NEVER FIRES and 0.35 could not be fitted; that is
  a cover-source defect, not a tint defect. (2) two census tiles are
  near-achromatic and over-bright -- (-12,28) sat 0.054 lum 124.3 and (-16,28)
  sat 0.018 lum 126.8, RGB RMS 51.2 and 48.6 against a ~20 typical: they look
  like a missing/greyscale base texture. (3) the road materials do not resolve
  from the unpacked data root (materials/c:/projects/fallout4/... is not in
  any archive), so the road colour is whatever the fallback is. (4) we are
  more saturated than vanilla on every tile measured (0.243 vs 0.219 mean) and
  no brightness model can explain it -- a gain leaves S unchanged; that is the
  next question and it is a CONTENT question, for the lane that owns the layer
  blend. (5) THE RECORD IS CORRECTED: "the off-road ground is ~16 levels too
  dark" is true of (-20,20) only; it reverses sign on (-20,24). Of the road's
  ~21 levels of over-contrast on (-20,20), 6.3 are the road being too bright
  and 14.7 are the ground being too dark.
