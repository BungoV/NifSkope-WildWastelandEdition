/* WW_WATERUI_TEST -- lane WATER7 / UI2, 2026-09-10.

   TWO RULINGS OF bungo's, ONE GATE, because both are about the same strip.

   (A) THE WATER TOOL IS A TAB OF THE LOD GENERATION WORKSPACE.
       On a screenshot of the Workspaces menu: "Two issues with water window
       and water marking appearing here", corrected the same minute to "They
       should be in the LOD gen workspace"; then, over the Header | Blocks |
       Files strip, "You'd access them like this". So: a fourth tab in that
       strip, shown while the LOD Generation workspace is open; no water dock;
       no entry for either half of the tool in the Workspaces menu.

   (B) EVERY BAR AT THE TOP OF THE WINDOW IS ONE COMPACT HEIGHT.
       On a screenshot of the aligned strip beside the Object Mode row:
       "compact these vertically like this, the top bar and the buttons". Lane
       BUILD9 gave the main toolbars, the viewport header and the dock strip
       one height (35 px, measured). It did not include the MENU BAR, and it
       did not touch the buttons inside any of them.

   WHY A GEOMETRY GATE AND NOT A PICTURE. A picture shows a step; only the
   rectangles say by how many pixels, and only they can say it is gone. Every
   number is read off the LIVE widgets in MAIN-WINDOW coordinates, because two
   widgets in different parents cannot be compared any other way. The pictures
   are taken as well, and they are for bungo, not for the verdict.

   WHAT IT READS, and it reads WIDGETS by object name, never private members:
     LeftColumnModeSelector   QTabBar        the segmented strip
     LeftColumnStack          QStackedWidget its pages
     WaterMarkPanel           QWidget        the water page
     LodGenerationDock        QDockWidget    the workspace that scopes the tab
     ViewWorkspacesMenu       QMenu          the menu the two entries left
     menubar / tFile / tLOD / tView / ViewportHeader   the bars of row B

   FLOORS (a check that cannot fail is not a check):
     * every rectangle non-degenerate, printed one per line;
     * a search for a tab named "Wagter" must find NOTHING, so T1's search is
       seen to be a real search;
     * T5 asserts BOTH halves of the scoping in one run -- a tab that is always
       visible and a tab that is never visible both go red;
     * T7's scan of the Workspaces menu must still FIND "LOD Generation", or a
       scan that finds nothing would pass for the wrong reason;
     * R2's comparison is shown going red with the menu bar 4 px taller (the
       arithmetic is grown, never the widget, so nothing is left disturbed);
     * R3 prints the number of bar buttons it found and refuses below 4.

   (C) FOUR PIXELS FROM EVERY NEIGHBOUR (lane UI4, 2026-09-10 20:1x).
       On a screenshot of the top rows, verbatim: "just do what is on my
       screenshot, 4 pixels from each nearby element of separation for the
       header / blocks / files". Group S measures the five distances -- the
       row's top edge, the row's bottom edge, the window's content edge, the
       toolbar beside it, and each pair of segments -- and asserts the ROW did
       not move while the segments got shorter inside it.
       It measures PIXELS, not rects: QSS margins on QTabBar::tab are honoured
       but tabRect() returns the rect INCLUDING the margin, so every rect the
       strip can be asked for reads the same before and after the change.

   ENVIRONMENT (tests/spells/water_ui.sh sets these):
     WW_WATERUI_TEST=1        arm
     WW_WATERUI_SHOT=<png>    grab of the whole top strip
     WW_WATERUI_TABSHOT=<png> grab of the left dock with the Water tab open
     WW_WATERUI_STRIPSHOT=<png>  4x crop of the segmented strip and its
                                 neighbours, for the air
   Log: release/ww_waterui_test.log
*/

#include "nifskope.h"
#include "model/nifmodel.h"
#include "wwskin.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QRect>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWidget>

namespace
{

struct WwWaterUiState
{
	QTextStream * out = nullptr;
	int checks = 0;
	int fails = 0;
	int skips = 0;
	QString shot;
	QString tabShot;
	QString stripShot;
};

void say( WwWaterUiState & st, const QString & s )
{
	if ( st.out )
		( *st.out ) << s << "\n";
}

void check( WwWaterUiState & st, const QString & what, bool ok )
{
	st.checks++;
	if ( !ok )
		st.fails++;
	if ( st.out )
		( *st.out ) << ( ok ? "  ok   " : "  FAIL " ) << what << "\n";
}

void skip( WwWaterUiState & st, const QString & why )
{
	st.skips++;
	if ( st.out )
		( *st.out ) << "  SKIP " << why << "\n";
}

/*! THE PAINTED BOX of one segment of the strip, in the tab bar's own
 *  coordinates -- or a null rect if no pixel of `want` is on it.
 *
 *  WHY PIXELS AND NOT QTabBar::tabRect(). Lane UI4 measured it outside the
 *  application (scratchpad/ui4_20260910/probe.cpp): QSS margins on
 *  QTabBar::tab ARE honoured, and tabRect() RETURNS THE RECT INCLUDING THE
 *  MARGIN. Every rect the strip can be asked for is therefore identical before
 *  and after this lane's change, and a gate built on them would be green on
 *  both. The air only exists in what is drawn, so what is drawn is what is
 *  measured: the segment is selected (its fill and its border are both
 *  `selBgActive`, the one colour nothing else in the strip carries) and the
 *  bounding box of that colour IS the box the user sees.
 *
 *  The caller passes a colour the strip does NOT carry as its own floor, so a
 *  scan that has stopped finding anything cannot pass for a strip with no air.
 */
QRect paintedSegment( QTabBar * tabs, int index, const QColor & want, qreal * dprOut )
{
	if ( !tabs || index < 0 || index >= tabs->count() )
		return QRect();
	tabs->setCurrentIndex( index );
	for ( int i = 0; i < 3; i++ )
		QApplication::processEvents();
	const QPixmap pm = tabs->grab();
	if ( pm.isNull() )
		return QRect();
	const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
	if ( dprOut )
		*dprOut = dpr;
	const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
	const QRgb rgb = want.rgb() & 0x00ffffff;
	int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			if ( ( img.pixel( x, y ) & 0x00ffffff ) != rgb )
				continue;
			x0 = qMin( x0, x ); x1 = qMax( x1, x );
			y0 = qMin( y0, y ); y1 = qMax( y1, y );
		}
	}
	if ( x1 < 0 )
		return QRect();
	// back into the widget's own logical pixels, whatever the display's ratio
	return QRect( QPoint( qRound( x0 / dpr ), qRound( y0 / dpr ) ),
				  QPoint( qRound( ( x1 + 1 ) / dpr ) - 1, qRound( ( y1 + 1 ) / dpr ) - 1 ) );
}

//! A widget's rectangle in MAIN-WINDOW coordinates.
QRect rectIn( QWidget * w, QWidget * ref )
{
	if ( !w || !ref || !w->isVisible() )
		return QRect();
	return QRect( w->mapTo( ref, QPoint( 0, 0 ) ), w->size() );
}

QString fmt( const QString & name, const QRect & r )
{
	if ( r.isNull() )
		return QStringLiteral( "  %1: <absent or hidden>" ).arg( name, -28 );
	return QStringLiteral( "  %1: x %2  top %3  w %4  h %5  bottom %6" )
		.arg( name, -28 ).arg( r.x(), 4 ).arg( r.top(), 4 )
		.arg( r.width(), 5 ).arg( r.height(), 3 ).arg( r.bottom(), 4 );
}

//! The tab whose text is exactly `text`, or -1. Used by T1 and by T1's own floor.
int tabNamed( QTabBar * tabs, const QString & text )
{
	if ( !tabs )
		return -1;
	for ( int i = 0; i < tabs->count(); i++ )
		if ( tabs->tabText( i ) == text )
			return i;
	return -1;
}

//! How many actions of this menu carry exactly this text. T7 and its floor.
int menuEntries( QMenu * menu, const QString & text )
{
	if ( !menu )
		return -1;
	int n = 0;
	for ( QAction * a : menu->actions() )
		if ( a->text().remove( QLatin1Char( '&' ) ) == text )
			n++;
	return n;
}

/*! Does this pair share a row? The same predicate the verdict and the floor
 *  both go through, so the floor exercises the code the verdict comes from. */
bool sameHeight( const QRect & a, const QRect & b, int tol )
{
	if ( a.isNull() || b.isNull() )
		return false;
	return qAbs( a.height() - b.height() ) <= tol;
}

} // namespace

void wwWaterUiHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_WATERUI_TEST" ) )
		return;

	auto * st = new WwWaterUiState;
	st->shot = qEnvironmentVariable( "WW_WATERUI_SHOT" );
	st->tabShot = qEnvironmentVariable( "WW_WATERUI_TABSHOT" );
	st->stripShot = qEnvironmentVariable( "WW_WATERUI_STRIPSHOT" );

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope, st]( bool ok, QString & ) {
		/* 1.5 s: the scene is BUILT on the load signal, but the first paint --
		 * and so the first layout pass and every geometry a gate reads -- is
		 * not. wwAlignBarRow itself runs after restoreState for the same
		 * reason, and this has to read what it left. */
		QTimer::singleShot( 1500, skope, [skope, st, ok]() {
			QFile logf( QApplication::applicationDirPath() + "/ww_waterui_test.log" );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			st->out = &log;

			check( *st, QStringLiteral( "the document loaded" ), ok );

			auto * tabs = skope->findChild<QTabBar *>( QStringLiteral( "LeftColumnModeSelector" ) );
			auto * stack = skope->findChild<QStackedWidget *>( QStringLiteral( "LeftColumnStack" ) );
			auto * lodDock = skope->findChild<QDockWidget *>( QStringLiteral( "LodGenerationDock" ) );
			auto * leftDock = skope->findChild<QDockWidget *>( QStringLiteral( "LeftColumnDock" ) );
			auto * wsMenu = skope->findChild<QMenu *>( QStringLiteral( "ViewWorkspacesMenu" ) );

			// =============================================================
			//  R -- the bar row (bungo's "compact these vertically")
			//  Measured FIRST, before group T opens a workspace and moves
			//  the left column off the page whose search row R5 reads.
			// =============================================================
			say( *st, QStringLiteral( "--- R: the bars, in main-window coordinates ---" ) );
			if ( tabs )
				for ( int i = 0; i < tabs->count(); i++ )
					if ( tabs->tabText( i ).compare( QStringLiteral( "Files" ),
													 Qt::CaseInsensitive ) == 0 ) {
						tabs->setCurrentIndex( i );
						break;
					}
			QApplication::processEvents();

			auto * menubar = skope->findChild<QMenuBar *>( QStringLiteral( "menubar" ) );
			auto * header = skope->findChild<QWidget *>( QStringLiteral( "ViewportHeader" ) );
			auto * tFile = skope->findChild<QToolBar *>( QStringLiteral( "tFile" ) );
			auto * tLOD = skope->findChild<QToolBar *>( QStringLiteral( "tLOD" ) );
			auto * tView = skope->findChild<QToolBar *>( QStringLiteral( "tView" ) );
			auto * filter = skope->findChild<QLineEdit *>( QStringLiteral( "bsaFilter" ) );
			QWidget * searchRow = filter ? filter->parentWidget() : nullptr;

			const int row = wwBarRowHeight();
			const QRect rMenu = rectIn( menubar, skope );
			const QRect rTabs = rectIn( tabs, skope );
			const QRect rHeader = rectIn( header, skope );
			const QRect rFile = rectIn( tFile, skope );
			const QRect rLOD = rectIn( tLOD, skope );
			const QRect rView = rectIn( tView, skope );
			const QRect rSearch = rectIn( searchRow, skope );

			/* WHICH STATE THIS RUN MEASURED. `UI/CompactTopBars` is the way back
			 * (CONSTITUTION 7) and it defaults to true; if a profile has it
			 * off, R1..R4 are measuring BUILD9's row and not this lane's, so
			 * they are SKIPPED BY NAME rather than reported as reds that are
			 * really a setting. A SKIP is never a pass, and the spell prints
			 * every SKIP line after the log. */
			const bool compact = wwCompactTopBars();
			say( *st, QStringLiteral( "  UI/CompactTopBars = %1" )
				.arg( compact ? QStringLiteral( "true" ) : QStringLiteral( "FALSE" ) ) );
			say( *st, QStringLiteral( "  wwBarRowHeight() = %1" ).arg( row ) );
			say( *st, fmt( QStringLiteral( "menu bar" ), rMenu ) );
			say( *st, fmt( QStringLiteral( "tFile (workspaces row)" ), rFile ) );
			say( *st, fmt( QStringLiteral( "tLOD" ), rLOD ) );
			say( *st, fmt( QStringLiteral( "tView (anim/collision)" ), rView ) );
			say( *st, fmt( QStringLiteral( "viewport header" ), rHeader ) );
			say( *st, fmt( QStringLiteral( "dock tab strip" ), rTabs ) );
			say( *st, fmt( QStringLiteral( "dock search row" ), rSearch ) );

			check( *st, QStringLiteral( "(R floor) the row height is a real number (%1 > 0)" )
				.arg( row ), row > 0 );
			if ( !compact )
				skip( *st, QStringLiteral( "R1..R4 measure the compact bar row; this profile has "
					"UI/CompactTopBars OFF, so the top of the window is BUILD9's row and these "
					"gates do not describe it" ) );
			// R1: every bar in the row reports the row's height
			if ( compact ) {
				struct Bar { const char * name; QRect r; };
				const Bar bars[] = {
					{ "menu bar", rMenu }, { "tFile", rFile }, { "tLOD", rLOD },
					{ "tView", rView }, { "viewport header", rHeader },
					{ "dock tab strip", rTabs }
				};
				int real = 0, agree = 0;
				QString offenders;
				for ( const Bar & b : bars ) {
					if ( b.r.isNull() || b.r.width() <= 0 || b.r.height() <= 0 )
						continue;
					real++;
					if ( qAbs( b.r.height() - row ) <= 1 )
						agree++;
					else
						offenders += QStringLiteral( " %1(%2)" )
							.arg( QLatin1String( b.name ) ).arg( b.r.height() );
				}
				say( *st, QStringLiteral( "  R1: %1 visible bars, %2 at the row height%3" )
					.arg( real ).arg( agree )
					.arg( offenders.isEmpty() ? QString() : QStringLiteral( "; off:" ) + offenders ) );
				check( *st, QStringLiteral( "(R floor) at least 4 of the row's bars are real "
					"rectangles (found %1)" ).arg( real ), real >= 4 );
				check( *st, QStringLiteral( "(R1) every visible bar at the top of the window is "
					"the row height within 1 px%1" ).arg( offenders ),
					real >= 4 && agree == real );
			}

			// R2: the menu bar and the dock strip, and the floor on the comparison
			if ( compact )
				check( *st, QStringLiteral( "(R2) the menu bar and the dock tab strip are the same "
					"height (%1 vs %2)" ).arg( rMenu.height() ).arg( rTabs.height() ),
					sameHeight( rMenu, rTabs, 1 ) );
			if ( compact && !rMenu.isNull() && !rTabs.isNull() ) {
				QRect grown = rMenu;
				grown.setHeight( grown.height() + 4 );
				say( *st, QStringLiteral( "  R2 floor: same height now %1; with the menu bar 4 px "
					"taller %2" )
					.arg( sameHeight( rMenu, rTabs, 1 ) ? QStringLiteral( "yes" )
														: QStringLiteral( "no" ),
						  sameHeight( grown, rTabs, 1 ) ? QStringLiteral( "yes" )
														: QStringLiteral( "no" ) ) );
				check( *st, QStringLiteral( "(R2 floor) a 4 px difference FAILS the same test" ),
					!sameHeight( grown, rTabs, 1 ) );
			}

			/* R3: THE BUTTONS IN THOSE BARS *ARE* THE ROW.
			 *
			 * bungo, 2026-09-10: "compact these vertically like this, the top
			 * bar and the buttons" -- the aligned strip's row is the height for
			 * the menu row, the Workspaces / LOD / Animation / Collision row AND
			 * EVERY BUTTON IN THEM. BUILD12 shipped a 35 px row carrying 39 px
			 * buttons and this gate passed it, because it allowed 8 px where the
			 * spell's own header promised 1. It is 1 px here, and the row's own
			 * number is read back from the skin -- never typed.
			 *
			 * Every button is named and printed, not just the extremes: a count
			 * cannot say WHICH bar disagrees, and "print the table, not the
			 * count" is what made BUILD12's red readable in one line. */
			if ( compact ) {
				QList<QToolButton *> btns;
				QStringList btnNames;
				for ( QToolBar * tb : { tFile, tLOD, tView } ) {
					if ( !tb )
						continue;
					for ( QToolButton * b : tb->findChildren<QToolButton *>() ) {
						if ( !b->isVisible() )
							continue;
						// Qt's own extension chevron is not one of our buttons
						if ( b->objectName() == QLatin1String( "qt_toolbar_ext_button" ) )
							continue;
						btns.append( b );
						btnNames.append( QStringLiteral( "%1/%2" ).arg( tb->objectName(),
							b->objectName().isEmpty() ? b->text() : b->objectName() ) );
					}
				}

				/* ONE predicate for the verdict, the table and the floor, so the
				 * floor exercises the code the verdict comes from. */
				auto worstOff = [&]( int & minH, int & maxH, int & agree ) {
					minH = 1 << 20; maxH = 0; agree = 0;
					int worst = 0;
					for ( QToolButton * b : btns ) {
						const int d = b->height() - row;
						minH = qMin( minH, b->height() );
						maxH = qMax( maxH, b->height() );
						if ( qAbs( d ) <= 1 )
							agree++;
						else if ( qAbs( d ) > qAbs( worst ) )
							worst = d;
					}
					if ( btns.isEmpty() )
						minH = 0;
					return worst;
				};

				int minH = 0, maxH = 0, agree = 0;
				const int worst = worstOff( minH, maxH, agree );
				for ( int i = 0; i < btns.size(); i++ )
					say( *st, QStringLiteral( "  R3 button %1: %2 px, y %3 (row %4)" )
						.arg( btnNames.at( i ) ).arg( btns.at( i )->height() )
						.arg( btns.at( i )->y() ).arg( row ) );
				say( *st, QStringLiteral( "  R3: %1 bar buttons, %2 at the row height, "
					"heights %3..%4, row %5, worst offset %6" )
					.arg( btns.size() ).arg( agree ).arg( minH ).arg( maxH ).arg( row ).arg( worst ) );
				say( *st, QStringLiteral( "  R3: the skin calibrated content %1 with a measured "
					"style overhead of %2" )
					.arg( wwBarRowButtonContent() ).arg( wwBarRowButtonOverhead() ) );

				check( *st, QStringLiteral( "(R3 floor) the row carries at least 4 buttons to "
					"measure (found %1)" ).arg( btns.size() ), btns.size() >= 4 );
				check( *st, QStringLiteral( "(R3) every button in the row is the row's own height "
					"within 1 px (%1 of %2, heights %3..%4 against %5)" )
					.arg( agree ).arg( btns.size() ).arg( minH ).arg( maxH ).arg( row ),
					btns.size() >= 4 && agree == btns.size() );
				check( *st, QStringLiteral( "(R3) ...and they agree with each other within 1 px "
					"(%1..%2)" ).arg( minH ).arg( maxH ),
					btns.size() >= 4 && maxH - minH <= 1 );
				check( *st, QStringLiteral( "(R3) ...and the calibration measured a real overhead "
					"(%1) instead of falling back" ).arg( wwBarRowButtonOverhead() ),
					wwBarRowButtonOverhead() >= 0 && wwBarRowButtonContent() > 0 );

				/* THE FLOOR THAT FIRES, both halves in one run (the T5 pattern).
				 *
				 * A gate that has only ever been seen green proves nothing, and
				 * this one was green on a 4 px error for a whole build. So the
				 * DELIBERATE WRONG NUMBER is applied here, live: BUILD12's own
				 * arithmetic -- min-height rowHeight - 4, padding (rowHeight -
				 * 18) / 2 -- appended over the shipped sheet, which is exactly
				 * the state that shipped at 17:45:29. The same predicate must go
				 * RED on it, and green again once it is taken away. */
				const int sabMin = qMax( 12, row - 4 );
				const int sabPad = qMax( 2, ( row - 18 ) / 2 );
				const QString sabotage = QStringLiteral(
					"QMenuBar::item { min-height: %1px; padding: %2px 8px; }"
					"QToolButton { min-height: %1px; padding: %2px 4px; }" )
					.arg( sabMin ).arg( sabPad );
				QStringList sheetsBefore;
				QList<QWidget *> victims;
				for ( QToolBar * tb : { tFile, tLOD, tView } )
					if ( tb )
						victims.append( tb );
				for ( QToolButton * b : btns )
					victims.append( b );
				for ( QWidget * w : victims )
					sheetsBefore.append( w->styleSheet() );

				for ( int i = 0; i < victims.size(); i++ )
					victims.at( i )->setStyleSheet( sheetsBefore.at( i ) + sabotage );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int sMin = 0, sMax = 0, sAgree = 0;
				const int sWorst = worstOff( sMin, sMax, sAgree );
				say( *st, QStringLiteral( "  R3 floor: with BUILD12's own arithmetic put back "
					"(min-height %1, padding %2) the buttons read %3..%4 against a row of %5, "
					"worst offset %6" )
					.arg( sabMin ).arg( sabPad ).arg( sMin ).arg( sMax ).arg( row ).arg( sWorst ) );
				check( *st, QStringLiteral( "(R3 floor) the SAME test goes red on the sheet that "
					"shipped at 39 px (%1 of %2 buttons at the row height)" )
					.arg( sAgree ).arg( btns.size() ),
					btns.size() >= 4 && sAgree < btns.size() );

				for ( int i = 0; i < victims.size(); i++ )
					victims.at( i )->setStyleSheet( sheetsBefore.at( i ) );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int rMin = 0, rMax = 0, rAgree = 0;
				worstOff( rMin, rMax, rAgree );
				say( *st, QStringLiteral( "  R3 floor: restored, the buttons read %1..%2 again" )
					.arg( rMin ).arg( rMax ) );
				check( *st, QStringLiteral( "(R3 floor) ...and taking it away puts every button "
					"back on the row (%1 of %2), so the picture below is the shipped state" )
					.arg( rAgree ).arg( btns.size() ),
					btns.size() >= 4 && rAgree == btns.size() );
			}

			/* R4: the row's box is stated ONCE, by the skin, from the row's own
			 * number -- and it is the sheet the code applied, not a copy. */
			if ( compact ) {
				const QString sheet = wwBarRowButtonQss( wwBarRowButtonContent() );
				const QString box = wwBarRowBoxQss();
				const int padT = sheet.count( QStringLiteral( "padding-top:" ) );
				const int padB = sheet.count( QStringLiteral( "padding-bottom:" ) );
				const int mins = sheet.count( QStringLiteral( "min-height:" ) );
				const int sels = sheet.count( QLatin1Char( '{' ) );
				say( *st, QStringLiteral( "  R4: the skin's button sheet is %1 chars, %2 selectors, "
					"%3 min-heights, %4/%5 vertical paddings; its bar-box sheet is \"%6\"" )
					.arg( sheet.size() ).arg( sels ).arg( mins ).arg( padT ).arg( padB ).arg( box ) );
				/* The box selectors are the ones that carry a height; the menu
				 * arrow's rule is the one selector that must NOT, and it is
				 * counted by name below rather than left out of the arithmetic. */
				check( *st, QStringLiteral( "(R4) the skin states the height and the air once per "
					"box selector (%1 selectors, %2 min-heights, %3 top / %4 bottom paddings)" )
					.arg( sels ).arg( mins ).arg( padT ).arg( padB ),
					mins >= 2 && padT == mins && padB == mins && sels == mins + 1 );
				check( *st, QStringLiteral( "(R4) ...and the menu arrow is centred with the row, "
					"so it does not drop to the corner of a taller button" ),
					sheet.contains( QStringLiteral( "menu-indicator" ) )
						&& sheet.contains( QStringLiteral( "right center" ) ) );
				check( *st, QStringLiteral( "(R4) ...and nothing horizontal, so each button keeps "
					"its own width" ),
					!sheet.contains( QStringLiteral( "padding-left" ) )
						&& !sheet.contains( QStringLiteral( "padding-right" ) )
						&& sheet.count( QStringLiteral( "padding:" ) ) == 0 );
				check( *st, QStringLiteral( "(R4) the bar's own box is taken away, which is what "
					"puts a button at y 0 instead of y 4" ),
					box.contains( QStringLiteral( "QToolBar" ) )
						&& box.contains( QStringLiteral( "margin: 0px" ) ) );
				check( *st, QStringLiteral( "(R4) ...and the bars actually carry it" ),
					tView != nullptr && tView->styleSheet().contains( box ) );
				check( *st, QStringLiteral( "(R4 floor) and no sheet at all for an empty row" ),
					wwBarRowButtonQss( 0 ).isEmpty() );
			}

			// R5: the search row's content top, printed pass or fail
			say( *st, QStringLiteral( "  R5: search row top %1, viewport content top %2" )
				.arg( rSearch.isNull() ? -1 : rSearch.top() )
				.arg( rHeader.isNull() ? -1 : rHeader.bottom() + 1 ) );
			check( *st, QStringLiteral( "(R5) the search row still starts where the viewport's "
				"content starts (<= 1 px)" ),
				!rSearch.isNull() && !rHeader.isNull()
					&& qAbs( rSearch.top() - ( rHeader.bottom() + 1 ) ) <= 1 );

			/* =============================================================
			 *  S -- FOUR PIXELS FROM EVERY NEIGHBOUR
			 *
			 *  bungo, 2026-09-10 20:1x, on a screenshot of the top rows,
			 *  verbatim: "just do what is on my screenshot, 4 pixels from
			 *  each nearby element of separation for the header / blocks /
			 *  files". Five distances, one number, 1 px of tolerance:
			 *
			 *    S1 the row's top edge      -> the strip's top
			 *    S2 the strip's bottom      -> the row's bottom (the search
			 *                                  row below it does not move)
			 *    S3 the window's content edge -> the strip's left
			 *    S4 the strip's right       -> the toolbar beside it
			 *    S5 each pair of segments
			 *
			 *  MEASURED IN PIXELS, not in rects, and the reason is in
			 *  paintedSegment() above: tabRect() includes the margin, so it
			 *  reads the same before and after and a gate built on it would
			 *  be green either way. The row's own height is R1/R2's business
			 *  and is asserted again here, because the whole ruling is that
			 *  the strip gets shorter INSIDE a row that does not move.
			 * ============================================================= */
			say( *st, QStringLiteral( "--- S: the strip's air (bungo's 4 px) ---" ) );
			const int air = wwSegmentedStripAir();
			say( *st, QStringLiteral( "  wwSegmentedStripAir() = %1" ).arg( air ) );
			if ( !tabs || rTabs.isNull() || rHeader.isNull() ) {
				skip( *st, QStringLiteral( "S1..S5 need the strip and the toolbar beside "
					"it; one of them is absent or hidden" ) );
			} else if ( air <= 0 ) {
				skip( *st, QStringLiteral( "UI/SegmentedStripAir is 0 -- the way back is on "
					"and the strip is flush against its neighbours BY REQUEST, so S1..S5 "
					"do not describe this profile" ) );
			} else {
				const QColor fill( wwSkinColor( "selBgActive" ) );
				QList<int> visible;
				for ( int i = 0; i < tabs->count(); i++ )
					if ( tabs->isTabVisible( i ) )
						visible.append( i );
				qreal dpr = 1.0;

				/* ONE measurement function for the table, the verdict and the
				 * floor, so the floor exercises the code the verdict comes
				 * from (the R3 pattern). */
				struct Five { int top, bottom, left, right, gapMin, gapMax; bool real; };
				auto measure = [&]( QList<QRect> * boxesOut ) -> Five {
					Five f{ -99, -99, -99, -99, -99, -99, false };
					QList<QRect> bs;
					for ( int i : visible )
						bs.append( paintedSegment( tabs, i, fill, &dpr ) );
					if ( boxesOut )
						*boxesOut = bs;
					if ( bs.size() < 3 )
						return f;
					for ( const QRect & b : bs )
						if ( b.isNull() || b.width() < 20 || b.height() < 8 )
							return f;
					f.real = true;
					f.top = bs.first().top();
					f.bottom = tabs->height() - 1 - bs.first().bottom();
					f.left = rTabs.x() + bs.first().left();
					f.right = rHeader.x() - ( rTabs.x() + bs.last().right() ) - 1;
					f.gapMin = 1 << 20;
					f.gapMax = -1;
					for ( int i = 1; i < bs.size(); i++ ) {
						const int g = bs.at( i ).left() - bs.at( i - 1 ).right() - 1;
						f.gapMin = qMin( f.gapMin, g );
						f.gapMax = qMax( f.gapMax, g );
					}
					return f;
				};
				auto atAir = []( const Five & f, int want ) {
					return f.real && qAbs( f.top - want ) <= 1 && qAbs( f.bottom - want ) <= 1
						&& qAbs( f.left - want ) <= 1 && qAbs( f.right - want ) <= 1
						&& qAbs( f.gapMin - want ) <= 1 && qAbs( f.gapMax - want ) <= 1;
				};

				QList<QRect> boxes;
				const Five f = measure( &boxes );
				say( *st, QStringLiteral( "  S: device pixel ratio %1, strip bar %2x%3 at "
					"x %4 top %5, toolbar beside it at x %6" )
					.arg( dpr ).arg( rTabs.width() ).arg( rTabs.height() )
					.arg( rTabs.x() ).arg( rTabs.top() ).arg( rHeader.x() ) );
				for ( int i = 0; i < boxes.size(); i++ )
					say( *st, QStringLiteral( "  S segment %1 \"%2\" painted: x %3 y %4 "
						"w %5 h %6 (right %7 bottom %8)" )
						.arg( i ).arg( tabs->tabText( visible.at( i ) ) )
						.arg( boxes.at( i ).x() ).arg( boxes.at( i ).y() )
						.arg( boxes.at( i ).width() ).arg( boxes.at( i ).height() )
						.arg( boxes.at( i ).right() ).arg( boxes.at( i ).bottom() ) );
				say( *st, QStringLiteral( "  S THE FIVE: top %1  bottom %2  left %3  "
					"right-to-toolbar %4  between %5..%6   (want %7, +/-1)" )
					.arg( f.top ).arg( f.bottom ).arg( f.left ).arg( f.right )
					.arg( f.gapMin ).arg( f.gapMax ).arg( air ) );

				check( *st, QStringLiteral( "(S floor) every segment of the strip was found "
					"as a real painted box (%1 of %2 visible tabs, first %3x%4)" )
					.arg( boxes.size() ).arg( visible.size() )
					.arg( boxes.isEmpty() ? 0 : boxes.first().width() )
					.arg( boxes.isEmpty() ? 0 : boxes.first().height() ),
					f.real && boxes.size() == visible.size() && visible.size() >= 3 );
				check( *st, QStringLiteral( "(S floor) ...and the SAME scan finds nothing at "
					"all for a colour the strip does not carry, so it is a real search" ),
					paintedSegment( tabs, visible.first(), QColor( 255, 0, 255 ),
						nullptr ).isNull() );

				check( *st, QStringLiteral( "(S1) the strip starts %1 px below the row's top "
					"edge (want %2)" ).arg( f.top ).arg( air ),
					f.real && qAbs( f.top - air ) <= 1 );
				check( *st, QStringLiteral( "(S2) ...and ends %1 px above the row's bottom "
					"edge, where the search row begins (want %2)" ).arg( f.bottom ).arg( air ),
					f.real && qAbs( f.bottom - air ) <= 1 );
				check( *st, QStringLiteral( "(S3) the first segment is %1 px from the "
					"window's content edge (want %2)" ).arg( f.left ).arg( air ),
					f.real && qAbs( f.left - air ) <= 1 );
				check( *st, QStringLiteral( "(S4) the last segment is %1 px from the toolbar "
					"beside it (want %2; the dock separator already paints %3 of them)" )
					.arg( f.right ).arg( air )
					.arg( skope->style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent,
						nullptr, skope ) ),
					f.real && qAbs( f.right - air ) <= 1 );
				check( *st, QStringLiteral( "(S5) every pair of segments is %1..%2 px apart "
					"(want %3)" ).arg( f.gapMin ).arg( f.gapMax ).arg( air ),
					f.real && qAbs( f.gapMin - air ) <= 1 && qAbs( f.gapMax - air ) <= 1 );
				check( *st, QStringLiteral( "(S6) and the ROW did not move: the strip's bar "
					"is still %1 px, the segments %2 -- shorter by twice the air inside the "
					"same row" ).arg( rTabs.height() )
					.arg( boxes.isEmpty() ? -1 : boxes.first().height() ),
					rTabs.height() == row && !boxes.isEmpty()
						&& qAbs( boxes.first().height() - ( row - 2 * air ) ) <= 1 );

				/* THE FLOOR THAT FIRES, both halves in one run (the R3 pattern).
				 *
				 * The 18:25:20 strip's own sheet -- flush margins, the shared
				 * seam, square inner corners, min-height row - 8 -- is appended
				 * live over the shipped one, and the SAME predicate is asked
				 * again. It must go RED, with every distance at 0, and green
				 * again once it is taken away; the restore is also what makes
				 * the pictures below the shipped state. */
				const QString saved = tabs->styleSheet();
				const QString flush = QStringLiteral(
					"QTabBar::tab { margin: 0px; border-left: 0; border-radius: 0;"
					" min-height: %1px; }"
					"QTabBar::tab:first { border-left: 1px solid %2; }"
					"QTabBar::tab:last { margin-right: 0px; }" )
					.arg( qMax( 10, row - 8 ) ).arg( wwSkinColor( "border" ) );
				tabs->setStyleSheet( saved + flush );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				const Five sab = measure( nullptr );
				say( *st, QStringLiteral( "  S floor: with the 18:25:20 flush strip put back, "
					"the five read top %1 bottom %2 left %3 right %4 between %5..%6" )
					.arg( sab.top ).arg( sab.bottom ).arg( sab.left ).arg( sab.right )
					.arg( sab.gapMin ).arg( sab.gapMax ) );
				check( *st, QStringLiteral( "(S floor) the SAME five go red on the strip that "
					"shipped at 18:25:20 (top %1, between %2)" )
					.arg( sab.top ).arg( sab.gapMax ), !atAir( sab, air ) );

				tabs->setStyleSheet( saved );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				const Five back = measure( nullptr );
				say( *st, QStringLiteral( "  S floor: restored, the five read top %1 bottom "
					"%2 left %3 right %4 between %5..%6 again" )
					.arg( back.top ).arg( back.bottom ).arg( back.left ).arg( back.right )
					.arg( back.gapMin ).arg( back.gapMax ) );
				check( *st, QStringLiteral( "(S floor) ...and taking it away puts all five "
					"back at %1, so the pictures below are the shipped state" ).arg( air ),
					atAir( back, air ) );

				// leave the strip where group R put it, for the picture
				for ( int i = 0; i < tabs->count(); i++ )
					if ( tabs->tabText( i ).compare( QStringLiteral( "Files" ),
													 Qt::CaseInsensitive ) == 0 ) {
						tabs->setCurrentIndex( i );
						break;
					}
				QApplication::processEvents();

				// the zoomed crop of the strip, for bungo
				if ( !st->stripShot.isEmpty() ) {
					const int pad = 10;
					const QRect crop( 0, qMax( 0, rTabs.top() - pad ),
						qMin( skope->width(), rTabs.x() + rTabs.width() + 190 ),
						rTabs.height() + 2 * pad );
					const QPixmap pm = skope->grab( crop );
					const QImage big = pm.toImage().scaled( pm.width() * 4, pm.height() * 4,
						Qt::IgnoreAspectRatio, Qt::FastTransformation );
					const bool wrote = !big.isNull() && big.save( st->stripShot );
					say( *st, QStringLiteral( "  strip crop %1x%2 -> 4x %3x%4 -> %5 (%6)" )
						.arg( pm.width() ).arg( pm.height() ).arg( big.width() )
						.arg( big.height() ).arg( st->stripShot,
							wrote ? QStringLiteral( "written" ) : QStringLiteral( "FAILED" ) ) );
					check( *st, QStringLiteral( "(shot) the zoomed strip crop was written" ),
						wrote && big.width() > 400 );
				}
			}

			// the top-strip picture, for bungo
			if ( !st->shot.isEmpty() && !rMenu.isNull() && !rTabs.isNull() ) {
				const int bottom = qMin( skope->height() - 1,
					qMax( rSearch.isNull() ? rTabs.bottom() : rSearch.bottom(),
						  rHeader.isNull() ? rTabs.bottom() : rHeader.bottom() ) + 14 );
				const QPixmap pm = skope->grab( QRect( QPoint( 0, 0 ),
					QPoint( skope->width() - 1, bottom ) ) );
				const bool wrote = !pm.isNull() && pm.toImage().save( st->shot );
				say( *st, QStringLiteral( "  top-strip grab %1x%2 -> %3 (%4)" )
					.arg( pm.width() ).arg( pm.height() ).arg( st->shot,
						  wrote ? QStringLiteral( "written" ) : QStringLiteral( "FAILED" ) ) );
				check( *st, QStringLiteral( "(shot) the top-strip grab was written" ), wrote );
				check( *st, QStringLiteral( "(shot floor) ...and it is not an empty strip" ),
					pm.width() > 400 && pm.height() > 40 );
			}

			// =============================================================
			//  L -- the Water tab, in the LOD GENERATION PANEL (lane WATER8)
			//
			//  bungo, on seeing lane WATER7's Water tab in the LEFT strip,
			//  verbatim: "What? I wanted it in that right panel though".  Group
			//  T measured the left strip and is RETIRED; group L measures the
			//  LOD Generation panel's own strip -- two tabs, LOD | Water, in the
			//  same row and the same sheet as Header | Blocks | Files -- and
			//  asserts the left strip went back to its three tabs in both
			//  workspace states.
			//
			//  It lives in src/wateruitest_lod.cpp because THIS file belonged to
			//  lane UI4 while WATER8 was written (one lane per file,
			//  CONSTITUTION 1), which is the same translation-unit rule every
			//  other harness in this tree follows.  It writes into this run's own
			//  log and counts, so there is still one spell and one verdict.
			// =============================================================
			say( *st, QStringLiteral( "  the window holds: LodGenerationDock %1, LeftColumnDock "
				"%2, LeftColumnStack %3 pages, ViewWorkspacesMenu %4 actions" )
				.arg( lodDock ? QStringLiteral( "yes" ) : QStringLiteral( "no" ),
					  leftDock ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) )
				.arg( stack ? stack->count() : -1 )
				.arg( wsMenu ? wsMenu->actions().size() : -1 ) );
			{
				extern void wwWaterUiLodTabs( NifSkope *, QTextStream &, int &, int &,
					int &, const QString &, const QString & );
				wwWaterUiLodTabs( skope, log, st->checks, st->fails, st->skips,
					qEnvironmentVariable( "WW_WATERUI_LODSHOT" ),
					qEnvironmentVariable( "WW_WATERUI_LODSHOT2" ) );
			}

			log << st->checks << " checks, " << st->fails << " failures, "
				<< st->skips << " skips\n";
			log << ( st->fails == 0 ? "PASS" : "FAIL" ) << "\n";
			log << "done\n";
			log.flush();
			logf.close();

			// leave nothing for the close dialog to ask about, or the app never
			// quits and the spell times out with no verdict
			if ( NifModel * n = skope->getNifModel(); n && n->undoStack )
				n->undoStack->setClean();
			skope->setWindowModified( false );
			QTimer::singleShot( 100, qApp, &QApplication::quit );
		} );
	} );
}
