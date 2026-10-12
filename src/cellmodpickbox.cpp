/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#include "cellmodpickbox.h"

#include "cellmodpick.h"
#include "wwskin.h"

#include <QBrush>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

// lane MODPICK1 -- see cellmodpickbox.h

namespace {
enum { kFile = 0, kMod = 1 };
enum { kPresentRole = Qt::UserRole + 1, kLockedRole };
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

	list = new QTreeWidget( this );
	list->setObjectName( QStringLiteral( "CellModPickList" ) );
	list->setColumnCount( 2 );
	list->setHeaderLabels( { tr( "Plugin" ), tr( "Mod" ) } );
	list->setRootIsDecorated( false );
	list->setUniformRowHeights( true );
	list->setSelectionMode( QAbstractItemView::NoSelection );
	list->setEditTriggers( QAbstractItemView::NoEditTriggers );
	list->setMinimumHeight( 160 );
	list->setMaximumHeight( 260 );
	list->header()->setSectionResizeMode( kFile, QHeaderView::Interactive );
	list->header()->setSectionResizeMode( kMod, QHeaderView::Stretch );
	list->header()->resizeSection( kFile, 210 );
	list->setStyleSheet( wwSelectionTreeQss() );
	v->addWidget( list );

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
	connect( applyButton, &QPushButton::clicked, this, [this]() { apply(); } );
	connect( resetButton, &QPushButton::clicked, this, [this]() { reset(); } );
	refresh( QString() );
}

void CellModPickBox::refresh( const QString & worldspace )
{
	ws = worldspace;
	filling = true;
	list->clear();
	offAtRefresh.clear();
	if ( ws.isEmpty() ) {
		source->setText( tr( "Open an exterior cell to choose its mods." ) );
		filling = false;
		updateButtons();
		return;
	}
	const CellModList L = cellModPickList( ws, cellModPickShownBake( ws ) );
	int gone = 0;
	for ( const CellModRow & r : L.rows )
		gone += r.present ? 0 : 1;
	if ( L.source == QLatin1String( "bake" ) )
		source->setText( tr( "%1 -- from the far LOD bake %2: %3 plugins%4" ).arg( ws )
			.arg( QFileInfo( L.from ).fileName() ).arg( L.rows.size() )
			.arg( gone ? tr( ", %1 gone from disk" ).arg( gone ) : QString() ) );
	else if ( L.source == QLatin1String( "mo2" ) )
		source->setText( tr( "%1 -- from the MO2 profile %2 (no far LOD bake shown): %3 plugins%4" ).arg( ws )
			.arg( QFileInfo( L.from ).fileName() ).arg( L.rows.size() )
			.arg( gone ? tr( ", %1 gone from disk" ).arg( gone ) : QString() ) );
	else
		source->setText( tr( "%1 -- no list: %2" ).arg( ws ).arg( L.error ) );

	const QColor greyInk( wwSkinColor( "textDisabled" ) );
	for ( int i = 0; i < L.rows.size(); i++ ) {
		const CellModRow & r = L.rows.at( i );
		QTreeWidgetItem * it = new QTreeWidgetItem( list );
		it->setText( kFile, r.file );
		const bool locked = r.present && i == 0;	// the worldspace's master always loads
		it->setData( kFile, kPresentRole, r.present );
		it->setData( kFile, kLockedRole, locked );
		it->setToolTip( kFile, QDir::toNativeSeparators( r.path ) );
		Qt::ItemFlags f = Qt::ItemIsEnabled;
		if ( r.present && !locked )
			f |= Qt::ItemIsUserCheckable;
		it->setFlags( f );
		it->setCheckState( kFile, r.on || locked ? Qt::Checked : Qt::Unchecked );
		if ( !r.present ) {
			it->setText( kMod, tr( "%1 -- gone from disk" ).arg( r.mod ) );
			it->setForeground( kFile, QBrush( greyInk ) );
			it->setForeground( kMod, QBrush( greyInk ) );
			it->setToolTip( kMod, tr( "Not on disk; it cannot load: %1" ).arg( QDir::toNativeSeparators( r.path ) ) );
		} else {
			it->setText( kMod, locked ? tr( "%1 -- always loads" ).arg( r.mod ) : r.mod );
		}
	}
	filling = false;
	offAtRefresh = offNow();
	updateButtons();
}

QStringList CellModPickBox::offNow() const
{
	QStringList off;
	for ( int i = 0; i < list->topLevelItemCount(); i++ ) {
		const QTreeWidgetItem * it = list->topLevelItem( i );
		if ( it->data( kFile, kPresentRole ).toBool() && !it->data( kFile, kLockedRole ).toBool()
			&& it->checkState( kFile ) != Qt::Checked )
			off << it->text( kFile ).toLower();
	}
	return off;
}

void CellModPickBox::updateButtons()
{
	int on = 0, present = 0;
	for ( int i = 0; i < list->topLevelItemCount(); i++ ) {
		const QTreeWidgetItem * it = list->topLevelItem( i );
		if ( !it->data( kFile, kPresentRole ).toBool() )
			continue;
		present++;
		on += it->checkState( kFile ) == Qt::Checked ? 1 : 0;
	}
	count->setText( ws.isEmpty() ? QString() : tr( "%1 of %2 load" ).arg( on ).arg( present ) );
	applyButton->setEnabled( !ws.isEmpty() && offNow() != offAtRefresh );
	resetButton->setEnabled( !ws.isEmpty() && cellModPickTouched( ws ) );
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
	const QTreeWidgetItem * it = list->topLevelItem( i );
	return it && it->data( kFile, kPresentRole ).toBool();
}

bool CellModPickBox::rowLocked( int i ) const
{
	const QTreeWidgetItem * it = list->topLevelItem( i );
	return it && it->data( kFile, kLockedRole ).toBool();
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
	cellModPickSave( ws, offNow() );
	offAtRefresh = offNow();
	updateButtons();
	if ( onChoiceChanged )
		onChoiceChanged();
}

void CellModPickBox::reset()
{
	if ( ws.isEmpty() || !cellModPickTouched( ws ) )
		return;
	cellModPickReset( ws );
	updateButtons();
	if ( onChoiceChanged )
		onChoiceChanged();
}
