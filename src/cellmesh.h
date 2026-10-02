#ifndef CELLMESH_H
#define CELLMESH_H

/* Lane SPEED1 (2026-10-02): THE CELL VIEW'S WELDED GEOMETRY, BESIDE THE DOCUMENT.
 *
 * A cell is welded into a few hundred shapes of up to 65,000 vertices. Writing each
 * vertex into the document costs eight or nine document items (about 1.7 kB a
 * vertex, measured: Sanctuary 5x5, 5.0 M vertices, 14.9 GB) and a name lookup per
 * field, and the scene then reads every one of them back. The picture needs none of
 * that: the scene wants seven plain arrays.
 *
 * So the welded shape keeps its block, name, counts, bounding sphere and material in
 * the document, and its vertex and triangle ROWS stay unwritten. The arrays wait
 * here, in exactly the values the document would hand back (the document keeps
 * floats; it packs them only when it writes a file), and BSShape takes them as they
 * are. The rows are written the moment something asks for the document itself:
 * selecting the shape, a spell, an export, a save (cellMeshMaterialize), or any
 * reader that looks the rows up by name (cellMeshRowsAsked). After that the shape
 * is an ordinary document shape and this table has forgotten it.
 *
 * WW_CELL_SPEED_RED=slow is the old path whole (every row written at once), kept as
 * the gate's reference and red control (tests/spells/cell_speed.sh). */

#include "data/niftypes.h"

#include <QModelIndex>
#include <QSharedPointer>
#include <QVector>

#include <atomic>

class NifItem;
class NifModel;

struct CellMesh
{
	QVector<Vector3> verts, norms, tangents, bitangents;
	QVector<Color4> colors;        //!< always one per vertex; (0,0,0,1) when the shape has no color stream
	QVector<Vector2> coords;
	QVector<Triangle> triangles;
	bool withColour = false;       //!< the shape's vertex format carries the color stream
};

//! False under WW_CELL_SPEED_RED=slow: the caller writes every row into the document, as before.
bool cellMeshOn();
//! Forget every shape of this document (a new cell is about to be built in it).
void cellMeshClear( const NifModel * nif );
void cellMeshPut( NifModel * nif, const QModelIndex & iShape, const QSharedPointer<CellMesh> & mesh );
//! The arrays of a welded shape whose rows are not in the document; null for every other block.
QSharedPointer<CellMesh> cellMeshFor( const NifModel * nif, const QModelIndex & iShape );
//! Write the rows of the welded shape holding `index` (any index inside its block). False = nothing to do.
bool cellMeshMaterialize( NifModel * nif, const QModelIndex & index );
//! Write the rows of every welded shape of this document; the count written.
int cellMeshMaterializeAll( NifModel * nif );
//! Shapes of this document still waiting here.
int cellMeshCount( const NifModel * nif );
//! The file bytes of a waiting shape's unwritten rows (its "Data Size"); -1 for every other block.
//! NifModel::updateHeader asks, so that it leaves those row arrays alone and still states the block's size.
int cellMeshWaitingBytes( const NifModel * nif, const NifItem * block );

/* THE NET UNDER EVERY OTHER READER. Some two hundred places read a shape's rows out of a document by name
 * (the mesh tools, the UV editor, exports, a second document's ghost ...). None of them is taught about this
 * table: the model's own by-name lookup (BaseModel::getItemInternal) asks here when it is about to hand out an
 * EMPTY array while shapes are waiting, and the rows are written before the caller sees the array. */
extern std::atomic<int> cellMeshWaiting;   //!< shapes waiting, all documents; 0 = the lookup pays one load
class BaseModel;
//! `array` (empty, a child of `block`) is being looked up by name: write the block's rows if they wait here.
void cellMeshRowsAsked( const BaseModel * model, const NifItem * block, const NifItem * array );
//! While one lives, lookups do not write rows (the scene taking the arrays, the builder still filling the shape).
struct CellMeshQuiet
{
	CellMeshQuiet();
	~CellMeshQuiet();
	Q_DISABLE_COPY( CellMeshQuiet )
};

//! WW_CELL_SPEED_SAVE=<file> (tests/spells/cell_speed.sh): the cell's document, saved as File > Save would.
void cellMeshSaveIfAsked( NifModel * nif );

#endif
