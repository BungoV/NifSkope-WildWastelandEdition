# NEAR2 deliverable text (lane NEAR2, 2026-09-26, branch near2-20260926)

## HANDOFF text

### NEAR2 -- near library textures, placement identity, texture arrays (2026-09-26 05:43): BUILT AND GATED, NOT MERGED, NOTHING INSTALLED

- Commits (branch near2-20260926, from main 335dfa50): c6b45033 cube flag, c556e8d2 identity + canonical paths +
  Texture2DArray pools, a57fd044 gates T / A / GI, e51db7bc near archive order, f82900f4 contract doc + identity
  refusals, plus this text.
- bungo's purple trees: NOT missing files. The NEAR1 shots ran the viewer with no MO2 stack (magenta BNS trees)
  and under his persisted Tint/Greyscale setting (lilac brick = the raw greyscale texture). The bake now writes
  `<ws>.near.resources.txt`; shots use it as `WW_LODGEN_RESOURCES` in a fresh settings scope. Magenta pixels,
  NEAR1 near shots 645 / 852 -> NEAR2 0 / 0.
- The bake stored the NIF/BGSM spelling of texture paths (2,746 of 2,989 whole-map diffuses did not open as
  written). Now canonical (`textures\...`, folded).
- Near archive order now follows plugin load order, later wins (`lodgenSetPluginOrder`, near branch only). The far
  bake still uses the name heuristic: DLCNukaWorld 256 px beats UHD 1024 px there. Candidate lane, needs a G4
  re-baseline.
- `.lodi` cold `identity` on a near pair = the plugin's load-order-resolved identity (full index, or
  0x8000 | light slot). Game form id = `nearGameFormId`. Sidecar `<ws>.near.plugins.txt`.
- New `<ws>.near.texarrays.bin` (NTXA v1): pools by (2d|cube, dxgi, w, h, mips), layer per material slot, no
  re-encode. Whole map: 80 pools, 4,868 layers, 421,963 B.
- Gates, whole Commonwealth:
  - G1-G3 + GI PASS (736,214 / 523,755 / 677,390).
  - T PASS: bake-path 0, renderer-stack 0, missing 8 (the same 8 as before).
  - A PASS.
  - Format self-test 22/22.
  - G4 130/130 against a rung exe built from 335dfa50.
- Owed:
  - The FO4CS reader for texarrays.bin and the identity.
  - The far .lodi has the same light-plugin offset (untouched).
  - The viewer has no plugin order.
  - A tint-off discriminator render was not run.
- Report: `scratchpad/near2_20260926/DONE.md`. Skill: `nifskope-ww-near-library` section 6.

## WW_CHANGES text

### Near library: textures, identity, texture arrays (lane NEAR2, 2026-09-26)
- Every texture path in the near library is stored in one canonical form. One image is one key: Boston went from
  1,742 to 1,696 distinct textures.
- The near bake reads archives in the order the game loads them: a plugin's archives at its load-order position,
  later wins. The UHD texture pack now beats DLC low-resolution copies, as it does in game. Far bakes are
  unchanged.
- The bake writes the resource list it used (`<ws>.near.resources.txt`). A viewer given that list sees the
  textures the bake saw.
- Each near placement records its plugin's load-order identity (light plugins included), so a reader can rebuild
  the in-game form ID. List: `<ws>.near.plugins.txt`.
- New `<ws>.near.texarrays.bin`: every texture a material uses, packed into Texture2DArray pools by format and
  size, with each material slot's pool and layer. Nothing is re-encoded. Two bakes are byte-identical.
- New gates:
  - `near_texture_check.py`: every diffuse resolves, sorted by cause.
  - `near_texarrays_check.py`: the arrays against each texture's own header.
  - Plugin identity inside `near_library_check.py`.

## MISTAKES text

### NEAR2 (2026-09-26): mistakes recognised in lanes NEAR1 and NEAR2
- **NEAR1 explained the lilac brick without measuring it.** It wrote "greyscale-to-palette drawn raw (the viewer
  has no palette)". The viewer has a palette (Scene::DoMaterialTint); the shot inherited bungo's persisted option.
  Rule: a colour explanation gets a render that flips it, or it is labelled a guess.
- **NEAR1 shot pictures under bungo's settings and with no resource stack.** Missing textures in the viewer were
  shown to bungo as the bake's output. Rule: every picture runs in a fresh settings scope, with the stack the
  artefact was baked from.
- **The first drop-entry refuter could not fire.** It dropped the last entry with a textures folder, which serves
  no Boston material, and stayed green. Rule: a refuter picks its victim by the property under test (an entry
  that ALONE serves a diffuse), never by position.
- **G4 was first compared against the wrong rung.** NEAR1's old far bakes predate FIX1's fullTriangles change on
  main, so the fx fixture differed for reasons outside this lane. Rule: G4's "old" is a rung exe built from this
  lane's base commit.
- **A Bash heredoc turned `\\` into `\` inside Python source.** It broke string literals in
  near_library_check.py. Rule: code with backslashes goes through the Write tool or a quoted heredoc
  (`<<'EOF'`), never an unquoted one.
