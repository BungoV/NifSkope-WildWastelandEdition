#include "wxoverride.h"
#include "wwswitch.h"
#include "recx.h"
#include "gl/lookdevstage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstdio>

namespace {

const WwWxRow kRows[] = {
	{ "WW_WX_FOG_NEAR", "Fog near", "Fog", 1, 0.0f, 500000.0f, 100.0f, "units" },
	{ "WW_WX_FOG_FAR", "Fog far", "Fog", 1, 0.0f, 1000000.0f, 500.0f, "units" },
	{ "WW_WX_FOG_POWER", "Fog power", "Fog", 1, 0.0f, 10.0f, 0.05f, "" },
	{ "WW_WX_FOG_MAX", "Fog max", "Fog", 1, 0.0f, 1.0f, 0.01f, "" },
	{ "WW_WX_FOG_NEAR_HEIGHT_MID", "Fog height mid (near)", "Fog", 1, -50000.0f, 100000.0f, 100.0f, "units" },
	{ "WW_WX_FOG_NEAR_HEIGHT_RANGE", "Fog height falloff (near)", "Fog", 1, 1.0f, 100000.0f, 100.0f, "units" },
	{ "WW_WX_FOG_FAR_HEIGHT_MID", "Fog height mid (far)", "Fog", 1, -50000.0f, 100000.0f, 100.0f, "units" },
	{ "WW_WX_FOG_FAR_HEIGHT_RANGE", "Fog height falloff (far)", "Fog", 1, 1.0f, 100000.0f, 100.0f, "units" },
	{ "WW_WX_FOG_HIGH_DENSITY", "Fog high density scale", "Fog", 1, 0.0f, 10.0f, 0.05f, "" },
	{ "WW_WX_FOG_NEAR_COLOR", "Fog near colour (now)", "Fog colours", 3, 0.0f, 10.0f, 0.01f, "linear" },
	{ "WW_WX_FOG_FAR_COLOR", "Fog far colour (now)", "Fog colours", 3, 0.0f, 10.0f, 0.01f, "linear" },
	{ "WW_WX_FOG_NEAR_HIGH_COLOR", "Fog near high colour (now)", "Fog colours", 3, 0.0f, 10.0f, 0.01f, "linear" },
	{ "WW_WX_FOG_FAR_HIGH_COLOR", "Fog far high colour (now)", "Fog colours", 3, 0.0f, 10.0f, 0.01f, "linear" },
	{ "WW_WX_FOG_NEAR_SUNRISE", "Fog near colour, sunrise", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_NEAR_DAY", "Fog near colour, day", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_NEAR_SUNSET", "Fog near colour, sunset", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_NEAR_NIGHT", "Fog near colour, night", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_FAR_SUNRISE", "Fog far colour, sunrise", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_FAR_DAY", "Fog far colour, day", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_FAR_SUNSET", "Fog far colour, sunset", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_FOG_FAR_NIGHT", "Fog far colour, night", "Fog colours", 3, 0.0f, 255.0f, 1.0f, "sRGB" },
	{ "WW_WX_HAZE_VISIBILITY", "Haze visibility", "Haze", 1, 50.0f, 300000.0f, 100.0f, "m" },
	{ "WW_WX_HAZE_HEIGHT", "Haze height", "Haze", 1, 10.0f, 20000.0f, 50.0f, "m" },
	{ "WW_WX_AIR_DENSITY", "Air density", "Volumetric air", 1, 0.0f, 50.0f, 0.01f, "per km" },
	{ "WW_WX_AIR_HEIGHT_SCALE", "Air height falloff", "Volumetric air", 1, 0.0f, 20000.0f, 10.0f, "m (0 = even)" },
	{ "WW_WX_AIR_PHASE_G", "Air phase g", "Volumetric air", 1, -0.95f, 0.95f, 0.01f, "" },
	{ "WW_WX_AIR_TINT", "Air tint", "Volumetric air", 3, 0.0f, 4.0f, 0.01f, "rgb" },
	{ "WW_WX_SUN_COLOR", "Sun colour", "Light", 3, 0.0f, 100000.0f, 0.01f, "linear" },
	{ "WW_WX_AMBIENT_COLOR", "Ambient colour", "Light", 3, 0.0f, 100000.0f, 0.01f, "linear" },
	{ "WW_WX_CLOUD_COVERAGE", "Cloud coverage", "Clouds", 1, 0.0f, 1.0f, 0.01f, "" },
	{ "WW_WX_CLOUD_DENSITY", "Cloud density", "Clouds", 1, 0.0f, 10.0f, 0.01f, "" },
};
static_assert( sizeof( kRows ) / sizeof( kRows[0] ) == size_t( WwWx::Count ), "one row per WwWx" );

constexpr int kN = int( WwWx::Count );

struct Pin
{
	bool set = false;
	float v[3] = {};
};

struct State
{
	quint64 seen = ~0ull;
	unsigned long long stamp = 1;
	bool red = false;
	Pin pin[kN];
	bool haveWeather[kN] = {};
	float weather[kN][3] = {};
};

State & S()
{
	static State s;
	return s;
}

QString fmt( const float * v, int n )
{
	QStringList p;
	for ( int i = 0; i < n; i++ )
		p << QString::number( double( v[i] ), 'g', 6 );
	return p.join( QLatin1Char( ',' ) );
}

void refresh()
{
	State & s = S();
	const quint64 g = wwSwitchGen();
	if ( g == s.seen )
		return;
	s.seen = g;
	bool moved = false;
	const bool red = wwEnvStr( "WW_WX_RED" ).trimmed() == QLatin1String( "1" );
	if ( red != s.red ) {
		s.red = red;
		moved = true;
		std::fprintf( stderr, "weather override: red %s (%s)\n", red ? "on" : "off",
			red ? "every override ignored, the weather's own values" : "overrides apply" );
	}
	for ( int i = 0; i < kN; i++ ) {
		const WwWxRow & r = kRows[i];
		const QString raw = wwEnvStr( r.sw ).trimmed();
		Pin p;
		if ( !raw.isEmpty() ) {
			const QStringList parts = raw.split( QLatin1Char( ',' ) );
			bool ok = parts.size() == r.n || ( r.n == 3 && parts.size() == 1 );
			for ( int c = 0; ok && c < r.n; c++ ) {
				bool okc = false;
				const float v = parts.value( parts.size() == 1 ? 0 : c ).trimmed().toFloat( &okc );
				ok = okc && std::isfinite( v );
				p.v[c] = std::min( std::max( v, r.min ), r.max );
			}
			p.set = ok;
			if ( !ok )
				std::fprintf( stderr, "weather override: %s = '%s' refused (want %d number%s)\n", r.sw, qPrintable( raw ),
					r.n, r.n == 1 ? "" : "s" );
		}
		Pin & o = s.pin[i];
		const bool same = o.set == p.set && ( !p.set || ( o.v[0] == p.v[0] && o.v[1] == p.v[1] && o.v[2] == p.v[2] ) );
		if ( same )
			continue;
		o = p;
		moved = true;
		std::fprintf( stderr, "weather override: %s = %s (weather %s)\n", r.sw,
			p.set ? qPrintable( fmt( p.v, r.n ) ) : "unset",
			s.haveWeather[i] ? qPrintable( fmt( s.weather[i], r.n ) ) : "not read yet" );
	}
	if ( moved )
		s.stamp++;
}

}	// namespace

const WwWxRow & wwWxRow( WwWx id )
{
	return kRows[int( id )];
}

int wwWxCount()
{
	return kN;
}

unsigned long long wwWxOvStamp()
{
	refresh();
	return S().stamp;
}

bool wwWxSet( WwWx id )
{
	refresh();
	return !S().red && S().pin[int( id )].set;
}

float wwWxOv( WwWx id, float weather )
{
	refresh();
	State & s = S();
	const int i = int( id );
	s.weather[i][0] = weather;
	s.haveWeather[i] = true;
	return ( !s.red && s.pin[i].set ) ? s.pin[i].v[0] : weather;
}

void wwWxOv3( WwWx id, float rgb[3] )
{
	refresh();
	State & s = S();
	const int i = int( id );
	for ( int c = 0; c < 3; c++ )
		s.weather[i][c] = rgb[c];
	s.haveWeather[i] = true;
	if ( !s.red && s.pin[i].set )
		for ( int c = 0; c < 3; c++ )
			rgb[c] = s.pin[i].v[c];
}

bool wwWxWeatherValue( WwWx id, float out[3] )
{
	const State & s = S();
	const int i = int( id );
	for ( int c = 0; c < 3; c++ )
		out[c] = s.weather[i][c];
	return s.haveWeather[i];
}

bool wwWxPinned( WwWx id, float out[3] )
{
	refresh();
	const State & s = S();
	const int i = int( id );
	for ( int c = 0; c < 3; c++ )
		out[c] = s.pin[i].v[c];
	return s.pin[i].set;
}

int wwWxFind( const QByteArray & sw )
{
	for ( int i = 0; i < kN; i++ )
		if ( sw == kRows[i].sw )
			return i;
	return -1;
}

QString wwWxWeatherText( WwWx id )
{
	const State & s = S();
	const int i = int( id );
	return s.haveWeather[i] ? fmt( s.weather[i], kRows[i].n ) : QString();
}

bool wwWxAnyTodColor()
{
	for ( int i = int( WwWx::FogNearSunrise ); i <= int( WwWx::FogFarNight ); i++ )
		if ( wwWxSet( WwWx( i ) ) )
			return true;
	return false;
}

QString wwWxSaveRecx()
{
	refresh();
	const State & s = S();
	quint32 formID = 0;
	QString edid, owner, dataDir;
	QStringList files;
	if ( !wwLookdevWeatherId( formID, edid, owner, files, dataDir ) )
		return QStringLiteral( "refused (no weather open)" );
	const QString root = wwAssetsModRoot();
	if ( root.isEmpty() )
		return QStringLiteral( "refused (Fusion Shaders - Assets not found)" );
	// outside Weathers/: a saved copy never competes with the weather's own file in the .recx scan
	QDir dir( root + QStringLiteral( "/Saved Weathers" ) );
	if ( !dir.exists() && !QDir().mkpath( dir.absolutePath() ) )
		return QStringLiteral( "refused (cannot make %1)" ).arg( dir.absolutePath() );
	const QString base = ( edid.isEmpty() ? QStringLiteral( "weather" ) : edid ) + QLatin1Char( '_' )
		+ QDateTime::currentDateTime().toString( QStringLiteral( "yyyyMMdd_HHmmss" ) );
	QString path = dir.filePath( base + QStringLiteral( ".recx" ) );
	for ( int k = 2; QFile::exists( path ); k++ )
		path = dir.filePath( base + QStringLiteral( "_%1.recx" ).arg( k ) );

	auto val = [&]( WwWx id, int c ) -> double {
		const int i = int( id );
		return double( s.pin[i].set ? s.pin[i].v[c] : s.weather[i][c] );
	};
	auto has = [&]( WwWx id ) { return s.pin[int( id )].set || s.haveWeather[int( id )]; };
	QJsonObject e;
	e.insert( QStringLiteral( "weather" ),
		owner + QStringLiteral( "|0x" ) + QStringLiteral( "%1" ).arg( formID & 0xFFFFFFu, 8, 16, QLatin1Char( '0' ) ).toUpper() );
	e.insert( QStringLiteral( "_editorId" ), edid );
	e.insert( QStringLiteral( "_derivedFrom" ), QStringLiteral( "saved from the NifSkope live weather overrides" ) );
	QJsonObject atmo, clouds, ov;
	if ( has( WwWx::HazeVisibility ) )
		atmo.insert( QStringLiteral( "visibility" ), val( WwWx::HazeVisibility, 0 ) );
	if ( has( WwWx::HazeHeight ) )
		atmo.insert( QStringLiteral( "hazeHeight" ), val( WwWx::HazeHeight, 0 ) );
	if ( has( WwWx::CloudCoverage ) )
		clouds.insert( QStringLiteral( "coverage" ), val( WwWx::CloudCoverage, 0 ) );
	if ( has( WwWx::CloudDensity ) )
		clouds.insert( QStringLiteral( "density" ), val( WwWx::CloudDensity, 0 ) );
	for ( int i = 0; i < kN; i++ ) {
		if ( !s.pin[i].set )
			continue;
		if ( kRows[i].n == 1 ) {
			ov.insert( QString::fromLatin1( kRows[i].sw ), double( s.pin[i].v[0] ) );
		} else {
			QJsonArray a;
			for ( int c = 0; c < 3; c++ )
				a.append( double( s.pin[i].v[c] ) );
			ov.insert( QString::fromLatin1( kRows[i].sw ), a );
		}
	}
	if ( !atmo.isEmpty() )
		e.insert( QStringLiteral( "atmosphere" ), atmo );
	if ( !clouds.isEmpty() )
		e.insert( QStringLiteral( "clouds" ), clouds );
	e.insert( QStringLiteral( "_overrides" ), ov );
	QJsonObject top;
	top.insert( QStringLiteral( "_" ), QStringLiteral( "Fusion Shaders weather file: a saved copy of the live overrides. "
		"Move it into Weathers/<plugin>/ to use it." ) );
	top.insert( QStringLiteral( "entries" ), QJsonArray{ e } );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::NewOnly ) )
		return QStringLiteral( "refused (cannot write %1)" ).arg( path );
	f.write( QJsonDocument( top ).toJson( QJsonDocument::Indented ) );
	f.close();
	std::fprintf( stderr, "weather override: saved %s (%d pinned)\n", qPrintable( path ), int( ov.size() ) );
	return path;
}
