#ifndef CELLAIRZONE_H
#define CELLAIRZONE_H

/* lane AIRZONE1 (bungo 10-29): the hand-placed volumes in the cell view. The files are src/airzone.h, the law and its
 * GPU packing src/airzonemath.h, the GLSL res/shaders/cell_airzone.glsl.
 *
 * The row "Air zones" (Show > Air, pin WW_CELL_AIRZONE: "0" off, else on; ships Off). On, the loader injects
 * `#define WW_CELL_AIRZONE` into the sources that mention it (src/gl/glcontext.cpp), so the air pass (cell_volair.frag)
 * and the lit particles (cell_volpart.glsl) read the zones; Off, those sources differ from main by preprocessor lines
 * only (the identical machine code). The fog and particle blocks are live (the files re-read within half a second, the
 * panel's unsaved copy at once); the gi blocks apply when the GI grid is built (a reopen). Red WW_CELL_AIRZONE_RED:
 * novol (the files ignored), hardedge (the weight 0 or 1, no soft edge). */

#include <QString>

class Scene;
class Transform;

//! the Air zones row (or its pin)
bool wwAirZonesOn();
//! WW_CELL_AIRZONE is set (the row greys)
bool wwAirZonesPinned();
//! the row's tick (a shader reload follows: the read is a #define)
void wwAirZonesSetOn( bool on );
//! the loader's `#define WW_CELL_AIRZONE` (src/gl/glcontext.cpp Shader::load)
bool wwAirZoneShaderDefine();
//! the current program's azCount, azRed and azZ (a no-op in a program without them: the define off, or not a reader)
void wwAirZoneUniforms( Scene * scene );
//! the volumes as wireframes over the frame (src/glview.cpp, beside the PRTP views): while the panel shows them, or
//! WW_CELL_AIRZONE_VIEW=1; fog blue, particles amber, gi green, the selected one white and never hidden by a wall
void wwAirZoneViewDraw( Scene * scene, const Transform & viewTrans );
//! the panel: show the volumes, and which one is selected (cell index, volume index; -1 none)
void wwAirZoneViewSet( bool on, int cell, int volume );
//! the last frame's pick, one line ("air zones frame: ...")
QString wwAirZoneFrameLine();

#endif // CELLAIRZONE_H
