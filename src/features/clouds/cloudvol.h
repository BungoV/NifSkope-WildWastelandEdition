#ifndef WW_CLOUDVOL_H
#define WW_CLOUDVOL_H

/* lane CLOUD2: volume clouds after Schneider & Vos 2015 (Horizon Zero Dawn's Nubis), drawn over the physical sky from
 * CLOUD1's map (cloudmap.h). Row Render > Scene > Volume Clouds: Off / Low / Medium / High / Ultra, ships Off.
 * Low keeps the 2D layers, lit by the model; Medium+ march Perlin-Worley density at 1/4 (Ultra 1/2) resolution,
 * 1/16 of the pixels a frame, the rest reprojected. Pin WW_CLOUDVOL (tier name or 0..4), red WW_CLOUDVOL_RED =
 * noreproj | nooctaves | nocal | everypixel (CLOUD2B adds noms | nodepth | notint). The density is ONE GLSL function, cloudDensity(p) in
 * res/shaders/features/clouds/cloud_common.glsl; readers (CLOUD3 shadows, CLOUD4) bind it with wwCloudVolBindDensity.
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
 * WW_CLOUDVOL_VARIETY (0/1, off/on); red in WW_CLOUDVOL_RED: nocurl. */
int wwCloudVolVariety();
void wwCloudVolSetVariety( int on );	//!< refused while WW_CLOUDVOL_VARIETY pins it
bool wwCloudVolVarietyPinned();
//! the row on and the volume at Medium or up: readers of the density take the *_v.prog twins
bool wwCloudVolVarietyOn();
/*! lane PERFFIX3: a coverage table (Low/Mid band, or the Variety set) is being built on a worker (cloudcaljobs.h) and the
 *  clouds draw the previous one meanwhile; a picture waits until this is false (nifskope_ui.cpp settle). Red
 *  WW_CLOUDVOL_RED=gltables: built on the GL thread as before (never pending). */
bool wwCloudVolTablesPending();
/*! lane CLOUD6d: one cloud profile set per weather (cloudmath.h lowTypeOf oneSet: the weather's type everywhere, no
 *  towers beside puffs), WW_CLOUDVOL_PROFILESET, ON BY DEFAULT, live; =0 / off or WW_CLOUDVOL_RED=typemix = CLOUD5's
 *  type swing (the red). Gate tests/features/clouds/cloudprofile_check.py. */
bool wwCloudVolProfileSet();
/*! lane CLOUDTYPE2: the cloud type's storm signal = the weather's thunder sounds (cloudmath.h lowTypeOf thunderSounds),
 *  WW_CLOUDVOL_TYPERULE, ON BY DEFAULT, live; =0 / off or WW_CLOUDVOL_RED=thunderbyte = CLOUD5's rule (the DATA thunder
 *  byte, Snow first: the red). Gate tests/features/clouds/cloudtype/run.sh. */
bool wwCloudVolTypeRule();
/*! lane CLOUDTYPE3: a Rainy-flag weather with no precipitation form (MNAM 0) and no thunder is typed Cloudy (cloudmath.h
 *  lowTypeOf precip), with the type rule on; WW_CLOUDVOL_RED=rainflag = CLOUDTYPE2 (typed like rain). */
bool wwCloudVolPrecipRule();

/* lane CLOUDBAND1 (bungo 10-30 "the clouds have banding"): Cloud Accumulation, WW_CLOUDVOL_ACCUM (0/1, off/on), live;
 * ON BY DEFAULT since lane CLOUDBAND2 (=0 / off is the old path, the red; so is WW_CLOUDVOL_AMBDARK=0). On, the march takes blue noise step offsets (cloud_march_a / cloud_fly_a; the variety twins have them) and
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

/*! lane CLOUDLIT1: the 2D layers lit from our sky as the volume's thin sheet (lookdev_clouds_ps.hlsl ldKey .. ldTint), the
 *  volume's own terms this frame: TO the key light and its radiance (cvKeyW, cvKeyK), the sky light at the clouds (cvAmb),
 *  the droplet phase (cvPhasePrm, its peak capped as the volume's cvPrm for the history the view p11 x viewH would get),
 *  the octave count (cvOctaves) and the Low band's tint. False (the layers keep colLin) when the sky did not draw this
 *  frame, no key light is up, or red WW_CLOUDVOL_RED=vanillalit. */
struct WwCloudLayerLight
{
	float keyW[3] = { 0.0f, 0.0f, 1.0f }, keyK = 0.0f, amb[3] = {}, phase[4] = {}, tint[3] = { 1.0f, 1.0f, 1.0f };
	int octaves = 4;
};
bool wwCloudLayerLight( float p11, int viewH, WwCloudLayerLight & out );

/*! lane SKYLIGHT1: the cloud map's Low + Mid deck over a ground point (clouddeckmath.h, the volume's own density), sun
 *  at mu0: out = cover, the sun's diffuse transmission, the clear sky's transmission, the mean column tau. False = no
 *  deck this frame. Memoised (tables per band shape, columns per coverage and mu0). GL thread. */
bool wwCloudDeckThrough( float mu0, float out[4] );

/*! SKYLIGHT1 round 2: the deck over six sky axes (clouddeckmath.h overAxes; up = the index a face looking up reads),
 *  every reader of the sky's ambient through this one call: the stage's DALC (vanilla and physical) and the GI's miss sky.
 *  pi: the axes are a record DALC (read over pi). Prints "sky deck: <who> ..." on change. Returns up after / before,
 *  0 = no deck (none this frame, the sun down, or the red WW_SKY_CLOUDDECK_RED=1). GL thread. */
float wwCloudDeckOnAxes( float ax[6][3], int up, const float sunHorizC[3], float mu0, float rhoG, bool pi, const char * who );

/*! lane LIGHTNING2 (LIGHTNINGSKY1): the storm's flash for the rest of the frame, the same strike, level and colour as the
 *  volume's (one schedule on the clouds' clock, gl/cloudlightning.h). Switch WW_LIGHTNING_SKY (live, on), red
 *  WW_LIGHTNING_SKY_RED = the volume clouds alone (CLOUD6f).
 *  wwLightningFrameBegin: the lookdev backdrop's first line (no sky pass this frame = no ground flash).
 *  wwLightningSky: the 2D layers' field (lightning_field.glsl) for the camera of scene; unitK = the layers' scene units
 *  x pi of a white sunlit surface (Volume Clouds Low: the atmosphere's sunK; the weather's own colours: pi x Sky Scale).
 *  False = no flash for the layers (no storm, between flashes, WW_CLOUD_LIGHTNING off, the sky switch off / red).
 *  wwLightningGround: this frame's DALC add (linear, [travel axis X+ X- Y+ Y- Z+ Z-][rgb], 1 = a white surface under the
 *  Day sun) for the exterior cell programs and the air; false (all 0) = none. */
struct WwLightningSky
{
	bool on = false;
	float src[4] = {}, prm[4] = {}, col[3] = {};	// as cloud_composite's cvLtSrc / cvLtPrm / cvLtCol
	float deckKm = 0.0f;	// the height a layer texel is lit at (the Low base, one extinction length in)
	float camKm[3] = {};
	float level = 0.0f;
};
void wwLightningFrameBegin();
bool wwLightningSky( Scene * scene, double secs, float unitK, WwLightningSky & out );
bool wwLightningGround( float add[6][3] );

//! lane WXBAND: the Low band's base km this frame (the cloud map's, else the default slab), the 2D layers' deck
float wwCloudLayerDeckKm();

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
