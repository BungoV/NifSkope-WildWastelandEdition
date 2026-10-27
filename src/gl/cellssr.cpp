/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellssr.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellssgi.h"
#include "gl/cellpasstime.h"	// lane PERF2: the draws' CPU setup time
#include "gl/celldrawstate.h"	// lane PERF3
#include "cellunits.h"
#include "celluniform.h"	// lane CELLUNI2

#include "gl/celllights.h"
#include "gl/cellwater.h"
#include "gl/celllooks.h"	// lane WATERREFL1: the Water reflections row picks the water march's twin
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

/* ---- lane SSR1: the screen-space reflections (cellssr.h, res/shaders/cell_ssr.frag) ---- */
namespace
{

constexpr int kSsrUnit = WwCellUnit::Ssr;			// the reflection (sampler2D, half the view); 10..16 are the cell lights' and AO's
// the game's: color scale, angle gate, normal z scale, confidence scale; its camera's near; its far ceiling
constexpr float kSsrK[4] = { 1.0f, 0.2f, 2.0f, 1.0f };
constexpr float kSsrNear = 15.0f, kSsrFarMax = 353840.0f;

struct SsrState
{
	bool loaded = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	int red = 0;			// 1 off (computed, not applied), 2 nogap, 4 nofade
	int probe = 0;			// WW_CELL_LIT_PROBE: another lane's probe reads its own term, without the reflection
	QString dump;			// WW_CELL_SSR_DUMP
	bool pass = false;		// the scene pass is drawing
	bool waterRay = false;	// lane WATER2: the water ray pass is drawing
	unsigned int gbuf = 0;
	bool flagged = false;	// the draw being set up carries the flag
	int zreuse = 0;			// lane PERF4: the scene pass tests against the obscurance pass's depth (1; 2 = its red)
	bool on = true, ext = false, pinOn = false, pinExt = false;	// lane CELLALL1: the rows (cellssr.h)
	int lastVerdict = -1;	// lane CELLALL1: the telemetry line prints when it changes
};

SsrState & ssr()
{
	static SsrState s;
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_SSR", "WW_CELL_LIT_PROBE" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		const QByteArray red = wwEnv( "WW_CELL_SSR_RED" ).trimmed();
		s.red = red == "off" ? 1 : red == "nogap" ? 2 : red == "nofade" ? 4 : 0;
		s.dump = QString::fromLocal8Bit( wwEnv( "WW_CELL_SSR_DUMP" ) );
		s.probe = wwEnvInt( "WW_CELL_LIT_PROBE" );
		const QByteArray pinOn = wwEnv( "WW_CELL_SSR" ).trimmed(), pinExt = wwEnv( "WW_CELL_SSR_EXT" ).trimmed();
		s.pinOn = !pinOn.isEmpty();
		s.pinExt = !pinExt.isEmpty();
		s.on = s.pinOn ? pinOn != "0" : QSettings().value( QStringLiteral( "WW/CellSsr" ), true ).toBool();
		s.ext = s.pinExt ? pinExt != "0" : QSettings().value( QStringLiteral( "WW/CellSsrExterior" ), false ).toBool();
	}
	return s;
}

struct SsrTarget
{
	GLuint tex = 0, fbo = 0;
	int w = 0, h = 0;
};

struct SsrGpu
{
	SsrTarget scene;			// full size: the lit color without the reflection, alpha the flag (RGBA32F, bilinear)
	SsrTarget ray;				// half size: u0, v0, start depth
	SsrTarget raw, across, fin;	// half size: rgb the reflected color, a the confidence (bilinear)
	SsrTarget wray;				// lane WATER2: full size, bottom row first: the water's ray in the view, view depth
	SsrTarget emission;				// lane SSGI2: full size, the scene pass's emitted share (RGBA16F, a = coverage, mips 0..4)
	const void * doc = nullptr;
	bool ready = false;
	bool sceneReady = false;	// lane SSGI1: step 1 drew this frame (the bounce's donor)
	bool emitReady = false;		// lane SSGI2: step 1 also wrote the emission (the bounce's emissive cast)
	bool waterOnly = false;		// an exterior: the pass ran for the water alone
	bool haveWater = false;		// the water ray pass drew
	int unit = -1;
	// lane PERF3 (WW_CELL_SSR_PREV): the previous frame's resolved linear color and its camera
	SsrTarget prev;
	bool prevValid = false;
	const void * prevDoc = nullptr;
	float prevRot[9] = {}, prevT[3] = {}, prevSc = 1.0f, prevP00 = 1.0f, prevP11 = 1.0f;
	int lastPrev = -1;			// the telemetry line prints when the verdict changes
};

bool ssrPrevOn()
{
	// lane PERF5: a speed change, on with the reflections; WW_CELL_SSR_PREV=0 or a red is the test way back
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_CELL_SSR_PREV" ).trimmed() != "0" && wwEnvInt( "WW_CELL_SSR_PREV_RED" ) == 0
		&& wwEnvInt( "WW_PERF3_RED" ) == 0 );	// lane LIVESET1: follows a live set
	return on;
}

QHash<const void *, SsrGpu> & ssrGpus()
{
	static QHash<const void *, SsrGpu> g;
	return g;
}

void ssrAlloc( NifSkopeOpenGLContext::GLFunctions * fn, SsrTarget & t, int w, int h, bool linear )
{
	if ( t.tex && t.w == w && t.h == h )
		return;
	if ( !t.tex ) {
		fn->glGenTextures( 1, &t.tex );
		fn->glGenFramebuffers( 1, &t.fbo );
	}
	fn->glBindTexture( GL_TEXTURE_2D, t.tex );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	t.w = w;
	t.h = h;
}

bool ssrIsCellProgram( const NifSkopeOpenGLContext::Program * p )
{
	return p && ( wwIsCellProgramName( p->name ) );
}

}	// namespace

bool wwCellSsrOn()
{
	return ssr().on;
}

void wwCellSsrSetOn( bool on )
{
	SsrState & s = ssr();
	if ( s.pinOn )
		return;
	s.on = on;
	QSettings().setValue( QStringLiteral( "WW/CellSsr" ), on );
}

bool wwCellSsrExteriorOn()
{
	return ssr().ext;
}

void wwCellSsrExteriorSetOn( bool on )
{
	SsrState & s = ssr();
	if ( s.pinExt )
		return;
	s.ext = on;
	QSettings().setValue( QStringLiteral( "WW/CellSsrExterior" ), on );
}

bool wwCellSsrWantsTargets( Scene * scene )
{
	if ( !scene || !scene->renderer || !wwCellLightsWanted( scene ) )
		return false;
	const SsrState & s = ssr();
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	// the same verdict as wwCellSsrPass, before its "no obscurance pass" step
	return ( L && s.on && ( L->interior || s.ext || wwCellWaterSsrWanted( scene ) ) ) || wwCellSsgiWantsScene( scene );
}

void wwCellSsrNote( bool flagged )
{
	ssr().flagged = flagged;
}

bool wwCellSsrPrevStencil()
{
	return ssrPrevOn() && ssr().on;
}

bool wwCellSsrFlagged()
{
	return ssr().flagged;
}

void wwCellSsrKeepFrame( Scene * scene, unsigned int hdrFbo, int w, int h )
{
	if ( !wwCellSsrPrevStencil() || !scene || !scene->renderer || !hdrFbo || w <= 0 || h <= 0 )
		return;
	Renderer * r = scene->renderer;
	SsrGpu & g = ssrGpus()[r];
	auto fn = r->fn;
	GLint prevDraw = 0, prevRead = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	ssrAlloc( fn, g.prev, w, h, true );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( hdrFbo ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.prev.fbo );
	fn->glBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST );	// the samples resolved
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	const Transform & vt = scene->view;
	for ( int j = 0; j < 3; j++ ) {
		for ( int k = 0; k < 3; k++ )
			g.prevRot[j * 3 + k] = vt.rotation( j, k );
		g.prevT[j] = vt.translation[j];
	}
	g.prevSc = vt.scale != 0.0f ? vt.scale : 1.0f;
	const auto & pm = r->globalUniforms->projectionMatrix;
	g.prevP00 = pm[0][0];
	g.prevP11 = pm[1][1];
	g.prevValid = true;
	g.prevDoc = scene->nifModel;
}

void wwCellSsrPass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	SsrState & s = ssr();
	Renderer * r = scene->renderer;
	SsrGpu & g = ssrGpus()[r];
	g.ready = false;
	g.sceneReady = false;
	if ( !run || !wwCellLightsWanted( scene ) )
		return;
	const WwCellLighting * L = wwCellLightsFor( scene->nifModel );
	WwCellAoTargets ao;
	const bool waterSsr = wwCellWaterSsrWanted( scene );	// lane WATER2
	// lane CELLALL1: the rows; an exterior only with "SSR outdoors" (before: interiors only)
	/* finalfix: an exterior with the SSR outdoors row off runs the pass for the water alone (WATER2): it says so,
	 * as verdict 5, rather than "on" -- the row still ships off and the line has to show it */
	int verdict = !L ? 0 : !s.on ? 1 : ( !L->interior && !s.ext && !waterSsr ) ? 2 : !wwCellAoTargets( scene, ao ) ? 3 : 4;
	if ( verdict == 4 && !L->interior && !s.ext )
		verdict = 5;
	if ( verdict != s.lastVerdict ) {
		static const char * const words[6] = { "no cell", "off (row)", "off (exterior, SSR outdoors row off)",
			"off (no obscurance pass)", "on", "water only (exterior, SSR outdoors row off)" };
		std::fprintf( stderr, "cell ssr: %s %s\n", words[verdict], L ? ( L->interior ? "interior" : "exterior" ) : "-" );
		s.lastVerdict = verdict;
	}
	// lane SSGI1: the bounce reads step 1 alone; the printed verdict stays the reflections'
	const bool sceneOnly = verdict != 4 && verdict != 5 && L && wwCellSsgiWantsScene( scene ) && wwCellAoTargets( scene, ao );
	if ( verdict != 4 && verdict != 5 && !sceneOnly )
		return;
	g.waterOnly = !L->interior && !s.ext;	// merge: SSR outdoors runs the full pass, else water only (WATER2)
	g.haveWater = false;
	QElapsedTimer timer;
	timer.start();
	auto fn = r->fn;
	if ( g.unit < 0 ) {
		GLint units = 0;
		fn->glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &units );
		g.unit = units > kSsrUnit ? kSsrUnit : 0;
	}
	if ( g.unit <= 0 )
		return;
	GLint prevFbo = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 }, prevActive = 0;
	GLboolean depthMask = GL_TRUE, colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const int W = ao.w, H = ao.h;
	const int hw = ( W + 1 ) / 2, hh = ( H + 1 ) / 2;
	auto restore = [&]() {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevFbo ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
		fn->glDepthMask( depthMask );
		if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
		if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
		if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
		if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST ); else fn->glDisable( GL_SCISSOR_TEST );
		if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	};

	// the targets; the scene pass borrows the obscurance pass's depth buffer (that pass is done with it)
	ssrAlloc( fn, g.scene, W, H, true );
	if ( !sceneOnly ) {
	ssrAlloc( fn, g.ray, hw, hh, false );
	ssrAlloc( fn, g.raw, hw, hh, true );
	ssrAlloc( fn, g.across, hw, hh, true );
	ssrAlloc( fn, g.fin, hw, hh, true );
	}
	/* lane PERF3 (cellssr.h): the flag from the obscurance pass's stencil and the color from the previous frame, when
	 * this frame can use them; else the scene pass as before */
	int prevWhy = 0;
	if ( ssrPrevOn() && s.on ) {
		// lane WATERREFL1: with the Water reflections row the water's rays march the previous frame too (no second scene)
		prevWhy = sceneOnly || wwCellSsgiWantsScene( scene ) ? 2 : waterSsr && !wwLookOn( WwLook::WaterRefl ) ? 3 : !g.prevValid ? 4
			: g.prevDoc != scene->nifModel ? 5 : ( g.prev.w != W || g.prev.h != H ) ? 6 : 1;
		if ( prevWhy != g.lastPrev ) {
			static const char * const words[7] = { "", "on", "scene pass (the bounce reads it)", "scene pass (water rays)",
				"scene pass (no previous frame)", "scene pass (another document)", "scene pass (size changed)" };
			std::fprintf( stderr, "cell ssr prev: %s\n", words[prevWhy] );
			g.lastPrev = prevWhy;
		}
	}
	const bool usePrev = prevWhy == 1;
	g.prevValid = false;	// used once: the end of this frame keeps the next
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	// lane SSGI2: the emitted share beside it (FO4CS's emission source), only while the bounce's emissive cast wants it
	const bool wantEmit = wwCellSsgiWantsEmission( scene );
	g.emitReady = false;
	if ( wantEmit && ( !g.emission.tex || g.emission.w != W || g.emission.h != H ) ) {
		if ( !g.emission.tex )
			fn->glGenTextures( 1, &g.emission.tex );
		fn->glBindTexture( GL_TEXTURE_2D, g.emission.tex );
		for ( int i = 0; i <= 4; i++ )
			fn->glTexImage2D( GL_TEXTURE_2D, i, GL_RGBA16F, std::max( W >> i, 1 ), std::max( H >> i, 1 ), 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4 );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		g.emission.w = W;
		g.emission.h = H;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.scene.fbo );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ao.depthRb );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, wantEmit ? g.emission.tex : 0, 0 );
	const GLenum sceneBufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	fn->glDrawBuffers( wantEmit ? 2 : 1, sceneBufs );
	const bool complete = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	// lane WATERREFL1: the twin (cell_ssrwater.prog) marches the water's rays further, edge fade only, unclamped
	const char * ssrProg = wwLookOn( WwLook::WaterRefl ) ? "cell_ssrwater.prog" : "cell_ssr.prog";
	NifSkopeOpenGLContext::Program * prog = complete ? r->useProgram( ssrProg ) : nullptr;
	if ( prog )
		r->stopProgram();
	if ( !prog ) {
		restore();
		return;
	}

	/* lane PERF2: the pass is CPU-bound per draw. memo: the scene-global setters once per program; opaque: the sorted
	 * draws that wrote nothing here last frame are skipped (PERF2FIX1), the water pass draws the water alone.
	 * =0 or *_RED=1: the old path */
	WW_SWITCH_STATIC( bool, memo, wwSwitchEcho( "ssr memo", qgetenv( "WW_CELL_SSR_MEMO" ) != "0" && !qEnvironmentVariableIntValue( "WW_CELL_SSR_MEMO_RED" ) ) );
	WW_SWITCH_STATIC( bool, opaqueOnly, qgetenv( "WW_CELL_SSR_OPAQUE" ) != "0" && !qEnvironmentVariableIntValue( "WW_CELL_SSR_OPAQUE_RED" ) );
	NodeList sceneSecond;
	const GLfloat none4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ( usePrev ) {
		// 1'. the flag alone: alpha 1 where the obscurance pass's stencil bit 0x01 says flagged
		fn->glViewport( 0, 0, W, H );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		fn->glClearBufferfv( GL_COLOR, 0, none4 );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_CULL_FACE );
		fn->glDisable( GL_BLEND );
		fn->glDepthMask( GL_FALSE );
		fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilMask( 0 );
		fn->glStencilFunc( GL_EQUAL, 1, 0x01 );
		prog = r->useProgram( ssrProg );
		prog->uni1i( "ssrStage", 0 );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
		fn->glDisable( GL_STENCIL_TEST );
		fn->glStencilMask( 0xFF );
		fn->glStencilFunc( GL_ALWAYS, 0, 0xFF );
		fn->glDepthMask( GL_TRUE );
	} else {
	// 1. the opaque cell-lit fragments once more: the lit color the game marches (probe 60)
	fn->glViewport( 0, 0, W, H );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glClearBufferfv( GL_COLOR, 0, none4 );
	if ( wantEmit )
		fn->glClearBufferfv( GL_COLOR, 1, none4 );
	/* lane PERF4: THE DEPTH KEPT. The obscurance pass just wrote this buffer from the same opaque draws (same programs,
	 * same masks, same cut), so it already holds the depth this pass would build. Testing against it (GL_LEQUAL, no
	 * write) shades only the fragment that ends visible, early-Z on, instead of every layer; the last fragment to pass
	 * is the same one, so the colour is the same. WW_CELL_SSR_ZREUSE=0: clear and build it as before.
	 * WW_CELL_SSR_ZREUSE_RED=1 (gates only): GL_LESS against the kept depth, every surface fails, the picture changes. */
	WW_SWITCH_STATIC( bool, zreuseOn, wwEnv( "WW_CELL_SSR_ZREUSE" ) != "0" );
	WW_SWITCH_STATIC( bool, zreuseRed, wwEnv( "WW_CELL_SSR_ZREUSE_RED" ) == "1" );
	s.zreuse = zreuseOn ? ( zreuseRed ? 2 : 1 ) : 0;
	{
		static int said = -1;
		if ( s.zreuse != said ) {
			said = s.zreuse;
			std::fprintf( stderr, "cell ssr: scene pass depth %s\n", s.zreuse == 0 ? "own (WW_CELL_SSR_ZREUSE=0)"
				: s.zreuse == 1 ? "kept from the obscurance pass (shades the visible layer only)" : "kept, RED less" );
		}
	}
	fn->glClear( s.zreuse ? GLbitfield( GL_STENCIL_BUFFER_BIT ) : GLbitfield( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ) );
	fn->glDisable( GL_BLEND );
	fn->glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );	// wwCellSsrDraw opens it for the opaque cell-lit draws
	s.pass = true;
	if ( memo )
		wwSetupMemoArm( true );
	scene->collectShapes( sceneSecond );
	if ( memo )
		wwSetupMemoArm( false );
	if ( opaqueOnly )
		wwOpaqueCutArm( true, "ssr" );
	Scene::drawDeferredShapes( sceneSecond );
	scene->drawShapeEffects();
	if ( opaqueOnly )
		wwOpaqueCutArm( false, "ssr" );
	s.pass = false;
	if ( s.zreuse == 2 )
		fn->glDepthFunc( GL_LEQUAL );
	s.zreuse = 0;
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );
	g.sceneReady = true;
	}
	g.doc = scene->nifModel;
	if ( wantEmit ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.scene.fbo );
		fn->glDrawBuffers( 1, sceneBufs );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0 );
		fn->glBindTexture( GL_TEXTURE_2D, g.emission.tex );
		fn->glGenerateMipmap( GL_TEXTURE_2D );	// box means: rgb the area's emission, a its coverage (FO4CS's pyramid)
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		g.emitReady = true;
	}
	if ( sceneOnly ) {
		restore();
		return;
	}

	// 1b. lane WATER2: the water's rays (the game's pass 02100), depth-tested against the opaque depth just drawn
	if ( waterSsr ) {
		ssrAlloc( fn, g.wray, W, H, false );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.wray.fbo );
		fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ao.depthRb );
		if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			fn->glViewport( 0, 0, W, H );
			fn->glClearBufferfv( GL_COLOR, 0, none4 );
			fn->glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );	// wwCellSsrDraw opens it for the water
			s.pass = true;
			s.waterRay = true;
			s.gbuf = ao.gbuf;
			if ( usePrev )	// lane WATERREFL1: the previous frame stood in for step 1, so nothing was collected
				scene->collectShapes( sceneSecond );
			// a particle inherits the masks of the draw before it (it may follow the water): then the whole old pass
			bool waterAlone = opaqueOnly;
			for ( Node * node : sceneSecond.list() )
				waterAlone = waterAlone && dynamic_cast<Shape *>( node ) != nullptr;
			if ( waterAlone ) {	// the sorted list's water, in its order (the opaque and the rest write nothing here)
				sceneSecond.alphaSort();
				for ( Node * node : sceneSecond.list() ) {
					Shape * sh = dynamic_cast<Shape *>( node );
					if ( sh && !sh->wwDecalDrawsFirst() && sh->wwWaterDraws() )
						node->drawShapes();
				}
			} else {
				NodeList second;
				scene->collectShapes( second );
				Scene::drawDeferredShapes( second );
			}
			s.pass = false;
			s.waterRay = false;
			g.haveWater = true;
			fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			fn->glDepthMask( GL_TRUE );
			fn->glDisable( GL_STENCIL_TEST );
			fn->glDisable( GL_POLYGON_OFFSET_FILL );
		}
	}

	// 2. the full-screen passes (cell_ssr.frag)
	prog = r->useProgram( ssrProg );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDepthMask( GL_FALSE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	const GLuint bound[9] = { ao.gbuf, ao.mip[1], ao.mip[2], ao.mip[3], ao.mip[4], g.scene.tex, 0,
		g.haveWater ? g.wray.tex : 0, usePrev ? g.prev.tex : 0 };
	const int nBound = usePrev ? 9 : 8;	// lane PERF3: unit 8 only for the previous frame
	for ( int u = 0; u < nBound; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, bound[u] );
	}
	prog->uni1i( "gbuf", 0 );
	prog->uni1i( "zMip1", 1 );
	prog->uni1i( "zMip2", 2 );
	prog->uni1i( "zMip3", 3 );
	prog->uni1i( "zMip4", 4 );
	prog->uni1i( "scene", 5 );
	prog->uni1i( "src", 6 );
	prog->uni1i( "waterRay", 7 );
	prog->uni1b( "haveWaterRay", g.haveWater );
	prog->uni1b( "opaqueRays", !g.waterOnly );
	fn->glUniform2i( prog->uniLocation( "fullSize" ), W, H );
	fn->glUniform2i( prog->uniLocation( "halfSize" ), hw, hh );
	fn->glUniform2i( prog->uniLocation( "mipSize[%d]", 0 ), W, H );
	for ( int m = 1; m < 5; m++ )
		fn->glUniform2i( prog->uniLocation( "mipSize[%d]", m ), ao.mipW[m], ao.mipH[m] );
	const auto & pm = r->globalUniforms->projectionMatrix;
	const float p00 = pm[0][0], p11 = pm[1][1];
	const float farZ = std::min( kSsrFarMax, L->clipDist > 0.0f ? L->clipDist : kSsrFarMax );
	// world = R^T x view: column j of the uniform is the view axis j in the world
	const Transform & vt = scene->view;
	float rot[9];
	for ( int j = 0; j < 3; j++ )
		for ( int k = 0; k < 3; k++ )
			rot[j * 3 + k] = vt.rotation( j, k );
	fn->glUniformMatrix3fv( prog->uniLocation( "viewToWorld" ), 1, GL_FALSE, rot );
	prog->uni2f( "proj", p00, p11 );
	prog->uni4f( "ssrK", FloatVector4( kSsrK[0], kSsrK[1], kSsrK[2], kSsrK[3] ) );
	prog->uni2f( "ssrClip", kSsrNear, farZ );
	prog->uni1i( "ssrRed", s.red );
	prog->uni1b( "ssrPrev", usePrev );
	if ( usePrev ) {
		/* game-unit view P = R world + t / sc (cellAoPassOut's depth); the previous view: M P + T with
		 * M = Rp R^T, T = tp / scp - M t / sc */
		const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
		float M[9], T[3];
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ ) {
				float m = 0.0f;
				for ( int k = 0; k < 3; k++ )
					m += g.prevRot[i * 3 + k] * vt.rotation( j, k );
				M[i * 3 + j] = m;
			}
		for ( int i = 0; i < 3; i++ ) {
			float mt = 0.0f;
			for ( int j = 0; j < 3; j++ )
				mt += M[i * 3 + j] * vt.translation[j];
			T[i] = g.prevT[i] / g.prevSc - mt / sc;
		}
		fn->glUniformMatrix3fv( prog->uniLocation( "ssrPrevRot" ), 1, GL_TRUE, M );	// M is row-major
		prog->uni3f( "ssrPrevT", T[0], T[1], T[2] );
		prog->uni2f( "ssrPrevProj", g.prevP00, g.prevP11 );
		prog->uni1i( "prevScene", 8 );
	}
	auto pass = [&]( int stage, const SsrTarget & out, const SsrTarget * in ) {
		fn->glActiveTexture( GL_TEXTURE6 );
		fn->glBindTexture( GL_TEXTURE_2D, in ? in->tex : 0 );
		prog->uni1i( "ssrStage", stage );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, out.fbo );
		fn->glViewport( 0, 0, out.w, out.h );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	};
	pass( 1, g.ray, nullptr );
	pass( 2, g.raw, &g.ray );
	pass( 3, g.across, &g.raw );
	pass( 4, g.fin, &g.across );
	r->stopProgram();
	for ( int u = 0; u < nBound; u++ ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );

	g.ready = true;
	g.doc = scene->nifModel;

	// WW_CELL_SSR_DUMP=<file>: int32 W H hw hh; the obscurance pass's normal + depth and the scene pass (W x H
	// RGBA float, bottom row first); the ray, the march and the blurred result (hw x hh RGBA float, top row
	// first); <file>.txt the numbers
	if ( !s.dump.isEmpty() ) {
		std::vector<float> full( size_t( W ) * size_t( H ) * 4 ), half( size_t( hw ) * size_t( hh ) * 4 );
		QFile f( s.dump );
		const bool open = f.open( QIODevice::WriteOnly );
		const qint32 hd[4] = { W, H, hw, hh };
		if ( open )
			f.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		GLuint gbFbo = 0;
		fn->glGenFramebuffers( 1, &gbFbo );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, gbFbo );
		fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ao.gbuf, 0 );
		fn->glReadPixels( 0, 0, W, H, GL_RGBA, GL_FLOAT, full.data() );
		if ( open )
			f.write( reinterpret_cast<const char *>( full.data() ), qint64( full.size() * sizeof( float ) ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.scene.fbo );
		fn->glReadPixels( 0, 0, W, H, GL_RGBA, GL_FLOAT, full.data() );
		if ( open )
			f.write( reinterpret_cast<const char *>( full.data() ), qint64( full.size() * sizeof( float ) ) );
		double sum = 0.0;
		for ( const SsrTarget * t : { &g.ray, &g.raw, &g.fin } ) {
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, t->fbo );
			fn->glReadPixels( 0, 0, hw, hh, GL_RGBA, GL_FLOAT, half.data() );
			if ( open )
				f.write( reinterpret_cast<const char *>( half.data() ), qint64( half.size() * sizeof( float ) ) );
			if ( t == &g.fin )
				for ( size_t i = 3; i < half.size(); i += 4 )
					sum += double( half[i] );
		}
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		fn->glDeleteFramebuffers( 1, &gbFbo );
		QFile t( s.dump + QStringLiteral( ".txt" ) );
		if ( t.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QString o = QStringLiteral( "ssr: on %1x%2 half %3x%4 near %5 far %6 k %7 %8 %9 %10 p00 %11 p11 %12 unit %13 red %14"
				" water %15 waterOnly %16 prev %17" )
				.arg( W ).arg( H ).arg( hw ).arg( hh ).arg( double( kSsrNear ), 0, 'f', 2 ).arg( double( farZ ), 0, 'f', 2 )
				.arg( double( kSsrK[0] ) ).arg( double( kSsrK[1] ) ).arg( double( kSsrK[2] ) ).arg( double( kSsrK[3] ) )
				.arg( double( p00 ), 0, 'f', 6 ).arg( double( p11 ), 0, 'f', 6 ).arg( g.unit ).arg( s.red )
				.arg( g.haveWater ? 1 : 0 ).arg( g.waterOnly ? 1 : 0 ).arg( usePrev ? 1 : 0 );
			o += QStringLiteral( " rot" );
			for ( float v : rot )
				o += QStringLiteral( " %1" ).arg( double( v ), 0, 'f', 7 );
			o += QStringLiteral( " confidence %1 ms %2\n" ).arg( sum / double( std::max( hw * hh, 1 ) ), 0, 'f', 6 )
				.arg( timer.elapsed() );
			t.write( o.toUtf8() );
		}
	}

	restore();
}

void wwCellSsrDraw( Scene * scene, bool cellProgram )
{
	PassTime::DrawCpuScope cpuT( PassTime::CpuSsrDraw );	// lane PERF2
	if ( !scene || !scene->renderer )
		return;
	SsrState & s = ssr();
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	cellProgram = cellProgram && ssrIsCellProgram( prog );
	if ( s.pass && s.waterRay ) {
		// lane WATER2: the water ray pass: the water alone writes, behind the opaque depth, writing no depth
		const bool water = prog && wwIsCellWaterProgramName( prog->name );
		glColorMask( water, water, water, water );
		glDepthMask( GL_FALSE );
		if ( water ) {
			glDisable( GL_BLEND );
			wwDrawGlCacheDirty();	// lane PERF3
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
		}
		return;
	}
	if ( s.pass ) {
		// the game's opaque (deferred) pass: no blended draw, no effect, nothing that is not cell-lit
		const bool opaque = cellProgram && !wwDrawBlend();
		glColorMask( opaque, opaque, opaque, opaque );
		if ( !opaque || s.zreuse )
			glDepthMask( GL_FALSE );	// lane PERF4: with the kept depth no draw writes it (it is already final)
		if ( opaque && s.zreuse == 2 )
			glDepthFunc( GL_LESS );
		wwOpaqueCutSaw( opaque );	// lane PERF2FIX1 (the cut's check)
		if ( cellProgram ) {
			prog->uni1i( "cellProbe", 60 );
			prog->uni1b( "cellAoOn", false );
			prog->uni1b( "cellSsrOn", false );
			prog->uni1b( "cellSsrMat", s.flagged );
		}
		return;
	}
	if ( !cellProgram || prog->uniLocation( "cellSsrOn" ) < 0 )
		return;
	SsrGpu & g = ssrGpus()[r];
	const bool on = g.ready && g.doc == scene->nifModel && g.unit > 0 && !( s.red & 1 ) && s.flagged && !g.waterOnly
		&& !wwDrawBlend() && wwCellLightsWanted( scene ) && ( s.probe == 0 || s.probe == 61 );
	prog->uni1b( "cellSsrOn", on );
	prog->uni1b( "cellSsrMat", s.flagged );
	if ( g.unit <= 0 )
		return;
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	WwCellUni::Units<std::remove_pointer_t<decltype( r->fn )>> u( r->fn, wwCellUniDsa() );	// lane CELLUNI2: no query
	if ( !u.dsa )
		u.seed( wwDrawActiveTexture() );	// lane PERF3
	u.bind( g.unit, GL_TEXTURE_2D, on ? g.fin.tex : 0 );
	u.done();
	prog->uni1i( "cellSsr", g.unit );
	if ( on ) {
		int vp[4];
		wwDrawViewport( vp );
		prog->uni4f_l( prog->uniLocation( "cellSsrRect" ), FloatVector4( float( vp[0] ), float( vp[1] ),
			1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) ) );
	}
}

bool wwCellSsrWaterRayPass( unsigned int * gbufTex )
{
	const SsrState & s = ssr();
	if ( gbufTex )
		*gbufTex = s.waterRay ? s.gbuf : 0;
	return s.pass && s.waterRay;
}

bool wwCellSsrInPass()
{
	return ssr().pass;
}

bool wwCellSsrSceneTexture( Scene * scene, unsigned int & tex )
{
	tex = 0;
	if ( !scene || !scene->renderer )
		return false;
	const auto it = ssrGpus().constFind( scene->renderer );
	if ( it == ssrGpus().constEnd() || !it->sceneReady || it->doc != scene->nifModel )
		return false;
	tex = it->scene.tex;
	return tex != 0;
}

bool wwCellSsrEmissionTexture( Scene * scene, unsigned int & tex )
{
	tex = 0;
	if ( !scene || !scene->renderer )
		return false;
	const auto it = ssrGpus().constFind( scene->renderer );
	if ( it == ssrGpus().constEnd() || !it->sceneReady || !it->emitReady || it->doc != scene->nifModel )
		return false;
	tex = it->emission.tex;
	return tex != 0;
}

bool wwCellSsrWaterTextures( Scene * scene, unsigned int & raw, unsigned int & fin )
{
	raw = fin = 0;
	if ( !scene || !scene->renderer )
		return false;
	const auto it = ssrGpus().constFind( scene->renderer );
	if ( it == ssrGpus().constEnd() || !it->ready || !it->haveWater || it->doc != scene->nifModel )
		return false;
	raw = it->raw.tex;
	fin = it->fin.tex;
	return raw && fin;
}
