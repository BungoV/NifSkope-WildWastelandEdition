# TIDY1 -- text for main's ledgers (the overseer splices; this lane writes none of them)

## HANDOFF (top block line)
TIDY1 (branch tidy1-20260927 from ground1-20260927 @ da045481; commits eb5dfa38, 8741678f and the docs/gates
commit after them): empty channels, duplicate array layers and misleading view labels.
- A texture or card array whose emissive is black on every layer now writes no `_g` file, and its `.lodm`
  names no `textures.emissive`.
- Layers with identical texels merge into one.
- The terrain vertex tint, cell height range and ground cover are relabelled in the viewer.

Built (exe sha1 59f08533). NOT BAKED: the NifSkope turn lock was stuck on "anon" the whole session, so the
off/on byte gates wait. Resume: `bash scratchpad/tidy1_20260927/run_gates.sh` once the turn is free (DONE.md §7).

## WW_CHANGES (one entry)
**2026-09-27 -- lane TIDY1: no black emissive sheets, one layer per identical texture, honest labels.**

- **Black emissive sheets.** Vanilla LOD has no glow source: `tools/lod_emission_probe.py` now also reads the
  glow-map flag and the glow slot, and finds 0 of 121 LOD materials and 0 of 3430 LOD shader blocks. So every
  mesh-array `_g` was a black file.
  - A mesh or card array whose emissive is black on every layer now writes no `_g`/`_e`. Its `.lodm` names no
    emissive (absent = emits nothing, as the VT sheets already had it).
  - Card sets are shot from the FULL models, and TreeAspen01-03 emit 0.05. Their 2 card arrays keep the sheet.
  - Saved, measured on the sheets: Boston box 21 files, 50,064,092 B (mesh 6,873,796, cards 43,190,296). The
    installed 09-25 whole-map bake has 90,317,160 B of black `_g` over 4 worldspaces (Commonwealth 55.7 MB).
- **Duplicate layers.** Two sources whose composed sheets are identical texel for texel are one layer. The
  other spellings are aliased, so UV2.y and the `A` lines resolve to the kept layer.
  - Boston: 114 -> 106 layers, 1,769,328 B.
  - Whole map (installed bake): 399 -> 371 layers, 6,815,280 B.
  - Not merged, because they differ: 6 same-texture pairs (5 in the alpha-test byte, Wrhs01 in gloss).
- **Gate-only way back:** `WW_LODGEN_KEEP_BLACK_EMISSIVE=1`, `WW_LODGEN_NO_LAYER_DEDUPE=1`.
- **Viewer labels:**
  - The colour plane is "Terrain vertex tint -- multiplies the ground textures, not the ground colour".
  - Cell range: "per-cell min/max height (culling table, one value per 4096-unit cell)".
  - Ground cover says it is Fallout 76 only and points at mask A.
  - Doc 4.x gains "Aliases, not extra maps" (ao = WW_LODL_AO=1, the v3 water planes repeat the v2 ones).
- **MAPS1 fixes** (copies in the lane scratchpad; the original is untouched):
  - The labeller keeps the audit numbers.
  - It drops the seven duplicate views.
  - It names an S_ sheet from the sidecar's sources, so A512 = "Tree/leaf LOD textures".
- **Harnesses:**
  - `lodgen_texture_arrays.sh` accepts and checks a set that names no emissive.
  - `lodgen_cardlink.py` hashes 4 files for such a set.

## MISTAKES (newest at the top)
- **2026-09-27 TIDY1: sibling objects copied AFTER editing a source hid the edit from make.** The worktree
  build copies `GeneratedFiles/.obj` from the sibling worktree (skill nifskope-ww-worktree-build). I had
  already edited `src/btdterrain.cpp`. The copied `btdterrain.o` was stamped newer than my source, so make
  skipped it and the exe linked without the new labels. The build said RC 0. Counting the UTF-16 label strings
  in the exe caught it; `touch` + rebuild fixed it.
  - Rule: copy the sibling objects BEFORE the first source edit, or `touch` every edited source after the copy.
  - Check: grep the exe for one new string per edited file before calling a build done.
  - Line for skill nifskope-ww-worktree-build, step "copy objects": "touch every source you already edited
    after this copy, or make keeps the sibling's object".
- **2026-09-27 TIDY1 (caught before shipping): "10 duplicates" and "no glow" were both census claims, not
  texel facts.**
  - Merging the 10 same-material pairs by path would have flipped the alpha test on 5 of them. Measured by
    texels, 7 of the 10 pairs plus 1 other pair are identical.
  - Dropping every emissive on "0 glowing LOD materials" would have dropped the TreeAspen card light.
  - Rule: skill ww-merge-by-texels.

## FO4CS reader changes (the reader lands last, by standing order)
1. **`.lodm` kind `array` and `cardArray`: `textures.emissive` may be absent.**
   - Absent = the set emits nothing: bind black / skip the emissive term. Do not refuse.
   - The same set still carries `array.emissiveScale` (all 0 on such a set).
   - No version bump: the emissive was already optional in the format (the VT sheets precedent).
2. **cardCorpusHash (doc 4.13):** a card set whose `.lodm` names no emissive hashes four files (.lodm, colour, normal,
   mask). A set that names one hashes five, as before. The reader's
   recompute must follow the `.lodm`, not assume five.
3. **Fewer layers:** a sidecar/`.lodm` may list fewer layers than there are distinct source keys. Shapes point
   at the kept layer through UV2.y / the `A` line. The reader indexes by layer, never by source key, so nothing
   changes, but any reader that counted sources to size its array must count layers.
4. **Nothing else:**
   - Mesh `_n` B/A and `_gsaos` B stay (BC3 blocks cost the same; readers already ignore B/A of a mesh normal).
   - The `.lodl` AO plane and the sky byte 0x11 stay.
   - `.lodl` ground cover was already absent on FO4.
   - `.lodi` stays v12 (GROUND1's).
