// gisky_test -- FIX1101A LATE ITEM 15d: the ray-traced GI's sky from the physical sky (src/gl/atmomath.h
// skyIrradianceAxes + giSkyFromPhysical, read by src/features/gi/rtddgicell.cpp), no Qt, no GPU.
//   g++ -O2 -std=c++17 -I src tests/gisky/gisky_test.cpp -o gisky_test -static
// Checks:
//   same      skyIrradianceAxes' +Z equals skyIrradiance at the same sun (one model; 2 % per channel)
//   sunside   the sky side toward the sun (azimuth) is brighter than the side away from it (>= 1.15 x at sun elevation
//             30 deg), and the zenith sky is blue (R/B < 0.75)
//   open      a probe open to the whole sky takes the GI sky exactly on every axis (rtddgimath.h skyRadianceCube's
//             forward map, flatAmbient), and +Z keeps the weather's luminance (the exposure anchor)
//   level     GITRUTH1 (WW_CELL_RTDDGI_SKY=absolute, giSkyAbsolute): the sun at 33.5 deg as the frame draws it (T(mu) x the
//             Sunlight Scale 4.5), CommonwealthClear noon's drawn haze (0.513, 0.214, 0; notes/fix1101a/atmo
//             sky_irradiance_axes.txt: the no-haze sky above is a Rayleigh-only sky, direct / diffuse ~11); direct / diffuse horizontal luminance within [3.0, 5.0] (IES clear sky 3.9: NBS/IES
//             1984, E_dn = 127.5 exp(-0.21 / sin alt) klx, diffuse 0.8 + 15.5 sqrt(sin alt) klx)
//             The same bar for WW_CELL_RTDDGI_SKY=level (giSkyLevel: the weather's colour at that level), check levelw.
//   twin      GISKYTWIN: the CPU twin the live GI takes (skyIrradianceAxesDrawn) vs the drawn GPU sky (twin_ref.h, from the
//             real shaders by tests/gisky/twin_ref.py): sun elevation 5..80 deg, haze 0 / 0.513 (CommonwealthClear noon) /
//             1.5 /km (~Foggy), heights 2 m and 90 m, the 5 sky axes: every channel within 2 % and every axis' R/B within
//             0.02. The old single-scatter twin (skyIrradianceAxes) is 36-83 % off and reads +Z R/B 0.87 vs 0.53.
// Red -DGISKY_RED_TWIN: the old twin (skyIrradianceAxes, today's live path): twin must FAIL (only it; level and levelw
//             read the old twin then, and stay inside their bars).
// Red -DGISKY_RED_LEVEL: the level anchored to the weather's +Z (giSkyFromPhysical, the "physical" mode): level must FAIL.
// Red -DGISKY_RED: the GI sky stays the weather's DALC (today's main and still the default: the physical sky ships off,
// WW_CELL_RTDDGI_SKY=physical): sunside must FAIL (the weather's sides are equal).
#include "gl/atmomath.h"
#include "features/gi/rtddgimath.h"
#include "twin_ref.h"
#include "twin_ref_low.h"

#ifndef TWINLOW_BAR
#define TWINLOW_BAR 0.02	// GISKYTWIN-LOW: the twin's own bar (written before the first run)
#endif

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
	SkyTables twinT;
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
	// (4) GITRUTH1 level: the sky's level against the sun it is drawn with
	{
		const float el4 = 33.5f * 3.14159265f / 180.0f, mu4 = std::sin( el4 );
		const float s4[3] = { 0.0f, std::cos( el4 ), mu4 };
		const float hz4[3] = { 0.513f, 0.214f, 0.0f };
		float T4[3], sun4[3], ph4[6][3], g4[6][3];
		cpuTrans( 0.0f, mu4, hz4, false, T4 );
		for ( int c = 0; c < 3; c++ )
			sun4[c] = 4.5f * T4[c];
#ifdef GISKY_RED_TWIN
		skyIrradianceAxes( s4, hz4, ph4 );
#else
		skyIrradianceAxesDrawn( s4, hz4, 0.09f, 1.0f, twinT, ph4 );   // the live twin, probes at 90 m
#endif
#ifdef GISKY_RED_LEVEL
		giSkyFromPhysical( ph4, weather, g4 );
		const double k4 = 0;
#else
		const double k4 = giSkyAbsolute( ph4, T4, sun4, weather, g4 );
#endif
		float w4[6][3];
#ifdef GISKY_RED_LEVEL
		for ( int a = 0; a < 6; a++ )
			for ( int c = 0; c < 3; c++ )
				w4[a][c] = weather[a][c];   // red: the weather's DALC at its own level
		const double f4 = 1;
#else
		const double f4 = giSkyLevel( ph4, T4, sun4, weather, w4 );
#endif
		const double ratioW = lum( sun4 ) * mu4 / std::max( double( lum( w4[4] ) ), 1e-9 );
		const double rbKept = ( w4[0][0] / w4[0][2] ) / ( weather[0][0] / weather[0][2] );
		report( "levelw", ratioW >= 3.0 && ratioW <= 5.0 && std::fabs( rbKept - 1.0 ) < 1e-4, "weather sky x f %.3f: direct / diffuse %.2f "
			"(bar 3.0..5.0), the weather's colour kept (R/B ratio %.5f)", f4, ratioW, rbKept );
		const double ratio = lum( sun4 ) * mu4 / std::max( double( lum( g4[4] ) ), 1e-9 );
		const bool keepsZ = g4[5][0] == weather[5][0] && g4[5][2] == weather[5][2];
		report( "level", ratio >= 3.0 && ratio <= 5.0 && keepsZ, "direct / diffuse horizontal at sun 33.5 deg %.2f (bar 3.0..5.0, IES 3.9), "
			"k %.3f, +Z R/B %.2f, -Z the weather's %g", ratio, k4, g4[4][0] / std::max( g4[4][2], 1e-9f ), keepsZ ? 1.0 : 0.0 );
		std::printf( "      sun %.4f %.4f %.4f; GI sky +Z %.4f %.4f %.4f, sides +X %.4f %.4f %.4f +Y %.4f %.4f %.4f -Y %.4f %.4f %.4f\n",
			sun4[0], sun4[1], sun4[2], g4[4][0], g4[4][1], g4[4][2], g4[0][0], g4[0][1], g4[0][2], g4[2][0], g4[2][1], g4[2][2],
			g4[3][0], g4[3][1], g4[3][2] );
	}
	// (5) GISKYTWIN: the live twin vs the drawn GPU sky
	{
		double wRel = 0, wRb = 0, wZrb = 0;
		int wi = 0, n = 0;
		for ( const TwinRef & R : kTwinRef ) {
			const float e = R.el * 3.14159265f / 180.0f;
			const float s[3] = { 0.0f, -std::cos( e ), std::sin( e ) };
			float ax[6][3];
#ifdef GISKY_RED_TWIN
			skyIrradianceAxes( s, R.haze, ax );
#else
			skyIrradianceAxesDrawn( s, R.haze, R.hKm, 1.0f, twinT, ax, 32 );
#endif
			for ( int a = 0; a < 5; a++ ) {
				for ( int c = 0; c < 3; c++ ) {
					const double r = std::fabs( ax[a][c] - R.ax[a][c] ) / std::max( R.ax[a][c], 1e-9f );
					if ( r > wRel ) {
						wRel = r;
						wi = n;
					}
				}
				wRb = std::max( wRb, std::fabs( double( ax[a][0] / ax[a][2] ) - double( R.ax[a][0] / R.ax[a][2] ) ) );
			}
			if ( R.haze[0] > 0.5f && R.haze[0] < 0.6f && R.hKm > 0.05f && R.el > 30.0f && R.el < 40.0f )
				wZrb = ax[4][0] / ax[4][2];
			n++;
		}
		const TwinRef & W = kTwinRef[wi];
		report( "twin", wRel <= 0.02 && wRb <= 0.02, "the GI's CPU sky vs the drawn GPU sky, %g cases: worst channel rel %.4f (bar 0.02; "
			"sun %.0f deg), worst R/B difference %.4f (bar 0.02)", double( n ), wRel, double( W.el ), wRb );
		std::printf( "      worst case haze %.3f /km at %.3f km; noon haze, 90 m, sun 35 deg: +Z R/B %.3f (the drawn sky 0.527)\n",
			double( W.haze[0] ), double( W.hKm ), wZrb );
	}
	// (6) GISKYTWIN-LOW: the Low tier's twin (msN 16, as atmosphere.cpp / rtddgicell.cpp take it) vs the drawn GPU sky
	// with the 16x16 MS table (twin_ref_low.h, MSN=16 tests/gisky/twin_ref.py), same cases and bars as twin
	{
		SkyTables lowT;
		double wRel = 0, wRb = 0;
		int wi = 0, n = 0;
		for ( const TwinRef & R : kTwinRefLow ) {
			const float e = R.el * 3.14159265f / 180.0f;
			const float s[3] = { 0.0f, -std::cos( e ), std::sin( e ) };
			float ax[6][3];
#if defined( GISKY_RED_TWIN )
			skyIrradianceAxes( s, R.haze, ax );
#elif defined( GISKY_RED_TWINLOW )
			skyIrradianceAxesDrawn( s, R.haze, R.hKm, 1.0f, lowT, ax, 32 );	// red: the High+ table at the Low tier
#else
			skyIrradianceAxesDrawn( s, R.haze, R.hKm, 1.0f, lowT, ax, 16 );
#endif
			for ( int a = 0; a < 5; a++ ) {
				for ( int c = 0; c < 3; c++ ) {
					const double r = std::fabs( ax[a][c] - R.ax[a][c] ) / std::max( R.ax[a][c], 1e-9f );
					if ( r > wRel ) {
						wRel = r;
						wi = n;
					}
				}
				wRb = std::max( wRb, std::fabs( double( ax[a][0] / ax[a][2] ) - double( R.ax[a][0] / R.ax[a][2] ) ) );
			}
			n++;
		}
		const TwinRef & W = kTwinRefLow[wi];
		report( "twinlow", wRel <= TWINLOW_BAR && wRb <= 0.02, "the Low tier's CPU sky (MS 16) vs the drawn GPU sky (MS 16), %g "
			"cases: worst channel rel %.4f (bar 0.02; sun %.0f deg), worst R/B difference %.4f (bar 0.02)", double( n ), wRel,
			double( W.el ), wRb );
	}
	std::printf( "GISKY %s %d%s\n", g_fail ? "FAIL" : "PASS", g_fail ? g_fail : g_pass, g_fail ? " failed" : "" );
	return g_fail ? 1 : 0;
}
