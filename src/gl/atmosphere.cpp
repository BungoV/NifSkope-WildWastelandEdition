#include "atmosphere.h"
#include "atmomath.h"
#include "wwswitch.h"
#include "gl/cellunits.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

// lane ATMO1: the physical sky (atmosphere.h). The constants are Hillaire 2020's Earth (tests/spells/atmo_model.py).

namespace
{

using namespace WwAtmoMath;	// lane SKYA: the constants and the CPU math, shared with the offline unit test

constexpr const char * kKey = "Settings/Render/Scene/Physical Sky";
constexpr const char * kApKey = "Settings/Render/Scene/Aerial Perspective";
constexpr const char * kSunKey = "Settings/Render/Scene/Sunlight From Sky";	// lane SKYA (ATMOSUN1)
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
constexpr int kTransW = 256, kTransH = 64;
/* lane SKYA (ATMO2): the aerial-perspective volume's reach. A Fallout 4 worldspace is ~4 km across (64 cells of
 * 58.5 m) and its far LOD a few km more: 16 km covers it with slice 0's centre at 4 m (quadratic slices) */
constexpr float kApMaxKm = 16.0f;
// the disc's display cap (scene units): bloom (post) still sees a bright disc, never an unbounded one
constexpr float kDiscCap = 48.0f;

struct AtState
{
	bool init = false;
	quint64 liveGen = 0;
	int tier = 0;
	bool pinned = false;
	float haze[3] = { 0.0f, 1.0f, 0.0f };	// last drawn: ground extinction /km, scale height km, plateau top km
	QString last = QStringLiteral( "off" );
	int ap = 0;	// lane SKYA (ATMO2): the Aerial Perspective row, 0 Off / 1 On (pin WW_ATMO_AP)
	bool apPinned = false;
	int sun = 0;	// lane SKYA (ATMOSUN1): the Sunlight From Sky row, 0 Off / 1 Tint / 2 Full (pin WW_ATMO_SUN)
	bool sunPinned = false;
};

//! lane SKYA: one row's value from its pin (already read by the caller: the survey needs literal names), a harness
//! run (0, pinned) or its QSettings key
int rowValue( const QString & pin, const char * key, const QStringList & names, bool & pinned )
{
	const QString e = pin.trimmed().toLower();
	pinned = true;
	if ( !e.isEmpty() ) {
		bool ok = false;
		const int v = e.toInt( &ok );
		if ( ok )
			return std::clamp( v, 0, int( names.size() ) - 1 );
		const int i = int( names.indexOf( e ) );
		return i >= 0 ? i : 0;
	}
	if ( wwHarnessRun() )
		return 0;
	pinned = false;
	return std::clamp( QSettings().value( QLatin1StringView( key ), 0 ).toInt(), 0, int( names.size() ) - 1 );
}

AtState & as()
{
	static AtState s;
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_ATMO" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		s.pinned = false;
		const QString e = wwEnvStr( "WW_ATMO" ).trimmed().toLower();
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
		// lane SKYA (ATMO2): the Aerial Perspective row (the prefix above re-reads it live)
		const QString a = wwEnvStr( "WW_ATMO_AP" ).trimmed().toLower();
		s.apPinned = true;
		if ( !a.isEmpty() )
			s.ap = ( a == QLatin1StringView( "1" ) || a == QLatin1StringView( "on" ) ) ? 1 : 0;
		else if ( wwHarnessRun() )
			s.ap = 0;
		else {
			s.apPinned = false;
			s.ap = QSettings().value( QLatin1StringView( kApKey ), 0 ).toInt() ? 1 : 0;
		}
		// lane SKYA: the rows after ATMO2 (the same prefix re-reads them live)
		s.sun = rowValue( wwEnvStr( "WW_ATMO_SUN" ), kSunKey, { "off", "tint", "full" }, s.sunPinned );
	}
	return s;
}

bool red( const char * word )
{
	return wwEnvStr( "WW_ATMO_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

//! lane SKYA (ATMO2): the aerial perspective's reds: nosunt (the in-scatter ignores the sun's transmittance), double
//! (the cell fog laid over the full aerial perspective: the haze counted twice)
bool apRed( const char * word )
{
	return wwEnvStr( "WW_ATMO_AP_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

struct Target
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0;
};

struct Volume	// lane SKYA (ATMO2): the aerial-perspective froxels, RGBA16F 3D, one FBO a slice at a time
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0, n = 0;
	bool valid = false;	// drawn this frame (wwAtmoApFrame clears it)
	float view[9] = {};	// the view rotation and projection it was built for: other views (probes) read none
	float proj[4] = {};	// P00, P11, P20, P21
	float kmPerView = 0.0f;
	float camR = 0.0f;	// km from the planet's centre (the sky's air reads the transmittance LUT from here)
};

struct AtGpu
{
	Target trans, ms, sunView, moonView;
	Volume ap;
	QString transKey, msKey;
	GLuint q[4] = { 0, 0, 0, 0 };	// two timestamp pairs, read a frame late
	bool pending[2] = { false, false };
	int cur = 0;
	double ms_ = -1.0;
	int frames = 0;
};

QHash<Renderer *, AtGpu> & gpus()
{
	static QHash<Renderer *, AtGpu> g;
	return g;
}

AtGpu * g_last = nullptr;

bool ensure( NifSkopeOpenGLContext::GLFunctions * fn, Target & t, int w, int h )
{
	if ( t.tex && t.w == w && t.h == h )
		return true;
	if ( !t.fbo )
		fn->glGenFramebuffers( 1, &t.fbo );
	if ( t.tex )
		fn->glDeleteTextures( 1, &t.tex );
	fn->glGenTextures( 1, &t.tex );
	fn->glBindTexture( GL_TEXTURE_2D, t.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	fn->glDrawBuffers( 1, &one );
	t.w = w;
	t.h = h;
	return fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
}

void quad( Renderer * r )
{
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

void medium( NifSkopeOpenGLContext::Program * p, const float haze[3], bool noOzone )
{
	p->uni3f( "atRay", kRay[0], kRay[1], kRay[2] );
	p->uni1f( "atMieS", kMieS );
	p->uni1f( "atMieE", kMieE );
	p->uni1f( "atG", kMieG );
	if ( noOzone )
		p->uni3f( "atOzone", 0.0f, 0.0f, 0.0f );
	else
		p->uni3f( "atOzone", kOzone[0], kOzone[1], kOzone[2] );
	p->uni3f( "atHaze", haze[0], haze[1], haze[2] );
}

void bindTex( NifSkopeOpenGLContext::GLFunctions * fn, NifSkopeOpenGLContext::Program * p, int unit, const char * name, GLuint tex )
{
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D, tex );
	p->uni1i( name, unit );
}

// the haze rung, the CPU transmittance and lum(): src/gl/atmomath.h (moved there by lane SKYA, unchanged)

void norm( float v[3] )
{
	const float l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	for ( int c = 0; c < 3; c++ )
		v[c] = l > 1e-9f ? v[c] / l : ( c == 2 ? 1.0f : 0.0f );
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, AtGpu & g, int slot, bool wait )
{
	if ( !g.pending[slot] )
		return;
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( g.q[slot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	GLuint64 a = 0, b = 0;
	fn->glGetQueryObjectui64v( g.q[slot * 2], GL_QUERY_RESULT, &a );
	fn->glGetQueryObjectui64v( g.q[slot * 2 + 1], GL_QUERY_RESULT, &b );
	g.ms_ = b > a ? double( b - a ) * 1e-6 : 0.0;
	g.pending[slot] = false;
	if ( ++g.frames % 120 == 1 )
		fprintf( stderr, "atmo: gpu %.4f ms\n", g.ms_ );
}

const char * tierName( int t )
{
	static const char * n[5] = { "Off", "Low", "Medium", "High", "Ultra" };
	return n[std::clamp( t, 0, 4 )];
}

/* lane SKYA (ATMO2): the aerial-perspective volume (atmo_ap.frag) over the current view's frustum, every frame (the
 * camera and the sun move). Froxels per tier: Low 16x16x16, Medium 32x32x16, High 32x32x32, Ultra 64x64x32. The
 * caller has saved the framebuffer, the viewport and the state; the transmittance and MS LUTs are this frame's. */
bool buildAp( Scene * scene, AtGpu & g, int msN, float camR, const float sunW[3], const float moonW[3], float sunK,
	float moonK, bool noOzone, bool noMs )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const AtState & s = as();
	const auto & gu = *r->globalUniforms;
	if ( gu.projectionMatrix[3][3] == 1.0f )
		return false;	// orthographic: no frustum to slice
	const int w = s.tier >= 4 ? 64 : s.tier >= 2 ? 32 : 16;
	const int n = s.tier >= 3 ? 32 : 16;
	Volume & v = g.ap;
	if ( !v.tex || v.w != w || v.h != w || v.n != n ) {
		if ( !v.fbo )
			fn->glGenFramebuffers( 1, &v.fbo );
		if ( v.tex )
			fn->glDeleteTextures( 1, &v.tex );
		fn->glGenTextures( 1, &v.tex );
		fn->glBindTexture( GL_TEXTURE_3D, v.tex );
		fn->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA16F, w, w, n, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
		fn->glBindTexture( GL_TEXTURE_3D, 0 );
		v.w = v.h = w;
		v.n = n;
	}
	auto p = r->useProgram( "atmo_ap.prog" );
	if ( !p )
		return false;
	// the frustum's world rays (atmo_sky.frag's: view ray ((x + P20) / P00, (y + P21) / P11, -1), world = vr x viewMatrix)
	const float P00 = gu.projectionMatrix[0][0], P11 = gu.projectionMatrix[1][1];
	const float P20 = gu.projectionMatrix[2][0], P21 = gu.projectionMatrix[2][1];
	auto ray = [&]( float x, float y, float o[3] ) {
		const float vr[3] = { ( x + P20 ) / P00, ( y + P21 ) / P11, -1.0f };
		for ( int c = 0; c < 3; c++ )
			o[c] = vr[0] * gu.viewMatrix[c][0] + vr[1] * gu.viewMatrix[c][1] + vr[2] * gu.viewMatrix[c][2];
	};
	float f0[3], fx[3], fy[3];
	ray( -1.0f, -1.0f, f0 );
	ray( 1.0f, -1.0f, fx );
	ray( -1.0f, 1.0f, fy );
	for ( int c = 0; c < 3; c++ ) {
		fx[c] -= f0[c];
		fy[c] -= f0[c];
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, v.fbo );
	glViewport( 0, 0, w, w );
	medium( p, s.haze, noOzone );
	p->uni2f( "atMsSize", float( msN ), float( msN ) );
	p->uni3f( "atApDims", float( w ), float( w ), float( n ) );
	p->uni1f( "atApMaxKm", kApMaxKm );
	p->uni3f( "atFrustum0", f0[0], f0[1], f0[2] );
	p->uni3f( "atFrustumX", fx[0], fx[1], fx[2] );
	p->uni3f( "atFrustumY", fy[0], fy[1], fy[2] );
	p->uni1f( "atCamR", camR );
	p->uni3f( "atSunW", sunW[0], sunW[1], sunW[2] );
	p->uni3f( "atMoonW", moonW[0], moonW[1], moonW[2] );
	p->uni1f( "atSunK", sunK );
	p->uni1f( "atMoonK", moonK );
	p->uni1f( "atMsOn", noMs ? 0.0f : 1.0f );
	p->uni1f( "atApNoSunT", apRed( "nosunt" ) ? 1.0f : 0.0f );
	bindTex( fn, p, 0, "atTransTex", g.trans.tex );
	bindTex( fn, p, 1, "atMsTex", g.ms.tex );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	bool ok = true;
	for ( int k = 0; k < n && ok; k++ ) {
		fn->glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, v.tex, 0, k );
		if ( k == 0 ) {
			fn->glDrawBuffers( 1, &one );
			ok = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
			if ( !ok )
				break;
		}
		p->uni1f( "atApSlice", float( k ) );
		quad( r );
	}
	r->stopProgram();
	if ( !ok )
		return false;
	for ( int c = 0; c < 3; c++ )
		for ( int k = 0; k < 3; k++ )
			v.view[c * 3 + k] = gu.viewMatrix[c][k];
	v.proj[0] = P00;
	v.proj[1] = P11;
	v.proj[2] = P20;
	v.proj[3] = P21;
	const float sc = scene->view.scale != 0.0f ? scene->view.scale : 1.0f;
	v.kmPerView = kUnitKm / sc;
	v.camR = camR;
	v.valid = true;
	return true;
}

}	// namespace

int wwAtmoTier()
{
	return as().tier;
}

void wwAtmoSetTier( int tier )
{
	AtState & s = as();
	if ( s.pinned )
		return;
	s.tier = std::clamp( tier, 0, 4 );
	QSettings().setValue( QLatin1StringView( kKey ), s.tier );
}

bool wwAtmoPinned()
{
	return as().pinned;
}

bool wwAtmoDrawSky( Scene * scene, const WwAtmoInputs & in, QString * line )
{
	AtState & s = as();
	if ( s.tier <= 0 || !scene || !scene->renderer )
		return false;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	AtGpu & g = gpus()[r];
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	if ( !g.q[0] )
		fn->glGenQueries( 4, g.q );
	timeRead( fn, g, 1 - g.cur, false );
	timeRead( fn, g, g.cur, true );
	fn->glQueryCounter( g.q[g.cur * 2], kTimestamp );

	const int vw = s.tier == 1 ? 96 : s.tier == 4 ? 256 : 192;
	const int vh = s.tier == 1 ? 54 : s.tier == 4 ? 144 : 108;
	const int msN = s.tier == 1 ? 16 : 32;
	const bool noOzone = red( "noozone" ), noMs = red( "noms" );
	hazeFromFog( in.fogFar, in.fogMax, in.fogMid, in.fogRange, red( "nohaze" ), s.haze );
	const float hKm = std::clamp( in.camZ * kUnitKm, 0.002f, 80.0f );
	const float camR = kRb + hKm;
	float sunW[3] = { in.sunW[0], in.sunW[1], in.sunW[2] };
	float moonW[3] = { in.moonW[0], in.moonW[1], in.moonW[2] };
	norm( sunW );
	norm( moonW );
	// scene units: displayed = pi k L, k = the Day sunlight luminance / the 45-degree sun's transmittance
	float t45[3];
	cpuTrans( 0.0f, 0.70710678f, s.haze, noOzone, t45 );
	const float tl = std::max( 1e-4f, lum( t45 ) );
	const float sunK = kPi * in.sunLum / tl;
	const float moonK = in.moonUp ? kPi * in.moonLum / tl : 0.0f;

	GLint prevDraw = 0, prevRead = 0, prevActive = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDepthMask( GL_FALSE );

	QString why;
	bool ok = ensure( fn, g.trans, kTransW, kTransH ) && ensure( fn, g.ms, msN, msN ) && ensure( fn, g.sunView, vw, vh )
		&& ensure( fn, g.moonView, vw, vh );
	if ( !ok )
		why = QStringLiteral( "a LUT target is incomplete" );
	const QString hazeKey = QString( "%1,%2,%3,%4" ).arg( double( s.haze[0] ), 0, 'g', 6 ).arg( double( s.haze[1] ), 0, 'g', 6 )
		.arg( double( s.haze[2] ), 0, 'g', 6 ).arg( noOzone );
	// 1. transmittance and multiple scattering: only when the medium moves (weather, hour through the fog keys)
	if ( ok && g.transKey != hazeKey ) {
		auto p = r->useProgram( "atmo_transmittance.prog" );
		ok = p != nullptr;
		if ( p ) {
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.trans.fbo );
			glViewport( 0, 0, kTransW, kTransH );
			medium( p, s.haze, noOzone );
			quad( r );
			r->stopProgram();
			g.transKey = hazeKey;
			g.msKey.clear();
		} else {
			why = QStringLiteral( "atmo_transmittance.prog did not link" );
		}
	}
	const QString msKey = hazeKey + QString( "|%1" ).arg( msN );
	if ( ok && g.msKey != msKey ) {
		auto p = r->useProgram( "atmo_ms.prog" );
		ok = p != nullptr;
		if ( p ) {
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.ms.fbo );
			glViewport( 0, 0, msN, msN );
			medium( p, s.haze, noOzone );
			p->uni2f( "atMsSize", float( msN ), float( msN ) );
			bindTex( fn, p, 0, "atTransTex", g.trans.tex );
			quad( r );
			r->stopProgram();
			g.msKey = msKey;
		} else {
			why = QStringLiteral( "atmo_ms.prog did not link" );
		}
	}
	// 2. the sky-view LUTs, every frame (the sun moves): the sun's, and the moon's while it is up
	auto view = [&]( Target & t, const float l[3] ) {
		auto p = r->useProgram( "atmo_skyview.prog" );
		if ( !p )
			return false;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
		glViewport( 0, 0, vw, vh );
		medium( p, s.haze, noOzone );
		p->uni2f( "atMsSize", float( msN ), float( msN ) );
		p->uni2f( "atViewSize", float( vw ), float( vh ) );
		p->uni1f( "atCamR", camR );
		p->uni1f( "atLightMu", l[2] );
		p->uni1f( "atMsOn", noMs ? 0.0f : 1.0f );
		bindTex( fn, p, 0, "atTransTex", g.trans.tex );
		bindTex( fn, p, 1, "atMsTex", g.ms.tex );
		quad( r );
		r->stopProgram();
		return true;
	};
	if ( ok && !view( g.sunView, sunW ) ) {
		ok = false;
		why = QStringLiteral( "atmo_skyview.prog did not link" );
	}
	if ( ok && moonK > 0.0f )
		view( g.moonView, moonW );
	// 2b. lane SKYA (ATMO2): the aerial-perspective volume over this view's frustum (Hillaire 2020 sec. 5.4)
	QString apLine;	// empty while the row is off: the sky line stays ATMO1's
	g.ap.valid = false;
	if ( ok && s.ap > 0 ) {
		const bool built = buildAp( scene, g, msN, camR, sunW, moonW, sunK, moonK, noOzone, noMs );
		apLine = built ? QString( "ap=%1x%2x%3@%4km" ).arg( g.ap.w ).arg( g.ap.h ).arg( g.ap.n ).arg( double( kApMaxKm ), 0, 'f', 0 )
			: QStringLiteral( "ap=refused(atmo_ap.prog or its target)" );
	}

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	glViewport( vp[0], vp[1], vp[2], vp[3] );
	// 3. the sky's pixels
	if ( ok ) {
		auto p = r->useProgram( "atmo_sky.prog" );
		if ( p ) {
			medium( p, s.haze, noOzone );
			bindTex( fn, p, 0, "atTransTex", g.trans.tex );
			bindTex( fn, p, 1, "atSunView", g.sunView.tex );
			bindTex( fn, p, 2, "atMoonView", g.moonView.tex );
			p->uni2f( "atViewSize", float( vw ), float( vh ) );
			p->uni4f( "atRect", FloatVector4( float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ),
				1.0f / float( std::max( vp[3], 1 ) ) ) );
			p->uni1f( "atCamR", camR );
			p->uni3f( "atSunW", sunW[0], sunW[1], sunW[2] );
			p->uni3f( "atMoonW", moonW[0], moonW[1], moonW[2] );
			p->uni1f( "atSunK", sunK );
			p->uni1f( "atMoonK", moonK );
			p->uni1f( "atLimb", s.tier >= 3 ? 1.0f : 0.0f );
			p->uni1f( "atDiscCap", in.disc ? kDiscCap : 0.0f );
			p->uni1f( "sceneExposure", in.exposure );
			p->uni1i( "viewTransform", in.viewTransform );
			quad( r );
			r->stopProgram();
		} else {
			ok = false;
			why = QStringLiteral( "atmo_sky.prog did not link" );
		}
	}
	for ( int u = 2; u >= 0; u-- ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasDepth )
		fn->glEnable( GL_DEPTH_TEST );
	if ( wasBlend )
		fn->glEnable( GL_BLEND );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	if ( wasScissor )
		fn->glEnable( GL_SCISSOR_TEST );
	if ( wasStencil )
		fn->glEnable( GL_STENCIL_TEST );
	fn->glDepthMask( depthMask );
	fn->glQueryCounter( g.q[g.cur * 2 + 1], kTimestamp );
	g.pending[g.cur] = true;
	g.cur = 1 - g.cur;
	g_last = ok ? &g : nullptr;

	const float sunEl = std::asin( std::clamp( sunW[2], -1.0f, 1.0f ) ) * 180.0f / kPi;
	QString reds;
	for ( const char * w : { "noozone", "noms", "nohaze" } )
		if ( red( w ) )
			reds = QStringLiteral( ",RED=%1" ).arg( QLatin1StringView( w ) );
	for ( const char * w : { "nosunt", "double" } )
		if ( apRed( w ) )
			reds += QStringLiteral( ",AP_RED=%1" ).arg( QLatin1StringView( w ) );
	s.last = ok ? QString( "physical(tier=%1,view=%2x%3,ms=%4,haze=%5/km@%6km,sun_elev=%7,k=%8,moon=%9%10)" )
			.arg( QLatin1StringView( tierName( s.tier ) ) ).arg( vw ).arg( vh ).arg( msN )
			.arg( double( s.haze[0] ), 0, 'f', 3 ).arg( double( s.haze[1] ), 0, 'f', 3 ).arg( double( sunEl ), 0, 'f', 1 )
			.arg( double( sunK / kPi ), 0, 'f', 3 ).arg( moonK > 0.0f ? QStringLiteral( "up" ) : QStringLiteral( "down" ) ).arg( reds )
			+ ( apLine.isEmpty() ? QString() : QStringLiteral( "," ) + apLine )
		: QString( "physical:refused(%1)" ).arg( why );
	if ( line )
		*line = s.last;
	return ok;
}

unsigned wwAtmoTransmittanceTex()
{
	return g_last ? g_last->trans.tex : 0;
}

unsigned wwAtmoMultiScatterTex()
{
	return g_last ? g_last->ms.tex : 0;
}

unsigned wwAtmoSkyViewTex()
{
	return g_last ? g_last->sunView.tex : 0;
}

void wwAtmoTransmittance( float heightKm, float cosZenith, float out[3] )
{
	cpuTrans( heightKm, cosZenith, as().haze, red( "noozone" ), out );
}

int wwAtmoAp()
{
	return as().ap;
}

void wwAtmoSetAp( int on )
{
	AtState & s = as();
	if ( s.apPinned )
		return;
	s.ap = on ? 1 : 0;
	QSettings().setValue( QLatin1StringView( kApKey ), s.ap );
}

bool wwAtmoApPinned()
{
	return as().apPinned;
}

int wwAtmoSun()
{
	return as().sun;
}

void wwAtmoSetSun( int mode )
{
	AtState & s = as();
	if ( s.sunPinned )
		return;
	s.sun = std::clamp( mode, 0, 2 );
	QSettings().setValue( QLatin1StringView( kSunKey ), s.sun );
}

bool wwAtmoSunPinned()
{
	return as().sunPinned;
}

int wwAtmoSunKey()
{
	const AtState & s = as();
	return s.tier > 0 ? s.sun + 3 * ( wwEnvStr( "WW_ATMO_SUN_RED" ).trimmed().toLower() == QLatin1StringView( "flat" ) )
		: 0;
}

bool wwAtmoKeyLight( float muLight, const float fog[4], float keyLum45, float sun[3], float amb[3], float dalc[6][3] )
{
	const AtState & s = as();
	if ( s.tier <= 0 || s.sun <= 0 )
		return false;
	const bool flat = wwEnvStr( "WW_ATMO_SUN_RED" ).trimmed().toLower() == QLatin1StringView( "flat" );
	float haze[3];
	hazeFromFog( fog[0], fog[1], fog[2], fog[3], red( "nohaze" ), haze );
	float rows[7][3];
	std::copy( amb, amb + 3, rows[0] );
	for ( int a = 0; a < 6; a++ )
		std::copy( dalc[a], dalc[a] + 3, rows[1 + a] );
	const float was[3] = { sun[0], sun[1], sun[2] };
	skyKeyLight( s.sun, muLight, haze, keyLum45, flat, sun, rows, 7 );
	std::copy( rows[0], rows[0] + 3, amb );
	for ( int a = 0; a < 6; a++ )
		std::copy( rows[1 + a], rows[1 + a] + 3, dalc[a] );
	// stderr: the first change and every 240th after (the light is recomputed once per hour/weather change)
	static int n = 0;
	if ( n++ % 240 == 0 )
		fprintf( stderr, "atmo sun: mode=%s%s mu=%.3f key %.4f,%.4f,%.4f -> %.4f,%.4f,%.4f amb -> %.4f,%.4f,%.4f\n",
			s.sun == 2 ? "full" : "tint", flat ? ",RED=flat" : "", double( muLight ), double( was[0] ), double( was[1] ),
			double( was[2] ), double( sun[0] ), double( sun[1] ), double( sun[2] ), double( amb[0] ), double( amb[1] ),
			double( amb[2] ) );
	return true;
}

void wwAtmoApFrame()
{
	for ( AtGpu & g : gpus() )
		g.ap.valid = false;
}

bool wwAtmoApActive()
{
	const AtState & s = as();
	if ( s.ap <= 0 || s.tier <= 0 )
		return false;
	for ( const AtGpu & g : gpus() )
		if ( g.ap.valid )
			return true;
	return false;
}

void wwAtmoApUniforms( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || prog->uniLocation( "atAp" ) < 0 )
		return;
	const AtState & s = as();
	auto it = gpus().find( r );
	const Volume * v = it != gpus().end() ? &it->ap : nullptr;
	bool on = v && v->valid && s.ap > 0 && s.tier > 0 && !scene->selecting;
	if ( on ) {
		// only the view the volume was built for (a reflection probe's face or a capture reads none)
		const auto & gu = *r->globalUniforms;
		for ( int c = 0; c < 3 && on; c++ )
			for ( int k = 0; k < 3 && on; k++ )
				on = v->view[c * 3 + k] == gu.viewMatrix[c][k];
		on = on && v->proj[0] == gu.projectionMatrix[0][0] && v->proj[1] == gu.projectionMatrix[1][1]
			&& v->proj[2] == gu.projectionMatrix[2][0] && v->proj[3] == gu.projectionMatrix[2][1];
	}
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	GLint prevActive = 0;
	r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AtmoAp ) );
	r->fn->glBindTexture( GL_TEXTURE_3D, on ? v->tex : 0 );
	r->fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "atApTex", WwCellUnit::AtmoAp );
	prog->uni4f( "atAp", FloatVector4( on ? 1.0f : 0.0f, kApMaxKm, on ? v->kmPerView : 0.0f, on ? float( v->n ) : 1.0f ) );
	if ( prog->uniLocation( "atSkyTransTex" ) >= 0 ) {
		// the sky's air (cell_volsky.frag): the physical sky already holds the air along its ray to space, so the
		// Volumetric Air over it keeps only its residual (atmo_apfog.glsl); the ray's transmittance from this LUT
		r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::AtmoTrans ) );
		r->fn->glBindTexture( GL_TEXTURE_2D, on ? it->trans.tex : 0 );
		r->fn->glActiveTexture( GLenum( prevActive ) );
		prog->uni1i( "atSkyTransTex", WwCellUnit::AtmoTrans );
		prog->uni1f( "atSkyCamR", on ? v->camR : 0.0f );
	}
	if ( !on )
		return;
	prog->uni4f( "atApProj", FloatVector4( v->proj[0], v->proj[1], v->proj[2], v->proj[3] ) );
	prog->uni1f( "atApDouble", apRed( "double" ) ? 1.0f : 0.0f );
}

QString wwAtmoSummary()
{
	const AtState & s = as();
	return s.tier <= 0 ? QStringLiteral( "off" ) : s.last;
}
