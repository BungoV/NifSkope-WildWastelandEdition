/* tests/celluni4 (lane CELLUNI4): the CELLUNI3 leftovers on the kit (src/gl/celluniform.h), run through the real driver.
 * The tone map's frame end as cellhdr.cpp wwCellHdrEnd runs it: the GlSnap read, the SSR keep, the temporal AA before the
 * tone map (its depth format cached per framebuffer and size, its error reads skipped once trusted), the measure, the
 * FO4CS bloom and the fog bloom (both remake their targets on a resize frame and bind their own framebuffer BEFORE their
 * save, a bug kept as it was), then the tone quads. Before it, the census of a water-ray pass (the SSR hook disables
 * blend) and FO76 draws binding CubeMap2 (the second cube of a texture) whose decode mode is read.
 * The view resizes at frame 6, its depth turning D32F_S8. Green, the red (WW_CELL_UNI_RED: the old asks) and "prev"
 * (main before this lane) must give the same pixels and state byte for byte. Doctored runs (the depth cache keyed
 * without the size, the known state taken on a resize frame, a CubeMap2 reload without its forget, a wrong known blend)
 * must differ. Usage: celluni4_gl [frames]   exit 0 = all ok */
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>
#include <QElapsedTimer>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gl/celluniform.h"

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

// every driver query the modelled code makes, counted on the way to the driver
struct CountFn
{
	QOpenGLExtraFunctions * f = nullptr;
	long long n = 0;
	void glGetIntegerv( GLenum p, GLint * v ) { n++; f->glGetIntegerv( p, v ); }
	void glGetFloatv( GLenum p, GLfloat * v ) { n++; f->glGetFloatv( p, v ); }
	void glGetBooleanv( GLenum p, GLboolean * v ) { n++; f->glGetBooleanv( p, v ); }
	bool glIsEnabled( GLenum e ) { n++; return f->glIsEnabled( e ) != GL_FALSE; }
	GLenum glGetError() { n++; return f->glGetError(); }
	void glGetFramebufferAttachmentParameteriv( GLenum t, GLenum a, GLenum p, GLint * v ) { n++; f->glGetFramebufferAttachmentParameteriv( t, a, p, v ); }
	void glGetTexLevelParameteriv( GLenum t, GLint l, GLenum p, GLint * v ) { n++; f->glGetTexLevelParameteriv( t, l, p, v ); }
};

static QOpenGLExtraFunctions * gl = nullptr;

enum Mode { Green, Red, Prev, GreenCheck, DocDepthNoKey, DocKnownOnMade, DocCubeNoForget, DocWrongBlend, DocNoClear, DocKnownWrongChecked };
static const char * const kModeName[] = { "green", "red", "prev", "green-check", "doc-depth-no-size-key", "doc-known-on-resize",
	"doc-cube2-no-forget", "doc-wrong-known-blend", "doc-bloom-made-keeps-slot", "doc-known-wrong-checked" };

struct Cfg
{
	bool red = false, snEarly = true, known = true, cube2 = true, blendKnown = true, check = false;
	bool docDepth = false, docMade = false, docCube = false, docBlend = false, docKnownWrong = false, docNoClear = false;
};
static Cfg cfgOf( Mode m )
{
	Cfg c;
	if ( m == Red )
		c.red = true;
	if ( m == Prev )	// main before CELLUNI4: the GlSnap read after the TAA, every new path off
		c.snEarly = c.known = c.cube2 = c.blendKnown = false;
	c.check = m == GreenCheck || m == DocKnownWrongChecked;
	c.docDepth = m == DocDepthNoKey;
	c.docMade = m == DocKnownOnMade;
	c.docCube = m == DocCubeNoForget;
	c.docBlend = m == DocWrongBlend;
	c.docKnownWrong = m == DocKnownWrongChecked;
	c.docNoClear = m == DocNoClear;
#ifdef CELLUNI4_RED	// the red build: green without the new forgets and with the known state on a resize frame
	if ( m == Green )
		c.docDepth = c.docCube = c.docMade = true;
#endif
	return c;
}

// ---- shaders ----
static const char * kVert = R"(#version 460 core
void main() { vec2 p = vec2( ( gl_VertexID & 1 ) * 4.0 - 1.0, ( gl_VertexID & 2 ) * 2.0 - 1.0 ); gl_Position = vec4( p, 0.0, 1.0 ); }
)";
static const char * kScene = R"(#version 460 core
uniform int frame; out vec4 o;
void main() { vec2 p = gl_FragCoord.xy; o = vec4( fract( p.x * 0.013 + float( frame ) * 0.07 ), fract( p.y * 0.021 ), fract( ( p.x + p.y ) * 0.007 ), 1.0 ) * 2.0; }
)";
static const char * kTaa = R"(#version 460 core
uniform sampler2D cur; uniform sampler2D dep; out vec4 o;
void main() { ivec2 p = ivec2( gl_FragCoord.xy ); o = texelFetch( cur, p, 0 ) * 0.75 + vec4( texelFetch( dep, p, 0 ).r * 0.25 ); }
)";
static const char * kBloom = R"(#version 460 core
uniform sampler2D src; out vec4 o;
void main() { ivec2 p = ivec2( gl_FragCoord.xy ) * 2; o = texelFetch( src, p, 0 ) * 0.5 + texelFetch( src, p + ivec2( 1 ), 0 ) * 0.5; }
)";
static const char * kFog = R"(#version 460 core
uniform sampler2D dep; out vec4 o;
void main() { float d = texelFetch( dep, ivec2( gl_FragCoord.xy ), 0 ).r; o = vec4( d, 1.0 - d, d * d, 1.0 ); }
)";
static const char * kMeas = R"(#version 460 core
uniform sampler2D src; out vec4 o;
void main() { o = texelFetch( src, ivec2( gl_FragCoord.xy ) * 4, 0 ); }
)";
static const char * kTone = R"(#version 460 core
uniform sampler2D hdr; uniform sampler2D bloom; uniform sampler2D fog; out vec4 o;
void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	vec3 c = texelFetch( hdr, p, 0 ).rgb * 0.4 + texelFetch( bloom, p / 2, 0 ).rgb * 0.3 + texelFetch( fog, p, 0 ).rgb * 0.2;
	o = vec4( c / ( 1.0 + c ), 1.0 );
}
)";
static const char * kDraw = R"(#version 460 core
uniform int aoSeen; uniform int blendSeen; uniform vec4 cellCubeMat; out vec4 o;
void main()
{
	float h = float( aoSeen ) * 0.31 + float( blendSeen ) * 0.17 + dot( cellCubeMat, vec4( 0.011, 0.013, 0.07, 0.019 ) );
	o = vec4( fract( h ), fract( h * 7.0 ), fract( h * 31.0 ), 1.0 );
}
)";

static GLuint link( const char * frag )
{
	auto sh = [&]( GLenum t, const char * s ) {
		GLuint x = gl->glCreateShader( t );
		gl->glShaderSource( x, 1, &s, nullptr );
		gl->glCompileShader( x );
		GLint ok = 0;
		gl->glGetShaderiv( x, GL_COMPILE_STATUS, &ok );
		if ( !ok ) {
			char log[4096] = {};
			gl->glGetShaderInfoLog( x, sizeof log, nullptr, log );
			std::printf( "shader: %s\n", log );
		}
		return x;
	};
	GLuint p = gl->glCreateProgram();
	gl->glAttachShader( p, sh( GL_VERTEX_SHADER, kVert ) );
	gl->glAttachShader( p, sh( GL_FRAGMENT_SHADER, frag ) );
	gl->glLinkProgram( p );
	GLint ok = 0;
	gl->glGetProgramiv( p, GL_LINK_STATUS, &ok );
	if ( !ok )
		std::printf( "link failed\n" );
	return p;
}

static GLuint tex2d( GLenum ifmt, int w, int h, GLenum fmt = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE )
{
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	gl->glBindTexture( GL_TEXTURE_2D, t );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, h, 0, fmt, type, nullptr );
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

static void fillCube( GLuint t, GLenum fmt, unsigned char v )
{
	const unsigned char px[4] = { v, (unsigned char)( v / 2 ), 200, 255 };
	gl->glBindTexture( GL_TEXTURE_CUBE_MAP, t );
	gl->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	gl->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	for ( int f = 0; f < 6; f++ )
		gl->glTexImage2D( GLenum( GL_TEXTURE_CUBE_MAP_POSITIVE_X + f ), 0, GLint( fmt ), 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
	gl->glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
}

static const int DW = 64, DH = 32, kCensus = 600, kCube = 300;

struct Res
{
	GLuint vao = 0;
	GLuint V = 0, vColor = 0, vDs = 0, R2 = 0, r2Tex = 0, L = 0, lColor = 0, ssrFbo = 0, ssrTex = 0, measFbo = 0, measTex = 0;
	GLuint D = 0, dTex = 0;
	GLuint pScene = 0, pTaa = 0, pBloom = 0, pFog = 0, pMeas = 0, pTone = 0, pDraw = 0;
	struct { GLuint inFbo = 0, cur = 0, dep = 0; GLenum dsFormat = 0; int w = 0, h = 0; } taa;
	struct { GLuint srcFbo = 0, resFbo = 0, resTex = 0, mipTex = 0; int w = 0, h = 0; } bloom;
	struct { GLuint fbo = 0, depthFbo = 0, tex = 0, depthTex = 0; int w = 0, h = 0; GLenum ds = 0; } fog;
	GLuint id0[3] = {}, id1[3] = {};
	WwCellUni::DepthCfg depthCfg;	// gametaa.cpp's static (one run = one fresh program)
	std::vector<int> cubeCache;	// gltex.h ParamShadow::cube, indexed by GL name
	GLuint cubeBound = 0;	// TexCache::wwCubeBound
};

static void allocView( Res & R, int w, int h, GLenum ds )
{
	if ( R.vColor )
		gl->glDeleteTextures( 1, &R.vColor );
	if ( R.lColor )
		gl->glDeleteTextures( 1, &R.lColor );
	R.vColor = tex2d( GL_RGBA8, w, h );
	R.lColor = tex2d( GL_RGBA16F, w, h, GL_RGBA, GL_FLOAT );
	if ( !R.vDs )
		gl->glGenRenderbuffers( 1, &R.vDs );
	gl->glBindRenderbuffer( GL_RENDERBUFFER, R.vDs );
	gl->glRenderbufferStorage( GL_RENDERBUFFER, ds, w, h );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, R.V );
	gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.vColor, 0 );
	gl->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, R.vDs );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, R.L );
	gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.lColor, 0 );
}

static GLuint fboWith( GLuint t )
{
	GLuint f = 0;
	gl->glGenFramebuffers( 1, &f );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, f );
	gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0 );
	return f;
}

static void build( Res & R )
{
	gl->glActiveTexture( GL_TEXTURE0 );
	gl->glGenVertexArrays( 1, &R.vao );
	gl->glGenFramebuffers( 1, &R.V );
	gl->glGenFramebuffers( 1, &R.L );
	R.r2Tex = tex2d( GL_RGBA8, 4, 4 );
	R.R2 = fboWith( R.r2Tex );
	R.measTex = tex2d( GL_RGBA16F, 8, 8, GL_RGBA, GL_FLOAT );
	R.measFbo = fboWith( R.measTex );
	R.dTex = tex2d( GL_RGBA8, DW, DH );
	R.D = fboWith( R.dTex );
	R.pScene = link( kScene );
	R.pTaa = link( kTaa );
	R.pBloom = link( kBloom );
	R.pFog = link( kFog );
	R.pMeas = link( kMeas );
	R.pTone = link( kTone );
	R.pDraw = link( kDraw );
	const GLenum f1[3] = { GL_SRGB8_ALPHA8, GL_RGBA16F, GL_RGBA8 };
	for ( int i = 0; i < 3; i++ ) {
		gl->glGenTextures( 1, &R.id0[i] );
		fillCube( R.id0[i], GL_RGBA8, (unsigned char)( 40 + 50 * i ) );
		gl->glGenTextures( 1, &R.id1[i] );	// the loader's second cube (FO76's CubeMap2), its own GL name
		fillCube( R.id1[i], f1[i], (unsigned char)( 90 + 40 * i ) );
	}
	gl->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

static void destroy( Res & R )
{
	GLuint t[] = { R.vColor, R.lColor, R.r2Tex, R.measTex, R.dTex, R.ssrTex, R.taa.cur, R.taa.dep, R.bloom.resTex, R.bloom.mipTex,
		R.fog.tex, R.fog.depthTex, R.id0[0], R.id0[1], R.id0[2], R.id1[0], R.id1[1], R.id1[2] };
	gl->glDeleteTextures( GLsizei( sizeof t / sizeof t[0] ), t );
	GLuint f[] = { R.V, R.R2, R.L, R.ssrFbo, R.measFbo, R.D, R.taa.inFbo, R.bloom.srcFbo, R.bloom.resFbo, R.fog.fbo, R.fog.depthFbo };
	gl->glDeleteFramebuffers( GLsizei( sizeof f / sizeof f[0] ), f );
	gl->glDeleteRenderbuffers( 1, &R.vDs );
	gl->glDeleteVertexArrays( 1, &R.vao );
	for ( GLuint p : { R.pScene, R.pTaa, R.pBloom, R.pFog, R.pMeas, R.pTone, R.pDraw } )
		gl->glDeleteProgram( p );
}

static void enableIf( GLenum e, bool on )
{
	if ( on )
		gl->glEnable( e );
	else
		gl->glDisable( e );
}

static void tri() { gl->glDrawArrays( GL_TRIANGLES, 0, 3 ); }

// cellhdr.cpp GlSnap: measurePart + quadsExtra
struct Snap
{
	WwCellUni::KnownGl k;
	GLint depthFunc = GL_LESS, blendF[4] = {};
};
static void readSnap( CountFn & cf, Snap & s )
{
	cf.glGetIntegerv( GL_ACTIVE_TEXTURE, &s.k.active );
	cf.glGetIntegerv( GL_VIEWPORT, s.k.vp );
	cf.glGetFloatv( GL_COLOR_CLEAR_VALUE, s.k.clear );
	cf.glGetBooleanv( GL_COLOR_WRITEMASK, s.k.colorMask );
	s.k.depth = cf.glIsEnabled( GL_DEPTH_TEST );
	s.k.blend = cf.glIsEnabled( GL_BLEND );
	s.k.cull = cf.glIsEnabled( GL_CULL_FACE );
	s.k.stencil = cf.glIsEnabled( GL_STENCIL_TEST );
	s.k.scissor = cf.glIsEnabled( GL_SCISSOR_TEST );
	cf.glGetIntegerv( GL_STENCIL_FUNC, &s.k.sFunc );
	cf.glGetIntegerv( GL_STENCIL_REF, &s.k.sRef );
	cf.glGetIntegerv( GL_STENCIL_VALUE_MASK, &s.k.sMask );
	cf.glGetIntegerv( GL_STENCIL_WRITEMASK, &s.k.sWrite );
	cf.glGetIntegerv( GL_STENCIL_FAIL, &s.k.sFail );
	cf.glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &s.k.sZFail );
	cf.glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &s.k.sPass );
	cf.glGetBooleanv( GL_DEPTH_WRITEMASK, &s.k.depthMask );
	cf.glGetIntegerv( GL_DEPTH_FUNC, &s.depthFunc );
	cf.glGetIntegerv( GL_BLEND_SRC_RGB, &s.blendF[0] );
	cf.glGetIntegerv( GL_BLEND_DST_RGB, &s.blendF[1] );
	cf.glGetIntegerv( GL_BLEND_SRC_ALPHA, &s.blendF[2] );
	cf.glGetIntegerv( GL_BLEND_DST_ALPHA, &s.blendF[3] );
}

// restores the 19 fields KnownGl holds (bloom's and the measure's restore)
static void restoreAll( const WwCellUni::KnownGl & k, bool fbos = true, bool vp = true )
{
	gl->glActiveTexture( GLenum( k.active ) );
	if ( fbos ) {
		gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( k.draw ) );
		gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( k.read ) );
	}
	if ( vp )
		gl->glViewport( k.vp[0], k.vp[1], k.vp[2], k.vp[3] );
	gl->glClearColor( k.clear[0], k.clear[1], k.clear[2], k.clear[3] );
	gl->glColorMask( k.colorMask[0], k.colorMask[1], k.colorMask[2], k.colorMask[3] );
	gl->glDepthMask( k.depthMask );
	gl->glStencilFunc( GLenum( k.sFunc ), k.sRef, GLuint( k.sMask ) );
	gl->glStencilMask( GLuint( k.sWrite ) );
	gl->glStencilOp( GLenum( k.sFail ), GLenum( k.sZFail ), GLenum( k.sPass ) );
	enableIf( GL_STENCIL_TEST, k.stencil );
	enableIf( GL_DEPTH_TEST, k.depth );
	enableIf( GL_BLEND, k.blend );
	enableIf( GL_CULL_FACE, k.cull );
	if ( k.scissor )
		gl->glEnable( GL_SCISSOR_TEST );
}

static void passState( int unit )	// what a frame-end pass sets for its own draws
{
	gl->glDisable( GL_DEPTH_TEST );
	gl->glDepthMask( GL_FALSE );
	gl->glDisable( GL_BLEND );
	gl->glDisable( GL_CULL_FACE );
	gl->glDisable( GL_SCISSOR_TEST );
	gl->glDisable( GL_STENCIL_TEST );
	gl->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	gl->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
}

// cellssr.cpp wwCellSsrKeepFrame
static void ssrKeep( Res & R, CountFn & cf, int w, int h )
{
	GLint prevDraw = 0, prevRead = 0;
	if ( const WwCellUni::KnownGl * kn = WwCellUni::known( &cf ) ) {
		prevDraw = kn->draw;
		prevRead = kn->read;
	} else {
		cf.glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
		cf.glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	}
	if ( !R.ssrFbo ) {
		R.ssrTex = tex2d( GL_RGBA16F, 96, 64, GL_RGBA, GL_FLOAT );
		R.ssrFbo = fboWith( R.ssrTex );	// binds GL_FRAMEBUFFER (after the save, as ssrAlloc)
	}
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, R.L );
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, R.ssrFbo );
	gl->glBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
}

// gametaa.cpp taaResolveImpl on the linear frame
static bool taa( Res & R, CountFn & cf, const Cfg & c )
{
	const WwCellUni::KnownGl * kn = WwCellUni::known( &cf );
	WwCellUni::DepthCfg & depthCfg = R.depthCfg;
	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	if ( kn ) {
		prevDraw = kn->draw;
		prevRead = kn->read;
		std::memcpy( vp, kn->vp, sizeof vp );
	} else {
		cf.glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
		cf.glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
		cf.glGetIntegerv( GL_VIEWPORT, vp );
	}
	const int W = vp[2], H = vp[3];
	const bool cfgHit = kn && ( c.docDepth ? depthCfg.fbo >= 0 : depthCfg.hit( &R, prevDraw, W, H ) );
	const bool checkErr = !cfgHit || !depthCfg.trusted;
	if ( checkErr ) {
		while ( cf.glGetError() != GL_NO_ERROR ) {
		}
	}
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	if ( cfgHit ) {
		depthBits = depthCfg.depthBits;
		stencilBits = depthCfg.stencilBits;
		compType = depthCfg.compType;
	} else {
		const GLenum att = prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
		cf.glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
		cf.glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
			GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
		cf.glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
		if ( kn )
			depthCfg.keep( &R, prevDraw, W, H, depthBits, stencilBits, compType );
	}
	GLenum dsFormat = 0, dsFmt = 0, dsType = 0;
	GLbitfield blitBits = GL_DEPTH_BUFFER_BIT;
	if ( stencilBits == 8 && depthBits == 24 && compType != GL_FLOAT ) {
		dsFormat = GL_DEPTH24_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_UNSIGNED_INT_24_8;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	} else if ( stencilBits == 8 && depthBits == 32 && compType == GL_FLOAT ) {
		dsFormat = GL_DEPTH32F_STENCIL8; dsFmt = GL_DEPTH_STENCIL; dsType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		blitBits |= GL_STENCIL_BUFFER_BIT;
	}
	if ( !dsFormat )
		return false;
	auto & g = R.taa;
	if ( !g.inFbo || g.w != W || g.h != H || g.dsFormat != dsFormat ) {
		if ( g.cur ) {
			gl->glDeleteTextures( 1, &g.cur );
			gl->glDeleteTextures( 1, &g.dep );
		}
		if ( !g.inFbo )
			gl->glGenFramebuffers( 1, &g.inFbo );
		g.cur = tex2d( GL_RGBA16F, W, H, GL_RGBA, GL_FLOAT );
		g.dep = tex2d( dsFormat, W, H, dsFmt, dsType );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, g.inFbo );
		gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.cur, 0 );
		gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, g.dep, 0 );
		g.w = W;
		g.h = H;
		g.dsFormat = dsFormat;
	}
	GLint prevActive = 0;
	GLboolean colorMask[4], depthMask = GL_TRUE;
	bool wasDepth = false, wasBlend = false, wasCull = false, wasStencil = false, wasScissor = false;
	if ( kn ) {
		prevActive = kn->active;
		std::memcpy( colorMask, kn->colorMask, sizeof colorMask );
		depthMask = kn->depthMask;
		wasDepth = kn->depth, wasBlend = kn->blend, wasCull = kn->cull, wasStencil = kn->stencil, wasScissor = kn->scissor;
	} else {
		cf.glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		cf.glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		cf.glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		wasDepth = cf.glIsEnabled( GL_DEPTH_TEST ), wasBlend = cf.glIsEnabled( GL_BLEND );
		wasCull = cf.glIsEnabled( GL_CULL_FACE ), wasStencil = cf.glIsEnabled( GL_STENCIL_TEST );
		wasScissor = cf.glIsEnabled( GL_SCISSOR_TEST );
	}
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, R.L );
	gl->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	if ( !kn ) {
		GLint rb = 0;
		cf.glGetIntegerv( GL_READ_BUFFER, &rb );
		if ( rb == GL_NONE )
			gl->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	}
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
	gl->glDisable( GL_SCISSOR_TEST );
	gl->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	gl->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, blitBits, GL_NEAREST );
	const GLenum blitErr = checkErr ? cf.glGetError() : GLenum( GL_NO_ERROR );
	if ( kn && blitErr != GL_NO_ERROR )
		depthCfg.forget();
	else if ( kn && checkErr && depthCfg.hit( &R, prevDraw, W, H ) )
		depthCfg.trusted = true;
	passState( 20 );
	const bool ok = blitErr == GL_NO_ERROR;
	if ( ok ) {
		gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, R.L );
		gl->glBindTexture( GL_TEXTURE_2D, g.cur );
		gl->glActiveTexture( GL_TEXTURE21 );
		gl->glBindTexture( GL_TEXTURE_2D, g.dep );
		gl->glUseProgram( R.pTaa );
		gl->glUniform1i( gl->glGetUniformLocation( R.pTaa, "cur" ), 20 );
		gl->glUniform1i( gl->glGetUniformLocation( R.pTaa, "dep" ), 21 );
		tri();
		gl->glUseProgram( 0 );
	}
	for ( int u : { 20, 21 } ) {
		gl->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		gl->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	gl->glActiveTexture( GLenum( prevActive ) );
	enableIf( GL_STENCIL_TEST, wasStencil );
	enableIf( GL_DEPTH_TEST, wasDepth );
	enableIf( GL_BLEND, wasBlend );
	enableIf( GL_CULL_FACE, wasCull );
	if ( wasScissor )
		gl->glEnable( GL_SCISSOR_TEST );
	gl->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	gl->glDepthMask( depthMask );
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	return ok;
}

// cellhdr.cpp isMeasureFrame with the shared snap: its own target, then the snap restored
static void measure( Res & R, const Snap & sn, GLint prevDraw, GLint prevRead )
{
	gl->glBindFramebuffer( GL_FRAMEBUFFER, R.measFbo );
	gl->glViewport( 0, 0, 8, 8 );
	gl->glClearColor( 0.7f, 0.1f, 0.2f, 0.3f );
	passState( 25 );
	gl->glStencilFunc( GL_NEVER, 3, 0x0F );
	gl->glClear( GL_COLOR_BUFFER_BIT );
	gl->glBindTexture( GL_TEXTURE_2D, R.lColor );
	gl->glUseProgram( R.pMeas );
	gl->glUniform1i( gl->glGetUniformLocation( R.pMeas, "src" ), 25 );
	tri();
	gl->glUseProgram( 0 );
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	WwCellUni::KnownGl k = sn.k;
	k.draw = prevDraw;
	k.read = prevRead;
	restoreAll( k );
	gl->glDepthMask( sn.k.depthMask );
}

// cellbloom.cpp wwCellBloomRun: a resize frame binds its own framebuffer before the save (kept as it was)
static void bloom( Res & R, CountFn & cf, const Cfg & c, int w, int h, bool remake )
{
	while ( cf.glGetError() != GL_NO_ERROR ) {
	}
	auto & b = R.bloom;
	bool made = false;
	if ( !b.srcFbo || b.w != w || b.h != h || remake ) {	// remake: a mip count change (the fog bloom keeps its targets)
		made = true;
		if ( b.resTex ) {
			gl->glDeleteTextures( 1, &b.resTex );
			gl->glDeleteTextures( 1, &b.mipTex );
		}
		if ( !b.srcFbo ) {
			gl->glGenFramebuffers( 1, &b.srcFbo );
			gl->glGenFramebuffers( 1, &b.resFbo );
		}
		b.resTex = tex2d( GL_RGBA16F, w, h, GL_RGBA, GL_FLOAT );
		b.mipTex = tex2d( GL_RGBA16F, w / 2, h / 2, GL_RGBA, GL_FLOAT );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, b.srcFbo );
		gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b.mipTex, 0 );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, b.resFbo );
		gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b.resTex, 0 );
		b.w = w;
		b.h = h;
	}
	WwCellUni::KnownGl kg;
	if ( const WwCellUni::KnownGl * kn = made && !c.docMade ? nullptr : WwCellUni::known( &cf ) )
		kg = *kn;
	else
		WwCellUni::askAll( &cf, kg );
	if ( made && !c.docMade && !c.docNoClear )
		WwCellUni::knownSlot() = nullptr;	// the restore binds what was asked: the passes after it ask too
	passState( 23 );
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, R.L );
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, b.resFbo );
	gl->glBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	const GLenum e1 = cf.glGetError();
	gl->glBindFramebuffer( GL_FRAMEBUFFER, b.srcFbo );
	gl->glViewport( 0, 0, w / 2, h / 2 );
	gl->glClearColor( 0.3f, 0.2f, 0.1f, 0.0f );
	gl->glStencilFunc( GL_ALWAYS, 0, 0xFF );
	gl->glStencilMask( 0 );
	gl->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	gl->glClear( GL_COLOR_BUFFER_BIT );
	gl->glBindTexture( GL_TEXTURE_2D, b.resTex );
	gl->glUseProgram( R.pBloom );
	gl->glUniform1i( gl->glGetUniformLocation( R.pBloom, "src" ), 23 );
	tri();
	gl->glUseProgram( 0 );
	const GLenum e2 = cf.glGetError();
	(void)e1;
	(void)e2;
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	restoreAll( kg );
}

// cellfogbloom.cpp wwCellFogBloomRun: the same order (its depth framebuffer bound before the save on a resize frame)
static void fogBloom( Res & R, CountFn & cf, const Cfg & c, int w, int h, GLenum ds )
{
	while ( cf.glGetError() != GL_NO_ERROR ) {
	}
	auto & g = R.fog;
	bool made = false;
	if ( !g.fbo || g.w != w || g.h != h || g.ds != ds ) {
		made = true;
		if ( !g.fbo ) {
			gl->glGenFramebuffers( 1, &g.fbo );
			gl->glGenFramebuffers( 1, &g.depthFbo );
		}
		if ( g.tex ) {
			gl->glDeleteTextures( 1, &g.tex );
			gl->glDeleteTextures( 1, &g.depthTex );
		}
		g.depthTex = tex2d( ds, w, h, GL_DEPTH_STENCIL, ds == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8 );
		g.tex = tex2d( GL_RGBA8, w, h );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.tex, 0 );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, g.depthFbo );
		gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, g.depthTex, 0 );
		g.w = w;
		g.h = h;
		g.ds = ds;
	}
	GLint prevDraw = 0, prevRead = 0, prevActive = 0;
	GLfloat clear[4];
	GLboolean colorMask[4], depthMask = GL_TRUE;
	GLint stFunc = GL_ALWAYS, stRef = 0, stValMask = 0xFF, stWriteMask = 0xFF, stFail = GL_KEEP, stZFail = GL_KEEP, stZPass = GL_KEEP;
	bool wasDepth = false, wasBlend = false, wasCull = false, wasStencil = false, wasScissor = false;
	if ( const WwCellUni::KnownGl * kn = made && !c.docMade ? nullptr : WwCellUni::known( &cf ) ) {
		prevDraw = kn->draw;
		prevRead = kn->read;
		prevActive = kn->active;
		std::memcpy( clear, kn->clear, sizeof clear );
		std::memcpy( colorMask, kn->colorMask, sizeof colorMask );
		depthMask = kn->depthMask;
		stFunc = kn->sFunc, stRef = kn->sRef, stValMask = kn->sMask, stWriteMask = kn->sWrite;
		stFail = kn->sFail, stZFail = kn->sZFail, stZPass = kn->sPass;
		wasDepth = kn->depth, wasBlend = kn->blend, wasCull = kn->cull, wasStencil = kn->stencil, wasScissor = kn->scissor;
	} else {
		cf.glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
		cf.glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
		cf.glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		cf.glGetFloatv( GL_COLOR_CLEAR_VALUE, clear );
		cf.glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		cf.glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		cf.glGetIntegerv( GL_STENCIL_FUNC, &stFunc );
		cf.glGetIntegerv( GL_STENCIL_REF, &stRef );
		cf.glGetIntegerv( GL_STENCIL_VALUE_MASK, &stValMask );
		cf.glGetIntegerv( GL_STENCIL_WRITEMASK, &stWriteMask );
		cf.glGetIntegerv( GL_STENCIL_FAIL, &stFail );
		cf.glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &stZFail );
		cf.glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &stZPass );
		wasDepth = cf.glIsEnabled( GL_DEPTH_TEST ), wasBlend = cf.glIsEnabled( GL_BLEND );
		wasCull = cf.glIsEnabled( GL_CULL_FACE ), wasStencil = cf.glIsEnabled( GL_STENCIL_TEST );
		wasScissor = cf.glIsEnabled( GL_SCISSOR_TEST );
	}
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, R.V );
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.depthFbo );
	gl->glBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
	(void)cf.glGetError();
	passState( 24 );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	gl->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	gl->glStencilFunc( GL_NOTEQUAL, 0, 0x7F );
	gl->glStencilMask( 0 );
	gl->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	gl->glClear( GL_COLOR_BUFFER_BIT );
	gl->glBindTexture( GL_TEXTURE_2D, g.depthTex );
	gl->glUseProgram( R.pFog );
	gl->glUniform1i( gl->glGetUniformLocation( R.pFog, "dep" ), 24 );
	tri();
	gl->glUseProgram( 0 );
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	gl->glActiveTexture( GLenum( prevActive ) );
	gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	gl->glClearColor( clear[0], clear[1], clear[2], clear[3] );
	gl->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	gl->glDepthMask( depthMask );
	gl->glStencilFunc( GLenum( stFunc ), stRef, GLuint( stValMask ) );
	gl->glStencilMask( GLuint( stWriteMask ) );
	gl->glStencilOp( GLenum( stFail ), GLenum( stZFail ), GLenum( stZPass ) );
	enableIf( GL_STENCIL_TEST, wasStencil );
	enableIf( GL_DEPTH_TEST, wasDepth );
	enableIf( GL_BLEND, wasBlend );
	enableIf( GL_CULL_FACE, wasCull );
	if ( wasScissor )
		gl->glEnable( GL_SCISSOR_TEST );
}

// the tone quads: into whatever framebuffer is bound, then cellhdr's final restore from the snap
static void tone( Res & R, const Snap & sn )
{
	gl->glDisable( GL_DEPTH_TEST );
	gl->glDepthMask( GL_FALSE );
	gl->glDisable( GL_BLEND );
	gl->glDisable( GL_CULL_FACE );
	gl->glDisable( GL_SCISSOR_TEST );
	gl->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	const GLuint t[3] = { R.lColor, R.bloom.mipTex, R.fog.tex };
	for ( int i = 0; i < 3; i++ ) {
		gl->glActiveTexture( GLenum( GL_TEXTURE26 + i ) );
		gl->glBindTexture( GL_TEXTURE_2D, t[i] );
	}
	gl->glUseProgram( R.pTone );
	gl->glUniform1i( gl->glGetUniformLocation( R.pTone, "hdr" ), 26 );
	gl->glUniform1i( gl->glGetUniformLocation( R.pTone, "bloom" ), 27 );
	gl->glUniform1i( gl->glGetUniformLocation( R.pTone, "fog" ), 28 );
	tri();
	gl->glUseProgram( 0 );
	for ( int i = 0; i < 3; i++ ) {
		gl->glActiveTexture( GLenum( GL_TEXTURE26 + i ) );
		gl->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	gl->glActiveTexture( GLenum( sn.k.active ) );
	enableIf( GL_STENCIL_TEST, sn.k.stencil );
	enableIf( GL_DEPTH_TEST, sn.k.depth );
	enableIf( GL_BLEND, sn.k.blend );
	enableIf( GL_CULL_FACE, sn.k.cull );
	if ( sn.k.scissor )
		gl->glEnable( GL_SCISSOR_TEST );
	gl->glColorMask( sn.k.colorMask[0], sn.k.colorMask[1], sn.k.colorMask[2], sn.k.colorMask[3] );
	gl->glDepthMask( sn.k.depthMask );
	gl->glDepthFunc( GLenum( sn.depthFunc ) );
	gl->glBlendFuncSeparate( GLenum( sn.blendF[0] ), GLenum( sn.blendF[1] ), GLenum( sn.blendF[2] ), GLenum( sn.blendF[3] ) );
}

// ---- the census (renderer.cpp DrawGlCache) and the cube decode mode (renderer.cpp wwCellCubeDecodeMode) ----
static int dgBlend = -1;
static bool drawBlend( CountFn & cf )
{
	if ( dgBlend < 0 )
		dgBlend = cf.glIsEnabled( GL_BLEND ) ? 1 : 0;
	return dgBlend != 0;
}

static void census( Res & R, CountFn & cf, const Cfg & c, int frame )
{
	using namespace WwCellUni;
	const GLint aoLoc = gl->glGetUniformLocation( R.pDraw, "aoSeen" ), bLoc = gl->glGetUniformLocation( R.pDraw, "blendSeen" );
	gl->glUniform4f( gl->glGetUniformLocation( R.pDraw, "cellCubeMat" ), 0.0f, 0.0f, 0.0f, 0.0f );
	for ( int d = 0; d < kCensus; d++ ) {
		if ( d % 7 == 3 )	// a pass between two draws flips blend and notes nothing
			gl->glEnable( GL_BLEND );
		else if ( d % 7 == 5 )
			gl->glDisable( GL_BLEND );
		forgetBlend();	// setupProgram's entry
		const int kind = ( d + frame ) % 4;
		if ( kind == 0 ) {
			gl->glEnable( GL_BLEND );
			noteBlend( true );
		} else if ( kind <= 2 ) {
			gl->glDisable( GL_BLEND );
			noteBlend( false );
		}	// kind 3: a route that writes no blend
		dgBlend = -1;	// WwDrawGlScope: seeded from the note (CELLUNI3)
		if ( blendNote().on >= 0 && !c.red )
			dgBlend = blendNote().on;
		const bool ao = drawBlend( cf );	// the AO hook
		if ( d % 3 == 0 ) {	// cellssr.cpp's water-ray pass: the water draws unblended
			gl->glDisable( GL_BLEND );
			if ( c.docBlend )
				dgBlend = 1;
			else
				dgBlend = c.blendKnown && !c.red ? 0 : -1;	// wwDrawGlCacheBlend( false ) / wwDrawGlCacheDirty()
		}
		const bool bl = drawBlend( cf );	// the reflection probe / SSGI hooks after it
		gl->glUniform1i( aoLoc, ao ? 1 : 0 );
		gl->glUniform1i( bLoc, bl ? 1 : 0 );
		gl->glScissor( d % DW, d / DW, 1, 1 );
		tri();
	}
	gl->glDisable( GL_BLEND );
}

static void cubes( Res & R, CountFn & cf, const Cfg & c )
{
	const GLint loc = gl->glGetUniformLocation( R.pDraw, "cellCubeMat" );
	gl->glUniform1i( gl->glGetUniformLocation( R.pDraw, "aoSeen" ), 0 );
	gl->glUniform1i( gl->glGetUniformLocation( R.pDraw, "blendSeen" ), 0 );
	auto bindCube = [&]( int t, bool second ) {	// gltex.cpp TexCache::bindCube
		if ( c.cube2 )
			R.cubeBound = 0;
		const GLuint id = second ? R.id1[t] : R.id0[t];
		gl->glBindTexture( GL_TEXTURE_CUBE_MAP, id );
		R.cubeBound = c.cube2 ? id : ( !second ? R.id0[t] : 0 );
	};
	for ( int d = 0; d < kCube; d++ ) {
		const int t = ( d / 2 ) % 3;
		gl->glActiveTexture( GL_TEXTURE8 );
		bindCube( t, false );	// CubeMap
		gl->glActiveTexture( GL_TEXTURE9 );
		bindCube( t, true );	// FO76: CubeMap2, bound after it
		// the decode mode: prev passed no cache for FO76 (nifVersion >= 151); the red never takes one
		const GLuint id = c.cube2 && !c.red ? R.cubeBound : 0;
		int mode = id && id < R.cubeCache.size() ? R.cubeCache[id] : 0;
		if ( !mode ) {
			GLint tex = GLint( id );
			if ( !id )
				cf.glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &tex );
			GLint f = 0;
			if ( tex )
				cf.glGetTexLevelParameteriv( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_INTERNAL_FORMAT, &f );
			mode = !tex ? 1 : ( f != 0x8C40 && f != 0x8C41 && f != 0x8C42 && f != 0x8C43 && ( f < 0x8C48 || f > 0x8C4F ) && f != 0x8E8D ) ? 1 : 3;
			if ( id ) {
				if ( id >= R.cubeCache.size() )
					R.cubeCache.resize( id + 64 );
				R.cubeCache[id] = mode;
			}
		}
		gl->glUniform4f( loc, 0.5f, 0.25f, float( mode ), 1.0f );
		const int px = kCensus + d;
		gl->glScissor( px % DW, px / DW, 1, 1 );
		tri();
	}
	gl->glActiveTexture( GL_TEXTURE0 );
}

struct FrameOut
{
	std::vector<unsigned char> px;
	std::vector<GLint> st;
	long long endQ = 0, ssrQ = 0, taaQ = 0, bloomQ = 0, fogQ = 0, censusQ = 0, cubeQ = 0;
	double endMs = 0;
	bool taaOk = false;
};

static std::vector<GLint> snapState()
{
	std::vector<GLint> s;
	auto geti = [&]( GLenum e, int n ) {
		GLint v[4] = {};
		gl->glGetIntegerv( e, v );
		for ( int i = 0; i < n; i++ )
			s.push_back( v[i] );
	};
	for ( GLenum e : { GL_DRAW_FRAMEBUFFER_BINDING, GL_READ_FRAMEBUFFER_BINDING, GL_ACTIVE_TEXTURE, GL_STENCIL_FUNC, GL_STENCIL_REF,
			GL_STENCIL_VALUE_MASK, GL_STENCIL_WRITEMASK, GL_STENCIL_FAIL, GL_STENCIL_PASS_DEPTH_FAIL, GL_STENCIL_PASS_DEPTH_PASS,
			GL_DEPTH_FUNC, GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA, GL_BLEND_DST_ALPHA } )
		geti( e, 1 );
	geti( GL_VIEWPORT, 4 );
	GLfloat cc[4];
	gl->glGetFloatv( GL_COLOR_CLEAR_VALUE, cc );
	for ( float f : cc ) {
		GLint b = 0;
		std::memcpy( &b, &f, 4 );
		s.push_back( b );
	}
	GLboolean cm[4], dm = 0;
	gl->glGetBooleanv( GL_COLOR_WRITEMASK, cm );
	gl->glGetBooleanv( GL_DEPTH_WRITEMASK, &dm );
	for ( GLboolean b : cm )
		s.push_back( b );
	s.push_back( dm );
	for ( GLenum e : { GL_DEPTH_TEST, GL_BLEND, GL_CULL_FACE, GL_STENCIL_TEST, GL_SCISSOR_TEST } )
		s.push_back( gl->glIsEnabled( e ) );
	GLint act = 0;
	gl->glGetIntegerv( GL_ACTIVE_TEXTURE, &act );
	for ( int u = 0; u < 32; u++ ) {
		gl->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		geti( GL_TEXTURE_BINDING_2D, 1 );
		geti( GL_TEXTURE_BINDING_CUBE_MAP, 1 );
	}
	gl->glActiveTexture( GLenum( act ) );
	return s;
}

static void readInto( std::vector<unsigned char> & px, GLuint fbo, int w, int h )
{
	const size_t at = px.size();
	px.resize( at + size_t( w * h * 4 ) );
	gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	gl->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	gl->glReadPixels( 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data() + at );
}

static void sizeOf( int f, int & w, int & h, GLenum & ds )
{
	const bool b = f >= 6;
	w = b ? 80 : 96;
	h = b ? 48 : 64;
	ds = b ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8;
}

static std::vector<FrameOut> run( Mode mode, int frames, long long * staleOut = nullptr )
{
	using namespace WwCellUni;
	const Cfg c = cfgOf( mode );
	state() = State();
	state().vpCheck = c.check;
	knownSlot() = nullptr;
	Res R;
	build( R );
	CountFn cf;
	cf.f = gl;
	std::vector<FrameOut> out;
	for ( int f = 0; f < frames; f++ ) {
		FrameOut o;
		int w = 0, h = 0;
		GLenum ds = 0;
		sizeOf( f, w, h, ds );
		if ( f == 0 || f == 6 )
			allocView( R, w, h, ds );	// the view (re)made at another size: its depth turns D32F_S8, the same framebuffer name
		if ( f == 3 ) {	// CubeMap2 of texture 0 reloaded under the same id with another format (gltex.cpp loadTex)
			fillCube( R.id1[0], GL_RGBA16F, 77 );
			for ( GLuint id : { R.id0[0], R.id1[0] } ) {
				const bool forget = id == R.id0[0] || ( c.cube2 && !c.docCube );	// before CELLUNI4 only id[0] was forgotten
				if ( forget && id < R.cubeCache.size() )
					R.cubeCache[id] = 0;
			}
		}
		// the frame's own content
		gl->glBindVertexArray( R.vao );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.L );
		gl->glViewport( 0, 0, w, h );
		for ( GLenum e : { GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE } )
			gl->glDisable( e );
		gl->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		gl->glDepthMask( GL_TRUE );
		gl->glStencilMask( 0xFF );
		gl->glUseProgram( R.pScene );
		gl->glUniform1i( gl->glGetUniformLocation( R.pScene, "frame" ), f );
		tri();
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.V );
		gl->glClearDepthf( 0.2f + 0.1f * float( f % 5 ) );
		gl->glClearStencil( 1 );
		gl->glClearColor( 0.1f, 0.2f, 0.3f, 1.0f );
		gl->glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		// the draws: a water-ray pass's census, FO76 cubes
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.D );
		gl->glViewport( 0, 0, DW, DH );
		gl->glClearColor( 0, 0, 0, 0 );
		gl->glClear( GL_COLOR_BUFFER_BIT );
		gl->glEnable( GL_SCISSOR_TEST );
		gl->glBlendFunc( GL_ONE, GL_ONE );
		gl->glUseProgram( R.pDraw );
		long long q0 = cf.n;
		census( R, cf, c, f );
		o.censusQ = cf.n - q0;
		q0 = cf.n;
		cubes( R, cf, c );
		o.cubeQ = cf.n - q0;
		gl->glUseProgram( 0 );
		// the state the frame end meets (cellhdr's End: DRAW the view, READ the frame's read framebuffer)
		gl->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, R.V );
		gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, R.R2 );
		gl->glViewport( 0, 0, w, h );
		gl->glActiveTexture( GLenum( GL_TEXTURE0 + f % 5 ) );
		gl->glClearColor( 0.01f * float( f ), 0.5f, 0.25f, 1.0f );
		gl->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, f % 4 != 2 );
		gl->glDepthMask( f % 2 ? GL_TRUE : GL_FALSE );
		enableIf( GL_DEPTH_TEST, f % 3 == 0 );
		gl->glDepthFunc( f % 2 ? GL_LEQUAL : GL_LESS );
		enableIf( GL_BLEND, f % 2 == 1 );
		gl->glBlendFuncSeparate( GL_SRC_ALPHA, f % 2 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO );
		enableIf( GL_CULL_FACE, f % 2 == 0 );
		enableIf( GL_STENCIL_TEST, f % 3 == 1 );
		gl->glStencilFunc( f % 2 ? GL_EQUAL : GL_ALWAYS, 1, GLuint( 0xFF - f % 3 ) );
		gl->glStencilMask( GLuint( 0xF0 | ( f % 16 ) ) );
		gl->glStencilOp( GL_KEEP, f % 2 ? GL_INCR : GL_KEEP, f % 3 ? GL_REPLACE : GL_KEEP );
		enableIf( GL_SCISSOR_TEST, f % 4 == 3 );
		gl->glScissor( 2, 2, w - 4, h - 4 );
		gl->glFinish();
		// ---- wwCellHdrEnd from the depth blit on ----
		const long long e0 = cf.n;
		QElapsedTimer t;
		t.start();
		Snap sn;
		if ( c.snEarly )
			readSnap( cf, sn );
		WwCellUni::KnownGl known = sn.k;
		known.draw = GLint( R.V );
		known.read = GLint( R.R2 );
		if ( c.docKnownWrong )
			known.clear[0] += 0.5f;
		knownSlot() = c.known && !c.red ? &known : nullptr;
		q0 = cf.n;
		ssrKeep( R, cf, w, h );
		o.ssrQ = cf.n - q0;
		q0 = cf.n;
		o.taaOk = taa( R, cf, c );
		o.taaQ = cf.n - q0;
		if ( !c.snEarly )
			readSnap( cf, sn );
		measure( R, sn, GLint( R.V ), GLint( R.R2 ) );
		q0 = cf.n;
		bloom( R, cf, c, w, h, f == 12 );
		o.bloomQ = cf.n - q0;
		q0 = cf.n;
		fogBloom( R, cf, c, w, h, ds );
		o.fogQ = cf.n - q0;
		knownSlot() = nullptr;
		tone( R, sn );
		o.endMs = double( t.nsecsElapsed() ) / 1e6;
		o.endQ = cf.n - e0;
		gl->glFinish();
		o.st = snapState();
		readInto( o.px, R.V, w, h );
		readInto( o.px, R.L, w, h );
		readInto( o.px, R.D, DW, DH );
		readInto( o.px, R.bloom.resFbo, w, h );
		out.push_back( std::move( o ) );
	}
	if ( staleOut )
		*staleOut = (long long)state().knownStale;
	gl->glBindVertexArray( 0 );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	destroy( R );
	state() = State();
	return out;
}

static double median( std::vector<double> v )
{
	std::sort( v.begin(), v.end() );
	return v.empty() ? 0.0 : v[v.size() / 2];
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const int frames = argc > 1 ? std::max( 10, std::atoi( argv[1] ) ) : 30;

	QGuiApplication app( argc, argv );
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 6 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	QOffscreenSurface surf;
	surf.setFormat( fmt );
	surf.create();
	QOpenGLContext ctx;
	ctx.setFormat( fmt );
	if ( !ctx.create() || !ctx.makeCurrent( &surf ) ) {
		std::printf( "FAIL no GL 4.6 context\n" );
		return 2;
	}
	gl = ctx.extraFunctions();
	std::printf( "GL %s | %s\n", (const char *)gl->glGetString( GL_VERSION ), (const char *)gl->glGetString( GL_RENDERER ) );

	// the kit itself
	{
		using namespace WwCellUni;
		DepthCfg d;
		int key = 0;
		const bool cold = !d.hit( &key, 5, 96, 64 );
		d.keep( &key, 5, 96, 64, 24, 8, 0 );
		const bool warm = d.hit( &key, 5, 96, 64 ) && !d.trusted;
		const bool sized = !d.hit( &key, 5, 80, 48 ) && !d.hit( &key, 6, 96, 64 ) && !d.hit( nullptr, 5, 96, 64 );
		d.trusted = true;
		d.forget();
		check( cold && warm && sized && !d.hit( &key, 5, 96, 64 ) && !d.trusted,
			"depth cfg: a hit only on the same context, framebuffer and size; keep() and forget() drop the trust" );
		KnownGl a, b;
		b.sZFail = 0x1E01;
		check( same( a, a ) && !same( a, b ), "known state: same() compares every field" );
		check( knownSlot() == nullptr, "known state: nothing installed outside the frame end" );
	}

	long long staleGreenCheck = -1, staleDoc = -1;
	const std::vector<FrameOut> red = run( Red, frames );
	const std::vector<FrameOut> prev = run( Prev, frames );
	const std::vector<FrameOut> green = run( Green, frames );
	const std::vector<FrameOut> greenCheck = run( GreenCheck, frames, &staleGreenCheck );
	const std::vector<FrameOut> knownWrong = run( DocKnownWrongChecked, frames, &staleDoc );
	std::vector<std::vector<FrameOut>> docs;
	for ( Mode m : { DocDepthNoKey, DocKnownOnMade, DocCubeNoForget, DocWrongBlend, DocNoClear } )
		docs.push_back( run( m, frames ) );
	std::vector<double> tr, tp, tg;	// interleaved, so drift hits all three
	for ( int k = 0; k < 3; k++ ) {
		for ( const auto & o : run( Red, frames ) )
			tr.push_back( o.endMs );
		for ( const auto & o : run( Prev, frames ) )
			tp.push_back( o.endMs );
		for ( const auto & o : run( Green, frames ) )
			tg.push_back( o.endMs );
	}

	auto differ = [&]( const std::vector<FrameOut> & a, bool state ) {
		int n = 0;
		for ( int f = 0; f < frames; f++ )
			n += state ? red[f].st != a[f].st : red[f].px != a[f].px;
		return n;
	};
	auto line = []( const char * who, const FrameOut & o ) {
		std::printf( "  %-12s frame end queries %4lld (ssr keep %lld, taa %lld, bloom %lld, fog bloom %lld) | census %lld | cube %lld\n",
			who, o.endQ, o.ssrQ, o.taaQ, o.bloomQ, o.fogQ, o.censusQ, o.cubeQ );
	};
	for ( int f : { 0, 1, 5, 6, 7, frames - 1 } ) {
		std::printf( "frame %d%s\n", f, f == 0 || f == 6 ? " (the targets made)" : "" );
		line( "red", red[f] );
		line( "prev", prev[f] );
		line( "green", green[f] );
	}
	bool taaAll = true;
	for ( int f = 0; f < frames; f++ )
		taaAll = taaAll && red[f].taaOk;
	const int pxG = differ( green, false ), stG = differ( green, true ), pxP = differ( prev, false ), stP = differ( prev, true );
	const int pxC = differ( greenCheck, false ), stC = differ( greenCheck, true ), pxK = differ( knownWrong, false );
	std::printf( "pixels/state differ vs red: green %d/%d, prev %d/%d, green-check %d/%d frames (of %d)\n", pxG, stG, pxP, stP, pxC, stC,
		frames );
	int docPx[5];
	for ( int i = 0; i < 5; i++ ) {
		docPx[i] = differ( docs[size_t( i )], false );
		std::printf( "doctored %-24s pixels differ %d frames\n", kModeName[DocDepthNoKey + i], docPx[i] );
	}
	std::printf( "knownstale: green-check %lld, a doctored wrong known state under the check %lld (its pixels differ %d frames)\n",
		staleGreenCheck, staleDoc, pxK );
	const double mr = median( tr ), mp = median( tp ), mg = median( tg );
	std::printf( "frame end cpu ms (median of %zu): red %.4f prev %.4f green %.4f (green vs prev %.1f%%)\n", tr.size(), mr, mp, mg,
		mp > 0 ? 100.0 * ( mg - mp ) / mp : 0.0 );

	check( taaAll, "red: the temporal AA resolved every frame (its depth blit raised no error)" );
	check( pxG == 0 && stG == 0, "green == red: pixels and state byte for byte, every frame" );
	check( pxP == 0 && stP == 0, "prev (main before the lane) == red" );
	check( pxC == 0 && stC == 0 && staleGreenCheck == 0, "WW_SETUP_MEMO_CHECK=1: green == red and no known state found wrong" );
	check( staleDoc > 0 && pxK == 0, "control: a wrong known state under the check is counted (knownstale) and repaired" );
	check( docPx[0] > 0, "control: the depth cache keyed without the size (no forget at the resize) changes the picture" );
	check( docPx[1] > 0, "control: the known state taken on a frame that made its targets changes the picture" );
	check( docPx[2] > 0, "control: a CubeMap2 reload without its forget changes the picture" );
	check( docPx[3] > 0, "control: a wrong known blend after the water-ray hook changes the picture" );
	check( docPx[4] > 0, "control: a bloom-only remake (frame 12) that leaves the known state installed changes the picture" );
	const FrameOut & g = green[frames - 1];
	const FrameOut & p = prev[frames - 1];
	const FrameOut & r = red[frames - 1];
	check( g.taaQ == 0 && g.ssrQ == 0 && green[7].taaQ == 0, "green: the TAA and the SSR keep ask nothing once trusted (frame 7 on, after the resize)" );
	check( green[6].taaQ > 0 && green[6].taaQ < p.taaQ, "green: the resize frame asks the new depth format (and reads its errors) only" );
	check( g.bloomQ <= 3 && g.fogQ <= 2, "green: the bloom and fog bloom ask only their error reads" );
	check( green[6].bloomQ > 19 && green[6].fogQ > 18, "green: a frame that made the targets asks as before (the kept bug)" );
	check( g.endQ * 2 < p.endQ, "green: the frame end asks under half as often as prev" );
	check( r.endQ >= p.endQ, "red asks at least as often as prev" );
	check( g.censusQ < p.censusQ, "green: the water-ray draws ask no blend" );
	check( g.cubeQ == 0 && p.cubeQ >= 2 * kCube, "green: FO76's CubeMap2 decode mode asked once per texture (prev: twice per draw)" );
	std::printf( "%s %d failed\n", fails ? "CELLUNI4 FAIL" : "CELLUNI4 PASS", fails );
	ctx.doneCurrent();
	return fails ? 1 : 0;
}
