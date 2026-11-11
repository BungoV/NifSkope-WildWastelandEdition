// lane MODULES1 (exposure, from gl/celllights.cpp): the eye's adaptation limit, the exposure law the cell
// uniforms and the imagespace echo read, and the tone curve's red switch
#include "features/exposure/exposure.h"
#include "features/exposure/physunits.h"	// MODULES1 R6-13: the master (off = the game's eye and curve)
#include <cstdio>
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "gl/celllooks.h"	// lane GAPLOOK1: the Wide exposure range row
#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/cellunits.h"	// the private header's texture units
#include "features/lights/celllampshadow.h"	// the private header's lamp walls
#include "features/lights/cellnifxlights.h"	// the private header's live .nifx lights
#include "features/lights/cellflickclock.h"	// the private header's flicker phase

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QHash>
#include <QSettings>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <vector>
#include "features/lights/celllights_p.h"	// the hub's state the row reads (st(), isLighting)

using namespace WwCellLightsP;

/* the frame's exposure: the tonemap PS's clamp( mid / (lum + 0.001), HNAM min, HNAM max ). lane GIBAKE2 (GISHADE1): the
 * Wide exposure range row lifts the max to 16, so a dim GI-lit room opens up (the imagespace caps it at ~3.25). */
/* lane MUSGI1 (11-03, bungo "night looks like the brightest noon"): the eye's adaptation limit. Above L0 (a daylight
 * adapted level, under Concord noon's 0.145) the eye adapts fully, the old law; under it only partly, as the mesopic and
 * scotopic eye does (the key falls with the light, Krawczyk 2005): the exposure follows adapted^-a, not adapted^-1, so a
 * dark scene stays dark. Equal at L0. Pin WW_CELL_EYE=a,L0 (live; default 0.4,0.14); red WW_CELL_EYE_RED=1 = full. */
WwCellEye wwCellEye()
{
	WW_SWITCH_STATIC( bool, red, [] {
		const QByteArray v = wwEnvIsSet( "WW_CELL_EYE_RED" ) ? wwEnv( "WW_CELL_EYE_RED" ).trimmed() : QByteArray();
		return !v.isEmpty() && v != "0";
	}() );
	WW_SWITCH_STATIC( QByteArray, pin, wwEnvIsSet( "WW_CELL_EYE" ) ? wwEnv( "WW_CELL_EYE" ).trimmed() : QByteArray() );
	WwCellEye e;
	// MODULES1 R6-13: with Physical Light Units off the eye is the game's (full adaptation between HNAM min and max),
	// unless WW_CELL_EYE pins a law or WW_PHYSUNITS_VANILLA_RED=eye keeps the partial one
	e.red = red || ( pin.isEmpty() && !wwPhysOn() && !wwPhysVanillaRed( "eye" ) );
	const QList<QByteArray> p = pin.split( ',' );
	bool ok = false;
	if ( !pin.isEmpty() && p.size() >= 1 ) {
		const float a = p[0].toFloat( &ok );
		if ( ok )
			e.a = std::clamp( a, 0.0f, 1.0f );
	}
	if ( p.size() >= 2 ) {
		const float l = p[1].toFloat( &ok );
		if ( ok && l > 0.0f )
			e.l0 = l;
	}
	return e;
}

float wwCellExposureFull( const float * h, float adapted )
{
	const float cap = wwLookOn( WwLook::ExposureWide ) ? std::max( h[4], 16.0f ) : h[4];
	return std::min( std::max( h[8] / ( adapted + 0.001f ), h[5] ), cap );
}

float wwCellExposure( const float * h, float adapted )
{
	const WwCellEye eye = wwCellEye();
	const float cap = wwLookOn( WwLook::ExposureWide ) ? std::max( h[4], 16.0f ) : h[4];
	const float ad = adapted + 0.001f;
	const bool partial = !eye.red && ad > 0.0f && ad < eye.l0 && eye.a < 1.0f;
	const float raw = partial ? h[8] / ( std::pow( ad, eye.a ) * std::pow( eye.l0, 1.0f - eye.a ) ) : h[8] / ad;
	const float e = std::min( std::max( raw, h[5] ), cap );
	static float saidE = -1.0f;
	static int saidLaw = -1;
	const int law = eye.red ? 2 : partial ? 1 : 0;
	if ( law != saidLaw || std::fabs( e - saidE ) > 0.02f * std::max( saidE, 0.01f ) ) {	// telemetry: on a 2 % move
		saidLaw = law;
		saidE = e;
		qInfo( "cell eye: exposure %.4f cap %.3f adapted %.5f law %s (a=%.2f L0=%.3f; full adaptation would give %.4f)", double( e ),
			double( cap ), double( adapted ), law == 2 ? "full (the game's: Physical Light Units off, or the red WW_CELL_EYE_RED)" : law == 1 ? "partial" : "full (over L0)",
			double( eye.a ), double( eye.l0 ), double( wwCellExposureFull( h, adapted ) ) );
	}
	return e;
}

/* lane FIX1101A T (LATE ITEM 26): the cell view's tone curve is Fusion Shaders' Physical (res/shaders/features/exposure/cell_tonemap.glsl);
 * WW_TONEMAP_RED=vanilla keeps the game's filmic curve. Read per draw (live), echoed once per change. */
bool wwCellTonemapVanilla()
{
	WW_SWITCH_STATIC( int, pin, [] {
		const QByteArray v = wwEnv( "WW_TONEMAP_RED" ).trimmed().toLower();
		return v == "vanilla" ? 1 : v == "physical" ? 2 : 0;
	}() );
	/* MODULES1 R6-13: with Physical Light Units off the curve is the game's filmic one (vanilla Fallout 4 lighting is the
	 * default); WW_TONEMAP_RED=vanilla / physical pins either, WW_PHYSUNITS_VANILLA_RED=curve keeps Physical while off */
	const bool van = pin == 1 || ( pin == 0 && !wwPhysOn() && !wwPhysVanillaRed( "curve" ) );
	static int said = -1;
	if ( said != ( van ? 1 : 0 ) + pin * 2 ) {
		said = ( van ? 1 : 0 ) + pin * 2;
		std::fprintf( stderr, "tonemap: curve=%s (%s)\n", van ? "vanilla, the game's filmic" : "physical",
			pin ? "PINNED WW_TONEMAP_RED" : van ? "Physical Light Units off" : wwPhysOn() ? "Physical Light Units on" : "RED WW_PHYSUNITS_VANILLA_RED=curve" );
	}
	return van;
}

// lane MODULES1 (exposure, from gl/celllights.cpp): the imagespace row
bool wwCellImageSpaceOn()
{
	return st().isOn;
}

void wwCellImageSpaceSetOn( bool on )
{
	ClState & s = st();
	if ( s.isPinned )
		return;
	s.isOn = on;
	QSettings().setValue( QStringLiteral( "WW/CellImageSpace2" ), on );
}

bool wwCellImageSpaceWanted( Scene * scene )
{
	if ( !st().isOn || !scene || wwCellPassFor( scene->nifModel ) > 0 || !wwCellLightsWanted( scene ) )
		return false;	// a Pass shows the probes' own values, not the imagespace's grade
	const WwCellLighting * L = isLighting( scene->nifModel );	// an exterior's is the weather's
	return L && L->hasImageSpace;
}

void wwCellImageSpaceMeasuring( bool on )
{
	st().measuring = on;
}

bool wwCellImageSpaceIsMeasuring()
{
	return st().measuring;
}

bool wwCellImageSpaceMeasuresEffects()
{
	return !( st().isRed & 16 );
}

float wwCellImageSpaceAdapted( Scene * scene )
{
	return scene && scene->nifModel ? st().adapted.value( scene->nifModel, -1.0f ) : -1.0f;
}

void wwCellImageSpaceSetAdapted( Scene * scene, float lum, int pixels )
{
	if ( !scene || !scene->nifModel )
		return;
	st().adapted.insert( scene->nifModel, lum );
	st().adaptedPixels.insert( scene->nifModel, pixels );
}

void wwCellImageSpaceSetBloom( Scene * scene, const float * rgba, int w, int h, int step )
{
	if ( !scene || !scene->nifModel || !rgba || w <= 0 || h <= 0 || step < 1 )
		return;
	const WwCellLighting * L = isLighting( scene->nifModel );
	if ( !L || !L->hasImageSpace )
		return;
	ClState & s = st();
	ClBloom & B = s.bloom[scene->nifModel];
	// the source: the measure at a quarter of the view (a full-size measure box-averaged step x step)
	const int bw = std::max( 1, w / step ), bh = std::max( 1, h / step );
	const float thresh = L->isHdr[2], scale = L->isHdr[3];
	std::vector<float> src( size_t( bw ) * size_t( bh ) * 3 );
	B.lit = 0;
	for ( int y = 0; y < bh; y++ )
		for ( int x = 0; x < bw; x++ )
			for ( int c = 0; c < 3; c++ ) {
				double sum = 0.0;
				for ( int j = 0; j < step; j++ )
					for ( int i = 0; i < step; i++ ) {
						const float v = rgba[( size_t( y * step + j ) * size_t( w ) + size_t( x * step + i ) ) * 4 + size_t( c )];
						if ( std::isfinite( v ) )
							sum += v;
					}
				src[( size_t( y ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )] = float( sum / double( step * step ) );
			}
	// the game's blur: radius 7, 15 taps exp(-2 x^2 / 49), normalized; vertical with the bright pass
	// scale * max(0, c - threshold) per tap, then horizontal plain
	constexpr int r = 7;
	float wt[2 * r + 1];
	float wsum = 0.0f;
	for ( int x = -r; x <= r; x++ )
		wsum += ( wt[x + r] = std::exp( -2.0f * float( x * x ) / float( r * r ) ) );
	for ( float & v : wt )
		v /= wsum;
	for ( size_t i = 0; i < src.size(); i += 3 ) {
		bool any = false;
		for ( int c = 0; c < 3; c++ ) {
			src[i + size_t( c )] = scale * std::max( 0.0f, src[i + size_t( c )] - thresh );
			any = any || src[i + size_t( c )] > 0.0f;
		}
		B.lit += any ? 1 : 0;
	}
	std::vector<float> mid( src.size() );
	for ( int y = 0; y < bh; y++ )
		for ( int x = 0; x < bw; x++ )
			for ( int c = 0; c < 3; c++ ) {
				float a = 0.0f;
				for ( int k = -r; k <= r; k++ ) {
					const int yy = std::min( std::max( y + k, 0 ), bh - 1 );
					a += wt[k + r] * src[( size_t( yy ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )];
				}
				mid[( size_t( y ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )] = a;
			}
	B.rgb.assign( src.size(), 0.0f );
	B.peak = 0.0;
	for ( int y = 0; y < bh; y++ )
		for ( int x = 0; x < bw; x++ )
			for ( int c = 0; c < 3; c++ ) {
				float a = 0.0f;
				for ( int k = -r; k <= r; k++ ) {
					const int xx = std::min( std::max( x + k, 0 ), bw - 1 );
					a += wt[k + r] * mid[( size_t( y ) * size_t( bw ) + size_t( xx ) ) * 3 + size_t( c )];
				}
				B.rgb[( size_t( y ) * size_t( bw ) + size_t( x ) ) * 3 + size_t( c )] = a;
				B.peak = std::max( B.peak, double( a ) );
			}
	B.w = bw;
	B.h = bh;
	B.gpuTex = 0;	// lane PERFFIX1: this blur is the one shown (the red, or the echo before the GPU's replaces it)
	B.version = s.nextBloomVersion++;
}

// lane PERFFIX1: the bloom's threshold and scale, as wwCellImageSpaceSetBloom reads them (false: the cell has no imagespace)
bool wwCellImageSpaceBloomParams( Scene * scene, float & thresh, float & scale )
{
	const WwCellLighting * L = scene && scene->nifModel ? isLighting( scene->nifModel ) : nullptr;
	if ( !L || !L->hasImageSpace )
		return false;
	thresh = L->isHdr[2];
	scale = L->isHdr[3];
	return true;
}

// lane PERFFIX1: the GPU's blur (cellhdr.cpp bloomBlur) stands for the bloom from now on; the echo's lit/peak stay the CPU's
void wwCellImageSpaceSetBloomGpu( Scene * scene, unsigned tex, int w, int h )
{
	float thresh = 0.0f, scale = 0.0f;
	if ( !tex || w <= 0 || h <= 0 || !wwCellImageSpaceBloomParams( scene, thresh, scale ) )
		return;
	ClState & s = st();
	ClBloom & B = s.bloom[scene->nifModel];
	B.w = w;
	B.h = h;
	B.gpuTex = tex;
	B.version = s.nextBloomVersion++;
}

QString wwCellImageSpaceEcho( Scene * scene )
{
	ClState & s = st();
	const WwCellLighting * L = scene && scene->nifModel ? isLighting( scene->nifModel ) : nullptr;
	QString o = QStringLiteral( "imagespace=%1(asked=%2)" ).arg( wwCellImageSpaceWanted( scene ) ? "on" : "off" ).arg( s.isOn ? 1 : 0 );
	if ( !L || !L->hasImageSpace )
		return o + QStringLiteral( " (the cell has none)" );
	const float a = s.adapted.value( scene->nifModel, -1.0f );
	const float * h = L->isHdr;
	const float e = wwCellExposure( h, a );
	o += QStringLiteral( " %1 adapted=%2 over %3 px exposure=%4 tonemapE=%5 lut=%6" ).arg( L->isName )
		.arg( double( a ), 0, 'g', 7 ).arg( s.adaptedPixels.value( scene->nifModel ) ).arg( double( e ), 0, 'g', 7 )
		.arg( double( h[1] ), 0, 'g', 4 ).arg( L->isLut.empty() ? QStringLiteral( "none" ) : L->isLutPath );
	const ClBloom bl = s.bloom.value( scene->nifModel );
	o += QStringLiteral( " bloom=%1x%2 threshold=%3 scale=%4 lit=%5 peak=%6" ).arg( bl.w ).arg( bl.h )
		.arg( double( h[2] ), 0, 'g', 7 ).arg( double( h[3] ), 0, 'g', 7 ).arg( bl.lit ).arg( bl.peak, 0, 'g', 6 );
	if ( s.isRed )
		o += QStringLiteral( " red=%1" ).arg( s.isRed );
	o += wwCellTonemapVanilla() ? QStringLiteral( " curve=vanilla" ) : QStringLiteral( " curve=physical" );
	{	// lane MUSGI1: the eye's limit (wwCellExposure)
		const WwCellEye eye = wwCellEye();
		o += QStringLiteral( " eye=%1 a=%2 L0=%3 full=%4" ).arg( eye.red ? QStringLiteral( "red" ) : QStringLiteral( "limit" ) )
			.arg( double( eye.a ), 0, 'g', 3 ).arg( double( eye.l0 ), 0, 'g', 3 ).arg( double( wwCellExposureFull( h, a ) ), 0, 'g', 7 );
	}
	return o;
}
