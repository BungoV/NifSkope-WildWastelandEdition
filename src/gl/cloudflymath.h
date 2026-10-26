#pragma once

#include <algorithm>
#include <cmath>

/* lane CLOUD4: the fly-through grid's numbers (no GL, no Qt: src/gl/cloudfly.cpp and tests/cloudfly/cloudfly_dump.cpp).
 * A camera-centred box of cells over the cloud band holds an occupancy bound of the density (cloud_flybake.frag) and its
 * Chebyshev distance in cells (cloud_flysdf.frag, capped at kCap). The march (cloud_fly.frag) steps finely inside the
 * box and jumps the empty cells, keeping kSafe cells plus the grid's age (wind, camera) away from any occupied one.
 * After Schneider, "Nubis Cubed", SIGGRAPH 2023 Advances (a voxel bound + a signed distance field to skip empty space;
 * the talk's slide numbers and constants are not known here: these numbers are ours). */

namespace WwCloudFlyMath
{

struct Preset
{
	int nxy, nz;	// cells across, cells high
	float cellKm;	// one cell's side
	float stepKm;	// the fine step inside the box (cellKm / 4: a jump of whole cells lands on the fine steps)
	int maxSteps;	// the fine loop's cap; the plain march takes the rest of the ray
};

// Low .. Ultra (index 1..4); 0 = off
inline const Preset & preset( int tier )
{
	static const Preset p[5] = { { 64, 16, 0.128f, 0.032f, 64 }, { 64, 16, 0.128f, 0.032f, 64 }, { 80, 20, 0.096f, 0.024f, 96 },
		{ 128, 32, 0.064f, 0.016f, 128 }, { 160, 40, 0.048f, 0.012f, 192 } };
	return p[std::clamp( tier, 0, 4 )];
}

constexpr int kCap = 15;	// the distance field's cap, cells (R8 stores n / 255)
constexpr int kSafe = 2;	// cells kept clear of an occupied one (the in-cell position, the earth's curve over the box)
constexpr float kNearKm = 0.5f;	// the grid is built while the camera is within this of a band
constexpr float kMarginS = 0.05f;	// added to the shape noise in the bound
constexpr float kSlackKm = 0.25f;	// the map's coverage is bounded over this much drift (its scroll between rebuilds)
constexpr int kFrames = 32;	// a rolling rebuild: bake, x, y, z passes over this many frames
constexpr int kStaleCells = 8;	// an older grid than this is rebuilt at once

//! layers drawn a frame by one rolling pass (4 passes x nz layers over kFrames)
inline int layersPerFrame( int nz )
{
	return std::max( 1, ( 4 * nz + kFrames - 1 ) / kFrames );
}

//! the camera is near the band span [lo, hi] (km above z = 0)
inline bool nearBand( float camZ, float lo, float hi )
{
	return hi > lo && camZ > lo - kNearKm && camZ < hi + kNearKm;
}

//! the box's corner (km): x, y centred on the camera on the cell lattice; z over the camera, held inside [lo, hi]
inline void place( const Preset & p, const float camKm[3], float lo, float hi, float org[3] )
{
	const float c = p.cellKm;
	org[0] = ( std::floor( camKm[0] / c ) - float( p.nxy / 2 ) ) * c;
	org[1] = ( std::floor( camKm[1] / c ) - float( p.nxy / 2 ) ) * c;
	const float h = float( p.nz ) * c;
	const float z = std::clamp( camKm[2] - 0.5f * h, lo, std::max( lo, hi - h ) );
	org[2] = std::floor( z / c ) * c;
}

//! the camera is far enough from the box's centre that a new placement is due (a quarter of the box)
inline bool replace( const Preset & p, const float org[3], const float camKm[3], float lo, float hi )
{
	float now[3];
	place( p, camKm, lo, hi, now );
	const float q = 0.25f * float( p.nxy ) * p.cellKm, qz = 0.25f * float( p.nz ) * p.cellKm;
	return std::fabs( now[0] - org[0] ) > q || std::fabs( now[1] - org[1] ) > q || std::fabs( now[2] - org[2] ) > qz;
}

/* cells the field may have moved since the bake: the wind (kWindKmS x the clock's age, along x) and the camera's move
 * in x, y (the cloud map follows the camera, cloudmap.cpp) */
inline int ageCells( const Preset & p, double ageSecs, float windKmS, float camMoveKm )
{
	const double km = std::fabs( ageSecs ) * double( windKmS ) + double( std::fabs( camMoveKm ) );
	return int( std::ceil( km / double( p.cellKm ) - 1e-6 ) );
}

//! bytes held: occupancy, ping-pong and two published distance fields, R8
inline double memoryBytes( const Preset & p )
{
	return 4.0 * double( p.nxy ) * p.nxy * p.nz;
}

/* the exact Chebyshev distance (cells) of every cell to the nearest occupied one, capped at kCap, out-of-grid cells
 * empty: the reference cloud_flysdf.frag must equal (three separable passes x, y, z) */
inline void chebyshev( const unsigned char * occ, int nx, int ny, int nz, unsigned char * out )
{
	const int n = nx * ny * nz;
	auto at = [&]( int x, int y, int z ) { return ( z * ny + y ) * nx + x; };
	unsigned char * a = new unsigned char[n];
	unsigned char * b = new unsigned char[n];
	for ( int z = 0; z < nz; z++ )
		for ( int y = 0; y < ny; y++ )
			for ( int x = 0; x < nx; x++ ) {
				int best = kCap;
				for ( int k = -kCap; k <= kCap; k++ ) {
					const int xx = x + k;
					if ( xx >= 0 && xx < nx && occ[at( xx, y, z )] )
						best = std::min( best, std::abs( k ) );
				}
				a[at( x, y, z )] = (unsigned char) best;
			}
	for ( int pass = 0; pass < 2; pass++ ) {
		const unsigned char * src = pass == 0 ? a : b;
		unsigned char * dst = pass == 0 ? b : out;
		for ( int z = 0; z < nz; z++ )
			for ( int y = 0; y < ny; y++ )
				for ( int x = 0; x < nx; x++ ) {
					int best = kCap;
					for ( int k = -kCap; k <= kCap; k++ ) {
						const int yy = pass == 0 ? y + k : y, zz = pass == 0 ? z : z + k;
						if ( yy < 0 || yy >= ny || zz < 0 || zz >= nz )
							continue;
						best = std::min( best, std::max( std::abs( k ), int( src[at( x, yy, zz )] ) ) );
					}
					dst[at( x, y, z )] = (unsigned char) best;
				}
	}
	delete[] a;
	delete[] b;
}

}	// namespace WwCloudFlyMath
