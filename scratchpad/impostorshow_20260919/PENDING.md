# IMPOSTORSHOW -- state at 2026-09-19 13:2x (phase B, build slot held)

Earlier notes kept as `PENDING_1210.md`, `PENDING_1054.md` and
`PENDING_phaseA_0930.md`; nothing in them is current except their lists of what
is still owed.

## Exe

`release/NifSkope.exe` **23,342,080 B, 13:09:26, sha1
`e3a59b973c67215e9da6ec43716879550b8bbb1a`**.
Shader copies in step (`res/shaders/impostor_oct.frag` is a runtime resource and
was re-copied to `release/shaders/` after every edit).
Baseline rung `release/NifSkope.before_impostorshow.exe` 23,141,376 B 09:44
intact, untouched. GLTFEXPORT1's files compiled; none of its gates were run.

## THE HEADLINE OF THIS SESSION: two real drawing defects, found and repaired

The distance strip is what exposed them -- the card drew as a wide sparse spray
at every apparent size while all four mesh rows drew a trunk. Neither was
visible in a single picture; both were found by moving one variable at a time.

**1. The height blend's sign was inverted.** `res/shaders/impostor_oct.frag`.
`d` is measured along `frameFwd`, which is row 2 of the bake's Euler matrix and
points from the object TOWARD the camera, while the baked height is positive
BEHIND the card plane (the bake writes `gl_FragCoord.z`,
`res/shaders/fo4_default.frag:310`). The parallax was therefore applied
backwards, which does not merely fail to remove the frames' disagreement -- it
DOUBLES it. Now `want = -( h - 0.5 ) * cardDepthSpan`. The refuter is stated in
the file: a correction applied the right way round cannot be worse than not
applying it at all. The pixel depth offset in section 2 keeps its `+`, and the
comment says why (it moves along `ray`, which points away from the camera).

**2. `frameOffset` was never read by anything in the draw path.** The bake
slides each view's silhouette to its own frame's centre and records the slide
(`lodgen.cpp:2999`); `src/impostorcard.cpp` mentioned the key only in a comment
and no parse existed. Every frame's picture therefore sat up to 13% of a frame
from where it belonged, and the three blended frames sat in three DIFFERENT
wrong places. lodgen's own note predicted exactly this: "a reader that ignores
the key gets ... a tree that steps sideways by the offset it skipped".
Now parsed by `readFrameOffsets` (all-or-none; absent and short arrays both fall
back to the older all-centred law), reached through
`ImpostorCardSet::frameOffsetOf`, uploaded as `frameOffset[3]` scaled by the
placement scale, and subtracted in `frameUvOf`.

**The sign of #2 was checked, not assumed.** One view at azimuth 45 said the
offset made things WORSE (0.3114 -> 0.2706), which is why it was then tested
over 24 views: subtract 0.4401 / add 0.2431 with 8 of 24 views degenerate. The
derivation was right and the single view was misleading. Do not re-litigate this
from one picture.

### Measured effect (orbit, 24 views each, chrome off, 512x768 proved)

| subject | before | after |
|---|---|---|
| blast_n4  (TreeMapleblasted05 N=4)    | 0.1949 | **0.3546** |
| maple_n4  (TreeMapleForest2)          | 0.1659 | **0.3206** |
| dead_n4   (BlastedForest...Upright01) | 0.1533 | **0.3957** |
| rock_n4   (RockCliff02_Alt, non-tree) | 0.4419 | **0.7944** |
| blast_n8  (N=8)                       | 0.2255 | **0.4152** |

This also CLOSES the open observation in the earlier note that the rock card's
vertical extent disagreed with its mesh. It was defect #2.

## Green, measured, on this exe

`tests/spells/impostor_draw.sh` -- **24 steps, 0 failures**, 13:15 and again
13:17 after the floor was raised:

```
F=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture
IMPOSTOR_PORT=27806 \
IMPOSTOR_LODM="$F/blast_n4/cards/000531b3_oct.lodm" \
IMPOSTOR_LODM_MORE="$F/blast_n5/cards/000531b3_oct.lodm $F/blast_n8/cards/000531b3_oct.lodm $F/blast_n12/cards/000531b3_oct.lodm" \
IMPOSTOR_LODM_B="$F/blast_n12/cards/000531b3_oct.lodm" \
IMPOSTOR_CHUNK="$F/blast_n4/chunk.bto" \
timeout 3000 bash tests/spells/impostor_draw.sh \
  "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif"
```

**The IoU floor was RAISED 0.12 -> 0.22** and the reasoning is written into the
gate above `IOU_FLOOR`. A floor that still passes the broken build is not a
floor. 0.22 sits above three of the four pre-repair numbers, so re-introducing
either defect turns rows 5, 9(N=8) and 9(N=12) red; it does NOT catch N=5, which
scored 0.2632 while broken, and the comment says so rather than inventing a
per-grid floor table that no measurement supports. This is the one direction a
floor may move -- up, after a repair, on measurement.

Row values now: 5 = 0.2778, 6 red control 0.0869, 7 margin 0.2740,
9 N=5 0.4735 / N=8 0.3917 / N=12 0.4053.

## OWED RULING, not shipped as a default: the alpha threshold (SPEC GAP #7)

The blend still costs silhouette on a bare tree: blend ON 0.3546 vs blend OFF
0.4401 over 24 views. The cause is not a bug -- it is the union effect. The
default threshold is the set's own `covFloor` = 16/255 = 0.063, so a texel any
ONE frame thinks is a twig survives, and the blended silhouette is the union of
three views. Measured sweep, blast_n4, 24 views:

| alphaThreshold | mean IoU | views counted |
|---|---|---|
| 0.063 (default, = covFloor) | 0.3546 | 24 of 24 |
| 0.12 | 0.3366 | 24 of 24 |
| 0.20 | 0.3748 | 23 of 24 |
| 0.30 | **0.4088** | 22 of 24 |
| 0.45 | 0.3465 | 22 of 24 |

0.30 scores best and ERASES the card entirely at 2 of 24 views. That trade is
bungo's to make, not this lane's, so the default stays at the set's declared
floor and the curve is reported. Spec 317..321 gives 0.5 for full crowns and
says a consumer "tests lower, or blends, for bare trees" without naming a
number; this table is the measurement that wording needs.

## Pictures in `images/` -- ALL REGENERATED on the repaired exe

`00_azimuth_180_explained.png`; `10`/`11` blast N=4; `12`/`13` maple forest;
`14`/`15` dead tree; `16`/`17` rock cliff (the non-tree, `--no-trees-only`);
`18`/`19` blast N=8; `20_distance_strip.png`.
No stale picture remains: every pre-13:09 strip and GIF showed the broken card
and has been overwritten. `20_distance_strip.png` also gained a caption saying
that azimuth 45 is a DIAGONAL -- the blend's worst case, kept on purpose rather
than swapped for a flattering cardinal -- with the 24-view mean beside it.

`compose_distance.py` had a composition fault of its own fixed: it downscaled
the 256 px column with NEAREST, which turned a trunk into confetti and libelled
the card. BOX when a tile comes down, NEAREST only when it goes up.

## Still owed

1. Sheet CONTACT SHEETS per subject (the four sheets `_d` `_n` `_gsaos` `_g`) --
   offline from the fixture PNGs, no exe run needed.
2. The CHUNK picture with triangle + draw-call counts (counts not yet sourced).
3. `images/00_READY` once 1 and 2 land.
4. Depth and AO gate rows; the mip cap (C19) and aux divisor (C32) in the shader.
5. Neighbours before/after: `render_shot.sh`, `native_open.sh`,
   `lodgen_octahedral.sh`.
6. Delete `wwImpostorTrace` in `src/gl/impostordraw.cpp` once the path it
   brackets has a gate row that fails without it, plus the matching `rm -f`
   of `ww_impostor_trace.log` in `run_harness`.
7. WW_CHANGES + HANDOFF text to the overseer (this lane must NOT edit those
   files) -- see below.

## Changelog text owed to the overseer

Must say plainly, in this order:

- **Every impostor set baked before this exe must be re-baked** (the 180-degree
  azimuth repair; a set without the `conv spec1` token is the old vintage).
- Two drawing defects repaired this session: the height blend's sign, and
  `frameOffset` never being read. Both were measured, both roughly doubled the
  silhouette agreement on every subject, and neither is behind a toggle.
- The coverage-decode repair (the sheet's alpha is ENCODED; decode before
  blending, test the fraction).
- The chunk master ships OFF and does not yet hide the LOD shape it draws over.
- Every IoU measured before 12:46 today was inflated by viewer chrome, and every
  IoU measured before 13:09 was measured on a card carrying the two defects
  above. Numbers quoted from earlier notes are not comparable to these.

## Findings that are NOT this lane's to fix

- NifSkope's shutdown does not finish when a window has no document, and fatals
  on `QPixmap` after `~QApplication` when it has one. Gate row 13b scores on the
  printed refusal and carries `timeout 300` because of this.
- lodgen places no card for a non-tree unless `--no-trees-only` is passed
  (`src/lodgen.cpp:3573`, refusal message at `:4257`).
- `GLView::glProjection` unions the AXIS MARKER into the bound sphere before
  choosing near and far. It did not bite the bake here only because the
  `max(radius, 1024)` floor dominated for these subjects; on an object whose
  bound radius exceeds 1024 the baked height channel's span and the `depthSpan`
  the `.lodm` declares would disagree. Observed, not chased, not this lane's.

## Files this lane owns and has written

`src/impostorpreviewtest.cpp`, `src/impostorcard.{h,cpp}`,
`src/gl/impostordraw.{h,cpp}`, `src/impostorchunk.{h,cpp}`,
`src/impostoroct.{h,cpp}`, `res/shaders/impostor_oct.{vert,frag,prog}`,
`tests/spells/impostor_draw.sh`, everything under this scratchpad, `images/`.

Shared files touched, smallest hunks, listed in full:
`NifSkope.pro`; `src/nifskope.cpp` (the `.lodm` open branch +
`ImpostorChunk::forget()`); `src/nifskope_ui.cpp` (menu row, plus the bake's
`rz = 270 - azim` repair and the `conv spec1` token); `src/glview.cpp`.
