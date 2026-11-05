#pragma once
/* Lane REFLPLACE1 (REFLPROBEPLACE-INT, 2026-11-02): the interior grid reflection probes stand in the rooms.
 * The live pass (src/features/reflections/cellreflprobe.cpp) puts a 3x3 grid of 2048 boxes round the camera, each probe at its box's
 * centre xy and camZ = round(cam.z/256)*256, indoors too (RTXGI cells have no PRT rooms). In the Museum 95 % of the
 * below-horizon misses past the soup came from grid probes outside the building, in an empty column or under the
 * lowest floor (BUGS1102E, reflcensus stand split). This fits each interior grid probe into the cell's own geometry,
 * the soup the GI keeps (no new bake): a top-down footprint of the soup (per 32-unit column the lowest and the topmost
 * surface under the column's centre, as reflcensus.h columnHits sees them), then
 *   kept   the probe's column has a surface under and over it: it stays where it is;
 *   moved  else the room column (a floor and a ceiling at least 2 x kAir apart) nearest the box's centre whose air
 *          overlaps the box's z: the probe goes to that column's centre, z = camZ clamped into the air;
 *   dropped no such column in the box: the box holds no room, its probe is not placed.
 * The box itself does not move (the parallax box). Red WW_CELL_REFLPLACE_RED=1 (reflcensus --fit --red place): the
 * old placement. Header-only, no Qt: tests/features/reflections/reflcensus builds it with g++ alone. */
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace reflplace
{
constexpr double kCell = 32.0;          // the footprint's column (soup_footprint.py's 32 u)
constexpr double kAir = 64.0;           // a probe keeps this far off the floor and the ceiling
constexpr std::size_t kMaxCells = 4u << 20;   // a bigger soup takes coarser columns

struct Footprint
{
	double x0 = 0.0, y0 = 0.0, cell = kCell;
	int nx = 0, ny = 0;
	double box[4] = { 0.0, 0.0, 0.0, 0.0 };   // the soup's xy box: minX, minY, maxX, maxY
	std::vector<float> zmin, zmax;            // per column; zmin > zmax = nothing under the column's centre
	std::size_t tris = 0;

	bool ok() const { return nx > 0 && ny > 0; }
	bool covered( int i, int j ) const { const std::size_t k = std::size_t( j ) * nx + i; return zmin[k] <= zmax[k]; }
	double cx( int i ) const { return x0 + ( i + 0.5 ) * cell; }
	double cy( int j ) const { return y0 + ( j + 0.5 ) * cell; }
};

//! the footprint of a soup (9 floats a triangle, world units): per column centre every triangle whose xy covers it
inline void build( const float * t, std::size_t nTri, Footprint & F )
{
	F = Footprint();
	F.tris = nTri;
	if ( !t || nTri == 0 )
		return;
	double mn[2] = { 1e300, 1e300 }, mx[2] = { -1e300, -1e300 };
	for ( std::size_t i = 0; i < nTri * 3; i++ )
		for ( int a = 0; a < 2; a++ ) {
			mn[a] = std::min( mn[a], double( t[i * 3 + a] ) );
			mx[a] = std::max( mx[a], double( t[i * 3 + a] ) );
		}
	F.box[0] = mn[0]; F.box[1] = mn[1]; F.box[2] = mx[0]; F.box[3] = mx[1];
	F.x0 = mn[0];
	F.y0 = mn[1];
	for ( ;; ) {
		F.nx = std::max( 1, int( std::ceil( ( mx[0] - mn[0] ) / F.cell ) ) );
		F.ny = std::max( 1, int( std::ceil( ( mx[1] - mn[1] ) / F.cell ) ) );
		if ( std::size_t( F.nx ) * std::size_t( F.ny ) <= kMaxCells )
			break;
		F.cell *= 2.0;
	}
	const std::size_t n = std::size_t( F.nx ) * std::size_t( F.ny );
	F.zmin.assign( n, 3.4e38f );
	F.zmax.assign( n, -3.4e38f );
	for ( std::size_t k = 0; k < nTri; k++ ) {
		const float * p = t + k * 9;
		const double ax = p[0], ay = p[1], az = p[2], bx = p[3], by = p[4], bz = p[5], qx = p[6], qy = p[7], qz = p[8];
		const double den = ( by - qy ) * ( ax - qx ) + ( qx - bx ) * ( ay - qy );
		if ( std::fabs( den ) < 1e-9 )
			continue;   // on edge from above (a wall): a downward ray at a column's centre does not meet it
		const double lo[2] = { std::min( { ax, bx, qx } ), std::min( { ay, by, qy } ) };
		const double hi[2] = { std::max( { ax, bx, qx } ), std::max( { ay, by, qy } ) };
		const int i0 = std::max( 0, int( std::ceil( ( lo[0] - F.x0 ) / F.cell - 0.5 ) ) );
		const int i1 = std::min( F.nx - 1, int( std::floor( ( hi[0] - F.x0 ) / F.cell - 0.5 ) ) );
		const int j0 = std::max( 0, int( std::ceil( ( lo[1] - F.y0 ) / F.cell - 0.5 ) ) );
		const int j1 = std::min( F.ny - 1, int( std::floor( ( hi[1] - F.y0 ) / F.cell - 0.5 ) ) );
		for ( int j = j0; j <= j1; j++ )
			for ( int i = i0; i <= i1; i++ ) {
				const double x = F.cx( i ), y = F.cy( j );
				const double u = ( ( by - qy ) * ( x - qx ) + ( qx - bx ) * ( y - qy ) ) / den;
				const double v = ( ( qy - ay ) * ( x - qx ) + ( ax - qx ) * ( y - qy ) ) / den;
				const double w = 1.0 - u - v;
				if ( u < 0.0 || v < 0.0 || w < 0.0 )
					continue;
				const float z = float( u * az + v * bz + w * qz );
				const std::size_t c = std::size_t( j ) * F.nx + i;
				F.zmin[c] = std::min( F.zmin[c], z );
				F.zmax[c] = std::max( F.zmax[c], z );
			}
	}
}

enum Fit : int { Kept = 0, Moved = 1, Dropped = 2 };

/* fit one interior grid probe (pos in/out; the box bmin/bmax as placed). Kept: its column has a surface under it and
 * one over it. Moved: the nearest room column in the box (see the header). Dropped: none. */
inline int fit( const Footprint & F, const double bmin[3], const double bmax[3], double pos[3] )
{
	if ( !F.ok() )
		return Kept;
	auto colOf = [&]( double x, double y, int & i, int & j ) {
		i = int( std::floor( ( x - F.x0 ) / F.cell ) );
		j = int( std::floor( ( y - F.y0 ) / F.cell ) );
		return i >= 0 && j >= 0 && i < F.nx && j < F.ny;
	};
	int i = 0, j = 0;
	if ( colOf( pos[0], pos[1], i, j ) && F.covered( i, j ) ) {
		const std::size_t c = std::size_t( j ) * F.nx + i;
		if ( F.zmin[c] < pos[2] && pos[2] < F.zmax[c] )
			return Kept;
	}
	int i0 = 0, j0 = 0, i1 = 0, j1 = 0;
	colOf( std::max( bmin[0], F.x0 ), std::max( bmin[1], F.y0 ), i0, j0 );
	colOf( std::min( bmax[0], F.box[2] ), std::min( bmax[1], F.box[3] ), i1, j1 );
	i0 = std::max( i0, 0 ); j0 = std::max( j0, 0 );
	i1 = std::min( i1, F.nx - 1 ); j1 = std::min( j1, F.ny - 1 );
	double best = 1e300, at[3] = { 0.0, 0.0, 0.0 };
	for ( int y = j0; y <= j1; y++ )
		for ( int x = i0; x <= i1; x++ ) {
			const double px = F.cx( x ), py = F.cy( y );
			if ( px < bmin[0] || py < bmin[1] || px > bmax[0] || py > bmax[1] )
				continue;
			const std::size_t c = std::size_t( y ) * F.nx + x;
			if ( !( double( F.zmax[c] ) - double( F.zmin[c] ) >= 2.0 * kAir ) )
				continue;   // nothing, or one surface (open ground, a roof), or a room lower than 2 x kAir
			const double lo = std::max( double( F.zmin[c] ) + kAir, bmin[2] ), hi = std::min( double( F.zmax[c] ) - kAir, bmax[2] );
			if ( lo > hi )
				continue;   // the column's air is out of the box's z
			const double dx = px - pos[0], dy = py - pos[1], d = dx * dx + dy * dy;
			if ( d < best ) {
				best = d;
				at[0] = px;
				at[1] = py;
				at[2] = std::min( std::max( pos[2], lo ), hi );
			}
		}
	if ( best == 1e300 )
		return Dropped;
	pos[0] = at[0];
	pos[1] = at[1];
	pos[2] = at[2];
	return Moved;
}
}   // namespace reflplace
