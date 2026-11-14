// HLSL source of features/far-lod/lodgen_bc7.glsl (compute shader), lane FO4MATCH1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: cs_6_0
// GL compiles the GLSL from text (src/features/far-lod/lodgengpu.cpp bc7Source()): its %TABLES% line is
// filled there from the vendored detex tables (lib/libfo76utils/src/bptc-tables.c); here they are written out
// (notes/fo4match1/scratch/bc7prep.py, the same text). Fusion Shaders (F4FS) has no host for it: nothing loads it.
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_MATDIAG4
#define WW_GLSL_MATDIAG4
float4x4 wwMat4Diag(float s) { return float4x4(s, 0.0, 0.0, 0.0, 0.0, s, 0.0, 0.0, 0.0, 0.0, s, 0.0, 0.0, 0.0, 0.0, s); }
#endif
#ifndef WW_GLSL_OUTERPRODUCT
#define WW_GLSL_OUTERPRODUCT
float2x2 ww_outerProduct(float2 c, float2 r) { return float2x2(c * r.x, c * r.y); }
float3x3 ww_outerProduct(float3 c, float3 r) { return float3x3(c * r.x, c * r.y, c * r.z); }
float4x4 ww_outerProduct(float4 c, float4 r) { return float4x4(c * r.x, c * r.y, c * r.z, c * r.w); }
#endif
static uint3 gl_GlobalInvocationID;
#define WW_NUMTHREADS numthreads(64, 1, 1)	// the entry: [WW_NUMTHREADS] void main(...)
#ifdef __spirv__
#define WW_BC7_BIND( b ) [[vk::binding( b, 0 )]]	// the GLSL's binding = b
#else
#define WW_BC7_BIND( b )
#endif
WW_BC7_BIND( 0 ) StructuredBuffer<uint> srcPx : register(t0, space0);
WW_BC7_BIND( 1 ) RWStructuredBuffer<uint4> dstBlk : register(u0, space0);
WW_BC7_BIND( 2 ) RWStructuredBuffer<int> dstErr : register(u1, space0);
WW_BC7_BIND( 3 ) StructuredBuffer<int> blkList : register(t1, space0);
WW_BC7_BIND( 31 ) cbuffer LooseUniforms : register(b31, space0)
{
	/* pass 0: mode 6 on every block; pass 1: modes 5 and 4 on blkList, the blocks pass 0 left with error; pass 2:
	 * the two-subset modes on the blocks pass 1 left with error. The single search, cut where it tests err > 0,
	 * so a warp's 32 threads run blocks that all still have work (the same bytes as one pass). */
	int uPass;
	int uW;
	int uH;
	int uBw;
	int uFirst;
	int uCount;
	int4 uWt;
};


// %TABLES%: the BC7 two-subset partitions (bit i = pixel i in subset 1) and their second anchors
static const uint P2SUB[64] = {52428u,34952u,61166u,60616u,51328u,65260u,65224u,60544u,51200u,65516u,65152u,59392u,65512u,65280u,65520u,61440u,63248u,142u,28928u,2254u,140u,29456u,12544u,36046u,2188u,12560u,26214u,13932u,6120u,4080u,29070u,14748u,43690u,61680u,23130u,13260u,15420u,21930u,38550u,42330u,29646u,5064u,12876u,15324u,27030u,49980u,39270u,1632u,626u,1252u,20032u,10016u,51510u,37740u,14790u,25500u,37686u,40134u,33150u,59160u,52464u,4044u,30532u,60962u};
static const int ANCHOR2[64] = {15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,2,8,2,2,8,8,15,2,8,2,2,8,8,2,2,15,15,6,8,2,8,15,15,2,8,2,2,2,15,15,6,6,2,6,8,15,15,2,2,15,15,15,15,15,2,2,15};


static const int INT_MAX = 0x7fffffff;
static const uint ALL = 0xFFFFu;

// the block's pixels, R G B A packed low to high: 16 words, not 16 vectors, so a dynamic index stays cheap
static uint SRCP[16];
static uint PXP[16];
int4 P( int i ) { uint p = PXP[i]; return int4( int( p & 255u ), int( ( p >> 8 ) & 255u ), int( ( p >> 16 ) & 255u ), int( p >> 24 ) ); }
static int4 WT;
static float4 SW;

static uint4 OUT;
static int POS;
static uint4 bestOut;
static int bestErr;

// the BC7 interpolation weights: round( 64 k / ( 2^ib - 1 ) ), the detex tables' values (the host checks)
// (64 k / n is never within 1/30 of a half, so the float rounding is exact)
int wOf( int ib, int k ) { return int( float( 64 * k ) / float( ( 1 << ib ) - 1 ) + 0.5 ); }
int expandBits( int v, int bits ) { v <<= ( 8 - bits ); return v | ( v >> bits ); }
bool inMask( uint m, int i ) { return ( ( m >> uint( i ) ) & 1u ) != 0u; }

struct Spec { int4 ch; int nc; int bits; int pbit; int ib; int4 on; };
struct Fit { int4 q[2]; int4 ep[2]; int2 p; uint2 idx; int err; };
#ifndef WW_MAKE_Spec
#define WW_MAKE_Spec
Spec ww_make_Spec(int4 ch, int nc, int bits, int pbit, int ib, int4 on) { Spec r; r.ch = ch; r.nc = nc; r.bits = bits; r.pbit = pbit; r.ib = ib; r.on = on; return r; }
#endif

Spec spec( int4 ch, int nc, int bits, int pbit, int ib )
{
	int4 on = (int4)( 0 );
	for ( int j = 0; j < nc; j++ ) on[ch[j]] = 1;
	return ww_make_Spec( ch, nc, bits, pbit, ib, on );
}

Fit fitNew()
{
	Fit f;
	f.q[0] = (int4)( 0 ); f.q[1] = (int4)( 0 );
	f.ep[0] = (int4)( 0 ); f.ep[1] = (int4)( 0 );
	f.p = (int2)( 0 );
	f.idx = (uint2)( 0u );
	f.err = INT_MAX;
	return f;
}

// the fitted index of PIXEL i (4 bits each)
int idxGet( uint2 v, int i ) { return int( ( v[i >> 3] >> uint( ( i & 7 ) * 4 ) ) & 15u ); }
uint2 idxSet( uint2 v, int i, int k )
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
int assignIdx( uint mask, Spec s, int4 ep0, int4 ep1, out uint2 idx )
{
	int np = 1 << s.ib;
	int4 wm = WT * s.on;
	int tot = 0;
	idx = (uint2)( 0u );
	for ( int i = 0; i < 16; i++ ) {
		if ( !inMask( mask, i ) )
			continue;
		int4 x = P( i );
		int best = INT_MAX, bk = 0;
		for ( int k = 0; k < np; k++ ) {
			// the palette entry, recomputed per pixel: ALU is cheaper than a spilled array
			int W = wOf( s.ib, k );
			int4 e = x - ( ( ( 64 - W ) * ep0 + W * ep1 + 32 ) >> 6 );
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
		if ( or( q < 0, q > maxq ) )
			continue;
		int v = p < 0 ? expandBits( q, bits ) : expandBits( ( q << 1 ) | p, bits + 1 );
		float d = abs( float( v ) - x );
		if ( d < bd ) { bd = d; best = q; }
	}
	return best;
}

void tryEndpoints( uint mask, Spec s, float4 e0, float4 e1, inout Fit best )
{
	int combos = s.pbit == 1 ? 4 : ( s.pbit == 2 ? 2 : 1 );
	for ( int cb = 0; cb < combos; cb++ ) {
		Fit f = fitNew();
		if ( s.pbit == 1 )
			f.p = int2( cb & 1, ( cb >> 1 ) & 1 );
		else if ( s.pbit == 2 )
			f.p = int2( cb, cb );
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
	float4 on = float4( s.on );
	float n = float( countbits( mask ) );
	float4 m = (float4)( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) )
			m += float4( P( i ) );
	m = m / n * on;
	float4 sw = lerp( (float4)( 1.0 ), SW, on );
	float4x4 cov = wwMat4Diag( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			float4 y = ( float4( P( i ) ) - m ) * sw * on;
			cov += ww_outerProduct( y, y );
		}
	int top = s.ch[0];
	for ( int j = 1; j < s.nc; j++ )
		if ( cov[s.ch[j]][s.ch[j]] > cov[top][top] )
			top = s.ch[j];
	float4 v = cov[top];
	float len = 0.0;
	for ( int it = 0; it < 12; it++ ) {
		float4 nv = mul(v, cov);
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
				float t = dot( ( float4( P( i ) ) - m ) * sw * on, v );
				tmin = min( tmin, t );
				tmax = max( tmax, t );
			}
	} else {
		v = (float4)( 0.0 );
	}
	float4 e0 = clamp( m + tmin * v / sw, 0.0, 255.0 ) * on;
	float4 e1 = clamp( m + tmax * v / sw, 0.0, 255.0 ) * on;
	Fit best = fitNew();
	// it == -1: the principal-axis endpoints; 0..2: the least-squares refinements
	for ( int it = -1; it < 3; it++ ) {
		if ( it >= 0 ) {
			if ( best.err <= 0 )
				break;
			float A = 0.0, B = 0.0, C = 0.0;
			float4 X0 = (float4)( 0.0 ), X1 = (float4)( 0.0 );
			for ( int i = 0; i < 16; i++ )
				if ( inMask( mask, i ) ) {
					float t = float( wOf( s.ib, idxGet( best.idx, i ) ) ) / 64.0;
					float u = 1.0 - t;
					A += u * u;
					B += u * t;
					C += t * t;
					X0 += u * float4( P( i ) );
					X1 += t * float4( P( i ) );
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
	for ( int passWw = 0; passWw < 3 && best.err > 0; passWw++ ) {
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
				if ( or( nq < 0, nq > maxq ) )
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
	Spec s = spec( int4( 0, 1, 2, 3 ), 4, 7, 1, 4 );
	Fit f = fitSubset( ALL, s );
	fixAnchor( f, ALL, 0, s );
	OUT = (uint4)( 0u ); POS = 0;
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
	SW = sqrt( float4( max( uWt, (int4)( 1 ) ) ) );
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
	Spec sc = spec( int4( 0, 1, 2, 0 ), 3, cbits, 0, cib );
	Spec sa = spec( int4( 3, 0, 0, 0 ), 1, abits, 0, aib );
	Fit fc = fitSubset( ALL, sc );
	fixAnchor( fc, ALL, 0, sc );
	Fit fa = fitSubset( ALL, sa );
	fixAnchor( fa, ALL, 0, sa );
	OUT = (uint4)( 0u ); POS = 0;
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
	uint2 firstIdx = swapSets ? fa.idx : fc.idx, secondIdx = swapSets ? fc.idx : fa.idx;
	int ib2 = mode == 4 ? 3 : 2;
	for ( int i = 0; i < 16; i++ )
		put( uint( idxGet( firstIdx, i ) ), i == 0 ? 1 : 2 );
	for ( int i = 0; i < 16; i++ )
		put( uint( idxGet( secondIdx, i ) ), i == 0 ? ib2 - 1 : ib2 );
	keep( fc.err + fa.err );
	for ( int i = 0; i < 16; i++ ) PXP[i] = SRCP[i];
	WT = uWt;
	SW = sqrt( float4( max( uWt, (int4)( 1 ) ) ) );
}

// a cheap unquantized estimate of a subset's fit error over channels 0..nc-1, for ranking partitions
float quickErr( uint mask, int nc, int levels )
{
	float n = float( countbits( mask ) );
	if ( n == 0.0 )
		return 0.0;
	float4 on = nc == 4 ? (float4)( 1.0 ) : float4( 1.0, 1.0, 1.0, 0.0 );
	float4 m = (float4)( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) )
			m += float4( P( i ) );
	m = m / n * on;
	float4 sw = SW * on;
	float4x4 cov = wwMat4Diag( 0.0 );
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			float4 y = ( float4( P( i ) ) - m ) * sw;
			cov += ww_outerProduct( y, y );
		}
	int top = 0;
	for ( int c = 1; c < nc; c++ )
		if ( cov[c][c] > cov[top][top] )
			top = c;
	float4 v = cov[top];
	for ( int it = 0; it < 6; it++ ) {
		float4 nv = mul(v, cov);
		float len = length( nv );
		if ( len < 1e-12 )
			return 0.0;
		v = nv / len;
	}
	float tmin = 1e30, tmax = -1e30;
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			float t = dot( ( float4( P( i ) ) - m ) * sw, v );
			tmin = min( tmin, t );
			tmax = max( tmax, t );
		}
	float stp = ( tmax - tmin ) / float( levels - 1 );
	float err = 0.0;
	for ( int i = 0; i < 16; i++ )
		if ( inMask( mask, i ) ) {
			float4 y = ( float4( P( i ) ) - m ) * sw;
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
			if ( or( parts[k] < 0, s < sc[k] ) )
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
	Spec s = spec( int4( 0, 1, 2, 3 ), mode == 7 ? 4 : 3, mode == 7 ? 5 : ( mode == 3 ? 7 : 6 ),
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
	OUT = (uint4)( 0u ); POS = 0;
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
		bool anchor = or( ( !in1 && i == 0 ), ( in1 && i == anchor1 ) );
		put( uint( idxGet( in1 ? f1.idx : f0.idx, i ) ), anchor ? s.ib - 1 : s.ib );
	}
	keep( err );
}

void comp_main()
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
	SW = sqrt( float4( max( uWt, (int4)( 1 ) ) ) );
	if ( uPass == 0 ) {
		bestErr = INT_MAX;
		bestOut = (uint4)( 0u );
		mode6();
#ifndef LODGEN_BC7_RED	// tests/hlslmatch/cullmatch.sh bc7: the red drops this write
		dstBlk[blk] = bestOut;
#endif
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

struct SPIRV_Cross_Input
{
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

[WW_NUMTHREADS]
void main(SPIRV_Cross_Input stage_input)
{
    gl_GlobalInvocationID = stage_input.gl_GlobalInvocationID;
    comp_main();
}
