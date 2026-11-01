// lane STREAMTEX2: the stream texture pool's frees with bindless handles (src/gl/streamtexpool.h + src/gl/texgrave.h,
// the real headers) against a mock GL driver and a mock of WwBl's handle cache (src/gl/cellbindless.cpp handleOfBound /
// forget, rule for rule). No Qt, no GL.
//   g++ -std=c++17 -O2 -Wall -I<tree>/src/gl tests/streamtex2/streamtex2_test.cpp -o streamtex2_test
//   streamtex2_test                -> STREAMTEX2 PASS, rc 0
//   streamtex2_test --red-unfenced -> WW_CELL_STREAMTEX_RED=unfenced (names and handles freed at once; round 1's
//                                     --red-nofence is the same): must FAIL, rc 1
//   streamtex2_test --red-eager    -> WW_CELL_STREAMTEX_RED=eager (round 2: the pool frees past 0, not its budget): (e)
//                                     must FAIL, rc 1
//   streamtex2_test --red-handle   -> the free deletes the name without dropping its handle (the brief's crash shape):
//                                     must FAIL, rc 1
//   streamtex2_test --red-busytrim -> WW_CELL_STREAMTEX_RED=busytrim (trim to the budget mid-write): must FAIL, rc 1
// The mock driver hands out the lowest free name (a deleted name comes straight back, as NVIDIA's does), takes one
// handle per texture object, and keeps kLag frames on the GPU behind the CPU: at the top of paint N, frames N-4..N-1 are
// still queued. GLView::wwCellStreamEvict's order per paint: sweep the grave, plan (busy = bound by one of the last 3
// paints; run EVERY paint here, the worst case of its 2 s turn), free, then the paint draws each attached cell's
// textures through the handle cache. The keep timer is not modelled (tests/streamtex has it).
// Two worlds, both walking east one column a crossing:
//   fence:  25 textures of 4 MB per cell (a column leaving releases 500 MB, budget 256 MB), the whole incoming column
//           written in one paint, so the plan frees the left column's textures 4 paints after they were last drawn;
//   writes: 6 own + 6 "pair" textures per cell (the pair set is shared by the cells 5 columns apart, i.e. the column
//           leaving and the one coming in), budget 128 MB, one incoming cell written every 2 paints.
// Rows (pre-registered):
//   (h) handle gone before the delete: no glDeleteTextures of a texture whose handle is resident or still cached
//   (f) no handle made non-resident while a frame still on the GPU names it
//   (n) a recycled name never returns an old handle: every handle a draw names is resident and of a live texture;
//       names were recycled (else the row never bit)
//   (w) writes world: no reload of a texture the pool freed, and the pool did free (else the row never bit)
//   (b) the grave is empty once the GPU drained; the pool never held more than budget + one crossing's release
//   (e) round 2: the pool keeps its budget: its frees made inside it (Census::freedEarly) are under 5% of its frees,
//       and it did free (else the row never bit)
// Caveat (round 2): the plan's `busy` here is "bound by one of the last 3 PAINTS" against a 4-frame GPU queue, the
// worst case. The view's own clock is in SECONDS (glview.cpp setUseTick(sec), busy = bound in or after the second of
// the paint 3 back), so live, the textures it frees were last bound at least a second-boundary ago.
#include "streamtexpool.h"
#include "texgrave.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
enum class Mode { Green, RedNoFence, RedHandle, RedBusyTrim, RedEager };
constexpr std::size_t kLag = 4;

struct MockGl
{
	std::set<std::uint32_t> names;	// names in use
	std::map<std::uint32_t, std::uint64_t> obj;	// name -> texture object
	std::uint64_t serial = 0;
	std::set<std::uint64_t> dead;	// deleted objects
	std::map<std::uint64_t, std::uint64_t> handleObj;	// handle -> object
	std::set<std::uint64_t> resident;
	std::deque<std::set<std::uint64_t>> gpu;	// queued frames, each the handles its rows name
	std::uint64_t submitted = 0, retired = 0;
	int badDelete = 0, badNonResident = 0, badSample = 0;

	std::uint32_t gen()
	{
		std::uint32_t n = 1;
		while ( names.count( n ) )
			n++;
		names.insert( n );
		obj[n] = ++serial;
		return n;
	}
	std::uint64_t texHandle( std::uint32_t n )
	{
		const std::uint64_t h = 0x100000000ull + obj.at( n );
		handleObj[h] = obj.at( n );
		return h;
	}
	void makeResident( std::uint64_t h ) { resident.insert( h ); }
	void nonResident( std::uint64_t h )
	{
		for ( const auto & f : gpu )
			if ( f.count( h ) ) {
				badNonResident++;
				break;
			}
		resident.erase( h );
	}
	void del( std::uint32_t n )
	{
		const std::uint64_t o = obj.at( n );
		for ( std::uint64_t h : resident )
			if ( handleObj[h] == o )
				badDelete++;
		dead.insert( o );
		names.erase( n );
		obj.erase( n );
	}
	void draw( const std::set<std::uint64_t> & frame )
	{
		for ( std::uint64_t h : frame )
			if ( !resident.count( h ) || dead.count( handleObj[h] ) )
				badSample++;
		gpu.push_back( frame );
		submitted++;
		while ( gpu.size() > kLag ) {
			gpu.pop_front();
			retired++;
		}
	}
	std::uint64_t fence() const { return submitted; }	// passed once every frame submitted before it retired
	void drain()
	{
		retired += gpu.size();
		gpu.clear();
	}
};

struct MockBl	// WwBl's handle cache: by GL name, a hit returns the cached handle without asking the driver
{
	MockGl & gl;
	std::unordered_map<std::uint32_t, std::uint64_t> handles;
	std::uint64_t handleOfBound( std::uint32_t tex )
	{
		auto it = handles.find( tex );
		if ( it != handles.end() )
			return it->second;
		const std::uint64_t h = gl.texHandle( tex );
		gl.makeResident( h );
		handles[tex] = h;
		return h;
	}
	void forget( std::uint32_t tex )
	{
		auto it = handles.find( tex );
		if ( it != handles.end() ) {
			gl.nonResident( it->second );
			handles.erase( it );
		}
	}
};

struct World
{
	const char * name;
	int unique, pair, attachEvery;
	std::uint64_t budget;
	std::vector<std::pair<std::string, std::uint64_t>> textures( int x, int y ) const
	{
		std::vector<std::pair<std::string, std::uint64_t>> t;
		for ( int i = 0; i < 10; i++ )	// everywhere
			t.emplace_back( "g" + std::to_string( i ), 2ull << 20 );
		for ( int i = 0; i < unique; i++ )
			t.emplace_back( "u" + std::to_string( x ) + "_" + std::to_string( y ) + "_" + std::to_string( i ), 4ull << 20 );
		const int m = ( ( x % 5 ) + 5 ) % 5;
		for ( int i = 0; i < pair; i++ )
			t.emplace_back( "p" + std::to_string( m ) + "_" + std::to_string( y ) + "_" + std::to_string( i ), 4ull << 20 );
		return t;
	}
};

struct Result
{
	int badCached = 0, badDelete = 0, badNonResident = 0, badSample = 0;
	int evicted = 0, recycled = 0, reloads = 0, reloadsPool = 0;
	std::uint64_t reloadPoolBytes = 0, maxKept = 0, maxRelease = 0;
	std::uint64_t earlyBytes = 0, evictedBytes = 0;	// round 2: (e)
	std::size_t graveLeft = 0;
};

Result walk( const World & w, Mode mode, bool verbose )
{
	using Pool = StreamTexPool<std::string>;
	Pool pool( w.budget, 2, false );
	MockGl gl;
	MockBl bl{ gl, {} };
	TexGrave<std::uint64_t> grave;
	std::map<std::string, std::uint32_t> id;	// TexCache: file -> GL name (0 = not loaded)
	std::map<std::string, std::uint64_t> bytesOf;
	std::map<std::string, std::uint32_t> useTick;	// TexCache::useTick, in paints
	std::set<std::pair<int, int>> attached;
	std::deque<std::pair<int, int>> toAttach;	// the incoming column, written a cell at a time
	std::set<std::uint32_t> everDeleted;
	Result r;
	std::uint32_t paint = 0;
	int cx = 0;

	// TexCache::wwGraveSweep's release (the reds: freeLoaded's at once; --red-handle skips the handle drop)
	auto release = [&]( std::uint32_t a, std::uint32_t b ) {
		for ( std::uint32_t n : { a, b } ) {
			if ( !n )
				continue;
			if ( mode != Mode::RedHandle )
				bl.forget( n );
			if ( bl.handles.count( n ) )
				r.badCached++;
			gl.del( n );
			everDeleted.insert( n );
		}
	};
	auto passed = [&]( std::uint64_t f ) { return gl.retired >= f; };
	auto drop = []( std::uint64_t ) {};
	auto bind = [&]( const std::string & f ) -> std::uint32_t {
		std::uint32_t & n = id[f];
		if ( !n ) {
			n = gl.gen();
			if ( everDeleted.count( n ) )
				r.recycled++;
			pool.loaded( f, bytesOf[f] );
		}
		useTick[f] = paint;
		return n;
	};
	auto attach = [&]( int x, int y ) {
		attached.insert( { x, y } );
		for ( const auto & t : w.textures( x, y ) ) {
			bytesOf[t.first] = t.second;
			pool.note( Pool::cellId( x, y ), t.first );
		}
	};
	auto onePaint = [&]() {
		paint++;
		if ( !toAttach.empty() && ( w.attachEvery == 0 || paint % std::uint32_t( w.attachEvery ) == 0 ) ) {
			do {
				attach( toAttach.front().first, toAttach.front().second );
				toAttach.pop_front();
			} while ( w.attachEvery == 0 && !toAttach.empty() );
		}
		grave.sweep( passed, release, drop );
		const bool writing = !toAttach.empty() && mode != Mode::RedBusyTrim;	// Stream::busy(): cells still written
		const auto plan = pool.plan( [&]( const std::string & f ) { return useTick[f] + 3 >= paint; }, writing,
			mode == Mode::RedEager );
		for ( const std::string & f : plan ) {
			std::uint32_t & n = id[f];
			if ( !n ) {
				pool.gone( f );
				continue;
			}
			if ( mode == Mode::RedNoFence || mode == Mode::RedHandle )
				release( n, 0 );
			else
				grave.bury( n, 0 );
			n = 0;
			pool.freed( f, true );
			r.evicted++;
		}
		if ( grave.unsealed() )
			grave.seal( gl.fence() );
		std::set<std::uint64_t> frame;
		for ( const auto & c : attached )
			for ( const auto & t : w.textures( c.first, c.second ) )
				frame.insert( bl.handleOfBound( bind( t.first ) ) );
		gl.draw( frame );
		std::uint64_t kept = 0;
		for ( const auto & kv : id )
			if ( kv.second && pool.refs( kv.first ) == 0 )
				kept += bytesOf[kv.first];
		r.maxKept = std::max( r.maxKept, kept );
	};
	auto census = [&]() {
		const auto c = pool.cross();
		r.reloads += c.reloaded;
		r.reloadsPool += c.reloadedPool;
		r.reloadPoolBytes += c.reloadedPoolBytes;
		r.maxRelease = std::max( r.maxRelease, c.releasedBytes );
		r.earlyBytes += c.freedEarlyBytes;
		r.evictedBytes += c.evictedBytes;
		if ( verbose )
			std::printf( "  %s crossing %d: released %.0f MB evicted %d (%.0f MB, %.0f MB inside the budget) reloaded %d "
				"(pool %d, %.0f MB) kept %.0f MB\n", w.name, c.crossing, c.releasedBytes / 1048576.0, c.evicted,
				c.evictedBytes / 1048576.0, c.freedEarlyBytes / 1048576.0, c.reloaded, c.reloadedPool,
				c.reloadedPoolBytes / 1048576.0, c.keptBytes / 1048576.0 );
	};

	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ )
			attach( x, y );
	for ( int p = 0; p < 12; p++ )
		onePaint();
	census();	// the open's
	for ( int k = 0; k < 12; k++ ) {
		cx++;
		for ( auto it = attached.begin(); it != attached.end(); ) {
			if ( std::abs( it->first - cx ) > 2 ) {
				pool.dropCell( Pool::cellId( it->first, it->second ) );
				it = attached.erase( it );
			} else {
				++it;
			}
		}
		for ( int y = -2; y <= 2; y++ )
			toAttach.emplace_back( cx + 2, y );
		for ( int p = 0; p < 16; p++ )
			onePaint();
		census();
	}
	gl.drain();
	grave.sweep( passed, release, drop );
	r.graveLeft = grave.size();
	r.badDelete = gl.badDelete;
	r.badNonResident = gl.badNonResident;
	r.badSample = gl.badSample;
	return r;
}

bool judge( const World & w, const Result & r, bool writesRow )
{
	const bool h = r.badDelete == 0 && r.badCached == 0;
	const bool f = r.badNonResident == 0;
	const bool n = r.badSample == 0 && r.recycled > 0;
	const bool wr = !writesRow || ( r.reloadsPool == 0 && r.evicted > 0 );
	const bool b = r.graveLeft == 0 && r.maxKept <= w.budget + r.maxRelease;
	const bool e = r.evictedBytes > 0 && double( r.earlyBytes ) <= 0.05 * double( r.evictedBytes );	// round 2
	std::printf( "%s world: (h) deletes with the handle resident %d, still cached %d: %s | (f) non-resident under a queued frame %d: %s"
		" | (n) dead handles drawn %d, names recycled %d: %s", w.name, r.badDelete, r.badCached, h ? "pass" : "FAIL",
		r.badNonResident, f ? "pass" : "FAIL", r.badSample, r.recycled, n ? "pass" : "FAIL" );
	if ( writesRow )
		std::printf( " | (w) pool frees %d, reloads of them %d (%.0f MB): %s", r.evicted, r.reloadsPool,
			r.reloadPoolBytes / 1048576.0, wr ? "pass" : "FAIL" );
	std::printf( " | (b) grave left %zu, pool peak %.0f MB <= budget %.0f + release %.0f: %s", r.graveLeft,
		r.maxKept / 1048576.0, w.budget / 1048576.0, r.maxRelease / 1048576.0, b ? "pass" : "FAIL" );
	std::printf( " | (e) freed inside the budget %.0f MB of %.0f MB freed (<= 5%%): %s\n", r.earlyBytes / 1048576.0,
		r.evictedBytes / 1048576.0, e ? "pass" : "FAIL" );
	return h && f && n && wr && b && e;
}
} // namespace

int main( int argc, char ** argv )
{
	Mode mode = Mode::Green;
	bool verbose = false;
	for ( int i = 1; i < argc; i++ ) {
		if ( !std::strcmp( argv[i], "--red-nofence" ) || !std::strcmp( argv[i], "--red-unfenced" ) )
			mode = Mode::RedNoFence;
		else if ( !std::strcmp( argv[i], "--red-eager" ) )
			mode = Mode::RedEager;
		else if ( !std::strcmp( argv[i], "--red-handle" ) )
			mode = Mode::RedHandle;
		else if ( !std::strcmp( argv[i], "--red-busytrim" ) )
			mode = Mode::RedBusyTrim;
		else if ( !std::strcmp( argv[i], "-v" ) )
			verbose = true;
	}
	const char * names[] = { "fenced", "red unfenced", "red handle", "red busytrim", "red eager" };
	std::printf( "mode %s\n", names[int( mode )] );
	const World fence{ "fence", 25, 0, 0, 256ull << 20 };
	const World writes{ "writes", 6, 6, 2, 128ull << 20 };
	const bool okF = judge( fence, walk( fence, mode, verbose ), false );
	const bool okW = judge( writes, walk( writes, mode, verbose ), true );
	// (g) the grave alone: batches pass in order, a not-passed fence holds the ones after it, `all` takes the open batch
	bool okG = true;
	{
		TexGrave<int> g;
		std::vector<std::uint32_t> out;
		auto rel = [&]( std::uint32_t a, std::uint32_t ) { out.push_back( a ); };
		auto none = []( int ) {};
		g.bury( 1, 0 );
		g.bury( 2, 7 );
		const bool s1 = g.seal( 10 ) && !g.seal( 11 );	// a seal with nothing buried takes no fence
		g.bury( 3, 0 );
		g.seal( 20 );
		g.bury( 4, 0 );
		const int a = g.sweep( []( int f ) { return f <= 10; }, rel, none );
		const int b = g.sweep( []( int f ) { return f > 10; }, rel, none );	// 20 passed: batch 2; the open batch stays
		const int c = g.sweep( []( int ) { return false; }, rel, none, true );
		okG = s1 && a == 2 && b == 1 && c == 1 && g.empty() && out == std::vector<std::uint32_t>{ 1, 2, 3, 4 };
		std::printf( "(g) grave order: seal-empty %d, swept %d/%d/%d, order 1234 %d: %s\n", s1, a, b, c,
			out == std::vector<std::uint32_t>{ 1, 2, 3, 4 }, okG ? "pass" : "FAIL" );
	}
	const bool ok = okF && okW && okG;
	std::printf( "STREAMTEX2 %s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
