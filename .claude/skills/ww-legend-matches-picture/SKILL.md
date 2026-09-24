---
name: ww-legend-matches-picture
description: Prove that a viewer's printed legend is the colour it actually drew, by dividing two renders of the same scene instead of hunting for pixel values. Use when an overlay, a debug view or a heat map prints an rgb next to a count, and nothing has ever checked that the two agree.
---

# The legend and the picture are two code paths until something compares them

A viewer that tints geometry by category almost always ends up with two
separate pieces of code: the one that writes the colour into the vertices, and
the one that prints the legend. Nothing makes them agree, and nothing notices
when they stop. NifSkope's cell view ran for a day with the draw site writing a
flat grey 0.35 for its "unknown" bucket while the legend printed mauve
0.60,0.21,0.37 for the same key -- every gate green, because every gate read the
notes and none of them looked at the image (lane CELLVIEW3, 2026-09-19).

## Why you cannot just look for the colour in the image

The overlay is usually a **vertex colour**, and the shape keeps its own texture.
What lands on screen is `overlay x texture x lighting`, so the legend's
0.35,0.35,0.35 appears nowhere in the file. Searching the PNG for it finds
nothing and proves nothing.

## The measurement: render twice and divide

Render the SAME scene from the SAME camera twice -- once with the overlay off,
once with it on. Then, per pixel:

    ratio = overlay_pixel / plain_pixel

Texture, lighting and tone mapping are identical in both frames and cancel. The
per-channel **median** of that ratio over the pixels the overlay changed is the
colour the overlay actually drew over most of the tinted geometry. Compare it
with the rgb the legend printed for its busiest key.

Practical points, all learned the hard way:

* Mask to pixels that CHANGED (`max|a-b| > 6`) and are LIT in the plain frame
  (`min channel > 24`). A near-black pixel divides into noise.
* Take the median, not the mean: specular highlights are additive, not
  multiplicative, and they drag a mean.
* Refuse, do not pass, when fewer than a few hundred pixels qualify.
* Tolerance 0.08 in RGB distance is comfortable; the real measurements land
  around 0.01.
* The reference implementation is
  `tests/spells/cell_legend_colour.py` in the NifSkope WW tree.

## The control comes free

Print the distance from the measured colour to **the colour the broken code
printed**, as a literal. That number is the row failing on the pre-repair state,
computed without building or launching the old binary -- which matters in trees
where running an old rung has side effects (NifSkope's older rungs rewrite the
user's Recent Files list). Measured example: distance to the drawn grey 0.014,
distance to the mauve the old legend printed 0.283.

## Repair the cause, not the row

Once the row is red, the fix is not to correct the legend's number. It is to
give both sides ONE function -- `overlayKeyColour(key, rgb)` -- that the draw
site and the legend both call, so the two cannot disagree again. The gate then
guards the property, not the constant.

## A sentinel is not a key

If the legend has "unknown"/"none"/"not applicable" buckets, give each its own
reserved key with a FIXED colour rather than letting the hash wheel colour it,
and print the sentinel's words beside the number. One grey bucket that means
two different things is a bucket that cannot report a defect: split it, colour
the correct half neutral and the defective half loud, and assert the defective
half is empty.
