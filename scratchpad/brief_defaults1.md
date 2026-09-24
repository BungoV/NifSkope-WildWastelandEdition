# Lane DEFAULTS1 -- bungo's ruled defaults go into the code: both identities OFF (legacy files = vanilla's vertex layout), his land-guide pick, road ground paint 0

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: `release/NifSkope.exe` 2026-09-12 19:45:31, 22,275,072 B
  (NATIVEVIEW1's). Rung ONCE before your first build: `release/NifSkope.before_defaults1.exe` (never delete any `release/NifSkope.before_*.exe`;
  never rename or delete any `NifSkope_inuse_*.exe` you did not create). Markers `scratchpad/defaults1_20260912/BUILDING` (touch FIRST) / `DONE`.
  Report `scratchpad/defaults1_20260912/lane_defaults1_report.md`, incremental; PENDING.md past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i Fallout4.exe` is ITS OWN COMMAND whose answer you read BEFORE the build or exe-launch command is typed -- never the
  check and the launch in one shell line (NATIVEVIEW1 ledgered exactly that). Up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port`
  is bungo's own window: rename the exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it. One GUI harness instance at a time,
  yours always `--port <unused>` and `WW_WINDOW_AT=1960,40`. Every path in argv and every WW_* path ABSOLUTE `E:/...`.
- Read first: `CONSTITUTION.md`; HANDOFF.md top block down to `RULED 15:53` (the five RULED paragraphs 15:53 / 15:56 / 16:03 / 16:55 and the
  SHOWCASE1 line "(5) --no-identity gates the WHOLE manifest" are THIS lane's charter); `src/nifcli.cpp` ~6383 (`lgIdentity`), ~6442
  (`lgTerrainIdentity`), ~6603-6690 (`--land-sample`, `--land-hex`, `--land-warp*`, `--land-guide`), ~6912 (`--road-ground-paint`), ~6977;
  `src/lodgen.h` 578 (`terrainIdentity`), 604 (`identity`), 979 (`roadGroundPaint`); `src/lodgen.cpp` ~3760 (the manifest written under the
  identity flag), ~4097 (`OBJ_VERTEX_DESC_COLORS + UV 2`), ~5945-6510 (land warp / guide / hex statics and their defaults);
  `src/lodgenmanager.cpp` (panel rows: `identityCheck`, `terrainIdCheck`, `wantIdentity()` 2288, `wantTerrainId()` 2296, the
  `wantIdentity() && arraysCheck` gates at 2585 / 3077 / 3155, land-guide rows ~1338-1380 and ~2411-2420, `roadGroundPaint` row 1487);
  `scratchpad/lane_land1_report.md` (the argv behind panel (c) of `a_land_guide_*.png` -- bungo's pick); `scratchpad/panel1_20260912/
  lane_panel1_report.md` §1 and `tests/spells/lodgen_byte_gate.sh` + `lodgen_tree_digest.py` (the panel-vs-CLI byte gate you will reuse);
  `tests/spells/lodgen_terrain.sh`, `lod_generation.sh`, `lodgen_identity.sh`, `lodgen_farring.sh`, `lodgen_native.sh`,
  `lodgen_native_baseline.sh`, `lodgen_tree_sway.sh`, `lodgen_ground_cover.sh`, `lodgen_terrain_vt.sh` (which of them assert the OLD defaults).
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen`, `nifskope-ww-build-verify`, `ww-module-off-is-identical`, `ww-panel-run-harness`,
  `ww-test-harness-add`, `nifskope-ww-panel-style`, `nifskope-ww-render-shot` (camera pin, absolute paths, WW_RENDER_SIZE floor).

## bungo's words, verbatim (2026-09-12)
- 15:53: terrain identity vertex colours on the .BTR are NOT used -- "We don't bake BTR for FO4CS, and so we do not use of that data for it at all."
- 15:56: "Legacy terrain bakes stay as they were, no extra data for FO4CS to be included in them. Only the .lod ones have new data in them."
- 16:03 (on `a_land_guide_*.png`, panel (c)): "C looks good, it's good if it's configurable in the UI" -- panel (c) = `--land-guide flatwarp:1.0
  --land-warp 341` on the hex tiling; that is the default, NOT LAND1's measured winner aspecthex.
- 16:55 (the verge planes beside the kerb, `materials/Landscape/Ground/CommonwealthDefault01.bgsm` shapes in the road NIFs): "the grass meshes
  included with the road nifs" are excluded from the road plane -> `--road-ground-paint` default 1.0 -> 0. `--road-opacity` stays 1.0, no row.
- Standing: `--road-detail` stays 1; erosion default stays 0 (a knob, not ruled); `--land-sample average` REJECTED ("lost all the detail, plain
  blobs"); footprint sampling stays.
This lane is THE chartered exception to "lanes never change defaults": it carries exactly these rulings and no other default moves.

## The work
1. **Defaults flip, in every place a default lives** (CLI variable, option-struct field, generator static, panel row's initial/QSettings default,
   help text that says DEFAULT): (a) `lgIdentity` and `LodgenObjectOptions::identity` -> false; `--identity` becomes the opt-in (add it if
   missing, keep `--no-identity`); (b) `lgTerrainIdentity` and `LodgenTerrainOptions::terrainIdentity` -> false; `--terrain-identity` opt-in;
   (c) the land default = the exact argv behind LAND1's panel (c) -- read it from LAND1's report, do not reconstruct it -- so a bake with NO
   land switches produces what that panel showed (guide flatwarp strength 1.0, warp amplitude 341, and the hex tiling if that argv carried it:
   say in the report which switches the picture used and which statics you moved; the OLD behaviour must stay reachable by explicit switches,
   name them, e.g. `--land-guide off --land-warp 0 --land-hex 0`); (d) `roadGroundPaint` 1.0 -> 0 (`--road-ground-paint 1` is the way back).
2. **Decouple the sidecars and the far-ring inputs from the identity flag.** With identity OFF the `.bto.manifest.txt` sidecar, the texture
   arrays (`--arrays`) and the impostor cards must still be written and used (SHOWCASE1's finding: `--no-identity` at lodgen.cpp:3760 gates the
   WHOLE manifest and so kills the card arrays; PANEL's `wantIdentity() && arraysCheck` gates at 2585/3077/3155 do the same). Assumption for
   the manifest (bungo's OPEN call, not ruled): keep it as a SIDECAR -- it is not inside the .BTO, so the 15:56 ruling is not touched; say so
   in §4 as his call. Per-vertex data that IS identity (colours R+G index, B AO, A sway, UV2) stays off with the flag.
3. **The gate that matters** (`tests/spells/lodgen_defaults.sh`, new, with floors on the other side):
   (a) the default bake of (-20,24) dim 4 (terrain + objects, `--road-detail 1`, nothing else) on the NEW exe is BYTE-IDENTICAL, file for file,
   to the RUNG's bake with the explicit switches `--no-identity --no-terrain-identity --road-ground-paint 0` + the land switches of panel (c)
   (nothing moved but the defaults); (b) the NEW exe with the old switches spelled out (`--identity --terrain-identity --road-ground-paint 1`
   + the old land switches) is BYTE-IDENTICAL to the rung's default bake (the way back exists); (c) every LEGACY file of the default bake --
   .BTR and .BTO at dim 4, 8, 16 and 32 for a region containing Sanctuary -- carries VANILLA's vertex descriptor `52776558133763` and no colour/
   UV2 stream (read the desc with the reader the tree already has, `lodgen --dump-geometry` or the BTO parser in the spells; refuter = the same
   read on the rung's default bake shows the identity desc); (d) far rings are NOT empty: dim 16 with `--impostors <SHOWCASE1's card dir>` (or
   `--slot-fallback`) places the same count with identity off as the rung did with identity on; manifest row count and `A`/`M` lines present;
   (e) the NATIVE outputs still carry their data: `--native` .lodo/.lodi and the `.lodt`/`.lodl` of the same region byte-identical rung vs new
   under equal explicit switches, and `--native-verify` clean (the FO4CS data lives only there and must not thin out with the flag).
4. **Harness re-base**: every harness in the read list that asserted the OLD defaults is re-based by spelling the switch it tests (a harness
   about identity passes `--identity`; it does not silently start passing by the default flip); record each harness's count before and after
   next to the exe timestamp; `lodgen_byte_gate.sh` (PANEL1) rerun: panel defaults vs CLI defaults byte-identical on the new exe (the harness
   FORCES its QSettings state -- PANEL1's lesson). `WW_LODGEN_TEST` round-trips the moved row defaults.
5. **Panel**: the rows follow the defaults (identity boxes unticked, land-guide selector on flatwarp with strength 1.0 and warp 341, road ground
   paint 0); no new rows, no removed rows, house style untouched. Picture: `WW_LODGEN_SHOT_FULL` of the panel after, next to the rung's.
6. **Pictures** (`scratchpad/defaults1_20260912/images/`, label burned in, sizes read back with PIL, `WW_RENDER_CLEAN=1`, textured roads):
   (i) the (-20,24) chunk lit, top view (`WW_RENDER_CENTER=(2*-20+4)*2048,(2*24+4)*2048,0`, `WW_RENDER_ORTHO=8192`, `WW_RENDER_VIEW=1`),
   rung default bake beside new default bake, .BTR + .BTO composited or the native `.lodl`+`.lodi` route (`WW_LODL_OBJECTS`, see
   `docs/LODGEN_NATIVE_LODO_LODI.md` `## Viewer`) -- say which; the resource root shaped `<root>/Textures/Terrain/Commonwealth/...`;
   (ii) a crop at a kerb where the verge plane used to paint (road ground paint 1 vs 0), same camera; (iii) OWED SEPARATELY, do it last and
   only if the switch exists (read `scratchpad/lane_tiling2_report.md`): the blend-edges-quadrant softening ALONE on footprint sampling for
   the same chunk, top view, beside the default -- TILING2 only ever showed it bundled with `average`; if there is no switch that isolates
   it, say so in one line and stop. Send nothing yourself; the director sends.
7. **Build**: the chain from `nifskope-ww-build-verify` (game check as its own command, rename aside, `make -j2` gated on make's rc, exe newer
   than EVERY changed file via `git status --porcelain -- src res tools tests`, stale-object check for every header touched, `make -n` zero
   compile lines). Counted relinks. Read every count next to the exe timestamp.
8. **Documents in `scratchpad/defaults1_20260912/`**: `WW_CHANGES_ENTRY.md` (starts `## 2026-09-12 — <title>`), `HANDOFF_BLOCK.md` (10-20
   lines, plain language, opening with a line that starts `- DEFAULTS1 (2026-09-12, ...)`), `CHANGED_FILES.txt` (A/M + path, CR/LF byte counts
   before and after), `MISTAKES_ENTRIES.md` -- AND you put those same entries at the TOP of root `MISTAKES.md` yourself the moment each is
   recognised (the director does NOT splice MISTAKES any more; say in DONE that they are already in the root file). Report sections:
   `## 0. Where each default lived` (table: default | old | new | file:line for CLI, struct, static, panel, help text), `## 1. Gates` (table,
   numbers, refuters), `## 2. Harness re-base` (harness | before | after | what was spelled), `## 3. Build and chain`, `## 4. Pictures`,
   `## 5. Owed / red / bungo's calls`, `## 6. Mistakes`, `## 7. Skill review` (amend `nifskope-ww-lodgen`'s default statements -- it says
   identity is ON and describes the identity profile as the default -- and list the file). Docs: `docs/LODGEN_TERRAIN_VT.md` and
   `docs/LODGEN_IMPOSTOR_SPEC.md` wherever they state the old default; the WW_CHANGES LAND1/GROUND1/ROADS4/IDENTITY blocks are history and stay.

## Rules
No default moves beyond the four ruled here; no writer format change; no tone or renderer change; erosion, road opacity, road detail, sheet
format untouched. `lodgen.cpp` behaviour changes are limited to the decoupling in item 2. Plain language; numbers beside floors; no
"final/true" claims (bungo confirms by eye); incremental writes; two-lane cap (you are the only lane); if context runs short, PENDING.md
with exact resume steps and a DONE-so-far list.
