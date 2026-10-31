#include "cloudmap.h"
#include "cloudmath.h"
#include "cloudfly.h"
#include "cloudvol.h"	// lane CLOUD5: Cloud Variety raises the Low top for towers
#include "cloudvolmath.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "gl/weatherrecx.h"	// lane WX1
#include "gl/weatherrecxmath.h"
#include "harnesswindow.h"

#include <QElapsedTimer>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

/* lane CLOUD1: the cloud map (cloudmap.h). Per band, the weather's layers are projected from the dome onto the band's
 * plane (res/shaders/cloudmap.*) and unioned; the band scalars are the census rung (gl/cloudcensus.inc, derived offline
 * by notes/cloud1/census/census.py) unioned per band (gl/cloudmath.h). */

namespace
{

using namespace WwCloudMath;

constexpr const char * kKey = "Settings/Render/Scene/Cloud Map";
constexpr float kUnitsPerM = 69.99f;	// game units per metre (the engine's 1/0.0142875)
constexpr float kHalfExtentM = 48000.0f;	// a 1 km base seen at 1.2 degrees above the horizon is still on the map
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP

struct CensusRow
{
	const char * tex;
	int role;
	float meanA, breakup;
};

const CensusRow kCensus[] = {
#define WW_CLOUD_CENSUS_ROW( tex, role, meanA, breakup ) { tex, role, meanA, breakup },
#include "gl/cloudcensus.inc"
};

const float kFoot[] = {
#define WW_CLOUD_FOOTPRINT( layer, share ) share,
#include "gl/cloudcensus.inc"
};
constexpr int kFootN = int( sizeof( kFoot ) / sizeof( kFoot[0] ) );

//! one layer to draw: a weather layer at its weight (the transition's share)
struct Draw
{
	WwCloudLayerIn in;
	int role = Other;
	float meanA = 0.5f;
	bool censused = false;
	float cover = 0.0f;	// the census rung at full weight
};

struct CmState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	WwCloudMap map;
	QString key;	// what the current map was built from
	QString last = QStringLiteral( "off" );
	// the transition: the previous weather's layers and band parameters, and when the change came
	QString weather;	// plugin|weather of the current layers
	QVector<WwCloudLayerIn> lastLayers;	// the layers of the last build
	QVector<WwCloudLayerIn> fromLayers;	// the previous weather's, frozen at the change
	BandParams prev[3], cur[3];
	bool havePrev = false;
	QElapsedTimer since;
	// GL
	GLuint fbo = 0;
	int texSize = 0;
	GLuint q[4] = { 0, 0, 0, 0 };
	bool pending[2] = { false, false };
	int cur2 = 0;
	double ms = 0.0;
	quint64 builds = 0;
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

//! the reds, read per build: max (largest layer instead of the union), colorblend (the tint blended as a colour)
bool red( const char * word )
{
	return wwEnvStr( "WW_CLOUDMAP_RED" ).split( QLatin1Char( ',' ) ).contains( QLatin1StringView( word ) );
}

void say( CmState & s, const QString & line )
{
	if ( line != s.last )
		fprintf( stderr, "cloudmap: %s\n", qPrintable( line ) );
	s.last = line;
}

void drop( Scene * scene, CmState & s )
{
	if ( scene && scene->renderer ) {
		auto fn = scene->renderer->fn;
		if ( s.map.tex )
			fn->glDeleteTextures( 1, &s.map.tex );
		if ( s.fbo )
			fn->glDeleteFramebuffers( 1, &s.fbo );
	}
	s.fbo = 0;
	s.texSize = 0;
	const quint64 g = s.map.generation;
	s.map = WwCloudMap();
	s.map.generation = g + 1;
	s.key.clear();
}

QString texKey( const QString & path )
{
	QString k = path.toLower();
	k.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	while ( k.startsWith( QLatin1Char( '/' ) ) )
		k.remove( 0, 1 );
	if ( k.startsWith( QLatin1StringView( "data/" ) ) )
		k.remove( 0, 5 );
	if ( k.startsWith( QLatin1StringView( "textures/" ) ) )
		k.remove( 0, 9 );
	return k;
}

const CensusRow * census( const QString & tex )
{
	static QHash<QString, int> idx;
	if ( idx.isEmpty() )
		for ( int i = 0; i < int( sizeof( kCensus ) / sizeof( kCensus[0] ) ); i++ )
			idx.insert( QString::fromLatin1( kCensus[i].tex ), i );
	const auto it = idx.constFind( texKey( tex ) );
	return it == idx.constEnd() ? nullptr : &kCensus[*it];
}

Draw drawOf( const WwCloudLayerIn & L )
{
	Draw d;
	d.in = L;
	const CensusRow * c = census( L.tex );
	if ( c ) {
		d.role = c->role;
		d.meanA = c->meanA;
		d.censused = true;
	} else {
		// a texture the census never saw (a mod's): its role by name, half the sky as its mean alpha
		d.role = roleOfName( texKey( L.tex ).toLatin1().constData() );
	}
	const float foot = ( L.layer >= 0 && L.layer < kFootN ) ? kFoot[L.layer] : 0.0f;
	d.cover = layerCover( L.alpha, d.meanA, foot );
	return d;
}

/* lane WEATHERBLEND3: a layer's draws. During a change ONE (tex cross-faded to tex2 by texBlend, the cover blended),
 * the game's one layer; two at their shares when the two textures' census bands differ */
void addDraws( QVector<Draw> & ds, const WwCloudLayerIn & L )
{
	if ( !( L.texBlend > 0.0f ) || L.tex2.isEmpty() ) {
		ds << drawOf( L );
		return;
	}
	WwCloudLayerIn a = L, b = L;
	a.tex2.clear();
	a.texBlend = 0.0f;
	b.tex = L.tex2;
	b.tex2.clear();
	b.texBlend = 0.0f;
	const Draw da = drawOf( a ), db = drawOf( b );
	const RoleInfo & ra = roleInfo( da.role );
	const RoleInfo & rb = roleInfo( db.role );
	if ( ra.band != rb.band || ra.sheet != rb.sheet ) {
		a.alpha *= 1.0f - L.texBlend;
		b.alpha *= L.texBlend;
		ds << drawOf( a ) << drawOf( b );
		return;
	}
	Draw d = db;
	d.in = L;
	d.meanA = da.meanA + ( db.meanA - da.meanA ) * L.texBlend;
	d.censused = da.censused && db.censused;
	d.cover = da.cover + ( db.cover - da.cover ) * L.texBlend;
	ds << d;
}

void paramsOf( const QVector<Draw> & ds, BandParams out[3], bool redMax,
	const std::vector<std::array<float, 2>> * heights = nullptr, const WwWeatherRecx * wx = nullptr, float eyeZ = 0.0f )
{
	std::vector<int> role;
	std::vector<float> cover;
	std::vector<std::array<float, 3>> tint;
	std::vector<std::array<float, 2>> height;	// lane WX1: clouds.layers.<n>.base/top, above the eye
	bool own = false;
	for ( const Draw & d : ds ) {
		role.push_back( d.role );
		cover.push_back( d.cover );
		std::array<float, 3> t;
		tintOfLayerColor( d.in.rgb, t.data() );
		tint.push_back( t );
		std::array<float, 2> h = { -1.0f, -1.0f };
		if ( wx && wx->layers > 0 && d.in.layer >= 0 && d.in.layer < 32 && wx->layerBase[d.in.layer] >= 0.0f ) {
			WwWeatherRecxMath::layerAboveEye( wx->layerBase[d.in.layer], wx->layerTop[d.in.layer], eyeZ / kUnitsPerM, h[0], h[1] );
			own = true;
		}
		height.push_back( h );
	}
	bandsOf( role.data(), cover.data(), reinterpret_cast<const float( * )[3]>( tint.data() ), int( role.size() ), out,
		redMax, heights ? reinterpret_cast<const float( * )[2]>( heights->data() )
				: own ? reinterpret_cast<const float( * )[2]>( height.data() ) : nullptr );
	if ( !wx || !wx->on )
		return;
	// lane WX1: clouds.coverage (each band keeps its share) and clouds.density
	float c[3] = { out[0].coverage, out[1].coverage, out[2].coverage };
	float d[3] = { out[0].density, out[1].density, out[2].density };
	WwWeatherRecxMath::cloudsFromFile( c, d, wx->coverage, wx->density, wx->recordFirst );
	for ( int b = 0; b < 3; b++ ) {
		out[b].coverage = c[b];
		out[b].density = d[b];
	}
}

bool ensureTarget( NifSkopeOpenGLContext::GLFunctions * fn, CmState & s, int n )
{
	if ( s.map.tex && s.texSize == n )
		return true;
	if ( !s.fbo )
		fn->glGenFramebuffers( 1, &s.fbo );
	if ( s.map.tex )
		fn->glDeleteTextures( 1, &s.map.tex );
	fn->glGenTextures( 1, &s.map.tex );
	fn->glBindTexture( GL_TEXTURE_2D, s.map.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, n, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, s.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s.map.tex, 0 );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	s.texSize = n;
	return fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, CmState & s, int slot, bool wait )
{
	if ( !s.pending[slot] )
		return;
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( s.q[slot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	GLuint64 a = 0, b = 0;
	fn->glGetQueryObjectui64v( s.q[slot * 2], GL_QUERY_RESULT, &a );
	fn->glGetQueryObjectui64v( s.q[slot * 2 + 1], GL_QUERY_RESULT, &b );
	s.ms = b > a ? double( b - a ) * 1e-6 : 0.0;
	s.pending[slot] = false;
	if ( s.builds % 120 == 1 )
		fprintf( stderr, "cloudmap: gpu %.4f ms (%dx%d)\n", s.ms, s.texSize, s.texSize );
}

//! the saved GL state around the map pass (the sky pass runs inside the frame)
struct SavedGl
{
	GLint draw = 0, read = 0, active = 0, tex0 = 0, vp[4] = { 0, 0, 1, 1 };
	GLint srcRgb = GL_ONE, dstRgb = GL_ZERO, srcA = GL_ONE, dstA = GL_ZERO, eqRgb = GL_FUNC_ADD, eqA = GL_FUNC_ADD;
	GLboolean mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE }, depthMask = GL_TRUE;
	GLfloat clear[4] = { 0, 0, 0, 0 };
	bool depth = false, blend = false, cull = false, scissor = false, stencil = false;

	void save( NifSkopeOpenGLContext::GLFunctions * fn )
	{
		fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &draw );
		fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &read );
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex0 );
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		fn->glGetIntegerv( GL_BLEND_SRC_RGB, &srcRgb );
		fn->glGetIntegerv( GL_BLEND_DST_RGB, &dstRgb );
		fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &srcA );
		fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &dstA );
		fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &eqRgb );
		fn->glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &eqA );
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, mask );
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		fn->glGetFloatv( GL_COLOR_CLEAR_VALUE, clear );
		depth = fn->glIsEnabled( GL_DEPTH_TEST );
		blend = fn->glIsEnabled( GL_BLEND );
		cull = fn->glIsEnabled( GL_CULL_FACE );
		scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
		stencil = fn->glIsEnabled( GL_STENCIL_TEST );
	}
	void restore( NifSkopeOpenGLContext::GLFunctions * fn )
	{
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( draw ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( read ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( tex0 ) );
		fn->glActiveTexture( GLenum( active ) );
		fn->glBlendFuncSeparate( GLenum( srcRgb ), GLenum( dstRgb ), GLenum( srcA ), GLenum( dstA ) );
		fn->glBlendEquationSeparate( GLenum( eqRgb ), GLenum( eqA ) );
		fn->glColorMask( mask[0], mask[1], mask[2], mask[3] );
		fn->glDepthMask( depthMask );
		fn->glClearColor( clear[0], clear[1], clear[2], clear[3] );
		auto set = [fn]( GLenum cap, bool on ) {
			if ( on )
				fn->glEnable( cap );
			else
				fn->glDisable( cap );
		};
		set( GL_DEPTH_TEST, depth );
		set( GL_BLEND, blend );
		set( GL_CULL_FACE, cull );
		set( GL_SCISSOR_TEST, scissor );
		set( GL_STENCIL_TEST, stencil );
	}
};

/* the map: each layer (weight w) drawn onto its band's plane into the band's channel, unioned by the blend
 * dst = src (1 - dst) + dst; the red "max" keeps the largest (GL_MAX) */
bool buildMap( Scene * scene, CmState & s, const QVector<Draw> & ds, const QVector<float> & weight, const BandParams bp[3],
	int n, bool redMax, QString * why )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	if ( !ensureTarget( fn, s, n ) ) {
		*why = QStringLiteral( "the map target is incomplete" );
		return false;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, s.fbo );
	fn->glViewport( 0, 0, n, n );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glClearColor( 0, 0, 0, 0 );
	fn->glClear( GL_COLOR_BUFFER_BIT );
	fn->glEnable( GL_BLEND );
	if ( redMax ) {
		fn->glBlendEquation( GL_MAX );
		fn->glBlendFunc( GL_ONE, GL_ONE );
	} else {
		fn->glBlendEquation( GL_FUNC_ADD );
		fn->glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ONE );
	}
	auto prog = r->useProgram( "cloudmap.prog" );
	if ( !prog ) {
		*why = QStringLiteral( "cloudmap.prog did not link" );
		return false;
	}
	int drawn = 0;
	for ( int i = 0; i < ds.size(); i++ ) {
		const Draw & d = ds[i];
		if ( !d.in.pos || !d.in.idx || d.in.nIdx <= 0 || weight[i] <= 0.0f )
			continue;
		const RoleInfo & ri = roleInfo( d.role );
		fn->glActiveTexture( GL_TEXTURE0 );
		if ( scene->bindTexture( QStringView( d.in.tex ), true ) <= 0 )
			continue;
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		float tb = 0.0f;	// lane WEATHERBLEND3: the change's new texture on unit 1 (one draw per layer)
		if ( d.in.texBlend > 0.0f && !d.in.tex2.isEmpty() ) {
			fn->glActiveTexture( GL_TEXTURE1 );
			if ( scene->bindTexture( QStringView( d.in.tex2 ), true ) > 0 ) {
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
				tb = d.in.texBlend;
			}
			fn->glActiveTexture( GL_TEXTURE0 );
		}
		const int b = ri.band;
		// R = Low, G = Mid, A = High; a sheet layer also writes B (the Low band's sheet coverage)
		fn->glColorMask( b == 0, b == 1, b == 0 && ri.sheet, b == 2 );
		prog->uni1i( "Tex", 0 );
		prog->uni1i( "Tex2", 1 );
		prog->uni1f( "texBlend", tb );
		prog->uni1b( "cmShow", false );
		prog->uni1f( "cmHeight", std::max( 1.0f, bp[b].baseM ) * kUnitsPerM );
		prog->uni1f( "cmHalfExtent", kHalfExtentM * kUnitsPerM );
		prog->uni2f( "uvOffset", d.in.uvOffset[0], d.in.uvOffset[1] );
		prog->uni1f( "alpha", std::clamp( d.in.alpha * weight[i], 0.0f, 1.0f ) );
		const float * attrs[8] = { d.in.pos, d.in.col, nullptr, nullptr, nullptr, nullptr, nullptr, d.in.uv };
		r->drawShape( unsigned( d.in.nv ), 0x20000043ULL, unsigned( d.in.nIdx ), GL_TRIANGLES, GL_UNSIGNED_SHORT, attrs,
			d.in.idx );
		drawn++;
	}
	r->stopProgram();
	if ( !drawn && !ds.isEmpty() ) {
		*why = QStringLiteral( "no layer drew (no mesh or texture resolved)" );
		return false;
	}
	return true;
}

//! WW_CLOUDMAP_ECHO=1: the map's channel means, read back once per build (rows check the shape against the scalars)
void echo( NifSkopeOpenGLContext::GLFunctions * fn, CmState & s )
{
	if ( wwEnvStr( "WW_CLOUDMAP_ECHO" ) != QLatin1StringView( "1" ) || !s.fbo )
		return;
	const int n = s.texSize;
	std::vector<unsigned char> px( size_t( n ) * size_t( n ) * 4 );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, s.fbo );
	fn->glReadPixels( 0, 0, n, n, GL_RGBA, GL_UNSIGNED_BYTE, px.data() );
	double m[4] = { 0, 0, 0, 0 };
	for ( size_t i = 0; i < px.size(); i++ )
		m[i & 3] += px[i];
	const double k = 1.0 / ( 255.0 * double( n ) * double( n ) );
	fprintf( stderr, "cloudmap: echo mean low=%.4f mid=%.4f sheet=%.4f high=%.4f\n", m[0] * k, m[1] * k, m[2] * k, m[3] * k );
}

//! WW_CLOUDMAP_SHOW=1: the map in the screen's top-right corner (R Low, G Mid, B High), every frame
void show( Scene * scene, CmState & s )
{
	if ( wwEnvStr( "WW_CLOUDMAP_SHOW" ) != QLatin1StringView( "1" ) || !s.map.valid || !s.map.tex )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	SavedGl g;
	g.save( fn );
	const int side = std::min( 256, std::max( 1, std::min( g.vp[2], g.vp[3] ) / 3 ) );
	fn->glViewport( g.vp[0] + g.vp[2] - side, g.vp[1] + g.vp[3] - side, side, side );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	auto prog = r->useProgram( "cloudmap.prog" );
	if ( prog ) {
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, s.map.tex );
		prog->uni1i( "Tex", 0 );
		prog->uni1b( "cmShow", true );
		prog->uni1f( "texBlend", 0.0f );	// lane WEATHERBLEND3: the map alone
		prog->uni1f( "alpha", 1.0f );
		static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		const float * attrs = q;
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
	}
	g.restore( fn );
}

const char * tierName( int t )
{
	static const char * k[5] = { "off", "low", "medium", "high", "ultra" };
	return k[std::clamp( t, 0, 4 )];
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
	if ( !wwLookdevCloudLayers( scene, layers, wkey, eye, &why ) ) {
		if ( s.map.valid || s.map.tex )
			drop( scene, s );
		s.weather.clear();
		s.havePrev = false;
		say( s, QStringLiteral( "none(%1)" ).arg( why ) );
		return s.map;
	}
	const bool redMax = red( "max" ), redColor = red( "colorblend" );
	QVector<Draw> cur;
	for ( const WwCloudLayerIn & L : layers )
		addDraws( cur, L );
	BandParams now[3];
	const WwWeatherRecx & wx = wwWeatherRecx();	// lane WX1: the weather .recx clouds block (Weather .recx row)
	/* lane CLOUD4: with Fly-through Clouds on, the bands sit at metres above sea level (the layer types' heights, the
	 * weather's .recx clouds.layers.<n>.base/top, the pin WW_CLOUDFLY_BAND for the Low band), not above the eye */
	const bool world = wwCloudFlyWorldHeights();
	if ( world ) {
		float lh[32][2];
		wwCloudFlyLayerHeights( wkey, lh );
		std::vector<std::array<float, 2>> h;
		for ( const Draw & d : cur )
			h.push_back( d.in.layer >= 0 && d.in.layer < 32 ? std::array<float, 2>{ lh[d.in.layer][0], lh[d.in.layer][1] }
															 : std::array<float, 2>{ 0.0f, 0.0f } );
		paramsOf( cur, now, redMax, &h, wx.on ? &wx : nullptr, eye[2] );
		float pb = 0.0f, pt = 0.0f;
		if ( wwCloudFlyBandPin( pb, pt ) ) {
			now[0].baseM = pb;
			now[0].topM = pt;
		}
	} else {
		paramsOf( cur, now, redMax, nullptr, wx.on ? &wx : nullptr, eye[2] );
	}
	{	// lane CLOUD5: the Low band's cloud type from the record's class (DATA flags, thunder), a .recx clouds.type wins
		// lane CLOUD6d: one profile set per weather (spread 0) unless WW_CLOUDVOL_PROFILESET=0 / RED=typemix
		// lane CLOUDTYPE2: the storm signal = the thunder sounds (WW_CLOUDVOL_TYPERULE), -1 = CLOUD5's byte rule (the red)
		unsigned flags = 0, thunder = 0;
		int sounds = 0;
		wwLookdevWeatherClass( flags, thunder, &sounds );
		lowTypeOf( flags, thunder, wx.on ? wx.type : -1.0f, now[0].type, now[0].typeSpread, wwCloudVolProfileSet(),
			wwCloudVolTypeRule() ? sounds : -1 );
	}

	// the transition: a new weather starts from the parameters shown at the change
	if ( wkey != s.weather ) {
		if ( !s.weather.isEmpty() ) {
			for ( int b = 0; b < 3; b++ )
				s.prev[b] = s.cur[b];
			s.fromLayers = s.map.valid ? s.lastLayers : QVector<WwCloudLayerIn>();
			s.havePrev = true;
		}
		s.weather = wkey;
		s.since.start();
	}
	// WW_CLOUDMAP_TRANSITION=<0..1> freezes the blend (rows, fixtures); WW_CLOUDMAP_FADE=<s> its length, else a snap
	// (the 2D layers change at once in the viewer)
	float t = 1.0f;
	const QString tp = wwEnvStr( "WW_CLOUDMAP_TRANSITION" ).trimmed();
	const float fade = wwEnvStr( "WW_CLOUDMAP_FADE" ).toFloat();
	if ( s.havePrev && !tp.isEmpty() )
		t = std::clamp( tp.toFloat(), 0.0f, 1.0f );
	else if ( s.havePrev && fade > 0.0f && s.since.isValid() )
		t = std::clamp( float( s.since.elapsed() ) / ( 1000.0f * fade ), 0.0f, 1.0f );
	BandParams shown[3];
	for ( int b = 0; b < 3; b++ ) {
		shown[b] = s.havePrev ? lerpParams( s.prev[b], now[b], t ) : now[b];
		if ( s.havePrev && redColor )
			lerpTintColorRed( s.prev[b].tint, now[b].tint, t, shown[b].tint );
	}

	// the layers drawn: this weather's at t, the previous weather's at 1 - t (their JNAM alpha is a parameter too)
	QVector<Draw> ds = cur;
	QVector<float> w( cur.size(), t );
	if ( s.havePrev && t < 1.0f )
		for ( const WwCloudLayerIn & P : s.fromLayers ) {
			WwCloudLayerIn L = P;
			if ( !wwLookdevCloudMesh( scene, L ) )
				continue;
			addDraws( ds, L );
			while ( w.size() < ds.size() )
				w << 1.0f - t;
		}
	if ( !s.havePrev || t >= 1.0f )
		w.fill( 1.0f );

	const int n = wwCloudMapSizeFor( s.tier );
	QString key = QStringLiteral( "%1|%2|%3|%4%5|%6" ).arg( wkey ).arg( n ).arg( double( t ), 0, 'f', 4 )
		.arg( redMax ? QStringLiteral( "max" ) : QString() ).arg( redColor ? QStringLiteral( "color" ) : QString() )
		.arg( ds.size() );
	if ( wx.on )
		key += QStringLiteral( "|wx:" ) + wx.key;	// lane WX1: a file's value moves the map
	for ( const Draw & d : ds )
		key += QStringLiteral( "|%1:%2,%3,%4" ).arg( d.in.layer ).arg( double( d.in.alpha ), 0, 'f', 5 )
			.arg( double( d.in.uvOffset[0] ), 0, 'f', 5 ).arg( double( d.in.uvOffset[1] ), 0, 'f', 5 )
			+ ( d.in.texBlend > 0.0f ? QStringLiteral( "~%1" ).arg( double( d.in.texBlend ), 0, 'f', 4 ) : QString() );

	if ( world )	// lane CLOUD4: the projection follows the heights
		for ( int b = 0; b < 3; b++ )
			key += QStringLiteral( "|w%1,%2" ).arg( double( shown[b].baseM ), 0, 'f', 1 ).arg( double( shown[b].topM ), 0, 'f', 1 );
	const bool variety = wwCloudVolVarietyOn();	// lane CLOUD5
	const bool shapes = wwCloudVolShapeOn();	// lane CLOUDSHAPE1: Cloud Shapes raises the Low top as Variety does
	if ( variety )
		key += QStringLiteral( "|var%1,%2" ).arg( double( shown[0].type ), 0, 'f', 3 ).arg( double( shown[0].typeSpread ), 0, 'f', 3 );
	if ( shapes )
		key += QStringLiteral( "|shp%1,%2" ).arg( double( shown[0].type ), 0, 'f', 3 ).arg( double( shown[0].typeSpread ), 0, 'f', 3 );

	WwCloudMap & m = s.map;
	m.centerX = eye[0];	// the sky follows the camera: only the square moves
	m.centerY = eye[1];
	for ( int b = 0; b < 3; b++ ) {
		m.band[b].baseZ = ( world ? 0.0f : eye[2] ) + shown[b].baseM * kUnitsPerM;
		m.band[b].topZ = ( world ? 0.0f : eye[2] ) + shown[b].topM * kUnitsPerM;
		m.band[b].depthZ = ( shown[b].topM - shown[b].baseM ) * kUnitsPerM;
	}
	/* lane CLOUD5: Cloud Variety on: the Low band reaches the tallest type its sky can hold (a cumulonimbus' tower over a
	 * storm); every reader (the march's span, the shadow map's steps, the fly box) takes this one top. shown[] keeps the
	 * band's own top, so a later transition never raises it twice. */
	m.variety = variety;
	if ( variety || shapes ) {
		const float tmax = std::min( 1.0f, shown[0].type + shown[0].typeSpread );
		const float depthKm = std::max( shown[0].topM - shown[0].baseM, 1.0f ) / 1000.0f;
		m.band[0].topZ = m.band[0].baseZ
			+ 1000.0f * WwCloudVolMath::typeDepthKm( tmax, depthKm, WwCloudVolMath::kVarTowerKm ) * kUnitsPerM;
	}
	if ( key != s.key || !m.valid ) {
		Renderer * r = scene->renderer;
		auto fn = r->fn;
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
		if ( !s.q[0] )
			fn->glGenQueries( 4, s.q );
		s.builds++;
		timeRead( fn, s, 1 - s.cur2, false );
		timeRead( fn, s, s.cur2, true );
		fn->glQueryCounter( s.q[s.cur2 * 2], kTimestamp );
		SavedGl g;
		g.save( fn );
		const bool ok = buildMap( scene, s, ds, w, shown, n, redMax, &why );
		if ( ok )
			echo( fn, s );
		g.restore( fn );
		fn->glQueryCounter( s.q[s.cur2 * 2 + 1], kTimestamp );
		s.pending[s.cur2] = true;
		s.cur2 = 1 - s.cur2;
		if ( !ok ) {
			drop( scene, s );
			say( s, QStringLiteral( "refused(%1)" ).arg( why ) );
			return s.map;
		}
		m.valid = true;
		m.stub = false;
		m.size = n;
		m.halfExtent = kHalfExtentM * kUnitsPerM;
		int counts[3] = { 0, 0, 0 };
		for ( const Draw & d : cur )
			if ( d.cover > 0.0f )
				counts[roleInfo( d.role ).band]++;
		for ( int b = 0; b < 3; b++ ) {
			WwCloudBandInfo & bi = m.band[b];
			bi.coverage = shown[b].coverage;
			bi.density = shown[b].density;
			bi.sheet = shown[b].sheet;
			for ( int c = 0; c < 3; c++ )
				bi.tint[c] = shown[b].tint[c];
			bi.layers = counts[b];
			bi.type = shown[b].type;	// lane CLOUD5
			bi.typeSpread = shown[b].typeSpread;
		}
		m.transition = t;
		m.generation++;
		s.key = key;
		for ( int b = 0; b < 3; b++ )
			s.cur[b] = shown[b];
		s.lastLayers = layers;
		int unc = 0;
		for ( const Draw & d : cur )
			unc += d.censused ? 0 : 1;
		say( s, QStringLiteral( "%1 %2x%2 low=%3 mid=%4 high=%5 sheet=%6 layers=%7%8 t=%9%10" ).arg( QLatin1StringView( tierName( s.tier ) ) )
			.arg( n ).arg( double( m.band[0].coverage ), 0, 'f', 3 ).arg( double( m.band[1].coverage ), 0, 'f', 3 )
			.arg( double( m.band[2].coverage ), 0, 'f', 3 ).arg( double( m.band[0].sheet ), 0, 'f', 3 ).arg( cur.size() )
			.arg( unc ? QStringLiteral( " uncensused=%1" ).arg( unc ) : QString() ).arg( double( t ), 0, 'f', 2 )
			.arg( redMax || redColor ? QStringLiteral( " red=%1%2" ).arg( redMax ? "max" : "" ).arg( redColor ? "colorblend" : "" ) : QString() )
			+ ( variety ? QStringLiteral( " type=%1+-%2 lowtop=%3m" ).arg( double( shown[0].type ), 0, 'f', 3 )
							  .arg( double( shown[0].typeSpread ), 0, 'f', 3 )
							  .arg( double( ( m.band[0].topZ - m.band[0].baseZ ) / kUnitsPerM ), 0, 'f', 0 )
						: QString() ) );
	}
	show( scene, s );
	return m;
}
