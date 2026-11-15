// Lane VKFAR3 (2026-11-15, plan row A1 step b): the screen GI's DDGI read on the present device (gihalf_vk.hlsl through the
// shipped WwVkPresent::lightRun, vkpresent.cpp with WW_VKPRESENT_NO_QT) against the GL program the app links
// (cell_gihalf.frag expanded with the loader's defines by gihalf_src.py). The atlas is ONE farTexelBuffer, as the app's
// cascades' buffer (rtddgicell.cpp combinedShare): GL writes it, GL's program reads it as cellDdgiAtlas, the present device
// reads it in place through ddgiAtlasView (the run's GL signal / wait carry the buffer). The run's uniforms are fxPack'd
// from GL's program (kT_gihalf), as ao.cpp giHalfPass does.
// Fixture: 3 cascades of 8x8x8 probes (anisotropic spacing x 2^c, states 2 / 0 / 1), random tiles, a rotated camera, a
// g-buffer with empty and normal-less texels. Bar (plan.md A1, written before the first run): every float of giOut and giKey
// within 4 ulp; the decoded view (E bits above bit 10, the five visibility codes, the normal codes) prints beside it. Greens: giStep 1 and 2; every active non-sampler uniform of GL's program is in kT_gihalf. Seen: >= 25 %
// of texels read a cascade, each cascade the first one for >= 2 %. Reds (must FAIL the bar): cascade 1's base one texel
// late, the count forced to 1, cellDdgiCStep shifted one vec4. Last line GIHALFVIEW GATE OK / FAILED.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace gh
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
	MSG m;
	while ( PeekMessageA( &m, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &m );
		DispatchMessageA( &m );
	}
}

// the GL 2.0+ entry points this gate calls (fxPack reads glGetUniformLocation / glGetUniformiv / glGetUniformfv off it)
struct Fns
{
	GLuint( APIENTRY * glCreateShader )( GLenum ) = nullptr;
	void( APIENTRY * glShaderSource )( GLuint, GLsizei, const char * const *, const GLint * ) = nullptr;
	void( APIENTRY * glCompileShader )( GLuint ) = nullptr;
	void( APIENTRY * glGetShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( APIENTRY * glGetShaderInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	GLuint( APIENTRY * glCreateProgram )() = nullptr;
	void( APIENTRY * glAttachShader )( GLuint, GLuint ) = nullptr;
	void( APIENTRY * glLinkProgram )( GLuint ) = nullptr;
	void( APIENTRY * glGetProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( APIENTRY * glGetProgramInfoLog )( GLuint, GLsizei, GLsizei *, char * ) = nullptr;
	void( APIENTRY * glUseProgram )( GLuint ) = nullptr;
	GLint( APIENTRY * glGetUniformLocation )( GLuint, const char * ) = nullptr;
	void( APIENTRY * glGetUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	void( APIENTRY * glGetUniformfv )( GLuint, GLint, GLfloat * ) = nullptr;
	void( APIENTRY * glGetActiveUniform )( GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * ) = nullptr;
	void( APIENTRY * glGetActiveUniformsiv )( GLuint, GLsizei, const GLuint *, GLenum, GLint * ) = nullptr;
	void( APIENTRY * glUniform1i )( GLint, GLint ) = nullptr;
	void( APIENTRY * glUniform2i )( GLint, GLint, GLint ) = nullptr;
	void( APIENTRY * glUniform2f )( GLint, GLfloat, GLfloat ) = nullptr;
	void( APIENTRY * glUniform1iv )( GLint, GLsizei, const GLint * ) = nullptr;
	void( APIENTRY * glUniform4iv )( GLint, GLsizei, const GLint * ) = nullptr;
	void( APIENTRY * glUniform4fv )( GLint, GLsizei, const GLfloat * ) = nullptr;
	void( APIENTRY * glGenFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( APIENTRY * glBindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( APIENTRY * glFramebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	void( APIENTRY * glDrawBuffers )( GLsizei, const GLenum * ) = nullptr;
	GLenum( APIENTRY * glCheckFramebufferStatus )( GLenum ) = nullptr;
	void( APIENTRY * glGenVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( APIENTRY * glBindVertexArray )( GLuint ) = nullptr;
	void( APIENTRY * glActiveTexture )( GLenum ) = nullptr;
	void( APIENTRY * glBindBuffer )( GLenum, GLuint ) = nullptr;
	void( APIENTRY * glBufferSubData )( GLenum, std::intptr_t, std::intptr_t, const void * ) = nullptr;
};

template <class T> bool load( T & f, const char * name )
{
	f = reinterpret_cast<T>( glProc( name ) );
	return f != nullptr;
}

bool loadAll( Fns & F )
{
	bool ok = true;
#define GHL( n ) ok = load( F.n, #n ) && ok
	GHL( glCreateShader ); GHL( glShaderSource ); GHL( glCompileShader ); GHL( glGetShaderiv ); GHL( glGetShaderInfoLog );
	GHL( glCreateProgram ); GHL( glAttachShader ); GHL( glLinkProgram ); GHL( glGetProgramiv ); GHL( glGetProgramInfoLog );
	GHL( glUseProgram ); GHL( glGetUniformLocation ); GHL( glGetUniformiv ); GHL( glGetUniformfv ); GHL( glGetActiveUniform ); GHL( glGetActiveUniformsiv );
	GHL( glUniform1i ); GHL( glUniform2i ); GHL( glUniform2f ); GHL( glUniform1iv ); GHL( glUniform4iv ); GHL( glUniform4fv );
	GHL( glGenFramebuffers ); GHL( glBindFramebuffer ); GHL( glFramebufferTexture2D ); GHL( glDrawBuffers );
	GHL( glCheckFramebufferStatus ); GHL( glGenVertexArrays ); GHL( glBindVertexArray ); GHL( glActiveTexture );
	GHL( glBindBuffer ); GHL( glBufferSubData );
#undef GHL
	return ok;
}

constexpr GLenum ghRgba32f = 0x8814, ghFloat = 0x1406, ghRgba = 0x1908, ghFb = 0x8D40, ghCol0 = 0x8CE0, ghTexBuf = 0x8C2A,
	ghArrayBuf = 0x8892, ghFrag = 0x8B30, ghVert = 0x8B31;

const char * kVert = "#version 410 core\n"
	"void main() { vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) ); gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";

std::uint32_t hashOf( std::uint32_t i, std::uint32_t seed )
{
	std::uint32_t h = i * 0x9E3779B1u ^ seed;
	h ^= h >> 15;
	h *= 0x85EBCA77u;
	h ^= h >> 13;
	h *= 0xC2B2AE3Du;
	h ^= h >> 16;
	return h;
}
float uni( std::uint32_t i, std::uint32_t seed )
{
	return float( hashOf( i, seed ) >> 8 ) / 16777216.0f;
}

// the fixture's cascades: 8 x 8 x 8 probes, spacing (100, 110, 90) x 2^c, the camera near the middle of each
constexpr int kCasc = 3, kDim = 8, kP = kDim * kDim * kDim;
const float kEye[3] = { 1234.5f, -876.25f, 300.0f };
struct Casc { float o[4], s[4]; int dims[4], base; };
Casc cascOf( int c )
{
	Casc k {};
	const float base[3] = { 100.0f, 110.0f, 90.0f }, jit[3] = { 0.37f, -0.21f, 0.13f };
	for ( int a = 0; a < 3; a++ ) {
		k.s[a] = base[a] * float( 1 << c );
		k.o[a] = kEye[a] - 3.5f * k.s[a] + jit[a] * k.s[a];
		k.dims[a] = kDim;
	}
	k.o[3] = 100.0f;	// the smallest spacing (cellDdgiOrigin.w)
	k.s[3] = 0.3f;		// the self-shadow bias (cellDdgiStep.w)
	k.dims[3] = kP;
	k.base = c * kP * 193;
	return k;
}

// a diagnostic only (GIHALF_PROBE=var, never the gate): the distance variance kept away from zero
float kProbeVar = 1.0f;

// the atlas in cell_ddgi.glsl's layout a cascade: irr 64 a probe, dist 128 a probe, the probes (offset, state)
std::vector<float> atlasOf()
{
	std::vector<float> a( size_t( kCasc ) * kP * 193 * 4 );
	for ( int c = 0; c < kCasc; c++ ) {
		const Casc k = cascOf( c );
		float * at = a.data() + size_t( k.base ) * 4;
		for ( int t = 0; t < 64 * kP; t++ ) {
			const std::uint32_t i = std::uint32_t( c * 1000003 + t );
			at[t * 4 + 0] = 0.05f + 0.85f * uni( i, 11 );
			at[t * 4 + 1] = 0.05f + 0.85f * uni( i, 12 );
			at[t * 4 + 2] = 0.05f + 0.85f * uni( i, 13 );
			at[t * 4 + 3] = uni( i, 14 );
		}
		float * dist = at + size_t( 64 * kP ) * 4;
		for ( int t = 0; t < 128 * kP; t++ ) {
			const std::uint32_t i = std::uint32_t( c * 1000003 + t );
			const float m = k.s[0] * ( 0.3f + 2.0f * uni( i, 21 ) );
			dist[t * 4 + 0] = m;
			dist[t * 4 + 1] = m * m * ( kProbeVar + 0.3f * uni( i, 22 ) );
		}
		float * pr = at + size_t( 192 * kP ) * 4;
		for ( int p = 0; p < kP; p++ ) {
			const std::uint32_t i = std::uint32_t( c * 1000003 + p );
			for ( int x = 0; x < 3; x++ )
				pr[p * 4 + x] = ( uni( i, 31 + x ) - 0.5f ) * 0.2f * k.s[x];
			const float u = uni( i, 35 );
			pr[p * 4 + 3] = u < 0.85f ? 2.0f : u < 0.92f ? 0.0f : 1.0f;
		}
	}
	return a;
}

// the camera: world = R posView + eye (cellRow), R = Rz(0.5236) Rx(0.3)
void rowsOf( float row[12] )
{
	const float cz = std::cos( 0.5236f ), sz = std::sin( 0.5236f ), cx = std::cos( 0.3f ), sx = std::sin( 0.3f );
	const float R[3][3] = { { cz, -sz * cx, sz * sx }, { sz, cz * cx, -cz * sx }, { 0.0f, sx, cx } };
	for ( int r = 0; r < 3; r++ ) {
		for ( int c = 0; c < 3; c++ )
			row[r * 4 + c] = R[r][c];
		row[r * 4 + 3] = kEye[r];
	}
}

// the g-buffer: view normal, linear depth (1e6 empty); 8 % empty, 3 % without a normal
std::vector<float> gbufOf( int W, int H )
{
	std::vector<float> g( size_t( W ) * H * 4 );
	for ( int i = 0; i < W * H; i++ ) {
		const std::uint32_t u = std::uint32_t( i );
		float * p = &g[size_t( i ) * 4];
		const float pick = uni( u, 41 );
		const float depth = 40.0f * std::pow( 30.0f, uni( u, 42 ) );
		if ( pick < 0.08f ) {
			p[0] = 0.0f; p[1] = 0.0f; p[2] = 1.0f; p[3] = 1.0e6f;
			continue;
		}
		if ( pick < 0.11f ) {
			p[0] = p[1] = p[2] = 0.0f;
			p[3] = depth;
			continue;
		}
		float n[3] = { uni( u, 43 ) * 2.0f - 1.0f, uni( u, 44 ) * 2.0f - 1.0f, uni( u, 45 ) * 2.0f - 1.0f + 0.6f };
		const float l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
		for ( int a = 0; a < 3; a++ )
			p[a] = n[a] / std::max( l, 1e-3f );
		p[3] = depth;
	}
	return g;
}

GLuint texOf( int w, int h, const float * data )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	glTexParameteri( GL_TEXTURE_2D, 0x813D, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0, GLint( ghRgba32f ), w, h, 0, ghRgba, ghFloat, data );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return t;
}

std::vector<std::uint32_t> readTex( GLuint t, int w, int h )
{
	std::vector<std::uint32_t> v( size_t( w ) * h * 4 );
	glBindTexture( GL_TEXTURE_2D, t );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glGetTexImage( GL_TEXTURE_2D, 0, ghRgba, ghFloat, v.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return v;
}

int ulps32( std::uint32_t a, std::uint32_t b )
{
	auto ord = []( std::uint32_t x ) { return ( x & 0x80000000u ) ? -std::int64_t( x & 0x7FFFFFFFu ) : std::int64_t( x ); };
	const std::int64_t d = ord( a ) - ord( b );
	return int( std::min<std::int64_t>( d < 0 ? -d : d, 1 << 30 ) );
}

// cell_gihalfat.glsl cellGiHalfPack5's five codes (v0 r, v1 a, v2 g, v3 bits 8..9 of all four, v4 b)
void codesOf( const std::uint32_t * o, int c[5] )
{
	c[0] = int( o[0] & 0xFFu );
	c[1] = int( o[3] & 0xFFu );
	c[2] = int( o[1] & 0xFFu );
	c[3] = int( ( ( o[0] >> 8 ) & 3u ) | ( ( ( o[1] >> 8 ) & 3u ) << 2 ) | ( ( ( o[2] >> 8 ) & 3u ) << 4 ) | ( ( ( o[3] >> 8 ) & 3u ) << 6 ) );
	c[4] = int( o[2] & 0xFFu );
}

struct Diff
{
	std::size_t texels = 0, differ = 0, out = 0, eOut = 0, eOut4 = 0, eOutKeySame = 0, uOut = 0, floatsOut = 0, decodedOut = 0;
	int eStep = 0, code = 0, ulp = 0, nrm = 0, ulpAll = 0;
	long firstOut = -1, worstKey = -1;
};

Diff compare( const std::vector<std::uint32_t> & vo, const std::vector<std::uint32_t> & go, const std::vector<std::uint32_t> & vk,
	const std::vector<std::uint32_t> & gk )
{
	Diff d;
	d.texels = go.size() / 4;
	for ( std::size_t t = 0; t < d.texels; t++ ) {
		const std::uint32_t * a = &vo[t * 4], * b = &go[t * 4], * ka = &vk[t * 4], * kb = &gk[t * 4];
		if ( std::memcmp( a, b, 16 ) || std::memcmp( ka, kb, 16 ) )
			d.differ++;
		int e = 0, cd = 0, u = 0, nm = 0;
		for ( int ch = 0; ch < 4; ch++ )
			e = std::max( e, std::abs( int( a[ch] >> 10 ) - int( b[ch] >> 10 ) ) );
		int ca[5], cb[5];
		codesOf( a, ca );
		codesOf( b, cb );
		for ( int k = 0; k < 5; k++ )
			cd = std::max( cd, std::abs( ca[k] - cb[k] ) );
		for ( int ch = 0; ch < 3; ch++ )
			u = std::max( u, ulps32( ka[ch], kb[ch] ) );
		float wa, wb;
		std::memcpy( &wa, &ka[3], 4 );
		std::memcpy( &wb, &kb[3], 4 );
		if ( ( wa < 0.0f ) != ( wb < 0.0f ) )
			nm = 1 << 20;
		else if ( wa >= 0.0f ) {
			const int qa = int( wa ), qb = int( wb );
			nm = std::max( std::abs( qa / 2048 - qb / 2048 ), std::abs( qa % 2048 - qb % 2048 ) );
		}
		if ( u > d.ulp )
			d.worstKey = long( t );
		if ( e > 1 ) {
			d.eOut++;
			if ( !std::memcmp( ka, kb, 12 ) )
				d.eOutKeySame++;
			if ( e > 4 )
				d.eOut4++;
		}
		if ( u > 4 )
			d.uOut++;
		d.eStep = std::max( d.eStep, e );
		d.code = std::max( d.code, cd );
		d.ulp = std::max( d.ulp, u );
		d.nrm = std::max( d.nrm, nm );
		// the bar (plan.md A1, written before the first run): every float of giOut and giKey within 4 ulp
		int pu = 0;
		for ( int ch = 0; ch < 4; ch++ ) {
			const int x = std::max( ulps32( a[ch], b[ch] ), ulps32( ka[ch], kb[ch] ) );
			pu = std::max( pu, x );
			d.floatsOut += ( ulps32( a[ch], b[ch] ) > 4 ) + ( ulps32( ka[ch], kb[ch] ) > 4 );
		}
		d.ulpAll = std::max( d.ulpAll, pu );
		if ( e > 1 || cd > 1 || u > 4 || nm > 1 )
			d.decodedOut++;
		if ( pu > 4 ) {
			if ( d.firstOut < 0 )
				d.firstOut = long( t );
			d.out++;
		}
	}
	return d;
}

std::string readText( const std::string & path )
{
	std::ifstream f( path, std::ios::binary );
	return std::string( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
}
}  // namespace gh

int main( int argc, char ** argv )
{
	using namespace gh;
	if ( argc < 2 ) {
		std::printf( "GIHALFVIEW GATE FAILED (usage: gihalfview <cell_gihalf source from gihalf_src.py>)\n" );
		return 2;
	}
	std::string frag = readText( argv[1] );
	const char * probe = std::getenv( "GIHALF_PROBE" );
	if ( probe && std::string( probe ) == "stage" ) {	// a diagnostic only: GL's giOut carries z, posView.xy, ndc.x
		const std::string at = "giKey = vec4( P, cellGiKeyPack( N ) );";
		const std::size_t k = frag.find( at );
		if ( k != std::string::npos )
			frag.insert( k + at.size(), " giOut = vec4( z, posView.xy, ndc.x );" );
		std::printf( "PROBE stage (not the gate): GL giOut = z, posView.xy, ndc.x (%s)\n", k != std::string::npos ? "placed" : "NOT placed" );
	}
	const char * dumpDir = std::getenv( "GIHALF_DUMP" );
	if ( probe && std::string( probe ) == "var" ) {
		kProbeVar = 1.5f;
		std::printf( "PROBE fixture (not the gate): distance variance >= 0.5 mean^2\n" );
	}
	const int VW = 320, VH = 180, S = 4, GW = 317, GH = 181;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwgihalfview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwgihalfview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, VW, VH };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwgihalfview", "gihalf view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( frag.empty() || !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "GIHALFVIEW GATE FAILED (no source or no 4x MSAA pixel format)\n" );
		return 2;
	}
	PIXELFORMATDESCRIPTOR pf {};
	DescribePixelFormat( dc, fmt, sizeof( pf ), &pf );
	SetPixelFormat( dc, fmt, &pf );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	std::printf( "GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	Fns F;
	if ( !loadAll( F ) ) {
		std::printf( "GIHALFVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::string why;
	if ( !WwVkPresent::viewBegin( VW, VH, S, glProc, why ) ) {
		std::printf( "GIHALFVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	bool ok = true;
	auto verdict = [&]( const std::string & what, bool pass, const std::string & detail ) {
		std::printf( "GIHALFVIEW %s %s (%s)\n", what.c_str(), pass ? "OK" : "FAILED", detail.c_str() );
		ok = ok && pass;
	};
	// GL's program, the app's source
	auto shader = [&]( GLenum kind, const char * src ) {
		const GLuint s = F.glCreateShader( kind );
		F.glShaderSource( s, 1, &src, nullptr );
		F.glCompileShader( s );
		GLint c = 0;
		F.glGetShaderiv( s, 0x8B81, &c );
		if ( !c ) {
			char log[2048] = {};
			F.glGetShaderInfoLog( s, sizeof( log ) - 1, nullptr, log );
			std::printf( "compile log: %s\n", log );
		}
		return c ? s : 0u;
	};
	const GLuint vs = shader( ghVert, kVert ), fs = shader( ghFrag, frag.c_str() );
	GLuint prog = 0;
	GLint linked = 0;
	if ( vs && fs ) {
		prog = F.glCreateProgram();
		F.glAttachShader( prog, vs );
		F.glAttachShader( prog, fs );
		F.glLinkProgram( prog );
		F.glGetProgramiv( prog, 0x8B82, &linked );
	}
	if ( !linked ) {
		std::printf( "GIHALFVIEW GATE FAILED (cell_gihalf did not compile or link)\n" );
		return 2;
	}
	// every active non-sampler uniform of GL's program is one kT_gihalf packs
	{
		std::set<std::string> table;
		for ( const WwVkPresent::FxField & x : WwVkPresent::kT_gihalf )
			table.insert( x.name );
		GLint n = 0;
		F.glGetProgramiv( prog, 0x8B86, &n );
		std::string missing;
		int samplers = 0;
		std::string inBlock;
		for ( GLint i = 0; i < n; i++ ) {
			char name[256] = {};
			GLint size = 0, block = -1;
			GLenum type = 0;
			F.glGetActiveUniform( prog, GLuint( i ), sizeof( name ) - 1, nullptr, &size, &type, name );
			const GLuint ui = GLuint( i );
			F.glGetActiveUniformsiv( prog, 1, &ui, 0x8A3A, &block );	// GL_UNIFORM_BLOCK_INDEX
			std::string nm( name );
			if ( nm.size() > 3 && nm.compare( nm.size() - 3, 3, "[0]" ) == 0 )
				nm.resize( nm.size() - 3 );
			const bool sampler = ( type >= 0x8B5D && type <= 0x8B64 ) || ( type >= 0x8DC0 && type <= 0x8DD8 ) || ( type >= 0x900C && type <= 0x900F )
				|| ( type >= 0x9108 && type <= 0x910D );
			if ( sampler ) {
				samplers++;
				if ( nm != "giGbuf" && nm != "cellDdgiAtlas" )
					missing += " sampler:" + nm;	// the kernel binds these two only
			} else if ( block >= 0 )
				inBlock += " " + nm;	// a uniform block's member (the frame's buffer), not the program's own default block
			else if ( !table.count( nm ) )
				missing += " " + nm;
		}
		verdict( "kT_gihalf holds GL's uniforms", missing.empty(), std::to_string( n ) + " active, " + std::to_string( samplers )
			+ " samplers" + ( missing.empty() ? std::string() : ", not packed:" + missing ) );
		std::printf( "info: uniform block members (not the kernel's: it reads no block):%s\n", inBlock.c_str() );
	}
	// the atlas: ONE farTexelBuffer (the app's cascades' buffer), GL writes it
	const std::vector<float> atlas = atlasOf();
	const int akey = 0;
	const std::uintptr_t ak = reinterpret_cast<std::uintptr_t>( &akey );
	unsigned abuf = 0, atex = 0;
	if ( !WwVkPresent::farTexelBuffer( ak, ghRgba32f, atlas.size() * 4, abuf, atex, why ) ) {
		std::printf( "GIHALFVIEW GATE FAILED (the atlas buffer: %s)\n", why.c_str() );
		return 2;
	}
	F.glBindBuffer( ghArrayBuf, abuf );
	F.glBufferSubData( ghArrayBuf, 0, std::intptr_t( atlas.size() * 4 ), atlas.data() );
	F.glBindBuffer( ghArrayBuf, 0 );
	std::uint64_t atexels = 0;
	const bool aview = WwVkPresent::ddgiAtlasView( &akey, &atexels ) != 0;
	verdict( "atlas seen by the lookup", aview && atexels == atlas.size() / 4, std::to_string( atexels ) + " texels, " + std::to_string( kCasc ) + " cascades of " + std::to_string( kP ) + " probes" );
	const std::vector<float> gb = gbufOf( GW, GH );
	const GLuint gbuf = texOf( GW, GH, gb.data() );
	GLuint vao = 0, fbo = 0;
	F.glGenVertexArrays( 1, &vao );
	F.glGenFramebuffers( 1, &fbo );
	const float p00 = 1.7f, p11 = 1.7f * float( GW ) / float( GH );
	float row[12];
	rowsOf( row );
	/* a diagnostic only (GIHALF_PROBE=rows_id / rows_perm / rows_three, never the gate): rows with no translation and one
	 * entry a row, so cellWorldPos is exact in any float order and the key carries posView (times the entry): splits the
	 * key's miss into z / posView and the matrix sum. id = identity, perm = (y 2, z 0.5, x 4), three = (z 3, x 3, y 3) */
	if ( probe && std::string( probe ).rfind( "rows_", 0 ) == 0 ) {
		const std::string mm = std::string( probe ).substr( 5 );
		const bool eye = mm.size() > 4 && mm.compare( mm.size() - 4, 4, "_eye" ) == 0;	// rows_id_eye: the eye's translation kept
		const std::string m = eye ? mm.substr( 0, mm.size() - 4 ) : mm;
		const int col[3] = { m == "id" ? 0 : m == "perm" ? 1 : 2, m == "id" ? 1 : m == "perm" ? 2 : 0, m == "id" ? 2 : m == "perm" ? 0 : 1 };
		const float sc[3] = { m == "id" ? 1.0f : m == "perm" ? 2.0f : 3.0f, m == "id" ? 1.0f : m == "perm" ? 0.5f : 3.0f,
			m == "id" ? 1.0f : m == "perm" ? 4.0f : 3.0f };
		for ( int r = 0; r < 3; r++ )
			for ( int c = 0; c < 4; c++ )
				row[r * 4 + c] = c == col[r] ? sc[r] : c == 3 && eye ? kEye[r] : 0.0f;
		std::printf( "PROBE %s (not the gate): rows with one entry each, no translation\n", probe );
	}
	// GL's uniforms, as wwCellLightsUniforms + giHalfPass set them
	auto loc = [&]( const char * n ) { return F.glGetUniformLocation( prog, n ); };
	F.glUseProgram( prog );
	F.glUniform1i( loc( "cellOn" ), 1 );
	F.glUniform4fv( loc( "cellRow" ), 3, row );
	F.glUniform1i( loc( "cellGiLoop8" ), 8 );
	F.glUniform1i( loc( "cellDdgiOn" ), 1 );
	F.glUniform1i( loc( "cellDdgiCount" ), kCasc );
	// a diagnostic only (GIHALF_PROBE=ddgired<N>, never the gate): the read's stage bits off by uniform (same binaries)
	F.glUniform1i( loc( "cellDdgiRed" ), probe && std::string( probe ).rfind( "ddgired", 0 ) == 0 ? std::atoi( probe + 7 ) : 0 );
	{
		float o[16] = {}, s[16] = {};
		int d[16] = {}, b[4] = {};
		for ( int c = 0; c < kCasc; c++ ) {
			const Casc k = cascOf( c );
			std::memcpy( o + 4 * c, k.o, 16 );
			std::memcpy( s + 4 * c, k.s, 16 );
			std::memcpy( d + 4 * c, k.dims, 16 );
			b[c] = k.base;
		}
		F.glUniform4fv( loc( "cellDdgiCOrigin" ), 4, o );
		F.glUniform4fv( loc( "cellDdgiCStep" ), 4, s );
		F.glUniform4iv( loc( "cellDdgiCDims" ), 4, d );
		F.glUniform1iv( loc( "cellDdgiCBase" ), 4, b );
	}
	F.glUniform2i( loc( "giFull" ), GW, GH );
	F.glUniform2f( loc( "giProj" ), p00, p11 );
	F.glUniform1i( loc( "giGbuf" ), 0 );
	F.glUniform1i( loc( "cellDdgiAtlas" ), 1 );
	F.glUseProgram( 0 );
	struct Run { int step; const char * red; };
	const Run runs[] = { { 1, nullptr }, { 2, nullptr }, { 1, "red base (cascade 1's base one texel late)" },
		{ 1, "red count (cellDdgiCount forced to 1)" }, { 1, "red shift (cellDdgiCStep one vec4 late)" } };
	for ( const Run & R : runs ) {
		const int hw = ( GW + R.step - 1 ) / R.step, hh = ( GH + R.step - 1 ) / R.step;
		std::vector<float> sentinel( size_t( hw ) * hh * 4, std::nanf( "" ) );
		const GLuint glOut = texOf( hw, hh, nullptr ), glKey = texOf( hw, hh, nullptr ), vkOut = texOf( hw, hh, sentinel.data() ),
			vkKey = texOf( hw, hh, sentinel.data() );
		// GL's reference: the app's draw (giHalfPass)
		F.glUseProgram( prog );
		F.glUniform1i( loc( "giStep" ), R.step );
		F.glActiveTexture( 0x84C0 );
		glBindTexture( GL_TEXTURE_2D, gbuf );
		F.glActiveTexture( 0x84C1 );
		glBindTexture( ghTexBuf, atex );
		F.glBindFramebuffer( ghFb, fbo );
		F.glFramebufferTexture2D( ghFb, ghCol0, GL_TEXTURE_2D, glOut, 0 );
		F.glFramebufferTexture2D( ghFb, ghCol0 + 1, GL_TEXTURE_2D, glKey, 0 );
		const GLenum bufs[2] = { ghCol0, ghCol0 + 1 };
		F.glDrawBuffers( 2, bufs );
		if ( F.glCheckFramebufferStatus( ghFb ) != 0x8CD5 ) {
			std::printf( "GIHALFVIEW GATE FAILED (GL's target incomplete)\n" );
			return 2;
		}
		glViewport( 0, 0, hw, hh );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );
		F.glBindVertexArray( vao );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		F.glBindVertexArray( 0 );
		F.glBindFramebuffer( ghFb, 0 );
		glBindTexture( ghTexBuf, 0 );
		F.glActiveTexture( 0x84C0 );
		glBindTexture( GL_TEXTURE_2D, 0 );
		// the Vulkan run: ao.cpp giHalfPass's (uniforms fxPack'd off the program GL just drew with)
		WwVkPresent::LightRun t;
		t.pass = WwVkPresent::LightGi;
		auto put = [&]( GLuint tex, int w, int h, bool in, bool out ) {
			WwVkPresent::LightImage i;
			i.tex = tex;
			i.glFmt = ghRgba32f;
			i.w = w;
			i.h = h;
			i.in = in;
			i.out = out;
			t.img.push_back( i );
			return int( t.img.size() ) - 1;
		};
		WwVkPresent::LightDispatch d;
		d.kernel = WwVkPresent::LightGiHalf;
		d.t[0] = put( gbuf, GW, GH, true, false );
		d.out0 = put( vkOut, hw, hh, false, true );
		d.out1 = put( vkKey, hw, hh, false, true );
		t.d.push_back( d );
		t.ubo.assign( size_t( WwVkPresent::kT_gihalfBytes ), 0 );
		WwVkPresent::fxPack( &F, prog, WwVkPresent::kT_gihalf, int( std::size( WwVkPresent::kT_gihalf ) ), t.ubo.data() );
		F.glUseProgram( 0 );
		if ( R.red && R.red[4] == 'b' ) {	// cellDdgiCBase[1] (offset 3984 + 16)
			std::int32_t v;
			std::memcpy( &v, &t.ubo[3984 + 16], 4 );
			v += 1;
			std::memcpy( &t.ubo[3984 + 16], &v, 4 );
		} else if ( R.red && R.red[4] == 'c' ) {	// cellDdgiCount (3776)
			const std::int32_t one = 1;
			std::memcpy( &t.ubo[3776], &one, 4 );
		} else if ( R.red && R.red[4] == 's' ) {	// cellDdgiCStep[0..3] (3856) one vec4 late
			std::memmove( &t.ubo[3856 + 16], &t.ubo[3856], 48 );
			std::memset( &t.ubo[3856], 0, 16 );
		}
		t.ddgiKey = &akey;
		t.ddgiGlBuf = abuf;
		const double t0 = nowMs();
		const bool ran = WwVkPresent::lightRun( t, why );
		const double ms = nowMs() - t0;
		const std::string tag = R.red ? std::string( R.red ) : "giStep " + std::to_string( R.step );
		if ( !ran ) {
			verdict( tag, false, "lightRun refused: " + why );
			continue;
		}
		const std::vector<std::uint32_t> go = readTex( glOut, hw, hh ), gk = readTex( glKey, hw, hh ), vo = readTex( vkOut, hw, hh ),
			vk2 = readTex( vkKey, hw, hh );
		const Diff df = compare( vo, go, vk2, gk );
		char line[512];
		std::snprintf( line, sizeof( line ), "%dx%d work texels, %zu differ, %zu past the bar (%zu floats past 4 ulp, max %d ulp); decoded: "
			"%zu texels past E 1 step / codes 1, max E step %d, vis code %d, key %d ulp, normal code %d; lightRun %.2f ms", hw, hh, df.differ,
			df.out, df.floatsOut, df.ulpAll, df.decodedOut, df.eStep, df.code, df.ulp, df.nrm, ms );
		std::string detail( line );
		if ( df.firstOut >= 0 ) {
			const std::size_t f = std::size_t( df.firstOut ) * 4;
			std::snprintf( line, sizeof( line ), "; first at %ld,%ld: vk %08x %08x %08x %08x / %08x gl %08x %08x %08x %08x / %08x",
				df.firstOut % hw, df.firstOut / hw, vo[f], vo[f + 1], vo[f + 2], vo[f + 3], vk2[f + 3], go[f], go[f + 1], go[f + 2], go[f + 3], gk[f + 3] );
			detail += line;
		}
		if ( !R.red ) {
			std::printf( "info %s: E past 1 step %zu (past 4: %zu, key xyz bit-equal there: %zu), key past 4 ulp %zu\n", tag.c_str(), df.eOut,
				df.eOut4, df.eOutKeySame, df.uOut );
			std::size_t ch[3] = {}, multi = 0, anyKey = 0;
			for ( std::size_t i = 0; i < gk.size() / 4; i++ ) {
				int n = 0;
				for ( int c = 0; c < 3; c++ )
					if ( vk2[i * 4 + c] != gk[i * 4 + c] ) {
						ch[c]++;
						n++;
					}
				anyKey += n > 0;
				multi += n > 1;
			}
			std::printf( "info %s: key bits differ x %zu y %zu z %zu, any %zu, in more than one channel %zu\n", tag.c_str(), ch[0], ch[1], ch[2],
				anyKey, multi );
			if ( dumpDir && R.step == 1 ) {
				const std::string base = std::string( dumpDir ) + "/";
				auto put = [&]( const char * name, const void * p, std::size_t bytes ) {
					std::ofstream f( base + name, std::ios::binary );
					f.write( static_cast<const char *>( p ), std::streamsize( bytes ) );
				};
				put( "gbuf.f32", gb.data(), gb.size() * 4 );
				put( "atlas.f32", atlas.data(), atlas.size() * 4 );
				put( "gl_out.u32", go.data(), go.size() * 4 );
				put( "gl_key.u32", gk.data(), gk.size() * 4 );
				put( "vk_out.u32", vo.data(), vo.size() * 4 );
				put( "vk_key.u32", vk2.data(), vk2.size() * 4 );
				put( "row.f32", row, sizeof( row ) );
				std::printf( "info: dumped step 1 to %s\n", dumpDir );
			}
			if ( df.worstKey >= 0 ) {
				const std::size_t f = std::size_t( df.worstKey ) * 4;
				float a[4], b[4];
				std::memcpy( a, &vk2[f], 16 );
				std::memcpy( b, &gk[f], 16 );
				std::printf( "info %s: worst key at %ld,%ld: vk %.9g %.9g %.9g %.9g gl %.9g %.9g %.9g %.9g\n", tag.c_str(), df.worstKey % hw,
					df.worstKey / hw, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3] );
			}
		}
		if ( R.red ) {
			verdict( std::string( R.red ) + " FAILS", df.out > 0, detail );
		} else {
			verdict( tag + " within the bar", df.out == 0, detail );
			// seen: the texels that read a cascade; each cascade the first (tight) one for its share
			std::size_t read = 0, first[kCasc] = {};
			for ( std::size_t i = 0; i < go.size() / 4; i++ ) {
				float a;
				std::memcpy( &a, &go[i * 4 + 3], 4 );
				float kw;
				std::memcpy( &kw, &gk[i * 4 + 3], 4 );
				if ( a > 0.0f )
					read++;
				if ( kw < 0.0f )
					continue;
				float P[3];
				std::memcpy( P, &gk[i * 4], 12 );
				for ( int c = 0; c < kCasc; c++ ) {
					const Casc k = cascOf( c );
					bool in = true;
					for ( int ax = 0; ax < 3; ax++ ) {
						const float g = ( P[ax] - k.o[ax] ) / k.s[ax];
						in = in && g > 1.0f && g < float( kDim - 2 );
					}
					if ( in ) {
						first[c]++;
						break;
					}
				}
			}
			const std::size_t n = go.size() / 4;
			verdict( tag + " seen: texels that read", read * 4 >= n, std::to_string( read ) + " of " + std::to_string( n ) );
			for ( int c = 0; c < kCasc; c++ )
				verdict( tag + " seen: cascade " + std::to_string( c ) + " first", first[c] * 50 >= n, std::to_string( first[c] ) + " of "
					+ std::to_string( n ) );
		}
		const GLuint dead[4] = { glOut, glKey, vkOut, vkKey };
		glDeleteTextures( 4, dead );
	}
	WwVkPresent::farTexelBufferDrop( ak );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "GIHALFVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
