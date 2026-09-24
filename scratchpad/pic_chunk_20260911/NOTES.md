# PIC-CHUNK -- our far-terrain chunk beside Bethesda's, 2026-09-11

Read-only lane. Nothing built, nothing launched, nothing committed. Everything
written by this lane is under `scratchpad/pic_chunk_20260911/`.

bungo's ask, as relayed: *"I only want a comparison image with a vanilla chunk
side by side"*, *"preferably, show me a chunk with roads"*. So: two panels, no
channel grid.

---

## 1. Which bake, and why that one

| bake dir | written | chunk | has colour/msn sheets | mask layout |
|---|---|---|---|---|
| **`scratchpad/roads1_20260911/out/after`** | **2026-09-11 12:19:52** | **(-20,20)** | **yes** | TERRAIN-R's RMAOS |
| `scratchpad/roads1_20260911/out/{before,rung}` | 12:19:52 | (-20,20) | pyramid only, no `tex/` | " |
| `scratchpad/roads1_20260911/out/noroad` | 12:20:35 | (-20,24) | yes | " |
| `scratchpad/terrain_r_20260911/out/*` | 11:02..11:31 | (-20,24) | some | " |
| `scratchpad/splat1_20260911` | -- | -- | no bake at all | -- |

`out/after` is the newest bake on disk that carries chunk sheets, and it is the
only one whose chunk is **(-20,20)** -- the Sanctuary loop road, which is the
chunk bungo asked for. It was produced with `--roads`, the shipped default.
Its own log (`out/after.log`, last line) reads
`roads 1 roadPlacements 253 roadMeshes 73 roadTriangles 88513 roadTexels 27695
roadDecalTexels 915 roadAlphaRejected 577 roadRefusedNoLoad 0
roadRefusedNoTexture 7`, so the road pass ran and wrote 27,695 texels.

Provenance of the exe that made it: `release/NifSkope.exe` **2026-09-11
12:19:06, 21,180,928 B** (ROADS1's own handoff block, `HANDOFF_BLOCK.md` lines
1-3). The exe now on disk is 16:20:xx, 21,419,520 B -- a later lane's build,
NOT the one that wrote this bake; the picture's title line names the bake's exe,
not today's.

Vanilla side: `E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\
Commonwealth\Commonwealth.4.-20.20.DDS` and `..._msn.DDS`, Bethesda's shipped
files, 2025-10-08, untouched.

DDS decoding on both sides is the lanes' own reader, `Dds` in
`tests/spells/lodgen_terrain_model.py`, mip 0 only. Script:
`scratchpad/pic_chunk_20260911/make_picture.py`; numbers dumped to
`facts.json`.

---

## 2. The pictures

### 2.1 `images/chunk_vs_vanilla.png` (1102 x 739) -- THE DELIVERABLE

**Described before any claim.** Two panels on one row, each the full 512 x 512
texels of chunk (-20,20) drawn 1:1, no resampling on either side. LEFT, labelled
OURS: a brown-grey ground with a pale grey road entering the left edge about
two-fifths down, running east, bending south-west into a long hook at the
bottom-left, with a circular cul-de-sac and a planted island in the upper-left
quadrant and short driveway stubs off both. RIGHT, labelled VANILLA: the same
ground in a lighter, warmer brown, with a road of the same shape -- same entry
on the left edge, same bend, same cul-de-sac circle and island, same stubs --
drawn in a darker blue-grey, and the ground carrying large soft blotches of
light and dark where ours carries fine speckle. Under each panel: the file name,
its format, its byte count, its size in texels, its mip count, and its mean RGB.
A red line across the bottom carries the TILE=2048 caption verbatim.

### 2.2 `images/chunk_vs_vanilla_msn.png` (1102 x 758) -- the optional second

**Described before any claim.** The same two panels for the model-space normal
sheet (`_msn`), R east, G up, B north, drawn as raw RGB. Both are dominated by
green (flat-ish ground, normal pointing up), with cyan and red-orange streaks
where slopes face north and east. The ridge and gully layout is the same in both
panels and lands in the same places. Vanilla's panel is dense with fine
herringbone erosion detail all over; ours is smooth, carrying only the broad
ridges and a handful of the biggest gullies.

---

## 3. The numbers

Measured on mip 0, all four sheets, by `make_picture.py` and the check pass
beside it. "Neighbour difference" = mean absolute difference between adjacent
texels of the luminance, averaged over both axes -- a texel-scale roughness.

| sheet | format | bytes | texels | mips | mean RGB | neighbour diff |
|---|---|---|---|---|---|---|
| OURS colour `Commonwealth.4.-20.20.DDS` | **BC1** | **174,888** | 512x512 | 8 | 80.6, 69.8, 60.3 | 6.91 |
| VANILLA colour, same name | **BC3** | **349,680** | 512x512 | 10 | 91.6, 83.4, 72.1 | 5.03 |
| OURS normal `..._msn.DDS` | **BC1** | **174,888** | 512x512 | 8 | 131.1, 252.4, 126.9 | 0.93 |
| VANILLA normal, same name | **BC3** | **349,680** | 512x512 | 10 | 127.3, 247.0, 121.8 | 10.15 |

(For completeness, not in the picture: our mask sheet
`Commonwealth.4.-20.20_data.DDS` is **BC3, 349,648 bytes**, 512x512, 8 mips --
TERRAIN-R's RMAOS layout, R roughness / G metallic / B AO / A ground cover.
Vanilla ships no counterpart for it.)

Difference against vanilla, mean of |ours - vanilla| per texel per channel:

| pair | mean | R | G | B |
|---|---|---|---|---|
| colour | **19.5** of 255 | 19.4 | 20.4 | 18.8 |
| normal | 15.1 of 255 | 24.4 | 6.1 | 14.9 |

**Registration, measured, not eyeballed.** The claim "the same road in the same
place" is not left to the eye. Two checks:

1. Mean absolute difference over the seven rigid re-orientations of our sheet:
   identity 19.51, flip-V 21.76, flip-H 22.52, rot180 23.01, transpose 22.82,
   rot90 21.98, rot270 22.47. Identity wins, so the sheet is not flipped or
   rotated relative to vanilla's.
2. Cross-correlation of the low-saturation (asphalt-coloured) mask of each sheet
   over shifts of +/-12 texels: peak at **dy 0, dx -1**, and the zero-shift
   value is 0.999 of the peak. The road networks register to within one texel.

---

## 4. What the pictures show, after the description

* **The road is there and it is in the right place.** Same entry point, same
  bend, same cul-de-sac, same island, registered to within one texel. That is
  ROADS1's result, standing in a full-resolution picture rather than a
  thumbnail.
* **Our road is lighter and greyer; vanilla's is darker and bluer.** Ours reads
  as pale concrete, vanilla's as wet asphalt. ROADS1 measured the same thing as
  a grading factor (0.82-0.83 on both road and ground) and attributed it to the
  splat-grading gap, not to the road pass.
* **The whole sheet is about 11 of 255 darker than vanilla's on every channel**
  (80.6/69.8/60.3 against 91.6/83.4/72.1). The gap is nearly the same size on R,
  G and B, so it is a brightness offset more than a hue error.
* **Our ground speckles where vanilla's blotches.** Neighbour difference 6.91
  against vanilla's 5.03 -- ours is 1.37x rougher texel to texel. This is the
  visible face of the tiling bug named in the red caption: the ground textures
  are laid down at TILE=2048 and repeat about six times too often inside the
  chunk, so the material's own grain survives into a sheet where vanilla's has
  averaged out into broad patches.
* **Our normal sheet is far too smooth.** Neighbour difference 0.93 against
  vanilla's 10.15 -- vanilla's `_msn` carries about **eleven times** our
  texel-scale detail. The big shapes agree; the erosion detail is simply not in
  ours. This is a separate open gap from the colour one, and it is not something
  the TILE=2048 fix will touch.
* Our sheets are **half the bytes** of vanilla's: BC1 against BC3, 174,888
  against 349,680. Vanilla spends an alpha block per texel on sheets whose alpha
  is not used for coverage here; we do not. Vanilla also ships mips down to 1x1
  (10 levels) where ours stop at 4x4 (8).

## 5. What is NOT shown, and what is not claimed

* No mask/height/cover panels -- bungo dropped the channel grid. The mask
  sheet's numbers are in section 3 for the record only; nobody has looked at its
  channels in this lane.
* The pyramid (`out/after/mod/Terrain/Commonwealth.VT.{2,4}.lodt`, 754,304 and
  189,056 bytes) exists in this bake and was NOT opened. No height panel.
* No claim that this bake is correct, fixed, or final. The colour sheet carries
  a known bug that is still open (RESUME3), and the normal-sheet detail gap
  measured above is stated as a gap, not as a diagnosis -- nothing here
  identifies its cause.
* Nothing in this lane was built, launched or committed.

## 6. Layout defects the picture had, found by opening it

Skill `ww-texel-picture` section 5 (open the picture before reporting it) paid
for itself twice on this one page:

1. The first render's subtitle ran off the right edge of the page and was cut
   mid-sentence. Fixed by splitting it into three lines, and an assertion now
   refuses any subtitle line wider than the page -- a floor, not a hope.
2. The OURS / VANILLA panel tags were drawn over the subtitle's last line.
   Fixed by giving the tags their own row below the subtitle block.
3. That new assertion then **fired on the second page**, whose longer title
   overflowed. It was a real defect caught by the floor, and the title was
   shortened. The floor is shown failing, per CONSTITUTION 4.

## 7. Finished-work skill review

* Loaded: `ww-texel-picture` (crop choice, fixed cells, captions carrying the
  report's own numbers, and above all "open the picture before reporting it"),
  `nifskope-ww-vanilla-compare` (same grid both sides, the camera/grid pin, and
  the two things that put a difference in the picture that is not the one under
  test -- here: an orientation flip and a resample, both ruled out by
  measurement rather than assumed).
* Wished had existed: a small **"sheet pair page"** procedure -- decode two DDS
  files with the lanes' own `Dds`, lay them out 1:1 side by side with provenance
  and per-panel statistics, and assert nothing clips. ROADS1 wrote this from
  scratch as `make_pictures.py`, TERRAIN-R wrote its own, and this lane wrote a
  third. Three times is past the "skill before the second use" line. It is a
  narrow, mechanical extension of `ww-texel-picture` (which covers texel crops,
  not whole-sheet pairs) and belongs as a section in it rather than as a new
  skill, so that the caption/assert rules are not duplicated. Not written here
  because this lane is read-only outside its own directory and may not edit
  `.claude/skills/`; flagged for the director.
