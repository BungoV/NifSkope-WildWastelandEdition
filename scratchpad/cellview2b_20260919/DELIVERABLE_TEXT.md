# CELLVIEW2B -- text for the overseer to splice

Two blocks. The first goes at the TOP of `WW_CHANGES.md` (that file is MIXED;
the 2026-09 entries at the top are LF-only -- match the neighbours, do not
normalise). The second goes into `HANDOFF.md` as bullets. This lane edited
neither file, as briefed.

---

## BLOCK 1 -- for WW_CHANGES.md, at the top

## Click a reference in the cell view; the ground is the cell's real paint (2026-09-19, lanes CELLVIEW2 + CELLVIEW2B)

Clicking in a `.wwcell` scene now picks the reference under the cursor and fills a **Reference** dock with flat Name|Value rows (reference and base form ids, editor id, record type, model, position, rotation, scale, triangle count), with a highlight box on the picked placement and a one-line summary of how many boxes were under the cursor. The master is `Render > Cell Pick Panel`, **checkable and unticked**: while it is off the click falls through to the ordinary block selection and nothing in the viewport changes. (`View` is not in this fork's menubar, hence Render.)

The cell's ground is no longer a flat sheet. The LAND record's quadrant `BTXT` base textures and its `ATXT` alpha layers are read and each 1/32 cell quad takes the strongest layer at its own SW corner. Sanctuary -20,7: **1024 quads over 6 landscape textures, 891 textured, 133 bare, 586 quads taking an ATXT layer over the quadrant's BTXT, out of 16 layers read**; downtown 5,-11: **1024 over 7, 1024 textured, 0 bare, 561 from a layer, 19 layers**. The layers are **NOT blended** -- this is a hard-edged mosaic of the real textures, not the engine's composite, and the census line says so in those words. A `.lodi` bake can be loaded (`WW_CELL_LODI`) and the identity-group overlay (`WW_CELL_OVERLAY=identity`) then tints each placement by the group the bake put it in, with a counted legend; with no bake it REFUSES by name and draws one grey.

New files `src/cellclick.*`, `cellpanel.*`, `cellpicktest.*`, `cellidentity.*`, `cellground.*`; new gate `tests/spells/cell_pick.sh`. Gate on the built exe, with a Sanctuary `.lodi` baked for it: **13 rows, 12 PASS / 1 FAIL / 0 SKIP**; the in-window pick self-test is **16 rows, 0 failures** including its three red controls (a ray pointed away reports nothing and collapses the highlight; the dock says "Nothing under the cursor"; with the master unticked the click is not taken) and the gate's own red (with no cell scene open the pick report FAILS, 2 failures). Identity: **216 groups in the bake, 20 colours drawn**; red control with no bake, 1 colour and a refusal by name.

**THE ONE RED IS THIS LANE'S AND IS NOT CURED**: row 7, "most quads resolved a texture", floor `>= 1024`, measured **891**. Two separate facts. (a) The 133 bare quads are real and visible as flat grey squares in the picture: their quadrant carries no `BTXT`, no `ATXT` layer clears `CELL_GROUND_LAYER_MIN` at their corner, and `src/cellground.cpp` has no worldspace-default landscape texture to fall back on. (b) The row cannot pass anyway: a cell's ground is exactly 32x32 = 1024 quads, so a row named "most" has its floor set to its whole population. Both are gate/design work for a follow-up; nothing was landed for either (resume skill rule 6).

Magenta materials, half repaired. `src/lodgen.cpp`'s two callers of `lodgenReadAsset` prepended `materials/` to paths that may already be absolute Bethesda build paths, giving `materials/c:/_bethesda/.../materials/x.bgsm`, an empty diffuse and the missing-texture magenta. Both now carry the cut that `lodgenCollectMaterials` (~1826) already had: **4 prepends / 4 cuts** in the file, against **2 / 0** on `git HEAD` (the gate's red control). **The cars at 5,-11 are still magenta on the same panels in the before and after pictures**, and the cause is measured and different: those panels are car GLASS, whose material is a `.bgem`, and `lodgenLoadModel` reads a material only when it ends in `.bgsm` (`src/lodgen.cpp` ~2210) -- an effect material's textures are never read at all, so the diffuse stays empty. Not repaired here, named for a follow-up.

Controls, all equal to their baselines on this exe: `cell_open.sh` **8/0** and its red PASS, `render_shot.sh` **82/0**, `harness_window.sh` **15/0**, `native_open.sh` **17/0**, `impostor_draw.sh` **24/0**, `lodgen_octahedral.sh` **116 ok / PASS**, `gltf_gates.sh` **0 gates not as registered**, `gltf_export_options.sh` **0 rows not as registered (66 PASS)**, `body_build.sh` **31/0**. Exe 2026-09-19 18:14:53, 23,619,072 B, sha1 ef4dab1f.... Rung `release/NifSkope.before_cellview2.exe` 16:48:03, 23,505,408 B, sha1 072d78f8....

---

## BLOCK 2 -- for HANDOFF.md

- CELLVIEW2B LANDED 2026-09-19 18:14:53 (the BUILD PENDING resume of CELLVIEW2). `hookup_cellview2.py --check` 20 edits
  over 5 files matching once, `--apply`, `qmake` then `make`: **QMAKE-RC=0, BUILD-RC=0, zero compile errors, no repair
  needed in the lane's new files**; 11 objects, all five new `.o` plus `moc_cellclick.o` / `moc_cellpanel.o` in the link
  script; exe-newer sweep over every changed path stale-count=0; stylesheet in step. Exe **23,619,072 B, 18:14:53,
  sha1 ef4dab1f9c8a819e8692e4bd40e430d96e7329b8**, first bytes MZ. **His open window needs a RESTART.**
- A SECOND build at 18:14:53 rebuilt exactly ONE translation unit, `cellpicktest.o` (`grep -oE "-o GeneratedFiles/..."`
  on the log), for a one-line change in the lane's own new file: the self-test's last two rows are red controls that end
  on a MISS, so the window a picture is taken of showed an empty dock; it now re-issues the honest click at the end,
  asserts nothing and adds no row. `cell_pick.sh` was re-run on that final exe (16 self-test rows, 0 failures, unchanged).
  The other nine gates were measured on the 17:49:14 exe, which differs from the final one only in that object.
- **RED, this lane's, NOT cured** (resume skill rule 6 -- measured and reported, not landed): `cell_pick.sh` row 7,
  891 of 1024 quads textured against a floor of 1024. 133 quads have no BTXT in their quadrant and no ATXT layer above
  `CELL_GROUND_LAYER_MIN` at their corner, and `cellground.cpp` has no worldspace-default fallback; separately the floor
  equals the row's own whole population (32x32), so "most quads" can only pass at 100 per cent. Both are in MISTAKES.md.
- **The magenta is only half this lane's.** 4 prepends / 4 cuts in `lodgen.cpp` (HEAD: 2 / 0), gate row green and its
  red control red -- but the downtown cars are magenta on the same panels BEFORE and AFTER. Measured cause: those are
  glass shapes with `.bgem` effect materials, and `lodgenLoadModel` reads a material only when it ends `.bgsm`
  (`src/lodgen.cpp` ~2210). An effect material's textures are never read, the diffuse stays empty, and empty diffuse is
  the missing-texture magenta. **Owed: a lane that teaches that loader `.bgem`.**
- **A `.lodi` had to be BAKED for Sanctuary**: not one in the tree covers cell -20,7 at v7 or above (the header's W/S/E/N
  at 0x48 are in CHUNKS, `H_CELLS` 0x50 = 4 cells a chunk; every Commonwealth bake in `scratchpad/horizon*` sits at
  chunks 0..2 / -3, the one that does cover it is v3 and is refused by version). The bake is
  `scratchpad/cellview2b_20260919/lodibake/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi`, **130,666 B, v7, 778 instances,
  216 groups, 107 singletons, biggest 297, 351 tree instances of which 96 alone in their group**, from
  `--terrain-region -20 4 -17 7 --dim 4 --road-detail 1` with shipped defaults (so: the ruled proximity join), 22 s,
  rc=0. 4 bases of 2974 have no loadable model (four `WrhsLeanTo*_LOD.nif`).
- **Found while looking at the identity picture, not repaired**: the notes' legend prints `overlayColour(key)` for EVERY
  bucket including the `unknown` sentinel `0xffffffffff` (`src/cellview.cpp` ~1193), so that row reads `rgb 0.60,0.21,0.37`
  while the viewport actually draws the hardcoded flat grey 0.35. At Sanctuary -20,7 that bucket is **154 of 240 drawn
  placements** -- most of the cell is references the bake never saw (no LOD model), which is a stated fact, not a gap,
  but the legend names the wrong colour for it.
- Pictures for bungo, `scratchpad/cellview2_20260919/images/`: `before_sanctuary.png` / `after_sanctuary.png` (flat grey
  sheet -> the real paint, with the 133 bare quads as grey squares), `after_sanctuary_identity.png` (+ its legend in
  `after_sanctuary_identity.stdout`), `pick_reference.png` (the Reference dock's rows; the GL viewport is black in it --
  `QWidget::grab` cannot photograph an OpenGL widget -- so the highlight is in `pick.png` instead, the mustard stripe
  being the box on the picked highway ramp), `before_downtown.png` / `after_downtown.png` (the magenta pair).
- CELLVIEW1's handoff bullet "WW_RENDER_VIEW cannot select ViewTop" is FALSE: `src/glview.h:452` has ViewDefault=0 and
  **ViewTop=1**, so `WW_RENDER_VIEW=1` is ViewTop and every picture this round is top-down. MISTAKES entry written.
- Nothing was committed, nothing stashed, `WW_CHANGES.md` and `HANDOFF.md` untouched by the lane. `tasklist` guard read
  rc=1 (Fallout4 and NifSkope down) before every build and every exe run: 17:44, 17:46, 17:49, 17:51, 17:54, 18:11,
  18:14, 18:15, 18:18, 18:20.
