# TERR1 deliverable text (for the overseer to splice)

Branch terr1-20260927, commit f78c574c (code) + the skill commit after it. Not merged, not installed.
STATUS: code built (exe runs/sky2, sha1 0c9908c5); NO bake has run with it. The turn lock was stuck ("anon")
all night. Every bake gate and every picture is still owed: `bash scratchpad/terr1_20260927/resume.sh`.

## For the FO4CS terrain reader

- **No format change.** `.lodt` VT sheets keep their version, roles, formats and channel order. The reader needs
  no code change. The `.lodl` is unchanged.
- **`_msn` (role 2) now carries object normals.** Where a flat object (road, rail, pavement slab, kerb, decal) is
  painted into the colour sheet, its own normal map is rotated into world space and blended over the
  heightmap normal with the same mask and alpha the colour uses. Channel order unchanged: R east, G up,
  B north, BC1. Off the colour's mask the sheet is meant to be byte-identical to before (gate G2, not yet run).
- **Mask B (role 5, blue) now sees the placed objects.** The sky march takes, per direction, the higher of the
  terrain slope and the object walls (the level-0 distant-LOD meshes on a 128-unit lattice, read every 64 units
  out to 1,458), plus overhead cover (bridges, decks). Street canyons and ground under bridges go darker.
  Where no object is in reach the byte is exactly the old terrain-only byte.
- **The `.lodl` AO plane stays the terrain-only term.** That is the intermediate FO4CS can use if it wants
  terrain AO without objects; the VT mask B is the combined one.
- The stock `.btr` `_msn` (chunk path) is NOT stamped and its AO does not change.
- Ways back (command line only): `--no-stamp-normals`, `--no-sky-objects`. Both together must give
  night-20260927's sheets byte for byte (gate not yet run).

## Measured so far (offline, no bake)

- Sky law chosen against a ray cast through the LOD triangles (stand-in lattice, 2,208 samples near objects):
  error 22.3 levels, correlation 0.905, against terrain-only 87.4 levels and 0.07.
- Predicted on the Boston sheets: street canyons 222 -> 71 on average (137,055 texels); open ground unchanged
  within 1 level on 95.6% of texels (correlation 0.995). Named streets: Theater District 224 -> 16,
  Vault-Tec office 223 -> 15, Diamond City outside 239 -> 42, Fens bank 231 -> 36.
- These are predictions from Python, not bake output.
