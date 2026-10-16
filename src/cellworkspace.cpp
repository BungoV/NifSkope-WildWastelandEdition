/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellworkspace.h"

#include "cellclick.h"
#include "cellpick.h"
#include "cellrefs.h"
#include "glview.h"
#include "wwskin.h"

#include "model/nifmodel.h"

#include "cellview.h"
#include "cellfarlod.h"
#include "cellmodpickbox.h"	// the Mods band
#include "gl/celllights.h"
#include "gl/cellcull.h"	// the Culling row
#include "gl/cellssr.h"	// the SSR rows
#include "gl/cellwater.h"	// the Water row
#include "cellaodecal.h"	// the AO decals row
#include "gl/gametaa.h"
#include "gl/cellgrass.h"
#include "gl/cellpost.h"
#include "gl/cellbloom.h"
#include "ui/widgets/wwnumberfield.h"	// the Bloom rows

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QCoreApplication>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QColor>
#include <QComboBox>
#include <QGridLayout>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStringList>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <cmath>

#include <algorithm>

// ===========================================================================
// THE OPEN CELL, AND THE ROWS' OVERRIDES OVER ITS SPEC
// ===========================================================================

namespace {

QString g_path;             //!< the `.wwcell` that is open
QString g_notes;            //!< the builder's own notes for it
CellSceneSpec g_spec;       //!< the spec it was built from
bool g_haveOverrides = false;
CellSceneSpec g_over;       //!< only the fields a touched row owns are read

} // namespace

void cellWorkspaceApplyOverrides( CellSceneSpec & spec )
{
	if ( !g_haveOverrides )
		return;
	/* Only the fields a row owns; worldspace, cell, block size, plugins and data root stay the file's. */
	spec.overlay = g_over.overlay;
	spec.terrain = g_over.terrain;
	spec.water = g_over.water;
	spec.grid = g_over.grid;
	spec.showMarkers = g_over.showMarkers;
	spec.showDisabled = g_over.showDisabled;
	spec.probes = g_over.probes;
	spec.probesShow = g_over.probesShow;
	spec.probesBake = g_over.probesBake;
}

void cellWorkspaceNoteOpened( const QString & path, const CellSceneSpec & spec,
	const QString & notes )
{
	g_path = path;
	g_spec = spec;
	g_notes = notes;
	// a bake is one click, not a mode: the next rebuild does not bake again
	g_spec.probesBake = false;
	g_over.probesBake = false;
	if ( !g_haveOverrides ) {
		// The first cell of the session seeds the rows from the file itself, so
		// a row shows what is actually drawn before anybody touches one.
		g_over = spec;
	}
	/* The builder's sceneChanged fires before these notes are stored, so ring again now
	 * or the panel shows the previous build. */
	emit CellPickBus::instance()->sceneChanged();
}

QString cellWorkspacePath()
{
	return g_path;
}

bool cellWorkspaceHasOverrides()
{
	return g_haveOverrides;
}

void cellWorkspaceForget()
{
	g_path.clear();
	g_notes.clear();
	g_spec = CellSceneSpec();
	g_haveOverrides = false;
	g_over = CellSceneSpec();
}

// ===========================================================================
// READING THE BUILDER'S NOTES -- the ONE producer of the census and the legend
// ===========================================================================

namespace {

//! One legend bucket exactly as the builder printed it.
struct LegendRow
{
	QString key;
	int placements = 0;
	float rgb[3] = { 0, 0, 0 };
	QString label;
};

/*!  key 0x<hex>  <n> placements  rgb r,g,b  <label>
 * Parses src/cellview.cpp's line; if its shape changes this returns nothing and the legend says so. */
QVector<LegendRow> legendFromNotes( const QString & notes )
{
	QVector<LegendRow> out;
	static const QRegularExpression re(
		QStringLiteral( "^\\s*key 0x([0-9a-fA-F]+)\\s+(\\d+) placements\\s+rgb "
			"([0-9.]+),([0-9.]+),([0-9.]+)\\s*(.*)$" ) );
	const QStringList lines = notes.split( QLatin1Char( '\n' ) );
	for ( const QString & line : lines ) {
		const QRegularExpressionMatch m = re.match( line );
		if ( !m.hasMatch() )
			continue;
		LegendRow r;
		r.key = QStringLiteral( "0x" ) + m.captured( 1 );
		r.placements = m.captured( 2 ).toInt();
		for ( int k = 0; k < 3; k++ )
			r.rgb[k] = m.captured( 3 + k ).toFloat();
		r.label = m.captured( 6 ).trimmed();
		out.append( r );
	}
	return out;
}

//! `  source triangles N, welded shapes N, vertices N, triangles N`
bool geometryFromNotes( const QString & notes, qint64 & verts, qint64 & tris, qint64 & shapes )
{
	static const QRegularExpression re(
		QStringLiteral( "source triangles (\\d+), welded shapes (\\d+), "
			"vertices (\\d+), triangles (\\d+)" ) );
	const QRegularExpressionMatch m = re.match( notes );
	if ( !m.hasMatch() )
		return false;
	shapes = m.captured( 2 ).toLongLong();
	verts = m.captured( 3 ).toLongLong();
	tris = m.captured( 4 ).toLongLong();
	return true;
}

//! The `-- REFUSED: ...` the builder attached to the overlay line, if any.
QString refusalFromNotes( const QString & notes )
{
	const QStringList lines = notes.split( QLatin1Char( '\n' ) );
	for ( const QString & line : lines ) {
		const int at = line.indexOf( QLatin1String( "-- REFUSED: " ) );
		if ( at >= 0 && line.contains( QLatin1String( "overlay:" ) ) )
			return line.mid( at + 12 ).trimmed();
	}
	return QString();
}

QString formatCount( qint64 n )
{
	QString s = QString::number( n );
	for ( int at = s.size() - 3; at > 0; at -= 3 )
		s.insert( at, QLatin1Char( ' ' ) );
	return s;
}

} // namespace

// ===========================================================================
// THE PANEL
// ===========================================================================

CellWorkspacePanel::CellWorkspacePanel( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "CellWorkspacePanel" ) );
	buildUi();

	connect( CellPickBus::instance(), &CellPickBus::picked,
		this, &CellWorkspacePanel::onPicked );
	connect( CellPickBus::instance(), &CellPickBus::sceneChanged,
		this, &CellWorkspacePanel::onSceneChanged );

	onSceneChanged();
}

/* The Show table: one row per toggle, so a toggle is added here and nowhere else.
 * `id` is both the QSettings key and the object-name suffix. Only fields CellSceneSpec has get a row. */
struct CellShowRow
{
	const char * id;
	const char * label;
	bool def;
	//! Which `CellSceneSpec` bool this row is. There is no row without one.
	bool CellSceneSpec::* field;
};

static const CellShowRow g_showRows[] = {
	/* "Cell borders" is the spec's `grid`; renamed in the UI only, since `.wwcell` files use the spec name. */
	{ "borders",  QT_TRANSLATE_NOOP( "CellWorkspacePanel", "Cell borders" ),        false, &CellSceneSpec::grid },
	{ "ground",   QT_TRANSLATE_NOOP( "CellWorkspacePanel", "Ground" ),              true,  &CellSceneSpec::terrain },
	{ "water",    QT_TRANSLATE_NOOP( "CellWorkspacePanel", "Water" ),               true,  &CellSceneSpec::water },
	{ "markers",  QT_TRANSLATE_NOOP( "CellWorkspacePanel", "Markers" ),             false, &CellSceneSpec::showMarkers },
	{ "disabled", QT_TRANSLATE_NOOP( "CellWorkspacePanel", "Disabled references" ), false, &CellSceneSpec::showDisabled }
};
static const int g_showRowCount = int( sizeof( g_showRows ) / sizeof( g_showRows[0] ) );

//! The one QSettings group the Show rows live in. Named once, used everywhere.
static const char * const CELL_SHOW_GROUP = "CellEditor/Show";

int CellWorkspacePanel::showRowCount() const
{
	return showActions.size();
}

QString CellWorkspacePanel::showRowLabel( int i ) const
{
	if ( i < 0 || i >= showActions.size() || !showActions.at( i ) )
		return QString();
	return showActions.at( i )->text();
}

bool CellWorkspacePanel::showRowChecked( int i ) const
{
	if ( i < 0 || i >= showActions.size() || !showActions.at( i ) )
		return false;
	return showActions.at( i )->isChecked();
}

void CellWorkspacePanel::setShowRow( int i, bool on )
{
	if ( i < 0 || i >= showActions.size() || !showActions.at( i ) )
		return;
	if ( showActions.at( i )->isChecked() == on )
		return;
	// setChecked emits toggled, which is the SAME path a click takes.
	showActions.at( i )->setChecked( on );
}

void CellWorkspacePanel::buildUi()
{
	QVBoxLayout * page = new QVBoxLayout( this );
	page->setContentsMargins( 6, 6, 6, 6 );
	page->setSpacing( 4 );

	// ---- band 1: the references
	page->addWidget( wwHeading( tr( "References" ), this ) );

	QWidget * filterRow = new QWidget( this );
	QHBoxLayout * fl = new QHBoxLayout( filterRow );
	fl->setContentsMargins( 0, 0, 0, 0 );
	fl->setSpacing( 4 );
	filterType = new QComboBox( filterRow );
	filterType->setObjectName( QStringLiteral( "CellWorkspaceTypeFilter" ) );
	filterType->addItem( tr( "All" ) );
	filterText = new QLineEdit( filterRow );
	filterText->setObjectName( QStringLiteral( "CellWorkspaceTextFilter" ) );
	filterText->setClearButtonEnabled( true );
	filterText->setPlaceholderText( tr( "Editor ID, form or model" ) );
	fl->addWidget( filterType );
	fl->addWidget( filterText, 1 );
	page->addWidget( filterRow );

	list = new QTreeWidget( this );
	list->setObjectName( QStringLiteral( "CellWorkspaceList" ) );
	/* One row per placed reference, FLAT with a Cell column (not nested like Blender/CK) so sorting stays
	 * global. `State` is CellRefTable::fateName, so undrawn references (lights, markers) are listed too. */
	list->setColumnCount( 6 );
	list->setHeaderLabels( { tr( "Type" ), tr( "Editor ID" ), tr( "Form" ),
		tr( "Tris" ), tr( "Cell" ), tr( "State" ) } );
	// FLAT, like every record view in this fork: no root decoration, no nesting.
	list->setRootIsDecorated( false );
	list->setUniformRowHeights( true );
	list->setAlternatingRowColors( false );
	list->setSelectionMode( QAbstractItemView::SingleSelection );
	list->setEditTriggers( QAbstractItemView::NoEditTriggers );
	list->setSortingEnabled( true );
	list->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	list->header()->setSectionResizeMode( 1, QHeaderView::Stretch );
	list->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
	list->header()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
	list->header()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );
	list->header()->setSectionResizeMode( 5, QHeaderView::ResizeToContents );
	list->setStyleSheet( wwSelectionTreeQss() );
	page->addWidget( list, 1 );

	// ---- band 2: the view
	page->addWidget( wwHeading( tr( "View" ), this ) );

	QWidget * viewRow = new QWidget( this );
	QHBoxLayout * vl = new QHBoxLayout( viewRow );
	vl->setContentsMargins( 0, 0, 0, 0 );
	vl->setSpacing( 4 );
	QLabel * overlayName = new QLabel( tr( "Color" ), viewRow );
	overlayBox = new QComboBox( viewRow );
	overlayBox->setObjectName( QStringLiteral( "CellWorkspaceOverlay" ) );
	/* The names are the enum's own spellings (`cellOverlayName`), so the row,
	 * the `WW_CELL_OVERLAY` variable, the notes line and the gate all say the
	 * same word for the same thing. */
	overlayBox->addItem( tr( "Textured" ), cellOverlayName( CellOverlay::None ) );
	for ( const QString & n : cellOverlayNames().split( QLatin1String( ", " ) ) ) {
		const QString name = n.trimmed();
		if ( name.isEmpty() || name == cellOverlayName( CellOverlay::None ) )
			continue;
		overlayBox->addItem( name, name );
	}
	vl->addWidget( overlayName );
	vl->addWidget( overlayBox, 1 );
	page->addWidget( viewRow );

	/* The Show popover is a child of this panel, so it only ever appears in the Cell workspace. */
	QToolButton * showButton = new QToolButton( viewRow );
	showButton->setObjectName( QStringLiteral( "CellWorkspaceShowButton" ) );
	showButton->setText( tr( "Show" ) );
	showButton->setPopupMode( QToolButton::InstantPopup );
	showButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
	showButton->setAutoRaise( false );
	showButton->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
	QMenu * showMenu = new QMenu( showButton );
	showMenu->setObjectName( QStringLiteral( "CellWorkspaceShowMenu" ) );

	/* The Show dropdown is the one home of every viewport feature switch, in three sections. Water is
	 * one tick that also drives the water shader switch, so the two cannot disagree. */
	auto tick = [&]( QMenu * m, const QString & label, const char * name, bool on ) {
		QAction * a = m->addAction( label );
		a->setObjectName( QLatin1String( name ) );
		a->setCheckable( true );
		a->setChecked( on );
		return a;
	};
	/* A heading row per section: QMenu::addSection's text is not drawn by this style, so use the
	 * panel's band heading in a widget action, after a separator. */
	auto section = [&]( const QString & name, const char * id ) {
		if ( !showMenu->actions().isEmpty() )
			showMenu->addSeparator();
		QLabel * h = wwHeading( name, showMenu );
		h->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; padding: 4px 8px 2px 8px; }" ) );
		auto * wa = new QWidgetAction( showMenu );
		wa->setDefaultWidget( h );
		wa->setText( name );
		wa->setObjectName( QStringLiteral( "CellWorkspaceShowSection_" ) + QLatin1String( id ) );
		showMenu->addAction( wa );
	};
	section( tr( "World" ), "world" );
	QSettings showSettings;
	for ( int i = 0; i < g_showRowCount; i++ ) {
		const CellShowRow & row = g_showRows[i];
		QAction * a = showMenu->addAction(
			QCoreApplication::translate( "CellWorkspacePanel", row.label ) );
		a->setObjectName( QStringLiteral( "CellWorkspaceShow_" ) + QLatin1String( row.id ) );
		a->setCheckable( true );
		a->setChecked( showSettings.value(
			QString( "%1/%2" ).arg( QLatin1String( CELL_SHOW_GROUP ), QLatin1String( row.id ) ),
			row.def ).toBool() );
		connect( a, &QAction::toggled, this, [this, i]( bool on ) {
			QSettings().setValue( QString( "%1/%2" )
				.arg( QLatin1String( CELL_SHOW_GROUP ), QLatin1String( g_showRows[i].id ) ), on );
			if ( g_showRows[i].field == &CellSceneSpec::water )
				wwCellWaterSetOn( on );	// the one Water tick
			viewRowChanged();
		} );
		showActions.append( a );
	}
	cellGrass = tick( showMenu, tr( "Grass" ), "CellWorkspaceCellGrass", wwCellGrassOn() );
	cellParticles = tick( showMenu, tr( "Particles" ), "CellWorkspaceParticles", wwCellParticlesOn() );
	cellPrevis = tick( showMenu, tr( "Previs" ), "CellWorkspacePrevis", wwCellPrevisOn() );
	cellCull = tick( showMenu, tr( "Culling" ), "CellWorkspaceCulling", wwCellCullOn() );
	cellFarLod = tick( showMenu, tr( "Far LOD" ), "CellWorkspaceFarLod", cellFarLodWanted() );
	// the LOD type, Vanilla | FO4CS; unset, it shows what the last build resolved to
	cellFarLodType = showMenu->addMenu( tr( "Far LOD source" ) );
	cellFarLodType->setObjectName( QStringLiteral( "CellWorkspaceFarLodType" ) );
	cellFarLodTypeGroup = new QActionGroup( cellFarLodType );
	cellFarLodTypeGroup->setExclusive( true );
	{
		const int t = cellFarLodTypePinned() != WwFarLodAuto ? cellFarLodTypePinned() : cellFarLodLastType();
		const QStringList names = { tr( "Vanilla" ), tr( "FO4CS" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellFarLodType->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == ( t == WwFarLodVanilla ? 0 : 1 ) );
			cellFarLodTypeGroup->addAction( a );
		}
	}
	section( tr( "Lighting" ), "lighting" );
	cellLights = tick( showMenu, tr( "Cell lights" ), "CellWorkspaceCellLights", wwCellLightsOn() );
	cellGi = tick( showMenu, tr( "GI" ), "CellWorkspaceCellGi", wwCellGiOn() );
	/* Skylighting strength: how much the GI's sky visibility darkens, 0-100 %. Live (the grid relights
	 * 0.4 s after the value stands still). */
	{
		QWidget * panel = new QWidget( showMenu );
		panel->setObjectName( QStringLiteral( "CellWorkspaceSkyVisPanel" ) );
		QGridLayout * gl = new QGridLayout( panel );
		gl->setContentsMargins( 8, 2, 8, 2 );
		gl->setHorizontalSpacing( 8 );
		gl->setColumnMinimumWidth( 0, 96 );
		gl->setColumnStretch( 1, 1 );
		QLabel * l = new QLabel( tr( "Sky visibility" ), panel );
		WwNumberField * f = new WwNumberField( panel );
		f->setObjectName( QStringLiteral( "CellWorkspaceSkyVis" ) );
		f->setDecimals( 0 );
		f->setRange( 0.0, 100.0 );
		f->setSingleStep( 5.0 );
		f->setSuffix( QStringLiteral( " %" ) );
		f->setMinimumWidth( 96 );
		f->setValue( wwCellSkyVisPercent() );
		const bool pinned = wwCellSkyVisPinned();
		const QString tip = tr( "How much the sky the GI sees is blocked: 0 % the open sky everywhere, 100 % as baked "
			"(FO4CS Skylighting fStrength)" ) + ( pinned ? tr( " (pinned by the environment)" ) : QString() );
		l->setToolTip( tip );
		f->setToolTip( tip );
		gl->addWidget( l, 0, 0 );
		gl->addWidget( f, 0, 1 );
		auto greys = [l, f, pinned]( bool giOn ) {
			l->setEnabled( giOn && !pinned );
			f->setEnabled( giOn && !pinned );
		};
		greys( wwCellGiOn() );
		connect( cellGi, &QAction::toggled, this, greys );
		connect( f, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this]( double v ) {
			wwCellSkyVisSetPercent( int( std::lround( v ) ) );
			if ( glView )
				glView->update();
		} );
		auto * wa = new QWidgetAction( showMenu );
		wa->setDefaultWidget( panel );
		wa->setText( tr( "Sky visibility" ) );
		wa->setObjectName( QStringLiteral( "CellWorkspaceSkyVisRow" ) );
		showMenu->addAction( wa );
	}
	cellFar = tick( showMenu, tr( "Far light" ), "CellWorkspaceCellFar", wwCellFarOn() );
	// the Pass, live once a bake is relit
	cellPass = showMenu->addMenu( tr( "Far light mode" ) );
	cellPass->setObjectName( QStringLiteral( "CellWorkspaceCellPass" ) );
	cellPassGroup = new QActionGroup( cellPass );
	cellPassGroup->setExclusive( true );
	{
		const QStringList names = wwCellPassNames();
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellPass->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == wwCellPass() );
			cellPassGroup->addAction( a );
		}
	}
	cellPass->setEnabled( false );
	cellShadow = tick( showMenu, tr( "Light shadows" ), "CellWorkspaceLightShadows", wwCellShadowOn() );
	cellAo = tick( showMenu, tr( "AO" ), "CellWorkspaceAo", wwCellAoOn() );
	cellAoDecal = tick( showMenu, tr( "AO decals" ), "CellWorkspaceAoDecals", aoDecalOn() );
	cellGiGpu = tick( showMenu, tr( "GPU relight" ), "CellWorkspaceGpuRelight", wwCellGiGpuOn() );
	section( tr( "Effects" ), "effects" );
	cellIs = tick( showMenu, tr( "Imagespace" ), "CellWorkspaceCellImageSpace", wwCellImageSpaceOn() );
	// the game's temporal AA, the same state as the Scene popup's Temporal AA row; ships off
	cellTaa = tick( showMenu, tr( "Temporal AA" ), "CellWorkspaceTemporalAa", wwGameTaaOn() );
	cellSsr = tick( showMenu, tr( "SSR" ), "CellWorkspaceSsr", wwCellSsrOn() );
	cellSsrExt = tick( showMenu, tr( "SSR outdoors" ), "CellWorkspaceSsrOutdoors", wwCellSsrExteriorOn() );
	cellDof = tick( showMenu, tr( "Depth of field" ), "CellWorkspaceCellDof", wwCellDofOn() );
	cellMblur = tick( showMenu, tr( "Motion blur" ), "CellWorkspaceCellMotionBlur", wwCellMotionBlurOn() );
	// the bloom: Vanilla (the imagespace's own, the default) | FO4CS with its settings and physical glare.
	// Every change applies on the next paint (gl/cellbloom.h)
	cellBloom = showMenu->addMenu( tr( "Bloom" ) );
	cellBloom->setObjectName( QStringLiteral( "CellWorkspaceBloom" ) );
	cellBloom->setToolTipsVisible( true );
	cellBloomGroup = new QActionGroup( cellBloom );
	cellBloomGroup->setExclusive( true );
	{
		const WwCellBloomSettings b = wwCellBloomSettings();
		const QStringList names = { tr( "Vanilla" ), tr( "FO4CS" ) };
		const QStringList tips = {
			tr( "The imagespace's own bloom: the blurred bright pass, added once before the exposure" ),
			tr( "Community Shaders' bloom: a threshold, a mip pyramid and a tent upsample on the exposed light" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellBloom->addAction( names.at( k ) );
			a->setObjectName( k ? QStringLiteral( "CellWorkspaceBloomFo4cs" ) : QStringLiteral( "CellWorkspaceBloomVanilla" ) );
			a->setCheckable( true );
			a->setData( k );
			a->setToolTip( tips.at( k ) );
			a->setChecked( k == ( b.fo4cs ? 1 : 0 ) );
			a->setEnabled( !wwCellBloomPinned( "mode" ) );
			cellBloomGroup->addAction( a );
		}
		// one label | field grid per block, one label width for both so the value edge is one line
		struct Row { const char * id; const char * name; QString label; QString tip; double lo, hi, step; int dec; QString prefix; };
		auto grid = [this]( const char * name, const QList<Row> & rows, QList<QWidget *> & out ) {
			QWidget * panel = new QWidget( cellBloom );
			panel->setObjectName( QLatin1String( name ) );
			QGridLayout * gl = new QGridLayout( panel );
			gl->setContentsMargins( 8, 2, 8, 2 );
			gl->setHorizontalSpacing( 8 );
			gl->setVerticalSpacing( 2 );
			gl->setColumnMinimumWidth( 0, 96 );
			gl->setColumnStretch( 1, 1 );
			for ( int i = 0; i < rows.size(); i++ ) {
				const Row & r = rows.at( i );
				QLabel * l = new QLabel( r.label, panel );
				WwNumberField * f = new WwNumberField( panel );
				f->setObjectName( QLatin1String( r.name ) );
				f->setDecimals( r.dec );
				f->setRange( r.lo, r.hi );
				f->setSingleStep( r.step );
				f->setPrefix( r.prefix );
				f->setMinimumWidth( 96 );
				const bool pinned = wwCellBloomPinned( r.id );
				const QString tip = pinned ? r.tip + tr( " (pinned by the environment)" ) : r.tip;
				l->setToolTip( tip );
				f->setToolTip( tip );
				f->setProperty( "wwPinned", pinned );
				gl->addWidget( l, i, 0 );
				gl->addWidget( f, i, 1 );
				out << l << f;
			}
			auto * wa = new QWidgetAction( cellBloom );
			wa->setDefaultWidget( panel );
			cellBloom->addAction( wa );
		};
		cellBloom->addSeparator();
		QList<QWidget *> fo, gw;
		grid( "CellWorkspaceBloomPanel", {
			{ "threshold", "CellWorkspaceBloomThreshold", tr( "Threshold" ),
				tr( "Exposed light below this does not bloom (FO4CS fBloomThreshold)" ), 0.0, 64.0, 0.05, 2, QString() },
			{ "knee", "CellWorkspaceBloomSoftKnee", tr( "Soft knee" ),
				tr( "How gently light crosses the threshold, as a share of it (fBloomSoftKnee)" ), 0.0, 1.0, 0.05, 2, QString() },
			{ "intensity", "CellWorkspaceBloomIntensity", tr( "Intensity" ),
				tr( "The bloom added to the frame (fBloomIntensity)" ), 0.0, 10.0, 0.05, 2, QString() },
			{ "radius", "CellWorkspaceBloomRadius", tr( "Radius" ),
				tr( "The upsample tent's reach in pixels of each level (fBloomUpsampleRadius)" ), 0.5, 4.0, 0.1, 2, QString() },
			{ "mips", "CellWorkspaceBloomMips", tr( "Mip levels" ),
				tr( "Halvings of the pyramid: more spreads the glow wider (iBloomMipLevels)" ), 0.0, 12.0, 1.0, 0, QString() },
		}, fo );
		cellBloom->addSeparator();
		cellBloomGlare = cellBloom->addAction( tr( "Physical glare" ) );
		cellBloomGlare->setObjectName( QStringLiteral( "CellWorkspaceBloomGlare" ) );
		cellBloomGlare->setCheckable( true );
		cellBloomGlare->setChecked( b.glare );
		cellBloomGlare->setToolTip( tr( "A lens's diffraction: each pyramid level weighted by the aperture's spread (bPhysicalGlareEnabled)" ) );
		grid( "CellWorkspaceBloomGlarePanel", {
			{ "strength", "CellWorkspaceBloomGlareStrength", tr( "Strength" ),
				tr( "The glare's energy, added to the intensity (fPhysicalGlareStrength)" ), 0.0, 4.0, 0.05, 2, QString() },
			{ "fnumber", "CellWorkspaceBloomGlareFNumber", tr( "F-number" ),
				tr( "The aperture: a higher f-number spreads the glare over wider levels (fPhysicalGlareFNumber)" ), 1.0, 22.0, 0.1, 1,
				QStringLiteral( "f/" ) },
			{ "dispersion", "CellWorkspaceBloomGlareDispersion", tr( "Dispersion" ),
				tr( "Red and blue pulled apart along the radius, in bloom pixels (fPhysicalGlareDispersion)" ), 0.0, 2.0, 0.05, 2, QString() },
		}, gw );
		auto field = []( const QList<QWidget *> & l, int i ) { return static_cast<WwNumberField *>( l.at( 2 * i + 1 ) ); };
		field( fo, 0 )->setValue( b.threshold );
		field( fo, 1 )->setValue( b.softKnee );
		field( fo, 2 )->setValue( b.intensity );
		field( fo, 3 )->setValue( b.radius );
		field( fo, 4 )->setValue( b.mips );
		field( gw, 0 )->setValue( b.glareStrength );
		field( gw, 1 )->setValue( b.glareFNumber );
		field( gw, 2 )->setValue( b.glareDispersion );
		// the rows grey with what they belong to: FO4CS's under Vanilla, the glare's while it is off; a pin greys its own
		auto greys = [this, fo, gw]() {
			const WwCellBloomSettings s = wwCellBloomSettings();
			for ( int i = 0; i < fo.size(); i += 2 ) {
				const bool on = s.fo4cs && !fo.at( i + 1 )->property( "wwPinned" ).toBool();
				fo.at( i )->setEnabled( on );
				fo.at( i + 1 )->setEnabled( on );
			}
			cellBloomGlare->setEnabled( s.fo4cs && !wwCellBloomPinned( "glare" ) );
			for ( int i = 0; i < gw.size(); i += 2 ) {
				const bool on = s.fo4cs && s.glare && !gw.at( i + 1 )->property( "wwPinned" ).toBool();
				gw.at( i )->setEnabled( on );
				gw.at( i + 1 )->setEnabled( on );
			}
		};
		greys();
		auto apply = [this, greys]( auto change ) {
			WwCellBloomSettings s = wwCellBloomSettings();
			change( s );
			wwCellBloomSet( s );
			greys();
			if ( glView )
				glView->update();
		};
		connect( cellBloomGroup, &QActionGroup::triggered, this, [apply]( QAction * a ) {
			const bool fo4cs = a->data().toInt() == 1;
			apply( [fo4cs]( WwCellBloomSettings & s ) { s.fo4cs = fo4cs; } );
		} );
		connect( cellBloomGlare, &QAction::toggled, this, [apply]( bool on ) {
			apply( [on]( WwCellBloomSettings & s ) { s.glare = on; } );
		} );
		float WwCellBloomSettings::* const floats[] = { &WwCellBloomSettings::threshold, &WwCellBloomSettings::softKnee,
			&WwCellBloomSettings::intensity, &WwCellBloomSettings::radius };
		for ( int i = 0; i < 4; i++ ) {
			float WwCellBloomSettings::* m = floats[i];
			connect( field( fo, i ), qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [apply, m]( double v ) {
				apply( [m, v]( WwCellBloomSettings & s ) { s.*m = float( v ); } );
			} );
		}
		connect( field( fo, 4 ), qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [apply]( double v ) {
			apply( [v]( WwCellBloomSettings & s ) { s.mips = int( std::lround( v ) ); } );
		} );
		float WwCellBloomSettings::* const gfloats[] = { &WwCellBloomSettings::glareStrength, &WwCellBloomSettings::glareFNumber,
			&WwCellBloomSettings::glareDispersion };
		for ( int i = 0; i < 3; i++ ) {
			float WwCellBloomSettings::* m = gfloats[i];
			connect( field( gw, i ), qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [apply, m]( double v ) {
				apply( [m, v]( WwCellBloomSettings & s ) { s.*m = float( v ); } );
			} );
		}
	}
	showButton->setMenu( showMenu );
	vl->addWidget( showButton );

	// ---- band 3: PRTP (Precomputed Radiance Transfer Probes) -- the probe controls only
	page->addWidget( wwHeading( tr( "PRTP" ), this ) );
	QWidget * prtpRow = new QWidget( this );
	QHBoxLayout * pl = new QHBoxLayout( prtpRow );
	pl->setContentsMargins( 0, 0, 0, 0 );
	pl->setSpacing( 4 );
	probesShow = new QCheckBox( tr( "Show probes" ), prtpRow );
	probesShow->setObjectName( QStringLiteral( "CellWorkspaceProbesShow" ) );
	probesShow->setChecked( QSettings().value(
		QString( "%1/probes" ).arg( QLatin1String( CELL_SHOW_GROUP ) ), true ).toBool() );
	probesPlace = new QPushButton( tr( "Place" ), prtpRow );
	probesPlace->setObjectName( QStringLiteral( "CellWorkspaceProbesPlace" ) );
	probesPlace->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 10px" ) ) );
	probesBake = new QPushButton( tr( "Bake" ), prtpRow );
	probesBake->setObjectName( QStringLiteral( "CellWorkspaceProbesBake" ) );
	probesBake->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 10px" ) ) );
	pl->addWidget( probesShow );
	pl->addStretch( 1 );
	pl->addWidget( probesPlace );
	pl->addWidget( probesBake );
	page->addWidget( prtpRow );

	probeKinds = new QTreeWidget( this );
	probeKinds->setObjectName( QStringLiteral( "CellWorkspaceProbeKinds" ) );
	probeKinds->setColumnCount( 2 );
	probeKinds->setHeaderLabels( { tr( "Kind" ), tr( "Probes" ) } );
	probeKinds->setRootIsDecorated( false );
	probeKinds->setUniformRowHeights( true );
	probeKinds->setSelectionMode( QAbstractItemView::NoSelection );
	probeKinds->setEditTriggers( QAbstractItemView::NoEditTriggers );
	probeKinds->setMinimumHeight( 222 );    // nine rows and the header, no scrolling
	probeKinds->setMaximumHeight( 222 );
	probeKinds->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	probeKinds->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	probeKinds->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	probeKinds->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	probeKinds->setStyleSheet( wwSelectionTreeQss() );
	probeKinds->setVisible( false );
	page->addWidget( probeKinds );

	connect( probesPlace, &QPushButton::clicked, this, &CellWorkspacePanel::placeProbes );
	connect( probesBake, &QPushButton::clicked, this, &CellWorkspacePanel::bakeProbes );
	// a shader switch, not a rebuild: the next frame draws with (or without) the cell's lights
	connect( cellLights, &QAction::toggled, this, [this]( bool on ) {
		wwCellLightsSetOn( on );
		if ( glView )
			glView->update();
	} );
	// the second row: a shader or pass switch repaints; a switch read when the cell opens reopens it.
	auto fxLive = [this]( QAction * box, void ( *set )( bool ) ) {
		connect( box, &QAction::toggled, this, [this, set]( bool on ) {
			set( on );
			if ( glView )
				glView->update();
		} );
	};
	fxLive( cellAo, &wwCellAoSetOn );
	fxLive( cellShadow, &wwCellShadowSetOn );
	fxLive( cellSsr, &wwCellSsrSetOn );
	fxLive( cellSsrExt, &wwCellSsrExteriorSetOn );
	auto fxReopen = [this]( QAction * box, void ( *set )( bool ) ) {
		connect( box, &QAction::toggled, this, [this, set]( bool on ) {
			set( on );
			if ( syncing || g_path.isEmpty() )
				return;	// nothing open: the next open reads it
			g_over = g_spec;
			g_haveOverrides = true;
			say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
			emit reopenRequested( g_path );
		} );
	};
	fxReopen( cellAoDecal, &aoDecalSetOn );
	fxReopen( cellGiGpu, &wwCellGiGpuSetOn );
	fxReopen( cellParticles, &wwCellParticlesSetOn );	// the systems are copied in when the cell opens
	connect( cellCull, &QAction::toggled, this, [this]( bool on ) {
		wwCellCullSetOn( on );
		if ( glView )
			glView->update();
	} );
	connect( cellPrevis, &QAction::toggled, this, [this]( bool on ) {
		wwCellPrevisSetOn( on );
		if ( glView )
			glView->update();
	} );
	connect( cellTaa, &QAction::toggled, this, [this]( bool on ) {
		wwGameTaaSetOn( on );
		if ( glView )
			glView->update();
	} );
	// the grass is placed when the cell opens, so the switch rebuilds the cell
	connect( cellGrass, &QAction::toggled, this, [this]( bool on ) {
		wwCellGrassSetOn( on );
		if ( syncing || g_path.isEmpty() )
			return;
		g_over = g_spec;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );
	// passes on the finished frame, no rebuild
	connect( cellDof, &QAction::toggled, this, [this]( bool on ) {
		wwCellDofSetOn( on );
		if ( glView )
			glView->update();
	} );
	connect( cellMblur, &QAction::toggled, this, [this]( bool on ) {
		wwCellMotionBlurSetOn( on );
		if ( glView )
			glView->update();
	} );
	// the bounce of the last bake, drawn while Cell lights is on
	connect( cellGi, &QAction::toggled, this, [this]( bool on ) {
		wwCellGiSetOn( on );
		if ( glView )
			glView->update();
	} );
	// a shader switch (the far sectors load with the cell)
	connect( cellFar, &QAction::toggled, this, [this]( bool on ) {
		wwCellFarSetOn( on );
		if ( glView )
			glView->update();
	} );
	connect( cellPassGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		wwCellSetPass( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
	// the game's exposure, curve, grade and LUT in place of the viewer's curve
	connect( cellIs, &QAction::toggled, this, [this]( bool on ) {
		wwCellImageSpaceSetOn( on );
		if ( glView )
			glView->update();
	} );
	// the far field is document geometry, so the cell is built again with (or without) it
	connect( cellFarLodTypeGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		cellFarLodSetType( a->data().toInt() == 0 ? WwFarLodVanilla : WwFarLodFo4cs );
		if ( syncing || g_path.isEmpty() || !cellFarLodWanted() )
			return;
		g_over = g_spec;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );
	connect( cellFarLod, &QAction::toggled, this, [this]( bool on ) {
		cellFarLodSetWanted( on );
		if ( syncing || g_path.isEmpty() )
			return;
		g_over = g_spec;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );
	connect( probesShow, &QCheckBox::toggled, this, [this]( bool on ) {
		QSettings().setValue( QString( "%1/probes" ).arg( QLatin1String( CELL_SHOW_GROUP ) ), on );
		if ( syncing || g_path.isEmpty() || !g_spec.probes )
			return;     // nothing placed: the choice waits for Place
		g_over = g_spec;
		g_over.probesShow = on;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );

	/* ---- which mods the cells load. The list, saved choice per worldspace and the one-stack rule live
	 * in cellmodpick.h; Apply or Reset rebuilds the cell like the Far LOD row. */
	page->addWidget( wwHeading( tr( "Mods" ), this ) );
	modPick = new CellModPickBox( this );
	modPick->onChoiceChanged = [this]() {
		if ( g_path.isEmpty() )
			return;	// nothing open: the next open reads it
		g_over = g_spec;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	};
	page->addWidget( modPick );

	// ---- band 4: the legend
	page->addWidget( wwHeading( tr( "Legend" ), this ) );
	legend = new QTreeWidget( this );
	legend->setObjectName( QStringLiteral( "CellWorkspaceLegend" ) );
	legend->setColumnCount( 3 );
	legend->setHeaderLabels( { tr( "Key" ), tr( "Placements" ), tr( "Name" ) } );
	legend->setRootIsDecorated( false );
	legend->setUniformRowHeights( true );
	legend->setSelectionMode( QAbstractItemView::NoSelection );
	legend->setEditTriggers( QAbstractItemView::NoEditTriggers );
	legend->setMaximumHeight( 150 );
	legend->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	legend->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	legend->header()->setSectionResizeMode( 2, QHeaderView::Stretch );
	legend->setStyleSheet( wwSelectionTreeQss() );
	page->addWidget( legend );

	/* Pinned under everything: the census, then the summary or refusal. */
	census = new QLabel( this );
	census->setObjectName( QStringLiteral( "CellWorkspaceCensus" ) );
	census->setWordWrap( true );
	page->addWidget( census );

	note = new QLabel( this );
	note->setObjectName( QStringLiteral( "CellWorkspaceNote" ) );
	note->setWordWrap( true );
	page->addWidget( note );

	connect( list, &QTreeWidget::itemSelectionChanged, this, &CellWorkspacePanel::rowClicked );
	connect( filterText, &QLineEdit::textChanged, this, &CellWorkspacePanel::filterChanged );
	connect( filterType, QOverload<int>::of( &QComboBox::currentIndexChanged ),
		this, &CellWorkspacePanel::filterChanged );
	connect( overlayBox, QOverload<int>::of( &QComboBox::currentIndexChanged ),
		this, &CellWorkspacePanel::viewRowChanged );

}

void CellWorkspacePanel::setNif( NifModel * n )
{
	nif = n;
}

void CellWorkspacePanel::setGLView( GLView * v )
{
	glView = v;
}

// ---- readers

int CellWorkspacePanel::totalRows() const
{
	return list ? list->topLevelItemCount() : 0;
}

int CellWorkspacePanel::visibleRows() const
{
	if ( !list )
		return 0;
	int n = 0;
	for ( int i = 0; i < list->topLevelItemCount(); i++ )
		if ( !list->topLevelItem( i )->isHidden() )
			n++;
	return n;
}

int CellWorkspacePanel::selectedEntry() const
{
	if ( !list )
		return -1;
	const QList<QTreeWidgetItem *> sel = list->selectedItems();
	if ( sel.isEmpty() )
		return -1;
	return sel.first()->data( 0, Qt::UserRole ).toInt();
}

QString CellWorkspacePanel::censusText() const
{
	return census ? census->text() : QString();
}

QString CellWorkspacePanel::noteText() const
{
	return note ? note->text() : QString();
}

bool CellWorkspacePanel::noteIsRefusal() const
{
	return noteRefusal;
}

int CellWorkspacePanel::legendRows() const
{
	return legend ? legend->topLevelItemCount() : 0;
}

QStringList CellWorkspacePanel::typeFilterNames() const
{
	QStringList out;
	if ( !filterType )
		return out;
	for ( int i = 0; i < filterType->count(); i++ )
		out.append( filterType->itemText( i ) );
	return out;
}

// ---- the list

void CellWorkspacePanel::onSceneChanged()
{
	rebuildList();
	rebuildLegendAndCensus();
	if ( cellPass )	// a Pass needs this document's bake relit
		cellPass->setEnabled( nif && wwCellGiFor( nif ) );
	if ( modPick )	// the open exterior's mods
		modPick->refresh( g_path.isEmpty() || g_spec.interior ? QString() : g_spec.world );
}

CellModPickBox * CellWorkspacePanel::modPicker() const
{
	return modPick;
}

void CellWorkspacePanel::rebuildList()
{
	if ( !list )
		return;
	const bool wasSorting = list->isSortingEnabled();
	list->setSortingEnabled( false );
	QSignalBlocker blockList( list );
	QSignalBlocker blockType( filterType );
	list->clear();

	/* The list is the reference model, not the draw data: one row per REFR, including modelless ones
	 * that never reach the pick table. Joined to pick entries by form id once, here (-1 when undrawn;
	 * an SCOL joins to its first part). */
	const CellPickTable & picks = cellPickTable();
	const CellRefTable & refs = cellRefTable();

	QHash<quint32, int> pickOfForm;
	for ( int i = 0; i < picks.size(); i++ ) {
		const quint32 f = picks.at( i ).refForm;
		if ( f && !pickOfForm.contains( f ) )
			pickOfForm.insert( f, i );
	}

	pickForRef.clear();
	pickForRef.resize( refs.size() );

	QStringList types;
	for ( int i = 0; i < refs.size(); i++ ) {
		const CellRefEntry & e = refs.at( i );
		const int pick = e.refForm ? pickOfForm.value( e.refForm, -1 ) : -1;
		pickForRef[i] = pick;

		const QString type = CellPickTable::typeName( e.baseType );
		const QString cell = refs.labelOfGrid( e.cellX, e.cellY );
		const QString state = CellRefTable::fateName( e.fate );

		QTreeWidgetItem * item = new QTreeWidgetItem( list );
		item->setText( 0, type );
		item->setText( 1, e.baseEdid.isEmpty() ? QStringLiteral( "-" ) : e.baseEdid );
		item->setText( 2, CellPickTable::formName( e.refForm ) );
		/* Sorts as a number; an undrawn reference prints a dash, not 0. */
		if ( pick >= 0 )
			item->setData( 3, Qt::DisplayRole, QVariant( qulonglong( picks.at( pick ).triangles ) ) );
		else
			item->setText( 3, QStringLiteral( "-" ) );
		item->setText( 4, cell );
		item->setText( 5, e.fate == CellRefFate::Drawn ? QString() : state );
		item->setData( 0, Qt::UserRole, i );
		/* The haystack the text filter searches, built once per row. */
		const QString hay = type + QLatin1Char( ' ' ) + e.baseEdid + QLatin1Char( ' ' )
			+ CellPickTable::formName( e.refForm ) + QLatin1Char( ' ' )
			+ CellPickTable::formName( e.baseForm ) + QLatin1Char( ' ' ) + e.model
			+ QLatin1Char( ' ' ) + cell + QLatin1Char( ' ' ) + state;
		item->setData( 1, Qt::UserRole, hay.toLower() );
		if ( !types.contains( type ) )
			types.append( type );
	}

	types.sort();
	const QString keepType = filterType->currentText();
	filterType->clear();
	filterType->addItem( tr( "All" ) );
	filterType->addItems( types );
	const int at = filterType->findText( keepType );
	filterType->setCurrentIndex( at >= 0 ? at : 0 );

	list->setSortingEnabled( wasSorting );
	applyFilter();
}

void CellWorkspacePanel::applyFilter()
{
	if ( !list )
		return;
	const QString type = filterType ? filterType->currentText() : QString();
	const bool allTypes = type.isEmpty() || type == tr( "All" );
	const QString text = filterText ? filterText->text().trimmed().toLower() : QString();

	for ( int i = 0; i < list->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * item = list->topLevelItem( i );
		bool show = allTypes || item->text( 0 ) == type;
		if ( show && !text.isEmpty() )
			show = item->data( 1, Qt::UserRole ).toString().contains( text );
		item->setHidden( !show );
	}
}

void CellWorkspacePanel::setTypeFilter( const QString & type )
{
	if ( !filterType )
		return;
	const int at = type.isEmpty() ? 0 : filterType->findText( type );
	filterType->setCurrentIndex( at >= 0 ? at : 0 );
}

void CellWorkspacePanel::setTextFilter( const QString & text )
{
	if ( filterText )
		filterText->setText( text );
}

void CellWorkspacePanel::filterChanged()
{
	applyFilter();
	const int total = totalRows();
	const int shown = visibleRows();
	if ( total == 0 )
		return;
	say( shown == total
		? tr( "%1 references" ).arg( total )
		: tr( "%1 of %2 references" ).arg( shown ).arg( total ), false );
}

// ---- selection, both directions

void CellWorkspacePanel::rowClicked()
{
	if ( syncing )
		return;
	const int index = selectedEntry();
	if ( index >= 0 )
		selectEntry( index, true );
}

void CellWorkspacePanel::selectEntry( int index, bool frame )
{
	/* Row and viewport clicks both end at `cellPickSelect`, so they cannot differ. */
	const int pick = ( index >= 0 && index < pickForRef.size() ) ? pickForRef.at( index ) : -1;
	if ( pick < 0 ) {
		/* Refuse by name: a light, marker, trigger or deleted record has nothing to select or frame. */
		const CellRefTable & refs = cellRefTable();
		if ( index < 0 || index >= refs.size() )
			say( tr( "no reference %1 in this cell" ).arg( index ), true );
		else
			say( tr( "%1 is not drawn (%2) -- nothing to select in the viewport" )
				.arg( CellPickTable::formName( refs.at( index ).refForm ),
					CellRefTable::fateName( refs.at( index ).fate ) ), true );
		return;
	}
	if ( !cellPickSelect( nif, pick, 1 ) ) {
		say( tr( "no reference %1 in this cell" ).arg( index ), true );
		return;
	}
	if ( frame )
		frameEntry( pick );
}

void CellWorkspacePanel::onPicked( int index, int candidates )
{
	if ( !list )
		return;
	/* `index` is a pick index; the row is found through the same join the list was built with. */
	syncing = true;
	if ( index < 0 ) {
		list->clearSelection();
	} else {
		for ( int i = 0; i < list->topLevelItemCount(); i++ ) {
			QTreeWidgetItem * item = list->topLevelItem( i );
			const int ref = item->data( 0, Qt::UserRole ).toInt();
			if ( ref < 0 || ref >= pickForRef.size() || pickForRef.at( ref ) != index )
				continue;
			list->setCurrentItem( item );
			list->scrollToItem( item );
			break;
		}
	}
	syncing = false;

	if ( index < 0 ) {
		say( tr( "nothing under the cursor" ), false );
		return;
	}
	const CellPickTable & table = cellPickTable();
	if ( index >= table.size() )
		return;
	const CellPickEntry & e = table.at( index );
	const QString name = e.baseEdid.isEmpty()
		? CellPickTable::formName( e.refForm ) : e.baseEdid;
	say( candidates > 1
		? tr( "%1 -- %2 under the cursor" ).arg( name ).arg( candidates )
		: name, false );
}

void CellWorkspacePanel::frameEntry( int index )
{
	if ( !glView )
		return;
	const CellPickTable & table = cellPickTable();
	if ( index < 0 || index >= table.size() )
		return;
	const CellPickEntry & e = table.at( index );

	/* The scene's shapes carry the block origin as Translation, so the camera target is the box
	 * center MINUS that origin (as cellHighlightShow does). */
	float origin[3] = { 0, 0, 0 };
	cellPickOrigin( origin );
	float centre[3];
	float radius = 0.0f;
	for ( int k = 0; k < 3; k++ ) {
		centre[k] = 0.5f * ( e.bmin[k] + e.bmax[k] ) - origin[k];
		radius = std::max( radius, 0.5f * ( e.bmax[k] - e.bmin[k] ) );
	}
	// GLView's Pos is the NEGATED look-at point; setCenter() does the same.
	glView->setPosition( -centre[0], -centre[1], -centre[2] );
	if ( radius > 0.0f )
		glView->setDistance( radius * 3.0f );
}

// ---- the view rows

void CellWorkspacePanel::viewRowChanged()
{
	if ( syncing )
		return;
	if ( g_path.isEmpty() ) {
		say( tr( "no cell is open" ), true );
		return;
	}
	bool known = false;
	const CellOverlay o = cellOverlayFromName(
		overlayBox->currentData().toString(), &known );
	g_over = g_spec;
	g_over.overlay = known ? o : CellOverlay::None;
	for ( int i = 0; i < g_showRowCount && i < showActions.size(); i++ )
		g_over.*( g_showRows[i].field ) = showActions.at( i )->isChecked();
	g_haveOverrides = true;

	/* A rebuild, not a repaint (the overlay is baked into vertex colors); say so before the pause. */
	say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
	emit reopenRequested( g_path );
}

// ---- the legend and the census, both READ from the builder's notes

void CellWorkspacePanel::rebuildLegendAndCensus()
{
	if ( !legend || !census )
		return;

	// the rows follow the spec the cell was actually built from
	syncing = true;
	if ( overlayBox ) {
		const QString name = cellOverlayName( g_spec.overlay );
		const int at = overlayBox->findData( name );
		overlayBox->setCurrentIndex( at >= 0 ? at : 0 );
	}
	/* Show what the cell was BUILT with: the builder may refuse a requested row. */
	for ( int i = 0; i < g_showRowCount && i < showActions.size(); i++ )
		showActions.at( i )->setChecked( g_spec.*( g_showRows[i].field ) );
	syncing = false;

	legend->clear();
	const QVector<LegendRow> rows = legendFromNotes( g_notes );
	for ( const LegendRow & r : rows ) {
		QTreeWidgetItem * item = new QTreeWidgetItem( legend );
		item->setText( 0, r.key );
		item->setData( 1, Qt::DisplayRole, QVariant( r.placements ) );
		item->setText( 2, r.label );
		/* The swatch is the builder's printed rgb, never recomputed here. */
		item->setBackground( 0, QColor::fromRgbF(
			qBound( 0.0f, r.rgb[0], 1.0f ),
			qBound( 0.0f, r.rgb[1], 1.0f ),
			qBound( 0.0f, r.rgb[2], 1.0f ) ) );
	}

	/* The census counts the plugin's references, not the drawn ones, same as the list. */
	const CellRefTable & refModel = cellRefTable();
	const int refs = refModel.size();
	if ( g_path.isEmpty() && refs == 0 ) {
		census->setText( QString() );
		say( tr( "no cell is open" ), true );
		rebuildProbeKinds();
		return;
	}

	qint64 verts = 0, tris = 0, shapes = 0;
	const bool haveGeom = geometryFromNotes( g_notes, verts, tris, shapes );
	census->setText( haveGeom
		? tr( "%1 references  -  %2 triangles  -  %3 vertices in %4 shapes" )
			.arg( formatCount( refs ) ).arg( formatCount( tris ) )
			.arg( formatCount( verts ) ).arg( formatCount( shapes ) )
		/* A refusal, not a zero: unparsed notes say nothing about the cell. */
		: tr( "%1 references  -  the builder's notes did not carry a geometry line" )
			.arg( formatCount( refs ) ) );

	const QString refusal = refusalFromNotes( g_notes );
	if ( !refusal.isEmpty() )
		say( refusal, true );
	else
		say( tr( "%1 references" ).arg( refs ), false );
	rebuildProbeKinds();
}

// ---- the PRTP band

void CellWorkspacePanel::placeProbes()
{
	if ( g_path.isEmpty() ) {
		say( tr( "no cell is open" ), true );
		return;
	}
	g_over = g_spec;
	g_over.probes = true;
	g_over.probesShow = probesShow && probesShow->isChecked();
	g_haveOverrides = true;
	say( tr( "placing probes in %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
	emit reopenRequested( g_path );
}

void CellWorkspacePanel::bakeProbes()
{
	if ( g_path.isEmpty() ) {
		say( tr( "no cell is open" ), true );
		return;
	}
	g_over = g_spec;
	g_over.probes = true;
	g_over.probesBake = true;
	g_over.probesShow = probesShow && probesShow->isChecked();
	g_haveOverrides = true;
	say( tr( "baking probes in %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
	emit reopenRequested( g_path );
}

void CellWorkspacePanel::setProbesShown( bool on )
{
	if ( probesShow )
		probesShow->setChecked( on );
}

bool CellWorkspacePanel::probesShown() const
{
	return probesShow && probesShow->isChecked();
}

QStringList CellWorkspacePanel::probeKindRows() const
{
	QStringList out;
	if ( !probeKinds || probeKinds->isHidden() )
		return out;
	for ( int i = 0; i < probeKinds->topLevelItemCount(); i++ )
		out << probeKinds->topLevelItem( i )->text( 0 ) + QLatin1Char( ' ' )
			+ probeKinds->topLevelItem( i )->data( 1, Qt::DisplayRole ).toString();
	return out;
}

/* The counts are read from the placer's census lines in the builder's notes, like the legend. */
void CellWorkspacePanel::rebuildProbeKinds()
{
	if ( !probeKinds )
		return;
	probeKinds->clear();
	static const QRegularExpression reAll( QStringLiteral(
		"probes: (\\d+) = first-hit (\\d+) \\+ interior (\\d+) \\+ wall (\\d+) \\+ aperture \\d+ \\+ room (\\d+) \\+ cover (\\d+)" ) );
	static const QRegularExpression reOpen( QStringLiteral(
		"openings: doorway (\\d+), window (\\d+), breach (\\d+)" ) );
	static const QRegularExpression reRefused( QStringLiteral( "probes REFUSED: ([^\\n]*)" ) );
	const QRegularExpressionMatch ma = reAll.match( g_notes ), mo = reOpen.match( g_notes );
	const QRegularExpressionMatch mr = reRefused.match( g_notes );
	if ( mr.hasMatch() )
		say( tr( "probes refused: %1" ).arg( mr.captured( 1 ).trimmed() ), true );
	if ( !ma.hasMatch() || !mo.hasMatch() ) {
		probeKinds->setVisible( false );
		return;
	}
	// in cellProbeKinds() order: first hit, interior, wall, doorway, window, breach, room, cover
	const int counts[8] = { ma.captured( 2 ).toInt(), ma.captured( 3 ).toInt(), ma.captured( 4 ).toInt(),
		mo.captured( 1 ).toInt(), mo.captured( 2 ).toInt(), mo.captured( 3 ).toInt(),
		ma.captured( 5 ).toInt(), ma.captured( 6 ).toInt() };
	int nk = 0;
	const CellProbeKind * kinds = cellProbeKinds( &nk );
	for ( int k = 0; k < nk && k < 8; k++ ) {
		QTreeWidgetItem * item = new QTreeWidgetItem( probeKinds );
		item->setText( 0, tr( kinds[k].name ) );
		item->setData( 1, Qt::DisplayRole, QVariant( counts[k] ) );
		QPixmap sw( 12, 12 );
		sw.fill( QColor::fromRgbF( kinds[k].rgb[0], kinds[k].rgb[1], kinds[k].rgb[2] ) );
		item->setIcon( 0, QIcon( sw ) );
	}
	QTreeWidgetItem * total = new QTreeWidgetItem( probeKinds );
	total->setText( 0, tr( "All" ) );
	total->setData( 1, Qt::DisplayRole, QVariant( ma.captured( 1 ).toInt() ) );
	probeKinds->setVisible( true );
	// what the bake wrote, read from its own census lines
	static const QRegularExpression reBake( QStringLiteral(
		"bake: surfels (\\d+), links (\\d+),[^\\n]*sectors (\\d+)" ) );
	static const QRegularExpression reDir( QStringLiteral( "bake folder ([^\\n]*)" ) );
	static const QRegularExpression reBakeRefused( QStringLiteral( "bake REFUSED: ([^\\n]*)" ) );
	const QRegularExpressionMatch mb = reBake.match( g_notes ), md = reDir.match( g_notes );
	const QRegularExpressionMatch mx = reBakeRefused.match( g_notes );
	if ( mx.hasMatch() )
		say( tr( "bake refused: %1" ).arg( mx.captured( 1 ).trimmed() ), true );
	else if ( mb.hasMatch() && md.hasMatch() )
		say( tr( "baked %1 files, %2 surfels, %3 links: %4" ).arg( mb.captured( 3 ), mb.captured( 1 ),
			mb.captured( 2 ), md.captured( 1 ).trimmed() ), false );
}

void CellWorkspacePanel::say( const QString & text, bool refusal )
{
	if ( !note )
		return;
	noteRefusal = refusal;
	note->setText( text );
	note->setStyleSheet( refusal
		? QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) )
		: QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
}
