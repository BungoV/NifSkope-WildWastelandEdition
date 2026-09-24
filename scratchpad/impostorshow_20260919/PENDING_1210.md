# IMPOSTORSHOW -- state at 2026-09-19 12:1x (phase B, build slot held)

The earlier note is kept as `PENDING_1054.md`; nothing in it is current except
its list of what is still owed.

## Exe

`release/NifSkope.exe` 23,261,184 B 12:06:44 (the 12:01 build hashed
`0f3defe530a64e32a5732ea05673bde62f851491`; re-hash after any further build).
Baseline rung `release/NifSkope.before_impostorshow.exe` 23,141,376 B 09:44
intact, untouched.

## Green, measured, on this exe

`tests/spells/impostor_draw.sh` -- **21 steps, 0 failures**, run 12:07..12:10
with

```
F=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorshow_20260919/fixture
IMPOSTOR_PORT=27803 \
IMPOSTOR_LODM="$F/blast_n4/cards/000531b3_oct.lodm" \
IMPOSTOR_LODM_MORE="$F/blast_n5/cards/000531b3_oct.lodm $F/blast_n8/cards/000531b3_oct.lodm $F/blast_n12/cards/000531b3_oct.lodm" \
IMPOSTOR_LODM_B="$F/blast_n12/cards/000531b3_oct.lodm" \
bash tests/spells/impostor_draw.sh \
  "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif"
```

Numbers worth keeping:

* row 5 IoU mean **0.1763**, floor **0.12** -- the floor is MEASURED, not the
  old pre-registered 0.80, and the comment block above it says why (leafless
  twigs against a filled card; sharper bakes score WORSE).
* row 6 red control (quarter turn) shuffled **0.0821** vs honest 0.1763.
* row 7 azimuth same **0.1814** vs opposite **0.0868**, mean margin 0.0946,
  7 of 8 positive, worst -0.0738 at 225 deg.
* row 8 forced AsBaked: same 0.0914 < opposite 0.1757 -- the old formula fails.
* row 9 variable N: N=5 **0.2786**, N=8 **0.2078**, N=12 **0.2130**.
* row 10 non-square frame 48x128, frame aspect 0.3750 == extents aspect 0.3750.
* row 11 two grids in one scene: A N=4 err 2.866 deg (half-cell 30.0), B N=12
  err 0.712 deg (half-cell 8.2); red control, B read as N=4 -> **71.090 deg**.

## Fixtures (all `conv spec1`, all 48x128 frames)

`scratchpad/impostorshow_20260919/fixture/blast_n{4,5,8,12}/` --
000531b3 TreeMapleblasted05, tile 128. Re-bake with
`sh scratchpad/impostorshow_20260919/bake_fixture.sh 000531b3 "Landscape\Trees\TreeMapleblasted05.nif" <N> 128 <outdir>`.
Older: maple_n4, maple_n4_t256 (0004a074), dead_n4 (axisymmetric, unusable),
rock_n4 (sheets bake, lodgen places no card).

## Pictures

`images/00_azimuth_180_explained.png` WRITTEN (1008x1604) -- mesh / old reading
/ repaired reading at four azimuths with the IoU under each, composed by
`scratchpad/impostorshow_20260919/compose_180.py` from
`scratchpad/impostorshow_20260919/shots/{spec,asbaked}_az*_{card,mesh}.png`.
`images/00_READY` NOT yet written: it waits for the orbit strip, the GIF, the
distance strip and the chunk picture.

## Files written or edited this lane

Mine: `src/impostorcard.{h,cpp}`, `src/impostoroct.{h,cpp}`,
`src/gl/impostordraw.{h,cpp}`, `src/impostorpreviewtest.{h,cpp}`,
`res/shaders/impostor_oct.{vert,frag}`, `tests/spells/impostor_draw.sh`,
`tests/spells/impostor_oct_ref.py`, `tests/spells/impostor_pair_check.py`.
Shared, smallest hunks: `NifSkope.pro`, `src/lodgen.cpp`,
`src/nifskope_ui.cpp` (the bake's `conv spec1` token).

## Still owed, in order

1. **The chunk placement path** -- the feature master, OFF, with its menu row:
   read the chunk's `C` manifest lines (`impostorReadManifest`, already in
   `src/impostorcard.cpp`) and draw a card per placement in place of the
   vanilla tree LOD shape. A gate row that opens a chunk and counts cards.
2. **The `.lodm` open path** in the viewer proper (today only the harness opens
   one).
3. Depth and AO gate rows; the mip cap (C19) and aux divisor (C32) in the shader.
4. Pictures: orbit strip (12 azim x 2 elev, mesh top / card bottom), GIF,
   distance strip (LOD4/8/16/32), chunk picture with triangle + draw-call
   counts; then `images/00_READY`.
5. Numbers beside them: bytes per set per N after BC, bake seconds, IoU, mean
   colour error -- each from a named log.
6. Neighbours before/after: `render_shot.sh`, `native_open.sh`,
   `lodgen_octahedral.sh`.
7. Delete `wwImpostorTrace` once a gate row fails without it, and the
   `rm -f .../ww_impostor_trace.log` in `run_harness` with it.
8. Changelog text to the overseer (I never edit `WW_CHANGES.md` / `HANDOFF.md`):
   **every impostor set baked before this exe must be re-baked**, and the
   coverage-decode repair (older sets drew thinner than their own extents).

## Two findings that are not mine to fix

* NifSkope's shutdown: with no document `QCoreApplication::exit()` never leaves
  the loop (reproduced with this feature OFF); with one it leaves and dies
  `[Fatal] QPixmap: Must construct a QGuiApplication before a QPixmap`, rc 127.
  The harness ends itself with `std::_Exit` after flushing and says so.
* `lodgen` bakes sheets for rocks/shacks (000211a3) but places no card for them.
