/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lookdevstage.h"
#include "gl/cellpasstime.h"	// lane PERF2: the draws' CPU setup time
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "esmweather.h"
#include "gl/cellvolair.h"	// lane VOLAIR1
#include "gl/celllights.h"
#include "gl/cellhdr.h"	// lane LIGHTBOOTH1
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "gl/scenelighting.h"
#include "gl/shadowset.h"	// lane TERRSHADOW1: the Sun Path row
#include "gl/atmosphere.h"	// lane ATMO1: the physical sky
#include "gl/cloudmap.h"	// lane CLOUD1: the cloud map
#include "gl/cloudvol.h"	// lane CLOUD2: the volume clouds
#include "gl/atmomath.h"	// lane SKYA (ATMOCHECK1): the weather tint table
#include "gl/skyephem.h"	// lane EPHEM1: the real moon's phase texture
#include "gl/skydisc.h"	// lane SKYDISC1: our moon globe's math
#include "gl/celllooks.h"	// lane SKYDISC1: the real-sky look decides the moon's frame
#include "gl/sunshadow.h"
#include "model/nifmodel.h"

#include "gamemanager.h"
#include "harnesswindow.h"
#include "lodgen.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>
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
const char * const kFogKey = "Settings/Render/Scene/Lookdev Fog";
const char * const kShadowsKey = "Settings/Render/Scene/Lookdev Shadows";

const char * const kDefaultCube = "textures/shared/cubemaps/mipblur_defaultoutside1.dds";
const char * const kGroundD = "textures/landscape/ground/commonwealthdefault01_d.dds";
const char * const kGroundN = "textures/landscape/ground/commonwealthdefault01_n.dds";
constexpr float kTile = 512.0f;		// research_lookdev_scene.md: UV tiled about every 512 units
constexpr float kGroundHalf = 4096.0f;	// an ~8192-unit quad

struct LdState
{
	bool init = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	QStringList pinPlugins;
	QString livePins;               //!< lane LIVESET4: WW_LOOKDEV_PLUGINS as last read (empty = not set)
	QString pinPlugin;
	bool pinWeather = false, pinHour = false, pinGround = false, pinCube = false;
	QString plugin;			// the chosen plugin (full path); empty = Data/Fallout4.esm
	QString weatherKey = QStringLiteral( "CommonwealthClear" );
	double hour = 12.0;		// ruling Q8: noon
	bool ground = true;		// the Ground row defaults ON in Lookdev
	QString cube;

	// resolved
	bool dirtyLoad = true, dirtyWeather = true;
	quint64 gen = 1;	// bumped whenever resolve() reloads (lane PERF1: the per-shape memos below key on it)
	EsmWeather esm;
	QString loadRefusal;
	QVector<WwWeatherEntry> list;
	WwWeatherData w;
	bool haveWeather = false;
	QString weatherRefusal;
	unsigned char tnam[4] = { 30, 54, 102, 126 };
	QString climateEdid;

	// the weather preview (lane PBRWX1)
	// lane SKYFULL1: all four ON by default (bungo 10-03: the real sky, never the low-res cube); the cube
	// stays the reflection source and the fallback when the dome refuses (named in the echo)
	bool sky = true, sun = true, clouds = true, moon = true;
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

	// the weather fog (lane FOG1)
	bool fog = false, pinFog = false;
	float fogProbe[3] = { 0, 0, 0 };	// WW_LOOKDEV_FOGPROBE=d,z,mode
	QString fogLast = QStringLiteral( "pending" );	// what the last fogged draw uploaded, for the echo

	// the cascaded sun shadows (lane CSM1, gl/sunshadow.cpp)
	bool shadows = false, pinShadows = false;

	// ground height cache
	float bsKey[4] = { 0, 0, 0, -1 };
	float groundZ = 0.0f;
	float groundXY[2] = { 0, 0 };
	float groundHalf = kGroundHalf;
};

LdState & st()
{
	static LdState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_LOOKDEV_" } ) ) {
		const bool again = s.init;
		const QString oldPlugin = s.plugin;
		s.init = true;
		s.liveGen = wwSwitchGen();
		{
			const LdState d {};
			s.pinWeather = d.pinWeather;
			s.pinHour = d.pinHour;
			s.pinGround = d.pinGround;
			s.pinCube = d.pinCube;
			s.weatherKey = d.weatherKey;
			s.hour = d.hour;
			s.ground = d.ground;
			s.sky = d.sky;
			s.sun = d.sun;
			s.clouds = d.clouds;
			s.moon = d.moon;
			s.gameDay = d.gameDay;
			s.pinSky = d.pinSky;
			s.pinSun = d.pinSun;
			s.pinClouds = d.pinClouds;
			s.pinMoon = d.pinMoon;
			s.pinDay = d.pinDay;
			s.pinCloudTime = d.pinCloudTime;
			s.probe = d.probe;
			s.probeLayer = d.probeLayer;
			s.fog = d.fog;
			s.pinFog = d.pinFog;
			s.shadows = d.shadows;
			s.pinShadows = d.pinShadows;
		}
		if ( again && !s.pinPlugin.isEmpty() )
			s.plugin.clear();
		for ( float & f : s.fogProbe )
			f = 0.0f;
		// lane LIVESET4: a live set (or unset) of the list loads the plugins again, as a launch with it would
		const QString pl = wwEnvStr( "WW_LOOKDEV_PLUGINS" ).trimmed();
		const QStringList oldPins = s.pinPlugins;
		if ( !pl.isEmpty() )
			s.pinPlugins = pl.split( QChar( ',' ), Qt::SkipEmptyParts );
		else if ( again && !s.livePins.isEmpty() )
			s.pinPlugins.clear();
		s.livePins = pl;
		s.pinPlugin = wwEnvStr( "WW_LOOKDEV_PLUGIN" ).trimmed();
		if ( !s.pinPlugin.isEmpty() )
			s.plugin = s.pinPlugin;
		const QString w = wwEnvStr( "WW_LOOKDEV_WEATHER" ).trimmed();
		if ( !w.isEmpty() ) {
			s.weatherKey = w;
			s.pinWeather = true;
		}
		bool ok = false;
		const double h = wwEnvStr( "WW_LOOKDEV_HOUR" ).toDouble( &ok );
		if ( ok ) {
			s.hour = std::clamp( h, 0.0, 23.99 );
			s.pinHour = true;
		}
		const QString g = wwEnvStr( "WW_LOOKDEV_GROUND" ).trimmed();
		if ( !g.isEmpty() ) {
			s.ground = g != QLatin1StringView( "0" );
			s.pinGround = true;
		}
		s.cube = wwEnvStr( "WW_LOOKDEV_CUBE" ).trimmed();
		s.pinCube = !s.cube.isEmpty();
		if ( s.cube.isEmpty() )
			s.cube = QString::fromLatin1( kDefaultCube );
		auto pinBool = []( const char * name, bool & v, bool & pin ) {
			const QString e = wwEnvStr( name ).trimmed();
			if ( !e.isEmpty() ) {
				v = e != QLatin1StringView( "0" ) && e.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
				pin = true;
			}
		};
		pinBool( "WW_LOOKDEV_SKY", s.sky, s.pinSky );
		pinBool( "WW_LOOKDEV_SUN", s.sun, s.pinSun );
		pinBool( "WW_LOOKDEV_CLOUDS", s.clouds, s.pinClouds );
		pinBool( "WW_LOOKDEV_MOON", s.moon, s.pinMoon );
		pinBool( "WW_LOOKDEV_FOG", s.fog, s.pinFog );
		pinBool( "WW_LOOKDEV_SHADOWS", s.shadows, s.pinShadows );
		const QStringList fp = wwEnvStr( "WW_LOOKDEV_FOGPROBE" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		if ( fp.size() == 3 )
			for ( int i = 0; i < 3; i++ )
				s.fogProbe[i] = fp.at( i ).toFloat();
		const double d = wwEnvStr( "WW_LOOKDEV_DAY" ).toDouble( &ok );
		if ( ok ) {
			s.gameDay = std::max( 0.0, d );
			s.pinDay = true;
		}
		const double ct = wwEnvStr( "WW_LOOKDEV_CLOUDTIME" ).toDouble( &ok );
		if ( ok )
			s.pinCloudTime = std::max( 0.0, ct );
		const QStringList pr = wwEnvStr( "WW_LOOKDEV_CLOUDPROBE" ).split( QChar( ',' ), Qt::SkipEmptyParts );
		if ( pr.size() == 3 ) {
			s.probe = true;
			s.probeLayer = pr.at( 0 ).toInt();
			s.probeUV[0] = pr.at( 1 ).toFloat();
			s.probeUV[1] = pr.at( 2 ).toFloat();
		}
		if ( !again )
			s.cloudClock.start();
		else {
			s.dirtyWeather = true;
			if ( s.plugin != oldPlugin || s.pinPlugins != oldPins )
				s.dirtyLoad = true;
		}
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
	if ( s.dirtyLoad || s.dirtyWeather )
		s.gen++;
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
	float effect[3];	// linear (NAM0 Effect Lighting, lane EFFECTPARITY1)
	float amb[3];		// linear (NAM0 Ambient)
	float dalc[6][3];	// linear
	bool fromWeather = false;
};

LdLight computeLight();
WwFog currentFog();

/* lane SKYDISC1: our moon (Physical Sky on): its place, the sun direction that lights it (the phase) and its axes.
 * Real sky (EPHEM1): the ephemeris moon, sun, spin pole and libration. Game clock: the engine's moon point, the sun
 * 45 deg x the phase index round the celestial equator from it, no libration. Red nophase: the sun behind the eye. */
struct MoonGeo
{
	double D[3] = { 0.0, 0.0, 1.0 }, S[3] = { 0.0, 0.0, -1.0 }, P[3] = { 0.0, 0.0, 1.0 };
	double lDeg = 0.0, alphaDeg = 0.0;
	bool real = false;
};

MoonGeo moonGeo( const LdState & s, const float gamePos[3] )
{
	MoonGeo g;
	if ( wwSkyMoonFrame( s.hour, s.gameDay, g.D, g.S, g.P, &g.lDeg ) ) {
		g.real = true;
	} else {
		const double len = std::sqrt( double( gamePos[0] ) * gamePos[0] + double( gamePos[1] ) * gamePos[1] + double( gamePos[2] ) * gamePos[2] );
		for ( int k = 0; k < 3; k++ )
			g.D[k] = len > 1e-9 ? gamePos[k] / len : ( k == 2 ? 1.0 : 0.0 );
		g.P[0] = 0.0;
		g.P[1] = std::cos( wwephem::kBostonLat * wwskydisc::kRad );
		g.P[2] = std::sin( wwephem::kBostonLat * wwskydisc::kRad );
		const int p = std::max( 0, wwMoonPhase( s.gameDay, s.clim.tnam[5] ) );
		wwskydisc::sunFromElongation( g.D, g.P, 180.0 + 45.0 * p, g.S );
	}
	if ( wwAtmoDiscRed( "nophase" ) )
		for ( int k = 0; k < 3; k++ )
			g.S[k] = -g.D[k];
	g.alphaDeg = wwskydisc::phaseAngleDeg( g.D, g.S );
	return g;
}

//! the phase angle the moonlight follows (the drawn globe's), memoised: the light asks once per shape
float moonAlphaNow( const LdState & s )
{
	static double hour = -1.0, day = -1.0;
	static int moons = -1, look = -1, red = -1;
	static WwSunPathKey path;
	static float memo = 0.0f;
	const WwSunPathKey pk = wwSunPathKey();
	const int lk = wwLookOn( WwLook::Ephemeris ) ? 1 : 0, rd = wwAtmoDiscRed( "nophase" ) ? 1 : 0;
	if ( hour != s.hour || day != s.gameDay || moons != s.clim.tnam[5] || look != lk || red != rd || !( path == pk ) ) {
		const float up[3] = { 0.0f, 0.0f, 1.0f };
		memo = float( moonGeo( s, up ).alphaDeg );
		hour = s.hour;
		day = s.gameDay;
		moons = s.clim.tnam[5];
		look = lk;
		red = rd;
		path = pk;
	}
	return memo;
}

//! the moon's light against the full moon's: our globe's phase angle with the Physical Sky on, else the engine's phase
float moonLightFactor( const LdState & s )
{
	return wwAtmoDiscs() ? wwAtmoMoonFactorDeg( moonAlphaNow( s ) ) : wwAtmoMoonFactor( wwMoonPhase( s.gameDay, s.clim.tnam[5] ) );
}

/* lane PERF1: every shape's program setup asks for the light several times a frame; it is a pure function of
 * the resolved weather (gen), the hour and the Sun row, so it is computed once per change of those */
LdLight currentLight()
{
	resolve();
	const LdState & s = st();
	static quint64 gen = 0;
	static double hour = -1.0;
	static bool sun = false;
	static int atmoSun = 0;	// lane SKYA (ATMOSUN1): the Sunlight From Sky row, its red and the Physical Sky tier
	static WwSunPathKey sunPath;	// lane GAPTOOL2: a live WW_SUNPATH set (or the Sun Path row) moves the light
	static LdLight memo;
	// lane SKYA (NIGHT1): the Moon Phase and Stars row with the moon's phase (the game day moves it)
	int atmoKey = wwAtmoSunKey() + 3 * 16 * wwAtmoNightKey( wwMoonPhase( s.gameDay, s.clim.tnam[5] ) );
	if ( wwAtmoDiscs() && wwAtmoNightOn() )	// lane SKYDISC1: the light follows our globe's phase angle (0.1 deg steps)
		atmoKey += 4096 * ( 1 + int( moonAlphaNow( s ) * 10.0f ) );
	const WwSunPathKey sunKey = wwSunPathKey();
	if ( gen != s.gen || hour != s.hour || sun != s.sun || atmoSun != atmoKey || !( sunPath == sunKey ) ) {
		memo = computeLight();
		gen = s.gen;
		hour = s.hour;
		sun = s.sun;
		atmoSun = atmoKey;
		sunPath = sunKey;
	}
	return memo;
}

LdLight computeLight()
{
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
	// lane TERRSHADOW1: Sun Path = Real replaces the direction (one sun for every producer); the colours keep the clock
	wwSunPathApply( s.hour, s.gameDay, L.sunDir, L.discPos );
	if ( !s.haveWeather ) {
		neutral( L.sun, L.amb, L.dalc );
		std::copy( L.sun, L.sun + 3, L.effect );
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
		EsmWeather::blendRowLab( s.w, WwRowEffectLighting, L.keys, rgb );
	else
		EsmWeather::blendRow( s.w, WwRowEffectLighting, L.keys, rgb );
	for ( int c = 0; c < 3; c++ )
		L.effect[c] = lin( rgb[c] );
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
	if ( wwAtmoSunKey() > 0 ) {
		/* lane SKYA (ATMOSUN1): the key and the ambient through the physical sky's air toward the light (the moon by
		 * night, as the engine's light is), the brightness anchored on the weather's Day (Night) key at 45 degrees */
		const WwSkyClock raw = wwSkyClock( s.hour, s.tnam, s.gmst );
		const bool night = s.hour < raw.A || s.hour > raw.D;
		WwTodKeys k;
		k.a = k.b = night ? WwTodNight : WwTodDay;
		k.t = 0.0f;
		EsmWeather::blendRowLab( s.w, WwRowSunlight, k, rgb );
		const float keyLum = 0.2126f * lin( rgb[0] ) + 0.7152f * lin( rgb[1] ) + 0.0722f * lin( rgb[2] );
		const float len = std::sqrt( L.sunDir[0] * L.sunDir[0] + L.sunDir[1] * L.sunDir[1] + L.sunDir[2] * L.sunDir[2] );
		const WwFog fog = currentFog();
		const float fogIn[4] = { fog.effOff ? 0.0f : fog.fogFar, fog.effOff ? 0.0f : fog.maxv, fog.fMid, fog.fRange };
		wwAtmoKeyLight( len > 0.0f ? L.sunDir[2] / len : 1.0f, fogIn, keyLum, L.sun, L.amb, L.dalc );
	}
	if ( wwAtmoNightOn() ) {
		// lane SKYA (NIGHT1): by night the engine's key light is the moon's: its phase dims it
		const WwSkyClock raw = wwSkyClock( s.hour, s.tnam, s.gmst );
		if ( s.hour < raw.A || s.hour > raw.D ) {
			const float k = moonLightFactor( s );
			for ( int c = 0; c < 3; c++ )
				L.sun[c] *= k;
		}
	}
	return L;
}

QString cubeSource()
{
	const LdState & s = st();
	return QString( "%1(%2)" ).arg( s.cube, s.pinCube ? QStringLiteral( "pinned" ) : QStringLiteral( "default" ) );
}

//! the ground frame (lowest visible vertex, centre, half size), recomputed when the scene's bounds move;
//! the ground quad sits on it and the fog measures height from it (lane FOG1)
void updateGroundFrame( Scene * scene )
{
	LdState & s = st();
	const BoundSphere bs = scene->bounds();
	const float key[4] = { bs.center[0], bs.center[1], bs.center[2], bs.radius };
	if ( std::equal( key, key + 4, s.bsKey ) )
		return;
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
	// lane TERRSHADOW1: WW_LOOKDEV_GROUND_HALF=<units> widens the quad (the 1-degree shadow of a 512-unit cube is 29 k long)
	bool ok = false;
	const float pinHalf = wwEnvStr( "WW_LOOKDEV_GROUND_HALF" ).toFloat( &ok );
	if ( ok && pinHalf > s.groundHalf )
		s.groundHalf = pinHalf;
}

//! the fog at the current hour (lane FOG1): always the engine clock, whatever the Sun row says
WwFog currentFog()
{
	resolve();
	const LdState & s = st();
	// lane PERF1: memoised like currentLight() (asked once per fogged shape)
	static quint64 gen = 0;
	static double hour = -1.0;
	static WwFog memo;
	if ( gen != s.gen || hour != s.hour ) {
		memo = wwFogAt( s.w, s.hour, s.tnam, s.gmst );
		gen = s.gen;
		hour = s.hour;
	}
	return memo;
}

QString fogSummary()
{
	const LdState & s = st();
	if ( !s.haveWeather )
		return QStringLiteral( "fog=refused(no weather)" );
	const WwFog f = currentFog();
	return QString( "fogw=%1 near=%2 far=%3 power=%4 max=%5 keys=%6,%7,%8 farlow=%9,%10,%11" )
		.arg( double( f.w ), 0, 'f', 4 ).arg( double( f.fogNear ), 0, 'f', 1 ).arg( double( f.fogFar ), 0, 'f', 1 )
		.arg( double( f.power ), 0, 'f', 4 ).arg( double( f.maxv ), 0, 'f', 4 )
		.arg( wwTodName( f.keys.a ), wwTodName( f.keys.b ) ).arg( double( f.keys.t ), 0, 'f', 4 )
		.arg( double( f.farLow[0] ), 0, 'f', 4 ).arg( double( f.farLow[1] ), 0, 'f', 4 ).arg( double( f.farLow[2] ), 0, 'f', 4 );
}

} // namespace

bool wwLookdevGodRays( float m[12], QString * note )
{
	// lane VOLFOG1: the weather's WGDR, the two ToD slots of the colour keys blended linearly (the engine's god-ray settings
	// the engine sums every term weight-linearly; a slot without a GDRY adds the fallback constants)
	resolve();
	const LdState & s = st();
	if ( !s.haveWeather )
		return false;
	const WwFog f = currentFog();
	const WwGodRayMedium g = wwGodRayMix( s.w.godRay[f.keys.a], s.w.godRay[f.keys.b], float( f.keys.t ) );
	const float v[12] = { g.air[0], g.air[1], g.air[2], g.fwd[0], g.fwd[1], g.fwd[2], g.back[0], g.back[1], g.back[2],
		g.gFwd, g.gBack, g.post };
	std::copy( v, v + 12, m );
	if ( note )
		*note = QStringLiteral( "WTHR %1 WGDR %2,%3 t=%4 found=%5/8%6" ).arg( s.w.edid, QString::fromLatin1( wwTodName( f.keys.a ) ), QString::fromLatin1( wwTodName( f.keys.b ) ) )
			.arg( double( f.keys.t ), 0, 'f', 3 ).arg( s.w.godRayFound ).arg( g.fallback ? QStringLiteral( " fallback" ) : QString() );
	return true;
}

bool wwLookdevFogWanted( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return false;
	LdState & s = st();
	const bool leak = !s.fog && wwLookdevRed( "fogleak" );
	const bool perspective = scene->renderer->globalUniforms->projectionMatrix[3][3] != 1.0f;
	bool on = ( s.fog || leak ) && wwLookdevActive() && perspective && !scene->selecting;
	if ( on ) {
		resolve();
		if ( !s.haveWeather ) {
			on = false;
			s.fogLast = QStringLiteral( "refused(no weather)" );
		}
	}
	return on;
}

void wwLookdevFogUniforms( Scene * scene )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuFogUni );	// lane PERF2
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "fogOn" ) < 0 )
		return;
	wwVolAirUniforms( scene );	// lane VOLAIR1: every program that holds the fog holds the air's sampler
	wwAtmoApUniforms( scene );	// lane SKYA: and the sky's aerial perspective (read with the fog row off too)
	LdState & s = st();
	const bool on = wwLookdevFogWanted( scene );
	prog->uni1b( "fogOn", on );
	if ( !on )
		return;
	updateGroundFrame( scene );
	const WwFog f = currentFog();
	for ( int k = 0; k < 6; k++ )
		prog->uni4f_l( prog->uniLocation( "fogK[%d]", k ), FloatVector4( f.K[k][0], f.K[k][1], f.K[k][2], f.K[k][3] ) );
	// the view: posView = R (sc p) + t, so p = R^T (posView - t) / sc; the z row, measured from the ground
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float zr[4] = { 0, 0, 0, 0 };
	for ( int k = 0; k < 3; k++ ) {
		zr[k] = vt.rotation( k, 2 ) / sc;
		zr[3] -= vt.rotation( k, 2 ) * vt.translation[k] / sc;
	}
	zr[3] -= s.groundZ;
	prog->uni4f( "fogView", FloatVector4( zr[0], zr[1], zr[2], zr[3] ) );
	prog->uni1f( "fogDistScale", 1.0f / sc );
	// the fog sun (INFERRED, spec_fog.md 2.5): the lookdev light's direction and Sunlight colour, intensity 1
	const LdLight L = currentLight();
	float v[3];
	for ( int i = 0; i < 3; i++ )
		v[i] = vt.rotation( i, 0 ) * L.sunDir[0] + vt.rotation( i, 1 ) * L.sunDir[1] + vt.rotation( i, 2 ) * L.sunDir[2];
	const float vl = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	for ( int i = 0; i < 3 && vl > 0.0f; i++ )
		v[i] /= vl;
	prog->uni4f( "fogSun", FloatVector4( v[0], v[1], v[2], 1.0f ) );
	prog->uni4f( "fogSunColour", FloatVector4( L.sun[0], L.sun[1], L.sun[2], s.gmst.dirFogPower ) );
	prog->uni4f( "fogProbe", FloatVector4( s.fogProbe[0], s.fogProbe[1], s.fogProbe[2], 0.0f ) );
	prog->uni1i( "fogRed", ( wwLookdevRed( "fogmaxclamp" ) ? 1 : 0 ) | ( wwLookdevRed( "fognoescape" ) ? 2 : 0 )
		| ( wwLookdevRed( "fogheight0" ) ? 4 : 0 ) );
	static float lastZ = 0.0f, lastSc = 0.0f;	// lane PERF1: the echo string rebuilt only when it changes
	static bool lastLeak = false, lastOn = false;
	if ( !lastOn || lastZ != s.groundZ || lastSc != sc || lastLeak != !s.fog || !s.fogLast.startsWith( QLatin1StringView( "on(" ) ) ) {
		s.fogLast = QString( "on(groundz=%1 scale=%2%3)" ).arg( double( s.groundZ ), 0, 'f', 2 ).arg( double( sc ), 0, 'g', 6 )
			.arg( !s.fog ? QStringLiteral( " leak" ) : QString() );	// on with the row off = the fogleak red
		lastOn = true;
		lastZ = s.groundZ;
		lastSc = sc;
		lastLeak = !s.fog;
	}
}

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

bool wwLookdevWeatherId( quint32 & formID, QString & edid, QString & owner, QStringList & files, QString & dataDir )
{
	resolve();
	const LdState & s = st();
	if ( !s.haveWeather )
		return false;
	formID = s.w.formID;
	edid = s.w.edid;
	owner.clear();
	for ( const WwWeatherEntry & e : s.list )
		if ( e.formID == s.w.formID )
			owner = e.ownerFile;
	files = s.esm.files();
	dataDir = EsmWeather::gameDataDir();
	return true;
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

void wwLookdevClimateTnam( unsigned char out[4] )
{
	std::copy( st().tnam, st().tnam + 4, out );
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

bool wwLookdevFog()
{
	return st().fog;
}

void wwLookdevSetFog( bool on )
{
	setPart( st().fog, on, st().pinFog, kFogKey );
}

bool wwLookdevShadows()
{
	return st().shadows;
}

void wwLookdevSetShadows( bool on )
{
	setPart( st().shadows, on, st().pinShadows, kShadowsKey );
}

void wwLookdevShadowLight( float sunDir[3], float disc[3] )
{
	const LdLight L = currentLight();
	for ( int c = 0; c < 3; c++ ) {
		sunDir[c] = L.sunDir[c];
		disc[c] = L.discPos[c];
	}
}

bool wwLookdevSunLens( const WwLens *& lens, float sunDir[3], float sunColor[3] )
{
	resolve();
	const LdState & s = st();
	lens = nullptr;
	if ( !s.haveWeather || !s.sun || s.w.sunLens.sprites.isEmpty() )
		return false;
	const LdLight L = currentLight();
	for ( int c = 0; c < 3; c++ ) {
		sunDir[c] = L.sunDir[c];
		sunColor[c] = L.sun[c];
	}
	lens = &s.w.sunLens;
	return true;
}

bool wwLookdevGroundFrame( Scene * scene, float & z, float xy[2], float & half )
{
	LdState & s = st();
	updateGroundFrame( scene );
	z = s.groundZ;
	xy[0] = s.groundXY[0];
	xy[1] = s.groundXY[1];
	half = s.groundHalf;
	return s.ground && wwEnvStr( "WW_LOOKDEV_GROUNDPASS" ) != QLatin1StringView( "0" );
}

double wwLookdevGameDay()
{
	return st().gameDay;
}

float wwLookdevMoonShadowTerm( bool invertClear, QString * why )
{
	LdState & s = st();
	if ( !s.haveWeather ) {
		if ( why )
			*why = QStringLiteral( "moonsky=0(no weather)" );
		return 0.0f;
	}
	const unsigned char moons = s.clim.tnam[5];
	const int phase = wwMoonPhase( s.gameDay, moons );
	if ( phase < 0 || !( moons & 0xC0 ) ) {
		if ( why )
			*why = QStringLiteral( "moonsky=0(no moons)" );
		return 0.0f;
	}
	float lit = std::fabs( float( phase ) - 4.0f ) / 4.0f;
	{
		// lane EPHEM1: the real moon's lit fraction, and none while it is under the horizon
		double md[3], mlit = 0.0;
		if ( wwSkyMoonReal( s.hour, s.gameDay, md, nullptr, &mlit ) )
			lit = md[2] > 0.0 ? float( mlit ) : 0.0f;
	}
	const WwSkyClock c = wwSkyClock( s.hour, s.tnam, s.gmst );
	float alpha = 0.0f;
	if ( moons & 0x80 )
		alpha += wwMoonAlpha( c, s.gmst, s.gmst.masserFadeStart, s.gmst.masserFadeEnd );
	if ( moons & 0x40 )
		alpha += wwMoonAlpha( c, s.gmst, s.gmst.secundaFadeStart, s.gmst.secundaFadeEnd );
	alpha = std::clamp( alpha, 0.0f, 1.0f );
	// the sky's clearness: the Day Sunlight's share of Sunlight + Ambient luma, smoothstep over 0.38..0.68
	auto luma = []( const unsigned char * k ) { return 0.2126f * k[0] + 0.7152f * k[1] + 0.0722f * k[2]; };
	const float d = luma( s.w.color[WwTodDay][WwRowSunlight] ), a = luma( s.w.color[WwTodDay][WwRowAmbient] );
	float clear = 1.0f;
	if ( d + a > 0.0f ) {
		const float t = std::clamp( ( d / ( d + a ) - 0.38f ) / 0.30f, 0.0f, 1.0f );
		clear = t * t * ( 3.0f - 2.0f * t );
	}
	if ( invertClear )
		clear = 1.0f - clear;
	const float term = lit * lit * lit * alpha * clear;
	if ( why )
		*why = QStringLiteral( "moonsky=%1(phase %2 alpha %3 clear %4)" ).arg( term, 0, 'f', 3 ).arg( phase ).arg( alpha, 0, 'f', 2 ).arg( clear, 0, 'f', 2 );
	return term;
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
	if ( !wwWorldClockLive() )
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
	part( s.pinFog, kFogKey, s.fog );
	part( s.pinShadows, kShadowsKey, s.shadows );
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
			: QString() )
		+ ( s.fog ? QString( " fog=on %1 drew=%2" ).arg( fogSummary(), s.fogLast ) : QString() )
		+ ( ( s.shadows || !wwEnvStr( "WW_CSM_RED" ).isEmpty() ) ? QStringLiteral( " " ) + wwSunShadowSummary() : QString() )
		+ ( wwVolAirOn() ? QStringLiteral( " " ) + wwVolAirSummary() : QString() );	// lane VOLAIR1
}

QString wwLookdevEcho()
{
	const QString asked = wwEnvIsSet( "WW_LOOKDEV" ) ? wwEnvStr( "WW_LOOKDEV" ) : QStringLiteral( "unset" );
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

bool wwLookdevEffectLight( float rgb[3] )
{
	const LdLight L = currentLight();
	std::copy( L.effect, L.effect + 3, rgb );
	return L.fromWeather;
}

bool wwLookdevRegionEmit( quint32 regn, float rgb[3] )
{
	struct Region
	{
		bool ok = false;
		WwWeatherData w;
		int a = -1, b = -1;
		float t = -1.0f;
		float rgb[3] = { 0, 0, 0 };
	};
	static QHash<quint32, Region> cache;
	static quint64 gen = ~quint64( 0 );
	resolve();
	LdState & s = st();
	if ( gen != s.gen ) {
		gen = s.gen;
		cache.clear();
	}
	auto it = cache.find( regn );
	if ( it == cache.end() ) {
		Region g;
		if ( s.loadRefusal.isEmpty() ) {
			const quint32 w = s.esm.regionWeather( regn );
			QString why;
			g.ok = w && s.esm.read( w, g.w, &why );
		}
		it = cache.insert( regn, g );
	}
	if ( !it->ok )
		return false;
	const WwTodKeys k = currentLight().keys;
	if ( it->a != k.a || it->b != k.b || it->t != k.t ) {
		// the game: Sky::FillColorBlend of colour 9 (Effect Lighting) for the region's weather, the sky's own blend
		it->a = k.a;
		it->b = k.b;
		it->t = k.t;
		EsmWeather::blendRowLab( it->w, WwRowEffectLighting, k, it->rgb );
		for ( float & c : it->rgb )
			c = std::clamp( c / 255.0f, 0.0f, 1.0f );
	}
	std::copy( it->rgb, it->rgb + 3, rgb );
	return true;
}

bool wwLookdevDalc( float rgb[6][3] )
{
	const LdLight L = currentLight();
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			rgb[a][c] = L.dalc[a][c];
	return L.fromWeather;
}

namespace
{
bool computeImageSpace( WwLookdevIs & o );
}

// lane PERF1: memoised like currentLight() (the same inputs; the strings it builds cost more than the blend)
bool wwLookdevImageSpace( WwLookdevIs & o )
{
	resolve();
	const LdState & s = st();
	static quint64 gen = 0;
	static double hour = -1.0;
	static bool sun = false, ok = false;
	static WwLookdevIs memo;
	if ( gen != s.gen || hour != s.hour || sun != s.sun ) {
		memo = WwLookdevIs();
		ok = computeImageSpace( memo );
		gen = s.gen;
		hour = s.hour;
		sun = s.sun;
	}
	if ( ok )
		o = memo;
	return ok;
}

namespace
{
bool computeImageSpace( WwLookdevIs & o )
{
	const LdLight L = currentLight();	// resolves the weather; the keys of the hour
	const LdState & s = st();
	if ( !L.fromWeather || !s.haveWeather )
		return false;
	int a = std::clamp( L.keys.a, 0, 7 ), b = std::clamp( L.keys.b, 0, 7 );
	float t = std::clamp( L.keys.t, 0.0f, 1.0f );
	if ( !s.w.isFound[a] && !s.w.isFound[b] )
		return false;
	if ( !s.w.isFound[a] ) {	// one key without an IMGS: the other alone
		a = b;
		t = 0.0f;
	} else if ( !s.w.isFound[b] ) {
		b = a;
		t = 0.0f;
	}
	for ( int k = 0; k < 9; k++ )
		o.hdr[k] = s.w.isHdr[a][k] + ( s.w.isHdr[b][k] - s.w.isHdr[a][k] ) * t;
	for ( int k = 0; k < 3; k++ )
		o.cine[k] = s.w.isCine[a][k] + ( s.w.isCine[b][k] - s.w.isCine[a][k] ) * t;
	for ( int k = 0; k < 4; k++ )
		o.tint[k] = s.w.isTint[a][k] + ( s.w.isTint[b][k] - s.w.isTint[a][k] ) * t;
	o.lutA = s.w.isLut[a];
	o.lutB = s.w.isLut[b];
	o.t = ( o.lutA == o.lutB ) ? 0.0f : t;
	o.name = ( a == b || t <= 0.0f ) ? s.w.isEdid[a]
		: QStringLiteral( "%1->%2 %3%" ).arg( s.w.isEdid[a], s.w.isEdid[b] ).arg( int( t * 100.0f + 0.5f ) );
	o.key = QStringLiteral( "%1|%2|%3|%4" ).arg( s.weatherKey ).arg( a ).arg( b ).arg( double( t ), 0, 'g', 6 );
	return true;
}
}	// namespace

float wwLookdevSunlightScale()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_SUNSCALE_RED" ).trimmed() == "one" );
	WwLookdevIs is;
	if ( red || !wwLookdevImageSpace( is ) || !( is.hdr[6] > 0.0f ) )
		return 1.0f;
	return is.hdr[6];
}

/* ------------------------------------------------------------------------
 * The weather preview passes (lane PBRWX1): dome, moon, sun disc, clouds,
 * glare, in that order (INFERRED from the RenderDoc sequence, spec_clouds.md
 * s5). Every pass: view rotation only, depth off, out through studioOutput.
 * ---------------------------------------------------------------------- */
namespace
{

bool drawCube( Scene * scene, bool glow );

/* lane SKYPIC1: while the sky is captured for the probes (wwLookdevCaptureSky) the dome, star and cloud passes
 * write light, not a display picture: exposure 1 and view transform 3 (lookdev_output.glsl: linear, unclamped).
 * Otherwise these are the view's own, so every drawn frame is unchanged. */
bool g_skyCapture = false;
float g_skyCaptureZ = NAN;	// SKY3: the probes' height for the physical sky's capture (NaN: the view's, as before)
float skyExposure()
{
	return g_skyCapture ? 1.0f : wwSceneExposureScale();
}
int skyViewTransform()
{
	return g_skyCapture ? 3 : wwSceneViewTransform();
}

struct SkyFrame
{
	WwSkyClock c;
	float skyScale = 1.0f;
	float up[3] = {}, lo[3] = {}, hz[3] = {}, sun[3] = {}, glare[3] = {}, moonGlare[3] = {};	// linear
	float stars[3] = {};	// lane SKYFULL1: NAM0 row 6, the stars' tint (linear)
	float glareAlpha = 0.0f;
};

SkyFrame skyFrame()
{
	LdState & s = st();
	SkyFrame f;
	f.c = wwSkyClock( s.hour, s.tnam, s.gmst );
	// lane TERRSHADOW1: Sun Path = Real moves the drawn disc and glare with the light (one sun); Off leaves the clock's
	if ( wwSunPathMode() == 1 )
		wwSunPathApply( s.hour, s.gameDay, f.c.lightDir, f.c.sunPos );
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
	row( WwRowStars, f.stars );
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
	// lane SKYFULL1: meshes\sky\stars.nif and its sky shader's texture
	const void * starsTriedWith = nullptr;
	bool starsTried = false, starsOk = false;
	SkyMesh stars;
	QString starsTex, starsWhy;
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
	/* lane ALLON1: the resource stack's ARCHIVES (WW_LODGEN_RESOURCES / his MO2 profile). A cell view opened on the
	 * stack has no Fallout 4 archive index in the game manager, and the loose pass below never opens a .ba2, so the
	 * dome and the clouds (Fallout4 - Meshes.ba2) were "not found" and every all-on cell shot fell back to the cube.
	 * The cell's own models are read this way (lodgenReadModelBytes): the sky now resolves where they do.
	 * WW_LOOKDEV_RED=nostacksky skips it (the gate's refuter). */
	if ( !found && !wwLookdevRed( "nostacksky" ) )
		found = lodgenReadModelBytes( QString(), path, bytes ) && !bytes.isEmpty();
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
		// lane SKYFULL1 red "nodome": the dome's file never resolves (the refusal is named, the cube falls back)
		const QString domePath = wwLookdevRed( "nodome" ) ? QStringLiteral( "meshes/sky/atmosphere_red_nodome.nif" )
			: QStringLiteral( "meshes/sky/atmosphere.nif" );
		if ( loadSkyNif( scene, domePath, src, &w ) ) {
			for ( int b = 0; b < src.getBlockCount() && !m.domeOk; b++ ) {
				const QModelIndex i = src.getBlockIndex( b );
				if ( src.blockInherits( i, "BSTriShape" ) )
					m.domeOk = readSkyShape( src, i, m.dome );
			}
			if ( !m.domeOk )
				w = domePath + QStringLiteral( " has no readable BSTriShape" );
		}
		m.domeWhy = w;
	}
	if ( why )
		*why = m.domeWhy;
	return m.domeOk ? &m.dome : nullptr;
}

//! Clouds.nif: root child k that is a shape = layer k (as the engine loads them), capped at 32
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

//! lane SKYFULL1: Stars.nif, its first shape (uv scale/offset of its sky shader baked in) and that shader's texture
const SkyMesh * starsMesh( Scene * scene, QString * tex, QString * why )
{
	SkyMeshes & m = skyMeshes();
	if ( !m.starsOk && ( !m.starsTried || m.starsTriedWith != scene->nifModel ) ) {
		m.starsTried = true;
		m.starsTriedWith = scene->nifModel;
		m.stars = SkyMesh();
		m.starsTex.clear();
		NifModel src;
		QString w;
		if ( loadSkyNif( scene, QStringLiteral( "meshes/sky/stars.nif" ), src, &w ) ) {
			for ( int b = 0; b < src.getBlockCount() && !m.starsOk; b++ ) {
				const QModelIndex i = src.getBlockIndex( b );
				if ( !src.blockInherits( i, "BSTriShape" ) || !readSkyShape( src, i, m.stars ) )
					continue;
				m.starsOk = true;
				const QModelIndex iSh = src.getBlockIndex( src.getLink( i, "Shader Property" ) );
				if ( iSh.isValid() && src.blockInherits( iSh, "BSSkyShaderProperty" ) ) {
					m.starsTex = src.get<QString>( iSh, "Source Texture" );
					const Vector2 o = src.get<Vector2>( iSh, "UV Offset" ), sc = src.get<Vector2>( iSh, "UV Scale" );
					if ( sc[0] != 0.0f || sc[1] != 0.0f )
						for ( int k = 0; k + 1 < m.stars.uv.size(); k += 2 ) {
							m.stars.uv[k] = m.stars.uv[k] * sc[0] + o[0];
							m.stars.uv[k + 1] = m.stars.uv[k + 1] * sc[1] + o[1];
						}
				}
			}
			if ( !m.starsOk )
				w = QStringLiteral( "meshes/sky/stars.nif has no readable BSTriShape" );
			else if ( m.starsTex.isEmpty() )
				m.starsTex = QStringLiteral( "textures/sky/skystars.dds" );
		}
		m.starsWhy = w;
	}
	if ( why )
		*why = m.starsWhy;
	if ( tex )
		*tex = m.starsTex;
	return m.starsOk ? &m.stars : nullptr;
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
	prog->uni1f( "sceneExposure", skyExposure() );
	prog->uni1i( "viewTransform", skyViewTransform() );
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
	// lane EPHEM1: the real moon (row "Real sun and moon" + Sun Path Real): its own place, its phase from the elongation
	double md[3], mel = 0.0;
	const bool real = wwSkyMoonReal( s.hour, s.gameDay, md, &mel, nullptr );
	const int phase = real ? wwephem::phaseIndex( mel ) : wwMoonPhase( s.gameDay, moons );
	float realPos[3];
	if ( real ) {
		const float * p = f.c.sunPos;
		const float len = std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] );
		for ( int k = 0; k < 3; k++ )
			realPos[k] = float( md[k] ) * ( len > 1e-3f ? len : std::fabs( s.gmst.sunX ) );
	}
	const float * moonPos = real ? realPos : f.c.sunPos;
	const bool full = mul >= 1.0f;
	struct M { unsigned char bit; const char * name; float size, fs, fe; };
	const M list[2] = { { 0x80, "masser", s.gmst.masserSize, s.gmst.masserFadeStart, s.gmst.masserFadeEnd },
		{ 0x40, "secunda", s.gmst.secundaSize, s.gmst.secundaFadeStart, s.gmst.secundaFadeEnd } };
	bool any = false;
	for ( const M & m : list ) {
		if ( !( moons & m.bit ) )
			continue;
		any = true;
		const float alpha = ( real && md[2] < 0.0 ) ? 0.0f : wwMoonAlpha( f.c, s.gmst, m.fs, m.fe );
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
		const bool shOk = drawBill( scene, moonPos, half, sh, shadowTint, full ? shadowA : mul, false );
		const QString tex = QString( "textures/sky/%1_%2.dds" ).arg( QString::fromLatin1( m.name ),
			QString::fromLatin1( wwMoonPhaseSuffix( phase ) ) );
		// lane SKYA (NIGHT1): the disc through the air toward it (Moon Phase and Stars on; else 1, today's disc); MERGE1023: toward moonPos (EPHEM1's real moon, else the game's)
		float glare[3], ext[3];
		const float ml = std::sqrt( moonPos[0] * moonPos[0] + moonPos[1] * moonPos[1] + moonPos[2] * moonPos[2] );
		wwAtmoStarExtinction( ml > 0.0f ? moonPos[2] / ml : 1.0f, ext );
		for ( int c = 0; c < 3; c++ )
			glare[c] = f.moonGlare[c] * ext[c];
		if ( drawBill( scene, moonPos, half, tex, glare, full ? alpha : mul, false ) )
			drew << QString( "moon:%1(alpha=%2,shadow=%3%4%5)" ).arg( tex, f3( full ? alpha : mul ), f3( full ? shadowA : mul ),
				shOk ? QString() : QStringLiteral( ",no moonshadow.dds" ),
				real ? QStringLiteral( ",real elev=%1 elong=%2" ).arg( std::asin( std::clamp( md[2], -1.0, 1.0 ) ) * 57.29577951308232, 0, 'f', 2 ).arg( mel, 0, 'f', 1 ) : QString() );
		else
			drew << QString( "moon:refused(%1 did not resolve)" ).arg( tex );
	}
	if ( !any )
		drew << QString( "moon:none(the climate's moons byte 0x%1 names neither moon)" ).arg( moons, 2, 16, QChar( '0' ) );
}

/* lane SKYDISC1: our moon globe in place of the vanilla moon billboards and shadow disc (Physical Sky on, after the
 * physical sky drew this frame). False = its textures did not resolve: the caller draws the vanilla moon. */
bool drawMoonGlobe( Scene * scene, const SkyFrame & f, float mul, QStringList & drew )
{
	LdState & s = st();
	if ( !s.haveWeather ) {
		drew << QString( "moon:refused(%1)" ).arg( s.weatherRefusal );
		return true;
	}
	const unsigned char moons = s.clim.tnam[5];
	if ( !( moons & 0xC0 ) ) {
		drew << QString( "moon:none(the climate's moons byte 0x%1 names neither moon)" ).arg( moons, 2, 16, QChar( '0' ) );
		return true;
	}
	const WwAtmoFrame & af = wwAtmoFrame();
	if ( !af.valid ) {
		drew << QStringLiteral( "moon:globe:refused(no physical sky this frame)" );
		return true;
	}
	const MoonGeo g = moonGeo( s, f.c.sunPos );
	const bool masser = moons & 0x80;
	// the real moon is up whenever it is above the horizon (faint by day); the game's fades with the engine's clock
	float alpha = g.real ? 1.0f : wwMoonAlpha( f.c, s.gmst, masser ? s.gmst.masserFadeStart : s.gmst.secundaFadeStart,
		masser ? s.gmst.masserFadeEnd : s.gmst.secundaFadeEnd );
	const bool black = f.moonGlare[0] <= 0.0f && f.moonGlare[1] <= 0.0f && f.moonGlare[2] <= 0.0f;
	const bool full = mul >= 1.0f;
	if ( full && ( black || alpha <= 0.0f || g.D[2] < -0.01 ) ) {
		drew << QString( "moon:globe:hidden(alpha=%1%2)" ).arg( f3( alpha ), black ? QStringLiteral( ",moonglare black" ) : QString() );
		return true;
	}
	alpha = full ? alpha : mul;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "sky_moon.prog" );
	if ( !prog ) {
		drew << QStringLiteral( "moon:refused(sky_moon.prog did not link) vanilla moon" );
		return false;
	}
	static const QString texD = QStringLiteral( "textures/sky/moon/moon_d.dds" ), texN = QStringLiteral( "textures/sky/moon/moon_n.dds" );
	for ( int u = 0; u < 2; u++ ) {
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		if ( scene->bindTexture( QStringView( u ? texN : texD ), true ) <= 0 ) {
			r->stopProgram();
			r->fn->glActiveTexture( GL_TEXTURE0 );
			drew << QString( "moon:refused(%1 not found: the loose files of mods\\FO4CS\\Textures\\Sky\\Moon) vanilla moon" ).arg( u ? texN : texD );
			return false;
		}
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	const GLuint luts[3] = { af.trans, af.sunView, af.moonView };
	for ( int u = 0; u < 3; u++ ) {
		r->fn->glActiveTexture( GLenum( GL_TEXTURE2 + u ) );
		glBindTexture( GL_TEXTURE_2D, luts[u] );
	}
	double ex[3], ey[3], ez[3];
	if ( !wwskydisc::bodyFrame( g.D, g.P, g.lDeg, ex, ey, ez ) ) {
		ez[0] = ez[1] = 0.0, ez[2] = 1.0;
		ex[0] = 1.0, ex[1] = ex[2] = 0.0;
		ey[0] = 0.0, ey[1] = 1.0, ey[2] = 0.0;
	}
	GLint vp[4];
	glGetIntegerv( GL_VIEWPORT, vp );
	const float p11 = r->globalUniforms->projectionMatrix[1][1];
	const double pxAng = 2.0 / ( double( p11 > 1e-6f ? p11 : 1.0f ) * double( std::max( vp[3], 1 ) ) );
	const double sinR = std::sin( wwskydisc::kMoonHalfDeg * wwskydisc::kRad );
	const float exposure = skyExposure();
	const wwskydisc::MoonTerms t = wwskydisc::moonTerms( g.alphaDeg, af.sunK, exposure );
	skyState( r, true, false );
	r->fn->glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );	// premultiplied coverage
	prog->uni1i( "atAlbedo", 0 );
	prog->uni1i( "atRelief", 1 );
	prog->uni1i( "atTransTex", 2 );
	prog->uni1i( "atSunView", 3 );
	prog->uni1i( "atMoonView", 4 );
	prog->uni2f( "atViewSize", float( af.viewW ), float( af.viewH ) );
	prog->uni4f( "atRect", FloatVector4( float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	prog->uni1f( "atCamR", af.camR );
	prog->uni3f( "atSunW", af.sunW[0], af.sunW[1], af.sunW[2] );
	prog->uni3f( "atMoonW", af.moonW[0], af.moonW[1], af.moonW[2] );
	prog->uni1f( "atSunK", af.sunK );
	prog->uni1f( "atMoonK", af.moonK );
	prog->uni1f( "atTintOn", af.tintOn ? 1.0f : 0.0f );
	const GLint tintLoc = prog->uniLocation( "atTint" );
	if ( af.tintOn && tintLoc >= 0 )
		r->fn->glUniform3fv( tintLoc, 17, &af.tint[0][0] );
	prog->uni3f( "moonD", float( g.D[0] ), float( g.D[1] ), float( g.D[2] ) );
	prog->uni3f( "moonS", float( g.S[0] ), float( g.S[1] ), float( g.S[2] ) );
	prog->uni3f( "bodyX", float( ex[0] ), float( ex[1] ), float( ex[2] ) );
	prog->uni3f( "bodyY", float( ey[0] ), float( ey[1] ), float( ey[2] ) );
	prog->uni3f( "bodyZ", float( ez[0] ), float( ez[1] ), float( ez[2] ) );
	prog->uni1f( "sinR", float( sinR ) );
	prog->uni1f( "pxAng", float( pxAng ) );
	prog->uni1f( "lodD", float( wwskydisc::moonLod( 2048.0, pxAng ) ) );
	prog->uni1f( "lodN", float( wwskydisc::moonLod( 1024.0, pxAng ) ) );
	prog->uni1f( "albedoInv", float( 1.0 / wwskydisc::kAlbedoMean ) );
	prog->uni1f( "sunTerm", float( t.sunTerm ) );
	prog->uni1f( "earthTerm", float( t.earthTerm ) );
	prog->uni1f( "alpha", alpha );
	prog->uni1f( "sceneExposure", exposure );
	prog->uni1i( "viewTransform", skyViewTransform() );
	const float c[3] = { float( g.D[0] ), float( g.D[1] ), float( g.D[2] ) };
	prog->uni3f( "centerWorld", c[0], c[1], c[2] );
	prog->uni1f( "halfSize", float( std::tan( wwskydisc::kMoonHalfDeg * wwskydisc::kRad ) * 1.5 + 2.0 * pxAng ) );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	for ( int u = 4; u >= 0; u-- ) {
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
	drew << QString( "moon:globe(alpha=%1,phase=%2deg,lit=%3,lib=%4,%5%6)" ).arg( f3( alpha ) )
		.arg( g.alphaDeg, 0, 'f', 1 ).arg( 0.5 * ( 1.0 + std::cos( g.alphaDeg * wwskydisc::kRad ) ), 0, 'f', 3 )
		.arg( g.lDeg, 0, 'f', 2 ).arg( g.real ? QStringLiteral( "real" ) : QStringLiteral( "game" ) )
		.arg( wwAtmoDiscRed( "nophase" ) ? QStringLiteral( ",DISC_RED=nophase" ) : QString() );
	return true;
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
		float lc[3];	// lane CLOUD2: Volume Clouds Low lights the layers by the model (sky units / skyScale)
		if ( wwCloudVolLowColour( lc ) )
			prog->uni3f( "colLin", lc[0] / std::max( f.skyScale, 1e-6f ), lc[1] / std::max( f.skyScale, 1e-6f ),
				lc[2] / std::max( f.skyScale, 1e-6f ) );
		else
			prog->uni3f( "colLin", lin( rgb[0] ), lin( rgb[1] ), lin( rgb[2] ) );
		prog->uni1f( "alpha", alpha < 0.0f ? a : alpha );
		prog->uni2f( "uvOffset", wwCloudOffset( sx, secs ), wwCloudOffset( sy, secs ) );
		prog->uni1f( "skyScale", f.skyScale );
		prog->uni1f( "sceneExposure", skyExposure() );
		prog->uni1i( "viewTransform", skyViewTransform() );
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

bool wwLookdevCloudLayers( Scene * scene, QVector<WwCloudLayerIn> & out, QString & key, float eye[3], QString * why )
{
	out.clear();
	resolve();
	LdState & s = st();
	if ( !s.haveWeather ) {
		if ( why )
			*why = s.weatherRefusal;
		return false;
	}
	const SkyFrame f = skyFrame();
	QString mwhy;
	const QVector<SkyMesh> * ms = scene ? cloudMeshes( scene, &mwhy ) : nullptr;
	const double secs = wwLookdevCloudSeconds();
	key = QStringLiteral( "%1|%2|%3" ).arg( s.plugin, s.weatherKey ).arg( s.gen );
	eye[0] = eye[1] = eye[2] = 0.0f;
	if ( scene ) {
		const Transform & vt = scene->view;
		const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ )
				eye[i] -= vt.rotation( j, i ) * vt.translation[j] / sc;
	}
	const int n = ms ? std::min( int( ms->size() ), 32 ) : 32;
	for ( int i = 0; i < n; i++ ) {
		if ( s.w.cloudDisabled & ( 1u << i ) )
			continue;
		WwCloudLayerIn L;
		L.layer = i;
		EsmWeather::blendCloud( s.w, i, f.c.keys, L.rgb, &L.alpha );
		if ( L.alpha <= 0.0f || s.w.cloudTex[i].isEmpty() )
			continue;
		L.tex = skyTexPath( s.w.cloudTex[i] );
		const int li = ( quint32( i ) >= s.w.cloudLayers ) ? 0 : i;
		L.uvOffset[0] = wwCloudOffset( EsmWeather::cloudSpeed( s.w.cloudSpeedX[li], s.gmst ), secs );
		L.uvOffset[1] = wwCloudOffset( EsmWeather::cloudSpeed( s.w.cloudSpeedY[li], s.gmst ), secs );
		if ( ms && !ms->at( i ).idx.isEmpty() ) {
			const SkyMesh & m = ms->at( i );
			L.pos = m.pos.constData();
			L.col = m.col.constData();
			L.uv = m.uv.constData();
			L.nv = int( m.nv );
			L.idx = m.idx.constData();
			L.nIdx = int( m.idx.size() );
		}
		out << L;
	}
	if ( why )
		*why = ( scene && !ms ) ? mwhy : QString();
	return true;
}

bool wwLookdevCloudMesh( Scene * scene, WwCloudLayerIn & io )
{
	const QVector<SkyMesh> * ms = scene ? cloudMeshes( scene, nullptr ) : nullptr;
	if ( !ms || io.layer < 0 || io.layer >= ms->size() || ms->at( io.layer ).idx.isEmpty() )
		return false;
	const SkyMesh & m = ms->at( io.layer );
	io.pos = m.pos.constData();
	io.col = m.col.constData();
	io.uv = m.uv.constData();
	io.nv = int( m.nv );
	io.idx = m.idx.constData();
	io.nIdx = int( m.idx.size() );
	return true;
}

namespace
{

/*! lane SKYFULL1: the night sky. The stars shape (sky object type 5) takes the weather's Stars colour row as
 * its tint and the sky clock's stars alpha; it is hidden at alpha 0 or a black row (as the game does). Its
 * turn: 2 pi fmod(whole days + hour/24, fStarsRotateDays) / fStarsRotateDays about the normalized
 * fStarsRotate{X,Y,Z}Axis (exe defaults 4 days, +Z). Drawn with the cloud layer program (same texture x
 * vertex colour x tint x Sky Scale, alpha blended). */
void drawStars( Scene * scene, const SkyFrame & f, QStringList & drew )
{
	LdState & s = st();
	if ( !s.haveWeather ) {
		drew << QString( "stars:refused(%1)" ).arg( s.weatherRefusal );
		return;
	}
	if ( wwLookdevRed( "nostars" ) )
		return;	// red: the night sky goes missing without a word (the stars gate must fail)
	const float a = f.c.starsAlpha;
	const bool black = f.stars[0] <= 0.0f && f.stars[1] <= 0.0f && f.stars[2] <= 0.0f;
	if ( a <= 0.0f || black ) {
		drew << QString( "stars:hidden(alpha=%1%2)" ).arg( f3( a ), black ? QStringLiteral( ",stars row black" ) : QString() );
		return;
	}
	QString tex, why;
	const SkyMesh * m = starsMesh( scene, &tex, &why );
	if ( !m ) {
		drew << QString( "stars:refused(%1)" ).arg( why );
		return;
	}
	// the turn
	constexpr double kTau = 6.283185307179586;
	const float rotDays = s.gmst.starsRotDays > 1e-6f ? s.gmst.starsRotDays : 4.0f;
	const double days = std::floor( s.gameDay ) + s.hour / 24.0;
	const float ang = float( kTau * std::fmod( days, double( rotDays ) ) / double( rotDays ) );
	float ax[3] = { s.gmst.starsAxis[0], s.gmst.starsAxis[1], s.gmst.starsAxis[2] };
	const float al = std::sqrt( ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2] );
	if ( al < 1e-6f ) {
		ax[0] = ax[1] = 0.0f;
		ax[2] = 1.0f;
	} else {
		for ( float & c : ax )
			c /= al;
	}
	const float cs = std::cos( ang ), sn = std::sin( ang );
	QVector<float> pos( m->pos.size() );
	for ( int k = 0; k + 2 < m->pos.size(); k += 3 ) {
		const float v[3] = { m->pos[k], m->pos[k + 1], m->pos[k + 2] };
		const float d = ax[0] * v[0] + ax[1] * v[1] + ax[2] * v[2];
		const float x[3] = { ax[1] * v[2] - ax[2] * v[1], ax[2] * v[0] - ax[0] * v[2], ax[0] * v[1] - ax[1] * v[0] };
		for ( int c = 0; c < 3; c++ )
			pos[k + c] = v[c] * cs + x[c] * sn + ax[c] * d * ( 1.0f - cs );	// Rodrigues
	}
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_clouds.prog" );
	if ( !prog ) {
		drew << QStringLiteral( "stars:refused(lookdev_clouds.prog did not link)" );
		return;
	}
	const QString path = skyTexPath( tex );
	r->fn->glActiveTexture( GL_TEXTURE0 );
	if ( scene->bindTexture( QStringView( path ), true ) <= 0 ) {
		r->stopProgram();
		drew << QString( "stars:refused(%1 did not resolve)" ).arg( path );
		return;
	}
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	skyState( r, true, false );
	prog->uni1i( "Tex", 0 );
	prog->uni1b( "probe", false );
	prog->uni3f( "colLin", f.stars[0], f.stars[1], f.stars[2] );
	prog->uni1f( "alpha", a );
	prog->uni2f( "uvOffset", 0.0f, 0.0f );
	prog->uni1f( "skyScale", f.skyScale );
	prog->uni1f( "sceneExposure", skyExposure() );
	prog->uni1i( "viewTransform", skyViewTransform() );
	// lane SKYA (NIGHT1): each star through the air toward it (Moon Phase and Stars on: the horizon fades and reddens)
	QVector<float> colExt;
	if ( wwAtmoNightOn() && m->nv > 0 && m->col.size() == int( m->nv ) * 4 && pos.size() == int( m->nv ) * 3 ) {
		colExt = m->col;
		for ( int v = 0; v < int( m->nv ); v++ ) {
			const float * p = pos.constData() + v * 3;
			const float l = std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] );
			float ext[3];
			wwAtmoStarExtinction( l > 0.0f ? p[2] / l : 1.0f, ext );
			for ( int c = 0; c < 3; c++ )
				colExt[v * 4 + c] *= ext[c];
		}
	}
	const float * attrs[8] = { pos.constData(), colExt.isEmpty() ? m->col.constData() : colExt.constData(), nullptr,
		nullptr, nullptr, nullptr, nullptr,
		m->uv.constData() };
	r->drawShape( m->nv, 0x20000043ULL, unsigned( m->idx.size() ), GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs, m->idx.constData() );
	r->stopProgram();
	drew << QString( "stars:%1(alpha=%2,turn=%3deg)" ).arg( path, f3( a ) ).arg( double( ang ) * 360.0 / kTau, 0, 'f', 1 );
}

//! lane ATMO1: the physical sky in place of the dome (gl/atmosphere.h); the vanilla sun quad and glare then stay off
bool drawAtmo( Scene * scene, QStringList & drew, bool capture = false )
{
	LdState & s = st();
	if ( wwAtmoTier() <= 0 || !s.haveWeather )
		return false;
	WwAtmoInputs in;
	const WwSkyClock raw = wwSkyClock( s.hour, s.tnam, s.gmst );
	const bool night = s.hour < raw.A || s.hour > raw.D;
	if ( wwSunPathMode() == 1 ) {
		double r[3];
		wwSunPathReal( s.hour, s.gameDay, r );
		for ( int c = 0; c < 3; c++ )
			in.sunW[c] = float( r[c] );
	} else {
		// the engine's arc; at night its point is the moon's, so the sun is that point mirrored under the horizon
		for ( int c = 0; c < 3; c++ )
			in.sunW[c] = raw.sunPos[c];
		if ( night )
			in.sunW[2] = -raw.sunPos[2];
	}
	if ( night ) {
		for ( int c = 0; c < 3; c++ )
			in.moonW[c] = raw.sunPos[c];
		in.moonUp = raw.sunPos[2] > 0.0f;
		if ( wwAtmoDiscs() ) {	// lane SKYDISC1: the sky's moonlight from where our globe is drawn
			const MoonGeo g = moonGeo( s, raw.sunPos );
			for ( int c = 0; c < 3; c++ )
				in.moonW[c] = float( g.D[c] );
			in.moonUp = g.D[2] > 0.0;
		}
	}
	auto keyLum = [&]( int tod ) {
		WwTodKeys k;
		k.a = k.b = tod;
		k.t = 0.0f;
		float rgb[3];
		EsmWeather::blendRowLab( s.w, WwRowSunlight, k, rgb );
		return 0.2126f * lin( rgb[0] ) + 0.7152f * lin( rgb[1] ) + 0.0722f * lin( rgb[2] );
	};
	const float scale = wwLookdevSunlightScale();
	in.sunLum = keyLum( WwTodDay ) * scale;
	in.moonLum = keyLum( WwTodNight ) * scale * moonLightFactor( s );
	const WwFog fog = currentFog();
	if ( !fog.effOff ) {
		in.fogFar = fog.fogFar;
		in.fogMax = fog.maxv;
		in.fogMid = fog.fMid;
		in.fogRange = fog.fRange;
	}
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int j = 0; j < 3; j++ )
		in.camZ -= vt.rotation( j, 2 ) * vt.translation[j] / sc;
	if ( capture && std::isfinite( g_skyCaptureZ ) )
		in.camZ = g_skyCaptureZ;	// SKY3: the probes see the sky from where they stand, not from the view camera
	in.disc = s.sun && !capture;	// lane SKYA (SKYGI1): the probes' capture has no disc (the sun is the bake's own term)
	in.capture = capture;
	in.discs = wwAtmoDiscs() && !capture;	// lane SKYDISC1: our sun disc
	in.exposure = skyExposure();
	in.viewTransform = skyViewTransform();
	// lane SKYA (ATMOCHECK1): the weather dome's colors by elevation for the Sky Color row's Weather Tint; the rings come
	// from the dome mesh's vertex weights once per mesh, the colors from this hour's weather each frame
	if ( wwAtmoTint() > 0 ) {
		QString why;
		const SkyMesh * m = domeMesh( scene, &why );
		const int cs = m && m->nv > 0 ? int( m->col.size() ) / int( m->nv ) : 0;
		if ( m && cs >= 3 && m->pos.size() >= int( m->nv ) * 3 ) {
			static const void * ringsFor = nullptr;
			static quint32 ringsNv = 0;
			static std::vector<float> ringEl, ringW;
			if ( ringsFor != m || ringsNv != m->nv ) {
				std::vector<float> el( m->nv ), w( size_t( m->nv ) * 3 );
				for ( quint32 i = 0; i < m->nv; i++ ) {
					const float * p = m->pos.constData() + size_t( i ) * 3;
					el[i] = std::atan2( p[2], std::hypot( p[0], p[1] ) ) * 57.2957795f;
					for ( int c = 0; c < 3; c++ )
						w[size_t( i ) * 3 + size_t( c )] = m->col[int( i ) * cs + c];
				}
				WwAtmoMath::tintRings( el.data(), reinterpret_cast<const float ( * )[3]>( w.data() ), int( m->nv ), ringEl, ringW );
				ringsFor = m;
				ringsNv = m->nv;
			}
			const SkyFrame f = skyFrame();
			WwAtmoMath::tintTable( ringEl, ringW, f.hz, f.lo, f.up, f.skyScale, wwAtmoTintRedFlat(), in.tint );
			in.tintOk = true;
		}
	}
	QString line;
	const bool ok = wwAtmoDrawSky( scene, in, &line );
	drew << QStringLiteral( "sky:" ) + line;
	return ok;
}

} // namespace

bool wwLookdevDrawBackground( Scene * scene )
{
	wwAtmoApFrame();	// lane SKYA: last frame's aerial perspective is stale (drawAtmo builds this frame's)
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
		// lane SKYFULL1: the game draws no sky in an interior unless its cell has Show Sky (DATA bit 7); with the
		// Sky row on, such an interior gets the black clear and no pass at all (red "interiorsky": the sky leaks in)
		const WwCellLighting * cl = scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
		if ( sx.sky && cl && cl->interior && !cl->showSky && !wwLookdevRed( "interiorsky" ) ) {
			GLfloat was[4];
			glGetFloatv( GL_COLOR_CLEAR_VALUE, was );
			glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
			glClear( GL_COLOR_BUFFER_BIT );
			glClearColor( was[0], was[1], was[2], was[3] );
			const QString line = QStringLiteral( "sky:none(interior without Show Sky)" );
			if ( sx.skyLast != line )
				fprintf( stderr, "lookdev sky: %s\n", qPrintable( line ) );
			sx.skyLast = line;
			return true;
		}
		/* finalfix (SUNCELL1): the backdrop draws single-sample. Its blended layers (clouds, stars, sun, moon) blended
		 * into the multisampled frame came out +-1 step differently run to run on identical inputs; with multisampling
		 * off two runs match. The backdrop has no geometry edge to smooth: the scene drawn after it keeps its MSAA. */
		struct SingleSample {
			const GLboolean was = glIsEnabled( GL_MULTISAMPLE );
			SingleSample() { glDisable( GL_MULTISAMPLE ); }
			~SingleSample() { if ( was ) glEnable( GL_MULTISAMPLE ); }
		} singleSample;
		const SkyFrame f = skyFrame();
		// the dome replaces the cube; a dome that will not resolve falls back to the cube, by name
		bool dome = false, physical = false;
		if ( sx.sky ) {
			physical = drawAtmo( scene, drew );	// lane ATMO1: Physical Sky Off = the dome below, as before
			// the dome is never fogged (ruling); red "fogsky": Fog ON darkens the dome, the sky gate must fail
			dome = physical || drawDome( scene, f, ( sx.fog && wwLookdevRed( "fogsky" ) ) ? 0.97f : 1.0f, drew );
		}
		/* SKY2 SKYGREY1 (bungo 10-23: the cube only where the game uses it, never as the sky or the void): no dome
		 * = the viewport's own grey clear, the sun/moon/cloud rows still draw over it. Red cubesky: the cube as before. */
		const bool cubeSky = wwLookdevRed( "cubesky" );
		if ( !dome && cubeSky && !drawCube( scene, !sx.sun ) )
			return false;
		if ( !dome && !cubeSky )
			glClear( GL_COLOR_BUFFER_BIT );
		if ( !dome )	// lane SKYFULL1: the cube is only ever the named fallback
			drew << ( cubeSky ? ( sx.sky ? QStringLiteral( "cube:fallback(the dome refused)" ) : QStringLiteral( "cube(sky row off)" ) )
							  : ( sx.sky ? QStringLiteral( "grey(the dome refused)" ) : QStringLiteral( "grey(sky row off)" ) ) );
		else
			drawStars( scene, f, drew );	// lane SKYFULL1: on the dome, under the moon (its shadow disc hides them)
		if ( !sx.sky && wwLookdevRed( "skyleak" ) )
			drawDome( scene, f, 0.02f, drew );
		const float moonMul = sx.moon ? 1.0f : ( wwLookdevRed( "moonleak" ) ? 0.02f : 0.0f );
		// lane SKYDISC1: the Physical Sky's moon is our globe; missing loose files = the vanilla moon, named in the line
		if ( moonMul > 0.0f && !( physical && wwAtmoDiscs() && drawMoonGlobe( scene, f, moonMul, drew ) ) )
			drawMoon( scene, f, moonMul, drew );
		// lane ATMO1: the physical sky draws its own disc; glow comes from the bloom (post), never a drawn glare
		const float sunMul = physical ? 0.0f : sx.sun ? 1.0f : ( wwLookdevRed( "sunleak" ) ? 0.02f : 0.0f );
		if ( sunMul > 0.0f )
			drawSunQuad( scene, f, false, sunMul, drew );
		const float cloudMul = sx.clouds ? 1.0f : ( wwLookdevRed( "cloudleak" ) ? 0.02f : 0.0f );
		// lane CLOUD2: Volume Clouds Medium+ draw the volume in place of the 2D layers (false = the layers, as before)
		const bool volume = cloudMul >= 1.0f && wwCloudVolTier() > 0
			&& wwCloudVolDraw( scene, { skyExposure(), skyViewTransform(), wwLookdevCloudSeconds(), g_skyCapture }, drew );
		if ( cloudMul > 0.0f && !volume )
			drawClouds( scene, f, cloudMul, drew );
		if ( sunMul > 0.0f )
			drawSunQuad( scene, f, true, sunMul, drew );
		if ( wwCloudMapTier() > 0 )
			wwCloudMapUpdate( scene );	// lane CLOUD1: Off builds nothing
		const QString line = drew.isEmpty() ? QStringLiteral( "nothing" ) : drew.join( QChar( '|' ) );
		static const QRegularExpression clock( QStringLiteral( " t=[0-9.]+" ) );
		if ( QString( sx.skyLast ).remove( clock ) != QString( line ).remove( clock ) )
			fprintf( stderr, "lookdev sky: %s\n", qPrintable( line ) );	// lane SKYFULL1: each pass drawn or refused
		sx.skyLast = line;
		glDisable( GL_BLEND );
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_TRUE );
		return true;
	}
	// SKY2 SKYGREY1: nothing on (or ortho) = the grey clear (the caller clears); red cubesky draws the cube
	return wwLookdevRed( "cubesky" ) && drawCube( scene, true );
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

/* lane LIGHTBOOTH1 (the light booth; harness pins, never a user toggle): the cell view draws into its own HDR frame and
 * draws the sky into it, so the ground drawn before that frame never reaches the picture. wwLookdevDrawGroundCellFrame
 * draws it again INSIDE the frame (WW_LOOKDEV_GROUND_CELL=1: lit by the placed lights, linear, depth written, so the
 * air and the fixture composite against it); WW_LOOKDEV_GROUND_CELL_AT=x,y,z centres it on the fixture's foot */
static bool sGroundInFrame = false;

static bool groundCellAt( float xy[2], float & z )
{
	WW_SWITCH_STATIC( QList<QByteArray>, p, wwEnv( "WW_LOOKDEV_GROUND_CELL_AT" ).split( ',' ) );
	if ( p.size() != 3 )
		return false;
	bool ok[3];
	const float v[3] = { p[0].trimmed().toFloat( &ok[0] ), p[1].trimmed().toFloat( &ok[1] ), p[2].trimmed().toFloat( &ok[2] ) };
	if ( !ok[0] || !ok[1] || !ok[2] )
		return false;
	xy[0] = v[0];
	xy[1] = v[1];
	z = v[2];
	return true;
}

void wwLookdevDrawGroundCellFrame( Scene * scene )
{
	WW_SWITCH_STATIC( bool, cellGround, wwEnvInt( "WW_LOOKDEV_GROUND_CELL" ) == 1 );
	if ( !cellGround || !wwLookdevActive() || !scene || !wwCellLightsWanted( scene ) )
		return;
	sGroundInFrame = true;
	wwLookdevDrawGround( scene );
	sGroundInFrame = false;
}

void wwLookdevDrawGround( Scene * scene )
{
	// harness reference: WW_LOOKDEV_GROUNDPASS=0 -> the ground pass never runs at
	// all (the "no ground" picture the Ground-off gate is judged against)
	WW_SWITCH_STATIC( bool, noPass, wwEnvStr( "WW_LOOKDEV_GROUNDPASS" ) == QLatin1StringView( "0" ) );
	if ( noPass )
		return;
	if ( !scene || !scene->renderer || scene->selecting || scene->hasVisMode( Scene::VisSilhouette ) )
		return;
	LdState & s = st();
	const bool leak = !s.ground && wwLookdevRed( "groundleak" );
	if ( !s.ground && !leak )
		return;

	updateGroundFrame( scene );

	Renderer * r = scene->renderer;
	// lane CSM1: the shadow-receiving ground is its own program (WW_SUNSHADOW defined),
	// swapped in only while the map was built this frame, so Shadows off draws the pre-CSM ground
	const bool receives = wwSunShadowWanted( scene );
	/* lane LIGHTBOOTH1 (the light booth; a harness pin, never a user toggle): WW_LOOKDEV_GROUND_CELL=1 draws the ground
	 * lit by the placed cell lights too (no sun cascades on it), so a fixture's pool of light shows on the booth floor */
	WW_SWITCH_STATIC( bool, cellGround, wwEnvInt( "WW_LOOKDEV_GROUND_CELL" ) == 1 );
	const bool litByCell = cellGround && wwCellLightsWanted( scene );
	NifSkopeOpenGLContext::Program * prog = r->useProgram( litByCell ? "lookdev_ground_cell.prog"
		: receives ? "lookdev_ground_csm.prog" : "lookdev_ground.prog" );
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
	wwLookdevFogUniforms( scene );
	if ( receives && !litByCell )
		wwSunShadowUniforms( scene );
	if ( litByCell )
		wwCellLightsUniforms( scene );

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

	float gxy[2] = { s.groundXY[0], s.groundXY[1] };
	float z = s.groundZ;
	const bool at = litByCell && groundCellAt( gxy, z );	// lane LIGHTBOOTH1
	if ( sGroundInFrame ) {
		static bool told = false;
		if ( !told ) {
			told = true;
			std::fprintf( stderr, "lookdev ground cell: drawn in the HDR frame (hdr %d) at %.1f %.1f %.1f (%s) half %.0f, lit by the placed lights %d\n",
				int( wwCellHdrActive() ), double( gxy[0] ), double( gxy[1] ), double( z ), at ? "WW_LOOKDEV_GROUND_CELL_AT" : "the scene's lowest vertex",
				double( s.groundHalf ), int( litByCell ) );
		}
	}
	const float x0 = gxy[0] - s.groundHalf, x1 = gxy[0] + s.groundHalf;
	const float y0 = gxy[1] - s.groundHalf, y1 = gxy[1] + s.groundHalf;
	const float quad[12] = { x0, y0, z, x1, y0, z, x0, y1, z, x1, y1, z };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	glDisable( GL_BLEND );
	glDepthMask( GL_TRUE );
}

/* lane WATER1: the game's water reflects a 3-colour sky (its water shader's horizon, lower and upper colours),
 * not the dome. The same rows the dome takes at the same hour, linear, without the dome's sky scale (INFERRED:
 * the scale is the sky shader's own). */
bool wwLookdevWaterSky( float horizon[3], float lower[3], float upper[3] )
{
	if ( !st().haveWeather )
		return false;
	const SkyFrame f = skyFrame();
	for ( int c = 0; c < 3; c++ ) {
		horizon[c] = f.hz[c];
		lower[c] = f.lo[c];
		upper[c] = f.up[c];
	}
	return true;
}

bool wwLookdevImageSpaceDof( float a[4], float b[4], float & t, bool & haveA, bool & haveB )
{
	if ( !st().haveWeather )
		return false;
	const SkyFrame f = skyFrame();
	const LdState & s = st();
	const int ka = f.c.keys.a, kb = f.c.keys.b;
	if ( ka < 0 || ka >= 8 || kb < 0 || kb >= 8 )
		return false;
	for ( int i = 0; i < 4; i++ ) {
		a[i] = s.w.dof[ka][i];
		b[i] = s.w.dof[kb][i];
	}
	haveA = s.w.hasDof[ka];
	haveB = s.w.hasDof[kb];
	t = f.c.keys.t;
	return true;
}

bool wwLookdevWind( quint8 out[4] )
{
	const LdState & s = st();
	if ( !s.haveWeather || !s.w.hasWind )
		return false;
	out[0] = s.w.windSpeed;
	out[1] = s.w.windDir;
	out[2] = s.w.windDirRange;
	out[3] = s.w.windTurb;
	return true;
}

/* ------------------------------------------------------------------------
 * lane SKYPIC1 (2026-10-11): the drawn sky, captured for the probes.
 * ---------------------------------------------------------------------- */
void wwSkyPictureFace( int f, float F[3], float R[3], float U[3] )
{
	// forward per face +X -X +Y -Y +Z -Z; up = +Z for the four sides, +Y for the two poles; right = forward x up
	static const float fw[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	static const float up[6][3] = { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };
	const int k = std::clamp( f, 0, 5 );
	for ( int c = 0; c < 3; c++ ) {
		F[c] = fw[k][c];
		U[c] = up[k][c];
	}
	R[0] = F[1] * U[2] - F[2] * U[1];
	R[1] = F[2] * U[0] - F[0] * U[2];
	R[2] = F[0] * U[1] - F[1] * U[0];
}

QString wwLookdevSkyPictureKey()
{
	resolve();
	const LdState & s = st();
	// the cloud clock is left out: in the window it runs, and the probes would never stand still (the capture
	// follows the weather, the hour, the day and the rows; the clouds are where they were at that moment)
	return QStringLiteral( "%1|%2|%3|day %4|sky %5 clouds %6|%7" ).arg( s.plugin, s.weatherKey ).arg( s.hour, 0, 'f', 4 )
		.arg( s.gameDay, 0, 'f', 2 ).arg( int( s.sky ) ).arg( int( s.clouds ) ).arg( s.haveWeather ? 1 : 0 )
		// lane SKYA (SKYGI1): only while the probes take the physical sky (Off: today's key, character for character)
		+ ( wwAtmoGiActive() ? QStringLiteral( "|atmo gi tier %1 tint %2 red %3" ).arg( wwAtmoTier() ).arg( wwAtmoTint() )
			.arg( wwAtmoGiRedDome() ? 1 : 0 ) : QString() );
}

void wwLookdevSetSkyCaptureZ( float z )
{
	g_skyCaptureZ = z;
}

bool wwLookdevCaptureSky( Scene * scene, int n, const float * flatRgb, WwSkyPicture & out, QString * why )
{
	auto fail = [why]( const QString & w ) {
		if ( why )
			*why = w;
		return false;
	};
	if ( !scene || !scene->renderer )
		return fail( QStringLiteral( "no renderer" ) );
	if ( !wwLookdevActive() )
		return fail( QStringLiteral( "not in Scene mode Lookdev" ) );
	resolve();
	LdState & sx = st();
	if ( !flatRgb ) {
		if ( !sx.haveWeather )
			return fail( QStringLiteral( "no weather (%1)" ).arg( sx.weatherRefusal ) );
		if ( !sx.sky )
			return fail( QStringLiteral( "the Sky row is off (the view draws the cube, not the sky)" ) );
	}
	n = std::clamp( n, 8, 512 );
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	QElapsedTimer clock;
	clock.start();

	GLint prevRead = 0, prevDraw = 0, vp[4], prevTex = 0, prevActive = 0;
	GLfloat clearWas[4];
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GL_TEXTURE0 );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	fn->glGetFloatv( GL_COLOR_CLEAR_VALUE, clearWas );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasDepth = fn->glIsEnabled( GL_DEPTH_TEST );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasMs = fn->glIsEnabled( GL_MULTISAMPLE ), wasSrgb = fn->glIsEnabled( GL_FRAMEBUFFER_SRGB );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const NifSkopeOpenGLContext::GlobalUniforms guWas = *r->globalUniforms;

	GLuint tex = 0, fbo = 0;
	fn->glGenTextures( 1, &tex );
	fn->glBindTexture( GL_TEXTURE_2D, tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, n, n, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	fn->glGenFramebuffers( 1, &fbo );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
	fn->glDrawBuffer( GL_COLOR_ATTACHMENT0 );
	fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	const bool complete = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	QStringList drew;
	std::vector<float> px( size_t( n ) * size_t( n ) * 4 );
	if ( complete ) {
		out.n = n;
		out.rgb.assign( size_t( 6 ) * size_t( n ) * size_t( n ) * 3, 0.0f );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glDisable( GL_MULTISAMPLE );
		fn->glViewport( 0, 0, n, n );
		g_skyCapture = true;
		const SkyFrame f = skyFrame();
		for ( int face = 0; face < 6; face++ ) {
			float F[3], R[3], U[3];
			wwSkyPictureFace( face, F, R, U );
			auto & gu = *r->globalUniforms;
			for ( int c = 0; c < 3; c++ )
				gu.viewMatrix[c] = FloatVector4( R[c], U[c], -F[c], 0.0f );	// columns: the world axes in view space
			// 90 degrees, square: clip x = view x, y = view y, w = -view z (the sky passes park z themselves)
			const float zn = 1.0f, zf = 1.0e6f;
			gu.projectionMatrix[0] = FloatVector4( 1.0f, 0.0f, 0.0f, 0.0f );
			gu.projectionMatrix[1] = FloatVector4( 0.0f, 1.0f, 0.0f, 0.0f );
			gu.projectionMatrix[2] = FloatVector4( 0.0f, 0.0f, ( zf + zn ) / ( zn - zf ), -1.0f );
			gu.projectionMatrix[3] = FloatVector4( 0.0f, 0.0f, 2.0f * zf * zn / ( zn - zf ), 0.0f );
			r->setGlobalUniforms();
			// below the dome's rim nothing is drawn: no light (the view shows its clear colour there, not sky)
			if ( flatRgb )
				fn->glClearColor( flatRgb[0], flatRgb[1], flatRgb[2], 1.0f );
			else
				fn->glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
			fn->glClear( GL_COLOR_BUFFER_BIT );
			if ( !flatRgb ) {
				QStringList d;
				// lane SKYA (SKYGI1): the physical sky in place of the dome (its row and Physical Sky on; red "dome" keeps it)
				const bool phys = wwAtmoGiActive() && !wwAtmoGiRedDome() && drawAtmo( scene, d, true );
				if ( phys || drawDome( scene, f, 1.0f, d ) ) {
					drawStars( scene, f, d );
					if ( sx.clouds )
						drawClouds( scene, f, 1.0f, d );
				}
				if ( face == 0 )
					drew = d;
			}
			fn->glFinish();
			fn->glReadPixels( 0, 0, n, n, GL_RGBA, GL_FLOAT, px.data() );
			float * o = out.rgb.data() + size_t( face ) * size_t( n ) * size_t( n ) * 3;
			for ( size_t t = 0; t < size_t( n ) * size_t( n ); t++ )
				for ( int c = 0; c < 3; c++ )
					o[t * 3 + size_t( c )] = std::max( px[t * 4 + size_t( c )], 0.0f );
		}
		g_skyCapture = false;
	}
	// restore
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glDeleteFramebuffers( 1, &fbo );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
	fn->glDeleteTextures( 1, &tex );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glClearColor( clearWas[0], clearWas[1], clearWas[2], clearWas[3] );
	wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND );
	wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST );
	wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE );
	wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST );
	wasMs ? fn->glEnable( GL_MULTISAMPLE ) : fn->glDisable( GL_MULTISAMPLE );
	wasSrgb ? fn->glEnable( GL_FRAMEBUFFER_SRGB ) : fn->glDisable( GL_FRAMEBUFFER_SRGB );
	fn->glDepthMask( depthMask );
	*r->globalUniforms = guWas;
	r->setGlobalUniforms();
	if ( !complete )
		return fail( QStringLiteral( "the float target was refused (framebuffer incomplete)" ) );
	out.key = wwLookdevSkyPictureKey();
	out.drew = flatRgb ? QStringLiteral( "flat(%1,%2,%3)" ).arg( double( flatRgb[0] ) ).arg( double( flatRgb[1] ) )
		.arg( double( flatRgb[2] ) ) : drew.join( QChar( '|' ) );
	out.ms = double( clock.nsecsElapsed() ) / 1.0e6;
	if ( !flatRgb && !out.drew.contains( QStringLiteral( "sky:dome(" ) ) && !out.drew.contains( QStringLiteral( "sky:physical(" ) ) )
		return fail( QStringLiteral( "the dome did not draw (%1)" ).arg( out.drew ) );
	return true;
}
