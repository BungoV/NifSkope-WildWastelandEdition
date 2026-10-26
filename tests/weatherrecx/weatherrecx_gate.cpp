/* lane WX1: the weather .recx reader (src/gl/weatherrecx.cpp) through the shipped math, no GL and no window.
 * Usage: weatherrecx_gate <fixture dir>   (sub-folders full, merge, prec, bad-values, case-free, each a Data root)
 * Prints the drawn values per arm and exits 1 on any failed check. Reds (env WW_WEATHER_RECX_RED=ignore|recordfirst)
 * must fail. Build: tests/weatherrecx/build.sh. */
#include "gl/atmomath.h"
#include "gl/celllooks.h"
#include "gl/cloudmath.h"
#include "gl/weatherrecx.h"
#include "gl/weatherrecxmath.h"
#include "recx.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QThread>

#include <cmath>
#include <cstdio>
#include <cstring>

// ---- stubs for the Lookdev stage (the weather and hour the gate picks)
static bool g_row = false;
static double g_hour = 8.0;
static QString g_root;
bool wwLookOn( WwLook l )
{
	return l == WwLook::WeatherRecx && g_row;
}
double wwLookdevHour()
{
	return g_hour;
}
QString wwLookdevWeatherKey()
{
	return g_root;	// a new root = a new weather: re-resolved at once
}
bool wwLookdevWeatherId( quint32 & formID, QString & edid, QString & owner, QStringList & files, QString & dataDir )
{
	formID = 0x01000801u;	// ExampleMod.esp (load order 1) | 0x000801
	edid = QStringLiteral( "ExampleFoggy" );
	owner = QStringLiteral( "ExampleMod.esp" );
	files = { g_root + QStringLiteral( "/Fallout4.esm" ), g_root + QStringLiteral( "/ExampleMod.esp" ) };
	dataDir = g_root;
	return true;
}

using namespace WwAtmoMath;
using namespace WwCloudMath;

namespace
{
int fails = 0;

void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "PASS" : "FAIL", what );
	if ( !ok )
		fails++;
}

bool near( double a, double b, double tol = 1e-4 )
{
	return std::fabs( a - b ) <= tol * std::max( 1.0, std::fabs( b ) );
}

// the record the gate draws: fog Far 30000 units, Max 0.7, Mid 0, Range 20000; clouds Lower 0.3 (layer 0), Upper 0.2
// (layer 3); stars x1, moonlight x1; eye 50 m above sea level
struct Drawn
{
	float haze[3], trans[3];
	BandParams bands[3];
	float stars, moon;
};

Drawn draw( bool recordFog = true, bool recordCloud = true )
{
	Drawn d;
	const float fog[4] = { recordFog ? 30000.0f : 0.0f, recordFog ? 0.7f : 0.0f, 0.0f, 20000.0f };
	// atmosphere.cpp wwAtmoDrawSky / wwAtmoKeyLight
	hazeFromFog( fog[0], fog[1], fog[2], fog[3], false, d.haze );
	wwWeatherRecxHaze( d.haze, fog[0] > 0.0f && fog[1] > 0.0f, kRay[1] + kMieE );
	cpuTrans( 0.0f, 0.70710678f, d.haze, false, d.trans, wwWeatherRecxOzone() );
	// cloudmap.cpp paramsOf
	const WwWeatherRecx & wx = wwWeatherRecx();
	const int role[2] = { Lower, Upper }, layer[2] = { 0, 3 };
	const float cover[2] = { recordCloud ? 0.3f : 0.0f, recordCloud ? 0.2f : 0.0f };
	float height[2][2] = { { -1, -1 }, { -1, -1 } };
	bool own = false;
	for ( int i = 0; i < 2; i++ )
		if ( wx.on && wx.layers > 0 && wx.layerBase[layer[i]] >= 0.0f ) {
			WwWeatherRecxMath::layerAboveEye( wx.layerBase[layer[i]], wx.layerTop[layer[i]], 50.0f, height[i][0], height[i][1] );
			own = true;
		}
	bandsOf( role, cover, nullptr, 2, d.bands, false, own ? height : nullptr );
	if ( wx.on ) {
		float c[3] = { d.bands[0].coverage, d.bands[1].coverage, d.bands[2].coverage };
		float n[3] = { d.bands[0].density, d.bands[1].density, d.bands[2].density };
		WwWeatherRecxMath::cloudsFromFile( c, n, wx.coverage, wx.density, wx.recordFirst );
		for ( int b = 0; b < 3; b++ ) {
			d.bands[b].coverage = c[b];
			d.bands[b].density = n[b];
		}
	}
	// lookdevstage.cpp drawStars / drawAtmo
	d.stars = wx.on && wx.stars >= 0.0f ? wx.stars : 1.0f;
	d.moon = wx.on && wx.moonlight >= 0.0f ? wx.moonlight : 1.0f;
	return d;
}

void print( const char * arm, const Drawn & d )
{
	std::printf( "%-10s haze %.6g %.6g %.6g | trans45 %.6g %.6g %.6g | low cov %.4f base %.1f top %.1f den %.4f |"
		" mid cov %.4f base %.1f top %.1f den %.4f | stars x%.3g moon x%.3g\n", arm, d.haze[0], d.haze[1], d.haze[2],
		d.trans[0], d.trans[1], d.trans[2], d.bands[0].coverage, d.bands[0].baseM, d.bands[0].topM, d.bands[0].density,
		d.bands[1].coverage, d.bands[1].baseM, d.bands[1].topM, d.bands[1].density, d.stars, d.moon );
	std::fflush( stdout );
}

void use( const QString & root, bool row, double hour )
{
	g_root = root;
	g_row = row;
	g_hour = hour;
	qputenv( "WW_RECX_ROOT", root.toLocal8Bit() );
	QThread::msleep( 120 );	// past the reader's 100 ms re-resolve
}
}	// namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	if ( argc < 2 ) {
		std::fprintf( stderr, "usage: weatherrecx_gate <fixture dir>\n" );
		return 2;
	}
	const QString fx = QDir::fromNativeSeparators( QString::fromLocal8Bit( argv[1] ) );
	const QString empty = QDir::tempPath() + QStringLiteral( "/wx1_gate_empty" );
	QDir().mkpath( empty );

	// 0. the record alone (no file): the reference picture
	use( empty, true, 8.0 );
	const Drawn rec = draw();
	print( "record", rec );
	check( !wwWeatherRecx().on, "no file: nothing set" );

	// 1. row off with a file present: byte-identical to the record
	use( fx + QStringLiteral( "/full" ), false, 8.0 );
	const Drawn off = draw();
	print( "row-off", off );
	check( std::memcmp( &off, &rec, sizeof( Drawn ) ) == 0, "row off = the record, byte for byte" );

	// 2. row on, the full case at 08:00: the file's values win over the record's
	use( fx + QStringLiteral( "/full" ), true, 8.0 );
	const WwWeatherRecx & w = wwWeatherRecx();
	const Drawn on = draw();
	print( "full", on );
	check( w.on && near( w.visibilityM, 3600.0 ), "visibility at 08:00 = 1200 (02:00) .. 6000 (14:00) -> 3600 m" );
	check( near( w.hazeHeightM, 400.0 ) && near( w.ozone, 1.1 ), "hazeHeight 400 m; ozone 1.2 (02:00, block's) .. 1 (14:00) -> 1.1" );
	check( near( w.coverage, 0.6 ) && near( w.density, 1.4 ), "coverage one hour key = 0.6 all day; density 1.4" );
	check( w.layers == 2 && near( w.layerBase[0], 900 ) && near( w.layerTop[3], 4800 ), "layers 0 (900-2400) and 3 (3200-4800)" );
	check( near( w.stars, 0.3 ) && near( w.moonlight, 0.8 ), "stars one hour key = 0.3; moonlight 0.8" );
	check( near( on.haze[0], WwWeatherRecxMath::hazeFromVisibility( 3600.0f, kRay[1] + kMieE ) ) && !near( on.haze[0], rec.haze[0] ),
		"precedence: .recx visibility > the record's fog (haze moves)" );
	check( near( on.haze[1], 0.4 ) && on.trans[2] != rec.trans[2], "hazeHeight -> scale height 0.4 km; ozone moves the 45-deg transmittance" );
	{
		const float c3[3] = { on.bands[0].coverage, on.bands[1].coverage, on.bands[2].coverage };
		const float r3[3] = { rec.bands[0].coverage, rec.bands[1].coverage, rec.bands[2].coverage };
		check( near( unionCover( c3, 3 ), 0.6, 1e-4 ) && near( c3[0] / c3[1], r3[0] / r3[1], 0.05 ) && !near( unionCover( r3, 3 ), 0.6, 1e-2 ),
			"coverage: the bands' union 0.44 -> 0.6, each band keeps about its share" );
	}
	check( near( on.bands[0].baseM, 850.0 ) && near( on.bands[1].baseM, 3150.0 ), "layer heights above sea level -> above the 50 m eye" );
	check( near( on.bands[0].density, rec.bands[0].density * 1.4f ), "density x1.4" );
	check( near( on.stars, 0.3 ) && near( on.moon, 0.8 ), "night: stars x0.3, moonlight x0.8" );

	// 3. the record without fog / cloud and no file: the default
	use( empty, true, 8.0 );
	const Drawn def = draw( false, false );
	print( "default", def );
	check( def.haze[0] == 0.0f && def.bands[0].coverage == 0.0f, "no file, no record value: the default (no haze, clear)" );
	// the file over a record without fog: still the file
	use( fx + QStringLiteral( "/full" ), true, 8.0 );
	const Drawn fileOnly = draw( false, false );
	print( "file-only", fileOnly );
	check( near( fileOnly.haze[0], on.haze[0] ) && fileOnly.bands[0].coverage == 0.0f,
		"file over the default: haze from the file; a weather with no cloud stays clear" );

	// 4. merge by provider: ExampleMod (a loaded plugin) < F4FS (no plugin: after all)
	use( fx + QStringLiteral( "/merge" ), true, 8.0 );
	const WwWeatherRecx & m = wwWeatherRecx();
	print( "merge", draw() );
	check( near( m.visibilityM, 2000 ) && m.coverage < 0.0f && m.layers == 1 && near( m.layerBase[0], 700 ) && near( m.stars, 2 ),
		"merge: atmosphere from ExampleMod, clouds whole from F4FS (no ExampleMod coverage), night from F4FS" );

	// 5. ranks: the form entry beats the wildcard in the same file; the wildcard still gives night
	use( fx + QStringLiteral( "/prec" ), true, 8.0 );
	const WwWeatherRecx & p = wwWeatherRecx();
	print( "prec", draw() );
	check( near( p.visibilityM, 2000 ) && near( p.stars, 4 ), "ranks: form 2000 m over wildcard 9000 m; wildcard night kept" );

	// 6. bad values: clamped, dropped and warned (stderr)
	use( fx + QStringLiteral( "/bad-values" ), true, 6.0 );
	const WwWeatherRecx & b = wwWeatherRecx();
	print( "bad", draw() );
	check( near( b.visibilityM, 100 ) && b.hazeHeightM < 0.0f && near( b.ozone, 0 ), "visibility 50 -> 100; hazeHeight \"deep\" dropped; ozone -1 -> 0" );
	check( near( b.coverage, 0 ) && near( b.density, 2 ), "coverage at 06:00: the hour's -0.2 -> 0; density 2" );
	check( b.layers == 1 && near( b.layerBase[0], 600 ) && near( b.layerTop[0], 2000 ), "layers: 0 base 300 -> 600; 1 below-base, 2 missing, 40, 5 dropped" );
	check( near( b.stars, 10 ) && b.moonlight < 0.0f, "stars 12 -> 10; moonlight null dropped" );

	// 7. case-free names; "Weather"/"Name" too
	use( fx + QStringLiteral( "/case-free" ), true, 8.0 );
	const WwWeatherRecx & c = wwWeatherRecx();
	print( "case-free", draw() );
	check( near( c.visibilityM, 3000 ) && c.layers == 1 && near( c.layerBase[4], 1500 ) && near( c.moonlight, 2 ),
		"case-free: ATMOSPHERE/Visibility, Clouds/LAYERS/Base/TOP, moonLight" );

	// 8. hot reload: a changed file moves the value with no restart
	const QString hot = QDir::tempPath() + QStringLiteral( "/wx1_gate_hot" );
	QDir( hot ).removeRecursively();
	QDir().mkpath( hot + QStringLiteral( "/Plugins/F4FS/Weathers" ) );
	const QString hf = hot + QStringLiteral( "/Plugins/F4FS/Weathers/ExampleFoggy.recx" );
	auto put = [&]( const char * text ) {
		QFile f( hf );
		f.open( QIODevice::WriteOnly | QIODevice::Truncate );
		f.write( text );
	};
	put( "{\"entries\":[{\"weather\":\"ExampleMod.esp|0x00000801\",\"atmosphere\":{\"visibility\":4000}}]}" );
	use( hot, true, 8.0 );
	const float v1 = wwWeatherRecx().visibilityM;
	put( "{\"entries\":[{\"weather\":\"ExampleMod.esp|0x00000801\",\"atmosphere\":{\"visibility\":12345}}]}" );
	QThread::msleep( 700 );	// recx.cpp rescans every 500 ms
	const float v2 = wwWeatherRecx().visibilityM;
	std::printf( "hot        visibility %.6g -> %.6g\n", double( v1 ), double( v2 ) );
	check( near( v1, 4000 ) && near( v2, 12345 ), "hot reload: 4000 -> 12345 m without a restart" );
	QDir( hot ).removeRecursively();

	std::printf( "%s %d failed\n", fails ? "WEATHERRECX FAIL" : "WEATHERRECX PASS", fails );
	return fails ? 1 : 0;
}
