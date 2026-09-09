## 2026-09-09 - Headless runs put no window on any screen (the bake's black/white strobe)

`src/nifskope_ui.cpp`, `tools/bake_impostor_cards.sh`, `tests/spells/render_shot.sh`.
**CODE ONLY - BUILD PENDING** (Fallout4.exe was up from 17:47; CONSTITUTION 6).
`g++ -fsyntax-only` on the changed translation unit: RC=0, no new warnings.

bungo, verbatim: *"when these trees bake their impostors, the screen is flashing
white and black, that's a view hazard for epileptics"*.

**WHAT SHOWED.** Every headless run opened a real, visible window. `createWindow`
had two placement branches and both of them ended in a shown window: with
`WW_WINDOW_AT` set - which `tests/spells/_harness.sh:29` exports for every
harness, and which `tools/bake_impostor_cards.sh` inherits by sourcing it - the
window was moved to `1960,40` and `show()`n on the second monitor; without it,
`showMaximized()` + `raise()` filled the primary monitor and took focus. There is
no third path: a `WW_RENDER_SHOT`, a `WW_LOD_CHANNEL` preview, a card bake and
every `WW_*_TEST` harness all came through it.

**WHAT FLASHED.** The impostor card baker photographs a TWO-PASS MATTE: the scene
over a black clear and over a white one, the difference of the two being
`1 - alpha`. Counted from the code, one octahedral view costs NINE full repaints
of that visible window - two for the extent matte of pass one, then two for the
card matte of pass two plus five channel renders (view-space normal, depth,
material, alpha-test, emissive). At `OCT=8` that is `64 * 9 + 4 = 580` repaints
per model, milliseconds apart, two in every nine of them alternating full-frame
black and full-frame white, and the driver runs model after model for dozens of
models. Nothing per-frame was created or destroyed and nothing was hidden and
re-shown; the strobe is the matte's own clears, on a window that had no reason to
be on a monitor.

**THE FIX: THE WINDOW IS MOVED OFF EVERY SCREEN BEFORE IT IS SHOWN.**
`grabFramebuffer()` reads the window's BACK BUFFER after `paintGL()` - the
capture path never consults the desktop - so a headless run needs a window, not a
visible one. `NifSkope::createWindow` now takes a third branch first when
`NifSkope::wwHeadlessRun()` is true (the same one-line predicate the Save
Confirmation guard uses: any `WW_*` variable, or `-no-gui`): the window is moved
to the left edge of the union of every screen, 64 px below its bottom, given
`Qt::WA_ShowWithoutActivating` - an off-screen window that steals the keyboard is
worse than a visible one - and only then shown. `WW_WINDOW_AT` still decides
where a window goes, but only when `WW_WINDOW_VISIBLE=1` asks for a visible one:
that is the exact way back to the old behaviour, and it is what the gate uses as
its control.

**ONE ROUTE STILL REACHES A SCREEN, DELIBERATELY.** `WW_GIZMONUM_TEST` walks the
window under the real mouse pointer (`QCursor::setPos` is a no-op for a process
that is not foreground, so it moves the window instead of the cursor), which
brings it back onto whichever monitor the pointer is on. It renders no matte,
repaints a handful of times, and flashes nothing. Every other headless route
stays off-screen.

**THE INSTRUMENTS, AND THE FLOOR.** A run now records its own top-level windows
to `release/ww_headless_windows.log` - at show, at each `WW_RENDER_SHOT` grab, at
the start of the bake matte and at the start of the octahedral sheet - one line
per window with its geometry and `onscreen=0/1` measured against
`QGuiApplication::screens()`. `tests/spells/render_shot.sh` gained sections 5 and
6: section 5 requires the three existing runs to have recorded windows at all
(anti-vacuity) and then to show zero `onscreen=1` records and zero on-screen
samples from an outside PowerShell watcher that samples every 200 ms for the life
of each run; section 6 re-runs the plain render with `WW_WINDOW_VISIBLE=1` and
requires BOTH instruments to see a window on a screen, or their zeros above prove
nothing. That control run is also the pixel gate: one build, one scene, two
window positions, and the two PNGs must be byte-identical. That is a tighter
comparison than a before/after exe, which would carry every other uncommitted
change in the tree as well. The gate's own on-screen window is one cube render of
a few seconds - not a matte and not a strobe.

Section 4 was also narrowed: it counted every `NifSkope.exe`, so a person's own
open window failed a check that has nothing to do with the code under test. It
now counts only harness instances, which always carry `--port`.

**NOT MEASURED YET** (BUILD PENDING): that an off-screen window on this machine
still renders and still grabs the same pixels. It should - DWM composes a window
wherever it sits, and the back buffer is not the desktop - but the identity check
in section 6 is exactly the thing that would fail if a driver disagreed, and it
has not been run. Nothing about the bake should be re-run until it has.
