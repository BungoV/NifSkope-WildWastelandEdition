#ifndef SKYPLACE_H
#define SKYPLACE_H

/* lane INTSKY1 (bungo 11-01, LATE ITEMs 27e-27g): WHERE on Earth a worldspace's sun and moon are computed, so Far Harbor
 * and Nuka-World get their own sun and moon paths and day length. One rule, the same in NifSkope and Fusion Shaders:
 *
 *   1. a .recx `location` block on the worldspace's climate (Plugins/<Provider>/Climates/<EditorID>.recx, key `climate`)
 *   2. the built-in table, by the worldspace (plugin + EditorID)
 *   3. a child worldspace (WRLD WNAM) takes its parent's answer (Diamond City, Goodneighbor, Nuka-World Market, ...)
 *   4. the built-in table, by the climate (plugin + EditorID): a mod worldspace on a vanilla climate
 *   5. Boston, the Commonwealth's: anything else (a mod's new worldspace) still gets a right sky
 *
 * The climate of a worldspace is its CNAM, or its parent's when it has no CNAM or its WRLD PNAM use flags carry
 * "use climate" (0x10, the engine's kClimate parent use flag). An interior has no worldspace: it takes the worldspace
 * whose WRLD XLCN location is on its CELL XLCN location's parent chain (LCTN PNAM), else the first worldspace of the
 * cell's own plugin that the table places, else the Commonwealth (wwplace::interiorWorld).
 *
 * The clock: the game hour is LOCAL MEAN TIME at the longitude unless `timeZone` names hours from UTC (the FO4CS Solar
 * rule, res/Solar/00_FO4CS_Places.json "localMeanTime": true); `startDate` is the calendar date of game day 0 (the
 * game's own 2287-10-23). Pure header (no Qt): tests/features/sky/skyplace checks it with a red per rule. */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace wwplace {

struct Place
{
	double latDeg = 42.36, lonDeg = -71.06;	// north, east (west negative)
	double tzHours = NAN;					// NaN = local mean time at the longitude
	int startYear = 2287, startMonth = 10, startDay = 23;
	bool localMeanTime() const { return !std::isfinite( tzHours ); }
};

inline bool operator==( const Place & a, const Place & b )
{
	return a.latDeg == b.latDeg && a.lonDeg == b.lonDeg && a.localMeanTime() == b.localMeanTime()
		&& ( a.localMeanTime() || a.tzHours == b.tzHours ) && a.startYear == b.startYear && a.startMonth == b.startMonth
		&& a.startDay == b.startDay;
}

struct Builtin
{
	const char * plugin;	// the defining plugin, lower case
	const char * edid;		// worldspace or climate EditorID
	bool climate;			// true = a climate row (rule 4), false = a worldspace row (rule 2)
	double lat, lon;
	const char * name;
	const char * source;
};

/* The vanilla table. Coordinates = Fusion Shaders' own shipped places (FO4CS res/Solar/00_FO4CS_Places.json, so both
 * readers agree to the digit); the place each stands for:
 *  - Boston, Massachusetts 42.36 N 71.06 W (Wikipedia "Boston": 42 21' 37" N, 71 03' 28" W)
 *  - Bar Harbor, Maine 44.39 N 68.20 W (Wikipedia "Bar Harbor (CDP), Maine": 44.3858 N, 68.2111 W; the port 68.2039 W)
 *  - Nuka-World: no real site is named by the game (it is "west of Boston" in game); 42.45 N 71.85 W ("north of
 *    Worcester, Massachusetts") is FO4CS's choice, kept for parity, not a sourced place.
 * Pre-war Sanctuary Hills is the Commonwealth's map before the war: Boston, as the Commonwealth. The climates are the
 * EditorIDs in Fallout4.esm, DLCCoast.esm and DLCNukaWorld.esm (CLMT records read by notes/intsky1/tools/esmwalk.py). */
constexpr double kBostonLat = 42.36, kBostonLon = -71.06;
constexpr double kBarHarborLat = 44.39, kBarHarborLon = -68.2;
constexpr double kNukaLat = 42.45, kNukaLon = -71.85;

inline const std::vector<Builtin> & builtins()
{
	static const std::vector<Builtin> t = {
		{ "fallout4.esm", "Commonwealth", false, kBostonLat, kBostonLon, "Boston, Massachusetts", "FO4CS Places + Wikipedia Boston" },
		{ "fallout4.esm", "SanctuaryHillsWorld", false, kBostonLat, kBostonLon, "Boston, Massachusetts (pre-war Commonwealth)", "as Commonwealth" },
		{ "dlccoast.esm", "DLC03FarHarbor", false, kBarHarborLat, kBarHarborLon, "Bar Harbor, Maine", "FO4CS Places + Wikipedia Bar Harbor" },
		{ "dlcnukaworld.esm", "NukaWorld", false, kNukaLat, kNukaLon, "north of Worcester, Massachusetts", "FO4CS Places (no real site in game)" },
		{ "fallout4.esm", "DefaultClimate", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "the Commonwealth's climate" },
		{ "fallout4.esm", "DefaultClimateBackup", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "the Commonwealth's climate" },
		{ "fallout4.esm", "ClearClimate", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "a Commonwealth climate" },
		{ "fallout4.esm", "DiamondCityClimate", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "a Commonwealth climate" },
		{ "fallout4.esm", "DiamondCityPastelClimate", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "a Commonwealth climate" },
		{ "fallout4.esm", "GoodneighborClimate", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "a Commonwealth climate" },
		{ "fallout4.esm", "SanctuaryHillsWorldClimate", true, kBostonLat, kBostonLon, "Boston, Massachusetts", "as Commonwealth" },
		{ "dlccoast.esm", "DLC03FarHarborClimate", true, kBarHarborLat, kBarHarborLon, "Bar Harbor, Maine", "Far Harbor's climate" },
		{ "dlccoast.esm", "DLC03AcadiaClimate", true, kBarHarborLat, kBarHarborLon, "Bar Harbor, Maine", "Acadia, on the island" },
		{ "dlccoast.esm", "VRClimate", true, kBarHarborLat, kBarHarborLon, "Bar Harbor, Maine", "Far Harbor's simulation" },
		{ "dlcnukaworld.esm", "DLC04DefaultClimate", true, kNukaLat, kNukaLon, "north of Worcester, Massachusetts", "Nuka-World's climate" },
	};
	return t;
}

inline std::string lower( std::string s )
{
	for ( char & c : s )
		if ( c >= 'A' && c <= 'Z' )
			c = char( c - 'A' + 'a' );
	return s;
}

inline const Builtin * builtin( const std::string & plugin, const std::string & edid, bool climate )
{
	const std::string p = lower( plugin ), e = lower( edid );
	for ( const Builtin & b : builtins() )
		if ( b.climate == climate && p == b.plugin && e == lower( b.edid ) )
			return &b;
	return nullptr;
}

struct World
{
	std::string edid, plugin;	// the WRLD's EditorID and defining plugin (file name)
	std::uint32_t local = 0;	// its id inside that plugin
	int parent = -1;			// index of the WNAM parent in the list, -1 none
	std::uint16_t useFlags = 0;	// WRLD PNAM: 0x10 = takes the parent's climate
	std::string climateEdid, climatePlugin;	// CNAM (empty = none)
	std::uint32_t climateLocal = 0;
	std::uint32_t location = 0;	// XLCN (a load-order form id), 0 none
};

enum Rule { RuleRecx = 1, RuleTableWorld, RuleParent, RuleTableClimate, RuleDefault };

inline const char * ruleName( int r )
{
	switch ( r ) {
	case RuleRecx: return "recx";
	case RuleTableWorld: return "table(worldspace)";
	case RuleParent: return "parent";
	case RuleTableClimate: return "table(climate)";
	default: return "default(Boston)";
	}
}

struct Resolved
{
	Place place;
	int rule = RuleDefault;
	int from = -1;				// the worldspace whose row or file answered (a parent for RuleParent)
	std::string name;			// the table row's place name, or the .recx file
	std::string climateEdid;	// the climate that was looked up (the worldspace's own or its parent's)
};

//! reds (WW_LOCATION_RED): table = no built-in rows (Far Harbor reads Boston), noparent, norecx
enum Red { RedTable = 1, RedParent = 2, RedRecx = 4 };

//! the worldspace whose climate W uses (its own CNAM, else up the parents)
inline int climateOwner( const std::vector<World> & ws, int i )
{
	for ( int guard = 0; i >= 0 && guard < 16; guard++ ) {
		const World & w = ws[size_t( i )];
		if ( !w.climateEdid.empty() && !( w.parent >= 0 && ( w.useFlags & 0x10 ) ) )
			return i;
		i = w.parent;
	}
	return -1;
}

/*! the place of worldspace i. recx(climateEdid, climatePlugin, climateLocal, out, file) answers rule 1 (may be empty) */
using RecxFn = std::function<bool( const World & climateOf, Place & out, std::string & file )>;

inline Resolved resolve( const std::vector<World> & ws, int i, const RecxFn & recx, int red = 0, int depth = 0 )
{
	Resolved r;
	if ( i < 0 || size_t( i ) >= ws.size() || depth > 16 )
		return r;
	const World & w = ws[size_t( i )];
	const int co = climateOwner( ws, i );
	if ( co >= 0 )
		r.climateEdid = ws[size_t( co )].climateEdid;
	// 1. the climate's .recx location
	if ( co >= 0 && recx && !( red & RedRecx ) ) {
		std::string file;
		if ( recx( ws[size_t( co )], r.place, file ) ) {
			r.rule = RuleRecx;
			r.from = co;
			r.name = file;
			return r;
		}
	}
	// 2. the table by worldspace
	if ( !( red & RedTable ) )
		if ( const Builtin * b = builtin( w.plugin, w.edid, false ) ) {
			r.place.latDeg = b->lat;
			r.place.lonDeg = b->lon;
			r.rule = RuleTableWorld;
			r.from = i;
			r.name = b->name;
			return r;
		}
	// 3. a child takes its parent's
	if ( w.parent >= 0 && !( red & RedParent ) ) {
		Resolved p = resolve( ws, w.parent, recx, red, depth + 1 );
		if ( p.rule != RuleDefault ) {
			p.from = p.from >= 0 ? p.from : w.parent;
			p.rule = p.rule == RuleRecx ? RuleRecx : RuleParent;
			if ( p.climateEdid.empty() )
				p.climateEdid = r.climateEdid;
			return p;
		}
	}
	// 4. the table by climate
	if ( co >= 0 && !( red & RedTable ) )
		if ( const Builtin * b = builtin( ws[size_t( co )].climatePlugin, ws[size_t( co )].climateEdid, true ) ) {
			r.place.latDeg = b->lat;
			r.place.lonDeg = b->lon;
			r.rule = RuleTableClimate;
			r.from = co;
			r.name = b->name;
			return r;
		}
	// 5. Boston
	r.rule = RuleDefault;
	r.name = "Boston, Massachusetts";
	return r;
}

/*! an interior's worldspace: the one whose XLCN is on the cell's location chain (chain = the cell's XLCN, then each
 *  LCTN PNAM up, as load-order form ids, nearest first), else the first worldspace of the cell's own plugin the table
 *  places by worldspace, else the Commonwealth (Fallout4.esm), else -1. why = "location", "plugin", "commonwealth" */
inline int interiorWorld( const std::vector<World> & ws, const std::vector<std::uint32_t> & chain, const std::string & cellPlugin,
	std::string & why )
{
	for ( std::uint32_t loc : chain )
		for ( size_t i = 0; i < ws.size(); i++ )
			if ( loc && ws[i].location == loc ) {
				why = "location";
				return int( i );
			}
	const std::string cp = lower( cellPlugin );
	for ( size_t i = 0; i < ws.size(); i++ )
		if ( lower( ws[i].plugin ) == cp && builtin( ws[i].plugin, ws[i].edid, false ) ) {
			why = "plugin";
			return int( i );
		}
	for ( size_t i = 0; i < ws.size(); i++ )
		if ( lower( ws[i].plugin ) == "fallout4.esm" && lower( ws[i].edid ) == "commonwealth" ) {
			why = "commonwealth";
			return int( i );
		}
	why = "none";
	return -1;
}

/*! a .recx `location` block -> Place. Keys (case-free in the reader): latitude -90..90, longitude -180..180,
 *  timeZone hours -14..14 or "mean", startDate "YYYY-MM-DD". A bad or missing latitude/longitude refuses the whole block
 *  (why says which); a bad timeZone / startDate keeps the default and says so in warn. */
inline bool placeFromKeys( bool haveLat, double lat, bool haveLon, double lon, bool haveTz, bool tzMean, double tz,
	const std::string & date, Place & out, std::string & why, std::string & warn )
{
	if ( !haveLat || !std::isfinite( lat ) || lat < -90.0 || lat > 90.0 ) {
		why = "latitude missing or outside -90..90";
		return false;
	}
	if ( !haveLon || !std::isfinite( lon ) || lon < -180.0 || lon > 180.0 ) {
		why = "longitude missing or outside -180..180";
		return false;
	}
	Place p;
	p.latDeg = lat;
	p.lonDeg = lon;
	if ( haveTz && !tzMean ) {
		if ( std::isfinite( tz ) && tz >= -14.0 && tz <= 14.0 )
			p.tzHours = tz;
		else
			warn += "timeZone outside -14..14 hours: local mean time; ";
	}
	if ( !date.empty() ) {
		int y = 0, m = 0, d = 0;
		static const int dim[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };	// no leap years (the game's calendar)
		if ( std::sscanf( date.c_str(), "%d-%d-%d", &y, &m, &d ) == 3 && m >= 1 && m <= 12 && d >= 1 && d <= dim[m] ) {
			p.startYear = y;
			p.startMonth = m;
			p.startDay = d;
		} else {
			warn += "startDate not YYYY-MM-DD: 2287-10-23; ";
		}
	}
	out = p;
	return true;
}

}	// namespace wwplace

#endif // SKYPLACE_H
