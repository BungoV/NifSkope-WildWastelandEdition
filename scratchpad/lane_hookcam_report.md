# lane HOOKCAM — the render hook's camera, pinned and proved

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main` at `720762a`,
**no commits** (CONSTITUTION 8).

Owed by lane CARDFIT3 and taken up here, in its words: *"the render hook cannot
pin a camera (WW_RENDER_CENTER does nothing; WW_RENDER_DIST fixed,
WW_RENDER_CLEAN=1 added) — the pinned-camera hook is OWED, and the one-texel
transition render with it."*

---

## 1. The cause, measured before a line was written

Everything in this section was measured on the exe **already on disk**
(`release/NifSkope.exe` 2026-09-09 22:04:35, lane CARDPAD's build), with a
512-unit cube written by the application's own CLI
(`-no-gui new --cube --size 512`; the mesh spans ±256 about its own origin and
its node is lifted 256 so the cube sits on the grid — its centre is world
`0,0,256`). Probe workspace: `scratchpad/hookcam_20260909/probe/`.

### 1.1 What the two switches actually did

| switch | claimed | measured |
|---|---|---|
| `WW_RENDER_CENTER` | points the camera at a world position | **nothing at all** on an axis view: look-at `0,0,256` and `400,0,256` gave two files with the same md5 `fff710bd…` |
| `WW_RENDER_CENTER` | — | it DOES take on `WW_RENDER_VIEW=8` (ViewUser) — different files, `aee71e3d…` / `ae551f3e…` |
| `WW_RENDER_DIST` | the orthographic half-height | scale ∝ **1/D²**, not 1/D, and the projection is **perspective**, not orthographic |

The distance sweep, at the half-intensity silhouette edge (sub-pixel; the
instrument is the one the gate ships, validated in 5.1), viewport 1507×421 in
every run. `eye implied` inverts the perspective law
`span = 512·H / (2·tan(fov/2)·(eye − 256))` at `H = 421`, `fov = 60`, the cube's
silhouette being its front face, 256 units nearer than the look-at:

| `WW_RENDER_DIST` | 400 | 500 | 600 | 700 | 768 | 800 | 900 | 1000 |
|---|---|---|---|---|---|---|---|---|
| span, px | 540.8 | 273.0 | 170.5 | 117.7 | 95.0 | 87.0 | 67.0 | 53.7 |
| eye implied | 601 | 940 | 1351 | 1841 | 2221 | 2402 | 3042 | 3729 |
| `want² / (eye/2)` | 532.3 | 532.0 | 533.0 | 532.2 | 531.1 | 533.0 | 532.5 | 536.3 |

The last row is constant at **532.8 ± 0.5%**, and the fixture's own auto-fit
distance is `bound radius 443.405 × 1.2 = 532.09`. So the camera the picture was
taken with was `want² / 532`, not `want`. (1500 and 2000 are left out of the
table: at 11–13 px the 3D-cursor gizmo of 1.4 is inside the silhouette and the
measurement is of the gizmo, which is the reason 1.4 was found at all.)

### 1.2 The cause: `center()` does not centre

`GLView::center()` is **`doCenter = true; update();`** — two lines
(`src/glview.cpp:5153`). The work happens later, inside `paintGL`:

```
	// Center the model
	if ( doCenter ) {
		setCenter();          // Pos = -bounds.centre, Dist = radius * 1.2, Zoom = 1
		doCenter = false;
	}
```

and `setOrientation( state, /*recenter*/ true )` ends in `center()`. So the
render hook's sequence

1. `setOrientation( ViewFront, true )` → **queues** the auto-fit
2. `setPosition( -centre )` → the look-at
3. `setDistance( want )` → the distance
4. `qApp->processEvents()` → **`paintGL` runs and `setCenter()` replaces both**
5. read back `Dist / Zoom`, find 532, "correct" it to `want² / 532`

destroys the look-at outright and mangles the distance into an inverse-square
law. Step 5 is why the distance had any effect at all; before lane CARDFIT3
added the read-back, `WW_RENDER_DIST` did nothing whatsoever.

**And the ViewUser exception is the same mechanism from the other side.**
`setOrientation` opens with `if ( state == view ) return;`. On ViewUser — the
startup view — the requested state already IS the current one, so the function
returns before `center()`, nothing is queued, and the camera the hook sets
survives. That is the whole of the "the pin takes on ViewUser and nowhere else"
note in `nifskope-ww-render-shot`; it was never about generated documents.

### 1.3 The projection is PERSPECTIVE, and nothing headless ever set it

`restoreUi()` has `const bool isPersp = true;` (`src/nifskope_ui.cpp:29232`) and
`ogl->setProjection( isPersp )`. Nothing in the render hook, and nothing in the
impostor bake, ever calls `setProjection` again — the only other callers are
the View menu and Numpad-5. Two measurements say so rather than the code:

* a scene with two identical cubes 1024 units apart in depth, one camera: the
  near one photographs **31 px** where a lone cube at the same setting is 27 —
  `4000/3488 = 1.147`, `27 × 1.147 = 31.0`. An orthographic camera would draw
  them the same size;
* the unpinned control frames the cube at **231 px**, and
  `512 × 421 / (2 × tan30 × (1064.17 − 256)) = 231.0` where `1064.17 = 2 × 1.2 ×
  443.405` is the auto-fit eye distance. The orthographic reading of the same
  state predicts 202.6.

**This is a finding for the impostor bake, not for this lane.** The bake reads
`orthographicHalfHeight()` (`Dist / Zoom`) and fits its frames with it, and
`glProjection` was drawing those frames through a 60° perspective frustum. It is
recorded here and in `MISTAKES.md`; it is CARDFINAL/CARDPAD territory and
nothing in this lane changed it.

### 1.4 A smaller one found on the way: `WW_RENDER_CLEAN` left the 3D cursor in

`WW_RENDER_CLEAN=1` clears `ShowGrid | ShowAxes | ShowNodes`, but the 3D cursor
is not in `Scene::options` — it is `GLView::showCursor`, which only the impostor
bake turned off. The gizmo is **27 px wide whatever the camera does**, so a
silhouette box taken off a small object read 27 px when the object was 11, and
the distance sweep looked as though it saturated. Fixed here.

---

## 2. The fix

### 2.1 Where it is

`GLView::WwCameraPin` — `src/glview.h`, `src/glview.cpp`. The hook
(`src/nifskope_ui.cpp`) loses 50 lines of camera arithmetic and gains three:
read the pin from the environment, apply it, log the census at the grab.

The pin **cancels the queued auto-fit and re-asserts itself at the top of every
paint**, immediately after the `doCenter` block that used to eat it. That is
what makes it hold on a generated `.lodl`/`.btd` document, which builds its
scene and reframes long after the hook has run, and on any later compile.

It is a module with its own switch (CONSTITUTION 10): `active` is false unless
one of `WW_RENDER_CENTER` / `_DIST` / `_FOV` / `_ORTHO` is set, and then nothing
in it runs — **a `WW_RENDER_VIEW`-only capture keeps exactly the old auto-fit
framing**, so no existing baseline moves. Every refusal is named in the census's
`arm=` field rather than swallowed.

### 2.2 The switches, and what each one now means

| switch | meaning |
|---|---|
| `WW_RENDER_CENTER=x,y,z` | the world look-at point |
| `WW_RENDER_DIST=d` | the **EYE's distance to that point**, in world units. It used to mean the orthographic half-height, a meaning that never once took effect |
| `WW_RENDER_VIEW=n` | as before; it joins the pin only when the pin is armed by something else |
| `WW_RENDER_FOV=deg` | full **vertical** field of view; forces perspective |
| `WW_RENDER_ORTHO=w` | orthographic, `w` = the half-**width** in world units; the eye distance then only moves the clip planes |

### 2.3 The formula a picture now carries

`upp` — units per pixel at the look-at plane — is in the census, and an extent
of `E` world units lying in that plane spans `E / upp` pixels.

```
orthographic:   upp = 2 * W / viewportWidth
perspective:    upp = 2 * tan( fov / 2 ) * eye / viewportHeight
```

### 2.4 The census

`release/ww_camera_pin.log`, or `$WW_CAMERA_CENSUS`. One line per stage,
truncated by the first record of each process so a reader can never quote the
previous run:

```
grab arm=... view=5 rot=... lookat=... eye=... dist=... zoom=... persp=0|1
     fov=... halfW=... halfH=... vp=WxH upp=...
```

Read off the live members at the moment of the grab, never off what was asked
for.

---

## 3. The camera numbers

**PENDING BUILD** — see section 7. What the gate will produce is pre-registered
here, before it runs (CONSTITUTION 1), on the 1507×421 viewport this machine's
clamp gives (`tests/spells/render_shot.sh` reads the viewport back from the PNG,
so a different clamp changes the predictions and not the gate):

### 3.1 Orthographic, `WW_RENDER_ORTHO=1024`, the 512-unit cube

`upp = 2 × 1024 / 1507 = 1.359`, so `span = 512 / upp = 376.75 px`, **at every
eye distance and on both views** — that invariance is the point of the
orthographic arm.

| view | eye 500 | eye 1000 | eye 2000 |
|---|---|---|---|
| Front (5) | 376.75 | 376.75 | 376.75 |
| Right (4) | 376.75 | 376.75 | 376.75 |

and the scale law at eye 1000: half-width 1024 / 2048 / 4096 → **376.75 / 188.4
/ 94.2 px**, a 4 : 2 : 1 ladder.

### 3.2 Perspective, `WW_RENDER_FOV=60`

`span = 512 × 421 / (2 × tan30 × (eye − 256))`, the cube's silhouette being its
front face:

| eye | 500 | 1000 | 2000 |
|---|---|---|---|
| predicted span, px | 765.1 | 250.9 | 107.0 |

### 3.3 The two floors

* **the control** — no camera switch at all, `WW_RENDER_VIEW=5` only:
  **231.0 px**, the old auto-fit framing, and it is already measured on the
  22:04:35 exe at exactly 231.000 px
  (`scratchpad/hookcam_20260909/probe/before_control_v5.png`, sha256
  `eb40c56d…`; without `WW_RENDER_CLEAN`, `188d7873…`, also 231.000). If the
  build changes that number, the pin has leaked into unpinned captures;
* **the look-at moves the picture** — half-width 1024, look-at `0,0,256` versus
  `400,0,256`: the silhouette centre must move `400 / 1.359 = 294.3 px`. On the
  22:04:35 exe those two runs are the SAME FILE (md5 `fff710bd…` both), which is
  this check going red on the defect.

## 4. The one-texel transition render

**PENDING BUILD** — it cannot be done without the pin, which is the whole reason
lane CARDFIT3 could not do it either.

The runner is written, compile-checked and never run:
`scratchpad/hookcam_20260909/transition.py`. One command, and it refuses if
`Fallout4.exe` or a `NifSkope.exe` is up. What it does:

1. bakes the same Sanctuary object chunk three times — **mesh** (the refs on
   their own LOD meshes), **card** (the same refs on their impostor cards), and
   **ctl** (the same cards with `center` zeroed in the sidecar, which is what a
   reader that ignores the pivot→centre offset draws);
2. picks, per tree, the instance standing furthest from any other ref, out of
   the bake's own manifest — so the pinned frame has as little else in it as
   possible, and the choice is zero-authored;
3. shoots six pinned perspective frames per tree — mid and ring distance from
   `scratchpad/cardfit_20260909/rend/dists.json` (1873.95 / 7495.80,
   1457.66 / 5830.63, 766.78 / 3067.13) — look-at = the ref's pivot plus the
   card's own centre offset, `WW_RENDER_FOV=60`, `WW_RENDER_CLEAN=1`;
4. measures each silhouette by flooding the component at the frame centre (the
   mask is closed by a 7×7 dilation first, so a canopy broken into leaf clusters
   is still one silhouette) and compares card against mesh:
   **centre within one card texel at that distance** (one texel = `2 × halfW /
   128` world units, printed in pixels too) and **extents within 2%**;
5. writes `scratchpad/hookcam_20260909/transition_<base>.png`, one per tree,
   `source | card | overlay`.

The control is the floor and it must FAIL. A passing control means the
measurement is not sensitive to the offset and the numbers are worthless.

Card library: the script prefers lane CARDFINAL's
(`scratchpad/cardfinal_20260909/cards`) when it exists and otherwise falls back
through CARDPAD's and CARDFIT3's `cards_after`, and it prints which it used. As
of this writing CARDFINAL has no `DONE` marker and no `cards` directory, and the
only library on disk carrying all three trees is
`scratchpad/cardfit_20260909/cards_after` (21:08) — the sample-set copy under
`scratchpad/images_20260909/gen/cards_trees` has `0004a074` and `00038599` but
not `0003a28b`. Nothing of CARDFINAL's was read or touched.

## 5. Mistakes

Three are other lanes' and are in `MISTAKES.md` (CARDFIT3's read-back that
shipped an inverse-square camera; the impostor bake fitting orthographic frames
through a perspective frustum; `WW_RENDER_CLEAN` leaving the 3D cursor in). Two
are this lane's own:

### 5.1 My own span instrument was 1 px wide, and only a hard edge caught it

The half-intensity crossing interpolated `i + (thr − a) / (b − a)` without the
`(j − i)` factor, and the right-hand edge is called with `j = i − 1`. So the
right crossing landed one pixel OUTSIDE the object and every span read 1.0 px
too wide — on a gate whose tolerance is 1 px. It survived a first look because
231 against a predicted 231.0 "passed": the error was exactly the size of the
tolerance.

It was caught by validating the instrument against a known answer before
trusting it: the render has **no antialiasing at all** (the edge profile steps
44.9 → 183.0 in one pixel), so the crossings must come out exactly on pixel
boundaries — 637.5 and 868.5 — and they did not. Fixed, and the file now says
why the factor is there. Rule: an instrument gets a known-answer test of its own
before it is pointed at the thing under test, and a result that agrees with the
prediction to within exactly the tolerance is a suspect, not a pass.

### 5.2 I edited a file this lane does not own

`src/nifskope_ui.cpp` is not in this lane's file list. The render hook lives
there and not in `src/nifskope.cpp`, so the call site had to change. Three small
hunks, none of them in the impostor-bake block lane CARDFINAL may be in;
enumerated in `scratchpad/hookcam_20260909/PENDING.md`. Recorded here rather
than quietly: a file list in a brief is a claim about where the code is, and
when it is wrong the right move is to say so with the diff, not to route around
it (the pin itself is in `glview.cpp`/`.h`, which this lane does own, precisely
to keep that hunk to three lines).

## 6. Skill review

**Loaded and used.** `nifskope-ww-render-shot` — its window rules (second
monitor, one instance, `WW_WINDOW_AT`), the `WW_RENDER_SIZE` clamp (which is why
every prediction here reads the viewport back out of the PNG rather than
believing `640x480`), and the save-confirmation section.
`nifskope-ww-build-verify` and `nifskope-ww-resume-pending` — the build gate and
the shape of this PENDING hand-over. `nifskope-ww-vanilla-compare` was read for
its "pin one camera across both halves and prove the pin took" section, which is
the thing this lane makes possible.

**Amended, in BOTH trees** (`E:\Projects\Claude\.claude\skills\…` and
`<repo>\.claude\skills\…`, byte-identical, `cmp` clean):
`nifskope-ww-render-shot`. It carried two paragraphs that are now known to be
wrong and that were costing lanes real work — "`WW_RENDER_CENTER` /
`WW_RENDER_DIST` do not take on a GENERATED document" and "the pin is also lost
on the AXIS VIEWS … if a picture has to be framed on a detail, it has to be
ViewUser". Both were the same auto-fit, and the ViewUser exception was an
accident of an early return. They are replaced by a section that names the one
cause, gives the numbers, adds the new switches with their formulas, and — the
part that matters while this lane is PENDING — **says the fix is uncompiled and
how to tell** (`release/ww_camera_pin.log` exists after a run, or it does not).
The switch table gained `WW_RENDER_FOV`, `WW_RENDER_ORTHO` and `WW_RENDER_CLEAN`.

**Also amended:** the skill's advice to "verify it took: two distances must give
two different files" is now called out as insufficient, because two distances
DID give two different files under the inverse-square defect. The replacement is
to read `upp` out of the census and check a known extent.

**Declined, with the reason.** A separate skill for "make a headless renderer
metric". The procedure is one repo's hook and one class's camera; it belongs in
`nifskope-ww-render-shot`, which is where a lane meets it, and a second copy is
the thing that drifts. What DOES generalise — build the instrument, give it a
known answer, and distrust a result that agrees to within exactly the tolerance
— went into `ww-control-calibration`'s territory as this report's 5.1 rather
than as a new file; if it recurs outside this repo, 5.1 is the text to lift.

## 7. Build status: BUILD PENDING

The gate was checked **once**, at the end, as the brief directs, and both halves
were red:

```
$ ls scratchpad/cardfinal_20260909/DONE
ls: cannot access ...: No such file or directory
$ tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
NifSkope.exe   16364 ...
rc=0
```

and pid 16364 is another lane's card bake
(`…/meshes/Landscape/Trees/TreeBlasted04.nif --port 45917`, started 23:52:26),
not a stray of this one. Nothing was compiled, nothing was run on a new exe, no
harness was executed.

**Everything in sections 1 and 3.3 was measured on `release/NifSkope.exe`
2026-09-09 22:04:35**, the build that was already on disk, and is true of the
defect rather than of the fix. Sections 3.1, 3.2 and 4 are predictions and are
labelled as such.

The resume is `scratchpad/hookcam_20260909/PENDING.md`: qmake before make
(`glview.h` gained members), then `tests/spells/render_shot.sh`, then
`transition.py`, then the four documents.

**His open NifSkope will need a restart** once this is built.

---

## Build (BUILD3)

Appended by lane BUILD3, 2026-09-10. Nothing above was rewritten. Nothing was
committed (CONSTITUTION 8).

### B.1 The build

`tasklist | grep -i -E "Fallout4|NifSkope"` printed `rc=1` at 00:09:43, before
the build, and again before every launch below. Lane CARDFINAL's `DONE` marker
was on disk (00:07). qmake first, because `src/glview.h` gained members:

    QMAKE-RC=0
    BUILD-RC=0
    release/NifSkope.exe  00:13:19
    release/style.qss     00:13:21   (cmp against res/style.qss: in step)

Zero `error:` lines. 27 objects recompiled -- every translation unit that
includes `glview.h`, plus `lodtfile.o` / `nifcli.o` / `btdterrain.o` for the
header lane WATER2 changed.

**`src/nifskope_ui.cpp` now carries BOTH edit sets in one link:** this lane's
three render-hook hunks (written 23:44:06) and lane CARDFINAL's bake changes.
The 23:41:00 exe carried only CARDFINAL's. Neither was touched to make them fit.

**Lane WATER2 rode along.** Its `src/lodtfile.h` (00:09:52) and
`src/lodtfile.cpp` (00:10:06) were on disk when the build started and compiled
clean -- no error, no warning attributable to them. Its `src/nifcli.cpp` (the
`lodl` subcommand) and `src/btdterrain.cpp` (the plane list) were still at their
committed content at 00:13 and are **not** in this exe; if WATER2 has written
them since, it needs its own build.

Every artefact's mtime in one table (CONSTITUTION 4):

| file | mtime | lane |
|---|---|---|
| `src/lodgen.cpp` | 2026-09-09 23:29:31 | CARDFINAL |
| `src/glview.cpp` | 2026-09-09 23:43:00 | HOOKCAM |
| `src/nifskope_ui.cpp` | 2026-09-09 23:44:06 | HOOKCAM + CARDFINAL |
| `src/lodtfile.h` | 2026-09-10 00:09:52 | WATER2 |
| `src/lodtfile.cpp` | 2026-09-10 00:10:06 | WATER2 |
| `src/glview.h` | 2026-09-10 00:02:19 | HOOKCAM |
| `tests/spells/render_shot.sh` | 2026-09-09 23:53:46 | HOOKCAM |
| **`release/NifSkope.exe`** | **2026-09-10 00:13:19** | this build, sha256 `2a9d72b6c236fbb2...` |
| `scratchpad/cardfinal_20260909/cards_perframe` | 2026-09-09 23:49-23:53 | CARDFINAL |
| `transition_*.png` | 2026-09-10 00:19:41 / 00:20:15 / 00:20:49 | this lane |

The exe is newer than every one of the nine changed files under `src`, `res`,
`tools` and `tests`, checked by sweep and not by the file I care about; and no
`.o` of any translation unit including `glview.h` or `lodtfile.h` is older than
its header.

### B.2 The camera: section 7 is 27 of 27, and every prediction held

`tests/spells/render_shot.sh`: **82 checks, 1 failure**. Section 7 -- the whole
of this lane's gate -- is entirely green. Viewport read back from the PNG:
1507x421, as predicted.

| check | pre-registered | measured |
|---|---|---|
| control, no camera switch, `arm=unpinned` | 230.98 px | **231.0** |
| ortho half-width 1024, eye 500 / 1000 / 2000, views Front (5) and Right (4) | 376.75 px, all six | **376.91**, all six, `upp=1.358991` in each |
| ortho half-width 2048 | 188.38 px | **188.82** |
| ortho half-width 4096 | 94.19 px | **94.50** |
| perspective fov 60, eye 500 | 765.06 px | **765.01** |
| perspective fov 60, eye 1000 | 250.91 px | **251.0** |
| perspective fov 60, eye 2000 | 107.04 px | **107.0** |
| look-at moved 400 units at half-width 1024 | 294.34 px | **294.36** |
| census `upp` against the arithmetic | equal | 1.358991 / 1.358991 |
| census viewport against the PNG | equal | 1507x421 / 1507x421 |
| perspective `upp` moves with distance | 1x / 2x / 4x | 1.371378 / 2.742757 / 5.485513 |
| orthographic `upp` does NOT move with distance | equal | 1.358991 at eye 500 and at 2000 |
| the two look-at files differ | differ | `ea644af68f880bcb` / `726c6a81765ba1fd` |
| `WW_RENDER_ORTHO=-5` refused by name | named | `arm=center/dist/fov/refused-WW_RENDER_ORTHO-not-positive/view`, `persp=1` |
| two runs of one pinned camera | identical | `ea644af68f880bcb` twice |
| the pin on a generated `.lodl` | holds | `upp` 53.085601 at half-width 40000, 106.171201 at 80000 |

The look-at row is the one that was RED on the defect -- two look-ats 400 units
apart produced one file, md5 `fff710bd...`. It is green, and green with the
right number: 294.36 px measured against 400 units / `upp` = 294.34 px.

`release/ww_camera_pin.log` exists after a run, which is the tell the skill
tells lanes to check.

### B.3 The one failure, and it is not the camera

    FAIL and the pixel sampler CAN see a window's pixels
         luminance range 169.847 over 56 samples (bar 295.737)

Section 6's floor for lane OFFSCREEN2's visible control. The bar is
`max(3 * desktop_noise, 15)`, and section 0 measured the desktop noise at
**98.579** at 00:14:24 -- a transient. The same region, the same
`screen_watch.ps1`, the same 200x200 at 2120,200, re-run at 00:17 with nothing
running: **range 0.111** over 110 samples, min 19.677, max 19.788. The skill's
own table gives 0.2 for an untouched region, so section 0's sample was ~500x
resting.

The discriminators, all in the same run: every hidden run measured 0.111-0.233
against that inflated bar and passed; the strobe control, which is on a FIXED
bar of 30, measured 251.314 and passed; `hidden and visible photograph the same
pixels` is green (`fedab869884174fb` both), so the `showCursor` change this lane
made did not reach section 6, which is what this lane's PENDING note flagged as
the thing to suspect.

Reported as a number. **Not re-pinned and not fixed** -- a resuming lane
measures the cause and stops. Entered in `MISTAKES.md`: one bar cannot serve a
floor and a ceiling, because noise moves both the same way and should move them
opposite ways.

### B.4 The reached suite

Run sequentially, one instance at a time, on the 00:13:19 exe.

| harness | result | why it was run |
|---|---|---|
| `render_shot.sh` | 82 checks, **1 failure** (B.3) | the hook this lane changed |
| `lodgen_octahedral.sh` | 85 ok, 0 fail | drives the hook's sibling bake block |
| `lodgen_impostor_cards.sh` | 12 ok, 0 fail | same |
| `lod_generation.sh` | 97 checks, 0 failures | `src/lodgen.cpp` is in this link |
| `lodl_open.sh` | 23 checks, 0 failures | reaches `lodtfile.h`/`.cpp` (WATER2) |
| `btd_terrain.sh` | 13 checks, 0 failures | same |
| `lodgen_identity.sh` | 8 ok, 0 fail | the byte-identity floor |

The three that print `RESULT PASS` and no count had their `ok` lines counted
rather than reported blank. All three match lane CARDFINAL's counts on the
23:41 exe exactly (85/12/8), which is the evidence that this build did not move
the bake.

Not run, with the reason: everything else in `tests/spells`. Nothing in this
link touches the block viewer, the panels, the collision or the NIF writers.

### B.5 The transition render: the pin held, the test cannot be met in this scene

**Library used: `scratchpad/cardfinal_20260909/cards_perframe`** -- lane
CARDFINAL's fresh per-frame library, baked on the 23:41 exe, 20 sidecars, all
three trees present. It was passed explicitly as `argv[1]`; the script's own
first candidate (`cardfinal_20260909/cards`) does not exist, so left to itself
it would have fallen back to `cardfit_20260909/cards_after` as this lane's
section 4 predicted. The report's fallback is therefore superseded: the fresh
library was used.

All three chunks baked (`mesh`, `card`, `ctl`, one `Commonwealth.4.-20.24.BTO`
each), the control library was written with `center` zeroed in 20 sidecars, and
all eighteen frames were shot. **The pin held exactly in every one of them**:

| tree | distance | `upp` measured | `2*tan(30)*eye/421` |
|---|---|---|---|
| `0003a28b` | mid 1873.95 | 5.140 | 5.140 |
| `0003a28b` | ring 7495.80 | 20.559 | 20.559 |
| `0004a074` | mid 1457.66 | 3.998 | 3.998 |
| `0004a074` | ring 5830.63 | 15.992 | 15.992 |
| `00038599` | mid 766.78 | 2.103 | 2.103 |
| `00038599` | ring 3067.13 | 8.412 | 8.412 |

And the gate is **UNMET**: 0 of 6 card rows and 0 of 6 control rows pass.

| tree | dist | arm | texel px | centre off, px | in texels | dx ext | dy ext |
|---|---|---|---|---|---|---|---|
| `0003a28b` | mid | card | 3.21 | 186.52 | 58.1 | 31.96% | 22.38% |
| `0003a28b` | mid | ctl | 3.21 | 186.52 | 58.1 | 31.96% | 22.38% |
| `0003a28b` | ring | card | 0.80 | 150.72 | 187.9 | 37.21% | 45.50% |
| `0003a28b` | ring | ctl | 0.80 | 162.88 | 203.1 | 39.61% | 38.50% |
| `0004a074` | mid | card | 2.03 | 48.79 | 24.1 | 4.72% | 26.81% |
| `0004a074` | mid | ctl | 2.03 | 207.33 | 102.4 | 47.82% | 26.81% |
| `0004a074` | ring | card | 0.51 | 316.00 | 624.2 | 70.28% | 37.84% |
| `0004a074` | ring | ctl | 0.51 | 317.22 | 626.6 | 70.28% | 42.23% |
| `00038599` | mid | card | 0.84 | 565.00 | 669.9 | 27.09% | 0.00% |
| `00038599` | mid | ctl | 0.84 | **12.50** | 14.8 | 12.25% | 0.00% |
| `00038599` | ring | card | 0.21 | 196.00 | 929.5 | 30.04% | 0.00% |
| `00038599` | ring | ctl | 0.21 | 152.50 | 723.2 | 23.37% | 0.00% |

Two rows say the numbers are not about card placement. On `0003a28b` mid the
card and the control are IDENTICAL to two decimals, when they differ by the
whole centre offset. On `00038599` mid the CONTROL is 45x closer than the card.
A control beating the thing it is the floor for is the signature of an
instrument that is not measuring what it names.

**The pictures say why, and the arithmetic confirms it.** All three were opened
(CONSTITUTION 5). None of them contains a single tree: the frames are a thicket
of dead trunks over terrain in the mesh arm, and in the card arm the camera is
INSIDE a neighbouring card -- flat untextured quads filling the whole frame.

Measured, per tree, from the bake's own manifest:

| tree | ref | half-extent | camera distance | nearest other ref | that neighbour, off axis | vfov needed to fit the subject | vfov allowed to exclude the neighbour |
|---|---|---|---|---|---|---|---|
| `0003a28b` | 674 | 1055.3 x 1055.3 | 1874 | 892 | 25.5 deg | **58.8 deg** | **15.1 deg** |
| `0004a074` | 109 | 518.2 x 829.0 | 1458 | 726 | 26.5 deg | **59.3 deg** | **15.8 deg** |
| `00038599` | 264 | 113.5 x 454.1 | 767 | 644 | 40.0 deg | **61.3 deg** | **26.4 deg** |

There is **no field of view that does both**, on any of the three trees, at
either distance, and the gap is a factor of 2.3-3.9. This lane's own PENDING
note offered "re-run with a smaller FOV constant if a frame is crowded"; that
route is closed, and the number above is why. It is not the 1507x421 clamp
either: the mid and ring distances are defined as multiples of the card's OWN
half-extent, so the subject fills a ~60 degree frame by construction whatever
the aspect, while the neighbours sit at 25-40 degrees.

So the finding is about the SCENE, not the pin and not the cards: the most
isolated instance of each of these trees in the placed Sanctuary chunk is not
isolated enough to be photographed alone at the distance at which it turns into
a card. A one-texel transition measurement needs a scene containing ONE ref.
That is a design change to the test and **it was not made here** -- a resuming
lane measures the cause and stops.

**Nothing about the card placement is claimed either way by this round.** The
geometric gate lane CARDFIT3 and lane CARDFINAL shipped
(`scratchpad/cardfinal_20260909/transition_bounds.py`, PASS on 3 bases) remains
the only evidence on that question, and it is a discrimination test, not a
one-texel test, exactly as it says.

Pictures, all three opened and all three showing the thicket:

* `scratchpad/hookcam_20260909/transition_0003a28b.png` (TreeHero01, 1874 units)
* `scratchpad/hookcam_20260909/transition_0004a074.png` (TreeMapleForest2, 1458 units)
* `scratchpad/hookcam_20260909/transition_00038599.png` (TreeBlasted01, 767 units)

Raw frames, camera censuses and `transition.json` in
`scratchpad/hookcam_20260909/trans/`.

### B.6 Documents

1. `WW_CHANGES.md` -- the **STATUS: NOT BUILT** block is replaced by the
   measured one: exe timestamp, the section 7 table, the one red with its cause,
   the suite, and the transition finding. Spliced in binary; CR count 19020
   before and after, asserted by the patch script.
2. `MISTAKES.md` -- two entries added at the top, both found by this build: the
   one-sided control that failed and proved nothing, and the shared noise bar
   that a noisy desktop can defeat. LF-only before and after.
3. This report -- appended, nothing above rewritten.
4. `nifskope-ww-render-shot` -- see B.7.

Uncommitted files in the tree at 00:28 on 2026-09-10: 22 modified, 12
untracked -- the tree is moving, lanes WATER2 and CLAMP are alive in it.
Nothing was committed by this lane.

### B.7 Skill review

**Loaded and used, all four the brief named.** `nifskope-ww-resume-pending` --
the read order, qmake-before-make, the exe-newer sweep over EVERY changed file
(which is what caught that nine files and not one had to be newer), the
sequential harness chain with its summary echo, and section 6, "when a gate
fails, measure the cause and STOP", which is the whole shape of B.3 and B.5.
`nifskope-ww-build-verify` -- make's own exit code as the gate, the stylesheet
`cmp`, and "a successful build is not a consistent one", the object-vs-header
sweep. `nifskope-ww-render-shot` -- the switch table and the `upp` arithmetic
that B.5's census column is checked against, and its instruction to verify the
pin by reading `upp` rather than by comparing two files, which is exactly the
check that would have caught the old defect. `ww-texel-picture` -- section 5,
"open the picture before reporting it", which is the only reason B.5 says what
it says instead of "the cards are 600 px out of place".

**The amendment `nifskope-ww-render-shot` still needs.** Its camera section now
opens with *"The pin is CODE ON DISK, not yet compiled as of the 22:04:35 exe of
2026-09-09"*. That is stale as of 00:13:19 and it is the sentence a lane reads
first. It should say the pin is BUILT, name the exe, and carry section 7's
numbers as the known-good values. Lane HOOKCAM wrote that paragraph and owns the
file; this lane leaves it rather than editing another lane's document mid-flight,
and lists it here as owed. **It must land in BOTH trees**, byte-identical.

**A skill this lane wished existed, and the case for writing it.** There is no
skill for "resume a pre-registered gate whose numbers are already written down".
Three separate times this round the right move was to compare a measured number
against a number someone else had committed to in advance, and decide whether a
miss is the code, the instrument or the environment -- and each time the
procedure was re-derived: find the discriminator that separates the three, run
it, and only then write a verdict. The discriminators used here (a fixed-bar
sibling check that passed beside a derived-bar check that failed; a control that
beat the thing it floors; a census value that matched the arithmetic to four
figures while the picture was wrong) are a generalisable set. It is a real
candidate, and it is DECLINED for now for one reason: it is one step away from
`nifskope-ww-resume-pending` section 6, which already says "measure the cause and
STOP", and a second document that says the same thing with more words is the one
that drifts. What this round adds is a worked example, not a procedure, so B.3
and B.5 are the text to lift if it recurs.

**Declined, with the reason.** A skill for "run the transition render". The
script is one lane's, it is run once, and the thing that will recur is not the
running of it but the geometric feasibility check in B.5 -- which belongs in the
script as its first refusal, not in a skill.

### B.7a The tree moved on after this build

Measured at 00:29:43, so that nobody quotes the 00:13:19 exe as current: lanes
WATER2 and the terrain lane have written since it linked --
`src/lodgen.cpp` 00:20:47, `src/btdterrain.cpp` 00:23:51, `src/lodtfile.cpp`
00:25:38, `src/lodtfile.h` 00:26:04, `src/nifcli.cpp` 00:29:43, and a new
**WRITTEN, NOT BUILT** entry at the top of `WW_CHANGES.md`. The exe-newer
sweep in B.1 was true at 00:13-00:21, which is the window every number in this
section was measured in, and it is no longer true now. This lane did not
rebuild: those files are other lanes' and building them is their step.

### B.8 Owed to bungo

* his open NifSkope, if any, is on the old exe -- **it needs a restart** to get
  the 00:13:19 build (nothing was open during this lane; `rc=1` throughout);
* the one-texel transition picture he asked for is **not delivered**: three
  pictures exist and all three show a thicket, for the measured reason in B.5.
  What is owed is a single-ref scene to shoot it in;
* the `nifskope-ww-render-shot` amendment in B.7, in both trees;
* the section 6 bar in `render_shot.sh`, which will go red again on any noisy
  desktop until the visible control gets a fixed bar.
