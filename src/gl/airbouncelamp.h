#pragma once
/* lane AIRBOUNCE1 (AIR-LAMPS-BOUNCE): a placed light's light bounced off the ground into the cell air, the CPU twin of
 * cell_volair.frag airLampGnd. Fitted to a backward Monte Carlo of a point lamp over a Lambertian floor in the same
 * medium (notes/airbounce1/tools/lamp_bounce_mc.py, bounce_fit.py; fixture tests/airbouncelamp/airbouncelamp_mc.tsv).
 * The lit floor stands for the bounce as a source at the lamp's image (hl under the floor under the lamp):
 *   pi B = K a (1 + m sigma D) 2 cos pi P2(c_s) (d / D)^2
 * D = |x - image|, cos = (hx + hl) / D (the floor's Lambertian lobe toward x), c_s = dot(x -> image, view), P2 = the
 * phase with each HG lobe's g x sb; a the floor's albedo. Over its value at d the lamp's e already carries 1 / d^2, so
 * (d / D)^2 moves it to the image. K 0.94, m 1, sb 0.5. Over 966 MC rows (6 phases, sigma 0.005..0.1 / m, lamp 2.5..8 m
 * up, albedo 0.1 / 0.2 / 0.4) single + AIRMS1's rest + this is within rms log 0.133 of the MC total (without it 0.395).
 * Off (or red): B = 0. The shader holds the term only in the loader's WW_CELL_AIR_BOUNCE program. */
#include <cmath>

#include "gl/airmslamp.h"

namespace airbouncelamp
{
constexpr float kK = 0.94f, kM = 1.0f, kSb = 0.5f;
constexpr float kAlbedo = 0.2f;		// the floor's albedo (cellvolair.cpp: volGndLamp.x = kK kAlbedo)
constexpr float kFloor = 192.0f;	// game units under the lamp when no terrain grid gives the floor (about 2.7 m)

// what the shader multiplies e by for the bounce: lamp L, floor at z hf, sample x, view dW (unit), d = |L - x|,
// od the optical depth over d; 0 when the lamp or x is under the floor
inline float lampGnd( const airmslamp::Lobes & p, const float L[3], float hf, const float x[3], const float dW[3], float d,
	float od, float albedo, bool on )
{
#ifndef AIRBOUNCELAMP_RED
	if ( !on )
		return 0.0f;
	const float hl = L[2] - hf, hx = x[2] - hf;
	if ( hl <= 0.0f || hx < 0.0f )
		return 0.0f;
	const float v[3] = { L[0] - x[0], L[1] - x[1], ( hf - hl ) - x[2] };
	const float D2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2], D = std::sqrt( D2 );
	const float c = ( v[0] * dW[0] + v[1] * dW[1] + v[2] * dW[2] ) / D;
	return kK * albedo * ( 1.0f + kM * ( od / d ) * D ) * 2.0f * ( ( hx + hl ) / D ) * airmslamp::phasePi( p, c, kSb ) * ( d * d / D2 );
#else
	(void)p; (void)L; (void)hf; (void)x; (void)dW; (void)d; (void)od; (void)albedo; (void)on;
	return 0.0f;
#endif
}
}	// namespace airbouncelamp
