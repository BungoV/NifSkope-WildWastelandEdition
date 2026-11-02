#ifndef CELLEDIT_H
#define CELLEDIT_H

/*
 * Lane ESPWRITE2 (2026-11-02): edits made in the cell view, saved as a Fallout 4 override plugin (src/io/espwrite.h).
 * Design: notes/espwrite-r/DESIGN.md sections 3-4 (skill fo4-cell-override-write).
 *
 * THE CORE (no widgets; the CLI `-no-gui espwrite place` and tests use it):
 *   - CellEditItem: one placed reference (base in LOAD-ORDER numbering, pose, persistence);
 *   - cellEditResolveEdid: an EditorID -> load-order form + OBND (each plugin's base groups indexed once, cached);
 *   - cellEditMakePlan / cellEditSave: the open cell's winner is copied, its masters + itself are the masters,
 *     each base is renumbered into that list (refused by name when its plugin is not in it); "Add to existing"
 *     carries the file's own refs (their ids kept) and overrides; then the .wwcell for the new load order;
 *   - cellEditPose: the REFR rotation for a yaw, Z-up or flat on a surface normal (the drawn convention
 *     fromEuler( -rx, -ry, -rz ), forward +Y);
 *   - cellEditGhostBox: the ghost's box and heading tip (gate 5).
 * THE PLACE MODE (GLView hooks in src/glview.cpp; the panel in src/celleditpanel.cpp): Shift+A starts it; the ghost
 *   follows the surface under the cursor (src/cellsurface.h); LMB drops it, a drag sets the yaw toward the drag point;
 *   T then a click faces it toward a second point; R + wheel turns it (15 deg, Ctrl 1 deg); N flat / Z-up;
 *   G ground snap; Ctrl+Z undoes; Esc leaves. Nothing touches the document: the edits are an overlay until Save.
 *
 * Switches: WW_CELL_EDIT=0 hides the panel section. WW_CELL_EDIT_SCRIPT=<file> drives the place mode from a file
 *   once a cell is open (the bench's use case; the verbs are listed in celledit.cpp scriptStep). Red WW_CELLEDIT_RED
 *   (read on every call) = `ghost`: the ghost uses the un-negated euler (the wrong sign; gate 5 must fail).
 */

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct CellSceneSpec;
class GLView;
class Scene;
class Transform;
namespace EspWrite { struct Plan; }

struct CellEditItem
{
	quint32 base = 0;           //!< LOAD-ORDER form id (as EsmWorld numbers it)
	QString baseEdid;
	quint32 baseFile = 0;       //!< filled by cellEditMakePlan: the id written
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 }; //!< radians, as stored
	float scale = 1.0f;
	bool persistent = true;
	quint32 formOut = 0;        //!< after a save: the new ref's id in the file
	float lo[3] = { -16, -16, -16 }, hi[3] = { 16, 16, 16 };   //!< the base's OBND (model space)
};

struct CellEditTarget
{
	QString path;               //!< the plugin written
	bool append = false;        //!< "Add to existing": keep the file's own refs
	bool light = true;          //!< ESL flag
};

//! WW_CELLEDIT_RED, read on every call
QString cellEditRed();
//! WW_CELL_EDIT (default on): the panel section shows
bool cellEditEnabled();

/*! An EditorID of a base form -> its load-order id, record type and OBND (model-space box; +-16 when absent). */
bool cellEditResolveEdid( const QStringList & plugins, const QString & edid, quint32 & loForm, QString & sig,
	float lo[3], float hi[3], QString * error );

//! The REFR rotation of a pose: yaw (radians, 0 = +Y, clockwise from above); flat aligns up to `normal`.
void cellEditPose( float yaw, bool flat, const float normal[3], float rot[3] );
//! The drawn rotation matrix of a REFR rotation (m[i][j], world = M * local); redGhost = the un-negated euler.
void cellEditMatrix( const float rot[3], float m[3][3], bool redGhost = false );
//! Yaw that faces `to` from `from` in the horizontal plane.
float cellEditYawToward( const float from[3], const float to[3] );

//! The ghost box: the AABB of the OBND corners under the pose, and the heading arrow's tip (pos + M * (0, 48, 0)).
void cellEditGhostBox( const CellEditItem & it, float lo[3], float hi[3], float tip[3], bool redGhost = false );
QString cellEditGhostLine( const CellEditItem & it );

/*! Build the plan for the open cell (spec) with `items`. On success `summary` is the action bar's sentence. */
bool cellEditMakePlan( const CellSceneSpec & spec, QVector<CellEditItem> & items, const CellEditTarget & target,
	EspWrite::Plan & plan, QString * summary, QString * error );
/*! Make the plan, write it (QSaveFile), write the .wwcell for <load order>,<target> into `specDir` (empty =
 *  openCellSpecDir()) and return it in `reopenPath`. `line` = the status line. */
bool cellEditSave( const CellSceneSpec & spec, QVector<CellEditItem> & items, const CellEditTarget & target,
	const QString & specDir, QString * reopenPath, QString * line, QString * error );
//! The .wwcell text of `spec` with `target` added to its load order (when it is not in it).
QString cellEditReopenText( const CellSceneSpec & spec, const QString & target );

// ---------------------------------------------------------------- the session (place mode, edit list)
class CellEditBus : public QObject
{
	Q_OBJECT
public:
	static CellEditBus * instance();
signals:
	//! the edit list, the place mode or the ghost changed
	void changed();
	//! a status sentence (a refusal when `refusal`)
	void status( const QString & text, bool refusal );
	//! Save wrote this .wwcell: open it
	void reopen( const QString & wwcell );
};

struct CellEditSession
{
	bool placing = false;
	bool facing = false;        //!< T: the next click faces the ghost toward it
	bool turning = false;       //!< R: the wheel turns the ghost
	bool flat = false;
	bool snap = false;
	bool locked = false;        //!< the ghost stays where it is (T pressed, or LMB held for a drag)
	float yaw = 0.0f;
	CellEditItem ghost;         //!< base + OBND of the base being placed; pos = the hover point
	bool ghostValid = false;
	bool baseValid = false;
	float normal[3] = { 0, 0, 1 };
	QVector<CellEditItem> edits;
	CellEditTarget target;
};

CellEditSession & cellEditSession();

//! Start / stop the place mode with the current base (false: no cell open or no base).
bool cellEditStartPlace( GLView * view );
void cellEditStopPlace();
bool cellEditActive();
//! Pick the base being placed (by EditorID); false + a status refusal when it does not resolve.
bool cellEditSetBase( const QString & edid );
void cellEditUndo();
void cellEditRemove( int index );
void cellEditClear();
//! Save the edit list to the session's target; emits reopen() on success.
bool cellEditSaveSession( QString * line );
//! The action bar's sentence for the session, or the one reason it cannot save (refusal = true).
QString cellEditSummary( bool * refusal );
//! The list row of an edit.
QString cellEditRowText( const CellEditItem & it );

// ---------------------------------------------------------------- GLView hooks (src/glview.cpp)
enum class CellEditMouse { Press, Move, Release, Wheel };
//! A mouse event; true = the place mode consumed it. `left` = the left button is the one pressed / held.
bool cellEditMouse( GLView * view, CellEditMouse kind, double x, double y, bool left, int modifiers, int wheel = 0 );
//! A key; true = consumed (Shift+A starts the mode in a cell document).
bool cellEditKey( GLView * view, int key, int modifiers );
//! The overlay point of paintGL: the depth probe for the pending pixel, then the ghost and the placed edits.
void cellEditFrame( GLView * view, Scene * scene, const Transform & viewTrans );

#endif // CELLEDIT_H
