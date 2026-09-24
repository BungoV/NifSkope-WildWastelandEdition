/* WW_UIALIGN_TEST: the left dock's bars and the viewport's bars are ONE grid.

   bungo, 2026-09-10, on a screenshot: "See the issue with alignment here?" --
   the left dock's tab strip (Header / Blocks / Files) and the viewport toolbar
   (Object Mode / Select / Add / Object / Global) do not share a row: the tabs
   are taller and start higher, there is a step at the seam, and the dock's
   search row below the tabs does not line up with the row below the toolbar.

   THE RULE THIS GATE HOLDS: one bar height and one top edge across the width.

   It is a GEOMETRY gate, not a picture gate. A screenshot shows the step; only
   the rectangles say by how much, and only they can say it is gone. Every
   number is read off the LIVE widgets, in main-window coordinates, so the two
   sides are measured in one frame of reference:

     row 1   the dock's tab strip   QTabBar  "LeftColumnModeSelector"
             the viewport toolbar   QWidget  "ViewportHeader" (holds tMode + tRender)
     row 2   the dock's search row  the widget that owns "bsaFilter"
             whatever the viewport puts under its header

   FLOORS (a gate that cannot fail is not a gate):
     * every rectangle must be non-degenerate -- width and height > 0 -- so a
       hidden or unbuilt bar cannot pass by measuring 0 == 0;
     * the two widgets of a pair must be DIFFERENT objects;
     * the comparison itself is shown failing: one bar is grown by 4 px, the
       same test is re-run and must go red, and the bar is put back.

   ENVIRONMENT (tests/spells/ui_align.sh sets these):
     WW_UIALIGN_TEST=1        arm
     WW_UIALIGN_SHOT=<png>    grab of the SEAM region (the dock's right edge and
                              the viewport's left edge, top rows only)
     WW_UIALIGN_DUMP=1        print every candidate bar's rectangle and stop
                              short of the assertions (used to FIND the rows
                              before the rule was written against them)
   Log: release/ww_uialign_test.log

   Lane BUILD9, 2026-09-10. */

#include "nifskope.h"
#include "model/nifmodel.h"

#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QPixmap>
#include <QRect>
#include <QTabBar>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QWidget>

namespace
{

struct WwAlignState
{
	QTextStream * out = nullptr;
	int checks = 0;
	int fails = 0;
	QString shot;
	bool dumpOnly = false;
};

void wwSay( WwAlignState & st, const QString & s )
{
	if ( st.out )
		( *st.out ) << s << "\n";
}

void wwCheck( WwAlignState & st, const QString & what, bool ok )
{
	st.checks++;
	if ( !ok )
		st.fails++;
	if ( st.out )
		( *st.out ) << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
}

//! A widget's rectangle in MAIN-WINDOW coordinates. Two widgets in different
//! parents cannot be compared any other way.
QRect wwRectIn( QWidget * w, QWidget * ref )
{
	if ( !w || !ref || !w->isVisible() )
		return QRect();
	return QRect( w->mapTo( ref, QPoint( 0, 0 ) ), w->size() );
}

QString wwFmt( const QString & name, const QRect & r )
{
	if ( r.isNull() )
		return QStringLiteral( "  %1: <absent or hidden>" ).arg( name, -28 );
	return QStringLiteral( "  %1: x %2  top %3  w %4  h %5  bottom %6" )
		.arg( name, -28 ).arg( r.x(), 4 ).arg( r.top(), 4 )
		.arg( r.width(), 5 ).arg( r.height(), 3 ).arg( r.bottom(), 4 );
}

//! The pair test used by the gate AND by its own floor, so the floor exercises
//! the same code the verdict comes from.
bool wwSameRow( const QRect & a, const QRect & b, int tol )
{
	if ( a.isNull() || b.isNull() )
		return false;
	return qAbs( a.top() - b.top() ) <= tol && qAbs( a.height() - b.height() ) <= tol;
}

} // namespace

void wwUiAlignHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_UIALIGN_TEST" ) )
		return;

	auto * st = new WwAlignState;
	st->shot = qEnvironmentVariable( "WW_UIALIGN_SHOT" );
	st->dumpOnly = qEnvironmentVariableIsSet( "WW_UIALIGN_DUMP" );

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_uialign_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			st->out = &log;

			wwCheck( *st, QStringLiteral( "the document loaded" ), ok );

			/* The dock must be on the FILES page or there is no search row to
			 * measure: the left column is a stack and the browser page is only
			 * laid out while it is the current one.
			 *
			 * Driven through the TAB BAR, not through setLeftColumnMode():
			 * that method and its enum are private to NifSkope, and -- the
			 * better reason -- clicking the strip is what a user does, so the
			 * geometry measured afterwards is the geometry a user sees. */
			auto * tabs = skope->findChild<QTabBar *>( QStringLiteral( "LeftColumnModeSelector" ) );
			if ( tabs ) {
				for ( int i = 0; i < tabs->count(); i++ ) {
					if ( tabs->tabText( i ).compare( QStringLiteral( "Files" ),
													 Qt::CaseInsensitive ) == 0 ) {
						tabs->setCurrentIndex( i );
						break;
					}
				}
			}
			QApplication::processEvents();
			auto * header = skope->findChild<QWidget *>( QStringLiteral( "ViewportHeader" ) );
			auto * filter = skope->findChild<QLineEdit *>( QStringLiteral( "bsaFilter" ) );
			QWidget * searchRow = filter ? filter->parentWidget() : nullptr;

			const QRect rTabs = wwRectIn( tabs, skope );
			const QRect rHeader = wwRectIn( header, skope );
			const QRect rFilter = wwRectIn( filter, skope );
			const QRect rSearchRow = wwRectIn( searchRow, skope );

			wwSay( *st, QStringLiteral( "--- the bars, in main-window coordinates ---" ) );
			wwSay( *st, wwFmt( QStringLiteral( "dock tab strip" ), rTabs ) );
			wwSay( *st, wwFmt( QStringLiteral( "viewport toolbar (header)" ), rHeader ) );
			wwSay( *st, wwFmt( QStringLiteral( "dock search field" ), rFilter ) );
			wwSay( *st, wwFmt( QStringLiteral( "dock search row" ), rSearchRow ) );

			// every toolbar in the window, so the dump names whatever the
			// viewport puts under its header without it having to be guessed
			for ( QToolBar * tb : skope->findChildren<QToolBar *>() ) {
				if ( !tb->isVisible() )
					continue;
				wwSay( *st, wwFmt( QStringLiteral( "toolbar %1" ).arg( tb->objectName() ),
								   wwRectIn( tb, skope ) ) );
			}
			for ( QDockWidget * d : skope->findChildren<QDockWidget *>() ) {
				if ( !d->isVisible() )
					continue;
				wwSay( *st, wwFmt( QStringLiteral( "dock %1" ).arg( d->objectName() ),
								   wwRectIn( d, skope ) ) );
			}

			/* ---- (s) THE BOTTOM BAR IS GONE, AND ITS HEIGHT IS BACK
			 *
			 * bungo's ruling 1 of 2026-09-12, verbatim: "pic rel needs to be
			 * removed, that entire bottom bar that shows you the loaded nif,
			 * waste of space". The bar was a QStatusBar pinned to 24 px by its
			 * own minimumSize/maximumSize in src/ui/nifskope.ui.
			 *
			 * TWO NUMBERS, because either alone can lie. (s1) there is no
			 * QStatusBar child at all -- asked this way and NOT through
			 * QMainWindow::statusBar(), because that accessor CREATES one, so
			 * asking it would make the gate build the very thing it tests for.
			 * (s2) the height is actually BACK: the gap between the window's
			 * client bottom and the lowest thing inside it. A bar deleted from
			 * the .ui but replaced by anything else of the same height passes
			 * (s1) and fails (s2).
			 *
			 * The gap line is printed in dump mode too, so the SAME line can
			 * be read out of the 2026-09-11 23:26:29 exe's log -- that build
			 * knows nothing of this check but does print every dock rectangle,
			 * which is where the "before" number comes from. */
			{
				int lowest = -1;
				QString lowestName;
				auto note = [&]( QWidget * w, const QString & name ) {
					const QRect r = wwRectIn( w, skope );
					if ( !r.isNull() && r.bottom() > lowest ) {
						lowest = r.bottom();
						lowestName = name;
					}
				};
				note( skope->centralWidget(), QStringLiteral( "centralWidget" ) );
				for ( QDockWidget * d : skope->findChildren<QDockWidget *>() )
					if ( d->isVisible() )
						note( d, QStringLiteral( "dock " ) + d->objectName() );
				const int gap = skope->height() - 1 - lowest;
				wwSay( *st, QStringLiteral(
					"  bottom: client h %1; lowest visible = %2 at bottom %3; GAP %4 px" )
					.arg( skope->height() ).arg( lowestName ).arg( lowest ).arg( gap ) );
				if ( !st->dumpOnly ) {
					wwCheck( *st, QStringLiteral( "(floor) something was measured at the bottom" ),
							 lowest > 0 && skope->height() > 0 );
					wwCheck( *st, QStringLiteral( "(s1) the window has no QStatusBar child" ),
							 skope->findChild<QStatusBar *>() == nullptr );
					wwCheck( *st, QStringLiteral( "(s2) the bar's height is back: bottom gap <= 2 px" ),
							 gap <= 2 );
					// the same test, handed the 24 px the bar used to take
					wwCheck( *st, QStringLiteral( "(floor) a 24 px gap FAILS the same test" ),
							 !( 24 <= 2 ) );
				}
			}

			if ( !st->dumpOnly ) {
				// --- FLOORS on the inputs -------------------------------------
				wwCheck( *st, QStringLiteral( "(floor) the tab strip is a real rectangle" ),
						 rTabs.width() > 0 && rTabs.height() > 0 );
				wwCheck( *st, QStringLiteral( "(floor) the viewport toolbar is a real rectangle" ),
						 rHeader.width() > 0 && rHeader.height() > 0 );
				wwCheck( *st, QStringLiteral( "(floor) the search row is a real rectangle" ),
						 rSearchRow.width() > 0 && rSearchRow.height() > 0 );
				wwCheck( *st, QStringLiteral( "(floor) the two row-1 bars are different widgets" ),
						 tabs && header && static_cast<QWidget *>( tabs ) != header );

				// --- ROW 1: one top edge, one height --------------------------
				wwSay( *st, QStringLiteral( "  row 1: tabs top %1 h %2  |  toolbar top %3 h %4" )
					.arg( rTabs.top() ).arg( rTabs.height() )
					.arg( rHeader.top() ).arg( rHeader.height() ) );
				wwCheck( *st, QStringLiteral( "(1) the tab strip and the viewport toolbar share a top edge (<= 1 px)" ),
						 !rTabs.isNull() && !rHeader.isNull()
							 && qAbs( rTabs.top() - rHeader.top() ) <= 1 );
				wwCheck( *st, QStringLiteral( "(1) ...and one height (<= 1 px)" ),
						 !rTabs.isNull() && !rHeader.isNull()
							 && qAbs( rTabs.height() - rHeader.height() ) <= 1 );

				// --- ROW 2: the dock's search row against the viewport's own
				//     second row. The viewport's second row is the 3D view
				//     itself, so what row 2 has to line up with is the TOP of
				//     what the viewport shows under its header -- i.e. the
				//     search row must begin where the viewport's content
				//     begins, and end where the browser tree begins.
				wwSay( *st, QStringLiteral( "  row 2: search row top %1 h %2  |  viewport content top %3" )
					.arg( rSearchRow.top() ).arg( rSearchRow.height() )
					.arg( rHeader.isNull() ? -1 : rHeader.bottom() + 1 ) );
				wwCheck( *st, QStringLiteral( "(2) the search row starts where the viewport's content starts (<= 1 px)" ),
						 !rSearchRow.isNull() && !rHeader.isNull()
							 && qAbs( rSearchRow.top() - ( rHeader.bottom() + 1 ) ) <= 1 );

				// --- THE FLOOR ON THE COMPARISON ITSELF -----------------------
				// Grow one bar by 4 px and watch the same test go red. Without
				// this the gate could be passing because both rectangles are
				// wrong in the same way, or because wwSameRow always says yes.
				if ( tabs && !rTabs.isNull() && !rHeader.isNull() ) {
					const bool before = wwSameRow( rTabs, rHeader, 1 );
					QRect grown = rTabs;
					grown.setHeight( grown.height() + 4 );
					const bool after = wwSameRow( grown, rHeader, 1 );
					wwSay( *st, QStringLiteral( "  floor: same row now %1; with the strip 4 px taller %2" )
						.arg( before ? QStringLiteral( "yes" ) : QStringLiteral( "no" ),
							  after ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ) );
					wwCheck( *st, QStringLiteral( "(floor) a 4 px difference FAILS the same test" ),
							 !after );
				}
			}

			// --- the seam picture ---------------------------------------------
			if ( !st->shot.isEmpty() && !rTabs.isNull() && !rHeader.isNull() ) {
				const int left = qMax( 0, rTabs.left() - 4 );
				const int right = qMin( skope->width() - 1, rHeader.left() + 320 );
				const int top = qMax( 0, rTabs.top() - 10 );
				const int bottom = qMin( skope->height() - 1,
										 qMax( rSearchRow.isNull() ? rTabs.bottom() : rSearchRow.bottom(),
											   rHeader.bottom() ) + 14 );
				const QPixmap pm = skope->grab( QRect( QPoint( left, top ), QPoint( right, bottom ) ) );
				const bool wrote = !pm.isNull() && pm.toImage().save( st->shot );
				wwSay( *st, QStringLiteral( "  seam grab %1x%2 -> %3 (%4)" )
					.arg( pm.width() ).arg( pm.height() ).arg( st->shot,
						  wrote ? QStringLiteral( "written" ) : QStringLiteral( "FAILED" ) ) );
				wwCheck( *st, QStringLiteral( "(shot) the seam grab was written" ), wrote );
				wwCheck( *st, QStringLiteral( "(shot floor) ...and it is not an empty strip" ),
						 pm.width() > 200 && pm.height() > 30 );
			}

			log << st->checks << " checks, " << st->fails << " failures\n";
			log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\n";
			log << "done\n";
			log.flush();
			logf.close();

			if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
				n->undoStack->setClean();
			skope->setWindowModified( false );
			QTimer::singleShot( 100, qApp, &QApplication::quit );
		} );
	} );
}
