#!/usr/bin/env python3
# BUILD2: turn the two 2026-09-09 entries from "BUILD PENDING / not built" into
# measured. WW_CHANGES.md is MIXED (CR 19020) but both regions edited here are
# LF-only, verified by reading the bytes; the CR count is asserted unchanged.

import sys

PATH = "E:/Projects/NifskopeWildWastelandEdition/WW_CHANGES.md"

# ---- 1. the rename entry's status block -------------------------------------
OLD1 = b"""**CODE AND DOCS ONLY - BUILD PENDING.** Lane OFFSCREEN's `src/nifskope_ui.cpp`
was finished but unbuilt when this lane ended, so no build was started (the
brief's gate). `g++ -fsyntax-only` on all six changed translation units: RC=0
each. NOTHING was renamed in bungo's mod folder - that step needs the new exe to
verify each file after the move. Resume: `scratchpad/rename_20260909/PENDING.md`.
"""

NEW1 = b"""**BUILT AND GATED 2026-09-09 by lane BUILD2**, together with the six-line GUI
half of the rename in `src/nifskope.cpp` (the file-type row, the load route and
the three suffix comparisons) and the one line in `src/nifskope_ui.cpp` that the
rename lane could not touch. `qmake` was re-run FIRST -- the two new
cross-includes (`lodtfile.cpp` -> `io/lodvfile.h`, `lodvfile.cpp` ->
`lodtfile.h`) are invisible to `make` until it is -- and `Makefile.Release` came
back naming all three dependencies, including the one lane BUILD1 had patched in
by hand for `btdterrain.o`, which no longer stands in for anything.
`make -j2` RC=0; `release/NifSkope.exe` 18:43:13, newer than every changed
source; `release/style.qss` in step.

MEASURED, on that exe: `lodl_write.sh` 34 checks 0 failures (magic still LODT
after the rename, heights round-trip exactly, and both refusal directions name
the other format); `lodl_open.sh` **23 checks 0 failures** on bungo's own renamed
Commonwealth file; `lodgen_terrain.sh` 26/0; `lodgen_identity.sh` 8 checks 0
failures; `lod_generation.sh` **97/0**
-- the one expected red, *"with one, the panel says what it will write"*, is
green now that the GUI line is in; `btd_terrain.sh` 13/0;
`lodgen_impostor_cards.sh` PASS; `lodgen_terrain_vt.sh` 31/1, the single failure
being V9a (the assembled colour sheet vs a direct bake, same size, different
bytes), a `dominantBase` scoping question that predates this rename and is
recorded as unattributed.

**bungo's installed set was renamed and read back**: `Commonwealth.lodl`
35,953,294 B, `DLC03FarHarbor.lodl` 9,195,933 B, `DiamondCity.lodl` 53,148 B,
`NukaWorld.lodl` 7,182,356 B, `NukaWorldAmphitheater.lodl` 38,303 B in
`E:\\Projects\\Fallout 4 Mods\\mods\\FO4CS\\Terrain\\`, each `lodl ... --info`
rc=0, and each `--verify-only` against its own plugin **0 mismatched** (36,864 /
256 / 5,568 / 69,696 / 128 samples; alpha and colour words 0 differing too). The
five `*.lodt.bak-20260909` copies were left exactly as they were, name included.

`--candidates trees` was measured on the Sanctuary region (-20,24)..(-17,27) of
Fallout4.esm: **19 candidates, every one under `Landscape\\Trees\\`**, against 33
from the default filter -- the 14 shacks and rock cliffs of the bug report are
gone.
"""

# ---- 2. the off-screen entry's status line ----------------------------------
OLD2 = b"""**CODE ONLY - BUILD PENDING** (Fallout4.exe was up from 17:47; CONSTITUTION 6).
`g++ -fsyntax-only` on the changed translation unit: RC=0, no new warnings.
"""

NEW2 = b"""**BUILT 2026-09-09 by lane BUILD2 -- AND THE FIX IS INERT. THE STROBE IS STILL
LIVE.** `release/NifSkope.exe` 18:43:13, `tests/spells/render_shot.sh`
**28 checks, 6 failures**. Sections 1-4 (the Save Confirmation guard) stay green;
all six failures are sections 5's, and they say the window was on a monitor.
Read the paragraph at the end of this entry before running any bake.
"""

# ---- 3. the "NOT MEASURED YET" block ----------------------------------------
OLD3 = b"""**NOT MEASURED YET** (BUILD PENDING): that an off-screen window on this machine
still renders and still grabs the same pixels. It should - DWM composes a window
wherever it sits, and the back buffer is not the desktop - but the identity check
in section 6 is exactly the thing that would fail if a driver disagreed, and it
has not been run. Nothing about the bake should be re-run until it has.
"""

NEW3 = b"""**MEASURED 2026-09-09 (lane BUILD2), AND BOTH HALVES CAME BACK BADLY.**

**1. The branch above never runs the way it reads.** `restoreUi()`, two lines
earlier, restores the window state the person last left -- MAXIMISED -- and on
Windows `move()` on a maximised window changes nothing except which monitor it
is maximised onto. The off-screen origin is on no monitor, so the move did
nothing: every headless run came up maximised on the primary screen, `2 of 4`
window records `onscreen=1`, the outside sampler seeing it at `-8,-8,1928,1048`.
The control run is the proof: it asked for `move(1960,40)` and the window's
client origin came out `1920,-42`, the maximised client origin of the monitor
containing that point, not the point. **bungo's strobe is exactly as it was.**

**2. And the off-screen window does not render.** With one line added to clear
the maximised bit before the move (built, measured, then REVERTED), the window
did leave every screen -- `0 of 4` records `onscreen=1` on all three runs -- and
then `WW_RENDER_SHOT` wrote **no PNG at all** and `WW_IMPOSTOR_BAKE` wrote its
sidecar and **no card image**, both exiting 0 in a few seconds. A window entirely
outside every screen is never exposed, so `QOpenGLWidget` never creates its
context and `grabFramebuffer()` returns a null image. An off-screen bake would
have written EMPTY card sets while reporting success, and the driver caches by
form id, so the emptiness would have been cached. That is worse than the hazard,
so the tree stands as this entry describes it and the cure is a design decision,
not a build lane's.

**Two candidates for that cure, neither measured.** (a) Render into an FBO
instead of the window -- `GLView::grabSupersampled` (`src/glview.cpp`) already
binds a `QOpenGLFramebufferObject` and short-circuits `shift == 0` to
`grabFramebuffer()`, which is the case that would need writing; an FBO render
does not need the window exposed once a context exists, and where that context
comes from is the open question. (b) Leave the window on a screen and set its
opacity to 0, which stays exposed and composited and so still renders, while
painting nothing a person can see -- the gate's instrument counts geometry, not
visibility, so section 5 would have to change with it.

**Section 6's identity check also has to grow a precondition.** It read
`off fedab869884174fb / on fedab869884174fb  ok` in the same run in which six
checks said the window was on a screen: it had compared a picture with itself.
It must first require the off-screen run to have recorded zero `onscreen=1`.
"""

with open(PATH, "rb") as fh:
    b = fh.read()
cr0 = b.count(b"\r")
for old, new, name in ((OLD1, NEW1, "rename status"), (OLD2, NEW2, "offscreen status"),
                       (OLD3, NEW3, "offscreen not-measured")):
    if b.count(old) != 1:
        print("ABORT: %s anchor count %d" % (name, b.count(old)))
        sys.exit(2)
    if b"\r" in old:
        print("ABORT: %s anchor carries CR" % name)
        sys.exit(2)
    b = b.replace(old, new)
cr1 = b.count(b"\r")
if cr1 != cr0:
    print("ABORT: CR %d -> %d" % (cr0, cr1))
    sys.exit(2)
with open(PATH, "wb") as fh:
    fh.write(b)
print("OK WW_CHANGES.md CR %d (unchanged), %d bytes" % (cr1, len(b)))
