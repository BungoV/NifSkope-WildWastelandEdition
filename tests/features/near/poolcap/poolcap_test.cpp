// lane STREAMTEX5: the bindless geometry pool's budget (WwBlPool::overBudget, src/features/near/cellbindlesspool.h) over a stream
// walk, with no GL. A twin of cellbindless.cpp poolBind's order: alloc, else (budget check over every class, else
// rebuild), a fence per frame 2 frames late, a shrink check every 64 frames. A refused shape draws unpooled.
//   poolcap_test <density> <budgetMB>   density 1 = a Concord-like 5x5 (~0.4 GB live), 3 = a downtown one (~1.2 GB)
//   prints the peak pool capacity, live, refused placements and the pooled share; CAP PASS when the peak stays
//   within the budget (budget <= 0, the red WW_BINDLESS_RED=poolnocap, has no bound: it prints CAP FAIL when the
//   peak passes the default 1536 MB).
// rc: 0 pass, 1 fail, 2 usage.
#include "features/near/cellbindlesspool.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <random>
#include <vector>

namespace
{
struct ShapeT
{
	std::uint32_t id = 0, verts = 0, idx = 0;
	int cls = 0;
};

const std::uint32_t kFloats[2] = { 12, 16 };
const double kDefaultBudget = 1536.0;

std::vector<ShapeT> cellShapes( int cx, int cy, int density )
{
	std::mt19937 rng( std::uint32_t( ( cx + 1000 ) * 7919 + ( cy + 1000 ) * 104729 ) );
	std::vector<ShapeT> v( std::size_t( density ) * ( 40 + rng() % 41 ) );
	std::uint32_t k = 0;
	for ( ShapeT & s : v ) {
		s.id = std::uint32_t( ( ( cx + 512 ) * 1024 + ( cy + 512 ) ) * 1024 + k++ );
		const std::uint32_t r = rng() % 100;
		s.verts = r < 70 ? 30 + rng() % 2000 : 2000 + rng() % 20000;
		s.idx = s.verts * ( 3 + rng() % 4 ) / 2;
		s.cls = int( rng() % 2 );
	}
	return v;
}

struct Entry
{
	WwBlPool::Span span;
	int cls = 0;
};

double capBytes( const WwBlPool::Arena & a, int c )
{
	return double( a.vertCap ) * kFloats[c] * 4.0 + double( a.idxCap ) * 2.0;
}
double liveBytes( const WwBlPool::Arena & a, int c )
{
	return double( a.liveVerts() ) * kFloats[c] * 4.0 + double( a.liveIdx() ) * 2.0;
}
}

int main( int argc, char ** argv )
{
	if ( argc < 3 ) {
		std::printf( "usage: poolcap_test <density> <budgetMB>\n" );
		return 2;
	}
	const int density = std::atoi( argv[1] );
	const double budget = std::atof( argv[2] );
	WwBlPool::Arena a[2];
	std::map<std::uint32_t, Entry> entries;
	std::map<std::pair<int, int>, std::vector<ShapeT>> cache;
	std::uint64_t frameNo = 1;
	std::vector<std::uint64_t> fenced;
	long long rebuilds = 0, shrinks = 0, refused = 0, draws = 0, pooled = 0;
	double peakCap = 0.0, peakLive = 0.0;

	auto total = [&]() { return capBytes( a[0], 0 ) + capBytes( a[1], 1 ); };
	auto rebuild = [&]( int c, std::uint32_t v, std::uint32_t i ) {
		std::uint32_t nv = 0, ni = 0;
		a[c].plan( v, i, nv, ni );
		std::vector<Entry *> live;
		for ( auto & e : entries )
			if ( e.second.cls == c )
				live.push_back( &e.second );
		std::sort( live.begin(), live.end(), []( const Entry * x, const Entry * y ) { return x->span.firstVert < y->span.firstVert; } );
		std::vector<WwBlPool::Span> spans;
		for ( Entry * e : live )
			spans.push_back( e->span );
		WwBlPool::pack( spans, a[c], nv, ni );
		for ( std::size_t j = 0; j < live.size(); j++ )
			live[j]->span = spans[j];
		rebuilds++;
	};
	auto draw = [&]( const ShapeT & s ) {
		draws++;
		auto it = entries.find( s.id );
		if ( it != entries.end() ) {
			pooled++;
			return;
		}
		WwBlPool::Span sp;
		if ( !a[s.cls].alloc( s.verts, s.idx, sp ) ) {
			const double other = total() - capBytes( a[s.cls], s.cls );
			if ( WwBlPool::overBudget( other, a[s.cls], s.verts, s.idx, kFloats[s.cls], budget ) ) {
				refused++;	// cellbindless.cpp: poolBind returns false, the shape's own vertex array draws it
				return;
			}
			rebuild( s.cls, s.verts, s.idx );
			if ( !a[s.cls].alloc( s.verts, s.idx, sp ) ) {
				std::printf( "CAP FAIL: no room after a rebuild\n" );
				std::exit( 1 );
			}
		}
		entries[s.id] = Entry{ sp, s.cls };
		pooled++;
	};

	const int kSteps = 22;
	for ( int step = 0; step < kSteps; step++ ) {
		const int cx = step, cy = step / 3;	// the window walks east, drifting north
		std::vector<std::pair<int, int>> want;
		for ( int y = cy - 2; y <= cy + 2; y++ )
			for ( int x = cx - 2; x <= cx + 2; x++ )
				want.push_back( { x, y } );
		// cells that left the window: their shapes' spans go to graves stamped with this frame
		for ( auto it = cache.begin(); it != cache.end(); ) {
			if ( std::find( want.begin(), want.end(), it->first ) == want.end() ) {
				for ( const ShapeT & s : it->second ) {
					auto e = entries.find( s.id );
					if ( e != entries.end() ) {
						a[e->second.cls].release( e->second.span, frameNo );
						entries.erase( e );
					}
				}
				it = cache.erase( it );
			} else {
				++it;
			}
		}
		for ( auto & w : want )
			if ( !cache.count( w ) )
				cache[w] = cellShapes( w.first, w.second, density );
		for ( int paint = 0; paint < 12; paint++, frameNo++ ) {
			for ( auto & kv : cache )
				for ( const ShapeT & s : kv.second )
					draw( s );
			fenced.push_back( frameNo );
			if ( frameNo >= 2 )
				for ( int c = 0; c < 2; c++ )
					a[c].retire( frameNo - 2 );
			if ( frameNo % 64 == 0 )
				for ( int c = 0; c < 2; c++ )
					if ( a[c].shrinkWanted() ) {
						rebuild( c, 0, 0 );
						shrinks++;
					}
			peakCap = std::max( peakCap, total() );
			peakLive = std::max( peakLive, liveBytes( a[0], 0 ) + liveBytes( a[1], 1 ) );
		}
	}
	const double M = 1048576.0;
	const double bound = budget > 0.0 ? budget : kDefaultBudget;
	const bool pass = peakCap / M <= bound;
	std::printf( "density %d budget %.0f MB%s: peak pool %.1f MB, peak live %.1f MB, rebuilds %lld, shrinks %lld, "
		"refused %lld of %lld draws (pooled %.1f%%)\n", density, budget, budget > 0.0 ? "" : " (RED poolnocap, judged at 1536)",
		peakCap / M, peakLive / M, rebuilds, shrinks, refused, draws, draws ? 100.0 * pooled / draws : 0.0 );
	std::printf( pass ? "CAP PASS\n" : "CAP FAIL\n" );
	return pass ? 0 : 1;
}
