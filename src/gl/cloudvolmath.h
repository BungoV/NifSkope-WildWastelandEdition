#ifndef WW_CLOUDVOLMATH_H
#define WW_CLOUDVOLMATH_H

/* lane CLOUD2: the volume clouds' CPU side, no GL and no Qt (the offline gates build it with g++). The density here is
 * the twin of res/shaders/cloud_common.glsl cloudDensity: keep the two in step. Sources:
 * - the density shape: Schneider & Vos 2015 (Horizon Zero Dawn), height gradient x coverage remap x detail erosion;
 * - the coverage calibration: ours (the drawn volume's mean column opacity = the cloud map's coverage);
 * - the droplet phase: Jendersie & d'Eon 2023, "An Approximate Mie Scattering Function for Fog and Cloud Rendering";
 * - the octaves: Wrenninge et al. 2013, "Oz: The Great and Volumetric" (a^i, b^i, c^i, a <= b). */

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace WwCloudVolMath
{

constexpr float kPi = 3.14159265f;
constexpr float kShapeKm = 6.0f;	// the shape volume's period: 128 texels of 47 m
constexpr float kDetailKm = 0.6f;	// the detail volume's period: 32 texels of 19 m
constexpr float kSigma = 40.0f;	// extinction /km at density 1: a 25 m mean free path (a plain cumulus)
constexpr float kAlbedo = 0.99f;	// single-scattering albedo of the droplets (visible light)
constexpr int kCalN = 17;	// coverage -> threshold table, per band (Low, Mid)
constexpr int kVolBands = 2;	// the volume draws Low and Mid; High is a thin sheet at its base
constexpr float kDropletUm = 20.0f;	// the droplet diameter of a plain cumulus, micrometres

inline float sat( float x )
{
	return std::clamp( x, 0.0f, 1.0f );
}

inline float smooth( float a, float b, float x )
{
	const float t = sat( ( x - a ) / ( b - a ) );
	return t * t * ( 3.0f - 2.0f * t );
}

inline float mixf( float a, float b, float t )
{
	return a + ( b - a ) * t;
}

//! the band's vertical profile: Low = cumulus mixed toward a low sheet by its stratus share, Mid = altocumulus
inline float gradient( int band, float hf, float sheet )
{
	if ( band == 0 ) {
		const float cu = smooth( 0.0f, 0.07f, hf ) * ( 1.0f - smooth( 0.55f, 1.0f, hf ) );
		const float st = smooth( 0.0f, 0.04f, hf ) * ( 1.0f - smooth( 0.12f, 0.3f, hf ) );
		return mixf( cu, st, sat( sheet ) );
	}
	return smooth( 0.0f, 0.15f, hf ) * ( 1.0f - smooth( 0.35f, 0.9f, hf ) );
}

//! trilinear read of an n^3 R8 volume with GL_REPEAT, texel centres at (i + 0.5) / n, as the GPU samples it
inline float sample3( const std::uint8_t * v, int n, float u, float w, float t )
{
	const float fx = u * float( n ) - 0.5f, fy = w * float( n ) - 0.5f, fz = t * float( n ) - 0.5f;
	const float x0 = std::floor( fx ), y0 = std::floor( fy ), z0 = std::floor( fz );
	const float ax = fx - x0, ay = fy - y0, az = fz - z0;
	auto at = [&]( float x, float y, float z ) {
		auto wr = [n]( float c ) {
			int i = int( c ) % n;
			return i < 0 ? i + n : i;
		};
		return float( v[( size_t( wr( z ) ) * size_t( n ) + size_t( wr( y ) ) ) * size_t( n ) + size_t( wr( x ) )] ) / 255.0f;
	};
	float c[2][2][2];
	for ( int k = 0; k < 8; k++ )
		c[k >> 2][( k >> 1 ) & 1][k & 1] = at( x0 + float( k & 1 ), y0 + float( ( k >> 1 ) & 1 ), z0 + float( k >> 2 ) );
	float r[2];
	for ( int z = 0; z < 2; z++ ) {
		const float a = mixf( c[z][0][0], c[z][0][1], ax ), b = mixf( c[z][1][0], c[z][1][1], ax );
		r[z] = mixf( a, b, ay );
	}
	return mixf( r[0], r[1], az );
}

/*! one band's density at a point (relative: extinction = kSigma x this x the band's density), given the shape and
 *  detail reads s, det, the height fraction hf in the band, the calibrated coverage c and the sheet share */
inline float bandDensity( float s, float det, int band, float hf, float c, float sheet )
{
	if ( hf <= 0.0f || hf >= 1.0f || c <= 0.0f )
		return 0.0f;
	const float t = 1.0f - c;
	const float base = sat( ( s * gradient( band, hf, sheet ) - t ) / std::max( 1.0f - t, 1e-4f ) ) * c;
	if ( base <= 0.0f )
		return 0.0f;
	const float mod = mixf( det, 1.0f - det, sat( hf * 4.0f ) ) * 0.2f;
	return sat( ( base - mod ) / ( 1.0f - mod ) );
}

//! the coverage table read: c in 0..1 -> the threshold coverage the density takes (piecewise linear, 17 entries)
inline float calAt( const float cal[kCalN], float c )
{
	const float t = sat( c ) * float( kCalN - 1 );
	const int i = std::min( int( t ), kCalN - 2 );
	return mixf( cal[i], cal[i + 1], t - float( i ) );
}

/*! the mean column opacity of a band at the threshold coverage cp, over 12 x 12 columns of one shape period
 *  (40 midpoint steps through the band): what a map texel of that coverage looks like from straight above */
inline float meanOpacity( const std::uint8_t * shape, int ns, const std::uint8_t * detail, int nd, int band, float baseKm,
	float topKm, float density, float sheet, float cp )
{
	constexpr int NC = 12, NZ = 40;
	const float th = topKm - baseKm;
	double acc = 0.0;
	for ( int j = 0; j < NC; j++ )
		for ( int i = 0; i < NC; i++ ) {
			const float x = ( float( i ) + 0.5f ) / NC * kShapeKm, y = ( float( j ) + 0.37f ) / NC * kShapeKm;
			float tau = 0.0f;
			for ( int k = 0; k < NZ; k++ ) {
				const float hf = ( float( k ) + 0.5f ) / NZ, z = baseKm + hf * th;
				const float s = sample3( shape, ns, x / kShapeKm, y / kShapeKm, z / kShapeKm );
				const float d = sample3( detail, nd, x / kDetailKm, y / kDetailKm, z / kDetailKm );
				tau += bandDensity( s, d, band, hf, cp, sheet ) * kSigma * density * th / NZ;
			}
			acc += 1.0 - std::exp( -double( tau ) );
		}
	return float( acc / ( NC * NC ) );
}

/*! the coverage table of one band: cal[i] = the threshold coverage whose mean column opacity is i / 16. Red "nocal"
 *  (identity: the map's coverage fed straight to the threshold) must fail the coverage gate. */
inline void calibrate( const std::uint8_t * shape, int ns, const std::uint8_t * detail, int nd, int band, float baseKm,
	float topKm, float density, float sheet, bool redNoCal, float cal[kCalN] )
{
	for ( int i = 0; i < kCalN; i++ )
		cal[i] = float( i ) / float( kCalN - 1 );
	if ( redNoCal || topKm - baseKm <= 1e-3f || density <= 0.0f )
		return;
	constexpr int NG = 33;
	float o[NG];
	o[0] = 0.0f;
	for ( int j = 1; j < NG; j++ )
		o[j] = std::max( o[j - 1], meanOpacity( shape, ns, detail, nd, band, baseKm, topKm, density, sheet, float( j ) / ( NG - 1 ) ) );
	for ( int i = 1; i < kCalN; i++ ) {
		const float want = float( i ) / float( kCalN - 1 );
		float cp = 1.0f;
		for ( int j = 1; j < NG; j++ )
			if ( o[j] >= want ) {
				const float a = o[j - 1], b = o[j];
				const float f = b > a ? ( want - a ) / ( b - a ) : 1.0f;
				cp = ( float( j - 1 ) + f ) / ( NG - 1 );
				break;
			}
		cal[i] = std::max( cal[i - 1], cp );
	}
}

//! Jendersie & d'Eon 2023 fits for droplet diameter d (micrometres, 5..50): x = gHG, y = gD, z = alpha, w = wD
struct Droplet
{
	float gHg = 0.0f, gD = 0.0f, alpha = 0.0f, wD = 0.0f;
};

inline Droplet dropletParams( float d )
{
	Droplet p;
	p.gHg = std::exp( -0.0990567f / ( d - 1.67154f ) );
	p.gD = std::exp( -2.20679f / ( d + 3.91029f ) - 0.428934f );
	p.alpha = std::exp( 3.62489f - 8.29288f / ( d + 5.52825f ) );
	p.wD = std::exp( -0.599085f / ( d - 0.641583f ) - 0.665888f );
	return p;
}

inline float hg( float g, float c )
{
	return ( 1.0f - g * g ) / ( 4.0f * kPi * std::pow( std::max( 1e-4f, 1.0f + g * g - 2.0f * g * c ), 1.5f ) );
}

inline float draine( float g, float a, float c )
{
	return hg( g, c ) * ( 1.0f + a * c * c ) / ( 1.0f + a * ( 1.0f + 2.0f * g * g ) / 3.0f );
}

//! the droplet phase (1/sr) at cos theta c, both lobes' g scaled by k (the octaves' c^i); twin of wwDropletPhase
inline float dropletPhase( const Droplet & p, float c, float k )
{
	return ( 1.0f - p.wD ) * hg( p.gHg * k, c ) + p.wD * draine( p.gD * k, p.alpha, c );
}

/*! Wrenninge 2013 octaves: sum_i a^i exp(-b^i tauL) p(c, k^i), a = b = k = 1/2 (a <= b keeps energy). n = 1 is single
 *  scattering (the red "nooctaves"). Twin of cloud_march.frag cvOctaves. */
inline float octaves( const Droplet & p, float cosT, float tauL, int n )
{
	float s = 0.0f, a = 1.0f, b = 1.0f, k = 1.0f;
	for ( int i = 0; i < n; i++ ) {
		s += a * std::exp( -b * tauL ) * dropletPhase( p, cosT, k );
		a *= 0.5f;
		b *= 0.5f;
		k *= 0.5f;
	}
	return s;
}

/*! lane CLOUD2B: the droplet phase's mean cosine, analytic: HG's is g, Draine's g (1 + a (3 + 2 g^2) / 5) /
 *  (1 + a (1 + 2 g^2) / 3) (Draine 2003, eq. 6); blended by wD as the phase (0.879 at 20 um). */
inline float msMeanCosine( const Droplet & p )
{
	const float g = p.gD, a = p.alpha;
	const float gDr = g * ( 1.0f + a * ( 3.0f + 2.0f * g * g ) / 5.0f ) / ( 1.0f + a * ( 1.0f + 2.0f * g * g ) / 3.0f );
	return ( 1.0f - p.wD ) * p.gHg + p.wD * gDr;
}

/*! lane CLOUD2B: the diffuse transmission of a slab of optical depth tau, per unit flux on its top, sun at mu0:
 *  delta-Eddington scaling (Joseph, Wiscombe & Weinman 1976) + the two-stream solution for albedo w < 1 (Meador &
 *  Weaver 1980, Eddington coefficients), the direct e^(-tau'/mu0) taken out. Twin of cloud_march.frag cvTdif. */
inline float msDiffuseT( float tau, float mu0, float g, float w )
{
	const float f = g * g;
	const float wp = ( 1.0f - f ) * w / ( 1.0f - f * w );
	const float tp = ( 1.0f - f * w ) * tau;
	const float gp = g / ( 1.0f + g );
	const float g1 = ( 7.0f - wp * ( 4.0f + 3.0f * gp ) ) * 0.25f;
	const float g2 = -( 1.0f - wp * ( 4.0f - 3.0f * gp ) ) * 0.25f;
	const float g3 = ( 2.0f - 3.0f * gp * mu0 ) * 0.25f;
	const float g4 = 1.0f - g3;
	const float a1 = g1 * g4 + g2 * g3;
	const float k = std::sqrt( std::max( g1 * g1 - g2 * g2, 1e-8f ) );
	if ( std::fabs( 1.0f - k * mu0 ) < 1e-3f )
		mu0 += 2e-3f;
	const float em = std::exp( -k * tp ), e0 = std::exp( -tp / mu0 );
	const float den = ( 1.0f - k * k * mu0 * mu0 ) * ( ( k + g1 ) + ( k - g1 ) * em * em );
	const float num = ( 1.0f + k * mu0 ) * ( a1 + k * g4 ) * e0 - ( 1.0f - k * mu0 ) * ( a1 - k * g4 ) * em * em * e0
		- 2.0f * k * ( g4 + a1 * mu0 ) * em;
	return std::max( -wp * num / den, 0.0f );
}

/*! the Low preset: the 2D layers' colour from the model, in the sky's scene units (displayed = this x the layer
 *  texture). The key light through the air to the cloud base (T at the base toward the light) plus the sky's mean
 *  radiance E / pi (E = the sky irradiance per unit sun, atmomath skyIrradiance), times the cloud's albedo. */
inline void lowColour( float keyK, const float T[3], float mu, float skyK, const float E[3], float out[3] )
{
	const float sunTerm = std::max( mu + 0.1f, 0.0f ) / 1.1f;
	for ( int c = 0; c < 3; c++ )
		out[c] = kAlbedo * ( keyK * T[c] * sunTerm + skyK * E[c] ) / kPi;
}

} // namespace WwCloudVolMath

#endif
