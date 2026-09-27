# Lane TIDY1 -- empty channels, duplicate texture layers, misleading views (2026-09-27)

Worktree E:\Projects\NifskopeWWE-tidy1, branch tidy1-20260927 from ground1-20260927 @ da045481
(night-20260927 + GROUND1's .lodi v12). Off baseline = a bake of ground1-20260927's exe.
The NifSkope turn lock was stuck (held by "anon", process gone) at launch and still at 06:29; not touched.

## 1. Skills loaded
- nifskope-ww-lodgen, search-lean, nifskope-ww-worktree-build (read)

## 2. What was wrong (audit ranks 7, 8, 9)

### Rank 7 -- empty channels: is there any glow to bake?
`tools/lod_emission_probe.py` ran over Fallout4.esm (Commonwealth, DiamondCity, DiamondCityFX). This lane
extended it to also read each material's glow-map flag and glow texture slot.
- LOD models named by a base: 3361, read 3354 (7 missing).
  - Shader blocks: 3430. Own-Emit is set on 100% of them, but 0 have a colour that is not black.
  - Models with an effect shader: 0. Models with a filled glow slot: 0.
- Materials those LOD models name: 121, all read.
  - Emit enabled 0. Glow-map flag 0. Glow texture slot filled 0. Emitting with a colour 0.
- The only emitters in the plugin are the full stadium-light models, and they have no LOD slots.

**Count of LOD materials with a glow source: 0.** Vanilla LOD has no glow, so no glow picture.

**One exception, found by reading the sheets:** card sets are shot from the FULL models. TreeAspen01-03 emit
0.05 (card `.txt`: `emissive 0.05 shapes 1`).
- On the Boston box, 21 of 23 `_g` files are black: all 7 mesh files and 14 of the 16 card files.
- The card 128x512 and 384x1024 files have 6 and 46 lit blocks, at most 8/255.
- The writer's test is "every texel black", so those two keep their sheet.

Decisions, one per channel:

| channel | decision | why | FO4CS reader impact |
|---|---|---|---|
| emissive `_g` sheet on every texture array (mesh and card) | **DROP** when every layer in the set is black | 0 glow sources in LOD, so the file is pure black. Boston box: 21 files, 50,064,092 B of 368,010,044 B in the arrays (14%). Whole map (installed 09-25 bake, 4 worldspaces, read only): 90,317,160 B; Commonwealth 55.7 MB | for such a set the `.lodm` no longer names `textures.emissive`; absent = no emission. The reader must treat a missing emissive as "none", card sets included |
| mesh `_n` blue/alpha (constant 132/0) | KEEP | a BC3 block is 16 bytes whatever the channels hold, so dropping B/A saves 0 bytes. Only BC1/BC5 would save, and that is a different codec for X/Y | none; readers already ignore B/A of a mesh normal |
| mesh `_gsaos` blue (constant 255) | KEEP | same: 0 bytes to save inside BC3 | none |
| `.lodl` ground-cover plane | already ABSENT on FO4 | Fallout4.esm has 0 GCVR records. GCVR is a Fallout 76 terrain concept, so the writer already leaves section bit 1 clear (the installed file has flags 13 = colour + AO + overview). 0 bytes shipped. The real FO4 ground cover is the VT mask sheet's A channel | none; the viewer's label now says so |
| `.lodl` AO plane (coarser copy of mask B) | KEEP | it is the floor under mask B for a bake or a reader with no VT sheets, and `--refresh-ao` rewrites it in place. 2.4 MB whole map. Its absence is already legal (bit 2) | none |
| per-piece sky byte 0x11 (coarser copy of the per-vertex stream) | KEEP | it sits inside the fixed-size instance record. Dropping it saves 0 bytes without a record-layout change, and it is the fallback for a reader without the stream (GROUND1's v12 must not be collided with) | none |

### Rank 8 -- duplicate array layers
The ground1 Boston bake's sidecars list 114 layers, with 13 same-texture pairs:
- 10 are the same material under two spellings, `c:\projects\fallout4\build\pc\data\materials\lod\X.bgsm`
  beside `materials\lod\X.bgsm`:
  - 128x128: BldgBrickLarge, DecoLarge, HitTechStructureLarge, NCALarge
  - 256x256: Billboard02, HitTechExtALarge, HitTechStreaks, HWOverpass, RWRiver, WhiteMarbleBLD
- 3 are keyed once by material and once by diffuse: RockSlab01, RockSlab02, Wrhs01.

**Measured on the shipped sheets** (a hash of each layer's colour + normal + mask slices, all mips,
`gates.py predict`):
- Identical texels: 7 of the 13 (BldgBrickLarge, DecoLarge, NCALarge, Billboard02, HitTechExtALarge,
  RockSlab01, RockSlab02).
- Also identical: ElmTrunks, under two different texture folders.
- So **114 -> 106 layers**, saving 1,769,328 B on Boston.
- The whole map, from the installed bake: 399 -> 371 layers, 6,815,280 B.

The other 6 pairs are NOT duplicates (`pairdiff.py`: colour and normal are identical, every mask block
differs):
- 5 differ in the mask sheet's A byte. That byte is the alpha-test flag of the shapes that used the spelling.
- Wrhs01 differs in gloss (98 vs 189).

Merging those by path would have switched alpha test on or off for their placements, so they stay two layers.

### Rank 9 -- views that mislead
- **37 (P_colour)** is the terrain vertex TINT, which multiplies the ground textures and is near white almost
  everywhere. It is not the ground's colour; that is the VT colour sheet. The old caption said "Ground: colour".
- **46 (S_A512_colour_d)** says "Building LOD textures", but the 512x512 array holds 14 materials, all trees.
  MAPS1's labeller picked the title from the file tag (`C...` = cards, anything else = buildings).
- **Duplicate views:**
  - 22 = 16 byte for byte (`WW_LODL_CHANNEL=ao` IS `WW_LODL_AO=1`, lodinative.cpp ~585).
  - 28 = 14 from another angle.
  - 66 = 65 in another colour code.
  - 68/70/72/73 = 35/40/43/44 (the v3 water bake repeats the v2 planes).
- **36 (P_cellrange)** did not say what it is (the culling table).
- **39 (P_groundcover)** said "NO DATA in this bake". It is absent on every Fallout 4 bake: 0 GCVR.

## 3. What changed
- `src/lodgen.cpp`, `lodgenBuildTextureArrays`:
  - One layer per identical texture. The composed 32-bit layers are hashed, then compared exactly (all four
    sheets and the emissive multiple). The duplicate key is aliased to the kept layer, so UV2.y and the `A`
    line resolve to it. Each merge is logged as `lodgen: arrays: KEY = layer N (REP), identical texels`.
  - An emissive that is black on every layer is not written, and the `.lodm` names no emissive.
  - The card-arrays writer gets the same emissive rule.
  - Report clauses appear only when they fire.
  - Gate-only way back: `WW_LODGEN_NO_LAYER_DEDUPE=1`, `WW_LODGEN_KEEP_BLACK_EMISSIVE=1`.
- `src/nativeemit.cpp`: a card set naming no emissive hashes four files (.lodm, colour, normal, mask) in
  cardCorpusHash, instead of being refused.
- `src/btdterrain.cpp` (viewer, labels and notes only; the water and identity views are untouched):
  - The tint label and note.
  - The cell-range label and note.
  - The ground-cover label, plus a note pointing at mask A when the file has 0 GCVR.
- `src/io/lodmfile.h`, `docs/LODGEN_IMPOSTOR_SPEC.md`, `docs/LODGEN_NATIVE_LODO_LODI.md`:
  - The emissive is optional on array/cardArray.
  - The measured numbers.
  - 4.13 item 4: four files when there is no emissive.
  - "Aliases, not extra maps".
  - No version bump: the emissive was already optional in `.lodm`, and `.lodi` stays v12.
- `tools/lod_emission_probe.py` reads the glow-map flag and the glow texture slot.
- Harnesses:
  - `tests/spells/lodgen_texture_arrays.sh` checks that a dark set names no emissive and has no `_g`/`_e`
    file. Its Python blocks compile.
  - `tests/spells/lodgen_cardlink.py` hashes the emissive only when it is named.
- MAPS1 fixes (copies in `scratchpad/tidy1_20260927/maps1_fix/`; the overseer's originals are untouched):
  - `label_full.py` has the new captions for 36/37/39. It titles an S_ sheet from the sidecar's sources,
    drops the 7 duplicates, and KEEPS the audit's numbers.
  - `render_all.sh` has no L06 and no W3 height/waterheight/watertype/cellflags.
- New skill `.claude/skills/ww-merge-by-texels/SKILL.md`.

## 4. Gates
Measured now (offline, no NifSkope run):
- Glow: **0 LOD materials with a glow source** (0/121 materials, 0/3430 shader blocks). No glow picture.
- Base snapshot `base_ground1_on.json` (ground1 `bakes/on`, made by ground1 release 04:50):
  - 153 files, 114 layers.
  - All 647 `A` lines resolve.
  - 23 emissive files: 21 black, 2 lit.
- The gate fails when it should: `compare base base on` gives 2 FAIL (black files not gone, emissive count),
  and `compare base base off` passes 153/153.
- Predicted from texels: 114 -> 106 layers (8 merged). Emissive saved: 50,064,092 B on Boston (mesh
  6,873,796 B, cards 43,190,296 B).
- Whole map (installed bake, read only): 90,317,160 B of emissive and 6,815,280 B of duplicate layers.
- Build: RC 0.
  - `release/NifSkope.exe` sha1 59f08533d00866724fe41c4126ff8ec3bd98e9f2, 25,923,584 B.
  - The run copy is in `scratchpad/tidy1_20260927/run/`.
  - Strings checked in the exe: both env switches, "identical texels", "vertex tint", "culling table".
  - The commit after the build changes comments and docs only (`git diff` has no code lines).

**NOT RUN (turn lock stuck on "anon" all session):**
- off bake == base, byte for byte.
- on bake: black `_g` gone, lit kept, identical texels for every source, A lines resolve.

`run_gates.sh` runs both and prints PASS/FAIL lines to `gates.log`.

## 5. Commits
- eb5dfa38: the probe reads the glow flag and slot; rank 7 decisions.
- 8741678f: no black emissive; one layer per identical texture; viewer labels; docs; harnesses.
- The next commit: measured corrections (lit card sets, 114 -> 106), gates/bake/resume scripts, MAPS1 fixed
  copies, the skill, DELIVERABLE_TEXT, this file.

## 6. Pictures
Made offline with the fixed labeller from MAPS1's existing Boston renders (view 8 ortho, maps1 camera). The
pixels are MAPS1's; only the title changed, and the viewer's labels are not drawn in a clean render. Full
size, one file each, 60 px bar:
- `E:\Projects\NifskopeWWE-tidy1\scratchpad\tidy1_20260927\pics\37_P_colour.png`: "Ground: vertex tint
  (multiplies the ground textures; not the ground colour)"
- `E:\Projects\NifskopeWWE-tidy1\scratchpad\tidy1_20260927\pics\46_S_A512_colour_d.png`: "Tree/leaf LOD
  textures (14 materials): colour"
- `E:\Projects\NifskopeWWE-tidy1\scratchpad\tidy1_20260927\pics\36_P_cellrange.png`: the culling-table caption

## 7. Still not right / resume steps
Blocked only on bakes. The turn lock `E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\.ns_turn\who` =
`anon` (still held at 06:29; the process is gone). Not touched. To resume, after bungo frees the lock:
1. Check that the game is down and no build is running.
2. Run `bash /e/Projects/NifskopeWWE-tidy1/scratchpad/tidy1_20260927/run_gates.sh`. It takes the turn as
   `tidy1` for each of the 2 Boston bakes (about 16 min each, as GROUND1's did). It writes
   `scratchpad/tidy1_20260927/gates.log`.
3. Expect:
   - OFF: `PASS off is byte-identical ... 153 of 153`.
   - ON: PASS on every line, with 21 black files gone (50,064,092 B), 2 lit kept byte-identical, 114 of 114
     sources with identical texels, 114 -> 106 with 8 merged spellings, and every A line resolving.
   - A `.lodo`/manifest byte change on ON is expected (cardCorpusHash, merged shapes). The gate checks what
     matters, not those bytes.
   - If ON merges fewer than 8, the writer's pre-compression compare found a difference the BC sheets hide.
     Record the number; the gate accepts any count that equals the logged aliases.
4. Fill section 4 with the numbers.
5. Delete `scratchpad/tidy1_20260927/bakes/` and the `run/` exe copy. Also delete
   `scratchpad/tidy1_20260927/glow/plugin.pkl`, a cache.
6. Optional: run `bash tests/spells/lodgen_texture_arrays.sh` and the cardlink spell on the new exe.

Not done by design:
- The mesh `_n` B/A and `_gsaos` B stay: BC3 costs the same, so there are 0 bytes to save.
- The 6 non-identical pairs stay two layers.
- O_sky_stream and T_msn_worldXYZ come from MAPS1's offline scripts. Only the labeller skips them.

## 8. Skills review
- Used: nifskope-ww-lodgen (its rule to compile the harness blocks first: blocks clean), search-lean,
  nifskope-ww-worktree-build.
- Missing, now written: `ww-merge-by-texels`. It covers dedupe and drop by composed texels, gating by a
  source -> layer hash, and checking card sheets before calling a channel empty.
- Gap in nifskope-ww-worktree-build: touch edited sources after copying the sibling objects (MISTAKES entry
  in DELIVERABLE_TEXT). That skill is outside the worktree, so the line is in DELIVERABLE_TEXT for the
  overseer to splice.
