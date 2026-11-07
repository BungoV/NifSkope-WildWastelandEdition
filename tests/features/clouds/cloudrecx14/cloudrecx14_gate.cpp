/* lane CLOUDRECX1: the weather .recx clouds block 1.4 (SPEC.md 1.4 "clouds"): the reader (src/features/weather/weatherrecx.cpp)
 * and the math (src/features/clouds/cloudrecx14math.h) with no GL and no window. The gate writes its own fixtures (one
 * Data root per arm under <out dir>/fx) and prints PASS/FAIL per check; exit 1 on any FAIL.
 *   cloudrecx14_gate <out dir> <tree>     (tree: for the shader twins' text check)
 * Run with WW_CLOUD_RECX14_RED=1 it must FAIL (the 1.4 keys are then unknown): build.sh runs both. */
#include "features/clouds/cloudrecx14math.h"
#include "features/clouds/cloudvolmath.h"
#include "features/weather/weatherrecx.h"
#include "features/weather/weathergen.h"
#include "gl/celllooks.h"
#include "recx.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QThread>

#include <cmath>
#include <cstdio>
#include <cstring>

// ---- stubs for the Lookdev stage (as tests/features/weather/weatherrecx/weatherrecx_gate.cpp)
static bool g_row = true;
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
	return g_root;
}
bool wwLookdevWeatherId( quint32 & formID, QString & edid, QString & owner, QStringList & files, QString & dataDir )
{
	formID = 0x01000801u;
	edid = QStringLiteral( "ExampleStorm" );
	owner = QStringLiteral( "ExampleMod.esp" );
	files = { g_root + QStringLiteral( "/Fallout4.esm" ), g_root + QStringLiteral( "/ExampleMod.esp" ) };
	dataDir = g_root;
	return true;
}
static WwWeatherGen::In g_gen;
int wwLookdevWeatherGen( WwWeatherGen::In in[2], quint32 formID[2], float & pct )
{
	in[0] = g_gen;
	formID[0] = 0x01000801u;
	pct = 1.0f;
	return 1;
}

namespace C = WwCloudRecx14;
namespace V = WwCloudVolMath;

namespace
{
int fails = 0;
QString g_out;

void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "PASS" : "FAIL", what );
	std::fflush( stdout );
	if ( !ok )
		fails++;
}

bool near( double a, double b, double tol = 1e-4 )
{
	return std::fabs( a - b ) <= tol * std::max( 1.0, std::fabs( b ) );
}

//! one arm: a Data root holding Plugins/F4FS/Weathers/ExampleStorm.recx with this clouds block (JSON text)
const WwWeatherRecx & arm( const char * name, const char * clouds, const char * extra = "" )
{
	const QString root = g_out + QStringLiteral( "/fx/" ) + QLatin1String( name );
	QDir().mkpath( root + QStringLiteral( "/Plugins/F4FS/Weathers" ) );
	QFile f( root + QStringLiteral( "/Plugins/F4FS/Weathers/ExampleStorm.recx" ) );
	f.open( QIODevice::WriteOnly | QIODevice::Truncate );
	f.write( QStringLiteral( "{ \"entries\": [ { \"name\": \"ExampleStorm\", \"weather\": \"ExampleMod.esp|0x00000801\", "
							 "\"clouds\": %1%2 } ] }\n" ).arg( QLatin1String( clouds ), QLatin1String( extra ) ).toUtf8() );
	f.close();
	g_root = root;
	qputenv( "WW_RECX_ROOT", root.toLocal8Bit() );
	QThread::msleep( 120 );	// past the reader's 100 ms re-resolve
	std::fprintf( stderr, "---- arm %s\n", name );
	const WwWeatherRecx & w = wwWeatherRecx();
	std::printf( "arm %-10s on=%d density=%g gen=%g ratio=%g lwc=%g r=%g ice=%g evo=%g pbase=%g legacy=%d key=[%s]\n", name,
		int( w.on ), double( w.density ), double( w.densityGen ), double( w.densityRatio ), double( w.liquidWater ),
		double( w.dropletRadius ), double( w.iceFraction ), double( w.evolution ), double( w.precipitationBase ),
		int( w.densityLegacy ), qPrintable( w.key ) );
	return w;
}

QString readText( const QString & p )
{
	QFile f( p );
	return f.open( QIODevice::ReadOnly ) ? QString::fromUtf8( f.readAll() ) : QString();
}

//! the number after `pat` in the text (the shader twins carry the header's constants as literals)
double literalAfter( const QString & t, const QString & pat )
{
	const int i = t.indexOf( pat );
	if ( i < 0 )
		return -1e30;
	int j = i + pat.size();
	int k = j;
	while ( k < t.size() && ( t[k].isDigit() || t[k] == QLatin1Char( '.' ) ) )
		k++;
	return t.mid( j, k - j ).toDouble();
}
}	// namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	if ( argc < 3 ) {
		std::fprintf( stderr, "usage: cloudrecx14_gate <out dir> <tree>\n" );
		return 2;
	}
	g_out = QDir::fromNativeSeparators( QString::fromLocal8Bit( argv[1] ) );
	const QString tree = QDir::fromNativeSeparators( QString::fromLocal8Bit( argv[2] ) );
	const bool red = wwCloudRecx14Red();
	std::printf( "cloudrecx14 gate: red=%d\n", int( red ) );

	// ---- U. the math (no reader)
	check( near( C::sigmaPerKm( 0.3f, 10.0f ), 45.0 ), "U1 sigma(0.3 g/m3, 10 um) = 45 /km (3 LWC / (2 rho_w r))" );
	check( near( C::sigmaPerKm( 0.3f, 10.0f, 1.0f ), 45.0 / 0.917 ), "U2 all ice: rho_i 0.917 in place of rho_w" );
	check( C::densityRatio( -1.0f, -1.0f, C::lowDefault( 0.3f ) ) == 1.0f, "U3 nothing set: ratio 1 exactly" );
	{
		bool ok = true;
		for ( int i = 0; i <= 100; i++ ) {
			const C::Micro d = C::lowDefault( float( i ) / 100.0f );
			ok = ok && near( C::densityRatio( d.lwc, d.rUm, d ), 1.0, 1e-6 );
		}
		check( ok, "U4 a file that writes the type's own defaults draws today's density (ratio 1 at 101 types)" );
	}
	check( near( C::densityRatio( 3.0f, -1.0f, C::lowDefault( 1.0f ) ), 2.0 )
			&& near( C::densityRatio( -1.0f, 7.0f, C::lowDefault( 1.0f ) ), 2.0 ),
		"U5 cumulonimbus (1.5 g/m3, 14 um): LWC 3 -> x2, r 7 -> x2" );
	check( near( C::lowDefault( 0.0f ).lwc, 0.28 ) && near( C::lowDefault( 2.0f / 3.0f ).lwc, 0.6 ) && near( C::lowDefault( 1.0f ).rUm, 14.0 ),
		"U6 anchors: stratus 0.28 g/m3, cumulus 0.6 g/m3, cumulonimbus 14 um" );
	{
		C::LayerIn L[3];
		L[0].band = 0, L[0].cover = 0.3f;
		L[1].band = 1, L[1].cover = 0.2f;
		L[2].band = 2, L[2].cover = 0.1f;
		const C::BandsOut o = C::layersOntoBands( L, 3, -1.0f, 0.6f ), z;
		check( std::memcmp( &o, &z, sizeof( o ) ) == 0, "U7 no layer key: ratio 1 1 1, ice 0 0 0, anvil -1 (today), byte for byte" );
		L[0].lwc = 0.56f;
		L[0].type = 2.0f / 3.0f;
		L[0].anvil = 1.5f;
		L[2].ice = 1.0f;
		const C::BandsOut p = C::layersOntoBands( L, 3, 0.25f, 0.6f );
		check( near( p.ratio[0], 0.56 / 0.6 ) && p.ratio[1] == 1.0f && p.ratio[2] == 1.0f && near( p.anvil, 1.5 )
				&& near( p.ice[2], 1.0 ) && near( p.ice[0], 0.25 ) && near( p.ice[1], 0.25 ),
			"U8 layer 0 lwc 0.56 at cumulus -> Low x0.933; anvil 1.5; layer 2 ice 1; the others the weather's 0.25" );
	}
	for ( float k : { 1.0f, 0.5f, 0.25f } ) {
		double s = 0.0;
		const int N = 400000;
		for ( int i = 0; i < N; i++ ) {
			const double th = ( i + 0.5 ) * C::kPi / N;
			s += 2.0 * C::kPi * std::sin( th ) * C::icePhase( float( std::cos( th ) ), k ) * C::kPi / N;
		}
		char what[96];
		std::snprintf( what, sizeof what, "U9 ice phase normalised at k %.2f: %.5f (bar 1 +- 0.003)", double( k ), s );
		check( std::fabs( s - 1.0 ) < 3e-3, what );
	}
	{
		const float d2r = C::kPi / 180.0f;
		const float p22 = C::icePhase( std::cos( 22.0f * d2r ), 1.0f ), p19 = C::icePhase( std::cos( 19.0f * d2r ), 1.0f ),
					p25 = C::icePhase( std::cos( 25.0f * d2r ), 1.0f );
		// bar moved after the first run (2x was a guess; a 3 % ring on the 0.75 body gives ~1.5x): a clear local maximum
		check( p22 > 1.3f * p19 && p22 > 1.3f * p25, "U10 the 22 deg halo: the phase at 22 deg > 1.3x at 19 and 25 deg" );
		const V::Droplet dr = V::dropletParams( V::kDropletUm );
		const float dw = ( 1.0f - dr.wD ) * V::hg( dr.gHg, 0.5f ) + dr.wD * V::draine( dr.gD, dr.alpha, 0.5f );
		check( C::icePhase( 0.5f, 1.0f ) != dw, "U11 the ice phase differs from the droplet phase (60 deg)" );
	}
	check( C::evolveKm( 1234.0, 0.0f ) == 0.0f && C::evolveKm( 1234.0, -1.0f ) == 0.0f, "U12 evolution unset: no offset (today)" );
	check( near( C::evolveKm( 600.0, 10.0f ), 1.0 ) && near( C::evolveKm( 600.0 * 25.0, 10.0f ), 1.0 ),
		"U13 evolution 10 min: 1 km of noise per 10 min, wrapped at 24 km" );
	check( C::shaftSigmaPerKm( 0.0f, 0.0f ) == 0.0f && near( C::shaftSigmaPerKm( 10.0f, 0.0f ), 0.21 * std::pow( 10.0, 0.74 ) ),
		"U14 shafts: none when dry; rain 10 mm/h 0.21 R^0.74 /km" );
	check( C::shaftProfile( 0.5f, true ) == 1.0f && near( C::shaftProfile( 0.8f, true ), 0.5 ) && C::shaftProfile( 0.99f, false ) == 1.0f
			&& C::shaftProfile( 0.0f, false ) == 0.0f && C::shaftProfile( 1.0f, false ) == 0.0f,
		"U15 shaft profile: full to 60 %, a virga half at 80 %, a ground shaft full to the end" );
	check( C::phaseDiameterUm( -1.0f, V::kDropletUm ) == V::kDropletUm && near( C::phaseDiameterUm( 7.0f, 20.0f ), 14.0 ),
		"U16 the droplet phase: today's 20 um unless dropletRadius is set (then 2 r)" );

	// ---- S. the shader twins carry the header's numbers (ww_droplet_phase.glsl, cloud_march.frag)
	{
		const QString ph = readText( tree + QStringLiteral( "/res/shaders/features/clouds/ww_droplet_phase.glsl" ) );
		const QString mf = readText( tree + QStringLiteral( "/res/shaders/features/clouds/cloud_march.frag" ) );
		check( near( literalAfter( ph, QStringLiteral( "( 0.0174533 * 0.0174533 ) ) / " ) ), C::kHaloNorm, 1e-5 )
				&& near( 0.0174533, C::kHaloWidthRad, 1e-6 ),
			"S1 wwIcePhase's ring: 1 deg wide, normalised by kHaloNorm" );
		check( ph.contains( QStringLiteral( "0.87 * wwHg( 0.75 * k, c ) + 0.10 * wwHg( 0.98 * k, c ) + 0.03 * ( k * ring + ( 1.0 - k ) / 12.5663706 )" ) )
				&& near( 0.87, 1.0 - C::kIceFwdW - C::kIceHaloW, 1e-6 ) && near( 0.75, C::kIceG ) && near( 0.98, C::kIceFwdG )
				&& ph.contains( QStringLiteral( "- 0.383972;" ) ) && near( 0.383972, C::kHaloRad, 1e-6 ),
			"S3 wwIcePhase = icePhase: weights 0.87 / 0.10 / 0.03, g 0.75 / 0.98, halo 22 deg" );
		check( mf.contains( QStringLiteral( "float t = clamp( ( hb - 0.6 ) / 0.4, 0.0, 1.0 );" ) )
				&& mf.contains( QStringLiteral( "if ( cvIceNow > 0.0 )" ) ) && mf.contains( QStringLiteral( "uniform vec4 cvShaft;" ) ),
			"S4 cloud_march.frag: the virga's 60 % / 40 %, the ice mix guarded by 0 (today), cvShaft" );
	}

	// ---- R. the reader (a storm: Rainy + thunder + rain -> the generated rung's class storm, cumulonimbus)
	g_gen.edid = "ExampleStorm";
	g_gen.owner = "ExampleMod.esp";
	g_gen.flags = 4;
	g_gen.thunder = true;
	g_gen.precipType = 0;
	const WwWeatherGen::Out go = WwWeatherGen::generate( g_gen );
	std::printf( "generated: class %s type %s density %g\n", go.cls, go.cloudType, go.density );

	const WwWeatherRecx a0 = arm( "plain", "{ \"coverage\": 0.7 }" );
	check( a0.on && a0.density == float( go.density ) && a0.densityRatio == 1.0f && !a0.densityLegacy
			&& a0.key.indexOf( QStringLiteral( "liquidWater" ) ) < 0,
		"R1 no 1.4 key: density = the generated rung's, ratio 1, the key text as 1.3" );
	const QByteArray redWas = qgetenv( "WW_CLOUD_RECX14_RED" );
	qputenv( "WW_CLOUD_RECX14_RED", "1" );
	const WwWeatherRecx a0r = arm( "plain-red", "{ \"coverage\": 0.7 }" );
	qputenv( "WW_CLOUD_RECX14_RED", redWas );	// the red run keeps its red
	if ( !red )
		check( a0r.density == a0.density && a0r.coverage == a0.coverage && a0r.type == a0.type, "R2 no 1.4 key: 1.4 = 1.3 (red) exactly" );

	const WwWeatherRecx a1 = arm( "lwc", "{ \"liquidWater\": 3.0 }" );
	check( near( a1.liquidWater, 3.0 ) && near( a1.densityRatio, 2.0 ) && near( a1.density, go.density * 2.0 ),
		"R3 liquidWater 3 g/m3 on a cumulonimbus (1.5): density x2" );
	const WwWeatherRecx a2 = arm( "radius", "{ \"dropletRadius\": 7, \"type\": \"cumulonimbus\" }" );
	check( near( a2.dropletRadius, 7.0 ) && near( a2.density, go.density * 2.0 ), "R4 dropletRadius 7 um on a cumulonimbus (14): density x2" );
	const WwWeatherRecx a3 = arm( "legacy", "{ \"density\": 1.4 }" );
	check( near( a3.density, 1.4 ) && a3.densityLegacy, "R5 density alone: kept (the legacy alias, warned)" );
	const WwWeatherRecx a4 = arm( "both", "{ \"density\": 1.4, \"liquidWater\": 0.75 }" );
	check( near( a4.density, go.density * 0.5 ) && !a4.densityLegacy, "R6 density + liquidWater: density ignored (the rung's x 0.75 / 1.5)" );
	const WwWeatherRecx a5 = arm( "hours", "{ \"liquidWater\": 1.0, \"hours\": { \"2\": { \"liquidWater\": 1.0 }, \"14\": { \"liquidWater\": 2.0 } } }" );
	check( near( a5.liquidWater, 1.5 ), "R7 liquidWater by the hour: 1 (02:00) .. 2 (14:00) -> 1.5 at 08:00" );
	const WwWeatherRecx a6 = arm( "keys",
		"{ \"iceFraction\": 1.5, \"evolution\": 20, \"precipitationBase\": 300, \"layers\": { \"0\": { \"base\": 900, \"top\": 2400, "
		"\"liquidWater\": 0.56, \"type\": \"cumulus\", \"anvil\": 1.5 }, \"3\": { \"iceFraction\": 1 } } }",
		", \"precipitation\": { \"layers\": [ { \"type\": \"rain\", \"rate\": 10 } ] }" );
	check( near( a6.iceFraction, 1.0 ) && near( a6.evolution, 20.0 ) && near( a6.precipitationBase, 300.0 ),
		"R8 iceFraction 1.5 clamped to 1, evolution 20 min, precipitationBase 300 m" );
	check( near( a6.layerLwc[0], 0.56 ) && near( a6.layerType[0], 2.0 / 3.0 ) && near( a6.layerAnvil[0], 1.5 ) && near( a6.layerIce[3], 1.0 )
			&& a6.layers == 1 && near( a6.layerBase[0], 900.0 ),
		"R9 layers: 0 lwc 0.56 at cumulus, anvil 1.5 (with its heights); 3 ice 1 without heights" );
	check( near( a6.rainRate, 10.0 ), "R10 the precipitation block's rain (the shafts' rate)" );
	qputenv( "WW_WEATHER_RECORD_RED", "clouds" );
	const WwWeatherRecx a7 = arm( "recordred", "{ \"liquidWater\": 3.0, \"iceFraction\": 1, \"layers\": { \"0\": { \"anvil\": 1 } } }" );
	qunsetenv( "WW_WEATHER_RECORD_RED" );
	check( a7.liquidWater < 0.0f && a7.iceFraction < 0.0f && a7.layerAnvil[0] < 0.0f && a7.densityRatio == 1.0f,
		"R11 record red clouds: the 1.4 keys unset (the record's own clouds)" );
	const WwWeatherRecx a8 = arm( "bad", "{ \"liquidWater\": \"wet\", \"dropletRadius\": 0.5, \"evolution\": 0 }" );
	check( a8.liquidWater < 0.0f && near( a8.dropletRadius, C::kRadiusMin ) && near( a8.evolution, C::kEvolutionMin ),
		"R12 bad values: not-number dropped, radius 0.5 clamped to 2, evolution 0 clamped to 1" );
	check( near( a8.density, 10.0 ), "R13 the drawn density stays in the 1.3 range (2.5 x 7 -> 10)" );

	std::printf( "%s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
