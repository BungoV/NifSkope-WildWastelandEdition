// lane WEATHERBLEND1: the weather change blend, against the hand values of expected.py.
// cut.py writes cut.inc: the shared blend helpers and the EsmWeather blends cut by text from src/esmweather.cpp
// (or, for the red, the blends from the base commit's copy). run.sh builds and runs both.
#include "esmweather.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

	std::printf( "WEATHERBLEND %s %d of %d failed\n", fails ? "FAIL" : "PASS", fails, checks );
	return fails ? 1 : 0;
}
