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

/* lane CLOUD5: row Render > Scene > Cloud Variety: Off / On, ships Off (needs Volume Clouds Medium+). On, the density
 * takes a cloud type per weather (stratus .. cumulonimbus towers, cloudmap.h WwCloudBandInfo::type), a weather noise
 * over tens of km, a curl-displaced, height-dependent erosion: every density program draws its *_v.prog twin
 * (cloud_march_v, cloud_fly_v, cloud_shadowmap_v, cloud_flybake_v); Off draws the old programs unchanged. Pin
 * WW_CLOUDVOL_VARIETY (0/1, off/on); reds in WW_CLOUDVOL_RED: onetype (one cumulus profile, no type swing), nocurl. */
int wwCloudVolVariety();
void wwCloudVolSetVariety( int on );	//!< refused while WW_CLOUDVOL_VARIETY pins it
bool wwCloudVolVarietyPinned();
//! the row on and the volume at Medium or up: readers of the density take the *_v.prog twins
bool wwCloudVolVarietyOn();

/* lane CLOUDBAND1 (bungo 10-30 "the clouds have banding"): Cloud Accumulation, WW_CLOUDVOL_ACCUM (0/1, off/on), ships
 * Off, live. On, the march takes blue noise step offsets (cloud_march_a / cloud_fly_a; the variety twins have them) and
 * cloud_resolve_a blends each fresh march into the history clamped to the fresh 3x3 box (RDR2, Bauer 2019): weight 1, 1/2,
 * 1/3, then 1/4 per refresh cycle (cloudvolmath.h accumAlpha). Off draws cloud_march / cloud_resolve unchanged. Reds in
 * WW_CLOUDVOL_RED: noaccum (weight 1), noclamp, ign (IGN offsets). Gates: tests/spells/cloudband_check.py. */

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
 *  has no map this frame. Needs the GL context current; call after wwCloudMapUpdate.
 *  lane CLOUD2C (2026-10-26): cvBands.windKm is the offset the last draw used (it was 0), so a reader's density at the
 *  drawn clock is the drawn density; a reader that caches what it built must rebuild while the wind moves. */
bool wwCloudVolBindDensity( Scene * scene, int firstUnit );
//! lane CLOUD4: the same with the wind at clock secs (the fly-through grid bakes at its own clock)
bool wwCloudVolBindDensityAt( Scene * scene, int firstUnit, double secs );

/*! lane CLOUD2B, the cell HDR frame only: wwCloudVolSceneDepth grabs the scene's depth after the last draw (before the
 *  tone map); wwCloudVolFront, after it, draws the volume over geometry it is in front of (a ridge behind a cloud).
 *  lane CLOUD2C: wwCloudVolSceneDepth draws them itself, into the linear frame; wwCloudVolFront only for the red display. */
void wwCloudVolSceneDepth( Scene * scene );
void wwCloudVolFront( Scene * scene );
/*! lane CLOUD2C: the frames outside the cell HDR frame (single-NIF lookdev): after the scene drew, its opaque depth
 *  (Scene::grabEffectDepth) ends the march and the clouds draw over the geometry they are in front of. */
void wwCloudVolLookdevFront( Scene * scene );

#endif
