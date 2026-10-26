/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencelldialog.h"
#include "wwswitch.h"	// lane CELLSPAWN1: WW_OPENCELL_RED=list, the shot harness

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <limits>

// src/ui/widgets/wwnumberfield.h: a selector styled as the number fields beside it (skill nifskope-ww-panel-style)
void wwMatchFieldStyle( QWidget * selector );

namespace {

QString key( const char * k )
{
	return QStringLiteral( "WW/OpenCell/" ) + QLatin1String( k );
}

bool parseXY( const QString & s, int & x, int & y )
{
	const QStringList p = s.split( QLatin1Char( ',' ) );
	bool okx = false, oky = false;
	if ( p.size() == 2 ) {
		x = p[0].trimmed().toInt( &okx );
		y = p[1].trimmed().toInt( &oky );
	}
	return okx && oky;
}

QString formHex( quint32 form )
{
	return QStringLiteral( "%1" ).arg( form, 8, 16, QLatin1Char( '0' ) ).toUpper();
}

// The roles every row carries, in the old list and the new one.
constexpr int RoleForm = Qt::UserRole;          // the reader's form (a spec line's interior name when no EDID)
constexpr int RoleXY = Qt::UserRole + 1;        // "x,y"
constexpr int RoleEdid = Qt::UserRole + 2;      // the real editor ID (the column may read "Wilderness")
constexpr int RoleWorld = Qt::UserRole + 3;     // the worldspace's EDID, empty for an interior
constexpr int RoleInterior = Qt::UserRole + 4;

bool oldList()
{
	// The red: the list as it was before lane CELLSPAWN1 (no FormID on exteriors, no plugins, no filters).
	return wwEnvStr( "WW_OPENCELL_RED" ) == QLatin1String( "list" );
}

struct Row
{
	const OpenCellCell * c = nullptr;
	QString world;     // EDID
	bool interior = false;
	int order = 0;     // the catalog's order, the tie-break of every sort
};

} // namespace

OpenCellDialog::OpenCellDialog( QWidget * parent ) : QDialog( parent )
{
	setWindowTitle( tr( "Open Cell" ) );
	build();
	loadSettings();
}

void OpenCellDialog::build()
{
	auto * grid = new QGridLayout( this );
	grid->setContentsMargins( 8, 8, 8, 8 );
	grid->setHorizontalSpacing( 6 );
	grid->setVerticalSpacing( 4 );
	int row = 0;
	auto label = [&]( const QString & t ) {
		auto * l = new QLabel( t, this );
		grid->addWidget( l, row, 0, Qt::AlignRight | Qt::AlignTop );
	};

	label( tr( "Data" ) );
	auto * dataRow = new QHBoxLayout;
	dataEdit = new QLineEdit( this );
	auto * browse = new QToolButton( this );
	browse->setText( QStringLiteral( "..." ) );
	browse->setToolTip( tr( "Pick the Data folder" ) );
	auto * import = new QToolButton( this );
	import->setText( tr( "Import" ) );
	import->setToolTip( tr( "Import a load order: plugins.txt, or an MO2 profile's plugins.txt" ) );
	dataRow->addWidget( dataEdit, 1 );
	dataRow->addWidget( browse );
	dataRow->addWidget( import );
	grid->addLayout( dataRow, row++, 1 );

	label( tr( "Language" ) );
	langCombo = new QComboBox( this );
	langCombo->setEditable( true );   // the game's language codes; any other a mod ships can be typed
	langCombo->addItems( { QStringLiteral( "en" ), QStringLiteral( "de" ), QStringLiteral( "fr" ), QStringLiteral( "it" ),
		QStringLiteral( "es" ), QStringLiteral( "pl" ), QStringLiteral( "ptbr" ), QStringLiteral( "ru" ),
		QStringLiteral( "ja" ), QStringLiteral( "cn" ) } );
	langCombo->setToolTip( tr( "Names of localized plugins come from Strings/<plugin>_<language>.STRINGS" ) );
	langCombo->setCurrentText( openCellStringsLanguage() );
	wwMatchFieldStyle( langCombo );
	grid->addWidget( langCombo, row++, 1 );

	label( tr( "Plugins" ) );
	pluginList = new QListWidget( this );
	pluginList->setDragDropMode( QAbstractItemView::InternalMove );   // the list order is the load order
	pluginList->setMinimumHeight( 120 );
	grid->addWidget( pluginList, row++, 1 );

	label( tr( "Worldspace" ) );
	worldCombo = new QComboBox( this );
	worldCombo->setObjectName( QStringLiteral( "OpenCellWorld" ) );
	wwMatchFieldStyle( worldCombo );
	grid->addWidget( worldCombo, row++, 1 );

	// The block list's search bar: one field, a Filters menu beside it (src/nifskope.cpp, BlockListToolbar).
	label( tr( "Cell" ) );
	auto * searchRow = new QHBoxLayout;
	searchRow->setSpacing( 2 );
	filterEdit = new QLineEdit( this );
	filterEdit->setObjectName( QStringLiteral( "OpenCellSearch" ) );
	filterEdit->setPlaceholderText( tr( "Search cells…" ) );
	filterEdit->setClearButtonEnabled( true );
	filterEdit->setToolTip( tr( "Editor ID, name or FormID; space-separated terms must all match. x, y finds a grid cell." ) );
	searchRow->addWidget( filterEdit, 1 );
	filterButton = new QToolButton( this );
	filterButton->setObjectName( QStringLiteral( "OpenCellFilters" ) );
	filterButton->setPopupMode( QToolButton::InstantPopup );
	filterButton->setAutoRaise( true );
	filterButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
	filterButton->setAccessibleName( tr( "Cell search filters" ) );
	searchRow->addWidget( filterButton );
	grid->addLayout( searchRow, row++, 1 );

	cellTree = new QTreeWidget( this );
	cellTree->setObjectName( QStringLiteral( "OpenCellList" ) );
	cellTree->setRootIsDecorated( false );
	cellTree->setUniformRowHeights( true );
	cellTree->setMinimumHeight( 220 );
	cellTree->setColumnCount( ColCount );
	cellTree->header()->setStretchLastSection( false );
	cellTree->header()->setSectionsClickable( true );
	grid->addWidget( cellTree, row++, 1 );
	countLabel = new QLabel( this );
	countLabel->setObjectName( QStringLiteral( "OpenCellCount" ) );
	grid->addWidget( countLabel, row++, 1 );

	auto * at = new QHBoxLayout;
	xyEdit = new QLineEdit( this );
	xyEdit->setPlaceholderText( QStringLiteral( "x, y" ) );
	sizeCombo = new QComboBox( this );
	for ( int n : { 1, 3, 5 } )
		sizeCombo->addItem( QStringLiteral( "%1 x %1" ).arg( n ), n );
	wwMatchFieldStyle( sizeCombo );
	at->addWidget( xyEdit, 1 );
	at->addWidget( new QLabel( tr( "Size" ), this ) );
	at->addWidget( sizeCombo );
	label( tr( "X, Y" ) );
	grid->addLayout( at, row++, 1 );

	status = new QLabel( this );
	status->setWordWrap( true );
	grid->addWidget( status, row++, 1 );

	auto * buttons = new QDialogButtonBox( QDialogButtonBox::Open | QDialogButtonBox::Cancel, this );
	openButton = buttons->button( QDialogButtonBox::Open );
	grid->addWidget( buttons, row++, 0, 1, 2 );
	grid->setColumnStretch( 1, 1 );
	resize( 720, 640 );

	readTimer = new QTimer( this );
	readTimer->setSingleShot( true );
	readTimer->setInterval( 400 );     // ticking several plugins in a row reads once
	searchTimer = new QTimer( this );
	searchTimer->setSingleShot( true );
	searchTimer->setInterval( 150 );   // a whole worldspace is thousands of rows: list once per pause in typing
	buildFilters();

	connect( browse, &QToolButton::clicked, this, &OpenCellDialog::browseData );
	connect( import, &QToolButton::clicked, this, &OpenCellDialog::browseImport );
	connect( dataEdit, &QLineEdit::editingFinished, this, [this]() { setDataDir( dataEdit->text() ); } );
	connect( langCombo, &QComboBox::currentTextChanged, this, &OpenCellDialog::setLanguage );
	connect( pluginList, &QListWidget::itemChanged, this, &OpenCellDialog::pluginsChanged );
	connect( pluginList->model(), &QAbstractItemModel::rowsMoved, this, &OpenCellDialog::pluginsChanged );
	connect( readTimer, &QTimer::timeout, this, &OpenCellDialog::readNow );
	connect( worldCombo, &QComboBox::currentIndexChanged, this, &OpenCellDialog::fillCells );
	connect( filterEdit, &QLineEdit::textChanged, this, [this]() { searchTimer->start(); } );
	connect( searchTimer, &QTimer::timeout, this, &OpenCellDialog::fillCells );
	connect( cellTree->header(), &QHeaderView::sectionClicked, this, [this]( int col ) {
		sortCells( col, col == sortColumn ? !sortDescending : false );
	} );
	connect( cellTree, &QTreeWidget::itemSelectionChanged, this, &OpenCellDialog::cellPicked );
	connect( cellTree, &QTreeWidget::itemDoubleClicked, this, &OpenCellDialog::openClicked );
	connect( buttons, &QDialogButtonBox::accepted, this, &OpenCellDialog::openClicked );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
}

void OpenCellDialog::buildFilters()
{
	// The block list's advanced-search menu: bold disabled headings, checkable groups, one reset at the end.
	filterMenu = new QMenu( filterButton );
	filterMenu->setObjectName( QStringLiteral( "OpenCellFilterMenu" ) );
	filterButton->setMenu( filterMenu );
	auto heading = [this]( const QString & text ) {
		if ( !filterMenu->actions().isEmpty() )
			filterMenu->addSeparator();
		QAction * h = filterMenu->addAction( text );
		h->setEnabled( false );
		QFont f = h->font();
		f.setBold( true );
		h->setFont( f );
	};
	auto item = [this]( const QString & text, const char * name, QActionGroup * group, bool on ) {
		QAction * a = filterMenu->addAction( text );
		a->setObjectName( QLatin1String( name ) );
		a->setCheckable( true );
		a->setChecked( on );
		if ( group )
			group->addAction( a );
		return a;
	};

	heading( tr( "Search In" ) );
	QList<QAction *> scope;
	scope << item( tr( "Editor ID" ), "OpenCellSearchEditorId", nullptr, true );
	scope << item( tr( "Name" ), "OpenCellSearchName", nullptr, true );
	scope << item( tr( "FormID" ), "OpenCellSearchFormId", nullptr, true );
	const int bits[3] = { SearchEditorId, SearchName, SearchFormId };
	for ( int i = 0; i < 3; i++ ) {
		scope[i]->setData( bits[i] );
		connect( scope[i], &QAction::toggled, this, [this, scope, i]( bool ) {
			int fields = 0;
			for ( QAction * a : scope )
				if ( a->isChecked() )
					fields |= a->data().toInt();
			// An empty scope would match nothing with no visible reason: keep the last field.
			if ( !fields ) {
				QSignalBlocker b( scope[i] );
				scope[i]->setChecked( true );
				fields = scope[i]->data().toInt();
			}
			searchFields = fields;
			fillCells();
		} );
	}

	heading( tr( "Cells" ) );
	auto * kinds = new QActionGroup( filterMenu );
	item( tr( "Interior and exterior" ), "OpenCellCellsAll", kinds, true )->setData( 0 );
	item( tr( "Interior only" ), "OpenCellCellsInterior", kinds, false )->setData( 1 );
	item( tr( "Exterior only" ), "OpenCellCellsExterior", kinds, false )->setData( 2 );
	connect( kinds, &QActionGroup::triggered, this, [this]( QAction * a ) { cellsKind = a->data().toInt(); fillCells(); } );

	heading( tr( "Name" ) );
	auto * names = new QActionGroup( filterMenu );
	item( tr( "Any" ), "OpenCellNameAny", names, true )->setData( 0 );
	item( tr( "With a name" ), "OpenCellNamed", names, false )->setData( 1 );
	item( tr( "Without a name" ), "OpenCellUnnamed", names, false )->setData( 2 );
	connect( names, &QActionGroup::triggered, this, [this]( QAction * a ) { nameKind = a->data().toInt(); fillCells(); } );

	heading( tr( "Plugin" ) );
	auto * roles = new QActionGroup( filterMenu );
	item( tr( "Added by" ), "OpenCellPluginOwner", roles, true )->setData( false );
	item( tr( "Last edited by" ), "OpenCellPluginLastEdit", roles, false )->setData( true );
	connect( roles, &QActionGroup::triggered, this, [this]( QAction * a ) { pluginLastEdit = a->data().toBool(); fillCells(); } );
	pluginMenu = filterMenu->addMenu( tr( "Any plugin" ) );
	pluginMenu->setObjectName( QStringLiteral( "OpenCellPluginMenu" ) );
	pluginGroup = new QActionGroup( pluginMenu );
	connect( pluginGroup, &QActionGroup::triggered, this, [this]( QAction * a ) {
		pluginIndex = a->data().toInt();
		fillCells();
	} );

	filterMenu->addSeparator();
	QAction * reset = filterMenu->addAction( tr( "Reset Search and Filters" ) );
	reset->setObjectName( QStringLiteral( "OpenCellResetSearchFilters" ) );
	connect( reset, &QAction::triggered, this, [this, scope]() {
		for ( QAction * a : scope ) {
			QSignalBlocker b( a );
			a->setChecked( true );
		}
		searchFields = SearchEditorId | SearchName | SearchFormId;
		for ( const char * n : { "OpenCellCellsAll", "OpenCellNameAny", "OpenCellPluginOwner", "OpenCellPluginAny" } )
			if ( QAction * a = filterMenu->findChild<QAction *>( QLatin1String( n ) ) )
				a->setChecked( true );
		cellsKind = 0;
		nameKind = 0;
		pluginLastEdit = false;
		pluginIndex = -1;
		{
			QSignalBlocker b( filterEdit );
			filterEdit->clear();
		}
		fillCells();
	} );
	fillPluginFilter();
	refreshFilterButton();
}

void OpenCellDialog::fillPluginFilter()
{
	pluginMenu->clear();
	for ( QAction * a : pluginGroup->actions() )
		pluginGroup->removeAction( a );
	auto add = [this]( const QString & text, int index ) {
		QAction * a = pluginMenu->addAction( text );
		a->setObjectName( index < 0 ? QStringLiteral( "OpenCellPluginAny" ) : QStringLiteral( "OpenCellPlugin_" ) + text );
		a->setCheckable( true );
		a->setData( index );
		a->setChecked( index == pluginIndex );
		pluginGroup->addAction( a );
	};
	add( tr( "Any plugin" ), -1 );
	pluginMenu->addSeparator();
	for ( int i = 0; i < catalog.plugins.size(); i++ )
		add( catalog.plugins[i], i );
}

void OpenCellDialog::setDataDir( const QString & dir )
{
	const QString d = QDir::cleanPath( dir );
	if ( dataEdit->text() != d )
		dataEdit->setText( d );
	if ( sources.dataDir == d && pluginList->count() )
		return;
	sources.dataDir = d;
	QSignalBlocker block( pluginList );
	pluginList->clear();
	for ( const QString & name : openCellDataPlugins( d ) ) {
		auto * it = new QListWidgetItem( name, pluginList );
		it->setFlags( it->flags() | Qt::ItemIsUserCheckable );
		it->setCheckState( Qt::Unchecked );
	}
	pluginsChanged();
}

void OpenCellDialog::setPluginsTicked( const QStringList & names )
{
	{
		QSignalBlocker block( pluginList );
		// Ticked plugins move up in the given order so the list reads as the load order.
		int at = 0;
		for ( const QString & n : names ) {
			const auto found = pluginList->findItems( n, Qt::MatchFixedString );
			QListWidgetItem * it = found.isEmpty() ? nullptr : found.first();
			if ( !it ) {
				it = new QListWidgetItem( n );
				it->setFlags( it->flags() | Qt::ItemIsUserCheckable );
			} else {
				pluginList->takeItem( pluginList->row( it ) );
			}
			it->setCheckState( Qt::Checked );
			pluginList->insertItem( at++, it );
		}
		for ( int i = at; i < pluginList->count(); i++ )
			pluginList->item( i )->setCheckState( Qt::Unchecked );
	}
	pluginsChanged();
}

void OpenCellDialog::setLanguage( const QString & lang )
{
	if ( langCombo->currentText() != lang ) {
		langCombo->setCurrentText( lang );   // re-enters through currentTextChanged
		return;
	}
	if ( lang.trimmed().isEmpty() || lang.trimmed() == openCellStringsLanguage() )
		return;
	openCellSetStringsLanguage( lang );
	pluginsChanged();
}

bool OpenCellDialog::importLoadOrder( const QString & pluginsTxt )
{
	QStringList plugins;
	QString err;
	if ( !openCellImportLoadOrder( pluginsTxt, plugins, sources, &err ) ) {
		status->setText( err );
		return false;
	}
	setPluginsTicked( plugins );
	return true;
}

void OpenCellDialog::browseData()
{
	const QString d = QFileDialog::getExistingDirectory( this, tr( "Data folder" ), dataEdit->text() );
	if ( !d.isEmpty() )
		setDataDir( d );
}

void OpenCellDialog::browseImport()
{
	const QString f = QFileDialog::getOpenFileName( this, tr( "Load order" ), sources.dataDir,
		tr( "Load order (plugins.txt *.txt)" ) );
	if ( !f.isEmpty() )
		importLoadOrder( f );
}

void OpenCellDialog::pluginsChanged()
{
	readTimer->start();
}

QStringList OpenCellDialog::loadOrderPaths() const
{
	QStringList ticked;
	for ( int i = 0; i < pluginList->count(); i++ )
		if ( pluginList->item( i )->checkState() == Qt::Checked )
			ticked.append( pluginList->item( i )->text() );
	QStringList missing;
	QStringList paths;
	for ( const QString & n : openCellWithMasters( ticked, sources, &missing ) )
		paths.append( sources.resolve( n ) );
	return paths;
}

void OpenCellDialog::readNow()
{
	readTimer->stop();
	QStringList ticked, missing;
	for ( int i = 0; i < pluginList->count(); i++ )
		if ( pluginList->item( i )->checkState() == Qt::Checked )
			ticked.append( pluginList->item( i )->text() );
	const QStringList order = openCellWithMasters( ticked, sources, &missing );
	QString err;
	const QString keepPlugin = pluginIndex >= 0 ? catalog.plugins.value( pluginIndex ) : QString();
	catalog = OpenCellCatalog();
	if ( !order.isEmpty() ) {
		QApplication::setOverrideCursor( Qt::WaitCursor );   // a full master takes seconds to index
		OpenCellReadOptions opt;
		opt.dataDir = sources.dataDir;
		openCellReadCatalog( loadOrderPaths(), catalog, &err, opt );
		QApplication::restoreOverrideCursor();
	}
	const int added = int( order.size() - ticked.size() ) + int( missing.size() );
	if ( !missing.isEmpty() )
		status->setText( tr( "Missing master: %1" ).arg( missing.join( QStringLiteral( ", " ) ) ) );
	else if ( !err.isEmpty() )
		status->setText( err );
	else if ( added > 0 )
		status->setText( tr( "%n master(s) added", nullptr, added ) );
	else if ( !catalog.noStrings.isEmpty() )
		status->setText( tr( "No %1 strings for %2: names stay blank" )
			.arg( openCellStringsLanguage(), catalog.noStrings.join( QStringLiteral( ", " ) ) ) );
	else
		status->clear();
	// the filter's plugin is kept by name while it is still in the load order
	pluginIndex = keepPlugin.isEmpty() ? -1 : int( catalog.plugins.indexOf( keepPlugin ) );
	fillPluginFilter();
	fillWorlds();
}

void OpenCellDialog::fillWorlds()
{
	const QString keep = worldCombo->currentData( Qt::UserRole + 1 ).toString();
	QSignalBlocker block( worldCombo );
	worldCombo->clear();
	for ( const OpenCellWorld & w : catalog.worlds ) {
		const QString text = w.name.isEmpty() ? w.edid : QStringLiteral( "%1 (%2)" ).arg( w.edid, w.name );
		worldCombo->addItem( text, w.form );
		worldCombo->setItemData( worldCombo->count() - 1, w.edid, Qt::UserRole + 1 );
	}
	if ( !catalog.interiors.isEmpty() ) {
		worldCombo->addItem( tr( interiorItem ), 0U );
		worldCombo->setItemData( worldCombo->count() - 1, QLatin1String( interiorItem ), Qt::UserRole + 1 );
	}
	if ( worldCombo->count() && !oldList() ) {
		worldCombo->addItem( tr( allItem ), 0U );
		worldCombo->setItemData( worldCombo->count() - 1, QLatin1String( allItem ), Qt::UserRole + 1 );
	}
	const int i = worldCombo->findData( keep, Qt::UserRole + 1 );
	worldCombo->setCurrentIndex( i >= 0 ? i : 0 );
	block.unblock();
	fillCells();
}

bool OpenCellDialog::allMode() const
{
	return worldCombo->currentData( Qt::UserRole + 1 ).toString() == QLatin1String( allItem );
}

bool OpenCellDialog::interiorMode() const
{
	if ( worldCombo->currentData( Qt::UserRole + 1 ).toString() == QLatin1String( interiorItem ) )
		return true;
	if ( !allMode() )
		return false;
	const auto sel = cellTree->selectedItems();
	return !sel.isEmpty() && sel.first()->data( 0, RoleInterior ).toBool();
}

QString OpenCellDialog::exteriorWorld() const
{
	if ( !allMode() )
		return worldCombo->currentData( Qt::UserRole + 1 ).toString();
	const auto sel = cellTree->selectedItems();
	return sel.isEmpty() ? QString() : sel.first()->data( 0, RoleWorld ).toString();
}

void OpenCellDialog::fillCellsOld()
{
	const bool interior = interiorMode();
	const QString f = filterEdit->text().trimmed();
	QSignalBlocker block( cellTree );
	cellTree->clear();
	for ( int c = 0; c < ColCount; c++ )
		cellTree->setColumnHidden( c, false );
	cellTree->setHeaderLabels( interior ? QStringList{ tr( "Editor ID" ), tr( "Name" ), tr( "Form" ) }
		: QStringList{ tr( "Editor ID" ), tr( "Name" ), tr( "X" ), tr( "Y" ) } );
	const QVector<OpenCellCell> & list = interior ? catalog.interiors
		: catalog.cells.value( worldCombo->currentData().toUInt() );
	QList<QTreeWidgetItem *> rows;
	for ( const OpenCellCell & c : list ) {
		const QString xy = QStringLiteral( "%1,%2" ).arg( c.x ).arg( c.y );
		if ( !f.isEmpty() && !c.edid.contains( f, Qt::CaseInsensitive ) && !c.name.contains( f, Qt::CaseInsensitive )
			&& !( !interior && xy == QString( f ).remove( QLatin1Char( ' ' ) ) ) )
			continue;
		QStringList cols{ c.edid, c.name };
		if ( interior )
			cols << formHex( c.form );
		else
			cols << QString::number( c.x ) << QString::number( c.y );
		auto * it = new QTreeWidgetItem( cols );
		it->setData( 0, RoleForm, c.form );
		it->setData( 0, RoleXY, xy );
		it->setData( 0, RoleEdid, c.edid );
		it->setData( 0, RoleWorld, interior ? QString() : worldCombo->currentData( Qt::UserRole + 1 ).toString() );
		it->setData( 0, RoleInterior, interior );
		rows.append( it );
	}
	cellTree->addTopLevelItems( rows );
	for ( int i = 0; i < cellTree->columnCount(); i++ )
		cellTree->resizeColumnToContents( i );
	totalRows = list.size();
	countLabel->setText( tr( "%1 of %2 cells" ).arg( rows.size() ).arg( totalRows ) );
	xyEdit->setEnabled( !interior );
	sizeCombo->setEnabled( !interior );
}

void OpenCellDialog::fillCells()
{
	searchTimer->stop();
	refreshFilterButton();
	if ( oldList() ) {
		fillCellsOld();
		return;
	}
	const QString key = worldCombo->currentData( Qt::UserRole + 1 ).toString();
	const bool interiorWorld = key == QLatin1String( interiorItem );
	const bool all = key == QLatin1String( allItem );

	// Which rows the selector holds, in the catalog's order.
	QVector<Row> rows;
	auto addWorld = [&]( const OpenCellWorld & w ) {
		const auto found = catalog.cells.constFind( w.form );
		if ( found == catalog.cells.constEnd() )
			return;
		for ( const OpenCellCell & c : *found )
			rows.append( Row{ &c, w.edid, false, int( rows.size() ) } );
	};
	if ( all ) {
		for ( const OpenCellWorld & w : catalog.worlds )
			addWorld( w );
	} else if ( !interiorWorld ) {
		for ( const OpenCellWorld & w : catalog.worlds )
			if ( w.edid == key ) {
				addWorld( w );
				break;
			}
	}
	if ( all || interiorWorld )
		for ( const OpenCellCell & c : catalog.interiors )
			rows.append( Row{ &c, QString(), true, int( rows.size() ) } );
	totalRows = rows.size();

	// The search: x, y names a grid cell; otherwise every term must match a field Search In ticks.
	const QString text = filterEdit->text().simplified();
	int gx = 0, gy = 0;
	const bool gridSearch = parseXY( QString( text ).remove( QLatin1Char( ' ' ) ), gx, gy );
	const QStringList terms = text.split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
	const QString wilderness = tr( "Wilderness" );
	auto shownEdid = [&]( const Row & r ) {
		return ( r.c->edid.isEmpty() && !r.interior ) ? wilderness : r.c->edid;
	};
	auto matches = [&]( const Row & r ) {
		if ( cellsKind == 1 && !r.interior )
			return false;
		if ( cellsKind == 2 && r.interior )
			return false;
		if ( nameKind == 1 && r.c->name.isEmpty() )
			return false;
		if ( nameKind == 2 && !r.c->name.isEmpty() )
			return false;
		if ( pluginIndex >= 0 && ( pluginLastEdit ? r.c->lastEdit : r.c->owner ) != pluginIndex )
			return false;
		if ( terms.isEmpty() )
			return true;
		if ( gridSearch )
			return !r.interior && r.c->x == gx && r.c->y == gy;
		const QString form = formHex( r.c->shownForm );
		for ( const QString & t : terms ) {
			const QString formTerm = t.startsWith( QLatin1String( "0x" ), Qt::CaseInsensitive ) ? t.mid( 2 ) : t;
			const bool hit = ( ( searchFields & SearchEditorId ) && shownEdid( r ).contains( t, Qt::CaseInsensitive ) )
				|| ( ( searchFields & SearchName ) && r.c->name.contains( t, Qt::CaseInsensitive ) )
				|| ( ( searchFields & SearchFormId ) && form.contains( formTerm, Qt::CaseInsensitive ) );
			if ( !hit )
				return false;
		}
		return true;
	};
	QVector<Row> shown;
	for ( const Row & r : rows )
		if ( matches( r ) )
			shown.append( r );

	// A header click sorts; X, Y and FormID as numbers, the rest as text; ties keep the catalog's order.
	if ( sortColumn >= 0 ) {
		auto keyNum = [&]( const Row & r ) -> qint64 {
			switch ( sortColumn ) {
			case ColX: return r.interior ? std::numeric_limits<qint64>::max() : r.c->x;
			case ColY: return r.interior ? std::numeric_limits<qint64>::max() : r.c->y;
			case ColForm: return qint64( r.c->shownForm );
			default: return 0;
			}
		};
		auto keyText = [&]( const Row & r ) -> QString {
			switch ( sortColumn ) {
			case ColEditorId: return shownEdid( r );
			case ColName: return r.c->name;
			case ColWorld: return r.world;
			case ColOwner: return catalog.plugins.value( r.c->owner );
			case ColLastEdit: return catalog.plugins.value( r.c->lastEdit );
			default: return QString();
			}
		};
		const bool numeric = sortColumn == ColX || sortColumn == ColY || sortColumn == ColForm;
		std::stable_sort( shown.begin(), shown.end(), [&]( const Row & a, const Row & b ) {
			int d = 0;
			if ( numeric ) {
				const qint64 ka = keyNum( a ), kb = keyNum( b );
				d = ka < kb ? -1 : ka > kb ? 1 : 0;
			} else {
				d = keyText( a ).compare( keyText( b ), Qt::CaseInsensitive );
			}
			if ( sortDescending )
				d = -d;
			return d != 0 ? d < 0 : a.order < b.order;
		} );
	}

	QSignalBlocker block( cellTree );
	cellTree->setUpdatesEnabled( false );
	cellTree->clear();
	cellTree->setColumnCount( ColCount );
	cellTree->setHeaderLabels( { tr( "Editor ID" ), tr( "Name" ), tr( "X" ), tr( "Y" ), tr( "FormID" ), tr( "Worldspace" ),
		tr( "Plugin" ), tr( "Last Edit" ) } );
	cellTree->setColumnHidden( ColX, interiorWorld );
	cellTree->setColumnHidden( ColY, interiorWorld );
	cellTree->setColumnHidden( ColWorld, !all );
	cellTree->setColumnHidden( ColForm, false );
	cellTree->setColumnHidden( ColOwner, false );
	cellTree->setColumnHidden( ColLastEdit, false );
	const QColor muted = cellTree->palette().color( QPalette::PlaceholderText );
	QList<QTreeWidgetItem *> items;
	items.reserve( shown.size() );
	for ( const Row & r : shown ) {
		const OpenCellCell & c = *r.c;
		const QString xy = QStringLiteral( "%1,%2" ).arg( c.x ).arg( c.y );
		const QStringList cols{ shownEdid( r ), c.name, r.interior ? QString() : QString::number( c.x ),
			r.interior ? QString() : QString::number( c.y ), formHex( c.shownForm ), r.world,
			catalog.plugins.value( c.owner ), catalog.plugins.value( c.lastEdit ) };
		auto * it = new QTreeWidgetItem( cols );
		it->setData( 0, RoleForm, c.form );
		it->setData( 0, RoleXY, xy );
		it->setData( 0, RoleEdid, c.edid );
		it->setData( 0, RoleWorld, r.world );
		it->setData( 0, RoleInterior, r.interior );
		if ( c.edid.isEmpty() && !r.interior )
			it->setForeground( ColEditorId, muted );   // the CK's "Wilderness": a grid cell with no editor ID
		for ( int col : { int( ColX ), int( ColY ), int( ColForm ) } )
			it->setTextAlignment( col, Qt::AlignRight | Qt::AlignVCenter );
		items.append( it );
	}
	cellTree->addTopLevelItems( items );
	for ( int i = 0; i < ColCount; i++ )
		if ( !cellTree->isColumnHidden( i ) )
			cellTree->resizeColumnToContents( i );
	cellTree->header()->setSortIndicatorShown( sortColumn >= 0 );
	if ( sortColumn >= 0 )
		cellTree->header()->setSortIndicator( sortColumn, sortDescending ? Qt::DescendingOrder : Qt::AscendingOrder );
	cellTree->setUpdatesEnabled( true );
	countLabel->setText( tr( "%1 of %2 cells" ).arg( items.size() ).arg( totalRows ) );
	xyEdit->setEnabled( !interiorWorld );
	sizeCombo->setEnabled( !interiorWorld );
}

void OpenCellDialog::refreshFilterButton()
{
	int active = 0;
	active += searchFields != ( SearchEditorId | SearchName | SearchFormId );
	active += cellsKind != 0;
	active += nameKind != 0;
	active += pluginIndex >= 0;
	filterButton->setText( active ? tr( "Filters (%1)" ).arg( active ) : tr( "Filters" ) );
	QStringList what;
	if ( cellsKind )
		what << ( cellsKind == 1 ? tr( "interior only" ) : tr( "exterior only" ) );
	if ( nameKind )
		what << ( nameKind == 1 ? tr( "with a name" ) : tr( "without a name" ) );
	if ( pluginIndex >= 0 )
		what << ( pluginLastEdit ? tr( "last edited by %1" ) : tr( "added by %1" ) ).arg( catalog.plugins.value( pluginIndex ) );
	filterButton->setToolTip( what.isEmpty() ? tr( "Search filters" )
		: tr( "Search filters: %1" ).arg( what.join( QStringLiteral( ", " ) ) ) );
	if ( pluginMenu )
		pluginMenu->setTitle( pluginIndex >= 0 ? catalog.plugins.value( pluginIndex ) : tr( "Any plugin" ) );
}

void OpenCellDialog::cellPicked()
{
	const auto sel = cellTree->selectedItems();
	if ( !sel.isEmpty() && !sel.first()->data( 0, RoleInterior ).toBool() )
		xyEdit->setText( sel.first()->data( 0, RoleXY ).toString() );
}

QStringList OpenCellDialog::worldItems() const
{
	QStringList out;
	for ( int i = 0; i < worldCombo->count(); i++ )
		out.append( worldCombo->itemText( i ) );
	return out;
}

QStringList OpenCellDialog::cellItems() const
{
	QStringList out;
	for ( int i = 0; i < cellTree->topLevelItemCount(); i++ ) {
		const QTreeWidgetItem * it = cellTree->topLevelItem( i );
		QStringList cols;
		for ( int c = 0; c < cellTree->columnCount(); c++ )
			if ( !cellTree->isColumnHidden( c ) )
				cols.append( it->text( c ) );
		out.append( cols.join( QLatin1Char( '\t' ) ) );
	}
	return out;
}

QStringList OpenCellDialog::cellHeaders() const
{
	QStringList out;
	for ( int c = 0; c < cellTree->columnCount(); c++ )
		if ( !cellTree->isColumnHidden( c ) )
			out.append( cellTree->headerItem()->text( c ) );
	return out;
}

int OpenCellDialog::cellCount() const
{
	return cellTree->topLevelItemCount();
}

int OpenCellDialog::cellTotal() const
{
	return totalRows;
}

void OpenCellDialog::setSearch( const QString & text )
{
	{
		QSignalBlocker b( filterEdit );
		filterEdit->setText( text );
	}
	fillCells();
}

void OpenCellDialog::sortCells( int column, bool descending )
{
	sortColumn = column;
	sortDescending = descending;
	fillCells();
}

void OpenCellDialog::selectWorld( const QString & edid )
{
	const int i = worldCombo->findData( edid, Qt::UserRole + 1 );
	if ( i >= 0 )
		worldCombo->setCurrentIndex( i );
}

void OpenCellDialog::selectCell( int x, int y, int n )
{
	xyEdit->setText( QStringLiteral( "%1,%2" ).arg( x ).arg( y ) );
	sizeCombo->setCurrentIndex( qMax( 0, sizeCombo->findData( n ) ) );
}

void OpenCellDialog::selectInterior( const QString & edid )
{
	for ( int i = 0; i < cellTree->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = cellTree->topLevelItem( i );
		if ( it->data( 0, RoleInterior ).toBool()
			&& it->data( 0, RoleEdid ).toString().compare( edid, Qt::CaseInsensitive ) == 0 )
			cellTree->setCurrentItem( it );
	}
}

QString OpenCellDialog::specLine() const
{
	const QStringList paths = loadOrderPaths();
	if ( paths.isEmpty() || worldCombo->currentIndex() < 0 )
		return QString();
	if ( interiorMode() ) {
		const auto sel = cellTree->selectedItems();
		if ( sel.isEmpty() )
			return QString();
		// An interior without an editor ID is named by its form, which the loader also accepts.
		const QString edid = sel.first()->data( 0, RoleEdid ).toString();
		const QString cell = edid.isEmpty() ? formHex( sel.first()->data( 0, RoleForm ).toUInt() ) : edid;
		return openCellInteriorLine( paths, cell );
	}
	int x = 0, y = 0;
	const QString world = exteriorWorld();
	if ( world.isEmpty() || !parseXY( xyEdit->text(), x, y ) )
		return QString();
	return openCellExteriorLine( paths, world, x, y, sizeCombo->currentData().toInt() );
}

bool OpenCellDialog::writeSpec( const QString & dir )
{
	const QString line = specLine();
	if ( line.isEmpty() ) {
		status->setText( interiorMode() ? tr( "Pick an interior cell" )
			: ( allMode() && exteriorWorld().isEmpty() ) ? tr( "Pick a cell" ) : tr( "Enter x, y" ) );
		return false;
	}
	QString name;
	if ( interiorMode() ) {
		name = openCellInteriorFileName( line.section( QLatin1Char( '|' ), 2, 2 ) );
	} else {
		int x = 0, y = 0;
		parseXY( xyEdit->text(), x, y );
		name = openCellFileName( exteriorWorld(), x, y, sizeCombo->currentData().toInt() );
	}
	const QString path = QDir( dir ).filePath( name );
	QString err;
	if ( !openCellWriteSpec( path, line, &err ) ) {
		status->setText( err );
		return false;
	}
	written = path;
	openCellRecentAdd( path );
	return true;
}

void OpenCellDialog::openClicked()
{
	if ( !writeSpec( openCellSpecDir() ) )
		return;
	saveSettings();
	accept();
}

void OpenCellDialog::saveSettings() const
{
	QSettings s;
	s.setValue( key( "DataDir" ), sources.dataDir );
	s.setValue( key( "ModDirs" ), sources.modDirs );
	QStringList order, ticked;
	for ( int i = 0; i < pluginList->count(); i++ ) {
		order.append( pluginList->item( i )->text() );
		if ( pluginList->item( i )->checkState() == Qt::Checked )
			ticked.append( pluginList->item( i )->text() );
	}
	s.setValue( key( "Ticked" ), ticked );
	s.setValue( key( "World" ), worldCombo->currentData( Qt::UserRole + 1 ) );
	s.setValue( key( "XY" ), xyEdit->text() );
	s.setValue( key( "Size" ), sizeCombo->currentData() );
}

void OpenCellDialog::loadSettings()
{
	QSettings s;
	const QString d = s.value( key( "DataDir" ) ).toString();
	if ( d.isEmpty() )
		return;
	sources.modDirs = s.value( key( "ModDirs" ) ).toStringList();
	setDataDir( d );
	setPluginsTicked( s.value( key( "Ticked" ) ).toStringList() );
	readNow();
	selectWorld( s.value( key( "World" ) ).toString() );
	xyEdit->setText( s.value( key( "XY" ) ).toString() );
	sizeCombo->setCurrentIndex( qMax( 0, sizeCombo->findData( s.value( key( "Size" ), 1 ) ) ) );
}

QString openCellShot( QWidget * parent, const QString & png )
{
	auto * dlg = new OpenCellDialog( parent );
	dlg->setAttribute( Qt::WA_DeleteOnClose );
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_DATA" ) )
		dlg->setDataDir( wwEnvStr( "WW_OPENCELL_SHOT_DATA" ) );
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_PLUGINS" ) )
		dlg->setPluginsTicked( wwEnvStr( "WW_OPENCELL_SHOT_PLUGINS" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
	dlg->readNow();
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_WORLD" ) )
		dlg->selectWorld( wwEnvStr( "WW_OPENCELL_SHOT_WORLD" ) );
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_SEARCH" ) )
		dlg->setSearch( wwEnvStr( "WW_OPENCELL_SHOT_SEARCH" ) );
	dlg->show();
	for ( int i = 0; i < 3; i++ )
		QApplication::processEvents();
	const bool ok = dlg->grab().save( png );
	bool menuOk = false;
	if ( auto * btn = dlg->findChild<QToolButton *>( QStringLiteral( "OpenCellFilters" ) ) ) {
		if ( QMenu * m = btn->menu() ) {
			m->popup( btn->mapToGlobal( QPoint( 0, btn->height() ) ) );
			for ( int i = 0; i < 3; i++ )
				QApplication::processEvents();
			menuOk = m->grab().save( png + QStringLiteral( ".filters.png" ) );
			m->hide();
		}
	}
	const QString line = QStringLiteral( "opencell shot: %1 (%2), filters %3; %4 of %5 cells, worlds %6" )
		.arg( png, ok ? QStringLiteral( "written" ) : QStringLiteral( "NOT SAVED" ),
			menuOk ? QStringLiteral( "written" ) : QStringLiteral( "NOT SAVED" ) )
		.arg( dlg->cellCount() ).arg( dlg->cellTotal() ).arg( dlg->worldItems().size() );
	dlg->close();
	return line;
}
