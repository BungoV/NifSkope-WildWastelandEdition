# Lane UI5-HOVERPIC -- the hovered/open menu title, before and after lane UI5

Deliverable: `scratchpad/ui5_20260910/images/cmp_menu_hover.png` (1080x382).
The menu bar `File / View / Spells / Options / Help` with **Spells hovered**,
BEFORE lane UI5 on top, AFTER below, 4x nearest, a red rule between the halves,
each half labelled -- the layout of `images/cmp_menu_zoom.png`.

## What each half was rendered from

Neither half is `release/NifSkope.exe`: lane UI6 holds the one allowed instance
and is building. Both halves come from **one standalone Qt program**,
`scratchpad/ui5_20260910/hoverprobe.cpp`, built by
`scratchpad/ui5_20260910/build_hoverprobe.sh` (MSYS2 UCRT64, `g++`, links
`Qt6Widgets/Qt6Gui/Qt6Core` only) into `release/ui5_hoverprobe.exe` and run
`./release/ui5_hoverprobe.exe -platform offscreen` from the repo root. Raw output:
`scratchpad/ui5_20260910/hoverprobe_out.txt`. The picture is composed by
`scratchpad/ui5_20260910/make_hoverpic.py`.

The rig follows `ww-qss-geometry-probe`:

* **`release/style.qss`, substituted by the probe itself.** The release copy is a
  byte copy of `res/style.qss`; the `${...}` tokens are NOT substituted on disk.
  The probe carries the dark column of `skinVars[]`
  (`src/nifskope_ui.cpp:299`) and does the same three replacements the
  application does at `src/nifskope_ui.cpp:30199-30211` -- comments stripped,
  `${theme}`, every `${name}`, then `${rgb}` -- before `setStyleSheet`. Feeding
  the file raw would have dropped every declaration carrying a colour, and the
  thing under test here IS a colour: `res/style.qss:49`
  `QMenuBar::item:selected { background: rgba(${rgb}, 255); }` = `#4a7ab0`.
* The widget is built the way the application builds it: a real `QMenuBar` on a
  `QMainWindow`, `setNativeMenuBar(false)`, the five real titles with mnemonics
  (`&File … &Help`), height pinned to 35 exactly as `wwAlignBarRow` pins the row
  before it applies its sheets (`src/nifskope_ui.cpp:867`).
* **BEFORE half** -- the row sheet as it stood before lane UI5:
  `wwBarRowBoxQss()` + `wwBarRowButtonQss( 26 )` and nothing else on
  `QMenuBar::item`, i.e.
  `QMenuBar::item { min-height: 26px; padding-top: 2px; padding-bottom: 2px; }`.
  The `min-height` is not consulted on this path (lane UI5's report, section 2,
  fifteen cases), so the item is `2 + 16 + 2 = 20` px at the top of the 35-px row.
* **AFTER half** -- the same sheet plus `wwBarRowMenuItemQss( 35, 16 )` appended
  LAST, exactly as `applyRow` appends it:
  `QMenuBar::item { padding-top: 9px; padding-bottom: 10px; }`. The item's
  content height of 16 is **measured, not typed**: the probe applies the row
  sheet with both vertical paddings zeroed, calls `ensurePolished()` with no
  event loop, and reads the tallest `actionGeometry().height()` back -- the same
  calibration `wwAlignBarRow` performs at `src/nifskope_ui.cpp:922`. It printed
  `item content 16 px (no event loop), row 35 -> padding-top 9 / bottom 10`.
* **The hovered state is set programmatically**, never photographed by hand: a
  synthetic `QMouseEvent(QEvent::MouseMove)` at the centre of the Spells item,
  no buttons down, sent with `QApplication::sendEvent`. That takes QMenuBar's
  `mouseMoveEvent` -> `setCurrentAction( action, popupState = false )`, which
  sets `State_Selected` on that item and repaints without opening a popup
  window, so the grab is the BAR. `QMenuBar::setActiveAction` is coded as the
  fallback; it was not needed -- both halves report
  `hovered "Spells" by synthetic QMouseEvent(MouseMove); activeAction = Spells`.
* **The image is `QWidget::grab()` of the menu bar**, 1512x35, saved as
  `images/hover_bar_before.png` / `images/hover_bar_after.png`. Never a desktop
  capture. Offscreen platform, so no window on anybody's monitor.

## The two hover-box heights, measured from the grabs

The number is the bounding box of the selection colour `#4a7ab0` inside the
hovered title's own x span, read out of the saved PNG in logical pixels -- not
read off `actionGeometry()`.

| half | hover box | height | row |
|---|---|---|---|
| BEFORE lane UI5 | y **0..19** | **20 px** | 35 |
| AFTER lane UI5 | y **0..34** | **35 px** | 35 |

`make_hoverpic.py` re-measures the same two numbers independently, from the
saved PNGs, and prints them into the picture's own labels, so the number under
the picture is the number in the picture.

**The floors, both green in the same run** (a scan that has quietly stopped
finding anything must not pass for a result):

* the same scan asked for a colour the bar cannot carry (`rgb(1,254,3)`) finds
  **nothing**, in both halves;
* the unhovered `File` title carries **no** highlight, in both halves -- so the
  20 and the 35 belong to the hovered item and not to a bar that is lit
  everywhere.

`actionGeometry()` agrees with the pixels in both halves (`h 20` before, `h 35`
after), which is the point lane UI5 made when it chose padding over a margin:
the rect stays honest.

## Caveat

**This is a probe render, not an in-app grab.** It answers "what does this sheet
do to this widget" and nothing else. It is not `release/NifSkope.exe`, it has no
`restoreState`, no other widget in the row, one theme (dark), and a device pixel
ratio of 1; an offscreen grab also has no sub-pixel antialiasing, so glyph edges
differ from the real window by about half a pixel (lane UI5 measured that
directly: probe ink centre 10.5 against the application's 11.0). The two heights
above are box fills, not glyphs, so they are not affected -- but the picture is
still a reconstruction of the two states, not a photograph of two builds. The
in-app before/after of the same row, from the two real exes, is
`images/cmp_menu_zoom.png` (lane UI5), which shows the titles' position rather
than the hover band.

Nothing under `src/`, `res/`, `tests/`, `tools/` or `NifSkope.pro` was touched by
this lane; the only files written are under `scratchpad/ui5_20260910/` plus
`release/ui5_hoverprobe.exe` and one skill file (below). (Files of lane UI6's
that changed during this work are UI6's own; this lane never opened them.)

## Finished-work skill review (CONSTITUTION 1a)

**Loaded and used:** `ww-qss-geometry-probe` -- the whole rig. Section 2's rule
that `release/style.qss` is NOT substituted on disk is what made the highlight
colour exist at all; section 3a's colour-bounding-box scan with a floor that must
find nothing is what the two heights are measured with.

**Written, and the director should mirror it to
`E:\\Projects\\Claude\\.claude\\skills`** (the two-tree rule): a new section
**"3c. When the thing under test is a STATE, put the widget in it"** in
`.claude/skills/ww-qss-geometry-probe/SKILL.md` (10,395 -> 14,244 B, CR 0). The
skill covered resting geometry only; half of `res/style.qss` exists only in
`:selected` / `:hover` / `:pressed` / `:checked`, and how to arm that in a probe
without opening a popup window -- plus the sibling-not-lit floor -- had to be
worked out from scratch here. It carries a second sub-section on the shell trap
below.

## Mistakes (for the director to splice into `MISTAKES.md`)

**2026-09-11 -- a document was appended through `python -c "..."` from bash and
every backticked span in it was silently deleted.** The section 3c text was
written into the skill with `python -c` inside a double-quoted bash string; bash
treats a backtick as command substitution, so every `` `code span` `` ran as a
command and was replaced by its (empty) output, while the wrapper still printed a
correct-looking size and `CR 0`. Found by reading the file's tail back
afterwards. The rule: text with backticks, quotes or `$` goes into a `.py` file
written with the Write tool and is run from there, or through a quoted heredoc --
never `python -c` in a double-quoted shell string -- and the tail is read back and
the backticks counted before the append is believed. The damaged append was
truncated back to the original 10,395 bytes and redone; the file is now 14,244 B,
CR 0, 174 backticks.
