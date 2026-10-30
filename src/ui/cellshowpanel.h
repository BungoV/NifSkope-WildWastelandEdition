/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSHOWPANEL_H
#define CELLSHOWPANEL_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class QDoubleSpinBox;
class QLabel;
class QMenu;
class QObject;
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

//! Every box and field of a panel from wwMenuAsPanel or wwMenuAsDisplay read again from its action or field (a change
//! made while the signals were blocked).
void wwMenuPanelSync( QWidget * panel );

/* lane OPENCELLFIX2 (bungo 10-27): the Display tab. One row per feature: a tick, and a dropdown on the same row when
 * the feature has settings (quality = Low / Medium / High / Ultra); anything finer folds under the row. Rows sit in
 * three groups: Editor (what only the editor draws), Vanilla (what the game draws), Fusion Shaders (ours). The menu's
 * actions stay the one truth, as in wwMenuAsPanel. */
struct WwDisplayRow
{
	QString group;                 //!< one of wwDisplayGroups()
	QString tick;                  //!< objectName of the checkable action the tick drives; empty: the choices' off item
	QString choices;               //!< objectName of a submenu: its exclusive items fill the dropdown
	QWidget * dropdown = nullptr;  //!< or a ready WwDropdown (the Color row); it moves into the row
	int off = -1;                  //!< without a tick action: the item that means off (unticking picks it)
	QString label;                 //!< empty: the tick's own text, else the submenu's title
	bool quality = false;          //!< the dropdown is the four quality levels
	QStringList extras;            //!< more of the menu (ticks, submenus, value rows) folded under the row
	/* lane OPENCELLFIX2 (bungo 10-27 note 18): a group is one row. A child names its group row's tick (or submenu)
	 * and unfolds indented under the group row's arrow, in list order. Red twin CELLSHOW=nest: children lie flat. */
	QString parent;
	bool master = false;           //!< the group row's tick is a master: off, its children grey
	bool shared = false;           //!< a child its master does not gate (a Scene window setting): never greyed
	/* round 4 (bungo 10-27): a Vanilla row whose path the engine skips while a Fusion Shaders row is on greys then
	 * (live, its value kept). overriddenBy: that row's tick, or its submenu (on = any item but overriddenOff). */
	QString overriddenBy;
	int overriddenOff = 0;
	bool heavy = false;            //!< built-in preset Low turns this Fusion Shaders row off (its GPU cost)
};

QStringList wwDisplayGroups();   //!< Editor, Vanilla, Fusion Shaders

/*! The Display panel of `menu` laid out by `rows`, with the preset bar on top. A checkable action no row names gets a
 *  row of its own in Fusion Shaders and is counted (wwDisplayLeftovers). Red WW_CELLSHOW_RED=groups (one group).
 *  Retired by REDRETIRE1 to red twins (src/wwredtwin.h, REDTWIN=CELLSHOW=<v>, tests/cellshow's red build only):
 *  `ticks` (a row with settings has no tick), `dropdown` (the dropdowns lose the mode selector's look),
 *  `arrowframe` (an open fold arrow keeps its pressed frame), `dropright` (a dropdown on the right of its row, not
 *  first in its fold), `override` (a Vanilla row never greys for Fusion Shaders), `builtin` (every built-in is Ultra),
 *  `presetbar` (no built-ins in the Preset dropdown), `foldrows`, `inline`. */
QWidget * wwMenuAsDisplay( QMenu * menu, const QList<WwDisplayRow> & rows, QWidget * parent, const WwMenuPanelStyle & style );
int wwDisplayLeftovers( const QWidget * panel );

/* Named presets of the whole Display state: every tick and choice of the menu, every value field, and the panel's
 * own dropdowns (Color). Saved in QSettings CellEditor/DisplayPresets; applying one goes through the actions, so it
 * is live. Red twin CELLSHOW=presets: a save writes nothing. */
QVariantMap wwDisplayState( const QWidget * panel );
void wwDisplaySetState( QWidget * panel, const QVariantMap & state );
QStringList wwDisplayPresetNames();      //!< his own, sorted
QStringList wwDisplayPresetBuiltins();   //!< Low, Medium, High, Ultra: the Fusion Shaders rows only; never renamed or deleted
QString wwDisplayPresetCurrent();
bool wwDisplayPresetSave( QWidget * panel, const QString & name );   //!< a new name, or overwrite one
bool wwDisplayPresetApply( QWidget * panel, const QString & name );
bool wwDisplayPresetRename( QWidget * panel, const QString & from, const QString & to );
bool wwDisplayPresetDelete( QWidget * panel, const QString & name );
QString wwDisplayPresetNext( QWidget * panel );   //!< applies the preset after the current one; its name

/* lane OPENCELLFIX2 (bungo 10-27 notes 22, 26): one setting with two homes (a Display row and a Scene window row)
 * follows itself live. Each home watches the topic with a re-read of the setting; whoever changed it says so.
 * A watch ends with its owner. Red twin CELLSHOW=livebind: nobody is told. */
void wwLiveWatch( QObject * owner, const QString & topic, const std::function<void()> & reread );
void wwLiveChanged( const QString & topic );

#endif // CELLSHOWPANEL_H
