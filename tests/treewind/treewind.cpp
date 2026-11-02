// Unit test of src/gl/celltreewind.h (the cell view's tree wind) against the game's own tree vertex shader, run op for
// op offline on synthetic placed trees (cases.tsv: 864 vertices, 6 trees x 3 winds, the game's answer in the last 3
// columns). Floor 0.5 units: the game itself moves a vertex by up to 0.38 when only the eye moves (its world sum is
// summed eye-relative in float32). -DTREEWIND_RED = today's path (no sway): must fail.
// build: g++ -std=c++17 -O2 -I<tree>/src tests/treewind/treewind.cpp -o treewind.exe ; run: treewind.exe <cases.tsv>
#include "gl/celltreewind.h"

#include <cmath>
#include <cstdio>

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 2 ) {
		std::printf( "usage: treewind <cases.tsv>\n" );
		return 2;
	}
	FILE * f = std::fopen( argv[1], "r" );
	if ( !f ) {
		std::printf( "cannot read %s\n", argv[1] );
		return 2;
	}
	int fails = 0, n = 0;
	double worst = 0.0, moved = 0.0;
	float c[27];
	for ( ;; ) {
		int got = 0;
		for ( int i = 0; i < 27; i++ )
			got += std::fscanf( f, "%f", &c[i] );
		if ( got != 27 )
			break;
		WwTreeWindK k;
		for ( int i = 0; i < 3; i++ )
			k.pivot[i] = c[8 + i];
		k.objSum = c[11];
		k.bound2 = c[12];
		k.amp = c[13];
		k.leafFreq = c[14];
		for ( int i = 0; i < 3; i++ )
			k.sumAxis[i] = c[15 + i];
		WwTreeWindW w;
		w.angle = c[18];
		w.prevPhase = c[19];
		w.phase = c[20];
		w.minS = c[21];
		w.maxS = c[22];
		w.freq = c[23];
		float out[3];
#ifdef TREEWIND_RED
		for ( int i = 0; i < 3; i++ )
			out[i] = c[i];
#else
		// the weld's weight round trip: bitangent length 2 + alpha
		const float wgt = wwTreeWindWeightOf( 2.0f + c[6] );
		wwTreeWindVertex( &c[0], &c[3], wgt, c[7], k, w, w.phase, out );
#endif
		double d = 0.0, m = 0.0;
		for ( int i = 0; i < 3; i++ ) {
			d = std::fmax( d, std::fabs( double( out[i] ) - c[24 + i] ) );
			m = std::fmax( m, std::fabs( double( c[24 + i] ) - c[i] ) );
		}
		worst = std::fmax( worst, d );
		moved = std::fmax( moved, m );
		fails += d > 0.5 ? 1 : 0;
		n++;
	}
	std::fclose( f );
	// the fade and the weight decode
	const bool fadeOk = wwTreeWindAmpFade( 3000.0f ) == 1.0f && wwTreeWindAmpFade( 5000.0f ) == 0.0f
		&& std::fabs( wwTreeWindAmpFade( 4100.0f ) - 0.5f ) < 1e-4f;
	const bool weightOk = wwTreeWindWeightOf( 1.0f ) == 0.0f && std::fabs( wwTreeWindWeightOf( 2.25f ) - 0.25f ) < 1e-6f;
	std::printf( "treewind: %d vertices, %d over 0.5 units, worst %.4f, largest game sway %.2f; fade %s, weight %s\n", n,
		fails, worst, moved, fadeOk ? "ok" : "BAD", weightOk ? "ok" : "BAD" );
	const bool ok = n == 864 && fails == 0 && moved > 1.0 && fadeOk && weightOk;
	std::printf( "TREEWIND UNIT %s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
