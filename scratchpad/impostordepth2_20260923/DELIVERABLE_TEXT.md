# IMPOSTORDEPTH2 -- text for the overseer to splice (the lane edited none of HANDOFF / WW_CHANGES / MISTAKES)

Exe `release/NifSkope.exe` built 13:43:42 after qmake, 24,090,624 B, sha1 6b8ed793 (BUILD-RC=0) --
the flat snap. The earlier f7403e44 (12:08:05) had the depth-moved snap at 0.
Rung: `release/NifSkope.before_impostordepth2.exe` = DEPTH1's e294ae80. Nothing committed.

## HANDOFF entry

**IMPOSTORDEPTH2 (2026-09-23) -- BC7 depth sheet, edge fix always on, the slider's two ends.**
- The card `_n` sheet is now **BC7** (DX10, DXGI 98). Encoder in-tree: `src/lodgenbc7.h` (header
  only, uses the vendored detex tables, deterministic, weights R1 G1 B32 A1). Every other sheet and
  the `.lodm` are byte-identical to the rung. Card ARRAYS: the `_n` array is BC7 too (verified by
  `lodgen_card_arrays.sh`). The aggregate `_n` and every mesh `_n` stay BC3.
- Height error against the height lodgen is given (bake PNG after `lodgenRepairOctHeight`):
  DXT5 2.64 levels mean / p95 7 / 27 of 58 heights -> BC7 **0.57 / p95 2 / 57 of 58**. Normal X/Y
  11.3/8.8 -> 3.3/3.3. Sway (alpha) 0.02 -> 1.34 mean (p95 4): the cost. File +20 B (DX10 header).
  Compress 12.2 s -> 9.6-10.1 s (single runs).
- Coverage edge fix (decode each texel, then filter) is the only path: uniform removed,
  `WW_IMPOSTOR_COVFILTER` retired (reads, does nothing, the log says so).
- Slider (`ImpostorDraw::Options::slider`, `WW_IMPOSTOR_SLIDER` harness override): 0 = the crisp end =
  the default = **FLAT SNAP** (bungo's ruling 2026-09-23 13:1x): the nearest frame alone, NOT moved by
  its depth -- the same picture as `WW_IMPOSTOR_BLEND=0` (the trunk gate proves them byte-identical);
  it still writes its depth. 1 = the smooth end = stipple + depth search 16; between = three frames
  with weights sharpened to the power 1/x. New `Resolved::parallax` drives the shader's
  `useHeightBlend`. The depth-MOVED snap is kept only as a harness override (`WW_IMPOSTOR_SNAP=1` /
  `Options::snap`; the log names it so); it fails the trunk bar (el 20: 203 of 360 views too thin).
  `WW_IMPOSTOR_SEARCH` stays a harness override. No menu row (none was asked for).
- **Known red, ruled:** the flat snap fails the TEAR bar (el 0 4.68%, el 20 10.14%) and jumps
  15 / 64 px (reported, not gated). bungo saw the GIF, was told both, and ruled it. The trunk gate
  keeps the tear row at its bar and names it "KNOWN RED (bungo's ruling 2026-09-23 13:1x)" -- the
  same treatment as native_lighting.sh's two legacy_btr reds.
- **Open finding:** the smooth end at el 20 misses T2 on 2 of 360 views (az 161, two trunk runs vs
  one) -- the same miss DEPTH1 had on uncompressed sheets, so not the sheet.
- qmake run (it was owed for the new `src/lodgenbc7.h`): Makefile.Release now names it.
- **`impostor_draw.sh` row 5, ruled (director 2026-09-23):** on the N=4 blast card the flat snap
  reads **0.3920 < 0.50** (same views: `WW_IMPOSTOR_BLEND=0` 0.3920, moved snap 0.5323, smooth
  end 0.6565). Kept at its bar as a NAMED KNOWN RED ("4x4 flat snap is below the outline floor by
  construction; not the shipped grid"). New twin **row 5t** measures the shipped grid, 8x8 at 2k
  (bungo 09:3x; DEFAULTS2 flips the default next), at the crisp end, same 0.50 floor: **0.7029
  PASS** (TreeMapleInstitute06Green, the lane's BC7 n8_2k card; `IMPOSTOR_LODM_8` /
  `IMPOSTOR_NIF_8` override; a missing fixture FAILS, never skips).
- Stale comments: `src/lodgen.h:405` and `src/nifcli.cpp:7844` say "there is no BC7 encoder in this
  tree" (about the `_msn` cache). There is now; whether the cache should use it is bungo's call.
- Lane folder: `scratchpad/impostordepth2_20260923/` (progress.md, gates/, gifs/, diag2.out).

## WW_CHANGES entry

### Impostor card: BC7 depth sheet, edge fix always on, crisp/smooth slider (lane IMPOSTORDEPTH2, 2026-09-23)
- lodgen writes the card's `_oct_n.DDS` (and the card `_n` array) as BC7 (DX10, DXGI 98) with an
  in-tree deterministic encoder (`src/lodgenbc7.h`). The height comes back 0.57 levels off on
  average instead of 2.64; 57 of 58 heights survive instead of 27. Other sheets unchanged.
- The card's coverage edge is always decoded per texel and then filtered; `WW_IMPOSTOR_COVFILTER`
  is retired.
- New draw slider: 0 (default) = crisp end, the nearest frame alone, flat (not moved by its
  depth); 1 = smooth end, stipple + depth search 16. `WW_IMPOSTOR_SLIDER` overrides it in the
  harness; `WW_IMPOSTOR_SNAP=1` is now a harness-only depth-moved snap.
- Docs: `docs/LODGEN_CARD_SHEETS.md` (§1.1, §1.2, §4, §6, new §6.1, invariant 7) and
  `docs/LODGEN_IMPOSTOR_SPEC.md` (table, the BC7 paragraph, "What FO4CS must decode").
- Gates: new `tests/spells/impostor_sheetbar.py`; `impostor_trunk.sh` rewritten;
  `impostor_bc_decode.py` reads DX10 (BC7 via Pillow); re-pinned `lodgen_octahedral.sh`,
  `lodgen_card_arrays.sh`, `impostor_draw.sh` rows 15, 17, 18; new row 5t (8x8 twin of row 5).

## MISTAKES entries

- **2026-09-23 IMPOSTORDEPTH2 -- a label taken from `$1` after `shift`.** The lane's discriminator
  (`diag.sh`) named its bar run `$1` after `shift 3`, so every arm that carried an env switch passed
  the switch as the label, the bar crashed on a bad path, and the grep that followed hid the
  traceback. Two 360-view orbits were wasted before the empty result was noticed. Rule: capture
  names into locals before any `shift`, and never pipe a measurement through a grep that can hide a
  traceback without also checking the line count.
- **2026-09-23 IMPOSTORDEPTH2 -- a floor on views where it cannot fire.** The first knob row
  "slider 0.5 is not the smooth end" used six views on the frame grid, where the blend weights are
  1/0/0 and sharpening them changes nothing: 1 px. The views now sit off the grid (192,501 px).
  Rule: a knob row must be run on views where the knob is live, or it tests the fixture.
- **2026-09-23 IMPOSTORDEPTH2 -- `--arrays` on a single `-o` chunk writes no arrays.** The card
  arrays are built only in the worldspace/region path (`nifcli.cpp:4634`). Use
  `lodgen_card_arrays.sh` (synthetic, one minute) to prove an array change.

## Re-pinned gates (bars never lowered)

| gate | what moved | by how much |
|---|---|---|
| `lodgen_octahedral.sh` ~752 | `_n` must be DX10 dxgi 98 arraySize 1; colour + mask stay DXT5 | format only |
| `lodgen_card_arrays.sh` | `_n` array wantDxgi 77 -> 98 (same size, 16-byte blocks) | format only |
| `impostor_draw.sh` row 17 | the blended run names `WW_IMPOSTOR_SLIDER=1` (the stipple is no longer the default) | torn share 0.1135 -> 0.1056, bar 0.1340 unchanged |
| `impostor_draw.sh` row 18 | all three runs `WW_IMPOSTOR_SLIDER=1` | az 0.32 -> 0.48, el 0.51 -> 0.57 of the mean cut, bar 1.5 unchanged; red control 2.06 / 2.09 |
| `impostor_draw.sh` row 15 | the parallax-ON run names `WW_IMPOSTOR_SLIDER=1` (the flat default would make the row trivially equal) | off 0.9108, on 0.9102, gap bar 0.01 unchanged |
| `impostor_draw.sh` row 5 | bar kept; now a NAMED known red (4x4 flat snap, not the shipped grid) | IoU 0.5147 (stipple) -> 0.5323 (moved snap) -> 0.3920 (flat) |
| `impostor_draw.sh` row 5t (new) | row 5's twin on the 8x8 2k card at the crisp end, floor 0.50 | 0.7029 PASS |
| `impostor_draw.sh` row 16 | not re-pinned; measures the flat-snap default | 16b 0.939 -> 0.940 |
| `impostor_trunk.sh` | rewritten for the rulings; tear references pinned to DEPTH1's measured drawer; crisp rows = flat snap, tear row a named KNOWN RED (bungo 13:1x) | bars unchanged |

## Gates (exe 6b8ed793, the flat snap; outputs gates/*.flat.out)

| gate | result |
|---|---|
| impostor_trunk.sh | 38 checks, 3 failures: crisp tear el 0 4.68% and el 20 10.14% (KNOWN RED, bungo 13:1x); smooth el 20 T2 on 2 of 360 views (OPEN). Crisp T1 0 of 360 outside at both el, T2 ok; jumps 15.25 / 64.36 px reported |
| impostor_draw.sh (blast_n4; gates/impostor_draw.final.out) | 33 steps, 1 failure = row 5 0.3920, the NAMED known red; row 5t 0.7029 PASS; 15/16/17/18 pass |
| impostor_aa.sh | 7 checks, 0 failures |
| impostor_shrubs.sh | RESULT PASS |
| not re-run | lodgen_octahedral / card_arrays (no lodgen object rebuilt), native_lighting (draws no card) -- their f7403e44 results below stand |

## Gates (exe f7403e44, the depth-moved snap at 0; outputs gates/*.new.out)

| gate | result |
|---|---|
| impostor_trunk.sh (new) | 38 checks, 5 failures: crisp el0 T1; crisp el20 T1, T2, tear; smooth el20 T2 (2 views) |
| impostor_trunk.sh (rung) | 38 checks, 20 failures: every trunk row red, floors hold |
| impostor_draw.sh (blast_n4) | 32 steps, 0 failures |
| impostor_aa.sh | 7 checks, 0 failures |
| lodgen_octahedral.sh | RESULT PASS |
| lodgen_card_arrays.sh | RESULT PASS (the `_n` array is BC7) |
| impostor_shrubs.sh | RESULT PASS (54 of 54 baked, 0 empty) |
| native_lighting.sh | 21 checks, 2 failures = the 2 known legacy_btr reds (control) |

Worst per-step jump (T3, reported not gated): flat snap (now the crisp end) el 0 15.25 px, el 20
64.36 px; the moved snap was 3.38 / 20.35 px.

## Skill review (finished work)

- `ww-shader-devloop-nobuild` §4: add "a discriminator arm's label is captured before any
  `shift`; and a SINGLE frame moved by its height thins thin parts (disocclusion) -- test snap
  variants flat vs parallax before blaming the sheet".
- `nifskope-ww-lodgen`: add "the card `_n` is BC7 since 2026-09-23 (`src/lodgenbc7.h`); a reader
  picks the decoder from the header; card arrays are built only on a worldspace/region run, never
  `-o`; prove an array change with `lodgen_card_arrays.sh`".
- `ww-test-harness-add`: add "a knob row runs on views where the knob is live (off the frame grid
  for anything that touches blend weights)".
- `nifskope-ww-build-verify`: the lane hit the "new header in no Makefile list" case again
  (`lodgenbc7.h`); the section already covers it -- no change.
