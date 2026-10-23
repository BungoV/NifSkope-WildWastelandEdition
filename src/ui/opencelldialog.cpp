/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "opencelldialog.h"

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
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

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
	grid->addWidget( langCombo, row++, 1 );

	label( tr( "Plugins" ) );
	pluginList = new QListWidget( this );
	pluginList->setDragDropMode( QAbstractItemView::InternalMove );   // the list order is the load order
	pluginList->setMinimumHeight( 140 );
	grid->addWidget( pluginList, row++, 1 );

	label( tr( "Worldspace" ) );
	worldCombo = new QComboBox( this );
	grid->addWidget( worldCombo, row++, 1 );

	label( tr( "Cell" ) );
	filterEdit = new QLineEdit( this );
	filterEdit->setPlaceholderText( tr( "Filter" ) );
	filterEdit->setClearButtonEnabled( true );
	grid->addWidget( filterEdit, row++, 1 );
	cellTree = new QTreeWidget( this );
	cellTree->setRootIsDecorated( false );
	cellTree->setUniformRowHeights( true );
	cellTree->setMinimumHeight( 180 );
	cellTree->header()->setStretchLastSection( false );
	grid->addWidget( cellTree, row++, 1 );

	auto * at = new QHBoxLayout;
	xyEdit = new QLineEdit( this );
	xyEdit->setPlaceholderText( QStringLiteral( "x, y" ) );
	sizeCombo = new QComboBox( this );
	for ( int n : { 1, 3, 5 } )
		sizeCombo->addItem( QStringLiteral( "%1 x %1" ).arg( n ), n );
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
	resize( 460, 560 );

	readTimer = new QTimer( this );
	readTimer->setSingleShot( true );
	readTimer->setInterval( 400 );     // ticking several plugins in a row reads once

	connect( browse, &QToolButton::clicked, this, &OpenCellDialog::browseData );
	connect( import, &QToolButton::clicked, this, &OpenCellDialog::browseImport );
	connect( dataEdit, &QLineEdit::editingFinished, this, [this]() { setDataDir( dataEdit->text() ); } );
	connect( langCombo, &QComboBox::currentTextChanged, this, &OpenCellDialog::setLanguage );
	connect( pluginList, &QListWidget::itemChanged, this, &OpenCellDialog::pluginsChanged );
	connect( pluginList->model(), &QAbstractItemModel::rowsMoved, this, &OpenCellDialog::pluginsChanged );
	connect( readTimer, &QTimer::timeout, this, &OpenCellDialog::readNow );
	connect( worldCombo, &QComboBox::currentIndexChanged, this, &OpenCellDialog::fillCells );
	connect( filterEdit, &QLineEdit::textChanged, this, &OpenCellDialog::fillCells );
	connect( cellTree, &QTreeWidget::itemSelectionChanged, this, &OpenCellDialog::cellPicked );
	connect( cellTree, &QTreeWidget::itemDoubleClicked, this, &OpenCellDialog::openClicked );
	connect( buttons, &QDialogButtonBox::accepted, this, &OpenCellDialog::openClicked );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
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
	const int i = worldCombo->findData( keep, Qt::UserRole + 1 );
	worldCombo->setCurrentIndex( i >= 0 ? i : 0 );
	block.unblock();
	fillCells();
}

bool OpenCellDialog::interiorMode() const
{
	return worldCombo->currentData( Qt::UserRole + 1 ).toString() == QLatin1String( interiorItem );
}

void OpenCellDialog::fillCells()
{
	const bool interior = interiorMode();
	const QString f = filterEdit->text().trimmed();
	QSignalBlocker block( cellTree );
	cellTree->clear();
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
		it->setData( 0, Qt::UserRole, c.form );
		it->setData( 0, Qt::UserRole + 1, xy );
		rows.append( it );
	}
	cellTree->addTopLevelItems( rows );
	for ( int i = 0; i < cellTree->columnCount(); i++ )
		cellTree->resizeColumnToContents( i );
	xyEdit->setEnabled( !interior );
	sizeCombo->setEnabled( !interior );
}

void OpenCellDialog::cellPicked()
{
	const auto sel = cellTree->selectedItems();
	if ( !sel.isEmpty() && !interiorMode() )
		xyEdit->setText( sel.first()->data( 0, Qt::UserRole + 1 ).toString() );
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
		for ( int c = 0; c < it->columnCount(); c++ )
			cols.append( it->text( c ) );
		out.append( cols.join( QLatin1Char( '\t' ) ) );
	}
	return out;
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
	for ( int i = 0; i < cellTree->topLevelItemCount(); i++ )
		if ( cellTree->topLevelItem( i )->text( 0 ).compare( edid, Qt::CaseInsensitive ) == 0 )
			cellTree->setCurrentItem( cellTree->topLevelItem( i ) );
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
		const QString cell = sel.first()->text( 0 ).isEmpty() ? sel.first()->text( 2 ) : sel.first()->text( 0 );
		return openCellInteriorLine( paths, cell );
	}
	int x = 0, y = 0;
	if ( !parseXY( xyEdit->text(), x, y ) )
		return QString();
	return openCellExteriorLine( paths, worldCombo->currentData( Qt::UserRole + 1 ).toString(), x, y,
		sizeCombo->currentData().toInt() );
}

bool OpenCellDialog::writeSpec( const QString & dir )
{
	const QString line = specLine();
	if ( line.isEmpty() ) {
		status->setText( interiorMode() ? tr( "Pick an interior cell" ) : tr( "Enter x, y" ) );
		return false;
	}
	QString name;
	if ( interiorMode() ) {
		name = openCellInteriorFileName( line.section( QLatin1Char( '|' ), 2, 2 ) );
	} else {
		int x = 0, y = 0;
		parseXY( xyEdit->text(), x, y );
		name = openCellFileName( worldCombo->currentData( Qt::UserRole + 1 ).toString(), x, y,
			sizeCombo->currentData().toInt() );
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
