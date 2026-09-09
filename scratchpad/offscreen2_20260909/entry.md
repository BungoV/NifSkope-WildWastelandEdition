## 2026-09-09 - a headless run is INVISIBLE and never on the primary monitor

`src/nifskope.cpp`, `src/nifskope.h`, `src/nifskope_ui.cpp`,
`tests/spells/render_shot.sh`, `tools/bake_impostor_cards.sh`.

bungo, two rules, both verbatim: *"Agent is launching nifskope on my main
monitor, which is a no no"* and *"the screen is flashing white and black,
that's a view hazard for epileptics"*.

**The earlier off-every-screen attempt is withdrawn.** It was inert as written
(`restoreUi()` restores a MAXIMISED window and on Windows `move()` on a
maximised window only chooses a monitor), and when the maximised bit was
cleared it worked and NOTHING RENDERED: `GLView` is a `QOpenGLWindow`, a
surface that is never exposed never gets a context, and `grabFramebuffer()`
returned a null image while the run exited 0 - no PNG, no card, an empty card
set reported as success.

**What ships instead.** A headless run (`NifSkope::wwHeadlessRun()` - any `WW_*`
variable, or `-no-gui`) has its window un-maximised, placed on a NON-PRIMARY
screen, shown without activating, and shown at WINDOW OPACITY 0. It is still on
a screen, deliberately, so it is still exposed and still renders. Three places
apply it, because one was not enough:

* `NifSkope::wwPlaceHeadlessWindow()` (`nifskope_ui.cpp`) is the one
  implementation. `wwHeadlessWindowOrigin()` beside it serves `WW_WINDOW_AT`
  when that point is not on the primary, else the first non-primary screen's
  top-left, else `1920,0`, else the primary with opacity 0 as the only
  guarantee - and NAMES the arm it served in the window log's `arm=` field, so a
  refusal is never silent;
* the `NifSkope` CONSTRUCTOR calls it before `ui->setupUi()`, which is what
  removes a real flash on his main monitor: an `EnumWindows` probe at 25 ms
  found a 426x306 OPAQUE window of the process at 632,249 on the PRIMARY at
  t=371 ms - Qt's default geometry, class `Qt6111QWindowIcon`, the application
  title, no filename - gone by t=1403 ms and replaced by class
  `Qt6111QWindowOwnDCIcon` at the asked-for place with layered alpha 0. The
  class change is the mechanism: the Qt Windows plugin picks a window class by
  whether the surface needs its own DC, so realising the GL container destroys
  the first native window and creates a second, and the first was created while
  the widget still had its default geometry;
* `createWindow` calls it again after `restoreUi()`, whose `restoreGeometry()`
  overwrites both the position and the maximised bit;
* the application event filter calls it on `QEvent::Show` for every OTHER
  top-level window - a dialog, a floating dock, a tool window - as a backstop.

`WW_WINDOW_VISIBLE=1` makes the window opaque again for watching one run. It
does NOT move it to the primary: the control run is placed by the same code,
because a visible-on-purpose control is how a NifSkope came to be on his main
monitor in the first place. Interactive use is untouched - with no `WW_*`
variable the maximise-and-raise branch runs exactly as before.

**Measured, on `release/NifSkope.exe` 19:35:14.** `tests/spells/render_shot.sh`,
rewritten sections 5-6: **55 checks, 0 failures**. Three instruments, each shown
able to fail, and every floor fired in the same run:

| | hidden runs | visible control | strobe control |
|---|---|---|---|
| windows on the primary (process's own log) | 0 of 4-6 records | 0 of 4 | 0 of 6 |
| windows on the primary (EnumWindows, outside) | 0 of 6-26 samples | 0 | 0 |
| mapped windows at opacity > 0 | 0 | 2 of 2 | - |
| layered alpha > 0 (Windows' own answer) | 0 samples | 11 of 11 | - |
| the SECOND MONITOR'S OWN PIXELS, luminance range | **0.233**, the desktop's own noise | 27.1 | **246.8** |

The pixel row is the one that answers him: the desktop is sampled every ~50 ms
over a 200x200 region of the second monitor where the window sits, and a hidden
4x4 octahedral bake - 148 full repaints, two in every nine alternating a black
clear and a white one - moved it by 0.233, which is what the region does with
nothing running at all. The same bake made visible on purpose moved it by 246.8.

**And the pixels do not change.** Same build, same scene, opacity 0 and opacity
1: the render PNG is byte-identical (`fedab869884174fb...`), and so are the six
card sheets of a bake (`3f2db029bde9d94d...`). Repeated on the real model bungo
was baking, `TreeMapleForest02.nif` at OCT=4/TILE=64: six sheets each way, card
set hash `6db4e81468e37b85...` **identical**, desktop luminance range 0.111
hidden against 251.6 visible.

`tests/spells/lod_generation.sh` 97 checks, 0 failures - a GUI harness that
takes focus, drives widgets and scrolls a wheel still works on an invisible
window.

**Not measured:** an OCT=8 bake of a full tree library end to end through
`tools/bake_impostor_cards.sh` (the driver's candidate machinery belongs to
another lane); and whether any other machine's driver honours a layered alpha
the way this one does - the gate's pixel instrument is what would catch it.
