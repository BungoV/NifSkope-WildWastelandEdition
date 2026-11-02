// Lane STREAMTEX3: the PERFFIX4 pool ordered by STREAMTEX1's per-cell counts (src/gl/texpool.h tiers, the rule of
// src/gl/streamtexpool.h). g++ -std=c++17 -O2 -I<tree>/src tests/texpool/tiers_test.cpp
// A walk whose working set is OVER the budget (as the c3 walk is over Ultra 6144 MB): 20 crossings of a 5x5 block,
// 120 textures a cell (half shared with the east neighbour), 300 never-streamed textures (far field, sky) bound every
// paint, and a camera that turns: the south two rows are behind it for the first half of a step, the north two for the
// second half (STREAMTEX1's case: the old rules freed a loaded cell's textures while the camera looked away).
// Budget 2048 MB; the loaded set + far field is 2100 MB.
// The same walk runs twice: tiers on (the default) and every texture tier 0 (WW_CELL_STREAMTEX_RED=off = PERFFIX4).
// Gates: no texture a loaded cell names is ever freed; fewer reloads than the PERFFIX4 order; the tier lookup + pick
// of 2500 entries adds <= 0.05 ms a frame (the pass runs every 2 s; its own frame cost is printed).
// Red: -DTIERS_RED runs the default walk with tiers off too and must FAIL.
#include "gl/texpool.h"
#include "gl/streamtexpool.h"

#include <chrono>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace WwTexPool;
using Pool = StreamTexPool<int>;

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

static int tierOfRule( Pool::Rule r )
{
	return r == Pool::Rule::Pool ? 0 : r == Pool::Rule::Keep ? kTierKeep : 1;
}

struct WalkResult
{
	int frees = 0, reloads = 0, keepFreed = 0, freesT0 = 0, freesT1 = 0;
	std::int64_t endMb = 0, peakMb = 0;
};

static WalkResult walk( bool tiersOn )
{
	WalkResult w;
	Pool pool( 0, 2, !tiersOn );
	struct Tex { bool resident = false, everFreed = false; std::uint32_t useTick = 0; };
	std::map<int, Tex> tex;
	const std::int64_t mb = std::int64_t( 1 ) << 20;
	const std::int64_t budget = 2048 * mb;
	auto cellTex = [&]( int cx, int cy, std::vector<int> & ids ) {
		for ( int k = 0; k < 120; k++ )
			ids.push_back( k < 60 ? ( ( cx + 100 ) * 1000 + cy + 100 ) * 200 + k : ( ( cx + 101 ) * 1000 + cy + 100 ) * 200 + k - 60 );
	};
	std::vector<int> base;
	for ( int k = 0; k < 300; k++ ) {
		base.push_back( 900000000 + k );
		pool.note( Pool::kBase, base.back() );
	}
	std::vector<int> path;
	for ( int r = 0; r < 2; r++ ) {
		for ( int x = 0; x < 5; x++ )
			path.push_back( x );
		for ( int x = 5; x > 0; x-- )
			path.push_back( x );
	}
	std::map<Pool::Cell, bool> loadedCells;
	std::uint32_t tick = 1, paintTick[4] = { 0, 0, 0, 0 };
	int paints = 0;
	std::int64_t total = 0, peak = 0;
	for ( size_t s = 0; s < path.size(); s++ ) {
		pool.cross();
		std::map<Pool::Cell, bool> want;
		for ( int dx = -2; dx <= 2; dx++ )
			for ( int dy = -2; dy <= 2; dy++ )
				want[Pool::cellId( path[s] + dx, dy )] = true;
		for ( auto & kv : loadedCells )
			if ( !want.count( kv.first ) )
				pool.dropCell( kv.first );
		std::vector<std::vector<int>> cellIds;
		for ( int dx = -2; dx <= 2; dx++ )
			for ( int dy = -2; dy <= 2; dy++ ) {
				std::vector<int> ids;
				cellTex( path[s] + dx, dy, ids );
				for ( const int id : ids )
					pool.note( Pool::cellId( path[s] + dx, dy ), id );
				cellIds.push_back( ids );
			}
		loadedCells = want;
		for ( int p = 0; p < 30; p++, tick++ ) {
			auto bind = [&]( int id ) {
				Tex & t = tex[id];
				if ( !t.resident ) {
					t.resident = true;
					total += mb;
					w.reloads += t.everFreed ? 1 : 0;
					pool.loaded( id, std::uint64_t( mb ) );
				}
				t.useTick = tick;
			};
			for ( int c = 0; c < 25; c++ )
				if ( p < 15 ? c % 5 >= 2 : c % 5 <= 2 )	// c % 5 = dy + 2: the rows behind the camera are not bound
					for ( const int id : cellIds[c] )
						bind( id );
			for ( const int id : base )
					bind( id );
			paintTick[paints++ & 3] = tick;
			if ( tick % 2 == 0 ) {	// the view's 2 s pass (GLView::wwCellStreamEvict -> TexCache::evictPool)
				std::vector<Entry> e;
				for ( auto & kv : tex )
					if ( kv.second.resident )
						e.push_back( { kv.first, kv.second.useTick, mb, 0 } );
				const std::uint32_t protectTick = paintTick[paints & 3];
				if ( total > budget && !pool.red() )	// as TexCache::evictPool: never asked of a protected one
					for ( Entry & x : e )
						if ( x.useTick < protectTick )
							x.tier = tierOfRule( pool.rule( x.index ) );
				for ( const int id : pick( e, total, budget, protectTick ) ) {
					// what the counts say of it (asked of a tiers-on pool's rule regardless of the order used)
					const int refs = pool.refs( id );
					w.keepFreed += refs > 0 ? 1 : 0;
					const bool isBase = id >= 900000000;
					w.freesT1 += isBase ? 1 : 0;
					w.freesT0 += !isBase && refs == 0 ? 1 : 0;
					tex[id].resident = false;
					tex[id].everFreed = true;
					total -= mb;
					w.frees++;
					pool.freed( id );
				}
			}
			peak = std::max( peak, total );
		}
	}
	w.endMb = total / mb;
	w.peakMb = peak / mb;
	return w;
}

int main()
{
	// unit checks of the tiered pick()
	{
		// index, useTick, bytes, tier
		std::vector<Entry> e = { { 0, 1, 100, kTierKeep }, { 1, 9, 100, 0 }, { 2, 2, 100, 1 }, { 3, 8, 100, 0 } };
		const std::vector<int> p = pick( e, 400, 250, 20 );
		check( p.size() == 2 && p[0] == 3 && p[1] == 1, "tier 0 goes first, oldest first, though tier 1/keep are older" );
		const std::vector<int> q = pick( e, 400, 100, 20 );
		check( q.size() == 3 && q[2] == 2, "then tier 1; never kTierKeep even far over budget" );
		const std::vector<int> r = pick( e, 400, 100, 9 );
		check( r.size() == 2 && r[0] == 3 && r[1] == 2, "a protected tier-0 entry is skipped, the next tier still frees" );
		std::vector<Entry> f = { { 0, 10, 100 }, { 1, 5, 100 }, { 2, 30, 100 }, { 3, 1, 100 } };
		const std::vector<int> s = pick( f, 400, 300, 25 );
		check( s.size() == 2 && s[0] == 3 && s[1] == 1, "all tiers equal == PERFFIX4's order (the red)" );
	}
#ifdef TIERS_RED
	const WalkResult g = walk( false );
#else
	const WalkResult g = walk( true );
#endif
	const WalkResult o = walk( false );
	std::printf( "walk tiers%s: frees %d (released %d, never-streamed %d, loaded-cell %d), reloads %d, end %lld MB, peak %lld MB\n",
#ifdef TIERS_RED
		" (red: off)",
#else
		"",
#endif
		g.frees, g.freesT0, g.freesT1, g.keepFreed, g.reloads, (long long)g.endMb, (long long)g.peakMb );
	std::printf( "walk PERFFIX4 order: frees %d (released %d, never-streamed %d, loaded-cell %d), reloads %d, end %lld MB, peak %lld MB (budget 2048 MB)\n",
		o.frees, o.freesT0, o.freesT1, o.keepFreed, o.reloads, (long long)o.endMb, (long long)o.peakMb );
	check( g.keepFreed == 0, "no texture a loaded cell names is freed" );
	check( g.reloads < o.reloads, "fewer reloads than the PERFFIX4 order on the same walk" );

	// cost: tier lookup + pick of 2500 entries vs PERFFIX4's pick alone (names ~60 chars, as the cache's paths)
	{
		using SP = StreamTexPool<std::string>;
		SP sp( 0, 2, false );
		const std::int64_t mb = std::int64_t( 1 ) << 20;
		std::vector<std::string> names;
		std::vector<Entry> e;
		for ( int i = 0; i < 2500; i++ ) {
			names.push_back( "textures/architecture/buildings/concord/wall" + std::to_string( i ) + "_d.dds" );
			sp.note( i % 3 ? SP::cellId( i % 25, 0 ) : SP::kBase, names.back() );
			sp.loaded( names.back(), 1 << 20 );
			e.push_back( { i, std::uint32_t( ( i * 7919 ) % 5000 ), mb, 0 } );
		}
		for ( int c = 0; c < 10; c++ )
			sp.dropCell( SP::cellId( c, 0 ) );
		const int runs = 400;
		volatile std::size_t sink = 0;
		auto t0 = std::chrono::steady_clock::now();
		for ( int r = 0; r < runs; r++ )
			sink = sink + pick( e, 2500 * mb, 2048 * mb, 4000 ).size();
		auto t1 = std::chrono::steady_clock::now();
		for ( int r = 0; r < runs; r++ ) {
			std::vector<Entry> f = e;
			for ( Entry & x : f ) {
				if ( x.useTick >= 4000 )
					continue;
				const SP::Rule ru = sp.rule( names[x.index] );
				x.tier = ru == SP::Rule::Pool ? 0 : ru == SP::Rule::Keep ? kTierKeep : 1;
			}
			sink = sink + pick( f, 2500 * mb, 2048 * mb, 4000 ).size();
		}
		auto t2 = std::chrono::steady_clock::now();
		const double a = std::chrono::duration<double, std::milli>( t1 - t0 ).count() / runs;
		const double b = std::chrono::duration<double, std::milli>( t2 - t1 ).count() / runs;
		// the pass runs every 2 s and only over budget: at 60 fps its cost spreads over 120 frames
		std::printf( "cost: PERFFIX4 pick %.4f ms, tiers + pick %.4f ms, extra %.4f ms on the pass's frame, %.5f ms a frame at 60 fps (2500 entries, 80%% unprotected)\n",
			a, b, b - a, ( b - a ) / 120.0 );
		check( ( b - a ) / 120.0 <= 0.05, "the tier lookup adds <= 0.05 ms a frame (the walk's frame-time bar)" );
	}
	std::printf( "TIERS %s (%d checks, %d failures)\n", fails ? "FAIL" : "PASS", checks, fails );
	return fails ? 1 : 0;
}
