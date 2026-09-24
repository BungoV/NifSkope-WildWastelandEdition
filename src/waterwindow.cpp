/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "waterwindow.h"

#include "watercurves.h"
#include "watermark.h"
#include "wwskin.h"
#include "ui/widgets/wwnumberfield.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <climits>
#include <cmath>
#include <initializer_list>

/* =========================================================================
 *  The water window
 *
 *  bungo, 2026-09-10: *"do you draw it on that tiny map?"* -- *"just make it
 *  open a new popup window that can be set to full screen and you can drag
 *  that shows the flowmap"*; *"curves you can draw in nifskope, that can have
 *  as many connection points as you want. Then you solve the rest with a
 *  button"*; *"Add all the tools needed to mark the rivers and solve it and
 *  export import there, into that new window."*
 *
 *  BLENDER IS THE REFERENCE, and the divergences are stated up front:
 *
 *   - The interaction is Blender's CURVE EDIT MODE with the Curve Pen tool:
 *     click on nothing extrudes a point from the active end, click on a point
 *     selects it (Shift extends), drag moves the selection, Ctrl+click on a
 *     segment inserts a point there, X / Delete removes the selected points,
 *     Esc / Enter / right-click finishes the curve so the next click starts a
 *     new one.  DIVERGENCE: the points are polyline vertices, not Bezier
 *     control points with handles -- the solve interpolates the segment, and
 *     a handle would be a second thing to place for no second constraint.
 *   - Blender shows a curve's direction only when "Normals" overlay is on;
 *     here the arrows are always drawn (one at every segment's middle, one at
 *     the end), because the direction IS the mark.
 *   - Blender's per-point Radius (N panel > Item) is the analogue of the
 *     "Point weight" row: a scalar a point carries along the curve.  Here it
 *     weights the SPEED at that point (1 = the curve's own Speed).
 *   - "Switch Direction" is Blender's name for Reverse; it lives in the Curve
 *     row here, not a menu.
 *   - Blender's Image editor: middle-drag pans, the wheel zooms about the
 *     cursor, Home fits the whole image.  The same three, unchanged.
 *   - Blender's box select is a left-drag on nothing in the Select tool; the
 *     same, and the Draw tool's left-click on nothing extrudes instead, so the
 *     two tools differ only in what an empty click means -- as in Blender.
 *   - Full screen: Blender's Ctrl+Space maximises an area, F11 is the OS
 *     rule everywhere else; F11 here, plus the button.
 *   - Undo is Ctrl+Z / Ctrl+Shift+Z over the curve document only; the land
 *     file's undo is Reload.
 * ========================================================================= */

namespace {

// ---- colours (the dock's, repeated: one rule, two files) ---------------------

QColor bodyColour( quint16 id )
{
	if ( !id )
		return QColor( wwSkinColor( "viewport" ) );
	quint32 h = quint32( id ) * 2654435761u;
	const double hue = double( h % 3600u ) / 3600.0 * 360.0;
	const double sat = 0.45 + double( ( h >> 12 ) % 40u ) / 100.0;
	const double val = 0.55 + double( ( h >> 20 ) % 35u ) / 100.0;
	return QColor::fromHsvF( hue / 360.0, qBound( 0.0, sat, 1.0 ), qBound( 0.0, val, 1.0 ) );
}

QColor formColour( quint32 form )
{
	quint32 h = form * 2246822519u + 0x9E3779B9u;
	const double hue = double( h % 3600u ) / 3600.0;
	return QColor::fromHsvF( hue, 0.55, 0.7 );
}

//! Hue is the direction, brightness the speed step, saturation the confidence.
QColor flowColour( quint16 word )
{
	if ( !word )
		return QColor( 34, 48, 70 );
	const double dir = double( word & 0xFF ) / 256.0;
	const double speed = double( ( word >> 8 ) & 0xF ) / 15.0;
	const double conf = double( ( word >> 12 ) & 0xF ) / 15.0;
	return QColor::fromHsvF( dir, 0.45 + 0.55 * conf, 0.35 + 0.6 * speed );
}

//! A folding section (the dock's MarkSection shape), fold persisted.
class FoldSection final : public QWidget
{
public:
	FoldSection( const QString & title, const QString & key, QWidget * parent )
		: QWidget( parent ), settingsKey( QStringLiteral( "WaterWindow/expanded/" ) + key )
	{
		setObjectName( QStringLiteral( "WaterWindow" ) + key + QStringLiteral( "Section" ) );
		auto * v = new QVBoxLayout( this );
		v->setContentsMargins( 0, 0, 0, 0 );
		v->setSpacing( 4 );
		auto * header = new QHBoxLayout();
		header->setContentsMargins( 0, 0, 0, 0 );
		header->setSpacing( 2 );
		arrow = new QToolButton( this );
		arrow->setObjectName( QStringLiteral( "WaterWindow" ) + key + QStringLiteral( "Expander" ) );
		arrow->setAutoRaise( true );
		arrow->setFixedSize( 16, 16 );
		arrow->setToolTip( tr( "Show or hide these settings" ) );
		header->addWidget( arrow, 0 );
		header->addWidget( wwHeading( title, this ), 1 );
		v->addLayout( header );
		bodyWidget = new QWidget( this );
		bodyWidget->setObjectName( QStringLiteral( "WaterWindow" ) + key + QStringLiteral( "Body" ) );
		v->addWidget( bodyWidget );
		open = QSettings().value( settingsKey, true ).toBool();
		apply();
		connect( arrow, &QToolButton::clicked, this, [this]() {
			open = !open;
			QSettings().setValue( settingsKey, open );
			apply();
		} );
	}
	QWidget * body() const { return bodyWidget; }

private:
	void apply()
	{
		bodyWidget->setVisible( open );
		arrow->setArrowType( open ? Qt::DownArrow : Qt::RightArrow );
	}
	QToolButton * arrow = nullptr;
	QWidget * bodyWidget = nullptr;
	QString settingsKey;
	bool open = true;
};

class WaterWindow;

//! A selected point: which curve, which point.
struct PointRef
{
	int curve = -1;
	int point = -1;
	bool operator==( const PointRef & o ) const { return curve == o.curve && point == o.point; }
};

/*! The map: the worldspace from above, one plane at a time, and the surface
 *  the curves are edited on.
 *
 *  Two pictures: an OVERVIEW sampled once per open at ~1024 texels a side
 *  (the Commonwealth's body plane is 6144 across and 75 MB) drawn scaled
 *  while the view moves, and a DETAIL image re-sampled texel for texel under
 *  the widget once the view has settled and a texel is at least a pixel --
 *  which is what "zoomable to texel level" costs, and it is only ever the
 *  pixels on screen. */
class WaterMapView final : public QWidget
{
public:
	enum Tool { Draw = 0, Select = 1, SourcePin = 2, OutletPin = 3, DyePin = 4, Erase = 5 };
	enum Plane { BodyId = 0, Flow = 1, Shore = 2, Dye = 3, WaterType = 4, Imported = 5 };

	explicit WaterMapView( WaterWindow * owner, QWidget * parent );

	void setDoc( WaterMarkDoc * d, WaterCurveDoc * m );
	void rebuildOverview();
	void invalidateDetail() { detail = QImage(); detailTimer->start(); }
	void setPlane( int p ) { plane = p; rebuildOverview(); invalidateDetail(); update(); }
	int currentPlane() const { return plane; }
	void setTool( int t ) { tool = t; if ( t != Draw ) active = -1; update(); }
	int currentTool() const { return tool; }
	int selectedBody() const { return selected; }
	void setSelectedBody( int id ) { selected = id; update(); }
	int activeCurve() const { return active; }
	void setActiveCurve( int i ) { active = i; update(); }
	const QVector<PointRef> & selection() const { return sel; }
	void clearSelection() { sel.clear(); update(); }
	void selectWholeCurve( int i );
	void fit();
	//! Centre on a world point at `pixelsPerTexel` (the harness's zoom).
	void lookAt( double wx, double wy, double pixelsPerTexel );
	double pixelsPerTexel() const;
	//! Blender's View All.
	void keyAction( QKeyEvent * e );
	QString lastMessage() const { return message; }
	void say( const QString & m ) { message = m; update(); }
	QPointF worldToView( double wx, double wy ) const;
	void viewToWorld( const QPointF & p, double & wx, double & wy ) const;

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * e ) override;
	void mouseMoveEvent( QMouseEvent * e ) override;
	void mouseReleaseEvent( QMouseEvent * e ) override;
	void mouseDoubleClickEvent( QMouseEvent * e ) override;
	void wheelEvent( QWheelEvent * e ) override;
	void resizeEvent( QResizeEvent * ) override { invalidateDetail(); update(); }
	void keyPressEvent( QKeyEvent * e ) override { keyAction( e ); }

private:
	QColor colourAt( double wx, double wy ) const;
	void renderDetail();
	//! The point under a view position, within `radiusPx`; -1/-1 when none.
	PointRef pointAt( const QPointF & v, double radiusPx ) const;
	//! The segment under a view position: curve, the index of its first point, the parameter.
	bool segmentAt( const QPointF & v, double radiusPx, int & curve, int & seg, double & t ) const;
	bool isSelected( const PointRef & r ) const { return sel.contains( r ); }

	WaterWindow * win = nullptr;
	WaterMarkDoc * doc = nullptr;
	WaterCurveDoc * model = nullptr;
	QImage overview;
	int overviewStep = 1;
	QImage detail;                 //!< the widget's own pixels, texel-sampled
	double detailCx = 0.0, detailCy = 0.0, detailScale = 0.0;
	QTimer * detailTimer = nullptr;
	int plane = BodyId;
	int tool = Draw;
	int selected = 0;              //!< body id
	int active = -1;               //!< the curve the next Draw click extends
	QVector<PointRef> sel;
	double cx = 0.0, cy = 0.0;     //!< the world point at the widget's centre
	double scale = 0.0;            //!< widget pixels per world unit
	bool panning = false, dragging = false, boxing = false, moved = false;
	QPoint panFrom;
	QPointF dragFromView, boxFrom, boxTo;
	double dragWx = 0.0, dragWy = 0.0;
	QString message;
};

/*! The window.  House style throughout -- wwHeading for sections,
 *  wwMakeScrubField for every number, wwMatchFieldStyle for every selector,
 *  one setting a row in a label | field grid with one label width for the
 *  page, the explanation in the tooltip, the settings scrolling on the left,
 *  the map on the right, the summary and the buttons pinned under both. */
class WaterWindow final : public QWidget
{
public:
	explicit WaterWindow( QMainWindow * mw );
	~WaterWindow() override { delete doc; }

	WaterMarkDoc * document() const { return doc; }
	WaterCurveDoc & model() { return curves; }
	WaterMapView * map() const { return view; }
	QLabel * summaryLabel() const { return summary; }
	QScrollArea * settingsScroll() const { return scroll; }
	QPushButton * saveBtn() const { return saveButton; }
	QPushButton * solveBtn() const { return solveButton; }
	int selectedBody() const { return selected; }

	bool openFile( const QString & path );
	void selectBody( int id );
	//! Every edit of the curve document goes through here: undo, dirty, the sentence.
	void edited( const QString & what );
	void undo();
	void redo();
	bool solve();
	bool saveNow();
	void reload();
	bool saveCurves( const QString & path );
	bool loadCurves( const QString & path );
	bool exportPng( const QString & flowPng, const QString & maskPng, qint64 * wet );
	bool importPng( const QString & flowPng );
	void toggleFullScreen();
	void refreshSummary();
	void setRefusal( const QString & r ) { refusal = r; refreshSummary(); }
	double curveSpeed() const { return speedSpin->value(); }
	double curveWidth() const { return widthSpin->value(); }
	double pointWeight() const { return weightSpin->value(); }
	QColor dyeColour() const { return dyePick; }
	QString lastSolveNote() const { return lastSolve; }
	bool unsolved() const { return needSolve; }
	//! The point weight row shows the selection's weight; the row writes it back.
	void selectionChanged();

protected:
	void keyPressEvent( QKeyEvent * e ) override;
	void closeEvent( QCloseEvent * e ) override;

private:
	void applyWeightToSelection( double w );
	void reverseSelected();
	void deleteSelectedPoints();
	void finishCurve();

	QMainWindow * mainWindow = nullptr;
	WaterMarkDoc * doc = nullptr;
	WaterCurveDoc curves;
	QVector<WaterCurveDoc> undoStack, redoStack;
	WaterMapView * view = nullptr;
	QSplitter * splitter = nullptr;
	QScrollArea * scroll = nullptr;
	QLineEdit * fileEdit = nullptr;
	QLineEdit * nameEdit = nullptr;
	QComboBox * showBox = nullptr;
	QComboBox * toolBox = nullptr;
	QComboBox * classBox = nullptr;
	QComboBox * formBox = nullptr;
	QComboBox * flowRateBox = nullptr;
	QDoubleSpinBox * speedSpin = nullptr;
	QDoubleSpinBox * widthSpin = nullptr;
	QDoubleSpinBox * weightSpin = nullptr;
	QDoubleSpinBox * dyeFadeSpin = nullptr;
	QCheckBox * colourCheck = nullptr;
	QCheckBox * stillCheck = nullptr;
	QCheckBox * dyeMouthCheck = nullptr;
	QPushButton * colourButton = nullptr;
	QPushButton * dyeColourButton = nullptr;
	QPushButton * reverseButton = nullptr;
	QPushButton * finishButton = nullptr;
	QPushButton * deleteButton = nullptr;
	QPushButton * saveCurvesButton = nullptr;
	QPushButton * loadCurvesButton = nullptr;
	QPushButton * exportButton = nullptr;
	QPushButton * importButton = nullptr;
	QPushButton * reloadButton = nullptr;
	QPushButton * solveButton = nullptr;
	QPushButton * saveButton = nullptr;
	QPushButton * fullButton = nullptr;
	QLabel * summary = nullptr;
	FoldSection * filesSection = nullptr;
	QColor pick = QColor( 60, 130, 200 );
	QColor dyePick = QColor( 110, 170, 40 );
	QString refusal, lastSolve, lastEdit;
	int selected = 0;
	bool loadingBody = false, loadingWeight = false, needSolve = false, curvesDirty = false;
};

QPointer<WaterWindow> theWindow;

// =========================================================================
//  the map
// =========================================================================

WaterMapView::WaterMapView( WaterWindow * owner, QWidget * parent )
	: QWidget( parent ), win( owner )
{
	setObjectName( QStringLiteral( "WaterWindowMap" ) );
	setMinimumSize( 320, 240 );
	setMouseTracking( true );
	setFocusPolicy( Qt::StrongFocus );
	setToolTip( tr( "The worldspace from above, north up.\n"
		"Draw curve: click to add a point, click a point to select it, drag to move,\n"
		"Ctrl+click a segment to insert, Delete to remove, Enter or right-click to finish.\n"
		"Middle-drag pans, the wheel zooms about the cursor, Home fits the whole map." ) );
	detailTimer = new QTimer( this );
	detailTimer->setSingleShot( true );
	detailTimer->setInterval( 120 );
	connect( detailTimer, &QTimer::timeout, this, [this]() { renderDetail(); update(); } );
}

void WaterMapView::setDoc( WaterMarkDoc * d, WaterCurveDoc * m )
{
	doc = d;
	model = m;
	scale = 0.0;
	active = -1;
	sel.clear();
	rebuildOverview();
	invalidateDetail();
	update();
}

QColor WaterMapView::colourAt( double wx, double wy ) const
{
	const quint16 id = doc->bodyAtWorld( wx, wy );
	int px = 0, py = 0;
	doc->worldToTexel( wx, wy, px, py );
	switch ( plane ) {
	case Flow:
		return id ? flowColour( doc->flowWordAt( px, py ) ) : QColor( wwSkinColor( "viewport" ) );
	case Shore: {
		if ( !id )
			return QColor( wwSkinColor( "viewport" ) );
		const int s = int( doc->file()->shoreAt( px, py ) );
		return QColor( qBound( 0, s * 3, 255 ), qBound( 0, s * 3, 255 ), 255 );
	}
	case Dye: {
		if ( !id )
			return QColor( wwSkinColor( "viewport" ) );
		const quint32 d = doc->dyeWordAt( px, py );
		QColor base( 34, 48, 70 );
		if ( d ) {
			const quint32 src = d & 0xFFFF;
			const double wgt = double( ( d >> 16 ) & 0xFF ) / 255.0;
			QColor dye = bodyColour( quint16( src & 0x7FFF ) );
			if ( src & 0x8000 ) {
				int k = 0;
				for ( const WaterStroke & s : doc->strokes() )
					if ( s.kind == WaterStroke::DyePin && s.enabled() && k++ == int( src & 0x7FFF ) )
						dye = QColor( s.colour[0], s.colour[1], s.colour[2] );
			}
			base = QColor( int( base.red() + ( dye.red() - base.red() ) * wgt ),
				int( base.green() + ( dye.green() - base.green() ) * wgt ),
				int( base.blue() + ( dye.blue() - base.blue() ) * wgt ) );
		}
		return base;
	}
	case WaterType: {
		if ( !id )
			return QColor( wwSkinColor( "viewport" ) );
		LodtWaterBody b;
		return doc->body( int( id ), b ) ? formColour( b.watrForm ) : QColor( 34, 48, 70 );
	}
	case Imported: {
		if ( !id )
			return QColor( wwSkinColor( "viewport" ) );
		quint16 w = 0;
		if ( model && model->rasterWordAt( px, py, w ) )
			return flowColour( w );
		return QColor( 34, 48, 70 );
	}
	default:
		return bodyColour( id );
	}
}

void WaterMapView::rebuildOverview()
{
	overview = QImage();
	if ( !doc || !doc->isOpen() )
		return;
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	doc->worldBounds( x0, y0, x1, y1 );
	const double u = doc->worldPerTexel();
	const int tw = int( ( x1 - x0 ) / u ), th = int( ( y1 - y0 ) / u );
	if ( tw <= 0 || th <= 0 )
		return;
	const int target = 1024;
	overviewStep = qMax( 1, qMax( tw, th ) / target );
	const int iw = tw / overviewStep, ih = th / overviewStep;
	QImage img( iw, ih, QImage::Format_RGB32 );
	for ( int j = 0; j < ih; j++ ) {
		// row 0 of the plane is SOUTH; the image's row 0 is NORTH, so it mirrors
		QRgb * line = reinterpret_cast<QRgb *>( img.scanLine( ih - 1 - j ) );
		for ( int i = 0; i < iw; i++ ) {
			const double wx = x0 + ( double( i ) * overviewStep + 0.5 ) * u;
			const double wy = y0 + ( double( j ) * overviewStep + 0.5 ) * u;
			line[i] = colourAt( wx, wy ).rgb();
		}
	}
	overview = img;
}

void WaterMapView::renderDetail()
{
	detail = QImage();
	if ( !doc || !doc->isOpen() || scale <= 0.0 || width() <= 0 || height() <= 0 )
		return;
	const double u = doc->worldPerTexel();
	// only once a texel is at least a pixel: below that the overview is the honest picture
	if ( u * scale < 1.0 )
		return;
	if ( qsizetype( width() ) * height() > 4096 * 4096 )
		return;
	QImage img( width(), height(), QImage::Format_RGB32 );
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	doc->worldBounds( x0, y0, x1, y1 );
	const QRgb outside = QColor( wwSkinColor( "viewport" ) ).rgb();
	for ( int j = 0; j < height(); j++ ) {
		QRgb * line = reinterpret_cast<QRgb *>( img.scanLine( j ) );
		double wx = 0, wy = 0;
		viewToWorld( QPointF( 0.5, j + 0.5 ), wx, wy );
		const double dx = 1.0 / scale;
		int lastPx = INT_MIN, lastPy = INT_MIN;
		QRgb lastRgb = outside;
		for ( int i = 0; i < width(); i++, wx += dx ) {
			if ( wx < x0 || wx >= x1 || wy < y0 || wy >= y1 ) {
				line[i] = outside;
				continue;
			}
			int px = 0, py = 0;
			doc->worldToTexel( wx, wy, px, py );
			if ( px != lastPx || py != lastPy ) {
				lastRgb = colourAt( wx, wy ).rgb();
				lastPx = px;
				lastPy = py;
			}
			line[i] = lastRgb;
		}
	}
	detail = img;
	detailCx = cx;
	detailCy = cy;
	detailScale = scale;
}

QPointF WaterMapView::worldToView( double wx, double wy ) const
{
	return QPointF( double( width() ) * 0.5 + ( wx - cx ) * scale,
		double( height() ) * 0.5 - ( wy - cy ) * scale );
}

void WaterMapView::viewToWorld( const QPointF & p, double & wx, double & wy ) const
{
	wx = cx + ( p.x() - double( width() ) * 0.5 ) / scale;
	wy = cy - ( p.y() - double( height() ) * 0.5 ) / scale;
}

void WaterMapView::fit()
{
	if ( !doc || !doc->isOpen() )
		return;
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	doc->worldBounds( x0, y0, x1, y1 );
	cx = ( x0 + x1 ) * 0.5;
	cy = ( y0 + y1 ) * 0.5;
	const double sx = double( width() ) / qMax( 1.0, x1 - x0 );
	const double sy = double( height() ) / qMax( 1.0, y1 - y0 );
	scale = qMin( sx, sy );
	invalidateDetail();
	update();
}

void WaterMapView::lookAt( double wx, double wy, double pixelsPerTexel )
{
	if ( !doc || !doc->isOpen() )
		return;
	cx = wx;
	cy = wy;
	scale = pixelsPerTexel / doc->worldPerTexel();
	invalidateDetail();
	update();
}

double WaterMapView::pixelsPerTexel() const
{
	return ( doc && doc->isOpen() && scale > 0.0 ) ? scale * doc->worldPerTexel() : 0.0;
}

void WaterMapView::selectWholeCurve( int i )
{
	sel.clear();
	if ( model && i >= 0 && i < model->curves.size() )
		for ( int k = 0; k < model->curves[i].pts.size(); k++ )
			sel.append( PointRef{ i, k } );
	active = i;
	update();
	win->selectionChanged();
}

PointRef WaterMapView::pointAt( const QPointF & v, double radiusPx ) const
{
	PointRef best;
	double bestD = radiusPx;
	if ( !model )
		return best;
	for ( int i = 0; i < model->curves.size(); i++ ) {
		const WaterCurve & c = model->curves[i];
		for ( int k = 0; k < c.pts.size(); k++ ) {
			const QPointF q = worldToView( c.pts[k].x, c.pts[k].y );
			const double d = std::hypot( q.x() - v.x(), q.y() - v.y() );
			if ( d < bestD ) {
				bestD = d;
				best = PointRef{ i, k };
			}
		}
	}
	return best;
}

bool WaterMapView::segmentAt( const QPointF & v, double radiusPx, int & curve, int & seg, double & t ) const
{
	double bestD = radiusPx;
	bool found = false;
	if ( !model )
		return false;
	for ( int i = 0; i < model->curves.size(); i++ ) {
		const WaterCurve & c = model->curves[i];
		for ( int k = 0; k + 1 < c.pts.size(); k++ ) {
			const QPointF a = worldToView( c.pts[k].x, c.pts[k].y );
			const QPointF b = worldToView( c.pts[k + 1].x, c.pts[k + 1].y );
			const double ex = b.x() - a.x(), ey = b.y() - a.y();
			const double L2 = ex * ex + ey * ey;
			double u = L2 > 0.0 ? ( ( v.x() - a.x() ) * ex + ( v.y() - a.y() ) * ey ) / L2 : 0.0;
			u = qBound( 0.0, u, 1.0 );
			const double d = std::hypot( a.x() + ex * u - v.x(), a.y() + ey * u - v.y() );
			if ( d < bestD ) {
				bestD = d;
				curve = i;
				seg = k;
				t = u;
				found = true;
			}
		}
	}
	return found;
}

void WaterMapView::paintEvent( QPaintEvent * )
{
	QPainter p( this );
	p.fillRect( rect(), QColor( wwSkinColor( "viewport" ) ) );
	if ( !doc || !doc->isOpen() || overview.isNull() ) {
		p.setPen( QColor( wwSkinColor( "textMuted" ) ) );
		p.drawText( rect(), Qt::AlignCenter, tr( "no landscape file" ) );
		return;
	}
	if ( scale <= 0.0 )
		fit();
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	doc->worldBounds( x0, y0, x1, y1 );
	const QPointF tl = worldToView( x0, y1 );
	const QPointF br = worldToView( x1, y0 );
	p.setRenderHint( QPainter::SmoothPixmapTransform, false );
	if ( !detail.isNull() && detailCx == cx && detailCy == cy && detailScale == scale
		&& detail.size() == size() )
		p.drawImage( QPoint( 0, 0 ), detail );
	else
		p.drawImage( QRectF( tl, br ), overview );
	p.setPen( QColor( wwSkinColor( "borderDim" ) ) );
	p.drawRect( QRectF( tl, br ) );

	// a cell grid once a cell is wider than 48 px: the map's own ruler
	if ( scale * 4096.0 > 48.0 ) {
		QColor g( wwSkinColor( "borderDim" ) );
		g.setAlpha( 90 );
		p.setPen( QPen( g, 1.0 ) );
		double wx0 = 0, wy0 = 0, wx1 = 0, wy1 = 0;
		viewToWorld( QPointF( 0, height() ), wx0, wy0 );
		viewToWorld( QPointF( width(), 0 ), wx1, wy1 );
		for ( double x = std::floor( wx0 / 4096.0 ) * 4096.0; x <= wx1; x += 4096.0 ) {
			const double vx = worldToView( x, 0 ).x();
			p.drawLine( QPointF( vx, 0 ), QPointF( vx, height() ) );
		}
		for ( double y = std::floor( wy0 / 4096.0 ) * 4096.0; y <= wy1; y += 4096.0 ) {
			const double vy = worldToView( 0, y ).y();
			p.drawLine( QPointF( 0, vy ), QPointF( width(), vy ) );
		}
	}

	auto arrowHead = [&]( const QPointF & at, double ang, double L, const QColor & col ) {
		QPolygonF head;
		head << at
			<< QPointF( at.x() - L * std::cos( ang - 0.45 ), at.y() - L * std::sin( ang - 0.45 ) )
			<< QPointF( at.x() - L * std::cos( ang + 0.45 ), at.y() - L * std::sin( ang + 0.45 ) );
		p.setPen( Qt::NoPen );
		p.setBrush( col );
		p.drawPolygon( head );
		p.setBrush( Qt::NoBrush );
	};
	const QColor accent( wwSkinColor( "accent" ) );
	const QColor focus( wwSkinColor( "focus" ) );
	const QColor text( wwSkinColor( "text" ) );
	if ( model ) {
		for ( int i = 0; i < model->curves.size(); i++ ) {
			const WaterCurve & c = model->curves[i];
			if ( c.pts.isEmpty() )
				continue;
			QColor col = accent;
			if ( c.kind == WaterCurve::SourcePin )
				col = QColor( 120, 220, 140 );
			else if ( c.kind == WaterCurve::OutletPin )
				col = QColor( 240, 160, 90 );
			else if ( c.kind == WaterCurve::DyePin )
				col = QColor( c.colour[0], c.colour[1], c.colour[2] );
			if ( !c.enabled )
				col = QColor( wwSkinColor( "textDisabled" ) );
			if ( i == active )
				col = col.lighter( 135 );
			QPolygonF poly;
			for ( const WaterCurvePoint & q : c.pts )
				poly << worldToView( q.x, q.y );
			// the influence width, faintly, so a person sees what the solve is told
			if ( poly.size() >= 2 && c.kind == WaterCurve::Curve ) {
				QColor band = col;
				band.setAlpha( 40 );
				p.setPen( QPen( band, qMax( 1.0, double( c.width ) * scale ), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
				p.drawPolyline( poly );
			}
			p.setPen( QPen( col, i == active ? 2.5 : 2.0 ) );
			if ( poly.size() >= 2 )
				p.drawPolyline( poly );
			// direction: an arrow at every segment's middle, and at the end
			for ( int k = 0; k + 1 < poly.size(); k++ ) {
				const QPointF a = poly[k], b = poly[k + 1];
				const double ang = std::atan2( b.y() - a.y(), b.x() - a.x() );
				const double segLen = std::hypot( b.x() - a.x(), b.y() - a.y() );
				if ( segLen > 24.0 )
					arrowHead( ( a + b ) * 0.5, ang, 7.0, col );
				if ( k + 2 == poly.size() )
					arrowHead( b, ang, 11.0, col );
			}
			// the points: Blender's vertices, filled when selected
			for ( int k = 0; k < poly.size(); k++ ) {
				const bool on = isSelected( PointRef{ i, k } );
				p.setPen( QPen( on ? focus : col, 1.5 ) );
				p.setBrush( on ? focus : QColor( wwSkinColor( "bg" ) ) );
				const double r = c.onePoint() || c.pts.size() == 1 ? 5.0 : 3.5;
				if ( c.kind == WaterCurve::Curve && c.pts.size() > 1 )
					p.drawRect( QRectF( poly[k].x() - r, poly[k].y() - r, 2 * r, 2 * r ) );
				else
					p.drawEllipse( poly[k], r, r );
				p.setBrush( Qt::NoBrush );
			}
		}
	}
	if ( boxing ) {
		QColor b = focus;
		b.setAlpha( 50 );
		p.setPen( QPen( focus, 1.0, Qt::DashLine ) );
		p.setBrush( b );
		p.drawRect( QRectF( boxFrom, boxTo ).normalized() );
		p.setBrush( Qt::NoBrush );
	}
	if ( !message.isEmpty() ) {
		p.setPen( text );
		p.drawText( rect().adjusted( 8, 6, -8, -6 ), Qt::AlignTop | Qt::AlignLeft, message );
	}
	// the scale, bottom left: one cell, so the zoom reads as a number
	{
		const double cellPx = scale * 4096.0;
		const QString s = tr( "one cell = %1 px, %2 px a texel" ).arg( cellPx, 0, 'f', 1 )
			.arg( pixelsPerTexel(), 0, 'f', 2 );
		p.setPen( QColor( wwSkinColor( "textMuted" ) ) );
		p.drawText( rect().adjusted( 8, 6, -8, -6 ), Qt::AlignBottom | Qt::AlignLeft, s );
	}
}

void WaterMapView::mousePressEvent( QMouseEvent * e )
{
	setFocus();
	if ( !doc || !doc->isOpen() || !model )
		return;
	if ( scale <= 0.0 )
		fit();
	if ( e->button() == Qt::MiddleButton ) {
		panning = true;
		panFrom = e->pos();
		return;
	}
	if ( e->button() == Qt::RightButton ) {
		if ( tool == Draw && active >= 0 ) {
			active = -1;
			message = tr( "curve finished; the next click starts a new one" );
			update();
		}
		return;
	}
	if ( e->button() != Qt::LeftButton )
		return;
	double wx = 0, wy = 0;
	viewToWorld( e->position(), wx, wy );
	const bool shift = e->modifiers() & Qt::ShiftModifier;
	const bool ctrl = e->modifiers() & Qt::ControlModifier;
	moved = false;

	if ( tool == Erase ) {
		int ci = -1, seg = 0;
		double t = 0;
		PointRef pr = pointAt( e->position(), 10.0 );
		if ( pr.curve < 0 && segmentAt( e->position(), 8.0, ci, seg, t ) )
			pr.curve = ci;
		if ( pr.curve >= 0 ) {
			model->curves.removeAt( pr.curve );
			sel.clear();
			active = -1;
			win->edited( tr( "one curve removed" ) );
			message = tr( "one curve removed" );
		} else {
			message = tr( "no curve here" );
		}
		update();
		return;
	}
	if ( tool == SourcePin || tool == OutletPin || tool == DyePin ) {
		const quint16 id = doc->bodyAtWorld( wx, wy );
		if ( !id ) {
			message = tr( "that is dry land; a pin goes on the water" );
			update();
			return;
		}
		WaterCurve c;
		c.kind = tool == SourcePin ? WaterCurve::SourcePin
			: tool == OutletPin ? WaterCurve::OutletPin : WaterCurve::DyePin;
		c.body = id;
		c.width = float( win->curveWidth() );
		c.speed = float( tool == DyePin ? 1.0 : win->curveSpeed() );
		if ( tool == DyePin ) {
			const QColor dc = win->dyeColour();
			c.colour[0] = quint8( dc.red() );
			c.colour[1] = quint8( dc.green() );
			c.colour[2] = quint8( dc.blue() );
			c.colour[3] = 255;
		}
		c.pts.append( WaterCurvePoint{ float( wx ), float( wy ), 1.0f } );
		model->curves.append( c );
		win->selectBody( int( id ) );
		selectWholeCurve( model->curves.size() - 1 );
		win->edited( tr( "pin placed on body %1" ).arg( id ) );
		message = tr( "pin on body %1" ).arg( id );
		update();
		return;
	}

	// Draw and Select share the point and segment hits
	const PointRef hit = pointAt( e->position(), 9.0 );
	if ( hit.curve >= 0 ) {
		if ( shift ) {
			if ( isSelected( hit ) )
				sel.removeAll( hit );
			else
				sel.append( hit );
		} else if ( !isSelected( hit ) ) {
			sel.clear();
			sel.append( hit );
		}
		active = hit.curve;
		if ( model->curves[hit.curve].body )
			win->selectBody( int( model->curves[hit.curve].body ) );
		dragging = true;
		dragFromView = e->position();
		dragWx = wx;
		dragWy = wy;
		win->selectionChanged();
		update();
		return;
	}
	int ci = -1, seg = 0;
	double t = 0;
	if ( segmentAt( e->position(), 8.0, ci, seg, t ) ) {
		if ( ctrl ) {
			// Curve Pen's insert: a point on the segment, its weight interpolated
			WaterCurve & c = model->curves[ci];
			const WaterCurvePoint & a = c.pts[seg];
			const WaterCurvePoint & b = c.pts[seg + 1];
			WaterCurvePoint q;
			q.x = float( a.x + ( b.x - a.x ) * t );
			q.y = float( a.y + ( b.y - a.y ) * t );
			q.w = float( a.w + ( b.w - a.w ) * t );
			c.pts.insert( seg + 1, q );
			sel.clear();
			sel.append( PointRef{ ci, seg + 1 } );
			active = ci;
			win->edited( tr( "point inserted" ) );
			win->selectionChanged();
			message = tr( "point inserted on curve %1" ).arg( ci + 1 );
		} else {
			selectWholeCurve( ci );
			if ( model->curves[ci].body )
				win->selectBody( int( model->curves[ci].body ) );
			message = tr( "curve %1 selected (%2 points)" ).arg( ci + 1 ).arg( model->curves[ci].pts.size() );
		}
		update();
		return;
	}

	// nothing under the cursor
	const quint16 id = doc->bodyAtWorld( wx, wy );
	if ( tool == Select ) {
		if ( id )
			win->selectBody( int( id ) );
		if ( !shift )
			sel.clear();
		boxing = true;
		boxFrom = boxTo = e->position();
		win->selectionChanged();
		update();
		return;
	}
	// Draw: extrude from the active curve, or start one on the body under the click
	if ( active < 0 || active >= model->curves.size()
		|| model->curves[active].kind != WaterCurve::Curve ) {
		if ( !id ) {
			message = tr( "that is dry land; a curve starts on the water" );
			update();
			return;
		}
		WaterCurve c;
		c.kind = WaterCurve::Curve;
		c.body = id;
		c.speed = float( win->curveSpeed() );
		c.width = float( win->curveWidth() );
		model->curves.append( c );
		active = model->curves.size() - 1;
		win->selectBody( int( id ) );
	}
	WaterCurve & c = model->curves[active];
	c.pts.append( WaterCurvePoint{ float( wx ), float( wy ), float( win->pointWeight() ) } );
	sel.clear();
	sel.append( PointRef{ active, int( c.pts.size() ) - 1 } );
	win->edited( tr( "point added" ) );
	win->selectionChanged();
	message = c.pts.size() == 1
		? tr( "curve %1 started on body %2; keep clicking to add points, Enter to finish" )
			.arg( active + 1 ).arg( c.body )
		: tr( "curve %1: %2 points" ).arg( active + 1 ).arg( c.pts.size() );
	if ( id && id != c.body )
		message += tr( " (this point is on body %1, outside curve %2's body %3; the solve ignores it)" )
			.arg( id ).arg( active + 1 ).arg( c.body );
	update();
}

void WaterMapView::mouseMoveEvent( QMouseEvent * e )
{
	if ( panning ) {
		const QPoint d = e->pos() - panFrom;
		panFrom = e->pos();
		cx -= double( d.x() ) / scale;
		cy += double( d.y() ) / scale;
		invalidateDetail();
		update();
		return;
	}
	if ( boxing ) {
		boxTo = e->position();
		update();
		return;
	}
	if ( !dragging || !model )
		return;
	double wx = 0, wy = 0;
	viewToWorld( e->position(), wx, wy );
	const double dx = wx - dragWx, dy = wy - dragWy;
	if ( !moved && std::hypot( e->position().x() - dragFromView.x(), e->position().y() - dragFromView.y() ) < 3.0 )
		return;
	moved = true;
	for ( const PointRef & r : sel )
		if ( r.curve < model->curves.size() && r.point < model->curves[r.curve].pts.size() ) {
			model->curves[r.curve].pts[r.point].x += float( dx );
			model->curves[r.curve].pts[r.point].y += float( dy );
		}
	dragWx = wx;
	dragWy = wy;
	update();
}

void WaterMapView::mouseReleaseEvent( QMouseEvent * e )
{
	if ( e->button() == Qt::MiddleButton ) {
		panning = false;
		return;
	}
	if ( e->button() != Qt::LeftButton )
		return;
	if ( boxing ) {
		boxing = false;
		const QRectF box = QRectF( boxFrom, boxTo ).normalized();
		if ( model && box.width() > 2 && box.height() > 2 ) {
			int n = 0;
			for ( int i = 0; i < model->curves.size(); i++ )
				for ( int k = 0; k < model->curves[i].pts.size(); k++ )
					if ( box.contains( worldToView( model->curves[i].pts[k].x, model->curves[i].pts[k].y ) ) ) {
						const PointRef r{ i, k };
						if ( !isSelected( r ) )
							sel.append( r );
						n++;
					}
			message = tr( "%1 point(s) selected" ).arg( n );
		}
		win->selectionChanged();
		update();
		return;
	}
	if ( dragging ) {
		dragging = false;
		if ( moved )
			win->edited( tr( "%1 point(s) moved" ).arg( sel.size() ) );
		update();
	}
}

void WaterMapView::mouseDoubleClickEvent( QMouseEvent * e )
{
	if ( e->button() == Qt::LeftButton && tool == Draw && active >= 0 ) {
		active = -1;
		message = tr( "curve finished; the next click starts a new one" );
		update();
	}
}

void WaterMapView::wheelEvent( QWheelEvent * e )
{
	if ( !doc || !doc->isOpen() )
		return;
	if ( scale <= 0.0 )
		fit();
	double wx = 0, wy = 0;
	viewToWorld( e->position(), wx, wy );
	const double f = e->angleDelta().y() > 0 ? 1.25 : 0.8;
	// bounded: the whole worldspace at a tenth of the widget, or 64 px a texel
	const double u = doc->worldPerTexel();
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	doc->worldBounds( x0, y0, x1, y1 );
	const double minScale = 0.1 * qMin( double( width() ) / ( x1 - x0 ), double( height() ) / ( y1 - y0 ) );
	scale = qBound( minScale, scale * f, 64.0 / u );
	// keep the point under the cursor where it was: Blender's zoom
	double nx = 0, ny = 0;
	viewToWorld( e->position(), nx, ny );
	cx += wx - nx;
	cy += wy - ny;
	invalidateDetail();
	update();
	e->accept();
}

void WaterMapView::keyAction( QKeyEvent * e )
{
	switch ( e->key() ) {
	case Qt::Key_Home:
		fit();
		break;
	case Qt::Key_Escape:
	case Qt::Key_Return:
	case Qt::Key_Enter:
		if ( active >= 0 ) {
			active = -1;
			message = tr( "curve finished; the next click starts a new one" );
		} else {
			sel.clear();
			win->selectionChanged();
		}
		update();
		break;
	case Qt::Key_A:
		if ( e->modifiers() & Qt::AltModifier ) {
			sel.clear();
		} else if ( model ) {
			sel.clear();
			for ( int i = 0; i < model->curves.size(); i++ )
				for ( int k = 0; k < model->curves[i].pts.size(); k++ )
					sel.append( PointRef{ i, k } );
		}
		win->selectionChanged();
		update();
		break;
	default:
		e->ignore();
		return;
	}
	e->accept();
}

// =========================================================================
//  the window
// =========================================================================

WaterWindow::WaterWindow( QMainWindow * mw )
	: QWidget( nullptr, Qt::Window ), mainWindow( mw )
{
	setObjectName( QStringLiteral( "WaterWindow" ) );
	setWindowTitle( tr( "Water" ) );
	setAttribute( Qt::WA_DeleteOnClose, false );
	resize( 1280, 800 );

	auto * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->setSpacing( 0 );
	splitter = new QSplitter( Qt::Horizontal, this );
	splitter->setObjectName( QStringLiteral( "WaterWindowSplitter" ) );
	splitter->setChildrenCollapsible( false );
	outer->addWidget( splitter, 1 );

	scroll = new QScrollArea( splitter );
	scroll->setObjectName( QStringLiteral( "WaterWindowSettingsScroll" ) );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	scroll->setMinimumWidth( 300 );
	auto * page = new QWidget( scroll );
	page->setObjectName( QStringLiteral( "WaterWindowSettingsPage" ) );
	auto * layout = new QVBoxLayout( page );
	layout->setContentsMargins( 6, 6, 6, 6 );
	layout->setSpacing( 5 );
	scroll->setWidget( page );
	splitter->addWidget( scroll );

	struct Form
	{
		QGridLayout * g = nullptr;
		int row = 0;
		QLabel * add( QWidget * parent, const QString & label, QWidget * field )
		{
			auto * l = new QLabel( label, parent );
			g->addWidget( l, row, 0 );
			g->addWidget( field, row++, 1 );
			return l;
		}
	};
	const int labelW = 132;      // fits "Flow samples per cell" under its indent
	auto form = [labelW]( int indent ) {
		Form f;
		f.g = new QGridLayout();
		f.g->setContentsMargins( indent, 0, 0, 0 );
		f.g->setHorizontalSpacing( 8 );
		f.g->setVerticalSpacing( 4 );
		f.g->setColumnMinimumWidth( 0, labelW - indent );
		f.g->setColumnStretch( 1, 1 );
		return f;
	};
	auto hostRow = [&]( QWidget * parent, std::initializer_list<QWidget *> ws ) {
		auto * host = new QWidget( parent );
		auto * h = new QHBoxLayout( host );
		h->setContentsMargins( 0, 0, 0, 0 );
		h->setSpacing( 4 );
		for ( QWidget * w : ws ) {
			w->setParent( host );
			h->addWidget( w, 1 );
		}
		return host;
	};

	// ---- Landscape file --------------------------------------------------
	layout->addWidget( wwHeading( tr( "Landscape file" ), page ) );
	Form src = form( 0 );
	fileEdit = new QLineEdit( page );
	fileEdit->setObjectName( QStringLiteral( "WaterWindowFileEdit" ) );
	fileEdit->setPlaceholderText( tr( "a version 3 .lodl" ) );
	fileEdit->setToolTip( tr( "The whole-worldspace landscape file whose water is being marked.\n"
		"It must carry the version 3 water sections, which the generator writes\n"
		"with Water bodies ticked. Its curves file, <Worldspace>.water.json, sits beside it." ) );
	wwMatchFieldStyle( fileEdit );
	{
		auto * browse = new QPushButton( tr( "Browse" ), page );
		browse->setObjectName( QStringLiteral( "WaterWindowBrowseButton" ) );
		browse->setToolTip( tr( "Choose the landscape file to mark" ) );
		connect( browse, &QPushButton::clicked, this, [this]() {
			const QString f = QFileDialog::getOpenFileName( this, tr( "Landscape file" ),
				fileEdit->text(), tr( "Landscape (*.lodl)" ) );
			if ( !f.isEmpty() )
				openFile( f );
		} );
		auto * host = new QWidget( page );
		auto * h = new QHBoxLayout( host );
		h->setContentsMargins( 0, 0, 0, 0 );
		h->setSpacing( 4 );
		h->addWidget( fileEdit, 1 );
		h->addWidget( browse, 0 );
		src.add( page, tr( "File" ), host );
	}
	showBox = new QComboBox( page );
	showBox->setObjectName( QStringLiteral( "WaterWindowShowBox" ) );
	showBox->addItem( tr( "Body ID" ), WaterMapView::BodyId );
	showBox->addItem( tr( "Flow" ), WaterMapView::Flow );
	showBox->addItem( tr( "Shore distance" ), WaterMapView::Shore );
	showBox->addItem( tr( "Dye" ), WaterMapView::Dye );
	showBox->addItem( tr( "Water type" ), WaterMapView::WaterType );
	showBox->addItem( tr( "Imported flow" ), WaterMapView::Imported );
	showBox->setToolTip( tr( "Which plane the map paints. Body ID gives every body its own colour;\n"
		"Flow paints the direction as hue, the speed as brightness and the\n"
		"confidence as saturation; Water type paints each body's water form;\n"
		"Imported flow paints the flow maps read in from PNG." ) );
	wwMatchFieldStyle( showBox );
	src.add( page, tr( "Show" ), showBox );
	layout->addLayout( src.g );

	// ---- Curves -----------------------------------------------------------
	layout->addWidget( wwHeading( tr( "Curves" ), page ) );
	Form cv = form( 0 );
	toolBox = new QComboBox( page );
	toolBox->setObjectName( QStringLiteral( "WaterWindowToolBox" ) );
	toolBox->addItem( tr( "Draw curve" ), WaterMapView::Draw );
	toolBox->addItem( tr( "Select" ), WaterMapView::Select );
	toolBox->addItem( tr( "Source pin" ), WaterMapView::SourcePin );
	toolBox->addItem( tr( "Outlet pin" ), WaterMapView::OutletPin );
	toolBox->addItem( tr( "Dye pin" ), WaterMapView::DyePin );
	toolBox->addItem( tr( "Erase" ), WaterMapView::Erase );
	toolBox->setToolTip( tr( "Draw curve: each click adds a point down the water, as many as you want;\n"
		"the curve's direction is the flow. Click a point to select it, drag to move it,\n"
		"Ctrl+click a segment to insert a point, Delete removes the selected points,\n"
		"Enter or a right-click finishes the curve. One point alone is a pin.\n"
		"Select: click or box-select points without adding any.\n"
		"Source and Outlet pins: the head and the mouth; the pair is the path.\n"
		"Dye pin: where something enters the water; its colour is carried downstream.\n"
		"Erase: click a curve to remove it whole." ) );
	wwMatchFieldStyle( toolBox );
	cv.add( page, tr( "Tool" ), toolBox );

	speedSpin = new QDoubleSpinBox( page );
	speedSpin->setObjectName( QStringLiteral( "WaterWindowSpeedSpin" ) );
	speedSpin->setRange( 0.0, 100.0 );
	speedSpin->setDecimals( 3 );
	speedSpin->setSingleStep( 0.05 );
	speedSpin->setValue( 0.25 );
	speedSpin->setToolTip( tr( "How fast the water moves along a new curve, in world units a second,\n"
		"the unit the game's own water form uses for its linear velocity.\n"
		"Each point's weight multiplies it." ) );
	wwMakeScrubField( speedSpin );
	cv.add( page, tr( "Speed" ), speedSpin );

	widthSpin = new QDoubleSpinBox( page );
	widthSpin->setObjectName( QStringLiteral( "WaterWindowWidthSpin" ) );
	widthSpin->setRange( 128.0, 65536.0 );
	widthSpin->setDecimals( 0 );
	widthSpin->setSingleStep( 128.0 );
	widthSpin->setValue( 4096.0 );
	widthSpin->setToolTip( tr( "How far from a new curve the solve prefers its direction, in world\n"
		"units. One cell is 4096. The faint band along a curve is this width." ) );
	wwMakeScrubField( widthSpin );
	cv.add( page, tr( "Width" ), widthSpin );

	weightSpin = new QDoubleSpinBox( page );
	weightSpin->setObjectName( QStringLiteral( "WaterWindowWeightSpin" ) );
	weightSpin->setRange( 0.0, 10.0 );
	weightSpin->setDecimals( 2 );
	weightSpin->setSingleStep( 0.05 );
	weightSpin->setValue( 1.0 );
	weightSpin->setToolTip( tr( "The speed weight of the selected point(s): 1 is the curve's own speed,\n"
		"2 twice it, 0.5 half. A new point takes this value. Blender's per-point\n"
		"Radius is the same idea." ) );
	wwMakeScrubField( weightSpin );
	cv.add( page, tr( "Point weight" ), weightSpin );

	reverseButton = new QPushButton( tr( "Reverse" ), page );
	reverseButton->setObjectName( QStringLiteral( "WaterWindowReverseButton" ) );
	reverseButton->setToolTip( tr( "Switch the direction of the selected curve(s): the water flows the other way." ) );
	finishButton = new QPushButton( tr( "Finish" ), page );
	finishButton->setObjectName( QStringLiteral( "WaterWindowFinishButton" ) );
	finishButton->setToolTip( tr( "End the curve being drawn; the next click starts a new one (Enter does the same)." ) );
	deleteButton = new QPushButton( tr( "Delete" ), page );
	deleteButton->setObjectName( QStringLiteral( "WaterWindowDeleteButton" ) );
	deleteButton->setToolTip( tr( "Remove the selected point(s); a curve with no points left goes with them (Delete or X)." ) );
	cv.add( page, tr( "Curve" ), hostRow( page, { reverseButton, finishButton, deleteButton } ) );
	layout->addLayout( cv.g );

	// ---- Selected body ------------------------------------------------------
	layout->addWidget( wwHeading( tr( "Selected body" ), page ) );
	Form sel = form( 0 );
	classBox = new QComboBox( page );
	classBox->setObjectName( QStringLiteral( "WaterWindowClassBox" ) );
	classBox->addItem( tr( "Automatic" ), -1 );
	classBox->addItem( tr( "Sea" ), 0 );
	classBox->addItem( tr( "River" ), 1 );
	classBox->addItem( tr( "Lake" ), 2 );
	classBox->setToolTip( tr( "What this body is. Automatic keeps the answer the generator\n"
		"measured from its shape and its neighbours." ) );
	wwMatchFieldStyle( classBox );
	sel.add( page, tr( "Class" ), classBox );

	formBox = new QComboBox( page );
	formBox->setObjectName( QStringLiteral( "WaterWindowFormBox" ) );
	formBox->setToolTip( tr( "The water form this body uses, from the ones the file interned out of\n"
		"the plugin's WATR list plus the worldspace default. It carries the\n"
		"colours, the fog and the noise." ) );
	wwMatchFieldStyle( formBox );
	sel.add( page, tr( "Water form" ), formBox );

	colourCheck = new QCheckBox( tr( "Override" ), page );
	colourCheck->setObjectName( QStringLiteral( "WaterWindowColourCheck" ) );
	colourCheck->setToolTip( tr( "Give this one body its own colour instead of the one its\n"
		"water form gives every body that shares it." ) );
	colourButton = new QPushButton( tr( "Choose" ), page );
	colourButton->setObjectName( QStringLiteral( "WaterWindowColourButton" ) );
	colourButton->setToolTip( tr( "Pick this body's colour" ) );
	sel.add( page, tr( "Colour" ), hostRow( page, { colourCheck, colourButton } ) );

	stillCheck = new QCheckBox( tr( "Still water" ), page );
	stillCheck->setObjectName( QStringLiteral( "WaterWindowStillCheck" ) );
	stillCheck->setToolTip( tr( "This body has no flow at all. A lake that no river reaches\n"
		"does not move, and this beats the velocity its water form carries." ) );
	sel.add( page, tr( "Flow" ), stillCheck );

	dyeMouthCheck = new QCheckBox( tr( "Dye at mouth" ), page );
	dyeMouthCheck->setObjectName( QStringLiteral( "WaterWindowDyeMouthCheck" ) );
	dyeMouthCheck->setToolTip( tr( "This body's water keeps its own colour past its mouth, as a plume\n"
		"into the body it drains into that fades over the Dye fade distance." ) );
	sel.add( page, tr( "Dye" ), dyeMouthCheck );

	nameEdit = new QLineEdit( page );
	nameEdit->setObjectName( QStringLiteral( "WaterWindowNameEdit" ) );
	nameEdit->setToolTip( tr( "A name for this body, stored in the file and in the curves file.\n"
		"Nothing reads it but a person." ) );
	wwMatchFieldStyle( nameEdit );
	sel.add( page, tr( "Name" ), nameEdit );
	layout->addLayout( sel.g );

	// ---- Dye ----------------------------------------------------------------
	layout->addWidget( wwHeading( tr( "Dye" ), page ) );
	Form dy = form( 0 );
	dyeColourButton = new QPushButton( tr( "Choose" ), page );
	dyeColourButton->setObjectName( QStringLiteral( "WaterWindowDyeColourButton" ) );
	dyeColourButton->setToolTip( tr( "The colour the next Dye pin releases into the water." ) );
	dy.add( page, tr( "Dye colour" ), dyeColourButton );
	dyeFadeSpin = new QDoubleSpinBox( page );
	dyeFadeSpin->setObjectName( QStringLiteral( "WaterWindowDyeFadeSpin" ) );
	dyeFadeSpin->setRange( 256.0, 262144.0 );
	dyeFadeSpin->setDecimals( 0 );
	dyeFadeSpin->setSingleStep( 512.0 );
	dyeFadeSpin->setValue( WaterMarkDoc::kDyeHalfDistanceDefault );
	dyeFadeSpin->setToolTip( tr( "How far along the flow a dye travels before half of it is gone, in\n"
		"world units; one cell is 4096. One distance serves every dye in the file." ) );
	wwMakeScrubField( dyeFadeSpin );
	dy.add( page, tr( "Dye fade" ), dyeFadeSpin );
	layout->addLayout( dy.g );

	// ---- Files (folds) -------------------------------------------------------
	filesSection = new FoldSection( tr( "Files" ), QStringLiteral( "Files" ), page );
	{
		auto * bodyW = filesSection->body();
		auto * bv = new QVBoxLayout( bodyW );
		bv->setContentsMargins( 0, 0, 0, 0 );
		bv->setSpacing( 4 );
		Form fl = form( 18 );
		saveCurvesButton = new QPushButton( tr( "Save curves" ), bodyW );
		saveCurvesButton->setObjectName( QStringLiteral( "WaterWindowSaveCurvesButton" ) );
		saveCurvesButton->setToolTip( tr( "Write every curve, pin and body setting to a <Worldspace>.water.json\n"
			"beside the landscape file, in world coordinates, so they can be loaded\n"
			"onto any regeneration of the same worldspace and edited again." ) );
		loadCurvesButton = new QPushButton( tr( "Load curves" ), bodyW );
		loadCurvesButton->setObjectName( QStringLiteral( "WaterWindowLoadCurvesButton" ) );
		loadCurvesButton->setToolTip( tr( "Read a curves file onto the open landscape file, replacing the\n"
			"curves shown; Solve then re-derives the planes from them." ) );
		fl.add( bodyW, tr( "Curves file" ), hostRow( bodyW, { saveCurvesButton, loadCurvesButton } ) );
		exportButton = new QPushButton( tr( "Export PNG" ), bodyW );
		exportButton->setObjectName( QStringLiteral( "WaterWindowExportButton" ) );
		exportButton->setToolTip( tr( "Write the flow map as a PNG at the file's own texel grid: red and\n"
			"green the direction (+green = north), blue the speed, alpha the\n"
			"confidence; and the body mask beside it as 16-bit grey." ) );
		importButton = new QPushButton( tr( "Import PNG" ), bodyW );
		importButton->setObjectName( QStringLiteral( "WaterWindowImportButton" ) );
		importButton->setToolTip( tr( "Read a flow map PNG at the file's grid as a layer that has authority\n"
			"where it is painted. A map whose green runs the wrong way is refused." ) );
		fl.add( bodyW, tr( "Flow map" ), hostRow( bodyW, { exportButton, importButton } ) );
		flowRateBox = new QComboBox( bodyW );
		flowRateBox->setObjectName( QStringLiteral( "WaterWindowFlowRateBox" ) );
		flowRateBox->addItem( tr( "8" ), 8 );
		flowRateBox->addItem( tr( "16" ), 16 );
		flowRateBox->addItem( tr( "32" ), 32 );
		flowRateBox->setCurrentIndex( 2 );
		flowRateBox->setToolTip( tr( "How many flow samples a cell the file stores. The curves are world\n"
			"coordinates, so this can change without losing them." ) );
		wwMatchFieldStyle( flowRateBox );
		fl.add( bodyW, tr( "Flow samples per cell" ), flowRateBox );
		bv->addLayout( fl.g );
	}
	layout->addWidget( filesSection );
	layout->addStretch( 1 );

	// ---- the map, on the splitter ---------------------------------------------
	view = new WaterMapView( this, splitter );
	splitter->addWidget( view );
	splitter->setStretchFactor( 0, 0 );
	splitter->setStretchFactor( 1, 1 );
	splitter->setSizes( { 340, 940 } );

	// ---- the summary and the action bar, pinned ----------------------------------
	summary = new QLabel( this );
	summary->setObjectName( QStringLiteral( "WaterWindowSummary" ) );
	summary->setWordWrap( true );
	summary->setContentsMargins( 8, 4, 8, 2 );
	outer->addWidget( summary, 0 );
	auto * bar = new QWidget( this );
	bar->setObjectName( QStringLiteral( "WaterWindowActionBar" ) );
	auto * bh = new QHBoxLayout( bar );
	bh->setContentsMargins( 8, 2, 8, 8 );
	bh->setSpacing( 6 );
	fullButton = new QPushButton( tr( "Full screen" ), bar );
	fullButton->setObjectName( QStringLiteral( "WaterWindowFullScreenButton" ) );
	fullButton->setToolTip( tr( "Fill the screen with this window; F11 does the same, and again to leave." ) );
	bh->addWidget( fullButton, 0 );
	bh->addStretch( 1 );
	reloadButton = new QPushButton( tr( "Reload" ), bar );
	reloadButton->setObjectName( QStringLiteral( "WaterWindowReloadButton" ) );
	reloadButton->setToolTip( tr( "Read the landscape file again and take its curves back out of it,\n"
		"throwing away anything unsaved." ) );
	solveButton = new QPushButton( tr( "Solve" ), bar );
	solveButton->setObjectName( QStringLiteral( "WaterWindowSolveButton" ) );
	solveButton->setToolTip( tr( "Fill in the gaps: run the flow simulation inside every body a curve or\n"
		"pin touches, and the dye along it. Nothing is written until Save." ) );
	saveButton = new QPushButton( tr( "Save" ), bar );
	saveButton->setObjectName( QStringLiteral( "WaterWindowSaveButton" ) );
	saveButton->setToolTip( tr( "Write the curves and the planes they derive into the landscape file,\n"
		"and the curves file beside it." ) );
	bh->addWidget( reloadButton, 0 );
	bh->addWidget( solveButton, 0 );
	bh->addWidget( saveButton, 0 );
	outer->addWidget( bar, 0 );

	// ---- wiring ----------------------------------------------------------------------
	connect( showBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
		[this]( int i ) { view->setPlane( showBox->itemData( i ).toInt() ); } );
	connect( toolBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
		[this]( int i ) { view->setTool( toolBox->itemData( i ).toInt() ); refreshSummary(); } );
	connect( weightSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
		if ( !loadingWeight )
			applyWeightToSelection( weightSpin->value() );
	} );
	connect( reverseButton, &QPushButton::clicked, this, [this]() { reverseSelected(); } );
	connect( finishButton, &QPushButton::clicked, this, [this]() { finishCurve(); } );
	connect( deleteButton, &QPushButton::clicked, this, [this]() { deleteSelectedPoints(); } );
	connect( classBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]( int i ) {
		if ( doc && selected && !loadingBody ) {
			const int cls = classBox->itemData( i ).toInt();
			doc->setBodyClass( selected, cls );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) )
				o->cls = cls;
			edited( tr( "class of body %1" ).arg( selected ) );
		}
		refreshSummary();
	} );
	connect( formBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]( int i ) {
		if ( doc && selected && !loadingBody ) {
			const quint32 f = quint32( formBox->itemData( i ).toUInt() );
			doc->setBodyForm( selected, f );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) )
				o->form = f;
			edited( tr( "water form of body %1" ).arg( selected ) );
		}
		refreshSummary();
	} );
	connect( stillCheck, &QCheckBox::toggled, this, [this]( bool on ) {
		if ( doc && selected && !loadingBody ) {
			doc->setBodyLockZero( selected, on );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) )
				o->still = on;
			edited( tr( "still water on body %1" ).arg( selected ) );
		}
		refreshSummary();
	} );
	connect( dyeMouthCheck, &QCheckBox::toggled, this, [this]( bool on ) {
		if ( doc && selected && !loadingBody ) {
			doc->setBodyDyeMouth( selected, on, 1.0f );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) ) {
				o->dyeMouth = on;
				o->dyeStrength = 1.0f;
			}
			edited( tr( "dye at the mouth of body %1" ).arg( selected ) );
		}
		refreshSummary();
	} );
	connect( colourCheck, &QCheckBox::toggled, this, [this]( bool on ) {
		if ( doc && selected && !loadingBody ) {
			doc->setBodyColour( selected, quint8( pick.red() ), quint8( pick.green() ), quint8( pick.blue() ), on );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) ) {
				o->colour[0] = quint8( pick.red() );
				o->colour[1] = quint8( pick.green() );
				o->colour[2] = quint8( pick.blue() );
				o->colour[3] = on ? 255 : 0;
			}
			edited( tr( "colour of body %1" ).arg( selected ) );
		}
		refreshSummary();
	} );
	connect( colourButton, &QPushButton::clicked, this, [this]() {
		const QColor c = QColorDialog::getColor( pick, this, tr( "Body colour" ) );
		if ( !c.isValid() )
			return;
		pick = c;
		if ( !colourCheck->isChecked() ) {
			colourCheck->setChecked( true );     // its toggled handler writes the colour
			return;
		}
		if ( doc && selected ) {
			doc->setBodyColour( selected, quint8( c.red() ), quint8( c.green() ), quint8( c.blue() ), true );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) ) {
				o->colour[0] = quint8( c.red() );
				o->colour[1] = quint8( c.green() );
				o->colour[2] = quint8( c.blue() );
				o->colour[3] = 255;
			}
			edited( tr( "colour of body %1" ).arg( selected ) );
		}
	} );
	connect( dyeColourButton, &QPushButton::clicked, this, [this]() {
		const QColor c = QColorDialog::getColor( dyePick, this, tr( "Dye colour" ) );
		if ( c.isValid() )
			dyePick = c;
	} );
	connect( dyeFadeSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
		if ( doc && !loadingBody ) {
			doc->setDyeHalfDistance( dyeFadeSpin->value() );
			curves.dyeHalfDistance = dyeFadeSpin->value();
			edited( tr( "dye fade" ) );
		}
	} );
	connect( nameEdit, &QLineEdit::editingFinished, this, [this]() {
		if ( doc && selected && !loadingBody ) {
			doc->setBodyName( selected, nameEdit->text() );
			if ( WaterBodyOverride * o = curves.overrideFor( selected, true ) )
				o->name = nameEdit->text();
			edited( tr( "name of body %1" ).arg( selected ) );
		}
		refreshSummary();
	} );
	connect( flowRateBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, [this]( int i ) {
		QString err;
		if ( doc && !doc->setFlowRate( flowRateBox->itemData( i ).toInt(), &err ) )
			refusal = err;
		refreshSummary();
	} );
	connect( saveCurvesButton, &QPushButton::clicked, this, [this]() {
		if ( !doc )
			return;
		const QString f = QFileDialog::getSaveFileName( this, tr( "Save curves" ),
			WaterCurveDoc::jsonPathFor( doc->path() ), tr( "Water curves (*.water.json)" ) );
		if ( !f.isEmpty() )
			saveCurves( f );
	} );
	connect( loadCurvesButton, &QPushButton::clicked, this, [this]() {
		if ( !doc )
			return;
		const QString f = QFileDialog::getOpenFileName( this, tr( "Load curves" ),
			WaterCurveDoc::jsonPathFor( doc->path() ), tr( "Water curves (*.water.json)" ) );
		if ( !f.isEmpty() )
			loadCurves( f );
	} );
	connect( exportButton, &QPushButton::clicked, this, [this]() {
		if ( !doc )
			return;
		const QFileInfo fi( doc->path() );
		const QString f = QFileDialog::getSaveFileName( this, tr( "Export flow map" ),
			fi.dir().filePath( fi.completeBaseName() + QStringLiteral( ".flow.png" ) ), tr( "PNG (*.png)" ) );
		if ( f.isEmpty() )
			return;
		QString mask = f;
		mask.replace( QRegularExpression( QStringLiteral( "\\.flow\\.png$" ) ), QStringLiteral( ".bodies.png" ) );
		if ( mask == f )
			mask = f + QStringLiteral( ".bodies.png" );
		qint64 wet = 0;
		exportPng( f, mask, &wet );
	} );
	connect( importButton, &QPushButton::clicked, this, [this]() {
		if ( !doc )
			return;
		const QFileInfo fi( doc->path() );
		const QString f = QFileDialog::getOpenFileName( this, tr( "Import flow map" ),
			fi.dir().path(), tr( "PNG (*.png)" ) );
		if ( !f.isEmpty() )
			importPng( f );
	} );
	connect( solveButton, &QPushButton::clicked, this, [this]() { solve(); } );
	connect( saveButton, &QPushButton::clicked, this, [this]() { saveNow(); } );
	connect( reloadButton, &QPushButton::clicked, this, [this]() { reload(); } );
	connect( fullButton, &QPushButton::clicked, this, [this]() { toggleFullScreen(); } );
	connect( fileEdit, &QLineEdit::editingFinished, this, [this]() {
		if ( !fileEdit->text().isEmpty() && ( !doc || doc->path() != fileEdit->text() ) )
			openFile( fileEdit->text() );
	} );
	auto * f11 = new QShortcut( QKeySequence( Qt::Key_F11 ), this );
	connect( f11, &QShortcut::activated, this, [this]() { toggleFullScreen(); } );
	auto * undoKey = new QShortcut( QKeySequence::Undo, this );
	connect( undoKey, &QShortcut::activated, this, [this]() { undo(); } );
	auto * redoKey = new QShortcut( QKeySequence::Redo, this );
	connect( redoKey, &QShortcut::activated, this, [this]() { redo(); } );
	auto * saveKey = new QShortcut( QKeySequence::Save, this );
	connect( saveKey, &QShortcut::activated, this, [this]() { saveNow(); } );
	refreshSummary();
}

void WaterWindow::keyPressEvent( QKeyEvent * e )
{
	if ( e->key() == Qt::Key_Delete || e->key() == Qt::Key_X ) {
		if ( view->hasFocus() || !focusWidget() || focusWidget() == this ) {
			deleteSelectedPoints();
			e->accept();
			return;
		}
	}
	if ( e->key() == Qt::Key_Escape && isFullScreen() ) {
		toggleFullScreen();
		e->accept();
		return;
	}
	QWidget::keyPressEvent( e );
}

void WaterWindow::closeEvent( QCloseEvent * e )
{
	QSettings().setValue( QStringLiteral( "WaterWindow/geometry" ), saveGeometry() );
	QSettings().setValue( QStringLiteral( "WaterWindow/splitter" ), splitter->saveState() );
	QWidget::closeEvent( e );
}

void WaterWindow::toggleFullScreen()
{
	if ( isFullScreen() ) {
		showNormal();
		fullButton->setText( tr( "Full screen" ) );
	} else {
		showFullScreen();
		fullButton->setText( tr( "Leave full screen" ) );
	}
}

bool WaterWindow::openFile( const QString & path )
{
	delete doc;
	doc = new WaterMarkDoc();
	QString err;
	if ( !doc->open( path, &err ) ) {
		refusal = err;
		delete doc;
		doc = nullptr;
		view->setDoc( nullptr, nullptr );
		refreshSummary();
		return false;
	}
	refusal.clear();
	fileEdit->setText( path );
	formBox->clear();
	for ( quint32 f : doc->waterForms() )
		formBox->addItem( QStringLiteral( "%1" ).arg( f, 8, 16, QLatin1Char( '0' ) ), f );
	curves.clear();
	QString why;
	curves.readFrom( *doc, &why );
	if ( !why.isEmpty() )
		lastEdit = why;
	undoStack.clear();
	redoStack.clear();
	curvesDirty = false;
	needSolve = false;
	loadingBody = true;
	dyeFadeSpin->setValue( doc->dyeHalfDistance() );
	loadingBody = false;
	// the planes as the FILE has them: the strokes it carries are the solve's input
	WaterMarkSolve st;
	if ( doc->solve( &st, &err ) )
		lastSolve = st.note;
	else
		refusal = err;
	view->setDoc( doc, &curves );
	view->fit();
	selected = 0;
	selectBody( 0 );
	const QString jsonBeside = WaterCurveDoc::jsonPathFor( path );
	if ( QFileInfo::exists( jsonBeside ) )
		lastEdit = tr( "a curves file sits beside it: %1 (Load curves reads it)" )
			.arg( QFileInfo( jsonBeside ).fileName() );
	refreshSummary();
	return true;
}

void WaterWindow::selectBody( int id )
{
	selected = id;
	view->setSelectedBody( id );
	loadingBody = true;
	LodtWaterBody b;
	if ( doc && doc->body( id, b ) ) {
		const int want = ( b.flags & ( 1u << 5 ) ) ? int( b.cls ) : -1;
		classBox->setCurrentIndex( qMax( 0, classBox->findData( want ) ) );
		const int fi = formBox->findData( b.watrForm );
		if ( fi >= 0 )
			formBox->setCurrentIndex( fi );
		colourCheck->setChecked( b.colour[3] != 0 );
		if ( b.colour[3] )
			pick = QColor( b.colour[0], b.colour[1], b.colour[2] );
		stillCheck->setChecked( doc->bodyLockZero( id ) );
		dyeMouthCheck->setChecked( doc->bodyDyeMouth( id ) );
		nameEdit->setText( doc->bodyName( id ) );
	} else {
		nameEdit->clear();
	}
	loadingBody = false;
	refreshSummary();
}

void WaterWindow::selectionChanged()
{
	// the Point weight row shows the selection's weight (the first, when they differ)
	const QVector<PointRef> & s = view->selection();
	loadingWeight = true;
	if ( !s.isEmpty() ) {
		const PointRef & r = s.first();
		if ( r.curve < curves.curves.size() && r.point < curves.curves[r.curve].pts.size() )
			weightSpin->setValue( double( curves.curves[r.curve].pts[r.point].w ) );
	}
	loadingWeight = false;
	refreshSummary();
}

void WaterWindow::applyWeightToSelection( double w )
{
	const QVector<PointRef> & s = view->selection();
	int n = 0;
	for ( const PointRef & r : s )
		if ( r.curve < curves.curves.size() && r.point < curves.curves[r.curve].pts.size() ) {
			curves.curves[r.curve].pts[r.point].w = float( w );
			n++;
		}
	if ( n )
		edited( tr( "weight %1 on %2 point(s)" ).arg( w ).arg( n ) );
	view->update();
}

void WaterWindow::reverseSelected()
{
	QVector<int> which;
	for ( const PointRef & r : view->selection() )
		if ( !which.contains( r.curve ) )
			which.append( r.curve );
	if ( which.isEmpty() && view->activeCurve() >= 0 )
		which.append( view->activeCurve() );
	int n = 0;
	for ( int i : which )
		if ( i >= 0 && i < curves.curves.size() ) {
			curves.curves[i].reverse();
			n++;
		}
	if ( n ) {
		view->clearSelection();
		edited( tr( "%1 curve(s) reversed" ).arg( n ) );
		view->say( tr( "%1 curve(s) reversed: the water now flows the other way" ).arg( n ) );
	} else {
		view->say( tr( "select a curve to reverse" ) );
	}
}

void WaterWindow::finishCurve()
{
	view->setActiveCurve( -1 );
	view->clearSelection();
	view->say( tr( "curve finished; the next click starts a new one" ) );
	refreshSummary();
}

void WaterWindow::deleteSelectedPoints()
{
	QVector<PointRef> s = view->selection();
	if ( s.isEmpty() ) {
		view->say( tr( "select a point to delete" ) );
		return;
	}
	// highest index first, so the indices below stay valid
	std::sort( s.begin(), s.end(), []( const PointRef & a, const PointRef & b ) {
		return a.curve != b.curve ? a.curve > b.curve : a.point > b.point;
	} );
	int removed = 0, curvesGone = 0;
	for ( const PointRef & r : s ) {
		if ( r.curve >= curves.curves.size() || r.point >= curves.curves[r.curve].pts.size() )
			continue;
		curves.curves[r.curve].pts.removeAt( r.point );
		removed++;
		if ( curves.curves[r.curve].pts.isEmpty() ) {
			curves.curves.removeAt( r.curve );
			curvesGone++;
		}
	}
	view->clearSelection();
	view->setActiveCurve( -1 );
	edited( tr( "%1 point(s) removed" ).arg( removed ) );
	view->say( curvesGone ? tr( "%1 point(s) removed, %2 curve(s) with them" ).arg( removed ).arg( curvesGone )
		: tr( "%1 point(s) removed" ).arg( removed ) );
}

void WaterWindow::edited( const QString & what )
{
	/* The stack holds the state AFTER each edit (the caller has already
	 * applied it; a drag pushes once at release, so a drag is one step).
	 * undo() pops the top and restores the one beneath, or the file's own. */
	undoStack.append( curves );
	if ( undoStack.size() > 64 )
		undoStack.removeFirst();
	redoStack.clear();
	curvesDirty = true;
	needSolve = true;
	lastEdit = what;
	refreshSummary();
	view->update();
}

void WaterWindow::undo()
{
	/* The stack holds the states AFTER each edit; the state before the first
	 * edit is the file's own, re-read from the land file's strokes. */
	if ( undoStack.isEmpty() )
		return;
	redoStack.append( undoStack.takeLast() );
	if ( !undoStack.isEmpty() ) {
		curves = undoStack.last();
	} else if ( doc ) {
		QString why;
		curves.readFrom( *doc, &why );
	}
	view->clearSelection();
	view->setActiveCurve( -1 );
	needSolve = true;
	lastEdit = tr( "undone" );
	refreshSummary();
	view->update();
}

void WaterWindow::redo()
{
	if ( redoStack.isEmpty() )
		return;
	undoStack.append( redoStack.takeLast() );
	curves = undoStack.last();
	view->clearSelection();
	needSolve = true;
	lastEdit = tr( "redone" );
	refreshSummary();
	view->update();
}

bool WaterWindow::solve()
{
	if ( !doc )
		return false;
	QString err;
	int refused = 0;
	curves.writeTo( *doc, &refused, &err );
	if ( refused )
		lastEdit = err;
	WaterMarkSolve st;
	const bool ok = doc->solve( &st, &err );
	if ( !ok )
		refusal = err;
	else {
		refusal.clear();
		lastSolve = st.note;
		needSolve = false;
	}
	view->rebuildOverview();
	view->invalidateDetail();
	view->update();
	refreshSummary();
	return ok;
}

bool WaterWindow::saveNow()
{
	if ( !doc )
		return false;
	if ( needSolve && !solve() )
		return false;
	QString err;
	if ( !doc->save( &err ) ) {
		refusal = err;
		refreshSummary();
		return false;
	}
	refusal.clear();
	const QString json = WaterCurveDoc::jsonPathFor( doc->path() );
	QString jerr;
	const bool jok = curves.saveJson( json, &jerr );
	lastSolve = jok
		? tr( "saved %1 and %2" ).arg( QFileInfo( doc->path() ).fileName(), QFileInfo( json ).fileName() )
		: tr( "saved %1; the curves file was NOT written: %2" ).arg( QFileInfo( doc->path() ).fileName(), jerr );
	curvesDirty = !jok;
	view->rebuildOverview();
	view->invalidateDetail();
	view->update();
	refreshSummary();
	return true;
}

void WaterWindow::reload()
{
	if ( doc )
		openFile( doc->path() );
}

bool WaterWindow::saveCurves( const QString & path )
{
	QString err;
	if ( !curves.saveJson( path, &err ) ) {
		refusal = err;
		refreshSummary();
		return false;
	}
	refusal.clear();
	curvesDirty = false;
	lastEdit = tr( "curves saved to %1 (%2 curves, %3 bodies)" ).arg( QFileInfo( path ).fileName() )
		.arg( curves.curves.size() ).arg( curves.bodies.size() );
	refreshSummary();
	return true;
}

bool WaterWindow::loadCurves( const QString & path )
{
	WaterCurveDoc d;
	QString err;
	if ( !d.loadJson( path, &err ) ) {
		refusal = err;
		refreshSummary();
		return false;
	}
	refusal.clear();
	if ( doc && !d.worldspace.isEmpty() && d.worldspace.compare( curves.worldspace, Qt::CaseInsensitive ) != 0 )
		lastEdit = tr( "the curves file says worldspace %1; the open file is %2 -- loaded anyway, "
			"the coordinates are world units" ).arg( d.worldspace, curves.worldspace );
	else
		lastEdit = tr( "curves loaded from %1 (%2 curves, %3 bodies); Solve re-derives the planes" )
			.arg( QFileInfo( path ).fileName() ).arg( d.curves.size() ).arg( d.bodies.size() );
	// the raster layers come from the PNGs the json names, beside it
	int missing = 0;
	for ( WaterRasterLayer & r : d.rasters ) {
		if ( !doc || r.sourceFile.isEmpty() )
			continue;
		const QString png = QFileInfo( path ).dir().filePath( r.sourceFile );
		WaterRasterLayer got;
		double a = 0, f = 0;
		QString why;
		if ( QFileInfo::exists( png ) && WaterCurveDoc::importFlowPng( *doc, png, got, &a, &f, &why ) )
			r = got;
		else
			missing++;
	}
	if ( missing )
		lastEdit += tr( "; %1 flow map PNG(s) named by the file were not found beside it" ).arg( missing );
	// keep the open file's provenance; the curves are what the file brought
	d.worldspace = curves.worldspace.isEmpty() ? d.worldspace : curves.worldspace;
	d.landFile = curves.landFile.isEmpty() ? d.landFile : curves.landFile;
	d.cellMinX = curves.cellMinX;
	d.cellMinY = curves.cellMinY;
	d.cellMaxX = curves.cellMaxX;
	d.cellMaxY = curves.cellMaxY;
	d.bodySamples = curves.bodySamples;
	curves = d;
	if ( doc ) {
		loadingBody = true;
		dyeFadeSpin->setValue( curves.dyeHalfDistance );
		loadingBody = false;
	}
	view->clearSelection();
	view->setActiveCurve( -1 );
	// one undo step: the post-load state goes on the stack, as every edit does
	undoStack.append( curves );
	redoStack.clear();
	needSolve = true;
	curvesDirty = false;
	refreshSummary();
	view->update();
	return true;
}

bool WaterWindow::exportPng( const QString & flowPng, const QString & maskPng, qint64 * wet )
{
	if ( !doc )
		return false;
	QString err;
	qint64 n = 0;
	if ( !WaterCurveDoc::exportFlowPng( *doc, flowPng, maskPng, &n, &err ) ) {
		refusal = err;
		refreshSummary();
		return false;
	}
	if ( wet )
		*wet = n;
	refusal.clear();
	lastEdit = tr( "flow map written: %1 (%2 wet texels) and %3" )
		.arg( QFileInfo( flowPng ).fileName() ).arg( n ).arg( QFileInfo( maskPng ).fileName() );
	refreshSummary();
	return true;
}

bool WaterWindow::importPng( const QString & flowPng )
{
	if ( !doc )
		return false;
	WaterRasterLayer r;
	double asIs = 0, flipped = 0;
	QString err;
	if ( !WaterCurveDoc::importFlowPng( *doc, flowPng, r, &asIs, &flipped, &err ) ) {
		refusal = err;
		refreshSummary();
		return false;
	}
	refusal.clear();
	curves.rasters.append( r );
	edited( tr( "flow map %1 imported as a layer: %2 painted texels, agreement with the file %3" )
		.arg( r.sourceFile ).arg( r.paintedCount() ).arg( asIs, 0, 'f', 3 ) );
	showBox->setCurrentIndex( qMax( 0, showBox->findData( WaterMapView::Imported ) ) );
	return true;
}

/*! THE SENTENCE. It says what Save will do, or the ONE reason it cannot, and
 *  it owns the buttons' enabled state. */
void WaterWindow::refreshSummary()
{
	const QString muted = wwSkinColor( "textMuted" );
	const QString danger = wwSkinColor( "danger" );
	QString text;
	bool can = false;
	if ( !refusal.isEmpty() ) {
		text = refusal;
	} else if ( !doc ) {
		text = tr( "No landscape file is open. Choose a version 3 .lodl to mark its water." );
	} else {
		can = true;
		int nCurves = 0, nPins = 0, nPts = 0;
		for ( const WaterCurve & c : curves.curves ) {
			if ( c.kind == WaterCurve::Curve && c.pts.size() > 1 )
				nCurves++;
			else
				nPins++;
			nPts += c.pts.size();
		}
		text = tr( "Write %1 curve(s) with %2 points, %3 pin(s) and %4 layer(s) into %5 and %6 "
			"(%7 bodies, flow at %8 samples a cell)%9" )
			.arg( nCurves ).arg( nPts ).arg( nPins ).arg( curves.rasters.size() )
			.arg( QFileInfo( doc->path() ).fileName() )
			.arg( QFileInfo( WaterCurveDoc::jsonPathFor( doc->path() ) ).fileName() )
			.arg( doc->bodyCount() ).arg( doc->flowSamples() )
			.arg( needSolve ? tr( " -- unsolved edits: Solve fills in the gaps first" ) : QString() );
		if ( selected )
			text += QStringLiteral( "\n" ) + doc->describeBody( selected );
		if ( !lastEdit.isEmpty() )
			text += QStringLiteral( "\n" ) + lastEdit;
		if ( !lastSolve.isEmpty() )
			text += QStringLiteral( "\n" ) + lastSolve;
	}
	summary->setText( text );
	summary->setStyleSheet( QStringLiteral( "color: %1;" ).arg( refusal.isEmpty() ? muted : danger ) );
	saveButton->setEnabled( can );
	solveButton->setEnabled( can );
	reloadButton->setEnabled( doc != nullptr );
	saveCurvesButton->setEnabled( can );
	loadCurvesButton->setEnabled( doc != nullptr );
	exportButton->setEnabled( can );
	importButton->setEnabled( can );
	classBox->setEnabled( doc && selected );
	formBox->setEnabled( doc && selected );
	colourCheck->setEnabled( doc && selected );
	colourButton->setEnabled( doc && selected );
	stillCheck->setEnabled( doc && selected );
	dyeMouthCheck->setEnabled( doc && selected );
	nameEdit->setEnabled( doc && selected );
	dyeFadeSpin->setEnabled( doc != nullptr );
	const bool haveSel = !view->selection().isEmpty();
	weightSpin->setEnabled( doc && ( haveSel || view->currentTool() == WaterMapView::Draw ) );
	reverseButton->setEnabled( doc && ( haveSel || view->activeCurve() >= 0 ) );
	deleteButton->setEnabled( doc && haveSel );
	finishButton->setEnabled( doc && view->activeCurve() >= 0 );
}

// =========================================================================
//  the self-test
// =========================================================================

struct Fnv
{
	quint64 h = 1469598103934665603ull;
	void add( quint64 v ) { h ^= v; h *= 1099511628211ull; }
};

//! Every flow word of the body plane, hashed in sweep order, plus the count that moved.
bool hashWords( const WaterMarkDoc & doc, int body, quint64 & hash, qint64 & bodyTexels,
	qint64 & bodyMoved, QString * err )
{
	Fnv f;
	bodyTexels = 0;
	bodyMoved = 0;
	const bool ok = doc.sweep( [&]( int px, int py, quint16 id, quint16 wordAuto, quint16 wordNow ) {
		f.add( ( quint64( quint32( px ) ) << 40 ) ^ ( quint64( quint32( py ) ) << 16 ) ^ wordNow );
		if ( int( id ) == body ) {
			bodyTexels++;
			if ( wordNow != wordAuto )
				bodyMoved++;
		}
	}, err );
	hash = f.h;
	return ok;
}

void runWindowSelfTest( QMainWindow * mw )
{
	QFile logf( QApplication::applicationDirPath() + QStringLiteral( "/ww_water_window_test.log" ) );
	if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	QTextStream log( &logf );
	int checksRun = 0, fails = 0;
	auto check = [&]( const QString & what, bool pass ) {
		checksRun++;
		if ( !pass )
			fails++;
		log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
		log.flush();
	};
	auto skip = [&]( const QString & what ) {
		log << "  SKIP " << what << "\n";
		log.flush();
	};

	WaterWindow * win = static_cast<WaterWindow *>( waterWindowOpen( mw, QString() ) );
	QApplication::processEvents();
	check( QStringLiteral( "the water window is a top-level window of its own" ),
		win && win->isWindow() && win->isVisible() );
	check( QStringLiteral( "with no file open the window says so: \"%1\"" ).arg( win->summaryLabel()->text() ),
		win->summaryLabel()->text().contains( QLatin1String( "No landscape file" ) ) && !win->saveBtn()->isEnabled() );

	// ---- W1: the house style, counted, each with a floor -------------------
	int numbers = 0, plain = 0;
	for ( QAbstractSpinBox * s : win->findChildren<QAbstractSpinBox *>() ) {
		numbers++;
		if ( !s->property( "wwScrubbed" ).toBool() )
			plain++;
	}
	log << "number fields: " << numbers << ", left as plain spin boxes: " << plain << "\n";
	check( QStringLiteral( "the numbers are scrub fields (%1, plain %2, floor 4)" ).arg( numbers ).arg( plain ),
		numbers >= 4 && plain == 0 );
	const int groups = win->findChildren<QGroupBox *>().size();
	int headings = 0;
	for ( QLabel * l : win->findChildren<QLabel *>() )
		if ( l->styleSheet().contains( QLatin1String( "font-weight: 600" ) ) )
			headings++;
	log << "group boxes: " << groups << ", headings: " << headings << "\n";
	check( QStringLiteral( "sections are headings, not group-box frames (%1 headings, floor 5)" ).arg( headings ),
		groups == 0 && headings >= 5 );
	int combos = 0, unmatched = 0;
	for ( QComboBox * c : win->findChildren<QComboBox *>() ) {
		combos++;
		if ( !c->styleSheet().contains( QLatin1String( "drop-down" ) ) )
			unmatched++;
	}
	log << "selectors: " << combos << ", in default chrome: " << unmatched << "\n";
	check( QStringLiteral( "every selector takes the matched field chrome (%1, floor 5)" ).arg( combos ),
		combos >= 5 && unmatched == 0 );
	int boxes = 0, dashed = 0, untipped = 0;
	for ( QCheckBox * c : win->findChildren<QCheckBox *>() ) {
		boxes++;
		if ( c->text().contains( QLatin1String( " - " ) ) )
			dashed++;
		if ( c->toolTip().isEmpty() )
			untipped++;
	}
	log << "check boxes: " << boxes << ", with a dash explanation: " << dashed << ", without a tooltip: " << untipped << "\n";
	check( QStringLiteral( "labels are names; the explanation is the tooltip (%1 boxes, floor 3)" ).arg( boxes ),
		boxes >= 3 && dashed == 0 && untipped == 0 );
	{
		QList<int> ys;
		int fields = 0;
		for ( const char * n : { "WaterWindowShowBox", "WaterWindowToolBox", "WaterWindowSpeedSpin",
				"WaterWindowWidthSpin", "WaterWindowWeightSpin", "WaterWindowClassBox",
				"WaterWindowFormBox", "WaterWindowNameEdit", "WaterWindowDyeFadeSpin" } ) {
			auto * w = win->findChild<QWidget *>( QLatin1String( n ) );
			if ( !w )
				continue;
			fields++;
			const int y = w->mapTo( win, QPoint( 0, 0 ) ).y();
			if ( !ys.contains( y ) )
				ys.append( y );
		}
		log << "settings: " << fields << " on " << ys.size() << " distinct rows\n";
		check( QStringLiteral( "settings sit one to a row (%1 on %2, floor 8)" ).arg( fields ).arg( ys.size() ),
			fields >= 8 && ys.size() == fields );
	}
	{
		auto * sc = win->settingsScroll();
		auto * mp = win->map();
		auto * sum = win->summaryLabel();
		auto * tb = win->findChild<QWidget *>( QStringLiteral( "WaterWindowToolBox" ) );
		check( QStringLiteral( "Save sits outside the scrolling settings" ), sc && !sc->isAncestorOf( win->saveBtn() ) );
		check( QStringLiteral( "the map sits outside the scrolling settings" ), sc && mp && !sc->isAncestorOf( mp ) );
		check( QStringLiteral( "the summary sits outside the scrolling settings" ), sc && sum && !sc->isAncestorOf( sum ) );
		check( QStringLiteral( "the settings themselves do scroll" ), sc && tb && sc->isAncestorOf( tb ) );
		int shown = 0, counted = 0;
		if ( sc && sc->viewport() ) {
			const QRect vp = sc->viewport()->rect();
			for ( const char * n : { "WaterWindowShowBox", "WaterWindowToolBox", "WaterWindowSpeedSpin",
					"WaterWindowWidthSpin", "WaterWindowWeightSpin", "WaterWindowClassBox",
					"WaterWindowFormBox", "WaterWindowNameEdit" } ) {
				auto * w = win->findChild<QWidget *>( QLatin1String( n ) );
				if ( !w )
					continue;
				counted++;
				const QPoint tl = w->mapTo( sc->viewport(), QPoint( 0, 0 ) );
				if ( vp.contains( QPoint( tl.x() + 2, tl.y() + 1 ) )
					&& vp.contains( QPoint( tl.x() + 2, tl.y() + w->height() - 1 ) ) )
					shown++;
			}
		}
		log << "settings visible without scrolling: " << shown << " of " << counted << "\n";
		check( QStringLiteral( "the settings band opens showing its settings (%1 of %2, floor 6)" )
			.arg( shown ).arg( counted ), counted >= 8 && shown >= 6 );
	}
	{
		auto * arrow = win->findChild<QToolButton *>( QStringLiteral( "WaterWindowFilesExpander" ) );
		auto * body = win->findChild<QWidget *>( QStringLiteral( "WaterWindowFilesBody" ) );
		const bool wasOpen = body && body->isVisible();
		if ( arrow )
			arrow->click();
		QApplication::processEvents();
		const bool nowOpen = body && body->isVisible();
		check( QStringLiteral( "the Files section folds (%1 -> %2)" ).arg( wasOpen ? "open" : "folded" )
			.arg( nowOpen ? "open" : "folded" ), arrow && body && wasOpen != nowOpen );
		if ( arrow )
			arrow->click();
		QApplication::processEvents();
	}
	{
		const bool before = win->isFullScreen();
		win->toggleFullScreen();
		QApplication::processEvents();
		const bool during = win->isFullScreen();
		win->toggleFullScreen();
		QApplication::processEvents();
		check( QStringLiteral( "the full-screen toggle fills the screen and leaves it again (%1 -> %2 -> %3)" )
			.arg( int( before ) ).arg( int( during ) ).arg( int( win->isFullScreen() ) ),
			!before && during && !win->isFullScreen() );
	}

	// ---- the file ----------------------------------------------------------
	const QString file = QString::fromLocal8Bit( qgetenv( "WW_WATER_WINDOW_FILE" ) );
	const bool opened = !file.isEmpty() && win->openFile( file );
	check( QStringLiteral( "the landscape file opens: %1" ).arg( file ), opened );
	const QString shotDir = QString::fromLocal8Bit( qgetenv( "WW_WATER_WINDOW_SHOT" ) );
	auto shot = [&]( const QString & name ) {
		if ( shotDir.isEmpty() )
			return;
		QApplication::processEvents();
		QApplication::processEvents();
		const QString path = QDir( shotDir ).filePath( name );
		const bool saved = win->grab().save( path );
		log << "screenshot " << ( saved ? "saved: " : "NOT saved: " ) << path << "\n";
	};
	if ( opened && win->document() ) {
		WaterMarkDoc * doc = win->document();
		{
			// the whole worldspace fits at first open: every corner is inside the map
			double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
			doc->worldBounds( x0, y0, x1, y1 );
			const QPointF a = win->map()->worldToView( x0, y0 ), b = win->map()->worldToView( x1, y1 );
			const QRectF r = win->map()->rect();
			check( QStringLiteral( "the whole worldspace fits the map at first open (%1 px a texel)" )
				.arg( win->map()->pixelsPerTexel(), 0, 'f', 3 ),
				r.contains( a ) && r.contains( b ) && win->map()->pixelsPerTexel() > 0.0 );
		}
		shot( QStringLiteral( "water_window_whole.png" ) );

		// the largest river, and five points down its own centreline
		int river = 0;
		quint32 best = 0;
		for ( int i = 1; i <= doc->bodyCount(); i++ ) {
			LodtWaterBody b;
			doc->body( i, b );
			if ( b.cls == 1 && b.area > best ) {
				best = b.area;
				river = i;
			}
		}
		check( QStringLiteral( "the file offers a river to mark (body %1, %2 texels)" ).arg( river ).arg( best ), river > 0 );
		LodtWaterBody rb;
		doc->body( river, rb );
		QVector<QPointF> spine;
		{
			int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
			doc->worldToTexel( double( rb.x0 ) * 4096.0, double( rb.y0 ) * 4096.0, px0, py0 );
			doc->worldToTexel( ( double( rb.x1 ) + 1.0 ) * 4096.0, ( double( rb.y1 ) + 1.0 ) * 4096.0, px1, py1 );
			const bool alongY = ( py1 - py0 ) >= ( px1 - px0 );
			const int a0 = alongY ? py0 : px0, a1 = alongY ? py1 : px1;
			const int b0 = alongY ? px0 : py0, b1 = alongY ? px1 : py1;
			for ( int a = a0; a <= a1; a += 4 ) {
				double sx = 0, sy = 0;
				qint64 n = 0;
				for ( int c = b0; c <= b1; c++ ) {
					double wx = 0, wy = 0;
					doc->texelToWorld( alongY ? c : a, alongY ? a : c, wx, wy );
					if ( doc->bodyAtWorld( wx, wy ) != rb.id )
						continue;
					sx += wx;
					sy += wy;
					n++;
				}
				if ( n )
					spine.append( QPointF( sx / double( n ), sy / double( n ) ) );
			}
		}
		check( QStringLiteral( "the river's centreline has enough samples for five points (%1)" ).arg( spine.size() ),
			spine.size() >= 5 );
		// the mouth: the river texel nearest the body it drains into, or the far end
		QPointF mouth = spine.isEmpty() ? QPointF() : spine.last();
		if ( rb.outlet && spine.size() >= 2 ) {
			LodtWaterBody ob;
			if ( doc->body( int( rb.outlet ), ob ) ) {
				const QPointF oc( ( double( ob.x0 ) + double( ob.x1 ) + 1.0 ) * 0.5 * 4096.0,
					( double( ob.y0 ) + double( ob.y1 ) + 1.0 ) * 0.5 * 4096.0 );
				const double dFirst = std::hypot( spine.first().x() - oc.x(), spine.first().y() - oc.y() );
				const double dLast = std::hypot( spine.last().x() - oc.x(), spine.last().y() - oc.y() );
				if ( dFirst < dLast )
					std::reverse( spine.begin(), spine.end() );
				mouth = spine.last();
			}
		}
		WaterCurveDoc & m = win->model();
		const int curvesBefore = m.curves.size();
		const float weights[5] = { 1.0f, 0.5f, 2.0f, 0.75f, 1.25f };
		WaterCurve c;
		c.kind = WaterCurve::Curve;
		c.body = quint16( river );
		c.speed = 0.5f;
		c.width = 4096.0f;
		for ( int k = 0; k < 5 && !spine.isEmpty(); k++ ) {
			const QPointF q = spine[ qMin( spine.size() - 1, k * ( spine.size() - 1 ) / 4 ) ];
			c.pts.append( WaterCurvePoint{ float( q.x() ), float( q.y() ), weights[k] } );
		}
		m.curves.append( c );
		WaterCurve sp;
		sp.kind = WaterCurve::SourcePin;
		sp.body = quint16( river );
		sp.pts.append( c.pts.first() );
		m.curves.append( sp );
		WaterCurve dp;
		dp.kind = WaterCurve::DyePin;
		dp.body = quint16( river );
		dp.speed = 1.0f;
		dp.width = 2048.0f;
		dp.colour[0] = 200;
		dp.colour[1] = 40;
		dp.colour[2] = 40;
		dp.colour[3] = 255;
		dp.pts.append( c.pts[2] );
		m.curves.append( dp );
		if ( WaterBodyOverride * o = m.overrideFor( river, true ) ) {
			o->name = QStringLiteral( "harness river" );
			o->colour[0] = 10;
			o->colour[1] = 20;
			o->colour[2] = 30;
			o->colour[3] = 255;
		}
		win->edited( QStringLiteral( "harness: five points, a source pin, a dye pin, a name and a colour" ) );
		win->selectBody( river );
		check( QStringLiteral( "the curve, the two pins and the override are in the document (%1 -> %2 curves)" )
			.arg( curvesBefore ).arg( m.curves.size() ), m.curves.size() == curvesBefore + 3 );
		check( QStringLiteral( "the sentence says the edits are unsolved" ),
			win->summaryLabel()->text().contains( QLatin1String( "unsolved" ) ) );

		// Solve = WATER4's solver, through the one entry point
		const bool solved = win->solve();
		LodtWaterBody after;
		doc->body( river, after );
		log << "the solve says: '" << win->lastSolveNote() << "'\n";
		check( QStringLiteral( "Solve runs the land file's solver and the body's flow comes from the curve (source %1 -> %2)" )
			.arg( rb.flowSource ).arg( after.flowSource ), solved && after.flowSource == 4 );
		check( QStringLiteral( "after Solve the sentence no longer says unsolved" ),
			!win->summaryLabel()->text().contains( QLatin1String( "unsolved" ) ) );

		// the zoomed picture: the mouth with the curve and its arrows
		win->map()->lookAt( mouth.x(), mouth.y(), 2.0 );
		QApplication::processEvents();
		QApplication::processEvents();
		check( QStringLiteral( "the map zooms to texel level (%1 px a texel at the mouth)" )
			.arg( win->map()->pixelsPerTexel(), 0, 'f', 2 ), win->map()->pixelsPerTexel() >= 1.0 );
		shot( QStringLiteral( "water_window_mouth.png" ) );

		// ---- W2: the json round trip -------------------------------------------
		const QString jsonPath = WaterCurveDoc::jsonPathFor( file );
		const QByteArray a = m.toJson();
		check( QStringLiteral( "the curves file is written: %1" ).arg( jsonPath ), win->saveCurves( jsonPath ) );
		WaterCurveDoc m2;
		QString err;
		const bool loaded = m2.loadJson( jsonPath, &err );
		const QByteArray b = m2.toJson();
		log << "json bytes: " << a.size() << " written, " << b.size() << " re-written\n";
		check( QStringLiteral( "the json loads back (%1)" ).arg( loaded ? "ok" : err ), loaded );
		check( QStringLiteral( "save -> load -> save is byte-identical (%1 bytes)" ).arg( a.size() ),
			loaded && a == b && a.size() > 200 );
		QString why;
		check( QStringLiteral( "the curves read back equal point for point and weight for weight%1" )
			.arg( why.isEmpty() ? QString() : QStringLiteral( ": " ) + why ),
			WaterCurveDoc::sameCurves( m, m2, &why ) );
		check( QStringLiteral( "the file carries the five weighted points (%1 '[' per point row)" ).arg( a.count( "], [" ) ),
			a.count( "\"points\": [[" ) >= 1 && a.contains( "0.5]" ) && a.contains( "2]" ) && a.contains( "0.75]" ) );

		// ---- W3: the store round trip --------------------------------------------
		const WaterCurveDoc snapshot = m;
		check( QStringLiteral( "the land file saves with the curves mirrored into its store" ), win->saveNow() );
		check( QStringLiteral( "the land file reopens" ), win->openFile( file ) );
		/* openFile DELETES the old document and makes a new one, so the local
		 * `doc` captured at the top of this function is dangling from here on.
		 * Everything below used it -- W4 first (lane BUILD10, 2026-09-10). */
		doc = win->document();
		WaterCurveDoc fromStore = win->model();
		WaterCurveDoc snapUnweighted = snapshot, storeUnweighted = fromStore;
		for ( WaterCurve & c2 : snapUnweighted.curves )
			for ( WaterCurvePoint & p : c2.pts )
				p.w = 1.0f;
		for ( WaterCurve & c2 : storeUnweighted.curves )
			for ( WaterCurvePoint & p : c2.pts )
				p.w = 1.0f;
		why.clear();
		check( QStringLiteral( "the curves come back out of the .lodl store point for point%1" )
			.arg( why.isEmpty() ? QString() : QStringLiteral( ": " ) + why ),
			WaterCurveDoc::sameCurves( snapUnweighted, storeUnweighted, &why ) );
#ifdef WATERMARK_STROKE_EXTRA
		why.clear();
		check( QStringLiteral( "and weight for weight (hook-up H2 applied)%1" )
			.arg( why.isEmpty() ? QString() : QStringLiteral( ": " ) + why ),
			WaterCurveDoc::sameCurves( snapshot, fromStore, &why ) );
#else
		skip( QStringLiteral( "weights not carried by the store until hook-up H2 (WATERMARK_STROKE_EXTRA) is applied; "
			"the json carries them" ) );
#endif
		check( QStringLiteral( "the override came back out of the table (name '%1')" )
			.arg( fromStore.overrideOf( river ) ? fromStore.overrideOf( river )->name : QString() ),
			fromStore.overrideOf( river ) && fromStore.overrideOf( river )->name == QLatin1String( "harness river" ) );

		// ---- W4: load onto a regenerated (unmarked) file -----------------------
		quint64 h1 = 0;
		qint64 t1 = 0, mv1 = 0;
		check( QStringLiteral( "the marked file's flow words hash" ), hashWords( *doc, river, h1, t1, mv1, &err ) );
		log << "marked file: body " << river << " has " << t1 << " texels, " << mv1 << " moved from the automatic word\n";
		check( QStringLiteral( "the curve moved at least 60 percent of the river's own texels (%1 of %2)" ).arg( mv1 ).arg( t1 ),
			t1 > 0 && mv1 * 100 >= t1 * 60 );
		const QString file2 = QString::fromLocal8Bit( qgetenv( "WW_WATER_WINDOW_FILE2" ) );
		if ( file2.isEmpty() ) {
			skip( QStringLiteral( "W4 needs WW_WATER_WINDOW_FILE2, an unmarked copy of the fixture" ) );
		} else {
			WaterMarkDoc doc2;
			const bool o2 = doc2.open( file2, &err );
			check( QStringLiteral( "the unmarked copy opens: %1" ).arg( o2 ? file2 : err ), o2 );
			quint64 h0 = 0;
			qint64 t0 = 0, mv0 = 0;
			WaterMarkSolve st0;
			doc2.solve( &st0, &err );
			hashWords( doc2, river, h0, t0, mv0, &err );
			check( QStringLiteral( "before the load the copy's words differ from the marked file's (floor)" ), h0 != h1 );
			WaterCurveDoc m3;
			check( QStringLiteral( "the json loads onto the copy" ), m3.loadJson( jsonPath, &err ) );
			int refused = 0;
			m3.writeTo( doc2, &refused, &err );
			WaterMarkSolve st2;
			const bool s2 = doc2.solve( &st2, &err );
			quint64 h2 = 0;
			qint64 t2 = 0, mv2 = 0;
			hashWords( doc2, river, h2, t2, mv2, &err );
			log << "regenerated copy after load + solve: " << mv2 << " of " << t2 << " moved; refused " << refused << "\n";
			check( QStringLiteral( "load onto the copy + Solve re-derives the SAME flow plane words (hash %1 vs %2)" )
				.arg( h1, 16, 16, QLatin1Char( '0' ) ).arg( h2, 16, 16, QLatin1Char( '0' ) ),
				s2 && refused == 0 && h1 == h2 );
		}

		// ---- W5 / W6: PNG export -> import, and the flipped green ----------------
		const QFileInfo fi( file );
		const QString flowPng = fi.dir().filePath( fi.completeBaseName() + QStringLiteral( ".flow.png" ) );
		const QString maskPng = fi.dir().filePath( fi.completeBaseName() + QStringLiteral( ".bodies.png" ) );
		qint64 wet = 0;
		check( QStringLiteral( "the flow map exports as PNG with its body mask" ), win->exportPng( flowPng, maskPng, &wet ) );
		log << "exported " << wet << " wet texels to " << flowPng << "\n";
		check( QStringLiteral( "the export covers at least the river (%1 wet texels, river %2)" ).arg( wet ).arg( t1 ),
			wet >= t1 && t1 > 0 );
		const int layersBefore = win->model().rasters.size();
		const bool imported = win->importPng( flowPng );
		check( QStringLiteral( "the same PNG imports as a raster layer (%1 -> %2 layers)" )
			.arg( layersBefore ).arg( win->model().rasters.size() ),
			imported && win->model().rasters.size() == layersBefore + 1 );
		if ( imported ) {
			const WaterRasterLayer & r = win->model().rasters.last();
			qint64 differ = 0, painted = 0;
			doc->sweep( [&]( int px, int py, quint16 id, quint16, quint16 wordNow ) {
				quint16 w = 0;
				if ( !r.wordAt( px, py, w ) )
					return;
				painted++;
				if ( !id || w != wordNow )
					differ++;
			}, &err );
			log << "raster: " << painted << " painted, " << differ << " differ from the document's words\n";
			check( QStringLiteral( "export -> import reproduces the flow plane's words exactly (%1 differ of %2)" )
				.arg( differ ).arg( painted ), differ == 0 && painted >= t1 );
		}
		const QString flipped = fi.dir().filePath( fi.completeBaseName() + QStringLiteral( ".flow-flipped.png" ) );
		check( QStringLiteral( "the control: the green channel mirrored into a second PNG" ),
			WaterCurveDoc::flipGreen( flowPng, flipped, &err ) );
		const int layersNow = win->model().rasters.size();
		const bool flippedImported = win->importPng( flipped );
		log << "on the flipped PNG the window says: '" << win->summaryLabel()->text().section( QLatin1Char( '\n' ), 0, 0 ) << "'\n";
		check( QStringLiteral( "a flipped green channel is refused in words and stored nowhere (%1 layers)" ).arg( win->model().rasters.size() ),
			!flippedImported && win->model().rasters.size() == layersNow
			&& win->summaryLabel()->text().contains( QLatin1String( "green" ) ) );
		win->setRefusal( QString() );
		const bool again = win->importPng( flowPng );
		check( QStringLiteral( "and the unflipped one right after is accepted (%1 layers)" ).arg( win->model().rasters.size() ),
			again && win->model().rasters.size() == layersNow + 1 );
	}

	log << checksRun << " checks, " << fails << " failures\n";
	log << ( fails ? "FAIL" : "PASS" ) << "\n";
	logf.close();
	QTimer::singleShot( 100, qApp, &QApplication::quit );
}

} // namespace

// =========================================================================
//  the two entry points
// =========================================================================

QWidget * waterWindowOpen( QMainWindow * mw, const QString & lodlPath )
{
	if ( !theWindow ) {
		theWindow = new WaterWindow( mw );
		const QByteArray geom = QSettings().value( QStringLiteral( "WaterWindow/geometry" ) ).toByteArray();
		if ( !geom.isEmpty() )
			theWindow->restoreGeometry( geom );
	}
	theWindow->show();
	theWindow->raise();
	if ( !lodlPath.isEmpty() && ( !theWindow->document() || theWindow->document()->path() != lodlPath ) )
		theWindow->openFile( lodlPath );
	return theWindow;
}

void waterWindowInstall( QMainWindow * mw )
{
	if ( !mw )
		return;
	/* NO ENTRY IN THE WORKSPACES MENU (lane WATER7).
	 *
	 * bungo, 2026-09-10, on a screenshot of that menu: *"Two issues with water
	 * window and water marking appearing here"*, then *"They should be in the
	 * LOD gen workspace"*. Two of them, for one tool -- and the second issue is
	 * the one that settles this file: a POP-UP WINDOW IS NOT A WORKSPACE. A
	 * workspace switches the layout of the main window; this opens a separate
	 * top-level window, so it never belonged in a list of layouts, and a
	 * checkable radio entry beside "Default" could never tell the truth about
	 * it.
	 *
	 * The one door left is the button in the Water tab of the LOD Generation
	 * workspace (src/watermarkpanel.cpp), which is where bungo put the whole
	 * tool. `waterWindowOpen` is unchanged and so is everything the window
	 * does; only the menu entry is gone. */
	if ( qEnvironmentVariableIsSet( "WW_WATER_WINDOW_TEST" ) ) {
		/* A timer and not a load signal: this file knows nothing about the
		 * main document; the window opens its own landscape file. */
		QTimer::singleShot( 1200, mw, [mw]() { runWindowSelfTest( mw ); } );
	}
}
