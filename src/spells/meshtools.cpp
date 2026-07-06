/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spellbook.h"
#include "nifsnapshot.h"
#include "message.h"

#include "data/nifitem.h"
#include "data/nifvalue.h"
#include "ui/widgets/ddspreview.h"
#include "ui/widgets/filebrowser.h"
#include "glview.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

/*! \file meshtools.cpp
 *  \brief Material/texture find & replace manager and node-name authority spells.
 */

//! True for a directly readable string item (sized strings / file paths and the
//! string-table refs). Deliberately excludes tStringOffset, which resolveString
//! does not support and which only appears inside controller-sequence blocks.
static bool tlIsStringItem( const NifItem * item )
{
	if ( !item )
		return false;
	NifValue::Type t = item->value().type();
	return item->value().isString() || t == NifValue::tStringIndex;
}

//! Does the text look like a material or texture resource path? (Not behavior
//! graphs / collision - those are not textures or materials.)
static bool tlLooksLikeResource( const QString & s )
{
	if ( s.isEmpty() )
		return false;
	static const char * exts[] = { ".dds", ".bgsm", ".bgem", ".tga" };
	QString low = s.toLower();
	for ( const char * e : exts ) {
		if ( low.endsWith( QLatin1String( e ) ) )
			return true;
	}
	return low.contains( QLatin1String( "textures" ) ) || low.contains( QLatin1String( "materials" ) );
}

//! Nearest geometry / owning block for a shader/texture property, for context
static QString tlOwnerLabel( NifModel * nif, const QModelIndex & iItem )
{
	// walk up to the containing block, then find who links to it
	QModelIndex iBlock = iItem;
	while ( iBlock.isValid() && iBlock.parent().isValid() )
		iBlock = iBlock.parent();
	int bn = nif->getBlockNumber( iBlock );
	if ( bn < 0 )
		return QString();
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iOther = nif->getBlockIndex( b );
		if ( !nif->blockInherits( iOther, "NiAVObject" ) )
			continue;
		const auto links = nif->getChildLinks( b );
		if ( links.contains( bn ) ) {
			QString nm = nif->resolveString( iOther, "Name" );
			return QString( "%1 %2%3" ).arg( b ).arg( nif->itemName( iOther ),
				nm.isEmpty() ? QString() : QStringLiteral( " \"" ) + nm + QStringLiteral( "\"" ) );
		}
	}
	return QString();
}

//! Recursively collect resource-path string items under a block
static void tlCollectResourceStrings( NifModel * nif, const QModelIndex & parent,
	QVector<QPersistentModelIndex> & out, QStringList & labels, QStringList & owners,
	const QString & owner, const QString & prefix, int depth = 0 )
{
	if ( depth > 12 )
		return;
	for ( int r = 0; r < nif->rowCount( parent ); r++ ) {
		QModelIndex row = nif->index( r, 0, parent );
		if ( !row.isValid() )
			continue;
		const NifItem * item = static_cast<const NifItem *>( row.internalPointer() );
		QString nm = nif->itemName( row );
		QString path = prefix.isEmpty() ? nm : prefix + QStringLiteral( "/" ) + nm;
		if ( tlIsStringItem( item ) ) {
			QString val = nif->resolveString( row );
			if ( tlLooksLikeResource( val ) ) {
				out.append( QPersistentModelIndex( row ) );
				labels.append( path );
				owners.append( owner );
			}
		}
		if ( nif->rowCount( row ) > 0 )
			tlCollectResourceStrings( nif, row, out, labels, owners, owner, path, depth + 1 );
	}
}


//! Archive listing filters for the resource browsers
static bool tlTexFileFilter( [[maybe_unused]] void * p, const std::string_view & s )
{
	return s.starts_with( "textures/" ) && s.ends_with( ".dds" );
}

static bool tlMatFileFilter( [[maybe_unused]] void * p, const std::string_view & s )
{
	return s.ends_with( ".bgsm" ) || s.ends_with( ".bgem" );
}

//! Standardize a Bethesda resource path: backslashes, no doubles, no leading
//! slash, lowercase, no stray quotes/whitespace
static QString tlNormalizeResourcePath( QString s )
{
	s = s.trimmed();
	s.remove( QChar( '"' ) );
	s.replace( QChar( '/' ), QChar( '\\' ) );
	while ( s.contains( QLatin1String( "\\\\" ) ) )
		s.replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );
	while ( s.startsWith( QChar( '\\' ) ) )
		s.remove( 0, 1 );
	return s.toLower();
}

//! Path table with drag & drop: dragging a path cell drops its text onto
//! another row's path (also accepts plain-text drops from outside)
class TlPathTable final : public QTableWidget
{
public:
	TlPathTable( QWidget * parent = nullptr ) : QTableWidget( 0, 3, parent )
	{
		setDragEnabled( true );
		setAcceptDrops( true );
		viewport()->setAcceptDrops( true );
		setDropIndicatorShown( true );
		setDragDropMode( QAbstractItemView::DragDrop );
	}

protected:
	void startDrag( Qt::DropActions ) override
	{
		QTableWidgetItem * it = currentItem();
		if ( !it )
			return;
		QTableWidgetItem * src = item( it->row(), 2 );
		if ( !src || src->text().isEmpty() )
			return;
		QMimeData * md = new QMimeData;
		md->setText( src->text() );
		QDrag * drag = new QDrag( this );
		drag->setMimeData( md );
		drag->exec( Qt::CopyAction );
	}
	void dragEnterEvent( QDragEnterEvent * e ) override
	{
		if ( e->mimeData()->hasText() )
			e->acceptProposedAction();
	}
	void dragMoveEvent( QDragMoveEvent * e ) override
	{
		if ( e->mimeData()->hasText() )
			e->acceptProposedAction();
	}
	void dropEvent( QDropEvent * e ) override
	{
		QTableWidgetItem * it = itemAt( e->position().toPoint() );
		if ( it && e->mimeData()->hasText() ) {
			QTableWidgetItem * dst = item( it->row(), 2 );
			if ( dst )
				dst->setText( e->mimeData()->text() );	// itemChanged applies + normalizes
			e->acceptProposedAction();
		}
	}
};

//! Resolve a resource path against the loaded archives, honouring its extension
static QString tlFindResource( const NifModel * nif, const QString & path )
{
	if ( path.isEmpty() )
		return QString();
	QString low = path.toLower();
	if ( low.endsWith( QLatin1String( ".bgsm" ) ) )
		return nif->findResourceFile( path, "materials", ".bgsm" );
	if ( low.endsWith( QLatin1String( ".bgem" ) ) )
		return nif->findResourceFile( path, "materials", ".bgem" );
	if ( low.endsWith( QLatin1String( ".tga" ) ) )
		return nif->findResourceFile( path, "textures", ".tga" );
	return nif->findResourceFile( path, "textures", ".dds" );
}

//! Extract .dds paths referenced by a binary material file (format-agnostic
//! printable-string scan; works for BGSM and BGEM)
static QStringList tlScanMaterialTextures( const QByteArray & data )
{
	QStringList out;
	QByteArray cur;
	for ( char ch : data ) {
		if ( ch >= 0x20 && ch != 0x7f ) {
			cur.append( ch );
			continue;
		}
		if ( cur.size() >= 5 ) {
			QString s = QString::fromLatin1( cur ).toLower();
			if ( s.endsWith( QLatin1String( ".dds" ) ) && !out.contains( s ) )
				out.append( s );
		}
		cur.clear();
	}
	if ( cur.size() >= 5 ) {
		QString s = QString::fromLatin1( cur ).toLower();
		if ( s.endsWith( QLatin1String( ".dds" ) ) && !out.contains( s ) )
			out.append( s );
	}
	return out;
}

//! Nearest NiAVObject whose child links contain the given block, or -1
static int tlOwnerBlock( NifModel * nif, int bn )
{
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		if ( nif->blockInherits( nif->getBlockIndex( b ), "NiAVObject" )
			&& nif->getChildLinks( b ).contains( bn ) )
			return b;
	}
	return -1;
}

//! Clipboard for whole BSShaderTextureSet contents
static QStringList tlCopiedTexSet;

//! Build the Material / Texture Manager panel: live editing, find & replace,
//! archive browser, drag & drop, texture preview, selection sync
QDockWidget * tlCreateMatTexManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	{
		QDockWidget * dock = new QDockWidget( QObject::tr( "Material / Texture Manager" ), mw );
		dock->setObjectName( QStringLiteral( "MatTexManagerDock" ) );

		QWidget * panel = new QWidget( dock );
		QVBoxLayout * lay = new QVBoxLayout( panel );

		QHBoxLayout * fr = new QHBoxLayout;
		QLineEdit * edFilter = new QLineEdit( panel );
		edFilter->setPlaceholderText( QObject::tr( "Filter rows" ) );
		edFilter->setClearButtonEnabled( true );
		QLineEdit * edFind = new QLineEdit( panel );
		edFind->setPlaceholderText( QObject::tr( "Find" ) );
		QLineEdit * edRepl = new QLineEdit( panel );
		edRepl->setPlaceholderText( QObject::tr( "Replace with" ) );
		QPushButton * btnReplace = new QPushButton( QObject::tr( "Replace All" ), panel );
		fr->addWidget( edFilter, 1 );
		fr->addWidget( edFind, 1 );
		fr->addWidget( edRepl, 1 );
		fr->addWidget( btnReplace );
		lay->addLayout( fr );

		QHBoxLayout * fr2 = new QHBoxLayout;
		QPushButton * btnBrowse = new QPushButton( QObject::tr( "Browse..." ), panel );
		btnBrowse->setToolTip( QObject::tr( "Pick a replacement from the game archives for the selected row" ) );
		QPushButton * btnRetarget = new QPushButton( QObject::tr( "Retarget Folder..." ), panel );
		btnRetarget->setToolTip( QObject::tr( "Replace a folder prefix on every matching path" ) );
		QCheckBox * cbGroup = new QCheckBox( QObject::tr( "Group by node" ), panel );
		QPushButton * btnRefresh = new QPushButton( QObject::tr( "Refresh" ), panel );
		fr2->addWidget( btnBrowse );
		fr2->addWidget( btnRetarget );
		fr2->addWidget( cbGroup );
		fr2->addStretch( 1 );
		fr2->addWidget( btnRefresh );
		lay->addLayout( fr2 );

		QSplitter * split = new QSplitter( Qt::Horizontal, panel );
		TlPathTable * tbl = new TlPathTable( split );
		tbl->setHorizontalHeaderLabels( { QObject::tr( "Field" ), QObject::tr( "Owner node" ), QObject::tr( "Path (editable, drag && drop)" ) } );
		tbl->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
		tbl->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
		tbl->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Stretch );
		tbl->verticalHeader()->setVisible( false );
		tbl->setContextMenuPolicy( Qt::CustomContextMenu );

		QScrollArea * prevScroll = new QScrollArea( split );
		prevScroll->setWidgetResizable( true );
		prevScroll->setMinimumWidth( 280 );
		{
			QLabel * l = new QLabel( QObject::tr( "Select a texture row for a preview" ), prevScroll );
			l->setAlignment( Qt::AlignCenter );
			l->setWordWrap( true );
			prevScroll->setWidget( l );
		}
		split->addWidget( tbl );
		split->addWidget( prevScroll );
		split->setStretchFactor( 0, 3 );
		split->setStretchFactor( 1, 2 );
		lay->addWidget( split, 1 );

		QLabel * hint = new QLabel( QObject::tr( "Edits apply immediately (undoable). Paths are normalized on input. Click a row to jump to the node using it; right-click for more." ), panel );
		hint->setWordWrap( true );
		lay->addWidget( hint );

		// shared state for the lambdas
		auto rows = std::make_shared<QVector<QPersistentModelIndex>>();
		auto applying = std::make_shared<bool>( false );

		// object-mode selection colours, mirroring the block list (active
		// light blue + #FF9D00, secondary dark blue + #FF7200)
		auto recolor = [tbl, ogl, applying]() {
			*applying = true;
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				QTableWidgetItem * c1 = tbl->item( r, 1 );
				QTableWidgetItem * c2 = tbl->item( r, 2 );
				if ( !c1 || !c2 )
					continue;
				int owner = c1->data( Qt::UserRole ).toInt();
				bool sel = ( owner >= 0 && ogl && ogl->objSelection.contains( owner ) );
				bool act = ( sel && owner == ogl->objActive );
				QBrush bg = sel ? QBrush( act ? QColor( 74, 122, 176 ) : QColor( 43, 66, 95 ) ) : QBrush();
				QBrush fg = sel ? QBrush( act ? QColor( 255, 157, 0 ) : QColor( 255, 114, 0 ) ) : QBrush();
				for ( int c = 0; c < 3; c++ ) {
					if ( QTableWidgetItem * it = tbl->item( r, c ) ) {
						it->setBackground( bg );
						it->setForeground( fg );
					}
				}
				if ( !sel && c2->data( Qt::UserRole ).toBool() )
					c2->setForeground( QColor( 235, 90, 90 ) );	// missing resource
			}
			*applying = false;
		};

		auto rebuild = [nif, tbl, rows, applying, recolor]() {
			*applying = true;
			tbl->setSortingEnabled( false );
			tbl->setRowCount( 0 );
			QVector<QPersistentModelIndex> items;
			QStringList labels, owners;
			QVector<int> blockNums, ownerNums;
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iBlock = nif->getBlockIndex( b );
				QString head = QString( "%1 %2" ).arg( b ).arg( nif->itemName( iBlock ) );
				int before = items.size();
				tlCollectResourceStrings( nif, iBlock, items, labels, owners, QString(), head );
				if ( items.size() > before ) {
					QString ownerNode = tlOwnerLabel( nif, iBlock );
					int ownerNum = tlOwnerBlock( nif, b );
					// a shader/texture path inside an AVObject block belongs to it
					if ( nif->blockInherits( iBlock, "NiAVObject" ) )
						ownerNum = b;
					for ( int k = before; k < owners.size(); k++ ) {
						owners[k] = ownerNode;
						blockNums.append( b );
						ownerNums.append( ownerNum );
					}
				}
			}
			tbl->setRowCount( items.size() );
			for ( int i = 0; i < items.size(); i++ ) {
				QTableWidgetItem * c0 = new QTableWidgetItem( labels.at( i ) );
				c0->setFlags( c0->flags() & ~Qt::ItemIsEditable );
				c0->setData( Qt::UserRole, i );	// stable back-reference after sorting
				c0->setData( Qt::UserRole + 1, blockNums.at( i ) );	// containing block
				tbl->setItem( i, 0, c0 );
				QTableWidgetItem * c1 = new QTableWidgetItem( owners.at( i ) );
				c1->setFlags( c1->flags() & ~Qt::ItemIsEditable );
				c1->setData( Qt::UserRole, ownerNums.at( i ) );	// owning NiAVObject
				tbl->setItem( i, 1, c1 );
				QString path = nif->resolveString( QModelIndex( items.at( i ) ) );
				QTableWidgetItem * c2 = new QTableWidgetItem( path );
				// paths missing from the loaded archives show in red
				bool missing = !path.isEmpty() && tlFindResource( nif, path ).isEmpty();
				c2->setData( Qt::UserRole, missing );
				if ( missing )
					c2->setForeground( QColor( 235, 90, 90 ) );
				tbl->setItem( i, 2, c2 );
			}
			*rows = items;
			tbl->setSortingEnabled( true );
			*applying = false;
			recolor();
		};

		// live apply with path normalization; this replaces the old
		// apply-on-OK flow whose sorted-row mapping could silently drop edits
		QObject::connect( tbl, &QTableWidget::itemChanged, panel, [nif, tbl, rows, applying]( QTableWidgetItem * it ) {
			if ( *applying || !it || it->column() != 2 )
				return;
			QString norm = tlNormalizeResourcePath( it->text() );
			*applying = true;
			if ( norm != it->text() )
				it->setText( norm );
			bool missing = !norm.isEmpty() && tlFindResource( nif, norm ).isEmpty();
			it->setData( Qt::UserRole, missing );
			it->setForeground( missing ? QBrush( QColor( 235, 90, 90 ) ) : QBrush() );
			*applying = false;
			QTableWidgetItem * c0 = tbl->item( it->row(), 0 );
			if ( !c0 )
				return;
			int i = c0->data( Qt::UserRole ).toInt();
			if ( i < 0 || i >= rows->size() )
				return;
			QModelIndex idx( rows->at( i ) );
			if ( !idx.isValid() || norm == nif->resolveString( idx ) )
				return;
			nifSnapshotOp( nif, QObject::tr( "Edit resource path" ), [&]() {
				nif->assignString( idx, norm );
			} );
		} );

		// selecting a row jumps to the node using the path and previews textures;
		// material files list the .dds paths they reference
		QObject::connect( tbl, &QTableWidget::currentCellChanged, panel,
			[nif, tbl, applying, mw, prevScroll]( int r, int, int, int ) {
			if ( *applying || r < 0 )
				return;
			QTableWidgetItem * c0 = tbl->item( r, 0 );
			QTableWidgetItem * c1 = tbl->item( r, 1 );
			QTableWidgetItem * c2 = tbl->item( r, 2 );
			if ( c0 && c1 && mw ) {
				int owner = c1->data( Qt::UserRole ).toInt();
				int bn = c0->data( Qt::UserRole + 1 ).toInt();
				QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection,
					Q_ARG( QModelIndex, nif->getBlockIndex( owner >= 0 ? owner : bn ) ) );
			}
			// preview on the right
			if ( QWidget * old = prevScroll->takeWidget() )
				old->deleteLater();
			QString pth = c2 ? c2->text() : QString();
			bool isMat = pth.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			             || pth.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
			if ( pth.endsWith( QLatin1String( ".dds" ), Qt::CaseInsensitive ) ) {
				try {
					prevScroll->setWidget( new DDSTextureInfo( nif->getGameResources(), pth, prevScroll ) );
					return;
				} catch ( ... ) {
					// fall through to the placeholder
				}
			} else if ( isMat ) {
				// list the textures the material file references
				const char * ext = pth.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive ) ? ".bgem" : ".bgsm";
				std::string fullPath = Game::GameManager::get_full_path( pth, "materials/", ext );
				QByteArray data;
				if ( nif->getGameResources().get_file( data, fullPath ) && !data.isEmpty() ) {
					QStringList texs = tlScanMaterialTextures( data );
					QLabel * l = new QLabel( QObject::tr( "Textures referenced by\n%1:\n\n%2" )
						.arg( pth, texs.isEmpty() ? QObject::tr( "(none found)" ) : texs.join( QStringLiteral( "\n" ) ) ), prevScroll );
					l->setAlignment( Qt::AlignTop | Qt::AlignLeft );
					l->setWordWrap( true );
					l->setTextInteractionFlags( Qt::TextSelectableByMouse );
					l->setMargin( 8 );
					prevScroll->setWidget( l );
					return;
				}
			}
			QLabel * l = new QLabel( pth.isEmpty() ? QObject::tr( "Select a texture row for a preview" )
				: ( isMat ? QObject::tr( "Material not found:\n%1" ).arg( pth )
					: QObject::tr( "Texture not found:\n%1" ).arg( pth ) ), prevScroll );
			l->setAlignment( Qt::AlignCenter );
			l->setWordWrap( true );
			prevScroll->setWidget( l );
		} );

		// archive browser (the "Select Material" / texture picker) for the row
		auto browseRow = [nif, tbl]() {
			int r = tbl->currentRow();
			if ( r < 0 )
				return;
			QTableWidgetItem * c2 = tbl->item( r, 2 );
			if ( !c2 )
				return;
			QString cur = c2->text();
			bool isMat = cur.endsWith( QLatin1String( ".bgsm" ), Qt::CaseInsensitive )
			             || cur.endsWith( QLatin1String( ".bgem" ), Qt::CaseInsensitive );
			std::set<std::string_view> files;
			nif->listResourceFiles( files, isMat ? &tlMatFileFilter : &tlTexFileFilter );
			std::string prv( QString( cur ).replace( QChar( '\\' ), QChar( '/' ) ).toLower().toStdString() );
			FileBrowserWidget browser( 720, 540, isMat ? "Select Material" : "Select Texture",
				files, prv, &( nif->getGameResources() ) );
			if ( browser.exec() == QDialog::Accepted ) {
				const std::string_view * s = browser.getItemSelected();
				if ( s && !s->empty() )
					c2->setText( QString::fromUtf8( s->data(), qsizetype( s->length() ) ) );
			}
		};
		QObject::connect( btnBrowse, &QPushButton::clicked, panel, browseRow );
		QObject::connect( tbl, &QTableWidget::cellDoubleClicked, panel, [browseRow]( int, int col ) {
			if ( col != 2 )
				browseRow();	// double-click Field/Owner opens the browser too
		} );

		QObject::connect( btnReplace, &QPushButton::clicked, panel, [tbl, edFind, edRepl, btnReplace]() {
			QString f = edFind->text();
			if ( f.isEmpty() )
				return;
			QString rep = edRepl->text();
			int n = 0;
			for ( int i = 0; i < tbl->rowCount(); i++ ) {
				QTableWidgetItem * it = tbl->item( i, 2 );
				if ( it && it->text().contains( f, Qt::CaseInsensitive ) ) {
					it->setText( QString( it->text() ).replace( f, rep, Qt::CaseInsensitive ) );
					n++;
				}
			}
			btnReplace->setText( QObject::tr( "Replace All (%1 changed)" ).arg( n ) );
		} );

		// live row filter
		QObject::connect( edFilter, &QLineEdit::textChanged, panel, [tbl]( const QString & f ) {
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				bool match = f.isEmpty();
				for ( int c = 0; c < 3 && !match; c++ ) {
					if ( QTableWidgetItem * it = tbl->item( r, c ) )
						match = it->text().contains( f, Qt::CaseInsensitive );
				}
				tbl->setRowHidden( r, !match );
			}
		} );

		// group rows by their owning node
		QObject::connect( cbGroup, &QCheckBox::toggled, panel, [tbl]( bool on ) {
			if ( on )
				tbl->sortItems( 1 );
			else
				tbl->sortItems( 0 );
		} );

		// bulk folder retarget: swap a path prefix everywhere it matches
		QObject::connect( btnRetarget, &QPushButton::clicked, panel, [tbl, panel]() {
			bool ok = false;
			QString from = QInputDialog::getText( panel, QObject::tr( "Retarget folder" ),
				QObject::tr( "Replace this folder prefix:" ), QLineEdit::Normal, QString(), &ok );
			if ( !ok || from.isEmpty() )
				return;
			QString to = QInputDialog::getText( panel, QObject::tr( "Retarget folder" ),
				QObject::tr( "...with this prefix:" ), QLineEdit::Normal, from, &ok );
			if ( !ok )
				return;
			from = tlNormalizeResourcePath( from );
			to = tlNormalizeResourcePath( to );
			int n = 0;
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				QTableWidgetItem * it = tbl->item( r, 2 );
				if ( it && it->text().startsWith( from, Qt::CaseInsensitive ) ) {
					it->setText( to + it->text().mid( from.length() ) );
					n++;
				}
			}
			Message::info( panel, QObject::tr( "Retargeted %1 path(s)." ).arg( n ) );
		} );

		// right-click menu: browse, clear, copy/paste, navigation, texture sets
		QObject::connect( tbl, &QTableWidget::customContextMenuRequested, panel,
			[nif, tbl, rows, mw, browseRow, rebuild]( const QPoint & pos ) {
			QTableWidgetItem * hit = tbl->itemAt( pos );
			if ( !hit )
				return;
			int r = hit->row();
			tbl->setCurrentCell( r, 2 );
			QTableWidgetItem * c0 = tbl->item( r, 0 );
			QTableWidgetItem * c1 = tbl->item( r, 1 );
			QTableWidgetItem * c2 = tbl->item( r, 2 );
			if ( !c0 || !c1 || !c2 )
				return;
			int i = c0->data( Qt::UserRole ).toInt();
			int bn = c0->data( Qt::UserRole + 1 ).toInt();
			int owner = c1->data( Qt::UserRole ).toInt();
			bool isTexSet = ( nif->itemName( nif->getBlockIndex( bn ) ) == QLatin1String( "BSShaderTextureSet" ) );

			QMenu menu( tbl );
			QAction * aBrowse = menu.addAction( QObject::tr( "Browse Archives..." ) );
			QAction * aClear = menu.addAction( QObject::tr( "Clear Path" ) );
			menu.addSeparator();
			QAction * aCopy = menu.addAction( QObject::tr( "Copy Path" ) );
			QAction * aPaste = menu.addAction( QObject::tr( "Paste Path" ) );
			aPaste->setEnabled( !QApplication::clipboard()->text().isEmpty() );
			menu.addSeparator();
			QAction * aJump = menu.addAction( QObject::tr( "Jump to Node" ) );
			QAction * aReveal = menu.addAction( QObject::tr( "Reveal in Block Details" ) );
			menu.addSeparator();
			QAction * aCopySet = menu.addAction( QObject::tr( "Copy Texture Set" ) );
			aCopySet->setEnabled( isTexSet );
			QAction * aPasteSet = menu.addAction( QObject::tr( "Paste Texture Set" ) );
			aPasteSet->setEnabled( isTexSet && !tlCopiedTexSet.isEmpty() );

			QAction * sel = menu.exec( tbl->viewport()->mapToGlobal( pos ) );
			if ( !sel )
				return;
			if ( sel == aBrowse ) {
				browseRow();
			} else if ( sel == aClear ) {
				c2->setText( QString() );
			} else if ( sel == aCopy ) {
				QApplication::clipboard()->setText( c2->text() );
			} else if ( sel == aPaste ) {
				c2->setText( QApplication::clipboard()->text() );
			} else if ( sel == aJump && mw ) {
				QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection,
					Q_ARG( QModelIndex, nif->getBlockIndex( owner >= 0 ? owner : bn ) ) );
			} else if ( sel == aReveal && mw && i >= 0 && i < rows->size() ) {
				QModelIndex idx( rows->at( i ) );
				if ( idx.isValid() )
					QMetaObject::invokeMethod( mw, "select", Qt::QueuedConnection, Q_ARG( QModelIndex, idx ) );
			} else if ( sel == aCopySet ) {
				tlCopiedTexSet.clear();
				QModelIndex iTex = nif->getIndex( nif->getBlockIndex( bn ), "Textures" );
				for ( int t = 0; t < nif->rowCount( iTex ); t++ )
					tlCopiedTexSet.append( nif->resolveString( nif->index( t, 0, iTex ) ) );
			} else if ( sel == aPasteSet ) {
				QModelIndex iTex = nif->getIndex( nif->getBlockIndex( bn ), "Textures" );
				nifSnapshotOp( nif, QObject::tr( "Paste texture set" ), [&]() {
					int n = std::min( (int) tlCopiedTexSet.size(), nif->rowCount( iTex ) );
					for ( int t = 0; t < n; t++ )
						nif->assignString( nif->index( t, 0, iTex ), tlCopiedTexSet.at( t ) );
				} );
				rebuild();
			}
		} );

		QObject::connect( btnRefresh, &QPushButton::clicked, panel, rebuild );
		// rebuild when a different file is loaded, or when the panel is opened
		QObject::connect( nif, &QAbstractItemModel::modelReset, dock, [dock, rebuild]() {
			if ( dock->isVisible() )
				rebuild();
		} );
		QObject::connect( dock, &QDockWidget::visibilityChanged, dock, [rebuild]( bool vis ) {
			if ( vis )
				rebuild();
		} );

		// selection sync: block-list / viewport selection colours matching rows
		if ( ogl ) {
			QObject::connect( ogl, &GLView::objectSelectionChanged, dock, [dock, recolor]() {
				if ( dock->isVisible() )
					recolor();
			} );
		}

		dock->setWidget( panel );
		if ( mw )
			mw->addDockWidget( Qt::RightDockWidgetArea, dock );
		// starts as its own small floating window; drag onto the main window to dock
		dock->setFloating( true );
		dock->resize( 1000, 560 );
		dock->hide();

		return dock;
	}
}


//! Propagate a node's name to the object palette and controller-sequence blocks
static int tlPropagateNodeName( NifModel * nif, int nodeNum, const QString & oldName, const QString & newName )
{
	int fixes = 0;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex iBlock = nif->getBlockIndex( b );

		if ( nif->blockInherits( iBlock, "NiDefaultAVObjectPalette" ) ) {
			QModelIndex iObjs = nif->getIndex( iBlock, "Objs" );
			for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
				QModelIndex iObj = nif->index( r, 0, iObjs );
				bool match = ( nif->getLink( iObj, "AV Object" ) == nodeNum );
				if ( !match && !oldName.isEmpty() )
					match = ( nif->resolveString( iObj, "Name" ) == oldName );
				if ( match ) {
					nif->assignString( iObj, "Name", newName );
					fixes++;
				}
			}
		} else if ( nif->blockInherits( iBlock, "NiControllerSequence" ) ) {
			QModelIndex iCB = nif->getIndex( iBlock, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
				QModelIndex iRow = nif->index( r, 0, iCB );
				if ( !oldName.isEmpty() && nif->resolveString( iRow, "Node Name" ) == oldName ) {
					nif->assignString( iRow, "Node Name", newName );
					fixes++;
				}
			}
		}
	}
	return fixes;
}


//! Rename a node and keep the palette + controller sequences in sync
class spRenameNodeSynced final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Rename (sync animation)..." ); }
	QString page() const override final { return Spell::tr( "Node" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif ? nif->getBlockIndex( index ) : QModelIndex();
		return iBlock.isValid() && nif->getIndex( iBlock, "Name" ).isValid()
		       && nif->blockInherits( iBlock, "NiObjectNET" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		int nodeNum = nif->getBlockNumber( iBlock );
		QString oldName = nif->resolveString( iBlock, "Name" );

		bool ok = false;
		QString newName = QInputDialog::getText( nullptr, name(),
			Spell::tr( "New name (the palette and all controller sequences will be updated to match):" ),
			QLineEdit::Normal, oldName, &ok );
		if ( !ok || newName == oldName )
			return index;

		int fixes = 0;
		nifSnapshotOp( nif, Spell::tr( "Rename node to %1" ).arg( newName ), [&]() {
			nif->assignString( iBlock, "Name", newName );
			fixes = tlPropagateNodeName( nif, nodeNum, oldName, newName );
		} );

		if ( fixes > 0 )
			Message::info( nullptr, Spell::tr( "Updated %1 palette/sequence reference(s)." ).arg( fixes ) );

		return iBlock;
	}
};

REGISTER_SPELL( spRenameNodeSynced )


//! Make every node's name authoritative: resync the palette and controller sequences
class spEnforceNameAuthority final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Enforce Node Name Authority" ); }
	QString page() const override final { return Spell::tr( "Sanitize" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & ) override final
	{
		if ( !nif )
			return false;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			if ( nif->blockInherits( nif->getBlockIndex( b ), "NiDefaultAVObjectPalette" ) )
				return true;
		}
		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		int fixes = 0;
		nifSnapshotOp( nif, name(), [&]() {
			// palette Objs are the bridge: the linked node's name is authoritative
			QHash<QString, QString> rename;	// old palette name -> authoritative node name
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iPal = nif->getBlockIndex( b );
				if ( !nif->blockInherits( iPal, "NiDefaultAVObjectPalette" ) )
					continue;
				QModelIndex iObjs = nif->getIndex( iPal, "Objs" );
				for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
					QModelIndex iObj = nif->index( r, 0, iObjs );
					QModelIndex iNode = nif->getBlockIndex( nif->getLink( iObj, "AV Object" ) );
					if ( !iNode.isValid() )
						continue;
					QString auth = nif->resolveString( iNode, "Name" );
					QString cur = nif->resolveString( iObj, "Name" );
					if ( !auth.isEmpty() && auth != cur ) {
						rename.insert( cur, auth );
						nif->assignString( iObj, "Name", auth );
						fixes++;
					}
				}
			}
			// carry the rename through the controller sequences
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex iSeq = nif->getBlockIndex( b );
				if ( !nif->blockInherits( iSeq, "NiControllerSequence" ) )
					continue;
				QModelIndex iCB = nif->getIndex( iSeq, "Controlled Blocks" );
				for ( int r = 0; r < nif->rowCount( iCB ); r++ ) {
					QModelIndex iRow = nif->index( r, 0, iCB );
					QString cur = nif->resolveString( iRow, "Node Name" );
					if ( rename.contains( cur ) ) {
						nif->assignString( iRow, "Node Name", rename.value( cur ) );
						fixes++;
					}
				}
			}
		} );

		Message::info( nullptr, fixes > 0
			? Spell::tr( "Fixed %1 name reference(s) to match their nodes." ).arg( fixes )
			: Spell::tr( "All names already match their nodes." ) );
		return index;
	}
};

REGISTER_SPELL( spEnforceNameAuthority )
