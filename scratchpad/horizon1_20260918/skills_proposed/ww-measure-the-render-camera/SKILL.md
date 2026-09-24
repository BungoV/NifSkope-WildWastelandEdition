---
name: ww-measure-the-render-camera
description: Measure a render harness's world-to-pixel map instead of assuming it, so a world position can be drawn onto a shot as an overlay mark. Covers the four-render calibration (base plus one translation per axis), reading the columns off a phase correlation, the held-out fifth render that CHECKS the map, the pixel tolerance that stops the script, and why a map measured at one framing may not be rescaled to another. Use whenever a picture must claim WHERE something is -- a disagreement, a defect, a sampled receiver -- on a render whose projection matrix you did not write.
---

# WW: measure the render camera, never assume it

A count answers "how many"; a picture has to answer "where". The moment a
report draws a dot on a render and says "the disagreements are HERE", the dot's
position is a claim, and a wrong one is worse than no picture at all: it is a
confident, legible, made-up statement about the defect's shape.

The tempting shortcut is to read a projection matrix out of the renderer and
reproduce it in the script. Do not. You would be reproducing the matrix you
*believe* ran, in a second implementation, with its own sign and row-order
mistakes, and nothing would tell you the two disagree.

**Measure the map from the renderer's own output.**

## The procedure

An orthographic view translates RIGIDLY: move the camera by a world vector and
every pixel of the image moves by exactly the projection of it. That gives a
measurement with no matrix in it.

1. **Four renders.** One base at centre `c0`, and three more at `c0 + d*x̂`,
   `c0 + d*ŷ`, `c0 + d*ẑ`. Everything else identical -- same size, same
   channel, same sun, same scene.
2. **Read each shift by phase correlation** (FFT of both greyscales, conjugate
   product, normalise the magnitude, inverse FFT, argmax; wrap indices past the
   half-size to negative). It is exact to the pixel for a rigid translation and
   needs no features, no corners and no thresholds.
3. **A column per axis**: the image moves OPPOSITE the camera, so the projection
   of one world unit along an axis is `-(shift)/step`. Three columns make the
   2x3 world->pixel matrix `M`; a world point lands at
   `(w/2, h/2) + M · (p - c0)`.
4. **A FIFTH render the map never saw** -- a translation with all three
   components non-zero and none of them equal to the calibration steps. Predict
   its shift with `M`, measure it, and compare. **This is the whole point of
   the procedure**: three renders can always be fitted, four cannot.
5. **A tolerance that stops the script.** More than ~1.5 px between the
   prediction and the measurement and the script exits WITHOUT drawing
   anything. Print the residual in the picture's own caption so a reader sees
   the accuracy of every mark.

## What this catches, and it will

* **A view that is not the one you assumed.** An oblique view (NifSkope's
  `WW_RENDER_VIEW=8`, ViewUser) has a z column that is not vertical and x/y
  columns that are not axis-aligned. Nobody guesses those three numbers right.
* **A map rescaled between framings.** Two framings that differ only in ortho
  half-width *should* differ only by that ratio -- but prove it: measure the
  second framing on its own, five more renders, and compare the columns. In
  lane HORIZON1 they did agree (to 0.5%), and the crude test that first asked
  the question (downscale one render, correlate it into the other) answered
  "105 px out in Y" and was itself wrong. Either way, the five renders settle
  it and an assumption does not.
* **A silently changed harness.** Re-run the calibration whenever the exe
  changes. It costs five renders.

## The rule the marks must also obey

The overlay data and the numbers in the report must come from **one run**.
Write the dump from inside the same pass that prints the census (in HORIZON1,
`WW_HORIZON_REFUTE_DUMP=<dir>` inside the refuter's own `census()`), emit a
`…DumpRows` token beside the sample count, and check the two are equal. Then
sha1 the artefact the pictures were rendered from against the artefact the dump
came from. A picture whose provenance is "a bake like the one in the table" is
decoration.

## Worked example

`scratchpad/horizon1_20260918/compose.py` (`calib` and `raycast` modes) and
`shots.sh` (`calib` mode). Close framing, 1400x1091 at ortho 2600:
columns `(+0.184,+0.088)`, `(+0.196,-0.082)`, `(0.000,+0.242)` px per world
unit, held-out check off by **0.80 px**. Wide framing at ortho 8192:
`(+0.0585,+0.0275)`, `(+0.0620,-0.0260)`, `(0.000,+0.0765)`, check **0.36 px**.
