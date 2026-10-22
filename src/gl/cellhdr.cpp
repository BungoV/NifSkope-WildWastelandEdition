#include "cellhdr.h"

#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QDebug>
#include <QHash>

#include <algorithm>
#include <cstdint>

// lane HDR1: the cell view's linear frame and its one tone map (cellhdr.h)

namespace {

constexpr int kSrcUnit = 8;		// the linear frame (sampler2DMS) in cell_hdr.prog; the cell units are 10..16

struct HdrGpu
{
	GLuint fbo = 0, color = 0, ds = 0;
	int w = 0, h = 0, samples = 0;
	GLenum dsFormat = 0;
};

struct HdrState
{
	bool active = false;
	Scene * scene = nullptr;
	GLint prevDraw = 0, prevRead = 0;
	int w = 0, h = 0, samples = 0;
	bool red = qgetenv( "WW_CELL_HDR_RED" ) == "perfrag";
	QString last = QStringLiteral( "off" );
	QString said;
};

HdrState & hst()
{
	static HdrState s;
	return s;
}

QHash<Renderer *, HdrGpu> & hdrGpus()
{
	static QHash<Renderer *, HdrGpu> g;
	return g;
}

void note( HdrState & s, const QString & line )
{
	s.last = line;
	if ( s.said != line ) {	// the notes get a line when it changes, not once a frame
		s.said = line;
		qInfo().noquote() << "cell hdr:" << line;
	}
}

}	// namespace

bool wwCellHdrActive()
{
	return hst().active;
}

QString wwCellHdrEcho()
{
	return hst().last;
}

bool wwCellHdrBegin( Scene * scene )
{
	HdrState & s = hst();
	s.active = false;
	s.scene = nullptr;
	if ( !scene || !scene->renderer || scene->selecting || !wwCellImageSpaceWanted( scene ) || wwCellProbePass( scene ) )
		return false;
	if ( s.red ) {
		note( s, QStringLiteral( "off (red perfrag: every fragment tone-mapped as drawn)" ) );
		return false;
	}
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	if ( !r->useProgram( "cell_hdr.prog" ) ) {
		note( s, QStringLiteral( "refused (no cell_hdr.prog)" ) );
		return false;
	}
	r->stopProgram();

	GLint vp[4] = { 0, 0, 1, 1 }, samples = 0;
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &s.prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &s.prevRead );
	fn->glGetIntegerv( GL_SAMPLES, &samples );
	// the frame's depth format: depth and stencil go back by a blit, which needs the same format both sides
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	const GLenum att = s.prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, s.prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
		GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
	GLenum dsFormat = 0;
	if ( stencilBits == 8 && depthBits == 24 && compType != GL_FLOAT )
		dsFormat = GL_DEPTH24_STENCIL8;
	else if ( stencilBits == 8 && depthBits == 32 && compType == GL_FLOAT )
		dsFormat = GL_DEPTH32F_STENCIL8;
	if ( !dsFormat ) {
		note( s, QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil: no stencil to sort the pixels by)" )
			.arg( depthBits ).arg( stencilBits ) );
		return false;
	}
	const int W = vp[0] + vp[2], H = vp[1] + vp[3], S = std::max( int( samples ), 1 );
	HdrGpu & g = hdrGpus()[r];
	if ( !g.fbo || g.w != W || g.h != H || g.samples != S || g.dsFormat != dsFormat ) {
		if ( !g.fbo )
			fn->glGenFramebuffers( 1, &g.fbo );
		if ( g.color )
			fn->glDeleteTextures( 1, &g.color );
		fn->glGenTextures( 1, &g.color );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.color );
		/* finalfix (SUNCELL1): the linear frame is RGBA32F, not RGBA16F. Multisampled additive blending into fp16
		 * (the blended ground layers, SRC_ALPHA/ONE) rounded +-1 step differently run to run on the same inputs
		 * (every draw's state, uniforms and textures traced identical); fp32 measured 0 of 7 frames differing. */
		fn->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, S, GL_RGBA32F, W, H, GL_TRUE );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		if ( !g.ds )
			fn->glGenRenderbuffers( 1, &g.ds );
		fn->glBindRenderbuffer( GL_RENDERBUFFER, g.ds );
		fn->glRenderbufferStorageMultisample( GL_RENDERBUFFER, S, dsFormat, W, H );
		fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, g.color, 0 );
		fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.ds );
		g.w = W;
		g.h = H;
		g.samples = S;
		g.dsFormat = dsFormat;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );
		note( s, QStringLiteral( "refused (the linear frame is incomplete)" ) );
		return false;
	}
	// a clean target: no colour (pixels no draw reaches are not written back), the depth / stencil clears
	GLfloat clear[4];
	fn->glGetFloatv( GL_COLOR_CLEAR_VALUE, clear );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDepthMask( GL_TRUE );
	fn->glStencilMask( 0xFF );
	fn->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	fn->glClearStencil( 0 );
	fn->glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	fn->glClearColor( clear[0], clear[1], clear[2], clear[3] );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	if ( wasScissor )
		fn->glEnable( GL_SCISSOR_TEST );
	s.active = true;
	s.scene = scene;
	s.w = W;
	s.h = H;
	s.samples = S;
	return true;
}

void wwCellHdrEnd( Scene * scene )
{
	HdrState & s = hst();
	if ( !s.active || s.scene != scene || !scene || !scene->renderer )
		return;
	s.active = false;
	s.scene = nullptr;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	HdrGpu & g = hdrGpus()[r];
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	// depth and stencil back to the frame: later draws test against the scene, the stencil sorts the pixels
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
	fn->glBlitFramebuffer( 0, 0, s.w, s.h, 0, 0, s.w, s.h, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST );
	const GLenum blitErr = fn->glGetError();
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );

	auto prog = r->useProgram( "cell_hdr.prog" );
	if ( !prog ) {
		note( s, QStringLiteral( "lost (cell_hdr.prog went away mid-frame)" ) );
		return;
	}
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	wwCellLightsUniforms( scene );	// the imagespace's uniforms (exposure, curve, grade, LUT, bloom)
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.color );
	prog->uni1i( "hdrSrc", kSrcUnit );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const bool sorted = blitErr == GL_NO_ERROR;
	fn->glStencilMask( 0 );
	if ( sorted ) {
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		fn->glStencilFunc( GL_EQUAL, 1, 0x03 );	// a cell program or a cell effect wrote it last: the imagespace
	} else {
		fn->glDisable( GL_STENCIL_TEST );
	}
	prog->uni1i( "hdrMode", 0 );
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	if ( sorted ) {
		fn->glStencilFunc( GL_EQUAL, 2, 0x03 );	// any other program: its value as written
		prog->uni1i( "hdrMode", 1 );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	}
	r->stopProgram();

	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glStencilMask( 0xFF );
	fn->glStencilFunc( GL_ALWAYS, 0, 0xFF );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );
	note( s, sorted ? QStringLiteral( "linear frame %1x%2, %3 sample(s), one tone map" ).arg( s.w ).arg( s.h ).arg( s.samples )
		: QStringLiteral( "linear frame %1x%2, %3 sample(s), one tone map over every pixel (depth / stencil blit refused, GL error %4)" )
			.arg( s.w ).arg( s.h ).arg( s.samples ).arg( blitErr ) );
}
