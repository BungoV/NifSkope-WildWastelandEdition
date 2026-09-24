## HANDOFF text

**VTFIX1 (LOD-B), 2026-09-24: DONE.**
- Branch vtfix1-20260924, commits 25a36ed and c29547d.
- Exe sha1 0518d243; rung sha1 bb60a7ea.

What the lane changed:
- **maskRules census closes.** The null LTEX (form 0) now counts under noneDefault, so the whole Commonwealth
  reads 101 = 101; it was 100 vs 101.
- **vanilla-blend adds detail on the right axes.** East detail had been landing in north. This changes only
  `--land-detail-source vanilla-blend` bakes; the default does not move.
- **Chunk input digest starts with the generator's identity** (sha1 of the running exe). `--incremental` now
  rebakes after a default flip or a code change. The first incremental run on any new exe rebakes everything
  once.

Gates (tests/spells/lodgen_vtfix.sh), rung vs lane exe:
- **G1:** 100 != 101 vs 101 = 101
- **G2:** north 77.15 vs east 78.74
- **G3:** 0 of 2 dirty vs 2 of 2 dirty

Kept green:
- terrain_vt 45/0
- terrain 26/0
- slab 16/0
- terrain_pbrm 14/0
- incremental
- bakerec
- lod_generation 128
- .lodl and the heightmap byte-identical

Open items:
- lodgen_defaults (d) "no C lines with identity off" is red on the rung too: 0 card lines. It predates this
  lane and belongs to the card region.
- The docs/LODGEN_LEDGER_FORMAT.md row 8a is parked in scratchpad/vtfix1_20260924/fix03_ledgerdoc.py for after
  INCRGATE1.
- INCRGATE1 may want a "generator changed" dirty reason in nifcli.cpp's comparison.
- Owed rulings, not done: VTNORMAL1's BC1 normal fix, sparse empty VT tiles, 2K vs 1K sheets.

## WW_CHANGES text

**LOD generator: terrain pyramid fixes before the whole bake (lane VTFIX1, 2026-09-24).**
- **The mask-rule census in a `.VT.lodm` now adds up.** The null land texture (form 0) is counted under
  `noneDefault`, so `pbrm + legacyInverted + noneDefault == distinctLtex`. On the whole Commonwealth it reads
  101 = 101. No sheet byte changes.
- **`--land-detail-source vanilla-blend` now puts vanilla's fine relief on the right axes.** The blend had
  swapped east and north when it unpacked the terrain normal word. The default (`vanilla`, a straight copy)
  and `none` are unchanged.
- **`--incremental` now notices a new generator.** Every chunk's input digest begins with the sha1 of the
  running NifSkope. A changed default or a code change that moves bytes dirties the chunk, where before the
  old bytes were kept as clean. The first incremental run on a new build rebakes everything once.
- **Docs:** LODGEN_PARITY now matches the code on the two identity switches (both off by default; the
  manifest is written either way). LODGEN_TERRAIN_VT's source anchors are refreshed.
- **New gate:** `tests/spells/lodgen_vtfix.sh` (G1 census, G2 known-answer blend orientation, G3 default flip
  vs `--incremental`). Each check was shown failing on the previous exe.

## MISTAKES text

- **2026-09-24, VTFIX1: blend east/north swap.** `lodgenBlendVanillaDetail` unpacked east from bits 0-7 and
  north from 16-23, the opposite of `lodgenTerrainMsnPixel`, and wrote them back into the same wrong slots.
  - Why it hid: nothing looked transposed, the unit-length recompute is symmetric in the two, and no gate fed
    it an asymmetric input.
  - The lesson: a pack/unpack pair needs a known-answer test with a one-axis input.
- **2026-09-24, VTFIX1: null LTEX never counted.** The mask cache stored form 0 without counting it, so a
  census advertised as an identity (`sum == distinctLtex`) was false on the one bake large enough to hit
  form 0.
  - The lesson: an identity written into a doc gets a gate on the largest input, not only the Sanctuary slice.
- **2026-09-24, VTFIX1: ledger never recorded the generator.** The incremental ledger digested inputs and
  argv but not the generator, so a default flip left every chunk "clean".
  - The lesson: a cache key must include the code that turns inputs into bytes.
- **2026-09-24, VTFIX1: stale gate reference.** The lane brief listed `lodt_write.sh` among the gates, but the
  spell is gone and `--lodt` is retired for `--lodl`.
  - The lesson: check a gate list against `tests/spells/` before the sweep.

## Skill review

- **nifskope-ww-lodgen:**
  - Its CLI table and gate list still name `--lodt` and `lodt_write.sh`. A note was added; the table itself
    needs a pass.
  - New section: testing a default flip with a patched exe copy. Qt literals are UTF-16LE, and `--data-root`
    must be spelled on every run.
  - New note: `.lodb` files never match across exes.
- **nifskope-ww-worktree-build (new):** how to stand up a lane worktree in about two minutes.
  - the qmake stash
  - reusing the main tree's objects only when they match the branch commit
  - the runtime copy
  - a gated build script that renames only the worktree's own exe
- **nifskope-ww-build-verify, ww-test-harness-add and search-lean** were accurate for this lane.
