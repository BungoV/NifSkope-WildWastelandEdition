/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENAO_H
#define LODGENAO_H

/* The chunk bake's CPU ambient-occlusion caster, in a header of its own since
 * 2026-09-18 so the `.lodo` writer (src/lodofile.cpp) can cast a model's own
 * self-AO with the SAME rays the chunk vertices get theirs from -- one law for
 * the stock `.BTO` colour B and the native library's selfAO byte. Moved from
 * src/lodgen.cpp verbatim; nothing about the rays changed. */

#include "data/niftypes.h"

#include <cmath>
#include <vector>

#include <QtGlobal>

/* CPU ambient-occlusion over the assembled chunk: a uniform XY grid of
 * triangle bins plus the terrain heightfield. Per vertex, a fixed cosine
 * hemisphere (rotated to the vertex normal) is sampled; ray hits against
 * nearby chunk geometry or the ground darken the vertex. This is the
 * per-PLACEMENT data no shared texture can carry — the reason the B channel
 * exists (docs/TO_BE_IMPLEMENTED.md). */
struct LodgenAoScene
{
	static constexpr int BINS = 64;
	/* The binned area and the heightfield may reach BEYOND the chunk, so the
	 * origin is explicit rather than assumed to be zero. With a bake skirt the
	 * chunk's own geometry sits in the middle of a larger field and skirt
	 * coordinates are negative on two sides. */
	float ox = 0.0f, oy = 0.0f;         // miniature position of the field origin
	float span = 4096.0f;               // miniature span of the BINNED area
	std::vector<float> tri;             // 9 floats per triangle
	std::vector<std::vector<int>> bins; // BINS*BINS triangle lists
	// terrain heightfield in miniature units (n x n), optional
	int hn = 0;
	float hSpacing = 1.0f;
	std::vector<float> hgt;

	void addTriangle( const Vector3 & a, const Vector3 & b, const Vector3 & c )
	{
		const int t = int( tri.size() / 9 );
		for ( const Vector3 * p : { &a, &b, &c } ) {
			tri.push_back( (*p)[0] );
			tri.push_back( (*p)[1] );
			tri.push_back( (*p)[2] );
		}
		if ( bins.empty() )
			bins.resize( BINS * BINS );
		const float mnx = qMin( a[0], qMin( b[0], c[0] ) ), mxx = qMax( a[0], qMax( b[0], c[0] ) );
		const float mny = qMin( a[1], qMin( b[1], c[1] ) ), mxy = qMax( a[1], qMax( b[1], c[1] ) );
		const int bx0 = qBound( 0, int( ( mnx - ox ) / span * BINS ), BINS - 1 );
		const int bx1 = qBound( 0, int( ( mxx - ox ) / span * BINS ), BINS - 1 );
		const int by0 = qBound( 0, int( ( mny - oy ) / span * BINS ), BINS - 1 );
		const int by1 = qBound( 0, int( ( mxy - oy ) / span * BINS ), BINS - 1 );
		for ( int by = by0; by <= by1; by++ )
			for ( int bx = bx0; bx <= bx1; bx++ )
				bins[by * BINS + bx].push_back( t );
	}

	float groundHeight( float x, float y ) const
	{
		if ( !hn )
			return -3.4e38f;
		const float fx = qBound( 0.0f, ( x - ox ) / hSpacing, float( hn - 1 ) - 0.001f );
		const float fy = qBound( 0.0f, ( y - oy ) / hSpacing, float( hn - 1 ) - 0.001f );
		const int ix = int( fx ), iy = int( fy );
		const float tx = fx - ix, ty = fy - iy;
		const float h00 = hgt[size_t( iy ) * hn + ix], h10 = hgt[size_t( iy ) * hn + ix + 1];
		const float h01 = hgt[size_t( iy + 1 ) * hn + ix], h11 = hgt[size_t( iy + 1 ) * hn + ix + 1];
		return ( h00 * ( 1 - tx ) + h10 * tx ) * ( 1 - ty )
			+ ( h01 * ( 1 - tx ) + h11 * tx ) * ty;
	}

	bool rayHit( const Vector3 & o, const Vector3 & d, float maxT ) const
	{
		// terrain: march and compare against the heightfield
		if ( hn && d[2] < 0.9f ) {
			for ( float t = 8.0f; t < maxT; t += 24.0f ) {
				const float x = o[0] + d[0] * t, y = o[1] + d[1] * t;
				if ( x < ox || y < oy || x > ox + span || y > oy + span )
					break;
				if ( o[2] + d[2] * t < groundHeight( x, y ) )
					return true;
			}
		}
		if ( bins.empty() )
			return false;
		// DDA over the XY bins
		const float cell = span / BINS;
		float t = 0.0f;
		int guard = 0;
		while ( t < maxT && guard++ < 2 * BINS ) {
			const float x = o[0] + d[0] * t, y = o[1] + d[1] * t;
			const int bx = int( ( x - ox ) / cell ), by = int( ( y - oy ) / cell );
			if ( bx < 0 || by < 0 || bx >= BINS || by >= BINS )
				break;
			for ( int ti : bins[by * BINS + bx] ) {
				const float * p = tri.data() + size_t( ti ) * 9;
				// Moller-Trumbore
				const Vector3 v0( p[0], p[1], p[2] ), v1( p[3], p[4], p[5] ), v2( p[6], p[7], p[8] );
				const Vector3 e1 = v1 - v0, e2 = v2 - v0;
				const Vector3 pv = Vector3::crossproduct( d, e2 );
				const float det = Vector3::dotproduct( e1, pv );
				if ( std::fabs( det ) < 1e-8f )
					continue;
				const float inv = 1.0f / det;
				const Vector3 tv = o - v0;
				const float u = Vector3::dotproduct( tv, pv ) * inv;
				if ( u < 0.0f || u > 1.0f )
					continue;
				const Vector3 qv = Vector3::crossproduct( tv, e1 );
				const float vv = Vector3::dotproduct( d, qv ) * inv;
				if ( vv < 0.0f || u + vv > 1.0f )
					continue;
				const float hitT = Vector3::dotproduct( e2, qv ) * inv;
				if ( hitT > 1.0f && hitT < maxT )
					return true;
			}
			// advance to the next bin boundary along the dominant axis
			const float step = cell / qMax( 0.05f,
				qMax( std::fabs( d[0] ), std::fabs( d[1] ) ) );
			t += step;
		}
		return false;
	}

	/*! Fraction of the UPPER hemisphere that reaches open sky.
	 *
	 *  Not ambient occlusion with a different name: AO is cosine-weighted about
	 *  the surface normal and answers "how enclosed is this point", while this
	 *  is normal-independent and answers "can weather and skylight land here".
	 *  A vertical wall face has low AO and high sky visibility; the floor of a
	 *  narrow gully has the reverse.
	 */
	float skyVisibility( const Vector3 & p, float maxT ) const
	{
		static const float dirs[9][3] = {
			{ 0.0f, 0.0f, 1.0f },
			{ 0.5f, 0.0f, 0.87f }, { -0.5f, 0.0f, 0.87f },
			{ 0.0f, 0.5f, 0.87f }, { 0.0f, -0.5f, 0.87f },
			{ 0.7f, 0.0f, 0.71f }, { -0.7f, 0.0f, 0.71f },
			{ 0.0f, 0.7f, 0.71f }, { 0.0f, -0.7f, 0.71f } };
		const Vector3 o = p + Vector3( 0.0f, 0.0f, 2.0f );
		int open = 0;
		for ( const auto & dv : dirs ) {
			Vector3 d( dv[0], dv[1], dv[2] );
			d.normalize();
			if ( !rayHit( o, d, maxT ) )
				open++;
		}
		return float( open ) / 9.0f;
	}

	float ambientOcclusion( const Vector3 & p, const Vector3 & n, float maxT ) const
	{
		// 8 fixed hemisphere directions blended toward the normal
		static const float dirs[8][3] = {
			{ 0.7f, 0.0f, 0.7f }, { -0.7f, 0.0f, 0.7f },
			{ 0.0f, 0.7f, 0.7f }, { 0.0f, -0.7f, 0.7f },
			{ 0.5f, 0.5f, 0.7f }, { -0.5f, 0.5f, 0.7f },
			{ 0.5f, -0.5f, 0.7f }, { -0.5f, -0.5f, 0.7f } };
		const Vector3 o = p + n * 2.0f;
		int hits = 0;
		for ( const auto & dv : dirs ) {
			Vector3 d( dv[0], dv[1], dv[2] );
			d = d + n * 0.6f;
			d.normalize();
			if ( Vector3::dotproduct( d, n ) < 0.05f )
				continue;
			if ( rayHit( o, d, maxT ) )
				hits++;
		}
		return 1.0f - 0.85f * float( hits ) / 8.0f;
	}
};

#endif // LODGENAO_H
