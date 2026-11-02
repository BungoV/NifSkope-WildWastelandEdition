#pragma once
/* Lane TOOLS1101 (REFLPROBECLI1, 2026-11-01): a census of the reflection probes' captures without a window.
 * The live pass (src/gl/cellreflprobe.cpp) renders each probe's cube from the cell's statics and relights it into an
 * octahedral tile (res/shaders/cell_reflrelight.frag); a texel whose G-buffer MISSED takes the sky above the horizon
 * and, since SKYBELOW2, the ground below it (res/shaders/cell_skybelow.glsl). How many below-horizon misses a real
 * probe has could only be counted in a window; this header traces the same texel directions through a bake's soup
 * (src/probebvh.h, the bake's own tracer) for the probes the pass would place round a camera.
 * MIRRORS (tests/reflcensus/run.sh greps each literal there; a drift fails the gate):
 *   placement  cellreflprobe.cpp: the 3x3 outdoor grid of kBox 2048 boxes round floor(cam/2048), z = camZ +- 1024,
 *              camZ = round(cam.z/256)*256, the probe at the box centre xy and camZ; the camera probe (slot 0) at the
 *              camera snapped to 128, half box 4096,4096,2048 outdoors and 1024,1024,512 indoors.
 *   capture    near 4, far = reach = max(2*|box far corner from the probe|, 4096), PLANAR (the cube face's depth),
 *              so a texel in direction d sees t in [4/m, reach/m], m = max|d_i|.
 *   relight    the 64x64 tile, texel centre uv, rpOctDecode; a miss below the horizon = d.z < 0 (wwSkyBelow);
 *              the ground's albedo from 9 taps (wwSkyBelowTap), 0.5 when none hit ("tapless").
 * NOT mirrored, said in every print: the pass culls casters by bounding sphere and draws only statics that cast a
 * sun shadow, the soup is what the bake saw (statics, alpha holes as the bake's masks, no glass); the rooms placement
 * (an interior with the old PRT GI's rooms) is not modelled, the grid is (RTXGI cells use the grid indoors too). */
#include <algorithm>
#include <cmath>
#include <vector>

#include "probebvh.h"

namespace reflcensus
{
constexpr int kFace = 64;           // cellreflprobe.cpp kFace: the oct tile is kFace x kFace texels
constexpr double kBox = 2048.0;     // cellreflprobe.cpp kBox
constexpr double kNear = 4.0;       // the capture's near plane
constexpr double kReachMin = 4096.0;

enum Red : int {
	RedNone = 0,
	RedLift = 1,      // the horizon at d.z < 0.1 (cell_skybelow.glsl's SKYBELOW2_RED_LIFT): below counts move
	RedNoReach = 2,   // no far plane: every ray runs to the soup's end (hits rise where the reach cut them)
	RedRoof = 4,      // lane INTGI1: --auto --interior puts the cameras on the topmost surface again (the roof)
	RedStand = 8,     // every probe counts as standing in the geometry (no split of the misses past the soup)
};

struct Probe
{
	double pos[3], bmin[3], bmax[3];
	int kind = 1;     // 0 the camera probe (slot 0), 1 a grid box
	int gi = 0, gj = 0;
};

struct Count
{
	long texels = 0, hits = 0, misses = 0, above = 0, below = 0;
	long belowTapless = 0;   // below-horizon misses of a probe none of whose 9 ground taps hit (they take albedo 0.5)
	long belowEdge = 0;      // below-horizon misses whose ray ends past the soup's xy box (census() given soupXY)
	int tapsHit = 0;
	double reach = 0.0;
};

//! the probes the pass places round a camera: the camera probe first, then the 3x3 grid (the pass keeps all 9)
inline void place( const double cam[3], bool interior, std::vector<Probe> & out )
{
	out.clear();
	Probe c;
	c.kind = 0;
	const double half[3] = { interior ? 1024.0 : 4096.0, interior ? 1024.0 : 4096.0, interior ? 512.0 : 2048.0 };
	for ( int a = 0; a < 3; a++ ) {
		c.pos[a] = std::round( float( cam[a] ) / 128.0f ) * 128.0;
		c.bmin[a] = c.pos[a] - half[a];
		c.bmax[a] = c.pos[a] + half[a];
	}
	out.push_back( c );
	const double camZ = std::round( float( cam[2] ) / 256.0f ) * 256.0;
	const int ix = int( std::floor( float( cam[0] ) / float( kBox ) ) ), iy = int( std::floor( float( cam[1] ) / float( kBox ) ) );
	for ( int j = -1; j <= 1; j++ )
		for ( int i = -1; i <= 1; i++ ) {
			Probe p;
			p.kind = 1;
			p.gi = ix + i;
			p.gj = iy + j;
			p.bmin[0] = double( ix + i ) * kBox;
			p.bmin[1] = double( iy + j ) * kBox;
			p.bmin[2] = camZ - kBox * 0.5;
			for ( int a = 0; a < 3; a++ ) {
				p.bmax[a] = p.bmin[a] + kBox;
				p.pos[a] = p.bmin[a] + kBox * 0.5;
			}
			p.pos[2] = camZ;
			out.push_back( p );
		}
}

//! cellreflprobe.cpp's reach: twice the box's far corner from the probe, at least 4096
inline double reachOf( const Probe & p )
{
	double r = 0.0;
	for ( int a = 0; a < 3; a++ ) {
		const double e = std::max( p.bmax[a] - p.pos[a], p.pos[a] - p.bmin[a] );
		r += e * e;
	}
	return std::max( std::sqrt( r ) * 2.0, kReachMin );
}

//! cell_reflrelight.frag rpOctDecode
inline void octDecode( double u, double v, double d[3] )
{
	double fx = u * 2.0 - 1.0, fy = v * 2.0 - 1.0;
	double nz = 1.0 - std::fabs( fx ) - std::fabs( fy );
	const double t = std::max( -nz, 0.0 );
	fx += fx >= 0.0 ? -t : t;
	fy += fy >= 0.0 ? -t : t;
	const double l = std::sqrt( fx * fx + fy * fy + nz * nz );
	d[0] = fx / l;
	d[1] = fy / l;
	d[2] = nz / l;
}

//! cell_skybelow.glsl wwSkyBelowTap
inline void tap( int k, double d[3] )
{
	if ( k == 0 ) {
		d[0] = 0.0; d[1] = 0.0; d[2] = -1.0;
		return;
	}
	if ( k < 5 ) {
		const double c = 0.93969262, s = -0.34202014;
		d[0] = k == 1 ? c : k == 2 ? -c : 0.0;
		d[1] = k == 3 ? c : k == 4 ? -c : 0.0;
		d[2] = s;
		return;
	}
	const double q = 0.35355339;
	d[0] = k == 5 || k == 8 ? q : -q;
	d[1] = k < 7 ? q : -q;
	d[2] = -0.86602540;
}

inline bool below( const double d[3], int red )
{
	return ( red & RedLift ) ? d[2] < 0.1 : d[2] < 0.0;
}

//! does the probe's capture draw something in unit direction d? (local = world minus O in x and y, as the bvh's soup)
inline bool sees( const probebvh::Bvh & bvh, const double O[2], const Probe & p, const double d[3], double reach, int red )
{
	const double m = std::max( { std::fabs( d[0] ), std::fabs( d[1] ), std::fabs( d[2] ) } );
	const double t0 = kNear / m, t1 = ( red & RedNoReach ) ? 1e9 : reach / m;
	const double o[3] = { p.pos[0] - O[0] + d[0] * t0, p.pos[1] - O[1] + d[1] * t0, p.pos[2] + d[2] * t0 };
	double th = 0.0;
	return bvh.ray( o, d, t1 - t0, &th );
}

//! soupXY (optional, world minX,minY,maxX,maxY): count belowEdge, the below misses whose ray ends past the soup
inline Count census( const probebvh::Bvh & bvh, const double O[2], const Probe & p, int red, const double * soupXY = nullptr )
{
	Count c;
	c.reach = reachOf( p );
	double d[3];
	for ( int k = 0; k < 9; k++ ) {
		tap( k, d );
		const double l = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		for ( int a = 0; a < 3; a++ )
			d[a] /= l;
		c.tapsHit += sees( bvh, O, p, d, c.reach, red ) ? 1 : 0;
	}
	for ( int y = 0; y < kFace; y++ )
		for ( int x = 0; x < kFace; x++ ) {
			octDecode( ( x + 0.5 ) / kFace, ( y + 0.5 ) / kFace, d );
			c.texels++;
			if ( sees( bvh, O, p, d, c.reach, red ) ) {
				c.hits++;
				continue;
			}
			c.misses++;
			if ( below( d, red ) ) {
				c.below++;
				if ( c.tapsHit == 0 )
					c.belowTapless++;
				if ( soupXY ) {   // the capture's far end past the soup's xy box: the miss may be the soup's end
					const double m = std::max( { std::fabs( d[0] ), std::fabs( d[1] ), std::fabs( d[2] ) } );
					const double t1 = ( red & RedNoReach ) ? 1e9 : c.reach / m;
					const double ex = p.pos[0] + d[0] * t1, ey = p.pos[1] + d[1] * t1;
					if ( ex < soupXY[0] || ey < soupXY[1] || ex > soupXY[2] || ey > soupXY[3] )
						c.belowEdge++;
				}
			} else {
				c.above++;
			}
		}
	return c;
}

/* lane INTGI1 (REFLPROBECLI2): --auto in an interior. The topmost surface over an interior soup is usually its roof,
 * whose probes look out into the void; the cameras stand in the rooms instead. zs = the surfaces under one column,
 * top down (columnHits). A camera stands eye over each surface that has something above it (not the topmost) with at
 * least eye + kHeadroom free up to that next surface (a floor under a ceiling; a slab's own two faces are too close).
 * Several storeys give several cameras. Red RedRoof: the topmost surface, as REFLPROBECLI1 did. */
constexpr double kHeadroom = 32.0;

inline void floorsOf( const std::vector<double> & zs, double eye, int red, std::vector<double> & camZ )
{
	if ( zs.empty() )
		return;
	if ( red & RedRoof ) {
		camZ.push_back( zs[0] + eye );
		return;
	}
	for ( size_t k = 1; k < zs.size(); k++ )
		if ( zs[k - 1] - zs[k] >= eye + kHeadroom )
			camZ.push_back( zs[k] + eye );
}

// the successive surfaces a downward ray from (x, y, z0) meets within len (local tracer coordinates), top down;
// each ray starts just under the last hit, 64 at most
inline void columnHits( const probebvh::Bvh & bvh, double x, double y, double z0, double len, std::vector<double> & zs )
{
	zs.clear();
	double z = z0, left = len;
	for ( int k = 0; k < 64 && left > 0.0; k++ ) {
		const double o[3] = { x, y, z }, d[3] = { 0.0, 0.0, -1.0 };
		double th = 0.0;
		if ( !bvh.ray( o, d, left, &th ) )
			break;
		zs.push_back( z - th );
		z -= th + 0.5;
		left -= th + 0.5;
	}
}

/* Where a probe stands against the soup, so a miss past the soup's edge can be told from a soup that stops short
 * (Museum, 11-02: 95 % of those misses came from grid probes outside the building). Outside: past the soup's xy box.
 * Void: no surface in the probe's column, under the column's lowest surface, or (interior) over its topmost one. */
enum Stand : int { StandGeometry = 0, StandOutside = 1, StandVoid = 2 };

inline int standOf( const probebvh::Bvh & bvh, const double O[2], const Probe & p, const double soupXY[4], double topZ,
	double len, bool interior, int red )
{
	if ( red & RedStand )
		return StandGeometry;
	if ( p.pos[0] < soupXY[0] || p.pos[1] < soupXY[1] || p.pos[0] > soupXY[2] || p.pos[1] > soupXY[3] )
		return StandOutside;
	std::vector<double> zs;
	columnHits( bvh, p.pos[0] - O[0], p.pos[1] - O[1], topZ, len, zs );
	if ( zs.empty() || p.pos[2] < zs.back() || ( interior && p.pos[2] > zs.front() ) )
		return StandVoid;
	return StandGeometry;
}
}   // namespace reflcensus
