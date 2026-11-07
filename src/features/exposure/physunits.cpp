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

namespace {

constexpr const char * kLightsKey = "WW/GameLightBrightness";

struct PhysState
{
	QElapsedTimer cellFrame;
	bool loaded = false;
	double g = 1.0;
	float evMetered = 0.0f, evApplied = 0.0f;
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

void wwPhysNoteCellFrame()
{
	ps().cellFrame.start();
}

bool wwPhysSkyOn()
{
	const PhysState & s = ps();
	return wwPhysOn() && s.cellFrame.isValid() && s.cellFrame.elapsed() < 1500;
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
	float e = wwphys::kEmitCdm2;
	pinnedFloat( "WW_PHYSUNITS_EMIT", e );
	return std::max( e, 0.0f ) * wwPhysGameLights();
}

float wwPhysLampK( float radius )
{
	const float rm = radius / wwlp::kUnitsPerMetre;
	const float drawn = 4.0f * wwphys::kPi * wwphys::kPi * wwlp::vanillaFlux() * rm * rm;
	return drawn > 1e-9f ? wwphys::kLumenPerFade / drawn : 1.0f;
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
	const float applied = pinned ? manual : std::clamp( ev - ecEye - ecKey, -6.0f, 18.0f );
	const float e = 1.0f / ( 1.2f * std::exp2( applied ) );
	if ( std::fabs( applied - s.evApplied ) > 0.03f || std::fabs( ev - s.evMetered ) > 0.03f ) {	// telemetry on a move
		std::fprintf( stderr, "phys camera: La %.5g cd/m2 EV100 metered %.2f eye %.2f key %.2f applied %.2f%s exposure %.5g\n",
			double( adapted ), double( ev ), double( ecEye ), double( ecKey ), double( applied ),
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
