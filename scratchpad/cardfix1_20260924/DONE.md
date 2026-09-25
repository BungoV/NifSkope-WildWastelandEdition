PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps. Steps 1-6 landed (6b, 6c after). Step 7
(IMPOSTORPBRM1) is BUILT and committed (exe 45719ad4). The director decided both run-3 reds (2026-09-25,
director decisions, not rulings by bungo). Gate run 4 on those bars: 13/14.
LEFT: one row. The non-aa arm, judged on fully covered texels as decided, still fails 4 rows of the
tree-animated material (roughness, metallic, sqrtF0.R, weight: 0.872-0.896 within 2 against 0.90; medians
exact). 98 % of the misses sit beside a texel of the OTHER material: they are texels where the two
materials meet, not partial coverage. Away from the other material the share is 0.997. The fully covered
population keeps only 13781 of that material's texels (the aa arm has 132925), so the boundary share
rises from 2.6 % to 12.2 %. All the breakages still fail on this population, so the decision's drop
clause did not fire. The row is neither passing nor dropped: the director's call (section 2, step 7,
"Run 4"). No bar was changed after run 4.
# 1. Skills loaded
nifskope-ww-worktree-build, nifskope-ww-build-verify, nifskope-ww-lodgen, nifskope-ww-render-shot,
ww-test-harness-add, search-lean (the common rules' list, loaded with the Skill tool before any work).

# 2. What was built, step by step

## Step 1 -- IMPOSTORDEPTH2 landed (code was already in the branch point)
- IMPOSTORDEPTH2's code is in the tree at 71f96c1 (commit 85c0b14 carried it in); nothing of it was
  re-applied. This step re-ran its gates on this worktree's exe and drops the two stale comments that
  said the tree has no BC7 encoder:
  - src/lodgen.h (the `_msn` cache note): now says the encoder exists (src/lodgenbc7.h, used for the
    card `_n` sheets) and that this cache does not use it -- BC7 in the card cache stays OFF (R9 "No").
  - src/nifcli.cpp (the cache-verify comment): "BC7 (src/lodgenbc7.h) is not used here."
  Comments only; no behaviour moved.
- lodgen_defaults.sh leg (d) (director's add, VTFIX1's red "no impostor-card lines with identity off"):
  NOT a generator defect. The spell's default card dir is repo-relative and git carries only the 24 `.txt`
  sidecars of that card set (the PNGs are untracked, main tree only), so in any WORKTREE the bake finds no
  card image and places no `C` line. Measured on the rung exe with CARDS = the main tree's
  showcase1_20260912/cards: d_new manifest rows 7626, C 7210, I 35. FIXED CHEAP in the spell: leg (d) now
  refuses BY NAME ("the card directory holds no card image ... set CARDS=") instead of reading as a
  generator failure. Kept-green runs pass CARDS=<main tree>/scratchpad/showcase1_20260912/cards.

## Step 2 -- the three "empty" models (Sapling01, TreeElmUndergrowth01, ShrubGroupLarge05)
- Diagnosis: ONE cause, already repaired before this lane. Every BSMeshLODTriShape of these models ships
  its LOD0 slot empty (0/n/m); the bake used to keep LOD0 only and so drew 0 triangles. IMPOSTORSHRUB1's
  per-model rule (keep the lowest slot any such shape of the model fills, sidecar `rangekept LOD<k>`) is
  in the branch point, so this worktree's rung exe ALREADY bakes all three. The brief's red ("the rung exe
  bakes them empty") therefore cannot be the rung; the red is the exe from before SHRUB1
  (main release/NifSkope.before_impostorshrub1.exe, 09-23 03:01, copied as release/NifSkope.pre_shrub1.exe).
- No code change in this step. Evidence script: step2.sh (impostor_shrubs.sh with MATCH per model).

## Step 3 -- IMPOSTORFIX5's owed re-runs
- impostor_draw.sh: re-run in step 1 on this lane's exe, 33 steps / 1 failure (row 5, the known red).
- cardres_test.sh (FIX5's frame-resolution discriminator, re-pointed at this worktree): run on exe ff86b488.
  A measurement, not a gate (FIX5 set no bar; the ruling on card resolution is bungo's).
- SHRUB1's "texel islands at el 20" (cedar01/02, hollyshrub01prewar): NOT measured -- not cheap (needs a new
  picture-based instrument). Step 4 below takes the stipple out of the DEFAULT draw by name, which is the
  class SHRUB1 suspected; if islands remain at the crisp end they are the frame's own coverage.

## Step 4 -- R5 defaults: N8, the crisp cut, the slider at the crisp end
- Checked first: N8 (the bake driver's OCT default, the panel's Card frames default) and the slider at
  the crisp end (frameCount 1, flat) were ALREADY the shipped defaults (IMPOSTORDEPTH2). Nothing moved there.
- The crisp cut is now the default BY NAME: `Resolved::cutRule`, which is 2 (the strongest frame) whenever
  one frame is drawn, and the Options cut otherwise; the shader's `cutRule` uniform reads the resolved rule
  (src/gl/impostordraw.h/.cpp). Before, the crisp end drew under the stipple rule and was crisp only because
  one frame at weight 1 happens to cut where that frame does. No pixel moves (D5, D7).
- The preview harness names the resolved cut in its log (src/impostorpreviewtest.cpp).
- New gate tests/spells/impostor_defaults.sh, rows D1-D7.

## Step 5 -- IMPOSTORRING1: a horizon ring card set (16 views x 1 row)
- `WW_IMPOSTOR_RING=V` bakes V views evenly around the horizon at elevation 0 into a V x 1 sheet (frame v
  at x = v*tw; eye (cos p, sin p, 0), right (-sin p, cos p, 0), up z). The sidecar says `ring V tw th` and
  echoes one `ringview v az el` per frame, which the gate reads back against the law (src/nifskope_ui.cpp).
- lodgen carries a ring set as `views` V / `grid` [V,1] with NO `oct` key and frameOffset 2V
  (src/lodgen.cpp card region; docs/LODGEN_LODM_FORMAT.md 3.2). The drawer picks the nearest azimuth frame
  (src/impostorcard.*, src/gl/impostordraw.cpp). An old exe refuses a ring set by name ("oct is 0, outside").
- Why 16 x 1: a tree seen from LOD distance is seen from within a few degrees of the horizon; a ring spends
  every texel there, and 16 x 1 at tile 256 is a quarter of the N8 sheet's pixels.
- The preview harness gained WW_IMPOSTOR_ORBIT_SELECT (src/impostorpreviewtest.cpp); the bake driver
  passes WW_IMPOSTOR_RING through and defaults the tree run to 16 (tools/bake_impostor_cards.sh); the
  FO4CS reader is owed (spec text only).
- FINDING for bungo: N8 is BETTER than the 16-view ring at every elevation measured, including the horizon.
  N8 already has 28 frames near the horizon (largest step 16.2 degrees), so the ring buys pixels, not shape.
  The ring sits at its own ceiling (the mesh against itself rotated by half a step). The bake driver's
  TREE run defaults to RING=16 as ruled (bungo 2026-09-23 04:4x, "22.5 degrees per take"; RING=0 = the
  N8 grid, the empty-slot run keeps the grid); the numbers above argue for his second look.
- Owed: lodgenaggregate learning the ring (refused by name today); the panel's Card frames row and
  cardsOnDisk ignore ring sets; the FO4CS reader.

## Step 6 -- IMPOSTORWIND1 job 3: sway A (the tree's OWN wind weights in the card)
- The bake reads the model's raw vertex alpha (the only wind input the game's tree shader reads) through
  channel 11 G, only on shapes with the tree-animation flag (0 elsewhere): res/shaders/fo4_default.vert/.frag,
  the raw vertex alpha pass only. The card hook writes `_n.A = W x h` (W that weight, h linear from the
  view's own coverage bottom row) on a model with a tree-animation shape, else the synthetic
  h^2 x (0.35 + 0.65 r) byte for byte; the sidecar says `sway model|synthetic` (src/nifskope_ui.cpp).
- lodgen writes `lodm` 2 on a card / card array carrying model sway, with `sway` "model" and the base's own
  `leafAmplitude` / `leafFrequency` (arrays: parallel lists); everything else stays 1 (src/lodgen.cpp card
  regions). The reader accepts 2 on the card family only and refuses a source claiming 2 by name
  (src/io/lodmfile.cpp). Contract: docs/LODGEN_LODM_FORMAT.md 3.3, LODGEN_IMPOSTOR_SPEC.md (owed item 6),
  LODGEN_CARD_SHEETS.md.
- The preview harness takes WW_IMPOSTOR_SWAY_AMP / WW_IMPOSTOR_SWAY_PHASE (src/impostorpreviewtest.cpp).
- A STEP-5 DEFECT found here and fixed (fix24): a ring card array's file name took the group key's `|`
  ("could not write ...legacy.2304x256|ring_d.DDS"); now `...2304x256.ring_d.DDS`. No step-5 gate had put a
  ring set through --arrays; the wind gate's G3 does.
- New gate tests/spells/impostor_wind.sh (+ impostor_wind.py, and impostor_wind_nif.py: an independent
  rasteriser of the NIF's own per-vertex wind weights, sharing no code with the bake).
- The GIF (brief): scratchpad/cardfix1_20260924/wind/gif/elm_sway.gif, "3D model" | "Octahedral impostor",
  12 phases. NifSkope does not animate tree wind on the mesh, so the left panel is STATIC; the right panel is
  the drawer's sway shear driven by the baked weight. It shows the weight is where the tree moves, not how the
  game animates it. (untracked: binaries stay out of the public repo)
- lodgen_octahedral.sh's "lodm 1 card" check was stale (its fixture is a tree, so its card is now lodm 2): it
  now ties the version to the sidecar's sway line (fix27).

## Step 6b -- the N8 grid is the bake default (bungo RULED 2026-09-25)
- bungo, 2026-09-25, verbatim (relayed by the director): **"Yes, 8x8 is the default choice for a bake"**.
  It replaces his 2026-09-23 ring default for tree bakes, after step 5 measured N8 better than the ring at
  every elevation, the horizon included.
- tools/bake_impostor_cards.sh: RING defaults to 0 (the N8 grid) for every run, trees included; RING=16
  still bakes the ring, anything else is refused by name. The panel has no ring path (the only
  WW_IMPOSTOR_RING reader is the bake hook), so nothing changed there. docs/LODGEN_LODM_FORMAT.md 3.2 and
  docs/LODGEN_CARD_SHEETS.md say the ring is an option. No exe change (script and docs only).
- New gate row R7 in tests/spells/impostor_ring.sh: it runs the driver itself with MAX=0 (nothing is
  photographed) and reads its library.txt.

## Step 6c -- G4 re-pinned: DIRECTOR DECISION (a), 2026-09-25 (not a ruling by bungo)
- The director's decision, with its reasoning: re-pin G4's bar to the codec floor measured on the SAME
  sheet (its normal R/G channels, 3.266 / 12 on the elm), plus the margin of the skill
  ww-preregister-bar-from-the-subject, and state it in the gate as "bar = codec floor of the sheet's other
  channels, measured". The BC7 weights are NOT changed: option (b) would move every card's normals for a
  sway error of about 1.4 % of full scale, which nobody can see. A red must still fail.
- tests/spells/impostor_wind.py/.sh (fix31_g4_repin.py): the bar is computed in the run from that run's
  normal R/G error, x 1.25 on the mean and x 1.25 rounded up on the p95. If the floor was not measured
  (empty, or a mean under 0.5), G4 fails by name; it never falls back to a constant. Two red controls must
  fail the same bar: the next frame's picture, and the sway channel corrupted to 4 bits
  (floor(A/16)*16+8, a codec about twice as coarse as BC7's own error here).
- Which check the first bar skipped (skill check 3): 3.0 / 12 was copied from the synthetic input
  (1.34 / 4) onto the real one. Red run kept: gates/impostor_wind.run3.out.

## Step 7 -- IMPOSTORPBRM1: cards from .pbrm models keep their PBR quantities

### Job 1 -- the fixture, and what the current exe bakes from it
- No `.pbrm` on disk belongs to a tree (census of 169 files: 161 v6; the only TintMask ones are the four X-01
  power-armour files in the `FO4CS TintMask Test` mod; none sets a non-default specular). So the fixture is
  MADE: `tests/spells/impostor_pbrm.py fixture <dir>` writes two v6 `.pbrm` files beside the two materials
  vanilla `TreeMapleForest1.nif` names (`materials/Landscape/Trees/MapleAtlas01.pbrm`, `MapleAtlas02_Tree.pbrm`,
  the same-name-sibling route) plus three uniform 64 x 64 maps. Base colour and normal are the vanilla maple
  maps. MapleAtlas01 takes the CONSTANT route (sparse slot values: colour #E6CCB3 multiplier, roughness 0.35,
  metallic 0.8, specular weight 0.75, specular colour #80C0FF, IOR 1.33, tint masks 0.5 / 0.2 constant, Add).
  MapleAtlas02_Tree takes the TEXTURE route (RMAOS map 0.702 / 0.2 / 1 / weight 0.502; specular-colour map
  RGB 200 150 100 with IOR 2.0 in its alpha over iorMax 4.25; a tint-mask map 0.6 / 0.3 / 0.2 / 0.1 whose sum
  1.2 makes Normalize bite). Nothing binary is committed; the script regenerates it.
- Baked on the current exe 309f3aa9 (N8, TILE 256, `scratchpad/cardfix1_20260924/pbrm/job1.sh`):
  **family legacy; the third sheet is `_gsaos`; no specular sheet.** Class 0 (96,860 texels): albedo
  113.8 101.3 77.9, gsaos R (gloss) 80.8, G (specular) 18.7, B (AO) 238.7, emissive 0. Class 1 (132,771):
  albedo 109.9 97.5 82.4, gloss 80.4, specular 14.2, AO 221.0, emissive 0. Those are the VANILLA material's
  numbers: the bake never opens a `.pbrm` (the sidecar says `lodm ... none` for both shapes), so the fixture's
  roughness, metallic, specular colour, IOR and tint are all absent from the card.

### Design (written before the code; the director relays before a format ships)
1. **Family.** A textured shape is pbr-sourced when it carries a pbr source `.lodm` (as today) or resolves a
   `.pbrm` through the viewport's one resolver, `io/pbrmresolve` (the direct `.pbrm` name, else the
   same-name sibling of the `.bgsm`/`.bgem`, else the diffuse stem), reading the loose root
   `WW_LODGEN_DATA_ROOT` first and then the resource stack -- the mask path's order (src/lodgen.cpp
   ~1849-1869). A shape with both keeps its `.lodm` (the explicit LOD source wins). The card is family pbr
   when EVERY textured shape is pbr-sourced. The sidecar gets one `pbrm <path> <route> tree <0|1>` line per
   shape that resolved one.
2. **Mixed models** (some shapes `.pbrm`, some not): **legacy, exactly as today**, and the sidecar names the
   `.pbrm` shapes whose data was not used. A legacy card describes the vanilla materials; turning a `.pbrm`
   into gloss/specular would lose metallic. The alternative is offered, not built: a pbr card whose legacy
   shapes are converted by the mesh-LOD mask path's own law (roughness = 1 - gloss, metallic 0, bungo's
   metallic ruling).
3. **How the bake reads it: no shader change.** For each pbrm shape the bake evaluates the `.pbrm` law in
   TEXTURE space on the CPU and writes the results as uncompressed DDS files with box-filtered mips into a
   per-bake folder that it registers as a resource root; the existing retarget hook
   (`wwTextureOverride`) points the shape's slots at them, and the existing channels photograph them raw in
   the same 4x offscreen bake. `fo4_default.frag` is untouched.
   - slot 0 (colour): sRGB( decode(map) x colour x tintMix ), alpha = the opacity law (the map's A when
     `overrideOpacity` is off, else the constant). The tint is applied HERE, so `_bc` holds the tinted colour.
   - slot 1 (normal): the `.pbrm`'s normal map as it is (a `strength` other than 1 is not applied: stated).
   - slot 7 (RMAOS): R roughness, G metallic, B AO -- the map's channel while its override is off, else the
     constant.
   - slot 2 (emissive): sRGB colour x mask, and the card's `emissiveScale` = luminance / 100 (100 nits = 1.0,
     the PBRM alignment's convention); luminance 0 = black.
   - specular: two extra channel-10 passes per view with slot 7 pointed at the specular source, only when a
     shape departs from the default specular.
4. **Specular weight, colour and IOR -> one new sheet `_s`: RGB = sqrt(F0'), A = specular weight.
   RECOMMENDED over a sheet of raw colour + IOR.** F0' = clamp(weight x colour x ((ior-1)/(ior+1))^2) is
   everything a dielectric's lobe uses in the PBRM law (the editor's preview and NifSkope's viewport;
   F90' = sat(50 F0') is derived; OpenPBR's remap ior' = (1 + sqrt F0') / (1 - sqrt F0')), so folding loses
   nothing for a dielectric, and F0 averages linearly through the 4x downsample and the mips, where an IOR
   does not. sqrt is the Fresnel amplitude: the default F0 0.04 sits at level 51 and one level is 3.9 % of
   it (a linear F0 would be 10 % a level). A = the weight, because a METAL's lobe is weight x F82(base,
   colour): the metal keeps its weight and loses only the colour as its F82 edge tint, which FO4CS does not
   read yet (PBRM-v6.md:245, dielectrics only). Precision cost: `_s` is BC7 at the aux size, the same bytes
   as `_n`; BC1 was rejected (5:6:5 endpoints = about 8 % of F0 a step at 0.04, no alpha). Written only when a
   shape departs from the default (weight 1, white, IOR 1.5); no `_s` = F0 0.04, weight 1. The `.lodm` gets a
   new texture key `specular` on family pbr cards. A reader ignores keys it does not know (LODGEN_LODM_FORMAT
   section 2) and no existing key changes meaning, so **no version bump**. Coat, fuzz, transmission, thin film,
   subsurface colour: dropped -- no LOD reader for them, sub-pixel at LOD distance, each would be another
   sheet (LODGEN_IMPOSTOR_SPEC.md "Ours, for LOD" gets this list).
5. **Tint masks against Warframe's published TennoGen rules (a finding; the editor is not changed).**
   DE's texturing guide: the mask is its own map, R/G/B/A = tints 0-3; the base is authored GREY (non-metal
   about 128, metal about 186) and the tint supplies the colour; unmasked texels stay grey; the masks are
   STACKED layers (G over R, B over G, A over everything) and authors extend a lower mask under the upper
   one on purpose. DE publishes no blend formula (multiply on a grey base is the likely reading). Ours:
   the base keeps its authored colour and is multiplied; unmasked = unchanged; Normalize / Add SUM
   overlapping masks, so Warframe-style overlapped masks mix colours instead of the upper winning; only
   "Priority RGBA" stacks, and in the opposite order (our R wins, Warframe's A wins). Warframe's energy
   (emissive) colour has no tint-mask equivalent here. Sources: warframe.com/en/steamworkshop/texturing-guide,
   basic-art-guide, operator-accessories.
6. **Per-reference tint in FO4 data: it exists, by material swap.** A placed reference can carry XMSP
   (a Material Swap, MSWP), whose substitutions each carry a Color Remapping Index (CNAM float); models carry
   the same index (MODC). The maple itself has swap materials on disk (MapleInstituteAtlasGreen/Orange,
   MaplePreWarAtlas*). A swap replaces the whole material, so its sibling `.pbrm` (its own tint colours)
   applies to that reference only; the card bake is per base model, so a swapped reference would show the
   base material's card. Carrying tint per reference would need the mask on the card (another sheet and four
   colours per reference). Reported, not built.
7. **FO4CS reader:** contract text only (LODGEN_LODM_FORMAT.md, LODGEN_IMPOSTOR_SPEC.md); FO4CS readers are
   built last.
8. **Gate `tests/spells/impostor_pbrm.sh`, bars pre-registered here.** The rung exe 309f3aa9 is the red
   (family legacy, no `_rmaos`, no `_s`). Per material class (split by the subsurface mask and the sidecar's
   tree flag): roughness, metallic, sqrt(F0') R/G/B and weight each have a median within 1.0 level of the
   independent Python law, AND at least 0.90 of the texels within 2 levels (uniform inputs, two 8-bit
   roundings, edge texels allowed 10 %). Colour: the law applied to the LEGACY card's texel (same base map,
   same view) against the pbrm card, with a median error no more than 1.25 x the FLOOR measured in the
   same run (an identity `.pbrm`: base map only, white, no tint, against the legacy card). The law is
   perturbed three ways, each of which must fail: Add in place of Normalize, IOR ignored (1.5), and the
   specular colour dropped. Kept green as the brief lists: lodgen_octahedral, impostor_draw, and the
   native_lighting control.

### Jobs 3-5 -- built (exe 45719ad4; commits 1f0368d bake, 3e92051 the `_s` format, separable)
- The design as written above, with one addition found by the gate. The retargeted COLOUR source's mips
  are the law evaluated on the map's OWN mip at each level (fix37), not a box filter of level 0. Run 1
  on the box-filtered chain (exe 07a0bc7e) lost alpha coverage at the coarse mips a 1:1 render reads.
  On the non-aa arm the silhouette's halfW was 370 against the legacy card's 416, and coverage was
  -1.8 %. With fix37, halfW is 416.548 on both, and the identity card equals the legacy card (median
  0.00 levels, p90 1.0). This is also the viewport's order: it samples the map first, then applies the law.
- Pictures (job 5, untracked): pbrm/pics/pbrm_all.png, made by pbrm_pics.sh. They show "3D model" |
  "Octahedral impostor" for roughness, metallic and the tinted colour, TreeMapleForest1 at az 30 el 5.
  The mesh half uses the bake's own retarget (WW_IMPOSTOR_MESH_PBRM=1, LOD channel 10 / 12). The trunk
  (MapleAtlas01) reads roughness 89 / metallic 204 on both halves, and the branches (MapleAtlas02_Tree)
  179 / 51. The first picture run drew the mesh magenta: the preview registered its root before
  writing the sources, and the index is built at the next lookup. fix38 reorders it; the log now says
  "colour found".
- The `_s` sheet and the `specular` key are SEPARABLE (director relays before a format ships): commit
  3e92051 holds lodgen's `_s` DDS and the key. Without it, the bake still writes `_oct_s.png` and a
  `specular _s` sidecar line, which nothing reads.
- BC7 weights {1,1,32,1} apply to `_s` too (lodgenWriteDds' BC7 path is the `_n` sheet's), so B is
  favoured. The measured error is still far under the codec floor (R4).

### Run 4 -- the director's decisions applied (2026-09-25 02:4x; director decisions, not rulings by bungo)
- **(1) Colour rows:** bar = max(1.25 x identity floor, 0.5 level), which is one 8-bit rounding plus
  margin. Every breakage still fails at 0.5. The margins below are bar minus measured, so a negative margin
  means a failure. `--red add` gives colour 4.14 levels on the aa arm (margin -3.64) and 4.00 on the non-aa
  arm (-3.50). `--red ior` and `--red decode` fail their sqrtF0 rows with 0.000-0.016 of texels within 2
  (bar 0.90). The pre-step-7 exe fails family (legacy) and sheets (_rmaos / _s absent) on both arms.
  Correct code: MapleAtlas01 0.32 (margin +0.18), MapleAtlas02_Tree 0.35 (+0.15) on the aa arm; 0.34 (+0.16)
  and 0.39 (+0.11) on the non-aa arm.
- **(2) Non-aa arm:** kept, judged on fully covered texels only (coverage == 1, alpha 255; `COVER=full`,
  named in the gate header and in impostor_pbrm.py's covmin). It has its own identity floor on that
  population (bake identna: median 0.00, p90 1.0 over 90395 texels) and its own pre-step-7 bake (prevna).
  Every breakage fails on it: add 5 rows (colour 4.00), ior 9, decode 8, pre-step-7 exe 2. The drop clause
  did not fire.
- **Still red: the non-aa arm, 4 of 15 rows**, all MapleAtlas02_Tree (the tree-animated class):
  roughness 0.878, metallic 0.872, sqrtF0.R 0.896, specWeight 0.881 within 2 (bar 0.90). The medians are
  exact (179 / 51 / 46 vs 45.77 / 128). MapleAtlas01 passes every row, and so does the colour row of both
  materials. An offline probe on the run-4 bakes (not a gate row) found this:
  - 98.0 % of the misses have a 4-neighbour of the other class, and the missed values read between the two
    materials (roughness p50 155, between 89 and 179): texels where trunk and leaf meet.
  - Away from class 0, the class-1 share within 2 is 0.997 (11082 texels).
  - The aa arm has the same boundary texels (92 % of its misses), but they are 2.6 % of 132925. The fully
    covered non-aa population is 13781 leaf texels, and 12.2 % of them sit on a boundary.
  So this is the per-texel class split at material boundaries, a population effect of the decided rule,
  not the step-7 law. Options for the director: (a) judge both arms away from the other class (texels with
  no 4-neighbour of the other class). On run 4, the leaf class's roughness then reads aa 0.998 and non-aa
  0.997 (only that channel was probed). (b) Drop the non-aa
  row with this reason. (c) Accept 4 known reds. Not applied: the decision said re-run once.

### Reds of run 3 (reported, not re-pinned; decided by the director, see "Run 4" above)
1. **R1 colour rows: the pre-registered bar is 0.** The bar was 1.25 x the identity floor. fix37 made
   the identity card equal the legacy card, so the floor is median 0.00 and the bar is 0.00. Correct
   code measures 0.32 (MapleAtlas01) and 0.35 (MapleAtlas02_Tree) levels median: the law applied to the
   legacy card's 8-bit texel, re-rounded, against the card computed from the source at full precision.
   The 13 constant rows all pass. The red still separates: `--red add` measures 4.14 on
   MapleAtlas02_Tree. Proposed: bar = max(1.25 x floor, 0.5 level), the one 8-bit rounding the
   comparison itself adds. This is the skill's check "a relative bar needs a floor that cannot be zero".
2. **R2, the non-aa arm (WW_IMPOSTOR_AA=0): 14 / 15 rows fail.** This row was added in the gate and
   was NOT in the design's pre-registration. The constant rows have the right median (roughness 89 vs
   89.25), but only 0.79-0.89 of texels are within 2. The misses are all partially covered texels
  , which the arm un-premultiplies by the matte's coverage. Fully covered class-0
   texels are within 2 on 0.977 (an ad hoc probe during the step, not a gate row). The arm's PRE-EXISTING mask path (roughness, 0.802) fails exactly as
   the new `_s` path does (weight, 0.791), so this is the arm's edge law, not step 7's. The colour
   rows fail on the zero bar as in 1. Proposed: drop R2, or gate it on fully covered texels only. The
   aa arm is the shipped default.

# 3. Gates (numbers; red runs)

## Step 1 (exe 97716e49, the worktree's first build = the rung, before the comment rebuild)
| gate | pre-registered | measured |
|---|---|---|
| impostor_trunk.sh | 38/3, the 3 named | 38 checks, 3 failures: 2 KNOWN RED flat-snap tear (el 0 4.68 %, el 20 10.14 %) + smooth end el 20 trunk bar -- the named three |
| impostor_draw.sh | 33/1, row 5 known | 33 steps, 1 failure: row 5 KNOWN RED (4x4 flat snap IoU 0.3920 < 0.50) |
| impostor_aa.sh | 7/0 | 7 checks, 0 failures |
| impostor_shrubs.sh | PASS | RESULT PASS |
| lodgen_octahedral.sh | PASS | RESULT PASS |
| lodgen_card_arrays.sh | PASS | RESULT PASS |
| lodgen_impostor_cards.sh | PASS | RESULT PASS |
Reds inside those gates fired in the same runs (trunk: the DXT5 sheet FAILS the sheet bar; the old drawer
FAILS the trunk bar; draw row 18: strongest-frame control breaks popping 2.06/2.09).
lodgen_defaults (d): the red is the worktree's own card dir (0 images -> now a NAMED refusal); green = C 7210.
Outputs: gates/*.new.out, gates/lodgen_defaults_d_maincards.out.

## Step 2 (exe ff86b488, N8 / TILE 512 / REF 1326.5, impostor_shrubs.sh MATCH=<model>)
| model | this exe: covered texels | pre-SHRUB1 exe (red) |
|---|---|---|
| sapling01 | 43,531 (halfW 93.83), S3 ok | 0 (halfW 1.08 = nothing measured), S3 FAIL |
| treeelmundergrowth01 | 40,952 (halfW 159.13), S3 ok | 0, S3 FAIL |
| shrubgrouplarge05 | 212,521 (halfW 405.21), S3 ok | 0, S3 FAIL |
The full 54-model impostor_shrubs.sh run of step 1 (RESULT PASS, 0 empty) covers the same three.
Outputs: gates/step2_<model>.<new|red>.out.

## Step 3 (exe ff86b488; orbit IoU vs the mesh, 16 bake directions, WW_IMPOSTOR_BLEND=0)
| subject | FIX5 (exe c529e3c1) | this exe |
|---|---|---|
| maple (TreeMapleForest2) at the full 128 long side, no ladder | 0.5022 (64x128 frames) | 0.4651 |
| maple, FIX5's ladder-rung fixture (32x64) | 0.4673 | 0.4072 |
| blast (TreeMapleblasted05) re-baked full size | 0.8701 | 0.9074 |
| blast, FIX5's fixture | 0.8701 | 0.8847 |
Same exe, full size vs ladder: maple +0.058, blast +0.023 -- FIX5's finding holds (the ladder rung is a real
part of the maple's gap, not all of it). The FIX5-era fixtures read differently on this exe (maple -0.060,
blast +0.015): the drawer moved since c529e3c1 (AA4, DEPTH2 crisp end), so only same-exe pairs compare.
Output: gates/cardres_test.out (pictures under cardres/, not committed).

## Step 4 (exe 56724fa6)
- impostor_defaults.sh: 7 checks / 0 failures (gates/impostor_defaults.new.out). D7 = the default picture is
  byte-identical to the rung's default at all 16 views. RED on the rung exe 97716e49: 6 / 1, D4 (the rung
  names no strongest-frame cut; gates/impostor_defaults.rung.out). D6 is the floor: the smooth end differs
  from the default at 8 of 16 views, so D5/D7 can see a change.
- kept green on 56724fa6: impostor_trunk 38/3 (the three named: flat-snap tear el 0 and el 20 = bungo's
  13:1x ruling, smooth end el 20), impostor_draw 33/1 (row 5, the known red), impostor_aa 7/0.
  Outputs gates/*.s4.out.

## Step 5 (exe eaa4b0b6)
- tests/spells/impostor_ring.sh 13 / 0 (gates/impostor_ring.s5.out). R1 16 view echoes, error 0.0000.
  R2 views 16, grid [16,1], albedo 1280 x 256. R3 this exe loads it ("grid: RING of 16 views"); the rung
  refuses it by name ("oct is 0, outside"); floor: the rung loads this exe's N8 set. R4 in-between azimuths
  el 0: IoU 0.4886 >= 0.4513 (0.90 x the mesh's own ceiling 0.5015); RED shuffled frames 0.0776.
  R4a at the bake directions 0.9131. R6 16 / 16 nearest-frame picks. R5 pixels ring16 62,795 vs N8 47,453
  bytes compressed (1.32); ring8 at 1.66 bites the size bar.
- RUN 1 FAILED 13 / 2 (gates/impostor_ring.run1.out): R4's bar was an absolute 0.60 pre-registered without
  measuring the subject; the mesh rotated by half a step against itself only reaches 0.5015. Re-pinned to
  0.90 x that measured ceiling (fix12), and the red filter now drops only 'EXCLUDED: mesh' lines (it was
  also dropping colour EXCLUDED lines). MISTAKES text in DELIVERABLE_TEXT.md.
- Mean IoU at the in-between azimuths, ring16 vs N8 (M, not gated):
  el 0: 0.4886 vs 0.8249 | el 5: 0.4863 vs 0.8044 | el 15: 0.4266 vs 0.6511 | el 30: 0.2849 vs 0.7947 |
  el 60: 0.2086 vs 0.3712. Full turn at el 0: ring16 0.6904 (ceiling 0.7012), N8 0.7842, ring8 0.5255.
- Kept green on eaa4b0b6 (gates/*.s5.out, *.s5b.out): impostor_trunk 38 / 3, the three named (flat-snap tear
  el 0 and el 20 = bungo's 13:1x ruling; smooth end el 20) -- re-run ALONE (s5b) because the chained run
  was refused while another lane's --port harness was up; impostor_draw 33 / 1 (row 5, the known red);
  impostor_aa 7 / 0; impostor_defaults 7 / 0 with RUNG= the rung (D7 byte-identical at all 16 views; the
  chained run without RUNG reads 6 / 0 because D7 only runs with a rung); impostor_shrubs PASS (54 of 54
  baked, 0 empty); lodgen_octahedral 116 ok / 0; lodgen_card_arrays 37 ok / 0; lodgen_impostor_cards
  12 ok / 0.
- lodgen_defaults.sh phase (d) with CARDS=<main tree>/scratchpad/showcase1_20260912/cards: 6 / 0
  (C lines 7210 = 7210, I 35, M 12, A 23, arrays 11 = 11; gates/lodgen_defaults.d.s5.out). The director's
  "no impostor-card lines with identity off" red is NOT a generator defect: a fresh worktree's card
  directory holds only the .txt sidecars git carries, so no card places and every C count reads 0. Fixed
  cheaply in step 1 (19c0347): the gate now fails BY NAME ("the card directory holds no card image ...
  set CARDS=") instead of reporting 0 C lines. Gate dir restored with git checkout afterwards.

## Step 6 (exe 309f3aa9; gates/impostor_wind.run3.out = 27 checks, 1 failure)
- G1 (the weight is the model's): elm G1a n/a (one shape, all tree-animated: no mask-0 texels; not counted),
  crown 250 distinct weights, 0.0000 at 255. Maple: A = 0 on 0.9562 of the mask-0 (trunk) texels, 256
  distinct; pine 0.9735, 247 distinct. RED: the previous exe's synthetic maple has A = 0 on only 0.1188.
  G1c, against impostor_wind_nif.py's independent W x h reprojection of the NIF: maple frame 0 r 0.8873,
  error 19.72 <= 33.73; frame 4 r 0.9086, error 21.69 <= 35.76. REDS fail the same bars: the C.a law
  (255 x h) r 0.7321 / 0.5391, error 67.45 / 71.51; the previous exe's bake r 0.5758 / 0.5008, error
  55.01 / 48.89. Measured only: elm r 0.8173 err 24.96 (C.a 61.61), pine r 0.6082 err 7.61 (C.a 106.94).
- G2 (no tree-animation shape = byte-identical): Hero and a rock, 6 files each, 0 differ, the sidecars
  identical minus the new sway line; Hero says synthetic; its compressed .lodm + DDS byte-identical to the
  previous exe's (5 same). FLOOR: the maple's normal sheet differs across the exes.
- G3 (lodm 2, refused by old readers): the elm card is lodm 2, sway model, leafAmplitude 1, leafFrequency 1;
  its card array is lodm 2 with array.sway ['model']; Hero stays lodm 1. The previous exe refuses the v2
  card by name ("payload is not a lodm 1 object"); FLOOR: it reads Hero's v1. This exe reads v2, refuses a
  SOURCE claiming 2 by name, and the preview loads the v2 set.
- **G4 RED** (BC7 keeps the weight): elm mean 3.573, p95 13 against the pre-registered bar 3.0 / 12. Red
  control: the next frame's picture reads 51.099. The same sheet's untouched normal R/G read 3.266 / p95 12
  (Hero's synthetic sway 1.281 / 4, its R/G 4.992 / 16). The bar came from the synthetic sway's 1.34 / 4,
  never measured on a real weight (MISTAKES text). NOT re-pinned. **Decision owed:** (a) accept at the
  measured level (the sway channel then costs what BC7 already costs the normal), or (b) raise the BC7 alpha
  weight for model-sway sets (kCardNormalBc7Weights {1,1,32,1}, src/lodgen.cpp ~4764; plumbed through
  lodgenWriteDds), which costs normal/height precision and must be measured first.
- Runs 1-2 (gates/impostor_wind.run1/run2.out): run 1 died at the first compress (`$1` after `shift 2`,
  set -u) and counted G1a elm as a failure; run 2 found the ring array's `|` file name (fixed, fix24).
- The arrays route in G3 returns rc 1 ("2 chunk(s) written ..., 1 failed"): the chunk TEXTURE-array stage
  looks for the stock front/side card (`materials/fo4cslod/cards/000531b3_fs.lodm`) in the resource stack,
  and the gate's card folder is not a data folder. The card arrays themselves are written ("1 card sets in
  4 arrays, 0 unreadable") and G3 reads them. A fixture limit of the route, not the sway path; not chased.
- Kept green on 309f3aa9 (gates/*.s6*.out): impostor_ring 13 / 0; impostor_trunk 38 / 3, the three named;
  impostor_draw row 5 only (the known red); impostor_aa 7 / 0; impostor_defaults 7 / 0 with RUNG (D7
  byte-identical at 16 views); impostor_shrubs PASS (0 empty); lodgen_card_arrays PASS 37 ok;
  lodgen_impostor_cards PASS 12 ok; lodgen_octahedral 115 / 1 on the stale "lodm 1 card" premise (its
  fixture is a tree), PASS after fix27 (s6b).
- pbr_shade_ab (fo4_default.vert/.frag changed): **10 cases, 0 failures, PASS** (s6c; census moves effect,
  lit, particles; 3 particle cases empty by the viewer, as the harness classifies them). OLD arm =
  release/before_pbrr0 built from the step-5 exe + the pre-step-6 shaders (git HEAD). Two refused runs
  before it: s6 had no OLD arm; s6b's OLD arm lacked its DLLs (my copy, rc 127 on every OLD picture).
- The GIF: wind/gif/elm_sway.gif, 12 frames 1020 x 1030, "3D model" | "Octahedral impostor", amplitude 0.08,
  az 30 el 5. Card coverage moves 0.0370 .. 0.0422 across the phases (the shear is live); IoU vs the static
  mesh 0.42 .. 0.46. Run 1 drew the card blank: the sheets need a `textures\` tree beside the .lodm for the
  preview's texture cache (the log said so by name); wind_gif.sh now builds one.

## Step 6b (exe 309f3aa9, unchanged)
- tests/spells/impostor_ring.sh 17 / 0 (gates/impostor_ring.s6n.out): R1-R6 as before, plus R7: the tree
  run with no RING writes `ring 0`; RING=16 writes `ring 16`; RING=5 exits 2 naming the rule. RED: the
  step-5 driver (git 1303334, pulled out beside the real one and removed afterwards) wrote `ring 16`.

## Step 6c (exe 309f3aa9, unchanged; gates/impostor_wind.run4.out = 28 checks, 0 failures, PASS)
- G4 elm: sway error mean 3.573 p95 13 (max 88), 93533 covered texels. Floor (normal R/G) 3.266 / 12 ->
  bar 4.082 / 15: ok. RED 1, the next frame: 51.099 > 4.082. RED 2, the 4-bit sway: mean 6.702 fails.
  The synthetic Hero set for comparison: 1.281 / 4 against its own R/G floor 4.992 / 16.
- G1-G3 unchanged from run 3 (same exe, same bake).

## Step 7, run 4 on the director's bars (exe 45719ad4; gates/impostor_pbrm.run4.out; fix39)
| row | measured |
|---|---|
| R1 aa arm | 15 / 15 ok (colour 0.32 / 0.35, bar 0.50) |
| R2 non-aa arm, coverage == 1 | 11 / 15 ok; 4 FAIL, MapleAtlas02_Tree boundary texels (0.872-0.896 vs 0.90) |
| reds, aa: add / ior / decode / pre-step-7 exe | 1 / 6 / 5 / 2 FAIL |
| reds, non-aa: add / ior / decode / pre-step-7 exe | 5 / 9 / 8 / 2 FAIL |
| R3 / R4 | ok / ok (0.067 / 1.0 <= 2.776 / 10; 4-bit 5.572 fails) |
| total | 13 / 14 ok |

## Step 7 (exe 45719ad4; gates/impostor_pbrm.run3.out + .run3.log)
| gate | pre-registered | measured |
|---|---|---|
| impostor_pbrm.sh R1 (aa) | 15 rows ok | 13 ok; 2 colour rows FAIL on a bar of 0.00 (red 1) |
| impostor_pbrm.sh R2 (non-aa) | not pre-registered | 14 FAIL of 15 (red 2) |
| reds add / ior / decode / pre-step-7 exe | each >= 1 FAIL | 2 / 8 / 7 / 2 FAIL (the pre-step-7 exe: family legacy) |
| R3 `.lodm` | pbrm: `specular` -> `_oct_s.DDS`; legacy: none | ok / ok (both `lodm` 2: the maple carries model sway) |
| R4 BC7 `_s` | <= 1.25 x the set's `_n` R/G floor | worst channel 0.067 / p95 1 <= 2.776 / 10; 4-bit red 5.572 fails |
| lodgen_octahedral.sh | PASS | RESULT PASS |
| impostor_draw.sh | 33/1, row 5 known | 33 steps, 1 failure: row 5 KNOWN RED |
| lodgen_impostor_cards.sh / lodgen_card_arrays.sh | PASS | PASS / PASS |
| native_lighting.sh (control) | its own | 21 checks, 2 failures: gate (a) legacy_btr_top / _obl not byte-identical to baseline. The SAME 2 fail on the pre-step-7 exe eaa4b0b6, and the step-7 top picture is byte-identical to that exe's, so the drift predates step 7. Its fixtures are only in the main tree: run from a temporary copy that reads them there (read only) and writes into this worktree |
Run 1 (exe 07a0bc7e): the box-filtered colour mips (fixed, fix37); compress made no chunk (the gate did not
copy `_front`/`_side`; fixed). Run 2 (exe a4d0c877): the same as run 3 (fix38 is preview-only).

# 4. Exe sha1 + commits
- rung / first build: release/NifSkope.exe 97716e4988e493f7b0eab6952780ac18aca0a609 (21:43:55),
  kept as release/NifSkope.before_cardfix1.exe.
- step 1 build (comments only, 10 objects recompiled): ff86b488aa769d1e453b719ee8e07e5f9ce8164f (22:24:34), same size 24,716,800.
- step 4 build: 56724fa6332362467884619efe19667e158024fc (22:31:49), 24,717,312 B.
- step 5 build: eaa4b0b60e9ff6796df846f54ebf292a94cc0aca (23:00:20), 24,729,600 B; kept as
  release/NifSkope.s5_eaa4b0b6.exe (the step-6 gates' previous exe).
- commits: step 1 19c0347; step 2 91ddd41 (evidence only); step 3 d8302c9; step 4 7896ad1;
  step 5 1303334 (code) + 6c5f5f8 (DONE); step 6 24e7835 (code, gate, DONE); step 6b 6430dff; step 6c = the
  commit carrying this text (G4 re-pin + run4).
- step 6 builds: 0eeade3a (23:59:42, first); 309f3aa9a09897da12c94db644ff70f57f33dc7b (2026-09-25 00:22:33,
  24,737,280 B; + the ring array file name, fix24) = the exe every step-6 number is from.

- step 7 builds: 07a0bc7e (fix34-36), a4d0c877 (+ fix37, the colour mips),
  45719ad43e509a46e6bf04a4dfd3f9d342e844d1 (+ fix38, preview root order; built 02:27 2026-09-25) = the exe
  every step-7 number is from. Commits: 2672e43 (jobs 1-2), 1f0368d (bake), 3e92051 (`_s` format), then
  the commit carrying this text (gate, docs, DONE).

# 5. What the final bake needs
- A card bake from THIS branch: the N8 grid by default (no RING), TILE 256, the crisp cut (steps 4, 6b).
- Sway A is on for every model with a tree-animation shape (no switch; ruled). Those cards and their card
  arrays are `lodm` 2: an exe from before step 6 and FO4CS's current reader refuse them BY NAME. So the
  in-game test needs the FO4CS reader to learn `lodm` 2 (owed, FO4CS built last by standing order); the
  NifSkope side reads and draws them.
- The G4 ruling (accept 3.573 / 13, or raise the BC7 alpha weight) comes BEFORE the final bake, because
  option (b) changes every model-sway card's bytes.
- lodgenaggregate does not know ring sets or `lodm` 2 (not this lane's file): with the N8 default no ring
  set is made, and the aggregate composites the weight but writes lodm 1 with no `sway` key.
- Previewing a loose card set needs a `textures\` tree beside the .lodm (the harness says so by name).

- A `.pbrm` model's card is family pbr only when EVERY textured shape resolves a `.pbrm` (or a source
  `.lodm`). The `.pbrm` files must be in the resource stack or WW_LODGEN_DATA_ROOT at bake time. The
  `_s` sheet needs the FO4CS reader (contract in LODGEN_LODM_FORMAT 3.4; owed, FO4CS built last).

# 6. Skill review
- Written: E:\Projects\Claude\.claude\skills\ww-preregister-bar-from-the-subject\SKILL.md -- measure the
  reference's own ceiling, the subject's population and the REAL input before writing a bar; print the
  codec's error on an untouched channel beside a lossy-codec bar; a pre-registered bar that turns red on
  correct code is reported with options, not re-pinned in the same step. It folds this lane's four
  MISTAKES entries into one procedure.
- Candidates for the director (DELIVERABLE_TEXT.md, Skill review): staging one step's hunks of a file that
  already holds the next step's (stage6_docs.py: undo the later patch's pairs in memory, write the INDEX
  only); D7 in impostor_defaults.sh needs RUNG=; the preview's `textures\` tree for a loose set; the
  pbr_shade_ab OLD arm needs the whole runtime (DLLs, qt.conf), and an OLD arm that cannot start reads as
  "NO PICTURE" on every case, not as a refusal.
