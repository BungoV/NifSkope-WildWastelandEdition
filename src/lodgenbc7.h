/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENBC7_H
#define LODGENBC7_H

/*! A DETERMINISTIC BC7 (BPTC UNORM) BLOCK ENCODER, written for the card
 *  bake's `_n` sheet (lane IMPOSTORDEPTH2, 2026-09-23; bungo's ruling: the
 *  depth sheet is stored as BC7).
 *
 *  Why: `_n` carries the card's height in B. Under DXT5 that height rode the
 *  5:6:5 colour block, a 5-bit endpoint, and came back off by 2.81 levels on
 *  average (34 units): 28 of the 59 heights the bake wrote survived. BC7 gives
 *  every channel 7- or 8-bit endpoints and up to 16 steps between them.
 *
 *  What is in the tree and what is new:
 *    * the partition table, the anchor table and the three weight tables are
 *      the VENDORED detex ones (lib/libfo76utils/src/bptc-tables.c, ISC
 *      licence), already compiled into NifSkope for the BC7 DECODER in
 *      decompress-bptc.c. They are C symbols, hence the extern "C" below.
 *    * the encoder itself is new and lives only in this header.
 *
 *  Modes tried per block: 6 (one subset, RGBA, 16 steps), 5 (rotations 0-3),
 *  4 (rotations 0-3, both index selections), 7 (two subsets with alpha, best
 *  partitions by a cheap estimate), and 3 and 1 when the block's alpha is all
 *  255. The block keeps the candidate with the least WEIGHTED squared error,
 *  the weight per channel being the caller's (`wt`, RGBA): the card bake
 *  weights the height channel above the others.
 *
 *  Deterministic: no randomness, no thread-dependent state, fixed iteration
 *  counts, ties go to the earlier candidate. The same exe given the same
 *  pixels writes the same bytes. The caller may encode blocks in parallel.
 */

#include <cstdint>
#include <cstring>
#include <cmath>
#include <climits>


extern "C" {
extern const uint8_t detex_bptc_table_P2[64 * 16];
extern const uint8_t detex_bptc_table_anchor_index_second_subset[64];
extern const uint16_t detex_bptc_table_aWeight2[4];
extern const uint16_t detex_bptc_table_aWeight3[8];
extern const uint16_t detex_bptc_table_aWeight4[16];
}

namespace LodgenBc7
{
namespace detail
{

inline const uint16_t * weightsFor( int ib )
{
	return ib == 2 ? detex_bptc_table_aWeight2 : ib == 3 ? detex_bptc_table_aWeight3 : detex_bptc_table_aWeight4;
}

//! The decoder's endpoint expansion: shift up, replicate the top bits.
inline int expand( int v, int bits )
{
	v <<= ( 8 - bits );
	return v | ( v >> bits );
}

struct Bits
{
	uint8_t * b;
	int pos = 0;
	void put( uint32_t v, int n )
	{
		for ( int i = 0; i < n; i++, pos++ )
			if ( ( v >> i ) & 1U )
				b[pos >> 3] = uint8_t( b[pos >> 3] | ( 1U << ( pos & 7 ) ) );
	}
};

//! One subset's fit: codes, p-bits, the expanded 8-bit endpoints, indices, error.
struct Fit
{
	int q[2][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
	int p[2] = { 0, 0 };
	int ep[2][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
	uint8_t idx[16] = {};
	int64_t err = INT64_MAX;
};

//! What a subset is fitted with: which channels, how many endpoint bits,
//! the p-bit rule (0 none, 1 one per endpoint, 2 one shared by both) and
//! the index width.
struct Spec
{
	int ch[4];
	int nc;
	int bits;
	int pbit;
	int ib;
};

inline void reconstruct( const Spec & s, Fit & f )
{
	for ( int e = 0; e < 2; e++ )
		for ( int j = 0; j < s.nc; j++ ) {
			const int c = s.ch[j];
			f.ep[e][c] = s.pbit ? expand( ( f.q[e][c] << 1 ) | f.p[e], s.bits + 1 ) : expand( f.q[e][c], s.bits );
		}
}

//! Index every pixel to its nearest palette entry; returns the weighted error.
inline int64_t assign( const int ( *px )[4], const int * sel, int n, const Spec & s,
	const int ep[2][4], const int wt[4], uint8_t * idx )
{
	const uint16_t * W = weightsFor( s.ib );
	const int np = 1 << s.ib;
	int pal[16][4];
	for ( int k = 0; k < np; k++ )
		for ( int j = 0; j < s.nc; j++ ) {
			const int c = s.ch[j];
			pal[k][j] = ( ( 64 - W[k] ) * ep[0][c] + W[k] * ep[1][c] + 32 ) >> 6;
		}
	int64_t tot = 0;
	for ( int i = 0; i < n; i++ ) {
		const int * x = px[sel[i]];
		int best = INT_MAX, bk = 0;
		for ( int k = 0; k < np; k++ ) {
			int d = 0;
			for ( int j = 0; j < s.nc; j++ ) {
				const int e = x[s.ch[j]] - pal[k][j];
				d += wt[s.ch[j]] * e * e;
			}
			if ( d < best ) {
				best = d;
				bk = k;
			}
		}
		idx[i] = uint8_t( bk );
		tot += best;
	}
	return tot;
}

//! The code nearest a float endpoint value, for a given p-bit (-1 = none).
inline int quantize( double x, int bits, int p )
{
	const int maxq = ( 1 << bits ) - 1;
	int q0;
	if ( p < 0 )
		q0 = int( std::floor( x * maxq / 255.0 + 0.5 ) );
	else
		q0 = int( std::floor( ( x * ( ( 1 << ( bits + 1 ) ) - 1 ) / 255.0 - p ) * 0.5 + 0.5 ) );
	int best = 0;
	double bd = 1e30;
	for ( int q = q0 - 1; q <= q0 + 1; q++ ) {
		if ( q < 0 || q > maxq )
			continue;
		const int v = p < 0 ? expand( q, bits ) : expand( ( q << 1 ) | p, bits + 1 );
		const double d = std::fabs( v - x );
		if ( d < bd ) {
			bd = d;
			best = q;
		}
	}
	return best;
}

//! Try one pair of float endpoints under every p-bit choice the spec allows.
inline void tryEndpoints( const int ( *px )[4], const int * sel, int n, const Spec & s,
	const double e[2][4], const int wt[4], Fit & best )
{
	const int combos = s.pbit == 1 ? 4 : s.pbit == 2 ? 2 : 1;
	for ( int cb = 0; cb < combos; cb++ ) {
		Fit f;
		if ( s.pbit == 1 ) {
			f.p[0] = cb & 1;
			f.p[1] = ( cb >> 1 ) & 1;
		} else if ( s.pbit == 2 ) {
			f.p[0] = f.p[1] = cb;
		}
		for ( int k = 0; k < 2; k++ )
			for ( int j = 0; j < s.nc; j++ ) {
				const int c = s.ch[j];
				f.q[k][c] = quantize( e[k][c], s.bits, s.pbit ? f.p[k] : -1 );
			}
		reconstruct( s, f );
		f.err = assign( px, sel, n, s, f.ep, wt, f.idx );
		if ( f.err < best.err )
			best = f;
	}
}

//! Fit one subset (the pixels `sel[0..n)` of the block) to the spec.
inline Fit fitSubset( const int ( *px )[4], const int * sel, int n, const Spec & s, const int wt[4] )
{
	double m[4] = { 0, 0, 0, 0 }, sw[4] = { 1, 1, 1, 1 };
	for ( int j = 0; j < s.nc; j++ ) {
		const int c = s.ch[j];
		for ( int i = 0; i < n; i++ )
			m[j] += px[sel[i]][c];
		m[j] /= n;
		sw[j] = std::sqrt( double( wt[c] > 0 ? wt[c] : 1 ) );
	}
	double cov[4][4] = {};
	for ( int i = 0; i < n; i++ ) {
		double y[4];
		for ( int j = 0; j < s.nc; j++ )
			y[j] = ( px[sel[i]][s.ch[j]] - m[j] ) * sw[j];
		for ( int a = 0; a < s.nc; a++ )
			for ( int b = 0; b < s.nc; b++ )
				cov[a][b] += y[a] * y[b];
	}
	// principal axis: power iteration from the column of largest variance
	double v[4] = { 0, 0, 0, 0 };
	int top = 0;
	for ( int j = 1; j < s.nc; j++ )
		if ( cov[j][j] > cov[top][top] )
			top = j;
	for ( int j = 0; j < s.nc; j++ )
		v[j] = cov[j][top];
	double len = 0;
	for ( int it = 0; it < 12; it++ ) {
		double nv[4] = { 0, 0, 0, 0 };
		for ( int a = 0; a < s.nc; a++ )
			for ( int b = 0; b < s.nc; b++ )
				nv[a] += cov[a][b] * v[b];
		len = 0;
		for ( int j = 0; j < s.nc; j++ )
			len += nv[j] * nv[j];
		len = std::sqrt( len );
		if ( len < 1e-12 )
			break;
		for ( int j = 0; j < s.nc; j++ )
			v[j] = nv[j] / len;
	}
	double tmin = 0, tmax = 0;
	if ( len >= 1e-12 ) {
		tmin = 1e30;
		tmax = -1e30;
		for ( int i = 0; i < n; i++ ) {
			double t = 0;
			for ( int j = 0; j < s.nc; j++ )
				t += ( px[sel[i]][s.ch[j]] - m[j] ) * sw[j] * v[j];
			if ( t < tmin )
				tmin = t;
			if ( t > tmax )
				tmax = t;
		}
	} else {
		for ( int j = 0; j < s.nc; j++ )
			v[j] = 0;
	}
	double e[2][4] = {};
	for ( int j = 0; j < s.nc; j++ ) {
		const int c = s.ch[j];
		e[0][c] = std::fmin( 255.0, std::fmax( 0.0, m[j] + tmin * v[j] / sw[j] ) );
		e[1][c] = std::fmin( 255.0, std::fmax( 0.0, m[j] + tmax * v[j] / sw[j] ) );
	}
	Fit best;
	tryEndpoints( px, sel, n, s, e, wt, best );
	// least-squares refinement of the float endpoints against the indices
	const uint16_t * W = weightsFor( s.ib );
	for ( int it = 0; it < 3 && best.err > 0; it++ ) {
		double A = 0, B = 0, C = 0, X0[4] = { 0, 0, 0, 0 }, X1[4] = { 0, 0, 0, 0 };
		for ( int i = 0; i < n; i++ ) {
			const double t = W[best.idx[i]] / 64.0, u = 1.0 - t;
			A += u * u;
			B += u * t;
			C += t * t;
			for ( int j = 0; j < s.nc; j++ ) {
				X0[j] += u * px[sel[i]][s.ch[j]];
				X1[j] += t * px[sel[i]][s.ch[j]];
			}
		}
		const double det = A * C - B * B;
		if ( std::fabs( det ) < 1e-9 )
			break;
		for ( int j = 0; j < s.nc; j++ ) {
			const int c = s.ch[j];
			e[0][c] = std::fmin( 255.0, std::fmax( 0.0, ( C * X0[j] - B * X1[j] ) / det ) );
			e[1][c] = std::fmin( 255.0, std::fmax( 0.0, ( A * X1[j] - B * X0[j] ) / det ) );
		}
		const int64_t before = best.err;
		tryEndpoints( px, sel, n, s, e, wt, best );
		if ( best.err >= before )
			break;
	}
	// local search: one code step at a time, then the p-bits
	const int maxq = ( 1 << s.bits ) - 1;
	for ( int pass = 0; pass < 3 && best.err > 0; pass++ ) {
		bool improved = false;
		for ( int k = 0; k < 2; k++ )
			for ( int j = 0; j < s.nc; j++ )
				for ( int d = -1; d <= 1; d += 2 ) {
					const int c = s.ch[j];
					const int nq = best.q[k][c] + d;
					if ( nq < 0 || nq > maxq )
						continue;
					Fit f = best;
					f.q[k][c] = nq;
					reconstruct( s, f );
					f.err = assign( px, sel, n, s, f.ep, wt, f.idx );
					if ( f.err < best.err ) {
						best = f;
						improved = true;
					}
				}
		if ( s.pbit == 1 )
			for ( int k = 0; k < 2; k++ ) {
				Fit f = best;
				f.p[k] ^= 1;
				reconstruct( s, f );
				f.err = assign( px, sel, n, s, f.ep, wt, f.idx );
				if ( f.err < best.err ) {
					best = f;
					improved = true;
				}
			}
		if ( !improved )
			break;
	}
	return best;
}

//! The anchor pixel of a subset must index below half the palette: swap
//! the endpoints (and their p-bits) and mirror the indices if it does not.
//! The weight tables are symmetric, so the decoded pixels do not change.
inline void fixAnchor( Fit & f, int n, int anchorPos, int ib, const Spec & s )
{
	const int half = 1 << ( ib - 1 ), top = ( 1 << ib ) - 1;
	if ( f.idx[anchorPos] < half )
		return;
	for ( int j = 0; j < s.nc; j++ ) {
		const int c = s.ch[j];
		int t = f.q[0][c]; f.q[0][c] = f.q[1][c]; f.q[1][c] = t;
		t = f.ep[0][c]; f.ep[0][c] = f.ep[1][c]; f.ep[1][c] = t;
	}
	int t = f.p[0]; f.p[0] = f.p[1]; f.p[1] = t;
	for ( int i = 0; i < n; i++ )
		f.idx[i] = uint8_t( top - f.idx[i] );
}

struct Candidate
{
	uint8_t out[16];
	int64_t err = INT64_MAX;
};

inline void keep( Candidate & best, const uint8_t out[16], int64_t err )
{
	if ( err < best.err ) {
		best.err = err;
		std::memcpy( best.out, out, 16 );
	}
}

//! Two-subset partitions fully fitted per mode, best estimate first. Measured on
//! the fixture sheet: 16 or all 64 gave the same height error as 4.
static const int kParts = 4;

static const int kAll[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

//! Mode 6: one subset, RGBA 7 bits + a p-bit per endpoint, 4-bit indices.
inline void mode6( const int ( *px )[4], const int wt[4], Candidate & best )
{
	const Spec s = { { 0, 1, 2, 3 }, 4, 7, 1, 4 };
	Fit f = fitSubset( px, kAll, 16, s, wt );
	fixAnchor( f, 16, 0, 4, s );
	uint8_t out[16] = {};
	Bits w{ out };
	w.put( 1U << 6, 7 );
	for ( int c = 0; c < 4; c++ ) {
		w.put( uint32_t( f.q[0][c] ), 7 );
		w.put( uint32_t( f.q[1][c] ), 7 );
	}
	w.put( uint32_t( f.p[0] ), 1 );
	w.put( uint32_t( f.p[1] ), 1 );
	for ( int i = 0; i < 16; i++ )
		w.put( f.idx[i], i == 0 ? 3 : 4 );
	keep( best, out, f.err );
}

//! Modes 4 and 5: colour and a scalar channel fitted apart; `rot` moves one
//! colour channel into the scalar slot (1 R, 2 G, 3 B).
inline void mode45( int mode, int rot, int isb, const int ( *src )[4], const int wtIn[4], Candidate & best )
{
	int px[16][4];
	int wt[4] = { wtIn[0], wtIn[1], wtIn[2], wtIn[3] };
	for ( int i = 0; i < 16; i++ )
		for ( int c = 0; c < 4; c++ )
			px[i][c] = src[i][c];
	if ( rot ) {
		const int r = rot - 1;
		for ( int i = 0; i < 16; i++ ) {
			const int t = px[i][r]; px[i][r] = px[i][3]; px[i][3] = t;
		}
		const int t = wt[r]; wt[r] = wt[3]; wt[3] = t;
	}
	const int cbits = mode == 4 ? 5 : 7, abits = mode == 4 ? 6 : 8;
	const int cib = mode == 4 ? ( isb ? 3 : 2 ) : 2;
	const int aib = mode == 4 ? ( isb ? 2 : 3 ) : 2;
	const Spec sc = { { 0, 1, 2, 0 }, 3, cbits, 0, cib };
	const Spec sa = { { 3, 0, 0, 0 }, 1, abits, 0, aib };
	Fit fc = fitSubset( px, kAll, 16, sc, wt );
	Fit fa = fitSubset( px, kAll, 16, sa, wt );
	fixAnchor( fc, 16, 0, cib, sc );
	fixAnchor( fa, 16, 0, aib, sa );
	uint8_t out[16] = {};
	Bits w{ out };
	w.put( 1U << mode, mode + 1 );
	w.put( uint32_t( rot ), 2 );
	if ( mode == 4 )
		w.put( uint32_t( isb ), 1 );
	for ( int c = 0; c < 3; c++ ) {
		w.put( uint32_t( fc.q[0][c] ), cbits );
		w.put( uint32_t( fc.q[1][c] ), cbits );
	}
	w.put( uint32_t( fa.q[0][3] ), abits );
	w.put( uint32_t( fa.q[1][3] ), abits );
	// the 2-bit index set is written first; in mode 4 the index-selection
	// bit says whether it is the colour's (0) or the scalar's (1)
	const Fit & first = ( mode == 4 && isb ) ? fa : fc;
	const Fit & second = ( mode == 4 && isb ) ? fc : fa;
	const int ib1 = 2, ib2 = mode == 4 ? 3 : 2;
	for ( int i = 0; i < 16; i++ )
		w.put( first.idx[i], i == 0 ? ib1 - 1 : ib1 );
	for ( int i = 0; i < 16; i++ )
		w.put( second.idx[i], i == 0 ? ib2 - 1 : ib2 );
	keep( best, out, fc.err + fa.err );
}

//! A cheap unquantized estimate of a subset's fit error, for ranking partitions.
inline double quickErr( const int ( *px )[4], const int * sel, int n, int nc, int levels, const int wt[4] )
{
	if ( n == 0 )
		return 0;
	double m[4] = { 0, 0, 0, 0 }, sw[4];
	for ( int c = 0; c < nc; c++ ) {
		for ( int i = 0; i < n; i++ )
			m[c] += px[sel[i]][c];
		m[c] /= n;
		sw[c] = std::sqrt( double( wt[c] > 0 ? wt[c] : 1 ) );
	}
	double cov[4][4] = {};
	double y[16][4];
	for ( int i = 0; i < n; i++ ) {
		for ( int c = 0; c < nc; c++ )
			y[i][c] = ( px[sel[i]][c] - m[c] ) * sw[c];
		for ( int a = 0; a < nc; a++ )
			for ( int b = 0; b < nc; b++ )
				cov[a][b] += y[i][a] * y[i][b];
	}
	int top = 0;
	for ( int c = 1; c < nc; c++ )
		if ( cov[c][c] > cov[top][top] )
			top = c;
	double v[4];
	for ( int c = 0; c < nc; c++ )
		v[c] = cov[c][top];
	double len = 0;
	for ( int it = 0; it < 6; it++ ) {
		double nv[4] = { 0, 0, 0, 0 };
		for ( int a = 0; a < nc; a++ )
			for ( int b = 0; b < nc; b++ )
				nv[a] += cov[a][b] * v[b];
		len = 0;
		for ( int c = 0; c < nc; c++ )
			len += nv[c] * nv[c];
		len = std::sqrt( len );
		if ( len < 1e-12 )
			return 0;
		for ( int c = 0; c < nc; c++ )
			v[c] = nv[c] / len;
	}
	double t[16], tmin = 1e30, tmax = -1e30;
	for ( int i = 0; i < n; i++ ) {
		t[i] = 0;
		for ( int c = 0; c < nc; c++ )
			t[i] += y[i][c] * v[c];
		if ( t[i] < tmin ) tmin = t[i];
		if ( t[i] > tmax ) tmax = t[i];
	}
	const double step = ( tmax - tmin ) / ( levels - 1 );
	double err = 0;
	for ( int i = 0; i < n; i++ ) {
		double r2 = 0;
		for ( int c = 0; c < nc; c++ )
			r2 += y[i][c] * y[i][c];
		r2 -= t[i] * t[i];   // off-axis residual
		double q = 0;
		if ( step > 0 ) {
			const double k = std::floor( ( t[i] - tmin ) / step + 0.5 );
			q = t[i] - ( tmin + k * step );
		}
		err += ( r2 > 0 ? r2 : 0 ) + q * q;
	}
	return err;
}

//! The K best two-subset partitions for `nc` channels, best first.
inline void bestPartitions( const int ( *px )[4], int nc, int levels, const int wt[4], int K, int * out )
{
	double score[64];
	for ( int pt = 0; pt < 64; pt++ ) {
		int sel[2][16], n[2] = { 0, 0 };
		for ( int i = 0; i < 16; i++ ) {
			const int s = detex_bptc_table_P2[pt * 16 + i];
			sel[s][n[s]++] = i;
		}
		score[pt] = quickErr( px, sel[0], n[0], nc, levels, wt ) + quickErr( px, sel[1], n[1], nc, levels, wt );
	}
	bool used[64] = {};
	for ( int k = 0; k < K; k++ ) {
		int b = -1;
		for ( int pt = 0; pt < 64; pt++ )
			if ( !used[pt] && ( b < 0 || score[pt] < score[b] ) )
				b = pt;
		used[b] = true;
		out[k] = b;
	}
}

//! Modes 7 (RGBA, 5 bits + p-bit, 2-bit), 3 (RGB, 7 bits + p-bit, 2-bit) and
//! 1 (RGB, 6 bits + shared p-bit, 3-bit): two subsets over partition `pt`.
inline void mode2sub( int mode, int pt, const int ( *px )[4], const int wt[4], Candidate & best )
{
	const bool rgba = mode == 7;
	const Spec s = { { 0, 1, 2, 3 }, rgba ? 4 : 3, mode == 7 ? 5 : mode == 3 ? 7 : 6,
		mode == 1 ? 2 : 1, mode == 1 ? 3 : 2 };
	int sel[2][16], n[2] = { 0, 0 };
	for ( int i = 0; i < 16; i++ ) {
		const int ss = detex_bptc_table_P2[pt * 16 + i];
		sel[ss][n[ss]++] = i;
	}
	Fit f[2];
	int64_t err = 0;
	const int anchor1 = detex_bptc_table_anchor_index_second_subset[pt];
	for ( int ss = 0; ss < 2; ss++ ) {
		f[ss] = fitSubset( px, sel[ss], n[ss], s, wt );
		err += f[ss].err;
		// the subset's anchor, as a position in its own pixel list
		const int anchorPix = ss == 0 ? 0 : anchor1;
		int pos = 0;
		for ( int i = 0; i < n[ss]; i++ )
			if ( sel[ss][i] == anchorPix )
				pos = i;
		fixAnchor( f[ss], n[ss], pos, s.ib, s );
	}
	if ( err >= best.err )
		return;
	uint8_t out[16] = {};
	Bits w{ out };
	w.put( 1U << mode, mode + 1 );
	w.put( uint32_t( pt ), 6 );
	for ( int c = 0; c < s.nc; c++ )
		for ( int ss = 0; ss < 2; ss++ ) {
			w.put( uint32_t( f[ss].q[0][c] ), s.bits );
			w.put( uint32_t( f[ss].q[1][c] ), s.bits );
		}
	if ( mode == 1 ) {
		w.put( uint32_t( f[0].p[0] ), 1 );
		w.put( uint32_t( f[1].p[0] ), 1 );
	} else {
		for ( int ss = 0; ss < 2; ss++ ) {
			w.put( uint32_t( f[ss].p[0] ), 1 );
			w.put( uint32_t( f[ss].p[1] ), 1 );
		}
	}
	int pos[2] = { 0, 0 };
	for ( int i = 0; i < 16; i++ ) {
		const int ss = detex_bptc_table_P2[pt * 16 + i];
		const bool anchor = ( ss == 0 && i == 0 ) || ( ss == 1 && i == anchor1 );
		w.put( f[ss].idx[pos[ss]++], anchor ? s.ib - 1 : s.ib );
	}
	keep( best, out, err );
}

} // namespace detail

/*! Encode one 4x4 block. `rgba[i]` is pixel i in row-major order (i = y*4+x),
 *  channels R G B A as 0..255. `wt` weighs the squared error per channel
 *  (R G B A, each >= 1). Returns the weighted squared error of the choice. */
inline int64_t encodeBlock( const uint8_t rgba[16][4], const int wt[4], uint8_t out[16] )
{
	using namespace detail;
	int px[16][4];
	bool opaque = true;
	for ( int i = 0; i < 16; i++ ) {
		for ( int c = 0; c < 4; c++ )
			px[i][c] = rgba[i][c];
		opaque = opaque && rgba[i][3] == 255;
	}
	Candidate best;
	mode6( px, wt, best );
	for ( int rot = 0; rot < 4 && best.err > 0; rot++ ) {
		mode45( 5, rot, 0, px, wt, best );
		mode45( 4, rot, 0, px, wt, best );
		mode45( 4, rot, 1, px, wt, best );
	}
	if ( best.err > 0 ) {
		int parts[kParts];
		bestPartitions( px, 4, 4, wt, kParts, parts );
		for ( int k = 0; k < kParts && best.err > 0; k++ )
			mode2sub( 7, parts[k], px, wt, best );
	}
	if ( opaque && best.err > 0 ) {
		int parts[kParts];
		bestPartitions( px, 3, 4, wt, kParts, parts );
		for ( int k = 0; k < kParts && best.err > 0; k++ )
			mode2sub( 3, parts[k], px, wt, best );
		bestPartitions( px, 3, 8, wt, kParts, parts );
		for ( int k = 0; k < kParts && best.err > 0; k++ )
			mode2sub( 1, parts[k], px, wt, best );
	}
	std::memcpy( out, best.out, 16 );
	return best.err;
}

} // namespace LodgenBc7

#endif
