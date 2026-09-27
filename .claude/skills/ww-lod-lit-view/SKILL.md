---
name: ww-lod-lit-view
description: Render the WW NifSkope far-LOD view (terrain .lodl + VT sheets, .lodi/.lodo objects and trees) with real lighting -- one sun, one sky, the PBR BRDF over the baked normal/mask sheets, the objects' _n/_s and the per-vertex AO x sky -- and prove it (OFF cmp-identical, lit repeatable, normal and spec refuters, isolated terms). Use when asked for "the terrain lit", "normals/specular/gloss/AO working on the LOD", "sun only / sky only / specular only", or a lit whole-map picture.
---

# The lit LOD view (lane LIT1, 2026-09-27; branch lit1-20260927, commit ebcc7bc3)

Render mechanics: skill `nifskope-ww-render-shot`. Whole map past the object cap: skill `ww-whole-map-picture`.
Working scripts (copy, do not retype): the LIT1 session scratch `lit1/` = `shot.sh`, `run_boston.sh`,
`whole.sh`, `gates.py`, `sheet.py`, `composite.py`; report `scratchpad/lit1_20260927/DONE.md`.

## 1. Switches (environment only, no menu row)
| env | values | effect |
|---|---|---|
| `WW_LODL_LIT` | `1` | shapes with SLSF2 LOD_Landscape / LOD_Objects draw with `lod_lit.prog` |
| `WW_LODL_LIT_TERM` | `all` (default), `sun`, `sky`, `spec` | one term alone (spec = sun specular + sky specular) |
| `WW_LODL_LIT_RED` | `flipnorth`, `flipup`, `nospec`, `sunmirror` (combine with `,`) | refuters |
Refused, with a note line, while `WW_LODL_AO` or `WW_LODL_CHANNEL` is set: a data view wins.
Every lit document logs `WW_LODL_LIT: sun elevation ... program lod_lit.prog` and, for terrain,
`terrain mask sheet ... bound in slot 7 on N of N sheet tiles`, and for objects the visibility stats line.
**Grep all three before captioning.** "0 of N tiles" = the bake has no mask sheet; the picture is not lit terrain.

## 2. Facts that bite
* The terrain `_msn` sheet is R east, **B north, G up**. "Flip the green" is the UP axis: it blacks the map
  (lum 1.2 vs 120). The side-swap refuter is `flipnorth` vs `sunmirror`: they must be the SAME picture
  (mean abs diff 0.00), and far from normal (21.2).
* `normalMatrix` is used as `v * normalMatrix` (world/model -> view) in `fo4_default.*`. Sun, msn and object
  tangent normals are all in view space that way; `normalMatrix * v` goes back to world (sky lookups).
* Object visibility travels in the vertex ALPHA (RGB stays the library colour). VERTEX_ALPHA buckets keep their
  opacity and get visibility 1 (counted). The visibility darkens the SKY term only.
* The native view draws NO cards (trees are the .lodo tree meshes) and NO water surface. Do not promise either.
* LOD object materials give little specular: in the spec-only panel most buildings/rocks are near black.
  That is their `_s`/BGSM, not a wiring fault (the terrain riverbed is bright in the same panel).
* `lod_lit.frag` includes `pbrm_default.frag` whole with `#define main wwPbrmMainUnused`. Never copy the BRDF.

## 3. Gates (all four, numbers in the report)
1. OFF: the lane exe without `WW_LODL_LIT` vs the baseline exe, and vs the last shipped picture of that camera
   -> `gates.py same` = file bytes identical. Do it for the AO view AND the default view.
2. Two lit runs -> bytes identical.
3. Normal refuter: terrain only (`NOOBJ=1`), top view 1, `TERM=sun`, normal / flipnorth / sunmirror ->
   `gates.py refn`.
4. Spec refuter: `TERM=spec RED=nospec` -> `gates.py black`. Background leaks through sub-pixel cracks: check the
   leftover pixels' colour ratio equals the background's before calling it a fail.

## 4. Pictures
* Label every panel (`sheet.py out scale cols path[@x0,y0,x1,y1] label ...`).
* Whole map: `whole.sh lit 8` / `whole.sh off 8` (T + two object halves + composite, `nocensus` on the
  oblique). Label the installed bake's date: it may predate later bake fixes.
* A close crop: take it from the 1600 picture at 2x nearest, or render again at 3200 with the same camera.

## 5. When a GUI launch dies silently (rc 0, empty log, 15-20 s)
Run a known-good older copied exe as the control first. Control works, lane exe does not = the scanner; wait
about 10 minutes and re-probe. Never rename, repack or alter the binary to get past it.
