DONE -- lane SEAM1, worktree E:\Projects\NifskopeWWE-seam1, branch seam1-20260925 (from main ea0ca708). Built, gated, INSTALLED 16:14 (not flown).

- **Build.** The game went down at 13:46. The final exe is b9fd029b (release/NifSkope.exe, built 14:15:55).
- **Install.** The whole-map VT is installed into mods\FO4CSLOD\FO4CSLOD\Commonwealth: fill ON (bungo's ruling (a)), cover and height on, as in BAKE1.
- **Backups.** The 6 files it replaced are in `replaced/`.

## Commits (nothing pushed, nothing merged)

| commit | what |
|---|---|
| c21eb26a | **Sanctuary edge root fix** (docs/LODGEN_TERRAIN_VT.md §2.5 step 4). A LAND quadrant with no BTXT, and an ATXT layer that names LTEX 0, now paint the engine's default land texture (CommonwealthDefault01). Before, they painted the chunk's dominant base. |
| a6e5e8de | **`--vt-fill-vanilla`** (CLI and panel, OFF by default). Unpainted ground is blended toward Bethesda's dim-4 LOD colour (§2.6). The vanilla sheets are read loose at bake time and never shipped. |
| 62e53a3b | **`.lodo` v5**: an optional per-vertex colour stream, written only for shapes that have a colour channel AND Vertex_Colors. |
| 28605665 | Gate scripts. |
| 44805f8f | **Grass tint** (§1.1). The bake divided the smallest mip by alpha, but DDS mips hold straight alpha. Every alpha-cut grass therefore tinted white, which is the "sandy" cover. It now takes the alpha-weighted mean over the first mip of 1024 px or less. |
| 59a0dd33 | **Default-texture path fix.** c21eb26a's path had `\G`/`\C` escapes; g++ drops the backslash, so the path named no file and painted grey. Now escaped. Also adds the DG1 gate, escape_scan.py and grass_pic.py. |

## Grass question (bungo: "my grass is green, but the cover turns sandy")

**C4 wins: our averaging.**
- **Old tint.** Every GRAS came out (255,255,255).
- **New tint.** Examples: TG_DriedGrassObj01s (65,69,40), TG_GrassPatch_S (126,136,82).
- **Sanctuary cover texels at the default tint 0.35.** Old (137,129,121); new (90,83,62).
- **C1 is out.** The files we open are the same TrueGrass/BNS/vanilla files MO2 resolves (grass_census.txt).

## Gates on b9fd029b

All gates were pre-registered, and each was run on the broken or old exe first to prove it can fail.

| gate | before | after |
|---|---|---|
| G1 Sanctuary edge step (edge_gate.py) | shipped 12.93 RED | 0.41 GREEN |
| Grass (grass_gate.py: tint-1 vs tint-0 solve) | old (238,235,236) RED | (101,97,62) / (99,100,66) GREEN |
| DG1 default-ground grey share (default_gate.py) | broken exe 0.1635 RED | 0.0064 GREEN (old exe 0.0064) |
| escape_scan.py over the branch diff | -- | 0 lines |
| FG1 flat dim-4 chunks (SD < 2) | shipped 2019 | 0 (fill off: 0 too) |
| FG2 painted tiles byte-identical; other tiles identical except colour | -- | GREEN, 0 differ |
| FG3 the fill is wired | -- | colour moved on 8236 tiles, GREEN |
| A2 (a2_grey_after.txt) | 2019 flat | 0 flat, 0 chunks without vanilla |
| fill_model, file vs model: Sanctuary north | -- | cell max 10.03 (model F 12.78); at-line 6.36 < bar 10.15. GREEN |
| fill_model: Glowing Sea edge | -- | cell max 8.96 (F 17.82); at-line 8.25 < 12.63. GREEN |
| fill_model: north-east (load-order model) | old model RED | GREEN; planted RED (fm_lo3.out) |
| whole-map grey share (VT.16, chroma < 12 and lum > 100) | shipped 0.884 | fill off 0.0004, fill on 0.0103 |
| W4 (w4_gate.py, G1 re-pinned) | old exe: G2 RED; pre-pin G1 RED | G1 GREEN, G2 GREEN (exe cbdbffe7) |
| spells lodgen_native / lod_generation / lodgen_loadorder | -- | 32/0, 128/0, 24/0 PASS |

**North-east fill_model: RED under the old model, GREEN under the load-order model (coordinator order (B), 16:4x).**
- **What was wrong with the model.** It read Fallout4.esm's LAND everywhere. In his load order DLCCoast.esm's LAND
  wins 253 Commonwealth cells and paints x 11-16, y 29-35 with LDriedGrass01 (a whole-cell BTXT, material-backed).
  The model called those cells unpainted default ground; it also painted every material-backed layer flat grey.
- **The fix.** land_lo.py: the painted set from the winning LAND (4086 cells; +117 DLCCoast, +14 DLCNukaWorld).
  land_lo_esm.py + lo_model.py: B composites the WINNING LAND, LTEX/TXST from Fallout4.esm + the DLC masters, and a
  material layer's diffuse from its .bgsm. `LAND=esm PAINTED=esm` reproduces the old model.
- **The step is real.** Dried grass beside unpainted default ground is a step the game draws up close too; the fill
  leaves it (w = 0 at the line). The model now has it: 22.1 at the line, 20.8 cell-mean.
- **The gate, per border.** File step <= model step + vanilla's bar (line 16.73, cell 10.86). The old region-max
  cell clause had no tolerance for the model's absolute offset (the file sits 10-19 lum off the model in every
  region); it still reads 27.44 vs 20.76 here and is printed beside the verdict.
- **Planted refuter** (PLANT=auto, now pushed OUTWARD: +40 on a cell already darker than its painted neighbour
  narrowed the step and hid itself). The runs are in fm_lo3.out:

| region | installed bake | planted +/-40 | old model |
|---|---|---|---|
| north-east | GREEN (line 32.61 vs B 22.07; cell 27.44 vs 20.64) | RED (72.61; 66.84) | RED |
| Sanctuary north | GREEN | RED (42.98) | GREEN (old) |
| Glowing Sea | GREEN | RED (39.60) | GREEN (old) |

**W4 G1: attributed, re-pinned (coordinator order (A)).**
- **Bisect (prefix run, no stash; bisect/).** Exe a6e5e8de: 228, 0 diffs. Exe 62e53a3b: 201 on vertex rows 270124
  and 271177, byte 15 (water-tower meshes). 62e53a3b moves them.
- **Mechanism: codegen, not law.** The AO block of lodofile.cpp is the same text at both commits; lodgenao.h is
  untouched. 62e53a3b grew lodoAppendMesh around the inlined ambientOcclusion. -O3 -march=haswell may then contract
  a*b+c into FMA differently, and one of the 8 rays sits at a grazing tie (1 - 0.85*1/8 -> 228; 2 hits -> 201).
- **Proof.** Both trees rebuilt with lodofile.cpp under `#pragma GCC optimize("fp-contract=off")` bake
  byte-identical W4 files (w4_bytes.py fpc_a6e5e8de fpc_HEAD: 0 diffs, both regions).
- **Re-pin.** w4_gate.py G1 puts back exactly those two bytes, exactly 228 <- 201, before the compare. Every other
  byte and both CRCs must still match, and a third value refuses. The old gate is w4_gate_prepin.py.
- **Refuter of the ruling.** Any other byte moving, or these two holding a third value.

**Fill bake census (whole/on/log.txt).**
- **Tone fit.** One global fit: gain 0.612, offset 33.7, saturation 1.85.
- **Coverage.** Band 2 cells; 8321 tiles touched.
- **Missing vanilla.** 196 vanilla chunk sheets were missing and 1.6 M texels had no vanilla. A2 still reads 0 chunks without vanilla.

## Install (16:14, game down)

- **Source.** The files are `whole/on/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.{2,4,8,16,32}.lodt` and `Commonwealth.VT.lodm`, written by whole_vt.sh on. They were moved into FO4CSLOD.
- **Backups.** The shipped files were moved to `replaced/`.
- **sha1 checks.**
  - `install_sha1_before.txt` (shipped) equals `install_sha1_replaced.txt`.
  - `install_sha1_after.txt` equals `install_sha1_new_src.txt`.
- **VT.2.** Before 0093d335, after f5f15e79.
- **Rollback.** Move the 6 files from `replaced/` back.
- **Scope.** Nothing else under E:\Projects\Fallout 4 Mods was touched.
- **Leftover.** `whole/off/` (15 GB, the fill-off control) can be deleted.

## The 13-gate set on the final exe (16:41-17:52, game down): 13 of 13 green

- **Exe.** cbdbffe7 (release/NifSkope.exe). After the probe builds it was rebuilt from HEAD; it links as 4024369a, and only 4 PE-header bytes differ (link time and checksum).
- **Runner.** gates13.sh, then the reruns (gates13_rerun*.sh, gates13_attr.sh, gates13_repin.sh). Verdicts are in gates13/summary.txt. One NifSkope ran at a time, on the second monitor, each on its own port.

| spell | verdict |
|---|---|
| impostor_pbrm | 14/14 ok |
| impostor_ring | 17/0. The first run hit a missing rung exe, which is now copied in. |
| impostor_wind | **28/0 after a re-pin.** The red was G2: Hero's `_oct_n.DDS` moved against the previous exe. See below. |
| lodgen_octahedral | PASS |
| lodgen_impostor_cards | PASS |
| lodgen_card_arrays | PASS |
| lodgen_cardlink | **PASS (0 failures) after a re-pin.** The red was ID: 2 of 34 files differed. See below. |
| lodgen_incremental | FAILURES: 0 |
| lodgen_native | PASS |
| lod_generation | 128 checks |
| lodgen_loadorder | PASS |
| pbr_shade_ab | 10/0. The first run's 2 empty pictures were flaky and the rerun was clean. |
| native_lighting | **21/0.** The red was a fixture, not SEAM1: resroot was not copied into this worktree, so textures did not load (legacy_bto_top 390,854 B, the size the spell's own note gives). Main's 828ac612 reads 21/0 on the same fixtures. |

**impostor_wind G2, attributed to a6e5e8de.**
- **What moved.** From the same PNG, 57 BC7 blocks of Hero's `_oct_n` changed, in their index bits only.
- **Which commit.**

  | exe | `_oct_n.DDS` sha1 |
  |---|---|
  | s5, 828ac612, c21eb26a | ab28c561 |
  | a6e5e8de, 62e53a3b, cbdbffe7 | 77fb8981 |

- **Mechanism: FMA contraction.** The encoder's double-precision fit (lodgenbc7.h) is compiled inside lodgen.cpp. With that file built under fp-contract=off, ea0ca708 and HEAD both give 3782af27. The evidence is bisect/bcrun.out, fpcL.sh and bc7cmp.sh.
- **Re-pin.** The spell accepts only that exact sha1 pair for that one file.
- **Self-test (iw_pin_selftest.out).** The pair as is passes. One more byte in `_oct_n` fails, and so does one byte in `_oct_d`.

**lodgen_cardlink ID, attributed to 62e53a3b.**
- **What moved.** Two things, both from 62e53a3b:
  - the .lodo v5 colour stream;
  - the same two selfAO bytes as W4 (rows 270124 and 271177, 228 to 201).
- **Measured in cl_id.py / cl_id.out.** With the stream taken out and the two AO bytes put back, only the version word differs. The .lodi differs only at 0x0C..0x0F and 0x20..0x27.
- **New checker.** tests/spells/lodgen_cardlink_id.py. Every tolerance in it is exact and named.
- **Self-test (cl_id_selftest.out).** Each of these fails:
  - another AO byte;
  - a third value in an attributed byte;
  - one planted .BTR byte.

## Still owed

- A Boston oblique render with the .lodo v5 colour on. The .lodo was not re-baked, so v5 is not installed.
- The FO4CS reader of the .lodo v5 colour (standing order, not news).
- The model reads vanilla assets, not his MO2 textures (BNS Landscape re-points 26 LTEX); texture mods stay out of B.
- Offered: build lodofile.cpp (the selfAO) with -ffp-contract=off so a later edit of that file cannot move AO bytes.
  That moves ~14.7k .lodo bytes once (fpc bakes vs old), so it is the director's call, not done here.

## Pictures (untracked, pics/)

- **Sanctuary, 08 camera** (view 8, cells -23,18..-16,25, Z 6690):
  - `seam/sanctuary_before_oblique.png`
  - `seam/sanctuary_after_oblique.png`
  - `seam/sanctuary_before_after.png`
- **Whole map, terrain bounds** (-42..32 x -48..38, view 8):
  - `seam/whole_before_oblique.png`
  - `seam/whole_after_oblique.png`
  - `seam/whole_before_after.png`
- `grass_green_vs_sandy.png`: GRAS swatches, old vs new, plus Sanctuary crops from the old exe and b9fd029b.
- `controls_side_by_side_oblique.png`, `w3_overview_before_oblique.png`, `edge_zoom4x_before_oblique.png`, `fill_model_sanctuary_north.png`, `ao/`: the earlier set.

## Skill review

- **Loaded:**
  - ww-artefact-localise
  - ww-texel-picture
  - ww-lodl-offline-census
  - nifskope-ww-render-shot
  - fo4-nif-vertex-channel-census
  - nifskope-ww-worktree-build
- **Written:** ww-lodo-version-bump.
- **Updated:** nifskope-ww-worktree-build §7. Copied objects are stamped newer than sources edited before the copy, so touch the changed sources before the first make.
- **Wished for:** a skill that says which exe and which sheet cache a native render really used.
