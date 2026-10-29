// TERRAINBLOCK1 unit test of src/cellvertexalpha.h, no Qt, no GL, no game data.
// 1. The rule: which near cell shapes keep their own vertex alpha (lit + SLSF1 Vertex_Alpha now too).
// 2. The ground-decal case it was written for (CoastDune02's CoastGrassSand01Decal: border vertex alpha 0x9F, inner
//    0xFF, alpha test ref 110, texture alpha 96..239 with percentiles 10/50/90 = 133/150/179): the decal must be
//    cut along its border (a hole edge frays) and kept inside. With the old rule the vertex alpha read 1 and the
//    border passed everywhere: a hard-edged hole, bungo's grey blocks.
// 3. GLTFCELL1: a coloured shape without the flag keeps it too (the church window panes).
// `--red` evaluates the old rule (WW_CELL_VERTEXALPHA_RED=1), `--flagred` the flag rule (=flag): both must FAIL.
#include "cellvertexalpha.h"

#include <cstdio>
#include <cstring>

namespace {

int fails = 0;

void expect( bool got, bool want, const char * what )
{
	if ( got != want ) {
		std::printf( "FAIL %s: got %d want %d\n", what, int( got ), int( want ) );
		fails++;
	}
}

// texture alpha across the decal's texels, piecewise linear through the measured percentiles (0..255)
double texAlphaAt( double q )
{
	static const double qs[5] = { 0.0, 0.1, 0.5, 0.9, 1.0 }, as[5] = { 96, 133, 150, 179, 239 };
	for ( int i = 0; i < 4; i++ )
		if ( q <= qs[i + 1] )
			return as[i] + ( as[i + 1] - as[i] ) * ( q - qs[i] ) / ( qs[i + 1] - qs[i] );
	return as[4];
}

// share of texels passing the alpha test (a > ref, the BGSM's GREATER) at one vertex alpha as drawn
double passShare( double drawnVa )
{
	const int n = 1000;
	int pass = 0;
	for ( int i = 0; i < n; i++ )
		pass += ( drawnVa * texAlphaAt( ( i + 0.5 ) / n ) / 255.0 > 110.0 / 255.0 ) ? 1 : 0;
	return double( pass ) / n;
}

}	// namespace

int main( int argc, char ** argv )
{
	const bool red = argc > 1 && !std::strcmp( argv[1], "--red" );
	const bool flagRed = argc > 1 && !std::strcmp( argv[1], "--flagred" );
	// own colour, effect, cable, Vertex_Alpha flag, tree anim
	expect( wwCellKeepsVertexAlpha( false, true, false, true, false, red, flagRed ), false, "no colour stream" );
	expect( wwCellKeepsVertexAlpha( true, true, false, false, false, red, flagRed ), true, "effect shader" );
	expect( wwCellKeepsVertexAlpha( true, false, true, false, false, red, flagRed ), true, "cable wind weight" );
	expect( wwCellKeepsVertexAlpha( true, false, false, true, false, red, flagRed ), true, "lit shape with Vertex_Alpha" );
	expect( wwCellKeepsVertexAlpha( true, false, false, false, false, red, flagRed ), true, "lit shape without Vertex_Alpha" );
	expect( wwCellKeepsVertexAlpha( true, false, false, true, true, red, flagRed ), false, "tree: wind weight, not opacity" );
	expect( wwCellKeepsVertexAlpha( true, false, false, false, true, red, flagRed ), false, "tree without the flag" );

	// GLTFCELL1: BldChurch01Open's window pane, no Vertex_Alpha flag, vertex alpha 0x19, texture alpha 80, ref 10
	const bool paneKeep = wwCellKeepsVertexAlpha( true, false, false, false, false, red, flagRed );
	const double pane = ( paneKeep ? 25.0 / 255.0 : 1.0 ) * 80.0;
	std::printf( "church pane drawn alpha %.1f (want <= 10: cut, as in the game)\n", pane );
	if ( pane > 10.0 ) {
		std::printf( "FAIL church window panes drawn solid\n" );
		fails++;
	}

	const bool keep = wwCellKeepsVertexAlpha( true, false, false, true, false, red, flagRed );
	const double border = passShare( keep ? 159.0 / 255.0 : 1.0 ), inner = passShare( 1.0 );
	std::printf( "decal pass share: border %.3f (want < 0.25), inner %.3f (want > 0.9)\n", border, inner );
	if ( !( border < 0.25 ) ) {
		std::printf( "FAIL decal border passes the alpha test: hard-edged holes\n" );
		fails++;
	}
	if ( !( inner > 0.9 ) ) {
		std::printf( "FAIL decal inside is cut\n" );
		fails++;
	}
	std::printf( "%s (%d failures)\n", fails ? "CELL VERTEX ALPHA FAIL" : "CELL VERTEX ALPHA PASS", fails );
	return fails ? 1 : 0;
}
