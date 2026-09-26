# Lane GPU1 -- measure the LOD bake, then move its heaviest stages onto the GPU

Worktree E:\Projects\NifskopeWWE-gpu1, branch gpu1-20260926, from main 422881d4. Written incrementally.
Rung exe (this worktree's first build, 22:15:23): sha1 10fdde108ce6cbd087d540920660b3fc06211c94, kept as
release/NifSkope.before_gpu1.exe.

## 1. Skills loaded
- nifskope-ww-lodgen, nifskope-ww-build-verify, ww-measure-before-you-parallelise, ww-parallelise-a-stage,
  ww-module-off-is-identical (repo tree), nifskope-ww-worktree-build (live tree; the fresh worktree needed it).

## 2. Profile and ranked table

### How it was measured
- Two profiled Boston bakes (`prof_bake.sh`, box -8 -12 3 -1, the INCR2 whole-map switch set: native pair, VT with
  height and cover, vanilla fill, cards, arrays) on the rung exe, with `wwprof` sampling every thread every 200 ms
  (CPU burned per thread per interval + a stack walk; names offline from an unstripped link).
  - prof1: attach stalled (fixed in wwprof), so its trace starts ~208 s into the chunk stage; the VT is missing.
  - prof2: trace from chunk-stage +33 s to the end (962 s traced, 1443 CPU s, mean 1.50 busy cores).
    Chunk stage 1017 s. Stage times: meshes 653.4, textures 137.7, impostors 150.3, instances 297.4 s.
- Whole map from the INCR2 full log (`incr2/stage/logs/cw_chunks_full.log`, 7639 s wall, 3060 chunk jobs, 12276 VT
  tiles of which 9216 painted at level 0): meshes 2703.9, textures 4345.0 (VT ~3200), impostors 120.1,
  instances 1346.2 s, identity join 1330 s, peak 9.46 GB. The AO2 whole-map log (no VT) adds: join 1615 s,
  AO face cast 194 s, texture arrays ~120 s, card arrays ~160 s, bake record ~450 s.
- Caveat: AO2 lane bakes ran on the machine at the same time as both profiles, so Boston wall seconds are
  perturbed (up to ~2x in the busiest windows). The CPU attribution per function is not affected.

### Boston chunk stage, main thread (prof2, 30 s buckets)
| trace s | busy cores | what the main thread is in |
|---|---|---|
| 0-60 | 0.6-1.0 | road set gather (`LodgenRoadSet::gather/evaluateFlat` -> model loads), 54 CPU s |
| 60-120 | 0.6-1.0 | VT tiles: `lodgenBakeVtTile` 51 CPU s = cache lookup overhead ~17 s (Qt string lower/hash/list remove per texel), texture read+decode ~25 s, painting ~9 s |
| 150-470 | ~1.0 | chunk pass: `lodgenDilateFrames` under `lodgenCard` (100 card conversions of 47 distinct cards) |
| 480-600 | ~1.0 | card arrays: `lodgenDilateFrames` again, serial |
| 600-660 | 8-12 | card/texture arrays BC7 (mode45 249 s, mode2sub 100 s, mode6 71 s, quickErr 71 s CPU) |
| 660-840 | 1.0 | AO face cast `rayHitFace` 193 CPU s: parallel over chunks, one big chunk is the one-thread tail |
| 840-950 | 1.0 | `lodgenNativeWrite` serial: the identity join (103370 ms at Boston) |
| 950-962 | 0.03 | bake record re-reads every product file (`lodgenFileDigest`): 11 s here, 115 s in prof1 (disk/AV cache) |

Dilate self CPU: 182 s in ucrtbase (memcpy) + 66 s ntdll (allocation) + 78 s own code. The old dilate copied the whole
image mask every pass of every frame; the work it does is small.

### Ranked table (whole-map wall seconds; savings are estimates until section 5 measures them)
| # | stage | whole-map wall s | bound by | (a) GPU port saves | (b) CPU fix saves |
|---|---|---|---|---|---|
| 1 | VT terrain tiles (textures stage) | ~3200 | one thread: serial tile loop; per texel a cache lookup (~33%) and texture decode (~50%); painting ~15% | <= ~500 (painting only; decode stays CPU; cannot be byte-identical because of FMA) | ~2500-2800 (tiles fanned over 12 cores in order, cheaper cache lookup; byte-identical) |
| 2 | identity join (instances) | 1330 (1615 AO2) | one thread: a per-sample grid walk over 12.16 M samples | ~1300, could be exact (components do not depend on order) | ~1200+ (grouped walk, same components, same bytes) |
| 3 | card dilate in the chunk pass (meshes) | ~1300 of 2703 | memcpy + allocation per pass per frame; conversions repeat per chunk (1323 for 79 cards) | ~1200, exact possible (integer ops) | ~1200 (frame-local dilate, early stop, frames fanned; same bytes) |
| 4 | bake record digest | ~450 (11-115 at Boston) | disk / AV re-reading every product file | 0 | ~450 (hash while writing; not built in this lane) |
| 5 | BC7 of texture + card arrays | ~200 | CPU, already 8-12 cores | ~150, not byte-identical (needs the error gate per class) | none cheap |
| 6 | AO face cast | 194 | one-thread tail on the largest chunk | ~180, not byte-identical (float ray tests) | ~150 (fan faces of one chunk; same bytes) |
| 7 | card array dilate | part of ~160 | one thread | as row 3 | as row 3 (same rewrite) |
| 8 | road set gather (VT) | Boston 54; whole map not separated | model loads, one thread | 0 | not attempted |

### What this says
- The bake is slow because long stages run on ONE thread, not because the math is heavy. Every row above 5 is a
  single-thread stage whose fix is byte-identical CPU work. Together rows 1-4 are ~6000 of ~7600 s.
- The GPU helps where the math is heavy and already parallel: BC7 (row 5) and AO (row 6), ~330 s whole map.
  Neither can match the CPU bytes (the CPU build fuses multiply-adds; the GPU rounds differently), so both need the
  "no worse on the stage's own error measure" gate.
- Order of work: CPU fixes 3, 2, (texture cache lookup of) 1 first; then GPU BC7; then GPU AO last after the main merge.
