#include "gametaa.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gl/celluniform.h"	// lane CELLUNI4: the tone map's known state
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "gl/taainputs.h"	// lane TAA3: Ultra's velocity and mask
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
const char * const kTaa2Key = "Settings/Render/Modern temporal AA";	// lane TAA2: 0 Off .. 4 Ultra

// lane TAA2: RCAS strength in stops per tier (0 Off .. 4 Ultra; High and Ultra sharpen), tuned by notes/taa2/sim
constexpr float kRcasStops[5] = { 0.0f, 0.0f, 0.0f, 0.5f, 0.3f };
constexpr GLenum kTimestamp = 0x8E28;	// GL_TIMESTAMP
constexpr int kTaa2Crop = 256, kTaa2CropFrames = 32;	// WW_TAA2_DUMP: the centre crop of each of 32 frames

// the game's settings, read from the exe (fTAASharpen:Display, fTAALowFreq, fTAAHighFreq, fTAAPostSharpen,
// fTAAPostOverlay); the 128-pixel motion scale, 20, 0.01, 1.001 are the resolve's own literals (game_taa.frag)
constexpr float kSharpen = 1.0f;
constexpr float kLowFreq = 0.5f;
constexpr float kHighFreq = 0.8f;
constexpr float kPostSharpen = 0.21f;
constexpr float kPostOverlay = 0.21f;

constexpr int kUnitCur = 8, kUnitHist = 9, kUnitMv = 10, kUnitDepth = 11;
constexpr int kUnitLock = 12, kUnitVel = 13, kUnitMask = 14;	// lane TAA3
const char * const kTaa2HdrKey = "Settings/Render/Modern temporal AA before tone map";	// lane TAA3

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

/* the engine's Halton step, step for step in float: 1/base, then while index > 0: r += fmodf(index, base) * f,
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
	bool mvOk = false;	// lane VRS1: the last frame's vectors were drawn (the next rate image reads them)
	GLuint hist2[2] = { 0, 0 };	// lane TAA2: the Modern temporal AA's full-colour history (rgb, a = 1 - depth)
	GLuint q[4] = { 0, 0, 0, 0 };	// lane TAA2: two timestamp pairs, read a frame late so nothing waits
	bool qPending[2] = { false, false };
	int qCur = 0;
	GLuint lock[2] = { 0, 0 };	// lane TAA3: the thin-feature lock (r flip rate, g sign), ping-pong with hist2
	GLenum curFormat = 0;	// lane TAA3: t0's format (RGBA8, or the linear frame's before the tone map)
	GLenum outFormat = 0;
	GLenum histFormat = 0, histFilter = 0;	// lane MOTION1-FMT2: the game resolve's history pair (RGBA16F, or RGBA8 pinned)
};

struct TaaState
{
	bool init = false;
	quint64 liveGen = 0;	// lane TESTDRIVER1: the switch generation last read
	bool on = false, pinned = false;
	bool redNoClamp = false, redWrongJitter = false;
	bool hist8 = false, redHist8Nearest = false;	// lane MOTION1-FMT2: the game's RGBA8 history (test pin) and its red
	QString dumpDir;
	int dumpFrame = 8;
	// lane TAA2: the Modern temporal AA tier (0 Off = the game's resolve), its reds and its dump
	int taa2 = 0;
	bool taa2Pinned = false, taa2Fresh = true;
	int taa2Red = 0;	// bits: 1 noclip, 2 nobicubic, 4 wrongjitter, 8 nolock, 16 noinputs (lane TAA3), 32 oldmotion, 64 oldfilter (TAA4)
	bool taa2Hdr = false, taa2HdrPinned = false;	// lane TAA3: resolve the linear frame before its tone map
	bool hdrTook = false, inputsRan = false;	// lane TAA3: this frame's linear resolve ran / ran the inputs pass
	QString taa2Dump;
	double taa2Ms = 0.0;
	int taa2Timed = 0;

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
	// lane TESTDRIVER1: a live set of these switches re-reads them (pins and or-ed reds back to their defaults first)
	if ( !s.init || wwSwitchMovedFor( s.liveGen, { "WW_TAA" } ) ) {
		s.init = true;
		s.liveGen = wwSwitchGen();
		{
			const TaaState d {};
			s.dumpFrame = d.dumpFrame;
		}
		const QString pin = wwEnvStr( "WW_TAA" ).trimmed();
		if ( !pin.isEmpty() ) {
			s.on = pin != QLatin1StringView( "0" ) && pin.compare( QLatin1StringView( "off" ), Qt::CaseInsensitive ) != 0;
			s.pinned = true;
		} else if ( wwHarnessRun() ) {
			s.on = false;	// a harness measures what it pins: no pin, no TAA, whatever was saved
			s.pinned = true;
		} else {
			s.on = QSettings().value( QLatin1StringView( kTaaKey ), false ).toBool();
		}
		const QString red = wwEnvStr( "WW_TAA_RED" ).trimmed();
		s.redNoClamp = red.contains( QLatin1StringView( "noclamp" ) );
		s.redWrongJitter = red.contains( QLatin1StringView( "wrongjitter" ) );
		// lane MOTION1-FMT2: WW_TAA_HIST=8|rgba8 keeps the history pair in RGBA8 as the game does (AirOn.rdc); the red
		// hist8nearest reads it unfiltered. Not a row: the default stays RGBA16F until a live side-by-side with the game
		const QString hist = wwEnvStr( "WW_TAA_HIST" ).trimmed();
		s.redHist8Nearest = red.contains( QLatin1StringView( "hist8nearest" ) );
		s.hist8 = s.redHist8Nearest || hist == QLatin1StringView( "8" ) || hist.compare( QLatin1StringView( "rgba8" ), Qt::CaseInsensitive ) == 0;
		s.dumpDir = wwEnvStr( "WW_TAA_DUMP" ).trimmed();
		bool ok = false;
		const int df = wwEnvStr( "WW_TAA_DUMP_FRAME" ).toInt( &ok );
		if ( ok )
			s.dumpFrame = df;
		// lane TAA2: WW_TAA2=0..4 or off|low|medium|high|ultra pins the tier; a harness without it runs the game's
		const QString pin2 = wwEnvStr( "WW_TAA2" ).trimmed().toLower();
		const int was2 = s.taa2;
		s.taa2Pinned = !pin2.isEmpty() || wwHarnessRun();
		if ( !pin2.isEmpty() ) {
			static const QStringList names = { "off", "low", "medium", "high", "ultra" };
			bool num = false;
			const int k = pin2.toInt( &num );
			s.taa2 = num ? std::clamp( k, 0, 4 ) : std::max( int( names.indexOf( pin2 ) ), 0 );
		} else if ( wwHarnessRun() ) {
			s.taa2 = 0;
		} else {
			s.taa2 = std::clamp( QSettings().value( QLatin1StringView( kTaa2Key ), 0 ).toInt(), 0, 4 );
		}
		const QString red2 = wwEnvStr( "WW_TAA2_RED" ).trimmed();
		s.taa2Red = ( red2.contains( QLatin1StringView( "noclip" ) ) ? 1 : 0 ) | ( red2.contains( QLatin1StringView( "nobicubic" ) ) ? 2 : 0 )
			| ( red2.contains( QLatin1StringView( "wrongjitter" ) ) ? 4 : 0 ) | ( red2.contains( QLatin1StringView( "nolock" ) ) ? 8 : 0 )
			| ( red2.contains( QLatin1StringView( "noinputs" ) ) ? 16 : 0 )
			| ( red2.contains( QLatin1StringView( "oldmotion" ) ) ? 32 : 0 ) | ( red2.contains( QLatin1StringView( "oldfilter" ) ) ? 64 : 0 );
		s.taa2Dump = wwEnvStr( "WW_TAA2_DUMP" ).trimmed();
		// lane TAA3: WW_TAA2_HDR=0|1 pins the before-tone-map resolve; a harness without it resolves the toned frame
		const QString pinHdr = wwEnvStr( "WW_TAA2_HDR" ).trimmed();
		const bool wasHdr = s.taa2Hdr;
		s.taa2HdrPinned = !pinHdr.isEmpty() || wwHarnessRun();
		if ( !pinHdr.isEmpty() )
			s.taa2Hdr = pinHdr == QLatin1StringView( "1" ) || pinHdr.compare( QLatin1StringView( "on" ), Qt::CaseInsensitive ) == 0;
		else
			s.taa2Hdr = !wwHarnessRun() && QSettings().value( QLatin1StringView( kTaa2HdrKey ), false ).toBool();
		if ( s.taa2 != was2 || s.taa2Hdr != wasHdr )
			s.taa2Fresh = true;
	}
	return s;
}

// lane TAA2: one GPU time pair; the slot written two frames ago is read without waiting, the older one waits
void taa2TimeRead( NifSkopeOpenGLContext::GLFunctions * fn, TaaGpu & g, TaaState & s, int slot, bool wait )
{
	if ( !g.qPending[slot] )
		return;
	if ( !wait ) {
		GLuint avail = 0;
		fn->glGetQueryObjectuiv( g.q[slot * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &avail );
		if ( !avail )
			return;
	}
	GLuint64 a = 0, b = 0;
	fn->glGetQueryObjectui64v( g.q[slot * 2], GL_QUERY_RESULT, &a );
	fn->glGetQueryObjectui64v( g.q[slot * 2 + 1], GL_QUERY_RESULT, &b );
	s.taa2Ms = b > a ? double( b - a ) * 1e-6 : 0.0;
	g.qPending[slot] = false;
	if ( ++s.taa2Timed % 60 == 1 )
		qInfo().noquote() << QStringLiteral( "modern temporal aa: tier %1 gpu %2 ms" ).arg( s.taa2 ).arg( s.taa2Ms, 0, 'f', 4 );
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
	GLuint texs[10] = { g.cur, g.depth, g.mv, g.out, g.hist[0], g.hist[1], g.hist2[0], g.hist2[1], g.lock[0], g.lock[1] };
	for ( GLuint t : texs )
		if ( t )
			fn->glDeleteTextures( 1, &t );
	g.cur = g.depth = g.mv = g.out = g.hist[0] = g.hist[1] = g.hist2[0] = g.hist2[1] = g.lock[0] = g.lock[1] = 0;
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

std::vector<float> readFloat( Renderer * r, GLuint fbo, GLenum attachment, GLenum format, int ch, int w, int h, int x = 0, int y = 0 )
{
	auto fn = r->fn;
	std::vector<float> v( size_t( w ) * size_t( h ) * size_t( ch ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo );
	if ( attachment != GL_DEPTH_ATTACHMENT )
		fn->glReadBuffer( attachment );
	fn->glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	fn->glReadPixels( x, y, w, h, format, GL_FLOAT, v.data() );
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
	const TaaState & s = ts();
	return s.on || s.taa2 > 0;	// lane TAA2: either resolve jitters the frame
}

bool wwGameTaaRowOn()
{
	return ts().on;
}

int wwTaa2Tier()
{
	return ts().taa2;
}

bool wwTaa2Pinned()
{
	return ts().taa2Pinned;
}

void wwTaa2SetTier( int tier )
{
	TaaState & s = ts();
	s.taa2 = std::clamp( tier, 0, 4 );
	s.taa2Fresh = true;
	if ( !s.taa2Pinned )
		QSettings().setValue( QLatin1StringView( kTaa2Key ), s.taa2 );
}

bool wwTaa2HdrOn()
{
	return ts().taa2Hdr;
}

void wwTaa2SetHdrOn( bool on )
{
	TaaState & s = ts();
	if ( s.taa2Hdr != on )
		s.taa2Fresh = true;	// the history's colour space changes
	s.taa2Hdr = on;
	if ( !s.taa2HdrPinned )
		QSettings().setValue( QLatin1StringView( kTaa2HdrKey ), on );
}

bool wwGameTaaInputsRan()
{
	TaaState & s = ts();
	const bool ran = s.inputsRan;
	s.inputsRan = false;
	return ran;
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
	if ( !s.armed || !( s.on || s.taa2 > 0 ) || !scene || !scene->renderer || scene->selecting )
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
	// the engine's temporal-data update: iCurrentFrame = (iCurrentFrame + 1) & 7, n = iCurrentFrame + 1;
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

bool wwGameTaaJitter( float & offX, float & offY )
{
	const TaaState & s = ts();
	if ( !s.jittered )
		return false;
	offX = s.offX;
	offY = s.offY;
	return true;
}

bool wwGameTaaFrame( int & frame )
{
	const TaaState & s = ts();
	if ( !s.jittered )
		return false;
	frame = s.frame;
	return true;
}

bool wwGameTaaWantsSettle()
{
	TaaState & s = ts();
	if ( !( s.on || s.taa2 > 0 ) || s.fixedFrame >= 0 || wwHarnessRun() || s.settle <= 0 )
		return false;
	s.settle--;
	return true;
}

unsigned wwGameTaaMotionTex( Scene * scene, int & w, int & h )
{
	if ( !scene || !scene->renderer )
		return 0;
	auto it = taaGpus().find( scene->renderer );
	if ( it == taaGpus().end() || !it->mvOk || !it->mv )
		return 0;
	w = it->w;
	h = it->h;
	return it->mv;
}

QString wwGameTaaEcho()
{
	return ts().last;
}

namespace {

bool taaResolveImpl( Scene * scene, GLuint linearFbo, GLenum linearFormat, float expose );

}	// namespace

void wwGameTaaResolve( Scene * scene )
{
	TaaState & s = ts();
	if ( s.hdrTook ) {	// lane TAA3: resolved before the tone map this frame; the vectors stay this frame's
		s.hdrTook = false;
		s.jittered = false;
		return;
	}
	if ( scene && scene->renderer ) {	// lane VRS1: the vectors count only for the frame that drew them
		auto it = taaGpus().find( scene->renderer );
		if ( it != taaGpus().end() )
			it->mvOk = false;
	}
	if ( !s.jittered ) {
		const bool active = s.on || s.taa2 > 0;
		if ( active && scene && !scene->selecting )
			note( s, QStringLiteral( "refused (no jittered projection this frame)" ) );
		else if ( !active )
			s.last = QStringLiteral( "off" );
		return;
	}
	s.jittered = false;
	taaResolveImpl( scene, 0, 0, 1.0f );
}

void wwGameTaaResolveLinear( Scene * scene, unsigned fbo, unsigned format, float expose )
{
	TaaState & s = ts();
	s.hdrTook = false;
	s.inputsRan = false;
	if ( !s.jittered || !s.taa2Hdr || s.taa2 <= 0 || !fbo || !scene || !scene->renderer || scene->selecting )
		return;
	if ( s.taa2 >= 4 && !( s.taa2Red & 16 ) ) {	// the inputs pass runs here, before the frame's own call would
		wwTaaInputsRequest();
		wwTaaInputsPass( scene );
		s.inputsRan = true;
	}
	auto it = taaGpus().find( scene->renderer );
	if ( it != taaGpus().end() )
		it->mvOk = false;
	s.hdrTook = taaResolveImpl( scene, GLuint( fbo ), GLenum( format ), expose > 0.0f ? expose : 1.0f );
}

namespace {

bool taaResolveImpl( Scene * scene, GLuint linearFbo, GLenum linearFormat, float expose )
{
	TaaState & s = ts();
	if ( !scene || !scene->renderer || scene->selecting )
		return false;
	const bool linear = linearFbo != 0;
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	// lane CELLUNI4: before the tone map the state is the one cellhdr's End read (null: asked, as before)
	const WwCellUni::KnownGl * kn = linear ? WwCellUni::known( fn ) : nullptr;
	static WwCellUni::DepthCfg depthCfg;	// the frame's depth format, asked once per framebuffer and size

	GLint prevDraw = 0, prevRead = 0, vp[4] = { 0, 0, 1, 1 };
	if ( kn ) {
		prevDraw = kn->draw;
		prevRead = kn->read;
		std::memcpy( vp, kn->vp, sizeof vp );
	} else {
		fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
		fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
		fn->glGetIntegerv( GL_VIEWPORT, vp );
	}
	const int W = vp[2], H = vp[3];
	const bool cfgHit = kn && depthCfg.hit( r, prevDraw, W, H );
	const bool checkErr = !cfgHit || !depthCfg.trusted;	// errors read until a frame on this framebuffer raised none
	if ( checkErr ) {
		while ( fn->glGetError() != GL_NO_ERROR ) {
		}
	}
	if ( W != s.W || H != s.H ) {
		note( s, QStringLiteral( "refused (the viewport changed inside the frame)" ) );
		return false;
	}

	// the frame's depth format: the depth goes over by a blit, which needs the same format both sides
	GLint depthBits = 0, stencilBits = 0, compType = 0;
	if ( cfgHit ) {
		depthBits = depthCfg.depthBits;
		stencilBits = depthCfg.stencilBits;
		compType = depthCfg.compType;
	} else {
		const GLenum att = prevDraw ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthBits );
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, prevDraw ? GL_STENCIL_ATTACHMENT : GL_STENCIL,
			GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits );
		fn->glGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType );
		if ( kn )
			depthCfg.keep( r, prevDraw, W, H, depthBits, stencilBits, compType );
	}
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
		return false;
	}

	TaaGpu & g = taaGpus()[r];
	// lane TAA3: before the tone map t0 takes the linear frame's own format (a multisample blit needs it) and t_out half floats
	const GLenum curFormat = linear ? linearFormat : GLenum( GL_RGBA8 ), outFormat = linear ? GLenum( GL_RGBA16F ) : GLenum( GL_RGBA8 );
	const GLenum histFormat = s.hist8 ? GLenum( GL_RGBA8 ) : GLenum( GL_RGBA16F );
	const GLenum histFilter = s.redHist8Nearest ? GLenum( GL_NEAREST ) : GLenum( GL_LINEAR );
	const bool resized = !g.inFbo || g.w != W || g.h != H || g.dsFormat != dsFormat || g.curFormat != curFormat || g.outFormat != outFormat
		|| g.histFormat != histFormat || g.histFilter != histFilter;
	if ( resized ) {
		freeGpu( r, g );
		if ( !g.inFbo ) {
			fn->glGenFramebuffers( 1, &g.inFbo );
			fn->glGenFramebuffers( 1, &g.mvFbo );
			fn->glGenFramebuffers( 1, &g.outFbo );
		}
		g.cur = linear ? makeTex( r, curFormat, curFormat == GL_R11F_G11F_B10F ? GL_RGB : GL_RGBA, GL_FLOAT, W, H, GL_NEAREST )
			: makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, GL_NEAREST );		// t0 NEAREST
		g.depth = makeTex( r, dsFormat, dsFmt, dsType, W, H, GL_NEAREST );				// t3 NEAREST
		g.mv = makeTex( r, GL_RG32F, GL_RG, GL_FLOAT, W, H, GL_NEAREST );				// t2 NEAREST
		g.out = linear ? makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, GL_NEAREST )
			: makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, GL_NEAREST );
		g.curFormat = curFormat;
		g.outFormat = outFormat;
		s.taa2Fresh = true;	// lane TAA3: the Modern history went with the old targets
		for ( GLuint & t : g.hist )	// t1 BILERP
			t = s.hist8 ? makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, W, H, histFilter )
				: makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, histFilter );
		g.histFormat = histFormat;
		g.histFilter = histFilter;
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
	GLboolean colorMask[4], depthMask = GL_TRUE;
	bool wasDepth = false, wasBlend = false, wasCull = false, wasStencil = false, wasScissor = false;
	if ( kn ) {	// lane CELLUNI4
		prevActive = kn->active;
		std::memcpy( colorMask, kn->colorMask, sizeof colorMask );
		depthMask = kn->depthMask;
		wasDepth = kn->depth, wasBlend = kn->blend, wasCull = kn->cull, wasStencil = kn->stencil, wasScissor = kn->scissor;
	} else {
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
		wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
		wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	}

	// 1. the frame (resolved when multisampled) into t0 and t3 (lane TAA3: the colour from the linear frame when given)
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, linear ? linearFbo : GLuint( prevDraw ) );
	if ( linear )
		fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
	// lane MSAABLACK1: a pass that set the view's read buffer to GL_NONE makes this colour blit a silent no-op
	// (no GL error, t0 stays zero, the frame saves black): give the frame its colour buffer back first
	if ( !kn ) {	// lane CELLUNI4: before the tone map the read buffer was set just above
		WW_SWITCH_STATIC( bool, msaaBlackRed, wwEnvInt( "WW_MSAABLACK1_RED" ) == 1 );
		GLint rb = 0;
		fn->glGetIntegerv( GL_READ_BUFFER, &rb );
		if ( rb == GL_NONE ) {
			static bool said = false;
			if ( !said ) {
				said = true;
				qInfo().noquote() << QStringLiteral( "temporal aa: the frame's read buffer was GL_NONE (fbo %1); %2" ).arg( prevDraw )
					.arg( msaaBlackRed ? QStringLiteral( "left (WW_MSAABLACK1_RED)" ) : QStringLiteral( "set back to its colour buffer" ) );
			}
			if ( !msaaBlackRed )
				fn->glReadBuffer( prevDraw ? GL_COLOR_ATTACHMENT0 : GL_BACK );
		}
	}
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.inFbo );
	fn->glDisable( GL_SCISSOR_TEST );
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
	if ( linear )
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevDraw ) );	// the depth went back to the frame (cellhdr)
	fn->glBlitFramebuffer( 0, 0, W, H, 0, 0, W, H, g.depthBits, GL_NEAREST );
	const GLenum blitErr = checkErr ? fn->glGetError() : GLenum( GL_NO_ERROR );
	if ( kn && blitErr != GL_NO_ERROR )
		depthCfg.forget();	// lane CELLUNI4: asked again, and errors read, next frame
	else if ( kn && checkErr && depthCfg.hit( r, prevDraw, W, H ) )
		depthCfg.trusted = true;

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
			g.mvOk = true;
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
	const bool modern = s.taa2 > 0;	// lane TAA2: the Modern temporal AA replaces steps 3 and 4
	if ( !modern )
		s.taa2Fresh = true;	// its history skipped this frame
	if ( ok && !modern ) {
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.outFbo );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.hist[wr], 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.out, 0 );
		const GLenum two[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		fn->glDrawBuffers( 2, two );
		ok = resolvePass( 0 );
	}

	// the dump: every input and output of this frame's resolve, before the picture goes back
	if ( ok && !modern && !s.dumpDir.isEmpty() && s.frame == s.dumpFrame ) {
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
			  << "\nhist " << ( s.hist8 ? ( s.redHist8Nearest ? "rgba8 nearest" : "rgba8" ) : "rgba16f" )
			  << "\nred " << ( s.redNoClamp ? "noclamp" : s.redWrongJitter ? "wrongjitter" : "none" ) << '\n';
		}
		note( s, QStringLiteral( "dumped frame %1 to %2" ).arg( s.frame ).arg( s.dumpDir ) );
	}

	// lane TAA2: 3b. the Modern temporal AA resolve (taa2.frag) into its own history and t_out, then 4b. the picture
	// back through RCAS (High, Ultra) or the plain copy; timed as one span
	const float jpx = s.offX * 0.5f * float( W ), jpy = s.offY * 0.5f * float( H );	// pixel p saw the scene at p - j
	bool noHist2 = false;
	int taa2Inputs = 0;	// lane TAA3: bits 1 velocity, 2 mask read this frame
	GLuint taa2Vel = 0, taa2Mask = 0;	// lane TAA4: the vectors / mask the resolve read (WW_TAA2_DUMP's mv_, msk_)
	if ( ok && modern ) {
		if ( !g.hist2[0] ) {
			for ( int i = 0; i < 2; i++ ) {
				g.hist2[i] = makeTex( r, GL_RGBA16F, GL_RGBA, GL_FLOAT, W, H, GL_LINEAR );
				g.lock[i] = makeTex( r, GL_RG16F, GL_RG, GL_FLOAT, W, H, GL_NEAREST );	// lane TAA3
			}
			s.taa2Fresh = true;
		}
		if ( !g.q[0] )
			fn->glGenQueries( 4, g.q );
		taa2TimeRead( fn, g, s, 1 - g.qCur, false );
		taa2TimeRead( fn, g, s, g.qCur, true );
		fn->glQueryCounter( g.q[g.qCur * 2], kTimestamp );
		noHist2 = reset || s.taa2Fresh;
		fn->glBindFramebuffer( GL_FRAMEBUFFER, g.outFbo );
		if ( noHist2 ) {	// lane TAA3: no lock survives a history reset (a repeated frame reads the other slot)
			for ( int i = 0; i < 2; i++ ) {
				fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.lock[i], 0 );
				const GLenum one = GL_COLOR_ATTACHMENT0;
				fn->glDrawBuffers( 1, &one );
				const GLfloat zero[4] = { 0, 0, 0, 0 };
				fn->glClearBufferfv( GL_COLOR, 0, zero );
			}
		}
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.hist2[wr], 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g.out, 0 );
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g.lock[wr], 0 );
		const GLenum three[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
		fn->glDrawBuffers( 3, three );
		// lane TAA3: Ultra reads TAAINPUTS1's velocity and mask when this frame drew them (asked one frame ahead)
		int inputs = 0;
		GLuint velTex = 0, maskTex = 0;
		if ( s.taa2 >= 4 && !( s.taa2Red & 16 ) ) {
			wwTaaInputsRequest();
			int vw = 0, vh = 0, mw = 0, mh = 0;
			velTex = wwTaaInputsVelocityTex( scene, vw, vh );
			maskTex = wwTaaInputsMaskTex( scene, mw, mh );
			inputs = ( velTex && vw == W && vh == H ? 1 : 0 ) | ( maskTex && mw == W && mh == H ? 2 : 0 );
		}
		auto prog = r->useProgram( "taa2.prog" );
		if ( !prog ) {
			ok = false;
		} else {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitCur ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.cur );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitHist ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.hist2[rd] );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitMv ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.mv );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitDepth ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.depth );
			prog->uni1i( "curTex", kUnitCur );
			prog->uni1i( "histTex", kUnitHist );
			prog->uni1i( "mvTex", kUnitMv );
			prog->uni1i( "depthTex", kUnitDepth );
			prog->uni2f( "invSize", 1.0f / float( W ), 1.0f / float( H ) );
			prog->uni2f( "jitterPx", jpx, jpy );
			prog->uni1i( "taa2Tier", s.taa2 );
			prog->uni1i( "taa2Red", s.taa2Red );
			prog->uni1i( "taa2NoHist", noHist2 ? 1 : 0 );
			prog->uni4m( "reproj", reproj4 );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitLock ) );
			fn->glBindTexture( GL_TEXTURE_2D, g.lock[rd] );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitVel ) );
			fn->glBindTexture( GL_TEXTURE_2D, ( inputs & 1 ) ? velTex : g.mv );
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitMask ) );
			fn->glBindTexture( GL_TEXTURE_2D, ( inputs & 2 ) ? maskTex : g.mv );
			prog->uni1i( "lockTex", kUnitLock );
			prog->uni1i( "velTex", kUnitVel );
			prog->uni1i( "maskTex", kUnitMask );
			prog->uni1i( "taa2Inputs", inputs );
			taa2Inputs = inputs;
			taa2Vel = ( inputs & 1 ) ? velTex : g.mv;
			taa2Mask = ( inputs & 2 ) ? maskTex : 0;
			prog->uni1i( "taa2Linear", linear ? 1 : 0 );
			prog->uni1f( "taa2Expose", linear ? expose : 1.0f );
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
			r->stopProgram();
		}
		fn->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, 0, 0 );
		if ( ok ) {
			// the picture back: into the frame, or (lane TAA3) into the linear frame with its alpha kept for the tone map
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, linear ? linearFbo : GLuint( prevDraw ) );
			if ( linear ) {
				const GLenum one = GL_COLOR_ATTACHMENT0;
				fn->glDrawBuffers( 1, &one );
				fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
			}
			const bool rcas = kRcasStops[s.taa2] > 0.0f;
			auto fp = r->useProgram( rcas ? "taa2_rcas.prog" : "game_taa_copy.prog" );
			if ( !fp ) {
				ok = false;
			} else {
				fn->glActiveTexture( GLenum( GL_TEXTURE0 + kUnitCur ) );
				fn->glBindTexture( GL_TEXTURE_2D, g.out );
				fp->uni1i( "srcTex", kUnitCur );
				if ( rcas ) {
					fp->uni1f( "rcasSharp", std::exp2( -kRcasStops[s.taa2] ) );
					fp->uni1i( "rcasLinear", linear ? 1 : 0 );
					fp->uni1f( "rcasExpose", linear ? expose : 1.0f );
				}
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				r->stopProgram();
			}
		}
		fn->glQueryCounter( g.q[g.qCur * 2 + 1], kTimestamp );
		g.qPending[g.qCur] = true;
		g.qCur = 1 - g.qCur;
		s.taa2Fresh = !ok;
	}

	// 4. the picture back into the frame (every sample of a multisampled frame the same value)
	if ( ok && !modern ) {
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

	// lane TAA2: WW_TAA2_DUMP=<dir>: the centre crop of the frame as it went out and as it came in, 32 frames from
	// WW_TAA_DUMP_FRAME, whichever resolve ran (the live shimmer / lag metrics: notes/taa2/sim/taa2_live_metrics.py)
	if ( ok && !s.taa2Dump.isEmpty() && s.frame >= s.dumpFrame && s.frame < s.dumpFrame + kTaa2CropFrames
		&& W >= kTaa2Crop && H >= kTaa2Crop ) {
		QDir().mkpath( s.taa2Dump );
		const QString d = s.taa2Dump + QLatin1Char( '/' );
		const int x0 = ( W - kTaa2Crop ) / 2, y0 = ( H - kTaa2Crop ) / 2;
		// lane TAA4: a linear frame is cropped in float (RGBA8 clamped it to 1)
		const GLuint crop = linear ? makeTex( r, GL_RGBA32F, GL_RGBA, GL_FLOAT, kTaa2Crop, kTaa2Crop, GL_NEAREST )
			: makeTex( r, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, kTaa2Crop, kTaa2Crop, GL_NEAREST );
		GLuint cropFbo = 0;
		fn->glGenFramebuffers( 1, &cropFbo );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, cropFbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, crop, 0 );
		const GLenum one = GL_COLOR_ATTACHMENT0;
		fn->glDrawBuffers( 1, &one );
		// lane TAA4: a linear resolve wrote into the linear frame; prevDraw still held another picture (detail 0.003)
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, linear ? linearFbo : GLuint( prevDraw ) );	// a multisampled frame resolves here
		if ( linear )
			fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
		fn->glBlitFramebuffer( x0, y0, x0 + kTaa2Crop, y0 + kTaa2Crop, 0, 0, kTaa2Crop, kTaa2Crop, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		const QString tag = QString::number( s.frame - s.dumpFrame ).rightJustified( 2, QLatin1Char( '0' ) );
		dumpFloat( d + "out_" + tag + ".bin", kTaa2Crop, kTaa2Crop, 4, readFloat( r, cropFbo, GL_COLOR_ATTACHMENT0, GL_RGBA, 4, kTaa2Crop, kTaa2Crop ) );
		// the frame as it came in (lane TAA4: a linear one too; the metrics put both through one display transform)
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.inFbo );
		fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
		fn->glBlitFramebuffer( x0, y0, x0 + kTaa2Crop, y0 + kTaa2Crop, 0, 0, kTaa2Crop, kTaa2Crop, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		dumpFloat( d + "cur_" + tag + ".bin", kTaa2Crop, kTaa2Crop, 4, readFloat( r, cropFbo, GL_COLOR_ATTACHMENT0, GL_RGBA, 4, kTaa2Crop, kTaa2Crop ) );
		// lane TAA4: the vectors the resolve read and the frame's depth, same crop (the offline replay reads them)
		if ( taa2Vel ) {
			GLuint velFbo = 0;
			fn->glGenFramebuffers( 1, &velFbo );
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, velFbo );
			fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taa2Vel, 0 );
			dumpFloat( d + "mv_" + tag + ".bin", kTaa2Crop, kTaa2Crop, 2,
				readFloat( r, velFbo, GL_COLOR_ATTACHMENT0, GL_RG, 2, kTaa2Crop, kTaa2Crop, x0, y0 ) );
			if ( taa2Mask ) {	// TAAINPUTS1's mask (r reactive, g composition, b own motion)
				fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taa2Mask, 0 );
				dumpFloat( d + "msk_" + tag + ".bin", kTaa2Crop, kTaa2Crop, 4,
					readFloat( r, velFbo, GL_COLOR_ATTACHMENT0, GL_RGBA, 4, kTaa2Crop, kTaa2Crop, x0, y0 ) );
			}
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
			fn->glDeleteFramebuffers( 1, &velFbo );
		}
		dumpFloat( d + "dep_" + tag + ".bin", kTaa2Crop, kTaa2Crop, 1,
			readFloat( r, g.inFbo, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT, 1, kTaa2Crop, kTaa2Crop, x0, y0 ) );
		fn->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		fn->glDeleteFramebuffers( 1, &cropFbo );
		fn->glDeleteTextures( 1, &crop );
		QFile f( d + "taa2.txt" );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append ) ) {
			QTextStream t( &f );
			t << "frame " << s.frame << " tier " << s.taa2 << " gameRow " << ( s.on ? 1 : 0 ) << " red " << s.taa2Red
			  << " jitterPx " << f9( jpx ) << ' ' << f9( jpy ) << " noHist " << ( noHist2 ? 1 : 0 ) << " reset " << ( reset ? 1 : 0 )
			  << " ms " << f9( s.taa2Ms ) << " W " << W << " H " << H << " hdr " << ( linear ? 1 : 0 ) << " inputs " << taa2Inputs
			  << " expose " << f9( linear ? expose : 1.0f ) << " reproj " << mat( reproj4 ) << '\n';
		}
	}

	for ( int u : { kUnitCur, kUnitHist, kUnitMv, kUnitDepth, kUnitLock, kUnitVel, kUnitMask } ) {
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
			: modern ? QStringLiteral( "refused (a taa2 program is missing)" ) : QStringLiteral( "refused (a game_taa program is missing)" ) );
		return false;
	}
	if ( modern ) {
		static const char * const tiers[5] = { "off", "low", "medium", "high", "ultra" };
		note( s, QStringLiteral( "frame %1 n=%2 off=(%3,%4)px modern %5 gpu %6 ms%7%8" ).arg( s.frame ).arg( s.n )
			.arg( f9( px ), f9( py ), QLatin1StringView( tiers[s.taa2] ), QString::number( s.taa2Ms, 'f', 4 ),
				noHist2 ? QStringLiteral( " history-reset" ) : QString(),
				s.taa2Red ? QStringLiteral( " RED %1" ).arg( s.taa2Red ) : QString() )
			+ ( linear ? QStringLiteral( " before-tone-map" ) : QString() ) + ( taa2Inputs ? QStringLiteral( " inputs %1" ).arg( taa2Inputs ) : QString() ) );
		return true;
	}
	note( s, QStringLiteral( "frame %1 n=%2 off=(%3,%4)px%5%6" ).arg( s.frame ).arg( s.n )
		.arg( f9( px ), f9( py ), reset ? QStringLiteral( " history-reset" ) : QString(),
			s.redNoClamp ? QStringLiteral( " RED noclamp" ) : s.redWrongJitter ? QStringLiteral( " RED wrongjitter" ) : QString() )
		+ ( s.redHist8Nearest ? QStringLiteral( " history rgba8 RED hist8nearest" ) : s.hist8 ? QStringLiteral( " history rgba8" ) : QString() ) );
	return true;
}

}	// namespace
