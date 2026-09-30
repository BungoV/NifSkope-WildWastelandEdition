/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENSE BLOCK *****/

#include "probeplace.h"
#include "probebvh.h"

#include <QElapsedTimer>
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

/* The project builds with -march=haswell, where GCC fuses a * b + c into one
 * FMA. Off in this file: a ray that grazes a triangle edge must land on the same
 * side on every CPU and in the gate's Python re-trace (tests/spells/probe_place.py),
 * which does the same arithmetic in the same order. */
#if defined( __GNUC__ ) && !defined( __clang__ )
#pragma GCC optimize( "fp-contract=off" )
#endif

namespace {

using probebvh::Bvh;

constexpr float CELL = 4096.0f;

// ------------------------------------------------------- triangle vs voxel
bool planeBoxOverlap( const float n[3], const float v[3], const float h[3] )
{
	float vmin[3], vmax[3];
	for ( int q = 0; q < 3; q++ ) {
		if ( n[q] > 0.0f ) {
			vmin[q] = -h[q] - v[q];
			vmax[q] = h[q] - v[q];
		} else {
			vmin[q] = h[q] - v[q];
			vmax[q] = -h[q] - v[q];
		}
	}
	if ( n[0] * vmin[0] + n[1] * vmin[1] + n[2] * vmin[2] > 0.0f )
		return false;
	return n[0] * vmax[0] + n[1] * vmax[1] + n[2] * vmax[2] >= 0.0f;
}

//! Akenine-Moller separating-axis test, box at `c` with half size `h`.
bool triBox( const float c[3], const float h[3], const float * tri )
{
	float v[3][3];
	for ( int i = 0; i < 3; i++ )
		for ( int k = 0; k < 3; k++ )
			v[i][k] = tri[i * 3 + k] - c[k];
	for ( int q = 0; q < 3; q++ ) {
		const float mn = std::min( { v[0][q], v[1][q], v[2][q] } );
		const float mx = std::max( { v[0][q], v[1][q], v[2][q] } );
		if ( mn > h[q] || mx < -h[q] )
			return false;
	}
	float e[3][3];
	for ( int i = 0; i < 3; i++ )
		for ( int k = 0; k < 3; k++ )
			e[i][k] = v[( i + 1 ) % 3][k] - v[i][k];
	for ( int i = 0; i < 3; i++ ) {
		for ( int j = 0; j < 3; j++ ) {
			// axis = unit_j x e_i
			float a[3] = { 0, 0, 0 };
			const int j1 = ( j + 1 ) % 3, j2 = ( j + 2 ) % 3;
			a[j1] = -e[i][j2];
			a[j2] = e[i][j1];
			const float p0 = a[0] * v[0][0] + a[1] * v[0][1] + a[2] * v[0][2];
			const float p1 = a[0] * v[1][0] + a[1] * v[1][1] + a[2] * v[1][2];
			const float p2 = a[0] * v[2][0] + a[1] * v[2][1] + a[2] * v[2][2];
			const float r = h[0] * std::fabs( a[0] ) + h[1] * std::fabs( a[1] ) + h[2] * std::fabs( a[2] );
			if ( std::min( { p0, p1, p2 } ) > r || std::max( { p0, p1, p2 } ) < -r )
				return false;
		}
	}
	const float n[3] = { e[0][1] * e[1][2] - e[0][2] * e[1][1], e[0][2] * e[1][0] - e[0][0] * e[1][2],
		e[0][0] * e[1][1] - e[0][1] * e[1][0] };
	return planeBoxOverlap( n, v[0], h );
}

// ------------------------------------------------------------ the voxel grid
struct Grid
{
	int nx = 0, ny = 0, nz = 0;
	float o[3] = { 0, 0, 0 };   // local coords of voxel (0,0,0)'s low corner
	float v = 35.0f;
	std::vector<quint8> s;      // 1 = solid

	//! 1 solid, 0 air, -1 outside the grid
	int at( int x, int y, int z ) const
	{
		if ( x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz )
			return -1;
		return s[( size_t( z ) * size_t( ny ) + size_t( y ) ) * size_t( nx ) + size_t( x )];
	}
	// P/L/Z addressing: axis P is 0 (x) or 1 (y), L is the other
	int atPL( int P, int p, int l, int z ) const
	{
		return P == 0 ? at( p, l, z ) : at( l, p, z );
	}
	int dim( int axis ) const { return axis == 0 ? nx : axis == 1 ? ny : nz; }
};

//! Run of `state` along `axis` through c, capped at `cap` each way.
//! lo/hi are the last voxels of the run; bounded* says a different in-grid state ends it.
void runAlong( const Grid & g, int P, const int c[3], int axis, int state, int cap,
	int & lo, int & hi, bool & boundedLo, bool & boundedHi )
{
	// c = { p, l, z } in P/L/Z order; axis 0 = P, 1 = L, 2 = Z
	int q[3] = { c[0], c[1], c[2] };
	lo = hi = c[axis];
	boundedLo = boundedHi = false;
	for ( int k = 1; k <= cap + 1; k++ ) {
		q[axis] = c[axis] - k;
		const int s = g.atPL( P, q[0], q[1], q[2] );
		if ( s != state ) {
			boundedLo = ( s >= 0 ) && k <= cap + 1;
			break;
		}
		lo = q[axis];
		if ( k == cap + 1 )
			break;
	}
	for ( int k = 1; k <= cap + 1; k++ ) {
		q[axis] = c[axis] + k;
		const int s = g.atPL( P, q[0], q[1], q[2] );
		if ( s != state ) {
			boundedHi = ( s >= 0 );
			break;
		}
		hi = q[axis];
		if ( k == cap + 1 )
			break;
	}
}

struct ApCandidate
{
	float pos[3];
	float nrm[3];
	float width, height, sill;
	ApertureKind kind;
	bool roomToRoom;
	int voxels;
};

} // namespace

// ================================================================ placement
bool probePlace( const ProbeSoup & soup, const ProbePlaceSpec & spec, ProbePlaceResult * out )
{
	ProbePlaceResult & R = *out;
	R = ProbePlaceResult();
	R.soupTris = soup.triCount();
	if ( !R.soupTris ) {
		R.error = QStringLiteral( "the soup is empty" );
		return false;
	}
	if ( !( spec.maxX >= spec.minX ) || !( spec.maxY >= spec.minY ) || !( spec.spacing > 1.0f ) ) {
		R.error = QStringLiteral( "bad rect or spacing" );
		return false;
	}
	const bool redWall = spec.red == QLatin1String( "wall" );
	const bool redAp = spec.red == QLatin1String( "aperture" );
	const bool redFrames = spec.red == QLatin1String( "frames" );

	// local origin: the rect's center, z 0
	const double O[3] = { 0.5 * ( double( spec.minX ) + spec.maxX ), 0.5 * ( double( spec.minY ) + spec.maxY ), 0.0 };
	QElapsedTimer tm;
	tm.start();
	Bvh bvh;
	bvh.t.resize( soup.tris.size() );
	float zMin = 3.4e38f, zMax = -3.4e38f;
	for ( size_t i = 0; i < soup.tris.size(); i += 3 ) {
		bvh.t[i + 0] = float( double( soup.tris[i + 0] ) - O[0] );
		bvh.t[i + 1] = float( double( soup.tris[i + 1] ) - O[1] );
		bvh.t[i + 2] = soup.tris[i + 2];
		zMin = std::min( zMin, soup.tris[i + 2] );
		zMax = std::max( zMax, soup.tris[i + 2] );
	}
	bvh.build();
	R.msBvh = double( tm.nsecsElapsed() ) / 1e6;

	// local-space ray, from -> to; distance along it on a hit
	auto castRay = [&]( const double f[3], const double to[3], double * dist ) -> bool {
		const double d[3] = { to[0] - f[0], to[1] - f[1], to[2] - f[2] };
		const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		if ( !( len > 1e-9 ) )
			return false;
		const double u[3] = { d[0] / len, d[1] / len, d[2] / len };
		double t = 0;
		if ( !bvh.ray( f, u, len, &t ) )
			return false;
		if ( dist )
			*dist = t;
		return true;
	};

	auto addProbe = [&]( ProbePoint p ) {
		p.pos[0] = float( double( p.pos[0] ) + O[0] );
		p.pos[1] = float( double( p.pos[1] ) + O[1] );
		p.cellX = int( std::floor( p.pos[0] / CELL ) );
		p.cellY = int( std::floor( p.pos[1] / CELL ) );
		R.probes.push_back( p );
	};

	// ---- FO4CS B2f/B2n, number for number
	tm.restart();
	const double top = double( zMax ) + 16.0;
	const double bottom = double( zMin ) - 16.0;
	const size_t maxSurfaces = size_t( std::min( spec.maxLevels * 2 + 2, 64 ) );
	const float kWallDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	float lowestFloor = 3.4e38f;
	std::vector<float> surfaces;
	const qint64 i0 = qint64( std::ceil( spec.minX / spec.spacing ) );
	const qint64 j0 = qint64( std::ceil( spec.minY / spec.spacing ) );
	for ( qint64 j = j0;; ++j ) {
		const float wy = float( j ) * spec.spacing;
		if ( wy > spec.maxY )
			break;
		for ( qint64 i = i0;; ++i ) {
			const float wx = float( i ) * spec.spacing;
			if ( wx > spec.maxX )
				break;
			R.columns++;
			const double x = double( wx ) - O[0], y = double( wy ) - O[1];
			// the column descent
			surfaces.clear();
			double cursor = top;
			while ( surfaces.size() < maxSurfaces && cursor > bottom ) {
				const double f[3] = { x, y, cursor }, to[3] = { x, y, bottom };
				double dist = 0;
				if ( !castRay( f, to, &dist ) )
					break;
				const float hitZ = float( cursor - dist );
				if ( !surfaces.empty() && !( hitZ < surfaces.back() - 1e-3f ) )
					break;
				surfaces.push_back( hitZ );
				cursor = double( hitZ ) - spec.pierceStep;
			}
			if ( surfaces.empty() ) {
				R.columnsEmpty++;
				continue;
			}
			lowestFloor = std::min( lowestFloor, surfaces.back() );
			ProbePoint point;
			point.pos[0] = float( x );
			point.pos[1] = float( y );
			point.pos[2] = surfaces[0] + spec.eye;
			point.cls = ProbeClass::FirstHit;
			std::vector<ProbePoint> origins { point };
			addProbe( point );
			R.firstHit++;
			int level = 0;
			for ( size_t s = 1; s < surfaces.size(); ++s ) {
				const float gap = surfaces[s - 1] - surfaces[s];
				if ( !( gap >= spec.minAirGap ) ) {
					R.gapsRejected++;
					continue;
				}
				if ( level + 1 >= spec.maxLevels ) {
					R.levelsCapped++;
					break;
				}
				++level;
				ProbePoint in = point;
				in.pos[2] = surfaces[s] + std::min( spec.eye, gap * 0.5f );
				in.cls = ProbeClass::Interior;
				in.level = level;
				origins.push_back( in );
				addProbe( in );
				R.interior++;
			}
			// wall stacks off every origin
			bool columnFoundWall = false;
			for ( const ProbePoint & og : origins ) {
				for ( int d = 0; d < 4; d++ ) {
					const double dx = kWallDirs[d][0], dy = kWallDirs[d][1];
					const double eye[3] = { og.pos[0], og.pos[1], og.pos[2] };
					const double end[3] = { eye[0] + dx * spec.wallSearch, eye[1] + dy * spec.wallSearch, eye[2] };
					double wallDist = 0;
					if ( !castRay( eye, end, &wallDist ) )
						continue;
					const double standoff = wallDist - spec.wallStandoff;
					if ( !( standoff > 16.0 ) )
						continue;
					columnFoundWall = true;
					double last[3] = { eye[0] + dx * standoff, eye[1] + dy * standoff, eye[2] };
					int stack = 0;
					for ( const float hgt : spec.wallHeights ) {
						const double cand[3] = { last[0], last[1], double( og.pos[2] ) + hgt };
						if ( !( cand[2] - last[2] > 0.0 ) )
							break;
						if ( castRay( last, cand, nullptr ) ) {
							R.wallRefused++;
							break;
						}
						const double probe[3] = { cand[0] + dx * spec.wallStandoff * 2.0,
							cand[1] + dy * spec.wallStandoff * 2.0, cand[2] };
						if ( !redWall && !castRay( cand, probe, nullptr ) ) {
							R.wallRefused++;
							break;
						}
						ProbePoint w;
						w.pos[0] = float( cand[0] );
						w.pos[1] = float( cand[1] );
						w.pos[2] = float( cand[2] );
						w.cls = ProbeClass::Wall;
						w.nrm[0] = float( dx );   // toward the wall it hangs off
						w.nrm[1] = float( dy );
						w.level = ++stack;
						addProbe( w );
						R.wall++;
						std::memcpy( last, cand, sizeof last );
					}
				}
			}
			if ( columnFoundWall )
				R.wallColumns++;
		}
	}
	R.msColumns = double( tm.nsecsElapsed() ) / 1e6;
	if ( !spec.apertures )
		return true;

	/* ---- the openings. A wall is only thin in voxels when it runs along a
	 * grid axis, and whole towns stand at 45 degrees (Concord). So the soup is
	 * voxelized again in rotated frames (spec.apertureAngles: 0, 22.5, 45,
	 * 67.5 -- every wall within 11.25 degrees of one of them); each frame finds
	 * its openings, and the dedupe below keeps one probe per opening. */
	float gz0 = ( lowestFloor < 3.0e38f ? lowestFloor : zMin ) - 2.0f * spec.voxel;
	float gz1 = -3.4e38f;
	const float margin = spec.spacing;
	const float rx0 = float( spec.minX - O[0] ) - margin, rx1 = float( spec.maxX - O[0] ) + margin;
	const float ry0 = float( spec.minY - O[1] ) - margin, ry1 = float( spec.maxY - O[1] ) + margin;
	for ( size_t i = 0; i < bvh.t.size(); i += 9 ) {
		const float * p = &bvh.t[i];
		const float mnx = std::min( { p[0], p[3], p[6] } ), mxx = std::max( { p[0], p[3], p[6] } );
		const float mny = std::min( { p[1], p[4], p[7] } ), mxy = std::max( { p[1], p[4], p[7] } );
		if ( mxx < rx0 - margin || mnx > rx1 + margin || mxy < ry0 - margin || mny > ry1 + margin )
			continue;
		gz1 = std::max( gz1, std::max( { p[2], p[5], p[8] } ) );
	}
	gz1 += spec.voxel;
	std::vector<ApCandidate> cands;
	std::vector<quint8> mark;
	Grid g, g0;   // g0: the 0-degree frame, kept for the interior rule
	for ( const float angDeg : spec.apertureAngles ) {
		if ( redFrames && R.apFrames > 0 )
			break;
		const double th = double( angDeg ) * 3.14159265358979323846 / 180.0;
		const float ca = float( std::cos( th ) ), sa = float( std::sin( th ) );
		// frame coords (grid axes) from local, and back
		auto toG = [&]( float x, float y, float & gx, float & gy ) {
			gx = ca * x + sa * y;
			gy = -sa * x + ca * y;
		};
		auto fromG = [&]( double gx, double gy, double & x, double & y ) {
			x = double( ca ) * gx - double( sa ) * gy;
			y = double( sa ) * gx + double( ca ) * gy;
		};
		tm.restart();
		g = Grid();
		g.v = spec.voxel;
		float lx0 = 3.4e38f, lx1 = -3.4e38f, ly0 = 3.4e38f, ly1 = -3.4e38f;
		for ( int k = 0; k < 4; k++ ) {
			float gx, gy;
			toG( ( k & 1 ) ? rx1 : rx0, ( k & 2 ) ? ry1 : ry0, gx, gy );
			lx0 = std::min( lx0, gx );
			lx1 = std::max( lx1, gx );
			ly0 = std::min( ly0, gy );
			ly1 = std::max( ly1, gy );
		}
		g.nx = int( std::ceil( ( lx1 - lx0 ) / g.v ) );
		g.ny = int( std::ceil( ( ly1 - ly0 ) / g.v ) );
		g.nz = int( std::ceil( ( gz1 - gz0 ) / g.v ) );
		if ( g.nz > spec.maxVoxelLayers ) {
			g.nz = spec.maxVoxelLayers;
			R.gridClamped = true;
		}
		if ( R.apFrames == 0 ) {
			R.gridX = g.nx;
			R.gridY = g.ny;
			R.gridZ = g.nz;
		}
		if ( g.nx < 3 || g.ny < 3 || g.nz < 3 || double( g.nx ) * g.ny * g.nz > 6.0e8 ) {
			R.error = QStringLiteral( "aperture grid unusable (%1 x %2 x %3)" ).arg( g.nx ).arg( g.ny ).arg( g.nz );
			return true;   // the lattice probes stand; the census says why no openings
		}
		R.apFrames++;
		g.o[0] = lx0;
		g.o[1] = ly0;
		g.o[2] = gz0;
		g.s.assign( size_t( g.nx ) * size_t( g.ny ) * size_t( g.nz ), 0 );
		const float hv[3] = { g.v * 0.5f, g.v * 0.5f, g.v * 0.5f };
		for ( size_t i = 0; i < bvh.t.size(); i += 9 ) {
			float p[9];
			for ( int k = 0; k < 3; k++ ) {
				toG( bvh.t[i + size_t( k ) * 3 + 0], bvh.t[i + size_t( k ) * 3 + 1], p[k * 3 + 0], p[k * 3 + 1] );
				p[k * 3 + 2] = bvh.t[i + size_t( k ) * 3 + 2];
			}
			int lo[3], hi[3];
			bool skip = false;
			for ( int k = 0; k < 3; k++ ) {
				const float mn = std::min( { p[k], p[3 + k], p[6 + k] } );
				const float mx = std::max( { p[k], p[3 + k], p[6 + k] } );
				lo[k] = std::max( 0, int( std::floor( ( mn - g.o[k] ) / g.v ) ) );
				hi[k] = std::min( g.dim( k ) - 1, int( std::floor( ( mx - g.o[k] ) / g.v ) ) );
				if ( lo[k] > hi[k] )
					skip = true;
			}
			if ( skip )
				continue;
			for ( int z = lo[2]; z <= hi[2]; z++ )
				for ( int y = lo[1]; y <= hi[1]; y++ )
					for ( int x = lo[0]; x <= hi[0]; x++ ) {
						quint8 & cell = g.s[( size_t( z ) * size_t( g.ny ) + size_t( y ) ) * size_t( g.nx ) + size_t( x )];
						if ( cell )
							continue;
						const float c[3] = { g.o[0] + ( x + 0.5f ) * g.v, g.o[1] + ( y + 0.5f ) * g.v,
							g.o[2] + ( z + 0.5f ) * g.v };
						if ( triBox( c, hv, p ) )
							cell = 1;
					}
		}
		R.msVoxel += double( tm.nsecsElapsed() ) / 1e6;
		if ( angDeg == 0.0f && spec.coverage && g0.s.empty() )
			g0 = g;

		tm.restart();
		const int MAXW = 17;   // widest opening, voxels (595 u)
		const int MAXH = 20;   // tallest (700 u)
		const int THIN = 6;    // thickest wall (210 u)
		const int D = 3;       // air required beyond the wall, each side
		for ( int P = 0; P < 2; P++ ) {
			const int np = g.dim( P ), nl = g.dim( 1 - P );
			mark.assign( g.s.size(), 0 );
			auto mi = [&]( int p, int l, int z ) -> size_t {
				const int x = P == 0 ? p : l, y = P == 0 ? l : p;
				return ( size_t( z ) * size_t( g.ny ) + size_t( y ) ) * size_t( g.nx ) + size_t( x );
			};
			for ( int z = 0; z < g.nz; z++ ) {
				for ( int p = 0; p < np; p++ ) {
					int l = 0;
					while ( l < nl ) {
						if ( g.atPL( P, p, l, z ) != 0 ) {
							l++;
							continue;
						}
						int l1 = l;
						while ( l1 + 1 < nl && g.atPL( P, p, l1 + 1, z ) == 0 )
							l1++;
						const int l0 = l;
						l = l1 + 1;
						const int w = l1 - l0 + 1;
						if ( w < 2 || w > MAXW || l0 == 0 || l1 == nl - 1 )
							continue;   // not bounded by solid on both sides, or out of range
						// the jambs, thin along P
						int paL, pbL, paR, pbR;
						bool b1, b2;
						{
							const int cL[3] = { p, l0 - 1, z };
							runAlong( g, P, cL, 0, 1, THIN + 1, paL, pbL, b1, b2 );
							const int cR[3] = { p, l1 + 1, z };
							runAlong( g, P, cR, 0, 1, THIN + 1, paR, pbR, b1, b2 );
						}
						if ( !redAp && ( pbL - paL + 1 > THIN || pbR - paR + 1 > THIN ) )
							continue;
						const int WA = std::min( paL, paR ), WB = std::max( pbL, pbR );
						for ( int ll = l0; ll <= l1; ll++ ) {
							const int c[3] = { p, ll, z };
							int za, zb;
							bool bza, bzb;
							runAlong( g, P, c, 2, 0, MAXH + 1, za, zb, bza, bzb );
							const int h = zb - za + 1;
							if ( !bza || !bzb || h < 2 || h > MAXH )
								continue;
							if ( !redAp ) {
								// the lintel is part of a thin wall too
								int la, lb;
								bool x1, x2;
								const int cl[3] = { p, ll, zb + 1 };
								runAlong( g, P, cl, 0, 1, THIN + 1, la, lb, x1, x2 );
								if ( lb - la + 1 > THIN )
									continue;
								// straight through the wall, with air beyond on both sides
								bool through = true;
								for ( int q = WA - D; q <= WB + D && through; q++ )
									if ( g.atPL( P, q, ll, z ) != 0 )
										through = false;
								if ( !through )
									continue;
								// a neck: the air beyond is wider or taller on BOTH sides
								bool neck = true;
								for ( int side = 0; side < 2 && neck; side++ ) {
									const int q = side ? WB + D : WA - D;
									const int cb[3] = { q, ll, z };
									int a0, a1;
									bool ba0, ba1;
									runAlong( g, P, cb, 1, 0, MAXW + 1, a0, a1, ba0, ba1 );
									const bool wider = !ba0 || !ba1 || ( a1 - a0 + 1 ) > w + 1;
									runAlong( g, P, cb, 2, 0, MAXH + 1, a0, a1, ba0, ba1 );
									const bool taller = !ba0 || !ba1 || ( a1 - a0 + 1 ) > h + 1;
									neck = wider || taller;
								}
								if ( !neck )
									continue;
							}
							mark[mi( p, ll, z )] = 1;
						}
					}
				}
			}
			// components, 6-connected
			std::vector<int> st;
			for ( int z = 0; z < g.nz; z++ )
				for ( int l = 0; l < nl; l++ )
					for ( int p = 0; p < np; p++ ) {
						if ( mark[mi( p, l, z )] != 1 )
							continue;
						int lo[3] = { p, l, z }, hi[3] = { p, l, z };
						int count = 0;
						st.clear();
						st.push_back( p );
						st.push_back( l );
						st.push_back( z );
						mark[mi( p, l, z )] = 2;
						while ( !st.empty() ) {
							const int cz = st.back(); st.pop_back();
							const int cl = st.back(); st.pop_back();
							const int cp = st.back(); st.pop_back();
							count++;
							const int c[3] = { cp, cl, cz };
							for ( int k = 0; k < 3; k++ ) {
								lo[k] = std::min( lo[k], c[k] );
								hi[k] = std::max( hi[k], c[k] );
							}
							static const int nb[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 },
								{ 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
							for ( const auto & n : nb ) {
								const int q[3] = { cp + n[0], cl + n[1], cz + n[2] };
								if ( q[0] < 0 || q[1] < 0 || q[2] < 0 || q[0] >= np || q[1] >= nl || q[2] >= g.nz )
									continue;
								quint8 & m = mark[mi( q[0], q[1], q[2] )];
								if ( m != 1 )
									continue;
								m = 2;
								st.push_back( q[0] );
								st.push_back( q[1] );
								st.push_back( q[2] );
							}
						}
						R.apComponents++;
						const int spanP = hi[0] - lo[0] + 1, spanL = hi[1] - lo[1] + 1, spanZ = hi[2] - lo[2] + 1;
						if ( spanL < 2 || spanZ < 2 || spanP > THIN ) {
							R.apRejectedShape++;
							continue;
						}
						// local geometry
						const float cP = g.o[P] + ( lo[0] + hi[0] + 1 ) * 0.5f * g.v;
						const float cL = g.o[1 - P] + ( lo[1] + hi[1] + 1 ) * 0.5f * g.v;
						const float bot = g.o[2] + lo[2] * g.v;
						const float width = spanL * g.v, height = spanZ * g.v, thick = spanP * g.v;
						const float pz = bot + std::min( spec.eye, height * 0.5f );
						/* EACH SIDE IS OPEN SKY, A ROOM, OR A POCKET. A room is a space
						 * a probe could stand in: floor to ceiling at least the column
						 * descent's own air gap (140). A pocket -- the inside of a car
						 * wreck, the hollow of a rubble pile -- is not a volume the
						 * light graph connects, so an opening into one is not a neck. */
						float sill = 3.4e38f;
						bool roofed[2] = { false, false }, pocket = false;
						for ( int side = 0; side < 2; side++ ) {
							const float sp = cP + ( side ? 1.0f : -1.0f ) * ( thick * 0.5f + 2.0f * g.v );
							double s0[3], sg[2];
							sg[P] = sp;
							sg[1 - P] = cL;
							fromG( sg[0], sg[1], s0[0], s0[1] );
							s0[2] = bot + 10.0;
							double s1[3] = { s0[0], s0[1], s0[2] - 2000.0 };
							double dist = 0;
							double floorZ = -1e30;
							if ( castRay( s0, s1, &dist ) ) {
								floorZ = s0[2] - dist;
								sill = std::min( sill, float( double( bot ) - floorZ ) );
							}
							s0[2] = pz;
							s1[0] = s0[0];
							s1[1] = s0[1];
							s1[2] = pz + 8192.0;
							if ( castRay( s0, s1, &dist ) ) {
								roofed[side] = true;
								if ( pz + dist - floorZ < double( spec.minAirGap ) )
									pocket = true;
							}
						}
						if ( !roofed[0] && !roofed[1] ) {
							R.apRejectedUnroofed++;
							continue;
						}
						if ( pocket ) {
							R.apRejectedPocket++;
							continue;
						}
						double pg[2], ng[2] = { 0, 0 }, lp[2], ln[2];
						pg[P] = cP;
						pg[1 - P] = cL;
						ng[P] = ( roofed[1] && !roofed[0] ) ? -1.0 : 1.0;
						fromG( pg[0], pg[1], lp[0], lp[1] );
						fromG( ng[0], ng[1], ln[0], ln[1] );
						if ( lp[0] < rx0 || lp[0] > rx1 || lp[1] < ry0 || lp[1] > ry1 )
							continue;   // a rotated frame's corner, outside the rect and its margin
						ApCandidate a;
						a.pos[0] = float( lp[0] );
						a.pos[1] = float( lp[1] );
						a.pos[2] = pz;
						a.nrm[0] = float( ln[0] );
						a.nrm[1] = float( ln[1] );
						a.nrm[2] = 0.0f;
						a.roomToRoom = roofed[0] && roofed[1];
						a.width = width;
						a.height = height;
						a.sill = sill < 3.0e38f ? sill : -1.0f;
						if ( width > 400.0f || height > 400.0f )
							a.kind = ApertureKind::Breach;
						else if ( sill < 3.0e38f && sill <= 60.0f && height >= 140.0f )
							a.kind = ApertureKind::Doorway;
						else
							a.kind = ApertureKind::Window;
						a.voxels = count;
						cands.push_back( a );
					}
		}
		R.msApertures += double( tm.nsecsElapsed() ) / 1e6;
	}
	tm.restart();
	// one probe per opening: two pass axes and neighboring frames can all see one
	std::sort( cands.begin(), cands.end(), []( const ApCandidate & a, const ApCandidate & b ) {
		return a.voxels > b.voxels;
	} );
	std::vector<const ApCandidate *> kept;
	for ( const ApCandidate & a : cands ) {
		bool dup = false;
		for ( const ApCandidate * k : kept ) {
			const float dx = a.pos[0] - k->pos[0], dy = a.pos[1] - k->pos[1], dz = a.pos[2] - k->pos[2];
			if ( dx * dx + dy * dy + dz * dz < 105.0f * 105.0f ) {
				dup = true;
				break;
			}
		}
		if ( dup ) {
			R.apMerged++;
			continue;
		}
		kept.push_back( &a );
		ProbePoint pp;
		std::memcpy( pp.pos, a.pos, sizeof pp.pos );
		std::memcpy( pp.nrm, a.nrm, sizeof pp.nrm );
		pp.cls = ProbeClass::Aperture;
		pp.kind = a.kind;
		pp.width = a.width;
		pp.height = a.height;
		pp.sill = a.sill;
		pp.roomToRoom = a.roomToRoom;
		const float wx = float( double( a.pos[0] ) + O[0] ), wy = float( double( a.pos[1] ) + O[1] );
		for ( const ProbeSoup::Door & dr : soup.doors ) {
			if ( wx >= dr.lo[0] - 35.0f && wx <= dr.hi[0] + 35.0f && wy >= dr.lo[1] - 35.0f
				&& wy <= dr.hi[1] + 35.0f && a.pos[2] >= dr.lo[2] - 35.0f && a.pos[2] <= dr.hi[2] + 35.0f ) {
				pp.doorRef = dr.ref;
				break;
			}
		}
		addProbe( pp );
		switch ( a.kind ) {
		case ApertureKind::Doorway: R.doorway++; break;
		case ApertureKind::Window: R.window++; break;
		default: R.breach++; break;
		}
		if ( pp.doorRef )
			R.doored++;
		if ( pp.roomToRoom )
			R.roomToRoom++;
	}
	R.msApertures += double( tm.nsecsElapsed() ) / 1e6;

	/* ---- THE INTERIOR RULE (bungo 2026-09-30: "some rooms are tight and
	 * separated, we probably need more probes inside of buildings that cover all
	 * the hallways, rooms, corners"). On the 0-degree voxel grid a walkable cell
	 * is air on solid with minAirGap of air above it; covered when a roof closes
	 * that air. Rooms are the covered walkable cells joined across a step of one
	 * voxel, cut at every opening found above. A room whose edge is mostly open
	 * ground (a porch, the shade of an overpass) is outdoors: the lattice has it.
	 * Each enclosed room gets a probe at its cell farthest from the walls; then,
	 * widest cells first, every walkable cell must SEE a probe (line of sight)
	 * within coverRadius -- hallRadius in a hallway -- or it gets one. */
	if ( !spec.coverage || g0.s.empty() )
		return true;
	tm.restart();
	const bool redCover = spec.red == QLatin1String( "coverage" );
	const Grid & G = g0;
	const double v = G.v;
	const int headVox = std::max( 1, int( std::ceil( spec.minAirGap / G.v - 1e-4f ) ) );
	struct WCell { int x, y, z, head; bool covered; int comp; };
	std::vector<WCell> W;
	std::vector<int> colStart( size_t( G.nx ) * size_t( G.ny ) + 1, 0 );
	for ( int y = 0; y < G.ny; y++ )
		for ( int x = 0; x < G.nx; x++ ) {
			colStart[size_t( y ) * size_t( G.nx ) + size_t( x )] = int( W.size() );
			for ( int z = 1; z < G.nz; z++ ) {
				if ( G.at( x, y, z ) != 0 || G.at( x, y, z - 1 ) != 1 )
					continue;
				int h = 0;
				while ( z + h < G.nz && G.at( x, y, z + h ) == 0 )
					h++;
				if ( h >= headVox )
					W.push_back( { x, y, z, h, z + h < G.nz, -1 } );
			}
		}
	colStart.back() = int( W.size() );
	R.walkCells = int( W.size() );
	// the walkable cell in column (x, y) nearest z, at most dz away; -1 = none
	auto cellAt = [&]( int x, int y, int z, int dz ) -> int {
		if ( x < 0 || y < 0 || x >= G.nx || y >= G.ny )
			return -1;
		const size_t c = size_t( y ) * size_t( G.nx ) + size_t( x );
		int best = -1;
		for ( int k = colStart[c]; k < colStart[c + 1]; k++ ) {
			const int d = std::abs( W[size_t( k )].z - z );
			if ( d <= dz && ( best < 0 || d < std::abs( W[size_t( best )].z - z ) ) )
				best = k;
		}
		return best;
	};
	// the cut: every walkable cell inside an opening's slab belongs to no room; a cell under
	// a window's slab is lit (its room has a way in, so it is no sealed hollow)
	std::vector<quint8> lit( W.size(), 0 );
	for ( const ApCandidate * a : kept ) {
		const double n[2] = { a->nrm[0], a->nrm[1] }, t[2] = { -double( a->nrm[1] ), a->nrm[0] };
		const double bot = double( a->pos[2] ) - std::min( double( spec.eye ), double( a->height ) * 0.5 );
		const double reach = double( a->width ) * 0.5 + v;
		const int x0 = int( std::floor( ( a->pos[0] - reach - G.o[0] ) / v ) ), x1 = int( std::ceil( ( a->pos[0] + reach - G.o[0] ) / v ) );
		const int y0 = int( std::floor( ( a->pos[1] - reach - G.o[1] ) / v ) ), y1 = int( std::ceil( ( a->pos[1] + reach - G.o[1] ) / v ) );
		for ( int y = std::max( 0, y0 ); y <= std::min( G.ny - 1, y1 ); y++ )
			for ( int x = std::max( 0, x0 ); x <= std::min( G.nx - 1, x1 ); x++ ) {
				const double cx = G.o[0] + ( x + 0.5 ) * v - a->pos[0], cy = G.o[1] + ( y + 0.5 ) * v - a->pos[1];
				if ( std::fabs( cx * n[0] + cy * n[1] ) > 1.5 * v || std::fabs( cx * t[0] + cy * t[1] ) > reach )
					continue;
				const size_t c = size_t( y ) * size_t( G.nx ) + size_t( x );
				for ( int k = colStart[c]; k < colStart[c + 1]; k++ ) {
					const double fz = G.o[2] + W[size_t( k )].z * v;
					if ( fz >= bot - 2.0 * v && fz <= bot + a->height && W[size_t( k )].comp != -2 ) {
						W[size_t( k )].comp = -2;
						R.cutCells++;
					} else if ( fz < bot - 2.0 * v && fz + W[size_t( k )].head * v > bot ) {
						lit[size_t( k )] = 1;
					}
				}
			}
	}
	// rooms: covered cells, 4-connected in xy across a one-voxel step
	struct Room { std::vector<int> cells; int open = 0, edges = 0, drop = 0, door = 0; };
	std::vector<Room> rooms;
	static const int n4[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	std::vector<int> st;
	for ( size_t i0 = 0; i0 < W.size(); i0++ ) {
		if ( !W[i0].covered || W[i0].comp != -1 )
			continue;
		const int id = int( rooms.size() );
		rooms.emplace_back();
		W[i0].comp = id;
		st.assign( 1, int( i0 ) );
		while ( !st.empty() ) {
			const int i = st.back();
			st.pop_back();
			rooms.back().cells.push_back( i );
			for ( const auto & d : n4 ) {
				const int k = cellAt( W[size_t( i )].x + d[0], W[size_t( i )].y + d[1], W[size_t( i )].z, 1 );
				if ( k < 0 || !W[size_t( k )].covered || W[size_t( k )].comp != -1 )
					continue;
				W[size_t( k )].comp = id;
				st.push_back( k );
			}
		}
	}
	const double qx0 = double( spec.minX ) - O[0], qx1 = double( spec.maxX ) - O[0];
	const double qy0 = double( spec.minY ) - O[1], qy1 = double( spec.maxY ) - O[1];
	auto inRect = [&]( const WCell & c ) {
		const double x = G.o[0] + ( c.x + 0.5 ) * v, y = G.o[1] + ( c.y + 0.5 ) * v;
		return x >= qx0 && x <= qx1 && y >= qy0 && y <= qy1;
	};
	// the cell's sample point (voxel floor + eye), and where a probe placed there stands (the real floor)
	auto sampleOf = [&]( const WCell & c, double p[3] ) {
		p[0] = G.o[0] + ( c.x + 0.5 ) * v;
		p[1] = G.o[1] + ( c.y + 0.5 ) * v;
		p[2] = G.o[2] + c.z * v + std::min( double( spec.eye ), c.head * v * 0.5 );
	};
	auto standAt = [&]( const WCell & c, double p[3] ) {
		sampleOf( c, p );
		const double fz = G.o[2] + c.z * v;
		const double f0[3] = { p[0], p[1], fz + 0.5 * v }, f1[3] = { p[0], p[1], fz - 2.0 * v };
		double dist = 0;
		if ( !castRay( f0, f1, &dist ) )
			return;
		const double floorZ = f0[2] - dist;
		const double c0[3] = { p[0], p[1], floorZ + 1.0 }, c1[3] = { p[0], p[1], floorZ + 1.0 + ( c.head + 2 ) * v };
		double ceilZ = c1[2];
		if ( castRay( c0, c1, &dist ) )
			ceilZ = c0[2] + dist;
		p[2] = floorZ + std::min( double( spec.eye ), ( ceilZ - floorZ ) * 0.5 );
	};
	// every probe so far, bucketed for the line-of-sight test
	struct P3 { double p[3]; };
	std::vector<P3> PP;
	std::unordered_map<qint64, std::vector<int>> bucket;
	const double BK = 256.0;
	auto bkey = []( int bx, int by ) { return ( qint64( bx ) << 32 ) | qint64( quint32( by ) ); };
	auto indexProbe = [&]( const double p[3] ) {
		PP.push_back( { { p[0], p[1], p[2] } } );
		bucket[bkey( int( std::floor( p[0] / BK ) ), int( std::floor( p[1] / BK ) ) )].push_back( int( PP.size() ) - 1 );
	};
	for ( const ProbePoint & pp : R.probes ) {
		const double p[3] = { double( pp.pos[0] ) - O[0], double( pp.pos[1] ) - O[1], double( pp.pos[2] ) };
		indexProbe( p );
	}
	// a probe within rad of s; with los, one that also sees s
	auto seen = [&]( const double s[3], double rad, bool los ) -> bool {
		const int bx0 = int( std::floor( ( s[0] - rad ) / BK ) ), bx1 = int( std::floor( ( s[0] + rad ) / BK ) );
		const int by0 = int( std::floor( ( s[1] - rad ) / BK ) ), by1 = int( std::floor( ( s[1] + rad ) / BK ) );
		for ( int by = by0; by <= by1; by++ )
			for ( int bx = bx0; bx <= bx1; bx++ ) {
				const auto it = bucket.find( bkey( bx, by ) );
				if ( it == bucket.end() )
					continue;
				for ( const int k : it->second ) {
					const double * p = PP[size_t( k )].p;
					const double dx = s[0] - p[0], dy = s[1] - p[1], dz = s[2] - p[2];
					const double d2 = dx * dx + dy * dy + dz * dz;
					if ( d2 > rad * rad )
						continue;
					const double len = std::sqrt( d2 );
					double dist = 0;
					if ( !los || len < 1e-6 || !castRay( p, s, &dist ) || dist >= len - 1.0 )
						return true;
				}
			}
		return false;
	};
	auto place = [&]( const WCell & c, ProbeClass cls, int roomId ) {
		double p[3];
		standAt( c, p );
		indexProbe( p );
		ProbePoint pp;
		pp.pos[0] = float( p[0] );
		pp.pos[1] = float( p[1] );
		pp.pos[2] = float( p[2] );
		pp.cls = cls;
		pp.level = roomId;
		addProbe( pp );
	};
	/* each room: enclosed or open; then, per cell, its clearance measured at EYE HEIGHT by 8
	 * horizontal rays (the nearest wall), and whether it is a hallway (the narrowest
	 * wall-to-wall span through it at most twice hallWidth). Measured at eye height,
	 * not on the floor: tables, beds and display bases are not walls. */
	std::vector<int> roomNo( rooms.size(), -1 );
	std::vector<quint8> enclosed( rooms.size(), 0 ), hallOf( W.size(), 0 );
	std::vector<float> clr( W.size(), 0.0f );
	const double kCap = 600.0;
	static const double d8[8][2] = { { 1, 0 }, { 0.70710678, 0.70710678 }, { 0, 1 }, { -0.70710678, 0.70710678 },
		{ -1, 0 }, { -0.70710678, -0.70710678 }, { 0, -1 }, { 0.70710678, -0.70710678 } };
	for ( size_t id = 0; id < rooms.size(); id++ ) {
		Room & rm = rooms[id];
		for ( const int i : rm.cells ) {
			rm.door += lit[size_t( i )];
			for ( const auto & d : n4 ) {
				const int k = cellAt( W[size_t( i )].x + d[0], W[size_t( i )].y + d[1], W[size_t( i )].z, 1 );
				if ( k >= 0 && W[size_t( k )].comp == int( id ) )
					continue;
				rm.edges++;
				if ( k >= 0 ) {
					if ( W[size_t( k )].comp == -2 )
						rm.door++;   // an opening's cut: a wall with a way through
					else if ( !W[size_t( k )].covered )
						rm.open++;   // uncovered ground: outdoors
					continue;
				}
				// no floor there at all: the edge of a table top, a ledge, a foundation. A wall,
				// a railing or furniture standing on the floor is solid at our level instead.
				const WCell & c = W[size_t( i )];
				if ( G.at( c.x + d[0], c.y + d[1], c.z ) == 0 && G.at( c.x + d[0], c.y + d[1], c.z - 1 ) == 0 )
					rm.drop++;
			}
		}
		if ( int( rm.cells.size() ) < spec.roomMinCells ) {
			R.roomsTiny++;
			continue;
		}
		if ( !rm.open && !rm.drop && !rm.door ) {
			R.roomsSealed++;   // no way in: the hollow of a foundation, a crawlspace, a closed shell
			continue;
		}
		if ( rm.drop * 2 >= rm.edges ) {
			R.roomsLedge++;
			continue;
		}
		if ( double( rm.open + rm.drop ) > double( spec.roomOpenMax ) * rm.edges ) {
			R.roomsOpen++;
			continue;
		}
		enclosed[id] = 1;
		int best = -1;
		for ( const int i : rm.cells ) {
			double s[3];
			sampleOf( W[size_t( i )], s );
			double dd[8];
			for ( int k = 0; k < 8; k++ ) {
				const double to[3] = { s[0] + d8[k][0] * kCap, s[1] + d8[k][1] * kCap, s[2] };
				double dist = kCap;
				if ( !castRay( s, to, &dist ) )
					dist = kCap;
				dd[k] = dist;
			}
			double mn = kCap, span = 2.0 * kCap;
			for ( int k = 0; k < 8; k++ )
				mn = std::min( mn, dd[k] );
			for ( int k = 0; k < 4; k++ )
				span = std::min( span, dd[k] + dd[k + 4] );
			clr[size_t( i )] = float( mn );
			hallOf[size_t( i )] = span <= 2.0 * double( spec.hallWidth ) ? 1 : 0;
			if ( best < 0 || clr[size_t( i )] > clr[size_t( best )] )
				best = i;   // cells are in discovery order: the first widest wins
		}
		if ( !inRect( W[size_t( best )] ) )
			continue;       // the block that holds its middle places it
		roomNo[id] = R.rooms++;
		double s[3];
		standAt( W[size_t( best )], s );
		if ( seen( s, 70.0, false ) ) {
			R.roomNear++;
			continue;
		}
		place( W[size_t( best )], ProbeClass::Room, roomNo[id] );
		R.room++;
	}
	// the blind spots, widest cells first
	std::vector<int> order;
	for ( size_t id = 0; id < rooms.size(); id++ )
		if ( enclosed[id] )
			for ( const int i : rooms[id].cells )
				if ( inRect( W[size_t( i )] ) )
					order.push_back( i );
	std::stable_sort( order.begin(), order.end(), [&]( int a, int b ) {
		return clr[size_t( a )] > clr[size_t( b )];
	} );
	R.coverCells = int( order.size() );
	for ( const int i : order ) {
		const WCell & c = W[size_t( i )];
		const bool hall = hallOf[size_t( i )] != 0;
		R.hallCells += hall ? 1 : 0;
		double s[3];
		sampleOf( c, s );
		if ( seen( s, hall ? spec.hallRadius : spec.coverRadius, true ) )
			continue;
		if ( redCover ) {
			R.blindLeft++;
			continue;
		}
		place( c, ProbeClass::Cover, roomNo[size_t( c.comp )] );
		R.cover++;
	}
	R.msCoverage = double( tm.nsecsElapsed() ) / 1e6;
	return true;
}

// ===================================================================== I/O
static const char * className( ProbeClass c )
{
	switch ( c ) {
	case ProbeClass::FirstHit: return "first-hit";
	case ProbeClass::Interior: return "interior";
	case ProbeClass::Wall: return "wall";
	case ProbeClass::Room: return "room";
	case ProbeClass::Cover: return "cover";
	default: return "aperture";
	}
}

static const char * kindName( ApertureKind k )
{
	switch ( k ) {
	case ApertureKind::Doorway: return "doorway";
	case ApertureKind::Window: return "window";
	case ApertureKind::Breach: return "breach";
	default: return "-";
	}
}

QString probeCensusText( const ProbePlaceResult & r )
{
	QString s;
	QTextStream t( &s );
	t << "probes: " << r.probes.size() << " = first-hit " << r.firstHit << " + interior " << r.interior
	  << " + wall " << r.wall << " + aperture " << ( r.doorway + r.window + r.breach ) << " + room " << r.room
	  << " + cover " << r.cover << "\n";
	t << "probe columns: " << r.columns << " (" << r.columnsEmpty << " hit nothing), gaps under 140 "
	  << r.gapsRejected << ", level cap " << r.levelsCapped << ", wall levels refused " << r.wallRefused
	  << ", columns with a wall " << r.wallColumns << "\n";
	t << "openings: doorway " << r.doorway << ", window " << r.window << ", breach " << r.breach
	  << " (door standing in it " << r.doored << ", room to room " << r.roomToRoom << "); pieces found "
	  << r.apComponents << ", wrong shape " << r.apRejectedShape << ", no roof either side "
	  << r.apRejectedUnroofed << ", into a pocket " << r.apRejectedPocket << ", same opening seen twice " << r.apMerged << "\n";
	t << "rooms: " << r.rooms << " enclosed (" << r.room << " probes, " << r.roomNear << " already had one at the middle), "
	  << r.roomsOpen << " open to the ground or a drop, " << r.roomsLedge << " mostly a drop (furniture tops, ledges), " << r.roomsSealed << " sealed (hollows), " << r.roomsTiny << " too small; walkable cells " << r.walkCells
	  << ", in enclosed rooms " << r.coverCells << " (hallway " << r.hallCells << "), cut at openings " << r.cutCells
	  << "; blind cells left " << r.blindLeft << "\n";
	t << "probe soup: " << r.soupTris << " triangles; voxel grid " << r.gridX << " x " << r.gridY << " x "
	  << r.gridZ << ( r.gridClamped ? " (height capped)" : "" ) << ", searched at " << r.apFrames
	  << " wall angles\n";
	t << "probe time ms: bvh " << qRound( r.msBvh ) << ", columns " << qRound( r.msColumns ) << ", voxels "
	  << qRound( r.msVoxel ) << ", openings " << qRound( r.msApertures ) << ", rooms " << qRound( r.msCoverage ) << "\n";
	if ( !r.error.isEmpty() )
		t << "probe note: " << r.error << "\n";
	t.flush();
	return s;
}

bool probeWriteTsv( const QString & path, const ProbePlaceSpec & spec, const ProbePlaceResult & r, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) ) {
		if ( error )
			*error = f.errorString();
		return false;
	}
	QTextStream t( &f );
	// the rect at full float precision: the re-trace rebuilds the placer's center from it
	t << qSetRealNumberPrecision( 9 ) << "# probeplace v1 rect " << spec.minX << " " << spec.minY << " " << spec.maxX << " " << spec.maxY
	  << " spacing " << spec.spacing << " eye " << spec.eye << " voxel " << spec.voxel
	  << ( spec.red.isEmpty() ? QString() : QStringLiteral( " RED " ) + spec.red ) << "\n";
	for ( const QString & line : probeCensusText( r ).split( '\n', Qt::SkipEmptyParts ) )
		t << "# " << line << "\n";
	t << "id\tclass\tlevel\tx\ty\tz\tcellX\tcellY\tkind\tnx\tny\tnz\twidth\theight\tsill\tdoorRef\troomToRoom\n";
	int id = 0;
	for ( const ProbePoint & p : r.probes ) {
		t << id++ << '\t' << className( p.cls ) << '\t' << p.level << '\t'
		  << QString::number( p.pos[0], 'f', 2 ) << '\t' << QString::number( p.pos[1], 'f', 2 ) << '\t'
		  << QString::number( p.pos[2], 'f', 2 ) << '\t' << p.cellX << '\t' << p.cellY << '\t'
		  << kindName( p.kind ) << '\t' << p.nrm[0] << '\t' << p.nrm[1] << '\t' << p.nrm[2] << '\t'
		  << p.width << '\t' << p.height << '\t' << p.sill << '\t'
		  << QStringLiteral( "%1" ).arg( p.doorRef, 8, 16, QLatin1Char( '0' ) ) << '\t'
		  << ( p.roomToRoom ? 1 : 0 ) << '\n';
	}
	t.flush();
	return true;
}

bool probeSoupWrite( const QString & path, const ProbeSoup & soup, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		if ( error )
			*error = f.errorString();
		return false;
	}
	const quint32 head[3] = { 0x31505350u /* 'PSP1' */, quint32( soup.tris.size() / 9 ), quint32( soup.doors.size() ) };
	f.write( reinterpret_cast<const char *>( head ), sizeof head );
	f.write( reinterpret_cast<const char *>( soup.tris.data() ), qint64( soup.tris.size() * sizeof( float ) ) );
	for ( const ProbeSoup::Door & d : soup.doors ) {
		f.write( reinterpret_cast<const char *>( &d.ref ), 4 );
		f.write( reinterpret_cast<const char *>( d.lo ), 12 );
		f.write( reinterpret_cast<const char *>( d.hi ), 12 );
	}
	if ( !soup.alb.empty() && soup.alb.size() * 3 == soup.tris.size() ) {
		const quint32 tail[2] = { 0x31424C41u /* 'ALB1' */, quint32( soup.alb.size() / 3 ) };
		f.write( reinterpret_cast<const char *>( tail ), sizeof tail );
		f.write( reinterpret_cast<const char *>( soup.alb.data() ), qint64( soup.alb.size() ) );
	}
	return true;
}

bool probeSoupRead( const QString & path, ProbeSoup * soup, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error )
			*error = f.errorString();
		return false;
	}
	quint32 head[3];
	if ( f.read( reinterpret_cast<char *>( head ), sizeof head ) != qint64( sizeof head ) || head[0] != 0x31505350u ) {
		if ( error )
			*error = QStringLiteral( "not a PSP1 soup file" );
		return false;
	}
	soup->tris.resize( size_t( head[1] ) * 9 );
	const qint64 want = qint64( soup->tris.size() * sizeof( float ) );
	if ( f.read( reinterpret_cast<char *>( soup->tris.data() ), want ) != want ) {
		if ( error )
			*error = QStringLiteral( "soup file truncated" );
		return false;
	}
	soup->doors.resize( head[2] );
	for ( ProbeSoup::Door & d : soup->doors ) {
		if ( f.read( reinterpret_cast<char *>( &d.ref ), 4 ) != 4 || f.read( reinterpret_cast<char *>( d.lo ), 12 ) != 12
			|| f.read( reinterpret_cast<char *>( d.hi ), 12 ) != 12 ) {
			if ( error )
				*error = QStringLiteral( "soup file truncated in the doors" );
			return false;
		}
	}
	soup->alb.clear();
	quint32 tail[2];
	if ( f.read( reinterpret_cast<char *>( tail ), sizeof tail ) == qint64( sizeof tail ) && tail[0] == 0x31424C41u
		&& tail[1] == head[1] ) {
		soup->alb.resize( size_t( tail[1] ) * 3 );
		if ( f.read( reinterpret_cast<char *>( soup->alb.data() ), qint64( soup->alb.size() ) ) != qint64( soup->alb.size() ) )
			soup->alb.clear();
	}
	return true;
}

int probePlaceCli( const QStringList & args )
{
	QString soupPath, outPath, rect;
	ProbePlaceSpec spec;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outPath = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--spacing" ) ) { spec.spacing = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { spec.red = nx; i++; }
		else if ( a == QLatin1String( "--no-openings" ) ) { spec.apertures = false; }
		else if ( a == QLatin1String( "--no-rooms" ) ) { spec.coverage = false; }
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outPath.isEmpty() || rc.size() != 4 ) {
		std::fprintf( stderr, "usage: probeplace --soup <file> --rect minX,minY,maxX,maxY --out <tsv> "
			"[--spacing s] [--red wall|aperture|frames|coverage] [--no-openings] [--no-rooms]\n" );
		return 2;
	}
	spec.minX = rc[0].toFloat();
	spec.minY = rc[1].toFloat();
	spec.maxX = rc[2].toFloat();
	spec.maxY = rc[3].toFloat();
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "probeplace: %s\n", qPrintable( err ) );
		return 1;
	}
	ProbePlaceResult r;
	if ( !probePlace( soup, spec, &r ) ) {
		std::fprintf( stderr, "probeplace: %s\n", qPrintable( r.error ) );
		return 1;
	}
	if ( !probeWriteTsv( outPath, spec, r, &err ) ) {
		std::fprintf( stderr, "probeplace: %s\n", qPrintable( err ) );
		return 1;
	}
	std::fputs( qPrintable( probeCensusText( r ) ), stdout );
	return 0;
}
