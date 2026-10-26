// Runs the FO4-Extension-Formats .nifx test cases against src/io/nifxlights (lane NIFXLIGHT1), plus the conversion
// known answers. Build: g++ -std=c++17 -I<tree>/src tests/nifxconform/nifxconform.cpp src/io/nifxlights.cpp
// Run: nifxconform <spec>/tests/cases [--red ignore-lights|tint-dims|anchor800|radius1lux]   (a red must FAIL)

#include "io/nifxlights.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using nifxl::Json;

static std::string red;
// red anchor800 / radius1lux: the guessed constants from before lane NIFXCAL1
static nifxl::Converted conv( const nifxl::Light & l )
{
	return nifxl::convert( l, red == "tint-dims", red == "anchor800" ? nifxl::kLumensPerFadeOld : nifxl::kLumensPerFade,
		red == "radius1lux" ? nifxl::kRadiusPerSqrtCandelaOld : nifxl::kRadiusPerSqrtCandela );
}

static std::string slurp( const fs::path & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

static std::string lower( std::string s )
{
	for ( char & c : s )
		if ( c >= 'A' && c <= 'Z' )
			c = char( c - 'A' + 'a' );
	return s;
}

// The sidecar's Data path parts for a mesh path (reference_reader.sidecar_rel); empty when not a mesh.
static std::vector<std::string> sidecarRel( const std::string & mesh )
{
	std::vector<std::string> parts;
	std::string cur;
	for ( char c : mesh + "/" ) {
		if ( c == '/' || c == '\\' ) {
			if ( !cur.empty() )
				parts.push_back( cur );
			cur.clear();
		} else
			cur += c;
	}
	if ( parts.empty() )
		return {};
	const std::string last = lower( parts.back() );
	if ( last.size() <= 4 || last.compare( last.size() - 4, 4, ".nif" ) != 0 )
		return {};
	if ( lower( parts[0] ) != "meshes" )
		parts.insert( parts.begin(), "Meshes" );
	parts.back() += "x";
	return parts;
}

static bool findCaseFree( fs::path here, const std::vector<std::string> & parts, fs::path & out )
{
	for ( const auto & part : parts ) {
		if ( !fs::is_directory( here ) )
			return false;
		std::vector<std::string> names;
		for ( const auto & e : fs::directory_iterator( here ) )
			names.push_back( e.path().filename().string() );
		std::sort( names.begin(), names.end() );
		auto hit = std::find_if( names.begin(), names.end(), [&]( const std::string & n ) { return lower( n ) == lower( part ); } );
		if ( hit == names.end() )
			return false;
		here /= *hit;
	}
	out = here;
	return true;
}

static Json num( double v ) { Json j; j.kind = Json::Number; j.num = v; return j; }
static Json str( const std::string & v ) { Json j; j.kind = Json::String; j.str = v; return j; }
static Json vec3( const double * v )
{
	Json j;
	j.kind = Json::Array;
	for ( int k = 0; k < 3; ++k )
		j.arr.push_back( num( v[k] ) );
	return j;
}

// A kept light as the reference reader writes it.
static Json lightJson( const nifxl::Light & l )
{
	Json j;
	j.kind = Json::Object;
	auto put = [&]( const char * k, Json v ) { j.obj.emplace_back( k, std::move( v ) ); };
	if ( l.hasType ) put( "type", str( nifxl::typeWord( l.type ) ) );
	put( "lumens", num( l.lumens ) );
	if ( l.hasKelvin ) put( "kelvin", num( l.kelvin ) );
	if ( l.hasTint ) put( "tint", vec3( l.tint ) );
	if ( l.hasTier ) put( "tier", str( nifxl::tierWord( l.tier ) ) );
	if ( l.hasNode ) put( "node", str( l.node ) );
	if ( l.hasFollows ) put( "follows", str( l.follows ) );
	if ( l.hasPosition ) put( "position", vec3( l.position ) );
	if ( l.hasDirection ) put( "direction", vec3( l.direction ) );
	if ( l.hasCone ) put( "cone", num( l.cone ) );
	if ( l.hasFog ) {
		Json f;
		f.kind = Json::Object;
		f.obj.emplace_back( "scale", num( l.fogScale ) );
		put( "fog", f );
	}
	if ( l.hasRadius ) put( "radius", num( l.radius ) );
	return j;
}

static bool same( const Json & a, const Json & b, double rel = 0 )
{
	if ( a.kind != b.kind )
		return false;
	switch ( a.kind ) {
	case Json::Null: return true;
	case Json::Bool: return a.b == b.b;
	case Json::Number: return rel == 0 ? a.num == b.num : std::fabs( a.num - b.num ) <= rel * std::max( 1.0, std::fabs( b.num ) );
	case Json::String: return a.str == b.str;
	case Json::Array:
		if ( a.arr.size() != b.arr.size() ) return false;
		for ( size_t k = 0; k < a.arr.size(); ++k )
			if ( !same( a.arr[k], b.arr[k], rel ) ) return false;
		return true;
	case Json::Object:
		if ( a.obj.size() != b.obj.size() ) return false;
		for ( const auto & m : a.obj ) {
			auto hit = std::find_if( b.obj.begin(), b.obj.end(), [&]( const auto & n ) { return n.first == m.first; } );
			if ( hit == b.obj.end() || !same( m.second, hit->second, rel ) ) return false;
		}
		return true;
	}
	return false;
}

static const Json * key( const Json & o, const char * k )
{
	for ( const auto & m : o.obj )
		if ( m.first == k )
			return &m.second;
	return nullptr;
}

static std::string s( const Json * j ) { return j && j->kind == Json::String ? j->str : std::string(); }

// Warnings this reader owns: at the top (except other sections' value warnings) or inside `lights`.
static bool ours( const std::string & at, const std::string & k, const std::string & warning )
{
	if ( at.empty() )
		return warning != "value" || lower( k ) == "lights";
	std::string first = at.substr( 1, at.find( '/', 1 ) == std::string::npos ? std::string::npos : at.find( '/', 1 ) - 1 );
	return lower( first ) == "lights";
}

static std::string line( const nifxl::Warning & w )
{
	return w.at + " | " + w.key + " | " + w.warning + " | " + w.problem + " | " + w.suggest;
}

static bool runCase( const fs::path & dir, std::string & why )
{
	Json c, e;
	if ( !nifxl::parse( slurp( dir / "case.json" ), c ).empty() || !nifxl::parse( slurp( dir / "expected.json" ), e ).empty() ) {
		why = "case files unreadable";
		return false;
	}
	const std::string want = s( key( e, "status" ) );
	std::string got = "none";
	nifxl::Lights r;
	const auto parts = sidecarRel( s( key( c, "mesh" ) ) );
	fs::path file;
	if ( !parts.empty() ) {
		got = "absent";
		if ( findCaseFree( dir / "Data", parts, file ) && fs::is_regular_file( file ) ) {
			r = nifxl::read( slurp( file ) );
			got = r.status;
		}
	}
	if ( red == "ignore-lights" ) {
		r.lights.clear();
		r.hasLights = false;
		r.warnings.erase( std::remove_if( r.warnings.begin(), r.warnings.end(), []( const nifxl::Warning & w ) {
			return !w.at.empty() || lower( w.key ) == "lights"; } ), r.warnings.end() );
	}
	if ( got != want ) {
		why = "status " + got + " want " + want;
		return false;
	}
	if ( got == "refused" ) {
		if ( r.reason != s( key( e, "reason" ) ) ) {
			why = "reason " + r.reason + " want " + s( key( e, "reason" ) );
			return false;
		}
		return true;
	}
	if ( got != "ok" )
		return true;
	// unknown sections
	std::vector<std::string> wantUnknown;
	if ( const Json * u = key( e, "unknownSections" ) )
		for ( const auto & a : u->arr )
			wantUnknown.push_back( a.str );
	if ( wantUnknown != r.unknownSections ) {
		why = "unknownSections differ";
		return false;
	}
	// warnings
	std::vector<std::string> wantW, gotW;
	if ( const Json * ws = key( e, "warnings" ) )
		for ( const auto & w : ws->arr ) {
			nifxl::Warning x{ s( key( w, "at" ) ), s( key( w, "key" ) ), s( key( w, "warning" ) ), s( key( w, "problem" ) ), s( key( w, "suggest" ) ) };
			if ( ours( x.at, x.key, x.warning ) )
				wantW.push_back( line( x ) );
		}
	for ( const auto & w : r.warnings )
		if ( ours( w.at, w.key, w.warning ) )
			gotW.push_back( line( w ) );
	if ( wantW != gotW ) {
		why = "warnings differ:";
		for ( const auto & w : gotW ) why += "\n    got  " + w;
		for ( const auto & w : wantW ) why += "\n    want " + w;
		return false;
	}
	// the lights section
	const Json * sections = key( e, "sections" );
	const Json * wantLights = sections ? key( *sections, "lights" ) : nullptr;
	if ( bool( wantLights ) != r.hasLights ) {
		why = r.hasLights ? "lights read, none expected" : "lights expected, none read";
		return false;
	}
	if ( wantLights ) {
		Json gotLights;
		gotLights.kind = Json::Object;
		for ( const auto & l : r.lights )
			gotLights.obj.emplace_back( l.name, lightJson( l ) );
		if ( !same( gotLights, *wantLights ) ) {
			why = "lights entries differ";
			return false;
		}
	}
	// lookups
	if ( const Json * looks = key( e, "lookups" ) )
		for ( const auto & lk : looks->arr ) {
			if ( s( key( lk, "section" ) ) != "lights" )
				continue;
			const nifxl::Light * l = r.find( s( key( lk, "node" ) ) );
			const Json * entry = key( lk, "entry" );
			if ( bool( l ) != ( entry && entry->kind == Json::Object ) ) {
				why = "lookup " + s( key( lk, "node" ) ) + " found differs";
				return false;
			}
			if ( !l )
				continue;
			const auto cv = conv( *l );
			Json gotC;
			gotC.kind = Json::Object;
			double col[3] = { double( cv.color[0] ), double( cv.color[1] ), double( cv.color[2] ) };
			gotC.obj.emplace_back( "color", vec3( col ) );
			gotC.obj.emplace_back( "fade", num( cv.fade ) );
			gotC.obj.emplace_back( "radius", num( cv.radius ) );
			const Json * wantC = key( lk, "converted" );
			if ( !wantC || !same( gotC, *wantC, 1e-6 ) ) {
				char b[200];
				std::snprintf( b, sizeof b, "converted %s: [%d,%d,%d] fade %.9g radius %.9g", l->name.c_str(),
					cv.color[0], cv.color[1], cv.color[2], cv.fade, cv.radius );
				why = b;
				return false;
			}
		}
	return true;
}

// Known answers for the conversion (nifx/SPEC.md, Converting) and its rule that a tint never dims.
static int conversionTests()
{
	int fails = 0;
	auto check = [&]( const char * what, bool ok ) {
		std::printf( "%s CONVERT %s\n", ok ? "PASS" : "FAIL", what );
		fails += !ok;
	};
	nifxl::Light white;
	white.lumens = 800;
	auto w = conv( white );
	check( "800 lm white omni = 255,255,255 fade 1.255887 radius 120.4806 (anchor 637 lm, 15.1 per sqrt cd)",
		w.color[0] == 255 && w.color[1] == 255 && w.color[2] == 255 && std::fabs( w.fade - 1.2558870 ) < 1e-6 && std::fabs( w.radius - 120.4806 ) < 1e-3 );
	// The same lumens in any colour: luminance(colour) x fade is the same (the tint never dims).
	double worst = 0;
	const double tints[][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0.1, 0.6 }, { 0, 0, 0.25 }, { 0.3, 0.3, 0.3 } };
	for ( const auto & t : tints )
		for ( double k : { 0.0, 1667.0, 2700.0, 6500.0, 25000.0 } ) {
			nifxl::Light l = white;
			std::copy( t, t + 3, l.tint );
			l.hasTint = true;
			l.hasKelvin = k > 0;
			l.kelvin = k;
			auto c = conv( l );
			double y = 0.2126 * std::pow( c.color[0] / 255.0, 2.2 ) + 0.7152 * std::pow( c.color[1] / 255.0, 2.2 ) + 0.0722 * std::pow( c.color[2] / 255.0, 2.2 );
			worst = std::max( worst, std::fabs( y * c.fade * nifxl::kLumensPerFade / 800 - 1 ) );
		}
	char b[160];
	std::snprintf( b, sizeof b, "tint never dims (30 colours, worst |Y*fade*637/800-1| = %.3g)", worst );
	check( b, worst < 1e-9 );
	nifxl::Light spot = white;
	spot.type = nifxl::Spot;
	spot.lumens = 1200;
	spot.cone = 100;
	auto sp = conv( spot );
	check( "1200 lm white spot cone 100 = fade 10.547397 radius 349.1519", std::fabs( sp.fade - 10.547397 ) < 1e-5 && std::fabs( sp.radius - 349.1519 ) < 1e-3 );
	nifxl::Light bulb = white;
	bulb.hasKelvin = true;
	bulb.kelvin = 2700;
	auto bu = conv( bulb );
	check( "800 lm 2700 K = 255,171,90", bu.color[0] == 255 && bu.color[1] == 171 && bu.color[2] == 90 );
	nifxl::Light big = white;
	big.lumens = 1e9;
	nifxl::Light dark = white;
	dark.lumens = 0;
	check( "radius clamps 16..16384", conv( big ).radius == 16384 && conv( dark ).radius == 16 );
	return fails;
}

int main( int argc, char ** argv )
{
	if ( argc < 2 ) {
		std::printf( "usage: nifxconform <spec>/tests/cases [--red ignore-lights|tint-dims|anchor800|radius1lux]\n" );
		return 2;
	}
	for ( int k = 2; k + 1 < argc; ++k )
		if ( std::string( argv[k] ) == "--red" )
			red = argv[k + 1];
	std::vector<fs::path> cases;
	for ( const auto & d : fs::directory_iterator( argv[1] ) )
		if ( d.is_directory() && d.path().filename().string().rfind( "nifx-", 0 ) == 0 )
			cases.push_back( d.path() );
	std::sort( cases.begin(), cases.end() );
	int fails = 0;
	for ( const auto & c : cases ) {
		std::string why;
		bool ok = runCase( c, why );
		std::printf( "%s %s%s%s\n", ok ? "PASS" : "FAIL", c.filename().string().c_str(), ok ? "" : ": ", why.c_str() );
		fails += !ok;
	}
	fails += conversionTests();
	std::printf( "%s: %d cases, %d failed%s\n", fails ? "NIFXCONFORM FAIL" : "NIFXCONFORM PASS", int( cases.size() ), fails,
		red.empty() ? "" : ( " (red " + red + ")" ).c_str() );
	return fails ? 1 : 0;
}
