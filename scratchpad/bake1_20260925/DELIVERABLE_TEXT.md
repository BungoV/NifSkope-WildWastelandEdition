# BAKE1 deliverable text (director splice)

## HANDOFF text
BAKE1 (2026-09-25) DONE: the whole Commonwealth is baked from bungo's 47-plugin MO2 order into mods\FO4CSLOD.
- Contents: 3,677 files, 15.96 GB. VT with height + cover, native objects, 79 tree cards (42 from BNS Trees),
  all four rings.
- G1-G5 green, each with a red control.
- Branch bake1-20260925: 24f5f39b, ad482f49, 23cde121, e27fe279, 77f6eae5. Run-copy exe 27a7bb29.
- Pictures: scratchpad/bake1_20260925/pics/ (INDEX.md, contact sheet).
- bungo adds +FO4CSLOD to his modlist himself; nothing in MO2 was touched.

Owed:
- (a) The panel has no cover row under the FO4CS target. The whole-map bake runs from the CLI; see skill
  ww-whole-map-lod-bake.
- (b) FORCE_CARD is 0 on a whole map, because ring 4 is every tree's kept arrival. The card reader must pick
  cards by ring from the base's card layer. This needs a ruling from the card-link owner.
- (c) The proximity identity join is serial and takes 29 min of the 96-min bake. Make it parallel or bound it.
- (d) Three new skills name the owner and the symbol source. Scrub them before copying to AISkills.

## WW_CHANGES text
Whole-map bake fixes (lane BAKE1, 2026-09-25):
- ESM reader: a later plugin's own world group and the cell groups it opens under an overridden cell (persistent
  ones too) now reach the bake. BNS Trees.esp's 22,827 references were invisible before. The tree-candidate
  lister reads persistent references too (36 -> 79 candidates on a BNS load order).
- .lodo writer: the vertex-cache remap stays a true permutation when a LOD mesh has vertices no triangle uses.
  This fixes a segfault on BNS LOD trees. Vanilla bakes are byte-identical.
- lodgen CLI: `--dim all` bakes rings 4+8+16+32 in the panel's queue order. `--fo4cs-one-root` (opt-in) puts
  arrays, manifests and the .lodb under --native, the panel's layout. Both are off by default, so the output
  is unchanged when they are not given.
- Native emitter: the vertex-AO loop reads LAND records under a lock. The shared ESM reader decompresses in
  place and is not thread-safe. This fixes a whole-map crash in the instances stage ("Qt Concurrent has caught
  an exception thrown from a worker thread"). Output bytes are unchanged against a single-threaded run.

## MISTAKES text
- 2026-09-25 BAKE1: a dry run of 23 chunks passed, then the whole-map bake died after 70 min in the instances
  stage. lodgenParallelFor runs serially below 32 items, so the dry run never used the thread pool, and the
  shared ESM reader's non-thread-safe LAND decompression never ran concurrently. Rule: a dry run must cross
  every parallel threshold the real run crosses (here >= 32 chunks per ring). Gate shared-reader code with
  pool vs --threads 1 bytes.
- 2026-09-25 BAKE1: after the .lodo was written, a 30-minute single-core phase was nearly read as a hang. It
  was the serial proximity join. Name a long phase by sampling the stack (skill ww-stripped-frame-by-strings)
  before calling it stuck, and log the phase's own census line.
- 2026-09-25 BAKE1: a progress entry was stamped from elapsed-time feel (05:58 vs a real 05:53) and corrected.
  Read `date` in the same command that writes the line.

## Skill review
- Loaded: nifskope-ww-worktree-build, nifskope-ww-lodgen, nifskope-ww-render-shot, ww-panel-run-harness,
  ww-lodl-offline-census, mo2-mod-content-census; and nifskope-ww-crash-diagnose for the crashes.
- Written:
  - ww-esm-later-plugin-groups: later-plugin groups under overridden cells.
  - ww-meshopt-unused-vertices: the remap with unused vertices.
  - ww-stripped-frame-by-strings: naming a stripped frame by its function's strings.
  - ww-shared-reader-in-pool: a shared reader inside a pool, the 32-item trap and the byte gate.
  - ww-whole-map-lod-bake: the CLI bake, stage timings, verification, and the FORCE_CARD caveat.
- Wished for: a whole-map bake procedure and a thread-pool shared-state check. Both now exist.
