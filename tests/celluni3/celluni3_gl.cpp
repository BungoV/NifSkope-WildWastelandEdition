/* tests/celluni3 (lane CELLUNI3): the CELLUNI2 leftovers on the kit (src/gl/celluniform.h), run through the real driver.
 * One c3-shaped frame (passes of 1362 / 2926 / 1564 draws, each an armed setup-memo epoch). Per draw, as setupProgram:
 * the entry forgets the blend note; the setup writes blend (noted) or not at all; one draw in five binds its own cube
 * (the decode mode, cached per texture); the scene globals on every third draw run the sky (aerial perspective) and the
 * air (volume, tile, viewport); the CE1 tail (landSplat, editorBrdf, wwDecalPom, the far ground's units, fxAdditive,
 * FXLIT1's disable) in its own cached scope; then the census reads the blend. Passes outside the window flip blend with
 * no note. Green, the red (WW_CELL_UNI_RED: the old GL sequence) and "prev" (main before this lane) must give the same
 * pixels and texture state byte for byte. Doctored runs (a blend write without its note, no forget at setupProgram's
 * entry, a raw tail write the cache does not see, a cube reload without its forget) must differ.
 * Usage: celluni3_gl [frames]   exit 0 = all ok */
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>
#include <QElapsedTimer>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

// the GL entries the setters call, counted on the way to the driver (independent of the header's counters)
struct CountFn
{
	QOpenGLExtraFunctions * f = nullptr;
	long long gets = 0, actives = 0, binds = 0, enables = 0;
	void glGetIntegerv( GLenum p, GLint * v ) { gets++; f->glGetIntegerv( p, v ); }
	void glGetTexLevelParameteriv( GLenum t, GLint l, GLenum p, GLint * v ) { gets++; f->glGetTexLevelParameteriv( t, l, p, v ); }
	void glActiveTexture( GLenum u ) { actives++; f->glActiveTexture( u ); }
	void glBindTexture( GLenum t, GLuint x ) { binds++; f->glBindTexture( t, x ); }
	void glTexBuffer( GLenum t, GLenum fmt, GLuint b ) { f->glTexBuffer( t, fmt, b ); }
	bool glIsEnabled( GLenum e ) { enables++; return f->glIsEnabled( e ) != GL_FALSE; }
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
static void u1b( Prog & p, const char * n, bool b ) { u1i( p, n, b ? 1 : 0 ); }
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
static void u4f( Prog & p, const char * n, float x, float y, float z, float w )
{
	const int l = p.loc( n );
	const float v[4] = { x, y, z, w };
	if ( WwCellUni::want( p.wwUni, l, WwCellUni::K4f, v, 4, false ) ) { gl->glUniform4f( l, x, y, z, w ); glUniCalls++; }
}

enum Mode { Green, Red, Prev, DocNoteMissing, DocNoEntryForget, DocTailStale, DocCubeNoForget };
static const char * const kModeName[] = { "green", "red", "prev", "doc-note-missing", "doc-no-entry-forget", "doc-tail-stale",
	"doc-cube-no-forget" };

// what each path of the lane is on in a mode (prev = main before CELLUNI3: every new path off, CELLUNI2's on)
struct Cfg
{
	bool red = false, seed = true, fxNote = true, cube = true, units = true, airVp = true, tail = true;
};
static Cfg cfgOf( Mode m )
{
	Cfg c;
	if ( m == Red )
		c.red = true;
	if ( m == Prev )
		c.seed = c.fxNote = c.cube = c.units = c.airVp = c.tail = false;
	return c;
}

// ---- the per-draw GL cache (renderer.cpp DrawGlCache), seeded from the blend note as WwDrawGlScope does ----
static int dgBlend = -1;
static bool drawBlend( CountFn & cf )
{
	if ( dgBlend < 0 ) {
		dgBlend = cf.glIsEnabled( GL_BLEND ) ? 1 : 0;
		WwCellUni::queried();
	}
	return dgBlend != 0;
}

// ---- the cube decode mode (renderer.cpp wwCellCubeDecodeMode + gltex.h ParamShadow::cube) ----
static std::vector<int> cubeCache;
static float cubeDecode( CountFn & cf, GLuint id )
{
	if ( id && id < cubeCache.size() && cubeCache[id] )
		return float( cubeCache[id] );
	GLint tex = GLint( id );
	if ( !id ) {
		cf.glGetIntegerv( GL_TEXTURE_BINDING_CUBE_MAP, &tex );
		if ( !tex )
			return 1.0f;
	}
	GLint f = 0;
	cf.glGetTexLevelParameteriv( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_INTERNAL_FORMAT, &f );
	const int mode = ( f != 0x8C40 && f != 0x8C41 && f != 0x8C42 && f != 0x8C43 && ( f < 0x8C48 || f > 0x8C4F ) && f != 0x8E8D ) ? 1 : 3;
	if ( id ) {
		if ( id >= cubeCache.size() )
			cubeCache.resize( id + 64 );
		cubeCache[id] = mode;
	}
	return float( mode );
}

// ---- the GL scene ----
static const char * kVert = R"(#version 460 core
void main() { vec2 p = vec2( ( gl_VertexID & 1 ) * 4.0 - 1.0, ( gl_VertexID & 2 ) * 2.0 - 1.0 ); gl_Position = vec4( p, 0.0, 1.0 ); }
)";
static const char * kFrag = R"(#version 460 core
uniform sampler2D b0; uniform samplerCube CubeMap; uniform sampler3D atApTex; uniform sampler2D atSkyTransTex;
uniform sampler3D volTex; uniform sampler2D volTile; uniform sampler2D fgCells; uniform sampler2DArray fgPagesN;
uniform bool cellAoOn; uniform int blendSeen; uniform bool landSplat; uniform int landSplatDbg; uniform bool editorBrdf;
uniform vec2 wwDecalPom; uniform int fgMode; uniform vec4 fgGridMap; uniform bool fxAdditive; uniform vec4 cellCubeMat;
uniform vec4 atAp; uniform float atSkyCamR; uniform vec4 volRect;
out vec4 o;
void main()
{
	vec2 t = vec2( 0.25, 0.75 );
	vec4 s = texture( b0, t ) + 2.0 * texture( CubeMap, vec3( 1.0, 0.2, 0.1 ) ) + 3.0 * texture( atApTex, vec3( t, 0.5 ) )
		+ 5.0 * texture( atSkyTransTex, t ) + 7.0 * texture( volTex, vec3( t, 0.25 ) ) + 11.0 * texture( volTile, t )
		+ 13.0 * texture( fgCells, t ) + 17.0 * texture( fgPagesN, vec3( t, 1.0 ) );
	float h = s.x + s.y * 0.5 + s.z * 0.25;
	h += float( cellAoOn ) * 0.013 + float( blendSeen ) * 0.017 + float( landSplat ) * 0.019 + float( landSplatDbg ) * 0.023
		+ float( editorBrdf ) * 0.029 + float( fgMode ) * 0.031 + float( fxAdditive ) * 0.037;
	h += dot( wwDecalPom, vec2( 0.041, 0.043 ) ) + atSkyCamR * 0.047
		+ dot( fgGridMap + cellCubeMat * 2.0 + atAp * 3.0 + volRect * 5.0, vec4( 0.011, 0.013, 0.017, 0.019 ) );
	o = vec4( fract( h ), fract( h * 7.0 ), fract( h * 31.0 ), 1.0 );
}
)";

static const int W = 128, H = 64, kProgs = 4;
static const int kPassDraws[3] = { 1362, 2926, 1564 };	// c3's obscurance / reflections / main cell draws
enum Unit { UCube = 8, UAtAp = 26, UAtTrans = 27, UVol = 28, UTile = 29, UFgCells = 30, UFgPages = 31 };

struct Res
{
	GLuint fbo = 0, col = 0, vao = 0, base[3] = {}, cube[3] = {}, ap = 0, trans = 0, vol = 0, tile = 0, fgCells = 0, fgPages = 0;
	Prog p[kProgs];
};

static GLuint makeTex()
{
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	return t;
}

static void params( GLenum target )
{
	gl->glTexParameteri( target, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	gl->glTexParameteri( target, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
}

static void fill2d( GLuint t, unsigned char r, unsigned char g )
{
	const unsigned char px[16] = { r, g, 10, 255, r, g, 20, 255, r, g, 30, 255, r, g, 40, 255 };
	gl->glBindTexture( GL_TEXTURE_2D, t );
	params( GL_TEXTURE_2D );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
}

static void fill3d( GLenum target, GLuint t, unsigned char seed )
{
	std::vector<unsigned char> d( 2 * 2 * 3 * 4 );
	for ( size_t i = 0; i < d.size(); i++ )
		d[i] = (unsigned char)( seed + 7 * ( i % 11 ) );
	gl->glBindTexture( target, t );
	params( target );
	gl->glTexImage3D( target, 0, GL_RGBA8, 2, 2, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, d.data() );
}

static void fillCube( GLuint t, GLenum fmt, unsigned char v )
{
	const unsigned char px[4] = { v, (unsigned char)( v / 2 ), 200, 255 };
	gl->glBindTexture( GL_TEXTURE_CUBE_MAP, t );
	params( GL_TEXTURE_CUBE_MAP );
	for ( int f = 0; f < 6; f++ )
		gl->glTexImage2D( GLenum( GL_TEXTURE_CUBE_MAP_POSITIVE_X + f ), 0, GLint( fmt ), 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px );
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
			char log[4096] = {};
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
	R.col = makeTex();
	gl->glBindTexture( GL_TEXTURE_2D, R.col );
	gl->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	gl->glBindFramebuffer( GL_FRAMEBUFFER, R.fbo );
	gl->glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.col, 0 );
	gl->glGenVertexArrays( 1, &R.vao );
	GLuint * two[] = { &R.base[0], &R.base[1], &R.base[2], &R.trans, &R.tile, &R.fgCells };
	int k = 0;
	for ( GLuint * t : two ) {
		*t = makeTex();
		fill2d( *t, (unsigned char)( 20 + 15 * k ), (unsigned char)( 240 - 13 * k ) );
		k++;
	}
	R.ap = makeTex();
	fill3d( GL_TEXTURE_3D, R.ap, 60 );
	R.vol = makeTex();
	fill3d( GL_TEXTURE_3D, R.vol, 90 );
	R.fgPages = makeTex();
	fill3d( GL_TEXTURE_2D_ARRAY, R.fgPages, 120 );
	const GLenum fmts[3] = { GL_RGBA8, GL_SRGB8_ALPHA8, GL_RGBA8 };
	for ( int i = 0; i < 3; i++ ) {
		R.cube[i] = makeTex();
		fillCube( R.cube[i], fmts[i], (unsigned char)( 50 + 60 * i ) );
	}
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	gl->glBindTexture( GL_TEXTURE_3D, 0 );
	gl->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	gl->glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
	for ( int i = 0; i < kProgs; i++ ) {
		Prog & p = R.p[i];
		p.id = link();
		gl->glUseProgram( p.id );	// the samplers the cell setup sets raw (outside these setters)
		gl->glUniform1i( p.loc( "b0" ), 0 );
		gl->glUniform1i( p.loc( "CubeMap" ), UCube );
	}
	gl->glUseProgram( 0 );
}

static void destroy( Res & R )
{
	GLuint t[] = { R.col, R.base[0], R.base[1], R.base[2], R.cube[0], R.cube[1], R.cube[2], R.ap, R.trans, R.vol, R.tile, R.fgCells,
		R.fgPages };
	gl->glDeleteTextures( GLsizei( sizeof t / sizeof t[0] ), t );
	gl->glDeleteFramebuffers( 1, &R.fbo );
	gl->glDeleteVertexArrays( 1, &R.vao );
	for ( auto & p : R.p )
		gl->glDeleteProgram( p.id );
}

// wwSceneGlobals' new members: the sky's aerial perspective (atmosphere.cpp) and the air (cellvolair.cpp)
static void globals( Res & R, Prog & p, CountFn & cf, const WwCellUni::Dsa * dsa, const Cfg & c, quint64 epoch, int frame )
{
	using namespace WwCellUni;
	State & st = state();
	st.red = c.red;
	st.vpEpoch = c.red ? 0 : epoch;
	{
		const Scope scope( Globals );
		const bool on = ( frame % 4 ) != 3;
		{	// sky
			Units<CountFn> u( &cf, c.units ? dsa : nullptr );
			u.section();
			u.bind( UAtAp, GL_TEXTURE_3D, on ? R.ap : 0 );
			u.done();
			u1i( p, "atApTex", UAtAp );
			u4f( p, "atAp", on ? 1.0f : 0.0f, 400.0f, on ? 0.5f : 0.0f, on ? 32.0f : 1.0f );
			u.bind( UAtTrans, GL_TEXTURE_2D, on ? R.trans : 0 );
			u.done();
			u1i( p, "atSkyTransTex", UAtTrans );
			u1f( p, "atSkyCamR", on ? 6371.0f : 0.0f );
		}
		{	// air
			Units<CountFn> u( &cf, c.units ? dsa : nullptr );
			u.section();
			u.bind( UVol, GL_TEXTURE_3D, R.vol );
			u.bind( UTile, GL_TEXTURE_2D, ( frame & 1 ) ? R.tile : 0 );
			u.done();
			u1i( p, "volTex", UVol );
			u1i( p, "volTile", UTile );
			GLint vp[4] = { 0, 0, 1, 1 };
			if ( c.airVp )
				viewport( &cf, vp );
			else
				cf.glGetIntegerv( GL_VIEWPORT, vp );
			u4f( p, "volRect", float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) );
		}
	}
	st.vpEpoch = 0;
}

struct FrameOut
{
	std::vector<unsigned char> px;
	std::vector<GLint> st;	// active unit, blend, the 2D / 2D-array / 3D / cube binding of units 0..31
	WwCellUni::Counts cnt, site[WwCellUni::SiteCount];
	long long gets = 0, actives = 0, enables = 0, uni = 0;
	double cpuMs = 0;
};

static std::vector<GLint> snapState()
{
	std::vector<GLint> s;
	GLint act = 0;
	gl->glGetIntegerv( GL_ACTIVE_TEXTURE, &act );
	s.push_back( act );
	s.push_back( gl->glIsEnabled( GL_BLEND ) );
	GLint dst = 0;
	gl->glGetIntegerv( GL_BLEND_DST_RGB, &dst );
	s.push_back( dst );
	for ( int u = 0; u < 32; u++ ) {
		gl->glActiveTexture( GL_TEXTURE0 + u );
		for ( GLenum b : { GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_2D_ARRAY, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_CUBE_MAP } ) {
			GLint v = 0;
			gl->glGetIntegerv( b, &v );
			s.push_back( v );
		}
	}
	gl->glActiveTexture( GLenum( act ) );
	return s;
}

static int totalDraws() { return kPassDraws[0] + kPassDraws[1] + kPassDraws[2]; }

static std::vector<FrameOut> run( Mode mode, const WwCellUni::Dsa * dsa, int frames )
{
	using namespace WwCellUni;
	const Cfg c = cfgOf( mode );
	state() = State();
	forgetBlend();
	cubeCache.clear();
	Res R;
	build( R );
	CountFn cf;
	cf.f = gl;
	std::vector<FrameOut> out;
	quint64 epoch = 0;
	for ( int f = 0; f < frames; f++ ) {
		FrameOut o;
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.fbo );
		gl->glViewport( 0, 0, W, H );
		gl->glDisable( GL_SCISSOR_TEST );
		gl->glDisable( GL_BLEND );
		gl->glBlendFunc( GL_ONE, GL_ZERO );
		gl->glClearColor( 0, 0, 0, 0 );
		gl->glClear( GL_COLOR_BUFFER_BIT );
		gl->glEnable( GL_SCISSOR_TEST );
		gl->glBindVertexArray( R.vao );
		if ( f == 3 ) {	// a cube reloaded with another format under the same id (gltex.cpp loadTex: paramForget)
			fillCube( R.cube[0], GL_SRGB8_ALPHA8, 70 );
			if ( mode != DocCubeNoForget && R.cube[0] < cubeCache.size() )
				cubeCache[R.cube[0]] = 0;
		}
		if ( mode == DocTailStale && f == 2 ) {	// a raw write the cache is not told about (what forget() is for)
			gl->glUseProgram( R.p[2].id );
			gl->glUniform1i( R.p[2].loc( "fgMode" ), 1 );
		}
		if ( f & 1 ) {	// writes outside the tail the cache must hear of: a raw one (wrote) and a setter outside any scope
			gl->glUseProgram( R.p[1].id );
			gl->glUniform1i( R.p[1].loc( "fgMode" ), 1 );
			glUniCalls++;
			WwCellUni::wrote( R.p[1].wwUni, R.p[1].loc( "fgMode" ), 1 );
			gl->glUseProgram( R.p[3].id );
			state().red = c.red;
			u1b( R.p[3], "editorBrdf", true );
		}
		gl->glFinish();
		state().frame = Counts();
		for ( auto & s : state().sites )
			s = Counts();
		const long long g0 = cf.gets, a0 = cf.actives, e0 = cf.enables, n0 = glUniCalls;
		QElapsedTimer t;
		t.start();
		int px = 0;
		for ( int pass = 0; pass < 3; pass++ ) {
			epoch++;	// wwSetupMemoArm: each pass is one epoch
			gl->glViewport( 0, 0, W, pass == 1 ? 48 : H );
			for ( int d = 0; d < kPassDraws[pass]; d++, px++ ) {
				Prog & p = R.p[d % kProgs];
				gl->glUseProgram( p.id );
				gl->glActiveTexture( GL_TEXTURE0 + d % 3 );	// the renderer's own unit
				// a pass between two draws (outside setupProgram) flips blend and notes nothing
				if ( d % 17 == 5 )
					gl->glEnable( GL_BLEND );
				else if ( d % 17 == 9 )
					gl->glDisable( GL_BLEND );
				// ---- setupProgram ----
				if ( mode != DocNoEntryForget )
					forgetBlend();
				const int kind = d % 11;
				bool effect = false;
				if ( kind == 0 ) {	// a route that writes no blend
				} else if ( kind <= 2 ) {	// an additive effect
					gl->glEnable( GL_BLEND );
					gl->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE );
					noteBlend( true );
					noteBlendDst( GL_ONE );
					effect = true;
				} else if ( kind == 3 ) {	// a blended effect
					gl->glEnable( GL_BLEND );
					gl->glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
					noteBlend( true );
					noteBlendDst( GL_ONE_MINUS_SRC_ALPHA );
					effect = true;
				} else if ( kind == 4 ) {	// PBRM's composition: the factor not noted (asked if needed)
					gl->glEnable( GL_BLEND );
					gl->glBlendFuncSeparate( GL_SRC_ALPHA, ( d & 1 ) ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE );
					noteBlend( true );
					noteBlendDst( -1 );
					effect = ( d & 2 ) != 0;
				} else {
					gl->glDisable( GL_BLEND );
					noteBlend( false );
					effect = kind == 5;
				}
				if ( d % 5 == 1 ) {	// the material's own env cube (CE1, before FO76)
					cf.glActiveTexture( GL_TEXTURE0 + UCube );
					const GLuint id = R.cube[( d / 5 ) % 3];
					gl->glBindTexture( GL_TEXTURE_CUBE_MAP, id );
					const float m = cubeDecode( cf, c.cube && !c.red ? id : 0 );
					u4f( p, "cellCubeMat", 0.5f, 0.25f, m, 1.0f );
				}
				// ---- ce1Tail ----
				if ( d % 3 == 0 )
					globals( R, p, cf, dsa, c, epoch, f );
				{
					state().red = c.red;
					std::vector<Scope *> none;
					Scope * tail = c.tail ? new Scope( Tail ) : nullptr;
					u1b( p, "landSplat", d % 4 == 0 );
					u1i( p, "landSplatDbg", 0 );
					u1b( p, "editorBrdf", false );
					u2f( p, "wwDecalPom", ( d & 1 ) ? 0.1f : 0.0f, 8.0f );
					if ( d % kProgs == 2 ) {	// a live-ground twin: CellFarGround::uniforms
						Units<CountFn> u( &cf, c.units ? dsa : nullptr );
						u.section();
						u.bind( UFgCells, GL_TEXTURE_2D, R.fgCells );
						u.bind( UFgPages, GL_TEXTURE_2D_ARRAY, R.fgPages );
						u1i( p, "fgCells", UFgCells );
						u1i( p, "fgPagesN", UFgPages );
						u4f( p, "fgGridMap", 10.0f, 20.0f, 1.0f / 4096.0f, 0.5f );
						u1i( p, "fgMode", 3 );
						u.done();
					} else {
						u1i( p, "fgMode", 0 );
					}
					if ( effect ) {
						const BlendNote bn = ( c.red || !c.fxNote ) ? BlendNote() : blendNote();
						bool additive = false;
						if ( bn.on == 1 && bn.dst >= 0 ) {
							additive = bn.dst == GL_ONE;
						} else if ( bn.on != 0 ) {
							GLint dst = 0;
							cf.glGetIntegerv( GL_BLEND_DST_RGB, &dst );
							additive = ( bn.on == 1 || cf.glIsEnabled( GL_BLEND ) ) && dst == GL_ONE;
						}
						u1b( p, "fxAdditive", additive );
						if ( kind == 1 && ( d / 11 ) % 2 == 0 ) {	// FXLIT1: a probe writes the lit effect opaque
							gl->glDisable( GL_BLEND );
							if ( mode != DocNoteMissing )
								noteBlend( false );
						}
					} else {
						u1b( p, "fxAdditive", false );
					}
					delete tail;
				}
				// ---- wwProgramCensus ----
				{
					dgBlend = -1;
					const BlendNote & bn = blendNote();
					if ( c.seed && bn.on >= 0 && !c.red )
						dgBlend = bn.on;
					state().red = c.red;
					const Scope scope( Draw );
					const bool blend = c.red ? cf.glIsEnabled( GL_BLEND ) : drawBlend( cf );
					const bool cut = !( c.red ? cf.glIsEnabled( GL_BLEND ) : drawBlend( cf ) );
					u1b( p, "cellAoOn", cut );
					u1i( p, "blendSeen", blend ? 1 : 0 );
				}
				gl->glBindTexture( GL_TEXTURE_2D, R.base[( d / 3 ) % 3] );	// the renderer binds on whatever unit is active
				gl->glScissor( px % W, px / W, 1, 1 );
				gl->glDrawArrays( GL_TRIANGLES, 0, 3 );
			}
		}
		o.cpuMs = double( t.nsecsElapsed() ) / 1e6;
		o.cnt = state().frame;
		for ( int i = 0; i < SiteCount; i++ )
			o.site[i] = state().sites[i];
		o.gets = cf.gets - g0;
		o.actives = cf.actives - a0;
		o.enables = cf.enables - e0;
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
	gl->glDisable( GL_BLEND );
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
	const int frames = argc > 1 ? std::max( 8, std::atoi( argv[1] ) ) : 30;

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
	check( dsa.bindMulti && dsa.texBuffer, "EXT_direct_state_access on this driver" );

	// the note itself: forgetBlend clears both members, noteBlend keeps the factor
	{
		using namespace WwCellUni;
		forgetBlend();
		noteBlendDst( GL_ONE );
		noteBlend( true );
		const bool kept = blendNote().on == 1 && blendNote().dst == GL_ONE;
		forgetBlend();
		check( kept && blendNote().on == -1 && blendNote().dst == -1, "blend note: noteBlend keeps the factor, forgetBlend clears both" );
		check( SiteCount == 4 && Tail == 3, "the tail site exists (the `cell uni:` line prints it)" );
	}

	const std::vector<FrameOut> red = run( Red, &dsa, frames );
	const std::vector<FrameOut> prev = run( Prev, &dsa, frames );
	const std::vector<FrameOut> green = run( Green, &dsa, frames );
	const std::vector<FrameOut> noDsa = run( Green, nullptr, frames );
	std::vector<std::vector<FrameOut>> docs;
	for ( Mode m : { DocNoteMissing, DocNoEntryForget, DocTailStale, DocCubeNoForget } )
		docs.push_back( run( m, &dsa, frames ) );
	std::vector<double> tr, tp, tg;	// interleaved, so drift hits all three
	for ( int k = 0; k < 3; k++ ) {
		for ( const auto & o : run( Red, &dsa, frames ) )
			tr.push_back( o.cpuMs );
		for ( const auto & o : run( Prev, &dsa, frames ) )
			tp.push_back( o.cpuMs );
		for ( const auto & o : run( Green, &dsa, frames ) )
			tg.push_back( o.cpuMs );
	}

	const int draws = totalDraws();
	auto differ = [&]( const std::vector<FrameOut> & a, bool state ) {
		int n = 0;
		for ( int f = 0; f < frames; f++ )
			n += state ? red[f].st != a[f].st : red[f].px != a[f].px;
		return n;
	};
	long long litPx = 0;
	for ( int f = 0; f < frames; f++ )
		for ( size_t i = 0; i < red[f].px.size(); i += 4 )
			litPx += red[f].px[i + 3] != 0;
	auto line = []( const char * who, const FrameOut & o ) {
		std::printf( "  %-12s queries %6lld (glGet* %6lld + glIsEnabled %6lld) unit moves %6lld glUniform %6lld | cache uploads %llu kept %llu"
			" | tail: uploads %llu kept %llu | globals: uploads %llu kept %llu\n", who, o.gets + o.enables, o.gets, o.enables, o.actives, o.uni,
			(unsigned long long)o.cnt.uploads, (unsigned long long)o.cnt.kept, (unsigned long long)o.site[WwCellUni::Tail].uploads,
			(unsigned long long)o.site[WwCellUni::Tail].kept, (unsigned long long)o.site[WwCellUni::Globals].uploads,
			(unsigned long long)o.site[WwCellUni::Globals].kept );
	};
	for ( int f : { 0, 1, 5, frames - 1 } ) {
		std::printf( "frame %d (%d draws)\n", f, draws );
		line( "red", red[f] );
		line( "prev", prev[f] );
		line( "green", green[f] );
		line( "green noDSA", noDsa[f] );
	}
	const int pxG = differ( green, false ), stG = differ( green, true ), pxN = differ( noDsa, false ), stN = differ( noDsa, true );
	const int pxP = differ( prev, false ), stP = differ( prev, true );
	std::printf( "frames %d x %d draws, lit pixels %lld (of %lld)\n", frames, draws, litPx, (long long)frames * W * H );
	std::printf( "pixels/state differ vs red: green %d/%d, no-DSA %d/%d, prev %d/%d frames\n", pxG, stG, pxN, stN, pxP, stP );
	int docPx[4];
	for ( int i = 0; i < 4; i++ ) {
		docPx[i] = differ( docs[size_t( i )], false );
		std::printf( "doctored %-20s pixels differ %d frames\n", kModeName[DocNoteMissing + i], docPx[i] );
	}
	const double mr = median( tr ), mp = median( tp ), mg = median( tg );
	std::printf( "cpu ms per frame (median of %zu): red %.3f prev %.3f green %.3f (green vs prev %.1f%%)\n", tr.size(), mr, mp, mg,
		mp > 0 ? 100.0 * ( mg - mp ) / mp : 0.0 );

	check( litPx == (long long)frames * draws, "every draw drew its pixel" );
	check( pxG == 0 && stG == 0, "green == red: pixels and texture/blend state byte for byte, every frame" );
	check( pxN == 0 && stN == 0, "green without DSA == red" );
	check( pxP == 0 && stP == 0, "prev (main before the lane) == red" );
	check( docPx[0] > 0, "control: a blend write without its note changes the picture" );
	check( docPx[1] > 0, "control: no blend forget at setupProgram's entry changes the picture" );
	check( docPx[2] > 0, "control: a stale tail cache entry (raw write, no forget) changes the picture" );
	check( docPx[3] > 0, "control: a cube reload without its forget changes the picture" );
	const FrameOut & gs = green[frames - 1];
	const FrameOut & ps = prev[frames - 1];
	const FrameOut & rs = red[frames - 1];
	check( gs.enables * 8 < draws, "green: the census blend asked only where the setup wrote none (under 1/8 of the draws)" );
	check( ps.enables >= draws, "prev: one blend query per draw at least (the cost this lane removes)" );
	check( gs.gets + gs.enables <= ( ps.gets + ps.enables ) / 4, "green asks the driver under a quarter as often as prev" );
	check( gs.actives < ps.actives / 2, "green moves the active unit under half as often as prev" );
	check( gs.uni < ps.uni, "green uploads fewer uniforms than prev (the tail's kept scalars)" );
	check( rs.gets + rs.enables >= ps.gets + ps.enables, "red asks at least as often as prev" );
	std::printf( "%s %d failed\n", fails ? "CELLUNI3 FAIL" : "CELLUNI3 PASS", fails );
	ctx.doneCurrent();
	return fails ? 1 : 0;
}
