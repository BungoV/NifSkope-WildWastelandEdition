/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgengpu.h"

#include "lodgenbc7.h"
#include "harnesswindow.h"   // wwHarnessSettingsSuffix
#include "version.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QSettings>
#include <QStringList>
#include <QThread>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

// the vendored detex BC7 decoder (lib/libfo76utils/src/decompress-bptc.c): the self-check's independent measure
extern "C" bool detexDecompressBlockBPTC( const uint8_t * bitstring, uint32_t mode_mask, uint32_t flags, uint8_t * pixel_buffer );

const char * const kLodgenGpuSettingKey = "Settings/Nif/Use GPU";

/* THE BC7 ENCODER IN GLSL. The same search as src/lodgenbc7.h -- the same
 * modes in the same order, the same four ranked partitions per two-subset mode,
 * the same least-squares refinement and local search, the same ties (strict <,
 * earlier candidate wins) -- in single precision. Read the two side by side.
 *
 *  * NOT THE CPU'S BYTES. The CPU fits in doubles; a double-precision port that
 *    matched it bit for bit was measured slower than the CPU (lane GPU1: GPU
 *    3.0 s vs 16 CPU threads 0.4 s on a 1024x512 image; a GeForce runs doubles
 *    at 1/64 rate). In floats a fit can land one code step away from the
 *    CPU's, either way. What is exact: the palette, the index choice and the
 *    error of the chosen block are integer arithmetic, so the error each block
 *    reports is the true weighted squared error of the bytes it wrote.
 *  * THE GATE is the CPU's own error measure (the weighted squared error the
 *    encoder minimises), per texture class: on the card normal sheets, the one
 *    class that is BC7-encoded, the GPU's total must not exceed the CPU's.
 *    The context's start-up self-check re-measures it on a fixed image and
 *    decodes every GPU block with the vendored detex decoder: a block whose
 *    decoded error differs from its reported error, or a total above the
 *    CPU's, refuses the GPU path.
 *  * DETERMINISM: no atomics, no reductions across threads: one thread per
 *    block, each block a pure function of its 16 pixels. Two runs on one
 *    machine write the same bytes.
 *  * The error is an int, not an int64: its largest value is 16 texels x
 *    (1+1+32+1) x 255^2 = 36.4 million, under 2^31 for any weight up to 500.
 *    The host refuses heavier weights.
 *  * A subset is a 16-bit pixel mask, and the fitted indices are stored per
 *    pixel, four bits each. */
static const char * const kBc7Glsl = R"GLSL(
#version 430 core
layout( local_size_x = 64 ) in;
layout( std430, binding = 0 ) readonly buffer SrcBuf { uint srcPx[]; };
layout( std430, binding = 1 ) buffer DstBuf { uvec4 dstBlk[]; };
layout( std430, binding = 2 ) buffer ErrBuf { int dstErr[]; };
layout( std430, binding = 3 ) readonly buffer ListBuf { int blkList[]; };
/* pass 0: mode 6 on every block; pass 1: modes 5 and 4 on blkList, the blocks pass 0 left with error; pass 2:
 * the two-subset modes on the blocks pass 1 left with error. The single search, cut where it tests err > 0,
 * so a warp's 32 threads run blocks that all still have work (the same bytes as one pass). */
uniform int uPass;
uniform int uW;
uniform int uH;
uniform int uBw;
uniform int uFirst;
uniform int uCount;
uniform ivec4 uWt;

%TABLES%

const int INT_MAX = 0x7fffffff;
const uint ALL = 0xFFFFu;

// the block's pixels, R G B A packed low to high: 16 words, not 16 vectors, so a dynamic index stays cheap
uint SRCP[16];
uint PXP[16];
ivec4 P( int i ) { uint p = PXP[i]; return ivec4( int( p & 255u ), int( ( p >> 8 ) & 255u ), int( ( p >> 16 ) & 255u ), int( p >> 24 ) ); }
ivec4 WT;
vec4 SW;

uvec4 OUT;
int POS;
uvec4 bestOut;
int bestErr;

// the BC7 interpolation weights: round( 64 k / ( 2^ib - 1 ) ), the detex tables' values (the host checks)
// (64 k / n is never within 1/30 of a half, so the float rounding is exact)
int wOf( int ib, int k ) { return int( float( 64 * k ) / float( ( 1 << ib ) - 1 ) + 0.5 ); }
int expandBits( int v, int bits ) { v <<= ( 8 - bits ); return v | ( v >> bits ); }
bool inMask( uint m, int i ) { return ( ( m >> uint( i ) ) & 1u ) != 0u; }

struct Spec { ivec4 ch; int nc; int bits; int pbit; int ib; ivec4 on; };
struct Fit { ivec4 q[2]; ivec4 ep[2]; ivec2 p; uvec2 idx; int err; };

Spec spec( ivec4 ch, int nc, int bits, int pbit, int ib )
{
	ivec4 on = ivec4( 0 );
	for ( int j = 0; j < nc; j++ ) on[ch[j]] = 1;
	return Spec( ch, nc, bits, pbit, ib, on );
}

Fit fitNew()
{
	Fit f;
	f.q[0] = ivec4( 0 ); f.q[1] = ivec4( 0 );
	f.ep[0] = ivec4( 0 ); f.ep[1] = ivec4( 0 );
	f.p = ivec2( 0 );
	f.idx = uvec2( 0u );
	f.err = INT_MAX;
	return f;
}

// the fitted index of PIXEL i (4 bits each)
int idxGet( uvec2 v, int i ) { return int( ( v[i >> 3] >> uint( ( i & 7 ) * 4 ) ) & 15u ); }
uvec2 idxSet( uvec2 v, int i, int k )
{
	uint sh = uint( ( i & 7 ) * 4 );
	v[i >> 3] = ( v[i >> 3] & ~( 15u << sh ) ) | ( uint( k ) << sh );
	return v;
}

void reconstruct( Spec s, inout Fit f )
{
	for ( int e = 0; e < 2; e++ )
		for ( int j = 0; j < s.nc; j++ ) {
			int c = s.ch[j];
			f.ep[e][c] = s.pbit != 0 ? expandBits( ( f.q[e][c] << 1 ) | f.p[e], s.bits + 1 ) : expandBits( f.q[e][c], s.bits );
		}
}

// every pixel of the subset to its nearest palette entry; the weighted error, exact (integers)
int assignIdx( uint mask, Spec s, ivec4 ep0, ivec4 ep1, out uvec2 idx )
{
	int np = 1 << s.ib;
	ivec4 wm = WT * s.on;
	int tot = 0;
	idx = uvec2( 0u );
	for ( int i = 0; i < 16; i++ ) {
		if ( !inMask( mask, i ) )
			continue;
		ivec4 x = P( i );
		int best = INT_MAX, bk = 0;
		for ( int k = 0; k < np; k++ ) {
			// the palette entry, recomputed per pixel: ALU is cheaper than a spilled array
			int W = wOf( s.ib, k );
			ivec4 e = x - ( ( ( 64 - W ) * ep0 + W * ep1 + 32 ) >> 6 );
			int d = wm.x * e.x * e.x + wm.y * e.y * e.y + wm.z * e.z * e.z + wm.w * e.w * e.w;
			if ( d < best ) { best = d; bk = k; }
		}
		idx = idxSet( idx, i, bk );
		tot += best;
	}
	return tot;
}

int quantize( float x, int bits, int p )
{
	int maxq = ( 1 << bits ) - 1;
	int q0;
	if ( p < 0 )
		q0 = int( floor( x * float( maxq ) / 255.0 + 0.5 ) );
	else
		q0 = int( floor( ( x * float( ( 1 << ( bits + 1 ) ) - 1 ) / 255.0 - float( p ) ) * 0.5 + 0.5 ) );
	int best = 0;
	float bd = 1e30;
	for ( int q = q0 - 1; q <= q0 + 1; q++ ) {
		if ( q < 0 || q > maxq )
			continue;
		int v = p < 0 ? expandBits( q, bits ) : expandBits( ( q << 1 ) | p, bits + 1 );
		float d = abs( float( v ) - x );
		if ( d < bd ) { bd = d; best = q; }
	}
	return best;
}

void tryEndpoints( uint mask, Spec s, vec4 e0, vec4 e1, inout Fit best )
{
	int combos = s.pbit == 1 ? 4 : ( s.pbit == 2 ? 2 : 1 );
	for ( int cb = 0; cb < combos; cb++ ) {
		Fit f = fitNew();
		if ( s.pbit == 1 )
			f.p = ivec2( cb & 1, ( cb >> 1 ) & 1 );
		else if ( s.pbit == 2 )
			f.p = ivec2( cb, cb );
		for ( int j = 0; j < s.nc; j++ ) {
			int c = s.ch[j];
			f.q[0][c] = quantize( e0[c], s.bits, s.pbit != 0 ? f.p[0] : -1 );
			f.q[1][c] = quantize( e1[c], s.bits, s.pbit != 0 ? f.p[1] : -1 );
		}
		reconstruct( s, f );
		f.err = assignIdx( mask, s, f.ep[0], f.ep[1], f.idx );
		if ( f.err < best.err )
			best = f;
	}
}

Fit fitSubset( uint mask, Spec s )
{
	vec4 on = vec4( s.on );
	float n = float( bitCount( mask ) );
	vec4 m = vec4( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) )
			m += vec4( P( i ) );
	m = m / n * on;
	vec4 sw = mix( vec4( 1.0 ), SW, on );
	mat4 cov = mat4( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			vec4 y = ( vec4( P( i ) ) - m ) * sw * on;
			cov += outerProduct( y, y );
		}
	int top = s.ch[0];
	for ( int j = 1; j < s.nc; j++ )
		if ( cov[s.ch[j]][s.ch[j]] > cov[top][top] )
			top = s.ch[j];
	vec4 v = cov[top];
	float len = 0.0;
	for ( int it = 0; it < 12; it++ ) {
		vec4 nv = cov * v;
		len = length( nv );
		if ( len < 1e-12 )
			break;
		v = nv / len;
	}
	float tmin = 0.0, tmax = 0.0;
	if ( len >= 1e-12 ) {
		tmin = 1e30;
		tmax = -1e30;
		for ( int i = 0; i < 16; i++ )
			if ( inMask( mask, i ) ) {
				float t = dot( ( vec4( P( i ) ) - m ) * sw * on, v );
				tmin = min( tmin, t );
				tmax = max( tmax, t );
			}
	} else {
		v = vec4( 0.0 );
	}
	vec4 e0 = clamp( m + tmin * v / sw, 0.0, 255.0 ) * on;
	vec4 e1 = clamp( m + tmax * v / sw, 0.0, 255.0 ) * on;
	Fit best = fitNew();
	// it == -1: the principal-axis endpoints; 0..2: the least-squares refinements
	for ( int it = -1; it < 3; it++ ) {
		if ( it >= 0 ) {
			if ( best.err <= 0 )
				break;
			float A = 0.0, B = 0.0, C = 0.0;
			vec4 X0 = vec4( 0.0 ), X1 = vec4( 0.0 );
			for ( int i = 0; i < 16; i++ )
				if ( inMask( mask, i ) ) {
					float t = float( wOf( s.ib, idxGet( best.idx, i ) ) ) / 64.0;
					float u = 1.0 - t;
					A += u * u;
					B += u * t;
					C += t * t;
					X0 += u * vec4( P( i ) );
					X1 += t * vec4( P( i ) );
				}
			float det = A * C - B * B;
			if ( abs( det ) < 1e-9 )
				break;
			e0 = clamp( ( C * X0 - B * X1 ) / det, 0.0, 255.0 ) * on;
			e1 = clamp( ( A * X1 - B * X0 ) / det, 0.0, 255.0 ) * on;
		}
		int before = best.err;
		tryEndpoints( mask, s, e0, e1, best );
		if ( it >= 0 && best.err >= before )
			break;
	}
	// local search: one code step at a time (endpoint, channel, -1 then +1), then the p-bits
	int maxq = ( 1 << s.bits ) - 1;
	for ( int pass = 0; pass < 3 && best.err > 0; pass++ ) {
		bool improved = false;
		int nq4 = 4 * s.nc;
		int nmv = nq4 + ( s.pbit == 1 ? 2 : 0 );
		for ( int mv = 0; mv < nmv; mv++ ) {
			Fit f = best;
			if ( mv < nq4 ) {
				int k = mv / ( 2 * s.nc );
				int j = ( mv % ( 2 * s.nc ) ) / 2;
				int d = ( mv & 1 ) != 0 ? 1 : -1;
				int c = s.ch[j];
				int nq = best.q[k][c] + d;
				if ( nq < 0 || nq > maxq )
					continue;
				f.q[k][c] = nq;
			} else {
				int k = mv - nq4;
				f.p[k] ^= 1;
			}
			reconstruct( s, f );
			f.err = assignIdx( mask, s, f.ep[0], f.ep[1], f.idx );
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

void fixAnchor( inout Fit f, uint mask, int anchorPix, Spec s )
{
	int hf = 1 << ( s.ib - 1 ), top = ( 1 << s.ib ) - 1;
	if ( idxGet( f.idx, anchorPix ) < hf )
		return;
	for ( int j = 0; j < s.nc; j++ ) {
		int c = s.ch[j];
		int t = f.q[0][c]; f.q[0][c] = f.q[1][c]; f.q[1][c] = t;
		t = f.ep[0][c]; f.ep[0][c] = f.ep[1][c]; f.ep[1][c] = t;
	}
	int t = f.p[0]; f.p[0] = f.p[1]; f.p[1] = t;
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) )
			f.idx = idxSet( f.idx, i, top - idxGet( f.idx, i ) );
}

void put( uint v, int n )
{
	for ( int i = 0; i < n; i++, POS++ )
		if ( ( ( v >> uint( i ) ) & 1u ) != 0u )
			OUT[POS >> 5] |= 1u << uint( POS & 31 );
}

void keep( int err )
{
	if ( err < bestErr ) {
		bestErr = err;
		bestOut = OUT;
	}
}

void mode6()
{
	Spec s = spec( ivec4( 0, 1, 2, 3 ), 4, 7, 1, 4 );
	Fit f = fitSubset( ALL, s );
	fixAnchor( f, ALL, 0, s );
	OUT = uvec4( 0u ); POS = 0;
	put( 1u << 6, 7 );
	for ( int c = 0; c < 4; c++ ) {
		put( uint( f.q[0][c] ), 7 );
		put( uint( f.q[1][c] ), 7 );
	}
	put( uint( f.p[0] ), 1 );
	put( uint( f.p[1] ), 1 );
	for ( int i = 0; i < 16; i++ )
		put( uint( idxGet( f.idx, i ) ), i == 0 ? 3 : 4 );
	keep( f.err );
}

void mode45( int mode, int rot, int isb )
{
	for ( int i = 0; i < 16; i++ ) PXP[i] = SRCP[i];
	WT = uWt;
	SW = sqrt( vec4( max( uWt, ivec4( 1 ) ) ) );
	if ( rot != 0 ) {
		int r = rot - 1;
		uint sh = uint( 8 * r );
		for ( int i = 0; i < 16; i++ ) {
			uint p = PXP[i];
			uint a = p >> 24, c = ( p >> sh ) & 255u;
			PXP[i] = ( p & ~( 255u << sh ) & 0x00FFFFFFu ) | ( a << sh ) | ( c << 24 );
		}
		int t = WT[r]; WT[r] = WT[3]; WT[3] = t;
		float ts = SW[r]; SW[r] = SW[3]; SW[3] = ts;
	}
	int cbits = mode == 4 ? 5 : 7, abits = mode == 4 ? 6 : 8;
	int cib = mode == 4 ? ( isb != 0 ? 3 : 2 ) : 2;
	int aib = mode == 4 ? ( isb != 0 ? 2 : 3 ) : 2;
	Spec sc = spec( ivec4( 0, 1, 2, 0 ), 3, cbits, 0, cib );
	Spec sa = spec( ivec4( 3, 0, 0, 0 ), 1, abits, 0, aib );
	Fit fc = fitSubset( ALL, sc );
	fixAnchor( fc, ALL, 0, sc );
	Fit fa = fitSubset( ALL, sa );
	fixAnchor( fa, ALL, 0, sa );
	OUT = uvec4( 0u ); POS = 0;
	put( 1u << uint( mode ), mode + 1 );
	put( uint( rot ), 2 );
	if ( mode == 4 )
		put( uint( isb ), 1 );
	for ( int c = 0; c < 3; c++ ) {
		put( uint( fc.q[0][c] ), cbits );
		put( uint( fc.q[1][c] ), cbits );
	}
	put( uint( fa.q[0][3] ), abits );
	put( uint( fa.q[1][3] ), abits );
	bool swapSets = mode == 4 && isb != 0;
	uvec2 firstIdx = swapSets ? fa.idx : fc.idx, secondIdx = swapSets ? fc.idx : fa.idx;
	int ib2 = mode == 4 ? 3 : 2;
	for ( int i = 0; i < 16; i++ )
		put( uint( idxGet( firstIdx, i ) ), i == 0 ? 1 : 2 );
	for ( int i = 0; i < 16; i++ )
		put( uint( idxGet( secondIdx, i ) ), i == 0 ? ib2 - 1 : ib2 );
	keep( fc.err + fa.err );
	for ( int i = 0; i < 16; i++ ) PXP[i] = SRCP[i];
	WT = uWt;
	SW = sqrt( vec4( max( uWt, ivec4( 1 ) ) ) );
}

// a cheap unquantized estimate of a subset's fit error over channels 0..nc-1, for ranking partitions
float quickErr( uint mask, int nc, int levels )
{
	float n = float( bitCount( mask ) );
	if ( n == 0.0 )
		return 0.0;
	vec4 on = nc == 4 ? vec4( 1.0 ) : vec4( 1.0, 1.0, 1.0, 0.0 );
	vec4 m = vec4( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) )
			m += vec4( P( i ) );
	m = m / n * on;
	vec4 sw = SW * on;
	mat4 cov = mat4( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			vec4 y = ( vec4( P( i ) ) - m ) * sw;
			cov += outerProduct( y, y );
		}
	int top = 0;
	for ( int c = 1; c < nc; c++ )
		if ( cov[c][c] > cov[top][top] )
			top = c;
	vec4 v = cov[top];
	for ( int it = 0; it < 6; it++ ) {
		vec4 nv = cov * v;
		float len = length( nv );
		if ( len < 1e-12 )
			return 0.0;
		v = nv / len;
	}
	float tmin = 1e30, tmax = -1e30;
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			float t = dot( ( vec4( P( i ) ) - m ) * sw, v );
			tmin = min( tmin, t );
			tmax = max( tmax, t );
		}
	float stp = ( tmax - tmin ) / float( levels - 1 );
	float err = 0.0;
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			vec4 y = ( vec4( P( i ) ) - m ) * sw;
			float t = dot( y, v );
			float r2 = dot( y, y ) - t * t;
			float q = 0.0;
			if ( stp > 0.0 ) {
				float k = floor( ( t - tmin ) / stp + 0.5 );
				q = t - ( tmin + k * stp );
			}
			err += max( r2, 0.0 ) + q * q;
		}
	return err;
}

// the four best two-subset partitions, best first; ties to the lower partition number
void bestPartitions( int nc, int levels, out int parts[4] )
{
	float sc[4];
	for ( int k = 0; k < 4; k++ ) { parts[k] = -1; sc[k] = 0.0; }
	for ( int pt = 0; pt < 64; pt++ ) {
		uint m1 = P2SUB[pt];
		float s = quickErr( ALL & ~m1, nc, levels ) + quickErr( m1, nc, levels );
		// insert after every entry that is <= s: the earlier partition keeps a tie
		int at = 4;
		for ( int k = 3; k >= 0; k-- )
			if ( parts[k] < 0 || s < sc[k] )
				at = k;
		if ( at < 4 ) {
			for ( int k = 3; k > at; k-- ) { parts[k] = parts[k - 1]; sc[k] = sc[k - 1]; }
			parts[at] = pt;
			sc[at] = s;
		}
	}
}

void mode2sub( int mode, int pt )
{
	Spec s = spec( ivec4( 0, 1, 2, 3 ), mode == 7 ? 4 : 3, mode == 7 ? 5 : ( mode == 3 ? 7 : 6 ),
		mode == 1 ? 2 : 1, mode == 1 ? 3 : 2 );
	uint msk1 = P2SUB[pt];
	uint msk0 = ALL & ~msk1;
	int anchor1 = ANCHOR2[pt];
	// two named fits, not an array: a dynamically indexed array of structs lives in local memory
	Fit f0 = fitSubset( msk0, s );
	fixAnchor( f0, msk0, 0, s );
	Fit f1 = fitSubset( msk1, s );
	fixAnchor( f1, msk1, anchor1, s );
	int err = f0.err + f1.err;
	if ( err >= bestErr )
		return;
	OUT = uvec4( 0u ); POS = 0;
	put( 1u << uint( mode ), mode + 1 );
	put( uint( pt ), 6 );
	for ( int c = 0; c < s.nc; c++ ) {
		put( uint( f0.q[0][c] ), s.bits );
		put( uint( f0.q[1][c] ), s.bits );
		put( uint( f1.q[0][c] ), s.bits );
		put( uint( f1.q[1][c] ), s.bits );
	}
	if ( mode == 1 ) {
		put( uint( f0.p[0] ), 1 );
		put( uint( f1.p[0] ), 1 );
	} else {
		put( uint( f0.p[0] ), 1 );
		put( uint( f0.p[1] ), 1 );
		put( uint( f1.p[0] ), 1 );
		put( uint( f1.p[1] ), 1 );
	}
	for ( int i = 0; i < 16; i++ ) {
		bool in1 = inMask( msk1, i );
		bool anchor = ( !in1 && i == 0 ) || ( in1 && i == anchor1 );
		put( uint( idxGet( in1 ? f1.idx : f0.idx, i ) ), anchor ? s.ib - 1 : s.ib );
	}
	keep( err );
}

void main()
{
	uint gid = gl_GlobalInvocationID.x;
	if ( gid >= uint( uCount ) )
		return;
	int blk = uPass == 0 ? uFirst + int( gid ) : blkList[uFirst + int( gid )];
	int bx = blk % uBw, by = blk / uBw;
	bool opaque = true;
	for ( int i = 0; i < 16; i++ ) {
		int sx = min( bx * 4 + ( i & 3 ), uW - 1 );
		int sy = min( by * 4 + ( i >> 2 ), uH - 1 );
		uint p = srcPx[sy * uW + sx];   // 0xAARRGGBB -> R G B A low to high
		SRCP[i] = ( ( p >> 16 ) & 255u ) | ( p & 0xFF00u ) | ( ( p & 255u ) << 16 ) | ( p & 0xFF000000u );
		PXP[i] = SRCP[i];
		opaque = opaque && ( p >> 24 ) == 255u;
	}
	WT = uWt;
	SW = sqrt( vec4( max( uWt, ivec4( 1 ) ) ) );
	if ( uPass == 0 ) {
		bestErr = INT_MAX;
		bestOut = uvec4( 0u );
		mode6();
		dstBlk[blk] = bestOut;
		dstErr[blk] = bestErr;
		return;
	}
	// a later pass takes up where the one before stopped, on the blocks it left with error
	bestErr = dstErr[blk];
	bestOut = dstBlk[blk];
	if ( uPass == 1 ) {
		for ( int rot = 0; rot < 4 && bestErr > 0; rot++ )
			for ( int v = 0; v < 3; v++ )
				mode45( v == 0 ? 5 : 4, rot, v == 2 ? 1 : 0 );
		dstBlk[blk] = bestOut;
		dstErr[blk] = bestErr;
		return;
	}
	for ( int ph = 0; ph < 3; ph++ ) {
		if ( ph > 0 && !opaque )
			break;
		if ( bestErr <= 0 )
			break;
		int parts[4];
		bestPartitions( ph == 0 ? 4 : 3, ph == 2 ? 8 : 4, parts );
		int mode = ph == 0 ? 7 : ( ph == 1 ? 3 : 1 );
		for ( int k = 0; k < 4 && bestErr > 0; k++ )
			mode2sub( mode, parts[k] );
	}
	dstBlk[blk] = bestOut;
	dstErr[blk] = bestErr;
}
)GLSL";

namespace
{

//! The shader's weight formula (wOf) against the vendored detex weight tables; true when every entry matches.
bool weightFormulaMatches()
{
	const uint16_t * tabs[3] = { detex_bptc_table_aWeight2, detex_bptc_table_aWeight3, detex_bptc_table_aWeight4 };
	for ( int ib = 2; ib <= 4; ib++ )
		for ( int k = 0; k < ( 1 << ib ); k++ )
			if ( int( float( 64 * k ) / float( ( 1 << ib ) - 1 ) + 0.5f ) != int( tabs[ib - 2][k] ) )
				return false;
	return true;
}

//! The shader source with the vendored detex partition tables filled in.
QByteArray bc7Source()
{
	QString t;
	t += "const uint P2SUB[64] = uint[64](";
	for ( int pt = 0; pt < 64; pt++ ) {
		quint32 bits = 0;
		for ( int i = 0; i < 16; i++ )
			if ( detex_bptc_table_P2[pt * 16 + i] )
				bits |= 1u << i;
		t += QString( "%1%2u" ).arg( pt ? "," : "" ).arg( bits );
	}
	t += ");\nconst int ANCHOR2[64] = int[64](";
	for ( int pt = 0; pt < 64; pt++ )
		t += QString( "%1%2" ).arg( pt ? "," : "" ).arg( int( detex_bptc_table_anchor_index_second_subset[pt] ) );
	t += ");\n";
	QString s = QString::fromLatin1( kBc7Glsl );
	s.replace( "%TABLES%", t );
	return s.toLatin1();
}

/* Below this many blocks an image goes to the CPU loop: a dispatch round trip
 * costs more than the CPU spends on it. */
const int kMinGpuBlocks = 256;
//! The first batch; later batches are sized to ~kTargetMs each (Windows resets a GPU job that runs 2 s).
const int kFirstBatch = 4096;
const double kTargetMs = 150.0;

struct GpuState
{
	bool tried = false;          // the context and its self-check are made once per process
	bool ready = false;          // ... and passed
	bool dead = false;           // a job failed after that: the context is not trusted again
	QString why;                 // why not ready / why dead
	QString ready1;              // the report line of a ready context
	std::atomic<bool> on{ false };
	QString report = QStringLiteral( "gpu: CPU path -- not configured (only `lodgen` runs set the path)" );
	QMutex jobLock;
	QThread * thread = nullptr;
	QObject * worker = nullptr;
	QOffscreenSurface * surface = nullptr;
	QOpenGLContext * ctx = nullptr;
	QOpenGLFunctions_4_3_Core * gl = nullptr;
	GLuint prog = 0, bufIn = 0, bufOut = 0, bufErr = 0, bufList = 0;
	GLint locW = -1, locH = -1, locBw = -1, locFirst = -1, locCount = -1, locWt = -1, locPass = -1;
	qint64 capIn = 0, capOut = 0, capErr = 0, capList = 0;
	double blocksPerMs[3] = { 0.0, 0.0, 0.0 };   // per pass: batch sizing only
	std::atomic<quint64> images{ 0 }, blocks{ 0 }, fellBack{ 0 }, gpuMs{ 0 };
	QString lastError;
};

GpuState & G()
{
	static GpuState g;
	return g;
}

template <class F> bool onGpuThread( F && f )
{
	GpuState & g = G();
	bool ok = false;
	QMetaObject::invokeMethod( g.worker, [&]() { ok = f(); }, Qt::BlockingQueuedConnection );
	return ok;
}

//! GPU thread: context current, program built, buffers made. Fills `why` on failure.
bool gpuInit( QString * why, QString * renderer )
{
	GpuState & g = G();
	if ( !g.ctx->makeCurrent( g.surface ) ) {
		*why = QStringLiteral( "makeCurrent on the offscreen surface failed" );
		return false;
	}
	g.gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>( g.ctx );
	if ( !g.gl || !g.gl->initializeOpenGLFunctions() ) {
		*why = QStringLiteral( "the driver gives no OpenGL 4.3 core functions" );
		return false;
	}
	auto * gl = g.gl;
	*renderer = QString::fromLatin1( reinterpret_cast<const char *>( gl->glGetString( GL_RENDERER ) ) );
	if ( !weightFormulaMatches() ) {
		*why = QStringLiteral( "the shader's BC7 weight formula does not match the detex tables" );
		return false;
	}
	const QByteArray src = bc7Source();
	const char * p = src.constData();
	GLuint sh = gl->glCreateShader( GL_COMPUTE_SHADER );
	gl->glShaderSource( sh, 1, &p, nullptr );
	gl->glCompileShader( sh );
	GLint ok = 0;
	gl->glGetShaderiv( sh, GL_COMPILE_STATUS, &ok );
	if ( !ok ) {
		char log[2048] = {};
		gl->glGetShaderInfoLog( sh, sizeof( log ) - 1, nullptr, log );
		*why = QStringLiteral( "the BC7 shader did not compile: " ) + QString::fromLatin1( log ).left( 400 ).simplified();
		gl->glDeleteShader( sh );
		return false;
	}
	g.prog = gl->glCreateProgram();
	gl->glAttachShader( g.prog, sh );
	gl->glLinkProgram( g.prog );
	gl->glDeleteShader( sh );
	gl->glGetProgramiv( g.prog, GL_LINK_STATUS, &ok );
	if ( !ok ) {
		char log[2048] = {};
		gl->glGetProgramInfoLog( g.prog, sizeof( log ) - 1, nullptr, log );
		*why = QStringLiteral( "the BC7 shader did not link: " ) + QString::fromLatin1( log ).left( 400 ).simplified();
		return false;
	}
	g.locW = gl->glGetUniformLocation( g.prog, "uW" );
	g.locH = gl->glGetUniformLocation( g.prog, "uH" );
	g.locBw = gl->glGetUniformLocation( g.prog, "uBw" );
	g.locFirst = gl->glGetUniformLocation( g.prog, "uFirst" );
	g.locCount = gl->glGetUniformLocation( g.prog, "uCount" );
	g.locWt = gl->glGetUniformLocation( g.prog, "uWt" );
	gl->glGenBuffers( 1, &g.bufIn );
	gl->glGenBuffers( 1, &g.bufOut );
	gl->glGenBuffers( 1, &g.bufErr );
	gl->glGenBuffers( 1, &g.bufList );
	// the list buffer must exist before pass 0 binds it
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufList );
	gl->glBufferData( GL_SHADER_STORAGE_BUFFER, 4096, nullptr, GL_DYNAMIC_DRAW );
	g.capList = 4096;
	g.locPass = gl->glGetUniformLocation( g.prog, "uPass" );
	return gl->glGetError() == GL_NO_ERROR;
}

//! GPU thread: release everything the context owns.
void gpuRelease()
{
	GpuState & g = G();
	if ( g.ctx && g.gl && g.ctx->makeCurrent( g.surface ) ) {
		if ( g.bufIn ) g.gl->glDeleteBuffers( 1, &g.bufIn );
		if ( g.bufOut ) g.gl->glDeleteBuffers( 1, &g.bufOut );
		if ( g.bufErr ) g.gl->glDeleteBuffers( 1, &g.bufErr );
		if ( g.bufList ) g.gl->glDeleteBuffers( 1, &g.bufList );
		if ( g.prog ) g.gl->glDeleteProgram( g.prog );
		g.ctx->doneCurrent();
	}
	g.bufIn = g.bufOut = g.bufErr = g.bufList = g.prog = 0;
	g.gl = nullptr;
	delete g.ctx;
	g.ctx = nullptr;
}

/*! GPU thread: one image. False on any GL error (the caller then runs the CPU loop).
 *  `errs`, when given, receives each block's reported weighted error. */
bool gpuBc7( const quint32 * px, int w, int h, const int wt[4], quint8 * out, std::vector<qint32> * errs = nullptr )
{
	GpuState & g = G();
	auto * gl = g.gl;
	if ( !gl || !g.ctx->isValid() || !g.ctx->makeCurrent( g.surface ) ) {
		g.lastError = QStringLiteral( "the context is gone" );
		return false;
	}
	while ( gl->glGetError() != GL_NO_ERROR ) {}
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	const int nb = bw * bh;
	const qint64 inBytes = qint64( w ) * h * 4, outBytes = qint64( nb ) * 16;
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufIn );
	if ( inBytes > g.capIn ) {
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, inBytes, nullptr, GL_DYNAMIC_DRAW );
		g.capIn = inBytes;
	}
	gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, inBytes, px );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufOut );
	if ( outBytes > g.capOut ) {
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, outBytes, nullptr, GL_DYNAMIC_READ );
		g.capOut = outBytes;
	}
	const qint64 errBytes = qint64( nb ) * 4;
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufErr );
	if ( errBytes > g.capErr ) {
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, errBytes, nullptr, GL_DYNAMIC_READ );
		g.capErr = errBytes;
	}
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, g.bufIn );
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, g.bufOut );
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, g.bufErr );
	gl->glUseProgram( g.prog );
	gl->glUniform1i( g.locW, w );
	gl->glUniform1i( g.locH, h );
	gl->glUniform1i( g.locBw, bw );
	gl->glUniform4i( g.locWt, wt[0], wt[1], wt[2], wt[3] );
	gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, g.bufList );
	QElapsedTimer clock;
	clock.start();
	/* Three passes (see the shader's uPass). Between passes the host reads the
	 * errors back and lists, in block order, the blocks still above zero: the
	 * list is a function of the errors alone, so the passes change the speed,
	 * never a byte. */
	std::vector<qint32> err( static_cast<size_t>( nb ) );
	std::vector<qint32> list;
	for ( int pass = 0; pass < 3; pass++ ) {
		int todo = nb;
		if ( pass > 0 ) {
			gl->glMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT );
			gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufErr );
			gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, errBytes, err.data() );
			list.clear();
			for ( int b = 0; b < nb; b++ )
				if ( err[size_t( b )] > 0 )
					list.push_back( b );
			todo = int( list.size() );
			if ( !todo )
				break;
			const qint64 listBytes = qint64( todo ) * 4;
			gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufList );
			if ( listBytes > g.capList ) {
				gl->glBufferData( GL_SHADER_STORAGE_BUFFER, listBytes, nullptr, GL_DYNAMIC_DRAW );
				g.capList = listBytes;
			}
			gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, listBytes, list.data() );
			gl->glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
		}
		gl->glUniform1i( g.locPass, pass );
		double & perMs = g.blocksPerMs[pass];
		for ( int first = 0; first < todo; ) {
			int count = perMs > 0.0 ? int( perMs * kTargetMs ) : kFirstBatch;
			count = qBound( 256, ( count + 63 ) / 64 * 64, 1 << 20 );
			count = qMin( count, todo - first );
			QElapsedTimer one;
			one.start();
			gl->glUniform1i( g.locFirst, first );
			gl->glUniform1i( g.locCount, count );
			gl->glDispatchCompute( GLuint( ( count + 63 ) / 64 ), 1, 1 );
			gl->glFinish();
			const double ms = qMax( 1.0, double( one.nsecsElapsed() ) / 1e6 );
			const double rate = double( count ) / ms;
			perMs = perMs > 0.0 ? qMin( perMs, rate ) * 0.5 + rate * 0.5 : rate;
			if ( gl->glGetError() != GL_NO_ERROR ) {
				g.lastError = QStringLiteral( "GL error in the BC7 dispatch" );
				return false;
			}
			first += count;
		}
	}
	gl->glMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufOut );
	gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, outBytes, out );
	if ( errs ) {
		errs->assign( size_t( nb ), 0 );
		gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, g.bufErr );
		gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, errBytes, errs->data() );
	}
	const bool ok = gl->glGetError() == GL_NO_ERROR && g.ctx->isValid();
	g.gpuMs += quint64( clock.elapsed() );
	if ( !ok )
		g.lastError = QStringLiteral( "GL error reading the BC7 blocks back" );
	return ok;
}

//! A fixed image for the start-up self-check: flat, two-tone, gradients, noise, cut-out alpha.
std::vector<quint32> selfCheckImage( int w, int h )
{
	std::vector<quint32> px( size_t( w ) * h );
	quint32 s = 0x9E3779B9u;
	auto rnd = [&s]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			const int bx = x / 4, by = y / 4, kind = ( bx + by * 7 ) % 8;
			quint32 r = 0, gg = 0, b = 0, a = 255;
			switch ( kind ) {
			case 0: r = 128; gg = 128; b = 255; break;
			case 1: r = ( x & 1 ) ? 20 : 230; gg = 128; b = ( y & 1 ) ? 40 : 200; break;
			case 2: r = x * 255 / qMax( 1, w - 1 ); gg = y * 255 / qMax( 1, h - 1 ); b = ( x + y ) & 255; break;
			case 3: r = rnd() & 255; gg = rnd() & 255; b = rnd() & 255; a = rnd() & 255; break;
			case 4: r = rnd() & 255; gg = rnd() & 255; b = rnd() & 255; break;
			case 5: r = 128 + ( rnd() & 7 ); gg = 128 - ( rnd() & 7 ); b = ( bx * 37 ) & 255; a = ( ( x ^ y ) & 2 ) ? 0 : 255; break;
			case 6: r = 250; gg = 3; b = 128 + ( y & 3 ) * 30; a = 200 + ( x & 3 ); break;
			default: r = gg = b = ( rnd() & 1 ) ? 255 : 0; break;
			}
			px[size_t( y ) * w + x] = ( a << 24 ) | ( r << 16 ) | ( gg << 8 ) | b;
		}
	return px;
}

//! Block (bx, by) of the image as the encoder reads it: R G B A, the edge pixels repeated.
void blockPixels( const quint32 * px, int w, int h, int bx, int by, uint8_t rgba[16][4] )
{
	for ( int i = 0; i < 16; i++ ) {
		const int sx = qMin( bx * 4 + ( i & 3 ), w - 1 ), sy = qMin( by * 4 + ( i >> 2 ), h - 1 );
		const quint32 p = px[size_t( sy ) * w + sx];
		rgba[i][0] = uint8_t( p >> 16 );
		rgba[i][1] = uint8_t( p >> 8 );
		rgba[i][2] = uint8_t( p );
		rgba[i][3] = uint8_t( p >> 24 );
	}
}

//! The CPU encoder over the image; returns its total weighted error.
qint64 cpuBc7( const std::vector<quint32> & px, int w, int h, const int wt[4], std::vector<quint8> & out )
{
	const int bw = ( w + 3 ) / 4, bh = ( h + 3 ) / 4;
	out.assign( size_t( bw ) * bh * 16, 0 );
	qint64 tot = 0;
	for ( int by = 0; by < bh; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			uint8_t rgba[16][4];
			blockPixels( px.data(), w, h, bx, by, rgba );
			tot += LodgenBc7::encodeBlock( rgba, wt, out.data() + ( size_t( by ) * bw + bx ) * 16 );
		}
	return tot;
}

/*! The weighted squared error of one written block against its source pixels,
 *  measured by DECODING the block with the vendored detex decoder (not the
 *  encoder's own arithmetic). -1 when the block does not decode. */
qint64 decodedError( const quint8 blk[16], const uint8_t rgba[16][4], const int wt[4] )
{
	uint8_t dec[64];
	if ( !detexDecompressBlockBPTC( blk, 0xFFFFFFFFu, 0, dec ) )
		return -1;
	qint64 e = 0;
	for ( int i = 0; i < 16; i++ )
		for ( int c = 0; c < 4; c++ ) {
			const int d = int( dec[i * 4 + c] ) - int( rgba[i][c] );
			e += qint64( wt[c] ) * d * d;
		}
	return e;
}

void shutdownGpu()
{
	GpuState & g = G();
	g.on = false;
	if ( g.thread ) {
		QMetaObject::invokeMethod( g.worker, []() { gpuRelease(); }, Qt::BlockingQueuedConnection );
		g.thread->quit();
		g.thread->wait();
		delete g.worker;
		g.worker = nullptr;
		delete g.thread;
		g.thread = nullptr;
	}
	delete g.ctx;          // a context that never reached the thread
	g.ctx = nullptr;
	delete g.surface;
	g.surface = nullptr;
}

} // namespace

static QSettings * guiSettings()
{
	return new QSettings( QStringLiteral( "NifTools" ),
		QStringLiteral( "NifSkope " ) + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION ) + wwHarnessSettingsSuffix() );
}

bool lodgenGpuSettingEnabled()
{
	QScopedPointer<QSettings> s( guiSettings() );
	return s->value( QLatin1String( kLodgenGpuSettingKey ), true ).toBool();
}

bool lodgenGpuWantedForArgs( int argc, char ** argv )
{
	bool lodgen = false;
	for ( int i = 1; i < argc; i++ ) {
		if ( !qstrcmp( argv[i], "--no-gpu" ) )
			return false;
		if ( !qstrcmp( argv[i], "lodgen" ) )
			lodgen = true;
	}
	if ( !lodgen || !lodgenGpuSettingEnabled() || argc < 1 )
		return false;
	/* Without the platform plugin a QGuiApplication aborts the process, so the
	 * plugin must be there before one is made: beside the exe, as every
	 * NifSkope folder ships it. */
	QString dir;
#ifdef Q_OS_WIN
	wchar_t buf[4096];
	const DWORD n = GetModuleFileNameW( nullptr, buf, 4096 );
	if ( n > 0 && n < 4096 )
		dir = QFileInfo( QString::fromWCharArray( buf, int( n ) ) ).absolutePath();
#endif
	if ( dir.isEmpty() )
		dir = QFileInfo( QString::fromLocal8Bit( argv[0] ) ).absolutePath();
	return QFileInfo::exists( dir + QStringLiteral( "/platforms/qwindows.dll" ) );
}

//! Main thread, once per process: the context, its thread and the self-check. Sets ready / why.
static void makeContext()
{
	GpuState & g = G();
	g.tried = true;
	auto fail = [&g]( const QString & why ) {
		g.why = QStringLiteral( "no GPU context: " ) + why;
		shutdownGpu();
	};
	if ( !qobject_cast<QGuiApplication *>( QCoreApplication::instance() ) ) {
		fail( QStringLiteral( "this process has no GUI platform (platforms/qwindows.dll missing beside the exe)" ) );
		return;
	}
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 3 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	g.surface = new QOffscreenSurface();
	g.surface->setFormat( fmt );
	g.surface->create();
	if ( !g.surface->isValid() ) {
		fail( QStringLiteral( "no offscreen surface" ) );
		return;
	}
	g.ctx = new QOpenGLContext();
	g.ctx->setFormat( fmt );
	if ( !g.ctx->create() ) {
		delete g.ctx;
		g.ctx = nullptr;
		fail( QStringLiteral( "the driver made no OpenGL context" ) );
		return;
	}
	const QSurfaceFormat got = g.ctx->format();
	if ( got.majorVersion() * 10 + got.minorVersion() < 43 ) {
		const QString v = QString( "OpenGL %1.%2 < 4.3" ).arg( got.majorVersion() ).arg( got.minorVersion() );
		delete g.ctx;
		g.ctx = nullptr;
		fail( v );
		return;
	}
	g.thread = new QThread();
	g.thread->setObjectName( QStringLiteral( "lodgen-gpu" ) );
	g.worker = new QObject();
	g.worker->moveToThread( g.thread );
	g.ctx->moveToThread( g.thread );
	g.thread->start();
	qAddPostRoutine( shutdownGpu );
	QString why, renderer;
	if ( !onGpuThread( [&]() { return gpuInit( &why, &renderer ); } ) ) {
		fail( why.isEmpty() ? QStringLiteral( "OpenGL setup failed" ) : why );
		return;
	}
	/* THE SELF-CHECK, on a fixed image under an even and an uneven weighting
	 * (the card sheet's). The GPU path is live only if (1) every GPU block
	 * decodes, with the detex decoder, to exactly the error the shader reported
	 * -- the bytes are a well-formed encoding of the fit it chose -- and (2) the
	 * GPU's total error is no more than the CPU encoder's on the same image. */
	const int sw = 128, sh = 64, bw = sw / 4, bh = sh / 4;
	const std::vector<quint32> img = selfCheckImage( sw, sh );
	const int wts[2][4] = { { 1, 1, 1, 1 }, { 1, 1, 32, 1 } };
	int blocks = 0, bad = 0;
	qint64 cpuTot = 0, gpuTot = 0;
	QString over;
	for ( const auto & wt : wts ) {
		std::vector<quint8> cpu, gpu( size_t( bw ) * bh * 16 );
		std::vector<qint32> errs;
		const qint64 c = cpuBc7( img, sw, sh, wt, cpu );
		if ( !onGpuThread( [&]() { return gpuBc7( img.data(), sw, sh, wt, gpu.data(), &errs ); } ) ) {
			fail( QStringLiteral( "the self-check dispatch failed: " ) + g.lastError );
			return;
		}
		qint64 t = 0;
		for ( int by = 0; by < bh; by++ )
			for ( int bx = 0; bx < bw; bx++ ) {
				const int b = by * bw + bx;
				uint8_t rgba[16][4];
				blockPixels( img.data(), sw, sh, bx, by, rgba );
				const qint64 e = decodedError( gpu.data() + size_t( b ) * 16, rgba, wt );
				blocks++;
				if ( e < 0 || e != errs[size_t( b )] )
					bad++;
				t += e < 0 ? 0 : e;
			}
		cpuTot += c;
		gpuTot += t;
		if ( t > c && over.isEmpty() )
			over = QString( "weights %1,%2,%3,%4: GPU error %5 > CPU %6" ).arg( wt[0] ).arg( wt[1] ).arg( wt[2] ).arg( wt[3] ).arg( t ).arg( c );
	}
	g.gpuMs = 0;
	g.blocksPerMs[0] = g.blocksPerMs[1] = g.blocksPerMs[2] = 0.0;
	if ( bad ) {
		fail( QString( "the BC7 self-check: %1 of %2 GPU blocks do not decode to their reported error" ).arg( bad ).arg( blocks ) );
		return;
	}
	if ( !over.isEmpty() ) {
		fail( QStringLiteral( "the BC7 self-check: " ) + over );
		return;
	}
	g.ready = true;
	g.ready1 = QString( "%1, OpenGL %2.%3; BC7 self-check %4 blocks, error GPU %5 <= CPU %6" )
		.arg( renderer ).arg( got.majorVersion() ).arg( got.minorVersion() ).arg( blocks ).arg( gpuTot ).arg( cpuTot );
}

void lodgenGpuConfigure( bool forceCpu )
{
	GpuState & g = G();
	if ( forceCpu ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- switch --no-gpu" );
		return;
	}
	if ( !lodgenGpuSettingEnabled() ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- setting Use GPU off (Settings > NIF > LOD bake)" );
		return;
	}
	if ( !g.tried )
		makeContext();
	if ( g.dead ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- the GPU failed earlier in this session: " ) + g.lastError;
	} else if ( !g.ready ) {
		g.on = false;
		g.report = QStringLiteral( "gpu: CPU path -- " ) + g.why;
	} else {
		g.on = true;
		g.report = QStringLiteral( "gpu: GPU path -- setting Use GPU on; " ) + g.ready1;
	}
}

bool lodgenGpuOn()
{
	return G().on;
}

QString lodgenGpuReport()
{
	return G().report;
}

QString lodgenGpuDigestWord()
{
	return G().on ? QStringLiteral( "|bc7gpu" ) : QString();
}

QString lodgenGpuSummary()
{
	GpuState & g = G();
	return QString( "gpu: bc7 %1 image(s), %2 block(s) on the GPU in %3 ms; %4 image(s) fell back to the CPU%5" )
		.arg( g.images.load() ).arg( g.blocks.load() ).arg( g.gpuMs.load() ).arg( g.fellBack.load() )
		.arg( g.lastError.isEmpty() ? QString() : QStringLiteral( " (last: " ) + g.lastError + QChar( ')' ) );
}

bool lodgenGpuEncodeBc7( const quint32 * px, int w, int h, const int wt[4], quint8 * out )
{
	GpuState & g = G();
	if ( !g.on || w <= 0 || h <= 0 )
		return false;
	for ( int c = 0; c < 4; c++ )
		if ( wt[c] > 500 )
			return false;     // the GPU error sum is an int; see the shader's header
	const int nb = ( ( w + 3 ) / 4 ) * ( ( h + 3 ) / 4 );
	if ( nb < kMinGpuBlocks )
		return false;
	QMutexLocker lock( &g.jobLock );
	if ( !g.on )
		return false;
	const bool ok = onGpuThread( [&]() { return gpuBc7( px, w, h, wt, out ); } );
	if ( ok ) {
		g.images++;
		g.blocks += quint64( nb );
	} else {
		/* One failure turns the GPU path off for the rest of the run: a lost
		 * context does not come back, and every later image goes straight to
		 * the CPU instead of paying for a failed dispatch first. */
		g.fellBack++;
		g.on = false;
		g.dead = true;
	}
	return ok;
}
