#include "cellvrs.h"

#include "gl/cellhdr.h"
#include "gl/cellpasstime.h"
#include "gl/gametaa.h"
#include "gl/glscene.h"
#include "gl/renderer.h"

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QOpenGLContext>

#include <algorithm>
#include <cstdint>
#include <vector>

// lane VRS1: variable rate shading in the cell view (cellvrs.h)

#ifndef GL_SHADING_RATE_IMAGE_NV
#define GL_SHADING_RATE_IMAGE_NV 0x9563
#define GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV 0x9565
#define GL_SHADING_RATE_1_INVOCATION_PER_1X2_PIXELS_NV 0x9566
#define GL_SHADING_RATE_1_INVOCATION_PER_2X1_PIXELS_NV 0x9567
#define GL_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV 0x9568
#define GL_SHADING_RATE_1_INVOCATION_PER_2X4_PIXELS_NV 0x9569
#define GL_SHADING_RATE_1_INVOCATION_PER_4X2_PIXELS_NV 0x956A
#define GL_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV 0x956B
#define GL_SHADING_RATE_IMAGE_TEXEL_WIDTH_NV 0x955C
#define GL_SHADING_RATE_IMAGE_TEXEL_HEIGHT_NV 0x955D
#define GL_SHADING_RATE_IMAGE_PALETTE_SIZE_NV 0x955E
#define GL_MAX_COARSE_FRAGMENT_SAMPLES_NV 0x955F
#endif

namespace {

constexpr int kUnitSrc = 8;	// the units cellhdr's tone map used (free again once its quads are drawn)
constexpr int kUnitMv = 9;
constexpr int kPalette = 7;	// cell_vrs_rate.frag's indices: 1x1, 1x2, 2x1, 2x2, 2x4, 4x2, 4x4 (width x height)

using BindImageFn = void ( * )( GLuint );
using PaletteFn = void ( * )( GLuint, GLuint, GLsizei, const GLenum * );
using TexStorageFn = void ( * )( GLenum, GLsizei, GLenum, GLsizei, GLsizei );

enum Red { RedNone, Red1x1, Red4x4, RedNoAlpha };

struct VrsCfg
{
	bool on = false;
	int red = RedNone;
	float cut = 0.025f, cutSorted = 0.0125f, motion = 0.25f;
	int maxRate = 2;
	bool census = false;
	QString dump;
};

const VrsCfg & cfg()
{
	static const VrsCfg c = [] {
		VrsCfg v;
		v.on = qEnvironmentVariableIntValue( "WW_CELL_VRS" ) == 1;
		const QByteArray red = qgetenv( "WW_CELL_VRS_RED" ).trimmed().toLower();
		v.red = red == "1x1" ? Red1x1 : red == "4x4" ? Red4x4 : red == "noalpha" ? RedNoAlpha : RedNone;
		bool ok = false;
		float f = qEnvironmentVariable( "WW_CELL_VRS_THRESH" ).toFloat( &ok );
		if ( ok && f >= 0.0f )
			v.cut = f;
		f = qEnvironmentVariable( "WW_CELL_VRS_THRESH_SORTED" ).toFloat( &ok );
		v.cutSorted = ok && f >= 0.0f ? f : v.cut * 0.5f;
		f = qEnvironmentVariable( "WW_CELL_VRS_MOTION" ).toFloat( &ok );
		if ( ok && f >= 0.0f )
			v.motion = f;
		v.maxRate = qgetenv( "WW_CELL_VRS_MAX" ).trimmed() == "4x4" ? 4 : 2;
		v.census = qEnvironmentVariableIntValue( "WW_CELL_VRS_CENSUS" ) == 1;
		v.dump = qEnvironmentVariable( "WW_CELL_VRS_DUMP" );
		return v;
	}();
	return c;
}

struct VrsGpu
{
	bool init = false, ok = false;	// the extension and its entry points were looked up / are there
	BindImageFn bindImage = nullptr;
	PaletteFn palette = nullptr;
	TexStorageFn texStorage = nullptr;	// the rate images are immutable R8UI (GL 4.2; the view's functions are 4.1)
	bool passChecked = false;	// the first pass through these images read its GL error
	GLint texelW = 16, texelH = 16, paletteSize = 0, coarseSamples = 0;
	GLuint srcFbo = 0, src = 0, rateFbo = 0, rate[2] = { 0, 0 };
	int w = 0, h = 0, tw = 0, th = 0;
	bool trusted = false;	// a build on these targets raised no GL error
	bool ready = false;	// the images hold a frame of size w x h
	int built = 0;
};

struct VrsState
{
	bool active = false, enabled = false;
	bool alphaOff = false;	// the current draw is alpha-tested (shading 1x1)
	Renderer * r = nullptr;
	QString last = QStringLiteral( "off" );
	QString said;
};

VrsState & vst()
{
	static VrsState s;
	return s;
}

QHash<Renderer *, VrsGpu> & vrsGpus()
{
	static QHash<Renderer *, VrsGpu> g;
	return g;
}

void note( const QString & line )
{
	VrsState & s = vst();
	s.last = line;
	if ( s.said != line ) {
		s.said = line;
		qInfo().noquote() << "cell vrs:" << line;
	}
}

QString rateName( int i )
{
	static const char * n[kPalette] = { "1x1", "1x2", "2x1", "2x2", "2x4", "4x2", "4x4" };
	return QString::fromLatin1( i >= 0 && i < kPalette ? n[i] : "?" );
}

bool initGpu( Renderer * r, VrsGpu & g )
{
	if ( g.init )
		return g.ok;
	g.init = true;
	QOpenGLContext * cx = QOpenGLContext::currentContext();
	if ( !cx || !cx->hasExtension( QByteArrayLiteral( "GL_NV_shading_rate_image" ) ) ) {
		note( QStringLiteral( "off (no GL_NV_shading_rate_image on this card)" ) );
		return false;
	}
	g.bindImage = reinterpret_cast<BindImageFn>( cx->getProcAddress( "glBindShadingRateImageNV" ) );
	g.palette = reinterpret_cast<PaletteFn>( cx->getProcAddress( "glShadingRateImagePaletteNV" ) );
	g.texStorage = reinterpret_cast<TexStorageFn>( cx->getProcAddress( "glTexStorage2D" ) );
	if ( !g.bindImage || !g.palette || !g.texStorage ) {
		note( QStringLiteral( "off (GL_NV_shading_rate_image without its entry points)" ) );
		return false;
	}
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}
	fn->glGetIntegerv( GL_SHADING_RATE_IMAGE_TEXEL_WIDTH_NV, &g.texelW );
	fn->glGetIntegerv( GL_SHADING_RATE_IMAGE_TEXEL_HEIGHT_NV, &g.texelH );
	fn->glGetIntegerv( GL_SHADING_RATE_IMAGE_PALETTE_SIZE_NV, &g.paletteSize );
	fn->glGetIntegerv( GL_MAX_COARSE_FRAGMENT_SAMPLES_NV, &g.coarseSamples );
	if ( fn->glGetError() != GL_NO_ERROR || g.texelW < 1 || g.texelH < 1 || g.texelW > 64 || g.texelH > 64
		|| g.paletteSize < kPalette ) {
		note( QStringLiteral( "off (the shading rate image's limits did not read: texel %1x%2, palette %3)" )
			.arg( g.texelW ).arg( g.texelH ).arg( g.paletteSize ) );
		return false;
	}
	g.ok = true;
	return true;
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
	return t;
}

void makeTargets( Renderer * r, VrsGpu & g, int W, int H )
{
	auto fn = r->fn;
	if ( !g.srcFbo ) {
		fn->glGenFramebuffers( 1, &g.srcFbo );
		fn->glGenFramebuffers( 1, &g.rateFbo );
	}
	GLuint old[3] = { g.src, g.rate[0], g.rate[1] };
	for ( GLuint t : old )
		if ( t )
			fn->glDeleteTextures( 1, &t );
	g.tw = ( W + g.texelW - 1 ) / g.texelW;
	g.th = ( H + g.texelH - 1 ) / g.texelH;
	// the picture as the view holds it (display values, 8 bits): a resolve needs the view's format, RGBA8 here
	g.src = makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H );
	for ( GLuint & t : g.rate ) {
		fn->glGenTextures( 1, &t );
		fn->glBindTexture( GL_TEXTURE_2D, t );
		g.texStorage( GL_TEXTURE_2D, 1, GL_R8UI, g.tw, g.th );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	}
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.srcFbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.src, 0 );
	fn->glBindFramebuffer( GL_FRAMEBUFFER, g.rateFbo );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.rate[0], 0 );
	fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.rate[1], 0 );
	const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	fn->glDrawBuffers( 2, bufs );
	g.w = W;
	g.h = H;
	g.trusted = false;
	g.ready = false;
	g.passChecked = false;
}

// gates only: the share of each rate in both images, and the PGM dumps (a read-back: waits for the GPU)
void census( Renderer * r, VrsGpu & g )
{
	const VrsCfg & c = cfg();
	if ( !c.census && c.dump.isEmpty() )
		return;
	auto fn = r->fn;
	std::vector<std::uint8_t> px( size_t( g.tw ) * size_t( g.th ) );
	QString line = QStringLiteral( "census %1x%2 tiles" ).arg( g.tw ).arg( g.th );
	for ( int k = 0; k < 2; k++ ) {
		fn->glBindTexture( GL_TEXTURE_2D, g.rate[k] );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		fn->glGetTexImage( GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, px.data() );
		fn->glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		int n[kPalette] = {};
		for ( std::uint8_t v : px )
			n[std::min<int>( v, kPalette - 1 )]++;
		line += k ? QStringLiteral( " | sorted" ) : QStringLiteral( ", opaque" );
		for ( int i = 0; i < kPalette; i++ )
			if ( n[i] )
				line += QStringLiteral( " %1 %2%" ).arg( rateName( i ) ).arg( 100.0 * n[i] / double( px.size() ), 0, 'f', 1 );
		if ( !c.dump.isEmpty() ) {	// P5, top row first, index x 40
			QFile f( k ? c.dump + QStringLiteral( ".sorted.pgm" ) : c.dump );
			if ( f.open( QIODevice::WriteOnly ) ) {
				f.write( QStringLiteral( "P5\n%1 %2\n255\n" ).arg( g.tw ).arg( g.th ).toLatin1() );
				for ( int y = g.th - 1; y >= 0; y-- ) {
					QByteArray row( g.tw, 0 );
					for ( int x = 0; x < g.tw; x++ )
						row[x] = char( std::min( 255, px[size_t( y ) * size_t( g.tw ) + size_t( x )] * 40 ) );
					f.write( row );
				}
			}
		}
	}
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	if ( c.census )
		qInfo().noquote() << "cell vrs:" << line;
}

}	// namespace

bool wwCellVrsWanted()
{
	return cfg().on;
}

bool wwCellVrsActive()
{
	return vst().active;
}

QString wwCellVrsEcho()
{
	return vst().last;
}

void wwCellVrsBuild( Scene * scene, int prevDraw, int prevRead, int W, int H, const int vp[4] )
{
	const VrsCfg & c = cfg();
	if ( !c.on ) {
		static bool said = false;
		if ( !said ) {
			said = true;
			note( QStringLiteral( "off (WW_CELL_VRS unset)" ) );
		}
		return;
	}
	if ( !scene || !scene->renderer || scene->selecting || W < 1 || H < 1 )
		return;
	Renderer * r = scene->renderer;
	VrsGpu & g = vrsGpus()[r];
	if ( !initGpu( r, g ) )
		return;
	auto fn = r->fn;
	PassTime::subBegin( PassTime::ToneVrs );
	const bool fresh = !g.srcFbo || g.w != W || g.h != H;
	if ( fresh )
		makeTargets( r, g, W, H );
	const bool check = !g.trusted;	// errors read on new targets only (lane SYNC1's regime)
	if ( check ) {
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	// 1. this frame's finished picture, resolved (the view is multisampled)
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.srcFbo );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	// 2. the two rate images, one texel per tile
	bool ok = true;
	auto prog = r->useProgram( "cell_vrs_rate.prog" );
	if ( !prog ) {
		ok = false;
	} else {
		int mw = 0, mh = 0;
		const GLuint mv = c.motion > 0.0f ? wwGameTaaMotionTex( scene, mw, mh ) : 0;
		const bool mvOn = mv && mw == W && mh == H;
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.rateFbo );
		fn->glViewport( 0, 0, g.tw, g.th );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_STENCIL_TEST );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glDisable( GL_CULL_FACE );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitSrc ) );
		fn->glBindTexture( GL_TEXTURE_2D, g.src );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitMv ) );
		fn->glBindTexture( GL_TEXTURE_2D, mvOn ? mv : g.src );	// a valid sampler2D either way; unread without vectors
		prog->uni1i( "vrsSrc", kUnitSrc );
		prog->uni1i( "vrsMv", kUnitMv );
		prog->uni1i( "vrsMvOn", mvOn ? 1 : 0 );
		prog->uni2f( "vrsSize", float( W ), float( H ) );
		prog->uni2f( "vrsTile", float( g.texelW ), float( g.texelH ) );
		prog->uni1f( "vrsCut", c.cut );
		prog->uni1f( "vrsCutSorted", c.cutSorted );
		prog->uni1i( "vrsMax", c.maxRate );
		prog->uni1f( "vrsMotion", c.motion );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		const float * attrs = quad;
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		r->stopProgram();
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitSrc ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		static int saidMv = -1;
		if ( saidMv != int( mvOn ) ) {
			saidMv = int( mvOn );
			qInfo().noquote() << "cell vrs: motion" << ( mvOn ? "from the temporal AA's vectors" : "none (no vectors this frame)" );
		}
	}
	if ( ok && check ) {
		const GLenum err = fn->glGetError();
		ok = err == GL_NO_ERROR;
		if ( !ok )
			note( QStringLiteral( "refused (the rate build raised GL error 0x%1)" ).arg( err, 0, 16 ) );
		else
			g.trusted = true;
	}
	g.ready = ok;
	g.built++;
	if ( ok )
		census( r, g );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	PassTime::subEnd( PassTime::ToneVrs );
}

void wwCellVrsPass( Scene * scene, bool sorted, bool on )
{
	const VrsCfg & c = cfg();
	if ( !c.on )
		return;
	VrsState & s = vst();
	if ( !on ) {
		if ( s.active ) {
			VrsGpu & g = vrsGpus()[s.r];
			if ( s.enabled )
				s.r->fn->glDisable( GL_SHADING_RATE_IMAGE_NV );
			g.bindImage( 0 );
			s.active = s.enabled = false;
			s.r = nullptr;
		}
		return;
	}
	if ( s.active || !scene || !scene->renderer || scene->selecting || !wwCellHdrActive() )
		return;
	Renderer * r = scene->renderer;
	auto it = vrsGpus().find( r );
	if ( it == vrsGpus().end() || !it->ok || !it->ready )
		return;	// the first frame (no picture yet) or refused: every pixel shaded
	int fw = 0, fh = 0;
	wwCellHdrFrameSize( fw, fh );
	if ( fw != it->w || fh != it->h )
		return;	// the frame changed size: the next frame's image fits it
	VrsGpu & g = *it;
	static const GLenum kRates[kPalette] = { GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV, GL_SHADING_RATE_1_INVOCATION_PER_1X2_PIXELS_NV,
		GL_SHADING_RATE_1_INVOCATION_PER_2X1_PIXELS_NV, GL_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV,
		GL_SHADING_RATE_1_INVOCATION_PER_2X4_PIXELS_NV, GL_SHADING_RATE_1_INVOCATION_PER_4X2_PIXELS_NV,
		GL_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV };
	GLenum pal[kPalette];
	for ( int i = 0; i < kPalette; i++ )
		pal[i] = c.red == Red1x1 ? GLenum( GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV )
			: c.red == Red4x4 ? GLenum( GL_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV ) : kRates[i];
	if ( !g.passChecked ) {
		while ( r->fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	g.palette( 0, 0, kPalette, pal );
	g.bindImage( g.rate[sorted ? 1 : 0] );
	r->fn->glEnable( GL_SHADING_RATE_IMAGE_NV );
	if ( !g.passChecked ) {	// once per new image: a driver that refuses the binding turns the lane off
		g.passChecked = true;
		const GLenum err = r->fn->glGetError();
		if ( err != GL_NO_ERROR ) {
			r->fn->glDisable( GL_SHADING_RATE_IMAGE_NV );
			g.bindImage( 0 );
			while ( r->fn->glGetError() != GL_NO_ERROR ) {
			}
			g.ok = false;
			note( QStringLiteral( "refused (binding the rate image raised GL error 0x%1)" ).arg( err, 0, 16 ) );
			return;
		}
	}
	s.active = s.enabled = true;
	s.alphaOff = false;
	s.r = r;
	static const char * redNames[4] = { "", ", RED 1x1 (every rate 1x1)", ", RED 4x4 (every tile 4x4)",
		", RED noalpha (alpha-tested draws coarse too)" };
	note( QStringLiteral( "on, NV shading rate image %1x%2 px tiles (%3x%4), allowance %5 opaque / %6 sorted, max %7, motion %8, "
		"coarse samples max %9, alpha-tested draws 1x1%10" )
		.arg( g.texelW ).arg( g.texelH ).arg( g.tw ).arg( g.th ).arg( double( c.cut ) ).arg( double( c.cutSorted ) )
		.arg( c.maxRate == 4 ? QStringLiteral( "4x4" ) : QStringLiteral( "2x2" ) ).arg( double( c.motion ) )
		.arg( g.coarseSamples ).arg( QString::fromLatin1( redNames[c.red] ) ) );
}

void wwCellVrsDraw( bool alphaTested )
{
	VrsState & s = vst();
	if ( !s.active )
		return;
	const bool want = !( alphaTested && cfg().red != RedNoAlpha );
	if ( want == s.enabled )
		return;
	if ( want )
		s.r->fn->glEnable( GL_SHADING_RATE_IMAGE_NV );
	else
		s.r->fn->glDisable( GL_SHADING_RATE_IMAGE_NV );
	s.enabled = want;
}
