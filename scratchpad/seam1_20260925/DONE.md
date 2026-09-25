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
| fill_model: north-east | -- | RED at one border, see below |
| whole-map grey share (VT.16, chroma < 12 and lum > 100) | shipped 0.884 | fill off 0.0004, fill on 0.0103 |
| W4 (w4_gate.py) | old exe: G2 RED | G2 GREEN; G1 RED, see below |
| spells lodgen_native / lod_generation / lodgen_loadorder | -- | 32/0, 128/0, 24/0 PASS |

**North-east fill_model RED.**
- **Where.** One border, cells (11,30)/(12,30), steps 25.4 (line step 29.4, bar 16.73).
- **Not the fill.** The per-cell colour there is the same with the fill off.
- **Why the gate fails.** Cell (12,30) is painted in his load order, so the fill leaves it alone. The model's painted set comes from Fallout4.esm only, so it expects the fill to touch that cell.
- **Verdict.** A gate-definition mismatch, not a fill defect. Before this gate is trusted again, it should read the bake's own painted set.

**W4 G1 RED.**
- **What differs.** The no-colour strip of the .lodo differs from the old exe's output in the selfAO bytes on the water-tower meshes, plus the CRCs.
- **Deterministic.** It gives the same result on every run of a given exe.
- **Unproven hypothesis.** An FMA/inlining codegen shift in the header-inline selfAO.
- **Scope.** The .lodo was not re-baked or installed.

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

## Still owed

- A Boston oblique render with the .lodo v5 colour on. The .lodo was not re-baked, so v5 is not installed.
- The FO4CS reader of the .lodo v5 colour (standing order, not news).
- The north-east fill_model gate should use the bake's own painted set.

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
