/* tests/celluni2 (lane CELLUNI2): the per-draw setters (AO, SSR, reflection probes, SSGI) and the scene globals (fog,
 * sun, shadow set, cloud shadow) on the CELLUNI1 kit (src/gl/celluniform.h), run through the real driver. One c3-shaped
 * frame: an AO pass (1362 draws), the reflections pass (2926) and the main pass (1564), each an armed setup-memo epoch,
 * the globals on every third draw, one blended draw in seven, the renderer binding its own texture on whatever unit
 * is active. Green (cache + DSA + the per-draw blend/viewport cache) and the red (WW_CELL_UNI_RED, the old GL sequence)
 * must give the same pixels and texture state byte for byte. Doctored runs (a stale cache entry, a missing forget) must
 * differ. Usage: celluni2_gl [frames]   exit 0 = all ok */
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

enum Mode { Green, Red, DocStale, DocNoForget };

// ---- the per-draw GL cache (renderer.cpp DrawGlCache) ----
struct DrawGl
{
	int blend = -1, active = -1, vp[4] = {};
	bool vpOk = false;
};
static DrawGl dg;
static bool drawBlend( CountFn & cf )
{
	if ( dg.blend < 0 ) {
		dg.blend = cf.glIsEnabled( GL_BLEND ) ? 1 : 0;
		WwCellUni::queried();
	}
	return dg.blend != 0;
}
static int drawActive( CountFn & cf )
{
	if ( dg.active < 0 ) {
		GLint a = 0;
		cf.glGetIntegerv( GL_ACTIVE_TEXTURE, &a );
		WwCellUni::queried();
		dg.active = a;
	}
	return dg.active;
}
static void drawViewport( CountFn & cf, int vp[4], quint64 epoch, bool red )
{
	if ( !dg.vpOk ) {
		WwCellUni::State & cu = WwCellUni::state();
		if ( epoch && !red ) {	// the armed memo pass: the epoch's viewport
			const quint64 was = cu.vpEpoch;
			cu.vpEpoch = epoch;
			WwCellUni::viewport( &cf, dg.vp );
			cu.vpEpoch = was;
		} else {
			cf.glGetIntegerv( GL_VIEWPORT, dg.vp );
			WwCellUni::queried();
		}
		dg.vpOk = true;
	}
	for ( int i = 0; i < 4; i++ )
		vp[i] = dg.vp[i];
}

// ---- the GL scene ----
static const char * kVert = R"(#version 460 core
void main() { vec2 p = vec2( ( gl_VertexID & 1 ) * 4.0 - 1.0, ( gl_VertexID & 2 ) * 2.0 - 1.0 ); gl_Position = vec4( p, 0.0, 1.0 ); }
)";
static const char * kFrag = R"(#version 460 core
uniform sampler2D b0; uniform sampler2D b1; uniform sampler2D b2;
uniform sampler2D cellAo; uniform sampler2D cellAoDecal; uniform sampler2D cellCtGbuf; uniform sampler2D giHalf; uniform sampler2D giKey;
uniform sampler2D cellSsr; uniform sampler2D GreyscaleMap; uniform sampler2D cellSsgi; uniform sampler2DArray csmMap;
uniform sampler2D ussTerrMap; uniform sampler2D ussContactMap; uniform sampler2D ussCloudMap;
uniform int cellProbe; uniform int cellAoEarly; uniform bool cellAoOn; uniform bool cellAoDecalOn; uniform bool cellCtOn;
uniform bool cellGiHalfOn; uniform bool cellSsrOn; uniform bool cellSsrMat; uniform bool cellReflOn; uniform bool cellSsgiOn;
uniform vec4 cellAoRect; uniform vec4 cellAoDecalRect; uniform vec4 cellCtProj; uniform vec4 cellCtRect; uniform vec4 cellGiHalfRect;
uniform vec4 cellSsrRect; uniform vec4 cellSsgiRect; uniform vec4 cellReflAtlas; uniform float cellGiHalfScale; uniform float cellReflLods;
uniform vec2 cellSsgiK; uniform int cellSsgiRed; uniform int cellReflCount;
uniform vec4 cellReflPos[4]; uniform vec4 cellReflMin[4]; uniform vec4 cellReflMax[4];
uniform mat4 csmMat[3]; uniform mat4 ussWorldFromView; uniform mat4 ussCloudFromView;
uniform vec3 csmInvRange; uniform vec4 csmParams; uniform vec4 ussCloudFar; uniform int ussOn; uniform int ussCloudOn;
uniform vec4 fogA; uniform vec4 fogB; uniform vec4 fogC; uniform float fogD;
out vec4 o;
void main()
{
	vec2 t = vec2( 0.25, 0.75 );
	vec4 s = texture( b0, t ) + 2.0 * texture( b1, t ) + 3.0 * texture( b2, t ) + 5.0 * texture( cellAo, t ) + 7.0 * texture( cellAoDecal, t )
		+ 11.0 * texture( cellCtGbuf, t ) + 13.0 * texture( giHalf, t ) + 17.0 * texture( giKey, t ) + 19.0 * texture( cellSsr, t )
		+ 23.0 * texture( GreyscaleMap, t ) + 29.0 * texture( cellSsgi, t ) + 31.0 * texture( csmMap, vec3( t, 1.0 ) )
		+ 37.0 * texture( ussTerrMap, t ) + 41.0 * texture( ussContactMap, t ) + 43.0 * texture( ussCloudMap, t );
	float h = s.x + s.y * 0.5 + s.z * 0.25;
	h += float( cellProbe ) * 0.013 + float( cellAoEarly ) * 0.017 + float( cellAoOn ) * 0.019 + float( cellAoDecalOn ) * 0.023
		+ float( cellCtOn ) * 0.029 + float( cellGiHalfOn ) * 0.031 + float( cellSsrOn ) * 0.037 + float( cellSsrMat ) * 0.041
		+ float( cellReflOn ) * 0.043 + float( cellSsgiOn ) * 0.047 + float( cellSsgiRed ) * 0.053 + float( cellReflCount ) * 0.059
		+ float( ussOn ) * 0.061 + float( ussCloudOn ) * 0.067;
	h += dot( cellAoRect + cellAoDecalRect * 2.0 + cellCtProj * 3.0 + cellCtRect * 5.0 + cellGiHalfRect * 7.0 + cellSsrRect * 11.0
		+ cellSsgiRect * 13.0 + cellReflAtlas * 17.0 + csmParams * 19.0 + ussCloudFar * 23.0 + fogA * 29.0 + fogB * 31.0 + fogC * 37.0,
		vec4( 0.011, 0.013, 0.017, 0.019 ) );
	h += cellGiHalfScale * 0.07 + cellReflLods * 0.011 + dot( cellSsgiK, vec2( 0.03, 0.05 ) ) + dot( csmInvRange, vec3( 0.07, 0.11, 0.13 ) ) + fogD * 0.17;
	vec4 r = vec4( 0.0 );
	for ( int i = 0; i < 4; i++ )
		r += cellReflPos[i] * 0.03 + cellReflMin[i] * 0.05 + cellReflMax[i] * 0.07;
	h += dot( r, vec4( 0.1, 0.2, 0.3, 0.4 ) );
	vec4 one = vec4( 1.0, 0.5, 0.25, 1.0 );
	h += dot( csmMat[0] * one + csmMat[1] * one * 2.0 + csmMat[2] * one * 3.0 + ussWorldFromView * one * 5.0 + ussCloudFromView * one * 7.0,
		vec4( 0.01, 0.02, 0.03, 0.04 ) );
	o = vec4( fract( h ), fract( h * 7.0 ), fract( h * 31.0 ), 1.0 );
}
)";

static const int W = 128, H = 64, kProgs = 4;
static const int kPassDraws[3] = { 1362, 2926, 1564 };	// c3's obscurance / reflections / main cell draws
enum Pass { PAo, PSsr, PMain };
enum Unit { UAo = 14, UDecal = 15, UCt = 16, UGi = 17, UGiKey = 18, USsr = 19, URefl = 20, USsgi = 21, UCsm = 22, UTerr = 23, UCont = 24, UCloud = 25 };

struct Res
{
	GLuint fbo = 0, col = 0, vao = 0, base[3] = {}, ao = 0, decal = 0, gbuf = 0, gi = 0, giKey = 0, ssr = 0, atlas = 0, ssgi = 0,
		csm = 0, terr = 0, cont = 0, cloud = 0;
	Prog p[kProgs];
	std::map<GLuint, quint64> progStamp;
};

static GLuint makeTex()
{
	GLuint t = 0;
	gl->glGenTextures( 1, &t );
	return t;
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
	GLuint * two[] = { &R.base[0], &R.base[1], &R.base[2], &R.ao, &R.decal, &R.gbuf, &R.gi, &R.giKey, &R.ssr, &R.atlas, &R.ssgi,
		&R.terr, &R.cont, &R.cloud };
	int k = 0;
	for ( GLuint * t : two ) {
		*t = makeTex();
		fill2d( *t, (unsigned char)( 20 + 15 * k ), (unsigned char)( 240 - 13 * k ) );
		k++;
	}
	R.csm = makeTex();
	{
		std::vector<unsigned char> d( 2 * 2 * 3 * 4, 90 );
		for ( size_t i = 0; i < d.size(); i++ )
			d[i] = (unsigned char)( 60 + 7 * ( i % 11 ) );
		gl->glBindTexture( GL_TEXTURE_2D_ARRAY, R.csm );
		gl->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		gl->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		gl->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 2, 2, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, d.data() );
	}
	gl->glBindTexture( GL_TEXTURE_2D, 0 );
	gl->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	for ( int i = 0; i < kProgs; i++ ) {
		Prog & p = R.p[i];
		p.id = link();
		gl->glUseProgram( p.id );	// the samplers the cell setup sets elsewhere (outside these setters)
		gl->glUniform1i( p.loc( "b0" ), 0 );
		gl->glUniform1i( p.loc( "b1" ), 1 );
		gl->glUniform1i( p.loc( "b2" ), 2 );
		gl->glUniform1i( p.loc( "giHalf" ), UGi );
		gl->glUniform1i( p.loc( "giKey" ), UGiKey );
	}
	gl->glUseProgram( 0 );
}

static void destroy( Res & R )
{
	GLuint t[] = { R.col, R.base[0], R.base[1], R.base[2], R.ao, R.decal, R.gbuf, R.gi, R.giKey, R.ssr, R.atlas, R.ssgi, R.csm, R.terr,
		R.cont, R.cloud };
	gl->glDeleteTextures( GLsizei( sizeof t / sizeof t[0] ), t );
	gl->glDeleteFramebuffers( 1, &R.fbo );
	gl->glDeleteVertexArrays( 1, &R.vao );
	for ( auto & p : R.p )
		gl->glDeleteProgram( p.id );
}

static void rawMat( Prog & p, const char * n, float seed, bool forget )
{
	float m[16];
	for ( int i = 0; i < 16; i++ )
		m[i] = seed + 0.01f * float( i );
	const int l = p.loc( n );
	if ( l >= 0 ) {
		gl->glUniformMatrix4fv( l, 1, GL_FALSE, m );
		glUniCalls++;
		if ( forget )
			WwCellUni::wrote( p.wwUni, l, 1 );
	}
}

// the scene globals of one program-pass (wwSceneGlobals: fog, then the sun's wwSunShadowUniforms -> shadow set -> cloud)
static void globals( Res & R, Prog & p, CountFn & cf, const WwCellUni::Dsa * dsa, Mode mode, int frame )
{
	using namespace WwCellUni;
	state().red = mode == Red;
	const Scope scope( Globals );
	const float t = 0.01f * float( frame % 3 );
	u4f( p, "fogA", 0.1f, 0.2f, 0.3f, 0.4f );
	u4f( p, "fogB", 0.5f + t, 0.6f, 0.7f, 0.8f );
	u4f( p, "fogC", 0.05f, 0.04f, 0.03f, 0.02f );
	u1f( p, "fogD", 0.25f );
	{	// sun
		Units<CountFn> u( &cf, dsa );
		u.bind( UCsm, GL_TEXTURE_2D_ARRAY, R.csm );
		u.done();
		u1i( p, "csmMap", UCsm );
		static const char * const mats[3] = { "csmMat[0]", "csmMat[1]", "csmMat[2]" };
		for ( int i = 0; i < 3; i++ )
			rawMat( p, mats[i], 0.1f * float( i ) + t, true );
		u3f( p, "csmInvRange", 0.5f, 0.25f, 0.125f );
		u4f( p, "csmParams", 1.0f, 2.0f, 3.0f, 0.5f );
	}
	{	// shadow set
		u1i( p, "ussOn", 1 );
		Units<CountFn> u( &cf, dsa );
		u.bind( UTerr, GL_TEXTURE_2D, R.terr );
		u.bind( UCont, GL_TEXTURE_2D, ( frame & 1 ) ? R.cont : 0 );
		u.done();
		u1i( p, "ussTerrMap", UTerr );
		u1i( p, "ussContactMap", UCont );
		rawMat( p, "ussWorldFromView", 0.3f + t, true );
	}
	{	// cloud shadow
		u1i( p, "ussCloudOn", 1 );
		Units<CountFn> u( &cf, dsa );
		u.bind( UCloud, GL_TEXTURE_2D, R.cloud );
		u.done();
		u1i( p, "ussCloudMap", UCloud );
		rawMat( p, "ussCloudFromView", 0.7f, true );
		u4f( p, "ussCloudFar", 100.0f, 200.0f, 300.0f, 0.5f );
	}
}

// one draw's census: wwProgramCensus's blend sites, then wwCellAoDraw, wwCellSsrDraw, wwCellReflProbeDraw, wwCellSsgiDraw
static void census( Res & R, Prog & p, CountFn & cf, const WwCellUni::Dsa * dsa, Mode mode, int frame, Pass pass, int draw,
	quint64 epoch, quint64 stamp )
{
	using namespace WwCellUni;
	dg = DrawGl();	// the per-draw cache scope
	const bool uniRed = mode == Red;
	state().red = uniRed;
	const Scope scope( Draw );
	const bool fogMark = uniRed ? cf.glIsEnabled( GL_BLEND ) : drawBlend( cf );
	const bool cutWrote = !( uniRed ? cf.glIsEnabled( GL_BLEND ) : drawBlend( cf ) );
	(void)fogMark;
	(void)cutWrote;
	// AO
	if ( pass == PAo ) {
		u1i( p, "cellProbe", 20 );
		u1i( p, "cellAoEarly", 1 );
		u1b( p, "cellAoOn", false );
		u1b( p, "cellAoDecalOn", false );
		u1b( p, "cellCtOn", false );
	} else {
		const bool blend = drawBlend( cf );
		const bool on = !blend;
		Units<CountFn> u( &cf, dsa );
		if ( !u.dsa )
			u.seed( drawActive( cf ) );
		u.bind( UAo, GL_TEXTURE_2D, on ? R.ao : 0 );
		u.done();
		u1i( p, "cellAo", UAo );
		u1b( p, "cellAoOn", on );
		const int decalUnit = ( frame % 3 ) == 2 ? -1 : UDecal;	// the decal target missing in some frames
		const bool decalOn = decalUnit >= 0 && !blend;
		const int du = decalUnit >= 0 ? decalUnit : UAo;
		if ( decalUnit >= 0 )
			u.bind( du, GL_TEXTURE_2D, decalOn ? R.decal : 0 );
		else if ( !u.dsa )
			u.raw( du );
		u.done();
		u1i( p, "cellAoDecal", du );
		u1b( p, "cellAoDecalOn", decalOn );
		int vp[4];
		if ( decalOn ) {
			drawViewport( cf, vp, epoch, uniRed );
			u4f( p, "cellAoDecalRect", float( vp[0] ), float( vp[1] ), 0.0f, 0.0f );
		}
		const bool ctOn = !blend && ( draw % 5 ) != 0;
		u.bind( UCt, GL_TEXTURE_2D, ctOn ? R.gbuf : 0 );
		u.done();
		u1i( p, "cellCtGbuf", UCt );
		u1b( p, "cellCtOn", ctOn );
		if ( ctOn ) {
			u4f( p, "cellCtProj", 1.5f, 2.5f, 0.0f, 0.0f );
			u4f( p, "cellCtRect", 0.0f, 0.0f, float( W ), float( H ) );
		}
		const bool giOn = !blend;
		u.bind( UGi, GL_TEXTURE_2D, giOn ? R.gi : 0 );
		u.bind( UGiKey, GL_TEXTURE_2D, giOn ? R.giKey : 0 );
		u.done();
		u1b( p, "cellGiHalfOn", giOn );
		if ( giOn ) {
			drawViewport( cf, vp, epoch, uniRed );
			u4f( p, "cellGiHalfRect", float( vp[0] ), float( vp[1] ), 64.0f, 32.0f );
			u1f( p, "cellGiHalfScale", 0.5f );
		}
		if ( on ) {
			drawViewport( cf, vp, epoch, uniRed );
			u4f( p, "cellAoRect", float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) );
		}
	}
	// SSR (the AO pass skips it: aoLean)
	const bool flagged = ( draw % 4 ) != 0;
	if ( pass == PSsr ) {
		u1i( p, "cellProbe", 60 );
		u1b( p, "cellAoOn", false );
		u1b( p, "cellSsrOn", false );
		u1b( p, "cellSsrMat", flagged );
	} else if ( pass == PMain ) {
		const bool on = flagged && !drawBlend( cf );
		u1b( p, "cellSsrOn", on );
		u1b( p, "cellSsrMat", flagged );
		Units<CountFn> u( &cf, dsa );
		if ( !u.dsa )
			u.seed( drawActive( cf ) );
		u.bind( USsr, GL_TEXTURE_2D, on ? R.ssr : 0 );
		u.done();
		u1i( p, "cellSsr", USsr );
		if ( on ) {
			int vp[4];
			drawViewport( cf, vp, epoch, uniRed );
			u4f( p, "cellSsrRect", float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) );
		}
	}
	// a cached write over the probe array's first element (the overlap a raw array write must forget)
	if ( pass == PAo && ( frame % 4 ) == 1 )
		u4f( p, "cellReflPos[0]", 0.9f, 0.8f, 0.7f, 0.6f );
	// reflection probes
	{
		const bool on = pass != PSsr && !( uniRed ? cf.glIsEnabled( GL_BLEND ) : drawBlend( cf ) );
		u1b( p, "cellReflOn", on );
		if ( on ) {
			Units<CountFn> u( &cf, dsa );
			u.bind( URefl, GL_TEXTURE_2D, R.atlas );
			u.done();
			u1i( p, "GreyscaleMap", URefl );
			quint64 & ps = R.progStamp[p.id];
			if ( ps != stamp ) {
				ps = stamp;
				u1i( p, "cellReflCount", 3 );
				float a[12];
				for ( int i = 0; i < 12; i++ )
					a[i] = 0.05f * float( i ) + 0.01f * float( stamp % 7 );
				const int lp = p.loc( "cellReflPos[0]" ), ln = p.loc( "cellReflMin[0]" ), lx = p.loc( "cellReflMax[0]" );
				gl->glUniform4fv( lp, 3, a );
				gl->glUniform4fv( ln, 3, a );
				gl->glUniform4fv( lx, 3, a );
				glUniCalls += 3;
				if ( mode != DocNoForget ) {
					wrote( p.wwUni, lp, 3 );
					wrote( p.wwUni, ln, 3 );
					wrote( p.wwUni, lx, 3 );
				}
				u4f( p, "cellReflAtlas", 0.125f, 0.25f, 64.0f, 8.0f );
				u1f( p, "cellReflLods", 6.0f );
			}
		}
	}
	// SSGI (the AO pass skips it)
	if ( pass != PAo ) {
		const bool on = pass == PMain && !drawBlend( cf );
		u1b( p, "cellSsgiOn", on );
		Units<CountFn> u( &cf, dsa );
		if ( !u.dsa )
			u.seed( drawActive( cf ) );
		u.bind( USsgi, GL_TEXTURE_2D, on ? R.ssgi : 0 );
		u.done();
		u1i( p, "cellSsgi", USsgi );
		if ( on ) {
			int vp[4];
			drawViewport( cf, vp, epoch, uniRed );
			u4f( p, "cellSsgiRect", float( vp[0] ), float( vp[1] ), 1.0f / float( std::max( vp[2], 1 ) ), 1.0f / float( std::max( vp[3], 1 ) ) );
			u2f( p, "cellSsgiK", 0.6f, 0.4f );
			u1i( p, "cellSsgiRed", 0 );
		}
	}
}

struct FrameOut
{
	std::vector<unsigned char> px;
	std::vector<GLint> st;	// active unit + the 2D / 2D-array binding of units 0..31
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
	for ( int u = 0; u < 32; u++ ) {
		gl->glActiveTexture( GL_TEXTURE0 + u );
		GLint v = 0;
		gl->glGetIntegerv( GL_TEXTURE_BINDING_2D, &v );
		s.push_back( v );
		gl->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &v );
		s.push_back( v );
	}
	gl->glActiveTexture( GLenum( act ) );
	return s;
}

static int totalDraws() { return kPassDraws[0] + kPassDraws[1] + kPassDraws[2]; }

static std::vector<FrameOut> run( Mode mode, const WwCellUni::Dsa * dsa, int frames )
{
	using namespace WwCellUni;
	state() = State();
	Res R;
	build( R );
	CountFn cf;
	cf.f = gl;
	std::vector<FrameOut> out;
	quint64 epoch = 0;
	gl->glBlendFunc( GL_ONE, GL_ONE );
	for ( int f = 0; f < frames; f++ ) {
		FrameOut o;
		gl->glBindFramebuffer( GL_FRAMEBUFFER, R.fbo );
		gl->glViewport( 0, 0, W, H );
		gl->glDisable( GL_SCISSOR_TEST );
		gl->glDisable( GL_BLEND );
		gl->glClearColor( 0, 0, 0, 0 );
		gl->glClear( GL_COLOR_BUFFER_BIT );
		gl->glEnable( GL_SCISSOR_TEST );
		gl->glBindVertexArray( R.vao );
		gl->glFinish();
		state().frame = Counts();
		for ( auto & s : state().sites )
			s = Counts();
		const long long g0 = cf.gets, a0 = cf.actives, e0 = cf.enables, n0 = glUniCalls;
		const quint64 stamp = quint64( f / 5 + 1 );	// the probe set changes every 5 frames
		if ( mode == DocStale && f == 2 ) {	// a raw write the cache is not told about (what forget() is for)
			gl->glUseProgram( R.p[1].id );
			gl->glUniform1i( R.p[1].loc( "cellAo" ), 3 );
		}
		QElapsedTimer t;
		t.start();
		int px = 0;
		for ( int pass = 0; pass < 3; pass++ ) {
			epoch++;	// wwSetupMemoArm: each pass is one epoch
			gl->glViewport( 0, 0, W, pass == PSsr ? 48 : H );
			for ( int d = 0; d < kPassDraws[pass]; d++, px++ ) {
				Prog & p = R.p[d % kProgs];
				gl->glUseProgram( p.id );
				gl->glActiveTexture( GL_TEXTURE0 + d % 3 );	// the renderer's own unit
				if ( d % 7 == 0 )
					gl->glEnable( GL_BLEND );
				else
					gl->glDisable( GL_BLEND );
				if ( d % 2 == 0 )
					u1i( p, "GreyscaleMap", 3 );	// the material setup, outside every scope (its palette unit)
				if ( d % 3 == 0 )
					globals( R, p, cf, dsa, mode, f );
				census( R, p, cf, dsa, mode, f, Pass( pass ), d, mode == Red ? 0 : epoch, stamp );
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

	// the site split: a scope's counts land on its own site, nested scopes restore the outer one
	{
		using namespace WwCellUni;
		state() = State();
		Cache c;
		const float a = 1.0f;
		{
			const Scope d( Draw );
			want( c, 1, K1f, &a, 1, false );
			{
				const Scope g( Globals );
				want( c, 2, K1f, &a, 1, false );
				queried();
			}
			want( c, 3, K1f, &a, 1, false );
		}
		const State & s = state();
		check( s.frame.calls == 1 && s.sites[Draw].calls == 1 && s.sites[Globals].calls == 1, "site calls: one outer call, one per site" );
		check( s.sites[Draw].uploads == 2 && s.sites[Globals].uploads == 1 && s.sites[Globals].queries == 1 && s.frame.uploads == 3,
			"site counts: each upload/query on the innermost scope's site, the totals on the frame" );
		state() = State();
	}

	const std::vector<FrameOut> red = run( Red, &dsa, frames );
	const std::vector<FrameOut> green = run( Green, &dsa, frames );
	const std::vector<FrameOut> noDsa = run( Green, nullptr, frames );
	const std::vector<FrameOut> stale = run( DocStale, &dsa, frames );
	const std::vector<FrameOut> noForget = run( DocNoForget, &dsa, frames );
	std::vector<double> tr, tg;	// red and green interleaved, so drift hits both
	for ( int k = 0; k < 3; k++ ) {
		for ( const auto & o : run( Red, &dsa, frames ) )
			tr.push_back( o.cpuMs );
		for ( const auto & o : run( Green, &dsa, frames ) )
			tg.push_back( o.cpuMs );
	}

	const int draws = totalDraws();
	int pxDiff = 0, stDiff = 0, pxDiffN = 0, stDiffN = 0, pxStale = 0, pxNoForget = 0;
	long long litPx = 0;
	for ( int f = 0; f < frames; f++ ) {
		pxDiff += red[f].px != green[f].px;
		stDiff += red[f].st != green[f].st;
		pxDiffN += red[f].px != noDsa[f].px;
		stDiffN += red[f].st != noDsa[f].st;
		pxStale += red[f].px != stale[f].px;
		pxNoForget += red[f].px != noForget[f].px;
		for ( size_t i = 0; i < red[f].px.size(); i += 4 )
			litPx += red[f].px[i + 3] != 0;
	}
	auto line = []( const char * who, const FrameOut & o ) {
		std::printf( "  %-12s queries %6lld (glGetIntegerv %6lld + glIsEnabled %6lld) actives %6lld glUniform %6lld | cache uploads %llu kept %llu"
			" | draw: uploads %llu kept %llu queries %llu | globals: uploads %llu kept %llu queries %llu\n", who, o.gets + o.enables, o.gets,
			o.enables, o.actives, o.uni, (unsigned long long)o.cnt.uploads, (unsigned long long)o.cnt.kept,
			(unsigned long long)o.site[WwCellUni::Draw].uploads, (unsigned long long)o.site[WwCellUni::Draw].kept,
			(unsigned long long)o.site[WwCellUni::Draw].queries, (unsigned long long)o.site[WwCellUni::Globals].uploads,
			(unsigned long long)o.site[WwCellUni::Globals].kept, (unsigned long long)o.site[WwCellUni::Globals].queries );
	};
	for ( int f : { 0, 1, 5, frames - 1 } ) {
		std::printf( "frame %d (%d draws, %d globals)\n", f, draws, ( kPassDraws[0] + 2 ) / 3 + ( kPassDraws[1] + 2 ) / 3 + ( kPassDraws[2] + 2 ) / 3 );
		line( "red", red[f] );
		line( "green", green[f] );
		line( "green noDSA", noDsa[f] );
	}
	std::printf( "frames %d x %d draws, lit pixels %lld (of %lld)\n", frames, draws, litPx, (long long)frames * W * H );
	std::printf( "pixels differ red/green %d frames, state differs %d | red/no-DSA %d, %d\n", pxDiff, stDiff, pxDiffN, stDiffN );
	std::printf( "doctored: stale cache entry pixels differ %d frames | missing forget %d frames\n", pxStale, pxNoForget );
	const double mr = median( tr ), mg = median( tg );
	std::printf( "cpu ms per frame (median of %zu): red %.3f green %.3f (%.1f%%)\n", tr.size(), mr, mg, mr > 0 ? 100.0 * ( mg - mr ) / mr : 0.0 );

	check( litPx == (long long)frames * draws, "every draw drew its pixel" );
	check( pxDiff == 0 && stDiff == 0, "green == red: pixels and texture state byte for byte, every frame" );
	check( pxDiffN == 0 && stDiffN == 0, "green without DSA == red" );
	check( pxStale > 0, "control: a stale cache entry changes the picture (the comparator can fail)" );
	check( pxNoForget > 0, "control: a raw array write without forget changes the picture" );
	const FrameOut & gs = green[frames - 1];
	const FrameOut & rs = red[frames - 1];
	check( gs.enables == draws, "green: one blend query per draw" );
	check( gs.gets <= 3, "green: no unit query; the viewport once per pass (3 epochs)" );
	check( rs.gets + rs.enables >= 3LL * ( gs.gets + gs.enables ), "red asks the driver at least 3x as often (the old path)" );
	check( gs.uni < rs.uni / 2, "green uploads under half of the red's" );
	std::printf( "%s %d failed\n", fails ? "CELLUNI2 FAIL" : "CELLUNI2 PASS", fails );
	ctx.doneCurrent();
	return fails ? 1 : 0;
}
