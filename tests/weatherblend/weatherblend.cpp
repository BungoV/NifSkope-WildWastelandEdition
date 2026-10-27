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

	std::printf( "WEATHERBLEND %s %d of %d failed\n", fails ? "FAIL" : "PASS", fails, checks );
	return fails ? 1 : 0;
}
