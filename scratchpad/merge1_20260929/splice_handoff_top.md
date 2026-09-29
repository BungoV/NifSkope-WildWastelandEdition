## TOP BLOCK -- written @@WHEN@@ (`date`-read) by lane MERGE1: the LOD map-fix campaign is MERGED into main, baked whole-map, installed; NOT flown

bungo's order (09-26): "we fix all the maps that are broken, overnight" / "after you're done with fixing bakes, merge
it, then render the bakes for me, this time fixed". Done by MERGE1 on 2026-09-29. Report:
scratchpad/merge1_20260929/DONE.md (every gate line with its numbers).

### What landed (main merge b918dc0b = night-20260927 @ b177294f)
Lanes WATER1, GROUND1, TIDY1, IDENT1, TERR1, TILING5 (switches OFF), FLAT2, GPU1, AO2 -- one line each below under
"Lane lines". Merges into night were clean (no conflicts); night into main was clean.

### Boston re-gate on the merged exe (sha1 6dc429d1)
- All ways back together (`--no-water-bodies --identity-join proximity --occluder-fit piece --no-stamp-normals
  --no-sky-objects --no-collapse-uniform`, WW_LODGEN_NO_VERTEX_GROUND=1, WW_LODGEN_KEEP_BLACK_EMISSIVE=1,
  WW_LODGEN_NO_LAYER_DEDUPE=1) = the pre-campaign rung, byte for byte, except the water-shape vertex colour
  (dropped at bungo's word, no way back; 164 non-water blocks identical) and bookkeeping (.key, .lodb).
- GROUND1, TIDY1, TERR1, FLAT2: the lanes' numbers reproduce exactly. IDENT1: same numbers, same 3 FAILs.
- Harness legs: lodl_channels 54/54, lodi_v7, lodl_write PASS. lodgen_native 3 fails: leg 3 was a stale check
  (flag bit 0x10 became NEAR1's NEAR flag) -- FIXED to 0x20; leg 5 "unaccounted: products" (INCR2's .lodb rows
  never given a group in tests/spells/lodgen_btofree_ledger.py) -- left red, the group is a decision; leg 13c =
  IDENT1's known box FAIL (6 of 302). lodl_water.sh FAILs its oracle (353 vs 346 bodies = the 7 sloped bodies
  the oracle does not model) and its header-layout lines (header is 0x100 now) -- owed to WATER1's owner.

### Installed
- Whole Commonwealth baked on the GPU WITH `--no-collapse-uniform`: 00:58-02:52 (6,800 s), GPU BC7 on (163 images, 0 fell back);
  3,060 chunks; stage 3,656 files / 15,905,725,868 B; installed = stage (521 copied, 22 stale removed, 3,135 equal).
- Backup of the whole installed mods\FO4CSLOD before anything: E:\Projects\Fallout 4 Mods\backups\FO4CSLOD_before_MERGE1_20260929
  (5,072 files, 21,038,690,722 B, verified both sides). Replaced files also kept in
  E:\Projects\Fallout 4 Mods\backups\FO4CSLOD_replaced_MERGE1_20260929.
- **The installed bake carries `.lodl` v3 and `.lodi` v12, which today's FO4CS refuses.** Until the FO4CS readers
  land, far LOD in game may be missing for the Commonwealth. To go back: restore from the backup folder above.

### Maps
One picture per map, full size, 60 px title bar: E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/maps/
(70 pictures: 35 renders at the MAPS1 Boston camera + 35 offline panels; not in git). 3 are empty by design: ground
cover (Fallout 76 only), the tree-card glow sheet (TIDY1 writes none when it is black), and the v2 cell-flags panel
(a v2 file flags every cell).

### Owed (pick up in this order)
1. bungo: look at the maps; fly the installed bake only after the FO4CS readers land (or restore the backup).
2. FO4CS readers (FO4CS last, by standing order): `.lodl` v3 (0x100 header, water bodies, sloped surface plane),
   `.lodi` v12 (ground contact per vertex), texture sets with no emissive, collapsed one-value `.lodt` tiles. Then the
   installed bake can drop `--no-collapse-uniform`.
3. IDENT1 needs bungo's ruling: Diamond City in 15 groups, the row houses welded into 1, 5 of 511 boxes poke > 1%.
   Also owed there: file-wide ids (wider group word), hill boxes (format bump).
4. TERR1: deepest canyons still darker than physical (Theater 53.9 vs 92.3). TILING5: next lever is the hex
   sampler's interior grain.
5. CELL1: bungo's InstaLOD runs and their import.
6. Queue unchanged from the 09-27 block: FO4CS near-ground anti-tiling, SHADOW1 (parked), next speed lane (terrain VT
   across all cores, hash while writing, AO tail).

