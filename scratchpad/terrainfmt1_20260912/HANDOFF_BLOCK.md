## LANE TERRAINFMT1 — the far sheet format lands, the blue-purple turns out not to be a sheet, the detail term is refused

Main tree, `E:/Projects/NifskopeWildWastelandEdition`, branch `main`, launched
2026-09-12 12:33. Nothing committed. Report:
`scratchpad/lane_terrainfmt1_report.md`. Deliverables:
`scratchpad/terrainfmt1_20260912/`.

**Exe as it stands:** `release/NifSkope.exe` 2026-09-12 **12:58:48**,
**22,007,808 B**, sha1 **`ba7585cba389c8b38f0e07c6c11063e0cec1124c`**. One
build. The rung `release/NifSkope.before_terrainfmt1.exe` is 11:51:35,
22,000,128 B, sha1 `ecf5ecab537f70a3409f2df3da4f4bbda2b08708` -- byte-for-byte
the exe GROUND1's own handoff block recorded when it closed at 12:28, so the
rung is provably the launch exe. `make -n` emits zero compile lines; all six
objects that include the changed header are newer than it. **Any NifSkope window
bungo had open is running an older binary and needs a restart to get either new
switch.** Zero NifSkope and zero Fallout4 processes left running.

### Two switches, both off by default, both off == the rung's bytes

* `--sheet-format vanilla|legacy` (default `legacy`) -- colour and `_msn` as
  DXT5 with the full 10-mip chain and alpha 255, which is what all 6,120
  Commonwealth sheets were measured to be.
* `--msn-cache DIR` (default off) -- ingests bungo's cleaned 2K `_msn` PNGs and
  writes the `_msn` uncompressed B8G8R8A8 / DX10 dxgi 87 with a full mip chain.
  His cache directory was opened read-only and never written to.

Off against the rung: the **whole** output tree, **10 files, 0 differ**,
`.lodb` switches hash included.

### The finding that matters, and it refutes the brief

`--sheet-format vanilla` moves the rendered picture by **0.00 levels over 0
pixels**. The blue-purple cast is our `.BTR`'s own vertex colours:
`src/lodgen.cpp:952` packs the terrain identity channels there (R = material
class, G = wetness, B = occlusion, A = shore). `WW_RENDER_FLAT=1` on the same
chunk -- vanilla 237.3/237.3/237.3, ours **50.9/49.3/202.1**. Mean distance to
vanilla's render **137.77 before, 0.48 with `--no-terrain-identity`**.

**Owed to bungo, and it is a decision not a defect:** `docs/LODGEN_PARITY.md`
says *"Default output carries none of it"* about those channels;
`src/nifcli.cpp:6442` defaults `lgTerrainIdentity` to **true**. The document and
the code disagree and this lane changed neither. No look recommendation was
made.

### Gate F3 (the `_msn` detail term) — REFUSED, no code, no flag

The candidate reaches 35 % of vanilla's texel-scale roughness at strength 1 and
needs ~5.2 to match it, at which strength the 341.333 tiling repeat rings at ten
times vanilla's level. Its detail correlates with vanilla's at mean |r| 0.0010
against a phase-randomised twin floor of 0.0011 -- indistinguishable from
structureless noise, and our existing coarse bake scores 0.0116, ten times
better. There is no `--msn-detail` flag and no second build is owed.

### Gate F5 / the cache, and the number that is bungo's call

Per sheet the uncompressed 2K set is **21x** vanilla's BC1 `_msn`; at 1K mip 0
**6x**; BC7 would be 6x at 2K and 1.5x at 1K. Dropping mip 0 to 1K costs a
factor of four in size and half the linear resolution, and still leaves a 2x
linear gain over vanilla. The cleaned sheets keep 92.9 % of vanilla's fine
energy at the 512 band. **Load cost was not measured** -- nothing was installed
and nothing was timed in the game. **His call; this lane does not make it.**

### Harnesses

Nine at GROUND1's baselines, all matched: `lodgen_roads` 11/0, `lodgen_native`
18/0, `lodgen_native_baseline` 25 files / 25 baked / 0 differ,
`lodgen_terrain_vt` 41/1 (the baseline's own V9c), `lodgen_ground_cover` 29/5,
`lod_generation` 116/0, `lodl_open` 23/0, `lodgen_terrain_pbrm` 14/0, `animws`
236/0/1 skip. The ground-cover five were compared BY NAME, not by count: the
same harness re-run against the rung exe gives the identical name list, its one
extra failure being `C0 the exe is newer than every source`, which a frozen exe
cannot pass.

### Red, and not measured

* `docs/LODGEN_PARITY.md` vs the CLI default -- above.
* `lodgenBlendVanillaDetail` (`src/lodgen.cpp:7083`) adds the east detail to the
  north component and the north detail to the east one, on the non-default
  `--land-detail-source vanilla-blend` path. Found, **not fixed**; a fix needs a
  build. In `MISTAKES_ENTRIES.md`.
* Not measured: any in-game behaviour or load cost of either switch; nothing was
  installed. The harnesses run with both switches off, so nothing gates the
  vanilla-format or cache output beyond the sheet probes and the render pair.

### Next lane could take

1. The `LODGEN_PARITY.md` / default disagreement, once bungo says which way.
2. The `lodgenBlendVanillaDetail` axis swap (one build, with an asymmetric
   fixture as the gate).
3. Whether the cache ships at 2K or 1K, and in what container -- after his call.

Skills added this lane, both in this tree's `.claude/skills`:
`ww-render-arm-isolate` (new), `ww-texel-picture` (section 8, the ARM page).
