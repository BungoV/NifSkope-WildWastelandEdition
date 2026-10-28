#include "cellhdr.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store
#include "cellunits.h"

#include "gl/atmosphere.h"	// lane SKYA (EXPO1): the sky in the exposure's mean
#include "gl/cellbloom.h"
#include "gl/cellfogbloom.h"
#include "gl/celllights.h"
#include "gl/cellssr.h"
#include "gl/celluniform.h"	// lane CELLUNI4: the known state the frame end's passes take
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "gl/cellpasstime.h"
#include "gl/cellvrs.h"
#include "gl/gametaa.h"	// lane TAA3: the Modern resolve before the tone map

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// lane HDR1: the cell view's linear frame and its one tone map (cellhdr.h)

namespace {

constexpr int kSrcUnit = 8;		// the linear frame (sampler2DMS) in cell_hdr.prog; the cell units are 10..16
constexpr int kFogBloomUnit = 7;	// lane FOGBLOOM1: the bloom-intensity target (sampler2DMS); no TexCache map in this pass (9 in the lane: TONEMAP1's stencil holds it)
constexpr int kFoBloomUnit = WwCellUnit::Bloom;	// lane BLOOM1b: the FO4CS chain's top (sampler2D), on the vanilla bloom's unit (same type)

constexpr int kStencilUnit = WwPassUnit::HdrStencil;	// lane TONEMAP1: the frame's stencil (usampler2DMS) in cell_hdr.prog, always assigned

// lane TONEMAP1: the depth / stencil is a texture and the tone map tests the stencil in its shader, so only the depth
// goes back to the view (the MSAA stencil blit was the pass's cost). WW_CELL_HDR_STEX_RED=1: renderbuffer + both blitted.
bool stexOn()
{
	// lane LIVESET1: follows a live set (the frame's targets compare dsTex, so they are made again)
	WW_SWITCH_STATIC( bool, on, qEnvironmentVariableIntValue( "WW_CELL_HDR_STEX_RED" ) == 0 && qEnvironmentVariableIntValue( "WW_TONEMAP1_RED" ) == 0
		&& qEnvironmentVariableIntValue( "WW_CELL_HDR_EARLY_RED" ) == 0 );	// cell_hdr_fd.prog has no stencil test of its own
	return on;
}

/* lane AAFORMAT1 (study, default RGBA32F): WW_CELL_HDR_FORMAT=rgba16f | r11g11b10f picks the linear frame's format.
 * r11g11b10f has no alpha (the effects over the sky lose their coverage): a cost arm, never a default.
 * WW_CELL_HDR_FORMAT_RED=1 / WW_AAFORMAT1_RED=1: RGBA32F whatever was asked. */
struct HdrFormat
{
	GLenum gl = GL_RGBA32F;
	int bytes = 16;
	const char * name = "rgba32f";
	bool red = false;
};
const HdrFormat & hdrFormat()
{
	WW_SWITCH_STATIC( HdrFormat, f, [] {	// lane LIVESET1: a live set makes the frame's colour target again (HdrGpu.colorFormat)
		HdrFormat h;
		const QByteArray v = qgetenv( "WW_CELL_HDR_FORMAT" ).trimmed().toLower();
		h.red = qEnvironmentVariableIntValue( "WW_CELL_HDR_FORMAT_RED" ) != 0 || qEnvironmentVariableIntValue( "WW_AAFORMAT1_RED" ) != 0;
		if ( h.red )
			return h;
		if ( v == "rgba16f" ) {
			h.gl = GL_RGBA16F; h.bytes = 8; h.name = "rgba16f";
		} else if ( v == "r11g11b10f" ) {
			h.gl = GL_R11F_G11F_B10F; h.bytes = 4; h.name = "r11g11b10f";
		}
		return h;
	}() );
	return f;
}

// lane AAFORMAT1 (gates only): WW_CELL_AA_MEM=1 logs the card's used video memory (GL_NVX_gpu_memory_info) when it moves
template <typename F> void aaVidmemNote( F * fn )
{
	WW_SWITCH_STATIC( bool, on, qEnvironmentVariableIntValue( "WW_CELL_AA_MEM" ) != 0 );
	static int have = -1, lastMb = -1;
	if ( !on )
		return;
	if ( have < 0 ) {
		const QOpenGLContext * c = QOpenGLContext::currentContext();
		have = c && c->hasExtension( QByteArrayLiteral( "GL_NVX_gpu_memory_info" ) ) ? 1 : 0;
		if ( !have )
			qInfo().noquote() << "cell aa: vidmem unknown (no GL_NVX_gpu_memory_info)";
	}
	if ( !have )
		return;
	GLint totalKb = 0, freeKb = 0;
	fn->glGetIntegerv( 0x9048, &totalKb );	// GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
	fn->glGetIntegerv( 0x9049, &freeKb );	// GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
	const int usedMb = int( ( qint64( totalKb ) - freeKb ) / 1024 );
	if ( usedMb != lastMb ) {
		lastMb = usedMb;
		qInfo().noquote() << QStringLiteral( "cell aa: vidmem used %1 MB of %2 MB" ).arg( usedMb ).arg( totalKb / 1024 );
	}
}

// lane SYNC1: each glGet / glGetError waits for the driver's thread; the frame's own configuration is cached, the state
// read once per tone map, errors checked on a new configuration only. WW_CELL_HDR_SYNC_RED=1: every query every frame.
bool syncOn()
{
	WW_SWITCH_STATIC( bool, on, qEnvironmentVariableIntValue( "WW_CELL_HDR_SYNC_RED" ) == 0 && qEnvironmentVariableIntValue( "WW_TONEMAP1_RED" ) == 0 );
	return on;
}

struct HdrGpu
{
	GLuint fbo = 0, color = 0, ds = 0;
	int w = 0, h = 0, samples = 0;
	GLenum dsFormat = 0;
	GLenum colorFormat = 0;	// lane LIVESET1: the colour target's format, so a live WW_CELL_HDR_FORMAT makes it again
	bool dsTex = false;		// lane TONEMAP1: ds is a multisample texture (else a renderbuffer)
	bool stexFailed = false;	// its texture or target was refused once: renderbuffers from then on
	bool complete = false;	// lane SYNC1: the target passed its status check since it was made
	bool trusted = false;	// lane SYNC1: a frame on this target raised no GL error
	bool cfgValid = false;	// lane SYNC1: the view's (framebuffer 0) depth format and samples at this size
	int cfgW = 0, cfgH = 0, cfgSamples = 0;
	GLenum cfgFormat = 0;
};

// lane SYNC1: the state the measure and the tone quads restore, read once (sync) or each where it was (red)
struct GlSnap
{
	GLint active = 0, vp[4] = { 0, 0, 1, 1 };
	GLfloat clear[4] = { 0, 0, 0, 0 };
	GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE }, depthMask = GL_TRUE;
	bool depth = false, blend = false, cull = false, stencil = false, scissor = false;
	GLint sFunc = GL_ALWAYS, sRef = 0, sMask = 0xFF, sWrite = 0xFF, sFail = GL_KEEP, sZFail = GL_KEEP, sPass = GL_KEEP;
	GLint depthFunc = GL_LESS, blendF[4] = { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };

	template <typename F> void enables( F * fn )
	{
		depth = fn->glIsEnabled( GL_DEPTH_TEST );
		blend = fn->glIsEnabled( GL_BLEND );
		cull = fn->glIsEnabled( GL_CULL_FACE );
		stencil = fn->glIsEnabled( GL_STENCIL_TEST );
		scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	}
	template <typename F> void measurePart( F * fn )	// what isMeasureFrame read
	{
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		fn->glGetFloatv( GL_COLOR_CLEAR_VALUE, clear );
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		enables( fn );
		fn->glGetIntegerv( GL_STENCIL_FUNC, &sFunc );
		fn->glGetIntegerv( GL_STENCIL_REF, &sRef );
		fn->glGetIntegerv( GL_STENCIL_VALUE_MASK, &sMask );
		fn->glGetIntegerv( GL_STENCIL_WRITEMASK, &sWrite );
		fn->glGetIntegerv( GL_STENCIL_FAIL, &sFail );
		fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_FAIL, &sZFail );
		fn->glGetIntegerv( GL_STENCIL_PASS_DEPTH_PASS, &sPass );
	}
	template <typename F> void quadsExtra( F * fn )	// what the tone quads read beyond that
	{
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		fn->glGetIntegerv( GL_DEPTH_FUNC, &depthFunc );
		fn->glGetIntegerv( GL_BLEND_SRC_RGB, &blendF[0] );
		fn->glGetIntegerv( GL_BLEND_DST_RGB, &blendF[1] );
		fn->glGetIntegerv( GL_BLEND_SRC_ALPHA, &blendF[2] );
		fn->glGetIntegerv( GL_BLEND_DST_ALPHA, &blendF[3] );
	}
	template <typename F> void quadsPart( F * fn )	// what the tone quads read (red: as before)
	{
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		enables( fn );
		quadsExtra( fn );
	}
};

struct HdrState
{
	bool active = false;
	Scene * scene = nullptr;
	GLint prevDraw = 0, prevRead = 0;
	int w = 0, h = 0, samples = 0;
	std::uint64_t serial = 0;	// lane CELLUNI6: the HDR frames begun (the depth max's once-a-frame key)
	bool red = wwEnv( "WW_CELL_HDR_RED" ) == "perfrag";
	QString last = QStringLiteral( "off" );
	QString said;
};

HdrState & hst()
{
	static HdrState s;
	static quint64 liveGen = 0;
	if ( wwSwitchMovedFor( liveGen, { "WW_CELL_HDR" } ) )	// lane TESTDRIVER1: a live set re-reads the red
		s.red = wwEnv( "WW_CELL_HDR_RED" ) == "perfrag";
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
		const QByteArray spec = wwEnv( "WW_CELL_DRAW_PROBE" );
		if ( !spec.isEmpty() && !wwEnv( "WW_CELL_HDR_DUMP" ).isEmpty() ) {
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

/* lane PERF1: the exposure and the bloom from the linear frame already drawn, as the game reads its own HDR target
 * (its downsample chain), instead of a second scene submission (glview.cpp's measure pass). WW_IS_MEASURE, read each
 * frame: unset/frame = this frame's light, read back a frame late (no wait, the game's adaptation lags the same way);
 * sync = read back the same frame (waits for the GPU); old = the old submission only; red = the old submission runs
 * as well (its cost back) and the frame's values replace its. WW_CELL_IS_DUMP keeps the old (full-size) measure. */
int isMeasureMode()
{
	const QByteArray v = wwEnv( "WW_IS_MEASURE" ).trimmed();
	if ( !wwEnv( "WW_CELL_IS_DUMP" ).isEmpty() )
		return 2;
	return v == "sync" ? 1 : v == "old" ? 2 : v == "red" ? 3 : 0;
}

struct IsMeasureGpu
{
	GLuint fbo = 0, tex = 0, ds = 0, pbo = 0;
	int w = 0, h = 0;
	GLenum dsFormat = 0;
	GLsync fence = nullptr;	// frame mode: the readback in flight
	bool trusted = false;	// lane SYNC1: a measure on this target raised no GL error
	int qw = 0, qh = 0;
	const void * doc = nullptr;
};

QHash<Renderer *, IsMeasureGpu> & isGpus()
{
	static QHash<Renderer *, IsMeasureGpu> g;
	return g;
}

QHash<const void *, bool> & isServed()	// the document's last linear frame was measured (glview skips its pass)
{
	static QHash<const void *, bool> s;
	return s;
}

// the measure's quarter-size mip: rgb = the mean of the light over the linear pixels, a = their share of the texel
void isApply( Scene * scene, const float * px, int qw, int qh )
{
	const double wgt[3] = { 0.2125, 0.7154, 0.0721 };
	double sum = 0.0, cover = 0.0;
	for ( size_t i = 0; i < size_t( qw ) * size_t( qh ); i++ ) {
		const float a = px[i * 4 + 3];
		if ( !( a > 0.0f ) )
			continue;
		cover += a;
		for ( int c = 0; c < 3; c++ )
			if ( std::isfinite( px[i * 4 + c] ) )
				sum += wgt[c] * px[i * 4 + c];
	}
	// SKY2B (EXPO2): the sky pixels in view, each along its own ray
	wwCellImageSpaceSetAdapted( scene, wwAtmoAdaptedInView( scene, cover > 0.0 ? float( sum / cover ) : 0.0f, px, qw, qh ),
		int( std::lround( cover ) ) );
	wwCellImageSpaceSetBloom( scene, px, qw, qh, 1 );
	WW_SWITCH_STATIC( bool, echo, wwEnvInt( "WW_IS_MEASURE_ECHO" ) == 1 );
	static QString said;
	if ( echo ) {
		const QString e = wwCellImageSpaceEcho( scene );
		if ( e != said ) {
			said = e;
			qInfo().noquote() << "cell is measure (frame):" << e;
		}
	}
}

void isMeasureFrame( Scene * scene, HdrGpu & g, int W, int H, int S, GLint prevDraw, GLint prevRead, const GlSnap * shared )
{
	const int mode = isMeasureMode();
	static int saidMode = -1;
	if ( saidMode != mode ) {
		saidMode = mode;
		static const char * names[4] = { "from the linear frame, read a frame late", "from the linear frame, read the same frame",
			"by its own scene submission (old)", "by its own submission (red: its cost) and then from the linear frame" };
		qInfo().noquote() << "cell imagespace: exposure and bloom measured" << names[mode];
	}
	if ( mode == 2 )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const void * doc = scene->nifModel;
	IsMeasureGpu & m = isGpus()[r];
	const int qw = std::max( 1, W >> 2 ), qh = std::max( 1, H >> 2 );
	if ( m.fence ) {	// the last frame's readback (a frame old: the GPU is past it in a GPU-bound frame)
		fn->glClientWaitSync( m.fence, GL_SYNC_FLUSH_COMMANDS_BIT, GLuint64( 1000000000 ) );
		fn->glDeleteSync( m.fence );
		m.fence = nullptr;
		if ( m.doc == doc && mode != 1 ) {
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, m.pbo );
			const auto * px = static_cast<const float *>( fn->glMapBufferRange( GL_PIXEL_PACK_BUFFER, 0,
				GLsizeiptr( size_t( m.qw ) * size_t( m.qh ) * 4 * sizeof( float ) ), GL_MAP_READ_BIT ) );
			if ( px )
				isApply( scene, px, m.qw, m.qh );
			fn->glUnmapBuffer( GL_PIXEL_PACK_BUFFER );
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		}
	}
	isServed().insert( doc, false );
	auto prog = r->useProgram( "cell_is_measure.prog" );
	if ( !prog )
		return;	// glview's pass measures the next frame
	GLint prevTex = 0;
	GlSnap own;	// lane SYNC1: the tone map's one read of the state (shared), else this pass's own as before
	if ( !shared )
		own.measurePart( fn );
	const GlSnap & sn = shared ? *shared : own;
	const bool check = !shared || !m.trusted;	// lane SYNC1: errors read on a new target only
	if ( check ) {
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	if ( !m.fbo || m.w != W || m.h != H || m.dsFormat != g.dsFormat ) {
		m.trusted = false;
		if ( !m.fbo )
			fn->glGenFramebuffers( 1, &m.fbo );
		if ( m.tex )
			fn->glDeleteTextures( 1, &m.tex );
		fn->glGenTextures( 1, &m.tex );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
		fn->glBindTexture( GL_TEXTURE_2D, m.tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, W, H, 0, GL_RGBA, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2 );	// the mips stop at the quarter size
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
		if ( !m.ds )
			fn->glGenRenderbuffers( 1, &m.ds );
		fn->glBindRenderbuffer( GL_RENDERBUFFER, m.ds );
		fn->glRenderbufferStorage( GL_RENDERBUFFER, g.dsFormat, W, H );
		fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m.fbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m.tex, 0 );
		fn->glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m.ds );
		m.w = W;
		m.h = H;
		m.dsFormat = g.dsFormat;
		if ( !m.pbo )
			fn->glGenBuffers( 1, &m.pbo );
	}
	// the frame's stencil (who wrote each pixel last) into the measure's own, then its linear pixels only
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m.fbo );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_STENCIL_BUFFER_BIT, GL_NEAREST );
	bool ok = !check || fn->glGetError() == GL_NO_ERROR;
	if ( ok ) {
		fn->glViewport( 0, 0, W, H );
		fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		fn->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		fn->glClear( GL_COLOR_BUFFER_BIT );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_CULL_FACE );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilMask( 0 );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		fn->glStencilFunc( GL_EQUAL, 1, 0x03 );	// a cell program or a cell effect wrote it last: linear light
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.color );
		prog->uni1i( "hdrSrc", kSrcUnit );
		prog->uni1i( "hdrSamples", S );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		const float * attrs = quad;
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
		fn->glBindTexture( GL_TEXTURE_2D, m.tex );
		fn->glGenerateMipmap( GL_TEXTURE_2D );	// level 2: the quarter size, a 4 x 4 box mean (the game's downsample)
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		if ( mode == 1 ) {
			std::vector<float> px( size_t( qw ) * size_t( qh ) * 4 );
			fn->glGetTexImage( GL_TEXTURE_2D, 2, GL_RGBA, GL_FLOAT, px.data() );
			isApply( scene, px.data(), qw, qh );
		} else {
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, m.pbo );
			fn->glBufferData( GL_PIXEL_PACK_BUFFER, GLsizeiptr( size_t( qw ) * size_t( qh ) * 4 * sizeof( float ) ), nullptr, GL_STREAM_READ );
			fn->glGetTexImage( GL_TEXTURE_2D, 2, GL_RGBA, GL_FLOAT, nullptr );
			fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
			m.fence = fn->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
			m.doc = doc;
			m.qw = qw;
			m.qh = qh;
		}
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prevTex ) );
		ok = !check || fn->glGetError() == GL_NO_ERROR;
		if ( check && ok )
			m.trusted = true;
	}
	r->stopProgram();
	isServed().insert( doc, ok );
	fn->glActiveTexture( GLenum( sn.active ) );
	fn->glViewport( sn.vp[0], sn.vp[1], sn.vp[2], sn.vp[3] );
	fn->glClearColor( sn.clear[0], sn.clear[1], sn.clear[2], sn.clear[3] );
	fn->glColorMask( sn.colorMask[0], sn.colorMask[1], sn.colorMask[2], sn.colorMask[3] );
	fn->glStencilFunc( GLenum( sn.sFunc ), sn.sRef, GLuint( sn.sMask ) );
	fn->glStencilMask( GLuint( sn.sWrite ) );
	fn->glStencilOp( GLenum( sn.sFail ), GLenum( sn.sZFail ), GLenum( sn.sPass ) );
	if ( sn.stencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( sn.depth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( sn.blend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( sn.cull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( sn.scissor ) fn->glEnable( GL_SCISSOR_TEST );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
}

}	// namespace

bool wwCellImageSpaceMeasureByPass( Scene * scene )
{
	if ( isMeasureMode() >= 2 || !scene )
		return true;
	// the first frame (no value yet), or the last frame took no linear frame or its measure failed: the old pass
	return !isServed().take( scene->nifModel ) || wwCellImageSpaceAdapted( scene ) < 0.0f;
}

bool wwCellDrawProbeOn()
{
	return dprobe().on == 1;
}

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

void wwCellHdrFrameSize( int & w, int & h )
{
	const HdrState & s = hst();
	w = s.active ? s.w : 0;
	h = s.active ? s.h : 0;
}

unsigned int wwCellHdrDepthFormat( unsigned int fbo )
{
	if ( fbo )
		for ( const HdrGpu & g : hdrGpus() )
			if ( g.fbo == fbo )
				return g.dsFormat;
	return 0;
}

QString wwCellHdrEcho()
{
	return hst().last;
}

namespace {
struct DepthMaxGpu
{
	GLuint fbo = 0, tex = 0;
	int w = 0, h = 0;
	bool ok = false;
};
}

static unsigned int depthMaxMake( Scene * scene )
{
	HdrState & s = hst();
	if ( !s.active || s.scene != scene || !scene || !scene->renderer )
		return 0;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	HdrGpu & g = hdrGpus()[r];
	if ( !g.dsTex || !g.ds )
		return 0;
	static QHash<Renderer *, DepthMaxGpu> gpus;
	DepthMaxGpu & d = gpus[r];
	GLint prevDraw = 0, prevActive = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	if ( !d.fbo || d.w != s.w || d.h != s.h ) {
		if ( !d.fbo )
			fn->glGenFramebuffers( 1, &d.fbo );
		if ( d.tex )
			fn->glDeleteTextures( 1, &d.tex );
		fn->glGenTextures( 1, &d.tex );
		fn->glBindTexture( GL_TEXTURE_2D, d.tex );
		fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, s.w, s.h, 0, GL_RED, GL_FLOAT, nullptr );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, d.fbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d.tex, 0 );
		d.ok = fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		d.w = s.w;
		d.h = s.h;
		if ( !d.ok )
			qInfo().noquote() << "far cards: the depth reject's target is incomplete; cards march every fragment";
	}
	if ( !d.ok )
		return 0;
	auto prog = r->useProgram( "cell_depthmax.prog" );
	if ( !prog )
		return 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST ),
		wasBlend = fn->glIsEnabled( GL_BLEND ), wasCull = fn->glIsEnabled( GL_CULL_FACE ),
		wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, d.fbo );
	fn->glViewport( 0, 0, s.w, s.h );
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	// the frame's depth / stencil texture reads stencil for the tone map (TONEMAP1): depth for this one draw
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::CardDepth ) );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.ds );
	fn->glTexParameteri( GL_TEXTURE_2D_MULTISAMPLE, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT );
	prog->uni1i( "depthMs", WwCellUnit::CardDepth );
	prog->uni1i( "depthSamples", std::max( s.samples, 1 ) );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
	fn->glTexParameteri( GL_TEXTURE_2D_MULTISAMPLE, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
	r->stopProgram();
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	fn->glActiveTexture( GLenum( prevActive ) );
	return d.tex;
}

namespace {
WwCellUni::FrameOnce & depthMaxOnce()
{
	static WwCellUni::FrameOnce o;
	return o;
}
template <class F> std::vector<float> depthMaxRead( F * fn, GLuint tex, int w, int h )
{
	std::vector<float> px( size_t( w ) * size_t( h ) );
	GLint prev = 0;
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev );
	fn->glBindTexture( GL_TEXTURE_2D, tex );
	fn->glGetTexImage( GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, px.data() );
	fn->glBindTexture( GL_TEXTURE_2D, GLuint( prev ) );
	return px;
}
}

/* lane CELLUNI6: made once per HDR frame and handed to every later reader of that frame (the far cards, the volume
 * clouds, the underwater pass), until a draw that writes the frame's depth says so (wwCellHdrDepthMaxWrote: the far
 * cards, which draw after taking it). WW_SETUP_MEMO_CHECK=1 makes it again on a hit and counts a different one
 * (knownstale). WW_CELL_UNI6_RED=hdrdepth: made at every call. */
unsigned int wwCellHdrDepthMax( Scene * scene )
{
	HdrState & s = hst();
	if ( !s.active || s.scene != scene || !scene || !scene->renderer )
		return 0;
	Renderer * r = scene->renderer;
	WwCellUni::FrameOnce & once = depthMaxOnce();
	const std::uint64_t key = ( std::uint64_t( std::uint32_t( s.w ) ) << 32 ) | std::uint32_t( s.h );
	unsigned int tex = 0;
	if ( wwCellUni6Red( "hdrdepth" ) )
		return depthMaxMake( scene );
	if ( !once.take( r, s.serial, key, tex ) ) {
		tex = depthMaxMake( scene );
		if ( tex )
			once.keep( r, s.serial, key, tex );
		return tex;
	}
	if ( wwSetupMemoChecking() ) {
		const std::vector<float> kept = depthMaxRead( r->fn, GLuint( tex ), s.w, s.h );
		const unsigned int made = depthMaxMake( scene );
		if ( made != tex || depthMaxRead( r->fn, GLuint( made ), s.w, s.h ) != kept )
			WwCellUni::state().knownStale++;
		return made;
	}
	return tex;
}

void wwCellHdrDepthMaxWrote()
{
	depthMaxOnce().dirty();
}

namespace {
struct ColorOnlyGpu
{
	GLuint fbo = 0, color = 0;
	int w = 0, h = 0, samples = 0;
	bool ok = false;
	GLint prevDraw = 0;
};
QHash<Renderer *, ColorOnlyGpu> & colorOnlyGpus()
{
	static QHash<Renderer *, ColorOnlyGpu> g;
	return g;
}
}

int wwCellHdrDepthMsBegin( Scene * scene, int unit )
{
	HdrState & s = hst();
	if ( !s.active || s.scene != scene || !scene || !scene->renderer )
		return 0;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	HdrGpu & g = hdrGpus()[r];
	if ( !g.dsTex || !g.ds || !g.color || g.samples < 2 )
		return 0;
	ColorOnlyGpu & c = colorOnlyGpus()[r];
	GLint prev = 0;
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prev );
	if ( !c.fbo )
		fn->glGenFramebuffers( 1, &c.fbo );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, c.fbo );
	// attached every call: a remade colour target can reuse the old texture name
	fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, g.color, 0 );
	if ( c.color != g.color || c.w != g.w || c.h != g.h || c.samples != g.samples ) {
		c.ok = fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
		c.color = g.color;
		c.w = g.w;
		c.h = g.h;
		c.samples = g.samples;
	}
	if ( !c.ok ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prev ) );
		return 0;
	}
	c.prevDraw = prev;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.ds );
	fn->glTexParameteri( GL_TEXTURE_2D_MULTISAMPLE, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT );
	return g.samples;
}

void wwCellHdrDepthMsEnd( Scene * scene, int unit )
{
	if ( !scene || !scene->renderer )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glTexParameteri( GL_TEXTURE_2D_MULTISAMPLE, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( colorOnlyGpus()[r].prevDraw ) );
}

quint64 wwGlPaintCount();	// glview.cpp

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
	/* lane CELLUNI6: the frame's target as the obscurance pass asked it in this paint (every cell pass since put it
	 * back); no note (no obscurance pass, another paint) or WW_CELL_UNI6_RED=hdrbegin (it keeps none): asked */
	WwCellUni::targetFrom( fn, WwCellUni::frameTarget(), wwGlPaintCount(), s.prevDraw, s.prevRead, vp, wwSetupMemoChecking() );
	const int W = vp[0] + vp[2], H = vp[1] + vp[3];
	HdrGpu & g = hdrGpus()[r];
	const bool sync = syncOn();
	// lane SYNC1: the view's own framebuffer keeps its format for the window's life (an FBO name can come back as another)
	const bool cached = sync && s.prevDraw == 0 && g.cfgValid && g.cfgW == W && g.cfgH == H;
	GLenum dsFormat = 0;
	if ( cached ) {
		dsFormat = g.cfgFormat;
		samples = g.cfgSamples;
	} else {
		fn->glGetIntegerv( GL_SAMPLES, &samples );
		// the frame's depth format: depth and stencil go back by a blit, which needs the same format both sides
		GLint depthBits = 0, stencilBits = 0, compType = 0;
		const GLenum att = s.prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, s.prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
			GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
		if ( stencilBits == 8 && depthBits == 24 && compType != GL_FLOAT )
			dsFormat = GL_DEPTH24_STENCIL8;
		else if ( stencilBits == 8 && depthBits == 32 && compType == GL_FLOAT )
			dsFormat = GL_DEPTH32F_STENCIL8;
		if ( !dsFormat ) {
			note( s, QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil: no stencil to sort the pixels by)" )
				.arg( depthBits ).arg( stencilBits ) );
			return false;
		}
		if ( sync && s.prevDraw == 0 ) {
			g.cfgValid = true;
			g.cfgW = W;
			g.cfgH = H;
			g.cfgSamples = samples;
			g.cfgFormat = dsFormat;
		}
	}
	const int S = std::max( int( samples ), 1 );
	const bool wantTex = stexOn() && !g.stexFailed;
	if ( !g.fbo || g.w != W || g.h != H || g.samples != S || g.dsFormat != dsFormat || g.dsTex != wantTex
		|| g.colorFormat != hdrFormat().gl ) {
		if ( !g.fbo )
			fn->glGenFramebuffers( 1, &g.fbo );
		if ( g.color )
			fn->glDeleteTextures( 1, &g.color );
		fn->glGenTextures( 1, &g.color );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.color );
		/* finalfix (SUNCELL1): the linear frame is RGBA32F, not RGBA16F. Multisampled additive blending into fp16
		 * (the blended ground layers, SRC_ALPHA/ONE) rounded +-1 step differently run to run on the same inputs
		 * (every draw's state, uniforms and textures traced identical); fp32 measured 0 of 7 frames differing. */
		const HdrFormat & hf = hdrFormat();	// lane AAFORMAT1: RGBA32F unless a study run asks
		fn->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, S, hf.gl, W, H, GL_TRUE );
		g.colorFormat = hf.gl;
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		const double mb = double( W ) * H * S / ( 1024.0 * 1024.0 );
		qInfo().noquote() << QStringLiteral( "cell aa: linear frame %1%2, %3x%4, %5 sample(s): colour %6 MB, depth / stencil %7 MB" )
			.arg( QLatin1StringView( hf.name ) ).arg( hf.red ? QStringLiteral( " (red)" ) : QString() ).arg( W ).arg( H ).arg( S )
			.arg( mb * hf.bytes, 0, 'f', 1 ).arg( mb * ( dsFormat == GL_DEPTH32F_STENCIL8 ? 8 : 4 ), 0, 'f', 1 );
		if ( g.ds && g.dsTex )
			fn->glDeleteTextures( 1, &g.ds );
		else if ( g.ds )
			fn->glDeleteRenderbuffers( 1, &g.ds );
		g.ds = 0;
		g.dsTex = false;
		if ( wantTex ) {	// lane TONEMAP1: the stencil read as an unsigned integer (usampler2DMS) by the tone map
			while ( fn->glGetError() != GL_NO_ERROR ) {
			}
			fn->glGenTextures( 1, &g.ds );
			fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.ds );
			fn->glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, S, dsFormat, W, H, GL_TRUE );
			fn->glTexParameteri( GL_TEXTURE_2D_MULTISAMPLE, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX );
			fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
			g.dsTex = fn->glGetError() == GL_NO_ERROR;
			if ( !g.dsTex ) {
				fn->glDeleteTextures( 1, &g.ds );
				g.ds = 0;
				g.stexFailed = true;
				qInfo().noquote() << "cell hdr: the depth / stencil texture was refused; renderbuffer and stencil blit (lane TONEMAP1 off)";
			}
		}
		if ( !g.dsTex ) {
			fn->glGenRenderbuffers( 1, &g.ds );
			fn->glBindRenderbuffer( GL_RENDERBUFFER, g.ds );
			fn->glRenderbufferStorageMultisample( GL_RENDERBUFFER, S, dsFormat, W, H );
			fn->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
		}
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, g.color, 0 );
		if ( g.dsTex )
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, g.ds, 0 );
		else
			fn->glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.ds );
		g.w = W;
		g.h = H;
		g.samples = S;
		g.dsFormat = dsFormat;
		g.complete = false;
		g.trusted = false;
	}
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
	if ( !( sync && g.complete ) ) {
		if ( fn->glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );
			if ( g.dsTex ) {	// lane TONEMAP1: a target the driver will not take: renderbuffers from the next frame
				g.stexFailed = true;
				qInfo().noquote() << "cell hdr: the linear frame with a depth / stencil texture is incomplete; renderbuffer and stencil blit";
			}
			note( s, QStringLiteral( "refused (the linear frame is incomplete)" ) );
			return false;
		}
		g.complete = true;
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
	aaVidmemNote( fn );
	s.active = true;
	s.serial++;
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
	const bool sync = syncOn();
	const bool check = !sync || !g.trusted;	// lane SYNC1: errors read on a new target only
	if ( check ) {
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	/* depth and stencil back to the frame: later draws test against the scene, the stencil sorts the pixels.
	 * lane TONEMAP1: with the stencil a texture the tone map reads it itself; only the depth goes back. */
	PassTime::subBegin( PassTime::ToneBlit );	// lane PERF2: the tone-map pass's four parts
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( s.prevDraw ) );
	fn->glBlitFramebuffer( 0, 0, s.w, s.h, 0, 0, s.w, s.h, g.dsTex ? GL_DEPTH_BUFFER_BIT : GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
		GL_NEAREST );
	const GLenum blitErr = check ? fn->glGetError() : GLenum( GL_NO_ERROR );
	if ( check && sync && blitErr == GL_NO_ERROR )
		g.trusted = true;
	PassTime::subEnd( PassTime::ToneBlit );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( s.prevRead ) );
	GlSnap sn;	// lane SYNC1: read once here for the measure and the tone quads (the measure and the bloom restore it)
	if ( sync ) {
		sn.measurePart( fn );
		sn.quadsExtra( fn );
	}
	/* lane CELLUNI4: read before the SSR keep and the temporal AA too. Every pass from here to the tone quads restores
	 * what it changed, so they take this instead of asking (WW_CELL_UNI_RED=1: they ask) */
	WwCellUni::KnownGl known;
	known.draw = s.prevDraw;
	known.read = s.prevRead;
	known.active = sn.active;
	std::memcpy( known.vp, sn.vp, sizeof known.vp );
	std::memcpy( known.clear, sn.clear, sizeof known.clear );
	std::memcpy( known.colorMask, sn.colorMask, sizeof known.colorMask );
	known.depthMask = sn.depthMask;
	known.depth = sn.depth, known.blend = sn.blend, known.cull = sn.cull, known.stencil = sn.stencil, known.scissor = sn.scissor;
	known.sFunc = sn.sFunc, known.sRef = sn.sRef, known.sMask = sn.sMask, known.sWrite = sn.sWrite;
	known.sFail = sn.sFail, known.sZFail = sn.sZFail, known.sPass = sn.sPass;
	WwCellUni::knownSlot() = sync && !wwCellUniRed() ? &known : nullptr;
	wwCellSsrKeepFrame( scene, g.fbo, s.w, s.h );	// lane PERF3: WW_CELL_SSR_PREV's previous frame (else nothing)
	{	// lane TAA3: the Modern temporal AA on this linear frame when its row says so (no GL call otherwise)
		const float adapted = wwCellImageSpaceAdapted( scene );
		wwGameTaaResolveLinear( scene, g.fbo, g.colorFormat, adapted >= 0.0f ? std::clamp( 0.18f / ( adapted + 0.001f ), 1.0f / 64.0f, 64.0f ) : 1.0f );
	}
	// lane VOLAIR1 (gates only): WW_CELL_HDR_DUMP=<file> writes the linear frame before the bloom and the tone map,
	// samples resolved: int32 W H, then W x H x RGBA float32, bottom row first. Every frame (a still path: the last).
	WW_SWITCH_STATIC( QByteArray, hdrDump, wwEnv( "WW_CELL_HDR_DUMP" ) );
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
	// lane PERF1: the exposure and the bloom from this linear frame (before the uniforms take them)
	PassTime::subBegin( PassTime::ToneMeasure );
	isMeasureFrame( scene, g, s.w, s.h, s.samples, s.prevDraw, s.prevRead, sync ? &sn : nullptr );
	PassTime::subEnd( PassTime::ToneMeasure );
	// lane BLOOM1b: the FO4CS bloom's chain on the linear frame, when that mode is picked (cellbloom.h); Vanilla: no GL call
	PassTime::subBegin( PassTime::ToneBloom );
	const bool foBloom = wwCellBloomRun( scene, g.color, g.ds, s.w, s.h, s.samples, g.dsFormat, g.dsTex );
	PassTime::subEnd( PassTime::ToneBloom );
	// lane FOGBLOOM1: the per-pixel bloom strength from the air along each ray (cellfogbloom.h); off: no GL call
	const bool fogBloom = wwCellFogBloomRun( scene, g.fbo, g.ds, s.w, s.h, s.samples, g.dsFormat, g.dsTex );
	WwCellUni::knownSlot() = nullptr;	// lane CELLUNI4: the quads change the state

	// lane PERF2: the quad at the far plane keeps the early tests (WW_CELL_HDR_EARLY_RED=1: depth written per sample)
	WW_SWITCH_STATIC( bool, earlyRed, qEnvironmentVariableIntValue( "WW_CELL_HDR_EARLY_RED" ) != 0 );
	auto prog = r->useProgram( earlyRed && !g.dsTex ? "cell_hdr_fd.prog" : "cell_hdr.prog" );
	if ( !prog ) {
		wwCellFogBloomDone( r, kFogBloomUnit );
		wwCellBloomDone( r, kFoBloomUnit );
		note( s, QStringLiteral( "lost (cell_hdr.prog went away mid-frame)" ) );
		return;
	}
	if ( !sync )
		sn.quadsPart( fn );	// red: read here, as before
	const GLint prevActive = sn.active, prevDepthFunc = sn.depthFunc;
	const GLint * prevBlend = sn.blendF;
	const GLboolean * colorMask = sn.colorMask;
	const GLboolean depthMask = sn.depthMask;
	const bool wasDepth = sn.depth, wasBlend = sn.blend, wasCull = sn.cull, wasStencil = sn.stencil, wasScissor = sn.scissor;
	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	PassTime::subBegin( PassTime::ToneQuads );
	wwCellLightsUniforms( scene );	// the imagespace's uniforms (exposure, curve, grade, LUT, bloom)
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
	fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.color );
	prog->uni1i( "hdrSrc", kSrcUnit );
	// lane TONEMAP1: the stencil sampler always gets its own unit (two sampler types on one unit fail the draw)
	if ( g.dsTex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kStencilUnit ) );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, g.ds );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
	}
	prog->uni1i( "hdrStencil", kStencilUnit );
	prog->uni1i( "hdrStencilTest", 0 );
	/* lane CELLUNI5: from here to the fog bloom's Done the active unit is kSrcUnit (the quads draw on it, the VRS build
	 * ends on its own source unit, the same one), so the bloom passes take it instead of asking (red: they ask) */
	static_assert( WwPassUnit::VrsSrc == kSrcUnit, "the VRS build must end on the tone map's source unit" );
	const int quadsActive = sync && !wwCellUniRed() ? int( GL_TEXTURE0 + kSrcUnit ) : -1;
	wwCellFogBloomFinal( r, kFogBloomUnit, fogBloom, quadsActive );
	wwCellBloomFinal( r, kFoBloomUnit, quadsActive );	// lane BLOOM1b: the final combine's uniforms (off: cellIsFoBloomOn false)

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const bool sorted = blitErr == GL_NO_ERROR;
	// lane TONEMAP1: test 1 keeps the samples whose stencil (& 3) is ref, 2 the others; in the shader with the texture
	// WW_CELL_HDR_STEX_BREAK=1 (gates only): mode 1 keeps the wrong samples, so the picture gate must fail
	WW_SWITCH_STATIC( bool, stexBreak, qEnvironmentVariableIntValue( "WW_CELL_HDR_STEX_BREAK" ) == 1 );
	auto pick = [&]( int test, int ref ) {
		if ( stexBreak && ref == 2 )
			test = 2;
		if ( g.dsTex ) {
			prog->uni1i( "hdrStencilTest", test );
			prog->uni1i( "hdrStencilRef", ref );
		} else {
			fn->glStencilFunc( test == 1 ? GL_EQUAL : GL_NOTEQUAL, ref, 0x03 );
		}
	};
	fn->glStencilMask( 0 );
	if ( sorted && !g.dsTex ) {
		fn->glEnable( GL_STENCIL_TEST );
		fn->glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	} else {
		fn->glDisable( GL_STENCIL_TEST );
	}
	if ( sorted )
		pick( 1, 1 );	// a cell program or a cell effect wrote it last: the imagespace
	/* lane SKYHOLE1: a cell effect over the sky (no opaque draw under it, the depth still at the far plane) blended
	 * over 0 in the linear frame, and mode 0 laid its tone-mapped value over the backdrop: the Memory Den's steam
	 * (MistGroundWaterSteam01LongHalf) as a black patch. Mode 0 now takes only the pixels an opaque draw reached
	 * (the quad at the far plane passes GL_GREATER there), mode 2 the rest (GL_LEQUAL), composited by coverage.
	 * WW_CELL_SKYHOLE_RED=old: mode 0 over every 1 pixel, as before. */
	WW_SWITCH_STATIC( bool, skyholeOld, wwEnv( "WW_CELL_SKYHOLE_RED" ).trimmed() == "old" );
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
		pick( 1, 2 );	// any other program: its value as written
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
		pick( 2, 1 );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		if ( splitSky ) {	// the 1s at the far plane: mode 2's pixels
			pick( 1, 1 );
			fn->glEnable( GL_DEPTH_TEST );
			fn->glDepthFunc( GL_LEQUAL );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			fn->glDisable( GL_DEPTH_TEST );
			fn->glDepthFunc( GLenum( prevDepthFunc ) );
		}
		fn->glDisable( GL_BLEND );
	}
	r->stopProgram();
	wwCellBloomDone( r, kFoBloomUnit, quadsActive );
	if ( g.dsTex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kStencilUnit ) );
		fn->glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, 0 );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
	}
	PassTime::subEnd( PassTime::ToneQuads );
	// lane VRS1: the next frame's shading rate images from this picture (returns at once while the row is off)
	if ( !sync && wwCellVrsWanted() )
		fn->glGetIntegerv( GL_VIEWPORT, sn.vp );
	wwCellVrsBuild( scene, s.prevDraw, s.prevRead, s.w, s.h, sn.vp );
	wwCellFogBloomDone( r, kFogBloomUnit, quadsActive );

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
	// lane TONEMAP1 / SYNC1: which stencil path and which query regime ran (the gates read these words)
	const QString lane = QStringLiteral( " [stencil %1, sync %2]" ).arg( g.dsTex ? QStringLiteral( "in shader" ) : QStringLiteral( "blit" ),
		sync ? QStringLiteral( "cached" ) : QStringLiteral( "red" ) );
	note( s, sorted ? QStringLiteral( "linear frame %1x%2, %3 sample(s), one tone map" ).arg( s.w ).arg( s.h ).arg( s.samples ) + lane
		: QStringLiteral( "linear frame %1x%2, %3 sample(s), one tone map over every pixel (depth / stencil blit refused, GL error %4)" )
			.arg( s.w ).arg( s.h ).arg( s.samples ).arg( blitErr ) );
}
