/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellssgi.h"
#include "cellssr.h"
#include "cellunits.h"

#include "gl/celllights.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QFile>
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
constexpr int kEmissiveSlices = 2;	// FO4CS iEmissiveSlices

struct SsgiState
{
	bool loaded = false;
	bool on = false, pinOn = false, pinQ = false;
	int quality = 3;
	int red = 0;	// 1 off, 2 stack, 4 notemporal, 8 noblur; lane SSGI2: 16 ign, 32 nohiz, 64 oldblur, 128 noemissive;
					// SSGI2FIX: 256 oldfill, 512 oldframes
	int pathFrame = -1;	// SSGI2FIX: the camera path's frame (wwCellSsgiSetPath), -1 a still view
	int lastVerdict = -1;
	char echo[240] = {};
};

SsgiState & st()
{
	static SsgiState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray pin = qgetenv( "WW_RENDER_SSGI" ).trimmed(), q = qgetenv( "WW_CELL_SSGI_QUALITY" ).trimmed().toLower();
		const QByteArray red = qgetenv( "WW_CELL_SSGI_RED" ).trimmed();
		s.pinOn = !pin.isEmpty();
		s.on = s.pinOn ? ( pin != "0" && pin != "off" ) : QSettings().value( QStringLiteral( "WW/CellSsgi" ), false ).toBool();
		s.pinQ = !q.isEmpty();
		const int qs = q == "low" ? 0 : q == "medium" ? 1 : q == "high" ? 2 : q == "ultra" ? 3 : -1;
		s.quality = s.pinQ && qs >= 0 ? qs : std::clamp( QSettings().value( QStringLiteral( "WW/CellSsgiQuality" ), 3 ).toInt(), 0, 3 );
		for ( const QByteArray & w : red.split( ',' ) )
			s.red |= w == "off" ? 1 : w == "stack" ? 2 : w == "notemporal" ? 4 : w == "noblur" ? 8 : w == "ign" ? 16
				: w == "nohiz" ? 32 : w == "oldblur" ? 64 : w == "noemissive" ? 128 : w == "oldfill" ? 256
				: w == "oldframes" ? 512 : 0;
	}
	return s;
}

struct Target
{
	GLuint tex = 0, tex2 = 0, tex3 = 0, tex4 = 0, fbo = 0;	// tex4: SSGI2FIX's shut moment (acc)
	int w = 0, h = 0;
};

struct SsgiGpu
{
	Target cast, acc[2], fin;	// acc: two-sided history, attachment 1 = depth, n, normal xy, 2 = the moments
	Target qraw, qfilt;			// lane SSGI2: the quarter pyramid (qraw attachment 1 = its guide)
	Target ecast, eacc[2], efin;	// lane SSGI2: the emissive cast's own chain (as cast, acc, fin)
	bool eHistory = false;
	GLuint hizTex = 0, hizFbo = 0, noiseTex = 0;
	int hizW = 0, hizH = 0, hizN = 0;
	bool noiseTried = false;
	int cur = 0;
	bool haveHistory = false;
	float prevR[9] = {}, prevT[3] = {};
	const void * doc = nullptr;
	bool ready = false;
	int frame = 0;
	int unit = -1;
	// SSGI2FIX: a harness run's temporal state belongs to the view (or path frame), not to how many paints ran
	struct Key { const void * doc; int w, h, path; float p0, p1, R[9], T[3]; };
	struct Snap { int cur, frame; bool have, eHave; float R[9], T[3]; };
	Key key = {};
	bool keyValid = false;
	Snap snap = {};
};

QHash<const void *, SsgiGpu> & gpus()
{
	static QHash<const void *, SsgiGpu> g;
	return g;
}

void alloc( NifSkopeOpenGLContext::GLFunctions * fn, Target & t, int w, int h, int meta, bool linear )
{
	if ( t.tex && t.w == w && t.h == h && ( meta < 1 || t.tex2 ) && ( meta < 2 || t.tex3 ) && ( meta < 3 || t.tex4 ) )
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
	if ( meta >= 1 ) {
		mk( t.tex2 );
		// the depth key needs float precision
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	}
	if ( meta >= 2 ) {
		mk( t.tex3 );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	}
	if ( meta >= 3 )
		mk( t.tex4 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	if ( !t.fbo )
		fn->glGenFramebuffers( 1, &t.fbo );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	if ( meta >= 1 )
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, t.tex2, 0 );
	if ( meta >= 2 )
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, t.tex3, 0 );
	if ( meta >= 3 )
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, t.tex4, 0 );
	t.w = w;
	t.h = h;
}

// lane SSGI2: EA's FAST spatiotemporal blue noise (res/ssgi, BSD-3; the sheet FO4CS's cast reads), 64 frames of 128 x 128
GLuint loadNoise( NifSkopeOpenGLContext::GLFunctions * fn )
{
	QFile f( QStringLiteral( ":/ssgi/fast_2uges.rg8" ) );
	if ( !f.open( QIODevice::ReadOnly ) )
		return 0;
	const QByteArray b = f.readAll();
	if ( b.size() != 128 * 8192 * 2 )
		return 0;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RG8, 128, 8192, 0, GL_RG, GL_UNSIGNED_BYTE, b.constData() );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
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

bool wwCellSsgiWantsEmission( Scene * scene )
{
	return wwCellSsgiWantsScene( scene ) && !( st().red & 128 );
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
	unsigned int sceneTex = 0, emitTex = 0;
	const int verdict = !run || !wwCellSsgiWantsScene( scene ) ? 0 : !wwCellAoTargets( scene, ao ) ? 1
		: !wwCellSsrSceneTexture( scene, sceneTex ) ? 2 : 3;
	if ( verdict != s.lastVerdict && ( verdict != 0 || s.lastVerdict > 0 ) ) {
		static const char * const words[4] = { "off", "off (no obscurance pass)", "off (no scene pass)", "on" };
		std::snprintf( s.echo, sizeof( s.echo ),
			"cell ssgi: %s quality %d red %d noise %s hiz %s denoise %s emissive %s fill %s frames %s",
			words[verdict], s.quality, s.red, ( s.red & 16 ) ? "ign" : "fast", ( s.red & 32 ) ? "off" : "on",
			( s.red & 8 ) ? "none" : ( s.red & 64 ) ? "5x5" : "robust+quarter", ( s.red & 128 ) ? "off" : "on",
			( s.red & 256 ) ? "old" : "balanced", !wwHarnessRun() ? "live" : ( s.red & 512 ) ? "per-paint" : "pinned" );
		std::fprintf( stderr, "%s\n", s.echo );
		s.lastVerdict = verdict;
	}
	if ( verdict != 3 ) {
		g.haveHistory = false;
		g.eHistory = false;
		return;
	}
	const bool emis = !( s.red & 128 ) && wwCellSsrEmissionTexture( scene, emitTex );
	auto fn = r->fn;
	if ( g.unit < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
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
	const bool newBlur = !( s.red & ( 8 | 64 ) ), hizOn = !( s.red & 32 );
	const int qw = ( ww + 1 ) / 2, qh = ( wh + 1 ) / 2;
	// SSGI2FIX: the cast's location 1 and acc's attachment 3 carry the shut moment; the result's upper half
	// holds it for the surfaces (one sampler: the cell programs have no unit to spare)
	alloc( fn, g.cast, ww, wh, 1, false );
	alloc( fn, g.acc[0], ww, wh, 3, false );
	alloc( fn, g.acc[1], ww, wh, 3, false );
	alloc( fn, g.fin, ww, wh * 2, 0, true );
	if ( newBlur ) {
		alloc( fn, g.qraw, qw, qh, 1, false );
		alloc( fn, g.qfilt, qw, qh, 0, false );
	}
	if ( emis ) {
		const bool eResized = g.ecast.w != ww || g.ecast.h != wh;
		alloc( fn, g.ecast, ww, wh, 0, false );
		alloc( fn, g.eacc[0], ww, wh, newBlur ? 2 : 1, false );
		alloc( fn, g.eacc[1], ww, wh, newBlur ? 2 : 1, false );
		alloc( fn, g.efin, ww, wh, 0, false );
		if ( eResized )
			g.eHistory = false;
	} else
		g.eHistory = false;
	if ( !g.noiseTried ) {
		g.noiseTried = true;
		g.noiseTex = loadNoise( fn );
		if ( !g.noiseTex )
			std::fprintf( stderr, "cell ssgi: blue noise sheet missing, IGN used\n" );
	}
	const bool blue = g.noiseTex && !( s.red & 16 );
	// the interval pyramid: level i is mip i + 1 of the full-size depth, up to FO4CS's geometry mip 4
	int hizN = 0;
	if ( hizOn ) {
		while ( hizN < 4 && ( W >> ( hizN + 1 ) ) >= 1 && ( H >> ( hizN + 1 ) ) >= 1 )
			hizN++;
		if ( !g.hizTex || g.hizW != W || g.hizH != H || g.hizN != hizN ) {
			if ( !g.hizTex )
				fn->glGenTextures( 1, &g.hizTex );
			fn->glBindTexture( GL_TEXTURE_2D, g.hizTex );
			for ( int i = 0; i < hizN; i++ )
				fn->glTexImage2D( GL_TEXTURE_2D, i, GL_RG32F, std::max( W >> ( i + 1 ), 1 ), std::max( H >> ( i + 1 ), 1 ), 0,
					GL_RG, GL_FLOAT, nullptr );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, hizN - 1 );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
			if ( !g.hizFbo )
				fn->glGenFramebuffers( 1, &g.hizFbo );
			g.hizW = W;
			g.hizH = H;
			g.hizN = hizN;
		}
	}
	if ( resized || g.doc != scene->nifModel )
		g.haveHistory = g.eHistory = false;

	// this view (game units): v = R w + t / sc
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	float R[9], T[3];
	for ( int j = 0; j < 3; j++ ) {
		for ( int k = 0; k < 3; k++ )
			R[j * 3 + k] = vt.rotation( j, k );
		T[j] = vt.translation[j] / sc;
	}
	const auto & pm = r->globalUniforms->projectionMatrix;

	// SSGI2FIX: in a harness run a new view (or a path jump) starts the history and pre-rolls kPreroll frames at once,
	// the next path frame advances one, and a repaint of the same one redoes its last frame from the saved state, so
	// the grab never depends on how many paints the launch happened to run (red `oldframes`: one frame per paint)
	constexpr int kPreroll = 16;
	int subFrames = 1;
	if ( wwHarnessRun() && !( s.red & 512 ) ) {
		SsgiGpu::Key k = {};
		k.doc = scene->nifModel;
		k.w = W;
		k.h = H;
		k.path = s.pathFrame;
		k.p0 = pm[0][0];
		k.p1 = pm[1][1];
		if ( k.path < 0 ) {
			std::memcpy( k.R, R, sizeof( R ) );
			std::memcpy( k.T, T, sizeof( T ) );
		}
		SsgiGpu::Key nextOf = g.key;
		nextOf.path = g.key.path + 1;
		const bool same = g.keyValid && g.haveHistory && std::memcmp( &k, &g.key, sizeof( k ) ) == 0;
		const bool next = g.keyValid && g.haveHistory && k.path >= 0 && g.key.path >= 0
			&& std::memcmp( &k, &nextOf, sizeof( k ) ) == 0;
		if ( same ) {
			g.cur = g.snap.cur;
			g.frame = g.snap.frame;
			g.haveHistory = g.snap.have;
			g.eHistory = g.snap.eHave;
			std::memcpy( g.prevR, g.snap.R, sizeof( R ) );
			std::memcpy( g.prevT, g.snap.T, sizeof( T ) );
		} else if ( !next ) {
			g.haveHistory = g.eHistory = false;
			g.frame = 0;
			std::memcpy( g.prevR, R, sizeof( R ) );
			std::memcpy( g.prevT, T, sizeof( T ) );
			subFrames = kPreroll;
		}
		g.key = k;
		g.keyValid = true;
	} else
		g.keyValid = false;

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
	prog->uni1i( "gbuf", 0 );
	prog->uni1i( "scene", 1 );
	prog->uni1i( "src", 2 );
	prog->uni1i( "hist", 3 );
	prog->uni1i( "histMeta", 4 );
	prog->uni1i( "noiseTex", 5 );
	prog->uni1i( "hiz", 6 );
	prog->uni1i( "tstate", 7 );
	prog->uni1i( "qraw", 3 );
	prog->uni1i( "qguide", 4 );
	prog->uni1i( "qfilt", 8 );
	prog->uni1i( "emitTex", 9 );
	prog->uni1i( "emitFin", 9 );
	prog->uni1i( "emitSlices", kEmissiveSlices );
	prog->uni1b( "emitSub", emis );
	prog->uni1b( "emissivePass", false );
	prog->uni1b( "emitAdd", false );
	prog->uni1b( "blueNoise", blue );
	prog->uni1i( "hizLevels", hizN );
	prog->uni1b( "wantState", newBlur );
	prog->uni1f( "robustRadius", kBlurPx );
	fn->glUniform2i( prog->uniLocation( "quarterSize" ), qw, qh );
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
	prog->uni1f( "motionAccum", kMotionAccum );
	prog->uni1b( "oldFill", ( s.red & 256 ) != 0 );
	prog->uni1i( "srcBent", 10 );
	prog->uni1i( "histBent", 11 );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	auto bind = [&]( int u, GLuint t ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, t );
	};
	const GLenum bufs[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	// vy, vh: a band of out (the result's halves); 0 = all of it
	auto pass = [&]( int stage, const Target & out, GLuint in, int nDraw, int vy = 0, int vh = 0 ) {
		bind( 2, in );
		prog->uni1i( "ssgiStage", stage );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, out.fbo );
		fn->glDrawBuffers( nDraw, bufs );
		fn->glViewport( 0, vy, out.w, vh > 0 ? vh : out.h );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	};
	bind( 0, ao.gbuf );
	if ( hizN > 0 ) {
		// each level reads the one below it alone (base = max = that level), so no level is read while drawn
		bind( 6, g.hizTex );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.hizFbo );
		for ( int i = 0; i < hizN; i++ ) {
			if ( i > 0 ) {
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, i - 1 );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, i - 1 );
			}
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.hizTex, i );
			fn->glDrawBuffers( 1, bufs );
			prog->uni1i( "ssgiStage", i == 0 ? 10 : 11 );
			fn->glViewport( 0, 0, std::max( W >> ( i + 1 ), 1 ), std::max( H >> ( i + 1 ), 1 ) );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		}
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, hizN - 1 );
	}
	bind( 5, blue ? g.noiseTex : 0 );
	bind( 1, sceneTex );
	// the donor's mips for the far taps; lane SSR1 reads level 0 alone, so its filter is put back below
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4 );
	fn->glGenerateMipmap( GL_TEXTURE_2D );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	// one chain: cast, temporal, denoise into out (the emissive one with its spans, then the ordinary adding it)
	auto chain = [&]( int castStage, Target & cst, const Target & pv, Target & nw, Target & out ) {
		bind( 3, pv.tex );
		bind( 4, pv.tex2 );
		bind( 7, newBlur ? pv.tex3 : 0 );
		const bool ordinary = castStage == 1;
		bind( 10, 0 );	// the cast writes it
		bind( 11, ordinary ? pv.tex4 : 0 );
		pass( castStage, cst, 0, ordinary ? 2 : 1 );
		bind( 10, ordinary ? cst.tex2 : 0 );
		pass( 2, nw, cst.tex, ordinary ? 4 : newBlur ? 3 : 2 );
		const bool add = emis && castStage == 1;
		if ( add )
			bind( 9, g.efin.tex );	// the emissive result now (the emission's own unit, done with)
		prog->uni1b( "emitAdd", add );
		if ( !newBlur ) {
			pass( 3, out, nw.tex, 1, 0, wh );	// SSGI1's 5x5 (red `oldblur`), or none (red `noblur`)
		} else {
			// FO4CS's robust denoise into the cast target (spent), then the quarter pyramid into out
			bind( 7, nw.tex3 );
			pass( 4, cst, nw.tex, 1 );
			pass( 5, g.qraw, cst.tex, 2 );
			bind( 3, g.qraw.tex );
			bind( 4, g.qraw.tex2 );
			pass( 6, g.qfilt, 0, 1 );
			bind( 8, g.qfilt.tex );
			pass( 7, out, cst.tex, 1, 0, wh );
		}
		if ( ordinary )
			pass( 12, out, nw.tex4, 1, wh, wh );	// the moment into the result's upper half
	};
	for ( int sub = 0; sub < subFrames; sub++ ) {
		bind( 9, emis ? GLuint( emitTex ) : 0 );	// the last chain left efin there
		if ( sub == subFrames - 1 ) {
			g.snap = { g.cur, g.frame, g.haveHistory, g.eHistory, {}, {} };
			std::memcpy( g.snap.R, g.prevR, sizeof( R ) );
			std::memcpy( g.snap.T, g.prevT, sizeof( T ) );
		}
		// cur -> prev = Rp R^T (v - t) + tp
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
		bool moving = false;
		for ( int i = 0; i < 9; i++ )
			moving = moving || std::fabs( M[i] - ( i % 4 == 0 ? 1.0f : 0.0f ) ) > 1.0e-5f;
		for ( int i = 0; i < 3; i++ )
			moving = moving || std::fabs( Mt[i] ) > 1.0e-3f;
		prog->uni1b( "moving", moving );
		fn->glUniformMatrix3fv( prog->uniLocation( "curToPrev" ), 1, GL_FALSE, Mcol );
		prog->uni3f( "curToPrevT", Mt[0], Mt[1], Mt[2] );
		prog->uni1i( "frameIndex", g.frame & 63 );
		if ( emis ) {
			// FO4CS: the emitters' own cast, history and denoise; the ordinary result adds it at the end
			prog->uni1b( "emissivePass", true );
			prog->uni1b( "historyValid", g.eHistory && !( s.red & 4 ) );
			chain( 8, g.ecast, g.eacc[g.cur ^ 1], g.eacc[g.cur], g.efin );
			prog->uni1b( "emissivePass", false );
		}
		// the ordinary cast reads the emission (unit 9) to take it off its donor; its last stage reads efin there
		prog->uni1b( "historyValid", g.haveHistory && !( s.red & 4 ) );
		chain( 1, g.cast, g.acc[g.cur ^ 1], g.acc[g.cur], g.fin );
		std::memcpy( g.prevR, R, sizeof( R ) );
		std::memcpy( g.prevT, T, sizeof( T ) );
		g.cur ^= 1;
		g.haveHistory = true;
		g.eHistory = emis;
		g.frame++;
	}
	prog->uni1b( "emitAdd", false );
	r->stopProgram();
	bind( 1, sceneTex );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	for ( int u = 0; u < 12; u++ )
		bind( u, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	for ( const Target & a : g.acc ) {
		const GLenum b0 = GL_COLOR_ATTACHMENT0;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, a.fbo );
		fn->glDrawBuffers( 1, &b0 );
	}

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

void wwCellSsgiSetPath( int frame )
{
	st().pathFrame = frame;
}
