// lane ROOMFIT11: the small rooms' merged casts (src/roomfitbatch.h), with a stand-in scene.
// g++ -std=c++17 -O2 -pthread -I<tree>/src tests/roomfit11/roomfit11_test.cpp                  -> ROOMFIT11 PASS, rc 0
// g++ -std=c++17 -O2 -pthread -I<tree>/src -DROOMFIT11_RED tests/roomfit11/roomfit11_test.cpp  -> the red: the merged
//     hits go back in reverse room order, must print ROOMFIT11 FAIL, rc 1
//
// Each synthetic room runs the fit's shape: rounds of casts whose sizes and kinds (any-hit / nearest) depend on the hits
// of the round before (as the flood's frontier does), every hit folded into the room's digest in order. The serial arm
// casts each room's batches alone, one room after another; the batched arm runs the rooms on threads through the
// batcher. Every room's digest and hit count must match, and the batched arm must really have merged (floors below).
#include "roomfitbatch.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

namespace
{
struct Ray
{
	double o[3];
	double tmax;
	double d[3];
	double pad;
};
struct Hit
{
	double t;
	int tri;
};

std::uint64_t mix( std::uint64_t v )
{
	v ^= v >> 33;
	v *= 0xff51afd7ed558ccdULL;
	v ^= v >> 33;
	v *= 0xc4ceb9fe1a85ec53ULL;
	v ^= v >> 33;
	return v;
}

// the stand-in scene: a hit that is a function of the ray and its kind alone (as the real cast's is)
std::atomic<int> inFlight( 0 ), overlap( 0 );
std::atomic<long> sceneCalls( 0 );
void scene( const Ray * rays, size_t n, Hit * out, bool anyHit )
{
	if ( inFlight.fetch_add( 1 ) != 0 )
		overlap++;   // the scene casts one batch at a time; the batcher must never call it twice at once
	sceneCalls++;
	for ( size_t k = 0; k < n; k++ ) {
		const Ray & r = rays[k];
		std::uint64_t h = anyHit ? 0x9e3779b97f4a7c15ULL : 0;
		for ( double x : { r.o[0], r.o[1], r.o[2], r.d[0], r.d[1], r.d[2], r.tmax } ) {
			std::uint64_t b;
			static_assert( sizeof b == sizeof x, "" );
			__builtin_memcpy( &b, &x, 8 );
			h = mix( h ^ b );
		}
		out[k].tri = ( h & 7 ) == 0 ? -1 : int( h % 100000 );
		out[k].t = double( h >> 40 ) / double( 1 << 24 ) * r.tmax;
	}
	std::this_thread::sleep_for( std::chrono::microseconds( 200 ) );   // a submit's wait: lets the other rooms queue
	inFlight--;
}

struct RoomOut
{
	std::uint64_t digest = 0;
	long hits = 0, casts = 0;
};

template <class CastFn>
RoomOut runRoom( int room, CastFn && cast )
{
	RoomOut o;
	std::uint64_t seed = mix( std::uint64_t( room ) * 1315423911ULL + 7 );
	size_t n = 50 + size_t( seed % 900 );
	for ( int round = 0; round < 12 && n; round++ ) {
		std::vector<Ray> rays( n );
		for ( size_t k = 0; k < n; k++ ) {
			seed = mix( seed + k );
			for ( int q = 0; q < 3; q++ ) {
				rays[k].o[q] = double( ( seed >> ( q * 8 ) ) & 0xFFF ) * 0.25 + room;
				rays[k].d[q] = double( ( seed >> ( 24 + q * 8 ) ) & 0xFF ) / 255.0 - 0.5;
			}
			rays[k].tmax = 64.0 + double( seed & 0x3FF );
			rays[k].pad = 0;
		}
		std::vector<Hit> hits( n );
		const bool any = ( ( seed >> 7 ) & 1 ) != 0;
		cast( rays.data(), n, hits.data(), any );
		o.casts++;
		size_t next = 0;
		for ( const Hit & h : hits ) {
			std::uint64_t b;
			__builtin_memcpy( &b, &h.t, 8 );
			o.digest = mix( o.digest ^ b ^ std::uint64_t( std::uint32_t( h.tri ) ) << 1 );
			o.hits++;
			next += h.tri >= 0 && ( h.tri & 3 ) == 0;   // the next round's frontier follows these hits
		}
		n = std::min<size_t>( next * 2, 4000 );
	}
	return o;
}
}	// namespace

int main()
{
	const int kRooms = 96;
	int fails = 0, checks = 0;
	auto check = [&]( bool ok, const char * what ) {
		checks++;
		fails += ok ? 0 : 1;
		std::printf( "  %s %s\n", ok ? "ok  " : "FAIL", what );
	};

	// the serial arm: each room alone, one after another (WW_CELL_ROOMFIT_RED=serial casts this way)
	std::vector<RoomOut> serial( kRooms );
	for ( int r = 0; r < kRooms; r++ )
		serial[size_t( r )] = runRoom( r, scene );
	const long serialCalls = sceneCalls.exchange( 0 );

	// the batched arm, twice: the fit's cap, and a small cap that splits the queue across several casts
	for ( size_t cap : { size_t( 4000000 ), size_t( 3000 ) } ) {
		RoomCastBatcher<Ray, Hit> batch( scene, cap );
		std::vector<RoomOut> got( kRooms );
		std::atomic<int> next( 0 );
		std::vector<std::thread> pool;
		const int nThreads = 16;
		for ( int i = 0; i < nThreads; i++ )
			pool.emplace_back( [&]() {
				for ( int r; ( r = next.fetch_add( 1 ) ) < kRooms; )
					got[size_t( r )] = runRoom( r, [&]( const Ray * y, size_t n, Hit * h, bool a ) { batch.cast( y, n, h, a ); } );
			} );
		for ( std::thread & t : pool )
			t.join();
		int same = 0;
		long hitsS = 0, hitsB = 0;
		for ( int r = 0; r < kRooms; r++ ) {
			same += got[size_t( r )].digest == serial[size_t( r )].digest && got[size_t( r )].hits == serial[size_t( r )].hits
				&& got[size_t( r )].casts == serial[size_t( r )].casts;
			hitsS += serial[size_t( r )].hits;
			hitsB += got[size_t( r )].hits;
		}
		const long calls = sceneCalls.exchange( 0 );
		std::printf( "cap %zu: rooms %d same %d, hits %ld / %ld, room casts %zu in scene casts %zu (serial %ld), most rooms %zu\n", cap,
			kRooms, same, hitsB, hitsS, batch.requests(), batch.calls(), serialCalls, batch.mostRooms() );
		check( same == kRooms, "every room's hits equal the serial path's, in order" );
		check( hitsS > 10000 && hitsB == hitsS, "floor: the rooms cast real work (hits > 10000), the same count" );
		check( long( batch.requests() ) == serialCalls, "every room cast went through the batcher" );
		check( batch.mostRooms() > 1 && long( batch.calls() ) < serialCalls, "floor: casts were merged (most rooms > 1, fewer scene casts)" );
		check( long( batch.calls() ) == calls, "the census counts the scene's own calls" );
		check( overlap.load() == 0, "the scene never ran two casts at once" );
	}
	std::printf( "%d checks, %d failures\nROOMFIT11 %s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
