#ifndef WW_ATMOSPHERE_H
#define WW_ATMOSPHERE_H

/* lane ATMO1: the physical sky, the atmosphere's one owner (notes/atmores1/RESEARCH.md sec. 6). Hillaire 2020 LUTs
 * (transmittance, multiple scattering, sky-view) drawn as fragment passes (GL 4.1: no compute), the sky's pixels and
 * the sun disc. Row Scene > Sky > Physical Sky: Off / Low / Medium / High / Ultra, ships Off (Off = the weather dome).
 * Pin WW_ATMO (tier name or 0..4), red WW_ATMO_RED = noozone | noms | nohaze. Offline gates:
 * tests/spells/atmo_model.py (published numbers, Monte Carlo), tests/spells/atmo_lut_check.py (the GPU LUTs).
 * Later lanes read the LUTs and the CPU transmittance below: ATMO2 (air), SKYGI1 (probes), NIGHT1 (moon, stars). */

#include <QString>

class Scene;

int wwAtmoTier();
void wwAtmoSetTier( int tier );	//!< refused while WW_ATMO pins it
bool wwAtmoPinned();

struct WwAtmoInputs
{
	float sunW[3] = { 0, 0, 1 };	//!< world, TO the sun; below the horizon at night
	float moonW[3] = { 0, 0, 1 };
	bool moonUp = false;
	float sunLum = 1.0f;	//!< the weather's Day sunlight luminance (linear x the image space's sunlight scale)
	float moonLum = 0.0f;	//!< the Night sunlight luminance (the engine's night key light is the moon's)
	float fogFar = 0.0f, fogMax = 0.0f, fogMid = 0.0f, fogRange = 10000.0f;	//!< game units; fogFar 0 = no haze
	float camZ = 0.0f;	//!< game units
	bool disc = true;	//!< the sun disc (the Sun row)
	float exposure = 1.0f;
	int viewTransform = 0;
	float tint[17][3] = {};	//!< lane SKYA (ATMOCHECK1): the weather dome's linear color at atmomath.h's tint elevations
	bool tintOk = false;	//!< the table is filled (the dome resolved); else Weather Tint draws the physical color
	bool capture = false;	//!< lane SKYA (SKYGI1): the probes' sky capture; no timing, no exposure read-back, no air
};

//! draws the sky full screen in place of the dome; false when it cannot (the caller falls back to the dome)
bool wwAtmoDrawSky( Scene * scene, const WwAtmoInputs & in, QString * line );

//! the consumers' side (valid after a drawn frame, 0 otherwise): RGBA16F textures of the current context
unsigned wwAtmoTransmittanceTex();
unsigned wwAtmoMultiScatterTex();
unsigned wwAtmoSkyViewTex();

//! the CPU transmittance (the LUT's math) from height km along cos zenith, haze from the last drawn frame
void wwAtmoTransmittance( float heightKm, float cosZenith, float out[3] );

/* lane SKYA (ATMO2): the aerial perspective (Hillaire 2020 sec. 5.4), a froxel volume over the view built with the
 * sky's own medium and LUTs, read by every surface through lookdev_fog.glsl's wwFog (near geometry, the far LOD, the
 * far cards: one linear frame). Row Render > Scene > Aerial Perspective: Off / On, ships Off, needs Physical Sky on.
 * Pin WW_ATMO_AP (0/1), red WW_ATMO_AP_RED = nosunt | double. With the cell fog on, the fog keeps only the share
 * the air has not already taken (combined transmittance = min of the two: no haze counted twice). */
int wwAtmoAp();
void wwAtmoSetAp( int on );	//!< refused while WW_ATMO_AP pins it
bool wwAtmoApPinned();
void wwAtmoApFrame();	//!< the frame's start: last frame's volume is no longer valid
bool wwAtmoApActive();	//!< this frame drew a volume (Renderer then picks the fog program with the Fog row off)
void wwAtmoApUniforms( Scene * scene );	//!< the current program's atAp* uniforms and its sampler (unit AtmoAp)

/* lane SKYA (ATMOSUN1): the key light and the ambient coloured by the air (Hillaire 2020 sec. 4: the sun's light at
 * the ground = its light at the top of the air x the transmittance toward it). Row Render > Scene > Sunlight From Sky:
 * Off / Tint (the weather's brightness, the air's colour) / Full (the air's colour and brightness); ships Off, needs
 * Physical Sky on. Pin WW_ATMO_SUN (off|tint|full or 0..2), red WW_ATMO_SUN_RED = flat (a low sun no warmer). */
int wwAtmoSun();	//!< the row: 0 Off / 1 Tint / 2 Full
void wwAtmoSetSun( int mode );	//!< refused while WW_ATMO_SUN pins it
bool wwAtmoSunPinned();
int wwAtmoSunKey();	//!< 0 when it changes nothing; else a memo key of the mode and red (lookdevstage's light cache)
//! the light toward cos zenith muLight (the sun, or the moon by night); fog = the weather fog's Far, Max, Height Mid,
//! Height Range; keyLum45 = the weather's Day (Night) key luminance. False (nothing changed) when the row is Off.
bool wwAtmoKeyLight( float muLight, const float fog[4], float keyLum45, float sun[3], float amb[3], float dalc[6][3] );

/* lane SKYA (NIGHT1): the moon's phase in its light (Krisciunas & Schaefer 1991: a quarter moon ~0.09 of the full),
 * the night key light and the sky's moonlight both; the stars and the moon disc dimmed and reddened toward the
 * horizon by the air (T(mu) / T(zenith)). Row Render > Scene > Moon Phase and Stars: Off / On, ships Off, needs
 * Physical Sky on. Pin WW_ATMO_NIGHT (0/1), red WW_ATMO_NIGHT_RED = nophase | noext. The phase is the engine's
 * (esmweather wwMoonPhase on the game day). */
int wwAtmoNight();
void wwAtmoSetNight( int on );	//!< refused while WW_ATMO_NIGHT pins it
bool wwAtmoNightPinned();
bool wwAtmoNightOn();	//!< the row on and the Physical Sky on
int wwAtmoNightKey( int phase );	//!< 0 when off; else a memo key of the phase and red (lookdevstage's light cache)
float wwAtmoMoonFactor( int phase );	//!< the moon's light against the full moon's (1 when off)
void wwAtmoStarExtinction( float mu, float out[3] );	//!< T(mu) / T(1) from the drawn sky's haze (1 when off)

/* lane SKYA (EXPO1): the eye adaptation counts the sky. The imagespace measure averages the cell-lit pixels only;
 * with this row on, the rest of the frame counts at the sky's mean over the upper hemisphere (solid-angle weighted,
 * from the sky-view tables read back a frame late), in the sky pass's scene units. Row Render > Scene > Exposure From
 * Sky: Off / On, ships Off, needs Physical Sky on. Pin WW_ATMO_EXPO (0/1), red WW_ATMO_EXPO_RED = skyzero | zenithw. */
int wwAtmoExpo();
void wwAtmoSetExpo( int on );	//!< refused while WW_ATMO_EXPO pins it
bool wwAtmoExpoPinned();
//! the adapted mean: sceneMean over `counted` of `total` pixels, the rest at the sky's mean (sceneMean when off)
float wwAtmoAdapted( float sceneMean, int counted, int total );
/* lane SKY2B (EXPO2): the same from the measure's quarter mip (rgb = light, a = the lit share): the sky's share of each
 * texel is looked up along that texel's own ray, so only the sky in view counts. Red WW_ATMO_EXPO_RED=hemi = EXPO1. */
float wwAtmoAdaptedInView( Scene * scene, float sceneMean, const float * px, int qw, int qh );

/* lane SKYA (ATMOCHECK1): the Sky Color row. Physical = the model's color (today's); Weather Tint keeps the physical
 * sky's luminance (the light, the exposure and the bloom see the same brightness) and takes the weather's own sky
 * colors at each elevation (the dome's Horizon / Sky-Lower / Sky-Upper blend). Row Render > Scene > Sky Color, ships
 * Physical, needs Physical Sky on. Pin WW_ATMO_TINT (physical|weather or 0/1), red WW_ATMO_TINT_RED = flat. */
int wwAtmoTint();
void wwAtmoSetTint( int mode );	//!< refused while WW_ATMO_TINT pins it
bool wwAtmoTintPinned();
bool wwAtmoTintRedFlat();	//!< the red: the horizon's color at every elevation (the gradient lost)
//! SKY2B (SKYTINT2) red WW_ATMO_TINT_RED=physical: the aerial perspective and the ambient keep the physical color
bool wwAtmoTintRedPhysical();

/* lane SKYA (SKYGI1): the probes' sky light from the physical sky. The probe bake's sky term (src/probesky.h, lane
 * SKYPIC1) integrates a 6-face capture of the drawn sky; with this row on and Physical Sky on, the capture draws the
 * physical sky (its sky-view LUTs, no sun disc: the sun is the bake's own term) in place of the weather dome. Row
 * Render > Scene > GI Sky From Physical Sky: Off / On, ships Off; Off (or Physical Sky Off) = today's capture, key
 * and bake byte for byte. Pin WW_ATMO_GI (0/1), red WW_ATMO_GI_RED = dome (the row on, the dome still captured). */
int wwAtmoGi();
void wwAtmoSetGi( int on );	//!< refused while WW_ATMO_GI pins it
bool wwAtmoGiPinned();
bool wwAtmoGiActive();	//!< the row on and Physical Sky on: the capture takes the physical sky
bool wwAtmoGiRedDome();

QString wwAtmoSummary();

#endif
