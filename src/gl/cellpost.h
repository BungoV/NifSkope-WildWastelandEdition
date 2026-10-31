#ifndef CELLPOST_H
#define CELLPOST_H

#include <QString>

class EsmWorld;
class Scene;

/*! The game's depth of field and motion blur on the cell view's tone-mapped frame, before the temporal AA (the
 *  game's order); parameters from the IMGS DNAM (weather or cell XCIM), shaders game_dof*.frag / game_mblur.frag.
 *  Both rows ship OFF and then touch nothing. WW_CELL_DOF=0|1, WW_CELL_MBLUR=0|1 pin them;
 *  WW_CELL_DOF_RED, WW_CELL_MBLUR_RED and WW_CELL_POST_DUMP=<dir> (frame WW_CELL_POST_DUMP_FRAME) are debug aids. */

//! The rows' state (pinned, saved, or OFF in a harness run).
bool wwCellDofOn();
void wwCellDofSetOn( bool on );
bool wwCellMotionBlurOn();
void wwCellMotionBlurSetOn( bool on );

//! The cell view, at cell open: indoors the cell's XCIM record's DNAM (read here, 16 bytes as stored), outdoors the weather.
void wwCellPostSetCell( const EsmWorld & world, bool interior );

/*! The camera path: frame index k (>= 0) at fps frames a second; -1 ends the path (an interactive window
 *  measures its own frame time). */
void wwCellPostSetPath( int frame, float fps );
//! lane PERF2: the camera path's frame index (-1 outside a path)
int wwCellPostPathFrame();
//! The path's frames a second (0 off a path)
float wwCellPostPathFps();

//! After the tone map, before the temporal AA: the depth of field, then the motion blur, the frame back. Inert when
//! both rows are off.
void wwCellPostApply( Scene * scene );

//! One line for the notes: what the last frame ran, or why not.
QString wwCellPostEcho();

#endif
