// Lane VKSHARE1 (2026-11-12): the Modern TAA at High on the present device (vkpresent.cpp taaRun, built in here with
// WW_VKPRESENT_NO_QT: the shipped code) against GL's own passes (the tree's cell_ao.vert + game_taa_mv / taa2 / taa2_rcas,
// expanded by taaview_expand.py) drawn the way gametaa.cpp draws them.
// Usage: taaview <glsl dir> [W H]
// Each frame: a seeded, jittered, moving picture (RGBA8) and depth (D24S8) go into the shared cur / depth targets; GL runs
// its vectors, resolve and RCAS copy-back into its own targets and an MS RGBA8 reference; GL's history and lock of the
// read slot are copied into the shared ones (so every frame is ONE step from the same state); then taaRun draws the
// same on Vulkan into the view. Compared per frame: vectors (rule ndc: |d| <= 2^-22), next history and lock (rule step:
// <= 0.5 % halves differ, <= 0.1 % by > 1 ulp), the picture and the view per sample (rule step8: the same on bytes).
// Frames 0-5 without TAAINPUTS1's velocity / mask, 6-7 with both. Reds (must FAIL): the jitter's sign flipped on Vulkan
// (history), the vectors' jitter flipped (vectors). Also item 1: an R8 3D and an RGBA16F 2D-array shared target round trip.
// Prints `TAAVIEW ...` lines with counts and `TAAVIEW GATE OK|FAILED`.
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

namespace tav
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

constexpr GLenum cT2DMS = 0x9100, cT3D = 0x806F, cT2DArr = 0x8C1A, cFb = 0x8D40, cCol0 = 0x8CE0, cDsAtt = 0x821A;
constexpr GLenum cRgba16f = 0x881A, cRg32f = 0x8230, cRg16f = 0x822F, cR8 = 0x8229, cD24s8 = 0x88F0, cRg = 0x8227;
constexpr GLenum cR32f = 0x822E, cRed = 0x1903, cClamp = 0x812F, cHalf = 0x140B, cRgba8 = 0x8058, cRgba32f = 0x8814;
constexpr GLenum cTex0 = 0x84C0, cFrag = 0x8B30, cVert = 0x8B31, cCompiled = 0x8B81, cLinked = 0x8B82, cComplete = 0x8CD5;
constexpr GLenum cMaxLevel = 0x813D, cCompareMode = 0x884C, cColor = 0x1800;

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
	GLint( WINAPI * getUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * uniform1f )( GLint, GLfloat ) = nullptr;
	void( WINAPI * uniform2f )( GLint, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniformMatrix4fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
	void( WINAPI * activeTexture )( GLenum ) = nullptr;
	void( WINAPI * texImage2DMultisample )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean ) = nullptr;
	void( WINAPI * texSubImage3D )( GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * drawBuffers )( GLsizei, const GLenum * ) = nullptr;
	void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
	void( WINAPI * copyImageSubData )( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei,
		GLsizei, GLsizei ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * disableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
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
		&& load( f.useProgram, "glUseProgram" ) && load( f.getUniformLocation, "glGetUniformLocation" )
		&& load( f.uniform1i, "glUniform1i" ) && load( f.uniform1f, "glUniform1f" ) && load( f.uniform2f, "glUniform2f" )
		&& load( f.uniformMatrix4fv, "glUniformMatrix4fv" ) && load( f.activeTexture, "glActiveTexture" )
		&& load( f.texImage2DMultisample, "glTexImage2DMultisample" ) && load( f.texSubImage3D, "glTexSubImage3D" )
		&& load( f.genFramebuffers, "glGenFramebuffers" ) && load( f.bindFramebuffer, "glBindFramebuffer" )
		&& load( f.framebufferTexture2D, "glFramebufferTexture2D" ) && load( f.checkFramebufferStatus, "glCheckFramebufferStatus" )
		&& load( f.drawBuffers, "glDrawBuffers" ) && load( f.clearBufferfv, "glClearBufferfv" )
		&& load( f.copyImageSubData, "glCopyImageSubData" ) && load( f.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( f.disableVertexAttribArray, "glDisableVertexAttribArray" )
		&& load( f.vertexAttribPointer, "glVertexAttribPointer" );
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
	f.getShaderiv( s, cCompiled, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getShaderInfoLog( s, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "TAAVIEW compile failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs, const char * tag )
{
	const GLuint a = shader( cVert, vs, tag ), b = shader( cFrag, fs, tag );
	if ( !a || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, cLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "TAAVIEW link failed (%s): %.1500s\n", tag, log.data() );
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
void u2f( GLuint p, const char * n, float a, float b )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniform2f( l, a, b );
}
void u4m( GLuint p, const char * n, const float * m )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l >= 0 )
		f.uniformMatrix4fv( l, 1, GL_FALSE, m );
}

// gametaa.cpp makeTex's state: one level, CLAMP, the filter given
void texState( GLuint t, bool linear )
{
	glBindTexture( GL_TEXTURE_2D, t );
	const GLint flt = linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, cClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, cClamp );
	glTexParameteri( GL_TEXTURE_2D, cMaxLevel, 0 );
}

GLuint tex2d( int w, int h, GLenum ifmt, GLenum fmt, GLenum typ, const void * data, bool linear )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, h, 0, fmt, typ, data );
	texState( t, linear );
	return t;
}

GLuint msTex( int w, int h, GLenum ifmt, int s )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( cT2DMS, t );
	f.texImage2DMultisample( cT2DMS, s, ifmt, w, h, GL_TRUE );
	glBindTexture( cT2DMS, 0 );
	return t;
}

GLuint newFbo()
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	return b;
}

void bindUnit( int unit, GLenum kind, GLuint t )
{
	f.activeTexture( cTex0 + GLenum( unit ) );
	glBindTexture( kind, t );
}

const char * kTriVs = "#version 410 core\n"
	"void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); "
	"gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";
const char * kDepthFs = "#version 410 core\n"
	"uniform sampler2D genDepth;\n"
	"void main() { gl_FragDepth = texelFetch( genDepth, ivec2( gl_FragCoord.xy ), 0 ).r; }\n";
const char * kUnpackFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ); }\n";

float halton( int i, int b )
{
	float r = 0.0f, fr = 1.0f;
	for ( int k = i + 1; k > 0; k /= b ) {
		fr /= float( b );
		r += fr * float( k % b );
	}
	return r;
}

int halfOrd( std::uint16_t h )
{
	return ( h & 0x8000 ) ? -int( h & 0x7fff ) : int( h );
}

struct Verdict
{
	size_t diff = 0, big = 0, total = 0;
	double maxd = 0.0;
	const char * rule = "";
	bool ok = false;
	const char * word() const { return !ok ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

// step: <= 0.5 % differ, <= 0.1 % by more than one step (an ulp of a half, a byte level)
Verdict stepRule( size_t diff, size_t big, size_t total, double maxd, const char * rule )
{
	Verdict v;
	v.diff = diff;
	v.big = big;
	v.total = total;
	v.maxd = maxd;
	v.rule = rule;
	v.ok = total > 0 && double( diff ) <= 0.005 * double( total ) && double( big ) <= 0.001 * double( total );
	return v;
}

Verdict cmpHalf( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
{
	if ( a.size() != b.size() || a.empty() )
		return stepRule( 1, 1, 0, 65535, "step" );
	size_t diff = 0, big = 0;
	int maxd = 0;
	for ( size_t i = 0; i < a.size(); i++ ) {
		const int d = std::abs( halfOrd( a[i] ) - halfOrd( b[i] ) );
		diff += d != 0;
		big += d > 1;
		maxd = std::max( maxd, d );
	}
	return stepRule( diff, big, a.size(), maxd, "step" );
}

Verdict cmpByte( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	if ( a.size() != b.size() || a.empty() )
		return stepRule( 1, 1, 0, 255, "step8" );
	size_t diff = 0, big = 0;
	int maxd = 0;
	for ( size_t i = 0; i < a.size(); i++ ) {
		const int d = std::abs( int( a[i] ) - int( b[i] ) );
		diff += d != 0;
		big += d > 1;
		maxd = std::max( maxd, d );
	}
	return stepRule( diff, big, a.size(), maxd, "step8" );
}

Verdict cmpNdc( const std::vector<float> & a, const std::vector<float> & b )
{
	Verdict v;
	v.rule = "ndc";
	v.total = a.size();
	if ( a.size() != b.size() || a.empty() )
		return v;
	for ( size_t i = 0; i < a.size(); i++ ) {
		const double d = std::fabs( double( a[i] ) - double( b[i] ) );
		v.diff += d != 0.0;
		v.big += d > std::ldexp( 1.0, -22 );
		v.maxd = std::max( v.maxd, d );
	}
	v.ok = v.big == 0;
	return v;
}
}  // namespace tav

int main( int argc, char ** argv )
{
	using namespace tav;
	if ( argc < 2 ) {
		std::printf( "usage: taaview <glsl dir> [W H]\nTAAVIEW GATE FAILED\n" );
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
	wc.lpszClassName = "wwtaaview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwtaaview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwtaaview", "taa view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };	// window, GL, double, full accel, RGBA8, D24S8, 4 samples
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "TAAVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
		std::printf( "TAAVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "view %dx%d x%d, GL on %s\n", W, H, S, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	const std::string vs = readFile( dir + "/cell_ao.vert.glsl" );
	const GLuint pMv = program( vs, readFile( dir + "/game_taa_mv.frag.glsl" ), "game_taa_mv" );
	const GLuint pTaa = program( vs, readFile( dir + "/taa2.frag.glsl" ), "taa2" );
	const GLuint pRcas = program( vs, readFile( dir + "/taa2_rcas.frag.glsl" ), "taa2_rcas" );
	const GLuint pDepth = program( kTriVs, kDepthFs, "depth" ), pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	if ( !pMv || !pTaa || !pRcas || !pDepth || !pUnpack ) {
		std::printf( "TAAVIEW GATE FAILED (GL programs)\n" );
		return 2;
	}
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	bool ok = true;
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "TAAVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}

	// item 1: an R8 3D (the clouds' noise volumes' shape) and an RGBA16F 2D array, shared, written and read back by GL
	{
		std::mt19937 rng( 5 );
		size_t bytes = 0;
		bool same = true;
		unsigned t3 = 0, ta = 0;
		if ( !WwVkPresent::sharedTarget( 0x7E570001u, 1, 32, 32, 16, cR8, t3, why )
			|| !WwVkPresent::sharedTarget( 0x7E570002u, 2, 64, 32, 4, cRgba16f, ta, why ) ) {
			std::printf( "TAAVIEW SHARED FAIL (%s)\n", why.c_str() );
			ok = false;
		} else {
			std::vector<std::uint8_t> v3( 32 * 32 * 16 ), b3( v3.size() );
			for ( auto & x : v3 )
				x = std::uint8_t( rng() );
			glBindTexture( cT3D, t3 );
			f.texSubImage3D( cT3D, 0, 0, 0, 0, 32, 32, 16, cRed, GL_UNSIGNED_BYTE, v3.data() );
			glGetTexImage( cT3D, 0, cRed, GL_UNSIGNED_BYTE, b3.data() );
			glBindTexture( cT3D, 0 );
			std::vector<std::uint16_t> va( 64 * 32 * 4 * 4 ), ba( va.size() );
			for ( auto & x : va )
				x = std::uint16_t( rng() & 0x7bff );	// finite halves
			glBindTexture( cT2DArr, ta );
			f.texSubImage3D( cT2DArr, 0, 0, 0, 0, 64, 32, 4, GL_RGBA, cHalf, va.data() );
			glGetTexImage( cT2DArr, 0, GL_RGBA, cHalf, ba.data() );
			glBindTexture( cT2DArr, 0 );
			same = v3 == b3 && va == ba && glGetError() == GL_NO_ERROR;
			bytes = v3.size() + va.size() * 2;
			std::printf( "TAAVIEW SHARED %s (R8 3D 32x32x16 + RGBA16F array 64x32x4: %zu bytes round-tripped)\n", same ? "EXACT" : "FAIL",
				bytes );
			ok = ok && same;
		}
		WwVkPresent::sharedTargetDrop( 0x7E570001u );
		WwVkPresent::sharedTargetDrop( 0x7E570002u );
	}

	// the shared targets, gametaa.cpp's keys and formats (cur, depth, mv, pic, hist x2, lock x2)
	const std::uintptr_t key = 0x54410000u;
	const GLenum fmts[8] = { cRgba8, cD24s8, cRg32f, cRgba8, cRgba16f, cRgba16f, cRgba16f, cRgba16f };
	unsigned sh[8] = {};
	for ( int i = 0; i < 8; i++ )
		if ( !WwVkPresent::sharedTarget( key + std::uintptr_t( i ), 0, W, H, 1, fmts[i], sh[i], why ) ) {
			std::printf( "TAAVIEW GATE FAILED (shared target %d: %s)\n", i, why.c_str() );
			return 2;
		}
	for ( int i = 0; i < 8; i++ ) {
		texState( sh[i], i == 4 || i == 5 );
		if ( i == 1 )
			glTexParameteri( GL_TEXTURE_2D, cCompareMode, GL_NONE );
	}
	const GLuint sCur = sh[0], sDepth = sh[1], sMv = sh[2], sPic = sh[3], sHist[2] = { sh[4], sh[5] }, sLock[2] = { sh[6], sh[7] };
	// GL's own targets (gametaa.cpp's non-shared shapes) and the reference view
	const GLuint gMv = tex2d( W, H, cRg32f, cRg, GL_FLOAT, nullptr, false );
	const GLuint gOut = tex2d( W, H, cRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	GLuint gHist[2], gLock[2];
	for ( int i = 0; i < 2; i++ ) {
		gHist[i] = tex2d( W, H, cRgba16f, GL_RGBA, GL_FLOAT, nullptr, true );
		gLock[i] = tex2d( W, H, cRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	}
	const GLuint refCol = msTex( W, H, cRgba8, S );
	const int WS = W * S;
	const GLuint unp = tex2d( WS, H, cRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	const GLuint fbMv = newFbo(), fbOut = newFbo(), fbRef = newFbo(), fbUnp = newFbo(), fbDepth = newFbo();
	f.bindFramebuffer( cFb, fbMv );
	f.framebufferTexture2D( cFb, cCol0, GL_TEXTURE_2D, gMv, 0 );
	f.bindFramebuffer( cFb, fbRef );
	f.framebufferTexture2D( cFb, cCol0, cT2DMS, refCol, 0 );
	const bool refOk = f.checkFramebufferStatus( cFb ) == cComplete;
	f.bindFramebuffer( cFb, fbUnp );
	f.framebufferTexture2D( cFb, cCol0, GL_TEXTURE_2D, unp, 0 );
	f.bindFramebuffer( cFb, fbDepth );
	f.framebufferTexture2D( cFb, cDsAtt, GL_TEXTURE_2D, sDepth, 0 );
	glDrawBuffer( GL_NONE );
	glReadBuffer( GL_NONE );
	const bool depthOk = f.checkFramebufferStatus( cFb ) == cComplete;
	if ( !refOk || !depthOk ) {
		std::printf( "TAAVIEW GATE FAILED (GL targets: ref %d depth %d)\n", int( refOk ), int( depthOk ) );
		return 2;
	}
	// TAAINPUTS1's GL textures (frames 6-7): velocity RG16F, mask RGBA8
	std::mt19937 rng( 41 );
	std::uniform_real_distribution<float> ur( 0.0f, 1.0f );
	std::vector<float> vel( size_t( W ) * H * 2 );
	for ( float & x : vel )
		x = ( ur( rng ) - 0.5f ) * 0.004f;
	std::vector<std::uint8_t> mask( size_t( W ) * H * 4 );
	for ( size_t i = 0; i < mask.size(); i++ )
		mask[i] = ( i % 4 == 0 && ur( rng ) < 0.1f ) ? std::uint8_t( 255 ) : ( i % 4 == 2 && ur( rng ) < 0.05f ) ? std::uint8_t( 200 ) : 0;
	const GLuint gVel = tex2d( W, H, cRg16f, cRg, GL_FLOAT, vel.data(), false );
	const GLuint gMask = tex2d( W, H, cRgba8, GL_RGBA, GL_UNSIGNED_BYTE, mask.data(), false );
	std::vector<float> depthData( size_t( W ) * H );
	const GLuint tDepthData = tex2d( W, H, cR32f, cRed, GL_FLOAT, nullptr, false );
	std::vector<std::uint8_t> pic( size_t( W ) * H * 4 );

	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	auto drawQuad = [&]() {
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, quad );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx );
		f.disableVertexAttribArray( 0 );
	};
	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint8_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( cFb, fbUnp );
		glViewport( 0, 0, WS, H );
		f.useProgram( pUnpack );
		bindUnit( 8, cT2DMS, msColor );
		u1i( pUnpack, "src", 8 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, GL_UNSIGNED_BYTE, out.data() );
		glViewport( 0, 0, W, H );
		return out;
	};
	auto getHalf = [&]( GLuint t ) {
		std::vector<std::uint16_t> out( size_t( W ) * H * 4 );
		glBindTexture( GL_TEXTURE_2D, t );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, cHalf, out.data() );
		return out;
	};
	auto getRg = [&]( GLuint t ) {
		std::vector<float> out( size_t( W ) * H * 2 );
		glBindTexture( GL_TEXTURE_2D, t );
		glGetTexImage( GL_TEXTURE_2D, 0, cRg, GL_FLOAT, out.data() );
		return out;
	};
	auto getByte = [&]( GLuint t ) {
		std::vector<std::uint8_t> out( size_t( W ) * H * 4 );
		glBindTexture( GL_TEXTURE_2D, t );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data() );
		return out;
	};

	// one frame: the inputs, GL's passes, the copy of GL's read slot, Vulkan's passes; red 1 = the resolve's jitter flipped,
	// 2 = the vectors' jitter flipped (Vulkan only)
	const float shiftPx = 0.7f;
	size_t compared = 0;
	auto frame = [&]( int fi, int red, Verdict out[5] ) -> bool {
		const int wr = fi & 1, rd = wr ^ 1;
		const bool noHist = fi == 0;
		const int inputs = fi >= 6 ? 3 : 0;
		const float jpx = halton( fi % 8, 2 ) - 0.5f, jpy = halton( fi % 8, 3 ) - 0.5f;
		const float offX = 2.0f * jpx / float( W ), offY = 2.0f * jpy / float( H );
		const float shift = shiftPx * float( fi );
		// the picture: smooth bands, hard edges, 1 px lines, a mover; seen at p - jitter
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const float sx = float( x ) + 0.5f - jpx - shift, sy = float( y ) + 0.5f - jpy;
				float r = 0.5f + 0.5f * std::sin( sx * 0.31f ) * std::cos( sy * 0.17f );
				float g = ( ( int( std::floor( sx / 9.0f ) ) + int( std::floor( sy / 7.0f ) ) ) & 1 ) ? 0.8f : 0.2f;
				float b = std::fabs( std::fmod( sx + 3.0f * sy, 23.0f ) ) < 1.0f ? 1.0f : 0.1f + 0.002f * float( y );
				const float mx = float( x ) + 0.5f - jpx - 3.0f * float( fi ) - float( W ) * 0.3f;	// the mover: 3 px a frame, no vector
				if ( mx > 0.0f && mx < 40.0f && sy > float( H ) * 0.4f && sy < float( H ) * 0.6f )
					r = 1.0f, g = 0.05f, b = 0.05f;
				const float c[4] = { r, g, b, 1.0f };
				for ( int k = 0; k < 4; k++ )
					pic[( size_t( y ) * W + x ) * 4 + k] = std::uint8_t( std::lround( std::min( std::max( c[k], 0.0f ), 1.0f ) * 255.0f ) );
				depthData[size_t( y ) * W + x] = ( sy > float( H ) * 0.8f ) ? 1.0f : 0.3f + 0.6f * float( ( x * 7 + y * 13 ) % 97 ) / 97.0f;
			}
		glBindTexture( GL_TEXTURE_2D, sCur );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pic.data() );
		glBindTexture( GL_TEXTURE_2D, tDepthData );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, W, H, cRed, GL_FLOAT, depthData.data() );
		f.bindFramebuffer( cFb, fbDepth );
		glViewport( 0, 0, W, H );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		glDepthMask( GL_TRUE );
		f.useProgram( pDepth );
		bindUnit( 8, GL_TEXTURE_2D, tDepthData );
		u1i( pDepth, "genDepth", 8 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glDepthFunc( GL_LESS );
		glDisable( GL_DEPTH_TEST );
		// the camera: a small pan with a perspective term (GL's column-major mat4)
		float reproj[16] = { 1, 0, 0, 0.0002f, 0, 1, 0, -0.0001f, 0, 0, 1, 0, -2.0f * shiftPx / float( W ), 0.001f, 0, 1 };
		const int dither = fi * 7 + 3;
		// GL: gametaa.cpp's draws
		f.bindFramebuffer( cFb, fbMv );
		f.useProgram( pMv );
		bindUnit( 3, GL_TEXTURE_2D, sDepth );
		u1i( pMv, "depthTex", 3 );
		u4m( pMv, "reproj", reproj );
		u2f( pMv, "jitterNdc", offX, offY );
		u2f( pMv, "invSize", 1.0f / float( W ), 1.0f / float( H ) );
		drawQuad();
		f.bindFramebuffer( cFb, fbOut );
		if ( noHist )
			for ( int i = 0; i < 2; i++ ) {
				f.framebufferTexture2D( cFb, cCol0, GL_TEXTURE_2D, gLock[i], 0 );
				const GLenum one = cCol0;
				f.drawBuffers( 1, &one );
				const GLfloat zero[4] = { 0, 0, 0, 0 };
				f.clearBufferfv( cColor, 0, zero );
			}
		f.framebufferTexture2D( cFb, cCol0, GL_TEXTURE_2D, gHist[wr], 0 );
		f.framebufferTexture2D( cFb, cCol0 + 1, GL_TEXTURE_2D, gOut, 0 );
		f.framebufferTexture2D( cFb, cCol0 + 2, GL_TEXTURE_2D, gLock[wr], 0 );
		const GLenum three[3] = { cCol0, cCol0 + 1, cCol0 + 2 };
		f.drawBuffers( 3, three );
		f.useProgram( pTaa );
		bindUnit( 0, GL_TEXTURE_2D, sCur );
		bindUnit( 1, GL_TEXTURE_2D, gHist[rd] );
		bindUnit( 2, GL_TEXTURE_2D, gMv );
		bindUnit( 3, GL_TEXTURE_2D, sDepth );
		bindUnit( 4, GL_TEXTURE_2D, gLock[rd] );
		bindUnit( 5, GL_TEXTURE_2D, ( inputs & 1 ) ? gVel : gMv );
		bindUnit( 6, GL_TEXTURE_2D, ( inputs & 2 ) ? gMask : gMv );
		u1i( pTaa, "curTex", 0 );
		u1i( pTaa, "histTex", 1 );
		u1i( pTaa, "mvTex", 2 );
		u1i( pTaa, "depthTex", 3 );
		u1i( pTaa, "lockTex", 4 );
		u1i( pTaa, "velTex", 5 );
		u1i( pTaa, "maskTex", 6 );
		u2f( pTaa, "invSize", 1.0f / float( W ), 1.0f / float( H ) );
		u2f( pTaa, "jitterPx", jpx, jpy );
		u1i( pTaa, "taa2Tier", 3 );
		u1i( pTaa, "taa2Red", 0 );
		u1i( pTaa, "taa2NoHist", noHist ? 1 : 0 );
		u1i( pTaa, "wwDitherOn", 1 );
		u1i( pTaa, "wwDitherSeed", dither );
		u4m( pTaa, "reproj", reproj );
		u1i( pTaa, "taa2Inputs", inputs );
		u1i( pTaa, "taa2Linear", 0 );
		u1f( pTaa, "taa2Expose", 1.0f );
		drawQuad();
		f.framebufferTexture2D( cFb, cCol0 + 2, GL_TEXTURE_2D, 0, 0 );
		f.bindFramebuffer( cFb, fbRef );
		f.useProgram( pRcas );
		bindUnit( 0, GL_TEXTURE_2D, gOut );
		u1i( pRcas, "srcTex", 0 );
		u1f( pRcas, "rcasSharp", std::exp2( -0.5f ) );
		u1i( pRcas, "rcasLinear", 0 );
		u1f( pRcas, "rcasExpose", 1.0f );
		drawQuad();
		f.useProgram( 0 );
		// Vulkan starts from GL's read slot (one step from the same state)
		f.copyImageSubData( gHist[rd], GL_TEXTURE_2D, 0, 0, 0, 0, sHist[rd], GL_TEXTURE_2D, 0, 0, 0, 0, W, H, 1 );
		f.copyImageSubData( gLock[rd], GL_TEXTURE_2D, 0, 0, 0, 0, sLock[rd], GL_TEXTURE_2D, 0, 0, 0, 0, W, H, 1 );
		if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
			std::printf( "TAAVIEW f%d: the view: %s\n", fi, why.c_str() );
			return false;
		}
		f.bindFramebuffer( cFb, WwVkPresent::viewFbo() );
		WwVkPresent::TaaRun t;
		t.w = W;
		t.h = H;
		t.wr = wr;
		const float sgn = red == 1 ? -1.0f : 1.0f, msgn = red == 2 ? -1.0f : 1.0f;
		const float sj[4] = { 1.0f / float( W ), 1.0f / float( H ), sgn * jpx, sgn * jpy };
		const std::int32_t ints[4] = { 3, 0, noHist ? 1 : 0, inputs };
		const std::int32_t ints2[4] = { 0, 1, dither, 0 };
		std::memcpy( t.job.sizeJitter, sj, sizeof( sj ) );
		std::memcpy( t.job.ints, ints, sizeof( ints ) );
		std::memcpy( t.job.ints2, ints2, sizeof( ints2 ) );
		t.job.f[0] = 1.0f;
		std::memcpy( t.job.reproj, reproj, sizeof( reproj ) );
		for ( int c = 0; c < 4; c++ )
			std::memcpy( t.mvJob.c[c], reproj + 4 * c, sizeof( t.mvJob.c[c] ) );
		const float mvc[4] = { msgn * offX, msgn * offY, 1.0f / float( W ), 1.0f / float( H ) };
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
		t.vel = ( inputs & 1 ) ? gVel : 0;
		t.mask = ( inputs & 2 ) ? gMask : 0;
		t.velKey = key + 8;
		t.maskKey = key + 9;
		if ( !WwVkPresent::taaRun( t, why ) ) {
			std::printf( "TAAVIEW f%d: taaRun refused: %s\n", fi, why.c_str() );
			return false;
		}
		View & V = view();
		const std::vector<std::uint8_t> vkView = readSamples( V.slot[V.cur].colorTex ), glView = readSamples( refCol );
		out[0] = cmpNdc( getRg( sMv ), getRg( gMv ) );
		out[1] = cmpHalf( getHalf( sHist[wr] ), getHalf( gHist[wr] ) );
		out[2] = cmpHalf( getHalf( sLock[wr] ), getHalf( gLock[wr] ) );
		out[3] = cmpByte( getByte( sPic ), getByte( gOut ) );
		out[4] = cmpByte( vkView, glView );
		for ( int k = 0; k < 5; k++ )
			compared += out[k].total;
		if ( !WwVkPresent::present( wnd, W, H, false, glProc, why ) ) {
			std::printf( "TAAVIEW f%d: present: %s\n", fi, why.c_str() );
			return false;
		}
		pump();
		return glGetError() == GL_NO_ERROR;
	};

	const char * names[5] = { "vectors", "history", "lock", "picture", "view" };
	const int kFrames = 8;
	for ( int fi = 0; fi < kFrames; fi++ ) {
		Verdict v[5];
		if ( !frame( fi, 0, v ) ) {
			std::printf( "TAAVIEW f%d FAIL (the frame did not run)\n", fi );
			ok = false;
			continue;
		}
		for ( int k = 0; k < 5; k++ ) {
			std::printf( "TAAVIEW f%d%s %s %s (rule %s: %zu of %zu differ, %zu beyond, max %.3g)\n", fi, fi >= 6 ? " inputs" : "", names[k],
				v[k].word(), v[k].rule, v[k].diff, v[k].total, v[k].big, v[k].maxd );
			ok = ok && v[k].ok;
		}
	}
	// the reds, each on a frame with history (must FAIL their own compare)
	{
		Verdict v[5];
		const bool ran = frame( kFrames, 1, v );
		const bool fails = !ran || !v[1].ok;
		std::printf( "TAAVIEW RED %s (the resolve's jitter flipped on Vulkan: history %zu of %zu differ, %zu beyond; must FAIL)\n",
			fails ? "FAIL" : v[1].word(), v[1].diff, v[1].total, v[1].big );
		ok = ok && fails;
	}
	{
		Verdict v[5];
		const bool ran = frame( kFrames + 1, 2, v );
		const bool fails = !ran || !v[0].ok;
		std::printf( "TAAVIEW RED %s (the vectors' jitter flipped on Vulkan: %zu of %zu beyond 2^-22, max %.3g; must FAIL)\n",
			fails ? "FAIL" : v[0].word(), v[0].big, v[0].total, v[0].maxd );
		ok = ok && fails;
	}
	const WwVkPresent::TaaStat st = WwVkPresent::takeTaaStat();
	std::printf( "TAAVIEW stat: %d runs on Vulkan, %d failed (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in "
		"%.1f ms; %zu values compared\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs,
		st.pipelines, st.buildMs, compared );
	f.bindFramebuffer( cFb, 0 );
	for ( std::uintptr_t k = 0; k < 10; k++ )
		WwVkPresent::sharedTargetDrop( key + k );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "TAAVIEW GATE %s\n", ok && compared > 0 ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
