STATUS: DONE

# LIGHTSYS2 report

- Branch: `cloud-lightsys2`, from `main` at 5c98715.
- Commits: `7cd0051` (the lists, the shaders, both gates), plus this report.

## Files changed

New:
- `src/gl/cellcluster.{h,cpp}`: the builder. Pure C++, no Qt and no GL.
- `src/gl/cellclustergl.{h,cpp}`: per-draw rebuild, upload and uniforms.
- `res/shaders/cell_cluster.glsl`: the shared lookup.
- `tests/cellcluster/cellclustertest.cpp` and `tests/spells/cell_cluster_unit.sh`: the C++ unit test of the builder.
- `tests/spells/cell_cluster_check.py`: the independent Python twin.

Changed:
- `src/gl/celllights.cpp` (+7 lines): keep the uploaded light texels, call `wwCellClusterUniforms`.
- `res/shaders/cell_lights.glsl`: the include, plus five loops (`cellSumLights`, probe 1, probe 7, the far probes 91-94, the pass red "direct").
- `res/shaders/pbrm_default.frag`: the PBR light loop.
- `res/shaders/fo4_effectshader.frag`, `particles.frag`: a one-line comment each (see open question 1).
- `NifSkope.pro`: the four new sources.

## What it does

- **Clusters.** 80 px screen tiles x 24 slices, logarithmic in view depth between the camera's near and far planes. The near and far planes are read from the projection matrix the draw uses. This is the log slicing of Light Limit Fix (skyrim-community-shaders) and of Olsson et al. 2012, "Clustered Deferred and Forward Shading".
  - The end slices extend to the eye and to infinity, so nothing falls outside a slice.
  - Override with `WW_CELL_CLUSTER_TILE` and `WW_CELL_CLUSTER_SLICES`.
- **Build (CPU, per camera).**
  - Each cluster's view-space AABB is padded by 1 px, by 1e-3 of depth either side of a slice, and by 1e-4 of radius.
  - For each light: the slices its sphere spans, then per slice the screen rect of the sphere clipped to that slab, then per row the tiles the light really touches.
  - Culling: omni and box lights by sphere vs AABB. A spot by its cone vs the box's bounding sphere (Wronski 2016, "Cull that cone!"). A hemisphere by its half space vs the box's highest point.
  - (cluster, light) pairs go into a stable counting sort, so each list is in ascending light order.
  - Lights are split over up to 8 threads in contiguous chunks, merged in order.
- **Exactness.** A light is left out only where the flat loop's term is exactly 0 (out of range, outside the cone, behind the half space). The list keeps the flat loop's order, so the summed lighting is the same bits.
- **Overflow.** A cluster with more than 512 lights (`WW_CELL_CLUSTER_MAX`) stores count -1, and its pixels walk every light. The overflow is counted in the `cell cluster:` stderr line, which is printed whenever the lists' shape changes (`WW_CELL_CLUSTER_ECHO=1` prints every build). No light is ever dropped.
- **GPU.**
  - One `GL_R32I` buffer texture on unit 24: per cluster (offset, count), then the indices. A GPU without unit 24 binds the last unit and never turns the lists on.
  - `cellLightsBegin(P, n)` and `cellLightAt(base, k)` replace `for (i < cellLightCount)` at every surface loop.
  - The cluster comes from `gl_FragCoord` and the view depth of world P.
- **Which draws use it.**
  - The lists are rebuilt when the projection, the viewport, `scene->view` or the light buffer change. Every rebuild is keyed on what the draw actually uses.
  - More than 4 rebuilds within 16 ms (a pass switching cameras draw by draw) leaves the remaining draws on the flat loop.
  - `WW_CELL_CLUSTER=0` turns it off. `WW_CELL_CLUSTER_RED=midslice` is the refuter.
- **Probe 94.** It now counts the lights the shading loop really walks: the list length, or every light when there is no list.

## What was checked, and how

**1. Python twin, `tests/spells/cell_cluster_check.py`.** Independent: cluster-major brute force, with its own cone test (an angle form) and its own hemisphere test (box corners). It uses a 960x540 synthetic depth buffer (wall, floor, pillars) and random omni, spot and hemisphere lights. For every pixel, every light whose range reaches it must be in its list, and the float32 sums must be bit-equal.

| Lights | Green: missed | Green: unequal px | Green: mean list | Red midslice: missed | Red midslice: unequal px |
|---|---|---|---|---|---|
| 16 | 0 | 0 | 2.6 | 14576 | 14289 |
| 64 | 0 | 0 | 9.3 | 44514 | 35098 |
| 256 | 0 | 0 | 37.9 | 191692 | 104366 |
| 1024 | 0 | 0 | 154.7 | 748106 | 196262 |

Green PASS, red FAIL at every count: `GATE PASS (green PASS, red FAIL)`.

**2. C++ unit test, `tests/spells/cell_cluster_unit.sh`.** It runs the real builder at 1280x720 against the same per-pixel rule.

| Lights | Missed | Unequal px | Mean walked | Refs |
|---|---|---|---|---|
| 16 | 0 | 0 | 2.8 | — |
| 64 | 0 | 0 | 15.8 | — |
| 256 | 0 | 0 | 46.9 | — |
| 1024 | 0 | 0 | 201.1 (the flat loop walks 1024) | 520491 |

`--red` FAILs at every count (1024: 2249934 misses). Output: `CELL CLUSTER UNIT PASS`.

**3. GLSL.** Every lit program was expanded through its includes and checked with `glslangValidator -S frag`, on main and on this branch: fo4_cell, fo4_cellcsm, pbrm_cell, pbrm_cellcsm, fo4_effectcell, fo4_effectdecal(csm), particles_cell, fo4_fog(csm), cell_volfog, fo4_water, impostor_oct, impostor_cell, cell_hdr and cell_bloom_src. All OK. glslang does not model the driver's link limit, so shoot probe 5 first (see open question 2).

**4. Build time** (this VM, 4 cores, shared with a compile). These numbers are not the gate.

| Lights | Single thread | Threaded |
|---|---|---|
| 16 | 0.2 ms | — |
| 256 | 1.5 ms | — |
| 1024 | 4.7 ms | 3.7 ms |

These are synthetic scenes; the unit test's denser lights take about 15 ms at 1024. That is over the 1 ms bar here. The local gate on the real machine decides. The cost is about 300k candidate tests at about 16 ns each on this VM.

**5. Linux build:** Not finished. A full in-source build with a source-built Qt 6.8.3 was still compiling when the session wrapped up. The new files compile with -fsyntax-only against Qt 6.4 headers, and the unit tests build and run.

## Open questions

1. `fo4_effectshader.frag` and `particles.frag` keep their "every light" loops flat. Those loops run only under `fxLitMode & 2`, the red control. An effect's light ignores the hemisphere clip, which the lists cull by, so a list could drop a hemisphere light that the effect would still draw. Options:
   - leave them flat (as now);
   - cull hemispheres as spheres for everyone and cluster these too, at a small cost to surfaces.

   Which do you want?
2. **The link limit.** `cellLightsBegin` adds a small inlined call at five sites in `cell_lights.glsl` (plus one in pbrm). Earlier lanes found `fo4_cell(csm).prog` close to the driver's link limit. Shoot probe 5 first: it must be dark, mean about 1. If the cell program falls back to legacy, the first thing to try is calling `cellLightsBegin` once per shader and passing (base, n) down.
3. **Build cost.** If the 1 ms bar fails on real cells at 1024 lights, the next steps are, in order:
   - 96 px tiles and 16 slices (`WW_CELL_CLUSTER_TILE=96 WW_CELL_CLUSTER_SLICES=16`, no rebuild needed to try);
   - keeping the worker threads alive between builds;
   - skipping the rebuild when only the TAA jitter moved, since the 1 px pad already covers a sub-pixel jitter.

   The third is not done, because it would weaken the "keyed on what the draw actually uses" rule.
