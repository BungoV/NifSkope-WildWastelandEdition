#!/usr/bin/env python3
# BUILD2: three entries into MISTAKES.md, newest at the top (CONSTITUTION 2).
# MISTAKES.md is LF-only (CR 0); the assert below pins that.

import sys

PATH = "E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md"

ANCHOR = b"Newest at the top.\n\n"

ENTRY = b"""Newest at the top.

## 2026-09-09 -- the off-screen fix for bungo's strobe was INERT, and its own gate said PASS anyway

- **What was done:** lane OFFSCREEN moved a headless window off every screen
  before showing it (`src/nifskope_ui.cpp`, `createWindow`), and
  `WW_CHANGES.md` described the strobe as fixed with one caveat ("NOT MEASURED
  YET: that an off-screen window still renders"). It had never been built.
- **What was true:** on the first build that carried it (lane BUILD2,
  `release/NifSkope.exe` 18:29:24), every headless run still came up **maximised
  on the primary monitor**. `restoreUi()` restores the window state the person
  last left, which is maximised, and on Windows `move()` on a maximised window
  changes nothing except which monitor it is maximised onto. The off-screen
  origin is on no monitor, so the move did nothing at all.
  `tests/spells/render_shot.sh` read **28 checks, 6 failures**: 2 of 4 window
  records `onscreen=1` on each of the three runs, and the outside sampler saw
  the window at `-8,-8,1928,1048` on `\\\\.\\DISPLAY5`.
- **How it was found:** the lane's own instrument. `release/ww_render_shot/
  render_plain.winlog` said `shown QWidgetWindow geom=0,23,1920x1017
  onscreen=1` where the off-screen origin had been asked for, and the control
  run said `1920,-42` where `1960,40` had been asked for -- the maximised client
  origin of the monitor containing that point, not the point.
- **The rule:** a placement call is not a placement. Any code that moves a
  window states the window state it is moving FROM, and the gate reads the
  geometry back. "It should be off-screen" is a claim about an API, and this
  API's contract depends on a state the code never looked at.

## 2026-09-09 -- a pixel-identity check passed while the thing it was comparing had not happened

- **What was done:** section 6 of `tests/spells/render_shot.sh` compares the
  PNG from the off-screen run with the PNG from the `WW_WINDOW_VISIBLE=1`
  control and passes when they are byte-identical -- described as the gate that
  would fail "if a driver disagreed".
- **What was true:** on the build where the off-screen branch was inert, both
  runs put the window on a monitor, so the check compared a picture with itself
  and printed `off fedab869884174fb / on fedab869884174fb  ok`. It was green in
  the same run in which six other checks said the window was on a screen.
- **How it was found:** reading the failures next to it rather than the verdict.
- **The rule:** a comparison check asserts its own PRECONDITION or it is not a
  check. Section 6's identity check must first require that the off-screen run
  recorded zero `onscreen=1` records; without that it cannot fail for the reason
  it exists.

## 2026-09-09 -- lane BUILD2 changed behaviour its brief did not give it, and paid two builds for it

- **What was done:** BUILD2's brief was "apply the six-line GUI rename, build,
  gate, rename bungo's installed files". When the off-screen gate failed, this
  lane wrote the one-line un-maximise fix, rebuilt (18:38:46), re-ran the gate,
  found that an off-screen window renders NOTHING on this machine (no PNG from
  `WW_RENDER_SHOT`, no card image from `WW_IMPOSTOR_BAKE`, both exiting 0),
  reverted, and rebuilt again.
- **What was true instead:** the finding is real and worth having -- it is the
  refuter lane OFFSCREEN itself named -- but the diagnosis alone would have
  carried it, and two builds of `nifskope_ui.cpp` (~7 min each) were spent
  proving what a director would have decided in one line.
- **How it was found:** written here while doing it.
- **The rule:** a build lane that finds a DESIGN failure reports it and stops.
  Measuring the cause is in scope; landing the cure is not, and the second build
  is the tell that the line was crossed.

"""

with open(PATH, "rb") as fh:
    b = fh.read()
cr0 = b.count(b"\r")
if b.count(ANCHOR) != 1:
    print("ABORT: anchor count %d" % b.count(ANCHOR))
    sys.exit(2)
b = b.replace(ANCHOR, ENTRY)
cr1 = b.count(b"\r")
if cr1 != cr0:
    print("ABORT: CR %d -> %d" % (cr0, cr1))
    sys.exit(2)
with open(PATH, "wb") as fh:
    fh.write(b)
print("OK MISTAKES.md CR %d (unchanged), %d bytes" % (cr1, len(b)))
