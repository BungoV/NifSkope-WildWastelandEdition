/* lane SKY1 (2026-10-02): the sky and the sun in the cell view's bounce light; see probesky.h. */

#include "probesky.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "esmweather.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"	// lane GPURELIGHT1: Probe sharing
#include "gl/cellprobeview.h"
#include "gl/lookdevstage.h"
#include "gl/atmosphere.h"	// SKY3: the physical sky's capture height
#include "gl/scenelighting.h"
#include "probegi.h"
#include "gl/celllooks.h"	// lane LIGHTS5: the emittance row's live flip

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

bool ProbeSkyLight::same( const ProbeSkyLight & o ) const
{
	return on == o.on && std::memcmp( amb, o.amb, sizeof amb ) == 0 && std::memcmp( sunTo, o.sunTo, sizeof sunTo ) == 0
		&& std::memcmp( sun, o.sun, sizeof sun ) == 0 && vis == o.vis && pic == o.pic && ( !pic || std::memcmp( picQ, o.picQ, sizeof picQ ) == 0 );
}

/* ---- lane SKYPIC1: the drawn sky ---- */
namespace
{
bool skyPicOn()
{
	// on by default since 2026-10-15 (bungo); WW_CELL_SKYPIC=0 gives the weather's flat ambient (SKY1)
	return !wwEnvIsSet( "WW_CELL_SKYPIC" ) || wwEnvInt( "WW_CELL_SKYPIC" ) == 1;
}

//! WW_CELL_SKYPIC_FLAT=r,g,b (the known-answer control): the capture is that one colour; with the switch off the
//! weather's six colours are all that colour, so the two runs must agree
bool skyPicFlat( float rgb[3] )
{
	const QStringList p = QString::fromLatin1( wwEnv( "WW_CELL_SKYPIC_FLAT" ) ).split( QLatin1Char( ',' ) );
	if ( p.size() != 3 )
		return false;
	for ( int c = 0; c < 3; c++ )
		rgb[c] = p[c].toFloat();
	return true;
}

struct PicNow
{
	QString key;            //!< the capture's key (wwLookdevSkyPictureKey + the flat control); empty = none tried
	bool ok = false;
	WwSkyPicture pic;
	double Q[6][8][3] = {};
	QString why;
	float below[3] = { 0, 0, 0 };   //!< what stands below the horizon (the weather's ground-facing colour)
};

PicNow & picNow()
{
	static PicNow p;
	return p;
}

float captureZNow();

QString picKeyNow()
{
	float f[3];
	const float z = captureZNow();
	return wwLookdevSkyPictureKey() + ( skyPicFlat( f ) ? QStringLiteral( "|flat %1 %2 %3" ).arg( double( f[0] ) )
		.arg( double( f[1] ) ).arg( double( f[2] ) ) : QString() )
		// SKY3: only while the probes take the physical sky (the dome has no height; Off keeps today's key)
		+ ( std::isfinite( z ) ? QStringLiteral( "|z %1" ).arg( double( z ), 0, 'f', 0 ) : QString() );
}
}	// namespace

void probeSkyPictureQ( const WwSkyPicture & pic, const QString & red, double Q[6][8][3] )
{
	std::memset( Q, 0, sizeof( double ) * 6 * 8 * 3 );
	const int n = pic.n;
	if ( n <= 0 || pic.rgb.size() != size_t( 6 ) * size_t( n ) * size_t( n ) * 3 )
		return;
	const bool flipz = red == QLatin1String( "flipz" ), nocos = red == QLatin1String( "nocos" );
	auto A = []( double x, double y ) { return std::atan2( x * y, std::sqrt( x * x + y * y + 1.0 ) ); };
	for ( int face = 0; face < 6; face++ ) {
		float F[3], R[3], U[3];
		wwSkyPictureFace( face, F, R, U );
		// red flipz: every texel is read from the face mirrored in z (the picture upside down)
		int src = face;
		if ( flipz && face >= 4 )
			src = face ^ 1;
		const float * img = pic.rgb.data() + size_t( src ) * size_t( n ) * size_t( n ) * 3;
		for ( int j = 0; j < n; j++ ) {
			const double y0 = 2.0 * j / n - 1.0, y1 = 2.0 * ( j + 1 ) / n - 1.0, sy = 0.5 * ( y0 + y1 );
			for ( int i = 0; i < n; i++ ) {
				const double x0 = 2.0 * i / n - 1.0, x1 = 2.0 * ( i + 1 ) / n - 1.0, sx = 0.5 * ( x0 + x1 );
				const double om = A( x0, y0 ) - A( x0, y1 ) - A( x1, y0 ) + A( x1, y1 );	// the texel's solid angle
				double d[3];
				for ( int c = 0; c < 3; c++ )
					d[c] = F[c] + sx * R[c] + sy * U[c];
				const double l = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
				for ( double & c : d )
					c /= l;
				int row = j;
				if ( flipz && face < 4 )
					row = n - 1 - j;	// the side faces: up is +Z, so the mirror is the row
				const float * L = img + ( size_t( row ) * size_t( n ) + size_t( i ) ) * 3;
				const int o = ( d[0] < 0 ? 1 : 0 ) | ( d[1] < 0 ? 2 : 0 ) | ( d[2] < 0 ? 4 : 0 );	// the bake's octant
				for ( int a = 0; a < 6; a++ ) {
					const double cosA = ( a & 1 ) ? -d[a >> 1] : d[a >> 1];
					if ( cosA <= 0.0 )
						continue;
					const double w = om * ( nocos ? 1.0 : cosA );
					for ( int c = 0; c < 3; c++ )
						Q[a][o][c] += w * double( L[c] );
				}
			}
		}
	}
}

ProbeSkyLight probeSkyLightNow()
{
	ProbeSkyLight s;
	if ( !wwLookdevActive() )
		return s;
	float dir[3] = { 0, 0, 1 }, dif[4] = { 0, 0, 0, 1 }, amb[4] = { 0, 0, 0, 1 }, dalc[6][3];
	wwLookdevLight( dir, dif, amb );
	const bool fromWeather = wwLookdevDalc( dalc );
	// the weather names its six colors X+ X- Y+ Y- Z+ Z-; a surface facing up takes Z- (measured, lane
	// PBRR3; the PBR program's pick in pbrm_default.frag, and its red "dalcflip")
	const bool flip = wwLookdevRed( "dalcflip" );
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			s.amb[a][c] = dalc[flip ? a : ( a ^ 1 )][c];
	const double dl = std::sqrt( double( dir[0] ) * dir[0] + double( dir[1] ) * dir[1] + double( dir[2] ) * dir[2] );
	/* the PBR program's sun strength in a scene mode; lane BOUNCE1: x the imagespace Sunlight Scale, the sun the
	 * cell view's exterior surfaces draw (celllights.cpp), so the bounce is the sun the game lights with */
	const float sunScale = wwStudioSunScale() * wwLookdevSunlightScale();
	for ( int c = 0; c < 3; c++ ) {
		s.sunTo[c] = float( dir[c] / std::max( dl, 1e-9 ) );
		s.sun[c] = dif[c] * sunScale;
	}
	s.label = QStringLiteral( "%1 hour %2%3" ).arg( wwLookdevWeatherKey() ).arg( wwLookdevHour(), 0, 'f', 2 )
		.arg( fromWeather ? QString() : QStringLiteral( " (no weather record: the neutral light)" ) );
	s.on = true;
	s.vis = wwCellSkyVis();	// lane SKYSTR1: a change of the row relights as a change of weather does
	// lane SKYPIC1: the drawn sky per direction, once a frame has captured it for this weather and hour
	float flat[3];
	const bool haveFlat = skyPicFlat( flat );
	if ( skyPicOn() ) {
		const PicNow & p = picNow();
		if ( p.ok && p.key == picKeyNow() ) {
			s.pic = true;
			std::memcpy( s.picQ, p.Q, sizeof s.picQ );
			s.picNote = QStringLiteral( "the drawn sky (%1x%1 a face; %2)" ).arg( p.pic.n ).arg( p.pic.drew );
		} else {
			s.picNote = p.key == picKeyNow() ? QStringLiteral( "refused (%1): the weather's six colours" ).arg( p.why )
				: QStringLiteral( "no frame has drawn this sky yet: the weather's six colours until one does" );
		}
	} else if ( haveFlat ) {
		for ( int a = 0; a < 6; a++ )   // the known-answer control's other arm: SKY1's flat sky in that colour
			for ( int c = 0; c < 3; c++ )
				s.amb[a][c] = flat[c];
	}
	return s;
}

/* lane SKYSTR1: an octant's sky visibility as the relight takes it -- the stored share, opened toward the free sky by
 * the Sky visibility row as FO4CS's Skylighting strength does (lerp(1, v, strength)); a full row leaves it bit for bit */
static double octantVis( float v, const ProbeSkyLight & sky, bool redNoVis )
{
	if ( redNoVis )
		return 1.0;
	const double s = std::min( std::max( double( v ), 0.0 ), 1.0 );
	if ( sky.vis >= 1.0f )
		return s;
	return 1.0 + double( std::max( sky.vis, 0.0f ) ) * ( s - 1.0 );
}

void probeSkyCube( const float vis[8], const unsigned char ( *tint )[3], const ProbeSkyLight & sky, bool redNoVis,
	bool redNoTint, double E[6][3] )
{
	const double kPi = 3.14159265358979323846;
	if ( sky.pic ) {   // lane SKYPIC1: each octant's own drawn sky on this axis
		for ( int a = 0; a < 6; a++ )
			for ( int o = 0; o < 8; o++ ) {
				const double s = octantVis( vis[o], sky, redNoVis );
				for ( int c = 0; c < 3; c++ )
					E[a][c] += s * ( tint && !redNoTint ? tint[o][c] / 255.0 : 1.0 ) * sky.picQ[a][o][c];
			}
		return;
	}
	for ( int a = 0; a < 6; a++ ) {
		const int ax = a >> 1;
		const bool neg = ( a & 1 ) != 0;
		double v[3] = { 0, 0, 0 };
		for ( int o = 0; o < 8; o++ ) {
			// the bake's octant: bit 0 x < 0, bit 1 y < 0, bit 2 z < 0
			if ( ( ( ( o >> ax ) & 1 ) != 0 ) != neg )
				continue;
			const double s = octantVis( vis[o], sky, redNoVis );
			for ( int c = 0; c < 3; c++ )
				v[c] += s * ( tint && !redNoTint ? tint[o][c] / 255.0 : 1.0 );
		}
		// irradiance: the ambient color is what a white surface shows, so E = pi x color in the open
		for ( int c = 0; c < 3; c++ )
			E[a][c] += kPi * double( sky.amb[a][c] ) * v[c] * 0.25;
	}
}

namespace
{

/* lane GIPROBE2: Probe light smoothing. A relight no longer pops: the published grid fades toward the new one, each
 * texel with its own time constant tau = kFadeTau / (1 + 4 c), c its relative luminance change (a big change settles
 * faster; the per-texel rate is our own choice after DDGI's per-probe hysteresis). Steps at most every kFadeStepMs,
 * then the exact new grid at kFadeEndMs (4 tau: within 2 %). Another publish (a door run, a reopen) ends it. */
constexpr double kFadeTau = 1.0;
constexpr qint64 kFadeStepMs = 66, kFadeEndMs = 4000;

struct Fade
{
	const void * nif = nullptr;
	std::vector<float> to, rate;   // the target grid; per texel 1 / tau in 1/s
	QElapsedTimer clock;
	qint64 lastMs = 0;
	int ver = 0;                   // the publish this fade edits
	int steps = 0;
	double msSteps = 0;
};

struct Kept
{
	const void * nif = nullptr;
	ProbeSoup soup;
	QString bakeDir;
	ProbeGiSpec spec;
	ProbeSkyLight wanted;
	bool skyKept = true;   // lane GPURELIGHT1: false = a cell kept only for Probe sharing or, lane LIGHTS5, for the emittance row (no weather relight)
	QPointer<QTimer> timer;   // the view's child: it goes with the window
	QPointer<QWindow> view;
	Fade fade;   // lane GIPROBE2
};

Kept & kept()
{
	static Kept k;
	return k;
}

/* SKY3: the physical sky's capture stands at the kept exterior's probes' median height. It took the view camera's: the
 * probes' sky depended on where the camera was at the first capture (a raised camera thins the haze). Red
 * WW_ATMO_GI_RED=camz: the view's, as before. NaN while the dome is captured (no height) or no probe is known yet. */
float captureZNow()
{
	const Kept & k = kept();
	if ( !k.nif || !wwAtmoGiActive() || wwAtmoGiRedDome() || wwAtmoGiRedCamZ() )
		return NAN;
	const WwCellProbeView * pv = wwCellProbeViewFor( k.nif );
	if ( !pv || pv->probes.size() < 21 )
		return NAN;
	static const float * seenData = nullptr;
	static size_t seenSize = 0;
	static float z = NAN;
	if ( pv->probes.data() != seenData || pv->probes.size() != seenSize ) {
		std::vector<float> zs;
		zs.reserve( pv->probes.size() / 21 );
		for ( size_t i = 0; i + 21 <= pv->probes.size(); i += 21 )
			zs.push_back( pv->probes[i + 2] );
		std::nth_element( zs.begin(), zs.begin() + std::ptrdiff_t( zs.size() / 2 ), zs.end() );
		z = std::round( zs[zs.size() / 2] / 64.0f ) * 64.0f;	// 64 u steps: a relight's jitter never recaptures
		seenData = pv->probes.data();
		seenSize = pv->probes.size();
	}
	return z;
}

//! lane GIPROBE2: one smoothing step (or the snap to the target); false when no fade runs
bool fadeStep( Kept & k, bool snap )
{
	Fade & f = k.fade;
	if ( !f.nif )
		return false;
	std::vector<float> * cur = wwCellGiRgbaEdit( f.nif );
	if ( !cur || wwCellGiVersion( f.nif ) != f.ver || cur->size() != f.to.size() ) {
		f = Fade();   // another publish took the grid over
		return false;
	}
	const qint64 now = f.clock.elapsed();
	if ( !snap && now - f.lastMs < kFadeStepMs )
		return true;
	const bool end = snap || now >= kFadeEndMs;
	const double dt = double( now - f.lastMs ) / 1000.0;
	f.lastMs = now;
	QElapsedTimer t;
	t.start();
	if ( end ) {
		*cur = f.to;
	} else {
		const size_t n = f.rate.size();
		const int nt = int( std::max( 1u, std::min( 8u, std::thread::hardware_concurrency() ) ) );
		std::vector<std::thread> pool;
		for ( int w = 0; w < nt; w++ )
			pool.emplace_back( [&, w]() {
				for ( size_t i = n * size_t( w ) / size_t( nt ), e = n * size_t( w + 1 ) / size_t( nt ); i < e; i++ ) {
					const float a = float( 1.0 - std::exp( -dt * double( f.rate[i] ) ) );
					for ( size_t c = 0; c < 4; c++ ) {
						float & v = ( *cur )[i * 4 + c];
						v += a * ( f.to[i * 4 + c] - v );
					}
				}
			} );
		for ( std::thread & th : pool )
			th.join();
	}
	f.steps++;
	f.msSteps += double( t.nsecsElapsed() ) / 1e6;
	wwCellGiRgbaEdited( f.nif );
	if ( end ) {
		std::fprintf( stderr, "gi smooth: %d steps over %lld ms%s, CPU ms a step %.2f, texels %zu\n", f.steps, now,
			snap ? " (snapped)" : "", f.msSteps / std::max( 1, f.steps ), f.rate.size() );
		f = Fade();
	}
	return true;
}

//! lane GIPROBE2: start a fade from the grid on screen to `to` (moved in); gi.rgba becomes the grid on screen
void fadeBegin( Kept & k, const WwCellGi & was, WwCellGi & gi )
{
	Fade & f = k.fade;
	f = Fade();
	f.to = std::move( gi.rgba );
	gi.rgba = was.rgba;
	const size_t n = f.to.size() / 4;
	f.rate.resize( n );
	for ( size_t i = 0; i < n; i++ ) {
		const float * a = &gi.rgba[i * 4], * b = &f.to[i * 4];
		const double la = 0.2126 * a[0] + 0.7152 * a[1] + 0.0722 * a[2], lb = 0.2126 * b[0] + 0.7152 * b[1] + 0.0722 * b[2];
		const double c = std::abs( lb - la ) / std::max( { std::abs( la ), std::abs( lb ), 1e-6 } );
		f.rate[i] = float( ( 1.0 + 4.0 * std::min( c, 1.0 ) ) / kFadeTau );
	}
}

//! lane SKYPIC1: the relight that took the drawn sky states it, and a gate's dump folder gets it beside the bake's
void skyPicRelit( const ProbeGiResult & gr, const ProbeGiSpec & spec )
{
	const ProbeSkyLight & s = spec.sky;
	QString open;
	for ( int a = 0; a < 6; a++ ) {
		double q[3] = { 0, 0, 0 };
		for ( int o = 0; o < 8; o++ )
			for ( int c = 0; c < 3; c++ )
				q[c] += s.picQ[a][o][c];
		const double w = ( double( s.amb[a][0] ) + s.amb[a][1] + s.amb[a][2] ) * 3.14159265358979323846;
		open += QStringLiteral( " %1 %2" ).arg( QStringLiteral( "+X,-X,+Y,-Y,+Z,-Z" ).split( QLatin1Char( ',' ) )[a] )
			.arg( w > 0 ? ( q[0] + q[1] + q[2] ) / w : 0.0, 0, 'f', 3 );
	}
	const PicNow & p = picNow();
	std::fprintf( stderr, "gi skypic: %s; relit %s; captured in %.1f ms; open sky drawn / weather per axis:%s\n",
		qPrintable( s.picNote ), qPrintable( s.label ), p.pic.ms, qPrintable( open ) );
	std::fprintf( stderr, "gi skypic census: %s\n", qPrintable( probeGiCensusText( gr ).replace( QLatin1Char( '\n' ), QStringLiteral( " / " ) ) ) );
	const QString dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_GI_DUMP" ) );
	if ( dump.isEmpty() )
		return;
	QString err;
	if ( !probeGiDump( gr, spec, dump, &err ) ) {
		std::fprintf( stderr, "gi skypic dump FAILED: %s\n", qPrintable( err ) );
		return;
	}
	// gi_skypic.bin: "SKYPIC1\0", int32 n, then 6 x n x n x 3 float32 (face +X -X +Y -Y +Z -Z, row from the bottom)
	QFile b( QDir( dump ).filePath( QStringLiteral( "gi_skypic.bin" ) ) );
	if ( b.open( QIODevice::WriteOnly ) ) {
		b.write( "SKYPIC1", 8 );
		const qint32 n = p.pic.n;
		b.write( reinterpret_cast<const char *>( &n ), 4 );
		b.write( reinterpret_cast<const char *>( p.pic.rgb.data() ), qint64( p.pic.rgb.size() * sizeof( float ) ) );
	}
	QFile t( QDir( dump ).filePath( QStringLiteral( "gi_skypic.txt" ) ) );
	if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QString x = QStringLiteral( "key %1\ndrew %2\nn %3\nms %4\nnote %5\nbelow %6 %7 %8\n" ).arg( p.key, p.pic.drew )
			.arg( p.pic.n ).arg( p.pic.ms, 0, 'f', 2 ).arg( s.picNote ).arg( double( p.below[0] ), 0, 'g', 9 )
			.arg( double( p.below[1] ), 0, 'g', 9 ).arg( double( p.below[2] ), 0, 'g', 9 );
		for ( int f = 0; f < 6; f++ ) {
			float F[3], R[3], U[3];
			wwSkyPictureFace( f, F, R, U );
			x += QStringLiteral( "face %1 F %2 %3 %4 R %5 %6 %7 U %8 %9 %10\n" ).arg( f ).arg( F[0] ).arg( F[1] ).arg( F[2] )
				.arg( R[0] ).arg( R[1] ).arg( R[2] ).arg( U[0] ).arg( U[1] ).arg( U[2] );
		}
		for ( int a = 0; a < 6; a++ )
			for ( int o = 0; o < 8; o++ )
				x += QStringLiteral( "q %1 %2 %3 %4 %5\n" ).arg( a ).arg( o ).arg( s.picQ[a][o][0], 0, 'g', 12 )
					.arg( s.picQ[a][o][1], 0, 'g', 12 ).arg( s.picQ[a][o][2], 0, 'g', 12 );
		t.write( x.toUtf8() );
	}
}

void relightKept( bool emitOnly = false )
{
	Kept & k = kept();
	const WwCellLighting * L = k.nif ? wwCellLightsFor( k.nif ) : nullptr;
	if ( !L || ( k.skyKept && L->interior && !k.spec.interiorSky && !emitOnly ) )   // lane SKYINT1: an interior that shows the sky too
		return;
	if ( k.skyKept )
		k.spec.sky = k.wanted;
	ProbeGiResult gr;
	if ( !probeGiRelight( k.soup, k.bakeDir, *L, k.spec, &gr ) )
		return;
	if ( k.spec.sky.pic )
		skyPicRelit( gr, k.spec );
	WwCellGi gi;
	for ( int c = 0; c < 3; c++ ) {
		gi.origin[c] = gr.origin[c];
		gi.dims[c] = gr.dims[c];
	}
	gi.voxel = gr.voxel;
	gi.skyLit = gr.skyLit && !L->interior && k.spec.skyRed != QLatin1String( "keepamb" );
	gi.summary = QStringLiteral( "grid %1x%2x%3 voxel %4" ).arg( gr.dims[0] ).arg( gr.dims[1] ).arg( gr.dims[2] )
		.arg( double( gr.voxel ), 0, 'f', 1 );
	probeGiAoFreeSwap( gr );	// lane AODECAL1: the kept spec carries the decal copies
	gi.rgba = std::move( gr.grid );
	gi.sky = std::move( gr.gridSky );	// the Pass drop-down's sky share (weather-free, carried over)
	probeGiRoomsInto( gr, gi );   // lane ROOMCLAMP1
	// lane GIPROBE2: Probe light smoothing: the same grid shape on screen fades toward this relight
	const WwCellGi * was = wwLookOn( WwLook::ProbeSmooth ) ? wwCellGiFor( k.nif ) : nullptr;
	const bool fading = was && was->rgba.size() == gi.rgba.size() && was->rgba != gi.rgba && was->voxel == gi.voxel
		&& std::equal( was->dims, was->dims + 3, gi.dims ) && std::equal( was->origin, was->origin + 3, gi.origin );
	k.fade = Fade();
	if ( fading )
		fadeBegin( k, *was, gi );   // copies the old values before the publish replaces them
	wwCellGiPublish( k.nif, gi );
	if ( fading ) {
		k.fade.nif = k.nif;
		k.fade.ver = wwCellGiVersion( k.nif );
		k.fade.clock.start();
	}
	// the surfel and probe previews carry the relit light: publish them again, keeping Show probes
	WwCellProbeView pv;
	if ( const WwCellProbeView * old = wwCellProbeViewFor( k.nif ) )
		pv.probesShown = old->probesShown;
	pv.surfelCell = gr.surfelCell;
	pv.surfels = std::move( gr.surfelOut );
	pv.probes = std::move( gr.probeCube );
	pv.probeSky = std::move( gr.probeSky );
	pv.linkStart = std::move( gr.probeLinkStart );
	pv.links = std::move( gr.probeLinks );
	wwCellProbeViewPublish( k.nif, pv );
	if ( k.view )
		k.view->requestUpdate();
}

}	// namespace

bool probeSkyRelightPending()
{
	const Kept & k = kept();
	return k.timer && k.timer->isActive();
}

void probeSkyRelightNow()
{
	Kept & k = kept();
	if ( !k.timer || !k.timer->isActive() )
		return;
	k.timer->stop();
	relightKept();
}

void probeSkyForget()
{
	Kept & k = kept();
	if ( k.timer )
		k.timer->stop();
	k.nif = nullptr;
	k.fade = Fade();   // lane GIPROBE2
	picNow() = PicNow();
}

void probeSkyKeep( const void * nif, const ProbeSoup & soup, const QString & bakeDir, const ProbeGiSpec & spec, bool skyKept )
{
	Kept & k = kept();   // one cell at a time: the latest relit
	k.nif = nif;
	k.soup = soup;
	k.bakeDir = bakeDir;
	k.spec = spec;
	k.spec.record = nullptr;   // the caller's operators live on its stack; a later relight records nothing
	k.wanted = spec.sky;
	k.skyKept = skyKept;
	k.fade = Fade();   // lane GIPROBE2: a new cell's grid is published whole
}

void probeSkyTick( Scene * scene, const void * nif, QWindow * view )
{
	Kept & k = kept();
	if ( !k.nif || k.nif != nif || !wwCellGiOn() )
		return;
	// lane GIPROBE2: a smoothing step while a fade runs (the row off: the exact grid at once); keep painting until done
	if ( fadeStep( k, !wwLookOn( WwLook::ProbeSmooth ) ) && view )
		view->requestUpdate();
	// lane GPURELIGHT1: a Probe sharing tick relights at once (a toggle, nothing to wait out)
	// lane GIPROBE2: and so do Wide probe sharing (3 steps, implies sharing) and Probe depth visibility
	const bool wide = wwLookOn( WwLook::ProbeShareWide );
	const bool share = wwLookOn( WwLook::ProbeShare ) || wide;
	const int sharePasses = wide ? 3 : 1;
	const QString shareRed = wwEnvStr( "WW_CELL_GI_SHARE_RED" ).trimmed();
	const bool depthVis = wwLookOn( WwLook::ProbeDepthVis );
	const QString depthVisRed = wwEnvStr( "WW_CELL_GI_DEPTHVIS_RED" ).trimmed();
	if ( share != k.spec.share || shareRed != k.spec.shareRed || sharePasses != k.spec.sharePasses
		|| depthVis != k.spec.depthVis || depthVisRed != k.spec.depthVisRed ) {
		k.spec.share = share;
		k.spec.shareRed = shareRed;
		k.spec.sharePasses = sharePasses;
		k.spec.depthVis = depthVis;
		k.spec.depthVisRed = depthVisRed;
		if ( view )
			k.view = view;   // the fade's repaints
		relightKept();
		return;
	}
	if ( k.spec.emitBaked >= 0 ) {   // lane LIGHTS5: the "Emittance in bounce light" row, relit from the kept other state
		const int want = wwLookOn( WwLook::BounceEmittance ) ? 1 : 0;
		const bool stale = wwLookBounceEmitRedStale();
		if ( want != k.spec.emitWant || stale != k.spec.emitStale ) {
			k.spec.emitWant = want;
			k.spec.emitStale = stale;
			relightKept( true );
			return;
		}
	}
	if ( !k.skyKept )
		return;
	bool firstPic = false;
	if ( skyPicOn() && wwLookdevActive() ) {   // lane SKYPIC1: capture the drawn sky when it changed
		PicNow & p = picNow();
		const QString key = picKeyNow();
		if ( p.key != key ) {
			float flat[3];
			const bool haveFlat = skyPicFlat( flat );
			const int n = wwEnvIsSet( "WW_CELL_SKYPIC_N" ) ? wwEnvInt( "WW_CELL_SKYPIC_N" ) : 64;
			p.key = key;
			p.why.clear();
			wwLookdevSetSkyCaptureZ( captureZNow() );
			p.ok = wwLookdevCaptureSky( scene, n, haveFlat ? flat : nullptr, p.pic, &p.why );
			wwLookdevSetSkyCaptureZ( NAN );
			if ( p.ok ) {
				/* below the horizon the game shows ground (the far terrain), never sky; NifSkope's dome goes on below it
				 * only because the cell's ground ends. There the probes take the weather's ground-facing colour, as SKY1
				 * did (a surface facing down in the open takes pi x that colour, as before); the flat control: its colour */
				const ProbeSkyLight w = probeSkyLightNow();
				for ( int c = 0; c < 3; c++ )
					p.below[c] = haveFlat ? flat[c] : w.amb[5][c];
				const int m = p.pic.n;
				for ( int face = 0; face < 6; face++ ) {
					float F[3], R[3], U[3];
					wwSkyPictureFace( face, F, R, U );
					for ( int j = 0; j < m; j++ )
						for ( int i = 0; i < m; i++ ) {
							const double sx = 2.0 * ( i + 0.5 ) / m - 1.0, sy = 2.0 * ( j + 0.5 ) / m - 1.0;
							if ( F[2] + sx * R[2] + sy * U[2] >= 0.0 )
								continue;
							float * t = p.pic.rgb.data() + ( ( size_t( face ) * size_t( m ) + size_t( j ) ) * size_t( m ) + size_t( i ) ) * 3;
							for ( int c = 0; c < 3; c++ )
								t[c] = p.below[c];
						}
				}
			}
			if ( p.ok )
				probeSkyPictureQ( p.pic, QString::fromLatin1( wwEnv( "WW_CELL_SKYPIC_RED" ) ).trimmed(), p.Q );
			else
				std::fprintf( stderr, "gi skypic: capture refused (%s)\n", qPrintable( p.why ) );
			firstPic = p.ok && !k.wanted.pic;
		}
	}
	const ProbeSkyLight now = probeSkyLightNow();
	if ( now.same( k.wanted ) )
		return;
	if ( firstPic && now.pic ) {   // the cell opened before any frame: take the drawn sky now, not 0.4 s later
		k.wanted = now;
		relightKept();
		return;
	}
	// the light moved: relight once it has stood still (an hour slider drags through many values)
	k.wanted = now;
	if ( !k.timer || k.view != view ) {
		delete k.timer.data();
		k.timer = new QTimer( view );
		k.timer->setSingleShot( true );
		QObject::connect( k.timer.data(), &QTimer::timeout, []() { relightKept(); } );
	}
	k.view = view;
	k.timer->start( 400 );
}
