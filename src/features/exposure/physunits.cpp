// lane PHYSUNITS1: real photometric units (physunits.h)
#include "features/exposure/physunits.h"
#include "features/lights/lightphys.h"
#include "wwswitch.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QSettings>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

constexpr const char * kLightsKey = "WW/GameLightBrightness";

struct PhysState
{
	QElapsedTimer cellFrame;
	quint64 cellPaint = 0;	// MODULES1 11-09 item 1: the paint that last drew the cell's HDR camera (0 = none yet)
	bool loaded = false;
	double g = 1.0;
	float evMetered = 0.0f, evApplied = 0.0f;
	float hiSpread = 0.0f;	// MODULES1 item 14: log2( 97th percentile / mean ) of the last meter
	QElapsedTimer frameMeter;	// the frame's measure last applied a meter (one camera writer)
	float incEv = 0.0f, dayW = 0.0f;	// MODULES1 item 14: the incident meter's EV100 and its daylight weight (0..1)
	float luxNormal = -1.0f, luxLevel = -1.0f;
	bool outdoor = false;		// the cell drawn is an exterior (or a sunlit interior)
};

PhysState & ps()
{
	static PhysState s;
	if ( !s.loaded ) {
		s.loaded = true;
		s.g = std::clamp( QSettings().value( QLatin1StringView( kLightsKey ), 1.0 ).toDouble(), 0.25, 4.0 );
	}
	return s;
}

bool pinnedFloat( const char * name, float & out )
{
	if ( !wwEnvIsSet( name ) )
		return false;
	bool ok = false;
	const float v = wwEnv( name ).trimmed().toFloat( &ok );
	if ( ok && std::isfinite( v ) )
		out = v;
	return ok;
}

}	// namespace

bool wwPhysOn()
{
	WW_SWITCH_STATIC( bool, red, wwSwitchEcho( "physunits red (the old pinned scale)", [] {
		const QByteArray v = wwEnvIsSet( "WW_PHYSUNITS_RED" ) ? wwEnv( "WW_PHYSUNITS_RED" ).trimmed() : QByteArray();
		return !v.isEmpty() && v != "0";
	}() ) );
	return !red;
}

quint64 wwGlPaintCount();	// glview.cpp

namespace {
/* MODULES1 11-09 item 1: one owner for "real units on". The old 1500 ms wall-clock window turned off after any frame
 * over 1.5 s (a GI rebuild): the next frame drew the legacy sun (scale 3.6), the sky moved, the GI rebuilt again, and
 * the sun flip-flopped every frame. On = the cell's HDR camera drew in one of the last 3 paints (any view), whatever
 * the frame took. Red WW_PHYSSKY_WINDOW_RED=1: the old window. */
bool windowRed()
{
	WW_SWITCH_STATIC( bool, red, wwSwitchEcho( "physunits sky window red (the 1500 ms clock window)", wwEnvInt( "WW_PHYSSKY_WINDOW_RED" ) == 1 ) );
	return red;
}
}	// namespace

void wwPhysNoteCellFrame()
{
	PhysState & s = ps();
	s.cellFrame.start();
	s.cellPaint = wwGlPaintCount();
}

bool wwPhysSkyOn()
{
	const PhysState & s = ps();
	if ( !wwPhysOn() || !s.cellFrame.isValid() )
		return false;
	if ( windowRed() )
		return s.cellFrame.elapsed() < 1500;
	return s.cellPaint != 0 && wwGlPaintCount() - s.cellPaint <= 3;
}

float wwPhysGameLights()
{
	if ( !wwPhysOn() )
		return 1.0f;
	float pin = 1.0f;
	if ( pinnedFloat( "WW_PHYSUNITS_LIGHTS", pin ) )
		return std::clamp( pin, 0.0f, 16.0f );
	return float( ps().g );
}

void wwPhysSetGameLights( double g )
{
	PhysState & s = ps();
	s.g = std::clamp( g, 0.25, 4.0 );
	QSettings().setValue( QLatin1StringView( kLightsKey ), s.g );
}

bool wwPhysGameLightsPinned()
{
	return wwEnvIsSet( "WW_PHYSUNITS_LIGHTS" );
}

float wwPhysEmitScale()
{
	if ( !wwPhysOn() )
		return 1.0f;
	/* lane PHYSUNITS1 (bungo 11-08 15:15, 11-09 15:26): an emissive unit is the frame's own luminance unit, the one the
	 * placed lamps light surfaces in, so emissives take the lamps' factor (5 lux edge law: 22.9 cd/m2 per unit) and keep
	 * vanilla's authored sign-to-street ratio; the 500 cd/m2 sign anchor put them 22x over the lamps and blew the bloom */
	/* lane MODULES1 item 5 (bungo 11-09: the Rexford sign shows no bloom): one unit = 107 cd/m2, the geometric middle of
	 * the lamp law and the 500 anchor. Reds: =1 the 500 anchor, =lamp the lamp law (22.9). */
	WW_SWITCH_STATIC( int, red, [] {
		const int r = wwEnvInt( "WW_PHYSUNITS_EMIT_RED" ) == 1 ? 1 : wwEnv( "WW_PHYSUNITS_EMIT_RED" ).trimmed() == "lamp" ? 2 : 0;
		wwSwitchEcho( r == 2 ? "physunits emit red (lamp law, 22.9 cd/m2 per unit)" : "physunits emit red (500 cd/m2 per unit)", r != 0 );
		return r;
	}() );
	float e = red == 1 ? wwphys::kEmitCdm2 : red == 2 ? wwphys::kLampEdgeLux / ( wwphys::kPi * wwlp::vanillaFlux() )
		: wwphys::kEmitMidCdm2;
	pinnedFloat( "WW_PHYSUNITS_EMIT", e );
	return std::max( e, 0.0f ) * wwPhysGameLights();
}

float wwPhysLampK( float radius )
{
	const float rm = radius / wwlp::kUnitsPerMetre;
	const float drawn = 4.0f * wwphys::kPi * wwphys::kPi * wwlp::vanillaFlux() * rm * rm;
	return drawn > 1e-9f ? wwphys::kLumenPerFade / drawn : 1.0f;
}

float wwPhysLighK( float radius )
{
	WW_SWITCH_STATIC( bool, red, wwSwitchEcho( "physunits lamp red (637 lm per fade)", wwEnvInt( "WW_PHYSUNITS_LAMP_RED" ) == 1 ) );
	if ( red )
		return wwPhysLampK( radius );
	float lux = wwphys::kLampEdgeLux;
	pinnedFloat( "WW_PHYSUNITS_LAMP_LUX", lux );
	return std::max( lux, 0.0f ) / ( wwphys::kPi * wwlp::vanillaFlux() );
}

namespace {
bool meterRed()
{
	WW_SWITCH_STATIC( bool, red, wwSwitchEcho( "physunits meter red (arithmetic mean)", wwEnvInt( "WW_PHYSUNITS_METER_RED" ) == 1 ) );
	return red;
}
}	// namespace

void wwPhysNoteFrameMeter()
{
	ps().frameMeter.start();
}

bool wwPhysFrameMeterFresh()
{
	const PhysState & s = ps();
	return !meterRed() && wwPhysSkyOn() && s.frameMeter.isValid() && s.frameMeter.elapsed() < 1000;
}

float wwPhysBloomK()
{
	WW_SWITCH_STATIC( bool, red, wwSwitchEcho( "physunits bloom red (additive FO4CS bloom)", wwEnvInt( "WW_PHYSUNITS_BLOOM_RED" ) == 1 ) );
	if ( red || !wwPhysOn() )
		return -1.0f;
	float k = wwphys::kGlareK;
	pinnedFloat( "WW_PHYSUNITS_BLOOM_K", k );
	return k > 0.0f ? std::min( k, 1.0f ) : -1.0f;
}

float wwPhysMeter( const float * px, int n, const unsigned char * stencil )
{
	if ( meterRed() || !wwPhysSkyOn() || !px || n <= 0 )
		return -1.0f;
	std::vector<std::pair<float, float>> t;	// (log2 luminance, cover)
	t.reserve( size_t( n ) );
	double cover = 0.0;
	for ( int i = 0; i < n; i++ ) {
		const float a = stencil ? ( ( stencil[i] & 1 ) != 0 ? 1.0f : 0.0f ) : px[size_t( i ) * 4 + 3];	// the pass: its stencil's cover
		if ( !( a > 0.0f ) )
			continue;
		const float l = 0.2125f * px[size_t( i ) * 4] + 0.7154f * px[size_t( i ) * 4 + 1] + 0.0721f * px[size_t( i ) * 4 + 2];
		if ( !std::isfinite( l ) )
			continue;
		t.emplace_back( std::log2( std::max( l, 1e-6f ) ), a );
		cover += a;
	}
	if ( t.empty() || !( cover > 0.0 ) )
		return -1.0f;
	std::sort( t.begin(), t.end() );
	const double lo = 0.02 * cover, hi = 0.98 * cover;
	double run = 0.0, sum = 0.0, wsum = 0.0;
	float p97 = t.back().first;
	bool p97Set = false;
	for ( const auto & [lg, a] : t ) {
		const double w = std::max( 0.0, std::min( run + a, hi ) - std::max( run, lo ) );	// the cover inside [lo, hi]
		run += a;
		sum += w * lg;
		wsum += w;
		if ( !p97Set && run >= 0.97 * cover ) {
			p97 = lg;
			p97Set = true;
		}
	}
	if ( !( wsum > 0.0 ) )
		return -1.0f;
	ps().hiSpread = std::max( 0.0f, p97 - float( sum / wsum ) );
	return float( std::exp2( sum / wsum ) );
}

void wwPhysNoteDaylight( const float sun[3], float mu, const float skyUp[3] )
{
	PhysState & s = ps();
	auto lum = []( const float v[3] ) { return 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2]; };
	const float normal = wwphys::kPi * std::max( lum( sun ), 0.0f );
	const float sky = wwphys::kPi * std::max( lum( skyUp ), 0.0f );
	const float level = normal * std::max( mu, 0.0f ) + sky;
	const float incEv = std::log2( std::max( level, 1e-4f ) * 100.0f / 250.0f );
	const float t = std::clamp( ( std::log10( std::max( level, 1e-4f ) ) - 3.0f ), 0.0f, 1.0f );	// 1 klux .. 10 klux
	const float dayW = t * t * ( 3.0f - 2.0f * t );
	if ( std::fabs( level - s.luxLevel ) > 0.01f * std::max( s.luxLevel, 1.0f ) )
		std::fprintf( stderr, "phys sun: %.0f lux normal (C %.5g cd/m2 x pi), mu %.3f, level %.0f lux (sun %.0f + sky %.0f), incident EV100 %.2f, day weight %.2f\n",
			double( normal ), double( lum( sun ) ), double( mu ), double( level ), double( normal * std::max( mu, 0.0f ) ), double( sky ),
			double( incEv ), double( dayW ) );
	s.luxNormal = normal;
	s.luxLevel = level;
	s.incEv = incEv;
	s.dayW = dayW;
}

void wwPhysNoteOutdoor( bool outdoor )
{
	ps().outdoor = outdoor;
}

float wwPhysExposure( const float * h, float adapted )
{
	PhysState & s = ps();
	const float la = std::max( adapted, 1e-6f );
	const float ev = std::log2( la * 100.0f / 12.5f );
	// the eye's key falls in low light (Krawczyk, Myszkowski & Seidel 2005), against daylight (La 4000 cd/m2)
	auto key = []( float l ) { return 1.03f - 2.0f / ( 2.0f + std::log10( l + 1.0f ) ); };
	const float ecEye = std::clamp( std::log2( std::max( key( la ), 1e-3f ) / key( 4000.0f ) ), -3.0f, 0.5f );
	// the imagespace's middle grey: the camera maps the mean to 1 / (1.2 x 8); HNAM's key h[8] as compensation
	const float ecKey = h ? std::clamp( std::log2( std::max( h[8], 1e-3f ) * 1.2f * 8.0f ), -4.0f, 4.0f ) : 0.0f;
	float manual = 0.0f;
	const bool pinned = pinnedFloat( "WW_PHYSUNITS_EV", manual );
	/* MODULES1 item 14 (bungo 11-09: sunlit walls clip to flat white, the shade reads fine): the mean meter exposes a
	 * shaded street for its shade, and the sunlit walls ~3.5 EV above land on the tone curve's flat shoulder (GT7
	 * physical, 250-nit paper white: 97 % of peak by 1.15 x paper white). The highlight guard darkens just enough that
	 * the meter's 97th percentile sits at paper white (84 % of peak, texture kept), at most 2.5 EV, as histogram auto
	 * exposure does. Red WW_PHYSUNITS_HILITE_RED=1 (live): the mean alone. */
	/* MODULES1 item 14 (bungo 11-09: the sky and the sunlit street blow out): a shaded alley metered alone set EV 11-12
	 * under a 97 klux sun. By day outdoors the camera is an incident meter (ISO 2720, C 250: sunny 16, EV ~15 in direct
	 * sun) on the sun + sky illuminance on level ground, and the reflected meter moves it at most 1.5 EV either way;
	 * the weight rises from 1 to 10 klux (dusk, night and interiors: the reflected meter alone). Red
	 * WW_PHYSUNITS_INCIDENT_RED=1 (live): the reflected meter alone. */
	float base = ev - ecEye - ecKey;
	const float reflected = base;
	const bool incident = !pinned && s.outdoor && s.dayW > 0.0f && wwEnvInt( "WW_PHYSUNITS_INCIDENT_RED" ) != 1;
	if ( incident )
		base += ( std::clamp( base, s.incEv - 1.5f, s.incEv + 1.5f ) - base ) * s.dayW;
	float guard = 0.0f;
	if ( wwEnvInt( "WW_PHYSUNITS_HILITE_RED" ) != 1 && s.hiSpread > 0.0f )
		guard = std::clamp( std::log2( la * std::exp2( s.hiSpread ) / 1.2f ) - base, 0.0f, 2.5f );
	const float applied = pinned ? manual : std::clamp( base + guard, -6.0f, 18.0f );
	const float e = 1.0f / ( 1.2f * std::exp2( applied ) );
	if ( std::fabs( applied - s.evApplied ) > 0.03f || std::fabs( ev - s.evMetered ) > 0.03f ) {	// telemetry on a move
		std::fprintf( stderr, "phys camera: La %.5g cd/m2 EV100 metered %.2f eye %.2f key %.2f reflected %.2f incident %s%.2f (day %.2f, sun %.0f lux normal, %.0f lux level) hilite %.2f (p97 +%.2f EV) applied %.2f%s exposure %.5g\n",
			double( adapted ), double( ev ), double( ecEye ), double( ecKey ), double( reflected ), incident ? "" : "OFF ",
			double( s.incEv ), double( s.dayW ), double( s.luxNormal ), double( s.luxLevel ), double( guard ), double( s.hiSpread ), double( applied ),
			pinned ? " (PINNED WW_PHYSUNITS_EV)" : "", double( e ) );
	}
	s.evMetered = ev;
	s.evApplied = applied;
	return e;
}

float wwPhysLastEvMetered()
{
	return ps().evMetered;
}

float wwPhysLastEvApplied()
{
	return ps().evApplied;
}

QString wwPhysEcho()
{
	if ( !wwPhysOn() )
		return QStringLiteral( "physunits: RED ^WW_PHYSUNITS_RED=1 (the old pinned scale)" );
	return QStringLiteral( "physunits: on sky=%1 sun toa %2 lux moon toa %3 lux night floor %4 cd/m2 game lights %5%6 emit %7 cd/m2 "
		"EV100 metered %8 applied %9" )
		.arg( wwPhysSkyOn() ? 1 : 0 ).arg( double( wwphys::kSunToaLux ), 0, 'f', 0 ).arg( double( wwphys::kMoonToaLux ), 0, 'f', 2 )
		.arg( double( wwphys::kNightFloor ), 0, 'g', 3 ).arg( double( wwPhysGameLights() ), 0, 'f', 3 )
		.arg( wwPhysGameLightsPinned() ? QStringLiteral( " (PINNED)" ) : QString() ).arg( double( wwPhysEmitScale() ), 0, 'f', 1 )
		.arg( double( ps().evMetered ), 0, 'f', 2 ).arg( double( ps().evApplied ), 0, 'f', 2 );
}
