- TERRAIN-R LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  11:27:12, 21,137,920 B** (NATIVE1b's was 10:14:23, 21,101,056). Rollback rung
  `release/NifSkope.before_terrain_r.exe` (10:39:04, 21,101,056 — equal to the
  launch exe). Markers: `scratchpad/terrain_r_20260911/DONE` in, `BUILDING`
  gone. Report `scratchpad/lane_terrain_r_report.md`; entry text
  `scratchpad/terrain_r_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries
  NOT appended by the lane** — `scratchpad/terrain_r_20260911/MISTAKES_ENTRIES.md`,
  the director splices. ONE build (11:01:06) plus **TWO counted relinks**
  (11:23:53, 11:27:12), both declared in the report's section 5.

  **WHAT SHIPPED.** The `.lodt` container is **version 2** and its sheets are the
  OBJECT texture family (bungo 09:5x). Role 3 `data` (AO / wetness / shore /
  cover) is RETIRED and refused by name; **role 5 `mask`** carries `rmaos` —
  R roughness, G metallic, B AO, A ground cover — and **role 6 `emissive`** is
  written only when a layer's material supplies one. Shore proximity and wetness
  are dropped from the container (shore = a runtime subtraction from the `.lodl`
  water planes; far wetness = a weather state). The `.btr` chunk `_data.DDS` on
  the stock path is UNCHANGED and still byte-identical with the rung. `family` in
  the terrain index is **`"pbr"` and it means it**, with a per-layer rule census
  (`maskRules`) so the word is auditable. A v1 file is refused, not converted;
  bungo's installed `Data\Terrain` was listed READ-ONLY and holds no `.lodt`.

  **THE RING-0 GATE HE ASKED FOR (09:2x) IS DELIVERED.** The contract states the
  runtime blend's seven steps once (`docs/LODGEN_TERRAIN_VT.md` §2.5) and
  `tests/spells/lodgen_terrain_model.py ring0` implements them independently
  (own ESM walk, own BC1/BC3/**BC5U** decoding, own mip choice): two Sanctuary
  tiles at **mean 3.26 / 3.59, p95 8, max 14 / 17** of 255, floor (weights
  ignored) **13.70 / 15.15** = 4.2x, ceiling **0** over 73,984 texels.

  **GATES.** `lodgen_terrain_vt.sh` **41/1** (rung 35/1; +6 new green, the one
  failure is V9b, red on the rung too), `lodgen_ground_cover.sh` **29/5**
  (identical to the rung), `lodgen_terrain.sh` 26/0, `lodgen_native.sh` 18/0,
  `lodgen_card_arrays.sh` 35 ok PASS, `lodgen_texture_arrays.sh` 40 ok PASS,
  `lodl_open.sh` 23/0, `ui_align.sh` 11/0, `water_ui.sh` 82/0 (floor 72 — the
  brief's "86" was never this exe's number), and NEW
  **`lodgen_terrain_pbrm.sh` 14/0**. Identity: the stock path (6 files) and the
  object arrays bake (12 files) are byte-identical with the rung. Consistency:
  7 of 7 changed sources older than the exe, 11 of 11 dependent objects rebuilt,
  `res/style.qss` and `release/style.qss` byte-identical.

  **TWO CALLS FOR BUNGO.** (1) **Where the ground cover lives.** Shipped in the
  mask's alpha; `--vt-cover-in-color` reaches the other arm. Measured: the two
  cost **exactly the same bytes** (the format is per-tile by the COVER bit), and
  the stock engine tolerates a BC3 colour sheet either way (**2,001 of 2,001** of
  vanilla's own chunk colour sheets are DXT5). The only discriminator is meaning
  — the colour sheet's alpha is the slot `.lodm` §2.1 calls OPACITY and tells a
  consumer to alpha-test. (2) **The roads.**
  `scratchpad/terrain_r_20260911/images/ours_vs_vanilla_tile.png` puts our tile
  beside Bethesda's own sheet for the same ground at the same density: mean
  difference 19.96/255, and most of it is the roads and a rubble patch vanilla
  carries and we do not. That is lane ROADS1, and this is the case for running it
  before he bakes.

  **FOUR REDS / OWED.** (a) V9b, the assembled-vs-direct `_msn` byte identity the
  contract §2.4 claims — red on the rung, untouched here, nobody has measured
  whether the claim or the code is wrong. (b) **The resource stack cannot see a
  `.pbrm` OR a `.lodm`**: `BA2File`'s loose-file whitelist
  (`lib/libfo76utils/src/ba2file.cpp`) has neither extension, so `--resource`
  silently ignores both and `lodgen.h`'s own "consulted FIRST ... and .lodm
  alike" is not true today. Two lines in a vendored library; not taken here.
  (c) The object bake does not yet CONSUME the shared mask resolver — the law has
  one home and the gloss is genuinely shared, but the object path still takes its
  PBR answer from a `.lodm` sidecar, so a PBRM model bakes objects legacy and
  terrain PBR. (d) The render-hook top view of the region is owed; the four
  texel-level pictures are in `scratchpad/terrain_r_20260911/images/`.

  **NEW FILES.** `tests/spells/lodgen_terrain_model.py` (the independent terrain
  model: `mask` = T1/T2, `ring0` = T4), `tests/spells/lodgen_terrain_pbrm.sh` and
  `tests/spells/lodgen_terrain_pbrm_fixture.py` (T2/T3 both ways on a fixture),
  and the skill `.claude/skills/ww-corpus-absent-fixture/SKILL.md` — **written to
  the REPO tree, the director mirrors it to the live tree.**

  **RESTART: YES.** Anyone holding a NifSkope window from before 11:27:12 needs
  to restart it. No NifSkope was running at any point during this lane and none
  is running at its close; the game was down at every launch.
