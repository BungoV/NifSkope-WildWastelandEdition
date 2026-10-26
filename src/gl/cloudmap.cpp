#include "cloudmap.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdio>

// lane CLOUD1: the cloud map (cloudmap.h). This first cut is the STUB the interface promised: the plain 2D-layer
// union of the weather's JNAM alphas, every layer in the Low band, a 1x1 map.

namespace
{

constexpr const char * kKey = "Settings/Render/Scene/Cloud Map";
constexpr float kUnitsPerM = 69.99f;	// game units per metre (the engine's 1/0.0142875)

struct CmState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	WwCloudMap map;
	QString key;	// what the current map was built from
	QString last = QStringLiteral( "off" );
};

CmState & cs()
{
	static CmState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_CLOUDMAP" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;
		s.key.clear();	// a live set rebuilds
		const QString e = wwEnvStr( "WW_CLOUDMAP" ).trimmed().toLower();
		if ( !e.isEmpty() ) {
			bool ok = false;
			const int v = e.toInt( &ok );
			s.tier = ok ? std::clamp( v, 0, 4 )
				: e == QLatin1StringView( "low" ) ? 1 : e == QLatin1StringView( "medium" ) ? 2
				: e == QLatin1StringView( "high" ) ? 3 : e == QLatin1StringView( "ultra" ) ? 4 : 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.tier = 0;	// a harness measures what it pins
			s.pinned = true;
		} else {
			s.tier = std::clamp( QSettings().value( QLatin1StringView( kKey ), 0 ).toInt(), 0, 4 );
		}
	}
	return s;
}

void say( CmState & s, const QString & line )
{
	if ( line != s.last )
		fprintf( stderr, "cloudmap: %s\n", qPrintable( line ) );
	s.last = line;
}

void drop( Scene * scene, CmState & s )
{
	if ( s.map.tex && scene && scene->renderer )
		scene->renderer->fn->glDeleteTextures( 1, &s.map.tex );
	const quint64 g = s.map.generation;
	s.map = WwCloudMap();
	s.map.generation = g + 1;
	s.key.clear();
}

} // namespace

int wwCloudMapTier()
{
	return cs().tier;
}

void wwCloudMapSetTier( int tier )
{
	CmState & s = cs();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, 0, 4 );
	s.key.clear();
	QSettings().setValue( QLatin1StringView( kKey ), s.tier );
}

bool wwCloudMapPinned()
{
	return cs().pinned;
}

int wwCloudMapSizeFor( int tier )
{
	static const int k[5] = { 0, 256, 512, 512, 1024 };
	return k[std::clamp( tier, 0, 4 )];
}

const WwCloudMap & wwCloudMap()
{
	return cs().map;
}

const WwCloudMap & wwCloudMapUpdate( Scene * scene )
{
	CmState & s = cs();
	if ( !s.tier || !scene || !scene->renderer ) {
		if ( s.map.valid || s.map.tex )
			drop( scene, s );
		say( s, QStringLiteral( "off" ) );
		return s.map;
	}
	QVector<WwCloudLayerIn> layers;
	QString wkey, why;
	float eye[3] = { 0, 0, 0 };
	if ( !wwLookdevCloudLayers( nullptr, layers, wkey, eye, &why ) ) {
		if ( s.map.valid || s.map.tex )
			drop( scene, s );
		say( s, QStringLiteral( "none(%1)" ).arg( why ) );
		return s.map;
	}
	// the stub: every active layer's alpha unioned into the Low band
	double keep = 1.0;
	for ( const WwCloudLayerIn & L : layers )
		keep *= 1.0 - double( std::clamp( L.alpha, 0.0f, 1.0f ) );
	const float c = float( 1.0 - keep );
	const unsigned char px[4] = { (unsigned char)std::lround( c * 255.0f ), 0, 0, 0 };
	const QString key = QStringLiteral( "stub|%1|%2|%3" ).arg( wkey ).arg( int( px[0] ) ).arg( s.tier );
	if ( key == s.key && s.map.valid )
		return s.map;
	auto fn = scene->renderer->fn;
	if ( !s.map.tex )
		fn->glGenTextures( 1, &s.map.tex );
	GLint was = 0;
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &was );
	fn->glBindTexture( GL_TEXTURE_2D, s.map.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( was ) );
	WwCloudMap & m = s.map;
	m.valid = true;
	m.stub = true;
	m.size = 1;
	m.centerX = eye[0];
	m.centerY = eye[1];
	m.halfExtent = 48000.0f * kUnitsPerM;
	for ( auto & b : m.band )
		b = WwCloudBandInfo();
	WwCloudBandInfo & lo = m.band[WwCloudLow];
	lo.coverage = c;
	lo.baseZ = eye[2] + 1000.0f * kUnitsPerM;
	lo.topZ = eye[2] + 2500.0f * kUnitsPerM;
	lo.density = c > 0.0f ? 1.0f : 0.0f;
	lo.layers = int( layers.size() );
	m.transition = 1.0f;
	m.generation++;
	s.key = key;
	say( s, QStringLiteral( "stub low=%1 layers=%2" ).arg( double( c ), 0, 'f', 3 ).arg( layers.size() ) );
	return m;
}
