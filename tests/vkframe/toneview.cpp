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
	const GLuint pHdr = program( readFile( dir + "/cell_hdr.vert.glsl" ), readFile( dir + "/cell_hdr.frag.glsl" ), "cell_hdr" );
	if ( !pGen || !pUnpack || !pHdr ) {
		std::printf( "TONEVIEW GATE FAILED (GL programs)\n" );
		return 2;
	}
	// every sampler on a unit of its own (two sampler types on one unit refuse the draw); the read ones on fixed units
	{
		f.useProgram( pHdr );
		GLint n = 0;
		f.getProgramiv( pHdr, kActiveUnis, &n );
		int next = 20;
		for ( GLint i = 0; i < n; i++ ) {
			char name[256] = {};
			GLint size = 0;
			GLenum typ = 0;
			f.getActiveUniform( pHdr, GLuint( i ), 256, nullptr, &size, &typ, name );
			const bool sampler = ( typ >= 0x8B5D && typ <= 0x8B62 ) || ( typ >= 0x8DC1 && typ <= 0x8DD8 ) || typ == 0x900C
				|| typ == 0x900D || ( typ >= 0x9108 && typ <= 0x910A );
			if ( !sampler )
				continue;
			std::string nm( name );
			nm = nm.substr( 0, nm.find( '[' ) );
			const int unit = nm == "hdrSrc" ? 8 : nm == "hdrStencil" ? 9 : nm == "cellIsBloom" ? 11 : nm == "cellIsLut" ? 12 : next++;
			u1i( pHdr, nm.c_str(), unit );
		}
	}

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

	auto runVk = [&]( const Job & j, bool splitSky, std::vector<std::uint8_t> & out ) -> bool {
		if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
			std::printf( "TONEVIEW %s: the view: %s\n", j.name, why.c_str() );
			return false;
		}
		const GLuint vfb = WwVkPresent::viewFbo();
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
		u1i( pHdr, "cellIsFoBloomOn", 0 );
		u1i( pHdr, "cellFogBloomOn", 0 );
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
	const WwVkPresent::ToneStat st = WwVkPresent::takeToneStat();
	std::printf( "TONEVIEW stat: %d runs on Vulkan, %d failed (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in "
		"%.1f ms, %.1f MB\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines,
		st.buildMs, st.mb );
	f.bindFramebuffer( kFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "TONEVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
