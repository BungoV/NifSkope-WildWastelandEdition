#ifndef WW_CLOUDVOL_H
#define WW_CLOUDVOL_H

/* lane CLOUD2: volume clouds after Schneider & Vos 2015 (Horizon Zero Dawn's Nubis), drawn over the physical sky from
 * CLOUD1's map (cloudmap.h). Row Render > Scene > Volume Clouds: Off / Low / Medium / High / Ultra, ships Off.
 * Low keeps the 2D layers, lit by the model; Medium+ march Perlin-Worley density at 1/4 (Ultra 1/2) resolution,
 * 1/16 of the pixels a frame, the rest reprojected. Pin WW_CLOUDVOL (tier name or 0..4), red WW_CLOUDVOL_RED =
 * noreproj | nooctaves | nocal | everypixel (CLOUD2B adds noms | nodepth | notint). The density is ONE GLSL function, cloudDensity(p) in
 * res/shaders/cloud_common.glsl; readers (CLOUD3 shadows, CLOUD4) bind it with wwCloudVolBindDensity.
 * Changes after the first push are written in notes/cloud2/INTERFACE.md. */

#include <QString>
#include <QStringList>

class Scene;

//! the row: 0 Off / 1 Low / 2 Medium / 3 High / 4 Ultra
int wwCloudVolTier();
void wwCloudVolSetTier( int tier );	//!< refused while WW_CLOUDVOL pins it
bool wwCloudVolPinned();

struct WwCloudVolView
{
	float exposure = 1.0f;
	int viewTransform = 0;
	double cloudSeconds = 0.0;	//!< the 2D layers' scroll clock (the wind's offset)
	bool capture = false;	//!< the probes' sky capture: never the volume (its history is the view's)
};

/*! Medium+: marches, resolves and composites the volume over the sky in the bound framebuffer (the caller then skips
 *  the 2D layers). Returns false when the volume did not draw (Off, Low, no physical sky, no cloud map, a shader
 *  refused): the caller draws the 2D layers. One status entry is appended to drew either way when the row is on. */
bool wwCloudVolDraw( Scene * scene, const WwCloudVolView & view, QStringList & drew );

/*! Low: the 2D layers' colour from the model, scene units (displayed = this x the layer texture). False when Low is not
 *  the drawn tier or the sky did not draw this frame: the caller keeps the weather's colour. */
bool wwCloudVolLowColour( float out[3] );

/*! For readers of the density (CLOUD3, CLOUD4): sets cloud_common.glsl's uniforms (cvBands, cvCal) on the CURRENT
 *  program and binds cvShape, cvDetail, cvMap to units firstUnit .. firstUnit + 2. False (nothing bound) when the volume
 *  has no map this frame. Needs the GL context current; call after wwCloudMapUpdate. */
bool wwCloudVolBindDensity( Scene * scene, int firstUnit );

/*! lane CLOUD2B, the cell HDR frame only: wwCloudVolSceneDepth grabs the scene's depth after the last draw (before the
 *  tone map); wwCloudVolFront, after it, draws the volume over geometry it is in front of (a ridge behind a cloud). */
void wwCloudVolSceneDepth( Scene * scene );
void wwCloudVolFront( Scene * scene );

#endif
