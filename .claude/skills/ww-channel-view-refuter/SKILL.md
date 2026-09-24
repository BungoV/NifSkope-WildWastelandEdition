---
name: ww-channel-view-refuter
description: Prove a per-channel debug view is WIRED before any picture of it is captioned -- the two-render pixel floor, the note line that reads back what was uploaded, the independent reader whose mean the note must equal, and the inverted floor for a channel the artefact genuinely does not carry. Use whenever a lane adds or changes a "show me channel X" switch (WW_LOD_CHANNEL, WW_LODL_CHANNEL, a plane selector, a G-buffer view) and a picture of X is the deliverable; never for a geometry before/after, which is nifskope-ww-render-shot.
---

# WW: refute a channel view before you caption it

A channel view is the easiest thing in the tree to fake, and the fake is
invisible. Paint the mesh grey, caption it "sky visibility", and the render is
beautiful whether or not the uniform was ever bound. Root `MISTAKES.md` 05:1x is
that defect, caught once; 05:0x is its sibling, a picture of a PROXY of the
channel rather than the bytes the shipped file carries.

Four assertions, all of them BEFORE the first picture is named.

## 1. Two renders that must differ

Render the channel and render the DEFAULT of the same framing -- same exe, same
fixture, same size, same camera, same everything but the switch -- and count the
differing pixels. Zero means NOT WIRED. Fix it; never caption it.

* Take the default ONCE per framing and per render mode, and compare each
  channel against the default of ITS mode. A textured channel (a normal sheet, an
  emissive sheet) compared against a FLAT default differs in a million pixels for
  the wrong reason, and the count then proves nothing.
* Do it at BOTH framings the deliverable uses. The ratios invert -- a terrain
  channel that repaints 380k pixels close up repaints 1.01M wide -- and that
  inversion is itself a check that the two framings are not the same picture.
* Put the floor on the page: assert that the flat default and the textured
  default DIFFER, so a later "0 px" reads as "identical", not as "the comparison
  is broken".

## 2. The note line reads back the UPLOAD, not the intent

Every channel prints `name: what, from which FILE, N read; min, max, mean`, and
every number comes off the accumulator sitting on the write into the vertex
colour or on the decoded texel -- never off the value the code meant to write. A
log echo of intent passes on broken code.

## 3. An independent reader supplies the right-hand side

A second decoder of the same format, sharing no code with the viewer, reads the
same population and its mean must equal the note line's within one unit. And
(this is the other lane's lesson, `MISTAKES.md` 2026-09-18 08:0x) **the second
reader is not believed until one of its numbers reproduces a number the shipped
code already prints** -- a population count is the cheapest such number.

**Name the population, both of them, when they differ.** A texture channel
reaching the screen through a resample has TWO honest means: the resample at the
vertices the viewer uploads (what the picture is made of) and the content-texel
census over the decoded tiles (the file's own number). They are different
populations and they differ by a unit or two. Print both, say which one the
report quotes, and never let the resample stand in for the census -- that is how
a number misses by 1.5 and still looks right.

Give the tolerance its own floor: assert the SAME tolerance still REFUSES a
deliberately wrong pairing (channel A's note mean against channel B's reader
mean), or it is proving nothing.

## 4. A channel the artefact does not carry has the INVERTED floor

Some name in the list will have no bytes behind it on this artefact -- a BC1
sheet has no alpha, a container carries no emissive role. Do not drop it and do
not fake it. Its refuter is the opposite of the others':

* its render MUST be byte-identical to the default, and
* its note line MUST say ABSENT **by name and with the reason**
  (`mask-a: ABSENT on this bake -- tile 0,3 is BC1 (dxgi 71): it carries no alpha`).

Declare the absent ones in the report BEFORE the pictures, and picture them
anyway with the caption saying ABSENT. Then no picture can be captioned as a
channel it is not.

An unknown name gets the same treatment from the other side: refuse BY THE NAME
GIVEN, list the known names, and render byte-identically to the default -- a typo
otherwise photographs as a perfectly good picture of nothing.

## 5. The way back, byte for byte

If the new switch subsumes an old one bungo already reads (`WW_LODL_AO=1`
becoming the `ao` channel), the old spelling on the NEW exe and the old spelling
on the RUNG exe must both be byte-identical to the new one, at every framing.
Three sha1s, one line. Anything less is "I did not mean to change it".

## 6. What this is not

It is not a look check. Nothing here says the channel is CORRECT -- only that it
is the channel, from the file, with the numbers the file carries. Whether the
bake itself is right is a different lane and a different refuter.

## Worked example

`scratchpad/chanview1_20260918/refute.sh` + `refute.py` (the lane's own pass, two
framings), promoted to `tests/spells/lodl_channels.sh` +
`lodl_channels_check.py` + `lodl_channels_table.py` (48 checks), with the red
demonstration of every floor recorded in the lane report's section 5.
