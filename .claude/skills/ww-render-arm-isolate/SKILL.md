---
name: ww-render-arm-isolate
description: Find which INPUT a rendered defect actually comes from, when a picture shows a wrong colour or shading on generated geometry and more than one file feeds that pixel — mesh, colour sheet, normal sheet, material sheet, vertex channel. Covers building one resource root per arm, the mix arm that proves a root is read at all, holding the mesh byte-identical so only the sheets can differ, the synthetic flat-input probe that measures the renderer's own response, and the vertex-colour probe. Use before writing any code to "fix" a rendered defect, and especially when a brief already names the cause.
---

# WW: isolate a rendered defect to ONE input

A brief that names the cause is a hypothesis, not a finding. Lane TERRAINFMT1
was told a far-terrain chunk read blue-purple because our colour sheet was DXT1
where vanilla's is DXT5, and built the DXT5 writer. The DXT5 sheet renders
**0 pixels** different from the DXT1 one. The blue-purple was in the mesh's
vertex colours, which no sheet switch could ever have touched.

The cost of finding that out at the END was one build and two gates spent on a
switch that cannot change a colour. The cost of finding it out FIRST is four
renders.

## 1. One resource root per arm, and get the SHAPE right

Build a directory per arm holding only the files that arm changes:

    <root>/Textures/Terrain/Commonwealth/<sheet>.DDS

**Not `<root>/Data/Textures/...`.** The game path baked into the file names the
`Data\` prefix and the resolver strips it itself. A root one level too deep
MISSES SILENTLY: every arm falls through to the same fallback, every PNG comes
out byte-identical, and that reads exactly like "the switch changes nothing".

Drive it with `WW_LODGEN_RESOURCES=<root>` and an absolute `WW_RENDER_SHOT`.

## 2. The MIX arm: prove the root is read at all

One arm carries file A from vanilla and file B from your bake. If the roots are
never read, that arm is byte-identical to the pure-vanilla arm. If it differs,
both files are sampled and by how much each contributes is already bounded.

Report the mix arm's number even when it is boring. A before/after pair with no
mix arm cannot tell "the switch does nothing" from "the root was wrong".

## 3. Hold the MESH byte-identical across every sheet arm

`cmp` the mesh out of every bake and say so in the report:

    rung dbe3328b06c9 / ourleg dbe3328b06c9 / ourvan dbe3328b06c9 / cache dbe3328b06c9

Then every pixel that differs between those arms came out of the sheets and
nothing else, and that sentence is earned rather than assumed.

## 4. Pin the camera when the MESHES differ

Two different meshes auto-fit to two different cameras, and the auto-fit runs
in `paintGL` AFTER the hook sets the camera. Use `WW_RENDER_CENTER` +
`WW_RENDER_ORTHO` + `WW_RENDER_DIST`, and read the achieved framing back out of
`release/ww_camera_pin.log` (`upp`, `vp`) into the picture's caption. Never
quote the requested size: `WW_RENDER_SIZE` is `max(width, a per-build floor)`
by `height - 59`.

## 5. The SYNTHETIC FLAT input: measure the renderer's own response

Before blaming a sheet's content, find out what the renderer does to a sheet
whose content you chose. Write two roots whose colour sheet is a flat grey and
a flat red, everything else unchanged, and render them.

* flat red renders red -> the colour sheet decides the hue, so a wrong hue is
  in the sheet or in something multiplying it;
* flat grey renders strongly blue -> something multiplies the albedo per
  channel, and the ratio of the two arms gives you that multiplier directly.

A flat DDS is cheap to write if you copy a header the APPLICATION ITSELF wrote
and patch only the size fields and the mip count — then nothing in the probe
depends on your reading of the format spec.

## 6. The VERTEX-COLOUR probe

`WW_RENDER_FLAT=1` draws vertex colours only, no texture, no lighting. Run it
on your mesh AND on vanilla's. It is one render each and it answers "is the
cast in a sheet or in the geometry" outright:

| mesh | flat render mean |
|---|---|
| vanilla's | 237.3 / 237.3 / 237.3 |
| ours | 50.9 / 49.3 / 202.1 |

A generator that packs DATA channels into the vertex colour slot — a material
class, an occlusion term, a wetness — hands every consumer that multiplies the
albedo by the vertex colour a tinted world. That is a real finding about the
files, and it is NOT a recommendation about how anything should look.

## 7. The number, and its floor

Mean Euclidean RGB distance to the reference render, over a mask that is the
pixels covered in EVERY arm, so no arm can win by covering less. The BEFORE
arm is the floor. Report the median beside the mean, and when a block of pixels
disagrees with the reference IDENTICALLY in every arm, name it — that is a
coverage difference in the geometry, not a colour one, and it belongs outside
the number:

    pixels identical-in-every-arm: 82,388 (6.22 %), mean distance 426.3
    BEFORE 137.77   AFTER 0.48   (mean off that region)

## 8. Switch names are not synonyms

`--no-identity` and `--no-terrain-identity` are different switches in this
tree, and the first leaves the terrain mesh byte-identical. Before reporting
"the switch did nothing", `cmp` the artefact and check you turned off the thing
you meant to.
