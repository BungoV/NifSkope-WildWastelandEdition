---
name: nifskope-ww-panel-style
description: The house style for a NifSkope Wild Wasteland Edition dock or settings panel (E:\Projects\NifskopeWildWastelandEdition) -- the shared helpers every control must go through (wwHeading, wwMakeScrubField, wwMatchFieldStyle, wwGuardWheel, wwSkinColor), the one-field-per-row grid, folding sections, the three-band layout with a pinned action bar, the summary-or-refusal line, Blender as the reference, and the self-test counts that prove each of them with a floor. Use before writing or changing any QWidget panel in the fork; the LOD Generation panel shipped without all of it once (2026-09-06) and bungo saw every miss from one screenshot.
---

# NifSkope WW: how a panel is built

Reference implementation: `src/lodgenmanager.cpp` (`LodgenPanel`, `LodgenSection`) and its
self-test `WW_LODGEN_TEST` in `src/nifskope_ui.cpp`. The rules, each of which was a defect once:

## Controls (there is one implementation of each; use it)
* **Sections** are `wwHeading( text, parent )` labels (`wwskin.h`), never a `QGroupBox` title.
  A section that greys as one thing is a plain `QWidget` with the heading inside it.
* **Numbers** are `QSpinBox`/`QDoubleSpinBox` + `wwMakeScrubField( s )` (`ui/widgets/wwnumberfield.h`),
  or `WwNumberField`. Nothing sweeps a dock for you: the sweep covers only the Settings panes.
* **Selectors** take `wwMatchFieldStyle( combo )`, or a combo is a different species beside a number.
* **The wheel scrolls the panel, not a value.** Both helpers above apply `wwGuardWheel()` (a field
  takes the wheel only while focused -- Blender's rule); any other field in a scroll area gets it by hand.
* **Colours** come from the skin table (`skinVars[]` in nifskope_ui.cpp) through `wwSkinColor("...")`
  or `${...}` in `res/style.qss`; never a literal. Muted hints: `textMuted`; a refusal: `danger`;
  a ticked box: `toggle` (Blender's blue, white mark from `:/wnd/check.png`).
* **Check boxes and radios** are styled app-wide in `res/style.qss` -- Fusion's box measured 0 levels
  against this theme's ground. Do not restyle them per panel.

## Layout
* One `label | field` `QGridLayout` per section, `setColumnStretch( 1, 1 )`, ONE setting per row,
  `setColumnMinimumWidth( 0, labelW - indent )` with one `labelW` for the whole panel so the value
  edge is one line down the page. Whole-word labels; the explanation is the tooltip, never after a
  dash in the label. A label greys with its field (`Form::add` returns it for that).
* An output with settings is a folding section (`LodgenSection`: arrow folds, box enables, fold
  persists, a greyed body stays open). Things kept for compatibility start folded.
* Three bands: settings in a `QScrollArea`; the live part (map, bar) on a `QSplitter` below it,
  sizes persisted; the summary line and the buttons pinned under both, never scrolling away.
* One choice a user can make without knowing the formats (a Target) ticks what its reader needs
  and HIDES what it cannot use, the way Blender hides the other render engine's panels. The run
  reads tick-and-visible (`wantX()`), never the box alone: a tick under a hidden section is a
  saved setting, not a request.
* The action bar says what will happen, in words, or the one reason it cannot: `refreshSummary()`
  owns the button's enabled state. A greyed button with no sentence beside it is a broken button.
* The output is the mod folder itself, one field; in Mod Organizer a mod folder IS a Data folder.
* Assets come from the game's own folders and archives (Settings > Resources); no "unpacked Data
  folder" field. Meshes need the generator's own index (`lodgenMeshArchives()`): the manager's
  Fallout 4 filter drops every `.nif` at index time.
* Blender is the reference for anything the fork has no language of its own for; state divergences.

## The self-test counts each rule, with a floor
Copy the block in `WW_LODGEN_TEST`: plain spin boxes without the `wwScrubbed` stamp (0, of N >= 8),
group boxes (0) against weight-600 headings (>= 4), selectors without the matched `drop-down` rule
(0, of >= 4), check-box labels carrying " - " (0) and outputs without a tooltip (0), settings on
distinct rows (geometry, not layout class), the button and the map outside the scroll area, the
target hiding/unticking/restoring, the fold, the refusal and the summary text, an unticked box's
contrast against the ground (>= 24 levels, measured on the page's own render, not the box's grab),
white mark pixels inside a ticked box, and the wheel over an unfocused number leaving it while
stepping it once focused. `SHOT=<png>` grabs the dock; look at it after any layout change.

## ARRANGE THE DOCK BEFORE YOU GRAB IT (2026-09-11, lane LODUI1)

`SHOT=<png>` grabs the dock AS IT STANDS, and as it stands is almost never the
picture you want: the settings open scrolled to the top, so a grab shows Source,
Plugins and Resources, and the rows the lane changed are below the fold. Lane
LODUI1 spent TWO relinks on this -- the first grab showed none of its rows, the
second reached one section further and still missed them.

Three moves, in this order, immediately before `dock->grab()`:

1. `resizeDocks( { dock }, { 640 }, Qt::Horizontal )` -- the width.
2. **HIDE the pane that steals the height**, do not argue with the splitter.
   `LodgenSplitter`'s lower pane holds a map with a 160 px minimum and about
   140 px of furniture around it, so `setSizes( { 2000, 150 } )` moves nothing
   and the settings never get past ~270 px of a 741 px dock. `sp->widget( 1 )->
   setVisible( false )`, grab, then put it back -- and put it back through a
   named lambda beside the one that hides it, so a second grab site cannot
   forget.
3. **Scroll to the row under test, not to the top of its section.**
   `scroll->ensureWidgetVisible( anchor, 0, 300 )`. When the panel has per-target
   rows, pick the anchor that is VISIBLE at the current target
   (`if ( !anchor || anchor->isHidden() ) anchor = <the other one>;`) or the
   other target's grab centres on nothing.

Then `processEvents()` twice and grab. **And open the image.** A picture that
cannot contain the thing it is offered as proof of is not proof (CONSTITUTION
5), and no count will tell you it was framed wrong.

## Bars that meet along one line share ONE row (2026-09-10, lane BUILD9)

bungo, on a screenshot: *"See the issue with alignment here?"*. The window's top
strip is not one bar but several, in different parents -- the main toolbars in
the QMainWindow's toolbar area, the viewport's toolbar as the first widget of
the central column, the left dock's mode selector as the first widget of the
dock. **No layout contains more than one of them, so no layout can ever make
them agree**; something has to state the rule. Measured before it was stated:
main toolbars 35 px, viewport toolbar 33, dock tab strip 26 -- a 7 px step at
the seam, and the dock's second row starting 3 px above the line the viewport's
content starts on.

* The rule lives ONCE, in the shared skin helpers, never as three
  `setFixedHeight` calls at three call sites -- which is how they came to
  disagree. `wwAlignBarRow( { ...bars } )` in `src/wwskin.h` takes the TALLEST
  natural height among them and gives it to all; `wwBarRowHeight()` reads it
  back; `wwStartContentBelowBar( page )` zeroes the top margin of a dock page
  whose first row has to begin on the viewport's content line.
* **Grow, never shrink.** A squeezed `QToolBar` does not clip: it hides controls
  behind an extension chevron, silently, with the buttons still "there" as far
  as any code can tell.
* A tab strip that joins the row must have its TABS restyled to fill it, or it
  sits in the top 26 px of a 35 px band with a gap underneath.
  `wwSegmentedTabBarQss( rowHeight )` does the arithmetic (min-height = row
  minus the sheet's own padding and border) so no second number is typed at the
  call site.
* **Call it after `restoreState`**, not at construction: `restoreState` replays
  a saved layout, and the viewport header is only filled afterwards, so that is
  the first moment every bar exists in its final parent with its final contents.
* The gate is GEOMETRY, not a picture: `tests/spells/ui_align.sh` /
  `src/uialigntest.cpp` read each bar's `QRect` in MAIN-WINDOW coordinates
  (`w->mapTo( skope, QPoint(0,0) )` -- two widgets in different parents cannot be
  compared any other way), assert equal top and height within 1 px, and carry
  floors: every rect non-degenerate, the two widgets distinct, and the SAME
  comparison shown going red when one bar is grown by 4 px. The picture is a
  grab of the seam region before and after, which is what bungo looks at.

## A HIDDEN ROW READS AS ITS DEFAULT, AND NEVER OVERWRITES WHAT WAS TYPED (2026-09-12, lane PANEL1)

Rows that only one target can use are hidden under the other, and the panel now
has enough of them that the rule has to be stated:

* The RUN reads a hidden row as the command line's DEFAULT, not as the widget's
  value -- a setting the target cannot use must not reach the bake. In this
  panel that is `xvar()`, and it is the same tick-AND-VISIBLE discipline the
  existing rows already used for check boxes.
* The SAVE writes what the WIDGET says, not what the run used, so switching
  targets never destroys a number a person typed. `xraw()` for saving,
  `xvar()` for running.
* Rows that are merely GREYED (a dependent number under an option that is off)
  are visible and still save and load normally; greying is a statement about
  relevance, not about value.
* A compound selector that SETS the numbers below it hides them rather than
  greying them, so the panel never shows two answers to one question.
