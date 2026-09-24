<!-- Lane UI3, 2026-09-10. TEXT ONLY for WW_CHANGES.md; the director splices it
     (CONSTITUTION 8). WW_CHANGES.md is MIXED and its 2026-09 entries at the top
     are LF-only -- this text is LF-only and carries no CR. -->

## 2026-09-10 -- the buttons in the top row ARE the row: 39 px -> 35, and the gate that let 39 through (lane UI3)

bungo, of the aligned strip: *"compact these vertically like this, the top bar
and the buttons"*. WATER7 stated that rule and BUILD12 shipped it four pixels
out -- a **35 px** row carrying **39 px** buttons -- and `water_ui.sh` gate R3
passed it, because it allowed **8 px** where the spell's own header promised 1.

**Measured on `release/NifSkope.exe` 18:25:20 (20,773,888 bytes): every tool
button in the top row is now 35 px at y 0, against a row of 35 -- Workspaces,
LOD, Animation and Collision, worst offset 0.** Before: 39 px at y 4, with the
bottom 8 px of every box clipped off by the bar that held it.

**Why 39 won, measured outside the application.** Two standalone probes
(`scratchpad/ui3_20260910/probe.cpp`, `probe2.cpp`; Qt6Widgets only, they never
touch `release/NifSkope.exe`) rebuild the same `tView` toolbar with the same
four `QToolButton`s, the same `release/style.qss` and the same appended sheet,
and **reproduce 39 exactly**. Three causes, none of them the row height:

1. **A widget's own stylesheet outranks every ancestor's**, whatever the
   selectors say. All four buttons carry one -- `wwBoxedButtonQss`
   (`src/nifskope_ui.cpp:520`, set at `:27540` and `:26929`) -- so its
   `padding: 3px 6px` beat the row's padding before the row saw the button. A
   sheet on the BAR reaches only a property the button's own sheet does not
   mention, which is why WATER7's `min-height` arrived and its `padding` did not.
2. **QSS `min-height` on a QToolButton is a minimum on the CONTENTS**, not on
   the widget: Qt expands the contents to it, adds 3 px of its own for
   `CT_ToolButton`, then the padding and the border. `31 + 3 + 6 + 2 = 42`
   asked for, 39 laid out. `max-height` is not consulted on that path at all --
   the probe set it and nothing moved.
3. **A QToolBar starts its items 4 px below its own top**
   (`PM_ToolBarItemMargin 2 + PM_ToolBarFrameWidth 2`, measured), so even a
   button of exactly the row's height would hang past the bottom of a bar of
   exactly the row's height and be clipped there.

**The fix, through the skin, with the arithmetic measured rather than typed.**
`wwBarRowBoxQss()` (new) takes a bar's own box away, which puts its items at
y 0 and makes the layout CLAMP a button to the bar instead of letting it
overflow -- probe2 measured content 26..29 all landing on exactly 35 px, a
basin rather than a knife edge. `wwBarRowButtonQss( contentHeight )` states the
glyph line and the air above and below it and nothing horizontal, so the row has
one vertical box and every button keeps its own width. `wwAlignBarRow` appends
both to the bars AND to every tool button in them (cause 1 leaves no other
route), and CALIBRATES: it asks for a content height certainly taller than any
glyph line in the row, reads back what the style added to it, and takes that out
of the row -- so no Qt constant is written down and nothing here moves with a Qt
version. `wwBarRowButtonContent()` and `wwBarRowButtonOverhead()` read the two
numbers back. No `setFixedHeight` anywhere. `res/style.qss` was not touched.

**The way back is unchanged and still exact** (CONSTITUTION 7):
`UI/CompactTopBars = false` appends nothing and touches no widget's own sheet,
which is BUILD9's window to the pixel.

**The gate that let 39 through is now 1 px, and it can be seen going red.** R3
reads back EVERY tool button in `tFile` / `tLOD` / `tView` by name -- not just
the extremes -- against `wwBarRowHeight()`, within 1 px, and asserts the buttons
agree with each other within 1 px and that the calibration measured a real
overhead instead of falling back. Its floor is the deliberate wrong number, run
live in the same run and both halves of it: BUILD12's own arithmetic
(`min-height: row - 4`, `padding: (row - 18) / 2`) is appended over the shipped
sheet, the SAME predicate must go RED on it, and taking it away must put every
button back on the row -- which is also what makes the picture the spell writes
afterwards the shipped state. R4 now pins that the skin states one min-height
and one pair of vertical paddings per selector, nothing horizontal, and that the
bars actually carry the bar-box rule. The check-count floor is re-derived from
the group arithmetic, 24 -> 30, and the suite runs **37 checks, 0 failures**.
`ui_align.sh` 11/0, `top_bar.sh` 43/5, `files_tab.sh` 28/2 and `animws.sh`
57/0 are all exactly their BUILD12 baselines.

**One thing to look at, and it is a consequence rather than a change.** The
viewport header's **Global**, snap and grid buttons now show the dropdown arrow
they always had: a QToolButton's default `menu-indicator` sits in the
bottom-right corner, and theirs was inside the 8 px that the old
39-px-button-in-a-35-px-bar clipped away. The row's sheet now centres it, the
way `wwBoxedButtonQss` always did for the four boxed buttons -- so it sits
beside the glyph instead of dropping to the corner of a taller button, and the
gate pins that. On the two narrow icon-only buttons it touches the icon,
because `res/style.qss:205` gives them `padding: 1px 1px` and no room to the
right. Ways back, in order of size: a right margin on the row's indicator (one
line), or `UI/CompactTopBars = false`.
