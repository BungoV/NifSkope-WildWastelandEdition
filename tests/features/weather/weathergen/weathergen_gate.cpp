/* lane WEATHERRECX1: the generated rung's C++ (src/features/weather/weathergen.h) against the Python rule that wrote the
 * shipped weather files (src/features/weather/weathergen.py --table), one row per vanilla + DLC weather.
 * Usage: weathergen_gate <gen_table.tsv>; exits 1 on any differing value, on fewer than 100 rows, or when the
 * refuter (one class row's visibility edited in memory) is NOT caught.
 * Build: g++ -std=c++17 -O1 -I src tests/features/weather/weathergen/weathergen_gate.cpp -o weathergen_gate */
#include "features/weather/weathergen.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split( const std::string & line )
{
	std::vector<std::string> v;
	std::stringstream ss( line );
	for ( std::string f; std::getline( ss, f, '\t' ); )
		v.push_back( f );
	return v;
}

// the C++ values under the TSV's output column names ("-" = none, as Python's None)
static std::map<std::string, std::string> outputs( const WwWeatherGen::Out & o, double visScale )
{
	std::map<std::string, std::string> m;
	auto f = []( double x ) { char b[64]; std::snprintf( b, sizeof b, "%.6f", x ); return std::string( b ); };
	m["class"] = o.cls;
	m["region"] = o.region;
	m["visibility"] = f( o.visibility * visScale );
	m["visibilityNight"] = o.visibilityNight < 0 ? "-" : f( o.visibilityNight );
	m["hazeHeight"] = f( o.hazeHeight );
	m["ozone"] = f( o.ozone );
	m["hazeType"] = o.hazeType;
	m["humidity"] = f( o.humidity );
	m["groundAlbedo"] = f( o.groundAlbedo );
	m["coverage"] = f( o.coverage );
	m["cloudType"] = o.cloudType;
	m["density"] = f( o.density );
	m["windSpeed"] = f( o.windSpeed );
	m["windDirection"] = f( o.windDirection );
	m["turbulence"] = f( o.turbulence );
	m["gusts"] = f( o.gusts );
	m["tempDay"] = f( o.tempDay );
	m["tempDawn"] = f( o.tempDawn );
	m["lightning"] = f( o.lightning );
	m["precipType"] = o.precipType ? o.precipType : "-";
	m["precipRate"] = f( o.precipRate );
	m["wetting"] = f( o.wetting );
	m["drying"] = f( o.drying );
	m["transHours"] = f( o.transHours );
	m["precipStart"] = f( o.precipStart );
	m["precipEnd"] = f( o.precipEnd );
	m["lightningStart"] = f( o.lightningStart );
	m["lightningEnd"] = f( o.lightningEnd );
	m["stars"] = f( o.stars );
	m["moonlight"] = f( o.moonlight );
	return m;
}

// a Python cell as the same "%.6f" (words pass through)
static std::string norm( const std::string & s )
{
	char * end = nullptr;
	const double x = std::strtod( s.c_str(), &end );
	if ( s.empty() || s == "-" || *end )
		return s;
	char b[64];
	std::snprintf( b, sizeof b, "%.6f", x );
	return b;
}

int main( int argc, char ** argv )
{
	if ( argc < 2 ) {
		std::fprintf( stderr, "usage: weathergen_gate <gen_table.tsv>\n" );
		return 2;
	}
	std::ifstream in( argv[1] );
	std::string line;
	std::getline( in, line );
	const std::vector<std::string> head = split( line );
	const size_t nIn = 15;	// owner .. thunder; the outputs follow
	int rows = 0, bad = 0, refuted = 0;
	std::map<std::string, int> perClass;
	while ( std::getline( in, line ) ) {
		if ( line.empty() )
			continue;
		const std::vector<std::string> c = split( line );
		if ( c.size() != head.size() ) {
			std::printf( "FAIL row %d: %zu cells, header %zu\n", rows + 1, c.size(), head.size() );
			return 1;
		}
		WwWeatherGen::In w;
		w.owner = c[0];
		w.edid = c[2];
		w.flags = std::stoi( c[3] );
		w.windSpeed = std::stoi( c[4] );
		w.windDir = std::stoi( c[5] );
		w.windTurb = std::stoi( c[6] );
		w.transDelta = std::stoi( c[7] );
		w.precipIn = std::stoi( c[8] );
		w.precipOut = std::stoi( c[9] );
		w.thunderIn = std::stoi( c[10] );
		w.thunderOut = std::stoi( c[11] );
		w.precipType = c[12] == "rain" ? 0 : c[12] == "snow" ? 1 : -1;
		w.precipDensity = std::stod( c[13] );
		w.thunder = c[14] == "1";
		const WwWeatherGen::Out o = WwWeatherGen::generate( w );
		const auto mine = outputs( o, 1.0 ), wrong = outputs( o, 1.5 );
		for ( size_t i = nIn; i < head.size(); i++ ) {
			const auto it = mine.find( head[i] );
			if ( it == mine.end() ) {
				std::printf( "FAIL column %s has no C++ value\n", head[i].c_str() );
				return 1;
			}
			const std::string py = norm( c[i] );
			if ( it->second != py ) {
				bad++;
				std::printf( "DIFF %s %s: python %s, c++ %s\n", c[2].c_str(), head[i].c_str(), py.c_str(), it->second.c_str() );
			}
			if ( head[i] == "visibility" && wrong.at( head[i] ) != py )
				refuted++;
		}
		perClass[o.cls]++;
		rows++;
	}
	std::printf( "weathergen gate: %d weathers, %d differing values; refuter (visibility x1.5) caught on %d rows\n", rows, bad,
	             refuted );
	for ( const auto & kv : perClass )
		std::printf( "  class %-10s %d\n", kv.first.c_str(), kv.second );
	const bool ok = rows >= 100 && bad == 0 && refuted == rows;
	std::printf( "%s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
