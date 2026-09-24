/* Lane UI5 -- where a QMenuBar puts its items inside a 35 px row, and what
 * moves them, measured OUTSIDE the application (skill ww-qss-geometry-probe).
 *
 * CASE 0 IS THE SHIPPED STATE and has to reproduce what the 20:45:47 window
 * actually shows. The application cannot be asked (bungo has it open, one
 * instance ever), but it already answered once: lane UI4's in-application grab
 * scratchpad/ui4_20260910/images/strip_after.png starts at the window's own
 * (0,0), and measure_before.py reads the five items' text ink out of its first
 * 35 rows:
 *
 *     File 6..16  View 6..16  Spells 6..17  Options 6..17  Help 6..17
 *     ink centre 11.0 / 11.5 against a row centre of 17.0  ->  6 PX HIGH
 *     (the four TOOL BUTTONS in the same row read +0.0 / +1.0 -- centred)
 *
 * so case 0 must print an ink centre of 11 for File and View.
 *
 * WHAT IS MEASURED, three ways, because two of them can lie:
 *   - actionGeometry(): the item's rect. QSS margins on ::item go through
 *     QRenderRule::boxSize, so a rect INCLUDES its margin exactly as
 *     QTabBar::tabRect() does (lane UI4) -- a fix written as a margin cannot be
 *     seen here at all.
 *   - the PAINTED item box: a marker sheet paints every ::item's background in
 *     a colour nothing else carries, and the bounding box of that colour is the
 *     box the user sees. The probe checks the marker does not move the rect.
 *   - the TEXT INK: pixels far brighter than the bar's background. This is what
 *     bungo looked at, and it is what the gate reads.
 *
 * Build: bash scratchpad/ui5_20260910/build_probe.sh   (MSYS2 UCRT64)
 * Run  : ./release/ui5_probe.exe -platform offscreen   (from the repo root)
 */

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QRegularExpression>
#include <QStyle>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>

// ---------------------------------------------------------------- the palette
// The dark column of skinVars[] (src/nifskope_ui.cpp:299). release/style.qss is
// a BYTE COPY of res/style.qss -- the ${name} tokens are NOT substituted on
// disk, the application substitutes them at load, and a probe that feeds the
// file to setStyleSheet raw silently loses every rule carrying a colour
// (lane UI4's first probe measured a separator the application does not have).
static const struct { const char * name; const char * dark; } vars[] = {
	{ "bg", "#303236" }, { "bgWin", "#292b2f" }, { "bgBar", "#25272a" },
	{ "bgPanel", "#27292d" }, { "bgAlt", "#2d3034" }, { "bgCard", "#2b2d31" },
	{ "bgInput", "#3c3f44" }, { "bgBtn", "#3a3d42" }, { "bgBtnHover", "#484c52" },
	{ "bgBtnDown", "#355f86" }, { "bgHeader", "#2c2f33" }, { "border", "#4d5056" },
	{ "borderDim", "#3a3d42" }, { "borderStrong", "#1b1c1f" }, { "focus", "#5d92c5" },
	{ "scroll", "#62666c" }, { "scrollHover", "#777c83" }, { "text", "#e6e8eb" },
	{ "textMuted", "#aeb3ba" }, { "textBright", "#f2f3f5" }, { "accent", "#f0a54a" },
	{ "accentText", "#ffb54a" }, { "accentBg", "#40331f" }, { "textDisabled", "#6b7076" },
	{ "accentDisabled", "#8a6a3f" }, { "toggle", "#4772b3" },
	{ "toggleDisabled", "#3b4d68" }, { "danger", "#ff8484" }, { "viewport", "#2b2d31" },
	{ "selBgActive", "#4a7ab0" }, { "selBgInactive", "#2b425f" },
	{ "selTextActive", "#ff9d00" }, { "selTextInactive", "#ff7200" },
};

static QString substitute( QString s )
{
	QRegularExpression cssComment( QStringLiteral( R"regex(\/\*[^*]*\*+([^/*][^*]*\*+)*\/)regex" ) );
	s.replace( cssComment, QString() );
	s.replace( QStringLiteral( "${theme}" ), QStringLiteral( "dark" ) );
	for ( const auto & v : vars )
		s.replace( QStringLiteral( "${" ) + QLatin1String( v.name ) + QStringLiteral( "}" ),
			QLatin1String( v.dark ) );
	s.replace( QStringLiteral( "${rgb}" ), QStringLiteral( "74, 122, 176" ) );
	return s;
}

// ------------------------------------------------------- the sheets under test
static const int rowH = 35;
static const int pad = 2;   // wwBarRowPad

//! wwBarRowBoxQss() as it ships (src/nifskope_ui.cpp:733).
static QString boxQss()
{
	return QStringLiteral( "QToolBar { border: 0px; padding: 0px; margin: 0px; }" );
}

//! wwBarRowButtonQss( content ) as it ships (src/nifskope_ui.cpp:743).
static QString btnQss( int content, int menuContent = -1 )
{
	if ( menuContent < 0 )
		menuContent = content;
	return QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %3px; padding-bottom: %3px; }"
		"QToolButton { min-height: %2px; padding-top: %3px; padding-bottom: %3px; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" )
		.arg( menuContent ).arg( content ).arg( pad );
}

struct Rig
{
	QMainWindow * win = nullptr;
	QMenuBar * bar = nullptr;
	QList<QAction *> items;
};

static void settle()
{
	for ( int i = 0; i < 6; i++ )
		QApplication::processEvents();
}

static Rig buildRig()
{
	Rig r;
	r.win = new QMainWindow;
	r.win->resize( 1512, 400 );
	r.bar = new QMenuBar( r.win );
	r.bar->setObjectName( QStringLiteral( "menubar" ) );
	r.bar->setNativeMenuBar( false );
	for ( const char * name : { "&File", "&View", "&Spells", "&Options", "&Help" } ) {
		QMenu * m = r.bar->addMenu( QString::fromLatin1( name ) );
		m->addAction( QStringLiteral( "an entry" ) );
		r.items.append( m->menuAction() );
	}
	r.win->setMenuBar( r.bar );
	auto * central = new QWidget( r.win );
	r.win->setCentralWidget( central );
	return r;
}

/*! Bounding box of every pixel of `want` in the bar's own grab, or a null rect.
 *  Logical pixels, whatever the display ratio. */
static QRect paintedBox( QMenuBar * bar, QRgb want, int x0lim = 0, int x1lim = 1 << 20 )
{
	const QPixmap pm = bar->grab();
	if ( pm.isNull() )
		return QRect();
	const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
	const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
	const QRgb rgb = want & 0x00ffffff;
	int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			const int lx = qRound( x / dpr );
			if ( lx < x0lim || lx > x1lim )
				continue;
			if ( ( img.pixel( x, y ) & 0x00ffffff ) != rgb )
				continue;
			x0 = qMin( x0, lx ); x1 = qMax( x1, lx );
			y0 = qMin( y0, qRound( y / dpr ) ); y1 = qMax( y1, qRound( y / dpr ) );
		}
	}
	if ( x1 < 0 )
		return QRect();
	return QRect( QPoint( x0, y0 ), QPoint( x1, y1 ) );
}

//! The TEXT INK of the item spanning [x0,x1]: pixels much brighter than the bar.
static QRect inkBox( QMenuBar * bar, int x0lim, int x1lim )
{
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
		if ( it.value() > best ) { best = it.value(); bg = it.key(); }
	auto lum = []( QRgb c ) {
		return 0.299 * qRed( c ) + 0.587 * qGreen( c ) + 0.114 * qBlue( c );
	};
	const double thresh = lum( bg ) + 40.0;
	int y0 = 1 << 20, y1 = -1, ix0 = 1 << 20, ix1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			const int lx = qRound( x / dpr );
			if ( lx < x0lim || lx > x1lim )
				continue;
			if ( lum( img.pixel( x, y ) ) <= thresh )
				continue;
			y0 = qMin( y0, qRound( y / dpr ) ); y1 = qMax( y1, qRound( y / dpr ) );
			ix0 = qMin( ix0, lx ); ix1 = qMax( ix1, lx );
		}
	}
	if ( y1 < 0 )
		return QRect();
	return QRect( QPoint( ix0, y0 ), QPoint( ix1, y1 ) );
}

static void report( QTextStream & o, const QString & title, Rig & r )
{
	settle();
	const double centre = ( r.bar->height() - 1 ) / 2.0;
	o << "=== " << title << "\n";
	o << QStringLiteral( "  menu bar %1x%2   PM_MenuBarPanelWidth %3  PM_MenuBarVMargin %4"
		"  PM_MenuBarHMargin %5\n" )
		.arg( r.bar->width() ).arg( r.bar->height() )
		.arg( r.bar->style()->pixelMetric( QStyle::PM_MenuBarPanelWidth, nullptr, r.bar ) )
		.arg( r.bar->style()->pixelMetric( QStyle::PM_MenuBarVMargin, nullptr, r.bar ) )
		.arg( r.bar->style()->pixelMetric( QStyle::PM_MenuBarHMargin, nullptr, r.bar ) );
	/* THE REFUTER (lane UI5's brief). wwAlignBarRow takes the TALLEST bar's
	 * sizeHint/minimumSizeHint as the row height. A candidate that pushes the
	 * menu bar's own hint past 35 would make the WHOLE row grow the next time
	 * the row is aligned, so the hint is printed for every case and a case that
	 * exceeds the row does not ship however good its ink offset is.
	 * Measured with the min/max pin LIFTED, because that is the state
	 * wwAlignBarRow measures in (it aligns before it pins). */
	{
		const int pinMin = r.bar->minimumHeight(), pinMax = r.bar->maximumHeight();
		r.bar->setMinimumHeight( 0 );
		r.bar->setMaximumHeight( QWIDGETSIZE_MAX );
		r.bar->ensurePolished();
		const int sh = r.bar->sizeHint().height();
		const int msh = r.bar->minimumSizeHint().height();
		r.bar->setMinimumHeight( pinMin );
		r.bar->setMaximumHeight( pinMax );
		o << QStringLiteral( "  REFUTER: menu bar sizeHint h %1, minimumSizeHint h %2"
			" (row %3) -> %4\n" )
			.arg( sh, 3 ).arg( msh, 3 ).arg( rowH )
			.arg( qMax( sh, msh ) > rowH
				? QStringLiteral( "*** WOULD GROW THE ROW, does not ship ***" )
				: QStringLiteral( "row unchanged" ) );
	}

	// the marker pass: every ::item paints its own box in a colour nothing else
	// carries. It must not move the rects -- checked, not assumed.
	const QString saved = r.bar->styleSheet();
	QList<QRect> rects;
	for ( QAction * a : r.items )
		rects.append( r.bar->actionGeometry( a ) );
	r.bar->setStyleSheet( saved + QStringLiteral( "QMenuBar::item { background: #ff00ff; }" ) );
	settle();
	bool moved = false;
	for ( int i = 0; i < r.items.size(); i++ )
		if ( r.bar->actionGeometry( r.items.at( i ) ) != rects.at( i ) )
			moved = true;
	QList<QRect> boxes;
	for ( int i = 0; i < r.items.size(); i++ )
		boxes.append( paintedBox( r.bar, qRgb( 255, 0, 255 ),
			rects.at( i ).left(), rects.at( i ).right() ) );
	r.bar->setStyleSheet( saved );
	settle();

	double worstInk = 0.0, worstBox = 0.0;
	for ( int i = 0; i < r.items.size(); i++ ) {
		const QRect g = rects.at( i );
		const QRect b = boxes.at( i );
		const QRect ink = inkBox( r.bar, g.left(), g.right() );
		const double bc = b.isNull() ? -99 : ( b.top() + b.bottom() ) / 2.0;
		const double ic = ink.isNull() ? -99 : ( ink.top() + ink.bottom() ) / 2.0;
		if ( !b.isNull() && qAbs( bc - centre ) > qAbs( worstBox ) )
			worstBox = bc - centre;
		if ( !ink.isNull() && qAbs( ic - centre ) > qAbs( worstInk ) )
			worstInk = ic - centre;
		o << QStringLiteral( "  item %1: rect x %2 y %3 w %4 h %5 | painted y %6..%7 (c %8, "
			"%9) | ink y %10..%11 (c %12, %13)\n" )
			.arg( i ).arg( g.x(), 3 ).arg( g.y(), 3 ).arg( g.width(), 3 ).arg( g.height(), 3 )
			.arg( b.isNull() ? -1 : b.top(), 3 ).arg( b.isNull() ? -1 : b.bottom(), 3 )
			.arg( bc, 5, 'f', 1 ).arg( bc - centre, 5, 'f', 1 )
			.arg( ink.isNull() ? -1 : ink.top(), 3 ).arg( ink.isNull() ? -1 : ink.bottom(), 3 )
			.arg( ic, 5, 'f', 1 ).arg( ic - centre, 5, 'f', 1 );
	}
	o << QStringLiteral( "  ROW centre %1 -- worst painted-box offset %2, worst ink offset %3"
		"%4\n" )
		.arg( centre, 0, 'f', 1 ).arg( worstBox, 0, 'f', 1 ).arg( worstInk, 0, 'f', 1 )
		.arg( moved ? QStringLiteral( "   *** the marker sheet MOVED the rects ***" )
					: QStringLiteral( "   (the marker sheet moved no rect)" ) );
	o.flush();
}

static void runCase( QTextStream & o, const QString & title, const QString & sheet )
{
	Rig r = buildRig();
	r.win->show();
	settle();
	// wwAlignBarRow's own pinning, then its sheets
	r.bar->setMinimumHeight( rowH );
	r.bar->setMaximumHeight( rowH );
	r.bar->setStyleSheet( sheet );
	report( o, title, r );
	r.win->hide();
	delete r.win;
}

int main( int argc, char ** argv )
{
	QApplication app( argc, argv );
	QFile qss( QStringLiteral( "release/style.qss" ) );
	if ( qss.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		app.setStyleSheet( substitute( QString::fromUtf8( qss.readAll() ) ) );
	} else {
		fprintf( stderr, "WARNING: release/style.qss not read -- run from the repo root\n" );
	}

	QTextStream o( stdout );
	o << QStringLiteral( "qApp style \"%1\"\n" ).arg( QApplication::style()->objectName() );

	// case 0 -- the state that shipped at 20:45:47. Must read ink centre 11.
	runCase( o, QStringLiteral( "case 0 -- SHIPPED (content 26, pad 2) -- want ink centre 11.0" ),
		boxQss() + btnQss( 26 ) );

	// case 1 -- no row sheet at all, for the natural item height
	runCase( o, QStringLiteral( "case 1 -- no row sheet (the item's natural height)" ),
		QString() );

	// A: the menu item's min-height alone -- the map of item height against it
	for ( int mh = 22; mh <= 36; mh += 2 )
		runCase( o, QStringLiteral( "case A%1 -- QMenuBar::item min-height %1, pad 2" ).arg( mh ),
			boxQss() + btnQss( 26, mh ) );

	// B: and with the bar's own box zeroed as well, in case the panel/vmargin
	//    is what holds the items at the top
	for ( int mh = 28; mh <= 36; mh += 2 )
		runCase( o, QStringLiteral( "case B%1 -- min-height %1 + QMenuBar box zeroed" ).arg( mh ),
			boxQss() + btnQss( 26, mh )
				+ QStringLiteral( "QMenuBar { padding: 0px; margin: 0px; border: 0px; }" ) );

	// C: the margin route -- does a ::item margin move the item, and does the
	//    rect lie about it the way tabRect() does?
	for ( int mt = 0; mt <= 8; mt += 2 )
		runCase( o, QStringLiteral( "case C%1 -- ::item margin-top %1 (rect vs paint)" ).arg( mt ),
			boxQss() + btnQss( 26 )
				+ QStringLiteral( "QMenuBar::item { margin-top: %1px; margin-bottom: 0px; }" )
					.arg( mt ) );

	/* D: THE PADDING ROUTE, which is the one the code takes.
	 *
	 * min-height is not consulted on this path at all (cases A22..A36 all read
	 * 20), but the two vertical PADDINGS are, and unlike a margin they leave the
	 * rect honest. So the item's own content height is measured with no vertical
	 * padding, and the row's spare pixels are split above and below it. */
	for ( int pt = 0; pt <= 12; pt += 2 ) {
		const int pb = pt == 0 ? 0 : rowH - 16 - pt;
		runCase( o, QStringLiteral( "case D%1 -- ::item padding-top %1 / bottom %2" )
			.arg( pt ).arg( pb ),
			boxQss() + btnQss( 26 )
				+ QStringLiteral( "QMenuBar::item { padding-top: %1px; padding-bottom: %2px; }" )
					.arg( pt ).arg( pb ) );
	}
	// the shipped candidate, stated the way the code will derive it
	{
		const int content = 16;   // measured by case D0
		const int pt = ( rowH - content ) / 2;
		const int pb = rowH - content - pt;
		runCase( o, QStringLiteral( "case D* -- THE CANDIDATE: content %1 -> padding %2 / %3" )
			.arg( content ).arg( pt ).arg( pb ),
			boxQss() + btnQss( 26 )
				+ QStringLiteral( "QMenuBar::item { padding-top: %1px; padding-bottom: %2px; }" )
					.arg( pt ).arg( pb ) );
	}

	/* E: can the CALIBRATION read the item back with no event loop? wwAlignBarRow
	 * runs during construction and cannot pump events, so the number it reads
	 * has to be there the moment the sheet is set. */
	{
		Rig r = buildRig();
		r.win->show();
		settle();
		r.bar->setMinimumHeight( rowH );
		r.bar->setMaximumHeight( rowH );
		r.bar->setStyleSheet( boxQss() + btnQss( 26 )
			+ QStringLiteral( "QMenuBar::item { padding-top: 0px; padding-bottom: 0px; }" ) );
		r.bar->ensurePolished();
		const int noPump = r.bar->actionGeometry( r.items.first() ).height();
		settle();
		const int pumped = r.bar->actionGeometry( r.items.first() ).height();
		o << QStringLiteral( "=== case E -- the item's content height with no vertical padding:"
			" %1 without an event loop, %2 with one\n" ).arg( noPump ).arg( pumped );
		r.win->hide();
		delete r.win;
	}

	/* F: THE WHOLE FLOW, exactly as wwAlignBarRow will run it -- build, pin the
	 * row, apply the row sheet with the menu-item padding zeroed, read the item
	 * height back with ensurePolished() and NO event loop, derive the two
	 * paddings from the row height, apply, measure. If the derived number here
	 * does not equal case D*'s, the code cannot calibrate at construction time
	 * and the padding has to be stated from a measured constant instead. */
	{
		Rig r = buildRig();
		r.win->show();
		settle();
		r.bar->setMinimumHeight( rowH );
		r.bar->setMaximumHeight( rowH );
		const QString zero = QStringLiteral(
			"QMenuBar::item { padding-top: 0px; padding-bottom: 0px; }" );
		r.bar->setStyleSheet( boxQss() + btnQss( 26 ) + zero );
		r.bar->ensurePolished();
		int content = 0;
		for ( QAction * a : r.items )
			content = qMax( content, r.bar->actionGeometry( a ).height() );
		const int pt = ( rowH - content ) / 2;
		const int pb = rowH - content - pt;
		o << QStringLiteral( "=== case F -- the live flow: calibrated content %1 (no event loop)"
			" -> padding-top %2 / bottom %3\n" ).arg( content ).arg( pt ).arg( pb );
		r.bar->setStyleSheet( boxQss() + btnQss( 26 )
			+ QStringLiteral( "QMenuBar::item { padding-top: %1px; padding-bottom: %2px; }" )
				.arg( pt ).arg( pb ) );
		report( o, QStringLiteral( "case F (measured after the derived sheet)" ), r );
		r.win->hide();
		delete r.win;
	}

	/* G: THE WAY BACK. UI/CompactTopBars = false emits no row sheet at all, so
	 * the menu bar keeps res/style.qss alone -- this must read case 1 exactly. */
	runCase( o, QStringLiteral( "case G -- the way back (no row sheet, = case 1)" ),
		QString() );

	/* H: WHERE THE INK SITS INSIDE THE CONTENT BOX, and whether QFontMetrics
	 * predicts it. The built exe measured, in the real window, ink top =
	 * padding-top + 4 exactly (padding 2 -> 6, padding 9 -> 13) with File 11 rows
	 * tall and Spells 12; the even split therefore centres the BOX and leaves the
	 * TEXT 1.5 px low. This case prints the font's own numbers so the correction
	 * can be stated from them rather than typed. */
	{
		Rig r = buildRig();
		r.win->show();
		settle();
		r.bar->setMinimumHeight( rowH );
		r.bar->setMaximumHeight( rowH );
		r.bar->setStyleSheet( boxQss() + btnQss( 26 )
			+ QStringLiteral( "QMenuBar::item { padding-top: 0px; padding-bottom: 0px; }" ) );
		r.bar->ensurePolished();
		settle();
		const QFontMetrics fm = r.bar->fontMetrics();
		const int content = r.bar->actionGeometry( r.items.first() ).height();
		o << QStringLiteral( "=== case H -- the font, content %1: ascent %2 descent %3 height %4"
			" leading %5 capHeight %6 xHeight %7\n" )
			.arg( content ).arg( fm.ascent() ).arg( fm.descent() ).arg( fm.height() )
			.arg( fm.leading() ).arg( fm.capHeight() ).arg( fm.xHeight() );
		double lo = 1e9, hi = -1e9;
		for ( QAction * a : r.items ) {
			const QString t = a->text().remove( QLatin1Char( '&' ) );
			const QRect tb = fm.tightBoundingRect( t );
			const QRect br = fm.boundingRect( t );
			// where Qt puts the baseline inside a content box of `content` px
			const double base = ( content - fm.height() ) / 2.0 + fm.ascent();
			const double c = base + ( tb.top() + tb.bottom() ) / 2.0;
			lo = qMin( lo, c );
			hi = qMax( hi, c );
			o << QStringLiteral( "  %1: tight y %2..%3  bounding y %4..%5  baseline %6"
				"  predicted ink centre in content %7\n" )
				.arg( t, -8 ).arg( tb.top(), 3 ).arg( tb.bottom(), 3 )
				.arg( br.top(), 3 ).arg( br.bottom(), 3 )
				.arg( base, 5, 'f', 1 ).arg( c, 5, 'f', 1 );
		}
		const double mid = ( lo + hi ) / 2.0;
		const double rowCentre = ( rowH - 1 ) / 2.0;
		o << QStringLiteral( "  predicted ink midpoint in content %1 (extremes %2..%3);"
			" even split would be %4; padding-top to centre the INK = %5 -> %6\n" )
			.arg( mid, 0, 'f', 2 ).arg( lo, 0, 'f', 1 ).arg( hi, 0, 'f', 1 )
			.arg( ( rowH - content ) / 2 )
			.arg( rowCentre - mid, 0, 'f', 2 )
			.arg( qRound( rowCentre - mid ) );
		r.win->hide();
		delete r.win;
	}

	/* I: DOES TAKING THE SHEET OFF AGAIN MOVE THE ITEM? The built gate's M4
	 * appended the row's sheet, then set the menu bar's stylesheet to an empty
	 * string to show the way back -- and the ink did not move one pixel, while
	 * the SAME code appending a different sheet (M5's sabotage) moved it ten.
	 * So: is an empty sheet a no-op on this path, and does an explicit
	 * unpolish/polish fix it? */
	{
		Rig r = buildRig();
		r.win->show();
		settle();
		r.bar->setMinimumHeight( rowH );
		r.bar->setMaximumHeight( rowH );
		const QString rowSheet = boxQss() + btnQss( 26 )
			+ QStringLiteral( "QMenuBar::item { padding-top: 9px; padding-bottom: 10px; }" );
		r.bar->setStyleSheet( rowSheet );
		settle();
		auto itemH = [&]() { return r.bar->actionGeometry( r.items.first() ).height(); };
		auto inkTop = [&]() {
			const QRect g = r.bar->actionGeometry( r.items.first() );
			const QRect ink = inkBox( r.bar, g.left(), g.right() );
			return ink.isNull() ? -1 : ink.top();
		};
		o << QStringLiteral( "=== case I -- taking the sheet off again\n" );
		o << QStringLiteral( "  with the row sheet:            item h %1, ink top %2\n" )
			.arg( itemH(), 3 ).arg( inkTop(), 3 );
		r.bar->setStyleSheet( QString() );
		settle();
		o << QStringLiteral( "  after setStyleSheet(QString()): item h %1, ink top %2\n" )
			.arg( itemH(), 3 ).arg( inkTop(), 3 );
		r.bar->style()->unpolish( r.bar );
		r.bar->style()->polish( r.bar );
		r.bar->updateGeometry();
		settle();
		o << QStringLiteral( "  after unpolish/polish:         item h %1, ink top %2\n" )
			.arg( itemH(), 3 ).arg( inkTop(), 3 );
		// and back on, to show the same widget still responds
		r.bar->setStyleSheet( rowSheet );
		settle();
		o << QStringLiteral( "  row sheet put back:            item h %1, ink top %2\n" )
			.arg( itemH(), 3 ).arg( inkTop(), 3 );
		// the other candidate: state the app sheet's own menu-item rule instead
		r.bar->setStyleSheet( QStringLiteral( "QMenuBar::item { padding: 4px 8px; }" ) );
		settle();
		o << QStringLiteral( "  explicit \"padding: 4px 8px\":   item h %1, ink top %2"
			"   (case 1 reads item h 24, ink top 8)\n" )
			.arg( itemH(), 3 ).arg( inkTop(), 3 );
		r.win->hide();
		delete r.win;
	}

	o.flush();
	return 0;
}
