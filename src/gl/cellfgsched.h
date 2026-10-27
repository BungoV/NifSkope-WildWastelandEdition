#ifndef WW_CELLFGSCHED_H
#define WW_CELLFGSCHED_H

/* lane FARPAGES1: the far ground pages' sizes, bytes and repaint scheduler, free of Qt and GL so that
 * tests/fgpages can run them offline (src/gl/cellfarground.cpp does the GL half). One page a ring-0 cell, in one of
 * two pools: near (the finer side) or far. A cell with no page draws the baked sheet and fades to its page. */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace FgSched
{

constexpr float kCell = 4096.0f;
constexpr int kFadeStep = 32;	// 255 / 32: a fresh page fades in over 8 frames

//! A pool's page side by quality (Low/Medium/High) and the near-split factor k: near while texelFar > k x footprint.
constexpr int kNearSide[3] = { 256, 512, 1024 };
constexpr int kFarSide[3] = { 128, 256, 512 };
constexpr float kSplitK[3] = { 2.0f, 1.0f, 0.5f };

inline int levels( int side )
{
	int n = 0;
	for ( int s = side; s >= 4; s >>= 1 )
		n++;
	return n;	// down to 4x4: one BC1 block
}

inline std::int64_t bc1Bytes( int side )
{
	std::int64_t b = 0;
	for ( int s = side; s >= 4; s >>= 1 )
		b += std::int64_t( s / 4 ) * ( s / 4 ) * 8;
	return b;
}

inline std::int64_t rgba8Bytes( int side )
{
	std::int64_t b = 0;
	for ( int s = side; s >= 4; s >>= 1 )
		b += std::int64_t( s ) * s * 4;
	return b;
}

inline std::int64_t pageBytes( int side, bool compressed )
{
	return compressed ? bc1Bytes( side ) : rgba8Bytes( side );
}

//! The distance under which the far page's texel is coarser than k screen pixels (pixAngle = radians a pixel).
inline float nearDistance( int quality, float pixAngle )
{
	const float texelFar = kCell / float( kFarSide[quality] );
	return texelFar / ( kSplitK[quality] * std::max( pixAngle, 1e-6f ) );
}

//! 2D distance from (x, y) to the nearest point of cell (cx, cy) (world cell indices).
inline float cellDistance( float x, float y, int cx, int cy )
{
	const float x0 = float( cx ) * kCell, y0 = float( cy ) * kCell;
	const float dx = std::max( { x0 - x, 0.0f, x - ( x0 + kCell ) } );
	const float dy = std::max( { y0 - y, 0.0f, y - ( y0 + kCell ) } );
	return std::sqrt( dx * dx + dy * dy );
}

struct Cell
{
	int wx = 0, wy = 0;		// world cell
	int pool = 0;			// 0 none, 1 near, 2 far
	int layer = -1;
	int fade = 0;			// 0..255
};

struct View
{
	float x = 0, y = 0;			// camera, world
	float fx = 0, fy = 1;		// forward, xy (need not be unit)
	float halfFov = 0.8f;		// horizontal, radians
	float nearDist = 0;			// nearDistance()
	int sideN = 512, sideF = 256;
	std::int64_t budget = 4LL * 512 * 512;	// texels painted a frame (at least one page always goes)
};

struct Job
{
	int cell, pool;
};

class Sched
{
public:
	std::vector<Cell> cells;
	std::vector<int> freeN, freeF;
	int capN = 0, capF = 0;

	//! Cells and pools: capacity per pool. Every cell drops its page.
	void reset( int nearCap, int farCap )
	{
		capN = nearCap;
		capF = farCap;
		freeN.clear();
		freeF.clear();
		for ( int i = nearCap - 1; i >= 0; i-- )
			freeN.push_back( i );
		for ( int i = farCap - 1; i >= 0; i-- )
			freeF.push_back( i );
		for ( Cell & c : cells ) {
			c.pool = 0;
			c.layer = -1;
			c.fade = 0;
		}
	}

	static bool visible( const View & v, const Cell & c )
	{
		const float cx = ( float( c.wx ) + 0.5f ) * kCell - v.x, cy = ( float( c.wy ) + 0.5f ) * kCell - v.y;
		if ( cellDistance( v.x, v.y, c.wx, c.wy ) < kCell )
			return true;
		const float fl = std::sqrt( v.fx * v.fx + v.fy * v.fy ), cl = std::sqrt( cx * cx + cy * cy );
		if ( fl < 1e-6f || cl < 1e-6f )
			return true;
		const float cosA = ( v.fx * cx + v.fy * cy ) / ( fl * cl );
		return cosA >= std::cos( std::min( 3.14159f, v.halfFov + 0.2618f ) );	// + 15 degrees
	}

	//! The pool a cell wants (with a 10 % band so a cell on the line does not flip every frame).
	static int wantPool( const View & v, const Cell & c )
	{
		const float d = cellDistance( v.x, v.y, c.wx, c.wy );
		if ( d < v.nearDist )
			return 1;
		if ( d > v.nearDist * 1.1f || c.pool == 0 )
			return 2;
		return c.pool;
	}

	//! This frame's paints, nearest first: cells with no page, then pool moves; visible before hidden.
	std::vector<Job> plan( const View & v ) const
	{
		struct Cand
		{
			int cell, pool, has, hidden;
			float d;
		};
		std::vector<Cand> cand;
		for ( int i = 0; i < int( cells.size() ); i++ ) {
			const Cell & c = cells[size_t( i )];
			const int w = wantPool( v, c );
			if ( c.pool == w )
				continue;
#ifdef FG_SCHED_RED_VISONLY
			if ( !visible( v, c ) )
				continue;
#endif
			cand.push_back( { i, w, c.pool ? 1 : 0, visible( v, c ) ? 0 : 1, cellDistance( v.x, v.y, c.wx, c.wy ) } );
		}
		std::sort( cand.begin(), cand.end(), []( const Cand & a, const Cand & b ) {
			if ( a.has != b.has )
				return a.has < b.has;
			if ( a.hidden != b.hidden )
				return a.hidden < b.hidden;
			if ( a.d != b.d )
				return a.d < b.d;
			return a.cell < b.cell;
		} );
		std::vector<Job> out;
		int fN = int( freeN.size() ), fF = int( freeF.size() );
		std::int64_t spent = 0;
		for ( const Cand & c : cand ) {
			int pool = c.pool;
			if ( pool == 1 && fN == 0 )
				pool = c.has ? 0 : ( fF > 0 ? 2 : 0 );	// near full: a cell without a page takes a far one
			else if ( pool == 2 && fF == 0 )
				pool = c.has ? 0 : ( fN > 0 ? 1 : 0 );
			if ( !pool || pool == cells[size_t( c.cell )].pool )
				continue;
			const int side = pool == 1 ? v.sideN : v.sideF;
			const std::int64_t cost = std::int64_t( side ) * side;
#ifndef FG_SCHED_RED_NOBUDGET
			if ( !out.empty() && spent + cost > v.budget )
				break;
#endif
			spent += cost;
			( pool == 1 ? fN : fF )--;
			out.push_back( { c.cell, pool } );
		}
		return out;
	}

	//! A painted job's slot: takes a free layer of its pool and frees the cell's old one. Returns the layer.
	int commit( const Job & j )
	{
		Cell & c = cells[size_t( j.cell )];
		std::vector<int> & to = j.pool == 1 ? freeN : freeF;
		if ( to.empty() )
			return -1;
		const int layer = to.back();
		to.pop_back();
#ifndef FG_SCHED_RED_NOFREE
		if ( c.pool )
			( c.pool == 1 ? freeN : freeF ).push_back( c.layer );
#endif
		if ( !c.pool )
			c.fade = 0;	// fades in from the sheet; a pool move keeps showing (same picture, other size)
		c.pool = j.pool;
		c.layer = layer;
		return layer;
	}

	//! One frame of the fades. True while any cell still fades or lacks a page.
	bool tick()
	{
		bool busy = false;
		for ( Cell & c : cells ) {
			if ( !c.pool ) {
				busy = true;
				continue;
			}
			if ( c.fade < 255 ) {
				c.fade = std::min( 255, c.fade + kFadeStep );
				busy = true;
			}
		}
		return busy;
	}
};

//! The pools' VRAM ceiling by quality (BC1 bytes): Medium is the ~14 MB target, Low a quarter, High four times.
constexpr std::int64_t kPoolBudget[3] = { 14LL << 18, 14LL << 20, 14LL << 22 };

//! Pool sizes for n cells, nNear of them near at the cut centre: 4 spare layers each (a cell moving between pools
//! paints before it frees), the near pool trimmed to the ceiling (those cells take far pages instead).
inline void poolCaps( int n, int nNear, int quality, int & capN, int & capF )
{
	capN = std::max( 1, std::min( n, nNear + 4 ) );
	capF = std::max( 1, std::min( n, n - nNear + 4 ) );
	const std::int64_t pn = bc1Bytes( kNearSide[quality] ), pf = bc1Bytes( kFarSide[quality] );
	while ( capN > 1 && pn * capN + pf * capF > kPoolBudget[quality] ) {
		capN--;
		capF = std::min( n, std::max( capF, n - capN + 4 ) );
	}
}

//! Near cells for the camera at the cut's centre (the pool size): the cells closer than nearDist.
inline int nearCount( const std::vector<Cell> & cells, float camX, float camY, float nearDist )
{
	int n = 0;
	for ( const Cell & c : cells )
		n += cellDistance( camX, camY, c.wx, c.wy ) < nearDist ? 1 : 0;
	return n;
}

}

#endif
