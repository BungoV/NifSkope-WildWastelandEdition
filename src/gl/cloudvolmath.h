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
#include <vector>

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

/*! lane CLOUD2C: the same slab's reflection of a beam at mu0 (Meador & Weaver 1980 eq. 14, alpha2 = g1 g3 + g2 g4),
 *  delta-Eddington scaled, written with e^(-k tau) only. Twin of cloud_march.frag cvRdir. */
inline float msDiffuseR( float tau, float mu0, float g, float w )
{
	const float f = g * g;
	const float wp = ( 1.0f - f ) * w / ( 1.0f - f * w );
	const float tp = ( 1.0f - f * w ) * tau;
	const float gp = g / ( 1.0f + g );
	const float g1 = ( 7.0f - wp * ( 4.0f + 3.0f * gp ) ) * 0.25f;
	const float g2 = -( 1.0f - wp * ( 4.0f - 3.0f * gp ) ) * 0.25f;
	const float g3 = ( 2.0f - 3.0f * gp * mu0 ) * 0.25f;
	const float g4 = 1.0f - g3;
	const float a2 = g1 * g3 + g2 * g4;
	const float k = std::sqrt( std::max( g1 * g1 - g2 * g2, 1e-8f ) );
	if ( std::fabs( 1.0f - k * mu0 ) < 1e-3f )
		mu0 += 2e-3f;
	const float em = std::exp( -k * tp ), e0 = std::exp( -tp / mu0 );
	const float den = ( 1.0f - k * k * mu0 * mu0 ) * ( ( k + g1 ) + ( k - g1 ) * em * em );
	const float num = ( 1.0f - k * mu0 ) * ( a2 + k * g3 ) - ( 1.0f + k * mu0 ) * ( a2 - k * g3 ) * em * em
		- 2.0f * k * ( g3 - a2 * mu0 ) * e0 * em;
	return std::clamp( wp * num / den, 0.0f, 1.0f );
}

/*! lane CLOUD2C: the noise's drift (8 m/s along +x) on the 2D layers' clock, wrapped to 64 shape periods. */
constexpr float kWindKmS = 0.008f;
inline float windKm( double secs )
{
	return float( std::fmod( secs * double( kWindKmS ), double( kShapeKm ) * 64.0 ) );
}

/*! lane CLOUD2C: the offset wwCloudVolBindDensity hands a reader: the one the last draw used (its clock), so a cloud
 *  shadow moves with the drawn clouds. red = the old bind (0: shadows stood still while the clouds drifted). */
/* lane CLOUDBAND1: Cloud Accumulation's weight of a pixel's fresh march (cloud_resolve_a.frag cvAccAlpha) in refresh
 * cycle `cycles` since its history began (a cycle = the 16 frames of the stride-4 phase order): 1, 1/2, 1/3, then
 * 1/kAccumN (a plain mean of the first kAccumN marches of a pixel, then an exponential one of about the last kAccumN).
 * A fixed 1/4 from the first frame kept 0.75^n of the first frame's upsampled march (a startup pattern, cloudband_check). */
constexpr int kAccumN = 4;
inline float accumAlpha( long long cycles )
{
	return 1.0f / float( std::min( std::max( cycles, 0LL ) + 1, static_cast<long long>( kAccumN ) ) );
}

/* lane CLOUD6g: Change Clamp's threshold (cloud_resolve_a.frag cvAccReset, cvAccResetSd): the mean transmittance of the
 * 9 fresh march texels against the history at their places; a difference past te = kChangeReset + kChangeResetSd x
 * min( deviation of the fresh 9, of the history 9 ) / 3 starts the pixel's weight rising, at 2 te it is 1 (the
 * accumulation restarts from the fresh march). Tuned offline (tests/spells/cloudstale_check.py, notes fix1030c/cloud6g):
 * a fixed 0.0125 held the Off sky's bars but reset on the Variety sky's noise (CLOUDBAND1 gate M 0.87, bar 0.6); a
 * threshold on the fresh deviation alone missed the 3 km shift (5.65 levels, bar 5.50). The red (0) = CLOUDBAND1's clamp. */
constexpr float kChangeReset = 0.0075f;
constexpr float kChangeResetSd = 1.5f;

/* lane CLOUD6h: History Age (cloud_resolve_a.frag cvHistAge, cvAgeK) and Lit Clamp (cvLitReset, cvLitFloor). A trace not
 * marched for more than kHistAgeOld frames (a still camera refreshes every pixel within 16) gets Change Clamp's test at
 * kAgeChangeK x its threshold, against the 9 places the march looked this frame; the march stands in when they differ.
 * Lit Clamp: the relative change of the 9-mean luminance (history vs march) past kLitReset + kChangeResetSd x
 * min( deviations ) / 3 / mean restarts the pixel too; the mean is floored at kLitFloor (dark sky light). Tuned offline
 * (notes/fix1031a/cloud7/tmp/diag_*.log): age 16 x 1.0 raised the Variety sky's moving steady error +0.50 levels (the
 * history read off the march's sample places) and +0.23 aligned; x 1.5 aligned +0.14; Lit 0.03 / 0.05 / 0.10: still sun
 * move 4.01 / 4.03 / 4.13 levels (Off path 3.88), still steady +0.06 / +0.02 / +0.00; but 0.05 made a period-8 lattice
 * under FOGGLOW1's dither (cloudband_check Q 37.1, bar 33.5: resets on noise), 0.07 reads 24.8 and keeps the Ultra moving
 * shift under its bar (0.10: 4.04 > 3.98); x 1.25 then (Ultra moving shift 3.94, Variety moving steady +0.00).
 * The reds (0) = CLOUD6g's resolve. */
constexpr float kHistAgeOld = 16.0f;
constexpr float kAgeChangeK = 1.25f;
constexpr float kLitReset = 0.07f;
constexpr float kLitFloor = 0.02f;

/* lane CLOUDSTEPS1 (CLOUD6c's leftover): the march's step count. With CLOUDLIGHT1's read-free top light a lit sample
 * reads 7 (Medium) / 10 (High) densities instead of 10 / 16, and the saved reads buy steps at main's cost: the most
 * steps (of 48 / 60 / 72 / 96 Medium, 64 / 80 / 96 / 128 High) whose density reads per ray stay <= main's (the taps at the
 * preset steps) in both of CLOUD6c's views: Medium 60 (80.7 / 53.6 reads vs 83.4 / 55.8; 72 reads 96.5 > 83.4), High 80
 * (136.0 / 90.2 vs 157.5 / 104.9; 96 reads 162.8 > 157.5). Edge error (display levels vs a 16x-steps reference):
 * Medium 4.91 -> 3.65 / 6.13 -> 4.54, High 2.85 -> 1.97 / 3.74 -> 2.58 (notes/fix1031a/cloud7/tmp/steps1.log).
 * more = WW_CLOUDVOL_MORESTEPS (the default) AND the top column on (the cost holds only with it); else the preset's. */
inline int marchSteps( int tier, int presetSteps, bool more )
{
	if ( !more )
		return presetSteps;
	return tier == 2 ? 60 : tier == 3 ? 80 : presetSteps;
}

/* lane CLOUDBAND2 (manager 10-30: bungo's rule, a code choice is never handed to him): Cloud Accumulation
 * (WW_CLOUDVOL_ACCUM) and the ambient through the column above (WW_CLOUDVOL_AMBDARK) are the DEFAULT. A switch's value
 * (trimmed, lower case) turns it off only as "0", "off", "false" or "no" (the old path, kept as the red); unset, empty
 * or anything else is on. cloudvol.cpp cs() reads both through this; tests/cloudband/cloudband_dump.cpp prints it. */
inline bool defaultOnSwitch( const char * v )
{
	if ( !v )
		return true;
	static const char * const kOff[4] = { "0", "off", "false", "no" };
	for ( const char * off : kOff ) {
		const char * a = v;
		const char * b = off;
		while ( *a && *b && *a == *b ) {
			a++;
			b++;
		}
		if ( !*a && !*b )
			return false;
	}
	return true;
}

inline float readerWindKm( double drawnSecs, bool red )
{
	return red ? 0.0f : windKm( drawnSecs );
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

/* ---- lane CLOUD5: Cloud Variety (row Render > Scene > Cloud Variety, ships Off). Twin of cloud_common.glsl's
 * WW_CLOUD_VARIETY cloudDensity: keep the two in step (tests/spells/cloudvar_check.py gate T compares them).
 * Sources, per rule:
 * - a cloud TYPE beside the coverage, each type its own height profile: Schneider & Vos 2015 (HZD; weather map B = type,
 *   stratus / stratocumulus / cumulus gradients) [recalled, not re-read this lane]; Bauer 2019 (RDR2, Advances) slide 16:
 *   "LUT defines altitude-based shape, (Alto)Cumulus and stratus ... slice determined by weather parameters" [read,
 *   notes/fogheight1/rdr2/deck.txt]. The four profiles' numbers and the cumulonimbus anchor are ours (unknown in both).
 * - the anvil (coverage raised near a tall cloud's top): Schneider, GPU Pro 7 "Real-Time Volumetric Cloudscapes"
 *   (anvil_bias, a power on the coverage over the top of the profile) [recalled]; our heights 0.65..0.85, power 0.5.
 * - erosion that changes with height: Schneider & Vos 2015 (wispy bottoms, billowy tops: the detail fbm inverted with
 *   height, x 0.2) [recalled]; Bauer 2019 slide 18: "the z channel of the LUT controls the softness of the noise" over
 *   altitude [read]. Ours: 0.35 (CLOUD2's 0.2 read plain), x 0.55 at the base (flat bases) rising to x 1 by 35 %.
 * - a distortion of the detail lookup (wispy edges): Schneider & Vos 2015 (curl noise on the detail coordinates)
 *   [recalled]; Bauer 2019 slide 17 ("2x 2D displacement samples at xy and xz ... offset a 3D noise volume lookup")
 *   [read]. Ours: the curl of a sum of sines (Bridson, Hourihan & Nordenstam 2007, curl-noise) in xy and xz, no texture.
 * - the light taps' reach by weather: Bauer 2019 slide 33 ("the distance of this sample is controlled by weather
 *   parameters ... large, small or flat cloud shapes") [read]. Ours: 0.8 x the tallest type's depth, 1.2..6 km.
 * - a low-frequency weather noise over tens of km that moves coverage and type: ours (Schneider 2017's weather map is
 *   painted or simulated [recalled]; RDR2's cloud map is "modulated by texture and weather data" [read, slide 14]).
 *   Swing kept mean-preserving in map coverage, so the calibration still holds (gate A). */
constexpr int kVarTypes = 4;	// type anchors: 0 stratus, 1/3 stratocumulus, 2/3 cumulus, 1 cumulonimbus
/* the Low band's coverage tables at T = varCalT(k). Four (the type anchors) left a type between cumulus and
 * cumulonimbus 0.14 short of the map (T 0.75, coverage 0.5 drew 0.356: gate A, MERGE1029C 10-29); seven at k / 6 still
 * drew 0.05 for a map of 0.5 at T 0.92 (CLOUD5 round 2, MEASURED): a tower's threshold falls from 0.18 to 0.065 over T
 * 0.83 .. 1 and the opacity moves ~10x the threshold there, so a straight blend between anchors 1/6 apart misses. 13
 * anchors at T = 1 - (1 - k / 12)^2: 0.16 apart at stratus (where 1/6 was already within 0.02), 0.007 at the top. */
constexpr int kVarCal = 13;
inline float varCalT( int k )
{
	const float u = 1.0f - float( k ) / float( kVarCal - 1 );
	return 1.0f - u * u;
}
constexpr float kVarTowerKm = 7.0f;	// a cumulonimbus' depth (ours; typical tops 8-12 km over a ~1 km base: unknown source)
constexpr float kVarWrapKm = 384.0f;	// the weather noise's period: 64 shape periods, windKm's wrap (no seam at the wrap)
constexpr float kVarCurlPeriodKm = 24.0f;	// the curl potential's period (divides kVarWrapKm)
constexpr float kVarErode = 0.35f, kVarCurlKm = 0.15f, kVarAnvil = 1.0f, kVarSwing = 0.9f;

struct Variety
{
	float type = 2.0f / 3.0f;	// the Low band's weather type, 0..1
	float spread = 0.0f;	// the weather noise's swing of the type
	float swing = kVarSwing;	// the weather noise's swing of the coverage (x min(c, 1 - c), mean kept)
	float depthKm = 1.5f;	// the Low band's own depth (the cumulus anchor's), km
	float towerKm = kVarTowerKm;
	float erode = kVarErode;
	float curlKm = kVarCurlKm;	// 0 = the red nocurl
	float anvil = kVarAnvil;
};

inline float fractf( float x )
{
	return x - std::floor( x );
}

//! GLSL mod(x, y): x - y floor(x / y)
inline float gmod( float x, float y )
{
	return x - y * std::floor( x / y );
}

//! integer hash (32-bit wrap, as GLSL uint) -> [0, 1) on 24 bits (exact in float on both sides)
inline float varHash( int ix, int iy, int oct )
{
	std::uint32_t h = std::uint32_t( ix ) * 0x8da6b343u ^ std::uint32_t( iy ) * 0xd8163841u ^ std::uint32_t( oct ) * 0xcb1ab31fu;
	h ^= h >> 13;
	h *= 0x5bd1e995u;
	h ^= h >> 15;
	return float( h >> 8 ) * ( 1.0f / 16777216.0f );
}

/*! a lattice value, antithetic: the cell half a wrap east holds 1 - this one's (CLOUD5 round 2). Then the noise half a
 *  wrap east is exactly the negative, and its mean over the wrap is exactly 0: a plain hash averaged -0.098 over the
 *  gate's 192 km square, which drew a uniform map 0.044 thin at 0.5 through the swing alone (MEASURED). cells is even. */
inline float varLattice( int ix, int iy, int cells, int oct )
{
	const int h = cells / 2;
	return ix >= h ? 1.0f - varHash( ix - h, iy, oct ) : varHash( ix, iy, oct );
}

//! value noise on a cellKm lattice wrapped every kVarWrapKm (cells = kVarWrapKm / cellKm), smoothstep weights
inline float varValue( float x, float y, float cellKm, int cells, int oct )
{
	const float ux = gmod( x, kVarWrapKm ) / cellKm, uy = gmod( y, kVarWrapKm ) / cellKm;
	const int ix = std::min( int( std::floor( ux ) ), cells - 1 ), iy = std::min( int( std::floor( uy ) ), cells - 1 );
	const float fx = ux - float( ix ), fy = uy - float( iy );
	const int jx = ix + 1 == cells ? 0 : ix + 1, jy = iy + 1 == cells ? 0 : iy + 1;
	const float wx = fx * fx * ( 3.0f - 2.0f * fx ), wy = fy * fy * ( 3.0f - 2.0f * fy );
	const float a = varLattice( ix, iy, cells, oct ), b = varLattice( jx, iy, cells, oct );
	const float c = varLattice( ix, jy, cells, oct ), d = varLattice( jx, jy, cells, oct );
	return mixf( mixf( a, b, wx ), mixf( c, d, wx ), wy );
}

//! the weather noise, -1..1, mean 0: two octaves (16 km and 6 km cells), gain 4, clamped (a symmetric clamp keeps the
//! antithetic lattice's exact 0 mean)
inline float weatherNoise( float x, float y )
{
	const float v = 0.65f * varValue( x, y, 16.0f, 24, 0 ) + 0.35f * varValue( x, y, 6.0f, 64, 1 );
	return std::clamp( ( v - 0.5f ) * 4.0f, -1.0f, 1.0f );
}

/*! the detail lookup's displacement per km of amplitude: the curl of psi = sum sin(2 pi (m.u + phase)) / |m| (integer
 *  wave numbers m over the 24 km period: wavelengths 1.7..2.0 km), two waves in xy and two in xz; divergence free */
inline void curlRaw( float x, float y, float z, float out[3] )
{
	const float ux = gmod( x, kVarCurlPeriodKm ) / kVarCurlPeriodKm, uy = gmod( y, kVarCurlPeriodKm ) / kVarCurlPeriodKm;
	const float uz = gmod( z, kVarCurlPeriodKm ) / kVarCurlPeriodKm;
	constexpr float k2Pi = 6.2831853f;
	const float c1 = std::cos( k2Pi * fractf( 13.0f * ux + 5.0f * uy + 0.11f ) );
	const float c2 = std::cos( k2Pi * fractf( -6.0f * ux + 11.0f * uy + 0.33f ) );
	const float c3 = std::cos( k2Pi * fractf( 10.0f * ux + 8.0f * uz + 0.64f ) );
	const float c4 = std::cos( k2Pi * fractf( -7.0f * ux + 12.0f * uz + 0.85f ) );
	const float ax = c1 * 0.358979f + c2 * 0.877896f, ay = c1 * -0.933346f + c2 * 0.478852f;
	const float bx = c3 * 0.624695f + c4 * 0.863779f, bz = c3 * -0.780869f + c4 * 0.503871f;
	out[0] = 0.5f * ( ax + bx );
	out[1] = 0.5f * ay;
	out[2] = 0.5f * bz;
}

//! a type's depth, km: stratus 0.3, stratocumulus 0.6, cumulus 1.0 x the band's own depth, cumulonimbus the tower's
inline float typeDepthKm( float T, float depthKm, float towerKm )
{
	const float u = sat( T ) * 3.0f;
	if ( u < 1.0f )
		return depthKm * mixf( 0.3f, 0.6f, u );
	if ( u < 2.0f )
		return depthKm * mixf( 0.6f, 1.0f, u - 1.0f );
	return mixf( depthKm, std::max( towerKm, depthKm ), u - 2.0f );
}

/*! a type's height profile over its own depth (hf 0..1): rise to a plateau, fall to the top. Between the anchors the
 *  rise and fall heights blend, not the two profiles' values (CLOUD5 round 2): a blend of values left a shoulder at
 *  ~w x 1 above the lower type's fall, under the 1 - c a thin coverage needs, so a type at T 0.94 drew far less than
 *  one at 0.98 for the same threshold (the coverage tables' cliff, MEASURED) and the towers wore a step half way up. */
inline float typeProfile( float T, float hf )
{
	static const float rise[kVarTypes] = { 0.10f, 0.08f, 0.07f, 0.03f };
	static const float fall[kVarTypes] = { 0.60f, 0.45f, 0.55f, 0.85f };	// cumulus = CLOUD2's Low profile
	const float u = sat( T ) * 3.0f;
	const int k = std::min( int( u ), 2 );
	const float r = mixf( rise[k], rise[k + 1], u - float( k ) ), f = mixf( fall[k], fall[k + 1], u - float( k ) );
	return smooth( 0.0f, r, hf ) * ( 1.0f - smooth( f, 1.0f, hf ) );
}

//! the Low band's coverage table at type T (the kVarCal anchors' tables blended), c the map's coverage
inline float calAtT( const float cal[kVarCal][kCalN], float T, float c )
{
	const float u = ( 1.0f - std::sqrt( 1.0f - sat( T ) ) ) * float( kVarCal - 1 );	// varCalT's inverse
	const int k = std::min( int( u ), kVarCal - 2 );
	return mixf( calAt( cal[k], c ), calAt( cal[k + 1], c ), u - float( k ) );
}

//! the anvil: the coverage raised near a tall type's top (power 1 -> 0.5 over hf 0.65..0.85, for T 0.8..1)
inline float anvilCover( float c, float T, float hf, float anvil )
{
	const float e = mixf( 1.0f, 0.5f, anvil * smooth( 0.8f, 1.0f, T ) * smooth( 0.65f, 0.85f, hf ) );
	return c > 0.0f ? std::pow( c, e ) : 0.0f;
}

//! one band's density given the shape read s, the (displaced) detail read det, the profile, the threshold coverage c
inline float varCore( float s, float det, float prof, float c, float hf, float erode )
{
	if ( hf <= 0.0f || hf >= 1.0f || c <= 0.0f )
		return 0.0f;
	const float t = 1.0f - c;
	const float base = sat( ( s * prof - t ) / std::max( 1.0f - t, 1e-4f ) ) * c;
	if ( base <= 0.0f )
		return 0.0f;
	const float e = erode * mixf( 0.55f, 1.0f, smooth( 0.0f, 0.35f, hf ) );
	const float mod = mixf( det, 1.0f - det, sat( hf * 4.0f ) ) * e;
	return sat( ( base - mod ) / ( 1.0f - mod ) );
}

//! the light taps' reach for a sky whose tallest type is typeMax (Bauer 2019: the weather sets it); the preset's at least
inline float varLightKm( float typeMax, float depthKm, float towerKm, float presetKm )
{
	return std::clamp( 0.8f * typeDepthKm( typeMax, depthKm, towerKm ), presetKm, std::max( presetKm, 6.0f ) );
}

struct VarVolumes
{
	const std::uint8_t * shape;
	int ns;
	const std::uint8_t * detail;
	int nd;
};

/*! the whole density at (x, y, z) km (wind 0, mip 0): the twin of the WW_CLOUD_VARIETY cloudDensity. map = cvMapAt's
 *  (R Low, G Mid, B Low sheet, A High); base/top/density per band (Low's top is the map's, already raised for towers) */
inline float varietyDensity( const VarVolumes & vol, const Variety & v, const float baseKm[2], const float topKm[2],
	const float density[2], const float calLow[kVarCal][kCalN], const float calMid[kCalN], const float map[4], float x,
	float y, float z )
{
	const float n = weatherNoise( x, y );
	float d = 0.0f, s = -1.0f, curl[3] = { 0, 0, 0 };
	bool curlOk = false;
	for ( int b = 0; b < 2; b++ ) {
		if ( density[b] <= 0.0f )
			continue;
		float cov = b == 0 ? map[0] : map[1];
		cov += v.swing * n * std::min( cov, 1.0f - cov );
		float hf, prof, c;
		if ( b == 0 ) {
			const float sheet = sat( map[2] / std::max( map[0], 1e-3f ) );
			const float T = sat( v.type + v.spread * n ) * ( 1.0f - sheet );
			hf = ( z - baseKm[0] ) / std::max( typeDepthKm( T, v.depthKm, v.towerKm ), 1e-3f );
			if ( hf <= 0.0f || hf >= 1.0f )
				continue;
			c = anvilCover( calAtT( calLow, T, cov ), T, hf, v.anvil );
			prof = typeProfile( T, hf );
		} else {
			hf = ( z - baseKm[1] ) / std::max( topKm[1] - baseKm[1], 1e-3f );
			if ( hf <= 0.0f || hf >= 1.0f )
				continue;
			c = calAt( calMid, cov );
			prof = gradient( 1, hf, 0.0f );
		}
		if ( c <= 0.0f )
			continue;
		if ( s < 0.0f )
			s = sample3( vol.shape, vol.ns, x / kShapeKm, y / kShapeKm, z / kShapeKm );
		if ( !curlOk ) {
			curlRaw( x, y, z, curl );
			curlOk = true;
		}
		const float a = v.curlKm * ( 1.0f - 0.5f * hf );
		const float det = sample3( vol.detail, vol.nd, ( x + curl[0] * a ) / kDetailKm, ( y + curl[1] * a ) / kDetailKm,
			( z + curl[2] * a ) / kDetailKm );
		d += varCore( s, det, prof, c, hf, v.erode ) * density[b];
	}
	return d;
}

/*! the coverage table of one band under Cloud Variety (calibrate's rule: cal[i] = the threshold coverage whose mean
 *  column opacity from above is i / 16), the weather noise at 0. Band 0 at the type anchor T (its own depth, the anvil),
 *  band 1 over [baseKm, topKm] with the Mid profile. The curl and the new erosion are in. */
inline void varCalibrate( const VarVolumes & vol, const Variety & v, int band, float T, float baseKm, float topKm,
	float density, bool redNoCal, float cal[kCalN] )
{
	for ( int i = 0; i < kCalN; i++ )
		cal[i] = float( i ) / float( kCalN - 1 );
	const float th = band == 0 ? typeDepthKm( T, v.depthKm, v.towerKm ) : topKm - baseKm;
	if ( redNoCal || th <= 1e-3f || density <= 0.0f )
		return;
	constexpr int NC = 12, NZ = 40, NG = 33;
	/* the reads do not depend on the threshold: once per point, not once per level (CLOUD5 round 2: 21 -> ~3 ms a table on
	 * the lane's machine, the same numbers; 13 tables now) */
	std::vector<float> rs( NC * NC * NZ ), rd( NC * NC * NZ ), rp( NC * NC * NZ );
	for ( int j = 0, n = 0; j < NC; j++ )
		for ( int i = 0; i < NC; i++ ) {
			const float x = ( float( i ) + 0.5f ) / NC * kShapeKm, y = ( float( j ) + 0.37f ) / NC * kShapeKm;
			for ( int k = 0; k < NZ; k++, n++ ) {
				const float hf = ( float( k ) + 0.5f ) / NZ, z = baseKm + hf * th;
				float cu[3];
				curlRaw( x, y, z, cu );
				const float a = v.curlKm * ( 1.0f - 0.5f * hf );
				rs[n] = sample3( vol.shape, vol.ns, x / kShapeKm, y / kShapeKm, z / kShapeKm );
				rd[n] = sample3( vol.detail, vol.nd, ( x + cu[0] * a ) / kDetailKm, ( y + cu[1] * a ) / kDetailKm,
					( z + cu[2] * a ) / kDetailKm );
				rp[n] = band == 0 ? typeProfile( T, hf ) : gradient( 1, hf, 0.0f );
			}
		}
	auto opacity = [&]( float cp ) {
		double acc = 0.0;
		for ( int col = 0, n = 0; col < NC * NC; col++ ) {
			float tau = 0.0f;
			for ( int k = 0; k < NZ; k++, n++ ) {
				const float hf = ( float( k ) + 0.5f ) / NZ;
				const float c = band == 0 ? anvilCover( cp, T, hf, v.anvil ) : cp;
				tau += varCore( rs[n], rd[n], rp[n], c, hf, v.erode ) * kSigma * density * th / NZ;
			}
			acc += 1.0 - std::exp( -double( tau ) );
		}
		return float( acc / ( NC * NC ) );
	};
	float o[NG];
	o[0] = 0.0f;
	for ( int j = 1; j < NG; j++ )
		o[j] = std::max( o[j - 1], opacity( float( j ) / ( NG - 1 ) ) );
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

/* lane CLOUDSHAPE1 (bungo 10-31 "do we have shapes like in rdr2?" MEASURED no: the Off density never passes 0.113 and
 * ramps over 2.4 km): Cloud Shapes, the twin of cloud_common.glsl's WW_CLOUD_SHAPE cloudDensity. RDR2 (Bauer 2019 slides
 * 14-18) places clouds with a 2D map, shapes them over height with a LUT, and carves the detail with a SATURATING rescale
 * whose window sets the edge's softness; Schneider 2015 (HZD) gives the Perlin-Worley lumps and the Worley erosion,
 * inverted at the base. Here: a body field (cloudnoise.h cellField) over the coverage threshold, a dome on ONE flat base
 * per body (taller in stronger cells), lumps that grow with height, the eroded edge, then clamp(f / window): dense
 * inside, not multiplied by the coverage. Constants: notes/cloudshape1 (gate.py, the u2 set; RESEARCH.md). */
constexpr float kShpSoft = 0.2f;	// the rescale window (f units): RDR2's LUT z
constexpr float kShpErode = 0.7f;	// the detail erosion's depth (f units)
constexpr float kShpScale = 0.7f;	// the density inside a cloud (x kSigma: 28 /km; 1.0 put the shaded undersides past RDR2's contrast at 4 bearings)
constexpr float kShpShearKm = 0.3f;	// the tops lean downwind, km per km of height (HZD: wind varies over altitude)
constexpr float kShpRise = 0.04f;	// the base's rise (height fraction): a flat base
constexpr float kShpCurlKm = 0.25f;	// the detail lookup's curl displacement at the base, km
constexpr float kShpBaseErode = 0.3f;	// the erosion at the base, a share of kShpErode (keeps the base flat)
constexpr float kShpLump = 0.45f;	// the Perlin-Worley lumps' amplitude (f units per sd)
constexpr float kShpCellKm = 32.0f;	// the body field's period
constexpr float kShpGrowth = 0.8f;	// the lumps grow from 1 - this at the base to 1 at the top
constexpr float kShpTapFoot = 0.2f;	// a light tap reads the density at this share of its segment (HZD's cone smooths)
constexpr float kShpSoftPerKm = 0.5f;	// the window widens with the reader's footprint, per km (no aliasing)
constexpr float kShpMean = 0.884f, kShpSd = 0.053f;	// the shape volume's mean and sd (MEASURED; cloudshape_dump prints them)
constexpr float kShpDir[2] = { 1.0f, 0.0f };	// the shear's direction: the wind's (windKm is along +x)

/*! the per-type rule (the map's Low type T: 0 stratus, 1/3 stratocumulus, 2/3 cumulus, 1 cumulonimbus): the domes'
 *  height in the band, their shape exponent (2 = round domes; 1.2 for towers: the height follows the cell's strength, so
 *  only the strongest cells tower over the cumulus round them), the sheet share (stratus spreads flat), the anvil (the
 *  threshold lowered near the top: the towers' tops spread) */
struct ShapeType
{
	float topFrac, domeK, sheet, anvil;
};
inline ShapeType shapeType( float T )
{
	const float tw = smooth( 2.0f / 3.0f, 1.0f, T );
	// towers (tw > 0): the cumulus round them keep low domes (0.15 of the tall band), the towers come from shapeTower
	return { mixf( 0.6f, 0.15f, tw ), 2.0f, sat( 1.0f - 3.0f * T ), 0.5f * tw };
}

/* the towers of a cumulonimbus sky (anvil > 0: the tower share is anvil / 0.5): the body field read at kShpTowerScale x its
 * period (towers 3x the cumulus' width), only cells past the middle strength rise, their height following the strength:
 * f_t = w_big - (0.5 + 0.5 hf^1.2). The towers join the cumulus field (the larger f) */
constexpr float kShpTowerScale = 3.0f;
inline float shapeTowerF( float cellBig, float c, float hf )
{
	const float w = ( cellBig - ( 1.0f - c ) ) / std::max( c, 1e-3f );
	return w - ( 0.5f + 0.5f * std::pow( std::max( hf, 0.0f ), 1.2f ) );
}

//! the anvil's lift of f at hf (0 below 80 % of the band, the full anvil from 90 to 96 %, gone at the top)
inline float shapeAnvil( float anvil, float hf )
{
	return anvil * smooth( 0.8f, 0.9f, hf ) * ( 1.0f - smooth( 0.96f, 1.0f, hf ) );
}

//! a bilinear, repeating read of an n x n float field at (u, w) in periods, texel centres at (i + 0.5) / n
inline float sample2( const float * v, int n, float u, float w )
{
	const float fx = u * float( n ) - 0.5f, fy = w * float( n ) - 0.5f;
	const float x0 = std::floor( fx ), y0 = std::floor( fy );
	const float ax = fx - x0, ay = fy - y0;
	auto at = [&]( float x, float y ) {
		auto wr = [n]( float c ) {
			int i = int( c ) % n;
			return i < 0 ? i + n : i;
		};
		return v[size_t( wr( y ) ) * size_t( n ) + size_t( wr( x ) )];
	};
	return mixf( mixf( at( x0, y0 ), at( x0 + 1.0f, y0 ), ax ), mixf( at( x0, y0 + 1.0f ), at( x0 + 1.0f, y0 + 1.0f ), ax ), ay );
}

/*! the shape density's core given its reads: the body cell (0..1), the shape s, the detail det, the height fraction hf,
 *  the calibrated coverage c, the sheet share, the type rule, the reader's footprint km. Relative (x kSigma x the band's
 *  density). */
inline float shapeCore( float cell, float cellBig, float s, float det, float hf, float c, float sheet, const ShapeType & st,
	float footKm )
{
	if ( hf <= 0.0f || hf >= 1.0f || c <= 0.0f )
		return 0.0f;
	const float w = ( cell - ( 1.0f - c ) ) / std::max( c, 1e-3f );
	const float hd = hf / std::max( st.topFrac, 1e-3f );
	const float reach = mixf( std::pow( hd, st.domeK ), hf * 4.0f, sheet );
	float f = w - reach;
	if ( st.anvil > 0.0f )
		f = mixf( f, std::max( f, shapeTowerF( cellBig, c, hf ) ), st.anvil * 2.0f );
	f += shapeAnvil( st.anvil, hf ) - ( 1.0f - smooth( 0.0f, kShpRise, hf ) );
	f += ( s - kShpMean ) / kShpSd * kShpLump * mixf( 1.0f - kShpGrowth, 1.0f, hf );
	const float er = kShpErode * mixf( kShpBaseErode, 1.0f, smooth( 0.0f, 0.3f, hf ) );
	f -= mixf( det, 1.0f - det, sat( hf * 4.0f ) ) * er;
	return sat( f / std::max( kShpSoft + footKm * kShpSoftPerKm, 1e-3f ) ) * kShpScale;
}

struct ShapeVolumes
{
	const std::uint8_t * shape;
	int ns;
	const std::uint8_t * detail;
	int nd;
	const float * cell;
	int nc;
};

//! the three reads at (x, y, z) km for a band whose base is baseKm (wind 0, mip 0), as cloud_common.glsl takes them
inline void shapeReads( const ShapeVolumes & vol, int band, float baseKm, float hf, float x, float y, float z, float & cell,
	float & cellBig, float & s, float & det )
{
	const float qx = x + kShpDir[0] * kShpShearKm * ( z - baseKm ), qy = y + kShpDir[1] * kShpShearKm * ( z - baseKm );
	cell = sample2( vol.cell, vol.nc, qx / kShpCellKm + 0.37f * float( band ), qy / kShpCellKm + 0.37f * float( band ) );
	cellBig = sample2( vol.cell, vol.nc, qx / ( kShpCellKm * kShpTowerScale ) + 0.21f, qy / ( kShpCellKm * kShpTowerScale ) + 0.21f );
	s = sample3( vol.shape, vol.ns, qx / kShapeKm, qy / kShapeKm, z / kShapeKm );
	float cu[3];
	curlRaw( qx, qy, z, cu );
	const float a = kShpCurlKm * ( 1.0f - 0.5f * hf );
	det = sample3( vol.detail, vol.nd, ( qx + cu[0] * a ) / kDetailKm, ( qy + cu[1] * a ) / kDetailKm, ( z + cu[2] * a ) / kDetailKm );
}

/*! the whole density at (x, y, z) km: the twin of the WW_CLOUD_SHAPE cloudDensity (wind 0, footprint 0). map = cvMapAt's;
 *  the Low band's top is the map's (raised to the type's depth); types per band; cal per band (calAt) */
inline float shapeDensity( const ShapeVolumes & vol, const ShapeType st[2], const float baseKm[2], const float topKm[2],
	const float density[2], const float cal[2][kCalN], const float map[4], float x, float y, float z )
{
	float d = 0.0f;
	for ( int b = 0; b < 2; b++ ) {
		if ( density[b] <= 0.0f )
			continue;
		const float hf = ( z - baseKm[b] ) / std::max( topKm[b] - baseKm[b], 1e-3f );
		if ( hf <= 0.0f || hf >= 1.0f )
			continue;
		const float cov = b == 0 ? map[0] : map[1];
		const float sheet = b == 0 ? std::max( sat( map[2] / std::max( map[0], 1e-3f ) ), st[0].sheet ) : st[1].sheet;
		const float c = calAt( cal[b], cov );
		float cell, cellBig, s, det;
		shapeReads( vol, b, baseKm[b], hf, x, y, z, cell, cellBig, s, det );
		d += shapeCore( cell, cellBig, s, det, hf, c, sheet, st[b], 0.0f ) * density[b];
	}
	return d;
}

/*! the coverage table of one band under Cloud Shapes (calibrate's rule: cal[i] = the threshold coverage whose mean
 *  column opacity from above is i / 16), over 64 x 64 columns of one body-field period (the towers' period with
 *  towers), 40 steps through the band */
inline void shapeCalibrate( const ShapeVolumes & vol, const ShapeType & st, int band, float baseKm, float topKm,
	float density, float sheet, bool redNoCal, float cal[kCalN] )
{
	for ( int i = 0; i < kCalN; i++ )
		cal[i] = float( i ) / float( kCalN - 1 );
	const float th = topKm - baseKm;
	if ( redNoCal || th <= 1e-3f || density <= 0.0f )
		return;
	// 64 x 64 columns (0.5 km apart over the 32 km field: its smallest bodies are ~0.7 km; 32 x 32 missed the 0.7 stripe by
	// 0.06, tests/spells/cloudshape_check.py gate A)
	constexpr int NC = 64, NZ = 40, NG = 33;
	std::vector<float> rc( NC * NC * NZ ), rb( NC * NC * NZ ), rs( NC * NC * NZ ), rd( NC * NC * NZ );
	const float span = kShpCellKm * ( st.anvil > 0.0f ? kShpTowerScale : 1.0f );
	for ( int j = 0, n = 0; j < NC; j++ )
		for ( int i = 0; i < NC; i++ ) {
			const float x = ( float( i ) + 0.5f ) / NC * span, y = ( float( j ) + 0.37f ) / NC * span;
			for ( int k = 0; k < NZ; k++, n++ ) {
				const float hf = ( float( k ) + 0.5f ) / NZ;
				shapeReads( vol, band, baseKm, hf, x, y, baseKm + hf * th, rc[n], rb[n], rs[n], rd[n] );
			}
		}
	auto opacity = [&]( float cp ) {
		double acc = 0.0;
		for ( int col = 0, n = 0; col < NC * NC; col++ ) {
			float tau = 0.0f;
			for ( int k = 0; k < NZ; k++, n++ )
				tau += shapeCore( rc[n], rb[n], rs[n], rd[n], ( float( k ) + 0.5f ) / NZ, cp, sheet, st, 0.0f ) * kSigma * density * th / NZ;
			acc += 1.0 - std::exp( -double( tau ) );
		}
		return float( acc / ( NC * NC ) );
	};
	float o[NG];
	o[0] = 0.0f;
	for ( int j = 1; j < NG; j++ )
		o[j] = std::max( o[j - 1], opacity( float( j ) / ( NG - 1 ) ) );
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

} // namespace WwCloudVolMath

#endif
