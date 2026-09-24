/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lookdevstage.h"

#include "esmweather.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "gl/scenelighting.h"
#include "model/nifmodel.h"

#include "gamemanager.h"
#include "harnesswindow.h"
#include "lodgen.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

const char * const kPluginKey = "Settings/Render/Scene/Lookdev Plugin";
const char * const kWeatherKey = "Settings/Render/Scene/Lookdev Weather";
const char * const kHourKey = "Settings/Render/Scene/Lookdev Hour";
const char * const kGroundKey = "Settings/Render/Scene/Lookdev Ground";
const char * const kSkyKey = "Settings/Render/Scene/Lookdev Sky";
const char * const kSunKey = "Settings/Render/Scene/Lookdev Sun";
const char * const kCloudsKey = "Settings/Render/Scene/Lookdev Clouds";
const char * const kMoonKey = "Settings/Render/Scene/Lookdev Moon";
const char * const kDayKey = "Settings/Render/Scene/Lookdev Game Day";

const char * const kDefaultCube = "textures/shared/cubemaps/mipblur_defaultoutside1.dds";
const char * const kGroundD = "textures/landscape/ground/commonwealthdefault01_d.dds";
const char * const kGroundN = "textures/landscape/ground/commonwealthdefault01_n.dds";
constexpr float kTile = 512.0f;		// research_lookdev_scene.md: UV tiled about every 512 units
constexpr float kGroundHalf = 4096.0f;	// an ~8192-unit quad

struct LdState
{
	bool init = false;
	QStringList pinPlugins;
	QString pinPlugin;
	bool pinWeather = false, pinHour = false, pinGround = false, pinCube = false;
	QString plugin;			// the chosen plugin (full path); empty = Data/Fallout4.esm
	QString weatherKey = QStringLiteral( "CommonwealthClear" );
	double hour = 12.0;		// ruling Q8: noon
	bool ground = true;		// the Ground row defaults ON in Lookdev
	QString cube;

	// resolved
	bool dirtyLoad = true, dirtyWeather = true;
	EsmWeather esm;
	QString loadRefusal;
	QVector<WwWeatherEntry> list;
	WwWeatherData w;
	bool haveWeather = false;
	QString weatherRefusal;
	unsigned char tnam[4] = { 30, 54, 102, 126 };
	QString climateEdid;

	// the weather preview (lane PBRWX1)
	bool sky = false, sun = false, clouds = false, moon = false;
	double gameDay = 0.0;
	bool pinSky = false, pinSun = false, pinClouds = false, pinMoon = false, pinDay = false;
	double pinCloudTime = -1.0;
	bool probe = false;
	int probeLayer = 0;
	float probeUV[2] = { 0.5f, 0.5f };
	WwSkyGmst gmst;
	WwClimateData clim;
	QElapsedTimer cloudClock;
	QString skyLast = QStringLiteral( "pending" );	// what the last sky pass drew, for the echo

	// ground height cache
	float bsKey[4] = { 0, 0, 0, -1 };
	float groundZ = 0.0f;
	float groundXY[2] = { 0, 0 };
	float groundHalf = kGroundHalf;
};

LdState & st()
{
	static LdState s;
	if ( !s.init ) {
		s.init = true;
		const QString pl = qEnvironmentVariable( "WW_LOOKDEV_PLUGINS" ).trimmed();
		if ( !pl.isEmpty() )
			s.pinPlugins = pl.split( QChar( ',' ), Qt::SkipEmptyParts );
		s.pinPlugin = qEnvironmentVariable( "WW_LOOKDEV_PLUGIN" ).trimmed();
		if ( !s.pinPlugin.isEmpty() )
			s.plugin = s.pinPlugin;
		const QString w = qEnvironmentVariable( "WW_LOOKDEV_WEATHER" ).trimmed();
		if ( !w.isEmpty() ) {
			s.weatherKey = w;
			s.pinWeather = true;
		}
		bool ok = false;
		const double h = qEnvironmentVariable( "WW_LOOKDEV_HOUR" ).toDouble( &ok );
		if ( ok ) {
			s.hour = std::clamp( h, 0.0, 23.99 );
			s.pinHour = true;
		}
		const QString g = qEnvironmentVariable( "WW_LOOKDEV_GROUND" ).trimmed();
		if ( !g.isEmpty() ) {
			s.ground = g != QLatin1StringView( "0" );
			s.pinGround = true;
		}
		s.cube = qEnvironmentVariable( "WW_LOOKDEV_CUBE" ).trimmed();
		s.pinCube = !s.cube.isEmpty();
		if ( s.cube.isEmpty() )
			s.cube = QString::fromLatin1( kDefaultCube );
		auto pinBool = []( const char * name, bool & v, bool & pin ) {
			const QString e = qEnvironmentVariable( name ).trimmed();
			if ( !e.isEmpty() ) {
				v = e != QLatin1StringView( "0" ) && e.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
				pin = true;
			}
		};
		pinBool( "WW_LOOKDEV_SKY", s.sky, s.pinSky );
		pinBool( "WW_LOOKDEV_SUN", s.sun, s.pinSun );
		pinBool( "WW_LOOKDEV_CLOUDS", s.clouds, s.pinClouds );
		pinBool( "WW_LOOKDEV_MOON", s.moon, s.pinMoon );
		const double d = qEnvironmentVariable( "WW_LOOKDEV_DAY" ).toDouble( &ok );
		if ( ok ) {
			s.gameDay = std::max( 0.0, d );
			s.pinDay = true;
		}
		const double ct = qEnvironmentVariable( "WW_LOOKDEV_CLOUDTIME" ).toDouble( &ok );
		if ( ok )
			s.pinCloudTime = std::max( 0.0, ct );
		const QStringList pr = qEnvironmentVariable( "WW_LOOKDEV_CLOUDPROBE" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		if ( pr.size() == 3 ) {
			s.probe = true;
			s.probeLayer = pr.at( 0 ).toInt();
			s.probeUV[0] = pr.at( 1 ).toFloat();
			s.probeUV[1] = pr.at( 2 ).toFloat();
		}
		s.cloudClock.start();
	}
	return s;
}

QString hex8( quint32 v )
{
	return QString( "%1" ).arg( v, 8, 16, QChar( '0' ) ).toUpper();
}

QStringList resolveList( const LdState & s )
{
	const QString data = EsmWeather::gameDataDir();
	if ( !s.pinPlugins.isEmpty() ) {
		QStringList files;
		for ( const QString & p0 : s.pinPlugins ) {
			QString p = p0.trimmed();
			if ( !QFileInfo::exists( p ) && !data.isEmpty() && QFileInfo::exists( QDir( data ).filePath( p ) ) )
				p = QDir( data ).filePath( p );
			files << p;
		}
		return files;
	}
	QString plugin = s.plugin;
	if ( plugin.isEmpty() && !data.isEmpty() )
		plugin = QDir( data ).filePath( QStringLiteral( "Fallout4.esm" ) );
	if ( plugin.isEmpty() )
		return {};
	return EsmWeather::loadListFor( plugin, data );
}

void resolve()
{
	LdState & s = st();
	if ( s.dirtyLoad ) {
		s.dirtyLoad = false;
		s.dirtyWeather = true;
		s.list.clear();
		const QStringList files = resolveList( s );
		if ( files.isEmpty() ) {
			s.loadRefusal = QStringLiteral( "no Fallout 4 Data folder (set the game path)" );
		} else if ( !s.esm.load( files, &s.loadRefusal ) ) {
			if ( s.loadRefusal.isEmpty() )
				s.loadRefusal = QStringLiteral( "the plugins did not load" );
		} else {
			s.loadRefusal.clear();
			s.list = s.esm.list();
			s.gmst = s.esm.gmst();
			QString cwhy;
			if ( !s.esm.climateData( 0, s.clim, &cwhy ) )
				s.clim = WwClimateData();
			QString why;
			if ( !s.esm.climate( 0, s.tnam, &s.climateEdid, &why ) ) {
				const unsigned char def[4] = { 30, 54, 102, 126 };
				std::copy( def, def + 4, s.tnam );
				s.climateEdid = QStringLiteral( "fallback" );
			}
		}
	}
	if ( s.dirtyWeather ) {
		s.dirtyWeather = false;
		s.haveWeather = false;
		s.weatherRefusal.clear();
		if ( !s.loadRefusal.isEmpty() ) {
			s.weatherRefusal = s.loadRefusal;
		} else {
			const quint32 id = s.esm.find( s.weatherKey );
			QString why;
			if ( !id )
				s.weatherRefusal = QString( "weather %1 not in the loaded plugins" ).arg( s.weatherKey );
			else if ( !s.esm.read( id, s.w, &why ) )
				s.weatherRefusal = why;
			else
				s.haveWeather = true;
		}
	}
}

float lin( float byte )
{
	return std::pow( std::clamp( byte / 255.0f, 0.0f, 1.0f ), 2.2f );
}

//! the neutral stage a refusal falls back to: noon tent sun, white, flat grey ambient
void neutral( float sun[3], float amb[3], float dalc[6][3] )
{
	for ( int c = 0; c < 3; c++ ) {
		sun[c] = 1.0f;
		amb[c] = 0.2f;
		for ( int a = 0; a < 6; a++ )
			dalc[a][c] = 0.2f;
	}
}

//! everything the passes need, for the current hour
struct LdLight
{
	WwTodKeys keys;
	float sunDir[3];	// TO the light, world
	float discPos[3];	// the tent-arc disc, world, not normalised
	float sun[3];		// linear
	float amb[3];		// linear (NAM0 Ambient)
	float dalc[6][3];	// linear
	bool fromWeather = false;
};

LdLight currentLight()
{
	resolve();
	LdState & s = st();
	LdLight L;
	if ( s.sun ) {
		// the engine clock (lane PBRWX1): arc + night branch + GMSTs, keys on fDaytimeColorExtension
		const WwSkyClock c = wwSkyClock( s.hour, s.tnam, s.gmst );
		std::copy( c.lightDir, c.lightDir + 3, L.sunDir );
		std::copy( c.sunPos, c.sunPos + 3, L.discPos );
		L.keys = c.keys;
	} else {
		wwVanillaSunLightDir( s.hour, s.tnam, L.sunDir );
		wwVanillaSunPos( s.hour, s.tnam, L.discPos );
		L.keys = wwTodKeys( s.hour, s.tnam );
	}
	if ( !s.haveWeather ) {
		neutral( L.sun, L.amb, L.dalc );
		return L;
	}
	L.fromWeather = true;
	float rgb[3];
	if ( s.sun )
		EsmWeather::blendRowLab( s.w, WwRowSunlight, L.keys, rgb );
	else
		EsmWeather::blendRow( s.w, WwRowSunlight, L.keys, rgb );
	for ( int c = 0; c < 3; c++ )
		L.sun[c] = lin( rgb[c] );
	if ( s.sun )
		EsmWeather::blendRowLab( s.w, WwRowAmbient, L.keys, rgb );
	else
		EsmWeather::blendRow( s.w, WwRowAmbient, L.keys, rgb );
	for ( int c = 0; c < 3; c++ )
		L.amb[c] = lin( rgb[c] );
	for ( int a = 0; a < 6; a++ ) {
		if ( s.w.hasDalc ) {
			EsmWeather::blendDalc( s.w, a, L.keys, rgb );
			for ( int c = 0; c < 3; c++ )
				L.dalc[a][c] = lin( rgb[c] );
		} else {
			for ( int c = 0; c < 3; c++ )
				L.dalc[a][c] = L.amb[c];	// NAM0 Ambient: the flat fallback
		}
	}
	return L;
}

QString cubeSource()
{
	const LdState & s = st();
	return QString( "%1(%2)" ).arg( s.cube, s.pinCube ? QStringLiteral( "pinned" ) : QStringLiteral( "default" ) );
}

} // namespace

bool wwLookdevActive()
{
	return wwSceneMode() == WwSceneLookdev;
}

QStringList wwLookdevAvailablePlugins()
{
	QStringList out;
	const QString data = EsmWeather::gameDataDir();
	if ( data.isEmpty() )
		return out;
	QDir d( data );
	QStringList masters, plugins;
	for ( const QFileInfo & fi : d.entryInfoList( { "*.esm", "*.esp", "*.esl" }, QDir::Files, QDir::Name | QDir::IgnoreCase ) ) {
		if ( fi.suffix().compare( QLatin1StringView( "esm" ), Qt::CaseInsensitive ) == 0 )
			masters << fi.absoluteFilePath();
		else
			plugins << fi.absoluteFilePath();
	}
	// Fallout4.esm first, the other masters, then the plugins
	std::stable_sort( masters.begin(), masters.end(), []( const QString & a, const QString & b ) {
		const bool fa = QFileInfo( a ).fileName().compare( QLatin1StringView( "Fallout4.esm" ), Qt::CaseInsensitive ) == 0;
		const bool fb = QFileInfo( b ).fileName().compare( QLatin1StringView( "Fallout4.esm" ), Qt::CaseInsensitive ) == 0;
		return fa && !fb;
	} );
	return masters + plugins;
}

QString wwLookdevPlugin()
{
	const LdState & s = st();
	if ( !s.plugin.isEmpty() )
		return s.plugin;
	const QString data = EsmWeather::gameDataDir();
	return data.isEmpty() ? QString() : QDir( data ).filePath( QStringLiteral( "Fallout4.esm" ) );
}

void wwLookdevSetPlugin( const QString & path )
{
	LdState & s = st();
	if ( QDir::cleanPath( path ) == QDir::cleanPath( wwLookdevPlugin() ) && s.pinPlugins.isEmpty() )
		return;
	s.plugin = path;
	s.pinPlugins.clear();
	s.dirtyLoad = true;
	if ( s.pinPlugin.isEmpty() && !wwR2aRed( "nosave" ) )
		QSettings().setValue( QLatin1StringView( kPluginKey ), path );
}

QVector<WwWeatherEntry> wwLookdevWeathers()
{
	resolve();
	return st().list;
}

QString wwLookdevWeatherKey()
{
	return st().weatherKey;
}

void wwLookdevSetWeather( const QString & key )
{
	LdState & s = st();
	if ( key == s.weatherKey )
		return;
	s.weatherKey = key;
	s.dirtyWeather = true;
	if ( !s.pinWeather && !wwR2aRed( "nosave" ) )
		QSettings().setValue( QLatin1StringView( kWeatherKey ), key );
}

double wwLookdevHour()
{
	return st().hour;
}

void wwLookdevSetHour( double h )
{
	LdState & s = st();
	s.hour = std::clamp( h, 0.0, 23.99 );
	if ( !s.pinHour && !wwR2aRed( "nosave" ) )
		QSettings().setValue( QLatin1StringView( kHourKey ), s.hour );
}

bool wwLookdevGround()
{
	return st().ground;
}

void wwLookdevSetGround( bool on )
{
	LdState & s = st();
	s.ground = on;
	if ( !s.pinGround && !wwR2aRed( "nosave" ) )
		QSettings().setValue( QLatin1StringView( kGroundKey ), on );
}

QString wwLookdevCubePath()
{
	return st().cube;
}

namespace
{
void setPart( bool & v, bool on, bool pin, const char * key )
{
	v = on;
	if ( !pin && !wwR2aRed( "nosave" ) )
		QSettings().setValue( QLatin1StringView( key ), on );
}
} // namespace

bool wwLookdevSky()
{
	return st().sky;
}

void wwLookdevSetSky( bool on )
{
	setPart( st().sky, on, st().pinSky, kSkyKey );
}

bool wwLookdevSun()
{
	return st().sun;
}

void wwLookdevSetSun( bool on )
{
	setPart( st().sun, on, st().pinSun, kSunKey );
}

bool wwLookdevClouds()
{
	return st().clouds;
}

void wwLookdevSetClouds( bool on )
{
	LdState & s = st();
	if ( on && !s.clouds )
		s.cloudClock.restart();
	setPart( s.clouds, on, s.pinClouds, kCloudsKey );
}

bool wwLookdevMoon()
{
	return st().moon;
}

void wwLookdevSetMoon( bool on )
{
	setPart( st().moon, on, st().pinMoon, kMoonKey );
}

double wwLookdevGameDay()
{
	return st().gameDay;
}

void wwLookdevSetGameDay( double d )
{
	LdState & s = st();
	s.gameDay = std::max( 0.0, d );
	if ( !s.pinDay && !wwR2aRed( "nosave" ) )
		QSettings().setValue( QLatin1StringView( kDayKey ), s.gameDay );
}

double wwLookdevCloudSeconds()
{
	LdState & s = st();
	if ( s.pinCloudTime >= 0.0 )
		return s.pinCloudTime;
	if ( wwHarnessRun() )
		return 0.0;
	return double( s.cloudClock.elapsed() ) / 1000.0;
}

void wwLookdevLoadSettings()
{
	LdState & s = st();
	QSettings settings;
	if ( s.pinPlugins.isEmpty() && s.pinPlugin.isEmpty() && settings.contains( QLatin1StringView( kPluginKey ) ) ) {
		const QString p = settings.value( QLatin1StringView( kPluginKey ) ).toString();
		if ( QFileInfo::exists( p ) ) {
			s.plugin = p;
			s.dirtyLoad = true;
		}
	}
	if ( !s.pinWeather && settings.contains( QLatin1StringView( kWeatherKey ) ) ) {
		s.weatherKey = settings.value( QLatin1StringView( kWeatherKey ) ).toString();
		s.dirtyWeather = true;
	}
	if ( !s.pinHour && settings.contains( QLatin1StringView( kHourKey ) ) )
		s.hour = std::clamp( settings.value( QLatin1StringView( kHourKey ) ).toDouble(), 0.0, 23.99 );
	if ( !s.pinGround && settings.contains( QLatin1StringView( kGroundKey ) ) )
		s.ground = settings.value( QLatin1StringView( kGroundKey ) ).toBool();
	auto part = [&settings]( bool pin, const char * key, bool & v ) {
		if ( !pin && settings.contains( QLatin1StringView( key ) ) )
			v = settings.value( QLatin1StringView( key ) ).toBool();
	};
	part( s.pinSky, kSkyKey, s.sky );
	part( s.pinSun, kSunKey, s.sun );
	part( s.pinClouds, kCloudsKey, s.clouds );
	part( s.pinMoon, kMoonKey, s.moon );
	if ( !s.pinDay && settings.contains( QLatin1StringView( kDayKey ) ) )
		s.gameDay = std::max( 0.0, settings.value( QLatin1StringView( kDayKey ) ).toDouble() );
}

QString wwLookdevSummary()
{
	resolve();
	const LdState & s = st();
	const LdLight L = currentLight();
	QString head;
	if ( s.haveWeather ) {
		head = QString( "weather=%1 [%2] src=%3" ).arg( s.w.edid, hex8( s.w.formID ), s.w.srcFile );
	} else {
		head = QString( "refused=\"%1\" neutral-sun" ).arg( s.weatherRefusal );
	}
	auto trip = []( const float * v ) {
		return QString( "%1,%2,%3" ).arg( double( v[0] ), 0, 'f', 3 ).arg( double( v[1] ), 0, 'f', 3 ).arg( double( v[2] ), 0, 'f', 3 );
	};
	return QString( "%1 hour=%2 keys=%3 sun=%4 dir=%5 ambient=%6 dalcZ-=%7 cube=%8 ground=%9" )
		.arg( head ).arg( s.hour, 0, 'f', 2 ).arg( L.keys.describe(), trip( L.sun ), trip( L.sunDir ) )
		.arg( s.haveWeather ? ( s.w.hasDalc ? QStringLiteral( "dalc" ) : QStringLiteral( "nam0" ) ) : QStringLiteral( "neutral" ) )
		.arg( trip( L.dalc[5] ), cubeSource(), s.ground ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		+ ( ( s.sky || s.sun || s.clouds || s.moon )
			? QString( " preview=sky:%1,sun:%2,clouds:%3,moon:%4 day=%5 cloudtime=%6 drew=%7" )
				.arg( s.sky ? 1 : 0 ).arg( s.sun ? 1 : 0 ).arg( s.clouds ? 1 : 0 ).arg( s.moon ? 1 : 0 )
				.arg( s.gameDay, 0, 'f', 2 ).arg( wwLookdevCloudSeconds(), 0, 'f', 2 ).arg( s.skyLast )
			: QString() );
}

QString wwLookdevEcho()
{
	const QString asked = qEnvironmentVariableIsSet( "WW_LOOKDEV" ) ? qEnvironmentVariable( "WW_LOOKDEV" ) : QStringLiteral( "unset" );
	if ( !wwLookdevActive() )
		return QString( "lookdev=off(asked=%1)" ).arg( asked );
	return QString( "lookdev=on(asked=%1) %2" ).arg( asked, wwLookdevSummary() );
}

void wwLookdevLight( float lightDirWorld[3], float diffuse[4], float ambient[4] )
{
	const LdLight L = currentLight();
	for ( int c = 0; c < 3; c++ ) {
		lightDirWorld[c] = L.sunDir[c];
		diffuse[c] = L.sun[c];
		// legacy shapes: A = sqrt(amb) * 0.375, so A^2 = amb * 0.140625 = the linear ambient
		ambient[c] = L.amb[c] / 0.140625f;
	}
}

bool wwLookdevDalc( float rgb[6][3] )
{
	const LdLight L = currentLight();
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			rgb[a][c] = L.dalc[a][c];
	return L.fromWeather;
}

/* ------------------------------------------------------------------------
 * The weather preview passes (lane PBRWX1): dome, moon, sun disc, clouds,
 * glare, in that order (INFERRED from the RenderDoc sequence, spec_clouds.md
 * s5). Every pass: view rotation only, depth off, out through studioOutput.
 * ---------------------------------------------------------------------- */
namespace
{

bool drawCube( Scene * scene, bool glow );

struct SkyFrame
{
	WwSkyClock c;
	float skyScale = 1.0f;
	float up[3] = {}, lo[3] = {}, hz[3] = {}, sun[3] = {}, glare[3] = {}, moonGlare[3] = {};	// linear
	float glareAlpha = 0.0f;
};

SkyFrame skyFrame()
{
	LdState & s = st();
	SkyFrame f;
	f.c = wwSkyClock( s.hour, s.tnam, s.gmst );
	if ( !s.haveWeather )
		return f;
	f.skyScale = wwLookdevRed( "skyscale" ) ? 1.0f : EsmWeather::blendSkyScale( s.w, f.c.keys );
	const bool gammaRed = wwLookdevRed( "skygamma" );
	auto row = [&]( int r, float out[3] ) {
		float rgb[3];
		EsmWeather::blendRowLab( s.w, r, f.c.keys, rgb );
		for ( int c = 0; c < 3; c++ )
			out[c] = gammaRed ? std::clamp( rgb[c] / 255.0f, 0.0f, 1.0f ) : lin( rgb[c] );
	};
	row( WwRowSkyUpper, f.up );
	row( WwRowSkyLower, f.lo );
	row( WwRowHorizon, f.hz );
	row( WwRowSun, f.sun );
	row( WwRowSunGlare, f.glare );
	row( WwRowMoonGlare, f.moonGlare );
	if ( wwLookdevRed( "skyswap" ) )
		for ( int c = 0; c < 3; c++ )
			std::swap( f.up[c], f.hz[c] );
	f.glareAlpha = f.c.sunAlpha * float( s.w.sunGlare ) / 255.0f;
	return f;
}

struct SkyMesh
{
	QVector<float> pos, col, uv;
	QVector<std::uint16_t> idx;
	quint32 nv = 0;
};

struct SkyMeshes
{
	const void * domeTriedWith = nullptr, * cloudsTriedWith = nullptr;
	bool domeTried = false, cloudsTried = false, domeOk = false, cloudsOk = false;
	SkyMesh dome;
	QVector<SkyMesh> clouds;
	QString domeWhy, cloudsWhy;
};

SkyMeshes & skyMeshes()
{
	static SkyMeshes m;
	return m;
}

//! a sky NIF through the resource manager (mod folders + archives), then the game manager
bool loadSkyNif( Scene * scene, const QString & path, NifModel & src, QString * why )
{
	QByteArray bytes;
	bool found = false;
	if ( scene->nifModel )
		found = scene->nifModel->getResourceFile( bytes, path, "meshes", ".nif" );
	if ( !found )
		found = Game::GameManager::get_file( bytes, Game::FALLOUT_4, path, "meshes", ".nif" );
	if ( !found ) {
		// the archive index only exists while Fallout 4 is enabled in the Resources settings; a loose
		// data folder is read directly: the game manager's list (empty while that status is off) and
		// the resource stack (WW_LODGEN_RESOURCES), loose folders first-wins
		const QStringList dirs = Game::GameManager::folders( Game::FALLOUT_4 ) + lodgenResourceSearchPaths();
		// the stack lists each entry's Meshes/Textures/... sub-folder, the path starts with "meshes/":
		// try the folder and its parent
		for ( const QString & f : dirs ) {
			if ( !QFileInfo( f ).isDir() )
				continue;
			for ( const QString & root : { f, QDir::cleanPath( f + QStringLiteral( "/.." ) ) } ) {
				QFile file( QDir( root ).filePath( path ) );
				if ( file.open( QIODevice::ReadOnly ) ) {
					bytes = file.readAll();
					found = !bytes.isEmpty();
					if ( found )
						break;
				}
			}
			if ( found )
				break;
		}
	}
	if ( !found ) {
		*why = QString( "%1 not found in the data folders or archives" ).arg( path );
		return false;
	}
	QBuffer dev( &bytes );
	const bool ok = dev.open( QIODevice::ReadOnly ) && src.load( dev, path.toLocal8Bit().constData() );
	src.resetState();
	if ( !ok ) {
		*why = QString( "%1 read (%2 bytes) but did not load" ).arg( path ).arg( bytes.size() );
		return false;
	}
	return true;
}

Transform skyWorldOf( const NifModel & src, const QModelIndex & iShape )
{
	Transform t( &src, iShape );
	QModelIndex p = src.getBlockIndex( src.getParent( src.getBlockNumber( iShape ) ) );
	for ( int hop = 0; hop < 64 && p.isValid() && src.blockInherits( p, "NiAVObject" ); hop++ ) {
		t = Transform( &src, p ) * t;
		p = src.getBlockIndex( src.getParent( src.getBlockNumber( p ) ) );
	}
	return t;
}

bool readSkyShape( const NifModel & src, const QModelIndex & iShape, SkyMesh & m )
{
	const quint32 nv = src.get<quint32>( iShape, "Num Vertices" );
	if ( !nv || nv > 65535 )
		return false;
	const BSVertexDesc desc = src.get<BSVertexDesc>( iShape, "Vertex Desc" );
	const quint16 flags = quint16( ( desc.Value() >> 44 ) & 0xFFFF );
	const bool fullPrec = ( flags & 0x400 ) != 0;
	const bool hasColors = ( flags & 0x20 ) != 0;
	const QModelIndex iVD = src.getIndex( iShape, "Vertex Data" );
	if ( !iVD.isValid() )
		return false;
	const Transform xf = skyWorldOf( src, iShape );
	for ( quint32 v = 0; v < nv; v++ ) {
		const QModelIndex row = src.index( int( v ), 0, iVD );
		Vector3 p = fullPrec ? src.get<Vector3>( row, "Vertex" ) : Vector3( src.get<HalfVector3>( row, "Vertex" ) );
		p = xf * p;
		m.pos << p[0] << p[1] << p[2];
		const Vector2 uv( src.get<HalfVector2>( row, "UV" ) );
		m.uv << uv[0] << uv[1];
		const Color4 c = hasColors ? Color4( src.get<ByteColor4>( row, "Vertex Colors" ) ) : Color4( 1, 1, 1, 1 );
		m.col << c[0] << c[1] << c[2] << c[3];
	}
	const QModelIndex iTris = src.getIndex( iShape, "Triangles" );
	if ( iTris.isValid() )
		for ( const Triangle & t : src.getArray<Triangle>( iTris ) )
			if ( t[0] < nv && t[1] < nv && t[2] < nv )
				m.idx << t[0] << t[1] << t[2];
	m.nv = nv;
	return !m.idx.isEmpty();
}

const SkyMesh * domeMesh( Scene * scene, QString * why )
{
	SkyMeshes & m = skyMeshes();
	if ( !m.domeOk && ( !m.domeTried || m.domeTriedWith != scene->nifModel ) ) {
		m.domeTried = true;
		m.domeTriedWith = scene->nifModel;
		m.dome = SkyMesh();
		NifModel src;
		QString w;
		if ( loadSkyNif( scene, QStringLiteral( "meshes/sky/atmosphere.nif" ), src, &w ) ) {
			for ( int b = 0; b < src.getBlockCount() && !m.domeOk; b++ ) {
				const QModelIndex i = src.getBlockIndex( b );
				if ( src.blockInherits( i, "BSTriShape" ) )
					m.domeOk = readSkyShape( src, i, m.dome );
			}
			if ( !m.domeOk )
				w = QStringLiteral( "meshes/sky/atmosphere.nif has no readable BSTriShape" );
		}
		m.domeWhy = w;
	}
	if ( why )
		*why = m.domeWhy;
	return m.domeOk ? &m.dome : nullptr;
}

//! Clouds.nif: root child k that is a shape = layer k (Clouds::Init), capped at 32
const QVector<SkyMesh> * cloudMeshes( Scene * scene, QString * why )
{
	SkyMeshes & m = skyMeshes();
	if ( !m.cloudsOk && ( !m.cloudsTried || m.cloudsTriedWith != scene->nifModel ) ) {
		m.cloudsTried = true;
		m.cloudsTriedWith = scene->nifModel;
		m.clouds.clear();
		NifModel src;
		QString w;
		if ( loadSkyNif( scene, QStringLiteral( "meshes/sky/clouds.nif" ), src, &w ) ) {
			const QModelIndex iRoot = src.getBlockIndex( 0 );
			for ( qint32 l : src.getLinkArray( iRoot, QStringLiteral( "Children" ) ) ) {
				const QModelIndex i = src.getBlockIndex( l );
				if ( !i.isValid() || !src.blockInherits( i, "BSTriShape" ) || m.clouds.size() >= 32 )
					continue;
				SkyMesh sm;
				readSkyShape( src, i, sm );
				m.clouds << sm;
			}
			m.cloudsOk = !m.clouds.isEmpty();
			if ( !m.cloudsOk )
				w = QStringLiteral( "meshes/sky/clouds.nif has no layer shapes under its root" );
		}
		m.cloudsWhy = w;
	}
	if ( why )
		*why = m.cloudsWhy;
	return m.cloudsOk ? &m.clouds : nullptr;
}

//! a record's texture path ("Sky\X.dds", relative to Textures) as the texture cache takes it
QString skyTexPath( QString t )
{
	t.replace( QChar( '\\' ), QChar( '/' ) );
	if ( !t.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
		t.prepend( QStringLiteral( "textures/" ) );
	return t;
}

void skyState( Renderer * r, bool blend, bool additive )
{
	glDisable( GL_FRAMEBUFFER_SRGB );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable( GL_CULL_FACE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	if ( blend ) {
		glEnable( GL_BLEND );
		r->fn->glBlendFuncSeparate( GL_SRC_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
	} else {
		glDisable( GL_BLEND );
	}
}

QString f3( float v )
{
	return QString::number( double( v ), 'f', 3 );
}

bool drawDome( Scene * scene, const SkyFrame & f, float mul, QStringList & drew )
{
	LdState & s = st();
	if ( !s.haveWeather ) {
		drew << QString( "sky:refused(%1)" ).arg( s.weatherRefusal );
		return false;
	}
	QString why;
	const SkyMesh * m = domeMesh( scene, &why );
	if ( !m ) {
		drew << QString( "sky:refused(%1)" ).arg( why );
		return false;
	}
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_dome.prog" );
	if ( !prog ) {
		drew << QStringLiteral( "sky:refused(lookdev_dome.prog did not link)" );
		return false;
	}
	const bool full = mul >= 1.0f;
	if ( full )
		glClear( GL_COLOR_BUFFER_BIT );	// below the dome's -0.5 deg rim: the clear colour, as in the engine
	skyState( r, !full, false );
	prog->uni3f( "skyH", f.hz[0], f.hz[1], f.hz[2] );
	prog->uni3f( "skyL", f.lo[0], f.lo[1], f.lo[2] );
	prog->uni3f( "skyU", f.up[0], f.up[1], f.up[2] );
	prog->uni1f( "skyScale", f.skyScale );
	prog->uni1f( "alphaMul", full ? 1.0f : mul );
	prog->uni1f( "sceneExposure", wwSceneExposureScale() );
	prog->uni1i( "viewTransform", wwSceneViewTransform() );
	const float * attrs[2] = { m->pos.constData(), m->col.constData() };
	r->drawShape( m->nv, 0x43, unsigned( m->idx.size() ), GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, m->idx.constData() );
	r->stopProgram();
	drew << QString( "sky:dome(scale=%1)" ).arg( f3( f.skyScale ) );
	return full;
}

//! one camera-facing quad at a world direction, half-size = tan(half-angle)
bool drawBill( Scene * scene, const float dirWorld[3], float half, const QString & tex, const float tint[3], float alpha,
	bool additive )
{
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_bill.prog" );
	if ( !prog )
		return false;
	r->fn->glActiveTexture( GL_TEXTURE0 );
	if ( scene->bindTexture( QStringView( tex ), true ) <= 0 ) {
		r->stopProgram();
		return false;
	}
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	float d[3] = { dirWorld[0], dirWorld[1], dirWorld[2] };
	const float l = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
	for ( float & c : d )
		c = l > 1e-9f ? c / l : 0.0f;
	skyState( r, true, additive );
	prog->uni1i( "Tex", 0 );
	prog->uni3f( "centerWorld", d[0], d[1], d[2] );
	prog->uni1f( "halfSize", half );
	prog->uni3f( "tint", tint[0], tint[1], tint[2] );
	prog->uni1f( "alpha", alpha );
	prog->uni1f( "sceneExposure", wwSceneExposureScale() );
	prog->uni1i( "viewTransform", wwSceneViewTransform() );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	return true;
}

void drawSunQuad( Scene * scene, const SkyFrame & f, bool glare, float mul, QStringList & drew )
{
	LdState & s = st();
	const char * what = glare ? "glare" : "sun";
	if ( !s.haveWeather ) {
		drew << QString( "%1:refused(%2)" ).arg( QString::fromLatin1( what ), s.weatherRefusal );
		return;
	}
	const float a0 = glare ? f.glareAlpha : f.c.sunAlpha;
	const bool full = mul >= 1.0f;
	if ( full && a0 <= 0.0f ) {
		drew << QString( "%1:hidden(alpha=0)" ).arg( QString::fromLatin1( what ) );
		return;
	}
	const QString rec = glare ? s.clim.glareTex : s.clim.sunTex;
	if ( rec.isEmpty() ) {
		drew << QString( "%1:refused(the climate %2 has no %3)" ).arg( QString::fromLatin1( what ), s.clim.edid,
			glare ? QStringLiteral( "GNAM" ) : QStringLiteral( "FNAM" ) );
		return;
	}
	const float * p = f.c.sunPos;
	const float len = std::max( 1e-6f, std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] ) );
	const float half = ( glare ? s.gmst.sunGlare : s.gmst.sunBase ) / len;
	float tint[3];
	for ( int c = 0; c < 3; c++ )
		tint[c] = glare ? f.glare[c] : f.sun[c] * f.skyScale;	// the glare takes no Sky Scale (type 1)
	const QString tex = skyTexPath( rec );
	if ( drawBill( scene, p, half, tex, tint, full ? a0 : mul, glare ) )
		drew << QString( "%1:%2(alpha=%3)" ).arg( QString::fromLatin1( what ), tex, f3( full ? a0 : mul ) );
	else
		drew << QString( "%1:refused(%2 did not resolve)" ).arg( QString::fromLatin1( what ), tex );
}

void drawMoon( Scene * scene, const SkyFrame & f, float mul, QStringList & drew )
{
	LdState & s = st();
	if ( !s.haveWeather ) {
		drew << QString( "moon:refused(%1)" ).arg( s.weatherRefusal );
		return;
	}
	const unsigned char moons = s.clim.tnam[5];
	const int phase = wwMoonPhase( s.gameDay, moons );
	const bool full = mul >= 1.0f;
	struct M { unsigned char bit; const char * name; float size, fs, fe; };
	const M list[2] = { { 0x80, "masser", s.gmst.masserSize, s.gmst.masserFadeStart, s.gmst.masserFadeEnd },
		{ 0x40, "secunda", s.gmst.secundaSize, s.gmst.secundaFadeStart, s.gmst.secundaFadeEnd } };
	bool any = false;
	for ( const M & m : list ) {
		if ( !( moons & m.bit ) )
			continue;
		any = true;
		const float alpha = wwMoonAlpha( f.c, s.gmst, m.fs, m.fe );
		const float shadowA = std::min( alpha, f.c.starsAlpha );
		const bool black = f.moonGlare[0] <= 0.0f && f.moonGlare[1] <= 0.0f && f.moonGlare[2] <= 0.0f;
		if ( full && ( black || ( alpha <= 0.0f && shadowA <= 0.0f ) ) ) {
			drew << QString( "moon:%1:hidden(alpha=%2%3)" ).arg( QString::fromLatin1( m.name ), f3( alpha ),
				black ? QStringLiteral( ",moonglare black" ) : QString() );
			continue;
		}
		const float half = m.size / std::max( 1e-6f, std::fabs( s.gmst.sunX ) );
		float shadowTint[3];
		for ( int c = 0; c < 3; c++ )
			shadowTint[c] = f.up[c] * f.skyScale;
		const QString sh = QStringLiteral( "textures/sky/moonshadow.dds" );
		const bool shOk = drawBill( scene, f.c.sunPos, half, sh, shadowTint, full ? shadowA : mul, false );
		const QString tex = QString( "textures/sky/%1_%2.dds" ).arg( QString::fromLatin1( m.name ),
			QString::fromLatin1( wwMoonPhaseSuffix( phase ) ) );
		if ( drawBill( scene, f.c.sunPos, half, tex, f.moonGlare, full ? alpha : mul, false ) )
			drew << QString( "moon:%1(alpha=%2,shadow=%3%4)" ).arg( tex, f3( full ? alpha : mul ), f3( full ? shadowA : mul ),
				shOk ? QString() : QStringLiteral( ",no moonshadow.dds" ) );
		else
			drew << QString( "moon:refused(%1 did not resolve)" ).arg( tex );
	}
	if ( !any )
		drew << QString( "moon:none(the climate's moons byte 0x%1 names neither moon)" ).arg( moons, 2, 16, QChar( '0' ) );
}

void drawClouds( Scene * scene, const SkyFrame & f, float mul, QStringList & drew )
{
	LdState & s = st();
	if ( !s.haveWeather ) {
		drew << QString( "clouds:refused(%1)" ).arg( s.weatherRefusal );
		return;
	}
	QString why;
	const QVector<SkyMesh> * ms = cloudMeshes( scene, &why );
	if ( !ms && !s.probe ) {
		drew << QString( "clouds:refused(%1)" ).arg( why );
		return;
	}
	Renderer * r = scene->renderer;
	const double secs = wwLookdevCloudSeconds();
	const bool full = mul >= 1.0f;
	QStringList drawn, skipped;
	auto layerUniforms = [&]( NifSkopeOpenGLContext::Program * prog, int i, float alpha ) {
		float rgb[3], a = 0.0f;
		EsmWeather::blendCloud( s.w, i, f.c.keys, rgb, &a );
		const int li = ( quint32( i ) >= s.w.cloudLayers ) ? 0 : i;
		const float sx = EsmWeather::cloudSpeed( s.w.cloudSpeedX[li], s.gmst );
		const float sy = EsmWeather::cloudSpeed( s.w.cloudSpeedY[li], s.gmst );
		prog->uni3f( "colLin", lin( rgb[0] ), lin( rgb[1] ), lin( rgb[2] ) );
		prog->uni1f( "alpha", alpha < 0.0f ? a : alpha );
		prog->uni2f( "uvOffset", wwCloudOffset( sx, secs ), wwCloudOffset( sy, secs ) );
		prog->uni1f( "skyScale", f.skyScale );
		prog->uni1f( "sceneExposure", wwSceneExposureScale() );
		prog->uni1i( "viewTransform", wwSceneViewTransform() );
		return a;
	};
	auto bindLayer = [&]( int i ) {
		if ( i < 0 || i >= 32 || s.w.cloudTex[i].isEmpty() )
			return false;
		r->fn->glActiveTexture( GL_TEXTURE0 );
		if ( scene->bindTexture( QStringView( skyTexPath( s.w.cloudTex[i] ) ), true ) <= 0 )
			return false;
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		return true;
	};

	if ( s.probe ) {
		// the texel probe: one full-screen quad samples layer probeLayer at probeUV + the scroll offset
		NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_clouds.prog" );
		if ( !prog || !bindLayer( s.probeLayer ) ) {
			if ( prog )
				r->stopProgram();
			drew << QString( "cloudprobe:refused(layer %1 texture %2)" ).arg( s.probeLayer ).arg( s.w.cloudTex[s.probeLayer & 31] );
			return;
		}
		skyState( r, false, false );
		prog->uni1i( "Tex", 0 );
		prog->uni1b( "probe", true );
		prog->uni2f( "probeUV", s.probeUV[0], s.probeUV[1] );
		const float a = layerUniforms( prog, s.probeLayer, -1.0f );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		const float * attrs = quad;
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
		drew << QString( "cloudprobe:layer=%1,alpha=%2,t=%3" ).arg( s.probeLayer ).arg( f3( a ) ).arg( secs, 0, 'f', 3 );
		return;
	}

	for ( int i = 0; i < ms->size() && i < 32; i++ ) {
		const SkyMesh & m = ms->at( i );
		if ( s.w.cloudDisabled & ( 1u << i ) )
			continue;
		float rgb[3], a = 0.0f;
		EsmWeather::blendCloud( s.w, i, f.c.keys, rgb, &a );
		if ( full && a <= 0.0f )
			continue;
		if ( m.idx.isEmpty() ) {
			skipped << QString( "%1(no mesh)" ).arg( i );
			continue;
		}
		NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_clouds.prog" );
		if ( !prog ) {
			drew << QStringLiteral( "clouds:refused(lookdev_clouds.prog did not link)" );
			return;
		}
		if ( !bindLayer( i ) ) {
			r->stopProgram();
			skipped << QString( "%1(%2 did not resolve)" ).arg( i ).arg( skyTexPath( s.w.cloudTex[i] ) );
			continue;
		}
		skyState( r, true, false );
		prog->uni1i( "Tex", 0 );
		prog->uni1b( "probe", false );
		layerUniforms( prog, i, full ? a : mul );
		const float * attrs[8] = { m.pos.constData(), m.col.constData(), nullptr, nullptr, nullptr, nullptr, nullptr,
			m.uv.constData() };
		r->drawShape( m.nv, 0x20000043ULL, unsigned( m.idx.size() ), GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs,
			m.idx.constData() );
		r->stopProgram();
		drawn << QString::number( i );
	}
	drew << QString( "clouds:drawn=%1%2 t=%3" ).arg( drawn.isEmpty() ? QStringLiteral( "none" ) : drawn.join( QChar( ',' ) ),
		skipped.isEmpty() ? QString() : QStringLiteral( " skipped=" ) + skipped.join( QChar( ',' ) ) )
		.arg( secs, 0, 'f', 3 );
}

} // namespace

bool wwLookdevDrawBackground( Scene * scene )
{
	if ( !scene || !scene->renderer || scene->selecting || scene->hasVisMode( Scene::VisSilhouette ) )
		return false;
	Renderer * r = scene->renderer;
	LdState & sx = st();
	const bool anyPart = sx.sky || sx.sun || sx.clouds || sx.moon
		|| wwLookdevRed( "skyleak" ) || wwLookdevRed( "sunleak" ) || wwLookdevRed( "cloudleak" ) || wwLookdevRed( "moonleak" );
	const bool perspective = r->globalUniforms->projectionMatrix[3][3] != 1.0f;
	if ( anyPart && perspective ) {
		resolve();
		QStringList drew;
		const SkyFrame f = skyFrame();
		// the dome replaces the cube; a dome that will not resolve falls back to the cube, by name
		bool dome = false;
		if ( sx.sky ) {
			dome = drawDome( scene, f, 1.0f, drew );
		}
		if ( !dome && !drawCube( scene, !sx.sun ) )
			return false;
		if ( !sx.sky && wwLookdevRed( "skyleak" ) )
			drawDome( scene, f, 0.02f, drew );
		const float moonMul = sx.moon ? 1.0f : ( wwLookdevRed( "moonleak" ) ? 0.02f : 0.0f );
		if ( moonMul > 0.0f )
			drawMoon( scene, f, moonMul, drew );
		const float sunMul = sx.sun ? 1.0f : ( wwLookdevRed( "sunleak" ) ? 0.02f : 0.0f );
		if ( sunMul > 0.0f )
			drawSunQuad( scene, f, false, sunMul, drew );
		const float cloudMul = sx.clouds ? 1.0f : ( wwLookdevRed( "cloudleak" ) ? 0.02f : 0.0f );
		if ( cloudMul > 0.0f )
			drawClouds( scene, f, cloudMul, drew );
		if ( sunMul > 0.0f )
			drawSunQuad( scene, f, true, sunMul, drew );
		sx.skyLast = drew.isEmpty() ? QStringLiteral( "nothing" ) : drew.join( QChar( '|' ) );
		glDisable( GL_BLEND );
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_TRUE );
		return true;
	}
	return drawCube( scene, true );
}

namespace
{
bool drawCube( Scene * scene, bool glow )
{
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_sky.prog" );
	if ( !prog )
		return false;
	const LdLight L = currentLight();
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_FRAMEBUFFER_SRGB );

	r->fn->glActiveTexture( GL_TEXTURE0 );
	bool hasCube = wwBindStudioCube( scene->textures, scene->nifModel, st().cube, false );
	if ( !hasCube )
		scene->bindCube( QStringLiteral( "#FF555555c" ), 1 );	// the renderer's complete grey cube
	prog->uni1i( "CubeMap", 0 );
	prog->uni1b( "hasCubeMap", hasCube );
	prog->uni1b( "invertZAxis", !scene->nifModel || scene->nifModel->getBSVersion() < 170 );

	// the disc in view space: viewMatrix columns are the world axes in view space
	const auto & gu = *r->globalUniforms;
	float d[3] = { L.discPos[0], L.discPos[1], L.discPos[2] };
	const float dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
	float v[3] = { 0, 0, 0 };
	for ( int c = 0; c < 3; c++ )
		for ( int k = 0; k < 3; k++ )
			v[k] += gu.viewMatrix[c][k] * ( dl > 0 ? d[c] / dl : 0.0f );
	prog->uni3f( "sunDiscView", v[0], v[1], v[2] );
	prog->uni1b( "sunDiscUp", glow && L.discPos[2] > 0.0f );
	prog->uni3f( "sunLinear", L.sun[0], L.sun[1], L.sun[2] );
	prog->uni1f( "sceneExposure", wwSceneExposureScale() );
	prog->uni1i( "viewTransform", wwSceneViewTransform() );

	glDisable( GL_BLEND );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable( GL_CULL_FACE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	glDepthMask( GL_TRUE );
	return true;
}
} // namespace

void wwLookdevDrawGround( Scene * scene )
{
	// harness reference: WW_LOOKDEV_GROUNDPASS=0 -> the ground pass never runs at
	// all (the "no ground" picture the Ground-off gate is judged against)
	static const bool noPass = qEnvironmentVariable( "WW_LOOKDEV_GROUNDPASS" ) == QLatin1StringView( "0" );
	if ( noPass )
		return;
	if ( !scene || !scene->renderer || scene->selecting || scene->hasVisMode( Scene::VisSilhouette ) )
		return;
	LdState & s = st();
	const bool leak = !s.ground && wwLookdevRed( "groundleak" );
	if ( !s.ground && !leak )
		return;

	// the lowest visible vertex, recomputed when the scene's bounds move
	const BoundSphere bs = scene->bounds();
	const float key[4] = { bs.center[0], bs.center[1], bs.center[2], bs.radius };
	if ( !std::equal( key, key + 4, s.bsKey ) ) {
		std::copy( key, key + 4, s.bsKey );
		float zmin = std::numeric_limits<float>::max();
		for ( Node * node : scene->nodes.list() ) {
			Shape * sh = dynamic_cast<Shape *>( node );
			if ( !sh || !sh->isVisible() )
				continue;
			const Transform & t = sh->worldTrans();
			for ( const Vector3 & p : std::as_const( sh->verts ) )
				zmin = std::min( zmin, ( t * p )[2] );
		}
		if ( zmin == std::numeric_limits<float>::max() )
			zmin = bs.center[2] - bs.radius;
		s.groundZ = zmin;
		s.groundXY[0] = bs.center[0];
		s.groundXY[1] = bs.center[1];
		s.groundHalf = std::max( kGroundHalf, bs.radius * 4.0f );
	}

	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_ground.prog" );
	if ( !prog )
		return;
	const LdLight L = currentLight();
	glDisable( GL_FRAMEBUFFER_SRGB );
	glDisable( GL_POLYGON_OFFSET_FILL );

	r->fn->glActiveTexture( GL_TEXTURE0 );
	const bool hasD = scene->bindTexture( QStringView( QString::fromLatin1( kGroundD ) ), true ) > 0;
	prog->uni1i( "BaseMap", 0 );
	r->fn->glActiveTexture( GL_TEXTURE1 );
	const bool hasN = scene->bindTexture( QStringView( QString::fromLatin1( kGroundN ) ), true ) > 0;
	prog->uni1i( "NormalMap", 1 );
	r->fn->glActiveTexture( GL_TEXTURE0 );
	prog->uni1b( "hasBaseMap", hasD );
	prog->uni1b( "hasNormalMap", hasN );
	prog->uni1f( "tileSize", kTile );
	prog->uni3f( "sunDirWorld", L.sunDir[0], L.sunDir[1], L.sunDir[2] );
	prog->uni3f( "sunLinear", L.sun[0], L.sun[1], L.sun[2] );
	// vec3[6] in the program: element by element
	for ( int a = 0; a < 6; a++ )
		prog->uni3f_l( prog->uniLocation( "dalc[%d]", a ), L.dalc[a][0], L.dalc[a][1], L.dalc[a][2] );
	prog->uni1b( "dalcFlip", wwLookdevRed( "dalcflip" ) );
	prog->uni1f( "groundLeak", leak ? 0.04f : -1.0f );
	prog->uni1f( "sceneExposure", wwSceneExposureScale() );
	prog->uni1i( "viewTransform", wwSceneViewTransform() );
	prog->uni4m( "modelViewMatrix", scene->view.toMatrix4() );

	if ( leak ) {
		glEnable( GL_BLEND );
		r->fn->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
		glDepthMask( GL_FALSE );
	} else {
		glDisable( GL_BLEND );
		glDepthMask( GL_TRUE );
	}
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_CULL_FACE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	const float x0 = s.groundXY[0] - s.groundHalf, x1 = s.groundXY[0] + s.groundHalf;
	const float y0 = s.groundXY[1] - s.groundHalf, y1 = s.groundXY[1] + s.groundHalf;
	const float z = s.groundZ;
	const float quad[12] = { x0, y0, z, x1, y0, z, x0, y1, z, x1, y1, z };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	glDisable( GL_BLEND );
	glDepthMask( GL_TRUE );
}
