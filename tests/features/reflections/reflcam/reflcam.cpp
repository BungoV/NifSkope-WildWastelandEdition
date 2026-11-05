// lane PERFFIX2 a (item 4): the reflection probes' camera-probe schedule (src/features/reflections/cellreflcam.h), frame by frame, with
// gl/cellreflprobe.cpp's own bookkeeping round it. Pure C++.
// Build: g++ -std=c++17 -I<tree>/src tests/features/reflections/reflcam/reflcam.cpp      Red: -DREFL_RED (the old per-frame schedule): FAIL.
#include "features/reflections/cellreflcam.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
int fails = 0, checks = 0;
void check( bool ok, const std::string & what )
{
	checks++;
	fails += ok ? 0 : 1;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what.c_str() );
}

#ifdef REFL_RED
constexpr bool kPerFrame = true;
#else
constexpr bool kPerFrame = false;
#endif

// the camera probe as cellreflprobe.cpp keeps it (slot 0 + RpGpu's camera fields)
struct Cam
{
	int faces = 0, camFace = 0;
	bool lit = false;
	std::uint64_t litHash = 0, litKey = 0, faceKey[6] = {};
	std::uint64_t drawnUnder[6] = {};	// what each face's texels came from (the test's truth)
	int captures = 0, relights = 0;
	std::vector<int> order;
	// one frame: key = what a face would be drawn under, h = the placed probes' light hash, hc = the camera's
	void frame( std::uint64_t key, std::uint64_t h, std::uint64_t hc, bool perFrame )
	{
		const int f = WwReflCam::nextFace( faces, camFace, faceKey, key, perFrame );
		if ( f >= 0 ) {
			faceKey[f] = key;
			drawnUnder[f] = key;
			if ( faces >= 6 ) {
				camFace = ( f + 1 ) % 6;
				lit = false;
			} else {
				faces = faces > f + 1 ? faces : f + 1;
				camFace = faces % 6;
			}
			captures++;
			order.push_back( f );
		}
		if ( WwReflCam::relight( faces, lit, litHash, h, litKey, hc, perFrame ) ) {
			lit = true;
			litHash = h;
			litKey = hc;
			relights++;
		}
	}
	bool current( std::uint64_t key ) const
	{
		for ( std::uint64_t k : drawnUnder )
			if ( k != key )
				return false;
		return true;
	}
};
}	// namespace

int main()
{
	// 1. a still camera and light: after the first whole cube and its relight, nothing more
	{
		Cam c;
		for ( int i = 0; i < 6; i++ )
			c.frame( 11, 5, 50, kPerFrame );
		const int cap0 = c.captures, rel0 = c.relights;
		for ( int i = 0; i < 200; i++ )
			c.frame( 11, 5, 50, kPerFrame );
		check( cap0 == 6 && rel0 == 1 && c.lit && c.current( 11 ), "first 6 frames: 6 faces, 1 relight, the cube whole and current" );
		check( c.captures == cap0 && c.relights == rel0,
			"200 still frames: " + std::to_string( c.captures - cap0 ) + " faces drawn, " + std::to_string( c.relights - rel0 )
				+ " relights (want 0, 0)" );
	}
	// 2. the probe's place moves (a new 128-u snap): the next 6 frames draw the 6 faces in the old cycle's order, then none
	{
		Cam c, old;
		for ( int i = 0; i < 9; i++ ) {
			c.frame( 11, 5, 50, kPerFrame );
			old.frame( 11, 5, 50, true );
		}
		c.order.clear();
		old.order.clear();
		for ( int i = 0; i < 6; i++ ) {
			c.frame( 12, 5, 51, kPerFrame );
			old.frame( 12, 5, 51, true );
		}
		const bool same = c.order.size() == 6 && old.order.size() == 6 && c.current( 12 ) && old.current( 12 );
		for ( int i = 0; i < 30; i++ )
			c.frame( 12, 5, 51, kPerFrame );
		// the old cycle's order is its own camFace onward; this one's starts where its cycle stood (no faces since frame 6)
		bool distinct = true;
		for ( int i = 0; i < 6; i++ )
			for ( int j = i + 1; j < 6; j++ )
				distinct = distinct && c.order[size_t( i )] != c.order[size_t( j )];
		check( same && distinct && c.order.size() == 6 && c.relights >= 1,
			"a snap move: 6 distinct faces in 6 frames, the cube current under the new key, then none (old cycle: current too)" );
	}
	// 3. the light moves (h), and the view moves inside the snap (hc): relit each time, no face drawn
	{
		Cam c;
		for ( int i = 0; i < 8; i++ )
			c.frame( 11, 5, 50, kPerFrame );
		const int cap0 = c.captures, rel0 = c.relights;
		c.frame( 11, 6, 60, kPerFrame );	// the sun moved
		c.frame( 11, 6, 60, kPerFrame );
		c.frame( 11, 6, 61, kPerFrame );	// the view turned (the cascades take view space)
		c.frame( 11, 6, 61, kPerFrame );
		check( c.captures == cap0 && c.relights == rel0 + 2, "a light move then a view move: 2 relights, 0 faces (got "
			+ std::to_string( c.relights - rel0 ) + ", " + std::to_string( c.captures - cap0 ) + ")" );
	}
	// 4. the red is the old schedule: one face a frame forever, relit every frame
	{
		Cam c;
		for ( int i = 0; i < 6; i++ )
			c.frame( 11, 5, 50, true );
		const int cap0 = c.captures, rel0 = c.relights;
		for ( int i = 0; i < 12; i++ )
			c.frame( 11, 5, 50, true );
		check( c.captures == cap0 + 12 && c.relights == rel0 + 12, "red: 12 frames, 12 faces, 12 relights" );
	}
	// 5. whatever the history, a still camera ends with every face drawn under the current key (what the old cycle
	//    holds after 6 still frames): many random moves, then 6 still frames
	{
		Cam c, old;
		std::uint32_t r = 7u;
		bool ok = true;
		for ( int round = 0; round < 50 && ok; round++ ) {
			const int moves = int( r % 9u );
			for ( int i = 0; i < moves; i++ ) {
				r ^= r << 13, r ^= r >> 17, r ^= r << 5;
				const std::uint64_t key = 100 + ( r % 3u ), h = r % 2u, hc = r % 5u;
				c.frame( key, h, hc, kPerFrame );
				old.frame( key, h, hc, true );
			}
			const std::uint64_t key = 200 + std::uint64_t( round );
			for ( int i = 0; i < 6; i++ ) {
				c.frame( key, 1, 9, kPerFrame );
				old.frame( key, 1, 9, true );
			}
			ok = c.current( key ) && old.current( key ) && c.lit && c.litHash == 1 && c.litKey == 9;
		}
		check( ok, "50 random histories + 6 still frames: every face current, lit under the current light (as the old cycle)" );
	}
	std::printf( "%d checks, %d failures\n", checks, fails );
	return fails ? 1 : 0;
}
