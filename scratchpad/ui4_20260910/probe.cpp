/* Lane UI4 -- reproduce the Header | Blocks | Files strip's geometry OUTSIDE the
 * application, so the one build of the session is spent on a number that is
 * already known to work (skill ww-qss-geometry-probe).
 *
 * Case 0 is the SHIPPED state and must reproduce what water_ui.sh measured on
 * the 18:25:20 exe:
 *     dock tab strip : x 0  top 35  w 383  h 35   tabs 128 / 127 / 128
 *     viewport header: x 386          -> 3 px between the dock and the column
 *
 * TWO THINGS THIS PROBE FOUND THAT NO AMOUNT OF READING WOULD HAVE:
 *   (a) release/style.qss is a BYTE COPY of res/style.qss -- the `${name}`
 *       skin tokens are NOT substituted on disk, the application substitutes
 *       them at load (nifskope_ui.cpp:30199-30211). A probe that feeds the file
 *       to QApplication::setStyleSheet raw silently loses every rule that
 *       carries a colour, including QMainWindow::separator's 3 px width, and
 *       then measures a 6 px separator that the application does not have.
 *   (b) QSS margins on QTabBar::tab ARE honoured, but QTabBar::tabRect()
 *       RETURNS THE RECT INCLUDING THE MARGIN. Rect arithmetic therefore cannot
 *       see the air at all; only the pixels can. Every number below is measured
 *       from a grab of the strip by the selected segment's own fill colour.
 *
 * Build: bash scratchpad/ui4_20260910/build_probe.sh  (MSYS2 UCRT64)
 * Run  : ./release/ui4_probe.exe -platform offscreen   (from the repo root)
 */

#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QPixmap>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>

// ---------------------------------------------------------------- the palette
// The dark column of skinVars[] (src/nifskope_ui.cpp:299) -- only the names the
// segmented sheet and res/style.qss need for GEOMETRY plus the ones whose
// absence would make Qt drop a whole rule.
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

// ------------------------------------------------------- the sheet under test
// The shipped wwSegmentedQss() (src/nifskope_ui.cpp:538), with three injection
// points so a case can add margins without re-typing the sheet.
static QString segQss( int rowHeight, const QString & extraTab = QString(),
                       const QString & extraFirst = QString(),
                       const QString & extraLast = QString() )
{
	const int minH = rowHeight > 0 ? qMax( 10, rowHeight - 8 ) : 18;
	QString sheet = QStringLiteral(
		"QTabBar::tab { min-height: MINHpx; padding: 3px 10px; color: %1;"
		" background: %2; border: 1px solid %3; border-left: 0; border-radius: 0;"
		" EXTRATAB }"
		"QTabBar::tab:first { border-left: 1px solid %3; border-top-left-radius: 3px;"
		" border-bottom-left-radius: 3px; EXTRAFIRST }"
		"QTabBar::tab:last { border-top-right-radius: 3px;"
		" border-bottom-right-radius: 3px; EXTRALAST }"
		"QTabBar::tab:hover { background: %4; }"
		"QTabBar::tab:checked, QTabBar::tab:selected { border-color: %5; color: %6;"
		" background: %5; }" )
		.arg( skin( "text" ), skin( "bgBtn" ), skin( "border" ), skin( "bgBtnHover" ),
			skin( "selBgActive" ), skin( "textBright" ) );
	sheet.replace( QLatin1String( "MINH" ), QString::number( minH ) );
	sheet.replace( QLatin1String( "EXTRATAB" ), extraTab );
	sheet.replace( QLatin1String( "EXTRAFIRST" ), extraFirst );
	sheet.replace( QLatin1String( "EXTRALAST" ), extraLast );
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

	// the left dock, as nifskope_ui.cpp:24325-24397 builds it
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

static void settle()
{
	for ( int i = 0; i < 6; i++ )
		QApplication::processEvents();
}

/*! The PAINTED box of tab `i`: select it, grab the strip, and take the bounding
 *  box of the pixels carrying the selected segment's own fill colour. tabRect()
 *  cannot be used -- it includes the margin. */
static QRect paintedBox( QTabBar * tabs, int i )
{
	tabs->setCurrentIndex( i );
	settle();
	const QImage img = tabs->grab().toImage().convertToFormat( QImage::Format_RGB32 );
	const QRgb want = QColor( skin( "selBgActive" ) ).rgb();
	int x0 = 1 << 20, y0 = 1 << 20, x1 = -1, y1 = -1;
	for ( int y = 0; y < img.height(); y++ ) {
		for ( int x = 0; x < img.width(); x++ ) {
			if ( ( img.pixel( x, y ) & 0x00ffffff ) != ( want & 0x00ffffff ) )
				continue;
			x0 = qMin( x0, x ); x1 = qMax( x1, x );
			y0 = qMin( y0, y ); y1 = qMax( y1, y );
		}
	}
	if ( x1 < 0 )
		return QRect();
	return QRect( QPoint( x0, y0 ), QPoint( x1, y1 ) );
}

static void report( QTextStream & o, const QString & title, Rig & r )
{
	settle();
	const QPoint tabsAt = r.tabs->mapTo( r.win, QPoint( 0, 0 ) );
	const QPoint hdrAt = r.header->mapTo( r.win, QPoint( 0, 0 ) );
	o << "=== " << title << "\n";
	o << QStringLiteral( "  tab bar : x %1 top %2 w %3 h %4    header x %5    separator %6"
		"    PM_DockWidgetSeparatorExtent %7\n" )
		.arg( tabsAt.x() ).arg( tabsAt.y() ).arg( r.tabs->width() ).arg( r.tabs->height() )
		.arg( hdrAt.x() ).arg( hdrAt.x() - ( tabsAt.x() + r.tabs->width() ) )
		.arg( r.win->style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent, nullptr, r.win ) );

	QList<QRect> boxes;
	for ( int i = 0; i < r.tabs->count(); i++ ) {
		const QRect b = paintedBox( r.tabs, i );
		boxes.append( b );
		o << QStringLiteral( "  segment %1 painted: x %2 y %3 w %4 h %5 (right %6 bottom %7)"
			"   tabRect w %8 h %9\n" )
			.arg( i ).arg( b.x() ).arg( b.y() ).arg( b.width() ).arg( b.height() )
			.arg( b.right() ).arg( b.bottom() )
			.arg( r.tabs->tabRect( i ).width() ).arg( r.tabs->tabRect( i ).height() );
	}
	if ( boxes.size() == 3 && !boxes.at( 0 ).isNull() && !boxes.at( 2 ).isNull() ) {
		o << QStringLiteral( "  THE FIVE: top %1  bottom %2  left %3  right-to-toolbar %4"
			"  between %5 / %6\n" )
			.arg( boxes.at( 0 ).top() )
			.arg( r.tabs->height() - 1 - boxes.at( 0 ).bottom() )
			.arg( tabsAt.x() + boxes.at( 0 ).left() )
			.arg( hdrAt.x() - ( tabsAt.x() + boxes.at( 2 ).right() ) - 1 )
			.arg( boxes.at( 1 ).left() - boxes.at( 0 ).right() - 1 )
			.arg( boxes.at( 2 ).left() - boxes.at( 1 ).right() - 1 );
	}
}

static void runCase( QTextStream & o, const QString & title, const QString & sheet )
{
	Rig r = buildRig();
	r.tabs->setStyleSheet( segQss( 0 ) );
	r.win->show();
	settle();
	r.win->resizeDocks( { r.dock }, { 383 }, Qt::Horizontal );
	settle();
	// wwAlignBarRow's own pinning, then its restyle
	r.tabs->setMinimumHeight( 35 );
	r.tabs->setMaximumHeight( 35 );
	r.tabs->setStyleSheet( sheet );
	report( o, title, r );
	r.win->hide();
}

int main( int argc, char ** argv )
{
	QApplication app( argc, argv );
	QFile qss( QStringLiteral( "release/style.qss" ) );
	if ( qss.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
		const QString raw = QString::fromUtf8( qss.readAll() );
		app.setStyleSheet( substitute( raw ) );
	} else {
		fprintf( stderr, "WARNING: release/style.qss not read -- run from the repo root\n" );
	}

	QTextStream o( stdout );
	/* Can the SHEET be written without a widget in hand? wwSegmentedTabBarQss()
	 * takes none, so the separator has to come off qApp's style with a null
	 * widget, and a metric that only answers for a widget would be a trap. */
	o << QStringLiteral( "qApp style \"%1\": PM_DockWidgetSeparatorExtent(nullptr) = %2\n" )
		.arg( QApplication::style()->objectName() )
		.arg( QApplication::style()->pixelMetric( QStyle::PM_DockWidgetSeparatorExtent, nullptr, nullptr ) );

	runCase( o, QStringLiteral( "case 0 -- SHIPPED 18:25:20 (want 0 / 0 / 0 / 3 / 0)" ),
		segQss( 35 ) );

	/* The candidate: 4 px of air on every side of the strip and between the
	 * segments, stated as tab MARGINS, with the min-height reduced by the
	 * vertical pair so the bar itself stays 35. Every segment closes its own box
	 * (border-left back, radius on both ends), because a segment with a gap
	 * beside it and no left border is an open box.
	 * `right` sweeps what the LAST segment needs, since the 3 px painted
	 * QMainWindow::separator already stands between the dock and the column. */
	for ( int right = 0; right <= 4; right++ ) {
		const QString tab = QStringLiteral(
			"margin-top: 4px; margin-bottom: 4px; margin-left: 4px; margin-right: 0px;"
			" border-left: 1px solid %1; border-radius: 3px; min-height: %2px;" )
			.arg( skin( "border" ) ).arg( 35 - 8 - 8 );
		const QString last = QStringLiteral( "margin-right: %1px;" ).arg( right );
		runCase( o, QStringLiteral( "case A%1 -- 4 px air, last margin-right %1" ).arg( right ),
			segQss( 35, tab, QString(), last ) );
	}

	// the min-height basin: which value keeps the painted segment 27 px tall
	for ( int minh = 15; minh <= 22; minh++ ) {
		const QString tab = QStringLiteral(
			"margin-top: 4px; margin-bottom: 4px; margin-left: 4px; margin-right: 0px;"
			" border-left: 1px solid %1; border-radius: 3px; min-height: %2px;" )
			.arg( skin( "border" ) ).arg( minh );
		runCase( o, QStringLiteral( "case B%1 -- min-height %1" ).arg( minh ),
			segQss( 35, tab, QString(), QStringLiteral( "margin-right: 1px;" ) ) );
	}

	o.flush();
	return 0;
}
