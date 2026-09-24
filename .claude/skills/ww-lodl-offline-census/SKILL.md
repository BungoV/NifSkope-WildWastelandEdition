---
name: ww-lodl-offline-census
description: Measure a whole worldspace OFFLINE in Python — every level-0 height sample of a .lodl, its per-cell table, the matching CELL/WRLD/WATR records out of Fallout4.esm, and connected-component labels over the result — without building NifSkope, without running the exe, and without writing a second format reader. Use for any per-texel or per-cell census of terrain, water, AO or blend over a worldspace (bodies of water, flooded cells, shore length, coverage), and before writing any new .lodl analysis script in the NifSkope Wild Wasteland tree.
---

# Measuring a `.lodl` worldspace offline

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane WATER1
(2026-09-09), which measured 37.7 M height samples and 36,864 cells of the
Commonwealth in about four seconds a run and never touched `src/` or the exe.

**The whole point: a census does not need a build.** The `.lodl` and
`Fallout4.esm` are both readable from Python, both already have a validated
reader in this tree, and a lane that builds to count something has spent four
minutes and a lock on bungo's window for nothing.

## Do not write a second reader — there are already two

| what | where | note |
|---|---|---|
| `.lodl` decoder, INDEPENDENT of `src/lodtfile.cpp` | `tests/spells/lodl_open_authority.py` | class `Lodt`: header, per-cell table, AO, overview, and the progressive pyramid. One sample per call, by design |
| `.esm`/`.esp` structural reader | `scratchpad/mountains_20260907/fo4esm.py` | lane ESPWRITE's; asserts every GRUP's children consume exactly its size, handles the `XXXX` oversize escape and compressed CELLs |
| the format contract | `docs/LODGEN_BTD_FORMAT.md` | read it before believing any offset |

Import them. `sys.path.insert` to `tests/spells` and to
`scratchpad/mountains_20260907`. A third decoder is a third thing to be wrong.

## The bulk path the authority decoder does not have

`Lodt.plane_word` answers ONE sample; 37.7 M calls is not a census. Walk the
blocks once instead and scatter:

* levels run **coarsest first**; the directory index base for level `j` is the
  running sum of `blocks_x(k) * blocks_y(k)` for `k` from `levels-1` down to
  `j+1` — the same `first` the authority decoder computes;
* at the coarsest level a block holds a full `blockEdge²` grid, `k = wy*be+wx`;
* at every finer level it holds `(be/2)² * 3` entries and
  `k = (py*half+px)*3 + sub`, `sub` = 0 right, 1 below, 2 below-right, so the
  inverse is `wx = 2*px + (sub != 1)`, `wy = 2*py + (sub != 0)`;
* a level-`L` sample at `(lx, ly)` is the global sample `(lx << L, ly << L)`;
* plane 0 is height; the payload carries `2 + colour + groundcover` planes,
  from the section-flag word, so plane 0 is always at byte 0.

**The gate on the scatter, and it is not optional:** keep a `seen` array and
assert every level-0 sample was written **exactly once** (the pyramid's own
invariant), then compare a few hundred random samples against
`Lodt.plane_word`. Lane WATER1: 0 mismatches on 400 samples, 1.7 s for the
Commonwealth. A scatter that is wrong is plausible everywhere and correct
nowhere — the same failure mode the format document warns about for the Y
mirror.

`scratchpad/water_20260909/lodl_bulk.py` is that module: `bulk_height_words`,
`heights`, `cell_table` (a numpy structured view over the flat 16-byte records),
`watr_table`, `default_water`. Copy it; do not retype it.

## There is no scipy on this machine

Checked 2026-09-09: `import scipy` fails. Anything needing connected components,
a distance transform or a label pass is written out:

* **connected components**: run-length per row (vectorised with
  `flatnonzero` + a break mask), union-find over RUNS not texels, vertical
  union by walking two rows' run lists in step. 6144² in ~1 s.
  `scratchpad/water_20260909/ccl.py`, with an optional per-texel KEY that two
  texels must share to join — which is how "same height" or "same type"
  segmentation is expressed without a second pass.
* **distance transform**: two-pass 3-4 chamfer, rows vectorised, the horizontal
  sweep a plain loop. `water_model.chamfer_distance`.
* every such routine ships a `selftest()` of hand-computed answers (a U shape is
  one component; two diagonally-touching blobs are two; a keyed bar splits) and
  it is RUN before the real data, per `ww-control-calibration` step 1.

## Reading the ESM beside the file

The `.lodl` carries RESOLVED values (a cell's water height is already the
`XCLW`-or-`DNAM` answer). Go to the master for what the file deliberately does
not intern:

* `WRLD` `DNAM` = default land height + default water height, `NAM3` = LOD water
  WATR form, `NAM4` = LOD water height, `MNAM` = the **usable cell extent**
  (NW then SE, as `s16 x2` pairs) — the Commonwealth is 62×62 usable cells
  inside a 192×192 rectangle, and a census that forgets this counts the filler
  ring;
* `CELL` `DATA` is **u16** (bit1 Has Water, bit3 No LOD Water), `XCLW` is a raw
  float whose no-water sentinels are `0xFF7FFFFF`, `0x7F7FFFFF`, `0x4F7FFFC9`;
* `WATR` `DNAM` is 201 bytes and its field order is in xEdit's
  `wbDefinitionsFO4.pas` (`wbRecord(WATR, 'Water'`): fog block first (depth,
  shallow/deep colour as byte RGBA at +4 and +8), physical at +52, specular at
  +100, noise at +128, silt at +188, SSR byte at +200. Two of Fallout4.esm's 42
  records stop at 188 (`SetOptionalFrom(4)`);
* `WATR` `NAM0`/`NAM1` are Linear and Angular Velocity, `vec3` each — vanilla's
  only flow, and it is per FORM, not per body.

Fetch the definitions with
`curl -sL https://raw.githubusercontent.com/TES5Edit/TES5Edit/HEAD/Core/wbDefinitionsFO4.pas`
when a layout is needed; it is 465 KB and grep answers in one call.

**The corpus path is `X:\Programs\Steam\steamapps\common\Fallout 4\Data\`.**
`E:\Tools\Fallout 4\DataUnpacked\Data\` holds no plugin (it is Materials,
Meshes, Textures, LODSettings). `ls` every input path in the first call.

## Cache the walk, not the numbers

The ESM walk is ~20 s and the bulk decode ~3 s; both go to a pickle / `.npy`
beside the script so the tenth analysis pass costs nothing. Everything derived
(labels, masks) is recomputed, because a cached DERIVED array outlives the rule
that produced it and that is how a stale number gets into a report.

Deliverables live under `scratchpad/<topic>_<date>/` in the repo, never only in
`%TEMP%` (CONSTITUTION rule 8).

## What a census report owes

Per CONSTITUTION rule 4: the controls printed above the real numbers, the
population beside every count (a count that cannot be checked against its own
total is not reportable — lane WATER1 printed 73,728 land cells in a 36,864-cell
worldspace by summing a bit VALUE), and the discriminator measured before
anything is segmented on it.
