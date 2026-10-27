#ifndef CELLFARHIDE_H
#define CELLFARHIDE_H

/* lane FARHOLE2 (2026-10-27): the far field leaves room for every streamed near cell, wherever the camera flew.
 *
 * The far objects, cards and rings past 0 are cut once, round the OPEN's centre; the stream follows the camera with
 * no limit, so a few cells out the loaded cells drew the far field inside them. This table holds, per document, the
 * cells the stream has attached and the REFRs those cells list. Each far layer asks it:
 *   - instanced far object rows (gl/cellfarinst): a row whose REFR a loaded cell lists does not draw (that cell draws
 *     the full object); a row with no REFR goes by the cell it stands in. A neighbour's object reaching in stays,
 *     because nothing near draws it.
 *   - every other far shape (ring 1+ terrain and water, the game's .bto/.btr chunks, flattened rings): triangles
 *     centred in a loaded cell are degenerated when the shape takes its arrays (gl/bsshape.cpp).
 *   - the far cards (impostorchunk): cards placed in a loaded cell.
 * WW_CELL_FARHIDE_RED=off (nothing hidden: origin/main's picture), =bypos (rows by placement cell only, FARHOLE's
 * rule), =noshadow (the sun's cached shadow is not told). Read live. */

#include "cellmesh.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QVector>

class Transform;

namespace CellFarHide
{
enum Mode { Off = 0, ByPosition = 1, ByRef = 2 };
//! The live mode (WW_CELL_FARHIDE_RED)
int mode();
//! False under WW_CELL_FARHIDE_RED=noshadow: the shadow cache does not see the rows change
bool shadowKeyed();

inline qint64 cellKey( int x, int y ) { return ( qint64( x ) << 32 ) ^ qint64( quint32( y ) ); }

struct Hidden
{
	QSet<qint64> cells;
	QHash<quint32, int> refs;    //!< REFR -> loaded cells listing it
	quint64 gen = 0;             //!< bumped on every change
};

//! The document's hidden set; null when nothing was ever hidden in it
const Hidden * hiddenFor( const void * doc );
//! A streamed cell attached (hide) or left; `refs` = the REFRs it lists (the same on both calls)
void setCell( const void * doc, int x, int y, bool hide, const QVector<quint32> & refs );
//! Forget the document (its stream stopped)
void clear( const void * doc );

//! Pure: is row i of an instanced bucket hidden by `h` in `mode`
bool rowHidden( const CellMeshInst & d, int row, const Hidden & h, int mode );
//! Pure: the commands and tiles drawing only the rows `h` leaves; returns the rows hidden
qint64 filter( const CellMeshInst & d, const Hidden & h, int mode, QVector<CellMeshInst::Cmd> & cmds,
	QVector<CellMeshInst::Tile> & tiles );

struct View
{
	const QVector<CellMeshInst::Cmd> * cmds = nullptr;
	const QVector<CellMeshInst::Tile> * tiles = nullptr;
	quint64 key = 0;             //!< 0 = every row draws; changes whenever the rows drawn change
	qint64 hiddenRows = 0;
};
//! What an instanced bucket draws now (cached per change)
View view( const CellMeshInst & d );

//! A non-instanced far shape the stream may mask; cx0..cy1 = the cells its bound touches
void registerMask( const void * doc, int block, int cx0, int cy0, int cx1, int cy1 );
//! Cheap: is `block` a registered mask shape of the document
bool maskRegistered( const void * doc, int block );
//! The registered blocks whose bound touches cell (x, y) (all of them for x = y = INT_MIN)
QList<int> maskBlocks( const void * doc, int x, int y );
//! BSShape::updateData: degenerate the triangles of a registered block centred in a hidden cell; the count hidden
int applyMask( const void * doc, int block, const Transform & world, const QVector<Vector3> & verts,
	QVector<Triangle> & tris );
//! Pure: the same over `cells`
int maskPure( const Transform & world, const QVector<Vector3> & verts, QVector<Triangle> & tris,
	const QSet<qint64> & cells );
//! Triangles the masks hid at their last apply, all blocks of the document
qint64 maskedTris( const void * doc );
}

#endif
