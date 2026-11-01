// lane PERFFIX2 (item 7): src/gl/contactlines.h alone, with a CPU twin of res/shaders/ww_contact.comp's lane math
// (float, no fused multiply-add: the shader's lineY is `precise`). Every mask pixel must be written by exactly one lane
// (the light's own pixel by none: the clear), for lights inside, at the edge of and far outside the screen, at a full
// and a half-size mask. g++ -std=c++17 -ffp-contract=off -I<tree>/src tests/contactlines/contactlines.cpp
// Reds: -DCONTACTLINES_RED=1 the owner as "the line nearest the centre" by a division (rounds differently per lane:
// holes and doubles), =2 the lines clamped to the sector's diagonal at the anchor (holes on the diagonal near the light).
#include "gl/contactlines.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int fails = 0;

static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

static float lineY( int j, float lineOff, int u, float Da )
{
	volatile float r = float( u ) / Da;
	volatile float m = lineOff * r;
	return ( float( j ) + 0.5f ) - m;
}

// returns the pixels written 0 / 2+ times (the light's own pixel excluded)
static void cover( double lx, double ly, int mw, int mh, int & holes, int & doubles, int & waves )
{
	WwContactBands B = wwContactBands( lx, ly, mw, mh );
#if defined( CONTACTLINES_RED ) && CONTACTLINES_RED == 2
	{	// the old clamp: lines only up to the sector's diagonal at the anchor
		B.waves = 0;
		for ( size_t i = 0; i < B.bands.size(); i++ ) {
			WwContactBand & b = B.bands[i];
			const bool majorX = b.sector < 2;
			const double s = ( b.sector & 1 ) == 0 ? 1.0 : -1.0, Lm = majorX ? lx : ly, Ln = majorX ? ly : lx;
			const double Da = s * ( B.first[b.sector] + s * b.kAnchor + 0.5 - Lm );
			const int smin = int( std::floor( Ln - 0.5 - Da ) ) - 1, smax = int( std::ceil( Ln - 0.5 + Da ) ) + 1;
			const int jmax = b.jMin + B.lines[i] - 1;
			b.jMin = b.jMin > smin ? b.jMin : smin;
			B.lines[i] = ( jmax < smax ? jmax : smax ) - b.jMin + 1;
			b.waveStart = B.waves;
			B.waves += B.lines[i] > 0 ? B.lines[i] : 0;
		}
	}
#endif
	waves = B.waves;
	std::vector<int> cnt( size_t( mw ) * size_t( mh ), 0 );
	const float Lx = float( lx ), Ly = float( ly );
	for ( size_t bi = 0; bi < B.bands.size(); bi++ ) {
		const WwContactBand & b = B.bands[bi];
		const bool majorX = b.sector < 2;
		const float s = ( b.sector & 1 ) == 0 ? 1.0f : -1.0f;
		const float Lm = majorX ? Lx : Ly, Ln = majorX ? Ly : Lx;
		const float anchorC = float( B.first[b.sector] ) + s * float( b.kAnchor ) + 0.5f;
		const float Da = s * ( anchorC - Lm );
		for ( int li = 0; li < B.lines[bi]; li++ ) {
			const int j = b.jMin + li;
			const float lineOff = float( j ) + 0.5f - Ln, lineOffPrev = float( j - 1 ) + 0.5f - Ln;
			for ( int t = 0; t < 64; t++ ) {
				const float cT = anchorC - s * float( t );
				const float dist0 = s * ( cT - Lm );
				const float yT = lineY( j, lineOff, t, Da );
				const int px = majorX ? int( std::floor( cT ) ) : int( std::floor( yT ) );
				const int py = majorX ? int( std::floor( yT ) ) : int( std::floor( cT ) );
				if ( dist0 <= 0.0f || px < 0 || py < 0 || px >= mw || py >= mh )
					continue;
				const float minorOff = std::floor( yT ) + 0.5f - Ln;
				const bool inSector = majorX ? dist0 > std::fabs( minorOff ) : dist0 >= std::fabs( minorOff );
#if defined( CONTACTLINES_RED ) && CONTACTLINES_RED == 1
				const bool own = std::floor( minorOff * Da / dist0 + Ln ) == float( j );
#else
				const bool own = std::floor( lineY( j - 1, lineOffPrev, t, Da ) ) < std::floor( yT );
#endif
				if ( inSector && own )
					cnt[size_t( py ) * size_t( mw ) + size_t( px )]++;
			}
		}
	}
	holes = doubles = 0;
	const int lxi = int( std::floor( lx ) ), lyi = int( std::floor( ly ) );
	for ( int y = 0; y < mh; y++ )
		for ( int x = 0; x < mw; x++ ) {
			const int c = cnt[size_t( y ) * size_t( mw ) + size_t( x )];
			if ( c == 0 && !( x == lxi && y == lyi ) )
				holes++;
			if ( c > 1 )
				doubles++;
		}
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	struct Case { double lx, ly; int mw, mh; const char * what; };
	const Case cases[] = {
		{ 1402.8, 1109.3, 1920, 1080, "sun ahead, just off the top right (full size)" },
		{ 701.4, 554.6, 960, 540, "the same at half size" },
		{ 20669.3, 6764.0, 1920, 1080, "sun far off screen (low, to the side): Ln ~ 7e3" },
		{ 10334.6, 3382.0, 960, 540, "the same at half size" },
		{ 1068.1, 810.1, 1920, 1080, "sun in view, fractional (diagonal pixels)" },
		{ 960.5, 540.5, 1920, 1080, "the light on a pixel centre" },
		{ 480.0, 270.0, 960, 540, "the light on a pixel corner" },
		{ 672.1, -50.2, 960, 540, "sun behind the camera (below the screen)" },
		{ -5000.7, 300.3, 1920, 1080, "far left" },
		{ 100.25, -3.75, 960, 540, "just under the bottom edge" },
		{ -180000.0, 99000.0, 1920, 1080, "near the clip-w limit" },
	};
	int allHoles = 0, allDoubles = 0;
	for ( const Case & c : cases ) {
		int h = 0, d = 0, w = 0;
		cover( c.lx, c.ly, c.mw, c.mh, h, d, w );
		char line[256];
		std::snprintf( line, sizeof( line ), "%s: light %.1f,%.1f mask %dx%d waves %d, holes %d doubles %d", c.what, c.lx, c.ly,
			c.mw, c.mh, w, h, d );
		check( h == 0 && d == 0, line );
		allHoles += h;
		allDoubles += d;
	}
	// a sweep of fractional light positions round a pixel (the owner rule's rounding)
	int sweepBad = 0;
	for ( int i = 0; i < 24; i++ ) {
		int h = 0, d = 0, w = 0;
		cover( 300.0 + i * 0.37, 200.0 + i * 0.29, 640, 360, h, d, w );
		sweepBad += h + d;
	}
	check( sweepBad == 0, "24 fractional light positions on a 640x360 mask: every pixel one lane" );
	std::printf( "CONTACTLINES %s %d failed (holes %d doubles %d)\n", fails ? "FAIL" : "PASS", fails, allHoles, allDoubles );
	return fails ? 1 : 0;
}
