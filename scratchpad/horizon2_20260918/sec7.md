
## 7. The pictures, and their captions

All in `scratchpad/horizon2_20260918/images/`. Every render is a headless
`--port 12091` run with `WW_WINDOW_AT=1960,40`, one at a time, every path
absolute, the scene passed positionally, and `tasklist | grep -i -E
"Fallout4|NifSkope"` checked as its own command before the batch and again
inside the script before every single shot (`shots.sh`, this lane's copy of
HORIZON1's with three lines changed: `LANE`, `PORT`, and the object/sheet
sources). 39 renders, all `exit=0`, 01:02:25 -> 01:06:37 on 2026-09-19.

**The BEFORE panels are HORIZON1's own renders**, copied byte-for-byte into
`images/before/` with their logs. They are not re-renders of the old bytes by
the new exe: that would differ in two ways at once.

**Every caption number is read out of the render's own log** by `pairs.py`
(the viewer's note line for the role-7 sheet), not typed in, so a caption cannot
drift from its picture. The numbers in these captions are the numbers in
sections 4.5 and 4.6.

### 7.1 The twelve framings, BEFORE beside AFTER

`before_after_{close,full}_e{05,15,30}_a{120,240}.png`, 2824x1204 each, two
panels on one page, same camera in both. The caption under each panel is its own
log's line; the page's numbers:

| page | BEFORE mean / lit | AFTER mean / lit |
|---|---|---|
| `_close_e05_a120` / `_full_e05_a120` | 63.85 deg / 0.0% | 56.44 deg / 0.0% |
| `_close_e15_a120` / `_full_e15_a120` | 63.85 deg / **0.9%** | 56.44 deg / **5.3%** |
| `_close_e30_a120` / `_full_e30_a120` | 63.85 deg / 11.8% | 56.44 deg / **20.5%** |
| `_close_e05_a240` / `_full_e05_a240` | 64.37 deg / 0.0% | 56.95 deg / 0.0% |
| `_close_e15_a240` / `_full_e15_a240` | 64.37 deg / **0.0%** | 56.95 deg / **2.7%** |
| `_close_e30_a240` / `_full_e30_a240` | 64.37 deg / 7.9% | 56.95 deg / **18.5%** |

The caption on the left panel of every page:
> BEFORE -- exe 21:59:46, the sector-max footprint. terrain horizon
> 11.41..87.18 deg, mean 63.85, 0.9% lit. The stored byte is the MAXIMUM over
> the bin's whole 22.5 deg sector.

and on the right:
> AFTER -- exe 23:47:33, one square a tap. terrain horizon 4.32..86.82 deg,
> mean 56.44, 5.3% lit. Each stored byte is the skyline in its own direction,
> which is what the viewer blends.

What a reader sees, and it is the only claim these pictures make: in the BEFORE
panel the ground is one flat black field between the buildings; in the AFTER
panel the same ground carries lit patches along the streets and the open lots,
with the roofs unchanged. The roofs are the object stream and the ground is the
terrain sheet, so a change that touched only the terrain has to look like that
-- and it does.

The elevation-5 pages are in the set on purpose although neither side is lit:
**a picture that does not move is evidence too.** The third witness says only
2.8-3.8% of this chunk can be lit at 5 degrees, so a fix that lit it up there
would be the thing to distrust.

### 7.2 The control, beside the real picture

`control_beside_real.png`: the AFTER bake at sun 120,15, and beside it the SAME
bytes read four bins (90 degrees) away from the sun (`WW_HORIZON_BIN_ROT=4`).

> REAL -- the sun's own azimuth (120 deg): terrain horizon 4.32..86.82 deg,
> mean 56.44, 5.3% lit.
> CONTROL -- the same bytes a quarter turn away: terrain horizon 8.56..86.82
> deg, mean 56.17, 2.9% lit. If this were hard to tell from its neighbour, the
> azimuth is not being read and every picture here is decoration.

It is easy to tell: 5.3% lit against 2.9%, and the lit patches are in different
places. The renderer announces the control in its own note line
(`WW_HORIZON_BIN_ROT=4: THE CONTROL IS ON`), which the gate checks separately as
G3b.

### 7.3 The raycast overlay, before and after

`before_after_raycast_{close,full}_e{05,15,30}_a{120,240}.png` -- twelve pages,
each a BEFORE panel beside an AFTER one, every dot a sampled receiver where the
stored bins and the in-bake reference answered THAT sun differently. RED = a
terrain texel, ORANGE = a LOD vertex. The AFTER dumps come from a bake of the
same recipe with `WW_HORIZON_REFUTE_DUMP` set (`bake rc=0 34s`, 4,164 terrain
rows + 2,452 object rows, 01:01:51).

**The camera is measured, not assumed.** `compose.py` translates the camera by
three known world vectors, reads the image translation off by phase correlation,
and then predicts a FOURTH translation it never saw: close framing predicted
(-133.00, -86.80) against a measured (-133.00, -86.00), **0.80 px apart**; full
framing predicted (-168.70, -109.20) against (-169.00, -109.00), **0.36 px
apart**. The script stops itself above 1.5 px, because a red dot on the wrong
pixel is a claim about WHERE the two instruments disagree, invented.

Marks on the AFTER pages: close framing 3 / 33 / 48 (az 120, el 5/15/30) and
1 / 1 / 21 (az 240); full framing 19 / 121 / 217 and 5 / 54 / 221.

**The caption says what this page is worth, and it is less than it looks.** The
in-bake reference calls the same `maxAlong` the march calls, so it moved with
the fix; these pages show where two instruments that share a function disagree,
not where the sheet is wrong. The page that answers "is the sheet right" is
7.1's lit ground and the G6 numbers in 4.3.

### 7.4 The sixteen bins

`horizon_bins_close.png`, 2830x2246: one render a bin, laid out as a compass
rose -- bin 0 is NORTH (+Y) and the numbering runs CLOCKWISE toward EAST, each
tile captioned with its own azimuth. It is the picture that makes the
directional claim visible: the lit ground walks around the chunk as the bin
index advances, which is what a per-direction skyline has to do and what a
sector maximum blurs away.
