/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellssgi.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellssr.h"
#include "cellunits.h"

#include "gl/celllights.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

/* ---- lane SSGI1: screen-space GI (cellssgi.h, res/shaders/cell_ssgi.frag) ---- */
namespace
{

constexpr int kSsgiUnit = WwCellUnit::Ssgi;

// FO4CS's tiers (ScreenSpaceGI.h ApplyScreenSpaceGIQualityPreset): Low = its Standard, Medium = High, High = Ultra,
// Ultra = Extreme (full resolution); the menu's four rows map to the same ordinals there
struct Tier { bool full; int slices, steps; };
constexpr Tier kTiers[4] = { { false, 4, 8 }, { false, 4, 12 }, { false, 6, 12 }, { true, 4, 8 } };

// FO4CS's live defaults (notes/ssgi1/FO4CS_METHOD.md section 7): fRadius 8 x 32 view units, thickness 32,
// fHistoryBlend 0.9 (rest ceiling 10 frames), fBlurRadius 3, fSaturation 0.8, fAoAmount 1, fIndirectAmount 1
constexpr float kRadius = 256.0f, kThickness = 32.0f, kHistoryBlend = 0.9f, kBlurPx = 3.0f, kSaturation = 0.8f;
constexpr float kMotionAccum = 4.0f;	// FO4CS fMotionAccumFrames
constexpr float kAoAmount = 1.0f, kIndirect = 1.0f, kSource = 1.0f, kNormalWeight = 1.0f;

struct SsgiState
{
	bool loaded = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool on = false, pinOn = false, pinQ = false;
	int quality = 3;
	int red = 0;	// 1 off, 2 stack, 4 notemporal, 8 noblur
	int lastVerdict = -1;
	char echo[160] = {};
};

SsgiState & st()
{
	static SsgiState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_RENDER_SSGI", "WW_CELL_SSGI" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		{
			const SsgiState d {};
			s.red = d.red;
		}
		const QByteArray pin = wwEnv( "WW_RENDER_SSGI" ).trimmed(), q = wwEnv( "WW_CELL_SSGI_QUALITY" ).trimmed().toLower();
		const QByteArray red = wwEnv( "WW_CELL_SSGI_RED" ).trimmed();
		s.pinOn = !pin.isEmpty();
		s.on = s.pinOn ? ( pin != "0" && pin != "off" ) : QSettings().value( QStringLiteral( "WW/CellSsgi" ), false ).toBool();
		s.pinQ = !q.isEmpty();
		const int qs = q == "low" ? 0 : q == "medium" ? 1 : q == "high" ? 2 : q == "ultra" ? 3 : -1;
		s.quality = s.pinQ && qs >= 0 ? qs : std::clamp( QSettings().value( QStringLiteral( "WW/CellSsgiQuality" ), 3 ).toInt(), 0, 3 );
		for ( const QByteArray & w : red.split( ',' ) )
			s.red |= w == "off" ? 1 : w == "stack" ? 2 : w == "notemporal" ? 4 : w == "noblur" ? 8 : 0;
	}
	return s;
}

struct Target
{
	GLuint tex = 0, tex2 = 0, fbo = 0;
	int w = 0, h = 0;
};

struct SsgiGpu
{
	Target cast, acc[2], fin;	// acc: two-sided history, attachment 1 = depth, n, normal xy
	int cur = 0;
	bool haveHistory = false;
	float prevR[9] = {}, prevT[3] = {};
	const void * doc = nullptr;
	bool ready = false;
	int frame = 0;
	int unit = -1;
};

QHash<const void *, SsgiGpu> & gpus()
{
	static QHash<const void *, SsgiGpu> g;
	return g;
}

void alloc( NifSkopeOpenGLContext::GLFunctions * fn, Target & t, int w, int h, bool meta, bool linear )
{
	if ( t.tex && t.w == w && t.h == h )
		return;
	auto mk = [&]( GLuint & tex ) {
		if ( !tex )
			fn->glGenTextures( 1, &tex );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	};
	mk( t.tex );
	if ( meta ) {
		mk( t.tex2 );
		// the depth key needs float precision
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	}
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	if ( !t.fbo )
		fn->glGenFramebuffers( 1, &t.fbo );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	if ( meta )
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, t.tex2, 0 );
	t.w = w;
	t.h = h;
}

}	// namespace

bool wwCellSsgiOn()
{
	return st().on;
}

void wwCellSsgiSetOn( bool on )
{
	SsgiState & s = st();
	if ( s.pinOn )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellSsgi" ), on );
}

int wwCellSsgiQuality()
{
	return st().quality;
}

void wwCellSsgiSetQuality( int q )
{
	SsgiState & s = st();
	if ( s.pinQ )
		return;
	s.quality = std::clamp( q, 0, 3 );
	QSettings().setValue( QStringLiteral( "WW/CellSsgiQuality" ), s.quality );
}

bool wwCellSsgiPinned()
{
	return st().pinOn;
}

bool wwCellSsgiWantsScene( Scene * scene )
{
	return scene && scene->renderer && st().on && wwCellLightsWanted( scene ) && wwCellLightsFor( scene->nifModel );
}

const char * wwCellSsgiEcho()
{
	return st().echo;
}

void wwCellSsgiPass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	SsgiState & s = st();
	Renderer * r = scene->renderer;
	SsgiGpu & g = gpus()[r];
	g.ready = false;
	WwCellAoTargets ao;
	unsigned int sceneTex = 0;
	const int verdict = !run || !wwCellSsgiWantsScene( scene ) ? 0 : !wwCellAoTargets( scene, ao ) ? 1
		: !wwCellSsrSceneTexture( scene, sceneTex ) ? 2 : 3;
	if ( verdict != s.lastVerdict && ( verdict != 0 || s.lastVerdict > 0 ) ) {
		static const char * const words[4] = { "off", "off (no obscurance pass)", "off (no scene pass)", "on" };
		std::snprintf( s.echo, sizeof( s.echo ), "cell ssgi: %s quality %d red %d", words[verdict], s.quality, s.red );
		std::fprintf( stderr, "%s\n", s.echo );
		s.lastVerdict = verdict;
	}
	if ( verdict != 3 ) {
		g.haveHistory = false;
		return;
	}
	auto fn = r->fn;
	if ( g.unit < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );	// MERGEALL1: unit 33 is past the per-stage 32
		g.unit = units > kSsgiUnit ? kSsgiUnit : 0;
	}
	if ( g.unit <= 0 )
		return;
	const Tier tier = kTiers[s.quality];
	const int W = ao.w, H = ao.h;
	const int ww = tier.full ? W : ( W + 1 ) / 2, wh = tier.full ? H : ( H + 1 ) / 2;
	GLint prevFbo = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 }, prevActive = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );

	const bool resized = g.cast.w != ww || g.cast.h != wh;
	alloc( fn, g.cast, ww, wh, false, false );
	alloc( fn, g.acc[0], ww, wh, true, false );
	alloc( fn, g.acc[1], ww, wh, true, false );
	alloc( fn, g.fin, ww, wh, false, true );
	if ( resized || g.doc != scene->nifModel )
		g.haveHistory = false;

	// this view (game units): v = R w + t / sc; last frame's the same; cur -> prev = Rp R^T (v - t) + tp
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float R[9], T[3];
	for ( int j = 0; j < 3; j++ ) {
		for ( int k = 0; k < 3; k++ )
			R[j * 3 + k] = vt.rotation( j, k );
		T[j] = vt.translation[j] / sc;
	}
	float M[9], Mt[3];	// M = Rp R^T (row major), Mt = tp - M t
	for ( int i = 0; i < 3; i++ ) {
		for ( int k = 0; k < 3; k++ ) {
			float a = 0.0f;
			for ( int j = 0; j < 3; j++ )
				a += g.prevR[i * 3 + j] * R[k * 3 + j];
			M[i * 3 + k] = a;
		}
	}
	for ( int i = 0; i < 3; i++ )
		Mt[i] = g.prevT[i] - ( M[i * 3] * T[0] + M[i * 3 + 1] * T[1] + M[i * 3 + 2] * T[2] );
	float Mcol[9];	// GL wants column major
	for ( int i = 0; i < 3; i++ )
		for ( int k = 0; k < 3; k++ )
			Mcol[k * 3 + i] = M[i * 3 + k];

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_ssgi.prog" );
	if ( !prog ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		return;
	}
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	const auto & pm = r->globalUniforms->projectionMatrix;
	prog->uni1i( "gbuf", 0 );
	prog->uni1i( "scene", 1 );
	prog->uni1i( "src", 2 );
	prog->uni1i( "hist", 3 );
	prog->uni1i( "histMeta", 4 );
	fn->glUniform2i( prog->uniLocation( "fullSize" ), W, H );
	fn->glUniform2i( prog->uniLocation( "workSize" ), ww, wh );
	prog->uni2f( "proj", pm[0][0], pm[1][1] );
	prog->uni1i( "slices", tier.slices );
	prog->uni1i( "steps", tier.steps );
	prog->uni1f( "radius", kRadius );
	prog->uni1f( "thickness", kThickness );
	prog->uni1f( "sourceBrightness", kSource );
	prog->uni1f( "saturation", kSaturation );
	prog->uni1f( "historyBlend", kHistoryBlend );
	prog->uni1f( "maxAccum", 1.0f / ( 1.0f - kHistoryBlend ) );
	prog->uni1f( "blurRadius", ( s.red & 8 ) ? 0.0f : kBlurPx * ( tier.full ? 1.0f : 0.5f ) );
	prog->uni1f( "normalWeight", kNormalWeight );
	prog->uni1i( "frameIndex", g.frame & 63 );
	prog->uni1b( "historyValid", g.haveHistory && !( s.red & 4 ) );
	bool moving = false;
	for ( int i = 0; i < 9; i++ )
		moving = moving || std::fabs( M[i] - ( i % 4 == 0 ? 1.0f : 0.0f ) ) > 1.0e-5f;
	for ( int i = 0; i < 3; i++ )
		moving = moving || std::fabs( Mt[i] ) > 1.0e-3f;
	prog->uni1b( "moving", moving );
	prog->uni1f( "motionAccum", kMotionAccum );
	fn->glUniformMatrix3fv( prog->uniLocation( "curToPrev" ), 1, GL_FALSE, Mcol );
	prog->uni3f( "curToPrevT", Mt[0], Mt[1], Mt[2] );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const Target & prev = g.acc[g.cur ^ 1];
	Target & now = g.acc[g.cur];
	auto bind = [&]( int u, GLuint t ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, t );
	};
	auto pass = [&]( int stage, const Target & out, GLuint in, int nDraw ) {
		bind( 2, in );
		prog->uni1i( "ssgiStage", stage );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, out.fbo );
		const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( nDraw, bufs );
		fn->glViewport( 0, 0, out.w, out.h );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	};
	bind( 0, ao.gbuf );
	bind( 1, sceneTex );
	// the donor's mips for the far taps; lane SSR1 reads level 0 alone, so its filter is put back below
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4 );
	fn->glGenerateMipmap( GL_TEXTURE_2D );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	bind( 3, prev.tex );
	bind( 4, prev.tex2 );
	pass( 1, g.cast, 0, 1 );
	pass( 2, now, g.cast.tex, 2 );
	pass( 3, g.fin, now.tex, 1 );
	r->stopProgram();
	bind( 1, sceneTex );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	for ( int u = 0; u < 5; u++ )
		bind( u, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	{
		const GLenum b0 = GL_COLOR_ATTACHMENT0;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, now.fbo );
		fn->glDrawBuffers( 1, &b0 );
	}

	std::memcpy( g.prevR, R, sizeof( R ) );
	std::memcpy( g.prevT, T, sizeof( T ) );
	g.cur ^= 1;
	g.haveHistory = true;
	g.frame++;
	g.ready = true;
	g.doc = scene->nifModel;

	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glDepthMask( depthMask );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST ); else fn->glDisable( GL_SCISSOR_TEST );
}

void wwCellSsgiDraw( Scene * scene, bool cellProgram )
{
	if ( !scene || !scene->renderer || !cellProgram )
		return;
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	if ( !prog || !wwIsCellProgramName( prog->name ) || prog->uniLocation( "cellSsgiOn" ) < 0 )
		return;
	const SsgiState & s = st();
	const auto it = gpus().constFind( r );
	const bool have = it != gpus().constEnd() && it->unit > 0;
	// the scene pass (lane SSR1's) is the donor: it never reads the bounce it feeds
	const bool on = have && it->ready && it->doc == scene->nifModel && !( s.red & 1 ) && !wwCellSsrInPass()
		&& !glIsEnabled( GL_BLEND ) && wwCellLightsWanted( scene );
	prog->uni1b( "cellSsgiOn", on );
	if ( !have )
		return;
	GLint prevActive = 0;
	r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + it->unit ) );
	r->fn->glBindTexture( GL_TEXTURE_2D, on ? it->fin.tex : 0 );
	r->fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellSsgi", it->unit );
	if ( on ) {
		GLint vp[4] = { 0, 0, 1, 1 };
		glGetIntegerv( GL_VIEWPORT, vp );
		prog->uni4f_l( prog->uniLocation( "cellSsgiRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
		prog->uni2f( "cellSsgiK", kAoAmount, kIndirect );
		prog->uni1i( "cellSsgiRed", s.red );
	}
}
