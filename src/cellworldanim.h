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
};
//! cellBuild: the copies of this document (replaces any earlier list for it)
void wwCellAnimObjSet( const NifModel * nif, const QVector<WwAnimObjRoot> & roots );

#endif // CELLWORLDANIM_H
