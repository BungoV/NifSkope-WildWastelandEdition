#ifndef LARGEREF_H
#define LARGEREF_H

/* lane GEOLOD1 (LARGEREF1): the worldspace's large references (WRLD RNAM), std only so tests/largeref builds it with g++.
 * One RNAM subrecord = int16 gridY, int16 gridX, uint32 count, then count x { uint32 ref, int16 cellY, int16 cellX }.
 * Measured on Fallout4.esm Commonwealth: read Y,X, 63954 of 63954 rows put the ref in the cell its position says; read X,Y
 * only 911 do (the red). The rule: a large ref draws full while any cell whose RNAM lists it is loaded. */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

namespace LargeRef
{

struct Row
{
	int gx = 0, gy = 0;   //!< the grid cell whose list holds the ref
	uint32_t ref = 0;     //!< file-local form id (the caller maps it)
	int rx = 0, ry = 0;   //!< the cell the ref stands in
};

inline int16_t i16( const unsigned char * p )
{
	int16_t v;
	std::memcpy( &v, p, 2 );
	return v;
}

inline uint32_t u32( const unsigned char * p )
{
	uint32_t v;
	std::memcpy( &v, p, 4 );
	return v;
}

//! Append one RNAM subrecord's rows; yx = the measured order (false = the red X,Y reading). Returns rows read, -1 if short.
inline int decode( const unsigned char * d, size_t n, bool yx, std::vector<Row> & out )
{
	if ( n < 8 )
		return -1;
	const int a = i16( d ), b = i16( d + 2 );
	const uint32_t cnt = u32( d + 4 );
	if ( size_t( cnt ) * 8 + 8 > n )
		return -1;
	for ( uint32_t k = 0; k < cnt; k++ ) {
		const unsigned char * p = d + 8 + 8 * size_t( k );
		Row r;
		r.ref = u32( p );
		const int c = i16( p + 4 ), e = i16( p + 6 );
		r.gx = yx ? b : a;
		r.gy = yx ? a : b;
		r.rx = yx ? e : c;
		r.ry = yx ? c : e;
		out.push_back( r );
	}
	return int( cnt );
}

//! The refs that cells x0..x1, y0..y1 list and that stand outside that rectangle: one row per ref (first seen).
inline std::vector<Row> outside( const std::vector<Row> & rows, int x0, int y0, int x1, int y1 )
{
	std::vector<Row> out;
	std::set<uint32_t> seen;
	auto in = [&]( int x, int y ) { return x >= x0 && x <= x1 && y >= y0 && y <= y1; };
	for ( const Row & r : rows ) {
		if ( !in( r.gx, r.gy ) || in( r.rx, r.ry ) || !seen.insert( r.ref ).second )
			continue;
		out.push_back( r );
	}
	return out;
}

}

#endif
