#ifndef RESTIRMATH_H
#define RESTIRMATH_H

/* Lane RESTIRDI1 (2026-10-30): ReSTIR DI for the cell lights that cast shadows, the rules in one header (no Qt).
 * Each primitive names its GLSL twin in src/rtddgi/restir_common.glsl (the "MIRROR restirmath.h <name>" lines there);
 * change both or neither. restir::frame() below is the CPU twin of src/rtddgi/restir.comp's three passes (the GPU
 * runs the same steps on the same inputs with the same random streams), and tests/restir_math holds the unit test
 * (one red per rule) and the brute-force CPU reference gate.
 *
 * The estimator (Bitterli et al. 2020, "Spatiotemporal reservoir resampling"; Heitz et al. 2018, "Combining analytic
 * direct illumination and stochastic shadows"): each pixel keeps a reservoir of one shadow-casting light, chosen among
 * C uniform candidates from the shadow-flag lights by the target p_hat (the light's unshadowed Lambert irradiance
 * luminance, shape clip included), merged with its reprojected history (M clamped to 20 x C) and K neighbours (depth
 * within 10 %, normal within 25 degrees; Z counts only the reservoirs whose domain holds the light: unbiased). One
 * shadow ray goes to a point on the light's sphere. The pixel's shadowed / unshadowed estimates p_hat W V and p_hat W,
 * summed over a 5 x 5 edge-stopped window, give the ratio the cell shader multiplies the analytic sum of those lights
 * by (cell_lights.glsl cellShadowF under WW_CELL_RESTIR).
 *
 * Reds (each compiled with -D, each must fail its own check in tests/restir_math):
 *   RESTIR_RED_NOVIS       the shadowed term ignores the shadow ray           (check vis)
 *   RESTIR_RED_NOPDF       W = wsum / Z without 1 / p_hat(y)                  (check pdf)
 *   RESTIR_RED_NOTEMPORAL  no history                                         (check temporal)
 *   RESTIR_RED_NOCLAMP     the history's M is not clamped                     (check clamp)
 *   RESTIR_RED_NOSHAPE     p_hat ignores the hemisphere / box clip            (check shape)
 *   RESTIR_RED_HASH        the header's hash differs from the shader's        (check mirror)
 *   RESTIR_RED_VIEWPOS     the pixel's view point with the projection's offset sign flipped (check viewpos)
 * The runtime red bits (the GPU's and the twin's `red`, `-no-gui restir --red`) are kRedNoVis .. kRedNoHole. */

#include "rtddgimath.h"   // V3, lightRadial, spotCone

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace restir {

using rtddgi::V3;

constexpr int kRedNoVis = 1, kRedNoPdf = 2, kRedNoTemporal = 4, kRedNoClamp = 8, kRedNoHole = 16;   // restir_common.glsl RESTIR_RED_*
constexpr float kHistoryCap = 20.0f;    // [Bitterli 2020 sec. 6]
constexpr float kDepthTol = 0.1f;       // ours, inside Bitterli's 10 % / 25 deg rule
constexpr float kNormalTol = 0.906f;    // cos 25 deg
constexpr float kNone = 1e5f;           // a g-buffer depth at or past this is background (the GL pass clears to 1e6)
constexpr int kLightFloats = 32;        // src/rtddgi.cpp packLight: 8 vec4 a light
constexpr float kAccMax = 16.0f;        // restir_common.glsl RESTIR_ACC_MAX

//! the runtime red bits with the compile-time reds of this build folded in (the unit test's red builds)
inline int foldRed( int red )
{
#ifdef RESTIR_RED_NOVIS
	red |= kRedNoVis;
#endif
#ifdef RESTIR_RED_NOPDF
	red |= kRedNoPdf;
#endif
#ifdef RESTIR_RED_NOTEMPORAL
	red |= kRedNoTemporal;
#endif
#ifdef RESTIR_RED_NOCLAMP
	red |= kRedNoClamp;
#endif
	return red;
}

struct Res
{
	int y = -1;
	float wsum = 0.0f, M = 0.0f, W = 0.0f;
};

//! GLSL: restirMake
inline Res resMake( int y, float wsum, float M, float W )
{
	Res r;
	r.y = y;
	r.wsum = wsum;
	r.M = M;
	r.W = W;
	return r;
}

//! lowbias32.  GLSL: restirHash
inline uint32_t hash( uint32_t x )
{
	x ^= x >> 16;
#ifdef RESTIR_RED_HASH
	x *= 0x7feb352bu;   // red: one constant off the shader's
#else
	x *= 0x7feb352du;
#endif
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

//! number k of stream `seed`, [0, 1), 24 bits.  GLSL: restirRand
inline float rand01( uint32_t seed, uint32_t k )
{
	return float( hash( seed ^ hash( k * 0x9e3779b9u + 0x632be5abu ) ) >> 8 ) * ( 1.0f / 16777216.0f );
}

//! GLSL: restirSeed (pass 0 candidates, 1 temporal, 2 spatial, 3 shadow)
inline uint32_t pixelSeed( uint32_t pixel, uint32_t frame, uint32_t pass )
{
	return hash( ( pixel * 0x27d4eb2du ) ^ hash( frame * 4u + pass ) );
}

//! GLSL: restirLum
inline float luminance( float r, float g, float b ) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

//! the unshadowed Lambert irradiance luminance of packed light l (32 floats) at P, normal N.  GLSL: restirTarget
inline float target( const float * l, V3 P, V3 N )
{
	const V3 Lv( l[0] - P.x, l[1] - P.y, l[2] - P.z );
	const float d = rtddgi::length( Lv );
	if ( d >= l[3] )
		return 0.0f;
#ifndef RESTIR_RED_NOSHAPE
	const int shape = int( l[28] + 0.5f );
	if ( shape == 1 && ( P.x - l[0] ) * l[8] + ( P.y - l[1] ) * l[9] + ( P.z - l[2] ) * l[10] < 0.0f )
		return 0.0f;
	if ( shape == 2 ) {
		for ( int k = 0; k < 3; k++ ) {
			const float * b = &l[16 + k * 4];
			if ( std::fabs( b[0] * P.x + b[1] * P.y + b[2] * P.z + b[3] ) > 1.0f )
				return 0.0f;
		}
	}
#endif
	const float id = 1.0f / std::max( d, 0.001f );
	const float nl = ( N.x * Lv.x + N.y * Lv.y + N.z * Lv.z ) * id;
	if ( nl <= 0.0f )
		return 0.0f;
	float att = rtddgi::lightRadial( d, l[3], l[12], l[13], l[14] );
	if ( l[7] > 0.5f )
		att *= rtddgi::spotCone( -( Lv.x * l[8] + Lv.y * l[9] + Lv.z * l[10] ) * id, l[11], l[15] );
	return std::max( luminance( l[4], l[5], l[6] ) * att * nl, 0.0f );
}

//! stream a candidate of weight w standing for m candidates.  GLSL: restirUpdate
inline Res update( Res r, int y, float w, float m, float u )
{
	Res o = r;
	o.wsum = r.wsum + w;
	o.M = r.M + m;
	if ( w > 0.0f && u * o.wsum < w )
		o.y = y;
	return o;
}

//! W = wsum / (Z p_hat(y)).  GLSL: restirFinalize
inline Res finalize( Res r, float pHat, float Z, int red )
{
	Res o = r;
	if ( r.y < 0 || pHat <= 0.0f || Z <= 0.0f ) {
		o.W = 0.0f;
		return o;
	}
	o.W = ( red & kRedNoPdf ) ? r.wsum / Z : r.wsum / ( Z * pHat );
	return o;
}

//! GLSL: restirClampM
inline float clampHistory( float Mprev, float Mcur, int red )
{
	if ( red & kRedNoClamp )
		return Mprev;
	return std::min( Mprev, kHistoryCap * Mcur );
}

//! GLSL: restirSimilar
inline bool similar( float depthA, V3 nA, float depthB, V3 nB )
{
	if ( depthA <= 0.0f || depthB <= 0.0f || depthA >= kNone || depthB >= kNone )
		return false;
	if ( std::fabs( depthA - depthB ) > kDepthTol * depthA )
		return false;
	return nA.x * nB.x + nA.y * nB.y + nA.z * nB.z >= kNormalTol;
}

//! GLSL: restirSpherePoint
inline V3 spherePoint( V3 c, float rad, V3 P, float u1, float u2 )
{
	const float z = 1.0f - 2.0f * u1;
	const float s = std::sqrt( std::max( 1.0f - z * z, 0.0f ) );
	const float ph = 6.283185307179586f * u2;
	float dx = s * std::cos( ph ), dy = s * std::sin( ph ), dz = z;
	if ( dx * ( P.x - c.x ) + dy * ( P.y - c.y ) + dz * ( P.z - c.z ) < 0.0f ) {
		dx = -dx;
		dy = -dy;
		dz = -dz;
	}
	return V3( c.x + dx * rad, c.y + dy * rad, c.z + dz * rad );
}

//! GLSL: restirShadowed
inline float shadowed( float pHat, float W, bool visible, int red )
{
	if ( red & kRedNoVis )
		return pHat * W;
	return visible ? pHat * W : 0.0f;
}

//! GLSL: restirRatio
inline float ratio( float num, float den )
{
	if ( den <= 0.0f )
		return 1.0f;
	return std::min( std::max( num / den, 0.0f ), 1.0f );
}

//! GLSL: restirViewPos (proj = P00, P11, P20, P21)
inline V3 viewPos( float px, float py, float W, float H, float D, const float proj[4] )
{
	const float nx = ( px + 0.5f ) / W * 2.0f - 1.0f;
	const float ny = ( py + 0.5f ) / H * 2.0f - 1.0f;
#ifdef RESTIR_RED_VIEWPOS
	return V3( D * ( nx - proj[2] ) / proj[0], D * ( ny - proj[3] ) / proj[1], -D );
#else
	return V3( D * ( nx + proj[2] ) / proj[0], D * ( ny + proj[3] ) / proj[1], -D );
#endif
}

//! the two estimates' running means (num, den, frames).  GLSL: restirAccumulate
struct Acc
{
	float num = 0.0f, den = 0.0f, n = 0.0f;
};
inline Acc accumulate( Acc prev, float num, float den, bool valid, int red )
{
	Acc o;
	if ( !valid || ( red & kRedNoTemporal ) || prev.n < 0.5f ) {
		o.num = num;
		o.den = den;
		o.n = 1.0f;
		return o;
	}
	o.n = std::min( prev.n + 1.0f, kAccMax );
	const float a = 1.0f / o.n;
	o.num = prev.num + ( num - prev.num ) * a;
	o.den = prev.den + ( den - prev.den ) * a;
	return o;
}

// ------------------------------------------------------------------ the frame (the CPU twin of restir.comp)
//! a camera in the soup's local frame: view -> world rows (world_k = dot(rows[k].xyz, v) + rows[k].w), projection
struct Cam
{
	float rows[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
	float proj[4] = { 1, 1, 0, 0 };   // P00, P11, P20, P21
	V3 toWorld( V3 v ) const
	{
		return V3( rows[0] * v.x + rows[1] * v.y + rows[2] * v.z + rows[3], rows[4] * v.x + rows[5] * v.y + rows[6] * v.z + rows[7],
			rows[8] * v.x + rows[9] * v.y + rows[10] * v.z + rows[11] );
	}
	V3 dirToWorld( V3 v ) const
	{
		return V3( rows[0] * v.x + rows[1] * v.y + rows[2] * v.z, rows[4] * v.x + rows[5] * v.y + rows[6] * v.z,
			rows[8] * v.x + rows[9] * v.y + rows[10] * v.z );
	}
	//! world -> view (the rows' rotation is orthonormal: its transpose)
	V3 toView( V3 w ) const
	{
		const V3 t( w.x - rows[3], w.y - rows[7], w.z - rows[11] );
		return V3( rows[0] * t.x + rows[4] * t.y + rows[8] * t.z, rows[1] * t.x + rows[5] * t.y + rows[9] * t.z,
			rows[2] * t.x + rows[6] * t.y + rows[10] * t.z );
	}
};

struct Settings
{
	int candidates = 8;      // C: initial candidates a pixel a frame (Bitterli: 32 over all lights; ours draw from the shadow list)
	int spatial = 4;         // K neighbours
	float radius = 16.0f;    // pixels
	int filter = 2;          // the ratio's window: (2 f + 1)^2
	float bias = 0.5f;       // the shadow ray starts this far along the normal (trace.comp ddgiDirect's 0.5)
	float clear = 0.0f;      // and stops this short of the point on the light (the DDGI's fixtureClear)
};

struct FrameIn
{
	int W = 0, H = 0;
	const float * gbuf = nullptr;      // W x H x 4: view normal xyz, linear depth (>= kNone: none); bottom row first
	const float * prevGbuf = nullptr;  // last frame's (nullptr: no history)
	Cam cam, prev;
	const float * lights = nullptr;    // 32 floats a light (src/rtddgi.cpp packLight; [29] source radius, [30] shadow flag)
	int nLights = 0;
	const int * cand = nullptr;        // the candidate lights (the shadow-flag ones)
	int nCand = 0;
	uint32_t frame = 0;
	int red = 0;
	Settings set;
};

struct Buffers
{
	std::vector<Res> hist, cur;      // hist: last frame's output (pass 1), read by pass 0; cur: pass 0's
	std::vector<float> num, den, rat;
	std::vector<unsigned char> blocked;   // the shadow ray's answer a pixel (2: no ray)
	std::vector<int> histPix;             // pass 0: the history pixel it reused (-1 none)
	std::vector<Acc> acc, accPrev;        // pass 2: the running means; accPrev = last frame's
	void resize( size_t n )
	{
		if ( hist.size() != n )
			hist.assign( n, Res() );
		if ( accPrev.size() != n )
			accPrev.assign( n, Acc() );
		histPix.assign( n, -1 );
		acc.assign( n, Acc() );
		cur.assign( n, Res() );
		num.assign( n, 0.0f );
		den.assign( n, 0.0f );
		rat.assign( n, 1.0f );
		blocked.assign( n, 2 );
	}
};

struct Pixel
{
	bool ok = false;
	float D = 0.0f;
	V3 P, N, Nv;
};

inline Pixel pixelAt( const float * gbuf, int W, int H, const Cam & cam, int x, int y )
{
	Pixel o;
	const float * g = gbuf + ( size_t( y ) * size_t( W ) + size_t( x ) ) * 4;
	o.D = g[3];
	if ( !( o.D > 0.0f ) || o.D >= kNone )
		return o;
	o.Nv = V3( g[0], g[1], g[2] );
	o.P = cam.toWorld( viewPos( float( x ), float( y ), float( W ), float( H ), o.D, cam.proj ) );
	o.N = rtddgi::normalize( cam.dirToWorld( o.Nv ) );
	o.ok = true;
	return o;
}

inline float targetOf( const FrameIn & F, int y, const Pixel & p )
{
	return ( y >= 0 && y < F.nLights && p.ok ) ? target( F.lights + size_t( y ) * kLightFloats, p.P, p.N ) : 0.0f;
}

//! pass 0 of pixel (x, y): C candidates, then the reprojected history.  GLSL: restir.comp mode 0
inline Res passCandidates( const FrameIn & F, const Buffers & B, int x, int y, int red, int * hq )
{
	*hq = -1;
	const Pixel p = pixelAt( F.gbuf, F.W, F.H, F.cam, x, y );
	if ( !p.ok || F.nCand <= 0 )
		return Res();
	const uint32_t id = uint32_t( y * F.W + x );
	const uint32_t s0 = pixelSeed( id, F.frame, 0 );
	Res r;
	for ( int k = 0; k < F.set.candidates; k++ ) {
		const int j = std::min( int( rand01( s0, uint32_t( 2 * k ) ) * float( F.nCand ) ), F.nCand - 1 );
		const int l = F.cand[j];
		const float w = targetOf( F, l, p ) * float( F.nCand );
		r = update( r, l, w, 1.0f, rand01( s0, uint32_t( 2 * k + 1 ) ) );
	}
	r = finalize( r, targetOf( F, r.y, p ), r.M, red );
	if ( ( red & kRedNoTemporal ) || !F.prevGbuf )
		return r;
	const V3 pv = F.prev.toView( p.P );
	if ( !( pv.z < -1e-3f ) )
		return r;
	const float Dp = -pv.z;
	const float nx = ( F.prev.proj[0] * pv.x - F.prev.proj[2] * Dp ) / Dp, ny = ( F.prev.proj[1] * pv.y - F.prev.proj[3] * Dp ) / Dp;
	const int qx = int( std::floor( ( nx + 1.0f ) * 0.5f * float( F.W ) ) ), qy = int( std::floor( ( ny + 1.0f ) * 0.5f * float( F.H ) ) );
	if ( qx < 0 || qy < 0 || qx >= F.W || qy >= F.H )
		return r;
	const Pixel q = pixelAt( F.prevGbuf, F.W, F.H, F.prev, qx, qy );
	if ( !q.ok || !similar( Dp, p.N, q.D, q.N ) )
		return r;
	*hq = qy * F.W + qx;
	Res h = B.hist[size_t( qy ) * size_t( F.W ) + size_t( qx )];
	h.M = clampHistory( h.M, float( F.set.candidates ), red );
	const uint32_t s1 = pixelSeed( id, F.frame, 1 );
	Res s;
	s = update( s, r.y, targetOf( F, r.y, p ) * r.W * r.M, r.M, rand01( s1, 0 ) );
	s = update( s, h.y, targetOf( F, h.y, p ) * h.W * h.M, h.M, rand01( s1, 1 ) );
	float Z = 0.0f;
	if ( targetOf( F, s.y, p ) > 0.0f )
		Z += r.M;
	if ( targetOf( F, s.y, q ) > 0.0f )
		Z += h.M;
	return finalize( s, targetOf( F, s.y, p ), Z, red );
}

//! pass 1 of pixel (x, y): K neighbours, then the shadow ray. vis(o, d, tmax) is true when the segment is blocked.
//! GLSL: restir.comp mode 1
template<class Vis>
inline Res passSpatial( const FrameIn & F, const Buffers & B, int x, int y, int red, Vis & vis, float * num, float * den, unsigned char * blk )
{
	*num = *den = 0.0f;
	*blk = 2;
	const Pixel p = pixelAt( F.gbuf, F.W, F.H, F.cam, x, y );
	if ( !p.ok )
		return Res();
	const uint32_t id = uint32_t( y * F.W + x );
	const uint32_t s2 = pixelSeed( id, F.frame, 2 );
	const Res own = B.cur[size_t( id )];
	Res s;
	s = update( s, own.y, targetOf( F, own.y, p ) * own.W * own.M, own.M, rand01( s2, 0 ) );
	int qs[16];
	int nq = 0;
	for ( int k = 0; k < F.set.spatial && k < 16; k++ ) {
		const float a = 6.283185307179586f * rand01( s2, uint32_t( 1 + 3 * k ) );
		const float rr = F.set.radius * std::sqrt( rand01( s2, uint32_t( 2 + 3 * k ) ) );
		const int qx = x + int( std::floor( rr * std::cos( a ) + 0.5f ) ), qy = y + int( std::floor( rr * std::sin( a ) + 0.5f ) );
		if ( ( qx == x && qy == y ) || qx < 0 || qy < 0 || qx >= F.W || qy >= F.H )
			continue;
		const Pixel q = pixelAt( F.gbuf, F.W, F.H, F.cam, qx, qy );
		if ( !q.ok || !similar( p.D, p.Nv, q.D, q.Nv ) )
			continue;
		const Res n = B.cur[size_t( qy ) * size_t( F.W ) + size_t( qx )];
		s = update( s, n.y, targetOf( F, n.y, p ) * n.W * n.M, n.M, rand01( s2, uint32_t( 3 + 3 * k ) ) );
		qs[nq++] = qy * F.W + qx;
	}
	float Z = targetOf( F, s.y, p ) > 0.0f ? own.M : 0.0f;
	for ( int k = 0; k < nq; k++ ) {
		const Pixel q = pixelAt( F.gbuf, F.W, F.H, F.cam, qs[k] % F.W, qs[k] / F.W );
		if ( targetOf( F, s.y, q ) > 0.0f )
			Z += B.cur[size_t( qs[k] )].M;
	}
	s = finalize( s, targetOf( F, s.y, p ), Z, red );
	if ( s.y < 0 || !( s.W > 0.0f ) )
		return s;
	const float * l = F.lights + size_t( s.y ) * kLightFloats;
	const uint32_t s3 = pixelSeed( id, F.frame, 3 );
	const V3 X = spherePoint( V3( l[0], l[1], l[2] ), l[29], p.P, rand01( s3, 0 ), rand01( s3, 1 ) );
	const V3 o = p.P + p.N * F.set.bias;
	const V3 dl = X - o;
	const float len = rtddgi::length( dl );
	const bool blocked = len > 1e-6f && vis( o, dl * ( 1.0f / len ), len - F.set.clear );
	*blk = blocked ? 1 : 0;
	const float ph = targetOf( F, s.y, p );
	*num = shadowed( ph, s.W, !blocked, red );
	*den = ph * s.W;
	return s;
}

//! pass 3 of pixel (x, y): the ratio of the running means over the edge-stopped window.  GLSL: restir.comp mode 3
inline float passRatio( const FrameIn & F, const Buffers & B, int x, int y )
{
	const float * g = F.gbuf + ( size_t( y ) * size_t( F.W ) + size_t( x ) ) * 4;
	const V3 nv( g[0], g[1], g[2] );
	float n = 0.0f, d = 0.0f;
	for ( int dy = -F.set.filter; dy <= F.set.filter; dy++ )
		for ( int dx = -F.set.filter; dx <= F.set.filter; dx++ ) {
			const int qx = x + dx, qy = y + dy;
			if ( qx < 0 || qy < 0 || qx >= F.W || qy >= F.H )
				continue;
			const float * h = F.gbuf + ( size_t( qy ) * size_t( F.W ) + size_t( qx ) ) * 4;
			if ( ( dx || dy ) && !similar( g[3], nv, h[3], V3( h[0], h[1], h[2] ) ) )
				continue;
			n += B.acc[size_t( qy ) * size_t( F.W ) + size_t( qx )].num;
			d += B.acc[size_t( qy ) * size_t( F.W ) + size_t( qx )].den;
		}
	return ratio( n, d );
}

/*! one frame: pass 0 into B.cur (+ histPix), pass 1 into B.hist (the next frame's history) + num / den / blocked, pass 2 the running
 *  means into B.acc (then B.accPrev), pass 3 the ratio into B.rat.
 *  The caller keeps the previous g-buffer and camera (F.prevGbuf, F.prev) */
template<class Vis>
inline void frame( const FrameIn & F, Buffers & B, Vis vis )
{
	const size_t n = size_t( F.W ) * size_t( F.H );
	B.resize( n );
	const int red = foldRed( F.red );
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ )
			B.cur[size_t( y ) * size_t( F.W ) + size_t( x )] = passCandidates( F, B, x, y, red, &B.histPix[size_t( y ) * size_t( F.W ) + size_t( x )] );
	std::vector<Res> next( n );
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ ) {
			const size_t i = size_t( y ) * size_t( F.W ) + size_t( x );
			next[i] = passSpatial( F, B, x, y, red, vis, &B.num[i], &B.den[i], &B.blocked[i] );
		}
	B.hist.swap( next );
	for ( size_t i = 0; i < n; i++ ) {   // pass 2 (GLSL: restir.comp mode 2)
		const int hp = B.histPix[i];
		B.acc[i] = accumulate( hp >= 0 ? B.accPrev[size_t( hp )] : Acc(), B.num[i], B.den[i], hp >= 0, red );
	}
	for ( int y = 0; y < F.H; y++ )   // pass 3 (mode 3)
		for ( int x = 0; x < F.W; x++ )
			B.rat[size_t( y ) * size_t( F.W ) + size_t( x )] = passRatio( F, B, x, y );
	B.accPrev.swap( B.acc );
}

} // namespace restir

#endif // RESTIRMATH_H
