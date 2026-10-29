#ifndef WW_AIRMSWEIGHT_H
#define WW_AIRMSWEIGHT_H

/* lane GLAREBLOW1: the cell air's multiple-scattering octave weight, the CPU twin of res/shaders/cell_volair.frag
 * airMsEsc / airMsA (pure header: tests/glareblow1_test.cpp compiles it with g++ alone).
 *
 * The octaves (GAPLOOK1, Wrenninge et al. 2013 "Oz") let octave i scatter a^i of the sun's light. Oz's a = 1/2 is a thick
 * cloud's: there half of the scattered light meets the medium again. The clear air near the ground is thin, so the light
 * scattered once mostly leaves: at Concord-like air (vertical optical depth ~0.15) the octaves gave 1.2-1.6x the multiple
 * scattering a Monte Carlo of the same medium and phase finds (notes_cloud/GLAREBLOW1/RESULTS.md, table A/C), the share
 * that took the sky near the sun over 1.0.
 *
 * Now a = min(1/2, f), f = the chance that light scattered isotropically at height z meets the air again: half the
 * sphere looks up through the column to the air's top, half down to the ground, each 1 - E2(tau) (E2 the exponential
 * integral: the mean over mu in (0, 1] of exp(-tau / mu), 4-point Gauss-Legendre here), and what reaches the ground comes
 * back at albedo kWwAirMsGround. Thin air: f -> 0.1 (the ground's share), the octaves fade; thick fog: f -> 1/2 capped,
 * the octaves as before. Red WW_CELL_GLAREBLOW_RED=off: a = 1/2 (the old law, bit for bit: the same float ops). */

#include <algorithm>
#include <cmath>

static const float kWwAirMsGround = 0.2f;	//!< the ground's albedo for the light that escapes downward

//! E2(tau) = int_0^1 exp(-tau / mu) dmu, 4-point Gauss-Legendre (the GLSL's airMsEsc, the same nodes and order)
inline float wwAirMsEsc( float tau )
{
	return 0.1739274226f * std::exp( -tau / 0.0694318442f ) + 0.3260725774f * std::exp( -tau / 0.3300094782f )
		+ 0.3260725774f * std::exp( -tau / 0.6699905218f ) + 0.1739274226f * std::exp( -tau / 0.9305681558f );
}

//! the re-scatter chance at a point with optical depth tauUp to the air's top and tauDown to the ground
inline float wwAirMsRescatter( float tauUp, float tauDown )
{
	const float eu = wwAirMsEsc( tauUp ), ed = wwAirMsEsc( tauDown );
	return 0.5f * ( 1.0f - eu ) + 0.5f * ( 1.0f - ed ) + 0.5f * ed * kWwAirMsGround;
}

//! the octaves' a: a0 (volMs.x, 1/2) when `medium` is false (the red), else min(a0, the re-scatter chance)
inline float wwAirMsA( float a0, bool medium, float tauUp, float tauDown )
{
	return medium ? std::min( a0, wwAirMsRescatter( tauUp, tauDown ) ) : a0;
}

#endif
