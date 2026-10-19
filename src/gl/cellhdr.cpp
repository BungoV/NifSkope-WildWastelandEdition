#include "cellhdr.h"
#include "cellunits.h"

#include "gl/cellbloom.h"
#include "gl/celllights.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QDebug>
#include <QFile>
#include <QHash>

#include <algorithm>
#include <cstdint>
#include <vector>

// lane HDR1: the cell view's linear frame and its one tone map (cellhdr.h)

namespace {

constexpr int kSrcUnit = 8;		// the linear frame (sampler2DMS) in cell_hdr.prog; the cell units are 10..16
constexpr int kFoBloomUnit = WwCellUnit::Bloom;	// lane BLOOM1b: the FO4CS chain's top (sampler2D), on the vanilla bloom's unit (same type)

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

/* lane VOLAIR1 (gates only): WW_CELL_DRAW_PROBE=x,y[;x,y...] (picture pixels, row 0 at the top) watches those pixels
 * of whatever float target a draw goes to (the linear frame, the obscurance pass's g-buffer) and, with WW_CELL_HDR_DUMP
 * set, writes <dump>.probe once a frame: every draw that changed one, with the target and the draw's depth test, depth
 * write and blend state. Names the draw behind a pixel. */
struct DrawProbe
{
	std::vector<int> xy;	// picture coordinates, top row 0
	QHash<GLint, std::vector<float>> last;	// per target
	QHash<GLint, std::vector<float>> lastZ;	// per target: window depth (-1 where it cannot be read, multisampled)
	QByteArray log;
	GLuint fbo = 0, tex = 0;
	GLuint zfbo = 0, ztex = 0;	// the multisampled frame's depth, resolved for the read
	GLenum zfmt = 0;
	int w = 0, h = 0, zw = 0, zh = 0;
	int on = -1;
};

DrawProbe & dprobe()
{
	static DrawProbe p;
	if ( p.on < 0 ) {
		p.on = 0;
		const QByteArray spec = qgetenv( "WW_CELL_DRAW_PROBE" );
		if ( !spec.isEmpty() && !qgetenv( "WW_CELL_HDR_DUMP" ).isEmpty() ) {
			for ( const QByteArray & pt : spec.split( ';' ) ) {
				const QList<QByteArray> v = pt.split( ',' );
				if ( v.size() == 2 ) {
					p.xy.push_back( v[0].trimmed().toInt() );
					p.xy.push_back( v[1].trimmed().toInt() );
				}
			}
			p.on = p.xy.empty() ? 0 : 1;
		}
	}
	return p;
}

// the watched pixels of the bound draw target (W x H, bottom row 0 in GL); false when it cannot be read as float
bool probeRead( Renderer * r, GLint src, int W, int H, DrawProbe & p, std::vector<float> & out )
{
	for ( size_t i = 0; i + 1 < p.xy.size(); i += 2 ) {
		if ( p.xy[i] < 0 || p.xy[i] >= W || p.xy[i + 1] < 0 || p.xy[i + 1] >= H )
			return false;	// a smaller target (a shadow map, a probe face): not the picture's
	}
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	if ( !p.fbo || p.w != W || p.h != H ) {
		if ( !p.fbo )
			fn->glGenFramebuffers( 1, &p.fbo );
		if ( p.tex )
			fn->glDeleteTextures( 1, &p.tex );
		fn->glGenTextures( 1, &p.tex );
		fn->glBindTexture( GL_TEXTURE_2D, p.tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, W, H, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		p.w = W;
		p.h = H;
	}
	GLint prevRead = 0;
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	const bool scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, p.fbo );
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p.tex, 0 );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( src ) );
	out.assign( p.xy.size() * 2, 0.0f );
	for ( size_t i = 0; i + 1 < p.xy.size(); i += 2 ) {
		const int x = p.xy[i], y = H - 1 - p.xy[i + 1];
		fn->glBlitFramebuffer( x, y, x + 1, y + 1, x, y, x + 1, y + 1, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	}
	bool ok = fn->glGetError() == GL_NO_ERROR;
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, p.fbo );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
	for ( size_t i = 0; ok && i + 1 < p.xy.size(); i += 2 )
		fn->glReadPixels( p.xy[i], H - 1 - p.xy[i + 1], 1, 1, GL_RGBA, GL_FLOAT, &out[i * 2] );
	ok = ok && fn->glGetError() == GL_NO_ERROR;
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( src ) );
	if ( scissor )
		fn->glEnable( GL_SCISSOR_TEST );
	return ok;
}

}	// namespace

void wwCellDrawProbe( Scene * scene, const QString & what )
{
	DrawProbe & p = dprobe();
	if ( p.on != 1 || !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	GLint src = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &src );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	if ( src == 0 || GLuint( src ) == p.fbo )
		return;
	std::vector<float> now;
	if ( !probeRead( r, src, vp[0] + vp[2], vp[1] + vp[3], p, now ) )
		return;
	GLboolean dmask = GL_FALSE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &dmask );
	GLint dfunc = 0;
	fn->glGetIntegerv( GL_DEPTH_FUNC, &dfunc );
	// the depth too: a draw that writes depth and no colour moves what later fragments are tested against
	std::vector<float> nowZ( p.xy.size() / 2, -1.0f );
	{
		GLint prevRead = 0;
		fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( src ) );
		for ( size_t i = 0; i + 1 < p.xy.size(); i += 2 )
			fn->glReadPixels( p.xy[i], vp[1] + vp[3] - 1 - p.xy[i + 1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &nowZ[i / 2] );
		if ( fn->glGetError() != GL_NO_ERROR ) {
			nowZ.assign( nowZ.size(), -1.0f );
			const HdrGpu g = hdrGpus().value( r );
			const int W = vp[0] + vp[2], H = vp[1] + vp[3];
			if ( GLuint( src ) == g.fbo && g.dsFormat ) {	// the frame: resolve its depth into a plain texture first
				if ( !p.zfbo || p.zw != W || p.zh != H || p.zfmt != g.dsFormat ) {
					if ( !p.zfbo )
						fn->glGenFramebuffers( 1, &p.zfbo );
					if ( p.ztex )
						fn->glDeleteTextures( 1, &p.ztex );
					fn->glGenTextures( 1, &p.ztex );
					fn->glBindTexture( GL_TEXTURE_2D, p.ztex );
					fn->glTexImage2D( GL_TEXTURE_2D, 0, g.dsFormat, W, H, 0, GL_DEPTH_STENCIL,
						g.dsFormat == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8, nullptr );
					fn->glBindTexture( GL_TEXTURE_2D, 0 );
					p.zw = W;
					p.zh = H;
					p.zfmt = g.dsFormat;
				}
				const bool scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
				fn->glDisable( GL_SCISSOR_TEST );
				fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, p.zfbo );
				fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, p.ztex, 0 );
				fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
				fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, p.zfbo );
				for ( size_t i = 0; i + 1 < p.xy.size(); i += 2 )
					fn->glReadPixels( p.xy[i], H - 1 - p.xy[i + 1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &nowZ[i / 2] );
				if ( fn->glGetError() != GL_NO_ERROR )
					nowZ.assign( nowZ.size(), -2.0f );
				fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( src ) );
				if ( scissor )
					fn->glEnable( GL_SCISSOR_TEST );
			}
		}
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	}
	std::vector<float> & lastZ = p.lastZ[src];
	if ( lastZ.size() != nowZ.size() )
		lastZ.assign( nowZ.size(), -1.0f );
	const bool seen = p.last.contains( src );
	std::vector<float> & last = p.last[src];
	if ( !seen )
		last.assign( now.size(), -1.0f );
	const QString target = GLuint( src ) == hdrGpus().value( r ).fbo ? QStringLiteral( "frame" ) : QStringLiteral( "fbo%1" ).arg( src );
	for ( size_t i = 0; i + 1 < p.xy.size(); i += 2 ) {
		const float * a = &last[i * 2];
		const float * b = &now[i * 2];
		if ( a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && lastZ[i / 2] == nowZ[i / 2] )
			continue;
		GLint cullFace = 0, frontFace = 0;
		GLfloat offF = 0.0f, offU = 0.0f;
		fn->glGetIntegerv( GL_CULL_FACE_MODE, &cullFace );
		fn->glGetIntegerv( GL_FRONT_FACE, &frontFace );
		fn->glGetFloatv( GL_POLYGON_OFFSET_FACTOR, &offF );
		fn->glGetFloatv( GL_POLYGON_OFFSET_UNITS, &offU );
		p.log += QStringLiteral( "cull %1/0x%2/0x%3 offset %4 %5 %6 " ).arg( int( fn->glIsEnabled( GL_CULL_FACE ) ) ).arg( cullFace, 0, 16 )
			.arg( frontFace, 0, 16 ).arg( int( fn->glIsEnabled( GL_POLYGON_OFFSET_FILL ) ) ).arg( double( offF ) ).arg( double( offU ) ).toUtf8();
		p.log += QStringLiteral( "z %1 -> %2 " ).arg( double( lastZ[i / 2] ), 0, 'g', 9 ).arg( double( nowZ[i / 2] ), 0, 'g', 9 ).toUtf8();
		p.log += QStringLiteral( "px %1,%2 %3 %4 test=%5 func=0x%6 write=%7 blend=%8 rgba %9 %10 %11 %12 -> %13 %14 %15 %16\n" )
			.arg( p.xy[i] ).arg( p.xy[i + 1] ).arg( target, what )
			.arg( int( fn->glIsEnabled( GL_DEPTH_TEST ) ) ).arg( dfunc, 0, 16 ).arg( int( dmask ) )
			.arg( int( fn->glIsEnabled( GL_BLEND ) ) )
			.arg( double( a[0] ), 0, 'g', 9 ).arg( double( a[1] ), 0, 'g', 9 ).arg( double( a[2] ), 0, 'g', 9 )
			.arg( double( a[3] ), 0, 'g', 9 )
			.arg( double( b[0] ), 0, 'g', 9 ).arg( double( b[1] ), 0, 'g', 9 ).arg( double( b[2] ), 0, 'g', 9 )
			.arg( double( b[3] ), 0, 'g', 9 ).toUtf8();
	}
	last.swap( now );
	lastZ.swap( nowZ );
}

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
	wwCellDrawProbe( scene, QStringLiteral( "at the frame's end" ) );	// lane VOLAIR1 (gates only)
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
	// lane VOLAIR1 (gates only): WW_CELL_HDR_DUMP=<file> writes the linear frame before the bloom and the tone map,
	// samples resolved: int32 W H, then W x H x RGBA float32, bottom row first. Every frame (a still path: the last).
	static const QByteArray hdrDump = qgetenv( "WW_CELL_HDR_DUMP" );
	if ( !hdrDump.isEmpty() ) {
		GLuint tex = 0, fbo = 0;
		fn->glGenTextures( 1, &tex );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, s.w, s.h, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glGenFramebuffers( 1, &fbo );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
		fn->glBlitFramebuffer( 0, 0, s.w, s.h, 0, 0, s.w, s.h, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
		std::vector<float> px( size_t( s.w ) * size_t( s.h ) * 4 );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		fn->glReadPixels( 0, 0, s.w, s.h, GL_RGBA, GL_FLOAT, px.data() );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
		fn->glDeleteFramebuffers( 1, &fbo );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glDeleteTextures( 1, &tex );
		QFile f( QString::fromLocal8Bit( hdrDump ) );
		if ( f.open( QIODevice::WriteOnly ) ) {
			const int32_t wh[2] = { s.w, s.h };
			f.write( reinterpret_cast<const char *>( wh ), sizeof( wh ) );
			f.write( reinterpret_cast<const char *>( px.data() ), qint64( px.size() * sizeof( float ) ) );
		}
		static bool said = false;
		if ( !said ) {
			said = true;
			qInfo().noquote() << QStringLiteral( "cell hdr: dump of the linear frame %1x%2 to %3 (every frame)" ).arg( s.w ).arg( s.h )
				.arg( QString::fromLocal8Bit( hdrDump ) );
		}
		// and <file>.depth: the main pass's own depth (samples nearest-resolved), to set against the obscurance
		// pass's: int32 W H, float32 projection [col][row] x 16, then W x H window depth float32, bottom row first
		GLuint dtex = 0, dfbo = 0;
		fn->glGenTextures( 1, &dtex );
		fn->glBindTexture( GL_TEXTURE_2D, dtex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, g.dsFormat, s.w, s.h, 0, GL_DEPTH_STENCIL,
			g.dsFormat == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8, nullptr );
		fn->glGenFramebuffers( 1, &dfbo );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, dfbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, dtex, 0 );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
		fn->glBlitFramebuffer( 0, 0, s.w, s.h, 0, 0, s.w, s.h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, dfbo );
		std::vector<float> dz( size_t( s.w ) * size_t( s.h ) );
		fn->glReadPixels( 0, 0, s.w, s.h, GL_DEPTH_COMPONENT, GL_FLOAT, dz.data() );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
		fn->glDeleteFramebuffers( 1, &dfbo );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glDeleteTextures( 1, &dtex );
		if ( dprobe().on == 1 ) {	// this frame's log (its obscurance pass, then the frame), then a clean one
			QFile pf( QString::fromLocal8Bit( hdrDump ) + QStringLiteral( ".probe" ) );
			if ( pf.open( QIODevice::WriteOnly ) )
				pf.write( dprobe().log );
			dprobe().log.clear();
			dprobe().last.clear();
		}
		QFile df( QString::fromLocal8Bit( hdrDump ) + QStringLiteral( ".depth" ) );
		if ( df.open( QIODevice::WriteOnly ) ) {
			const auto & pm = r->globalUniforms->projectionMatrix;
			float proj[16];
			for ( int c = 0; c < 4; c++ )
				for ( int k = 0; k < 4; k++ )
					proj[c * 4 + k] = pm[c][k];
			const int32_t wh[2] = { s.w, s.h };
			df.write( reinterpret_cast<const char *>( wh ), sizeof( wh ) );
			df.write( reinterpret_cast<const char *>( proj ), sizeof( proj ) );
			df.write( reinterpret_cast<const char *>( dz.data() ), qint64( dz.size() * sizeof( float ) ) );
		}
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	// lane BLOOM1b: the FO4CS bloom's chain on the linear frame, when that mode is picked (cellbloom.h); Vanilla: no GL call
	const bool foBloom = wwCellBloomRun( scene, g.color, g.ds, s.w, s.h, s.samples, g.dsFormat );

	auto prog = r->useProgram( "cell_hdr.prog" );
	if ( !prog ) {
		wwCellBloomDone( r, kFoBloomUnit );
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
	GLint prevDepthFunc = GL_LESS;
	fn->glGetIntegerv( GL_DEPTH_FUNC, &prevDepthFunc );
	GLint prevBlend[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
	fn->glGetIntegerv( GL_BLEND_SRC_RGB, &prevBlend[0] );
	fn->glGetIntegerv( GL_BLEND_DST_RGB, &prevBlend[1] );
	fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &prevBlend[2] );
	fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &prevBlend[3] );
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
	wwCellBloomFinal( r, kFoBloomUnit );	// lane BLOOM1b: the final combine's uniforms (off: cellIsFoBloomOn false)

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
	/* lane SKYHOLE1: a cell effect over the sky (no opaque draw under it, the depth still at the far plane) blended
	 * over 0 in the linear frame, and mode 0 laid its tone-mapped value over the backdrop: the Memory Den's steam
	 * (MistGroundWaterSteam01LongHalf) as a black patch. Mode 0 now takes only the pixels an opaque draw reached
	 * (the quad at the far plane passes GL_GREATER there), mode 2 the rest (GL_LEQUAL), composited by coverage.
	 * WW_CELL_SKYHOLE_RED=old: mode 0 over every 1 pixel, as before. */
	static const bool skyholeOld = qgetenv( "WW_CELL_SKYHOLE_RED" ).trimmed() == "old";
	static bool skyholeSaid = false;
	if ( !skyholeSaid ) {
		skyholeSaid = true;
		qInfo().noquote() << "cell hdr: effects over the sky" << ( skyholeOld
			? "replace the backdrop (RED=old)" : "composite over the backdrop by their coverage (lane SKYHOLE1)" );
	}
	const bool splitSky = sorted && !skyholeOld;
	if ( splitSky ) {
		fn->glEnable( GL_DEPTH_TEST );
		fn->glDepthFunc( GL_GREATER );	// an opaque draw's depth is nearer than the quad's 1.0
	}
	prog->uni1i( "hdrMode", 0 );
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	if ( splitSky ) {
		fn->glDepthFunc( GL_LEQUAL );	// the depth still at the far plane: only effects (or nothing) drew there
		fn->glEnable( GL_BLEND );
		fn->glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
		prog->uni1i( "hdrMode", 2 );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDepthFunc( GLenum( prevDepthFunc ) );
	}
	if ( sorted ) {
		fn->glStencilFunc( GL_EQUAL, 2, 0x03 );	// any other program: its value as written
		prog->uni1i( "hdrMode", 1 );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	}
	/* lane BLOOM1b: the FO4CS glow over every pixel mode 0 did not take (the sky, the other programs, the effects over
	 * the sky), added in display space: FO4CS spreads it over its whole HDR target, the sky included; here only the
	 * cell-lit pixels are light, so the rest take the tone-mapped difference the bloom makes (cell_hdr.frag mode 3) */
	if ( foBloom && sorted ) {
		fn->glEnable( GL_BLEND );
		fn->glBlendFunc( GL_ONE, GL_ONE );
		prog->uni1i( "hdrMode", 3 );
		fn->glStencilFunc( GL_NOTEQUAL, 1, 0x03 );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		if ( splitSky ) {	// the 1s at the far plane: mode 2's pixels
			fn->glStencilFunc( GL_EQUAL, 1, 0x03 );
			fn->glEnable( GL_DEPTH_TEST );
			fn->glDepthFunc( GL_LEQUAL );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			fn->glDisable( GL_DEPTH_TEST );
			fn->glDepthFunc( GLenum( prevDepthFunc ) );
		}
		fn->glDisable( GL_BLEND );
	}
	r->stopProgram();
	wwCellBloomDone( r, kFoBloomUnit );

	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glStencilMask( 0xFF );
	fn->glStencilFunc( GL_ALWAYS, 0, 0xFF );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	fn->glBlendFuncSeparate( GLenum( prevBlend[0] ), GLenum( prevBlend[1] ), GLenum( prevBlend[2] ), GLenum( prevBlend[3] ) );
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
