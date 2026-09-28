---
name: ww-bc-sheet-mask-gate
description: Gate "channel X changed only where Y was written" on BC1/BC3-compressed NifSkope WW sheets (.lodt VT msn, colour, mask B) -- judge whole 4x4 blocks, take the mask from the producer's own record (a diagnostic exe's per-texel write log), never from another compressed sheet, and give the gate a planted and a shifted refuter. Use for any "off-mask identity" gate between two bakes, and before believing a per-texel reading of a compressed sheet.
---

# Off-mask identity on compressed sheets

Written from lane TERR1 (2026-09-28). Its gate G2 ("the ground normal sheet changes only inside the roads mask")
failed with 324 blocks for two days; the stamp code was right, the gate was wrong twice over.

## Trap 1: another compressed sheet is not the mask
G2 took "the roads mask" = blocks whose COLOUR sheet bytes differ roads vs no-roads. A road whose colour is within a
few levels of the ground (measured: median 5.6 levels, some over 8) at ONE texel of a block does not move BC1 bytes
(565 endpoints, 4 palette entries), while the msn, where the normal moved 10-80 degrees, does. The proxy under-reports
the real mask exactly where the two channels disagree in contrast. Symptom: every violator has colour diff 0 and sits
within 1 block of the mask (TERR1: 315 of 324).

## Trap 2: per-texel reading of a BC sheet
Writing one texel of a 4x4 block moves the block's endpoints, so its 15 block-mates move too with no write of their
own. A per-texel "was this texel written?" count on moved texels reports most of them unwritten (TERR1's first
reading: 2,180 of 2,588 "no record" -> a false "something else writes the sheet"). Always ask per BLOCK: does the moved
block contain at least one written texel?

## Procedure
1. Diagnostic exe: a patch that appends a fixed-size float record per texel the producer writes (world x, y, weight,
   and the competing channel's move BEFORE rounding), behind an env var; build it, reverse the patch, prove the source is
   clean (`grep -c` of the patch marker = 0). TERR1: `scratchpad/terr1_20260927/stamp_diag.patch`, `resume.sh diag`.
2. Bake once with it and prove its sheets equal the shipped bake's (`cmp`), so the record describes the shipped bytes.
3. Key check first: share of records on texels that surely were / surely were not written (TERR1 g2keycheck.py: quiet
   0.007%, changed 86%). A bad key looks exactly like trap 2.
4. The gate (TERR1 g2gate.py): blocks whose bytes differ ON vs OFF (mip 0, stored sheet incl. border) that hold no
   record texel = 0.
5. Two refuters, both must fire: a planted one-byte change in an unwritten, unmoved block = exactly 1; the record mask
   shifted by one block = many (TERR1: 17,324). Report the old proxy's count beside it.
6. The same shape for a law-identity claim on a BC channel (TERR1 opengate.py, mask B): gate only blocks whose 16 texels
   all satisfy the "nothing in reach" condition; report the coupled texels separately.

## Commands (TERR1)
```
python g2block.py  bakes/stamp_diag.bin on.VT.2.lodt off.VT.2.lodt noroads.VT.2.lodt g2block.json
python g2gate.py   bakes/stamp_diag.bin on.VT.2.lodt off.VT.2.lodt noroads.VT.2.lodt g2gate.json
python opengate.py on.VT.2.lodt off.VT.2.lodt bakes/objh_on.bin opengate.json
```
