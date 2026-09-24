/* Lane UI5-HOVERPIC -- the HOVERED / OPEN menu title, before and after lane
 * UI5, measured and photographed OUTSIDE the application (skill
 * ww-qss-geometry-probe).
 *
 * Why a probe and not an in-app grab: lane UI6 holds the one allowed NifSkope
 * instance and is building. This rig links Qt6Widgets only -- it cannot touch
 * release/NifSkope.exe, GeneratedFiles/ or the Makefile.
 *
 * WHAT IS UNDER TEST. res/style.qss:49 states
 *     QMenuBar::item:selected { background: rgba(${rgb}, 255); }   -- #4a7ab0
 * and QStyleSheetStyle draws that rule over the item's WHOLE rect. So the
 * hover/open highlight is exactly as tall as the menu item's painted box:
 *   BEFORE lane UI5 -- the row states only `QMenuBar::item { min-height; }`,
 *      which is not consulted on this path, so the item is 2 + 16 + 2 = 20 px
 *      at the TOP of the 35 px row (report section 1).
 *   AFTER  lane UI5 -- wwBarRowMenuItemQss states padding-top 9 / bottom 10
 *      derived from (row - measured item content), so the item's box IS the
 *      row: 35 px (report section 2, case D*).
 * The number this rig reports is measured from the grab, not from a rect: the
 * bounding box of the selection colour inside the hovered title's x span.
 *
 * Build: bash scratchpad/ui5_20260910/build_hoverprobe.sh   (MSYS2 UCRT64)
 * Run  : ./release/ui5_hoverprobe.exe -platform offscreen   (from the repo root)
 */

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFontMetrics>
#include <QImage>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPixmap>
#include <QRegularExpression>
#include <QStyle>
#include <QTextStream>
#include <QWidget>

#include <cstdio>

// ---------------------------------------------------------------- the palette
// The dark column of skinVars[] (src/nifskope_ui.cpp:299). release/style.qss is
// a BYTE COPY of res/style.qss -- the ${name} tokens are NOT substituted on
// disk; the application substitutes them at load (src/nifskope_ui.cpp:30199).
// A probe that feeds the file to setStyleSheet raw silently loses every rule
// carrying a colour -- which here would lose the selection background itself.
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
static const int wwBarRowPad = 2;     // src/nifskope_ui.cpp:728
static const int rowContent = 26;     // what wwAlignBarRow calibrates in this row

//! wwBarRowBoxQss() as it ships (src/nifskope_ui.cpp:734).
static QString boxQss()
{
	return QStringLiteral( "QToolBar { border: 0px; padding: 0px; margin: 0px; }" );
}

//! wwBarRowButtonQss( content ) as it ships (src/nifskope_ui.cpp:743) -- this is
//! the WHOLE of what the menu bar carried BEFORE lane UI5.
static QString btnQss( int content )
{
	return QStringLiteral(
		"QMenuBar::item { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton { min-height: %1px; padding-top: %2px; padding-bottom: %2px; }"
		"QToolButton::menu-indicator { subcontrol-position: right center;"
		" subcontrol-origin: padding; }" )
		.arg( content ).arg( wwBarRowPad );
}

//! wwBarRowMenuItemQss( rowHeight, itemContent ) as it ships (:784) -- appended
//! LAST, after btnQss's own menu-item rule, exactly as applyRow does (:867).
static QString menuItemQss( int rowHeight, int itemContent )
{
	if ( rowHeight <= 0 || itemContent <= 0 || itemContent > rowHeight )
		return QString();
	const int top = ( rowHeight - itemContent ) / 2;
	const int bottom = rowHeight - itemContent - top;
	return QStringLiteral( "QMenuBar::item { padding-top: %1px; padding-bottom: %2px; }" )
		.arg( top ).arg( bottom );
}

struct Rig
{
	QMainWindow * win = nullptr;
	QMenuBar * bar = nullptr;
	QList<QAction *> items;
};

static void settle()
{
	for ( int i = 0; i < 8; i++ )
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
	r.win->setCentralWidget( new QWidget( r.win ) );
	return r;
}

/*! Bounding box of every pixel of `want`, in LOGICAL pixels, inside the x span
 *  [x0lim, x1lim]. Returns a null rect when the colour is not there -- which is
 *  what the floor below asks for on purpose. */
static QRect colourBox( const QImage & img, qreal dpr, QRgb want, int x0lim, int x1lim )
{
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

/*! One half of the picture: apply `sheet`, hover the Spells title, grab the bar,
 *  save it, and measure the highlight's height out of the pixels. */
static void half( QTextStream & o, const QString & tag, const QString & sheet,
	const QString & png, int hoverIndex )
{
	Rig r = buildRig();
	r.win->show();
	settle();
	// wwAlignBarRow pins the row, then applies its sheets (src/nifskope_ui.cpp:867)
	r.bar->setMinimumHeight( rowH );
	r.bar->setMaximumHeight( rowH );
	r.bar->setStyleSheet( sheet );
	settle();

	QAction * hovered = r.items.at( hoverIndex );
	const QRect g = r.bar->actionGeometry( hovered );

	/* HOVER, not a screenshot of a menu somebody opened by hand. A plain
	 * MouseMove with no buttons down takes QMenuBar's mouseMoveEvent path
	 * `setCurrentAction( action, popupState=false )`, which sets State_Selected
	 * on that item and repaints -- no popup window, so the grab is the BAR.
	 * setActiveAction() is the fallback below if that ever stops working. */
	const QPointF local( g.center() );
	const QPointF global( r.bar->mapToGlobal( g.center() ) );
	QMouseEvent move( QEvent::MouseMove, local, global,
		Qt::NoButton, Qt::NoButton, Qt::NoModifier );
	QApplication::sendEvent( r.bar, &move );
	settle();
	QString how = QStringLiteral( "synthetic QMouseEvent(MouseMove)" );
	if ( r.bar->activeAction() != hovered ) {
		r.bar->setActiveAction( hovered );
		settle();
		how = QStringLiteral( "QMenuBar::setActiveAction (mouse move did not take)" );
	}

	const QPixmap pm = r.bar->grab();          // the WIDGET, never the desktop
	const qreal dpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
	const QImage img = pm.toImage().convertToFormat( QImage::Format_RGB32 );
	img.save( png );

	// the highlight: res/style.qss:49 rgba(74, 122, 176, 255)
	const QRect hi = colourBox( img, dpr, qRgb( 74, 122, 176 ), g.left(), g.right() );
	// THE FLOOR: the same scan asked for a colour the bar cannot carry must find
	// nothing, so a scan that has quietly stopped finding anything cannot pass.
	const QRect none = colourBox( img, dpr, qRgb( 1, 254, 3 ), g.left(), g.right() );
	// and the unhovered neighbour must carry no highlight at all
	const QRect nb = colourBox( img, dpr, qRgb( 74, 122, 176 ),
		r.bar->actionGeometry( r.items.at( 0 ) ).left(),
		r.bar->actionGeometry( r.items.at( 0 ) ).right() );

	o << QStringLiteral( "=== %1\n" ).arg( tag );
	o << QStringLiteral( "  sheet on the menu bar: %1\n" ).arg( sheet );
	o << QStringLiteral( "  grab %1x%2 dpr %3 -> %4\n" )
		.arg( img.width() ).arg( img.height() ).arg( dpr ).arg( png );
	o << QStringLiteral( "  hovered \"%1\" by %2; activeAction = %3\n" )
		.arg( hovered->text().remove( QLatin1Char( '&' ) ) ).arg( how )
		.arg( r.bar->activeAction() ? r.bar->activeAction()->text().remove( QLatin1Char( '&' ) )
									: QStringLiteral( "(none)" ) );
	o << QStringLiteral( "  menu bar %1x%2, item rect x %3 y %4 w %5 h %6\n" )
		.arg( r.bar->width() ).arg( r.bar->height() )
		.arg( g.x() ).arg( g.y() ).arg( g.width() ).arg( g.height() );
	if ( hi.isNull() ) {
		o << QStringLiteral( "  *** NO HIGHLIGHT FOUND -- the picture is not the hovered state\n" );
	} else {
		o << QStringLiteral( "  HOVER BOX (measured from the grab): y %1..%2 -> %3 px tall,"
			" in a row of %4\n" )
			.arg( hi.top() ).arg( hi.bottom() ).arg( hi.height() ).arg( rowH );
	}
	o << QStringLiteral( "  floor: a colour the bar cannot carry -> %1 (must be none)\n" )
		.arg( none.isNull() ? QStringLiteral( "none" ) : QStringLiteral( "FOUND -- scan is wrong" ) );
	o << QStringLiteral( "  floor: the unhovered File title -> %1 (must be none)\n" )
		.arg( nb.isNull() ? QStringLiteral( "none" ) : QStringLiteral( "FOUND -- every item is lit" ) );
	// the x span the picture should crop to
	o << QStringLiteral( "  CROP-HINT titles x %1..%2\n" )
		.arg( r.bar->actionGeometry( r.items.first() ).left() )
		.arg( r.bar->actionGeometry( r.items.last() ).right() );
	o.flush();

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
		fprintf( stderr, "FATAL: release/style.qss not read -- run from the repo root\n" );
		return 2;
	}

	QTextStream o( stdout );
	o << QStringLiteral( "qApp style \"%1\"\n" ).arg( QApplication::style()->objectName() );

	/* The item's own content height, MEASURED the way wwAlignBarRow measures it
	 * (:922) -- the row sheet with both vertical paddings taken away,
	 * ensurePolished(), no event loop -- so the AFTER half's two paddings are
	 * derived here exactly as the application derives them, not typed. */
	int itemContent = 0;
	{
		Rig r = buildRig();
		r.win->show();
		settle();
		r.bar->setMinimumHeight( rowH );
		r.bar->setMaximumHeight( rowH );
		r.bar->setStyleSheet( boxQss() + btnQss( rowContent )
			+ QStringLiteral( "QMenuBar::item { padding-top: 0px; padding-bottom: 0px; }" ) );
		r.bar->ensurePolished();
		for ( QAction * a : r.items )
			itemContent = qMax( itemContent, r.bar->actionGeometry( a ).height() );
		o << QStringLiteral( "calibration: item content %1 px (no event loop), row %2"
			" -> padding-top %3 / bottom %4\n" )
			.arg( itemContent ).arg( rowH )
			.arg( ( rowH - itemContent ) / 2 ).arg( rowH - itemContent - ( rowH - itemContent ) / 2 );
		r.win->hide();
		delete r.win;
	}

	const int SPELLS = 2;

	// BEFORE: the row sheet as it stood before lane UI5 -- wwBarRowBoxQss +
	// wwBarRowButtonQss and NOTHING else on QMenuBar::item.
	half( o, QStringLiteral( "BEFORE lane UI5 -- min-height 26, padding 2/2 (item 20 px at the top)" ),
		boxQss() + btnQss( rowContent ),
		QStringLiteral( "scratchpad/ui5_20260910/images/hover_bar_before.png" ), SPELLS );

	// AFTER: the same, plus wwBarRowMenuItemQss appended last.
	half( o, QStringLiteral( "AFTER lane UI5 -- + wwBarRowMenuItemQss (item = the whole 35 px row)" ),
		boxQss() + btnQss( rowContent ) + menuItemQss( rowH, itemContent ),
		QStringLiteral( "scratchpad/ui5_20260910/images/hover_bar_after.png" ), SPELLS );

	o.flush();
	return 0;
}
