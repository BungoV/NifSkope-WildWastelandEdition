#ifndef WW_CLOUDFLY_H
#define WW_CLOUDFLY_H

/* lane CLOUD4: fly-through clouds. A camera at cloud height (free camera, TCL) flies into CLOUD2's volume: the bands sit
 * at metres above sea level (the layer types' heights; a weather's .recx clouds.layers.<n>.base/top, WX1's keys; the pin
 * WW_CLOUDFLY_BAND=<base>,<top> m for the Low band) instead of above the eye, and near a band a box of cells round the
 * camera holds a bound of the density and its distance field (cloudflymath.h), so the march steps finely through the
 * cloud around the camera (the fog inside, the aerial perspective) and jumps the empty cells. After Schneider, "Nubis
 * Cubed", SIGGRAPH 2023 Advances (its numbers are not known here). Row Render > Scene > Fly-through Clouds:
 * Off / Low / Medium / High / Ultra, ships Off; needs Volume Clouds Medium+. Pin WW_CLOUDFLY (tier name or 0..4),
 * red WW_CLOUDFLY_RED = skip | sdfshrink | stale | nobox | eyeheights. */

#include "gl/glcontext.hpp"

#include <QString>

class Scene;
class Renderer;

//! the row: 0 Off / 1 Low / 2 Medium / 3 High / 4 Ultra
int wwCloudFlyTier();
void wwCloudFlySetTier( int tier );	//!< refused while WW_CLOUDFLY pins it
bool wwCloudFlyPinned();

//! the cloud map's bands at metres above sea level (the row on, not the red eyeheights)
bool wwCloudFlyWorldHeights();
/*! the weather's .recx heights per layer (metres above sea level, clamped 600..20000, top above base): out[n] = {base,
 *  top}, {0, 0} = the layer type's height. wkey = the weather's key (a memo, re-read every second). Returns the count. */
int wwCloudFlyLayerHeights( const QString & wkey, float out[32][2] );
//! the pin WW_CLOUDFLY_BAND=<base>,<top> (m above sea level) for the Low band; false when unset
bool wwCloudFlyBandPin( float & baseM, float & topM );

//! before the march (wwCloudVolDraw): places the box over the camera (km) and builds it; secs = the drawn cloud clock
void wwCloudFlyPrepare( Scene * scene, const float camKm[3], double secs );
//! the march takes cloud_fly.prog this frame
bool wwCloudFlyActive( Renderer * r );
//! the fly uniforms on prog, the distance field on unit; Release unbinds it
void wwCloudFlyUniforms( Renderer * r, NifSkopeOpenGLContext::Program * prog, int unit );
void wwCloudFlyRelease( Renderer * r, int unit );
//! the status line ("off", "idle(...)", "<tier>(...)")
QString wwCloudFlySummary();

#endif
