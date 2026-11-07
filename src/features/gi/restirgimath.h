#ifndef RESTIRGIMATH_H
#define RESTIRGIMATH_H

/* Lane RTEMIS1 part 2 (bungo 11-06: "we need restir GI"): ReSTIR GI on top of the traced probe volume, the rules in one
 * header (no Qt). restirgi::frame() is the CPU twin of the per-pixel pass; tests/features/gi/restirgi_math holds the unit
 * test (one red per rule) and the brute-force reference gate.
 *
 * The estimator (Ouyang et al. 2021, "ReSTIR GI: Path Resampling for Real-Time Path Tracing"; Bitterli et al. 2020):
 * each pixel (visible point x_v, normal n_v) traces ONE cosine-distributed bounce ray. Its hit x_s (normal n_s) sends
 * the outgoing radiance Lo, shaded exactly as a probe ray's hit (rtddgimath.h hitRadiance: albedo x (direct + the probe
 * volume's irradiance) / pi + the surface's emission, part 1), so the volume stays the energy past the first bounce; a
 * miss sends the volume's miss radiance (sky above, ground below). The sample is the point x_s (a direction for a miss).
 * Target p_hat = lum(Lo) x cos(theta_v) (the irradiance integrand), so the first candidate's weight is
 * p_hat / (cos / pi) = pi lum(Lo).
 *   temporal: the pixel's history reservoir (M clamped to kHistoryCap) is merged at its own p_hat; the result is the
 *             next frame's history.
 *   spatial:  K neighbours' temporal reservoirs are merged at THIS pixel's p_hat, with a shadow ray to their x_s (a
 *             neighbour's sample can be hidden from here) and the reconnection Jacobian
 *             J = (cos phi_v / cos phi_q) x (|x_q - x_s|^2 / |x_v - x_s|^2) (the change of solid angle at x_s);
 *             Z counts the M of every merged reservoir whose pixel sees the chosen x_s with p_hat > 0 (unbiased).
 *             A neighbour whose J is outside [1/10, 10] is not merged (jacobianKept).
 * The pixel's irradiance is Lo(y) cos(theta_v) W; the cell shader reads it in place of the volume's irradiance.
 *
 * Reds (compiled with -D, each must fail its own check in tests/features/gi/restirgi_math):
 *   RESTIRGI_RED_NOJACOBIAN  J = 1                                     (check jacobian)
 *   RESTIRGI_RED_NOVIS       a neighbour's sample is taken unseen      (check vis)
 *   RESTIRGI_RED_NOPDF       the first candidate's weight is p_hat     (check pdf)
 *   RESTIRGI_RED_NOTEMPORAL  no history                                (check temporal)
 * The runtime red bits (kRed*) are the same rules for the live pass and the CLI. */

#include "features/gi/rtddgimath.h"   // V3, kPi
#include "restirmath.h"               // hash, rand01, pixelSeed (one random stream rule for DI and GI)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace restirgi {

using rtddgi::V3;

constexpr int kRedNoJacobian = 1, kRedNoVis = 2, kRedNoPdf = 4, kRedNoTemporal = 8;
constexpr int kRedWhite = 16;          // not a red: the white-noise bounce (the one-frame noise pair's before)
constexpr int kRedR2 = 32;             // the integer R2 lattice bounce in place of the STBN mask (the mask's before)
constexpr int kRedNoJClamp = 64;       // every Jacobian merged as it is (jacobianKept's drop's before)
constexpr float kHistoryCap = 20.0f;   // [Ouyang 2021 sec. 4.3 / Bitterli 2020 sec. 6]
constexpr int kStbnSide = 128, kStbnSlices = 64;   // res/air/stbn_scalar_128x128x64.r8 (FOGGRID22A, ours)
constexpr float kEps = 1e-3f;          // ray offsets, in the scene's units

//! the runtime red bits with the compile-time reds of this build folded in (the unit test's red builds)
inline int foldRed( int red )
{
#ifdef RESTIRGI_RED_NOJACOBIAN
	red |= kRedNoJacobian;
#endif
#ifdef RESTIRGI_RED_NOVIS
	red |= kRedNoVis;
#endif
#ifdef RESTIRGI_RED_NOPDF
	red |= kRedNoPdf;
#endif
#ifdef RESTIRGI_RED_NOTEMPORAL
	red |= kRedNoTemporal;
#endif
	return red;
}

inline float lum( V3 c ) { return restir::luminance( c.x, c.y, c.z ); }

//! one bounce sample: the hit point and its normal (hit), or the direction (a miss), and the radiance it sends back
struct Sample
{
	V3 xs, ns, Lo;
	bool hit = false;
};

struct Res
{
	Sample y;
	float wsum = 0.0f, M = 0.0f, W = 0.0f;
	bool any = false;
};

//! a cosine-distributed direction about n (pdf cos / pi per steradian)
inline V3 cosineDir( V3 n, float u1, float u2 )
{
	const V3 a = std::fabs( n.x ) > 0.9f ? V3( 0, 1, 0 ) : V3( 1, 0, 0 );
	const V3 t = rtddgi::normalize( rtddgi::cross( a, n ) ), b = rtddgi::cross( n, t );
	const float r = std::sqrt( u1 ), ph = 2.0f * rtddgi::kPi * u2;
	const float z = std::sqrt( std::max( 0.0f, 1.0f - u1 ) );
	return rtddgi::normalize( t * ( r * std::cos( ph ) ) + b * ( r * std::sin( ph ) ) + n * z );
}

/*! one of the bounce's random pair: an R2 lattice dither a pixel (blue-noise-like in space) walked by the 2D R2 sequence a
 *  frame (low discrepancy in time) [Roberts 2018, "The unreasonable effectiveness of quasirandom sequences"]. Integer
 *  math, so the kernel's rgBlue matches bit for bit. Every pixel's own sequence is equidistributed: unbiased over frames. */
inline float blue( int x, int y, uint32_t f, uint32_t seed, int dim )
{
	const uint32_t a = dim == 0 ? 3242174889u : 2447445414u, b = dim == 0 ? 2447445414u : 3242174889u;
	const uint32_t v = uint32_t( x ) * a + uint32_t( y ) * b + f * a + restir::pixelSeed( seed, 0u, uint32_t( 7 + dim ) );
	return float( v >> 8u ) * ( 1.0f / 16777216.0f );
}

/*! the STBN mask's byte for pixel (x, y) at frame f: tiled, one slice a frame, the seed shifts the tile, the second value
 *  from a fixed spatial offset at the SAME slice (skill ww-blue-noise-mask: never offset the time index) */
inline uint32_t stbnIndex( int x, int y, uint32_t f, uint32_t seed, int dim )
{
	const uint32_t ox = ( seed & 127u ) + ( dim == 0 ? 0u : 37u ), oy = ( ( seed >> 7u ) & 127u ) + ( dim == 0 ? 0u : 71u );
	return ( ( uint32_t( x ) + ox ) & 127u ) + ( ( uint32_t( y ) + oy ) & 127u ) * 128u + ( f & 63u ) * 16384u;
}

inline float stbnValue( uint32_t v ) { return ( float( v ) + 0.5f ) * ( 1.0f / 256.0f ); }

/*! the bounce direction of pixel (x, y) at frame f: the STBN mask's pair (mask: kStbnSide^2 x kStbnSlices bytes; null or
 *  kRedR2: the integer R2 lattice, the mask's before; kRedWhite: the white-noise pair, both's before) */
inline V3 bounceDir( V3 n, int x, int y, int W, uint32_t seed, uint32_t f, int red, const uint8_t * mask = nullptr )
{
	if ( red & kRedWhite ) {
		const uint32_t s0 = restir::pixelSeed( seed ^ uint32_t( y * W + x ), f, 0 );
		return cosineDir( n, restir::rand01( s0, 0 ), restir::rand01( s0, 1 ) );
	}
	if ( mask && !( red & kRedR2 ) )
		return cosineDir( n, stbnValue( mask[stbnIndex( x, y, f, seed, 0 )] ), stbnValue( mask[stbnIndex( x, y, f, seed, 1 )] ) );
	return cosineDir( n, blue( x, y, f, seed, 0 ), blue( x, y, f, seed, 1 ) );
}

//! the direction from receiver x to the sample (a hit's point or a miss's direction)
inline V3 dirTo( V3 x, const Sample & y )
{
	return y.hit ? rtddgi::normalize( y.xs - x ) : y.xs;
}

//! p_hat at receiver (x, n), visibility not included: lum(Lo) x cos theta
inline float target( V3 x, V3 n, const Sample & y )
{
	const float c = rtddgi::dot( n, dirTo( x, y ) );
	return c > 0.0f ? lum( y.Lo ) * c : 0.0f;
}

//! the reconnection Jacobian from receiver xq's solid angle to receiver xv's (1 for a miss: a direction at infinity)
inline float jacobian( V3 xv, V3 xq, const Sample & y, int red )
{
	if ( !y.hit || ( red & kRedNoJacobian ) )
		return 1.0f;
	const V3 dv = xv - y.xs, dq = xq - y.xs;
	const float lv = rtddgi::dot( dv, dv ), lq = rtddgi::dot( dq, dq );
	if ( lv <= 0.0f || lq <= 0.0f )
		return 0.0f;
	const float cv = std::fabs( rtddgi::dot( y.ns, dv ) ) / std::sqrt( lv ), cq = std::fabs( rtddgi::dot( y.ns, dq ) ) / std::sqrt( lq );
	return cq > 1e-6f ? ( cv / cq ) * ( lq / lv ) : 0.0f;
}

/*! a neighbour's Jacobian as the merge uses it: outside [1/10, 10] the neighbour is dropped (-1: not merged, not in Z)
 *  [NVIDIA RTXDI, ReSTIR GI spatial resampling, RTXDI_ValidateGISampleWithJacobian's rejection]. A grazing reconnection
 *  (cos phi_q near 0: J up to 1e6) is otherwise a firefly the young history cannot average out. RTXDI also clamps J to
 *  [1/3, 3]; that clamp failed the unit's unbiased bars (vis chi 2.33, jacobian bias -0.028), the drop alone passes. */
inline float jacobianKept( float j, int red )
{
	if ( red & kRedNoJClamp )
		return j;
	return j >= 0.1f && j <= 10.0f ? j : -1.0f;
}

//! stream a candidate of weight w standing for m candidates (u in [0, 1))
inline void update( Res & r, const Sample & y, float w, float m, float u )
{
	r.M += m;
	if ( !( w > 0.0f ) )
		return;
	r.wsum += w;
	if ( u * r.wsum < w ) {
		r.y = y;
		r.any = true;
	}
}

//! W = wsum / (Z p_hat(y))
inline void finalize( Res & r, float pHat, float Z )
{
	r.W = r.any && pHat > 0.0f && Z > 0.0f ? r.wsum / ( Z * pHat ) : 0.0f;
}

//! the first candidate: the pixel's own bounce, drawn with pdf cos / pi
inline Res candidate( V3 x, V3 n, const Sample & y, int red )
{
	Res r;
	const float pHat = target( x, n, y );
	const float pdf = rtddgi::dot( n, dirTo( x, y ) ) / rtddgi::kPi;
	const float w = ( red & kRedNoPdf ) ? pHat : ( pdf > 0.0f ? pHat / pdf : 0.0f );
	update( r, y, w, 1.0f, 0.0f );
	finalize( r, pHat, r.M );
	return r;
}

//! the pixel's irradiance estimate: Lo(y) cos theta W
inline V3 estimate( V3 x, V3 n, const Res & r )
{
	if ( !r.any || !( r.W > 0.0f ) )
		return V3( 0, 0, 0 );
	const float c = std::max( 0.0f, rtddgi::dot( n, dirTo( x, r.y ) ) );
	return r.y.Lo * ( c * r.W );
}

struct Pixel
{
	V3 x, n;
	bool valid = false;
};

struct Settings
{
	int spatialK = 4;      // neighbours a frame
	int radius = 3;        // the neighbour square's half side, in pixels
	bool temporal = true;
	const uint8_t * mask = nullptr;   // the STBN mask (bounceDir); null: the R2 lattice
	std::vector<float> * jmax = nullptr;   // the CLI's diagnosis: each pixel's largest raw Jacobian over merged hits
};

/*! one frame of the per-pixel pass on a W x H grid of visible points. Scene: Sample trace(V3 o, V3 d) (the bounce ray,
 *  shaded) and bool blocked(V3 o, V3 d, float tmax) (a shadow ray; tmax < 0 = to infinity). hist holds the history
 *  reservoirs (any = false: none) and becomes this frame's temporal ones; out the irradiance a pixel. */
template<class Scene>
inline void frame( const Scene & scene, const std::vector<Pixel> & px, int W, int H, uint32_t seed, uint32_t f,
	const Settings & S, int red, std::vector<Res> & hist, std::vector<V3> & out )
{
	red = foldRed( red );
	const int n = W * H;
	std::vector<Res> tmp( static_cast<size_t>( n ) );
	hist.resize( static_cast<size_t>( n ) );
	out.assign( static_cast<size_t>( n ), V3( 0, 0, 0 ) );
	if ( S.jmax )
		S.jmax->assign( static_cast<size_t>( n ), 0.0f );
	auto sees = [&]( V3 x, V3 nn, const Sample & y ) -> bool {
		if ( red & kRedNoVis )
			return true;
		const V3 o = x + nn * kEps;
		if ( !y.hit )
			return !scene.blocked( o, y.xs, -1.0f );
		const V3 d = y.xs - o;
		const float l = rtddgi::length( d );
		return l > 2.0f * kEps && !scene.blocked( o, d * ( 1.0f / l ), l - 2.0f * kEps );
	};
	// pass 0: the bounce ray, then the history (same pixel, same point: J = 1, Z = M)
	for ( int i = 0; i < n; i++ ) {
		const Pixel & p = px[static_cast<size_t>( i )];
		if ( !p.valid ) {
			tmp[static_cast<size_t>( i )] = Res();
			continue;
		}
		const uint32_t s1 = restir::pixelSeed( seed ^ uint32_t( i ), f, 1 );
		const V3 d = bounceDir( p.n, i % W, i / W, W, seed, f, red, S.mask );
		const Sample y = scene.trace( p.x + p.n * kEps, d );
		Res r = candidate( p.x, p.n, y, red );
		const Res & h = hist[static_cast<size_t>( i )];
		if ( S.temporal && !( red & kRedNoTemporal ) && h.any ) {
			const float Mh = std::min( h.M, kHistoryCap );
			Res t;
			update( t, r.y, target( p.x, p.n, r.y ) * r.W * r.M, r.M, 0.0f );
			update( t, h.y, target( p.x, p.n, h.y ) * h.W * Mh, Mh, restir::rand01( s1, 0 ) );
			finalize( t, target( p.x, p.n, t.y ), t.M );
			r = t;
		}
		tmp[static_cast<size_t>( i )] = r;
	}
	hist = tmp;
	// pass 1: K neighbours' temporal reservoirs at this pixel's p_hat, seen, with the Jacobian; Z over the merged pixels
	for ( int i = 0; i < n; i++ ) {
		const Pixel & p = px[static_cast<size_t>( i )];
		if ( !p.valid )
			continue;
		const uint32_t s2 = restir::pixelSeed( seed ^ uint32_t( i ), f, 2 );
		const Res & own = tmp[static_cast<size_t>( i )];
		Res r;
		update( r, own.y, target( p.x, p.n, own.y ) * own.W * own.M, own.M, restir::rand01( s2, 0 ) );
		int from[16];
		int nf = 0;
		from[nf++] = i;
		const int x0 = i % W, y0 = i / W;
		for ( int k = 0; k < std::min( S.spatialK, 15 ); k++ ) {
			const int side = 2 * S.radius + 1;
			const int o = int( restir::rand01( s2, uint32_t( 1 + 3 * k ) ) * float( side * side ) );
			const int qx = x0 + o % side - S.radius, qy = y0 + o / side - S.radius;
			if ( qx < 0 || qy < 0 || qx >= W || qy >= H || ( qx == x0 && qy == y0 ) )
				continue;
			const int q = qy * W + qx;
			const Pixel & pq = px[static_cast<size_t>( q )];
			const Res & rq = tmp[static_cast<size_t>( q )];
			if ( !pq.valid )
				continue;
			const float jr = rq.any ? jacobian( p.x, pq.x, rq.y, red ) : 1.0f;
			const float jk = jacobianKept( jr, red );
			if ( S.jmax && rq.any && rq.y.hit )
				( *S.jmax )[static_cast<size_t>( i )] = std::max( ( *S.jmax )[static_cast<size_t>( i )], jr );
			if ( jk < 0.0f )
				continue;
			from[nf++] = q;
			float w = 0.0f;
			if ( rq.any && sees( p.x, p.n, rq.y ) )
				w = target( p.x, p.n, rq.y ) * rq.W * rq.M * jk;
			update( r, rq.y, w, rq.M, restir::rand01( s2, uint32_t( 2 + 3 * k ) ) );
		}
		float Z = 0.0f;
		for ( int k = 0; k < nf; k++ ) {
			const Pixel & ps = px[static_cast<size_t>( from[k] )];
			if ( r.any && target( ps.x, ps.n, r.y ) > 0.0f && sees( ps.x, ps.n, r.y ) )
				Z += tmp[static_cast<size_t>( from[k] )].M;
		}
		const float pHat = r.any && sees( p.x, p.n, r.y ) ? target( p.x, p.n, r.y ) : 0.0f;
		finalize( r, pHat, Z );
		out[static_cast<size_t>( i )] = estimate( p.x, p.n, r );
	}
}

} // namespace restirgi

#endif
