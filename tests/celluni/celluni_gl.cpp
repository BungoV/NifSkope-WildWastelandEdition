/* tests/celluni (lane CELLUNI1): the cell-uniform cache and the DSA unit tracker of src/gl/celluniform.h, run through
 * the real driver. A frame of 1941 cell calls over 4 programs, the way wwCellLightsUniforms sets them up: green (cache +
 * DSA) and the red (WW_CELL_UNI_RED, the old GL sequence) must give the same pixels and the same texture state byte
 * for byte, with green asking the driver almost nothing. Two doctored runs must differ (the comparator's own red).
 * Usage: celluni_gl [frames]   exit 0 = all ok */
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>
#include <QElapsedTimer>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "gl/celluniform.h"

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

// the GL entries Units<F> calls, counted on the way to the driver (an instrument independent of the header's counters)
struct CountFn
{
	QOpenGLExtraFunctions * f = nullptr;
	long long gets = 0, actives = 0, binds = 0, texbufs = 0;
	void glGetIntegerv( GLenum p, GLint * v ) { gets++; f->glGetIntegerv( p, v ); }
	void glActiveTexture( GLenum u ) { actives++; f->glActiveTexture( u ); }
	void glBindTexture( GLenum t, GLuint x ) { binds++; f->glBindTexture( t, x ); }
	void glTexBuffer( GLenum t, GLenum fmt, GLuint b ) { texbufs++; f->glTexBuffer( t, fmt, b ); }
};

static QOpenGLExtraFunctions * gl = nullptr;
static long long glUniCalls = 0;

struct Prog
{
	GLuint id = 0;
	WwCellUni::Cache wwUni;
	std::map<std::string, int> locs;
	int loc( const char * n )
	{
		auto it = locs.find( n );
		if ( it != locs.end() )
			return it->second;
		return locs[n] = gl->glGetUniformLocation( id, n );
	}
};

// the Program setters as glcontext.cpp writes them
static void u1i( Prog & p, const char * n, int x )
{
	const int l = p.loc( n );
	if ( WwCellUni::want( p.wwUni, l, WwCellUni::K1i, &x, 1, false ) ) { gl->glUniform1i( l, x ); glUniCalls++; }
}
static void u1f( Prog & p, const char * n, float x )
{
	const int l = p.loc( n );
	if ( WwCellUni::want( p.wwUni, l, WwCellUni::K1f, &x, 1, false ) ) { gl->glUniform1f( l, x ); glUniCalls++; }
}
static void u2f( Prog & p, const char * n, float x, float y )
{
	const int l = p.loc( n );
	const float v[2] = { x, y };
	if ( WwCellUni::want( p.wwUni, l, WwCellUni::K2f, v, 2, false ) ) { gl->glUniform2f( l, x, y ); glUniCalls++; }
}
static void u3f( Prog & p, const char * n, float x, float y, float z )
{
	const int l = p.loc( n );
	const float v[3] = { x, y, z };
	if ( WwCellUni::want( p.wwUni, l, WwCellUni::K3f, v, 3, false ) ) { gl->glUniform3f( l, x, y, z ); glUniCalls++; }
}
static void u4f( Prog & p, const char * n, float x, float y, float z, float w )
{
	const int l = p.loc( n );
	const float v[4] = { x, y, z, w };
	if ( WwCellUni::want( p.wwUni, l, WwCellUni::K4f, v, 4, false ) ) { gl->glUniform4f( l, x, y, z, w ); glUniCalls++; }
}
static void u1fv( Prog & p, const char * n, int cnt, const float * x )
{
	const int l = p.loc( n );
	gl->glUniform1fv( l, cnt, x );
	glUniCalls++;
	WwCellUni::wrote( p.wwUni, l, cnt );
}

// ---- pure cache logic ----
static void unitCache()
{
	using namespace WwCellUni;
	state() = State();
	Cache c;
	const float a = 1.0f, b = 2.0f;
	check( want( c, 3, K1f, &a, 1, false ), "outside the scope: always uploads" );
	{
		Scope s;
		check( want( c, 3, K1f, &a, 1, false ), "first write in scope uploads" );
		check( !want( c, 3, K1f, &a, 1, false ), "same bits kept" );
		check( want( c, 3, K1f, &b, 1, false ), "changed value uploads" );
		check( want( c, 3, K1i, &b, 1, false ), "same bits, other kind uploads" );
		check( want( c, 3, K1i, &b, 1, true ), "a recording draw packet always uploads" );
		check( !want( c, -1, K1f, &a, 1, false ), "location -1 skipped" );
		check( state().frame.unused == 1, "location -1 counted unused" );
		want( c, 5, K4f, &a, 1, false );
		wrote( c, 4, 3 );
		check( want( c, 5, K4f, &a, 1, false ), "an array write over the location forgets it" );
	}
	want( c, 3, K1i, &b, 1, false );	// outside: the slot is unknown afterwards
	{
		Scope s;
		check( want( c, 3, K1i, &b, 1, false ), "a write outside the scope forgets the slot" );
		c.forget( 3, 1 );
		check( want( c, 3, K1i, &b, 1, false ), "forget() forgets" );
		c.reset();
		check( want( c, 3, K1i, &b, 1, false ), "reset() forgets all" );
	}
	state().red = true;
	{
		Scope s;
		check( want( c, 3, K1i, &b, 1, false ) && want( c, 3, K1i, &b, 1, false ), "red: every write uploads" );
	}
	state() = State();
	// red of the unit test itself: a cache that never forgets on an outside write keeps a stale value
	{
		Cache d;
		Scope s;
		want( d, 7, K1f, &a, 1, false );
		d.s[7].kind = K1f;	// doctored: the outside write below is not seen
		check( !want( d, 7, K1f, &a, 1, false ), "doctored control: a stale slot is skipped (the case forget() exists for)" );
	}
	state() = State();
}

// ---- the GL scene ----
static const char * kVert = R"(#version 460 core
void main() { vec2 p = vec2( ( gl_VertexID & 1 ) * 4.0 - 1.0, ( gl_VertexID & 2 ) * 2.0 - 1.0 ); gl_Position = vec4( p, 0.0, 1.0 ); }
)";
static const char * kFrag = R"(#version 460 core
uniform samplerBuffer tb; uniform sampler3D g3a; uniform sampler3D g3b; uniform sampler2D bl; uniform samplerCubeArray sh;
uniform sampler2D b0; uniform sampler2D b1; uniform sampler2D b2;
uniform int tbIdx; uniform vec4 a; uniform float b; uniform vec3 d; uniform vec2 e; uniform float arr[4]; uniform vec4 rect;
out vec4 o;
void main()
{
	vec4 s = texelFetch( tb, tbIdx ) * 0.2 + texture( g3a, vec3( 0.3 ) ) * 0.1 + texture( g3b, vec3( 0.6 ) ) * 0.1
		+ texture( bl, vec2( 0.5 ) ) * 0.1 + texture( sh, vec4( 1.0, 0.2, 0.1, 1.0 ) ) * 0.1
		+ texture( b0, vec2( 0.5 ) ) * 0.07 + texture( b1, vec2( 0.5 ) ) * 0.07 + texture( b2, vec2( 0.5 ) ) * 0.07;
	o = s + a * 0.05 + vec4( b, d.y, e.x, arr[2] ) * 0.05 + vec4( rect.zw * 8.0, 0.0, 0.0 );
}
)";

static const int W = 64, H = 32, kCalls = 1941, kProgs = 4;

struct Res
{
	GLuint fbo = 0, col = 0, vao = 0, tbBuf = 0, tbTex = 0, gi = 0, room = 0, bloom = 0, shadow = 0, base[3] = {};
	bool tbAttached = false;
	Prog p[kProgs];
};

static GLuint makeTex( GLenum target )
{
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	return t;
}

static void fill3d( GLuint t, float v )
{
	std::vector<float> d( 4 * 4 * 4 * 4 );
	for ( size_t i = 0; i < d.size(); i++ )
		d[i] = v + float( i % 7 ) * 0.01f;
	gl->glBindTexture( GL_TEXTURE_3D, t );
	gl->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	gl->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	gl->glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA32F, 4, 4, 4, 0, GL_RGBA, GL_FLOAT, d.data() );
}

static void fill2d( GLuint t, unsigned char r, unsigned char g )
{
	const unsigned char px[16] = { r, g, 10, 255, r, g, 20, 255, r, g, 30, 255, r, g, 40, 255 };
	gl->glBindTexture( GL_TEXTURE_2D, t );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	gl->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
}

static GLuint link()
{
	auto sh = [&]( GLenum t, const char * s ) {
		GLuint x = gl->glCreateShader( t );
		gl->glShaderSource( x, 1, &s, nullptr );
		gl->glCompileShader( x );
		GLint ok = 0;
		gl->glGetShaderiv( x, GL_COMPILE_STATUS, &ok );
		if ( !ok ) {
			char log[2048] = {};
			gl->glGetShaderInfoLog( x, sizeof log, nullptr, log );
			std::printf( "shader: %s\n", log );
		}
		return x;
	};
	GLuint p = gl->glCreateProgram();
	gl->glAttachShader( p, sh( GL_VERTEX_SHADER, kVert ) );
	gl->glAttachShader( p, sh( GL_FRAGMENT_SHADER, kFrag ) );
	gl->glLinkProgram( p );
	GLint ok = 0;
	gl->glGetProgramiv( p, GL_LINK_STATUS, &ok );
	if ( !ok )
		std::printf( "link failed\n" );
	return p;
}

static void build( Res & R )
{
	gl->glActiveTexture( GL_TEXTURE0 );
	gl->glGenFramebuffers( 1, &R.fbo );
	R.col = makeTex( GL_TEXTURE_2D );
	gl->glBindTexture( GL_TEXTURE_2D, R.col );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, R.fbo );
	gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.col, 0 );
	gl->glGenVertexArrays( 1, &R.vao );
	gl->glBindVertexArray( R.vao );
	gl->glGenBuffers( 1, &R.tbBuf );
	std::vector<float> tb( 8 * 4 );
	for ( size_t i = 0; i < tb.size(); i++ )
		tb[i] = float( i ) / 40.0f;
	gl->glBindBuffer( GL_TEXTURE_BUFFER, R.tbBuf );
	gl->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( tb.size() * 4 ), tb.data(), GL_STATIC_DRAW );
	gl->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	R.tbTex = makeTex( GL_TEXTURE_BUFFER );
	R.gi = makeTex( GL_TEXTURE_3D );
	R.room = makeTex( GL_TEXTURE_3D );
	fill3d( R.room, 0.4f );
	R.bloom = makeTex( GL_TEXTURE_2D );
	fill2d( R.bloom, 200, 30 );
	R.shadow = makeTex( GL_TEXTURE_CUBE_MAP_ARRAY );
	{
		std::vector<float> d( 2 * 2 * 6 * 4, 0.7f );
		gl->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, R.shadow );
		gl->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		gl->glTexParameteri( GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		gl->glTexImage3D( GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA16F, 2, 2, 6, 0, GL_RGBA, GL_FLOAT, d.data() );
	}
	for ( int i = 0; i < 3; i++ ) {
		R.base[i] = makeTex( GL_TEXTURE_2D );
		fill2d( R.base[i], (unsigned char)( 40 + 60 * i ), (unsigned char)( 250 - 70 * i ) );
	}
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	gl->glBindTexture( GL_TEXTURE_3D, 0 );
	gl->glBindTexture( GL_TEXTURE_CUBE_MAP_ARRAY, 0 );
	for ( int i = 0; i < kProgs; i++ )
		R.p[i].id = link();
}

static void destroy( Res & R )
{
	GLuint t[] = { R.col, R.tbTex, R.gi, R.room, R.bloom, R.shadow, R.base[0], R.base[1], R.base[2] };
	gl->glDeleteTextures( GLsizei( sizeof t / sizeof t[0] ), t );
	gl->glDeleteBuffers( 1, &R.tbBuf );
	gl->glDeleteFramebuffers( 1, &R.fbo );
	gl->glDeleteVertexArrays( 1, &R.vao );
	for ( auto & p : R.p )
		gl->glDeleteProgram( p.id );
}

enum Mode { Green, Red, DocStale, DocNoRestore };

struct FrameOut
{
	std::vector<unsigned char> px;
	std::vector<GLint> st;	// active unit + the binding of each target on units 0..15
	WwCellUni::Counts cnt;
	long long gets = 0, actives = 0, uni = 0;
	double cpuMs = 0;
};

static std::vector<GLint> snapState()
{
	std::vector<GLint> s;
	GLint act = 0;
	gl->glGetIntegerv( GL_ACTIVE_TEXTURE, &act );
	s.push_back( act );
	const GLenum q[] = { GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_BUFFER, GL_TEXTURE_BINDING_CUBE_MAP_ARRAY };
	for ( int u = 0; u < 16; u++ ) {
		gl->glActiveTexture( GL_TEXTURE0 + u );
		for ( GLenum e : q ) {
			GLint v = 0;
			gl->glGetIntegerv( e, &v );
			s.push_back( v );
		}
		GLint d = 0;
		gl->glGetIntegerv( GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &d );
		s.push_back( d );
	}
	gl->glActiveTexture( GLenum( act ) );
	return s;
}

// one cell call: the shape of wwCellLightsUniforms (sections, an upload inside a section, the LUT leave, the viewport)
static void cellCall( Res & R, Prog & p, CountFn & cf, const WwCellUni::Dsa * dsa, Mode mode, int frame, int call )
{
	using namespace WwCellUni;
	state().red = mode == Red;
	const Scope scope;
	Units<CountFn> u( &cf, dsa );
	const bool broken = mode == DocNoRestore && call == 0 && frame == 3;	// the doctored run: no restore in this call
	auto done = [&]() { if ( !broken ) u.done(); };
	u.section();
	u.bind( 9, GL_TEXTURE_BUFFER, R.tbTex );
	if ( !R.tbAttached ) {
		u.texBuffer( R.tbTex, GL_RGBA32F, R.tbBuf );
		R.tbAttached = true;
	}
	if ( call == 0 && ( frame == 0 || frame == 3 ) ) {	// an upload: moves the active unit, done() puts it back
		u.raw( 10 );
		fill3d( R.gi, frame == 0 ? 0.2f : 0.5f );
	}
	u.bind( 10, GL_TEXTURE_3D, R.gi );
	u.bind( 11, GL_TEXTURE_3D, ( call & 1 ) ? R.room : 0 );	// zero binds too
	done();
	u1i( p, "tbIdx", call % 8 );
	u4f( p, "a", 0.1f, 0.2f, 0.3f, 0.4f + 0.1f * float( &p - R.p ) );
	u1f( p, "b", 0.1f * float( frame % 5 ) );
	u3f( p, "d", 0.5f, 0.25f, 0.125f );
	u2f( p, "e", 0.75f, 0.5f );
	const float arr[4] = { 0.1f, 0.2f, 0.3f + 0.01f * float( frame ), 0.4f };
	u1fv( p, "arr", 4, arr );
	u1i( p, "tb", 9 );
	u1i( p, "g3a", 10 );
	u1i( p, "g3b", 11 );
	u1i( p, "bl", 12 );
	u1i( p, "sh", 13 );
	u1i( p, "b0", 0 );
	u1i( p, "b1", 1 );
	u1i( p, "b2", 2 );
	u1i( p, "nothere", 5 );	// location -1
	if ( call == 1 && frame == 2 ) {	// the LUT upload: the old code left the active unit on its unit
		u.leave( 12 );
		fill2d( R.bloom, 210, 35 );
	}
	u.section();
	u.bind( 12, GL_TEXTURE_2D, R.bloom );
	done();
	int vp[4] = { 0, 0, 1, 1 };
	WwCellUni::viewport( &cf, vp );
	u4f( p, "rect", float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) );
	u.bind( 13, GL_TEXTURE_CUBE_MAP_ARRAY, R.shadow );
	done();
}

static std::vector<FrameOut> run( Mode mode, const WwCellUni::Dsa * dsa, int frames )
{
	using namespace WwCellUni;
	state() = State();
	Res R;
	build( R );
	CountFn cf;
	cf.f = gl;
	std::vector<FrameOut> out;
	for ( int f = 0; f < frames; f++ ) {
		FrameOut o;
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.fbo );
		gl->glViewport( 0, 0, W, H );
		gl->glDisable( GL_SCISSOR_TEST );
		gl->glClearColor( 0, 0, 0, 0 );
		gl->glClear( GL_COLOR_BUFFER_BIT );
		gl->glEnable( GL_SCISSOR_TEST );
		gl->glBindVertexArray( R.vao );
		gl->glFinish();
		state().frame = Counts();
		state().vpEpoch = mode == Red ? 0 : quint64( f + 1 );	// one armed setup-memo pass per frame
		const long long g0 = cf.gets, a0 = cf.actives, n0 = glUniCalls;
		if ( mode == DocStale && f == 2 ) {	// a raw write the cache is not told about (what forget() is for)
			gl->glUseProgram( R.p[1].id );
			gl->glUniform4f( R.p[1].loc( "a" ), 0.9f, 0.9f, 0.9f, 0.9f );
		}
		QElapsedTimer t;
		t.start();
		for ( int c = 0; c < kCalls; c++ ) {
			Prog & p = R.p[c % kProgs];
			gl->glUseProgram( p.id );
			gl->glActiveTexture( GL_TEXTURE0 + c % 3 );	// the renderer's own unit
			cellCall( R, p, cf, dsa, mode, f, c );
			gl->glBindTexture( GL_TEXTURE_2D, R.base[( c / 3 ) % 3] );	// the renderer binds on whatever unit is active
			gl->glScissor( c % W, c / W, 1, 1 );
			gl->glDrawArrays( GL_TRIANGLES, 0, 3 );
		}
		o.cpuMs = double( t.nsecsElapsed() ) / 1e6;
		state().vpEpoch = 0;
		o.cnt = state().frame;
		o.gets = cf.gets - g0;
		o.actives = cf.actives - a0;
		o.uni = glUniCalls - n0;
		gl->glFinish();
		o.px.resize( size_t( W * H * 4 ) );
		gl->glReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, o.px.data() );
		o.st = snapState();
		out.push_back( std::move( o ) );
	}
	gl->glUseProgram( 0 );
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
	const int frames = argc > 1 ? std::max( 6, std::atoi( argv[1] ) ) : 30;
	unitCache();

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
	WwCellUni::Dsa dsa;
	if ( ctx.hasExtension( QByteArrayLiteral( "GL_EXT_direct_state_access" ) ) ) {
		dsa.bindMulti = reinterpret_cast<WwCellUni::BindMultiFn>( ctx.getProcAddress( "glBindMultiTextureEXT" ) );
		dsa.texBuffer = reinterpret_cast<WwCellUni::TexBufferFn>( ctx.getProcAddress( "glTextureBufferEXT" ) );
	}
	std::printf( "dsa %d\n", dsa.bindMulti && dsa.texBuffer ? 1 : 0 );
	check( dsa.bindMulti && dsa.texBuffer, "EXT_direct_state_access on this driver" );

	// the viewport cache: one ask per epoch; WW_SETUP_MEMO_CHECK finds a stale one
	{
		using namespace WwCellUni;
		state() = State();
		CountFn cf;
		cf.f = gl;
		gl->glViewport( 0, 0, 640, 360 );
		state().vpEpoch = 7;
		int vp[4];
		viewport( &cf, vp );
		viewport( &cf, vp );
		check( cf.gets == 1 && vp[2] == 640, "viewport: asked once in an epoch" );
		gl->glViewport( 0, 0, 320, 180 );
		viewport( &cf, vp );
		check( vp[2] == 640, "control: a viewport moved inside an epoch is NOT seen without the check (the memo invariant)" );
		state().vpCheck = true;
		viewport( &cf, vp );
		check( state().vpStale == 1 && vp[2] == 320, "memo check: the stale viewport is counted and corrected" );
		state().vpEpoch = 8;
		state().vpCheck = false;
		viewport( &cf, vp );
		check( vp[2] == 320, "a new epoch asks again" );
		state() = State();
	}

	const std::vector<FrameOut> red = run( Red, &dsa, frames );
	const std::vector<FrameOut> green = run( Green, &dsa, frames );
	const std::vector<FrameOut> noDsa = run( Green, nullptr, frames );
	const std::vector<FrameOut> stale = run( DocStale, &dsa, frames );
	const std::vector<FrameOut> noRestore = run( DocNoRestore, &dsa, frames );
	// timing: red and green interleaved, so drift hits both
	std::vector<double> tr, tg;
	for ( int k = 0; k < 3; k++ ) {
		for ( const auto & o : run( Red, &dsa, frames ) )
			tr.push_back( o.cpuMs );
		for ( const auto & o : run( Green, &dsa, frames ) )
			tg.push_back( o.cpuMs );
	}

	int pxDiff = 0, stDiff = 0, pxDiffN = 0, stDiffN = 0, pxStale = 0, stNoRestore = 0, pxNoRestore = 0;
	long long litPx = 0;
	for ( int f = 0; f < frames; f++ ) {
		pxDiff += red[f].px != green[f].px;
		stDiff += red[f].st != green[f].st;
		pxDiffN += red[f].px != noDsa[f].px;
		stDiffN += red[f].st != noDsa[f].st;
		pxStale += red[f].px != stale[f].px;
		pxNoRestore += red[f].px != noRestore[f].px;
		stNoRestore += red[f].st != noRestore[f].st;
		for ( size_t i = 0; i < red[f].px.size(); i += 4 )
			litPx += red[f].px[i + 3] != 0;
	}
	for ( int f = 0; f < std::min( frames, 6 ); f++ )
		std::printf( "frame %d | red: queries %lld actives %lld glUniform %lld | green: queries %lld actives %lld glUniform %lld "
			"(cache uploads %llu kept %llu unused %llu) | green no-DSA: queries %lld\n", f, red[f].gets, red[f].actives, red[f].uni,
			green[f].gets, green[f].actives, green[f].uni, (unsigned long long)green[f].cnt.uploads,
			(unsigned long long)green[f].cnt.kept, (unsigned long long)green[f].cnt.unused, noDsa[f].gets );
	std::printf( "frames %d x %d calls, lit pixels %lld (of %d)\n", frames, kCalls, litPx, frames * W * H );
	std::printf( "pixels differ red/green %d frames, state differs %d | red/no-DSA %d, %d\n", pxDiff, stDiff, pxDiffN, stDiffN );
	std::printf( "doctored: stale cache pixels differ %d frames | no restore pixels %d state %d\n", pxStale, pxNoRestore, stNoRestore );
	const double mr = median( tr ), mg = median( tg );
	std::printf( "cpu ms per frame (median of %zu): red %.3f green %.3f (%.1f%%)\n", tr.size(), mr, mg, mr > 0 ? 100.0 * ( mg - mr ) / mr : 0.0 );

	check( litPx == (long long)frames * kCalls, "every call drew its pixel" );
	check( pxDiff == 0 && stDiff == 0, "green == red: pixels and texture state byte for byte, every frame" );
	check( pxDiffN == 0 && stDiffN == 0, "green without DSA == red" );
	check( pxStale > 0, "control: a stale cache entry changes the picture (the comparator can fail)" );
	check( pxNoRestore > 0 || stNoRestore > 0, "control: a missing restore changes the picture/state" );
	long long gq = 0, rq = 0;
	for ( int f = 4; f < frames; f++ ) {
		gq += green[f].gets;
		rq += red[f].gets;
	}
	check( rq >= (long long)( frames - 4 ) * kCalls * 3, "red asks the driver 3+ times per call (the old path)" );
	check( gq <= frames - 4, "green steady frames: at most one query per frame (the epoch's viewport)" );
	check( green[3].gets <= 3, "green upload frame: the lazy ask only" );
	check( green[frames - 1].uni < red[frames - 1].uni / 4, "green uploads under a quarter of the red's" );
	std::printf( "%s %d failed\n", fails ? "CELLUNI FAIL" : "CELLUNI PASS", fails );
	ctx.doneCurrent();
	return fails ? 1 : 0;
}
