<!-- Lane WATER7 / UI2, 2026-09-10. TEXT ONLY for WW_CHANGES.md; the director
     splices it (CONSTITUTION 8). WW_CHANGES.md is MIXED and its 2026-09
     entries at the top are LF-only -- this text is LF-only and carries no CR.
     NOT BUILT: every number below that only a built exe could produce is
     marked as a prediction, by name. -->

## 2026-09-10 -- the water tool becomes a tab of the LOD Generation workspace, and the top of the window is one row (lane WATER7 / UI2)

**NOT BUILT.** `scratchpad/build11_20260910/DONE` did not exist and that lane
was still writing (its `gates_summary.txt` at 17:19:40 against a 17:23 check),
so this lane ended BUILD PENDING with
`scratchpad/water7_20260910/PENDING.md`. `hookup.py` is **not applied**. Every
gate figure here is a PRE-REGISTRATION, not a result.

**bungo's two rulings, both on screenshots, both about the same strip.** First,
of the Workspaces menu: *"Two issues with water window and water marking
appearing here"*, corrected the same minute to *"They should be in the LOD gen
workspace"*, then, over the Header | Blocks | Files strip, *"You'd access them
like this"*. Second, of the aligned strip beside the Object Mode row:
*"compact these vertically like this, the top bar and the buttons"*.

**The Water tab.** There is no `Water Marking` dock any more and no entry for
either half of the tool in the Workspaces menu. The marking rows -- body
select, the per-body rows, the curve tools, Solve, Save/Load curves,
Export/Import PNG and the button that opens the full-screen flow window -- are
a fourth page of `LeftColumnStack`, reached by a fourth tab of
`LeftColumnModeSelector`: **Header | Blocks | Files | Water**, one strip, one
bar height. `LeftColumnMode` gains `LeftWater = 3` and `setLeftColumnMode`
stops clamping 3 back to 0. **How the strip scoped tabs per workspace before
this: it did not** -- all three tabs are added unconditionally in
`NifSkope::initDockWidgets` and nothing hides one. The only per-workspace
mechanism in the window is the `workspaceRole` property on the manager docks
and the exclusive-visibility rule over them, so the Water tab follows it: shown
exactly while `LodGenerationDock` is visible, and never left as the current tab
of a strip it is about to vanish from. The install refuses in words, and
installs nothing, if its page does not land at stack index 3 -- a tab that
silently bounces to Blocks reads as a broken tool rather than an absent one.

**One compact height for the whole top of the window.** BUILD9 gave the main
toolbars, the viewport header and the dock strip one height (35 px, measured
2026-09-10) and left out the MENU BAR and everything inside every bar. Now the
menu bar joins the same `wwAlignBarRow` row and the row's own child sheet --
new `wwBarRowButtonQss()`, one piece of arithmetic derived from the row height
-- is appended to each bar, so `QMenuBar::item` and every `QToolButton` in the
row take the row's height and one vertical padding. No per-widget
`setFixedHeight` anywhere, and no second number at any call site.
**PREDICTION, not a measurement: the row stays 35 and the menu bar grows to it
from ~23-25.** `wwAlignBarRow` takes the TALLEST natural height, so if the menu
bar turns out to be the tallest the row GROWS instead -- the opposite of what
bungo asked for. The resume registers that refuter and says not to ship it in
that case.

**The way back, exact at its off value** (CONSTITUTION 7): `UI/CompactTopBars`,
QSettings, default true. False leaves the menu bar out of the row and restyles
no bar's children -- BUILD9's behaviour, byte for byte. One reader
(`wwCompactTopBars()`), so the sheet and the call site cannot disagree. The
Water tab is not behind it; that is his ruling, not a preference.

**BUILD10's five reds, all five addressed.**

1. **X2b's instrument replaced** (`src/watermark.cpp`). The old one compared
   the solved flow with the straight-line radial over a DISC of four pin
   widths, and read 0.742 on body 2 against 0.371 on body 3 with the pin
   behaving identically -- the Charles bends inside four widths, so the cosine
   fell for a reason that is not about the pin. It is now the NET OUTWARD FLUX
   through a thin ring at two pin widths, measured with the pin and without it
   over the SAME texels: through-flow enters and leaves one ring and cancels in
   the difference, whatever the channel does. **Floor registered before the
   code was written: the difference must exceed 0.5, and the no-pin state on
   the same ring must read below 0.2.** The retired disc number is still
   printed, informational, so the first run reads as a comparison of two
   instruments on one fixture.
2. **`tests/spells/water_flow.sh` stops calling a green gate red.** Its loop
   took the INFORMATIONAL line printed above a verdict; it now filters
   `grep -aE '^  (ok|FAIL) '` before `head -1`, as `water_weights.sh` always
   did.
3. **Its floor is 17, not 18**, and the spell prints the arithmetic: 19
   registered F-gates minus the 2 pre-registered as red (F2's island bank, F5's
   p99) -- one below a green run, so losing another gate still goes red.
4. **`water_mark.sh` pins `WW_WATER_MARK_BODY=3`** and prints the body. It ran
   the headless half on the default, body 2, the marsh, whose dye has no mouth,
   while WATER4's F5 and dye gates were registered on body 3.
5. **Two defects in what the marking tool SAVES.** (a) A dye pin's per-point
   weight is now written -- and the READING half is fixed in the same change,
   because `parseStoreExtras` excluded `DyePin` too AND did not allow for the
   four colour bytes a dye pin's record carries before its trailing bytes, so
   the writer alone would have emitted weights nothing reads. (b) The FIRST
   named body in any file the tool saves read back nameless: `encodeTable`
   started its running name offset at 0, and 0 is what every reader spells
   "no name" (`LodtFile::bodyName`, and the generator's own
   `table.u32( 0 ); // name offset: unnamed`). The name blob now reserves byte
   0 and offsets start at 1. **A document with no names still writes no blob at
   all and every offset stays 0**, so an unedited document re-encodes to the
   file's own bytes -- which is the property the byte-identity gate rests on.

**New gate.** `tests/spells/water_ui.sh` / `src/wateruitest.cpp`
(`WW_WATERUI_TEST`), its own translation unit: eight checks on the tab (T) and
five on the bar row (R), each with a floor that can fire -- a search for a tab
named `Wagter` must find nothing, the Workspaces scan must still find
`LOD Generation`, both halves of the tab's scoping are asserted in one run, and
R2's comparison is shown going red with the menu bar 4 px taller (the
arithmetic is grown, never the widget). **T4 is named after the hook-up**: a
red there with everything else green means the `setLeftColumnMode` clamp edit
did not land. `water_weights.sh`'s floor rises 15 -> 16 because X2b became two
gates.

**Syntax, not a build.** `sx_WATER7.sh` (flags read out of `Makefile.Release`)
returned RC=0 on all five owned translation units, and the hook-up's own
inserted text was proved to compile without touching the shared tree:
`scratchpad/water7_20260910/sx_overlay.py` applies `hookup.py`'s EDITS table to
copies under `sx/` and the patched `nifskope_ui.cpp` compiles RC=0 with that
directory first on the include path -- self-proving, since `mode > LeftWater`
only compiles against the patched header. It proves the files compile and
nothing about linking, moc or behaviour.

**Line endings.** Every file this lane touched is LF-only and stayed so, CR 0
before and after, measured with Python byte counts: `src/watermarkpanel.h`,
`src/watermarkpanel.cpp`, `src/waterwindow.cpp`, `src/watercurves.cpp`,
`src/watermark.cpp`, `src/wwskin.h`, the three edited spells, and the two new
files `src/wateruitest.cpp` and `tests/spells/water_ui.sh`. `MISTAKES.md`
gained three entries, append-only, 204,423 -> 208,304 bytes, CR 0 -> 0.
`res/style.qss` was not changed: the row's padding is emitted at run time from
a height a static sheet cannot know, and the sheet's own `4px 8px` is what the
off state falls back to. The `.lodl` contract and the curve json are untouched.

**Pictures: OWED, and refused rather than faked.** At 17:23 BUILD11 was still
writing its gate logs, so launching a NifSkope instance for a before/after grab
would have broken "one instance ever" in the middle of another lane's runs.
`water_ui.sh` takes both (`SHOT=` the whole top strip, `TABSHOT=` the left dock
with the Water tab open) and the resume says which exe is still a valid BEFORE.
