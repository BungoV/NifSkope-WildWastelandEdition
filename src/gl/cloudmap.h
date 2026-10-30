#ifndef WW_CLOUDMAP_H
#define WW_CLOUDMAP_H

/* lane CLOUD1: the per-weather cloud map (notes/atmores1/RESEARCH.md sec. 5.3), the CONSUMER side. The weather's 2D
 * cloud layers (WTHR x0TX, JNAM alpha, NAM1, QNAM/RNAM scroll) are projected from the sky dome onto one plane per
 * band and unioned per band: c = 1 - prod(1 - c_i). Each layer's band comes from the texture census (role by name:
 * Fill / Lower / LowerLight / Horizon -> Low, Upper -> Mid, wispy / cirrus -> High). Weather changes blend the band
 * PARAMETERS (coverage, heights, density, tint), never colours.
 * Row Render > Scene > Cloud Map: Off / Low 256^2 / Medium 512^2 / High 512^2 / Ultra 1024^2, ships Off.
 * Pin WW_CLOUDMAP (tier name or 0..4), red WW_CLOUDMAP_RED = max | colorblend.
 * Readers (CLOUD2 volume, CLOUD3 shadows) use only this header. Changes after the first push are written in
 * notes/cloud1/INTERFACE.md. */

#include <QtGlobal>

enum WwCloudBand { WwCloudLow = 0, WwCloudMid = 1, WwCloudHigh = 2, WwCloudBandCount = 3 };

//! the row: 0 Off / 1 Low / 2 Medium / 3 High / 4 Ultra
int wwCloudMapTier();
void wwCloudMapSetTier( int tier );	//!< refused while WW_CLOUDMAP pins it
bool wwCloudMapPinned();
//! texels per side of tier t (0 for Off): 256 / 512 / 512 / 1024
int wwCloudMapSizeFor( int tier );

struct WwCloudBandInfo
{
	float coverage = 0.0f;	//!< 0..1, the band's dome coverage (the census rung: layer alpha x texture mean alpha, unioned)
	float baseZ = 0.0f;	//!< world z of the band's base, game units (the eye's z at build + the type's base height)
	float topZ = 0.0f;	//!< world z of the band's top, game units
	float density = 0.0f;	//!< relative optical thickness, 1 = a plain cumulus; 0 = an empty band
	float sheet = 0.0f;	//!< 0..1, the share of the band's coverage from sheet (stratus, Fill) layers
	float tint[3] = { 1, 1, 1 };	//!< linear albedo tint, white unless the weather's clouds are deliberately coloured
	int layers = 0;	//!< how many weather layers fed the band this frame
	/* lane CLOUD5 (Cloud Variety): the Low band's cloud type 0..1 (stratus .. cumulonimbus) and the weather noise's swing
	 * of it (cloudmath.h lowTypeOf); depthZ = the band's own depth, game units. With Cloud Variety on, topZ of the Low
	 * band is raised to the tallest type's top (baseZ + typeDepthKm(type + spread)); off, topZ = baseZ + depthZ. */
	float type = 0.0f;
	float typeSpread = 0.0f;
	float depthZ = 0.0f;
};

/*! The map: GL_TEXTURE_2D, RGBA8, linear filtered, clamp to edge, of the context current at the build.
 *  R = Low band coverage, G = Mid band coverage, B = the Low band's sheet (stratus) coverage, A = High band coverage.
 *  Texel (u, v) covers world x = centerX + (2u - 1) halfExtent, y = centerY + (2v - 1) halfExtent (u east, v north),
 *  each band's channel at that band's own height (the dome direction toward (x, y, baseZ) from the eye).
 *  Outside the square: the band's coverage scalar. */
struct WwCloudMap
{
	bool valid = false;	//!< false: no map (row Off, no weather, no context); readers draw no clouds
	bool stub = false;	//!< true: the plain 2D-layer union, every layer in Low, uniform (size 1); no census, no shapes
	unsigned tex = 0;	//!< GL texture name; 0 when !valid
	int size = 0;	//!< texels per side (1 for the stub)
	float centerX = 0.0f, centerY = 0.0f;	//!< world, game units: the eye at build (the vanilla sky follows the camera)
	float halfExtent = 0.0f;	//!< game units
	WwCloudBandInfo band[WwCloudBandCount];
	float transition = 1.0f;	//!< 0..1 from the previous weather's parameters to the current (1 = settled)
	quint64 generation = 0;	//!< bumps on every rebuild (a reader's cache key)
	bool variety = false;	//!< lane CLOUD5: built for Cloud Variety (the Low top raised for towers)
};

//! the latest map; call after wwCloudMapUpdate in the same frame. Never null.
const WwCloudMap & wwCloudMap();

class Scene;
/*! Builds the map for this frame if its inputs moved (weather, hour, scroll, tier, camera); idempotent within a frame,
 *  so the sky pass (CLOUD1) and a reader (CLOUD2) may both call it. Needs the GL context current. */
const WwCloudMap & wwCloudMapUpdate( Scene * scene );

#endif
