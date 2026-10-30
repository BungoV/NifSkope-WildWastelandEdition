#ifndef WW_CLOUDMATH_H
#define WW_CLOUDMATH_H

/* lane CLOUD1: the cloud map's CPU math, no GL and no Qt, shared by gl/cloudmap.cpp and the offline unit test
 * (tests/cloudmap/cloudmath_dump.cpp). The role table is notes/atmores1/RESEARCH.md sec. 5.3: typical heights, not
 * game data (the game's 2D layers have no height). */

#include <algorithm>
#include <cmath>
#include <cstring>

namespace WwCloudMath
{

enum Role { Fill = 0, Lower = 1, LowerLight = 2, Upper = 3, Horizon = 4, Wispy = 5, Other = 6, RoleCount = 7 };

struct RoleInfo
{
	int band;	// 0 Low / 1 Mid / 2 High
	bool sheet;	// a stratus sheet (Fill)
	float baseM, topM;	// metres above the eye
	float density;	// relative optical thickness, 1 = a plain cumulus
};

inline const RoleInfo & roleInfo( int r )
{
	static const RoleInfo k[RoleCount] = {
		{ 0, true, 600.0f, 2000.0f, 1.5f },	// Fill: stratus / nimbostratus sheet
		{ 0, false, 1000.0f, 2500.0f, 1.0f },	// Lower: stratocumulus / cumulus
		{ 0, false, 1200.0f, 1900.0f, 0.6f },	// LowerLight: thinner cumulus
		{ 1, false, 3000.0f, 5000.0f, 0.4f },	// Upper: altocumulus / altostratus
		{ 0, false, 1000.0f, 2500.0f, 1.0f },	// Horizon: the far cumulus ring (adds Low coverage at distance)
		{ 2, false, 8000.0f, 8500.0f, 0.2f },	// Wispy: cirrus (mods; vanilla has none)
		{ 0, false, 1000.0f, 2500.0f, 1.0f },	// Other: an unnamed sheet, read as Lower
	};
	return k[std::clamp( r, 0, RoleCount - 1 )];
}

//! role by the texture's file name (lower case), the census's rule (notes/cloud1/census/census.py role_of)
inline int roleOfName( const char * lowerName )
{
	const char * b = std::strrchr( lowerName, '/' );
	b = b ? b + 1 : lowerName;
	auto has = [b]( const char * s ) { return std::strstr( b, s ) != nullptr; };
	if ( has( "fill" ) || has( "overcast" ) || has( "fog" ) )
		return Fill;
	if ( has( "horizon" ) )
		return Horizon;
	if ( has( "lowerlight" ) )
		return LowerLight;
	if ( has( "lower" ) )
		return Lower;
	if ( has( "upper" ) )
		return Upper;
	if ( has( "wisp" ) || has( "cirr" ) || has( "streak" ) || has( "high" ) )
		return Wispy;
	return Other;
}

//! coverage of stacked independent layers: 1 - prod(1 - c_i); the red "max" takes the largest instead
inline float unionCover( const float * c, int n, bool redMax = false )
{
	if ( redMax ) {
		float m = 0.0f;
		for ( int i = 0; i < n; i++ )
			m = std::max( m, std::clamp( c[i], 0.0f, 1.0f ) );
		return m;
	}
	double keep = 1.0;
	for ( int i = 0; i < n; i++ )
		keep *= 1.0 - double( std::clamp( c[i], 0.0f, 1.0f ) );
	return float( 1.0 - keep );
}

//! the census rung of one layer: JNAM alpha x the texture's mean alpha x the layer mesh's share of the dome
inline float layerCover( float alpha, float meanA, float footprint )
{
	return std::clamp( alpha * meanA * footprint, 0.0f, 1.0f );
}

struct BandParams
{
	float coverage = 0.0f, baseM = 0.0f, topM = 0.0f, density = 0.0f, sheet = 0.0f;
	float tint[3] = { 1.0f, 1.0f, 1.0f };
	float type = 0.0f, typeSpread = 0.0f;	// lane CLOUD5: the Low band's cloud type 0..1 and its swing (lowTypeOf)
};

/* lane CLOUD5: the WTHR DATA flags byte (offset 11; xEdit wbDefinitionsFO4 WTHR DATA: Pleasant, Cloudy, Rainy, Snow, ...)
 * and the Thunder/Lightning Frequency byte (offset 10). Read from xEdit's definitions, not yet censused over the vanilla
 * weathers: drop/census_type.py (lane CLOUD5) prints every weather's two bytes and the type below. */
constexpr unsigned kWxPleasant = 1, kWxCloudy = 2, kWxRainy = 4, kWxSnow = 8;

/* lane CLOUD5: the Low band's cloud type (0 stratus, 1/3 stratocumulus, 2/3 cumulus, 1 cumulonimbus) and its swing over
 * the sky (the weather noise moves it by +- spread), from the record's class. Ours (no shipped source gives this map):
 * fair or unflagged -> cumulus-ish 0.6 +- 0.2 (stratocumulus to congestus); cloudy 0.45; rainy 0.75 (congestus, deep
 * rain clouds); rainy with thunder 0.9 +- 0.25 (cumulonimbus towers); snow 0.3 (stratiform). The sheet (Fill) layers
 * still pull a texel toward stratus through the map's B channel. recxType >= 0 (a weather .recx clouds.type) wins.
 * lane CLOUD6d: oneSet = ONE profile set per weather (WW_CLOUDVOL_PROFILESET, the default): the weather's type is
 * the type everywhere in its sky, spread 0, so a storm draws one cumulonimbus family and a fair sky one cumulus family
 * (RDR2: one height-LUT slice per weather, "slice determined by weather parameters", Bauer 2019 slide 16 + note 16).
 * Size variety stays with the coverage swing. oneSet false = CLOUD5's +- spread (towers beside puffs), the red. */
inline void lowTypeOf( unsigned flags, unsigned thunder, float recxType, float & type, float & spread, bool oneSet = false )
{
	type = 0.6f;
	spread = 0.2f;
	if ( flags & kWxSnow ) {
		type = 0.3f;
		spread = 0.15f;
	} else if ( flags & kWxRainy ) {
		type = thunder > 0 ? 0.9f : 0.75f;
		spread = thunder > 0 ? 0.25f : 0.2f;
	} else if ( flags & kWxCloudy ) {
		type = 0.45f;
	}
	if ( recxType >= 0.0f )
		type = std::clamp( recxType, 0.0f, 1.0f );
	if ( oneSet )
		spread = 0.0f;
}

/* the band parameters of one weather from its layers (role, cover); heights, density and tint are coverage-weighted
 * means, sheet = the share of the band's coverage that sheet layers give */
inline void bandsOf( const int * role, const float * cover, const float ( *tint )[3], int n, BandParams out[3],
	bool redMax = false, const float ( *heightM )[2] = nullptr )
{
	// lane WX1: heightM[i] = a layer's own base/top above the eye (a weather .recx clouds.layers entry), < 0 = its role's
	for ( int b = 0; b < 3; b++ ) {
		float cs[32], sheetC[32];
		int k = 0, ks = 0;
		double w = 0.0, base = 0.0, top = 0.0, den = 0.0, t[3] = { 0, 0, 0 };
		for ( int i = 0; i < n && k < 32; i++ ) {
			const RoleInfo & r = roleInfo( role[i] );
			if ( r.band != b || cover[i] <= 0.0f )
				continue;
			cs[k++] = cover[i];
			if ( r.sheet )
				sheetC[ks++] = cover[i];
			w += cover[i];
			// lane CLOUD4: heightM[i] = {base, top} m from the weather's .recx (top > base > 0), else the type's height; WX1: base/top above the eye from the same keys, < 0 = none
			const bool own = heightM && heightM[i][0] >= 0.0f && heightM[i][1] > heightM[i][0];
			base += cover[i] * ( own ? heightM[i][0] : r.baseM );
			top += cover[i] * ( own ? heightM[i][1] : r.topM );
			den += cover[i] * r.density;
			for ( int c = 0; c < 3; c++ )
				t[c] += cover[i] * ( tint ? tint[i][c] : 1.0f );
		}
		BandParams & p = out[b];
		p = BandParams();
		const RoleInfo & d = roleInfo( b == 0 ? Lower : b == 1 ? Upper : Wispy );
		p.baseM = d.baseM;
		p.topM = d.topM;
		if ( k == 0 )
			continue;
		p.coverage = unionCover( cs, k, redMax );
		p.sheet = p.coverage > 0.0f ? unionCover( sheetC, ks, redMax ) / p.coverage : 0.0f;
		p.baseM = float( base / w );
		p.topM = float( top / w );
		p.density = float( den / w );
		for ( int c = 0; c < 3; c++ )
			p.tint[c] = float( t[c] / w );
	}
}

//! a weather change blends the PARAMETERS (coverage, heights, density, sheet, tint as linear albedo), never colours
inline BandParams lerpParams( const BandParams & a, const BandParams & b, float t )
{
	t = std::clamp( t, 0.0f, 1.0f );
	auto L = [t]( float x, float y ) { return x + ( y - x ) * t; };
	BandParams o;
	o.coverage = L( a.coverage, b.coverage );
	// an empty band has no height of its own: take the other side's
	const bool ea = a.coverage <= 0.0f, eb = b.coverage <= 0.0f;
	o.baseM = ea ? b.baseM : eb ? a.baseM : L( a.baseM, b.baseM );
	o.topM = ea ? b.topM : eb ? a.topM : L( a.topM, b.topM );
	o.density = L( a.density, b.density );
	o.sheet = L( a.sheet, b.sheet );
	o.type = ea ? b.type : eb ? a.type : L( a.type, b.type );	// lane CLOUD5: as the heights (an empty band has none)
	o.typeSpread = ea ? b.typeSpread : eb ? a.typeSpread : L( a.typeSpread, b.typeSpread );
	for ( int c = 0; c < 3; c++ )
		o.tint[c] = L( a.tint[c], b.tint[c] );
	return o;
}

//! hue (degrees 0..360), saturation, value of a linear rgb
inline void hsv( const float rgb[3], float & h, float & s, float & v )
{
	const float mx = std::max( { rgb[0], rgb[1], rgb[2] } ), mn = std::min( { rgb[0], rgb[1], rgb[2] } );
	v = mx;
	s = mx > 0.0f ? ( mx - mn ) / mx : 0.0f;
	const float d = mx - mn;
	if ( d <= 0.0f ) {
		h = 0.0f;
		return;
	}
	if ( mx == rgb[0] )
		h = 60.0f * std::fmod( ( rgb[1] - rgb[2] ) / d + 6.0f, 6.0f );
	else if ( mx == rgb[1] )
		h = 60.0f * ( ( rgb[2] - rgb[0] ) / d + 2.0f );
	else
		h = 60.0f * ( ( rgb[0] - rgb[1] ) / d + 4.0f );
}

inline void rgbOfHsv( float h, float s, float v, float out[3] )
{
	h = std::fmod( std::fmod( h, 360.0f ) + 360.0f, 360.0f );
	const float c = v * s, x = c * ( 1.0f - std::fabs( std::fmod( h / 60.0f, 2.0f ) - 1.0f ) ), m = v - c;
	float r = 0, g = 0, b = 0;
	const int sec = int( h / 60.0f ) % 6;
	switch ( sec ) {
	case 0: r = c; g = x; break;
	case 1: r = x; g = c; break;
	case 2: g = c; b = x; break;
	case 3: g = x; b = c; break;
	case 4: r = x; b = c; break;
	default: r = c; b = x; break;
	}
	out[0] = r + m;
	out[1] = g + m;
	out[2] = b + m;
}

//! the red "colorblend": the tint blended as a colour (hue, saturation, value lerped as numbers, no wrap)
inline void lerpTintColorRed( const float a[3], const float b[3], float t, float out[3] )
{
	float ha, sa, va, hb, sb, vb;
	hsv( a, ha, sa, va );
	hsv( b, hb, sb, vb );
	rgbOfHsv( ha + ( hb - ha ) * t, sa + ( sb - sa ) * t, va + ( vb - va ) * t, out );
}

//! the research rule (sec. 5.3 item 5): PNAM is light; it becomes an albedo tint only when its chroma is high
inline void tintOfLayerColor( const float rgb255[3], float out[3], float chromaMin = 0.35f )
{
	const float mx = std::max( { rgb255[0], rgb255[1], rgb255[2] } ), mn = std::min( { rgb255[0], rgb255[1], rgb255[2] } );
	if ( mx <= 0.0f || ( mx - mn ) / mx < chromaMin ) {
		out[0] = out[1] = out[2] = 1.0f;
		return;
	}
	for ( int c = 0; c < 3; c++ )
		out[c] = std::pow( rgb255[c] / mx, 2.2f );
}

} // namespace WwCloudMath

#endif
