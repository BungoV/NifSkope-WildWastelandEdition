#ifndef WWALPHACOV_H
#define WWALPHACOV_H

/* lane TREELEAF1 (bungo 10-19: "near tree leaves break into flecks with hard cut edges, worse at distance").
 *
 * An alpha-tested texture's mips are filtered: each smaller level averages the leaf with the space around it, so the
 * share of texels that pass the test (alpha > ref) drifts level by level (the foliage census on the game's and BNS's
 * leaf atlases: down to 0.68 of level 0 at 16 x 16 for ref 185, up to 3 x for ref 35), and the leaves break into
 * flecks (or blobs) as they get smaller on screen. Coverage-preserving alpha mips (I. Castano, "Computing Alpha
 * Mipmaps", 2010; B. Golus, "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage", 2017, its mip-scale section):
 * scale each level's alpha by the one factor that gives it level 0's coverage at the material's threshold.
 *
 * Two additions the game's files need (both MEASURED on the census, notes/fix1030b/treeleaf1):
 *  - Ties: many texels of a level share one decoded alpha (BC3 palettes), so a scale moves the coverage in steps, some
 *    over 2 %. And some vanilla plant mips are 1-bit (alpha 0 or 255 only; hedgerow, forsythia, dead shrub): no scale
 *    can lower their coverage without dropping it to 0. After the scale, the texels still over or under level 0's
 *    count are moved across the test in order of (scaled alpha, then the 3 x 3 neighbourhood's alpha): the texel inside
 *    the leaf mass passes first, the isolated one fails first (an erosion / dilation of the silhouette edge).
 *  - BC3 re-encode: each block's 16 alpha values are decoded, scaled and encoded again (8-value mode, endpoints = the
 *    block's floor(min) / ceil(max)); every texel takes the nearest palette entry on the side of the test it must be on.
 *    Scaling the two endpoints alone is not enough: a leaf-edge block spanning 0..255 keeps its palette under any scale
 *    (the unit's red RED_ENDPOINTS). The colour half of the block (bytes 8..15) is never touched.
 *
 * Pure C++, no Qt, no GL: the texture loader (gltexloaders.cpp) and the unit test (tests/treeleaf1) run this same code.
 * Tests count exactly as the shader runs: alpha / 255 > ref / 255, the BC3 palette as exact fractions. */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace WwAlphaCov
{

struct LevelStat
{
	int w = 0, h = 0;
	double before = 0.0;	// coverage of the level as it came in
	double after = 0.0;	// coverage after the rewrite (== before when the level was left alone)
	double scale = 1.0;	// the factor applied to the level's alpha (1: none)
	std::uint64_t moved = 0;	// texels moved across the test after the scale (the tie-break); 0 for an untouched level
};

//! a level's bytes: 16 per 4 x 4 block
inline size_t levelBytes( int w, int h )
{
	return size_t( ( w + 3 ) / 4 ) * size_t( ( h + 3 ) / 4 ) * 16;
}

//! a BC3 alpha block's 16 values in 1/35 units (exact: the 8-value palette is in sevenths, the 6-value one in fifths)
inline void decodeBC3Alpha( const std::uint8_t * b, int out[16] )
{
	const int a0 = b[0], a1 = b[1];
	int pal[8];
	pal[0] = a0 * 35;
	pal[1] = a1 * 35;
	if ( a0 > a1 ) {
		for ( int k = 2; k < 8; k++ )
			pal[k] = ( ( 8 - k ) * a0 + ( k - 1 ) * a1 ) * 5;
	} else {
		for ( int k = 2; k < 6; k++ )
			pal[k] = ( ( 6 - k ) * a0 + ( k - 1 ) * a1 ) * 7;
		pal[6] = 0;
		pal[7] = 255 * 35;
	}
	std::uint64_t bits = 0;
	for ( int k = 0; k < 6; k++ )
		bits |= std::uint64_t( b[2 + k] ) << ( 8 * k );
	for ( int t = 0; t < 16; t++ )
		out[t] = pal[( bits >> ( 3 * t ) ) & 7];
}

//! does a value in 1/35 units, scaled by s, pass alpha > ref? (the one comparison the count and the rewrite share)
inline bool scaledPasses( int u35, double s, int ref )
{
	return double( u35 ) * s > double( ref ) * 35.0;
}

/*! One BC3 alpha block from 16 wanted values (0..255, any real) and the side of the test each must be on: 8-value
 *  mode, endpoints floor(min) and ceil(max) of the wanted values, widened so each side that has a texel has an entry
 *  (both endpoints are entries); each texel on the nearest entry of its side. A wanted value on the wrong side of ref
 *  (a tie-break texel) is first set to ref + 1 / ref. */
inline void encodeBC3AlphaSided( const double want[16], const bool pass[16], int ref, std::uint8_t * b )
{
	double x[16];
	double mn = 1e9, mx = -1e9;
	bool anyPass = false, anyFail = false;
	for ( int t = 0; t < 16; t++ ) {
		x[t] = std::clamp( want[t], 0.0, 255.0 );
		if ( pass[t] && !( x[t] > double( ref ) ) )
			x[t] = double( ref + 1 );
		else if ( !pass[t] && x[t] > double( ref ) )
			x[t] = double( ref );
		mn = std::min( mn, x[t] );
		mx = std::max( mx, x[t] );
		( pass[t] ? anyPass : anyFail ) = true;
	}
	int hi = std::clamp( int( std::ceil( mx ) ), 0, 255 ), lo = std::clamp( int( std::floor( mn ) ), 0, 255 );
	if ( anyPass )
		hi = std::max( hi, ref + 1 );
	if ( anyFail )
		lo = std::min( lo, ref );
	std::uint64_t bits = 0;
	if ( hi <= lo ) {	// one value: 6-value mode, every texel on entry 0 (= a0 = a1)
		b[0] = b[1] = std::uint8_t( hi );
	} else {
		b[0] = std::uint8_t( hi );
		b[1] = std::uint8_t( lo );
		int pal7[8];	// entries in 1/7 units
		pal7[0] = hi * 7;
		pal7[1] = lo * 7;
		for ( int k = 2; k < 8; k++ )
			pal7[k] = ( 8 - k ) * hi + ( k - 1 ) * lo;
		for ( int t = 0; t < 16; t++ ) {
			int best = pass[t] ? 0 : 1;
			double bestD = 1e18;
			for ( int k = 0; k < 8; k++ ) {
#ifndef WWALPHACOV_RED_NEAREST	// the unit's red: the nearest entry whatever its side (texels near ref flip)
				if ( ( pal7[k] > 7 * ref ) != pass[t] )
					continue;
#endif
				const double d = std::fabs( double( pal7[k] ) - x[t] * 7.0 );
				if ( d < bestD ) {
					bestD = d;
					best = k;
				}
			}
			bits |= std::uint64_t( best ) << ( 3 * t );
		}
	}
	for ( int k = 0; k < 6; k++ )
		b[2 + k] = std::uint8_t( bits >> ( 8 * k ) );
}

/*! One level's alpha, decoded (1/35 units, w x h, row-major) and as a histogram (0..8925), so a scale's coverage costs
 *  one walk over the bins. Texels outside the level's real extent (a 2 x 2 level sits in one 4 x 4 block) do not count. */
struct Level
{
	int w = 0, h = 0;
	std::vector<int> u;
	std::vector<std::uint64_t> bins;
	std::uint64_t texels = 0;

	void build( const std::uint8_t * d, int lw, int lh, bool keepTexels = true )
	{
		w = lw;
		h = lh;
		texels = std::uint64_t( w ) * std::uint64_t( h );
		u.assign( keepTexels ? size_t( texels ) : 0, 0 );
		bins.assign( 255 * 35 + 1, 0 );
		const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
		int v[16];
		for ( int by = 0; by < bh; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			decodeBC3Alpha( d + ( size_t( by ) * size_t( bw ) + size_t( bx ) ) * 16, v );
			for ( int t = 0; t < 16; t++ ) {
				const int x = bx * 4 + ( t & 3 ), y = by * 4 + ( t >> 2 );
				if ( x < w && y < h ) {
					if ( keepTexels )
						u[size_t( y ) * size_t( w ) + size_t( x )] = v[t];
					bins[size_t( v[t] )]++;
				}
			}
		}
	}

	//! texels passing alpha > ref once the level's alpha is scaled by s
	std::uint64_t passing( int ref, double s ) const
	{
		std::uint64_t n = 0;
		for ( size_t v = 0; v < bins.size(); v++ ) {
			if ( bins[v] && scaledPasses( int( v ), s, ref ) )
				n += bins[v];
		}
		return n;
	}
};

/*! Rewrite one level: alpha x s, then exactly `want` texels pass. The (passing - want) surplus fails in order of the
 *  lowest (scaled alpha, 3 x 3 neighbourhood alpha); a shortfall passes in order of the highest. Returns the texels
 *  moved across the test after the scale. */
inline std::uint64_t rewriteLevel( std::uint8_t * d, const Level & L, double s, int ref, std::uint64_t want )
{
	const size_t n = size_t( L.texels );
	std::vector<std::uint8_t> pass( n );
	std::vector<std::uint32_t> on, off;
	for ( size_t i = 0; i < n; i++ ) {
		pass[i] = scaledPasses( L.u[i], s, ref ) ? 1 : 0;
		( pass[i] ? on : off ).push_back( std::uint32_t( i ) );
	}
	std::uint64_t moved = 0;
	if ( on.size() != want ) {
		// the key: the texel's own alpha first (the scale's order), then the sum of its 3 x 3 neighbourhood (clamped)
		auto key = [&L]( std::uint32_t i ) -> std::uint64_t {
			const int x = int( i % std::uint32_t( L.w ) ), y = int( i / std::uint32_t( L.w ) );
			std::uint64_t sum = 0;
			for ( int dy = -1; dy <= 1; dy++ )
			for ( int dx = -1; dx <= 1; dx++ ) {
				const int xx = std::clamp( x + dx, 0, L.w - 1 ), yy = std::clamp( y + dy, 0, L.h - 1 );
				sum += std::uint64_t( L.u[size_t( yy ) * size_t( L.w ) + size_t( xx )] );
			}
			return ( std::uint64_t( L.u[i] ) << 20 ) | sum;	// sum <= 9 x 8925 < 2^20
		};
		const bool surplus = on.size() > want;	// the surplus: the lowest keys among the passing fail
		const std::vector<std::uint32_t> & from = surplus ? on : off;	// the shortfall: the highest among the failing pass
		const size_t k = surplus ? on.size() - size_t( want ) : size_t( want ) - on.size();
		std::vector<std::pair<std::uint64_t, std::uint32_t>> keyed( from.size() );
		for ( size_t j = 0; j < from.size(); j++ )	// ties of the whole key go by texel order (deterministic)
			keyed[j] = { surplus ? key( from[j] ) : ~key( from[j] ), from[j] };
		std::nth_element( keyed.begin(), keyed.begin() + std::ptrdiff_t( k ), keyed.end() );
		for ( size_t j = 0; j < k; j++ )
			pass[keyed[j].second] = surplus ? 0 : 1;
		moved = k;
	}
	const int bw = ( L.w + 3 ) / 4, bh = ( L.h + 3 ) / 4;
	for ( int by = 0; by < bh; by++ )
	for ( int bx = 0; bx < bw; bx++ ) {
		double x[16];
		bool p[16];
		for ( int t = 0; t < 16; t++ ) {	// texels outside a tiny level repeat its edge texel
			const int xx = std::min( bx * 4 + ( t & 3 ), L.w - 1 ), yy = std::min( by * 4 + ( t >> 2 ), L.h - 1 );
			const size_t i = size_t( yy ) * size_t( L.w ) + size_t( xx );
			x[t] = double( L.u[i] ) * s / 35.0;
			p[t] = pass[i] != 0;
		}
		encodeBC3AlphaSided( x, p, ref, d + ( size_t( by ) * size_t( bw ) + size_t( bx ) ) * 16 );
	}
	return moved;
}

/*! The rule. levels[0] (BC3, w0 x h0) is left alone and sets the target count at `ref` (alpha > ref, ref in 0..254). Each
 *  later level: the scale in [1/16, 16] whose count is nearest the target (bisection on the monotone count; a scale
 *  that leaves no texel passing, or every texel, while the target is neither, is never taken), then the tie-break to
 *  the target exactly. A level within 0.5 % of its target is not rewritten. Returns the number of levels rewritten. */
inline int preserve( std::uint8_t * const * levels, int nLevels, int w0, int h0, int ref, LevelStat * stats = nullptr )
{
	if ( nLevels < 1 || w0 < 1 || h0 < 1 || ref < 0 || ref > 254 )
		return 0;
	Level L;
	L.build( levels[0], w0, h0, false );
	const double cov0 = L.texels ? double( L.passing( ref, 1.0 ) ) / double( L.texels ) : 0.0;
	if ( stats ) {
		stats[0] = LevelStat();
		stats[0].w = w0;
		stats[0].h = h0;
		stats[0].before = stats[0].after = cov0;
	}
	int changed = 0;
	for ( int l = 1; l < nLevels; l++ ) {
		const int w = std::max( w0 >> l, 1 ), h = std::max( h0 >> l, 1 );
		L.build( levels[l], w, h );
		const std::uint64_t N = L.texels;
		const std::uint64_t want = std::uint64_t( std::llround( cov0 * double( N ) ) );
		const std::uint64_t now = L.passing( ref, 1.0 );
		double best = 1.0;
		std::uint64_t moved = 0;
		// a level within 0.5 % of its target keeps its bytes (the re-encode costs a little alpha precision)
		const bool off = double( now > want ? now - want : want - now ) > 0.005 * double( want );
		if ( off ) {
			auto err = [&]( double s ) -> double {
				const std::uint64_t c = L.passing( ref, s );
				if ( ( c == 0 && want > 0 ) || ( c == N && want < N ) )
					return 1e30;	// degenerate: the scale wipes the level (or fills it)
				return std::fabs( double( c ) - double( want ) );
			};
			double bestErr = err( 1.0 );
			double lo = 1.0 / 16.0, hi = 16.0;
			for ( int it = 0; it < 40; it++ ) {
				const double mid = std::sqrt( lo * hi );
				if ( L.passing( ref, mid ) >= want )
					hi = mid;
				else
					lo = mid;
			}
			for ( double s : { lo, hi } ) {
				const double e = err( s );
				if ( e < bestErr ) {
					bestErr = e;
					best = s;
				}
			}
			moved = rewriteLevel( levels[l], L, best, ref, want );
			changed++;
		}
		if ( stats ) {
			stats[l] = LevelStat();
			stats[l].w = w;
			stats[l].h = h;
			stats[l].before = N ? double( now ) / double( N ) : 0.0;
			stats[l].after = N ? double( off ? want : now ) / double( N ) : 0.0;
			stats[l].scale = best;
			stats[l].moved = moved;
		}
	}
	return changed;
}

}	// namespace WwAlphaCov

#endif // WWALPHACOV_H
