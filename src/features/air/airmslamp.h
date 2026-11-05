#pragma once
/* lane AIRMS1 (AIR-MS-LAMPS): the placed lights' multiple scattering in the cell air, the CPU twin of cell_volair.frag
 * airLampPh. The single scattering of a lamp at a froxel point is e T p(c) (T = exp(-od), od the optical depth to the
 * lamp); the orders >= 2 of the same medium (albedo 1, no ground) add e M(c, od), fitted to a backward Monte Carlo of a
 * point lamp (notes/airms1/tools/lamp_ms_mc.py, mc_fit.py; fixture tests/features/air/airmslamp/airmslamp_mc.tsv):
 *   M = (K0 + K1 gbar) od P2(c) exp(-a od),  P2 = the phase with each HG lobe's g x s (the order-2 lobe, broadened)
 * K0 1.9, K1 -0.75, a 0.4, s 0.8; gbar the phase's L1 g (volGi.w). Over 495 MC rows (7 phases, od 0.01..2.4, into/side/
 * away the lamp) single + rest is within rms log 0.14 of the MC (single alone 0.98). Off (or red): M = 0. */
#include <cmath>

namespace airmslamp
{
constexpr float kK0 = 1.9f, kK1 = -0.75f, kA = 0.4f, kS = 0.8f;
constexpr float kPi = 3.14159265f;

struct Lobes
{
	float wAir, wFwd, gFwd, wBack, gBack;	// luminance weights (sum 1) and the HG g of the forward and back lobes
};

inline float hg( float c, float g )
{
	float d = 1.0f + g * g - 2.0f * g * c;
	return ( 1.0f - g * g ) / ( 4.0f * kPi * d * std::sqrt( d ) );
}

// pi x the phase with the HG lobes' g x s (s 1: volPhasePi)
inline float phasePi( const Lobes & p, float c, float s )
{
	return kPi * ( p.wAir * ( 3.0f / ( 16.0f * kPi ) ) * ( 1.0f + c * c ) + p.wFwd * hg( c, p.gFwd * s ) + p.wBack * hg( c, p.gBack * s ) );
}

inline float gbar( const Lobes & p )
{
	return p.wFwd * p.gFwd + p.wBack * p.gBack;
}

// what the shader multiplies e by: T pi p(c) + (on ? pi M : 0)
inline float lampPh( const Lobes & p, float c, float od, bool on )
{
	float r = std::exp( -od ) * phasePi( p, c, 1.0f );
#ifndef AIRMSLAMP_RED
	if ( on ) {
		float k = std::fmax( kK0 + kK1 * gbar( p ), 0.0f );
		r += k * od * std::exp( -kA * od ) * phasePi( p, c, kS );
	}
#else
	(void)on;
#endif
	return r;
}
}	// namespace airmslamp
