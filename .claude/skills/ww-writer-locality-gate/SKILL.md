---
name: ww-writer-locality-gate
description: Gate a NifSkope WW .lodl writer change that is MEANT to change real data in a few places (a new input the vanilla bake turns out to have) -- instead of a byte-identity gate whose premise the real data breaks. Decode both files, match records by content not index, and prove every difference lies in the feature's own cells, with a pre-registered footprint, an amendment rule, and a floor. Use when a byte-identity or "+N offsets only" comparison of old vs new bake fails because the new input exists in vanilla.
---

# A writer change that should touch only its own cells

Written by lane WATER1 (2026-09-27, task 3, sloped water from placed meshes). Its gate `lodl_cmp.py` said:
"old and new differ only by the header growing 8 bytes". The real Commonwealth has 6 sloped placed water
meshes, so the new file also had +1 WATR form and +7 bodies: offsets moved +12, the gate failed, and its
floor failed too (a floor on a failing subject proves nothing). The premise, not the code, was wrong.

## The procedure
1. Read the new bake's census first (`placed water: N ref(s) ..., K sloped used`). K > 0 means the real data
   exercises the feature: a byte-identity gate between old and new writers is the wrong question.
   The byte-identity question still belongs to a fixture with the feature absent (the selftest's "no ribbon"
   file) or to an OFF switch.
2. Decode, never diff raw: offsets, table sizes and IDs all move. `scratchpad/water1_20260927/real_cmp.py`
   decodes the WATR table, the body table (48-byte records), the three plane containers (tile directory of
   u64 offset / u32 csize / u32 uniform value, zlib tiles; body-ID and flow 2 bytes a sample, shore 1 --
   read `bps` from the container, do not assume) and the 16-byte cell records.
3. Match records by CONTENT: body IDs are assigned by descending area, so one new body renumbers every
   smaller one (251 of 348 in WATER1's case). Match old->new by (class, WATR form, height, bbox), then compare
   the body-ID plane through that map.
4. Pre-register the footprint S from the NEW file's own feature data (cells with a non-uniform surface tile)
   and a reach (2 cells for shore distance). Every differing cell must be within reach of S.
5. When the first run fails, find out why before amending, and label the amendment as written after the run.
   WATER1: a 1-texel sloped body has surface offset 0 at its only texel (its height IS its lowest surface),
   so its tile is uniform and S missed it; the amendment adds the cells of bodies the old file lacked.
6. Floor: flip one decoded sample far from S in the old file: must FAIL. Report both the pre-registered FAIL
   and the amended PASS.

## Pictures
The same idea for renders: `pixcell.py` (skill ww-picture-point-to-cell) masks the projected cells and fails
on any differing pixel outside. Region-relative colour ramps and area-ordered IDs change everywhere by design;
say so with the legend lines, do not loosen the mask.
