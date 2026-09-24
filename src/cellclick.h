/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLCLICK_H
#define CELLCLICK_H

#include <QModelIndex>
#include <QObject>

class NifModel;

/* ---------------------------------------------------------------------------
 * CLICKING A REFERENCE IN THE CELL VIEW.
 *
 * `src/cellpick.h` has been complete since 2026-09-19 12:26 -- every placement
 * with its world box, a CPU ray test, and the flat Name|Value rows a panel
 * would show -- and it had exactly ONE caller in the whole tree: the builder
 * that fills it.  Nothing read it back, because the mouse lives in
 * `src/glview.cpp` and the docks in `src/nifskope_ui.cpp`.  This file is the
 * join, and it is deliberately the smallest possible one:
 *
 *   - the viewport calls `cellPickClick( origin, direction )` ONCE, from inside
 *     `GLView::mouseReleaseEvent`, and gets back true when a reference was
 *     picked.  It needs no cell-view header, no pick-table type and no panel.
 *   - everything else -- the ray test, the highlight, telling the panel --
 *     happens here.
 *
 * ===========================================================================
 * THE MASTER SWITCH
 * ===========================================================================
 *
 * `cellPickEnabled()` is FALSE until somebody ticks Render > Cell Pick Panel
 * (the Render menu, not View: `View` is removed from this fork's menubar in
 * src/nifskope_ui.cpp ~25782),
 * which is the menu row the switch ships with (bungo's standing rule: every
 * feature master ships OFF, and no INI key exists without a row).  While it is
 * false this file changes NOTHING: the click falls through to the ordinary
 * block selection, the panel is hidden, and the highlight shape is never
 * written.  The colour overlays INSIDE the cell view are not masters and have
 * no row -- they are one view's display setting, not a feature.
 *
 * ===========================================================================
 * THE HIGHLIGHT IS DOCUMENT GEOMETRY, NOT A RENDERER OVERLAY
 * ===========================================================================
 *
 * The scene welds, so the picked reference is a few hundred vertices inside a
 * shape holding a hundred thousand, and there is nothing for the application's
 * own selection highlight to light up.  A renderer overlay is the obvious
 * answer and it is the one thing this lane may not write: `src/glview.cpp` and
 * `src/gl/` belong to other lanes.
 *
 * So the highlight is a BSTriShape like every other part of this scene: twelve
 * thin bars along the edges of the picked placement's world box, created ONCE
 * when the cell scene is built (`cellHighlightCreate`) and MOVED on each pick
 * (`cellHighlightShow`).  Created once and moved, rather than inserted and
 * removed per click, because inserting and removing blocks in a document the
 * user is looking at is a structural edit and this viewer is read-only.
 * Hiding it collapses its vertices to a point: no block appears or disappears,
 * the block list never changes shape, and a screenshot of a pick is a
 * screenshot of the document.
 * --------------------------------------------------------------------------- */

//! The master. False until the menu row is ticked; nothing here acts while it is false.
bool cellPickEnabled();
void cellPickSetEnabled( bool on );

/*! Tells listeners a pick happened. `index` is into `cellPickTable()`, -1 when
 *  the ray entered no box; `candidates` is how many boxes it entered at all, so
 *  the panel can say "3 under the cursor" instead of implying there was one. */
class CellPickBus : public QObject
{
	Q_OBJECT
public:
	static CellPickBus * instance();
signals:
	void picked( int index, int candidates );
	//! A new cell scene was built: whatever the panel was showing is gone.
	void sceneChanged();
};

/*! THE VIEWPORT'S ONE CALL. False -- and nothing touched -- when the master is
 *  off, when no cell scene is open, or when the ray entered no box, so the
 *  caller can fall through to its ordinary selection in every one of those
 *  cases and a user who never ticks the row loses nothing they had. */
bool cellPickClick( NifModel * nif, const float origin[3], const float direction[3] );

//! The current pick, or -1. `cellPickTable()` is where its fields live.
int cellPickCurrent();

/*! SELECT ENTRY `index` EXACTLY AS A CLICK ON IT WOULD (lane CELLWORK1).
 *
 *  `cellPickClick` above is now the ray test plus a call to this; the Cell
 *  workspace's reference list calls this directly.  One function, so a row in
 *  the list and the mouse in the viewport cannot end in different states -- the
 *  same current index, the same highlight bars, the same `picked` signal.  That
 *  identity is what the workspace gate asserts, and it is only worth asserting
 *  because there is exactly one place it could break.
 *
 *  `candidates` is what the caller wants reported to the panel: the ray test
 *  passes how many boxes it entered, a list row passes 1 because the row NAMED
 *  the reference and nothing was ambiguous about it.
 *
 *  The master switch is deliberately NOT consulted here.  `cellPickClick` keeps
 *  its guard, because a click in the viewport must fall through to the ordinary
 *  block selection while the row is unticked; naming a reference in a list is
 *  not a click that could fall through to anything.
 *
 *  False, and nothing touched, when there is no table or the index is out of
 *  range.  `index < 0` is a deselect and returns true. */
bool cellPickSelect( NifModel * nif, int index, int candidates = 1 );

/*! The block origin the cell scene's shapes carry as their Translation, as
 *  `cellHighlightCreate` was given it.  Anything that aims the CAMERA at a
 *  pick-table box needs it: the table is in world units and the scene is not. */
void cellPickOrigin( float out[3] );

/*! Create the highlight shape under `iRoot`, once, at the end of a cell build.
 *  `origin` is the block origin the scene's shapes carry as their Translation,
 *  so the bars land in the same space the geometry does. Returns the block
 *  number, or -1. */
int cellHighlightCreate( NifModel * nif, const QModelIndex & iRoot, const float origin[3] );

//! Move the bars onto this world box; `thickness` is in world units.
void cellHighlightShow( NifModel * nif, const float bmin[3], const float bmax[3] );
//! Collapse the bars to a point. The block stays, so nothing structural moves.
void cellHighlightHide( NifModel * nif );
//! Forget the shape (a new document is being built).
void cellHighlightForget();

#endif // CELLCLICK_H
