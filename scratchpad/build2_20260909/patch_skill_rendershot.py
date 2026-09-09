#!/usr/bin/env python3
# BUILD2: the render-shot skill states an off-screen rule that the BUILT code
# does not honour. It is in the LIVE tree, so every lane reads it. Correct the
# heading and put the measured state at the top of that section.
# LF-only file; the assert pins it.

import sys

PATH = "E:/Projects/Claude/.claude/skills/nifskope-ww-render-shot/SKILL.md"

OLD_HEAD = b"## A headless run NEVER puts a window on a screen (2026-09-09, lane OFFSCREEN)\n"

NEW_HEAD = b"""## Headless runs and the screen: WHAT IS TRUE AS BUILT (2026-09-09, lane BUILD2)

**READ THIS BEFORE THE SECTION BELOW.** The off-screen placement described below
was written by lane OFFSCREEN, built for the first time by lane BUILD2, and it
**does not work**. `tests/spells/render_shot.sh` reads **28 checks, 6 failures**
on `release/NifSkope.exe` 18:43:13: every headless run still comes up MAXIMISED
on the primary monitor, because `restoreUi()` restores a maximised window and on
Windows `move()` on a maximised window changes nothing except which monitor it
maximises onto -- and an off-screen point is on no monitor. So:

* **a bake still strobes**, and bungo's photosensitivity report is open. Do not
  run `tools/bake_impostor_cards.sh` on a machine he is looking at until this is
  fixed;
* `WW_WINDOW_AT` still decides where the window goes, exactly as it did before;
* clearing the maximised bit DOES take the window off every screen, and then
  **nothing renders at all** -- measured: no PNG from `WW_RENDER_SHOT`, no card
  image from `WW_IMPOSTOR_BAKE`, both exiting 0. A window outside every screen is
  never exposed, so `QOpenGLWidget` never creates its context and
  `grabFramebuffer()` returns a null image. An off-screen bake writes EMPTY card
  sets and reports success. That one-line change was built, measured and
  REVERTED; do not re-apply it without the capture path to go with it;
* the open candidates are the FBO path (`GLView::grabSupersampled`,
  `src/glview.cpp`, which short-circuits `shift == 0` to `grabFramebuffer()`) and
  leaving the window on screen at opacity 0. Neither is measured.

`release/ww_headless_windows.log` and `release/ww_render_shot/*.winlog` are how
this is checked, and they are the only honest answer to "did that run show a
window".

## What lane OFFSCREEN intended, and what is actually in the code (2026-09-09)
"""

with open(PATH, "rb") as fh:
    b = fh.read()
cr0 = b.count(b"\r")
if b.count(OLD_HEAD) != 1:
    print("ABORT: anchor count %d" % b.count(OLD_HEAD))
    sys.exit(2)
b = b.replace(OLD_HEAD, NEW_HEAD)
if b.count(b"\r") != cr0:
    print("ABORT: CR moved")
    sys.exit(2)
with open(PATH, "wb") as fh:
    fh.write(b)
print("OK skill amended, %d bytes, CR %d" % (len(b), b.count(b"\r")))
