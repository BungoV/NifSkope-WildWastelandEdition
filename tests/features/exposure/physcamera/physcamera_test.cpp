// lane MODULES1 G1 (bungo 11-09: "maybe auto exposure is not working properly"): the physical camera's law
// (src/features/exposure/physcamera.h, the code the app runs) against bungo's two Goodneighbor flights
// (tests/fixtures/physcamera_flight1109.tsv: every `phys camera` line, grouped by the camera's eye = a turn on the spot).
//   g++ -std=c++17 -Isrc tests/features/exposure/physcamera/physcamera_test.cpp ; a.exe tests/fixtures/physcamera_flight1109.tsv
// -DPHYSCAMERA_RED runs the claim rows on the red (WW_PHYSUNITS_CAMERA_RED=1, the law before G1): B, C and G must fail.
// Bars, set before the first run:
//   A  the red law reproduces the logged `applied` within 0.06 EV on >= 90 % of the unpinned lines (the port is right)
//   B  night: every spot of 8+ lines but at most one swings <= 0.5 EV (the one: the start spot, where one view meters
//      2.03 cd/m2); all night targets span <= 1.1 EV (the flights: 6.99) and >= 95 % sit within 0.5 EV of his pin 2.2
//   C  day rows the old guard closed 2+ EV (the land black from high up): the target is <= the incident EV and >= 1 EV
//      under the old one; no guard at night; a guarded target never passes the incident EV
//   D  by day no line is darker than before
//   E  adaptation: 63.2 % after one tau (0.48 s brighter, 1.44 s darker), no overshoot, frame-rate independent, snaps
//   F  the red == a verbatim copy of the law and the meter before G1, bit for bit
//   G  meter: a street (log-normal, 1 EV) with 5 % of its cover 8 EV over it moves < 0.2 EV (before >= 0.25), with
//      20 % < 0.75 EV (before > 1.4); no emitters: the cap moves the mean < 0.01 EV. (Planned as 0.15 / 0.6 from a
//      rough estimate; the closed form of the trimmed mean gives 0.146 / 0.67, so the bars were set to 0.2 / 0.75
//      before the first run.)
#include "features/exposure/physcamera.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#ifdef PHYSCAMERA_RED
static const bool kArmRed = true;
#else
static const bool kArmRed = false;
#endif

static int fails = 0;

static void row( const char * id, bool ok, const char * fmt, ... ) __attribute__(( format( printf, 3, 4 ) ));
static void row( const char * id, bool ok, const char * fmt, ... )
{
	char b[512];
	va_list ap;
	va_start( ap, fmt );
	std::vsnprintf( b, sizeof( b ), fmt, ap );
	va_end( ap );
	std::printf( "%-4s %s  %s\n", id, ok ? "ok  " : "FAIL", b );
	if ( !ok )
		fails++;
}

// the law before G1, verbatim (physunits.cpp wwPhysExposure at 41f3c0ed; s.* and the switches as arguments)
static float oldExposure( bool hasKey, float h8, float adapted, bool incident, float incEv, float dayW, float hiSpread )
{
	const float la = std::max( adapted, 1e-6f );
	const float ev = std::log2( la * 100.0f / 12.5f );
	auto key = []( float l ) { return 1.03f - 2.0f / ( 2.0f + std::log10( l + 1.0f ) ); };
	const float ecEye = std::clamp( std::log2( std::max( key( la ), 1e-3f ) / key( 4000.0f ) ), -3.0f, 0.5f );
	const float ecKey = hasKey ? std::clamp( std::log2( std::max( h8, 1e-3f ) * 1.2f * 8.0f ), -4.0f, 4.0f ) : 0.0f;
	float base = ev - ecEye - ecKey;
	if ( incident )
		base += ( std::clamp( base, incEv - 1.5f, incEv + 1.5f ) - base ) * dayW;
	float guard = 0.0f;
	if ( hiSpread > 0.0f )
		guard = std::clamp( std::log2( la * std::exp2( hiSpread ) / 1.2f ) - base, 0.0f, 2.5f );
	return std::clamp( base + guard, -6.0f, 18.0f );
}

// the meter before G1, verbatim (physunits.cpp wwPhysMeter at 41f3c0ed, from the sort on)
static float oldMeter( std::vector<std::pair<float, float>> & t, float & hiSpread )
{
	double cover = 0.0;
	for ( const auto & e : t )
		cover += e.second;
	if ( t.empty() || !( cover > 0.0 ) )
		return -1.0f;
	std::sort( t.begin(), t.end() );
	const double lo = 0.02 * cover, hi = 0.98 * cover;
	double run = 0.0, sum = 0.0, wsum = 0.0;
	float p97 = t.back().first;
	bool p97Set = false;
	for ( const auto & [lg, a] : t ) {
		const double w = std::max( 0.0, std::min( run + a, hi ) - std::max( run, lo ) );
		run += a;
		sum += w * lg;
		wsum += w;
		if ( !p97Set && run >= 0.97 * cover ) {
			p97 = lg;
			p97Set = true;
		}
	}
	if ( !( wsum > 0.0 ) )
		return -1.0f;
	hiSpread = std::max( 0.0f, p97 - float( sum / wsum ) );
	return float( std::exp2( sum / wsum ) );
}

struct Line
{
	int log = 0, spot = -1;
	float la = 0, ecKey = 0, incEv = 0, dayW = 0, p97 = 0, applied = 0;
	bool incOn = false, pinned = false;
};

static wwphys::CameraIn inputOf( const Line & l, bool red )
{
	wwphys::CameraIn in;
	in.la = l.la;
	in.hasKey = true;
	in.key = std::exp2( l.ecKey ) / 9.6f;	// the line prints the key's EV: log2( h[8] x 1.2 x 8 )
	in.day = l.incOn && l.dayW > 0.0f;		// the line's incident flag = outdoors with daylight (no pin, no red in the flights)
	in.incident = in.day;
	in.incEv = l.incEv;
	in.dayW = l.dayW;
	in.hiSpread = l.p97;
	in.red = red;
	return in;
}

static unsigned long long lcgState = 0x1109u;
static double lcg()
{
	lcgState = lcgState * 6364136223846793005ull + 1442695040888963407ull;
	return double( ( lcgState >> 11 ) + 1 ) / 9007199254740994.0;	// (0, 1)
}
static float gauss()
{
	return float( std::sqrt( -2.0 * std::log( lcg() ) ) * std::cos( 6.283185307179586 * lcg() ) );
}

// a street: n texels log-normal round 0.3 cd/m2 (1 EV), `share` of them emitters 8 EV over the middle
static std::vector<std::pair<float, float>> street( int n, double share )
{
	std::vector<std::pair<float, float>> t;
	const float mid = std::log2( 0.3f );
	for ( int i = 0; i < n; i++ )
		t.emplace_back( double( i ) + 0.5 < share * n ? mid + 8.0f : mid + gauss(), 1.0f );
	return t;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 2 ) {
		std::printf( "usage: physcamera_test <tests/fixtures/physcamera_flight1109.tsv>\n" );
		return 2;
	}
	std::vector<Line> lines;
	{
		std::ifstream f( argv[1] );
		std::string s;
		while ( std::getline( f, s ) ) {
			if ( s.empty() || s[0] == '#' )
				continue;
			std::istringstream is( s );
			Line l;
			int seq = 0, inc = 0, pin = 0;
			if ( !( is >> l.log >> seq >> l.spot >> l.la >> l.ecKey >> inc >> l.incEv >> l.dayW >> l.p97 >> l.applied >> pin ) ) {
				std::printf( "bad fixture line: %s\n", s.c_str() );
				return 2;
			}
			l.incOn = inc != 0;
			l.pinned = pin != 0;
			lines.push_back( l );
		}
	}
	if ( lines.size() < 1000 ) {
		std::printf( "fixture: %zu lines (1694 expected)\n", lines.size() );
		return 2;
	}
	std::printf( "physcamera_test: %zu lines, arm %s\n", lines.size(), kArmRed ? "RED (the law before G1)" : "green" );

	// A + F: the red against the log and against the verbatim copy
	{
		int n = 0, near = 0, same = 0;
		for ( const Line & l : lines ) {
			if ( l.pinned )
				continue;
			const wwphys::CameraIn in = inputOf( l, true );
			const float red = wwphys::cameraTarget( in ).target;
			n++;
			near += std::fabs( red - l.applied ) <= 0.06f;
			same += red == oldExposure( true, in.key, in.la, in.incident, in.incEv, in.dayW, in.hiSpread );
		}
		row( "A", near >= 0.9 * n, "the red within 0.06 EV of the logged applied: %d of %d lines (bar 90 %%)", near, n );
		row( "F1", same == n, "the red == the law before G1, bit for bit: %d of %d flight lines", same, n );
		int m = 0, eq = 0;
		for ( float lg = -7.0f; lg <= 17.0f; lg += 0.125f )
			for ( int v = 0; v < 12; v++ ) {
				wwphys::CameraIn in;
				in.la = std::exp2( lg );
				in.hasKey = v % 2 == 0;
				in.key = 0.05f + 0.03f * float( v );
				in.day = v % 3 != 0;
				in.incident = in.day && v % 4 != 1;
				in.incEv = 9.0f + 0.6f * float( v );
				in.dayW = in.day ? 0.25f * float( 1 + v % 4 ) : 0.0f;
				in.hiSpread = 0.5f * float( v % 7 );
				in.red = true;
				m++;
				eq += wwphys::cameraTarget( in ).target == oldExposure( in.hasKey, in.key, in.la, in.incident, in.incEv, in.dayW, in.hiSpread );
			}
		row( "F2", eq == m, "the red == the law before G1 on a sweep (2^-7..2^17 cd/m2 x 12 states): %d of %d", eq, m );
	}

	// B: night, a turn on the spot
	{
		struct Spot { int n = 0; float lo = 1e9f, hi = -1e9f, oldLo = 1e9f, oldHi = -1e9f, laHi = 0; };
		std::map<int, Spot> spots;
		float lo = 1e9f, hi = -1e9f, oldLo = 1e9f, oldHi = -1e9f;
		int n = 0, near = 0, guarded = 0;
		for ( const Line & l : lines ) {
			if ( l.pinned || l.dayW > 0.0f )
				continue;
			const wwphys::CameraOut o = wwphys::cameraTarget( inputOf( l, kArmRed ) );
			Spot & s = spots[l.log * 100000 + l.spot];
			s.n++;
			s.lo = std::min( s.lo, o.target ); s.hi = std::max( s.hi, o.target );
			s.oldLo = std::min( s.oldLo, l.applied ); s.oldHi = std::max( s.oldHi, l.applied );
			s.laHi = std::max( s.laHi, l.la );
			lo = std::min( lo, o.target ); hi = std::max( hi, o.target );
			oldLo = std::min( oldLo, l.applied ); oldHi = std::max( oldHi, l.applied );
			n++;
			near += std::fabs( o.target - 2.2f ) <= 0.5f;
			guarded += o.guard > 0.0f;
		}
		int big = 0, wide = 0;
		for ( const auto & [id, s] : spots ) {
			if ( s.n < 8 )
				continue;
			big++;
			const bool w = s.hi - s.lo > 0.5f;
			wide += w;
			std::printf( "     night spot %6d: %3d lines, logged %.2f..%.2f (swing %.2f EV) -> target %.2f..%.2f (swing %.2f EV), La up to %.3g cd/m2%s\n",
				id, s.n, double( s.oldLo ), double( s.oldHi ), double( s.oldHi - s.oldLo ), double( s.lo ), double( s.hi ), double( s.hi - s.lo ),
				double( s.laHi ), w ? "  <- over 0.5 EV" : "" );
		}
		row( "B1", big >= 8 && wide <= 1, "night spots of 8+ lines swinging over 0.5 EV: %d of %d (bar: at most 1)", wide, big );
		row( "B2", hi - lo <= 1.1f, "all night targets %.2f..%.2f EV, swing %.2f EV (bar 1.1; the flights logged %.2f..%.2f, swing %.2f)",
			double( lo ), double( hi ), double( hi - lo ), double( oldLo ), double( oldHi ), double( oldHi - oldLo ) );
		row( "B3", near >= 0.95 * n, "night lines within 0.5 EV of his pin 2.2: %d of %d (bar 95 %%)", near, n );
		row( "C3", guarded == 0, "night lines the highlight guard darkens: %d of %d (bar 0)", guarded, n );
	}

	// C + D: day
	{
		int n = 0, darker = 0, hard = 0, hardOk = 0, past = 0;
		float worst = 0.0f, exOld = 0.0f, exNew = 0.0f, exInc = 0.0f;
		for ( const Line & l : lines ) {
			if ( l.pinned || !( l.dayW > 0.0f ) || !l.incOn )
				continue;
			const wwphys::CameraOut o = wwphys::cameraTarget( inputOf( l, kArmRed ) );
			const wwphys::CameraOut r = wwphys::cameraTarget( inputOf( l, true ) );
			n++;
			if ( o.target > r.target + 1e-4f ) {
				darker++;
				worst = std::max( worst, o.target - r.target );
			}
			past += o.guard > 0.0f && o.target > l.incEv + 1e-3f;
			if ( r.guard >= 2.0f ) {
				hard++;
				const bool ok = o.target <= l.incEv + 1e-3f && o.target <= r.target - 1.0f + 1e-3f;
				hardOk += ok;
				if ( r.target - o.target > exOld - exNew ) {
					exOld = r.target; exNew = o.target; exInc = l.incEv;
				}
			}
		}
		row( "C1", hard > 0 && hardOk == hard, "day lines the old guard closed 2+ EV: %d of %d now <= the incident EV and 1+ EV more open (e.g. %.2f -> %.2f, incident %.2f)",
			hardOk, hard, double( exOld ), double( exNew ), double( exInc ) );
		row( "C2", past == 0, "guarded day lines past the incident EV: %d of %d (bar 0)", past, n );
		row( "D", darker == 0, "day lines darker than before: %d of %d (worst +%.3f EV; bar 0)", darker, n, double( worst ) );
	}

	// E: adaptation
	{
		using namespace wwphys;
		const float up = cameraAdapt( 0.0f, 1.0f, kAdaptTauBrighter ), down = cameraAdapt( 1.0f, 0.0f, kAdaptTauDarker );
		row( "E1", std::fabs( up - 0.632f ) < 0.01f && std::fabs( down - 0.368f ) < 0.01f,
			"one tau: to a brighter scene %.3f of the way in %.2f s, to a darker %.3f in %.2f s (bar 0.632 +- 0.01)",
			double( up ), double( kAdaptTauBrighter ), double( 1.0f - down ), double( kAdaptTauDarker ) );
		bool over = false;
		for ( float dt = 0.001f; dt < 200.0f; dt *= 1.7f ) {
			const float a = cameraAdapt( 2.0f, 5.0f, dt ), b = cameraAdapt( 5.0f, 2.0f, dt );
			over = over || a < 2.0f || a > 5.0f || b < 2.0f || b > 5.0f;
		}
		row( "E2", !over && cameraAdapt( 2.0f, 5.0f, 100.0f ) == 5.0f, "no overshoot from 1 ms to 200 s steps, a long step lands on the target" );
		float a = 3.0f, b = 3.0f;
		for ( int i = 0; i < 6; i++ )
			a = cameraAdapt( a, 6.0f, 0.08f );
		for ( int i = 0; i < 60; i++ )
			b = cameraAdapt( b, 6.0f, 0.008f );
		row( "E3", std::fabs( a - b ) < 1e-4f, "frame-rate independent: 6 x 80 ms %.5f == 60 x 8 ms %.5f", double( a ), double( b ) );
		row( "E4", cameraAdapt( 3.0f, 6.0f, 0.0f ) == 3.0f && cameraAdapt( 3.0f, 6.0f, -1.0f ) == 3.0f && cameraAdapt( 3.0f, 6.0f, std::nanf( "" ) ) == 3.0f,
			"a zero, negative or NaN step holds the EV" );
		const bool snaps = cameraSnap( false, 0.016f, 10.0f, 10.0f, true ) && cameraSnap( true, 1.5f, 10.0f, 10.0f, true )
			&& cameraSnap( true, 0.016f, 10.6f, 10.0f, true ) && cameraSnap( true, 0.016f, 10.0f, 10.0f, false )
			&& cameraSnap( true, -1.0f, 10.0f, 10.0f, true ) == false && cameraSnap( true, std::nanf( "" ), 10.0f, 10.0f, true );
		row( "E5", snaps && !cameraSnap( true, 0.016f, 10.2f, 10.0f, true ),
			"snap on: first frame, a 1.5 s stall, an incident step of 0.6 EV, a standing world clock; fade on a 16 ms frame" );
	}

	// F3 + G: the meter
	{
		int m = 0, eq = 0;
		for ( int k = 0; k < 40; k++ ) {
			std::vector<std::pair<float, float>> t;
			const int n = 1 + int( lcg() * 600.0 );
			for ( int i = 0; i < n; i++ )
				t.emplace_back( -4.0f + 3.0f * gauss(), k % 2 ? 1.0f : float( 0.01 + lcg() ) );
			std::vector<std::pair<float, float>> u = t;
			float hs = 0.0f;
			const float o = oldMeter( u, hs );
			const wwphys::MeterOut r = wwphys::cameraMeter( t, true );
			m++;
			eq += r.mean == o && r.hiSpread == hs && r.capShare == 0.0f;
		}
		row( "F3", eq == m, "the red meter == the meter before G1, bit for bit: %d of %d random fields", eq, m );
		auto shift = []( double share, bool red, float & capShare ) {
			lcgState = 0x1109u;
			std::vector<std::pair<float, float>> plain = street( 20000, 0.0 );
			lcgState = 0x1109u;
			std::vector<std::pair<float, float>> lit = street( 20000, share );
			const wwphys::MeterOut a = wwphys::cameraMeter( plain, red ), b = wwphys::cameraMeter( lit, red );
			capShare = b.capShare;
			return std::log2( b.mean / a.mean );
		};
		float c5 = 0, c20 = 0, c0 = 0, x = 0;
		const float n5 = shift( 0.05, kArmRed, c5 ), o5 = shift( 0.05, true, x );
		const float n20 = shift( 0.20, kArmRed, c20 ), o20 = shift( 0.20, true, x );
		row( "G1", n5 < 0.2f && o5 >= 0.25f, "5 %% of the cover 8 EV over the street moves the meter %.3f EV (bar < 0.2; before %.3f, bar >= 0.25), cap holds %.1f %% of the cover",
			double( n5 ), double( o5 ), double( c5 * 100.0f ) );
		row( "G2", n20 < 0.75f && o20 > 1.4f, "20 %% of the cover 8 EV over the street moves the meter %.3f EV (bar < 0.75; before %.3f, bar > 1.4), cap holds %.1f %%",
			double( n20 ), double( o20 ), double( c20 * 100.0f ) );
		lcgState = 0x1109u;
		std::vector<std::pair<float, float>> p1 = street( 20000, 0.0 );
		std::vector<std::pair<float, float>> p2 = p1;
		const wwphys::MeterOut a = wwphys::cameraMeter( p1, false ), b = wwphys::cameraMeter( p2, true );
		c0 = a.capShare;
		row( "G3", std::fabs( std::log2( a.mean / b.mean ) ) < 0.01f && c0 < 0.005f, "no emitters: the cap moves the meter %.4f EV (bar 0.01), holds %.2f %% of the cover (bar 0.5 %%)",
			double( std::log2( a.mean / b.mean ) ), double( c0 * 100.0f ) );
	}

	std::printf( "PHYSCAMERA RESULT %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
