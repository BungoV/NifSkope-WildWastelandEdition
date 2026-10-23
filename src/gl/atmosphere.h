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

QString wwAtmoSummary();

#endif
