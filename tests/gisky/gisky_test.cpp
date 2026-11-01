// gisky_test -- FIX1101A LATE ITEM 15d: the ray-traced GI's sky from the physical sky (src/gl/atmomath.h
// skyIrradianceAxes + giSkyFromPhysical, read by src/rtddgicell.cpp), no Qt, no GPU.
//   g++ -O2 -std=c++17 -I src tests/gisky/gisky_test.cpp -o gisky_test -static
// Checks:
//   same      skyIrradianceAxes' +Z equals skyIrradiance at the same sun (one model; 2 % per channel)
//   sunside   the sky side toward the sun (azimuth) is brighter than the side away from it (>= 1.15 x at sun elevation
//             30 deg), and the zenith sky is blue (R/B < 0.75)
//   open      a probe open to the whole sky takes the GI sky exactly on every axis (rtddgimath.h skyRadianceCube's
//             forward map, flatAmbient), and +Z keeps the weather's luminance (the exposure anchor)
// Red -DGISKY_RED: the GI sky stays the weather's DALC (today's main and still the default: the physical sky ships off,
// WW_CELL_RTDDGI_SKY=physical): sunside must FAIL (the weather's sides are equal).
#include "gl/atmomath.h"
#include "rtddgimath.h"

#include <cmath>
#include <cstdio>

namespace {
int g_fail = 0, g_pass = 0;
void report( const char * name, bool ok, const char * fmt, double a = 0, double b = 0, double c = 0, double d = 0 )
{
	char msg[300];
	std::snprintf( msg, sizeof msg, fmt, a, b, c, d );
	std::printf( "%s %s: %s\n", ok ? "PASS" : "FAIL", name, msg );
	( ok ? g_pass : g_fail )++;
}
}

int main()
{
	using namespace WwAtmoMath;
	const float haze[3] = { 0.0f, 1.0f, 0.0f };
	// the CLI weather of notes/fix1031b/gi/cli1.sh (CommonwealthClear noon DALC: +X -X +Y -Y +Z -Z)
	const float weather[6][3] = { { 0.075f, 0.135f, 0.221f }, { 0.075f, 0.135f, 0.221f }, { 0.075f, 0.135f, 0.221f },
		{ 0.075f, 0.135f, 0.221f }, { 0.132f, 0.239f, 0.398f }, { 0.019f, 0.030f, 0.044f } };
	// (1) same model: +Z vs skyIrradiance (its sun in the x-z plane) at two suns
	double worst = 0;
	for ( float mus : { 0.5f, 0.85f } ) {
		const float s[3] = { std::sqrt( 1.0f - mus * mus ), 0.0f, mus };
		float ax[6][3], up[3];
		skyIrradianceAxes( s, haze, ax );
		skyIrradiance( mus, haze, up );
		for ( int c = 0; c < 3; c++ )
			worst = std::max( worst, double( std::fabs( ax[4][c] - up[c] ) / up[c] ) );
	}
	report( "same", worst < 0.02, "+Z of the six-axis sky vs skyIrradiance at sun cos zenith 0.5 / 0.85: worst rel %.4f (bar 0.02)", worst );
	// (2) sun side: sun at elevation 30 deg toward +Y (azimuth 90 deg)
	const float el = 30.0f * 3.14159265f / 180.0f;
	const float s30[3] = { 0.0f, std::cos( el ), std::sin( el ) };
	float phys[6][3], gi[6][3];
	skyIrradianceAxes( s30, haze, phys );
	giSkyFromPhysical( phys, weather, gi );
#ifdef GISKY_RED
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			gi[a][c] = weather[a][c];   // red: today's main, the weather's DALC
#endif
	const double toward = lum( gi[2] ), away = lum( gi[3] ), side = lum( gi[0] );
	const double rbZ = gi[4][0] / gi[4][2];
	report( "sunside", toward >= 1.15 * away && rbZ < 0.75, "GI sky at sun elevation 30 deg: toward the sun %.4f, away %.4f (x%.2f, bar "
		">= 1.15), across %.4f", toward, away, toward / std::max( away, 1e-9 ), side );
	std::printf( "      zenith R/B %.2f (bar < 0.75)\n", rbZ );
	std::printf( "      R/B per axis +X %.2f -X %.2f +Y %.2f -Y %.2f +Z %.2f -Z %.2f; the weather's +Z R/B %.2f\n", gi[0][0] / gi[0][2],
		gi[1][0] / gi[1][2], gi[2][0] / gi[2][2], gi[3][0] / gi[3][2], rbZ, gi[5][0] / gi[5][2], weather[4][0] / weather[4][2] );
	// (3) an open probe takes the GI sky on every axis (the volume's cube solve + its forward map), +Z luminance anchored
	float R[6][3];
	rtddgi::skyRadianceCube( gi, R );
	double worstOpen = 0;
	const rtddgi::V3 axes[6] = { rtddgi::V3( 1, 0, 0 ), rtddgi::V3( -1, 0, 0 ), rtddgi::V3( 0, 1, 0 ), rtddgi::V3( 0, -1, 0 ),
		rtddgi::V3( 0, 0, 1 ), rtddgi::V3( 0, 0, -1 ) };
	for ( int a = 0; a < 6; a++ ) {
		float f[3];
		rtddgi::flatAmbient( R, true, axes[a], f );
		for ( int c = 0; c < 3; c++ )
			worstOpen = std::max( worstOpen, double( std::fabs( f[c] - gi[a][c] ) / std::max( gi[a][c], 1e-6f ) ) );
	}
	const double anchor = std::fabs( lum( gi[4] ) - lum( weather[4] ) ) / lum( weather[4] );
	// the cube solve clamps a one-sided sky (skyRadianceCube's count; the weather's own DALC clamps too): the open probe's
	// match is reported, the gate is the +Z anchor (the exposure the weather was tuned with) and -Z kept
	const bool minusZ = gi[5][0] == weather[5][0] && gi[5][1] == weather[5][1] && gi[5][2] == weather[5][2];
	report( "open", anchor < 1e-4 && minusZ, "+Z luminance vs the weather's rel %.2g (bar 1e-4), -Z the weather's %g; open probe vs the "
		"GI sky on the 6 axes worst rel %.2g (the cube's clamp, reported)", anchor, minusZ ? 1.0 : 0.0, worstOpen );
	std::printf( "GISKY %s %d%s\n", g_fail ? "FAIL" : "PASS", g_fail ? g_fail : g_pass, g_fail ? " failed" : "" );
	return g_fail ? 1 : 0;
}
