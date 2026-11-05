// lane INTSKY1: gl/skyplace.h (the sun's place) + the skyephem.h time zone / start date. Pure headers, no Qt.
// Each check prints "ok <name>" or "FAIL <name> ..."; exit 1 on any FAIL. SKYPLACE_RED=<name> runs one red: the rule
// it names is switched off (or broken) and that check must FAIL (tests/features/sky/skyplace/run.sh drives all of them).
#include "features/sky/skyplace.h"
#include "features/sky/skyephem.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int fails = 0;
static std::string red;

static void check( const char * name, bool ok, const char * fmt = "", double a = 0, double b = 0 )
{
	std::printf( "%s %s ", ok ? "ok" : "FAIL", name );
	std::printf( fmt, a, b );
	std::printf( "\n" );
	if ( !ok )
		fails++;
}

static bool isRed( const char * n ) { return red == n; }

using namespace wwplace;

// the vanilla worlds as Fallout4.esm + DLCs have them (notes/intsky1/tools/show_wrld.py), plus three mod fixtures
static std::vector<World> worlds()
{
	std::vector<World> w( 9 );
	w[0] = { "Commonwealth", "Fallout4.esm", 0x3C, -1, 0, "DefaultClimate", "Fallout4.esm", 0x15F, 0x2CF0 };
	w[1] = { "Goodneighbor", "Fallout4.esm", 0x54BD5, 0, 0x1F, "", "", 0, 0x22610 };
	w[2] = { "DLC03FarHarbor", "DLCCoast.esm", 0xB0F, -1, 0, "DLC03FarHarborClimate", "DLCCoast.esm", 0x74ED, 0x03020168 };
	w[3] = { "DLC03VRWorldspace", "DLCCoast.esm", 0x4EA4, 2, 0, "VRClimate", "DLCCoast.esm", 0x2833A, 0x0300750E };
	w[4] = { "NukaWorld", "DLCNukaWorld.esm", 0x290F, -1, 0, "DLC04DefaultClimate", "DLCNukaWorld.esm", 0xAB03, 0x06008060 };
	w[5] = { "NukaWorldMarket", "DLCNukaWorld.esm", 0x53C58, 4, 0x1F, "", "", 0, 0 };
	w[6] = { "ModIsland", "MyMod.esp", 0x800, -1, 0, "ModIslandClimate", "MyMod.esp", 0x801, 0 };	// nothing known
	w[7] = { "ModFarHarborCave", "MyMod.esp", 0x802, 2, 0, "ModCaveClimate", "MyMod.esp", 0x803, 0 };	// child of FH, own climate
	w[8] = { "ModOnVanillaClimate", "MyMod.esp", 0x804, -1, 0, "DLC03AcadiaClimate", "DLCCoast.esm", 0x45AB1, 0 };
	return w;
}

static int redBits()
{
	return isRed( "table" ) ? RedTable : isRed( "noparent" ) ? RedParent : isRed( "norecx" ) ? RedRecx : 0;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( const char * r = std::getenv( "SKYPLACE_RED" ) )
		red = r;
	const std::vector<World> ws = worlds();
	const int rb = redBits();
	const RecxFn none;

	// rule 2: the table by worldspace (red table: Far Harbor reads Boston)
	{
		const Resolved r = resolve( ws, 2, none, rb );
		check( "table", r.rule == RuleTableWorld && r.place.latDeg == 44.39 && r.place.lonDeg == -68.2,
			"Far Harbor lat %.4f lon %.4f", r.place.latDeg, r.place.lonDeg );
		const Resolved n = resolve( ws, 4, none, rb );
		check( "tablenuka", n.rule == RuleTableWorld && n.place.latDeg == 42.45 && n.place.lonDeg == -71.85,
			"Nuka-World lat %.4f lon %.4f", n.place.latDeg, n.place.lonDeg );
		const Resolved c = resolve( ws, 0, none, rb );
		check( "tableboston", c.rule == RuleTableWorld && c.place.latDeg == 42.36 && c.place.lonDeg == -71.06,
			"Commonwealth lat %.4f lon %.4f", c.place.latDeg, c.place.lonDeg );
	}
	// rule 3: a child takes its parent's (the mod cave under Far Harbor has its own unknown climate)
	{
		const Resolved r = resolve( ws, 7, none, rb );
		check( "parent", r.rule == RuleParent && r.place.latDeg == 44.39 && r.from == 2,
			"child of Far Harbor lat %.4f from %.0f", r.place.latDeg, r.from );
		const Resolved g = resolve( ws, 1, none, rb );
		check( "parentclimate", g.climateEdid == "DefaultClimate" && g.place.latDeg == 42.36, "Goodneighbor lat %.4f", g.place.latDeg );
		const Resolved v = resolve( ws, 3, none, rb );
		check( "parentvr", v.place.latDeg == 44.39 && v.place.lonDeg == -68.2, "the VR world lat %.4f lon %.4f", v.place.latDeg, v.place.lonDeg );
	}
	// rule 4: a mod worldspace on a vanilla climate
	{
		const Resolved r = resolve( ws, 8, none, rb );
		check( "climate", r.rule == RuleTableClimate && r.place.latDeg == 44.39, "Acadia climate lat %.4f", r.place.latDeg );
	}
	// rule 5: anything else is Boston
	{
		const Resolved r = resolve( ws, 6, none, rb );
		check( "default", r.rule == RuleDefault && r.place.latDeg == 42.36 && r.place.lonDeg == -71.06 && r.place.localMeanTime(),
			"mod island lat %.4f lon %.4f", r.place.latDeg, r.place.lonDeg );
	}
	// rule 1: the climate's .recx wins over the table, and a child under a parent with a file takes it
	{
		const RecxFn fn = []( const World & co, Place & out, std::string & file ) {
			if ( co.climateEdid != "DLC03FarHarborClimate" )
				return false;
			out.latDeg = 10.0;
			out.lonDeg = 20.0;
			out.tzHours = -4.0;
			file = "Climates/DLC03FarHarborClimate.recx";
			return true;
		};
		const Resolved r = resolve( ws, 2, fn, rb );
		check( "recx", r.rule == RuleRecx && r.place.latDeg == 10.0 && r.place.tzHours == -4.0, "lat %.2f tz %.2f", r.place.latDeg, r.place.tzHours );
		const Resolved c = resolve( ws, 7, fn, rb & ~RedRecx );
		check( "recxchild", c.rule == RuleRecx && c.place.latDeg == 10.0, "child lat %.2f", c.place.latDeg );
	}
	// the climate owner: no CNAM or use-climate flag -> the parent's
	check( "climateowner", climateOwner( ws, 1 ) == 0 && climateOwner( ws, 5 ) == 4 && climateOwner( ws, 3 ) == 3 && climateOwner( ws, 7 ) == 7 );
	// interiors
	{
		std::string why;
		const int a = interiorWorld( ws, { 0x111u, 0x2CF0u }, "Fallout4.esm", why );
		check( "intlocation", a == 0 && why == "location", "chain -> %.0f", a );
		const int b = interiorWorld( ws, { 0x999u }, isRed( "intplugin" ) ? "Fallout4.esm" : "DLCCoast.esm", why );
		check( "intplugin", b == 2 && why == "plugin", "DLCCoast cell, no chain -> %.0f", b );
		const int c = interiorWorld( ws, {}, "MyMod.esp", why );
		check( "intdefault", c == 0 && why == "commonwealth", "mod cell -> %.0f", c );
	}
	// keys
	{
		Place p;
		std::string why, warn;
		check( "keysok", placeFromKeys( true, 44.39, true, -68.2, true, false, -5.0, "2287-06-21", p, why, warn )
			&& p.tzHours == -5.0 && p.startMonth == 6 && p.startDay == 21 && warn.empty() );
		check( "keysbadlat", !placeFromKeys( true, 95.0, true, 0.0, false, false, 0.0, "", p, why, warn ) );
		warn.clear();
		check( "keysmean", placeFromKeys( true, 1.0, true, 2.0, true, true, NAN, "", p, why, warn ) && p.localMeanTime() );
		warn.clear();
		check( "keysbaddate", placeFromKeys( true, 1.0, true, 2.0, false, false, 0.0, "2287-02-30", p, why, warn )
			&& p.startMonth == 10 && !warn.empty() );
	}
	// the ephemeris: the default (local mean time, the game's start) is the EPHEM1 arithmetic bit for bit
	{
		const wwephem::Date d0 = wwephem::dateFromGameDay( 3.0, 9.5 );
		const wwephem::Date d1 = wwephem::dateFromGameDay( 3.0, 9.5, 2287, 10, 23 );
		const wwephem::Body a = wwephem::sun( d0, 42.36, -71.06 ), b = wwephem::sun( d1, 42.36, -71.06, NAN );
		// the EPHEM1 julian day: local mean time at the longitude
		const double jdOld = wwephem::julianDay( d0.year, d0.month, d0.day + ( d0.hour + 71.06 / 15.0 ) / 24.0 );
		check( "ephemsame", d0.year == d1.year && d0.month == d1.month && d0.day == d1.day && a.elevDeg == b.elevDeg
			&& a.azDeg == b.azDeg && a.julianDay == jdOld, "elev %.6f jd %.6f", a.elevDeg, a.julianDay );
		const wwephem::Body z = wwephem::sun( d0, 42.36, -71.06, isRed( "zone" ) ? NAN : -5.0 );
		// 9:30 EST is 15.76 min LATER in UT than 9:30 Boston mean time (UT-5 vs UT-4.737): a higher morning sun
		check( "zone", std::fabs( z.julianDay - a.julianDay - ( 5.0 - 71.06 / 15.0 ) / 24.0 ) < 1e-9 && z.elevDeg > a.elevDeg,
			"elev EST %.4f vs mean %.4f", z.elevDeg, a.elevDeg );
		const wwephem::Date s = wwephem::dateFromGameDay( 10.0, 12.0, 2287, isRed( "start" ) ? 10 : 6, isRed( "start" ) ? 23 : 21 );
		check( "start", s.month == 7 && s.day == 1, "day 10 from 21 June = month %.0f day %.0f", s.month, s.day );
		const wwephem::Date w = wwephem::dateFromGameDay( 70.0, 12.0 );
		check( "startwrap", w.year == 2288 && w.month == 1 && w.day == 1, "day 70 = %.0f-%.0f", w.month, w.day );
	}
	// Far Harbor vs the Commonwealth at the same date and hour (the picture's numbers)
	{
		const wwephem::Date d = wwephem::dateFromGameDay( 0.0, 12.0 );
		const wwephem::Body cw = wwephem::sun( d, kBostonLat, kBostonLon ), fh = wwephem::sun( d, kBarHarborLat, kBarHarborLon );
		std::printf( "info noon 2287-10-23 local mean time: Commonwealth elev %.3f az %.3f, Far Harbor elev %.3f az %.3f\n",
			cw.elevDeg, cw.azDeg, fh.elevDeg, fh.azDeg );
		check( "farharborlower", fh.elevDeg < cw.elevDeg - 1.5 && fh.elevDeg > cw.elevDeg - 2.5, "FH %.3f CW %.3f", fh.elevDeg, cw.elevDeg );
		// day length: sunrise to sunset at 0 deg geometric, 1-minute steps
		auto dayLen = [&]( double lat, double lon ) {
			int up = 0;
			for ( int m = 0; m < 1440; m++ ) {
				const wwephem::Date t = wwephem::dateFromGameDay( 0.0, m / 60.0 );
				up += wwephem::sun( t, lat, lon ).elevDeg > 0.0 ? 1 : 0;
			}
			return up / 60.0;
		};
		const double lc = dayLen( kBostonLat, kBostonLon ), lf = dayLen( kBarHarborLat, kBarHarborLon );
		std::printf( "info day length 2287-10-23: Commonwealth %.3f h, Far Harbor %.3f h\n", lc, lf );
		check( "daylength", lf < lc, "FH %.3f h CW %.3f h", lf, lc );
	}
	std::printf( "%s: %d failed%s%s\n", fails ? "FAIL" : "PASS", fails, red.empty() ? "" : " under red ", red.c_str() );
	return fails ? 1 : 0;
}
