- ROADS1 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  12:19:06, 21,180,928 B** (TERRAIN-R's was 11:27:12, 21,137,920). Rollback rung
  `release/NifSkope.before_roads1.exe` (12:11:51, 21,137,920 — sha1 `a3283ae6…`,
  equal to the launch exe byte for byte). Markers:
  `scratchpad/roads1_20260911/DONE` in, `BUILDING` gone. Report
  `scratchpad/lane_roads1_report.md`; entry text
  `scratchpad/roads1_20260911/WW_CHANGES_ENTRY.md`; **three MISTAKES entries NOT
  appended by the lane** — `scratchpad/roads1_20260911/MISTAKES_ENTRIES.md`, the
  director splices. ONE build (12:12) plus **ONE counted relink** (12:19),
  declared in the report's section 3.1 with what the relink fixed.

  **WHAT VANILLA WAS MEASURED TO DO, BEFORE ANY CODE.** On Bethesda's own
  `Commonwealth.4.-20.20.DDS` — and that tile matters, see the correction below.
  (1) The road is NOT in the LAND paint: all sixteen cells have a LAND record and
  none of the sixteen painted landscape textures is a road, asphalt, concrete or
  pavement. (2) It IS at the road meshes' footprint and at no other family's:
  road AUC **0.716** bright / **0.678** grey against its own displaced floor
  (0.470–0.601 / 0.448–0.513), while trees 0.529, rocks 0.448, architecture
  0.621, set dressing 0.560 and every generic `bDecal` shape 0.564 all stay
  inside their own floors; ceiling 1.000. (3) The colour is the road material's
  own diffuse under the sheet's own grading — ×0.82 on the road,
  ×0.83 on the background, the same factor. (4) **The `_msn` does NOT carry it**:
  vanilla's `_msn` against a normal from the LAND heightmap alone disagrees by
  13.58° on the road and 14.14° on the background (control 14.45°).

  **WHAT SHIPPED.** Road meshes are scan-converted top-down into the far-terrain
  **colour sheet only**, both bake paths, between the VCLR multiply and the grass
  tint; the cover byte under a road is scaled by
  `1 − coverage × --road-cover-suppress` (default 1). Topmost triangle wins;
  colour = the shape's diffuse at the interpolated UV × vertex colour, mip from
  the triangle's own UV-area ratio; opaque shapes cover fully and only a shape
  whose MATERIAL or `NiAlphaProperty` says so honours the texture's alpha.
  Normal, height, roughness, metallic and emissive sheets untouched. The mesh
  test is component equality — `STAT` + `landscape`/`roads`|`sidewalks` + a
  component after — never a substring (`SetDressing\RailRoad\WaxCandle02Off.nif`
  is placed twelve times in Sanctuary). `--roads` / `--no-roads` (**on by default
  under both targets**) and `--road-cover-suppress F`.

  **GATES.** New `lodgen_roads.sh` **11/0 PASS**. Baselines all held:
  `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1 (V9b, red on the rung
  too), `lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0,
  `lodgen_texture_arrays.sh` PASS, `lodgen_card_arrays.sh` PASS,
  `lodgen_native.sh` 18/0, `lodl_open.sh` 23/0, `ui_align.sh` 11/0,
  `water_ui.sh` 82/0. `--no-roads` vs the rung exe: **9 of 9 files identical**.
  Road-presence metric, mask taken from vanilla's own sheet by colour and the
  extractor controlled first (51.1 % inside the independent geometric projection
  against 3.5–16.3 % displaced): **ceiling 1.0000, floor 0.1274, after 0.3065**,
  surrounding ground 0.3442; pre-registered bars ≥2× floor and ≥0.8× ground, both
  met. Whole-tile mean error 24.39 → **22.81**, on the road centreline
  **38.85 → 24.36**. Census: 253 placements / 73 meshes / 88,513 triangles /
  **27,695 texels** on the loop-road region, **0 texels** on the road-free one.
  Consistency: 3 of 3 changed sources older than the exe, both objects rebuilt,
  `res/style.qss` and `release/style.qss` byte-identical.

  **A CORRECTION TO THIS HANDOFF.** TERRAIN-R's block and
  `docs/LODGEN_PARITY.md` said of chunk (-20,24) that "most of" its 19.96 mean
  difference "is the roads". **Chunk (-20,24) contains zero road triangles** —
  86,770 are in (-20,20), 63,703 in (-16,24), 284,532 in (-16,16). The 19.96 is
  real and its cause is the splat grading, the next line in the same gap list.
  ROADS1 re-took the gate on (-20,20). MISTAKES entry written.

  **TWO CALLS FOR BUNGO.** (1) **The shared material resolver.** Every
  `Landscape\Roads\Country\*` and `\Alley\*` piece names its material as an
  absolute Bethesda build path and carries an empty texture set;
  `lodgenLoadModel` prepends `materials/` and resolves nothing, so 65 of 270 road
  shapes had no diffuse and every decal among them was invisible. The ROAD pass
  fixes it for itself (`lodgenRoadMaterialPath`, keying on the last
  `materials/`); the OBJECT bakes still drop those textures, and fixing it in the
  shared loader would move output the byte-identity gates pin. Yes or no.
  (2) **`--road-cover-suppress` default 1.0** — no grass and no grass tint under
  a road. Nothing was measured about vanilla here, because vanilla ships no cover
  plane to measure. 0.0 is the exact way back.

  **THREE REDS / OWED.** (a) Our road reads lighter and less blue than
  Bethesda's — the same splat-grading gap as the ground around it, which is why
  its error now matches the background's (24.36 vs 24.4) rather than beating it.
  (b) **Our far-terrain sheets are DXT1 with 8 mips; all 6,120 of Bethesda's
  Commonwealth sheets are DXT5 with 10** (3,060 colour + 3,060 `_msn`, measured
  over the whole folder). A DXT1 `_msn` has no alpha channel; this is what makes
  our chunk render blue-purple beside vanilla's in `top_region.png`. A lane of
  its own. (c) `Landscape\Sidewalks\*` is carried by the rule and is untested:
  187 texels against `Landscape\Roads`' 23,170 on the measurement tile.

  **PICTURES.** `scratchpad/roads1_20260911/images/cmp_sanctuary_road.png`
  (vanilla | ours without | ours with, the same 512 texels at the same 32 units,
  plus 4× zooms of the cul-de-sac), `road_mask_and_metric.png` (the gate's mask
  over all three, its own control burned in), `top_region.png` (the region from
  above through the render hook, each side staged as its own data root).

  **NEW FILES.** `tests/spells/lodgen_roads.sh`,
  `tests/spells/lodgen_roads_metric.py`. **TWO SKILLS AMENDED IN THE REPO TREE,
  the director mirrors them to the live tree:**
  `nifskope-ww-vanilla-compare` gains step 1a (pick the tile by PROJECTING the
  thing under test — the histogram that found (-20,24) empty), and
  `nifskope-ww-lodgen` gains the trap that a spell shelling out to `python`
  measures whichever `python` is on the PATH (the MSYS2 one has no `numpy`, and
  `lodl_open.sh` read 23/1 with two EMPTY numbers because of it before reading
  23/0 from Git Bash).

  **RESTART: YES.** Anyone holding a NifSkope window from before 12:19:06 needs
  to restart it. No NifSkope was running at any point during this lane except its
  own three headless renders and the GUI harnesses, one at a time, on the second
  monitor; none is running at its close; the game was down at every launch.
