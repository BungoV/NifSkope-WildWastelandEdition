---
name: ww-qss-geometry-probe
description: Answer "what will this stylesheet actually do to that widget's size or position" by BUILDING a 40-line Qt program against the shipped sheet, in about two minutes, without the NifSkope build slot -- instead of reasoning about Qt's box model and finding out four minutes and one relink later. Reproduce the WRONG number first, sweep the parameter, invert the map, and only then write the fix into the tree. Use before any change to res/style.qss or a wwskin QSS helper whose gate is a pixel, and whenever a lane is about to spend its one build on "this should make it 35".
---

# Measure a Qt stylesheet outside the application

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane UI3
(2026-09-10), which had to make four tool buttons read 35 px in a 35 px row
after lane WATER7 had shipped them at 39 and BUILD12 had measured it.

## 1. Why not just reason about it

Every one of these was believed, and every one is wrong:

* *"`min-height` sets the widget's minimum height."* It sets a minimum on the
  **contents**, which Qt then adds `CT_ToolButton`'s own 3 px to, then the
  padding, then the border.
* *"`max-height` will clamp it."* On the QToolButton path Qt does not consult
  it at all. Setting it changes nothing, silently.
* *"The bar's sheet reaches the buttons in the bar."* Only for properties the
  BUTTON's own stylesheet does not mention: a widget's own sheet outranks every
  ancestor's whatever the selectors say, so any `setStyleSheet` on the button
  (`wwBoxedButtonQss` and friends) wins.
* *"A button inside a 35 px bar cannot be taller than 35."* It can. A
  `QToolBarLayout` gives the row the tallest item's hint and lets it hang over,
  clipped, starting `PM_ToolBarItemMargin + PM_ToolBarFrameWidth` px down.

One NifSkope build is ~4 minutes and there is one per session. A probe is ~15
seconds to compile and instant to run, and it tells you the number.

## 2. The rig

`scratchpad/<lane>/probe.cpp`, written with the WRITE TOOL, and a two-line
build script. Copy `scratchpad/ui3_20260910/probe.cpp` and `probe2.cpp`.

```sh
g++ -std=gnu++2a -O1 -DUNICODE -D_UNICODE -DWIN32 \
    -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB \
    -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtWidgets \
    -IC:/msys64/ucrt64/include/qt6/QtGui -IC:/msys64/ucrt64/include/qt6/QtCore \
    scratchpad/<lane>/probe.cpp -o release/<lane>_probe.exe \
    -lQt6Widgets -lQt6Gui -lQt6Core
```

run from an MSYS2 UCRT64 login shell, then
`./release/<lane>_probe.exe -platform offscreen` **from the repo root**.

* **`release/style.qss`, not `res/style.qss` -- AND SUBSTITUTE IT YOURSELF.**
  The release copy is the one the application reads, but it is a BYTE COPY:
  `grep -c '${' ` returns the same count on both files. The application strips
  the comments and replaces every `${name}` at load
  (`src/nifskope_ui.cpp:30194-30211`, the `skinVars[]` table at `:299`, plus
  `${theme}` and `${rgb}`). Feed the file to `QApplication::setStyleSheet` raw
  and Qt drops every declaration carrying a colour, in silence -- lane UI4's
  first probe lost `QMainWindow::separator { width: 3px }` that way and
  measured a 6 px dock gap the application does not have, which would have put
  its fix 3 px out. So the probe carries the dark column of `skinVars[]` as a
  small table and does the same three replacements before
  `app.setStyleSheet(...)`. Copy `scratchpad/ui4_20260910/probe.cpp`'s
  `substitute()`.
* **A style METRIC needs a widget.** `pixelMetric( PM_..., nullptr, nullptr )`
  answers the BASE style even under a stylesheet: lane UI4 measured
  `PM_DockWidgetSeparatorExtent` = **3** asked with the QMainWindow and **6**
  asked with nullptr, for the same sheet in the same process. Any helper that
  builds a sheet from a metric therefore has to take a widget, and the probe is
  where that is discovered rather than in the build.
* **`-platform offscreen`**: no window on anybody's monitor, real layout passes,
  real size hints. The NifSkope one-instance rule does not apply -- this is a
  different binary -- but there is no reason to show a window.
* It links Qt only. It cannot touch `release/NifSkope.exe`, `GeneratedFiles/`
  or the Makefile, so it is safe while a build lane holds the slot, and safe on
  account B.
* Reproduce the widget the way the application builds it: the same
  `setToolButtonStyle`, `setPopupMode`, `setIconSize`, and **its own
  `setStyleSheet` if it has one** -- that last is usually the cause and leaving
  it out gets a clean, wrong answer.

## 3. The procedure

1. **Reproduce the wrong number first.** Case 0 of the probe is the state that
   shipped. If it does not print the number the harness measured in the real
   window, the rig is not the widget: fix the rig, not the theory. UI3's
   printed `height=39 sizeHint=42` against the application's 39 -- that match
   is what licensed everything after it.
2. **One case per hypothesis, all in one run**, printed side by side: no sheet,
   the shipped sheet on the bar, the shipped sheet on the bar AND the buttons,
   each candidate. This is where "the widget's own sheet wins" stops being a
   doc quote and becomes two lines that differ by 10 px.
3. **Print `height()` AND `sizeHint()` AND `y()`.** They disagree, and the gate
   reads `height()`. A fix tuned to the hint lands 3 px out; a fix that ignores
   `y()` puts a correct-height button half outside its bar.
3a. **When the change is a MARGIN, no rect will show it -- read the pixels.**
   QSS margins on `QTabBar::tab` are honoured, and `QTabBar::tabRect()` returns
   the rect INCLUDING the margin: lane UI4 measured `128/127/128 at y 0, h 35`
   both before and after giving every segment 4 px of air. Anything the style
   draws INSIDE a widget's own box -- a margin, a `menu-indicator`, a
   `::handle`, a focus ring -- moves without moving a rect. Grab the widget
   (`w->grab().toImage()`), pick a colour only the thing under test carries
   (the SELECTED segment's fill is ideal: one colour, nothing else in the strip
   has it), and take the bounding box of that colour; divide by
   `devicePixelRatio()` to come back to logical pixels. Search for a colour the
   widget does NOT carry in the same run, so a scan that has stopped finding
   anything cannot pass for a widget with no air. The application's gate then
   uses the same function, which is how the probe's number and the harness's
   number become the same number.
4. **Sweep the parameter and print the map.** `for content = 18..40`, one line
   each. It takes ten more lines and it replaces the entire argument: the map
   showed `height = content + 6`, a flat `y 4`, and -- once the bar's box was
   zeroed -- a **basin** where content 26..29 all landed on exactly 35 at y 0.
   Aim at the middle of a basin, never at a point solution.
5. **Then write the fix**, knowing the number. And if the code can MEASURE what
   the sweep measured (ask for a known content height, read the hint back,
   subtract), do that instead of writing the constant down: the probe's job is
   to prove the calibration converges, not to hand you a magic 9.

## 4. What the probe cannot tell you

It has no theme switch, no HiDPI, no `restoreState`, and no other lane's
widgets. It answers "what does this sheet do to this widget", not "does the
window still look right" -- so:

* **The picture still decides.** UI3's first build was green at 1 px and the
  grab showed three dropdown arrows had moved to the bottom-right corner of
  buttons that were now tall enough to show them. A probe measuring heights
  cannot see that. When a rule changes a widget's BOX, list the subcontrols the
  style places against that box -- `menu-indicator`, `down-arrow`, the toolbar
  chevron, `::separator` -- and state them in the same sheet.
* Keep the probe and its output under `scratchpad/<lane>/` and cite it from the
  report: it is the measurement the claim rests on (CONSTITUTION 4), and the
  next lane to touch that sheet re-runs it in seconds.

## 3c. A widget with no background of its own grabs onto WHITE (lane UI6, 2026-09-11)

Sections 3a and 3b both say "grab the widget and read the pixels". On an
**auto-raise QToolButton** that produces a picture with nothing in it.

`QWidget::grab()` allocates a QPixmap and renders the widget into it. A button
that paints no background of its own -- `autoRaise`, or any QSS rule whose
`background` is `transparent` -- leaves whatever the pixmap was filled with,
and Qt fills it **white**. Measured, lane UI6's probe: background `#efefef`
(luminance 239), the theme's glyph colour `#e6e8eb` (luminance 232), **seven
levels apart**. Every ink scan in 3b reported "found nothing" on four buttons
that were drawn perfectly, while the button beside them with a dark label read
fine -- so the failure looks like a per-widget bug rather than an instrument.

Render over a fill you chose instead:

```cpp
QPixmap pm( w, h );
pm.fill( QColor( skin( "bgBar" ) ) );          // the bar's own colour
b->render( &pm, QPoint(), QRegion(), QWidget::DrawChildren );
const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
```

`DrawChildren` without `DrawWindowBackground` keeps the widget's own painting
and does not erase the fill. The same call works in the in-application harness,
so the probe's number and the gate's number stay one number.

**And the same trap in the probe's own fixtures**: an icon built as
`QPixmap pm; QPainter p(&pm); ...; return QIcon(pm);` copies the pixmap while
the painter is still attached to it, and the icon comes out empty. `p.end()`
before the copy. A rig whose fixture is invisible reports the widget as
correct-and-empty, which is indistinguishable from the defect under test.

## 3d. Isolating a SUBCONTROL: two renders, one pinned geometry (lane UI6)

When the thing under test is a subcontrol the style draws -- a
`menu-indicator`, a `down-arrow`, a `::handle` -- no rect says where it went
(3a), and its ink runs into the widget's own ink, so a single scan cannot tell
the two apart. Take **two renders of the same widget** and diff the columns:

1. pin the geometry (`setFixedSize` at the current size), so nothing can move;
2. render once as it stands;
3. take the thing that makes the style draw the subcontrol away --
   `setMenu(nullptr)` for a menu indicator, and its default action's menu too;
4. render again, restore the menu and the size hints;
5. the columns that DIFFER are the subcontrol; the last INK column of the
   second render is the glyph's; the gap between them is the number.

Its own floor comes free: if no column differs, the subcontrol was never drawn
and the read is refused by name rather than reported as a huge gap.

**Pin the whole ROW, not one widget at a time.** Lane UI6's first version
pinned and unpinned each button inside its own measurement, so a toolbar of
thirteen buttons re-laid out thirteen times per sweep; the third sweep of a
CORRECT window read one button a pixel narrow and the gate's restore half went
red. One layout state for the whole sweep: pin every widget, measure them all,
unpin them all.

## 3b. When the thing that moved is TEXT, read the INK, not a colour (lane UI5, 2026-09-10)

Section 3a says a margin needs the pixels rather than a rect. There is a second
case it does not cover: the widget's own box did not move at all, and the thing
under test is where the TEXT sits inside it.

Lane UI5 had to centre `File / View / Spells / Options / Help` in a 35 px menu
row. Once the row states the menu item's vertical padding, the item's
`actionGeometry()` and its painted box are **both exactly the row** -- before
and after -- and the marker-colour trick of 3a measures the box, so it reads
"centred" in both states too. Only the glyphs moved.

The instrument is the same grab, read by BRIGHTNESS instead of by colour:

* take the widget's grab, find the commonest colour in it -- that is the bar's
  own background, whatever the theme is -- and call a pixel INK when its
  luminance (`0.299 r + 0.587 g + 0.114 b`) is more than about 40 above the
  background's. No colour is typed, so the same code works on either theme.
* group the ink columns into words by their gaps (5 empty columns is a word
  break for a menu bar), or bound each item by the x range of its own
  `actionGeometry()` when the rects are trustworthy horizontally -- they usually
  are even when they lie vertically.
* the number to compare is `(top + bottom) / 2` against `(height - 1) / 2`.
* **The floor is a LIFT on the threshold**, not a second colour: ask the same
  scan for a brightness the bar cannot carry (`+255`) and it must find nothing.
  A scan that has quietly stopped finding anything cannot then pass for centred
  text.
* Expect the text's centre to sit about **one pixel below** its own content
  box's centre -- ascender-heavy words with no descenders ("File", "Help") are
  drawn high in their line box. So a perfectly centred BOX gives an ink offset
  of +/- 0.5 and no integer padding split does better. Gate the ink at 1 px, not
  at 0, and say which of the two the report is quoting.
* An offscreen probe and the real window disagree by half a pixel on the ink's
  bottom row, because the probe's grab has no sub-pixel antialiasing. Case 0
  agreeing to 1 px is the pass; demanding 0 is measuring the platform.

The same function then goes into the in-application harness, so the probe's
number and the gate's number are one number.

## 3c. When the thing under test is a STATE, put the widget in it (lane UI5-HOVERPIC, 2026-09-11)

A probe usually renders the resting widget. Half the rules in 
only exist in a state -- , , ,  -- and the
geometry question is often about THAT box, not the resting one. Lane UI5-HOVERPIC
had to show the hover/open highlight behind a menu title before and after the
titles were centred (20 px -> 35 px, the whole row).

Drive the state programmatically and never by hand:

* **A menu bar title**: a synthetic  at the centre
  of , NO buttons down, through .
  QMenuBar's  then takes ,
  which sets  and repaints WITHOUT opening a popup -- so
   is still the bar.  also works but
  pops the menu up, which puts the thing you want to photograph in another
  window. Code the second as a fallback and PRINT which one took.
* Assert the state actually took () before believing the
  grab; a state that silently did not arm gives a clean picture of the wrong thing.
* Read the state's box by its own colour with the 3a scan -- a 
  background is a solid fill, so the bounding box is exact -- and carry TWO floors:
  the same scan asked for a colour the widget cannot carry must find nothing, AND
  a sibling that is NOT in the state must find nothing either. The second is the
  one that catches a sheet that lit the whole bar.
* Have the picture script re-measure the number out of the SAVED png and write it
  into its own label, so the number under the picture and the number in the
  picture cannot drift.

## 3c. When the thing under test is a STATE, put the widget in it (lane UI5-HOVERPIC, 2026-09-11)

A probe usually renders the resting widget. Half the rules in `res/style.qss`
exist only in a state -- `:selected`, `:hover`, `:pressed`, `:checked` -- and the
geometry question is often about THAT box, not the resting one. Lane UI5-HOVERPIC
had to show the hover/open highlight behind a menu title before and after the
titles were centred: 20 px at the top of the row, against the whole 35 px row.

Drive the state programmatically and never by hand:

* **A menu bar title**: a synthetic `QMouseEvent(QEvent::MouseMove)` at the centre
  of `actionGeometry(a)`, NO buttons down, through `QApplication::sendEvent`.
  QMenuBar's `mouseMoveEvent` then takes `setCurrentAction( a, popupState=false )`,
  which sets `State_Selected` and repaints WITHOUT opening a popup -- so
  `bar->grab()` is still the bar. `QMenuBar::setActiveAction()` also works but
  pops the menu up, which puts the thing you wanted to photograph in another
  window. Code the second as a fallback and PRINT which one took.
* Assert the state actually armed (`bar->activeAction() == a`) before believing
  the grab. A state that silently did not arm gives a clean picture of the wrong
  thing, and no rect will say so.
* Read the state's box by its own colour with the 3a scan -- a `:selected`
  background is a solid fill, so its bounding box is exact -- and carry TWO
  floors: the same scan asked for a colour the widget cannot carry must find
  nothing, AND a sibling that is NOT in the state must find nothing either. The
  second is the one that catches a sheet that lit the whole bar.
* Have the picture script re-measure the number out of the SAVED png and write it
  into its own label, so the number under the picture and the number in the
  picture cannot drift.

### The shell will eat your backticks

This section was first appended with `python -c "..."` from a bash prompt, and
every backticked span in it came back EMPTY: inside a double-quoted bash string a
backtick is command substitution, so `` `res/style.qss` `` ran as a command and
was replaced by its output. Write the text to a `.py` file with the Write tool
and run that file, or use a quoted heredoc. Then read the tail back and count the
backticks -- the damage is invisible in a success message.
