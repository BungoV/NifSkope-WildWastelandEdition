#include "cellpost.h"

#include "esmdata.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/renderer.h"
#include "harnesswindow.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "esmfile.hpp"

// lane GRASSMB1: the game's depth of field and motion blur on the cell view (cellpost.h)

namespace {

const char * const kDofKey = "WW/CellDof";
const char * const kMblurKey = "WW/CellMotionBlur";

// ImageSpaceEffectMotionBlur::UpdateParams: the imagespace's motion blur scale (50, times the literal 0.001 and
// 1 / the frame time), its max blur (0.01) and fThreshold (70)
constexpr float kMbScale = 50.0f;
constexpr float kMbMax = 0.01f;
constexpr float kMbThreshold = 70.0f;

constexpr int kUnitCur = 12, kUnitBlur = 13, kUnitDepth = 14, kUnitMv = 15;

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
};

Dmat fromColumnMajor( const float * f )	// Matrix4 is column-major: data()[c * 4 + r]
{
	Dmat d;
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

struct PostGpu
{
	GLuint inFbo = 0, fbo = 0, mvFbo = 0;
	GLuint cur = 0, depth = 0, blur[2] = { 0, 0 }, mv = 0, out = 0;
	int w = 0, h = 0;
	GLenum dsFormat = 0;
	GLbitfield depthBits = 0;
};

struct PostState
{
	bool init = false;
	bool dof = false, dofPinned = false, mb = false, mbPinned = false;
	bool redNoCoc = false, redNoVelocity = false;
	QString dumpDir;
	int dumpFrame = 2;

	// the cell
	bool interior = false, haveCellDnam = false;
	float cellDnam[4] = { 0, 0, 0, 0 };

	// the path
	int pathFrame = -1;
	float pathFps = 0.0f;
	int autoFrame = 0;
	QElapsedTimer clock;

	// the motion vectors' previous frame
	bool haveLast = false;
	int lastFrame = 0, lastW = 0, lastH = 0;
	Dmat lastVP = Dmat::identity(), lastPrevVP = Dmat::identity();

	QString lastDof = QStringLiteral( "off" ), lastMb = QStringLiteral( "off" );
	QString saidDof, saidMb;
};

bool readPin( const char * env, const char * key, bool & pinned )
{
	const QString pin = qEnvironmentVariable( env ).trimmed();
	if ( !pin.isEmpty() ) {
		pinned = true;
		return pin != QLatin1StringView( "0" ) && pin.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
	}
	if ( wwHarnessRun() ) {
		pinned = true;	// a harness measures what it pins: no pin, no pass, whatever was saved
		return false;
	}
	pinned = false;
	return QSettings().value( QLatin1StringView( key ), false ).toBool();	// new rows ship OFF
}

PostState & ps()
{
	static PostState s;
	if ( !s.init ) {
		s.init = true;
		s.dof = readPin( "WW_CELL_DOF", kDofKey, s.dofPinned );
		s.mb = readPin( "WW_CELL_MBLUR", kMblurKey, s.mbPinned );
		s.redNoCoc = qEnvironmentVariable( "WW_CELL_DOF_RED" ).contains( QLatin1StringView( "nococ" ) );
		s.redNoVelocity = qEnvironmentVariable( "WW_CELL_MBLUR_RED" ).contains( QLatin1StringView( "novelocity" ) );
		s.dumpDir = qEnvironmentVariable( "WW_CELL_POST_DUMP" ).trimmed();
		bool ok = false;
		const int df = qEnvironmentVariable( "WW_CELL_POST_DUMP_FRAME" ).toInt( &ok );
		if ( ok )
			s.dumpFrame = df;
	}
	return s;
}

QHash<Renderer *, PostGpu> & postGpus()
{
	static QHash<Renderer *, PostGpu> g;
	return g;
}

// telemetry: one line per change of verdict, never per frame (the frame number is left out of the comparison)
void noteDof( PostState & s, const QString & line )
{
	s.lastDof = line;
	if ( s.saidDof != line ) {
		s.saidDof = line;
		qInfo().noquote() << "cell dof:" << line;
	}
}

void noteMb( PostState & s, const QString & line, const QString & key )
{
	s.lastMb = line;
	if ( s.saidMb != key ) {
		s.saidMb = key;
		qInfo().noquote() << "cell mblur:" << line;
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

void freeGpu( Renderer * r, PostGpu & g )
{
	auto fn = r->fn;
	GLuint texs[6] = { g.cur, g.depth, g.blur[0], g.blur[1], g.mv, g.out };
	for ( GLuint t : texs )
		if ( t )
			fn->glDeleteTextures( 1, &t );
	g.cur = g.depth = g.blur[0] = g.blur[1] = g.mv = g.out = 0;
}

// a float32 dump: int32 w h channels, then rows bottom-up (GL order), channels interleaved (the TAA dump's format)
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

std::vector<float> readTex( Renderer * r, GLuint fbo, GLuint tex, bool depth, GLenum format, int ch, int w, int h )
{
	auto fn = r->fn;
	std::vector<float> v( size_t( w ) * size_t( h ) * size_t( ch ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	if ( !depth ) {
		fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	}
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	fn->glReadPixels( 0, 0, w, h, format, GL_FLOAT, v.data() );
	return v;
}

QString f9( double v )
{
	return QString::number( v, 'g', 9 );
}

QString v4( const FloatVector4 & v )
{
	return f9( v[0] ) + QLatin1Char( ' ' ) + f9( v[1] ) + QLatin1Char( ' ' ) + f9( v[2] ) + QLatin1Char( ' ' ) + f9( v[3] );
}

//! the depth of field's record this frame: the cell's XCIM indoors, the weather's keys outdoors
bool dofRecord( const PostState & s, float out[4], QString & src )
{
	if ( s.interior ) {
		if ( !s.haveCellDnam ) {
			src = QStringLiteral( "interior without an imagespace DNAM" );
			return false;
		}
		std::memcpy( out, s.cellDnam, sizeof( s.cellDnam ) );
		src = QStringLiteral( "interior XCIM" );
		return true;
	}
	float a[4], b[4], t = 0.0f;
	bool haveA = false, haveB = false;
	if ( !wwLookdevImageSpaceDof( a, b, t, haveA, haveB ) ) {
		src = QStringLiteral( "no weather loaded" );
		return false;
	}
	if ( !haveA && !haveB ) {
		src = QStringLiteral( "the weather's imagespaces have no DNAM" );
		return false;
	}
	if ( !haveA )
		std::memcpy( a, b, sizeof( a ) );
	if ( !haveB )
		std::memcpy( b, a, sizeof( b ) );
	for ( int i = 0; i < 3; i++ )
		out[i] = a[i] + ( b[i] - a[i] ) * t;
	out[3] = t < 0.5f ? a[3] : b[3];	// the flags are a bit field: the nearer key's, never a blend
	src = QStringLiteral( "weather keys t=%1" ).arg( f9( t ) );
	return true;
}

}	// namespace

bool wwCellDofOn()
{
	return ps().dof;
}

void wwCellDofSetOn( bool on )
{
	PostState & s = ps();
	s.dof = on;
	if ( !s.dofPinned )
		QSettings().setValue( QLatin1StringView( kDofKey ), on );
}

bool wwCellMotionBlurOn()
{
	return ps().mb;
}

void wwCellMotionBlurSetOn( bool on )
{
	PostState & s = ps();
	s.mb = on;
	s.haveLast = false;
	if ( !s.mbPinned )
		QSettings().setValue( QLatin1StringView( kMblurKey ), on );
}

void wwCellPostSetCell( const EsmWorld & world, bool interior )
{
	PostState & s = ps();
	s.interior = interior;
	s.haveCellDnam = false;
	for ( int i = 0; i < 4; i++ )
		s.cellDnam[i] = 0.0f;
	s.haveLast = false;
	const quint32 xcim = interior ? world.interior().imageSpace : 0U;
	ESMFile * esm = world.plugin();
	if ( !xcim || !esm )
		return;
	try {
		const ESMFile::ESMRecord * r = esm->findRecord( xcim );
		if ( !r || !( *r == "IMGS" ) )
			return;
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "DNAM" && f.size() >= 16 ) {
				std::memcpy( s.cellDnam, f.data(), 16 );
				s.haveCellDnam = true;
				break;
			}
		}
	} catch ( std::exception & ) {
		s.haveCellDnam = false;
	}
}

void wwCellPostSetPath( int frame, float fps )
{
	PostState & s = ps();
	s.pathFrame = frame;
	s.pathFps = fps;
	if ( frame < 0 )
		s.haveLast = false;
}

QString wwCellPostEcho()
{
	const PostState & s = ps();
	return QStringLiteral( "cell dof: %1 | cell mblur: %2" ).arg( s.lastDof, s.lastMb );
}

void wwCellPostApply( Scene * scene )
{
	PostState & s = ps();
	if ( !s.dof )
		s.lastDof = QStringLiteral( "off" );
	if ( !s.mb )
		s.lastMb = QStringLiteral( "off" );
	if ( ( !s.dof && !s.mb ) || !scene || !scene->renderer || scene->selecting )
		return;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	while ( fn->glGetError() != GL_NO_ERROR ) {
	}

	// this frame's index and time
	int frame = 0;
	double dt = 0.0;
	if ( s.pathFrame >= 0 ) {
		frame = s.pathFrame;
		dt = s.pathFps > 0.0f ? 1.0 / double( s.pathFps ) : 0.0;
	} else {
		frame = ++s.autoFrame;
		if ( s.clock.isValid() ) {
			dt = double( s.clock.nsecsElapsed() ) * 1e-9;
			s.clock.restart();
		} else {
			s.clock.start();
		}
	}

	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	const int W = vp[2], H = vp[3];
	if ( W < 2 || H < 2 )
		return;

	// the camera: the projection as uploaded (column-major), near and far from its depth row
	const float * P = &( r->globalUniforms->projectionMatrix[0][0] );
	const bool perspective = P[11] < -0.5f && std::fabs( P[15] ) < 1e-6f;
	float zNear = 0.0f, zFar = 0.0f;
	if ( perspective ) {
		zNear = P[14] / ( P[10] - 1.0f );
		zFar = P[14] / ( P[10] + 1.0f );
	}

	// the depth of field's verdict and constants (ImageSpaceEffectDepthOfField::UpdateParams)
	bool runDof = false;
	bool farOnly = false;
	int radius = 3;
	FloatVector4 dc0( 0.0f ), dc1( 0.0f ), dc2( 0.0f ), dc3( 1.0f );
	if ( s.dof ) {
		float rec[4] = { 0, 0, 0, 0 };
		QString src;
		if ( !perspective ) {
			noteDof( s, QStringLiteral( "refused (an orthographic view has no depth of field)" ) );
		} else if ( !dofRecord( s, rec, src ) ) {
			noteDof( s, QStringLiteral( "refused (%1)" ).arg( src ) );
		} else {
			const float strength = rec[0], d = rec[1], rg = rec[2];
			const float fl = rec[3];
			const quint32 flags = ( std::isfinite( fl ) && fl > 0.0f && fl < 4294967296.0f ) ? quint32( fl ) : 0U;
			const quint32 mode = flags & 3U;
			radius = int( flags >> 3 ) ? int( flags >> 3 ) : 3;
			radius = std::min( radius, 64 );
			farOnly = mode == 2U;
			dc0 = FloatVector4( d - ( d - rg ), ( rg + d ) - d, d, 0.0f );
			dc1 = FloatVector4( strength, mode > 1U ? 0.0f : 1.0f, ( flags & 1U ) ? 0.0f : 1.0f, ( flags & 4U ) ? 1.0f : 0.0f );
			dc2 = FloatVector4( -1e8f, zNear, zFar - zNear, zFar * zNear );
			const QString what = QStringLiteral( "strength %1 distance %2 range %3 flags %4 radius %5 %6 from %7%8" )
				.arg( f9( strength ), f9( d ), f9( rg ) ).arg( flags ).arg( radius )
				.arg( farOnly ? QStringLiteral( "far-only" ) : QStringLiteral( "near+far" ), src,
					s.redNoCoc ? QStringLiteral( " RED nococ" ) : QString() );
			if ( !( strength > 0.0f ) ) {
				noteDof( s, QStringLiteral( "refused (strength 0: %1)" ).arg( what ) );
			} else {
				runDof = true;
				noteDof( s, QStringLiteral( "on %1" ).arg( what ) );
			}
		}
	}

	// the motion vectors' frames: the next index continues the path, the same index repeats it
	const Dmat curVP = fromColumnMajor( P ) * fromTransform( scene->view );
	bool runMb = false;
	Dmat reproj = Dmat::identity();
	FloatVector4 mc0( 0.0f ), mc1( 0.0f );
	if ( s.mb ) {
		Dmat prevVP = curVP;
		bool have = false;
		if ( s.haveLast && s.lastW == W && s.lastH == H ) {
			if ( frame == s.lastFrame + 1 || s.pathFrame < 0 ) {
				prevVP = s.lastVP;
				have = true;
			} else if ( frame == s.lastFrame ) {
				prevVP = s.lastPrevVP;
				have = true;
			}
		}
		s.lastPrevVP = prevVP;
		s.lastVP = curVP;
		s.lastFrame = frame;
		s.lastW = W;
		s.lastH = H;
		s.haveLast = true;
		Dmat invCur;
		if ( !have ) {
			noteMb( s, QStringLiteral( "waiting (no previous frame)" ), QStringLiteral( "waiting" ) );
		} else if ( !( dt > 0.0 ) ) {
			noteMb( s, QStringLiteral( "refused (no frame time)" ), QStringLiteral( "nodt" ) );
		} else if ( !invert( curVP, invCur ) ) {
			noteMb( s, QStringLiteral( "refused (the camera matrix does not invert)" ), QStringLiteral( "noinv" ) );
		} else {
			reproj = prevVP * invCur;
			runMb = true;
			// c0 = (scale x 0.001 x (1 / dt), max blur, fThreshold, 0); c1 = (1, 1, (W - 1) / W, (H - 1) / H)
			mc0 = FloatVector4( kMbScale * 0.001f * ( 1.0f / float( dt ) ), kMbMax, kMbThreshold, 0.0f );
			mc1 = FloatVector4( 1.0f, 1.0f, float( W - 1 ) / float( W ), float( H - 1 ) / float( H ) );
			const QString line = QStringLiteral( "on frame %1 dt %2 c0 %3%4" ).arg( frame ).arg( f9( dt ), v4( mc0 ),
				s.redNoVelocity ? QStringLiteral( " RED novelocity" ) : QString() );
			// the key: a path's fixed frame time, never an interactive window's measured one (no line per paint)
			noteMb( s, line, QStringLiteral( "on %1 %2" ).arg( s.pathFrame >= 0 ? f9( dt ) : QStringLiteral( "measured" ) )
				.arg( s.redNoVelocity ? 1 : 0 ) );
		}
	}
	if ( !runDof && !runMb )
		return;

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
		const QString why = QStringLiteral( "refused (the frame's depth is %1 bits, %2 stencil)" ).arg( depthBits ).arg( stencilBits );
		if ( runDof )
			noteDof( s, why );
		if ( runMb )
			noteMb( s, why, why );
		return;
	}

	PostGpu & g = postGpus()[r];
	if ( !g.inFbo || g.w != W || g.h != H || g.dsFormat != dsFormat ) {
		freeGpu( r, g );
		if ( !g.inFbo ) {
			fn->glGenFramebuffers( 1, &g.inFbo );
			fn->glGenFramebuffers( 1, &g.fbo );
			fn->glGenFramebuffers( 1, &g.mvFbo );
		}
		g.cur = makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, GL_LINEAR );			// t0
		g.depth = makeTex( r, dsFormat, dsFmt, dsType, W, H, GL_NEAREST );					// the DoF's t2
		g.blur[0] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );
		g.blur[1] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );			// the DoF's t1
		g.mv = makeTex( r, GL_RG32F, GL_RG, GL_FLOAT, W, H, GL_LINEAR );					// the blur's t1
		g.out = makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, GL_LINEAR );
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
	}

	// state kept for the restore
	GLint prevActive = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	GLboolean colorMask[4], depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	const bool wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );

	// 1. the frame (resolved when multisampled) into t0 and the depth
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, g.depthBits, GL_NEAREST );
	const GLenum blitErr = fn->glGetError();
	bool ok = blitErr == GL_NO_ERROR;

	fn->glDisable( GL_DEPTH_TEST );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_BLEND );
	fn->glDisable( GL_CULL_FACE );
	fn->glDisable( GL_STENCIL_TEST );
	fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = quad;
	const bool dump = !s.dumpDir.isEmpty() && frame == s.dumpFrame;
	const QString dd = s.dumpDir + QLatin1Char( '/' );
	if ( dump )
		QDir().mkpath( s.dumpDir );
	const GLenum one = GL_COLOR_ATTACHMENT0;
	auto target = [&]( GLuint tex ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		fn->glDrawBuffers( 1, &one );
	};
	auto bind = [&]( int unit, GLuint tex ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
		fn->glBindTexture( GL_TEXTURE_2D, tex );
	};
	if ( ok && dump ) {
		dumpFloat( dd + "cur.bin", W, H, 4, readTex( r, g.fbo, g.cur, false, GL_RGBA, 4, W, H ) );
		dumpFloat( dd + "depth.bin", W, H, 1, readTex( r, g.inFbo, 0, true, GL_DEPTH_COMPONENT, 1, W, H ) );
	}

	// 2. the depth of field: the blur (two separable passes), then the game's composite into out, out back to cur
	if ( ok && runDof ) {
		const float sigma = float( radius ) * 0.5f;
		for ( int pass = 0; pass < 2 && ok; pass++ ) {
			target( g.blur[pass] );
			auto prog = r->useProgram( "game_dof_blur.prog" );
			if ( !prog ) {
				ok = false;
				break;
			}
			bind( kUnitCur, pass == 0 ? g.cur : g.blur[0] );
			prog->uni1i( "srcTex", kUnitCur );
			prog->uni2f( "stepUv", pass == 0 ? 1.0f / float( W ) : 0.0f, pass == 0 ? 0.0f : 1.0f / float( H ) );
			prog->uni1i( "radius", radius );
			prog->uni1f( "sigma", sigma );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
		if ( ok ) {
			target( g.out );
			auto prog = r->useProgram( "game_dof.prog" );
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitCur, g.cur );
				bind( kUnitBlur, g.blur[1] );
				bind( kUnitDepth, g.depth );
				prog->uni1i( "t0", kUnitCur );
				prog->uni1i( "t1", kUnitBlur );
				prog->uni1i( "t2", kUnitDepth );
				prog->uni4f( "c0", dc0 );
				prog->uni4f( "c1", dc1 );
				prog->uni4f( "c2", dc2 );
				prog->uni4f( "c3", dc3 );
				prog->uni1i( "farOnly", farOnly ? 1 : 0 );
				prog->uni1i( "dofRed", s.redNoCoc ? 1 : 0 );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		if ( ok && dump ) {
			dumpFloat( dd + "dof_blur.bin", W, H, 4, readTex( r, g.fbo, g.blur[1], false, GL_RGBA, 4, W, H ) );
			dumpFloat( dd + "dof_out.bin", W, H, 4, readTex( r, g.fbo, g.out, false, GL_RGBA, 4, W, H ) );
		}
		if ( ok ) {	// the motion blur reads the depth of field's picture
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fbo );
			fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.out, 0 );
			fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
			fn->glDrawBuffers( 1, &one );
			fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		}
	}

	// 3. the motion blur: the camera's vectors (the temporal AA's vector pass), then the game's blur into out
	if ( ok && runMb ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.mvFbo );
		fn->glDrawBuffers( 1, &one );
		if ( s.redNoVelocity ) {
			const GLfloat zero[4] = { 0, 0, 0, 0 };
			fn->glClearBufferfv( GL_COLOR, 0, zero );
		} else {
			auto prog = r->useProgram( "game_taa_mv.prog" );
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitDepth, g.depth );
				prog->uni1i( "depthTex", kUnitDepth );
				prog->uni4m( "reproj", toMatrix4( reproj ) );
				prog->uni2f( "jitterNdc", 0.0f, 0.0f );
				prog->uni2f( "invSize", 1.0f / float( W ), 1.0f / float( H ) );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		if ( ok && dump ) {
			dumpFloat( dd + "mb_in.bin", W, H, 4, readTex( r, g.fbo, g.cur, false, GL_RGBA, 4, W, H ) );
			dumpFloat( dd + "mv.bin", W, H, 2, readTex( r, g.mvFbo, g.mv, false, GL_RG, 2, W, H ) );
		}
		if ( ok ) {
			target( g.out );
			auto prog = r->useProgram( "game_mblur.prog" );
			if ( !prog ) {
				ok = false;
			} else {
				bind( kUnitCur, g.cur );
				bind( kUnitMv, g.mv );
				prog->uni1i( "t0", kUnitCur );
				prog->uni1i( "t1", kUnitMv );
				prog->uni4f( "c0", mc0 );
				prog->uni4f( "c1", mc1 );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		if ( ok && dump )
			dumpFloat( dd + "mb_out.bin", W, H, 4, readTex( r, g.fbo, g.out, false, GL_RGBA, 4, W, H ) );
	}

	if ( ok && dump ) {
		QFile f( dd + "post.txt" );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream t( &f );
			t << "frame " << frame << "\nW " << W << "\nH " << H << "\ndt " << f9( dt )
			  << "\ndof " << ( runDof ? 1 : 0 ) << "\nfarOnly " << ( farOnly ? 1 : 0 ) << "\nradius " << radius
			  << "\ndc0 " << v4( dc0 ) << "\ndc1 " << v4( dc1 ) << "\ndc2 " << v4( dc2 ) << "\ndc3 " << v4( dc3 )
			  << "\nmb " << ( runMb ? 1 : 0 ) << "\nmc0 " << v4( mc0 ) << "\nmc1 " << v4( mc1 )
			  << "\nreproj";
			const Matrix4 rp = toMatrix4( reproj );
			for ( int i = 0; i < 16; i++ )
				t << ' ' << f9( rp.data()[i] );
			t << "\nred " << ( s.redNoCoc ? "nococ" : s.redNoVelocity ? "novelocity" : "none" ) << '\n';
		}
		qInfo().noquote() << "cell post: dumped frame" << frame << "to" << s.dumpDir;
	}

	// 4. the picture back into the frame (every sample of a multisampled frame the same value)
	if ( ok ) {
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
		auto prog = r->useProgram( "game_taa_copy.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			bind( kUnitCur, runMb ? g.out : g.cur );	// after the DoF alone its picture is already in cur
			prog->uni1i( "srcTex", kUnitCur );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
	}

	for ( int u : { kUnitCur, kUnitBlur, kUnitDepth, kUnitMv } ) {
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
		const QString why = blitErr != GL_NO_ERROR ? QStringLiteral( "refused (the frame's blit failed, GL error %1)" ).arg( blitErr )
			: QStringLiteral( "refused (a game_dof / game_mblur program is missing)" );
		if ( runDof )
			noteDof( s, why );
		if ( runMb )
			noteMb( s, why, why );
	}
}
