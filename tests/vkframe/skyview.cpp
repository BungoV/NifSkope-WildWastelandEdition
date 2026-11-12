// Lane VKSKY1 (2026-11-12, row G first pass): the cell sky on the present device (vkpresent.cpp skyRun, built in here
// with WW_VKPRESENT_NO_QT: the shipped code) against GL's own atmo_sky program (cell_ao.vert + atmo_sky.frag, expanded by
// skyview_expand.py) drawn the way atmosphere.cpp step 3 draws it, into the same kind of frame, per sample.
// Usage: skyview <glsl dir> [W H]
// Both arms: the frame (RGBA16F + D24S8, 4 samples) cleared (colour, depth 1, stencil 0), the sky (GL quad, every test
// off / Vulkan skyRun into toneFrame's images), then GL draws an "object" (depth 0.5, test LESS) over a centre rectangle:
// the object must cover the sky in both (the crossing orders them). The LUTs are seeded RGBA16F GL textures at the app's
// sizes; the global block is the same 336 bytes (a GL UBO / SkyRun::global); the loose block is packed from the GL
// program's own uniform values (glGetUniform over kSkyFields). Read back per sample (texelFetch) as halves.
// Prints `SKYVIEW <job> EXACT|BOUND|FAIL`, `SKYVIEW RED <name> ...` (each must FAIL) and `SKYVIEW GATE OK|FAILED`.
// BOUND (stated before the first run, never widened): every half within 4 ulp and <= 0.1 % of halves past 1 ulp. GL and
// Vulkan lower acos / sqrt / pow / exp2 and the bilinear LUT weights in their own ways; a field or LUT mix-up moves
// whole regions by far more.
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

namespace sv
{
using WwVkPresent::kSkyGlobalBytes;
using WwVkPresent::kSkyLooseBytes;
using WwVkPresent::kSkyFields;
using WwVkPresent::SkyField;
HMODULE gOpenGl = nullptr;
void * glProc( const char * name )
{
	void * p = reinterpret_cast<void *>( wglGetProcAddress( name ) );
	const intptr_t i = reinterpret_cast<intptr_t>( p );
	if ( !p || i == 1 || i == 2 || i == 3 || i == -1 )
		p = reinterpret_cast<void *>( GetProcAddress( gOpenGl, name ) );
	return p;
}

LRESULT CALLBACK wndProc( HWND hw, UINT m, WPARAM wp, LPARAM lp )
{
	return DefWindowProcA( hw, m, wp, lp );
}

void pump()
{
	MSG msg;
	while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &msg );
		DispatchMessageA( &msg );
	}
}

constexpr GLenum kT2DMS = 0x9100, kFb = 0x8D40, kCol0 = 0x8CE0, kDsAtt = 0x821A, kD24s8 = 0x88F0;	// kRgba16f: vkpresent.cpp's
constexpr GLenum kClamp = 0x812F, kTex0 = 0x84C0, kFrag = 0x8B30, kVert = 0x8B31, kCompiled = 0x8B81, kLinked = 0x8B82;
constexpr GLenum kActiveUnis = 0x8B86, kComplete = 0x8CD5, kMaxLevel = 0x813D, kHalf = 0x140B;
constexpr GLenum kUbo = 0x8A11, kBlockSize = 0x8A40, kStatic = 0x88E4;

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
	void( WINAPI * uniform2f )( GLint, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform3f )( GLint, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform4f )( GLint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * getUniformfv )( GLuint, GLint, GLfloat * ) = nullptr;
	void( WINAPI * getUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	GLuint( WINAPI * getUniformBlockIndex )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniformBlockBinding )( GLuint, GLuint, GLuint ) = nullptr;
	void( WINAPI * getActiveUniformBlockiv )( GLuint, GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * genBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferData )( GLenum, std::ptrdiff_t, const void *, GLenum ) = nullptr;
	void( WINAPI * bindBufferBase )( GLenum, GLuint, GLuint ) = nullptr;
	void( WINAPI * activeTexture )( GLenum ) = nullptr;
	void( WINAPI * texImage2DMultisample )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
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
		&& load( f.uniform1f, "glUniform1f" ) && load( f.uniform2f, "glUniform2f" ) && load( f.uniform3f, "glUniform3f" )
		&& load( f.uniform4f, "glUniform4f" ) && load( f.getUniformfv, "glGetUniformfv" )
		&& load( f.getUniformiv, "glGetUniformiv" ) && load( f.getUniformBlockIndex, "glGetUniformBlockIndex" )
		&& load( f.uniformBlockBinding, "glUniformBlockBinding" )
		&& load( f.getActiveUniformBlockiv, "glGetActiveUniformBlockiv" ) && load( f.genBuffers, "glGenBuffers" )
		&& load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.bindBufferBase, "glBindBufferBase" ) && load( f.activeTexture, "glActiveTexture" )
		&& load( f.texImage2DMultisample, "glTexImage2DMultisample" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTexture2D, "glFramebufferTexture2D" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" )
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

GLuint shader( GLenum type, const std::string & src, const char * tag )
{
	const GLuint s = f.createShader( type );
	const char * p = src.c_str();
	f.shaderSource( s, 1, &p, nullptr );
	f.compileShader( s );
	GLint ok = 0;
	f.getShaderiv( s, kCompiled, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getShaderInfoLog( s, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "SKYVIEW compile failed (%s): %.1500s\n", tag, log.data() );
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
		std::printf( "SKYVIEW link failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return p;
}

GLint loc( GLuint p, const char * n )
{
	return f.getUniformLocation( p, n );
}
void u1i( GLuint p, const char * n, int v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform1i( l, v );
}
void u1f( GLuint p, const char * n, float v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform1f( l, v );
}
void u2f( GLuint p, const char * n, float a, float b )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform2f( l, a, b );
}
void u3f( GLuint p, const char * n, const float * v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform3f( l, v[0], v[1], v[2] );
}
void u4f( GLuint p, const char * n, const float * v )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform4f( l, v[0], v[1], v[2], v[3] );
}

GLuint tex2d( int w, int hgt, GLenum ifmt, GLenum fmt, GLenum typ, const void * data, bool linear )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, hgt, 0, fmt, typ, data );
	const GLint flt = linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClamp );
	glTexParameteri( GL_TEXTURE_2D, kMaxLevel, 0 );
	return t;
}

GLuint msTex( int w, int hgt, GLenum ifmt, int s )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( kT2DMS, t );
	f.texImage2DMultisample( kT2DMS, s, ifmt, w, hgt, GL_TRUE );
	glBindTexture( kT2DMS, 0 );
	return t;
}

GLuint fbo( GLenum colType, GLuint col, GLuint ds )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( kFb, b );
	f.framebufferTexture2D( kFb, kCol0, colType, col, 0 );
	if ( ds )
		f.framebufferTexture2D( kFb, kDsAtt, kT2DMS, ds, 0 );
	if ( f.checkFramebufferStatus( kFb ) != kComplete ) {
		std::printf( "SKYVIEW framebuffer incomplete\n" );
		return 0;
	}
	return b;
}

void bindUnit( int unit, GLenum type, GLuint t )
{
	f.activeTexture( kTex0 + GLenum( unit ) );
	glBindTexture( type, t );
}

const char * kTriVs = "#version 410 core\n"
	"void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); "
	"gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";
const char * kObjFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.25, 0.5, 0.75, 1.0 ); }\n";
const char * kUnpackFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ); }\n";

struct V3
{
	float x, y, z;
};
V3 dirAt( float azDeg, float elDeg )
{
	const float a = azDeg * 3.14159265f / 180.0f, e = elDeg * 3.14159265f / 180.0f;
	return { std::cos( e ) * std::cos( a ), std::cos( e ) * std::sin( a ), std::sin( e ) };
}
V3 cross( V3 a, V3 b )
{
	return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
V3 norm( V3 a )
{
	const float l = std::sqrt( a.x * a.x + a.y * a.y + a.z * a.z );
	return { a.x / l, a.y / l, a.z / l };
}

struct Job
{
	const char * name;
	float camAz, camEl, fov, jx, jy;
	float sunAz, sunEl, moonAz, moonEl;
	float sunK, moonK;
	int viewTransform;
	float exposure;
	float limb, discHue, sunSize, discAir;
	float floorC[3];
	float tintOn, recNight[2];
	float ins[3], dirIns[3], dirExp;
	int vp[4];  // 0 0 0 0 = the whole frame
};

// the GLOBAL block as std140 packs it (uniforms.glsl): viewMatrix mat3 at 0 (3 columns, stride 16), projection at 96
void globalBytes( const Job & j, int vw, int vh, std::uint8_t * out )
{
	std::memset( out, 0, size_t( kSkyGlobalBytes ) );
	float * fl = reinterpret_cast<float *>( out );
	const V3 fw = dirAt( j.camAz, j.camEl ), r = norm( cross( fw, { 0, 0, 1 } ) ), up = cross( r, fw );
	const V3 bk = { -fw.x, -fw.y, -fw.z };
	const float rows[3][3] = { { r.x, r.y, r.z }, { up.x, up.y, up.z }, { bk.x, bk.y, bk.z } };
	for ( int c = 0; c < 3; c++ )
		for ( int rr = 0; rr < 3; rr++ )
			fl[c * 4 + rr] = rows[rr][c];  // column c holds row rr's component c
	for ( int c = 0; c < 3; c++ )         // envMapRotation (unused by the sky): the identity
		fl[12 + c * 4 + c] = 1.0f;
	const float fy = 1.0f / std::tan( j.fov * 3.14159265f / 360.0f ), n = 0.1f, fa = 10000.0f;
	float * P = fl + 24;
	P[0] = fy * float( vh ) / float( vw );
	P[5] = fy;
	P[8] = j.jx;
	P[9] = j.jy;
	P[10] = ( fa + n ) / ( n - fa );
	P[11] = -1.0f;
	P[14] = 2.0f * fa * n / ( n - fa );
	for ( int i = 40; i < 84; i++ )       // the rest: light sources, scales, viewport (the sky reads none)
		fl[i] = 0.01f * float( i );
}

struct Halves
{
	size_t total = 0, diff = 0, past1 = 0, past4 = 0;
	int maxu = 0;
	const char * word() const { return total == 0 ? "FAIL" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
	bool ok() const { return total > 0 && maxu <= 4 && past1 * 1000 <= total; }
};

int ordered( std::uint16_t hv )
{
	return ( hv & 0x8000 ) ? -int( hv & 0x7FFF ) : int( hv );
}

Halves compare( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
{
	Halves v;
	if ( a.size() != b.size() || a.empty() )
		return v;
	v.total = a.size();
	for ( size_t i = 0; i < a.size(); i++ ) {
		const bool nanA = ( a[i] & 0x7C00 ) == 0x7C00 && ( a[i] & 0x3FF ), nanB = ( b[i] & 0x7C00 ) == 0x7C00 && ( b[i] & 0x3FF );
		int d = std::abs( ordered( a[i] ) - ordered( b[i] ) );
		if ( nanA || nanB )
			d = nanA == nanB ? 0 : 1 << 16;
		v.diff += d != 0;
		v.past1 += d > 1;
		v.past4 += d > 4;
		v.maxu = std::max( v.maxu, d );
	}
	return v;
}
}  // namespace sv

int main( int argc, char ** argv )
{
	using namespace sv;
	if ( argc < 2 ) {
		std::printf( "usage: skyview <glsl dir> [W H]\nSKYVIEW GATE FAILED\n" );
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
	wc.lpszClassName = "wwskyview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwskyview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwskyview", "sky view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "SKYVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
		std::printf( "SKYVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "frame %dx%d x%d, GL on %s\n", W, H, S, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );

	const GLuint pObj = program( kTriVs, kObjFs, "object" ), pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	const GLuint pSky = program( readFile( dir + "/cell_ao.vert.glsl" ), readFile( dir + "/atmo_sky.frag.glsl" ), "atmo_sky" );
	if ( !pObj || !pUnpack || !pSky ) {
		std::printf( "SKYVIEW GATE FAILED (GL programs)\n" );
		return 2;
	}
	// the global block: binding 0, its size must be the 336 bytes SkyRun copies
	{
		const GLuint bi = f.getUniformBlockIndex( pSky, "globalUniforms" );
		GLint size = 0;
		if ( bi != 0xFFFFFFFFu )
			f.getActiveUniformBlockiv( pSky, bi, kBlockSize, &size );
		std::printf( "global block %d bytes (SkyRun copies %d)\n", size, kSkyGlobalBytes );
		if ( size != kSkyGlobalBytes ) {
			std::printf( "SKYVIEW GATE FAILED (the global block's size)\n" );
			return 2;
		}
		f.uniformBlockBinding( pSky, bi, 0 );
	}
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );

	// the LUTs at the app's sizes (atmosphere.cpp: transmittance 256 x 64, MS 32 x 32, sky views 192 x 108), seeded smooth
	// fields with a little texel noise (the bilinear weights matter), uploaded as floats into RGBA16F
	std::mt19937 rng( 41 );
	std::uniform_real_distribution<float> ur( 0.0f, 1.0f );
	const int lw[4] = { 256, 32, 192, 192 }, lh[4] = { 64, 32, 108, 108 };
	GLuint lut[4] = {};
	for ( int k = 0; k < 4; k++ ) {
		std::vector<float> d( size_t( lw[k] ) * lh[k] * 4 );
		const float ph = 0.7f * float( k + 1 );
		for ( int y = 0; y < lh[k]; y++ )
			for ( int x = 0; x < lw[k]; x++ )
				for ( int c = 0; c < 4; c++ ) {
					const float u = float( x ) / float( lw[k] ), v = float( y ) / float( lh[k] );
					float val;
					if ( k == 0 )
						val = std::exp( -( 0.2f + 0.5f * float( c ) ) * ( 1.0f - u ) * ( 0.3f + v ) * 3.0f );
					else
						val = ( 0.3f + 0.25f * float( c ) ) * ( 1.2f + std::sin( u * 9.0f + ph + float( c ) ) * std::cos( v * 7.0f - ph ) )
							* ( k == 3 ? 0.02f : 1.0f );
					d[( size_t( y ) * lw[k] + x ) * 4 + size_t( c )] = val * ( 1.0f + 0.02f * ( ur( rng ) - 0.5f ) );
				}
		lut[k] = tex2d( lw[k], lh[k], kRgba16f, GL_RGBA, GL_FLOAT, d.data(), true );
	}
	static int keyHome[8];  // the shared twins' keys: their addresses (atmosphere.cpp uses its targets')

	// GL's reference target and the per-sample read-back target
	const GLuint refCol = msTex( W, H, kRgba16f, S ), refDs = msTex( W, H, kD24s8, S );
	const GLuint fbRef = fbo( kT2DMS, refCol, refDs );
	const int WS = W * S;
	const GLuint unp = tex2d( WS, H, kRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0 );
	if ( !fbRef || !fbUnp ) {
		std::printf( "SKYVIEW GATE FAILED (GL targets)\n" );
		return 2;
	}
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( kFb, fbUnp );
		glViewport( 0, 0, WS, H );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_STENCIL_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_SCISSOR_TEST );
		f.useProgram( pUnpack );
		bindUnit( 8, kT2DMS, msColor );
		u1i( pUnpack, "src", 8 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, kHalf, out.data() );
		return out;
	};
	auto clearFrame = [&]( GLuint fb ) {
		f.bindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glDisable( GL_SCISSOR_TEST );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.05f, 0.1f, 0.15f, 0.0f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
	};
	// an opaque object after the sky: depth 0.5 against the cleared 1, over a centre rectangle
	auto object = [&]( GLuint fb ) {
		f.bindFramebuffer( kFb, fb );
		glViewport( 0, 0, W, H );
		glEnable( GL_SCISSOR_TEST );
		glScissor( W / 3, H / 3, W / 3, H / 3 );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		f.useProgram( pObj );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_SCISSOR_TEST );
	};
	// the sky program's loose uniforms for a job (atmosphere.cpp sets these before its quad)
	float tint[17][3];
	for ( int i = 0; i < 17; i++ )
		for ( int c = 0; c < 3; c++ )
			tint[i][c] = 0.02f + 0.01f * float( i ) * ( 1.0f + 0.3f * float( c ) );
	auto setLoose = [&]( const Job & j, const int vp[4] ) {
		f.useProgram( pSky );
		u1i( pSky, "atTransTex", 0 );
		u1i( pSky, "atMsTex", 1 );
		u1i( pSky, "atSunView", 2 );
		u1i( pSky, "atMoonView", 3 );
		const float haze[3] = { 0.11f, 1.2f, 0.5f }, ray[3] = { 5.802e-3f, 13.558e-3f, 33.1e-3f }, oz[3] = { 0.65e-3f, 1.881e-3f, 0.085e-3f };
		const float alb[3] = { 0.97f, 0.96f, 0.95f }, spec[3] = { 0.8f, 1.0f, 1.27f };
		u3f( pSky, "atHaze", haze );
		u3f( pSky, "atRay", ray );
		u1f( pSky, "atMieS", 3.996e-3f );
		u1f( pSky, "atMieE", 4.40e-3f );
		u3f( pSky, "atOzone", oz );
		u1f( pSky, "atG", 0.7f );
		u3f( pSky, "atHazeAlbedo", alb );
		u3f( pSky, "atHazeSpec", spec );
		u1f( pSky, "atMsOld", 0.0f );
		u2f( pSky, "atViewSize", float( lw[2] ), float( lh[2] ) );
		const float rect[4] = { float( vp[0] ), float( vp[1] ), 1.0f / float( vp[2] ), 1.0f / float( vp[3] ) };
		u4f( pSky, "atRect", rect );
		u1f( pSky, "atCamR", 6360.02f );
		const V3 sw = dirAt( j.sunAz, j.sunEl ), mw = dirAt( j.moonAz, j.moonEl );
		const float swf[3] = { sw.x, sw.y, sw.z }, mwf[3] = { mw.x, mw.y, mw.z };
		u3f( pSky, "atSunW", swf );
		u3f( pSky, "atMoonW", mwf );
		u1f( pSky, "atSunK", j.sunK );
		u1f( pSky, "atMoonK", j.moonK );
		u1f( pSky, "atLimb", j.limb );
		u1f( pSky, "atDiscCap", 200.0f );
		u1f( pSky, "atSunSize", j.sunSize );
		u1f( pSky, "atDiscHue", j.discHue );
		u1f( pSky, "atDiscAir", j.discAir );
		u1f( pSky, "sceneExposure", j.exposure );
		u1i( pSky, "viewTransform", j.viewTransform );
		u1f( pSky, "atTintOn", j.tintOn );
		for ( int i = 0; i < 17; i++ ) {
			const std::string n = "atTint[" + std::to_string( i ) + "]";
			u3f( pSky, n.c_str(), tint[i] );
		}
		u2f( pSky, "atRecNight", j.recNight[0], j.recNight[1] );
		u2f( pSky, "atMsSize", 32.0f, 32.0f );
		u1f( pSky, "atMsOn", 1.0f );
		u1f( pSky, "atGround", 1.0f );
		u1f( pSky, "atPerPixel", 0.0f );
		u3f( pSky, "atFloor", j.floorC );
		u3f( pSky, "atInscatter", j.ins );
		u3f( pSky, "atDirIns", j.dirIns );
		u3f( pSky, "atDirL", swf );
		u1f( pSky, "atDirExp", j.dirExp );
	};
	// the loose block packed from the GL program's own values (a field the program dropped stays zero)
	auto packLoose = [&]( std::uint8_t * out, const char * dropField ) {
		std::memset( out, 0, size_t( kSkyLooseBytes ) );
		for ( const SkyField & fd : kSkyFields ) {
			if ( dropField && std::strcmp( fd.name, dropField ) == 0 )
				continue;
			for ( int i = 0; i < fd.count; i++ ) {
				const std::string n = fd.count > 1 ? std::string( fd.name ) + "[" + std::to_string( i ) + "]" : std::string( fd.name );
				const GLint l = loc( pSky, n.c_str() );
				if ( l < 0 )
					continue;
				std::uint8_t * at = out + fd.offset + i * 16;
				if ( fd.isInt ) {
					GLint iv[4] = {};
					f.getUniformiv( pSky, l, iv );
					std::memcpy( at, iv, size_t( fd.words ) * 4 );
				} else {
					GLfloat fv[4] = {};
					f.getUniformfv( pSky, l, fv );
					std::memcpy( at, fv, size_t( fd.words ) * 4 );
				}
			}
		}
	};

	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "SKYVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) ) {
		std::printf( "SKYVIEW GATE FAILED (toneFrame: %s)\n", why.c_str() );
		return 2;
	}
	const GLuint fbFrame = fbo( kT2DMS, fc, fds );
	if ( !fbFrame ) {
		std::printf( "SKYVIEW GATE FAILED (the shared frame's FBO)\n" );
		return 2;
	}
	auto vpOf = [&]( const Job & j, int vp[4] ) {
		const bool whole = j.vp[2] == 0;
		vp[0] = whole ? 0 : j.vp[0];
		vp[1] = whole ? 0 : j.vp[1];
		vp[2] = whole ? W : j.vp[2];
		vp[3] = whole ? H : j.vp[3];
	};
	enum Red { kNone, kField, kLutSwap, kOrder };
	auto runVk = [&]( const Job & j, Red red, std::vector<std::uint16_t> & out ) -> bool {
		int vp[4];
		vpOf( j, vp );
		clearFrame( fbFrame );
		if ( red == kOrder )
			object( fbFrame );
		setLoose( j, vp );
		WwVkPresent::SkyRun t;
		t.w = W;
		t.h = H;
		std::memcpy( t.vp, vp, sizeof( t.vp ) );
		t.color = fc;
		for ( int k = 0; k < 4; k++ ) {
			const int kk = red == kLutSwap && k >= 2 ? 5 - k : k;
			t.lut[k] = lut[kk];
			t.lutW[k] = lw[kk];
			t.lutH[k] = lh[kk];
			t.lutKey[k] = reinterpret_cast<std::uintptr_t>( &keyHome[kk] );
		}
		globalBytes( j, vp[2], vp[3], t.global );
		packLoose( t.loose, red == kField ? "atSunW" : nullptr );
		f.bindFramebuffer( kFb, fbFrame );
		glViewport( vp[0], vp[1], vp[2], vp[3] );
		if ( !WwVkPresent::skyRun( t, why ) ) {
			std::printf( "SKYVIEW %s: skyRun refused: %s\n", j.name, why.c_str() );
			return false;
		}
		if ( red != kOrder )
			object( fbFrame );
		out = readSamples( fc );
		return glGetError() == GL_NO_ERROR;
	};
	// atmosphere.cpp step 3's GL quad (depth, blend, cull, scissor and stencil off), into GL's own target
	auto runGl = [&]( const Job & j, std::vector<std::uint16_t> & out ) {
		int vp[4];
		vpOf( j, vp );
		clearFrame( fbRef );
		std::uint8_t g[kSkyGlobalBytes];
		globalBytes( j, vp[2], vp[3], g );
		f.bindBuffer( kUbo, ubo );
		f.bufferData( kUbo, kSkyGlobalBytes, g, kStatic );
		f.bindBufferBase( kUbo, 0, ubo );
		setLoose( j, vp );
		for ( int k = 0; k < 4; k++ )
			bindUnit( k, GL_TEXTURE_2D, lut[k] );
		f.bindFramebuffer( kFb, fbRef );
		glViewport( vp[0], vp[1], vp[2], vp[3] );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, quad );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx );
		f.disableVertexAttribArray( 0 );
		object( fbRef );
		out = readSamples( refCol );
		return glGetError() == GL_NO_ERROR;
	};

	const Job jobs[3] = {
		// day: the sun 30 deg up in view, limb, one-scale disc at 4x size, linear output (the HDR frame's 3)
		{ "day", 8.0f, 22.0f, 55.0f, 0.0004f, -0.0003f, 0.0f, 30.0f, 200.0f, 40.0f, 3.0f, 0.0f, 3, 1.0f, 1.0f, 1.0f, 4.0f, 0.8f,
			{ 0, 0, 0 }, 0.0f, { 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 1.0f, { 0, 0, 0, 0 } },
		// night: the sun below, the moon's LUT, the floor, the weather tint and the record's dome, sRGB output
		{ "night", 120.0f, 5.0f, 70.0f, 0.0f, 0.0f, 10.0f, -20.0f, 130.0f, 35.0f, 1.0f, 0.5f, 0, 0.6f, 0.0f, 0.0f, 1.0f, 0.0f,
			{ 0.02f, 0.03f, 0.05f }, 1.0f, { 0.3f, 0.5f }, { 0, 0, 0 }, { 0, 0, 0 }, 1.0f, { 0, 0, 0, 0 } },
		// inscatter: the overrides' lobe, an offset smaller viewport (atRect + the pass's viewport), AgX output
		{ "inscatter", -40.0f, -3.0f, 40.0f, -0.0002f, 0.0005f, -30.0f, 12.0f, 90.0f, 20.0f, 2.0f, 0.1f, 1, 0.8f, 0.0f, 1.0f, 1.0f,
			1.0f, { 0, 0, 0 }, 0.0f, { 0, 0 }, { 0.05f, 0.04f, 0.03f }, { 0.2f, 0.15f, 0.1f }, 8.0f, { 0, 0, 0, 0 } },
	};
	Job jobs2[3] = { jobs[0], jobs[1], jobs[2] };
	jobs2[2].vp[0] = W / 8;
	jobs2[2].vp[1] = H / 6;
	jobs2[2].vp[2] = W / 2;
	jobs2[2].vp[3] = H / 2;
	bool ok = true;
	std::vector<std::uint16_t> refs[3];
	for ( int ji = 0; ji < 3; ji++ ) {
		const Job & j = jobs2[ji];
		std::vector<std::uint16_t> vkOut;
		if ( !runVk( j, kNone, vkOut ) || !runGl( j, refs[ji] ) ) {
			std::printf( "SKYVIEW %s FAIL (a run failed)\n", j.name );
			ok = false;
			continue;
		}
		const Halves v = compare( vkOut, refs[ji] );
		if ( std::getenv( "SKYVIEW_DUMP" ) ) {  // int32 W*S, H, then the halves: vk_<job>.bin and gl_<job>.bin
			for ( int side = 0; side < 2; side++ ) {
				const std::vector<std::uint16_t> & b = side ? refs[ji] : vkOut;
				std::ofstream o( dir + ( side ? "/gl_" : "/vk_" ) + j.name + ".bin", std::ios::binary );
				const std::int32_t hd[2] = { WS, H };
				o.write( reinterpret_cast<const char *>( hd ), sizeof( hd ) );
				o.write( reinterpret_cast<const char *>( b.data() ), std::streamsize( b.size() * 2 ) );
			}
		}
		std::printf( "SKYVIEW %s %s (%zu of %zu halves differ, %zu past 1 ulp, %zu past 4, max %d ulp; view transform %d)\n", j.name,
			v.word(), v.diff, v.total, v.past1, v.past4, v.maxu, j.viewTransform );
		ok = ok && v.ok();
	}
	// the reds against the green references: each must FAIL
	const struct
	{
		const char * name;
		Red red;
		int job;
	} reds[3] = { { "field (atSunW dropped from the loose block)", kField, 0 }, { "lutswap (the sun and moon views swapped)", kLutSwap, 1 },
		{ "order (the object drawn before the Vulkan sky)", kOrder, 0 } };
	for ( const auto & r : reds ) {
		std::vector<std::uint16_t> vkOut;
		const bool ran = runVk( jobs2[r.job], r.red, vkOut );
		const Halves v = compare( vkOut, refs[r.job] );
		const bool redFails = !ran || !v.ok();
		std::printf( "SKYVIEW RED %s %s (%zu halves differ, %zu past 4 ulp, max %d; must FAIL)\n", redFails ? "FAIL" : v.word(), r.name,
			v.diff, v.past4, v.maxu );
		ok = ok && redFails;
	}
	const WwVkPresent::SkyStat st = WwVkPresent::takeSkyStat();
	std::printf( "SKYVIEW stat: %d runs on Vulkan, %d failed (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in "
		"%.1f ms\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
	f.bindFramebuffer( kFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "SKYVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
