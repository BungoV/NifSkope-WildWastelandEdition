/*! Lane UMBRA1: the previs tome's visibility query (see cellumbra.h). Every float operation below is written in the
 *  order of notes/umbra1/umbra_query.py, one float32 rounding per operation, so the two agree bit for bit. */
#if defined( __clang__ )
#pragma clang fp contract( off )
#elif defined( __GNUC__ )
#pragma GCC optimize( "fp-contract=off" )
#elif defined( _MSC_VER )
#pragma fp_contract( off )
#endif

#include "cellumbra.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <unordered_map>

namespace
{

//! bounds-checked little-endian reads of the tome; an out-of-range read sets `bad` and returns 0
struct Rd
{
	const unsigned char * p = nullptr;
	std::uint64_t n = 0;
	mutable bool bad = false;

	explicit Rd( const QByteArray & d ) : p( reinterpret_cast<const unsigned char *>( d.constData() ) ), n( std::uint64_t( d.size() ) ) {}
	bool has( std::uint64_t o, std::uint64_t len ) const { return o <= n && len <= n - o; }
	std::uint32_t u32( std::uint64_t o ) const
	{
		if ( !has( o, 4 ) ) {
			bad = true;
			return 0;
		}
		std::uint32_t v;
		std::memcpy( &v, p + o, 4 );
		return v;
	}
	std::uint16_t u16( std::uint64_t o ) const
	{
		if ( !has( o, 2 ) ) {
			bad = true;
			return 0;
		}
		std::uint16_t v;
		std::memcpy( &v, p + o, 2 );
		return v;
	}
	float f32( std::uint64_t o ) const
	{
		const std::uint32_t w = u32( o );
		float f;
		std::memcpy( &f, &w, 4 );
		return f;
	}
};

std::uint32_t fbits( float f )
{
	std::uint32_t w;
	std::memcpy( &w, &f, 4 );
	return w;
}

float bitsf( std::uint32_t w )
{
	float f;
	std::memcpy( &f, &w, 4 );
	return f;
}

const float K65535 = bitsf( 0x37800080U );	// 1/65535 as the game's float
const float EPS = bitsf( 0x34000000U );		// 1.1920929e-07
const float BIAS_HI = bitsf( 0x4203FFFFU );	// 32.9999962

// ------------------------------------------------------------------------------------------------ the KD trees

std::uint32_t kdLutSize( std::uint32_t n )
{
	return 4 * ( ( n >> 16 ) + ( ( ( n >> 8 ) - ( n >> 16 ) + 1 ) >> 1 ) + ( ( ( n >> 4 ) - ( n >> 8 ) + 3 ) >> 2 ) );
}

std::uint32_t kdDataDwords( std::uint32_t n )
{
	return ( ( 2 * n + 31 ) >> 5 ) + ( kdLutSize( n ) >> 2 );
}

bool kdInit( WwUmbraKd & k, const Rd & rd, std::uint32_t n, std::uint64_t words, std::uint64_t splits,
	std::uint32_t numSplits )
{
	k = WwUmbraKd();
	if ( !n || n > ( 1U << 27 ) )
		return false;
	k.n = n;
	k.nwords = ( 2 * n + 31 ) >> 5;
	const std::uint32_t dd = kdDataDwords( n );
	k.lutWords = dd - k.nwords;
	if ( !rd.has( words, 4ULL * dd ) || ( numSplits && !rd.has( splits, 4ULL * numSplits ) ) )
		return false;
	k.words = std::uint32_t( words );
	k.splits = std::uint32_t( splits );
	k.numSplits = numSplits;
	k.midOff = n >> 16;
	k.botOff = ( n >> 16 ) + ( ( ( n >> 8 ) - ( n >> 16 ) + 1 ) >> 1 );
	k.present = true;
	return true;
}

std::uint32_t kdWord( const WwUmbraKd & k, const Rd & rd, std::uint64_t i )
{
	if ( i >= std::uint64_t( k.nwords ) + k.lutWords ) {
		rd.bad = true;
		return 0;
	}
	return rd.u32( k.words + 4 * i );
}

std::uint32_t kdLut( const WwUmbraKd & k, const Rd & rd, std::int64_t i )
{
	if ( i < 0 || i >= std::int64_t( k.lutWords ) ) {
		rd.bad = true;
		return 0;
	}
	return rd.u32( k.words + 4 * ( std::uint64_t( k.nwords ) + std::uint64_t( i ) ) );
}

int kdSplit( const WwUmbraKd & k, const Rd & rd, std::uint32_t i )
{
	return int( ( kdWord( k, rd, ( 2ULL * i ) >> 5 ) >> ( ( i & 15 ) * 2 ) ) & 3 );
}

std::int64_t kdLookup( const WwUmbraKd & k, const Rd & rd, std::uint32_t j )
{
	std::int64_t r = 0;
	if ( j & 0xffff0000U )
		r = kdLut( k, rd, std::int64_t( j >> 16 ) - 1 );
	if ( j & 0xff00U ) {
		const std::int64_t m = std::int64_t( j >> 8 ) - std::int64_t( j >> 16 ) - 1;
		const std::uint32_t w = kdLut( k, rd, std::int64_t( k.midOff ) + ( m >> 1 ) );
		r += ( w >> ( 16 * ( m & 1 ) ) ) & 0xffff;
	}
	if ( j & 0xf0U ) {
		const std::int64_t b = std::int64_t( j >> 4 ) - std::int64_t( j >> 8 ) - 1;
		const std::uint32_t w = kdLut( k, rd, std::int64_t( k.botOff ) + ( b >> 2 ) );
		r += ( w >> ( 8 * ( b & 3 ) ) ) & 0xff;
	}
	return r;
}

std::int64_t kdRank( const WwUmbraKd & k, const Rd & rd, std::uint32_t i )
{
	const std::uint32_t j = i + 1;
	const std::uint64_t idx = ( 2ULL * j ) >> 5;
	std::uint32_t w = 0;
	if ( idx < k.nwords )
		w = kdWord( k, rd, idx ) & std::uint32_t( ( 1ULL << ( 2 * ( j & 15 ) ) ) - 1 );
	std::uint32_t x = w & ( w >> 1 ) & 0x55555555U;
	int leaves = 0;
	for ( ; x; x &= x - 1 )
		leaves++;
	return kdLookup( k, rd, j ) + std::int64_t( j & 15 ) - leaves;
}

float kdSplitValue( const WwUmbraKd & k, const Rd & rd, std::uint32_t i, const float * mn, const float * mx, int a )
{
	if ( i < k.numSplits )
		return rd.f32( k.splits + 4ULL * i );
	return ( mn[a] + mx[a] ) * 0.5f;
}

//! every LEAF node whose closed box holds p (umbra_tiles KDTree.nodes_containing, leaves only)
template <class F>
void kdLeavesContaining( const WwUmbraKd & k, const Rd & rd, const float * mn0, const float * mx0, const float * p, F leaf )
{
	struct E
	{
		std::uint32_t i;
		float mn[3], mx[3];
	};
	std::vector<E> st;
	E e0;
	e0.i = 0;
	for ( int a = 0; a < 3; a++ ) {
		e0.mn[a] = mn0[a];
		e0.mx[a] = mx0[a];
	}
	st.push_back( e0 );
	std::uint64_t guard = 4ULL * k.n + 64;
	while ( !st.empty() && !rd.bad ) {
		if ( !guard-- ) {
			rd.bad = true;
			return;
		}
		const E e = st.back();
		st.pop_back();
		bool in = true;
		for ( int a = 0; a < 3; a++ )
			in = in && p[a] >= e.mn[a] && p[a] <= e.mx[a];
		if ( !in )
			continue;
		const int sp = kdSplit( k, rd, e.i );
		if ( sp == 3 ) {
			leaf( e.i );
			continue;
		}
		const float s = kdSplitValue( k, rd, e.i, e.mn, e.mx, sp );
		const std::int64_t r = 2 * kdRank( k, rd, e.i );
		if ( r - 1 <= std::int64_t( e.i ) || r >= std::int64_t( k.n ) ) {
			rd.bad = true;
			return;
		}
		E l = e, rr = e;
		l.i = std::uint32_t( r - 1 );
		l.mx[sp] = s;
		rr.i = std::uint32_t( r );
		rr.mn[sp] = s;
		st.push_back( rr );
		st.push_back( l );
	}
}

// ------------------------------------------------------------------------------------------------ tiles: leaf -> cell

std::int64_t nodeData( const WwUmbraTile & t, const Rd & rd, std::uint64_t leaf )
{
	if ( !t.nodeData )
		return -1;
	const std::uint32_t w = t.nodeBits;
	if ( !w ) {
		rd.bad = true;
		return -1;
	}
	const std::uint64_t bit = std::uint64_t( w ) * leaf;
	const std::uint64_t base = std::uint64_t( t.off ) + t.nodeData + 4 * ( bit >> 5 );
	const std::uint64_t lo = rd.u32( base );
	const std::uint64_t hi = base + 8 <= rd.n ? rd.u32( base + 4 ) : 0;
	const std::uint64_t mask = ( 1ULL << w ) - 1;
	const std::uint64_t v = ( ( lo | ( hi << 32 ) ) >> ( bit & 31 ) ) & mask;
	if ( ( v >> ( w - 1 ) ) & 1 ) {
		if ( v == mask )
			return -1;
		return std::int64_t( ( v & ( mask >> 1 ) ) | 0x80000000ULL );
	}
	return std::int64_t( v );
}

std::int64_t cellIndex( const WwUmbraTile & t, const Rd & rd, std::uint64_t leaf, const float * p )
{
	const std::int64_t v = nodeData( t, rd, leaf );
	if ( v >= 0 && !( v & 0x80000000LL ) )
		return v;
	if ( v == -1 )
		return -1;
	std::uint64_t idx = std::uint64_t( v & 0x7fffffffLL );
	for ( std::uint64_t it = 0; it < std::uint64_t( t.numBsp ) + 1 && !rd.bad; it++ ) {
		const std::uint64_t at = std::uint64_t( t.off ) + t.bsp + 8 * idx;
		const std::uint32_t lo = rd.u32( at ), hi = rd.u32( at + 4 );
		const std::uint64_t pa = std::uint64_t( t.off ) + t.planes + 16ULL * ( lo & 0x3fffffffU );
		const float pl[4] = { rd.f32( pa ), rd.f32( pa + 4 ), rd.f32( pa + 8 ), rd.f32( pa + 12 ) };
		const float t0 = p[0] * pl[0];
		const float t1 = p[1] * pl[1];
		const float t2 = p[2] * pl[2];
		const float dot = ( ( t0 + t1 ) + t2 ) + pl[3];
		std::uint32_t nxt, isLeaf;
		if ( dot < 0.0f ) {
			nxt = hi & 0xffffU;
			isLeaf = ( lo >> 30 ) & 1;
		} else {
			nxt = hi >> 16;
			isLeaf = ( lo >> 31 ) & 1;
		}
		if ( isLeaf )
			return nxt == 0xffffU ? -1 : std::int64_t( nxt );
		idx = nxt;
	}
	rd.bad = true;	// the Python reader raises 'BSP loop'
	return -1;
}

std::vector<std::pair<int, int>> startCells( const WwUmbraTome & T, const Rd & rd, const float * p )
{
	std::vector<std::pair<int, int>> out;
	kdLeavesContaining( T.tree, rd, T.mn, T.mx, p, [&]( std::uint32_t i ) {
		if ( i >= T.tiles.size() )
			return;
		const WwUmbraTile & t = T.tiles[i];
		if ( !t.present || !t.tree.present || !t.cellNodes )
			return;
		kdLeavesContaining( t.tree, rd, t.mn, t.mx, p, [&]( std::uint32_t j ) {
			const std::int64_t leaf = std::int64_t( j ) - kdRank( t.tree, rd, j );
			if ( leaf < 0 )
				return;
			const std::int64_t c = cellIndex( t, rd, std::uint64_t( leaf ), p );
			if ( c >= 0 && c < std::int64_t( t.cells.size() ) )
				out.emplace_back( int( i ), int( c ) );
		} );
	} );
	std::sort( out.begin(), out.end() );
	out.erase( std::unique( out.begin(), out.end() ), out.end() );
	return out;
}

std::string hex8( float f )
{
	char b[16];
	std::snprintf( b, sizeof( b ), "%08X", unsigned( fbits( f ) ) );
	return b;
}

std::uint64_t listBits( const Rd & rd, std::uint64_t base, std::uint64_t pos, std::uint32_t width )
{
	const std::uint64_t wi = pos >> 5;
	const std::uint64_t lo = rd.u32( base + 4 * wi );
	const std::uint64_t hi = base + 4 * wi + 8 <= rd.n ? rd.u32( base + 4 * wi + 4 ) : 0;
	return ( ( lo | ( hi << 32 ) ) >> ( pos & 31 ) ) & ( ( 1ULL << width ) - 1 );
}

// ------------------------------------------------------------------------------------------------ the camera

struct Cam
{
	float Mt[4][4];		//!< Mt[k] = the clip image of world axis k (k = 3: the translation)
	float pos[3];
	bool mirror = false;
	float planes[6][4];
	float rec[6][3][4];	//!< per face: P0, P1, P2, four lanes
};

double det4( const double m[4][4] )
{
	double a[4][4];
	std::memcpy( a, m, sizeof( a ) );
	double d = 1.0;
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( a[r][c] ) > std::fabs( a[p][c] ) )
				p = r;
		if ( a[p][c] == 0.0 )
			return 0.0;
		if ( p != c ) {
			for ( int k = 0; k < 4; k++ )
				std::swap( a[p][k], a[c][k] );
			d = -d;
		}
		d *= a[c][c];
		for ( int r = c + 1; r < 4; r++ ) {
			const double f = a[r][c] / a[c][c];
			for ( int k = c; k < 4; k++ )
				a[r][k] -= f * a[c][k];
		}
	}
	return d;
}

void edgeRecords( Cam & c )
{
	const float A[3] = { c.Mt[0][0], c.Mt[0][1], c.Mt[0][3] };
	const float B[3] = { c.Mt[1][0], c.Mt[1][1], c.Mt[1][3] };
	const float C[3] = { c.Mt[2][0], c.Mt[2][1], c.Mt[2][3] };
	auto cr = []( const float * U, const float * V, float * o ) {
		float x, y;
		x = U[2] * V[1];
		y = U[1] * V[2];
		o[0] = x - y;
		x = U[0] * V[2];
		y = U[2] * V[0];
		o[1] = x - y;
		x = U[1] * V[0];
		y = U[0] * V[1];
		o[2] = x - y;
	};
	float K[3][3];
	cr( C, B, K[0] );
	cr( C, A, K[1] );
	cr( B, A, K[2] );
	for ( int i = 0; i < 3; i++ ) {
		const float * p = K[( i + 1 ) % 3];
		const float * q = K[( i + 2 ) % 3];
		int fd = 2 * i + 1, fc = 2 * i;
		if ( c.mirror )
			std::swap( fd, fc );
		for ( int k = 0; k < 3; k++ ) {
			c.rec[fd][k][0] = p[k];
			c.rec[fd][k][1] = -q[k];
			c.rec[fd][k][2] = -p[k];
			c.rec[fd][k][3] = q[k];
			c.rec[fc][k][0] = -q[k];
			c.rec[fc][k][1] = p[k];
			c.rec[fc][k][2] = q[k];
			c.rec[fc][k][3] = -p[k];
		}
	}
}

Cam makeCam( const WwUmbraCamera & in )
{
	Cam c;
	double md[4][4];
	for ( int r = 0; r < 4; r++ )
		for ( int k = 0; k < 4; k++ ) {
			c.Mt[k][r] = in.M[r * 4 + k];
			md[r][k] = double( in.M[r * 4 + k] );
		}
	for ( int a = 0; a < 3; a++ )
		c.pos[a] = in.pos[a];
	c.mirror = det4( md ) < 0.0;
	const float * r0 = in.M;
	const float * r1 = in.M + 4;
	const float * r2 = in.M + 8;
	const float * r3 = in.M + 12;
	const float n11 = r2[1] * r2[1];
	const float n00 = r2[0] * r2[0];
	const float n22 = r2[2] * r2[2];
	const float ln = std::sqrt( ( n11 + n00 ) + n22 );
	const float inv = 1.0f / ln;
	for ( int k = 0; k < 4; k++ ) {
		c.planes[0][k] = r2[k] * inv;
		c.planes[1][k] = r3[k] - r2[k];
		c.planes[2][k] = r3[k] - r0[k];
		c.planes[3][k] = r3[k] + r0[k];
		c.planes[4][k] = r3[k] - r1[k];
		c.planes[5][k] = r3[k] + r1[k];
	}
	edgeRecords( c );
	return c;
}

// ------------------------------------------------------------------------------------------------ projection

struct Quad
{
	float X[4], Y[4], Z[4], W[4], N[4];
};

Quad corners( const Cam & c, const float * mn, const float * mx, int face )
{
	Quad q;
	float d[3], o[4], ex[4], ey[4], ez[4];
	for ( int a = 0; a < 3; a++ )
		d[a] = mx[a] - mn[a];
	for ( int j = 0; j < 4; j++ ) {
		const float t2 = mn[2] * c.Mt[2][j];
		const float s0 = t2 + c.Mt[3][j];
		const float t1 = mn[1] * c.Mt[1][j];
		const float s1 = s0 + t1;
		const float t0 = mn[0] * c.Mt[0][j];
		o[j] = s1 + t0;
		ex[j] = d[0] * c.Mt[0][j];
		ey[j] = d[1] * c.Mt[1][j];
		ez[j] = d[2] * c.Mt[2][j];
	}
	const float *N, *Bv, *Av;
	switch ( face >> 1 ) {
	case 0: N = ex; Bv = ey; Av = ez; break;
	case 1: N = ey; Bv = ez; Av = ex; break;
	default: N = ez; Bv = ex; Av = ey; break;
	}
	if ( ( face & 1 ) != int( c.mirror ) )
		std::swap( Av, Bv );
	for ( int j = 0; j < 4; j++ ) {
		const float c0 = o[j];
		const float c1 = Bv[j] + o[j];
		const float c2 = c1 + Av[j];
		const float c3 = Av[j] + o[j];
		float * row = j == 0 ? q.X : j == 1 ? q.Y : j == 2 ? q.Z : q.W;
		row[0] = c0;
		row[1] = c1;
		row[2] = c2;
		row[3] = c3;
		q.N[j] = N[j];
	}
	return q;
}

//! the accept test and the ndc box (x0, y0, x1, y1); false = rejected
bool ndcBox( const Quad & q, float * nd )
{
	float Xb[4], Yb[4], Zb[4], Wb[4];
	bool ok = false;
	for ( int l = 0; l < 4; l++ ) {
		Xb[l] = q.X[l] + q.N[0];
		Yb[l] = q.Y[l] + q.N[1];
		Zb[l] = q.Z[l] + q.N[2];
		Wb[l] = q.W[l] + q.N[3];
		const float mw = q.W[l] > Wb[l] ? q.W[l] : Wb[l];
		ok = ok || ( ( q.Z[l] <= q.W[l] || Zb[l] <= Wb[l] ) && 0.0f <= mw );
	}
	if ( !ok )
		return false;
	const float * xs[2] = { q.X, Xb };
	const float * ys[2] = { q.Y, Yb };
	const float * ws[2] = { q.W, Wb };
	float nx[2][4], ny[2][4];
	float minW = q.W[0];
	for ( int s = 0; s < 2; s++ )
		for ( int l = 0; l < 4; l++ ) {
			const float w = ws[s][l];
			const float w1 = w == 0.0f ? 1.0f : w;
			const float r = 1.0f / w1;
			nx[s][l] = r * xs[s][l];
			ny[s][l] = r * ys[s][l];
			minW = std::min( minW, w );
		}
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool first = true;
	for ( int s = 0; s < 2; s++ )
		for ( int l = 0; l < 4; l++ ) {
			float vx0, vy0, vx1, vy1;
			if ( minW < 0.0f ) {
				const float w = ws[s][l];
				const float vals[2] = { xs[s][l], ys[s][l] };
				const float nds[2] = { nx[s][l], ny[s][l] };
				float mxv[2], mnv[2];
				for ( int k = 0; k < 2; k++ ) {
					const float v = vals[k];
					const bool inn = ( -w < v ) && ( v < w );
					mxv[k] = v < w ? ( inn ? nds[k] : -1.0f ) : 1.0f;
					mnv[k] = -w < v ? ( inn ? nds[k] : 1.0f ) : -1.0f;
				}
				vx0 = mnv[0];
				vy0 = mnv[1];
				vx1 = mxv[0];
				vy1 = mxv[1];
			} else {
				vx0 = vx1 = nx[s][l];
				vy0 = vy1 = ny[s][l];
			}
			if ( first ) {
				x0 = vx0;
				y0 = vy0;
				x1 = vx1;
				y1 = vy1;
				first = false;
			} else {
				x0 = std::min( x0, vx0 );
				y0 = std::min( y0, vy0 );
				x1 = std::max( x1, vx1 );
				y1 = std::max( y1, vy1 );
			}
		}
	nd[0] = x0;
	nd[1] = y0;
	nd[2] = x1;
	nd[3] = y1;
	return true;
}

std::int64_t trunc32( float t )
{
	if ( std::isfinite( t ) && std::fabs( double( t ) ) < 2147483648.0 )
		return std::int64_t( std::trunc( t ) );
	return std::int64_t( INT_MIN );
}

void irect( const float * nd, std::int64_t * ir )
{
	const float b[4] = { 32.0f, 32.0f, BIAS_HI, BIAS_HI };
	std::int64_t r[4];
	for ( int k = 0; k < 4; k++ ) {
		const float m = nd[k] * 32.0f;
		r[k] = trunc32( m + b[k] );
	}
	ir[0] = std::max<std::int64_t>( r[0], 0 );
	ir[1] = std::max<std::int64_t>( r[1], 0 );
	ir[2] = std::min<std::int64_t>( r[2], 64 );
	ir[3] = std::min<std::int64_t>( r[3], 64 );
}

// ------------------------------------------------------------------------------------------------ raster (path S)

void coeffs( const Quad & q, const float rec[3][4], float * a2, float * b2, float * c2 )
{
	float nx[4], ny[4], nw[4];
	for ( int l = 0; l < 4; l++ ) {
		const float p2 = rec[2][l] * q.W[l];
		const float p1 = rec[1][l] * q.Y[l];
		const float p0 = rec[0][l] * q.X[l];
		const float s = ( p2 + p1 ) + p0;
		const bool m = 0.0f < s;
		nx[l] = m ? q.N[0] : 0.0f;
		ny[l] = m ? q.N[1] : 0.0f;
		nw[l] = m ? q.N[3] : 0.0f;
	}
	for ( int l = 0; l < 4; l++ ) {
		const int l1 = ( l + 1 ) & 3;
		const float Xp = q.X[l] + nx[l], Yp = q.Y[l] + ny[l], Wp = q.W[l] + nw[l];
		const float X1 = q.X[l1] + nx[l], Y1 = q.Y[l1] + ny[l], W1 = q.W[l1] + nw[l];
		float u, v;
		u = W1 * Yp;
		v = Y1 * Wp;
		const float a = u - v;
		u = X1 * Wp;
		v = W1 * Xp;
		const float b = u - v;
		u = Y1 * Xp;
		v = X1 * Yp;
		const float c = u - v;
		a2[l] = a * 0.03125f;
		b2[l] = b * 0.03125f;
		const float ta = a2[l] * -31.5f;
		const float tb = b2[l] * -31.5f;
		const float c1 = ( c + ta ) + tb;
		const float ab = std::fabs( b2[l] ) + std::fabs( a2[l] );
		const float mx = ab > EPS ? ab : EPS;
		const float th = mx * -0.5f;
		c2[l] = c1 + th;
	}
}

void raster( const std::int64_t * ir, const float * a2, const float * b2, const float * c2, WwUmbraRaster & img )
{
	std::memset( img.row, 0, sizeof( img.row ) );
	const int rx0 = int( ir[0] >> 2 ), ry0 = int( ir[1] >> 2 ), rx1 = int( ( ir[2] + 3 ) >> 2 ), ry1 = int( ( ir[3] + 3 ) >> 2 );
	const int w = rx1 - rx0, h = ry1 - ry0;
	float a4[4], b4[4], Rs[4];
	for ( int l = 0; l < 4; l++ ) {
		a4[l] = a2[l] * 4.0f;
		b4[l] = b2[l] * 4.0f;
		const float t = float( rx0 ) * a4[l];
		const float u = t + c2[l];
		const float v = float( ry0 ) * b4[l];
		Rs[l] = u + v;
	}
	for ( int j = 0; j < h; j++ ) {
		if ( j )
			for ( int l = 0; l < 4; l++ )
				Rs[l] = Rs[l] + b4[l];
		float Eb[4] = { Rs[0], Rs[1], Rs[2], Rs[3] };
		for ( int i = 0; i < w; i++ ) {
			if ( i )
				for ( int l = 0; l < 4; l++ )
					Eb[l] = Eb[l] + a4[l];
			float row[4][4];	// [lane][k]
			for ( int l = 0; l < 4; l++ )
				for ( int k = 0; k < 4; k++ ) {
					const float t = a2[l] * float( k );
					row[l][k] = Eb[l] + t;
				}
			for ( int r = 0; r < 4; r++ ) {
				if ( r )
					for ( int l = 0; l < 4; l++ )
						for ( int k = 0; k < 4; k++ )
							row[l][k] = row[l][k] + b2[l];
				const int y = ( ry0 + j ) * 4 + r;
				if ( y < 0 || y >= 64 )
					continue;
				for ( int k = 0; k < 4; k++ ) {
					const int x = ( rx0 + i ) * 4 + k;
					if ( x < 0 || x >= 64 )
						continue;
					if ( std::signbit( row[0][k] ) && std::signbit( row[1][k] ) && std::signbit( row[2][k] )
						&& std::signbit( row[3][k] ) )
						img.row[y] |= 1ULL << x;
				}
			}
		}
	}
}

bool anyBits( const WwUmbraRaster & r )
{
	for ( int y = 0; y < 64; y++ )
		if ( r.row[y] )
			return true;
	return false;
}

bool testRectAny( const WwUmbraRaster & b, const std::int64_t * ir )
{
	if ( ir[0] >= ir[2] || ir[1] >= ir[3] )
		return false;
	const int n = int( ir[2] - ir[0] );
	const std::uint64_t mask = ( n >= 64 ? ~0ULL : ( ( 1ULL << n ) - 1 ) ) << ir[0];
	for ( std::int64_t y = ir[1]; y < ir[3]; y++ )
		if ( b.row[y] & mask )
			return true;
	return false;
}

bool objectVisible( const WwUmbraTome & T, const Cam & cam, std::uint32_t o, const WwUmbraRaster & b )
{
	if ( !T.dist.empty() ) {
		const float * P = &T.dist[8 * size_t( o )];
		const float * Q = P + 4;
		float d[3];
		for ( int a = 0; a < 3; a++ ) {
			const float mx = P[a] > cam.pos[a] ? P[a] : cam.pos[a];
			const float cc = Q[a] < mx ? Q[a] : mx;
			d[a] = cc - cam.pos[a];
		}
		const float d11 = d[1] * d[1];
		const float d00 = d[0] * d[0];
		const float d22 = d[2] * d[2];
		const float s = ( ( d11 + d00 ) + d22 ) + 0.0f;
		const float d2 = s * 1.0f;
		if ( Q[3] <= d2 || d2 < P[3] )
			return false;
	}
	const float * mn = &T.bounds[6 * size_t( o )];
	const float * mx = mn + 3;
	const float a[4] = { mn[0], mn[1], mn[2], 1.0f };
	const float bb[4] = { mx[0], mx[1], mx[2], 1.0f };
	for ( int i = 0; i < 6; i++ ) {
		const float * pl = cam.planes[i];
		float t[4];
		for ( int k = 0; k < 4; k++ ) {
			const float p = 0.0f < pl[k] ? bb[k] : a[k];
			t[k] = p * pl[k];
		}
		if ( ( ( t[1] + t[0] ) + t[2] ) + t[3] < 0.0f )
			return false;
	}
	const Quad q = corners( cam, mn, mx, 4 | int( cam.mirror ) );	// axis 2, no swap (the object face: not decoded)
	float nd[4];
	if ( !ndcBox( q, nd ) )
		return false;
	for ( int k = 0; k < 4; k++ )
		nd[k] = std::min( std::max( nd[k], -1.0f ), 1.0f );
	std::int64_t ir[4];
	irect( nd, ir );
	return testRectAny( b, ir );
}

} // namespace

// ------------------------------------------------------------------------------------------------ the decode

bool WwUmbraTome::load( const QByteArray & bytes )
{
	*this = WwUmbraTome();
	d = bytes;
	const Rd rd( d );
	auto fail = [this]( const char * why ) {
		ok = false;
		error = QString::fromLatin1( why );
		tiles.clear();
		return false;
	};
	if ( rd.n < 0xa4 )
		return fail( "shorter than a tome header" );
	if ( ( rd.u32( 0 ) & 0xFFFF0000U ) != 0xD6000000U || rd.u32( 8 ) != rd.n )
		return fail( "not a 3.x tome (magic or size field)" );
	for ( int a = 0; a < 3; a++ ) {
		mn[a] = rd.f32( 0x14 + 4 * a );
		mx[a] = rd.f32( 0x20 + 4 * a );
	}
	const std::uint32_t treeN = rd.u32( 0x2c ) >> 5;
	numObjects = rd.u32( 0x40 );
	numClusters = rd.u32( 0x7c );
	numTiles = rd.u32( 0x90 );
	numLeaf = rd.u32( 0x8c );
	numGates = rd.u32( 0x68 );
	gateVert = rd.u32( 0x70 );
	gateIdx = rd.u32( 0x78 );
	const std::uint32_t tofs = rd.u32( 0xa0 ), cs = rd.u32( 0x88 );
	if ( !rd.has( tofs, 4ULL * numTiles ) || !rd.has( cs, 4ULL * ( std::uint64_t( numTiles ) + 1 ) ) )
		return fail( "tile table or cell starts out of range" );
	numCells = rd.u32( cs + 4ULL * numTiles );
	if ( rd.u32( 0x6c ) && !rd.has( rd.u32( 0x6c ), 4ULL * numGates ) )
		return fail( "gate id map out of range" );
	// objects
	const std::uint32_t bo = rd.u32( 0x44 ), dof = rd.u32( 0x48 );
	if ( numObjects ) {
		if ( !bo || !rd.has( bo, 24ULL * numObjects ) )
			return fail( "object boxes absent or out of range" );
		if ( dof && !rd.has( dof, 32ULL * numObjects ) )
			return fail( "object distances out of range" );
	}
	bounds.resize( 6 * size_t( numObjects ) );
	for ( size_t i = 0; i < bounds.size(); i++ )
		bounds[i] = rd.f32( bo + 4ULL * i );
	if ( dof ) {
		dist.resize( 8 * size_t( numObjects ) );
		for ( size_t i = 0; i < dist.size(); i++ )
			dist[i] = rd.f32( dof + 4ULL * i );
	}
	const std::uint32_t uidStarts = rd.u32( 0x4c ), uidOff = rd.u32( 0x50 );
	uids.resize( numObjects );
	for ( std::uint32_t i = 0; i < numObjects; i++ ) {
		if ( uidStarts ) {
			const std::uint32_t s0 = rd.u32( uidStarts + 4ULL * i );
			uids[i] = rd.u32( uidOff + 4ULL * s0 );
		} else {
			uids[i] = rd.u32( uidOff + 4ULL * i );
		}
	}
	if ( rd.bad )
		return fail( "object ids out of range" );
	const std::uint32_t lw = rd.u32( 0x54 ), olOff = rd.u32( 0x58 ), olN = rd.u32( 0x5c );
	const std::uint32_t ew = lw & 31, cw = ( lw >> 5 ) & 31;
	// tiles
	tiles.resize( numTiles );
	std::uint64_t listed = 0;
	for ( std::uint32_t i = 0; i < numTiles; i++ ) {
		WwUmbraTile & t = tiles[i];
		const std::uint32_t o = rd.u32( tofs + 4ULL * i );
		if ( !o )
			continue;
		if ( !rd.has( o, 0x50 ) )
			return fail( "tile out of range" );
		t.present = true;
		t.off = o;
		for ( int a = 0; a < 3; a++ ) {
			t.mn[a] = rd.f32( o + 4ULL * a );
			t.mx[a] = rd.f32( o + 12ULL + 4ULL * a );
		}
		t.leaf = ( rd.u32( o + 0x2cULL ) & 1 ) != 0;
		t.pe = rd.f32( o + 0x30ULL );
		const std::uint32_t ncells = rd.u16( o + 0x34ULL );
		t.numCells = ncells;
		const std::uint32_t cn = rd.u32( o + 0x38ULL ), po = rd.u32( o + 0x3cULL );
		const std::uint32_t treeWord = rd.u32( o + 0x18ULL );
		t.nodeBits = treeWord & 31;
		t.nodeData = rd.u32( o + 0x20ULL );
		const std::uint32_t treeData = rd.u32( o + 0x1cULL ), numSplits = rd.u32( o + 0x24ULL ), splits = rd.u32( o + 0x28ULL );
		t.cellNodes = cn;
		t.bsp = rd.u32( o + 0x40ULL );
		t.numBsp = rd.u32( o + 0x44ULL );
		t.planes = rd.u32( o + 0x48ULL );
		if ( ( treeWord >> 5 ) && treeData
			&& !kdInit( t.tree, rd, treeWord >> 5, std::uint64_t( o ) + treeData, std::uint64_t( o ) + splits, numSplits ) )
			return fail( "tile KD tree out of range" );
		std::uint64_t npt = 0;
		if ( cn ) {
			if ( !rd.has( std::uint64_t( o ) + cn, 36ULL * ncells ) )
				return fail( "cell nodes out of range" );
			t.cells.resize( ncells );
			for ( std::uint32_t c = 0; c < ncells; c++ ) {
				const std::uint64_t at = std::uint64_t( o ) + cn + 36ULL * c;
				WwUmbraCell & cell = t.cells[c];
				cell.portal = rd.u32( at );
				cell.portalCount = rd.u32( at + 4 );
				const std::uint32_t oi = rd.u32( at + 8 ), oc = rd.u32( at + 12 );
				npt = std::max<std::uint64_t>( npt, std::uint64_t( cell.portal ) + cell.portalCount );
				// the run-length object list (umbra_query.Tome.cell_objects)
				const std::uint64_t W = ew + cw;
				std::uint64_t k = oi;
				while ( cell.objects.size() < oc && k < olN ) {
					const std::uint32_t e = std::uint32_t( listBits( rd, olOff, W * k, ew ) );
					const std::uint32_t cnt = std::uint32_t( listBits( rd, olOff, W * k + ew, cw ) );
					k++;
					if ( !cnt )
						break;
					const std::uint32_t take = std::min<std::uint32_t>( cnt, oc - std::uint32_t( cell.objects.size() ) );
					listed += take;
					if ( rd.bad || listed > ( 1ULL << 26 ) )
						return fail( "object lists out of range" );
					for ( std::uint32_t x = 0; x < take; x++ )
						cell.objects.push_back( e + x );
				}
			}
		}
		if ( npt ) {
			if ( !po || !rd.has( std::uint64_t( o ) + po, 16ULL * npt ) )
				return fail( "portals absent or out of range" );
			t.portals.resize( size_t( npt ) );
			for ( std::uint64_t k = 0; k < npt; k++ ) {
				const std::uint64_t at = std::uint64_t( o ) + po + 16ULL * k;
				WwUmbraPortal & p = t.portals[size_t( k )];
				p.w0 = rd.u32( at );
				p.z = rd.u16( at + 4 );
				p.target = rd.u16( at + 6 );
				p.w8 = rd.u32( at + 8 );
				p.wc = rd.u32( at + 12 );
				if ( ( p.w0 >> 27 ) & 1 ) {	// a gate portal: its gate list and its box
					if ( !rd.has( gateIdx + 4ULL * ( p.w8 >> 12 ), 4ULL * ( p.w8 & 0xfff ) )
						|| !rd.has( gateVert + 12ULL * ( p.wc >> 12 ), 24 ) )
						return fail( "gate portal out of range" );
				}
			}
		}
		if ( rd.bad )
			return fail( "tile field out of range" );
	}
	// a portal into a leaf tile names one of its cells
	for ( const WwUmbraTile & t : tiles )
		for ( const WwUmbraPortal & p : t.portals ) {
			const std::uint32_t slot = p.w0 & 0x3ffffffU;
			if ( ( p.w0 >> 29 ) >= 6 )
				return fail( "portal face out of range" );
			if ( slot < numTiles && tiles[slot].present && tiles[slot].leaf && p.target >= tiles[slot].cells.size() )
				return fail( "portal target cell out of range" );
		}
	if ( !kdInit( tree, rd, treeN, rd.u32( 0x30 ), rd.u32( 0x3c ), treeN ) )
		return fail( "tome KD tree absent or out of range" );
	if ( rd.bad )
		return fail( "header field out of range" );
	ok = true;
	return true;
}

std::string WwUmbraTome::dump() const
{
	std::string s;
	char b[512];
	const Rd rd( d );
	std::snprintf( b, sizeof( b ), "tome objects=%u clusters=%u tiles=%u leaf=%u cells=%u gates=%u\n", unsigned( numObjects ),
		unsigned( numClusters ), unsigned( numTiles ), unsigned( numLeaf ), unsigned( numCells ), unsigned( numGates ) );
	s += b;
	for ( size_t i = 0; i < tiles.size(); i++ ) {
		const WwUmbraTile & t = tiles[i];
		if ( !t.present )
			continue;
		s += "tile " + std::to_string( i ) + " leaf=" + std::to_string( t.leaf ? 1 : 0 ) + " cells="
			+ std::to_string( t.numCells ) + " portals=" + std::to_string( t.portals.size() ) + " min=" + hex8( t.mn[0] )
			+ "," + hex8( t.mn[1] ) + "," + hex8( t.mn[2] ) + " max=" + hex8( t.mx[0] ) + "," + hex8( t.mx[1] ) + ","
			+ hex8( t.mx[2] ) + " pe=" + hex8( t.pe ) + "\n";
		for ( size_t c = 0; c < t.cells.size(); c++ ) {
			const WwUmbraCell & cell = t.cells[c];
			s += "cell " + std::to_string( i ) + "." + std::to_string( c ) + " portals=" + std::to_string( cell.portal ) + "+"
				+ std::to_string( cell.portalCount ) + " objects=";
			if ( cell.objects.empty() )
				s += "-";
			for ( size_t k = 0; k < cell.objects.size(); k++ )
				s += ( k ? "," : "" ) + std::to_string( cell.objects[k] );
			s += "\n";
		}
		for ( size_t k = 0; k < t.portals.size(); k++ ) {
			const WwUmbraPortal & p = t.portals[k];
			const unsigned face = p.w0 >> 29, out = ( p.w0 >> 28 ) & 1, slot = p.w0 & 0x3ffffffU;
			if ( ( p.w0 >> 27 ) & 1 ) {
				std::string gs;
				for ( std::uint32_t j = 0; j < ( p.w8 & 0xfff ); j++ ) {
					const std::int32_t g = std::int32_t( rd.u32( gateIdx + 4ULL * ( ( p.w8 >> 12 ) + j ) ) );
					gs += ( j ? "," : "" ) + std::to_string( g );
				}
				std::string box;
				for ( int m = 0; m < 6; m++ )
					box += ( m ? "," : "" ) + hex8( rd.f32( gateVert + 12ULL * ( p.wc >> 12 ) + 4ULL * m ) );
				std::snprintf( b, sizeof( b ), "portal %u.%u face=%u out=%u gate=1 slot=%u cell=%u gates=", unsigned( i ),
					unsigned( k ), face, out, slot, unsigned( p.target ) );
				s += b + gs + " box=" + box + "\n";
			} else {
				std::snprintf( b, sizeof( b ), "portal %u.%u face=%u out=%u gate=0 slot=%u cell=%u q=%u,%u,%u,%u,%u\n",
					unsigned( i ), unsigned( k ), face, out, slot, unsigned( p.target ), unsigned( p.z ), unsigned( p.w8 >> 16 ),
					unsigned( p.w8 & 0xffff ), unsigned( p.wc >> 16 ), unsigned( p.wc & 0xffff ) );
				s += b;
			}
		}
	}
	for ( std::uint32_t i = 0; i < numObjects; i++ ) {
		std::snprintf( b, sizeof( b ), "object %u uid=%08X b=", unsigned( i ), unsigned( uids[i] ) );
		s += b;
		for ( int k = 0; k < 6; k++ )
			s += ( k ? "," : "" ) + hex8( bounds[6 * size_t( i ) + size_t( k )] );
		s += " d=";
		if ( dist.empty() )
			s += "-";
		else
			for ( int k = 0; k < 8; k++ )
				s += ( k ? "," : "" ) + hex8( dist[8 * size_t( i ) + size_t( k )] );
		s += "\n";
	}
	return s;
}

// ------------------------------------------------------------------------------------------------ the query

std::uint64_t wwUmbraRasterHash( const WwUmbraRaster & r )
{
	std::uint64_t h = 0xcbf29ce484222325ULL;
	for ( int y = 0; y < 64; y++ )
		for ( int k = 0; k < 8; k++ ) {
			h ^= ( r.row[y] >> ( 8 * k ) ) & 0xff;
			h *= 0x100000001b3ULL;
		}
	return h;
}

void wwUmbraQuery( const WwUmbraTome & T, const WwUmbraCamera & camIn, const std::vector<char> & gateOpen,
	WwUmbraResult & out, bool flipFace )
{
	out = WwUmbraResult();
	if ( !T.ok ) {
		out.error = QStringLiteral( "tome not decoded" );
		return;
	}
	const Rd rd( T.d );
	const Cam cam = makeCam( camIn );
	out.starts = startCells( T, rd, cam.pos );
	if ( rd.bad ) {
		out.error = QStringLiteral( "start-cell search read out of range" );
		return;
	}
	std::unordered_map<std::uint64_t, int> at;
	std::vector<WwUmbraRaster> buf;
	std::vector<std::uint64_t> keyOf;
	std::vector<char> inwork;
	std::deque<int> work;
	for ( const auto & s : out.starts ) {
		const std::uint64_t key = ( std::uint64_t( s.first ) << 32 ) | std::uint32_t( s.second );
		WwUmbraRaster r;
		for ( int y = 0; y < 64; y++ )
			r.row[y] = ~0ULL;
		at[key] = int( buf.size() );
		buf.push_back( r );
		keyOf.push_back( key );
		inwork.push_back( 1 );
		work.push_back( int( buf.size() ) - 1 );
	}
	struct TileConst
	{
		bool done = false;
		float s[3], E;
		std::int64_t q[3], th[3];
	};
	std::vector<TileConst> tconst( T.tiles.size() );
	auto tc = [&]( size_t ti ) -> const TileConst & {
		TileConst & c = tconst[ti];
		if ( !c.done ) {
			const WwUmbraTile & t = T.tiles[ti];
			c.E = t.pe;
			for ( int a = 0; a < 3; a++ ) {
				const float ext = t.mx[a] - t.mn[a];
				c.s[a] = ext * K65535;
				const float r = 1.0f / c.s[a];
				const float dp = cam.pos[a] - t.mn[a];
				c.q[a] = trunc32( dp * r );
				const float re = r * c.E;
				c.th[a] = trunc32( re + 1.0f );
			}
			c.done = true;
		}
		return c;
	};
	while ( !work.empty() ) {
		const int bi = work.front();
		work.pop_front();
		inwork[bi] = 0;
		const std::uint64_t key = keyOf[bi];
		const size_t ti = size_t( key >> 32 );
		const std::uint32_t ci = std::uint32_t( key & 0xffffffffU );
		const WwUmbraTile & t = T.tiles[ti];
		const WwUmbraRaster parent = buf[bi];
		const TileConst & k0 = tc( ti );
		const WwUmbraCell & cell = t.cells[ci];
		for ( std::uint64_t pk = cell.portal; pk < std::uint64_t( cell.portal ) + cell.portalCount; pk++ ) {
			const WwUmbraPortal & p = t.portals[size_t( pk )];
			out.portals++;
			const int face = int( p.w0 >> 29 );
			const int axis = face >> 1, sgn = face & 1;
			if ( ( p.w0 >> 28 ) & 1 )
				continue;	// outside: the depth output's cell only
			const std::uint32_t slot = p.w0 & 0x3ffffffU;
			if ( slot >= T.tiles.size() || !T.tiles[slot].present || !T.tiles[slot].leaf )
				continue;	// only leaf tiles are active
			float mn[3], mx[3];
			int fc;
			const float E = k0.E;
			if ( ( p.w0 >> 27 ) & 1 ) {
				bool open = true;
				for ( std::uint32_t j = 0; j < ( p.w8 & 0xfff ); j++ ) {
					const std::int32_t g = std::int32_t( rd.u32( T.gateIdx + 4ULL * ( ( p.w8 >> 12 ) + j ) ) );
					if ( !( g < 0 || size_t( g ) >= gateOpen.size() || gateOpen[size_t( g )] ) )
						open = false;
				}
				if ( !open )
					continue;
				const std::uint64_t gv = T.gateVert + 12ULL * ( p.wc >> 12 );
				for ( int a = 0; a < 3; a++ ) {
					mn[a] = rd.f32( gv + 4ULL * a ) - E;
					mx[a] = E + rd.f32( gv + 12ULL + 4ULL * a );
				}
				fc = -1;
				float best = 0.0f;
				for ( int a = 0; a < 3; a++ ) {
					const float lo = mn[a] - cam.pos[a];
					const float hi = cam.pos[a] - mx[a];
					if ( lo > best ) {
						best = lo;
						fc = 2 * a | 1;
					} else if ( hi > best ) {
						best = hi;
						fc = 2 * a;
					}
				}
			} else {
				std::int64_t v = k0.q[axis] - std::int64_t( p.z );
				if ( sgn == 0 )
					v = -v;
				if ( v > k0.th[axis] )
					continue;	// backfacing
				const int a1 = ( axis + 1 ) % 3, a2 = ( a1 + 1 ) % 3;
				std::int64_t qmin[3], qmax[3];
				qmin[axis] = qmax[axis] = p.z;
				qmin[a1] = p.w8 >> 16;
				qmax[a1] = p.w8 & 0xffff;
				qmin[a2] = p.wc >> 16;
				qmax[a2] = p.wc & 0xffff;
				for ( int a = 0; a < 3; a++ ) {
					const float lo = float( qmin[a] ) * k0.s[a];
					const float lo2 = lo + t.mn[a];
					mn[a] = lo2 - E;
					const float hi = float( qmax[a] ) * k0.s[a];
					const float hi2 = hi + t.mn[a];
					mx[a] = hi2 + E;
				}
				if ( -v > k0.th[axis] ) {
					fc = face;
				} else {
					fc = -1;
					std::int64_t best = 0;
					for ( const int a : { a1, a2 } ) {
						std::int64_t dd = qmin[a] - k0.q[a] - k0.th[a];
						if ( dd > best ) {
							best = dd;
							fc = 2 * a | 1;
						}
						dd = k0.q[a] - qmax[a] - k0.th[a];
						if ( dd > best ) {
							best = dd;
							fc = 2 * a;
						}
					}
				}
			}
			out.entered++;
			const std::uint64_t dkey = ( std::uint64_t( slot ) << 32 ) | p.target;
			WwUmbraRaster cover;
			if ( fc < 0 ) {
				out.whole++;
				cover = parent;
			} else {
				if ( flipFace )
					fc ^= 1;
				const Quad q = corners( cam, mn, mx, fc );
				float nd[4];
				if ( !ndcBox( q, nd ) )
					continue;
				std::int64_t ir[4];
				irect( nd, ir );
				if ( ir[0] >= ir[2] || ir[1] >= ir[3] )
					continue;
				out.rastered++;
				float a2[4], b2[4], c2[4];
				coeffs( q, cam.rec[fc], a2, b2, c2 );
				raster( ir, a2, b2, c2, cover );
				for ( int y = 0; y < 64; y++ )
					cover.row[y] &= parent.row[y];
			}
			auto it = at.find( dkey );
			if ( it == at.end() ) {
				if ( !anyBits( cover ) )
					continue;
				at[dkey] = int( buf.size() );
				buf.push_back( cover );
				keyOf.push_back( dkey );
				inwork.push_back( 1 );
				work.push_back( int( buf.size() ) - 1 );
				continue;
			}
			const int di = it->second;
			bool grows = false;
			for ( int y = 0; y < 64; y++ )
				grows = grows || ( cover.row[y] & ~buf[di].row[y] ) != 0;
			if ( grows ) {
				for ( int y = 0; y < 64; y++ )
					buf[di].row[y] |= cover.row[y];
				if ( !inwork[di] ) {
					inwork[di] = 1;
					work.push_back( di );
				}
			}
		}
	}
	// the objects: distance, six planes, projection, rectangle against the cell's coverage (no depth form)
	out.objVisible.assign( T.numObjects, 0 );
	for ( size_t bi = 0; bi < buf.size(); bi++ ) {
		if ( !anyBits( buf[bi] ) )
			continue;
		const WwUmbraTile & t = T.tiles[size_t( keyOf[bi] >> 32 )];
		const WwUmbraCell & cell = t.cells[size_t( keyOf[bi] & 0xffffffffU )];
		for ( const std::uint32_t o : cell.objects ) {
			if ( o >= T.numObjects || out.objVisible[o] )
				continue;
			if ( objectVisible( T, cam, o, buf[bi] ) )
				out.objVisible[o] = 1;
		}
	}
	for ( std::uint32_t o = 0; o < T.numObjects; o++ )
		if ( out.objVisible[o] )
			out.visible.push_back( o );
	// the reached cells, sorted by key
	std::vector<int> order( buf.size() );
	for ( size_t i = 0; i < order.size(); i++ )
		order[i] = int( i );
	std::sort( order.begin(), order.end(), [&]( int x, int y ) { return keyOf[size_t( x )] < keyOf[size_t( y )]; } );
	for ( const int i : order ) {
		if ( !anyBits( buf[size_t( i )] ) )
			continue;
		out.keys.push_back( keyOf[size_t( i )] );
		out.rasters.push_back( buf[size_t( i )] );
	}
	if ( rd.bad ) {
		out.error = QStringLiteral( "query read out of range" );
		return;
	}
	out.ok = true;
}

std::string wwUmbraQueryText( const WwUmbraTome & t, const WwUmbraCamera & cam, const std::vector<char> & gateOpen,
	const WwUmbraResult & r )
{
	(void)t;
	std::string s = "M=";
	for ( int k = 0; k < 16; k++ )
		s += ( k ? "," : "" ) + hex8( cam.M[k] );
	s += "\npos=" + hex8( cam.pos[0] ) + "," + hex8( cam.pos[1] ) + "," + hex8( cam.pos[2] ) + "\ngates=";
	for ( const char g : gateOpen )
		s += g ? '1' : '0';
	s += "\nstart=";
	for ( size_t k = 0; k < r.starts.size(); k++ )
		s += ( k ? "," : "" ) + std::to_string( r.starts[k].first ) + "." + std::to_string( r.starts[k].second );
	s += "\n";
	char b[96];
	for ( size_t k = 0; k < r.keys.size(); k++ ) {
		int bits = 0;
		for ( int y = 0; y < 64; y++ )
			for ( std::uint64_t x = r.rasters[k].row[y]; x; x &= x - 1 )
				bits++;
		std::snprintf( b, sizeof( b ), "cell=%u.%u %d %016llX\n", unsigned( r.keys[k] >> 32 ),
			unsigned( r.keys[k] & 0xffffffffU ), bits, (unsigned long long)wwUmbraRasterHash( r.rasters[k] ) );
		s += b;
	}
	s += "visible=";
	for ( size_t k = 0; k < r.visible.size(); k++ )
		s += ( k ? "," : "" ) + std::to_string( r.visible[k] );
	s += "\n";
	return s;
}
