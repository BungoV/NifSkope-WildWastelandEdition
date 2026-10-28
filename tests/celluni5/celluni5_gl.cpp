/* tests/celluni5 (lane CELLUNI5): the CELLUNI4 leftovers on the kit (src/gl/celluniform.h), run through the real driver.
 * The tone map's frame end as cellhdr.cpp wwCellHdrEnd runs it after CELLUNI5: the FO4CS bloom and the fog bloom read
 * their GL errors (a drain before, reads after) only until one clean frame on their targets (WwCellUni::ErrTrust, lane
 * SYNC1's regime), and their Final/Done calls take the tone quads' active unit from the caller instead of asking
 * (WwCellUni::activeUnit). The view is remade at frame 10, and frames 10 and 11 raise a real GL error inside the bloom
 * pass, so it refuses both (the red, which reads every frame, refuses both too).
 * Green, the red (WW_CELL_UNI_RED: every read and ask) and "prev" (main before this lane) must give the same pixels and
 * state byte for byte, and green must ask the driver less. Doctored runs must differ: the trust kept after an error
 * (frame 11 then draws a bloom the red refuses), a remake that keeps the trust (frame 10 then draws), and a wrong known
 * unit (the restore lands on another unit; WW_SETUP_MEMO_CHECK=1 counts it).
 * The red build (-DCELLUNI5_RED) runs green as main did: it must fail the "asks less" checks.
 * Usage: celluni5_gl [frames]   exit 0 = all ok */
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>

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

// every driver query the modelled code makes, counted on the way to the driver (errors and active-unit asks apart)
struct CountFn
{
	QOpenGLExtraFunctions * f = nullptr;
	long long errors = 0, actives = 0, other = 0;
	void glGetIntegerv( GLenum p, GLint * v )
	{
		( p == GL_ACTIVE_TEXTURE ? actives : other )++;
		f->glGetIntegerv( p, v );
	}
	GLenum glGetError()
	{
		errors++;
		return f->glGetError();
	}
};

static QOpenGLExtraFunctions * gl = nullptr;

enum Mode { Green, Red, Prev, GreenCheck, DocKeepTrust, DocRemakeTrusted, DocWrongUnit, DocWrongUnitChecked };
static const char * const kModeName[] = { "green", "red", "prev", "green-check", "doc-trust-kept-after-error",
	"doc-remake-keeps-trust", "doc-wrong-known-unit", "doc-wrong-known-unit-checked" };

struct Cfg
{
	bool red = false, prev = false, check = false;
	bool docKeep = false, docRemake = false, docUnit = false;
};
static Cfg cfgOf( Mode m )
{
	Cfg c;
	c.red = m == Red;
	c.prev = m == Prev;
	c.check = m == GreenCheck || m == DocWrongUnitChecked;
	c.docKeep = m == DocKeepTrust;
	c.docRemake = m == DocRemakeTrusted;
	c.docUnit = m == DocWrongUnit || m == DocWrongUnitChecked;
#ifdef CELLUNI5_RED	// the red build: green as main ran it (every read and ask)
	if ( m == Green )
		c.prev = true;
#endif
	return c;
}

// ---- shaders: the tone quad reads the frame on unit 8 and the two blooms on units 6 (FO4CS) and 7 (fog) ----
static const char * kVert = R"(#version 460 core
const vec2 p[3] = vec2[3]( vec2( -1, -1 ), vec2( 3, -1 ), vec2( -1, 3 ) );
void main() { gl_Position = vec4( p[gl_VertexID], 0, 1 ); })";
static const char * kTone = R"(#version 460 core
layout( binding = 8 ) uniform sampler2D src;
layout( binding = 6 ) uniform sampler2D bloom;
layout( binding = 7 ) uniform sampler2D fog;
uniform int bloomOn, fogOn;
out vec4 o;
void main() {
	ivec2 q = ivec2( gl_FragCoord.xy );
	vec4 c = texelFetch( src, q, 0 );
	if ( bloomOn != 0 ) c += 0.25 * texelFetch( bloom, q, 0 );
	if ( fogOn != 0 ) c += 0.125 * texelFetch( fog, q, 0 );
	o = c;
})";

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

static GLuint tex2d( int w, int h )
{
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	gl->glBindTexture( GL_TEXTURE_2D, t );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}
static GLuint fboWith( GLuint t )
{
	GLuint f = 0;
	gl->glGenFramebuffers( 1, &f );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, f );
	gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0 );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	return f;
}

static const int W = 32, H = 16, kSrcUnit = 8, kBloomUnit = 6, kFogUnit = 7, kRemakeFrame = 10;

struct PassGpu	// one bloom pass's targets (BloomGpu / FbGpu) and its trust
{
	GLuint tex = 0, fbo = 0;
	int made = -1;	// the view generation the targets were made for
	WwCellUni::ErrTrust err;
};
struct Res
{
	GLuint src = 0, srcFbo = 0, out = 0, outFbo = 0, tone = 0, vao = 0;
	PassGpu bloom, fog;
};

static void clearTo( GLuint fbo, float r, float g, float b )
{
	gl->glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	gl->glViewport( 0, 0, W, H );
	gl->glClearColor( r, g, b, 1.0f );
	gl->glClear( GL_COLOR_BUFFER_BIT );
}

/* one bloom pass as wwCellBloomRun / wwCellFogBloomRun run it: the drain, the targets (remade on a new view), the
 * draw, the error reads, the trust. raise: a real GL error inside the pass. Returns ok (the tone map takes it). */
static bool pass( PassGpu & g, CountFn & cf, const Cfg & c, int gen, int frame, bool raise, float shade, int reads )
{
	const bool remake = g.made != gen;
	bool check = true;
	if ( !c.prev ) {
		check = c.docRemake ? ( c.red || !g.err.trusted ) : g.err.check( c.red, remake );
		if ( check )
			WwCellUni::drainErrors( &cf );
	} else {
		WwCellUni::drainErrors( &cf );
	}
	if ( remake ) {
		if ( !c.prev && !c.docRemake )
			g.err.forget();
		if ( g.tex ) {
			gl->glDeleteFramebuffers( 1, &g.fbo );
			gl->glDeleteTextures( 1, &g.tex );
		}
		g.tex = tex2d( W, H );
		g.fbo = fboWith( g.tex );
		g.made = gen;
	}
	clearTo( g.fbo, shade * float( 1 + frame % 3 ), 0.5f * shade, 0.25f );
	if ( raise )
		gl->glBindTexture( GL_TEXTURE_2D, 0x7FFFFFF0u );	// a name never made: GL_INVALID_OPERATION in a core context
	bool ok = true;
	for ( int i = 0; i < reads; i++ ) {
		const GLenum e = check ? cf.glGetError() : GLenum( GL_NO_ERROR );
		if ( ok && e != GL_NO_ERROR )
			ok = false;
	}
	if ( !c.prev ) {
		if ( c.docKeep ) {
			if ( check )
				g.err.trusted = true;	// doctored: trusted after any read, a failed one too
		} else {
			g.err.after( check, ok );
		}
	}
	return ok;
}

// the Final / Done pair as cellbloom.cpp and cellfogbloom.cpp run it: the unit, then back to the one before
static void bindOn( CountFn & cf, const Cfg & c, int unit, GLuint t, int known )
{
	const int k = c.prev || c.red ? -1 : known;
	const GLint prevActive = WwCellUni::activeUnit( &cf, k );
	gl->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	gl->glBindTexture( GL_TEXTURE_2D, t );
	gl->glActiveTexture( GLenum( prevActive ) );
}

struct FrameOut
{
	std::vector<unsigned char> px;
	std::vector<GLint> state;
	long long errors = 0, actives = 0;
	bool bloomOk = false, fogOk = false;
};

static std::vector<GLint> snapState()
{
	std::vector<GLint> s;
	GLint v = 0;
	gl->glGetIntegerv( GL_ACTIVE_TEXTURE, &v );
	s.push_back( v );
	for ( int u : { kBloomUnit, kFogUnit, kSrcUnit } ) {
		gl->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		gl->glGetIntegerv( GL_TEXTURE_BINDING_2D, &v );
		s.push_back( v ? 1 : 0 );
	}
	gl->glActiveTexture( GLenum( s[0] ) );
	return s;
}

static std::vector<FrameOut> run( Mode mode, int frames, long long * staleOut = nullptr )
{
	const Cfg c = cfgOf( mode );
	WwCellUni::State & st = WwCellUni::state();
	st.red = c.red;
	st.vpCheck = c.check;
	st.knownStale = 0;
	Res R;
	R.src = tex2d( W, H );
	R.srcFbo = fboWith( R.src );
	R.out = tex2d( W, H );
	R.outFbo = fboWith( R.out );
	R.tone = link( kTone );
	gl->glGenVertexArrays( 1, &R.vao );
	CountFn cf;
	cf.f = gl;
	std::vector<FrameOut> outs;
	for ( int f = 0; f < frames; f++ ) {
		const int gen = f < kRemakeFrame ? 0 : 1;
		const long long e0 = cf.errors, a0 = cf.actives;
		clearTo( R.srcFbo, 0.1f * float( f % 5 ), 0.2f, 0.3f );
		FrameOut fo;
		// the frame end: the passes, then the tone quads with kSrcUnit active (cellhdr.cpp)
		fo.bloomOk = pass( R.bloom, cf, c, gen, f, f == kRemakeFrame || f == kRemakeFrame + 1, 0.2f, 2 );
		fo.fogOk = pass( R.fog, cf, c, gen, f, false, 0.1f, 2 );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.outFbo );
		gl->glViewport( 0, 0, W, H );
		gl->glUseProgram( R.tone );
		gl->glActiveTexture( GLenum( GL_TEXTURE0 + kSrcUnit ) );
		gl->glBindTexture( GL_TEXTURE_2D, R.src );
		const int known = c.docUnit ? int( GL_TEXTURE0 + kFogUnit ) : int( GL_TEXTURE0 + kSrcUnit );
		bindOn( cf, c, kFogUnit, fo.fogOk ? R.fog.tex : 0, known );	// wwCellFogBloomFinal
		bindOn( cf, c, kBloomUnit, fo.bloomOk ? R.bloom.tex : 0, known );	// wwCellBloomFinal
		gl->glUniform1i( gl->glGetUniformLocation( R.tone, "bloomOn" ), fo.bloomOk ? 1 : 0 );
		gl->glUniform1i( gl->glGetUniformLocation( R.tone, "fogOn" ), fo.fogOk ? 1 : 0 );
		gl->glBindVertexArray( R.vao );
		gl->glDrawArrays( GL_TRIANGLES, 0, 3 );
		gl->glBindVertexArray( 0 );
		gl->glUseProgram( 0 );
		bindOn( cf, c, kBloomUnit, 0, known );	// wwCellBloomDone
		bindOn( cf, c, kFogUnit, 0, known );	// wwCellFogBloomDone (after the VRS build, which ends on kSrcUnit)
		gl->glBindTexture( GL_TEXTURE_2D, 0 );	// cellhdr: the source unbound on the active unit
		gl->glActiveTexture( GL_TEXTURE0 );	// back to prevActive (unit 0 at the frame end in this model)
		fo.state = snapState();
		fo.px.resize( size_t( W ) * H * 4 );
		gl->glBindFramebuffer( GL_READ_FRAMEBUFFER, R.outFbo );
		gl->glReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, fo.px.data() );
		gl->glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		fo.errors = cf.errors - e0;
		fo.actives = cf.actives - a0;
		outs.push_back( fo );
	}
	if ( staleOut )
		*staleOut = (long long)st.knownStale;
	for ( PassGpu * g : { &R.bloom, &R.fog } ) {
		gl->glDeleteFramebuffers( 1, &g->fbo );
		gl->glDeleteTextures( 1, &g->tex );
	}
	gl->glDeleteFramebuffers( 1, &R.srcFbo );
	gl->glDeleteFramebuffers( 1, &R.outFbo );
	gl->glDeleteTextures( 1, &R.src );
	gl->glDeleteTextures( 1, &R.out );
	gl->glDeleteProgram( R.tone );
	gl->glDeleteVertexArrays( 1, &R.vao );
	while ( gl->glGetError() != GL_NO_ERROR ) {
	}
	st.red = false;
	st.vpCheck = false;
	return outs;
}

static bool samePx( const std::vector<FrameOut> & a, const std::vector<FrameOut> & b, bool withState, int * firstDiff = nullptr )
{
	for ( size_t i = 0; i < a.size() && i < b.size(); i++ )
		if ( a[i].px != b[i].px || a[i].bloomOk != b[i].bloomOk || a[i].fogOk != b[i].fogOk || ( withState && a[i].state != b[i].state ) ) {
			if ( firstDiff )
				*firstDiff = int( i );
			return false;
		}
	return a.size() == b.size();
}
static long long sum( const std::vector<FrameOut> & v, long long FrameOut::*m, int from = 0 )
{
	long long s = 0;
	for ( size_t i = size_t( from ); i < v.size(); i++ )
		s += v[i].*m;
	return s;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const int frames = argc > 1 ? std::max( 16, std::atoi( argv[1] ) ) : 30;

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
		ErrTrust t;
		const bool cold = t.check( false, false );
		t.after( true, true );
		const bool warm = !t.check( false, false ) && t.check( true, false ) && t.check( false, true );
		t.after( false, false );
		const bool unread = t.trusted;	// a skipped frame keeps the trust
		t.after( true, false );
		const bool failed = !t.trusted;
		t.after( true, true );
		t.forget();
		check( cold && warm && unread && failed && !t.trusted,
			"error trust: read until a clean frame; red, a remake or a failed read reads again; forget() drops it" );
	}

	long long staleCheck = -1, staleDoc = -1;
	const std::vector<FrameOut> red = run( Red, frames );
	const std::vector<FrameOut> prev = run( Prev, frames );
	const std::vector<FrameOut> green = run( Green, frames );
	const std::vector<FrameOut> greenCheck = run( GreenCheck, frames, &staleCheck );
	const std::vector<FrameOut> docKeep = run( DocKeepTrust, frames );
	const std::vector<FrameOut> docRemake = run( DocRemakeTrusted, frames );
	const std::vector<FrameOut> docUnit = run( DocWrongUnit, frames );
	const std::vector<FrameOut> docUnitChecked = run( DocWrongUnitChecked, frames, &staleDoc );

	check( !red[kRemakeFrame].bloomOk && !red[kRemakeFrame + 1].bloomOk && red[kRemakeFrame + 2].bloomOk && red[0].fogOk,
		"red: the bloom refuses the two frames that raise a GL error, the others draw" );
	check( samePx( green, red, true ), "green == red: pixels, refusals and state, every frame" );
	check( samePx( green, prev, true ), "green == prev (main): pixels, refusals and state, every frame" );
	check( samePx( greenCheck, red, true ) && staleCheck == 0, "green with WW_SETUP_MEMO_CHECK=1 == red, no stale known unit" );

	const long long eR = sum( red, &FrameOut::errors ), eP = sum( prev, &FrameOut::errors ), eG = sum( green, &FrameOut::errors );
	const long long eGTail = sum( green, &FrameOut::errors, kRemakeFrame + 3 );
	const long long aR = sum( red, &FrameOut::actives ), aP = sum( prev, &FrameOut::actives ), aG = sum( green, &FrameOut::actives );
	std::printf( "error reads: red %lld prev %lld green %lld (green from frame %d on: %lld); active asks: red %lld prev %lld green %lld\n",
		eR, eP, eG, kRemakeFrame + 3, eGTail, aR, aP, aG );
	check( eG < eP && eG < eR, "green reads fewer GL errors than red and main" );
	check( eGTail == 0, "green: no error read once both passes trust their targets" );
	check( green[kRemakeFrame].errors > 0 && green[kRemakeFrame + 1].errors > 0, "green: the remake frame and the frame after its error read again" );
	check( aG == 0 && aR == 4LL * frames && aP == 4LL * frames, "green: no active-unit ask (red and main: 4 a frame)" );

	int d = -1;
	const bool keepDiffers = !samePx( docKeep, red, false, &d );
	std::printf( "doc trust kept after an error: first difference at frame %d\n", d );
	check( keepDiffers && d == kRemakeFrame + 1, "doctored: the trust kept after an error draws the bloom the red refuses" );
	d = -1;
	const bool remakeDiffers = !samePx( docRemake, red, false, &d );
	std::printf( "doc remake keeps the trust: first difference at frame %d\n", d );
	check( remakeDiffers && d == kRemakeFrame, "doctored: a remake that keeps the trust misses the remake frame's error" );
	check( !samePx( docUnit, red, true ), "doctored: a wrong known unit leaves another unit active (state differs)" );
	std::printf( "doc wrong unit, checked: knownStale %lld\n", staleDoc );
	check( samePx( docUnitChecked, red, true ) && staleDoc > 0, "doctored + WW_SETUP_MEMO_CHECK=1: the driver's unit wins, the stale count is non-zero" );

	std::printf( "%s (%d fail)\n", fails ? "CELLUNI5 FAIL" : "CELLUNI5 PASS", fails );
	return fails ? 1 : 0;
}
