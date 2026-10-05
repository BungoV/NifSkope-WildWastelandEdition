#ifndef CELLPOST_H
#define CELLPOST_H

#include <QString>

class EsmWorld;
class Scene;

/*! Lane GRASSMB1 (2026-10-05): THE GAME'S DEPTH OF FIELD AND MOTION BLUR on the cell view's finished frame,
 *  rebuilt from Todd's treat and the game's own pixel shaders (the lane notes hold the trace).
 *
 *  Depth of field (ImageSpaceEffectDepthOfField):
 *   - the parameters are the ImageSpace (IMGS) record's DNAM: strength, distance, range and a flags float whose
 *     integer value packs the mode (bit 0: no far blur, (flags & 3) > 1: no near blur, bit 2: the sky stays sharp)
 *     and the blur radius (flags >> 3, 0 meaning 3). Outdoors the weather's two IMGS around the hour (strength,
 *     distance and range blended on the weather's colour keys, the flags from the nearer key); indoors the cell's
 *     XCIM record;
 *   - the constants as UpdateParams sets them: c0 = (d - (d - r), (r + d) - d, d, 0), c1 = (strength, near on,
 *     far on, sky sharp), c2 = (-1e8, near, far - near, far * near), c3 = 1;
 *   - the composite is the game's pixel shader instruction for instruction (game_dof.frag): the near + far
 *     variant, or the far-only one when (flags & 3) == 2, as the effect picks its technique;
 *   - the blur the composite reads is a separable Gaussian of the record's radius (sigma = radius / 2). The
 *     game's own blur kernel (ImageSpaceEffectBlur) is NOT ported: a stated gap.
 *  Motion blur (ImageSpaceEffectMotionBlur):
 *   - c0 = (fMotionBlur 50 x 0.001 / dt, the max blur 0.01, fThreshold 70, 0), c1 = (1, 1, (W - 1) / W,
 *     (H - 1) / H); the pixel shader is game_mblur.frag, instruction for instruction (4 taps along the vector,
 *     a per-tap weight from the vector-length difference, a radial mask from the frame's far corner);
 *   - the vectors are the camera's reprojection of the frame's depth (the temporal AA's own vector pass), dt is
 *     the camera path's frame time (1 / --path fps); an interactive window measures it between paints.
 *  Both run on the display frame after the temporal AA (the game runs them in its HDR chain): a stated divergence.
 *
 *  Two rows, both in the Cell workspace, both ship OFF, persisted. OFF touches nothing: no target, no pass, so an
 *  OFF frame is the before-lane frame byte for byte. A pick render never takes them.
 *
 *  Pins (a harness without the pin is OFF whatever was saved):
 *   WW_CELL_DOF=0|1                 the depth of field row
 *   WW_CELL_DOF_RED=nococ           the composite ignores the depth (blur = strength everywhere): the gate's red
 *   WW_CELL_MBLUR=0|1               the motion blur row
 *   WW_CELL_MBLUR_RED=novelocity    the vectors read as zero: the gate's red
 *   WW_CELL_POST_DUMP=<dir>         at frame WW_CELL_POST_DUMP_FRAME (default 2): every input and output of both
 *                                   passes, raw, plus post.txt (the constants) -- the numpy rebuild's food */

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

//! After the temporal AA: the depth of field, then the motion blur, the frame back. Inert when both rows are off.
void wwCellPostApply( Scene * scene );

//! One line for the notes: what the last frame ran, or why not.
QString wwCellPostEcho();

#endif
