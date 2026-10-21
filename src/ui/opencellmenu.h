/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef OPENCELLMENU_H
#define OPENCELLMENU_H

#include <QString>

#include <functional>

class QAction;
class QMenu;
class QWidget;

/*! Adds "Open Cell..." before `openBefore` and "Recent Cells" before `recentBefore` in `file`.
 *  `open` receives a `.wwcell` path and must open it the way any other file is opened. */
void openCellInstallMenu( QMenu * file, QAction * openBefore, QAction * recentBefore, QWidget * parent,
	const std::function<void( const QString & )> & open );

#endif // OPENCELLMENU_H
