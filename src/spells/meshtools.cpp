/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spellbook.h"
#include "nifsnapshot.h"
#include "message.h"

#include "data/nifitem.h"
#include "data/nifvalue.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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


//! Manage every material and texture path in the file: browse, edit, find & replace
class spMaterialTextureManager final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Material / Texture Manager..." ); }
	QString page() const override final { return Spell::tr( "Textures" ); }
	bool constant() const override final { return false; }

	bool isApplicable( const NifModel * nif, const QModelIndex & ) override final
	{
		return nif && nif->getBlockCount() > 0;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QVector<QPersistentModelIndex> items;
		QStringList labels, owners;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex iBlock = nif->getBlockIndex( b );
			QString head = QString( "%1 %2" ).arg( b ).arg( nif->itemName( iBlock ) );
			// geometry / node this property is attached to (blank until resolved)
			QString ownerNode;
			int before = items.size();
			tlCollectResourceStrings( nif, iBlock, items, labels, owners, QString(), head );
			// fill in the owning geometry for the entries just collected
			if ( items.size() > before ) {
				ownerNode = tlOwnerLabel( nif, iBlock );
				for ( int k = before; k < owners.size(); k++ )
					owners[k] = ownerNode;
			}
		}

		if ( items.isEmpty() ) {
			QMessageBox::information( nullptr, name(), Spell::tr( "No material or texture paths found in this file." ) );
			return index;
		}

		QDialog dlg;
		dlg.setWindowTitle( name() );
		QVBoxLayout * lay = new QVBoxLayout( &dlg );

		QHBoxLayout * fr = new QHBoxLayout;
		QLineEdit * edFind = new QLineEdit;
		edFind->setPlaceholderText( Spell::tr( "Find" ) );
		QLineEdit * edRepl = new QLineEdit;
		edRepl->setPlaceholderText( Spell::tr( "Replace with" ) );
		QPushButton * btnReplace = new QPushButton( Spell::tr( "Replace All" ) );
		fr->addWidget( edFind, 1 );
		fr->addWidget( edRepl, 1 );
		fr->addWidget( btnReplace );
		lay->addLayout( fr );

		QTableWidget * tbl = new QTableWidget( items.size(), 3 );
		tbl->setHorizontalHeaderLabels( { Spell::tr( "Field" ), Spell::tr( "Owner node" ), Spell::tr( "Path (editable)" ) } );
		tbl->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
		tbl->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
		tbl->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Stretch );
		tbl->verticalHeader()->setVisible( false );
		tbl->setSortingEnabled( false );	// fill first, enable after
		for ( int i = 0; i < items.size(); i++ ) {
			QTableWidgetItem * c0 = new QTableWidgetItem( labels.at( i ) );
			c0->setFlags( c0->flags() & ~Qt::ItemIsEditable );
			c0->setData( Qt::UserRole, i );	// stable back-reference after sorting
			tbl->setItem( i, 0, c0 );
			QTableWidgetItem * c1 = new QTableWidgetItem( owners.at( i ) );
			c1->setFlags( c1->flags() & ~Qt::ItemIsEditable );
			tbl->setItem( i, 1, c1 );
			tbl->setItem( i, 2, new QTableWidgetItem( nif->resolveString( QModelIndex( items.at( i ) ) ) ) );
		}
		// click a column header to sort by id/field, owner, or path (name)
		tbl->setSortingEnabled( true );
		lay->addWidget( tbl, 1 );

		QObject::connect( btnReplace, &QPushButton::clicked, [&]() {
			QString f = edFind->text();
			if ( f.isEmpty() )
				return;
			QString rep = edRepl->text();
			int n = 0;
			for ( int i = 0; i < tbl->rowCount(); i++ ) {
				QTableWidgetItem * it = tbl->item( i, 2 );
				if ( it && it->text().contains( f, Qt::CaseInsensitive ) ) {
					it->setText( it->text().replace( f, rep, Qt::CaseInsensitive ) );
					n++;
				}
			}
			btnReplace->setText( Spell::tr( "Replace All (%1 changed)" ).arg( n ) );
		} );

		QDialogButtonBox * bb = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
		bb->button( QDialogButtonBox::Ok )->setText( Spell::tr( "Apply" ) );
		lay->addWidget( bb );
		QObject::connect( bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
		QObject::connect( bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

		dlg.resize( 720, 480 );
		if ( dlg.exec() != QDialog::Accepted )
			return index;

		int changed = 0;
		nifSnapshotOp( nif, name(), [&]() {
			// rows may be re-sorted; map each back to its item via UserRole
			for ( int r = 0; r < tbl->rowCount(); r++ ) {
				QTableWidgetItem * c0 = tbl->item( r, 0 );
				QTableWidgetItem * c2 = tbl->item( r, 2 );
				if ( !c0 || !c2 )
					continue;
				int i = c0->data( Qt::UserRole ).toInt();
				if ( i < 0 || i >= items.size() )
					continue;
				QModelIndex idx( items.at( i ) );
				if ( !idx.isValid() )
					continue;
				QString newVal = c2->text();
				if ( newVal != nif->resolveString( idx ) ) {
					nif->assignString( idx, newVal );
					changed++;
				}
			}
		} );

		return index;
	}
};

REGISTER_SPELL( spMaterialTextureManager )


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
	QString page() const override final { return Spell::tr( "Animation" ); }

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
