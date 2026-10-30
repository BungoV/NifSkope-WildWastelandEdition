#ifndef RTDDGIMATH_H
#define RTDDGIMATH_H

/* Lane RTXGI1 (2026-10-29): the DDGI probe math, header-only, no Qt. One copy of every rule the volume uses, read
 * by src/rtddgi.cpp (the CPU twin), by tests/rtddgi_math (the unit test with its reds) and mirrored line for line in
 * src/rtddgi/ddgi_common.glsl (each GLSL function carries "MIRROR rtddgimath.h <name>").
 *
 * Written from the published math only (notes/rtxgi1/GUIDE.md is the spec; tags):
 *   [J19] Majercik et al., JCGT 8(2) 2019 (octahedral probes, cosine blend, Chebyshev visibility, weights)
 *   [J21] Majercik et al., JCGT 10(2) 2021 (self-shadow bias eq 2, backface rays, gamma 5, per-texel hysteresis,
 *         relocation fig 8, probe states)
 *   [C14] Cigolle et al. 2014 (octahedral unit vectors); [K15] Keinert et al. 2015 (spherical Fibonacci);
 *   [A92] Arvo, Graphics Gems III (fast random rotation matrices)
 *   ours  our own choice (named in the handoff, notes/rtxgi1/M1_HANDOFF.md)
 * No NVIDIA SDK code is in here or anywhere in the lane (its licence is not open source). */

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rtddgi {

// ------------------------------------------------------------------ constants (GUIDE.md sections 1-3)
constexpr int kIrrInterior = 6;            // [J21 7.1] 6x6 + a one-texel border
constexpr int kIrrTile = kIrrInterior + 2; // 8
constexpr int kDistInterior = 14;          // [J21 7.1] 14x14 + border
constexpr int kDistTile = kDistInterior + 2;   // 16
constexpr int kFixedRays = 32;             // [SDK] the first 32 rays, never rotated, never blended
constexpr int kMaxRays = 256;              // ours: the blend's shared memory holds one probe's rays
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGamma = 5.0f;             // [J21 4.2]
constexpr float kHysteresis = 0.97f;       // ours, inside [J19]'s 0.85-0.98
constexpr float kChangeSmall = 0.25f;      // [J21 4.3 fig 5] significantChangeThreshold
constexpr float kChangeSmallDrop = 0.15f;  // [J21 4.3] h -= 0.15
constexpr float kChangeLarge = 0.8f;       // [J21 4.3] newDistributionChangeThreshold: h = 0
constexpr float kSharpness = 50.0f;        // ours / GUIDE: the distance blend's cosine power
constexpr float kDistWeightMin = 0.001f;   // [J19] distance weights under this are skipped
constexpr float kBackfaceDist = 0.2f;      // [J21 4.1] a backface hit's distance shortened by 80 %
constexpr float kBackfaceShare = 0.25f;    // [J21 fig 8, 6.1] inside geometry above this share
constexpr float kBiasN = 0.2f, kBiasV = 0.8f, kBiasD = 0.75f;   // [J21 eq 2]
constexpr float kSelfShadowBias = 0.3f;    // [J21 eq 2] B default
constexpr float kBackWeightFloor = 0.2f;   // [J19 5.2] smooth backface ((cos + 1) / 2)^2 + 0.2
constexpr float kChebFloor = 0.05f;        // [J19 5.2] w *= max(0.05, cheb^3)
constexpr float kCrush = 0.2f;             // [J19 5.2] low-weight crush below 0.2
constexpr float kRelocLimit = 0.45f;       // [SDK doc] a probe moves at most 45 % of its cell per axis
constexpr int kRelocIterations = 5;        // [J21 5]
constexpr float kRelocStep = 0.2f;         // GUIDE 2.5: step <= 0.2 x spacing toward the farthest face
constexpr float kRelocMinFront = 0.25f;    // ours: only a probe whose closest front face is nearer than this x spacing steps
constexpr float kMiss = 1e27f;             // a miss's stored distance (the blend clamps it to the volume's distClamp)
constexpr float kTraceTmin = 1e-4f;        // probebvh's ray / anyHitIn: a hit counts only at t > 1e-4 (the GPU's too, M1FIX)
constexpr float kTieAbs = 1e-4f, kTieRel = 2e-6f;   // ours (M1FIX): the nearest hit's tie window, tieWindow
enum State : int { kOff = 0, kAsleep = 1, kVigilant = 2 };
//! M3 (ours): a probe a scroll has just reset; relocated and classified alone before the next update (GLSL DDGI_FRESH).
//! The read never takes it (only kVigilant is usable)
constexpr int kFresh = 3;

// ------------------------------------------------------------------ vectors
struct V3
{
	float x = 0, y = 0, z = 0;
	V3() = default;
	V3( float a, float b, float c ) : x( a ), y( b ), z( c ) {}
	float operator[]( int k ) const { return k == 0 ? x : k == 1 ? y : z; }
	float & operator[]( int k ) { return k == 0 ? x : k == 1 ? y : z; }
};
inline V3 operator+( V3 a, V3 b ) { return V3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline V3 operator-( V3 a, V3 b ) { return V3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline V3 operator-( V3 a ) { return V3( -a.x, -a.y, -a.z ); }
inline V3 operator*( V3 a, float s ) { return V3( a.x * s, a.y * s, a.z * s ); }
inline V3 operator*( V3 a, V3 b ) { return V3( a.x * b.x, a.y * b.y, a.z * b.z ); }
inline float dot( V3 a, V3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross( V3 a, V3 b ) { return V3( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x ); }
inline float length( V3 a ) { return std::sqrt( dot( a, a ) ); }
inline V3 normalize( V3 a ) { const float l = length( a ); return l > 0.0f ? a * ( 1.0f / l ) : V3( 0, 0, 1 ); }
inline float signNotZero( float v ) { return v >= 0.0f ? 1.0f : -1.0f; }
inline float clampf( float v, float a, float b ) { return std::min( std::max( v, a ), b ); }
inline float fractf( float v ) { return v - std::floor( v ); }

// ------------------------------------------------------------------ octahedral map [C14] (Z up: +Z at the centre)
//! unit dir -> (u, v) in [-1, 1]^2.  GLSL: ddgiOctEncode
inline void octEncode( V3 d, float & u, float & v )
{
	const float l1 = std::fabs( d.x ) + std::fabs( d.y ) + std::fabs( d.z );
	u = d.x / l1;
	v = d.y / l1;
	if ( d.z < 0.0f ) {
		const float ou = u;
#ifdef RTDDGI_RED_OCT
		(void)ou;   // red: the lower hemisphere is not folded (a wrong map, the round trip fails)
#else
		u = ( 1.0f - std::fabs( v ) ) * signNotZero( ou );
		v = ( 1.0f - std::fabs( ou ) ) * signNotZero( v );
#endif
	}
}
//! (u, v) in [-1, 1]^2 -> unit dir.  GLSL: ddgiOctDecode
inline V3 octDecode( float u, float v )
{
	V3 d( u, v, 1.0f - std::fabs( u ) - std::fabs( v ) );
	if ( d.z < 0.0f ) {
		const float ox = d.x;
		d.x = ( 1.0f - std::fabs( d.y ) ) * signNotZero( ox );
		d.y = ( 1.0f - std::fabs( ox ) ) * signNotZero( d.y );
	}
	return normalize( d );
}
//! the direction at the centre of interior texel (tx, ty) of an n x n map (0-based, border excluded).  GLSL: ddgiTexelDir
inline V3 texelDir( int tx, int ty, int n )
{
	return octDecode( ( float( tx ) + 0.5f ) / float( n ) * 2.0f - 1.0f, ( float( ty ) + 0.5f ) / float( n ) * 2.0f - 1.0f );
}
/*! the source texel of tile texel (x, y) of an S x S tile (border 1): an interior texel is its own; a border texel
 *  copies the interior texel the octahedral fold puts next to it ([J19] fig 12, [J21] fig 13): edges mirror along
 *  the edge, corners take the opposite corner.  GLSL: ddgiBorderSource */
inline void borderSource( int x, int y, int S, int & sx, int & sy )
{
	const int n = S - 2;
	sx = x;
	sy = y;
	const bool l = x == 0, r = x == S - 1, t = y == 0, b = y == S - 1;
	if ( ( l || r ) && ( t || b ) ) {   // corner: the diagonally opposite interior corner
		sx = l ? n : 1;
		sy = t ? n : 1;
		return;
	}
#ifdef RTDDGI_RED_BORDER
	if ( t ) sy = 1;   // red: a clamp copy, no mirror
	if ( b ) sy = n;
	if ( l ) sx = 1;
	if ( r ) sx = n;
#else
	if ( t ) { sx = S - 1 - x; sy = 1; }
	if ( b ) { sx = S - 1 - x; sy = n; }
	if ( l ) { sx = 1; sy = S - 1 - y; }
	if ( r ) { sx = n; sy = S - 1 - y; }
#endif
}
//! border texel k (0 .. 4S-5) of an S x S tile: the top row, the bottom row, the left column, the right column.  GLSL: ddgiBorderXY
inline void borderXY( int k, int S, int & x, int & y )
{
	if ( k < S ) { x = k; y = 0; return; }
	k -= S;
	if ( k < S ) { x = k; y = S - 1; return; }
	k -= S;
	if ( k < S - 2 ) { x = 0; y = 1 + k; return; }
	k -= S - 2;
	x = S - 1;
	y = 1 + k;
}
/*! bilinear read of a tile (S x S texels of `ch` floats, the border filled) at unit direction d: out[0..ch).
 *  GLSL: ddgiSampleIrr / ddgiSampleDist (one per buffer) */
inline void sampleTile( const float * tile, int S, int ch, V3 d, float * out )
{
	float u, v;
	octEncode( d, u, v );
	const int n = S - 2;
	const float fx = 1.0f + ( u * 0.5f + 0.5f ) * float( n ) - 0.5f;   // texel space, centres on integers
	const float fy = 1.0f + ( v * 0.5f + 0.5f ) * float( n ) - 0.5f;
	const int x0 = std::min( std::max( int( std::floor( fx ) ), 0 ), S - 2 ), y0 = std::min( std::max( int( std::floor( fy ) ), 0 ), S - 2 );
	const float ax = clampf( fx - float( x0 ), 0.0f, 1.0f ), ay = clampf( fy - float( y0 ), 0.0f, 1.0f );
	for ( int c = 0; c < ch; c++ ) {
		const float a = tile[( y0 * S + x0 ) * ch + c], b = tile[( y0 * S + x0 + 1 ) * ch + c];
		const float e = tile[( ( y0 + 1 ) * S + x0 ) * ch + c], f = tile[( ( y0 + 1 ) * S + x0 + 1 ) * ch + c];
		out[c] = ( a * ( 1.0f - ax ) + b * ax ) * ( 1.0f - ay ) + ( e * ( 1.0f - ax ) + f * ax ) * ay;
	}
}

// ------------------------------------------------------------------ rays: spherical Fibonacci [K15], rotation [A92]
//! direction i of n.  GLSL: ddgiFibonacci
inline V3 sphericalFibonacci( int i, int n )
{
	const float kPhiMinus1 = 0.61803398874989484820f;   // (sqrt 5 + 1) / 2 - 1
	const float phi = 2.0f * kPi * fractf( float( i ) * kPhiMinus1 );
#ifdef RTDDGI_RED_FIB
	const float cosT = 1.0f - float( i ) / float( n );   // red: the strata start at the pole and never reach the other
#else
	const float cosT = 1.0f - ( 2.0f * float( i ) + 1.0f ) / float( n );
#endif
	const float sinT = std::sqrt( clampf( 1.0f - cosT * cosT, 0.0f, 1.0f ) );
	return V3( std::cos( phi ) * sinT, std::sin( phi ) * sinT, cosT );
}
//! a 3x3 rotation, rows m[0..2], m[3..5], m[6..8]
struct M3 { float m[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }; };
inline V3 mul( const M3 & r, V3 v )
{
	return V3( r.m[0] * v.x + r.m[1] * v.y + r.m[2] * v.z, r.m[3] * v.x + r.m[4] * v.y + r.m[5] * v.z,
		r.m[6] * v.x + r.m[7] * v.y + r.m[8] * v.z );
}
/*! [A92]: three uniform numbers in [0, 1) -> a uniformly distributed rotation: a turn 2 pi x1 about z, then the
 *  Householder reflection by v = (cos(2 pi x2) sqrt x3, sin(2 pi x2) sqrt x3, sqrt(1 - x3)), negated (det +1). */
inline M3 arvoRotation( float x1, float x2, float x3 )
{
	const float th = 2.0f * kPi * x1, ph = 2.0f * kPi * x2, r = std::sqrt( x3 );
	const float vx = std::cos( ph ) * r, vy = std::sin( ph ) * r, vz = std::sqrt( std::max( 0.0f, 1.0f - x3 ) );
	const float ct = std::cos( th ), st = std::sin( th );
	const float R[9] = { ct, st, 0, -st, ct, 0, 0, 0, 1 };
	const float v[3] = { vx, vy, vz };
	M3 out;
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ ) {
			float s = 0.0f;
			for ( int k = 0; k < 3; k++ )
				s += ( 2.0f * v[i] * v[k] - ( i == k ? 1.0f : 0.0f ) ) * R[k * 3 + j];   // -(I - 2 v v^T) R
			out.m[i * 3 + j] = s;
		}
	return out;
}
//! splitmix64: the per-update rotation's numbers (CPU only; the GPU gets the matrix)
inline uint64_t splitmix( uint64_t & s )
{
	uint64_t z = ( s += 0x9E3779B97F4A7C15ULL );
	z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ULL;
	z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBULL;
	return z ^ ( z >> 31 );
}
inline float unit01( uint64_t & s ) { return float( splitmix( s ) >> 40 ) / float( 1ULL << 24 ); }
//! the rotation of update `frame` of a volume seeded `seed` (CPU and GPU take the same matrix)
inline M3 updateRotation( uint32_t seed, uint32_t frame )
{
	uint64_t s = ( uint64_t( seed ) << 32 ) ^ uint64_t( frame ) * 0x2545F4914F6CDD1DULL;
	const float a = unit01( s ), b = unit01( s ), c = unit01( s );
	return arvoRotation( a, b, c );
}
/*! ray i of a probe's n: the first kFixedRays are the fixed set (Fibonacci of kFixedRays, never rotated), the rest
 *  the Fibonacci set of n - kFixedRays turned by this update's rotation [SDK fixed rays].  GLSL: ddgiRayDir */
inline V3 rayDir( int i, int n, const M3 & rot )
{
	if ( i < kFixedRays )
		return sphericalFibonacci( i, kFixedRays );
	return normalize( mul( rot, sphericalFibonacci( i - kFixedRays, n - kFixedRays ) ) );
}

// ------------------------------------------------------------------ the nearest hit's tie rule (ours, M1FIX)
/*! Faces a ray meets within this window of the nearest hit's t count as one surface (a flipped duplicate made for two
 *  sides, a shell's back and a room's face on one plane, the GPU's float t against the CPU's double): 1e-4 +
 *  2e-6 x (|o|_1 + t), above the float rounding of t at that origin, below a decal's offset.  GLSL: ddgiTieWindow */
inline float tieWindow( V3 o, float t )
{
	return kTieAbs + kTieRel * ( std::fabs( o.x ) + std::fabs( o.y ) + std::fabs( o.z ) + t );
}
/*! Inside the window, face A (back = a one-sided face seen from behind, tri = its soup number) replaces the kept face
 *  B when A is a front face and B a back face, or both are alike and A's number is lower. Both tracers then keep the
 *  same face whatever order their trees visit them in (probebvh keeps the last visited, the hardware any).
 *  GLSL: ddgiTieWins */
inline bool tieWins( bool backA, int triA, bool backB, int triB )
{
#ifdef RTDDGI_RED_TIE
	(void)backA;
	(void)backB;
	return triA < triB;   // red: the number alone, so a flipped duplicate's back face can win
#else
	if ( backA != backB )
		return !backA;
	return triA < triB;
#endif
}

// ------------------------------------------------------------------ encoding and hysteresis [J21 4.2, 4.3]
inline float gammaEncode( float x )   //! GLSL: ddgiGammaEncode
{
#ifdef RTDDGI_RED_GAMMA
	return std::pow( std::max( x, 0.0f ), 1.0f / 2.0f );   // red: gamma 2
#else
	return std::pow( std::max( x, 0.0f ), 1.0f / kGamma );
#endif
}
inline float gammaDecode( float e ) { return std::pow( std::max( e, 0.0f ), kGamma ); }   //! GLSL: ddgiGammaDecode
/*! the hysteresis of one irradiance texel this update: the base h lowered by 0.15 when any channel of the
 *  encoded value moved more than 0.25, to 0 when more than 0.8 ([J21] fig 5; the change is the largest |new - old|
 *  over the channels, ours: the figure's maxComponent of a signed difference then abs).  GLSL: ddgiTexelHysteresis */
inline float texelHysteresis( float h, const float oldEnc[3], const float newEnc[3] )
{
	float ch = 0.0f;
	for ( int c = 0; c < 3; c++ )
		ch = std::max( ch, std::fabs( newEnc[c] - oldEnc[c] ) );
#ifdef RTDDGI_RED_HYST
	if ( ch > kChangeLarge ) h = std::max( 0.0f, h - kChangeSmallDrop );   // red: the thresholds swapped
	if ( ch > kChangeSmall ) h = 0.0f;
#else
	if ( ch > kChangeSmall )
		h = std::max( 0.0f, h - kChangeSmallDrop );
	if ( ch > kChangeLarge )
		h = 0.0f;
#endif
	return h;
}
/*! the base hysteresis of a probe's update number `age` (0 = its first): 0 first ([J21 6.3] a newly vigilant probe
 *  converges at once), then the running mean age / (age + 1) until it reaches h (ours: an offline volume
 *  converges in ~1 / (1 - h) updates without a warm-up bias).  GLSL: ddgiAgeHysteresis */
inline float ageHysteresis( uint32_t age, float h )
{
	return age == 0 ? 0.0f : std::min( h, float( age ) / float( age + 1 ) );
}

/*! one irradiance texel's update from a probe's blended rays (dirs, radiance rgb): the cosine-weighted mean x 0.5
 *  ([J19] eq. in 5.1, GUIDE 2.3; decoded x 2 pi it is the irradiance), encoded, blended with old by the texel
 *  hysteresis. old/out encoded rgb. Returns false when no ray faces the texel (out = old). hOut (when given) gets the
 *  texel hysteresis used, for the sky-visibility texel beside it (blendSkyVisTexel).  GLSL: blend.comp */
inline bool blendIrradianceTexel( V3 t, const V3 * dirs, const float * rad, int nRays, float hBase, const float old[3], float out[3],
	float * hOut = nullptr )
{
	if ( hOut )
		*hOut = hBase;
	double s[3] = { 0, 0, 0 }, w = 0;
	for ( int r = 0; r < nRays; r++ ) {
		const float c = std::max( 0.0f, dot( t, dirs[r] ) );
		if ( c <= 0.0f )
			continue;
		for ( int k = 0; k < 3; k++ )
			s[k] += double( c ) * rad[r * 3 + k];
		w += c;
	}
	if ( w <= 0.0 ) {
		for ( int k = 0; k < 3; k++ )
			out[k] = old[k];
		return false;
	}
	float enc[3];
	for ( int k = 0; k < 3; k++ ) {
#ifdef RTDDGI_RED_NORM
		enc[k] = gammaEncode( float( s[k] / w ) );   // red: no 0.5 (decoded x 2 pi gives 2 pi L, not pi L)
#else
		enc[k] = gammaEncode( float( 0.5 * s[k] / w ) );
#endif
	}
	const float h = texelHysteresis( hBase, old, enc );
	for ( int k = 0; k < 3; k++ )
		out[k] = enc[k] * ( 1.0f - h ) + old[k] * h;
	if ( hOut )
		*hOut = h;
	return true;
}
/*! lane SKYBELOW1 (bungo 10-30): is a ray direction below the horizon (the volume's frame is the world's, z up).
 *  GLSL: ddgiBelowHorizon */
inline bool belowHorizon( V3 d )
{
#ifdef RTDDGI_RED_SKYABOVE
	return d.z < 0.1f;   // red: a lifted horizon (the near-horizon sky above it turns ground)
#else
	return d.z < 0.0f;
#endif
}
/*! lane SKYBELOW1: does a ray count as sky visibility: a miss (nothing within the trace reach) ABOVE the horizon; a
 *  below-horizon direction is never sky, open or not (bungo 10-30). rule false = the red WW_SKYBELOW_RED (every miss,
 *  today's main).  GLSL: ddgiSkyVisRay */
inline bool skyVisRay( V3 d, float dist, bool rule )
{
#ifdef RTDDGI_RED_SKYVISUP
	rule = false;   // red: a below-horizon miss is sky visibility too
#endif
	return dist >= kMiss * 0.5f && !( rule && belowHorizon( d ) );
}
/*! one sky-visibility texel (ours, bungo 10-29; GUIDE section 1, stored in the irradiance texel's alpha): the
 *  cosine-weighted share of the blended rays that MISS (stored distance kMiss: nothing within the trace reach),
 *  NOT gamma encoded, blended with old by h = the same texel's irradiance hysteresis this update (hOut above).
 *  Geometry only: a --no-sky volume still has it. Returns false when no ray faces the texel (out = old).
 *  SKYBELOW1: a miss counts only above the horizon (skyVisRay; upper false = the red).  GLSL: blend.comp */
inline bool blendSkyVisTexel( V3 t, const V3 * dirs, const float * dist, int nRays, float h, float old, float * out,
	bool upper = true )
{
	double s = 0, w = 0;
	for ( int r = 0; r < nRays; r++ ) {
		const float c = std::max( 0.0f, dot( t, dirs[r] ) );
		if ( c <= 0.0f )
			continue;
		const double miss = skyVisRay( dirs[r], dist[r], upper ) ? 1.0 : 0.0;
#ifdef RTDDGI_RED_SKYVIS
		s += miss;   // red: the plain share of the facing rays, not cosine weighted
		w += 1.0;
#else
		s += double( c ) * miss;
		w += c;
#endif
	}
	if ( w <= 0.0 ) {
		*out = old;
		return false;
	}
	*out = float( s / w ) * ( 1.0f - h ) + old * h;
	return true;
}
/*! one distance texel: weights max(0, cos)^sharpness (under kDistWeightMin skipped), the mean and mean square of
 *  min(|d|, clamp), blended by the base hysteresis only (never the irradiance thresholds [J21 4.3]).  GLSL: blend.comp */
inline bool blendDistanceTexel( V3 t, const V3 * dirs, const float * dist, int nRays, float sharpness, float clampD, float hBase,
	const float old[2], float out[2] )
{
	double s = 0, s2 = 0, w = 0;
	for ( int r = 0; r < nRays; r++ ) {
		const float c = std::max( 0.0f, dot( t, dirs[r] ) );
		const float wr = std::pow( c, sharpness );
		if ( wr < kDistWeightMin )
			continue;
		const float d = std::min( std::fabs( dist[r] ), clampD );
		s += double( wr ) * d;
		s2 += double( wr ) * d * d;
		w += wr;
	}
	if ( w <= 0.0 ) {
		out[0] = old[0];
		out[1] = old[1];
		return false;
	}
	out[0] = float( s / w ) * ( 1.0f - hBase ) + old[0] * hBase;
	out[1] = float( s2 / w ) * ( 1.0f - hBase ) + old[1] * hBase;
	return true;
}

// ------------------------------------------------------------------ sampling a surface [J19 5.2, J21 4.1 eq 2]
//! the self-shadow bias vector: (0.2 n + 0.8 o) x 0.75 D x B.  GLSL: ddgiBiasVector
inline V3 biasVector( V3 n, V3 o, float D, float B )
{
#ifdef RTDDGI_RED_BIAS
	return ( n * kBiasV + o * kBiasN ) * ( kBiasD * D * B );   // red: the two weights swapped
#else
	return ( n * kBiasN + o * kBiasV ) * ( kBiasD * D * B );
#endif
}
//! the smooth backface weight ((dot(dirToProbe, n) + 1) / 2)^2 + 0.2.  GLSL: ddgiBackfaceWeight
inline float backfaceWeight( V3 dirToProbe, V3 n )
{
	const float c = ( dot( dirToProbe, n ) + 1.0f ) * 0.5f;
	return c * c + kBackWeightFloor;
}
/*! the Chebyshev visibility factor of a point r from the probe given the texel's (mean, mean^2): 1 at or before the
 *  mean, else max(0.05, (var / (var + (r - mean)^2))^3).  GLSL: ddgiChebyshev */
inline float chebyshevWeight( float r, float mean, float mean2 )
{
	if ( r <= mean )
		return 1.0f;
	const float var = std::fabs( mean * mean - mean2 );
	const float d = r - mean;
	float c = var / std::max( var + d * d, 1e-12f );
#ifndef RTDDGI_RED_CHEB
	c = c * c * c;   // red: the cube dropped
#endif
	return std::max( kChebFloor, std::max( c, 0.0f ) );
}
//! [J19 5.2] the low-weight crush: under 0.2, w x w^2 / 0.04.  GLSL: ddgiCrush
inline float crushWeight( float w )
{
	return w < kCrush ? w * w * w / ( kCrush * kCrush ) : w;
}

// ------------------------------------------------------------------ the 8-probe read (M2: the bounce and the surfaces)
//! the read's red bits: the twin's (src/rtddgi.cpp kRedNoCheb / kRedNoBias, ddgi_common.glsl DDGI_RED_*) and the cell's
constexpr unsigned kSampleTrilinear = 1u;   // WW_CELL_RTDDGI_RED=trilinear: the trilinear weight alone (no backface, no Chebyshev, no crush)
constexpr unsigned kSampleNoCheb = 2u;      // no Chebyshev factor
constexpr unsigned kSampleNoBias = 4u;      // no self-shadow bias
//! a probe the read takes: Vigilant and blended at least once (Asleep probes are not traced, Off ones sit in walls)
inline bool probeUsable( int state, uint32_t age )
{
	return state == kVigilant && age > 0;
}
/*! A volume as the read sees it: the GPU's buffers (one probe after another: irr 8 x 8 x 4 floats, dist 16 x 16 x 2,
 *  probes 4 = offset xyz + state, age 1), the first probe, the spacing, the probe counts, D = the smallest spacing and
 *  B = the self-shadow bias. */
struct SampleVolume
{
	const float * irr = nullptr;
	const float * dist = nullptr;
	const float * probes = nullptr;
	const uint32_t * age = nullptr;
	V3 origin, step;
	int dims[3] = { 0, 0, 0 };
	float D = 1.0f, B = kSelfShadowBias;
};
//! E = irradiance (decoded x 2 pi), vis = sky visibility, w = the weights' sum (0: no probe usable), fade = see below
struct SampleResult
{
	V3 E;
	float vis = 0.0f, w = 0.0f, fade = 0.0f;
};
/*! RTXCASCADE1 (2026-10-30), one axis of a volume's blend weight at grid coordinate g (0 at the first probe, dims - 1
 *  at the last): 1 inside, falling to 0 over the last cell [J21 7.3]; a camera-locked volume tightens the region by
 *  one cell (tight: 0 over the outer cell, where the plane a scroll has just reset sits, then 0 -> 1 over the next).
 *  M2's fade is the untightened one (sampleVolume's tight = false).  GLSL: cell_ddgi.glsl fade lines (CELL_DDGI_TIGHT) */
inline float cascadeEdge( float g, int dims, bool tight )
{
	const float e = std::min( g, float( dims - 1 ) - g );
#ifdef RTDDGI_RED_CASCTIGHT
	(void)tight;
	return clampf( e, 0.0f, 1.0f );   // red: a camera-locked volume blends over its outer cell (the fresh plane) too
#else
	return clampf( tight ? e - 1.0f : e, 0.0f, 1.0f );
#endif
}
/*! The irradiance at surface point P (normal n, o = unit direction to the viewer, or back along the ray for a hit)
 *  from the 8 probes of the cage round the biased point P' = P + biasVector ([J19 5.2], [J21 4.1 eq 2], GUIDE 3):
 *  per probe w = backfaceWeight(dir P -> probe) x max(0.05, Chebyshev(|P' - probe|) at dir probe -> P') floored at
 *  1e-6, crushed, x the trilinear weight of P' in the cage; probes that are not usable weigh 0. The tiles are read at
 *  direction d (n for the irradiance; a reflection's lobe for the sky visibility): bilinear on the ENCODED value,
 *  then e^2.5 per probe, the weighted mean, squared, x 2 pi ([J21] fig 4: the blend's gamma 5). vis is the weighted
 *  mean of the tile's alpha (linear, M1FIX). fade (ours, M2): 1 one cell inside the probe box, falling to 0 at its
 *  faces and past them (the cell view hands over to the baked grid there). The twin's bounce (traceRay) and the
 *  GLSL read (res/shaders/cell_ddgi.glsl cellDdgiRead, the unit test's "sample" check) both follow it.
 *  GLSL: trace.comp ddgiIrradianceAt (d = n, no vis, no fade), res/shaders/cell_ddgi.glsl cellDdgiRead.
 *  RTXCASCADE1: tight = a cascade's read (cascadeEdge's camera-locked fade; cell_ddgi.glsl cellDdgiReadOne) */
inline SampleResult sampleVolume( const SampleVolume & v, V3 P, V3 n, V3 o, V3 d, unsigned red, bool tight = false )
{
	SampleResult out;
	V3 Pb = P;
	if ( !( red & kSampleNoBias ) )
		Pb = Pb + biasVector( n, o, v.D, v.B );
	int base[3];
	float a[3];
	float fade = 1.0f;
	for ( int k = 0; k < 3; k++ ) {
		const float g = ( Pb[k] - v.origin[k] ) / v.step[k];
		base[k] = std::min( std::max( int( std::floor( g ) ), 0 ), std::max( v.dims[k] - 2, 0 ) );
		a[k] = clampf( g - float( base[k] ), 0.0f, 1.0f );
		fade = std::min( fade, cascadeEdge( g, v.dims[k], tight ) );
	}
	out.fade = fade;
	double sum[3] = { 0, 0, 0 }, vsum = 0, wsum = 0;
	for ( int c = 0; c < 8; c++ ) {
		const int off[3] = { c & 1, ( c >> 1 ) & 1, ( c >> 2 ) & 1 };
		int pc[3];
		for ( int k = 0; k < 3; k++ )
			pc[k] = std::min( base[k] + off[k], v.dims[k] - 1 );
		const int p = pc[0] + v.dims[0] * ( pc[1] + v.dims[1] * pc[2] );
		const float * pr = &v.probes[size_t( p ) * 4];
		if ( !probeUsable( int( pr[3] + 0.5f ), v.age[size_t( p )] ) )
			continue;
		float trilinear = 1.0f;
		for ( int k = 0; k < 3; k++ )
			trilinear *= off[k] ? a[k] : 1.0f - a[k];
		const V3 pp( v.origin.x + float( pc[0] ) * v.step.x + pr[0], v.origin.y + float( pc[1] ) * v.step.y + pr[1],
			v.origin.z + float( pc[2] ) * v.step.z + pr[2] );
		float w = 1.0f;
		if ( !( red & kSampleTrilinear ) ) {
#ifdef RTDDGI_RED_SAMPLE
			w = 1.0f;   // red: the backface weight dropped (the GLSL read keeps it: the "sample" check must fail)
#else
			w = backfaceWeight( normalize( pp - P ), n );
#endif
			const V3 bToProbe = pp - Pb;
			const float r = length( bToProbe );
			if ( !( red & kSampleNoCheb ) && r > 0.0f ) {
				float m[2];
				sampleTile( &v.dist[size_t( p ) * kDistTile * kDistTile * 2], kDistTile, 2, bToProbe * ( -1.0f / r ), m );
				w *= chebyshevWeight( r, m[0], m[1] );
			}
			w = std::max( w, 1e-6f );
			w = crushWeight( w );
		}
		w *= trilinear;
		float e[4];
		sampleTile( &v.irr[size_t( p ) * kIrrTile * kIrrTile * 4], kIrrTile, 4, d, e );
		for ( int k = 0; k < 3; k++ )
			sum[k] += double( w ) * std::pow( std::max( e[k], 0.0f ), 2.5f );
		vsum += double( w ) * e[3];
		wsum += w;
	}
	if ( wsum <= 0.0 )
		return out;
	const V3 m( float( sum[0] / wsum ), float( sum[1] / wsum ), float( sum[2] / wsum ) );
	out.E = m * m * ( 2.0f * kPi );
	out.vis = float( vsum / wsum );
	out.w = float( wsum );
	return out;
}

// ------------------------------------------------------------------ RTXCASCADE1: nested camera-following volumes [J21 7.3]
/*! Cascades (GUIDE 4, [J21 7.3]): nested axis-aligned volumes round the camera, each 2x coarser than the one inside
 *  it; a surface samples them dense to coarse and stops once the weights reach 1. Cascade 0 is M3's volume; the
 *  coarser ones reach the far buildings M3's window leaves on the flat ambient. */
constexpr int kMaxCascades = 4;   // ours: 4 x 2^3 = 8x the near window's reach
/*! cascade `level`'s spacing (and window) as a multiple of cascade 0's: 2^level on every axis [J21 7.3 "each 2x
 *  coarser"]. With the lattice at whole multiples of the step (packVolume's floor), every coarse probe sits on a
 *  fine one.  GLSL: none (the C++ sizes the volumes; src/rtddgicell.cpp) */
inline double cascadeScale( int level )
{
#ifdef RTDDGI_RED_CASCSCALE
	return 1.0 + double( level );   // red: each cascade one step coarser, not twice as coarse
#else
	return std::ldexp( 1.0, level );
#endif
}
/*! the frames cascade `level` updates on (ours, PLAN.md cost lever "far ones update less often"): cascade 0 every
 *  frame, cascade L every `every`^L frames, at frame % every^L == L so the coarse ones take turns (for every >= 4
 *  and L <= 3 no two coarse cascades share a frame).  GLSL: none (src/rtddgicell.cpp's frame tick) */
inline bool cascadeDue( int level, uint64_t frame, int every )
{
	if ( level <= 0 )
		return true;
	uint64_t period = 1;
	for ( int l = 0; l < level; l++ )
		period *= uint64_t( std::max( every, 1 ) );
#ifdef RTDDGI_RED_CASCDUE
	period = uint64_t( std::max( every, 1 ) );   // red: every coarse cascade on the same short period (no "less often")
#endif
	return frame % period == uint64_t( level ) % period;
}
/*! RTXCASCADE2 (2026-10-30) cost lever "fewer rays for coarse cascades" (Flax; PLAN.md): cascade 0 traces `rays`, a
 *  coarse cascade `crays` when that is set (0 = the same as cascade 0: RTXCASCADE1's), clamped to the volume's range
 *  (kFixedRays + 1 .. kMaxRays).  GLSL: none (src/rtddgicell.cpp sizes each cascade's spec) */
inline int cascadeRays( int rays, int crays, int level )
{
#ifdef RTDDGI_RED_CASCRAYS
	(void)crays;
	(void)level;
	return rays;   // red: every cascade traces cascade 0's count (RTXCASCADE1's)
#else
	if ( level <= 0 || crays <= 0 )
		return rays;
	return std::min( std::max( crays, kFixedRays + 1 ), kMaxRays );
#endif
}
/*! RTXCASCADE2 cost lever "skip a coarse probe a finer cascade covers whole": the coarse volume's probes (first probe
 *  oC, step sC, dims dC; one axis) the read never takes because the finer cascade inside it holds there. sampleCascades
 *  reads dense to coarse and stops once the weights reach 1: where the finer one's tight fade (cascadeEdge, tight) is 1
 *  the coarser one takes nothing. A coarse probe at q weighs in the read of points in (q - sC, q + sC) only (its
 *  trilinear cage), and the finer fade is 1 on [oF + 2 sF, oF + (dF - 3) sF] (tight: e - 1 >= 1). So probe i is
 *  covered whole when q - sC >= oF + 2 sF and q + sC <= oF + (dF - 3) sF. Out: the covered indices lo..hi (inclusive);
 *  false when none. Caveat (REASONED, why it is a knob): where the finer cascade has no usable probe at a point (w = 0)
 *  the read falls through to the coarse one, which is then not updated.  GLSL: ddgi_common.glsl ddgiSkipped (the
 *  kernels read the range from the params) */
inline bool cascadeCoveredAxis( double oC, double sC, int dC, double oF, double sF, int dF, int & lo, int & hi )
{
	lo = 0;
	hi = -1;
#ifdef RTDDGI_RED_CASCSKIP
	(void)oC; (void)sC; (void)dC; (void)oF; (void)sF; (void)dF;
	return false;   // red: every coarse probe is traced
#else
	if ( sC <= 0.0 || sF <= 0.0 || dF < 6 || dC < 1 )
		return false;
	const double fLo = oF + 2.0 * sF, fHi = oF + double( dF - 3 ) * sF;
	const double eps = 1e-6 * sC;
	lo = std::max( 0, int( std::ceil( ( fLo + sC - oC ) / sC - 1e-9 ) ) );
	hi = std::min( dC - 1, int( std::floor( ( fHi - sC - oC ) / sC + 1e-9 ) ) );
	while ( lo <= hi && oC + double( lo ) * sC - sC < fLo - eps )
		lo++;
	while ( hi >= lo && oC + double( hi ) * sC + sC > fHi + eps )
		hi--;
	return lo <= hi;
#endif
}
//! the three axes of cascadeCoveredAxis: a box of covered coarse probes (lo..hi inclusive per axis); false when empty
inline bool cascadeCovered( const double oC[3], const double sC[3], const int dC[3], const double oF[3], const double sF[3],
	const int dF[3], int lo[3], int hi[3] )
{
	bool any = true;
	for ( int k = 0; k < 3; k++ )
		any = cascadeCoveredAxis( oC[k], sC[k], dC[k], oF[k], sF[k], dF[k], lo[k], hi[k] ) && any;
	return any;
}
/*! RTXCASCADE2: a camera jump. Cascade 0 scrolls `cells` whole cells this frame; when any axis moves its whole size
 *  (|cells| >= dims: no plane is kept, scrollRuns keeps nothing) the camera jumped (a teleport, a new place), not
 *  walked. Then every cascade steps this frame (whatever its period, cascadeDue) and resets every plane (no kept
 *  plane, so no hysteresis against the old place: the first blend of a fresh probe takes its update whole). Walking
 *  keeps RTXCASCADE1's rule: a coarse cascade scrolls on its own frames and keeps its planes.  GLSL: none */
inline bool cascadeJump( const int cells[3], const int dims[3] )
{
#ifdef RTDDGI_RED_CASCJUMP
	(void)cells;
	(void)dims;
	return false;   // red: no jump rule (RTXCASCADE1: the coarse ones wait for their frames and keep their planes)
#else
	for ( int k = 0; k < 3; k++ )
		if ( dims[k] > 0 && std::abs( cells[k] ) >= dims[k] )
			return true;
	return false;
#endif
}
//! RTXCASCADE2: what cascade `level` does on `frame` (cascadeDue, cascadeJump): step it (due), reset every plane (resetAll)
inline void cascadeStepPlan( int level, uint64_t frame, int every, bool jump, bool & due, bool & resetAll )
{
	due = cascadeDue( level, frame, every ) || jump;
	resetAll = jump && level > 0;   // cascade 0's own jump already resets every plane (scrollRuns keeps none)
}
/*! [J21 7.3] the read over n cascades (v[0] the densest): each one's weight is its tight fade (cascadeEdge, the
 *  camera-locked blend), and it takes that share of what the denser ones left: take = w x left, left -= take, stop
 *  when left is 0. A cascade with no usable probe at P (w = 0) takes nothing. E and vis = the takes' weighted mean;
 *  w = the weights' sum over the cascades read (0: none); fade = the takes' sum (the share the volumes cover: the
 *  caller's flat ambient takes the rest, as past M3's box).  GLSL: res/shaders/cell_ddgi.glsl cellDdgiRead under
 *  WW_CELL_RTDDGI_CASCADES */
inline SampleResult sampleCascades( const SampleVolume * v, int n, V3 P, V3 nrm, V3 o, V3 d, unsigned red )
{
	SampleResult out;
	double E[3] = { 0, 0, 0 }, vis = 0, acc = 0, wsum = 0, left = 1.0;
	for ( int i = 0; i < n; i++ ) {
#ifdef RTDDGI_RED_CASCADE
		const int c = n - 1 - i;   // red: coarse to dense (the far volume's value wins where the near one holds)
#else
		const int c = i;
#endif
		const SampleResult r = sampleVolume( v[c], P, nrm, o, d, red, true );
		if ( !( r.w > 0.0f && r.fade > 0.0f ) )
			continue;
		const double take = double( r.fade ) * left;
		E[0] += take * r.E.x;
		E[1] += take * r.E.y;
		E[2] += take * r.E.z;
		vis += take * r.vis;
		wsum += r.w;
		acc += take;
		left -= take;
		if ( left <= 0.0 )
			break;
	}
	out.w = float( wsum );
	out.fade = float( acc );
	if ( acc > 0.0 ) {
		out.E = V3( float( E[0] / acc ), float( E[1] / acc ), float( E[2] / acc ) );
		out.vis = float( vis / acc );
	}
	return out;
}

// ------------------------------------------------------------------ relocation and classification [J21 5, 6; SDK doc]
/*! one relocation iteration of a probe from its fixed rays (dirs, signed stored distances: a backface hit stores
 *  -0.2 t, a miss kMiss), its current offset and its cell (step per axis). [J21 fig 8]: backface share > 0.25 ->
 *  move along the closest backface's vector (its true distance, |d| / 0.2) as far as the offset limit (0.45 of the
 *  cell per axis) allows, only if that passes the face; else, when the closest front face is nearer than
 *  kRelocMinFront x the smallest step (ours) and the farthest direction is not the closest's (dot <= 0.5), step
 *  min(0.2 x step, farthest distance) toward the farthest, clamped to the limit (ours: clamp, not refuse).
 *  GLSL: reloc.comp mode 0 */
inline V3 relocateStep( const V3 * dirs, const float * dist, int n, V3 offset, V3 step )
{
	int back = 0, iBack = -1, iNear = -1, iFar = -1;
	float dBack = 1e30f, dNear = 1e30f, dFar = -1.0f;
	for ( int i = 0; i < n; i++ ) {
		const float d = dist[i];
		if ( d < 0.0f ) {
			back++;
			const float tr = -d / kBackfaceDist;
			if ( tr < dBack ) { dBack = tr; iBack = i; }
			continue;
		}
		if ( d < dNear ) { dNear = d; iNear = i; }
		if ( d > dFar ) { dFar = d; iFar = i; }
	}
	const V3 lim = step * kRelocLimit;
	const float mn = std::min( step.x, std::min( step.y, step.z ) );
	if ( float( back ) / float( n ) > kBackfaceShare && iBack >= 0 ) {
#ifdef RTDDGI_RED_RELOC
		return offset;   // red: a probe inside a wall never moves through it
#endif
		const V3 v = dirs[iBack] * dBack;
		float s = 1e30f;
		for ( int k = 0; k < 3; k++ ) {
			if ( std::fabs( v[k] ) < 1e-9f )
				continue;
			const float a = ( lim[k] - offset[k] ) / v[k], b = ( -lim[k] - offset[k] ) / v[k];
			s = std::min( s, std::max( a, b ) );
		}
		s -= 1e-3f;
		if ( s > 1.0f && s < 1e29f )
			return offset + v * s;
		return offset;
	}
	if ( iNear < 0 || iFar < 0 || dNear >= kRelocMinFront * mn || dot( dirs[iFar], dirs[iNear] ) > 0.5f )
		return offset;
	V3 o = offset + dirs[iFar] * std::min( kRelocStep * mn, dFar );
	for ( int k = 0; k < 3; k++ )
		o[k] = clampf( o[k], -lim[k], lim[k] );
	return o;
}
/*! a probe's state from its fixed rays ([SDK doc] two phases, [J21 6]): backface share > 0.25 -> Off; else any hit
 *  nearer than where its ray leaves the probe's voxel (the box of +-step about the probe) -> Vigilant; else Asleep.
 *  GLSL: reloc.comp mode 1 */
inline int classify( const V3 * dirs, const float * dist, int n, V3 step )
{
	int back = 0;
	for ( int i = 0; i < n; i++ )
		back += dist[i] < 0.0f ? 1 : 0;
#ifdef RTDDGI_RED_CLASS
	if ( float( back ) / float( n ) > 0.75f )   // red: a probe with half its rays on backfaces stays on
#else
	if ( float( back ) / float( n ) > kBackfaceShare )
#endif
		return kOff;
	for ( int i = 0; i < n; i++ ) {
		const float d = std::fabs( dist[i] );
		if ( d >= kMiss * 0.5f )
			continue;
		float leave = 1e30f;
		for ( int k = 0; k < 3; k++ )
			if ( std::fabs( dirs[i][k] ) > 1e-6f )
				leave = std::min( leave, step[k] / std::fabs( dirs[i][k] ) );
		if ( d < leave )
			return kVigilant;
	}
	return kAsleep;
}

// ------------------------------------------------------------------ M3: the volume follows the camera (scrolling)
/*! [SDK doc, DDGIVolume.md "Infinite Scrolling Movement"] the volume moves by whole probe cells: the interior probes
 *  keep their values, the planes that come in are reset and converge again. The SDK keeps the probes in place and
 *  offsets the index (a ring); ours moves the kept probes' data in the device's
 *  buffers (src/rtddgi/rtddgi_vk.inc), so probe p stays grid point p and the read (sampleVolume, cell_ddgi.glsl) and
 *  every kernel keep their indexing: no ring offset in the 13 inlined read sites of fo4_cell (skill
 *  ww-shader-link-limit). The rules here: where the window wants to be, how many cells that is, and which old probe
 *  each new grid point keeps. */

//! one axis: the low corner of a window `extent` wide centred on the eye, kept inside [gridLo, gridHi] when it fits
//! (else gridLo: the window covers the grid on this axis and never scrolls on it)
inline double scrollWindowLo( double eye, double gridLo, double gridHi, double extent )
{
	const double top = gridHi - extent;
	if ( top <= gridLo )
		return gridLo;
	const double lo = eye - 0.5 * extent;
	return lo < gridLo ? gridLo : lo > top ? top : lo;
}
//! one axis: whole cells from the volume's first probe (a multiple of step, packVolume's floor) to the window's
inline int scrollCells( double origin, double windowLo, double step )
{
	return int( std::floor( windowLo / step ) - std::floor( origin / step + 0.5 ) );
}
/*! after a scroll by s cells (new first probe = old first probe + s steps) the old probe that new grid point (x, y, z)
 *  keeps (same world point), or -1: a fresh probe (its point was outside the old volume) */
inline int scrollSource( const int dims[3], const int s[3], int x, int y, int z )
{
#ifdef RTDDGI_RED_SCROLL
	const int ox = x - s[0], oy = y - s[1], oz = z - s[2];   // red: the shift taken the wrong way round
#else
	const int ox = x + s[0], oy = y + s[1], oz = z + s[2];
#endif
	if ( ox < 0 || oy < 0 || oz < 0 || ox >= dims[0] || oy >= dims[1] || oz >= dims[2] )
		return -1;
	return ox + dims[0] * ( oy + dims[1] * oz );
}
/*! the kept probes as copy runs, run( old first probe, new first probe, count ), contiguous along x (one a kept
 *  (y, z) row); returns the fresh count. The GPU copies exactly these (src/rtddgi.cpp RtDdgiLive::step) */
template <class F>
inline int scrollRuns( const int dims[3], const int s[3], F && run )
{
	int kept = 0;
	for ( int z = 0; z < dims[2]; z++ )
		for ( int y = 0; y < dims[1]; y++ ) {
			int x = 0;
			while ( x < dims[0] ) {
				const int src = scrollSource( dims, s, x, y, z );
				if ( src < 0 ) {
					x++;
					continue;
				}
				int n = 1;
				while ( x + n < dims[0] && scrollSource( dims, s, x + n, y, z ) == src + n )
					n++;
				run( src, x + dims[0] * ( y + dims[1] * z ), n );
				kept += n;
				x += n;
			}
		}
	return dims[0] * dims[1] * dims[2] - kept;
}

// ------------------------------------------------------------------ the light at a hit (probegi's rules)
/*! PRTP2 section 1, the game's curve (probegi.cpp radial(), bit for bit in double there; float here): x = d / r,
 *  k = 1 - clamp(scale x^exp + bias, 0, 1), a = k^2.2.  GLSL: ddgiRadial */
inline float lightRadial( float d, float r, float bias, float scale, float expo )
{
	const float x = clampf( d / std::max( r, 0.001f ), 0.0f, 1.0f );
	const float xe = expo > 0.0f ? std::pow( x, expo ) : 1.0f;
	const float k = 1.0f - clampf( scale * xe + bias, 0.0f, 1.0f );
	return std::pow( k, 2.2f );
}
//! probegi's spot cone: base = clamp(1 - (1 - dl) / (1 - cosOuter)), a *= min(base^cone, 1).  GLSL: ddgiSpot
inline float spotCone( float dl, float cosOuter, float cone )
{
	const float base = clampf( 1.0f - ( 1.0f - dl ) / std::max( 1.0f - cosOuter, 1e-4f ), 0.0f, 1.0f );
	return std::min( std::pow( base, std::max( cone, 1e-3f ) ), 1.0f );
}

/*! The sky as a radiance cube R (six axis colours, L(d) = sum_k d_k^2 R[axis k, side of d]) solved so a probe open
 *  to the whole sky takes exactly the weather's per-axis irradiance amb on each axis (ours). Over the hemisphere of
 *  +X: E = pi/2 R+X + pi/8 (R+Y + R-Y + R+Z + R-Z), so per axis pair a = R+ + R-, b = R+ - R-:
 *  b = 2 (E+ - E-) / pi, S = sum a = sum E / pi, a = 4 (E+ + E-) / pi - S. Negative parts (a very one-sided
 *  weather) clamp to 0 and are counted in the return.  GLSL: the C++ solves it, the shader reads R */
inline int skyRadianceCube( const float amb[6][3], float R[6][3] )
{
	int clamped = 0;
	for ( int c = 0; c < 3; c++ ) {
		float sumE = 0.0f;
		for ( int f = 0; f < 6; f++ )
			sumE += amb[f][c];
		const float S = sumE / kPi;
		for ( int k = 0; k < 3; k++ ) {
			const float Ep = amb[k * 2][c], Em = amb[k * 2 + 1][c];
#ifdef RTDDGI_RED_SKY
			const float a = 2.0f * ( Ep + Em ) / kPi, b = 0.0f;   // red: the naive amb / pi per axis
			(void)S;
#else
			const float a = 4.0f * ( Ep + Em ) / kPi - S, b = 2.0f * ( Ep - Em ) / kPi;
#endif
			float rp = 0.5f * ( a + b ), rm = 0.5f * ( a - b );
			if ( rp < 0.0f ) { rp = 0.0f; clamped++; }
			if ( rm < 0.0f ) { rm = 0.0f; clamped++; }
			R[k * 2][c] = rp;
			R[k * 2 + 1][c] = rm;
		}
	}
	return clamped;
}
//! the sky radiance of direction d from the cube.  GLSL: ddgiSky
inline void skyRadiance( const float R[6][3], V3 d, float out[3] )
{
	const int fx = d.x >= 0.0f ? 0 : 1, fy = d.y >= 0.0f ? 2 : 3, fz = d.z >= 0.0f ? 4 : 5;
	for ( int c = 0; c < 3; c++ )
		out[c] = d.x * d.x * R[fx][c] + d.y * d.y * R[fy][c] + d.z * d.z * R[fz][c];
}

// ------------------------------------------------------------------ lane SKYBELOW1 (bungo 10-30): below the horizon is ground
/*! the ground a below-horizon miss sees (Unreal SkyLight "Lower Hemisphere Is Solid Color", Unity gradient ambient
 *  "ground"): an exterior's ground radiance = its albedo x the sky irradiance from above (eUp, what an up-facing open
 *  surface takes: the weather's +Z amb) / pi; an interior's = 0 (the void under a cell is never sky).  C++ only: the
 *  kernels read the result (Params sky[1..3].w) */
inline void groundRadiance( const float albedo[3], const float eUp[3], bool interior, float out[3] )
{
	for ( int c = 0; c < 3; c++ )
		out[c] = interior ? 0.0f : albedo[c] * eUp[c] / kPi;
}
/*! a miss's radiance: above the horizon the sky cube (skyRadiance, unchanged), below it the ground (rgb, 0 in an
 *  interior). rule false = the red WW_SKYBELOW_RED (today's main: the sky in every direction).  GLSL: ddgiMiss */
inline void missRadiance( const float R[6][3], const float ground[3], V3 d, bool rule, float out[3] )
{
#ifdef RTDDGI_RED_SKYBELOW
	rule = false;   // red: the sky below the horizon too
#endif
	if ( rule && belowHorizon( d ) ) {
		for ( int c = 0; c < 3; c++ )
			out[c] = ground[c];
		return;
	}
	skyRadiance( R, d, out );
}

} // namespace rtddgi

#endif // RTDDGIMATH_H
