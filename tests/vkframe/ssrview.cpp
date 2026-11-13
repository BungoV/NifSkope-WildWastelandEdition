// Lane VKLIGHT2 (2026-11-13, row L4): the reflections on the present device (vkpresent.cpp ssrRun, built in here with
// WW_VKPRESENT_NO_QT: the shipped code) against GL's own cell_ssr / cell_ssrwater programs (expanded by ssrview_expand.py),
// drawn the way cellssr.cpp draws them: stage 5 (the bounce's donor, alpha kept) into the scene, then stages 1-4 at half size.
// Usage: ssrview <glsl dir> [W H]
// Inputs (both arms the same bytes): a view-space g-buffer of a floor and two walls under a yawed, pitched camera (RGBA32F,
// bottom row first, alpha the view depth, 0 = sky), its min-of-2x2 depth mips 1..4 (R32F, floor halving), the scene (RGBA32F,
// alpha the reflection flag on the floor), a water ray band, the previous frame (RGBA16F) and a small camera move.
// Vulkan's targets are made with sharedTarget as cellssr.cpp's ssrPlace makes them; GL's are its own, as ssrAlloc makes them.
// Prints `SSRVIEW <job> <image> ...`, `SSRVIEW <job> EXACT|BOUND|FAIL`, `SSRVIEW RED <name> ...` (each must FAIL) and
// `SSRVIEW GATE OK|FAILED`.
// BOUND (the offline row's bar, tests/vklight light.sh ssr jobs `ulp1`, stated before the first run, never widened): every
// float within 1 ulp of GL's. The new parts (stage 5, the previous frame, the water look) take the same bar.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rv
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

constexpr GLenum kFb = 0x8D40, kCol0 = 0x8CE0, kClamp = 0x812F, kTex0 = 0x84C0, kFrag = 0x8B30, kVert = 0x8B31;
constexpr GLenum kCompiled = 0x8B81, kLinked = 0x8B82, kComplete = 0x8CD5, kMaxLevel = 0x813D, kRed = 0x1903;

struct Fns
{
	GLuint( WINAPI * createShader )( GLenum ) = nullptr;
	void( WINAPI * shaderSource )( GLuint, GLsizei, const char * const *, const GLint * ) = nullptr;
	void( WINAPI * compileShader )( GLuint ) = nullptr;
	void( WINAPI * getShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getShaderInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	GLuint( WINAPI * createProgram )() = nullptr;
	void( WINAPI * attachShader )( GLuint, GLuint ) = nullptr;
	void( WINAPI * bindAttribLocation )( GLuint, GLuint, const char * ) = nullptr;
	void( WINAPI * linkProgram )( GLuint ) = nullptr;
	void( WINAPI * getProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * getProgramInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( WINAPI * useProgram )( GLuint ) = nullptr;
	GLint( WINAPI * getUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * uniform2i )( GLint, GLint, GLint ) = nullptr;
	void( WINAPI * uniform2f )( GLint, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform3f )( GLint, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform4f )( GLint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniformMatrix3fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
	void( WINAPI * activeTexture )( GLenum ) = nullptr;
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
		&& load( f.attachShader, "glAttachShader" ) && load( f.bindAttribLocation, "glBindAttribLocation" )
		&& load( f.linkProgram, "glLinkProgram" ) && load( f.getProgramiv, "glGetProgramiv" )
		&& load( f.getProgramInfoLog, "glGetProgramInfoLog" ) && load( f.useProgram, "glUseProgram" )
		&& load( f.getUniformLocation, "glGetUniformLocation" ) && load( f.uniform1i, "glUniform1i" )
		&& load( f.uniform2i, "glUniform2i" ) && load( f.uniform2f, "glUniform2f" ) && load( f.uniform3f, "glUniform3f" )
		&& load( f.uniform4f, "glUniform4f" ) && load( f.uniformMatrix3fv, "glUniformMatrix3fv" )
		&& load( f.activeTexture, "glActiveTexture" ) && load( f.genFramebuffers, "glGenFramebuffers" )
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
		std::printf( "SSRVIEW compile failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs, const char * tag )
{
	if ( vs.empty() || fs.empty() ) {
		std::printf( "SSRVIEW no source (%s)\n", tag );
		return 0;
	}
	const GLuint a = shader( kVert, vs, tag ), b = shader( kFrag, fs, tag );
	if ( !a || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.bindAttribLocation( p, 0, "vertexPosition" );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, kLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "SSRVIEW link failed (%s): %.1500s\n", tag, log.data() );
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
void u2i( GLuint p, const char * n, int a, int b )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform2i( l, a, b );
}
void u2f( GLuint p, const char * n, float a, float b )
{
	const GLint l = loc( p, n );
	if ( l >= 0 )
		f.uniform2f( l, a, b );
}

void params( GLuint t, bool linear )
{
	glBindTexture( GL_TEXTURE_2D, t );
	const GLint flt = linear ? GL_LINEAR : GL_NEAREST;
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kClamp );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kClamp );
}

GLuint tex2d( int w, int hgt, GLenum ifmt, GLenum fmt, const float * data, bool linear )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, hgt, 0, fmt, GL_FLOAT, data );
	params( t, linear );
	glTexParameteri( GL_TEXTURE_2D, kMaxLevel, 0 );
	return t;
}

GLuint fboOf( GLuint t )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( kFb, b );
	f.framebufferTexture2D( kFb, kCol0, GL_TEXTURE_2D, t, 0 );
	if ( f.checkFramebufferStatus( kFb ) != kComplete ) {
		std::printf( "SSRVIEW framebuffer incomplete\n" );
		return 0;
	}
	return b;
}

std::vector<float> readTex( GLuint t, int w, int hgt )
{
	std::vector<float> out( size_t( w ) * size_t( hgt ) * 4 );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, out.data() );
	return out;
}

void upload( GLuint t, int w, int hgt, const std::vector<float> & d )
{
	glBindTexture( GL_TEXTURE_2D, t );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, w, hgt, GL_RGBA, GL_FLOAT, d.data() );
}

struct Floats
{
	size_t total = 0, diff = 0, past1 = 0, nz = 0;
	long long maxu = 0;
	const char * word() const { return total == 0 ? "FAIL" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
	bool ok() const { return total > 0 && past1 == 0; }
};

long long ordered( float x )
{
	std::uint32_t u;
	std::memcpy( &u, &x, 4 );
	return ( u & 0x80000000u ) ? -( long long )( u & 0x7FFFFFFFu ) : ( long long )u;
}

Floats compare( const std::vector<float> & a, const std::vector<float> & b )
{
	Floats v;
	if ( a.size() != b.size() || a.empty() )
		return v;
	v.total = a.size();
	for ( size_t i = 0; i < a.size(); i++ ) {
		const bool nanA = a[i] != a[i], nanB = b[i] != b[i];
		long long d = std::llabs( ordered( a[i] ) - ordered( b[i] ) );
		if ( nanA || nanB )
			d = nanA == nanB ? 0 : 1ll << 40;
		v.diff += d != 0;
		v.past1 += d > 1;
		v.maxu = std::max( v.maxu, d );
		v.nz += b[i] != 0.0f;
	}
	return v;
}

float hashf( int x, int y, int k )
{
	std::uint32_t h = std::uint32_t( x ) * 73856093u ^ std::uint32_t( y ) * 19349663u ^ std::uint32_t( k ) * 83492791u;
	h ^= h >> 13;
	h *= 0x5bd1e995u;
	h ^= h >> 15;
	return float( h & 0xFFFFFF ) / float( 0x1000000 );
}
}  // namespace rv

int main( int argc, char ** argv )
{
	using namespace rv;
	if ( argc < 2 ) {
		std::printf( "usage: ssrview <glsl dir> [W H]\nSSRVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	const int W = argc > 2 ? std::atoi( argv[2] ) : 641, H = argc > 3 ? std::atoi( argv[3] ) : 361;
	const int hw = ( W + 1 ) / 2, hh = ( H + 1 ) / 2;
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwssrview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwssrview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwssrview", "ssr view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "SSRVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
		std::printf( "SSRVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "frame %dx%d, half %dx%d, GL on %s\n", W, H, hw, hh, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	const std::string vs = readFile( dir + "/cell_ssr.vert.glsl" );
	const GLuint pBase = program( vs, readFile( dir + "/cell_ssr.frag.glsl" ), "cell_ssr" );
	const GLuint pWater = program( vs, readFile( dir + "/cell_ssrwater.frag.glsl" ), "cell_ssrwater" );
	if ( !pBase || !pWater ) {
		std::printf( "SSRVIEW GATE FAILED (GL programs)\n" );
		return 2;
	}

	// ---- the inputs. The camera: yaw 1.1, pitch -0.35 (tests/vklight light_gl_ref.py ssr_rot); rot row j = view axis j in the world
	const float yaw = 1.1f, pitch = -0.35f;
	const float cy = std::cos( yaw ), sy = std::sin( yaw ), cp = std::cos( pitch ), sp = std::sin( pitch );
	const float fwd[3] = { cy * cp, sy * cp, sp }, right[3] = { sy, -cy, 0.0f };
	const float up[3] = { right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2], right[0] * fwd[1] - right[1] * fwd[0] };
	float rot[9];
	for ( int k = 0; k < 3; k++ ) {
		rot[0 * 3 + k] = right[k];
		rot[1 * 3 + k] = up[k];
		rot[2 * 3 + k] = -fwd[k];
	}
	const float fy = 1.0f / std::tan( 30.0f * 3.14159265f / 180.0f );
	const float P00 = fy * float( H ) / float( W ), P11 = fy, nearZ = 15.0f, farZ = 80000.0f;
	const float ssrK[4] = { 1.0f, 0.2f, 2.0f, 1.0f };
	// planes in the world (camera at the origin): the floor z = -200 (up), a wall y = 3000 (facing -y), a wall x = 2500 (facing -x)
	struct Plane
	{
		float n[3], d;  // dot( n, Pw ) = d; the surface faces -n for the walls, n for the floor
		float face[3];
	};
	const Plane planes[3] = { { { 0, 0, 1 }, -200.0f, { 0, 0, 1 } }, { { 0, 1, 0 }, 3000.0f, { 0, -1, 0 } }, { { 1, 0, 0 }, 2500.0f, { -1, 0, 0 } } };
	std::vector<float> gbuf( size_t( W ) * H * 4, 0.0f ), scene( size_t( W ) * H * 4, 0.0f ), wray( size_t( W ) * H * 4, 0.0f );
	std::vector<float> prev( size_t( W ) * H * 4, 0.0f ), depthTop( size_t( W ) * H, 1.0e6f );
	for ( int yb = 0; yb < H; yb++ ) {
		const int yt = H - 1 - yb;
		for ( int x = 0; x < W; x++ ) {
			const float u = ( float( x ) + 0.5f ) / float( W ), v = ( float( yt ) + 0.5f ) / float( H );
			const float d[3] = { ( 2.0f * u - 1.0f ) / P00, ( 1.0f - 2.0f * v ) / P11, -1.0f };
			float best = 1.0e30f;
			int hit = -1;
			for ( int p = 0; p < 3; p++ ) {
				float a = 0.0f;  // dot( n, R^T d ) = sum_j d_j dot( n, rot_j )
				for ( int j = 0; j < 3; j++ )
					a += d[j] * ( planes[p].n[0] * rot[j * 3 + 0] + planes[p].n[1] * rot[j * 3 + 1] + planes[p].n[2] * rot[j * 3 + 2] );
				if ( std::fabs( a ) < 1e-6f )
					continue;
				const float t = planes[p].d / a;
				if ( t > nearZ && t < 60000.0f && t < best ) {
					best = t;
					hit = p;
				}
			}
			float * g = &gbuf[( size_t( yb ) * W + x ) * 4];
			float * c = &scene[( size_t( yb ) * W + x ) * 4];
			float * pr = &prev[( size_t( yb ) * W + x ) * 4];
			pr[0] = 0.4f + 0.4f * std::sin( float( x ) / 11.0f );
			pr[1] = 1.5f * float( yb ) / float( H );
			pr[2] = float( ( x / 24 + yb / 24 ) % 2 ) * 2.0f;
			pr[3] = 1.0f;
			if ( hit < 0 ) {  // sky: no depth, a bright gradient
				c[0] = 0.6f;
				c[1] = 0.8f + v;
				c[2] = 3.0f;
				continue;
			}
			float nv[3];  // the face normal in the view: n_v[j] = dot( rot_j, face )
			for ( int j = 0; j < 3; j++ )
				nv[j] = rot[j * 3 + 0] * planes[hit].face[0] + rot[j * 3 + 1] * planes[hit].face[1] + rot[j * 3 + 2] * planes[hit].face[2];
			const float wob = 0.03f * ( hashf( x, yb, 1 ) - 0.5f );
			nv[0] += wob;
			const float l = std::sqrt( nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2] );
			g[0] = nv[0] / l;
			g[1] = nv[1] / l;
			g[2] = nv[2] / l;
			g[3] = best;
			depthTop[size_t( yt ) * W + x] = best;
			float Pw[3] = {};
			for ( int k = 0; k < 3; k++ )
				for ( int j = 0; j < 3; j++ )
					Pw[k] += best * d[j] * rot[j * 3 + k];
			const int cx = int( std::floor( Pw[0] / 150.0f ) ), cyy = int( std::floor( Pw[1] / 150.0f ) ), cz = int( std::floor( Pw[2] / 150.0f ) );
			const float chk = float( ( ( cx + cyy + cz ) % 2 + 2 ) % 2 );
			c[0] = hit == 1 ? 4.0f * chk + 0.2f : 0.3f + 0.2f * chk;
			c[1] = hit == 2 ? 2.5f * chk + 0.1f : 0.25f;
			c[2] = hit == 0 ? 0.15f + 0.5f * chk : 0.6f;
			c[3] = hit == 0 && ( ( x / 40 + yb / 30 ) % 5 != 0 ) ? 1.0f : 0.0f;
			// the water band (the lower quarter): a rippled reflection ray of the floor's, some refused (zero)
			if ( hit == 0 && yb < H / 4 ) {
				float * w = &wray[( size_t( yb ) * W + x ) * 4];
				if ( hashf( x, yb, 7 ) >= 0.05f ) {
					float nn[3] = { g[0] + 0.08f * ( hashf( x, yb, 2 ) - 0.5f ), g[1], g[2] + 0.08f * ( hashf( x, yb, 3 ) - 0.5f ) };
					const float nl = std::sqrt( nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2] );
					const float dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + 1.0f );
					const float i3[3] = { d[0] / dl, d[1] / dl, d[2] / dl };
					const float dn = ( i3[0] * nn[0] + i3[1] * nn[1] + i3[2] * nn[2] ) / ( nl * nl );
					for ( int k = 0; k < 3; k++ )
						w[k] = i3[k] - 2.0f * dn * nn[k];
				}
				w[3] = best;
			}
		}
	}
	// the depth mips 1..4 (top row first, as the march reads them): min of 2 x 2, floor halving, the sky far
	int mipW[5] = { W }, mipH[5] = { H };
	std::vector<float> mips[5];
	mips[0] = depthTop;
	GLuint mipTex[5] = {};
	for ( int m = 1; m < 5; m++ ) {
		mipW[m] = std::max( mipW[m - 1] / 2, 1 );
		mipH[m] = std::max( mipH[m - 1] / 2, 1 );
		mips[m].assign( size_t( mipW[m] ) * mipH[m], 0.0f );
		for ( int y = 0; y < mipH[m]; y++ )
			for ( int x = 0; x < mipW[m]; x++ ) {
				float z = 1.0e30f;
				for ( int dy = 0; dy < 2; dy++ )
					for ( int dx = 0; dx < 2; dx++ )
						z = std::min( z, mips[m - 1][size_t( std::min( 2 * y + dy, mipH[m - 1] - 1 ) ) * mipW[m - 1] + size_t( std::min( 2 * x + dx, mipW[m - 1] - 1 ) )] );
				mips[m][size_t( y ) * mipW[m] + x] = z;
			}
		mipTex[m] = tex2d( mipW[m], mipH[m], kGlR32f, kRed, mips[m].data(), false );
	}
	const GLuint gbufTex = tex2d( W, H, kRgba32f, GL_RGBA, gbuf.data(), false );
	const GLuint prevTex = tex2d( W, H, kRgba16f, GL_RGBA, prev.data(), true );
	// the previous view: a small turn about the view's y and a step (row-major M, as ssrPrevMT gives it)
	const float ta = 0.012f, ca = std::cos( ta ), sa = std::sin( ta );
	const float prevM[9] = { ca, 0.002f, sa, -0.001f, 1.0f, 0.003f, -sa, 0.0f, ca };
	const float prevT[3] = { 6.0f, -3.5f, 9.0f };
	const float prevProj[2] = { P00 * 1.01f, P11 * 1.01f };

	// ---- GL's targets (ssrAlloc) and Vulkan's (ssrPlace: sharedTarget, keyed by the target)
	enum Img { kScene, kWray, kRay, kRaw, kAcross, kFin, kImgs };
	const char * imgName[kImgs] = { "scene", "wray", "ray", "raw", "across", "fin" };
	const bool imgLinear[kImgs] = { true, false, false, true, true, true };
	const int imgW[kImgs] = { W, W, hw, hw, hw, hw }, imgH[kImgs] = { H, H, hh, hh, hh, hh };
	GLuint glT[kImgs] = {}, glF[kImgs] = {}, vkT[kImgs] = {};
	static char keyHome[kImgs + 3];
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "SSRVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	for ( int i = 0; i < kImgs; i++ ) {
		glT[i] = tex2d( imgW[i], imgH[i], kRgba32f, GL_RGBA, nullptr, imgLinear[i] );
		glF[i] = fboOf( glT[i] );
		if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keyHome[i] ), 0, imgW[i], imgH[i], 1, kRgba32f, vkT[i], why ) ) {
			std::printf( "SSRVIEW GATE FAILED (sharedTarget %s: %s)\n", imgName[i], why.c_str() );
			return 2;
		}
		params( vkT[i], imgLinear[i] );
		if ( !glF[i] )
			return 2;
	}

	struct Job
	{
		const char * name;
		bool water, look, prevOn, stage5, march;
		int ssrRed;
	};
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	auto drawQuad = [&]() {
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, quad );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, idx );
		f.disableVertexAttribArray( 0 );
	};
	auto seed = [&]( const GLuint * t, const Job & j ) {
		upload( t[kScene], W, H, scene );
		upload( t[kWray], W, H, j.water ? wray : std::vector<float>( wray.size(), 0.0f ) );
		const std::vector<float> zero( size_t( hw ) * hh * 4, 0.0f );
		for ( int i = kRay; i < kImgs; i++ )
			upload( t[i], hw, hh, zero );
	};
	// cellssr.cpp's GL path: the units, the uniforms, stage 5 (alpha kept), then pass( 1..4 )
	auto runGl = [&]( const Job & j, std::vector<float> out[kImgs] ) {
		seed( glT, j );
		const GLuint prog = j.look ? pWater : pBase;
		f.useProgram( prog );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );
		glDisable( GL_BLEND );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glDepthMask( GL_FALSE );
		const GLuint bound[9] = { gbufTex, mipTex[1], mipTex[2], mipTex[3], mipTex[4], glT[kScene], 0, j.water ? glT[kWray] : 0u, j.prevOn ? prevTex : 0u };
		const int nBound = j.prevOn ? 9 : 8;
		for ( int u = 0; u < nBound; u++ ) {
			f.activeTexture( kTex0 + GLenum( u ) );
			glBindTexture( GL_TEXTURE_2D, bound[u] );
		}
		u1i( prog, "gbuf", 0 );
		u1i( prog, "zMip1", 1 );
		u1i( prog, "zMip2", 2 );
		u1i( prog, "zMip3", 3 );
		u1i( prog, "zMip4", 4 );
		u1i( prog, "scene", 5 );
		u1i( prog, "src", 6 );
		u1i( prog, "waterRay", 7 );
		u1i( prog, "haveWaterRay", j.water ? 1 : 0 );
		u1i( prog, "opaqueRays", 1 );
		u2i( prog, "fullSize", W, H );
		u2i( prog, "halfSize", hw, hh );
		u2i( prog, "mipSize[0]", W, H );
		for ( int m = 1; m < 5; m++ ) {
			const std::string n = "mipSize[" + std::to_string( m ) + "]";
			u2i( prog, n.c_str(), mipW[m], mipH[m] );
		}
		f.uniformMatrix3fv( loc( prog, "viewToWorld" ), 1, GL_FALSE, rot );
		u2f( prog, "proj", P00, P11 );
		f.uniform4f( loc( prog, "ssrK" ), ssrK[0], ssrK[1], ssrK[2], ssrK[3] );
		u2f( prog, "ssrClip", nearZ, farZ );
		u1i( prog, "ssrRed", j.ssrRed );
		u1i( prog, "ssrPrev", j.prevOn ? 1 : 0 );
		if ( j.prevOn ) {
			f.uniformMatrix3fv( loc( prog, "ssrPrevRot" ), 1, GL_TRUE, prevM );
			f.uniform3f( loc( prog, "ssrPrevT" ), prevT[0], prevT[1], prevT[2] );
			u2f( prog, "ssrPrevProj", prevProj[0], prevProj[1] );
			u1i( prog, "prevScene", 8 );
		}
		if ( j.stage5 ) {
			f.activeTexture( kTex0 + 5 );
			glBindTexture( GL_TEXTURE_2D, 0 );
			f.bindFramebuffer( kFb, glF[kScene] );
			glViewport( 0, 0, W, H );
			glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
			u1i( prog, "ssrStage", 5 );
			drawQuad();
			glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			glBindTexture( GL_TEXTURE_2D, glT[kScene] );
		}
		if ( j.march ) {
			const int outs[4] = { kRay, kRaw, kAcross, kFin }, ins[4] = { -1, kRay, kRaw, kAcross };
			for ( int s = 0; s < 4; s++ ) {
				f.activeTexture( kTex0 + 6 );
				glBindTexture( GL_TEXTURE_2D, ins[s] >= 0 ? glT[ins[s]] : 0u );
				u1i( prog, "ssrStage", s + 1 );
				f.bindFramebuffer( kFb, glF[outs[s]] );
				glViewport( 0, 0, hw, hh );
				drawQuad();
			}
		}
		f.useProgram( 0 );
		for ( int u = 0; u < 9; u++ ) {
			f.activeTexture( kTex0 + GLenum( u ) );
			glBindTexture( GL_TEXTURE_2D, 0 );
		}
		f.activeTexture( kTex0 );
		f.bindFramebuffer( kFb, 0 );
		for ( int i = 0; i < kImgs; i++ )
			out[i] = readTex( glT[i], imgW[i], imgH[i] );
		return glGetError() == GL_NO_ERROR;
	};
	// cellssr.cpp's Vulkan branch: the same SsrRun it fills
	auto runVk = [&]( const Job & j, int red, std::vector<float> out[kImgs] ) {
		seed( vkT, j );
		WwVkPresent::SsrRun v;
		v.W = W; v.H = H; v.hw = hw; v.hh = hh;
		v.gbuf = gbufTex;
		for ( int m = 1; m < 5; m++ ) {
			v.mip[m] = mipTex[m];
			v.mipW[m] = mipW[m];
			v.mipH[m] = mipH[m];
		}
		v.gbufKey = reinterpret_cast<std::uintptr_t>( &keyHome[kImgs] );
		v.atlasKey = reinterpret_cast<std::uintptr_t>( &keyHome[kImgs + 1] );
		v.prevKey = reinterpret_cast<std::uintptr_t>( &keyHome[kImgs + 2] );
		v.prev = j.prevOn ? prevTex : 0;
		v.prevFmt = j.prevOn ? kRgba16f : 0;
		unsigned * names[kImgs] = { &v.scene, &v.wray, &v.ray, &v.raw, &v.across, &v.fin };
		std::uintptr_t * keys[kImgs] = { &v.sceneKey, &v.wrayKey, &v.rayKey, &v.rawKey, &v.acrossKey, &v.finKey };
		for ( int i = 0; i < kImgs; i++ ) {
			*names[i] = vkT[i];
			*keys[i] = reinterpret_cast<std::uintptr_t>( &keyHome[i] );
		}
		v.haveWater = j.water;
		v.waterLook = j.look;
		v.prevOn = j.prevOn;
		v.stage5 = j.stage5;
		v.march = j.march;
		const float c0[4] = { float( W ), float( H ), float( hw ), float( hh ) }, c1[4] = { P00, P11, nearZ, farZ };
		for ( int k = 0; k < 4; k++ ) {
			v.c[0][k] = c0[k];
			v.c[1][k] = c1[k];
			v.c[2][k] = ssrK[k];
		}
		for ( int jj = 0; jj < 3; jj++ )
			for ( int k = 0; k < 3; k++ )
				v.c[3 + jj][k] = rot[jj * 3 + k];
		v.i[0] = j.ssrRed;
		v.i[1] = j.water ? 1 : 0;
		v.i[2] = 1;
		std::memcpy( v.prevM, prevM, sizeof( prevM ) );
		std::memcpy( v.prevT, prevT, sizeof( prevT ) );
		std::memcpy( v.prevProj, prevProj, sizeof( prevProj ) );
		v.red = red;
		if ( !WwVkPresent::ssrRun( v, why ) ) {
			std::printf( "SSRVIEW %s: ssrRun refused: %s\n", j.name, why.c_str() );
			return false;
		}
		for ( int i = 0; i < kImgs; i++ )
			out[i] = readTex( vkT[i], imgW[i], imgH[i] );
		return glGetError() == GL_NO_ERROR;
	};
	// the images a job writes: the scene after stage 5, the four half-size targets after a march
	auto judge = [&]( const Job & j, std::vector<float> a[kImgs], std::vector<float> b[kImgs], bool print, const char * tag ) {
		bool ok = true;
		size_t diff = 0;
		for ( int i = 0; i < kImgs; i++ ) {
			if ( i == kWray || ( i == kScene && !j.stage5 ) || ( i >= kRay && !j.march ) )
				continue;
			const Floats v = compare( a[i], b[i] );
			diff += v.diff;
			if ( print )
				std::printf( "SSRVIEW %s %s %s: %zu of %zu floats differ, %zu past 1 ulp, max %lld ulp (GL non-zero %zu)\n", tag, j.name,
					imgName[i], v.diff, v.total, v.past1, v.maxu, v.nz );
			ok = ok && v.ok() && v.nz > 0;
		}
		return std::make_pair( ok, diff );
	};

	const Job jobs[7] = {
		{ "base", false, false, false, false, true, 0 },
		{ "bits6", false, false, false, false, true, 6 },
		{ "water", true, false, false, false, true, 0 },
		{ "look", true, true, false, false, true, 0 },
		{ "lookold", true, true, false, false, true, 8 },
		{ "prev", true, false, true, true, true, 0 },
		{ "bounce", false, false, true, true, false, 0 },
	};
	bool ok = true;
	std::vector<float> ref[7][kImgs];
	for ( int ji = 0; ji < 7; ji++ ) {
		const Job & j = jobs[ji];
		std::vector<float> vkOut[kImgs];
		if ( !runGl( j, ref[ji] ) || !runVk( j, 0, vkOut ) ) {
			std::printf( "SSRVIEW %s FAIL (a run failed)\n", j.name );
			ok = false;
			continue;
		}
		const auto r = judge( j, vkOut, ref[ji], true, "job" );
		std::printf( "SSRVIEW %s %s\n", j.name, !r.first ? "FAIL" : r.second == 0 ? "EXACT" : "BOUND" );
		ok = ok && r.first;
	}
	// the reds against the green references: each must FAIL
	const struct
	{
		const char * name;
		int red, job;
	} reds[4] = { { "atlas (the depth mips' offsets off by one)", 1, 0 }, { "prevrot (M untransposed)", 2, 5 },
		{ "look (the water look's rays on the base pipelines)", 4, 3 }, { "nostage5 (the bounce's donor not made)", 8, 6 } };
	for ( const auto & r : reds ) {
		std::vector<float> vkOut[kImgs];
		const bool ran = runVk( jobs[r.job], r.red, vkOut );
		const auto v = ran ? judge( jobs[r.job], vkOut, ref[r.job], false, "red" ) : std::make_pair( false, size_t( 0 ) );
		const bool redFails = !v.first;
		std::printf( "SSRVIEW RED %s %s (%zu floats differ; must FAIL)\n", redFails ? "FAIL" : "PASS", r.name, v.second );
		ok = ok && redFails;
	}
	const WwVkPresent::SsrStat st = WwVkPresent::takeSsrStat();
	std::printf( "SSRVIEW stat: %d runs on Vulkan, %d refused (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in "
		"%.1f ms\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
	for ( int i = 0; i < kImgs; i++ )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &keyHome[i] ) );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "SSRVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
