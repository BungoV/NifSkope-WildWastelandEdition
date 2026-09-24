R = 'E:/Projects/NifskopeWildWastelandEdition/'
P = R + 'src/gl/lookdevstage.cpp'
H = R + 'src/gl/lookdevstage.h'
s = open(P, 'rb').read().decode('utf-8')
h = open(H, 'rb').read().decode('utf-8')
cr0 = s.count('\r'); hr0 = h.count('\r')
snip = open(R + 'scratchpad/pbrwx1_20260924/p04_draw.cpp.txt', 'rb').read().decode('utf-8')

def rep(old, new, which='s'):
    global s, h
    t = s if which == 's' else h
    assert t.count(old) == 1, (which, old[:70], t.count(old))
    t = t.replace(old, new)
    if which == 's': s = t
    else: h = t

# ---------------- header
rep(r""" * the stored settings; nothing is stored while a pin holds the value. */""",
r""" * the stored settings; nothing is stored while a pin holds the value.
 *
 * The weather preview (lane PBRWX1): four rows of the Scene window, each ships
 * OFF and acts live -- Sky (the Atmosphere.nif dome in the WTHR sky colours,
 * CIELab-blended, x IMGS Sky Scale), Sun (the engine clock: the arc, the disc
 * and glare, and the lighting switched to it), Clouds (the WTHR layers on
 * Clouds.nif, scrolling in real seconds) and Moon (position, phase, texture;
 * no light), plus Game day (the moon phase). Pins: WW_LOOKDEV_SKY / _SUN /
 * _CLOUDS / _MOON = 0|1, WW_LOOKDEV_DAY=<days>, WW_LOOKDEV_CLOUDTIME=<s> (the
 * scroll clock; a harness run without it uses 0), WW_LOOKDEV_CLOUDPROBE=
 * <layer>,<u>,<v> (the cloud pass draws one texel probe instead of the
 * layers: left half the shaded colour, right half the alpha as grey).
 * Reds (WW_LOOKDEV_RED): skyswap, skygamma, skyscale, and the toggle leaks
 * skyleak, sunleak, cloudleak, moonleak (the OFF path still draws at 0.02).
 * Nothing here is HDR: every element goes out through studioOutput and is
 * blended in display space (the engine blends in HDR; ruling: no HDR yet). */""", 'h')

rep(r"""bool wwLookdevGround();
void wwLookdevSetGround( bool on );""", r"""bool wwLookdevGround();
void wwLookdevSetGround( bool on );
//! the weather preview rows (lane PBRWX1), all OFF by default
bool wwLookdevSky();
void wwLookdevSetSky( bool on );
bool wwLookdevSun();
void wwLookdevSetSun( bool on );
bool wwLookdevClouds();
void wwLookdevSetClouds( bool on );
bool wwLookdevMoon();
void wwLookdevSetMoon( bool on );
double wwLookdevGameDay();
void wwLookdevSetGameDay( double d );
//! seconds on the cloud scroll clock (the pin, 0 in a harness run, else real time since Clouds went on)
double wwLookdevCloudSeconds();""", 'h')

# ---------------- cpp includes + keys + state
rep(r"""#include <QDir>
#include <QFileInfo>
#include <QSettings>
""", r"""#include "gamemanager.h"
#include "harnesswindow.h"

#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSettings>
""")

rep(r"""const char * const kGroundKey = "Settings/Render/Scene/Lookdev Ground";
""", r"""const char * const kGroundKey = "Settings/Render/Scene/Lookdev Ground";
const char * const kSkyKey = "Settings/Render/Scene/Lookdev Sky";
const char * const kSunKey = "Settings/Render/Scene/Lookdev Sun";
const char * const kCloudsKey = "Settings/Render/Scene/Lookdev Clouds";
const char * const kMoonKey = "Settings/Render/Scene/Lookdev Moon";
const char * const kDayKey = "Settings/Render/Scene/Lookdev Game Day";
""")

rep(r"""	unsigned char tnam[4] = { 30, 54, 102, 126 };
	QString climateEdid;
""", r"""	unsigned char tnam[4] = { 30, 54, 102, 126 };
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
""")

rep(r"""		s.cube = qEnvironmentVariable( "WW_LOOKDEV_CUBE" ).trimmed();
		s.pinCube = !s.cube.isEmpty();
		if ( s.cube.isEmpty() )
			s.cube = QString::fromLatin1( kDefaultCube );
	}
	return s;""", r"""		s.cube = qEnvironmentVariable( "WW_LOOKDEV_CUBE" ).trimmed();
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
	return s;""")

# resolve: GMSTs + the climate data
rep(r"""			s.list = s.esm.list();
			QString why;
			if ( !s.esm.climate( 0, s.tnam, &s.climateEdid, &why ) ) {""", r"""			s.list = s.esm.list();
			s.gmst = s.esm.gmst();
			QString cwhy;
			if ( !s.esm.climateData( 0, s.clim, &cwhy ) )
				s.clim = WwClimateData();
			QString why;
			if ( !s.esm.climate( 0, s.tnam, &s.climateEdid, &why ) ) {""")

# currentLight: the engine clock when the Sun row is on
rep(r"""	LdLight L;
	wwVanillaSunLightDir( s.hour, s.tnam, L.sunDir );
	wwVanillaSunPos( s.hour, s.tnam, L.discPos );
	L.keys = wwTodKeys( s.hour, s.tnam );
	if ( !s.haveWeather ) {
		neutral( L.sun, L.amb, L.dalc );
		return L;
	}
	L.fromWeather = true;
	float rgb[3];
	EsmWeather::blendRow( s.w, WwRowSunlight, L.keys, rgb );
	for ( int c = 0; c < 3; c++ )
		L.sun[c] = lin( rgb[c] );
	EsmWeather::blendRow( s.w, WwRowAmbient, L.keys, rgb );""", r"""	LdLight L;
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
		EsmWeather::blendRow( s.w, WwRowAmbient, L.keys, rgb );""")

# setters / getters + settings load
rep(r"""QString wwLookdevCubePath()
{
	return st().cube;
}
""", r"""QString wwLookdevCubePath()
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
""")

rep(r"""	if ( !s.pinGround && settings.contains( QLatin1StringView( kGroundKey ) ) )
		s.ground = settings.value( QLatin1StringView( kGroundKey ) ).toBool();
}""", r"""	if ( !s.pinGround && settings.contains( QLatin1StringView( kGroundKey ) ) )
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
}""")

# summary: the weather preview tail, only when a part is on (the W1 line stays as it was)
rep(r"""		.arg( trip( L.dalc[5] ), cubeSource(), s.ground ? QStringLiteral( "on" ) : QStringLiteral( "off" ) );
}""", r"""		.arg( trip( L.dalc[5] ), cubeSource(), s.ground ? QStringLiteral( "on" ) : QStringLiteral( "off" ) )
		+ ( ( s.sky || s.sun || s.clouds || s.moon )
			? QString( " preview=sky:%1,sun:%2,clouds:%3,moon:%4 day=%5 cloudtime=%6 drew=%7" )
				.arg( s.sky ? 1 : 0 ).arg( s.sun ? 1 : 0 ).arg( s.clouds ? 1 : 0 ).arg( s.moon ? 1 : 0 )
				.arg( s.gameDay, 0, 'f', 2 ).arg( wwLookdevCloudSeconds(), 0, 'f', 2 ).arg( s.skyLast )
			: QString() );
}""")

# the background: dome or cube, then the sky passes
rep(r"""bool wwLookdevDrawBackground( Scene * scene )
{
	if ( !scene || !scene->renderer || scene->selecting || scene->hasVisMode( Scene::VisSilhouette ) )
		return false;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->useProgram( "lookdev_sky.prog" );
	if ( !prog )
		return false;
	const LdLight L = currentLight();""", snip + r"""
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
	const LdLight L = currentLight();""")

rep(r"""	prog->uni1b( "sunDiscUp", L.discPos[2] > 0.0f );""", r"""	prog->uni1b( "sunDiscUp", glow && L.discPos[2] > 0.0f );""")

rep(r"""	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	glDepthMask( GL_TRUE );
	return true;
}
""", r"""	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	r->stopProgram();
	glDepthMask( GL_TRUE );
	return true;
}
} // namespace
""")

assert s.count('\r') == cr0 and h.count('\r') == hr0
open(P, 'wb').write(s.encode('utf-8'))
open(H, 'wb').write(h.encode('utf-8'))
print('ok lookdev')
