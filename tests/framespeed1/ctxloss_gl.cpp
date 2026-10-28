/* tests/framespeed1 (lane FRAMESPEED1): the TAA's depth-format cache (WwCellUni::DepthCfg, gametaa.cpp) across a context
 * loss, through the real driver. Context A: framebuffer f with D24S8 at 96x64, the cache keeps it. A is destroyed and B is
 * made; B's first framebuffer gets the same id f, now D32F_S8, same size. The cache is keyed on (renderer, fbo, size),
 * so without the context watch it still answers 24 bits: the depth blit then fails (format mismatch) every frame.
 * Green: wwGlContextSerial() differs for B, CtxWatch says changed, the cache forgets and asks: 32 bits float.
 * -DCTXLOSS_RED: no watch (the code before this lane): must print CTXLOSS FAIL. Usage: ctxloss_gl   exit 0 = ok */
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>

#include <cstdio>
#include <memory>

#include "gl/celluniform.h"
#include "gl/glctxserial.h"

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

struct Made { std::unique_ptr<QOpenGLContext> ctx; GLuint fbo = 0, rb = 0; };

static Made makeCtx( QOffscreenSurface & surf, GLenum dsFormat )
{
	Made m;
	m.ctx = std::make_unique<QOpenGLContext>();
	m.ctx->setFormat( surf.format() );
	m.ctx->create();
	m.ctx->makeCurrent( &surf );
	QOpenGLExtraFunctions * f = m.ctx->extraFunctions();
	f->glGenRenderbuffers( 1, &m.rb );
	f->glBindRenderbuffer( GL_RENDERBUFFER, m.rb );
	f->glRenderbufferStorage( GL_RENDERBUFFER, dsFormat, 96, 64 );
	f->glGenFramebuffers( 1, &m.fbo );
	f->glBindFramebuffer( GL_FRAMEBUFFER, m.fbo );
	f->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m.rb );
	return m;
}

// gametaa.cpp taaResolveImpl's cache step, as the lane leaves it (the red skips the watch)
static int depthOf( const void * owner, WwCellUni::DepthCfg & d, WwCellUni::CtxWatch & w, GLuint fbo, int * asks )
{
	const bool changed = w.changed( owner, wwGlContextSerial() );
#ifndef CTXLOSS_RED
	if ( changed && d.ctx == owner )
		d.forget();
#else
	( void ) changed;
#endif
	if ( d.hit( owner, int( fbo ), 96, 64 ) )
		return d.depthBits;
	QOpenGLExtraFunctions * f = QOpenGLContext::currentContext()->extraFunctions();
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	f->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
	f->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	f->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	f->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
	d.keep( owner, int( fbo ), 96, 64, depthBits, stencilBits, compType );
	( *asks )++;
	return depthBits;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	QGuiApplication app( argc, argv );
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 6 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	QOffscreenSurface surf;
	surf.setFormat( fmt );
	surf.create();
	const int renderer = 0;	// the Renderer pointer: the same object across the loss
	WwCellUni::DepthCfg d;
	WwCellUni::CtxWatch w;
	int asks = 0;

	Made a = makeCtx( surf, GL_DEPTH24_STENCIL8 );
	const std::uint64_t sa = wwGlContextSerial();
	const int a1 = depthOf( &renderer, d, w, a.fbo, &asks );
	const int a2 = depthOf( &renderer, d, w, a.fbo, &asks );
	check( a1 == 24 && a2 == 24 && asks == 1, "context A: 24 bits, asked once over two frames" );
	check( wwGlContextSerial() == sa && sa != 0, "context A: its serial holds" );
	const GLuint fa = a.fbo;
	a.ctx->doneCurrent();
	a.ctx.reset();	// the context loss: every object of A is gone

	Made b = makeCtx( surf, GL_DEPTH32F_STENCIL8 );
	const std::uint64_t sb = wwGlContextSerial();
	std::printf( "fbo ids: A %u B %u; serials A %llu B %llu\n", fa, b.fbo, (unsigned long long) sa, (unsigned long long) sb );
	check( b.fbo == fa, "context B reuses the framebuffer id (the case the cache keys cannot see)" );
	check( sb != sa, "context B has a new serial" );
	const int b1 = depthOf( &renderer, d, w, b.fbo, &asks );
	check( b1 == 32 && asks == 2, "context B: the cache asks again and reads 32 bits" );
	const int b2 = depthOf( &renderer, d, w, b.fbo, &asks );
	check( b2 == 32 && asks == 2, "context B: then the cache holds again (no ask on the next frame)" );
	b.ctx->doneCurrent();
	std::printf( "CTXLOSS %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
