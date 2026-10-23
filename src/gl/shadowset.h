/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SHADOWSET_H
#define WW_SHADOWSET_H

/* lane TERRSHADOW1: the unified sun-shadow set -- one composition over three producers and one sun.
 *
 *   (A) upgraded cascades  the CSM1 cascades (gl/sunshadow.cpp) with a longer reach, per-cascade bias,
 *                          receiver-plane depth bias and a Vogel-disk filter; Off = CSM1 exactly
 *   (B) contact shadows    a per-pixel port of Bend Studio's screen-space shadows (res/shaders/ww_contact.frag,
 *                          Apache-2.0, Sony) over the cell view's shared opaque pass
 *   (C) terrain shadows    a shadow-height map S(x,y) = max_t [ H(p + t d) - t tan(elev) ] swept on the CPU, a
 *                          band of lines a frame, over the terrain shapes drawn (cells' LAND, far LOD terrain)
 *   (D) the sun path       Engine (the sky clock's arc, floored) or Real (Boston's solar path from the game
 *                          day and hour, down to and below the horizon, then a crossfade to the night light)
 *
 * Composition (res/shaders/ww_sunshadow.glsl, ussCompose): visibility = min( mix( 1, cascades, fade ), terrain,
 * contact ). Every producer fails to 1 (lit); min never darkens twice; the terrain takes over where the cascades'
 * distance fade lets go, so there is no seam. With every new row Off the receiver runs the CSM1 code unchanged.
 *
 * Rows (Scene window, all ship Off/Engine, live, persisted): Cascade Quality, Contact Shadows, Terrain Shadows
 * (Off / Low / Medium / High / Ultra), Sun Path (Engine / Real).
 * Pins: WW_SHADOW_CASCADE / WW_SHADOW_CONTACT / WW_SHADOW_TERRAIN = 0..4, WW_SUNPATH = 0|1,
 * WW_SUNPATH_PIN = <elev>,<azimuth> degrees (azimuth from north, clockwise), WW_SHADOW_VIEW = 1 cascades,
 * 2 terrain, 3 contact, 4 composed (raw grey), 5 weights (R cascade fade weight, G terrain inside its map, B contact),
 * WW_SHADOW_TERRAIN_SYNC = 1 (the whole sweep in one frame: shots), WW_SHADOW_DUMP = <absolute path> (the
 * terrain heights and the swept map, for tests/spells/terrshadow1_march.py), WW_SHADOW_ECHO = <absolute path>.
 * Reds (WW_SHADOW_RED=<a,b>): terrnodecay (the sweep drops - t tan), terrflip (the sweep marches away from the
 * sun), terrseam (the terrain only past the cascades' reach: the fade band goes lit), mulcompose (cascades x
 * terrain x contact instead of min), contactflip (the contact ray marches away from the light), novogel. */

#include "gl/glcontext.hpp"

#include <QString>

class Scene;

enum WwShadowTier { WwShadowOff = 0, WwShadowLow = 1, WwShadowMedium = 2, WwShadowHigh = 3, WwShadowUltra = 4 };

int wwShadowCascadeTier();
void wwShadowSetCascadeTier( int tier );
int wwShadowContactTier();
void wwShadowSetContactTier( int tier );
int wwShadowTerrainTier();
void wwShadowSetTerrainTier( int tier );
//! 0 Engine (the sky clock), 1 Real (the solar path)
int wwSunPathMode();
void wwSunPathSetMode( int mode );

//! (D) the real sun, TO the sun, world (x east, y north, z up), unit; returns the elevation in degrees
double wwSunPathReal( double hour, double gameDay, double out[3] );
/*! (D) with Sun Path = Real: replaces the shaded light direction (TO the light) and the disc, crossfading to
 *  `sunDir`'s own night light between 0 and -6 degrees of elevation. False (nothing changed) on Engine. */
bool wwSunPathApply( double hour, double gameDay, float sunDir[3], float disc[3] );
/*! lane EPHEM1: with Sun Path = Real and the row "Real sun and moon" on, the moon TO-vector (unit, world), its
 *  sun-moon elongation (0 new, 180 full) and lit fraction; false (nothing written) otherwise. */
bool wwSkyMoonReal( double hour, double gameDay, double dir[3], double * elongDeg, double * litFraction );

//! (A) the upgraded cascades' preset; false = Off (CSM1's own numbers)
struct WwCascadePreset
{
	double D = 3000.0, split0 = 800.0, split1 = 3000.0, blend = 100.0, lightBack = 15000.0;
	int map = 2048;
	float radius = 3.0f;		// the Vogel filter's radius, texels
	int taps = 32;
	float biasTexels = 1.5f;	// the receiver offset, texels of each cascade (world units along the light)
	float slopeBias = 6.0f, unitBias = 12.0f;	// glPolygonOffset of the caster pass
};
bool wwShadowCascadePreset( WwCascadePreset & p );

//! the frame's terrain sweep step; called from wwSunShadowPass whatever the Cascaded Shadows row says
void wwShadowSetFrame( Scene * scene );
//! (B) the contact pass needs the cell view's shared opaque pass this frame
bool wwContactShadowWantsTargets( Scene * scene );
//! (B) the contact mask, after wwCellAoPass (glview)
void wwContactShadowPass( Scene * scene );
//! any new producer valid this frame: the receiver runs the composition (and the csm programs are swapped in)
bool wwShadowSetActive( Scene * scene );
//! the composition's uniforms on the current program (called from wwSunShadowUniforms)
void wwShadowSetUniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog, bool csmValid, const float csmTexel[3] );
/*! (C) the published terrain shadow-height map, for a reader outside the receivers (a later far-fog grid lane
 *  samples the SAME map): RG16 texture, .r = the lower sun limb's height (lit above), .g = the upper limb's
 *  (shadowed below), both as zLo + v * zSpan, 0 = no shadow height (lit); texel (i, j) covers world
 *  [x0 + i * texel, x0 + (i + 1) * texel) x [y0 + j * texel, ...). The volumetric fog already reads it through
 *  wwShadowSetUniforms. False while Terrain Shadows is Off or no sweep has finished. */
struct WwTerrainShadowMap
{
	unsigned tex = 0;
	int n = 0;
	double x0 = 0, y0 = 0, texel = 0, elevDeg = 0, azDeg = 0;
	float zLo = 0, zSpan = 1;
};
bool wwTerrainShadowMap( WwTerrainShadowMap & out );
/*! lane SHADOWPAR1 (bungo 10-21): the FO4CS-level items, one bit each (the row "FO4CS-Level Shadows" Off / On = all,
 *  ships Off; pin WW_SHADOW_PAR = all | <mask> | <name,name>). Red WW_SHADOW_RED=no<name> takes one item back to the
 *  TERRSHADOW1 path and cost. Names: dist cradius cphase cslope contact objlod near pen lowsun moon tslope dim
 *  temporal fogtail. */
enum WwShadowPar : int
{
	WwParDistance = 1,	// the cascades' reach from the Shadow Distance slider (FO4CS 0..24000, 14000)
	WwParCascRadius = 2,	// world-unit Vogel radius (2.4 / 9 / 36) and per-cascade taps
	WwParCascPhase = 4,	// the Vogel rotation's temporal phase (frame % 61, TAA only)
	WwParCascSlope = 8,	// the caster slope bias clamped at a slope of 4 (glPolygonOffsetClamp)
	WwParContact = 16,	// contact: 384 samples, thickness 0.005, evidence 4, completion / edge fades, rejects
	WwParObjLod = 32,	// far object-LOD shapes cast into the terrain map (radius >= 128, budget 1500)
	WwParNear = 64,		// a 64 u/texel near tier (16 cells) over the far map
	WwParPenumbra = 128,	// the terrain's disc penumbra from the occluder distance (<= 16 texels)
	WwParLowSun = 256,	// a 4 -> 1 degree fade instead of the hard cutoff
	WwParMoon = 512,	// the night light casts terrain shadows (strength 0.15)
	WwParTerrSlope = 1024,	// the terrain bias 12 (1 + slope) / sin(elevation)
	WwParDim = 2048,	// the ambient (cool tint 0.35) and specular dimmed in terrain shadow (0.5 / 0.5)
	WwParTemporal = 4096,	// the swept map blends with the last one (0.5, TAA only)
	WwParFogTail = 8192,	// the air's far tail reads the terrain map (the fog leak behind ridges)
	WwParAll = 16383
};
//! the item is on (its bit set, its red not)
bool wwShadowPar( int bit );
int wwShadowParMask();
void wwShadowSetParMask( int mask );
//! the Shadow Distance slider (world units, 2000..24000; the cascades' reach under WwParDistance)
//! the air tail's ambient share (celllights.cpp, from the weather's DALC and sun; 0.3 until set)
void wwShadowSetAirAmbientShare( float a );
double wwShadowDistance();
void wwShadowSetDistance( double d );
//! fix 10-22: a terrain shadow sweep is owed, running or uploading (the live driver's pictures wait for it)
bool wwTerrainShadowPending();
//! the census line
QString wwShadowSetSummary();
bool wwShadowRed( const char * name );

#endif
