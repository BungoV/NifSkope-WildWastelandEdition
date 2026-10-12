/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellmodpickbox.h"

#include "wwskin.h"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

// lane MODPICK1 -- see cellmodpickbox.h

namespace {
enum { kFile = 0, kMod = 1 };
enum { kRowRole = Qt::UserRole + 1 };

const QString kItemMime = QStringLiteral( "application/x-qabstractitemmodeldatalist" );

bool sameChoice( const CellModChoice & a, const CellModChoice & b )
{
	return a.off == b.off && a.order == b.order && a.added == b.added;
}

//! The list: rows dragged inside it move (never copied, never removed by the view); files dropped from
//! Explorer are handed to the box. A line between rows shows where a drop lands.
class ModTree final : public QTreeWidget
{
public:
	explicit ModTree( CellModPickBox * owner ) : QTreeWidget( owner ), box( owner ) {}

	int dropIndexAt( const QPoint & pos ) const
	{
		const QTreeWidgetItem * it = itemAt( pos );
		if ( !it )
			return topLevelItemCount();
		const int i = indexOfTopLevelItem( const_cast<QTreeWidgetItem *>( it ) );
		return pos.y() < visualItemRect( it ).center().y() ? i : i + 1;
	}

protected:
	QStringList mimeTypes() const override
	{
		return QTreeWidget::mimeTypes() << QStringLiteral( "text/uri-list" );
	}
	Qt::DropActions supportedDropActions() const override
	{
		return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
	}
	void dragEnterEvent( QDragEnterEvent * e ) override
	{
		QTreeWidget::dragEnterEvent( e );
		if ( e->mimeData()->hasUrls() || e->mimeData()->hasFormat( kItemMime ) )
			e->acceptProposedAction();
	}
	void dragMoveEvent( QDragMoveEvent * e ) override
	{
		QTreeWidget::dragMoveEvent( e );	// paints the line
		if ( e->mimeData()->hasUrls() || e->mimeData()->hasFormat( kItemMime ) )
			e->acceptProposedAction();
	}
	void dropEvent( QDropEvent * e ) override
	{
		const int before = dropIndexAt( e->position().toPoint() );
		const QMimeData * m = e->mimeData();
		stopAutoScroll();
		setState( NoState );
		viewport()->update();
		if ( m->hasUrls() ) {
			QStringList paths;
			for ( const QUrl & u : m->urls() )
				if ( u.isLocalFile() )
					paths << u.toLocalFile();
			e->setDropAction( Qt::CopyAction );	// read only: the source files stay where they are
			e->accept();
			box->dropPathsAt( paths, before );
		} else if ( m->hasFormat( kItemMime ) ) {
			e->setDropAction( Qt::CopyAction );	// the view must not delete the dragged rows itself
			e->accept();
			box->moveSelectedTo( before );
		} else {
			e->ignore();
		}
	}
	void keyPressEvent( QKeyEvent * e ) override
	{
		if ( e->key() == Qt::Key_Delete ) {
			box->removeSelectedAdded();
			e->accept();
			return;
		}
		QTreeWidget::keyPressEvent( e );
	}

private:
	CellModPickBox * box;
};
}

CellModPickBox::CellModPickBox( QWidget * parent ) : QWidget( parent )
{
	setObjectName( QStringLiteral( "CellModPickBox" ) );
	QVBoxLayout * v = new QVBoxLayout( this );
	v->setContentsMargins( 0, 0, 0, 0 );
	v->setSpacing( 4 );

	source = new QLabel( this );
	source->setObjectName( QStringLiteral( "CellModPickSource" ) );
	source->setWordWrap( true );
	source->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
	v->addWidget( source );

	ModTree * t = new ModTree( this );
	list = t;
	list->setObjectName( QStringLiteral( "CellModPickList" ) );
	list->setColumnCount( 2 );
	list->setHeaderLabels( { tr( "Plugin" ), tr( "Mod" ) } );
	list->setRootIsDecorated( false );
	list->setUniformRowHeights( true );
	list->setSelectionMode( QAbstractItemView::ExtendedSelection );
	list->setEditTriggers( QAbstractItemView::NoEditTriggers );
	list->setDragEnabled( true );
	list->setAcceptDrops( true );
	list->viewport()->setAcceptDrops( true );
	list->setDragDropMode( QAbstractItemView::DragDrop );
	list->setDefaultDropAction( Qt::CopyAction );
	list->setDropIndicatorShown( true );
	list->setContextMenuPolicy( Qt::CustomContextMenu );
	list->setMinimumHeight( 160 );
	list->setMaximumHeight( 260 );
	list->header()->setSectionResizeMode( kFile, QHeaderView::Interactive );
	list->header()->setSectionResizeMode( kMod, QHeaderView::Stretch );
	list->header()->resizeSection( kFile, 210 );
	list->setStyleSheet( wwSelectionTreeQss() );
	v->addWidget( list );

	hint = new QLabel( this );
	hint->setObjectName( QStringLiteral( "CellModPickHint" ) );
	hint->setWordWrap( true );
	hint->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "accent" ) ) );
	hint->hide();
	v->addWidget( hint );

	QWidget * bar = new QWidget( this );
	QHBoxLayout * h = new QHBoxLayout( bar );
	h->setContentsMargins( 0, 0, 0, 0 );
	h->setSpacing( 4 );
	count = new QLabel( bar );
	count->setObjectName( QStringLiteral( "CellModPickCount" ) );
	count->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
	resetButton = new QPushButton( tr( "Reset" ), bar );
	resetButton->setObjectName( QStringLiteral( "CellModPickReset" ) );
	resetButton->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 10px" ) ) );
	applyButton = new QPushButton( tr( "Apply" ), bar );
	applyButton->setObjectName( QStringLiteral( "CellModPickApply" ) );
	applyButton->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 10px" ) ) );
	h->addWidget( count );
	h->addStretch( 1 );
	h->addWidget( resetButton );
	h->addWidget( applyButton );
	v->addWidget( bar );

	connect( list, &QTreeWidget::itemChanged, this, [this]( QTreeWidgetItem *, int ) {
		if ( !filling )
			updateButtons();
	} );
	connect( list, &QWidget::customContextMenuRequested, this, [this]( const QPoint & pos ) {
		QTreeWidgetItem * it = list->itemAt( pos );
		if ( !it )
			return;
		const int i = list->indexOfTopLevelItem( it );
		if ( i < 0 || i >= rows.size() || !rows.at( i ).added )
			return;
		if ( !it->isSelected() ) {
			list->clearSelection();
			it->setSelected( true );
		}
		QMenu m( this );
		QAction * remove = m.addAction( tr( "Remove" ) );
		if ( m.exec( list->viewport()->mapToGlobal( pos ) ) == remove )
			removeSelectedAdded();
	} );
	connect( applyButton, &QPushButton::clicked, this, [this]() { apply(); } );
	connect( resetButton, &QPushButton::clicked, this, [this]() { reset(); } );
	refresh( QString() );
}

void CellModPickBox::refresh( const QString & worldspace )
{
	if ( !ws.isEmpty() && worldspace.compare( ws, Qt::CaseInsensitive ) == 0 && !sameChoice( choiceNow(), atRefresh ) )
		return;	// unapplied edits (ticks, a drag, a drop) survive a redraw of the same worldspace
	ws = worldspace;
	rows.clear();
	keysAdded.clear();
	listNow = CellModList();
	note.clear();
	hint->hide();
	if ( ws.isEmpty() ) {
		source->setText( tr( "Open an exterior cell to choose its mods." ) );
		fill();
		atRefresh = choiceNow();
		updateButtons();
		return;
	}
	listNow = cellModPickList( ws, cellModPickShownBake( ws ) );
	rows = listNow.rows;
	int gone = 0, added = 0;
	for ( const CellModRow & r : std::as_const( rows ) ) {
		gone += r.present ? 0 : 1;
		added += r.added ? 1 : 0;
	}
	if ( !listNow.source.isEmpty() )
		source->setText( tr( "%1 -- from %2: %3 plugins%4%5%6" ).arg( ws, listNow.label ).arg( rows.size() - added )
			.arg( gone ? tr( ", %1 gone from disk" ).arg( gone ) : QString() )
			.arg( added ? tr( ", %1 added by hand" ).arg( added ) : QString() )
			.arg( listNow.note.isEmpty() ? QString() : QStringLiteral( "\n" ) + listNow.note ) );
	else
		source->setText( tr( "%1 -- no list: %2%3" ).arg( ws, listNow.error )
			.arg( listNow.note.isEmpty() ? QString() : QStringLiteral( "\n" ) + listNow.note ) );
	fill();
	atRefresh = choiceNow();
	updateButtons();
}

void CellModPickBox::fill()
{
	filling = true;
	list->clear();
	const QColor greyInk( wwSkinColor( "textDisabled" ) );
	for ( int i = 0; i < rows.size(); i++ ) {
		const CellModRow & r = rows.at( i );
		QTreeWidgetItem * it = new QTreeWidgetItem( list );
		it->setText( kFile, r.file );
		it->setData( kFile, kRowRole, i );
		it->setToolTip( kFile, QDir::toNativeSeparators( r.path ) );
		Qt::ItemFlags f = Qt::ItemIsEnabled;
		if ( !r.locked )
			f |= Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
		if ( r.present && !r.locked )
			f |= Qt::ItemIsUserCheckable;
		it->setFlags( f );
		it->setCheckState( kFile, r.on || r.locked ? Qt::Checked : Qt::Unchecked );
		if ( !r.present ) {
			it->setText( kMod, tr( "%1 -- gone from disk" ).arg( r.mod ) );
			it->setForeground( kFile, QBrush( greyInk ) );
			it->setForeground( kMod, QBrush( greyInk ) );
			it->setToolTip( kMod, tr( "Not on disk; it cannot load: %1" ).arg( QDir::toNativeSeparators( r.path ) ) );
		} else {
			it->setText( kMod, r.locked ? tr( "%1 -- always loads" ).arg( r.mod ) : r.mod );
			it->setToolTip( kMod, QDir::toNativeSeparators( r.added ? r.path : r.mod ) );
		}
	}
	filling = false;
}

void CellModPickBox::syncTicks()
{
	for ( int i = 0; i < list->topLevelItemCount() && i < rows.size(); i++ )
		if ( rows[i].present && !rows[i].locked )
			rows[i].on = list->topLevelItem( i )->checkState( kFile ) == Qt::Checked;
}

CellModChoice CellModPickBox::choiceNow() const
{
	CellModChoice c;
	QStringList keys;
	for ( int i = 0; i < rows.size(); i++ ) {
		const CellModRow & r = rows.at( i );
		keys << r.key;
		const QTreeWidgetItem * it = list->topLevelItem( i );
		const bool on = it ? it->checkState( kFile ) == Qt::Checked : r.on;
		if ( r.present && !r.locked && !on )
			c.off << r.key;
		if ( r.added )
			c.added << r.path;
	}
	QStringList natural;
	for ( const QString & k : listNow.naturalKeys + keysAdded )
		if ( keys.contains( k ) && !natural.contains( k ) )
			natural << k;
	if ( keys != natural )
		c.order = keys;
	return c;
}

void CellModPickBox::updateButtons()
{
	int on = 0, present = 0;
	for ( int i = 0; i < list->topLevelItemCount() && i < rows.size(); i++ ) {
		if ( !rows.at( i ).present || rows.at( i ).folder )
			continue;
		present++;
		on += list->topLevelItem( i )->checkState( kFile ) == Qt::Checked ? 1 : 0;
	}
	count->setText( ws.isEmpty() ? QString() : tr( "%1 of %2 load" ).arg( on ).arg( present ) );
	applyButton->setEnabled( !ws.isEmpty() && !sameChoice( choiceNow(), atRefresh ) );
	resetButton->setEnabled( !ws.isEmpty() && ( cellModPickTouched( ws ) || !sameChoice( choiceNow(), atRefresh ) ) );
}

void CellModPickBox::say( const QString & text )
{
	note = text;
	hint->setText( text );
	hint->setVisible( !text.isEmpty() );
	if ( QMainWindow * w = qobject_cast<QMainWindow *>( window() ) )
		w->statusBar()->showMessage( text, 8000 );
}

int CellModPickBox::firstFree() const
{
	int i = 0;
	while ( i < rows.size() && rows.at( i ).locked )
		i++;
	return i;
}

void CellModPickBox::moveSelectedTo( int before )
{
	if ( ws.isEmpty() )
		return;
	syncTicks();
	QVector<int> picked;
	for ( int i = 0; i < list->topLevelItemCount() && i < rows.size(); i++ )
		if ( list->topLevelItem( i )->isSelected() && !rows.at( i ).locked )
			picked << i;
	if ( picked.isEmpty() )
		return;
	if ( before < firstFree() ) {
		say( tr( "Fallout4.esm and the DLC masters stay at the top" ) );
		return;
	}
	QVector<CellModRow> moving, rest;
	int at = before;
	for ( int i = 0; i < rows.size(); i++ ) {
		if ( picked.contains( i ) ) {
			moving << rows.at( i );
			if ( i < before )
				at--;
		} else {
			rest << rows.at( i );
		}
	}
	QVector<CellModRow> next = rest;
	for ( int k = 0; k < moving.size(); k++ )
		next.insert( at + k, moving.at( k ) );
	const QString problem = cellModOrderProblem( rows, next );
	if ( !problem.isEmpty() ) {
		say( tr( "Not moved: %1" ).arg( problem ) );
		return;
	}
	rows = next;
	fill();
	QStringList names;
	for ( const CellModRow & r : std::as_const( moving ) )
		names << r.file;
	selectRows( names );
	say( QString() );
	updateButtons();
}

void CellModPickBox::dropPathsAt( const QStringList & paths, int before )
{
	if ( ws.isEmpty() )
		return;
	syncTicks();
	QString skipped;
	const QVector<CellModRow> dropped = cellModDropRows( paths, &skipped );
	QVector<CellModRow> fresh;
	QStringList already;
	for ( const CellModRow & r : dropped ) {
		bool listed = false;
		for ( const CellModRow & x : std::as_const( rows ) )
			listed = listed || x.key == r.key;
		if ( listed )
			already << r.file;
		else
			fresh << r;
	}
	if ( fresh.isEmpty() ) {
		QStringList why;
		if ( !already.isEmpty() )
			why << tr( "already listed: %1" ).arg( already.join( QStringLiteral( ", " ) ) );
		if ( !skipped.isEmpty() )
			why << tr( "not a plugin or a folder: %1" ).arg( skipped );
		say( tr( "Nothing added%1" ).arg( why.isEmpty() ? QString() : QStringLiteral( " -- " ) + why.join( QStringLiteral( "; " ) ) ) );
		return;
	}
	const int at = std::max( before, firstFree() );
	QVector<CellModRow> next = rows;
	for ( int k = 0; k < fresh.size(); k++ )
		next.insert( std::min( int( next.size() ), at + k ), fresh.at( k ) );
	const QString problem = cellModOrderProblem( rows, next );
	if ( !problem.isEmpty() ) {
		say( tr( "Not added: %1" ).arg( problem ) );
		return;
	}
	rows = next;
	for ( const CellModRow & r : std::as_const( fresh ) )
		keysAdded << r.key;
	fill();
	QStringList names;
	for ( const CellModRow & r : std::as_const( fresh ) )
		names << r.file;
	selectRows( names );
	QString text = tr( "Added %1 -- read in place, nothing copied" ).arg( names.join( QStringLiteral( ", " ) ) );
	if ( !already.isEmpty() )
		text += tr( "; already listed: %1" ).arg( already.join( QStringLiteral( ", " ) ) );
	if ( !skipped.isEmpty() )
		text += tr( "; skipped: %1" ).arg( skipped );
	say( text );
	updateButtons();
}

void CellModPickBox::removeSelectedAdded()
{
	if ( ws.isEmpty() )
		return;
	syncTicks();
	QVector<CellModRow> next;
	QStringList gone;
	for ( int i = 0; i < rows.size(); i++ ) {
		const QTreeWidgetItem * it = list->topLevelItem( i );
		if ( rows.at( i ).added && it && it->isSelected() )
			gone << rows.at( i ).file;
		else
			next << rows.at( i );
	}
	if ( gone.isEmpty() ) {
		say( tr( "Only rows added by hand can be removed" ) );
		return;
	}
	const QString problem = cellModOrderProblem( rows, next );
	if ( !problem.isEmpty() ) {
		say( tr( "Not removed: %1" ).arg( problem ) );
		return;
	}
	rows = next;
	fill();
	say( tr( "Removed %1" ).arg( gone.join( QStringLiteral( ", " ) ) ) );
	updateButtons();
}

QPoint CellModPickBox::dropPointBefore( int i ) const
{
	const int n = list->topLevelItemCount();
	if ( n == 0 )
		return QPoint( 20, 10 );
	if ( i >= n ) {
		const QRect r = list->visualItemRect( list->topLevelItem( n - 1 ) );
		return QPoint( r.left() + 20, r.bottom() - 2 );
	}
	const QRect r = list->visualItemRect( list->topLevelItem( i ) );
	return QPoint( r.left() + 20, r.top() + 2 );
}

void CellModPickBox::selectRows( const QStringList & files )
{
	list->clearSelection();
	for ( int i = 0; i < list->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = list->topLevelItem( i );
		if ( files.contains( it->text( kFile ), Qt::CaseInsensitive ) && ( it->flags() & Qt::ItemIsSelectable ) ) {
			it->setSelected( true );
			list->scrollToItem( it );
		}
	}
}

int CellModPickBox::rowCount() const
{
	return list->topLevelItemCount();
}

QString CellModPickBox::rowFile( int i ) const
{
	const QTreeWidgetItem * it = list->topLevelItem( i );
	return it ? it->text( kFile ) : QString();
}

QString CellModPickBox::rowMod( int i ) const
{
	const QTreeWidgetItem * it = list->topLevelItem( i );
	return it ? it->text( kMod ) : QString();
}

bool CellModPickBox::rowOn( int i ) const
{
	const QTreeWidgetItem * it = list->topLevelItem( i );
	return it && it->checkState( kFile ) == Qt::Checked;
}

bool CellModPickBox::rowPresent( int i ) const
{
	return i >= 0 && i < rows.size() && rows.at( i ).present;
}

bool CellModPickBox::rowLocked( int i ) const
{
	return i >= 0 && i < rows.size() && rows.at( i ).locked;
}

bool CellModPickBox::rowAdded( int i ) const
{
	return i >= 0 && i < rows.size() && rows.at( i ).added;
}

int CellModPickBox::rowOf( const QString & file ) const
{
	for ( int i = 0; i < rowCount(); i++ )
		if ( rowFile( i ).compare( file, Qt::CaseInsensitive ) == 0 )
			return i;
	return -1;
}

void CellModPickBox::setRowOn( int i, bool on )
{
	QTreeWidgetItem * it = list->topLevelItem( i );
	if ( it && ( it->flags() & Qt::ItemIsUserCheckable ) )
		it->setCheckState( kFile, on ? Qt::Checked : Qt::Unchecked );
}

QString CellModPickBox::sourceText() const
{
	return source->text();
}

bool CellModPickBox::applyEnabled() const
{
	return applyButton->isEnabled();
}

void CellModPickBox::apply()
{
	if ( ws.isEmpty() || !applyButton->isEnabled() )
		return;
	syncTicks();
	const CellModChoice c = choiceNow();
	cellModPickSave( ws, c );
	atRefresh = c;
	updateButtons();
	if ( onChoiceChanged )
		onChoiceChanged();
}

void CellModPickBox::reset()
{
	if ( ws.isEmpty() )
		return;
	const bool was = cellModPickTouched( ws );
	cellModPickReset( ws );
	atRefresh = choiceNow();	// the edits are dropped: let the refresh read the list again
	const QString w = ws;
	ws.clear();
	refresh( w );
	if ( !was )	// nothing saved yet: only the unsaved edits go
		return;
	updateButtons();
	if ( onChoiceChanged )
		onChoiceChanged();
}
