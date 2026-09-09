#!/usr/bin/env python3
# BUILD2: append the build/gate section to both lane reports. Both are LF-only.

import sys

BASE = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/"

COMMON_TABLE = """
| gate | result | numbers |
|---|---|---|
| `render_shot.sh` | **FAIL** | 28 checks, 6 failures. Sections 1-4 green; all six are section 5: each of `render_plain`, `bake_plain`, `bake_lod` recorded `2-3 of 4-5` window records `onscreen=1` and the outside sampler saw the window on `\\\\.\\DISPLAY5`. Section 6's floors both fired and its identity check read `off fedab869884174fb / on fedab869884174fb` -- which proves nothing here, because both runs were on a screen |
| `lodl_write.sh` | PASS | 34 checks, 0 failures. `magic is still LODT after the .lodl rename`; heights round-trip exactly (36,864 samples, 0 mismatched); both refusal directions name the other format; the renamed file `--verify-only` at 0 mismatches |
| `lodl_open.sh` | PASS | **23 checks, 0 failures**, run against bungo's own renamed `Commonwealth.lodl`; whole-worldspace render coverage 0.0959, luminance SD 39.05 |
| `lodgen_terrain.sh` | PASS | 26 checks, 0 failures |
| `lodgen_identity.sh` | PASS | 8 checks, 0 failures; 406 objects shared between the rings, all with the same base and position |
| `lodgen_terrain_vt.sh` (the sheets as `.lodt`) | **FAIL** | 31 checks, 1 failure: V9a, the assembled colour sheet vs a direct bake -- same size (174,888 B both), different bytes. Not a naming or parse failure and not attributed to either lane: it is the `dominantBase` scoping question `scratchpad/specs_20260906/spec_terrain_vt.md` V9a was written for. NOT measured further |
| `lod_generation.sh` | PASS | **97 checks, 0 failures** -- the one expected red, *"with one, the panel says what it will write"*, is green now that the `src/nifskope_ui.cpp` line is in |
| `btd_terrain.sh` | PASS | 13 checks, 0 failures |
| `lodgen_impostor_cards.sh` | PASS | 10 checks, 0 failures (`RESULT PASS`); candidate `0004a074`, the chunk shrinks 1,280,239 -> 1,001,308 B when cards replace the meshes |

**Skipped, and why.** `lodl_btd.sh` and the Fallout 76 `.btd` route (a ~25-minute
conversion; the same reader is already exercised by `btd_terrain.sh` and by
`lodl_write.sh`'s round trip). `lodgen_octahedral.sh` (two real GUI bakes, ~3
min): `lodgen_impostor_cards.sh` covers the same candidate filter change and is
the harness the brief named first. `lodgen_texture_arrays.sh`,
`lodgen_card_arrays.sh`, `lodgen_merge.sh`, `lodgen_farring.sh`,
`lodgen_resources.sh`, `lod_channel_preview.sh` and every non-LOD harness: no
file either lane touched is on their path. The one real tree bake of
`offscreen_20260909/PENDING.md` step 6 was NOT run -- it is the 580-repaint run,
and section 5 says the window is still on a monitor, so running it would have
put the hazard on bungo's screen for no new information.

**mtimes, one table (CONSTITUTION 4).**

| artefact | time |
|---|---|
| `release/NifSkope.exe` (final) | 2026-09-09 18:43:13 |
| `release/style.qss` | 2026-09-09 18:43:14 (`cmp res/style.qss release/style.qss` equal) |
| every changed source | older than the exe; checked file by file over `git status` |
| bungo's five installed files | 2026-09-09 17:27 (content untouched, name only) |
| the gate logs | `scratchpad/build2_20260909/logs/`, 18:45-18:47 |

**`release/NifSkope.before.exe` was NOT deleted.** The brief conditions that on
the off-screen gate passing with its same-build comparison, and it did not, so
the before-picture is still the only picture of the pre-fix renderer. It sits in
`release/` (17,796,608 B, sha256 `5ca38e28...`).

**Nothing was committed** (CONSTITUTION 8).
"""

OFFSCREEN = """
## Build (BUILD2), 2026-09-09

`qmake NifSkope.pro` RC=0, then `make -j2` RC=0. `release/NifSkope.exe`
**18:43:13**, newer than every changed source; `release/style.qss` 18:43:14 and
byte-equal to `res/style.qss`.

**THE FIX IS INERT. bungo's strobe is exactly as he reported it.** Measured on
the first build that carried it (18:29:24) and again on the final one:
`tests/spells/render_shot.sh` reads **28 checks, 6 failures**, and the six are
this lane's sections 5.

**Why**, from this lane's own instrument. `release/ww_render_shot/
render_plain.winlog` reads `shown QWidgetWindow geom=0,23,1920x1017 visible=1
onscreen=1` where `wwOffscreenWindowOrigin()` had been asked for, and the
`WW_WINDOW_VISIBLE=1` control reads `1920,-42` where `move(1960,40)` had been
asked for -- the maximised client origin of the monitor that contains that
point, not the point. `restoreUi()`, two lines above the new branch, restores
the window state the person last left, which is MAXIMISED, and on Windows
`move()` on a maximised window changes nothing except which monitor it is
maximised onto. An off-screen origin is on no monitor, so it did nothing at all.

**And the off-screen window does not render.** One line clearing the maximised
bit before the move was written, built (18:38:46), measured and then REVERTED:
with it, all three runs read `0 of 4` records `onscreen=1` -- the window really
did leave every screen -- and `WW_RENDER_SHOT` then wrote **no PNG at all**,
while `WW_IMPOSTOR_BAKE` wrote its sidecar (`cube_plain.txt`, 110 B) and **no
card image**, both exiting 0 in 3-5 s. A window entirely outside every screen is
never exposed, so `QOpenGLWidget` never creates its context and
`grabFramebuffer()` returns a null image. An off-screen bake would have written
EMPTY card sets while reporting success, and the driver caches by form id. That
is a worse failure than the hazard, so the tree stands as this lane left it and
the cure is the director's call. The two candidates, neither measured, are in
`WW_CHANGES.md`: the FBO path this lane already named
(`GLView::grabSupersampled`), or leaving the window on a screen at opacity 0.

**One more thing the gate has to grow.** Section 6's identity check passed in
the same run in which six checks said the window was on a screen -- it had
compared a picture with itself. It must first require the off-screen run to have
recorded zero `onscreen=1`.

**A second, smaller finding.** With the window genuinely off-screen, `bake_lod`
still produced ONE on-screen sample from the outside watcher, a 426x268 window
at `2552,184` -- a transient dialog, not the main window, and the inside
instrument never saw it because it logs at fixed points. Off-screen placement of
the main window alone does not empty the screen.
""" + COMMON_TABLE

RENAME = """
## Build (BUILD2), 2026-09-09

**`qmake` FIRST, as the resume asked**: `qmake NifSkope.pro` RC=0, then
`make -j2` RC=0, `release/NifSkope.exe` **18:43:13**, newer than every changed
source, `release/style.qss` in step. The three dependencies the resume named all
survive the regenerated `Makefile.Release`: `src/lodtfile.h` appears 7 times;
`GeneratedFiles/.obj/lodtfile.o` lists `io/lodvfile.h`;
`GeneratedFiles/.obj/lodvfile.o` lists `lodtfile.h`; and
`GeneratedFiles/.obj/btdterrain.o` lists `lodtfile.h` on its own now, so lane
BUILD1's hand patch is no longer standing in for anything. Every object that
includes `lodtfile.h` is newer than it.

**The GUI half was applied by this lane**, exactly as
`scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md` wrote it: five one-line
changes plus three comment mentions in `src/nifskope.cpp` (all CRLF lines,
replaced at equal byte length, CR 9,370 and LF 10,493 both unchanged) and the
one self-test line in `src/nifskope_ui.cpp` (LF-only, unchanged counts). The
C++ names (`LodtWorldInfo`, `lodtQueryRegion`, `lodtPendingRegion`), the panel
objectNames and `LodGeneration/lodt` were left alone. **Two strings in
`src/nifskope.cpp` were NOT changed because the note did not list them** and are
owed to whoever owns that file: the log lines `qInfo() << "lodt terrain:"` and
`qWarning() << "lodt terrain:"` at ~10,096 and ~10,098 still say `lodt` about
the landscape route.

**bungo's installed set, renamed and read back one at a time.**

| file | bytes | `lodl ... --info` | `--verify-only` vs its plugin |
|---|---|---|---|
| `Commonwealth.lodl` | 35,953,294 | rc=0, cells [-96,-96]..[95,95], 48,960 blocks | **0 mismatched** of 36,864 samples; alpha 0 of 36,864, colour 0 of 36,864 |
| `DLC03FarHarbor.lodl` | 9,195,933 | rc=0, 26,286 blocks | **0 mismatched** of 5,568 |
| `DiamondCity.lodl` | 53,148 | rc=0, 226 blocks | **0 mismatched** of 256 |
| `NukaWorld.lodl` | 7,182,356 | rc=0, 5,684 blocks | **0 mismatched** of 69,696 |
| `NukaWorldAmphitheater.lodl` | 38,303 | rc=0, 154 blocks | **0 mismatched** of 128 |

All five in `E:\\Projects\\Fallout 4 Mods\\mods\\FO4CS\\Terrain\\`, mtime
unchanged at 2026-09-09 17:27 (the rename moved the name, not the bytes). The
five `*.lodt.bak-20260909` copies were left exactly as they were, name included.

**`--candidates trees` measured** on Fallout4.esm, terrain region
(-20,24)..(-17,27): **19 candidates, and every one of them is under
`Landscape\\Trees\\`** -- three `TreeMapleForest`, seven `TreeMapleblasted` /
`TreeBlasted`, two `TreeElmForest`, two `BlastedForestBurntTreeUpright`,
`TreeHero01`, `TreeBlasted01Lichen`. The default filter returns **33** over the
same region, so the 14 shacks and rock cliffs of the bug report are gone.
""" + COMMON_TABLE

for rel, text in (("lane_offscreen_report.md", OFFSCREEN),
                  ("lane_rename_report.md", RENAME)):
    path = BASE + rel
    with open(path, "rb") as fh:
        b = fh.read()
    if b.count(b"\r"):
        print("ABORT: %s has CR" % rel)
        sys.exit(2)
    if b.endswith(b"\n"):
        b = b + text.encode("utf-8")
    else:
        b = b + b"\n" + text.encode("utf-8")
    with open(path, "wb") as fh:
        fh.write(b)
    print("OK %s: %d bytes, CR %d" % (rel, len(b), b.count(b"\r")))
