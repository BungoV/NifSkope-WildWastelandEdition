/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lookdevstage.h"
#include "features/shared/cellpasstime.h"	// lane PERF2: the draws' CPU setup time
#include "gl/celluniprof.h"	// lane PERFFIX1: WW_CELL_UNI_PROF
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellworldanim.h"	// MODULES1: the cell world clock (nothing acts paused; red WW_CELL_CLOCK_RED=harness)

#include "features/weather/esmweather.h"
#include "features/air/cellvolair.h"	// lane VOLAIR1
#include "gl/celllights.h"
#include "features/exposure/cellhdr.h"	// lane LIGHTBOOTH1
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "gl/scenelighting.h"
#include "features/shadows/shadowset.h"	// lane TERRSHADOW1: the Sun Path row
#include "features/sky/atmosphere.h"	// lane ATMO1: the physical sky
#include "features/weather/weatherrecx.h"	// lane WX1: the weather .recx night block
#include "features/weather/weathergen.h"	// lane WEATHERRECX1: the generated rung's inputs
#include "features/clouds/cloudfusionlaw.h"	// MODULES1 10c: the 2D layers' scatter law
#include "features/clouds/cloudmap.h"	// lane CLOUD1: the cloud map
#include "features/clouds/cloudvol.h"	// lane CLOUD2: the volume clouds
#include "features/clouds/clouddeckmath.h"	// lane SKYLIGHT1: the deck's light on the ground
#include "features/clouds/cloudlightning.h"	// lane CLOUD6f: the storm share of the lightning
#include "features/sky/atmomath.h"	// lane SKYA (ATMOCHECK1): the weather tint table
#include "features/sky/skyephem.h"	// lane EPHEM1: the real moon's phase texture
#include "features/sky/skyplacerun.h"	// lane INTSKY1: the worldspace's place
#include "features/sky/skydisc.h"	// lane SKYDISC1: our moon globe's math
#include "features/sky/moonlight.h"	// lane FIX1101A (moon): the one key light round the clock
#include "gl/celllooks.h"	// lane SKYDISC1: the real-sky look decides the moon's frame
#include "features/shadows/sunshadow.h"
#include "model/nifmodel.h"

#include "gamemanager.h"
#include "harnesswindow.h"
#include "features/far-lod/lodgen.h"
#include "cellmodsource.h"	// lane SKYTOP1: the active load order
#include "cellstream.h"	// lane WEATHERBLEND6: the streamed camera's cell, for the region walk
#include "features/exposure/physunits.h"	// lane PHYSUNITS1: real units
#include "features/exposure/physcamera.h"	// MODULES1 R6-12: the camera's adaptation floor (the gameplay moon)
#include "features/weather/wxoverride.h"	// lane MODULES1 item 12: live weather overrides

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QDebug>
#include <QMap>
#include <QMutex>
#include <atomic>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

bool wwAtmoGiSkyAxes( const float sunTo[3], float out[6][3], float * trans = nullptr, float hKm = 0.002f );	// atmosphere.cpp

namespace
{

// lane SKYTOP1: the old key ("Lookdev Plugin", a plugin that defaulted to Fallout4.esm) is no longer read; the
// override key's empty value is "Load order"
const char * const kPluginKey = "Settings/Render/Scene/Lookdev Plugin Override";
const char * const kWeatherKey = "Settings/Render/Scene/Lookdev Weather";
const char * const kHourKey = "Settings/Render/Scene/Lookdev Hour";
const char * const kGroundKey = "Settings/Render/Scene/Lookdev Ground";
const char * const kSkyKey = "Settings/Render/Scene/Lookdev Sky";
const char * const kSunKey = "Settings/Render/Scene/Lookdev Sun";
const char * const kCloudsKey = "Settings/Render/Scene/Lookdev Clouds";
const char * const kMoonKey = "Settings/Render/Scene/Lookdev Moon";
const char * const kDayKey = "Settings/Render/Scene/Lookdev Game Day";
const char * const kFogKey = "Settings/Render/Scene/Lookdev Fog";
const char * const kShadowsKey = "Settings/Render/Scene/Lookdev Shadows2";	// lane DEFAULTS1: new key (ships on)

const char * const kDefaultCube = "textures/shared/cubemaps/mipblur_defaultoutside1.dds";
const char * const kGroundD = "textures/landscape/ground/commonwealthdefault01_d.dds";
const char * const kGroundN = "textures/landscape/ground/commonwealthdefault01_n.dds";
constexpr float kTile = 512.0f;		// research_lookdev_scene.md: UV tiled about every 512 units
constexpr float kGroundHalf = 4096.0f;	// an ~8192-unit quad

// lane SKYTOP1: the open cell's load order (set by the cell build, read by resolve)
QMutex cellMx;
QStringList cellOrder;
std::atomic<quint64> cellOrderGen { 0 };

QStringList cellOrderNow()
{
	QMutexLocker l( &cellMx );
	return cellOrder;
}

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
	quint64 cellSeen = 0;	// lane SKYTOP1: the cell load order generation the last load read
	QString loadSource;	// lane SKYTOP1: where the plugin list came from, for the echo
	qint64 loadMs = 0, loadHeld = 0, loadFiles = 0;
	quint64 gen = 1;	// bumped whenever resolve() reloads (lane PERF1: the per-shape memos below key on it)
	quint64 loadGen = 0;	// lane SKYLIVE1: bumped per plugin load; a key on the weather name alone misses a new source
	EsmWeather esm;
	QString loadRefusal;
	QVector<WwWeatherEntry> list;
	WwWeatherData w;
	bool haveWeather = false;
	QString weatherRefusal;
	QString orderFor;	// lane MODULES1 part 24: the weather that pulled the load order past the pinned plugins
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
	QElapsedTimer transClock;	// lane WEATHERBLEND1: the running weather change's wall clock
	qint64 transMs = -1;	// its reading at the last pct step
	std::shared_ptr<WwWeatherData> pending;	// lane WEATHERBLEND2: a pick made during a change waits for its end
	float accelFrom = -1.0f;	// the pct the change was accelerated at (fWeatherTransAccel), -1 = not
	bool interior = false;	// the open cell is an interior: a pick snaps
	// lane WEATHERBLEND3: the Sky flags the next pick meets (fast travel, wait, teleport), the waited hours inside the
	// change, its seconds at the last step, the last cell document, and each layer's scroll base (u, v)
	unsigned snapFlags = 0;
	QString snapLast;
	double waitHours = 0.0, changeSecs = 0.0;
	const void * lastDoc = nullptr;
	double cloudBase[32][2] = {};
	const void * regionDoc = nullptr;	// lane WEATHERBLEND4: a teleport's cell, its region weather picked at the next resolve
	QString regionLast;
	// lane WEATHERBLEND5: the region the last pick chose, and the current weather's form and start hour (its elapsed)
	quint32 regionForm = 0, startForm = 0;
	double startHour = 0.0;
	// lane WEATHERBLEND6: the region walk round a streamed camera, its clock, and the current region's EditorID
	WwRegionWalk walk;
	QElapsedTimer walkClock;
	QString regionName;
	QString skyLast = QStringLiteral( "pending" );	// what the last sky pass drew, for the echo

	// the weather fog (lane FOG1)
	bool fog = false, pinFog = false;
	float fogProbe[3] = { 0, 0, 0 };	// WW_LOOKDEV_FOGPROBE=d,z,mode
	QString fogLast = QStringLiteral( "pending" );	// what the last fogged draw uploaded, for the echo

	// the cascaded sun shadows (lane CSM1, gl/sunshadow.cpp)
	// lane DEFAULTS1: ships on (new key below so an old OFF does not stick); WW_DEFAULTS1_RED=1 = the old default, off
	bool shadows = wwEnvInt( "WW_DEFAULTS1_RED" ) != 1, pinShadows = false;

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
			if ( s.plugin != oldPlugin || s.pinPlugins != oldPins ) {
				s.dirtyLoad = true;
				s.orderFor.clear();
			}
		}
	}
	return s;
}

QString hex8( quint32 v )
{
	return QString( "%1" ).arg( v, 8, 16, QChar( '0' ) ).toUpper();
}

QStringList resolveList( const LdState & s, bool & groups, QString & source )
{
	groups = false;
	const QString data = EsmWeather::gameDataDir();
	auto inData = [&data]( const QStringList & names ) {
		QStringList files;
		for ( const QString & p0 : names ) {
			QString p = p0.trimmed();
			if ( !QFileInfo::exists( p ) && !data.isEmpty() && QFileInfo::exists( QDir( data ).filePath( p ) ) )
				p = QDir( data ).filePath( p );
			files << p;
		}
		return files;
	};
	if ( !s.pinPlugins.isEmpty() && s.orderFor.isEmpty() ) {
		source = QStringLiteral( "pinned" );
		return inData( s.pinPlugins );
	}
	QString plugin = s.orderFor.isEmpty() ? s.plugin : QString();
	/* lane SKYTOP1 (bungo 10-26: "the weather should be read from just any plugin"): no override = the load order,
	 * the last plugin's record wins. Red WW_LOOKDEV_RED=oldresolve: Fallout4.esm and its masters, as before. */
	if ( plugin.isEmpty() && !wwLookdevRed( "oldresolve" ) ) {
		QStringList order = cellOrderNow();
		QString from = QStringLiteral( "the cell" );
		if ( order.isEmpty() ) {
			const CellModSource m = cellModSource();
			order = m.plugins;
			from = m.label;
		}
		if ( !order.isEmpty() ) {
			groups = true;
			source = QString( "load order (%1, %2 plugins)" ).arg( from ).arg( order.size() );
			return inData( order );
		}
	}
	if ( plugin.isEmpty() && !data.isEmpty() )
		plugin = QDir( data ).filePath( QStringLiteral( "Fallout4.esm" ) );
	if ( plugin.isEmpty() )
		return {};
	source = s.plugin.isEmpty() ? QStringLiteral( "Fallout4.esm" ) : QFileInfo( plugin ).fileName();
	return EsmWeather::loadListFor( plugin, data );
}

//! lane SKYTOP1: one line per load naming the plugin whose version of each weather won
void loadCensus( const LdState & s )
{
	QMap<QString, int> per;
	QStringList each;
	for ( const WwWeatherEntry & e : s.list ) {
		per[e.srcFile]++;
		each << e.edid + QLatin1Char( '=' ) + e.srcFile;
	}
	QStringList counts;
	for ( auto it = per.cbegin(); it != per.cend(); ++it )
		counts << QString( "%1:%2" ).arg( it.key() ).arg( it.value() );
	qInfo().noquote() << QString( "lookdev census: source=%1 files=%2 weathers=%3 ms=%4 held=%5MB of %6MB; won %7; %8" )
		.arg( s.loadSource ).arg( s.esm.files().size() ).arg( s.list.size() ).arg( s.loadMs )
		.arg( double( s.loadHeld ) / 1048576.0, 0, 'f', 1 ).arg( double( s.loadFiles ) / 1048576.0, 0, 'f', 1 )
		.arg( counts.join( QChar( ',' ) ), each.join( QChar( ' ' ) ) );
}

/* lane WEATHERBLEND1: the weather change. WW_WEATHER_BLEND_RED=snap is the old instant swap. The clock is the wall clock
 * in a live window, WW_WEATHER_BLEND_TIME seconds since the change in a harness, else none (a harness snaps) */
bool weatherBlendSnap()
{
	WW_SWITCH_STATIC( bool, snap, wwEnvStr( "WW_WEATHER_BLEND_RED" ).split( ',' ).contains( QStringLiteral( "snap" ) ) );
	return snap;
}

/* lane WEATHERBLEND2: how a pick behaves. WW_WEATHER_PICK=force = the console's force-weather (snaps, cancels the
 * change); noaccel = a pick during a change does not accelerate it (Papyrus SetActive's abAccelerate false) */
bool weatherPickForce()
{
	WW_SWITCH_STATIC( bool, force, wwEnvStr( "WW_WEATHER_PICK" ).split( ',' ).contains( QStringLiteral( "force" ) ) );
	return force;
}

bool weatherPickAccel()
{
	WW_SWITCH_STATIC( bool, accel, !wwEnvStr( "WW_WEATHER_PICK" ).split( ',' ).contains( QStringLiteral( "noaccel" ) ) );
	return accel;
}

//! lane WEATHERBLEND3: WW_WEATHER_PICK=fasttravel = every pick carries the fast-travel flag (applied at once, snapped)
bool weatherPickFastTravel()
{
	WW_SWITCH_STATIC( bool, ft, wwEnvStr( "WW_WEATHER_PICK" ).split( ',' ).contains( QStringLiteral( "fasttravel" ) ) );
	return ft;
}

//! lane WEATHERBLEND3: WW_WEATHER_BLEND_RED=<v>, the old paths: nosnapevents, cloudtwo, windbytes, nowindoffset
bool weatherBlendRed( const char * v )
{
	WW_SWITCH_STATIC( QStringList, red, wwEnvStr( "WW_WEATHER_BLEND_RED" ).split( ',' ) );
	return red.contains( QLatin1String( v ) );
}

//! lane WEATHERBLEND3: layer i's speed on axis 0 (u) / 1 (v) in weather W (a layer past its count reads layer 0's)
float layerSpeed( const LdState & s, const WwWeatherData & W, int i, int axis )
{
	const int li = ( quint32( i ) >= W.cloudLayers ) ? 0 : i;
	return EsmWeather::cloudSpeed( axis ? W.cloudSpeedY[li] : W.cloudSpeedX[li], s.gmst );
}

//! layer i's ONE scroll (the game's): base + old speed + (new - old) x the pct integral; red cloudtwo = W's own
float cloudScroll( const LdState & s, const WwWeatherData & W, int i, int axis, double secs )
{
	if ( weatherBlendRed( "cloudtwo" ) )
		return wwCloudOffset( layerSpeed( s, W, i, axis ), secs );
	const float vNew = layerSpeed( s, s.w, i, axis );
	const float vOld = s.w.blending() ? layerSpeed( s, *s.w.last, i, axis ) : vNew;
	const double ps = s.w.blending()
		? wwWeatherTransPctSeconds( s.changeSecs, s.waitHours, s.w.transDelta, s.gmst, s.accelFrom ) : 0.0;
	return wwCloudScroll( ( i >= 0 && i < 32 ) ? s.cloudBase[i][axis] : 0.0, vOld, vNew, secs, ps );
}

//! the change ends: each base takes the scroll shown, so the new speed carries on from it (no clock: base 0)
void cloudBaseEnd( LdState & s, bool clock )
{
	const double secs = wwLookdevCloudSeconds();
	for ( int i = 0; i < 32; i++ )
		for ( int a = 0; a < 2; a++ ) {
			const float vNew = layerSpeed( s, s.w, i, a );
			const double b = ( clock && s.w.blending() )
				? double( cloudScroll( s, s.w, i, a, secs ) ) - double( wwCloudScroll( 0.0, vNew, vNew, secs, 0.0 ) ) : 0.0;
			s.cloudBase[i][a] = b - std::floor( b );
		}
}

//! seconds into the change; -1 = no clock (snap), -2 = still inside the last 15 ms step (no new pct)
double transSeconds( LdState & s )
{
	WW_SWITCH_STATIC( double, pin, wwEnvStr( "WW_WEATHER_BLEND_TIME" ).isEmpty() ? -1.0
		: std::max( 0.0, wwEnvStr( "WW_WEATHER_BLEND_TIME" ).toDouble() ) );
	if ( pin >= 0.0 )
		return pin;
	if ( !wwCellClockLive() )
		return -1.0;
	const qint64 ms = s.transClock.elapsed();
	if ( s.transMs >= 0 && ms - s.transMs < 15 )	// about one step a frame: every memo keyed on gen rebuilds per step
		return -2.0;
	s.transMs = ms;
	return double( ms ) / 1000.0;
}

//! the pct now; a new pct bumps gen (every memo rebuilds), the end drops the weather left behind
void transTick( LdState & s )
{
	if ( !s.w.last )
		return;
	const double secs = weatherBlendSnap() ? -1.0 : transSeconds( s );
	if ( secs == -2.0 )
		return;
	// lane WEATHERBLEND3: a wait inside the change (PassTime) adds its game hours
	const float raw = secs < 0.0 ? 1.0f
		: wwWeatherTransPct( wwWeatherTransGameHours( secs ) + s.waitHours, s.w.transDelta, s.gmst );
	if ( secs >= 0.0 )
		s.changeSecs = secs;
	const float p = wwWeatherTransAccelPct( raw, s.accelFrom, s.gmst );
	if ( p >= 1.0f ) {
		if ( !weatherBlendRed( "cloudtwo" ) )
			cloudBaseEnd( s, secs >= 0.0 );
		s.w.last.reset();
		s.w.transPct = 1.0f;
		s.accelFrom = -1.0f;
		s.gen++;
		if ( s.pending ) {	// lane WEATHERBLEND2: the waiting pick starts its change now (Sky::UpdateWeather)
			auto prev = std::make_shared<WwWeatherData>( s.w );
			s.w = *s.pending;
			s.pending.reset();
			s.w.last = prev;
			s.w.transPct = 0.0f;
			s.transClock.start();
			s.transMs = -1;
			s.waitHours = s.changeSecs = 0.0;
		}
	} else if ( p != s.w.transPct ) {
		s.w.transPct = p;
		s.gen++;
	}
}

// lane WEATHERBLEND5: the current weather's start hour (Sky's elapsed reads it), noted when the weather in use changes
void noteWeatherStart( LdState & s )
{
	if ( s.haveWeather && s.w.formID != s.startForm ) {
		s.startForm = s.w.formID;
		s.startHour = s.hour;
	}
}

//! the region's weather becomes the weather key (the region's, not the user's pick: not saved); its key
QString regionApply( LdState & s, quint32 pick )
{
	QString key = hex8( pick );
	for ( const WwWeatherEntry & e : s.list )
		if ( e.formID == pick && !e.edid.isEmpty() )
			key = e.edid;
	if ( key != s.weatherKey ) {
		s.weatherKey = key;
		s.dirtyWeather = true;
	}
	return key;
}

/* lane WEATHERBLEND4: a teleport to an exterior picks the region weather (Sky::UpdateWeather on a new region): of the
 * cell's regions (XCLR) with weather data whose areas hold the player, the override / priority winner; then its list's
 * chance draw, seeded by the cell (a reopen repeats). Red WW_WEATHER_BLEND_RED=noregionpick.
 * WEATHERBLEND5: the spawn eye (red regioncentre: the cell's middle); deleted / other-world / no-area regions skipped
 * (reds noregionskip, noareainside); the current weather kept by its duration and elapsed hours (red nodurationkeep) */
void regionPick( LdState & s )
{
	const void * doc = s.regionDoc;
	s.regionDoc = nullptr;
	const WwCellLighting * cl = doc ? wwCellLightsFor( doc ) : nullptr;
	if ( !cl || cl->interior || s.pinWeather || weatherBlendRed( "noregionpick" ) )
		return;
	noteWeatherStart( s );
	const bool centre = !cl->regionEyeOk || weatherBlendRed( "regioncentre" );
	const float px = centre ? cl->regionAt[0] : cl->regionEye[0], py = centre ? cl->regionAt[1] : cl->regionEye[1];
	const int red = ( weatherBlendRed( "noareainside" ) ? int( WwRegionRedNoAreaInside ) : 0 )
		| ( weatherBlendRed( "noregionskip" ) ? int( WwRegionRedNoSkip ) : 0 );
	QVector<WwRegionWeather> in;
	QVector<WwRegionRank> rank;
	for ( quint32 regn : cl->regions ) {
		WwRegionWeather g;
		if ( s.esm.regionWeatherAt( regn, px, py, g ) && wwRegionCounts( g, cl->regionWorld, red ) ) {
			in.append( g );
			rank.append( g.rank );
		}
	}
	const int k = wwRegionChoose( rank.constData(), int( rank.size() ) );
	if ( k < 0 ) {
		s.regionLast = QStringLiteral( "none(%1 regions)" ).arg( cl->regions.size() );
		s.regionForm = 0;
		s.regionName = QStringLiteral( "none" );	// lane WEATHERBLEND6
		return;
	}
	const WwRegionWeather & g = in.at( k );
	const bool same = g.form == s.regionForm;
	s.regionForm = g.form;
	s.regionName = g.edid;	// lane WEATHERBLEND6
	const quint32 cur = s.haveWeather ? s.w.formID : 0u;
	const quint32 draw = wwRegionDraw( cl->regionSeed );
	const double dur = wwWeatherDurationHours( s.clim.tnam[4], s.haveWeather ? s.w.volMult : 1.0f, s.gmst );
	const double el = wwWeatherElapsedHours( s.hour, s.startHour );
	quint32 pick = 0;
	if ( weatherBlendRed( "nodurationkeep" ) ) {	// the WEATHERBLEND4 rule: a listed current weather is kept outright
		const bool usesCur = cur && std::any_of( g.list.cbegin(), g.list.cend(),
			[&]( const WwRegionWeatherEntry & e ) { return e.weather == cur; } );
		pick = usesCur ? cur : wwRegionWeatherPick( g.list.constData(), int( g.list.size() ), draw );
	} else {
		pick = wwRegionWeatherFollow( g.list.constData(), int( g.list.size() ), cur, same, el, dur, draw,
			double( wwRegionDraw( cl->regionSeed ^ 0x9e3779b9u ) ) / 4294967296.0 );
	}
	const QString law = QStringLiteral( "(%1 el=%2 dur=%3)" ).arg( same ? QStringLiteral( "same" ) : QStringLiteral( "new" ) )
		.arg( el, 0, 'f', 2 ).arg( dur, 0, 'f', 2 );
	if ( !pick || pick == cur ) {
		s.regionLast = QStringLiteral( "%1:%2%3" ).arg( g.edid, pick ? QStringLiteral( "keep" ) : QStringLiteral( "empty" ), law );
		return;
	}
	s.regionLast = QStringLiteral( "%1:%2%3" ).arg( g.edid, regionApply( s, pick ), law );
}

//! lane WEATHERBLEND6: a region weather's name (its EditorID when the list has one)
QString weatherName( const LdState & s, quint32 form )
{
	if ( !form )
		return QStringLiteral( "none" );
	for ( const WwWeatherEntry & e : s.list )
		if ( e.formID == form && !e.edid.isEmpty() )
			return e.edid;
	return hex8( form );
}

/* lane WEATHERBLEND6: the region walk. A streamed exterior's camera moves like the player and cells load round it;
 * when its cell or the regions counting under it change (sampled at most every kWwRegionWalkMs, only after a move,
 * so the areas are never tested per frame), the region choice runs again by regionPick's rules (the camera's eye,
 * the skips, override / priority) and on entering a new region the follow law keeps the current weather when
 * u01 > elapsed / duration, else that region's draw starts its weather through the weather change (a pick during
 * a change waits for its end). The first sample after an open or a teleport is the baseline. A stderr line and the
 * echo's region= on each re-pick. Red WW_CELL_REGION_RED=spawnonly: the teleport's pick only (WEATHERBLEND5) */
void regionWalk( LdState & s, const NifModel * doc )
{
	if ( s.regionDoc || s.dirtyLoad || !s.loadRefusal.isEmpty() || s.pinWeather || weatherBlendRed( "noregionpick" ) )
		return;
	const CellStreamCamera * cam = cellStreamCamera( doc );
	const WwCellLighting * cl = cam ? wwCellLightsFor( doc ) : nullptr;
	if ( !cl || cl->interior )
		return;
	if ( !s.walkClock.isValid() )
		s.walkClock.start();
	const qint64 now = s.walkClock.elapsed();
	if ( !wwRegionWalkDue( s.walk, now, cam->cx, cam->cy, cam->eye ) )
		return;
	const int red = ( weatherBlendRed( "noareainside" ) ? int( WwRegionRedNoAreaInside ) : 0 )
		| ( weatherBlendRed( "noregionskip" ) ? int( WwRegionRedNoSkip ) : 0 );
	QVector<WwRegionWeather> in;
	QVector<WwRegionRank> rank;
	QVector<quint32> under;
	for ( quint32 regn : cam->regions ) {
		WwRegionWeather g;
		if ( s.esm.regionWeatherAt( regn, cam->eye[0], cam->eye[1], g ) && wwRegionCounts( g, cam->world, red ) ) {
			in.append( g );
			rank.append( g.rank );
			under.append( g.form );
		}
	}
	const int k = wwRegionChoose( rank.constData(), int( rank.size() ) );
	WW_SWITCH_STATIC( int, walkRed, wwEnvStr( "WW_CELL_REGION_RED" ).contains( QLatin1String( "spawnonly" ) )
		? int( WwRegionWalkRedSpawnOnly ) : 0 );
	const bool first = !s.walk.started;
	const quint32 wasRegion = s.walk.region;
	if ( !wwRegionWalkSample( s.walk, now, cam->cx, cam->cy, cam->eye, under, k < 0 ? 0u : in.at( k ).form, walkRed ) ) {
		if ( first && s.regionName.isEmpty() && k >= 0 )
			s.regionName = in.at( k ).edid;	// the region the open stands in, for the Weather box
		return;
	}
	noteWeatherStart( s );
	const QString from = !s.regionName.isEmpty() ? s.regionName : wasRegion ? hex8( wasRegion ) : QStringLiteral( "none" );
	s.regionForm = s.walk.region;
	s.regionName = k < 0 ? QStringLiteral( "none" ) : in.at( k ).edid;
	const quint32 cur = s.haveWeather ? s.w.formID : 0u;
	quint32 pick = 0;
	QString why = QStringLiteral( "no region counts under the camera: the weather stays" );
	if ( k >= 0 ) {
		const WwRegionWeather & g = in.at( k );
		const double dur = wwWeatherDurationHours( s.clim.tnam[4], s.haveWeather ? s.w.volMult : 1.0f, s.gmst );
		const double el = wwWeatherElapsedHours( s.hour, s.startHour );
		const quint32 draw = wwRegionDraw( cam->cellForm );	// seeded by the cell the border is crossed in
		if ( weatherBlendRed( "nodurationkeep" ) ) {	// regionPick's red: a listed current weather is kept outright
			const bool usesCur = cur && std::any_of( g.list.cbegin(), g.list.cend(),
				[&]( const WwRegionWeatherEntry & e ) { return e.weather == cur; } );
			pick = usesCur ? cur : wwRegionWeatherPick( g.list.constData(), int( g.list.size() ), draw );
			why = QStringLiteral( "red nodurationkeep" );
		} else {
			pick = wwRegionWalkFollow( g.list.constData(), int( g.list.size() ), cur, el, dur, draw,
				double( wwRegionDraw( cam->cellForm ^ 0x9e3779b9u ) ) / 4294967296.0, &why );
		}
	}
	const bool changed = pick && pick != cur;
	const QString key = changed ? regionApply( s, pick ) : weatherName( s, cur );
	s.regionLast = QStringLiteral( "%1:%2(walk %3->%4 %5)" ).arg( s.regionName, changed ? key : QStringLiteral( "keep" ), from,
		s.regionName, changed ? QStringLiteral( "changed" ) : QStringLiteral( "kept" ) );
	std::fprintf( stderr, "lookdev region walk: cell %d,%d region %s -> %s, weather %s %s%s: %s\n", cam->cx, cam->cy,
		qPrintable( from ), qPrintable( s.regionName ), changed ? "changed" : "kept", qPrintable( weatherName( s, cur ) ),
		changed ? qPrintable( QStringLiteral( " -> " ) + key ) : "", qPrintable( why ) );
}

/* lane MODULES1 part 24 (bungo's coast black sky): a weather missing from the pinned plugins or the chosen plugin is read
 * from the load order, as SKYTOP1's "just any plugin" rule. Red ^WW_LOOKDEV_FINDRED=1: refused, the grey dome. */
bool lookdevFindRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_LOOKDEV_FINDRED" ).trimmed() == QLatin1StringView( "1" ) );
	return red;
}

void resolve()
{
	LdState & s = st();
	if ( !s.orderFor.isEmpty() && s.orderFor.compare( s.weatherKey, Qt::CaseInsensitive ) != 0 ) {
		s.orderFor.clear();	// another weather: back to the pinned plugins
		s.dirtyLoad = true;
	}
	// lane SKYTOP1: a cell opened with another load order reloads the weather (only while the row is "Load order")
	if ( cellOrderGen.load() != s.cellSeen ) {
		s.cellSeen = cellOrderGen.load();
		if ( ( s.pinPlugins.isEmpty() && s.plugin.isEmpty() ) || !s.orderFor.isEmpty() )
			s.dirtyLoad = true;
	}
	const bool newLoad = s.dirtyLoad;	// lane WEATHERBLEND1: another load snaps (its records are others)
	if ( s.dirtyLoad || s.dirtyWeather )
		s.gen++;
	if ( s.dirtyLoad ) {
		s.dirtyLoad = false;
		s.dirtyWeather = true;
		s.loadGen++;
		s.list.clear();
		bool groups = false;
		QElapsedTimer lt;
		lt.start();
		const QStringList files = resolveList( s, groups, s.loadSource );
		if ( files.isEmpty() ) {
			s.loadRefusal = QStringLiteral( "no Fallout 4 Data folder (set the game path)" );
		} else if ( !( groups ? s.esm.loadGroups( files, &s.loadRefusal ) : s.esm.load( files, &s.loadRefusal ) ) ) {
			if ( s.loadRefusal.isEmpty() )
				s.loadRefusal = QStringLiteral( "the plugins did not load" );
		} else {
			s.loadRefusal.clear();
			s.loadMs = lt.elapsed();
			s.loadHeld = groups ? s.esm.groupBytes() : 0;
			s.loadFiles = groups ? s.esm.fileBytes() : 0;
			s.list = s.esm.list();
			if ( groups )
				loadCensus( s );
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
	noteWeatherStart( s );	// lane WEATHERBLEND5
	if ( s.regionDoc && s.loadRefusal.isEmpty() ) {
		regionPick( s );
		s.walk = WwRegionWalk();	// lane WEATHERBLEND6: the walk starts again from the teleport's eye
	}
	if ( s.dirtyWeather ) {
		s.dirtyWeather = false;
		// lane WEATHERBLEND1: a change of weather keeps the one it leaves; a new load, a refusal or the snap red snaps
		std::shared_ptr<WwWeatherData> prev;
		if ( s.haveWeather && !newLoad && !weatherBlendSnap() )
			prev = std::make_shared<WwWeatherData>( s.w );
		s.haveWeather = false;
		s.weatherRefusal.clear();
		if ( !s.loadRefusal.isEmpty() ) {
			s.weatherRefusal = s.loadRefusal;
		} else {
			const quint32 id = s.esm.find( s.weatherKey );
			QString why;
			if ( !id && s.orderFor.isEmpty() && !lookdevFindRed() && !s.loadSource.startsWith( QLatin1StringView( "load order" ) ) ) {
				const QString was = s.loadSource;
				s.orderFor = s.weatherKey;
				s.dirtyLoad = true;
				resolve();
				std::fprintf( stderr, "lookdev find: weather %s not in the %s plugins -> %s from %s\n", qPrintable( s.orderFor ),
					qPrintable( was ), s.haveWeather ? "read" : "refused", qPrintable( s.loadSource ) );
				return;
			}
			if ( !id )
				s.weatherRefusal = QString( "weather %1 not in the loaded plugins" ).arg( s.weatherKey );
			else if ( !s.esm.read( id, s.w, &why ) )
				s.weatherRefusal = why;
			else
				s.haveWeather = true;
		}
		// lane WEATHERBLEND2: the game's pick rule: force-weather and interiors snap, a change keeps a new pick waiting
		int rule = ( prev && s.haveWeather )
			? wwWeatherPickRule( prev->blending(), prev->formID == s.w.formID, s.interior, weatherPickForce() ) : WwPickSnap;
		// lane WEATHERBLEND3: fast travel applies a waiting pick at once; it, a wait or a teleport snap a new change
		const unsigned flags = s.snapFlags | ( weatherPickFastTravel() ? unsigned( WwSnapFastTravel ) : 0u );
		if ( prev && s.haveWeather && flags && !weatherBlendRed( "nosnapevents" ) ) {
			const int was = rule;
			rule = wwWeatherSnapRule( rule, flags );
			if ( rule != was )
				s.snapLast = QStringLiteral( "%1:%2" )
					.arg( ( flags & WwSnapFastTravel ) ? QStringLiteral( "fasttravel" ) : QStringLiteral( "passtime" ),
						was == WwPickDefer ? QStringLiteral( "defer" ) : QStringLiteral( "start" ) );
		}
		if ( rule == WwPickKeep ) {	// the same weather read again: a running change goes on
			s.w.last = prev->last;
			s.w.transPct = prev->transPct;
			s.pending.reset();
		} else if ( rule == WwPickDefer ) {
			s.pending = std::make_shared<WwWeatherData>( s.w );
			s.w = *prev;
		} else if ( rule == WwPickStart ) {
			prev->last.reset();
			prev->transPct = 1.0f;
			s.w.last = prev;
			s.w.transPct = 0.0f;
			s.transClock.start();
			s.transMs = -1;
			s.waitHours = s.changeSecs = 0.0;
		} else if ( rule == WwPickSnap ) {	// lane WEATHERBLEND3: a snap drops the change's clock and the scroll bases
			s.waitHours = s.changeSecs = 0.0;
			std::fill( &s.cloudBase[0][0], &s.cloudBase[0][0] + 64, 0.0 );
		}
		if ( ( rule == WwPickKeep || rule == WwPickDefer ) && s.w.blending() ) {
			if ( s.accelFrom < 0.0f && weatherPickAccel() )	// the pick hurries the running change (SetWeatherExternal)
				s.accelFrom = s.w.transPct;
		} else if ( rule != WwPickKeep ) {
			s.pending.reset();
			s.accelFrom = -1.0f;
		}
	}
	s.snapFlags = 0;	// lane WEATHERBLEND3: the game clears the flags at the end of each weather update
	transTick( s );
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
	float physKey = 1.0f;	// lane PHYSUNITS1: the real key over the weather's (1 = the old scale)
	float physFog = 1.0f;	// lane PHYSUNITS1: the real light over the weather's, sun + ambient (the fog's colours take it)
	float nightW = 0.0f;	// MODULES1 R7: the night share the record's night sky is drawn by (0 = day, or the red)
	float skyGame[3] = { 0.0f, 0.0f, 0.0f };	// MODULES1 R7: master On: the record's sky x Sky Scale x the key's unit x nightW, cd/m2
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

/* lane FIX1101A (moon): WW_MOON_RED (live) brings the old night back: phase8 = the vanilla 8-step phase on our globe
 * and its light, handover = the old key switch at the engine's D and the light slerped to the engine's arc after the
 * real sunset (the ghost sun), vanilla = both */
bool wwMoonRed( const char * word )
{
	const QString v = wwEnvStr( "WW_MOON_RED" ).trimmed().toLower();
	return v == QLatin1StringView( "vanilla" ) || v == QLatin1StringView( word );
}

/* MODULES1 R7: WW_NIGHTSKY_RED=physical (live): the night sky without the weather record's level (computeLight) */
bool wwNightSkyRed()
{
	return wwEnvStr( "WW_NIGHTSKY_RED" ).trimmed().toLower() == QLatin1StringView( "physical" );
}

/* MODULES1 R7 item 5 (the white far tower pink with the sun BEHIND it). The weather names its six ambient colours by
 * the light's travel: a face looking up reads Z- (index 5), a face looking east reads X- (every reader: celllights,
 * probesky, lookdev_ground). The physical sky's axes are named by the face (index 4 = a face looking up), and went in
 * unturned: every face took the sky of the opposite side (a wall facing away from a low sun took the sun side's sky,
 * the ground 15 % of its sky, a ceiling all of it). Red WW_PHYSUNITS_AXES_RED=1 (live): unturned, as before. */
/* MODULES1 R7-4 (level strips in the fog): the air tail's source from a window of the grid, not its last slice alone
 * (lookdev_fog.glsl wwAirTailJ). Red WW_CELL_AIR_TAILJ_RED=1 (live, fogRed 8): the last slice, as before. */
bool wwAirTailJRed()
{
	return wwEnvStr( "WW_CELL_AIR_TAILJ_RED" ).trimmed() == QLatin1StringView( "1" );
}

/* MODULES1 R7 night sky d: the saturated air's night colour (lookdev_fog.glsl fogNight) and the record dome's own cover
 * (cell_volsky.frag volSkyDome). Red WW_NIGHTAIR_RED=1 (live): neither, the air's own light alone, as before. */
bool wwNightAirRed()
{
	return wwEnvStr( "WW_NIGHTAIR_RED" ).trimmed() == QLatin1StringView( "1" );
}

/* FARWATER1 late item 3 (bungo b71: the night haze grey, brown between a blue sky and blue water): the air's light at
 * night takes the drawn night sky's horizon tint (lookdev_fog.glsl wwAirNightHue, weighted by the night share, so
 * twilight blends into it). Red WW_NIGHTAIR_RED=neutral (live): the neutral lobes' grey as before. */
bool wwNightHazeRed()
{
	return wwEnvStr( "WW_NIGHTAIR_RED" ).trimmed() == QLatin1StringView( "neutral" );
}

namespace {
float g_nightHaze[4] = { 0.0f, 0.0f, 0.0f, 0.0f };	// the tint (the sky's horizon colour) and the night share
}

QString wwNightHazeNote()
{
	const float * h = g_nightHaze;
	if ( h[3] <= 0.0f )
		return QStringLiteral( " night haze=off (night share 0)" );
	if ( wwNightHazeRed() )
		return QStringLiteral( " night haze=RED neutral (WW_NIGHTAIR_RED=neutral: the lobes' grey)" );
	return QStringLiteral( " night haze=%1,%2,%3 R/B %4 share %5 (the drawn night sky's horizon tint at the air's own "
		"luminance; red WW_NIGHTAIR_RED=neutral)" ).arg( double( h[0] ), 0, 'g', 3 ).arg( double( h[1] ), 0, 'g', 3 )
		.arg( double( h[2] ), 0, 'g', 3 ).arg( h[2] > 0.0f ? double( h[0] / h[2] ) : 0.0, 0, 'f', 2 ).arg( double( h[3] ), 0, 'f', 2 );
}

/* MODULES1 R7 item 6 (the 1 px bright row at eye level, bungo b61): the sky's level ray through the air ends like the
 * rows beside it (lookdev_fog.glsl wwVolAirSky). Red WW_CELL_AIR_LEVELRAY_RED=1 (live, fogRed 16): the old branch. */
bool wwAirLevelRayRed()
{
	return wwEnvStr( "WW_CELL_AIR_LEVELRAY_RED" ).trimmed() == QLatin1StringView( "1" );
}

bool wwPhysAxesRed()
{
	return wwEnvStr( "WW_PHYSUNITS_AXES_RED" ).trimmed() == QLatin1StringView( "1" );
}

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
		g.P[1] = std::cos( wwSkyPlaceNow().latDeg * wwskydisc::kRad );	// lane INTSKY1: the place's latitude
		g.P[2] = std::sin( wwSkyPlaceNow().latDeg * wwskydisc::kRad );
		const int p = std::max( 0, wwMoonPhase( s.gameDay, s.clim.tnam[5] ) );
		double elong = 180.0 + 45.0 * p;
		if ( !wwMoonRed( "phase8" ) ) {
			/* lane FIX1101A (moon, LATE ITEMs 24c / 24d): the phase from the ephemeris elongation on the game's date and
			 * hour (skyephem.h, the EPHEM1 calendar: 2287-10-23 + the day), continuous, as the real sky's; the vanilla
			 * 8-step phase (45 degree steps at midnight every 4 days) only as the red phase8 */
			const wwephem::Date d = wwephem::dateFromGameDay( s.gameDay, s.hour );
			elong = wwephem::elongationDeg( wwephem::julianDayLmt( d, wwephem::kBostonLon ) );
		}
		wwskydisc::sunFromElongation( g.D, g.P, elong, g.S );
	}
	if ( wwAtmoDiscRed( "nophase" ) )
		for ( int k = 0; k < 3; k++ )
			g.S[k] = -g.D[k];
	g.alphaDeg = wwskydisc::phaseAngleDeg( g.D, g.S );
	return g;
}

/* MODULES1 M6 (bungo 11-10, sun_glare_offset_0630 / moon_above_flare_0630): one direction per body. The sun = the
 * physical sky's disc (Sun Path Real: the ephemeris sun; else the engine's arc, mirrored under the horizon at the
 * engine's night); its lens takes it, never the key light (the moon's, or the 0..-6 degree twilight slerp to the
 * engine's night point). The game moon = the engine's own night point (the raw clock), never the disc Sun Path Real
 * moved onto the sun. Red WW_SKY_BODYDIR_RED=1: the lens on the key light and the moon on the moved disc, as before. */
bool wwSkyBodyDirRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_SKY_BODYDIR_RED" ).trimmed() == QLatin1StringView( "1" ) );
	return red;
}

/* MODULES1 M7 (bungo 11-10: 7:00 the whole sky lights up, 6:30 the ground below the horizon blown white). Under Sun
 * Path Real the light kept two engine-clock inputs: the night share (1 until the engine's sunrise, so at 7:00 with the
 * real sun at +1.2 degrees the land took the moon's key, 2 lux level, under a drawn sunrise sky and the meter opened
 * ~3 EV) and the sun's sky from the disc (still the engine's night point while the real sun is down: at 6:30 a 21-degree
 * day sky, 3395 lux level, at real -4 degrees). Both now follow the real sun. Red WW_SKY_TWILIGHT_RED=1. */
bool wwSkyTwilightRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_SKY_TWILIGHT_RED" ).trimmed() == QLatin1StringView( "1" ) );
	return red;
}

void sunBodyDir( const LdState & s, float out[3] )
{
	const WwSkyClock raw = wwSkyClock( s.hour, s.tnam, s.gmst );
	if ( wwSunPathMode() == 1 ) {
		double r[3];
		wwSunPathReal( s.hour, s.gameDay, r );
		for ( int c = 0; c < 3; c++ )
			out[c] = float( r[c] );
	} else {
		// the engine's arc; at night its point is the moon's, so the sun is that point mirrored under the horizon
		for ( int c = 0; c < 3; c++ )
			out[c] = raw.sunPos[c];
		if ( s.hour < raw.A || s.hour > raw.D )
			out[2] = -raw.sunPos[2];
	}
	const float l = std::sqrt( out[0] * out[0] + out[1] * out[1] + out[2] * out[2] );
	for ( int c = 0; c < 3; c++ )
		out[c] = l > 1e-9f ? out[c] / l : ( c == 2 ? 1.0f : 0.0f );
}

//! the phase angle the moonlight follows (the drawn globe's), memoised: the light asks once per shape
float moonAlphaNow( const LdState & s )
{
	static double hour = -1.0, day = -1.0;
	static int moons = -1, look = -1, red = -1;
	static WwSunPathKey path;
	static float memo = 0.0f;
	const WwSunPathKey pk = wwSunPathKey();
	const int lk = wwLookOn( WwLook::Ephemeris ) ? 1 : 0, rd = ( wwAtmoDiscRed( "nophase" ) ? 1 : 0 ) + ( wwMoonRed( "phase8" ) ? 2 : 0 );
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
	const float phase = wwAtmoDiscs() ? wwAtmoMoonFactorDeg( moonAlphaNow( s ) ) : wwAtmoMoonFactor( wwMoonPhase( s.gameDay, s.clim.tnam[5] ) );
	/* lane INTSKY1 (LATE ITEM 27f): with the real moon (its place and date, gl/skyplacerun.h) a moon under the horizon
	 * lights nothing: its light fades over its last 3.5 degrees (refraction and the disc's half width at -0.5); the
	 * air on its way (its elevation) is wwAtmoKeyLight's, which reads the light's direction, now the moon's. Red
	 * WW_MOONLIGHT_RED=engine: the phase alone, as before */
	double m[3];
	if ( wwAtmoNightOn() && wwEnvStr( "WW_MOONLIGHT_RED" ).trimmed() != QLatin1String( "engine" )
		&& wwSkyMoonReal( s.hour, s.gameDay, m, nullptr, nullptr ) ) {
		const double el = std::asin( std::clamp( m[2], -1.0, 1.0 ) ) * 180.0 / 3.14159265358979323846;
		return phase * float( std::clamp( ( el + 0.5 ) / 3.5, 0.0, 1.0 ) );
	}
	return phase;
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
	static unsigned long long wxStamp = 0;	// lane WX1: a weather .recx value (visibility, moonlight) moves the light
	static QString moonRed;	// lane FIX1101A (moon): WW_MOON_RED, live
	static bool physSky = false;	// lane PHYSUNITS1: real units while the cell's HDR camera draws
	static unsigned long long wxOv = 0;	// lane MODULES1 item 12: a live sun / ambient override
	static LdLight memo;
	// lane SKYA (NIGHT1): the Moon Phase and Stars row with the moon's phase (the game day moves it)
	int atmoKey = wwAtmoSunKey() + 3 * 16 * wwAtmoNightKey( wwMoonPhase( s.gameDay, s.clim.tnam[5] ) );
	if ( wwAtmoDiscs() && wwAtmoNightOn() )	// lane SKYDISC1: the light follows our globe's phase angle (0.1 deg steps)
		atmoKey += 4096 * ( 1 + int( moonAlphaNow( s ) * 10.0f ) );
	const WwSunPathKey sunKey = wwSunPathKey();
	const unsigned long long wx = wwWeatherRecx().stamp;
	// MODULES1 R6-12: the gameplay moon's pin moves the light too (live)
	const QString mr = wwEnvStr( "WW_MOON_RED" ).trimmed().toLower() + QLatin1Char( '|' ) + wwEnvStr( "WW_MOON_GAME" ).trimmed()
		+ QLatin1Char( '|' ) + wwEnvStr( "WW_NIGHTSKY_RED" ).trimmed().toLower()
		+ QLatin1Char( '|' ) + wwEnvStr( "WW_NIGHTSKY_STOPS" ).trimmed()
		+ QLatin1Char( '|' ) + wwEnvStr( "WW_PHYSUNITS_AXES_RED" ).trimmed();
	if ( gen != s.gen || hour != s.hour || sun != s.sun || atmoSun != atmoKey || !( sunPath == sunKey ) || wxStamp != wx
		|| mr != moonRed || physSky != wwPhysSkyOn() || wxOv != wwWxOvStamp() ) {
		physSky = wwPhysSkyOn();
		memo = computeLight();
		wxOv = wwWxOvStamp();
		wwWxOv3( WwWx::SunColor, memo.sun );
		float amb[3] = { memo.amb[0], memo.amb[1], memo.amb[2] };
		wwWxOv3( WwWx::AmbientColor, amb );
		for ( int c = 0; c < 3; c++ ) {	// the six axes keep their shape, scaled to the pinned mean
			const float k = memo.amb[c] > 1e-9f ? amb[c] / memo.amb[c] : 1.0f;
			for ( int a = 0; a < 6; a++ )
				memo.dalc[a][c] = memo.amb[c] > 1e-9f ? memo.dalc[a][c] * k : amb[c];
			memo.amb[c] = amb[c];
		}
		moonRed = mr;
		wxStamp = wx;
		gen = s.gen;
		hour = s.hour;
		sun = s.sun;
		atmoSun = atmoKey;
		sunPath = sunKey;
	}
	return memo;
}

/* lane PHYSUNITS1: the light in real units. The key came through the air from the top of the atmosphere (airKey), else
 * the weather's key x rKey (the real 45-degree key over the weather's). The ambient is the physical sky's irradiance on
 * the six axes at its absolute level (GITRUTH1 giSkyAbsolute: k = the key over its transmittance), the sun's sky added
 * while the key is the moon, the ground's light on -Z from albedo 0.15, and the dark night sky's floor on every axis;
 * without the physical sky the weather's ambient x rKey. Effect Lighting and the fog's colours take the ratio of the
 * whole light (sun + ambient) to the weather's. */
void physFinish( LdLight & L, float rKey, bool airKey )
{
	auto lumOf = []( const float v[3] ) { return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2]; };
	if ( !airKey )
		for ( int c = 0; c < 3; c++ )
			L.sun[c] *= rKey;
	const float ambOld = lumOf( L.amb );
	const float skyAl = wwWeatherRecx().groundAlbedo >= 0.0f ? wwWeatherRecx().groundAlbedo : 0.15f;
	const bool axRed = wwPhysAxesRed();
	const int up = axRed ? 4 : 5;	// the index a face looking up reads: the weather's order (Z-), red: the face order
	float n[3] = { L.sunDir[0], L.sunDir[1], L.sunDir[2] };
	const float len = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
	float axes[6][3], T[3], out[6][3];
	bool sky = false;
	if ( len > 0.0f ) {
		for ( float & x : n )
			x /= len;
		sky = wwAtmoGiSkyAxes( n, axes, T ) && WwAtmoMath::giSkyAbsolute( axes, T, L.sun, L.dalc, out ) > 0.0f;
	}
	// MODULES1 round 10: the one night sky's factor on the key's scattered light (never on the key itself)
	const WwNightSky & nsk = wwAtmoNightSky();
	const float nsFloor = nsk.mode == 0 ? nsk.floorGain : 1.0f;
	if ( sky && nsk.mode == 0 )
		for ( int a = 0; a < 5; a++ )
			for ( int c = 0; c < 3; c++ )
				out[a][c] *= nsk.gain;
	float d[3] = { L.discPos[0], L.discPos[1], L.discPos[2] };
	/* MODULES1 M7: the sun's sky from the sun itself (sunBodyDir). Under the horizon: the sky of a sun just on it
	 * (elevation 0.57 degrees) x 10^(0.4 x elevation), ~750 lux at sunset to ~3 lux at -6 (civil twilight's end) */
	float twi = 1.0f;
	if ( !wwSkyTwilightRed() ) {
		sunBodyDir( st(), d );
		const float e = std::asin( std::clamp( d[2], -1.0f, 1.0f ) ) * 57.29578f;
		if ( d[2] < 0.01f ) {
			twi = std::pow( 10.0f, 0.4f * std::min( e, 0.0f ) );
			const float h = std::sqrt( d[0] * d[0] + d[1] * d[1] );
			d[0] = h > 1e-6f ? d[0] / h * 0.99995f : 0.99995f;
			d[1] = h > 1e-6f ? d[1] / h * 0.99995f : 0.0f;
			d[2] = 0.01f;
		}
		static int saidE = 9999;
		if ( int( std::lround( e * 10.0f ) ) != saidE ) {
			saidE = int( std::lround( e * 10.0f ) );
			fprintf( stderr, "sun sky: from the sun body, elevation %.1f deg, sky x%.4g (red WW_SKY_TWILIGHT_RED=1: the disc)\n",
				double( e ), double( twi ) );
		}
	}
	const float dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
	if ( dl > 0.0f ) {
		for ( float & x : d )
			x /= dl;
		float sAxes[6][3];
		if ( ( !sky || n[0] * d[0] + n[1] * d[1] + n[2] * d[2] < 0.995f ) && wwAtmoGiSkyAxes( d, sAxes ) ) {
			if ( !sky ) {	// the key is down (or the moon's sky refused): the sun's sky alone
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						out[a][c] = 0.0f;
				sky = true;
			}
			for ( int a = 0; a < 5; a++ )
				for ( int c = 0; c < 3; c++ )
					out[a][c] += sAxes[a][c] * wwphys::kSunToaLux / wwphys::kPi * twi;
		}
	}
	if ( sky ) {
		WwAtmoMath::giSkyGround( L.sun, n[2], wwWeatherRecx().groundAlbedo >= 0.0f ? wwWeatherRecx().groundAlbedo : 0.15f, out );
		/* lane SKYLIGHT1 (bungo: overcast land too dark): the ambient comes from the sky as drawn WITH its clouds. The deck
		 * (the cloud map's Low + Mid, the volume's own density) passes the key's light on its top (what a surface takes:
		 * key x sunlight scale x mu) as diffuse light and the clear sky's through (clouddeckmath.h). Up = that; the down face
		 * the ground's bounce of it; the sides under the deck (0.396 + rho / 2) x up (CIE overcast sky, Ev / Eh, + the ground's
		 * half), blended by the cover. Red WW_SKY_CLOUDDECK_RED=1: the clear sky's ambient. */
		WW_SWITCH_STATIC( bool, deckRed, wwEnvStr( "WW_SKY_CLOUDDECK_RED" ).trimmed() == QLatin1StringView( "1" ) );
		float dk[4];
		if ( !deckRed && len > 0.0f && n[2] > 0.0f && wwCloudDeckThrough( n[2], dk ) ) {
			const float rho = skyAl, ks = wwLookdevSunlightScale() * n[2];
			const WwCloudDeck::Through t = { dk[0], dk[1], dk[2], dk[3] };
			const float clearUp = lumOf( out[4] );
			for ( int c = 0; c < 3; c++ ) {
				const float upD = WwCloudDeck::upC( t, L.sun[c] * ks, out[4][c], rho, false );
				const float add = upD - out[4][c];
				for ( int a = 0; a < 4; a++ )
					out[a][c] = ( 1.0f - t.cover ) * out[a][c] + t.cover * ( 0.396f + 0.5f * rho ) * upD;
				out[4][c] = upD;
				out[5][c] += rho * add;
			}
			static int saidDk = -1;
			const int k = int( std::lround( dk[0] * 100.0f ) ) * 1000 + int( std::lround( dk[1] * 100.0f ) );
			if ( k != saidDk ) {
				saidDk = k;
				fprintf( stderr, "sky deck: cover %.3f tau %.2f sun through %.3f sky through %.3f -> up x%.3f of the clear sky's"
					" (red WW_SKY_CLOUDDECK_RED=1)\n", double( dk[0] ), double( dk[3] ), double( dk[1] ), double( dk[2] ),
					double( lumOf( out[4] ) / std::max( clearUp, 1e-9f ) ) );
			}
		}
		for ( int a = 0; a < 6; a++ )	// out is by the face; the readers take the weather's order (wwPhysAxesRed)
			std::copy( out[a], out[a] + 3, L.dalc[axRed ? a : ( a ^ 1 )] );
	} else {
		for ( int a = 0; a < 6; a++ )
			for ( int c = 0; c < 3; c++ )
				L.dalc[a][c] *= rKey;
	}
	for ( int c = 0; c < 3; c++ ) {
		float m = 0.0f;
		for ( int a = 0; a < 6; a++ )
			m += ( L.dalc[a][c] += wwphys::kNightFloor * nsFloor	// MODULES1 R7: + the record's night sky: all of it on +Z, its ground bounce on -Z
				+ ( a == up ? 1.0f : a == ( up ^ 1 ) ? skyAl : 0.5f * ( 1.0f + skyAl ) ) * L.skyGame[c] );
		L.amb[c] = m / 6.0f;
	}
	const float oldTot = lumOf( L.sun ) / std::max( rKey, 1e-9f ) + ambOld;
	const float rEff = oldTot > 1e-12f ? ( lumOf( L.sun ) + lumOf( L.amb ) ) / oldTot : rKey;
	/* MODULES1 M3 round 2 (bungo 11-10, steam_white_wall / steam_mist_wide: the street steam a bright white curtain in the
	 * shaded alley). A lit effect takes no shadow and no sky occlusion (the game's base, HAZECARD1), so its level must stand
	 * to the SHADED light as the game's does: the Effect Lighting row sits beside the weather's ambient, not beside the full
	 * sun. rEff kept its share of sun + ambient, but the physical sky's ambient share is far below the weather's (Clear noon:
	 * ambient 4418 of 63550 lux), so the steam came out ~2x a wall in the open shade and more in an alley. Now the effect
	 * scales with the ambient alone (rAmb). Red WW_FX_EFFECT_SCALE_RED=1: the sun + ambient scale, as before. */
	WW_SWITCH_STATIC( bool, fxScaleRed, wwEnvStr( "WW_FX_EFFECT_SCALE_RED" ).trimmed() == QLatin1StringView( "1" ) );
	const float rAmb = ambOld > 1e-12f ? lumOf( L.amb ) / ambOld : rEff;
	const float rFx = fxScaleRed ? rEff : rAmb;
	for ( int c = 0; c < 3; c++ )
		L.effect[c] *= rFx;
	L.physKey = rKey;
	L.physFog = rEff;
	wwPhysNoteDaylight( L.sun, len > 0.0f ? n[2] : 0.0f, L.dalc[up] );	// MODULES1 item 14: the incident meter
	static QString said;
	const QString line = QStringLiteral( "phys light: key %1 cd/m2 (x%2 %3) sky +Z %4 -Z %5 ambient %6 effect x%7 sky=%8 (fog x%9)" )
		.arg( double( lumOf( L.sun ) ), 0, 'g', 5 ).arg( double( rKey ), 0, 'g', 5 )
		.arg( airKey ? QStringLiteral( "air" ) : QStringLiteral( "weather" ) ).arg( double( lumOf( L.dalc[up] ) ), 0, 'g', 5 )
		.arg( double( lumOf( L.dalc[up ^ 1] ) ), 0, 'g', 5 ).arg( double( lumOf( L.amb ) ), 0, 'g', 5 ).arg( double( rFx ), 0, 'g', 5 )
		.arg( sky ? QStringLiteral( "physical" ) : QStringLiteral( "weather" ) ).arg( double( rEff ), 0, 'g', 5 )
		+ ( axRed ? QStringLiteral( " axes=RED WW_PHYSUNITS_AXES_RED=1 (by the face: every face reads the opposite side's sky)" )
		          : QStringLiteral( " axes=weather order (a face looking up reads Z-; red WW_PHYSUNITS_AXES_RED=1)" ) );
	if ( line != said ) {
		said = line;
		fprintf( stderr, "%s\n", qPrintable( line ) );
	}
}

/* MODULES1 round 10 (bungo 11-11: "our physical sky is meant to be used day and night", "Why are you showing me vanilla
 * sky"): the light's own sky unit goes to the atmosphere (the drawn sky takes it by night, so the drawn sky and the
 * RTXGI miss sky are one sky), and with the master Off the six ambient axes cross to the SAME physical sky x factor by
 * the night share (they were the record's rows, tinted). The moon down (no sky from the key): the axes stay as they
 * were. Reds WW_NIGHTSKY_RED=physical / record. */
void nightFinish( LdLight & L, WwNightSky ns, bool phys )
{
	auto lumOf = []( const float v[3] ) { return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2]; };
	float n[3] = { L.sunDir[0], L.sunDir[1], L.sunDir[2] };
	const float len = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
	float axes[6][3], T[3] = { 0.0f, 0.0f, 0.0f };
	bool have = false;
	if ( len > 0.0f && ns.share > 0.0f ) {
		for ( float & x : n )
			x /= len;
		have = wwAtmoGiSkyAxes( n, axes, T );
	}
	const float lt = have ? lumOf( T ) : 0.0f;
	ns.lightK = lt > 1e-9f ? lumOf( L.sun ) / lt : 0.0f;
	wwAtmoSetNightSky( ns );
	const bool axRed = wwPhysAxesRed();
	const int up = axRed ? 4 : 5;
	if ( !phys && ns.mode == 0 && have ) {
		float out[6][3];
		if ( WwAtmoMath::giSkyAbsolute( axes, T, L.sun, L.dalc, out ) > 0.0f ) {
			for ( int a = 0; a < 5; a++ )
				for ( int c = 0; c < 3; c++ )
					out[a][c] *= ns.gain;
			WwAtmoMath::giSkyGround( L.sun, n[2], wwWeatherRecx().groundAlbedo >= 0.0f ? wwWeatherRecx().groundAlbedo : 0.15f, out );
			for ( int c = 0; c < 3; c++ ) {
				float m = 0.0f;
				for ( int a = 0; a < 6; a++ ) {	// out is by the face; the readers take the weather's order
					float & d = L.dalc[axRed ? a : ( a ^ 1 )][c];
					d += ( out[a][c] - d ) * ns.share;
					m += d;
				}
				L.amb[c] += ( m / 6.0f - L.amb[c] ) * ns.share;
			}
		}
	}
	/* MODULES1 10b-4 (bungo: sky physical day AND night, light never from painted record colours): with the key under
	 * the horizon the physical night sky is its floor alone (the airglow x the floor gain, as the sky draws it). The
	 * master-Off axes took the record's rows there; now they cross to that floor by the night share: +Z gathers
	 * pi L, -Z the ground's bounce rho x +Z, a side half of each. The record's rows: red WW_NIGHTSKY_RED=record. */
	float floorUnit = 0.0f, fs[3] = { 0.0f, 0.0f, 0.0f };
	const bool floorOnly = !phys && ns.mode == 0 && !have && ns.share > 0.0f && wwAtmoNightFloorScene( fs, &floorUnit );
	if ( floorOnly ) {
		const float rho = wwWeatherRecx().groundAlbedo >= 0.0f ? wwWeatherRecx().groundAlbedo : 0.15f;
		for ( int c = 0; c < 3; c++ ) {
			const float upE = 3.14159265f * fs[c];
			float m = 0.0f;
			for ( int a = 0; a < 6; a++ ) {	// by the face (4 = up, 5 = down); the readers take the weather's order
				const float want = a == 4 ? upE : a == 5 ? rho * upE : 0.5f * ( 1.0f + rho ) * upE;
				float & d = L.dalc[axRed ? a : ( a ^ 1 )][c];
				d += ( want - d ) * ns.share;
				m += d;
			}
			L.amb[c] += ( m / 6.0f - L.amb[c] ) * ns.share;
		}
	}
	{
		static int said = -1;
		const int nowSaid = floorOnly ? 1 + int( std::lround( ns.share * 20.0f ) ) : 0;
		if ( nowSaid != said && !phys && ns.mode == 0 ) {
			said = nowSaid;
			qInfo().noquote() << ( floorOnly
				? QStringLiteral( "night sky: moon down: floor: the master-Off ambient axes take the physical night sky's floor"
					" (airglow x floor gain %1, unit %2) by the night share %3: +Z %4 %5 %6 (level %7); the record's rows are the"
					" red WW_NIGHTSKY_RED=record" )
					.arg( double( ns.floorGain ), 0, 'g', 4 ).arg( double( floorUnit ), 0, 'g', 4 ).arg( double( ns.share ), 0, 'f', 2 )
					.arg( double( L.dalc[up][0] ), 0, 'g', 4 ).arg( double( L.dalc[up][1] ), 0, 'g', 4 ).arg( double( L.dalc[up][2] ), 0, 'g', 4 )
					.arg( double( lumOf( L.dalc[up] ) ), 0, 'g', 4 )
				: QStringLiteral( "night sky: ambient axes: %1" ).arg( have ? QStringLiteral( "the key is up: the physical sky x factor" )
					: ns.share > 0.0f ? QStringLiteral( "moon down, no sky frame yet: the record's rows this frame" )
					: QStringLiteral( "day: no night share" ) ) );
		}
	}
	wwAtmoNightSkyGot( WwNsAmbient, floorOnly ? floorUnit : ns.lightK * ( ns.mode == 0 ? ns.gain : 1.0f ), lumOf( L.dalc[up] ) );
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
	{	// MODULES1 R6-6: the real path is read on the game's clock (its horizon hours are the sky clock's A and D)
		const WwSkyClock gc = wwSkyClock( s.hour, s.tnam, s.gmst );
		wwSunPathGameHorizon( gc.A, gc.D );
	}
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
	/* lane FIX1101A (moon, LATE ITEMs 24b / 24c / 24d / 27f): the moon is lit like the sun (gl/moonlight.h). moonK = the
	 * moon's light against the record's Night key: our globe's phase (Krisciunas & Schaefer, moonLightFactor) x the
	 * weather .recx night.moonlight (lane WX1). The engine clock's night share wE = 1 - its sun disc alpha (the 9-minute
	 * fade C..D, A..B): the key no longer switches at D. Red WW_MOON_RED=handover (or vanilla): the switch at D, and under
	 * Sun Path = Real the light slerped to the engine's arc after the real sunset (a ghost day sun until D). */
	const WwSkyClock raw = wwSkyClock( s.hour, s.tnam, s.gmst );
	const bool engNight = s.hour < raw.A || s.hour > raw.D;
	const bool oldNight = wwMoonRed( "handover" );
	// MODULES1 M7: under Sun Path Real the night share follows the real sun (the physical night's law), not the clock
	double rsNow[3];
	const bool realShare = !oldNight && wwSunPathMode() == 1 && !wwSkyTwilightRed();
	const float wE = oldNight ? ( engNight ? 1.0f : 0.0f )
		: realShare ? wwmoon::nightWeight( float( wwSunPathReal( s.hour, s.gameDay, rsNow ) ) ) : 1.0f - raw.sunAlpha;
	{
		static QString said;
		const QString line = QStringLiteral( "light night share: %1 (%2)" ).arg( double( wE ), 0, 'f', 3 )
			.arg( realShare ? QStringLiteral( "the real sun" ) : QStringLiteral( "the engine clock" ) );
		if ( line != said ) {
			said = line;
			fprintf( stderr, "%s\n", qPrintable( line ) );
		}
	}
	float moonK = 1.0f;
	if ( wwAtmoNightOn() )	// lane SKYA (NIGHT1): by night the engine's key light is the moon's: its phase dims it
		moonK *= moonLightFactor( s );
	const WwWeatherRecx & wx = wwWeatherRecx();
	if ( wx.on && wx.moonlight >= 0.0f )	// lane WX1: night.moonlight of the weather's .recx (Weather .recx row)
		moonK *= wx.moonlight;
	auto weatherKeyOf = [&s]( int tod ) {
		WwTodKeys k;
		k.a = k.b = tod;
		k.t = 0.0f;
		float c3[3];
		EsmWeather::blendRowLab( s.w, WwRowSunlight, k, c3 );
		return 0.2126f * lin( c3[0] ) + 0.7152f * lin( c3[1] ) + 0.0722f * lin( c3[2] );
	};
	/* lane PHYSUNITS1: real units: the key is the top-of-atmosphere sun (moon) over pi, the air's transmittance on it
	 * (atmosphere.cpp); rKey = that at 45 degrees (T ~ 0.8) over the weather's key, the two crossfaded as the keys are */
	const bool phys = wwPhysSkyOn();
	/* MODULES1 R6-12 (bungo 11-10: "It's night time, I can't see anything", "the moon seems underpowered a bit, for
	 * gameplay purposes"): ONE gameplay factor on the moon's illuminance at its source (the key, and through it the
	 * sky's unit, the ambient, the fog and the GI). The real full moon (0.32 lux) leaves the land at ~0.007 cd/m2, and
	 * the camera does not adapt under 1 cd/m2 (EV100 3 less the key, at any night), so the night drew 7 EV under
	 * middle grey. The factor makes the night as dark against the day as the weather record says: the record's Night
	 * sunlight over its Day sunlight (linear) x the sun over the moon x the camera's floor over its daylight
	 * luminance (the exposure ratio between a metered day and the floor). CommonwealthClear: x8.1 (2.6 lux).
	 * WW_MOON_GAME=<factor> pins it (live); red WW_MOON_RED=physical: x1, the real moon. */
	float moonGame = 1.0f;
	if ( phys && !wwMoonRed( "physical" ) ) {
		const float vd = weatherKeyOf( WwTodDay ), vn = weatherKeyOf( WwTodNight );
		if ( vd > 1e-9f && vn > 0.0f )
			moonGame = ( vn / vd ) * ( wwphys::kSunToaLux / wwphys::kMoonToaLux ) * ( wwphys::kAdaptFloorCdm2 / wwphys::kKeyDaylightCdm2 );
		bool ok = false;
		const float pin = wwEnvStr( "WW_MOON_GAME" ).trimmed().toFloat( &ok );
		if ( ok && pin > 0.0f )
			moonGame = pin;
		moonGame = std::clamp( moonGame, 1.0f, 64.0f );	// never under the real moon
		static QString saidGame;
		const QString line = QStringLiteral( "moon light game: factor x%1%2 (record night/day key %3, sun/moon %4, camera floor/daylight %5); full moon above the air %6 lux, x phase and weather %7 = %8 lux applied (red WW_MOON_RED=physical: x1; the land's luminance and the EV: the phys camera line)" )
			.arg( double( moonGame ), 0, 'f', 2 ).arg( ok && pin > 0.0f ? QStringLiteral( " (PINNED WW_MOON_GAME)" ) : QString() )
			.arg( vd > 1e-9f ? double( vn / vd ) : 0.0, 0, 'f', 4 ).arg( double( wwphys::kSunToaLux / wwphys::kMoonToaLux ), 0, 'f', 0 )
			.arg( double( wwphys::kAdaptFloorCdm2 / wwphys::kKeyDaylightCdm2 ), 0, 'g', 3 )
			.arg( double( wwphys::kMoonToaLux * moonGame ), 0, 'f', 2 ).arg( double( moonK ), 0, 'f', 3 )
			.arg( double( wwphys::kMoonToaLux * moonGame * moonK ), 0, 'f', 3 );
		if ( line != saidGame ) {
			saidGame = line;
			fprintf( stderr, "%s\n", qPrintable( line ) );
		}
	} else if ( phys ) {
		static bool saidRed = false;
		if ( !saidRed ) {
			saidRed = true;
			fprintf( stderr, "moon light game: RED WW_MOON_RED=physical: x1, the real moon (%.2f lux above the air)\n", double( wwphys::kMoonToaLux ) );
		}
	}
	auto keyLumOf = [&]( int tod ) {
		if ( !phys )
			return weatherKeyOf( tod );
		return ( tod == WwTodNight ? wwphys::kMoonToaLux * moonGame : wwphys::kSunToaLux ) / wwphys::kPi;
	};
	float rKey = 1.0f;
	if ( phys ) {
		const float vd = weatherKeyOf( WwTodDay ), vn = weatherKeyOf( WwTodNight );
		const float rd = vd > 1e-9f ? keyLumOf( WwTodDay ) * 0.8f / vd : 1.0f;
		const float rn = vn > 1e-9f ? keyLumOf( WwTodNight ) * 0.8f / vn : rd;
		rKey = rd * std::pow( rn / rd, std::clamp( wE, 0.0f, 1.0f ) );
	}
	/* MODULES1 R7 (bungo 11-11: "the sky is pitch black, why", "I already told you to fix the physical sky being too dark
	 * at night"): the night sky at the level the weather record gives it. Put in the light's units, the record's night
	 * sky is ~40x over the real one (2.2e-4 cd/m2 + the moon's scatter), and the camera does not adapt under 1 cd/m2.
	 * Master On: the record's sky rows x Sky Scale x the key's unit x the night share are ADDED to the drawn sky
	 * (atmo_sky.frag atRecNight.y) and to the ambient (physFinish). Master Off: the drawn sky crossfades to the record's
	 * dome colours by the night share (atRecNight.x), as the game draws them. Red WW_NIGHTSKY_RED=physical: neither. */
	/* MODULES1 round 10: no record dome in the default path. ONE night sky, the physical one, x one readable-night
	 * factor: film's day-for-night shows a night scene about 2 stops under the day's exposure (notes: r10 BARS.md), so
	 * the sky a full moon lights is drawn 2^-stops x the weather's Day key over its Night key brighter, and the moon's
	 * phase share is cancelled on the SKY (the key keeps it: a thin moon gives darker ground under the same readable
	 * sky). The floor (airglow) takes the same rule in the moon's own unit. The record crossfade above is the red
	 * WW_NIGHTSKY_RED=record; =physical is the sky with no factor. WW_NIGHTSKY_STOPS (live, default 2). */
	const QString nsWord = wwEnvStr( "WW_NIGHTSKY_RED" ).trimmed().toLower();
	const int nsMode = nsWord == QLatin1StringView( "physical" ) ? 1 : nsWord == QLatin1StringView( "record" ) ? 2 : 0;
	L.nightW = ( nsMode != 2 || !s.haveWeather ) ? 0.0f : std::clamp( wE, 0.0f, 1.0f );
	WwNightSky ns;
	ns.mode = nsMode;
	ns.share = std::clamp( wE, 0.0f, 1.0f );
	{
		bool ok = false;
		const float st = wwEnvStr( "WW_NIGHTSKY_STOPS" ).trimmed().toFloat( &ok );
		ns.stops = ok ? std::clamp( st, 0.0f, 8.0f ) : 2.0f;
		const float vd = weatherKeyOf( WwTodDay ), vn = weatherKeyOf( WwTodNight );
		// the night key over the day key as the screen shows them (master On: through the moon's gameplay factor)
		const float onScreen = phys ? ( wwphys::kMoonToaLux * moonGame / wwphys::kSunToaLux ) * ( wwphys::kKeyDaylightCdm2 / wwphys::kAdaptFloorCdm2 )
			: ( vd > 1e-9f ? vn / vd : 0.0f );
		const float full = onScreen > 1e-9f ? std::clamp( std::exp2( -ns.stops ) / onScreen, 1.0f, 4096.0f ) : 1.0f;
		ns.phase = !wwAtmoNightOn() ? 1.0f : std::clamp( wwAtmoDiscs() ? wwAtmoMoonFactorDeg( moonAlphaNow( s ) )
			: wwAtmoMoonFactor( wwMoonPhase( s.gameDay, s.clim.tnam[5] ) ), 0.02f, 1.0f );
		ns.factor = std::clamp( full / ns.phase, 1.0f, 4096.0f );
		// the floor is in the sun's unit: x the moon's unit over the sun's (master Off: the record's keys against the real ones)
		const float unit = phys ? moonGame : onScreen * wwphys::kSunToaLux / wwphys::kMoonToaLux;
		const float floorFull = std::clamp( full * unit, 1.0f, 1.0e7f );
		if ( nsMode == 0 && s.haveWeather ) {
			ns.gain = 1.0f + ( ns.factor - 1.0f ) * ns.share;
			ns.floorGain = 1.0f + ( floorFull - 1.0f ) * ns.share;
		}
	}
	wwAtmoSetNightSky( ns );	// physFinish reads the gain; nightFinish adds the light's unit
	if ( s.haveWeather && nsMode != 0 ) {
		const int rows3[3] = { WwRowSkyUpper, WwRowSkyLower, WwRowHorizon };
		float m3[3] = { 0.0f, 0.0f, 0.0f };
		for ( int r3 : rows3 ) {
			float c3[3];
			EsmWeather::blendRowLab( s.w, r3, L.keys, c3 );
			for ( int c = 0; c < 3; c++ )
				m3[c] += lin( c3[c] ) / 3.0f;
		}
		const float sc = EsmWeather::blendSkyScale( s.w, L.keys );
		if ( phys )
			for ( int c = 0; c < 3; c++ )
				L.skyGame[c] = m3[c] * sc * rKey * L.nightW;
		const double recL = double( ( 0.2126f * m3[0] + 0.7152f * m3[1] + 0.0722f * m3[2] ) * sc );
		static QString saidSky;
		const QString line = wwNightSkyRed()
			? QStringLiteral( "night sky: RED WW_NIGHTSKY_RED=physical: the physical night sky alone (master %1; the record's sky %2 not used)" )
				.arg( phys ? QStringLiteral( "On" ) : QStringLiteral( "Off" ) ).arg( recL, 0, 'g', 3 )
			: phys ? QStringLiteral( "night sky: master On: record sky %1 (rows x Sky Scale %2) x key unit %3 x night share %4 = +%5 cd/m2 added to the drawn sky and the ambient (rgb %6 %7 %8; the real floor %9; red WW_NIGHTSKY_RED=physical)" )
				.arg( recL, 0, 'g', 3 ).arg( double( sc ), 0, 'g', 3 ).arg( double( rKey ), 0, 'g', 4 ).arg( double( L.nightW ), 0, 'f', 3 )
				.arg( recL * double( rKey * L.nightW ), 0, 'g', 3 ).arg( double( L.skyGame[0] ), 0, 'g', 3 ).arg( double( L.skyGame[1] ), 0, 'g', 3 )
				.arg( double( L.skyGame[2] ), 0, 'g', 3 ).arg( double( wwphys::kNightFloor ), 0, 'g', 3 )
			: QStringLiteral( "night sky: master Off: the drawn sky x %1 + the record's dome colours x %2 (night share; record sky %3 = rows %4 %5 %6 x Sky Scale %7; red WW_NIGHTSKY_RED=physical)" )
				.arg( double( 1.0f - L.nightW ), 0, 'f', 3 ).arg( double( L.nightW ), 0, 'f', 3 ).arg( recL, 0, 'g', 3 )
				.arg( double( m3[0] ), 0, 'g', 3 ).arg( double( m3[1] ), 0, 'g', 3 ).arg( double( m3[2] ), 0, 'g', 3 ).arg( double( sc ), 0, 'g', 3 );
		if ( line != saidSky ) {
			saidSky = line;
			fprintf( stderr, "%s\n", qPrintable( line ) );
		}
	}
	const WwFog fog = currentFog();
	const float fogIn[4] = { fog.effOff ? 0.0f : fog.fogFar, fog.effOff ? 0.0f : fog.maxv, fog.fMid, fog.fRange };
	if ( wwAtmoSunKey() > 0 && wwAtmoSun() == 2 && wwAtmoNightOn() && wwSunPathMode() == 1 && !oldNight ) {
		/* the physical night (Full + Moon Phase and Stars + Sun Path Real): the real sun's part plus our globe's part,
		 * the Day / Night keys anchored at 45 degrees as ATMOSUN1 anchors the sun (tests/features/sky/moonlight1101a) */
		double rs[3];
		const double eSun = wwSunPathReal( s.hour, s.gameDay, rs );
		const MoonGeo g = moonGeo( s, raw.sunPos );
		const bool moonUp = ( g.real || engNight ) && g.D[2] > 0.0;	// the game clock's moon is the engine's night point
		const float wN = wwmoon::nightWeight( float( eSun ) );
		float sun[3] = { 0.0f, 0.0f, 0.0f }, moon[3] = { 0.0f, 0.0f, 0.0f };
		float ambX[3], dalcX[6][3];	// the parts' ambient tints are not kept (the light's own direction tints it below)
		if ( rs[2] > 0.0 ) {
			std::copy( L.amb, L.amb + 3, ambX );
			std::copy( &L.dalc[0][0], &L.dalc[0][0] + 18, &dalcX[0][0] );
			wwAtmoKeyLight( float( rs[2] ), fogIn, keyLumOf( WwTodDay ), sun, ambX, dalcX );
		}
		if ( moonUp && wN > 0.0f ) {
			std::copy( L.amb, L.amb + 3, ambX );
			std::copy( &L.dalc[0][0], &L.dalc[0][0] + 18, &dalcX[0][0] );
			wwAtmoKeyLight( float( g.D[2] ), fogIn, keyLumOf( WwTodNight ), moon, ambX, dalcX );
			for ( int c = 0; c < 3; c++ )
				moon[c] *= moonK * wN;
		}
		const float sd[3] = { float( rs[0] ), float( rs[1] ), float( rs[2] ) };
		const float md[3] = { float( g.D[0] ), float( g.D[1] ), float( g.D[2] ) };
		float dir[3];
		wwmoon::combine( sun, sd, moon, md, wN, L.sun, dir );
		std::copy( dir, dir + 3, L.sunDir );
		float keyX[3] = { L.sun[0], L.sun[1], L.sun[2] };
		wwAtmoKeyLight( dir[2], fogIn, keyLumOf( wN >= 0.5f ? WwTodNight : WwTodDay ), keyX, L.amb, L.dalc );
		static QString said;
		const QString line = QStringLiteral( "moon light: rule=physical sun_elev=%1 night_w=%2 moon=%3 moon_elev=%4 alpha=%5 ks=%6 sun_lum=%7 moon_lum=%8 light_elev=%9" )
			.arg( eSun, 0, 'f', 2 ).arg( double( wN ), 0, 'f', 3 ).arg( moonUp ? QStringLiteral( "up" ) : QStringLiteral( "down" ) )
			.arg( std::asin( std::clamp( g.D[2], -1.0, 1.0 ) ) / wwskydisc::kRad, 0, 'f', 2 ).arg( g.alphaDeg, 0, 'f', 2 )
			.arg( double( moonLightFactor( s ) ), 0, 'f', 4 ).arg( double( wwmoon::lumOf( sun ) ), 0, 'g', 5 )
			.arg( double( wwmoon::lumOf( moon ) ), 0, 'g', 5 ).arg( std::asin( std::clamp( double( dir[2] ), -1.0, 1.0 ) ) / wwskydisc::kRad, 0, 'f', 2 );
		if ( line != said ) {
			said = line;
			fprintf( stderr, "%s\n", qPrintable( line ) );
		}
		if ( phys )
			physFinish( L, rKey, true );
		nightFinish( L, ns, phys );
		return L;
	}
	bool airKey = false;
	if ( wwAtmoSunKey() > 0 || phys ) {
		/* lane SKYA (ATMOSUN1): the key and the ambient through the physical sky's air toward the light (the moon by
		 * night, as the engine's light is), the brightness anchored on the weather's Day (Night) key at 45 degrees;
		 * lane FIX1101A: the two keys crossfaded by the engine's night share, geometrically (x1.34 a minute over the
		 * 9-minute fade, not one x14 step; Full: the moon's factor in the Night key) */
		const float dayL = keyLumOf( WwTodDay ), nightL = keyLumOf( WwTodNight ) * ( ( wwAtmoSun() == 2 || phys ) ? moonK : 1.0f );
		const float keyLum = oldNight ? ( engNight ? keyLumOf( WwTodNight ) : dayL )
			: dayL > 1e-9f ? dayL * std::pow( std::max( nightL, 0.0f ) / dayL, wE ) : nightL * wE;
		const float len = std::sqrt( L.sunDir[0] * L.sunDir[0] + L.sunDir[1] * L.sunDir[1] + L.sunDir[2] * L.sunDir[2] );
		airKey = wwAtmoKeyLight( len > 0.0f ? L.sunDir[2] / len : 1.0f, fogIn, keyLum, L.sun, L.amb, L.dalc );
	}
	// the moon's factor on the night share (already in the Full key above, unless the red)
	const float f = oldNight ? ( engNight ? moonK : 1.0f )
		: ( ( wwAtmoSunKey() > 0 && wwAtmoSun() == 2 ) || airKey ) ? 1.0f : std::pow( std::max( moonK, 0.0f ), wE );
	for ( int c = 0; c < 3; c++ )
		L.sun[c] *= f;
	if ( phys )
		physFinish( L, rKey, airKey );
	nightFinish( L, ns, phys );
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
	static unsigned long long wxOv = 0;	// lane MODULES1 item 12: a live fog override moves the fog next frame
	if ( gen != s.gen || hour != s.hour || wxOv != wwWxOvStamp() ) {
		wxOv = wwWxOvStamp();
		// the record's per-time-of-day fog colours (sRGB bytes), each pinnable
		static const WwWx tod[2][4] = { { WwWx::FogNearSunrise, WwWx::FogNearDay, WwWx::FogNearSunset, WwWx::FogNearNight },
			{ WwWx::FogFarSunrise, WwWx::FogFarDay, WwWx::FogFarSunset, WwWx::FogFarNight } };
		const int rows[2] = { WwRowFogNear, WwRowFogFar };
		const bool patch = wwWxAnyTodColor();
		WwWeatherData w;
		if ( patch )
			w = s.w;
		for ( int r = 0; r < 2; r++ )
			for ( int t = 0; t < 4; t++ ) {
				float v[3];
				for ( int c = 0; c < 3; c++ )
					v[c] = float( s.w.color[t][rows[r]][c] );
				wwWxOv3( tod[r][t], v );
				if ( patch )
					for ( int c = 0; c < 3; c++ )
						w.color[t][rows[r]][c] = (unsigned char)std::lround( std::min( std::max( v[c], 0.0f ), 255.0f ) );
			}
		memo = wwFogAt( patch ? w : s.w, s.hour, s.tnam, s.gmst );
		memo.fogNear = wwWxOv( WwWx::FogNear, memo.fogNear );
		memo.fogFar = wwWxOv( WwWx::FogFar, memo.fogFar );
		memo.power = wwWxOv( WwWx::FogPower, memo.power );
		memo.maxv = wwWxOv( WwWx::FogMax, memo.maxv );
		memo.nMid = wwWxOv( WwWx::FogNearHeightMid, memo.nMid );
		memo.nRange = wwWxOv( WwWx::FogNearHeightRange, memo.nRange );
		memo.fMid = wwWxOv( WwWx::FogFarHeightMid, memo.fMid );
		memo.fRange = wwWxOv( WwWx::FogFarHeightRange, memo.fRange );
		memo.hds = wwWxOv( WwWx::FogHighDensity, memo.hds );
		wwWxOv3( WwWx::FogNearColor, memo.nearLow );
		wwWxOv3( WwWx::FogFarColor, memo.farLow );
		wwWxOv3( WwWx::FogNearHighColor, memo.nearHigh );
		wwWxOv3( WwWx::FogFarHighColor, memo.farHigh );
		memo.effOff = false;
		wwFogPackK( memo );
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
	WwGodRayMedium g = wwGodRayMix( s.w.godRay[f.keys.a], s.w.godRay[f.keys.b], float( f.keys.t ) );
	if ( s.w.blending() )	// lane WEATHERBLEND1: the weather left behind x (1 - pct), the engine's four weights
		g = wwGodRayMix( wwGodRayMix( s.w.last->godRay[f.keys.a], s.w.last->godRay[f.keys.b], float( f.keys.t ) ), g, s.w.transPct );
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

/* MODULES1 M8 (bungo 11-10, 3 AM: the vanilla distance fog glowed blue against a black sky, a hard line where they
 * met). The fog's far colours are the drawn sky's just above the horizon (wwAtmoHorizon: the sun's and the moon's
 * scatter, the night floor, the tint), its near colours keep the weather's hue at the same luminance step. One colour
 * where sky and fog meet, by day and by night. Red WW_FOG_HORIZON_RED=1: the weather's colours x the light's scale. */
bool wwFogHorizonRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_FOG_HORIZON_RED" ).trimmed() == QLatin1StringView( "1" ) );
	return red;
}

/* MODULES1 R5A-1 (bungo b60 11-10, brown haze on a clear day): the horizon's hue is the sky's at the end of a path of
 * hundreds of km (CommonwealthClear day: 1.38e4 9821 6908, 1 : 0.71 : 0.50), not the haze's over a few km, and the far
 * colours took it whole. They keep the step (mean far luminance = the horizon's) and their own records' hue.
 * Red WW_FOG_HORIZON_RED=skyhue: the far colours = the horizon's colour. */
static bool wwFogHorizonSkyHue()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_FOG_HORIZON_RED" ).trimmed() == QLatin1StringView( "skyhue" ) );
	return red;
}

void wwLookdevFogUniforms( Scene * scene )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuFogUni );	// lane PERF2
	WwUniProf::Marks prof( 1 );	// lane PERFFIX1: WW_CELL_UNI_PROF=1, where this call's CPU goes
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "fogOn" ) < 0 )
		return;
	prof.prog( prog );
	prof.at( 0, "head" );
	wwVolAirUniforms( scene );	// lane VOLAIR1: every program that holds the fog holds the air's sampler
	prof.at( 1, "volair" );
	wwAtmoApUniforms( scene );	// lane SKYA: and the sky's aerial perspective (read with the fog row off too)
	prof.at( 2, "atmoap" );
	LdState & s = st();
	const bool on = wwLookdevFogWanted( scene );
	prog->uni1b( "fogOn", on );
	prof.at( 3, "wanted" );
	if ( !on )
		return;
	updateGroundFrame( scene );
	prof.at( 4, "ground" );
	const WwFog f = currentFog();
	prof.at( 5, "currentfog" );
	/* lane PHYSUNITS1: the fog's colours (fogK[1..4]) in the light's units; fogUnit lets the gray weight read the old scale */
	const float fu = wwPhysSkyOn() ? currentLight().physFog : 1.0f;
	float hz[3];
	const bool horizon = wwPhysSkyOn() && !wwFogHorizonRed() && wwAtmoHorizon( hz );
	const bool skyHue = wwFogHorizonSkyHue();
	float gNear = 1.0f;
	if ( horizon ) {	// MODULES1 M8: the far colours = the sky's horizon; the near ones scale by the same step
		auto lum = []( const float * c ) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; };
		const float far[3] = { 0.5f * ( f.K[3][0] + f.K[4][0] ) * fu, 0.5f * ( f.K[3][1] + f.K[4][1] ) * fu,
			0.5f * ( f.K[3][2] + f.K[4][2] ) * fu };
		const float lf = lum( far ), lh = lum( hz );
		gNear = lf > 1e-12f ? lh / lf : 1.0f;
		static float said = -1.0f;
		if ( std::abs( gNear - said ) > 0.02f * std::max( said, 1e-6f ) ) {
			said = gNear;
			fprintf( stderr, "fog horizon: sky %.4g %.4g %.4g (L %.4g), weather far L %.4g, fog x%.4g hue=%s (red WW_FOG_HORIZON_RED=1|skyhue)\n",
				double( hz[0] ), double( hz[1] ), double( hz[2] ), double( lh ), double( lf ), double( gNear ),
				skyHue ? "sky" : "record" );
		}
	}
	for ( int k = 0; k < 6; k++ ) {
		const float m = ( k >= 1 && k <= 4 ) ? fu * ( horizon ? gNear : 1.0f ) : 1.0f;
		if ( horizon && skyHue && ( k == 3 || k == 4 ) )
			prog->uni4f_l( prog->uniLocation( "fogK[%d]", k ), FloatVector4( hz[0], hz[1], hz[2], f.K[k][3] ) );
		else
			prog->uni4f_l( prog->uniLocation( "fogK[%d]", k ), FloatVector4( f.K[k][0] * m, f.K[k][1] * m, f.K[k][2] * m, f.K[k][3] ) );
	}
	prog->uni1f( "fogUnit", fu );
	prof.at( 6, "fogk" );
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
	prof.at( 7, "currentlight" );
	float v[3];
	for ( int i = 0; i < 3; i++ )
		v[i] = vt.rotation( i, 0 ) * L.sunDir[0] + vt.rotation( i, 1 ) * L.sunDir[1] + vt.rotation( i, 2 ) * L.sunDir[2];
	const float vl = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	for ( int i = 0; i < 3 && vl > 0.0f; i++ )
		v[i] /= vl;
	prog->uni4f( "fogSun", FloatVector4( v[0], v[1], v[2], 1.0f ) );
	prog->uni4f( "fogSunColour", FloatVector4( L.sun[0], L.sun[1], L.sun[2], s.gmst.dirFogPower ) );
	prog->uni4f( "fogProbe", FloatVector4( s.fogProbe[0], s.fogProbe[1], s.fogProbe[2], 0.0f ) );
	{	// MODULES1 R7 night sky d
		/* MODULES1 round 10: in the default path the saturated air's night colour is the drawn (lifted) sky's own
		 * colour just above the horizon (wwAtmoHorizon), so thick far air, the horizon band and the sky are one
		 * colour; no record colour, no dome. The record's colours stay under the red WW_NIGHTSKY_RED=record. */
		const bool phys = wwPhysSkyOn(), nRed = wwNightAirRed();
		const WwNightSky & nsk = wwAtmoNightSky();
		const float n = nRed ? 0.0f : nsk.mode == 0 ? nsk.share : L.nightW;
		const WwAtmoFrame & af = wwAtmoFrame();
		const float dome = ( nRed || !af.valid ) ? 0.0f : af.recNight[0];	// the dome's share as drawn
		const float mF = fu * ( horizon ? gNear : 1.0f );
		float c[3], hzN[3] = { 0.0f, 0.0f, 0.0f };
		const bool hzOk = nsk.mode == 0 && wwAtmoHorizon( hzN );
		for ( int k = 0; k < 3; k++ )
			c[k] = hzOk ? hzN[k] : ( phys || nsk.mode == 0 ) ? L.amb[k] : 0.5f * ( f.K[3][k] + f.K[4][k] ) * mF;
		{
			const float lc = ( 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2] ) * n;
			wwAtmoNightSkyGot( WwNsAir, nsk.lightK * nsk.gain, lc );
			wwAtmoNightSkyGot( WwNsFar, nsk.lightK * nsk.gain, hzOk ? 0.2126f * hzN[0] + 0.7152f * hzN[1] + 0.0722f * hzN[2] : 0.0f );
		}
		prog->uni4f( "fogNight", FloatVector4( c[0] * n, c[1] * n, c[2] * n, n ) );
		if ( prog->uniLocation( "fogNightHue" ) >= 0 )	// FARWATER1 late item 3: the night haze takes the sky's tint
			prog->uni1f( "fogNightHue", wwNightHazeRed() ? 0.0f : 1.0f );
		g_nightHaze[0] = c[0], g_nightHaze[1] = c[1], g_nightHaze[2] = c[2], g_nightHaze[3] = n;
		if ( prog->uniLocation( "volSkyDome" ) >= 0 )
			prog->uni1f( "volSkyDome", dome );
		static int saidKey = -1;	// the line is built only when the state moves (this runs per fogged program)
		const int key = nRed ? 1 : n <= 0.0f ? 2 : ( phys ? 3 : 5 ) + ( n >= 0.999f ? 1 : 0 ) + ( dome > 0.0f ? 4 : 0 ) + ( hzOk ? 16 : nsk.mode == 0 ? 32 : 0 );
		const QString line = key == saidKey ? QString() : nRed ? QStringLiteral( "night air: RED WW_NIGHTAIR_RED=1: the air's own light alone, the record's dome under the sky's residual and band" )
			: n <= 0.0f ? QStringLiteral( "night air: off (night share 0)" )
			: QStringLiteral( "night air: master %1: saturated air = %2 %3 %4 %5 x night share %6; a ray's air holds at least that x (1 - T) (sky cover and surfaces); the record's dome takes the air's own T over share %7 of the drawn sky (red WW_NIGHTAIR_RED=1)" )
				.arg( phys ? QStringLiteral( "On" ) : QStringLiteral( "Off" ) )
				.arg( hzOk ? QStringLiteral( "the drawn night sky's own horizon colour (round 10)" ) : nsk.mode == 0 ? QStringLiteral( "the ambient axes' mean (no sky drawn yet)" )
					: phys ? QStringLiteral( "the sky's mean with the record's level (ambient axes, albedo 1)" ) : QStringLiteral( "the record's far fog colour" ) )
				.arg( double( c[0] ), 0, 'g', 3 ).arg( double( c[1] ), 0, 'g', 3 ).arg( double( c[2] ), 0, 'g', 3 )
				.arg( double( n ), 0, 'f', 2 ).arg( double( dome ), 0, 'f', 2 );
		if ( key != saidKey ) {
			saidKey = key;
			fprintf( stderr, "%s\n", qPrintable( line ) );
		}
	}
	const bool tailRed = wwAirTailJRed();	// MODULES1 R7-4
	const bool levelRed = wwAirLevelRayRed();	// MODULES1 R7 item 6
	prog->uni1i( "fogRed", ( wwLookdevRed( "fogmaxclamp" ) ? 1 : 0 ) | ( wwLookdevRed( "fognoescape" ) ? 2 : 0 )
		| ( wwLookdevRed( "fogheight0" ) ? 4 : 0 ) | ( tailRed ? 8 : 0 ) | ( levelRed ? 16 : 0 ) );
	{	// the line is said when the eye crosses the air's top or the red moves (this runs per fogged program)
		float med[4], layer[4], gnd[2];
		static int saidLevel = -1;
		const bool have = wwVolAirMediumNow( med, layer, gnd );
		const int key = !have ? 0 : ( zr[3] >= med[3] ? 2 : 1 ) + ( levelRed ? 2 : 0 );
		if ( key != saidLevel ) {
			saidLevel = key;
			if ( have )
				fprintf( stderr, "sky air level ray: eye %.0f u in the air's frame, the air's top %.0f u: %s (red WW_CELL_AIR_LEVELRAY_RED=1)\n",
					double( zr[3] ), double( med[3] ),
					levelRed ? ( zr[3] >= med[3] ? "RED: the level row (sine within 1e-4) is clear above the top: the sky shows through the grids' T in one pixel row"
							: "RED: the old branch, opaque under the top (no row)" )
						: ( zr[3] >= med[3] ? "above the top: the level row ends like the rows beside it (the top's falloff holds air there)"
							: "under the top: the level row ends like the rows beside it" ) );
		}
	}
	prof.at( 8, "sun+reds" );
	static float lastZ = 0.0f, lastSc = 0.0f;	// lane PERF1: the echo string rebuilt only when it changes
	static bool lastLeak = false, lastOn = false, lastTail = false;
	if ( !lastOn || lastZ != s.groundZ || lastSc != sc || lastLeak != !s.fog || lastTail != tailRed
		|| !s.fogLast.startsWith( QLatin1StringView( "on(" ) ) ) {
		s.fogLast = QString( "on(groundz=%1 scale=%2%3 tailJ=%4)" ).arg( double( s.groundZ ), 0, 'f', 2 ).arg( double( sc ), 0, 'g', 6 )
			.arg( !s.fog ? QStringLiteral( " leak" ) : QString() )	// on with the row off = the fogleak red
			.arg( tailRed ? QStringLiteral( "RED last slice dL/dT" ) : QStringLiteral( "window 1/16 depth, thin by the law" ) );
		lastTail = tailRed;
		lastOn = true;
		lastZ = s.groundZ;
		lastSc = sc;
		lastLeak = !s.fog;
	}
	prof.at( 9, "echo" );
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
	return st().plugin;
}

void wwLookdevSetCellPlugins( const QStringList & plugins )
{
	QMutexLocker l( &cellMx );
	if ( plugins == cellOrder )
		return;
	cellOrder = plugins;
	cellOrderGen++;
}

void wwLookdevSetPlugin( const QString & path )
{
	LdState & s = st();
	if ( QDir::cleanPath( path ) == QDir::cleanPath( s.plugin ) && s.pinPlugins.isEmpty() )
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

QString wwLookdevRegion()
{
	return st().regionName;
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

bool wwLookdevWeatherClass( unsigned & flags, unsigned & thunder, int * thunderSounds, int * precip )
{
	resolve();
	const LdState & s = st();
	flags = thunder = 0;
	if ( thunderSounds )
		*thunderSounds = 0;
	if ( precip )	// lane CLOUDTYPE3: WTHR MNAM (esmweather.cpp, CLOUD6e)
		*precip = s.haveWeather && s.w.precipForm != 0 ? 1 : 0;
	if ( !s.haveWeather || !s.w.hasClass )
		return false;
	flags = s.w.classFlags;	// lane CLOUD5: WTHR DATA bytes 11 and 10 (esmweather.cpp)
	thunder = s.w.thunderFreq;
	if ( thunderSounds )	// lane CLOUDTYPE2: the thunder SNAMs (esmweather.cpp, CLOUD6f's storm signal)
		*thunderSounds = s.w.thunderSounds;
	return true;
}

bool wwLookdevLightning( float & share, float rgb[3], quint32 & seed, unsigned * freq )
{
	resolve();
	const LdState & s = st();
	share = 0.0f;
	rgb[0] = rgb[1] = rgb[2] = 0.0f;
	seed = 0;
	if ( freq )
		*freq = 255u;
	if ( !s.haveWeather )
		return false;
	const WwWeatherData & w = s.w;
	const bool changing = w.blending();
	const WwWeatherData * L = changing ? w.last.get() : nullptr;
	bool stormNow = w.thunderSounds > 0, stormLast = L && L->thunderSounds > 0;
	/* lane AIRPERF1 A9 (bungo 11-11): under the entry's physicalWeather switch a lightning.frequency written in the
	 * file says alone whether the weather flashes (> 0), no longer the record's thunder sound. Reds
	 * WW_WEATHER_RECX_RED=noswitch-lightning (the reader clears stormAlone) and WW_WEATHER_RECORD_RED=lightning */
	if ( !wwWeatherRecordRed( WwRecLightning ) ) {
		const WwWeatherRecx & wxl = wwWeatherRecx();
		if ( wxl.on && wxl.stormAlone )
			stormNow = wxl.lightningFreqOwn > 0.0f;
		if ( wxl.on && L && wxl.stormAloneLast )
			stormLast = wxl.lightningFreqLast > 0.0f;
	}
	share = WwCloudLightning::stormShare( stormNow, float( w.thunderFadeIn ) / 255.0f, changing, stormLast,
		L ? float( L->thunderFadeOut ) / 255.0f : 1.0f, w.transPct );
	const WwWeatherData & src = ( stormNow || !stormLast ) ? w : *L;
	for ( int c = 0; c < 3; c++ )
		rgb[c] = float( src.lightningRgb[c] ) / 255.0f;
	seed = src.formID;
	if ( freq )	// lane LIGHTNING2 (LIGHTNINGFREQ1): the storm side's Thunder/Lightning - Frequency (cloudlightning.h)
		*freq = src.thunderFreq;
	/* lane WEATHERRECX1: the .recx owns the lightning (SPEC.md 1.3): lightning.frequency in flashes / min onto the byte
	 * law (cloudlightning.h freqChance: a strike per 2 s slot at (255 - byte) / 255, so 30 / min = byte 0), already
	 * blended over a change; the colour is a ~30 000 K plasma channel, never the record's DATA 12..14. Red
	 * WW_WEATHER_RECORD_RED=lightning: the record's byte and colour (as before) */
	if ( !wwWeatherRecordRed( WwRecLightning ) && wwWeatherRecx().lightningFreq >= 0.0f ) {
		if ( freq )
			*freq = unsigned( std::lround( 255.0f - 255.0f * std::min( 1.0f, wwWeatherRecx().lightningFreq / 30.0f ) ) );
		static const float kPlasma[3] = { 0.63f, 0.73f, 1.0f };
		for ( int c = 0; c < 3; c++ )
			rgb[c] = kPlasma[c];
	}
	return true;
}

bool wwLookdevWeatherLeaving( quint32 & formID, QString & edid, QString & owner, unsigned & classFlags, float & pct )
{
	resolve();
	const LdState & s = st();
	if ( !s.haveWeather || !s.w.blending() )
		return false;
	const WwWeatherData & L = *s.w.last;
	formID = L.formID;
	edid = L.edid;
	owner.clear();
	for ( const WwWeatherEntry & e : s.list )
		if ( e.formID == L.formID )
			owner = e.ownerFile;
	classFlags = L.hasClass ? L.classFlags : 0u;
	pct = s.w.transPct;
	return true;
}

// lane WEATHERRECX1: one record -> the generated rung's inputs (weathergen.h; colours and fog distances never read)
static WwWeatherGen::In weatherGenIn( const LdState & s, const WwWeatherData & w )
{
	WwWeatherGen::In in;
	in.edid = w.edid.toStdString();
	for ( const WwWeatherEntry & e : s.list )
		if ( e.formID == w.formID )
			in.owner = e.ownerFile.toStdString();
	in.flags = w.hasClass ? int( w.classFlags ) : 0;
	in.precipType = w.precipForm != 0 ? w.precipType : -1;
	in.precipDensity = w.precipDensity >= 0.0f ? double( w.precipDensity ) : 7.0;
	in.thunder = w.thunderSounds > 0;
	in.windSpeed = w.windSpeed;
	in.windDir = w.windDir;
	in.windTurb = w.windTurb;
	in.transDelta = w.transDelta;
	in.precipIn = w.precipFadeIn;
	in.precipOut = w.precipFadeOut;
	in.thunderIn = w.thunderFadeIn;
	in.thunderOut = w.thunderFadeOut;
	return in;
}

int wwLookdevWeatherGen( WwWeatherGen::In in[2], quint32 formID[2], float & pct )
{
	resolve();
	const LdState & s = st();
	pct = 1.0f;
	if ( !s.haveWeather )
		return 0;
	in[0] = weatherGenIn( s, s.w );
	formID[0] = s.w.formID;
	if ( !s.w.blending() )
		return 1;
	in[1] = weatherGenIn( s, *s.w.last );
	formID[1] = s.w.last->formID;
	pct = s.w.transPct;
	return 2;
}

bool wwLookdevWeatherRains( bool & cur, bool & blending, bool & old, float & pct )
{
	resolve();
	const LdState & s = st();
	cur = blending = old = false;
	pct = 1.0f;
	if ( !s.haveWeather )
		return false;
	cur = s.w.precipForm != 0;	// lane CLOUD6e: WTHR MNAM (esmweather.cpp)
	if ( s.w.blending() ) {
		blending = true;
		old = s.w.last->precipForm != 0;
		pct = s.w.transPct;
	}
	return true;
}

bool wwLookdevWeatherPrecip( int type[2], float density[2], unsigned flags[2], bool & blending, float & pct )
{
	resolve();
	const LdState & s = st();
	for ( int i = 0; i < 2; i++ ) {
		type[i] = -2;
		density[i] = -1.0f;
		flags[i] = 0u;
	}
	blending = false;
	pct = 1.0f;
	if ( !s.haveWeather )
		return false;
	auto one = []( const WwWeatherData & w, int & t, float & d, unsigned & f ) {	// lane RAINFOG2: MNAM -> SPGD (esmweather.cpp)
		t = w.precipForm ? w.precipType : -2;
		d = w.precipDensity;
		f = w.hasClass ? w.classFlags : 0u;
	};
	one( s.w, type[0], density[0], flags[0] );
	if ( s.w.blending() ) {
		blending = true;
		one( *s.w.last, type[1], density[1], flags[1] );
		pct = s.w.transPct;
	}
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
	const double was = s.hour;
	s.hour = std::clamp( h, 0.0, 23.99 );
	// lane WEATHERBLEND3: a new hour inside a change is a wait (PassTime): its hours move the change on, a pick snaps
	if ( s.w.blending() && s.hour != was && !s.pinHour && !weatherBlendRed( "nosnapevents" ) ) {
		// lane WEATHERBLEND4: the shortest way round; a backward drag waits nothing (red backwait: forward only, ~24 h)
		const double d = weatherBlendRed( "backwait" ) ? wwWeatherWaitHours( was, s.hour ) : wwWeatherHourStep( was, s.hour );
		if ( d > 0.0 ) {
			s.waitHours += d;
			s.snapFlags |= WwSnapPassTime;
			s.transMs = -1;
		}
	}
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
	float body[3];
	sunBodyDir( s, body );	// MODULES1 M6: the sun's own disc (the moon gets no sun-sized starburst)
	const bool red = wwSkyBodyDirRed();
	for ( int c = 0; c < 3; c++ ) {
		sunDir[c] = red ? L.sunDir[c] : body[c];
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
	if ( !wwCellClockLive() )
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
		head = QString( "weather=%1 [%2] src=%3 from=\"%4\" load=%5ms" ).arg( s.w.edid, hex8( s.w.formID ), s.w.srcFile,
			s.loadSource ).arg( s.loadMs );
		if ( s.w.blending() )	// lane WEATHERBLEND1
			head += QString( " change=%1->%2 pct=%3 delta=%4" ).arg( s.w.last->edid, s.w.edid ).arg( double( s.w.transPct ), 0, 'f', 4 )
				.arg( s.w.transDelta );
		if ( s.pending )	// lane WEATHERBLEND2
			head += QString( " pending=%1" ).arg( s.pending->edid );
		if ( s.accelFrom >= 0.0f )
			head += QString( " accel=%1" ).arg( double( s.accelFrom ), 0, 'f', 4 );
		if ( s.waitHours > 0.0 )	// lane WEATHERBLEND3
			head += QString( " wait=%1h" ).arg( s.waitHours, 0, 'f', 3 );
		if ( !s.snapLast.isEmpty() )
			head += QString( " event=%1" ).arg( s.snapLast );
		if ( !s.regionLast.isEmpty() )	// lane WEATHERBLEND4
			head += QString( " region=%1" ).arg( s.regionLast );
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

bool wwLookdevFogNow( WwFog & out )
{
	resolve();
	if ( !st().haveWeather )
		return false;
	out = currentFog();
	return true;
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
/* lane SKYLIVE1: the same weather name from another plugin (Plugin row, pinned list, cell load order) is another
 * record, so the keys below carry the load; red WW_CELL_SKYLIVE_RED=stale keys on the name alone (the old cache) */
QString loadKeyPart( const LdState & s )
{
	WW_SWITCH_STATIC( bool, stale, wwEnvStr( "WW_CELL_SKYLIVE_RED" ).split( ',' ).contains( QStringLiteral( "stale" ) ) );
	return stale ? QString() : QStringLiteral( "|load %1" ).arg( s.loadGen );
}

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
	o.key = QStringLiteral( "%1|%2|%3|%4" ).arg( s.weatherKey ).arg( a ).arg( b ).arg( double( t ), 0, 'g', 6 ) + loadKeyPart( s );
	if ( s.w.blending() ) {
		// lane WEATHERBLEND1: the weather left behind x (1 - pct), with its own one-key fallback
		const WwWeatherData & W = *s.w.last;
		int la = std::clamp( L.keys.a, 0, 7 ), lb = std::clamp( L.keys.b, 0, 7 );
		float lt = std::clamp( L.keys.t, 0.0f, 1.0f );
		const float p = s.w.transPct;
		if ( W.isFound[la] || W.isFound[lb] ) {
			if ( !W.isFound[la] ) {
				la = lb;
				lt = 0.0f;
			} else if ( !W.isFound[lb] ) {
				lb = la;
				lt = 0.0f;
			}
			for ( int k = 0; k < 9; k++ )
				o.hdr[k] = o.hdr[k] * p + ( W.isHdr[la][k] + ( W.isHdr[lb][k] - W.isHdr[la][k] ) * lt ) * ( 1.0f - p );
			for ( int k = 0; k < 3; k++ )
				o.cine[k] = o.cine[k] * p + ( W.isCine[la][k] + ( W.isCine[lb][k] - W.isCine[la][k] ) * lt ) * ( 1.0f - p );
			for ( int k = 0; k < 4; k++ )
				o.tint[k] = o.tint[k] * p + ( W.isTint[la][k] + ( W.isTint[lb][k] - W.isTint[la][k] ) * lt ) * ( 1.0f - p );
			// lane WEATHERBLEND2: the game weighs up to four LUTs (one per slot); the same path twice adds up
			float wt[4];
			wwImageSpaceSlots( t, lt, p, true, wt );
			const QString paths[4] = { s.w.isLut[a], s.w.isLut[b], W.isLut[la], W.isLut[lb] };
			for ( int k = 0; k < 4; k++ ) {
				if ( !( wt[k] > 0.0f ) || paths[k].isEmpty() )
					continue;
				const int at = int( o.lutMix.indexOf( paths[k] ) );
				if ( at < 0 ) {
					o.lutW[o.lutMix.size()] = wt[k];
					o.lutMix << paths[k];
				} else {
					o.lutW[at] += wt[k];
				}
			}
		}
		o.key += QStringLiteral( "|change %1 %2" ).arg( W.edid ).arg( double( p ), 0, 'g', 6 );
	}
	return true;
}
}	// namespace

float wwLookdevSunlightScale()
{
	WW_SWITCH_STATIC( bool, red, wwEnv( "WW_CELL_SUNSCALE_RED" ).trimmed() == "one" );
	WwLookdevIs is;
	if ( red || wwPhysSkyOn() || !wwLookdevImageSpace( is ) || !( is.hdr[6] > 0.0f ) )	// lane PHYSUNITS1: real units
		return 1.0f;
	return is.hdr[6];
}

float wwLookdevPhysLight()
{
	return wwPhysSkyOn() ? currentLight().physFog : 1.0f;
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
/* lane PERFFIX3 (item 2): the backdrop drawn INSIDE the cell's linear frame (WW_SKY_LINEAR=1): light in scene units
 * (exposure 1, view transform 3 = linear), so the cell's one tone map and its one exposure take the sky with the
 * surfaces. g_skyLinear while that draw runs; g_skyDeferred = the scene whose backdrop waits for the frame. */
bool g_skyLinear = false;
const void * g_skyDeferred = nullptr;
float skyExposure()
{
	return ( g_skyCapture || g_skyLinear ) ? 1.0f : wwSceneExposureScale();
}
int skyViewTransform()
{
	return ( g_skyCapture || g_skyLinear ) ? 3 : wwSceneViewTransform();
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
	wwSunPathGameHorizon( f.c.A, f.c.D );	// MODULES1 R6-6
	{	/* MODULES1 R6-12 b: the game's own discs for the Sun and Moon Size row: the record's quad (fSunBaseSize,
		 * iMasserSize over fSunXExtreme, as drawSunQuad / the game's moon draw them) x the share of the quad the art's
		 * disc takes (measured on the vanilla art, notes/modules1/r6/skytex_disc.py: sun.dds solid core, alpha over
		 * 240, 0.031 of the quad; masser_full.dds disc, alpha over half its peak, 0.83). */
		const float X = std::max( 1.0f, std::fabs( s.gmst.sunX ) );
		wwAtmoSetBodyGame( 0.031f * s.gmst.sunBase / X, 0.83f * s.gmst.masserSize / X );
	}
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
	if ( wwPhysSkyOn() ) {	// lane PHYSUNITS1: the weather's sky in the light's units (the drawn sky without Physical Sky)
		const float k = currentLight().physKey;
		f.skyScale *= k;
		for ( int c = 0; c < 3; c++ ) {
			f.glare[c] *= k;
			f.moonGlare[c] *= k;
		}
	}
	f.glareAlpha = f.c.sunAlpha * float( s.w.sunGlare ) / 255.0f;
	if ( s.w.blending() )	// lane WEATHERBLEND1: DATA byte 4 on pct (read in Todd's treat by WEATHERBLEND2)
		f.glareAlpha = f.c.sunAlpha * ( float( s.w.sunGlare ) * s.w.transPct + float( s.w.last->sunGlare ) * ( 1.0f - s.w.transPct ) ) / 255.0f;
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
	const WwSkyClock rawClock = wwSkyClock( s.hour, s.tnam, s.gmst );	// MODULES1 M6: the engine's night point
	const float * moonPos = real ? realPos : wwSkyBodyDirRed() ? f.c.sunPos : rawClock.sunPos;
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
	const WwSkyClock rawClock = wwSkyClock( s.hour, s.tnam, s.gmst );	// MODULES1 M6: the engine's night point
	const MoonGeo g = moonGeo( s, wwSkyBodyDirRed() ? f.c.sunPos : rawClock.sunPos );
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
			drew << QString( "moon:refused(%1 not found: NifSkope's own Data\\Textures\\Sky\\Moon, or a mod's) vanilla moon" ).arg( u ? texN : texD );
			return false;
		}
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		// lane SKYDISC1B: the limb's footprint is long radially (and the poles' along a parallel)
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, TexCache::get_max_anisotropy() );
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
	// MODULES1 R6-12 b: the Sun and Moon Size row scales the drawn globe (its radiance kept)
	const double moonTan = std::tan( wwskydisc::kMoonHalfDeg * wwskydisc::kRad ) * double( wwAtmoBodyScale( true ) );
	const double sinR = std::sin( std::atan( moonTan ) );
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
	prog->uni2f( "atRecNight", af.recNight[0], af.recNight[1] );	// MODULES1 R7: the sky behind the globe takes the same
	prog->uni1f( "atDomeAir", wwNightAirRed() ? 0.0f : af.recNight[0] );	// R7 night sky d: the globe over the record's dome
	const GLint tintLoc = prog->uniLocation( "atTint" );
	if ( ( af.tintOn || af.recNight[1] > 0.0f ) && tintLoc >= 0 )
		r->fn->glUniform3fv( tintLoc, 17, &af.tint[0][0] );
	prog->uni3f( "moonD", float( g.D[0] ), float( g.D[1] ), float( g.D[2] ) );
	prog->uni3f( "moonS", float( g.S[0] ), float( g.S[1] ), float( g.S[2] ) );
	prog->uni3f( "bodyX", float( ex[0] ), float( ex[1] ), float( ex[2] ) );
	prog->uni3f( "bodyY", float( ey[0] ), float( ey[1] ), float( ey[2] ) );
	prog->uni3f( "bodyZ", float( ez[0] ), float( ez[1] ), float( ez[2] ) );
	prog->uni1f( "sinR", float( sinR ) );
	prog->uni1f( "pxAng", float( pxAng ) );
	prog->uni1f( "albedoInv", float( 1.0 / wwskydisc::kAlbedoMean ) );
	// lane FIX1101A (moon, 24d(2)): the globe through the air as its light (gl/moonlight.h discAirGain; 1 = none)
	prog->uni1f( "sunTerm", float( t.sunTerm ) * af.moonDiscAir );
	prog->uni1f( "earthTerm", float( t.earthTerm ) * af.moonDiscAir );
	prog->uni1f( "alpha", alpha );
	prog->uni1f( "sceneExposure", exposure );
	prog->uni1i( "viewTransform", skyViewTransform() );
	const float c[3] = { float( g.D[0] ), float( g.D[1] ), float( g.D[2] ) };
	prog->uni3f( "centerWorld", c[0], c[1], c[2] );
	prog->uni1f( "halfSize", float( moonTan * 1.5 + 2.0 * pxAng ) );
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
	/* lane LIGHTNING2 (LIGHTNINGSKY1): a storm's flash on the layers, the volume's strike at the Low base
	 * (lightning_field.glsl; ldLtOn 0 = the layer as before). Units: Low's colour is scene units (the atmosphere's
	 * sunK); the weather's own colour x Sky Scale is 1 for a white sunlit layer (pi x skyScale) */
	float lowCol[3];
	const bool lowTier = wwCloudVolLowColour( lowCol );
	const WwAtmoFrame & atf = wwAtmoFrame();
	WwLightningSky lsky;
	wwLightningSky( scene, secs, lowTier && atf.valid ? atf.sunK : WwCloudVolMath::kPi * f.skyScale, lsky );
	auto flashUniforms = [&]( NifSkopeOpenGLContext::Program * prog, bool on ) {
		prog->uni1i( "ldLtOn", on ? 1 : 0 );	// every draw: a program keeps its uniforms between draws
		if ( !on )
			return;
		prog->uni4f( "ldLtSrc", FloatVector4( lsky.src[0], lsky.src[1], lsky.src[2], lsky.src[3] ) );
		prog->uni4f( "ldLtPrm", FloatVector4( lsky.prm[0], lsky.prm[1], lsky.prm[2], lsky.prm[3] ) );
		prog->uni3f( "ldLtCol", lsky.col[0], lsky.col[1], lsky.col[2] );
		prog->uni4f( "ldLtCam", FloatVector4( lsky.camKm[0], lsky.camKm[1], lsky.camKm[2], lsky.deckKm ) );
	};
	/* lane WXBAND (WEATHER-HORIZON-BAND): the sky's aerial perspective to the deck (lookdev_clouds.frag ldApOn), the
	 * Low base's shell; red WW_LOOKDEV_RED=cloudnoap = the un-hazed layer that met the horizon band with a seam */
	const float apDeckR = WwAtmoMath::kRb + wwCloudLayerDeckKm();
	const bool apOn = atf.valid && atf.trans != 0 && atf.camR < apDeckR && !wwLookdevRed( "cloudnoap" );
	/* MODULES1 10c: the Fusion Shaders cloud scatter law on the layers (cloudfusionlaw.h, lookdev_clouds.frag
	 * ldFsOn): the key light's direction and elevation gate, the weather class's response (during a change the two
	 * classes by the change's share). A share of the layer's own lit colour: no light is added that the layer did
	 * not have. Red WW_LOOKDEV_RED=cloudnoscatter = the layer as before. */
	WwCloudFusionLaw::Scatter fs;
	float fsTo[3] = { 0.0f, 0.0f, 1.0f };
	QString fsCls;
	const bool fsRed = wwLookdevRed( "cloudnoscatter" );
	{
		float disc[3];
		wwLookdevShadowLight( fsTo, disc );
		const float len = std::sqrt( fsTo[0] * fsTo[0] + fsTo[1] * fsTo[1] + fsTo[2] * fsTo[2] );
		for ( int c = 0; c < 3; c++ )
			fsTo[c] = len > 0.0f ? fsTo[c] / len : ( c == 2 ? 1.0f : 0.0f );
		WwWeatherGen::In gin[2];
		quint32 gid[2] = {};
		float gpct = 1.0f;
		const int gn = wwLookdevWeatherGen( gin, gid, gpct );
		WwCloudFusionLaw::ScatterRow row = WwCloudFusionLaw::scatterRow( nullptr );
		fsCls = QStringLiteral( "none" );
		if ( gn >= 1 ) {
			const char * c0 = WwWeatherGen::classify( gin[0].edid, gin[0].flags, gin[0].precipType, gin[0].thunder );
			row = WwCloudFusionLaw::scatterRow( c0 );
			fsCls = QString::fromLatin1( c0 );
			if ( gn >= 2 ) {
				const char * c1 = WwWeatherGen::classify( gin[1].edid, gin[1].flags, gin[1].precipType, gin[1].thunder );
				const WwCloudFusionLaw::ScatterRow old = WwCloudFusionLaw::scatterRow( c1 );
				const float p = std::clamp( gpct, 0.0f, 1.0f );
				row.scale = old.scale + ( row.scale - old.scale ) * p;
				row.warmth = old.warmth + ( row.warmth - old.warmth ) * p;
				fsCls += QChar( '<' ) + QString::fromLatin1( c1 );
			}
		}
		fs = WwCloudFusionLaw::scatter( float( std::asin( std::clamp( fsTo[2], -1.0f, 1.0f ) ) * 57.29577951 ), row );
	}
	const bool fsOn = !fsRed && fs.rim + fs.body > 0.0f;
	auto scatterUniforms = [&]( NifSkopeOpenGLContext::Program * prog, bool on ) {
		prog->uni1i( "ldFsOn", on ? 1 : 0 );	// every draw: a program keeps its uniforms between draws
		if ( !on )
			return;
		prog->uni4f( "ldFsSun", FloatVector4( fsTo[0], fsTo[1], fsTo[2], WwCloudFusionLaw::kScatterG ) );
		prog->uni4f( "ldFsRb", FloatVector4( fs.rim, fs.body, WwCloudFusionLaw::kScatterCeiling, 0.0f ) );
		prog->uni3f( "ldFsTint", fs.tint[0], fs.tint[1], fs.tint[2] );
	};
	auto apUniforms = [&]( NifSkopeOpenGLContext::Program * prog, bool on ) {
		prog->uni1i( "ldApOn", on ? 1 : 0 );	// every draw: a program keeps its uniforms between draws
		if ( !on )
			return;
		r->fn->glActiveTexture( GL_TEXTURE2 );
		r->fn->glBindTexture( GL_TEXTURE_2D, atf.trans );
		r->fn->glActiveTexture( GL_TEXTURE0 );
		prog->uni1i( "ldTrans", 2 );
		prog->uni2f( "ldApR", atf.camR, apDeckR );
	};
	// lane WEATHERBLEND1: W = the weather whose texture and scroll a pass draws (s.w, or the one a change leaves)
	auto layerUniforms = [&]( NifSkopeOpenGLContext::Program * prog, const WwWeatherData & W, int i, float alpha ) {
		float rgb[3], a = 0.0f;
		EsmWeather::blendCloud( s.w, i, f.c.keys, rgb, &a );
		float lc[3];	// lane CLOUD2: Volume Clouds Low lights the layers by the model (sky units / skyScale)
		if ( wwCloudVolLowColour( lc ) )
			prog->uni3f( "colLin", lc[0] / std::max( f.skyScale, 1e-6f ), lc[1] / std::max( f.skyScale, 1e-6f ),
				lc[2] / std::max( f.skyScale, 1e-6f ) );
		else
			prog->uni3f( "colLin", lin( rgb[0] ), lin( rgb[1] ), lin( rgb[2] ) );
		prog->uni1f( "alpha", alpha < 0.0f ? a : alpha );
		prog->uni2f( "uvOffset", cloudScroll( s, W, i, 0, secs ), cloudScroll( s, W, i, 1, secs ) );	// WEATHERBLEND3: one scroll
		prog->uni1f( "skyScale", f.skyScale );
		prog->uni1f( "sceneExposure", skyExposure() );
		prog->uni1i( "viewTransform", skyViewTransform() );
		return a;
	};
	auto bindLayer = [&]( const WwWeatherData & W, int i, int unit = 0 ) {
		if ( i < 0 || i >= 32 || W.cloudTex[i].isEmpty() )
			return false;
		r->fn->glActiveTexture( GL_TEXTURE0 + unit );
		const bool ok = scene->bindTexture( QStringView( skyTexPath( W.cloudTex[i] ) ), true ) > 0;
		if ( ok ) {
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		}
		if ( unit )
			r->fn->glActiveTexture( GL_TEXTURE0 );
		return ok;
	};

	if ( s.probe ) {
		// the texel probe: one full-screen quad samples layer probeLayer at probeUV + the scroll offset
		NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_clouds.prog" );
		if ( !prog || !bindLayer( s.w, s.probeLayer ) ) {
			if ( prog )
				r->stopProgram();
			drew << QString( "cloudprobe:refused(layer %1 texture %2)" ).arg( s.probeLayer ).arg( s.w.cloudTex[s.probeLayer & 31] );
			return;
		}
		skyState( r, false, false );
		prog->uni1i( "Tex", 0 );
		prog->uni1i( "Tex2", 1 );
		prog->uni1f( "texBlend", 0.0f );
		prog->uni1b( "probe", true );
		flashUniforms( prog, false );
		apUniforms( prog, false );
		scatterUniforms( prog, false );
		prog->uni2f( "probeUV", s.probeUV[0], s.probeUV[1] );
		const float a = layerUniforms( prog, s.w, s.probeLayer, -1.0f );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		const float * attrs = quad;
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
		drew << QString( "cloudprobe:layer=%1,alpha=%2,t=%3" ).arg( s.probeLayer ).arg( f3( a ) ).arg( secs, 0, 'f', 3 );
		return;
	}

	/* lane WEATHERBLEND2: during a change a layer draws ONCE, the left weather's texture cross-faded to the new one's by
	 * pct, each at its own scroll (the game's layer: two textures and a factor); a layer one weather lacks fades by share */
	const float pct = s.w.transPct;
	for ( int i = 0; i < ms->size() && i < 32; i++ ) {
		const SkyMesh & m = ms->at( i );
		const bool hasNew = !( s.w.cloudDisabled & ( 1u << i ) );
		const bool hasOld = s.w.blending() && !( s.w.last->cloudDisabled & ( 1u << i ) );
		if ( !hasNew && !hasOld )
			continue;
		bool fade = hasNew && hasOld;
		const WwWeatherData * W = hasNew ? &s.w : s.w.last.get();
		float share = !s.w.blending() || fade ? 1.0f : ( hasNew ? pct : 1.0f - pct );
		QString tag = fade ? QStringLiteral( "%1x" ).arg( i ) : ( hasNew ? QString::number( i ) : QStringLiteral( "%1'" ).arg( i ) );
		float rgb[3], a = 0.0f;
		EsmWeather::blendCloud( s.w, i, f.c.keys, rgb, &a );
		if ( full && a * share <= 0.0f )
			continue;
		if ( m.idx.isEmpty() ) {
			skipped << QString( "%1(no mesh)" ).arg( tag );
			continue;
		}
		NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_clouds.prog" );
		if ( !prog ) {
			drew << QStringLiteral( "clouds:refused(lookdev_clouds.prog did not link)" );
			return;
		}
		if ( fade ) {	// the left weather's texture is the base, the new one's the blend target
			const bool okOld = bindLayer( *s.w.last, i, 0 ), okNew = bindLayer( s.w, i, 1 );
			if ( okOld && okNew ) {
				W = s.w.last.get();
			} else {	// one side did not resolve: the other alone at its share
				fade = false;
				W = okNew ? &s.w : s.w.last.get();
				share = okNew ? pct : 1.0f - pct;
				tag = okNew ? QString::number( i ) : QStringLiteral( "%1'" ).arg( i );
				if ( ( !okOld && !okNew ) || ( okNew && !bindLayer( s.w, i ) ) ) {
					r->stopProgram();
					skipped << QString( "%1(%2 did not resolve)" ).arg( tag ).arg( skyTexPath( W->cloudTex[i] ) );
					continue;
				}
			}
		} else if ( !bindLayer( *W, i ) ) {
			r->stopProgram();
			skipped << QString( "%1(%2 did not resolve)" ).arg( tag ).arg( skyTexPath( W->cloudTex[i] ) );
			continue;
		}
		a *= share;
		skyState( r, true, false );
		prog->uni1i( "Tex", 0 );
		prog->uni1i( "Tex2", 1 );
		prog->uni1b( "probe", false );
		layerUniforms( prog, *W, i, full ? a : mul * share );
		prog->uni1f( "texBlend", fade ? pct : 0.0f );
		flashUniforms( prog, lsky.on );
		apUniforms( prog, apOn );
		scatterUniforms( prog, fsOn );
		if ( fade )	// lane WEATHERBLEND3: the same one scroll (red cloudtwo: the new weather's own)
			prog->uni2f( "uvOffset2", cloudScroll( s, s.w, i, 0, secs ), cloudScroll( s, s.w, i, 1, secs ) );
		const float * attrs[8] = { m.pos.constData(), m.col.constData(), nullptr, nullptr, nullptr, nullptr, nullptr,
			m.uv.constData() };
		r->drawShape( m.nv, 0x20000043ULL, unsigned( m.idx.size() ), GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs,
			m.idx.constData() );
		r->stopProgram();
		drawn << tag;
	}
	drew << QString( "clouds:drawn=%1%2 t=%3" ).arg( drawn.isEmpty() ? QStringLiteral( "none" ) : drawn.join( QChar( ',' ) ),
		skipped.isEmpty() ? QString() : QStringLiteral( " skipped=" ) + skipped.join( QChar( ',' ) ) )
		.arg( secs, 0, 'f', 3 );
	if ( lsky.on && !drawn.isEmpty() )	// lane LIGHTNING2: the flash the layers took
		drew.last() += QStringLiteral( " flash=%1" ).arg( double( lsky.level ), 0, 'f', 3 );
	if ( !drawn.isEmpty() )	// lane WXBAND: the layers' aerial perspective and its deck
		drew.last() += apOn ? QStringLiteral( " ap=on(deck=%1km)" ).arg( double( apDeckR - WwAtmoMath::kRb ), 0, 'f', 2 )
			: QStringLiteral( " ap=off" );
	if ( !drawn.isEmpty() )	// MODULES1 10c: the layers' scatter law and what fed it
		drew.last() += fsRed ? QStringLiteral( " scatter=off(red=cloudnoscatter)" )
			: QStringLiteral( " scatter=%1(fusion:rim=%2,body=%3,g=%4,cap=%5,gate=%6,class=%7:scale=%8:warmth=%9" )
				.arg( fsOn ? QStringLiteral( "on" ) : QStringLiteral( "off" ) ).arg( double( fs.rim ), 0, 'f', 3 )
				.arg( double( fs.body ), 0, 'f', 3 ).arg( double( WwCloudFusionLaw::kScatterG ), 0, 'f', 2 )
				.arg( double( WwCloudFusionLaw::kScatterCeiling ), 0, 'f', 2 ).arg( double( fs.gate ), 0, 'f', 3 )
				.arg( fsCls ).arg( double( fs.scale ), 0, 'f', 2 ).arg( double( fs.warmth ), 0, 'f', 2 )
			+ QStringLiteral( ",tint=%1/%2/%3,of=the_layer's_own_lit_colour)" ).arg( double( fs.tint[0] ), 0, 'f', 2 )
				.arg( double( fs.tint[1] ), 0, 'f', 2 ).arg( double( fs.tint[2] ), 0, 'f', 2 );
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
	/* lane WEATHERBLEND3: during a change ONE entry per layer, the left weather's texture cross-faded to the new one's by
	 * pct at the layer's one scroll (the game's layer); red cloudtwo: one entry per weather at its share (WEATHERBLEND1) */
	const bool two = s.w.blending() && weatherBlendRed( "cloudtwo" );
	const int nSrc = two ? 2 : 1;
	const float pct = s.w.transPct;
	const WwWeatherData * O = ( s.w.blending() && !two ) ? s.w.last.get() : nullptr;
	for ( int i = 0; i < n; i++ ) {
		for ( int src = nSrc - 1; src >= 0; src-- ) {
			const WwWeatherData & W = src ? *s.w.last : s.w;
			const bool hasW = !( W.cloudDisabled & ( 1u << i ) ) && !W.cloudTex[i].isEmpty();
			const bool hasO = O && !( O->cloudDisabled & ( 1u << i ) ) && !O->cloudTex[i].isEmpty();
			if ( !hasW && !hasO )
				continue;
			WwCloudLayerIn L;
			L.layer = i;
			EsmWeather::blendCloud( s.w, i, f.c.keys, L.rgb, &L.alpha );
			if ( two )
				L.alpha *= src ? 1.0f - pct : pct;
			else if ( O && !( hasW && hasO ) )
				L.alpha *= hasW ? pct : 1.0f - pct;
			if ( L.alpha <= 0.0f )
				continue;
			L.tex = skyTexPath( hasO ? O->cloudTex[i] : W.cloudTex[i] );
			if ( hasW && hasO ) {
				L.tex2 = skyTexPath( W.cloudTex[i] );
				L.texBlend = pct;
			}
			L.uvOffset[0] = cloudScroll( s, W, i, 0, secs );
			L.uvOffset[1] = cloudScroll( s, W, i, 1, secs );
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
	prog->uni1i( "ldFsOn", 0 );	// MODULES1 10c: the stars take no cloud scatter (the program keeps its uniforms)
	// lane WX1: night.stars of the weather's .recx, a multiple of the weather's star colour
	const WwWeatherRecx & wx = wwWeatherRecx();
	const float starK = wx.on && wx.stars >= 0.0f ? wx.stars : 1.0f;
	if ( starK != 1.0f )
		prog->uni3f( "colLin", f.stars[0] * starK, f.stars[1] * starK, f.stars[2] * starK );
	else
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
	drew << QString( "stars:%1(alpha=%2,turn=%3deg,level=%4)" ).arg( path, f3( a ) ).arg( double( ang ) * 360.0 / kTau, 0, 'f', 1 )
		.arg( double( ( 0.2126f * f.stars[0] + 0.7152f * f.stars[1] + 0.0722f * f.stars[2] ) * starK * f.skyScale ), 0, 'g', 3 );
}

/* MODULES1 M6: every user of the sun's and the moon's direction this frame, azimuth/elevation in degrees, printed when
 * one moves 0.1 degree: the sky's disc, the body, the lens, the key light, the engine's point; the globe, the sky's
 * moonlight. The moon's own lens would be the sun's x its light over the sun's (under 1/255: none is drawn). */
void skyBodiesLine( const SkyFrame & f )
{
	LdState & s = st();
	auto ae = []( const float * v ) {
		const double l = std::sqrt( double( v[0] ) * v[0] + double( v[1] ) * v[1] + double( v[2] ) * v[2] );
		if ( !( l > 1e-9 ) )
			return QStringLiteral( "none" );
		return QStringLiteral( "%1/%2" ).arg( std::atan2( double( v[0] ), double( v[1] ) ) / wwskydisc::kRad, 0, 'f', 1 )
			.arg( std::asin( std::clamp( double( v[2] ) / l, -1.0, 1.0 ) ) / wwskydisc::kRad, 0, 'f', 1 );
	};
	const WwSkyClock raw = wwSkyClock( s.hour, s.tnam, s.gmst );
	{	// MODULES1 R6-6: which clock each body used, once per change
		static QString saidClock;
		const QString cl = wwSunPathClockEcho();
		if ( cl != saidClock ) {
			saidClock = cl;
			fprintf( stderr, "%s\n", qPrintable( cl ) );
		}
	}
	float body[3], lens[3], tint[3];
	sunBodyDir( s, body );
	const WwLens * lensRec = nullptr;
	const bool lensOn = wwLookdevSunLens( lensRec, lens, tint );
	const LdLight L = currentLight();
	const WwAtmoFrame & af = wwAtmoFrame();
	const MoonGeo g = moonGeo( s, wwSkyBodyDirRed() ? f.c.sunPos : raw.sunPos );
	const float gd[3] = { float( g.D[0] ), float( g.D[1] ), float( g.D[2] ) };
	const double moonOverSun = double( wwphys::kMoonToaLux ) / double( wwphys::kSunToaLux ) * double( moonLightFactor( s ) );
	const QString line = QStringLiteral( "sky bodies (az/elev deg): sun disc %1 body %2 lens %3 light %4 engine %5 | moon globe %6 (%7) sky %8 | moon lens x%9 of the sun's, none drawn | red %10" )
		.arg( af.valid ? ae( af.sunW ) : QStringLiteral( "none" ), ae( body ), lensOn ? ae( lens ) : QStringLiteral( "off" ),
			ae( L.sunDir ), ae( raw.sunPos ), ae( gd ), g.real ? QStringLiteral( "real" ) : QStringLiteral( "engine night point" ),
			af.valid ? ae( af.moonW ) : QStringLiteral( "none" ) )
		.arg( moonOverSun, 0, 'g', 3 ).arg( wwSkyBodyDirRed() ? 1 : 0 );
	static QString said;
	if ( line != said ) {
		said = line;
		fprintf( stderr, "%s\n", qPrintable( line ) );
	}
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
	sunBodyDir( s, in.sunW );	// MODULES1 M6: the one sun direction (its lens takes the same)
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
	const bool phys = wwPhysSkyOn();	// lane PHYSUNITS1: the top-of-atmosphere sun and moon over pi (atmosphere.cpp)
	in.sunLum = phys ? wwphys::kSunToaLux / wwphys::kPi : keyLum( WwTodDay ) * scale;
	in.moonLum = ( phys ? wwphys::kMoonToaLux / wwphys::kPi : keyLum( WwTodNight ) * scale ) * moonLightFactor( s );
	if ( wwWeatherRecx().on && wwWeatherRecx().moonlight >= 0.0f )
		in.moonLum *= wwWeatherRecx().moonlight;	// lane WX1: night.moonlight, the glow round the moon
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
	in.camFogZ = in.camZ - s.groundZ;	// lane FIX1101A (moon): the air's frame (lookdev_fog.glsl fogView.w)
	in.disc = s.sun && !capture;	// lane SKYA (SKYGI1): the probes' capture has no disc (the sun is the bake's own term)
	in.capture = capture;
	in.discs = wwAtmoDiscs() && !capture;	// lane SKYDISC1: our sun disc
	in.exposure = skyExposure();
	in.viewTransform = skyViewTransform();
	{	// MODULES1 R7: the record's night sky (computeLight): master Off = a crossfade to the dome's colours, On = added
		const LdLight nl = currentLight();
		in.recNight[0] = phys ? 0.0f : nl.nightW;
		in.recNight[1] = nl.nightW;
	}
	// lane SKYA (ATMOCHECK1): the weather dome's colors by elevation for the Sky Color row's Weather Tint; the rings come
	// from the dome mesh's vertex weights once per mesh, the colors from this hour's weather each frame
	{	// LATE 7: a glowing record sky (the radstorms) draws Weather Tint (atmosphere.h wwAtmoSetGlowSky)
		const SkyFrame gf = skyFrame();
		const float * u = gf.up;
		wwAtmoSetGlowSky( u[1] > 0.0f && u[1] >= u[0] && u[1] >= u[2] && u[2] < 0.75f * u[1] && u[2] < u[0] );
	}
	if ( wwAtmoTintDrawn() > 0 || in.recNight[1] > 0.0f ) {
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

bool wwLookdevSkyLinearOn()
{
	/* MODULES1 item 18: on by default. The sky under the frame took the menu's exposure, not the camera's, so wherever
	 * the air no longer covered it (a camera above ~200 m) it showed blown white; in the frame it takes the one exposure
	 * and tone map. Red WW_SKY_LINEAR=0: the sky in the view under the frame, as before. */
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_SKY_LINEAR" ).trimmed() != QLatin1StringView( "0" ) );
	return on;
}

namespace
{
/* lane PERFFIX3 (item 2): the backdrop waits for the cell's linear frame only where that frame will run (a cell-lit
 * document, a perspective view, not a probe/capture/selection pass) and the backdrop is a sky (an interior without
 * Show Sky keeps its black clear in the view, as before) */
bool skyLinearWanted( Scene * scene )
{
	if ( g_skyLinear || g_skyCapture || !wwLookdevSkyLinearOn() || !scene || !scene->renderer || scene->selecting )
		return false;
	if ( scene->renderer->globalUniforms->projectionMatrix[3][3] == 1.0f || scene->hasVisMode( Scene::VisSilhouette ) )
		return false;
	if ( !wwCellImageSpaceWanted( scene ) || wwCellProbePass( scene ) )
		return false;
	const WwCellLighting * cl = scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
	return !( cl && cl->interior && !cl->showSky );
}
}	// namespace

bool wwLookdevDrawBackground( Scene * scene )
{
	if ( skyLinearWanted( scene ) ) {
		g_skyDeferred = scene;	// the frame draws it (wwLookdevSkyLinearDraw); the view gets the caller's clear
		return false;
	}
	wwAtmoApFrame();	// lane SKYA: last frame's aerial perspective is stale (drawAtmo builds this frame's)
	wwLightningFrameBegin();	// lane LIGHTNING2: no sky pass this frame = no flash on the cell's ambient
	if ( !scene || !scene->renderer || scene->selecting || scene->hasVisMode( Scene::VisSilhouette ) )
		return false;
	Renderer * r = scene->renderer;
	LdState & sx = st();
	const WwCellLighting * icl = scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
	sx.interior = icl && icl->interior;	// lane WEATHERBLEND2: the next pick snaps in an interior (the game's sky mode)
	if ( sx.interior && wwCellSkyLitNow( icl ) && !( wwIntSkyRed() & 1 ) )
		sx.interior = false;	// lane INTSKY1: a live-weather interior runs the full sky mode: its weather changes blend
	if ( scene->nifModel != sx.lastDoc ) {	// lane WEATHERBLEND3: another cell is a teleport (PositionPlayer: PassTime)
		if ( sx.lastDoc ) {
			sx.snapFlags |= WwSnapPassTime;
			sx.regionDoc = scene->nifModel;	// lane WEATHERBLEND4: and its region's weather
		}
		sx.lastDoc = scene->nifModel;
		sx.walk = WwRegionWalk();	// lane WEATHERBLEND6: another document: the walk's baseline again
	}
	regionWalk( sx, scene->nifModel );	// lane WEATHERBLEND6: a few samples a second at most, after a move
	const bool anyPart = sx.sky || sx.sun || sx.clouds || sx.moon
		|| wwLookdevRed( "skyleak" ) || wwLookdevRed( "sunleak" ) || wwLookdevRed( "cloudleak" ) || wwLookdevRed( "moonleak" );
	const bool perspective = r->globalUniforms->projectionMatrix[3][3] != 1.0f;
	if ( anyPart && perspective ) {
		resolve();
		QStringList drew;
		// lane SKYFULL1: the game draws no sky in an interior unless its cell has Show Sky (DATA bit 7); with the
		// Sky row on, such an interior gets the black clear and no pass at all (red "interiorsky": the sky leaks in)
		const WwCellLighting * cl = scene->nifModel ? wwCellLightsFor( scene->nifModel ) : nullptr;
		// lane INTSKY1: a sunlit interior (a .recx Cells `sunlight` on a cell without Show Sky) draws the real sky too
		if ( sx.sky && cl && cl->interior && !cl->showSky && !wwCellSunlitNow( cl ) && !wwLookdevRed( "interiorsky" ) ) {
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
		skyBodiesLine( f );	// MODULES1 M6: one direction per body, every user named
		if ( wwCloudMapTier() > 0 )
			wwCloudMapUpdate( scene );	// lane CLOUD1: Off builds nothing
		if ( g_skyLinear )
			drew << QStringLiteral( "linear frame(one tone map)" );	// lane PERFFIX3
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

void wwLookdevSkyLinearDraw( Scene * scene, bool hdr )
{
	if ( !scene || g_skyDeferred != scene )
		return;
	g_skyDeferred = nullptr;
	if ( !hdr ) {	// the frame was refused after all: the backdrop into the view as before (display space)
		wwLookdevDrawBackground( scene );
		return;
	}
	// into the linear frame: every sky pixel marked as cell light (stencil 1), so the tone map's mode 0, the exposure
	// measure and the bloom take it; the depth stays at the far plane (every backdrop pass draws with the depth off)
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	GLint sFunc = GL_ALWAYS, sRef = 0, sMask = 0xFF, sWrite = 0xFF, sFail = GL_KEEP, sZFail = GL_KEEP, sPass = GL_KEEP;
	fn->glGetIntegerv( GL_STENCIL_FUNC, &sFunc );
	fn->glGetIntegerv( GL_STENCIL_REF, &sRef );
	fn->glGetIntegerv( GL_STENCIL_VALUE_MASK, &sMask );
	fn->glGetIntegerv( GL_STENCIL_WRITEMASK, &sWrite );
	fn->glGetIntegerv( GL_STENCIL_FAIL, &sFail );
	fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &sZFail );
	fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &sPass );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	fn->glEnable( GL_STENCIL_TEST );
	fn->glStencilMask( 0x03 );
	fn->glStencilFunc( GL_ALWAYS, 1, 0xFF );
	fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	g_skyLinear = true;
	const bool drew = wwLookdevDrawBackground( scene );
	g_skyLinear = false;
	/* lane SKYSEA1 (item 1): the physical sky's quad draws with the stencil test off (atmosphere.cpp wwAtmoDrawSky), so
	 * only the passes after it (the air over the sky, the cloud layers, the sun) marked their pixels. With the air off or
	 * refused (an empty cell, no fog records, the fog row off) the rest kept stencil 0, the tone map took them in no mode
	 * and the view's clear colour showed through as flat grey polygons. Nothing but the backdrop has drawn into the frame
	 * yet, so every pixel is a sky pixel: mark them all 1. Red WW_SKY_LINEAR_RED=nomark: only what the passes marked. */
	WW_SWITCH_STATIC( bool, noMark, wwEnv( "WW_SKY_LINEAR_RED" ).trimmed() == QLatin1StringView( "nomark" ) );
	const bool mark = drew && !noMark;
	if ( mark ) {
		GLint clearS = 0;
		GLint maskS = 0xFF;
		fn->glGetIntegerv( GL_STENCIL_CLEAR_VALUE, &clearS );
		fn->glGetIntegerv( GL_STENCIL_WRITEMASK, &maskS );
		const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glStencilMask( 0x03 );
		fn->glClearStencil( 1 );
		fn->glClear( GL_STENCIL_BUFFER_BIT );
		fn->glClearStencil( clearS );
		fn->glStencilMask( GLuint( maskS ) );
		if ( wasScissor )
			fn->glEnable( GL_SCISSOR_TEST );
	}
	static int markSaid = -1;
	if ( drew && markSaid != int( mark ) ) {
		markSaid = int( mark );
		fprintf( stderr, "lookdev sky: %s\n", mark
			? "every sky pixel marked cell light (stencil 1 under the whole backdrop: no clear-colour holes; lane SKYSEA1)"
			: "sky pixels marked only where a later pass drew (red WW_SKY_LINEAR_RED=nomark: holes where the air is off)" );
	}
	fn->glStencilFunc( GLenum( sFunc ), sRef, GLuint( sMask ) );
	fn->glStencilMask( GLuint( sWrite ) );
	fn->glStencilOp( GLenum( sFail ), GLenum( sZFail ), GLenum( sPass ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( drew )
		wwCellHdrNoteSkyIn();	// the tone map takes the sky as cell light (no sky split); the air blends over it
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

// lane WATERUNDER5: the game's sky colour type 14 is this row, blended like every sky colour and kept as stored (no gamma)
bool wwLookdevWaterMult( float rgb[3] )
{
	if ( !st().haveWeather )
		return false;
	const SkyFrame f = skyFrame();
	float c[3];
	EsmWeather::blendRowLab( st().w, WwRowWaterMult, f.c.keys, c );
	for ( int k = 0; k < 3; k++ )
		rgb[k] = std::clamp( c[k] / 255.0f, 0.0f, 1.0f );
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
	if ( s.w.blending() ) {	// lane WEATHERBLEND1: the left weather's DOF x (1 - pct)
		const WwWeatherData & W = *s.w.last;
		const float p = s.w.transPct;
		for ( int i = 0; i < 4; i++ ) {
			a[i] = a[i] * p + W.dof[ka][i] * ( 1.0f - p );
			b[i] = b[i] * p + W.dof[kb][i] * ( 1.0f - p );
		}
		haveA = haveA || W.hasDof[ka];
		haveB = haveB || W.hasDof[kb];
	}
	t = f.c.keys.t;
	return true;
}

bool wwLookdevWind( float out[5] )
{
	const LdState & s = st();
	if ( !s.haveWeather || !s.w.hasWind )
		return false;
	out[0] = float( s.w.windSpeed );
	out[1] = float( s.w.windDir );
	out[2] = float( s.w.windDirRange );
	out[3] = float( s.w.windTurb );
	// lane WEATHERBLEND3: the game's random range offset, seeded per weather (red nowindoffset: 0)
	out[4] = weatherBlendRed( "nowindoffset" ) ? 0.0f : wwWindRangeOffset( s.w.windDir, s.w.windDirRange, s.w.formID );
	/* lane WEATHERRECX1: the .recx owns the wind (m/s, degrees FROM; blended over a change by weatherrecx.cpp) on the
	 * byte scale the consumers take; the record keeps only its direction range (the gusts' art). Red
	 * WW_WEATHER_RECORD_RED=wind: the record's bytes (as before) */
	if ( !wwWeatherRecordRed( WwRecWind ) && wwWeatherRecxWindBytes( out[0], out[1], out[3] ) )
		return true;
	if ( s.w.blending() && s.w.last->hasWind ) {	// lane WEATHERBLEND1: speed and turbulence blend on pct (the game's)
		const float p = s.w.transPct;
		out[0] = wwWeatherWindBlend( s.w.windSpeed, s.w.last->windSpeed, p );	// WEATHERBLEND3: unrounded
		out[3] = wwWeatherWindBlend( s.w.windTurb, s.w.last->windTurb, p );
		if ( weatherBlendRed( "windbytes" ) ) {	// red: rounded to bytes (the old path)
			out[0] = float( std::lround( out[0] ) );
			out[3] = float( std::lround( out[3] ) );
		}
	}
	return true;
}

bool wwLookdevWeatherChanging()
{
	return bool( st().w.last );
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
		.arg( s.gameDay, 0, 'f', 2 ).arg( int( s.sky ) ).arg( int( s.clouds ) ).arg( s.haveWeather ? 1 : 0 ) + loadKeyPart( s )
		// lane SKYA (SKYGI1): only while the probes take the physical sky (Off: today's key, character for character)
		+ ( wwAtmoGiActive() ? QStringLiteral( "|atmo gi tier %1 tint %2 red %3" ).arg( wwAtmoTier() ).arg( wwAtmoTint() )
			.arg( wwAtmoGiRedDome() ? 1 : 0 ) : QString() )
		// lane WEATHERBLEND1: a change recaptures in 1% steps
		+ ( s.w.blending() ? QStringLiteral( "|change %1 %2" ).arg( s.w.last->edid ).arg( double( s.w.transPct ), 0, 'f', 2 ) : QString() );
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
