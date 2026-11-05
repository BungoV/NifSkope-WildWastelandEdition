// CARDMEM2 unit: the whole-map VT pyramid's land-cache drop and texture budget (src/lodgenbakemem.h).
// Green: g++ -std=c++17 -I<tree>/src cardmem.cpp; red: add -DCARDMEM_RED (the old rules). Exit 0 = PASS.
// The walk models the Commonwealth's pyramid: 96 x 96 finest tiles of dim 2 from north 95, west -96; a tile
// reads land cells one past each edge (LODGEN_TERRAIN_RING_CELLS 1); 8 workers take tiles round-robin.
#include "lodgenbakemem.h"
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

#ifdef CARDMEM_RED
static const bool kRed = true;
#else
static const bool kRed = false;
#endif

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	int fails = 0;
	const int north = 95, west = -96, dim = 2, tiles = 96, workers = 8, ring = 1;
	std::vector<std::set<std::pair<int, int>>> cache( workers );
	size_t peak = 0;
	long long misses = 0, reads = 0;
	for ( int ty = 0; ty < tiles; ty++ ) {
		const int cellY0 = north - ( ty + 1 ) * dim + 1;
		for ( int tx = 0; tx < tiles; tx++ ) {
			auto & c = cache[size_t( ( tx + ty ) % workers )];
			const int cellX0 = west + tx * dim;
			for ( int y = cellY0 - ring; y < cellY0 + dim + ring; y++ )
				for ( int x = cellX0 - ring; x < cellX0 + dim + ring; x++ ) {
					reads++;
					if ( c.insert( { x, y } ).second )
						misses++;
				}
		}
		size_t held = 0;
		for ( auto & c : cache ) {
			for ( auto it = c.begin(); it != c.end(); )
				it = lodgenVtLandDrop( it->second, ty, north, dim, kRed ) ? c.erase( it ) : std::next( it );
			held += c.size();
		}
		if ( held > peak )
			peak = held;
	}
	// a tile row reaches dim + 2 cell rows; two of them carry over to the next row
	const size_t floorPeak = size_t( workers ) * size_t( tiles * dim + 2 * ring ) * 2;
	std::printf( "land: peak %zu cells over %d workers (floor %zu), reads %lld misses %lld\n", peak, workers,
		floorPeak, reads, misses );
	if ( peak > floorPeak ) {
		std::printf( "FAIL land: the caches keep passed rows\n" );
		fails++;
	}
	// a dropped cell must never be read again: the misses equal a no-drop walk's (the drop costs no reload)
	{
		std::vector<std::set<std::pair<int, int>>> all( workers );
		long long m0 = 0;
		for ( int ty = 0; ty < tiles; ty++ )
			for ( int tx = 0; tx < tiles; tx++ ) {
				auto & c = all[size_t( ( tx + ty ) % workers )];
				const int cellY0 = north - ( ty + 1 ) * dim + 1, cellX0 = west + tx * dim;
				for ( int y = cellY0 - ring; y < cellY0 + dim + ring; y++ )
					for ( int x = cellX0 - ring; x < cellX0 + dim + ring; x++ )
						m0 += c.insert( { x, y } ).second ? 1 : 0;
			}
		std::printf( "land: misses %lld vs keep-all %lld\n", misses, m0 );
		if ( misses != m0 ) {
			std::printf( "FAIL land: the drop threw away cells a later row reads\n" );
			fails++;
		}
	}
	const long long realMB = ( lodgenTexBudgetUnits( 0, kRed ) * kLodgenTexBytesPerUnit ) >> 20;
	const long long envMB = ( lodgenTexBudgetUnits( 1536, kRed ) * kLodgenTexBytesPerUnit ) >> 20;
	std::printf( "tex: default budget %lld MB real, WW_LODGEN_TEXBUDGET_MB=1536 -> %lld MB\n", realMB, envMB );
	if ( realMB > 2048 || ( !kRed && envMB != 1536 ) ) {
		std::printf( "FAIL tex: the default budget is over 2 GiB real\n" );
		fails++;
	}
	std::printf( fails ? "CARDMEM FAIL %d\n" : "CARDMEM PASS\n", fails );
	return fails ? 1 : 0;
}
