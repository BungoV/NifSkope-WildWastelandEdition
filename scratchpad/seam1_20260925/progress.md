# SEAM1 progress

- 12:01 (clock read) start. Game UP (Fallout4.exe) -> no builds. Brief + common rules read. Skills: ww-artefact-localise, ww-texel-picture, ww-lodl-offline-census, nifskope-ww-render-shot, fo4-nif-vertex-channel-census.
- 12:05 addenda 1-7 received (vanilla-colour fill, all grey tiles, "proper" P1-P6, AO pictures, Boston oblique first, W4 building vertex colour).
- AO pictures DONE (pics/ao/, exe 27a7bb29 run copy, WW_LODL_AO=1 / default / WW_LODL_CHANNEL=ao flat); diffuse panels byte-identical to BAKE1's. Paths sent to director.
- W4 DONE offline: source carries no tint (1,058 of 1,086 shapes have neither colour stream nor Vertex_Colors bit); LOD diffuses are near-grey (sat 0.03-0.09); .lodo v4 has no colour slot; v5 optional-stream proposal sent, awaiting call.
- 12:08 (clock read) back to Sanctuary edge localisation.

## 12:19 (clock read)
- Addendum: the oblique Sanctuary AO set (08's camera: view 8, 8x8 cells, Z 6690 = 1050 u under the region's mean ground, the same as Boston) is written to pics/ao/01_sanctuary_oblique_{ao_x_diffuse,diffuse,ao_only,sheet}.png and the paths are messaged. 08 is an ORTHOGRAPHIC oblique view (persp=0); matched as is.
- SANCTUARY EDGE LOCALISED. From the file's own texels (vtread.py, an mmap .lodt reader; 0 mismatches against the authority BC1 decoder on 278,784 texels) the block is exactly cells x -20..-16, y 20..24, i.e. one dim-4 chunk on the -96 grid. The steps sit at -20.00, -16.00, 24.00 and 20.00 (12.9, 11.3, 11.6 and 10.3 lum). Interior dim-2 tile borders (-18, 22) step <= 1.5.
  - C4 is out: the edge is in the file.
  - C1 is out: LAND in these cells comes only from Fallout4.esm; the DLC/test-plugin LAND is elsewhere. Bethesda's own dim-4 LOD over the same chunks steps 0.16-1.47, against a noise p99 of 1.3-2.5.
  - C2-family, our colour law §2.5 step 4: a BTXT=0 quadrant (and a NULL-LTEX layer) paints the enclosing dim-4 chunk's DOMINANT base. Chunk (-20,20) = LRiverbedSilt01 (dark silt); its neighbours = LRubbleRock01 / LRootsEroded01.
  - The engine's rule, from the exe's string table: the INI [Landscape] sDefaultLandDiffuseTexture, default Ground\CommonwealthDefault01_d.dds, one texture world-wide.
  - Model replay (law_predict.py): the shipped rule gives border steps 18.9 (west), 12.3 (north), 8.5 (south) and 6.5 (east); the engine default gives 0.7 / 4.8 / 2.2 / 1.2; the neighbour steps are about 1.3-1.9.
  - Against vanilla per cell (law_vs_vanilla.py): block minus ring is -17.9 on the shipped rule, -3.8 on the engine default and +1.7 in vanilla; correlation 0.20 against 0.35.
- Game UP at 12:11 -> the fix is written as code, BUILD PENDING.

## 12:33 (clock read)
- G2 comparer written (g2_compare.py): self-control rung/g2 vs itself = all identical; negative control rung/a vs rung/c = colour+mask differ on 44/48 tiles, msn+height identical -> the comparer can see a change and can see none.
- Commit c21eb26a: engine-default law in src + models + docs §2.5 step 4. Game UP at 12:31 -> BUILD PENDING.
- Next: W1 census finish (empty-list guard), coarser VT levels.

## 12:46 (clock read)
- W1 measured: 2023 of 2304 dim-4 chunks carry LAND with NO BTXT and NO ATXT anywhere ('bare'); the shipped VT paints 2016 of them ONE flat colour (132,128,132) = the old law's dominantBase 0 -> colour default grey; SD 0.61. The coloured flat squares are painted chunks whose majority is BTXT-less and got the chunk DOMINANT base (e.g. (-36,-36) LGlowingSeaRubble01NoGrass = the dark Glowing Sea squares; (-24,28) LNFoothillsDirt01 = the tan squares). SAME ROOT as Sanctuary: §2.5 step 4.
- Vanilla LOD has a dim-4 tile on all 2023 bare chunks (SD ~5-15). Engine-default law predicts SD 6.3 there (not flat).
- W2: .lodl covers all 192x192 cells; 184,431 placements in 4,541 cells; 1,058 of those cells (14,714 placements, 8.0%) have NO painted LAND -> they stand on the flat grey fill, which reads as 'no terrain'. Same root.
- Fill model (fill_model.py, no build): tone fit on overlap cells, band from measurement, gain capped at 1. Sanctuary-north region: border steps A max 19.0 (3 over bar 12.94) -> B 12.53 (0) -> F 12.78 (0); inside the band F p95 8.25 vs vanilla 9.88. Picture pics/fill_model_sanctuary_north.png.
- 13:03 (clock read) a6e5e8de: --vt-fill-vanilla stage in src (lodgen.cpp LodgenVtFill + driver pre-pass/apply/report, CLI, panel), docs 2.6. Vanilla read LOOSE under --vanilla-lod-root (2.5b provenance rule), not the resource stack. fill_model.py aligned to the C++ definitions (Chebyshev ring, bar over overlap+ring, gain cap 1); three regions pre-registered. fill_gate.py FG1-FG3 harness validated on the rung controls. Game UP 13:02 -> BUILD PENDING. Next: pictures (perspective), A2 lists, then W4 .lodo v5.

## 13:16 (clock read)
- W4 (.lodo v5 colour stream) code written, uncommitted: lodofile.h/.cpp (writer + reader + describe readout), lodgen.cpp (loader: colour only where the shape has a colour channel AND SLSF2 Vertex_Colors; A kept, Vertex_Alpha rides as a mesh flag), lodinative.cpp (viewer multiplies the stream in, alpha only on a VERTEX_ALPHA mesh). Independent Python decoder tests/spells/lodgen_native_decode.py taught v5; reads the shipped v4 library unchanged (297,452 verts, 0 colour rows). Game UP 13:14 -> BUILD PENDING. Next: gate script, docs, commit.

## 13:32 (clock read)
- Commit 62e53a3b: .lodo v5 colour stream (src + docs 3.7 + independent decoder + fixture expectation). Gate pre-registered: w4_bakes.sh + w4_gate.py (G1 no-colour byte identity but 0x04; G2 Amphitheater + blasted maples carry colour, flags agree with the source both ways, rows = source rows; G3 refuter). Instruments self-tested on a hand-built v5 (w4_synth.py GREEN: rows round-trip, strip() gives v4 but 0x04, 3 mutations refused by their own rule, control accepted). OLD exe bakes (w4/old) read G1 GREEN, G2 RED -> the refuter holds.
- Boston look (measurement only): vanilla's 9 downtown object-LOD chunks carry NO vertex colour and no Vertex_Colors bit (27 shapes, 375,058 verts), SLSF1 0x80400001 / SLSF2 0x5 (the same words our viewer writes), emissive black x1, one atlas; the atlas at their own UVs is near-grey too (lum 80.2, sat 0.067). No LOD tint exists in the files -> candidate = the engine's weather light (sun colour + DALC ambient) on the LOD_Objects path. Not fixed.
- Pictures (before, perspective): pics/w3_overview_before_oblique.png (terrain bounds -42..32 x -48..38), pics/edge_zoom4x_before_oblique.png (cells -21..-20 x 21..22).
- A2 before: a2_grey_before.txt -- shipped VT.2: 2019 of 2304 dim-4 chunks flat (SD<2); vanilla covers all 2304.
- Game UP at 13:32.

## 13:46-14:27 (from the logs; game DOWN at 13:46)
- Builds 1-3 (build1 recompiled nothing: copied objects newer than the edited sources; touched and rebuilt). Final exe b9fd029b (14:15:55).
- Grass: C4 wins (bake tint = smallest mip rgb/alpha, clamped white). Fix 44805f8f (alpha-weighted mean, first mip <= 1024). Grass gate old (238,235,236) RED -> new (101,97,62) / (99,100,66) GREEN, re-run on b9fd029b 14:21 GREEN. Report sent.
- Engine-default path bug in c21eb26a ("\G" escapes -> no file -> grey). Fix 59a0dd33. DG1 (re-registered 14:21) broken 0.1635 RED, fixed 0.0064 GREEN. escape_scan: 0.
- G1 edge GREEN (shipped 12.93 RED); G2a/G2c GREEN; G2b unmeasurable (0 untouched tiles). Spells on b9fd029b: lodgen_native 32/0, lod_generation 128/0, lodgen_loadorder 24/0 PASS.
- W4 on b9fd029b: G2 GREEN, G1 RED (selfAO bytes at 0xA8 on the water-tower meshes + CRCs; deterministic per exe; hypothesis FMA/inlining codegen, unproven). Not part of the VT re-bake.
- 14:27 pics/seam/sanctuary_before_oblique.png (shipped FO4CSLOD).

## 14:39 (clock read)
- Director restart at 14:38 cut off both whole-map VT bakes (no end line, 0-byte VT.32) and a spell re-run. Game down (gate read). Both bakes relaunched 14:38:53 on b9fd029b; spells + shipped VT sha1 re-running.
- 14:53 (clock read) spells on b9fd029b re-run clean: lodgen_native 32/0, lod_generation 128/0, lodgen_loadorder 24/0 PASS. Shipped VT sha1 recorded (install_sha1_before.txt). pics/seam/whole_before_oblique.png (shipped, W3 terrain-bounds framing -42..32 x -48..38, view 8). Bakes running.
- 16:20 (clock read) bakes ended 15:53 (off) / 15:55 (on), rc 0. FG1-FG3 GREEN, A2 0 flat, fill_model Sanctuary-north + Glowing Sea GREEN, north-east RED at one border that is identical with the fill off (painted-set mismatch in the model). Game gate DOWN -> installed 16:14 into FO4CSLOD (6 files, replaced/ + sha1 verified both ways). Sanctuary after picture 16:14, messaged. Whole after 16:19. DONE.md rewritten.
