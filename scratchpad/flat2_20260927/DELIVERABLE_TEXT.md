# FLAT2 -- ledger text for the overseer to splice (the lane writes no ledger itself)

## HANDOFF (NifSkope WW, top block line)
FLAT2 (2026-09-27, branch flat2-20260927): a terrain tile sheet that is one value over every texel of
every mip (open sea and the ring outside the worldspace: normal + height in the Commonwealth, all four
sheets in Nuka-World) is no longer stored in full in the `.lodt`. Tile flag bit 2+k marks sheet k; its
place in the payload holds a 16-byte record (the sheet's one BC block / one R16 texel, then zeros), and
repeating that unit gives the old bytes back exactly. Measured on the installed whole-map bake: 5.96 GB of
19.98 GB (29.8 %) is such sheets. On by default; `--no-collapse-uniform` (CLI only) writes today's bytes.
`.lodt` stays v2 (the mipSkip precedent: an old reader refuses the new flag bit by rule 16). The viewer and
every in-tree reader expand the record; the validator gained rule 16c (record pad zero, block one value).
Gated on two small bakes against the pre-change exe: the sea edge (48 of 180 sheets stored as one value, file
54.9 -> 34.1 MB) and Nuka-World's north edge (84 of 256, all four sheet kinds, 78.1 -> 52.6 MB): every file other
than the .lodt identical, the .lodt identical with the switch off, 0 texels differ after decode, the validator
refuses a doctored record, and the sea-edge render is the same picture before and after.
NOT FLOWN. Until the FO4CS reader change below lands, FO4CS refuses a default bake (unknown flag bit):
bake with `--no-collapse-uniform` for the game until then.

## WW_CHANGES.md
- Terrain texture files (.lodt): a tile's sheet that is a single value over the whole tile is stored as one
  16-byte value instead of the full sheet (open sea and the empty border: about 30 % of the whole-map bake).
  Nothing the viewer draws changes. `--no-collapse-uniform` writes every sheet in full, as before.

## FO4CS reader change list (owed; FO4CS is built last by standing order)
1. Tile table `flags`: accept bits 2 .. 2+sheetCount-1 (bit 2+k = sheet k is one value). Keep refusing
   every bit above them. A file without these bits is unchanged.
2. `rawBytes` of such a tile = sum over sheets of (16 if its bit is set, else the sheet's full size). The
   per-sheet offsets inside a tile's payload move accordingly: walk the header's sheets[] in order, adding
   16 for a one-value sheet and its full mip-chain size otherwise.
3. For a one-value sheet, read the 16-byte record: its first UNIT bytes are the sheet's repeating unit (BC1
   8, BC3 16, R16 2, R8G8B8A8 4); the rest are zero. Either expand (repeat the unit over every mip's full
   size, which gives exactly the uncollapsed bytes) or skip the upload and bind a constant: the writer only
   collapses a block that decodes to one value under any decoder (docs/LODGEN_TERRAIN_VT.md 3.2a), so the
   constant is the block's decode (BC1 c0 for index 0 etc.), identical on every mip.
4. Nothing else moves: header, level table, CRC rules and the v2 version word are as before.
