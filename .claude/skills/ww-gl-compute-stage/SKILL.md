---
name: ww-gl-compute-stage
description: Move a WW lodgen bake stage onto the GPU with an OpenGL 4.3 compute shader (offscreen context, headless too), keep it deterministic, gate it on the CPU stage's own error measure, and find out why a GLSL kernel is slow on NVIDIA (local-memory spills). Use before writing any GPU code for the bake, or when a compute shader is slower than the CPU it replaces.
---

# Put a lodgen stage on GL compute (lane GPU1, 2026-09-26)

The worked example is `src/lodgengpu.h/.cpp` (BC7 of the card normal sheets). Read it beside this page.

## 0. Is the stage worth it?
Profile first (skill `ww-measure-before-you-parallelise`). GPU1's profile said most of the bake's time was
SINGLE-THREAD stages, not heavy math: those are CPU fixes (fan out, stop re-copying) and they keep the bytes.
The GPU only pays where the math is heavy AND already parallel on the CPU (BC7: 2.3x over 16 threads).

## 1. The context (copy src/lodgengpu.cpp)
* One `QOpenGLContext` 4.3 core on a `QOffscreenSurface`, owned by a thread of its own; jobs are posted to it
  and serialised under one mutex. No window is ever shown; nothing is drawn.
* Headless: `-no-gui lodgen` normally makes a QCoreApplication, which cannot make a GL context. `main()` asks
  `lodgenGpuWantedForArgs(argc, argv)` BEFORE any app object exists and makes a QGuiApplication instead. That
  function checks the platform plugin is beside the exe first: without it QGuiApplication aborts the process.
* The surface must be created on the main thread; the context is moved to the worker thread after.
* A failure anywhere (context, compile, link, self-check, a job) turns the path off with ONE log line naming why,
  and the caller's CPU loop runs. A lost context does not come back: after one failed job the rest of the run is CPU.

## 2. Determinism rules (the gate is two runs, same bytes)
* One thread per independent output unit (a BC7 block); each is a pure function of its inputs.
* No atomics, no float atomics above all, no reductions across threads. If a pass list is needed (compaction),
  build it on the HOST from a readback, in unit order.
* Integer math wherever the result is compared or written (palette, index choice, error); floats only inside
  a search whose result is re-scored in integers.

## 3. Exact vs "no worse"
* A double-precision port can match a CPU fit bit for bit, and on a GeForce it is SLOWER than the CPU
  (doubles run at 1/64 rate; GPU1: 3.0 s vs 0.4 s). Expect float and a different byte on some units.
* Then the gate is the CPU stage's own error measure, per texture class: the class total on the GPU must not
  exceed the CPU's, measured on the REAL class (GPU1: the 79 card normal sheets), not on synthetic noise.
* Re-measure the error independently: decode the GPU output with a separate decoder (BC7: the vendored detex,
  `detexDecompressBlockBPTC`, lib/libfo76utils/src/decompress-bptc.c) and require decoded error == reported.
* Put the same check in the product as a start-up self-check on a fixed image (~0.5 s): a driver that rounds
  differently then refuses the GPU instead of shipping worse bytes.
* The incremental chunk digest must take the PATH TAKEN (not the switch): `lodgenGpuDigestWord()` is
  `|bc7gpu` on the GPU and empty on the CPU, so the CPU path's digest and bytes are exactly today's.

## 4. Why is my kernel slow? (NVIDIA)
* Dump the program binary (`glGetProgramBinary`) and search it as text for `lmem`: a large local-memory size
  (GPU1: 288 words per thread) means spills, and that alone made the float kernel slower than the CPU.
* Causes, in the order GPU1 found them: `const` lookup tables indexed by a runtime value (copied to local
  memory per thread; compute the value instead), arrays indexed dynamically (use named struct fields or
  unrolled code), and wide per-thread state (pack pixels as one uint each).
* Divergence: one thread per unit running ALL modes wastes the warp on units that finished early. Split the
  search into passes and dispatch each later pass only over the units still with error (host-built list).
  Cut the passes exactly at the CPU's own early-exit tests so the result is the same as one pass.

## 5. A standalone gate before the product build
Build a tiny exe from the product source (`src/lodgengpu.cpp`) plus a test main (GPU1:
`gputest.cpp` + `mk_gputest.sh` in the lane scratch): per image, CPU encode on all cores, GPU twice, decode both,
print differing units, error of each, times and hashes; end with the class total and GATE PASS/FAIL.
It compiles in seconds, so the kernel can be iterated without relinking NifSkope.

## 6. The switch (bungo 2026-09-26)
GPU on by default; ONE Settings row (NIF page, "LOD bake" group, "Use GPU", label + checkbox, no text) read by
the GUI bake and the headless one alike (same QSettings scope, WW_SETTINGS_SCOPE honoured); `--no-gpu` wins for
one run. The bake log says which path and why, and ends with images / blocks / GPU ms / fall-backs.
Harness: `WW_USEGPU_TEST=1` under a planted WW_SETTINGS_SCOPE (src/nifskope_ui.cpp).

## 7. Is it faster IN the bake? (the fair A/B)
Standalone numbers do not carry over by themselves, and a bake timed beside other lanes lies. GPU1's gate bakes,
run while other lanes rendered and baked, put the GPU path 16-100 s SLOWER than the CPU at Boston; standalone it was
faster at every mip level, with or without idle gaps. The fair test: same exe, back to back, order CPU, GPU, GPU, CPU
(ABBA, so drift hits both sides), on a quiet machine (no other NifSkope or make running), and compare the stage the
GPU touches as well as the whole wall time. GPU1 (scratch `ab_bakes.sh`): CPU 469/466 s, GPU 447/432 s, the card array
stage 52 -> 31.5 s. Each pair must also be byte-identical to its twin, which checks determinism for free.
