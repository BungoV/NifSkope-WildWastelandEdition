/* lane CAMGATE1: unit test of the camera bounds rule (src/cellcambounds.h, namespace WwCamBounds), no Qt.
 *   g++ -std=c++17 -I<tree>/src tests/cambounds/cambounds_test.cpp -o cambounds_test         green: every check passes
 *   ... -DCAMBOUNDS_RED_UNDER   the old rule without the ground test: check "under" must FAIL
 *   ... -DCAMBOUNDS_RED_OUTSIDE the old rule without the block test: check "outside" must FAIL
 *   ... -DCAMBOUNDS_RED_FREEZE   (lane CAMGATE2) CAMGATE1's frozen view, the camera state left where it was refused:
 *                                checks "putback", "nextinput", "hold", "homeput", "roomkeep" must FAIL
 *   ... -DCAMBOUNDS_RED_INTERIOR (lane CAMGATE2) interiors not judged: "roomout", "roombelow", "roomkeep" must FAIL
 *   ... -DCAMBOUNDS_RED_LOOSEBOX (lane CAMGATE3) CAMGATE2's one box of the origins: "clumps", "setboxes", "setgap",
 *                                "sethall", "setmarker" must FAIL; on a real fixture "realin" / "realout" / "realtwin"
 *   cambounds_test --land <33 lines of 33 heights> <cx> <cy> <points file: x y per line>
 *       prints "x y ground" per point (the twin check against notes/camgate1/tools/camgate2.py ground())
 *   cambounds_test --room <fixture>
 *       (lane CAMGATE2) a real interior's placements (notes/fix1030b/camgate2/tools/interior_census.py --dump);
 *       (lane CAMGATE3) or a box-set fixture (notes/fix1030b/camgate3/tools/mk_fixtures.py, format at roomMode): the set
 *       holds the spawn, the navmesh, the room corners and the lane paths, refuses the void and the outside; rc 0 / 1 */
#define CAMBOUNDS_PURE
#include "cellcambounds.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace WwCamBounds;

static int g_fail = 0, g_checks = 0;
static void check( const char * name, bool ok, const char * what )
{
	g_checks++;
	if ( !ok )
		g_fail++;
	std::printf( "%s %-12s %s\n", ok ? "ok  " : "FAIL", name, what );
}

/* lane CAMGATE2: a toy free camera for WwCamBounds::Keeper. The STATE is an orbit pivot, a yaw and a distance (as
 * GLView's Pos / Rot / Dist); the VIEW is the eye it gives (pivot - dist x forward) and the yaw. A path camera hands
 * the gate a view that no state made. */
struct TS
{
	double p[3] = { 0, 0, 0 };
	double yaw = 0.0, dist = 64.0;
};
struct TV
{
	double e[3] = { 0, 0, 0 };
	double yaw = 0.0;
};
static TV viewOf( const TS & s )
{
	const double a = s.yaw * 3.14159265358979323846 / 180.0;
	TV v;
	v.e[0] = s.p[0] - s.dist * std::sin( a );
	v.e[1] = s.p[1] - s.dist * std::cos( a );
	v.e[2] = s.p[2];
	v.yaw = s.yaw;
	return v;
}
static bool sameS( const TS & a, const TS & b )
{
	return a.p[0] == b.p[0] && a.p[1] == b.p[1] && a.p[2] == b.p[2] && a.yaw == b.yaw && a.dist == b.dist;
}
static bool sameV( const TV & a, const TV & b )
{
	return a.e[0] == b.e[0] && a.e[1] == b.e[1] && a.e[2] == b.e[2] && a.yaw == b.yaw;
}

template<class JudgeFn>
struct ToyOps
{
	TS & cam;
	JudgeFn judgeEye;
	bool haveHome = false;
	TS homeState;
	int said = 0, puts = 0;
	Kept lastKept = Kept::None;
	bool same( const TV & a, const TV & b ) const { return sameV( a, b ); }
	Verdict judge( const TV & v, std::string * why, std::string * key ) const
	{
		const Verdict r = judgeEye( v.e[0], v.e[1], v.e[2] );
		if ( r != Verdict::Ok ) {
			*key = std::to_string( int( r ) ) + "|" + std::to_string( cellOf( v.e[0] ) ) + "," + std::to_string( cellOf( v.e[1] ) );
			*why = verdictName( r );
		}
		return r;
	}
	TS state() const { return cam; }
	void put( const TS & s )
	{
		cam = s;
		puts++;
	}
	TV rawNow() const { return viewOf( cam ); }
	bool home( TV & v, TS & s, std::string & w ) const
	{
		if ( !haveHome )
			return false;
		s = homeState;
		v = viewOf( s );
		w = "home";
		return true;
	}
	void say( const std::string &, Kept k, const std::string & )
	{
		said++;
		lastKept = k;
	}
};

/* lane CAMGATE3: a real interior through the box set (WwCamBounds::roomSetOf, the code cellCamBoundsKeep runs). Lines:
 *   x y z                                     (CAMGATE2 fixtures) a placement's origin
 *   r x y z rx ry rz s [b lx ly lz hx hy hz]  a placement: origin, rotation, scale, its base's OBND
 *   m x y z rx ry rz s hx hy hz               a RoomMarker / PortalMarker (XPRM half extents)
 *   # coc x y z                               the spawn eye (always in bounds)
 *   in x y z / out x y z                      must be in bounds / refused (notes/fix1030b/camgate3/tools/mk_fixtures.py)
 *   # expect boxes N                          the offline twin's box count (room_census.py tight_boxes) */
static int roomMode( const char * file )
{
	std::ifstream f( file );
	if ( !f )
		return 2;
	std::vector<Placed> P;
	std::vector<Room> marks;
	std::vector<std::array<double, 3>> ins, outs;
	bool haveCoc = false;
	double coc[3] = { 0, 0, 0 };
	int expect = -1;
	std::string line;
	while ( std::getline( f, line ) ) {
		std::istringstream s( line );
		std::string tag;
		if ( line.rfind( "# coc", 0 ) == 0 ) {
			std::string h, c;
			s >> h >> c >> coc[0] >> coc[1] >> coc[2];
			haveCoc = true;
			continue;
		}
		if ( line.rfind( "# expect boxes", 0 ) == 0 ) {
			std::string h, a, b;
			s >> h >> a >> b >> expect;
			continue;
		}
		if ( line.empty() || line[0] == '#' )
			continue;
		if ( !std::isalpha( (unsigned char) line[0] ) ) {	// CAMGATE2: an origin
			Placed p;
			if ( s >> p.pos[0] >> p.pos[1] >> p.pos[2] )
				P.push_back( p );
			continue;
		}
		s >> tag;
		double v[16];
		int n = 0;
		std::string b;
		if ( tag == "r" ) {
			for ( ; n < 7 && s >> v[n]; n++ ) {}
			if ( n < 7 )
				continue;
			Placed p;
			for ( int k = 0; k < 3; k++ )
				p.pos[k] = v[k];
			if ( s >> b && b == "b" ) {
				double o[6];
				int m = 0;
				for ( ; m < 6 && s >> o[m]; m++ ) {}
				if ( m == 6 )
					p.box = obndWorldBox( v, v + 3, v[6], o, o + 3 );
			}
			P.push_back( p );
		} else if ( tag == "m" ) {
			for ( ; n < 10 && s >> v[n]; n++ ) {}
			if ( n == 10 )
				marks.push_back( markerBox( v, v + 3, v[6], v + 7 ) );
		} else if ( tag == "in" || tag == "out" ) {
			std::array<double, 3> q;
			if ( s >> q[0] >> q[1] >> q[2] )
				( tag == "in" ? ins : outs ).push_back( q );
		}
	}
	RoomSet set = roomSetOf( P, marks );
	const size_t before = set.boxes.size();
	if ( haveCoc )
		set.include( coc[0], coc[1], coc[2], kRoomMargin );
	const Room u = set.bound();
	std::printf( "room %s: %zu placements, %zu markers, %zu boxes (%d clumps) within %.0f,%.0f,%.0f .. %.0f,%.0f,%.0f\n", file,
		P.size(), marks.size(), before, set.clumps, u.lo[0], u.lo[1], u.lo[2], u.hi[0], u.hi[1], u.hi[2] );
	if ( expect >= 0 ) {
		char what[96];
		std::snprintf( what, sizeof what, "%zu boxes = the offline twin's %d (room_census.py)", before, expect );
		check( "realtwin", int( before ) == expect, what );
	}
	if ( set.mainBox >= 0 ) {
		const Room & m = set.boxes[size_t( set.mainBox )];
		check( "realcentre", judgeRoomSet( set, 0.5 * ( m.lo[0] + m.hi[0] ), 0.5 * ( m.lo[1] + m.hi[1] ), 0.5 * ( m.lo[2] + m.hi[2] ) )
			== Verdict::Ok, "the biggest clump's box centre (the home with no spawn) is in bounds" );
	}
	if ( haveCoc )
		check( "realcoc", judgeRoomSet( set, coc[0], coc[1], coc[2] ) == Verdict::Ok, "the spawn (COC marker / door arrival) is in bounds" );
	int inBad = 0, outBad = 0;
	for ( const auto & q : ins )
		inBad += judgeRoomSet( set, q[0], q[1], q[2] ) != Verdict::Ok;
	for ( const auto & q : outs )
		outBad += judgeRoomSet( set, q[0], q[1], q[2] ) != Verdict::OutsideRoom;
	if ( !ins.empty() ) {
		char what[128];
		std::snprintf( what, sizeof what, "%zu points of the real interior (navmesh, room corners, spawns, lane paths): %d refused",
			ins.size(), inBad );
		check( "realin", inBad == 0, what );
	}
	if ( !outs.empty() ) {
		char what[128];
		std::snprintf( what, sizeof what, "%zu void / outside points (gaps between clumps, a path shot from outside): %d let through",
			outs.size(), outBad );
		check( "realout", outBad == 0, what );
	}
	for ( int k = 0; k < 3; k++ )
		for ( int sgn = -1; sgn <= 1; sgn += 2 ) {
			double e[3] = { 0.5 * ( u.lo[0] + u.hi[0] ), 0.5 * ( u.lo[1] + u.hi[1] ), 0.5 * ( u.lo[2] + u.hi[2] ) };
			e[k] = ( sgn < 0 ? u.lo[k] : u.hi[k] ) + sgn * kRoomMargin;
			char what[96];
			std::snprintf( what, sizeof what, "one margin past the set's %s%c face: refused", sgn < 0 ? "-" : "+", "xyz"[k] );
			check( "realface", judgeRoomSet( set, e[0], e[1], e[2] ) == Verdict::OutsideRoom, what );
		}
	std::printf( "%d checks, %d failures\n%s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS" );
	return g_fail ? 1 : 0;
}

// ---- lane CAMGATE3: the box set on synthetic data (each check names the red that must fail it)
static void setChecks()
{
	const double PI = 3.14159265358979323846;
	{
		// fromEuler(-rot), yaw +90 degrees: local (x, y) -> world (y, -x) (niftypes.cpp Matrix::fromEuler)
		const double pos[3] = { 1000, 2000, 0 }, rot[3] = { 0, 0, PI / 2 }, lo[3] = { -100, -10, 0 }, hi[3] = { 300, 10, 50 };
		const Room b = obndWorldBox( pos, rot, 2.0, lo, hi );
		check( "obndrot", std::fabs( b.lo[0] - 980 ) < 1e-6 && std::fabs( b.hi[0] - 1020 ) < 1e-6 && std::fabs( b.lo[1] - 1400 ) < 1e-6
			&& std::fabs( b.hi[1] - 2200 ) < 1e-6 && std::fabs( b.lo[2] ) < 1e-6 && std::fabs( b.hi[2] - 100 ) < 1e-6,
			"an OBND turned 90 degrees and scaled 2: x 980..1020, y 1400..2200, z 0..100" );
		const double half[3] = { 100, 10, 50 };
		const Room m = markerBox( pos, rot, 1.0, half );
		check( "markerrot", std::fabs( ( m.hi[0] - m.lo[0] ) - 20 ) < 1e-6 && std::fabs( ( m.hi[1] - m.lo[1] ) - 200 ) < 1e-6,
			"a room marker turned 90 degrees: its long axis lies along y" );
	}
	// two buildings 9000 apart (one clump each, the far one a single crate) and a long hall mesh whose origin is at
	// its west end (its OBND reaches 2600 east)
	std::vector<Placed> P;
	for ( int i = 0; i < 40; i++ ) {
		Placed p;
		p.pos[0] = ( i % 8 ) * 256.0;
		p.pos[1] = ( i / 8 ) * 256.0;
		p.pos[2] = 0.0;
		P.push_back( p );
	}
	Placed hall;
	hall.pos[0] = 0;
	hall.pos[1] = -512;
	{
		const double rot[3] = { 0, 0, 0 }, lo[3] = { 0, -128, 0 }, hi[3] = { 4600, 128, 400 };
		hall.box = obndWorldBox( hall.pos, rot, 1.0, lo, hi );
	}
	P.push_back( hall );
	Placed crate;
	crate.pos[0] = 9000;
	crate.pos[1] = 0;
	P.push_back( crate );
	std::vector<Room> marks;
	{
		const double pos[3] = { 900, 500, 300 }, rot[3] = { 0, 0, 0 }, half[3] = { 900, 600, 2500 };	// a tall stair room
		marks.push_back( markerBox( pos, rot, 1.0, half ) );
	}
	const RoomSet s = roomSetOf( P, marks );
	check( "clumps", s.clumps == 2, "two clumps: the building (with the hall's origin) and the far crate (red loosebox: 1)" );
	check( "setboxes", s.boxes.size() == 3, "three boxes: two clumps + one room marker (red loosebox: 1)" );
	check( "setin", judgeRoomSet( s, 1000, 600, 120 ) == Verdict::Ok, "an eye inside the building: ok" );
	check( "setcrate", judgeRoomSet( s, 9000, 100, 120 ) == Verdict::Ok, "an eye at the far crate: ok (its own box)" );
	check( "setgap", judgeRoomSet( s, 6000, 600, 120 ) == Verdict::OutsideRoom,
		"an eye in the void between the building and the crate: refused (red loosebox: inside the one box)" );
	check( "sethall", judgeRoomSet( s, 4400, -1100, 120 ) == Verdict::Ok,
		"an eye beside the far end of the hall mesh, 4400 past its origin: ok (red loosebox: refused)" );
	check( "setmarker", judgeRoomSet( s, 900, 500, 2600 ) == Verdict::Ok,
		"an eye high in the stair room marker, 2600 above every origin: ok (red loosebox: refused)" );
	check( "setbelow", judgeRoomSet( s, 1000, 600, -3000 ) == Verdict::OutsideRoom, "an eye far below everything: refused" );
	RoomSet h = s;
	h.include( 6000, 600, 120, kRoomMargin );
	check( "setspawn", judgeRoomSet( h, 6000, 600, 120 ) == Verdict::Ok && judgeRoomSet( h, 6400, 600, 120 ) == Verdict::Ok
		&& judgeRoomSet( h, 7000, 600, 120 ) == Verdict::OutsideRoom, "a spawn in the gap gets its own box (+ margin), no more" );
	check( "setempty", judgeRoomSet( RoomSet(), 1e6, 1e6, 1e6 ) == Verdict::Ok, "an empty set judges nothing" );
	std::vector<Placed> chain;
	for ( int i = 0; i < 12; i++ ) {
		Placed p;
		p.pos[0] = -5000.0 + i * 1000.0;	// 1000 apart: always in the same or the next 1024 voxel -> one clump
		p.pos[1] = -1.0;
		chain.push_back( p );
	}
	check( "clumpchain", clumpsOf( chain ).size() == 1, "origins 1000 apart along a corridor (crossing 0, negative cells): one clump" );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc == 3 && !std::strcmp( argv[1], "--room" ) )
		return roomMode( argv[2] );
	if ( argc == 6 && !std::strcmp( argv[1], "--land" ) ) {
		static float H[kGrid][kGrid];
		std::ifstream f( argv[2] );
		for ( int r = 0; r < kGrid; r++ )
			for ( int c = 0; c < kGrid; c++ )
				if ( !( f >> H[r][c] ) )
					return 2;
		const int cx = std::atoi( argv[3] ), cy = std::atoi( argv[4] );
		std::ifstream p( argv[5] );
		double x, y;
		while ( p >> x >> y )
			std::printf( "%.3f %.3f %.6f\n", x, y, groundIn( H, cx, cy, x, y ) );
		return 0;
	}

	// a 3 x 3 block round cell (10, -4): a plane z = 0.25 x - 0.5 y + 300 (bilinear reproduces a plane exactly), cell (11, -4)
	// has no LAND, and one cell of the 5 x 5 ring (13, -4) has LAND for the roaming check
	static float P[kGrid][kGrid];
	std::map<std::pair<int, int>, bool> haveLand;
	for ( int x = 8; x <= 13; x++ )
		for ( int y = -6; y <= -2; y++ )
			haveLand[{ x, y }] = !( x == 11 && y == -4 );
	auto plane = []( double x, double y ) { return 0.25 * x - 0.5 * y + 300.0; };
	std::map<std::pair<int, int>, std::pair<bool, float ( * )[kGrid]>> grids;
	static float G[64][kGrid][kGrid];
	int used = 0;
	auto land = [&]( int cx, int cy ) -> const float ( * )[kGrid] {
		auto it = haveLand.find( { cx, cy } );
		if ( it == haveLand.end() || !it->second )
			return nullptr;
		auto g = grids.find( { cx, cy } );
		if ( g == grids.end() ) {
			float ( *H )[kGrid] = G[used++];
			for ( int r = 0; r < kGrid; r++ )
				for ( int c = 0; c < kGrid; c++ )
					H[r][c] = float( plane( cx * kCellUnits + c * 128.0, cy * kCellUnits + r * 128.0 ) );
			g = grids.emplace( std::make_pair( cx, cy ), std::make_pair( true, H ) ).first;
		}
		return g->second.second;
	};
	(void) P;
	Box b;
	b.cx = 10;
	b.cy = -4;
	b.half = 1;
	const double x0 = 10.5 * kCellUnits, y0 = -3.5 * kCellUnits;	// the centre cell's middle
	const double g0 = plane( x0, y0 );
	double g = 0.0;

	check( "ground", judge( b, land, x0, y0, g0 + 500.0, &g ) == Verdict::Ok && std::fabs( g - g0 ) < 1e-3,
		"bilinear on a plane = the plane (the middle of the centre cell)" );
	{
		const double x = 10 * kCellUnits + 1000.37, y = -4 * kCellUnits + 77.9;
		double gg = 0.0;
		judge( b, land, x, y, 1e6, &gg );
		check( "plane", std::fabs( gg - plane( x, y ) ) < 1e-2, "an off-grid point reads the plane" );
	}
	check( "above", judge( b, land, x0, y0, g0 + kClear + 1.0 ) == Verdict::Ok, "eye at ground + 51: ok" );
	check( "under", judge( b, land, x0, y0, g0 - 100.0 ) == Verdict::Under, "eye at ground - 100: refused (the brief's red row)" );
	check( "underedge", judge( b, land, x0, y0, g0 + kClear - 1.0 ) == Verdict::Under, "eye at ground + 49: refused" );
	check( "outside", judge( b, land, ( 12 + 0.5 ) * kCellUnits, y0, 1e5 ) == Verdict::Outside,
		"eye one cell east of the 3 x 3, high in the air: refused" );
	check( "edgein", judge( b, land, 9 * kCellUnits + 0.5, y0, plane( 9 * kCellUnits + 0.5, y0 ) + 200.0 ) == Verdict::Ok,
		"eye half a unit inside the block's west edge: ok" );
	check( "edgeout", judge( b, land, 9 * kCellUnits - 0.5, y0, plane( 9 * kCellUnits - 0.5, y0 ) + 200.0 ) == Verdict::Outside,
		"eye half a unit outside the block's west edge (cell 8 has LAND): refused" );
	check( "noland", judge( b, land, 11.5 * kCellUnits, y0, 1e5 ) == Verdict::NoLand, "eye over a loaded cell with no LAND: refused" );
	Box roam = b;
	roam.roam = true;
	check( "roam", judge( roam, land, 13.5 * kCellUnits, y0, plane( 13.5 * kCellUnits, y0 ) + 200.0 ) == Verdict::Ok,
		"a streamed document: any cell with LAND" );
	check( "roamunder", judge( roam, land, 13.5 * kCellUnits, y0, plane( 13.5 * kCellUnits, y0 ) - 10.0 ) == Verdict::Under,
		"a streamed document: still never under the ground" );
	check( "origin", judge( b, land, 0.0, 0.0, 0.0 ) == Verdict::Outside, "a fresh document's world origin: refused" );
	check( "nan", judge( b, land, std::nan( "" ), y0, 1e4 ) == Verdict::Outside, "a NaN eye: refused" );
	check( "cellof", cellOf( -0.5 ) == -1 && cellOf( 4095.9 ) == 0 && cellOf( -4096.0 ) == -1 && cellOf( -4096.1 ) == -2,
		"cells floor toward minus infinity (the negative half of the map)" );

	// ---- lane CAMGATE2: the camera state snaps back (WwCamBounds::Keeper, the code GLView::wwCameraBoundsGate runs)
	{
		auto ext = [&]( double x, double y, double z ) { return judge( b, land, x, y, z ); };
		TS cam;
		cam.p[0] = x0;
		cam.p[1] = y0 + 64.0;	// the eye stands 64 south of the pivot: (x0, y0)
		cam.p[2] = g0 + 300.0;
		ToyOps<decltype( ext )> ops{ cam, ext };
		Keeper<TV, TS, std::string> K;
		auto ask = [&]( bool path = false, const TV & pv = TV() ) {
			const TV raw = path ? pv : viewOf( cam );
			return K.pass( raw, !path, true, ops );
		};
		TV drawn = ask();
		check( "start", sameV( drawn, viewOf( cam ) ) && K.why.empty(), "eye at ground + 300: drawn as asked" );
		// fly down 200 a step (the fly key): + 100 ok, then - 100 under the ground
		cam.p[2] -= 200.0;
		drawn = ask();
		const TS lastGood = cam;
		const TV lastGoodView = viewOf( cam );
		check( "stepok", sameV( drawn, lastGoodView ), "ground + 100: drawn as asked" );
		cam.p[2] -= 200.0;
		drawn = ask();
		check( "refused", sameV( drawn, lastGoodView ) && !K.why.empty(), "ground - 100: refused, the last in-bounds view drawn" );
		check( "putback", sameS( cam, lastGood ),
			"after the refusal the camera STATE is the kept pose (red freeze: it stays at the refused eye)" );
		// the next input starts from the kept pose: 100 north, level
		cam.p[1] += 100.0;
		drawn = ask();
		check( "nextinput", K.why.empty() && cam.p[2] == lastGood.p[2] && sameV( drawn, viewOf( cam ) ),
			"the next move starts at the kept height and is drawn (red: still under the ground, refused again)" );
		// hold the down key 10 frames: refused each frame, the state put back each frame, one notes line
		const TS before = cam;
		const int said0 = ops.said;
		bool allBack = true;
		for ( int i = 0; i < 10; i++ ) {
			cam.p[2] -= 200.0;
			drawn = ask();
			allBack = allBack && sameS( cam, before ) && sameV( drawn, viewOf( before ) );
		}
		check( "hold", allBack, "10 frames of the down key under the ground: the state is the kept pose after every one" );
		check( "onelog", ops.said - said0 == 1, "one `camera refused` line for the whole hold (kind + cell)" );
		// a path camera under the ground: the view is refused, the free camera's state is not written
		const TS pathBefore = cam;
		TV pv;
		pv.e[0] = x0 + 300.0;
		pv.e[1] = y0;
		pv.e[2] = g0 - 100.0;
		drawn = ask( true, pv );
		check( "path", !K.why.empty() && sameS( cam, pathBefore ) && sameV( drawn, K.good ),
			"a path camera's refused eye: last view kept, the free camera's state untouched" );
	}
	{
		// a fresh document whose first view is the world origin (the overview refit): the home pose, written back
		auto ext = [&]( double x, double y, double z ) { return judge( b, land, x, y, z ); };
		TS cam;	// pivot at the origin: outside the block
		ToyOps<decltype( ext )> ops{ cam, ext };
		ops.haveHome = true;
		ops.homeState.p[0] = x0;
		ops.homeState.p[1] = y0 + 64.0;
		ops.homeState.p[2] = g0 + kEyeHeight;
		Keeper<TV, TS, std::string> K;
		const TV drawn = K.pass( viewOf( cam ), true, true, ops );
		check( "homeview", sameV( drawn, viewOf( ops.homeState ) ), "the first view refused: the cell's home drawn" );
		check( "homeput", sameS( cam, ops.homeState ), "... and the camera state is the home pose (red freeze: still at the origin)" );
		ToyOps<decltype( ext )> none{ cam, ext };
		Keeper<TV, TS, std::string> K2;
		TS farS = cam;
		farS.p[0] = 0.0;
		farS.p[1] = 0.0;
		farS.p[2] = 0.0;
		cam = farS;
		const TV d2 = K2.pass( viewOf( cam ), true, true, none );
		check( "nohome", sameV( d2, viewOf( farS ) ) && sameS( cam, farS ) && none.lastKept == Kept::None,
			"no home and no last view: drawn as asked, nothing written" );
	}

	// ---- lane CAMGATE2: interiors, the room of the placements + kRoomMargin
	{
		Room r;
		const double P[][3] = { { -1000, -700, 0 }, { 1000, 800, 0 }, { 0, 0, 400 }, { 300, -200, 150 }, { -650, 420, 0 } };
		for ( const auto & p : P )
			r.add( p[0], p[1], p[2] );
		r.grow( kRoomMargin );
		check( "roomin", judgeRoom( r, 0, 0, 120 ) == Verdict::Ok, "an eye in the middle of the room: ok" );
		check( "roomedge", judgeRoom( r, 1000 + kRoomMargin - 1, 0, 120 ) == Verdict::Ok, "an eye 1 inside the margin: ok" );
		check( "roomout", judgeRoom( r, 1000 + kRoomMargin + 100, 0, 120 ) == Verdict::OutsideRoom,
			"an eye 100 past the margin (east): refused (red interior: ok)" );
		check( "roombelow", judgeRoom( r, 0, 0, -kRoomMargin - 500 ) == Verdict::OutsideRoom,
			"an eye 500 below the lowest placement's margin: refused" );
		check( "roomnan", judgeRoom( r, std::nan( "" ), 0, 0 ) == Verdict::OutsideRoom, "a NaN eye: refused" );
		Room empty;
		check( "roomempty", judgeRoom( empty, 1e6, 1e6, 1e6 ) == Verdict::Ok, "a room with no placements judges nothing" );
		Room h = r;
		h.include( 4000, 0, 100, kRoomMargin );
		check( "roomhome", h.holds( 4000, 0, 100 ) && h.holds( 4000 + kRoomMargin, 0, 100 ) && h.holds( 0, 0, 120 ),
			"a spawn outside the placements widens the room to the spawn + the margin" );
		// fly out through the east wall: the state is put back inside
		auto inr = [&]( double x, double y, double z ) { return judgeRoom( r, x, y, z ); };
		TS cam;
		cam.p[0] = 0.0;
		cam.p[1] = 64.0;
		cam.p[2] = 120.0;
		cam.yaw = 0.0;
		ToyOps<decltype( inr )> ops{ cam, inr };
		Keeper<TV, TS, std::string> K;
		K.pass( viewOf( cam ), true, true, ops );
		TS lastIn = cam;
		bool refusedOnce = false;
		for ( int i = 0; i < 20; i++ ) {
			cam.p[0] += 200.0;
			K.pass( viewOf( cam ), true, true, ops );
			if ( !K.why.empty() )
				refusedOnce = true;
			else
				lastIn = cam;
		}
		check( "roomkeep", refusedOnce && sameS( cam, lastIn ) && judgeRoom( r, viewOf( cam ).e[0], viewOf( cam ).e[1], viewOf( cam ).e[2] ) == Verdict::Ok,
			"flying east through the wall: refused, the camera state put back inside the room" );
	}
	setChecks();	// lane CAMGATE3
	std::printf( "%d checks, %d failures\n%s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS" );
	return g_fail ? 1 : 0;
}
