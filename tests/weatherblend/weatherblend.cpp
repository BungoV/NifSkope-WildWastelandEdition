// lane WEATHERBLEND1: the weather change blend, against the hand values of expected.py.
// cut.py writes cut.inc: the shared blend helpers and the EsmWeather blends cut by text from src/esmweather.cpp
// (or, for the red, the blends from the base commit's copy). run.sh builds and runs both.
#include "esmweather.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool wwLookdevRed( const char * )
{
	return false;
}

static WwTodKeys g_keys;
WwSkyClock wwSkyClock( double hour, const unsigned char[4], const WwSkyGmst & )
{
	WwSkyClock c;
	c.hour = hour;
	c.keys = g_keys;
	return c;
}

void wwFogPackK( WwFog & )
{
}

#include "cut.inc"

static int fails = 0, checks = 0;

// lane WEATHERBLEND6: the region walk on a synthetic streamed path. Fake regions: an area polygon, a rank and a
// weather list (no game data); the polygons are tested here (the product tests the REGN's RPLD in regionWeatherAt)
struct FakeRegion
{
	quint32 form;
	std::vector<float> poly;	// x0,y0, x1,y1, ...
	WwRegionRank rank;
	QVector<WwRegionWeatherEntry> list;
	const char * edid;
};

static bool inPoly( const std::vector<float> & p, float x, float y )
{
	bool in = false;
	const size_t n = p.size() / 2;
	for ( size_t i = 0, j = n - 1; i < n; j = i++ ) {
		const float xi = p[i * 2], yi = p[i * 2 + 1], xj = p[j * 2], yj = p[j * 2 + 1];
		if ( ( yi > y ) != ( yj > y ) && x < ( xj - xi ) * ( y - yi ) / ( yj - yi ) + xi )
			in = !in;
	}
	return in;
}

struct WalkLog
{
	int frames = 0, samples = 0, repicks = 0, holdSamples = 0;
	float repickX = -1.0f;
	quint32 weather = 0;	// the weather after the walk
	QString why;
	QVector<quint32> repickRegions;
};

/* the camera from x0 to x1 at y (speed units/s, 16 ms frames), holding still `holdS` seconds at holdX; samples when
 * due, the regions counting at the eye chosen by rank; a re-pick applies the follow law (elapsed, duration, draw, u01) */
static WalkLog walkPath( const std::vector<FakeRegion> & R, float x0, float x1, float y, float speed, float holdX,
	double holdS, quint32 cur, double elapsed, double duration, quint32 draw, double u01, int red )
{
	WalkLog log;
	log.weather = cur;
	WwRegionWalk w;
	qint64 ms = 0;
	float x = x0;
	double held = 0.0;
	bool holding = false;
	while ( x < x1 ) {
		log.frames++;
		const float eye[2] = { x, y };
		const int cx = int( std::floor( x / 4096.0f ) ), cy = int( std::floor( y / 4096.0f ) );
		if ( wwRegionWalkDue( w, ms, cx, cy, eye ) ) {
			QVector<WwRegionWeather> in;
			QVector<WwRegionRank> rank;
			QVector<quint32> under;
			for ( const FakeRegion & f : R ) {	// every fake cell lists every region (XCLR)
				WwRegionWeather g;
				g.hasWeather = true;
				g.areas = true;
				g.inside = inPoly( f.poly, x, y );
				g.rank = f.rank;
				g.list = f.list;
				g.form = f.form;
				g.edid = QString::fromLatin1( f.edid );
				if ( wwRegionCounts( g, 0u ) ) {
					in.append( g );
					rank.append( g.rank );
					under.append( g.form );
				}
			}
			const int k = wwRegionChoose( rank.constData(), int( rank.size() ) );
			log.samples++;
			if ( holding )
				log.holdSamples++;
			if ( wwRegionWalkSample( w, ms, cx, cy, eye, under, k < 0 ? 0u : in.at( k ).form, red ) ) {
				log.repicks++;
				log.repickRegions.append( w.region );
				if ( log.repicks == 1 )
					log.repickX = x;
				if ( k >= 0 ) {
					const quint32 p = wwRegionWalkFollow( in.at( k ).list.constData(), int( in.at( k ).list.size() ),
						log.weather, elapsed, duration, draw, u01, &log.why );
					if ( p )
						log.weather = p;
				}
			}
		}
		ms += 16;
		if ( !holding && held == 0.0 && x < holdX && x + speed * 0.016f >= holdX ) {
			holding = true;
			x = holdX;
		}
		if ( holding ) {
			held += 0.016;
			holding = held < holdS;
			if ( !holding )
				held = -1.0;
			continue;
		}
		x += speed * 0.016f;
	}
	return log;
}

static void near3( const char * what, const float got[3], double e0, double e1, double e2, double tol = 0.05 )
{
	checks++;
	const double e[3] = { e0, e1, e2 };
	bool ok = true;
	for ( int c = 0; c < 3; c++ )
		ok = ok && std::fabs( double( got[c] ) - e[c] ) <= tol;
	std::printf( "%s %s got %.3f,%.3f,%.3f want %.3f,%.3f,%.3f\n", ok ? "ok  " : "FAIL", what, got[0], got[1], got[2], e0, e1, e2 );
	fails += ok ? 0 : 1;
}

static void near1( const char * what, double got, double want, double tol = 1e-4 )
{
	checks++;
	const bool ok = std::fabs( got - want ) <= tol;
	std::printf( "%s %s got %.6f want %.6f\n", ok ? "ok  " : "FAIL", what, got, want );
	fails += ok ? 0 : 1;
}

static void setRow( WwWeatherData & w, int tod, int row, unsigned char r, unsigned char g, unsigned char b )
{
	w.color[tod][row][0] = r;
	w.color[tod][row][1] = g;
	w.color[tod][row][2] = b;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	auto old = std::make_shared<WwWeatherData>();
	WwWeatherData cur;
	old->edid = "Old";
	cur.edid = "New";
	setRow( cur, WwTodDay, WwRowSunlight, 20, 60, 220 );
	setRow( cur, WwTodSunset, WwRowSunlight, 240, 120, 10 );
	setRow( *old, WwTodDay, WwRowSunlight, 200, 100, 50 );
	setRow( *old, WwTodSunset, WwRowSunlight, 90, 200, 160 );
	cur.last = old;

	// 1. one key (Day): 0 % = the old weather, 50 % the CIELab midpoint, 100 % the new one
	WwTodKeys day;
	day.a = day.b = WwTodDay;
	day.t = 0.0f;
	const double one[3][3] = { { 199.999, 100.006, 50.002 }, { 157.790, 74.988, 137.226 }, { 20.033, 59.997, 220.000 } };
	const float pcts[3] = { 0.0f, 0.5f, 0.999999f };
	const char * names[3] = { "Day 0%", "Day 50%", "Day 100%" };
	for ( int i = 0; i < 3; i++ ) {
		cur.transPct = pcts[i];
		float rgb[3];
		EsmWeather::blendRowLab( cur, WwRowSunlight, day, rgb );
		near3( names[i], rgb, one[i][0], one[i][1], one[i][2] );
	}

	// 2. two keys (Day -> Sunset, t 0.25): the four weights
	WwTodKeys two;
	two.a = WwTodDay;
	two.b = WwTodSunset;
	two.t = 0.25f;
	const double tw[3][3] = { { 186.581, 128.473, 76.902 }, { 167.949, 98.673, 128.072 }, { 136.133, 69.051, 174.955 } };
	const char * names2[3] = { "Day->Sunset 0%", "Day->Sunset 50%", "Day->Sunset 100%" };
	for ( int i = 0; i < 3; i++ ) {
		cur.transPct = pcts[i];
		float rgb[3];
		EsmWeather::blendRowLab( cur, WwRowSunlight, two, rgb );
		near3( names2[i], rgb, tw[i][0], tw[i][1], tw[i][2] );
	}

	// 3. the linear rows and DALC: four-weight byte sums
	cur.transPct = 0.5f;
	{
		float rgb[3];
		EsmWeather::blendRow( cur, WwRowSunlight, day, rgb );
		near3( "row linear Day 50%", rgb, 110.0, 80.0, 135.0, 1e-3 );
		cur.hasDalc = old->hasDalc = true;
		cur.dalc[WwTodDay][4][0] = 100;
		old->dalc[WwTodDay][4][0] = 200;
		EsmWeather::blendDalc( cur, 4, day, rgb );
		near3( "dalc Z+ Day 50%", rgb, 150.0, 0.0, 0.0, 1e-3 );
		old->hasDalc = false;	// a leaving weather without DALC: its NAM0 Ambient
		setRow( *old, WwTodDay, WwRowAmbient, 50, 60, 70 );
		EsmWeather::blendDalc( cur, 4, day, rgb );
		near3( "dalc Z+ Day 50% (old has no DALC)", rgb, 75.0, 30.0, 35.0, 1e-3 );
	}

	// 4. clouds: a zero colour takes the other weather's; the alpha is linear
	{
		cur.cloudLayers = old->cloudLayers = 16;
		const unsigned char oc[3] = { 200, 100, 50 };
		std::memcpy( old->cloudColor[WwTodDay][2], oc, 3 );
		cur.cloudAlpha[WwTodDay][2] = 1.0f;
		old->cloudAlpha[WwTodDay][2] = 0.2f;
		float rgb[3], a = 0.0f;
		EsmWeather::blendCloud( cur, 2, day, rgb, &a );
		near3( "cloud colour, new zero 50%", rgb, 199.999, 100.006, 50.002 );
		near1( "cloud alpha 50%", a, 0.6 );
	}

	// 5. the sky scale and the fog: scalars on pct, the NAM4 scale on four weights
	{
		cur.skyScale[WwTodDay] = 2.0f;
		old->skyScale[WwTodDay] = 1.0f;
		near1( "sky scale 50%", EsmWeather::blendSkyScale( cur, day ), 1.5 );
		cur.fog[0] = 3000.0f;	// day near
		old->fog[0] = 1000.0f;
		cur.fogScale[0][WwTodDay] = 2.0f;
		old->fogScale[0][WwTodDay] = 1.0f;
		g_keys = day;
		const unsigned char tn[4] = { 30, 54, 102, 126 };
		WwSkyGmst g;
		const WwFog f = wwFogAt( cur, 12.0, tn, g );
		near1( "fog near (day) 50%", f.fogNear, 2000.0, 1e-2 );
		near1( "fog NAM4 near scale 50%", f.scale[0], 1.5 );
	}

	// 6. the change length (Fallout4.esm: fWeatherTransMax 3) and the pct on the wall clock
	{
		WwSkyGmst g;
		g.transMax = 3.0f;
		near1( "delta 0 hours", wwWeatherTransHours( 0, g ), 0.01, 1e-6 );
		near1( "delta 128 hours", wwWeatherTransHours( 128, g ), 1.510863, 1e-5 );
		near1( "delta 255 hours", wwWeatherTransHours( 255, g ), 3.0, 1e-5 );
		near1( "delta 128 pct at 135.9776 s", wwWeatherTransPct( wwWeatherTransGameHours( 135.9776 ), 128, g ), 0.5, 1e-4 );
		near1( "delta 255 pct at 540 s", wwWeatherTransPct( wwWeatherTransGameHours( 540.0 ), 255, g ), 1.0, 1e-6 );
		near1( "delta 255 pct at 0 s", wwWeatherTransPct( 0.0, 255, g ), 0.0, 1e-6 );
		near1( "delta 0 pct at 1 s", wwWeatherTransPct( wwWeatherTransGameHours( 1.0 ), 0, g ), 1.0 / 1.8, 1e-4 );
	}

	// 7. no change running: the old two-key blend, byte for byte
	{
		WwWeatherData plain = cur;
		plain.last.reset();
		plain.transPct = 1.0f;
		float a[3], b[3];
		EsmWeather::blendRowLab( plain, WwRowSunlight, two, a );
		wwLabBlend( plain.color[two.a][WwRowSunlight], plain.color[two.b][WwRowSunlight], two.t, b );
		checks++;
		const bool same = a[0] == b[0] * 255.0f && a[1] == b[1] * 255.0f && a[2] == b[2] * 255.0f;
		std::printf( "%s no change = the two-key blend, bit for bit\n", same ? "ok  " : "FAIL" );
		fails += same ? 0 : 1;
	}

	// 8. lane WEATHERBLEND2: the imagespace slots (four LUTs on the four weights, not a swap at half way)
	{
		const float ps[2] = { 0.3f, 0.7f };
		const double want[2][4] = { { 0.225, 0.075, 0.525, 0.175 }, { 0.525, 0.175, 0.225, 0.075 } };
		const char * nm[2][4] = { { "slot cur a 30%", "slot cur b 30%", "slot last a 30%", "slot last b 30%" },
			{ "slot cur a 70%", "slot cur b 70%", "slot last a 70%", "slot last b 70%" } };
		for ( int i = 0; i < 2; i++ ) {
			float w[4];
			wwImageSpaceSlots( 0.25f, 0.25f, ps[i], true, w );
			for ( int k = 0; k < 4; k++ )
				near1( nm[i][k], w[k], want[i][k], 1e-5 );
		}
		float w[4];
		wwImageSpaceSlots( 0.0f, 0.0f, 1.0f, false, w );	// no change, one key: cur a alone
		near1( "slot no change", w[0] + 10.0 * ( w[1] + w[2] + w[3] ), 1.0, 1e-6 );
		const std::vector<unsigned char> L[4] = { { 0, 100, 255 }, { 255, 0, 10 }, { 40, 40, 40 }, { 200, 220, 0 } };
		const std::vector<unsigned char> * lut[4] = { &L[0], &L[1], &L[2], &L[3] };
		wwImageSpaceSlots( 0.25f, 0.25f, 0.3f, true, w );
		std::vector<unsigned char> o;
		wwLutBlendN( lut, w, o );
		float got[3] = { float( o[0] ), float( o[1] ), float( o[2] ) };
		near3( "four LUTs at 30%", got, 75.0, 82.0, 79.0, 0.0 );
		lut[2] = nullptr;	// a LUT that did not load: the others renormalised
		wwLutBlendN( lut, w, o );
		float got3[3] = { float( o[0] ), float( o[1] ), float( o[2] ) };
		near3( "three LUTs at 30%", got3, 114.0, 128.0, 122.0, 0.0 );
	}

	// 9. fWeatherTransAccel (Fallout4.esm 16): from the pct at the pick on, (Accel + 1) times as fast
	{
		WwSkyGmst g;
		g.transMax = 3.0f;
		g.transAccel = 16.0f;
		near1( "accel 0.2 raw 0.21", wwWeatherTransAccelPct( 0.21f, 0.2f, g ), 0.37, 1e-5 );
		near1( "accel 0.2 raw 0.25", wwWeatherTransAccelPct( 0.25f, 0.2f, g ), 1.0, 1e-6 );
		near1( "no accel", wwWeatherTransAccelPct( 0.21f, -1.0f, g ), 0.21, 1e-6 );
		const float raw = wwWeatherTransPct( wwWeatherTransGameHours( 143.9763 ), 128, g );
		near1( "delta 128 accelerated at half way ends at 143.98 s", wwWeatherTransAccelPct( raw, 0.5f, g ), 1.0, 1e-4 );
	}

	// 10. the pick: a running change defers it; force-weather and an interior snap
	{
		near1( "pick during a change waits", wwWeatherPickRule( true, false, false, false ), WwPickDefer, 0.0 );
		near1( "pick of the current weather keeps the change", wwWeatherPickRule( true, true, false, false ), WwPickKeep, 0.0 );
		near1( "pick with no change starts one", wwWeatherPickRule( false, false, false, false ), WwPickStart, 0.0 );
		near1( "pick in an interior snaps", wwWeatherPickRule( false, false, true, false ), WwPickSnap, 0.0 );
		near1( "force-weather during a change snaps", wwWeatherPickRule( true, false, false, true ), WwPickSnap, 0.0 );
	}

	// 11. lane WEATHERBLEND3: the Sky flags. Fast travel (0x10) applies a waiting pick at once, snapped; a wait or a
	// teleport (0x1000000) snaps a pick that would start a change
	{
		near1( "fast travel applies the waiting pick", wwWeatherSnapRule( WwPickDefer, WwSnapFastTravel ), WwPickSnap, 0.0 );
		near1( "a wait snaps a new change", wwWeatherSnapRule( WwPickStart, WwSnapPassTime ), WwPickSnap, 0.0 );
		near1( "a wait leaves a waiting pick", wwWeatherSnapRule( WwPickDefer, WwSnapPassTime ), WwPickDefer, 0.0 );
		near1( "fast travel keeps the same weather", wwWeatherSnapRule( WwPickKeep, WwSnapFastTravel ), WwPickKeep, 0.0 );
		near1( "a wait 22h -> 2h is 4 game hours", wwWeatherWaitHours( 22.0, 2.0 ), 4.0, 1e-9 );
		near1( "a wait 10h -> 13.5h", wwWeatherWaitHours( 10.0, 13.5 ), 3.5, 1e-9 );
	}

	// 12. the pct integral (expected.py integrates the pct numerically): delta 255, 1..3 h -> 540 real seconds
	{
		WwSkyGmst g;
		g.transMin = 1.0f;
		g.transMax = 3.0f;
		g.transAccel = 16.0f;
		near1( "pct integral 270 s", wwWeatherTransPctSeconds( 270.0, 0.0, 255, g, -1.0f ), 67.5, 1e-3 );
		near1( "pct integral past the end", wwWeatherTransPctSeconds( 600.0, 0.0, 255, g, -1.0f ), 330.0, 1e-3 );
		near1( "pct integral after a 1 h wait", wwWeatherTransPctSeconds( 180.0, 1.0, 255, g, -1.0f ), 90.0, 1e-3 );
		near1( "pct integral accelerated at 0.5", wwWeatherTransPctSeconds( 280.0, 0.0, 255, g, 0.5f ), 74.074074, 1e-3 );
	}

	// 13. the layer's one scroll: base + old speed x secs + (new - old) x the pct integral, wrapped
	{
		near1( "one scroll mid change", wwCloudScroll( 0.25, 0.5f, 1.5f, 10.0, 4.0 ), 0.15, 1e-5 );
		near1( "one scroll, no change, base 0 = the closed form", wwCloudScroll( 0.0, 0.7f, 0.7f, 3.0, 0.0 ), 0.21, 1e-5 );
	}

	// 14. the wind: speed / turbulence unrounded; the range offset seeded, the nearest to 0 of 4 draws in +- range / 2
	{
		near1( "wind speed at 30% unrounded", wwWeatherWindBlend( 200, 51, 0.3f ), 95.7, 1e-4 );
		near1( "no range, no offset", wwWindRangeOffset( 64, 0, 7u ), 0.0, 0.0 );
		const float o1 = wwWindRangeOffset( 64, 255, 0x1234u ), o2 = wwWindRangeOffset( 64, 255, 0x1234u );
		near1( "the offset repeats for a weather", double( o1 - o2 ), 0.0, 0.0 );
		double sum = 0.0, mx = 0.0;
		for ( quint32 k = 1; k <= 4000; k++ ) {
			const double x = std::fabs( double( wwWindRangeOffset( 64, 255, k ) ) );
			sum += x;
			mx = std::max( mx, x );
		}
		near1( "mean |offset| = range / 2 / 5 (nearest of 4)", sum / 4000.0, 18.0, 1.0 );
		near1( "|offset| within range / 2", mx <= 90.0 ? 1.0 : 0.0, 1.0, 0.0 );
	}

	// 14. lane WEATHERBLEND4: the hour box the shortest way round (only a forward step is a wait)
	{
		near1( "hour step 22h -> 2h", wwWeatherHourStep( 22.0, 2.0 ), 4.0, 1e-9 );
		near1( "hour step 2h -> 1h is backward", wwWeatherHourStep( 2.0, 1.0 ), -1.0, 1e-9 );
		near1( "hour step 1h -> 23h is backward", wwWeatherHourStep( 1.0, 23.0 ), -2.0, 1e-9 );
		near1( "hour step 10h -> 13.5h", wwWeatherHourStep( 10.0, 13.5 ), 3.5, 1e-9 );
	}

	// 15. the region list pick (r % sum over the bins in order; a zero chance holds no bin) and the region choice
	{
		const WwRegionWeatherEntry L[3] = { { 0xA, 60 }, { 0xB, 0 }, { 0xC, 40 } };
		near1( "pick r 59 -> A", wwRegionWeatherPick( L, 3, 59u ), 0xA, 0.0 );
		near1( "pick r 60 -> C (B holds no bin)", wwRegionWeatherPick( L, 3, 60u ), 0xC, 0.0 );
		near1( "pick r 199 -> C (mod the sum)", wwRegionWeatherPick( L, 3, 199u ), 0xC, 0.0 );
		const WwRegionWeatherEntry Z[2] = { { 0xA, 0 }, { 0xB, -5 } };
		near1( "pick of no chances -> none", wwRegionWeatherPick( Z, 2, 7u ), 0.0, 0.0 );
		int hit[2] = { 0, 0 };
		for ( quint32 k = 1; k <= 10000; k++ )
			hit[wwRegionWeatherPick( L, 3, wwRegionDraw( k ) ) == 0xA ? 0 : 1]++;
		near1( "drawn picks follow the chances (60 %)", hit[0] / 10000.0, 0.6, 0.02 );
		const WwRegionRank p[2] = { { false, 5 }, { false, 9 } };
		near1( "choose: the higher priority", wwRegionChoose( p, 2 ), 1.0, 0.0 );
		const WwRegionRank o[2] = { { false, 9 }, { true, 1 } };
		near1( "choose: an override beats priority", wwRegionChoose( o, 2 ), 1.0, 0.0 );
		const WwRegionRank q[2] = { { true, 9 }, { false, 99 } };
		near1( "choose: an override keeps its place", wwRegionChoose( q, 2 ), 0.0, 0.0 );
		const WwRegionRank t[2] = { { false, 5 }, { false, 5 } };
		near1( "choose: a tie keeps the first", wwRegionChoose( t, 2 ), 0.0, 0.0 );
		near1( "choose: none", wwRegionChoose( t, 0 ), -1.0, 0.0 );
	}

	// 16. lane WEATHERBLEND5: which regions count, the weather's duration and elapsed hours, the keep-or-pick law
	{
		WwRegionWeather g;
		g.hasWeather = true;
		near1( "counts: no area is never inside", wwRegionCounts( g, 7u ) ? 1.0 : 0.0, 0.0, 0.0 );
		near1( "counts: red noareainside", wwRegionCounts( g, 7u, WwRegionRedNoAreaInside ) ? 1.0 : 0.0, 1.0, 0.0 );
		g.areas = g.inside = true;
		g.deleted = true;
		near1( "counts: a deleted region is skipped", wwRegionCounts( g, 7u ) ? 1.0 : 0.0, 0.0, 0.0 );
		near1( "counts: red noregionskip keeps it", wwRegionCounts( g, 7u, WwRegionRedNoSkip ) ? 1.0 : 0.0, 1.0, 0.0 );
		g.deleted = false;
		g.world = 5u;
		near1( "counts: another worldspace is skipped", wwRegionCounts( g, 7u ) ? 1.0 : 0.0, 0.0, 0.0 );
		near1( "counts: its own worldspace", wwRegionCounts( g, 5u ) ? 1.0 : 0.0, 1.0, 0.0 );
		const WwSkyGmst gm;
		near1( "duration: volatility 0 -> 23 h", wwWeatherDurationHours( 0, 1.0f, gm ), 23.0, 1e-9 );
		near1( "duration: volatility 128", wwWeatherDurationHours( 128, 1.0f, gm ), 1.0 + 22.0 * 127.0 / 255.0, 1e-9 );
		near1( "duration: 200 x VNAM 2 clamps to 255 -> 1 h", wwWeatherDurationHours( 200, 2.0f, gm ), 1.0, 1e-9 );
		WwSkyGmst g0;
		g0.durMin = g0.durMax = 0.0f;
		near1( "duration: at least 0.1 h", wwWeatherDurationHours( 0, 1.0f, g0 ), 0.1, 1e-9 );
		near1( "elapsed 12h -> 14h", wwWeatherElapsedHours( 14.0, 12.0 ), 2.0, 1e-9 );
		near1( "elapsed 23h -> 1h over midnight", wwWeatherElapsedHours( 1.0, 23.0 ), 2.0, 1e-9 );
		const WwRegionWeatherEntry L[2] = { { 0xA, 60 }, { 0xC, 40 } };
		near1( "follow: same region, inside its duration: kept", wwRegionWeatherFollow( L, 2, 0xA, true, 2.0, 5.0, 70u, 0.5 ), 0xA, 0.0 );
		near1( "follow: same region, duration over: picked", wwRegionWeatherFollow( L, 2, 0xA, true, 6.0, 5.0, 70u, 0.5 ), 0xC, 0.0 );
		near1( "follow: new region, u > elapsed/duration: kept", wwRegionWeatherFollow( L, 2, 0xA, false, 1.0, 4.0, 70u, 0.9 ), 0xA, 0.0 );
		near1( "follow: new region, u <= elapsed/duration: picked", wwRegionWeatherFollow( L, 2, 0xA, false, 1.0, 4.0, 70u, 0.1 ), 0xC, 0.0 );
		near1( "follow: an unlisted weather: picked", wwRegionWeatherFollow( L, 2, 0xD, true, 0.0, 4.0, 10u, 0.9 ), 0xA, 0.0 );
	}

	// 17. lane WEATHERBLEND6: the region walk. Two fake regions side by side (A x < 8192, B x >= 8192, one cell row),
	// a low-priority C inside A; the camera runs 1000 -> 15000 at 600 units/s in 16 ms frames, 2 s still at 3000.
	// Red WW_CELL_REGION_RED=spawnonly (as the viewer's switch) or the base build: no re-pick, the crossing fails
	{
		const int red = std::getenv( "WW_CELL_REGION_RED" ) && std::strstr( std::getenv( "WW_CELL_REGION_RED" ), "spawnonly" )
			? int( WwRegionWalkRedSpawnOnly ) : 0;
		if ( red )
			std::printf( "red: WW_CELL_REGION_RED=spawnonly\n" );
		const quint32 W1 = 0x100, W2 = 0x200, W3 = 0x300;
		const std::vector<FakeRegion> R = {
			{ 0xA0, { 0, 0, 8192, 0, 8192, 4096, 0, 4096 }, { false, 10 }, { { W1, 100 } }, "RegionA" },
			{ 0xB0, { 8192, 0, 16384, 0, 16384, 4096, 8192, 4096 }, { false, 10 }, { { W1, 50 }, { W2, 50 } }, "RegionB" },
			{ 0xC0, { 5000, 0, 6000, 0, 6000, 4096, 5000, 4096 }, { false, 1 }, { { W3, 100 } }, "RegionC" } };
		// keep: W1 listed in B, elapsed / duration 1 / 4 = 0.25 < u01 0.9 -> W1 kept
		const WalkLog keep = walkPath( R, 1000.0f, 15000.0f, 2048.0f, 600.0f, 3000.0f, 2.0, W1, 1.0, 4.0, 70u, 0.9, red );
		near1( "walk: one re-pick on the path (A -> B)", keep.repicks, 1.0, 0.0 );
		near1( "walk: the re-pick enters B", keep.repickRegions.isEmpty() ? 0.0 : double( keep.repickRegions.at( 0 ) ), 0xB0, 0.0 );
		near1( "walk: it fires at the first sample past the border (x - 8192 in [0, 153.6))",
			keep.repickX >= 8192.0f && keep.repickX < 8192.0f + 600.0f * 0.256f ? 1.0 : 0.0, 1.0, 0.0 );
		near1( "walk: keep rule, u01 0.9 > 0.25: W1 kept", keep.weather, W1, 0.0 );
		near1( "walk: the reason names u01", keep.why.contains( QLatin1String( "u01 >" ) ) ? 1.0 : 0.0, 1.0, 0.0 );
		near1( "walk: at most one sample a 250 ms (not per frame)",
			keep.samples <= keep.frames * 16 / 250 + 1 && keep.samples * 10 < keep.frames ? 1.0 : 0.0, 1.0, 0.0 );
		near1( "walk: standing still 2 s, only the sample that sees it arrive", keep.holdSamples <= 1 ? 1.0 : 0.0, 1.0, 0.0 );
		// change by the duration rule: elapsed 5 > duration 4 -> B's draw (70 of 100 -> W2)
		const WalkLog dur = walkPath( R, 1000.0f, 15000.0f, 2048.0f, 600.0f, 3000.0f, 2.0, W1, 5.0, 4.0, 70u, 0.9, red );
		near1( "walk: duration over: B's draw W2 starts", dur.weather, W2, 0.0 );
		near1( "walk: the reason names the duration rule", dur.why.contains( QLatin1String( "duration rule" ) ) ? 1.0 : 0.0, 1.0, 0.0 );
		// change by u01: u01 0.1 <= 0.25 -> W2
		const WalkLog lo = walkPath( R, 1000.0f, 15000.0f, 2048.0f, 600.0f, 3000.0f, 2.0, W1, 1.0, 4.0, 70u, 0.1, red );
		near1( "walk: u01 0.1 <= 0.25: W2 starts", lo.weather, W2, 0.0 );
		near1( "walk: the reason names u01 <=", lo.why.contains( QLatin1String( "u01 <=" ) ) ? 1.0 : 0.0, 1.0, 0.0 );
		// inside one region (cell borders at 4096, C's area entered and left: the set changes, A still wins): none
		const WalkLog inA = walkPath( R, 100.0f, 8100.0f, 2048.0f, 600.0f, 3000.0f, 2.0, W1, 1.0, 4.0, 70u, 0.1, red );
		near1( "walk: inside A (cells and C crossed): no re-pick", inA.repicks, 0.0, 0.0 );
		near1( "walk: inside A the weather stays", inA.weather, W1, 0.0 );
		// the follow law at a re-pick, case by case (B's list: W1 50, W2 50)
		const WwRegionWeatherEntry B[2] = { { W1, 50 }, { W2, 50 } };
		QString why;
		near1( "walk follow: not listed -> the draw", wwRegionWalkFollow( B, 2, W3, 1.0, 4.0, 70u, 0.9, &why ), W2, 0.0 );
		near1( "walk follow: why not listed", why.contains( QLatin1String( "does not list" ) ) ? 1.0 : 0.0, 1.0, 0.0 );
		near1( "walk follow: the draw is cur -> kept", wwRegionWalkFollow( B, 2, W1, 5.0, 4.0, 10u, 0.0, &why ), W1, 0.0 );
		near1( "walk follow: why the draw", why.contains( QLatin1String( "draw is the current" ) ) ? 1.0 : 0.0, 1.0, 0.0 );
		const WwRegionWeatherEntry none[1] = { { W1, 0 } };
		near1( "walk follow: no chances -> nothing", wwRegionWalkFollow( none, 1, W1, 1.0, 4.0, 10u, 0.0, &why ), 0.0, 0.0 );
		// due: the first time; not within 250 ms; not without a move; on a cell change or a move after 250 ms
		WwRegionWalk w;
		const float e0[2] = { 10.0f, 10.0f }, e1[2] = { 10.5f, 10.0f }, e2[2] = { 30.0f, 10.0f };
		near1( "walk due: first", wwRegionWalkDue( w, 0, 0, 0, e0 ) ? 1.0 : 0.0, 1.0, 0.0 );
		wwRegionWalkSample( w, 0, 0, 0, e0, {}, 0u, red );
		near1( "walk due: not within 250 ms", wwRegionWalkDue( w, 249, 1, 0, e2 ) ? 1.0 : 0.0, 0.0, 0.0 );
		near1( "walk due: not without a move", wwRegionWalkDue( w, 1000, 0, 0, e1 ) ? 1.0 : 0.0, 0.0, 0.0 );
		near1( "walk due: a move after 250 ms", wwRegionWalkDue( w, 250, 0, 0, e2 ) ? 1.0 : 0.0, 1.0, 0.0 );
		near1( "walk due: a cell change after 250 ms", wwRegionWalkDue( w, 250, 1, 0, e0 ) ? 1.0 : 0.0, 1.0, 0.0 );
		std::printf( "walk: %d frames, %d samples, %d re-pick(s) at x %.1f\n", keep.frames, keep.samples, keep.repicks,
			double( keep.repickX ) );
	}

	std::printf( "WEATHERBLEND %s %d of %d failed\n", fails ? "FAIL" : "PASS", fails, checks );
	return fails ? 1 : 0;
}
