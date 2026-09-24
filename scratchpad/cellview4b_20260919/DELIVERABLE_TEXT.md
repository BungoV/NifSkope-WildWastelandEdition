# Lane CELLVIEW4B -- apply, build and gate CELLVIEW4's never-compiled code

(The brief's `report.md` name is refused by this session's tool harness, same
as it was for CELLVIEW4; the brief's stated fallback name is used instead.)

## 0. Status and the exe at launch

Started 2026-09-19 20:39 (`date`). Process check before start: `tasklist |
grep -i -E "Fallout4|NifSkope"` -> rc=1, **no match**: the game is down and no
NifSkope (not even bungo's) is running. The build slot and the exe slot are
free and this lane owns them.

Exe at launch, as the brief states it:

| | |
|---|---|
| path | `release/NifSkope.exe` |
| size | 23,625,728 B |
| mtime | 2026-09-19 19:49:03 |
| sha1 | c529e3c12fe4216a3c33cc14631e3c811169fbb0 |

Read before starting: `CONSTITUTION.md`, the HANDOFF top block,
`scratchpad/cellview4_20260919/PENDING.md`,
`scratchpad/cellview4_20260919/DELIVERABLE_TEXT.md`.

CELLVIEW4 is CODE-ONLY: `src/cellsplat.cpp` and `src/cellsplat.h` have never
been through a compiler, the 13-edit hook-up has never been applied, and the
gate `tests/spells/cell_splat_compare.py` has never been run. This lane's job
is to turn that into a built, gated, pictured result.

Sections follow as they are finished.


## 1. The exe

| | rung at launch | shipping |
|---|---|---|
| path | `release/NifSkope.before_cellview4.exe` | `release/NifSkope.exe` |
| size | 23,625,728 B | 23,684,608 B |
| mtime | 2026-09-19 19:49:03 | 2026-09-19 20:58:28 |
| sha1 | `c529e3c12fe4216a3c33cc14631e3c811169fbb0` | `595f4b9fdef7091fe1b8ec0a9760c92be2e07943` |

The rung was never launched. Every "before" number and picture in this report
comes from CELLVIEW3's own `after_*` run on that exe, or from a control built
inside this lane out of two of MY builds -- never from a GUI run of a `before_*`
exe. bungo's open window, if any, predates all of this: the next launch of
`release\NifSkope.exe` (20:58) is the first one with any of it.

`hookup.py --check` said 13 of 13 anchors matched exactly once; `--apply` wrote
them. **The code compiled with no errors.** I did not preserve the build log, so
I cannot quote the warning list, and I will not claim it was empty.


## 2. What I changed in the design, and why

CELLVIEW4's code had never been through a compiler, and the brief allows repairs
freely provided a change to the DESIGN is reported rather than made silently.
Nothing here was forced by a compiler error; all four are judgement calls.

1. **`src/cellsplat.h` carried a false claim.** It said that "because neighbours
   share their corner values there is no seam between quads". That is true
   inside a quadrant and false across one. I replaced the paragraph with the
   measured numbers (section 4) rather than deleting it, because the next reader
   needs to know the sharing argument has a boundary.
2. **The bare-quad count was one number doing two jobs.** `quadsBare` mixed
   "nothing painted here" with "the LTEX named no texture". Split into
   `quadsBareUnpainted` and `quadsBareNoTexture`, worded in the legend exactly
   as the mosaic words it so one gate pattern reads both arms.
3. **The root is `NiNode` again, and the ground gets its own `BSOrderedNode`.**
   Section 5.
4. **`WW_CELL_SPLAT_CAP`** -- a harness hook, not a feature switch. Section 6.


## 3. Sanctuary -20,7: the simulation against the real viewer

**How the lighting is normalised, stated before any number.** The two targets
(`sim_m20_7_blended.png`, `sim_m20_7_mosaic.png`) are UNLIT: raw diffuse texels
composited at the opacities the LAND record stores, no light, no vertex colour,
no tone map. The shot has all three. I do not try to undo that physically. I
reduce all three pictures to 32x32 per-quad block MEANS, so a quad is one colour
and the question becomes *which texture won this quad* rather than a texel
comparison; then I fit **one gain and one offset per channel over the whole
cell** (least squares, shot -> target). A global affine per channel can absorb an
exposure and a black level. It cannot absorb a PER-QUAD difference -- and a
per-quad difference is the entire distinction between the two rules. Fitted
gains, shot -> blended: `0.040,+0.384  0.013,+0.351  -0.002,+0.309`. The gain
being tiny and the offset carrying almost everything says plainly how little of
the shot's absolute level survives: the residuals below are NOT a verdict on
colour accuracy. The comparative column is the verdict.

| | vs BLENDED | vs MOSAIC |
|---|---|---|
| mean abs residual, all 1024 quads | 0.0246 | 0.0403 |
| mean abs residual, the 249 quads where the rules disagree | **0.0159** | **0.0591** |
| of those 249, nearer the blended target | **232 (93.2%)** | 17 |

| quadrant | vs BLEND | vs MOSAIC | disagreeing | nearer blend |
|---|---|---|---|---|
| TL (2) | 0.0187 | 0.0308 | 84 | 95.2% |
| TR (3) -- the no-BTXT one | **0.0354** | 0.0664 | 26 | 84.6% |
| BL (0) | 0.0221 | 0.0323 | 74 | 93.2% |
| BR (1) | 0.0222 | 0.0317 | 65 | 93.8% |

The pre-registered gate `tests/spells/cell_splat_compare.py`, which CELLVIEW4
wrote and fixed its line at 90% with both ends measured before this lane
existed, scores the same shot **225 of 249 = 90.4% PASS**, against its own
failing control (the mosaic shot) at 75.1%. My script says 93.2% on the same
data because it blocks and fits slightly differently. **Quote the gate's 90.4%,
not my 93.2%** -- the gate is the number that was registered in advance.

**The margin is thin and one confound works against it**: the shot contains
OBJECTS and the simulation is terrain-only, and there is no objects-off switch
for a terrain shot. Every object pixel is noise pushing both columns up. That
makes 90.4% a floor rather than an estimate, but it also means a future change
of 2-3 points cannot be read as signal.


## 4. The seam the director saw: it is in the data

The verdict is **the game's own data, not the viewer and not the simulation.**
No viewer repair is warranted and none was made. `seam_probe.py` measures it
from the record bytes, four ways:

* **Quadrant 3 (TR) of -20,7 carries no BTXT at all**, and a layer set the other
  three do not have (it has `DirtPath01`; no neighbour does).
* Where two quadrants share a grid line and share an LTEX, their stored
  opacities for it agree across **BL|BR to a maximum of 0.0353** -- and across
  **BR|TR they disagree by up to 0.7490**.
* Of the 64 quad pairs straddling the two centre lines, **30 change their
  dominant texture. All 30** are pairs with the no-BTXT quadrant on one side.
* Of the 32 pairs where **both** sides have a BTXT, **zero** change.

So the viewer's quadrant arithmetic is clean: it is only ever discontinuous
where the DATA is. The arithmetic cross-check agrees -- quadrant 3 is 256 quads
and the census reports 250 promoted + 6 bare = 256 exactly, so every single
promotion is inside the no-BTXT quadrant and none leaks out of it.

The residual table in section 3 is the independent confirmation: TR is the worst
quadrant (0.0354 against 0.0187 for TL) in BOTH the blended and the mosaic
column, which is what a genuinely odd quadrant looks like, and the quads on the
two centre lines carry a residual only **1.14x** the cell mean -- a seam the
viewer had invented would put a spike there, not a 14% lift.

**What the first-layer promotion is.** A quadrant with no BTXT has no opaque
floor, so the first layer with any weight is drawn opaque underneath the rest.
That is the only way to avoid drawing the void, and it is what makes TR look
different from its neighbours. It is honest, it is visible, and it is the data's
fault -- but it is worth saying that the engine may pick a different floor, and
nothing in this lane measured what the engine does.


## 5. The ordered node: the refuter did NOT fire

CELLVIEW4 made the scene root a `BSOrderedNode`. Reading
`NodeList::orderedNodeSort()` (`src/gl/glnode.cpp:166`): it sets `presorted=true`
on every child, `Node::drawShapes` recurses, and `compareNodesAlpha` returns
block order **only when both nodes are presorted**, otherwise alpha-then-depth.
A `BSOrderedNode` ROOT therefore marks the **entire scene** presorted, which
re-sorts every transparent shape in the cell by block order -- a global change
made to fix the ground.

CELLVIEW4 wrote its own refuter for this and it deserved to be run. I built the
A/B out of my own two builds so the only difference is the node, and shot
downtown 5,-11 **objects only** (`WW_CELL_NOTERRAIN=1`), same camera:

    downtown_obj_rootordered.png    root BSOrderedNode
    downtown_obj_groundordered.png  root NiNode, ground under its own node
    both sha1 012aca3bd329db89b1f9e57bf0e908a371454dd5
    0 of 1,685,350 pixels differ

**Say this plainly: the refuter did not fire, and I fixed no visible bug.** On
this cell the two arrangements draw the same picture. I kept the narrow one
anyway -- the root is `NiNode` and the ground shapes hang off a lazily created
`BSOrderedNode` named `ground` -- because it makes the guarantee structural
instead of a per-cell coincidence, and because a global sort dependency is the
kind of thing that bites in a cell nobody shot. That is a reason, not evidence.
Anyone who wants the evidence needs a cell with overlapping transparent objects
and a different answer.


## 6. The vertex budget, and a refusal that is now provable

    Sanctuary -20,7   8,936 land vertices counted before allocating, cap 12,000,000  (0.074%)
    downtown  5,-11  10,080 land vertices counted before allocating, cap 12,000,000  (0.084%)
    2.18 and 2.46 passes per land quad respectively

The count is printed whether or not the cap trips -- CELLVIEW4 printed it only
on refusal, which makes the cheap case invisible. `cellSplatCountVerts()` still
runs before a single vertex is allocated.

Proving the refusal honestly at 12M means a rectangle of roughly 38x38 cells:
minutes of LAND and REFR reading for one boolean, which is a gate nobody runs,
and a refusal path nobody runs is a refusal path nobody knows works.
`WW_CELL_SPLAT_CAP` lowers the cap for one run -- same family as the existing
`WW_CELL_NOTERRAIN` / `NOWATER` / `NOGRID`, a harness hook that forces the state
the measurement needs. Unset, the cap is `CELL_MAX_TOTAL_VERTS` exactly as
before, and a gate row pins that. With `WW_CELL_SPLAT_CAP=1`:

    ground: the BLEND REFUSED -- 8936 vertices is past the 1 cap; the hard-edged
    mosaic was drawn instead.  <then the full mosaic legend>

Three rows now hold that: the refusal happens and **names itself**; the mosaic
legend is printed too, so the downgrade is not silent (CONSTITUTION 10); and a
RED control that the uncapped run of the same cell carries no refusal line.
I also had to carry a `QString splatNote` past the fallback, because the
refusal text was being overwritten by the mosaic's own legend on the way out --
the refusal existed and then erased itself.


## 7. Bare quads, and the black arrow

The forced-refusal run gave a free cross-check, because it renders the same cell
under both rules in one session:

    mosaic arm   24 bare quads, 6 landscape textures
    blend  arm    6 bare quads, 11 landscape textures

so the brief's predicted **24 -> 6 is confirmed on the shipping exe**, and the
6 that remain are now split by the census: 6 unpainted, 0 that chose an LTEX
naming no texture.

**The marker.** `markerxheading.nif` (REFR `00066245`) is gone from the downtown
picture -- pure-black pixels in the whole frame **497 -> 0**, markers hidden
**2 -> 12**. The cause is one clause: a marker at the meshes ROOT has no
backslash before its name, and every test in `isMarkerModel()` needed one, so
`markerxheading.nif`, `markercocheading.nif` and the whole `markers\...` subtree
were drawn as ordinary statics. Confirmed on exe `595f4b9f` by rendering, not by
reasoning.


## 8. Gates

Before-and-after, the two the change owns:

| gate | rows | failures | note |
|---|---|---|---|
| `cell_pick.sh` | 16 | 0 | 6 rows added by this lane |
| `cell_open.sh` | 8 | 0 | went RED on this lane's own change; see below |

Neighbours, run on the shipping exe:

| gate | result |
|---|---|
| `render_shot.sh` | 82 checks, 0 failures, PASS |
| `harness_window.sh` | 15 checks, 0 failures, 0 skips, PASS |
| `native_open.sh` | 17 checks, 0 failures, 2 skipped, PASS |
| `impostor_draw.sh` | 11 steps, **1 failure** -- `IMPOSTOR_LODM is not set to a baked <id>_oct.lodm`. Not mine and not a defect: the gate REFUSES rather than passing with no subject, which is the behaviour this project asks for. It needs a baked card to have a subject at all. |

**`cell_open.sh` went red on my own marker repair, and I made the row stronger
rather than making it agree.** `cell_open_check.py:is_marker_model()` says in its
own docstring that it mirrors the C++ "element for element"; I widened the C++
and not the copy, so the gate called three now-hidden markers "references
dropped although their model is present" (`XMarkerHeading`, `COCMarkerHeading`,
`FloatingPlatformHelperSwingOnly01`). Mirroring a rule by hand is the weak form:
two sources that LOOK alike, checked by eye. The run supplies a real accounting
identity instead --

    downtown 5,-11: plugin draws 1438, scene has 1426, gap 12
    census:  hidden: disabled 0, markers 12, deleted 0, no base 0

-- so the row is now *every reference the plugin draws and the scene does not
must be accounted for by a category the viewer NAMED in its own census*. It is a
closed sum: a reference dropped for an unnamed reason breaks it, and widening or
narrowing the marker rule merely moves a reference between two named categories
and leaves the sum alone, so the row cannot be satisfied by editing the rule.
The old excuse it replaces -- "9 more were dropped with no loose model, which is
expected" -- is open-ended and would hide a real drop.

I proved the new row can fail before believing it: fed a census with
`markers 12` changed to `markers 11`, it says

    FAIL: 12 references are missing from the scene but the census names only 11
    -- 1 dropped for a reason the viewer does not state

and it moves with the subject (0 on Sanctuary, 12 on downtown).

**One flaky row to report**: `cell_pick.sh`'s `downtown` render wrote a 7,042-byte
picture with one unique colour on the 20:42 build. I investigated rather than
assumed -- the notes for that same run showed 1505 refrs, 1575 drawn, ground
blended, i.e. the scene built correctly -- and re-shooting the identical camera
with CELLVIEW3's own script gave a real 2.88 MB picture. It is a harness flake,
not the build, and I do not know its cause.


## 9. The pictures, looked at

`scratchpad/cellview4b_20260919/images/`, `before_*` | `after_*`, same camera,
half-size copies beside each:

    before_ground.png   | after_ground.png     Sanctuary -20,7 top-down
    before_downtown.png | after_downtown.png   downtown 5,-11

Plain words on what I see.

**Sanctuary, the win.** The before is a checkerboard: every 128-unit quad snaps
to one texture and the grid is the first thing your eye finds. In the after that
grid is gone from the transitions -- grass into dirt into gravel now happens
across a quad instead of at its edge, and the big organic dirt patch left of
centre reads as a shape rather than as a staircase.

**Sanctuary, what is wrong with it.** The after is **flatter and hazier**. The
whole cell trends toward one mid-brown; the dark grey gravel block left of
centre, which is clearly a different surface in the before, is washed toward the
background in the after, and fine texture detail is softer everywhere. That is
what averaging several layers per quad does, and I am not going to call it
finished. A faint grid is **still visible** in the upper middle and left -- the
quad structure has been softened, not removed. The top-right dirt patch still
ends on a straight line: that is the no-BTXT quadrant of section 4, it is in the
data, and it will still look like that until something decides what the engine
puts under an unpainted quadrant.

**Downtown.** The black arrow at the right of the frame is gone and nothing else
moved -- which is exactly what the zero-pixel ordered-node control predicted.
The ground is mostly under objects here, so the blend barely shows; where it
does (right edge, bottom) it has the same slight flattening.


## 10. Text for `WW_CHANGES.md` (I did not edit the file)

> **Landscape is blended, not tiled (cell viewer).** The ground under a cell used
> to be a mosaic: one texture per 128-unit quad, hard edges on a visible grid.
> It is now drawn the way the game paints it -- the quadrant's base texture
> first, then every painted layer over it in the order the record stores, at the
> opacity the record stores. Sanctuary -20,7 goes from 6 textures and 24 bare
> quads to 11 textures and 6 bare quads. Against an independent simulation built
> from the record bytes and the shipped .dds files, the render agrees with the
> blended rule on 225 of the 249 quads where the two rules disagree (90.4%,
> against a pre-registered line of 90% and a failing control at 75.1%).
>
> The blend costs one pass per contributing layer, so it counts its vertices
> before allocating any (Sanctuary 8,936, downtown 10,080, against a 12,000,000
> cap) and past the cap it REFUSES IN WORDS and draws the hard-edged mosaic
> instead, printing both the refusal and the mosaic's own legend.
>
> Markers at the meshes root -- `markerxheading.nif` and its family -- are hidden
> like every other marker now, instead of being drawn as ordinary statics. The
> black arrow is out of the downtown picture.

## 11. Text for `HANDOFF.md` (I did not edit the file)

> **CELLVIEW4B, 2026-09-19 21:2x.** Applied CELLVIEW4's 13-edit hook-up (13 of 13
> anchors), compiled it for the first time, gated it and shot it.
> `release/NifSkope.exe` 23,684,608 B, 20:58:28, sha1 `595f4b9f...`. His open
> window predates it; the next launch has it.
> Blend gate 90.4% (line 90, control 75.1%). `cell_pick` 16/0, `cell_open` 8/0,
> `render_shot` 82/0, `harness_window` 15/0, `native_open` 17/0.
> `impostor_draw` 1 failure, pre-existing, refuses for want of a baked card.
> **Two things need his eye.** (a) The blended ground is smoother but FLATTER --
> contrast between surfaces is lower than the mosaic's and a faint grid remains;
> look at `after_ground.png` beside `before_ground.png` before anyone calls this
> done. (b) The straight seam he saw on the top-right quadrant is **in the
> data**, not the viewer: that quadrant has no base texture at all and a layer
> set no neighbour shares (opacities disagree by up to 0.749 across that line, by
> at most 0.035 across the others; 30 of 64 straddling quad pairs change texture
> and all 30 touch that quadrant, 0 of the 32 pairs where both sides have a base
> texture change). The viewer draws the first painted layer opaque underneath to
> avoid drawing the void. **Nobody has measured what the engine does there** --
> that is the open question.
> The `BSOrderedNode` root CELLVIEW4 introduced is reverted to `NiNode` with the
> ground alone under its own ordered node; its refuter was run (downtown,
> objects only, two builds, same camera) and came back **0 of 1,685,350 pixels
> different**, so no visible bug was fixed by either arrangement.


## 12. MISTAKES

Four entries appended to the ROOT `MISTAKES.md`, newest-at-the-top, by byte
splice at the first `## ` heading. The file is pure CRLF and stayed so --
measured with Python byte counts, never grep:

    MISTAKES.md  bytes 654,688 -> 659,195
      CR 10,590 -> 10,668   LF 10,590 -> 10,668   (78 lines added, CR == LF)

The script (`scratchpad/cellview4b_20260919/mistakes_splice.py`) asserts the CR
rise equals the CR count of the inserted bytes and that CR still equals LF, so a
stray bare LF fails the write instead of landing.

The four: a census field the gate READ but nothing ever WROTE, inside a green
run; a rule mirrored in two languages with only one copy widened; the wrong
instrument (`grep Makefile.Release` for a header macro) and briefly believing
it; and a failed patch script repaired with `sed`, which put a syntax error into
the script while fixing a syntax problem.


## 13. Skills, both trees, equal sha1

| skill | sha1 | bytes |
|---|---|---|
| `ww-anchored-hookup/SKILL.md` (CELLVIEW4's owed addition) | `e9daf8d4f202c0bb589c51f766acae915b1977e3` | 11,872 |
| `ww-gate-closed-sum/SKILL.md` (earned by this lane) | `767788d5ae1913341171c2911f82f9f62fcc53ab` | 4,705 |

Written to `E:/Projects/NifskopeWildWastelandEdition/.claude/skills` and
`E:/Projects/Claude/.claude/skills` from the same bytes, both LF-only, sha1
compared after the write rather than assumed.

CELLVIEW4's text calls itself "section 5c"; `ww-anchored-hookup` has a 5a and no
5b, so it is filed as **5b** with a one-line note saying so. The text is
otherwise unchanged.

`ww-gate-closed-sum` is this lane's own: when a gate re-implements a rule the
product owns, replace the mirrored copy with a closed sum over the categories
the product NAMES in its census -- because the sum cannot be satisfied by
editing the rule, has no open-ended excuse, and fails with a number. It carries
the two proofs the row needs (perturb the census by one and watch it fail; run
two subjects and watch it move) and the case where the answer is that the
product owes a census line, not the gate a row.
