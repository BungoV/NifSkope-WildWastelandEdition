# IMPOSTORFIX1 -- state at 2026-09-19 14:4x. PHASE 2 COMPLETE.

(The 13:44 phase-2-start state is kept beside this as `PENDING_phase2_1344.md`.)

Nothing is committed. Two source files, one shader, three new test scripts, one
gate script edited. The build slot and the exe slot are released.

## The answer

The director was right and the previous lane's excuse was wrong. A single
un-blended frame viewed from its own bake direction scores **0.8823** (16
directions, range 0.8563-0.9107) once the instrument stops painting a grid into
the mesh grab. The draw is sound; it was the SHEET and the RAY.

Decomposition, blast_n4, same exe, same sheets, same mesh:

| views | blend + parallax | IoU |
|---|---|---|
| 16 bake directions | OFF | 0.8823 |
| 16 bake directions | ON | 0.6507 |
| 24 orbit views | OFF | 0.4401 |
| 24 orbit views | ON | 0.3546 (reproduces the shipped number exactly) |

## The two repairs

1. **`lodgenRepairOctHeight`, src/lodgen.cpp:2585**, called at 3022 (per-card
   DDS writer) and 13931 (atlas builder), AFTER the dilates. `lodgenDilateFrames`
   floods everything the silhouette does not cover with the FRAME'S AVERAGE;
   the parallax step reads that as a displacement (+264 world units mean, +743
   p95, against a card half-width of 135). The repair seeds from texels with
   encoded alpha >= 250, ring-dilates their height outward, writes it on
   partially covered texels, writes the card plane (128) outside the coverage
   floor, and self-checks that no covered texel decodes outside the frame's own
   [hMin,hMax]. Chosen from a five-row candidate table (in the code comment):
   it scores within 0.0015 of the best candidate and needs no projection
   change, no format change and no spec change. `depthSpan` was NOT touched --
   it is `GLView::glProjection`'s ortho clip range written down, not a free
   parameter.
2. **res/shaders/impostor_oct.vert** -- `cardOrtho` arrives false from every
   caller, so an ORTHOGRAPHIC camera got the perspective ray fan (up to 14 deg
   of lean at the frame edge) and the parallax stopped being the no-op the
   algebra says it is at a bake direction. Now decided from the projection
   matrix itself: `abs( projectionMatrix[2][3] ) < 1e-6`. The C++ uniform is
   untouched. Deployed by `cp` to release/shaders/ -- a RUNTIME ASSET, not
   linked into the exe, so the exe alone does not carry this fix.

Plus one instrument repair, src/impostorpreviewtest.cpp: `Scene::ShowGrid`
cleared beside `ShowAxes`. `Scene::drawGrid` returns early in ortho unless the
view is axis-aligned, so at azim 0/90/180/270 elev 0 and nowhere else a lattice
was painted into the MESH grab only, entering the union of every comparison.
The shipped 0.3546 was NOT contaminated (elev 15/45 is never axis-aligned).

## Numbers, 24 orbit views, blend + parallax on

| subject | shipped 88d6abb3 | old sheets + ortho ray | repaired sheets + ortho ray |
|---|---|---|---|
| blast_n4 | 0.3546 | 0.3651 | **0.5038** |
| blast_n8 | 0.4152 | 0.3853 | **0.6754** |
| maple_n4 | 0.3206 | 0.3550 | **0.3545** |
| dead_n4 | 0.3957 | 0.4176 | **0.5721** |
| rock_n4 | 0.7944 | 0.7891 | **0.7724** |

Card/mesh ink ratio: blast_n4 1.63->1.48, blast_n8 2.21->1.26, maple_n4
2.00->1.85, dead_n4 1.56->1.13, rock_n4 1.12->1.02.

Distance strip, azimuth 45 (the three-frame blend's worst case), IoU
before->after: 256px 0.190->0.287, 128px 0.198->0.308, 64px 0.230->0.315,
32px 0.217->0.290, 16px 0.180->0.345. Card ink at 256px 0.00525->0.00441
against the mesh's 0.00242. The mesh grabs of the two runs are pixel-identical,
so only the card moved.

## Gate

`tests/spells/impostor_draw.sh`: **21 steps, 0 failures** (3 SKIP for optional
fixtures). Floor raised 0.22 -> **0.35** on three measurements of row 5 written
into the comment: 88d6abb3 + shipped sheets 0.2778, this exe + shipped sheets
0.3047, this exe + repaired sheets 0.4469.

Two new rows, both watched RED against the build they convict
(`scratchpad/impostorfix1_20260919/redrows.sh`):

* **14, the sheet's own height** -- `tests/spells/impostor_sheet_check.py`, no
  application, no scene. Shipped blast_n4: 16 of 16 frames fail, worst
  excursion 1530 world units. Repaired: 0 of 16.
* **15, the photograph row** -- bake directions derived from the `.lodm`'s own
  grid by `tests/spells/impostor_bake_views.py`; parallax ON must EQUAL
  parallax OFF, and both must clear 0.80. Red control A, exe 88d6abb3: counted
  **24 of 24** instead of 16 (it predates `WW_IMPOSTOR_ORBIT_VIEWS` and silently
  orbited its own ring, scoring 0.4991 on a different set of views -- which is
  exactly why the count is asserted). Red control B, this exe with the SHIPPED
  shader: off 0.8701, on 0.7472, gap 0.123 against a tolerance of 0.01. Green:
  0.8701 / 0.8701.
* What row 15 does NOT catch, said in its comment: at a bake direction a
  correct ray makes the parallax a no-op, so a broken height sheet scores there
  exactly as well as a repaired one. That defect is row 14's.

Also fixed while running the gate: `$PY` was a NAME, re-resolved after the
script prepends `/c/msys64/ucrt64/bin` for g++, so every later `$PY` ran MSYS2's
python instead of the one row 0 announced. Row 14 found it by failing on a
missing numpy that the announced interpreter has. Now `command -v` absolute.

## Honest residuals -- I looked at the pictures

* `00_before_after.png` / `30_distance_blast_n4.png`: the old card is confetti,
  the new one is a trunk standing where the mesh's trunk stands. **Flakes
  remain**, concentrated above the trunk and around the base, and the trunk
  reads as several parallel strands where the mesh has one. Ink ratio still
  1.48.
* `12_orbit_maple_n4_el15.png`: the maple's crown is **still a blob on a stick**.
  Only 401 of 32,768 texels are fully covered (2,907 have any coverage), so
  "dilate from fully covered" has almost nothing to work from; relaxing the seed
  to 160 reaches 858, i.e. essentially every covered texel, which is the shipped
  behaviour. maple_n4 still fails 2 of 16 frames in row 14, and that is
  reported rather than tuned away. The remedy is a nearest/median depth filter
  in `frameOf`'s downsample instead of the box average -- a photography-side
  change in src/nifskope_ui.cpp needing a full re-bake. NOT DONE.
* **rock_n4 regressed 0.7944 -> 0.7724 (-2.8%).** For a nearly-all-whole-texel
  solid the old flood average sat close to the object's real depth and the
  card-plane clamp throws that away. Candidate R2 (dilate everywhere, no clamp)
  would likely recover it and costs the trees 0.03. Reported, not chased.

## RULING OWED (bungo) -- the `_n` sheet's channel assignment

Spec line 45: `| _n | BC3 | normal X | normal Y | height | sway weight |`.
BC3 gives ALPHA an 8-bit interpolated block and gives R, G, B a single
four-entry RGB565 palette per 4x4. So height sits in the coarse half and sway
in the precise one. **Swapping them costs ZERO bytes** -- same format, same
size, pure channel reassignment -- but it is a format-contract change, so the
in-spec repair shipped and this is the ruling. Measured BC3 height error on
whole texels AFTER the repair: blast_n4 mean 2.2 levels = 26 units, p95 5.1 =
62, max 25 = 297; maple_n4 mean 4.1 = 49, p95 11.6 = 140, max 57 = 691;
dead_n4 mean 2.9 = 35, p95 7.9 = 95, max 54 = 655. (blast_n4 before the repair:
mean 4.8 = 57, p95 18.8 = 227.) A swap would divide those by roughly four and
move the same error onto sway, which no parallax reads. A yes needs a spec
edit, a bake-side encode change, a draw-side read change and a full re-bake of
every shipped `_n`.

## Files

Changed: `src/lodgen.cpp`, `src/impostorpreviewtest.cpp`,
`res/shaders/impostor_oct.vert` (+ `release/shaders/impostor_oct.vert`),
`tests/spells/impostor_draw.sh`, `MISTAKES.md`.
New: `tests/spells/impostor_sheet_check.py`, `tests/spells/impostor_bc_decode.py`,
`tests/spells/impostor_bake_views.py`,
`.claude/skills/ww-reference-card-diagnose/SKILL.md` (and the same file in
`E:/Projects/Claude/.claude/skills/`).

Exe: `release/NifSkope.exe` 23,353,856 B, 2026-09-19 14:03:07, sha1
`161568a58be437a679aff747a172de2084bc9275`. Rung
`release/NifSkope.before_impostorfix1.exe` 23,346,176 B, sha1
`88d6abb32dfda556c2144ef912575db1862d794e`, taken once at 13:45:15 and not
touched since. The deployed shader is 2026-09-19 14:15:46 and `cmp`-equal to
`res/`.

Pictures: `scratchpad/impostorfix1_20260919/images/` -- `00_before_after.png`,
`10..14_orbit_*_el15/el45.png` + `_card.gif`/`_mesh.gif`,
`20_parallax_is_the_flake_maker.png`, `30_distance_blast_n4.png`.
Fixtures: `scratchpad/impostorfix1_20260919/fixture/*/cards` (repaired) and
`cards_before/` (the shipped DDS kept once beside each).
