#ifndef WWSKIN_H
#define WWSKIN_H

#include <QString>


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
 * "textDisabled", "accentDisabled", "danger", "viewport", "selBgActive",
 * "selBgInactive", "selTextActive", "selTextInactive".
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
QString wwSegmentedTabBarQss();

#endif // WWSKIN_H
