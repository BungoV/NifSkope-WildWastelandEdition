/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellfavoritespanel.h"

#include "cellclick.h"
#include "cellview.h"
#include "cellworkspace.h"
#include "glview.h"
#include "wwskin.h"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kCell = 4096.0f;
}

CellFavoritesPanel::CellFavoritesPanel( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "CellFavoritesPanel" ) );
	auto * l = new QVBoxLayout( this );
	l->setContentsMargins( 0, 0, 0, 0 );
	l->setSpacing( 4 );

	list = new QTreeWidget( this );
	list->setObjectName( QStringLiteral( "CellFavoritesList" ) );
	list->setColumnCount( 2 );
	list->setHeaderHidden( true );
	list->setRootIsDecorated( false );
	list->setUniformRowHeights( true );
	list->setSelectionMode( QAbstractItemView::SingleSelection );
	list->setEditTriggers( QAbstractItemView::NoEditTriggers );	// F2 and Rename open the editor themselves
	list->header()->setStretchLastSection( false );
	list->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	list->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	list->setContextMenuPolicy( Qt::CustomContextMenu );
	list->installEventFilter( this );
	l->addWidget( list, 1 );

	auto * row = new QWidget( this );
	auto * rl = new QHBoxLayout( row );
	rl->setContentsMargins( 0, 0, 0, 0 );
	rl->setSpacing( 4 );
	note = new QLabel( row );
	note->setObjectName( QStringLiteral( "CellFavoritesNote" ) );
	note->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
	saveButton = new QPushButton( tr( "Save Camera" ), row );
	saveButton->setObjectName( QStringLiteral( "CellFavoritesSave" ) );
	rl->addWidget( note, 1 );
	rl->addWidget( saveButton, 0 );
	l->addWidget( row, 0 );

	connect( saveButton, &QPushButton::clicked, this, [this]() { saveCamera(); } );
	connect( list, &QTreeWidget::itemClicked, this, [this]( QTreeWidgetItem * it, int ) { jumpTo( rowOf( it ) ); } );
	connect( list, &QTreeWidget::itemChanged, this, [this]( QTreeWidgetItem * it, int col ) {
		if ( filling || col != 0 )
			return;
		renameRow( rowOf( it ), it->text( 0 ) );
	} );
	connect( list, &QTreeWidget::customContextMenuRequested, this, &CellFavoritesPanel::showMenu );
	connect( CellPickBus::instance(), &CellPickBus::sceneChanged, this, &CellFavoritesPanel::refresh );
	refresh();
}

int CellFavoritesPanel::count() const
{
	return list->topLevelItemCount();
}

QString CellFavoritesPanel::nameAt( int i ) const
{
	const QTreeWidgetItem * it = list->topLevelItem( i );
	return it ? it->text( 0 ) : QString();
}

int CellFavoritesPanel::startRow() const
{
	return key.isEmpty() ? -1 : cellFavoritesStart( store.lists.value( key ) );
}

int CellFavoritesPanel::rowOf( QTreeWidgetItem * it ) const
{
	return it ? list->indexOfTopLevelItem( it ) : -1;
}

void CellFavoritesPanel::refresh()
{
	CellSceneSpec spec;
	key.clear();
	if ( cellWorkspaceOpenSpec( spec ) ) {
		key = cellFavoritesKey( spec.interior, spec.world, spec.interiorCell );
		interior = spec.interior;
		cx = spec.cx;
		cy = spec.cy;
		n = spec.n;
	}
	QString err;
	if ( !cellFavoritesRead( cellFavoritesPath(), store, &err ) )
		qWarning().noquote() << "cell favorites:" << err;
	fill();
}

void CellFavoritesPanel::fill()
{
	filling = true;
	list->clear();
	const QVector<CellFavorite> fav = store.lists.value( key );
	const int start = cellFavoritesStart( fav );
	const int h = std::max( n, 1 ) / 2;
	for ( int i = 0; i < fav.size(); i++ ) {
		const CellFavorite & c = fav.at( i );
		auto * it = new QTreeWidgetItem( list );
		it->setText( 0, c.name );
		it->setText( 1, i == start ? tr( "Start" ) : QString() );
		it->setFlags( it->flags() | Qt::ItemIsEditable );
		const int fx = int( std::floor( c.eye[0] / kCell ) ), fy = int( std::floor( c.eye[1] / kCell ) );
		const bool inRange = interior || ( fx >= cx - h && fx <= cx + h && fy >= cy - h && fy <= cy + h );
		it->setToolTip( 0, QStringLiteral( "%1, %2, %3  cell %4, %5" ).arg( double( c.eye[0] ), 0, 'f', 0 )
			.arg( double( c.eye[1] ), 0, 'f', 0 ).arg( double( c.eye[2] ), 0, 'f', 0 ).arg( fx ).arg( fy ) );
		if ( !inRange )
			it->setForeground( 0, QBrush( QColor( wwSkinColor( "textMuted" ) ) ) );
		if ( i == start )
			it->setForeground( 1, QBrush( QColor( wwSkinColor( "toggle" ) ) ) );
	}
	filling = false;
	const bool open = !key.isEmpty();
	saveButton->setEnabled( open && glView );
	note->setText( open ? QString() : tr( "Open a cell to save its camera" ) );
	list->setEnabled( open );
}

void CellFavoritesPanel::write()
{
	QString err;
	if ( !cellFavoritesWrite( cellFavoritesPath(), store, &err ) )
		note->setText( err );
}

int CellFavoritesPanel::saveCamera( const QString & name )
{
	if ( key.isEmpty() || !glView )
		return -1;
	CellFavorite c;
	float rot[3];
	if ( !glView->wwCameraNow( c.eye, rot, c.fovDeg, c.dist, c.at ) ) {
		note->setText( tr( "A path camera holds the view" ) );
		return -1;
	}
	c.pitchDeg = rot[0] + 90.0f;
	c.rollDeg = rot[1];
	c.yawDeg = rot[2];
	cellFavoritesRead( cellFavoritesPath(), store );	// another window may have saved since
	QVector<CellFavorite> & fav = store.lists[key];
	c.name = name.trimmed().isEmpty() ? cellFavoritesNewName( fav ) : name.trimmed();
	cellFavoritesTouch( store, c );
	fav.append( c );
	write();
	fill();
	const int r = fav.size() - 1;
	if ( QTreeWidgetItem * it = list->topLevelItem( r ) ) {
		list->setCurrentItem( it, 0 );
		if ( name.trimmed().isEmpty() && isVisible() )
			list->editItem( it, 0 );
	}
	return r;
}

bool CellFavoritesPanel::jumpTo( int row )
{
	if ( key.isEmpty() || !glView )
		return false;
	QVector<CellFavorite> & fav = store.lists[key];
	if ( row < 0 || row >= fav.size() )
		return false;
	CellFavorite & c = fav[row];
	const float rot[3] = { c.pitchDeg - 90.0f, c.rollDeg, c.yawDeg };
	qInfo().noquote() << glView->wwCameraGoTo( c.eye, rot, c.fovDeg, c.dist ) << "| favorite" << c.name;
	cellFavoritesTouch( store, c );
	write();
	const int start = cellFavoritesStart( fav );
	filling = true;
	for ( int i = 0; i < list->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = list->topLevelItem( i );
		it->setText( 1, i == start ? tr( "Start" ) : QString() );
		it->setForeground( 1, QBrush( QColor( wwSkinColor( "toggle" ) ) ) );
	}
	filling = false;
	return true;
}

bool CellFavoritesPanel::renameRow( int row, const QString & name )
{
	if ( key.isEmpty() )
		return false;
	QVector<CellFavorite> & fav = store.lists[key];
	if ( row < 0 || row >= fav.size() )
		return false;
	const QString t = name.trimmed();
	if ( t.isEmpty() || t == fav[row].name ) {
		fill();
		return false;
	}
	fav[row].name = t;
	write();
	fill();
	if ( QTreeWidgetItem * it = list->topLevelItem( row ) )
		list->setCurrentItem( it, 0 );
	return true;
}

bool CellFavoritesPanel::deleteRow( int row )
{
	if ( key.isEmpty() )
		return false;
	QVector<CellFavorite> & fav = store.lists[key];
	if ( row < 0 || row >= fav.size() )
		return false;
	fav.remove( row );
	write();
	fill();
	if ( QTreeWidgetItem * it = list->topLevelItem( std::min( row, list->topLevelItemCount() - 1 ) ) )
		list->setCurrentItem( it, 0 );
	return true;
}

void CellFavoritesPanel::showMenu( const QPoint & at )
{
	QTreeWidgetItem * it = list->itemAt( at );
	const int row = rowOf( it );
	QMenu m( this );
	QAction * save = m.addAction( tr( "Save Camera" ) );
	save->setEnabled( saveButton->isEnabled() );
	QAction * jump = m.addAction( tr( "Jump To" ) );
	QAction * ren = m.addAction( tr( "Rename" ) );
	ren->setShortcut( QKeySequence( Qt::Key_F2 ) );
	QAction * del = m.addAction( tr( "Delete" ) );
	del->setShortcut( QKeySequence( QKeySequence::Delete ) );
	for ( QAction * a : { jump, ren, del } )
		a->setEnabled( row >= 0 );
	QAction * got = m.exec( list->viewport()->mapToGlobal( at ) );
	if ( got == save )
		saveCamera();
	else if ( got == jump )
		jumpTo( row );
	else if ( got == ren && it )
		list->editItem( it, 0 );
	else if ( got == del )
		deleteRow( row );
}

bool CellFavoritesPanel::eventFilter( QObject * o, QEvent * e )
{
	/* F2 and Del belong to the list while it has the focus: claim them before a window shortcut does (the main
	 * window's Delete removes a block). The name editor is its own widget with the focus, so its keys never come here. */
	if ( o == list && ( e->type() == QEvent::ShortcutOverride || e->type() == QEvent::KeyPress ) ) {
		const int k = static_cast<QKeyEvent *>( e )->key();
		if ( k == Qt::Key_F2 || k == Qt::Key_Delete ) {
			if ( e->type() == QEvent::ShortcutOverride ) {
				e->accept();
				return true;
			}
			QTreeWidgetItem * it = list->currentItem();
			if ( it && k == Qt::Key_F2 )
				list->editItem( it, 0 );
			else if ( it )
				deleteRow( rowOf( it ) );
			return true;
		}
	}
	return QWidget::eventFilter( o, e );
}
