# lane HOOKCAM — BUILD PENDING

**Why.** The build gate in this lane's brief was checked once, at the end, and
both halves were red:

```
$ ls scratchpad/cardfinal_20260909/DONE
ls: cannot access 'scratchpad/cardfinal_20260909/DONE': No such file or directory
$ tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
NifSkope.exe   16364 Console   1   145,484 K
rc=0
```

and that process is another lane's card bake, not a stray of this one:

```
ProcessId    : 16364
CreationDate : 9/9/2026 11:52:26 PM
CommandLine  : ...\release\NifSkope.exe
               "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeBlasted04.nif"
               --port 45917
```

So: nothing was built, nothing was run on a new exe, and **every number in
`scratchpad/lane_hookcam_report.md` sections 1 and 5 was measured on the exe
already on disk** (`release/NifSkope.exe` 2026-09-09 22:04:35). Sections 3 and 4
are the ones this resume produces.

## What is on disk, uncompiled

| file | change | line endings |
|---|---|---|
| `src/glview.h` | `GLView::WwCameraPin` + 7 members/methods, and the `wwPin` member beside `doCenter` | LF-only, 0 CR (unchanged) |
| `src/glview.cpp` | the pin's implementation, and its re-assertion in `paintGL` after the `doCenter` block | CRLF; CR 22,753 → 23,000, and CRLF rose by the same 247, so every added line is CRLF |
| `src/nifskope_ui.cpp` | **NOT this lane's file** — 3 small edits, see below | LF-only, 0 CR (unchanged) |
| `tests/spells/render_shot.sh` | section 7, the camera gate | LF-only |
| `MISTAKES.md` | three entries at the top | LF-only |
| `scratchpad/hookcam_20260909/transition.py` | step 3, written and compile-checked, never run | LF |

**`src/nifskope_ui.cpp` is outside the file list this lane was given, and it had
to change because the render hook lives there and not in `src/nifskope.cpp`.**
The edits were kept as small as possible so a merge with lane CARDFINAL (which
is baking cards, and may be in the impostor-bake block of the same file) is a
three-hunk conflict at worst:

1. the `WW_RENDER_VIEW` / `WW_RENDER_CENTER` / `WW_RENDER_DIST` block (~50 lines)
   becomes `GLView::wwCameraPinFromEnvironment()` + `wwApplyCameraPin()`, with
   the old view-only path kept in the `else`;
2. `WW_RENDER_CLEAN` also sets `skope->ogl->showCursor = false`;
3. one line beside `wwLogTopLevelWindows( "grab" )`:
   `skope->ogl->wwLogCameraCensus( "grab" )`.

Nothing in the impostor-bake block was touched.

## The resume, in order

Everything below is paste-able. `<scratch>` is
`E:/Projects/NifskopeWildWastelandEdition/scratchpad/hookcam_20260909`.

### 0. The gate, again

```bash
cd E:/Projects/NifskopeWildWastelandEdition
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?      # must print rc=1
```

### 1. Build — qmake FIRST

`src/glview.h` gained members, so every object that includes it is stale, and
`nifskope_ui.cpp` gained a new call into `GLView`. qmake before make
(`nifskope-ww-resume-pending` §3):

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/hookcam_20260909/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/hookcam_20260909/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/hookcam_20260909/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

Then the exe-newer sweep over EVERY changed file, not the one you edited:

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue; [ "$EXE" -nt "$f" ] || echo "STALE vs $f"; done
cmp release/style.qss res/style.qss && echo "stylesheet in step"
```

**Things most likely to be wrong on the first compile**, so look for them before
anything else: `wwBlenderStartupRotation` is a file-static in `glview.cpp` at
line 164 and the pin's implementation sits after it; `viewRotations`,
`perspectiveMode`, `Pos`, `Rot`, `Dist`, `Zoom`, `aspect`, `pixelWidth` are all
private members touched only from inside `GLView`; the census uses
`QString::number` rather than `arg()` markers on purpose.

### 2. The camera gate

```bash
bash tests/spells/render_shot.sh 2>&1 | tee scratchpad/hookcam_20260909/render_shot.log | tail -60
```

Section 7 is the new one. It is **pre-registered**, and these are the numbers it
must produce on a 1507×421 viewport (it reads the viewport back from the PNG, so
a different clamp changes the predictions and not the gate):

| case | prediction |
|---|---|
| control, no camera switch | 231.0 px — the old auto-fit framing, unchanged |
| ortho half-width 1024, eye 500 / 1000 / 2000, views Front and Right | 376.75 px, the SAME at all three |
| ortho half-width 2048 / 4096 | 188.4 / 94.2 px |
| perspective fov 60, eye 500 / 1000 / 2000 | 765.1 / 250.9 / 107.0 px |
| look-at moved 400 units at half-width 1024 | the silhouette centre moves 294.3 px |
| `WW_RENDER_ORTHO=-5` | `arm=` carries `refused-WW_RENDER_ORTHO-not-positive`, `persp=1` |
| two runs of one pinned camera | byte-identical |

Sections 0–6 are lane OFFSCREEN2's and were **55 checks, 0 failures** on the
22:04 exe; they must stay green. If section 6's `hidden and visible photograph
the same pixels` goes red, suspect the `showCursor` change — both halves of that
pair are `WW_RENDER_CLEAN`-free, so it should not reach them.

The only harness this change reaches besides `render_shot.sh` is
`lodgen_impostor_cards.sh` / `lodgen_octahedral.sh` (they drive the same hook's
sibling block). Run them if the build touched anything in the bake; nothing here
did, and say so rather than skipping silently.

### 3. THE ONE-TEXEL TRANSITION RENDER — the thing bungo asked for

```bash
python scratchpad/hookcam_20260909/transition.py                # or: ... <cards dir>
```

It gates on `Fallout4.exe`/`NifSkope.exe` itself and refuses rather than
running. It picks the card library automatically and PREFERS lane CARDFINAL's
(`scratchpad/cardfinal_20260909/cards`) when that exists — **say in the report
which library it used**, because the brief requires that. It then bakes three
chunks (mesh / card / centre-zeroed control), picks the most isolated instance
of each of the three trees out of the bake's own manifest, shoots six pinned
frames per tree, and prints a table.

The gate, as pre-registered in the brief:

* the card's silhouette centre within **one card texel at that distance**
  (the script prints the texel in both world units and pixels);
* extents within **2%** on both axes;
* the centre-zeroed control must **FAIL** — a control that passes means the
  measurement is not sensitive to the offset and the numbers are worthless.

Pictures land at `scratchpad/hookcam_20260909/transition_<base>.png`, one per
tree, `source | card | overlay`. **Open every one of them** (CONSTITUTION 5) and
check the obvious failure first: whether a neighbouring tree is inside the frame
and the flood fill took it. The script prints each chosen ref's nearest-neighbour
distance for exactly that reason; if a frame is crowded, re-run with a smaller
`FOV` constant at the top of the file rather than trusting the number.

### 4. The four documents

1. `WW_CHANGES.md` — the entry is already written with a **NOT BUILT** status
   block. Replace that block with the exe timestamp and the harness numbers.
   The file is MIXED: assert its CR count is unchanged (19,020) after the edit,
   and splice in binary.
2. `MISTAKES.md` — three entries are in; add anything the build finds.
3. `scratchpad/lane_hookcam_report.md` — sections 3, 4 and 7 say PENDING BUILD.
   Fill them; append rather than rewriting the rest.
4. `nifskope-ww-render-shot` — the amendment text is in section 6 of the report,
   and it must land in **both** skill trees
   (`E:\Projects\Claude\.claude\skills\...` and `<repo>\.claude\skills\...`),
   byte-identical, `cmp` clean. The skill currently tells every lane that the
   pin does not take on axis views and blames generated documents; that
   explanation is wrong and it is the most expensive sentence in the file.

### 5. Tell bungo his open NifSkope needs a restart

The exe changes, so his window is on the old one until he restarts it
(CONSTITUTION 6).
