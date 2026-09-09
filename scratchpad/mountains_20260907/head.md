# Lane MOUNTAINS — why rebaked Fallout 4 terrain LOD goes dark

Read-only forensic lane. Everything below is measured on this machine unless
labelled UNVERIFIED or RECALLED. Sections 1-6 hold the evidence; the two
sections here hold the conclusion.

---

## THE ANSWER

Fallout 4's Commonwealth worldspace is 192x192 cells, but **only the playable
middle of it — cells x -36..32, y -41..32, 10.7 % of the total — has any
landscape texture painted into the ESM at all**; the mountains bungo is looking
at north of Sanctuary have height data and *nothing else*, no base texture, no
layers, and no worldspace default to fall back on. Bethesda nevertheless shipped
hand-made, per-cell-distinct terrain LOD textures out there which are measurably
**more** saturated than the playable area (meanSat 0.191 vs 0.155), so a tool
that regenerates terrain LOD from the ESM has literally nothing to sample in the
outer region and must substitute a single flat default — **that accounts for the
DESATURATION and most of the FLATNESS outright, and for the DARKENING if the
substitute is darker than vanilla's mean of lum 71.0**. Two things compound it:
xLODGen ships a documented "Default diffuse size / Default normal size" control
whose stated purpose is to shrink the textures of exactly these "outer regions
without landscape textures", and because it shrinks the **normal** map too it
removes the only thing that shades terrain at that distance — a level-32 mesh
carries **2.86 triangles per cell**, so the `_msn` is doing all the work.
What I could **not** measure: there is no rebaked LOD anywhere on this machine,
so the exact colour his tool substituted is unverified, and so is whether
xLODGen transposes the normal map's green and blue channels — a mistake his own
NifSkope fork *does* make (`src/lodgen.cpp:5026`), and which would cost **68 %
of the light and 92 % of the shading variation** on its own.

---

## WHAT TO CHECK OR CHANGE

1. **First, rule out the free explanations.** His load order has
   `F76Weathers.esp` and `UltraExteriorLighting.esp`; if the two screenshots
   were taken under different weather or time of day, part of the difference
   is not LOD. And check whether *mid-distance terrain inside the playable
   box* also went dark. If it did, the cause is global (normal map / vertex
   colour intensity / gamma). If only the outer region changed, it is the
   missing-source-data cause below. **This one observation discriminates
   between the two candidates and costs nothing.**
2. **Do not regenerate terrain LOD for the outer region at all.** Vanilla's
   outer tiles cannot be reproduced from `Fallout4.esm` — the data is not
   there (section 2.4). Generate only over roughly cells -36..32 / -41..32
   using xLODGen's Chunk options and keep Bethesda's shipped tiles outside it.
   This is the single highest-value change. Mind the documented skip rule: to
   re-do a level-4 tile you must also delete the level 8/16/32 tiles covering
   it, or generation is silently skipped.
3. **Set "Default diffuse size" and "Default normal size" to `None`.** These
   are the controls that, by their own documentation, minimise "outer regions
   without landscape textures" — the exact cells in question.
4. **Set Normal Size for LOD4 to 512, not the 256 the "native" hint suggests.**
   Vanilla FO4 ships every terrain LOD texture at 512x512 (measured); accepting
   256 gives a quarter of the texels for the map that carries all the distant
   shading. Consider enabling "Bake normal-maps" as the readme suggests.
5. **Verify the generated normal map's channel order before shipping the bake:**
   `python msn_updecide.py <out>\Textures\Terrain\Commonwealth\Commonwealth.4.<x>.<y>_msn.DDS`.
   Green must be the up channel — small residual, and zero pixels below 128.
   If it is not, that is a second, independent and much larger problem.
6. **Leave Diffuse Brightness / Contrast / Gamma alone until 1-5 are done.**
   They compensate for a bad bake rather than fixing one, and they cannot
   restore saturation or relief that was never generated.
7. **If he is using this fork's own `lodgen`, its `_msn` is wrong.**
   `src/lodgen.cpp:5021-5027` (and the same packing at `:5994-5999`) writes
   `R = east, G = north, B = up`; FO4 wants `R = east, G = up, B = north`.
   Its shader flags, by contrast, are byte-for-byte vanilla (section 3.2), so
   this is a one-line class of fix, not a redesign.

---
