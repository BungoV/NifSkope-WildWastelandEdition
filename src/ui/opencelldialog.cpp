/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencelldialog.h"
#include "wwswitch.h"	// lane CELLSPAWN1: WW_OPENCELL_RED=list, the shot harness
#include "widgets/wwdropdown.h"	// lane OPENCELLFIX2: the dock's one dropdown

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

// src/wwskin.h: a section heading
QLabel * wwHeading( const QString & text, QWidget * parent );

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

//! lane OPENCELLFIX1: a load-order file "as it was": its size and time, so a changed plugins.txt is read again
QString stampOf( const QString & path )
{
	const QFileInfo fi( path );
	if ( !fi.isFile() )
		return QString();
	return QStringLiteral( "%1@%2" ).arg( fi.size() ).arg( fi.lastModified().toMSecsSinceEpoch() );
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

//! One label width for both pages, so the value edge is one line down the dock (skill nifskope-ww-panel-style).
constexpr int kLabelW = 84;

QGridLayout * formGrid()
{
	auto * g = new QGridLayout;
	g->setContentsMargins( 0, 0, 0, 0 );
	g->setHorizontalSpacing( 6 );
	g->setVerticalSpacing( 4 );
	g->setColumnMinimumWidth( 0, kLabelW );
	g->setColumnStretch( 1, 1 );
	return g;
}

} // namespace

OpenCellPicker::OpenCellPicker( QWidget * parent ) : QWidget( parent )
{
	build();
	loadSettings();
}

void OpenCellPicker::build()
{
	auto * outer = new QVBoxLayout( this );
	outer->setContentsMargins( 0, 0, 0, 0 );
	loPage = new QWidget( this );
	loPage->setObjectName( QStringLiteral( "OpenCellLoadOrderPage" ) );
	cellPage = new QWidget( this );
	cellPage->setObjectName( QStringLiteral( "OpenCellCellsPage" ) );
	outer->addWidget( loPage );
	outer->addWidget( cellPage, 1 );

	// ---- Load order: where the plugins are, which ones load, in what order
	auto * lo = new QVBoxLayout( loPage );
	lo->setContentsMargins( 6, 6, 6, 6 );
	lo->setSpacing( 4 );
	auto * g = formGrid();
	int row = 0;
	auto label = [&]( QGridLayout * grid, const QString & t ) {
		grid->addWidget( new QLabel( t ), row, 0, Qt::AlignLeft | Qt::AlignVCenter );
	};
	label( g, tr( "Data folder" ) );
	auto * dataRow = new QHBoxLayout;
	dataEdit = new QLineEdit( loPage );
	dataEdit->setObjectName( QStringLiteral( "OpenCellData" ) );
	dataEdit->setPlaceholderText( tr( "Fallout 4\\Data" ) );
	auto * browse = new QToolButton( loPage );
	browse->setText( QStringLiteral( "..." ) );
	browse->setToolTip( tr( "Pick the game's Data folder" ) );
	dataRow->addWidget( dataEdit, 1 );
	dataRow->addWidget( browse );
	g->addLayout( dataRow, row++, 1 );

	label( g, tr( "Load order" ) );
	auto * orderRow = new QHBoxLayout;
	orderEdit = new QLineEdit( loPage );
	orderEdit->setObjectName( QStringLiteral( "OpenCellLoadOrderFile" ) );
	orderEdit->setReadOnly( true );
	orderEdit->setPlaceholderText( tr( "plugins.txt" ) );
	orderEdit->setToolTip( tr( "A Mod Organizer profile's plugins.txt, or the game's (%LOCALAPPDATA%\\Fallout4)" ) );
	auto * import = new QToolButton( loPage );
	import->setObjectName( QStringLiteral( "OpenCellImport" ) );
	import->setText( QStringLiteral( "..." ) );
	import->setToolTip( tr( "Pick a plugins.txt" ) );
	orderRow->addWidget( orderEdit, 1 );
	orderRow->addWidget( import );
	g->addLayout( orderRow, row++, 1 );
	// lane OPENCELLFIX2 (bungo 10-27): no language box; the game's sLanguage picks the string tables
	lo->addLayout( g );

	auto * plHead = new QHBoxLayout;
	plHead->addWidget( wwHeading( tr( "Plugins" ), loPage ) );
	plHead->addStretch( 1 );
	pluginCount = new QLabel( loPage );
	pluginCount->setObjectName( QStringLiteral( "OpenCellPluginCount" ) );
	plHead->addWidget( pluginCount );
	lo->addLayout( plHead );
	pluginList = new QListWidget( loPage );
	pluginList->setObjectName( QStringLiteral( "OpenCellPlugins" ) );
	pluginList->setDragDropMode( QAbstractItemView::InternalMove );   // the list order is the load order
	pluginList->setMinimumHeight( 120 );
	lo->addWidget( pluginList, 1 );
	loStatus = new QLabel( loPage );
	loStatus->setObjectName( QStringLiteral( "OpenCellLoadOrderStatus" ) );
	loStatus->setWordWrap( true );
	loStatus->hide();
	lo->addWidget( loStatus );

	// ---- Cells: the CK's Cell View
	auto * cl = new QVBoxLayout( cellPage );
	cl->setContentsMargins( 6, 6, 6, 6 );
	cl->setSpacing( 4 );
	auto * cg = formGrid();
	row = 0;
	label( cg, tr( "Worldspace" ) );
	worldCombo = new WwDropdown( cellPage );	// OPENCELLFIX2: the Object Mode selector's look (note 7)
	worldCombo->setObjectName( QStringLiteral( "OpenCellWorld" ) );
	cg->addWidget( worldCombo, row++, 1 );

	// The block list's search bar: one field, a Filters menu beside it (src/nifskope.cpp, BlockListToolbar).
	label( cg, tr( "Cell" ) );
	auto * searchRow = new QHBoxLayout;
	searchRow->setSpacing( 2 );
	filterEdit = new QLineEdit( cellPage );
	filterEdit->setObjectName( QStringLiteral( "OpenCellSearch" ) );
	filterEdit->setPlaceholderText( tr( "Search cells…" ) );
	filterEdit->setClearButtonEnabled( true );
	filterEdit->setToolTip( tr( "Editor ID, name or FormID; space-separated terms must all match. x, y finds a grid cell." ) );
	searchRow->addWidget( filterEdit, 1 );
	filterButton = new QToolButton( cellPage );
	filterButton->setObjectName( QStringLiteral( "OpenCellFilters" ) );
	WwDropdown::applyStyle( filterButton );
	filterButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
	filterButton->setAccessibleName( tr( "Cell search filters" ) );
	searchRow->addWidget( filterButton );
	cg->addLayout( searchRow, row++, 1 );
	cl->addLayout( cg );

	cellTree = new QTreeWidget( cellPage );
	cellTree->setObjectName( QStringLiteral( "OpenCellList" ) );
	cellTree->setRootIsDecorated( false );
	cellTree->setUniformRowHeights( true );
	cellTree->setMinimumHeight( 160 );
	cellTree->setColumnCount( ColCount );
	cellTree->header()->setStretchLastSection( false );
	cellTree->header()->setSectionsClickable( true );
	cl->addWidget( cellTree, 1 );
	countLabel = new QLabel( cellPage );
	countLabel->setObjectName( QStringLiteral( "OpenCellCount" ) );
	cl->addWidget( countLabel );

	auto * ag = formGrid();
	row = 0;
	label( ag, tr( "X, Y" ) );
	auto * at = new QHBoxLayout;
	xyEdit = new QLineEdit( cellPage );
	xyEdit->setObjectName( QStringLiteral( "OpenCellXY" ) );
	xyEdit->setPlaceholderText( QStringLiteral( "x, y" ) );
	sizeCombo = new WwDropdown( cellPage );
	sizeCombo->setObjectName( QStringLiteral( "OpenCellSize" ) );
	for ( int n : { 1, 3, 5 } )
		sizeCombo->addItem( QStringLiteral( "%1 x %1" ).arg( n ), n );
	at->addWidget( xyEdit, 1 );
	at->addWidget( new QLabel( tr( "Size" ), cellPage ) );
	at->addWidget( sizeCombo );
	ag->addLayout( at, row++, 1 );
	cl->addLayout( ag );

	status = new QLabel( cellPage );
	status->setObjectName( QStringLiteral( "OpenCellStatus" ) );
	status->setWordWrap( true );
	status->hide();
	cl->addWidget( status );
	// lane OPENCELLFIX2 (bungo 10-27): no Open button; a double-click (or Enter) on a cell opens it

	readTimer = new QTimer( this );
	readTimer->setSingleShot( true );
	readTimer->setInterval( 400 );     // ticking several plugins in a row reads once
	searchTimer = new QTimer( this );
	searchTimer->setSingleShot( true );
	searchTimer->setInterval( 150 );   // a whole worldspace is thousands of rows: list once per pause in typing
	dataTimer = new QTimer( this );
	dataTimer->setSingleShot( true );
	dataTimer->setInterval( 500 );     // a typed or pasted Data folder counts once it names a folder
	buildFilters();

	connect( browse, &QToolButton::clicked, this, &OpenCellPicker::browseData );
	connect( import, &QToolButton::clicked, this, &OpenCellPicker::browseImport );
	// lane OPENCELLFIX1: a Data folder counts as soon as it exists, not only after Enter (it was lost on a paste)
	connect( dataEdit, &QLineEdit::textEdited, this, [this]() { dataTimer->start(); } );
	connect( dataTimer, &QTimer::timeout, this, [this]() {
		if ( QFileInfo( dataEdit->text() ).isDir() )
			setDataDir( dataEdit->text() );
	} );
	connect( dataEdit, &QLineEdit::editingFinished, this, [this]() {
		dataTimer->stop();
		if ( QDir::cleanPath( dataEdit->text() ) != sources.dataDir )
			setDataDir( dataEdit->text() );
	} );
	connect( pluginList, &QListWidget::itemChanged, this, &OpenCellPicker::pluginsChanged );
	connect( pluginList->model(), &QAbstractItemModel::rowsMoved, this, &OpenCellPicker::pluginsChanged );
	connect( readTimer, &QTimer::timeout, this, &OpenCellPicker::readNow );
	connect( worldCombo, &WwDropdown::currentIndexChanged, this, [this]() { fillCells(); saveSettings(); } );
	connect( filterEdit, &QLineEdit::textChanged, this, [this]() { searchTimer->start(); } );
	connect( searchTimer, &QTimer::timeout, this, &OpenCellPicker::fillCells );
	connect( cellTree->header(), &QHeaderView::sectionClicked, this, [this]( int col ) {
		sortCells( col, col == sortColumn ? !sortDescending : false );
	} );
	connect( cellTree, &QTreeWidget::itemSelectionChanged, this, &OpenCellPicker::cellPicked );
	if ( wwEnvStr( "WW_OPENCELL_RED" ) != QLatin1String( "dblclick" ) ) {	// red: the double-click opens nothing
		// the clicked row is made current first, so the cell opened is the one under the pointer
		connect( cellTree, &QTreeWidget::itemDoubleClicked, this, [this]( QTreeWidgetItem * it ) {
			if ( !it )
				return;
			if ( cellTree->currentItem() != it )
				cellTree->setCurrentItem( it );
			openClicked();
		} );
	}
	connect( xyEdit, &QLineEdit::textChanged, this, [this]() { saveSettings(); } );
	connect( sizeCombo, &WwDropdown::currentIndexChanged, this, [this]() { saveSettings(); } );
	cellPage->installEventFilter( this );
	cellTree->installEventFilter( this );
}

bool OpenCellPicker::eventFilter( QObject * o, QEvent * e )
{
	// The catalog is read when the Cells page is first looked at, not at start: a full master takes seconds.
	if ( o == cellPage && e->type() == QEvent::Show && readOwed )
		readTimer->start( 0 );
	if ( o == cellTree && e->type() == QEvent::KeyPress && cellTree->currentItem() ) {	// Enter opens, as the button did
		const int k = static_cast<QKeyEvent *>( e )->key();
		if ( k == Qt::Key_Return || k == Qt::Key_Enter ) {
			openClicked();
			return true;
		}
	}
	return QWidget::eventFilter( o, e );
}

void OpenCellPicker::buildFilters()
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

void OpenCellPicker::fillPluginFilter()
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

void OpenCellPicker::setDataDir( const QString & dir )
{
	const QString d = QDir::cleanPath( dir );
	if ( dataEdit->text() != d && !( d == QLatin1String( "." ) && dataEdit->text().isEmpty() ) ) {
		QSignalBlocker b( dataEdit );
		dataEdit->setText( d );
	}
	if ( sources.dataDir == d && pluginList->count() )
		return;
	sources.dataDir = d;
	if ( !restoring && orderFile.isEmpty() ) {	// a hand-made list: no profile's mod folders any more
		sources.modDirs.clear();
		sources.resources.clear();
	}
	const QStringList keep = tickedPlugins();
	{
		QSignalBlocker block( pluginList );
		pluginList->clear();
		for ( const QString & name : openCellDataPlugins( d ) ) {
			auto * it = new QListWidgetItem( name, pluginList );
			it->setFlags( it->flags() | Qt::ItemIsUserCheckable );
			it->setCheckState( Qt::Unchecked );
		}
	}
	if ( !keep.isEmpty() )
		setPluginsTicked( keep );   // the ticks survive a Data folder change
	else
		pluginsChanged();
}

void OpenCellPicker::setPluginsTicked( const QStringList & names )
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

QString OpenCellPicker::stringsLanguage() const
{
	return openCellStringsLanguage( orderFile.isEmpty() ? QString() : QFileInfo( orderFile ).absolutePath() );
}

bool OpenCellPicker::importLoadOrder( const QString & pluginsTxt )
{
	QStringList plugins;
	QString err;
	OpenCellSources next = sources;
	if ( !openCellImportLoadOrder( pluginsTxt, plugins, next, &err ) ) {
		setStatus( err );
		return false;
	}
	orderFile = QDir::cleanPath( QFileInfo( pluginsTxt ).absoluteFilePath() );
	orderEdit->setText( QDir::toNativeSeparators( orderFile ) );
	sources.modDirs = next.modDirs;
	sources.resources = next.resources;
	if ( next.dataDir != sources.dataDir || !pluginList->count() ) {
		{
			QSignalBlocker b( pluginList );
			pluginList->clear();	// the file's ticks replace the old ones
		}
		sources.dataDir = QString();
		setDataDir( next.dataDir );
	}
	setPluginsTicked( plugins );
	setStatus( err );
	saveSettings();
	return true;
}

void OpenCellPicker::browseData()
{
	const QString d = QFileDialog::getExistingDirectory( this, tr( "Data folder" ), dataEdit->text() );
	if ( !d.isEmpty() )
		setDataDir( d );
}

void OpenCellPicker::browseImport()
{
	const QString f = QFileDialog::getOpenFileName( this, tr( "Load order" ),
		orderFile.isEmpty() ? sources.dataDir : QFileInfo( orderFile ).absolutePath(), tr( "Load order (plugins.txt *.txt)" ) );
	if ( !f.isEmpty() )
		importLoadOrder( f );
}

void OpenCellPicker::pluginsChanged()
{
	readOwed = true;
	refreshCount();
	saveSettings();
	if ( cellPage->isVisible() )
		readTimer->start();
}

void OpenCellPicker::refreshCount()
{
	pluginCount->setText( tr( "%1 ticked of %2" ).arg( tickedPlugins().size() ).arg( pluginList->count() ) );
}

QStringList OpenCellPicker::tickedPlugins() const
{
	QStringList ticked;
	for ( int i = 0; i < pluginList->count(); i++ )
		if ( pluginList->item( i )->checkState() == Qt::Checked )
			ticked.append( pluginList->item( i )->text() );
	return ticked;
}

QStringList OpenCellPicker::missingMasters() const
{
	QStringList missing;
	openCellWithMasters( tickedPlugins(), sources, &missing );
	return missing;
}

QStringList OpenCellPicker::loadOrderPaths() const
{
	QStringList missing;
	QStringList paths;
	for ( const QString & n : openCellWithMasters( tickedPlugins(), sources, &missing ) )
		paths.append( sources.resolve( n ) );
	return paths;
}

void OpenCellPicker::readNow()
{
	readTimer->stop();
	readOwed = false;
	const QStringList ticked = tickedPlugins();
	QStringList missing;
	const QStringList order = openCellWithMasters( ticked, sources, &missing );
	QString err;
	const QString keepPlugin = pluginIndex >= 0 ? catalog.plugins.value( pluginIndex ) : QString();
	catalog = OpenCellCatalog();
	if ( !order.isEmpty() ) {
		QApplication::setOverrideCursor( Qt::WaitCursor );   // a full master takes seconds to index
		OpenCellReadOptions opt;
		opt.dataDir = sources.dataDir;
		opt.lang = stringsLanguage();	// the profile's own inis first, as the game reads them
		openCellReadCatalog( loadOrderPaths(), catalog, &err, opt );
		QApplication::restoreOverrideCursor();
	}
	const int added = int( order.size() - ticked.size() ) + int( missing.size() );
	if ( !missing.isEmpty() )
		setStatus( refusal() );
	else if ( !err.isEmpty() )
		setStatus( err );
	else if ( added > 0 )
		setStatus( tr( "%n master(s) added", nullptr, added ) );
	else if ( !catalog.noStrings.isEmpty() )
		setStatus( tr( "No %1 strings for %2: names stay blank" )
			.arg( stringsLanguage(), catalog.noStrings.join( QStringLiteral( ", " ) ) ) );
	else
		setStatus( QString() );
	// the filter's plugin is kept by name while it is still in the load order
	pluginIndex = keepPlugin.isEmpty() ? -1 : int( catalog.plugins.indexOf( keepPlugin ) );
	fillPluginFilter();
	const bool wasRestoring = restoring;
	restoring = true;	// the combo refills: what was saved stays saved
	fillWorlds();
	if ( !pendingWorld.isEmpty() ) {
		selectWorld( pendingWorld );
		pendingWorld.clear();
	}
	if ( !pendingCell.isEmpty() ) {
		selectInterior( pendingCell );
		pendingCell.clear();
	}
	restoring = wasRestoring;
}

void OpenCellPicker::fillWorlds()
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

bool OpenCellPicker::allMode() const
{
	return worldCombo->currentData( Qt::UserRole + 1 ).toString() == QLatin1String( allItem );
}

bool OpenCellPicker::interiorMode() const
{
	if ( worldCombo->currentData( Qt::UserRole + 1 ).toString() == QLatin1String( interiorItem ) )
		return true;
	if ( !allMode() )
		return false;
	const auto sel = cellTree->selectedItems();
	return !sel.isEmpty() && sel.first()->data( 0, RoleInterior ).toBool();
}

QString OpenCellPicker::exteriorWorld() const
{
	if ( !allMode() )
		return worldCombo->currentData( Qt::UserRole + 1 ).toString();
	const auto sel = cellTree->selectedItems();
	return sel.isEmpty() ? QString() : sel.first()->data( 0, RoleWorld ).toString();
}

void OpenCellPicker::fillCellsOld()
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

void OpenCellPicker::fillCells()
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

void OpenCellPicker::refreshFilterButton()
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

void OpenCellPicker::cellPicked()
{
	const auto sel = cellTree->selectedItems();
	if ( !sel.isEmpty() && !sel.first()->data( 0, RoleInterior ).toBool() )
		xyEdit->setText( sel.first()->data( 0, RoleXY ).toString() );
	saveSettings();
}

QStringList OpenCellPicker::worldItems() const
{
	QStringList out;
	for ( int i = 0; i < worldCombo->count(); i++ )
		out.append( worldCombo->itemText( i ) );
	return out;
}

QStringList OpenCellPicker::cellItems() const
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

QStringList OpenCellPicker::cellHeaders() const
{
	QStringList out;
	for ( int c = 0; c < cellTree->columnCount(); c++ )
		if ( !cellTree->isColumnHidden( c ) )
			out.append( cellTree->headerItem()->text( c ) );
	return out;
}

int OpenCellPicker::cellCount() const
{
	return cellTree->topLevelItemCount();
}

int OpenCellPicker::cellTotal() const
{
	return totalRows;
}

void OpenCellPicker::setSearch( const QString & text )
{
	{
		QSignalBlocker b( filterEdit );
		filterEdit->setText( text );
	}
	fillCells();
}

void OpenCellPicker::sortCells( int column, bool descending )
{
	sortColumn = column;
	sortDescending = descending;
	fillCells();
}

void OpenCellPicker::selectWorld( const QString & edid )
{
	const int i = worldCombo->findData( edid, Qt::UserRole + 1 );
	if ( i >= 0 )
		worldCombo->setCurrentIndex( i );
}

void OpenCellPicker::selectCell( int x, int y, int n )
{
	xyEdit->setText( QStringLiteral( "%1,%2" ).arg( x ).arg( y ) );
	sizeCombo->setCurrentIndex( qMax( 0, sizeCombo->findData( n ) ) );
}

void OpenCellPicker::selectInterior( const QString & edid )
{
	for ( int i = 0; i < cellTree->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = cellTree->topLevelItem( i );
		if ( it->data( 0, RoleInterior ).toBool()
			&& it->data( 0, RoleEdid ).toString().compare( edid, Qt::CaseInsensitive ) == 0 )
			cellTree->setCurrentItem( it );
	}
}

QString OpenCellPicker::specLine() const
{
	const QStringList paths = loadOrderPaths();
	if ( paths.isEmpty() || worldCombo->currentIndex() < 0 )
		return QString();
	// lane OPENCELLFIX1: a load order with a master missing is refused, never opened without it (0 references)
	if ( !missingMasters().isEmpty() && wwEnvStr( "WW_OPENCELL_RED" ) != QLatin1String( "masters" ) )
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

QString OpenCellPicker::refusal() const
{
	if ( tickedPlugins().isEmpty() )
		return tr( "Tick the plugins to load on the Load order tab" );
	const QStringList missing = missingMasters();
	if ( !missing.isEmpty() ) {
		const bool game = missing.contains( QStringLiteral( "Fallout4.esm" ), Qt::CaseInsensitive );
		return game ? tr( "Missing master: %1. Set the Data folder on the Load order tab" ).arg( missing.join( QStringLiteral( ", " ) ) )
			: tr( "Missing master: %1" ).arg( missing.join( QStringLiteral( ", " ) ) );
	}
	if ( worldCombo->currentIndex() < 0 )
		return tr( "No worldspace or interior in these plugins" );
	if ( interiorMode() )
		return tr( "Pick an interior cell" );
	if ( allMode() && exteriorWorld().isEmpty() )
		return tr( "Pick a cell" );
	return tr( "Enter x, y" );
}

bool OpenCellPicker::writeSpec( const QString & dir )
{
	const QString line = specLine();
	if ( line.isEmpty() ) {
		setStatus( refusal() );
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
	// lane OPENCELLFIX1: an MO2 profile's folders ride along on a second line, so the cell's models and textures
	// come from the same mods its plugins do (cellSpecFromFile reads it; older builds stop at the first line)
	QString text = line;
	if ( !sources.resources.isEmpty() )
		text += QStringLiteral( "\nresources|" ) + sources.resources.join( QLatin1Char( ';' ) );
	QString err;
	if ( !openCellWriteSpec( path, text, &err ) ) {
		setStatus( err );
		return false;
	}
	written = path;
	openCellRecentAdd( path );
	return true;
}

void OpenCellPicker::openClicked()
{
	if ( !writeSpec( openCellSpecDir() ) )
		return;
	setStatus( QString() );
	saveSettings();
	emit openRequested( written );
}

void OpenCellPicker::setStatus( const QString & text )
{
	status->setText( text );
	status->setVisible( !text.isEmpty() );
	loStatus->setText( text );
	loStatus->setVisible( !text.isEmpty() );
}

QString OpenCellPicker::statusText() const
{
	return status->text();
}

void OpenCellPicker::saveSettings() const
{
	if ( restoring )
		return;
	QSettings s;
	s.setValue( key( "DataDir" ), sources.dataDir );
	s.setValue( key( "LoadOrderFile" ), orderFile );
	s.setValue( key( "LoadOrderStamp" ), stampOf( orderFile ) );
	s.setValue( key( "ModDirs" ), sources.modDirs );
	s.setValue( key( "Resources" ), sources.resources );
	s.setValue( key( "Ticked" ), tickedPlugins() );
	if ( worldCombo->currentIndex() >= 0 )
		s.setValue( key( "World" ), worldCombo->currentData( Qt::UserRole + 1 ) );
	s.setValue( key( "XY" ), xyEdit->text() );
	s.setValue( key( "Size" ), sizeCombo->currentData() );
	const auto sel = cellTree->selectedItems();
	s.setValue( key( "Cell" ), !sel.isEmpty() && sel.first()->data( 0, RoleInterior ).toBool()
		? sel.first()->data( 0, RoleEdid ).toString() : QString() );
}

void OpenCellPicker::loadSettings()
{
	/* lane OPENCELLFIX1: everything comes back on every start. A saved Data folder or load-order file that is gone
	 * (or none saved yet) falls back to the LOD panel's MO2 profile; a plugins.txt changed since is read again.
	 * WW_OPENCELL_RED=restore: nothing comes back (the unit test's red). */
	if ( wwEnvStr( "WW_OPENCELL_RED" ) == QLatin1String( "restore" ) )
		return;
	restoring = true;
	QSettings s;
	QString d = s.value( key( "DataDir" ) ).toString();
	QString f = s.value( key( "LoadOrderFile" ) ).toString();
	const bool dataOk = !d.isEmpty() && QFileInfo( d ).isDir();
	const bool fileOk = f.isEmpty() || QFileInfo( f ).isFile();
	bool fell = false;
	if ( !dataOk || !fileOk ) {
		const OpenCellFallback fb = openCellFallback();
		if ( !dataOk )
			d = fb.dataDir;
		if ( !fileOk || ( !dataOk && f.isEmpty() ) )
			f = fb.pluginsTxt;
		fell = true;
	}
	sources.modDirs = s.value( key( "ModDirs" ) ).toStringList();
	sources.resources = s.value( key( "Resources" ) ).toStringList();
	if ( !f.isEmpty() && QFileInfo( f ).isFile() ) {
		orderFile = QDir::cleanPath( f );
		orderEdit->setText( QDir::toNativeSeparators( orderFile ) );
	}
	if ( !d.isEmpty() )
		setDataDir( d );
	if ( !orderFile.isEmpty() && ( fell || stampOf( orderFile ) != s.value( key( "LoadOrderStamp" ) ).toString() ) ) {
		restoring = false;
		importLoadOrder( orderFile );	// new or changed since: read it again
		restoring = true;
	} else if ( !fell ) {
		setPluginsTicked( s.value( key( "Ticked" ) ).toStringList() );
	}
	pendingWorld = s.value( key( "World" ) ).toString();
	pendingCell = s.value( key( "Cell" ) ).toString();
	xyEdit->setText( s.value( key( "XY" ) ).toString() );
	sizeCombo->setCurrentIndex( qMax( 0, sizeCombo->findData( s.value( key( "Size" ), 1 ) ) ) );
	restoring = false;
	refreshCount();
	if ( fell )
		saveSettings();
}

QString openCellShot( QWidget * parent, const QString & png )
{
	auto * dlg = new OpenCellPicker( nullptr );
	dlg->setAttribute( Qt::WA_DeleteOnClose );
	dlg->setWindowFlag( Qt::Tool );
	Q_UNUSED( parent );
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_DATA" ) )
		dlg->setDataDir( wwEnvStr( "WW_OPENCELL_SHOT_DATA" ) );
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_PLUGINS" ) )
		dlg->setPluginsTicked( wwEnvStr( "WW_OPENCELL_SHOT_PLUGINS" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
	dlg->readNow();
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_WORLD" ) )
		dlg->selectWorld( wwEnvStr( "WW_OPENCELL_SHOT_WORLD" ) );
	if ( wwEnvIsSet( "WW_OPENCELL_SHOT_SEARCH" ) )
		dlg->setSearch( wwEnvStr( "WW_OPENCELL_SHOT_SEARCH" ) );
	dlg->resize( 640, 900 );
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
