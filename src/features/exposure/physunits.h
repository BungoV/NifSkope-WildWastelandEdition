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
constexpr float kEmitCdm2 = 500.0f;		//!< one emissive unit in cd/m2: the middle of the lit-sign range (CIE 150, ILP)
constexpr float kDiscCap = 6.0e4f;			//!< the sun disc's cap in cd/m2: the RGBA16F frame's head room (65504)
constexpr float kPi = 3.14159265f;
}

//! the physical scale (false under the red ^WW_PHYSUNITS_RED=1)
bool wwPhysOn();
//! the cell view's HDR camera drew this frame (celllights.cpp); the sky and the key take real units only while it does
void wwPhysNoteCellFrame();
bool wwPhysSkyOn();
//! Game Light Brightness: one factor on every vanilla light (LIGH) and the emissives, live. Setting WW/GameLightBrightness,
//! pin WW_PHYSUNITS_LIGHTS (read per call). .nifx lights are absolute lumens and never take it. 0.25..4, default 1.
float wwPhysGameLights();
void wwPhysSetGameLights( double g );
bool wwPhysGameLightsPinned();
//! the emissive scale: kEmitCdm2 (pin WW_PHYSUNITS_EMIT) x Game Light Brightness (1 under the red)
float wwPhysEmitScale();
/*! A lamp's colour factor (lumens law): the light draws a colour C with the vanilla falloff to radius r (game units),
 *  so its intensity is lum(C) x (r / 69.99)^2 x vanillaFlux x pi cd; the record's is 637 lm per fade / 4 pi. The factor is
 *  their ratio, the same for a .nifx light (its fade comes from the same 637 law): 637 / (4 pi^2 vanillaFlux (r/69.99)^2). */
float wwPhysLampK( float radius );
/*! The physical camera: EV100 = log2(La 100 / 12.5) metered on the mean luminance, the eye's low-light key
 *  (Krawczyk 2005) and the imagespace's middle grey (h[8]) as exposure compensation; exposure = 1 / (1.2 x 2^EV).
 *  Pin WW_PHYSUNITS_EV=<EV100> (a manual camera). h may be null (no compensation from the imagespace). */
float wwPhysExposure( const float * h, float adapted );
//! the last metered and applied EV100 (telemetry)
float wwPhysLastEvMetered();
float wwPhysLastEvApplied();
QString wwPhysEcho();

#endif
