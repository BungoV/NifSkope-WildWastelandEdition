// lane SKYA: prints src/gl/atmomath.h's results on a fixed grid, one "key value..." line each, for
// tests/spells/atmomath_check.py (which compares them with tests/spells/atmo_model.py and published numbers).
// Build: g++ -O2 -std=c++17 -I src tests/atmomath/atmomath_dump.cpp -o <scratch>/atmomath_dump.exe
#include "gl/atmomath.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace WwAtmoMath;

int main()
{
	float hz[2][3];
	hazeFromFog( 0.0f, 0.0f, 0.0f, 10000.0f, false, hz[0] );
	hazeFromFog( 20000.0f / ( kUnitKm * 1000.0f ), 0.6f, 0.0f, 10000.0f, false, hz[1] );	// Far 20 km, Max 0.6
	for ( int k = 0; k < 2; k++ )
		std::printf( "haze %d %.7g %.7g %.7g\n", k, hz[k][0], hz[k][1], hz[k][2] );
	const float mus[] = { 1.0f, 0.7071f, 0.3f, 0.1f, 0.03f, 0.0f };
	for ( int k = 0; k < 2; k++ )
		for ( float mu : mus ) {
			float t[3];
			cpuTrans( 0.0f, mu, hz[k], false, t );
			std::printf( "trans %d %.4f %.7g %.7g %.7g\n", k, mu, t[0], t[1], t[2] );
		}
	// ATMOSUN1: sun at 45 deg and at 3 deg, the weather's key (a warm-white) and ambient (a blue-grey)
	const float key[3] = { 1.0f, 0.95f, 0.85f }, amb[3] = { 0.20f, 0.24f, 0.30f };
	float t45[3], tlow[3];
	cpuTrans( 0.0f, 0.7071f, hz[1], false, t45 );
	cpuTrans( 0.0f, 0.0523f, hz[1], false, tlow );
	const float kl = lum( key );
	for ( int mode = 1; mode <= 2; mode++ ) {
		float o45[3], olow[3];
		sunLight( mode, kl, t45, t45, key, o45 );
		sunLight( mode, kl, tlow, t45, key, olow );
		std::printf( "sun %d 45 %.7g %.7g %.7g\n", mode, o45[0], o45[1], o45[2] );
		std::printf( "sun %d 3 %.7g %.7g %.7g\n", mode, olow[0], olow[1], olow[2] );
	}
	float a45[3] = { amb[0], amb[1], amb[2] }, alow[3] = { amb[0], amb[1], amb[2] };
	ambientTint( t45, t45, a45 );
	ambientTint( tlow, t45, alow );
	std::printf( "amb 45 %.7g %.7g %.7g\n", a45[0], a45[1], a45[2] );
	std::printf( "amb 3 %.7g %.7g %.7g\n", alow[0], alow[1], alow[2] );
	std::printf( "key %.7g %.7g %.7g\n", key[0], key[1], key[2] );
	std::printf( "ambin %.7g %.7g %.7g\n", amb[0], amb[1], amb[2] );
	// the sky irradiance (the ambient's tint) at 45, 20, 10 and 3 degrees
	for ( int deg : { 45, 20, 10, 3 } ) {
		float e[3];
		skyIrradiance( std::sin( float( deg ) * kPi / 180.0f ), hz[1], e );
		std::printf( "irr %d %.7g %.7g %.7g\n", deg, e[0], e[1], e[2] );
	}
	// the shipped policy (skyKeyLight, what atmosphere.cpp's wwAtmoKeyLight calls), with and without red "flat"
	for ( int redFlat = 0; redFlat < 2; redFlat++ )
		for ( int mode = 1; mode <= 2; mode++ )
			for ( int deg : { 45, 3 } ) {
				float s[3] = { key[0], key[1], key[2] }, a[1][3] = { { amb[0], amb[1], amb[2] } };
				skyKeyLight( mode, deg == 45 ? 0.7071f : 0.0523f, hz[1], kl, redFlat != 0, s, a, 1 );
				std::printf( "skykey %d %d %d %.7g %.7g %.7g %.7g %.7g %.7g\n", redFlat, mode, deg, s[0], s[1], s[2],
					a[0][0], a[0][1], a[0][2] );
			}
	// NIGHT1
	for ( int p = 0; p < 8; p++ )
		std::printf( "moon %d %.4f %.7g %.7g\n", p, moonPhaseAlpha( p ), moonPhaseFactor( p, false ), moonPhaseFactor( p, true ) );
	// NIGHT1: the stars' extinction against the zenith, with and without red "noext"
	for ( int redNoExt = 0; redNoExt < 2; redNoExt++ )
		for ( float mu : { 1.0f, 0.5f, 0.1f, 0.03f } ) {
			float e[3];
			starExtinction( mu, hz[1], redNoExt != 0, e );
			std::printf( "star %d %.4f %.7g %.7g %.7g\n", redNoExt, mu, e[0], e[1], e[2] );
		}
	// EXPO1: a synthetic sky-view table holding max(cos zenith, 0) at each texel's direction (the inverse mapping,
	// atmo_common.glsl atSkyUvToDir); its solid-angle mean over the upper hemisphere is exactly 1/2
	{
		const int w = 192, h = 108;
		const float r = kRb + 0.002f;
		std::vector<float> t( size_t( w ) * size_t( h ) * 4 );
		const float vh = std::sqrt( r * r - kRb * kRb ), beta = std::acos( std::clamp( vh / r, -1.0f, 1.0f ) ), zh = kPi - beta;
		for ( int j = 0; j < h; j++ )
			for ( int i = 0; i < w; i++ ) {
				const float vy = float( j ) / float( h - 1 );
				float za;
				if ( vy < 0.5f ) {
					const float c = 1.0f - 2.0f * vy;
					za = zh * ( 1.0f - c * c );
				} else {
					const float c = vy * 2.0f - 1.0f;
					za = zh + beta * c * c;
				}
				float * p = t.data() + ( size_t( j ) * size_t( w ) + size_t( i ) ) * 4;
				p[0] = p[1] = p[2] = std::max( std::cos( za ), 0.0f );
				p[3] = 1.0f;
			}
		std::printf( "skymean %.7g %.7g\n", skyViewMeanLum( t.data(), w, h, r, false ), skyViewMeanLum( t.data(), w, h, r, true ) );
	}
	// ATMOCHECK1: a synthetic dome (rings at 0, 10, 30, 60, 90 deg; Horizon at 0 fading to Sky-Upper at 90, two
	// vertices a ring with a 0.01 deg jitter that must merge) and a blue physical color, tinted at 0, 20, 90 deg
	{
		const float els[] = { 0.0f, 0.01f, 10.0f, 10.01f, 30.0f, 30.01f, 60.0f, 60.01f, 90.0f, 90.0f };
		float wts[10][3];
		for ( int i = 0; i < 10; i++ ) {
			const float u = els[i] / 90.0f;
			wts[i][0] = 1.0f - u;
			wts[i][1] = 0.0f;
			wts[i][2] = u;
		}
		std::vector<float> re, rw;
		tintRings( els, wts, 10, re, rw );
		const float H[3] = { 1.0f, 0.5f, 0.2f }, L[3] = { 0.3f, 0.3f, 0.3f }, U[3] = { 0.2f, 0.4f, 1.0f };
		const float phys[3] = { 0.3f, 0.5f, 0.9f };
		for ( int k = 0; k < 2; k++ ) {
			float tab[kTintN][3];
			tintTable( re, rw, H, L, U, 1.3f, k == 1, tab );
			for ( float el : { 0.0f, 20.0f, 90.0f, -5.0f } ) {
				float o[3];
				weatherTint( phys, tab, el, o );
				std::printf( "tint %d %zu %.7g %.7g %.7g %.7g\n", k, re.size(), el, o[0], o[1], o[2] );
			}
		}
	}
	// EXPO1
	std::printf( "expo %.7g %.7g %.7g\n", adaptedWithSky( 0.2f, 0.6f, 1.5f, false ), adaptedWithSky( 0.2f, 0.6f, 1.5f, true ),
		adaptedWithSky( 0.2f, 1.0f, 1.5f, false ) );
	return 0;
}
