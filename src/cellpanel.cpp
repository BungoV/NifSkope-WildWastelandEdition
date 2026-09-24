/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellpanel.h"

#include "cellclick.h"
#include "cellpick.h"
#include "wwskin.h"

#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>


CellPickPanel::CellPickPanel( QWidget * parent )
	: QWidget( parent )
{
	setObjectName( QStringLiteral( "CellPickPanel" ) );

	QVBoxLayout * page = new QVBoxLayout( this );
	page->setContentsMargins( 6, 6, 6, 6 );
	page->setSpacing( 4 );

	page->addWidget( wwHeading( tr( "Reference" ), this ) );

	rows = new QTreeWidget( this );
	rows->setObjectName( QStringLiteral( "CellPickRows" ) );
	rows->setColumnCount( 2 );
	rows->setHeaderLabels( { tr( "Name" ), tr( "Value" ) } );
	// FLAT: no tree, no root decoration, no expanding -- the rows are a record,
	// not a hierarchy, and every panel in this fork shows a record this way
	rows->setRootIsDecorated( false );
	rows->setUniformRowHeights( true );
	rows->setAlternatingRowColors( false );
	rows->setSelectionMode( QAbstractItemView::SingleSelection );
	rows->setEditTriggers( QAbstractItemView::NoEditTriggers );
	rows->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	rows->header()->setSectionResizeMode( 1, QHeaderView::Stretch );
	rows->setStyleSheet( wwSelectionTreeQss() );
	page->addWidget( rows, 1 );

	/* The summary line is PINNED under the tree and never scrolls away: the
	 * action bar's rule from the panel style, for a panel whose only action is
	 * clicking in the viewport. */
	summary = new QLabel( this );
	summary->setObjectName( QStringLiteral( "CellPickSummary" ) );
	summary->setWordWrap( true );
	page->addWidget( summary );

	connect( CellPickBus::instance(), &CellPickBus::picked,
		this, &CellPickPanel::showPick );
	connect( CellPickBus::instance(), &CellPickBus::sceneChanged,
		this, &CellPickPanel::onSceneChanged );

	onSceneChanged();
}

void CellPickPanel::setRefusal( const QString & why )
{
	rows->clear();
	summary->setStyleSheet( QString( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
	summary->setText( why );
}

void CellPickPanel::onSceneChanged()
{
	if ( !cellPickEnabled() ) {
		setRefusal( tr( "Picking is off. Tick Render > Cell Pick Panel." ) );
		return;
	}
	if ( cellPickTable().size() == 0 ) {
		setRefusal( tr( "No cell scene is open. Open a .wwcell file." ) );
		return;
	}
	setRefusal( tr( "%n placement(s) in this scene. Click one in the viewport.",
		nullptr, cellPickTable().size() ) );
}

void CellPickPanel::showPick( int index, int candidates )
{
	const CellPickTable & table = cellPickTable();
	if ( index < 0 || index >= table.size() ) {
		setRefusal( tr( "Nothing under the cursor." ) );
		return;
	}

	rows->clear();
	const QVector<QPair<QString, QString>> flat = table.rowsFor( index );
	for ( const QPair<QString, QString> & r : flat ) {
		QTreeWidgetItem * item = new QTreeWidgetItem( rows );
		item->setText( 0, r.first );
		item->setText( 1, r.second );
	}

	/* HOW MANY BOXES THE RAY ENTERED, always.  The pick is by world AABB and
	 * ties go to the smaller box (src/cellpick.h); a panel that showed one row
	 * and said nothing would be claiming an unambiguous hit that this test
	 * cannot promise. */
	summary->setStyleSheet( QString( "color: %1;" ).arg(
		candidates > 1 ? wwSkinColor( "textMuted" ) : wwSkinColor( "text" ) ) );
	if ( candidates > 1 ) {
		summary->setText( tr( "%1 boxes under the cursor; the nearest, smallest one is shown." )
			.arg( candidates ) );
	} else {
		summary->setText( tr( "1 box under the cursor." ) );
	}
}
