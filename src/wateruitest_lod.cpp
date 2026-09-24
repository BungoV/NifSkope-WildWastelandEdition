/* WW_WATERUI_TEST, group L -- lane WATER8, 2026-09-10.

   THE WATER TOOL IS A TAB OF THE LOD GENERATION PANEL, ON THE RIGHT.

   bungo, on seeing lane WATER7's Water tab in the LEFT strip, verbatim:

       "What? I wanted it in that right panel though"

   read together with what he had said an hour before -- "They should be in the
   LOD gen workspace", then, over a screenshot of the Header | Blocks | Files
   strip, "You'd access them like this". The strip was the STYLE; the LOD
   Generation panel is the PLACE. WATER7 took the second sentence for the
   place, so its whole group T measured the wrong strip. This file is group T
   retired and re-aimed: the same kinds of assertion, on the panel's own strip,
   plus the two that say the left strip went back to what it was.

   WHY A SEPARATE TRANSLATION UNIT. src/wateruitest.cpp belonged to lane UI4
   while this was written (one lane per file, CONSTITUTION 1), so the checks
   live here and wateruitest.cpp calls in with one line -- the same shape every
   other harness in this tree uses for a contended file
   (ww-test-harness-add section 1). Nothing here is armed by its own
   environment variable: it runs inside WW_WATERUI_TEST's single log, so
   tests/spells/water_ui.sh stays the one spell and its count does not drop.

   WHAT IT READS -- WIDGETS by object name, never private members:
     LodPanelModeSelector   QTabBar         the panel's segmented strip
     LodPanelStack          QStackedWidget  its pages
     LodgenPanel            QWidget         page 0, the generator's settings
     WaterMarkPanel         QWidget         page 1, the marking rows
     WaterMarkWindowButton  QToolButton     the door to the full-screen flow window
     LodGenerationDock      QDockWidget     the workspace itself
     LeftColumnModeSelector QTabBar         the left strip, which must be 3 again
     LeftColumnStack        QStackedWidget  and 3 pages
     ViewWorkspacesMenu     QMenu           the menu the water entries left

   FLOORS (a check that cannot fail is not a check, CONSTITUTION 4):
     * L1's search for a tab named "Watre" must find NOTHING;
     * L2's page predicate is asked a SECOND time against a wrong index and
       must go red, printed, in the same run;
     * L3 asserts the LOD tab as well as the Water tab, so a stack that never
       switches and a stack that is stuck on the water page both fail;
     * L5 compares the two sheets and ALSO hashes the compact default, which
       must differ from both, so the comparison is seen to be able to differ;
     * L6 asserts the left strip in BOTH workspace states in one run;
     * L7's scans must still find LodGenerationDock and "LOD Generation";
     * L8 measures the painted air on BOTH strips and prints both.
*/

#include "nifskope.h"
#include "wwskin.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QDockWidget>
#include <QImage>
#include <QMenu>
#include <QPixmap>
#include <QRect>
#include <QStackedWidget>
#include <QTabBar>
#include <QTextStream>
#include <QWidget>

namespace
{

void lodSay( QTextStream & log, const QString & line )
{
	log << line << "\n";
}

void lodCheck( QTextStream & log, int & checks, int & fails, const QString & what, bool pass )
{
	checks++;
	if ( !pass )
		fails++;
	log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
}

void lodSkip( QTextStream & log, int & skips, const QString & why )
{
	skips++;
	log << "  SKIP " << why << "\n";
}

//! The tab whose text is exactly `text`, or -1. Used by L1 and by L1's own floor.
int tabNamed( QTabBar * tabs, const QString & text )
{
	if ( !tabs )
		return -1;
	for ( int i = 0; i < tabs->count(); i++ )
		if ( tabs->tabText( i ) == text )
			return i;
	return -1;
}

//! How many actions of this menu carry exactly this text. L7 and its floor.
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

//! A widget's rectangle in MAIN-WINDOW coordinates -- the only frame two
//! widgets in different parents can be compared in.
QRect rectIn( QWidget * w, QWidget * ref )
{
	if ( !w || !ref || !w->isVisible() )
		return QRect();
	return QRect( w->mapTo( ref, QPoint( 0, 0 ) ), w->size() );
}

QString fmtRect( const QString & name, const QRect & r )
{
	if ( r.isNull() )
		return QStringLiteral( "  %1: <absent or hidden>" ).arg( name, -28 );
	return QStringLiteral( "  %1: x %2  top %3  w %4  h %5  bottom %6" )
		.arg( name, -28 ).arg( r.x(), 4 ).arg( r.top(), 4 )
		.arg( r.width(), 5 ).arg( r.height(), 3 ).arg( r.bottom(), 4 );
}

/*! FNV-1a over the sheet's UTF-8 bytes. A hash and not a `==` because the log
 *  has to SAY which two things were compared when they differ, and a 30-line
 *  stylesheet printed twice is unreadable; the length is printed beside it so
 *  a collision would have to match both. */
quint32 fnv1a( const QString & s )
{
	quint32 h = 2166136261u;
	for ( char c : s.toUtf8() ) {
		h ^= quint8( c );
		h *= 16777619u;
	}
	return h;
}

/*! THE PAINTED BOX of one segment of a strip, in the tab bar's own
 *  coordinates -- or a null rect if no pixel of `want` is on it.
 *
 *  The same instrument lane UI4 registered for the left strip, and it is a
 *  COPY rather than a call because src/wateruitest.cpp, where UI4 put it,
 *  belonged to that lane while this was written. It is here for the same
 *  reason it is there: QSS margins on QTabBar::tab ARE honoured, and
 *  tabRect() RETURNS THE RECT INCLUDING THE MARGIN, so every rect the strip
 *  can be asked for is identical with air and without it. The air exists only
 *  in what is drawn, so what is drawn is measured -- the segment is selected
 *  (its fill and its border are both `selBgActive`, the one colour nothing
 *  else in the strip carries) and the bounding box of that colour IS the box
 *  the user sees.
 */
QRect paintedSegment( QTabBar * tabs, int index, const QColor & want )
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

//! Is this widget on screen inside `page`, i.e. would the user see it?
bool shownIn( QWidget * page, QWidget * w )
{
	return page && w && w->isVisible() && page->isAncestorOf( w );
}

} // namespace

/*! Group L. Called from wwWaterUiHarness (src/wateruitest.cpp) with its log
 *  and its running counts, so the whole run stays one verdict and one file. */
void wwWaterUiLodTabs( NifSkope * skope, QTextStream & log, int & checks, int & fails,
	int & skips, const QString & lodShot, const QString & waterShot )
{
	if ( !skope )
		return;

	lodSay( log, QStringLiteral( "--- L: the Water tab, in the LOD Generation panel ---" ) );

	auto * lodDock = skope->findChild<QDockWidget *>( QStringLiteral( "LodGenerationDock" ) );
	auto * tabs = skope->findChild<QTabBar *>( QStringLiteral( "LodPanelModeSelector" ) );
	auto * stack = skope->findChild<QStackedWidget *>( QStringLiteral( "LodPanelStack" ) );
	auto * leftTabs = skope->findChild<QTabBar *>( QStringLiteral( "LeftColumnModeSelector" ) );
	auto * leftStack = skope->findChild<QStackedWidget *>( QStringLiteral( "LeftColumnStack" ) );
	auto * wsMenu = skope->findChild<QMenu *>( QStringLiteral( "ViewWorkspacesMenu" ) );

	lodCheck( log, checks, fails,
		QStringLiteral( "(L floor) the LOD Generation panel's strip and page stack exist" ),
		tabs != nullptr && stack != nullptr );

	/* A harness FORCES the state it measures (ww-test-harness-add section 6),
	 * and a stacked page is only LAID OUT while it is current, so nothing
	 * below is meaningful until the workspace is open and the events are
	 * drained. */
	if ( lodDock ) {
		lodDock->show();
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
	}

	// =========================================================
	//  L1 -- two tabs, LOD then Water
	// =========================================================
	const int lodTab = tabNamed( tabs, QStringLiteral( "LOD" ) );
	const int waterTab = tabNamed( tabs, QStringLiteral( "Water" ) );
	QString strip;
	if ( tabs )
		for ( int t = 0; t < tabs->count(); t++ )
			strip += QStringLiteral( " %1=%2" ).arg( tabs->tabText( t ) )
				.arg( tabs->tabData( t ).toInt() );
	lodSay( log, QStringLiteral( "  the LOD panel's strip carries %1 tabs; tab -> page:%2" )
		.arg( tabs ? tabs->count() : -1 ).arg( strip ) );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L1) the LOD panel's strip carries exactly 2 tabs (%1)" )
			.arg( tabs ? tabs->count() : -1 ),
		tabs != nullptr && tabs->count() == 2 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L1) they are \"LOD\" at %1 then \"Water\" at %2" )
			.arg( lodTab ).arg( waterTab ),
		lodTab == 0 && waterTab == 1 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L1 floor) ...and the same search finds no \"Watre\", so it is a "
			"real search" ),
		tabNamed( tabs, QStringLiteral( "Watre" ) ) < 0 );

	// =========================================================
	//  L2 -- the pages, and what the tabs map to
	// =========================================================
	auto * lodPage = skope->findChild<QWidget *>( QStringLiteral( "LodgenPanel" ) );
	auto * waterPage = skope->findChild<QWidget *>( QStringLiteral( "WaterMarkPanel" ) );
	const int lodIndex = ( stack && lodPage ) ? stack->indexOf( lodPage ) : -1;
	const int waterIndex = ( stack && waterPage ) ? stack->indexOf( waterPage ) : -1;
	lodSay( log, QStringLiteral( "  the stack holds %1 pages: LodgenPanel at %2, "
		"WaterMarkPanel at %3" ).arg( stack ? stack->count() : -1 )
		.arg( lodIndex ).arg( waterIndex ) );
	const bool pagesRight = stack != nullptr && stack->count() == 2
		&& lodIndex == 0 && waterIndex == 1;
	lodCheck( log, checks, fails,
		QStringLiteral( "(L2) the stack carries the generator's page at 0 and the water page "
			"at 1, and nothing else" ), pagesRight );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L2) each tab's data is its page index (%1 -> %2, %3 -> %4)" )
			.arg( lodTab ).arg( lodTab >= 0 && tabs ? tabs->tabData( lodTab ).toInt() : -1 )
			.arg( waterTab ).arg( waterTab >= 0 && tabs ? tabs->tabData( waterTab ).toInt() : -1 ),
		tabs != nullptr && lodTab >= 0 && waterTab >= 0
			&& tabs->tabData( lodTab ).toInt() == lodIndex
			&& tabs->tabData( waterTab ).toInt() == waterIndex );
	/* THE FLOOR THAT FIRES, in the same run, on the same predicate: the water
	 * page is asked for at index 0, where the generator's page is. If this
	 * came back true the check above would be passing on something other than
	 * the index. */
	const bool wrongOnPurpose = stack != nullptr && waterPage != nullptr
		&& stack->indexOf( waterPage ) == 0;
	lodCheck( log, checks, fails,
		QStringLiteral( "(L2 floor) the SAME predicate asked against the wrong index (water "
			"page at 0) is false, so it is reading the index and not merely existing" ),
		!wrongOnPurpose );

	// =========================================================
	//  L3 -- selecting Water actually shows the marking rows
	// =========================================================
	auto * windowButton = skope->findChild<QWidget *>( QStringLiteral( "WaterMarkWindowButton" ) );
	auto * solveButton = skope->findChild<QWidget *>( QStringLiteral( "WaterMarkSolveButton" ) );
	auto * saveButton = skope->findChild<QWidget *>( QStringLiteral( "WaterMarkSaveButton" ) );
	auto * toolBox = skope->findChild<QWidget *>( QStringLiteral( "WaterMarkToolBox" ) );
	auto * generateButton = skope->findChild<QWidget *>( QStringLiteral( "LodgenGenerateButton" ) );

	if ( !tabs || waterTab < 0 || lodTab < 0 || !stack ) {
		lodSkip( log, skips, QStringLiteral( "L3 needs both tabs and the stack; one is absent" ) );
	} else {
		tabs->setCurrentIndex( waterTab );
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
		const int onWater = stack->currentIndex();
		const bool rowsShown = shownIn( waterPage, toolBox ) && shownIn( waterPage, solveButton )
			&& shownIn( waterPage, saveButton );
		const bool doorShown = shownIn( waterPage, windowButton );
		lodSay( log, QStringLiteral( "  L3: Water selected -> page %1 (want %2); rows on screen "
			"%3, the flow-window button %4" ).arg( onWater ).arg( waterIndex )
			.arg( rowsShown ? QStringLiteral( "yes" ) : QStringLiteral( "no" ),
				  doorShown ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ) );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L3) selecting Water shows the water page (%1, wanted %2)" )
				.arg( onWater ).arg( waterIndex ),
			onWater >= 0 && onWater == waterIndex );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L3) ...and the marking rows are on screen -- the tool box, Solve "
				"and Save all visible" ), rowsShown );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L3) ...and the full-screen flow window's button with them" ),
			doorShown );

		if ( !waterShot.isEmpty() && lodDock ) {
			skope->resizeDocks( { lodDock }, { 660 }, Qt::Horizontal );
			for ( int i = 0; i < 3; i++ )
				QApplication::processEvents();
			const bool wrote = lodDock->grab().save( waterShot );
			lodSay( log, QStringLiteral( "  LOD-dock grab, Water selected -> %1 (%2)" )
				.arg( waterShot, wrote ? QStringLiteral( "written" )
									   : QStringLiteral( "FAILED" ) ) );
			lodCheck( log, checks, fails,
				QStringLiteral( "(shot) the Water-tab grab of the LOD panel was written" ),
				wrote );
		}

		// the floor: the same assertion on the tab that was always there
		tabs->setCurrentIndex( lodTab );
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
		const int onLod = stack->currentIndex();
		lodCheck( log, checks, fails,
			QStringLiteral( "(L3 floor) selecting LOD puts the generator's page back (%1, "
				"wanted %2) with its Generate button on screen -- the same assertion, on the "
				"tab that has always been there" ).arg( onLod ).arg( lodIndex ),
			onLod == lodIndex && shownIn( lodPage, generateButton ) );

		if ( !lodShot.isEmpty() && lodDock ) {
			const bool wrote = lodDock->grab().save( lodShot );
			lodSay( log, QStringLiteral( "  LOD-dock grab, LOD selected -> %1 (%2)" )
				.arg( lodShot, wrote ? QStringLiteral( "written" )
									 : QStringLiteral( "FAILED" ) ) );
			lodCheck( log, checks, fails,
				QStringLiteral( "(shot) the LOD-tab grab of the LOD panel was written" ), wrote );
		}
	}

	// =========================================================
	//  L4 -- the panel's strip is the SAME ROW as the left strip
	// =========================================================
	const QRect lodStripRect = rectIn( tabs, skope );
	const QRect leftStripRect = rectIn( leftTabs, skope );
	const int row = wwBarRowHeight();
	lodSay( log, QStringLiteral( "  wwBarRowHeight() = %1" ).arg( row ) );
	lodSay( log, fmtRect( QStringLiteral( "LodPanelModeSelector" ), lodStripRect ) );
	lodSay( log, fmtRect( QStringLiteral( "LeftColumnModeSelector" ), leftStripRect ) );
	if ( row <= 0 ) {
		lodSkip( log, skips, QStringLiteral( "L4 needs the shared bar row; wwBarRowHeight() is "
			"0, which is UI/CompactTopBars off or wwAlignBarRow never called" ) );
	} else {
		lodCheck( log, checks, fails,
			QStringLiteral( "(L4) the LOD panel's strip is the shared row's height (%1 vs %2)" )
				.arg( lodStripRect.height() ).arg( row ),
			!lodStripRect.isNull() && qAbs( lodStripRect.height() - row ) <= 1 );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L4) ...the same height as the left strip (%1 vs %2)" )
				.arg( lodStripRect.height() ).arg( leftStripRect.height() ),
			!lodStripRect.isNull() && !leftStripRect.isNull()
				&& qAbs( lodStripRect.height() - leftStripRect.height() ) <= 1 );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L4 floor) both rectangles are non-degenerate" ),
			lodStripRect.width() > 0 && lodStripRect.height() > 0
				&& leftStripRect.width() > 0 && leftStripRect.height() > 0 );
	}

	// =========================================================
	//  L5 -- and the SAME SHEET, byte for byte
	// =========================================================
	const QString lodSheet = tabs ? tabs->styleSheet() : QString();
	const QString leftSheet = leftTabs ? leftTabs->styleSheet() : QString();
	const QString compact = wwSegmentedTabBarQss();
	lodSay( log, QStringLiteral( "  sheets: LOD %1 chars / %2, left %3 chars / %4, the compact "
		"default %5 chars / %6" )
		.arg( lodSheet.size() ).arg( fnv1a( lodSheet ), 8, 16, QLatin1Char( '0' ) )
		.arg( leftSheet.size() ).arg( fnv1a( leftSheet ), 8, 16, QLatin1Char( '0' ) )
		.arg( compact.size() ).arg( fnv1a( compact ), 8, 16, QLatin1Char( '0' ) ) );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L5) the two strips carry the SAME stylesheet, byte for byte" ),
		!lodSheet.isEmpty() && lodSheet == leftSheet );
	if ( row <= 0 ) {
		lodSkip( log, skips, QStringLiteral( "L5's floor compares the row sheet with the "
			"compact default, and there is no row (wwBarRowHeight() is 0)" ) );
	} else {
		/* The floor is that the thing being compared is not a constant: the
		 * sheet the strips carry is the ROW sheet, and the compact default --
		 * the same helper, asked for no row -- must be a different string. If
		 * these were equal, L5 above would be passing on two empty or two
		 * default sheets and would say nothing about the row at all. */
		lodCheck( log, checks, fails,
			QStringLiteral( "(L5 floor) ...and the compact default (%1 chars) is a DIFFERENT "
				"string from the row sheet (%2 chars), so the comparison is able to differ" )
				.arg( compact.size() ).arg( lodSheet.size() ),
			!lodSheet.isEmpty() && compact != lodSheet );
	}

	// =========================================================
	//  L6 -- the LEFT strip went back to three, in both workspace states
	// =========================================================
	auto leftCount = [leftTabs]() {
		int visible = 0;
		if ( leftTabs )
			for ( int t = 0; t < leftTabs->count(); t++ )
				if ( leftTabs->isTabVisible( t ) )
					visible++;
		return visible;
	};
	QString leftNames;
	if ( leftTabs )
		for ( int t = 0; t < leftTabs->count(); t++ )
			leftNames += QStringLiteral( " %1" ).arg( leftTabs->tabText( t ) );
	const int openCount = leftCount();
	if ( lodDock ) {
		lodDock->hide();
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
	}
	const int shutCount = leftCount();
	if ( lodDock ) {
		lodDock->show();
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
	}
	lodSay( log, QStringLiteral( "  the left strip:%1 -- %2 visible with the LOD workspace "
		"open, %3 with it closed; LeftColumnStack has %4 pages" )
		.arg( leftNames ).arg( openCount ).arg( shutCount )
		.arg( leftStack ? leftStack->count() : -1 ) );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L6) the left strip has exactly 3 tabs with the LOD workspace OPEN "
			"(%1)" ).arg( openCount ), openCount == 3 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L6) ...and exactly 3 with it CLOSED (%1) -- both states in one run, "
			"so a strip that is 3 only in one of them goes red" ).arg( shutCount ),
		shutCount == 3 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L6) no tab named \"Water\" is left in the left strip" ),
		tabNamed( leftTabs, QStringLiteral( "Water" ) ) < 0 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L6) LeftColumnStack is back to 3 pages (%1)" )
			.arg( leftStack ? leftStack->count() : -1 ),
		leftStack != nullptr && leftStack->count() == 3 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L6 floor) ...and the three it still has are Header, Blocks and Files" ),
		tabNamed( leftTabs, QStringLiteral( "Header" ) ) >= 0
			&& tabNamed( leftTabs, QStringLiteral( "Blocks" ) ) >= 0
			&& tabNamed( leftTabs, QStringLiteral( "Files" ) ) >= 0 );

	// =========================================================
	//  L7 -- no dock, no menu entries (unchanged rules, kept from group T)
	// =========================================================
	const int nMarking = menuEntries( wsMenu, QStringLiteral( "Water Marking" ) );
	const int nWindow = menuEntries( wsMenu, QStringLiteral( "Water window" ) );
	const int nLodGen = menuEntries( wsMenu, QStringLiteral( "LOD Generation" ) );
	lodSay( log, QStringLiteral( "  the Workspaces menu holds %1 actions: Water Marking %2, "
		"Water window %3, LOD Generation %4" )
		.arg( wsMenu ? wsMenu->actions().size() : -1 )
		.arg( nMarking ).arg( nWindow ).arg( nLodGen ) );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L7 floor) the scan finds \"LOD Generation\" (%1), so it is able to "
			"find something" ).arg( nLodGen ), nLodGen >= 1 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L7) the Workspaces menu offers no \"Water Marking\" (%1)" )
			.arg( nMarking ), nMarking == 0 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L7) ...and no \"Water window\" (%1)" ).arg( nWindow ), nWindow == 0 );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L7) there is no Water Marking dock left in the window" ),
		skope->findChild<QDockWidget *>( QStringLiteral( "WaterMarkDock" ) ) == nullptr );
	lodCheck( log, checks, fails,
		QStringLiteral( "(L7 floor) ...and the same search DOES find the LOD Generation dock" ),
		lodDock != nullptr );

	// =========================================================
	//  L8 -- the four pixels of air, painted, on BOTH strips
	// =========================================================
	const int air = wwSegmentedStripAir();
	const QColor sel( wwSkinColor( "selBgActive" ) );
	if ( row <= 0 || !tabs || !leftTabs || !sel.isValid() ) {
		lodSkip( log, skips, QStringLiteral( "L8 needs the shared row, both strips and the "
			"selection colour" ) );
	} else {
		const QRect lodSeg = paintedSegment( tabs, 0, sel );
		const QRect leftSeg = paintedSegment( leftTabs, 0, sel );
		lodSay( log, QStringLiteral( "  wwSegmentedStripAir() = %1; painted segment 0: LOD "
			"strip top %2 bottom %3 (h %4), left strip top %5 bottom %6 (h %7), both in a "
			"row of %8" ).arg( air )
			.arg( lodSeg.top() ).arg( lodSeg.bottom() ).arg( lodSeg.height() )
			.arg( leftSeg.top() ).arg( leftSeg.bottom() ).arg( leftSeg.height() )
			.arg( row ) );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L8) the LOD strip's first segment is %1 px clear of the row's "
				"top (%2) and bottom (%3)" ).arg( air ).arg( lodSeg.top() )
				.arg( row - 1 - lodSeg.bottom() ),
			!lodSeg.isNull() && qAbs( lodSeg.top() - air ) <= 1
				&& qAbs( ( row - 1 - lodSeg.bottom() ) - air ) <= 1 );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L8 floor) ...and the LEFT strip reads the same two numbers, so "
				"this is the air the whole window agreed on and not one strip's accident" ),
			!leftSeg.isNull() && qAbs( leftSeg.top() - lodSeg.top() ) <= 1
				&& qAbs( leftSeg.bottom() - lodSeg.bottom() ) <= 1 );
		/* L8's MISSING HALF, added by lane UI6.
		 *
		 * Lane WATER8 registered L8 as "4 px clear of the row's top and bottom
		 * AND OF ONE ANOTHER" and shipped only the vertical half: nothing
		 * measured the gap between this strip's two segments, so the very
		 * number bungo's "Why are they separated?" sends to 0 was unmeasured on
		 * the right-hand strip. WATER8's own report said so and handed it here.
		 * Same instrument as the LEFT strip's group S: PAINTED boxes, never
		 * tabRect(), which includes the margin and reads the same either way. */
		const QRect lodSeg1 = paintedSegment( tabs, 1, sel );
		auto gapOf = []( const QRect & a, const QRect & b ) {
			return ( a.isNull() || b.isNull() ) ? -99 : b.left() - a.right() - 1;
		};
		const int lodGap = gapOf( lodSeg, lodSeg1 );
		lodSay( log, QStringLiteral( "  the LOD strip's two segments: 0 painted x %1 w %2, "
			"1 painted x %3 w %4 -> %5 px apart (want 0)" )
			.arg( lodSeg.x() ).arg( lodSeg.width() ).arg( lodSeg1.x() ).arg( lodSeg1.width() )
			.arg( lodGap ) );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L8) the LOD strip's two segments TOUCH (%1 px apart, want 0)" )
				.arg( lodGap ), lodGap == 0 );

		/* AND ITS FLOOR, both halves in the same run: UI4's separated sheet --
		 * a left margin on every segment, its own border, its own rounded
		 * corners -- appended live over the shipped one. The same predicate
		 * must read the air back and go red, then read 0 again. */
		const QString lodSaved = tabs->styleSheet();
		const QString apartQss = QStringLiteral(
			"QTabBar::tab { margin-left: %1px; border-left: 1px solid %2;"
			" border-radius: 3px; }" ).arg( qMax( 1, air ) ).arg( wwSkinColor( "border" ) );
		tabs->setStyleSheet( lodSaved + apartQss );
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
		const int sepGap = gapOf( paintedSegment( tabs, 0, sel ),
								  paintedSegment( tabs, 1, sel ) );
		lodSay( log, QStringLiteral( "  L8 floor: with UI4's separated strip put back, the two "
			"segments read %1 px apart" ).arg( sepGap ) );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L8 floor) the SAME gap test goes red on a separated strip "
				"(%1 px apart)" ).arg( sepGap ), sepGap != 0 );

		tabs->setStyleSheet( lodSaved );
		for ( int i = 0; i < 3; i++ )
			QApplication::processEvents();
		const int backGap = gapOf( paintedSegment( tabs, 0, sel ),
								   paintedSegment( tabs, 1, sel ) );
		lodSay( log, QStringLiteral( "  L8 floor: restored, the two segments read %1 px apart "
			"again" ).arg( backGap ) );
		lodCheck( log, checks, fails,
			QStringLiteral( "(L8 floor) ...and taking it away joins them again (%1 px apart)" )
				.arg( backGap ), backGap == 0 );

		// leave the strip where a reader expects it
		tabs->setCurrentIndex( 0 );
		QApplication::processEvents();
	}
}
