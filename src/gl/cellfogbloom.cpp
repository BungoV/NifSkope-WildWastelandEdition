#include "cellfogbloom.h"
#include "wwswitch.h"	// lane LIVESET1: the pin and reds follow a live set

#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDebug>
#include <QHash>
#include <QSettings>

#include <cstdint>

// lane FOGBLOOM1: the bloom-intensity target (cellfogbloom.h)

namespace {

constexpr int kDepthUnit = 1;	// the resolved depth (its own program); off 0, where the air's sampler3D falls when its units are refused
constexpr const char * kKey = "WW/CellFogBloom";
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
// k (at most twice the bloom in opaque fog), b (a translucent takes 1.5x the fog term), the mean extinction from
// mist (V 2 km) to fog (V 1 km), Koschmieder sigma = 3.912 / V, 1 game unit = 1.4287 cm
constexpr float kStrength = 1.0f, kTranslucent = 0.5f;
constexpr float kSigmaMist = 3.912f / 139986.0f, kSigmaFog = 3.912f / 69993.0f;

struct FbGpu
{
	GLuint fbo = 0, depthFbo = 0, depth = 0, target = 0;
	int w = 0, h = 0, samples = 0;
	GLenum dsFormat = 0;
	GLuint q[4] = { 0, 0, 0, 0 };	// two timestamp pairs, read a frame late
	bool pending[2] = { false, false };
	int cur = 0;
};

struct FbState
{
	bool on = false, pinned = false, red = false, time = false;
	quint64 liveGen = 0;	// lane LIVESET1
	bool ran = false;
	GLuint target = 0;
	double ms = -1.0;
	int frames = 0;
	QString last = QStringLiteral( "off" ), said;
};

FbState & fs()
{
	static FbState s;
	static bool init = false;
	// lane LIVESET1: a live set of WW_CELL_FOGBLOOM* reads the switches again (the targets are per renderer)
	if ( !init || wwSwitchMovedFor( s.liveGen, { "WW_CELL_FOGBLOOM" } ) ) {
		init = true;
		const QByteArray pin = qgetenv( "WW_CELL_FOGBLOOM" ).trimmed();
		s.pinned = pin == "0" || pin == "1";
		// a harness reads only the pin (off otherwise), never what the window saved
		s.on = s.pinned ? pin == "1" : !wwHarnessRun() && QSettings().value( QLatin1StringView( kKey ), false ).toBool();
		s.red = qgetenv( "WW_CELL_FOGBLOOM_RED" ).trimmed() == "flat";
		s.time = qgetenv( "WW_CELL_FOGBLOOM_TIME" ).trimmed() == "1";
	}
	return s;
}

QHash<Renderer *, FbGpu> & gpus()
{
	static QHash<Renderer *, FbGpu> g;
	return g;
}

void note( FbState & s, const QString & line )
{
	s.last = line;
	if ( line != s.said ) {
		s.said = line;
		qInfo().noquote() << "cell fog bloom:" << line;
	}
}

void timeRead( NifSkopeOpenGLContext::GLFunctions * fn, FbGpu & g, FbState & s, int slot, bool wait )
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
	s.ms = b > a ? double( b - a ) * 1e-6 : 0.0;
	g.pending[slot] = false;
	if ( ++s.frames % 60 == 1 )
		qInfo().noquote() << QStringLiteral( "cell fog bloom: gpu %1 ms" ).arg( s.ms, 0, 'f', 4 );
}

}

bool wwCellFogBloomOn()
{
	return fs().on;
}

void wwCellFogBloomSetOn( bool on )
{
	FbState & s = fs();
	if ( s.pinned )
		return;
	s.on = on;
	QSettings().setValue( QLatin1StringView( kKey ), on );
}

bool wwCellFogBloomPinned()
{
	return fs().pinned;
}

bool wwCellFogBloomMarks()
{
	const FbState & s = fs();
	return s.on && !s.red;
}

bool wwCellFogBloomRun( Scene * scene, unsigned hdrFbo, unsigned ds, int W, int H, int S, unsigned dsFormat )
{
	FbState & s = fs();
	s.ran = false;
	s.target = 0;
	if ( !s.on ) {
		note( s, QStringLiteral( "off" ) );
		return false;
	}
	if ( s.red ) {
		note( s, QStringLiteral( "RED flat: the uniform bloom" ) );
		return false;
	}
	if ( !scene || !scene->renderer || !hdrFbo || !ds || W <= 0 || H <= 0 )
		return false;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	FbGpu & g = gpus()[r];
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	if ( s.time ) {
		if ( !g.q[0] )
			fn->glGenQueries( 4, g.q );
		timeRead( fn, g, s, 1 - g.cur, false );
		timeRead( fn, g, s, g.cur, true );
		fn->glQueryCounter( g.q[g.cur * 2], kTimestamp );
	}
	if ( !g.fbo || g.w != W || g.h != H || g.samples != S || g.dsFormat != GLenum( dsFormat ) ) {
		if ( !g.fbo ) {
			fn->glGenFramebuffers( 1, &g.fbo );
			fn->glGenFramebuffers( 1, &g.depthFbo );
		}
		if ( g.depth )
			fn->glDeleteTextures( 1, &g.depth );
		if ( g.target )
			fn->glDeleteTextures( 1, &g.target );
		// the depth resolve: a blit needs the frame's own depth format on both sides
		fn->glGenTextures( 1, &g.depth );
		fn->glBindTexture( GL_TEXTURE_2D, g.depth );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( dsFormat ), W, H, 0, GL_DEPTH_STENCIL,
			dsFormat == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.depthFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, g.depth, 0 );
		// the target: R8 at the frame's sample count, beside the frame's own stencil (the translucents' marks)
		fn->glGenTextures( 1, &g.target );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.target );
		fn->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, S, GL_R8, W, H, GL_TRUE );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		g.w = W;
		g.h = H;
		g.samples = S;
		g.dsFormat = GLenum( dsFormat );
	}
	GLint prevDraw = 0, prevRead = 0, prevActive = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLfloat clear[4];
	fn->glGetFloatv( GL_COLOR_CLEAR_VALUE, clear );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	GLint stFunc = GL_ALWAYS, stRef = 0, stValMask = 0xFF, stWriteMask = 0xFF, stFail = GL_KEEP, stZFail = GL_KEEP, stZPass = GL_KEEP;
	fn->glGetIntegerv( GL_STENCIL_FUNC, &stFunc );
	fn->glGetIntegerv( GL_STENCIL_REF, &stRef );
	fn->glGetIntegerv( GL_STENCIL_VALUE_MASK, &stValMask );
	fn->glGetIntegerv( GL_STENCIL_WRITEMASK, &stWriteMask );
	fn->glGetIntegerv( GL_STENCIL_FAIL, &stFail );
	fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &stZFail );
	fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &stZPass );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );

	bool ok = true;
	QString why;
	// 1. the depth, samples nearest-resolved
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( hdrFbo ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.depthFbo );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	if ( const GLenum e = fn->glGetError(); e != GL_NO_ERROR ) {
		ok = false;
		why = QStringLiteral( "the depth resolve failed, GL error %1" ).arg( e );
	}
	// 2. the target on the frame's stencil
	const GLenum one = GL_COLOR_ATTACHMENT0;
	if ( ok ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, g.target, 0 );
		fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, GLuint( ds ) );
		fn->glDrawBuffers( 1, &one );
		if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			ok = false;
			why = QStringLiteral( "the target is incomplete" );
		}
	}
	if ( ok ) {
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_CULL_FACE );
		fn->glDepthMask( GL_FALSE );
		fn->glStencilMask( 0 );
		fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		fn->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		fn->glClear( GL_COLOR_BUFFER_BIT );
		auto prog = r->useProgram( "cell_fogbloom.prog" );
		if ( !prog ) {
			ok = false;
			why = QStringLiteral( "no cell_fogbloom.prog" );
		} else {
			wwLookdevFogUniforms( scene );	// the fog and the air (volTex, volRect from the scene's viewport)
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kDepthUnit ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.depth );
			prog->uni1i( "fbDepth", kDepthUnit );
			const auto & pm = r->globalUniforms->projectionMatrix;
			prog->uni2f( "fbProj", pm[0][0], pm[1][1] );
			prog->uni2f( "fbProjZ", pm[2][2], pm[3][2] );
			prog->uni4f( "fbParams", FloatVector4( kStrength, kTranslucent, kSigmaMist, kSigmaFog ) );
			static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
			static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
			const float * attrs = quad;
			fn->glDisable( GL_STENCIL_TEST );
			prog->uni1i( "fbTranslucent", 0 );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			// the samples a blended linear draw (an effect, glass) wrote last: the same air, raised
			fn->glEnable( GL_STENCIL_TEST );
			fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
			fn->glStencilFunc( GL_EQUAL, 0x04, 0x04 );
			prog->uni1i( "fbTranslucent", 1 );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kDepthUnit ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
		}
		fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0 );
		if ( const GLenum e = fn->glGetError(); ok && e != GL_NO_ERROR ) {
			ok = false;
			why = QStringLiteral( "the pass failed, GL error %1" ).arg( e );
		}
	}
	// restore
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glClearColor( clear[0], clear[1], clear[2], clear[3] );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glStencilFunc( GLenum( stFunc ), stRef, GLuint( stValMask ) );
	fn->glStencilMask( GLuint( stWriteMask ) );
	fn->glStencilOp( GLenum( stFail ), GLenum( stZFail ), GLenum( stZPass ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	if ( s.time ) {
		fn->glQueryCounter( g.q[g.cur * 2 + 1], kTimestamp );
		g.pending[g.cur] = true;
		g.cur = 1 - g.cur;
	}
	if ( !ok ) {
		note( s, QStringLiteral( "refused (%1): the uniform bloom" ).arg( why ) );
		return false;
	}
	s.ran = true;
	s.target = g.target;
	note( s, QStringLiteral( "on %1x%2 x%3 k=%4 translucent=%5 sigma=%6..%7" ).arg( W ).arg( H ).arg( S )
		.arg( double( kStrength ) ).arg( double( kTranslucent ) ).arg( double( kSigmaMist ), 0, 'g', 4 ).arg( double( kSigmaFog ), 0, 'g', 4 ) );
	return true;
}

void wwCellFogBloomFinal( Renderer * r, int unit, bool ran )
{
	FbState & s = fs();
	auto prog = r ? r->getCurrentProgram() : nullptr;
	if ( !prog )
		return;
	const bool on = ran && s.ran && s.target;
	prog->uni1i( "cellFogBloom", unit );	// always its own unit: a sampler2DMS, never left on 0 beside another type
	prog->uni1b( "cellFogBloomOn", on );
	auto fn = r->fn;
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, on ? s.target : 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
}

void wwCellFogBloomDone( Renderer * r, int unit )
{
	FbState & s = fs();
	if ( r ) {
		auto fn = r->fn;
		GLint prevActive = 0;
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		fn->glActiveTexture( GLenum( prevActive ) );
	}
	s.ran = false;
	s.target = 0;
}

QString wwCellFogBloomEcho()
{
	const FbState & s = fs();
	return s.ms >= 0.0 ? QStringLiteral( "%1 gpu=%2ms" ).arg( s.last ).arg( s.ms, 0, 'f', 4 ) : s.last;
}
