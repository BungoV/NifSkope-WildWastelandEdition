#ifndef WW_CLOUDDECKMATH_H
#define WW_CLOUDDECKMATH_H

/* lane SKYLIGHT1: the light that comes through the drawn cloud deck to the ground, no GL and no Qt. The deck is the
 * volume clouds' own density (cloudvolmath.h bandDensity at the calibrated threshold), read as independent columns
 * (Cahalan et al. 1994, "The albedo of fractal stratocumulus clouds": the mean of plane-parallel columns). Each column
 * passes the sun as two-stream diffuse light (msDiffuseT, delta-Eddington) and the clear sky's diffuse light at the
 * diffusivity cosine 1 / 1.66 (Elsasser 1942). The direct sun through the deck stays with the key light (cloud shadow). */

#include "cloudvolmath.h"

namespace WwCloudDeck
{

struct Band
{
	float baseKm = 0.0f, topKm = 0.0f, density = 0.0f, sheet = 0.0f, cov = 0.0f;
};

struct Through
{
	float cover = 0.0f;	// mean column opacity 1 - e^-tau (= the map's coverage after calibration)
	float tKey = 0.0f;	// mean diffuse transmission of the sun (per unit horizontal sun irradiance on top)
	float tSky = 0.0f;	// mean transmission of the clear sky's diffuse light (direct + diffuse part)
	float tauMean = 0.0f;
};

constexpr float kDiffusivityMu = 1.0f / 1.66f;

inline bool bandOn( const Band & b )
{
	return b.cov > 0.0f && b.topKm - b.baseKm > 1e-3f && b.density > 0.0f;
}

//! band k's coverage table, as the volume's (cloudvolmath.h calibrate); a function of the band's shape alone, so cache it
inline void bandCal( const std::uint8_t * shape, int ns, const std::uint8_t * detail, int nd, int k, const Band & b,
	float cal[WwCloudVolMath::kCalN] )
{
	WwCloudVolMath::calibrate( shape, ns, detail, nd, k, b.baseKm, b.topKm, b.density, b.sheet, false, cal );
}

//! the deck over a ground point: Low and Mid bands of the volume with their tables, sun at mu0 (> 0)
inline Through through( const std::uint8_t * shape, int ns, const std::uint8_t * detail, int nd, const Band b[2],
	const float * const cal[2], float mu0 )
{
	using namespace WwCloudVolMath;
	constexpr int NC = 12, NZ = 40;
	float cp[2] = { 0.0f, 0.0f };
	for ( int k = 0; k < 2; k++ )
		if ( bandOn( b[k] ) )
			cp[k] = calAt( cal[k], b[k].cov );
	const float g = msMeanCosine( dropletParams( kDropletUm ) ), w = kAlbedo;
	mu0 = std::clamp( mu0, 0.02f, 1.0f );
	double cover = 0.0, tk = 0.0, ts = 0.0, tm = 0.0;
	for ( int j = 0; j < NC; j++ )
		for ( int i = 0; i < NC; i++ ) {
			const float x = ( float( i ) + 0.5f ) / NC * kShapeKm, y = ( float( j ) + 0.37f ) / NC * kShapeKm;
			float tau = 0.0f;
			for ( int k = 0; k < 2; k++ ) {
				if ( cp[k] <= 0.0f )
					continue;
				const float th = b[k].topKm - b[k].baseKm;
				for ( int s = 0; s < NZ; s++ ) {
					const float hf = ( float( s ) + 0.5f ) / NZ, z = b[k].baseKm + hf * th;
					const float sh = sample3( shape, ns, x / kShapeKm, y / kShapeKm, z / kShapeKm );
					const float de = sample3( detail, nd, x / kDetailKm, y / kDetailKm, z / kDetailKm );
					tau += bandDensity( sh, de, k, hf, cp[k], b[k].sheet ) * kSigma * b[k].density * th / NZ;
				}
			}
			cover += 1.0 - std::exp( -double( tau ) );
			tk += msDiffuseT( tau, mu0, g, w );
			ts += std::exp( -double( tau ) / kDiffusivityMu ) + msDiffuseT( tau, kDiffusivityMu, g, w );
			tm += tau;
		}
	const double n = double( NC * NC );
	Through t;
	t.cover = float( cover / n );
	t.tKey = float( tk / n );
	t.tSky = float( std::min( ts / n, 1.0 ) );
	t.tauMean = float( tm / n );
	return t;
}

/*! the up face's ambient C (= E / pi) under the deck: the sun's horizontal C on the deck top (key x sunlight scale x
 *  mu) through as diffuse light, plus the clear sky's up C through the deck, times the ground-deck reflections
 *  1 / (1 - rhoG Rdeck), Rdeck ~ 1 - tSky at albedo 0.99 (the adding method: Liou 2002, sec. 6.4). The 1D columns miss
 *  the light cloud sides send into the gaps of a broken deck, so the up face keeps the clear sky's share of the gaps
 *  (1 - cover) at least. SKYLIGHT1 round 3 (bungo: Cloud Density 10 -> 0 left the light the same): the floor was the
 *  whole clear sky, which a thick deck (tau 20) never went under; oldFloor = that floor (red WW_SKY_CLOUDDECK_RED=floor).
 *  Red: the clear sky's up C alone. */
inline float upC( const Through & t, float sunHorizC, float clearUpC, float rhoG, bool red, bool oldFloor = false )
{
	if ( red )
		return clearUpC;
	const float r = std::clamp( rhoG, 0.0f, 1.0f ) * std::clamp( 1.0f - t.tSky, 0.0f, 1.0f );
	const float floorC = oldFloor ? clearUpC : clearUpC * std::clamp( 1.0f - t.cover, 0.0f, 1.0f );
	return std::max( ( sunHorizC * t.tKey + clearUpC * t.tSky ) / ( 1.0f - r ), floorC );
}

/*! SKYLIGHT1 round 2: the deck over six sky axes (up = the index a face looking up reads, down = up ^ 1, sides 0..3).
 *  sunHorizC = the key on a level surface in C. pi: the axes are a record DALC, which the cell law reads over pi
 *  (lightmeas.cpp: stage DALC up 0.528897 -> C 0.1684), so up C = axis / pi; else the axes are C already (the physical
 *  sky's, giSkyGround's units). Up = upC; the sides (0.396 + rho / 2) x up blended by the cover (CIE overcast Ev / Eh +
 *  the ground's half); down + rho x the lift. Returns lum(up after) / lum(up before). */
inline float overAxes( float ax[6][3], int up, const Through & t, const float sunHorizC[3], float rhoG, bool pi,
	bool oldFloor = false )
{
	auto lum = []( const float v[3] ) { return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2]; };
	const float k = pi ? 3.14159265f : 1.0f, before = lum( ax[up] );
	for ( int c = 0; c < 3; c++ ) {
		const float upD = k * upC( t, sunHorizC[c], ax[up][c] / k, rhoG, false, oldFloor );
		const float add = upD - ax[up][c];
		for ( int a = 0; a < 4; a++ )
			ax[a][c] = ( 1.0f - t.cover ) * ax[a][c] + t.cover * ( 0.396f + 0.5f * rhoG ) * upD;
		ax[up][c] = upD;
		ax[up ^ 1][c] += rhoG * add;
	}
	return before > 1e-9f ? lum( ax[up] ) / before : 1.0f;
}

/*! SKYLIGHT1 round 2: the clear sky's in-scatter in front of the deck, seen from under it, x what the air there is lit by:
 *  the deck's diffuse light (upC's numerator, no ground term) over the clear sky's horizontal light (key + sky),
 *  per colour, blended by the cover. cloud_march.frag cvDeckAp is the GPU copy. Red: 1. */
inline float apUnder( const Through & t, float keyHorizC, float skyC )
{
	const float clr = keyHorizC + skyC;
	if ( clr <= 1e-9f )
		return 1.0f;
	const float und = keyHorizC * t.tKey + skyC * t.tSky;
	return ( 1.0f - t.cover ) + t.cover * std::clamp( und / clr, 0.0f, 1.0f );
}

}	// namespace WwCloudDeck

#endif
