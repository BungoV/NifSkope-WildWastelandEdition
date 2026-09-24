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
#include <QHash>
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

/*! THE TEXT INK of one menu title: the rows of pixels much brighter than the
 *  bar's own background, inside [x0,x1] of the bar's own grab, in the bar's own
 *  logical pixels.
 *
 *  WHY PIXELS AND NOT actionGeometry(). Lane UI5 measured it outside the
 *  application (scratchpad/ui5_20260910/probe.cpp, 30 cases): once the row
 *  states the item's vertical padding, the item's RECT and the item's PAINTED
 *  BOX are both exactly the row -- and what bungo looked at is the TEXT inside
 *  them, which no rect the menu bar can be asked for describes. It is also the
 *  same method scratchpad/ui5_20260910/measure_before.py used on the shipped
 *  grab, so the harness's number and the report's number are one number.
 *
 *  `lift` is added to the threshold, so the caller can ask the SAME scan for a
 *  brightness no pixel in the bar carries and watch it find nothing -- a scan
 *  that has stopped finding anything cannot then pass for a centred title.
 */
QRect menuInk( QMenuBar * bar, int x0, int x1, double lift )
{
	if ( !bar || x1 < x0 )
		return QRect();
	const QPixmap pm = bar->grab();
	if ( pm.isNull() )
		return QRect();
	const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
	const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
	// the bar's own background is the commonest colour in the strip
	QHash<QRgb, int> hist;
	for ( int y = 0; y < img.height(); y++ )
		for ( int x = 0; x < img.width(); x++ )
			hist[img.pixel( x, y ) & 0x00ffffff]++;
	QRgb bg = 0;
	int best = -1;
	for ( auto it = hist.cbegin(); it != hist.cend(); ++it )
		if ( it.value() > best ) {
			best = it.value();
			bg = it.key();
		}
	auto lum = []( QRgb c ) {
		return 0.299 * qRed( c ) + 0.587 * qGreen( c ) + 0.114 * qBlue( c );
	};
	const double thresh = lum( bg ) + 40.0 + lift;
	int y0 = 1 << 20, y1 = -1, ix0 = 1 << 20, ix1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			const int lx = qRound( x / dpr );
			if ( lx < x0 || lx > x1 )
				continue;
			if ( lum( img.pixel( x, y ) ) <= thresh )
				continue;
			y0 = qMin( y0, qRound( y / dpr ) );
			y1 = qMax( y1, qRound( y / dpr ) );
			ix0 = qMin( ix0, lx );
			ix1 = qMax( ix1, lx );
		}
	}
	if ( y1 < 0 )
		return QRect();
	return QRect( QPoint( ix0, y0 ), QPoint( ix1, y1 ) );
}

//! A widget's rectangle in MAIN-WINDOW coordinates.
/*! WHAT THE MENU ARROW IS CLEAR OF, in the button's own logical pixels.
 *
 *  bungo, 2026-09-11 05:4x, over scratchpad/ui3_20260910/images/cmp_zoom.png:
 *  "That's fine, as long as the dropdown arrows do not intersect with the text
 *  / icons like on the screenshots you showed me".
 *
 *  TWO RENDERS OF THE SAME BUTTON at the same PINNED size: one as it stands,
 *  one with its menu taken away. The columns that DIFFER between them are the
 *  arrow and nothing else -- the geometry cannot move, because the size is
 *  pinned across the pair -- and the glyph's last ink column is read off the
 *  second render, so both numbers come from one instrument.
 *
 *  NOT a grab: an auto-raise QToolButton paints no background of its own and
 *  QWidget::grab() hands back a pixmap Qt filled with WHITE, on which this
 *  theme's near-white glyphs are invisible (measured in the probe: background
 *  #efefef, icon #e6e8eb, seven levels apart). Rendering over a known dark fill
 *  is what makes the ink findable at all.
 *
 *  NOT a rect either: nothing a QToolButton can be asked for says where the
 *  style drew its menu-indicator.
 */
struct WwArrow
{
	int arrowFirst = -1;   //!< first column the arrow occupies
	int inkLast = -1;      //!< last column the icon or label occupies
	int cols = 0;          //!< how many columns the arrow occupies
	int gap = -99;         //!< clear columns between them
	bool ok = false;       //!< both were found, so `gap` means something
};

WwArrow arrowGap( QToolButton * b )
{
	WwArrow a;
	if ( !b || !b->isVisible() || b->width() < 8 || b->height() < 8 )
		return a;
	const int w = b->width(), h = b->height();
	/* The caller has already pinned every button in the row (see pinAll below).
	 * Pinning and unpinning them ONE AT A TIME makes the toolbar re-lay out
	 * thirteen times per sweep, and the sweep after that reads a button a pixel
	 * narrow -- which is what the first run of this group went red on, in its
	 * restore half, on a window that was correct. */
	const QSize mn = b->minimumSize(), mx = b->maximumSize();
	b->setFixedSize( w, h );
	QApplication::processEvents();

	auto shot = [&]() {
		QPixmap pm( w, h );
		pm.fill( QColor( wwSkinColor( "bgBar" ) ) );
		b->render( &pm, QPoint(), QRegion(), QWidget::DrawChildren );
		return pm.toImage().convertToFormat( QImage::Format_RGB32 );
	};
	const QImage with = shot();
	QMenu * ownMenu = b->menu();
	QAction * da = b->defaultAction();
	QMenu * actMenu = da ? da->menu() : nullptr;
	b->setMenu( nullptr );
	if ( actMenu )
		da->setMenu( nullptr );
	QApplication::processEvents();
	const QImage without = shot();
	if ( ownMenu )
		b->setMenu( ownMenu );
	if ( actMenu )
		da->setMenu( actMenu );
	b->setMinimumSize( mn );
	b->setMaximumSize( mx );
	QApplication::processEvents();
	if ( with.size() != without.size() || with.width() < 4 )
		return a;

	QHash<QRgb, int> hist;
	for ( int y = 0; y < without.height(); y++ )
		for ( int x = 0; x < without.width(); x++ )
			hist[without.pixel( x, y ) & 0x00ffffff]++;
	QRgb bg = 0;
	int best = -1;
	for ( auto it = hist.cbegin(); it != hist.cend(); ++it )
		if ( it.value() > best ) { best = it.value(); bg = it.key(); }
	auto lum = []( QRgb c ) {
		return 0.299 * qRed( c ) + 0.587 * qGreen( c ) + 0.114 * qBlue( c );
	};
	const double bgL = lum( bg );

	for ( int x = 0; x < with.width(); x++ ) {
		bool differs = false, ink = false;
		for ( int y = 0; y < with.height(); y++ ) {
			if ( ( with.pixel( x, y ) & 0x00ffffff ) != ( without.pixel( x, y ) & 0x00ffffff ) )
				differs = true;
			if ( qAbs( lum( without.pixel( x, y ) & 0x00ffffff ) - bgL ) > 40.0 )
				ink = true;
		}
		if ( differs ) {
			if ( a.arrowFirst < 0 )
				a.arrowFirst = x;
			a.cols++;
		}
		if ( ink )
			a.inkLast = x;
	}
	a.ok = ( a.arrowFirst >= 0 && a.inkLast >= 0 );
	if ( a.ok )
		a.gap = a.arrowFirst - a.inkLast - 1;
	return a;
}

//! Does this button carry a menu, so that the style draws an arrow on it?
bool hasMenu( QToolButton * b )
{
	return b && ( b->menu() != nullptr
		|| ( b->defaultAction() && b->defaultAction()->menu() != nullptr ) );
}

/*! Freeze the whole row's geometry for the length of a sweep, and give it back.
 *
 *  One layout state for thirteen measurements. Without it each arrowGap() call
 *  re-lays out the bar, the next button is measured mid-relayout, and the third
 *  sweep of a correct window reads one button a pixel narrow. Returns the sizes
 *  to hand back to unpinAll().
 */
QList<QPair<QSize, QSize>> pinAll( const QList<QToolButton *> & btns )
{
	QList<QPair<QSize, QSize>> saved;
	for ( QToolButton * b : btns ) {
		saved.append( qMakePair( b->minimumSize(), b->maximumSize() ) );
		b->setFixedSize( b->width(), b->height() );
	}
	for ( int i = 0; i < 4; i++ )
		QApplication::processEvents();
	return saved;
}

void unpinAll( const QList<QToolButton *> & btns, const QList<QPair<QSize, QSize>> & saved )
{
	for ( int i = 0; i < btns.size() && i < saved.size(); i++ ) {
		btns.at( i )->setMinimumSize( saved.at( i ).first );
		btns.at( i )->setMaximumSize( saved.at( i ).second );
	}
	for ( int i = 0; i < 6; i++ )
		QApplication::processEvents();
}

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
					mins >= 2 && padT == mins && padB == mins && sels == mins + 2 );
				check( *st, QStringLiteral( "(R4) ...and the menu arrow is centred with the row, "
					"so it does not drop to the corner of a taller button" ),
					sheet.contains( QStringLiteral( "menu-indicator" ) )
						&& sheet.contains( QStringLiteral( "right center" ) ) );
				/* AMENDED BY LANE UI6. This check used to read "nothing
				 * horizontal". bungo, 2026-09-11 05:4x: "That's fine, as long as
				 * the dropdown arrows do not intersect with the text / icons
				 * like on the screenshots you showed me" -- so the row now
				 * states exactly ONE horizontal rule, the column the menu arrow
				 * needs, and only for the buttons that HAVE a menu. Everything
				 * else horizontal is still forbidden, and the count pins that
				 * there is one such rule and not two. */
				check( *st, QStringLiteral( "(R4) ...and the only horizontal rule is the menu "
					"arrow's own column (%1 padding-right, %2 [wwHasMenu] selector, 0 "
					"padding-left, 0 shorthand padding)" )
					.arg( sheet.count( QStringLiteral( "padding-right:" ) ) )
					.arg( sheet.count( QStringLiteral( "QToolButton[wwHasMenu=\"true\"]" ) ) ),
					!sheet.contains( QStringLiteral( "padding-left" ) )
						&& sheet.count( QStringLiteral( "padding:" ) ) == 0
						&& sheet.count( QStringLiteral( "padding-right:" ) ) == 1
						&& sheet.count( QStringLiteral( "QToolButton[wwHasMenu=\"true\"]" ) ) == 1 );
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
				/* AMENDED BY LANE UI6. Four of the five distances are the air;
				 * the fifth is ZERO. bungo, 2026-09-10 21:0x, over the zoomed
				 * screenshot of the strip UI4 had just gapped, verbatim: "Why
				 * are they separated?" -- a segmented control is ONE element,
				 * and the air belongs outside its box, never inside it. */
				auto atAir = []( const Five & f, int want ) {
					return f.real && qAbs( f.top - want ) <= 1 && qAbs( f.bottom - want ) <= 1
						&& qAbs( f.left - want ) <= 1 && qAbs( f.right - want ) <= 1
						&& f.gapMin == 0 && f.gapMax == 0;
				};
				auto joined = []( const Five & f ) {
					return f.real && f.gapMin == 0 && f.gapMax == 0;
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
				check( *st, QStringLiteral( "(S5) the segments TOUCH: every pair is %1..%2 px "
					"apart (want 0)" ).arg( f.gapMin ).arg( f.gapMax ), joined( f ) );
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
				/* THE SECOND FLOOR, and the one this lane exists for.
				 *
				 * The floor above puts back the 18:25:20 FLUSH strip, which
				 * moves all five distances at once -- so it would still fire if
				 * the gap alone had been left at UI4's 4. This one puts back
				 * UI4's SEPARATED sheet and nothing else: `margin-left` on every
				 * segment, its own border, its own rounded corners. S5's own
				 * predicate must go RED with the gap back at 4, and green again
				 * when it is taken away. Without it, S5 is a check that has only
				 * ever been seen green. */
				const QString apart = QStringLiteral(
					"QTabBar::tab { margin-left: %1px; border-left: 1px solid %2;"
					" border-radius: 3px; }" ).arg( air ).arg( wwSkinColor( "border" ) );
				tabs->setStyleSheet( saved + apart );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				const Five sep = measure( nullptr );
				say( *st, QStringLiteral( "  S floor: with UI4's SEPARATED strip put back, the "
					"gap reads %1..%2 (top %3, left %4)" )
					.arg( sep.gapMin ).arg( sep.gapMax ).arg( sep.top ).arg( sep.left ) );
				check( *st, QStringLiteral( "(S5 floor) the SAME gap test goes red on the strip "
					"bungo asked about (%1..%2 apart, want 0)" )
					.arg( sep.gapMin ).arg( sep.gapMax ), !joined( sep ) );

				tabs->setStyleSheet( saved );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				const Five rejoined = measure( nullptr );
				say( *st, QStringLiteral( "  S floor: restored, the gap reads %1..%2 again" )
					.arg( rejoined.gapMin ).arg( rejoined.gapMax ) );
				check( *st, QStringLiteral( "(S5 floor) ...and taking it away joins the segments "
					"again (%1..%2)" ).arg( rejoined.gapMin ).arg( rejoined.gapMax ),
					joined( rejoined ) );

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

			/* =============================================================
			 *  M -- THE TITLES IN THE MENU BAR SIT ON THE ROW'S CENTRE LINE
			 *
			 *  bungo, 2026-09-10 20:2x, verbatim: "Also, please center file /
			 *  view / spells / options / help buttons, top left".
			 *
			 *  Measured on the shipped 20:45:47 window by
			 *  scratchpad/ui5_20260910/measure_before.py: the five titles' text
			 *  sat at y 6..16 in a 35 px row whose centre line is 17.0 -- six
			 *  pixels high. This group reads the same thing out of the LIVE menu
			 *  bar, in PIXELS. The rect cannot answer it: once the row states the
			 *  item's padding, the rect and the painted box are both the whole
			 *  row, and it is the text inside them that moved.
			 *
			 *  It runs BEFORE the picture below, and both halves of its floor are
			 *  checks, so whatever it appends is proved taken away again and the
			 *  picture is the shipped state.
			 * ============================================================= */
			say( *st, QStringLiteral( "--- M: the menu bar's titles in the row ---" ) );
			if ( !compact )
				skip( *st, QStringLiteral( "M1..M6 measure the compact bar row's menu items; this "
					"profile has UI/CompactTopBars OFF, so the titles are where they were before "
					"lane UI3 and these gates do not describe them" ) );
			if ( compact && menubar && row > 0 ) {
				const double mCentre = ( menubar->height() - 1 ) / 2.0;
				/* The PAINTED titles, not every action the bar holds: a hidden or
				 * zero-width action has no box to read ink out of, and counting it
				 * as a missing title would make the floor below refuse a menu bar
				 * that is perfectly correct. Counted here once, and the same rule
				 * is what readInk skips on. */
				QList<QAction *> titles;
				for ( QAction * a : menubar->actions() ) {
					const QRect g = menubar->actionGeometry( a );
					if ( !g.isNull() && g.width() > 0 && !a->isSeparator() )
						titles.append( a );
				}
				/* WHICH BAND OF INK IS "THE TEXT" -- found by running this gate
				 * once and reading its own numbers (2026-09-11).
				 *
				 * The first version of M2 took the whole ink band, top row to
				 * bottom row, and called its midpoint the text's centre. That band
				 * is not the letters. It also contains (a) the MNEMONIC UNDERLINE,
				 * which Qt draws two rows BELOW the baseline under the F of File,
				 * the V of View and so on, and (b) the DESCENDER of the p in Spells
				 * and Options, which reaches four rows below it. Both pull the
				 * midpoint down, and by different amounts per title -- so the band's
				 * midpoint read 18.0 for File and 18.5 for Spells while the letters
				 * themselves were sitting exactly on the row's centre line.
				 *
				 * What a reader judges "centred" by is the CAP BAND: the top of the
				 * capital letter down to the baseline. Its top is the topmost ink
				 * row, which is measured here, and its depth is the font's own
				 * capHeight, which is measured by Qt -- no constant is typed and
				 * nothing about the row is assumed. Measured on this build:
				 * ascent 12, descent 4, height 16, capHeight 8.
				 *
				 * The whole ink band is still PRINTED beside it, every run, because
				 * it is what the before/after pictures show and it is what
				 * scratchpad/ui5_20260910/measure_before.py read off the shipped
				 * window.
				 *
				 * ONE predicate for the verdict, the way back and the floor, so the
				 * floor exercises the code the verdict comes from. */
				const int capH = menubar->fontMetrics().capHeight();
				say( *st, QStringLiteral( "  M: the menu bar's font: ascent %1, descent %2, height "
					"%3, capHeight %4 -- the verdict reads the CAP BAND (ink top .. ink top + "
					"capHeight), not the whole ink band, which also holds the mnemonic underline "
					"and the descender of Spells / Options" )
					.arg( menubar->fontMetrics().ascent() ).arg( menubar->fontMetrics().descent() )
					.arg( menubar->fontMetrics().height() ).arg( capH ) );
				auto readInk = [&]( double lift, QString * detail, int * found,
									double * worst, double * spread ) {
					*found = 0;
					*worst = 0.0;
					double lo = 1e9, hi = -1e9;
					QString d;
					for ( QAction * a : titles ) {
						const QRect g = menubar->actionGeometry( a );
						if ( g.isNull() || g.width() <= 0 )
							continue;
						const QRect ink = menuInk( menubar, g.left(), g.right(), lift );
						if ( ink.isNull() )
							continue;
						( *found )++;
						// the CAP BAND: the topmost ink row down by the font's own
						// capHeight. The whole band is printed beside it.
						const double c = ink.top() + capH / 2.0;
						if ( qAbs( c - mCentre ) > qAbs( *worst ) )
							*worst = c - mCentre;
						lo = qMin( lo, c );
						hi = qMax( hi, c );
						d += QStringLiteral( " %1[ink y %2..%3, cap c %4]" )
							.arg( a->text().remove( QLatin1Char( '&' ) ) )
							.arg( ink.top() ).arg( ink.bottom() ).arg( c, 0, 'f', 1 );
					}
					*spread = ( *found > 0 ) ? hi - lo : -1.0;
					if ( detail )
						*detail = d;
				};

				QString detail;
				int found = 0;
				double worst = 0.0, spread = 0.0;
				readInk( 0.0, &detail, &found, &worst, &spread );
				say( *st, QStringLiteral( "  M: menu bar %1x%2 at y %3, row %4, centre line %5; the "
					"skin measured item content %6 -> padding-top %7 / padding-bottom %8" )
					.arg( rMenu.width() ).arg( rMenu.height() ).arg( rMenu.top() ).arg( row )
					.arg( mCentre, 0, 'f', 1 ).arg( wwBarRowMenuItemContent() )
					.arg( wwBarRowMenuItemPadTop() ).arg( wwBarRowMenuItemPadBottom() ) );
				say( *st, QStringLiteral( "  M: %1 titles read:%2  worst %3, spread %4" )
					.arg( found ).arg( detail ).arg( worst, 0, 'f', 1 ).arg( spread, 0, 'f', 1 ) );

				check( *st, QStringLiteral( "(M floor) every painted title in the menu bar was found "
					"as real text ink (%1 of %2), and the font reports a real cap height (%3)" )
					.arg( found ).arg( titles.size() ).arg( capH ),
					titles.size() >= 5 && found == titles.size() && capH > 0 );
				{
					int none = 0;
					double w2 = 0.0, s2 = 0.0;
					QString d2;
					readInk( 255.0, &d2, &none, &w2, &s2 );
					check( *st, QStringLiteral( "(M floor) ...and the SAME scan finds nothing for a "
						"brightness the bar does not carry (%1 titles)" ).arg( none ), none == 0 );
				}
				check( *st, QStringLiteral( "(M1) the menu bar is still exactly the row's height "
					"(%1 px at y %2, row %3) -- centring the titles did not grow it" )
					.arg( rMenu.height() ).arg( rMenu.top() ).arg( row ),
					!rMenu.isNull() && rMenu.height() == row && rMenu.top() == 0 );
				check( *st, QStringLiteral( "(M2) every title's text is on the row's centre line "
					"within 1 px (worst cap-band offset %1)" ).arg( worst, 0, 'f', 1 ),
					found >= 5 && qAbs( worst ) <= 1.0 );
				check( *st, QStringLiteral( "(M2) ...and the five agree with each other within 1 px "
					"(cap-band spread %1)" ).arg( spread, 0, 'f', 1 ),
					found >= 5 && spread >= 0.0 && spread <= 1.0 );

				/* M3 IS THE REFUTER this lane registered before it wrote a line:
				 * wwAlignBarRow takes the TALLEST bar's own hint as the row, so a
				 * menu bar whose hint had grown past 35 would take the whole row
				 * with it the next time the row is aligned. */
				const int mHint = qMax( menubar->sizeHint().height(),
					menubar->minimumSizeHint().height() );
				say( *st, QStringLiteral( "  M3: the menu bar's own hint is %1 against a row of %2" )
					.arg( mHint ).arg( row ) );
				check( *st, QStringLiteral( "(M3) the menu bar's own size hint does not exceed the row "
					"(%1 <= %2), so aligning the row again cannot grow it" ).arg( mHint ).arg( row ),
					mHint <= row );
				{
					int real = 0, agree = 0;
					for ( const QRect & r : { rFile, rLOD, rView, rHeader, rTabs } ) {
						if ( r.isNull() || r.height() <= 0 )
							continue;
						real++;
						if ( qAbs( r.height() - row ) <= 1 )
							agree++;
					}
					check( *st, QStringLiteral( "(M3) ...and every other bar in the row is still the "
						"row's height (%1 of %2)" ).arg( agree ).arg( real ),
						real >= 4 && agree == real );
				}

				/* M4: THE WAY BACK, live. UI/CompactTopBars false appends no row
				 * sheet at all, so the menu bar carries res/style.qss alone -- which
				 * is this, and it must read the PRE-UI3 position, not the centre
				 * line. The setting itself is not written: this harness runs against
				 * the user's own QSettings and never changes a state it did not make.
				 * The sheet is the whole of the off value, so this is the whole of
				 * the way back for the titles. */
				const QString menuSaved = menubar->styleSheet();
				const QString rowRule = wwBarRowMenuItemQss( row, wwBarRowMenuItemContent() );
				check( *st, QStringLiteral( "(M4) the row's menu-item rule is the one the skin states, "
					"and the menu bar actually carries it (\"%1\")" ).arg( rowRule ),
					!rowRule.isEmpty() && menuSaved.contains( rowRule ) );
				check( *st, QStringLiteral( "(M4 floor) ...and the skin states no rule at all for a row "
					"of 0 or a content of 0" ),
					wwBarRowMenuItemQss( 0, 16 ).isEmpty()
						&& wwBarRowMenuItemQss( row, 0 ).isEmpty() );
				/* HOW THE OFF VALUE IS REPRODUCED, and why not by clearing the sheet
				 * (measured 2026-09-11, first run of this gate). With
				 * UI/CompactTopBars false wwAlignBarRow appends nothing, so the menu
				 * bar is styled by res/style.qss alone. The obvious way to show that
				 * -- setStyleSheet( QString() ) -- moved the titles by ZERO pixels in
				 * the application, although the identical call moves them in a
				 * standalone rig (scratchpad/ui5_20260910/probe.cpp case I: item
				 * 35 -> 24, ink top 13 -> 8). Something on this widget's path does
				 * not re-resolve on a sheet that becomes empty, and a gate is not the
				 * place to find out what.
				 *
				 * So the off state is reproduced by STATING the declaration the menu
				 * bar falls back to, read out of the live application stylesheet --
				 * not typed here, and not guessed: the first `QMenuBar::item {...}`
				 * block of res/style.qss, whatever it says today. A non-empty sheet
				 * is re-resolved (M5's sabotage below proves it in the same run), and
				 * the computed style is the one the off value produces.
				 *
				 * If the declaration cannot be found, this half SKIPS by name rather
				 * than passing. */
				QString fallbackRule;
				{
					const QString appSheet = qApp->styleSheet();
					const int b = appSheet.indexOf( QStringLiteral( "QMenuBar::item {" ) );
					const int e = b >= 0 ? appSheet.indexOf( QLatin1Char( '}' ), b ) : -1;
					if ( b >= 0 && e > b )
						fallbackRule = appSheet.mid( b, e - b + 1 );
				}
				say( *st, QStringLiteral( "  M4: the declaration the menu bar falls back to, read out "
					"of the live application stylesheet: \"%1\"" )
					.arg( fallbackRule.isEmpty() ? QStringLiteral( "<not found>" ) : fallbackRule ) );
				menubar->setStyleSheet( menuSaved + fallbackRule );
				for ( int i = 0; i < 4; i++ )
					QApplication::processEvents();
				int offFound = 0;
				double offWorst = 0.0, offSpread = 0.0;
				QString offDetail;
				readInk( 0.0, &offDetail, &offFound, &offWorst, &offSpread );
				say( *st, QStringLiteral( "  M4: with the row's rule overridden by the sheet's own "
					"(the off value of UI/CompactTopBars) the titles read:%1  worst %2; the item is "
					"%3 px" ).arg( offDetail ).arg( offWorst, 0, 'f', 1 )
					.arg( titles.isEmpty() ? -1
						: menubar->actionGeometry( titles.first() ).height() ) );
				if ( fallbackRule.isEmpty() )
					skip( *st, QStringLiteral( "the way-back half of M4: res/style.qss carries no "
						"\"QMenuBar::item {\" declaration to fall back to, so the off state cannot "
						"be reproduced live in this run" ) );
				else
					check( *st, QStringLiteral( "(M4) the way back puts the titles back high in the row, "
						"where they were before this lane (worst %1, against %2 with the row's sheet on)" )
						.arg( offWorst, 0, 'f', 1 ).arg( worst, 0, 'f', 1 ),
						offFound >= 5 && offWorst < -2.0 );

				/* M5: THE FLOOR THAT FIRES, live, in this same log. The arithmetic
				 * that SHIPPED at 20:45:47 -- the row's own two pixels of air, which
				 * is what wwBarRowButtonQss states and what left the titles six
				 * pixels high -- appended over the live sheet, and the SAME M2
				 * predicate asked again. */
				menubar->setStyleSheet( menuSaved + QStringLiteral(
					"QMenuBar::item { padding-top: 2px; padding-bottom: 2px; }" ) );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int sabFound = 0;
				double sabWorst = 0.0, sabSpread = 0.0;
				QString sabDetail;
				readInk( 0.0, &sabDetail, &sabFound, &sabWorst, &sabSpread );
				say( *st, QStringLiteral( "  M5 floor: with the 20:45:47 arithmetic put back (padding "
					"2 / 2) the titles read:%1  worst %2" )
					.arg( sabDetail ).arg( sabWorst, 0, 'f', 1 ) );
				check( *st, QStringLiteral( "(M5 floor) the SAME test goes red on the sheet that "
					"shipped with the titles six pixels high (worst %1)" ).arg( sabWorst, 0, 'f', 1 ),
					sabFound >= 5 && qAbs( sabWorst ) > 1.0 );

				menubar->setStyleSheet( menuSaved );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int backFound = 0;
				double backWorst = 0.0, backSpread = 0.0;
				QString backDetail;
				readInk( 0.0, &backDetail, &backFound, &backWorst, &backSpread );
				say( *st, QStringLiteral( "  M5 floor: restored, the titles read:%1  worst %2" )
					.arg( backDetail ).arg( backWorst, 0, 'f', 1 ) );
				check( *st, QStringLiteral( "(M5 floor) ...and taking it away puts every title back on "
					"the centre line (worst %1), so the picture below is the shipped state" )
					.arg( backWorst, 0, 'f', 1 ),
					backFound >= 5 && qAbs( backWorst ) <= 1.0 );

				/* M6: the rule is stated ONCE, by the skin, derived from the row --
				 * not a literal anybody typed -- and nothing horizontal. */
				say( *st, QStringLiteral( "  M6: the skin's menu-item rule is \"%1\"" ).arg( rowRule ) );
				check( *st, QStringLiteral( "(M6) the skin states one top and one bottom padding for "
					"the menu item, derived from the row (%1 + %2 + %3 = %4)" )
					.arg( wwBarRowMenuItemPadTop() ).arg( wwBarRowMenuItemContent() )
					.arg( wwBarRowMenuItemPadBottom() ).arg( row ),
					rowRule.count( QStringLiteral( "padding-top:" ) ) == 1
						&& rowRule.count( QStringLiteral( "padding-bottom:" ) ) == 1
						&& rowRule.count( QLatin1Char( '{' ) ) == 1
						&& wwBarRowMenuItemPadTop() + wwBarRowMenuItemContent()
							+ wwBarRowMenuItemPadBottom() == row );
				check( *st, QStringLiteral( "(M6) ...and nothing horizontal, so a title keeps the 8 px "
					"res/style.qss gives it on either side" ),
					!rowRule.contains( QStringLiteral( "padding-left" ) )
						&& !rowRule.contains( QStringLiteral( "padding-right" ) )
						&& rowRule.count( QStringLiteral( "padding:" ) ) == 0 );
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

			/* GROUP A RUNS LAST (lane UI6, after its own floor caught it).
			 * It is the only group here that leaves the window changed -- see
			 * the note in its restore half -- so it is placed after every
			 * grab this harness writes, and the pictures are of the window
			 * as it ships. */
			/* =============================================================
			 *  A -- THE DROPDOWN ARROWS DO NOT TOUCH THE GLYPHS
			 *
			 *  bungo, 2026-09-11 05:4x, over
			 *  scratchpad/ui3_20260910/images/cmp_zoom.png, verbatim: "That's
			 *  fine, as long as the dropdown arrows do not intersect with the
			 *  text / icons like on the screenshots you showed me".
			 *
			 *  Every button in the row that HAS a menu is measured, named and
			 *  printed -- a count cannot say which button is the tight one, and
			 *  the two tight ones are the whole complaint.
			 * ============================================================= */
			say( *st, QStringLiteral( "--- A: the menu arrows' air ---" ) );
			if ( !compact ) {
				skip( *st, QStringLiteral( "A1..A2 measure the compact row's button sheet; this "
					"profile has UI/CompactTopBars OFF, so the arrows are drawn against "
					"BUILD9's buttons and these gates do not describe them" ) );
			} else {
				QList<QToolButton *> menuBtns;
				QStringList menuNames;
				QList<QWidget *> barsWithButtons;
				barsWithButtons << header << tFile << tLOD << tView;
				for ( QWidget * w : barsWithButtons ) {
					if ( !w )
						continue;
					for ( QToolButton * b : w->findChildren<QToolButton *>() ) {
						if ( !b->isVisible()
							 || b->objectName() == QLatin1String( "qt_toolbar_ext_button" ) )
							continue;
						if ( !hasMenu( b ) )
							continue;
						menuBtns.append( b );
						QString n = b->objectName();
						if ( n.isEmpty() )
							n = b->text();
						if ( n.isEmpty() )
							n = QStringLiteral( "<icon %1x%2 at x %3>" )
								.arg( b->width() ).arg( b->height() )
								.arg( b->mapTo( skope, QPoint( 0, 0 ) ).x() );
						menuNames.append( n );
					}
				}

				/* ONE measurement function for the table, the verdict and the
				 * floor, so the floor exercises the code the verdict comes from
				 * (the R3 / S pattern). */
				auto sweep = [&]( int * worstOut, QString * worstName, int * readOut,
								  bool print ) {
					int worst = 1 << 20, read = 0;
					QString who;
					// ONE layout state for the whole sweep (see pinAll)
					const QList<QPair<QSize, QSize>> pinned = pinAll( menuBtns );
					for ( int i = 0; i < menuBtns.size(); i++ ) {
						const WwArrow a = arrowGap( menuBtns.at( i ) );
						if ( print )
							say( *st, QStringLiteral( "  A %1: %2x%3; arrow %4 col(s) from %5; "
								"glyph last ink %6; GAP %7%8" )
								.arg( menuNames.at( i ), -26 )
								.arg( menuBtns.at( i )->width() ).arg( menuBtns.at( i )->height() )
								.arg( a.cols ).arg( a.arrowFirst ).arg( a.inkLast ).arg( a.gap )
								.arg( a.ok ? QString()
										   : QStringLiteral( "   (nothing to compare)" ) ) );
						if ( !a.ok )
							continue;
						read++;
						if ( a.gap < worst ) {
							worst = a.gap;
							who = menuNames.at( i );
						}
					}
					unpinAll( menuBtns, pinned );
					if ( read == 0 )
						worst = -99;
					if ( worstOut ) *worstOut = worst;
					if ( worstName ) *worstName = who;
					if ( readOut ) *readOut = read;
				};

				int worst = 0, read = 0;
				QString who;
				sweep( &worst, &who, &read, true );
				say( *st, QStringLiteral( "  A: %1 menu button(s) in the row, %2 readable; the "
					"skin states padding-right for them and nothing else horizontal" )
					.arg( menuBtns.size() ).arg( read ) );
				check( *st, QStringLiteral( "(A floor) the row carries menu buttons to measure "
					"and both renders differed on them (%1 found, %2 readable)" )
					.arg( menuBtns.size() ).arg( read ),
					menuBtns.size() >= 4 && read >= 4 );
				check( *st, QStringLiteral( "(A1) every menu button's arrow is at least 2 px "
					"clear of its glyph (worst %1 on \"%2\", of %3 read)" )
					.arg( worst ).arg( who ).arg( read ),
					read >= 4 && worst >= 2 );

				/* THE FLOOR THAT FIRES, both halves in one run. res/style.qss
				 * gives every toolbar button `padding: 2px 4px`, and 4 is
				 * exactly what the arrows had before this lane: putting it back
				 * live is the 05:58:21 window, and the SAME sweep must find a
				 * button whose arrow is under 2 px from its glyph. */
				const QString noAir = QStringLiteral(
					"QToolButton[wwHasMenu=\"true\"] { padding-right: 4px; }" );
				QList<QWidget *> victims;
				QStringList victimSheets;
				for ( QWidget * w : barsWithButtons )
					if ( w )
						victims.append( w );
				for ( QToolButton * b : menuBtns )
					victims.append( b );
				for ( QWidget * w : victims )
					victimSheets.append( w->styleSheet() );
				for ( int i = 0; i < victims.size(); i++ )
					victims.at( i )->setStyleSheet( victimSheets.at( i ) + noAir );
				for ( int i = 0; i < 3; i++ )
					QApplication::processEvents();
				int sWorst = 0, sRead = 0;
				QString sWho;
				sweep( &sWorst, &sWho, &sRead, true );
				say( *st, QStringLiteral( "  A floor: with res/style.qss's own 4 px put back "
					"(the 05:58:21 window) the worst is %1 px on \"%2\", of %3 read" )
					.arg( sWorst ).arg( sWho ).arg( sRead ) );
				check( *st, QStringLiteral( "(A1 floor) the SAME test goes red on the sheet that "
					"shipped at 05:58:21 (worst %1 on \"%2\")" ).arg( sWorst ).arg( sWho ),
					sRead >= 4 && sWorst < 2 );

				/* PUTTING THE STRING BACK IS NOT PUTTING THE BOX BACK. Qt keeps
				 * the sabotage's computed box until the widget is re-polished,
				 * so the third sweep of a CORRECT window read every arrow two
				 * pixels left of where it is drawn -- and this floor's restore
				 * half went red on the state the picture below shows. Unpolish
				 * and polish each victim, which is what forces the stylesheet's
				 * geometry to be worked out again. */
				for ( int i = 0; i < victims.size(); i++ ) {
					QWidget * w = victims.at( i );
					w->setStyleSheet( victimSheets.at( i ) );
					w->style()->unpolish( w );
					w->style()->polish( w );
					w->updateGeometry();
				}
				for ( int i = 0; i < 6; i++ )
					QApplication::processEvents();
				int rWorst = 0, rRead = 0;
				QString rWho;
				sweep( &rWorst, &rWho, &rRead, true );
				/* WHAT THIS HALF CAN HONESTLY SAY, and why it is not "worst >= 2".
				 *
				 * arrowGap() pins each button's geometry so its two renders are
				 * comparable. Measured over three runs: after the sabotage is
				 * appended and taken away, three of the narrow buttons come
				 * back 31 px wide where they started at 33 -- the style
				 * re-computes their size hint two pixels smaller once the sheet
				 * has been swapped and restored. The window on screen is not
				 * what this third sweep reads, so asserting the shipped numbers
				 * here was asking the instrument for something it had itself
				 * disturbed. The SHIPPED number is the first sweep's, taken
				 * before anything was appended, and this group now runs after
				 * every picture so nothing else is measured on the disturbed
				 * window. What is left to assert is that the sabotage was
				 * undone, and all three numbers are printed. */
				say( *st, QStringLiteral( "  A floor: shipped worst %1, sabotaged %2, restored "
					"%3 (the restored sweep re-measures buttons this harness pinned, and three "
					"narrow ones come back 2 px smaller)" )
					.arg( worst ).arg( sWorst ).arg( rWorst ) );
				check( *st, QStringLiteral( "(A1 floor) ...and taking it away moves every arrow "
					"back off its glyph (%1 -> %2, shipped %3)" )
					.arg( sWorst ).arg( rWorst ).arg( worst ),
					rRead >= 4 && rWorst > sWorst && rWorst >= 1 );

				/* A2: the rule reached the right buttons and no others. The
				 * property is what the sheet selects on, so a button that has a
				 * menu and does NOT carry it would be missed in silence. */
				int stamped = 0, wrongly = 0, allBtns = 0;
				for ( QWidget * w : barsWithButtons ) {
					if ( !w )
						continue;
					for ( QToolButton * b : w->findChildren<QToolButton *>() ) {
						if ( !b->isVisible()
							 || b->objectName() == QLatin1String( "qt_toolbar_ext_button" ) )
							continue;
						allBtns++;
						const bool prop = b->property( "wwHasMenu" ).toBool();
						if ( hasMenu( b ) && prop )
							stamped++;
						else if ( !hasMenu( b ) && prop )
							wrongly++;
					}
				}
				say( *st, QStringLiteral( "  A2: %1 buttons in the row, %2 with a menu, %3 of "
					"those stamped wwHasMenu, %4 stamped without one" )
					.arg( allBtns ).arg( menuBtns.size() ).arg( stamped ).arg( wrongly ) );
				check( *st, QStringLiteral( "(A2) every button with a menu carries wwHasMenu "
					"(%1 of %2)" ).arg( stamped ).arg( menuBtns.size() ),
					menuBtns.size() >= 4 && stamped == menuBtns.size() );
				check( *st, QStringLiteral( "(A2 floor) ...and no button WITHOUT a menu carries "
					"it (%1 of %2), so the stamp is a measurement and not a blanket" )
					.arg( wrongly ).arg( allBtns - menuBtns.size() ),
					wrongly == 0 && allBtns > menuBtns.size() );
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
