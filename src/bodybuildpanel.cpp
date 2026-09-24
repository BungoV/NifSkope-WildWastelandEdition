/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* THE BODY BUILD DOCK. See bodybuildpanel.h for what it is and what it refuses
   to be. Two things are worth repeating beside the code itself:

   IT WRITES NO FILE. There is no QFile::open for writing in this translation
   unit outside the harness's own log, and there never will be: the panel's
   whole output is `Node::local` on the nodes of the scene that is open, and
   every one of those is put back exactly.

   THE UNDO IS BYTE FOR BYTE, and it is that way for the same reason
   `HkxPlayback::restore()` is: `savedLocal` holds the pre-preview Transform BY
   VALUE, so restoring assigns back the same bit patterns that were read, with
   no arithmetic in between. Rebuilding the transform from the NIF instead would
   go through `Transform( nif, index )` and could promise nothing. */

#include "bodybuildpanel.h"

#include "gamemanager.h"
#include "glview.h"
#include "nifskope.h"
#include "wwskin.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"		// lane GLTFEXPORT1: Shape::skinVertex, the harness (i) row
#include "model/nifmodel.h"
#include "ui/widgets/wwnumberfield.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWheelEvent>

#include <cmath>


namespace {

//! The unit equilateral triangle's height -- bodybuild.h's own h, so the
//! control and `bodyBuildCentroidK` measure the same triangle.
const float kTriH = 0.86602540378f;		// sqrt(3)/2

//! `*_skin`, the only bones this preview ever touches.
const char * const kSkinSuffix = "_skin";

/*! WRITING `Node::local` FROM OUTSIDE THE NODE.
 *
 *  `Node::local` is protected and its friend list is a fixed set of controller
 *  classes plus `HkxPlayback`; adding a line to that list is a change to
 *  `src/gl/glnode.h`, which belongs to the hook-up and not to this file. The
 *  route that needs no such change is the node's OWN public reader:
 *  `virtual const Transform & localTrans() const { return local; }` hands back a
 *  reference to the very member, and the object behind it is not const, so
 *  casting the const away and assigning is defined behaviour rather than a
 *  trick -- the same member `applyLocal()` writes, at the same place in the
 *  frame.
 *
 *  WHAT WOULD REFUTE IT: a `Node` subclass that overrides the no-argument
 *  `localTrans()` to return something other than its own `local` (the
 *  `int parentNode` overload beside it is a DIFFERENT function and is not used
 *  here). Measured 2026-09-19: nothing in `src/` overrides either, so every
 *  call lands on glnode.h:132. If one ever does, this must become a friend
 *  declaration instead, and the gate that catches it is (e) -- the preview
 *  would stop moving nodes and its FLOOR would go red.
 */
void writeLocal( Node * n, const Transform & t )
{
	const_cast<Transform &>( n->localTrans() ) = t;
}

//! Zero the negative weights and renormalise, twice, which is enough to land
//! any point of the plane inside the closed triangle.
void clampWeights( float & a, float & b, float & c )
{
	for ( int pass = 0; pass < 2; pass++ ) {
		if ( a < 0.0f ) a = 0.0f;
		if ( b < 0.0f ) b = 0.0f;
		if ( c < 0.0f ) c = 0.0f;
		const float s = a + b + c;
		if ( s <= 1.0e-6f ) {
			a = b = c = 1.0f / 3.0f;			// the centroid, the game's default
			return;
		}
		a /= s; b /= s; c /= s;
	}
}

/*! Where the RACE data is read from: the first Fallout4.esm the game manager
 *  serves. A miss is a refusal carrying the list of what was tried, never a
 *  silent empty combo. */
QString findFallout4Esm( QStringList * looked )
{
	for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) ) {
		const QString c = f + QStringLiteral( "/Fallout4.esm" );
		if ( looked )
			*looked << c;
		if ( QFile::exists( c ) )
			return c;
	}
	return QString();
}

/*! A folding section: the arrow folds the body, the heading names it, the fold
 *  persists. Blender's panel, less the check box in the title -- nothing in
 *  this panel is an optional OUTPUT, so there is nothing for such a box to
 *  switch, and a box that switches nothing is worse than no box. */
class BodyBuildSection final : public QWidget
{
public:
	BodyBuildSection( const QString & title, const QString & key, bool persist, QWidget * parent )
		: QWidget( parent ), settingsKey( QStringLiteral( "BodyBuild/expanded/" ) + key ), saves( persist )
	{
		setObjectName( QStringLiteral( "BodyBuild" ) + key + QStringLiteral( "Section" ) );
		auto * v = new QVBoxLayout( this );
		v->setContentsMargins( 0, 0, 0, 0 );
		v->setSpacing( 4 );
		auto * header = new QHBoxLayout();
		header->setContentsMargins( 0, 0, 0, 0 );
		header->setSpacing( 2 );
		arrow = new QToolButton( this );
		arrow->setObjectName( QStringLiteral( "BodyBuild" ) + key + QStringLiteral( "Expander" ) );
		arrow->setAutoRaise( true );
		arrow->setFixedSize( 16, 16 );
		arrow->setToolTip( tr( "Show or hide these settings" ) );
		header->addWidget( arrow, 0 );
		header->addWidget( wwHeading( title, this ), 1 );
		v->addLayout( header );
		bodyWidget = new QWidget( this );
		bodyWidget->setObjectName( QStringLiteral( "BodyBuild" ) + key + QStringLiteral( "Body" ) );
		v->addWidget( bodyWidget );
		// CONSTITUTION rule 6: a forced panel inherits nothing it might have saved.
		open = saves ? QSettings().value( settingsKey, true ).toBool() : true;
		apply();
		connect( arrow, &QToolButton::clicked, this, [this]() {
			open = !open;
			if ( saves )
				QSettings().setValue( settingsKey, open );
			apply();
		} );
	}
	QWidget * body() const { return bodyWidget; }
	bool isOpen() const { return open; }

private:
	void apply()
	{
		bodyWidget->setVisible( open );
		arrow->setArrowType( open ? Qt::DownArrow : Qt::RightArrow );
	}
	QToolButton * arrow = nullptr;
	QWidget * bodyWidget = nullptr;
	QString settingsKey;
	bool saves = true;
	bool open = true;
};

} // namespace


/*
 *  The triangle
 */

BodyBuildTriangle::BodyBuildTriangle( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "BodyBuildTriangle" ) );
	setCursor( Qt::CrossCursor );
	setMouseTracking( false );
	setFocusPolicy( Qt::ClickFocus );
}

QSize BodyBuildTriangle::sizeHint() const
{
	return QSize( 220, 200 );
}

QSize BodyBuildTriangle::minimumSizeHint() const
{
	return QSize( 120, 110 );
}

void BodyBuildTriangle::setWeights( float t, float m, float f )
{
	clampWeights( t, m, f );
	if ( t == wThin && m == wMuscular && f == wFat )
		return;
	wThin = t;
	wMuscular = m;
	wFat = f;
	update();
	emit weightsChanged( wThin, wMuscular, wFat );
}

void BodyBuildTriangle::setCentre()
{
	setWeights( 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f );
}

void BodyBuildTriangle::corners( QPointF & p0, QPointF & p1, QPointF & p2 ) const
{
	/* The captions sit above the two top corners and below the bottom one, so
	 * the triangle itself gets the rest. One caption height at each end, and the
	 * largest equilateral triangle that fits what is left. */
	const int cap = fontMetrics().height() + 3;
	const float availW = float( width() ) - 8.0f;
	const float availH = float( height() ) - float( cap * 2 ) - 6.0f;
	float side = availW;
	if ( side * kTriH > availH )
		side = ( availH > 0.0f ) ? availH / kTriH : 0.0f;
	if ( side < 1.0f )
		side = 1.0f;
	const float cx = float( width() ) * 0.5f;
	const float top = float( cap ) + 3.0f + ( availH - side * kTriH ) * 0.5f;
	p0 = QPointF( cx - side * 0.5f, top );					// THIN, top left
	p2 = QPointF( cx + side * 0.5f, top );					// FAT, top right
	p1 = QPointF( cx, top + side * kTriH );					// MUSCULAR, bottom
}

QPointF BodyBuildTriangle::handlePoint() const
{
	QPointF p0, p1, p2;
	corners( p0, p1, p2 );
	return QPointF( p0.x() * double( wThin ) + p1.x() * double( wMuscular ) + p2.x() * double( wFat ),
					p0.y() * double( wThin ) + p1.y() * double( wMuscular ) + p2.y() * double( wFat ) );
}

void BodyBuildTriangle::takePoint( const QPointF & p )
{
	QPointF p0, p1, p2;
	corners( p0, p1, p2 );
	const double side = p2.x() - p0.x();
	if ( side <= 0.0 )
		return;
	/* Screen -> the unit triangle of bodybuild.h: x across, y UP from the
	 * MUSCULAR corner, which is why the y term is subtracted rather than added. */
	const double ux = ( p.x() - p0.x() ) / side;
	const double uy = double( kTriH ) - ( p.y() - p0.y() ) / side;
	float m = float( 1.0 - uy / double( kTriH ) );
	float f = float( ux - 0.5 * double( m ) );
	float t = 1.0f - m - f;
	setWeights( t, m, f );
}

void BodyBuildTriangle::mousePressEvent( QMouseEvent * e )
{
	if ( e->button() != Qt::LeftButton ) {
		QWidget::mousePressEvent( e );
		return;
	}
	// Blender's colour picker: a press anywhere inside jumps the handle there
	// and begins the drag, rather than demanding the handle be hit first.
	dragging = true;
	takePoint( e->position() );
	e->accept();
}

void BodyBuildTriangle::mouseMoveEvent( QMouseEvent * e )
{
	if ( !dragging ) {
		QWidget::mouseMoveEvent( e );
		return;
	}
	takePoint( e->position() );
	e->accept();
}

void BodyBuildTriangle::mouseReleaseEvent( QMouseEvent * e )
{
	if ( dragging && e->button() == Qt::LeftButton ) {
		dragging = false;
		e->accept();
		return;
	}
	QWidget::mouseReleaseEvent( e );
}

void BodyBuildTriangle::wheelEvent( QWheelEvent * e )
{
	// The wheel scrolls the panel, not the build -- the number fields' rule
	// (wwGuardWheel), applied by hand because this is not a number field.
	e->ignore();
}

void BodyBuildTriangle::paintEvent( QPaintEvent * )
{
	QPointF p0, p1, p2;
	corners( p0, p1, p2 );

	QPainter p( this );
	p.setRenderHint( QPainter::Antialiasing, true );

	// Every colour through the skin table; no literal greys (wwSkinColor).
	const QColor ground( wwSkinColor( "bgInput" ) );
	const QColor edge( wwSkinColor( "borderStrong" ) );
	const QColor tick( wwSkinColor( "borderDim" ) );
	const QColor caption( wwSkinColor( "textMuted" ) );
	const QColor knob( wwSkinColor( "toggle" ) );
	const QColor ring( wwSkinColor( "textBright" ) );

	QPainterPath path;
	path.moveTo( p0 );
	path.lineTo( p1 );
	path.lineTo( p2 );
	path.closeSubpath();
	p.fillPath( path, ground );
	p.setPen( QPen( edge, 1.0 ) );
	p.drawPath( path );

	// the neutral default, marked where it is
	const QPointF centre( ( p0.x() + p1.x() + p2.x() ) / 3.0, ( p0.y() + p1.y() + p2.y() ) / 3.0 );
	p.setPen( QPen( tick, 1.0 ) );
	p.drawLine( QPointF( centre.x() - 4, centre.y() ), QPointF( centre.x() + 4, centre.y() ) );
	p.drawLine( QPointF( centre.x(), centre.y() - 4 ), QPointF( centre.x(), centre.y() + 4 ) );

	// the corner names -- names, not explanations
	const int cap = fontMetrics().height();
	p.setPen( caption );
	p.drawText( QRectF( 0, p0.y() - cap - 2, width() * 0.5, cap ),
				Qt::AlignLeft | Qt::AlignVCenter, tr( "Thin" ) );
	p.drawText( QRectF( width() * 0.5, p2.y() - cap - 2, width() * 0.5, cap ),
				Qt::AlignRight | Qt::AlignVCenter, tr( "Fat" ) );
	p.drawText( QRectF( 0, p1.y() + 2, width(), cap ),
				Qt::AlignHCenter | Qt::AlignVCenter, tr( "Muscular" ) );

	const QPointF h = handlePoint();
	p.setBrush( knob );
	p.setPen( QPen( ring, 1.5 ) );
	p.drawEllipse( h, 5.5, 5.5 );
}


/*
 *  The panel
 */

BodyBuildPanel::BodyBuildPanel( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "BodyBuildPanel" ) );
	forced = qEnvironmentVariableIsSet( "WW_BODY_BUILD" );
	buildUi();
	loadRaceList();

	/* THE FORCED STATE (CONSTITUTION rule 6). `WW_BODY_BUILD=<race>|<gender>|
	 * <t>,<m>,<f>` is the whole state this panel has, and when it is set the
	 * panel reads no QSettings and writes none, so a harness measures the build
	 * it asked for and never the one the last session left behind. */
	if ( forced ) {
		const QStringList part = qEnvironmentVariable( "WW_BODY_BUILD" ).split( QLatin1Char( '|' ) );
		if ( part.count() > 0 && !part.at( 0 ).trimmed().isEmpty() )
			setRaceByEditorId( part.at( 0 ).trimmed() );
		if ( part.count() > 1 ) {
			const QString g = part.at( 1 ).trimmed();
			setGender( ( g.compare( QStringLiteral( "female" ), Qt::CaseInsensitive ) == 0
						 || g == QLatin1String( "1" ) ) ? 1 : 0 );
		}
		if ( part.count() > 2 ) {
			const QStringList w = part.at( 2 ).split( QLatin1Char( ',' ) );
			if ( w.count() == 3 )
				setWeights( w.at( 0 ).toFloat(), w.at( 1 ).toFloat(), w.at( 2 ).toFloat() );
		}
		btnPreview->setChecked( true );
	} else {
		QSettings s;
		const QString race = s.value( QStringLiteral( "BodyBuild/race" ) ).toString();
		if ( !race.isEmpty() )
			setRaceByEditorId( race );
		setGender( s.value( QStringLiteral( "BodyBuild/gender" ), 0 ).toInt() );
		setWeights( s.value( QStringLiteral( "BodyBuild/thin" ), 1.0 / 3.0 ).toFloat(),
					s.value( QStringLiteral( "BodyBuild/muscular" ), 1.0 / 3.0 ).toFloat(),
					s.value( QStringLiteral( "BodyBuild/fat" ), 1.0 / 3.0 ).toFloat() );
		const QByteArray st = s.value( QStringLiteral( "BodyBuild/splitter" ) ).toByteArray();
		if ( !st.isEmpty() )
			split->restoreState( st );
		// The preview ships OFF: opening a dock must not move a character.
	}

	loadTable();
	refreshSummary();
}

BodyBuildPanel::~BodyBuildPanel()
{
	/* The scene outlives this widget in the ordinary teardown order, so the
	 * preview is taken off the nodes here rather than left on them. */
	restoreNodes();
}

void BodyBuildPanel::buildUi()
{
	/* THREE BANDS (nifskope-ww-panel-style): the settings scroll, the triangle
	 * sits under them on a splitter the user drags, and the summary line and the
	 * action bar are pinned under both and never scroll away. */
	auto * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->setSpacing( 0 );

	split = new QSplitter( Qt::Vertical, this );
	split->setObjectName( QStringLiteral( "BodyBuildSplitter" ) );
	split->setChildrenCollapsible( false );
	outer->addWidget( split, 1 );

	scroll = new QScrollArea( split );
	scroll->setObjectName( QStringLiteral( "BodyBuildSettingsScroll" ) );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	auto * page = new QWidget( scroll );
	page->setObjectName( QStringLiteral( "BodyBuildSettingsPage" ) );
	auto * pageLayout = new QVBoxLayout( page );
	pageLayout->setContentsMargins( 6, 6, 6, 6 );
	pageLayout->setSpacing( 5 );
	scroll->setWidget( page );
	split->addWidget( scroll );

	/* One `label | field` grid, ONE setting per row, and ONE label width for the
	 * whole panel so every value sits on the same edge down the page. */
	const int labelW = 96;					// fits "Muscular"
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
	auto form = [labelW]( QWidget * host ) {
		Form f;
		f.g = new QGridLayout( host );
		f.g->setContentsMargins( 0, 0, 0, 0 );
		f.g->setHorizontalSpacing( 8 );
		f.g->setVerticalSpacing( 4 );
		f.g->setColumnMinimumWidth( 0, labelW );
		f.g->setColumnStretch( 1, 1 );
		return f;
	};

	// ---- Race
	auto * raceSection = new BodyBuildSection( tr( "Race" ), QStringLiteral( "Race" ), !forced, page );
	pageLayout->addWidget( raceSection );
	Form raceForm = form( raceSection->body() );

	raceBox = new QComboBox( raceSection->body() );
	raceBox->setObjectName( QStringLiteral( "BodyBuildRace" ) );
	raceBox->setToolTip( tr( "The RACE record whose Bone Scale Data drives the triangle" ) );
	wwMatchFieldStyle( raceBox );
	raceForm.add( raceSection->body(), tr( "Race" ), raceBox );

	genderBox = new QComboBox( raceSection->body() );
	genderBox->setObjectName( QStringLiteral( "BodyBuildGender" ) );
	genderBox->addItem( tr( "Male" ), 0 );
	genderBox->addItem( tr( "Female" ), 1 );
	genderBox->setToolTip( tr( "Which of the race's two bone scale sets is read" ) );
	wwMatchFieldStyle( genderBox );
	raceForm.add( raceSection->body(), tr( "Gender" ), genderBox );

	// ---- Build
	auto * buildSection = new BodyBuildSection( tr( "Build" ), QStringLiteral( "Build" ), !forced, page );
	pageLayout->addWidget( buildSection );
	Form buildForm = form( buildSection->body() );

	auto makeWeight = [&]( const char * name, const QString & label, const QString & tip ) {
		auto * w = new QDoubleSpinBox( buildSection->body() );
		w->setObjectName( QLatin1String( name ) );
		w->setRange( 0.0, 1.0 );
		w->setDecimals( 3 );
		w->setSingleStep( 0.01 );
		w->setToolTip( tip );
		// the one number field of this program: drag to scrub, click to type,
		// and the wheel left to the panel
		wwMakeScrubField( w );
		buildForm.add( buildSection->body(), label, w );
		connect( w, &QDoubleSpinBox::editingFinished, this, &BodyBuildPanel::fieldEdited );
		connect( w, QOverload<double>::of( &QDoubleSpinBox::valueChanged ),
				 this, [this]( double ) { fieldEdited(); } );
		return w;
	};
	thinBox = makeWeight( "BodyBuildThin", tr( "Thin" ),
						  tr( "Corner weight; the three always sum to 1" ) );
	muscBox = makeWeight( "BodyBuildMuscular", tr( "Muscular" ),
						  tr( "Corner weight; the three always sum to 1" ) );
	fatBox = makeWeight( "BodyBuildFat", tr( "Fat" ),
						 tr( "Corner weight; the three always sum to 1" ) );
	pageLayout->addStretch( 1 );

	// ---- the live band
	auto * triHost = new QWidget( split );
	triHost->setObjectName( QStringLiteral( "BodyBuildTriangleHost" ) );
	auto * triLayout = new QVBoxLayout( triHost );
	triLayout->setContentsMargins( 6, 4, 6, 4 );
	triLayout->setSpacing( 0 );
	tri = new BodyBuildTriangle( triHost );
	tri->setToolTip( tr( "Drag to set the build; the centre is the game's default" ) );
	triLayout->addWidget( tri, 1 );
	split->addWidget( triHost );
	split->setStretchFactor( 0, 1 );
	split->setStretchFactor( 1, 1 );

	// ---- pinned: the summary-or-refusal line, then the action bar
	note = new QLabel( this );
	note->setObjectName( QStringLiteral( "BodyBuildNote" ) );
	note->setWordWrap( false );
	note->setContentsMargins( 8, 2, 8, 2 );
	outer->addWidget( note, 0 );

	actionBar = new QWidget( this );
	actionBar->setObjectName( QStringLiteral( "BodyBuildActionBar" ) );
	auto * bar = new QHBoxLayout( actionBar );
	bar->setContentsMargins( 6, 4, 6, 6 );
	bar->setSpacing( 6 );
	bar->addStretch( 1 );
	btnReset = new QPushButton( tr( "Reset" ), actionBar );
	btnReset->setObjectName( QStringLiteral( "BodyBuildReset" ) );
	btnReset->setToolTip( tr( "Put the handle back at the centre, the game's default build" ) );
	bar->addWidget( btnReset, 0 );
	btnPreview = new QPushButton( tr( "Preview" ), actionBar );
	btnPreview->setObjectName( QStringLiteral( "BodyBuildPreview" ) );
	btnPreview->setCheckable( true );
	btnPreview->setToolTip( tr( "Scale the loaded character's _skin bones; nothing is written to a file" ) );
	bar->addWidget( btnPreview, 0 );
	outer->addWidget( actionBar, 0 );

	connect( raceBox, QOverload<int>::of( &QComboBox::currentIndexChanged ),
			 this, [this]( int ) { raceChosen(); } );
	connect( genderBox, QOverload<int>::of( &QComboBox::currentIndexChanged ),
			 this, [this]( int ) { genderChosen(); } );
	connect( tri, &BodyBuildTriangle::weightsChanged, this, &BodyBuildPanel::triangleMoved );
	connect( btnPreview, &QPushButton::toggled, this, &BodyBuildPanel::previewToggled );
	connect( btnReset, &QPushButton::clicked, this, &BodyBuildPanel::resetToCentre );
	connect( split, &QSplitter::splitterMoved, this, [this]( int, int ) {
		saveSetting( QStringLiteral( "BodyBuild/splitter" ), split->saveState() );
	} );

	pushWeightsToFields();
}


/*
 *  The data layer's answers
 */

void BodyBuildPanel::loadRaceList()
{
	esmLookedIn.clear();
	races.clear();
	esm = findFallout4Esm( &esmLookedIn );

	const bool was = syncing;
	syncing = true;
	raceBox->clear();
	if ( !esm.isEmpty() ) {
		QString err;
		if ( !bodyBuildListRaces( esm, races, err ) )
			tableError = err;
		for ( const auto & r : races )
			raceBox->addItem( r.second, QVariant( r.first ) );
		// HumanRace is the default; the list is every RACE that carries the data.
		for ( int i = 0; i < raceBox->count(); i++ )
			if ( raceBox->itemData( i ).toUInt() == BODYBUILD_HUMAN_RACE ) {
				raceBox->setCurrentIndex( i );
				break;
			}
	}
	syncing = was;
}

void BodyBuildPanel::loadTable()
{
	table = BodyBuildTable();
	tableError.clear();
	if ( esm.isEmpty() ) {
		reapply();
		return;
	}
	const quint32 form = raceBox->count() > 0
		? raceBox->currentData().toUInt()
		: BODYBUILD_HUMAN_RACE;
	QString err;
	if ( !bodyBuildLoadRace( esm, form, table, err ) )
		tableError = err;
	else if ( table.isEmpty() )
		tableError = err;			// the loader's own refusal sentence
	reapply();
}


/*
 *  The preview
 */

Scene * BodyBuildPanel::scene() const
{
	return glView ? glView->getScene() : nullptr;
}

void BodyBuildPanel::redraw()
{
	if ( Scene * sc = scene() )
		sc->transformDirty = true;
	if ( glView )
		glView->update();
}

void BodyBuildPanel::restoreNodes()
{
	if ( savedLocal.isEmpty() )
		return;
	if ( Scene * sc = scene() ) {
		/* BYTE FOR BYTE. A Transform is nine rotation floats, three translation
		 * floats and a scale: assigning the saved copy writes back the same bit
		 * patterns that were read, with no arithmetic in between. */
		for ( Node * n : sc->getNodes() ) {
			if ( !n )
				continue;
			auto it = savedLocal.constFind( n->id() );
			if ( it != savedLocal.constEnd() )
				writeLocal( n, it.value() );
		}
	}
	savedLocal.clear();
	redraw();
}

int BodyBuildPanel::applyNodes( bool write )
{
	applied.clear();
	missing.clear();
	skinNodes = 0;

	Scene * sc = scene();
	if ( !sc )
		return 0;

	QHash<QString, Node *> byLower;
	for ( Node * n : sc->getNodes() ) {
		if ( !n )
			continue;
		const QString nm = n->getName();
		if ( nm.isEmpty() )
			continue;
		if ( nm.endsWith( QLatin1String( kSkinSuffix ), Qt::CaseInsensitive ) )
			skinNodes++;
		const QString lo = nm.toLower();
		if ( !byLower.contains( lo ) )
			byLower.insert( lo, n );
	}
	// skeleton.hkx has no *_skin bone at all; the body NIFs and skeleton.nif do.
	if ( skinNodes == 0 )
		return 0;

	const BodyBuildSet * set = table.set( gender() );
	if ( !set )
		return 0;

	const float wT = tri->thin(), wM = tri->muscular(), wF = tri->fat();

	for ( const BodyBuildBone & b : set->bones ) {
		Node * n = byLower.value( b.name.toLower(), nullptr );
		// SCALE ONLY, AND ONLY ON A `_skin` BONE. Those carry no animation
		// track, so this can never fight a clip that is playing.
		if ( !n || !n->getName().endsWith( QLatin1String( kSkinSuffix ), Qt::CaseInsensitive ) ) {
			missing << b.name;
			continue;
		}
		const Vector3 s = bodyBuildScale( b, wT, wM, wF );
		applied.insert( b.name, s );
		if ( !write )
			continue;
		if ( !savedLocal.contains( n->id() ) )
			savedLocal.insert( n->id(), n->localTrans() );
		/* A NifSkope Transform carries ONE scale and the build needs three, so
		 * the per-axis scale is folded into the 3x3 basis -- which is exactly
		 * what `Transform( translation, Vector3 )` in niftypes.h already does.
		 * R * S, not S * R: the scale is the bone's OWN cross section, applied
		 * in the bone's own frame before its rotation carries it to the parent. */
		const Transform base = savedLocal.value( n->id() );
		Matrix sm;
		sm( 0, 0 ) = s[0];
		sm( 1, 1 ) = s[1];
		sm( 2, 2 ) = s[2];
		Transform out = base;
		out.rotation = base.rotation * sm;
		writeLocal( n, out );
	}
	return applied.count();
}

void BodyBuildPanel::reapply()
{
	restoreNodes();
	applyNodes( previewOn() );
	redraw();
	refreshSummary();
}

void BodyBuildPanel::clearPreview()
{
	restoreNodes();
	applyNodes( false );
	redraw();
	refreshSummary();
}

void BodyBuildPanel::onSceneRebuilt()
{
	/* The Node objects those ids named are gone. Restoring onto whatever node
	 * has re-used an id would write a stale transform onto a stranger, so the
	 * record is DROPPED rather than replayed, and the preview is worked out
	 * again against the nodes that exist now. */
	savedLocal.clear();
	applyNodes( previewOn() );
	redraw();
	refreshSummary();
}

void BodyBuildPanel::refresh()
{
	loadTable();
}


/*
 *  Wiring
 */

void BodyBuildPanel::setNif( NifModel * model )
{
	nif = model;
	savedLocal.clear();
	reapply();
}

void BodyBuildPanel::setGLView( GLView * view )
{
	if ( glView == view )
		return;
	restoreNodes();
	glView = view;
	savedLocal.clear();
	reapply();
}

void BodyBuildPanel::setRaceByEditorId( const QString & editorId )
{
	for ( int i = 0; i < raceBox->count(); i++ ) {
		if ( raceBox->itemText( i ).compare( editorId, Qt::CaseInsensitive ) == 0 ) {
			raceBox->setCurrentIndex( i );
			return;
		}
	}
	// A name no RACE in the file carries leaves the list where it was; the
	// pinned line still names the race that IS selected, so nothing is silent.
}

void BodyBuildPanel::setGender( int g )
{
	const int i = genderBox->findData( ( g == 1 ) ? 1 : 0 );
	if ( i >= 0 )
		genderBox->setCurrentIndex( i );
}

void BodyBuildPanel::setWeights( float t, float m, float f )
{
	tri->setWeights( t, m, f );
	pushWeightsToFields();
}

void BodyBuildPanel::setPreviewOn( bool on )
{
	btnPreview->setChecked( on );
}

bool BodyBuildPanel::previewOn() const
{
	return btnPreview && btnPreview->isChecked() && btnPreview->isEnabled();
}

int BodyBuildPanel::gender() const
{
	return genderBox ? genderBox->currentData().toInt() : 0;
}

QString BodyBuildPanel::raceEditorId() const
{
	return raceBox ? raceBox->currentText() : QString();
}

bool BodyBuildPanel::hasSkinBones() const
{
	return skinNodes > 0;
}

Vector3 BodyBuildPanel::appliedScaleFor( const QString & boneName ) const
{
	return applied.value( boneName, Vector3( 1, 1, 1 ) );
}

QString BodyBuildPanel::summaryText() const
{
	return note ? note->text() : QString();
}

QString BodyBuildPanel::summaryDetail() const
{
	return noteDetail;
}

void BodyBuildPanel::saveSetting( const QString & key, const QVariant & value )
{
	if ( forced )
		return;				// CONSTITUTION rule 6: a forced panel saves nothing
	QSettings().setValue( key, value );
}

void BodyBuildPanel::pushWeightsToFields()
{
	const bool was = syncing;
	syncing = true;
	thinBox->setValue( double( tri->thin() ) );
	muscBox->setValue( double( tri->muscular() ) );
	fatBox->setValue( double( tri->fat() ) );
	syncing = was;
}

void BodyBuildPanel::raceChosen()
{
	if ( syncing )
		return;
	saveSetting( QStringLiteral( "BodyBuild/race" ), raceBox->currentText() );
	loadTable();
}

void BodyBuildPanel::genderChosen()
{
	if ( syncing )
		return;
	saveSetting( QStringLiteral( "BodyBuild/gender" ), gender() );
	reapply();
}

void BodyBuildPanel::fieldEdited()
{
	if ( syncing )
		return;
	/* THEY ALWAYS SUM TO 1. The field that was touched keeps what was typed and
	 * the other two are rescaled into what is left, in the ratio they were
	 * already in -- so editing one number never silently re-centres the build.
	 * Both are zero only at a corner, and there the remainder splits evenly. */
	const QObject * src = sender();
	double t = thinBox->value(), m = muscBox->value(), f = fatBox->value();
	double kept = 0.0, o1 = 0.0, o2 = 0.0;
	if ( src == muscBox ) {
		kept = m; o1 = t; o2 = f;
	} else if ( src == fatBox ) {
		kept = f; o1 = t; o2 = m;
	} else {
		kept = t; o1 = m; o2 = f;
	}
	if ( kept < 0.0 ) kept = 0.0;
	if ( kept > 1.0 ) kept = 1.0;
	const double rest = 1.0 - kept;
	const double sum = o1 + o2;
	if ( sum <= 1.0e-6 ) {
		o1 = o2 = rest * 0.5;
	} else {
		o1 = o1 * rest / sum;
		o2 = o2 * rest / sum;
	}
	if ( src == muscBox ) {
		t = o1; m = kept; f = o2;
	} else if ( src == fatBox ) {
		t = o1; m = o2; f = kept;
	} else {
		t = kept; m = o1; f = o2;
	}

	syncing = true;
	tri->setWeights( float( t ), float( m ), float( f ) );
	syncing = false;
	pushWeightsToFields();
	saveSetting( QStringLiteral( "BodyBuild/thin" ), double( tri->thin() ) );
	saveSetting( QStringLiteral( "BodyBuild/muscular" ), double( tri->muscular() ) );
	saveSetting( QStringLiteral( "BodyBuild/fat" ), double( tri->fat() ) );
	reapply();
}

void BodyBuildPanel::triangleMoved( float, float, float )
{
	if ( syncing )
		return;
	pushWeightsToFields();
	saveSetting( QStringLiteral( "BodyBuild/thin" ), double( tri->thin() ) );
	saveSetting( QStringLiteral( "BodyBuild/muscular" ), double( tri->muscular() ) );
	saveSetting( QStringLiteral( "BodyBuild/fat" ), double( tri->fat() ) );
	reapply();
}

void BodyBuildPanel::previewToggled( bool )
{
	// refreshSummary() unticks this button when the panel has to refuse, and
	// that must not come back round as a second pass over the nodes.
	if ( syncing )
		return;
	reapply();
}

void BodyBuildPanel::resetToCentre()
{
	tri->setCentre();
	pushWeightsToFields();
	reapply();
}

void BodyBuildPanel::say( const QString & text, bool refusal, const QString & detail )
{
	noteRefusal = refusal;
	noteDetail = detail.isEmpty() ? text : detail;
	note->setText( text );
	// NO BLURBS IN THE DOCK: the label says the few words, and everything the
	// sentence carried -- every missing bone by name -- is one hover away.
	note->setToolTip( noteDetail );
	note->setStyleSheet( QStringLiteral( "color:%1;" )
						 .arg( wwSkinColor( refusal ? "danger" : "textMuted" ) ) );
}

void BodyBuildPanel::refreshSummary()
{
	/* THE ACTION BAR SAYS WHAT WILL HAPPEN, IN WORDS, OR THE ONE REASON IT
	 * CANNOT. This function owns the Preview button's enabled state; a greyed
	 * button with no sentence beside it is a broken button. */
	bool can = false;
	if ( esm.isEmpty() ) {
		say( tr( "No Fallout4.esm" ), true,
			 tr( "The build triangle reads the RACE record's Bone Scale Data and no Fallout4.esm was found; looked in %1" )
			 .arg( esmLookedIn.isEmpty() ? tr( "no game folder at all" ) : esmLookedIn.join( QStringLiteral( ", " ) ) ) );
	} else if ( races.isEmpty() ) {
		say( tr( "No race carries build data" ), true,
			 tableError.isEmpty()
			 ? tr( "No RACE in %1 carries Bone Scale Data" ).arg( esm )
			 : tableError );
	} else if ( table.isEmpty() ) {
		say( tr( "No build data for this race" ), true,
			 tableError.isEmpty()
			 ? tr( "%1 carries no Bone Scale Data" ).arg( raceEditorId() )
			 : tableError );
	} else if ( !table.set( gender() ) ) {
		say( tr( "No set for this gender" ), true,
			 tr( "%1 carries no Bone Scale Data for the %2 set" )
			 .arg( raceEditorId(), gender() == 1 ? tr( "female" ) : tr( "male" ) ) );
	} else if ( !scene() ) {
		say( tr( "No character loaded" ), true,
			 tr( "The build triangle previews on the character that is open; nothing is." ) );
	} else if ( skinNodes == 0 ) {
		say( tr( "No _skin bones in this file" ), true,
			 tr( "The build scales the *_skin bones only. The body meshes and skeleton.nif carry them; skeleton.hkx does not, so nothing was applied." ) );
	} else {
		can = true;
		const int total = applied.count() + missing.count();
		const QString shortText = previewOn()
			? tr( "%1 of %2 bones scaled" ).arg( applied.count() ).arg( total )
			: tr( "%1 of %2 bones ready" ).arg( applied.count() ).arg( total );
		QString detail = tr( "%1, %2: %3 of %4 bones found on this character" )
			.arg( raceEditorId(), gender() == 1 ? tr( "female" ) : tr( "male" ) )
			.arg( applied.count() ).arg( total );
		if ( !missing.isEmpty() )
			detail += tr( ". Not on this character: %1" ).arg( missing.join( QStringLiteral( ", " ) ) );
		detail += tr( ". Preview only -- no file is written." );
		say( shortText, false, detail );
	}

	if ( btnPreview ) {
		btnPreview->setEnabled( can );
		if ( !can && btnPreview->isChecked() ) {
			const bool was = syncing;
			syncing = true;
			btnPreview->setChecked( false );
			syncing = was;
		}
	}
}


/* =========================================================================
 *  WW_BODY_BUILD -- the gates, run inside the real application.
 *
 *  `WW_BODY_BUILD=<race>|<gender>|<t>,<m>,<f>` both FORCES the panel's state
 *  (the constructor above reads the same variable) and runs this, so the gate
 *  and the panel can never be measuring two different builds.
 *  `WW_BODY_BUILD_SHOT=<png>` also grabs the dock.
 *  `WW_BODY_BUILD_VIEW_SHOT=<prefix>` also photographs the BODY, front and
 *  side, as <prefix>_front.png and <prefix>_side.png.
 *
 *  Every number is read off the WIDGETS and off the panel's public readers, and
 *  the pose off `Node::local` in the scene -- never off the panel's own record
 *  of what it thinks it wrote. The independent instrument is `bodybuild.h`
 *  itself, re-read here from the same .esm.
 * ========================================================================= */

namespace {

struct WwBbState
{
	QTextStream * out = nullptr;
	int checks = 0, fails = 0;
};

void say( WwBbState & st, const QString & line )
{
	if ( st.out )
		*st.out << line << "\n";
}

void check( WwBbState & st, const QString & what, bool ok )
{
	st.checks++;
	if ( !ok )
		st.fails++;
	say( st, QStringLiteral( "%1 %2" ).arg( ok ? QStringLiteral( "ok  " ) : QStringLiteral( "FAIL" ), what ) );
}

//! Exact equality, field by field -- the undo gate needs no tolerance at all.
bool sameTransform( const Transform & a, const Transform & b )
{
	for ( unsigned c = 0; c < 3; c++ )
		for ( unsigned d = 0; d < 3; d++ )
			if ( a.rotation( c, d ) != b.rotation( c, d ) )
				return false;
	for ( int i = 0; i < 3; i++ )
		if ( a.translation[i] != b.translation[i] )
			return false;
	return a.scale == b.scale;
}

} // namespace

void wwBodyBuildHarness( NifSkope * skope )
{
	if ( !skope || !qEnvironmentVariableIsSet( "WW_BODY_BUILD" ) )
		return;

	QObject::connect( skope, &NifSkope::completeLoading, skope, [skope]( bool ok, QString & ) {
		static bool ran = false;
		if ( ran )
			return;
		ran = true;
		QTimer::singleShot( 1200, skope, [skope, ok]() {
			QFile logf( QApplication::applicationDirPath() + QStringLiteral( "/ww_body_build_test.log" ) );
			if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
				return;
			QTextStream log( &logf );
			WwBbState st;
			st.out = &log;

			auto finish = [&]() {
				log << st.checks << " checks, " << st.fails << " failures\n";
				log << ( st.fails == 0 ? "PASS" : "FAIL" ) << "\n";
				log << "done\n";
				log.flush();
				logf.close();
				skope->setWindowModified( false );
				QTimer::singleShot( 100, qApp, &QApplication::quit );
			};

			check( st, "the fixture loaded", ok );
			auto * panel = skope->findChild<BodyBuildPanel *>( QStringLiteral( "BodyBuildPanel" ) );
			check( st, "(a) the Body Build panel exists (hook-up applied)", panel != nullptr );
			GLView * ogl = skope->getGLView();
			if ( !panel || !ogl ) {
				finish();
				return;
			}
			if ( auto * dock = qobject_cast<QDockWidget *>( panel->parentWidget() ) )
				dock->show();
			panel->show();
			qApp->processEvents();

			check( st, "(a) the panel is forced by the environment, not by QSettings",
				   panel->forcedByEnvironment() );

			auto * raceBox = panel->findChild<QComboBox *>( QStringLiteral( "BodyBuildRace" ) );
			auto * genderBox = panel->findChild<QComboBox *>( QStringLiteral( "BodyBuildGender" ) );
			auto * thinBox = panel->findChild<QDoubleSpinBox *>( QStringLiteral( "BodyBuildThin" ) );
			auto * muscBox = panel->findChild<QDoubleSpinBox *>( QStringLiteral( "BodyBuildMuscular" ) );
			auto * fatBox = panel->findChild<QDoubleSpinBox *>( QStringLiteral( "BodyBuildFat" ) );
			const BodyBuildTriangle * tri = panel->triangle();
			check( st, "(a) race, gender, the three fields and the triangle are all there",
				   raceBox && genderBox && thinBox && muscBox && fatBox && tri );
			if ( !raceBox || !genderBox || !thinBox || !muscBox || !fatBox || !tri ) {
				finish();
				return;
			}

			// ---- (a) the race list, with its floor
			bool hasHuman = false, hasNonsense = false;
			for ( int i = 0; i < raceBox->count(); i++ ) {
				if ( raceBox->itemText( i ).compare( QStringLiteral( "HumanRace" ), Qt::CaseInsensitive ) == 0 )
					hasHuman = true;
				if ( raceBox->itemText( i ).compare( QStringLiteral( "NotARace" ), Qt::CaseInsensitive ) == 0 )
					hasNonsense = true;
			}
			say( st, QStringLiteral( "races listed: %1, esm: %2" )
				 .arg( raceBox->count() ).arg( panel->esmPath() ) );
			check( st, "(a) the race list carries HumanRace", hasHuman && raceBox->count() >= 1 );
			// FLOOR: a name no RACE carries is NOT in the list, so the check above
			// is reading the file and not just a non-empty combo.
			check( st, "(a) FLOOR: a race that does not exist is not listed", !hasNonsense );

			// ---- (b) the forced state landed on the widgets
			const float wt = tri->thin(), wm = tri->muscular(), wf = tri->fat();
			say( st, QStringLiteral( "state: %1 / %2 / %3,%4,%5" )
				 .arg( panel->raceEditorId() )
				 .arg( panel->gender() ).arg( double( wt ) ).arg( double( wm ) ).arg( double( wf ) ) );
			check( st, "(b) the three weights sum to 1", std::fabs( wt + wm + wf - 1.0f ) < 1.0e-4f );
			check( st, "(b) the fields carry what the triangle carries",
				   std::fabs( thinBox->value() - double( wt ) ) < 1.0e-3
				   && std::fabs( muscBox->value() - double( wm ) ) < 1.0e-3
				   && std::fabs( fatBox->value() - double( wf ) ) < 1.0e-3 );

			// ---- (c) two-way binding, and the normalisation
			panel->setWeights( 1.0f, 0.0f, 0.0f );
			qApp->processEvents();
			check( st, "(c) triangle -> fields at the THIN corner",
				   std::fabs( thinBox->value() - 1.0 ) < 1.0e-3
				   && std::fabs( muscBox->value() ) < 1.0e-3
				   && std::fabs( fatBox->value() ) < 1.0e-3 );
			// type an un-normalised triple: the panel must normalise it
			thinBox->setValue( 0.5 );
			qApp->processEvents();
			const double s3 = thinBox->value() + muscBox->value() + fatBox->value();
			say( st, QStringLiteral( "after typing thin=0.5: %1 %2 %3 (sum %4)" )
				 .arg( thinBox->value() ).arg( muscBox->value() ).arg( fatBox->value() ).arg( s3 ) );
			check( st, "(c) fields -> triangle, and the three still sum to 1",
				   std::fabs( s3 - 1.0 ) < 1.0e-3
				   && std::fabs( double( tri->thin() ) - thinBox->value() ) < 1.0e-3 );

			// ---- the independent instrument: bodybuild.h re-read here
			BodyBuildTable table;
			QString err;
			const bool haveTable = !panel->esmPath().isEmpty()
				&& bodyBuildLoadRace( panel->esmPath(), BODYBUILD_HUMAN_RACE, table, err );
			const BodyBuildSet * set = haveTable ? table.set( panel->gender() ) : nullptr;
			say( st, QStringLiteral( "independent table: %1 bones%2" )
				 .arg( set ? set->bones.count() : 0 )
				 .arg( err.isEmpty() ? QString() : QStringLiteral( " (" ) + err + QStringLiteral( ")" ) ) );

			// ---- (d) the centroid is the control (bodybuild.h's own refuter)
			panel->setRaceByEditorId( QStringLiteral( "HumanRace" ) );
			panel->setWeights( 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f );
			panel->setPreviewOn( true );
			qApp->processEvents();
			int off = 0, checked = 0;
			double worst = 0.0;
			if ( set ) {
				for ( const BodyBuildBone & b : set->bones ) {
					const Vector3 v = panel->appliedScaleFor( b.name );
					if ( !panel->refusedBoneNames().contains( b.name ) ) {
						checked++;
						for ( int i = 0; i < 3; i++ ) {
							const double d = std::fabs( double( v[i] ) - 1.0 );
							if ( d > worst )
								worst = d;
							if ( d > 1.0e-4 )
								off++;
						}
					}
				}
			}
			say( st, QStringLiteral( "centroid: %1 bones checked, %2 axes off 1.0, worst %3" )
				 .arg( checked ).arg( off ).arg( worst ) );
			check( st, "(d) at the centre every applied bone is (1,1,1) -- the shipped Default file's own answer",
				   checked > 0 && off == 0 );
			// FLOOR: the same measurement at a CORNER must NOT come out (1,1,1),
			// or the check above would pass on a panel that applies nothing.
			panel->setWeights( 1.0f, 0.0f, 0.0f );
			qApp->processEvents();
			int moved = 0;
			double biggest = 0.0;
			if ( set ) {
				for ( const BodyBuildBone & b : set->bones ) {
					const Vector3 v = panel->appliedScaleFor( b.name );
					for ( int i = 0; i < 3; i++ ) {
						const double d = std::fabs( double( v[i] ) - 1.0 );
						if ( d > biggest )
							biggest = d;
						if ( d > 1.0e-3 )
							moved++;
					}
				}
			}
			say( st, QStringLiteral( "thin corner: %1 axes away from 1.0, biggest %2" )
				 .arg( moved ).arg( biggest ) );
			check( st, "(d) FLOOR: at the THIN corner the scales are not 1.0", moved > 0 );
			// and the panel's number is the data layer's own number, not a copy
			int disagree = 0;
			if ( set ) {
				for ( const BodyBuildBone & b : set->bones ) {
					if ( panel->refusedBoneNames().contains( b.name ) )
						continue;
					const Vector3 want = bodyBuildScale( b, 1.0f, 0.0f, 0.0f );
					const Vector3 got = panel->appliedScaleFor( b.name );
					for ( int i = 0; i < 3; i++ )
						if ( std::fabs( double( want[i] ) - double( got[i] ) ) > 1.0e-5 )
							disagree++;
				}
			}
			check( st, "(d) the panel's scale IS bodyBuildScale's, bone for bone", disagree == 0 );

			// ---- (e) the preview is exactly undoable
			Scene * sc = ogl->getScene();
			check( st, "(e) the scene is there", sc != nullptr );
			if ( sc ) {
				panel->setPreviewOn( false );
				qApp->processEvents();
				QHash<int, Transform> before;
				for ( Node * n : sc->getNodes() )
					if ( n )
						before.insert( n->id(), n->localTrans() );
				panel->setWeights( 0.0f, 0.0f, 1.0f );		// the FAT corner
				panel->setPreviewOn( true );
				qApp->processEvents();
				int changed = 0;
				for ( Node * n : sc->getNodes() ) {
					if ( !n )
						continue;
					auto it = before.constFind( n->id() );
					if ( it != before.constEnd() && !sameTransform( it.value(), n->localTrans() ) )
						changed++;
				}
				say( st, QStringLiteral( "preview on: %1 nodes moved, %2 bones applied, %3 refused, skin bones %4" )
					 .arg( changed ).arg( panel->appliedCount() ).arg( panel->refusedCount() )
					 .arg( panel->hasSkinBones() ? 1 : 0 ) );
				// FLOOR for (e): unless something actually moved, the undo below
				// proves nothing at all.
				check( st, "(e) FLOOR: the preview actually moved nodes", changed > 0 );
				panel->setPreviewOn( false );
				qApp->processEvents();
				int differs = 0;
				for ( Node * n : sc->getNodes() ) {
					if ( !n )
						continue;
					auto it = before.constFind( n->id() );
					if ( it != before.constEnd() && !sameTransform( it.value(), n->localTrans() ) )
						differs++;
				}
				check( st, "(e) preview off restores every node BYTE FOR BYTE", differs == 0 );
				panel->setPreviewOn( true );
				qApp->processEvents();
			}

			// ---- (f) the census adds up, and the tooltip names the refused
			const int total = panel->appliedCount() + panel->refusedCount();
			check( st, "(f) applied + refused is the set's bone count",
				   set ? ( total == set->bones.count() ) : ( total > 0 ) );
			bool named = true;
			for ( const QString & b : panel->refusedBoneNames() )
				if ( !panel->summaryDetail().contains( b ) )
					named = false;
			say( st, QStringLiteral( "summary: %1 | detail: %2" )
				 .arg( panel->summaryText(), panel->summaryDetail() ) );
			check( st, "(f) the summary line is not empty and the tooltip names every refused bone",
				   !panel->summaryText().isEmpty() && named );

			// ---- (g) the panel-style counts, each with a floor
			int numbers = 0, plain = 0;
			for ( QAbstractSpinBox * s : panel->findChildren<QAbstractSpinBox *>() ) {
				numbers++;
				if ( !s->property( "wwScrubbed" ).toBool() )
					plain++;
			}
			say( st, QStringLiteral( "number fields: %1, left as plain spin boxes: %2" ).arg( numbers ).arg( plain ) );
			check( st, "(g) every number is a scrub field (floor 3)", numbers >= 3 && plain == 0 );

			const int groups = panel->findChildren<QGroupBox *>().size();
			int headings = 0;
			for ( QLabel * l : panel->findChildren<QLabel *>() )
				if ( l->styleSheet().contains( QLatin1String( "font-weight: 600" ) ) )
					headings++;
			say( st, QStringLiteral( "group boxes: %1, headings: %2" ).arg( groups ).arg( headings ) );
			check( st, "(g) sections are wwHeading labels, not group-box frames", groups == 0 && headings >= 2 );

			int combos = 0, unmatched = 0;
			for ( QComboBox * c : panel->findChildren<QComboBox *>() ) {
				combos++;
				if ( !c->styleSheet().contains( QLatin1String( "drop-down" ) ) )
					unmatched++;
			}
			say( st, QStringLiteral( "selectors: %1, in default chrome: %2" ).arg( combos ).arg( unmatched ) );
			check( st, "(g) every selector takes the matched field chrome (floor 2)",
				   combos >= 2 && unmatched == 0 );

			int dashed = 0, untipped = 0;
			for ( QWidget * w : QList<QWidget *>{ raceBox, genderBox, thinBox, muscBox, fatBox,
					panel->findChild<QPushButton *>( QStringLiteral( "BodyBuildPreview" ) ),
					panel->findChild<QPushButton *>( QStringLiteral( "BodyBuildReset" ) ) } ) {
				if ( !w || w->toolTip().isEmpty() )
					untipped++;
			}
			for ( QLabel * l : panel->findChildren<QLabel *>() )
				if ( l->text().contains( QLatin1String( " - " ) ) )
					dashed++;
			say( st, QStringLiteral( "controls without a tooltip: %1, labels with a dash explanation: %2" )
				 .arg( untipped ).arg( dashed ) );
			check( st, "(g) labels are names, the explanation is the tooltip", untipped == 0 && dashed == 0 );

			// one field per row: three distinct y positions, read as GEOMETRY
			QList<int> ys;
			for ( QWidget * w : QList<QWidget *>{ thinBox, muscBox, fatBox } ) {
				const int y = w->mapTo( panel, QPoint( 0, 0 ) ).y();
				if ( !ys.contains( y ) )
					ys.append( y );
			}
			say( st, QStringLiteral( "weight fields on distinct rows: %1" ).arg( ys.size() ) );
			check( st, "(g) the weight fields sit one to a row", ys.size() == 3 );

			auto * split = panel->findChild<QSplitter *>( QStringLiteral( "BodyBuildSplitter" ) );
			auto * scroll = panel->findChild<QScrollArea *>( QStringLiteral( "BodyBuildSettingsScroll" ) );
			auto * noteLabel = panel->findChild<QLabel *>( QStringLiteral( "BodyBuildNote" ) );
			auto * bar = panel->findChild<QWidget *>( QStringLiteral( "BodyBuildActionBar" ) );
			auto inside = []( const QWidget * w, const QWidget * host ) {
				for ( const QWidget * p = w ? w->parentWidget() : nullptr; p; p = p->parentWidget() )
					if ( p == host )
						return true;
				return false;
			};
			check( st, "(g) three bands: settings scroll, triangle on the splitter, note and bar pinned outside it",
				   split && scroll && noteLabel && bar
				   && inside( scroll, split ) && inside( tri, split )
				   && !inside( noteLabel, split ) && !inside( bar, split ) );

			// the wheel over the UNFOCUSED thin field leaves it
			const double v0 = thinBox->value();
			thinBox->clearFocus();
			QWheelEvent we( QPointF( 10, 10 ), thinBox->mapToGlobal( QPoint( 10, 10 ) ),
							QPoint( 0, 120 ), QPoint( 0, 120 ), Qt::NoButton, Qt::NoModifier,
							Qt::NoScrollPhase, false );
			QApplication::sendEvent( thinBox, &we );
			check( st, QStringLiteral( "(g) the wheel over the unfocused Thin field leaves it: %1 -> %2" )
				   .arg( v0 ).arg( thinBox->value() ), thinBox->value() == v0 );
			say( st, QStringLiteral( "     (the focused half cannot fire in an inactive harness window: hasFocus %1, window active %2)" )
				 .arg( thinBox->hasFocus() ).arg( panel->window()->isActiveWindow() ) );
			// and the triangle hands the wheel on too
			QWheelEvent we2( QPointF( 10, 10 ), QPoint( 10, 10 ), QPoint( 0, 120 ), QPoint( 0, 120 ),
							 Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false );
			const float tw = tri->thin();
			QApplication::sendEvent( const_cast<BodyBuildTriangle *>( tri ), &we2 );
			check( st, "(g) the wheel over the triangle scrolls the panel, not the build",
				   tri->thin() == tw );

			// ---- the picture
			const QString shot = qEnvironmentVariable( "WW_BODY_BUILD_SHOT" );
			if ( !shot.isEmpty() ) {
				QWidget * grabTarget = panel;
				if ( auto * dock = qobject_cast<QDockWidget *>( panel->parentWidget() ) )
					grabTarget = dock;
				qApp->processEvents();
				qApp->processEvents();
				const bool wrote = grabTarget->grab().save( shot );
				say( st, QStringLiteral( "shot: %1 -> %2" ).arg( shot, wrote ? QStringLiteral( "written" ) : QStringLiteral( "FAILED" ) ) );
			}

			/* ---- THE BODY ITSELF, front and side.
			 *
			 * The grab above photographs the DOCK, which shows the numbers and
			 * not the body they moved. `WW_BODY_BUILD_VIEW_SHOT=<prefix>` adds
			 * the two viewport pictures the brief asks for, <prefix>_front.png
			 * and <prefix>_side.png, read off the GL buffer the same way the
			 * skeleton-view harness does (src/nifskope_ui.cpp:3684).
			 *
			 * setOrientation( state, true ) ends in center(), whose auto-fit
			 * runs inside the NEXT paintGL and not inside the call -- the
			 * comment at src/glview.h:546 -- so each view is pumped before it
			 * is grabbed, and a null image is reported rather than saved
			 * silently. */
			const QString viewShot = qEnvironmentVariable( "WW_BODY_BUILD_VIEW_SHOT" );
			if ( !viewShot.isEmpty() && ogl ) {
				const GLView::ViewState was = ogl->viewState();
				const struct { GLView::ViewState v; const char * tag; } views[2] = {
					{ GLView::ViewFront, "front" }, { GLView::ViewLeft, "side" }
				};
				for ( const auto & vw : views ) {
					ogl->setOrientation( vw.v, true );
					for ( int i = 0; i < 4; i++ ) {
						ogl->update();
						qApp->processEvents();
					}
					const QString f = QStringLiteral( "%1_%2.png" ).arg( viewShot, QString::fromLatin1( vw.tag ) );
					const QImage fb = ogl->grabFramebuffer();
					const bool ok = !fb.isNull() && fb.save( f );
					say( st, QStringLiteral( "view shot: %1 (%2x%3) -> %4" )
						 .arg( f ).arg( fb.width() ).arg( fb.height() )
						 .arg( ok ? QStringLiteral( "written" ) : QStringLiteral( "FAILED" ) ) );
				}
				ogl->setOrientation( was, true );
				ogl->update();
				qApp->processEvents();
			}

			/* ---- (i) A BELLY VERTEX MOVES OUTWARD, thin -> fat.
			 *
			 * Every check above reads the panel's own numbers. This one reads
			 * the MESH, through Shape::skinVertex -- "the same blended vertex
			 * skin transform used by the GPU" (src/gl/glshape.h:79) -- so it
			 * is the deformation a person would see and not the intention.
			 *
			 * THE PREDICTION, and it is a bracket rather than a single number
			 * because a real belly vertex is weighted to more than one bone:
			 * a vertex driven ONLY by Belly_skin moves so that its offset from
			 * that bone grows by the bone's own per-axis scale ratio, so the
			 * largest such ratio is an upper bound for every vertex, and 1 is
			 * the lower one (outward, never inward). A vertex that moved more
			 * than the bracket allows means the scale is being applied twice
			 * or in the wrong frame.
			 *
			 * FLOOR: the same state read twice must give 0. Without it, a
			 * cache that never refreshed and a body that never moved look the
			 * same from here. */
			if ( sc && !sc->shapes.isEmpty() ) {
				Shape * shape = nullptr;
				// The shape with the most vertices. Shape::isSkinned is protected,
				// and it does not need to be asked: skinVertex() hands an UNSKINNED
				// vertex straight back unchanged (glshape.cpp:246), so an unskinned
				// shape shows up as "nothing moved" and fails the row by itself.
				for ( Shape * s2 : sc->shapes )
					if ( s2 && ( !shape || s2->verts.count() > shape->verts.count() ) )
						shape = s2;
				auto readAll = [&]( QVector<Vector3> & out ) {
					out.clear();
					ogl->update();
					qApp->processEvents();
					qApp->processEvents();
					if ( !shape )
						return;
					for ( int i = 0; i < shape->verts.count(); i++ )
						out << shape->skinVertex( i, shape->verts[i] );
				};
				QVector<Vector3> pThin, pThin2, pFat;
				panel->setPreviewOn( true );
				panel->setWeights( 1.0f, 0.0f, 0.0f );
				readAll( pThin );
				readAll( pThin2 );
				panel->setWeights( 0.0f, 0.0f, 1.0f );
				readAll( pFat );
				check( st, "(i) a skinned shape with vertices was found", shape != nullptr
					   && !pThin.isEmpty() && pThin.count() == pFat.count() );
				if ( shape && !pThin.isEmpty() && pThin.count() == pFat.count() ) {
					double floorMove = 0.0;
					for ( int i = 0; i < pThin.count() && i < pThin2.count(); i++ )
						floorMove = std::max( floorMove, double( ( pThin[i] - pThin2[i] ).length() ) );
					int worst = 0;
					double biggestMove = 0.0;
					for ( int i = 0; i < pThin.count(); i++ ) {
						const double d = double( ( pFat[i] - pThin[i] ).length() );
						if ( d > biggestMove ) { biggestMove = d; worst = i; }
					}
					say( st, QStringLiteral( "vertex move thin->fat: worst %1 units on vertex %2 "
											 "(FLOOR, the same state twice: %3)" )
						 .arg( biggestMove ).arg( worst ).arg( floorMove ) );
					check( st, "(i) FLOOR: the same state read twice does not move", floorMove < 1.0e-5 );
					check( st, "(i) the body changes shape between THIN and FAT", biggestMove > 0.1 );

					// the bracket, around Belly_skin
					const BodyBuildBone * belly = set ? set->find( QStringLiteral( "Belly_skin" ) ) : nullptr;
					Node * bellyNode = nullptr;
					for ( Node * n : sc->getNodes() )
						if ( n && n->getName().compare( QStringLiteral( "Belly_skin" ), Qt::CaseInsensitive ) == 0 )
							{ bellyNode = n; break; }
					if ( belly && bellyNode ) {
						double maxRatio = 1.0;
						for ( int i = 0; i < 3; i++ ) {
							const double t0 = double( belly->thin[i] ), f0 = double( belly->fat[i] );
							if ( t0 > 1.0e-6 )
								maxRatio = std::max( maxRatio, f0 / t0 );
						}
						// skinVertex answers in the SHAPE's own object space (glshape.cpp:104,
						// wtInv = worldTrans().inverted()), so the bone's origin has to be
						// brought into the same space or the two are not comparable.
						const Vector3 o = shape->worldTrans().inverted() * bellyNode->worldTrans().translation;
						const double rT = double( ( pThin[worst] - o ).length() );
						const double rF = double( ( pFat[worst] - o ).length() );
						const double ratio = rT > 1.0e-6 ? rF / rT : 0.0;
						say( st, QStringLiteral( "the worst-moving vertex sits %1 u from Belly_skin at THIN "
												 "and %2 u at FAT: ratio %3, the bone's own largest ratio is %4" )
							 .arg( rT ).arg( rF ).arg( ratio ).arg( maxRatio ) );
						check( st, "(i) it moved OUTWARD and by no more than the bone's own scale allows",
							   ratio > 1.0 && ratio <= maxRatio * 1.05 );
					} else {
						check( st, "(i) Belly_skin is in the table and in the scene",
							   belly != nullptr && bellyNode != nullptr );
					}
				}
			}

			/* ---- WW_BODY_BUILD_DUMP: the panel's own answer, in the SAME
			 * shape the Python arm prints it.
			 *
			 *   ROW <gender> <state> <bone> x y z
			 *
			 * This is what lets tests/spells/body_build.sh compare two
			 * INDEPENDENT instruments -- this C++ panel, driving real Node
			 * objects, against scratchpad/gltfexport1_20260919/body_build_table.py,
			 * which is in turn compared against the eight files Bethesda
			 * shipped. Everything above re-reads bodybuild.h in the same
			 * process, which cannot catch an error that is IN bodybuild.h.
			 * The dump is written for the four named states, the panel is put
			 * back where the harness left it, and the gender written on each
			 * row is the panel's, so a female table dumped onto a male body
			 * announces itself. */
			const QString dump = qEnvironmentVariable( "WW_BODY_BUILD_DUMP" );
			if ( !dump.isEmpty() ) {
				QFile df( dump );
				if ( df.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
					QTextStream ds( &df );
					const char * const stateName[4] = { "thin", "muscular", "fat", "centroid" };
					const float wt0 = tri->thin(), wm0 = tri->muscular(), wf0 = tri->fat();
					int rows = 0;
					for ( int c = 0; c < 4; c++ ) {
						float a, b2, f2;
						bodyBuildCornerWeights( c, a, b2, f2 );
						panel->setWeights( a, b2, f2 );
						qApp->processEvents();
						if ( !set )
							continue;
						for ( const BodyBuildBone & b : set->bones ) {
							if ( panel->refusedBoneNames().contains( b.name ) )
								continue;
							const Vector3 v = panel->appliedScaleFor( b.name );
							ds << "ROW " << ( panel->gender() == 1 ? "female" : "male" ) << ' '
							   << stateName[c] << ' ' << b.name << ' '
							   << QString::number( double( v[0] ), 'f', 6 ) << ' '
							   << QString::number( double( v[1] ), 'f', 6 ) << ' '
							   << QString::number( double( v[2] ), 'f', 6 ) << '\n';
							rows++;
						}
					}
					ds.flush();
					df.close();
					panel->setWeights( wt0, wm0, wf0 );
					qApp->processEvents();
					say( st, QStringLiteral( "dump: %1 rows -> %2" ).arg( rows ).arg( dump ) );
					check( st, "(h) the dump carries every bone at all four states",
						   set ? rows == 4 * ( set->bones.count() - panel->refusedCount() ) : rows > 0 );
				} else {
					check( st, "(h) WW_BODY_BUILD_DUMP could be opened", false );
				}
			}

			finish();
		} );
	} );
}
