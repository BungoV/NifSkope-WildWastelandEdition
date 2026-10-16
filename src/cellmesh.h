#ifndef CELLMESH_H
#define CELLMESH_H

/* The cell view's welded geometry, kept beside the document: a welded shape's vertex and triangle rows stay
 * unwritten (writing them costs ~1.7 kB a vertex) and BSShape takes these arrays directly. The rows are written
 * when something asks for the document itself (cellMeshMaterialize, cellMeshRowsAsked). WW_CELL_SPEED_RED=slow: write all rows at once. */

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
	//! the far objects' UV 2 (0, layer); empty = the shape has no UV 2 stream
	QVector<float> uv2y;
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

/* The net under every other reader: the model's by-name lookup (BaseModel::getItemInternal) asks here before
 * handing out an EMPTY array while shapes wait, and the rows are written before the caller sees it. */
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

//! WW_CELL_SPEED_SAVE=<file>: the cell's document, saved as File > Save would.
void cellMeshSaveIfAsked( NifModel * nif );

#endif
