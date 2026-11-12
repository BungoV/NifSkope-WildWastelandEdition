#ifndef WW_FEATURES_EXPOSURE_PHYSUNITS_H
#define WW_FEATURES_EXPOSURE_PHYSUNITS_H

/* lane PHYSUNITS1 (bungo 11-07 "We need real brightness ASAP"): real photometric units. A scene value is cd/m2 (a
 * light's colour C = E / pi, its illuminance in lux over pi). The sun and moon come from the top of the atmosphere through
 * the sky's own air, lamps from lumens, emissives from cd/m2, and the frame is exposed by a physical camera (EV100).
 * Red ^WW_PHYSUNITS_RED=1: the old scale (the 45-degree sun = the weather's Day sunlight). Sources: RESEARCH.md of the lane. */

class QString;

namespace wwphys {
constexpr float kSunToaLux = 133800.0f;	//!< Darula, Kittler & Gueymard 2005: the luminous solar constant
constexpr float kMoonToaLux = 0.32f;		//!< the full moon, V -12.73 against the sun's -26.74 (Krisciunas & Schaefer 1991)
constexpr float kNightFloor = 2.2e-4f;		//!< the dark night sky's background, cd/m2 (Leinert et al. 1998)
constexpr float kLumenPerFade = 637.0f;	//!< the .nifx SPEC's anchor: a 637 lm white omni light is fade 1
//! a placed lamp's authored radius is where its light falls to this illuminance (inverse square): Phi = fade 4 pi r^2 E.
//! 5 lux puts an 800 lm bulb at 3.6 m (250 units) and a 15 klm sodium street lamp at 15.5 m (1080 units); RESEARCH.md
constexpr float kLampEdgeLux = 5.0f;
//! the energy-conserving glare's share (Jimenez 2014, Call of Duty: Advanced Warfare: a ~4 % lerp, no threshold)
constexpr float kGlareK = 0.04f;
constexpr float kEmitCdm2 = 500.0f;		//!< one emissive unit in cd/m2: the middle of the lit-sign range (CIE 150, ILP)
//! lane MODULES1 item 5: one emissive unit = the geometric middle of the lamp law (22.9, no glow) and 500 (blown out),
//! 107 cd/m2, inside CIE 150's illuminated-sign limits for dark surroundings (50 E1 .. 400 E2)
constexpr float kEmitMidCdm2 = 107.0f;
constexpr float kDiscCap = 6.0e4f;			//!< the sun disc's cap in cd/m2: the RGBA16F frame's head room (65504)
constexpr float kPi = 3.14159265f;
}

/*! MODULES1 R6-13 (bungo 11-11: physical units are "part of vanilla Fallout 4 lighting, which is the default if
 *  anything else is not toggled on"): the master, the Scene panel's Physical Light Units row. Off = vanilla Fallout 4
 *  lighting (ships), On = real photometric units; live. Pin WW_PHYSUNITS_RED: set and not "0" = Off, "0" = On. */
bool wwPhysOn();
int wwPhysMaster();	//!< the row's value, 0 Off / 1 On (the pin's while pinned)
void wwPhysSetMaster( int on );	//!< refused while WW_PHYSUNITS_RED pins it
bool wwPhysMasterPinned();
/*! R6-13: while the master is off each term is the game's own. WW_PHYSUNITS_VANILLA_RED=<list of curve, eye,
 *  lampscale; or all> keeps a term as it was drawn before the master existed (the Physical tone curve, the partial
 *  eye adaptation, the sunlit interior's lamp scale). Read per call. */
bool wwPhysVanillaRed( const char * term );
//! the cell view's HDR camera drew this frame (celllights.cpp); the sky and the key take real units only while it does
void wwPhysNoteCellFrame();
bool wwPhysSkyOn();
//! Game Light Brightness: one factor on every vanilla light (LIGH) and the emissives, live. Setting WW/GameLightBrightness,
//! pin WW_PHYSUNITS_LIGHTS (read per call). .nifx lights are absolute lumens and never take it. 0.25..4, default 1.
float wwPhysGameLights();
void wwPhysSetGameLights( double g );
bool wwPhysGameLightsPinned();
//! the emissive scale: kEmitMidCdm2 cd/m2 per unit (reds ^WW_PHYSUNITS_EMIT_RED=1 = kEmitCdm2, =lamp = the placed
//! lamps' factor kLampEdgeLux / pi vanillaFlux; pin WW_PHYSUNITS_EMIT) x Game Light Brightness (1 under the physical red)
float wwPhysEmitScale();
/*! A lamp's colour factor (lumens law): the light draws a colour C with the vanilla falloff to radius r (game units),
 *  so its intensity is lum(C) x (r / 69.99)^2 x vanillaFlux x pi cd; the record's is 637 lm per fade / 4 pi. The factor is
 *  their ratio, the same for a .nifx light (its fade comes from the same 637 law): 637 / (4 pi^2 vanillaFlux (r/69.99)^2). */
float wwPhysLampK( float radius );
/*! A placed (LIGH) lamp's colour factor, the radius law (LATE ITEM 11-08, bungo "lights need to get normalized against"
 *  the sun's lux): the record's flux is fade x 4 pi (r/69.99)^2 x kLampEdgeLux lm, so the factor is that over the drawn
 *  flux, kLampEdgeLux / (pi vanillaFlux) for every radius. The 637 lm law gave a 15 m street lamp a bulb's flux.
 *  Pin WW_PHYSUNITS_LAMP_LUX=<lux>; red WW_PHYSUNITS_LAMP_RED=1 = wwPhysLampK (637 lm per fade). */
float wwPhysLighK( float radius );
/*! The physical camera's meter (LATE ITEM 11-08): the log average (Reinhard 2002's key) of the measure's texels between
 *  the 2nd and 98th percentile of luminance, weighted by their cover. A bulb or a sign then cannot stop the camera down
 *  for the street it lights. Red WW_PHYSUNITS_METER_RED=1 (and the red ^WW_PHYSUNITS_RED=1) = the arithmetic mean.
 *  MODULES1 G1: each texel counts at most 3 EV over the cover's median (physcamera.h; red WW_PHYSUNITS_CAMERA_RED=1).
 *  px: rgba texels (a = cover), returns < 0 when the meter is off (the caller keeps its own mean). */
float wwPhysMeter( const float * px, int n, const unsigned char * stencil = nullptr );
/*! One camera writer (RESUME 11-09 b): the frame's measure (cellhdr.cpp isApply) notes each meter it applied; while one
 *  is fresh (under 1 s) the old measure pass (glview.cpp) stands down, so one meter sets the camera instead of two
 *  alternating (hold3's 0.88 / 0.13 cd/m2 swing). Red WW_PHYSUNITS_METER_RED=1: fresh is never true (both write). */
void wwPhysNoteFrameMeter();
bool wwPhysFrameMeterFresh();
/*! Energy-conserving glare (RESUME 11-09 b): the FO4CS bloom under the physical units takes no threshold, its chain's
 *  flat-field gain is divided out, and the composite lerps the frame toward that blur by k (Jimenez 2014, ~4 %), so the
 *  glare moves light instead of adding it. Pin WW_PHYSUNITS_BLOOM_K=<0..1>; < 0 = off: red ^WW_PHYSUNITS_BLOOM_RED=1
 *  (the additive FO4CS bloom, threshold 1) and the red ^WW_PHYSUNITS_RED=1. */
float wwPhysBloomK();
/*! The physical camera: EV100 = log2(La 100 / 12.5) metered on the mean luminance, an adaptation floor of 1 cd/m2
 *  and the imagespace's middle grey (h[8]) as exposure compensation, adapted in time like the eye (the law and its
 *  sources: physcamera.h, MODULES1 G1); exposure = 1 / (1.2 x 2^EV). Red WW_PHYSUNITS_CAMERA_RED=1 (live): the law
 *  before (Krawczyk 2005 key, instant). Pin WW_PHYSUNITS_EV=<EV100> (a manual camera). h may be null. */
float wwPhysExposure( const float * h, float adapted );
/*! MODULES1 item 14: the incident meter's input, the key light (C, cd/m2 = lux / pi), its cos zenith and the sky's
 *  light on an up-facing surface (the DALC's Z+, C); prints the sun's lux on a change */
void wwPhysNoteDaylight( const float sun[3], float mu, const float skyUp[3] );
//! the cell drawn is outdoors (an exterior or a sunlit interior): the incident meter may anchor the camera
void wwPhysNoteOutdoor( bool outdoor );
//! the last metered and applied EV100 (telemetry)
float wwPhysLastEvMetered();
float wwPhysLastEvApplied();
QString wwPhysEcho();

#endif
