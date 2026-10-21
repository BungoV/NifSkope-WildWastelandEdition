/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellcluster.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>

namespace {

// Slack that keeps the CPU's cluster bounds a superset of what the GPU's float math assigns to the cluster.
constexpr double kPadPx = 1.0;
constexpr double kPadDepth = 1e-3;     // relative, either side of a slice
constexpr double kPadRadius = 1e-4;    // relative

struct Proj
{
	const float * m;
	bool ortho = false;
	double nearZ = 0.0, farZ = 0.0;
	double at( int col, int row ) const { return double( m[col * 4 + row] ); }

	// view-space x, y of the point at NDC (nx, ny) and view depth d (view z = -d)
	void unproject( double nx, double ny, double d, double & x, double & y ) const
	{
		const double zv = -d;
		const double w = at( 2, 3 ) * zv + at( 3, 3 );
		x = ( nx * w - at( 2, 0 ) * zv - at( 3, 0 ) ) / at( 0, 0 );
		y = ( ny * w - at( 2, 1 ) * zv - at( 3, 1 ) ) / at( 1, 1 );
	}
	// NDC of a view-space point; false when it is at or behind the eye
	bool project( const double v[3], double & nx, double & ny ) const
	{
		const double w = at( 2, 3 ) * v[2] + at( 3, 3 );
		if ( w <= 1e-9 )
			return false;
		nx = ( at( 0, 0 ) * v[0] + at( 2, 0 ) * v[2] + at( 3, 0 ) ) / w;
		ny = ( at( 1, 1 ) * v[1] + at( 2, 1 ) * v[2] + at( 3, 1 ) ) / w;
		return true;
	}
};

bool readProj( const float * m, Proj & p )
{
	p.m = m;
	// Only x/y scale, the off-centre (jitter) terms and the depth terms may be set.
	for ( int k : { 1, 2, 4, 6, 3, 7 } )
		if ( m[k] != 0.0f )
			return false;
	if ( p.at( 0, 0 ) == 0.0 || p.at( 1, 1 ) == 0.0 || p.at( 2, 2 ) == 0.0 )
		return false;
	p.ortho = p.at( 3, 3 ) == 1.0 && p.at( 2, 3 ) == 0.0;
	if ( p.ortho ) {
		p.nearZ = ( p.at( 3, 2 ) + 1.0 ) / p.at( 2, 2 );
		p.farZ = ( p.at( 3, 2 ) - 1.0 ) / p.at( 2, 2 );
	} else if ( p.at( 2, 3 ) == -1.0 && p.at( 3, 3 ) == 0.0 ) {
		p.nearZ = p.at( 3, 2 ) / ( p.at( 2, 2 ) - 1.0 );
		p.farZ = p.at( 3, 2 ) / ( p.at( 2, 2 ) + 1.0 );
	} else {
		return false;
	}
	if ( !( p.farZ > 0.0 ) || !std::isfinite( p.farZ ) )
		return false;
	// log slicing needs a positive near plane; an ortho near at or behind the eye starts at far / 1e6
	p.nearZ = std::max( p.nearZ, p.farZ * 1e-6 );
	return p.nearZ < p.farZ;
}

struct Box
{
	double lo[3], hi[3];
};

// The tests run in float: the pads above dwarf its rounding.
struct FBox
{
	float lo[3], hi[3], c[3], rad;  // corners, centre and bounding radius
};

struct FLight
{
	float pos[3], dir[3], r, cosOuter, sinOuter;
	int shape;
};

float dist2( const FBox & b, const float p[3] )
{
	float s = 0.0f;
	for ( int k = 0; k < 3; k++ ) {
		const float d = std::max( std::max( b.lo[k] - p[k], p[k] - b.hi[k] ), 0.0f );
		s += d * d;
	}
	return s;
}

// Conservative: false only when no point of the box can receive light from `l`.
bool touches( const FLight & l, const FBox & b )
{
	if ( dist2( b, l.pos ) > l.r * l.r )
		return false;
	if ( l.shape == 2 ) {
		// the box's highest point along the normal must reach the lit side
		float m = 0.0f;
		for ( int k = 0; k < 3; k++ )
			m += l.dir[k] * ( ( l.dir[k] >= 0.0f ? b.hi[k] : b.lo[k] ) - l.pos[k] );
		return m >= -1e-4f * l.r;
	}
	if ( l.shape == 1 && l.cosOuter > 0.0f ) {
		// cone against the box's bounding sphere (the sphere-cone test of Wronski 2016, "Cull that cone!")
		const float sr = b.rad + 1e-4f * l.r;
		float len2 = 0.0f, along = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			const float v = b.c[k] - l.pos[k];
			len2 += v * v;
			along += v * l.dir[k];
		}
		const float across = std::sqrt( std::max( 0.0f, len2 - along * along ) );
		if ( l.cosOuter * across - along * l.sinOuter > sr || along < -sr )
			return false;
	}
	return true;
}

// The tiles the part of a sphere inside the depth slab [dLo, dHi] can cover; false when it covers none.
bool screenRect( const Proj & P, const WwClusterGrid & g, const double pos[3], double r, double dLo, double dHi,
	const int vp[4], int & tx0, int & tx1, int & ty0, int & ty1 )
{
	tx0 = 0, tx1 = g.tilesX - 1, ty0 = 0, ty1 = g.tilesY - 1;
	const double zc = -pos[2];
	const double zLo = std::max( zc - r, dLo ), zHi = std::min( zc + r, dHi );
	if ( zLo > zHi )
		return false;
	// the widest cross-section of the sphere inside the slab
	const double gap = zc < zLo ? zLo - zc : zc > zHi ? zc - zHi : 0.0;
	const double rc = std::sqrt( std::max( 0.0, r * r - gap * gap ) );
	double lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };
	for ( int c = 0; c < 8; c++ ) {
		const double v[3] = { pos[0] + ( c & 1 ? rc : -rc ), pos[1] + ( c & 2 ? rc : -rc ), -( c & 4 ? zHi : zLo ) };
		double nx, ny;
		if ( !P.project( v, nx, ny ) )
			return true;    // reaches the eye plane: every tile
		lo[0] = std::min( lo[0], nx ), hi[0] = std::max( hi[0], nx );
		lo[1] = std::min( lo[1], ny ), hi[1] = std::max( hi[1], ny );
	}
	const double px = 2.0 * kPadPx / vp[2], py = 2.0 * kPadPx / vp[3];
	if ( hi[0] + px < -1.0 || lo[0] - px > 1.0 || hi[1] + py < -1.0 || lo[1] - py > 1.0 )
		return false;
	auto tile = [&]( double ndc, double size, int n ) {
		return std::clamp( int( std::floor( ( ndc + 1.0 ) * 0.5 * size / g.tilePx ) ), 0, n - 1 );
	};
	tx0 = tile( lo[0] - px, vp[2], g.tilesX ), tx1 = tile( hi[0] + px, vp[2], g.tilesX );
	ty0 = tile( lo[1] - py, vp[3], g.tilesY ), ty1 = tile( hi[1] + py, vp[3], g.tilesY );
	return true;
}

} // namespace

int WwClusterGrid::clusterOf( float fx, float fy, float depth ) const
{
	// float math, as the shader does it (cell_cluster.glsl cellClusterIndexOf)
	const int tx = std::clamp( int( std::floor( ( fx - float( vp[0] ) ) / float( tilePx ) ) ), 0, tilesX - 1 );
	const int ty = std::clamp( int( std::floor( ( fy - float( vp[1] ) ) / float( tilePx ) ) ), 0, tilesY - 1 );
	const float s = std::floor( ( std::log( std::max( depth, 1e-6f ) ) - logNear ) * sliceScale );
	const int sz = std::clamp( int( std::isfinite( s ) ? s : 0.0f ), 0, slices - 1 );
	return ( sz * tilesY + ty ) * tilesX + tx;
}

bool wwClusterBuild( const float proj[16], const int vp[4], const std::vector<WwClusterLight> & lights,
	const WwClusterParams & params, WwClusterGrid & out )
{
	const auto t0 = std::chrono::steady_clock::now();
	out = WwClusterGrid();
	Proj P;
	if ( !readProj( proj, P ) || vp[2] <= 0 || vp[3] <= 0 || params.tilePx <= 0 || params.slices <= 0 )
		return false;
	WwClusterGrid & g = out;
	std::copy( vp, vp + 4, g.vp );
	g.tilePx = params.tilePx;
	g.slices = params.slices;
	g.tilesX = ( vp[2] + g.tilePx - 1 ) / g.tilePx;
	g.tilesY = ( vp[3] + g.tilePx - 1 ) / g.tilePx;
	g.logNear = float( std::log( P.nearZ ) );
	g.sliceScale = float( double( g.slices ) / std::log( P.farZ / P.nearZ ) );

	// slice k covers depth near * (far / near)^(k / S) .. ^((k + 1) / S); the end slices take everything beyond
	std::vector<double> sliceLo( size_t( g.slices ) ), sliceHi( size_t( g.slices ) );
	for ( int k = 0; k < g.slices; k++ ) {
		const double a = P.nearZ * std::pow( P.farZ / P.nearZ, double( k ) / g.slices );
		const double b = P.nearZ * std::pow( P.farZ / P.nearZ, double( k + 1 ) / g.slices );
		sliceLo[size_t( k )] = a * ( 1.0 - kPadDepth );
		sliceHi[size_t( k )] = b * ( 1.0 + kPadDepth );
		if ( params.redMidSlice )
			sliceLo[size_t( k )] = sliceHi[size_t( k )] = 0.5 * ( a + b );
	}
	if ( !params.redMidSlice ) {
		sliceLo[0] = P.ortho ? -1e30 : 0.0;
		sliceHi[size_t( g.slices - 1 )] = 1e30;
	}
	auto ndcX = [&]( double px ) { return 2.0 * px / double( vp[2] ) - 1.0; };
	auto ndcY = [&]( double py ) { return 2.0 * py / double( vp[3] ) - 1.0; };
	auto clusterBox = [&]( int tx, int ty, int sz ) {
		const double nx[2] = { ndcX( tx * g.tilePx - kPadPx ), ndcX( ( tx + 1 ) * g.tilePx + kPadPx ) };
		const double ny[2] = { ndcY( ty * g.tilePx - kPadPx ), ndcY( ( ty + 1 ) * g.tilePx + kPadPx ) };
		const double d[2] = { sliceLo[size_t( sz )], sliceHi[size_t( sz )] };
		Box b = { { 1e300, 1e300, 1e300 }, { -1e300, -1e300, -1e300 } };
		for ( double dd : d )
			for ( double xx : nx )
				for ( double yy : ny ) {
					double v[3];
					P.unproject( xx, yy, dd, v[0], v[1] );
					v[2] = -dd;
					for ( int k = 0; k < 3; k++ ) {
						b.lo[k] = std::min( b.lo[k], v[k] );
						b.hi[k] = std::max( b.hi[k], v[k] );
					}
				}
		return b;
	};
	std::vector<FBox> boxes( size_t( g.clusterCount() ) );
	for ( int sz = 0; sz < g.slices; sz++ )
		for ( int ty = 0; ty < g.tilesY; ty++ )
			for ( int tx = 0; tx < g.tilesX; tx++ ) {
				const Box b = clusterBox( tx, ty, sz );
				FBox & f = boxes[size_t( ( sz * g.tilesY + ty ) * g.tilesX + tx )];
				double rad2 = 0.0;
				for ( int k = 0; k < 3; k++ ) {
					// float rounding of the corners goes outward
					f.lo[k] = std::nextafter( float( b.lo[k] ), -HUGE_VALF );
					f.hi[k] = std::nextafter( float( b.hi[k] ), HUGE_VALF );
					f.c[k] = float( 0.5 * ( b.lo[k] + b.hi[k] ) );
					rad2 += 0.25 * ( b.hi[k] - b.lo[k] ) * ( b.hi[k] - b.lo[k] );
				}
				f.rad = float( std::sqrt( rad2 ) * ( 1.0 + kPadRadius ) );
			}
	auto sliceOf = [&]( double depth ) {
		const double s = std::floor( ( std::log( std::max( depth, 1e-30 ) ) - std::log( P.nearZ ) ) * g.slices
			/ std::log( P.farZ / P.nearZ ) );
		return int( std::clamp( s, 0.0, double( g.slices - 1 ) ) );
	};

	// (cluster, light) pairs per contiguous chunk of lights; merged chunk by chunk, each cluster's list stays
	// in ascending light order, so the shader's sum keeps the flat loop's order
	struct Chunk
	{
		std::vector<std::int32_t> cluster, light;
		int offscreen = 0;
	};
	const int threads = lights.size() < 128 || std::getenv( "WW_CELL_CLUSTER_THREADS1" ) ? 1
		: int( std::clamp( std::thread::hardware_concurrency(), 1u, 8u ) );
	std::vector<Chunk> chunks( static_cast<size_t>( threads ) );
	auto work = [&]( int t ) {
		Chunk & ch = chunks[size_t( t )];
		const size_t i0 = lights.size() * size_t( t ) / size_t( threads );
		const size_t i1 = lights.size() * size_t( t + 1 ) / size_t( threads );
		ch.cluster.reserve( ( i1 - i0 ) * 64 );
		ch.light.reserve( ( i1 - i0 ) * 64 );
		for ( size_t i = i0; i < i1; i++ ) {
			const WwClusterLight & l = lights[i];
			if ( !( l.radius > 0.0 ) )
				continue;
			const double r = l.radius * ( 1.0 + kPadRadius );
			const double dNear = -( l.pos[2] + r ), dFar = -( l.pos[2] - r );
			if ( !P.ortho && dFar <= 0.0 ) {
				ch.offscreen++;
				continue;
			}
			// padded like the slices, so a fragment the GPU rounds into the next slice still finds the light
			const int s0 = sliceOf( dNear * ( dNear > 0.0 ? 1.0 - kPadDepth : 1.0 + kPadDepth ) );
			const int s1 = sliceOf( dFar * ( 1.0 + kPadDepth ) );
			FLight fl;
			for ( int k = 0; k < 3; k++ ) {
				fl.pos[k] = float( l.pos[k] );
				fl.dir[k] = float( l.dir[k] );
			}
			fl.r = float( r );
			fl.shape = l.shape;
			fl.cosOuter = float( l.cosOuter );
			fl.sinOuter = float( std::sqrt( std::max( 0.0, 1.0 - l.cosOuter * l.cosOuter ) ) );
			bool any = false;
			for ( int sz = s0; sz <= s1; sz++ ) {
				int tx0, tx1, ty0, ty1;
				if ( !screenRect( P, g, l.pos, r, sliceLo[size_t( sz )], sliceHi[size_t( sz )], vp, tx0, tx1, ty0, ty1 ) )
					continue;
				any = true;
				for ( int ty = ty0; ty <= ty1; ty++ ) {
					const int row = ( sz * g.tilesY + ty ) * g.tilesX;
					int a = tx0, b = tx1;
					if ( fl.shape == 0 ) {
						// a sphere meets a run of a row: test in from both ends, take the middle untested
						// (an extra light in a list costs time, never a wrong sum)
						while ( a <= b && !touches( fl, boxes[size_t( row + a )] ) )
							a++;
						while ( b > a && !touches( fl, boxes[size_t( row + b )] ) )
							b--;
					}
					for ( int tx = a; tx <= b; tx++ ) {
						if ( fl.shape != 0 && !touches( fl, boxes[size_t( row + tx )] ) )
							continue;
						ch.cluster.push_back( row + tx );
						ch.light.push_back( std::int32_t( i ) );
					}
				}
			}
			ch.offscreen += !any;
		}
	};
	if ( threads == 1 ) {
		work( 0 );
	} else {
		std::vector<std::thread> pool;
		for ( int t = 0; t < threads; t++ )
			pool.emplace_back( work, t );
		for ( std::thread & th : pool )
			th.join();
	}
	std::vector<std::int32_t> count( size_t( g.clusterCount() ), 0 );
	for ( const Chunk & ch : chunks ) {
		g.culledOffscreen += ch.offscreen;
		for ( std::int32_t c : ch.cluster )
			count[size_t( c )]++;
	}
	g.cells.assign( size_t( g.clusterCount() ) * 2, 0 );
	std::vector<std::int32_t> at( size_t( g.clusterCount() ), 0 );
	std::int32_t offset = 0;
	for ( int c = 0; c < g.clusterCount(); c++ ) {
		const std::int32_t n = count[size_t( c )];
		g.maxCount = std::max( g.maxCount, int( n ) );
		if ( n > params.maxPerCluster ) {
			g.overflowed++;
			g.cells[size_t( c ) * 2 + 1] = -1;
			at[size_t( c )] = -1;
			continue;
		}
		g.cells[size_t( c ) * 2] = offset;
		g.cells[size_t( c ) * 2 + 1] = n;
		at[size_t( c )] = offset;
		offset += n;
	}
	g.index.resize( size_t( offset ) );
	for ( const Chunk & ch : chunks )
		for ( size_t k = 0; k < ch.cluster.size(); k++ ) {
			std::int32_t & w = at[size_t( ch.cluster[k] )];
			if ( w >= 0 )
				g.index[size_t( w++ )] = ch.light[k];
		}
	g.refs = offset;
	g.buildMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
	return true;
}
