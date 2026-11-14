// Lane VKFRAME1 round 11 (2026-11-12): the tone map on the present device (vkpresent.cpp toneFrame / toneRun, built in
// here with WW_VKPRESENT_NO_QT: the shipped code) against GL's own tone quads (the tree's cell_hdr.vert + cell_hdr.frag,
// expanded by toneview_expand.py) drawn the way cellhdr.cpp wwCellHdrEnd draws them, on the same frame, per sample.
// Usage: toneview <glsl dir> [W H]
// GL draws a linear frame (RGBA16F + D24S8, 4 samples, per-sample colour / stencil / depth from seeded data) into the
// images toneFrame made, blits its depth into the view (viewBegin) and lays a backdrop into the view's colour; then
// toneRun draws modes 0 / 2 / 1 on Vulkan into the view. The reference is GL's cell_hdr program into a GL MS RGBA8 +
// D24S8 target with the same depth and backdrop. Both read back per sample (texelFetch, every byte).
// Prints `TONEVIEW <job> EXACT|BOUND|FAIL` (BOUND: no byte off by more than 1), `TONEVIEW RED ...` (the Vulkan run
// with splitSky off against the split reference: must FAIL) and `TONEVIEW GATE OK|FAILED`.
// Lane VKPOST2: `TONEVIEW MERGE <job> <output> EXACT|FAIL`: the tone pass + TAA in one crossing against two crossings,
// and `TONEVIEW MERGE RED` (the view's resolve into cur skipped: must FAIL).
// Lane VKBLOOM1: `TONEVIEW BLOOM <case> view|measure EXACT|BOUND|FAIL`: the Fusion Shaders bloom chain + mode 3 + the
// quarter measure in the tone pass against GL's cellbloom.cpp chain (the tree's cell_bloom_*.frag + cell_is_measure_q.frag),
// `TONEVIEW BLOOM RED 1|2|3` (mode 3 not drawn / measure not run / no backdrop: must FAIL), and `TONEVIEW MERGE bloom`
// (bloom + measure + tone + TAA in one crossing against two).
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace tv
{
HMODULE gOpenGl = nullptr;
void * glProc( const char * name )
{
	void * p = reinterpret_cast<void *>( wglGetProcAddress( name ) );
	const intptr_t i = reinterpret_cast<intptr_t>( p );
	if ( !p || i == 1 || i == 2 || i == 3 || i == -1 )
		p = reinterpret_cast<void *>( GetProcAddress( gOpenGl, name ) );
	return p;
}

LRESULT CALLBACK wndProc( HWND h, UINT m, WPARAM wp, LPARAM lp )
{
	return DefWindowProcA( h, m, wp, lp );
}

void pump()
{
	MSG msg;
	while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &msg );
		DispatchMessageA( &msg );
	}
}

constexpr GLenum kT2DMS = 0x9100, kT3D = 0x806F, kFb = 0x8D40, kRead = 0x8CA8, kDraw = 0x8CA9, kCol0 = 0x8CE0, kDsAtt = 0x821A;
constexpr GLenum kRgb32f = 0x8815, kRgb8 = 0x8051, kD24s8 = 0x88F0;	// kRgba8 / kRgba32f: vkpresent.cpp's
constexpr GLenum kR8ui = 0x8232, kR32f = 0x822E, kRedInt = 0x8D94, kRed = 0x1903, kClamp = 0x812F, kWrapR = 0x8072;
constexpr GLenum kTex0 = 0x84C0, kFrag = 0x8B30, kVert = 0x8B31, kCompiled = 0x8B81, kLinked = 0x8B82, kActiveUnis = 0x8B86;
constexpr GLenum kComplete = 0x8CD5, kMaxLevel = 0x813D;

struct Fns
{
	GLuint( WINAPI * createShader )( GLenum ) = nullptr;
	void( WINAPI * shaderSource )( GLuint, GLsizei, const char * const *, const GLint * ) = nullptr;
	void( WINAPI * compileShader )( GLuint ) = nullptr;
	void( WINAPI * getShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getShaderInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	GLuint( WINAPI * createProgram )() = nullptr;
	void( WINAPI * attachShader )( GLuint, GLuint ) = nullptr;
	void( WINAPI * linkProgram )( GLuint ) = nullptr;
	void( WINAPI * getProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getProgramInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( WINAPI * useProgram )( GLuint ) = nullptr;
	void( WINAPI * getActiveUniform )( GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * ) = nullptr;
	GLint( WINAPI * getUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * uniform1f )( GLint, GLfloat ) = nullptr;
	void( WINAPI * uniform3f )( GLint, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform4f )( GLint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * activeTexture )( GLenum ) = nullptr;
	void( WINAPI * texImage2DMultisample )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean ) = nullptr;
	void( WINAPI * texImage3D )( GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * blitFramebuffer )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * disableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * uniform2f )( GLint, GLfloat, GLfloat ) = nullptr;  // lane VKBLOOM1: the bloom chain's GL reference
	void( WINAPI * framebufferTextureLayer )( GLenum, GLenum, GLuint, GLint, GLint ) = nullptr;
};
Fns f;

template <class P> bool load( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	return p != nullptr;
}

bool loadAll()
{
	return load( f.createShader, "glCreateShader" ) && load( f.shaderSource, "glShaderSource" )
		&& load( f.compileShader, "glCompileShader" ) && load( f.getShaderiv, "glGetShaderiv" )
		&& load( f.getShaderInfoLog, "glGetShaderInfoLog" ) && load( f.createProgram, "glCreateProgram" )
		&& load( f.attachShader, "glAttachShader" ) && load( f.linkProgram, "glLinkProgram" )
		&& load( f.getProgramiv, "glGetProgramiv" ) && load( f.getProgramInfoLog, "glGetProgramInfoLog" )
		&& load( f.useProgram, "glUseProgram" ) && load( f.getActiveUniform, "glGetActiveUniform" )
		&& load( f.getUniformLocation, "glGetUniformLocation" ) && load( f.uniform1i, "glUniform1i" )
		&& load( f.uniform1f, "glUniform1f" ) && load( f.uniform3f, "glUniform3f" ) && load( f.uniform4f, "glUniform4f" )
		&& load( f.activeTexture, "glActiveTexture" ) && load( f.texImage2DMultisample, "glTexImage2DMultisample" )
		&& load( f.texImage3D, "glTexImage3D" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTexture2D, "glFramebufferTexture2D" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" ) && load( f.blitFramebuffer, "glBlitFramebuffer" )
		&& load( f.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( f.disableVertexAttribArray, "glDisableVertexAttribArray" )
		&& load( f.vertexAttribPointer, "glVertexAttribPointer" ) && load( f.uniform2f, "glUniform2f" )
		&& load( f.framebufferTextureLayer, "glFramebufferTextureLayer" );
}

std::string readFile( const std::string & p )
{
	std::ifstream in( p, std::ios::binary );
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

GLuint shader( GLenum kind, const std::string & src, const char * tag )
{
	const GLuint s = f.createShader( kind );
	const char * p = src.c_str();
	f.shaderSource( s, 1, &p, nullptr );
	f.compileShader( s );
	GLint ok = 0;
	f.getShaderiv( s, kCompiled, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getShaderInfoLog( s, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "TONEVIEW compile failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs, const char * tag )
{
	const GLuint a = shader( kVert, vs, tag ), b = shader( kFrag, fs, tag );
	if ( !a || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, kLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "TONEVIEW link failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return p;
}

void u1i( GLuint p, const char * n, int v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform1i( l, v );
}
void u1f( GLuint p, const char * n, float v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform1f( l, v );
}
void u2f( GLuint p, const char * n, float x, float y )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform2f( l, x, y );
}
void u3f( GLuint p, const char * n, const float * v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform3f( l, v[0], v[1], v[2] );
}
void u4f( GLuint p, const char * n, const float * v )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform4f( l, v[0], v[1], v[2], v[3] );
}

GLuint tex2d( int w, int h, GLenum ifmt, GLenum fmt, GLenum typ, const void * data, bool linear )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, h, 0, fmt, typ, data );
	const GLint flt = linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClamp );
	glTexParameteri( GL_TEXTURE_2D, kMaxLevel, 0 );
	return t;
}

GLuint msTex( int w, int h, GLenum ifmt, int s )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( kT2DMS, t );
	f.texImage2DMultisample( kT2DMS, s, ifmt, w, h, GL_TRUE );
	glBindTexture( kT2DMS, 0 );
	return t;
}

GLuint fbo( GLenum colKind, GLuint col, GLuint ds )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( kFb, b );
	f.framebufferTexture2D( kFb, kCol0, colKind, col, 0 );
	if ( ds )
		f.framebufferTexture2D( kFb, kDsAtt, kT2DMS, ds, 0 );
	if ( f.checkFramebufferStatus( kFb ) != kComplete ) {
		std::printf( "TONEVIEW framebuffer incomplete\n" );
		return 0;
	}
	return b;
}

void bindUnit( int unit, GLenum kind, GLuint t )
{
	f.activeTexture( kTex0 + GLenum( unit ) );
	glBindTexture( kind, t );
}

const char * kTriVs = "#version 410 core\n"
	"void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); "
	"gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";
// the frame per sample: colour, depth (gl_FragDepth) and, one draw a value, the stencil (REPLACE)
const char * kGenFs = "#version 410 core\n"
	"uniform sampler2D genData; uniform usampler2D genSten; uniform sampler2D genDepth; uniform int genS; uniform int genV;\n"
	"out vec4 o;\n"
	"void main() { ivec2 q = ivec2( int( gl_FragCoord.x ) * genS + gl_SampleID, int( gl_FragCoord.y ) );\n"
	"  if ( int( texelFetch( genSten, q, 0 ).r ) != genV ) discard;\n"
	"  o = texelFetch( genData, q, 0 ); gl_FragDepth = texelFetch( genDepth, q, 0 ).r; }\n";
const char * kUnpackFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ); }\n";
// lane VKFAR2: the fog bloom's R8 target per sample (k / 255 values, so the 8-bit store is exact)
const char * kFogFs = "#version 410 core\n"
	"uniform sampler2D fogData; uniform int genS; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( int( gl_FragCoord.x ) * genS + gl_SampleID, int( gl_FragCoord.y ) );\n"
	"  o = vec4( texelFetch( fogData, q, 0 ).r, 0.0, 0.0, 1.0 ); }\n";

struct Job
{
	const char * name;
	bool splitSky, sorted;
	int cellOn, van, lut, measure, dither, seed, red;
	float E, expo, adapted, cine[3], tint[4], wb[3];
};

struct Verdict
{
	size_t diff = 0, total = 0;
	int maxd = 0;
	const char * word() const { return diff == 0 ? "EXACT" : maxd <= 1 ? "BOUND" : "FAIL"; }
	bool ok() const { return maxd <= 1 && total > 0; }
};

Verdict compare( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	Verdict v;
	v.total = a.size();
	if ( a.size() != b.size() || a.empty() ) {
		v.maxd = 255;
		v.diff = std::max( a.size(), b.size() );
		return v;
	}
	for ( size_t i = 0; i < a.size(); i++ ) {
		const int d = std::abs( int( a[i] ) - int( b[i] ) );
		v.diff += d != 0;
		v.maxd = std::max( v.maxd, d );
	}
	return v;
}
}  // namespace tv

int main( int argc, char ** argv )
{
	using namespace tv;
	if ( argc < 2 ) {
		std::printf( "usage: toneview <glsl dir> [W H]\nTONEVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	const int W = argc > 2 ? std::atoi( argv[2] ) : 640, H = argc > 3 ? std::atoi( argv[3] ) : 360;
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwtoneview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwtoneview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwtoneview", "tone view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };	// window, GL, double, full accel, RGBA8, D24S8, 4 samples
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "TONEVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
		return 2;
	}
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, fmt, sizeof( got ), &got );
	SetPixelFormat( dc, fmt, &got );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() ) {
		std::printf( "TONEVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "view %dx%d x%d, GL on %s\n", W, H, S, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );

	const GLuint pGen = program( kTriVs, kGenFs, "gen" ), pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	const GLuint pFog = program( kTriVs, kFogFs, "fog" );
	const GLuint pHdr = program( readFile( dir + "/cell_hdr.vert.glsl" ), readFile( dir + "/cell_hdr.frag.glsl" ), "cell_hdr" );
	if ( !pGen || !pUnpack || !pFog || !pHdr ) {
		std::printf( "TONEVIEW GATE FAILED (GL programs)\n" );
		return 2;
	}
	// lane VKBLOOM1: the bloom chain's and the measure's GL programs (cellbloom.cpp's passes, cellhdr.cpp's quarter draw)
	const std::string aoVs = readFile( dir + "/cell_ao.vert.glsl" );
	const GLuint pSrc = program( aoVs, readFile( dir + "/cell_bloom_src.frag.glsl" ), "cell_bloom_src" );
	const GLuint pDown = program( aoVs, readFile( dir + "/cell_bloom_down.frag.glsl" ), "cell_bloom_down" );
	const GLuint pHold = program( aoVs, readFile( dir + "/cell_bloom_hold.frag.glsl" ), "cell_bloom_hold" );
	const GLuint pUp = program( aoVs, readFile( dir + "/cell_bloom_up.frag.glsl" ), "cell_bloom_up" );
	const GLuint pComb = program( aoVs, readFile( dir + "/cell_bloom_comb.frag.glsl" ), "cell_bloom_comb" );
	const GLuint pMeas = program( aoVs, readFile( dir + "/cell_is_measure_q.frag.glsl" ), "cell_is_measure_q" );
	if ( !pSrc || !pDown || !pHold || !pUp || !pComb || !pMeas ) {
		std::printf( "TONEVIEW GATE FAILED (GL bloom / measure programs)\n" );
		return 2;
	}
	// every sampler on a unit of its own (two sampler types on one unit refuse the draw); the read ones on fixed units
	auto units = [&]( GLuint p, bool hdr ) {
		f.useProgram( p );
		GLint n = 0;
		f.getProgramiv( p, kActiveUnis, &n );
		int next = 20;
		for ( GLint i = 0; i < n; i++ ) {
			char name[256] = {};
			GLint size = 0;
			GLenum typ = 0;
			f.getActiveUniform( p, GLuint( i ), 256, nullptr, &size, &typ, name );
			const bool sampler = ( typ >= 0x8B5D && typ <= 0x8B62 ) || ( typ >= 0x8DC1 && typ <= 0x8DD8 ) || typ == 0x900C
				|| typ == 0x900D || ( typ >= 0x9108 && typ <= 0x910A );
			if ( !sampler )
				continue;
			std::string nm( name );
			nm = nm.substr( 0, nm.find( '[' ) );
			int unit = nm == "hdrSrc" ? 8 : nm == "hdrStencil" ? 9 : nm == "cellIsBloom" ? 11 : nm == "cellIsLut" ? 12 : -1;
			if ( unit < 0 && hdr )
				unit = nm == "cellIsFoBloom" ? 13 : nm == "cellIsFoBloomComb" ? 14 : nm == "hdrBackdrop" ? 15 : -1;
			if ( unit < 0 && !hdr )	// the chain's passes: kUnitA 8, kUnitB 9 (cellbloom.cpp)
				unit = nm == "bloomSrc" || nm == "bloomRing" || ( p == pComb && nm == "cellIsFoBloom" ) ? 8 : nm == "bloomMip" ? 9 : -1;
			u1i( p, nm.c_str(), unit < 0 ? next++ : unit );
		}
	};
	units( pHdr, true );
	for ( GLuint p : { pSrc, pDown, pHold, pUp, pComb, pMeas } )
		units( p, false );

	// the frame's data, per sample (column x * S + s), seeded
	std::mt19937 rng( 29 );
	std::normal_distribution<float> nd( -0.5f, 2.0f ), nb( -2.5f, 1.5f ), nl( 0.0f, 6.0f );
	std::uniform_real_distribution<float> ur( 0.0f, 1.0f );
	const int WS = W * S;
	std::vector<float> data( size_t( WS ) * H * 4 ), depth( size_t( WS ) * H );
	std::vector<std::uint8_t> sten( size_t( WS ) * H );
	const std::uint8_t stenVals[6] = { 0, 1, 2, 3, 5, 7 };
	const float stenP[6] = { 0.30f, 0.40f, 0.08f, 0.07f, 0.10f, 0.05f };
	const float alphas[6] = { 0.0f, 0.0005f, 0.25f, 0.6f, 1.0f, 1.3f };
	for ( size_t i = 0; i < sten.size(); i++ ) {
		float r = ur( rng ), acc = 0.0f;
		int k = 5;
		for ( int j = 0; j < 6; j++ ) {
			acc += stenP[j];
			if ( r < acc ) {
				k = j;
				break;
			}
		}
		sten[i] = stenVals[k];
		const bool isFar = ur( rng ) < 0.3f;
		depth[i] = isFar ? 1.0f : 0.2f + 0.7f * ur( rng );
		const float a = alphas[std::min( 5, int( ur( rng ) * 6.0f ) )];
		for ( int c = 0; c < 3; c++ ) {
			float v = std::exp( nd( rng ) );
			if ( ur( rng ) < 0.02f )
				v = -v;
			data[i * 4 + size_t( c )] = isFar ? v * std::min( std::max( a, 0.0f ), 1.0f ) : v;	// over the sky: premultiplied
		}
		data[i * 4 + 3] = a;
	}
	const int mw = ( W + 3 ) / 4, mh = ( H + 3 ) / 4;
	std::vector<float> mb( size_t( mw ) * mh * 3 );
	for ( float & v : mb )
		v = std::exp( nb( rng ) );
	std::vector<std::uint8_t> lut( 16 * 16 * 16 * 3 );
	for ( int b = 0; b < 16; b++ )
		for ( int g = 0; g < 16; g++ )
			for ( int r = 0; r < 16; r++ )
				for ( int c = 0; c < 3; c++ ) {
					const float x = float( c == 0 ? r : c == 1 ? g : b ) / 15.0f;
					const float v = std::pow( x, 1.1f ) * 255.0f + nl( rng );
					lut[size_t( ( ( b * 16 + g ) * 16 + r ) * 3 + c )] = std::uint8_t( std::lround( std::min( std::max( v, 0.0f ), 255.0f ) ) );
				}
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	const GLuint tData = tex2d( WS, H, kRgba32f, GL_RGBA, GL_FLOAT, data.data(), false );
	const GLuint tSten = tex2d( WS, H, kR8ui, kRedInt, GL_UNSIGNED_BYTE, sten.data(), false );
	const GLuint tDepth = tex2d( WS, H, kR32f, kRed, GL_FLOAT, depth.data(), false );
	const GLuint tMeasure = tex2d( mw, mh, kRgb32f, GL_RGB, GL_FLOAT, mb.data(), true );	// as celllights.cpp uploads it
	GLuint tLut = 0;
	glGenTextures( 1, &tLut );
	glBindTexture( kT3D, tLut );
	glTexParameteri( kT3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( kT3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( kT3D, GL_TEXTURE_WRAP_S, kClamp );
	glTexParameteri( kT3D, GL_TEXTURE_WRAP_T, kClamp );
	glTexParameteri( kT3D, kWrapR, kClamp );
	f.texImage3D( kT3D, 0, GLint( kRgb8 ), 16, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, lut.data() );
	glBindTexture( kT3D, 0 );
	// the reference target (GL's own, the view's format) and the per-sample read-back target
	const GLuint refCol = msTex( W, H, kRgba8, S ), refDs = msTex( W, H, kD24s8, S );
	const GLuint fbRef = fbo( kT2DMS, refCol, refDs );
	const GLuint unp = tex2d( WS, H, kRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0 );
	if ( !fbRef || !fbUnp ) {
		std::printf( "TONEVIEW GATE FAILED (GL targets)\n" );
		return 2;
	}

	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint8_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( kFb, fbUnp );
		glViewport( 0, 0, WS, H );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_STENCIL_TEST );
		glDisable( GL_BLEND );
		f.useProgram( pUnpack );
		bindUnit( 8, kT2DMS, msColor );
		u1i( pUnpack, "src", 8 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, GL_UNSIGNED_BYTE, out.data() );
		return out;
	};
	// the frame drawn by GL into fb (colour, depth and stencil per sample)
	auto genFrame = [&]( GLuint fb ) {
		f.bindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glDisable( GL_SCISSOR_TEST );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0, 0, 0, 0 );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		glEnable( GL_STENCIL_TEST );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		f.useProgram( pGen );
		bindUnit( 8, GL_TEXTURE_2D, tData );
		bindUnit( 9, GL_TEXTURE_2D, tSten );
		bindUnit( 10, GL_TEXTURE_2D, tDepth );
		u1i( pGen, "genData", 8 );
		u1i( pGen, "genSten", 9 );
		u1i( pGen, "genDepth", 10 );
		u1i( pGen, "genS", S );
		for ( std::uint8_t v : stenVals ) {
			glStencilFunc( GL_ALWAYS, v, 0xFF );
			u1i( pGen, "genV", v );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
		}
		glDisable( GL_STENCIL_TEST );
		glDepthFunc( GL_LESS );
		glDisable( GL_DEPTH_TEST );
	};
	// what GL drew into the view before the tone map (the backdrop): every sample the same in a band
	auto backdrop = [&]( GLuint fb ) {
		f.bindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glClearColor( 0.2f, 0.3f, 0.5f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );
		glEnable( GL_SCISSOR_TEST );
		for ( int i = 0; i < 8; i++ ) {
			glScissor( 0, i * H / 8, W, H / 16 + 1 );
			glClearColor( i / 8.0f, 1.0f - i / 8.0f, 0.5f, i % 2 ? 1.0f : 0.5f );
			glClear( GL_COLOR_BUFFER_BIT );
		}
		glDisable( GL_SCISSOR_TEST );
	};

	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "TONEVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) ) {
		std::printf( "TONEVIEW GATE FAILED (toneFrame: %s)\n", why.c_str() );
		return 2;
	}
	const GLuint fbFrame = fbo( kT2DMS, fc, fds );
	if ( !fbFrame ) {
		std::printf( "TONEVIEW GATE FAILED (the shared frame's FBO)\n" );
		return 2;
	}
	std::uint64_t gen = 0;
	// lane VKBLOOM1: the bloom's job and the measure's ask the next tone passes carry (cellhdr.cpp's flags 7 and 11)
	const WwVkPresent::ToneBloom * bloomNow = nullptr;
	float bloomFo[3] = {};
	bool measNow = false;
	const int qw = std::max( 1, W >> 2 ), qh = std::max( 1, H >> 2 );
	// lane VKFAR2: the fog bloom target the next passes read (0: off); GL binds the same shared name as cellFogBloom
	std::uintptr_t vkFogKey = 0;
	unsigned vkFogTex = 0;
	int vkFogRed = 0;
	GLuint glFogNow = 0;

	// the frame into the shared frame, its depth into the view, the backdrop; the tone pass's job for it
	auto toneOf = [&]( const Job & j, bool splitSky, GLuint vfb ) {
		genFrame( fbFrame );
		f.bindFramebuffer( kRead, fbFrame );
		f.bindFramebuffer( kDraw, vfb );
		f.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_DEPTH_BUFFER_BIT, GL_NEAREST );	// cellhdr.cpp's depth blit
		backdrop( vfb );
		f.bindFramebuffer( kFb, vfb );
		glDisable( GL_SCISSOR_TEST );
		WwVkPresent::ToneRun t;
		for ( int c = 0; c < 3; c++ ) {
			t.job.cineE[c] = j.cine[c];
			t.job.wbExposure[c] = j.wb[c];
		}
		t.job.cineE[3] = j.E;
		for ( int c = 0; c < 4; c++ )
			t.job.tint[c] = j.tint[c];
		t.job.wbExposure[3] = j.expo;
		t.job.bloomRect[2] = 1.0f / float( W );
		t.job.bloomRect[3] = 1.0f / float( H );
		t.job.fo[3] = j.adapted;
		t.job.ints[0] = ( j.red & 15 ) | ( j.van ? 1 << 4 : 0 ) | ( j.lut ? 1 << 5 : 0 ) | ( j.measure ? 1 << 6 : 0 ) | ( j.cellOn ? 1 << 10 : 0 );
		if ( bloomNow ) {
			t.job.ints[0] |= ( 1 << 7 ) | ( 1 << 11 );
			t.bloom = bloomNow;
			for ( int c = 0; c < 3; c++ )
				t.job.fo[c] = bloomFo[c];
		}
		if ( measNow ) {
			t.measQw = qw;
			t.measQh = qh;
		}
		t.job.ints[1] = j.dither;
		t.job.ints[2] = j.seed;
		t.splitSky = splitSky;
		t.sorted = j.sorted;
		t.lut = lut.data();
		t.lutGen = 1;
		t.measure = tMeasure;
		t.measureW = mw;
		t.measureH = mh;
		t.measureGen = ++gen;	// copied each run (the GPU blur's case)
		t.vp[2] = W;
		t.vp[3] = H;
		t.drawFb = t.readFb = vfb;
		return t;
	};
	auto runVk = [&]( const Job & j, bool splitSky, std::vector<std::uint8_t> & out ) -> bool {
		if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
			std::printf( "TONEVIEW %s: the view: %s\n", j.name, why.c_str() );
			return false;
		}
		WwVkPresent::ToneRun t = toneOf( j, splitSky, WwVkPresent::viewFbo() );
		if ( vkFogKey ) {	// lane VKFAR2: cellhdr.cpp's flag 9 with the fog bloom's shared target
			t.job.ints[0] |= 1 << 9;
			t.fogKey = vkFogKey;
			t.fogTex = vkFogTex;
			t.fogRed = vkFogRed;
		}
		if ( !WwVkPresent::toneRun( t, why ) ) {
			std::printf( "TONEVIEW %s: toneRun refused: %s\n", j.name, why.c_str() );
			return false;
		}
		View & V = view();
		out = readSamples( V.slot[V.cur].colorTex );
		if ( !WwVkPresent::present( wnd, W, H, false, glProc, why ) ) {
			std::printf( "TONEVIEW %s: present: %s\n", j.name, why.c_str() );
			return false;
		}
		return true;
	};
	/* lane VKBLOOM1: GL's chain already drawn (top, combine): the tone quads read it (wwCellBloomFinal) and mode 3 adds the
	 * glow over the backdrop (cellhdr.cpp backdropCopy: the view resolved into RGBA16F) */
	struct GlBloom
	{
		GLuint top, comb;
		float radius, weight, dispersion, ec, fogCap;
	};
	const GlBloom * glBloomNow = nullptr;
	const GLuint backTex = tex2d( W, H, kRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbBack = fbo( GL_TEXTURE_2D, backTex, 0 );
	// cellhdr.cpp wwCellHdrEnd's GL quads (the stencil read in the shader), into GL's own target
	auto runGl = [&]( const Job & j, std::vector<std::uint8_t> & out ) {
		genFrame( fbFrame );
		f.bindFramebuffer( kRead, fbFrame );
		f.bindFramebuffer( kDraw, fbRef );
		f.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
		backdrop( fbRef );
		f.bindFramebuffer( kFb, fbRef );
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		f.useProgram( pHdr );
		bindUnit( 8, kT2DMS, fc );
		bindUnit( 9, kT2DMS, fds );
		bindUnit( 11, GL_TEXTURE_2D, j.measure ? tMeasure : 0 );
		bindUnit( 12, kT3D, j.lut ? tLut : 0 );
		u1i( pHdr, "hdrStencilTest", 0 );
		u1i( pHdr, "hdrBackdropOn", 0 );
		u1i( pHdr, "wwDitherOn", j.dither );
		u1i( pHdr, "wwDitherSeed", j.seed );
		u1i( pHdr, "cellOn", j.cellOn );
		u1i( pHdr, "cellIsOn", j.cellOn );
		u1f( pHdr, "cellIsExposure", j.expo );
		u1f( pHdr, "cellIsE", j.E );
		u1f( pHdr, "cellIsAdapted", j.adapted );
		u3f( pHdr, "cellIsCine", j.cine );
		u4f( pHdr, "cellIsTint", j.tint );
		u3f( pHdr, "cellIsWbD", j.wb );
		u1i( pHdr, "cellIsLutOn", j.lut );
		u1i( pHdr, "cellIsRed", j.red );
		u1i( pHdr, "cellIsVanCurve", j.van );
		u1i( pHdr, "cellIsBloomOn", j.measure );
		const float rect[4] = { 0.0f, 0.0f, 1.0f / float( W ), 1.0f / float( H ) };
		u4f( pHdr, "cellIsBloomRect", rect );
		const GlBloom * bg = glBloomNow;
		u1i( pHdr, "cellIsFoBloomOn", bg ? 1 : 0 );
		u1i( pHdr, "cellIsFoBloomCombOn", bg ? 1 : 0 );
		if ( bg ) {
			bindUnit( 13, GL_TEXTURE_2D, bg->top );
			bindUnit( 14, GL_TEXTURE_2D, bg->comb );
			const float fp[4] = { bg->radius, bg->weight, bg->dispersion, 1.0f };
			u4f( pHdr, "cellIsFoBloomParams", fp );
			u1f( pHdr, "cellIsFoBloomEc", bg->ec );
			u1f( pHdr, "cellIsFoBloomFogCap", bg->fogCap );
			u2f( pHdr, "cellIsFoBloomFrame", float( W ), float( H ) );
		}
		// lane VKFAR2: cellfogbloom.cpp wwCellFogBloomFinal (the target on its own unit, a sampler2DMS)
		bindUnit( 16, kT2DMS, glFogNow );
		u1i( pHdr, "cellFogBloom", 16 );
		u1i( pHdr, "cellFogBloomOn", glFogNow ? 1 : 0 );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, quad );
		auto draw = [&]( int mode ) {
			u1i( pHdr, "hdrMode", mode );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx );
		};
		auto pick = [&]( int test, int ref ) {
			u1i( pHdr, "hdrStencilTest", test );
			u1i( pHdr, "hdrStencilRef", ref );
		};
		if ( j.sorted )
			pick( 1, 1 );
		if ( j.splitSky ) {
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_GREATER );
		}
		draw( 0 );
		if ( j.splitSky ) {
			glDepthFunc( GL_LEQUAL );
			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
			draw( 2 );
			glDisable( GL_BLEND );
			glDisable( GL_DEPTH_TEST );
			glDepthFunc( GL_LESS );
		}
		if ( j.sorted ) {
			pick( 1, 2 );
			draw( 1 );
		}
		if ( bg && j.sorted ) {	// cellhdr.cpp's mode-3 tail
			f.bindFramebuffer( kRead, fbRef );
			f.bindFramebuffer( kDraw, fbBack );
			f.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
			f.bindFramebuffer( kFb, fbRef );
			bindUnit( 15, GL_TEXTURE_2D, backTex );
			u1i( pHdr, "hdrBackdropOn", 1 );
			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE, GL_ONE );
			pick( 2, 1 );
			draw( 3 );
			if ( j.splitSky ) {
				pick( 1, 1 );
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				draw( 3 );
				glDisable( GL_DEPTH_TEST );
				glDepthFunc( GL_LESS );
			}
			glDisable( GL_BLEND );
			u1i( pHdr, "hdrBackdropOn", 0 );
		}
		f.disableVertexAttribArray( 0 );
		glDepthMask( GL_TRUE );
		out = readSamples( refCol );
		return glGetError() == GL_NO_ERROR;
	};

	const Job jobs[3] = {
		{ "split", true, true, 1, 0, 1, 1, 1, 7, 0, 0.02f, 0.37f, 0.3f, { 1.1f, 1.0f, 1.05f }, { 0.1f, 1.0f, 0.9f, 0.8f }, { 0, 0, 0 } },
		{ "skyin", false, true, 1, 1, 0, 0, 0, 0, 4, 0.4f, 0.37f, 0.3f, { 1.1f, 1.0f, 1.05f }, { 0.1f, 1.0f, 0.9f, 0.8f },
			{ 0.1f, -0.05f, 0.2f } },
		{ "unsorted", false, false, 0, 0, 1, 1, 1, 12, 0, 0.02f, 0.5f, 0.25f, { 0.9f, 1.1f, 0.95f }, { 0.3f, 0.8f, 1.0f, 0.7f },
			{ 0, 0, 0 } },
	};
	bool ok = true;
	std::vector<std::uint8_t> refSplit;
	for ( const Job & j : jobs ) {
		std::vector<std::uint8_t> vk, ref;
		if ( !runVk( j, j.splitSky, vk ) || !runGl( j, ref ) ) {
			std::printf( "TONEVIEW %s FAIL (a run failed)\n", j.name );
			ok = false;
			continue;
		}
		const Verdict v = compare( vk, ref );
		std::printf( "TONEVIEW %s %s (%zu of %zu bytes differ, max %d; splitSky %d, sorted %d)\n", j.name, v.word(), v.diff, v.total,
			v.maxd, int( j.splitSky ), int( j.sorted ) );
		ok = ok && v.ok();
		if ( &j == &jobs[0] )
			refSplit = ref;
	}
	// the red: the Vulkan pass without the sky split against the split reference
	{
		std::vector<std::uint8_t> vk;
		const bool ran = runVk( jobs[0], false, vk );
		const Verdict v = compare( vk, refSplit );
		const bool redFails = !ran || !v.ok();
		std::printf( "TONEVIEW RED %s (splitSky off on Vulkan: %zu bytes differ, max %d; must FAIL)\n", redFails ? "FAIL" : v.word(),
			v.diff, v.maxd );
		ok = ok && redFails;
	}
	/* lane VKFAR2: the fog bloom on (cellhdr.cpp flag 9): the tone pass reads fogBloomRun's shared R8 target per sample at
	 * binding 5; GL's quads bind the same name as cellFogBloom (wwCellFogBloomFinal). Two seeded fills (A: 40 % clear air,
	 * B: dense), four cases (split A, unsorted A, skyin A, split B: 3 + 1 + 1 + 3 draws). Bound as the jobs above (BOUND: no
	 * byte off by more than 1). Armed: GL's split with fog A must differ from GL's split without it. Red
	 * (WW_VKVIEW_TONE_FOGBLOOM_RED=1, ToneRun::fogRed): the pass ignores the target, must FAIL. */
	{
		static char fogKeyTag;
		const std::uintptr_t fogKey = reinterpret_cast<std::uintptr_t>( &fogKeyTag );
		unsigned fogTex = 0;
		GLuint fbFog = 0;
		if ( !WwVkPresent::sharedTarget( fogKey, 0, W, H, 1, kGlR8, fogTex, why, 1, S ) || !( fbFog = fbo( kT2DMS, fogTex, 0 ) ) ) {
			std::printf( "TONEVIEW FOG FAIL (the shared R8 target: %s)\n", why.c_str() );
			ok = false;
		} else {
			std::mt19937 fr( 31 );	// its own seed: the arms after this one keep their data
			std::vector<float> fogA( size_t( WS ) * H ), fogB( size_t( WS ) * H );
			for ( size_t i = 0; i < fogA.size(); i++ ) {
				fogA[i] = ur( fr ) < 0.4f ? 0.0f : float( int( ur( fr ) * 256.0f ) & 255 ) / 255.0f;
				fogB[i] = float( 128 + ( int( ur( fr ) * 128.0f ) & 127 ) ) / 255.0f;
			}
			const GLuint tFogA = tex2d( WS, H, kR32f, kRed, GL_FLOAT, fogA.data(), false );
			const GLuint tFogB = tex2d( WS, H, kR32f, kRed, GL_FLOAT, fogB.data(), false );
			auto fill = [&]( GLuint src ) {	// what fogBloomRun leaves in the target
				f.bindFramebuffer( kFb, fbFog );
				glViewport( 0, 0, W, H );
				glDisable( GL_DEPTH_TEST );
				glDisable( GL_STENCIL_TEST );
				glDisable( GL_BLEND );
				glDisable( GL_SCISSOR_TEST );
				f.useProgram( pFog );
				bindUnit( 8, GL_TEXTURE_2D, src );
				u1i( pFog, "fogData", 8 );
				u1i( pFog, "genS", S );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
			};
			struct FogCase
			{
				const char * name;
				const Job * job;
				GLuint data;
			};
			const FogCase cases[4] = { { "fog split A", &jobs[0], tFogA }, { "fog unsorted A", &jobs[2], tFogA },
				{ "fog skyin A", &jobs[1], tFogA }, { "fog split B", &jobs[0], tFogB } };
			std::vector<std::uint8_t> refFogSplit;
			for ( const FogCase & c : cases ) {
				std::vector<std::uint8_t> vk, ref;
				fill( c.data );
				vkFogKey = fogKey;
				vkFogTex = fogTex;
				glFogNow = fogTex;
				const bool ran = runVk( *c.job, c.job->splitSky, vk ) && runGl( *c.job, ref );
				const Verdict v = compare( vk, ref );
				std::printf( "TONEVIEW %s %s (%zu of %zu bytes differ, max %d)\n", c.name, ran ? v.word() : "FAIL (a run failed)", v.diff,
					v.total, v.maxd );
				ok = ok && ran && v.ok();
				if ( &c == &cases[0] )
					refFogSplit = ref;
			}
			const Verdict armed = compare( refFogSplit, refSplit );
			std::printf( "TONEVIEW FOG armed %s (GL split with fog A vs without: %zu bytes differ; must differ)\n",
				armed.diff > 0 ? "yes" : "NO", armed.diff );
			ok = ok && armed.diff > 0;
			{	// the red: the pass ignores the target (gain 1) against GL's split with fog A
				std::vector<std::uint8_t> vk;
				fill( tFogA );
				vkFogRed = 1;
				const bool ran = runVk( jobs[0], true, vk );
				vkFogRed = 0;
				const Verdict v = compare( vk, refFogSplit );
				const bool redFails = !ran || !v.ok();
				std::printf( "TONEVIEW FOG RED %s (the tone pass ignores the fog bloom target: %zu bytes differ, max %d; must FAIL)\n",
					redFails ? "FAIL" : v.word(), v.diff, v.maxd );
				ok = ok && redFails;
			}
			vkFogKey = 0;
			vkFogTex = 0;
			glFogNow = 0;
			bindUnit( 16, kT2DMS, 0 );
		}
		WwVkPresent::sharedTargetDrop( fogKey );	// fbFog stays (the harness frees no FBO; the context ends with it)
	}
	/* lane VKBLOOM1: the Fusion Shaders bloom (source, down / hold / up, combine) and the exposure measure in the tone pass
	 * on the present device (toneRun with the job cellbloom.cpp fills, flags 7 + 11, the measure's quarter ask) against
	 * GL: cellbloom.cpp's passes (the tree's cell_bloom_*.frag) on the same frame, the tone quads reading the combine,
	 * backdropCopy + mode 3, and cellhdr.cpp's quarter draw (cell_is_measure_q.frag). Bound stated before the first run:
	 * the measure EXACT (float bits); the view BOUND (no byte off by more than 1: the backdrop is the view resolved into
	 * RGBA16F on both, by bloom_back.hlsl on Vulkan and a blit on GL; the source is BOUND 1 fp16 ulp, tests/vkframe/srccomb.sh). Reds (must FAIL):
	 * bloomRedTest 1 (mode 3 not drawn), 3 (mode 3 without the backdrop), 2 (the measure not dispatched; its target
	 * cleared by GL first). */
	struct BloomCase
	{
		const char * name;
		int job;
		float thr, knee, ec, weight, radius, dispersion, exposure;
		int red;
		bool hold, measure;
	};
	const BloomCase bcases[2] = {
		{ "bloom_split", 0, 0.0f, 0.0f, 0.6f, 0.45f, 1.0f, 0.15f, 0.8f, 0, false, true },
		{ "bloom_add", 1, 1.0f, 0.5f, 0.0f, 0.3f, 1.6f, 0.15f, 1.0f, 1, true, true },
	};
	int bdims[WwVkPresent::kToneBloomMips][2] = {};
	int bmips = 0;
	{	// cellbloom.cpp wwBloomPlanMips, 6 levels (the row's default)
		int bw = W, bh = H;
		for ( int l = 0; l < 6 && ( bw > 1 || bh > 1 ); l++ ) {
			bw = std::max( ( bw + 1 ) / 2, 1 );
			bh = std::max( ( bh + 1 ) / 2, 1 );
			bdims[bmips][0] = bw;
			bdims[bmips][1] = bh;
			bmips++;
		}
	}
	auto bloomJob = [&]( const BloomCase & c ) {
		WwVkPresent::ToneBloom b;
		b.mips = bmips;
		std::memcpy( b.dims, bdims, sizeof( bdims ) );
		b.thr = c.thr;
		b.knee = c.knee;
		b.red = c.red;
		b.hold = c.hold;
		b.holdK = 3;
		b.holdN = 1;
		b.radius = c.radius;
		b.dispersion = c.dispersion;
		for ( int l = 0; l + 1 < bmips; l++ ) {	// bandWeights' shape (bloom_gl_ref.py's)
			b.srcW[l] = 0.55f + 0.05f * float( l );
			b.accW[l] = l + 2 == bmips ? 0.7f : 1.0f;
		}
		b.exposure = c.exposure;
		b.on = true;
		b.impulseX = float( W / 2 );
		b.impulseY = float( H / 2 );
		return b;
	};
	// GL's chain on the frame now in the shared frame (cellbloom.cpp steps 1-3 and the combine)
	const GLuint bMs = msTex( W, H, kRgba16f, S );
	const GLuint fbBms = fbo( kT2DMS, bMs, fds );
	const GLuint bSrc = tex2d( W, H, kRgba16f, GL_RGBA, GL_FLOAT, nullptr, true );
	const GLuint fbBres = fbo( GL_TEXTURE_2D, bSrc, 0 );
	GLuint bMip[WwVkPresent::kToneBloomMips] = {}, bUp[WwVkPresent::kToneBloomMips] = {};
	for ( int i = 0; i < bmips; i++ ) {
		bMip[i] = tex2d( bdims[i][0], bdims[i][1], kRgba16f, GL_RGBA, GL_FLOAT, nullptr, true );
		bUp[i] = tex2d( bdims[i][0], bdims[i][1], kRgba16f, GL_RGBA, GL_FLOAT, nullptr, true );
	}
	const GLuint bComb = tex2d( W, H, kRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint bMeas = tex2d( qw, qh, kRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbMeas = fbo( GL_TEXTURE_2D, bMeas, 0 );
	constexpr GLenum kArr = 0x8C1A;
	GLuint bRing = 0;
	glGenTextures( 1, &bRing );
	glBindTexture( kArr, bRing );
	f.texImage3D( kArr, 0, GLint( kRgba16f ), bdims[0][0], bdims[0][1], 8, 0, GL_RGBA, GL_FLOAT, nullptr );
	glTexParameteri( kArr, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( kArr, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( kArr, GL_TEXTURE_WRAP_S, kClamp );
	glTexParameteri( kArr, GL_TEXTURE_WRAP_T, kClamp );
	glBindTexture( kArr, 0 );
	GLuint fbChain = 0;
	f.genFramebuffers( 1, &fbChain );
	static const float bQuad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t bIdx[6] = { 0, 1, 2, 2, 1, 3 };
	auto quadDraw = [&]() {
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, bQuad );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, bIdx );
		f.disableVertexAttribArray( 0 );
	};
	auto chainTarget = [&]( GLuint t, int layer, int w, int h ) {
		f.bindFramebuffer( kFb, fbChain );
		if ( layer >= 0 )
			f.framebufferTextureLayer( kFb, kCol0, t, 0, layer );
		else
			f.framebufferTexture2D( kFb, kCol0, GL_TEXTURE_2D, t, 0 );
		glViewport( 0, 0, w, h );
	};
	auto chainGl = [&]( const BloomCase & c, const WwVkPresent::ToneBloom & b, GlBloom & out ) {
		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		glDisable( GL_BLEND );
		glDisable( GL_SCISSOR_TEST );
		// 1. the source: the cell-lit samples (stencil & 3 == 1), exposed, per sample; resolved
		f.bindFramebuffer( kFb, fbBms );
		glViewport( 0, 0, W, H );
		glClearColor( 0, 0, 0, 0 );
		glClear( GL_COLOR_BUFFER_BIT );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0 );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		glStencilFunc( GL_EQUAL, 1, 0x03 );
		f.useProgram( pSrc );
		bindUnit( 8, kT2DMS, fc );
		const float imp[3] = { b.impulseX, b.impulseY, 0.0f };
		u3f( pSrc, "bloomImpulse", imp );
		u1i( pSrc, "cellOn", 1 );
		u1i( pSrc, "cellIsOn", 1 );
		u1i( pSrc, "cellIsRed", 0 );
		u1f( pSrc, "cellIsExposure", c.exposure );
		quadDraw();
		glDisable( GL_STENCIL_TEST );
		glStencilMask( 0xFF );
		f.bindFramebuffer( kRead, fbBms );
		f.bindFramebuffer( kDraw, fbBres );
		f.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		// 2. down (level 0 into the ring's layer k and the hold's mean into mip 0 when held)
		for ( int i = 0; i < bmips; i++ ) {
			if ( i == 0 && b.hold )
				chainTarget( bRing, b.holdK, bdims[0][0], bdims[0][1] );
			else
				chainTarget( bMip[i], -1, bdims[i][0], bdims[i][1] );
			f.useProgram( pDown );
			bindUnit( 8, GL_TEXTURE_2D, i == 0 ? bSrc : bMip[i - 1] );
			u2f( pDown, "bloomDst", float( bdims[i][0] ), float( bdims[i][1] ) );
			const float dp[4] = { b.thr, b.knee, i == 0 ? 1.0f : 0.0f, 1.0f };
			u4f( pDown, "bloomParams", dp );
			u1i( pDown, "bloomRed", b.red );
			quadDraw();
			if ( i == 0 && b.hold ) {
				chainTarget( bMip[0], -1, bdims[0][0], bdims[0][1] );
				f.useProgram( pHold );
				bindUnit( 8, kArr, bRing );
				u1i( pHold, "bloomHoldK", b.holdK );
				u1i( pHold, "bloomHoldN", b.holdN );
				quadDraw();
				bindUnit( 8, kArr, 0 );
			}
		}
		// 3. up, from the deepest mip to level 0
		GLuint acc = bMip[bmips - 1];
		for ( int offset = bmips - 1; offset > 0; offset-- ) {
			const int level = offset - 1;
			chainTarget( bUp[level], -1, bdims[level][0], bdims[level][1] );
			f.useProgram( pUp );
			bindUnit( 8, GL_TEXTURE_2D, bMip[level] );
			bindUnit( 9, GL_TEXTURE_2D, acc );
			u2f( pUp, "bloomDst", float( bdims[level][0] ), float( bdims[level][1] ) );
			const float upp[4] = { b.radius, b.srcW[level], b.accW[level], b.dispersion };
			u4f( pUp, "bloomParams", upp );
			quadDraw();
			acc = bUp[level];
		}
		// the combine once per pixel (cell_bloom_comb.frag, lod 0), RGBA32F
		chainTarget( bComb, -1, W, H );
		f.useProgram( pComb );
		bindUnit( 8, GL_TEXTURE_2D, acc );
		const float cp[4] = { b.radius, c.weight, b.dispersion, 1.0f };
		u4f( pComb, "cellIsFoBloomParams", cp );
		u2f( pComb, "cellIsFoBloomFrame", float( W ), float( H ) );
		quadDraw();
		bindUnit( 8, GL_TEXTURE_2D, 0 );
		bindUnit( 9, GL_TEXTURE_2D, 0 );
		glDepthMask( GL_TRUE );
		out = GlBloom { acc, bComb, b.radius, c.weight, b.dispersion, c.ec, 2.0f };
		return glGetError() == GL_NO_ERROR;
	};
	// cellhdr.cpp's quarter draw of the measure on the frame now in the shared frame
	auto measureGl = [&]( std::vector<std::uint8_t> & out ) {
		f.bindFramebuffer( kFb, fbMeas );
		glViewport( 0, 0, qw, qh );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_STENCIL_TEST );
		glDisable( GL_BLEND );
		f.useProgram( pMeas );
		bindUnit( 8, kT2DMS, fc );
		bindUnit( 9, kT2DMS, fds );
		u1i( pMeas, "hdrSamples", S );
		quadDraw();
		out.assign( size_t( qw ) * qh * 16, 0 );
		glReadPixels( 0, 0, qw, qh, GL_RGBA, GL_FLOAT, out.data() );
		return glGetError() == GL_NO_ERROR;
	};
	// the measure the tone pass read back (toneMeasureTake, as cellhdr.cpp's next frame takes it)
	auto measureTake = [&]( std::vector<std::uint8_t> & out ) {
		const float * px = nullptr;
		int tqw = 0, tqh = 0;
		unsigned ttex = 0;
		int took = -1;
		for ( int k = 0; k < 4000 && took < 0; k++ ) {
			took = WwVkPresent::toneMeasureTake( px, tqw, tqh, ttex );
			if ( took < 0 )
				Sleep( 1 );
		}
		if ( took <= 0 || !px || tqw != qw || tqh != qh )
			return false;
		out.assign( reinterpret_cast<const std::uint8_t *>( px ), reinterpret_cast<const std::uint8_t *>( px ) + size_t( qw ) * qh * 16 );
		return true;
	};
	std::vector<std::uint8_t> bloomRefView, bloomRefMeas;
	WwVkPresent::ToneBloom bloomRefJob;
	Job bloomRefTone {};
	{
		for ( const BloomCase & c : bcases ) {
			Job j = jobs[c.job];
			j.measure = 0;	// the vanilla bloom stands down while this one replaces it
			const WwVkPresent::ToneBloom b = bloomJob( c );
			// GL: the frame, the chain, the measure, then the tone quads (runGl draws the frame again: the same bits)
			std::vector<std::uint8_t> ref, refMeas, vk, vkMeas;
			genFrame( fbFrame );
			GlBloom gb {};
			const bool glChain = chainGl( c, b, gb ) && measureGl( refMeas );
			glBloomNow = &gb;
			const bool glTone = glChain && runGl( j, ref );
			glBloomNow = nullptr;
			// Vulkan: the same frame, everything in the tone pass
			bloomNow = &b;
			bloomFo[0] = c.weight;
			bloomFo[1] = c.ec;
			bloomFo[2] = 2.0f;
			measNow = c.measure;
			const bool vkRan = runVk( j, j.splitSky, vk ) && ( !c.measure || measureTake( vkMeas ) );
			bloomNow = nullptr;
			measNow = false;
			if ( !glChain || !glTone || !vkRan ) {
				std::printf( "TONEVIEW BLOOM %s FAIL (a run failed: GL chain %d, GL tone %d, Vulkan %d)\n", c.name, int( glChain ),
					int( glTone ), int( vkRan ) );
				ok = false;
				continue;
			}
			const Verdict v = compare( vk, ref ), m = compare( vkMeas, refMeas );
			std::printf( "TONEVIEW BLOOM %s view %s (%zu of %zu bytes differ, max %d; splitSky %d, hold %d, ec %.2f)\n", c.name, v.word(),
				v.diff, v.total, v.maxd, int( j.splitSky ), int( c.hold ), double( c.ec ) );
			std::printf( "TONEVIEW BLOOM %s measure %s (%zu of %zu bytes differ)\n", c.name, m.diff == 0 && m.total > 0 ? "EXACT" : "FAIL",
				m.diff, m.total );
			ok = ok && v.ok() && m.diff == 0 && m.total > 0;
			if ( &c == &bcases[0] ) {
				bloomRefView = ref;
				bloomRefMeas = refMeas;
				bloomRefJob = b;
				bloomRefTone = j;
				bloomRefTone.name = "bloom";
			}
		}
		// the reds, on the first case
		const BloomCase & c = bcases[0];
		for ( int red : { 1, 3, 2 } ) {
			if ( red == 2 ) {	// the measure's target cleared (GL, before the pass): a skipped dispatch reads zeros
				if ( !fbo( GL_TEXTURE_2D, GLuint( tone().bloom.meas.tex ), 0 ) )
					ok = false;
				glViewport( 0, 0, qw, qh );
				glClearColor( 0, 0, 0, 0 );
				glClear( GL_COLOR_BUFFER_BIT );
				f.bindFramebuffer( kFb, 0 );
			}
			std::vector<std::uint8_t> vk, vkMeas;
			bloomNow = &bloomRefJob;
			bloomFo[0] = c.weight;
			bloomFo[1] = c.ec;
			bloomFo[2] = 2.0f;
			measNow = true;
			bloomRedTest = red;
			const bool ran = runVk( bloomRefTone, bloomRefTone.splitSky, vk ) && measureTake( vkMeas );
			bloomRedTest = 0;
			bloomNow = nullptr;
			measNow = false;
			const Verdict v = red == 2 ? compare( vkMeas, bloomRefMeas ) : compare( vk, bloomRefView );
			const bool redFails = !ran || ( red == 2 ? v.diff > 0 : !v.ok() );
			std::printf( "TONEVIEW BLOOM RED %d %s (%s: %zu bytes differ, max %d; must FAIL)\n", red, redFails ? "FAIL" : v.word(),
				red == 1 ? "mode 3 not drawn" : red == 3 ? "mode 3 without the backdrop" : "the measure not dispatched", v.diff, v.maxd );
			ok = ok && redFails;
		}
	}
	/* lane VKPOST2: the tone pass and the TAA in ONE crossing (toneRun defers, taaRun colorFromView: Vulkan resolves the
	 * view into cur) against the two crossings (toneRun alone, GL's colour blit into cur as gametaa.cpp does, taaRun).
	 * Both routes start from the same frame, history and lock; cur, vectors, history, lock, picture and the view's samples
	 * must be byte-EXACT. Red: mergeRedTest = 1 (the resolve into cur skipped; cur keeps its clear) must FAIL. */
	const WwVkPresent::ToneStat st0 = WwVkPresent::takeToneStat();
	{
		constexpr GLenum kHalf = 0x140B, kRg = 0x8227;
		const std::uintptr_t key = 0x54410000u;
		const GLenum fmts[8] = { kRgba8, kD24s8, kGlRg32f, kRgba8, kRgba16f, kRgba16f, kRgba16f, kRgba16f };
		unsigned sh[8] = {};
		bool made = true;
		for ( int i = 0; i < 8 && made; i++ )
			made = WwVkPresent::sharedTarget( key + std::uintptr_t( i ), 0, W, H, 1, fmts[i], sh[i], why );
		GLuint fbCur = 0, fbSd = 0;
		if ( made ) {
			f.genFramebuffers( 1, &fbCur );
			f.bindFramebuffer( kFb, fbCur );
			f.framebufferTexture2D( kFb, kCol0, GL_TEXTURE_2D, sh[0], 0 );
			made = f.checkFramebufferStatus( kFb ) == kComplete;
			f.genFramebuffers( 1, &fbSd );
			f.bindFramebuffer( kFb, fbSd );
			f.framebufferTexture2D( kFb, kDsAtt, GL_TEXTURE_2D, sh[1], 0 );
			glDrawBuffer( GL_NONE );
			glReadBuffer( GL_NONE );
			made = made && f.checkFramebufferStatus( kFb ) == kComplete;
		}
		if ( !made ) {
			std::printf( "TONEVIEW MERGE FAIL (the TAA targets: %s)\n", why.c_str() );
			ok = false;
		} else {
			std::mt19937 hr( 53 );
			std::vector<std::uint16_t> histInit( size_t( W ) * H * 4 ), lockInit( histInit.size() );
			for ( auto & x : histInit )
				x = std::uint16_t( hr() & 0x3bff );	// finite halves under 1
			for ( auto & x : lockInit )
				x = std::uint16_t( hr() & 0x3bff );
			const std::vector<std::uint8_t> zero( size_t( W ) * H * 4, 0 );
			auto put = [&]( GLuint t, GLenum typ, const void * d ) {
				glBindTexture( GL_TEXTURE_2D, t );
				glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, typ, d );
			};
			auto get = [&]( GLuint t, GLenum fmt, GLenum typ, size_t bpp ) {
				std::vector<std::uint8_t> o( size_t( W ) * H * bpp );
				glBindTexture( GL_TEXTURE_2D, t );
				glGetTexImage( GL_TEXTURE_2D, 0, fmt, typ, o.data() );
				return o;
			};
			const int wr = 1, rd = 0;
			// out: cur, vectors, history, lock, picture, the view's samples
			auto route = [&]( const Job & j, bool merged, int red, std::vector<std::uint8_t> out[6] ) -> bool {
				put( sh[0], GL_UNSIGNED_BYTE, zero.data() );
				put( sh[4 + rd], kHalf, histInit.data() );
				put( sh[6 + rd], kHalf, lockInit.data() );
				if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
					std::printf( "TONEVIEW MERGE %s: the view: %s\n", j.name, why.c_str() );
					return false;
				}
				const GLuint vfb = WwVkPresent::viewFbo();
				Tone & T = tone();
				T.mergeRed = !merged;	// the app's red WW_VKVIEW_MERGE_RED=1: the tone pass crosses alone
				T.mergeHint = merged;
				T.backoff = 0;
				const WwVkPresent::ToneRun tr = toneOf( j, j.splitSky, vfb );
				if ( !WwVkPresent::toneRun( tr, why ) ) {
					std::printf( "TONEVIEW MERGE %s: toneRun refused: %s\n", j.name, why.c_str() );
					return false;
				}
				if ( T.deferred != merged ) {
					std::printf( "TONEVIEW MERGE %s: the tone pass %s\n", j.name, merged ? "crossed alone" : "was deferred" );
					return false;
				}
				// gametaa.cpp's blits: the depth (both routes), the colour (the two crossings only)
				f.bindFramebuffer( kRead, vfb );
				f.bindFramebuffer( kDraw, fbSd );
				f.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
				if ( !merged ) {
					f.bindFramebuffer( kDraw, fbCur );
					f.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
				}
				f.bindFramebuffer( kFb, vfb );
				WwVkPresent::TaaRun t;
				t.w = W;
				t.h = H;
				t.wr = wr;
				const float jpx = 0.25f, jpy = -0.15f;
				const float offX = 2.0f * jpx / float( W ), offY = 2.0f * jpy / float( H );
				const float reproj[16] = { 1, 0, 0, 0.0002f, 0, 1, 0, -0.0001f, 0, 0, 1, 0, -1.4f / float( W ), 0.001f, 0, 1 };
				const float sj[4] = { 1.0f / float( W ), 1.0f / float( H ), jpx, jpy };
				const std::int32_t ints[4] = { 3, 0, 0, 0 };
				const std::int32_t ints2[4] = { 0, 1, 10, 0 };
				std::memcpy( t.job.sizeJitter, sj, sizeof( sj ) );
				std::memcpy( t.job.ints, ints, sizeof( ints ) );
				std::memcpy( t.job.ints2, ints2, sizeof( ints2 ) );
				t.job.f[0] = 1.0f;
				std::memcpy( t.job.reproj, reproj, sizeof( reproj ) );
				for ( int c = 0; c < 4; c++ )
					std::memcpy( t.mvJob.c[c], reproj + 4 * c, sizeof( t.mvJob.c[c] ) );
				const float mvc[4] = { offX, offY, 1.0f / float( W ), 1.0f / float( H ) };
				std::memcpy( t.mvJob.c[4], mvc, sizeof( mvc ) );
				t.rcasJob.c[0][0] = std::exp2( -0.5f );
				t.rcasJob.c[0][1] = 1.0f;
				t.cur = key + 0;
				t.depth = key + 1;
				t.mv = key + 2;
				t.pic = key + 3;
				for ( int i = 0; i < 2; i++ ) {
					t.hist[i] = key + 4 + std::uintptr_t( i );
					t.lock[i] = key + 6 + std::uintptr_t( i );
				}
				t.colorFromView = merged;
				mergeRedTest = red;
				const bool ran = WwVkPresent::taaRun( t, why );
				mergeRedTest = 0;
				if ( !ran ) {
					std::printf( "TONEVIEW MERGE %s: taaRun refused: %s\n", j.name, why.c_str() );
					return false;
				}
				View & V = view();
				out[5] = readSamples( V.slot[V.cur].colorTex );
				out[0] = get( sh[0], GL_RGBA, GL_UNSIGNED_BYTE, 4 );
				out[1] = get( sh[2], kRg, GL_FLOAT, 8 );
				out[2] = get( sh[4 + wr], GL_RGBA, kHalf, 8 );
				out[3] = get( sh[6 + wr], GL_RGBA, kHalf, 8 );
				out[4] = get( sh[3], GL_RGBA, GL_UNSIGNED_BYTE, 4 );
				if ( !WwVkPresent::present( wnd, W, H, false, glProc, why ) ) {
					std::printf( "TONEVIEW MERGE %s: present: %s\n", j.name, why.c_str() );
					return false;
				}
				pump();
				return glGetError() == GL_NO_ERROR;
			};
			const char * outNames[6] = { "cur", "vectors", "history", "lock", "picture", "view" };
			size_t mergedBytes = 0;
			std::vector<std::uint8_t> twoRef[6];
			// lane VKBLOOM1: + the default frame: the bloom and the measure in the tone pass, all of it in the TAA's crossing
			const Job * const mergeJobs[3] = { &jobs[0], &jobs[2], &bloomRefTone };
			for ( const Job * jp : mergeJobs ) {
				const bool withBloom = jp == &bloomRefTone;
				std::vector<std::uint8_t> two[6], one[6], twoMeas, oneMeas;
				bloomNow = withBloom ? &bloomRefJob : nullptr;
				bloomFo[0] = bcases[0].weight;
				bloomFo[1] = bcases[0].ec;
				bloomFo[2] = 2.0f;
				measNow = withBloom;
				const bool ran = route( *jp, false, 0, two ) && ( !withBloom || measureTake( twoMeas ) ) && route( *jp, true, 0, one )
					&& ( !withBloom || measureTake( oneMeas ) );
				bloomNow = nullptr;
				measNow = false;
				if ( !ran ) {
					std::printf( "TONEVIEW MERGE %s FAIL (a route did not run)\n", jp->name );
					ok = false;
					continue;
				}
				if ( withBloom ) {
					const Verdict vm = compare( oneMeas, twoMeas ), vg = compare( oneMeas, bloomRefMeas );
					std::printf( "TONEVIEW MERGE bloom measure %s (one crossing vs two: %zu of %zu bytes differ; vs GL %zu)\n",
						vm.diff == 0 && vg.diff == 0 && vm.total > 0 ? "EXACT" : "FAIL", vm.diff, vm.total, vg.diff );
					ok = ok && vm.diff == 0 && vg.diff == 0 && vm.total > 0;
				}
				for ( int k = 0; k < 6; k++ ) {
					const Verdict v = compare( one[k], two[k] );
					std::printf( "TONEVIEW MERGE %s %s %s (one crossing vs two: %zu of %zu bytes differ, max %d)\n", jp->name, outNames[k],
						v.diff == 0 && v.total > 0 ? "EXACT" : "FAIL", v.diff, v.total, v.maxd );
					ok = ok && v.diff == 0 && v.total > 0;
					mergedBytes += v.total;
				}
				if ( jp == &jobs[0] )
					for ( int k = 0; k < 6; k++ )
						twoRef[k] = two[k];
			}
			// the red: the merged route without the resolve into cur, against the two crossings
			{
				std::vector<std::uint8_t> one[6];
				const bool ran = route( jobs[0], true, 1, one );
				const Verdict vc = compare( one[0], twoRef[0] ), vp = compare( one[4], twoRef[4] );
				const bool redFails = !ran || vc.diff > 0 || vp.diff > 0;
				std::printf( "TONEVIEW MERGE RED %s (the view's resolve into cur skipped: cur %zu of %zu bytes differ, picture %zu; must "
					"FAIL)\n", redFails ? "FAIL" : "EXACT", vc.diff, vc.total, vp.diff );
				ok = ok && redFails;
			}
			const WwVkPresent::TaaStat ts = WwVkPresent::takeTaaStat();
			std::printf( "TONEVIEW MERGE stat: %d TAA runs, %d in one crossing with the tone pass, %d failed (%s); %zu bytes compared\n",
				ts.runs, ts.merged, ts.fails, ts.failWhy.c_str(), mergedBytes );
			ok = ok && ts.merged == 4 && mergedBytes > 0;	// lane VKBLOOM1: split, unsorted, the bloom frame, the red
		}
		f.bindFramebuffer( kFb, 0 );
		for ( std::uintptr_t k = 0; k < 8; k++ )
			WwVkPresent::sharedTargetDrop( key + k );
	}
	const WwVkPresent::ToneStat st = WwVkPresent::takeToneStat();
	std::printf( "TONEVIEW stat: %d runs on Vulkan, %d failed (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in "
		"%.1f ms, %.1f MB; merge arm: %d runs, %d deferred, %d flushed\n", st0.runs, st0.fails, st0.failWhy.c_str(),
		st0.runs ? st0.cpuMs / st0.runs : 0.0, st0.maxMs, st0.pipelines, st0.buildMs, st0.mb, st.runs, st.deferred, st.flushed );
	f.bindFramebuffer( kFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "TONEVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
