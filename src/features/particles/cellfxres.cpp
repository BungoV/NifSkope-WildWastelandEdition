#include "features/particles/cellfxres.h"
#include "wwswitch.h"	// lane LIVESET1: the configuration follows a live set

#include "features/exposure/cellhdr.h"
#include "gl/celllights.h"
#include "features/shared/cellpasstime.h"
#include "features/reflections/cellssr.h"
#include "gl/cellunits.h"	// lane WATERFIX7: the passes' named units
#include "features/near/cellvrs.h"
#include "features/vkframe/vkfxres.h"	// lane VKSMALL5: steps 1 and 3 on the present device
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/gltools.h"
#include "gl/renderer.h"

#include <QDebug>
#include <QHash>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

namespace {

// the full-screen passes' units: the per-draw setup binds its own textures there again (and the memo is re-armed)
constexpr int kUnit0 = WwPassUnit::FullScreen0;

enum Red { RedNone, RedFull, RedQuarter, RedBilinear };

struct FxCfg
{
	bool on = false;
	int red = RedNone;
	int scale = 2;
	float minPx = 32.0f, minArea = 0.1f, edge = 0.1f;
	bool census = false;
};

const FxCfg & cfg()
{
	WW_SWITCH_STATIC( FxCfg, c, [] {	// lane LIVESET1: makeTargets keys on the scale, so a live set sizes them again
		FxCfg v;
		v.on = qEnvironmentVariableIntValue( "WW_CELL_FXRES" ) == 1;
		const QByteArray red = qgetenv( "WW_CELL_FXRES_RED" ).trimmed().toLower();
		v.red = red == "full" ? RedFull : red == "quarter" ? RedQuarter : red == "bilinear" ? RedBilinear : RedNone;
		bool ok = false;
		const int s = qEnvironmentVariable( "WW_CELL_FXRES_SCALE" ).toInt( &ok );
		if ( ok && ( s == 1 || s == 2 || s == 4 ) )
			v.scale = s;
		if ( v.red == RedQuarter )
			v.scale = 4;
		float f = qEnvironmentVariable( "WW_CELL_FXRES_MINPX" ).toFloat( &ok );
		if ( ok && f >= 0.0f )
			v.minPx = f;
		f = qEnvironmentVariable( "WW_CELL_FXRES_MINAREA" ).toFloat( &ok );
		if ( ok && f >= 0.0f )
			v.minArea = f;
		f = qEnvironmentVariable( "WW_CELL_FXRES_EDGE" ).toFloat( &ok );
		if ( ok && f > 0.0f )
			v.edge = f;
		v.census = qEnvironmentVariableIntValue( "WW_CELL_FXRES_CENSUS" ) == 1;
		return v;
	}() );
	return c;
}

struct FxGpu
{
	GLuint zFbo = 0, layerFbo = 0, z = 0, depth = 0, c = 0, t = 0, cov = 0;
	int W = 0, H = 0, w = 0, h = 0, s = 0;
	bool complete = false;
	bool trusted = false;	// a pass on these targets raised no GL error (errors read on new targets only)
	bool failed = false;	// a GL error on these targets: the row stays off for this view
	bool vkAsked = false, vkShared = false;	// lane VKSMALL5: the five targets are the present device's (owner = the renderer)
	GLuint srcTex = 0;	// lane VKSMALL5: fxrSrc's internal format, read once a texture / size
	int srcW = 0, srcH = 0;
	GLenum srcFmt = 0;
};

struct FxState
{
	bool drawing = false;
	QVector<Shape *> refused;
	int sorted = 0, soft = 0, small = 0, layered = 0;
	double share = 0.0;
	QString last = QStringLiteral( "off" );
	QString said;
};

FxState & fst()
{
	static FxState s;
	return s;
}

QHash<Renderer *, FxGpu> & fxGpus()
{
	static QHash<Renderer *, FxGpu> g;
	return g;
}

void note( const QString & line )
{
	FxState & s = fst();
	s.last = line;
	if ( s.said != line ) {
		s.said = line;
		qInfo().noquote() << "cell fxres:" << line;
	}
}

GLuint makeTex( Renderer * r, GLenum fmt, GLenum pixFmt, GLenum type, int w, int h )
{
	auto fn = r->fn;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), w, h, 0, pixFmt, type, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	return t;
}

void freeTargets( Renderer * r, FxGpu & g )
{
	auto fn = r->fn;
	const GLuint tex[5] = { g.z, g.depth, g.c, g.t, g.cov };
	if ( g.vkShared )
		WwVkPresent::fxresTargetsDrop( r );	// lane VKSMALL5: the route owns those names
	else
		fn->glDeleteTextures( 5, tex );
	const GLuint fbo[2] = { g.zFbo, g.layerFbo };
	fn->glDeleteFramebuffers( 2, fbo );
	g = FxGpu();
}

//! the layer's targets for a W x H frame at s pixels per texel; fbo = the framebuffer bound after
bool makeTargets( Renderer * r, FxGpu & g, int W, int H, int s, GLuint fbo )
{
	const bool vkAsk = WwVkPresent::shareKept( "WW_VKVIEW_FXRES_RED" );	// lane VKSMALL5
	const WwVkPresent::FxresTargets have { g.z, g.depth, g.c, g.t, g.cov };
	if ( g.zFbo && g.W == W && g.H == H && g.s == s && g.vkAsked == vkAsk && ( !g.vkShared || WwVkPresent::fxresTargetsAre( r, have ) ) )
		return g.complete;
	if ( g.zFbo )
		freeTargets( r, g );
	auto fn = r->fn;
	GLint prevTex = 0;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	g.W = W;
	g.H = H;
	g.s = s;
	g.w = ( W + s - 1 ) / s;
	g.h = ( H + s - 1 ) / s;
	g.vkAsked = vkAsk;
	WwVkPresent::FxresTargets vt;
	std::string why;
	if ( vkAsk && WwVkPresent::fxresTargets( r, g.w, g.h, vt, why ) ) {
		// lane VKSMALL5: the present device's twins (steps 1 and 3 draw there on Vulkan); GL attaches them as before
		g.vkShared = true;
		g.z = vt.z;
		g.depth = vt.depth;
		g.c = vt.c;
		g.t = vt.t;
		g.cov = vt.cov;
		const GLuint tex[5] = { g.z, g.depth, g.c, g.t, g.cov };
		for ( GLuint t : tex ) {
			fn->glBindTexture( GL_TEXTURE_2D, t );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		}
	} else {
		if ( vkAsk )
			WwVkPresent::fxresOnGl( ( "the layer's targets: " + why ).c_str() );
		g.z = makeTex( r, GL_R32F, GL_RED, GL_FLOAT, g.w, g.h );
		g.depth = makeTex( r, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, g.w, g.h );
		g.c = makeTex( r, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, g.w, g.h );
		g.t = makeTex( r, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, g.w, g.h );
		g.cov = makeTex( r, GL_R8, GL_RED, GL_UNSIGNED_BYTE, g.w, g.h );
	}
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
	fn->glGenFramebuffers( 1, &g.zFbo );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.zFbo );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.z, 0 );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g.depth, 0 );
	bool ok = fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	fn->glGenFramebuffers( 1, &g.layerFbo );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.layerFbo );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.c, 0 );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.t, 0 );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g.cov, 0 );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g.depth, 0 );
	const GLenum bufs[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	fn->glDrawBuffers( 3, bufs );
	ok = ok && fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
	g.complete = ok;
	g.trusted = false;
	return ok;
}

void quad( Renderer * r )
{
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
}

//! the GL state the full-screen passes change, put back for the draws after them
struct GlKeep
{
	GLboolean blend, depthTest, depthMask, stencilTest, cull, scissor, colorMask[4];
	GLint depthFunc, blendSrcRgb, blendDstRgb, blendSrcA, blendDstA, eqRgb, eqA;
	GLint stencilFunc, stencilRef, stencilValueMask, stencilWriteMask, stencilFail, stencilZFail, stencilZPass;
	GLint activeTex;	// the program is not kept: the renderer binds its own at every draw (stopProgram left 0)

	explicit GlKeep( Renderer * r )
	{
		auto fn = r->fn;
		blend = fn->glIsEnabled( GL_BLEND );
		depthTest = fn->glIsEnabled( GL_DEPTH_TEST );
		stencilTest = fn->glIsEnabled( GL_STENCIL_TEST );
		cull = fn->glIsEnabled( GL_CULL_FACE );
		scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
		fn->glGetIntegerv( GL_BLEND_SRC_RGB, &blendSrcRgb );
		fn->glGetIntegerv( GL_BLEND_DST_RGB, &blendDstRgb );
		fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &blendSrcA );
		fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &blendDstA );
		fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &eqRgb );
		fn->glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &eqA );
		fn->glGetIntegerv( GL_STENCIL_FUNC, &stencilFunc );
		fn->glGetIntegerv( GL_STENCIL_REF, &stencilRef );
		fn->glGetIntegerv( GL_STENCIL_VALUE_MASK, &stencilValueMask );
		fn->glGetIntegerv( GL_STENCIL_WRITEMASK, &stencilWriteMask );
		fn->glGetIntegerv( GL_STENCIL_FAIL, &stencilFail );
		fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &stencilZFail );
		fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &stencilZPass );
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &activeTex );
	}
	void put( Renderer * r ) const
	{
		auto fn = r->fn;
		auto set = [fn]( GLenum cap, GLboolean on ) {
			if ( on )
				fn->glEnable( cap );
			else
				fn->glDisable( cap );
		};
		set( GL_BLEND, blend );
		set( GL_DEPTH_TEST, depthTest );
		set( GL_STENCIL_TEST, stencilTest );
		set( GL_CULL_FACE, cull );
		set( GL_SCISSOR_TEST, scissor );
		fn->glDepthMask( depthMask );
		fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
		fn->glDepthFunc( GLenum( depthFunc ) );
		fn->glBlendFuncSeparate( GLenum( blendSrcRgb ), GLenum( blendDstRgb ), GLenum( blendSrcA ), GLenum( blendDstA ) );
		fn->glBlendEquationSeparate( GLenum( eqRgb ), GLenum( eqA ) );
		fn->glStencilFunc( GLenum( stencilFunc ), stencilRef, GLuint( stencilValueMask ) );
		fn->glStencilMask( GLuint( stencilWriteMask ) );
		fn->glStencilOp( GLenum( stencilFail ), GLenum( stencilZFail ), GLenum( stencilZPass ) );
		fn->glActiveTexture( GLenum( activeTex ) );
	}
};

bool srcOk( GLint f )
{
	return f == GL_ZERO || f == GL_ONE || f == GL_SRC_ALPHA || f == GL_ONE_MINUS_SRC_ALPHA || f == GL_SRC_COLOR
		|| f == GL_ONE_MINUS_SRC_COLOR;
}

bool dstOk( GLint f )
{
	return f == GL_ZERO || f == GL_ONE || f == GL_SRC_ALPHA || f == GL_ONE_MINUS_SRC_ALPHA;
}

//! the pass qualifies: the cell view's main HDR frame (not the obscurance's, the reflections', the measure's or a pick)
bool passWanted( Scene * scene )
{
	return scene && scene->renderer && !scene->selecting && wwCellHdrActive() && wwCellLightsWanted( scene )
		&& !( wwCellFxRed() & 1 ) && !wwCellImageSpaceIsMeasuring() && !wwCellAoInPass() && !wwCellSsrInPass();
}

}	// namespace

QString wwCellFxResEcho()
{
	return fst().last;
}

bool wwCellFxResDrawing()
{
	return fst().drawing;
}

void wwCellFxResDrawHook( Shape * mesh, const std::string_view & program )
{
	FxState & s = fst();
	if ( !s.drawing || !mesh || !mesh->wwFxResScene() || !mesh->wwFxResScene()->renderer )
		return;
	auto fn = mesh->wwFxResScene()->renderer->fn;
	// the layer holds a draw exactly when it is dst = src x A + dst x B, B a function of the draw's alpha alone
	bool ok = program == std::string_view( "fo4_effectcellfx.prog" ) && fn->glIsEnabled( GL_BLEND );
	if ( ok ) {
		GLint f[6] = { 0, 0, 0, 0, 0, 0 };
		fn->glGetIntegerv( GL_BLEND_SRC_RGB, &f[0] );
		fn->glGetIntegerv( GL_BLEND_DST_RGB, &f[1] );
		fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &f[2] );
		fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &f[3] );
		fn->glGetIntegerv( GL_BLEND_EQUATION_RGB, &f[4] );
		fn->glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &f[5] );
		ok = srcOk( f[0] ) && dstOk( f[1] ) && srcOk( f[2] ) && dstOk( f[3] ) && f[4] == GL_FUNC_ADD && f[5] == GL_FUNC_ADD;
	}
	fn->glColorMask( ok, ok, ok, ok );
	if ( !ok && !s.refused.contains( mesh ) )
		s.refused.append( mesh );
}

QVector<Node *> wwCellFxResSplit( const QVector<Node *> & sorted )
{
	const FxCfg & c = cfg();
	QVector<Node *> out;
	if ( !c.on ) {
		static bool said = false;
		if ( !said ) {
			said = true;
			note( QStringLiteral( "off (WW_CELL_FXRES unset)" ) );
		}
		return out;
	}
	// the pass is static (it may hold several documents' shapes): the layer takes the first soft draw's scene
	Scene * scene = nullptr;
	for ( Node * node : sorted ) {
		Shape * sh = dynamic_cast<Shape *>( node );
		if ( sh && sh->wwFxResSoft() ) {
			scene = sh->wwFxResScene();
			break;
		}
	}
	if ( !scene || !passWanted( scene ) )
		return out;
	Renderer * r = scene->renderer;
	auto it = fxGpus().find( r );
	if ( it != fxGpus().end() && it->failed )
		return out;
	GLint vp[4] = { 0, 0, 0, 0 };
	r->fn->glGetIntegerv( GL_VIEWPORT, vp );
	int fw = 0, fh = 0;
	wwCellHdrFrameSize( fw, fh );
	if ( vp[0] != 0 || vp[1] != 0 || vp[2] < 1 || vp[3] < 1 || vp[2] != fw || vp[3] != fh ) {
		note( QStringLiteral( "refused (the viewport %1,%2 %3x%4 is not the frame %5x%6)" )
			.arg( vp[0] ).arg( vp[1] ).arg( vp[2] ).arg( vp[3] ).arg( fw ).arg( fh ) );
		return out;
	}
	// a draw leaves its place when its bounding sphere is large on screen (sparks and small puffs stay full resolution)
	const auto & pm = r->globalUniforms->projectionMatrix;	// pm[column][row]
	const float pxPerUnit = pm[1][1] * float( vp[3] ) * 0.5f;
	const double screen = double( vp[2] ) * double( vp[3] );
	FxState & s = fst();
	s.sorted = int( sorted.size() );
	s.soft = s.small = 0;
	s.share = 0.0;
	for ( Node * node : sorted ) {
		Shape * sh = dynamic_cast<Shape *>( node );
		if ( !sh || !sh->wwFxResSoft() || sh->wwFxResScene() != scene )
			continue;
		s.soft++;
		const BoundSphere vb = scene->view * sh->bounds();
		if ( vb.center[2] > vb.radius )
			continue;	// wholly behind the eye: draws nothing
		const float dist = std::max( vb.center.length() - vb.radius, 1.0f );
		const float px = vb.radius * pxPerUnit / dist;
		if ( px < c.minPx ) {
			s.small++;
			continue;
		}
		s.share += std::min( 3.14159265 * double( px ) * double( px ) / screen, 1.0 );
		out.append( node );
	}
	s.layered = int( out.size() );
	if ( c.census )
		qInfo().noquote() << QStringLiteral( "cell fxres census: sorted %1 soft %2 small %3 layer %4 share %5" )
			.arg( s.sorted ).arg( s.soft ).arg( s.small ).arg( s.layered ).arg( s.share, 0, 'f', 3 );
	if ( c.red == RedFull ) {
		note( QStringLiteral( "RED full: %1 of %2 soft draws would leave their place (share %3); every draw stays in place" )
			.arg( s.layered ).arg( s.soft ).arg( s.share, 0, 'f', 2 ) );
		out.clear();
		return out;
	}
	if ( out.isEmpty() || s.share < double( c.minArea ) ) {
		note( QStringLiteral( "kept in place (%1 soft draws large on screen, share %2 under %3)" )
			.arg( s.layered ).arg( s.share, 0, 'f', 2 ).arg( double( c.minArea ) ) );
		out.clear();
	}
	return out;
}

/* lane VKSMALL5 (census row 77): step 1 (cell_fxres_down) and step 3 (cell_fxres_comp) on the present device
 * (vkfxres.inc; WW_VKVIEW=1, red WW_VKVIEW_FXRES_RED=1: GL's quads). Each is its own crossing and GL waits on Vulkan;
 * step 2 stays GL, into the shared targets. true: drawn there; false: GL draws the quad (the reason on "vk fxres:") */
static bool fxresOnVk( int step, Scene * scene, FxGpu & g, int W, int H, const GLint vp[4], GLint prevDraw )
{
	static QString said;
	static auto at = std::chrono::steady_clock::now();
	auto say = [&]( const QString & line ) {
		if ( line == said )
			return;
		said = line;
		at = std::chrono::steady_clock::now();
		qInfo().noquote() << line;
	};
	if ( !WwVkPresent::shareWanted( "WW_VKVIEW_FXRES_RED" ) ) {
		say( QStringLiteral( "vk fxres: on GL (red WW_VKVIEW_FXRES_RED=1 or the view not on Vulkan)" ) );
		return false;
	}
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const FxCfg & c = cfg();
	const GLuint src = scene->fxDepthTexId;
	bool drew = false;
	if ( !g.vkShared ) {
		WwVkPresent::fxresOnGl( "the layer's targets are GL's own (refused at their make)" );
	} else if ( !src || scene->fxDepthTexW != W || scene->fxDepthTexH != H ) {
		WwVkPresent::fxresOnGl( "the opaque depth is not at the frame's size" );
	} else {
		const std::uintptr_t key = scene->fxDepthShared ? reinterpret_cast<std::uintptr_t>( &scene->fxDepthTexId ) : 0;
		if ( !key && ( g.srcTex != src || g.srcW != W || g.srcH != H ) ) {
			GLint prevTex = 0, f = 0;
			fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
			fn->glBindTexture( GL_TEXTURE_2D, src );
			fn->glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &f );
			fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
			g.srcTex = src;
			g.srcW = W;
			g.srcH = H;
			g.srcFmt = GLenum( f );
		}
		std::string why;
		if ( step == 1 ) {
			WwVkPresent::FxresDown d;
			d.owner = r;
			d.W = W;
			d.H = H;
			d.scale = g.s;
			d.src = src;
			d.srcFmt = g.srcFmt;
			d.srcKey = key;
			drew = WwVkPresent::fxresDownRun( d, why );
		} else {
			GLint color = 0;
			if ( prevDraw )
				fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &color );
			WwVkPresent::FxresComp cp;
			cp.owner = r;
			cp.color = unsigned( color );
			cp.W = W;
			cp.H = H;
			cp.scale = g.s;
			cp.src = src;
			cp.srcFmt = g.srcFmt;
			cp.srcKey = key;
			const auto & pm = r->globalUniforms->projectionMatrix;
			cp.proj[0] = pm[2][2];
			cp.proj[1] = pm[3][2];
			cp.edge = c.edge;
			cp.mode = c.red == RedBilinear ? 1 : 0;
			for ( int i = 0; i < 4; i++ )
				cp.vp[i] = vp[i];
			drew = WwVkPresent::fxresCompRun( cp, why );
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFxresStat();
	static WwVkPresent::FxStat sum;
	sum.runs += st.runs;
	sum.gl += st.gl;
	sum.fails += st.fails;
	sum.copies += st.copies;
	sum.cpuMs += st.cpuMs;
	sum.maxMs = std::max( sum.maxMs, st.maxMs );
	sum.pipelines = st.pipelines;
	sum.buildMs = st.buildMs;
	if ( !st.glWhy.empty() )
		sum.glWhy = st.glWhy;
	if ( !st.failWhy.empty() )
		sum.failWhy = st.failWhy;
	if ( std::chrono::duration<double>( std::chrono::steady_clock::now() - at ).count() >= 5.0 || !said.startsWith( QStringLiteral( "vk fxres: on V" ) ) ) {
		QString line = sum.runs > 0
			? QStringLiteral( "vk fxres: on Vulkan %1 runs (steps 1 and 3 of the soft-effect layer, one crossing each, GL waits; %2 opaque "
				"depth copies; %3 pipelines built in %4 ms; drawing thread %5 ms, max %6)" ).arg( sum.runs ).arg( sum.copies )
				.arg( sum.pipelines ).arg( sum.buildMs, 0, 'f', 1 ).arg( sum.cpuMs / sum.runs, 0, 'f', 3 ).arg( sum.maxMs, 0, 'f', 3 )
			: QStringLiteral( "vk fxres: on Vulkan wanted, 0 runs drawn" );
		if ( sum.gl > 0 )
			line += QStringLiteral( ", on GL %1 runs (last: %2)" ).arg( sum.gl ).arg( QString::fromStdString( sum.glWhy ) );
		if ( sum.fails > 0 )
			line += QStringLiteral( ", refused %1 (last: %2)" ).arg( sum.fails ).arg( QString::fromStdString( sum.failWhy ) );
		said.clear();
		say( line );
		sum = WwVkPresent::FxStat();
	}
	return drew;
}

void wwCellFxResDraw( const QVector<Node *> & soft )
{
	if ( soft.isEmpty() )
		return;
	Scene * scene = static_cast<Shape *>( soft.first() )->wwFxResScene();	// wwCellFxResSplit took Shapes of one scene
	const FxCfg & c = cfg();
	FxState & s = fst();
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	auto inPlace = [&soft]() {
		for ( Node * node : soft )
			node->drawShapes();
	};
	// the full-resolution opaque depth (the soft fade's, grabbed once a pass) is the layer's source
	if ( !scene->grabEffectDepth() ) {
		note( QStringLiteral( "refused (no opaque depth this pass)" ) );
		inPlace();
		return;
	}
	GLint vp[4] = { 0, 0, 0, 0 }, prevDraw = 0, prevRead = 0;
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	FxGpu & g = fxGpus()[r];
	const int W = vp[2], H = vp[3];
	if ( !makeTargets( r, g, W, H, c.scale, GLuint( prevDraw ) ) ) {
		g.failed = true;
		note( QStringLiteral( "refused (the layer's framebuffer is incomplete)" ) );
		inPlace();
		return;
	}
	const bool check = !g.trusted;
	if ( check ) {
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	const GlKeep keep( r );
	const bool vrs = wwCellVrsActive();
	if ( vrs )
		wwCellVrsPass( scene, true, false );	// the rate image is the frame's: the layer shades every texel

	// 1. the layer's depth: the nearest opaque depth under each texel; then the layer cleared
	PassTime::subBegin( PassTime::FxResDown );
	const GLuint fullDepth = scene->fxDepthTexId;
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.zFbo );
	fn->glViewport( 0, 0, g.w, g.h );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_ALWAYS );
	fn->glDepthMask( GL_TRUE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	bool ok = false;
	const bool vkDown = fxresOnVk( 1, scene, g, W, H, vp, prevDraw );	// lane VKSMALL5: z and depth drawn on Vulkan, GL waited
	if ( vkDown ) {
		ok = true;
	} else if ( auto prog = r->useProgram( "cell_fxres_down.prog" ) ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnit0 ) );
		fn->glBindTexture( GL_TEXTURE_2D, fullDepth );
		prog->uni1i( "fxrSrc", kUnit0 );
		prog->uni1i( "fxrScale", g.s );
		fn->glUniform2i( prog->uniLocation( "fxrSrcSize" ), W, H );
		quad( r );
		r->stopProgram();
		ok = true;
	}
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.layerFbo );
	const GLfloat zero[4] = { 0, 0, 0, 0 }, one[4] = { 1, 1, 1, 1 };
	fn->glClearBufferfv( GL_COLOR, 0, zero );
	fn->glClearBufferfv( GL_COLOR, 1, one );
	fn->glClearBufferfv( GL_COLOR, 2, zero );
	PassTime::subEnd( PassTime::FxResDown );
	if ( !ok ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		keep.put( r );
		if ( vrs )
			wwCellVrsPass( scene, true, true );
		note( QStringLiteral( "refused (cell_fxres_down.prog did not load)" ) );
		inPlace();
		return;
	}

	// 2. the soft draws, in their sorted order, into the layer (the soft fade reads the layer's depth)
	PassTime::subBegin( PassTime::FxResLayer );
	keep.put( r );	// each draw sees the state it would have had in place
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.layerFbo );
	fn->glViewport( 0, 0, g.w, g.h );
	scene->fxDepthTexId = g.z;
	s.refused.clear();
	s.drawing = true;
	wwSetupMemoArm( true );	// the scene globals set again under the layer's viewport
	for ( Node * node : soft )
		node->drawShapes();
	s.drawing = false;
	wwSetupMemoArm( true );
	scene->fxDepthTexId = fullDepth;
	PassTime::subEnd( PassTime::FxResLayer );

	// 3. the composite: frame = C + frame x T, the stencil marks the pixels a draw landed on as linear light (1)
	PassTime::subBegin( PassTime::FxResComp );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glEnable( GL_BLEND );
	fn->glBlendEquation( GL_FUNC_ADD );
	fn->glBlendFuncSeparate( GL_ONE, GL_SRC1_COLOR, GL_ONE, GL_SRC1_ALPHA );
	fn->glEnable( GL_STENCIL_TEST );
	fn->glStencilMask( 0x03 );
	fn->glStencilFunc( GL_ALWAYS, 1, 0xFF );
	fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	const bool vkComp = fxresOnVk( 3, scene, g, W, H, vp, prevDraw );	// lane VKSMALL5: the composite drawn on Vulkan, GL waited
	if ( vkComp ) {
		// GL's state above stays set (keep.put restores it): nothing bound here
	} else if ( auto prog = r->useProgram( "cell_fxres_comp.prog" ) ) {
		const GLuint tex[5] = { g.c, g.t, g.cov, g.z, fullDepth };
		static const char * const names[5] = { "fxrC", "fxrT", "fxrCov", "fxrZ", "fxrDepth" };
		for ( int i = 0; i < 5; i++ ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnit0 + i ) );
			fn->glBindTexture( GL_TEXTURE_2D, tex[i] );
			prog->uni1i( names[i], kUnit0 + i );
		}
		prog->uni1i( "fxrScale", g.s );
		fn->glUniform2i( prog->uniLocation( "fxrSize" ), g.w, g.h );
		const auto & pm = r->globalUniforms->projectionMatrix;
		prog->uni2f( "fxrProj", pm[2][2], pm[3][2] );
		prog->uni1f( "fxrEdge", c.edge );
		prog->uni1i( "fxrMode", c.red == RedBilinear ? 1 : 0 );
		quad( r );
		r->stopProgram();
		for ( int i = 4; i >= 0; i-- ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnit0 + i ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
		}
	} else {
		ok = false;
	}
	keep.put( r );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	if ( vrs )
		wwCellVrsPass( scene, true, true );
	PassTime::subEnd( PassTime::FxResComp );
	if ( check ) {
		const GLenum err = fn->glGetError();
		if ( err != GL_NO_ERROR ) {
			g.failed = true;
			note( QStringLiteral( "refused (the layer raised GL error 0x%1; off for this view)" ).arg( err, 0, 16 ) );
			return;
		}
		g.trusted = ok;
	}
	// 4. a draw the layer could not hold, in place over the composite
	const QVector<Shape *> late = s.refused;
	s.refused.clear();
	for ( Shape * sh : late )
		sh->drawShapes();
	if ( !ok ) {
		note( QStringLiteral( "refused (cell_fxres_comp.prog did not load)" ) );
		return;
	}
	static const char * const redNames[4] = { "", "", ", RED quarter (1/4 per axis)", ", RED bilinear (no depth test in the upsample)" };
	note( QStringLiteral( "on, layer 1/%1 (%2x%3 of %4x%5), %6 of %7 soft draws there, %8 drawn in place after it, edge %9%10" )
		.arg( g.s ).arg( g.w ).arg( g.h ).arg( W ).arg( H ).arg( soft.size() - late.size() ).arg( s.soft ).arg( late.size() )
		.arg( double( c.edge ) ).arg( QString::fromLatin1( redNames[c.red] ) ) );
}
