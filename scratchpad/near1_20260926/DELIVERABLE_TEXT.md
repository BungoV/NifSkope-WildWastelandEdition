# NEAR1 deliverable text (lane NEAR1, 2026-09-26, branch near1-20260926)

## HANDOFF text

### NEAR1 -- near library, rung N1 (2026-09-26 03:2x): BUILT AND GATED, NOT MERGED, NOTHING INSTALLED

- New CLI: `lodgen ... --near-library <abs dir>` (src/nearlib.cpp). Bakes the full-detail MODL of every eligible
  STAT/SCOL placement into `<ws>.near.lodo` + `.near.lodi` + refs/shapes/textures sidecars. The far writer is not
  entered.
- **FORMAT VERSIONS MOVED (near files only):**
  - `.lodo` v7 = v6 layout + header flag bit4 `NEAR` (16; set exactly on v7) + material row byte +7 `features`
    (1 parallax, 2 env map, 4 greyscale, 8 vertex colour, 16 model-space normals; bits 5..7 refused).
  - `.lodi` v11 = v10 layout + instance flag bit8 `INITIALLY_DISABLED` (0x100), written only when an instance
    carries it (Sanctuary has none and wrote v9).
  - Far bakes are unchanged, version word included (G4: 130 of 130 files byte-identical to the rung).
  - READER1 still runs on lodo v6 / lodi v10. The FO4CS reader owes v7/v11 before it can read a near library.
- Whole Commonwealth (his MO2 stack):
  - 736,214 REFRs read, 523,755 eligible, 677,390 placements.
  - 38,622 (mesh, material) pairs; 12.16 M triangles; 777,824 clusters.
  - `.lodo` 382 MB, `.lodi` 26.5 MB; 178 s. Output is in `scratchpad/near1_20260926/commonwealth`.
- Clusters are the far code's 16 triangles / 48 vertices, not the brief's ~128. Bigger clusters are a later rung's call.
- Known gaps:
  - The near `.lodi` writes placement identity 0.
  - Texture arrays are a list only (`textures.txt`); nothing is re-encoded.
  - Materials are all legacy (0 PBR) on his stack.
- Gates: `tests/spells/near_library_check.py` (G1-G3, independent ESM + NIF + BGSM reading, `--sabotage` refuters),
  `tests/spells/near_format_selftest.py`, and `lodgen_native_fields.py` j0c. The skill is `nifskope-ww-near-library`.

## WW_CHANGES text

### Near library bake (lane NEAR1, 2026-09-26)
- `lodgen --near-library <dir>` writes a worldspace's full-detail static models into `<ws>.near.lodo` /
  `.near.lodi`, for a future close-range renderer.
- Eligibility:
  - Kept: STAT and SCOL placements, with SCOL parts expanded.
  - Out: destructible, animated, marker and non-static types.
  - Shapes out: alpha-blend, effect, decal and tree-wind shapes.
- Every excluded REFR is counted by its reason.
- BSMeshLODTriShape models draw their full-detail level only.
- Each placement keeps its reference form ID, its initially-disabled flag and its scrappable flag.
- New format versions for these files only: `.lodo` v7 and `.lodi` v11. Far-field bakes are byte-identical to before.
- Whole Commonwealth: 523,755 of 736,214 references, 12.2 M triangles, 382 MB, about 3 minutes.

## MISTAKES text

- 2026-09-26 02:4x NEAR1: started the near bake's model workers without first building the resource stack's
  lazy static index. Four workers built it at once: every model came back "not found", then a segfault
  (rc 139). The far path already calls `lodgenWarmSharedIndices()` before its pool, and I did not look.
  Rule: a new parallel path copies the far path's warm-up, and a first run is read for "all not found".
- 2026-09-26 02:4x NEAR1: passed a relative `--near-library` directory. The exe chdirs to release/, so the
  library landed under release/. Rule: every lodgen output path is absolute (the render-shot rule, again).
- 2026-09-26 03:1x NEAR1: ran a `find /e/Projects -maxdepth 4` to look for a `.lodl`. It timed out, and I stopped it.
  That breaks the search-lean rule (one folder per search). The installed path was already known. Rule: name the
  folder, never the projects root.
