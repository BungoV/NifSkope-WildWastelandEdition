/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSHOWPANEL_H
#define CELLSHOWPANEL_H

#include <QString>

#include <functional>

class QDoubleSpinBox;
class QLabel;
class QMenu;
class QWidget;

/* lane OPENCELLFIX1 (bungo 10-27): the Cell dock's Show switches as a tab, and a Show menu that stays open.
 *
 * "For show, don't close it when I click on one thing" / "Maybe make that a sub category with checkmarks".
 * The menu's QActions stay the one source of truth: the tab is built FROM the menu and every box drives its action
 * (trigger(), the same path a click in the menu takes), and every action drives its box back. Nothing in the tab
 * saves or applies a value of its own. */

//! A tick in `menu` or any of its submenus toggles and leaves the menu open (a release over a checkable row is
//! swallowed after triggering it; Return, Enter and Space too). Escape or a click outside still closes it.
//! WW_CELLSHOW_RED=menu: not installed (the unit test's red).
void wwKeepMenuOpenOnTick( QMenu * menu );

struct WwMenuPanelStyle
{
	std::function<QLabel *( const QString &, QWidget * )> heading;   //!< a section heading (wwHeading)
	std::function<void( QDoubleSpinBox * )> number;                  //!< style a mirrored number field
	QString foldGroup;                                               //!< QSettings group for the folds
};

/*! Builds a panel of `menu`: a heading row (a QWidgetAction showing a QLabel) starts a fold section, a submenu is a
 *  nested fold, a checkable row is a check box (a radio button in an exclusive group), and a value row (a
 *  QWidgetAction whose widget lays out label | field in a grid) is the same label | field again, bound to the
 *  original field both ways. Folds remember being closed under `style.foldGroup`.
 *  WW_CELLSHOW_RED=sync: an action that changes does not move its box (the unit test's red). */
QWidget * wwMenuAsPanel( QMenu * menu, QWidget * parent, const WwMenuPanelStyle & style );

//! Every box and field of a panel from wwMenuAsPanel read again from its action or field (a change made while the
//! signals were blocked).
void wwMenuPanelSync( QWidget * panel );

#endif // CELLSHOWPANEL_H
