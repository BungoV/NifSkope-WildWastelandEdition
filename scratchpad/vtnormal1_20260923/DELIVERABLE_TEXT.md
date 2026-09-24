# VTNORMAL1 -- deliverable text for the overseer to splice

Lane folder: `scratchpad/vtnormal1_20260923/` (progress.md = the full record).
Rung: `release/NifSkope.before_vtnormal1.exe` (sha1 add1bf84). Built exe: `release/NifSkope.exe` 2026-09-24 01:35:28 (sha1 97d2b3a2).
Not committed. Built on top of the uncommitted IMPOSTORDEPTH1/2, DEFAULTS2 and BLENDSEAM1 changes.

Files touched (all LF-only, 0 CR before and after):
`src/io/lodvfile.h`, `src/io/lodvfile.cpp`, `src/lodtsheets.cpp`, `src/lodgen.h`, `src/lodgen.cpp`,
`src/nifcli.cpp`, `src/lodgenmanager.cpp`, `src/nifskope_ui.cpp`, `tests/spells/lodgen_vt_check.py`.
Other lanes' uncommitted edits share these files, so commit by explicit path only after reading the diff.

---

## WW_CHANGES.md entry

**Terrain pyramid: its normal now comes from bungo's upscaled `_msn` sheets (lane VTNORMAL1, 2026-09-23).**
When the *Normal sheets folder* (`--msn-cache`) is set, the terrain virtual texture's normal is his
sheets, reduced to the pyramid's texel size, instead of the normal computed from the LAND heights.
Each dim-4 sheet `<ws>.4.<x>.<y>_msn.DDS` (or the older `.png` form) is read once, box-filtered as
unit vectors and renormalised, and written over the finest level's tiles, border included; coarser
levels are filtered from it by the existing vector box filter. A chunk with no sheet keeps the heights
normal, and the census says how many tiles took which: `normalMsnCache / normalHeights / normalMixed /
msnSheetsRead / msnSheetsMissing` on the `vt:` line and `terrain.normalSource` in the index.
- Chunk sheets (`tex/`) are unchanged: byte-identical to the rung with the same folder, including the
  dim-8 `_msn` assembled from the pyramid (the heights normal is kept in a second staging plane for it).
- Without the folder the whole bake is byte-identical to the rung (tex, obj, .lodt, .lodm).
- **Finest texel size, one choice** (`--vt-density 32|16|8`, panel row *Finest texel size*, panel
  default **16**): 32 = finest dim 2 / content 256, 16 = dim 2 / content 512, 8 = dim 1 / content 512
  (his sheets used as-is). Byte-identical to the old `--vt-finest` / `--vt-content` pairs; naming both
  is refused. The CLI default stays 32 (the previous bytes). The *Tile content* panel row is gone.
- **Half-resolution aux sheets** (`--vt-half-aux`, panel row *Half-resolution normal, mask, height and
  emissive tiles*, default OFF): colour stays at the chosen size, the other sheets store only mips 1..
  of the full sheet (bit-identical to them). Declared per sheet in header descriptor byte 6
  (`mipSkip`). No version bump: OFF is the previous bytes, and the old reader refuses an ON file by
  rule 16 (rawBytes vs header) instead of misreading it. Refused with `--vt-mips 1`.
- Whole Commonwealth, with ground cover (estimator): 32 u 2.22 GB (half 0.93), 16 u 8.26 GB (3.40),
  8 u 29.7 GB (12.9), plus 2.14 GB of chunk sheets.
- **His Upscaled Terrain Normals mod works as named** (bungo 2026-09-24): the *Normal sheets folder* may
  be the sheets' own folder, a MOD ROOT, a Data folder or a Textures folder; the reader looks in the
  folder, then `Textures/Terrain/<world>/`, then `Terrain/<world>/`. Before this, the mod root read
  nothing and the bake silently used the heights normal. An EMPTY panel row now means **auto**: the
  last resource folder holding `<world>.4.*_msn.DDS` wider than vanilla's 512 px (CLI
  `--msn-cache auto`); `none` turns the sheets off. The census says `msnCacheDir <folder> (auto)`.
  Gate `gate_auto.sh` 7/0, and the rung, given the mod root, writes the no-cache chunk sheets (the
  old-code failure, shown).
- Gate: `scratchpad/vtnormal1_20260923/gate.sh` (27 ok, 1 fail = the pre-set r bar on north, see below),
  `lodgen_terrain_vt.sh` 45/0, `lod_generation.sh` 128/0 (floor 121), `lodgen_perf.sh` all green but
  leg (c), which is a harness mismatch (HANDOFF block).

---

## HANDOFF block

VTNORMAL1 (2026-09-23/24, not committed, exe 2026-09-24 01:35:28 97d2b3a2, rung before_vtnormal1 = add1bf84):
- The pyramid normal is his upscaled sheets when *Normal sheets folder* is set. His profile had that
  folder EMPTY; at 01:3x on 2026-09-24 (director's addition) the lane set
  `HKCU\Software\NifTools\NifSkope 2.0\LodGeneration\msnCache` =
  `E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals` (no NifSkope was running). The panel writes
  its keys only on Generate, never on close; an OLD window left open would write its empty field back
  on Generate, so he restarts NifSkope before baking. The folder may now be the MOD ROOT (the reader
  also looks in `Textures/Terrain/<world>/` and `Terrain/<world>/`); an EMPTY row means auto, `none` off.
- Build 5 = `release/NifSkope.exe` 01:35:28, sha1 97d2b3a2 (build 4, 7212fdc9, has the same lodgen code).
- 16 u, sheets on vs off, 2x2-chunk region (-24..-17 x 16..23), the finest level: on and off differ by
  16.0 degrees on average (median 14.7, 92 % of texels over 5 degrees). Against his sheets: ON r
  0.939 east / 0.836 north (mean 8.8 degrees off), OFF 0.576 / 0.633 (17.5 degrees). His sheets are
  steeper: mean slope 22.6 degrees, ON 21.0, OFF 12.5.
- lodgen_perf leg (c) reads RED on this lane's rung for a harness reason: it gives the exe under test
  `--library near --native-ladder` to match a rung from before 2026-09-17, and add1bf84 already has the
  new defaults, so the two sides baked with different switches (.lodo 6.2 MB vs 223 MB). The same leg
  with equal switches (`scratchpad/vtnormal1_20260923/waybk.sh`): 56 files, 0 differ. Every other perf
  leg green. Follow-up: make the harness's `eq` depend on the rung's own defaults.
- His saved `vtFinest=2 / vtContent=256` are no longer read; his `vtDensity` reads 16 (the panel
  self-test's save wrote the default), so his panel opens at the ruled 16 units a texel.
- OWED: the r bar (0.88, set before code) fails on NORTH, 0.8624 (east 0.9449). The transfer is exact
  (the tile equals this tree's own BC1 of his downsampled sheet on 100 % of texels within 1/64); the
  loss is `lodgenEncodeBC1Block`'s min/max-LUMINANCE endpoints, which weight north (blue) at 0.114.
  A least-squares / PCA endpoint fit for normal sheets would lift north to ~0.925 (simulated), but it
  moves the no-folder bytes, so it is a separate lane and bungo's call. At 8 u the finest level reads
  0.966 / 0.935.
- Caveat: at 16 u on the stock command-line path the assembled `.btr` chunk sheets come out at 1024 px
  (content 512 x 2 cells); the panel's pyramid is FO4CS-only, so the panel is not affected.
- Time, whole Commonwealth, ESTIMATED from a 256-cell region x 144: ~39 min (32 u), ~81 min (16 u),
  ~4.8 h (8 u); half-aux saves little time (the staging is the cost, not the encode).
- Memory: his sheets are held for two chunk rows at a time; at 8 u that is about 0.5 GB.

---

## docs/LODGEN_TERRAIN_VT.md

### §2.1 (stale sentence, replace)
Old: "Full mode (`--vt-finest 1`) adds a dim-1 level ... dim 2 is *always* baked from the paint in
both modes, and filtering always starts at dim 4 reading dim 2".
Measured 2026-09-23: in full mode dim 2 is FILTERED from dim 1 (the 8 u bake's dim-2 normal carries his
sheets, r 0.942 / 0.866, which a paint bake could not). Replace with:
> Full mode (`--vt-finest 1`, or `--vt-density 8`) makes dim 1 the finest level at 16 units per texel
> at content 256, or 8 at content 512; every coarser level, dim 2 included, is filtered from the one
> below it.

### §2.2 or new §2.2b -- where the normal sheet comes from
> The `msn` sheet of the finest level is computed from the LAND heights, unless a normal-sheets folder
> is set (`--msn-cache`, panel *Normal sheets folder*). Then each finest tile's normal, border
> included, is taken from the folder's dim-4 sheets `<ws>.4.<x>.<y>_msn.DDS` (or `<ws>.4.<x>.<y>.png`,
> §7a.3): each sheet is read once as unit vectors, reduced to the level's texel size by a box filter in
> vector space (sum, renormalise), encoded, and copied texel for texel; sheet row 0 is NORTH, R is +east,
> B is +north (proved by flipping: r drops from 0.94 to 0.12). Coarser levels are filtered from it by
> §2.3. A texel whose chunk has no sheet keeps the heights normal; the census counts finest tiles as
> `normalMsnCache` (all from sheets), `normalHeights` (none), `normalMixed` (some). The chunk sheets are
> not changed by this: the heights normal is kept in a second staging plane, filtered alongside, and
> the `.btr` assembly reads that.

### §3.1 header table, row 0xA0 (amend)
> 8 bytes each: u16 `dxgiFormat`, u16 `dxgiFormatCover`, u8 `role`, u8 `colorSpace`, **u8 `mipSkip`**,
> u8 zero. `mipSkip` is 0, or 1 on a HALF-RESOLUTION sheet (`--vt-half-aux`): the sheet stores the full
> sheet's mips 1.. and no mip 0, so its stored side is `storedTexels >> 1` and it carries `mipCount - 1`
> mips. Only roles 2, 4, 5 and 6 may set it; colour never does. It raises no version: a file without it
> is byte-identical to before, and a reader that predates it refuses a half file by rule 16 (a tile's
> `rawBytes` no longer matches the size the header implies) rather than misparsing it.

### §3.4 validation (add to rule 13)
> `mipSkip` > 1, `mipSkip` >= `mipCount`, `mipSkip` on the colour role, or `mipSkip` on an unused sheet
> slot is refused.

### §4 index (add)
> `halfAux: true` and, per sheet, `mipSkip` / `texels` when half-resolution sheets were written.
> `normalSource {rule msnCache|heights|mixed, filter, tilesMsnCache, tilesHeights, tilesMixed,
> sheetsRead, sheetsMissing}` when a normal-sheets folder was set. Both are absent otherwise.

### §5 CLI (add rows, amend two)
> | `--vt-density 32\|16\|8` | 32 | world units per texel at the finest level: 32 = `--vt-finest 2 --vt-content 256`, 16 = `--vt-finest 2 --vt-content 512`, 8 = `--vt-finest 1 --vt-content 512` (byte-identical to those pairs). Refused beside `--vt-finest` / `--vt-content`, and for any other value. The panel's *Finest texel size* row defaults to 16 |
> | `--vt-half-aux` | off | §3.1 `mipSkip`: the normal, mask, height and emissive sheets store only mips 1..; colour keeps its size. Refused with `--vt-mips 1`. Whole Commonwealth with cover: 0.93 / 3.40 / 12.9 GB at 32 / 16 / 8 u against 2.22 / 8.26 / 29.7 |
> `--msn-cache DIR` row: "Read the `_msn` for each chunk from `<DIR>/<name>_msn.DDS` (R8G8B8A8, DX10,
> one mip, vanilla's channel order, stored G used) or `<DIR>/<name>.png` (the cleaned-cache law below)
> ... **and, since lane VTNORMAL1, the terrain pyramid's normal from the same sheets** (§2.2b).
> DIR may also be a mod root, Data or Textures folder: each sheet is looked for at `<DIR>/<file>`, then
> `<DIR>/Textures/Terrain/<world>/<file>`, then `<DIR>/Terrain/<world>/<file>` (<world> = the name up to
> its first dot). `--msn-cache auto` takes the LAST `--resource` folder (archives skipped) that holds a
> `<world>.4.*_msn.DDS` wider than 512 px -- vanilla's dim-4 `_msn` are 512, so an unpacked Data does
> not qualify -- or none; the census line reads `msnCacheDir <folder> (auto)`. The panel's empty row
> is `auto`, `none` is off."

### §7a.3 (append)
> The DDS form (bungo's upscaled set, 2026-09-18) is read first. Since 2026-09-23 the folder also feeds
> the pyramid (§2.2b). Measured over Sanctuary (-20,24): the L02 normal against his sheet box-reduced to
> 32 u reads r 0.9449 east / 0.8624 north, which is exactly this tree's BC1 of that sheet (100 % of
> texels within 1/64); the north loss is the luminance endpoint rule above, the same finding as the grid
> residue.

---

## MISTAKES.md entries (root file, newest at the top)

**2026-09-24 VTNORMAL1 -- a self-test step that picked the default.** The panel check "the pyramid's
summary is computed" moved the density row to index 1 and compared sentences; the lane had made index 1
(16 u) the default, so both sentences matched and the check failed on a correct panel. It now steps to
another entry than the one showing. Rule: a check that moves a control to compare two states moves it
AWAY from the current value, never to a fixed index.

**2026-09-24 VTNORMAL1 -- a folder row that took only the leaf.** The normal-sheets reader looked only
in the named folder, so naming his mod (the natural thing to paste) read nothing, and a miss is silent
by design (the heights normal stands in). Caught by the director's question, not by a gate. Rule: a
folder row that names game assets accepts the mod root and says in the census what it found.

**2026-09-23 VTNORMAL1 -- a bar calibrated on an encoder the bake does not use.** The new row's bar
(r >= 0.88 on east and north) was set from a PCA-fit BC1 simulation whose ceiling was 0.97 / 0.925.
The bake's `lodgenEncodeBC1Block` picks min/max-luminance endpoints and its ceiling on the same sheet is
0.9449 / 0.8624, so the bar failed on north with a perfect transfer. §7a.3 of the contract already said
that encoder is harsh on normals. Rule: a codec ceiling is measured with a port of the codec the bake
runs, before the bar is written.

**2026-09-23 VTNORMAL1 -- a float sum left to the compiler's contraction.** Moving a branch into the
chunk-sheet cache reader's loop flipped GCC's choice of which product to fuse in `e*e + n*n + up*up`
(-O3 -march=haswell contracts by default), and 2 texels of a 2048 sheet moved one step. Restoring the
loop verbatim did NOT restore the bytes. A whole-sheet probe of every fused form named the rung's
(`fma(up,up,fma(n,n,e*e))`, 4194304 of 4194304 texels), and it is now spelled with `std::fma`. Rule:
where a byte-identity gate covers float arithmetic that rounds to 8 bits, spell the fused form.

**2026-09-23 VTNORMAL1 -- Python inlined in a heredoc, twice.** A DXT5 patch to the lane's measure
script and an anchor fix were run as heredoc Python, against the brief. Neither carried a backslash or an
apostrophe, so nothing broke, which is luck and not a reason. Rule stands: Write the file, run the file.

---

## Skill review (nifskope-ww-lodgen)

Add under "Editing traps":
* **Float contraction moves bytes.** `src/lodgen.cpp` is compiled `-O3 -march=haswell`, where GCC fuses
  `a*b + c` into FMA by its own choice. An unrelated edit near a plain sum that is rounded to 8 bits can
  flip that choice and move a byte-identity gate by one step on a few texels. Probe the rung's form over
  a whole sheet (`scratchpad/vtnormal1_20260923/fmaprobe.py` is the pattern) and spell it with `std::fma`.
* **A BC1 ceiling uses OUR encoder.** `lodgenEncodeBC1Block` is min/max luminance; on a normal sheet
  north (blue) is nearly ignored. A numpy port of it is `scratchpad/vtnormal1_20260923/encceil.py`.
* **The `.lodb` is never byte-equal across two exes** (exe size, clock, output paths, the census lines).
  A tree-identity gate excludes it and compares it with those lines dropped.
Add to the CLI section: `--vt-density 32|16|8` and `--vt-half-aux`, and that `--msn-cache` feeds the
pyramid.
* **lodgen_perf leg (c) assumes a rung from before 2026-09-17.** It hands the exe under test
  `--library near --native-ladder`; with a newer rung that is a different bake and (c) reads RED
  (.lodo 6.2 MB vs 223 MB). Read both logs' `native-library:` lines before believing it;
  `scratchpad/vtnormal1_20260923/waybk.sh` is the equal-switch rerun.
Add to the CLI section: `--msn-cache DIR|auto` takes a mod root, and `auto` searches `--resource` folders.

## Follow-ups (not done here)
* A least-squares / PCA endpoint BC1 for normal sheets (north r ~0.86 -> ~0.93); moves the no-folder bytes.
* `tests/spells/lodgen_perf.sh` leg (c): give the exe under test the rung's own defaults, not a fixed
  pre-2026-09-17 pair.
* Panel auto-detect in Specified mode sees only the Resources rows (his are empty); a Data-folder MO2
  mods scan was not added.
