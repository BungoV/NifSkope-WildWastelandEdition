#include "gametaa.h"

#include "gl/glscene.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// lane MOTION1: the game's temporal AA (gametaa.h)

namespace {

const char * const kTaaKey = "Settings/Render/Temporal AA";

// the game's settings, read from the exe (fTAASharpen:Display, fTAALowFreq, fTAAHighFreq, fTAAPostSharpen,
// fTAAPostOverlay); the 128-pixel motion scale, 20, 0.01, 1.001 are the resolve's own literals (game_taa.frag)
constexpr float kSharpen = 1.0f;
constexpr float kLowFreq = 0.5f;
constexpr float kHighFreq = 0.8f;
constexpr float kPostSharpen = 0.21f;
constexpr float kPostOverlay = 0.21f;

constexpr int kUnitCur = 8, kUnitHist = 9, kUnitMv = 10, kUnitDepth = 11;

struct Dmat
{
	double m[4][4];	// [row][col]
	static Dmat identity()
	{
		Dmat d;
		for ( int r = 0; r < 4; r++ )
			for ( int c = 0; c < 4; c++ )
				d.m[r][c] = r == c ? 1.0 : 0.0;
		return d;
	}
	Dmat operator*( const Dmat & b ) const
	{
		Dmat o;
		for ( int r = 0; r < 4; r++ )
			for ( int c = 0; c < 4; c++ ) {
				double s = 0.0;
				for ( int k = 0; k < 4; k++ )
					s += m[r][k] * b.m[k][c];
				o.m[r][c] = s;
			}
		return o;
	}
	bool operator==( const Dmat & b ) const { return std::memcmp( m, b.m, sizeof( m ) ) == 0; }
};

Dmat fromMatrix4( const Matrix4 & p )	// Matrix4 is column-major: data()[c * 4 + r]
{
	Dmat d;
	const float * f = p.data();
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			d.m[r][c] = double( f[c * 4 + r] );
	return d;
}

Dmat fromTransform( const Transform & t )
{
	Dmat d = Dmat::identity();
	for ( int r = 0; r < 3; r++ ) {
		for ( int c = 0; c < 3; c++ )
			d.m[r][c] = double( t.rotation( r, c ) ) * double( t.scale );
		d.m[r][3] = double( t.translation[r] );
	}
	return d;
}

bool invert( const Dmat & a, Dmat & out )
{
	double t[4][8];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 8; c++ )
			t[r][c] = c < 4 ? a.m[r][c] : ( c - 4 == r ? 1.0 : 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( t[r][c] ) > std::fabs( t[p][c] ) )
				p = r;
		if ( std::fabs( t[p][c] ) < 1e-300 )
			return false;
		if ( p != c )
			for ( int k = 0; k < 8; k++ )
				std::swap( t[p][k], t[c][k] );
		const double inv = 1.0 / t[c][c];
		for ( int k = 0; k < 8; k++ )
			t[c][k] *= inv;
		for ( int r = 0; r < 4; r++ ) {
			if ( r == c || t[r][c] == 0.0 )
				continue;
			const double f = t[r][c];
			for ( int k = 0; k < 8; k++ )
				t[r][k] -= f * t[c][k];
		}
	}
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			out.m[r][c] = t[r][c + 4];
	return true;
}

Matrix4 toMatrix4( const Dmat & d )
{
	float f[16];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			f[c * 4 + r] = float( d.m[r][c] );
	return Matrix4( f );
}

/* BSGraphics::State::Halton, step for step in float: 1/base, then while index > 0: r += fmodf(index, base) * f,
 * index = floor(index / base) (as index * (1/base)), f *= 1/base. */
float gameHalton( float base, float index )
{
	const float invB = 1.0f / base;
	float f = invB, r = 0.0f, i = index;
	while ( i > 0.0f ) {
		const float m = std::fmod( i, base ) * f;
		const float q = invB * i;
		r += m;
		i = std::floor( q );
		f *= invB;
	}
	return r;
}

// ImageSpaceEffectTemporalAA::UpdateParams' tent: u = (S t + 1) / 2, 1 - 2 |1/2 - u| on [0, 1], else 0
float gameTent( float t )
{
	const float u = ( kSharpen * t + 1.0f ) * 0.5f;
	if ( u < 0.0f || u > 1.0f )
		return 0.0f;
	return 1.0f - std::fabs( 0.5f - u ) * 2.0f;
}

struct TaaGpu
{
	GLuint inFbo = 0, mvFbo = 0, outFbo = 0;
	GLuint cur = 0, depth = 0, mv = 0, out = 0, hist[2] = { 0, 0 };
	int w = 0, h = 0;
	GLenum dsFormat = 0;
	GLbitfield depthBits = 0;
};

struct TaaState
{
	bool init = false;
	bool on = false, pinned = false;
	bool redNoClamp = false, redWrongJitter = false;
	QString dumpDir;
	int dumpFrame = 8;

	int fixedFrame = -1;	// >= 0: the camera path's frame index
	int autoFrame = 0;
	bool armed = false;

	// this frame
	bool jittered = false;
	int frame = 0, n = 1;
	float offX = 0.0f, offY = 0.0f;
	int W = 0, H = 0;
	Matrix4 projUnjittered, projJittered;

	// the history's frame bookkeeping
	bool haveLast = false;
	int lastFrame = 0;
	Dmat lastVP = Dmat::identity(), lastPrevVP = Dmat::identity();
	int histW = 0, histH = 0;
	int settle = 0;

	QString last = QStringLiteral( "off" );
	QString said;
};

TaaState & ts()
{
	static TaaState s;
	if ( !s.init ) {
		s.init = true;
		const QString pin = qEnvironmentVariable( "WW_TAA" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.on = pin != QLatin1StringView( "0" ) && pin.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.on = false;	// a harness measures what it pins: no pin, no TAA, whatever was saved
			s.pinned = true;
		} else {
			s.on = QSettings().value( QLatin1StringView( kTaaKey ), false ).toBool();
		}
		const QString red = qEnvironmentVariable( "WW_TAA_RED" ).trimmed();
		s.redNoClamp = red.contains( QLatin1StringView( "noclamp" ) );
		s.redWrongJitter = red.contains( QLatin1StringView( "wrongjitter" ) );
		s.dumpDir = qEnvironmentVariable( "WW_TAA_DUMP" ).trimmed();
		bool ok = false;
		const int df = qEnvironmentVariable( "WW_TAA_DUMP_FRAME" ).toInt( &ok );
		if ( ok )
			s.dumpFrame = df;
	}
	return s;
}

QHash<Renderer *, TaaGpu> & taaGpus()
{
	static QHash<Renderer *, TaaGpu> g;
	return g;
}

void note( TaaState & s, const QString & line )
{
	s.last = line;
	if ( s.said != line && !line.startsWith( QLatin1StringView( "frame " ) ) ) {	// changes only, not every frame
		s.said = line;
		qInfo().noquote() << "temporal aa:" << line;
	}
}

GLuint makeTex( Renderer * r, GLenum internal, GLenum format, GLenum type, int w, int h, GLenum filter )
{
	auto fn = r->fn;
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( internal ), w, h, 0, format, type, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint( filter ) );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLint( filter ) );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
	if ( format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL )
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

void freeGpu( Renderer * r, TaaGpu & g )
{
	auto fn = r->fn;
	GLuint texs[6] = { g.cur, g.depth, g.mv, g.out, g.hist[0], g.hist[1] };
	for ( GLuint t : texs )
		if ( t )
			fn->glDeleteTextures( 1, &t );
	g.cur = g.depth = g.mv = g.out = g.hist[0] = g.hist[1] = 0;
}

// a float32 dump: int32 w h channels, then rows bottom-up (GL order), channels interleaved
bool dumpFloat( const QString & path, int w, int h, int ch, const std::vector<float> & v )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	const std::int32_t hdr[3] = { w, h, ch };
	f.write( reinterpret_cast<const char *>( hdr ), sizeof( hdr ) );
	f.write( reinterpret_cast<const char *>( v.data() ), qint64( v.size() * sizeof( float ) ) );
	return true;
}

std::vector<float> readFloat( Renderer * r, GLuint fbo, GLenum attachment, GLenum format, int ch, int w, int h )
{
	auto fn = r->fn;
	std::vector<float> v( size_t( w ) * size_t( h ) * size_t( ch ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	if ( attachment != GL_DEPTH_ATTACHMENT )
		fn->glReadBuffer( attachment );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	fn->glReadPixels( 0, 0, w, h, format, GL_FLOAT, v.data() );
	return v;
}

QString f9( double v )
{
	return QString::number( v, 'g', 9 );
}

QString mat( const Matrix4 & m )
{
	QStringList l;
	for ( int i = 0; i < 16; i++ )
		l << f9( m.data()[i] );
	return l.join( QLatin1Char( ' ' ) );
}

}	// namespace

bool wwGameTaaOn()
{
	return ts().on;
}

void wwGameTaaSetOn( bool on )
{
	TaaState & s = ts();
	s.on = on;
	s.haveLast = false;	// the history starts again
	if ( !s.pinned )
		QSettings().setValue( QLatin1StringView( kTaaKey ), on );
}

void wwGameTaaSetFrame( int index )
{
	ts().fixedFrame = index;
}

void wwGameTaaArmJitter( bool armed )
{
	ts().armed = armed;
	if ( armed )
		ts().jittered = false;	// a frame that never reached its resolve leaves nothing behind (disarming keeps it)
}

void wwGameTaaJitterProjection( Scene * scene, Matrix4 & proj )
{
	TaaState & s = ts();
	if ( !s.armed || !s.on || !scene || !scene->renderer || scene->selecting )
		return;
	s.armed = false;	// one projection a frame: a tool's own glProjection later in the frame is not jittered again
	GLint vp[4] = { 0, 0, 1, 1 };
	scene->renderer->fn->glGetIntegerv( GL_VIEWPORT, vp );
	s.W = std::max( int( vp[2] ), 1 );
	s.H = std::max( int( vp[3] ), 1 );
	if ( s.fixedFrame >= 0 ) {
		s.frame = s.fixedFrame;
	} else {
		s.autoFrame++;
		s.frame = s.autoFrame;
	}
	// BSGraphics::State::UpdateTemporalData: iCurrentFrame = (iCurrentFrame + 1) & 7, n = iCurrentFrame + 1;
	// frame index k here IS that counter after its increment
	s.n = ( s.frame & 7 ) + 1;
	const float bx = s.redWrongJitter ? 3.0f : 2.0f, by = s.redWrongJitter ? 2.0f : 3.0f;
	s.offX = ( gameHalton( bx, float( s.n ) ) * 2.0f - 1.0f ) / float( s.W );
	s.offY = ( gameHalton( by, float( s.n ) ) * 2.0f - 1.0f ) / float( s.H );
	s.projUnjittered = proj;
	// ProjMat * J with J's translation row (offX, offY, 0, 1): clip.xy += off * clip.w (column-major m[c][r])
	float * m = const_cast<float *>( proj.data() );
	for ( int c = 0; c < 4; c++ ) {
		m[c * 4 + 0] += s.offX * m[c * 4 + 3];
		m[c * 4 + 1] += s.offY * m[c * 4 + 3];
	}
	s.projJittered = proj;
	s.jittered = true;
}

bool wwGameTaaUnjittered( Matrix4 & proj )
{
	const TaaState & s = ts();
	if ( !s.jittered )
		return false;
	proj = s.projUnjittered;
	return true;
}

bool wwGameTaaWantsSettle()
{
	TaaState & s = ts();
	if ( !s.on || s.fixedFrame >= 0 || wwHarnessRun() || s.settle <= 0 )
		return false;
	s.settle--;
	return true;
}

QString wwGameTaaEcho()
{
	return ts().last;
}

void wwGameTaaResolve( Scene * scene )
{
	TaaState & s = ts();
	if ( !s.jittered ) {
		if ( s.on && scene && !scene->selecting )
			note( s, QStringLiteral( "refused (no jittered projection this frame)" ) );
		else if ( !s.on )
			s.last = QStringLiteral( "off" );
		return;
	}
	s.jittered = false;
	if ( !scene || !scene->renderer || scene->selecting )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}

	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const int W = vp[2], H = vp[3];
	if ( W != s.W || H != s.H ) {
		note( s, QStringLiteral( "refused (the viewport changed inside the frame)" ) );
		return;
	}

	// the frame's depth format: the depth goes over by a blit, which needs the same format both sides
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	const GLenum att = prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
		GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
	fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
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
		note( s, QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil)" ).arg( depthBits ).arg( stencilBits ) );
		return;
	}

	TaaGpu & g = taaGpus()[r];
	const bool resized = !g.inFbo || g.w != W || g.h != H || g.dsFormat != dsFormat;
	if ( resized ) {
		freeGpu( r, g );
		if ( !g.inFbo ) {
			fn->glGenFramebuffers( 1, &g.inFbo );
			fn->glGenFramebuffers( 1, &g.mvFbo );
			fn->glGenFramebuffers( 1, &g.outFbo );
		}
		g.cur = makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, GL_NEAREST );		// t0 NEAREST
		g.depth = makeTex( r, dsFormat, dsFmt, dsType, W, H, GL_NEAREST );				// t3 NEAREST
		g.mv = makeTex( r, GL_RG32F, GL_RG, GL_FLOAT, W, H, GL_NEAREST );				// t2 NEAREST
		g.out = makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, GL_NEAREST );
		g.hist[0] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );		// t1 BILERP
		g.hist[1] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.inFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.cur, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, dsFmt == GL_DEPTH_STENCIL ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, g.depth, 0 );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.mvFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.mv, 0 );
		g.w = W;
		g.h = H;
		g.dsFormat = dsFormat;
		g.depthBits = blitBits;
		s.haveLast = false;
	}

	// the history's frame: the next index continues it, the same index repeats it, anything else starts again
	const Dmat V = fromTransform( scene->view );
	const Dmat curVP = fromMatrix4( s.projUnjittered ) * V;
	Dmat prevVP = curVP;
	bool reset = true;
	if ( s.haveLast && s.histW == W && s.histH == H ) {
		if ( s.frame == s.lastFrame + 1 ) {
			prevVP = s.lastVP;
			reset = false;
		} else if ( s.frame == s.lastFrame ) {
			prevVP = s.lastPrevVP;
			reset = false;
		}
	}
	const int wr = s.frame & 1, rd = wr ^ 1;
	if ( reset ) {
		// both histories black: the first frame's history is the bracket's clamp of nothing
		for ( int i = 0; i < 2; i++ ) {
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.outFbo );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.hist[i], 0 );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0 );
			const GLenum one = GL_COLOR_ATTACHMENT0;
			fn->glDrawBuffers( 1, &one );
			const GLfloat zero[4] = { 0, 0, 0, 0 };
			fn->glClearBufferfv( GL_COLOR, 0, zero );
		}
	}
	if ( s.frame != s.lastFrame || !s.haveLast ) {
		if ( !( curVP == s.lastVP ) )
			s.settle = 16;
	}
	s.lastPrevVP = prevVP;
	s.lastVP = curVP;
	s.lastFrame = s.frame;
	s.haveLast = true;
	s.histW = W;
	s.histH = H;

	// the camera's reprojection, current unjittered clip -> previous unjittered clip, built in double
	Dmat invCur, reproj = Dmat::identity();
	if ( !reset && invert( curVP, invCur ) )
		reproj = prevVP * invCur;

	// the game's constants (ImageSpaceEffectTemporalAA::UpdateParams)
	const float px = s.offX * 0.5f * float( W ), py = s.offY * 0.5f * float( H );
	const float sx = std::ceil( s.offX ) > 0.5f ? -1.0f : 1.0f;
	const float sy = std::ceil( s.offY ) > 0.5f ? -1.0f : 1.0f;
	const float tx0 = gameTent( px ), ty0 = gameTent( py ), tx1 = gameTent( px + sx ), ty1 = gameTent( py + sy );
	const float w0 = ty0 * tx0, w1 = ty1 * tx0, w2 = ty0 * tx1, w3 = ty1 * tx1;
	const float inv = 1.0f / ( ( ( w0 + w1 ) + w2 ) + w3 );
	const FloatVector4 c0( 1.0f / float( W ), 1.0f / float( H ), 1.0f, 1.0f );
	const FloatVector4 c2( w0 * inv, w1 * inv, w2 * inv, w3 * inv );
	// c3: (s_x / W, s_y / H) in the game's texture space, v DOWN; here v is up, so its y turns over
	const FloatVector4 c3( sx / float( W ), -sy / float( H ), 0.0f, 0.0f );
	const FloatVector4 c4( kLowFreq, kHighFreq, kPostSharpen, kPostOverlay );
	const FloatVector4 c5( 1.0f / float( W ), 1.0f / float( H ), 1.0f, 1.0f );

	// state kept for the restore
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );

	// 1. the frame (resolved when multisampled) into t0 and t3
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, g.depthBits, GL_NEAREST );
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
	bool ok = blitErr == GL_NO_ERROR;
	const Matrix4 reproj4 = toMatrix4( reproj );

	// 2. the motion vectors (t2): the camera's reprojection of the frame's depth
	if ( ok ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.mvFbo );
		auto prog = r->useProgram( "game_taa_mv.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitDepth ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.depth );
			prog->uni1i( "depthTex", kUnitDepth );
			prog->uni4m( "reproj", reproj4 );
			prog->uni2f( "jitterNdc", s.offX, s.offY );
			prog->uni2f( "invSize", 1.0f / float( W ), 1.0f / float( H ) );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
	}

	// 3. the resolve: o0 the next history, o1 the picture (tap 1: o0 the filtered history sample, for the dump)
	auto resolvePass = [&]( int tap ) -> bool {
		auto prog = r->useProgram( "game_taa.prog" );
		if ( !prog )
			return false;
		{
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitCur ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.cur );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitHist ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.hist[rd] );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitMv ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.mv );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitDepth ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.depth );
			prog->uni1i( "curTex", kUnitCur );
			prog->uni1i( "histTex", kUnitHist );
			prog->uni1i( "mvTex", kUnitMv );
			prog->uni1i( "depthTex", kUnitDepth );
			prog->uni4f( "c0", c0 );
			prog->uni4f( "c2", c2 );
			prog->uni4f( "c3", c3 );
			prog->uni4f( "c4", c4 );
			prog->uni4f( "c5", c5 );
			prog->uni1i( "taaRed", s.redNoClamp ? 1 : 0 );
			prog->uni1i( "taaTap", tap );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
		return true;
	};
	if ( ok ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.outFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.hist[wr], 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.out, 0 );
		const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, two );
		ok = resolvePass( 0 );
	}

	// the dump: every input and output of this frame's resolve, before the picture goes back
	if ( ok && !s.dumpDir.isEmpty() && s.frame == s.dumpFrame ) {
		QDir().mkpath( s.dumpDir );
		const QString d = s.dumpDir + QLatin1Char( '/' );
		// the history as the filtering hardware returned it to this resolve (its sub-texel weights are the
		// hardware's, not specified to the bit): the checker takes the luma from here and judges the filter apart
		{
			const GLuint tapTex = makeTex( r, GL_RGBA32F, GL_RGBA, GL_FLOAT, W, H, GL_NEAREST );
			fn->glBindFramebuffer( GL_FRAMEBUFFER, g.outFbo );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tapTex, 0 );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0 );
			const GLenum one = GL_COLOR_ATTACHMENT0;
			fn->glDrawBuffers( 1, &one );
			if ( resolvePass( 1 ) )
				dumpFloat( d + "hist_tap.bin", W, H, 4, readFloat( r, g.outFbo, GL_COLOR_ATTACHMENT0, GL_RGBA, 4, W, H ) );
			fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
			fn->glDeleteTextures( 1, &tapTex );
		}
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.outFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.cur, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.hist[rd], 0 );
		dumpFloat( d + "cur.bin", W, H, 4, readFloat( r, g.outFbo, GL_COLOR_ATTACHMENT0, GL_RGBA, 4, W, H ) );
		dumpFloat( d + "hist_in.bin", W, H, 4, readFloat( r, g.outFbo, GL_COLOR_ATTACHMENT1, GL_RGBA, 4, W, H ) );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.hist[wr], 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.out, 0 );
		dumpFloat( d + "hist_out.bin", W, H, 4, readFloat( r, g.outFbo, GL_COLOR_ATTACHMENT0, GL_RGBA, 4, W, H ) );
		dumpFloat( d + "out.bin", W, H, 4, readFloat( r, g.outFbo, GL_COLOR_ATTACHMENT1, GL_RGBA, 4, W, H ) );
		dumpFloat( d + "mv.bin", W, H, 2, readFloat( r, g.mvFbo, GL_COLOR_ATTACHMENT0, GL_RG, 2, W, H ) );
		dumpFloat( d + "depth.bin", W, H, 1, readFloat( r, g.inFbo, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT, 1, W, H ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		QFile f( d + "taa.txt" );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream t( &f );
			// read back from the GPU's own copy where there is one: the projection as uploaded
			const Matrix4 uploaded( &( r->globalUniforms->projectionMatrix[0][0] ) );
			t << "frame " << s.frame << "\nn " << s.n << "\nW " << W << "\nH " << H
			  << "\noffX " << f9( s.offX ) << "\noffY " << f9( s.offY )
			  << "\nc0 " << f9( c0[0] ) << ' ' << f9( c0[1] ) << ' ' << f9( c0[2] ) << ' ' << f9( c0[3] )
			  << "\nc2 " << f9( c2[0] ) << ' ' << f9( c2[1] ) << ' ' << f9( c2[2] ) << ' ' << f9( c2[3] )
			  << "\nc3 " << f9( c3[0] ) << ' ' << f9( c3[1] ) << ' ' << f9( c3[2] ) << ' ' << f9( c3[3] )
			  << "\nc4 " << f9( c4[0] ) << ' ' << f9( c4[1] ) << ' ' << f9( c4[2] ) << ' ' << f9( c4[3] )
			  << "\nc5 " << f9( c5[0] ) << ' ' << f9( c5[1] ) << ' ' << f9( c5[2] ) << ' ' << f9( c5[3] )
			  << "\nproj_unjittered " << mat( s.projUnjittered )
			  << "\nproj_jittered " << mat( s.projJittered )
			  << "\nproj_uploaded " << mat( uploaded )
			  << "\nreproj " << mat( reproj4 )
			  << "\nreset " << ( reset ? 1 : 0 )
			  << "\nred " << ( s.redNoClamp ? "noclamp" : s.redWrongJitter ? "wrongjitter" : "none" ) << '\n';
		}
		note( s, QStringLiteral( "dumped frame %1 to %2" ).arg( s.frame ).arg( s.dumpDir ) );
	}

	// 4. the picture back into the frame (every sample of a multisampled frame the same value)
	if ( ok ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		auto prog = r->useProgram( "game_taa_copy.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitCur ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.out );
			prog->uni1i( "srcTex", kUnitCur );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
	}

	for ( int u : { kUnitCur, kUnitHist, kUnitMv, kUnitDepth } ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	fn->glActiveTexture( GLenum( prevActive ) );
	if ( wasStencil ) fn->glEnable( GL_STENCIL_TEST ); else fn->glDisable( GL_STENCIL_TEST );
	if ( wasDepth ) fn->glEnable( GL_DEPTH_TEST ); else fn->glDisable( GL_DEPTH_TEST );
	if ( wasBlend ) fn->glEnable( GL_BLEND ); else fn->glDisable( GL_BLEND );
	if ( wasCull ) fn->glEnable( GL_CULL_FACE ); else fn->glDisable( GL_CULL_FACE );
	if ( wasScissor ) fn->glEnable( GL_SCISSOR_TEST );
	fn->glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );
	fn->glDepthMask( depthMask );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	if ( !ok ) {
		s.haveLast = false;
		note( s, blitErr != GL_NO_ERROR ? QStringLiteral( "refused (the frame's blit failed, GL error %1)" ).arg( blitErr )
			: QStringLiteral( "refused (a game_taa program is missing)" ) );
		return;
	}
	note( s, QStringLiteral( "frame %1 n=%2 off=(%3,%4)px%5%6" ).arg( s.frame ).arg( s.n )
		.arg( f9( px ), f9( py ), reset ? QStringLiteral( " history-reset" ) : QString(),
			s.redNoClamp ? QStringLiteral( " RED noclamp" ) : s.redWrongJitter ? QStringLiteral( " RED wrongjitter" ) : QString() ) );
}
