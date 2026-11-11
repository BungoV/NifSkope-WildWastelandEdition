// Lane AIRPERF1 round 4: unit test of src/cellstreamrule.h and src/features/gi/rttracesetrule.h (pure rules, no Qt).
// Each red (-D...) must fail its own check: tests/cellstreamrule/run.sh.
#include "cellstreamrule.h"
#include "features/gi/rttracesetrule.h"
#include "features/gi/rtddgimath.h"

#include <algorithm>
#include <cstdio>

static int fails = 0;

static void check( bool ok, const char * name, const char * what )
{
	std::printf( "%s %s: %s\n", ok ? "ok  " : "FAIL", name, what );
	if ( !ok )
		fails++;
}

// the faded edge of a camera-locked cascade (rtddgimath.h cascadeEdge, tight); the red = no fade (a hard edge)
static float edgeFade( float g, int dims )
{
#ifdef TRACESET_RED_NOFADE
	return g >= 0.0f && g <= float( dims - 1 ) ? 1.0f : 0.0f;
#else
	return rtddgi::cascadeEdge( g, dims, true );
#endif
}

static std::vector<rttracesetrule::Unit> block( int cx, int cy, int half )
{
	std::vector<rttracesetrule::Unit> u;
	for ( int y = cy - half; y <= cy + half; y++ )
		for ( int x = cx - half; x <= cx + half; x++ ) {
			rttracesetrule::Unit a;
			a.x = x;
			a.y = y;
			a.tris = 1000u + unsigned( ( x * 31 + y * 17 ) & 255 );
			u.push_back( a );
		}
	return u;
}

int main()
{
	using namespace cellstreamrule;
	// u1: the open rule
	{
		bool ok = openOn( openRule( false, 0, true, false, false ) );    // a spec open, no pin: streams
		ok = ok && openOn( openRule( false, 0, false, false, false ) );   // the menu's open
		ok = ok && !openOn( openRule( true, 0, true, false, false ) );    // the explicit fixed block
		ok = ok && openOn( openRule( true, 1, true, false, false ) );
		ok = ok && !openOn( openRule( false, 0, true, true, false ) );    // the census walk welds
		check( ok, "u1 open", "a spec open streams; WW_CELL_STREAM=0 is the fixed block; 1 pins on" );
		check( !openOn( openRule( false, 0, true, false, true ) ) && openOn( openRule( false, 0, false, false, true ) ),
			"u1 red word", "WW_CELL_STREAM_RED=specfixed: a spec open is fixed, the menu's open streams" );
	}
	// bar (i) + (iv): the centre follows a 3-cell flight; held, it stays; released, it picks up from the camera's cell
	const int sx = -15, sy = 17;
	const double shift = 2048.0;
	{
		int cx = sx, cy = sy;
		double ex = ( sx + 0.5 ) * 4096.0, ey = ( sy + 0.5 ) * 4096.0;
		int worst = 0;
		for ( int i = 0; i < 3 * 64; i++ ) {
			ex += 4096.0 / 64.0;
			centreFor( ex, ey, shift, false, cx, cy );
			worst = std::max( worst, std::abs( cellOf( ex ) - cx ) );
		}
		std::printf( "flight: eye cell %d,%d centre %d,%d worst lag %d cell\n", cellOf( ex ), cellOf( ey ), cx, cy, worst );
		check( cx == cellOf( ex ) && cy == cellOf( ey ) && cx == sx + 3, "u1 follow",
			"after 3 cells the block's centre is the camera's cell" );
		check( worst <= 1, "u1 lag", "on the way the centre is never more than one cell behind (the half-cell hysteresis)" );
		// the row off: the same flight back, the centre stays
		int hx = cx, hy = cy;
		double bx = ex;
		for ( int i = 0; i < 3 * 64; i++ ) {
			bx -= 4096.0 / 64.0;
			if ( liveOn( false, 0, false, false ) )
				centreFor( bx, ey, shift, false, hx, hy );
			else
				centreFor( bx, ey, shift, true, hx, hy );
		}
		check( hx == cx && hy == cy, "u1 hold", "row off: the centre does not move while the camera flies 3 cells" );
		// the row on again: one tick picks up from the camera's cell
		if ( liveOn( false, 0, true, false ) )
			centreFor( bx, ey, shift, false, hx, hy );
		check( hx == cellOf( bx ) && hy == cellOf( ey ), "u1 resume", "row on: the centre is the camera's cell again" );
		check( !liveOn( true, 0, true, false ) && liveOn( true, 1, false, false ) && liveOn( false, 0, true, false )
			&& !liveOn( false, 0, false, false ) && liveOn( false, 0, false, true ),
			"u1 live", "the pin wins over the row; the row decides without it; the row red holds nothing" );
	}
	// u2: the traced set's key
	{
		using namespace rttracesetrule;
		const std::vector<Unit> a = block( sx, sy, 2 ), a2 = block( sx, sy, 2 ), b = block( sx + 1, sy, 2 );
		std::vector<Unit> c = a;
		c[3].tris += 9;
		const bool same = setKey( a, 8, 100, 5000 ) == setKey( a2, 8, 100, 5000 );
		const bool moved = setKey( a, 8, 100, 5000 ) != setKey( b, 8, 100, 5000 )
			&& setKey( a, 8, 100, 5000 ) != setKey( c, 8, 100, 5000 )
			&& setKey( a, 8, 100, 5000 ) != setKey( a, 9, 100, 5000 )
			&& setKey( a, 8, 100, 5000 ) != setKey( a, 8, 101, 5000 )
			&& setKey( a, 8, 100, 5000 ) != setKey( a, 8, 100, 5001 );
		check( same && moved, "u2 key", "the same loaded set keeps its key; another cell, triangle count, ring or far part changes it" );
		check( cameraIn( b, sx + 1, sy ) && cameraIn( b, sx + 3, sy ) && !cameraIn( b, sx + 4, sy ) && !cameraIn( a, sx + 3, sy ),
			"u2 camera", "the camera's cell is found among the set's cells, and only there" );
		bool redParked = false;
#ifdef TRACESET_RED_PARKED
		redParked = true;
#endif
		const bool pub = publishNow( true, false, false, 0.0, kMinGapMs, redParked )        // the first set at once
			&& !publishNow( true, true, true, 9000.0, kMinGapMs, redParked )                // one merge at a time
			&& !publishNow( false, false, true, 9000.0, kMinGapMs, redParked )              // unchanged: nothing
			&& !publishNow( true, false, true, 200.0, kMinGapMs, redParked );               // inside the gap: waits
		check( pub && publishNow( true, false, true, 1600.0, kMinGapMs, redParked ), "u2 publish",
			"a changed set is published once the gap has passed (the GI follows); never two builds inside the gap" );
		check( farRing( 0.0, 2 ) == 5 && farRing( 8192.0, 2 ) == 5 && farRing( 32768.0, 2 ) == 11 && farRing( 1e6, 2 ) == kMaxRing
			&& farRing( 100.0, 6 ) == 7, "u2 ring", "the far ring covers the coarsest cascade's reach, capped, never inside the block" );
	}
	// u3: the set's box (what the cascades' window is clamped to) after the flight
	{
		using namespace rttracesetrule;
		int cx = sx, cy = sy;
		double ex = ( sx + 0.5 ) * 4096.0, ey = ( sy + 0.5 ) * 4096.0;
		double lo[2], hi[2];
		setBox( cx, cy, 2, lo, hi );
		const bool first = boxHolds( lo, hi, ex, ey );
		for ( int i = 0; i < 3 * 64; i++ ) {
			ex += 4096.0 / 64.0;
			centreFor( ex, ey, shift, false, cx, cy );
		}
#ifndef TRACESET_RED_FIRSTBOX
		setBox( cx, cy, 2, lo, hi );   // the red keeps the first set's box (the soup built once)
#endif
		const double mid = 0.5 * ( lo[0] + hi[0] );
		std::printf( "box x %.0f .. %.0f, camera x %.0f, centre off by %.0f units\n", lo[0], hi[0], ex, ex - mid );
		check( first && boxHolds( lo, hi, ex, ey ) && std::fabs( ex - mid ) <= 2048.0 + 1e-6, "u3 box",
			"after 3 cells the set's box holds the camera, centred within half a cell" );
	}
	// u4: the faded edge where a cascade's coverage ends
	{
		const int dims = 24;
		bool mono = true;
		float prev = 0.0f;
		for ( int i = 0; i <= 120; i++ ) {
			const float f = edgeFade( float( i ) * 0.1f, dims );
			mono = mono && f + 1e-6f >= prev;
			prev = f;
		}
		const bool ends = edgeFade( 0.0f, dims ) == 0.0f && edgeFade( float( dims - 1 ), dims ) == 0.0f
			&& edgeFade( 1.0f, dims ) == 0.0f && edgeFade( 1.5f, dims ) > 0.4f && edgeFade( 1.5f, dims ) < 0.6f
			&& edgeFade( 2.0f, dims ) == 1.0f && edgeFade( 11.5f, dims ) == 1.0f;
		check( mono && ends, "u4 fade", "0 at the box edge and over the outer cell, 0 to 1 over the next, 1 inside, monotonic" );
	}
	// ---- round 5
	// u5: the far objects' budget, the traced rows, what a loaded cell traces already
	{
		using namespace rttracesetrule;
		std::vector<FarCell> c( 5 );
		c[0].x = 9; c[0].y = 0; c[0].tris = 50;    // 4 out
		c[1].x = 6; c[1].y = 0; c[1].tris = 100;   // 1 out
		c[2].x = 5; c[2].y = 3; c[2].tris = 400;   // 3 out
		c[3].x = 5; c[3].y = 2; c[3].tris = 200;   // 2 out
		c[4].x = 5; c[4].y = 1; c[4].tris = 300;   // 1 out, y after cell 1
		std::uint64_t dropped = 0;
		const size_t keep = farCap( c, 5, 0, 700, &dropped );   // 100 + 300 + 200 = 600 fits, + 400 does not
		std::printf( "farcap keep %d dropped %d, order %d,%d %d,%d %d,%d\n", int( keep ), int( dropped ), c[0].x, c[0].y, c[1].x,
			c[1].y, c[2].x, c[2].y );
		check( keep == 3 && dropped == 450 && c[0].x == 6 && c[1].y == 1 && c[2].y == 2 && c[3].y == 3 && c[4].x == 9, "u5 farcap",
			"nearest cells first, kept while the sum fits, the first that does not fit ends the list (the 50 past it is out too)" );
		check( rowTraced( 0 ) && rowTraced( 0x80 ) && rowTraced( 0x83 ) && !rowTraced( 1 ) && !rowTraced( 2 ) && !rowTraced( 3 ),
			"u5 rows", "the ring's own row is traced, a tree ladder's other steps are not" );
		check( farSkip( 7, true, false ) && !farSkip( 7, false, true ) && !farSkip( 7, false, false ) && farSkip( 0, false, true )
			&& !farSkip( 0, true, false ), "u5 skip", "a placed object by its ref being listed, geometry without a ref by its cell" );
	}
	// u6: the hand-over's warm bar
	{
		using namespace rttracesetrule;
		std::printf( "warm updates 0.97 -> %d, 0.9 -> %d, 0 -> %d\n", warmUpdates( 0.97 ), warmUpdates( 0.9 ), warmUpdates( 0.0 ) );
		check( warmUpdates( 0.97 ) == 33 && warmUpdates( 0.9 ) == 9 && warmUpdates( 0.0 ) == 1, "u6 warm",
			"the smallest count with age / (age + 1) >= the hysteresis" );
	}
	// u7: a drawn cell owes its soup; the saved row
	{
		using namespace cellstreamrule;
		check( soupOwed( true, true, false, false ) && !soupOwed( false, true, false, false ) && !soupOwed( true, false, false, false )
			&& !soupOwed( true, true, true, false ) && !soupOwed( true, true, false, true ), "u7 owed",
			"follows, drawn, no soup, not asked yet" );
		check( rowStart( false, false ) && rowStart( false, true ) && rowStart( true, true ) && !rowStart( true, false ), "u7 row",
			"nothing saved: on; else the saved value" );
	}
	// u8: the soup's frame is the world's (a cell of the opened block vs the same cell streamed in under another origin)
	{
		using namespace rttracesetrule;
		const double oa = 4096.0 * -14.5, ob = 4096.0 * 9.5;   // two documents' origins
		bool same = true;
		for ( int cell = -20; cell <= 20; cell += 5 ) {
			const double a = soupCellOrigin( cell, oa ), b = soupCellOrigin( cell, ob ), w = double( cell ) * 4096.0;
			same = same && a - b == 0.0 && a - w == 0.0 && docWorld( w - oa, oa ) - a == 0.0 && docWorld( w - ob, ob ) - b == 0.0;
		}
		check( same, "u8 frame", "a cell's land starts at cell x 4096 under any origin, and an origin-relative far vertex meets it" );
	}
	std::printf( fails ? "CELLSTREAMRULE FAIL %d\n" : "CELLSTREAMRULE PASS %d failed\n", fails );
	return fails ? 1 : 0;
}
