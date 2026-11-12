// lane PHYSUNITS1: real photometric units (physunits.h)
#include "features/exposure/physunits.h"
#include "features/exposure/physcamera.h"
#include "features/lights/lightphys.h"
#include "cellworldanim.h"
#include "wwswitch.h"
#include "features/shared/camunired.h"	// lane EMPTY240 R12-1: the master's pin read once per switch generation

#include <QByteArray>
#include <QElapsedTimer>
#include <QSettings>
#include <QString>
#include <QtGlobal>

#include <QList>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

constexpr const char * kLightsKey = "WW/GameLightBrightness";
constexpr const char * kMasterKey = "Settings/Render/Scene/Physical Light Units 1";	// MODULES1 R6-13: its own key
constexpr bool kMasterDef = false;	// masters ship off: vanilla Fallout 4 lighting

struct PhysState
{
	QElapsedTimer cellFrame;
	quint64 cellPaint = 0;	// MODULES1 11-09 item 1: the paint that last drew the cell's HDR camera (0 = none yet)
	bool loaded = false;
	bool master = kMasterDef;	// MODULES1 R6-13: the Physical Light Units row
	double g = 1.0;
	float evMetered = 0.0f, evApplied = 0.0f;
	float hiSpread = 0.0f;	// MODULES1 item 14: log2( 97th percentile / mean ) of the last meter
	QElapsedTimer frameMeter;	// the frame's measure last applied a meter (one camera writer)
	float incEv = 0.0f, dayW = 0.0f;	// MODULES1 item 14: the incident meter's EV100 and its daylight weight (0..1)
	float luxNormal = -1.0f, luxLevel = -1.0f;
	bool outdoor = false;		// the cell drawn is an exterior (or a sunlit interior)
	// MODULES1 G1: the camera's adaptation (EV) and the meter's emitter cap
	float adaptEv = 0.0f, adaptIncEv = 0.0f, capShare = 0.0f;
	float logMetered = 0.0f, logApplied = 0.0f;	// the values the last `phys camera` line printed
	bool adaptValid = false;
	QElapsedTimer adaptClock;
};

PhysState & ps()
{
	static PhysState s;
	if ( !s.loaded ) {
		s.loaded = true;
		s.g = std::clamp( QSettings().value( QLatin1StringView( kLightsKey ), 1.0 ).toDouble(), 0.25, 4.0 );
		s.master = QSettings().value( QLatin1StringView( kMasterKey ), kMasterDef ? 1 : 0 ).toInt() != 0;
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

bool wwPhysMasterPinned()
{
	return wwEnvIsSet( "WW_PHYSUNITS_RED" );
}

bool wwPhysVanillaRed( const char * term )
{
	if ( !wwEnvIsSet( "WW_PHYSUNITS_VANILLA_RED" ) )
		return false;
	const QList<QByteArray> l = wwEnv( "WW_PHYSUNITS_VANILLA_RED" ).trimmed().toLower().split( ',' );
	return l.contains( QByteArray( term ) ) || l.contains( QByteArrayLiteral( "all" ) );
}

/* MODULES1 R6-13: the master. It was a hidden red (WW_PHYSUNITS_RED) with real units as the default; now it is a row
 * that ships Off, and Off is the game's lighting on every term (the line below names each). Read per call: the row
 * and the pin are live (a lamp's colour factor follows in celllamps.cpp physTick). */
bool wwPhysOn()
{
	// lane EMPTY240 R12-1: the pin is a switch (a live set moves the generation); red WW_CAMUNI_RED=envkeys reads per call
	WW_SWITCH_STATIC( bool, pinnedKept, wwEnvIsSet( "WW_PHYSUNITS_RED" ) );
	WW_SWITCH_STATIC( QByteArray, pinKept, wwEnv( "WW_PHYSUNITS_RED" ).trimmed() );
	const bool perCall = wwCamUniRed( "envkeys" );
	const bool pinned = perCall ? wwEnvIsSet( "WW_PHYSUNITS_RED" ) : pinnedKept;
	bool on = ps().master;
	QByteArray pin;
	if ( pinned ) {
		pin = perCall ? wwEnv( "WW_PHYSUNITS_RED" ).trimmed() : pinKept;
		on = pin.isEmpty() || pin == "0";
	}
	static std::atomic<int> said { -1 };
	const int key = ( on ? 1 : 0 ) | ( pinned ? 2 : 0 );
	if ( said.exchange( key ) != key ) {
		const QByteArray how = pinned ? QByteArray( "PINNED WW_PHYSUNITS_RED=" ) + pin : QByteArray( "the row; ships Off" );
		const bool curve = wwPhysVanillaRed( "curve" ), eye = wwPhysVanillaRed( "eye" ), lamp = wwPhysVanillaRed( "lampscale" );
		if ( on )
			std::fprintf( stderr, "physunits master: Physical Light Units ON (%s). physical: lamps (LIGH: the %g lux edge law x Game "
				"Light Brightness), .nifx lights (absolute lumens), sun and moon (top of the atmosphere over pi, through the air), "
				"ambient (the physical sky's irradiance + the night floor), sky scale and glare, emissives (%g cd/m2 per unit x "
				"Game Light Brightness), unlit effects, particles and fog colours (the light's ratio to the weather's), exposure "
				"(the EV100 camera), bloom (the energy-conserving glare), tone curve (Physical), frame RGBA32F. vanilla: none\n",
				how.constData(), double( wwphys::kLampEdgeLux ), double( wwphys::kEmitMidCdm2 ) );
		else
			std::fprintf( stderr, "physunits master: Physical Light Units OFF (%s) = vanilla Fallout 4 lighting. vanilla: lamps (LIGH "
				"colour x fade x1, no Game Light Brightness%s), sun and moon light (the weather record's sunlight x the imagespace's "
				"sunlight scale), ambient (the weather record's), sky scale and glare (x1), emissives (x1 x the Glow row), unlit "
				"effects, particles and fog colours (x1), exposure (the imagespace's eye adaptation, HNAM min / max%s), tone curve "
				"(%s), bloom (the Bloom row's own law, no glare), frame RGBA16F. no vanilla form: .nifx lights (ours; their "
				"authored fade, 637 lm per fade, x1). physical: none (the Physical Sky rows are their own masters). Reds "
				"WW_PHYSUNITS_VANILLA_RED=curve,eye,lampscale\n",
				how.constData(), lamp ? "; RED lampscale: a sunlit interior's lamps x the derived scale" : "",
				eye ? "; RED eye: the partial law under L0" : ", full law",
				curve ? "RED curve: Physical" : "the game's filmic" );
	}
	return on;
}

int wwPhysMaster()
{
	return wwPhysOn() ? 1 : 0;
}

void wwPhysSetMaster( int on )
{
	if ( wwPhysMasterPinned() )
		return;
	PhysState & s = ps();
	if ( s.master == ( on != 0 ) )
		return;
	s.master = on != 0;
	QSettings().setValue( QLatin1StringView( kMasterKey ), s.master ? 1 : 0 );
	// the statics that read it (the frame's format, cellhdr.cpp) take their value again, as after a live set
	g_wwSwitchGen.fetch_add( 1, std::memory_order_acq_rel );
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
	// MODULES1 G1: the meter's core lives in physcamera.h (the emitter cap; red WW_PHYSUNITS_CAMERA_RED=1 = no cap)
	const wwphys::MeterOut m = wwphys::cameraMeter( t, wwEnvInt( "WW_PHYSUNITS_CAMERA_RED" ) == 1 );
	if ( m.mean < 0.0f )
		return -1.0f;
	ps().hiSpread = m.hiSpread;
	ps().capShare = m.capShare;
	return m.mean;
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
	/* MODULES1 G1 (bungo 11-09: turning on the spot swung the exposure 6.6 EV; a pinned EV 2.2 "worked, looks much
	 * better"): the law is physcamera.h's (adaptation floor 1 cd/m2, the guard by day only and bounded, eye-like
	 * adaptation in time). Red WW_PHYSUNITS_CAMERA_RED=1 (live): the law before, in full. */
	const bool camRed = wwEnvInt( "WW_PHYSUNITS_CAMERA_RED" ) == 1;
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
	wwphys::CameraIn in;
	in.la = adapted;
	in.hasKey = h != nullptr;
	in.key = h ? h[8] : 0.18f;
	in.day = s.outdoor && s.dayW > 0.0f;
	in.incident = !pinned && in.day && wwEnvInt( "WW_PHYSUNITS_INCIDENT_RED" ) != 1;
	in.incEv = s.incEv;
	in.dayW = s.dayW;
	in.hiSpread = wwEnvInt( "WW_PHYSUNITS_HILITE_RED" ) != 1 ? s.hiSpread : 0.0f;
	in.red = camRed;
	const wwphys::CameraOut c = wwphys::cameraTarget( in );
	const float target = pinned ? manual : c.target;
	// the adaptation: the camera follows its target like the eye, and steps where a fade would show a load or a set
	const float dt = s.adaptClock.isValid() ? float( s.adaptClock.nsecsElapsed() ) * 1e-9f : -1.0f;
	const bool metered = adapted > 0.0f;
	const bool snap = camRed || pinned || !metered
		|| wwphys::cameraSnap( s.adaptValid, dt, s.incEv, s.adaptIncEv, wwCellWorldClockMoves() );
	const float applied = snap ? target : wwphys::cameraAdapt( s.adaptEv, target, dt );
	if ( metered || pinned ) {
		s.adaptEv = applied;
		s.adaptIncEv = s.incEv;
		s.adaptValid = true;
		s.adaptClock.start();
	}
	const float e = 1.0f / ( 1.2f * std::exp2( applied ) );
	s.evMetered = c.ev;
	s.evApplied = applied;
	if ( std::fabs( applied - s.logApplied ) > 0.03f || std::fabs( c.ev - s.logMetered ) > 0.03f ) {	// telemetry on a move
		std::fprintf( stderr, "phys camera: La %.5g cd/m2 EV100 metered %.2f eye %.2f key %.2f reflected %.2f incident %s%.2f (day %.2f, sun %.0f lux normal, %.0f lux level) hilite %.2f (p97 +%.2f EV) applied %.2f%s exposure %.5g target %.2f adapt %s dt %.3f s emitter cap %.1f %% of cover%s\n",
			double( adapted ), double( c.ev ), double( c.ecEye ), double( c.ecKey ), double( c.reflected ), in.incident ? "" : "OFF ",
			double( s.incEv ), double( s.dayW ), double( s.luxNormal ), double( s.luxLevel ), double( c.guard ), double( s.hiSpread ), double( applied ),
			pinned ? " (PINNED WW_PHYSUNITS_EV)" : "", double( e ), double( target ), snap ? "snap" : target > applied ? "closing" : "opening",
			double( dt ), double( s.capShare * 100.0f ), camRed ? " (RED WW_PHYSUNITS_CAMERA_RED)" : "" );
		s.logMetered = c.ev;
		s.logApplied = applied;
	}
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
	if ( !wwPhysOn() )	// MODULES1 R6-13
		return wwPhysMasterPinned() ? QStringLiteral( "physunits: OFF, vanilla Fallout 4 lighting (PINNED ^WW_PHYSUNITS_RED)" )
			: QStringLiteral( "physunits: OFF, vanilla Fallout 4 lighting (the Physical Light Units row)" );
	return QStringLiteral( "physunits: on sky=%1 sun toa %2 lux moon toa %3 lux night floor %4 cd/m2 game lights %5%6 emit %7 cd/m2 "
		"EV100 metered %8 applied %9" )
		.arg( wwPhysSkyOn() ? 1 : 0 ).arg( double( wwphys::kSunToaLux ), 0, 'f', 0 ).arg( double( wwphys::kMoonToaLux ), 0, 'f', 2 )
		.arg( double( wwphys::kNightFloor ), 0, 'g', 3 ).arg( double( wwPhysGameLights() ), 0, 'f', 3 )
		.arg( wwPhysGameLightsPinned() ? QStringLiteral( " (PINNED)" ) : QString() ).arg( double( wwPhysEmitScale() ), 0, 'f', 1 )
		.arg( double( ps().evMetered ), 0, 'f', 2 ).arg( double( ps().evApplied ), 0, 'f', 2 );
}
