# LODGEN — NifSkope generates Fallout 4's world LOD

Campaign plan, bungo-scoped 2026-08-31. The charter with the full design
contract lives at the top of [TO_BE_IMPLEMENTED.md](TO_BE_IMPLEMENTED.md);
the measured format groundwork is
`E:\Projects\Fo4CommunityShaders\Codex\lod-fo4-vs-fo76-comparison.md`.
This document is the execution plan: rungs, deliverables, verification,
risks. Each rung ships independently.

## What is being built

NifSkope-WW becomes an FO4 world-LOD generator: base-game-parity terrain
(.btr) and object (.bto) chunks from an ESM + the game's own per-object
`_LOD.nif` library, plus additions vanilla never had — the headline being
**per-placement identity in vertex colours**, which turns merged chunks
from anonymous soup into addressable objects for FO4CS (screen-size fade,
pivot-correct wind, repetition breaking, per-object dither).

The data contract (settled, in the charter): R+G = 16-bit per-chunk object
index · B = baked AO · A = class-switched parameter (tree sway / ground
blend / wetness) · per-chunk manifest for object constants · optional
extended profile on UV2 (measured virgin: 0 of 119,701 vanilla shapes) and
EyeData. Tiering rule: texture = surface-shared, vertex =
placement-individual, manifest = object-constant.

## Where it lives

- Code: `src/lodgen.{h,cpp}` (generator) + `src/esmdata.{h,cpp}` (record
  layer over the vendored esmfile), flat-src convention like btdterrain/
  nifmerge. CLI: `nifskope-cli lodgen <info|terrain|objects>` — the
  primary, harness-driven interface.
- **The generator core emits per-chunk callbacks** (chunk done → NIF
  bytes): the CLI writes files and progress lines; the GUI additionally
  splices each finished chunk into a LIVE PREVIEW DOCUMENT — each chunk
  under a wrapper node carrying its world translation + dim scale (the
  FO76 water-node trick) — so generation is watchable tile by tile in the
  viewport, with a progress bar (chunks done/total, rung, worldspace) and
  cancel honoured between chunks (bungo-requested 2026-08-31). The
  whole-map .btd build already proved the viewport at this scale.
- GUI face (a later round): a compact **World LOD manager** in the left
  column, Collision-Manager-style — worldspace pick, region, rung
  toggles, the progress bar. Generated chunks are ordinary NIFs; the
  normal viewer is the inspector.

## Rung 0 — foundations

*The enabling work; no user-visible output.*

- **Compile the vendored `esmfile.cpp`** (sits uncompiled in
  lib/libfo76utils) and build the record layer over it: WRLD → CELL grid →
  REFR (position/rotation/scale, base link, XSCL, initially-disabled/
  deleted flags) → STAT (MNAM LOD model slots, DNAM leaf params) → TREE
  (CNAM flexibility/amplitude) → LAND (heightfield, LTEX layers, VTXT
  opacities). Layout authority: wbDefinitionsFO4.pas (re-fetch:
  `https://raw.githubusercontent.com/TES5Edit/TES5Edit/dev/wbDefinitionsFO4.pas`).
- **CLI-first**: `nifskope-cli lodgen` drives everything headless; GUI
  later if ever. Same pattern as `btd`.
- **Harness skeleton**: comparators that hold generated chunks against
  vanilla ones (the collision campaign's method — external authority is
  the shipped game).

Exit: the CLI can print a chunk's REFR list with resolved LOD model paths
and a cell's heightfield, verified against xEdit for one known cell.

## Rung 1 — terrain (.btr)

*Mostly existing machinery: the .btd builder already does
grid → seam-closed BSTriShape.*

- LAND records → per-chunk heightfield → Land shape. v1 may ship a regular
  grid (denser than vanilla's decimated ~4.2k tris/dim4); v1.5 decimates
  via the **vendored meshoptimizer simplifier** to vanilla-scale counts
  with a screen-error target.
- Water shape from cell water heights: 16 segments (per-cell hiding),
  effect shader, vanilla layout — all format details measured and in the
  comparison doc.
- **Terrain vertex bakes**: dominant LTEX material class + flow-
  accumulation wetness (+ heightmap AO) in the colour channels — the
  Physical Weathers tie-in.
- Textures: v1 points at vanilla's existing per-chunk bakes
  (`textures/terrain/<ws>/<ws>.<dim>.<x>.<y>.DDS` + `_msn`). Regenerating
  them (splat evaluation + DDS/BC1 writer) is rung 3.
- Multibound/miniature bookkeeping identical to vanilla (measured).

Verification: `lodgen_terrain.sh` — heights against LAND (external
authority, python-decoded), seams closed, water segments cover wet cells
exactly, byte-level structure parity spots vs vanilla .btr; then the
in-game gate: one region's vanilla .btr replaced by ours, bungo flies it.

## Rung 2 — objects (.bto) — the big one

- **REFR walk per chunk footprint**: filter to bases with LOD models
  (STAT MNAM / TREE), respect initially-disabled/deleted, apply XSCL,
  select the `_LOD_n` variant by chunk dim (the per-object distance
  ladder), transform into miniature chunk space.
- **Stitch**: weld into per-material buckets (NEVER across source
  objects), per-cell segments at dim4, BSMultiBound/AABB per shape,
  alpha-tested split (`-at` convention). **No atlas in v1** — reference
  the source LOD textures directly (xLODGen-legal).
- **Identity + manifest**: 16-bit index per placement; manifest sidecar
  (binary + JSON debug dump) with form ID, pivot, bound radius, class,
  tree wind knobs (copied from TREE/STAT records), emissive toggle.
- **Bakes** (each a toggle): AO ray-cast against the assembled chunk
  (CPU BVH; correctness first, speed later), tree sway alpha COPIED from
  the source mesh's authored vertex alpha (vanilla convention measured:
  255=rigid), building ground-blend from terrain contact.
- **Repetition breaking at stitch time**: mirror half the cards by hash,
  rotate card sets per tree.
- **EARLY in this rung, the two owed tolerance checks**: stock engine
  (no CS) loading chunks with the fatter vertex desc, and with vertex
  alpha present. These gate the whole additions layer and cost one
  in-game session.

Verification: `lodgen_objects.sh` — REFR coverage vs xEdit for sample
chunks, identity constant-per-triangle invariant, segment ranges exact,
manifest↔mesh index agreement, a damage control proving the checks can
fail; in-game A/B against vanilla chunks.

## Rung 3 — bake passes (the FO4CS asset layer)

Separable rounds, order negotiable:

1. **Terrain texture baking** — evaluate the LAND splat into per-chunk
   diffuse+MSN (needs a BC encoder; BC1 is simple, or ship uncompressed
   first). Frees terrain from vanilla's bakes; enables modded-worldspace
   support.
2. **Atlas baker** (classic) or **texture arrays** (FO76-style: layers +
   per-triangle index, with the CS renderer counterpart). Arrays are the
   better target since CS exists; the atlas path is for vanilla-only
   users.
3. **Impostor card baker** — render source models to card sets (d/n at
   minimum) for the long tail of objects with no authored `_LOD.nif`,
   FO76-style but ours.
4. **Instancing manifests** — FO76-style repeated-object lists (mesh ref
   + transforms) consumed by a CS instanced draw path; stitched fallback
   retained for vanilla.
5. **Geomorph weights** in EyeData (continuous LOD transitions) — the
   thing neither Bethesda game did; needs the CS shader half.

## The CS counterpart (separate repo, coupled schedule)

FO4CS work: LOD shader reading identity/AO/class channels, manifest
loader, wind function with per-tree phase + pivot bend, hash-driven
hue/value jitter, screen-size fade (hybrid-LOD rung 0) applied per
object inside chunks. **One-track rule applies on the CS side** — that
half schedules with the hybrid-LOD campaign (~late Sep 2026); the
NifSkope generator side is independent and can proceed any time.

## Risks and open questions

- **Stock-engine tolerance** (desc width, vertex alpha) — checked early
  rung 2; if vanilla chokes, the additions become a CS-only output mode
  (generator flag), parity output stays vanilla-desc.
- **Decimation quality** vs vanilla's authored-looking terrain meshes —
  meshoptimizer with conservative error targets; measured against
  vanilla tri counts.
- **AO bake time** on big worldspaces — correctness first; budget/
  parallelism later (the rebuild.sh worker pattern exists).
- **ESM completeness traps**: persistent-cell REFRs, XESP enable
  parents, leveled/base swaps — enumerate against xLODGen behaviour
  when hit, not speculatively.
- FO76 per-triangle array-index decode stays optional (only needed for
  the Appalachia-port texturing route or byte-faithful array cloning).

## Sequencing

Rung 0 + 1 together are one focused stretch (the terrain builder exists;
ESM reading is the new work). Rung 2 is the campaign's centre of mass.
Rung 3 is several independent rounds, each unblocking a CS feature. First
session: compile esmfile, read one cell's LAND, feed the terrain builder,
diff against vanilla.
