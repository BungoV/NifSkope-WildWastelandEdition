## 2026-09-12 — the far-terrain sheet format, a cleaned normal cache, and what the blue-purple chunk actually is

Lane TERRAINFMT1, main tree. The brief was to measure vanilla's far-terrain
sheet format, write sheets that match it, and remove a blue-purple cast from our
chunks. Two of those landed as switches. The third turned out not to be a sheet
problem at all, and the numbers that say so are the most useful thing in this
entry.

### What vanilla actually ships (measured over the whole corpus, before any code)

All 6,120 Commonwealth far-terrain sheets were read, not sampled.

* The colour sheet is **DXT5 with 10 mips**, down to 1x1. Ours was DXT1 stopping
  short of the bottom.
* The colour ALPHA is **255 everywhere** on every one of the 50+ sheets
  histogrammed. Vanilla carries nothing in it. That matters because our `_data`
  sheet's cover stamp (`WWCV` in `dwReserved1`) had been argued about as though
  it collided with a vanilla meaning; there is no vanilla meaning to collide
  with.
* The `_msn` is **BC1**, and its normals are not unit length after a block
  decode -- an artefact of the compressor, not a convention.

### `--sheet-format vanilla` (default `legacy` = the previous bake's bytes)

Writes the colour sheet and the `_msn` as DXT5 with the full 10-mip chain,
alpha 255 throughout, matching what was measured above.

**And it changes the rendered picture by nothing.** On a pinned camera over
chunk -20,24, mean RGB distance to vanilla's own render: our bake with the old
sheets **131.95**, our bake with `--sheet-format vanilla` **131.95** --
**0 pixels differ** between those two renders. The switch is honest about the
container; it was never going to move a colour, and the brief's premise that it
would is refuted rather than confirmed.

### What the blue-purple actually is: our own vertex colours

`WW_RENDER_FLAT=1` draws vertex colours alone -- no texture, no lighting. On the
same chunk:

| mesh | flat render, mean R/G/B |
|---|---|
| vanilla's `.BTR` | 237.3 / 237.3 / 237.3 |
| ours | **50.9 / 49.3 / 202.1** |
| ours with `--no-terrain-identity` | 237.2 / 237.2 / 237.2 |

`src/lodgen.cpp:952` writes the terrain identity channels into the vertex colour
slot: **R = land material class id, G = wetness, B = ambient occlusion,
A = shore proximity**. A consumer that multiplies albedo by vertex colour gets a
blue-tinted world, because B (occlusion) is near 1 while R and G are small ids
and wetness. Mean distance to vanilla's render: **137.77 before, 0.48 after
`--no-terrain-identity`** (median 0.00).

Three instruments prove the sheets really were being sampled, so "the sheets do
not matter" is a measurement and not a missing file: a mix arm (vanilla colour
sheet + our `_msn`) sits 3.34 levels off pure vanilla over 168,211 pixels; a
synthetic flat-red colour sheet renders (59.8, 0.2, 0.2) and a flat grey one
(31.0, 28.7, 131.1); removing the `_data` sheet changes nothing.

**No default was changed and no look decision was made.** `--no-terrain-identity`
already existed and still defaults to off. What this entry adds is that
`docs/LODGEN_PARITY.md` says *"Default output carries none of it"* about those
channels and the CLI's default is `true`, so the document and the code disagree.
Which way to settle it is bungo's call.

### `--msn-cache DIR` (default off)

Ingests bungo's cleaned 2K `_msn` PNGs (2,304 files, 2048x2048) and writes the
`_msn` **uncompressed B8G8R8A8 behind a DX10 header, dxgi 87, full mip chain**.
The directory is opened read-only and never written to.

Uncompressed rather than re-encoded, and that is measured, not assumed: pushing
the cleaned sheet back through the tree's own BC1 endpoint rule brings the 4x4
block grid back. The cleaned sheets keep **92.9 %** of vanilla's fine energy at
the 512 band while carrying 2.0x a bicubic upscale's energy at the 2K band, so
the extra detail is real and lives below vanilla's own resolution.

Size, for the decision that is bungo's and not this lane's: per sheet the
uncompressed 2K set is 21x vanilla's BC1 `_msn`; at 1K mip 0 it is 6x; BC7 at 2K
would be 6x and at 1K 1.5x. Dropping mip 0 to 1K costs a factor of four in size
and half the linear resolution, and still leaves a 2x linear gain over vanilla.
**Load cost was not measured** -- nothing was installed and nothing was timed in
the game.

### The `_msn` detail term: REFUSED, with the numbers

The brief asked for the land textures' own `_n` maps to be blended into the
height normal so our `_msn` carries vanilla's texel-scale roughness. It was
built as a candidate, measured on both tiles, and **refused**. No code shipped;
there is no `--msn-detail` flag.

| | vanilla | ours | candidate s=1 | candidate at fitted s | phase-twin floor |
|---|---|---|---|---|---|
| roughness (mean 1-texel step) | 19.825 / 20.302 | 2.221 / 2.326 | 7.085 / 7.149 | -- | -- |
| structure vs vanilla, mean abs r | -- | **0.0116 / 0.0144** | 0.0010 / 0.0013 | -- | 0.0011 / 0.0010 |
| tiling line at k=48 | 0.988 / 1.027 | 0.991 / 0.986 | 6.089 / 5.810 | 10.094 / 9.627 | -- |

The candidate reaches 35 % of vanilla's roughness at strength 1 and needs
strength ~5.2 to reach it, at which point the 341.333-unit tiling repeat rings
at **ten times** vanilla's level. Worse, its detail has no relationship to
vanilla's: mean absolute correlation 0.0010, against its own phase-randomised
twin floor of 0.0011 -- indistinguishable from structureless noise. Our existing
coarse bake, with none of this, correlates ten times better. A term that adds
the right amount of the wrong thing is not the detail vanilla has.

### Gates

One build. `release/NifSkope.exe` 2026-09-12 12:58:48, 22,007,808 B, sha1
`ba7585cba389c8b38f0e07c6c11063e0cec1124c`. `make -n` emits zero compile lines
afterwards; all six objects that include the changed header are newer than it.

Both switches at off against the rung exe: the **whole** output tree,
**10 files, 0 differ** -- including the `.lodb` ledger whose switches hash would
move if a default had.

Nine harnesses at GROUND1's baselines: `lodgen_roads` 11/0, `lodgen_native`
18/0, `lodgen_native_baseline` 25 files / 25 baked / 0 differ,
`lodgen_terrain_vt` 41/1 (same named check), `lodgen_ground_cover` 29/5,
`lod_generation` 116/0, `lodl_open` 23/0, `lodgen_terrain_pbrm` 14/0, `animws`
236/0 with 1 skip. The ground-cover failures were compared BY NAME rather than
by count: the same harness was re-run against the rung exe and the two name
lists are identical, the rung's one extra failure being `C0 the exe is newer
than every source`, which a frozen exe cannot pass.

Documented in `docs/LODGEN_TERRAIN_VT.md` section 7a. Full report with every
floor: `scratchpad/lane_terrainfmt1_report.md`.
