## TOP BLOCK -- written @@WHEN@@ (`date`-read) by lane MERGE2: IDENT2 + TERRLIVE1 MERGED into main, main's exe rebuilt; NOT re-baked, NOT flown

bungo's word (2026-09-29, after both lanes' final pictures): merge "Yes". Done by MERGE2. Report:
scratchpad/merge2_20260929/DONE.md (every gate with its numbers, the red controls, the scripts beside it).

### State of main now (@@MAINHEAD@@)
- **LOD terrain options: HYBRID (default) and DYNAMIC. FULL is ditched** (gone from CLI, panel and help).
  `--terrain-option hybrid|dynamic`, panel row "Terrain". HYBRID bakes only the 64/128/256 u texture levels and
  draws near/mid live from the .lodl; DYNAMIC bakes no terrain texture.
- **Outside paint: Vanilla (default) / Rule.** Vanilla (law 2): outside our painted ground the far terrain is
  vanilla's own dim-4 LOD diffuse, untouched; ours rises over it across an 8,192 u per-texel band inside our edge (no
  whole-cell steps, no dirt outline). Rule (`--outside-paint rule`, panel row "Outside paint", ships OFF): the outside
  is painted with the worldspace's own landscape textures by slope, height and best match to vanilla's colour.
- **`.lodi` version 13: group ids are file-wide** (u32, dense from 0), so a group that crosses a chunk line (Diamond
  City) is one id in the file. Older v3..v12 files still load. `WW_LODI_GROUPS_PER_CHUNK=1` writes the v12 file.
- **Landmarks are one group each** (res/lodgen_landmarks.txt: Diamond City, Hub towers east and west, Trinity Church;
  `--landmarks <file>|none`), with their footprint: a piece inside a landmark's outline joins it (<= 512 u out).
  Occluder boxes: probe fix (0 of 517 poke > 1%), and a group wider than 4,096 u gets one box per 4,096 u square.
- **New files:** `.lodd` + `.lodg` = projected road / flat-object decals, written by EVERY bake (`--decal-check`);
  `.lodr` = the rule paint map, only with `--outside-paint rule` (`--rule-check`). The `.lodb` records the terrain
  option (`terrain hybrid|dynamic`) and, when on, `outside rule`.
- `--terrain-preview <spec.json>` renders and times the options offscreen.

### The installed bake is the DITCHED one -- owed a re-bake
mods\FO4CSLOD still holds MERGE1's whole-map bake of 2026-09-29 00:58-02:52: the FULL option (every .lodt level)
with law 1 (the old per-cell fill: whole-cell steps, dirt outline), `.lodi` v12, no decals, `.lodl` v3. It was not
touched by this lane. It is owed a whole-map HYBRID re-bake + install, AFTER the rock tuning lane (the rule paint
picks rock less often than Bethesda painted it). The MERGE1 backup folder
(E:\Projects\Fallout 4 Mods\backups\FO4CSLOD_before_MERGE1_20260929) is still the way back to the pre-campaign install.

### Merge
- Branch merge2-20260929 from origin/main bc8f6f7e (bungo's rulings commits d07c52e4..bc8f6f7e kept): ident2-20260929
  @ 9597d868, then terrlive1-20260929 @ d2dad00b. **Both merges clean, no conflicts.** The two lanes share one source
  file, src/nifcli.cpp, in hunks that do not touch; IDENT2 never touches lodgen.cpp / lodgenmanager.cpp / the terrain
  doc. MISTAKES.md (main appended at the end, IDENT2 at the top) auto-merged.
- main fast-forwarded to the merge, then this ledger splice.

### Gates on the merged exe (sha1 1e961ba5bdb6; only those the merge reaches)
@@GATES@@

### Main's deployed exe
@@EXE@@

### bungo's rulings (carried from MERGE1's block; status after MERGE2 below them)
@@RULINGS@@
Status after MERGE2: 1 (landmark rule) and the IDENT1 box pokes: DONE by IDENT2, merged. 3 (TERR2) no longer waits on
TERRLIVE1. 4 (TILING6) hooks into `ltexFetch()` in src/terrainpreview.cpp. 5: the options are HYBRID + DYNAMIC now
(FULL ditched, see above); decals exist (.lodd/.lodg). 6: the decal bake now exists, so the FO4CS readers are due.

### Owed (pick up in this order)
1. bungo: restart NifSkope (his open window runs the old exe); look at TERRLIVE1's pics2/ + pics3/ and IDENT2's pics/.
2. Rock tuning lane (the rule paint picks rock too rarely), then the whole-map HYBRID re-bake and install (above).
3. FO4CS readers (FO4CS last, by standing order): `.lodl` v3, `.lodi` v12 AND v13 (u32 file-wide group word),
   `.lodd`/`.lodg` decals, `.lodr` rule paint, texture sets with no emissive, collapsed one-value tiles, live splat.
4. TERRLIVE1's open items: the "draw vanilla here" reader contract for the 577 MB of all-vanilla HYBRID tiles
   (bungo's call); live-splat colour (live 3-5 lum darker than baked deep inside, dark blocky patches); box culling at
   eye level; a black L-shaped line in the west close-up (before and after, not chased); 2 quadrants painted only by a
   NULL LTEX (bake 15,893 vs .lodl 15,891); close-range AO choice; LTEX 000464c5 has no texture path.
5. IDENT2's open item: the row-house far-shadow check, when that harness is next set up for a new chunk.
6. lodgen_native.sh leg 5 ("unaccounted: products", INCR2's .lodb rows have no group in
   tests/spells/lodgen_btofree_ledger.py): needs a decision which group they belong to.
7. Lanes TILING6, then TERR2; the rest of MERGE1's queue (FO4CS near-ground anti-tiling, SHADOW1 parked, speed lane).

### Lane lines (spliced by MERGE2 from each lane's DELIVERABLE_TEXT; newest first)
- **TERRLIVE1** (branch terrlive1-20260929 @ d2dad00b, merged): two terrain options (HYBRID default, DYNAMIC); FULL
  ditched. Law 2 at the painted edge, cause measured (the old fill was per cell, so empty quadrants of edge cells and
  the first 2-4 km outside kept the engine-default ground, lum ~67 against vanilla's ~80): now per quadrant, outside =
  vanilla untouched, ours rises over an 8,192 u per-texel band (one constant LODGEN_VT_FILL_BAND for bake and preview).
  Edge gate (edge/gate_law2.py): outside |ours - vanilla| 1.39 / 1.48 lum, outline dip 0.00 / 0.27 (north / west) PASS;
  law 1 read 6.61 / 6.45 and 10.55 / 9.53 FAIL. Whole map HYBRID 913.8 MB (.lodt 749.5 MB + decals 164.3 MB), bake
  1,332 s; DYNAMIC ships the decals only and reads vanilla's sheets live. Preview GPU ms HYBRID / DYNAMIC: Boston 0.107
  / 0.146, street 0.184 / 0.195, whole 0.405 / 0.534. Live/baked crossover on wholly-ours ground: oblique 30,720 u.
  Rule paint outside (optional): .lodr 2.6 MB (512 u samples, 2 textures a sample from the worldspace's own 36 LTEX);
  +214 s HYBRID bake (+38 s DYNAMIC), GPU up to +0.06 ms whole map live, +0.11 ms on a low view over outside ground;
  edge gate with the rule clauses PASS (dip 0.00 / 0.54, step 0.78 / 0.10); drift from vanilla outside mean 5.1 lum
  (p95 21), far hills lose some grey rock to brown/olive. OFF = law 2 byte for byte (all 7 whole-map files).
- **IDENT2** (branch ident2-20260929 @ 9597d868, merged): named landmarks one group each, no size cap, with their
  footprint (convex hull of the named pieces; per piece, <= 512 u out joins, trees never). Boston: Diamond City 15 ->
  1 group (404 pieces), west Hub 4 -> 1, east and Trinity 1; row houses stay 1 group of 54. `.lodi` v13 file-wide u32
  group ids: Diamond City one id (1178) over both chunks; all groups 4,613 -> 4,527. Occluder probe fix + split:
  517 boxes, 0 over 1 % (was 5 of 511), street coverage 0.560 -> 0.588. Pixel gate 0 wrong pixels in all 4 outlines
  (red control, the v12 file: 49,904). Way back (`--identity-join proximity --occluder-fit piece` +
  WW_LODI_GROUPS_PER_CHUNK=1) = main's bytes, 233 files. Refusals for v13 (id past groupCount, unused id,
  groupCount+1, stride 2, no group table) refused by both readers; old v12 files load.
