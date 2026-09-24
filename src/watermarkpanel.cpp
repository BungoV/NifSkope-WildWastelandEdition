/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "watermarkpanel.h"

#include "watermark.h"
#include "waterwindow.h"
#include "wwskin.h"
#include "ui/widgets/wwnumberfield.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>        // lane WATER7: the left editor's page stack
#include <QTabBar>               // lane WATER7: its segmented tab strip
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

/* =========================================================================
 *  The Water Marking dock
 *
 *  bungo, 2026-09-09: *"in nifskope, have the player mark the water direction
 *  in a smart way"* and *"different water colors for different bodies of
 *  water"* -- *"or at least an ID for them"*.
 *
 *  BLENDER IS THE REFERENCE (CONSTITUTION rule 10), and the divergences are
 *  stated rather than discovered:
 *
 *   - Blender's grease pencil draws in the 3D viewport; THIS DRAWS ON A
 *     TOP-DOWN MAP inside the dock. Two reasons, and the first is the honest
 *     one: the 3D viewport lives in `src/glview.cpp`, which another lane held
 *     open while this was written, and the brief's file rule forbade touching
 *     it. The second is that it is the better canvas anyway -- a river reach is
 *     eleven cells long, its direction is a fact about the MAP, and orbiting a
 *     terrain mesh to draw a line down it is the gesture Blender itself
 *     replaces with a 2D editor (the UV and Image editors) whenever the thing
 *     being edited is flat. The 3D viewer still SHOWS the result: it reads the
 *     same planes back out of the file.
 *   - Blender's stroke thickness comes from tablet pressure; here the Width row
 *     is the only source, so a stroke is reproducible from the file alone.
 *   - Blender's eraser has a radius; here Erase removes the whole stroke under
 *     the cursor, because a stroke is one constraint and half a constraint is
 *     not a smaller constraint.
 *   - Pan and zoom are Blender's: middle-drag pans, the wheel zooms about the
 *     cursor, and the wheel over a NUMBER FIELD still does nothing unless the
 *     field has focus (wwGuardWheel), which is the same rule.
 * ========================================================================= */

namespace {

//! The categorical colour of a body id -- the picture that answers "or at least an ID".
QColor bodyColour( quint16 id )
{
	if ( !id )
		return QColor( 24, 26, 30 );
	/* A hash, not a palette: ids run to thousands and neighbouring ids must not
	 * be neighbouring colours, or two touching bodies read as one. */
	quint32 h = quint32( id ) * 2654435761u;
	const double hue = double( h % 3600u ) / 3600.0 * 360.0;
	const double sat = 0.45 + double( ( h >> 12 ) % 40u ) / 100.0;
	const double val = 0.55 + double( ( h >> 20 ) % 35u ) / 100.0;
	return QColor::fromHsvF( hue / 360.0, qBound( 0.0, sat, 1.0 ), qBound( 0.0, val, 1.0 ) );
}

//! A flow word as a colour: hue is the direction, value is the confidence.
QColor flowColour( quint16 word )
{
	if ( !word )
		return QColor( 24, 26, 30 );
	const double dir = double( word & 0xFF ) / 256.0;
	const double conf = double( ( word >> 12 ) & 0xF ) / 15.0;
	return QColor::fromHsvF( dir, 0.75, 0.35 + 0.6 * conf );
}

/*! A folding section with an arrow and a heading, the LodgenSection shape
 *  without the check box: the bake is not optional, so its header is not a
 *  setting. The fold persists, which is the user's and not ours. */
class MarkSection final : public QWidget
{
public:
	MarkSection( const QString & title, const QString & key, QWidget * parent )
		: QWidget( parent ), settingsKey( QStringLiteral( "WaterMark/expanded/" ) + key )
	{
		setObjectName( QStringLiteral( "WaterMark" ) + key + QStringLiteral( "Section" ) );
		auto * v = new QVBoxLayout( this );
		v->setContentsMargins( 0, 0, 0, 0 );
		v->setSpacing( 4 );
		auto * header = new QHBoxLayout();
		header->setContentsMargins( 0, 0, 0, 0 );
		header->setSpacing( 2 );
		arrow = new QToolButton( this );
		arrow->setObjectName( QStringLiteral( "WaterMark" ) + key + QStringLiteral( "Expander" ) );
		arrow->setAutoRaise( true );
		arrow->setFixedSize( 16, 16 );
		arrow->setToolTip( tr( "Show or hide these settings" ) );
		header->addWidget( arrow, 0 );
		header->addWidget( wwHeading( title, this ), 1 );
		v->addLayout( header );
		bodyWidget = new QWidget( this );
		bodyWidget->setObjectName( QStringLiteral( "WaterMark" ) + key + QStringLiteral( "Body" ) );
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
	bool open = true;
};

class WaterMarkPanel;

/*! The canvas: the worldspace from above, one plane at a time, and the surface
 *  the strokes are drawn on.
 *
 *  It never holds the plane. An OVERVIEW image is sampled once per open at
 *  about a thousand pixels a side (the Commonwealth's body plane is 6144 across
 *  and 75 MB), and a zoomed view re-samples only the texels under the widget.
 *  That is the same discipline the reader itself uses. */
class WaterMarkCanvas final : public QWidget
{
public:
	explicit WaterMarkCanvas( WaterMarkPanel * owner, QWidget * parent )
		: QWidget( parent ), panel( owner )
	{
		setObjectName( QStringLiteral( "WaterMarkCanvas" ) );
		setMinimumHeight( 220 );
		setMouseTracking( true );
		setToolTip( tr( "The worldspace from above, north up. Drag to draw a stroke down the "
			"water; click a body to select it; middle-drag to pan and the wheel to zoom." ) );
	}

	void setDoc( WaterMarkDoc * d );
	void rebuildOverview();
	void setPlane( int p ) { plane = p; rebuildOverview(); update(); }
	int selectedBody() const { return selected; }
	void setSelectedBody( int id ) { selected = id; update(); }
	//! The harness's hands: lay a stroke between two world points, as a drag would.
	QString layStroke( double x0, double y0, double x1, double y1 );
	QString lastMessage() const { return message; }

protected:
	void paintEvent( QPaintEvent * ) override;
	void mousePressEvent( QMouseEvent * e ) override;
	void mouseMoveEvent( QMouseEvent * e ) override;
	void mouseReleaseEvent( QMouseEvent * e ) override;
	void wheelEvent( QWheelEvent * e ) override;
	void resizeEvent( QResizeEvent * ) override { update(); }

private:
	QPointF worldToView( double wx, double wy ) const;
	void viewToWorld( const QPointF & p, double & wx, double & wy ) const;
	void fit();

	WaterMarkPanel * panel = nullptr;
	WaterMarkDoc * doc = nullptr;
	QImage overview;
	int plane = 0;                 //!< 0 body id, 1 flow, 2 shore
	int selected = 0;
	double cx = 0.0, cy = 0.0;     //!< the world point at the widget's centre
	double scale = 0.0;            //!< widget pixels per world unit
	bool drawing = false, panning = false;
	QPoint panFrom;
	QVector<QPointF> current;      //!< the stroke being drawn, in world units
	QString message;
};

/*! The dock's own panel. House style throughout -- wwHeading for sections,
 *  wwMakeScrubField for every number, wwMatchFieldStyle for every selector, one
 *  setting a row in a label | field grid with one label width for the page, the
 *  explanation in the tooltip, and the summary and the buttons pinned outside
 *  the scroll area. `WW_WATER_MARK_TEST` counts each of those with a floor. */
class WaterMarkPanel final : public QWidget
{
public:
	explicit WaterMarkPanel( QWidget * parent = nullptr )
		: QWidget( parent )
	{
		setObjectName( QStringLiteral( "WaterMarkPanel" ) );
		auto * outer = new QVBoxLayout( this );
		outer->setContentsMargins( 0, 0, 0, 0 );
		outer->setSpacing( 0 );
		splitter = new QSplitter( Qt::Vertical, this );
		splitter->setObjectName( QStringLiteral( "WaterMarkSplitter" ) );
		splitter->setChildrenCollapsible( false );
		outer->addWidget( splitter, 1 );

		scroll = new QScrollArea( splitter );
		scroll->setObjectName( QStringLiteral( "WaterMarkSettingsScroll" ) );
		scroll->setWidgetResizable( true );
		scroll->setFrameShape( QFrame::NoFrame );
		auto * page = new QWidget( scroll );
		page->setObjectName( QStringLiteral( "WaterMarkSettingsPage" ) );
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

		// ---- Landscape file --------------------------------------------
		layout->addWidget( wwHeading( tr( "Landscape file" ), page ) );
		Form src = form( 0 );
		fileEdit = new QLineEdit( page );
		fileEdit->setObjectName( QStringLiteral( "WaterMarkFileEdit" ) );
		fileEdit->setPlaceholderText( tr( "a version 3 .lodl" ) );
		fileEdit->setToolTip( tr( "The whole-worldspace landscape file whose water is being\n"
			"marked. It must carry the version 3 water sections, which the\n"
			"generator writes with Water bodies ticked." ) );
		wwMatchFieldStyle( fileEdit );
		{
			auto * host = new QWidget( page );
			auto * h = new QHBoxLayout( host );
			h->setContentsMargins( 0, 0, 0, 0 );
			h->setSpacing( 4 );
			h->addWidget( fileEdit, 1 );
			auto * browse = new QPushButton( tr( "Browse" ), host );
			browse->setObjectName( QStringLiteral( "WaterMarkBrowseButton" ) );
			browse->setToolTip( tr( "Choose the landscape file to mark" ) );
			connect( browse, &QPushButton::clicked, this, [this]() {
				const QString f = QFileDialog::getOpenFileName( this,
					tr( "Landscape file" ), fileEdit->text(),
					tr( "Landscape (*.lodl)" ) );
				if ( !f.isEmpty() ) {
					fileEdit->setText( f );
					openFile( f );
				}
			} );
			h->addWidget( browse, 0 );
			src.add( page, tr( "File" ), host );
		}
		showBox = new QComboBox( page );
		showBox->setObjectName( QStringLiteral( "WaterMarkShowBox" ) );
		showBox->addItem( tr( "Body ID" ), 0 );
		showBox->addItem( tr( "Flow" ), 1 );
		showBox->addItem( tr( "Shore distance" ), 2 );
		showBox->addItem( tr( "Dye" ), 3 );
		showBox->setToolTip( tr( "Which stored plane the map paints. Body ID gives every body\n"
			"of water its own colour, which is what the file added." ) );
		wwMatchFieldStyle( showBox );
		src.add( page, tr( "Show" ), showBox );
		layout->addLayout( src.g );

		// ---- Marking ----------------------------------------------------
		layout->addWidget( wwHeading( tr( "Marking" ), page ) );
		Form mk = form( 0 );
		toolBox = new QComboBox( page );
		toolBox->setObjectName( QStringLiteral( "WaterMarkToolBox" ) );
		toolBox->addItem( tr( "Stroke" ), int( WaterStroke::Stroke ) );
		toolBox->addItem( tr( "Pin" ), int( WaterStroke::Pin ) );
		toolBox->addItem( tr( "Source pin" ), int( WaterStroke::SourcePin ) );
		toolBox->addItem( tr( "Outlet pin" ), int( WaterStroke::OutletPin ) );
		toolBox->addItem( tr( "Dye pin" ), int( WaterStroke::DyePin ) );
		toolBox->addItem( tr( "Erase" ), -1 );
		toolBox->setToolTip( tr( "Stroke: drag along the water and its tangent is the direction.\n"
			"Pin: drag out one arrow at one place.\n"
			"Source and Outlet pins: click the head and the mouth, and the pair\n"
			"is the path between them.\n"
			"Dye pin: click where something enters the water; its colour is carried\n"
			"downstream and fades over the Dye fade distance.\n"
			"Erase: click a stroke to remove it." ) );
		wwMatchFieldStyle( toolBox );
		mk.add( page, tr( "Tool" ), toolBox );

		speedSpin = new QDoubleSpinBox( page );
		speedSpin->setObjectName( QStringLiteral( "WaterMarkSpeedSpin" ) );
		speedSpin->setRange( 0.0, 100.0 );
		speedSpin->setDecimals( 3 );
		speedSpin->setSingleStep( 0.05 );
		speedSpin->setValue( 0.25 );
		speedSpin->setToolTip( tr( "How fast the water moves, in world units a second, the same\n"
			"unit the game's own water form uses for its linear velocity." ) );
		wwMakeScrubField( speedSpin );
		mk.add( page, tr( "Speed" ), speedSpin );

		widthSpin = new QDoubleSpinBox( page );
		widthSpin->setObjectName( QStringLiteral( "WaterMarkWidthSpin" ) );
		widthSpin->setRange( 128.0, 65536.0 );
		widthSpin->setDecimals( 0 );
		widthSpin->setSingleStep( 128.0 );
		widthSpin->setValue( 4096.0 );
		widthSpin->setToolTip( tr( "How far from the stroke the direction is held exactly, in\n"
			"world units. One cell is 4096. Beyond it the field is smoothed into\n"
			"the rest of the body and the confidence falls away." ) );
		wwMakeScrubField( widthSpin );
		mk.add( page, tr( "Width" ), widthSpin );

		dyeColourButton = new QPushButton( tr( "Choose" ), page );
		dyeColourButton->setObjectName( QStringLiteral( "WaterMarkDyeColourButton" ) );
		dyeColourButton->setToolTip( tr( "The colour a Dye pin releases into the water." ) );
		mk.add( page, tr( "Dye colour" ), dyeColourButton );

		dyeFadeSpin = new QDoubleSpinBox( page );
		dyeFadeSpin->setObjectName( QStringLiteral( "WaterMarkDyeFadeSpin" ) );
		dyeFadeSpin->setRange( 256.0, 262144.0 );
		dyeFadeSpin->setDecimals( 0 );
		dyeFadeSpin->setSingleStep( 512.0 );
		dyeFadeSpin->setValue( WaterMarkDoc::kDyeHalfDistanceDefault );
		dyeFadeSpin->setToolTip( tr( "How far along the flow a dye travels before half of it is gone,\n"
			"in world units; one cell is 4096. The same distance serves every dye\n"
			"in the file, and it is stored in the file." ) );
		wwMakeScrubField( dyeFadeSpin );
		mk.add( page, tr( "Dye fade" ), dyeFadeSpin );
		layout->addLayout( mk.g );

		// ---- Selected body ----------------------------------------------
		layout->addWidget( wwHeading( tr( "Selected body" ), page ) );
		Form sel = form( 0 );
		classBox = new QComboBox( page );
		classBox->setObjectName( QStringLiteral( "WaterMarkClassBox" ) );
		classBox->addItem( tr( "Automatic" ), -1 );
		classBox->addItem( tr( "Sea" ), 0 );
		classBox->addItem( tr( "River" ), 1 );
		classBox->addItem( tr( "Lake" ), 2 );
		classBox->setToolTip( tr( "What this body is. Automatic keeps the answer the generator\n"
			"measured from its shape and its neighbours." ) );
		wwMatchFieldStyle( classBox );
		sel.add( page, tr( "Class" ), classBox );

		formBox = new QComboBox( page );
		formBox->setObjectName( QStringLiteral( "WaterMarkFormBox" ) );
		formBox->setToolTip( tr( "The water form this body uses, from the ones the plugin\n"
			"already defines. It carries the colours, the fog and the noise." ) );
		wwMatchFieldStyle( formBox );
		sel.add( page, tr( "Water form" ), formBox );

		{
			auto * host = new QWidget( page );
			auto * h = new QHBoxLayout( host );
			h->setContentsMargins( 0, 0, 0, 0 );
			h->setSpacing( 4 );
			colourCheck = new QCheckBox( tr( "Override" ), host );
			colourCheck->setObjectName( QStringLiteral( "WaterMarkColourCheck" ) );
			colourCheck->setToolTip( tr( "Give this one body its own colour instead of the one its\n"
				"water form gives every body that shares it." ) );
			colourButton = new QPushButton( tr( "Choose" ), host );
			colourButton->setObjectName( QStringLiteral( "WaterMarkColourButton" ) );
			colourButton->setToolTip( tr( "Pick this body's colour" ) );
			h->addWidget( colourCheck, 0 );
			h->addWidget( colourButton, 1 );
			sel.add( page, tr( "Colour" ), host );
		}

		stillCheck = new QCheckBox( tr( "Still water" ), page );
		stillCheck->setObjectName( QStringLiteral( "WaterMarkStillCheck" ) );
		stillCheck->setToolTip( tr( "This body has no flow at all. A lake that no river reaches\n"
			"does not move, and this beats the velocity its water form carries." ) );
		sel.add( page, tr( "Flow" ), stillCheck );

		dyeMouthCheck = new QCheckBox( tr( "Dye at mouth" ), page );
		dyeMouthCheck->setObjectName( QStringLiteral( "WaterMarkDyeMouthCheck" ) );
		dyeMouthCheck->setToolTip( tr( "This body's water keeps its own colour past its mouth, as a\n"
			"plume into the body it drains into that fades over the Dye fade\n"
			"distance. A river into the sea, or a creek into a lake." ) );
		sel.add( page, tr( "Dye" ), dyeMouthCheck );

		nameEdit = new QLineEdit( page );
		nameEdit->setObjectName( QStringLiteral( "WaterMarkNameEdit" ) );
		nameEdit->setToolTip( tr( "A name for this body, stored in the file. Nothing reads it\n"
			"but a person." ) );
		wwMatchFieldStyle( nameEdit );
		sel.add( page, tr( "Name" ), nameEdit );
		layout->addLayout( sel.g );

		// ---- Bake (folds) -------------------------------------------------
		bakeSection = new MarkSection( tr( "Bake" ), QStringLiteral( "Bake" ), page );
		{
			Form bk = form( 18 );
			bk.g->setParent( nullptr );
			auto * bodyW = bakeSection->body();
			auto * bv = new QVBoxLayout( bodyW );
			bv->setContentsMargins( 0, 0, 0, 0 );
			bv->setSpacing( 4 );
			flowRateBox = new QComboBox( bodyW );
			flowRateBox->setObjectName( QStringLiteral( "WaterMarkFlowRateBox" ) );
			flowRateBox->addItem( tr( "8" ), 8 );
			flowRateBox->addItem( tr( "16" ), 16 );
			flowRateBox->addItem( tr( "32" ), 32 );
			flowRateBox->setCurrentIndex( 2 );
			flowRateBox->setToolTip( tr( "How many flow samples a cell the file stores. The strokes\n"
				"are world coordinates, so this can change without losing them." ) );
			wwMatchFieldStyle( flowRateBox );
			Form f2 = form( 18 );
			f2.add( bodyW, tr( "Flow samples per cell" ), flowRateBox );
			bv->addLayout( f2.g );
		}
		layout->addWidget( bakeSection );
		layout->addStretch( 1 );

		// ---- the canvas, on the splitter --------------------------------
		canvas = new WaterMarkCanvas( this, splitter );
		splitter->addWidget( canvas );
		/* bungo, 2026-09-10: "do you draw it on that tiny map?" -- the tiny map
		 * goes away.  The canvas stays as the model's hands for the harness (its
		 * layStroke) and is never shown; the map lives in the water window. */
		canvas->hide();
		splitter->setStretchFactor( 0, 0 );
		splitter->setStretchFactor( 1 , 1 );
		/* AND A STARTING SIZE FOR EACH BAND.  A QScrollArea reports a fixed
		 * ~100x30 sizeHint whatever it holds, so without this the splitter
		 * had nothing to open the settings on and gave the map everything:
		 * the dock opened with two of its eleven rows above the fold, and
		 * every self-test count stayed green because they measure the layout
		 * and not what is on screen.  The canvas takes the rest, because it
		 * is the stretching half. */
		splitter->setSizes( { page->sizeHint().height() + 12,
			canvas->minimumHeight() } );

		// ---- the summary and the action bar, pinned ---------------------
		summary = new QLabel( this );
		summary->setObjectName( QStringLiteral( "WaterMarkSummary" ) );
		summary->setWordWrap( true );
		summary->setContentsMargins( 8, 4, 8, 2 );
		outer->addWidget( summary, 0 );

		auto * bar = new QWidget( this );
		bar->setObjectName( QStringLiteral( "WaterMarkActionBar" ) );
		auto * bh = new QHBoxLayout( bar );
		bh->setContentsMargins( 8, 2, 8, 8 );
		bh->setSpacing( 6 );
		bh->addStretch( 1 );
		auto * windowButton = new QPushButton( tr( "Water window" ), bar );
		windowButton->setObjectName( QStringLiteral( "WaterMarkWindowButton" ) );
		windowButton->setToolTip( tr( "Open the water window: the whole flow map, the curves and every\n"
			"marking tool, draggable, resizable and full-screen on F11" ) );
		connect( windowButton, &QPushButton::clicked, this, [this]() {
			QMainWindow * mw = nullptr;
			for ( QWidget * w = parentWidget(); w; w = w->parentWidget() )
				if ( ( mw = qobject_cast<QMainWindow *>( w ) ) )
					break;
			waterWindowOpen( mw, doc ? doc->path() : fileEdit->text() );
		} );
		bh->addWidget( windowButton, 0 );
		reloadButton = new QPushButton( tr( "Reload" ), bar );
		reloadButton->setObjectName( QStringLiteral( "WaterMarkReloadButton" ) );
		reloadButton->setToolTip( tr( "Read the file again and re-derive the planes from the\n"
			"strokes it carries, throwing away anything unsaved." ) );
		solveButton = new QPushButton( tr( "Solve" ), bar );
		solveButton->setObjectName( QStringLiteral( "WaterMarkSolveButton" ) );
		solveButton->setToolTip( tr( "Spread the strokes through the bodies they were drawn on" ) );
		saveButton = new QPushButton( tr( "Save" ), bar );
		saveButton->setObjectName( QStringLiteral( "WaterMarkSaveButton" ) );
		saveButton->setToolTip( tr( "Write the strokes and the planes they derive into the file" ) );
		bh->addWidget( reloadButton, 0 );
		bh->addWidget( solveButton, 0 );
		bh->addWidget( saveButton, 0 );
		outer->addWidget( bar, 0 );

		connect( showBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int i ) { canvas->setPlane( showBox->itemData( i ).toInt() ); } );
		connect( classBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int i ) {
				if ( doc && selected && !loadingBody )
					doc->setBodyClass( selected, classBox->itemData( i ).toInt() );
				refreshSummary();
			} );
		connect( formBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int i ) {
				if ( doc && selected && !loadingBody )
					doc->setBodyForm( selected, quint32( formBox->itemData( i ).toUInt() ) );
				refreshSummary();
			} );
		connect( stillCheck, &QCheckBox::toggled, this, [this]( bool on ) {
			if ( doc && selected && !loadingBody ) {
				doc->setBodyLockZero( selected, on );
				solve();
			}
			refreshSummary();
		} );
		connect( dyeMouthCheck, &QCheckBox::toggled, this, [this]( bool on ) {
			if ( doc && selected && !loadingBody ) {
				doc->setBodyDyeMouth( selected, on, 1.0f );
				solve();
			}
			refreshSummary();
		} );
		connect( dyeColourButton, &QPushButton::clicked, this, [this]() {
			const QColor c = QColorDialog::getColor( dyePick, this, tr( "Dye colour" ) );
			if ( c.isValid() )
				dyePick = c;
		} );
		connect( dyeFadeSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
			if ( doc && !loadingBody ) {
				doc->setDyeHalfDistance( dyeFadeSpin->value() );
				solve();
			}
		} );
		connect( colourCheck, &QCheckBox::toggled, this, [this]( bool on ) {
			if ( doc && selected && !loadingBody )
				doc->setBodyColour( selected, quint8( pick.red() ), quint8( pick.green() ),
					quint8( pick.blue() ), on );
			refreshSummary();
		} );
		connect( colourButton, &QPushButton::clicked, this, [this]() {
			const QColor c = QColorDialog::getColor( pick, this, tr( "Body colour" ) );
			if ( !c.isValid() )
				return;
			pick = c;
			colourCheck->setChecked( true );
			if ( doc && selected )
				doc->setBodyColour( selected, quint8( c.red() ), quint8( c.green() ),
					quint8( c.blue() ), true );
			refreshSummary();
		} );
		connect( nameEdit, &QLineEdit::editingFinished, this, [this]() {
			if ( doc && selected && !loadingBody )
				doc->setBodyName( selected, nameEdit->text() );
			refreshSummary();
		} );
		connect( flowRateBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this,
			[this]( int i ) {
				QString err;
				if ( doc && !doc->setFlowRate( flowRateBox->itemData( i ).toInt(), &err ) )
					refusal = err;
				refreshSummary();
			} );
		connect( solveButton, &QPushButton::clicked, this, [this]() { solve(); } );
		connect( saveButton, &QPushButton::clicked, this, [this]() { saveNow(); } );
		connect( reloadButton, &QPushButton::clicked, this, [this]() {
			if ( doc )
				openFile( doc->path() );
		} );
		connect( fileEdit, &QLineEdit::editingFinished, this, [this]() {
			if ( !fileEdit->text().isEmpty() && ( !doc || doc->path() != fileEdit->text() ) )
				openFile( fileEdit->text() );
		} );
		refreshSummary();
	}

	~WaterMarkPanel() override { delete doc; }

	WaterMarkDoc * document() const { return doc; }
	WaterMarkCanvas * view() const { return canvas; }
	QLabel * summaryLabel() const { return summary; }
	QScrollArea * settingsScroll() const { return scroll; }
	QPushButton * saveBtn() const { return saveButton; }
	MarkSection * bake() const { return bakeSection; }
	int currentTool() const { return toolBox->currentData().toInt(); }
	double strokeSpeed() const { return speedSpin->value(); }
	double strokeWidth() const { return widthSpin->value(); }
	QColor dyeColour() const { return dyePick; }

	bool openFile( const QString & path )
	{
		delete doc;
		doc = new WaterMarkDoc();
		QString err;
		if ( !doc->open( path, &err ) ) {
			refusal = err;
			delete doc;
			doc = nullptr;
			canvas->setDoc( nullptr );
			refreshSummary();
			return false;
		}
		refusal.clear();
		fileEdit->setText( path );
		formBox->clear();
		for ( quint32 f : doc->waterForms() )
			formBox->addItem( QStringLiteral( "%1" ).arg( f, 8, 16, QLatin1Char( '0' ) ), f );
		canvas->setDoc( doc );
		selected = 0;
		loadingBody = true;
		dyeFadeSpin->setValue( doc->dyeHalfDistance() );
		loadingBody = false;
		solve();
		selectBody( 0 );
		return true;
	}

	void selectBody( int id )
	{
		selected = id;
		canvas->setSelectedBody( id );
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
			const double mag = std::sqrt( double( b.flowX ) * b.flowX + double( b.flowY ) * b.flowY );
			if ( mag > 0.0 )
				speedSpin->setValue( mag );
		} else {
			nameEdit->clear();
		}
		loadingBody = false;
		refreshSummary();
	}

	void solve()
	{
		if ( !doc )
			return;
		QString err;
		WaterMarkSolve st;
		if ( !doc->solve( &st, &err ) )
			refusal = err;
		else
			lastSolve = st.note;
		canvas->rebuildOverview();
		canvas->update();
		refreshSummary();
	}

	void saveNow()
	{
		if ( !doc )
			return;
		QString err;
		if ( !doc->save( &err ) ) {
			refusal = err;
		} else {
			refusal.clear();
			lastSolve = tr( "saved %1" ).arg( QFileInfo( doc->path() ).fileName() );
			canvas->rebuildOverview();
			canvas->update();
		}
		refreshSummary();
	}

	/*! THE SENTENCE. It says what Save will do, or the ONE reason it cannot,
	 *  and it owns the button's enabled state -- a greyed button with no
	 *  sentence beside it is a broken button. */
	void refreshSummary()
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
			int strokes = 0;
			for ( const WaterStroke & s : doc->strokes() )
				if ( s.enabled() )
					strokes++;
			text = tr( "Write %1 stroke(s) into %2 (%3 bodies, flow at %4 samples a cell)" )
				.arg( strokes ).arg( QFileInfo( doc->path() ).fileName() )
				.arg( doc->bodyCount() ).arg( doc->flowSamples() );
			if ( selected )
				text += QStringLiteral( "\n" ) + doc->describeBody( selected );
			if ( !lastSolve.isEmpty() )
				text += QStringLiteral( "\n" ) + lastSolve;
		}
		summary->setText( text );
		summary->setStyleSheet( QStringLiteral( "color: %1;" )
			.arg( refusal.isEmpty() ? muted : danger ) );
		saveButton->setEnabled( can );
		solveButton->setEnabled( can );
		reloadButton->setEnabled( doc != nullptr );
		classBox->setEnabled( doc && selected );
		formBox->setEnabled( doc && selected );
		colourCheck->setEnabled( doc && selected );
		colourButton->setEnabled( doc && selected );
		stillCheck->setEnabled( doc && selected );
		dyeMouthCheck->setEnabled( doc && selected );
		dyeFadeSpin->setEnabled( doc != nullptr );
		nameEdit->setEnabled( doc && selected );
	}

	QString refusalText() const { return refusal; }
	void setRefusal( const QString & r ) { refusal = r; refreshSummary(); }

private:
	WaterMarkDoc * doc = nullptr;
	WaterMarkCanvas * canvas = nullptr;
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
	QCheckBox * colourCheck = nullptr;
	QCheckBox * stillCheck = nullptr;
	QCheckBox * dyeMouthCheck = nullptr;
	QPushButton * dyeColourButton = nullptr;
	QDoubleSpinBox * dyeFadeSpin = nullptr;
	QColor dyePick = QColor( 110, 170, 40 );   //!< a sludge green, until chosen
	QPushButton * colourButton = nullptr;
	QPushButton * saveButton = nullptr;
	QPushButton * solveButton = nullptr;
	QPushButton * reloadButton = nullptr;
	QLabel * summary = nullptr;
	MarkSection * bakeSection = nullptr;
	QColor pick = QColor( 60, 130, 200 );
	QString refusal, lastSolve;
	int selected = 0;
	bool loadingBody = false;
};

// ---- the canvas ----------------------------------------------------------

void WaterMarkCanvas::setDoc( WaterMarkDoc * d )
{
	doc = d;
	scale = 0.0;
	rebuildOverview();
	update();
}

void WaterMarkCanvas::rebuildOverview()
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
	const int target = 768;
	const int step = qMax( 1, qMax( tw, th ) / target );
	const int iw = tw / step, ih = th / step;
	QImage img( iw, ih, QImage::Format_RGB32 );
	for ( int j = 0; j < ih; j++ ) {
		// row 0 of the plane is SOUTH; the image's row 0 is NORTH, so it mirrors
		QRgb * line = reinterpret_cast<QRgb *>( img.scanLine( ih - 1 - j ) );
		for ( int i = 0; i < iw; i++ ) {
			double wx = 0, wy = 0;
			wx = x0 + ( double( i ) * step + 0.5 ) * u;
			wy = y0 + ( double( j ) * step + 0.5 ) * u;
			const quint16 id = doc->bodyAtWorld( wx, wy );
			QColor c;
			if ( plane == 1 ) {
				int px = 0, py = 0;
				doc->worldToTexel( wx, wy, px, py );
				c = flowColour( doc->flowWordAt( px, py ) );
			} else if ( plane == 3 ) {
				/* the dye plane: the source's colour -- a body's hash, or the pin's
				 * own -- blended by its weight over the body's dark base */
				int px = 0, py = 0;
				doc->worldToTexel( wx, wy, px, py );
				const quint32 d = id ? doc->dyeWordAt( px, py ) : 0;
				QColor base = id ? QColor( 34, 48, 70 ) : QColor( 24, 26, 30 );
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
				c = base;
			} else if ( plane == 2 ) {
				int px = 0, py = 0;
				doc->worldToTexel( wx, wy, px, py );
				const int s = id ? int( doc->file()->shoreAt( px, py ) ) : 255;
				c = id ? QColor( qBound( 0, s * 3, 255 ), qBound( 0, s * 3, 255 ), 255 )
					: QColor( 24, 26, 30 );
			} else {
				c = bodyColour( id );
			}
			line[i] = c.rgb();
		}
	}
	overview = img;
}

QPointF WaterMarkCanvas::worldToView( double wx, double wy ) const
{
	return QPointF( double( width() ) * 0.5 + ( wx - cx ) * scale,
		double( height() ) * 0.5 - ( wy - cy ) * scale );
}

void WaterMarkCanvas::viewToWorld( const QPointF & p, double & wx, double & wy ) const
{
	wx = cx + ( p.x() - double( width() ) * 0.5 ) / scale;
	wy = cy - ( p.y() - double( height() ) * 0.5 ) / scale;
}

void WaterMarkCanvas::fit()
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
}

void WaterMarkCanvas::paintEvent( QPaintEvent * )
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
	p.drawImage( QRectF( tl, br ), overview );
	p.setPen( QColor( wwSkinColor( "borderDim" ) ) );
	p.drawRect( QRectF( tl, br ) );

	// the strokes, as they were drawn, with an arrow at the far end
	auto drawPoly = [&]( const QVector<QPointF> & pts, const QColor & col, bool arrow ) {
		if ( pts.size() < 1 )
			return;
		p.setPen( QPen( col, 2.0 ) );
		QPolygonF poly;
		for ( const QPointF & w : pts )
			poly << worldToView( w.x(), w.y() );
		if ( poly.size() >= 2 )
			p.drawPolyline( poly );
		else
			p.drawEllipse( poly.first(), 3.0, 3.0 );
		if ( arrow && poly.size() >= 2 ) {
			const QPointF a = poly[poly.size() - 2], b = poly.last();
			const double ang = std::atan2( b.y() - a.y(), b.x() - a.x() );
			const double L = 9.0;
			QPolygonF head;
			head << b
				<< QPointF( b.x() - L * std::cos( ang - 0.4 ), b.y() - L * std::sin( ang - 0.4 ) )
				<< QPointF( b.x() - L * std::cos( ang + 0.4 ), b.y() - L * std::sin( ang + 0.4 ) );
			p.setBrush( col );
			p.drawPolygon( head );
			p.setBrush( Qt::NoBrush );
		}
	};
	for ( const WaterStroke & s : doc->strokes() ) {
		QVector<QPointF> pts;
		for ( const WaterStrokePoint & q : s.pts )
			pts << QPointF( double( q.x ), double( q.y ) );
		QColor col = QColor( wwSkinColor( "accent" ) );
		if ( s.kind == WaterStroke::SourcePin )
			col = QColor( 120, 220, 140 );
		else if ( s.kind == WaterStroke::OutletPin )
			col = QColor( 240, 160, 90 );
		else if ( s.kind == WaterStroke::ZeroFlow || s.kind == WaterStroke::DyeMouth )
			col = QColor( wwSkinColor( "textMuted" ) );
		else if ( s.kind == WaterStroke::DyePin )
			col = QColor( s.colour[0], s.colour[1], s.colour[2] );
		else if ( s.kind == WaterStroke::DyeKnob )
			continue;
		if ( int( s.body ) == selected )
			col = col.lighter( 130 );
		drawPoly( pts, col, s.kind != WaterStroke::ZeroFlow && s.kind != WaterStroke::DyeMouth );
	}
	if ( drawing && current.size() >= 1 )
		drawPoly( current, QColor( wwSkinColor( "focus" ) ), true );
	if ( !message.isEmpty() ) {
		p.setPen( QColor( wwSkinColor( "text" ) ) );
		p.drawText( rect().adjusted( 6, 6, -6, -6 ), Qt::AlignTop | Qt::AlignLeft, message );
	}
}

void WaterMarkCanvas::mousePressEvent( QMouseEvent * e )
{
	if ( !doc || !doc->isOpen() )
		return;
	if ( scale <= 0.0 )
		fit();
	if ( e->button() == Qt::MiddleButton ) {
		panning = true;
		panFrom = e->pos();
		return;
	}
	if ( e->button() != Qt::LeftButton )
		return;
	double wx = 0, wy = 0;
	viewToWorld( e->position(), wx, wy );
	const quint16 id = doc->bodyAtWorld( wx, wy );
	if ( id )
		panel->selectBody( int( id ) );
	const int tool = panel->currentTool();
	if ( tool < 0 ) {
		// Erase: the whole stroke under the cursor, not part of it
		int best = -1;
		double bestD = 1e30;
		const QVector<WaterStroke> & ss = doc->strokes();
		for ( int i = 0; i < ss.size(); i++ )
			for ( const WaterStrokePoint & q : ss[i].pts ) {
				const double d = std::hypot( double( q.x ) - wx, double( q.y ) - wy );
				if ( d < bestD ) {
					bestD = d;
					best = i;
				}
			}
		if ( best >= 0 && bestD < 8.0 * doc->worldPerTexel() ) {
			doc->removeStroke( best );
			message = tr( "one stroke removed" );
			panel->solve();
		} else {
			message = tr( "no stroke here" );
		}
		update();
		return;
	}
	drawing = true;
	current.clear();
	current << QPointF( wx, wy );
	update();
}

void WaterMarkCanvas::mouseMoveEvent( QMouseEvent * e )
{
	if ( panning ) {
		const QPoint d = e->pos() - panFrom;
		panFrom = e->pos();
		cx -= double( d.x() ) / scale;
		cy += double( d.y() ) / scale;
		update();
		return;
	}
	if ( !drawing || !doc )
		return;
	double wx = 0, wy = 0;
	viewToWorld( e->position(), wx, wy );
	if ( current.isEmpty()
		|| std::hypot( wx - current.last().x(), wy - current.last().y() ) > doc->worldPerTexel() ) {
		current << QPointF( wx, wy );
		update();
	}
}

void WaterMarkCanvas::mouseReleaseEvent( QMouseEvent * e )
{
	if ( e->button() == Qt::MiddleButton ) {
		panning = false;
		return;
	}
	if ( !drawing || !doc )
		return;
	drawing = false;
	const int tool = panel->currentTool();
	WaterStroke s;
	s.kind = quint8( tool );
	s.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
	s.speed = float( panel->strokeSpeed() );
	s.width = float( panel->strokeWidth() );
	if ( tool == WaterStroke::SourcePin || tool == WaterStroke::OutletPin
		|| tool == WaterStroke::DyePin ) {
		WaterStrokePoint p;
		p.x = float( current.first().x() );
		p.y = float( current.first().y() );
		s.pts << p;
		if ( tool == WaterStroke::DyePin ) {
			// strength 1 at the pin; the Width row is its radius
			s.speed = 1.0f;
			const QColor c = panel->dyeColour();
			s.colour[0] = quint8( c.red() );
			s.colour[1] = quint8( c.green() );
			s.colour[2] = quint8( c.blue() );
			s.colour[3] = 255;
		}
	} else {
		for ( const QPointF & q : current ) {
			WaterStrokePoint p;
			p.x = float( q.x() );
			p.y = float( q.y() );
			s.pts << p;
		}
	}
	current.clear();
	QString msg;
	doc->addStroke( s, &msg );
	message = msg;
	panel->solve();
	update();
}

void WaterMarkCanvas::wheelEvent( QWheelEvent * e )
{
	if ( !doc || !doc->isOpen() )
		return;
	if ( scale <= 0.0 )
		fit();
	double wx = 0, wy = 0;
	viewToWorld( e->position(), wx, wy );
	const double f = e->angleDelta().y() > 0 ? 1.25 : 0.8;
	scale *= f;
	// keep the point under the cursor where it was: Blender's zoom
	double nx = 0, ny = 0;
	viewToWorld( e->position(), nx, ny );
	cx += wx - nx;
	cy += wy - ny;
	update();
	e->accept();
}

QString WaterMarkCanvas::layStroke( double x0, double y0, double x1, double y1 )
{
	if ( !doc )
		return QStringLiteral( "no landscape file" );
	WaterStroke s;
	s.kind = quint8( panel->currentTool() < 0 ? int( WaterStroke::Stroke ) : panel->currentTool() );
	s.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;
	s.speed = float( panel->strokeSpeed() );
	s.width = float( panel->strokeWidth() );
	const int steps = 64;
	/* AS DRAWN.  This used to drop every point that was not on water, so a
	 * stroke laid on dry land reached the model EMPTY and came back "that
	 * stroke has no points" instead of the refusal the spec asks for -- and
	 * the file no longer held what the user actually drew.  The model names
	 * the body from the first point that lands on water and reports how many
	 * missed, which is the same forgiveness said out loud. */
	for ( int k = 0; k <= steps; k++ ) {
		const double t = double( k ) / steps;
		const double wx = x0 + ( x1 - x0 ) * t;
		const double wy = y0 + ( y1 - y0 ) * t;
		WaterStrokePoint p;
		p.x = float( wx );
		p.y = float( wy );
		s.pts << p;
	}
	QString msg;
	doc->addStroke( s, &msg );
	message = msg;
	panel->solve();
	update();
	return msg;
}

// ---- the self-test -------------------------------------------------------

/*! WW_WATER_MARK_TEST: the house-style counts, each with a floor, plus the two
 *  behaviours a picture cannot show -- that a click selects the body under it
 *  and that the summary says what Save will do or why it cannot. */
/*! LANE WATER7: the harness reads the tool where the tool now IS.
 *
 *  It used to take the `Water Marking` dock. There is no such dock any more --
 *  the rows are a page of the left editor's stack, reached by the `Water` tab
 *  of the same segmented strip that carries Header / Blocks / Files. Nothing
 *  about the CHECKS changed, so the floors `water_mark.sh` carries (16 dock
 *  checks) are the same floors; only the three lines that showed, sized and
 *  grabbed the container moved to the left dock. */
void runSelfTest( QMainWindow * mw, WaterMarkPanel * panel )
{
	/* LANE WATER8: the rows are a tab of the LOD GENERATION PANEL now, so the
	 * dock this grabs and the strip this drives are that panel's, not the left
	 * editor's. NO CHECK BELOW CHANGED -- the same assertions on the same
	 * widgets, in the dock they moved to, so this spell's dock floor of 16 and
	 * its 20/0 are the same floors on the same rules. */
	QDockWidget * dock = mw ? mw->findChild<QDockWidget *>( QStringLiteral( "LodGenerationDock" ) )
							: nullptr;
	auto * tabs = mw ? mw->findChild<QTabBar *>( QStringLiteral( "LodPanelModeSelector" ) )
					 : nullptr;
	if ( !dock || !panel )
		return;
	/* A harness FORCES the state it measures rather than inheriting whatever
	 * the last session left (ww-test-harness-add section 6): the workspace is
	 * opened, then the Water tab selected, before anything is read. */
	dock->show();
	QApplication::processEvents();
	if ( tabs )
		for ( int t = 0; t < tabs->count(); t++ )
			if ( tabs->tabText( t ) == QObject::tr( "Water" ) ) {
				tabs->setCurrentIndex( t );
				break;
			}

	QFile logf( QApplication::applicationDirPath() + QStringLiteral( "/ww_water_mark_test.log" ) );
	if ( !logf.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return;
	QTextStream log( &logf );
	int checksRun = 0, fails = 0;
	auto check = [&]( const QString & what, bool pass ) {
		checksRun++;
		if ( !pass )
			fails++;
		log << ( pass ? "  ok   " : "  FAIL " ) << what << "\n";
	};

	dock->show();
	QApplication::processEvents();

	// the refusal, BEFORE a file is open -- the other side of the summary's floor
	check( QStringLiteral( "with no file open the panel says so: \"%1\"" )
		.arg( panel->summaryLabel()->text() ),
		panel->summaryLabel()->text().contains( QLatin1String( "No landscape file" ) )
		&& !panel->saveBtn()->isEnabled() );

	const QString file = QString::fromLocal8Bit( qgetenv( "WW_WATER_MARK_FILE" ) );
	const bool opened = !file.isEmpty() && panel->openFile( file );
	check( QStringLiteral( "the landscape file opens: %1" ).arg( file ), opened );

	// ---- the house style, counted, each with a floor ---------------------
	int numbers = 0, plain = 0;
	for ( QAbstractSpinBox * s : panel->findChildren<QAbstractSpinBox *>() ) {
		numbers++;
		if ( !s->property( "wwScrubbed" ).toBool() )
			plain++;
	}
	log << "number fields: " << numbers << ", left as plain spin boxes: " << plain << "\n";
	check( QStringLiteral( "the numbers are scrub fields, like every other number" ),
		numbers >= 2 && plain == 0 );

	const int groups = panel->findChildren<QGroupBox *>().size();
	int headings = 0;
	for ( QLabel * l : panel->findChildren<QLabel *>() )
		if ( l->styleSheet().contains( QLatin1String( "font-weight: 600" ) ) )
			headings++;
	log << "group boxes: " << groups << ", headings: " << headings << "\n";
	check( QStringLiteral( "sections are headings, not group-box frames" ),
		groups == 0 && headings >= 4 );

	int combos = 0, unmatched = 0;
	for ( QComboBox * c : panel->findChildren<QComboBox *>() ) {
		combos++;
		if ( !c->styleSheet().contains( QLatin1String( "drop-down" ) ) )
			unmatched++;
	}
	log << "selectors: " << combos << ", in default chrome: " << unmatched << "\n";
	check( QStringLiteral( "every selector takes the matched field chrome" ),
		combos >= 5 && unmatched == 0 );

	int boxes = 0, dashed = 0, untipped = 0;
	for ( QCheckBox * c : panel->findChildren<QCheckBox *>() ) {
		boxes++;
		if ( c->text().contains( QLatin1String( " - " ) ) )
			dashed++;
		if ( c->toolTip().isEmpty() )
			untipped++;
	}
	log << "check boxes: " << boxes << ", with a dash explanation: " << dashed
		<< ", without a tooltip: " << untipped << "\n";
	check( QStringLiteral( "labels are names; the explanation is the tooltip" ),
		boxes >= 2 && dashed == 0 && untipped == 0 );

	// one setting a row, measured on the laid-out panel's GEOMETRY
	{
		QList<int> ys;
		int fields = 0;
		for ( const char * n : { "WaterMarkToolBox", "WaterMarkSpeedSpin", "WaterMarkWidthSpin",
				"WaterMarkDyeFadeSpin", "WaterMarkClassBox", "WaterMarkFormBox", "WaterMarkNameEdit" } ) {
			auto * w = panel->findChild<QWidget *>( QLatin1String( n ) );
			if ( !w )
				continue;
			fields++;
			const int y = w->mapTo( panel, QPoint( 0, 0 ) ).y();
			if ( !ys.contains( y ) )
				ys.append( y );
		}
		log << "settings: " << fields << " on " << ys.size() << " distinct rows\n";
		check( QStringLiteral( "settings sit one to a row" ), fields >= 6 && ys.size() == fields );
	}

	// the three bands: the settings scroll, the canvas and the actions do not
	{
		auto * sc = panel->settingsScroll();
		auto * cv = panel->findChild<QWidget *>( QStringLiteral( "WaterMarkCanvas" ) );
		auto * sum = panel->summaryLabel();
		check( QStringLiteral( "Save sits outside the scrolling settings" ),
			sc && !sc->isAncestorOf( panel->saveBtn() ) );
		check( QStringLiteral( "the map sits outside the scrolling settings" ),
			sc && cv && !sc->isAncestorOf( cv ) );
		check( QStringLiteral( "the summary sits outside the scrolling settings" ),
			sc && sum && !sc->isAncestorOf( sum ) );
		auto * tb = panel->findChild<QWidget *>( QStringLiteral( "WaterMarkToolBox" ) );
		check( QStringLiteral( "the settings themselves do scroll" ),
			sc && tb && sc->isAncestorOf( tb ) );
	}

	/* AND HOW MANY OF THEM THE BAND ACTUALLY SHOWS when the dock opens.
	 * "6 settings on 6 distinct rows" above is true of a panel whose
	 * settings band is 55 pixels tall, which is what the first screenshot
	 * of this dock showed: 0 of these 6 inside the visible viewport, every
	 * count green.  This is that picture as a number, with its floor. */
	{
		auto * sc = panel->settingsScroll();
		int shown = 0, counted = 0;
		if ( sc && sc->viewport() ) {
			const QRect vp = sc->viewport()->rect();
			for ( const char * n : { "WaterMarkToolBox", "WaterMarkSpeedSpin",
					"WaterMarkWidthSpin", "WaterMarkClassBox", "WaterMarkFormBox",
					"WaterMarkNameEdit" } ) {
				auto * w = panel->findChild<QWidget *>( QLatin1String( n ) );
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
		check( QStringLiteral( "the settings band opens showing its settings (%1 of %2, "
			"floor 5)" ).arg( shown ).arg( counted ), counted >= 6 && shown >= 5 );
	}

	// the fold
	{
		auto * arrow = panel->findChild<QToolButton *>( QStringLiteral( "WaterMarkBakeExpander" ) );
		auto * body = panel->findChild<QWidget *>( QStringLiteral( "WaterMarkBakeBody" ) );
		const bool wasOpen = body && body->isVisible();
		if ( arrow )
			arrow->click();
		QApplication::processEvents();
		const bool nowOpen = body && body->isVisible();
		check( QStringLiteral( "the Bake section folds (%1 -> %2)" )
			.arg( wasOpen ? "open" : "folded" ).arg( nowOpen ? "open" : "folded" ),
			arrow && body && wasOpen != nowOpen );
		if ( arrow )
			arrow->click();
		QApplication::processEvents();
	}

	// ---- the behaviour: select a body, mark it, and watch the sentence ----
	if ( opened && panel->document() ) {
		WaterMarkDoc * doc = panel->document();
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
		check( QStringLiteral( "the file offers a river to mark (body %1, %2 texels)" )
			.arg( river ).arg( best ), river > 0 );
		panel->selectBody( river );
		const QString sel = panel->summaryLabel()->text();
		log << "the summary with a body selected: '" << sel.split( QLatin1Char( '\n' ) ).value( 1 )
			<< "'\n";
		check( QStringLiteral( "selecting a body puts it in the sentence" ),
			sel.contains( QStringLiteral( "body %1" ).arg( river ) ) );
		LodtWaterBody rb;
		doc->body( river, rb );
		const double x0 = ( double( rb.x0 ) + 0.5 ) * 4096.0;
		const double y0 = ( double( rb.y0 ) + 0.5 ) * 4096.0;
		const double x1 = ( double( rb.x1 ) + 0.5 ) * 4096.0;
		const double y1 = ( double( rb.y1 ) + 0.5 ) * 4096.0;
		const int before = doc->strokes().size();
		const QString msg = panel->view()->layStroke( x0, y0, x1, y1 );
		log << "the canvas says: '" << msg << "'\n";
		check( QStringLiteral( "a stroke drawn on the map is stored (%1 -> %2)" )
			.arg( before ).arg( doc->strokes().size() ), doc->strokes().size() == before + 1 );
		LodtWaterBody after;
		doc->body( river, after );
		check( QStringLiteral( "and the body's flow now comes from a stroke (source %1 -> %2)" )
			.arg( rb.flowSource ).arg( after.flowSource ), after.flowSource == 4 );
		const QString sum2 = panel->summaryLabel()->text();
		log << "the summary then says: '" << sum2.split( QLatin1Char( '\n' ) ).value( 0 ) << "'\n";
		check( QStringLiteral( "the sentence counts the stroke and names the file" ),
			sum2.contains( QLatin1String( "Write 1 stroke" ) ) && panel->saveBtn()->isEnabled() );

		// a stroke on dry land: refused, in words, and nothing stored
		const int n2 = doc->strokes().size();
		double bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
		/* Found in the file, never assumed: the worldspace corner is open sea
		 * on the Commonwealth, so a stroke laid there is correctly ACCEPTED. */
		const bool haveDry = doc->dryStroke( bx0, by0, bx1, by1 );
		log << "the dry point found in the file is (" << bx0 << ", " << by0 << ")\n";
		check( QStringLiteral( "a dry point was found in the file, not assumed" ), haveDry );
		const QString dry = panel->view()->layStroke( bx0, by0, bx1, by1 );
		log << "on dry land the canvas says: '" << dry << "'\n";
		check( QStringLiteral( "a stroke on dry land is refused in words and stored nowhere" ),
			doc->strokes().size() == n2 && dry.contains( QLatin1String( "dry land" ) ) );
	}

	/* WW_WATER_MARK_SHOT=<png>: the dock as a person would see it, from inside
	 * the app, in the state this test leaves -- a river selected and marked. */
	const QByteArray shot = qgetenv( "WW_WATER_MARK_SHOT" );
	if ( !shot.isEmpty() ) {
		mw->resizeDocks( { dock }, { 660 }, Qt::Horizontal );
		QApplication::processEvents();
		QApplication::processEvents();
		const bool saved = dock->grab().save( QString::fromLocal8Bit( shot ) );
		log << "screenshot " << ( saved ? "saved: " : "NOT saved: " )
			<< QString::fromLocal8Bit( shot ) << "\n";
	}
	log << checksRun << " checks, " << fails << " failures\n";
	log << ( fails ? "FAIL" : "PASS" ) << "\n";
	logf.close();
	QTimer::singleShot( 100, qApp, &QApplication::quit );
}

} // namespace

void waterMarkInstall( QMainWindow * mw )
{
	if ( !mw )
		return;
	/* A TAB OF THE LOD GENERATION PANEL, NOT A WORKSPACE OF ITS OWN.
	 *
	 * bungo, 2026-09-10, on a screenshot of the Workspaces menu: *"Two issues
	 * with water window and water marking appearing here"*, corrected the same
	 * minute to *"They should be in the LOD gen workspace"*, and then, on a
	 * screenshot of the Header | Blocks | Files strip, *"You'd access them like
	 * this"*.
	 *
	 * LANE WATER7 READ THAT LAST SENTENCE AS THE PLACE and put a fourth tab in
	 * the LEFT strip. He saw it and said, verbatim: *"What? I wanted it in that
	 * right panel though"*. So the strip was the STYLE and the LOD Generation
	 * PANEL -- the dock on the right -- is the PLACE. Lane WATER8 moves it
	 * there and gives the left strip its three tabs back.
	 *
	 * WHERE IT GOES NOW. `tlCreateLodGenerationDock` (src/lodgenmanager.cpp)
	 * builds that dock as a segmented strip over a page stack, in exactly the
	 * skin the left editor's strip uses -- `LodPanelModeSelector` over
	 * `LodPanelStack`, one tab `LOD` over the generator's settings. This adds
	 * the second page and the second tab, `Water`, and nothing else: no water
	 * workspace, no water dock, and no entry in the Workspaces menu for either
	 * half of the tool. The button inside the rows still opens the full-screen
	 * flow window (src/waterwindow.cpp, unchanged). The land file IS a LOD
	 * product, which is his reason for putting it here.
	 *
	 * NO SCOPING RULE IS NEEDED ANY MORE, and that is a real part of the point:
	 * the strip lives INSIDE the LOD Generation dock, so it is on screen
	 * exactly while that workspace is, by construction. WATER7 needed a
	 * visibilityChanged connection and a "never leave the strip on a tab that
	 * is about to vanish" dance to imitate that from the left dock; both are
	 * gone, and with them the two ways they could disagree.
	 *
	 * The tab's `tabData` is the STACK PAGE INDEX, exactly as the `LOD` tab's
	 * is, which is why the index is asserted rather than assumed below. */
	auto * tabs = mw->findChild<QTabBar *>( QStringLiteral( "LodPanelModeSelector" ) );
	auto * stack = mw->findChild<QStackedWidget *>( QStringLiteral( "LodPanelStack" ) );
	if ( !tabs || !stack ) {
		qWarning( "the water tool needs the LOD Generation panel's tab strip and page stack "
			"(LodPanelModeSelector / LodPanelStack); this window has %s, so the "
			"Water tab is not installed",
			tabs ? "the stack missing" : ( stack ? "the strip missing" : "neither" ) );
		return;
	}

	auto * panel = new WaterMarkPanel( stack );
	panel->setObjectName( QStringLiteral( "WaterMarkPanel" ) );
	const int page = stack->addWidget( panel );
	/* THE REFUSAL, IN WORDS (CONSTITUTION 10, modules and fallbacks). The strip
	 * ships with one page, the generator's, so the water page is index 1. A
	 * page landing anywhere else means something has already added one and the
	 * `LOD` tab's own mapping can no longer be trusted; a tab that silently
	 * shows the wrong page is worse than no tab, because it reads as a broken
	 * tool rather than an absent one. */
	if ( page != 1 ) {
		qWarning( "the LOD Generation panel's page stack already has %d pages, so the water "
			"page landed at index %d and not at 1; the Water tab is not installed",
			stack->count(), page );
		stack->removeWidget( panel );
		delete panel;
		return;
	}

	const int waterTab = tabs->addTab( QObject::tr( "Water" ) );
	tabs->setTabData( waterTab, page );
	tabs->setTabToolTip( waterTab,
		QObject::tr( "Mark water direction, colour and dye on the open landscape file" ) );

	waterWindowInstall( mw );      // WW_WATER_WINDOW_TEST only; the menu entry is gone

	if ( qEnvironmentVariableIsSet( "WW_WATER_MARK_TEST" ) ) {
		/* A timer and not a load signal: this file knows nothing about the
		 * document, and the panel opens its own landscape file. */
		QTimer::singleShot( 1200, panel, [mw, panel]() { runSelfTest( mw, panel ); } );
	}
}
