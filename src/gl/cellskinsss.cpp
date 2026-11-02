#include "gl/cellskinsss.h"
#include "gl/wwskinsss.h"
#include "gl/wwskinssshalf.h"
#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "wwswitch.h"

#include <QDebug>
#include <QHash>
#include <QString>

#include <cstdint>

bool wwSkinSssShaderDefine();	// src/gl/celllooks.cpp (the Skin subsurface row / WW_CELL_SKINSSS)
GLuint wwCellSkinSssTag( Scene * scene, int & w, int & h );	// src/gl/celllights.cpp (the opaque pass's attachment 1)

namespace {

constexpr int kUnitSrc = 12, kUnitLit = 13, kUnitDepth = 14, kUnitSkin = 15;

struct SssGpu
{
	GLuint inFbo = 0, fbo = 0, cur = 0, depth = 0, ext = 0, bx = 0;
	GLuint hext = 0, hbx = 0, hby = 0, hdep = 0;	// lane SKINSSS2: the half-resolution targets (+ the half depth)
	int w = 0, h = 0;
	GLenum colFormat = 0, dsFormat = 0;
};

QHash<const void *, SssGpu> & gpus()
{
	static QHash<const void *, SssGpu> g;
	return g;
}

void note( const QString & s )
{
	static QString last;
	if ( s != last ) {
		last = s;
		qInfo().noquote() << "cell skinsss:" << s;
	}
}

GLuint makeTex( Renderer * r, GLenum internal, GLenum format, GLenum type, int w, int h )
{
	auto fn = r->fn;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( internal ), w, h, 0, format, type, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	if ( format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL )
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

void freeGpu( Renderer * r, SssGpu & g )
{
	for ( GLuint * t : { &g.cur, &g.depth, &g.ext, &g.bx, &g.hext, &g.hbx, &g.hby, &g.hdep } ) {
		if ( *t )
			r->fn->glDeleteTextures( 1, t );
		*t = 0;
	}
	g.w = g.h = 0;
}

}	// namespace

bool wwCellSkinSssWanted( Scene * scene )
{
	return scene && wwSkinSssShaderDefine() && wwCellLightsWanted( scene );
}

void wwCellSkinSssApply( Scene * scene )
{
	if ( !wwCellSkinSssWanted( scene ) ) {
		note( QStringLiteral( "off" ) );
		return;
	}
	if ( !scene->renderer || scene->selecting )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	int tw = 0, th = 0;
	const GLuint tag = wwCellSkinSssTag( scene, tw, th );
	if ( !tag ) {
		note( QStringLiteral( "waiting (the opaque pass wrote no skin tag this frame)" ) );
		return;
	}
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const int W = vp[2], H = vp[3];
	if ( !prevDraw || vp[0] != 0 || vp[1] != 0 || W != tw || H != th ) {	// the passes draw at the frame's own viewport
		note( QStringLiteral( "refused (frame %1x%2 fbo %3, skin tag %4x%5)" ).arg( W ).arg( H ).arg( prevDraw ).arg( tw ).arg( th ) );
		return;
	}

	// the frame's formats: the colour's precision, and the depth (blitted, so the same format both sides)
	GLint redBits = 0, depthBits = 0, stencilBits = 0, compType = 0;
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &redBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
	GLenum dsFormat = 0, dsFmt = 0, dsType = 0;
	GLbitfield blitBits = GL_DEPTH_BUFFER_BIT;
	if ( stencilBits == 8 && depthBits == 24 && compType != GL_FLOAT ) {
		dsFormat = GL_DEPTH24_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_UNSIGNED_INT_24_8;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 8 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH32F_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 0 && depthBits == 24 ) {
		dsFormat = GL_DEPTH_COMPONENT24; dsFmt = GL_DEPTH_COMPONENT; dsType = GL_UNSIGNED_INT;
	} else if ( stencilBits == 0 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH_COMPONENT32F; dsFmt = GL_DEPTH_COMPONENT; dsType = GL_FLOAT;
	}
	if ( !dsFormat ) {
		note( QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil)" ).arg( depthBits ).arg( stencilBits ) );
		return;
	}
	const GLenum colFormat = redBits >= 32 ? GL_RGBA32F : GL_RGBA16F;

	SssGpu & g = gpus()[r];
	if ( !g.inFbo || g.w != W || g.h != H || g.colFormat != colFormat || g.dsFormat != dsFormat ) {
		freeGpu( r, g );
		if ( !g.inFbo ) {
			fn->glGenFramebuffers( 1, &g.inFbo );
			fn->glGenFramebuffers( 1, &g.fbo );
		}
		g.cur = makeTex( r, colFormat, GL_RGBA, GL_FLOAT, W, H );
		g.depth = makeTex( r, dsFormat, dsFmt, dsType, W, H );
		g.ext = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H );	// half floats between the passes, as FO4CS's targets
		g.bx = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H );
		for ( GLuint * t : { &g.hext, &g.hbx, &g.hby } )	// lane SKINSSS2: the half path's targets
			*t = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, WwSkinSss::halfSize( W ), WwSkinSss::halfSize( H ) );
		g.hdep = makeTex( r, GL_R32F, GL_RED, GL_FLOAT, WwSkinSss::halfSize( W ), WwSkinSss::halfSize( H ) );
		fn->glBindTexture( GL_TEXTURE_2D, g.hby );	// the composite's plain bilinear reads it filtered
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.inFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.cur, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, dsFmt == GL_DEPTH_STENCIL ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, g.depth, 0 );
		g.w = W;
		g.h = H;
		g.colFormat = colFormat;
		g.dsFormat = dsFormat;
	}

	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	GLint blendState[6] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO, GL_FUNC_ADD, GL_FUNC_ADD };	// lane SKINSSS2: the half composite blends
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &blendState[0] );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &blendState[1] );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &blendState[2] );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &blendState[3] );
	fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &blendState[4] );
	fn->glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &blendState[5] );

	// the frame (resolved when multisampled) and its depth
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, blitBits, GL_NEAREST );
	const GLenum blitErr = fn->glGetError();

	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const GLenum one = GL_COLOR_ATTACHMENT0;
	auto target = [&]( GLuint tex, GLuint second = 0 ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, second, 0 );	// 0 = detached
		const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		if ( second )
			fn->glDrawBuffers( 2, two );
		else
			fn->glDrawBuffers( 1, &one );
	};
	auto bind = [&]( int unit, GLuint tex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
	};

	// lane SKINSSS2: WW_CELL_SKINSSS_HALF=1 runs the half-resolution passes (FO4CS's work size, 3 taps; 0.47 of the full
	// cost, a softer picture: notes/skinsss2/RESULTS.md); off = SKINSSS1's full-resolution passes (6 taps)
	const bool half = wwEnv( "WW_CELL_SKINSSS_HALF" ).trimmed() == "1";
	const int radius = WwSkinSss::kernelRadius( half ? WwSkinSss::kHalfWorkScale : WwSkinSss::kWorkScale );
	WwSkinSss::V3 k[WwSkinSss::kMaxTaps];
	WwSkinSss::kernel( k, radius );
	const bool red = wwEnv( "WW_CELL_SKINSSS_RED" ).trimmed() == "1";
	bool ok = blitErr == GL_NO_ERROR;
	const int last = half ? 3 : 2;
	for ( int pass = 0; ok && pass <= last; pass++ ) {
		if ( pass < last ) {
			if ( half )
				target( pass == 0 ? g.hext : pass == 1 ? g.hbx : g.hby, pass == 0 ? g.hdep : 0 );
			else
				target( pass == 0 ? g.ext : g.bx );
			if ( half )
				fn->glViewport( 0, 0, WwSkinSss::halfSize( W ), WwSkinSss::halfSize( H ) );
		} else {
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
			fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
			fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );	// the frame's alpha (coverage over the sky) stays
			if ( half ) {	// ( scattered, coverage ) over the frame: lit + ( o - lit ) x a; uncovered pixels discard
				fn->glEnable( GL_BLEND );
				fn->glBlendEquation( GL_FUNC_ADD );
				fn->glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			}
		}
		auto prog = r->useProgram( half ? "cell_skinssshalf.prog" : "cell_skinsss.prog" );
		if ( !prog ) {
			ok = false;
			break;
		}
		if ( half )
			bind( kUnitSrc, pass == 0 ? g.cur : pass == 1 ? g.hext : pass == 2 ? g.hbx : g.hby );
		else
			bind( kUnitSrc, pass == 0 ? g.cur : pass == 1 ? g.ext : g.bx );
		bind( kUnitLit, half && pass > 0 ? g.hdep : g.cur );	// the half path reads no lit frame: its unit takes the half depth
		if ( half )
			prog->uni1i( "hdepthTex", kUnitLit );
		bind( kUnitDepth, g.depth );
		bind( kUnitSkin, tag );
		prog->uni1i( "srcTex", kUnitSrc );
		prog->uni1i( "litTex", kUnitLit );
		prog->uni1i( "depthTex", kUnitDepth );
		prog->uni1i( "skinTex", kUnitSkin );
		prog->uni1i( "sssPass", pass );
		prog->uni1i( "sssRadius", radius );
		prog->uni1b( "sssRed", red );
		const GLint kl = prog->uniLocation( "sssKernel" );
		if ( kl >= 0 )
			fn->glUniform3fv( kl, WwSkinSss::kMaxTaps, &k[0].r );
		const GLint pl = prog->uniLocation( "sssParams" );
		if ( pl >= 0 )
			fn->glUniform4f( pl, WwSkinSss::kAmount, WwSkinSss::kSaturation, WwSkinSss::kDepthThreshold, WwSkinSss::kDepthFalloff );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
	}

	for ( int u : { kUnitSrc, kUnitLit, kUnitDepth, kUnitSkin } ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	fn->glBlendFuncSeparate( GLenum( blendState[0] ), GLenum( blendState[1] ), GLenum( blendState[2] ), GLenum( blendState[3] ) );
	fn->glBlendEquationSeparate( GLenum( blendState[4] ), GLenum( blendState[5] ) );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	if ( !ok ) {
		note( blitErr != GL_NO_ERROR ? QStringLiteral( "refused (the frame's blit failed, GL error %1)" ).arg( blitErr )
			: QStringLiteral( "refused (no cell_skinsss.prog)" ) );
		return;
	}
	note( QStringLiteral( "on radius %1 amount %2 saturation %3 depth %4 x%5%6" ).arg( radius )
		.arg( double( WwSkinSss::kAmount ) ).arg( double( WwSkinSss::kSaturation ) ).arg( double( WwSkinSss::kDepthThreshold ) )
		.arg( double( WwSkinSss::kDepthFalloff ) ).arg( red ? QStringLiteral( " RED keep" ) : QString() )
		+ ( half ? QStringLiteral( " half" ) : QStringLiteral( " full" ) ) );
}
