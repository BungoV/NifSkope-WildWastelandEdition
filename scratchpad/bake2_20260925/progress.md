# BAKE2 progress (lane BAKE2, worktree E:\Projects\NifskopeWWE-bake2, branch bake2-20260925 from main d5764fbe)

- 18:53 (clock read) worktree built: main's objects reused (main clean in src, make -n 0 g++), REVISION objects
  deleted and rebuilt, ww_build.sh rc 0. exe 7cfccac1 (rung copy release/NifSkope.before_bake2.exe; run copy
  scratchpad/bake2_20260925/run/release). NIFSKOPE_REVISION is empty in this Makefile, as in main's.
- 18:55 worldspaces in his 47-plugin order (--list-worldspaces): SanctuaryHillsWorld 000A7FF4 (Fallout4.esm),
  NukaWorld 0600290F (DLCNukaWorld.esm), DLC03FarHarbor 03000B0F (DLCCoast.esm).
- LAND extents (--dump-land, land_extent.py): pre-war 118 LAND cells, x -25..2 y -9..25 (two blocks: the
  Sanctuary block -25..-16 x 17..25 = 90 cells, and 28 cells at x -3..2 y -9..-4); Nuka-World 4225 cells, -32..32
  x -32..32 (full square); Far Harbor 2067 cells, x -29..20 y -21..31.
- .lodl header bounds (after the lodl stage, i32 at 0x08): pre-war -28,-13..7,34; Nuka-World -32,-32..32,32;
  Far Harbor -73,-59..69,78. The header is the worldspace's CELL box, wider than the LAND box on pre-war and FH.
- Vanilla terrain LOD loose in DataUnpacked: Commonwealth, DiamondCity, SanctuaryHillsWorld only. Pre-war ships a
  whole-map set (3062 .BTR incl. objects dir, 6121 textures, 22 object meshes, LODSettings -96..) -> fill ON there.
  DLC03FarHarbor and NukaWorld: none loose -> fill OFF for both (nothing extracted).
- 18:56 cards: 61 tree candidates over the three worlds (sanct 18, NW 26 all vanilla 00-, FH 43 incl. 14 DLCCoast);
  30 already in BAKE1's library (no card-bake code change on main since BAKE1's base) -> copied; 31 new baking with
  BAKE1's library settings (N8, ring 0, tile 256, ref 1328.6 = also the max over the three lists).
- 18:57 lodl stage rc 0 for all three (1-3 s each), cross-check 0 mismatched.
- G5 before-listings taken 18:56 (mods top 78, root 8, profile 13 files, FO4CSLOD 3681 entries: Commonwealth only).
- 19:01 (clock read) cards DONE: 31 new sets baked 18:56:59 -> 19:01:27, 0 FAILED/MISSING; library 61 sets, 0 empty,
  lowest coverage 7.3 % (000531ae, a reused BAKE1 set); new and reused sidecars both `projection ortho`, `legacy 256`.
- 19:01:36 pre-war chunks stage launched (region -25 -9 2 25, --dim all, fill ON). TINT1's -no-gui Commonwealth bake
  is running at the same time (allowed; the starve rule is a bake past twice its expected time).
- Instruments ready: checks.py (no-LAND placement cells, named; flat-grey dim-4 chunks from VT.16). Grey refuter:
  SEAM1's pre-fill Commonwealth VT.16 reads 2021 of 2304 chunks flat grey; the installed fill-ON one 0 of 2304;
  playable Sanctuary / Boston blocks 0 of 16 on both (the grey was the unpainted ring). Threshold chroma < 6 and
  luminance SD < 2 per chunk.
- Landmark cells (cell_names.py over the plugins' CELL EDIDs): Nuka-World Galactic Zone -6..-3 x -2..1, Nuka-Town
  -1..0 x -6..-1, Kiddie Kingdom 0..3 x 0..3 -> oblique frame -8 -7 3 4; Far Harbor town FarHarborExt 13..14 x
  6..8 -> frame 9 3 16 10; pre-war PrewarSanctuaryExt01 -20,21, PrewarPlayerHouse01 -20,22 -> frame -24 18 -17 25.

## 19:15 (clock read) -- pre-war Sanctuary (SanctuaryHillsWorld, WRLD 000A7FF4) INSTALLED and pictured
- First bake (region = LAND box -25 -9 2 25) wrote ONE VT level (VT.2). Cause read in the source
  (lodgenVtLevelsFromBounds): the VT ladder stops at the coarsest dim dividing the region's WEST and SOUTH edge;
  -25 divides by nothing. The region is clipped to the CELL bounds (-28,-13), so the best a full-LAND region can do
  here is -28 -12 (dim 4). Re-baked on -28 -12 2 25 (covers every LAND cell): VT.2 + VT.4 (2 levels), 84 s rc 0.
  Old run kept as prewar_r1_unaligned/. Far Harbor gets -32 -32 20 31 (CELL bounds -73,-59 allow it: 5 levels).
- Cards: 0 linked, by design: a card stands in only where the ring's MNAM slot is EMPTY (lodgen.cpp card rule);
  the pre-war maples ship TreeMaple*_LOD_0/_LOD_2 for every ring. G3: 34 bases, 0 with card, all index 00.
- G1: native-verify rc 0 (pair identity ok); census --self-floor rc 0 FLOOR ok; lodb/lodm/lodl readers rc 0 (lodm
  2 checks 0 failures); --lodt-check VT.2 and VT.4 rc 0. G2: .lodb plugins 47. G4: VT.2 mask BC3, grass cells
  -25,17 -22,22 -21,24 non-uniform (PASS); red (g4red.sh, no --cover) BC1 no channel on all 3 (RED as it must).
- Placement cells with no LAND: 15 cells / 101 placements, all named (x -15..-11, y 16..24; east of the NW block).
- Flat grey: LAND chunks 0 of 18. NEW: no-LAND VT cells 42 of 1162 flat grey (chroma < 6, lum ~124), ALL at distance
  1 from LAND = the ring round the NW block. Vanilla's LOD texture there reads lum ~55 chroma ~38. Cause (source):
  the fill blends a no-LAND cell from the generator's own grey toward vanilla by a smoothstep over bandCells (4) of
  distance, so the first ring stays grey. The Commonwealth never shows it (every VT cell has LAND). Reported, not
  changed (code, not bake).
- Installed mods/FO4CSLOD/FO4CSLOD/SanctuaryHillsWorld (was absent): 163 files, 517,928,527 B, sha1-identical to scratch.
- Pictures: pics/prewar_topdown.png 3200x4264 on header -28,-13..7,34, INSIDE >=20 px (46 px), lum SD 29.8,
  15,566 colours, 1350 of 1350 drawn. pics/prewar_oblique_sanctuary.png (BAKE1 08 camera: view 8, 8x8 cells, ORT
  S*2048, 1600^2, LV 2, slot 0) lum SD 34.6, 12,175 colours; the houses have no authored LOD model (manifests hold
  only street lamps and cliffs besides trees), so the cul-de-sac road shows without houses.
- Director messaged with the top-down path at 19:14.

## 19:35 (clock read) -- Nuka-World BLOCKED; Far Harbor baking
- Nuka-World (NukaWorld, WRLD 0600290F), region -32 -32 32 32, fill OFF: chunks stage 19:15:36 -> 19:33:46 rc 1 (1090 s).
  VT 5 levels written (2.7 GB scratch), then the .lodi writer REFUSED: ref 0604DDB9 (RockCliff03_LOD_0) scale 8.33 >
  7.99988 (u16/8192 ceiling, src/lodifile.cpp, "refused, not clamped"). overscale.py over the plugins: 12 NukaWorld refs
  above the line, 4 with an MNAM base (0604D45A 9.97, 0604D45D 8.33, 0604DDA1 9.23, 0604DDB9 8.33). No .lodi/.lodb/
  manifests -> NOT installed. Director asked for a ruling (format change / drop-and-name / leave out); no code change.
- Far Harbor: overscale.py 8 refs above the line, 0 with an MNAM base -> safe. Region widened to -32 -32 20 31 (CELL
  bounds -73,-59 allow; covers every LAND cell) so the VT ladder reaches 32. Chunks stage launched 19:35.
- G4 picks: Nuka-World is desert (1 full-grass cell of 4225): -28,2 / 2,0 / -8,6 (all-grass share 0.80-0.83).
  Far Harbor: 194 grass cells: -24,6 / -23,-4 / -22,2.

## 19:55 (clock read) -- Far Harbor baked, verified, installed; ruling (a) code patched, not built
- Far Harbor (DLC03FarHarbor, WRLD 03000B0F), region -32 -32 20 31, fill OFF (vanilla LOD archived only): chunks
  19:35:01 -> 19:51:41 rc 0 (1000 s). VT 5 levels (2,4,8,16,32), 1164 tiles. .lodl header -73,-59..69,78.
  578 files, 1,733,543,578 B.
- G1: native-verify rc 0; census --self-floor FLOOR ok (doctored instanceCount caught); lodb/lodm/lodl rc 0;
  --lodt-check all 5 VT levels rc 0. G2: 47 plugins. G3: 909 bases, 43 with a card (29 Fallout4.esm, 14 DLCCoast).
- G4: VT.2 mask BC3. Mixed-grass cells -27,7 / -25,3 / -26,10 non-uniform (PASS); red run (no --cover) BC1 on all 3.
  The first picks -24,6 and -22,2 read uniform 173: grass_fh.txt says both are all-grass base quadrants with 0 layers,
  so full uniform cover is the plugin's own answer there, not a missing channel. -23,-4 non-uniform.
- No-LAND placement cells: 0. Flat grey: LAND chunks 0 of 147. No-LAND VT cells 2029 of 2029 default grey (lum 129.8,
  chroma 2): the sea round the island inside the aligned VT box; fill is off because vanilla LOD is not loose.
- Installed mods/FO4CSLOD/FO4CSLOD/DLC03FarHarbor (was absent): 578 files, sha1-identical to scratch.
- Pictures: pics/farharbor_topdown.png 3200x3124 on header -73,-59..69,78 (header corners 38 px L/R, 54 px T/B by
  upp 187.5), content INSIDE, lum SD 35.2, 27,790 of 27,790 drawn. pics/farharbor_oblique_town.png (08 camera over
  9 3 16 10, FarHarborExt): lum SD 28.3, 7,644 colours, church/docks/boats drawn; the 08 camera's diamond touches the
  side edges, as the pre-war one does.
- Ruling (a): v10 wide-scale bit patched into src (lodifile.h/.cpp, lodinative.cpp, nativeemit.cpp), the decoder,
  fields spell, census check, cut.py and the docs (§4.14). Not built yet. Next: gate instruments, build, gates.

## 20:14 (clock read) -- ruling (a): built; instruments, rung RED, fixture identical
- Max XSCL in his load order (overscale.py, maxscale.txt): Commonwealth 10.0 (26 refs over 7.99988, 0 with LOD),
  pre-war 3.62, Far Harbor 10.0 (8 over, 0 with LOD), Nuka-World 10.0 (12 over, 4 with LOD). Engine cap 10.0; the
  v10 range 8..15.99988 gives 60 percent headroom at the same 1/8192 step.
- Instruments, no build (widescale_synth.py on the installed pre-war v7 .lodi): hand-built v10 read by the new
  decoder (instance 0 1.0 -> 9.0, 1349 others unchanged); strip = version + flag byte + 3 CRCs only; HEAD's decoder
  refuses it ('version 10'); bit 7 in v9 / v7, bit 8 in v10, scale 0 without bit 7 each refused by its own rule;
  scale 0 with bit 7 reads 8.0; recrc control identical. WIDESCALE INSTRUMENTS PASS.
- widescale_check.py refuter on the pre-war v7 file: RED on W1/W2/W3/W5 as it must be.
- Build 20:00 (6 objects: every includer of lodifile.h) gave exe 50e5d920, which Avast blocked (rc 126, 'Access is
  denied' after a 90 s hold, three tries). Relinked 20:12 -> 0d71d0d6, runs after the 90 s scan. The blocked file is
  kept as release/NifSkope.blocked_50e5d920.exe (untracked). No exclusion added.
- Gate on the RUNG (7cfccac1): NW objects bake rc 1, 'ref 0x0604ddb9 ... scale 8.33 is outside 0 .. 7.99988;
  refused, not clamped' -> WIDESCALE G2 FAIL (pre-registered RED).
- Fixture (--native-fixture) rung vs new: 3 files, 0 differ.

## 20:39 -- halo (bungo on prewar_topdown.png) and the director's fill-ON ruling for the DLCs

* Halo measured (halo.py, halo_prewar.txt): no-LAND cells 1/2/3 cells off pre-war's LAND read +38.1/+30.5/+19.0
  luminance over vanilla, +10.8 from 5 out (the tone match). Fill OFF: flat placeholder grey 129.6 everywhere
  off-LAND. Vanilla's own texels there: 64..66, no ring. Cause: the fill blended FROM the placeholder grey over a
  4-cell band (and that grey also set the band). Commonwealth: 0 flat-grey cells (halo_cw.txt), so no halo there.
* Fix in src/lodgen.cpp (patch_halo.py): a no-LAND cell is filled whole (w = 1) and left out of the band's p95.
  Gate halo_gate.py pre-registered: RED on the current bake (halo_gate_rung.txt: 100 of 180 near cells hot).
* Director's ruling 2026-09-25 (no calls after approval): Far Harbor and Nuka-World fill ON. Vanilla dim-4 LOD
  colour tiles extracted READ-ONLY from DLCCoast/DLCNukaWorld - Textures.ba2 into the input cache
  E:/Tools/Fallout 4/DataUnpacked/Data (1296 + 256 tiles, 512 BC1), plus LODSettings/*.LOD from the Main BA2s;
  manifest dlc_lod_manifest.tsv (archive, path, bytes, sha1; 1554 lines). Nothing into the mod or git.
* Found: Far Harbor's vanilla dim-4 grid is offset (LODSettings SW cell -73,-59, so x = 3, y = 1 mod 4); the fill
  addressed only multiples of 4 and would have found no sheet. patch_grid.py reads the phase from
  <vanilla root>/LODSettings/<WS>.LOD (phase 0,0 = old addressing). Registration checked: best cell shift 0,0 on
  Far Harbor (r 0.576) and pre-war (r 0.528) (align_check.py).
* Far Harbor BEFORE (installed, fill off): VT.4 1517 of 3584 cells flat grey, 77 of 224 tiles all grey;
  no-LAND cells +45 over vanilla at every distance (halo_gate_fh_before.txt).
* Build: exe 4638a958 (v10 + halo + DLC grid), run copy run3/. Running halo_runs.sh (pre-war fix/off legs, CW
  region rung-vs-new VT identity).

## 21:02 -- halo fixed on pre-war, installed; Far Harbor fill-ON bake running

* Exe 4638a958 was blocked by Avast (rc 126 from both run3 and release after about 110 s); relinked: 62412e83
  (same sources), runs from the worktree release/ (no builds pending).
* halo_runs.out: HALO GATE PASS on pre-war fill ON (d=1/2/3 ON-VAN +10.76/+10.75/+10.77 vs far +10.75; 0 of 180
  hot cells, max excess +0.2). .lodi/.lodo/.lodl identical to the install. Fill OFF new vs old exe: 163 files, only
  the .lodb differs (revision). Commonwealth region -24 16 -9 31 fill ON, rung vs new: VT.2/4/8 + VT.lodm IDENTICAL
  (noLandCells=0, grid=0,0 from LODSettings -96,-96).
* Pre-war reinstalled: VT.2, VT.4, .lodb replaced, old copies + sha1 in replaced/prewar_halo_2100/sha1.txt, all MATCH.
* Pictures (same camera, cam logs identical): pics/prewar_topdown_after.png; side by side
  pics/prewar_halo_before_after.png. Picture luminance by distance from LAND, before -> after:
  d1 124.5 -> 95.8, d2 116.4 -> 94.8, d3 106.5 -> 96.9, d4 97.6 -> 96.3, >=6 94.0 -> 94.0 (halo_pics_prewar.txt).
* Far Harbor fill ON bake started (farharbor_fill/).
