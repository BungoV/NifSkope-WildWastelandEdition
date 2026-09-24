# CELLVIEW2 -- BUILD PENDING (director clock 2026-09-19 17:xx)

Lane CELLVIEW2 was CODE-ONLY: no build, no exe run, no exe-starting spell, and
no existing `src/` file edited in place. Ten new files, one refusing anchored
hook-up, one new gate. **Nothing in the existing tree has been changed yet** --
`hookup_cellview2.py` has only ever been run with `--check`, which writes
nothing.

## What is on disk

| new file | what it is |
|---|---|
| `src/cellclick.h` / `.cpp` | (1) the click glue + the highlight shape. Master OFF. |
| `src/cellpanel.h` / `.cpp` | (1) `CellPickPanel`, the flat Name\|Value dock. |
| `src/cellpicktest.h` / `.cpp` | (1) the in-window self-test, `WW_CELLPICK_TEST=<report>`. |
| `src/cellidentity.h` / `.cpp` | (2) the `.lodi` identity-group index + census. |
| `src/cellground.h` / `.cpp` | (3) the LAND paint, pure data, no NifModel/Qt/GL. |
| `tests/spells/cell_pick.sh` | the gate. Every row NOT RUN; see below. |
| `scratchpad/cellview2_20260919/hookup_cellview2.py` | the ONE hook-up. |
| `scratchpad/cellview2_20260919/sx_CELLVIEW2.sh` | the syntax pass (flags read out of `Makefile.Release`). |

`--check`, last run: **20 edits over 5 files, every anchor matches exactly
once.** Line endings measured and asserted per file: `src/glview.cpp` is CRLF
(23905 CRLF, 81 bare LF) and the edits carry +21 CR; `NifSkope.pro`,
`src/cellview.cpp`, `src/nifskope_ui.cpp`, `src/lodgen.cpp` are LF-only and
carry 0. Predicted sizes after `--apply`:

```
NifSkope.pro        21553 -> 21767   (+214)
src/cellview.cpp    39668 -> 46283   (+6615)   marker x14
src/glview.cpp     891780 -> 892893  (+1113)   marker x2
src/nifskope_ui.cpp 1606934 -> 1609587 (+2653) marker x4
src/lodgen.cpp     631168 -> 632418  (+1250)   marker x1
```

The MARKER is the string `lane CELLVIEW2`, which appears in 0 of those files
today and in none of the anchors. **Decide "applied or not" from the marker
count and the byte size, never from a `--check` that still says OK** -- an
`after` anchor still matches after its text was inserted.

`g++ -fsyntax-only` with the real Release flags: **10 of 10 new files clean, 0
warnings** (`bash scratchpad/cellview2_20260919/sx_CELLVIEW2.sh`, rc=0).

## The director's commands, in this order

```powershell
# 0. the process guard, as its own command, before anything that builds or runs
tasklist | findstr /I "Fallout4 NifSkope"
```
If `Fallout4.exe` is up, stop here and stay at BUILD PENDING.

```bash
# 1. apply the hook-up (it refuses unless all 20 anchors still match once)
cd /e/Projects/NifskopeWildWastelandEdition
python scratchpad/cellview2_20260919/hookup_cellview2.py            # --check, writes nothing
python scratchpad/cellview2_20260919/hookup_cellview2.py --apply

# 2. QMAKE FIRST, then make. Not optional: five headers join HEADERS, and
#    cellclick.h and cellpanel.h carry Q_OBJECT, so moc must be regenerated.
#    An incremental make without qmake compiles and then fails to link on
#    CellPickBus::picked / CellPickPanel's vtable.
qmake NifSkope.pro
make -j8 release
```

```bash
# 3. the gate (needs the new exe; every row below is NOT RUN as of this note)
bash tests/spells/cell_pick.sh
LODI=<a .lodi bake for Commonwealth> bash tests/spells/cell_pick.sh   # rows 4-5

# 4. the neighbours this change reaches, and only those:
bash tests/spells/cell_open.sh          # cellview.cpp: the ground loop moved
bash tests/spells/cell_open.sh --red    # its transform control
bash tests/spells/render_shot.sh        # glview.cpp: the click site
bash tests/spells/native_open.sh        # lodgen.cpp: the two material paths
```
`native_open.sh`'s failure `the .lodi scene draws everything the .BTO draws
(covered 0.8978 >= 0.90)` is the KNOWN red the coordinator pre-declared on
2026-09-19; it is not this lane's.

## The gate rows -- ALL NOT RUN

`tests/spells/cell_pick.sh`, written against an exe that does not exist yet.
Row 0 refuses for exactly that reason, so a green log cannot be mistaken for a
measured one.

| # | row | red control |
|---|---|---|
| 0 | the exe is newer than all 14 sources this gate covers | NOT RUN |
| 1 | the pick self-test ran and wrote >= 10 rows | NOT RUN |
| 2 | every pick row passed | NOT RUN |
| 3 | **RED** with no cell scene open the pick report FAILS | NOT RUN |
| 4 | the bake's group census reaches the notes (>= 2 groups) | NOT RUN |
| 5 | the identity overlay draws >= 2 distinct colours | NOT RUN |
| 5r | **RED** with no bake: REFUSED by name, <= 1 colour | NOT RUN |
| 6 | the ground uses >= 2 landscape textures | NOT RUN |
| 7 | >= 1024 quads resolved a texture | NOT RUN |
| 8 | the ATXT layers are read, not only the quadrant BTXTs | NOT RUN |
| 8r | **RED** `WW_CELL_NOTERRAIN=1` removes the mosaic line entirely | NOT RUN |
| 9 | every `materials/` prepend in lodgen.cpp is paired with the cut | NOT RUN |
| 9r | **RED** the same expression is FALSE on `git HEAD` | NOT RUN |

Rows 9 and 9r are SOURCE rows and were dry-run today, which is the one thing
here that has a number: working tree **4 prepends / 2 cuts (row 9 FAILS, the
defect is present)**, `git HEAD` **2 prepends / 0 cuts (row 9r PASSES, the
search can see the defect)**, and after `--apply` **4 prepends / 4 cuts (row 9
passes)**. Rows 1-8 need the exe.

Inside the pick self-test (`src/cellpicktest.cpp`) there are two further red
controls, which is why row 2 is one line here: a ray pointed away must report
no pick, say "Nothing under the cursor" in the dock and collapse the highlight;
and with the master row unticked the click must not be taken at all.

## The masters, and what ships off

* **`Render > Cell Pick Panel`** -- checkable, **unticked**, the dock hidden.
  While it is off `cellPickClick()` returns false and the viewport's click
  falls through to the ordinary block selection, exactly as before this lane.
  (`View` is removed from this fork's menubar at `src/nifskope_ui.cpp` ~25782,
  so the row is in Render.)
* The colour overlays INSIDE the cell view -- identity included -- are **not**
  masters and get no row: they are one view's display setting, chosen per
  scene by `WW_CELL_OVERLAY` / the `.wwcell` line.
* The painted ground is not a feature switch either; it replaces what the same
  `spec.terrain` flag already drew, and the old vertex-colour sheet remains as
  the fallback when the mosaic refuses, so there is never no ground.

## The magenta, diagnosed

Not `lodgenReadAsset` -- its CALLERS. Two sites, the same three-line shape:

* `src/lodgen.cpp:2212-2213`, in `lodgenLoadModel` (the cell view's own model
  loader).
* `src/lodgen.cpp:4758-4759`, in `lodgenLoadTexture` (the landscape-texture
  loader, which the new painted ground now goes through as well).

Both prepend `materials/` to a path that may ALREADY be an absolute Bethesda
build path, producing `materials/c:/_bethesda/.../materials/x.bgsm`. That
resolves to nothing, the diffuse slot is left EMPTY, and an empty diffuse is
not neutral: it binds the missing-texture magenta under `Scene::DoErrorColor`
(`src/gl/renderer.cpp` ~951). The correct shape already exists in the same file
at `lodgenCollectMaterials` (~1826): cut everything before the LAST
`materials/`, and only prepend when there is none. Both repairs are in the
hook-up (edits 19 and 20) because they are four lines each.

Whether a particular street stops being pink is bungo's eye on a picture; row 9
only proves the broken shape is gone from the file.

## What is still owed, and is NOT in here

* **XCRI / precombined** stays out of scope, as briefed. The `Precombined`
  overlay keeps its refusal line.
* **Texture BLENDING.** The ground is a hard-edged mosaic of the strongest
  layer per quad, not the engine's composite; `cellground.h` says so in those
  words and the census line says it in the notes. Blending is the splat
  compositor the terrain bake owns (`docs/LODGEN_TERRAIN_VT.md`).
* **`T = 341.3333`** (twelve repeats a cell) is `fLandTextureTilingMult` and is
  NOT in the data; it is a constant in `cellground.h` with the same default the
  bake uses, overridable by the caller.
* **A `.lodi` bake for Commonwealth.** The only `.lodi` in the tree is another
  lane's `scratchpad/lodgen_scrappable_gate/flipped.lodi`, which is a
  deliberately corrupted fixture for a different gate. Rows 4-5 SKIP BY NAME
  until someone passes `LODI=`.
* **A picture.** No image was made: this lane never ran the exe.
