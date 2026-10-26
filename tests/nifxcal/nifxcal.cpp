// Lane NIFXCAL1: every vanilla light, to .nifx terms and back (src/io/nifxlights: fromVanilla, then convert).
// Colour bytes within 1 per channel (through kelvin + tint, after scaling the vanilla colour to a brightest 255),
// the drawn light (bytes x fade) and the radius within 1%.
// Build: g++ -std=c++17 -O2 -I<tree>/src tests/nifxcal/nifxcal.cpp src/io/nifxlights.cpp -o nifxcal
// Run:   nifxcal <vanilla_lights.tsv> [--red anchor800|notint|nokelvin|noradius]   (a red must FAIL)
// The tsv (notes/nifxcal1/unit, from the census): r g b fade radius type fov shadow placed, one distinct light a row.
#include "io/nifxlights.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main( int argc, char ** argv )
{
	if ( argc < 2 ) {
		std::printf( "usage: nifxcal <vanilla_lights.tsv> [--red anchor800|notint|nokelvin|noradius]\n" );
		return 2;
	}
	std::string red;
	for ( int k = 2; k + 1 < argc; ++k )
		if ( std::string( argv[k] ) == "--red" )
			red = argv[k + 1];
	std::ifstream in( argv[1] );
	if ( !in ) {
		std::printf( "NIFXCAL FAIL: cannot read %s\n", argv[1] );
		return 2;
	}
	long rows = 0, placed = 0, bad = 0, badPlaced = 0, outOfRange = 0, tinted = 0, kelvinOnly = 0, white = 0, ownRadius = 0;
	int worstByte = 0;
	double worstFade = 0, worstRadius = 0;
	std::string line;
	while ( std::getline( in, line ) ) {
		if ( line.empty() || line[0] == '#' )
			continue;
		std::istringstream s( line );
		int col[3], type, shadow;
		double fade, radius, fov;
		long n;
		if ( !( s >> col[0] >> col[1] >> col[2] >> fade >> radius >> type >> fov >> shadow >> n ) ) {
			std::printf( "NIFXCAL FAIL: bad row %s\n", line.c_str() );
			return 2;
		}
		rows++;
		placed += n;
		// outside the spec's ranges (radius 16..16384, lumens >= 0): the reader clamps them, no round trip owed
		if ( radius < 16 || radius > 16384 || fade < 0 ) {
			outOfRange += n;
			continue;
		}
		nifxl::Light l = nifxl::fromVanilla( col, fade, radius, type, fov, shadow != 0 );
		if ( red == "notint" )
			l.hasTint = false;
		if ( red == "nokelvin" )
			l.hasKelvin = false;
		if ( red == "noradius" )
			l.hasRadius = false;
		tinted += l.hasTint ? n : 0;
		kelvinOnly += l.hasKelvin && !l.hasTint ? n : 0;
		white += !l.hasKelvin && !l.hasTint ? n : 0;
		ownRadius += l.hasRadius ? n : 0;
		const nifxl::Converted c = nifxl::convert( l, false,
			red == "anchor800" ? nifxl::kLumensPerFadeOld : nifxl::kLumensPerFade,
			nifxl::kRadiusPerSqrtCandela );
		// FO4 bytes need not reach 255, the spec's always do: compare the hue (the vanilla colour scaled to a brightest
		// 255) byte for byte, and the light the engine draws (pow(byte/255, 2.2) x fade, the brightest channel) by 1%
		double lin[3], m = 0, got = 0;
		for ( int k = 0; k < 3; ++k ) {
			lin[k] = std::pow( col[k] / 255.0, 2.2 );
			m = std::max( m, lin[k] );
			got = std::max( got, std::pow( c.color[k] / 255.0, 2.2 ) * c.fade );
		}
		int db = 0;
		for ( int k = 0; k < 3; ++k ) {
			const int hue = m > 0 ? int( std::floor( 255 * std::pow( lin[k] / m, 1 / 2.2 ) + 0.5 ) ) : 255;
			db = std::max( db, std::abs( c.color[k] - hue ) );
		}
		const double want = m * fade;
		const double df = want > 0 ? std::fabs( got / want - 1 ) : std::fabs( got );
		// a radius the light keeps is written into the .nifx; one the law gives must match the law within 1%
		const double dr = std::fabs( c.radius / radius - 1 );
		worstByte = std::max( worstByte, db );
		worstFade = std::max( worstFade, df );
		worstRadius = std::max( worstRadius, dr );
		if ( db > 1 || df > 0.01 || dr > 0.01 ) {
			if ( bad < 5 )
				std::printf( "  off: %d,%d,%d fade %g radius %g type %d fov %g -> %d,%d,%d fade %g radius %g (K %g%s)\n",
					col[0], col[1], col[2], fade, radius, type, fov, c.color[0], c.color[1], c.color[2], c.fade, c.radius,
					l.hasKelvin ? l.kelvin : 0.0, l.hasTint ? " + tint" : "" );
			bad++;
			badPlaced += n;
		}
	}
	std::printf( "%ld distinct lights (%ld placed): %ld placed white, %ld kelvin only, %ld kelvin + tint, %ld keep their own radius,"
		" %ld outside the spec's ranges\n", rows, placed, white, kelvinOnly, tinted, ownRadius, outOfRange );
	std::printf( "worst: %d byte, drawn light %.4f%%, radius %.4f%%\n", worstByte, 100 * worstFade, 100 * worstRadius );
	const bool ok = bad == 0 && rows > 1000;
	std::printf( "%s: %ld distinct lights off (%ld placed)%s\n", ok ? "NIFXCAL PASS" : "NIFXCAL FAIL", bad, badPlaced,
		red.empty() ? "" : ( " (red " + red + ")" ).c_str() );
	return ok ? 0 : 1;
}
