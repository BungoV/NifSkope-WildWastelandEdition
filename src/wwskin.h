#ifndef WWSKIN_H
#define WWSKIN_H

#include <QList>
#include <QString>

class QWidget;


/*! Skin colour by name — the same table `style.qss`'s `${...}` variables are
 * substituted from (`skinVars[]` in nifskope_ui.cpp), exposed to C++ so the
 * per-widget stylesheets scattered through the docks stop hardcoding greys.
 *
 * Returns a `#rrggbb` string for the CURRENT theme, so a sheet built with it
 * follows Dark/Light like the main stylesheet does. Names are the variable
 * names without the `${}`: "bg", "bgWin", "bgBar", "bgPanel", "bgAlt",
 * "bgCard", "bgInput", "bgBtn", "bgBtnHover", "bgBtnDown", "bgHeader",
 * "border", "borderDim", "borderStrong", "focus", "scroll", "scrollHover",
 * "text", "textMuted", "textBright", "accent", "accentText", "accentBg",
 * "textDisabled", "accentDisabled", "toggle", "toggleDisabled", "danger",
 * "viewport", "selBgActive", "selBgInactive", "selTextActive", "selTextInactive",
 * and the animation sheet's six (lane UINOTES1, bungo's rulings 2 / 7b / 8,
 * values read out of the installed Blender 4.5): "animKey", "animKeySel",
 * "animKeySelOther", "animPlayhead", "animPlayheadText", "animOutOfRange".
 *
 * "textDisabled"/"accentDisabled" are the INERT register: `textMuted` and
 * `accent` mean "off" and "on" for a control you can click, and a control you
 * cannot click has to be visibly a different thing from one that is merely off.
 *
 * An unknown name returns an empty string and warns, which shows up as an
 * ignored CSS declaration rather than a wrong colour.
 *
 * Widgets built BEFORE the theme loads get the default (dark) column; rebuild
 * or restyle on `NifSkope::reloadTheme()` if a widget must follow a live theme
 * switch.
 */
QString wwSkinColor( const char * name );

/*! The shared selection palette for a tree or list view, as a stylesheet.
 *
 * Four colours -- selBgActive / selBgInactive / selTextActive /
 * selTextInactive -- were hardcoded identically in six files before this.
 * Views that highlight the ACTIVE member of a multi-selection (rather than
 * Qt's window-focus `:!active`) should read those four names through
 * wwSkinColor instead; a stylesheet cannot express that distinction.
 */
QString wwSelectionTreeQss();

/*! A section heading label, in the one weight the manager docks agreed on.
 *
 * There were four idioms: a plain font-weight:600 QLabel (Pose, Unfuck,
 * Rigging x3), the same plus "padding: 4px 2px" (Collision, Materials x2),
 * QFont::setBold with no sheet at all (PhysicsSimPanel, the timeline
 * inspector), and QGroupBox titles. Scrolling one Collision column passed
 * through three of them. Pose factored its version into a function whose own
 * comment said "matching the other manager docks" -- from inside an anonymous
 * namespace no other dock could reach, which is precisely how four idioms
 * happen.
 *
 * The padding variant is dropped: it indented two panels against the rest.
 */
class QLabel;
class QWidget;
QLabel * wwHeading( const QString & text, QWidget * parent = nullptr );

/*! The boxed menu-button look: transparent plate, no border until hover.
 *
 * The rationale beside its definition is explicit that bordered boxes were
 * deliberately REMOVED -- fifteen of them competing for attention. It was
 * `static`, so no dock could reach it, and res/style.qss scopes its
 * QToolButton rules to "QToolBar QToolButton", so a dock button inherits
 * nothing either. The one workaround written in its absence,
 * skelBoxedButtonQss, reinstated exactly what the rationale removed: a
 * filled plate, a visible 1px border and an amber checked state.
 *
 * `padding` is the QSS padding value, e.g. "3px 8px".
 */
QString wwBoxedButtonQss( const QString & padding );

/*! A joined, equal-height mode selector with one shared border at each seam.
 *
 * Tool-button groups mark their outer buttons with the dynamic boolean
 * properties `wwSegmentFirst` / `wwSegmentLast`. QTabBar already exposes
 * `:first` / `:last`, so it has a companion sheet using the same geometry and
 * palette. Only the two OUTER corners are rounded; adjoining edges stay square.
 */
QString wwSegmentedToolButtonQss();

/*! `rowHeight` > 0 makes the strip fill exactly that tall a row, so a tab strip
 * can be part of a shared bar row (see wwAlignBarRow). 0 keeps the compact
 * default.
 *
 * FOUR PIXELS FROM EVERY NEIGHBOUR (bungo, 2026-09-10, on a screenshot of the
 * top rows, verbatim: "just do what is on my screenshot, 4 pixels from each
 * nearby element of separation for the header / blocks / files").
 *
 * On the row path the segments no longer fill the row edge to edge: they sit
 * inside it with `wwSegmentedStripAir()` px above, below, to the left of the
 * first and between each pair, and each closes its own box. THE ROW ITSELF DOES
 * NOT MOVE -- the air is margin, the strip widget keeps the row's height, and
 * the search row beneath it stays where it was.
 *
 * `inWindow` is the window the strip lives in, and it is needed for the RIGHT
 * hand air alone: what stands between this dock and the toolbar beside it is
 * `QMainWindow::separator` (res/style.qss:66, 3 px), and only a style metric
 * asked WITH A WIDGET reports it -- asked with nullptr the same metric answers
 * the base style's 6 (measured, scratchpad/ui4_20260910/probe.cpp). The last
 * segment therefore adds only the air the separator does not already give. Pass
 * the QMainWindow; nullptr falls back to the full air on that side and is named
 * in the sheet's own comment rather than silently wrong. */
QString wwSegmentedTabBarQss( int rowHeight = 0, const QWidget * inWindow = nullptr );

/*! The air, in pixels, between a row-height segmented strip and everything
 * around it -- bungo's 4. THE WAY BACK, exact at its off value (CONSTITUTION 7):
 * `UI/SegmentedStripAir = 0` puts the segments back flush against the row's
 * edges and against one another, which is the 18:25:20 strip to the pixel.
 * ONE reader, so the sheet and the gate can never disagree about it. */
int wwSegmentedStripAir();

/*! ONE BAR HEIGHT AND ONE TOP EDGE ACROSS THE WIDTH (bungo, 2026-09-10, on a
 * screenshot: "See the issue with alignment here?").
 *
 * The window's top strip is not one bar but several, in different parents: the
 * main toolbars live in the QMainWindow's toolbar area, the viewport's own
 * toolbar is the first widget of the central column, and the left dock's mode
 * selector is the first widget of the dock. Nothing made them agree, and they
 * did not: measured 2026-09-10, the main toolbars were 35 px, the viewport
 * toolbar 33 and the dock's tab strip 26, so there was a 7 px step at the seam
 * and the dock's search row began 3 px above the viewport's content.
 *
 * `wwAlignBarRow` gives them one height: the TALLEST natural height among
 * them, so the row is set by whichever bar needs the most and nothing is ever
 * squeezed -- a squeezed QToolBar does not clip, it hides controls behind an
 * extension chevron, which is how Add and Object once vanished off the mode
 * bar. `wwBarRowHeight()` reads the number back afterwards, for a tab strip
 * that has to restyle its tabs to fill the row and for the gate that measures
 * it (WW_UIALIGN_TEST, src/uialigntest.cpp).
 *
 * Lane WATER7: it also appends the row's own button sheet to each bar, so the
 * row's BUTTONS take the row's height too. Lane UI3: and to each BUTTON, and
 * with the arithmetic calibrated rather than assumed -- see below. The
 * signature is deliberately unchanged, so every existing call site still
 * compiles and no call site types a second number. */
void wwAlignBarRow( const QList<QWidget *> & bars );
int wwBarRowHeight();

/*! ...AND THE BUTTONS IN THE ROW ARE THE ROW (bungo, 2026-09-10, on a
 * screenshot of the aligned Header | Blocks | Files strip beside the Object
 * Mode row: "compact these vertically like this, the top bar and the buttons").
 *
 * `wwAlignBarRow` above made the BARS agree with one another. WATER7 then tried
 * to make what is INSIDE them agree too and missed by four pixels: BUILD12
 * measured the row at 35 px and its buttons at 39. Lane UI3 measured why,
 * outside the application, in `scratchpad/ui3_20260910/probe.cpp` and
 * `probe2.cpp`, and the three causes are written out in full above the
 * definitions in `src/nifskope_ui.cpp`. In short:
 *
 *   - a widget's OWN stylesheet outranks every ancestor's, whatever the
 *     selectors say, so `wwBoxedButtonQss`'s padding beat the bar's;
 *   - QSS `min-height` on a QToolButton is a minimum on the CONTENTS, and Qt
 *     adds 3 px of its own on top of it, the padding and the border;
 *   - a QToolBar starts its items 4 px below its own top, so a button of
 *     exactly the row's height hangs over the bottom and is clipped.
 *
 * Hence two sheets and one measurement:
 *
 * `wwBarRowBoxQss()` takes a bar's own box away, which puts its items at y = 0
 * and makes the layout clamp them to the bar instead of letting them overflow.
 *
 * `wwBarRowButtonQss( contentHeight )` states the glyph line and the air above
 * and below it -- nothing horizontal, so each button keeps its own width -- for
 * the two kinds of child a bar in this row has, a QMenuBar's items and a
 * QToolBar's buttons. `contentHeight` is not the row: it is what is left of the
 * row once the style's own additions are taken out, and `wwAlignBarRow`
 * MEASURES that rather than typing Qt's constants, so nothing here moves with a
 * Qt version. `wwBarRowButtonContent()` and `wwBarRowButtonOverhead()` read the
 * two numbers back, for the gate and for the log.
 *
 * `contentHeight` <= 0 returns an empty string: no row, no rule.
 *
 * THE WAY BACK, exact at its off value (CONSTITUTION 7). Both sheets return an
 * empty string when the setting `UI/CompactTopBars` is false, and the call site
 * that puts the MENU BAR in the row reads the same key -- so one key, set
 * false, leaves every bar and every button exactly as BUILD9 shipped them on
 * 2026-09-10, and the gate pins that. The key is read here rather than passed
 * in, so there is one reader of it and no call site can disagree. */
QString wwBarRowButtonQss( int contentHeight );
QString wwBarRowBoxQss();
int wwBarRowButtonContent();
int wwBarRowButtonOverhead();

/*! ...AND THE TITLES IN THE MENU BAR SIT ON THE ROW'S CENTRE LINE (lane UI5,
 * 2026-09-10). bungo, verbatim: "Also, please center file / view / spells /
 * options / help buttons, top left".
 *
 * Lane UI3 put the menu BAR in the row and said in its own handoff that the
 * ITEMS were not made the row's height, only the bar. They were not: measured
 * on the shipped 20:45:47 window (its own in-application grab, read by
 * scratchpad/ui5_20260910/measure_before.py), the five titles' text sat at
 * y 6..16 in a 35 px row whose centre line is 17.0 -- six pixels high, which is
 * what his screenshot shows.
 *
 * `wwBarRowMenuItemQss( rowHeight, itemContent )` states the air above and below
 * one menu title and nothing else: `itemContent` is the item's own height with
 * no vertical padding, and the row's spare pixels are split evenly over and
 * under it. Nothing horizontal, so the 8 px `res/style.qss` gives a title on
 * either side is left alone -- the same rule `wwBarRowButtonQss` follows.
 *
 * WHY PADDING AND NOT min-height AND NOT A MARGIN, measured outside the
 * application over 30 cases (scratchpad/ui5_20260910/probe.cpp, probe_out2.txt):
 *
 *   - `QMenuBar::item { min-height }` IS NOT CONSULTED ON THIS PATH. Fifteen
 *     cases from 22 to 36 px leave the item exactly 20 px tall. The rule has
 *     been in the row's sheet since lane WATER7 and has never moved a pixel.
 *   - a `::item` MARGIN does move the item, but `actionGeometry()` grows with
 *     it, so the rect lies about where the item is drawn -- exactly as
 *     `QTabBar::tabRect()` did for lane UI4. A margin therefore cannot be gated
 *     on geometry and is not used.
 *   - the two vertical PADDINGS are consulted and leave the rect honest. The
 *     item's painted box becomes the whole row (offset 0.0 from the centre
 *     line) and the title's text lands within half a pixel of it (+0.5). No
 *     integer split does better: the ink sits one pixel below its own content's
 *     centre, so 8/11 reads -0.5 and 9/10 reads +0.5.
 *
 * `wwAlignBarRow` MEASURES `itemContent` rather than typing 16: it applies the
 * row's sheet once with both vertical paddings zeroed and reads the item's
 * height back, which needs `ensurePolished()` and no event loop (probe case E)
 * -- and that is what makes it usable, because the row is aligned during
 * construction where no events can be pumped.
 * `wwBarRowMenuItemContent()`, `wwBarRowMenuItemPadTop()` and
 * `wwBarRowMenuItemPadBottom()` read the three numbers back, for the gate and
 * for the log; the two paddings are -1 until the row has been aligned.
 *
 * THE WAY BACK is the same single key. `UI/CompactTopBars` false returns an
 * empty string here too -- one reader, `wwCompactTopBars()`, shared with both
 * other sheets -- so the menu bar keeps `res/style.qss` alone and the titles sit
 * where they did before lane UI3. A row height or a content of zero also
 * returns an empty string: no row, no rule. */
QString wwBarRowMenuItemQss( int rowHeight, int itemContent );
int wwBarRowMenuItemContent();
int wwBarRowMenuItemPadTop();
int wwBarRowMenuItemPadBottom();

/*! Is the compact top-bar row on? (`UI/CompactTopBars`, default true.)
 *
 * Exposed so the gate can say which state it measured instead of reporting a
 * red that is really a setting, and so the one call site that has to decide
 * whether the menu bar joins the row asks the same reader the sheet does. */
bool wwCompactTopBars();

/*! ...and the row UNDER the bar row starts where the viewport's content starts.
 *
 * The second half of the same complaint. Once the bars agree, a dock page whose
 * own layout carries a top margin still begins its first row a few pixels below
 * the line the viewport's content begins on -- 4 px, measured 2026-09-10 on the
 * file browser page, whose Designer layout has topMargin 4. The left and right
 * margins are the page's own business and are left alone; only the TOP is the
 * shared grid's business. */
void wwStartContentBelowBar( QWidget * page );

#endif // WWSKIN_H
