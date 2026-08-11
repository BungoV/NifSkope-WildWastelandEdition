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

#ifndef NIFTREEVIEW_H
#define NIFTREEVIEW_H

#include <QTreeView> // Inherited
#include <QSet>
#include <data/nifvalue.h>

#include <functional>
#include <memory>
#include <utility>

class NifItem;

//! Widget for showing a nif file as tree, list, or block details.
class NifTreeView final : public QTreeView
{
	Q_OBJECT

public:
	//! Constructor
	NifTreeView( QWidget * parent = 0, Qt::WindowFlags flags = Qt::Widget );
	//! Destructor
	~NifTreeView();

	//! Set the model used by the widget
	void setModel( QAbstractItemModel * model ) override final;
	//! Expand all branches
	void setAllExpanded( const QModelIndex & index, bool e );

	//! Accessor for EvalConditions
	bool evalConditions() const { return doRowHiding; }
	//! Is a row hidden?
	bool isRowHidden( int row, const QModelIndex & parent ) const;
	//! Centered explanation painted when the owning panel has no rows to show.
	void setEmptyMessage( const QString & message )
	{
		if ( emptyMessage == message ) return;
		emptyMessage = message;
		viewport()->update();
	}
	QString emptyMessageText() const { return emptyMessage; }

	//! Block Details text filter (WW): while active, rows outside the keep
	//! set hide IN ADDITION to condition/version hiding. The filter must live
	//! here because every hiding re-derivation (doItemsLayout, the expansion
	//! hook, resets) rewrites row visibility from isRowHidden() — a filter
	//! applied only by the window was silently clobbered by the next pass.
	void setDetailsFilter( bool active, QSet<const void *> && keep )
	{
		detailsFilterActive = active;
		detailsFilterKeep = std::move( keep );
	}
protected:
	bool isRowHidden( const NifItem * rowItem ) const;

	bool detailsFilterActive = false;
	QSet<const void *> detailsFilterKeep;
	QString emptyMessage;

public:
	/*! Block-list drag-and-drop (WW): begin the drag instead of QTreeView.
	 *
	 *  Set by NifSkope, which is the only object that knows the proxy, the
	 *  selection and what a row means as a block number. Returning true
	 *  suppresses the view's own drag; unset (the Block Details tree, the KFM
	 *  tree) leaves stock behaviour untouched.
	 *
	 *  A hook rather than a subclass because `list` is promoted in nifskope.ui,
	 *  and rather than an event filter because startDrag() is a protected
	 *  virtual — a filter never sees it.
	 */
	std::function<bool()> startBlockDrag;

	/*! The other half: enter/move/leave/drop. Returns true when it consumed the
	 *  event, so anything that is not a block payload falls through to
	 *  QTreeView untouched — the Block Details and KFM trees never set this at
	 *  all and keep stock behaviour exactly.
	 *
	 *  This is a view override rather than an event filter on the viewport
	 *  because a filter never runs for these: an event sent to the viewport with
	 *  QApplication::sendEvent reaches neither the object nor the application
	 *  filter, measured. Qt routes viewport drag events to the VIEW's handlers
	 *  through viewportEvent(), so this is the path a real drag takes anyway.
	 */
	std::function<bool( QEvent * )> blockDropEvent;

	/*! Clicked past the last row (WW, Block List only).
	 *
	 *  A tree with no way to select nothing has no way to SAY nothing, and the
	 *  Block List needs to: paste follows the pointer, and "no parent" is a real
	 *  answer. Unset for the field views, where the current row is the thing
	 *  being edited and losing it on a stray click would be hostile.
	 *
	 *  A hook rather than a bool because deselecting is not the view's business
	 *  alone: the row COLOUR comes from the object selection, which lives in the
	 *  3D view, so clearing only what the tree knows about leaves the row looking
	 *  selected while the status bar says nothing is.
	 */
	std::function<void()> wwBlankClicked;

	/*! Keys the Block List answers before anything else does (WW).
	 *
	 *  H / Alt+H, Blender's hide and reveal-all — which already worked in the
	 *  viewport and did nothing here, over a panel that is a scene outliner in
	 *  every other respect. Return true to consume the key.
	 *
	 *  It has to run FIRST, ahead of keyPressEvent's own body: that body returns
	 *  early (swallowing the key) whenever the NifModel is driving the view and
	 *  nothing is selected, then hands what is left to the spell book, then to
	 *  QTreeView, whose keyboardSearch would treat a bare 'h' as type-to-find.
	 *  Unset for the field views. */
	std::function<bool( QKeyEvent * )> wwKeyHook;

	/*! The Block List's per-row visibility toggles (WW): which glyph is under a
	 *  point in this row, or -1.
	 *
	 *  Two hooks rather than one bool, for the reason the Loaded-NIFs strip is
	 *  built the same way: the VIEW has to own the whole press-move-release
	 *  gesture so that a click on a glyph neither selects the row nor starts a
	 *  drag, while the WINDOW is the only thing that knows what a row means as a
	 *  block number and what a toggle means to the scene. */
	std::function<int( const QModelIndex &, const QPoint & )> wwVisSlotAt;
	//! Flip one toggle. Called on RELEASE, and only over the pressed glyph.
	std::function<void( const QModelIndex &, int slot )> wwVisToggle;
	//! How many toggles the gesture above has completed. The harness reads it to
	//! tell "the click did nothing" from "the click never arrived".
	int wwVisTogglesDone = 0;

	//! How many drag events the overrides above have been handed. The harness
	//! reads it to prove the override ran, rather than measuring a hook it
	//! called itself.
	int wwDragEventsSeen = 0;

	/*! Hand a drag event to the same overrides Qt's routing would.
	 *
	 *  A drag event CANNOT be delivered with QApplication::sendEvent:
	 *  QApplication::notify routes drag and drop through the drag manager, so a
	 *  synthetic one reaches neither the widget's event() nor any event filter.
	 *  That was measured both ways -- sent to the view and sent to the viewport,
	 *  the override count stayed at zero and no filter fired -- after an event
	 *  filter on the viewport was tried first and silently did nothing.
	 *
	 *  So a harness needs an entry point that begins where Qt's routing ends.
	 *  This is it: everything below this line is ours and is covered; the one
	 *  step above it, viewport -> viewportEvent() -> these overrides, is Qt's.
	 */
	void wwDeliverDragEvent( QEvent * e );

	/*! The insertion line for a reorder drag: viewport y, or -1 for none.
	 *
	 *  Drawn here rather than through Qt's drop indicator because that one is set
	 *  only inside QAbstractItemView::dragMoveEvent, which asks the model
	 *  canDropMimeData() first — and NifModel does not implement it, so the
	 *  indicator never appears no matter what the drop does.
	 */
	int wwDropLineY = -1;
	int wwDropLineFrom = 0;

	//! Minimum size
	QSize minimumSizeHint() const override final { return { 50, 50 }; }
	//! Default size
	QSize sizeHint() const override final { return { 400, 200 }; }

signals:
	//! Signal emmited when the current index changes; probably connected to NifSkope::select()
	void sigCurrentIndexChanged( const QModelIndex & );

public slots:
	//! Sets the root index
	void setRootIndex( const QModelIndex & index ) override final;
	//! Clear the root index; probably conncted to NifSkope::dList
	void clearRootIndex();

	//! Sets Hiding of non-applicable rows
	void setRowHiding( bool );

	//! Re-apply row hiding over the whole visible block. Safe to call any
	//! time; defers itself while the model is loading/processing (a bailed
	//! mid-load application would otherwise strand version-mismatched rows
	//! visible, since a later select() of the same block skips setRootIndex).
	void refreshRowHiding();

	//! QTreeView::reset() clears all hidden-row state; re-apply row hiding
	//! after every model reset so version-mismatched rows stay hidden.
	void reset() override;

	//! The hidden-row state lives in QPersistentModelIndexes that assorted
	//! model activity can silently invalidate (with no reset signal). Re-derive
	//! it whenever the view rebuilds its layout, so version-gated rows can
	//! never "un-hide" behind our back.
	void doItemsLayout() override;

	//! Updates version conditions (connect to dataChanged)
	void updateConditions( const QModelIndex & topLeft, const QModelIndex & bottomRight );
protected slots:
	//! Recursively updates version conditions. descendArrays=false skips the
	//! contents of arrays (their element rows are not individually
	//! version-gated) — used by the frequent doItemsLayout re-derivation so a
	//! block with huge vertex/triangle arrays stays cheap to relayout; the
	//! root-change path keeps the full walk to derive array-member hiding.
	void updateConditionRecurse( const QModelIndex & index, bool descendArrays = true );
	//! Expanded-aware re-derivation for the dataChanged path: applies row
	//! hiding to exactly the rows the user can see, descending only into
	//! open subtrees (closed ones re-derive on expansion via the lazy hook).
	//! Bounds a block-field edit to O(visible rows) instead of walking every
	//! element of a 38k-row vertex array.
	void updateConditionsLazy( const QModelIndex & index );
	//! Called when the current index changes
	void currentChanged( const QModelIndex & current, const QModelIndex & previous ) override final;

	//! Scroll to index; connected to expanded()
	void scrollExpand( const QModelIndex & index );

	void onItemCollapsed( const QModelIndex & index );

protected:
	void drawBranches( QPainter * painter, const QRect & rect, const QModelIndex & index ) const override final;
	void paintEvent( QPaintEvent * e ) override final;
	void startDrag( Qt::DropActions supportedActions ) override final;
	void dragEnterEvent( QDragEnterEvent * e ) override final;
	void dragMoveEvent( QDragMoveEvent * e ) override final;
	void dragLeaveEvent( QDragLeaveEvent * e ) override final;
	void dropEvent( QDropEvent * e ) override final;
	void keyPressEvent( QKeyEvent * e ) override final;
	void mousePressEvent( QMouseEvent * event ) override final;
	void mouseReleaseEvent( QMouseEvent * event ) override final;
	void mouseMoveEvent( QMouseEvent * event ) override final;

	void autoExpandBlock( const QModelIndex & blockIndex );
	void autoExpandItem( const NifItem * item );

	bool doRowHiding = true;
	//! re-entry guard for the doItemsLayout hiding refresh
	bool inLayoutHidingRefresh = false;
	bool autoExpanded = false;
public:
	// Do "smart auto-expand" of items when the view changes NiBlock.
	bool doAutoExpanding = false;
protected:
	// Block mouseRelease and mouseMove events processing. Is reset on mousePress event.
	// This is a workaround for the following "feature" of QTreeView in Qt 5:
	//     If you click the expand/collapse icon (">") of an item with a mouse button
	//     and this leads to the item shifting in the view (e.g., see scrollExpand here),
	//     then, when you release the mouse button (or move the cursor), you may accidently select several items in the view
	//     because QTreeView treats this as if you'd click on the expanded/collapsed item and drag the cursor to another item.
	bool blockMouseSelection = false;

	//! In-flight visibility-glyph press: the row and which glyph. While a slot
	//! is claimed the view owns the gesture outright — see mouseMoveEvent.
	QPersistentModelIndex pressedVisRow;
	int pressedVisSlot = -1;

	class BaseModel * nif = nullptr;

	//! Row Copy
	void copy();
	//! Row Paste
	void paste();
	void pasteTo( const QModelIndex idx, const NifValue & srcValue );

	//! Array/Compound Paste
	void pasteArray();

	//! Get a list of only the value column fields from lists of rows
	QModelIndexList valueIndexList( const QModelIndexList & rows ) const;
};


//! A global clipboard for NifTreeView to store a NifValue for all windows
class NifValueClipboard
{

public:
	NifValue getValue() { return value; }
	void setValue( const NifValue & val ) { value = val; }

	const std::vector<NifValue>& getValues() const { return values; }
	void setValues( const std::vector<NifValue>& vals ) { values = vals; }

	void clear()
	{
		value.clear();
	}

private:
	//! The value stored from a single row copy
	NifValue value = NifValue();
	//! The values stored from a single array copy
	std::vector<NifValue> values;
};

// The global NifTreeView clipboard pointer
static auto valueClipboard = std::make_unique<NifValueClipboard>();

#endif
