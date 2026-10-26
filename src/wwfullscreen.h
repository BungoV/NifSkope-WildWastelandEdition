#ifndef WWFULLSCREEN_H
#define WWFULLSCREEN_H

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QWidget>

//! lane VIEWFULL1: full screen for a top-level window (the water window, the viewport). Hides the window's
//! menu, status bar, docks, toolbars and any extra chrome; leaving puts back the exact geometry and dock layout.
class WwFullScreen
{
public:
	explicit WwFullScreen( QWidget * window ) : win( window ) {}

	//! Widgets hidden while full screen besides a main window's own bars and docks (e.g. the viewport header).
	void setExtraChrome( const QList<QWidget *> & widgets ) { extra = widgets; }
	bool active() const { return on; }
	void enter();
	void leave();
	void toggle() { if ( on ) leave(); else enter(); }

private:
	QWidget * win;
	QList<QWidget *> extra;
	QList<QPointer<QWidget>> hidden;
	QByteArray geometry;
	QByteArray dockState;
	Qt::WindowStates before;
	bool on = false;
};

#endif
