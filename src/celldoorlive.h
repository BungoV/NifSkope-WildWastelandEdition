#ifndef CELLDOORLIVE_H
#define CELLDOORLIVE_H

/*! lane DOORLIVE1: ONE LIVE DOOR IN THE CELL VIEW. WW_CELL_DOOR_ANIM=<ref hex>:<open|close|cycle> picks a placed door;
 *  its model is copied in whole (not welded) so its own Open / Close NiControllerSequence moves it, and the GI follows:
 *  the relight's door slot for it is shut by (1 - open fraction) and run on the GPU on each change (no rebake), the
 *  published grid = the bake + (that run - the all-open run). The cell lights' shadow cubes re-render while it moves.
 *  WW_CELL_DOOR_ANIM=list ranks the cell's doors by the light each one stops and suggests a dark-side camera.
 *  Reds: WW_CELL_DOORLIVE_RED=static (the GI stays at the bake while the mesh moves) | snap (the GI flips at half open).
 *  Off (unset): nothing here runs and the cell builds as before. */

#include <QString>

#include <vector>

class NifModel;
class QWindow;
class Scene;
struct ProbeRelightOps;
struct ProbeRelightOut;
struct WwCellGi;

//! a door's box as the probe soup tagged it
struct WwDoorLiveBox
{
	quint32 ref = 0;
	float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
};

//! the live door's ref (0: off or list mode)
quint32 wwDoorLiveRef();
//! the cell build should record the relight's operators and the doors' geometry (live or list)
bool wwDoorLiveWantsGi();

//! cellBuild: the door's branch was copied in under `wrapper` (its root at `root`, `count` blocks from there)
void wwDoorLiveSetBranch( NifModel * nif, quint32 ref, const QString & model, qint32 wrapper, qint32 root, qint32 count,
	const QString & note );
//! cellBuild: the relight's operators and the grid just published; returns census lines for the notes
QString wwDoorLiveKeepGi( const void * nif, const ProbeRelightOps & ops, const WwCellGi & gi,
	const std::vector<WwDoorLiveBox> & boxes );

//! One call a frame (before the scene transforms): poses the door by the wall clock, relights when it moved
void wwDoorLiveTick( Scene * scene, QWindow * view );
//! A path frame (WW_RENDER_PATH): poses the door at `seconds` of its clock and relights at once; the echo line
QString wwDoorLiveStep( Scene * scene, double seconds );
//! The test driver's door:<open|close|cycle|t=s|state> and the port's NifSkope::door; the echo line
QString wwDoorLiveCommand( Scene * scene, const QString & arg );
//! The census line (door ref, state, open fraction, relight ms)
QString wwDoorLiveEcho();

//! lane LIGHTPHYS4: the published grid's door share, rgba += shut run - all-open run (grid, and grid2 when rgba holds both)
void wwDoorLiveDelta( std::vector<float> & rgba, size_t nVox, const ProbeRelightOut & shut, const ProbeRelightOut & open );
/*! lane LIGHTPHYS4: the light rows changed; probesky relit the bake (`relit` = its new grid, null when it did not).
 *  The live door takes that grid as its base and the current rows, and relights at the next frame. */
void wwDoorLiveRowsRelit( const void * nif, const WwCellGi * relit );

#endif // CELLDOORLIVE_H
