/* WW_VIEWFULL_TEST: the full-screen viewport (lane VIEWFULL1).

   Gates, read off the live window:
     * F11's action enters full screen: the window is full screen, the view covers the screen, and the menu,
       status bar, every dock, every toolbar and the viewport header are hidden;
     * Esc while flying ends the flight and stays full screen; a second Esc leaves full screen;
     * after leaving, the window geometry (saveGeometry bytes, frame rect, window state) and the dock layout
       (saveState bytes, every dock's and bar's visibility and rect) equal the state before, exactly;
     * the same comparison is shown going red: one visible dock hidden must count as a difference.
   Run once with the window normal and once maximized.

   WW_VIEWFULL_TEST=1 arms it; WW_VIEWFULL_KEEP=1 leaves the window open. Log: release/ww_viewfull_test.log */

#include "nifskope.h"
#include "glview.h"
#include "wwswitch.h"

#include <QApplication>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QKeyEvent>
#include <QScreen>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>

namespace
{

struct WwFullSnap
{
	QByteArray geom, state;
	Qt::WindowStates ws;
	QRect frame;
	QStringList parts;
};

void wwFullWait( int ms )
{
	QEventLoop loop;
	QTimer::singleShot( ms, &loop, &QEventLoop::quit );
	loop.exec();
}

QString wwRectText( const QRect & r )
{
	return QStringLiteral( "%1,%2 %3x%4" ).arg( r.x() ).arg( r.y() ).arg( r.width() ).arg( r.height() );
}

WwFullSnap wwFullSnap( NifSkope * skope )
{
	WwFullSnap s;
	s.geom = skope->saveGeometry();
	s.state = skope->saveState();
	s.ws = skope->windowState();
	s.frame = skope->frameGeometry();
	auto part = [&s]( const QString & what, QWidget * w ) {
		if ( w )
			s.parts << QStringLiteral( "%1 vis=%2 %3" ).arg( what ).arg( int( w->isVisible() ) ).arg( wwRectText( w->geometry() ) );
	};
	part( QStringLiteral( "menu" ), skope->menuWidget() );
	part( QStringLiteral( "status" ), skope->findChild<QStatusBar *>( QString(), Qt::FindDirectChildrenOnly ) );
	part( QStringLiteral( "header" ), skope->findChild<QWidget *>( QStringLiteral( "ViewportHeader" ) ) );
	part( QStringLiteral( "view" ), skope->getGraphicsView() );
	for ( auto * d : skope->findChildren<QDockWidget *>( QString(), Qt::FindDirectChildrenOnly ) )
		part( QStringLiteral( "dock %1 float=%2" ).arg( d->objectName() ).arg( int( d->isFloating() ) ), d );
	for ( auto * t : skope->findChildren<QToolBar *>( QString(), Qt::FindDirectChildrenOnly ) )
		if ( skope->toolBarArea( t ) != Qt::NoToolBarArea )
			part( QStringLiteral( "bar %1" ).arg( t->objectName() ), t );
	return s;
}

//! Differences between two snapshots, each named in the log.
int wwFullDiff( const WwFullSnap & a, const WwFullSnap & b, QTextStream & log )
{
	int n = 0;
	auto say = [&]( const QString & s ) { n++; log << "    differs: " << s << "\n"; };
	if ( a.geom != b.geom )
		say( QStringLiteral( "saveGeometry bytes" ) );
	if ( a.state != b.state )
		say( QStringLiteral( "saveState bytes" ) );
	if ( a.ws != b.ws )
		say( QStringLiteral( "window state %1 -> %2" ).arg( int( a.ws ) ).arg( int( b.ws ) ) );
	if ( a.frame != b.frame )
		say( QStringLiteral( "frame %1 -> %2" ).arg( wwRectText( a.frame ), wwRectText( b.frame ) ) );
	if ( a.parts.size() != b.parts.size() )
		say( QStringLiteral( "part count %1 -> %2" ).arg( a.parts.size() ).arg( b.parts.size() ) );
	for ( int i = 0; i < std::min( a.parts.size(), b.parts.size() ); i++ )
		if ( a.parts[i] != b.parts[i] )
			say( a.parts[i] + QStringLiteral( "  ->  " ) + b.parts[i] );
	return n;
}

void wwFullEsc( GLView * ogl )
{
	QKeyEvent press( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
	QApplication::sendEvent( ogl, &press );
	QKeyEvent release( QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier );
	QApplication::sendEvent( ogl, &release );
}

} // namespace

void wwViewFullHarness( NifSkope * skope )
{
	if ( !skope || !wwEnvIsSet( "WW_VIEWFULL_TEST" ) )
		return;
	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool, QString & ) {
		QTimer::singleShot( 1500, skope, [skope]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_viewfull_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			int checks = 0, fails = 0;
			auto check = [&]( const QString & what, bool ok ) {
				checks++;
				if ( !ok )
					fails++;
				log << ( ok ? "ok   " : "FAIL " ) << what << "\n";
				log.flush();
			};
			GLView * ogl = skope->getGLView();
			QAction * aFull = skope->findChild<QAction *>( QStringLiteral( "aViewportFullScreen" ) );
			check( QStringLiteral( "the view and the Full Screen action exist" ), ogl && aFull );
			check( QStringLiteral( "the action's key is F11" ), aFull && aFull->shortcut() == QKeySequence( Qt::Key_F11 ) );
			// two actions on one key make Qt fire neither (an ambiguous shortcut)
			QStringList f11Owners;
			for ( QAction * a : skope->findChildren<QAction *>() )
				if ( a != aFull && a->shortcuts().contains( QKeySequence( Qt::Key_F11 ) ) )
					f11Owners << a->objectName();
			check( QStringLiteral( "no other action holds F11 (%1)" ).arg( f11Owners.join( ' ' ) ), f11Owners.isEmpty() );
			QAction * aFlip = skope->findChild<QAction *>( QStringLiteral( "aViewFlip" ) );
			check( QStringLiteral( "Flip View is on Numpad 9" ), aFlip
				&& aFlip->shortcut() == QKeySequence( QKeyCombination( Qt::KeypadModifier, Qt::Key_9 ) ) );
			if ( !ogl || !aFull ) {
				log << checks << " checks, " << fails << " failures\nFAIL\n";
				logf.close();
				QApplication::quit();
				return;
			}

			for ( int round = 0; round < 2; round++ ) {
				const QString tag = round == 0 ? QStringLiteral( "normal" ) : QStringLiteral( "maximized" );
				if ( round == 1 ) {
					skope->showMaximized();
					wwFullWait( 1000 );
				}
				const WwFullSnap before = wwFullSnap( skope );
				log << "round " << tag << ": " << before.parts.size() << " parts, frame " << wwRectText( before.frame ) << "\n";
				check( tag + QStringLiteral( ": at least 3 docks and bars measured" ), before.parts.size() >= 7 );

				aFull->trigger();
				wwFullWait( 1000 );
				check( tag + QStringLiteral( ": full screen entered" ), skope->isFullScreen() && skope->viewportFullScreen() );
				int shown = 0;
				QStringList shownNames;
				for ( const QString & p : wwFullSnap( skope ).parts ) {
					if ( !p.startsWith( QLatin1String( "view " ) ) && p.contains( QLatin1String( " vis=1 " ) ) ) {
						shown++;
						shownNames << p;
					}
				}
				check( tag + QStringLiteral( ": no menu, status bar, dock, toolbar or header shown (%1: %2)" )
					.arg( shown ).arg( shownNames.join( QStringLiteral( "; " ) ) ), shown == 0 );
				const QRect scr = skope->screen() ? skope->screen()->geometry() : QRect();
				QWidget * gv = skope->getGraphicsView();
				const QRect view( gv->mapToGlobal( QPoint( 0, 0 ) ), gv->size() );
				check( tag + QStringLiteral( ": the view covers the screen (view %1, screen %2)" )
					.arg( wwRectText( view ), wwRectText( scr ) ), !scr.isEmpty() && view == scr );

				// Esc goes to the flight first, then leaves full screen
				ogl->freeCamera = true;
				wwFullEsc( ogl );
				wwFullWait( 300 );
				check( tag + QStringLiteral( ": Esc in flight ends the flight and keeps full screen" ),
					!ogl->freeCamera && skope->viewportFullScreen() );
				wwFullEsc( ogl );
				wwFullWait( 1200 );
				check( tag + QStringLiteral( ": a second Esc leaves full screen" ), !skope->viewportFullScreen() && !skope->isFullScreen() );

				const WwFullSnap after = wwFullSnap( skope );
				const int d = wwFullDiff( before, after, log );
				check( tag + QStringLiteral( ": geometry and dock layout restored exactly (%1 differences)" ).arg( d ), d == 0 );

				// the comparison goes red when one visible dock is hidden
				QDockWidget * victim = nullptr;
				for ( auto * dk : skope->findChildren<QDockWidget *>( QString(), Qt::FindDirectChildrenOnly ) )
					if ( dk->isVisible() ) {
						victim = dk;
						break;
					}
				if ( victim ) {
					victim->hide();
					wwFullWait( 200 );
					log << "  red (dock " << victim->objectName() << " hidden):\n";
					const int r = wwFullDiff( before, wwFullSnap( skope ), log );
					check( tag + QStringLiteral( ": red, a hidden dock counts (%1 differences)" ).arg( r ), r > 0 );
					victim->show();
					skope->restoreState( before.state );
					wwFullWait( 300 );
				} else {
					check( tag + QStringLiteral( ": red, a visible dock to hide" ), false );
				}
			}
			log << checks << " checks, " << fails << " failures\n" << ( fails ? "FAIL" : "PASS" ) << "\n";
			logf.close();
			if ( !wwEnvIsSet( "WW_VIEWFULL_KEEP" ) )
				QApplication::quit();
		} );
	} );
}
