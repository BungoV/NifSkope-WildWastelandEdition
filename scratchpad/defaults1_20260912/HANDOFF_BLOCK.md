- DEFAULTS1 (2026-09-12, exe 21:52:25, 22,280,192 B; bungo's open window needs a
  RESTART) put bungo's four rulings in
  as the SHIPPED defaults -- CLI, struct, statics and the LOD Generation panel
  rows alike: object identity OFF (`--identity` is now the opt-in), terrain
  identity OFF (`--terrain-identity`), LAND1 panel (c)'s land look ON (hex 256,
  warp 341, mip bias -0.22, guide flatwarp 1.0; lattice, octaves, guide scale and
  slope unchanged, base rule still FOOTPRINT), and `--road-ground-paint` 0 so the
  verge beside a kerb keeps the landscape's colour. The old look is one line:
  `--identity --terrain-identity --road-ground-paint 1 --land-hex 0 --land-warp 0
  --land-mip-bias 0 --land-guide off`.
- Riding with them: the `.bto.manifest.txt` sidecar, the texture arrays, the
  impostor cards and the native `.lodo/.lodi/.lodt/.lodl` no longer depend on the
  identity flag. The manifest stays a SIDECAR -- bungo's open call.
- New gate `tests/spells/lodgen_defaults.sh`, five phases, every one against the
  rung `release/NifSkope.before_defaults1.exe`. Seven spell files re-based by
  SPELLING the switch, never by loosening a bar -- including the panel self-test,
  which now ticks the identity box itself instead of borrowing it (121/0 on both
  exes); `lodgen_native_baseline --check`
  against the frozen 2026-09-10 baseline is 25 files, 0 differ.
- Pictures: `scratchpad/defaults1_20260912/images/` (11 files) -- `i_btr_identity.png`
  and `ii_verge_kerb.png` are the headlines, each with its difference number.
- NOTE FOR BUNGO: the one-shot panel migration has already run on his registry,
  so his saved LOD rows now read 256 / 341 / -0.22 / Flat warp / verge 0.
- RED and NOT this lane's: `lodgen_ground_cover` (29/6, its frozen baseline is
  missing) and `lodgen_terrain_vt` V9c seam -- identical numbers on the rung.
  FOR BUNGO: `vt()` now spells the OLD land switches, because the ruled default
  makes the pyramid sheet and the direct bake differ by 4 and 27 bytes of 174,888
  on 2 of 4 chunks (report section 5). Nothing is committed.
