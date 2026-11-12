#ifndef WW_WEATHERGEN_H
#define WW_WEATHERGEN_H

/* lane WEATHERRECX1: the weather .recx GENERATED RUNG (rung 2: a key no file sets), no GL and no Qt. One WTHR record's
 * class -> every Weathers block in physical units. Deterministic: the same rule as src/features/weather/weathergen.py
 * (which wrote the shipped files); tests/features/weather/weathergen checks the two agree on all 107 vanilla + DLC
 * weathers. Spec: FO4-Extension-Formats recx/SPEC.md "The generated rung: a weather with no file".
 * Reads only the EditorID, the defining plugin, DATA bytes 0/3/6-9/11/17/19, the precipitation shader's type and
 * density, and whether a thunder sound is linked: never a colour, never a fog distance (bungo 11-06). */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace WwWeatherGen
{

struct In
{
	std::string edid, owner;	//!< owner = the defining plugin's file name ("DLCCoast.esm")
	int flags = 0;	//!< DATA byte 11: 1 Pleasant, 2 Cloudy, 4 Rainy, 8 Snow
	int precipType = -1;	//!< the MNAM shader's DATA Type: 0 rain, 1 snow; -1 none
	double precipDensity = 7.0;	//!< its Particle Density (vanilla: 7)
	bool thunder = false;	//!< an SNAM sound of type 3
	int windSpeed = 0, windDir = 0, windTurb = 0, transDelta = 0;	//!< DATA 0, 17, 19, 3
	int precipIn = 0, precipOut = 0, thunderIn = 0, thunderOut = 0;	//!< DATA 6, 7, 8, 9
};

struct Out
{
	const char * cls = "clear";
	const char * region = "commonwealth";
	double visibility = 30000, visibilityNight = -1, hazeHeight = 1200, ozone = 1;	// visibilityNight < 0: none
	const char * hazeType = "clean";
	double humidity = 55, groundAlbedo = 0.15;
	double coverage = 0.2;
	const char * cloudType = "cumulus";
	double density = 1;
	double windSpeed = 0, windDirection = 0, turbulence = 0, gusts = 0;	// m/s, degrees FROM (0 north, 90 east)
	double tempDay = 12, tempDawn = 6;	// deg C at 14:00 and 05:00
	double lightning = 0;	// flashes per minute
	const char * precipType = nullptr;	// "rain" / "snow_fine" / none
	double precipRate = 0, wetting = 20, drying = 90;
	double transHours = 0.01, precipStart = 0, precipEnd = 0, lightningStart = 0, lightningEnd = 0;
	double stars = 1, moonlight = 1;
};

//! the cloud type words as clouds.type numbers (weatherrecx.cpp typeWords)
inline double cloudTypeNumber( const char * w )
{
	const std::string s = w ? w : "";
	return s == "stratocumulus" ? 1.0 / 3.0 : s == "cumulus" ? 2.0 / 3.0 : s == "cumulonimbus" ? 1.0 : 0.0;
}

//! "%.3f" then back: the Python rule's r3 (the files hold these numbers)
inline double r3( double x )
{
	char b[64];
	std::snprintf( b, sizeof b, "%.3f", x );
	return std::strtod( b, nullptr );
}

inline std::string lower( std::string s )
{
	for ( char & c : s )
		c = char( std::tolower( static_cast<unsigned char>( c ) ) );
	return s;
}

inline bool has( const std::string & s, const char * w )
{
	return s.find( w ) != std::string::npos;
}

//! SPEC.md "The generated rung" 1: the first rule that holds
inline const char * classify( const std::string & edid, int flags, int precipType, bool thunder )
{
	std::string e = lower( edid );
	for ( size_t p; ( p = e.find( "vbfog" ) ) != std::string::npos; )
		e.erase( p, 5 );	// CommonwealthClear_VBFog: a clear weather with volumetric fog, not a fog
	if ( has( e, "radstorm" ) )
		return "radstorm";
	if ( precipType == 0 )
		return thunder ? "storm" : has( e, "mist" ) ? "drizzle" : "rain";
	if ( precipType == 1 )
		return "snow";
	if ( has( e, "nuke" ) )
		return "smoke";
	if ( has( e, "fog" ) )
		return "fog";
	if ( has( e, "mist" ) )
		return "mist";
	if ( has( e, "dust" ) )
		return has( e, "dusty" ) ? "dust" : "duststorm";
	if ( has( e, "pollut" ) )
		return "polluted";
	if ( has( e, "overcast" ) || has( e, "dark" ) )
		return "overcast";
	if ( has( e, "clearest" ) )
		return "clearest";
	if ( flags & ( 2 | 4 | 8 ) )
		return "overcast";
	return "clear";
}

inline const char * regionOf( const std::string & edid, const std::string & owner )
{
	const std::string o = lower( owner );
	if ( o == "dlccoast.esm" )
		return "coast";
	if ( o == "dlcnukaworld.esm" )
		return "nukaworld";
	if ( has( lower( edid ), "commonwealthgs" ) )
		return "glowingsea";
	return "commonwealth";
}

struct ClassRow
{
	const char * name;
	double visibility;
	const char * hazeType;
	double humidity, coverage;
	const char * cloudType;
	double density, wind, tempOffset, lightning, hazeHeight;
};

inline const ClassRow & classRow( const char * cls )
{
	static const ClassRow rows[] = {
		{ "clearest", 80000, "clean", 45, 0.05, "cumulus", 0.8, 3, 1, 0, 1200 },
		{ "clear", 30000, "clean", 55, 0.2, "cumulus", 1.0, 4, 0, 0, 1200 },
		{ "polluted", 12000, "urban", 60, 0.3, "stratocumulus", 1.0, 3, 0, 0, 1200 },
		{ "overcast", 15000, "clean", 75, 0.95, "stratus", 1.2, 6, -2, 0, 1200 },
		{ "mist", 3000, "clean", 93, 0.7, "stratus", 1.0, 2, -3, 0, 200 },
		{ "fog", 500, "clean", 100, 0.9, "stratus", 1.0, 1.5, -4, 0, 120 },
		{ "drizzle", 2500, "clean", 97, 1.0, "stratus", 1.5, 3, -3, 0, 600 },
		{ "rain", 5000, "clean", 95, 1.0, "stratus", 2.0, 7, -3, 0, 600 },
		{ "storm", 4000, "clean", 95, 1.0, "cumulonimbus", 2.5, 11, -3, 4, 600 },
		{ "snow", 2000, "clean", 90, 1.0, "stratus", 1.5, 5, -14, 0, 600 },
		{ "dust", 8000, "dust", 30, 0.2, "cumulus", 1.0, 8, 4, 0, 1200 },
		{ "duststorm", 800, "dust", 20, 0.4, "cumulus", 1.0, 15, 5, 0, 2500 },
		{ "smoke", 2000, "smoke", 40, 0.6, "stratocumulus", 1.5, 10, 6, 0, 2500 },
		{ "radstorm", 2500, "glowing", 60, 1.0, "cumulonimbus", 2.5, 14, 3, 6, 1200 },	// haze in the mixed layer: CALIOP Za 1.2 km, eastern US (Koffi 2012); 2500 m went white at noon
	};
	for ( const ClassRow & r : rows )
		if ( std::string( r.name ) == cls )
			return r;
	return rows[1];
}

constexpr double kTransMin = 0.01, kTransMax = 3.0;	// fWeatherTransMin (the exe), fWeatherTransMax (Fallout4.esm GMST)

inline Out generate( const In & in )
{
	Out o;
	o.cls = classify( in.edid, in.flags, in.precipType, in.thunder );
	o.region = regionOf( in.edid, in.owner );
	const std::string cls = o.cls, reg = o.region;
	const ClassRow & c = classRow( o.cls );
	double base = 12, albedo = 0.15;
	const char * regionHaze = "clean";
	if ( reg == "coast" ) {
		base = 7;
		regionHaze = "maritime";
		albedo = 0.12;
	} else if ( reg == "nukaworld" ) {
		base = 14;
		albedo = 0.20;
	} else if ( reg == "glowingsea" ) {
		base = 16;
		regionHaze = "glowing";
		albedo = 0.25;
	}
	o.hazeType = std::string( c.hazeType ) == "clean" ? regionHaze : c.hazeType;
	if ( has( lower( in.edid ), "rad" ) && ( cls == "fog" || cls == "mist" ) )
		o.hazeType = "glowing";	// Far Harbor's radioactive fog
	o.visibility = c.visibility;
	o.visibilityNight = ( cls == "mist" || cls == "fog" ) ? r3( 0.7 * c.visibility ) : -1.0;
	o.hazeHeight = c.hazeHeight;
	o.ozone = 1.0;
	o.humidity = c.humidity;
	o.groundAlbedo = albedo;
	o.coverage = c.coverage;
	o.cloudType = c.cloudType;
	o.density = c.density;
	const double speed = c.wind * ( 0.5 + in.windSpeed / 255.0 );
	const double turb = in.windTurb / 255.0;
	const double a = in.windDir / 255.0 * 360.0;
	o.windSpeed = r3( speed );
	o.windDirection = r3( std::fmod( std::fmod( 270.0 - a, 360.0 ) + 360.0, 360.0 ) );
	o.turbulence = r3( turb );
	o.gusts = r3( speed * ( 1.0 + turb ) );
	const bool clearNight = cls == "clear" || cls == "clearest" || cls == "dust";
	o.tempDay = base + c.tempOffset;
	o.tempDawn = base + c.tempOffset - ( clearNight ? 6 : 2 );
	o.lightning = in.thunder ? c.lightning : 0.0;
	double rate = 0.0, wetting = 20.0;
	if ( cls == "drizzle" ) { rate = 1; wetting = 30; }
	else if ( cls == "rain" ) { rate = 4; wetting = 15; }
	else if ( cls == "storm" ) { rate = 8; wetting = 15; }
	else if ( cls == "snow" ) { rate = 1; wetting = 30; }
	if ( ( in.precipType == 0 || in.precipType == 1 ) && rate > 0.0 ) {
		o.precipType = in.precipType == 0 ? "rain" : "snow_fine";
		o.precipRate = r3( rate * in.precipDensity / 7.0 );
		o.wetting = wetting;
	}
	o.drying = 90;
	o.transHours = r3( in.transDelta / 255.0 * ( kTransMax - kTransMin ) + kTransMin );
	o.precipStart = r3( in.precipIn / 255.0 );
	o.precipEnd = r3( in.precipOut / 255.0 );
	o.lightningStart = r3( in.thunderIn / 255.0 );
	o.lightningEnd = r3( in.thunderOut / 255.0 );
	if ( cls == "fog" ) {
		o.stars = 0.1;
		o.moonlight = 0.3;
	} else if ( cls == "overcast" || cls == "rain" || cls == "storm" || cls == "snow" || cls == "radstorm" || cls == "drizzle" ) {
		o.stars = 0.3;
		o.moonlight = 0.5;
	}
	return o;
}

/* the hazeType words (SPEC.md atmosphere 1.3; OPAC at 50 % humidity): albedo at 680 / 550 / 440 nm, angstrom, g,
 * airglow */
struct HazeType
{
	const char * word;
	float albedo[3], angstrom, g, airglow;
	float cloudAlbedo[3];	//!< NIGHTHAZE1: the cloud medium's single-scattering albedo (clouds.albedo's default)
};

inline const HazeType * hazeType( const std::string & w )
{
	static const HazeType t[] = {
		{ "clean", { 0.97f, 0.97f, 0.97f }, 1.1f, 0.70f, 1.0f, { 0.99f, 0.99f, 0.99f } },
		{ "maritime", { 0.99f, 0.99f, 0.99f }, 0.3f, 0.75f, 1.0f, { 0.99f, 0.99f, 0.99f } },
		{ "urban", { 0.82f, 0.82f, 0.82f }, 1.4f, 0.65f, 1.0f, { 0.99f, 0.99f, 0.99f } },
		{ "dust", { 0.97f, 0.92f, 0.80f }, 0.2f, 0.75f, 1.0f, { 0.99f, 0.99f, 0.99f } },
		{ "smoke", { 0.86f, 0.84f, 0.80f }, 1.8f, 0.60f, 1.0f, { 0.99f, 0.99f, 0.99f } },
		// NIGHTHAZE1: the glowing cloud medium absorbs a little red and blue (deck_green.py: a tau 30 deck lets through
		// R/G 0.68, B/G 0.47 at 0.89 x the grey deck's light)
		{ "glowing", { 0.88f, 0.97f, 0.82f }, 0.6f, 0.72f, 4.0f, { 0.985f, 0.99f, 0.98f } },
	};
	const std::string l = lower( w );
	for ( const HazeType & h : t )
		if ( l == h.word )
			return &h;
	return nullptr;
}

/* Hänel's growth (gamma 0.25), relative to the table's 50 %: f = ((1 - RH) / 0.5)^-0.25 with RH = min(h, 99) / 100;
 * albedo -> 1 - (1 - albedo) / f, angstrom / f, g + 0.05 (f - 1) (max 0.9); f = 1 at 50 % */
inline void humidityGrowth( float humidity, float albedo[3], float & angstrom, float & g )
{
	const double rh = std::min( 99.0, std::max( 0.0, double( humidity ) ) ) / 100.0;
	const double f = std::pow( ( 1.0 - rh ) / 0.5, -0.25 );
	for ( int i = 0; i < 3; i++ )
		albedo[i] = float( 1.0 - ( 1.0 - albedo[i] ) / f );
	angstrom = float( angstrom / f );
	g = float( std::min( 0.9, g + 0.05 * ( f - 1.0 ) ) );
}

}	// namespace WwWeatherGen

#endif
