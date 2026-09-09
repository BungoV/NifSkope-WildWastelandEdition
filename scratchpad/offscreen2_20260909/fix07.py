import sys, io, os

LIVE = r'E:\Projects\Claude\.claude\skills\nifskope-ww-render-shot\SKILL.md'
REPO = r'E:\Projects\NifskopeWildWastelandEdition\.claude\skills\nifskope-ww-render-shot\SKILL.md'

NEW_DESC = ("description: Photograph a NIF, .bto or .btr headlessly through NifSkope Wild "
            "Wasteland Edition's render hook (WW_RENDER_SHOT and its WW_RENDER_* switches, "
            "WW_LOD_CHANNEL for the generated vertex channels), one instance at a time and "
            "never a desktop capture -- for showing bungo a bake (tree sway, AO, identity, "
            "terrain wetness) or pixel-diffing a shader change. A headless run is INVISIBLE "
            "(window opacity 0) and never on the primary monitor; read the first section "
            "before running anything that repaints in a loop. Use whenever a picture of "
            "rendered geometry is the deliverable or the gate; never screen-capture the "
            "desktop.\n")

NEW_FIRST = """## Headless runs and the screen: WHAT IS TRUE AS BUILT (2026-09-09, lane OFFSCREEN2)

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
"""


def rewrite(path):
    if not os.path.exists(path):
        return None
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    t = b.decode('utf-8')

    # 1. the front-matter description line
    lines = t.split('\n')
    hit = [i for i, l in enumerate(lines) if l.startswith('description: ')]
    if len(hit) != 1:
        sys.stderr.write('%s: %d description lines\n' % (path, len(hit)))
        sys.exit(1)
    lines[hit[0]] = NEW_DESC.rstrip('\n')
    t = '\n'.join(lines)

    # 2. replace everything from the first "## Headless runs" heading up to the
    #    heading after the OFFSCREEN section.
    start_key = '## Headless runs and the screen'
    end_key = '\n| switch | meaning |'
    s = t.find(start_key)
    e = t.find(end_key)
    if s < 0 or e < 0 or e <= s:
        sys.stderr.write('%s: could not bracket the section (%d, %d)\n' % (path, s, e))
        sys.exit(1)
    t = t[:s] + NEW_FIRST + t[e:]

    out = t.encode('utf-8')
    if out.count(b'\r') != cr:
        sys.stderr.write('%s: CR moved %d -> %d\n' % (path, cr, out.count(b'\r')))
        sys.exit(1)
    open(path, 'wb').write(out)
    return len(out)


for p in (LIVE, REPO):
    n = rewrite(p)
    print(p, '->', n if n else 'ABSENT')
