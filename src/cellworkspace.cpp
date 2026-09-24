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

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QColor>
#include <QComboBox>
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
}

void cellWorkspaceNoteOpened( const QString & path, const CellSceneSpec & spec,
	const QString & notes )
{
	g_path = path;
	g_spec = spec;
	g_notes = notes;
	if ( !g_haveOverrides ) {
		// The first cell of the session seeds the rows from the file itself, so
		// a row shows what is actually drawn before anybody touches one.
		g_over = spec;
	}
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

	// ---- band 2: the view
	page->addWidget( wwHeading( tr( "View" ), this ) );

	QWidget * viewRow = new QWidget( this );
	QHBoxLayout * vl = new QHBoxLayout( viewRow );
	vl->setContentsMargins( 0, 0, 0, 0 );
	vl->setSpacing( 4 );
	QLabel * overlayName = new QLabel( tr( "Colour" ), viewRow );
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

	/* THE SHOW POPOVER -- and it exists ONLY here. It is a child of this panel,
	 * which is a child of the Cell workspace's dock, so it is on screen exactly
	 * when that workspace is and can appear in no other workspace's menus. That
	 * is not a rule to be enforced; it is where the widget lives. */
	QToolButton * showButton = new QToolButton( viewRow );
	showButton->setObjectName( QStringLiteral( "CellWorkspaceShowButton" ) );
	showButton->setText( tr( "Show" ) );
	showButton->setPopupMode( QToolButton::InstantPopup );
	showButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
	showButton->setAutoRaise( false );
	showButton->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 6px" ) ) );
	QMenu * showMenu = new QMenu( showButton );
	showMenu->setObjectName( QStringLiteral( "CellWorkspaceShowMenu" ) );

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
			viewRowChanged();
		} );
		showActions.append( a );
	}
	showButton->setMenu( showMenu );
	vl->addWidget( showButton );

	// ---- band 3: the legend
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
		say( tr( "%1 references" ).arg( refs ), false );
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
