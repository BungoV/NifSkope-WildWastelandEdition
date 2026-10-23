#include "atmosphere.h"
#include "wwswitch.h"
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

constexpr const char * kKey = "Settings/Render/Scene/Physical Sky";
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
constexpr float kRb = 6360.0f, kRt = 6460.0f;
constexpr float kUnitKm = 0.0142875f / 1000.0f;	// one game unit (70 = 1 m)
constexpr float kRay[3] = { 5.802e-3f, 13.558e-3f, 33.1e-3f };
constexpr float kMieS = 3.996e-3f, kMieE = 4.40e-3f, kMieG = 0.8f;
constexpr float kOzone[3] = { 0.650e-3f, 1.881e-3f, 0.085e-3f };
constexpr float kPi = 3.14159265f;
constexpr int kTransW = 256, kTransH = 64;
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
};

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
	}
	return s;
}

bool red( const char * word )
{
	return wwEnvStr( "WW_ATMO_RED" ).trimmed().toLower() == QLatin1StringView( word );
}

struct Target
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0;
};

struct AtGpu
{
	Target trans, ms, sunView, moonView;
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

/* the haze rung (RESEARCH.md sec. 7): the fog reaches Max at Far; Koschmieder's visibility in its general form,
 * sigma = -ln(1 - min(Max, 0.98)) / Far, less the clear air's ground extinction, is a haze layer flat up to the
 * fog's Far Height Mid and e-folding over its Range (atmo_model.py haze_from_fog) */
void hazeFromFog( const WwAtmoInputs & in, float out[3] )
{
	out[0] = 0.0f;
	out[1] = 1.0f;
	out[2] = 0.0f;
	if ( in.fogFar <= 0.0f || in.fogMax <= 0.0f || red( "nohaze" ) )
		return;
	const float vKm = in.fogFar * kUnitKm;
	const float sigma = -std::log( 1.0f - std::min( in.fogMax, 0.98f ) ) / vKm;
	out[0] = std::clamp( sigma - kRay[1] - kMieE, 0.0f, 50.0f );
	out[1] = std::max( 0.02f, in.fogRange * kUnitKm );
	out[2] = std::max( 0.0f, in.fogMid * kUnitKm );
}

void odAt( float h, const float haze[3], bool noOzone, float e[3] )
{
	h = std::max( h, 0.0f );
	const float dr = std::exp( -h / 8.0f ), dm = std::exp( -h / 1.2f );
	const float dz = std::max( 0.0f, 1.0f - std::fabs( h - 25.0f ) / 15.0f );
	const float dh = haze[0] * std::exp( -std::max( h - haze[2], 0.0f ) / std::max( haze[1], 0.02f ) );
	for ( int c = 0; c < 3; c++ )
		e[c] = kRay[c] * dr + kMieE * dm + dh + ( noOzone ? 0.0f : kOzone[c] ) * dz;
}

void cpuTrans( float hKm, float mu, const float haze[3], bool noOzone, float out[3] )
{
	const float r = kRb + std::max( hKm, 0.0f );
	if ( mu < 0.0f && r * r * ( mu * mu - 1.0f ) + kRb * kRb >= 0.0f ) {
		out[0] = out[1] = out[2] = 0.0f;
		return;
	}
	const float d = std::max( 0.0f, -r * mu + std::sqrt( std::max( 0.0f, r * r * ( mu * mu - 1.0f ) + kRt * kRt ) ) );
	constexpr int N = 256;
	double od[3] = { 0, 0, 0 };
	for ( int i = 0; i < N; i++ ) {
		// packed toward the start: the haze layer sits at the bottom
		const float a0 = float( i ) / N, a1 = float( i + 1 ) / N, am = 0.5f * ( a0 + a1 );
		const float t = d * am * am, w = d * ( a1 * a1 - a0 * a0 );
		const float rr = std::sqrt( r * r + t * t + 2.0f * r * mu * t );
		float e[3];
		odAt( rr - kRb, haze, noOzone, e );
		for ( int c = 0; c < 3; c++ )
			od[c] += double( e[c] ) * w;
	}
	for ( int c = 0; c < 3; c++ )
		out[c] = float( std::exp( -od[c] ) );
}

float lum( const float c[3] )
{
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

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
	hazeFromFog( in, s.haze );
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
	s.last = ok ? QString( "physical(tier=%1,view=%2x%3,ms=%4,haze=%5/km@%6km,sun_elev=%7,k=%8,moon=%9%10)" )
			.arg( QLatin1StringView( tierName( s.tier ) ) ).arg( vw ).arg( vh ).arg( msN )
			.arg( double( s.haze[0] ), 0, 'f', 3 ).arg( double( s.haze[1] ), 0, 'f', 3 ).arg( double( sunEl ), 0, 'f', 1 )
			.arg( double( sunK / kPi ), 0, 'f', 3 ).arg( moonK > 0.0f ? QStringLiteral( "up" ) : QStringLiteral( "down" ) ).arg( reds )
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

QString wwAtmoSummary()
{
	const AtState & s = as();
	return s.tier <= 0 ? QStringLiteral( "off" ) : s.last;
}
