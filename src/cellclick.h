/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLCLICK_H
#define CELLCLICK_H

#include <QModelIndex>
#include <QObject>

class NifModel;

/* Clicking a reference in the cell view: the viewport calls cellPickClick() once
 * from GLView::mouseReleaseEvent; the ray test, highlight and panel update live here.
 * The highlight is a BSTriShape created once per scene and moved per pick (never
 * inserted/removed), so the read-only document never changes structure. */

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

/*! The viewport's one call. False (nothing touched) when the master is off, no cell
 *  scene is open, or the ray hit no box, so the caller falls through to ordinary selection. */
bool cellPickClick( NifModel * nif, const float origin[3], const float direction[3] );

//! The current pick, or -1. `cellPickTable()` is where its fields live.
int cellPickCurrent();

/*! Select entry `index` exactly as a click on it would, so list rows and viewport clicks
 *  end in the same state. Ignores the master switch; `index < 0` deselects; false if out of range. */
bool cellPickSelect( NifModel * nif, int index, int candidates = 1 );

/*! The block origin the cell scene's shapes carry as their Translation, as
 *  `cellHighlightCreate` was given it.  Anything that aims the CAMERA at a
 *  pick-table box needs it: the table is in world units and the scene is not. */
void cellPickOrigin( float out[3] );

/*! Create the highlight shape under `iRoot`, once, at the end of a cell build.
 *  `origin` is the scene's block origin, so the bars share the geometry's space. Returns the block or -1. */
int cellHighlightCreate( NifModel * nif, const QModelIndex & iRoot, const float origin[3] );

//! Move the bars onto this world box; `thickness` is in world units.
void cellHighlightShow( NifModel * nif, const float bmin[3], const float bmax[3] );
//! Collapse the bars to a point. The block stays, so nothing structural moves.
void cellHighlightHide( NifModel * nif );
//! Forget the shape (a new document is being built).
void cellHighlightForget();

#endif // CELLCLICK_H
