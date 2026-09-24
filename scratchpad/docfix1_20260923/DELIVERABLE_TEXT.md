# DOCFIX1 deliverable (2026-09-23 22:35)

Source: `scratchpad/plansync1_20260923/DELIVERABLE_TEXT.md`, "Defects in contract pages". Every item was
re-checked against the current `src/` before the edit. All six docs are still LF-only (0 CRLF, checked
with Python byte counts). Nothing is committed. Note: `git diff --numstat docs/` also shows other lanes'
uncommitted edits in these files. Commit by explicit path, and only after reading the diff.

## Status per item (23 in scope: 23 fixed, 0 already right, 0 stale; 1 needs a ruling)

| doc | item | status | source checked |
|---|---|---|---|
| BTD | 1 bit 8 dye | fixed | lodtfile.h:51 `LODL_SECT_DYE` |
| LODM | 1 `conv` key | fixed (§3 row + §4 layer list) | lodgen.cpp:3328-3336, 14305 |
| LODM | 2 `_n` format (added by the director) | fixed: §2.1 normal row now BC7; the colour (BC3), mask (BC3) and emissive (BC1) rows checked and unchanged | lodgen.cpp:3237 (colour), 3238-3248 (`_n` bc7=true, mask false), 3250-3253 (emissive), 4804-4815 (DX10/BC7_UNORM header) |
| CARDS | 1 Cards path | fixed (+ the same stale path in LODM §3/§3a and IMPOSTOR_SPEC) | lodgenlayout.cpp:39-42, lodgen.cpp:3257-3258, lodgenaggregate.cpp:184 |
| CARDS | 2 centre frame | fixed: odd N only (+ the same sentence in LODM §3) | impostoroct.h:139-145 |
| CARDS | 3 diagonal | fixed: (i+1,j)-(i,j+1) plus the weights | impostoroct.h:117-137, impostoroct.cpp:211-216 |
| SPEC | 1 frame law | fixed: multiple of 16, 42.9 %, ladder marked retired | nifskope_ui.cpp:23251 |
| CENSUS | 1 version row | fixed: quad `lodo4/lodi7/lodl2/lodt2` + a refusal word per file. **The spelling is my proposal; needs a ruling** | lodofile.h:78, lodifile.cpp:971-990, lodtfile.cpp:57-58, io/lodvfile.h:91 |
| CENSUS | 2 `unmeasured` | fixed (6th default, §1.2 rule 5) | - |
| CENSUS | 3 59/0/31 | fixed: marked historical (the pairs are LODO/LODI v3, measured from their bytes) | lodofile.cpp v3 refusal |
| NATIVE | 1 title/versions | fixed (+ a VERSIONS TODAY banner) | lodofile.h:78, lodifile.h:243 |
| NATIVE | 2 base row | fixed (+ mesh flag bit2 WATERTIGHT) | lodofile.h:189, 294-312 |
| NATIVE | 3 §3.5.7 default | fixed (REVERSED 2026-09-17 banner) | nifcli.cpp:7465, 7497 |
| NATIVE | 4 flag bit6 | fixed (scrappable, v9 only) | lodifile.h:364-366, lodifile.cpp:1596-1607 |
| NATIVE | 5 far shadow | fixed (keys on the group) | nativeemit.cpp |
| NATIVE | 6 crossPx16 | fixed. **The item was partly wrong:** crossPx16[2] still exists and is written 0 | nativeemit.cpp:1672-1673 |
| NATIVE | 7 Deviation 12 | fixed | lodifile.h:93 |
| NATIVE | 8 identity join | fixed: proximity (gap 64) is the default; the legacy rule is relabelled; 167 vs 588 groups read from the `.lodi` 0x108 bytes | lodgen identity join |
| NATIVE | 9 §6 size | fixed: 6,204,388 B, md5 89407121f640, measured in 3 bakes | measured |
| NATIVE | 10 §12 free room | fixed: header is 512 B on v7+, 0x11C-0x1FF = 228 B | lodifile.cpp:56-78, 1229-1238 |
| NATIVE | 11 nifcli anchors | fixed. The anchors moved again (8101, 8107, 3859) | nifcli.cpp |
| NATIVE | 12 §5 refusals | fixed. **The item's wording was inverted:** §4 row 0x90 already called a between-file hash mismatch hard. The defect was that §5 listed it only as soft. §5 now has hard rows for both files, pairing, `.lodo` and `.lodi`; the soft row covers live data only | lodofile.cpp:1727+, lodifile.cpp:949+, 1068-1071, 1232-1238, nativeemit.cpp:2976-2989, 3023-3037 |
| NATIVE | 13 cell band | fixed (§4.1: `lodiCellAgrees`, about 0.1289 u) | lodifile.cpp |

## Owed after VTNORMAL1 (docs/LODGEN_TERRAIN_VT.md, not touched)
1. §4 prints the old `Data\Terrain\` path for the pyramid and index.
2. §5's CLI table lists the sampler defaults as off. Since DEFAULTS1 they are: hex 256, warp 341, mip bias -0.22, flatwarp 1.0.
3. §3.4 rule 13 says 1 to 6 sheets. The reader accepts up to 10 sheets and role 7.
4. §2.3 says the AO march reaches 2,048. The effective reach is 1,458.
5. A §5 row cites "§3.6", which does not exist.

## Source strings (for a source lane; not edited)
* `src/lodofile.cpp` v1/v2 refusal messages say "this reader knows version 3". It knows 4. (re-listed)
* The `src/nifskope_ui.cpp` aspect comment describes the retired ladder. (re-listed)
* NEW: the `src/lodifile.cpp` v1/v2 refusal messages say "knows version 3 only / 3 and 4". It reads 3 to 9.
* NEW: the "Deviation 6" comments at `src/lodifile.h:93` and `src/lodifile.cpp:725, 982` are stale.
* NEW: `lodiRead` does not sweep header bytes 0x10E-0x10F (lodifile.cpp:1232-1238).
* NEW: the Python decoder's `CELL_QUANT_TOL` uses 2^-23 and C++ uses 2^-22, so the two tolerances differ by about 0.002 u.
* The rest of the NATIVE "Source, as built" table's line numbers are stale and were not re-derived.

## WW_CHANGES text
DOCFIX1 (2026-09-23): the far-field contract pages now match the source.
* `.lodo` v4 and `.lodi` v7 (reads 3 to 9) are named, and the §5 refusal table is split into hard rows for both files, pairing, `.lodo` and `.lodi`.
* The authored-only default and the proximity identity join are documented.
* Card paths are now `Data\FO4CSLOD\Cards` and `<ws>\Aggregate`. The LODM page gives the card `_n` sheet as BC7. The card pages cover the odd-N centre frame, the grid diagonal, the multiple-of-16 aspect (42.9 %) and the `conv` key.
* The BTD page has the dye flag bit. The census `version` row covers all four files.
* The census 59/0/31 figure is marked as a v3 measurement from history.

## HANDOFF text
DOCFIX1 done, uncommitted: 23 of 23 doc defects fixed in six LODGEN pages. The 5 TERRAIN_VT items are owed after VTNORMAL1. There are 6 source-string items for a source lane (listed in scratchpad/docfix1_20260923/DELIVERABLE_TEXT.md). Ruling owed: the census `version` quad spelling.

## MISTAKES text
Two items in the PLANSYNC1 defect list were wrong about the source: crossPx16 was said to be gone, but it is written 0, and the loadOrderHash hard/soft direction was inverted. Re-verify every audit item against the source before editing; the list itself is not the truth.
