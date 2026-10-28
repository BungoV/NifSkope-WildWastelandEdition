#pragma once

/*! lane HALLINDOOR1: the indoor rule's openings. A room's threshold to the outside (a skylight, a window, a door) is a
 *  light path into the room, so a ray that leaves the world, or runs past the reach, through such an opening does not make
 *  the point it starts from outdoor air (ConcordMuseum01's main hall under its glass roof read "near the outdoor limit").
 *  A point in or past the opening still reads by its own rays: only a crossing that starts outside the box counts. */

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

class ProbeVolOpenings
{
public:
	//! one opening box: centre, half sizes, R (local = R^T (p - c))
	void add( const double c[3], const double h[3], const double r[3][3] )
	{
		Box b;
		for ( int i = 0; i < 3; i++ ) {
			b.c[i] = c[i];
			b.h[i] = h[i];
			for ( int j = 0; j < 3; j++ )
				b.r[i][j] = r[i][j];
		}
		for ( int i = 0; i < 3; i++ ) {
			const double e = std::fabs( r[i][0] ) * h[0] + std::fabs( r[i][1] ) * h[1] + std::fabs( r[i][2] ) * h[2];
			b.lo[i] = c[i] - e;
			b.hi[i] = c[i] + e;
		}
		boxes.push_back( b );
	}
	//! after the last add: buckets of `cell` units list the boxes within `reach` of them
	void build( double reach, double cell = 512.0 )
	{
		range = reach;
		bucket = cell;
		grid.clear();
		for ( size_t n = 0; n < boxes.size(); n++ ) {
			const Box & b = boxes[n];
			int k0[3], k1[3];
			for ( int i = 0; i < 3; i++ ) {
				k0[i] = int( std::floor( ( b.lo[i] - reach ) / cell ) );
				k1[i] = int( std::floor( ( b.hi[i] + reach ) / cell ) );
			}
			for ( int x = k0[0]; x <= k1[0]; x++ )
				for ( int y = k0[1]; y <= k1[1]; y++ )
					for ( int z = k0[2]; z <= k1[2]; z++ )
						grid[key( x, y, z )].push_back( int( n ) );
		}
	}
	bool empty() const { return boxes.empty(); }
	size_t size() const { return boxes.size(); }
	//! the ray from o along unit d enters an opening box within min( tmax, reach ), starting outside it
	bool through( const double o[3], const double d[3], double tmax ) const
	{
		if ( boxes.empty() )
			return false;
		const auto it = grid.find( key( int( std::floor( o[0] / bucket ) ), int( std::floor( o[1] / bucket ) ), int( std::floor( o[2] / bucket ) ) ) );
		if ( it == grid.end() )
			return false;
		const double t1max = std::min( tmax, range );
		for ( int n : it->second ) {
			const Box & b = boxes[size_t( n )];
			double lo[3], ld[3];
			for ( int j = 0; j < 3; j++ ) {
				const double q[3] = { o[0] - b.c[0], o[1] - b.c[1], o[2] - b.c[2] };
				lo[j] = b.r[0][j] * q[0] + b.r[1][j] * q[1] + b.r[2][j] * q[2];
				ld[j] = b.r[0][j] * d[0] + b.r[1][j] * d[1] + b.r[2][j] * d[2];
			}
			if ( std::fabs( lo[0] ) <= b.h[0] && std::fabs( lo[1] ) <= b.h[1] && std::fabs( lo[2] ) <= b.h[2] )
				continue;   // the point sits in the opening: its own rays decide
			double t0 = 0, t1 = t1max;
			bool miss = false;
			for ( int j = 0; j < 3 && !miss; j++ ) {
				if ( std::fabs( ld[j] ) < 1e-12 ) {
					miss = std::fabs( lo[j] ) > b.h[j];
					continue;
				}
				double u = ( -b.h[j] - lo[j] ) / ld[j], v = ( b.h[j] - lo[j] ) / ld[j];
				if ( u > v )
					std::swap( u, v );
				t0 = std::max( t0, u );
				t1 = std::min( t1, v );
				miss = t0 > t1;
			}
			if ( !miss )
				return true;
		}
		return false;
	}

private:
	struct Box
	{
		double c[3], h[3], r[3][3], lo[3], hi[3];
	};
	static long long key( int x, int y, int z )
	{
		return ( ( long long )( x & 0x1FFFFF ) << 42 ) | ( ( long long )( y & 0x1FFFFF ) << 21 ) | ( long long )( z & 0x1FFFFF );
	}
	std::vector<Box> boxes;
	std::unordered_map<long long, std::vector<int>> grid;
	double range = 0, bucket = 512;
};
