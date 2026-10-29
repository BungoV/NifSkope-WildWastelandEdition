// lane STREAMTEX1: the stream texture pool's policy (src/gl/streamtexpool.h) on a synthetic 5x5 walk, no Qt, no GL.
// g++ -std=c++17 -O2 -I<tree>/src/gl tests/streamtex/streamtex_test.cpp -o streamtex_test
//   streamtex_test              -> the pool (default budget 256 MB): STREAMTEX PASS, rc 0
//   streamtex_test --red        -> WW_CELL_STREAMTEX_RED=off, the keep timer (5 s) as before: must FAIL, rc 1
//   streamtex_test --red-keep0  -> the red with WW_CELL_STREAM_TEXKEEP=0 (nothing freed, keep-all): must FAIL, rc 1
//   streamtex_test --red-norefs -> the pool with no count by cell (every texture poolable at once): must FAIL, rc 1
// The walk: the 5x5 block centred on the camera's cell; 10 crossings east, 10 back west, then 6 back and forth over
// one border. A crossing is 8 virtual seconds of 10 paints; the camera faces the way it walks and binds the textures
// of the loaded cells in front of it (and the far field's). A cell's textures go up when it attaches (the pump).
// Rows (pre-registered):
//   (a) memory: resident texture bytes at the end <= the fresh open's + the budget
//   (b) no free of a texture a loaded cell names (the world's own truth, not the pool's counts)
//   (c) no reload storm: a texture freed and loaded again within N = 2 crossings is a reload. The pool may only reload
//       what the budget had no room for: each crossing's reloaded bytes <= max(0, the bytes released into the pool
//       in the N crossings before it - the budget). Light world: a row and its region sets release 200 MB, under the
//       budget alone; heavy: 820 MB, so it must reload something (the bar is shown to bite) and no more than that.
#include "streamtexpool.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
enum class Mode { Pool, Red, RedKeep0, RedNoRefs };

struct World
{
	int unique = 6;                // textures only this cell uses
	std::uint64_t uniqueBytes = 4ull << 20;
	int region = 20;               // a 3x3 region's own set
	std::uint64_t regionBytes = 2ull << 20;
	const char * name = "light";

	std::vector<std::pair<std::string, std::uint64_t>> textures( int x, int y ) const
	{
		std::vector<std::pair<std::string, std::uint64_t>> t;
		for ( int i = 0; i < 30; i++ )	// everywhere: ground, rocks, the common clutter
			t.emplace_back( "g" + std::to_string( i ), 2ull << 20 );
		const int rx = x >= 0 ? x / 3 : ( x - 2 ) / 3, ry = y >= 0 ? y / 3 : ( y - 2 ) / 3;
		for ( int i = 0; i < region; i++ )	// a 3x3 region's set
			t.emplace_back( "r" + std::to_string( rx ) + "_" + std::to_string( ry ) + "_" + std::to_string( i ), regionBytes );
		for ( int i = 0; i < unique; i++ )
			t.emplace_back( "u" + std::to_string( x ) + "_" + std::to_string( y ) + "_" + std::to_string( i ), uniqueBytes );
		return t;
	}
};

struct Result
{
	std::uint64_t fresh = 0, end = 0, budget = 0;
	int badFrees = 0, reloads = 0, overReloadCrossings = 0;
	std::uint64_t reloadBytes = 0, worstOverMb = 0;
};

Result walk( const World & w, Mode mode, bool print )
{
	const std::uint64_t budget = 256ull << 20;
	const std::uint32_t keep = mode == Mode::RedKeep0 ? 0 : 5;
	StreamTexPool<std::string> pool( budget, 2, mode == Mode::Red || mode == Mode::RedKeep0 );
	using Cell = StreamTexPool<std::string>::Cell;
	std::map<std::string, std::uint32_t> lastBind;	// texture -> paint serial (TexCache::useTick, in paints here)
	std::set<std::string> resident;
	std::map<std::pair<int, int>, bool> loaded;
	std::vector<std::pair<std::string, std::uint64_t>> base;
	for ( int i = 0; i < 20; i++ )
		base.emplace_back( "far" + std::to_string( i ), 2ull << 20 );
	std::map<std::string, std::uint64_t> bytesOf;
	Result r;
	r.budget = budget;
	auto load = [&]( const std::string & k, std::uint64_t b ) {
		if ( resident.insert( k ).second )
			pool.loaded( k, b );
		bytesOf[k] = b;
	};
	auto free1 = [&]( const std::string & k ) {
		if ( !resident.erase( k ) )
			return;
		for ( const auto & c : loaded )
			for ( const auto & t : w.textures( c.first.first, c.first.second ) )
				if ( t.first == k ) {
					r.badFrees++;
					goto counted;
				}
	counted:
		pool.freed( k );
	};
	auto attach = [&]( int x, int y ) {
		loaded[{ x, y }] = true;
		const Cell id = StreamTexPool<std::string>::cellId( x, y );
		for ( const auto & t : w.textures( x, y ) ) {
			pool.note( id, t.first );
			load( t.first, t.second );
		}
		if ( mode == Mode::RedNoRefs )
			pool.dropCell( id );
	};
	for ( const auto & t : base ) {
		pool.note( StreamTexPool<std::string>::kBase, t.first );
		load( t.first, t.second );
	}
	int cx = 0, cy = 0, dir = 1;
	auto setBlock = [&]() {
		for ( auto it = loaded.begin(); it != loaded.end(); ) {
			if ( std::abs( it->first.first - cx ) > 2 || std::abs( it->first.second - cy ) > 2 ) {
				pool.dropCell( StreamTexPool<std::string>::cellId( it->first.first, it->first.second ) );
				it = loaded.erase( it );
			} else {
				++it;
			}
		}
		for ( int dy = -2; dy <= 2; dy++ )
			for ( int dx = -2; dx <= 2; dx++ )
				if ( !loaded.count( { cx + dx, cy + dy } ) )
					attach( cx + dx, cy + dy );
	};
	std::uint32_t paint = 0;
	std::uint32_t paintSec[4] = {};
	std::uint32_t nextEvict = 0;
	auto settle = [&]() {	// 8 s of paints, GLView::wwCellStreamEvict's clock and rules
		for ( int p = 0; p < 80; p++ ) {
			paint++;
			const std::uint32_t sec = 1 + paint / 10;
			for ( const auto & t : base ) {
				if ( !resident.count( t.first ) )
					load( t.first, t.second );
				lastBind[t.first] = sec;
			}
			for ( const auto & c : loaded ) {
				if ( ( c.first.first - cx ) * dir < 0 )
					continue;	// behind the camera: not bound
				for ( const auto & t : w.textures( c.first.first, c.first.second ) ) {
					if ( !resident.count( t.first ) )
						load( t.first, t.second );
					lastBind[t.first] = sec;
				}
			}
			paintSec[paint & 3U] = sec;
			if ( paint < 4 || sec < nextEvict || sec <= 5 )
				continue;
			nextEvict = sec + 2U;
			const std::uint32_t threePaintsAgo = paintSec[( paint + 1 ) & 3U];
			if ( keep ) {	// the keep timer, on the textures the pool leaves to it
				const std::uint32_t before = std::min( sec - keep, threePaintsAgo );
				std::vector<std::string> go;
				for ( const auto & k : resident )
					if ( pool.rule( k ) == StreamTexPool<std::string>::Rule::Timer && lastBind[k] < before )
						go.push_back( k );
				for ( const auto & k : go )
					free1( k );
			}
			for ( const auto & k : pool.plan( [&]( const std::string & k ) { return lastBind[k] >= threePaintsAgo; } ) )
				free1( k );
		}
	};
	setBlock();
	settle();
	{
		const auto c0 = pool.cross();	// the fresh open's census
		r.fresh = pool.residentBytes();
		if ( print )
			std::printf( "  open: resident %.0f MB refd %d (%.0f MB) kept %d\n", r.fresh / 1048576.0, c0.refd,
				c0.refdBytes / 1048576.0, c0.kept );
	}
	std::vector<int> steps;
	for ( int i = 0; i < 10; i++ )
		steps.push_back( 1 );
	for ( int i = 0; i < 10; i++ )
		steps.push_back( -1 );
	for ( int i = 0; i < 6; i++ )
		steps.push_back( i & 1 ? -1 : 1 );
	std::vector<std::uint64_t> released( 1, 0 );
	for ( size_t i = 0; i < steps.size(); i++ ) {
		dir = steps[i];
		cx += dir;
		setBlock();
		settle();
		const auto c = pool.cross();
		r.reloads += c.reloaded;
		r.reloadBytes += c.reloadedBytes;
		std::uint64_t room = 0;	// what the last N crossings released past the budget
		for ( size_t k = released.size() > 2 ? released.size() - 2 : 0; k < released.size(); k++ )
			room += released[k];
		room = room > budget ? room - budget : 0;
		if ( c.reloadedBytes > room ) {
			r.overReloadCrossings++;
			r.worstOverMb = std::max( r.worstOverMb, ( c.reloadedBytes - room ) >> 20 );
		}
		released.push_back( c.releasedBytes );
		if ( print )
			std::printf( "  stream tex: crossing %d kept %d (%.0f MB) refd %d (%.0f MB) evicted %d (%.0f MB) reloaded %d (%.0f MB) "
				"released %d (%.0f MB) resident %.0f MB\n", c.crossing, c.kept, c.keptBytes / 1048576.0, c.refd,
				c.refdBytes / 1048576.0, c.evicted, c.evictedBytes / 1048576.0, c.reloaded, c.reloadedBytes / 1048576.0,
				c.released, c.releasedBytes / 1048576.0, pool.residentBytes() / 1048576.0 );
	}
	r.end = pool.residentBytes();
	return r;
}

bool judge( const World & w, const Result & r, bool heavy )
{
	const bool a = r.end <= r.fresh + r.budget;
	const bool b = r.badFrees == 0;
	// none past the overflow bar; the heavy world must reload SOMETHING (else the bar never bit)
	const bool c = r.overReloadCrossings == 0 && ( !heavy || r.reloads > 0 );
	std::printf( "%s world: (a) end %.0f MB <= open %.0f + budget %.0f MB: %s | (b) frees of a loaded cell's texture %d: %s"
		" | (c) reloads %d (%.0f MB), crossings past the overflow bar %d (worst +%llu MB): %s\n", w.name, r.end / 1048576.0,
		r.fresh / 1048576.0, r.budget / 1048576.0, a ? "pass" : "FAIL", r.badFrees, b ? "pass" : "FAIL", r.reloads,
		r.reloadBytes / 1048576.0, r.overReloadCrossings, (unsigned long long) r.worstOverMb, c ? "pass" : "FAIL" );
	return a && b && c;
}
} // namespace

int main( int argc, char ** argv )
{
	Mode mode = Mode::Pool;
	bool verbose = false;
	for ( int i = 1; i < argc; i++ ) {
		if ( !std::strcmp( argv[i], "--red" ) )
			mode = Mode::Red;
		else if ( !std::strcmp( argv[i], "--red-keep0" ) )
			mode = Mode::RedKeep0;
		else if ( !std::strcmp( argv[i], "--red-norefs" ) )
			mode = Mode::RedNoRefs;
		else if ( !std::strcmp( argv[i], "-v" ) )
			verbose = true;
	}
	World light;
	World heavy;
	heavy.unique = 25;	// a row: 5 x 25 x 4 MB + two region sets of 40 x 4 MB = 820 MB, past the 256 MB budget
	heavy.region = 40;
	heavy.regionBytes = 4ull << 20;
	heavy.name = "heavy";
	const char * names[] = { "pool", "red (timer)", "red keep0 (keep-all)", "red norefs" };
	std::printf( "mode %s\n", names[int( mode )] );
	const bool okL = judge( light, walk( light, mode, verbose ), false );
	const bool okH = judge( heavy, walk( heavy, mode, verbose ), true );
	const bool ok = okL && okH;
	std::printf( "STREAMTEX %s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
