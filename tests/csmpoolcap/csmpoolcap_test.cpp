// lane CRASH1116: the cascades' mesh pool must never let a run place more vertices than its smallest per-vertex buffer
// holds (positions 12, uvs 8, wind 4 bytes a vertex). bungo's 11-16 Goodneighbor flight: positions 32 MB (2796202
// vertices), uvs 16 MB (2097152), a mesh of 63954 vertices placed at 2068873 -> csmPool's uv memcpy ran off the mapping.
// RED_POSONLY: the pool's law before the fix (positions alone), must FAIL.
#include "features/vkframe/csmpoolcap.h"
#include <cstdio>

static std::uint64_t vertCap( std::uint64_t p, std::uint64_t u, std::uint64_t w )
{
#ifdef CSMPOOL_RED_POSONLY
	(void)u;
	(void)w;
	return p / 12;
#else
	return WwCsmPool::vertCap( p, u, w );
#endif
}

int main()
{
	int fails = 0;
	std::uint64_t compared = 0, over = 0, firstOver = 0;
	// every repack size csmPool can ask for (needV = 2 x the run's vertices), 1 .. 8M vertices
	for ( std::uint64_t needV = 1; needV <= 8000000; needV++ ) {
		const std::uint64_t p = WwCsmPool::bufCap( needV * 12 ), u = WwCsmPool::bufCap( needV * 8 ), w = WwCsmPool::bufCap( needV * 4 );
		const std::uint64_t vc = vertCap( p, u, w );
		compared++;
		if ( vc * 12 > p || vc * 8 > u || vc * 4 > w ) {
			if ( !over )
				firstOver = needV;
			over++;
		}
		if ( vc < needV ) {
			std::printf( "FAIL needV %llu: the pool holds %llu vertices after a repack for it\n", (unsigned long long)needV, (unsigned long long)vc );
			fails++;
			break;
		}
	}
	std::printf( "sweep: %llu repack sizes compared, %llu let more vertices in than a buffer holds (first at needV %llu)\n",
		(unsigned long long)compared, (unsigned long long)over, (unsigned long long)firstOver );
	if ( over ) {
		std::printf( "FAIL sweep: %llu repack sizes overrun the uvs or the wind\n", (unsigned long long)over );
		fails++;
	}
	// the flight's dump: posUsed 2068873 + a 63954-vertex mesh must not fit 32 MB / 16 MB / 8 MB
	const std::uint64_t flight = vertCap( 32ull << 20, 16ull << 20, 8ull << 20 );
	std::printf( "flight: the pool holds %llu vertices, the run placed up to %llu\n", (unsigned long long)flight, 2068873ull + 63954ull );
	if ( 2068873ull + 63954ull <= flight ) {
		std::printf( "FAIL flight: the 63954-vertex mesh fits at 2068873 (the uvs hold 2097152)\n" );
		fails++;
	}
	std::printf( "CSMPOOLCAP %s (%d fails)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
