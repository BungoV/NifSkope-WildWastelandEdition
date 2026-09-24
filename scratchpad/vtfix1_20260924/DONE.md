DONE -- lane VTFIX1 (LOD-B), 2026-09-24, branch vtfix1-20260924 from 47b2cad. Exe sha1 0518d24348db5f789538607188910daa454eef77.

# 1. Skills loaded
nifskope-ww-lodgen, nifskope-ww-build-verify, ww-test-harness-add, search-lean. Written by this lane:
nifskope-ww-worktree-build (new). nifskope-ww-lodgen got a new section on testing a default flip.

# 2. What was built
All in src/lodgen.cpp (commit 25a36ed):
- **Item 1, maskRules.** `LodgenVtMaskCache::resolve` now counts form 0 (the null LTEX) under the rule that
  serves it, which is none-default. So pbrm + legacyInverted + noneDefault == distinctLtex.
- **Item 2, blend axes.** `lodgenBlendVanillaDetail` now reads and writes the `_msn` word the way
  lodgenTerrainMsnPixel packs it (east in bits 16-23, up in 8-15, north in 0-7). The swap was real: the
  known-answer run put east detail into north. It changes only bakes run with
  `--land-detail-source vanilla-blend` where vanilla's `_msn` exists. The default (vanilla, a copy) and
  `none` do not move.
- **Item 4, generator identity.** The chunk input digest lives in lodgen.cpp (`lodgenChunkInputDigest`), so I
  fixed it there. `lodgenGeneratorIdentity()` feeds `generator <sha1 of the running exe>;` first. That covers
  every default (nifcli lg* locals, lodgen.h initialisers, g_* globals) and every code change that moves bytes.
  It can only over-rebake. The switch digest and the dirty comparison stay in nifcli.cpp (~4238 read,
  ~4884 write), which INCRGATE1 owns. A chunk dirtied this way is reported as "inputs moved"; a separate
  "generator changed" reason would be INCRGATE1's to add.
- **Item 3, docs** (commit c29547d):
  - LODGEN_PARITY.md now says what the code does: `--terrain-identity` is OFF and only colours the .BTR;
    `--identity` is OFF and covers objects; the manifest is written either way.
  - LODGEN_TERRAIN_VT.md: the stale nifcli line anchors are fixed and the null-LTEX census has a note.
  - The docs/LODGEN_LEDGER_FORMAT.md row 8a text is PARKED in fix03_ledgerdoc.py. That file is not mine,
    and INCRGATE1 works next to it.
- New spell tests/spells/lodgen_vtfix.sh (G1 through G3) and its checker lodgen_vtfix_check.py.
- **Item 5, listed only, not done:** VTNORMAL1's BC1 normal fix, sparse empty VT tiles, 2K vs 1K sheets.

# 3. Gates (rung = release/NifSkope.before_vtfix1.exe bb60a7ea; new = 0518d243)
| gate | rung (red) | new |
|---|---|---|
| G1 maskRules sum vs distinctLtex, whole Commonwealth --vt | FAIL 100 != 101 | PASS 101 = 101 (noneDefault 1 -> 2) |
| G2 east-only fixture, blend detail axis | FAIL east(R) 4.21, north(B) 77.15 | PASS east(R) 78.74, north(B) 4.89 |
| G3 default flip in a patched exe copy, then --incremental | FAIL 0 of 2 chunks dirty, 4 stale files | PASS 2 of 2 dirty, output == full flipped bake |

- **G3 floor:** the flip moves 4 of 12 files in a full bake.
- **Kept green on the new exe:**
  - lodgen_terrain_vt 45/0
  - lodgen_terrain 26/0
  - lodgen_slab 16/0
  - lodgen_terrain_pbrm 14/0
  - lodgen_incremental 0 failures
  - lodgen_bakerec PASS
  - lod_generation PASS, 128 checks (floor 121)
- **lodt_write.sh no longer exists.** `--lodt` is retired; its successor `--lodl` writes
  Commonwealth.lodl byte-identical to the rung (35,953,294 B).
- **Heightmap:** byte-identical to the shipped FO4CS reference.
- **lodgen_defaults: 31 checks, 1 failure.** The failure is "(d) no C lines with identity off", 0 impostor-card
  lines in both arms. The rung fails it the same way, so it predates this lane and sits in the card region.

**Which outputs move:**
- **Whole-map VT bake:** 6 files, 5 byte-identical. The .VT.lodm differs by one byte, noneDefault 1 -> 2.
- **Chunk 4.-20.24, none arm:** all sheets, the BTO, the BTR and the manifest are identical.
- **Chunk 4.-20.24, blend arm:** only `_msn` moves.
- **Every .lodb:** the `inputs` digests now differ between exes, by design. Same-exe comparisons are
  unaffected.

# 4. Exe + commits
- release/NifSkope.exe sha1 0518d24348db5f789538607188910daa454eef77 (21:23)
- rung sha1 bb60a7ea9344b4a2bab342df8e05a5162dfd5c5a
- commits: 25a36ed (code, checker, lane scripts), c29547d (spell, docs, parked ledger-doc patch). Not pushed.

# 5. What the final bake needs from this lane
- Merge 25a36ed + c29547d.
- **The first --incremental on the merged exe rebakes every chunk once**, because the generator word is new.
  Plan the whole-map bake as a full bake.
- The .VT.lodm census now closes (101 = 101).
- If the final bake uses `--land-detail-source vanilla-blend`, its relief now leans the right way.
- Apply fix03_ledgerdoc.py after INCRGATE1 lands.

# 6. Skill review
- **nifskope-ww-lodgen:**
  - It still names tests/spells/lodt_write.sh (gone) and `--lodt` (retired for `--lodl`). Its CLI table needs
    a pass; I added a note, not a rewrite.
  - Added: how to test a default flip with a patched exe copy (UTF-16 literals), and that .lodb files never
    match across exes now.
- **New skill nifskope-ww-worktree-build:**
  - covers the qmake stash, object reuse, the runtime copy and the gated build script
  - tools/ww_build.sh cannot be used in a worktree
- **nifskope-ww-build-verify:** accurate. The only gap was worktrees, now covered by the new skill.
