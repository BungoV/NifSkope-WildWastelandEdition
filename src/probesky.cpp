/* lane SKY1 (2026-10-02): the sky and the sun in the cell view's bounce light; see probesky.h. */

#include "probesky.h"

#include "esmweather.h"
#include "gl/celllights.h"
#include "gl/lookdevstage.h"
#include "gl/scenelighting.h"
#include "probegi.h"

#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <cstring>

bool ProbeSkyLight::same( const ProbeSkyLight & o ) const
{
	return on == o.on && std::memcmp( amb, o.amb, sizeof amb ) == 0 && std::memcmp( sunTo, o.sunTo, sizeof sunTo ) == 0
		&& std::memcmp( sun, o.sun, sizeof sun ) == 0;
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
	const float sunScale = wwStudioSunScale();   // the PBR program's sun strength in a scene mode
	for ( int c = 0; c < 3; c++ ) {
		s.sunTo[c] = float( dir[c] / std::max( dl, 1e-9 ) );
		s.sun[c] = dif[c] * sunScale;
	}
	s.label = QStringLiteral( "%1 hour %2%3" ).arg( wwLookdevWeatherKey() ).arg( wwLookdevHour(), 0, 'f', 2 )
		.arg( fromWeather ? QString() : QStringLiteral( " (no weather record: the neutral light)" ) );
	s.on = true;
	return s;
}

void probeSkyCube( const float vis[8], const unsigned char ( *tint )[3], const ProbeSkyLight & sky, bool redNoVis,
	bool redNoTint, double E[6][3] )
{
	const double kPi = 3.14159265358979323846;
	for ( int a = 0; a < 6; a++ ) {
		const int ax = a >> 1;
		const bool neg = ( a & 1 ) != 0;
		double v[3] = { 0, 0, 0 };
		for ( int o = 0; o < 8; o++ ) {
			// the bake's octant: bit 0 x < 0, bit 1 y < 0, bit 2 z < 0
			if ( ( ( ( o >> ax ) & 1 ) != 0 ) != neg )
				continue;
			const double s = redNoVis ? 1.0 : std::min( std::max( double( vis[o] ), 0.0 ), 1.0 );
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

struct Kept
{
	const void * nif = nullptr;
	ProbeSoup soup;
	QString bakeDir;
	ProbeGiSpec spec;
	ProbeSkyLight wanted;
	QPointer<QTimer> timer;   // the view's child: it goes with the window
	QPointer<QWindow> view;
};

Kept & kept()
{
	static Kept k;
	return k;
}

void relightKept()
{
	Kept & k = kept();
	const WwCellLighting * L = k.nif ? wwCellLightsFor( k.nif ) : nullptr;
	if ( !L || L->interior )
		return;
	k.spec.sky = k.wanted;
	ProbeGiResult gr;
	if ( !probeGiRelight( k.soup, k.bakeDir, *L, k.spec, &gr ) )
		return;
	WwCellGi gi;
	for ( int c = 0; c < 3; c++ ) {
		gi.origin[c] = gr.origin[c];
		gi.dims[c] = gr.dims[c];
	}
	gi.voxel = gr.voxel;
	gi.sky = gr.sky && k.spec.skyRed != QLatin1String( "keepamb" );
	gi.summary = QStringLiteral( "grid %1x%2x%3 voxel %4" ).arg( gr.dims[0] ).arg( gr.dims[1] ).arg( gr.dims[2] )
		.arg( double( gr.voxel ), 0, 'f', 1 );
	gi.rgba = std::move( gr.grid );
	wwCellGiPublish( k.nif, gi );
	if ( k.view )
		k.view->requestUpdate();
}

}	// namespace

void probeSkyKeep( const void * nif, const ProbeSoup & soup, const QString & bakeDir, const ProbeGiSpec & spec )
{
	Kept & k = kept();   // one exterior at a time: the latest relit
	k.nif = nif;
	k.soup = soup;
	k.bakeDir = bakeDir;
	k.spec = spec;
	k.wanted = spec.sky;
}

void probeSkyTick( const void * nif, QWindow * view )
{
	Kept & k = kept();
	if ( !k.nif || k.nif != nif || !wwCellGiOn() )
		return;
	const ProbeSkyLight now = probeSkyLightNow();
	if ( now.same( k.wanted ) )
		return;
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
