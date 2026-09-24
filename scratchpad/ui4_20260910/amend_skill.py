"""Lane UI4 -- amend ww-qss-geometry-probe in BOTH skill trees (CONSTITUTION 1a).

Two corrections, both paid for in this lane:
  * the release copy is NOT substituted -- the probe must substitute it itself,
    or it measures a stylesheet the application does not have;
  * a widget's PAINTED box is not always its rect (QTabBar::tabRect), so the
    probe has to be able to read pixels.

Anchored and refusing, like a hook-up: the anchor must match once in each tree.
"""
import sys

TREES = [
    r"E:\Projects\Claude\.claude\skills\ww-qss-geometry-probe\SKILL.md",
    r"E:\Projects\NifskopeWildWastelandEdition\.claude\skills\ww-qss-geometry-probe\SKILL.md",
]

OLD = b"""* **`release/style.qss`, not `res/style.qss`.** The release copy is the one the
  application reads and its `${...}` skin variables are already substituted, so
  the probe needs no colour table. `app.setStyleSheet(...)` it, whole.
"""

NEW = b"""* **`release/style.qss`, not `res/style.qss` -- AND SUBSTITUTE IT YOURSELF.**
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
"""

OLD2 = b"""3. **Print `height()` AND `sizeHint()` AND `y()`.** They disagree, and the gate
   reads `height()`. A fix tuned to the hint lands 3 px out; a fix that ignores
   `y()` puts a correct-height button half outside its bar.
"""

NEW2 = b"""3. **Print `height()` AND `sizeHint()` AND `y()`.** They disagree, and the gate
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
"""

OLD3 = b"""* **`-platform offscreen`**: no window on anybody's monitor, real layout passes,
  real size hints."""

NEW3 = b"""* **A style METRIC needs a widget.** `pixelMetric( PM_..., nullptr, nullptr )`
  answers the BASE style even under a stylesheet: lane UI4 measured
  `PM_DockWidgetSeparatorExtent` = **3** asked with the QMainWindow and **6**
  asked with nullptr, for the same sheet in the same process. Any helper that
  builds a sheet from a metric therefore has to take a widget, and the probe is
  where that is discovered rather than in the build.
* **`-platform offscreen`**: no window on anybody's monitor, real layout passes,
  real size hints."""

EDITS = [(OLD, NEW), (OLD2, NEW2), (OLD3, NEW3)]

rc = 0
for path in TREES:
    with open(path, "rb") as f:
        data = f.read()
    ok = True
    out = data
    for old, new in EDITS:
        n = out.count(old)
        print("%s: anchor %d bytes matched %d" % (path.split("\\")[-3], len(old), n))
        if n != 1:
            ok = False
            continue
        out = out.replace(old, new)
    if not ok:
        print("REFUSED for %s -- nothing written" % path)
        rc = 1
        continue
    with open(path, "wb") as f:
        f.write(out)
    print("amended %s: %d -> %d bytes, CR %d -> %d"
          % (path, len(data), len(out), data.count(b"\r"), out.count(b"\r")))

sys.exit(rc)
