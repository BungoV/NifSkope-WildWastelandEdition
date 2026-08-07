/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "nifview.h"

#include "spellbook.h"
#include "wwskin.h"	// the reorder insertion line's colour
#include "model/basemodel.h"
#include "model/nifproxymodel.h"
#include "model/undocommands.h"

#include <QApplication>
#include <QMimeData>
#include <QClipboard>
#include <QFile>
#include <QKeyEvent>
#include <QPainter>
#include <QTextStream>
#include <QTimer>

#include <vector>

NifTreeView::NifTreeView( QWidget * parent, Qt::WindowFlags flags ) : QTreeView()
{
	Q_UNUSED( flags );

	setParent( parent );

	connect( this, &NifTreeView::expanded, this, &NifTreeView::scrollExpand );
	connect( this, &NifTreeView::collapsed, this, &NifTreeView::onItemCollapsed );
	// expanding reveals rows the root-scoped hiding pass may never have
	// visited (whole-model tree mode has no valid root to recurse from) —
	// re-apply row hiding for whatever just became visible. Shallow: hiding
	// is derived one level per expansion (expanding a 40k-element vertex
	// array must not walk every element's members; expanding one element
	// derives its own members via this same hook)
	connect( this, &NifTreeView::expanded, this, [this]( const QModelIndex & idx ) {
		if ( nif && nif->getState() == BaseModel::Default )
			updateConditionRecurse( idx, false );
	} );
}

NifTreeView::~NifTreeView()
{
}

void NifTreeView::setModel( QAbstractItemModel * model )
{
	if ( nif )
		disconnect( nif, &BaseModel::dataChanged, this, &NifTreeView::updateConditions );

	nif = qobject_cast<BaseModel *>( model );

	QTreeView::setModel( model );

	if ( nif )
		connect( nif, &BaseModel::dataChanged, this, &NifTreeView::updateConditions );
}

void NifTreeView::setRootIndex( const QModelIndex & index )
{
	QModelIndex root = index;

	if ( root.isValid() && root.column() != 0 )
		root = root.sibling( root.row(), 0 );

	QTreeView::setRootIndex( root );

	// Re-apply row hiding for the newly shown block's fields. currentChanged
	// only refreshes the current field's subtree, so without this the sibling
	// rows keep stale visibility — e.g. version-mismatched fields (Skyrim
	// shader flags on a Fallout 4 NIF) would stay visible when switching blocks.
	refreshRowHiding();
}

// TEMP DIAGNOSTIC (WW_EXTRUDE_TEST): trace refreshRowHiding decisions
static void wwHideTrace( const QString & msg )
{
	if ( !qEnvironmentVariableIsSet( "WW_EXTRUDE_TEST" ) )
		return;
	QFile f( QApplication::applicationDirPath() + "/ww_hide.log" );
	if ( f.open( QIODevice::Append | QIODevice::Text ) )
		QTextStream( &f ) << msg << "\n";
}

void NifTreeView::reset()
{
	wwHideTrace( QString( "RESET on %1" ).arg( objectName() ) );
	QTreeView::reset();
	// the reset cleared every hidden row; re-apply once things settle (the
	// root/current usually get restored right after the reset)
	QTimer::singleShot( 0, this, &NifTreeView::refreshRowHiding );
}

void NifTreeView::doItemsLayout()
{
	// re-derive row hiding right before the layout is rebuilt: the stored
	// hidden set can silently rot (its persistent indexes get invalidated by
	// model activity that emits no reset), which previously un-hid the
	// version-mismatched rows on every block switch after the first
	if ( !inLayoutHidingRefresh && nif && rootIndex().isValid()
		&& nif->getState() == BaseModel::Default ) {
		inLayoutHidingRefresh = true;
		// shallow walk: layouts happen constantly (selection, scroll, expand),
		// and descending into a big shape's vertex/triangle arrays here made
		// every click on a high-poly block take seconds. Array-member hiding
		// is derived once per root change (refreshRowHiding's full walk).
		updateConditionRecurse( rootIndex(), false );
		inLayoutHidingRefresh = false;
	}
	QTreeView::doItemsLayout();
}

void NifTreeView::refreshRowHiding()
{
	wwHideTrace( QString( "refresh obj=%1 nif=%2 rootValid=%3 state=%4" )
		.arg( objectName() ).arg( nif != nullptr )
		.arg( rootIndex().isValid() )
		.arg( nif ? int( nif->getState() ) : -1 ) );
	if ( !nif || !rootIndex().isValid() )
		return;
	if ( nif->getState() != BaseModel::Default ) {
		// mid-load (e.g. the session-restore selection while the file is still
		// parsing): updateConditionRecurse would silently bail and the rows
		// would stay stranded-visible, because a later select() of the same
		// block skips setRootIndex. Retry once the model settles.
		QTimer::singleShot( 0, this, &NifTreeView::refreshRowHiding );
		return;
	}
	// shallow: this runs on every block switch, and descending a high-poly
	// shape's vertex/triangle arrays took seconds per click. Hiding inside
	// arrays is derived lazily, one level at a time, by the expansion hook.
	updateConditionRecurse( rootIndex(), false );
	doItemsLayout();
	if ( qEnvironmentVariableIsSet( "WW_EXTRUDE_TEST" ) ) {
		int hiddenCount = 0;
		for ( int r = 0; r < model()->rowCount( rootIndex() ); r++ )
			if ( QTreeView::isRowHidden( r, rootIndex() ) )
				hiddenCount++;
		wwHideTrace( QString( "  applied on %1: %2 of %3 rows hidden" )
			.arg( objectName() ).arg( hiddenCount ).arg( model()->rowCount( rootIndex() ) ) );
	}
}

void NifTreeView::clearRootIndex()
{
	setRootIndex( QModelIndex() );
}

void NifTreeView::setRowHiding( bool show )
{
	if ( doRowHiding != show )
		return;

	doRowHiding = !show;

	if ( nif )
		connect( nif, &BaseModel::dataChanged, this, &NifTreeView::updateConditions );

	// refresh
	updateConditionRecurse( rootIndex() );
	doItemsLayout();
}


bool NifTreeView::isRowHidden( [[maybe_unused]] int r, const QModelIndex & index ) const
{
	const NifItem * item = static_cast<const NifItem *>( index.internalPointer() );
	return isRowHidden( item );
}

bool NifTreeView::isRowHidden( const NifItem * rowItem ) const
{
	// active Block Details text filter: everything outside the keep set is
	// hidden, on top of the condition/version rules below
	if ( rowItem && detailsFilterActive && !detailsFilterKeep.contains( rowItem ) )
		return true;

	if ( rowItem && nif ) {
		// A field that does not apply to this file's version is always hidden,
		// regardless of the row-hiding mode or a type condition. Without this,
		// version-conditioned typed fields (e.g. the Skyrim shader-flag variants
		// on a Fallout 4 NIF) would still show because only their `cond` — not
		// their version condition — was being evaluated.
		if ( !nif->evalVersion( rowItem ) )
			return true;

		if ( ( doRowHiding || rowItem->hasTypeCondition() ) && !nif->evalCondition( rowItem ) )
			return true;
	}

	return false;
}

void NifTreeView::setAllExpanded( const QModelIndex & index, bool e )
{
	if ( !model() )
		return;

	for ( int r = 0; r < model()->rowCount( index ); r++ ) {
		QModelIndex child = model()->index( r, 0, index );

		if ( model()->hasChildren( child ) ) {
			setExpanded( child, e );
			setAllExpanded( child, e );
		}
	}
}

void NifTreeView::copy()
{
	QModelIndex idx = selectionModel()->selectedIndexes().first();
	auto item = static_cast<NifItem *>(idx.internalPointer());
	if ( !item )
		return;

	if ( !item->isArray() && !item->isCompound() ) {
		valueClipboard->setValue( item->value() );
	} else {
		std::vector<NifValue> v;
		v.reserve( item->childCount() );
		for ( const auto i : item->children() )
			v.push_back( i->value() );

		valueClipboard->setValues( v );
	}
}

void NifTreeView::pasteTo( const QModelIndex iDest, const NifValue & srcValue )
{
	// Only run once per row for the correct column
	if ( iDest.column() != NifModel::ValueCol )
		return;

	NifItem * item = nif->getItem( iDest );
	if ( !item || item->valueType() != srcValue.type() || item->valueType() == NifValue::tNone )
		return;

	nif->setItemValue( item, srcValue );

	auto valueType = model()->sibling( iDest.row(), 0, iDest ).data().toString();

	auto n = static_cast<NifModel*>(nif);
	if ( n )
		n->undoStack->push( new ChangeValueCommand( iDest, item->value(), srcValue, valueType, n ) );
}

void NifTreeView::paste()
{
	ChangeValueCommand::createTransaction();
	for ( const auto & i : valueIndexList( selectionModel()->selectedIndexes() ) )
		pasteTo( i, valueClipboard->getValue() );
}

void NifTreeView::pasteArray()
{
	QModelIndexList selected = selectionModel()->selectedIndexes();
	QModelIndexList values = valueIndexList( selected );

	Q_ASSERT( selected.size() == NifModel::NumColumns );
	Q_ASSERT( values.size() == 1 );

	auto root = values.at( 0 );
	auto cnt = nif->rowCount( root );

	ChangeValueCommand::createTransaction();
	nif->setState( BaseModel::Processing );
	for ( int i = 0; i < cnt && i < int( valueClipboard->getValues().size() ); i++ ) {
		auto iDest = nif->getIndex( root, i, NifModel::ValueCol );
		auto srcValue = valueClipboard->getValues().at( iDest.row() );

		pasteTo( iDest, srcValue );
	}
	nif->restoreState();

	if ( cnt > 0 )
		emit nif->dataChanged( nif->getIndex( root, 0, NifModel::ValueCol ), nif->getIndex( root, cnt - 1, NifModel::ValueCol ) );
}

void NifTreeView::drawBranches( QPainter * painter, const QRect & rect, const QModelIndex & index ) const
{
	// Fill the branch/indent gutter with the row's background colour (e.g. the
	// object-mode selection highlight) so the arrow area isn't left dark while
	// the rest of the row is coloured.
	QVariant bg = index.data( Qt::BackgroundRole );
	if ( bg.canConvert<QColor>() )
		painter->fillRect( rect, bg.value<QColor>() );

	if ( rootIsDecorated() )
		QTreeView::drawBranches( painter, rect, index );
}

void NifTreeView::updateConditions( const QModelIndex & topLeft, const QModelIndex & bottomRight )
{
	if ( nif->getState() != BaseModel::Default )
		return;

	Q_UNUSED( bottomRight );
	// Was updateConditionRecurse( parent ) with a full array descent: a
	// block-level field edit then walked every element (and every element's
	// members) of the block's arrays — O(38k × fields) per qualifying edit,
	// twice (Block Details + Header both connect here). The expanded-aware
	// walk derives the same visible result at O(visible rows): hidden state
	// of closed subtrees is derived lazily on expansion anyway (see the
	// expanded() hook), exactly like every other hiding pass in this view.
	updateConditionsLazy( topLeft.parent() );
	doItemsLayout();
}

void NifTreeView::updateConditionsLazy( const QModelIndex & index )
{
	if ( nif->getState() != BaseModel::Default )
		return;

	NifItem * item = static_cast<NifItem *>( index.internalPointer() );
	if ( index.isValid() && !item )
		return;

	// Skip flat array items
	if ( item && item->parent() && item->parent()->isArray() && !item->childCount() )
		return;

	// Descend only where the children are actually on screen: the invisible
	// root, the view root, or an expanded row. A closed row's subtree keeps
	// its stale flags harmlessly and re-derives them when opened.
	if ( !index.isValid() || index == rootIndex() || isExpanded( index ) ) {
		for ( int r = 0, cnt = model()->rowCount( index ); r < cnt; r++ )
			updateConditionsLazy( model()->index( r, 0, index ) );
	}

	if ( item )
		setRowHidden( index.row(), index.parent(), isRowHidden( item ) );
}

void NifTreeView::updateConditionRecurse( const QModelIndex & index, bool descendArrays )
{
	if ( nif->getState() != BaseModel::Default )
		return;

	NifItem * item = static_cast<NifItem *>(index.internalPointer());
	if ( !item )
		return;

	// Skip flat array items
	if ( item->parent() && item->parent()->isArray() && !item->childCount() )
		return;

	if ( descendArrays || !item->isArray() ) {
		for ( int r = 0; r < model()->rowCount( index ); r++ ) {
			QModelIndex child = model()->index( r, 0, index );
			updateConditionRecurse( child, descendArrays );
		}
	}

	setRowHidden( index.row(), index.parent(), isRowHidden(item) );
}

auto splitMime = []( QString format ) {
	for ( const QString & separator : { QStringLiteral( "/" ), QStringLiteral( "\u02C2" ) } ) {
		QStringList split = format.split( separator );
		if ( split.value( 0 ) == QLatin1String( "nifskope" )
			&& ( split.value( 1 ) == QLatin1String( "niblock" )
				|| split.value( 1 ) == QLatin1String( "nibranch" ) ) )
			return !split.value( 2 ).isEmpty();
	}

	return false;
};

QModelIndexList NifTreeView::valueIndexList( const QModelIndexList & rows ) const
{
	QModelIndexList values;
	for ( int i = NifModel::ValueCol; i < rows.count(); i += NifModel::NumColumns )
		values << rows[i];

	return values;
}

/*! Hand the drag to the block list's own implementation, if one is installed.
 *
 *  The stock path asks the model for mimeData(), and neither NifModel nor
 *  NifProxyModel implements it — so without this the block list simply never
 *  starts a drag, silently. Qt's row-move semantics would not fit blocks and
 *  links anyway; the hook builds its own QDrag with block numbers in it.
 */
void NifTreeView::startDrag( Qt::DropActions supportedActions )
{
	if ( startBlockDrag && startBlockDrag() )
		return;

	QTreeView::startDrag( supportedActions );
}

void NifTreeView::paintEvent( QPaintEvent * e )
{
	QTreeView::paintEvent( e );

	if ( wwDropLineY < 0 )
		return;

	/* Indented to where the row's own text starts, so it reads as "between these
	 * two children" rather than as a rule across the whole dock — Blender's
	 * insertion marker does the same. The cap on the left end is what makes it
	 * read as an insertion point rather than as a separator that was always
	 * there; a bare hairline gets lost against the row grid.
	 */
	QPainter p( viewport() );
	p.setRenderHint( QPainter::Antialiasing );
	const QColor accent = QColor::fromString( wwSkinColor( "accent" ) );
	const int y = qBound( 1, wwDropLineY, viewport()->height() - 2 );
	p.setPen( QPen( accent, 2 ) );
	p.drawLine( wwDropLineFrom + 5, y, viewport()->width() - 3, y );
	p.setPen( Qt::NoPen );
	p.setBrush( accent );
	p.drawEllipse( QPoint( wwDropLineFrom + 3, y ), 3, 3 );
}

void NifTreeView::wwDeliverDragEvent( QEvent * e )
{
	if ( !e )
		return;

	switch ( e->type() ) {
	case QEvent::DragEnter:
		dragEnterEvent( static_cast<QDragEnterEvent *>( e ) );
		break;
	case QEvent::DragMove:
		dragMoveEvent( static_cast<QDragMoveEvent *>( e ) );
		break;
	case QEvent::DragLeave:
		dragLeaveEvent( static_cast<QDragLeaveEvent *>( e ) );
		break;
	case QEvent::Drop:
		dropEvent( static_cast<QDropEvent *>( e ) );
		break;
	default:
		break;
	}
}

void NifTreeView::dragEnterEvent( QDragEnterEvent * e )
{
	wwDragEventsSeen++;
	if ( blockDropEvent && blockDropEvent( e ) )
		return;

	QTreeView::dragEnterEvent( e );
}

void NifTreeView::dragMoveEvent( QDragMoveEvent * e )
{
	wwDragEventsSeen++;
	if ( blockDropEvent && blockDropEvent( e ) )
		return;

	QTreeView::dragMoveEvent( e );
}

void NifTreeView::dragLeaveEvent( QDragLeaveEvent * e )
{
	wwDragEventsSeen++;
	if ( blockDropEvent )
		blockDropEvent( e );	// always returns false: the view clears its own state too

	QTreeView::dragLeaveEvent( e );
}

void NifTreeView::dropEvent( QDropEvent * e )
{
	wwDragEventsSeen++;
	if ( blockDropEvent && blockDropEvent( e ) )
		return;

	QTreeView::dropEvent( e );
}

void NifTreeView::keyPressEvent( QKeyEvent * e )
{
	NifModel * nif = nullptr;
	NifProxyModel * proxy = nullptr;

	nif = static_cast<NifModel *>(model());
	if ( model()->inherits( "NifModel" ) && nif ) {
		// Determine if a block or branch has been copied
		bool hasBlockCopied = false;
		if ( e->matches( QKeySequence::Copy ) || e->matches( QKeySequence::Paste ) ) {
			auto mime = QApplication::clipboard()->mimeData();
			if ( mime ) {
				for ( const QString& form : mime->formats() ) {
					if ( splitMime( form ) ) {
						hasBlockCopied = true;
						break;
					}
				}
			}
		}

		QModelIndexList selectedRows = selectionModel()->selectedIndexes();
		QModelIndexList valueColumns = valueIndexList( selectedRows );
		if ( !(selectedRows.size() && valueColumns.size()) )
			return;

		if ( e->matches( QKeySequence::Copy ) ) {
			copy();
			// Clear the clipboard in case it holds a block to prevent conflicting behavior
			QApplication::clipboard()->clear();
			return;
		} else if ( e->matches( QKeySequence::Paste )
					&& (valueClipboard->getValue().isValid() || valueClipboard->getValues().size() > 0)
					&& !hasBlockCopied ) {
			// Do row paste if there is no block/branch copied and the NifValue is valid
			if ( valueColumns.size() == 1 && nif->rowCount( selectedRows.at( 0 ) ) > 0 ) {
				pasteArray();
			} else if ( valueClipboard->getValue().isValid() ) {
				paste();
			}
			return;
		}
	}

	auto spells = SpellBook::lookup( QKeySequence( e->modifiers() | e->key() ) );

	for ( auto spell : spells ) {
		QPersistentModelIndex oldidx;

		// Clear this on any spell cast to prevent it overriding other paste behavior like block -> link row
		// TODO: Value clipboard does not get cleared when using the context menu.
		valueClipboard->clear();

		if ( model()->inherits( "NifModel" ) ) {
			nif = static_cast<NifModel *>( model() );
			oldidx = currentIndex();
		} else if ( model()->inherits( "NifProxyModel" ) ) {
			proxy = static_cast<NifProxyModel *>( model() );
			nif = static_cast<NifModel *>( proxy->model() );
			oldidx = proxy->mapTo( currentIndex() );
		}

		// Cast non-modifying spells
		if ( spell->constant() && spell->isApplicable( nif, oldidx ) ) {
			spell->cast( nif, oldidx );
			return;
		}

		if ( nif && spell->isApplicable( nif, oldidx ) ) {
			selectionModel()->setCurrentIndex( QModelIndex(), QItemSelectionModel::Clear | QItemSelectionModel::Rows );

			bool noSignals = spell->batch();
			if ( noSignals )
				nif->setState( BaseModel::Processing );
			// Cast the spell and return index
			QModelIndex newidx = spell->cast( nif, oldidx );
			if ( noSignals )
				nif->resetState();

			// Refresh the header
			nif->invalidateHeaderConditions();
			nif->updateHeader();

			if ( nif->getProcessingResult() ) {
				emit nif->dataChanged( newidx, newidx );
			}

			if ( proxy )
				newidx = proxy->mapFrom( newidx, oldidx );

			// grab selection from the selection model as it tends to be more accurate
			if ( !newidx.isValid() ) {
				if ( oldidx.isValid() )
					newidx = ( proxy ) ? proxy->mapFrom( oldidx, oldidx ) : ( (QModelIndex)oldidx );
				else
					newidx = selectionModel()->currentIndex();
			}

			if ( newidx.isValid() ) {
				selectionModel()->setCurrentIndex( newidx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows );
				scrollTo( newidx, EnsureVisible );
				emit clicked( newidx );
			}

			return;
		}
	}

	QTreeView::keyPressEvent( e );
}

void NifTreeView::mousePressEvent( QMouseEvent * event )
{
	blockMouseSelection = false;

	/* BLANK SPACE DESELECTS. QTreeView leaves the previous row selected when you
	 * click past the end of the list, so there was no way to have no primary
	 * selection — and no way for a paste to mean "no parent".
	 *
	 * The current index goes too, not just the selection: the two are separate in
	 * Qt, and leaving a current index behind means the spell book still has a
	 * block to act on while the list shows nothing selected.
	 */
	if ( wwBlankClickClearsSelection && selectionModel() && !indexAt( event->pos() ).isValid() ) {
		selectionModel()->clearSelection();
		selectionModel()->clearCurrentIndex();
		event->accept();
		return;
	}

	QTreeView::mousePressEvent( event );
}

void NifTreeView::mouseReleaseEvent( QMouseEvent * event )
{
	if ( !blockMouseSelection )
		QTreeView::mouseReleaseEvent( event );
}

void NifTreeView::mouseMoveEvent( QMouseEvent * event )
{
	if ( !blockMouseSelection )
		QTreeView::mouseMoveEvent( event );
}

void NifTreeView::currentChanged( const QModelIndex & current, const QModelIndex & last )
{
	QTreeView::currentChanged( current, last );

	if ( nif ) {
		// shallow: currentChanged fires on every click, and a full descent of
		// a high-poly shape's arrays took seconds; array interiors are derived
		// by the expansion hook instead
		updateConditionRecurse( current, false );
		// safety net: any interaction re-applies the whole visible block's
		// row hiding (deferred while loading), so a hiding pass that bailed
		// mid-load can never leave version-mismatched rows stranded visible
		refreshRowHiding();
	}

	autoExpanded = false;
	if ( doAutoExpanding )
		autoExpandBlock( current );

	emit sigCurrentIndexChanged( currentIndex() );
}

void NifTreeView::autoExpandBlock( const QModelIndex & blockIndex )
{
	auto mdl = qobject_cast<const NifModel *>( nif );
	if ( !mdl )
		return;

	auto block = mdl->getItem( blockIndex, false );
	if ( !mdl->isNiBlock(block) )
		return;

	if ( mdl->inherits(block->name(), "NiTransformInterpolator") || mdl->inherits(block->name(), "NiBSplineTransformInterpolator") ) {
		// Auto-Expand NiQuatTransform
		autoExpandItem( mdl->getItem(block, "Transform") );
	} else if ( mdl->inherits(block->name(), "NiNode") ) {
		// Auto-Expand Children array
		autoExpandItem( mdl->getItem(block, "Children") );
	} else if ( mdl->inherits(block->name(), "NiSkinPartition") || mdl->inherits(block->name(), "BSDismemberSkinInstance") ) {
		// Auto-Expand skin partitions array
		autoExpandItem( mdl->getItem(block, "Partitions") );
	} else {
		// Auto-Expand final arrays/compounds
		for( int i = block->childCount() - 1; i >= 0; i-- ) {
			auto c = block->child(i);
			if ( c && !isRowHidden(c) ) {
				autoExpandItem( c );
				break;
			}
		}
	}
}

void NifTreeView::autoExpandItem( const NifItem * item )
{
	if ( !item )
		return;

	auto nChildren = item->childCount();
	if ( nChildren > 0 && nChildren < 100 ) {
		autoExpanded = true;
		blockMouseSelection = true;
		expand( nif->itemToIndex(item) );
	}
}

void NifTreeView::scrollExpand( const QModelIndex & index )
{
	blockMouseSelection = true;

	// this is a compromise between scrolling to the top, and scrolling the last child to the bottom
	if ( !autoExpanded )
		scrollTo( index, PositionAtCenter );
}

void NifTreeView::onItemCollapsed( const QModelIndex & index )
{
	Q_UNUSED( index );
	blockMouseSelection = true;
}
