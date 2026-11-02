#ifndef CELLPOST_H
#define CELLPOST_H

#include <QString>

class EsmWorld;
class Scene;

/*! The game's depth of field and motion blur on the cell view's tone-mapped frame, before the temporal AA (the
 *  game's order); parameters from the IMGS DNAM (weather or cell XCIM), shaders game_dof*.frag / game_mblur.frag.
 *  Both rows ship OFF and then touch nothing. WW_CELL_DOF=0|1, WW_CELL_MBLUR=0|1 pin them;
 *  WW_CELL_DOF_RED, WW_CELL_MBLUR_RED and WW_CELL_POST_DUMP=<dir> (frame WW_CELL_POST_DUMP_FRAME) are debug aids.
 *  lane POSTFX2: with both on, the composite runs inside the blur's pass (game_dofmblur.frag); WW_CELL_POSTFUSE_RED=1
 *  keeps the two passes. */

//! The rows' state (pinned, saved, or OFF in a harness run).
bool wwCellDofOn();
void wwCellDofSetOn( bool on );
bool wwCellMotionBlurOn();
void wwCellMotionBlurSetOn( bool on );
/*! lane POSTFX1: per-object motion blur (bungo: objects blur, the camera never does). The things that moved on their
 *  own (TAAINPUTS1's object vectors, the camera taken out) blur over half the frame (a 180 degree shutter); a still
 *  world under a turning camera stays the input frame bit for bit. Shaders cell_objblur_vec / _tile / cell_objblur
 *  (tile max, neighbour max, a depth-compared gather). WW_CELL_OBJBLUR=0|1 pins it (default OFF, waits on bungo's
 *  eye); WW_CELL_OBJBLUR_RED=camera keeps the camera's vectors (the old full-frame blur, the gate's red). Runs
 *  after the game's depth of field / motion blur, before the temporal AA. */
bool wwCellObjectBlurOn();
void wwCellObjectBlurSetOn( bool on );
/*! lane POSTFX2: the depth of field and motion blur on the linear frame before the tone map (bright lights keep their
 *  energy in the blur) instead of the tone-mapped one. WW_CELL_POST_LINEAR=0|1 pins it (key WW/CellPostLinear, default
 *  OFF: it changes the picture). The object blur and the temporal AA stay on the tone-mapped frame. */
bool wwCellPostLinearOn();
void wwCellPostLinearSetOn( bool on );
/*! lane DOFLENS1: the depth of field through a thin lens instead of the game's Gaussian: the circle of confusion from
 *  the thin-lens law (focal length from the field of view, the record's focus distance, an aperture matched to the
 *  record's blur), a near / far gather (bokeh disks; a sharp subject is never blurred by what is behind it). Runs where
 *  the game's would; WW_CELL_DOF_STATE=menu runs the Pip-Boy / dialogue lens (f/2.8, autofocus, far only).
 *  WW_CELL_DOF_LENS=0|1 pins it (key WW/CellDofLens, default OFF); WW_CELL_DOF_LENS_RED=1 keeps the game's passes. */
bool wwCellDofLensOn();
void wwCellDofLensSetOn( bool on );

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
//! lane POSTFX2: in the linear frame, just before the tone map: the depth of field and motion blur when the linear row
//! is on (wwCellPostApply then skips them that frame); inert otherwise.
void wwCellPostApplyLinear( Scene * scene );

//! One line for the notes: what the last frame ran, or why not.
QString wwCellPostEcho();

#endif
