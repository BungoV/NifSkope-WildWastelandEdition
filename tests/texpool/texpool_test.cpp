// Lane PERFFIX4: the streaming texture pool (src/gl/texpool.h). g++ -std=c++17 -I<tree>/src tests/texpool/texpool_test.cpp
// It replays a 20-crossing walk: each step a 5x5 block of cells, 120 textures a cell, half shared with a neighbour;
// textures leave view when their cell leaves the block and come back on a crossing back. It counts frees and reloads
// (a reload = a texture freed earlier and bound again: the driver-pool churn) and the resident peak.
// Red: -DTEXPOOL_RED replays the old rule (free whatever no paint bound for 5 s) and must FAIL the churn gate.
#include "gl/texpool.h"

#include <cstdio>
#include <map>
#include <set>
#include <vector>

using namespace WwTexPool;

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

struct Tex
{
	bool resident = false, everFreed = false;
	std::uint32_t useTick = 0;
	std::int64_t bytes = 0;
};

int main()
{
	// unit checks of pick()
	{
		std::vector<Entry> e = { { 0, 10, 100 }, { 1, 5, 100 }, { 2, 30, 100 }, { 3, 1, 100 } };
		check( pick( e, 400, 500, 25 ).empty(), "under budget: nothing freed" );
		const std::vector<int> p = pick( e, 400, 300, 25 );
		check( p.size() == 2 && p[0] == 3 && p[1] == 1, "over budget: the two oldest go, to <= 90 %" );
		const std::vector<int> q = pick( e, 400, 100, 5 );
		check( q.size() == 1 && q[0] == 3, "never one bound at or after the protect tick" );
		check( budgetBytes( 3 ) == ( std::int64_t( 6144 ) << 20 ) && budgetBytes( 700 ) == ( std::int64_t( 700 ) << 20 ),
			"budget: Ultra 6144 MB, an explicit MB count" );
	}
	// the walk: 9 cells east then back again, twice (20 crossings); one tick = one second, 30 paints a step
	std::map<int, Tex> tex;	// texture id -> state
	auto cellTex = [&]( int cx, int cy, std::vector<int> & ids ) {
		for ( int k = 0; k < 120; k++ ) {
			// half shared with the cell to the east (same id), so neighbours overlap
			const int id = k < 60 ? ( ( cx * 1000 + cy ) * 200 + k ) : ( ( ( cx + 1 ) * 1000 + cy ) * 200 + k - 60 );
			ids.push_back( id );
		}
	};
	std::uint32_t tick = 1;
	int frees = 0, reloads = 0, loads = 0;
	std::int64_t total = 0, peak = 0, step0 = -1;
	const std::int64_t texBytes = std::int64_t( 1 ) << 20;	// 1 MB each: a 5x5 block ~ 2.4 GB
	const std::int64_t budget = budgetBytes( 3 );
	std::vector<int> path;
	for ( int r = 0; r < 2; r++ ) {
		for ( int x = 0; x < 5; x++ )
			path.push_back( x );
		for ( int x = 5; x > 0; x-- )
			path.push_back( x );
	}
	std::uint32_t paintTick[4] = { 0, 0, 0, 0 };
	int paints = 0;
	for ( size_t s = 0; s < path.size(); s++ ) {
		std::vector<int> want;
		for ( int dx = -2; dx <= 2; dx++ )
			for ( int dy = -2; dy <= 2; dy++ )
				cellTex( path[s] + dx, dy, want );
		for ( int p = 0; p < 30; p++, tick++ ) {
			for ( const int id : want ) {
				Tex & t = tex[id];
				if ( !t.resident ) {
					t.resident = true;
					t.bytes = texBytes;
					total += texBytes;
					loads++;
					reloads += t.everFreed ? 1 : 0;
				}
				t.useTick = tick;
			}
			paintTick[paints++ & 3] = tick;
			const std::uint32_t threePaintsAgo = paintTick[paints & 3];
#ifdef TEXPOOL_RED
			// the old rule: every 2 s, free what no paint bound for 5 s (and not by the last 3 paints)
			if ( tick % 2 == 0 && tick > 5 ) {
				const std::uint32_t before = std::min( tick - 5, threePaintsAgo );
				for ( auto & kv : tex )
					if ( kv.second.resident && kv.second.useTick < before ) {
						kv.second.resident = false;
						kv.second.everFreed = true;
						total -= kv.second.bytes;
						frees++;
					}
			}
#else
			if ( tick % 2 == 0 ) {
				std::vector<Entry> e;
				for ( auto & kv : tex )
					if ( kv.second.resident )
						e.push_back( { kv.first, kv.second.useTick, kv.second.bytes } );
				for ( const int id : pick( e, total, budget, threePaintsAgo ) ) {
					Tex & t = tex[id];
					t.resident = false;
					t.everFreed = true;
					total -= t.bytes;
					frees++;
				}
			}
#endif
			peak = std::max( peak, total );
		}
		if ( s == 0 )
			step0 = total;
	}
	std::printf( "walk: %zu steps, loads %d, frees %d, reloads %d, resident step0 %lld MB, end %lld MB, peak %lld MB, budget %lld MB\n",
		path.size(), loads, frees, reloads, (long long)( step0 >> 20 ), (long long)( total >> 20 ), (long long)( peak >> 20 ),
		(long long)( budget >> 20 ) );
	check( peak <= budget + 25 * 120 * texBytes, "the resident peak stays within the budget plus one block's working set" );
	check( reloads == 0, "no texture freed and read again on the walk (it fits the Ultra pool)" );
	std::printf( "TEXPOOL %s (%d checks, %d failures)\n", fails ? "FAIL" : "PASS", checks, fails );
	return fails ? 1 : 0;
}
