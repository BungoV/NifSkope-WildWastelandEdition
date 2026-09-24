# PLANSYNC1 deliverable text, 2026-09-23

These are contract-page defects the lane found while re-syncing `docs/FO4CS_IMPROVED_LOD_PLAN.md`. The lane was
allowed to edit that page only, so none of these is fixed. Each one needs a docs lane, or the overseer, to decide
it. The evidence is in the four audit files in this folder, which give line numbers and anchors.

## Changelog line (for the overseer to splice into WW_CHANGES)

2026-09-23 PLANSYNC1 (docs only): `docs/FO4CS_IMPROVED_LOD_PLAN.md` was re-synced with the writer and the contract
pages. It gets a new "CHANGED SINCE 2026-09-16" block with 61 corrections, and the §8 version table was rewritten
from source:
* `.lodo` v4 only;
* `.lodi` 3-9, default v7 with a 512 B header;
* `.lodl` default v2.

R0 gate 1 is re-pinned to the authored-only default bake (6,204,388 B, 10,634 clusters, levelMax 0). Other changes:
* the R0 hard list is completed, and pairing is HARD;
* the §8.5 clipmap ruling is folded into R2;
* an R4 card-law block is added;
* §5 rows 20-28 and §6 (o)-(u) are new;
* contract page and source stamps are re-hashed.

The page grew from 1,605 to 2,119 lines. No other page and no source was touched.

## Defects in contract pages

### LODGEN_NATIVE_LODO_LODI.md (13)
1. The title and banner still say v4/v6. The writer runs `.lodi` to v9 (default v7).
2. §3.2 still names `crossPx16[4]`. In v4 the field is `fullTriangles`.
3. §3.5.7 still says the near library is the default. It was reversed 2026-09-17: `--library mnam`, no ladder.
4. §4.1: the bit-6 (scrappable) wording is incomplete. It is refused below v9.
5. §4.4 line ~912 still says the far-shadow identity is the instance index. It is the GROUP (v7), per §4.9.
6. §4.4 says `crossPx16` is written 0, but the field no longer exists in v4.
7. §4.6.1 cites Deviation 6 where Deviation 12 is meant.
8. §4.9 documents only the legacy 16-unit join. The default is the proximity join, `--identity-join`, gap 64
   (`src/nifcli.cpp`, `float lgIdentityJoinGap = 64.0f;`).
9. §6 still quotes 9,710,564 B. Today's default `.lodo` is 6,204,388 B.
10. §12 reserved-byte range 0xF1-0xFF does not match the v7+ header, which is 512 B with group and sky words.
11. A cited anchor at line 5796 has moved to 8059 (`--native-no-ladder`).
12. §5's hard-refusal list is incomplete against `lodoRead` / `lodiRead`, which has pairing, `cardCount`,
    WATERTIGHT, group, sky, cell band and more (full list: `audit_native.md` part 4). The §4 header row 0x90 calls
    a pairing mismatch soft; `src/nativeemit.cpp` refuses it hard.
13. The row-6 cell-quantisation band (`lodiCellAgrees`, `LODI_CELL_QUANT_TOL`) is in code but not documented.

### LODGEN_TERRAIN_VT.md (5)
1. §4 prints the old `Data\Terrain\` path for the pyramid and index.
2. §5's CLI table lists the sampler defaults as off. Since DEFAULTS1 (2026-09-12) they are hex 256, warp 341,
   mip bias -0.22 and flatwarp 1.0.
3. §3.4 rule 13 says 1-6 sheets and refuses roles above 6. The reader accepts up to 10 sheets and role 7.
4. §2.3 says the AO march reaches 2,048. The effective reach is 1,458 (§2.2, census `objAoReach`).
5. A §5 row cites "§3.6", which does not exist.

### LODGEN_CARD_SHEETS.md (3)
1. §1.1 and the anchor table use the old Cards output path.
2. §2 says "the centre frame is the exact top". That holds only for odd N.
3. §2 does not specify the diagonal of the three-frame blend's triangles. The viewer fixes it as a SPEC GAP in
   `src/impostoroct.h`.

### LODGEN_LODM_FORMAT.md (1)
1. §3 has no row for the `conv` key (set and layer), which has been written since 2026-09-19.

### LODGEN_IMPOSTOR_SPEC.md (1)
1. The frame-law section still prints the retired five-rung aspect ladder and 46.4 %. CARDS 3.2 and 3.5 give
   multiples of 16 and 42.9 %.

### LODGEN_CENSUS.md (3)
1. The §2 `version` field still reads `lodo3/lodi3` with `refused:v1/v2`, and has no slot for `.lodl` or `.lodt`.
2. The `unmeasured` default used in §5.3 is not defined in §1.2.
3. The §7 figure 59/0/31 was measured on v3 pairs that today's decoder refuses, so it cannot be repeated.

### LODGEN_BTD_FORMAT.md (1)
1. The bit list at lines ~200-202 has no bit 8.

## Source strings (not docs, for a source lane)
* The `src/lodofile.cpp` v1/v2 refusal messages still say "this reader knows version 3". The reader knows 4.
* The aspect comment in `src/nifskope_ui.cpp` still describes the retired aspect ladder.
