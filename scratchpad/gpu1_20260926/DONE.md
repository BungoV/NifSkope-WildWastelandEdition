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

## 3. What was built, per stage, and how each stays deterministic

### 3a. CPU fixes first (the ranked table said the slow stages are single-thread, not heavy math)
- **Card dilate** (row 3, fe4b19c1): dilate works on each frame's own rectangle, stops when a pass changes nothing,
  frames fan out over the cores and are written back in frame order; the texture cache's LRU is kept as stamps
  instead of a list remove per lookup. Same bytes: Boston 253 files identical to the rung.
- **Identity join** (row 2, 212af0be): every placement called `reserve(size()+n)` with the exact size, which
  defeats the vector's doubling, so each placement copied the whole sample array. The walk is grouped and the
  reserve lines are gone. Boston join 109175 ms -> 706 ms, same 42306 placements / 1,931,045 samples / 40,294
  pairs; the whole Boston bake is byte-identical to the one before it (cmp_trees: SAME, 253 files).

### 3b. GPU BC7 (row 5, baf912be)
- Where: the card normal sheets (`_n`, weights R1 G1 B32 A1), the bake's only BC7 images: `lodgenWriteDds` and
  `lodgenEncodeArrayLayer` in src/lodgen.cpp call `lodgenGpuEncodeBc7` first and run their CPU loop when it
  returns false.
- How: one offscreen OpenGL 4.3 context (Qt `QOffscreenSurface`, no window, headless too: `main()` makes a
  QGuiApplication for a `lodgen` run that wants the GPU) on a thread of its own; a compute shader running the
  same search as src/lodgenbc7.h in single precision, one thread per block, in three passes (mode 6; modes 4/5
  on the blocks with error left; the two-subset modes on those still with error), each pass compacted on the
  host in block order.
- First try was a double-precision port that matched the CPU byte for byte: 3.0 s on a 1024x512 image against
  0.4 s on 16 CPU threads (a GeForce runs doubles at 1/64 rate). Dropped.
- The float version first ran slower than the CPU too: the NVIDIA compiler put the constant tables and the
  dynamically indexed arrays in per-thread local memory (the program binary showed `lmem` of 288 words).
  Packed pixels, weights computed instead of looked up, named struct fields instead of indexed arrays and the
  pass compaction brought it to 2.3x faster than 16 CPU threads.
- Not the CPU's bytes: a float fit can land one code step off the double fit, either way. What IS exact: the
  palette, the index choice and each block's error are integer math, so every block's reported error is the
  true weighted error of the bytes it wrote.
- Deterministic: no atomics, no cross-thread reductions; every block is a pure function of its 16 pixels; the
  host builds the pass lists in block order.
- Refused at start-up unless a self-check passes: a fixed 128x64 image is encoded by both under two weightings
  (equal, and the card weights; 1024 blocks), every GPU block is decoded with the vendored detex decoder and
  must decode to its reported error, and the GPU total must be <= the CPU total for each weighting. Measured
  on this card: GPU 26082378 <= CPU 26090366. Costs ~0.5 s once per process.
- Fallback: context creation failure, self-check failure or a failed job turn the GPU path off with a log
  line (`gpu: CPU path -- <why>`); a failed job falls back to the CPU loop for that image and the rest of the run.
- The switch: Settings > NIF > LOD bake > Use GPU (on by default). The headless `-no-gui lodgen` reads the same
  key; `--no-gpu` wins over it for one run. The bake log's first lines say `gpu: GPU path -- setting Use GPU on;
  <card>, OpenGL 4.3; BC7 self-check ...` or `gpu: CPU path -- switch --no-gpu` / `-- setting Use GPU off`, and
  the last line counts images, blocks, GPU ms and fall-backs.
- Incremental cache: the path taken goes into the chunk digest (`|bc7gpu`, empty on the CPU path), so a cache
  from one path is never reused by the other and the CPU path's digest is today's.

### 3c. Not built, and why
- **GPU AO / sky ray cast (row 6, 194 s whole map).** AO2 merged? Yes: 53fb5d8d is in main and this branch
  starts at main 422881d4. Not built because (1) the AO2 follow-up lane is changing the AO cast right now, so a
  GPU port would chase moving code; (2) the stage is slow because the largest chunk runs on one thread (the
  profile's one-core tail), which a byte-identical CPU fan-out of that chunk's faces fixes (~150 of 194 s)
  without the float-ray error gate; (3) at 194 s it is the smallest row left. The AO spot gates (ballpark roof,
  tower face, lone box) therefore did not apply.
- **Terrain painting on the GPU (part of row 1).** Painting is ~15% of the VT tile time; the rest is texture
  decode and the cache lookup, which stay on the CPU. The big saving there (~2500 s) is fanning the tile loop
  over the cores, a CPU change; the loop shares the land/mask caches, the census and the sheet state, so it is
  a lane of its own (see section 5, what is left).

## 4. Gates with numbers

### Boston box (-8 -12 3 -1), whole bake tree (253 files), `cmp_trees.sh` (sha1 of every file; masks only the exe digest in chunk keys and the run path in the flat-objects report)
| pair | result | what it proves |
|---|---|---|
| rung exe vs dilate + join fixes (cpu1) | SAME, 253 files | the CPU fixes keep every byte |
| gpuA vs gpuB (GPU on, two runs) | SAME, 253 files | the GPU path is deterministic |
| cpu1 vs cpu2 (`--no-gpu`) | SAME, 253 files | the switch gives today's CPU bytes |
| cpu1 vs off (setting Use GPU planted off) | SAME, 253 files | the setting gives today's CPU bytes |
| cpu1 vs gpuA | 36 files differ, all expected | 16 card normal arrays (`_n`), the .lodb, the .lodo/.lodi (header CRC + card corpus hash), 17 .key files (digest word `|bc7gpu`) |

### Quality gate, card normal sheets (the only BC7 class in the bake), on the CPU encoder's own error measure
- 79 card normal sheets (`gputest`, the lane's offline harness around the product encoder, gt5.out): class total
  CPU 6254874358, GPU 6254772022 (GPU lower); 18 single images above the CPU, worst +0.011%. Every GPU block
  decodes (detex) to exactly its reported error; two GPU runs identical per image. GATE PASS.
- Decoded difference in the bake (dds_diff.py, cpu2 vs gpuA, the 16 `_n` arrays, top mip): 99.73% of pixels
  identical, worst |d| 52 of 255 on one channel of one texel; the class error above is the measure that counts.
- Start-up self-check in the product (every run): GPU 26082378 <= CPU 26090366 on the fixed test image.

### Settings row (GUI harness WW_USEGPU_TEST, second monitor, own settings scope, Use GPU PLANTED off)
- The bake read "off" before the dialog opened (the planted state was really read); the row is labelled
  "Use GPU" in the group "LOD bake" (one label there), opens unchecked, and ticking/unticking it stores
  true/false and the bake reads on/off. PASS. Scope wiped afterwards.

### Pictures
- Boston oblique, CPU over GPU: pixel-identical, because the view draws the card albedo, not the card normal
  arrays; the normal difference is shown separately (card_normal_cpu_gpu_diff.png, amplified).
