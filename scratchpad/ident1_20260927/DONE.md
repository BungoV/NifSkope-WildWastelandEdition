# IDENT1 -- one id per building, and occluder boxes per building (started 2026-09-27 04:37, `date`-read)

Worktree E:\Projects\NifskopeWWE-ident1, branch ident1-20260927 from night-20260927 @ 8f58e7db.

## 1. Skills loaded
nifskope-ww-lodgen, search-lean (more added below as they are loaded).

## 2. What was wrong (audit1 ranks 3 and 6; sections 2.2-2.4; pictures 04, 05, 10, 25)
- Rank 3: the building identity (.lodi 4.9 group table) is dense PER CHUNK, so a building crossing a chunk line
  is cut (Diamond City in 4 groups); the 64-unit mesh-sample join welds neighbours (ballpark group 420 pieces,
  row-house group 530 pieces over 12,696 u).
- Rank 6: occluder boxes (.lodi 4.5) are wall slabs (median 5 u thick), 340 in the audited region, no hills.
- Views: 05 `identityraw` is the low byte of the piece id; 10 `placement` reads as one texture.

### 2a. STOP on "one id space for the whole file": the group word is too small (measured 04:50)
The brief: "if the group table cannot hold file-wide ids in its current layout, stop that step and report
exactly what field is too small". It cannot.
- **The field: `.lodi` v7 group table entry, `u16 group[instanceCount]` at header 0x100, `groupStride` = 2 at
  header 0x10C** (docs/LODGEN_NATIVE_LODO_LODI.md 4.9; writer src/lodifile.cpp:653-697; reader :1430-1461
  refuses any stride but 2 and any id at or past its chunk's placement count).
- Measured on the installed whole-Commonwealth file (E:\Projects\Fallout 4 Mods\mods\FO4CSLOD\FO4CSLOD\Commonwealth,
  read only; script scratchpad\ident1_20260927\treecount.py): 184,431 placements, 85,582 of them trees.
  A tree never joins (4.9 clause 3) except through its own SCOL, so the trees ALONE need **60,768** distinct
  file-wide ids (every non-SCOL tree one, every tree SCOL one). The same file holds 17,096 multi-placement groups
  and 66,126 per-chunk groups in all. A file-wide id space therefore needs about 60,768 + the building groups
  (several thousand, more with a tighter join) > **65,536**, the u16's range.
- So the ids stay dense per chunk in this lane. What the fix needs (for the overseer to fold into GROUND1's
  version bump, not done here): the group word to u32 (`groupStride` 4), ids dense over the FILE, the reader's
  per-chunk density check replaced by a file-wide one. The emitter already computes one GLOBAL key per placement
  (src/nativeemit.cpp `groupKey`), so only the writer and reader change.
  (Alternative with the u16 kept: a reserved "alone" id 0xFFFF for trees and singletons, which leaves 65,535 ids
  for multi-placement groups; also a semantic change that needs the bump.)
