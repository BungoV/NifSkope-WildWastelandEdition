// Lane STREAMTEX4: unit test of src/gl/texgrave.h (STREAMTEX2's fenced frees) against a mock driver whose GPU runs
// LAG frames behind the CPU. Each frame samples the bindless handles of the textures it binds; the stream pool frees
// textures not bound by the last 3 paints, which a queued frame may still name. Green (fenced, as TexCache::evictPool
// with fenced true): no handle is dropped and no GL name deleted or handed out again while a queued frame names it.
// Red -DGRAVE_RED (WW_CELL_STREAMTEX_RED=unfenced): the free happens at once and must count violations.
#include "gl/texgrave.h"

#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <vector>

namespace
{
constexpr int LAG = 4;          // frames the GPU runs behind (driver queue + GPU)
constexpr int PROTECT = 3;      // the pool never frees what the last 3 paints bound
constexpr int FRAMES = 4000;
constexpr int FILES = 400;

struct Driver
{
	std::set<std::uint32_t> live;           // names glGenTextures handed out and not deleted
	std::set<std::uint32_t> freeNames;      // deleted names, handed out again lowest first (as drivers do)
	std::uint32_t next = 1;
	std::uint64_t handleGen = 1;
	std::map<std::uint32_t, std::uint64_t> resident;   // name -> resident handle
	int frame = 0;                          // CPU frame now being recorded
	int done = -1;                          // last frame the GPU finished
	std::deque<std::pair<int, std::vector<std::uint64_t>>> queued;    // frames not finished: the handles they sample
	long long droppedLive = 0, nameReusedLive = 0, deletedLive = 0;

	bool queuedNames( std::uint64_t h ) const
	{
		for ( const auto & q : queued )
			for ( std::uint64_t x : q.second )
				if ( x == h )
					return true;
		return false;
	}
	std::uint32_t gen()
	{
		std::uint32_t n;
		if ( !freeNames.empty() ) {
			n = *freeNames.begin();
			freeNames.erase( freeNames.begin() );
		} else {
			n = next++;
		}
		live.insert( n );
		return n;
	}
	std::uint64_t handleOf( std::uint32_t name )
	{
		auto it = resident.find( name );
		if ( it != resident.end() )
			return it->second;
		return resident[name] = ( std::uint64_t( name ) << 32 ) | handleGen++;
	}
	void nonResident( std::uint32_t name )
	{
		auto it = resident.find( name );
		if ( it == resident.end() )
			return;
		if ( queuedNames( it->second ) )
			droppedLive++;
		resident.erase( it );
	}
	void del( std::uint32_t name )
	{
		if ( !name )
			return;
		nonResident( name );
		live.erase( name );
		freeNames.insert( name );
	}
	void submit( std::vector<std::uint64_t> handles )
	{
		queued.emplace_back( frame, std::move( handles ) );
		frame++;
		while ( !queued.empty() && queued.front().first <= frame - 1 - LAG ) {
			done = queued.front().first;
			queued.pop_front();
		}
	}
};

struct Tex
{
	std::uint32_t id = 0;
	int lastBind = -1000;
	std::uint64_t handle = 0;   // the handle the last frame sampled for this file
};
}

int main()
{
#ifdef GRAVE_RED
	const bool fenced = false;
#else
	const bool fenced = true;
#endif
	Driver d;
	TexGrave<int> grave;    // fence = the frame it was taken in (passed once the GPU finished that frame)
	std::vector<Tex> tex( FILES );
	long long frees = 0, released = 0, gens = 0;
	unsigned rng = 12345;
	auto rnd = [&rng]() { rng = rng * 1103515245u + 12345u; return ( rng >> 8 ) & 0xffff; };
	for ( int f = 0; f < FRAMES; f++ ) {
		// wwGraveSweep: the batches whose fence the GPU passed
		released += grave.sweep( [&d]( int fence ) { return fence <= d.done; },
			[&d]( std::uint32_t a, std::uint32_t b ) { d.del( a ); d.del( b ); }, []( int ) {} );
		// the camera's window: 60 files around a centre that walks through the set
		const int centre = ( f / 8 ) % FILES;
		std::vector<std::uint64_t> handles;
		for ( int k = 0; k < 60; k++ ) {
			if ( rnd() % 4 == 0 )
				continue;   // turned from it this paint
			Tex & t = tex[( centre + k ) % FILES];
			if ( !t.id ) {
				t.id = d.gen();
				gens++;
				// bornName: a name a queued frame still samples under the old texture's handle
				for ( const auto & q : d.queued )
					for ( std::uint64_t h : q.second )
						if ( std::uint32_t( h >> 32 ) == t.id ) {
							d.nameReusedLive++;
							goto counted;
						}
			counted:;
			}
			t.lastBind = f;
			t.handle = d.handleOf( t.id );
			handles.push_back( t.handle );
		}
		// the pool's free: every file not bound by the last PROTECT paints, every 4th paint
		if ( f % 4 == 0 ) {
			for ( Tex & t : tex ) {
				if ( !t.id || t.lastBind > f - PROTECT )
					continue;
				frees++;
				if ( fenced ) {
					grave.bury( t.id, 0 );
				} else {
					d.del( t.id );
					released++;
				}
				t.id = 0;
			}
			if ( fenced )
				grave.seal( f - 1 );    // the fence goes in after every frame recorded before this paint's frees
		}
		d.submit( std::move( handles ) );
	}
	// TexCache::flush: glFinish, then everything
	d.queued.clear();
	released += grave.sweep( []( int ) { return true; }, [&d]( std::uint32_t a, std::uint32_t b ) { d.del( a ); d.del( b ); },
		[]( int ) {}, true );
	std::printf( "walk %s frames %d frees %lld released %lld gens %lld names %zu live %zu grave %zu\n",
		fenced ? "fenced" : "unfenced", FRAMES, frees, released, gens, std::size_t( d.next - 1 ), d.live.size(), grave.size() );
	std::printf( "census freedlive %lld stale %lld\n", d.droppedLive, d.nameReusedLive );
	const bool ok = d.droppedLive == 0 && d.nameReusedLive == 0 && frees > 0 && released == frees && grave.empty();
	std::printf( "%s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
