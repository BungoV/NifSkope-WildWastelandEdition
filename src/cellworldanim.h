#ifndef CELLWORLDANIM_H
#define CELLWORLDANIM_H

/*! lane FXPARITY2 (WORLDANIM1): THE CELL'S WORLD CLOCK. With the World animation row (WW/CellWorldAnim, pin
 *  WW_CELL_WORLDANIM) a cell document animates on a clock of its own: every controller (UV scroll, visibility,
 *  transforms), the copied particle systems and the water's noise scroll. A live or fly session (WW_FLY=1) reads the
 *  wall clock; a harness stands at WW_CELL_WORLDANIM_TIME (0 when unset). Red WW_CELL_WORLDANIM_RED=still holds it at 0.
 *  The Animated objects row (WW/CellAnimObjects, pin WW_CELL_ANIMOBJ) copies placed models with controllers whole
 *  instead of welding them (src/cellview.cpp); each copy's looping sequence is bound here. Off: nothing here runs. */

#include <QString>
#include <QVector>

class NifModel;
class Scene;

//! the World animation row (or its pin)
bool wwCellWorldAnimOn();
//! the world clock's seconds (0 in a harness without WW_CELL_WORLDANIM_TIME, and with the red)
double wwCellWorldSeconds();
//! one call a paint, before the scene transforms: a cell with the row on animates at the world clock (writes `time`)
bool wwCellWorldAnimFrame( Scene * scene, float & time );
//! the clock moves in this view: it wants a frame every tick
bool wwCellWorldAnimWantsFrames( Scene * scene );

//! an animated object copied in whole: its root block and the looping sequence it plays (empty: free controllers only)
struct WwAnimObjRoot
{
	qint32 root = -1;
	QString loopSeq;
	QString model;	// lane FIREFX1: the census names it
};
//! cellBuild: the copies of this document (replaces any earlier list for it)
void wwCellAnimObjSet( const NifModel * nif, const QVector<WwAnimObjRoot> & roots );

/*! lane FIREFX1: THE COPIES' DRAW CENSUS. Once per cell document, on its third paint, one line per copied model:
 *  `cell animated objects drawn: <model> x<copies>: shapes S, in the scene tree T, hidden H, reached a draw D; first at
 *  x y z`, and a total line. "Reached a draw" = BSShape::drawShapes got its program for the shape on the second paint
 *  (wwCellAnimObjNoteDraw), before any cull. Runs with the World animation row off too. */
void wwCellAnimObjCensus( Scene * scene );
//! BSShape::drawShapes: this shape got its program this paint (a no-op unless the census is listening)
void wwCellAnimObjNoteDraw( const void * shape );

#endif // CELLWORLDANIM_H
