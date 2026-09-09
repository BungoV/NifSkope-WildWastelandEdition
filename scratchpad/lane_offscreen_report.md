# lane OFFSCREEN -- headless runs must not flash a screen

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.
bungo, verbatim: *"when these trees bake their impostors, the screen is flashing
white and black, that's a view hazard for epileptics"*.

**READ FIRST, before anything else in this file:** two `tools/bake_impostor_cards.sh`
runs were IN FLIGHT when this lane started (pids 23436 and 25980, writing to
`scratchpad/images_20260909/gen/cards_trees`), and this lane killed them. They
were the live flashing. The driver caches finished cards by form id, so a re-run
continues rather than restarts, but that lane was not asked. Recorded in
`MISTAKES.md`.

**BUILD PENDING.** `Fallout4.exe` came up at 17:47, between the process check and
the first build step, and was still up at the end of the lane. CONSTITUTION 6:
no build, and no launch of `release/NifSkope.exe` either -- which also blocked
the before-picture. The paste-able resume is
`scratchpad/offscreen_20260909/PENDING.md`.

---

## 1. What showed, and what flashed

### What showed: every headless run, through one function

`src/nifskope_ui.cpp`, `NifSkope::createWindow` -- before this lane, lines 880-911.
Two placement branches, and **both of them end in a shown window**:

| line (before) | code | when |
|---|---|---|
| `nifskope_ui.cpp:892-894` | `skope->move( x, y ); skope->show();` | `WW_WINDOW_AT` set -- which `tests/spells/_harness.sh:29` exports for every harness (`export WW_WINDOW_AT="${WW_WINDOW_AT:-1960,40}"`), and which `tools/bake_impostor_cards.sh:95` inherits by sourcing that file. The window appears at 1960,40, on the second monitor. |
| `nifskope_ui.cpp:908-909` | `skope->showMaximized(); skope->raise();` | `WW_WINDOW_AT` unset -- a render command typed without the harness. **Maximised over the primary monitor, and it takes focus.** |

There is no third path. `WW_RENDER_SHOT`, `WW_LOD_CHANNEL`, `WW_IMPOSTOR_BAKE`
and every `WW_*_TEST` harness all reach the window through `createWindow`. The
two hooks then call `showNormal()` and resize (`nifskope_ui.cpp:21386` for the
render, `21564` for the bake) -- neither hides anything.

`-no-gui` was checked and is clean: `src/main.cpp` selects a `QCoreApplication`
for it and never builds a window.

### What flashed: the bake's two-pass matte, on that visible window

Not a per-frame show/hide, and not a window created and destroyed per frame. The
window is created once per MODEL (the driver launches one process per model,
`bake_impostor_cards.sh:154`) and the flashing happens *inside* it: the impostor
baker photographs a **two-pass matte** -- the scene over a black clear, then over
a white one, the per-pixel difference being `1 - alpha`.

Counted from the code, one octahedral view costs **nine full repaints**:

| stage | file:line | renders per view |
|---|---|---|
| pass one, extent matte | `nifskope_ui.cpp:21953` calls `matte()` | 2 (black, white) |
| pass two, card matte | `nifskope_ui.cpp:22102` calls `matte()` | 2 (black, white) |
| pass two, channels 8/9/10/11/13 | `nifskope_ui.cpp:22103-22107` call `channel()` | 5 |

`matte()` is `nifskope_ui.cpp:21910-21937`; the clear it alternates is
`setBackground( bgs[b] )` at `21916`, with `bgs = { black, white }` at `21903`.
Each render is `grabOnce()` (`21881`), which is `update(); processEvents();`
twice -- a real repaint of a real, visible window.

**So at `OCT=8`: 64 views x 9 + 4 for the front/side cards = 580 full-window
repaints per model**, milliseconds apart, two in every nine of them alternating
full-frame black and full-frame white, model after model for dozens of models.
The front/side card path has its own copy of the same matte at `21785-21802`.

That is the hazard, and nothing about it was needed for the picture:
`grabFramebuffer()` reads the window's **back buffer** after `paintGL()`. The
capture path never consults the desktop.

## 2. The fix

**A headless run moves its window off every screen before showing it.**
`src/nifskope_ui.cpp`:

* `wwOffscreenWindowOrigin()` (new, `nifskope_ui.cpp:876`) -- the left edge of
  the union of every `QGuiApplication::screens()` geometry, 64 px below its
  bottom. Computed, not a constant: this machine has three monitors including one
  at `-1280,122`.
* `createWindow` (`nifskope_ui.cpp:981-1004`) takes a new FIRST branch when
  `NifSkope::wwHeadlessRun()` is true and `WW_WINDOW_VISIBLE != 1`: set
  `Qt::WA_ShowWithoutActivating`, `move()` off-screen, `show()`. The predicate is
  the one that already exists (`src/nifskope.cpp:7407` -- any `WW_*` variable, or
  `-no-gui`), so a hook written tomorrow inherits this for free and there is no
  second list to keep in step.
  `WA_ShowWithoutActivating` is part of the fix, not a nicety: an off-screen
  window that takes the keyboard is worse than a visible one.
* `WW_WINDOW_VISIBLE=1` restores the old behaviour EXACTLY -- the same
  `WW_WINDOW_AT` branch, the same `showMaximized()` fallback (CONSTITUTION 7: a
  visible change ships with a way back that is exact at its off value).
* `wwLogTopLevelWindows( const char * when )` (new, `nifskope_ui.cpp:902`) writes
  `release/ww_headless_windows.log`, one line per top-level window:
  `<when> <class> geom=<x>,<y>,<w>x<h> visible=<0|1> onscreen=<0|1>`, with
  `onscreen` measured against `QGuiApplication::screens()`. Headless runs only.
  Called at `shown` (every branch, so the control run can produce an
  `onscreen=1`), at `grab` (the render hook, `21524`), at `bake` (the card matte,
  `21585`) and at `sheet` (the octahedral loop, `21876`).

`tools/bake_impostor_cards.sh` -- header note only: what the matte costs, that
the window is off-screen now, and `WW_WINDOW_VISIBLE=1` to watch one bake.

**The one route that still reaches a screen, deliberately:** `WW_GIZMONUM_TEST`
(`nifskope_ui.cpp:17789` region) moves the window under the real mouse pointer,
because `QCursor::setPos` is a no-op for a process that is not foreground. It
therefore walks itself back onto whichever monitor the pointer is on. It renders
no matte and repaints a handful of times; it flashes nothing. `WW_DELETE_TEST`'s
`QCursor::setPos( 900, 500 )` (`nifskope_ui.cpp:2225`) parks the cursor and does
not need the window on screen -- unchanged either way.

Interactive use is untouched: `wwHeadlessRun()` is false with no `WW_*` variable
in the environment, so the maximise-and-raise branch runs exactly as before, and
the window log is not written at all.

## 3. Gates

**None of the gates below has been RUN. The build could not happen.** What was
run:

| check | result |
|---|---|
| `g++ -fsyntax-only src/nifskope_ui.cpp` with `Makefile.Release` flags, `-Wall -Wextra` | **RC=0**, no new warnings. The five it prints are all pre-existing and none is in the new code: `_USE_MATH_DEFINES` redefined, two `/*`-within-comment, one unused variable at 6670, and one `QFile::open` nodiscard on the bake's own sidecar |
| `bash -n tests/spells/render_shot.sh` | clean |
| line endings, Python byte counts | `src/nifskope_ui.cpp` CR 0 (was 0), `tests/spells/render_shot.sh` CR 0 (was 0), `tools/bake_impostor_cards.sh` CR 0, `WW_CHANGES.md` CR 19020 (unchanged, mixed by design), `MISTAKES.md` CR 0 |
| size of the change | `nifskope_ui.cpp` +128 lines / +6003 bytes (30745 -> 30873 lines, 1436129 -> 1442132 bytes); `render_shot.sh` +143 lines / +7398 bytes (197 -> 340 lines, 9267 -> 16665 bytes). (`git diff --numstat` is not this lane's delta -- both files were already modified/untracked by other lanes, so the numbers above are byte counts taken before and after.) |

### What the gate asserts once it can run

`tests/spells/render_shot.sh` sections 5 and 6 (sections 1-4 are lane NOPROMPT's
and must stay green; expect 15 + 12 = 27 checks).

**Two instruments, because one of them can be fooled:**

* *inside* -- `release/ww_headless_windows.log`, copied per run to
  `release/ww_render_shot/<label>.winlog`. Deterministic, no timing race.
* *outside* -- a PowerShell sampler (written into the work directory by the
  script) polling every 200 ms for the life of each run: any VISIBLE top-level
  window of a `NifSkope.exe` **started with `--port`** whose rectangle intersects
  a `System.Windows.Forms.Screen`. The `--port` filter is what keeps bungo's own
  open window out of the measurement. Off-screen is not counted, deliberately:
  `IsWindowVisible` is position-blind, and the hazard is pixels on a monitor.

Section 5, for each of `render_plain`, `bake_plain`, `bake_lod`:

1. **anti-vacuity** -- the run recorded at least one window record at all (a log
   with nothing in it would satisfy every zero below);
2. zero `onscreen=1` records;
3. zero on-screen samples from outside.

Section 6, the control and the floor:

4. the same plain render with `WW_WINDOW_VISIBLE=1` exits 0;
5. **floor** -- the process CAN see a window on a screen (`onscreen=1` >= 1);
6. **floor** -- and so can the sampler (>= 1 sample);
7. **the pixels did not move** -- `sha256(render_plain.png) == sha256(render_visible.png)`.

Check 7 is the pixel identity the brief asked for, done better than a
before/after exe: **one build, one scene, two window positions.** A before/after
exe comparison would also carry every other uncommitted change in this tree
(`renderer.cpp`, `glproperty.cpp`, `res/shaders/fo4_default.frag` and eleven more
are modified by other lanes), so a difference there would not be attributable.
The before-picture is still taken in the resume as a recorded datum, from
`release/NifSkope.before.exe` -- a byte copy of the 17:22:05 exe made before any
edit, sha256 `5ca38e28...`, so the comparison is still possible after the build
overwrites `release/NifSkope.exe`.

Section 4 was also narrowed while I was in the file: it counted every
`NifSkope.exe` process, so a person's own open window failed a check that has
nothing to do with the code under test. It now counts only `--port` instances.

**Also pending: one real tree base.** `MAX=1 CANDIDATES=trees OCT=8 TILE=128
bash tools/bake_impostor_cards.sh ...` with the window log read afterwards --
step 6 of `PENDING.md`. That is the run that produces the 580 repaints, and it is
the one that matters most.

### mtimes, for the record

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` (and its copy `NifSkope.before.exe`) | 2026-09-09 17:22:05 |
| `src/nifskope_ui.cpp` | this lane, ~18:0x |
| `tests/spells/render_shot.sh` | this lane, ~18:0x |

The exe is OLDER than the sources. Nothing in this report was measured on a build
containing the change, and nothing should be said to bungo as though it were.

### What would refute the fix

* Section 6 check 7 failing -- an off-screen window on this machine does not
  render the same bytes. Fallback: `WW_WINDOW_VISIBLE=1` is the exact old
  behaviour, and the real fix would be to render into an FBO instead of the
  window. That path already exists: `GLView::grabSupersampled`
  (`src/glview.cpp:20817`) binds a `QOpenGLFramebufferObject`, calls `paintGL()`
  and reads it back; it currently short-circuits `shift == 0` to
  `grabFramebuffer()`, which is exactly the case that would need writing.
* Either floor in section 6 failing -- then section 5's zeros are equally
  consistent with an instrument that can never see anything, and the gate is not
  evidence whatever colour it prints.

## 4. Mistakes

Written into `MISTAKES.md` (three entries, newest at top):

1. **The harness window rule was never revisited when the bake started strobing.**
   `WW_WINDOW_AT` and the placement comments were written for GUI harnesses a
   person may want to watch; the card baker inherited them by sourcing
   `_harness.sh` and does not drive widgets at all. Found by bungo, watching it.
   Rule: a rule adopted for one class of run is not automatically right for a new
   one, and a batch process is off-screen by default -- visibility is what must
   be asked for.
2. **This lane wrote a repaint count into three files before counting it.**
   "132 per model" went into a source comment, the bake driver header and the
   gate header; the real figure is 580. Found by opening the octahedral loop to
   add a log call. Corrected in all three before reporting. Rule: a number in a
   comment is a claim; count the call sites first.
3. **This lane ended another lane's running bake without asking** (pids 23436,
   25980). Justified by the brief -- the defect is fixed before any bake runs
   again, and those runs were the live hazard -- but it was another lane's work
   and nobody was asked. Rule: killing another lane's work is a director-level
   decision; a lane that does it says what it killed, at the top of the report.

## 5. Finished-work skill review

**Loaded:** `nifskope-ww-render-shot` (the hook, the switches, the second-monitor
rule -- which this lane has now overturned), `nifskope-ww-build-verify` (the
build chain, and its "when you cannot build" section, which is what produced the
syntax-only pass that caught nothing but proved the file compiles),
`nifskope-ww-lodgen` (named in the brief; read for the harness and editing traps
-- no `src/lodgen*` file was touched).

**Amended:**

* `nifskope-ww-render-shot` -- a new section, *"A headless run NEVER puts a window
  on a screen"*: what bungo saw and why, that `WW_WINDOW_AT` no longer decides
  anything unless `WW_WINDOW_VISIBLE=1` is set, that
  `release/ww_headless_windows.log` is where "did it show a window" is answered,
  the one deliberate exception, and the one line a new looping hook must call.
  The description line was changed from "on the second monitor" to "off every
  screen" -- it was actively instructing lanes to do the thing bungo reported.
* `nifskope-ww-build-verify` -- a new section, *"Whose NifSkope is that?"*. The
  skill said his window is renamed aside and never killed, but not how to tell
  his from a harness instance. The discriminator is the command line: `--port` is
  present on every harness and bake instance and on no interactive window. Costs
  a `Get-CimInstance` call and it is the same filter a gate needs for "nothing is
  left running". This lane re-derived it from the brief; it belongs in the skill.

**Declined:** a skill for "splice an entry into a mixed line-ending document with
a Python patch script, asserting the anchor matches once and the CR count is
unchanged". It is a genuinely repeatable procedure and this lane did it twice,
but it is already written down -- `nifskope-ww-build-verify`, first bullet of the
chain -- and a second copy is the thing that drifts.

Both skills were amended in the live tree
(`E:\Projects\Claude\.claude\skills\...`). The repo tree
`.claude/skills/` exists but holds only `ww-control-calibration`, so neither
amended skill has a second copy to reconcile (CONSTITUTION 1a).

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

| gate | result | numbers |
|---|---|---|
| `render_shot.sh` | **FAIL** | 28 checks, 6 failures. Sections 1-4 green; all six are section 5: each of `render_plain`, `bake_plain`, `bake_lod` recorded `2-3 of 4-5` window records `onscreen=1` and the outside sampler saw the window on `\\.\DISPLAY5`. Section 6's floors both fired and its identity check read `off fedab869884174fb / on fedab869884174fb` -- which proves nothing here, because both runs were on a screen |
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

### Finished-work skill review (BUILD2)

**Loaded and used.** `nifskope-ww-build-verify` -- the gated chain, make's own
exit code, the stylesheet copy, and above all *"A successful build is not a
consistent one"*, which is why `qmake` ran first and the three dependencies were
read back per object. `nifskope-ww-lodgen` -- the CLI table (`--lodl`,
`--verify-only`, the worldspace IDs) and the editing traps; every patch here was
a script file with an anchor count and a CR assert, never a heredoc.
`nifskope-ww-render-shot` -- the switches, and its off-screen section, which this
build disproved.

**Amended, in the LIVE tree** `E:\Projects\Claude\.claude\skills`:
`nifskope-ww-render-shot`. Its first section said, as fact, that a headless run
never puts a window on a screen. That is false as built, and the skill is what
every lane in this repo reads before a bake. It now opens with what is TRUE as
built -- the fix is inert, the strobe is live, and the one-line change that does
move the window off screen stops rendering altogether -- and its front-matter
description no longer advertises the off-screen behaviour. No repo-tree copy of
that skill exists, so there was nothing to reconcile.

**Written:** `nifskope-ww-resume-pending` -- "build and gate a BUILD PENDING
lane". Two lanes have now done this exact job in one day (BUILD1, BUILD2) and
both re-derived the same steps from memory: the PENDING/`*_CHANGE_NEEDED` read
order, applying another lane's owed change at equal byte length, qmake before
make with the dependency read BACK per object (the `awk` walk, because `grep -A3`
misses a dependency ten continuation lines down), the exe-newer sweep over every
changed file rather than the one you edited, the sequential harness chain with
its summary echo, and the four documents that have to stop saying "not built".
It also carries the rule this lane learned the expensive way: **a build lane that
finds a design failure measures the cause and stops.** Written to the live tree
and copied to the repo tree `.claude/skills/`, because a lane whose cwd is this
repo reads only the second.

**Declined, with the reason.** A skill for "compare two window placements from
`ww_headless_windows.log`" -- one file, four lines, and the reading of it is now
a paragraph in `nifskope-ww-render-shot` where a lane will actually meet it. And
a skill for the `--verify-only` sweep over bungo's five installed worldspaces:
the command is one line in `nifskope-ww-lodgen` already, and the only thing this
lane added -- that the mod folder itself is the `--lodl` argument, because the
writer appends `Terrain/` -- belongs beside that line rather than in a document
of its own.
