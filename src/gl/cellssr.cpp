/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellssr.h"

#include "gl/celllights.h"
#include "gl/cellwater.h"
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

constexpr int kSsrUnit = 17;			// the reflection (sampler2D, half the view); 10..16 are the cell lights' and AO's
// the game's: color scale, angle gate, normal z scale, confidence scale; its camera's near; its far ceiling
constexpr float kSsrK[4] = { 1.0f, 0.2f, 2.0f, 1.0f };
constexpr float kSsrNear = 15.0f, kSsrFarMax = 353840.0f;

struct SsrState
{
	bool loaded = false;
	int red = 0;			// 1 off (computed, not applied), 2 nogap, 4 nofade
	int probe = 0;			// WW_CELL_LIT_PROBE: another lane's probe reads its own term, without the reflection
	QString dump;			// WW_CELL_SSR_DUMP
	bool pass = false;		// the scene pass is drawing
	bool waterRay = false;	// lane WATER2: the water ray pass is drawing
	unsigned int gbuf = 0;
	bool flagged = false;	// the draw being set up carries the flag
	bool on = true, ext = false, pinOn = false, pinExt = false;	// lane CELLALL1: the rows (cellssr.h)
	int lastVerdict = -1;	// lane CELLALL1: the telemetry line prints when it changes
};

SsrState & ssr()
{
	static SsrState s;
	if ( !s.loaded ) {
		s.loaded = true;
		const QByteArray red = qgetenv( "WW_CELL_SSR_RED" ).trimmed();
		s.red = red == "off" ? 1 : red == "nogap" ? 2 : red == "nofade" ? 4 : 0;
		s.dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_SSR_DUMP" ) );
		s.probe = qEnvironmentVariableIntValue( "WW_CELL_LIT_PROBE" );
		const QByteArray pinOn = qgetenv( "WW_CELL_SSR" ).trimmed(), pinExt = qgetenv( "WW_CELL_SSR_EXT" ).trimmed();
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
	const void * doc = nullptr;
	bool ready = false;
	bool waterOnly = false;		// an exterior: the pass ran for the water alone
	bool haveWater = false;		// the water ray pass drew
	int unit = -1;
};

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

void wwCellSsrNote( bool flagged )
{
	ssr().flagged = flagged;
}

void wwCellSsrPass( Scene * scene, bool run )
{
	if ( !scene || !scene->renderer )
		return;
	SsrState & s = ssr();
	Renderer * r = scene->renderer;
	SsrGpu & g = ssrGpus()[r];
	g.ready = false;
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
	if ( verdict != 4 && verdict != 5 )
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
	ssrAlloc( fn, g.ray, hw, hh, false );
	ssrAlloc( fn, g.raw, hw, hh, true );
	ssrAlloc( fn, g.across, hw, hh, true );
	ssrAlloc( fn, g.fin, hw, hh, true );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.scene.fbo );
	fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ao.depthRb );
	const bool complete = fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	NifSkopeOpenGLContext::Program * prog = complete ? r->useProgram( "cell_ssr.prog" ) : nullptr;
	if ( prog )
		r->stopProgram();
	if ( !prog ) {
		restore();
		return;
	}

	// 1. the opaque cell-lit fragments once more: the lit color the game marches (probe 60)
	fn->glViewport( 0, 0, W, H );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	const GLfloat none4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	fn->glClearBufferfv( GL_COLOR, 0, none4 );
	fn->glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	fn->glDisable( GL_BLEND );
	fn->glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );	// wwCellSsrDraw opens it for the opaque cell-lit draws
	s.pass = true;
	{
		NodeList second;
		scene->collectShapes( second );
		Scene::drawDeferredShapes( second );
		scene->drawShapeEffects();
	}
	s.pass = false;
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_POLYGON_OFFSET_FILL );

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
			{
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
	prog = r->useProgram( "cell_ssr.prog" );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_BLEND );
	fn->glDepthMask( GL_FALSE );
	fn->glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	const GLuint bound[8] = { ao.gbuf, ao.mip[1], ao.mip[2], ao.mip[3], ao.mip[4], g.scene.tex, 0,
		g.haveWater ? g.wray.tex : 0 };
	for ( int u = 0; u < 8; u++ ) {
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
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
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
	for ( int u = 0; u < 8; u++ ) {
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
				" water %15 waterOnly %16" )
				.arg( W ).arg( H ).arg( hw ).arg( hh ).arg( double( kSsrNear ), 0, 'f', 2 ).arg( double( farZ ), 0, 'f', 2 )
				.arg( double( kSsrK[0] ) ).arg( double( kSsrK[1] ) ).arg( double( kSsrK[2] ) ).arg( double( kSsrK[3] ) )
				.arg( double( p00 ), 0, 'f', 6 ).arg( double( p11 ), 0, 'f', 6 ).arg( g.unit ).arg( s.red )
				.arg( g.haveWater ? 1 : 0 ).arg( g.waterOnly ? 1 : 0 );
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
	if ( !scene || !scene->renderer )
		return;
	SsrState & s = ssr();
	Renderer * r = scene->renderer;
	NifSkopeOpenGLContext::Program * prog = r->getCurrentProgram();
	cellProgram = cellProgram && ssrIsCellProgram( prog );
	if ( s.pass && s.waterRay ) {
		// lane WATER2: the water ray pass: the water alone writes, behind the opaque depth, writing no depth
		const bool water = prog && prog->name == std::string_view( "fo4_water.prog" );
		glColorMask( water, water, water, water );
		glDepthMask( GL_FALSE );
		if ( water ) {
			glDisable( GL_BLEND );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
		}
		return;
	}
	if ( s.pass ) {
		// the game's opaque (deferred) pass: no blended draw, no effect, nothing that is not cell-lit
		const bool opaque = cellProgram && !glIsEnabled( GL_BLEND );
		glColorMask( opaque, opaque, opaque, opaque );
		if ( !opaque )
			glDepthMask( GL_FALSE );
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
		&& !glIsEnabled( GL_BLEND ) && wwCellLightsWanted( scene ) && ( s.probe == 0 || s.probe == 61 );
	prog->uni1b( "cellSsrOn", on );
	prog->uni1b( "cellSsrMat", s.flagged );
	if ( g.unit <= 0 )
		return;
	// bound whether or not this draw reads it (a sampler left on unit 0 would sit beside BaseMap)
	GLint prevActive = 0;
	r->fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	r->fn->glActiveTexture( GLenum( GL_TEXTURE0 + g.unit ) );
	r->fn->glBindTexture( GL_TEXTURE_2D, on ? g.fin.tex : 0 );
	r->fn->glActiveTexture( GLenum( prevActive ) );
	prog->uni1i( "cellSsr", g.unit );
	if ( on ) {
		GLint vp[4] = { 0, 0, 1, 1 };
		glGetIntegerv( GL_VIEWPORT, vp );
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
