#ifndef WW_CLOUDSHADOW_H
#define WW_CLOUDSHADOW_H

/* lane CLOUD3: the clouds' shadows on the ground, in the fog's air and in the probe GI's sun. One Beer shadow map
 * (Hillaire 2016 sec. 5.5) along the sun from the volume's density (cloud_common.glsl cloudDensity, bound with
 * wwCloudVolBindDensity) or, at Low or without the volume, from CLOUD1's map coverage. Receivers: ww_sunshadow.glsl
 * ussCloudT, multiplied into wwSunShadow (surfaces), the air froxels' sun term (cell_volair airCloudShadow) and, at
 * Ultra, the far air tail; the probe GI's sun is scaled by the clouds over the eye (probesky.cpp).
 * Row Render > Scene > Cloud Shadows: Off / Low / Medium / High / Ultra, ships Off. Needs the Cloud Map row on.
 * Pin WW_CLOUDSHADOW (tier name or 0..4), red WW_CLOUDSHADOW_RED = vertical | noinject | everyframe | nogi. */

#include "gl/glcontext.hpp"

#include <QString>

class Scene;

//! the row: 0 Off / 1 Low / 2 Medium / 3 High / 4 Ultra
int wwCloudShadowTier();
void wwCloudShadowSetTier( int tier );	//!< refused while WW_CLOUDSHADOW pins it
bool wwCloudShadowPinned();

//! builds rows of the map (frame start, after the sun shadow pass): size/16 a call, all of them when the fit moves
void wwCloudShadowPass( Scene * scene );
//! a map the receivers may read this frame
bool wwCloudShadowValid();
//! the receivers' uniforms on prog (ww_sunshadow.glsl ussCloud*); volume = the fog's froxel pass, airAmb = the air's
//! ambient share (shadowset.cpp; the Ultra air taps darken only the rest)
void wwCloudShadowUniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog, bool volume, float airAmb );
//! the probe GI's sun scale: the clouds' mean transmittance over the eye, in 1/32 steps; exactly 1 when off
float wwCloudShadowGiFactor();
//! the status line ("off", "refused(...)", "on ...")
QString wwCloudShadowSummary();

#endif
