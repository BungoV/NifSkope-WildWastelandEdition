#include "features/lights/lightmeas.h"
#include "features/clouds/cloudshadow.h"
#include "features/exposure/physunits.h"
#include "features/sky/atmomath.h"
#include "features/sky/atmosphere.h"
#include "features/weather/esmweather.h"
#include "features/weather/weatherrecx.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "wwswitch.h"

#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

bool wwAtmoGiSkyAxes( const float sunTo[3], float out[6][3], float * trans = nullptr, float hKm = 0.002f );	// atmosphere.cpp

/* lane LIGHTMEAS1: what each line says, the references' sources and the offline check: notes/lightmeas1/README.md and
 * notes/lightmeas1/lightmeas_ref.py. Nothing here feeds a renderer: it reads the stage's light, the drawn sky's tables
 * (its own copy, built like wwAtmoHorizon's) and the exposure's mip, and prints. */
namespace
{
constexpr float kPi = 3.14159265f;
constexpr float kDeg = kPi / 180.0f;
constexpr float kGroundAlbedo = 0.12f;	// the brief's test ground (dry soil / asphalt)
constexpr float kWaterF0 = 0.0204f;	// water, n 1.333

float lumOf( const float c[3] )
{
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

std::string col( const float c[3] )
{
	const float s = std::max( c[0] + c[1] + c[2], 1e-30f );
	char b[160];
	std::snprintf( b, sizeof( b ), "%.4g(rgb %.3f,%.3f,%.3f R/B %.2f)", double( lumOf( c ) ), double( c[0] / s ), double( c[1] / s ),
		double( c[2] / s ), double( c[0] / std::max( c[2], 1e-30f ) ) );
	return b;
}

struct SkyModel
{
	WwAtmoMath::SkyTables T;
	float hq[3] = { 0, 1, 0 };
	bool ok = false;
};

SkyModel & skyModel()
{
	static SkyModel m;
	return m;
}

// the drawn sky's tables at the last sky frame's haze, quantised like atmosphere.cpp's wwAtmoHorizon
bool skyReady( const WwAtmoFrame & f )
{
	SkyModel & m = skyModel();
	if ( !f.valid || wwAtmoTier() <= 0 || f.camR <= 0.0f ) {
		m.ok = false;
		return false;
	}
	auto q2 = []( float x ) { return x > 1e-4f ? std::exp( std::round( std::log( x ) * 50.0f ) / 50.0f ) : x; };
	m.hq[0] = q2( f.haze[0] );
	m.hq[1] = q2( f.haze[1] );
	m.hq[2] = std::round( f.haze[2] * 100.0f ) / 100.0f;
	WwAtmoMath::skyTablesBuild( m.hq, wwWeatherRecxOzone(), wwAtmoTier() == 1 ? 16 : 32, m.T );
	m.ok = true;
	return true;
}

// the drawn clear sky (Hillaire: sun + moon, no clouds, no floor) along v, scene units (cd/m2)
void skyAt( const WwAtmoFrame & f, const float v[3], float out[3], float * sunPart = nullptr )
{
	SkyModel & m = skyModel();
	out[0] = out[1] = out[2] = 0.0f;
	if ( !m.ok )
		return;
	double L[3];
	WwAtmoMath::skyMarch( m.T, m.hq, wwWeatherRecxOzone(), f.camR, v, f.sunW, L );
	for ( int c = 0; c < 3; c++ )
		out[c] = float( double( f.sunK ) * L[c] );
	if ( sunPart )
		std::copy( out, out + 3, sunPart );
	if ( f.moonK > 0.0f ) {
		WwAtmoMath::skyMarch( m.T, m.hq, wwWeatherRecxOzone(), f.camR, v, f.moonW, L );
		for ( int c = 0; c < 3; c++ )
			out[c] += float( double( f.moonK ) * L[c] );
	}
}

void dirAt( float elevDeg, float az, float v[3] )
{
	const float e = elevDeg * kDeg;
	v[0] = std::cos( e ) * std::cos( az );
	v[1] = std::cos( e ) * std::sin( az );
	v[2] = std::sin( e );
}

double field( const QString & s, const char * key )
{
	const int i = s.indexOf( QLatin1StringView( key ) );
	if ( i < 0 )
		return -1.0;
	const int b = i + int( std::strlen( key ) );
	int e = b;
	while ( e < s.size() && ( s[e].isDigit() || s[e] == '.' || s[e] == '-' ) )
		e++;
	bool ok = false;
	const double v = s.mid( b, e - b ).toDouble( &ok );
	return ok ? v : -1.0;
}

float median( std::vector<float> v )
{
	if ( v.empty() )
		return -1.0f;
	std::nth_element( v.begin(), v.begin() + v.size() / 2, v.end() );
	return v[v.size() / 2];
}

void say( int k, const std::string & line )
{
	static std::string last[12];
	if ( k < 0 || k >= 12 || line == last[k] )
		return;
	last[k] = line;
	std::fprintf( stderr, "light meas: %s\n", line.c_str() );
}
}	// namespace

void wwLightMeasFrame( Scene * scene, const float * px, int qw, int qh )
{
	if ( wwEnvStr( "WW_LIGHT_MEAS" ).trimmed() == QLatin1StringView( "0" ) )
		return;
	using clk = std::chrono::steady_clock;
	static clk::time_point next;
	const clk::time_point now = clk::now();
	if ( now < next )
		return;
	next = now + std::chrono::seconds( 1 );
	if ( !wwLookdevActive() )
		return;
	char b[1400];

	// ---- the key light (sun by day, moon by night) as the surfaces take it
	float keyTo[3], key[4], ambLegacy[4];
	wwLookdevLight( keyTo, key, ambLegacy );
	const float sunScale = wwLookdevSunlightScale();
	const WwAtmoFrame & f = wwAtmoFrame();
	const bool night = f.valid ? f.sunW[2] <= 0.0f : keyTo[2] <= 0.0f;
	const float kl = std::sqrt( keyTo[0] * keyTo[0] + keyTo[1] * keyTo[1] + keyTo[2] * keyTo[2] );
	const float mu = kl > 0.0f ? std::max( keyTo[2] / kl, 0.0f ) : 0.0f;
	const float elev = std::asin( std::clamp( mu, -1.0f, 1.0f ) ) / kDeg;
	const float keyN = kPi * lumOf( key ) * sunScale;	// lux on a surface facing the key, after the air (no cloud)
	const bool tables = skyReady( f );
	float T[3] = { 1, 1, 1 };
	if ( f.valid )
		WwAtmoMath::cpuTrans( std::max( f.camR - WwAtmoMath::kRb, 0.0f ), std::max( mu, 0.01f ),
			tables ? skyModel().hq : f.haze, false, T, wwWeatherRecxOzone() );
	const QString cs = wwCloudShadowSummary();
	const double farT = field( cs, "far_t=" ), giT = field( cs, "gi=" );
	const float cloudEye = giT >= 0.0 ? float( giT ) : 1.0f;	// the 5x5 mean cloud T over the eye (0 = all cut)
	const float toa = night ? wwphys::kMoonToaLux : wwphys::kSunToaLux;
	std::snprintf( b, sizeof( b ),
		"key %s elev %.1f deg | TOA %.6g lux, air T %.3f,%.3f,%.3f (lum %.3f) -> %.6g lux normal | surfaces get %.6g lux normal"
		" (key C %s x pi x SunlightScale %.3g) | clouds: far_t %.3f, over the eye %.4f -> %.6g lux normal | direct horizontal"
		" %.6g lux (clear air) %.6g lux (x cloud over the eye)",
		night ? "moon" : "sun", double( elev ), double( toa ), double( T[0] ), double( T[1] ), double( T[2] ), double( lumOf( T ) ),
		double( toa * lumOf( T ) ), double( keyN ), col( key ).c_str(), double( sunScale ), farT, double( cloudEye ),
		double( keyN * cloudEye ), double( keyN * mu ), double( keyN * mu * cloudEye ) );
	say( 0, b );

	// ---- the drawn clear sky at five directions, and its horizontal irradiance by quadrature
	const bool sky = tables;
	float Eh_drawn = -1.0f, Eh_axes = -1.0f;
	float sunAz = 0.0f;
	if ( f.valid ) {
		const float lw = night ? std::hypot( f.moonW[0], f.moonW[1] ) : std::hypot( f.sunW[0], f.sunW[1] );
		sunAz = lw > 1e-6f ? ( night ? std::atan2( f.moonW[1], f.moonW[0] ) : std::atan2( f.sunW[1], f.sunW[0] ) ) : 0.0f;
	}
	if ( sky ) {
		struct D { const char * name; float e, az; };
		const D ds[6] = { { "zenith", 89.5f, 0.0f }, { "45 toward", 45.0f, sunAz }, { "45 away", 45.0f, sunAz + kPi },
			{ "horizon toward", 1.0f, sunAz }, { "horizon away", 1.0f, sunAz + kPi }, { "horizon 90", 1.0f, sunAz + 0.5f * kPi } };
		std::string s = "sky drawn (clear Hillaire, no clouds, cd/m2):";
		for ( const D & d : ds ) {
			float v[3], L[3];
			dirAt( d.e, d.az, v );
			skyAt( f, v, L );
			s += std::string( " " ) + d.name + " " + col( L );
		}
		say( 1, s );
		double acc[3] = { 0, 0, 0 };
		constexpr int NE = 9, NA = 24;
		for ( int i = 0; i < NE; i++ ) {
			const float e = ( float( i ) + 0.5f ) * 90.0f / NE;
			const float dO = std::cos( e * kDeg ) * ( 90.0f / NE * kDeg ) * ( 2.0f * kPi / NA );
			for ( int a = 0; a < NA; a++ ) {
				float v[3], L[3];
				dirAt( e, ( float( a ) + 0.5f ) * 2.0f * kPi / NA, v );
				skyAt( f, v, L );
				for ( int c = 0; c < 3; c++ )
					acc[c] += double( L[c] ) * std::sin( e * kDeg ) * dO;
			}
		}
		const float accF[3] = { float( acc[0] ), float( acc[1] ), float( acc[2] ) };
		Eh_drawn = lumOf( accF );
		float axes[6][3], Tg[3];
		if ( !night && wwAtmoGiSkyAxes( f.sunW, axes, Tg ) )
			Eh_axes = lumOf( axes[4] ) * f.sunK;
	}

	// ---- the ambient the ground shading receives: the stage's DALC, the up face (the weather's order, index 5)
	float w[6][3];
	const bool haveDalc = wwLookdevDalc( w );
	const bool axRed = wwEnvStr( "WW_PHYSUNITS_AXES_RED" ).trimmed() == QLatin1StringView( "1" );
	const int up = axRed ? 4 : 5;
	float ambUp[3] = { 0, 0, 0 }, ambShader[3] = { 0, 0, 0 }, env[3] = { 0, 0, 0 };
	if ( haveDalc )
		for ( int c = 0; c < 3; c++ ) {
			// celllights.cpp: g = DALC^(1/2.2) per axis, cellDalc = (half differences, mean); cell_lights.glsl cellAmbient:
			// pow(dot(cellDalc, (N, 1)), 2.2); an up normal reads mean + 0.5 (g5 - g4)
			float g[6], mean = 0.0f;
			for ( int a = 0; a < 6; a++ ) {
				g[a] = std::pow( std::max( w[a][c], 0.0f ), 1.0f / 2.2f );
				mean += g[a] / 6.0f;
				env[c] += w[a][c] / 6.0f;
			}
			ambUp[c] = w[up][c];
			ambShader[c] = std::pow( std::max( mean + 0.5f * ( g[5] - g[4] ), 0.0f ), 2.2f );
		}
	const float Eh_dalc = kPi * lumOf( ambUp ), Eh_shader = kPi * lumOf( ambShader );

	// ---- the frame: the exposure's quarter mip along each texel's own ray (atmosphere.cpp wwAtmoAdaptedInView's rays)
	float frameGround = -1.0f, frameCB = -1.0f, frameCBe = 0.0f, ratioAll = -1.0f, centreE = 0.0f;
	std::string bands;
	if ( px && qw > 0 && qh > 0 && scene && scene->renderer && scene->renderer->globalUniforms ) {
		const auto & gu = *scene->renderer->globalUniforms;
		if ( gu.projectionMatrix[3][3] != 1.0f ) {
			const float P00 = gu.projectionMatrix[0][0], P11 = gu.projectionMatrix[1][1];
			const float P20 = gu.projectionMatrix[2][0], P21 = gu.projectionMatrix[2][1];
			auto ray = [&]( float x, float y, float o[3] ) {
				const float vr[3] = { ( x + P20 ) / P00, ( y + P21 ) / P11, -1.0f };
				for ( int c = 0; c < 3; c++ )
					o[c] = vr[0] * gu.viewMatrix[c][0] + vr[1] * gu.viewMatrix[c][1] + vr[2] * gu.viewMatrix[c][2];
				const float l = std::sqrt( o[0] * o[0] + o[1] * o[1] + o[2] * o[2] );
				for ( int c = 0; c < 3; c++ )
					o[c] /= std::max( l, 1e-9f );
			};
			auto texL = [&]( int x, int y ) {
				const float * p = px + ( size_t( y ) * size_t( qw ) + size_t( x ) ) * 4;
				return p[3] > 0.5f ? lumOf( p ) / p[3] : -1.0f;
			};
			float cr[3];
			ray( 0.0f, 0.0f, cr );
			centreE = std::asin( std::clamp( cr[2], -1.0f, 1.0f ) ) / kDeg;
			// centre-bottom: the middle column, a tenth of the height up (row 0 = the screen's bottom)
			{
				const int x = qw / 2, y = std::max( qh / 10, 0 );
				float r[3];
				ray( 2.0f * ( float( x ) + 0.5f ) / float( qw ) - 1.0f, 2.0f * ( float( y ) + 0.5f ) / float( qh ) - 1.0f, r );
				frameCBe = std::asin( std::clamp( r[2], -1.0f, 1.0f ) ) / kDeg;
				frameCB = texL( x, y );
			}
			std::vector<float> gnd, fr[3], md[3], rt[3], rtAll;
			const float edge[4] = { 3.0f, 15.0f, 40.0f, 90.1f };
			const int sx = std::max( qw / 32, 1 ), sy = std::max( qh / 18, 1 );
			for ( int y = sy / 2; y < qh; y += sy )
				for ( int x = sx / 2; x < qw; x += sx ) {
					const float L = texL( x, y );
					if ( L < 0.0f )
						continue;
					float r[3];
					ray( 2.0f * ( float( x ) + 0.5f ) / float( qw ) - 1.0f, 2.0f * ( float( y ) + 0.5f ) / float( qh ) - 1.0f, r );
					const float e = std::asin( std::clamp( r[2], -1.0f, 1.0f ) ) / kDeg;
					if ( e < -3.0f ) {
						gnd.push_back( L );
						continue;
					}
					for ( int k = 0; k < 3; k++ )
						if ( e >= edge[k] && e < edge[k + 1] ) {
							fr[k].push_back( L );
							if ( sky ) {
								float m[3];
								skyAt( f, r, m );
								const float lm = lumOf( m );
								md[k].push_back( lm );
								if ( lm > 0.0f ) {
									rt[k].push_back( L / lm );
									rtAll.push_back( L / lm );
								}
							}
						}
				}
			frameGround = median( gnd );
			ratioAll = median( rtAll );
			const char * bn[3] = { "3-15", "15-40", "40-90" };
			for ( int k = 0; k < 3; k++ ) {
				char t[200];
				std::snprintf( t, sizeof( t ), " | up %s deg: n %d frame %.4g model %.4g frame/model %.3g", bn[k], int( fr[k].size() ),
					double( median( fr[k] ) ), double( median( md[k] ) ), double( median( rt[k] ) ) );
				bands += t;
			}
			char t[260];
			std::snprintf( t, sizeof( t ), "frame (cd/m2, medians of the exposure's quarter mip, rays above +3 deg may hold geometry):"
				" view centre elev %.1f deg | below -3 deg: n %d median %.4g | centre-bottom (elev %.1f) %.4g",
				double( centreE ), int( gnd.size() ), double( frameGround ), double( frameCBe ), double( frameCB ) );
			say( 2, std::string( t ) + bands );
		}
	}

	// ---- the sky's horizontal irradiance: what the ground receives vs what is drawn, and the references
	{
		const float sinS = std::sin( std::max( elev, 0.0f ) * kDeg );
		const float LzCie = 123.0f + 8600.0f * sinS;	// Krochmann and Seidl 1974 (CIE overcast zenith, cd/m2)
		const float EhCie = 7.0f * kPi / 9.0f * LzCie;	// CIE overcast (Moon and Spencer 1942): Eh = 7 pi Lz / 9
		const float EhFrame = ( ratioAll > 0.0f && Eh_drawn > 0.0f ) ? ratioAll * Eh_drawn : -1.0f;
		std::snprintf( b, sizeof( b ),
			"sky Eh (lux): ground path = stage DALC up (wwLookdevDalc[%d] -> celllights.cpp cellDalc -> cell_lights.glsl"
			" cellAmbient) %.6g, as the shader decodes an up normal %.6g, colour %s | drawn clear sky integrated %.6g"
			" (GI axes +Z x sunK %.6g) | the frame's sky / drawn model %.3g -> drawn sky with clouds ~%.6g |"
			" gap ground/drawn-with-clouds %.3g | ref CIE overcast at %.1f deg: Lz %.0f cd/m2 Eh %.0f lux;"
			" real overcast 5000-20000, clear-sky diffuse ~10000-20000 at 30-35 deg",
			up, double( Eh_dalc ), double( Eh_shader ), col( ambShader ).c_str(), double( Eh_drawn ), double( Eh_axes ),
			double( ratioAll ), double( EhFrame ), EhFrame > 0.0f ? double( Eh_shader / EhFrame ) : -1.0, double( elev ),
			double( LzCie ), double( EhCie ) );
		say( 3, b );

		// ---- the ground: a 0.12 Lambert as the shader lights it (no AO, no cascade shadow; the cloud T over the eye)
		const float Lsun = kGroundAlbedo * lumOf( key ) * sunScale * mu * cloudEye;
		const float Lamb = kGroundAlbedo * lumOf( ambShader );
		const float Lg = Lsun + Lamb;
		const float LgCie = kGroundAlbedo * EhCie / kPi;
		std::snprintf( b, sizeof( b ),
			"ground 0.12 Lambert (cd/m2): sun %.4g + sky %.4g = %.4g | frame below -3 deg %.4g, centre-bottom %.4g |"
			" ref CIE overcast ground %.4g (zenith/ground %.3g, 45 deg/ground %.3g, horizon/ground %.3g)",
			double( Lsun ), double( Lamb ), double( Lg ), double( frameGround ), double( frameCB ), double( LgCie ),
			double( LzCie / std::max( LgCie, 1e-9f ) ), double( LzCie * ( 1.0f + 2.0f * std::sin( 45.0f * kDeg ) ) / 3.0f / std::max( LgCie, 1e-9f ) ),
			double( LzCie / 3.0f / std::max( LgCie, 1e-9f ) ) );
		say( 4, b );
	}

	// ---- water: the weather's three sky rows (fo4_water.frag), Schlick F0 0.0204, the reflected sky at view angles
	{
		float hz[3], lo[3], upr[3];
		if ( wwLookdevWaterSky( hz, lo, upr ) ) {
			std::string s = "water: rows horizon " + col( hz ) + " lower " + col( lo ) + " upper " + col( upr ) + " |";
			const float angs[4] = { 2.0f, 10.0f, 30.0f, std::max( -centreE, 0.5f ) };
			for ( int i = 0; i < 4; i++ ) {
				const float a = angs[i], rz = std::sin( a * kDeg );
				float skyR[3];
				for ( int c = 0; c < 3; c++ ) {
					const float m1 = hz[c] + ( lo[c] - hz[c] ) * std::clamp( rz + 0.75f, 0.0f, 1.0f );
					skyR[c] = m1 + ( upr[c] - m1 ) * std::clamp( rz * 1.9f + 0.35f, 0.0f, 1.0f );
				}
				const float F = kWaterF0 + ( 1.0f - kWaterF0 ) * std::pow( 1.0f - rz, 5.0f );
				float drawn[3] = { 0, 0, 0 };
				if ( sky ) {
					float v[3];
					dirAt( a, sunAz + kPi, v );
					skyAt( f, v, drawn );
				}
				char t[240];
				std::snprintf( t, sizeof( t ), " %s%.1f deg: F %.4f row sky %.4g -> reflected %.4g (drawn sky there %.4g -> %.4g)",
					i == 3 ? "view centre " : "", double( a ), double( F ), double( lumOf( skyR ) ), double( F * lumOf( skyR ) ),
					double( lumOf( drawn ) ), double( F * lumOf( drawn ) ) );
				s += t;
			}
			say( 5, s );
		}
	}

	// ---- exposure
	{
		const float ev = wwPhysLastEvApplied();
		std::snprintf( b, sizeof( b ),
			"exposure: EV100 applied %.2f metered %.2f -> mid grey at L = 2^EV x 12.5/100 = %.4g cd/m2 (reflected-light K 12.5)",
			double( ev ), double( wwPhysLastEvMetered() ), double( std::exp2( ev ) * 0.125f ) );
		say( 6, b );
	}

	// ---- the air's in-scatter sources by light, with colours
	{
		std::string s = "air sources (scene units):";
		float hzT[3];
		if ( wwAtmoHorizon( hzT ) )
			s += " drawn horizon total " + col( hzT );
		if ( sky ) {
			double aS[3] = { 0, 0, 0 }, aT[3] = { 0, 0, 0 };
			constexpr int NA = 16;
			for ( int a = 0; a < NA; a++ ) {
				float v[3], L[3], Ls[3];
				dirAt( std::asin( 0.01f ) / kDeg, ( float( a ) + 0.5f ) * 2.0f * kPi / NA, v );
				skyAt( f, v, L, Ls );
				for ( int c = 0; c < 3; c++ ) {
					aS[c] += Ls[c] / NA;
					aT[c] += L[c] / NA;
				}
			}
			const float sunP[3] = { float( aS[0] ), float( aS[1] ), float( aS[2] ) };
			const float moonP[3] = { float( aT[0] - aS[0] ), float( aT[1] - aS[1] ), float( aT[2] - aS[2] ) };
			s += " = sun " + col( sunP ) + " + moon " + col( moonP );
		}
		float fl[3], unit = 0.0f;
		if ( wwAtmoNightFloorScene( fl, &unit ) )
			s += " + floor(zenith) " + col( fl );
		WwAtmoAirExtras ex;
		if ( wwAtmoAirExtras( ex ) )
			s += " | haze glow " + col( ex.glow ) + " inscatter override " + col( ex.inscatter ) + " dir " + col( ex.dir );
		s += " | key " + col( key ) + " white-through-air " + col( T ) + " | env (DALC mean) " + col( env );
		WwFog fog;
		if ( wwLookdevFogNow( fog ) )
			s += " | fog record near " + col( fog.nearLow ) + " far " + col( fog.farLow );
		say( 7, s );
	}

	// ---- the references that move with the key (once per change)
	{
		std::snprintf( b, sizeof( b ),
			"refs: %s | full moon 0.1-0.3 lux at a high moon (TOA %.2f lux); moonlight is physically a little REDDER than"
			" sunlight (CCT ~4100-4700 K), seen bluer only by the Purkinje shift | clear sky at %.0f deg: direct normal"
			" ~70000-95000 lux, sky diffuse horizontal ~10000-20000 lux | sky/ground under CIE overcast with a 0.12 ground:"
			" zenith 10.7x, 45 deg 8.6x, horizon 3.6x",
			night ? "night" : "day", double( wwphys::kMoonToaLux ), double( elev ) );
		say( 8, b );
	}
}
