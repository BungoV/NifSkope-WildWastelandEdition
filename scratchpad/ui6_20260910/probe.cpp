/* Lane UI6 -- the two questions this lane's build turns on, measured OUTSIDE the
 * application (skill ww-qss-geometry-probe).
 *
 *   PART 1, the strip.  Case 0 must reproduce the SHIPPED 20:45:47/05:58:21
 *   state that water_ui.sh's group S measured on the real window:
 *       segments 4 px apart, 4 px of air top / bottom / left, segment h 27 in a
 *       row of 35.
 *   Then the JOINED candidate: the same outer air with the segments touching.
 *
 *   PART 2, the menu arrow.  Case 0 must reproduce the OVERLAP bungo saw on the
 *   viewport header's two icon buttons: the dropdown arrow drawn on top of the
 *   glyph. Then a sweep of `padding-right` on the buttons that have a menu.
 *
 * Every number is read from PIXELS. tabRect() includes a tab's margin (lane
 * UI4), and no rect a QToolButton can be asked for says where the style drew
 * its menu-indicator.
 *
 * Build: bash scratchpad/ui6_20260910/build_probe.sh   (MSYS2 UCRT64)
 * Run  : ./release/ui6_probe.exe -platform offscreen   (from the repo root)
 */

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>

// ---------------------------------------------------------------- the palette
// The dark column of skinVars[] (src/nifskope_ui.cpp:299).
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

static QString skin( const char * name )
{
	for ( const auto & v : vars )
		if ( qstrcmp( v.name, name ) == 0 )
			return QString::fromLatin1( v.dark );
	return QStringLiteral( "#ff00ff" );
}

// The application's own substitution, nifskope_ui.cpp:30194-30211.
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

static void settle()
{
	for ( int i = 0; i < 6; i++ )
		QApplication::processEvents();
}

/* ========================================================================
 *  PART 1 -- the segmented strip
 * ===================================================================== */

/*! The SHIPPED wwSegmentedQss (src/nifskope_ui.cpp:548) at `air`, and the
 *  JOINED candidate at the same air. `joined` is the whole of the change. */
static QString segQss( int rowHeight, int air, int neighbourGap, bool joined )
{
	const int minH = rowHeight > 0 ? qMax( 10, rowHeight - 8 - 2 * air ) : 18;
	const int rightAir = qMax( 0, air - neighbourGap );
	QString sheet = QStringLiteral(
		"%1 { min-height: MINHpx; padding: 3px 10px; color: %4; background: %5;"
		" border: 1px solid %6; SEAM border-radius: BRADpx; MARGINALL }"
		"%2 { border-left: 1px solid %6; border-top-left-radius: 3px;"
		" border-bottom-left-radius: 3px; MARGINFIRST }"
		"%3 { border-top-right-radius: 3px; border-bottom-right-radius: 3px;"
		" MARGINLAST }"
		"%1:hover { background: %7; }"
		"%1:checked, %1:selected { border-color: %8; color: %9; background: %8; }" )
		.arg( QStringLiteral( "QTabBar::tab" ), QStringLiteral( "QTabBar::tab:first" ),
			QStringLiteral( "QTabBar::tab:last" ), skin( "text" ), skin( "bgBtn" ),
			skin( "border" ), skin( "bgBtnHover" ), skin( "selBgActive" ),
			skin( "textBright" ) );
	sheet.replace( QLatin1String( "MINH" ), QString::number( minH ) );
	if ( joined ) {
		// the JOINED look at any air: one shared seam, square inner corners,
		// the outer air on the outer segments only
		sheet.replace( QLatin1String( "SEAM" ), QStringLiteral( "border-left: 0;" ) );
		sheet.replace( QLatin1String( "BRAD" ), QStringLiteral( "0" ) );
		sheet.replace( QLatin1String( "MARGINALL" ), air > 0
			? QStringLiteral( "margin-top: %1px; margin-bottom: %1px; margin-left: 0px;"
				" margin-right: 0px;" ).arg( air ) : QString() );
		sheet.replace( QLatin1String( "MARGINFIRST" ), air > 0
			? QStringLiteral( "margin-left: %1px;" ).arg( air ) : QString() );
		sheet.replace( QLatin1String( "MARGINLAST" ), air > 0
			? QStringLiteral( "margin-right: %1px;" ).arg( rightAir ) : QString() );
	} else {
		// the SHIPPED separated look
		sheet.replace( QLatin1String( "SEAM" ),
			air > 0 ? QString() : QStringLiteral( "border-left: 0;" ) );
		sheet.replace( QLatin1String( "BRAD" ), QString::number( air > 0 ? 3 : 0 ) );
		sheet.replace( QLatin1String( "MARGINALL" ), air > 0
			? QStringLiteral( "margin-top: %1px; margin-bottom: %1px; margin-left: %1px;"
				" margin-right: 0px;" ).arg( air ) : QString() );
		sheet.replace( QLatin1String( "MARGINFIRST" ), QString() );
		sheet.replace( QLatin1String( "MARGINLAST" ), air > 0
			? QStringLiteral( "margin-right: %1px;" ).arg( rightAir ) : QString() );
	}
	return sheet;
}

struct Rig
{
	QMainWindow * win = nullptr;
	QTabBar * tabs = nullptr;
	QWidget * header = nullptr;
	QDockWidget * dock = nullptr;
};

static Rig buildRig()
{
	Rig r;
	r.win = new QMainWindow;
	r.win->resize( 1512, 400 );

	auto * tb = new QToolBar( QStringLiteral( "tFile" ), r.win );
	tb->setObjectName( QStringLiteral( "tFile" ) );
	tb->setMinimumHeight( 35 );
	tb->setMaximumHeight( 35 );
	r.win->addToolBar( Qt::TopToolBarArea, tb );

	auto * central = new QWidget( r.win );
	auto * col = new QVBoxLayout( central );
	col->setContentsMargins( 0, 0, 0, 0 );
	col->setSpacing( 0 );
	r.header = new QWidget( central );
	r.header->setObjectName( QStringLiteral( "ViewportHeader" ) );
	r.header->setMinimumHeight( 35 );
	r.header->setMaximumHeight( 35 );
	col->addWidget( r.header, 0 );
	col->addWidget( new QWidget( central ), 1 );
	r.win->setCentralWidget( central );

	r.dock = new QDockWidget( QStringLiteral( "Left Editor" ), r.win );
	r.dock->setObjectName( QStringLiteral( "LeftColumnDock" ) );
	r.dock->setAllowedAreas( Qt::LeftDockWidgetArea );
	r.dock->setFeatures( QDockWidget::NoDockWidgetFeatures );
	auto * hiddenTitle = new QWidget( r.dock );
	hiddenTitle->setFixedHeight( 0 );
	r.dock->setTitleBarWidget( hiddenTitle );

	auto * host = new QWidget( r.dock );
	host->setObjectName( QStringLiteral( "LeftColumnHost" ) );
	auto * hostLayout = new QVBoxLayout( host );
	hostLayout->setContentsMargins( 0, 0, 0, 0 );
	hostLayout->setSpacing( 0 );

	r.tabs = new QTabBar( host );
	r.tabs->setObjectName( QStringLiteral( "LeftColumnModeSelector" ) );
	r.tabs->setDocumentMode( true );
	r.tabs->setDrawBase( false );
	r.tabs->setExpanding( true );
	r.tabs->setUsesScrollButtons( false );
	r.tabs->addTab( QStringLiteral( "Header" ) );
	r.tabs->addTab( QStringLiteral( "Blocks" ) );
	r.tabs->addTab( QStringLiteral( "Files" ) );
	hostLayout->addWidget( r.tabs, 0 );

	auto * stack = new QStackedWidget( host );
	stack->addWidget( new QWidget( stack ) );
	hostLayout->addWidget( stack, 1 );
	r.dock->setWidget( host );
	r.win->addDockWidget( Qt::LeftDockWidgetArea, r.dock );
	return r;
}

static QRect paintedBox( QTabBar * tabs, int i )
{
	tabs->setCurrentIndex( i );
	settle();
	const QImage img = tabs->grab().toImage().convertToFormat( QImage::Format_RGB32 );
	const QRgb want = QColor( skin( "selBgActive" ) ).rgb() & 0x00ffffff;
	int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			if ( ( img.pixel( x, y ) & 0x00ffffff ) != want )
				continue;
			x0 = qMin( x0, x ); x1 = qMax( x1, x );
			y0 = qMin( y0, y ); y1 = qMax( y1, y );
		}
	}
	if ( x1 < 0 )
		return QRect();
	return QRect( QPoint( x0, y0 ), QPoint( x1, y1 ) );
}

static void stripCase( QTextStream & o, const QString & title, int air, bool joined )
{
	Rig r = buildRig();
	r.tabs->setStyleSheet( segQss( 0, 0, 0, false ) );
	r.win->show();
	settle();
	r.win->resizeDocks( { r.dock }, { 383 }, Qt::Horizontal );
	settle();
	r.tabs->setMinimumHeight( 35 );
	r.tabs->setMaximumHeight( 35 );
	const int gap = r.win->style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent,
		nullptr, r.win );
	r.tabs->setStyleSheet( segQss( 35, air, gap, joined ) );
	settle();

	const QPoint tabsAt = r.tabs->mapTo( r.win, QPoint( 0, 0 ) );
	const QPoint hdrAt = r.header->mapTo( r.win, QPoint( 0, 0 ) );
	o << "=== " << title << "\n";
	o << QStringLiteral( "  tab bar x %1 top %2 w %3 h %4; header x %5; separator metric %6\n" )
		.arg( tabsAt.x() ).arg( tabsAt.y() ).arg( r.tabs->width() ).arg( r.tabs->height() )
		.arg( hdrAt.x() ).arg( gap );
	QList<QRect> boxes;
	for ( int i = 0; i < r.tabs->count(); i++ ) {
		const QRect b = paintedBox( r.tabs, i );
		boxes.append( b );
		o << QStringLiteral( "  segment %1 painted x %2 y %3 w %4 h %5 (right %6 bottom %7);"
			" tabRect x %8 w %9 h %10\n" )
			.arg( i ).arg( b.x() ).arg( b.y() ).arg( b.width() ).arg( b.height() )
			.arg( b.right() ).arg( b.bottom() )
			.arg( r.tabs->tabRect( i ).x() ).arg( r.tabs->tabRect( i ).width() )
			.arg( r.tabs->tabRect( i ).height() );
	}
	if ( boxes.size() == 3 && !boxes.at( 0 ).isNull() && !boxes.at( 2 ).isNull() ) {
		o << QStringLiteral( "  THE FIVE: top %1  bottom %2  left %3  right-to-toolbar %4"
			"  between %5 / %6   (segment h %7)\n" )
			.arg( boxes.at( 0 ).top() )
			.arg( r.tabs->height() - 1 - boxes.at( 0 ).bottom() )
			.arg( tabsAt.x() + boxes.at( 0 ).left() )
			.arg( hdrAt.x() - ( tabsAt.x() + boxes.at( 2 ).right() ) - 1 )
			.arg( boxes.at( 1 ).left() - boxes.at( 0 ).right() - 1 )
			.arg( boxes.at( 2 ).left() - boxes.at( 1 ).right() - 1 )
			.arg( boxes.at( 0 ).height() );
	}
	r.win->hide();
}

/* ========================================================================
 *  PART 2 -- the dropdown arrow on a viewport-header button
 * ===================================================================== */

//! The row's button sheet as it ships today (src/nifskope_ui.cpp:743), plus the
//! candidate's extra rule for the buttons that carry a menu.
static QString rowBtnQss( int content, int pad, int arrowAir )
{
	QString s = QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" ).arg( content ).arg( pad );
	if ( arrowAir > 0 )
		s += QStringLiteral( "QToolButton[popupMode=\"2\"] { padding-right: %1px; }" )
			.arg( arrowAir );
	return s;
}

struct BtnRig
{
	QMainWindow * win = nullptr;
	QToolBar * bar = nullptr;
	QList<QToolButton *> btns;
	QStringList names;
};

static QIcon dot( const QColor & c, bool ring )
{
	QPixmap pm( 16, 16 );
	pm.fill( Qt::transparent );
	QPainter p( &pm );
	p.setRenderHint( QPainter::Antialiasing );
	p.setPen( QPen( c, 1.4 ) );
	if ( ring ) {
		// a 3x3 grid, like the header's grid button
		for ( int i = 0; i <= 3; i++ ) {
			p.drawLine( 2 + i * 4, 2, 2 + i * 4, 14 );
			p.drawLine( 2, 2 + i * 4, 14, 2 + i * 4 );
		}
	} else {
		p.setBrush( c );
		p.drawEllipse( QPointF( 8, 8 ), 3.2, 3.2 );
	}
	p.end();		// the painter must be OFF the pixmap before QIcon copies it
	return QIcon( pm );
}

static BtnRig buildBtnRig()
{
	BtnRig r;
	r.win = new QMainWindow;
	r.win->resize( 1200, 200 );
	r.bar = new QToolBar( QStringLiteral( "tRender" ), r.win );
	r.bar->setObjectName( QStringLiteral( "tRender" ) );
	r.win->addToolBar( Qt::TopToolBarArea, r.bar );

	auto add = [&]( const QString & name, const QString & text, bool icon, bool grid ) {
		auto * b = new QToolButton( r.win );
		b->setPopupMode( QToolButton::InstantPopup );
		b->setAutoRaise( true );
		auto * m = new QMenu( b );
		m->addAction( QStringLiteral( "one" ) );
		m->addAction( QStringLiteral( "two" ) );
		b->setMenu( m );
		if ( icon )
			b->setIcon( dot( QColor( skin( "text" ) ), grid ) );
		if ( !text.isEmpty() )
			b->setText( text );
		b->setToolButtonStyle( text.isEmpty() ? Qt::ToolButtonIconOnly
											  : Qt::ToolButtonTextBesideIcon );
		b->setObjectName( name );
		r.bar->addWidget( b );
		r.btns.append( b );
		r.names.append( name );
		return b;
	};
	add( QStringLiteral( "Global" ), QStringLiteral( "Global" ), true, false );
	add( QStringLiteral( "pivot" ), QString(), true, false );
	add( QStringLiteral( "snap" ), QString(), true, true );
	add( QStringLiteral( "grid" ), QString(), true, true );
	return r;
}

/*! The arrow's column, the glyph's last column, and the gap between them --
 *  from TWO grabs of the same button at the same size, one with its menu taken
 *  away. The columns that differ ARE the arrow; nothing else can move, because
 *  the button's geometry is pinned across the pair. */
struct ArrowRead { int arrowFirst = -1; int inkLast = -1; int gap = -99; int arrowCols = 0; bool ok = false; };

static ArrowRead readArrow( QToolButton * b )
{
	ArrowRead a;
	if ( !b )
		return a;
	const int w = b->width(), h = b->height();
	b->setFixedSize( w, h );                       // pin, so the pair is comparable
	settle();
	/* NOT b->grab(): an auto-raise button paints no background of its own, and
	 * grab() hands back a pixmap Qt filled with white -- on which this theme's
	 * near-white glyphs are invisible (measured: bg #efefef, icon #e6e8eb, 7
	 * apart). Render over a known dark fill instead. */
	auto shot = [&]() {
		QPixmap pm( w, h );
		pm.fill( QColor( skin( "bgBar" ) ) );
		b->render( &pm, QPoint(), QRegion(), QWidget::DrawChildren );
		return pm.toImage().convertToFormat( QImage::Format_RGB32 );
	};
	const QImage with = shot();
	QMenu * m = b->menu();
	b->setMenu( nullptr );
	settle();
	const QImage without = shot();
	b->setMenu( m );
	b->setMinimumSize( 0, 0 );
	b->setMaximumSize( QWIDGETSIZE_MAX, QWIDGETSIZE_MAX );
	settle();
	if ( with.size() != without.size() || with.width() < 4 )
		return a;

	// the background is the commonest colour in the "without" grab
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
			a.arrowCols++;
		}
		if ( ink )
			a.inkLast = x;
	}
	a.ok = ( a.arrowFirst >= 0 && a.inkLast >= 0 );
	if ( a.ok )
		a.gap = a.arrowFirst - a.inkLast - 1;
	if ( !a.ok ) {
		// a scan that found nothing has to say what it was looking at
		with.save( QStringLiteral( "scratchpad/ui6_20260910/images/dbg_%1_with.png" )
			.arg( b->objectName() ) );
		without.save( QStringLiteral( "scratchpad/ui6_20260910/images/dbg_%1_without.png" )
			.arg( b->objectName() ) );
		fprintf( stderr, "  DBG %s: icon null %d, size %dx%d, bg #%06x lum %.1f\n",
			qPrintable( b->objectName() ), int( b->icon().isNull() ),
			without.width(), without.height(), unsigned( bg ), bgL );
	}
	return a;
}

static void arrowCase( QTextStream & o, const QString & title, int arrowAir )
{
	BtnRig r = buildBtnRig();
	const QString box = QStringLiteral( "QToolBar { border: 0px; padding: 0px; margin: 0px; }" );
	r.bar->setMinimumHeight( 35 );
	r.bar->setMaximumHeight( 35 );
	r.win->show();
	settle();
	/* wwAlignBarRow's own calibration, reproduced: ask for a content height
	 * certainly taller than the glyph line, read back what the style added,
	 * take that much out of the row. */
	auto applyRow = [&]( int content ) {
		const QString btn = rowBtnQss( content, 2, arrowAir );
		r.bar->setStyleSheet( box + btn );
		for ( QToolButton * b : r.btns )
			b->setStyleSheet( btn );
		settle();
	};
	applyRow( 35 );
	int overhead = -1;
	for ( QToolButton * b : r.btns ) {
		b->ensurePolished();
		overhead = qMax( overhead, b->sizeHint().height() - 35 );
	}
	const int content = ( overhead >= 0 && 35 - overhead >= 8 ) ? 35 - overhead : 26;
	applyRow( content );
	o << "=== " << title << "\n";
	o << QStringLiteral( "  calibration: overhead %1 -> content %2\n" ).arg( overhead ).arg( content );
	for ( int i = 0; i < r.btns.size(); i++ ) {
		QToolButton * b = r.btns.at( i );
		const ArrowRead a = readArrow( b );
		o << QStringLiteral( "  %1: %2x%3 at y %4; arrow cols %5 first %6; glyph last ink %7;"
			"  GAP %8%9\n" )
			.arg( r.names.at( i ), -8 ).arg( b->width() ).arg( b->height() ).arg( b->y() )
			.arg( a.arrowCols ).arg( a.arrowFirst ).arg( a.inkLast ).arg( a.gap )
			.arg( a.ok ? QString() : QStringLiteral( "  (UNREADABLE)" ) );
	}
	r.win->hide();
}

int main( int argc, char ** argv )
{
	QApplication app( argc, argv );
	QFile qss( QStringLiteral( "release/style.qss" ) );
	if ( qss.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		app.setStyleSheet( substitute( QString::fromUtf8( qss.readAll() ) ) );
		qss.close();
	} else {
		fprintf( stderr, "no release/style.qss -- run from the repo root\n" );
		return 2;
	}

	QFile out( QStringLiteral( "scratchpad/ui6_20260910/probe_out.txt" ) );
	if ( !out.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return 2;
	QTextStream o( &out );

	o << "---- PART 1: the segmented strip ----\n";
	stripCase( o, QStringLiteral( "case 0 -- THE SHIPPED SHEET, air 4, separated"
		" (must read between 4 / 4)" ), 4, false );
	stripCase( o, QStringLiteral( "case 1 -- air 0, the 18:25:20 flush strip"
		" (must read every distance 0)" ), 0, false );
	stripCase( o, QStringLiteral( "case J4 -- THE CANDIDATE: joined, air 4"
		" (want top/bottom/left/right 4, between 0 / 0, segment h 27)" ), 4, true );
	stripCase( o, QStringLiteral( "case J0 -- joined at air 0 (the way back:"
		" must equal case 1 exactly)" ), 0, true );

	o << "\n---- PART 2: the dropdown arrow ----\n";
	arrowCase( o, QStringLiteral( "case A0 -- THE SHIPPED SHEET, no arrow air"
		" (must show the overlap: gap <= 0 on the icon-only buttons)" ), 0 );
	for ( int air : { 2, 4, 6, 8, 10, 12, 14 } )
		arrowCase( o, QStringLiteral( "case A%1 -- padding-right %1 on"
			" QToolButton[popupMode=\"2\"]" ).arg( air ), air );

	out.close();
	fprintf( stderr, "probe done -> scratchpad/ui6_20260910/probe_out.txt\n" );
	return 0;
}
