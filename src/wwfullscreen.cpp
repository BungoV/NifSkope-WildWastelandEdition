#include "wwfullscreen.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QStatusBar>
#include <QToolBar>

void WwFullScreen::enter()
{
	if ( on || !win )
		return;
	before = win->windowState();
	geometry = win->saveGeometry();
	QList<QWidget *> chrome = extra;
	if ( auto * mw = qobject_cast<QMainWindow *>( win ) ) {
		dockState = mw->saveState();
		chrome << mw->menuWidget();
		// statusBar() would create one; take only an existing bar
		chrome << mw->findChild<QStatusBar *>( QString(), Qt::FindDirectChildrenOnly );
		for ( auto * d : mw->findChildren<QDockWidget *>( QString(), Qt::FindDirectChildrenOnly ) )
			chrome << d;
		for ( auto * t : mw->findChildren<QToolBar *>( QString(), Qt::FindDirectChildrenOnly ) )
			if ( mw->toolBarArea( t ) != Qt::NoToolBarArea )
				chrome << t;
	}
	hidden.clear();
	for ( QWidget * w : chrome ) {
		if ( w && !w->isHidden() ) {
			hidden << w;
			w->hide();
		}
	}
	on = true;
	win->showFullScreen();
}

void WwFullScreen::leave()
{
	if ( !on || !win )
		return;
	on = false;
	win->setWindowState( before );
	// restoreGeometry puts back the normal rect and the maximized state saved with it
	win->restoreGeometry( geometry );
	for ( const QPointer<QWidget> & w : std::as_const( hidden ) )
		if ( w )
			w->show();
	hidden.clear();
	// the docks' sizes, tabs and floating places, exactly as they were
	if ( auto * mw = qobject_cast<QMainWindow *>( win ) )
		mw->restoreState( dockState );
}
