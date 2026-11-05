#ifndef PRECIPROOF_H
#define PRECIPROOF_H

/* lane INTSKY1 (bungo 11-01, LATE ITEM 30b: "rain/snow falling through open roofs, only under open sky, not under a
 * roof"): a top-down roof height map of a sunlit interior. Each texel keeps the HIGHEST occluder z over it (every
 * triangle of the cell's solid shapes rasterised from above, max z; a triangle smaller than a texel or standing
 * vertical splats its corners and centre). A particle falls in the open when it is above that height: under a roof,
 * a balcony or a glass skylight it is dropped; in the museum's open atrium it falls to the floor. The game's own
 * precipitation occlusion is a top-down depth map too (WTHR DATA "rain occlusion" flag); this is its CPU form, built
 * once per cell (the interior's shapes do not move). Pure header: tests/features/weather/preciproof checks it. */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

struct WwRoofMap
{
	float x0 = 0.0f, y0 = 0.0f, cell = 32.0f;
	int nx = 0, ny = 0;
	std::vector<float> z;	// -inf = nothing above (open)

	bool valid() const { return nx > 0 && ny > 0; }

	//! the map over [x0, x1] x [y0, y1], texel `cellSize`, grown so neither side passes maxSide texels
	void reset( float minX, float minY, float maxX, float maxY, float cellSize = 32.0f, int maxSide = 1024 )
	{
		cell = std::max( cellSize, std::max( maxX - minX, maxY - minY ) / float( std::max( maxSide, 1 ) ) );
		x0 = minX - cell;
		y0 = minY - cell;
		nx = int( std::ceil( ( maxX - minX ) / cell ) ) + 3;
		ny = int( std::ceil( ( maxY - minY ) / cell ) ) + 3;
		z.assign( size_t( nx ) * size_t( ny ), -INFINITY );
	}

	void splat( float x, float y, float h )
	{
		const int i = int( std::floor( ( x - x0 ) / cell ) ), j = int( std::floor( ( y - y0 ) / cell ) );
		if ( i < 0 || j < 0 || i >= nx || j >= ny )
			return;
		float & d = z[size_t( j ) * size_t( nx ) + size_t( i )];
		d = std::max( d, h );
	}

	//! one triangle (world): every texel centre inside it takes its height there; corners + centre always splat
	void addTri( const float a[3], const float b[3], const float c[3] )
	{
		splat( a[0], a[1], a[2] );
		splat( b[0], b[1], b[2] );
		splat( c[0], c[1], c[2] );
		splat( ( a[0] + b[0] + c[0] ) / 3.0f, ( a[1] + b[1] + c[1] ) / 3.0f, std::max( { a[2], b[2], c[2] } ) );
		const float den = ( b[1] - c[1] ) * ( a[0] - c[0] ) + ( c[0] - b[0] ) * ( a[1] - c[1] );
		if ( std::fabs( den ) < 1e-6f )
			return;	// seen edge-on from above (a wall): its corners stand for it
		const float mnx = std::min( { a[0], b[0], c[0] } ), mxx = std::max( { a[0], b[0], c[0] } );
		const float mny = std::min( { a[1], b[1], c[1] } ), mxy = std::max( { a[1], b[1], c[1] } );
		const int i0 = std::max( 0, int( std::floor( ( mnx - x0 ) / cell - 0.5f ) ) ), i1 = std::min( nx - 1, int( std::ceil( ( mxx - x0 ) / cell - 0.5f ) ) );
		const int j0 = std::max( 0, int( std::floor( ( mny - y0 ) / cell - 0.5f ) ) ), j1 = std::min( ny - 1, int( std::ceil( ( mxy - y0 ) / cell - 0.5f ) ) );
		for ( int j = j0; j <= j1; j++ )
			for ( int i = i0; i <= i1; i++ ) {
				const float px = x0 + ( float( i ) + 0.5f ) * cell, py = y0 + ( float( j ) + 0.5f ) * cell;
				const float l1 = ( ( b[1] - c[1] ) * ( px - c[0] ) + ( c[0] - b[0] ) * ( py - c[1] ) ) / den;
				const float l2 = ( ( c[1] - a[1] ) * ( px - c[0] ) + ( a[0] - c[0] ) * ( py - c[1] ) ) / den;
				const float l3 = 1.0f - l1 - l2;
				if ( l1 < 0.0f || l2 < 0.0f || l3 < 0.0f )
					continue;
				float & d = z[size_t( j ) * size_t( nx ) + size_t( i )];
				d = std::max( d, l1 * a[2] + l2 * b[2] + l3 * c[2] );
			}
	}

	//! the highest occluder over (x, y): -inf = open sky (outside the map counts as open)
	float roofAt( float x, float y ) const
	{
		const int i = int( std::floor( ( x - x0 ) / cell ) ), j = int( std::floor( ( y - y0 ) / cell ) );
		if ( !valid() || i < 0 || j < 0 || i >= nx || j >= ny )
			return -INFINITY;
		return z[size_t( j ) * size_t( nx ) + size_t( i )];
	}

	//! a particle at p falls in the open (nothing above it)
	bool open( const double p[3] ) const { return double( roofAt( float( p[0] ), float( p[1] ) ) ) < p[2]; }

	//! the share of the map's texels no shape covers at all (census only)
	double openShare() const
	{
		if ( z.empty() )
			return 0.0;
		size_t o = 0;
		for ( float v : z )
			o += std::isinf( v ) ? 1 : 0;
		return double( o ) / double( z.size() );
	}
};

/* lane BUGS1102 (PRECIPROOF-GPU) made a roofed interior take the CPU field: the GPU field (PERFFIX3 D,
 * cell_precip_field.glsl) had no roof map and rained under the roof. Lane BUGS1102B (PRECIPROOF-GPU2): the GPU field reads
 * the same map as a texture (pcRoof, texel rule of roofAt), so a roofed interior keeps the GPU field too.
 * redCpuRoof (WW_CELL_PRECIP_RED=cpuroof) = BUGS1102's rule: a roofed interior takes the CPU field. */
inline bool wwPrecipGpuField( bool cpuForced, bool noWrapRed, bool roofed, bool redCpuRoof )
{
	return !cpuForced && !noWrapRed && ( !roofed || !redCpuRoof );
}

//! the GPU field tests the roof map; redGpuRoof (WW_CELL_PRECIP_RED=gpuroof) = the GPU field with no roof test (pre-BUGS1102)
inline bool wwPrecipGpuRoofTest( bool gpuField, bool roofed, bool redGpuRoof )
{
	return gpuField && roofed && !redGpuRoof;
}

//! a particle at p is drawn: noRoofTest = the GPU field without its roof test; otherwise a roof drops it
inline bool wwPrecipDrawn( const WwRoofMap * roof, bool noRoofTest, const double p[3] )
{
	return noRoofTest || !roof || roof->open( p );
}

//! the map's texel as the GPU field reads it (R32F): open (-inf) uploads as -1e30, which every particle is above
inline float wwRoofTexel( float z )
{
	return std::isinf( z ) ? -1e30f : z;
}

//! the GPU field's pcRoofCam: the eye in the map's texels (in double, then float) and its z; pcRoofInv = 1 / m.cell
inline void wwRoofCam( const WwRoofMap & m, const double cam[3], float out[3] )
{
	out[0] = float( ( cam[0] - double( m.x0 ) ) / double( m.cell ) );
	out[1] = float( ( cam[1] - double( m.y0 ) ) / double( m.cell ) );
	out[2] = float( cam[2] );
}

#endif // PRECIPROOF_H
