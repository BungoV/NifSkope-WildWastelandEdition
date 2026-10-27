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
#include "cellsplat.h"
#include "cellgroundlegend.h"	// lane DUNESAND3
#include "cellfarlod.h"
#include "cellmodpickbox.h"	// lane MODPICK1: the Mods band
#include "gl/celllights.h"
#include "gl/cellcull.h"	// lane SUNCELL1: the Culling row
#include "gl/cellssr.h"	// lane CELLALL1: the SSR rows
#include "gl/cellreflprobe.h"	// lane REFLPROBE1: the Reflection probes row
#include "gl/cellssgi.h"	// lane SSGI1
#include "gl/cellfarground.h"	// lane FARDRAW1: the Far ground rows
#include "gl/celllampshadow.h"	// lane LAMPSHADOW1
#include "gl/celllooks.h"	// lane GAPLOOK1
#include "gl/cellwater.h"	// lane CELLALL1: the Water row
#include "cellaodecal.h"	// lane CELLALL1: the AO decals row
#include "gl/gametaa.h"
#include "gl/cellgrass.h"	// lane GRASSMB1
#include "gl/cellpost.h"	// lane GRASSMB1
#include "gl/cellbloom.h"	// lane BLOOM1b
#include "gl/cellfogbloom.h"
#include "ui/widgets/wwnumberfield.h"	// lane BLOOM1b: the Bloom rows
#include "ui/opencelldialog.h"	// lane OPENCELLFIX1: the Load order and Cells tabs
#include "ui/cellshowpanel.h"	// lane OPENCELLFIX1: the Show tab, the Show menu stays open
#include "ui/widgets/wwdropdown.h"	// lane OPENCELLFIX2: the dock's one dropdown
#include "gl/cellvolair.h"	// lane OPENCELLFIX2: the fog group's rows (notes 22)
#include "gl/lookdevstage.h"
#include "gl/shadowset.h"	// lane OPENCELLFIX2: the shadow group's Scene window rows (note 26)
#include "wwswitch.h"
#include "cellmodpick.h"

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
#include <QDoubleSpinBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
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
	/* ONLY THE FIELDS A ROW OWNS. The worldspace, the cell, the block size, the
	 * plugins and the data root come from the file and are not the panel's to
	 * change -- a row that silently re-pointed the view at another cell would
	 * make the `.wwcell` a lie. */
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
	// lane PRTPBAKE: a bake is one click, not a mode -- the next rebuild does not bake again
	g_spec.probesBake = false;
	g_over.probesBake = false;
	if ( !g_haveOverrides ) {
		// The first cell of the session seeds the rows from the file itself, so
		// a row shows what is actually drawn before anybody touches one.
		g_over = spec;
	}
	/* THE PANEL READS THE NEW NOTES NOW (lane PRTPPLACE, 2026-09-30). The builder's
	 * own sceneChanged fires inside nifCreateCellScene, BEFORE this stores its notes,
	 * so after a rebuild the legend, census and PRTP counts showed the build
	 * before it. A second ring once the notes are stored. */
	emit CellPickBus::instance()->sceneChanged();
}

QString cellWorkspacePath()
{
	return g_path;
}

QString cellWorkspaceNotesLive()
{
	const bool nospec = cellSplatSpecRed();
	const QString now = cellGroundSpecClauseLive( g_notes, nospec );
	if ( now == g_notes )
		return QString();
	g_notes = now;
	emit CellPickBus::instance()->sceneChanged();
	return nospec ? QStringLiteral( "ground legend: RED CONTROL nospec" ) : QStringLiteral( "ground legend: TX07 bound" );
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
 *
 * The format is src/cellview.cpp's, and this is a READ of it. If the builder's
 * line changes shape this returns nothing and the legend band says so, which is
 * a visible refusal rather than a panel quietly inventing its own colours. */
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

/* ---------------------------------------------------------------------------
 * THE SHOW TABLE (bungo 2026-09-19: "And an option to show cell borders too, a
 * lot of visual / visibility toggles that will only live in the cell editor
 * workspace, that are specific to cell stuff").
 *
 * ONE ROW PER TOGGLE, and a later lane adds a toggle by adding a row here --
 * not by adding a widget, a member, a connect and a settings key in four
 * different places, which is how five unrelated checkboxes become six
 * disagreeing ones.
 *
 * `id` is BOTH the QSettings key inside the group and the object-name suffix,
 * so what a gate looks for and what is persisted cannot drift apart.
 *
 * SHIPPED ROWS ARE ONLY THE ONES THE BUILDER ALREADY HAS A FIELD FOR. bungo
 * also named Objects, Bare-ground quads highlight and a Cell name label; none
 * of the three has anything behind it in `CellSceneSpec`, so shipping a row for
 * them would be a control that does nothing. They are in the report's NEXT list
 * with what each needs, which is the honest place for them.
 * --------------------------------------------------------------------------- */
struct CellShowRow
{
	const char * id;
	const char * label;
	bool def;
	//! Which `CellSceneSpec` bool this row is. There is no row without one.
	bool CellSceneSpec::* field;
};

static const CellShowRow g_showRows[] = {
	/* "Cell borders" is bungo's name for what the builder has always called
	 * `grid`: the lines at the 4096-unit cell edges. It is renamed in the UI and
	 * NOT in the spec, because the spec's name is in `.wwcell` files that exist. */
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
	/* lane OPENCELLFIX1 (bungo 10-27: "So this whole thing needs a redo", "logical for modders to use"): the dock is
	 * tabs on the block list's strip, in the order a modder works: Load order, Cells, References, Show. The bands
	 * below keep their widgets; `page` points at the tab each band now lives in. */
	QVBoxLayout * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	outer->setSpacing( 0 );
	tabs = new QTabBar( this );
	tabs->setObjectName( QStringLiteral( "CellWorkspaceTabs" ) );
	tabs->setDocumentMode( true );
	tabs->setDrawBase( false );
	tabs->setExpanding( true );
	tabs->setUsesScrollButtons( false );
	tabs->setStyleSheet( wwSegmentedTabBarQss() );
	tabStack = new QStackedWidget( this );
	tabStack->setObjectName( QStringLiteral( "CellWorkspaceStack" ) );
	auto newPage = []( const char * id ) {
		auto * w = new QWidget;
		w->setObjectName( QStringLiteral( "CellWorkspaceTab_" ) + QLatin1String( id ) );
		auto * l = new QVBoxLayout( w );
		l->setContentsMargins( 6, 6, 6, 6 );
		l->setSpacing( 4 );
		return l;
	};
	auto addTab = [this]( const QString & name, const char * id, QWidget * w ) {
		const int t = tabs->addTab( name );
		tabs->setTabData( t, QString::fromLatin1( id ) );
		tabStack->addWidget( w );
	};
	QVBoxLayout * loL = newPage( "loadorder" );
	QVBoxLayout * cellsL = newPage( "cells" );
	QVBoxLayout * refsL = newPage( "references" );
	QVBoxLayout * showL = newPage( "show" );
	auto * showScroll = new QScrollArea;
	showScroll->setObjectName( QStringLiteral( "CellWorkspaceShowScroll" ) );
	showScroll->setWidgetResizable( true );
	showScroll->setFrameShape( QFrame::NoFrame );
	showScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	showScroll->setWidget( showL->parentWidget() );
	addTab( tr( "Load order" ), "loadorder", loL->parentWidget() );
	addTab( tr( "Cells" ), "cells", cellsL->parentWidget() );
	addTab( tr( "References" ), "references", refsL->parentWidget() );
	addTab( tr( "Display" ), "show", showScroll );	// lane OPENCELLFIX2: renamed; the id stays (saved tab)
	outer->addWidget( tabs, 0 );
	outer->addWidget( tabStack, 1 );

	picker = new OpenCellPicker( this );
	picker->hide();
	loL->addWidget( picker->loadOrderPage(), 1 );
	cellsL->addWidget( picker->cellsPage(), 1 );
	connect( picker, &OpenCellPicker::openRequested, this, [this]( const QString & path ) {
		/* The tabs' load order is the one the cell loads: a Mods choice saved for this worldspace (perhaps from a
		 * load order that has since changed) would otherwise override it. */
		CellSceneSpec s;
		if ( cellSpecFromFile( path, s, nullptr ) && !s.interior && !s.world.isEmpty() && cellModPickTouched( s.world ) )
			cellModPickReset( s.world );
		emit openCellRequested( path );
	} );

	auto box = []( QVBoxLayout * into, const char * name, int stretch ) {
		auto * w = new QWidget;
		w->setObjectName( QLatin1String( name ) );
		auto * l = new QVBoxLayout( w );
		l->setContentsMargins( 0, 0, 0, 0 );
		l->setSpacing( 4 );
		into->addWidget( w, stretch );
		return l;
	};
	QVBoxLayout * refsLay = box( refsL, "CellWorkspaceReferences", 1 );
	refsBox = refsLay->parentWidget();
	QVBoxLayout * page = refsLay;

	QWidget * filterRow = new QWidget( this );
	QHBoxLayout * fl = new QHBoxLayout( filterRow );
	fl->setContentsMargins( 0, 0, 0, 0 );
	fl->setSpacing( 4 );
	filterType = new WwDropdown( filterRow );
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
	/* SIX COLUMNS, ONE ROW PER PLACED REFERENCE.
	 *
	 * DIVERGENCE FROM BLENDER AND FROM THE CREATION KIT, stated because it was a
	 * choice: both would NEST -- Blender's outliner nests by collection, the CK
	 * groups by cell. This list is FLAT with a Cell column, because what people
	 * do with it is sort and filter (what is heavy, what is a marker, where is
	 * form 0x000ABCDE), and nesting makes sorting lie: a tree sorts inside each
	 * parent, so "heaviest first" is no longer the list's order. Grouping
	 * arrives when there are enough cells for it to save more than it costs; the
	 * model already carries the cell list it would group by.
	 *
	 * `State` is the builder's own word for why a reference is not on screen
	 * (`CellRefTable::fateName`), so a light, a sound marker and a deleted record
	 * are all IN THE LIST -- which is the point of listing what exists rather
	 * than what was drawn. */
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

	// ---- band 2: the view (the Show tab)
	page = showL;

	/* lane OPENCELLFIX2: Color is the Display tab's first Editor row (Textured = its tick off), and the Show button
	 * that sat beside it is gone (bungo: "there is a redundant show on the right"). WW_CELLSHOW_RED=showbutton keeps it. */
	overlayBox = new WwDropdown( this );
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

	/* THE SHOW MENU -- a child of this panel, so it lives only in the Cell workspace. Since OPENCELLFIX2 nothing pops
	 * it: the Display tab is built from it and its actions stay the one truth. */
	QToolButton * showButton = nullptr;
	if ( wwEnvStr( "WW_CELLSHOW_RED" ) == QLatin1String( "showbutton" ) ) {
		showButton = new QToolButton( this );
		showButton->setObjectName( QStringLiteral( "CellWorkspaceShowButton" ) );
		showButton->setText( tr( "Show" ) );
		WwDropdown::applyStyle( showButton );
		page->addWidget( showButton );
	}
	QMenu * showMenu = new QMenu( this );
	showMenu->setObjectName( QStringLiteral( "CellWorkspaceShowMenu" ) );

	/* LANE SHOWMENU1 (bungo 10-12, "Tidy that up"): the Show dropdown is the ONE home of every
	 * viewport feature switch, in three sections. Each tick keeps the key, default, objectName and
	 * live path its old checkbox had; Water is one tick (the Show row), which also drives the
	 * water shader switch so the two can no longer disagree. */
	auto tick = [&]( QMenu * m, const QString & label, const char * name, bool on ) {
		QAction * a = m->addAction( label );
		a->setObjectName( QLatin1String( name ) );
		a->setCheckable( true );
		a->setChecked( on );
		return a;
	};
	/* A heading row per section. QMenu::addSection's text is not drawn by this style (measured: a bare
	 * line), so the heading is the panel's own band heading in a widget action, after a separator. */
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
				wwCellWaterSetOn( on );	// SHOWMENU1: the one Water tick
			viewRowChanged();
		} );
		showActions.append( a );
	}
	cellGrass = tick( showMenu, tr( "Grass" ), "CellWorkspaceCellGrass", wwCellGrassOn() );
	cellParticles = tick( showMenu, tr( "Particles" ), "CellWorkspaceParticles", wwCellParticlesOn() );
	cellPrevis = tick( showMenu, tr( "Previs" ), "CellWorkspacePrevis", wwCellPrevisOn() );
	cellCull = tick( showMenu, tr( "Culling" ), "CellWorkspaceCulling", wwCellCullOn() );
	cellFarLod = tick( showMenu, tr( "Far LOD" ), "CellWorkspaceFarLod", cellFarLodWanted() );
	// lane FARLOD1: the LOD type -- Vanilla | FO4CS; unset, it shows what the last build resolved to
	cellFarLodType = showMenu->addMenu( tr( "Far LOD source" ) );
	cellFarLodType->setObjectName( QStringLiteral( "CellWorkspaceFarLodType" ) );
	cellFarLodTypeGroup = new QActionGroup( cellFarLodType );
	cellFarLodTypeGroup->setExclusive( true );
	{
		const int t = cellFarLodTypePinned() != WwFarLodAuto ? cellFarLodTypePinned() : cellFarLodLastType();
		const QStringList names = { tr( "Vanilla" ), tr( "Fusion Shaders" ) };	// note 21: the full name
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellFarLodType->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == ( t == WwFarLodVanilla ? 0 : 1 ) );
			cellFarLodTypeGroup->addAction( a );
		}
	}
	// lane FARDRAW1: ring 0's ground -- the baked sheet (today), live layers + shader decals, or live layers + cached pages
	cellFarGround = showMenu->addMenu( tr( "Far ground" ) );
	cellFarGround->setObjectName( QStringLiteral( "CellWorkspaceFarGround" ) );
	cellFarGround->setEnabled( !CellFarGround::modePinned() );
	cellFarGroundGroup = new QActionGroup( cellFarGround );
	cellFarGroundGroup->setExclusive( true );
	{
		const QStringList names = { tr( "Baked" ), tr( "Live, shader decals" ), tr( "Live, cached pages" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellFarGround->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == CellFarGround::mode() );
			cellFarGroundGroup->addAction( a );
		}
	}
	cellFarGroundQ = showMenu->addMenu( tr( "Far ground pages" ) );
	cellFarGroundQ->setObjectName( QStringLiteral( "CellWorkspaceFarGroundPages" ) );
	cellFarGroundQ->setEnabled( !CellFarGround::qualityPinned() );
	cellFarGroundQGroup = new QActionGroup( cellFarGroundQ );
	cellFarGroundQGroup->setExclusive( true );
	{
		const QStringList names = { tr( "Low" ), tr( "Medium" ), tr( "High" ) };	// FARPAGES1: Ultra dropped
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellFarGroundQ->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == CellFarGround::quality() );
			cellFarGroundQGroup->addAction( a );
		}
	}
	section( tr( "Lighting" ), "lighting" );
	cellLights = tick( showMenu, tr( "Cell lights" ), "CellWorkspaceCellLights", wwCellLightsOn() );
	cellGi = tick( showMenu, tr( "Precomputed Radiance Transfer" ), "CellWorkspaceCellGi", wwCellGiOn() );	// bungo 10-27 note 12: the label only
	/* lane SKYSTR1 (bungo 10-13, FO4CS's Skylighting strength): how much the GI's sky visibility darkens, 0-100 %.
	 * A label | field row as the Bloom rows; live (the grid relights 0.4 s after the value stands still) */
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
	// lane PROBEVIEW1: the Pass (Division deck s18/s40/s65), live once a bake is relit
	cellPass = showMenu->addMenu( tr( "Debug view" ) );	// bungo 10-27 note 13: under Precomputed Radiance Transfer
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
	cellFlicker = tick( showMenu, tr( "Light flicker" ), "CellWorkspaceLightFlicker", wwCellFlickerOn() );
	// lane LAMPSHADOW1: stored lamp shadows, ships Off; the tier is the face size (Medium = the cube pass's own)
	cellLampShadow = showMenu->addMenu( tr( "Cached lamp shadows" ) );	// note 16: label only
	cellLampShadow->setObjectName( QStringLiteral( "CellWorkspaceLampShadows" ) );
	cellLampShadowGroup = new QActionGroup( cellLampShadow );
	cellLampShadowGroup->setExclusive( true );
	{
		const QStringList names = { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellLampShadow->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k - 1 );
			a->setChecked( k - 1 == wwCellLampShadowTier() );
			cellLampShadowGroup->addAction( a );
		}
	}
	cellLampShadow->setEnabled( !wwCellLampShadowPinned() );
	// lane GAPLOOK1 (LIGHTSYS-3): the lamp-shadow budget, ships Off (the 16 nearest, as before)
	cellShadowBudget = showMenu->addMenu( tr( "Lamp shadow budget" ) );
	cellShadowBudget->setObjectName( QStringLiteral( "CellWorkspaceShadowBudget" ) );
	cellShadowBudgetGroup = new QActionGroup( cellShadowBudget );
	cellShadowBudgetGroup->setExclusive( true );
	{
		const int vals[] = { 0, 4, 8, 16 };
		for ( int v : vals ) {
			QAction * a = cellShadowBudget->addAction( v ? QString::number( v ) : tr( "Off" ) );
			a->setCheckable( true );
			a->setData( v );
			a->setChecked( v == wwCellShadowBudget() );
			cellShadowBudgetGroup->addAction( a );
		}
	}
	cellShadowBudget->setEnabled( !wwCellShadowBudgetPinned() );
	// lane LIGHTPHYS1: the light intensity curve, ships Off (curve 1)
	cellLightCurve = showMenu->addMenu( tr( "Lamp brightness contrast" ) );	// note 17: label only
	cellLightCurve->setObjectName( QStringLiteral( "CellWorkspaceLightCurve" ) );
	cellLightCurveGroup = new QActionGroup( cellLightCurve );
	cellLightCurveGroup->setExclusive( true );
	{
		const double vals[] = { 1.0, 0.75, 1.25, 1.5, 2.0 };
		for ( double v : vals ) {
			QAction * a = cellLightCurve->addAction( v == 1.0 ? tr( "Off" ) : QString::number( v ) );
			a->setCheckable( true );
			a->setData( v );
			a->setChecked( float( v ) == wwCellLightCurve() );
			cellLightCurveGroup->addAction( a );
		}
	}
	cellLightCurve->setEnabled( !wwCellLightCurvePinned() );
	// lane LIGHTPHYS3: the Advanced light model's calibration, ships 1 (FO4CS's default)
	cellLightCalib = showMenu->addMenu( tr( "Calibration" ) );	// note 25: under Advanced model
	cellLightCalib->setObjectName( QStringLiteral( "CellWorkspaceLightCalibration" ) );
	cellLightCalibGroup = new QActionGroup( cellLightCalib );
	cellLightCalibGroup->setExclusive( true );
	{
		const double vals[] = { 0.5, 0.75, 1.0, 1.5, 2.0 };
		for ( double v : vals ) {
			QAction * a = cellLightCalib->addAction( QString::number( v ) );
			a->setCheckable( true );
			a->setData( v );
			a->setChecked( float( v ) == wwCellLightCalibration() );
			cellLightCalibGroup->addAction( a );
		}
	}
	cellLightCalib->setEnabled( !wwCellLightCalibrationPinned() );
	// lane GAPLOOK1: the gap sweep's look ticks, each Off by default
	for ( int k = 0; k < int( WwLook::Count ); k++ ) {
		const WwLook l = WwLook( k );
		if ( !wwLookHasRow( l ) )
			continue;	// lane FOGPERF1: a speed switch rides with its feature, no row
		QAction * a = tick( showMenu, tr( wwLookLabel( l ) ),
			( QByteArray( "CellWorkspaceLook_" ) + wwLookPinName( l ) ).constData(), wwLookOn( l ) );
		a->setEnabled( !wwLookPinned( l ) );
		a->setData( k );
		lookActions.append( a );
	}
	cellAo = tick( showMenu, tr( "Ambient Occlusion" ), "CellWorkspaceAo", wwCellAoOn() );	// note 25: no abbreviations
	{	// lane OPENCELLFIX2 item 27 (lane SSAO2's methods): inline on the Ambient Occlusion row; a pin greys it
		QMenu * m = showMenu->addMenu( tr( "Ambient Occlusion method" ) );
		m->setObjectName( QStringLiteral( "CellWorkspaceAoMethod" ) );
		auto * grp = new QActionGroup( m );
		grp->setExclusive( true );
		const QStringList names = { tr( "Vanilla Screen-Space Ambient Occlusion" ), tr( "Ground Truth Ambient Occlusion" ),
			tr( "Intel Xe Ground Truth Ambient Occlusion" ), tr( "Horizon-Based Ambient Occlusion Plus" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = m->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == wwCellAoMethod() );
			grp->addAction( a );
		}
		m->menuAction()->setEnabled( !wwCellAoMethodPinned() );
		connect( grp, &QActionGroup::triggered, this, [this]( QAction * a ) {
			wwCellAoSetMethod( a->data().toInt() );	// WW/CellAoMethod
			if ( glView )
				glView->update();
		} );
	}
	cellAoDecal = tick( showMenu, tr( "Ambient occlusion decals" ), "CellWorkspaceAoDecals", aoDecalOn() );
	cellSpecOcc = tick( showMenu, tr( "Sky specular occlusion" ), "CellWorkspaceSkySpecOcc", wwCellSpecOccOn() );	// lane SPECOCC1
	cellSpecOcc->setEnabled( !wwCellSpecOccPinned() );
	cellGiGpu = tick( showMenu, tr( "GPU relight" ), "CellWorkspaceGpuRelight", wwCellGiGpuOn() );
	cellEmissiveGrid = tick( showMenu, tr( "Emissive GI grid" ), "CellWorkspaceEmissiveGiGrid", wwCellEmissiveGridOn() );	// lane EMISSIVEGRID1
	section( tr( "Effects" ), "effects" );
	cellIs = tick( showMenu, tr( "Imagespace" ), "CellWorkspaceCellImageSpace", wwCellImageSpaceOn() );
	// lane MOTION1: the game's temporal AA, the same state as the Scene popup's Temporal AA row; ships off
	cellTaa = tick( showMenu, tr( "Temporal AA" ), "CellWorkspaceTemporalAa", wwGameTaaRowOn() );
	// lane TAA2: our own resolve in place of the game's, Off | Low | Medium | High | Ultra; ships Off
	cellModernTaa = showMenu->addMenu( tr( "Improved Temporal Anti-Aliasing" ) );	// note 19: label only
	cellModernTaa->setObjectName( QStringLiteral( "CellWorkspaceModernTaa" ) );
	cellModernTaa->setEnabled( !wwTaa2Pinned() );
	cellModernTaaGroup = new QActionGroup( cellModernTaa );
	cellModernTaaGroup->setExclusive( true );
	{
		const QStringList names = { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellModernTaa->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == wwTaa2Tier() );
			cellModernTaaGroup->addAction( a );
		}
	}
	// lane TAA3: the Modern resolve on the linear frame, before the tone map; ships Off
	cellModernTaaHdr = tick( showMenu, tr( "Before tone map" ), "CellWorkspaceModernTaaHdr", wwTaa2HdrOn() );
	cellSsr = tick( showMenu, tr( "Screen-Space Reflections" ), "CellWorkspaceSsr", wwCellSsrOn() );	// note 15: labels only
	if ( wwCellSsrExteriorOn() != wwCellSsrOn() )
		wwCellSsrRowSet( wwCellSsrOn() );	// note 20: one row sets both halves; an old split takes the row's state
	cellReflProbe = tick( showMenu, tr( "Reflection probes" ), "CellWorkspaceReflProbe", wwCellReflProbeOn() );	// lane REFLPROBE1
	// lane SSGI1: the screen-space bounce, ships off; its quality is FO4CS's four tiers
	cellSsgi = tick( showMenu, tr( "Screen-Space Global Illumination" ), "CellWorkspaceSsgi", wwCellSsgiOn() );	// note 14: label only
	cellSsgi->setEnabled( !wwCellSsgiPinned() );
	cellSsgiQ = showMenu->addMenu( tr( "Screen-Space Global Illumination quality" ) );
	cellSsgiQ->setObjectName( QStringLiteral( "CellWorkspaceSsgiQuality" ) );
	cellSsgiQGroup = new QActionGroup( cellSsgiQ );
	cellSsgiQGroup->setExclusive( true );
	{
		const QStringList names = { tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = cellSsgiQ->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == wwCellSsgiQuality() );
			cellSsgiQGroup->addAction( a );
		}
	}
	cellDof = tick( showMenu, tr( "Depth of field" ), "CellWorkspaceCellDof", wwCellDofOn() );
	cellMblur = tick( showMenu, tr( "Motion blur" ), "CellWorkspaceCellMotionBlur", wwCellMotionBlurOn() );
	// lane BLOOM1b: the bloom -- Vanilla (the imagespace's own, the default) | FO4CS, then FO4CS's settings, one per
	// row, and its physical glare. Every change applies on the next paint (gl/cellbloom.h)
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
		// lane FOGBLOOM1: the bloom each pixel takes rises with the fog along its view ray (gl/cellfogbloom.h; ships off)
		QAction * fogBloom = cellBloom->addAction( tr( "Fog bloom" ) );
		fogBloom->setObjectName( QStringLiteral( "CellWorkspaceBloomFog" ) );
		fogBloom->setCheckable( true );
		fogBloom->setChecked( wwCellFogBloomOn() );
		fogBloom->setEnabled( !wwCellFogBloomPinned() );
		fogBloom->setToolTip( tr( "Glow spreads stronger through fog and from glowing glass and effects; none in clear air" ) );
		connect( fogBloom, &QAction::toggled, this, [this]( bool on ) {
			wwCellFogBloomSetOn( on );
			if ( glView )
				glView->update();
		} );
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
	/* lane OPENCELLFIX2 (bungo 10-27 notes 17-26): the Display groups' own ticks -- a master per new or split group
	 * (gl/celllooks.h WwGroup) -- and the Scene window's fog and shadow settings, one state with its rows: each side
	 * says wwLiveChanged( topic ) and re-reads on the other's word. */
	QMenu * groupsMenu = showMenu->addMenu( tr( "Groups" ) );
	groupsMenu->setObjectName( QStringLiteral( "CellWorkspaceGroups" ) );
	for ( int k = 0; k < int( WwGroup::Count ); k++ ) {
		const WwGroup g = WwGroup( k );
		QAction * a = tick( groupsMenu, tr( wwGroupLabel( g ) ),
			( QByteArray( "CellWorkspaceGroup_" ) + wwGroupName( g ) ).constData(), wwGroupOn( g ) );
		a->setData( k );
	}
	auto liveTick = [&]( const char * name, const QString & label, const char * topic, bool ( *get )(), void ( *set )( bool ) ) {
		QAction * a = tick( groupsMenu, label, name, get() );
		const QString t = QLatin1String( topic );
		connect( a, &QAction::toggled, this, [this, set, t]( bool on ) {
			set( on );
			wwLiveChanged( t );
			if ( glView )
				glView->update();
		} );
		wwLiveWatch( a, t, [a, get]() {
			if ( a->isChecked() != get() )
				a->setChecked( get() );
		} );
	};
	auto liveTier = [&]( const char * name, const QString & label, const char * topic, int ( *get )(), void ( *set )( int ) ) {
		QMenu * m = groupsMenu->addMenu( label );
		m->setObjectName( QLatin1String( name ) );
		auto * grp = new QActionGroup( m );
		grp->setExclusive( true );
		const QStringList names = { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) };
		for ( int k = 0; k < names.size(); k++ ) {
			QAction * a = m->addAction( names.at( k ) );
			a->setCheckable( true );
			a->setData( k );
			a->setChecked( k == get() );
			grp->addAction( a );
		}
		const QString t = QLatin1String( topic );
		connect( grp, &QActionGroup::triggered, this, [this, set, t]( QAction * a ) {
			set( a->data().toInt() );
			wwLiveChanged( t );
			if ( glView )
				glView->update();
		} );
		wwLiveWatch( m, t, [grp, get]() {
			for ( QAction * a : grp->actions() )
				if ( a->data().toInt() == get() && !a->isChecked() )
					a->setChecked( true );
		} );
	};
	liveTick( "CellWorkspaceVolAir", tr( "Improved Volumetric Fog" ), "volAir", &wwVolAirOn, &wwVolAirSetOn );
	liveTier( "CellWorkspaceFarAir", tr( "Far air" ), "farAir", &wwVolAirFarTier, &wwVolAirSetFarTier );
	liveTick( "CellWorkspaceVolPart", tr( "Particles" ), "volPart", &wwVolPartOn, &wwVolPartSetOn );
	liveTick( "CellWorkspaceFogDrift", tr( "Drift" ), "fogDrift", &wwVolAirDriftOn, &wwVolAirSetDriftOn );
	liveTick( "CellWorkspaceCascadedShadows", tr( "Cascaded shadows" ), "lookdevShadows", &wwLookdevShadows, &wwLookdevSetShadows );
	liveTier( "CellWorkspaceCascadeQuality", tr( "Cascade quality" ), "cascadeQuality", &wwShadowCascadeTier, &wwShadowSetCascadeTier );
	{	// the cascades' reach, a label | field row folded under Cascaded shadows
		QWidget * panel = new QWidget( groupsMenu );
		panel->setObjectName( QStringLiteral( "CellWorkspaceShadowDistancePanel" ) );
		QGridLayout * gl = new QGridLayout( panel );
		gl->setContentsMargins( 8, 2, 8, 2 );
		WwNumberField * f = new WwNumberField( panel );
		f->setObjectName( QStringLiteral( "CellWorkspaceShadowDistance" ) );
		f->setRange( 2000.0, 24000.0 );
		f->setSingleStep( 500.0 );
		f->setDecimals( 0 );
		f->setValue( wwShadowDistance() );
		gl->addWidget( new QLabel( tr( "Distance" ), panel ), 0, 0 );
		gl->addWidget( f, 0, 1 );
		connect( f, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this]( double v ) {
			wwShadowSetDistance( v );
			wwLiveChanged( QStringLiteral( "shadowDistance" ) );
			if ( glView )
				glView->update();
		} );
		wwLiveWatch( f, QStringLiteral( "shadowDistance" ), [f]() {
			if ( f->value() != wwShadowDistance() )
				f->setValue( wwShadowDistance() );
		} );
		auto * wa = new QWidgetAction( groupsMenu );
		wa->setDefaultWidget( panel );
		wa->setText( tr( "Distance" ) );
		wa->setObjectName( QStringLiteral( "CellWorkspaceShadowDistanceRow" ) );
		groupsMenu->addAction( wa );
	}
	liveTier( "CellWorkspaceScreenSpaceShadows", tr( "Screen-space shadows" ), "contactShadows", &wwShadowContactTier,
		&wwShadowSetContactTier );
	liveTier( "CellWorkspaceTerrainShadows", tr( "Terrain shadows" ), "terrainShadows", &wwShadowTerrainTier,
		&wwShadowSetTerrainTier );
	liveTick( "CellWorkspaceShadowRules", tr( "Fusion Shaders shadow rules" ), "shadowPar",
		[]() { return wwShadowParMask() != 0; }, []( bool on ) { wwShadowSetParMask( on ? WwParAll : 0 ); } );
	if ( showButton )
		showButton->setMenu( showMenu );
	/* lane OPENCELLFIX1 (bungo 10-27: "For show, don't close it when I click on one thing", "Maybe make that a sub
	 * category with checkmarks"): a tick leaves the menu open, and the Show tab lays the same actions out as
	 * folding check-box groups, values inline. The actions stay the one source of truth. */
	wwKeepMenuOpenOnTick( showMenu );
	{
		WwMenuPanelStyle ps;
		ps.heading = []( const QString & t, QWidget * p ) { return wwHeading( t, p ); };
		ps.number = []( QDoubleSpinBox * f ) { f->setMinimumWidth( 96 ); };
		ps.foldGroup = QStringLiteral( "CellEditor/ShowFolds" );
		/* lane OPENCELLFIX2: the Display tab -- one row per feature (tick + dropdown), in three groups. A switch
		 * no row names still gets a row in Fusion Shaders (wwDisplayLeftovers counts them; the self-test wants 0). */
		const QStringList g = wwDisplayGroups();
		auto row = []( const QString & group, const char * tick, const char * choices = "", int off = -1,
			bool quality = false, const QStringList & extras = QStringList() ) {
			WwDisplayRow r;
			r.group = group;
			r.tick = QLatin1String( tick );
			r.choices = QLatin1String( choices );
			r.off = off;
			r.quality = quality;
			r.extras = extras;
			return r;
		};
		QList<WwDisplayRow> rows;
		WwDisplayRow color;
		color.group = g.at( 0 );
		color.dropdown = overlayBox;
		color.off = 0;	// Textured
		color.label = tr( "Color" );
		rows << color << row( g.at( 0 ), "CellWorkspaceShow_borders" ) << row( g.at( 0 ), "CellWorkspaceShow_markers" )
			<< row( g.at( 0 ), "CellWorkspaceShow_disabled" ) << row( g.at( 0 ), "CellWorkspacePrevis" )
			<< row( g.at( 0 ), "CellWorkspaceCulling" ) << row( g.at( 0 ), "CellWorkspaceLook_WW_BINDLESS" );
		/* bungo 10-27 notes 18-26: a group is one row and its rows fold under it, indented; groups by kind (lighting,
		 * reflections, air, water, distance, image, content), then the plain rows. A child drops the group's word.
		 * What the game draws is Vanilla, the rest Fusion Shaders; a group split between them has a master per half. */
		const QString V = g.at( 1 ), F = g.at( 2 );
		QString in;	// the group row the next children go under
		auto head = [&]( const QString & group, const char * tick, const char * choices = "", int off = -1,
			bool quality = false, const QStringList & extras = QStringList(), const QString & label = QString() ) {
			WwDisplayRow r = row( group, tick, choices, off, quality, extras );
			r.label = label;
			in = QLatin1String( *tick ? tick : choices );
			rows << r;
		};
		auto master = [&]( const QString & group, WwGroup m ) {
			const QByteArray name = QByteArray( "CellWorkspaceGroup_" ) + wwGroupName( m );
			WwDisplayRow r = row( group, name.constData() );
			r.master = true;
			in = QString::fromLatin1( name );
			rows << r;
		};
		auto kid = [&]( const char * tick, const QString & label, const char * choices = "", int off = -1,
			bool quality = false, const QStringList & extras = QStringList(), bool shared = false ) {
			WwDisplayRow r = row( QString(), tick, choices, off, quality, extras );
			r.label = label;
			r.parent = in;
			r.shared = shared;
			rows << r;
		};
		auto look = [&]( WwLook l, const QString & label ) {
			kid( ( QByteArray( "CellWorkspaceLook_" ) + wwLookPinName( l ) ).constData(), label );
		};
		// Vanilla
		head( V, "CellWorkspaceCellLights" );
		kid( "CellWorkspaceLightFlicker", tr( "Flicker" ) );
		look( WwLook::LightPatterns, tr( "Patterns" ) );
		master( V, WwGroup::Emittance );
		look( WwLook::PlacementEmittance, tr( "Placement" ) );
		look( WwLook::RegionEmittance, tr( "Region" ) );
		look( WwLook::ParticleEmittance, tr( "Particles" ) );
		master( V, WwGroup::ShadowsVanilla );
		kid( "CellWorkspaceLightShadows", tr( "Light shadows" ) );
		look( WwLook::CableShadowSway, tr( "Cable shadows sway" ) );
		head( V, "CellWorkspaceSsr" );	// note 20: one row, indoors and outdoors
		master( V, WwGroup::BloomVanilla );
		look( WwLook::LensFlares, tr( "Lens flares" ) );
		master( V, WwGroup::CameraVanilla );
		kid( "CellWorkspaceCellDof", tr( "Depth of field" ) );
		kid( "CellWorkspaceCellMotionBlur", tr( "Motion blur" ) );
		master( V, WwGroup::Decals );
		look( WwLook::DecalFade, tr( "Distance fade" ) );
		look( WwLook::DecalParallax, tr( "Parallax" ) );
		master( V, WwGroup::Objects );
		look( WwLook::ActorsOutdoors, tr( "Actors outdoors" ) );
		look( WwLook::WeaponParts, tr( "Weapon mod parts" ) );
		look( WwLook::AnimObjects, tr( "Animated objects" ) );
		look( WwLook::WorldAnim, tr( "World animation" ) );
		head( V, "CellWorkspaceCellGrass" );
		head( V, "CellWorkspaceParticles" );
		head( V, "CellWorkspaceShow_ground" );
		head( V, "CellWorkspaceShow_water" );
		head( V, "CellWorkspaceCellImageSpace" );
		head( V, "CellWorkspaceTemporalAa", "", -1, false, QStringList(), tr( "Temporal Anti-Aliasing" ) );	// round 4: Title Case
		rows.last().overriddenBy = QStringLiteral( "CellWorkspaceModernTaa" );	// gametaa.cpp: a Modern tier replaces it
		// Fusion Shaders
		head( F, "CellWorkspaceCellGi", "", -1, false,
			{ QStringLiteral( "CellWorkspaceSkyVisRow" ), QStringLiteral( "CellWorkspaceCellPass" ) } );
		rows.last().heavy = true;	// built-in Low: off (PRTP bounce light)
		look( WwLook::ProbeShare, tr( "Probe sharing" ) );
		look( WwLook::ProbeShareWide, tr( "Wide probe sharing" ) );
		look( WwLook::ProbeDepthVis, tr( "Probe depth visibility" ) );
		look( WwLook::ProbeSmooth, tr( "Probe light smoothing" ) );
		look( WwLook::ProbeClear, tr( "Probes out of walls" ) );
		kid( "CellWorkspaceEmissiveGiGrid", tr( "Emissive grid" ) );
		kid( "CellWorkspaceGpuRelight", tr( "GPU relight" ) );
		look( WwLook::BounceEmittance, tr( "Emittance in bounce light" ) );
		kid( "CellWorkspaceSkySpecOcc", tr( "Sky specular occlusion" ) );
		kid( "CellWorkspaceAoDecals", tr( "Ambient occlusion decals" ) );
		master( F, WwGroup::ImprovedLights );
		look( WwLook::LightPhysFalloff, tr( "Physical falloff" ) );
		look( WwLook::LightArea, tr( "Area" ) );
		kid( "CellWorkspaceLook_WW_CELL_LIGHT_ADV", tr( "Advanced model" ), "", -1, false,
			{ QStringLiteral( "CellWorkspaceLightCalibration" ) } );
		master( F, WwGroup::CellLights );
		look( WwLook::NifxLights, tr( "Mesh (.nifx)" ) );
		kid( "", tr( "Lamp brightness contrast" ), "CellWorkspaceLightCurve", 0 );
		kid( "CellWorkspaceCellFar", tr( "Far" ) );
		master( F, WwGroup::Shadows );
		kid( "CellWorkspaceCascadedShadows", tr( "Cascaded shadows" ), "CellWorkspaceCascadeQuality", -1, false,
			{ QStringLiteral( "CellWorkspaceShadowDistanceRow" ) }, true );
		kid( "", tr( "Screen-space shadows" ), "CellWorkspaceScreenSpaceShadows", 0, true, QStringList(), true );
		kid( "", tr( "Terrain shadows" ), "CellWorkspaceTerrainShadows", 0, true, QStringList(), true );
		kid( "CellWorkspaceShadowRules", tr( "Fusion Shaders shadow rules" ), "", -1, false, QStringList(), true );
		kid( "", tr( "Cached lamp shadows" ), "CellWorkspaceLampShadows", 0, true,
			{ QStringLiteral( "CellWorkspaceShadowBudget" ) } );
		look( WwLook::ContactLocal, tr( "Lamp contact shadows" ) );
		look( WwLook::ContactFar, tr( "Far contact shadows" ) );
		look( WwLook::TerrMoonSky, tr( "Moonlit terrain shadows" ) );
		look( WwLook::TerrSlopeMap, tr( "Terrain shadow slope from heights" ) );
		look( WwLook::TerrGroundGate, tr( "Object LOD shadows on ground only" ) );
		look( WwLook::FarCardShadows, tr( "Far tree card shadows" ) );
		look( WwLook::TerrPbrDim, tr( "PBR ambient in terrain shadow" ) );
		head( F, "CellWorkspaceAo", "CellWorkspaceAoMethod" );	// item 27 + round 4: the method first in its fold
		head( F, "CellWorkspaceSsgi", "CellWorkspaceSsgiQuality", -1, true );
		rows.last().heavy = true;
		master( F, WwGroup::Reflections );
		kid( "CellWorkspaceReflProbe", tr( "Probes" ) );
		head( F, "CellWorkspaceVolAir" );	// note 22: the Volumetric Air switch is the group's tick
		rows.last().heavy = true;	// fogperf1: the air zone ~3.4-3.8 ms at Concord
		look( WwLook::AirMultiScatter, tr( "Multiple scattering" ) );
		look( WwLook::GroundBurnOff, tr( "Ground burn-off" ) );
		look( WwLook::GroundFogDerived, tr( "Ground layer from weather" ) );
		kid( "", tr( "Far air" ), "CellWorkspaceFarAir", 0, true );
		rows.last().heavy = true;
		kid( "CellWorkspaceVolPart", tr( "Particles" ) );
		rows.last().heavy = true;
		kid( "CellWorkspaceFogDrift", tr( "Drift" ) );
		master( F, WwGroup::Sky );
		look( WwLook::Ephemeris, tr( "Real sun and moon" ) );
		look( WwLook::WeatherRecx, tr( "Weather .recx" ) );
		master( F, WwGroup::Water );
		look( WwLook::WaterDepth, tr( "Depth colour" ) );
		look( WwLook::WaterCaustics, tr( "Caustics" ) );
		look( WwLook::WaterSoftEdge, tr( "Soft edge" ) );
		look( WwLook::ShoreFoam, tr( "Shore foam" ) );
		look( WwLook::WaterSwell, tr( "Swell" ) );
		look( WwLook::WaterRefl, tr( "Reflections" ) );
		look( WwLook::WaterGlint, tr( "Sun glare" ) );
		look( WwLook::WaterOcean, tr( "Ocean waves" ) );
		head( F, "CellWorkspaceFarLod", "", -1, false, QStringList(), tr( "LOD" ) );	// note 21 + 26b: one LOD group
		kid( "", tr( "Source" ), "CellWorkspaceFarLodType" );
		kid( "", tr( "Terrain" ), "CellWorkspaceFarGround" );
		kid( "", tr( "Terrain pages" ), "CellWorkspaceFarGroundPages" );
		look( WwLook::BtoSway, tr( "Game trees sway" ) );
		head( F, "", "CellWorkspaceModernTaa", 0, true );
		kid( "CellWorkspaceModernTaaHdr", tr( "Before tone map" ) );
		head( F, "", "CellWorkspaceBloom", 0 );
		look( WwLook::BloomMip, tr( "Width in fog" ) );
		master( F, WwGroup::Camera );
		look( WwLook::ExposureWide, tr( "Wide exposure range" ) );
		showPanel = wwMenuAsDisplay( showMenu, rows, this, ps );
		page->addWidget( showPanel );
	}

	// ---- band 3: PRTP (Precomputed Radiance Transfer Probes) -- the probe controls only
	page = box( showL, "CellWorkspacePrtp", 0 );
	prtpBox = page->parentWidget();
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
	// lane CELLALL1: the second row. A shader or pass switch repaints; a switch read when the cell opens reopens it.
	auto fxLive = [this]( QAction * box, void ( *set )( bool ) ) {
		connect( box, &QAction::toggled, this, [this, set]( bool on ) {
			set( on );
			if ( glView )
				glView->update();
		} );
	};
	fxLive( cellAo, &wwCellAoSetOn );
	fxLive( cellShadow, &wwCellShadowSetOn );
	fxLive( cellFlicker, &wwCellFlickerSetOn );
	fxLive( cellSsr, &wwCellSsrRowSet );	// note 20: indoors and outdoors alike
	fxLive( cellReflProbe, &wwCellReflProbeSetOn );
	fxLive( cellSsgi, &wwCellSsgiSetOn );
	fxLive( cellSpecOcc, &wwCellSpecOccSetOn );	// lane SPECOCC1: the GI texture re-uploads with or without the sky grid
	connect( cellLampShadowGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		wwCellLampShadowSetTier( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
	connect( cellShadowBudgetGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {	// lane GAPLOOK1
		wwCellShadowBudgetSet( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
	// lane LIVESET4 (bungo: settings update live, like games): the far LOD and the streamed cells rebuild in place; lane FAR6: Game LOD trees sway too
	auto liveFirst = [this]() {
		QString why;
		const int r = cellLiveRebuild( nif, &why );
		if ( r == 2 )
			return false;	// the reopen below
		if ( r == 1 ) {
			const QString legend = cellWorkspaceNotesLive();
			say( legend.isEmpty() ? why : why + QStringLiteral( "; " ) + legend, false );
		}
		if ( glView )
			glView->update();
		return true;
	};
	connect( cellLightCurveGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {	// lane LIGHTPHYS1
		wwCellLightCurveSet( float( a->data().toDouble() ) );
		if ( glView )
			glView->update();
	} );
	connect( cellLightCalibGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {	// lane LIGHTPHYS3
		wwCellLightCalibrationSet( float( a->data().toDouble() ) );
		if ( glView )
			glView->update();
	} );
	for ( int k = 0; k < lookActions.size(); k++ )
		connect( lookActions.at( k ), &QAction::toggled, this, [this, l = WwLook( lookActions.at( k )->data().toInt() ), liveFirst]( bool on ) {
			wwLookSet( l, on );
			if ( wwLookReopens( l ) && !syncing && !g_path.isEmpty() ) {	// lane GAPLOOK2: built when the cell opens
				if ( liveFirst() )
					return;
				g_over = g_spec;
				g_haveOverrides = true;
				say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
				emit reopenRequested( g_path );
				return;
			}
			if ( glView )
				glView->update();
		} );
	// lane OPENCELLFIX2: a group master. Its rows read off with it; a row read while the cell is built reopens it
	for ( QAction * a : groupsMenu->actions() ) {
		if ( !a->objectName().startsWith( QLatin1String( "CellWorkspaceGroup_" ) ) )
			continue;
		connect( a, &QAction::toggled, this, [this, g = WwGroup( a->data().toInt() ), liveFirst]( bool on ) {
			wwGroupSet( g, on );
			bool reopen = false;
			for ( QAction * la : lookActions ) {
				const WwLook l = WwLook( la->data().toInt() );
				reopen = reopen || ( wwLookGroup( l ) == g && wwLookReopens( l ) && la->isChecked() );
			}
			if ( reopen && !syncing && !g_path.isEmpty() ) {
				if ( liveFirst() )
					return;
				g_over = g_spec;
				g_haveOverrides = true;
				say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
				emit reopenRequested( g_path );
				return;
			}
			if ( glView )
				glView->update();
		} );
	}
	connect( cellModernTaaGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		wwTaa2SetTier( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
	connect( cellFarGroundGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {	// lane FARDRAW1
		CellFarGround::setMode( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
	connect( cellFarGroundQGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		CellFarGround::setQuality( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
	connect( cellSsgiQGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		wwCellSsgiSetQuality( a->data().toInt() );
		if ( glView )
			glView->update();
	} );
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
	fxReopen( cellEmissiveGrid, &wwCellEmissiveGridSetOn );	// lane EMISSIVEGRID1: built when the cell opens
	fxReopen( cellParticles, &wwCellParticlesSetOn );	// lane SUNCELL1: the systems are copied in when the cell opens
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
	connect( cellModernTaaHdr, &QAction::toggled, this, [this]( bool on ) {	// lane TAA3
		wwTaa2SetHdrOn( on );
		if ( glView )
			glView->update();
	} );
	// lane GRASSMB1: the grass is placed when the cell opens, so the switch rebuilds the cell
	connect( cellGrass, &QAction::toggled, this, [this]( bool on ) {
		wwCellGrassSetOn( on );
		if ( syncing || g_path.isEmpty() )
			return;
		g_over = g_spec;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );
	// lane GRASSMB1: passes on the finished frame, no rebuild
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
	// lane PRTPGI: the bounce of the last bake, drawn while Cell lights is on
	connect( cellGi, &QAction::toggled, this, [this]( bool on ) {
		wwCellGiSetOn( on );
		if ( glView )
			glView->update();
	} );
	// lane FARVIEW1: a shader switch (the far sectors load with the cell)
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
	// lane IMGS1: the game's exposure, curve, grade and LUT in place of the viewer's curve
	connect( cellIs, &QAction::toggled, this, [this]( bool on ) {
		wwCellImageSpaceSetOn( on );
		if ( glView )
			glView->update();
	} );
	// lane FARLOD1: the far field is document geometry, so the cell is built again with (or without) it
	connect( cellFarLodTypeGroup, &QActionGroup::triggered, this, [this, liveFirst]( QAction * a ) {
		cellFarLodSetType( a->data().toInt() == 0 ? WwFarLodVanilla : WwFarLodFo4cs );
		if ( syncing || g_path.isEmpty() || !cellFarLodWanted() )
			return;
		if ( liveFirst() )
			return;
		g_over = g_spec;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );
	connect( cellFarLod, &QAction::toggled, this, [this, liveFirst]( bool on ) {
		cellFarLodSetWanted( on );
		if ( syncing || g_path.isEmpty() )
			return;
		if ( liveFirst() )
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
		QString why;
		if ( cellLiveProbesShow( nif, on, &why ) == 1 ) {	// lane LIVESET1: in place, no rebuild
			g_spec.probesShow = on;
			if ( glView )
				glView->update();
			say( why, false );
			return;
		}
		g_over = g_spec;
		g_over.probesShow = on;
		g_haveOverrides = true;
		say( tr( "rebuilding %1 ..." ).arg( QFileInfo( g_path ).fileName() ), false );
		emit reopenRequested( g_path );
	} );

	/* ---- lane MODPICK1: which mods the cells load (bungo 10-12). The list, the saved choice per worldspace
	 * and the one-stack rule live in cellmodpick.h; Apply or Reset rebuilds the cell like the Far LOD row. */
	page = box( loL, "CellWorkspaceMods", 1 );
	modsBox = page->parentWidget();
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

	// ---- band 4: the legend (the References tab)
	page = refsLay;
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

	/* PINNED under everything: the cell's one-line census, then the summary or
	 * the refusal. Neither scrolls away -- the panel style's action-bar rule for
	 * a panel whose only action is clicking. */
	showL->addStretch( 1 );
	page = new QVBoxLayout;
	page->setContentsMargins( 6, 2, 6, 6 );
	page->setSpacing( 2 );
	outer->addLayout( page );
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
	connect( filterType, &WwDropdown::currentIndexChanged, this, &CellWorkspacePanel::filterChanged );
	connect( overlayBox, &WwDropdown::currentIndexChanged, this, &CellWorkspacePanel::viewRowChanged );

	// lane OPENCELLFIX1: the last tab comes back; the Show tab re-reads its actions when shown
	connect( tabs, &QTabBar::currentChanged, this, [this]( int index ) {
		if ( index < 0 )
			return;
		tabStack->setCurrentIndex( index );
		const QString id = tabs->tabData( index ).toString();
		if ( id == QLatin1String( "show" ) )
			wwMenuPanelSync( showPanel );
		QSettings().setValue( QStringLiteral( "CellEditor/Tab" ), id );
	} );
	const QString savedTab = QSettings().value( QStringLiteral( "CellEditor/Tab" ), QStringLiteral( "loadorder" ) ).toString();
	setCurrentTab( savedTab );
	showCellParts( false );
}

QString CellWorkspacePanel::currentTab() const
{
	return tabs ? tabs->tabData( tabs->currentIndex() ).toString() : QString();
}

void CellWorkspacePanel::setCurrentTab( const QString & id )
{
	if ( !tabs )
		return;
	for ( int i = 0; i < tabs->count(); i++ )
		if ( tabs->tabData( i ).toString() == id ) {
			if ( tabs->currentIndex() == i ) {
				tabStack->setCurrentIndex( i );
				QSettings().setValue( QStringLiteral( "CellEditor/Tab" ), id );
			} else {
				tabs->setCurrentIndex( i );
			}
			return;
		}
}

void CellWorkspacePanel::showCellParts( bool on )
{
	for ( QWidget * w : { refsBox, modsBox, prtpBox } )
		if ( w )
			w->setVisible( on );
}

bool CellWorkspacePanel::cellPartsShown() const
{
	return refsBox && !refsBox->isHidden();
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
	/* lane OPENCELLFIX1: the parts that need a cell show once one is open, and a NEW cell (not a reopen of the same
	 * one by a Show switch) brings the References tab forward. */
	showCellParts( !g_path.isEmpty() );
	if ( g_path.isEmpty() )
		openedPath.clear();
	else if ( g_path != openedPath ) {
		openedPath = g_path;
		setCurrentTab( QStringLiteral( "references" ) );
	}
	rebuildList();
	rebuildLegendAndCensus();
	if ( cellPass )	// lane PROBEVIEW1: a Pass needs this document's bake relit
		cellPass->setEnabled( nif && wwCellGiFor( nif ) );
	if ( modPick )	// lane MODPICK1: the open exterior's mods
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

	/* THE LIST IS THE REFERENCE MODEL, NOT THE DRAW DATA (bungo: "view all the
	 * technical placed objects ... do everything creation kit does with cell
	 * editing"). One row per REFR the plugin has -- including the lights, the
	 * sound markers, the triggers and the primitives, which carry no model and
	 * therefore never reach the pick table at all. Its count IS the plugin's
	 * placed-reference count for the block, which is what the gate checks
	 * against an independent read of the same plugin.
	 *
	 * The two tables are joined by REFERENCE FORM ID, once, here: a drawn
	 * reference gets the pick index its row selects with, an undrawn one gets -1
	 * and an honest refusal when clicked. An SCOL is several pick entries and
	 * ONE reference, so the join takes the first part. */
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
		/* The triangle column SORTS AS A NUMBER. A text column would put 9 after
		 * 10000, and this column exists to answer "what is heavy in this cell".
		 * A reference that was not drawn prints a DASH rather than 0, because 0
		 * is a claim about its geometry and a dash is not. */
		if ( pick >= 0 )
			item->setData( 3, Qt::DisplayRole, QVariant( qulonglong( picks.at( pick ).triangles ) ) );
		else
			item->setText( 3, QStringLiteral( "-" ) );
		item->setText( 4, cell );
		item->setText( 5, e.fate == CellRefFate::Drawn ? QString() : state );
		item->setData( 0, Qt::UserRole, i );
		/* The haystack the text filter searches, built once per row rather than
		 * per keystroke. */
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
	// lane OPENCELLFIX1: the census already counts them; the line speaks only while a filter hides some
	say( shown == total ? QString() : tr( "%1 of %2 references" ).arg( shown ).arg( total ), false );
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
	/* ONE FUNCTION FOR BOTH DOORS. A row click and a viewport click end at
	 * `cellPickSelect`, which is the tail the mouse already used, so the
	 * selection, the highlight bars and the `picked` signal cannot differ
	 * between the two -- which is precisely what the gate asserts. */
	const int pick = ( index >= 0 && index < pickForRef.size() ) ? pickForRef.at( index ) : -1;
	if ( pick < 0 ) {
		/* A REFUSAL BY NAME. A light, a sound marker, a trigger box or a deleted
		 * record has no geometry to select or frame, and saying WHICH of those it
		 * is beats a click that quietly does nothing. */
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
	/* `index` is a PICK index -- the viewport picks draw data. The row it lands
	 * on is found through the same join the list was built with, so the mouse
	 * and a row click cannot select different rows for the same reference. */
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

	/* THE VIEW IS IN THE SCENE'S SPACE, NOT THE WORLD'S. The cell's shapes carry
	 * the block origin as their Translation, so the camera target is the box
	 * centre MINUS that origin -- the same subtraction the highlight bars make
	 * (src/cellclick.cpp, cellHighlightShow). Framing in world units would put
	 * the camera tens of thousands of units off for any cell but 0,0. */
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

	/* A REBUILD, AND THE PANEL SAYS SO BEFORE IT STARTS. The overlay is baked
	 * into vertex colours while the scene welds, so this is not a repaint; the
	 * whole cell is read again. Saying it here means the pause is explained
	 * rather than looking like a hang. */
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
	/* The rows show what the cell WAS BUILT WITH, not what was last asked for.
	 * If the builder refused a row -- and it can -- the popover says what is on
	 * screen rather than what was requested. */
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
		/* THE SWATCH IS THE PRINTED NUMBER, NOT A SECOND OPINION. The rgb comes
		 * from the builder's own line, which lane CELLVIEW3 made the draw site's
		 * own function; painting anything else here would reintroduce exactly
		 * the disagreement that lane removed. */
		item->setBackground( 0, QColor::fromRgbF(
			qBound( 0.0f, r.rgb[0], 1.0f ),
			qBound( 0.0f, r.rgb[1], 1.0f ),
			qBound( 0.0f, r.rgb[2], 1.0f ) ) );
	}

	/* THE CENSUS COUNTS THE PLUGIN'S REFERENCES, not the drawn ones -- the same
	 * number the list shows, because it is the same table. */
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
		/* A REFUSAL, NOT A ZERO. Printing "0 triangles" because the notes could
		 * not be parsed would be this panel asserting something about the cell
		 * that it does not know. */
		: tr( "%1 references  -  the builder's notes did not carry a geometry line" )
			.arg( formatCount( refs ) ) );

	const QString refusal = refusalFromNotes( g_notes );
	if ( !refusal.isEmpty() )
		say( refusal, true );
	else
		say( QString(), false );	// lane OPENCELLFIX1: the census line above already says how many
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

/* THE COUNTS ARE READ, NOT COUNTED. They come out of the placer's own census
 * lines in the builder's notes (probeCensusText), the way the legend does. */
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
	// lane PRTPBAKE: what the bake wrote, read from its own census lines
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
	note->setVisible( !text.isEmpty() );
	note->setStyleSheet( refusal
		? QStringLiteral( "color: %1;" ).arg( wwSkinColor( "danger" ) )
		: QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
}
