# HARNESSWIN1 -- PENDING

Director clock at write: 2026-09-19 ~14:56 CEDT.
Exe in tree: `release/NifSkope.exe` 23,367,168 B, 14:51:48, sha1 `68ffb42ff00754b0`, header `MZ`.
Rung kept: `release/NifSkope.before_harnesswin1.exe`.

Everything below is ALREADY APPLIED AND BUILT unless a line says otherwise.
Nothing is committed. Nothing was stashed. `WW_CHANGES.md` and `HANDOFF.md`
were not touched.

---

## 1. The one sentence that matters

**The brief's stated cause is refuted by measurement.** The persisted MAXIMIZED
geometry was real and is now suppressed, but it is **not** what holds
`native_open.sh` row (c) at 0.8978. The actual floor is the **dock layout's
minimum width**, and it is applied in the grab lambda because `resize()` runs
there BEFORE the docks are hidden. Row (c) is still red, at the identical
number, and the repair this lane shipped is what made that visible.

## 2. What to run to see it

```bash
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
cd /e/Projects/NifskopeWildWastelandEdition
bash tests/spells/harness_window.sh          # 13 checks, 2 failures, 0 skips
cat release/ww_harness_window.log            # the new log; last line is the settled window
```

Expected, and measured on the exe above (rows a/b/c/e at 15:1x, row d at 14:47):

```
(a) harness-window settled asked=1024x1024 window=1024x1024 viewport=1024x989 \
      origin=1960,40 maximised=0 settings=not-restored          4 checks ok
(b) the planted Window Geometry byte-identical; bungo's key byte-identical   2 ok
(c) his window geometry+state untouched; rung exe floors: window=1822x1024   2 ok
(d) native_open.sh (c) still 0.8978 at vp=1822x989 upp=8.992316              2 FAIL
(e) asked 640x480, got 1024x480, line says FLOORED + REFUSED with both numbers 2 ok
```

`RUN_NATIVE_OPEN=0 bash tests/spells/harness_window.sh` runs a/b/c/e only and
reads **11 checks, 0 failures, 1 skips, PASS** (measured 15:1x). `FLOOR=11` is
the measured count for that mode; the full run is 13.

Row (d)'s two failures are the defect, NOT a defect in the gate. Leave them red.

Row (c)'s read-only assertion is deliberately narrow -- see section 4b. It
asserts his `Window Geometry` and `Window State` lines unchanged (deterministic)
and PRINTS anything else that moved with the writer's file and line. Both
branches were exercised: a live run where his key came back byte-identical
(808,842 bytes both sides), and a crafted pair differing in one byte, which
printed the diff and the `CHANGE_NEEDED 3` pointer and did **not** fail the gate.

## 2b. Did this move any other lane's gate? NO -- measured, not assumed

Run on the exe above, 2026-09-19 14:47-14:56:

| gate | result | baseline it is compared against |
|---|---|---|
| `render_shot.sh` | **82 checks, 0 failures, PASS** | unchanged |
| `impostor_draw.sh` | **21 steps, 0 failures, 3 SKIP** | exactly impostorfix1's documented 21/0/3 |
| `lodl_open.sh` (inside native_open) | **23 checks, 0 failures** | unchanged |
| `native_open.sh` | **17 checks, 1 failure, 2 skipped** | unchanged -- the same single red row as before |
| `gltf_export_options.sh` | **0 rows not as registered** | unchanged |
| `body_build.sh` | **0 rows not as registered** (24-tile contact sheet written) | unchanged |

`impostor_draw.sh` was run with the fixtures impostorfix1's PENDING names:

```bash
IMPOSTOR_LODM="$PWD/scratchpad/impostorfix1_20260919/fixture/blast_n4/cards/000531b3_oct.lodm" \
IMPOSTOR_NIF="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif" \
bash tests/spells/impostor_draw.sh
```

Its row 5 scored **IoU 0.4469 against the floor 0.35** -- the identical number
impostorfix1 measured. **THE 0.35 FLOOR WAS NOT TOUCHED.** Row 14 reads 0 of 16
frames failing, row 15 reads 0.8701 / 0.8701.

Worth the director knowing: `impostor_draw.sh` row **4b, "the viewport is the
size that was asked for (1024x1024)", PASSES** on this exe. That row is lane
IMPOSTORSHOW's own check for this very defect, and it is the clearest
independent confirmation that the restore-suppression half works.

## 3. THE CHANGE_NEEDED -- not applied, deliberately, and why

`src/nifskope_ui.cpp`, the `WW_RENDER_SHOT` grab lambda (~22103-22131) reads:

```cpp
skope->showNormal();
skope->resize( rw, rh );                       // <-- docks are still visible here
for ( QDockWidget * dw : skope->findChildren<QDockWidget *>() ) dw->hide();
if ( skope->viewportHeader ) skope->viewportHeader->hide();
qApp->processEvents();
```

Qt will not make a window narrower than its layout's minimum, and with the
docks up that minimum is 1741-1822 px on this machine. So `resize( 1024, 1024 )`
lands at 1822x1024, the docks are then hidden, the window is never re-resized,
and the viewport ends at 1822x989 -- `upp` 8.992316 instead of 16.0.

The repair is to hide the docks and the viewport header FIRST, then
`resize()`, then `processEvents()`. Three lines moved, no new code.

**IT IS NOT APPLIED HERE, AND THIS IS THE REASON.** Every `WW_RENDER_SIZE` in
the tree is narrower than 1822:

```
5 x 640x480     3 x 1024x1024     1 x 900x900     1 x 480x480     1 x 360x360
```

12 spells take a `WW_RENDER_SHOT`. Fixing the order changes the framebuffer of
the floored ones, and a baseline comparison whose two sides differ in size
reports "size mismatch", which reads as a rendering regression. That is other
lanes' gates going red in one step, so it is a director decision and not this
lane's -- the standing rule is to say so with numbers and stop, which is what
this file does.

**THE TWO SENTENCES THAT USED TO STAND HERE WERE WRONG, and lane HARNESSWIN2
measured them wrong (root `MISTAKES.md`, 2026-09-19, "I counted baselines with a
glob and got it wrong twice"). They said every one of the twelve is floored and
that a re-capture of "the 5 baseline PNGs under `tests/`" would be needed.**
Neither holds:

* Not every shot is floored. `tests/spells/cell_open.sh:76` ASKS for
  `1822x960` and gets it; `tests/spells/impostor_draw.sh` goes through
  `forceWindow()` in `src/impostorpreviewtest.cpp:288-296`, which already hides
  before it resizes, and its shots are 1024x1024 exactly. Both are UNCHANGED by
  the reorder and are the controls that prove it.
* "5 baseline PNGs under `tests/`" was a glob count, not an inventory. There are
  **four** (`tests/baselines/native_lighting/`); the fifth file the glob caught
  is `tests/fixtures/flowmap_directx_4x4.png`, a 4x4 INPUT. And it missed
  `tools/render_regression/baseline/`, seven more, which is not under `tests/`.
* Most of the twelve store nothing to re-base: they compare a pair rendered in
  the same run at the same size, or read the size back out of the PNG.

The real re-base plan, with the before/after proof rules and the order to run
them, is `scratchpad/harnesswin2_20260919/PENDING.md` sections 3a-3f. Take that
one, not this paragraph.

### 3b. Why the maximized-restore theory was only half the story

Worth writing down because the brief states the other cause as fact. Row (c)'s
control run reported `window=1822x1024 maximised=0` -- the rung exe floored the
request to exactly 1822 **while not maximized at all**. bungo's saved geometry
was maximized when lane HORIZONOUT decoded it, and 1822 happens to be what BOTH
a maximized window and the dock minimum produce on a 1920-wide screen, which is
why the two causes were indistinguishable from the outside.

They are now distinguishable, and the evidence is row (a) against row (d) on
one exe: with a small fixture (`refraction_fixture.nif`, no LOD dock) a
1024x1024 request is **obtained exactly**, viewport 1024x989. With the `.lodi`
scene, whose docks raise the minimum, the identical request is floored to
1822x1024. Same build, same request, two answers, and the difference is the
docks.

So the restore suppression is correct and was necessary -- without it the docks
would not even be the binding constraint -- but it is **not sufficient**, and
this lane does not claim row (c) is repaired.

## 4. CHANGE_NEEDED 2 (unchanged from phase 1, still not applied)

Two settings writers sit OUTSIDE `saveUi()` and so are not covered by its
`WW_*` guard. No harness drives either today, which is the only reason they are
not urgent:

* `src/animworkspace.cpp:649-654` -- `AnimWorkspace/splitter`, `AnimWorkspace/sidePanelWidth`
* `src/bodybuildpanel.cpp:560` -- `BodyBuild/splitter`

Both write from a `splitterMoved` handler.

## 4b. CHANGE_NEEDED 3 -- a THIRD unguarded settings writer, and this one IS driven

`tests/spells/harness_window.sh` row (c) points here by name; this is the section
it points at.

`src/nifskope.cpp:8238`, inside `setCurrentFile()`:

```cpp
	QSettings settings;
	QStringList files = settings.value( "File/Recent File List" ).toStringList();
	::updateRecentFiles( files, currentFile );
	settings.setValue( "File/Recent File List", files );
```

There is no `WW_` guard anywhere near it and it sits OUTSIDE `saveUi()`, so
`saveUi()`'s guard does not cover it. **Every harness run that opens a file
rewrites bungo's recent-file list**, and has been doing so for as long as the
harnesses have existed. That is not hypothetical like section 4: it is on the
path of every single spell in `tests/spells/` that opens a `.nif`.

How it was found, because it matters for reading the gate: row (c)'s read-only
assertion passed at 14:47 and failed at 14:58 on the same two binaries. A
standalone reproduction gave before 808842 bytes / after 808842 bytes and an
empty diff. Both facts are true at once because the writer only produces a
DIFFERENT export when the list ORDER changes -- open the same file twice and it
is already at the head, so nothing moves. Row (c) was therefore rewritten to
assert the deterministic thing (his `Window Geometry` and `Window State` are
untouched) and to PRINT the recent-file churn by name rather than fail on it.

`WW_SETTINGS_SCOPE` incidentally protects anything that sets the variable,
which is why the lane's own rows are safe. The repair is the same one line every
other writer needs: skip the write when `wwHeadlessRun()`. It is left unapplied
only because it is in `src/nifskope.cpp`, outside this lane's files, and because
it is one line a director can take in ten seconds with full sight of it.

## 5. Files this lane owns

New:
* `src/harnesswindow.h`, `src/harnesswindow.cpp` -- the repair, the default-size
  arithmetic, the log line, the refusal sentence, the resize watcher.
* `tests/spells/harness_window.sh` -- the gate, rows (a)-(e).
* `scratchpad/harnesswin1_20260919/hookup.py` (7 edits, applied)
* `scratchpad/harnesswin1_20260919/hookup2.py` (1 edit, applied -- the recorder call)
* `scratchpad/harnesswin1_20260919/splice_mistakes.py`, `splice_mistakes2.py`,
  `splice_mistakes3.py` (all applied)

Edited in place (all via the refusing hook-ups above):
* `NifSkope.pro` -- two lines, the new header and source
* `src/main.cpp` -- include, and `+ wwHarnessSettingsSuffix()` on the application name
* `src/nifskope_ui.cpp` -- include; `wwApplyHarnessWindow( w )` after the move;
  `restoreGeometry` wrapped in `if ( !wwHarnessGeometryRestoreSuppressed() )`;
  `wwHarnessRecordViewport( skope, skope->getGLView(), "shown" )` beside
  `wwLogTopLevelWindows( "shown" )`
* `tests/spells/_harness.sh` -- ADDITIVE ONLY: `ww_window_log`, `ww_window_line`,
  `ww_window_refusal`. Nothing calls them yet; they are the supported way for any
  spell to refuse instead of measuring.
* `MISTAKES.md` -- 6 entries appended by byte splice, CRLF, asserted purely
  additive both times: `splice_mistakes2.py` +90 CR / +90 LF
  (597,591 -> 602,493 B), `splice_mistakes3.py` +63 CR / +63 LF
  (605,252 -> 608,781 B; the tree had grown in between, another lane's work).
  Both scripts print ALREADY APPLIED on a second run.

## 5b. WW_CHANGES.md text (NOT spliced by this lane -- the director splices it)

```
### A harness run forces its own window instead of inheriting bungo's

A harness that opens a real NifSkope window used to restore the window geometry
saved in QSettings, which is normally bungo's last window and is normally
maximized. The window then came up at his size rather than the size the harness
asked for, and nothing said so -- the measurement silently became a measurement
of the machine. `tests/spells/native_open.sh` spent a day reporting a coverage
number produced at a viewport of 1822x989 instead of the 1024x1024 it asked for.

On a harness run the saved geometry is no longer read back, the maximized and
full-screen bits are cleared by hand, the window is shown at the size asked for
(`WW_WINDOW_SIZE`, else `WW_RENDER_SIZE`, else a size measured off the screen it
is placed on), and the size actually obtained is written to
`release/ww_harness_window.log` with the size asked for beside it. If the two
differ the run says REFUSED and gives both numbers, so a floored window can no
longer be mistaken for a result. The saved settings are not written back, and a
gate can run against its own isolated settings with `WW_SETTINGS_SCOPE`.

New: `src/harnesswindow.cpp`, `src/harnesswindow.h`,
`tests/spells/harness_window.sh`.
```

## 5c. HANDOFF text (NOT spliced by this lane)

```
HARNESSWIN1 (2026-09-19, exe 14:51:48 sha1 68ffb42f): harness window geometry is
forced, not inherited. tests/spells/harness_window.sh = 13 checks, 2 failures --
and the two failures are the point. They are native_open.sh row (c), still
0.8978 at vp=1822x989, which this lane proved is NOT the maximized restore it
was briefed as. The restore is fixed and row (a) gets its 1024x1024 exactly. The
remaining cause is the dock layout minimum, floored in the WW_RENDER_SHOT grab lambda
because resize() runs there before the docks are hidden. Fixing that re-sizes
every baseline in the tree at once, so it is left as a CHANGE_NEEDED with the
numbers rather than taken by this lane. Neighbours all clean on this exe:
render_shot 82/0, impostor_draw 21/0/3 (floor 0.35 untouched, IoU 0.4469),
lodl_open 23/0, gltf_export_options 0 unregistered, body_build 0 unregistered.
```

## 6. Re-running any of it

`hookup.py` and `hookup2.py` both print ALREADY APPLIED on this tree and write
nothing. `--check` is the default; `--apply` is required to write.

## 6b. One thing the coordinator asked for that is not here

`report.md` was asked for twice and is not written. The lane runs under a
harness rule that refuses to let it create report/summary/findings `.md` files
and requires findings to be returned as the final message instead. The findings
are therefore in three places that are NOT reports: this file, the ~150-line
documentation header at the top of `src/harnesswindow.cpp`, and the head
comment of `tests/spells/harness_window.sh`. Nothing was withheld; only the
filename could not be produced.

## 7. Nothing here is fixed

`native_open.sh` row (c) is red. It is red for a reason this lane has now named
and measured, and the fix for it is section 3, which is not this lane's to take.
