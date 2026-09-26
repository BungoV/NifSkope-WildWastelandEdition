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
	// lane AO2: z bounds for the face caster (rayHitFace). Filled by addTriangle and
	// prepareGround; the three stock casters (rayHit and its two callers) never read them.
	std::vector<float> binZ;            // 2 a bin: lowest, highest z of the triangles listed in it
	float triZMin = 3.4e38f, triZMax = -3.4e38f;
	float groundZMax = 3.4e38f;         // +inf until prepareGround(): no early out

	void addTriangle( const Vector3 & a, const Vector3 & b, const Vector3 & c )
	{
		const int t = int( tri.size() / 9 );
		for ( const Vector3 * p : { &a, &b, &c } ) {
			tri.push_back( (*p)[0] );
			tri.push_back( (*p)[1] );
			tri.push_back( (*p)[2] );
		}
		if ( bins.empty() ) {
			bins.resize( BINS * BINS );
			binZ.assign( size_t( 2 * BINS * BINS ), 0.0f );
			for ( int k = 0; k < BINS * BINS; k++ ) {
				binZ[size_t( 2 * k )] = 3.4e38f;
				binZ[size_t( 2 * k + 1 )] = -3.4e38f;
			}
		}
		const float mnz = qMin( a[2], qMin( b[2], c[2] ) ), mxz = qMax( a[2], qMax( b[2], c[2] ) );
		triZMin = qMin( triZMin, mnz );
		triZMax = qMax( triZMax, mxz );
		const float mnx = qMin( a[0], qMin( b[0], c[0] ) ), mxx = qMax( a[0], qMax( b[0], c[0] ) );
		const float mny = qMin( a[1], qMin( b[1], c[1] ) ), mxy = qMax( a[1], qMax( b[1], c[1] ) );
		const int bx0 = qBound( 0, int( ( mnx - ox ) / span * BINS ), BINS - 1 );
		const int bx1 = qBound( 0, int( ( mxx - ox ) / span * BINS ), BINS - 1 );
		const int by0 = qBound( 0, int( ( mny - oy ) / span * BINS ), BINS - 1 );
		const int by1 = qBound( 0, int( ( mxy - oy ) / span * BINS ), BINS - 1 );
		for ( int by = by0; by <= by1; by++ )
			for ( int bx = bx0; bx <= bx1; bx++ ) {
				bins[by * BINS + bx].push_back( t );
				float & lo = binZ[size_t( 2 * ( by * BINS + bx ) )];
				float & hi = binZ[size_t( 2 * ( by * BINS + bx ) + 1 )];
				lo = qMin( lo, mnz );
				hi = qMax( hi, mxz );
			}
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

	/* ==== THE FACE CASTER (lane AO2, 2026-09-26) ================================
	 * bungo on the Boston oblique: "isn't the vertex AO kind of strong and placed
	 * not in the right places?". Used ONLY by the native `.lodi` v6 vertex-AO and
	 * v7 vertex-sky streams (src/nativeemit.cpp). The `.BTO` colour B, the v5
	 * placement byte and the `.lodo` selfAO keep rayHit / ambientOcclusion /
	 * skyVisibility above, byte for byte.
	 *
	 * What differs from rayHit, and why:
	 *  - the bins are walked cell by cell (Amanatides-Woo). rayHit steps one bin
	 *    length along the ray from t = 0 and can step over the corner of a bin a
	 *    diagonal ray only clips, so it misses hits a brute-force cast finds
	 *    (measured: 2 of 59 vertices on the Trinity Church tower);
	 *  - a bin whose triangles all lie above or below the ray's z over that bin is
	 *    skipped, and an upward ray stops once it is above every triangle and the
	 *    whole heightfield. Exact, not a sampling change: any hit lies in the bin
	 *    that holds its own point, at a z inside that bin's bounds;
	 *  - with `hit` it returns the NEAREST hit (t, triangle, facing), which the
	 *    buried-sample census and the sky reach census read.
	 * The terrain march is rayHit's (t from 8, step 24, rays with d.z < 0.9). */
	struct FaceHit
	{
		float t = 0.0f;
		int tri = -1;        // index into tri / 9, -1 = terrain
		bool back = false;   // the ray met the triangle's back (the origin is behind it)
	};

	void prepareGround()
	{
		groundZMax = -3.4e38f;
		for ( float h : hgt )
			groundZMax = qMax( groundZMax, h );
		if ( hgt.empty() )
			groundZMax = -3.4e38f;
	}

	bool rayHitFace( const Vector3 & o, const Vector3 & d, float maxT, FaceHit * hit = nullptr ) const
	{
		float best = 3.4e38f;
		int bestTri = -1;
		bool bestBack = false, any = false;
		if ( hn && d[2] < 0.9f ) {
			for ( float t = 8.0f; t < maxT; t += 24.0f ) {
				const float z = o[2] + d[2] * t;
				if ( d[2] >= 0.0f && z > groundZMax )
					break;
				const float x = o[0] + d[0] * t, y = o[1] + d[1] * t;
				if ( x < ox || y < oy || x > ox + span || y > oy + span )
					break;
				if ( z < groundHeight( x, y ) ) {
					if ( !hit )
						return true;
					best = t;
					any = true;
					break;
				}
			}
		}
		if ( !bins.empty() ) {
			const float cell = span / BINS;
			const float fx = ( o[0] - ox ) / cell, fy = ( o[1] - oy ) / cell;
			if ( fx >= 0.0f && fy >= 0.0f && fx < float( BINS ) && fy < float( BINS ) ) {
				int bx = int( fx ), by = int( fy );
				const int sx = d[0] >= 0.0f ? 1 : -1, sy = d[1] >= 0.0f ? 1 : -1;
				const float adx = std::fabs( d[0] ), ady = std::fabs( d[1] );
				constexpr float INF = 3.4e38f;
				float tMaxX = adx > 1e-9f ? ( sx > 0 ? float( bx + 1 ) - fx : fx - float( bx ) ) * cell / adx : INF;
				float tMaxY = ady > 1e-9f ? ( sy > 0 ? float( by + 1 ) - fy : fy - float( by ) ) * cell / ady : INF;
				const float tDX = adx > 1e-9f ? cell / adx : INF, tDY = ady > 1e-9f ? cell / ady : INF;
				float tIn = 0.0f;
				for ( ;; ) {
					const float tOut = qMin( qMin( tMaxX, tMaxY ), maxT );
					const float zIn = o[2] + d[2] * tIn, zOut = o[2] + d[2] * tOut;
					if ( ( d[2] >= 0.0f && zIn > triZMax ) || ( d[2] <= 0.0f && zIn < triZMin ) )
						break;
					const int b = by * BINS + bx;
					const float zlo = qMin( zIn, zOut ) - 0.01f, zhi = qMax( zIn, zOut ) + 0.01f;
					if ( zhi >= binZ[size_t( 2 * b )] && zlo <= binZ[size_t( 2 * b + 1 )] ) {
						for ( int ti : bins[size_t( b )] ) {
							const float * p = tri.data() + size_t( ti ) * 9;
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
							if ( hitT > 1.0f && hitT < maxT && hitT < best ) {
								if ( !hit )
									return true;
								best = hitT;
								bestTri = ti;
								bestBack = det < 0.0f;   // det = -d . (e1 x e2): negative = the back
								any = true;
							}
						}
					}
					// nearest: a later bin can only hold hits past this bin's exit
					if ( best <= tOut || tOut >= maxT )
						break;
					if ( tMaxX < tMaxY ) {
						bx += sx;
						if ( bx < 0 || bx >= BINS )
							break;
						tIn = tMaxX;
						tMaxX += tDX;
					} else {
						by += sy;
						if ( by < 0 || by >= BINS )
							break;
						tIn = tMaxY;
						tMaxY += tDY;
					}
				}
			}
		}
		if ( hit && any ) {
			hit->t = best;
			hit->tri = bestTri;
			hit->back = bestTri >= 0 && bestBack;
		}
		return any;
	}

	/*! The vertex-AO law of the stock caster -- the same 8 directions blended 0.6
	 *  toward the normal, the same 2-unit origin offset, the same 1 - 0.85 x
	 *  hits / 8 -- cast through rayHitFace. `buried` is set when every cast ray's
	 *  nearest hit is a BACK face: the point is inside closed geometry. */
	float ambientOcclusionFace( const Vector3 & p, const Vector3 & n, float maxT, bool * buried = nullptr ) const
	{
		static const float dirs[8][3] = {
			{ 0.7f, 0.0f, 0.7f }, { -0.7f, 0.0f, 0.7f },
			{ 0.0f, 0.7f, 0.7f }, { 0.0f, -0.7f, 0.7f },
			{ 0.5f, 0.5f, 0.7f }, { -0.5f, 0.5f, 0.7f },
			{ 0.5f, -0.5f, 0.7f }, { -0.5f, -0.5f, 0.7f } };
		const Vector3 o = p + n * 2.0f;
		int hits = 0, cast = 0, back = 0;
		for ( const auto & dv : dirs ) {
			Vector3 d( dv[0], dv[1], dv[2] );
			d = d + n * 0.6f;
			d.normalize();
			if ( Vector3::dotproduct( d, n ) < 0.05f )
				continue;
			cast++;
			FaceHit h;
			if ( rayHitFace( o, d, maxT, buried ? &h : nullptr ) ) {
				hits++;
				back += h.back ? 1 : 0;
			}
		}
		if ( buried )
			*buried = cast > 0 && back == cast;
		return 1.0f - 0.85f * float( hits ) / 8.0f;
	}

	/*! THE HORIZON-AWARE SKY LAW (bungo 2026-09-26: "Yes, horizon aware would be
	 *  preferable"). The fraction of the sky's IRRADIANCE on a horizontal receiver
	 *  at p that arrives unblocked -- cosine-weighted about world +Z, the FO4CS
	 *  Skylighting convention (its directions are r = sqrt(u), z = sqrt(1 - r^2)).
	 *  Still normal-independent in its directions ("can skylight and weather land
	 *  here"); the normal only moves the origin off the surface, as the AO does.
	 *
	 *  Seven elevation bands 0-10-20-30-45-60-75-90 deg. A band [a, b] carries
	 *  sin^2 b - sin^2 a of the irradiance (the integral of sin e cos e de,
	 *  normalised), so 0-10 = 0.030, 10-20 = 0.087, 20-30 = 0.133, 30-45 = 0.25,
	 *  45-60 = 0.25, 60-75 = 0.183, 75-90 = 0.067. Each band is cast as one ring at
	 *  its irradiance-median elevation, sin^2 e = (sin^2 a + sin^2 b) / 2, with
	 *  RING_AZ azimuths, odd rings turned half a step. 56 fixed rays, no RNG. The
	 *  stock skyVisibility cast 9 rays all within 45 deg of the zenith, so a street
	 *  canyon's walls (below 45 deg elevation seen from the street) never counted.
	 *
	 *  `hist` (optional, 8 counters) takes each blocked ray's nearest-hit distance
	 *  in WORLD units (t x worldPerUnit): < 250, 500, 1000, 2000, 4000, 8000,
	 *  16000, beyond. */
	static constexpr int SKY_RINGS = 7, RING_AZ = 8;
	struct SkyDir { float d[3]; float w; };
	static const SkyDir * skyDirs()
	{
		static const std::vector<SkyDir> table = [] {
			const double band[SKY_RINGS + 1] = { 0, 10, 20, 30, 45, 60, 75, 90 };
			const double kPi = 3.14159265358979323846;
			std::vector<SkyDir> t;
			for ( int r = 0; r < SKY_RINGS; r++ ) {
				const double sa = std::sin( band[r] * kPi / 180.0 ), sb = std::sin( band[r + 1] * kPi / 180.0 );
				const double w = sb * sb - sa * sa;
				const double e = std::asin( std::sqrt( 0.5 * ( sa * sa + sb * sb ) ) );
				for ( int k = 0; k < RING_AZ; k++ ) {
					const double az = ( double( k ) + ( ( r & 1 ) ? 0.5 : 0.0 ) ) * 2.0 * kPi / RING_AZ;
					SkyDir sd;
					sd.d[0] = float( std::cos( e ) * std::cos( az ) );
					sd.d[1] = float( std::cos( e ) * std::sin( az ) );
					sd.d[2] = float( std::sin( e ) );
					sd.w = float( w / RING_AZ );
					t.push_back( sd );
				}
			}
			return t;
		}();
		return table.data();
	}

	float skyVisibilityFace( const Vector3 & p, const Vector3 & n, float maxT, float worldPerUnit = 1.0f,
		quint64 * hist = nullptr ) const
	{
		const SkyDir * sd = skyDirs();
		const Vector3 o = p + n * 2.0f;
		double open = 0.0;
		for ( int k = 0; k < SKY_RINGS * RING_AZ; k++ ) {
			const Vector3 d( sd[k].d[0], sd[k].d[1], sd[k].d[2] );
			FaceHit h;
			if ( !rayHitFace( o, d, maxT, hist ? &h : nullptr ) ) {
				open += sd[k].w;
			} else if ( hist ) {
				const float w = h.t * worldPerUnit;
				int b = 0;
				for ( float lim = 250.0f; b < 7 && w >= lim; lim *= 2.0f )
					b++;
				hist[b]++;
			}
		}
		return float( open );
	}

	/*! Across-the-face sample points of one triangle (bungo 2026-09-26: "Sample
	 *  across the face sounds good"). The triangle is cut into k x k equal
	 *  sub-triangles and each one's centroid is a sample, so every sample stands
	 *  for the same area. Returns barycentric weights (a, b, c), 3 floats a sample,
	 *  k^2 samples. A fixed pattern: no RNG, no seed. */
	static void faceSamples( int k, std::vector<float> & out )
	{
		out.clear();
		const float inv = 1.0f / float( k );
		for ( int i = 0; i < k; i++ )
			for ( int j = 0; i + j < k; j++ ) {
				// upright sub-triangle (i, j): corners (i,j) (i+1,j) (i,j+1) in b/c steps
				float wb = ( float( i ) + 1.0f / 3.0f ) * inv, wc = ( float( j ) + 1.0f / 3.0f ) * inv;
				out.push_back( 1.0f - wb - wc );
				out.push_back( wb );
				out.push_back( wc );
				if ( i + j + 1 < k ) {
					// the inverted one beside it: (i+1,j) (i,j+1) (i+1,j+1)
					wb = ( float( i ) + 2.0f / 3.0f ) * inv;
					wc = ( float( j ) + 2.0f / 3.0f ) * inv;
					out.push_back( 1.0f - wb - wc );
					out.push_back( wb );
					out.push_back( wc );
				}
			}
	}
};

#endif // LODGENAO_H
