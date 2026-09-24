# WW_CHANGES.md -- REPLACEMENT text from lane RESUME3 (delivered, NOT applied)

**What this supersedes.** `WW_CHANGES.md` currently carries two BUILD PENDING
entries at the top, both measured as LF-only (0 of 35 and 0 of 55 lines end
CRLF; the file as a whole is mixed, CR 19,020, and stays so):

* **lines 79-113**, `## 2026-09-11 -- lane SPLAT1: the landscape textures are
  baked 6x too large (MEASUREMENT ONLY, NO CODE)`
* **lines 114-168**, `## 2026-09-11 -- The bake's thread-safety blocker,
  re-opened and given an experiment`

Both say "not built, not applied". Replace **lines 79 through 168 inclusive**
(that is both entries and the blank line between them) with the single entry
below, which is the measured one. Line 78 and line 169 are untouched. The new
text is LF-only, matching its neighbours.

---

## 2026-09-11 -- The bake's thread-safety blocker, NAMED and fixed; and the landscape textures stop being baked 6x too large

Two long-running blockers closed in one build, both with the measurement beside
them rather than a theory.

### The bake fault was never in the NIF parser

The previous round shipped the LOD generator's chunk fan-out switched off with
the blocker named as *"the NIF parser is not thread-safe"*. That came from one
stack taken in the middle of a whole chunk bake, where the parser, the plugin
reader, the texture cache, the archive layer and the message sink are all live
at once -- and for a heap-corruption fault a stack names where the damage was
detected, not where it was done.

**The experiment that separated them.** `NifSkope -no-gui parsestress`
(`src/nifparsestress.{h,cpp}`, `tests/spells/parse_stress.sh`) reads the
fixtures once on the calling thread, then builds, loads, walks and destroys
documents on N threads with no plugin reader, no texture cache, no archive
lookup and no file I/O in the threaded region; every worker must reproduce the
single-threaded reference digest of what it read back, so corruption that
happens not to fault still fails. **20 consecutive runs at 16 threads: 10,240
loads, 0 digest mismatches, 0 faults.** Both floors were seen red first -- one
flipped byte for one worker comes back as a named mismatch, and putting every
worker on one shared document segfaults.

**What it actually is.** With the parse lock removed, the 9-chunk Sanctuary
region faulted **3 of 5** at `--chunk-threads 16` (`0xC0000374`,
STATUS_HEAP_CORRUPTION). Four symbolised faults were caught, all under
`ChunkThread::run -> lodgenBakeTerrainTextures -> LodgenRoadSet::gather ->
addPlacement -> lodgenLoadModel`; **two of the four are inside
`cliMessageHandler`**, and the other two are an innocent `QList` reallocation
that reached the corrupt heap first. `cliMessageHandler` wrote through `err()`
-- a function-local `static QTextStream` with no lock anywhere -- and
`GameResources::get_file` calls `qWarning()` on every miss, which the road pass
does on every placement. Sixteen workers grew one `QString` write buffer at
once.

The handler now writes the line with `std::fputs` to `stderr` under its own
mutex; the C runtime locks the `FILE *`. **On one thread the bytes and their
order are exactly what the old code produced.**

* **`--chunk-threads 16` is clean: 20 of 20 on Sanctuary and 20 of 20 on
  Boston**, against 3 of 5 faulting before.
* BAKEPERF1's process-wide parse lock in `lodgenLoadModel` is **gone** -- it was
  containment for a fault that was not in the parser.
* Also made safe, because the stack named the function they live in: the shared
  archive index (`GameResources::init_archives` / `close_archives` /
  `find_file` / `get_file` now under one recursive read/write lock, read on the
  hot path so the texture stage keeps its time), and `Message::append` /
  `Message::message`, which built `QMessageBox` **widgets** on whatever thread
  called them -- not reachable from the command line, but live for the LOD
  Generation panel.
* Also fixed, a plain editor bug: the block-tree name column's
  `QHash & map = arrayPseudonyms; map = multiArrayPseudonyms1;` does not
  re-point the reference, it copy-assigns into the application's global
  singular-name table, so the first multi-array row ever displayed replaced that
  table for the rest of the session.
* **`--chunk-threads 0`** now means the smaller of the core count and what free
  memory holds, and the bake census says which bound decided it
  (`chunk threads N bound by cores|memory|asked`).

Ways back, unchanged and exact: `--chunk-threads 1` is the bake that has always
run and is byte-identical to the previous exe, and `--threads 1` turns the
general fan-out off as well.

### The landscape textures were tiled 6x too large

bungo asked whether the far-terrain bake samples the landscape textures at their
correct scale. It did not.

* The bake tiled every landscape texture at **2,048 world units a repeat**. The
  engine's own repeat is **341.3333**: `fLandTextureTilingMult` = 1.5f in
  `Fallout4.exe` 1.10.155 (Setting record at file 0x36E83A8, its single code
  reference at VA 0x1403A74C6), and the 17x17 landscape quadrant loop at
  0x1403A7620 stores `u = column * mult/4 = column * 0.375` over a 2,048-unit
  quadrant, i.e. 128 units a vertex step -- **128 / 0.375 = 341.3333**, six
  repeats a quadrant, twelve a cell. The bake was **6.0000x** too coarse.
* **The default moved to 341.3333.** All fourteen sampling sites -- colour, mask
  and emissive, in both the stock chunk bake and the pyramid -- read the one
  value, so the mask sheet cannot end up describing a different patch of ground
  from the colour beside it. The normal sheet (`_msn`) comes from VHGT and is
  byte-identical at both values, which is a gate, not an assumption.
* **`--land-tiling <units>` is the switch and `--land-tiling 2048` is the exact
  way back** -- byte-identical to the previous exe on every file of both test
  tiles. It exists because another user can set `fLandTextureTilingMult` in an
  INI.
* The MIP SELECTION was not at fault: it already picks the footprint-matched mip
  (5.00 for all 20 layer textures, every one 2048x2048 with 12 mips), and an
  exact box mean over the footprint is 0.50 units smoother, not 52. Ruled out
  with their own numbers as well: the grass tint, VCLR, the BC1 codec and the
  17x17 blend.
* **The tiling fix does NOT close the whole-tile colour error against vanilla.**
  That is the grading -- vanilla darkens road and background alike by x0.82-0.83
  -- and it is still open as "splat calibration vs vanilla grading". Anyone
  reading "the terrain bake was fixed" should read this line too.

`docs/LODGEN_TERRAIN_VT.md` 2.5 now states the tiling as `T` with its provenance,
and the runtime formula carries the same `T` -- ring 0 seams if the two disagree.
The same section's claim that Sanctuary's VCLR bytes run 249..255 did not
reproduce (11 of 16 cells carry one, over 203..255) and has been corrected.

Reports: `scratchpad/lane_resume3_report.md`, with
`scratchpad/lane_nifparse1_report.md` and `scratchpad/lane_splat1_report.md`
carrying their `## Build (RESUME3)` sections. Pictures:
`scratchpad/resume3_20260911/images/cmp_tiling_fixed.png` and
`scratchpad/pic_grass_20260911/images/cmp_grass_tint.png`.
