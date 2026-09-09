---
name: nifskope-ww-render-shot
description: Photograph a NIF, .bto or .btr headlessly through NifSkope Wild Wasteland Edition's render hook (WW_RENDER_SHOT and its WW_RENDER_* switches, WW_LOD_CHANNEL for the generated vertex channels), one instance at a time and never a desktop capture -- for showing bungo a bake (tree sway, AO, identity, terrain wetness) or pixel-diffing a shader change. A headless run is INVISIBLE (window opacity 0) and never on the primary monitor; read the first section before running anything that repaints in a loop. Use whenever a picture of rendered geometry is the deliverable or the gate; never screen-capture the desktop.
---

# NifSkope WW: headless renders

The hook lives in `src/nifskope_ui.cpp` near "RENDER REGRESSION BASELINE". One NifSkope instance
at a time; each run needs an unused `--port`. The app exits by itself after the grab.

```bash
WW_RENDER_SHOT="<out.png>" WW_RENDER_SIZE=1600x1000 WW_RENDER_VIEW=0 WW_RENDER_TIME=3 \
  timeout 120 release/NifSkope.exe --port 42321 "<file.bto>" >/dev/null 2>&1
```

## Headless runs and the screen: WHAT IS TRUE AS BUILT (2026-09-09, lane OFFSCREEN2)

**READ THIS BEFORE ANYTHING THAT REPAINTS IN A LOOP.** bungo gave two rules and
both hold for every headless run, measured on `release/NifSkope.exe` 19:35:14:

* *"Agent is launching nifskope on my main monitor, which is a no no"*
* *"the screen is flashing white and black, that's a view hazard for epileptics"*

A headless run -- `NifSkope::wwHeadlessRun()`, i.e. ANY `WW_*` variable in the
environment, or `-no-gui` -- now has its window **un-maximised, placed on a
non-primary screen, shown without activating, and shown at WINDOW OPACITY 0**.
It is deliberately still ON a screen. `tests/spells/render_shot.sh` is the gate:
**55 checks, 0 failures**, and its floors all fire in the same run.

* **The bake still strobes; nothing of it reaches an eye.** The two-pass matte is
  how the alpha is measured (nine repaints per octahedral view, 580 per model at
  OCT=8). Measured: a hidden 4x4 bake moved the second monitor's own luminance by
  **0.233**, which is what that region does with nothing running; the same bake
  made visible moved it by **246.8**.
* **The pixels are unchanged.** Same build, same scene, opacity 0 vs opacity 1:
  the render PNG is byte-identical and so are the card sheets -- on the cube
  fixture and on `TreeMapleForest02.nif` (card-set hash `6db4e814...` both ways).
* **`WW_WINDOW_AT` decides where it goes again**, for hidden runs AND for the
  visible control -- but only if the point is NOT on the primary screen. If it
  is, the placement refuses it and falls back to the first non-primary screen,
  saying so in the log's `arm=` field. `_harness.sh` exports `1960,40`.
* **`WW_WINDOW_VISIBLE=1` makes the window opaque** for watching one run. It does
  NOT move it to the primary. Use it sparingly on a bake: that is the strobe.
* **`release/ww_headless_windows.log`** is the only honest answer to "did that run
  show anything". One line per top-level window at show, at each grab, at the
  bake matte and at the sheet:
  `<when> <class> geom=... visible=0|1 onscreen=0|1 onprimary=0|1 opacity=0.00 maximised=0|1 arm=<...>`.
  `onprimary=1` or a mapped record at `opacity` other than `0.00` is the defect.
  `onscreen=1` is EXPECTED and required -- see the next paragraph.

### Two dead ends, so nobody pays for them again

* **Moving the window off every screen does not work.** As first written it was
  inert: `restoreUi()` restores a MAXIMISED window and on Windows `move()` on a
  maximised window only chooses which monitor it maximises onto. Un-maximise
  first and it does leave the desktop -- and then **nothing renders**. `GLView`
  is a `QOpenGLWindow`; a surface that is never exposed never gets a context, so
  `grabFramebuffer()` returns a null image. Measured: no PNG from
  `WW_RENDER_SHOT`, no card image from `WW_IMPOSTOR_BAKE`, both exiting 0, the
  bake's `.txt` sidecar written as usual. An off-screen bake writes EMPTY card
  sets and reports success.
* **Placing the window in `createWindow` alone is too late.** An `EnumWindows`
  probe at 25 ms caught a 426x306 OPAQUE window of the process on the PRIMARY at
  t=371 ms (class `Qt6111QWindowIcon`, the app title, no filename), gone by
  t=1403 ms and replaced by class `Qt6111QWindowOwnDCIcon` at the asked-for place
  with layered alpha 0. Qt's Windows plugin picks the window class by whether the
  surface needs its own DC, so realising the GL container destroys the first
  native window and creates a second -- and the first was created while the
  widget still had Qt's default geometry. The placement therefore runs in the
  `NifSkope` CONSTRUCTOR before `setupUi`, again in `createWindow` after
  `restoreUi` (which overwrites it), and again from the application event filter
  on `QEvent::Show` for every other top-level window.

### Measuring "nothing was on the screen" without fooling yourself

Three instruments, and the first two only echo what Windows was TOLD:

1. the process's own `ww_headless_windows.log` (`onprimary`, `opacity`);
2. an outside sampler over the process's windows. **Enumerate with
   `EnumWindows`; never `Get-Process().MainWindowHandle`** -- it returns ONE
   handle by a heuristic and cannot support "there was no other window". Read the
   layered alpha back with `GetLayeredWindowAttributes` (LWA_ALPHA is `0x2`; not
   layered means opaque), and record the class and title so a hit is NAMED;
3. **the desktop's own pixels.** A layered alpha of 0 is a claim about an API; a
   GL child window that presented over its parent would satisfy it and still
   strobe. Sample a region of the second monitor every ~50 ms, scale to 8x8, take
   the mean luminance, and use the RANGE. Measured amplitudes on this machine:
   0.2 the region left alone, 5.4 a console printing into it, 27.4 an opaque
   window appearing, 250 the matte strobing. A bar of 15 separates them.

Every one of those needs a FLOOR beside it or its zero means nothing: the
`WW_WINDOW_VISIBLE=1` control must be SEEN by all three, and a visible
`WW_IMPOSTOR_OCT=4` bake must be seen alternating. On 2026-09-09 the outside
sampler silently wrote nothing for a whole run (a PowerShell case-insensitivity
bug, `$b` clobbering `$B`) and only that floor caught it.

Writing a new `WW_*` hook: it inherits all of this for free through
`wwHeadlessRun()`. If it repaints in a loop, call
`wwLogTopLevelWindows( "<stage>" )` once before that loop so the gate can see
where the window was.

One route still walks back onto a monitor on purpose: `WW_GIZMONUM_TEST` moves
the window under the real mouse pointer, because `QCursor::setPos` is a no-op for
a process that is not foreground. It renders no matte, and it is at opacity 0
like everything else.

| switch | meaning |
|---|---|
| `WW_RENDER_SHOT=<png>` | grab the GL framebuffer (`grabFramebuffer`, not a widget grab) and quit |
| `WW_RENDER_SIZE=WxH` | framebuffer size |
| `WW_RENDER_VIEW=n` | `GLView::ViewState` index: 0/unset Front, 1 Top, ...; negative keeps the startup camera |
| `WW_RENDER_TIME=s` | scene time for time-driven particles (default 1.0) |
| `WW_RENDER_FLAT=1` | vertex colours only, no textures, no lighting |
| `WW_LOD_CHANNEL=1..7` | the LOD preview channel (below); only meaningful on a `.bto`/`.btr` |
| `WW_RENDER_CENTER=x,y,z` / `WW_RENDER_DIST=d` | frame a point instead of the whole scene (verify it took: two distances must give two different files) |
| `WW_RENDER_REFRACTION=0` | switch refraction off |

Channels: 1 identity hashed colour per object, 2 identity raw R+G, 3 ambient occlusion (B),
4 tree sway / shore proximity (vertex alpha), 5 terrain material class (R, hashed), 6 terrain
wetness (G), 7 water depth (R), 8 geometric normal in view space (half-packed), 9 window depth,
10 the material's smoothness (normal map alpha), 11 the shape's alpha-test flag (1 = alpha-tested).
Channels 8 and 9 keep texturing ON (no WW_RENDER_FLAT) or the leaf cards' alpha test stops cutting
and every card photographs as a solid quad. The impostor bake (`WW_IMPOSTOR_BAKE=<dir>
WW_IMPOSTOR_OCT=N WW_IMPOSTOR_TILE=px`) uses them per view; camera per view =
`setRotation( -90 + elevation, 0, 90 - azimuth )` after one `setOrientation( ViewFront, true )`. Terrain chunks reach the channel program only because
`setupProgram` routes FO4 shapes to `fo4_default.prog` while the preview is on (Shader Type 18 is
otherwise excluded) -- `tests/spells/lod_channel_preview.sh` is the gate.

Object chunks: `lodgen <esm> --worldspace 3C --objects X Y --dim 4 --data-root <unpacked> -o out.bto`
(the CLI never initialises the game manager, so it needs the loose root). The near Sanctuary
chunk (-20,24) at dim 4 places 678 objects with maple trees; the far chunk (-32,16) at dim 16 is
EMPTY without `--impostors`. The manifest beside the chunk lists each object's world position.

A whole chunk framed front-on is a thin band; render large (3200x1000) and crop, or frame with
CENTER/DIST once that is verified to take. Compare a normal render and a channel render of the
same framing side by side; the channel render alone tells a person nothing.

## Driving many renders from a script (2026-09-09, lane LODTOPEN3)

Four traps, each of which cost pictures or a wrong conclusion in one session.

* **A conditional environment variable goes through `env`, never a bare
  `${flag:+NAME=1}` prefix.** Bash decides which words are assignments BEFORE
  expansion, so the expanded word lands in COMMAND position and the run dies
  with **rc 127**, writing no file. Nine plane renders were lost that way.
  Write `env ${flat:+WW_RENDER_FLAT=1} WW_RENDER_SHOT="$out" ... timeout 900
  "$NS" --port "$p" "$file"`.
* **Every shot function prints the size of the file it just wrote** (or
  `NO FILE`). An exec failure is silent otherwise -- `>/dev/null 2>&1` hides
  the shell's own error, and a loop of eleven renders looks like it ran.
* **`WW_RENDER_SIZE` is clamped to one screen.** Asking for 1400x1400 on this
  machine gives **1507x1067**, and so does asking for 360x360. The hook says so
  itself near `src/nifskope_ui.cpp:21404`. Never quote a requested size as the
  picture's size -- read it back with PIL.
* **`WW_RENDER_CENTER` / `WW_RENDER_DIST` do not take on a GENERATED document**
  (a `.btd` or `.lodt` opened through the terrain route). Measured: the same
  file at distances 90000 and 140000 produced one identical PNG. Candidate
  cause, unproved: `load()` reframes on the new contents after generating the
  scene (`src/nifskope.cpp`, `ogl->setOrientation( ogl->viewState(), true )`).
  The skill's own two-distance verification is what catches this -- run it
  BEFORE trusting any framing, and if the pin fails, earn "same camera" another
  way: identical scene geometry plus the renderer's determinism (two
  independent runs of one scene are byte-identical, 0 of 194,565 terrain pixels
  differing).

* **The pin is also lost on the AXIS VIEWS, on an ordinary parsed file**
  (2026-09-09, lane IMAGES5). Measured on one `.BTO`, with a control on each
  side: `WW_RENDER_VIEW=8` (ViewUser), one centre, `WW_RENDER_DIST` 3000 vs
  60000 -> 8,723 B vs 10,983 B, the pin TAKES; the same file on
  `WW_RENDER_VIEW=3/5/4` (Left/Front/Right) with the centre AND the distance
  both changed -> 11,277 / 11,598 / 10,801 bytes, unchanged across two runs,
  the pin does NOT take. So the rule is not "generated documents only": if a
  picture has to be framed on a detail, it has to be ViewUser, and a
  three-direction picture of one object cannot be framed at all. No cause
  claimed -- the hook applies `setPosition` / `setDistance` after
  `setOrientation( view, true )` in both cases (`src/nifskope_ui.cpp:21575`).

## The run hangs and writes nothing: the Save Confirmation dialog (2026-09-09, lane NOPROMPT)

**A headless run that sits until its `timeout` and produces no file is usually not
a slow render. It is a modal question with nobody to answer it.** bungo, after a
session of it: *"agents keep always hanging on save confirmation"*, over a box
reading "You have unsaved changes to TreeMapleForest3".

`qApp->quit()`, which every WW_* hook ends in, is not `exit(0)` on Qt 6.5+:
`QCoreApplicationPrivate::quit()` is virtual and `QGuiApplicationPrivate`
overrides it to CLOSE EVERY TOP-LEVEL WINDOW first. So the quit runs
`NifSkope::closeEvent`, which asks `saveConfirm()` whenever the document is
modified. Two routes leave it modified:

* **the impostor bake** (`WW_IMPOSTOR_BAKE`) writes into the loaded model —
  `LOD1/LOD2 Size` = 0 and the hidden flag on every `_L1`/`_L2` shape, so the
  in-cell detail steps do not stack up in the card. Any tree with those shapes
  (`TreeMapleForest3` is one) comes out dirty;
* **a generated terrain document** (`.btd`, `.lodt`) is BUILT, not parsed, and
  building it fires `dataChanged` → `setWindowModified`. It was born dirty.

`WW_RENDER_SHOT` and `WW_LOD_CHANNEL` write nothing into the model themselves.

**The fix, from 2026-09-09:** `NifSkope::wwHeadlessRun()` (the one predicate —
any `WW_*` environment variable, the same test `saveUi()` has used since
2026-07-27) guards the top of `closeEvent`, which then discards instead of
asking, and names what it discarded in `release/ww_headless_close.log`. A
generated `.btd`/`.lodt` also starts with a clean undo stack now. Gate:
`tests/spells/render_shot.sh`.

**Reading a hang, before and after:** `rc=124` from `timeout` with the PNG or
the cards already on disk is this dialog, every time. If it recurs on a build
that has the guard, check the log file: a line there means the guard ran and
the hang is something else; no line and no `WW_*` variable in the environment
means the run was not recognised as headless at all — which is what happens if a
switch is passed some other way than through the environment.

**A flat render of a view with no colour channel is a blank white sheet.**
`WW_RENDER_FLAT=1` draws vertex colours and nothing else, so a `.lodt` or
`.btd` height view -- which carries no colour channel at all, by design --
photographs as pure white. That is not a failed render; it is the same fact a
28-byte-vertex check asserts numerically. Render such a view LIT, and keep the
blank one only as evidence.
