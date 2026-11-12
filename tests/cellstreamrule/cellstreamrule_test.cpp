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
	// u9 (round 6): far water is a source, the endless sea is not; a ladder ring row stands in for a tree card
	{
		using namespace rttracesetrule;
		check( farSrcOf( "FarLOD water v 4 x #2" ) == 2 && farSrcOf( "FarLOD_water 3" ) == 2 && farSrcOf( "FarLOD water" ) == 2
			&& farSrcOf( "FarLOD sea" ) == -1 && farSrcOf( "FarLOD terrain v 4 x #0" ) == 0 && farSrcOf( "FarLOD_terrain" ) == 0
			&& farSrcOf( "FarLOD v 4 obj x #1" ) == 1 && farSrcOf( "Land -3 4" ) == -1 && farSrcOf( nullptr ) == -1, "u9 src",
			"water 2, terrain 0, objects 1, the sea and anything else not traced" );
		check( rowCard( 0x80 ) && rowCard( 0x86 ) && !rowCard( 0 ) && !rowCard( 2 ) && !rowCard( 0x7F ), "u9 card",
			"the ladder's ring row (0x80) is the card's stand-in; plain rows and the other steps are not" );
	}
	// u10 (round 6): a BLAS a part gives the hits of the single soup
	{
		using namespace rttracesetrule;
		// the parts' plan: ascending, first 0, no empty part, capped
		const std::vector<std::uint32_t> raw = { 0, 0, 100, 250, 250, 251, 480, 900 };
		const std::vector<std::uint32_t> pp = blasParts( raw, 600, kBlasPartCap );
		bool planOk = pp.size() == 5 && pp[0] == 0 && pp[1] == 100 && pp[2] == 250 && pp[3] == 251 && pp[4] == 480;
		std::vector<std::uint32_t> many;
		for ( std::uint32_t i = 0; i < 1000; i++ )
			many.push_back( i * 3 );
		const std::vector<std::uint32_t> capd = blasParts( many, 3000, 256 );
		planOk = planOk && !capd.empty() && capd.size() <= 256 && capd[0] == 0 && blasParts( { 0 }, 600, 256 ).empty()
			&& blasParts( {}, 600, 256 ).empty() && !blasParts( { 7 }, 600, 256 ).empty() && blasParts( { 7 }, 600, 256 )[0] == 0;
		for ( size_t i = 1; i < capd.size(); i++ )
			planOk = planOk && capd[i] > capd[i - 1];
		check( planOk, "u10 parts", "the parts cover the soup from triangle 0, ascending, none empty, at most the cap" );

		// a fixed soup: 600 triangles, about a fifth alpha-tested
		std::uint32_t seed = 12345u;
		const auto rnd = [&seed]() {
			seed = seed * 1664525u + 1013904223u;
			return float( seed >> 8 ) / 16777216.0f;
		};
		const std::uint32_t nTri = 600;
		std::vector<float> soup( nTri * 9 );
		std::vector<char> isMasked( nTri );
		for ( std::uint32_t t = 0; t < nTri; t++ ) {
			const float c[3] = { rnd() * 100.0f, rnd() * 100.0f, rnd() * 100.0f };
			for ( int k = 0; k < 9; k++ )
				soup[t * 9 + std::uint32_t( k )] = c[k % 3] + ( rnd() - 0.5f ) * 24.0f;
			isMasked[t] = rnd() < 0.2f;
		}
		// the tables as bakert builds them: opaque soup triangles, then the alpha-tested ones
		std::vector<std::uint32_t> remap;
		for ( std::uint32_t t = 0; t < nTri; t++ )
			if ( !isMasked[t] )
				remap.push_back( t );
		const std::uint32_t nOpaque = std::uint32_t( remap.size() );
		for ( std::uint32_t t = 0; t < nTri; t++ )
			if ( isMasked[t] )
				remap.push_back( t );
		const auto masked = [&isMasked]( std::uint32_t t ) { return isMasked[t] != 0; };
		const std::vector<BlasPart> plan = blasPlan( pp, nTri, masked );
		// every (BLAS, primitive) names a row of its own, and every row is named
		std::vector<int> seen( nTri, 0 );
		bool rowsOk = true;
		for ( const BlasPart & b : plan )
			for ( std::uint32_t i = 0; i < b.count; i++ ) {
				const std::uint32_t r = blasRow( b.row, 0, i, nOpaque );
				rowsOk = rowsOk && r < nTri && ( r >= nOpaque ) == b.alpha;
				if ( r < nTri )
					seen[r]++;
			}
		for ( std::uint32_t r = 0; r < nTri; r++ )
			rowsOk = rowsOk && seen[r] == 1;
		// the single soup under the same rule (custom index 0) is the old formula
		for ( std::uint32_t i = 0; i < 50; i++ )
			rowsOk = rowsOk && blasRow( 0, 0, i, nOpaque ) == i && blasRow( 0, 1, i, nOpaque ) == i + nOpaque;
		// the nearest hit (ties: the lower soup triangle), through the parts and through the single soup
		const auto hitRow = [&]( std::uint32_t row, const double o[3], const double d[3], double & t ) {
			const float * v = &soup[size_t( remap[row] ) * 9];
			double e1[3], e2[3], p[3], s[3], q[3];
			for ( int k = 0; k < 3; k++ ) {
				e1[k] = double( v[3 + k] ) - double( v[k] );
				e2[k] = double( v[6 + k] ) - double( v[k] );
				s[k] = o[k] - double( v[k] );
			}
			p[0] = d[1] * e2[2] - d[2] * e2[1];
			p[1] = d[2] * e2[0] - d[0] * e2[2];
			p[2] = d[0] * e2[1] - d[1] * e2[0];
			const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
			if ( std::fabs( det ) < 1e-12 )
				return false;
			const double u = ( s[0] * p[0] + s[1] * p[1] + s[2] * p[2] ) / det;
			q[0] = s[1] * e1[2] - s[2] * e1[1];
			q[1] = s[2] * e1[0] - s[0] * e1[2];
			q[2] = s[0] * e1[1] - s[1] * e1[0];
			const double w = ( d[0] * q[0] + d[1] * q[1] + d[2] * q[2] ) / det;
			t = ( e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2] ) / det;
			return u >= 0.0 && w >= 0.0 && u + w <= 1.0 && t > 1e-6;
		};
		int hits = 0, differ = 0;
		for ( int r = 0; r < 4096; r++ ) {
			const double o[3] = { double( rnd() ) * 100.0, double( rnd() ) * 100.0, double( rnd() ) * 100.0 };
			double d[3] = { double( rnd() ) - 0.5, double( rnd() ) - 0.5, double( rnd() ) - 0.5 };
			const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] ) + 1e-9;
			for ( int k = 0; k < 3; k++ )
				d[k] /= len;
			double bestA = 1e300, bestB = 1e300;
			long long triA = -1, triB = -1;
			const auto take = [&]( std::uint32_t row, double & best, long long & tri ) {
				double t = 0.0;
				if ( row >= nTri || !hitRow( row, o, d, t ) )
					return;
				if ( t < best || ( t == best && ( long long )( remap[row] ) < tri ) ) {
					best = t;
					tri = ( long long )( remap[row] );
				}
			};
			for ( const BlasPart & b : plan )   // the TLAS of parts: one instance a BLAS, one geometry each
				for ( std::uint32_t i = 0; i < b.count; i++ )
					take( blasRow( b.row, 0, i, nOpaque ), bestA, triA );
			for ( std::uint32_t i = 0; i < nOpaque; i++ )   // the single soup: geometry 0, then geometry 1
				take( blasRow( 0, 0, i, nOpaque ), bestB, triB );
			for ( std::uint32_t i = 0; i < nTri - nOpaque; i++ )
				take( blasRow( 0, 1, i, nOpaque ), bestB, triB );
			hits += triB >= 0 ? 1 : 0;
			differ += ( triA != triB || bestA != bestB ) ? 1 : 0;
		}
		std::printf( "blas parts %d, BLASes %d (opaque rows %u of %u), rays 4096: hits %d, differ %d\n", int( pp.size() ), int( plan.size() ),
			nOpaque, nTri, hits, differ );
		check( rowsOk && hits > 400 && differ == 0, "u10 rows",
			"each (BLAS, primitive) is one table row, all rows named once, and 4096 rays hit the same triangle at the same t" );

		// the local origin holds still while the box's middle stays in one grid square; the single soup keeps the old rule
		const bool org = stableOrigin( -1000.0, 9000.0, true ) == stableOrigin( 3000.0, 21000.0, true )
			&& stableOrigin( -1000.0, 9000.0, true ) == 0.0 && stableOrigin( 30000.0, 40000.0, true ) == 32768.0
			&& stableOrigin( -1000.5, 9000.0, false ) == 3999.0;
		check( org, "u10 origin", "with parts the origin snaps to 32768; without, the floor of the middle" );
		bool grow = growQuant( 0.0 ) == 0.0;
		for ( double del = 0.07; del < 0.12; del += 0.01 )
			grow = grow && growQuant( del ) == 0.125 && growQuant( del ) >= del;
		const std::uint64_t ka = bytesKey( soup.data(), 3600, 1 );
		soup[10] += 0.25f;
		grow = grow && growQuant( 0.13 ) == 0.25 && ka != bytesKey( soup.data(), 3600, 1 ) && ka != bytesKey( soup.data(), 3600, 2 );
		check( grow, "u10 grow", "the grow bound is the next power of two (one key an octave), and a part's key follows its bytes" );
	}
	std::printf( fails ? "CELLSTREAMRULE FAIL %d\n" : "CELLSTREAMRULE PASS %d failed\n", fails );
	return fails ? 1 : 0;
}
