---
name: nifskope-ww-render-shot
description: Photograph a NIF, .bto or .btr headlessly through NifSkope Wild Wasteland Edition's render hook (WW_RENDER_SHOT and its WW_RENDER_* switches, WW_LOD_CHANNEL for the generated vertex channels, WW_LODL_CHANNEL=<name> for the native .lodo/.lodi/.lodt far field's own baked channels), one instance at a time and never a desktop capture -- for showing bungo a bake (tree sway, AO, identity, terrain wetness) or pixel-diffing a shader change. A headless run is INVISIBLE (window opacity 0) and never on the primary monitor; read the first section before running anything that repaints in a loop. Use whenever a picture of rendered geometry is the deliverable or the gate; never screen-capture the desktop.
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

**Every `WW_*` output path is ABSOLUTE.** `WW_RENDER_SHOT` with a RELATIVE
path writes NOTHING and says nothing: the grab runs, `release/ww_camera_pin.log`
gets its `grab` line, the process exits 0, and `QImage::save` fails silently
because the hook ignores its return. The tell is a `grab` line in that log with
no file on disk. Two Charles renders were lost to it (2026-09-10, lane BUILD5b).
The same holds for `WW_IMPOSTOR_BAKE`, `SHOT=` and `WW_WATER_MARK_SHOT`.

**And so is every INPUT path -- argv included** (2026-09-10, lane BUILD7). A
Git-Bash parent gets NO MSYS2 argv/environment conversion and `_harness.sh`'s
`winpath()` only rewrites the `/e/...` form, so a REPO-RELATIVE path reaches the
app as a file it cannot open, and nothing says so. The two symptoms, both
measured in one session:

* the NIF in argv -- `release/NifSkope.exe --port N fixtures/x.nif` -- opens a
  scene with **one unnamed node**. `tests/spells/hkxanim_play.sh` then reported
  "0 bones matched (expected 78)", 8 failures of 27, none of them the code's;
* `WW_HKXANIM_CLIP=scratchpad/.../jog.hkx` loads no clip and the hook throws the
  refusal away, so all four of gate (e)'s renders come out as the BIND POSE --
  which is that gate's own refuter for "the pose never reached the rig".

The tell for the first is a harness log line naming a node COUNT that cannot
belong to the file you think you opened. Write every path
`E:/Projects/.../file`, or run it through `winpath` first.

| switch | meaning |
|---|---|
| `WW_RENDER_SHOT=<png>` | grab the GL framebuffer (`grabFramebuffer`, not a widget grab) and quit |
| `WW_RENDER_SIZE=WxH` | framebuffer size |
| `WW_RENDER_VIEW=n` | `GLView::ViewState` index: 0/unset Front, 1 Top, ...; negative keeps the startup camera |
| `WW_RENDER_TIME=s` | scene time for time-driven particles (default 1.0) |
| `WW_RENDER_FLAT=1` | vertex colours only, no textures, no lighting |
| `WW_LOD_CHANNEL=1..7` | the LOD preview channel (below); only meaningful on a `.bto`/`.btr` |
| `WW_RENDER_CENTER=x,y,z` | the world look-at point. PIN, not a hint -- see the camera section below |
| `WW_RENDER_DIST=d` | the EYE's distance to that point, world units. It used to mean the ortho half-height and never took effect |
| `WW_RENDER_FOV=deg` | full VERTICAL field of view; forces perspective |
| `WW_RENDER_ORTHO=w` | orthographic, `w` = half-WIDTH in world units. The metric arm: an extent of E units spans `E * viewportWidth / (2 * w)` px |
| `WW_RENDER_CLEAN=1` | the model and nothing else: no grid, no axes, no node markers, no 3D cursor |
| `WW_RENDER_REFRACTION=0` | switch refraction off |

**Particles: the `MPS*` effect NIFs photograph EMPTY in this viewer** (measured lane PBRR0,
2026-09-24). `MPSFireSmall01.nif` and `MPSSmokeFireMed01.nif` are BSMasterParticleSystem
children fed by `BSPSysMultiTargetEmitterCtlr`, which `src/gl/controllers.cpp` never simulates,
so they hold 0 particles at every `WW_RENDER_TIME`. `AttachFXMist01.nif` draws according to the census but
reaches no pixel. A particle picture or pixel gate uses
`Meshes/Actors/CreateABot/CharacterAssets/ShockHAndLeft.nif` or `Meshes/Effects/CryoJet01.nif`,
which do draw. A byte-identical pair of empty frames guards nothing.

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

### The NATIVE far field has its OWN channel switch: `WW_LODL_CHANNEL=<name>` (2026-09-18, lane CHANVIEW1)

`WW_LOD_CHANNEL` above is the STOCK path: a numbered channel, on a `.bto`/`.btr`, through
`res/shaders/fo4_default.frag`. It does nothing on the native pair. The `.lodo`/`.lodi`/`.lodt`
far field carries different bytes, and they are viewed by NAME:

| name | what it paints | where the byte lives |
|---|---|---|
| `identity` | **the GROUP**, hashed to colour (the stock channel 1 palette). One colour a building, and the far-shadow route is keyed on exactly this | `.lodi` v7 group table (0x100); a v6 file has none, and the note line says the PLACEMENT served it |
| `placement` | the PLACEMENT identity, hashed the same way -- the **before** picture the join is read against | `.lodi` instance identity |
| `identityraw` | that identity's low byte as grey | `.lodi` instance identity & 0xFF |
| `sky` | per-placement sky visibility | `.lodi` byte 0x11 |
| `ground` | ground-contact blend, PLACEMENTS and TERRAIN in one ramp | `.lodi` byte 0x12 (terrain = the ramp at the surface, constant 255) |
| `seed` | tree seed hashed to colour, **0 = not a tree = black** | `.lodi` byte 0x13 |
| `sway` | per-vertex leaf sway | `.lodo` vertex byte 0x0E |
| `selfao` | per-vertex self-AO | `.lodo` vertex byte 0x0F |
| `ao` | **exactly what `WW_LODL_AO=1` has always drawn** | `.lodi` v6 vertex AO x placement AO; terrain = mask sheet B |
| `mask-r` | terrain roughness | `.lodt` role-5 sheet, R |
| `mask-g` | terrain metallic | role-5 sheet, G |
| `mask-b` | terrain sky AO | role-5 sheet, B |
| `mask-a` | terrain ground cover | role-5 sheet, A -- **a BC1 sheet has none**, and the switch says so |
| `emissive` | role-6 emissive sheet as base colour, **texturing ON** | `.lodt` role 6, absent on most bakes |
| `normal` | role-2 MSN sheet as base colour, **texturing ON** | `.lodt` role 2 |
| `scrappable` | **magenta** = the player can scrap this placement at a workshop and it will not be there; **grey** = it stays. Two colours and nothing between them, because the bit is one bit | `.lodi` **v9** instance flags 0x14 bit 6 (`0x0040`), written by `--scrappable` -- **NOT FLOWN**; measured by lane HORIZON3, kept by lane HORIZONOUT when the rest of that route was dropped |

Six things that decide whether the picture is worth taking:

* **`WW_RENDER_FLAT=1` for the fourteen flat-byte names, and NOT for `normal`/`emissive`.**
  Those two are textures; render them with texturing on and `WW_LOD_CHANNEL=12` (raw base
  colour, unlit, no tone map) or you photograph the lighting instead of the sheet.
* **The switch needs the pair AND the sheets pinned**: `WW_LODL_OBJECTS=<file.lodi>`,
  `WW_LODL_SHEETS=<dir with the .lodt>`, `WW_LODL_REGION=x0,y0,x1,y1,level`,
  `WW_LODI_LEVEL`, `WW_LODI_SLOT`, opened on the matching `.lodl`. A `.lodi` at v5 has no
  vertex-AO stream and no per-vertex self-AO, so `ao` and `selfao` come out empty on it --
  check the version before blaming the switch.
* **Read the note line, do not trust the render.** Every name prints
  `WW_LODL_CHANNEL=<name>: <what> from <file>, N placements|vertices|texels read; min, max,
  mean`, read back from what was UPLOADED. A terrain channel prints two lines over two
  populations -- the bilinear resample at the grid vertices (what the picture is made of)
  and the content-texel census over the decoded tiles (the file's own number). Quote the
  census when the report quotes the file.
* **`scrappable` on a file below version 9 is all grey, and the note line says WHY.** The bit
  only exists from `.lodi` v9, which only a bake given `--scrappable` writes (it ships OFF, and
  the default bake writes v7). An all-grey picture off a v7 file means THE FILE SAYS NOTHING,
  not that the settlement has nothing scrappable in it, and the switch prints the version rather
  than letting the picture be read either way. The read-back on this channel is 1s and 0s, so its printed **mean is the
  share** -- 0.0004 on the measured urban region (cells 0 -12 to 11 -1, dim 4), 14 of 33,123. A whole
  street coming back magenta means the build-area box test is wrong; that is what the picture is for.
  Gate: `tests/spells/lodgen_scrappable.sh`, whose G2 re-derives the same 14 out of `Fallout4.esm`.
* **A channel whose render is byte-identical to the default render is NOT WIRED.** Take the
  default of the same framing every time and count the differing pixels before captioning
  anything (root `MISTAKES.md` 05:1x). The two names a bake genuinely lacks invert that
  floor: they must be identical AND must say `ABSENT` by name.
* **A name with an ARGUMENT on it is refused whole.** No channel takes one any more --
  `horizonbin=<n>` was the only one, and it went with the baked-horizon route that bungo
  dropped on 2026-09-19 (lane HORIZONOUT). A stale `WW_LODL_CHANNEL=horizonbin=3` left in a
  script therefore draws NOTHING and is named in the note line, rather than quietly drawing the
  default picture and being captioned as a horizon (`src/lodinative.cpp:435`). `WW_SUN`,
  `WW_HORIZON_SOFT_DEG` and `WW_HORIZON_BIN_ROT` are gone with it; so is
  `tests/spells/lodgen_horizon.sh`. The far shadow is now keyed on the `identity` picture
  above, so THAT is the render to take when a shadow question comes up.

An unknown name is refused by the name given and lists the known names; it draws the default
scene, so a typo photographs as a perfectly good picture of nothing. Gate:
`tests/spells/lodl_channels.sh`.


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
* **`WW_RENDER_SIZE` is not a clamp -- it is `max(width, a floor)` by
  `height - 59`** (measured 2026-09-10, lane BUILD11, on `release/NifSkope.exe`
  17:08:39; three requests, every one read back with PIL):

  | requested | measured | exe |
  |---|---|---|
  | 1000x1000 | **1437x941** | 2026-09-10 15:52:46 |
  | 1000x1000 | **1293x941** | 2026-09-10 17:08:39 |
  | 1500x1059 | **1500x1000** | 2026-09-10 17:08:39 |
  | 1800x800 | **1800x741** | 2026-09-10 17:08:39 |
  | 1000x1000 | **1524x941** | 2026-09-11 07:06:04 (lane SKEL2) |

  **The corollary, and it cost lane SKEL2 a near-miss (2026-09-11): a SCREEN
  COORDINATE is never carried between builds either.** The brief named two
  marks in a picture by their pixel positions -- `(486,485)` and `(463,503)` in
  BUILD11's 1293-wide frame -- and the obvious route was to project the nodes
  and look for those two points. In the 1524-wide frame of the next build the
  same two nodes are at `(574,491)` and `(546,509)`: an orthographic camera's
  units-per-pixel is `2 * WW_RENDER_ORTHO / width`, so BOTH the scale and the
  centre move with the floor. Anything derived from the frame size -- a crop
  box, a pixel position, a "%% of the frame" -- is measured per build.

  The honest way to name a thing in a picture is to make the code that drew it
  say where it put it: lane SKEL2 added `WW_SKELOVERLAY_DUMP=<file>`, written
  inside the draw call, one row per drawn element with its projected position
  and the viewport size in the header, and matched the stray-pixel clusters
  against it with a refusal radius so a merely-nearest element is not named.

  The HEIGHT is exactly `requested - 59` every time -- the window chrome the
  framebuffer does not get. The WIDTH is honoured exactly when it is above the
  main window's own `minimumSizeHint`, and silently RAISED to that floor when it
  is not: the hook does `skope->resize( rw, rh )` (`src/nifskope_ui.cpp` ~21636,
  after `qMax( ..., 320 )` / `qMax( ..., 240 )`) and a `QMainWindow` will not go
  below its minimum. **The floor moves with the build** -- 1437 on the 15:52:46
  exe of 2026-09-10 (lane SKELFIX's pictures came out 1437x941 from a request of
  1000x1000 and it was logged as a defect), 1293 on the 17:08:39 exe after the
  workspace and bar-alignment work -- so it is MEASURED per build, never
  remembered. To get an exact WxH picture: ask for `W x (H+59)` with W above the
  floor, and read the result back. An older note said the size "is clamped to one
  screen" and that 1400x1400 and 360x360 both gave 1507x1067; that was the same
  two rules seen through one machine's screen size. Never quote a requested size
  as the picture's size -- read it back with PIL.

## THE CAMERA: what was wrong, and what it is now (2026-09-09, lane HOOKCAM)

**READ THIS BEFORE FRAMING ANY PICTURE ON A DETAIL, and check the exe date
first.** The pin is CODE ON DISK, not yet compiled as of the 22:04:35 exe of
2026-09-09 (lane HOOKCAM ended BUILD PENDING; resume
`scratchpad/hookcam_20260909/PENDING.md`). On an exe older than that build,
everything in "what was wrong" still applies and `WW_RENDER_FOV` /
`WW_RENDER_ORTHO` do not exist. `release/ww_camera_pin.log` existing after a run
is how you tell.

### What was wrong, and it was ONE cause

`GLView::center()` does not centre. It sets `doCenter` and asks for a repaint,
and the auto-fit -- `setCenter()`: `Pos = -bounds.centre`, `Dist = radius * 1.2`,
`Zoom = 1` -- runs inside the NEXT `paintGL`. `setOrientation( state, true )`
ends in `center()`, so the hook set the camera, one frame ran, and `paintGL`
replaced it.

* **`WW_RENDER_CENTER` did nothing at all on an axis view.** Look-ats `0,0,256`
  and `400,0,256` on a 512-unit cube produced two files with the same md5
  `fff710bd...`.
* **It "worked" on `WW_RENDER_VIEW=8` (ViewUser) by accident.**
  `setOrientation` opens with `if ( state == view ) return;`, and ViewUser is
  the startup view -- so on that one view it never queued the auto-fit. Lane
  IMAGES5's finding was real; the explanation ("no cause claimed", "it has to be
  ViewUser") was the symptom.
* **The GENERATED-DOCUMENT rule was wrong too.** A `.btd`/`.lodl` ignoring the
  pin is the same auto-fit, reached a second way: the document is BUILT and
  reframes after the hook has run.
* **`WW_RENDER_DIST` scaled as 1/D SQUARED.** Spans of that cube at DIST 400 /
  500 / 600 / 700 / 768 / 800 / 900 / 1000: 540.8 / 273.0 / 170.5 / 117.7 /
  95.0 / 87.0 / 67.0 / 53.7 px, which inverts to an eye distance of
  `want^2 / 532.8` -- and 532.09 is that fixture's own auto-fit distance. The
  read-back "fix" of lane CARDFIT3 is what produced the wrong law: the pump
  between the set and the read-back is the thing that destroys the camera.
* **The projection is PERSPECTIVE, 60 degrees, in every headless run** — with
  ONE exception since 2026-09-10, the impostor bake, which now asserts an
  orthographic camera itself (see the note below). Nothing else headless calls
  `setProjection`;
  `restoreUi()` hard-codes `isPersp = true`. `orthographicHalfHeight()` returns
  `Dist / Zoom` whatever the projection is -- the name is not a measurement.

### What it is once built

`GLView::WwCameraPin` cancels the queued auto-fit and re-asserts itself at the
top of every paint, so a later reframe cannot take the camera back. Armed only
by `WW_RENDER_CENTER` / `_DIST` / `_FOV` / `_ORTHO`; a `WW_RENDER_VIEW`-only
capture keeps exactly the old auto-fit framing, so old baselines do not move.

**A picture then carries a number.** `release/ww_camera_pin.log` (or
`$WW_CAMERA_CENSUS`) records, at the grab, `arm view rot lookat eye dist zoom
persp fov halfW halfH vp upp`. `upp` is the units per pixel AT THE LOOK-AT
PLANE, so an extent of E world units in that plane spans `E / upp` pixels:

    orthographic   upp = 2 * W / viewportWidth
    perspective    upp = 2 * tan( fov / 2 ) * eye / viewportHeight

Use `WW_RENDER_ORTHO` whenever a number is wanted: the scale is then exact and
independent of the eye distance. Use `WW_RENDER_FOV` + `WW_RENDER_DIST` when the
picture has to be what the game shows at a ring distance.

**Verify the pin took, every time, and not by comparing two distances.** Two
distances differed under the broken code as well. Read `upp` out of the census
and check the span of something whose size you know:
`tests/spells/render_shot.sh` section 7 is that check, on a cube written by the
app's own CLI (`-no-gui new --cube --size 512`), within 1 px.

### The impostor bake sets its OWN camera (2026-09-10, lane CARDORTHO)

`WW_IMPOSTOR_BAKE` is the one headless hook that does not inherit the 60-degree
perspective: it calls `setProjection(false)` before pass one, because every
extent it records is a world length read off viewport pixels through one
units-per-pixel constant and only an orthographic camera makes that true. Up to
2026-09-09 it did not, and every card sheet in the tree was DRAWN perspective
and MEASURED orthographic (MISTAKES.md; `docs/LODGEN_CARD_SHEETS.md` §3.7).

* the bake's sidecar names the arm that served — `projection ortho` /
  `projection persp` — read back off the live viewport, plus `orthofit <asked>
  <achieved> <persp 0|1>`. **A card library whose sidecars have no `projection`
  line is the older, foreshortened vintage**; re-bake it;
* `WW_IMPOSTOR_PERSP=1` restores the old camera exactly, and is the control the
  cube gate in `tests/spells/lodgen_octahedral.sh` (bake 4) must fail against;
* the bake does NOT read `WW_RENDER_ORTHO`. The pin and the bake are two
  cameras: the pin frames a picture, the bake fits each frame to a silhouette it
  measured itself. Setting `WW_RENDER_ORTHO` during a bake would arm the pin and
  fight the fit;
* `orthographicHalfHeight()` returns `Dist / Zoom` in EITHER projection. It is a
  name, not a measurement — never use it to decide which camera is running.
  `isPerspectiveProjection()` is the one that answers.

## The run hangs and writes nothing, second cause: NO FILE ON THE COMMAND LINE (2026-09-18, lane LODIV7)

**`WW_RENDER_SHOT` only arms when a file is on the command line.** The hook hangs
off `completeLoading`, and `src/nifskope_ui.cpp:22056` is explicit about it:

```cpp
if ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_RENDER_SHOT" ) )
```

Setting the variables is not the arming condition and never was. An exe launched
as `"$EXE" --port "$PORT"` with `WW_RENDER_SHOT`, `WW_LODL_OBJECTS` and
`WW_LODL_CHANNEL` all set and no scene **never loads, never renders, never quits,
and never prints a reason** -- and because the one-instance rule means the tree
holds exactly one NifSkope, that one wedged process blocks every later gate in
the run. It cost lane LODIV7 its pictures and its whole G4/G5 half.

The three parts of the fix, and all three belong in every driver that starts the
exe, not just the one that was bitten:

1. **Pass the scene.** For a native far-field picture that is the `.lodl`, and it
   is not optional. The `.lodi` goes in `WW_LODL_OBJECTS`, the sheets in
   `WW_LODL_SHEETS`, the window in `WW_LODL_REGION` -- but the document the exe
   OPENS is the `.lodl` and it is a positional argument.
2. **Wrap it in `timeout`.** `timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")"`.
   A stuck window is not a failed test, it is a failed TREE.

   As a precaution beside it -- not as a fix for anything measured -- **record the
   PID at launch and check after the wait that it is gone**: launch with `&`, keep
   `$!`, and afterwards look for the process by the `--port` you gave it, which is
   the only thing that tells your harness's window from bungo's. A driver that
   finds it still alive and cannot end it prints the PID and the port and stops,
   because the one-instance rule makes every later gate refuse anyway.

   **(An earlier version of this section claimed `timeout` had been measured to
   fire and be ignored by a Windows GUI process. That was wrong and is withdrawn;
   the process in question was launched by a stale shell with no `timeout` at all.
   See `MISTAKES.md` 2026-09-18 18:3x.)**

3. **A fix to this script does not reach a run already in progress.** bash reads a
   script incrementally, so a long-running `bash tests/spells/<gate>.sh` keeps
   whatever it has already parsed. Edit the file while the old shell is alive and
   that shell keeps launching the OLD, broken form -- and its output looks exactly
   like your fix failing. **Before trusting a fixed gate, check for a stale
   `bash <script>` by name**, not only for the exe it launched, and end it.

4. **Unwedging a harness NifSkope WITHOUT killing it: send it the scene over its
   own `--port`.** This is the tool of choice when process-kill is denied, and it
   is how two wedged windows were ended on 2026-09-18 (PID 24828 at 18:27:22, PID
   36512 at 18:30:13) with no kill and no permission prompt.

   `src/main.cpp`'s `IPCsocket` binds `127.0.0.1:<port>` when the exe is started
   with `--port`, and `IPCsocket::sendCommand` is nothing more than a UDP datagram
   carrying the command string's **raw UTF-16LE bytes** -- no length prefix, no
   terminator:

   ```cpp
   udp.writeDatagram( (const char *)cmd.data(), cmd.length() * sizeof( QChar ),
                      QHostAddress( QHostAddress::LocalHost ), port );
   ```

   `execCommand` acts on anything starting `NifSkope::open`, so the datagram to
   send is the string `NifSkope::open <absolute windows path>` encoded UTF-16LE.
   The process loads the file, `completeLoading` fires the `WW_RENDER_SHOT` hook,
   it writes its picture and quits on its own.

   **The picture it writes is NOT gate evidence** unless that process was started
   with the whole environment the gate specifies. A window unwedged this way had
   no `WW_LODL_SHEETS` and no `WW_LODL_REGION`, so what lands is a picture of the
   right file under the wrong conditions. Unwedge to free the tree, then re-run
   the gate properly.
5. **Assert the artefact after the launch**, in the driver:
   `[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"`. A harness
   whose output is a file must say in its log whether or not the file arrived.
   An absent line reads exactly like a passing one.

The shape that works, from `tests/spells/lodi_v7.sh`:

```sh
shot () {  # shot <tag> <channel> <lodi>
	WW_LODL_OBJECTS="$(winpath "$3")" \
	WW_LODL_SHEETS="$(winpath "$SHEETS")" \
	WW_LODL_REGION="$REGION" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
	WW_LODL_CHANNEL="$2" \
	WW_RENDER_SHOT="$(winpath "$OUT/$1.png")" \
	WW_RENDER_SIZE="$SIZE" WW_RENDER_CENTER="$CX,$CY,$CZ" \
	WW_RENDER_ORTHO="$ORTHO" WW_RENDER_VIEW="$VIEW" WW_RENDER_FLAT=1 WW_RENDER_CLEAN=1 \
	WW_WINDOW_AT=1960,40 \
		timeout 600 "$EXE" --port "$PORT" "$(winpath "$LODL")" > "$OUT/$1.log" 2>&1
	[ -s "$OUT/$1.png" ] || echo "  (no picture for $1, exit $?)"
}
```

**Telling the two hang causes apart without guessing:** the Save Confirmation
hang has a window with a message box on it; this one has a window with an empty
document, or no window at all yet. The log is the faster tell -- the dialog hang
has loaded the file and logged it, this one has logged nothing after startup.

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

## Photographing a terrain chunk against YOUR OWN baked sheets (2026-09-12, lane GROUND1)

A `.BTR`/`.BTO` names its textures by GAME path, so by default it photographs
with the INSTALLED sheets and a picture of a bake you just made is a picture of
vanilla. `WW_LODGEN_RESOURCES` is the way in: `src/main.cpp` reads it before the
GUI starts, hands it to `lodgenSetResources`, then puts those roots AHEAD of the
user's own Fallout 4 folders with `Game::GameManager::update_folders`. Session
only — his Settings > Resources is never written.

**The root is shaped `<root>/Textures/Terrain/Commonwealth/...`, NOT
`<root>/Data/Textures/...`** — even though the `.BTR` spells its textures
`Data\Textures\Terrain\Commonwealth\Commonwealth.4.-32.-20.DDS`. Rooted at
`Data` the lookup silently misses and the render falls back to the installed
sheets: it exits 0, writes a PNG, and the before/after pair comes out
**byte-identical**. That is the tell, and it is the only one.

```bash
WW_LODGEN_RESOURCES="$(cygpath -w "$W/bake_on/res")" \
WW_RENDER_SHOT="$(cygpath -w "$W/shot.png")" WW_RENDER_VIEW=1 WW_RENDER_SIZE=900x900 \
WW_RENDER_CENTER="8192,8192,0" WW_RENDER_ORTHO=5000 WW_RENDER_DIST=30000 \
WW_WINDOW_AT=1920,0 \
  timeout 180 release/NifSkope.exe --port 42611 "$(cygpath -w "$BTR")"
```

**A dim-4 chunk mesh sits at the ORIGIN, not at its world cell.** It spans
`0..16384` in x and y, so the look-at is `8192,8192,<ground z>` — not
`cellX*4096`. A pin at the world coordinates renders an empty grid, which looks
exactly like a pin that did not take.

**Two floors, both cheap, and neither is optional:**

* **the mesh must be byte-identical between the two bakes.** `cmp` the `.BTR`.
  If it is, every pixel that differs came out of the sheets and nothing else —
  no camera, no framing, no geometry. If it is not, the picture is of two
  different things and says nothing about the textures;
* **mix and match to prove each texture is actually sampled.** Build a third
  resource root holding the OFF normal sheet and the ON colour sheet and render
  that too. Three renders give the split: colour-sheet-alone, normal-sheet-alone
  and both. A texture the renderer ignores shows up as a zero in that table
  instead of being assumed away. Measured on one dim-4 chunk: colour alone
  1.32 of 255, normal alone 0.96, both 1.74.

**The colours are not the ground's.** As of `release/NifSkope.exe` 2026-09-12
11:51:35 the viewport draws FO4 terrain LOD in greens and magentas over a
diffuse sheet whose own mean is 68/61/53 of 255 — a dark brown. It does it with
vanilla's sheets too, so it is the renderer and not the bake. **Do not ship such
a frame as "what the ground looks like".** Ship it for what it can carry — that
a change reached a real framebuffer — and compute the look picture from the
sheets with the lighting written into the caption.

**And read the frame size back.** Requested 900x900 on that exe, measured
**1358x865** with PIL. The height rule in the section above (`requested - 59`)
gave 865 from 900, i.e. -35 on this build; the width floor was 1358. Both move
with the build. Never quote the request.

## A file that draws its own payload is not a look picture (2026-09-12, lane SHOWCASE1)

Two landed features write DATA into vertex colour -- `--terrain-identity` on the
`.BTR` (material / wetness / AO / shore) and `--identity` on the `.BTO` (object
index in R+G, AO in B, sway in A) -- and the viewport multiplies vertex colour
into the diffuse. A default bake therefore renders green-crushed (ours 61.5 / 2.2 /
84.2 mean RGB against vanilla 120 / 118 / 114 on the same camera) and that is
the FILE, not the renderer. Before any "what does it look like" picture: either
bake a look-only arm with `--no-identity --no-terrain-identity` and label every
picture with the bake it came from, or state that the picture shows a payload.
Never present a payload render as a look render. `--no-terrain-identity` fixes
the terrain only; the object switch is separate.

`WW_LOD_CHANNEL=3` is the AO view: it draws vertex B flat from a file already on
disk in one render. `--ao-grey` writes the same byte into RGB but needs a REBAKE.

A card is not geometry in a chunk: the chunk names only the merged object atlas,
so no viewport render can contain an impostor. Show the card's own sheets.

## Photographing a BUILT document (`.lodl`, `.lodi`, `.btd`)

These files carry no triangles. The document is BUILT on open out of a region,
a level and (for `.lodi`) the library file beside it, so the picture depends on
environment the skill's recipe does not mention, and a wrong value produces a
perfectly convincing wrong picture.

```
WW_LODL_REGION="x0,y0,x1,y1,lod"      the landscape region and level
WW_LODL_PLANE=<key>                    a DATA view; unset = the lit/height view
WW_LODL_SHEETS=<dir>                   where the .lodt pyramid is
WW_LODL_OBJECTS=<file.lodi>            add the native objects to the same document
WW_LODI_REGION="x0,y0,x1,y1"           the object region, cells, inclusive
WW_LODI_LEVEL=n                        the cluster-ladder level, 0 = finest
WW_LODI_DUMP=<file>                    the instance census, one row per placement
```

**The notes are part of the picture's evidence.** Each of these routes prints
what it MEASURED — placements read and drawn, sheet tiles unpacked, the region
it actually used after snapping. Capture stdout with the shot and read it. A
`.lodl` that fell back to the data view and a `.lodl` lit from its sheets are
both handsome; only the notes say which one you have.

**Pin the camera off the region's own arithmetic, never off a remembered screen
position.** A chunk of `DIM` cells at cell `(CX, CY)` is world
`X = CX*4096 .. (CX+DIM)*4096`, so:

```
WW_RENDER_CENTER="$(( (2*CX+DIM)*2048 )),$(( (2*CY+DIM)*2048 )),0"
WW_RENDER_ORTHO=$(( DIM * 2048 ))          # half-width: the chunk fills the frame
WW_RENDER_VIEW=1                           # top
```

Two files photographed with that pair are comparable pixel for pixel, which is
what a coverage-mask IoU or a mean-colour difference needs.

---


### The lighting a built LOD document is photographed under (lane NATIVEVIEW2, 2026-09-16)

A render-shot picture of terrain LOD is a picture of a LIGHT as much as of a
mesh, and three facts about that light decide what the picture can prove:

* **The default light is a HEADLIGHT.** `frontalLight` is true by default
  (`src/glview.h`), so `globalUniforms.lightSourcePosition[0]` is
  `(0, 0, 1)` in VIEW space -- the light points down the view axis and MOVES
  WITH THE CAMERA. Two views of the same shape are therefore two different
  lighting conditions, and the brightness difference between a top view and an
  oblique is not a defect. The direction in world axes is the bottom row of
  `Matrix::fromEuler( Rot )`: for `WW_RENDER_VIEW=1` (Top, rotation 0,0,0) it is
  `(0, 0, 1)`; for `WW_RENDER_VIEW=8` (ViewUser, the Blender startup rotation
  `-63.5593, 0, 133.3081`) it is `(-0.6516, +0.6142, +0.4453)`.
* **`diffuse = A + D * max(N.L, eps)`** in `res/shaders/fo4_default.frag`, with
  `A = sqrt(ambient) * 0.375` and `D = sqrt(diffuse)` from the vertex stage, and
  a tone map after it. So a frame's mean luma is NOT proportional to `N.L`: a
  prediction of the form "half the `N.L`, half the luma" will be wrong. What
  survives the tone map is ORDER and EQUALITY -- two normals with the same `N.L`
  must give the same picture, and a larger `N.L` must not be darker. Build a
  known-answer gate out of those, not out of a ratio.
* **Terrain LOD is lit from a MODEL-space normal sheet** when Shader Flags 1
  bit 12 is set (`docs/LODGEN_NATIVE_LODO_LODI.md`, "Lighting"). A picture that
  is meant to show terrain SHAPE must therefore be shot with the real `_msn`
  sheets present: `WW_LODL_SHEET_CACHE` pointed at a cache whose `.n.DDS` tiles
  are flat is a perfectly valid picture of nothing, and it is the control arm,
  not the subject. The legacy `.BTR` is a DIFFERENT program (`sk_msn.prog`,
  measured with `WW_PROGRAM_CENSUS`), so it is a control, never a like-for-like
  comparison.
* **`WW_PROGRAM_CENSUS=<ABSOLUTE path>`** writes which program lit which shape
  for the frame just taken -- `shape=".." bsver=.. msn=.. lodland=.. prog=..`,
  one row per first sighting, with the view-space light on the header line. Take
  it with every lighting picture: "the terrain looks wrong" and "the terrain is
  on the program I think it is" are different claims, and only one of them is
  cheap to check.

## The number: `WW_RENDER_SIZE` is a WINDOW size and its WIDTH has a floor

`src/nifskope_ui.cpp` ~22060 does `skope->resize( rw, rh )`. The main window
will not go below its own minimum width, so a narrow request is silently
floored and nothing says so.

**Measured 2026-09-12:** `WW_RENDER_SIZE=480x480` produced a **1024x445** PNG.
The height obeyed (480 minus the window chrome); the width did not move at all.

So: ask for at least 1024 wide, and ALWAYS read the frame size back (PIL) and
`upp` back from `release/ww_camera_pin.log` instead of computing either from
what you asked for. The skill already says to read the frame size back; this is
the reason it is not optional.

---

## The other half of a native picture: the resource root

A `.BTO` of a region bake asks for
`data\Textures\Terrain\<Worldspace>\Objects\<Worldspace>.LodgenObjects.DDS`,
but a bake writes its atlas to `<out>/tex/Objects/`. Pointing
`WW_LODGEN_RESOURCES` at the bake directory is NOT enough and the render comes
back magenta with no warning that looks like a resource problem. Assemble a
shim root shaped like the archive instead:

```
<root>/Textures/Terrain/<Worldspace>/           <- out/tex/*.DDS      (the .BTR sheets)
<root>/Textures/Terrain/<Worldspace>/Objects/   <- out/tex/Objects/*  (the atlas)
<root>/Textures/LOD/                            <- out/obj/textures/LOD/*
```

`GameManager::get_full_path` searches for the archive folder (`textures/`,
`materials/`) at any `/` boundary and ERASES everything before it, which is why
this works and why prepending a folder to a path that already contains one
BREAKS it.

## WW_UI_SHOT photographs the CHROME and never the viewport (2026-09-19, lane CELLVIEW2B)

There is a second picture hook beside `WW_RENDER_SHOT`, and it is not a smaller
version of it -- it answers a different question and it CANNOT answer this one's.

| switch | what it does |
|---|---|
| `WW_UI_SHOT=1` | `skope->grab().save( applicationDirPath() + "/ww_ui_shot.png" )` (`src/nifskope_ui.cpp` ~20558) and quit |
| `WW_UI_SHOT_DOCK=<objectName>` | show and raise that dock first, so it is in the frame |

Two things follow, both of which cost a picture if they are learned late:

* **The path is not yours.** The file always lands at
  `release/ww_ui_shot.png`, whatever the run is called. Delete it before the
  run and copy it away after, or a later reader is looking at an older shot
  with a plausible timestamp.
* **`QWidget::grab()` renders the widget tree through Qt's painter, and a
  `QOpenGLWidget`'s contents are not in that tree.** The 3D view comes back
  SOLID BLACK. This is not a context or an exposure problem and no
  `WW_RENDER_*` switch changes it: the grab never asks the GL surface for
  anything. Lane CELLVIEW2B shipped bungo a `pick_reference.png` whose dock
  rows were perfect and whose viewport -- the highlight box the dock was
  describing -- was a black rectangle.

So a deliverable that must show BOTH the chrome and the scene is **two runs of
the same scene**: one `WW_UI_SHOT=1 WW_UI_SHOT_DOCK=<dock>` for the panel's
rows, one `WW_RENDER_SHOT=<png>` for the viewport, and the report says which
half came from which. Never try to get the viewport out of the widget grab, and
never describe a black viewport as "the scene had not loaded".
