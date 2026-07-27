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
 * "danger".
 *
 * An unknown name returns an empty string and warns, which shows up as an
 * ignored CSS declaration rather than a wrong colour.
 *
 * Widgets built BEFORE the theme loads get the default (dark) column; rebuild
 * or restyle on `NifSkope::reloadTheme()` if a widget must follow a live theme
 * switch.
 */
QString wwSkinColor( const char * name );

#endif // WWSKIN_H
