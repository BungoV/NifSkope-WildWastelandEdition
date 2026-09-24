---
name: ww-texel-picture
description: Make the picture that proves a TEXEL-LEVEL claim about a generated sheet — an impostor card sheet, an atlas, a texture array layer, a VT tile — where the thing under test is a handful of texels on a border, a margin or an edge, and a rendered view would not show it. Covers choosing the crop where the defect actually is, the four drawing rules that make texels legible, the fixed-cell layout that stops captions clipping, and the caption arithmetic that keeps the picture's number the same number the report quotes. Use whenever the deliverable or the gate is "show me the sheet", and never for geometry — a rendered picture of a mesh or terrain is `nifskope-ww-render-shot`.
---

# WW: the texel-level picture

CONSTITUTION 5 requires a picture for a defect that was diagnosed in a picture,
from the same framing, before and after. For a MESH or a scene that is
`nifskope-ww-render-shot`. For a generated SHEET the subject is texels — a
one-texel margin, a border tap, a dilation ring — and a render will not show it.
This is that picture.

Three lanes have now built one of these from scratch (CARDFIT3, CARDPAD,
CARDFINAL). Everything below is what each of them re-derived.

## 1. Choose the crop where the DEFECT is, over EVERY orientation

The single most expensive mistake. Lane CARDFINAL's first pass picked the frame
pair whose silhouettes came closest **in the fixed library**, searching COLUMN
borders only, and photographed a border carrying 10/255 of a sheet whose worst
border carries 112/255. The picture understated the very thing it existed to
show, and it looked fine.

- Pick by the METRIC, on the BEFORE artefact, at the level the report quotes:
  the pair whose shared border carries the most of whatever is being removed.
- Search both orientations. A sheet's worst border is as often a row border as a
  column one. If the picture can only draw one orientation, that is a defect in
  the picture code, not a reason to search one.
- Fall back to "the tightest pair" only when the metric is zero everywhere, and
  say so in the caption.
- Use the SAME crop, orientation and indices in the before and after panels.
  Different crops are not a before-and-after.

## 2. Four drawing rules, or the texels are not readable

1. **Checkerboard under transparency.** A transparent texel on white is
   indistinguishable from a white texel. Two greys, 8 device pixels a square.
2. **Nearest-neighbour magnification.** Any smooth resample invents texels. Pick
   an integer factor from a target panel size, and clamp it on BOTH axes
   (`min(W_target // w, H_target // h)`) or a tall crop runs off the page.
3. **The texel grid only where a texel is visible** — draw it when the
   magnification is 4 or more, never at 1:1, where it would cover the image.
4. **Show a narrow frame WHOLE.** Lane CARDPAD cropped a 16x64 frame to a ten-row
   band around the border and it read as stripes, not as two trees. Two whole
   frames, always.

Mark the subject: the shared border in one colour, every texel that violates the
claim in another, one rectangle per texel. A reader must be able to COUNT the
marked texels.

## 3. A fixed cell per panel

Panels of one picture differ wildly in size — mip 0 of a 128-texel frame is
drawn 1:1 while the deepest mip is magnified thirteen times. A panel canvas sized
to its own content clips its own caption, silently, on the wide panels only.

Make every panel the same cell (`max(CELL, content)`), paste the content centred,
and draw the caption at the cell's top-left. Then the page is
`2*cellW x 2*cellH` plus a header, and nothing clips.

## 4. The caption carries the report's number, in the report's units

If the report says "a border tap picks up 56/255" and the picture says "alpha
112", the reader has two numbers for one fact and will trust neither. Decide the
quantity once — for a bilinear tap on a frame border it is HALF the neighbour's
alpha — and print that, in every panel, with the panel's own frame size beside
it. Colour it: red when the claim is violated, green when it is not.

Say in the header what the colours mean, which pair is shown, and which library
each row is.

## 5. Open the picture before reporting it

Read the PNG back and look at it. Every one of the three lanes found a layout
defect this way — a clipped caption, a band that read as stripes, a crop with the
defect outside it — and none of them was visible from the script's own output.
The script printing the right number is not evidence that the picture shows it.

## 6. Two libraries, three columns

When the change under test is a LAW and not only content, the picture and the
table both need three readings, not two: the old artefact under the old law, the
old artefact under the NEW law, and the new artefact under the new law. Anything
less reports a change of law and a change of content as one number. Nothing in a
generated sidecar usually distinguishes which law a file was written under, so
the law is an ARGUMENT to the measuring script, and the column heading says which.

## Worked example

`scratchpad/cardfinal_20260909/make_pictures.py` — four panels (before/after x
mip 0/deepest shipped) of two impostor card sheets, picking the worst border over
both orientations, with `measure_perframe.py` beside it as the three-column
table.

## 7. A crop's own floor is not the sheet's, and the verdict number is the sheet's (lane TILING2, 2026-09-11)

Rule 1 says choose the crop where the defect is, and rule 4 says the caption
carries the report's number. Those two pull against each other whenever the
statistic's noise depends on how much evidence it was given.

TILING2's repeat amplitude is read at one frequency bin. A 512-texel sheet holds
48 repeats; the 128-texel crop chosen to SHOW the repeat holds 12. The crop's
null floor is therefore several times the sheet's, and the same panel that makes
the defect visible reads "0.456 against a floor of 2.416" — which looks like a
clean pass for the artefact the picture exists to condemn.

**Print both, and say which one is the verdict.** The crop's number belongs in
the panel because it describes the texels the reader is looking at; the sheet's
number belongs beside it because it is the one the report gates on. One line in
the header says why they differ: fewer periods of evidence, higher floor. Never
quietly substitute one for the other.

The same applies to any statistic whose floor scales with the sample — a
correlation, an AUC, a per-band variance share. If the crop's own reading cannot
be made honest, the panel shows the picture and the caption shows the sheet's
number, labelled "whole sheet".

## 8. The ARM page: one panel per input, and the floor is ON the page (lane TERRAINFMT1, 2026-09-12)

Sections 1-7 are about a generated SHEET. The same page shape carries a render
comparison, and it needs two things a texel page does not.

**Every arm on the page, the reference and the BEFORE included.** A page showing
"vanilla" beside "after" is an advertisement. The page that says something is the
one carrying the reference, the before, each candidate, and the refuting arm
(the mix arm of `ww-render-arm-isolate`), each with its own number in its own
label:

    reference 0.00 | BEFORE 137.77 | --sheet-format vanilla 131.95 (0 px differ from BEFORE) | AFTER 0.48

That page states the finding without a sentence: the switch the brief was about
moved nothing, and the arm that moved it was a different one.

**The PAGE caption wraps; the PANEL caption does not.** Section 3 sizes the
panel cell so a panel label cannot clip. A page-level caption is a different
failure: it is one long line naming the camera, the mask and the exclusions, and
it runs off the right edge of a canvas sized from the tiles. Wrap it to the page
width, measure the wrapped height, and GROW the canvas by it — do not shrink the
text. Then open the PNG and read the last line of the caption (section 5); that
is the line that was missing.

**Name the camera in that caption, with the number the app logged**, not the one
you asked for: `ortho half-width 8600, look-at 8192,8192,8938, frame 1358x1024,
upp 12.665685 (release/ww_camera_pin.log)`. A reader who cannot reproduce the
framing cannot check the panels against each other.
