<!-- Lane UI5, 2026-09-11. ENTRY TEXT for WW_CHANGES.md; the director splices
     it (CONSTITUTION 8). WW_CHANGES.md is mixed line endings and stays so --
     splice it in binary and match its neighbours. Every number below is
     measured on release/NifSkope.exe 2026-09-11 05:58:21, 20,855,296 bytes. -->

### File / View / Spells / Options / Help centred in the 35-px menu row (lane UI5)

bungo, 2026-09-10, verbatim: *"Also, please center file / view / spells /
options / help buttons, top left"*. Lane UI3 put the menu BAR in the shared
35-px row and said in its own handoff that the ITEMS were not made the row's
height, only the bar. They were not.

**Before**, measured on the shipped 20:45:47 window's own in-application grab
(`scratchpad/ui4_20260910/images/strip_after.png`, read by
`scratchpad/ui5_20260910/measure_before.py`): the five titles' text sat at
**y 6..16**, ink centre **11.0 / 11.5** against a row centre of **17.0** -- six
pixels high. The item's painted box was 20 px at the top of a 35-px bar.

**After**, measured in the live window by the gate: the titles' ink is at
**y 13..23 / 13..24** and their CAP BAND -- the topmost ink row down by the
font's own capHeight of 8, which is what a reader judges centring by -- is
centred at **17.0, offset 0.0, all five agreeing to 0.0**. The item's painted
box is the whole row. The row is still **35**, the menu bar's own size hint is
**35** (so aligning the row again cannot grow it), and tFile, tLOD, tView, the
viewport header and the dock tab strip are all still 35.

The whole ink band is NOT the letters: it also holds the mnemonic underline
under F / V / S / O / H and the descender of the `p` in Spells and Options.
Measuring that band instead of the cap band is what the first run of this gate
got wrong, and it is why the log prints both.

**The mechanism, measured before it was written** (30 cases outside the
application, `scratchpad/ui5_20260910/probe.cpp` + `probe_out2.txt`; case 0
reproduces the shipped offset to 0.5 px):

* `QMenuBar::item { min-height }` **is not consulted on this path at all** --
  fifteen cases from 22 to 36 px leave the item exactly 20 px tall. The row's
  sheet has stated one since lane WATER7 and it has never moved a pixel.
* a `::item` **margin** does move the item, but `actionGeometry()` grows with
  it, so the rect lies about where the item is drawn -- exactly as
  `QTabBar::tabRect()` did for lane UI4. Rejected: it cannot be gated.
* the two vertical **paddings** are consulted and leave the rect honest. The
  item's own content is 16 px, the row is 35, and splitting the 19 spare pixels
  9 above and 10 below centres the box exactly -- and, measured afterwards in
  the live window, puts the capital letters at y 13..21 against a row centre
  line of 17.0, which is dead centre. The font's own numbers say why (ascent 13,
  descent 3, height 16, capHeight 8).

**All of it in the shared skin, no `setFixedHeight` anywhere.** New
`wwBarRowMenuItemQss( rowHeight, itemContent )` states the one rule and nothing
horizontal; `wwAlignBarRow` MEASURES `itemContent` (it applies the row's sheet
once with both paddings zeroed and reads the item's height back with
`ensurePolished()` and no event loop -- probe case E, which matters because the
row is aligned during construction) and appends the rule LAST so it outranks
the min-height rule above it. `wwBarRowMenuItemContent()`,
`wwBarRowMenuItemPadTop()` and `wwBarRowMenuItemPadBottom()` read the three
numbers back. Fallback, named: a menu bar with no titles, or one whose items
read back taller than the row, takes the font's own line height.

**The way back is exact and is the same one key.** `UI/CompactTopBars = false`
returns an empty string here too -- one reader, `wwCompactTopBars()`, shared
with both other row sheets -- so the menu bar keeps `res/style.qss` alone and
the titles sit where they did before lane UI3. Gate M4 measures that state live.

**Gate**: `src/wateruitest.cpp` group M, **14 checks, all green**, and
`tests/spells/water_ui.sh` reads all 14 back by name with its count floor
48 -> 62. The suite reads **76 checks, 0 failures, 0 skips, PASS** (was 59 / 0);
`ui_align.sh` 11 / 0, `top_bar.sh` 43 / 5, `files_tab.sh` 28 / 2, `animws.sh`
57 / 0 -- every one of them exactly its baseline.

Group M reads PIXELS, not rects: once the row states the item's padding the
rect and the painted box are both the whole row, and it is the text inside them
that moved. Two floors above the verdict (every painted title found as real ink;
the same scan asked for a brightness the bar does not carry finds nothing), and
M5 is a live floor -- the 20:45:47 arithmetic appended over the shipped sheet,
the same predicate asked again, red, then taken away and green.

Files: `src/wwskin.h`, `src/nifskope_ui.cpp`, `src/wateruitest.cpp`,
`tests/spells/water_ui.sh`. `res/style.qss` was NOT touched, and neither was
`wwAlignBarRow`'s row-height arithmetic, `wwBarRowButtonQss`,
`wwBarRowBoxQss` or anything of the segmented strip.

**Three links, not one, and no application code changed after the first.** The
first run of group M -- a gate that had never been executed -- found two defects
in itself: it called the mnemonic underline "the text", and its way-back half
set an empty stylesheet, which the application does not re-resolve (a standalone
rig does; `scratchpad/ui5_20260910/probe.cpp` case I). Links 2 and 3 rebuilt
only `src/wateruitest.cpp`. Five MISTAKES entries.

**Side effect, not asked for, his call**: the hover band is now the height of
the row. A title's painted box IS the row now, so `QMenuBar::item:selected`
(`res/style.qss:49`) paints 35 px where it painted 20. That is Blender's
behaviour and it is what "centred in the row" means geometrically.

Pictures: `scratchpad/ui5_20260910/images/cmp_menu_zoom.png` (File..Help from
both exes at 4x, labelled), `toprow_after.png`, `cmp_toprow.png`.
