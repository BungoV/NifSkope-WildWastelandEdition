#ifndef WW_PRECIPMATH_H
#define WW_PRECIPMATH_H

/* lane WEATHERLOOK1 phase B (2026-10-30): the precipitation field, the CPU code the cell view draws AND its unit test
 * (tests/precip). Plain C++, no Qt, no GL.
 *
 * Numbers and laws (notes/weatherlook1/airpart_data.md, sources there):
 *   fall speed (m/s, D in mm)  rain + sleet  v = 9.65 - 10.3 exp(-0.6 D), capped 9.17 (ATLAS73 / GK49; NF15: most ice
 *                                             pellets fall at raindrop speed)
 *                              hail          v = sqrt(4 g rho_ice D / (3 Cd rho_air)), Cd 0.6 (KK01, an upper bound)
 *                              dry snow      v = 0.8 D^0.16 (LH74 unrimed dendrite aggregates; extrapolated past 10 mm)
 *                              wet clumps    v = 0.79 D^0.27 (LH74 densely rimed; the pick over YUTER06's 2.0 m/s: it
 *                                             keeps the size dependence the atlas needs)
 *   mass (mg)                  rain 0.5236 D^3 (water), sleet + hail 0.9 x that (ice), dry snow 0.073 D^1.4,
 *                              wet clumps 0.037 D^1.9 (LH74)
 *   number density (per m^3)   N = rate x 277.78 / (m v): a rate of 1 mm/h of water carries 277.78 mg per m^2 per
 *                              second; every particle carries m and falls at v (one size per layer: ours)
 *   units                      1 game unit = 14.28 mm (airpart_data.md), so m/s x 70.03 = units/s
 *   wind                       the fog drift's law (cellvolair.cpp kDriftSpeed): 400 units/s x the weather's speed
 *                              byte / 255 at the game's wind bearing (gl/cellgrass.h wwGameWindAngle); every layer
 *                              rides it fully (its response time v/g is under a second: airpart_data.md "wind catch")
 *
 * The field: a box of side L centred on the camera, holding N = density x L^3 particles. Particle i of a layer has a
 * fixed start point in the box (a hash of i and the layer's seed) and moves at its own velocity (wind - fall, plus a
 * slow flutter for snow); its place at time t wraps into the box round the camera (world-anchored: walking moves the
 * camera through the field, never the field with it). N never changes with t: what leaves one face enters the
 * opposite one. Two tiers per layer:
 *   near  the true density, textured quads flat-on to the camera; L = cbrt(cap / density), kNearMin..kNearMax
 *   far   from the near box out to the far box, thinned to at most the far cap; each dot stands for `thin` particles,
 *         so its opacity is multiplied by thin (coverage kept), drawn from the texture's small mips
 * A weather change: the new weather's layers at weight pct, the weather left behind at 1 - pct; a layer at weight w
 * draws its first floor(w x N) particles (the rest wait), so its density moves with w and never jumps.
 * Reds (redBits, one per rule; WW_CELL_PRECIP_RED=constdensity,nowrap,onespeed,nowind; tests/precip): the rule removed. */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace WwPrecip
{

constexpr double kMmPerUnit = 14.28;
constexpr double kUnitsPerMetre = 1000.0 / kMmPerUnit;
constexpr double kM3PerU3 = ( kMmPerUnit / 1000.0 ) * ( kMmPerUnit / 1000.0 ) * ( kMmPerUnit / 1000.0 );
constexpr double kFluxPerMmH = 1.0e6 / 3600.0;	// mg of water per m^2 per s at 1 mm/h
constexpr double kWindUnitsPerSec = 400.0;	// the fog drift's (cellvolair.cpp kDriftSpeed)
constexpr double kShutter = 1.0 / 120.0;	// airpart_data.md draw stretch: 180 degrees at 60 fps
constexpr double kG = 9.81, kRhoIce = 900.0, kRhoAir = 1.2, kCdHail = 0.6;
constexpr double kNearMin = 128.0, kNearMax = 2048.0;	// game units
constexpr double kFarMax = 6144.0;
constexpr int kNearCap = 32768, kFarCap = 24576;
constexpr double kRedDensity = 100.0;	// per m^3: the red's constant

enum Type : int { Rain = 0, Sleet, Hail, SnowFine, SnowFarDot, SnowWetClump, SnowBigClump, TypeCount };

/* the reds, one per rule (WW_CELL_PRECIP_RED, tests/precip): 1 constdensity (a constant density, the weight ignored),
 * 2 nowrap (positions never wrap into the box), 4 onespeed (every particle falls at 1 m/s), 8 nowind (no wind) */
enum Red : int { RedConstDensity = 1, RedNoWrap = 2, RedOneSpeed = 4, RedNoWind = 8 };
inline int & redBits()
{
	static int bits = 0;
	return bits;
}

struct Variant
{
	double sizeMm;	// the particle's size (airpart_textures/<type>/<type>_meta.json size_mm)
	double cellMm;	// the atlas cell's width in mm (the quad's true size)
};

struct TypeInfo
{
	const char * name;
	const char * texture;	// under Textures\ (the default shipped in res/Data)
	int cols, rows;
	int nVariants;
	Variant v[16];
	double sizeMm;	// the layer's size when the .recx gives none
	double rateMmH;	// the layer's rate (water equivalent) when the .recx gives neither rate nor density
	double drawScale;	// the quad x this (bungo's Division reference: snow drawn above true size, airpart crowd mock 3x)
	double translucency;	// the share lit as thin medium (airpart_data.md: dry 0.6, wet 0.3; rain/sleet/hail ours)
	double spin;	// the largest spin about the view axis, rad/s (bungo 10-28: every crystal flat-on; slow spin)
	double swayU;	// the flutter's amplitude, game units (0: none)
	/* the deepest mip a near quad samples: below it the shapes touch their cell's border and bilinear reads bleed into
	 * the next cell (measured, notes/code1030/weatherlook1b/texgate.txt: 16 px a cell; hail 64 px). The far dots read
	 * the 1-texel-a-cell mip's centre (the cell's mean) instead, where no neighbour mixes in */
	int maxMip;
};

/* the atlases as phase A made them (meta.json, notes/weatherlook1/airpart_textures). snow_fine is the 1k 4 x 4 atlas
 * (A5); its cells are scaled per shape */
inline const TypeInfo & typeInfo( int t )
{
	static const TypeInfo kTypes[TypeCount] = {
		{ "rain", "WW/Precip/rain_d.dds", 4, 2, 8,
			{ { 0.5, 8 }, { 1, 8 }, { 1.5, 8 }, { 2, 8 }, { 3, 8 }, { 4, 8 }, { 5, 8 }, { 6, 8 } },
			2.0, 4.0, 1.0, 0.9, 0.0, 0.0, 4 },
		{ "sleet", "WW/Precip/sleet_d.dds", 4, 2, 8,
			{ { 1.2, 5.5 }, { 1.8, 5.5 }, { 2.2, 5.5 }, { 2.5, 5.5 }, { 3, 5.5 }, { 3.4, 5.5 }, { 2.0, 5.5 }, { 4.4, 5.5 } },
			2.5, 2.0, 1.0, 0.8, 1.0, 0.0, 4 },
		{ "hail", "WW/Precip/hail_d.dds", 4, 2, 8,
			{ { 5, 50 }, { 8, 50 }, { 12, 50 }, { 16, 50 }, { 20, 50 }, { 26, 50 }, { 34, 50 }, { 44, 50 } },
			10.0, 10.0, 1.0, 0.5, 2.0, 0.0, 2 },
		{ "snow_fine", "WW/Precip/snow_fine_d.dds", 4, 4, 16,
			{ { 3.0, 3.5572 }, { 3.0, 3.5698 }, { 3.0, 3.5572 }, { 3.0, 3.6575 }, { 3.93, 4.5971 }, { 4.1, 4.7982 },
				{ 4.59, 5.3648 }, { 4.55, 5.3144 }, { 5.5, 3.9031 }, { 5.5, 4.4517 }, { 5.5, 4.7008 }, { 5.5, 4.2866 },
				{ 10.0, 6.1308 }, { 10.0, 5.6934 }, { 10.0, 5.8648 }, { 10.0, 5.7326 } },
			5.5, 1.0, 3.0, 0.6, 0.5, 6.0, 4 },
		{ "snow_far_dot", "WW/Precip/snow_far_dot_d.dds", 4, 2, 8,
			{ { 4, 14 }, { 5, 14 }, { 6, 14 }, { 7, 14 }, { 8, 14 }, { 9, 14 }, { 10, 14 }, { 11, 14 } },
			6.0, 1.0, 3.0, 0.6, 0.0, 6.0, 4 },
		{ "snow_wet_clump", "WW/Precip/snow_wet_clump_d.dds", 4, 2, 8,
			{ { 5, 27 }, { 7, 27 }, { 9, 27 }, { 11, 27 }, { 14, 27 }, { 17, 27 }, { 21, 27 }, { 25, 27 } },
			10.0, 2.0, 2.0, 0.3, 0.3, 3.0, 4 },
		{ "snow_big_clump", "WW/Precip/snow_big_clump_d.dds", 4, 2, 8,
			{ { 15, 54 }, { 20, 54 }, { 25, 54 }, { 30, 54 }, { 35, 54 }, { 40, 54 }, { 45, 54 }, { 50, 54 } },
			20.0, 1.0, 1.5, 0.6, 0.4, 8.0, 4 },
	};
	return kTypes[std::clamp( t, 0, int( TypeCount ) - 1 )];
}

//! the type by its .recx word; -1 when none
inline int typeOf( const char * word )
{
	for ( int t = 0; t < TypeCount; t++ )
		if ( word && std::strcmp( word, typeInfo( t ).name ) == 0 )
			return t;
	return -1;
}

//! terminal fall speed in m/s of a particle of size D mm
inline double fallSpeedMps( int type, double D )
{
	if ( redBits() & RedOneSpeed )
		return 1.0;
	D = std::max( D, 0.05 );
	switch ( type ) {
	case Rain:
	case Sleet:
		return std::clamp( 9.65 - 10.3 * std::exp( -0.6 * D ), 0.1, 9.17 );
	case Hail:
		return std::sqrt( 4.0 * kG * kRhoIce * ( D / 1000.0 ) / ( 3.0 * kCdHail * kRhoAir ) );
	case SnowWetClump:
		return 0.79 * std::pow( D, 0.27 );
	default:
		return 0.8 * std::pow( D, 0.16 );
	}
}

//! mass in mg of a particle of size D mm
inline double massMg( int type, double D )
{
	switch ( type ) {
	case Rain:
		return 0.5236 * D * D * D;
	case Sleet:
	case Hail:
		return 0.9 * 0.5236 * D * D * D;
	case SnowWetClump:
		return 0.037 * std::pow( D, 1.9 );
	default:
		return 0.073 * std::pow( D, 1.4 );
	}
}

//! particles per m^3 that carry `rateMmH` of water at size D (red: the constant)
inline double densityFromRate( int type, double D, double rateMmH )
{
	if ( redBits() & RedConstDensity )
		return kRedDensity;
	const double mv = massMg( type, D ) * fallSpeedMps( type, D );
	return mv > 0.0 && rateMmH > 0.0 ? rateMmH * kFluxPerMmH / mv : 0.0;
}

//! a layer's weight in a weather change: the new weather's layers pct, the leaving weather's 1 - pct
inline double blendWeight( bool leaving, double pct )
{
	pct = std::clamp( pct, 0.0, 1.0 );
	return leaving ? 1.0 - pct : pct;
}

inline std::uint32_t hash32( std::uint32_t x )
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

inline double u01( std::uint32_t h )
{
	return double( h >> 8 ) * ( 1.0 / 16777216.0 );
}

//! the variant particle i takes: among the cells within a factor 1.6 of the layer's size, else the nearest
inline int variantFor( const TypeInfo & ti, double sizeMm, std::uint32_t h )
{
	int cand[16], n = 0, best = 0;
	double bestD = 1e30;
	for ( int k = 0; k < ti.nVariants; k++ ) {
		const double r = ti.v[k].sizeMm / std::max( sizeMm, 1e-3 );
		if ( r >= 1.0 / 1.6 && r <= 1.6 )
			cand[n++] = k;
		const double d = std::fabs( std::log( std::max( r, 1e-6 ) ) );
		if ( d < bestD ) {
			bestD = d;
			best = k;
		}
	}
	return n > 0 ? cand[h % std::uint32_t( n )] : best;
}

//! one tier's box: side (game units), particles allocated, and how many real particles one stands for
struct Box
{
	double side = 0.0;
	long count = 0;
	double thin = 1.0;
	double inner = 0.0;	// far tier: the near box it skips
};

//! the near box for `densityM3` particles per m^3 (cap: the most particles)
inline Box nearBox( double densityM3, int cap = kNearCap )
{
	Box b;
	const double dU = densityM3 * kM3PerU3;
	if ( !( dU > 0.0 ) )
		return b;
	b.side = std::clamp( std::cbrt( double( cap ) / dU ), kNearMin, kNearMax );
	const double n = dU * b.side * b.side * b.side;
	b.count = std::lround( std::min( n, double( cap ) ) );
	b.thin = b.count > 0 ? n / double( b.count ) : 1.0;
	return b;
}

//! the far box round the near one: 6 x its side (at least 2048 units, at most kFarMax), thinned to the far cap
inline Box farBox( double densityM3, const Box & nearB, int cap = kFarCap )
{
	Box b;
	const double dU = densityM3 * kM3PerU3;
	if ( !( dU > 0.0 ) || !( nearB.side > 0.0 ) || nearB.side >= kFarMax )
		return b;
	b.side = std::clamp( 6.0 * nearB.side, 2048.0, kFarMax );
	b.inner = nearB.side;
	if ( b.side <= b.inner )
		return Box();
	const double n = dU * b.side * b.side * b.side;	// allocated over the whole far cube; the inner ones are skipped
	b.count = std::lround( std::min( n, double( cap ) ) );
	b.thin = b.count > 0 ? n / double( b.count ) : 1.0;
	return b;
}

//! how many of a tier's allocated particles a layer at weight w draws (the first ones: monotonic in w)
inline long drawCount( long count, double w )
{
	if ( redBits() & RedConstDensity )
		return count;
	return long( std::floor( double( count ) * std::clamp( w, 0.0, 1.0 ) ) );
}

//! the wind's velocity (game units/s) from the speed byte (0..255, a float during a change) and the bearing (radians)
inline void windVelocity( double speedByte, double angle, double out[2] )
{
	const double v = ( redBits() & RedNoWind ) ? 0.0 : kWindUnitsPerSec * std::clamp( speedByte, 0.0, 255.0 ) / 255.0;
	out[0] = v * std::cos( angle );
	out[1] = v * std::sin( angle );
}

//! x wrapped into [0, L)
inline double wrap( double x, double L )
{
	double r = x - L * std::floor( x / L );
	if ( r >= L )
		r -= L;
	return r < 0.0 ? 0.0 : r;
}

struct Layer
{
	int type = Rain;
	double sizeMm = 2.0;
	double densityM3 = 0.0;	// full density (weight 1)
	double weight = 1.0;	// its weather's share
	double drawScale = 1.0;
	double windShare = 1.0;
	double spin = 0.0;
	double swayU = 0.0;
	std::uint32_t seed = 1;
	// prepare(): the per-variant numbers once a frame instead of once a particle (the same values, bit for bit)
	bool ready = false;
	int redAt = -1;
	int nCand = 0, best = 0, cand[16] = {};
	double fallU[16] = {}, halfU[16] = {};
};

//! fill the layer's per-variant table (call after any field change; particleAt falls back to the slow path without it)
inline void prepare( Layer & la )
{
	const TypeInfo & ti = typeInfo( la.type );
	la.nCand = 0;
	double bestD = 1e30;
	for ( int k = 0; k < ti.nVariants; k++ ) {
		const double r = ti.v[k].sizeMm / std::max( la.sizeMm, 1e-3 );
		if ( r >= 1.0 / 1.6 && r <= 1.6 )
			la.cand[la.nCand++] = k;
		const double d = std::fabs( std::log( std::max( r, 1e-6 ) ) );
		if ( d < bestD ) {
			bestD = d;
			la.best = k;
		}
		la.fallU[k] = fallSpeedMps( la.type, ti.v[k].sizeMm ) * kUnitsPerMetre;
		la.halfU[k] = 0.5 * ti.v[k].cellMm / kMmPerUnit * la.drawScale;
	}
	la.redAt = redBits();
	la.ready = true;
}

//! one particle's state at time t
struct Particle
{
	double pos[3];	// world
	double vel[3];	// world units/s
	double half;	// the quad's half size, world units (the cell's true size x drawScale / 2)
	double angle;	// about the view axis, radians
	int variant;
	bool inside;	// far tier: true when it sits inside the near box (skipped)
};

/* particle i of a tier (box b) of layer `la` at time t, the camera at cam (world), the wind wv (units/s) */
inline Particle particleAt( const Layer & la, const Box & b, long i, bool farTier, double t, const double cam[3],
	const double wv[2] )
{
	const TypeInfo & ti = typeInfo( la.type );
	const std::uint32_t s = hash32( la.seed * 0x9E3779B9U + ( farTier ? 0x68E31DA4U : 0U ) );
	std::uint32_t h = hash32( std::uint32_t( i ) ^ s );
	const double u0 = u01( h );
	h = hash32( h + 0x1B873593U );
	const double u1 = u01( h );
	h = hash32( h + 0x1B873593U );
	const double u2 = u01( h );
	h = hash32( h + 0x1B873593U );
	const bool fast = la.ready && la.redAt == redBits();
	const int var = fast ? ( la.nCand > 0 ? la.cand[h % std::uint32_t( la.nCand )] : la.best ) : variantFor( ti, la.sizeMm, h );
	h = hash32( h + 0x1B873593U );
	const double a0 = u01( h ) * 6.283185307179586;
	h = hash32( h + 0x1B873593U );
	const double spin = la.spin * ( 2.0 * u01( h ) - 1.0 );
	h = hash32( h + 0x1B873593U );
	const double ph = u01( h ) * 6.283185307179586, om = 0.8 + 0.8 * u01( hash32( h ^ 0x85EBCA6BU ) );

	Particle p;
	const double D = ti.v[var].sizeMm;
	const double fall = fast ? la.fallU[var] : fallSpeedMps( la.type, D ) * kUnitsPerMetre;
	p.vel[0] = wv[0] * la.windShare;
	p.vel[1] = wv[1] * la.windShare;
	p.vel[2] = -fall;
	const double sx = la.swayU * std::sin( om * t + ph ), sy = la.swayU * std::cos( 0.7 * om * t + 1.3 * ph );
	// the flutter's own speed joins the stretch (its derivative)
	p.vel[0] += la.swayU * om * std::cos( om * t + ph );
	p.vel[1] -= la.swayU * 0.7 * om * std::sin( 0.7 * om * t + 1.3 * ph );
	const double L = b.side;
	const double base[3] = { u0 * L, u1 * L, u2 * L };
	const double drift[3] = { wv[0] * la.windShare * t + sx, wv[1] * la.windShare * t + sy, -fall * t };
	p.inside = false;
	for ( int k = 0; k < 3; k++ ) {
		const double lo = cam[k] - 0.5 * L;
		p.pos[k] = ( redBits() & RedNoWrap ) ? base[k] + drift[k] : lo + wrap( base[k] + drift[k] - lo, L );
		if ( farTier )
			p.inside = k == 0 ? std::fabs( p.pos[k] - cam[k] ) < 0.5 * b.inner
				: ( p.inside && std::fabs( p.pos[k] - cam[k] ) < 0.5 * b.inner );
	}
	p.half = fast ? la.halfU[var] : 0.5 * ti.v[var].cellMm / kMmPerUnit * la.drawScale;
	p.angle = a0 + spin * t;
	p.variant = var;
	return p;
}

/* the draw's motion stretch (airpart_data.md "draw-time stretch"): a quad of half size s moving `travel` (same units)
 * over the shutter is drawn s + travel / 2 long, its opacity x 2s / (2s + travel) (the box average keeps coverage) */
inline double stretchCoverage( double half, double travel )
{
	return half > 0.0 ? 2.0 * half / ( 2.0 * half + std::max( travel, 0.0 ) ) : 0.0;
}

// ---- the shipped textures: a DDS header read (the loader's and the unit's)
struct DdsInfo
{
	int width = 0, height = 0, mips = 0;
	int blockBytes = 0;	// 8 BC1, 16 BC3 / BC7
	bool srgb = false;
	unsigned dxgi = 0;	// 71/72 BC1, 77/78 BC3, 98/99 BC7 (a legacy DXT1/DXT5 header maps to 71/77)
	std::size_t dataOffset = 0;
	std::size_t dataBytes = 0;	// every mip's bytes
};

inline unsigned rd32( const unsigned char * p )
{
	return unsigned( p[0] ) | ( unsigned( p[1] ) << 8 ) | ( unsigned( p[2] ) << 16 ) | ( unsigned( p[3] ) << 24 );
}

//! the bytes of mip level m of a block-compressed w x h texture
inline std::size_t ddsMipBytes( int w, int h, int m, int blockBytes )
{
	const int mw = std::max( 1, w >> m ), mh = std::max( 1, h >> m );
	return std::size_t( ( mw + 3 ) / 4 ) * std::size_t( ( mh + 3 ) / 4 ) * std::size_t( blockBytes );
}

//! false when the file is not a block-compressed DDS this loader takes, or is cut short
inline bool ddsParse( const unsigned char * b, std::size_t n, DdsInfo & o )
{
	o = DdsInfo();
	if ( n < 128 || std::memcmp( b, "DDS ", 4 ) != 0 || rd32( b + 4 ) != 124 )
		return false;
	o.height = int( rd32( b + 12 ) );
	o.width = int( rd32( b + 16 ) );
	o.mips = std::max( 1, int( rd32( b + 28 ) ) );
	const unsigned four = rd32( b + 84 );
	auto cc = []( const char * s ) { return unsigned( s[0] ) | ( unsigned( s[1] ) << 8 ) | ( unsigned( s[2] ) << 16 ) | ( unsigned( s[3] ) << 24 ); };
	o.dataOffset = 128;
	if ( four == cc( "DX10" ) ) {
		if ( n < 148 )
			return false;
		o.dxgi = rd32( b + 128 );
		o.dataOffset = 148;
	} else if ( four == cc( "DXT1" ) ) {
		o.dxgi = 71;
	} else if ( four == cc( "DXT5" ) ) {
		o.dxgi = 77;
	} else {
		return false;
	}
	switch ( o.dxgi ) {
	case 71: case 72: o.blockBytes = 8; break;
	case 77: case 78: case 98: case 99: o.blockBytes = 16; break;
	default: return false;
	}
	o.srgb = o.dxgi == 72 || o.dxgi == 78 || o.dxgi == 99;
	if ( o.width <= 0 || o.height <= 0 || o.width > 16384 || o.height > 16384 || o.mips > 15 )
		return false;
	for ( int m = 0; m < o.mips; m++ )
		o.dataBytes += ddsMipBytes( o.width, o.height, m, o.blockBytes );
	return o.dataOffset + o.dataBytes <= n;
}

}	// namespace WwPrecip

#endif	// WW_PRECIPMATH_H
