#ifndef NIFXLIGHTS_H
#define NIFXLIGHTS_H

// The .nifx `lights` reader (lane NIFXLIGHT1): plain C++17 without Qt, so the spec's test cases run against it.
// Rules: FO4-Extension-Formats nifx/SPEC.md ("lights", "Reading rules") and tests/reference_reader.py.

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nifxl {

struct Json
{
	enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
	bool b = false;
	double num = 0;
	std::string str;										// UTF-8
	std::vector<Json> arr;
	std::vector<std::pair<std::string, Json>> obj;		// the first of keys equal case-free wins
	std::vector<std::string> dups;						// the later spellings, dropped
	const Json * find( const std::string & key ) const;	// case-free (ASCII letters)
};

//! Bytes to a document by the spec's file rules (BOM, comments, trailing commas, limits).
//! "" when ok, else the refusal word: too-large, empty, not-object, binary, malformed, bad-escape, too-deep, too-many.
std::string parse( const std::string & bytes, Json & out );

struct Warning { std::string at, key, warning, problem, suggest; };

enum LightType { Omni = 0, Spot = 1, Hemisphere = 2 };
enum LightTier { Hero = 0, Normal = 1, Bulb = 2 };

struct Light
{
	std::string name;	// as authored
	bool hasType = false, hasKelvin = false, hasTint = false, hasTier = false, hasNode = false, hasFollows = false;
	bool hasPosition = false, hasDirection = false, hasCone = false, hasFog = false, hasRadius = false;
	int type = Omni;
	int tier = Normal;
	double lumens = 0, kelvin = 6500, cone = 90, fogScale = 1, radius = 0;
	double tint[3] = { 1, 1, 1 }, position[3] = { 0, 0, 0 }, direction[3] = { 1, 0, 0 };
	std::string node, follows;
};

struct Lights
{
	std::string status = "none";			// ok | refused
	std::string reason;						// the refusal word
	std::vector<std::string> unknownSections;	// sorted
	std::vector<Warning> warnings;			// sorted by at, key, warning, problem
	bool hasLights = false;					// a `lights` object was read
	std::vector<Light> lights;				// file order
	const Light * find( const std::string & name ) const;	// case-free, first wins
};

constexpr std::size_t kMaxBytes = std::size_t( 1 ) << 20;

//! The whole file's bytes (the size cap is checked here).
Lights read( const std::string & bytes );

struct Converted { int color[3] = { 255, 255, 255 }; double fade = 1, radius = 16; };

// Lane NIFXCAL1, measured on the vanilla masters (notes/nifxcal1/CENSUS.md). Vanilla's household bulbs (cage bulbs,
// ceiling lamps, sconces: 1710 placed lights) sit at a median white fade of 1.256; their real twin, a 60 W bulb, gives
// 800 lm (ENERGY STAR / FTC lumens table). So a white omni at fade 1.0 is 800 / 1.256 = 637 lm.
constexpr double kLumensPerFade = 637.0;
// Radius per sqrt(candela): the median over vanilla's 12,944 fixture lights. The 1-lux reach (69.99) put a .nifx bulb
// 4.6x further than vanilla's own bulbs; this one cuts at about 21 lux.
constexpr double kRadiusPerSqrtCandela = 15.1;
// The guesses before NIFXCAL1 (reds anchor800 and radius1lux).
constexpr double kLumensPerFadeOld = 800.0, kRadiusPerSqrtCandelaOld = 69.99;

//! lumens + kelvin + tint to FO4's colour bytes, fade and radius (nifx/SPEC.md, Converting). red = a tint dims.
Converted convert( const Light & l, bool redTintDims = false, double lumensPerFade = kLumensPerFade,
	double radiusPerSqrtCandela = kRadiusPerSqrtCandela );
void kelvinRgb( double kelvin, double rgb[3] );

//! A vanilla light (colour bytes, fade, radius, type, cone, shadow flag) in .nifx terms, so convert() gives it back:
//! kelvin = the nearest black body (CIE 1960 uv), tint = the rest as the gel colour, lumens by the same anchor,
//! tier hero when it casts a shadow. `radius` is set only when the law would give another one. FO4 bytes need not
//! reach 255; the spec's always do, so the bytes come back scaled to a brightest 255 and the fade carries the rest.
Light fromVanilla( const int color[3], double fade, double radius, int type, double cone, bool shadow,
	double lumensPerFade = kLumensPerFade, double radiusPerSqrtCandela = kRadiusPerSqrtCandela );

std::string fold( const std::string & s );
std::string suggest( const std::string & key, std::vector<std::string> known );
const char * typeWord( int type );
const char * tierWord( int tier );

} // namespace nifxl

#endif // NIFXLIGHTS_H
