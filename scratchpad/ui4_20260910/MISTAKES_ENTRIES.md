<!-- Lane UI4, 2026-09-10. Appended to MISTAKES.md by the lane (CONSTITUTION 2). -->

## 2026-09-10 -- lane UI4: the probe was fed an UNSUBSTITUTED stylesheet, and measured a separator the application does not have

**What was done.** `ww-qss-geometry-probe` says, in its own words, that
`release/style.qss` is the copy "the one the application reads and its `${...}`
skin variables are already substituted, so the probe needs no colour table".
The first probe of this lane took that at face value and did
`app.setStyleSheet( raw )`.

**What was true instead.** `release/style.qss` is a **byte copy** of
`res/style.qss` -- `grep -c '${' ` returns **89 on both files**. The
application substitutes the tokens at load
(`src/nifskope_ui.cpp:30199-30211`), so a raw sheet loses every declaration
carrying a colour, and Qt drops those rules in silence. The rule this lane
needed -- `QMainWindow::separator { background: ${borderStrong}; width: 3px; }`
-- was one of them, so the probe reported a **6 px** gap between the dock and
the column where the application has **3**. A fix sized on that number would
have put the strip 3 px away from bungo's 4 and passed its own probe.

**How it was found.** Because case 0 of a probe MUST reproduce the shipped
number before anything after it is believed (the skill's own step 1). The
before-run of `water_ui.sh` had already measured the real window: `dock tab
strip w 383` and `viewport header x 386`, so 3 px. The probe said the header
was at 389. Two numbers for one gap is the whole of the catch.

**The rule that prevents it.** A probe substitutes the sheet the way the
application does, and its case 0 is compared against a number measured IN THE
APPLICATION, not against expectation. `ww-qss-geometry-probe` is amended (its
section 2 now says the tokens are NOT substituted on disk and carries the
substitution step), in both skill trees.

## 2026-09-10 -- lane UI4: the exe guard ran four minutes before the link, and bungo opened the window in between

**What was done.** The build chain checked for a NifSkope holding
`release/NifSkope.exe`, found none ("exe not held by a window"), and started
`make -j2`. bungo opened `release\NifSkope.exe` at **20:45:04**, roughly two
minutes in. At ~20:47 `ld` died with `cannot open output file
release/NifSkope.exe: Permission denied` and the whole compile was spent for
nothing.

**What was true instead.** The guard answers a question about a moment that has
already passed. `nifskope-ww-build-verify` says this in as many words -- "The
process guard is a GATE, not a line of output ... the check belongs immediately
before the link, not at the start of the lane" -- and names the identical
14:26:18 incident of lane BUILD8. It had been read forty minutes earlier.

**How it was found.** `BUILD-RC=2` with a linker error and
`release/NifSkope.exe` still stamped 18:25:20.

**Cost and damage.** One relink (the objects survived; nothing recompiled).
bungo's window was never touched, and the old exe survived because the link
failed before it could be replaced. Recovered by renaming his running copy
aside (`release/NifSkope_inuse_46176.exe`) and running `make` again.

**The rule that prevents it.** The rename goes immediately before the link, in
the same shell as the link, not at the top of the chain -- i.e. use
`tools/ww_build.sh`, or make the link step itself re-check and rename. A guard
whose answer is four minutes old is not a guard.

## 2026-09-10 -- lane UI4: a gate built on QTabBar::tabRect() would have been green on both states

**What was done.** The first sketch of group S measured the air with
`QTabBar::tabRect()` and rect arithmetic, in the same idiom groups R1..R5 use.

**What was true instead.** QSS margins on `QTabBar::tab` are honoured -- the
painted box moves -- but **`tabRect()` returns the rect INCLUDING the margin**.
Measured, probe cases 0 and A1: the rects are `128 / 127 / 128` at y 0, h 35 in
BOTH, while the painted boxes go from `0,0 128x35` to `4,4 124x27`. A gate on
rects would have printed identical numbers before and after and passed both.

**How it was found.** The probe's first version printed rects only and reported
"margins have no effect", which contradicted the vertical sweep in the same run
(`min-height 19 -> tabRect h 35`, i.e. the margin WAS being counted). The
contradiction is what sent the probe to the pixels.

**The rule that prevents it.** When a change moves what is PAINTED inside a
widget's own box -- a margin, a subcontrol, an indicator -- the gate reads the
grab, not the geometry. A rect that cannot move is not a measurement of a
change that does.
