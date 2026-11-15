// Lane VKLAMP1 (2026-11-15, VKFAR7 item 3 slice 1): the lamps' plain shadow cubes on the present device (vklamp.inc lampRun,
// built in here with WW_VKPRESENT_NO_QT: the shipped code) against GL's own program (lights/cell_shadowdepth.vert + .frag)
// drawn the way celllamps.cpp draws a cube face (clear to 1, no cull, no offset, LEQUAL, viewport face x face, one draw a
// caster with its modelViewMatrix), into D16 cube arrays, every texel of every layer compared.
// Usage: lampview <tree root> [face]
// Four slots (24 layers), four lights of different radii and near clips; casters on every face axis, random ones, one
// inside the near clip, one straddling the light 1-2 units away, one past the radius; slot 2's faces 4 and 5 are cone-skipped
// (clear only); slot 3's face 0 gets a LOAD op after its clear op (a mover drawn over the kept casters). Frame 2 moves the
// casters and leaves slot 1 held (no op: it keeps frame 1's bytes); an unrelated shared target is made between the frames.
// Both arms are prefilled with junk depth before each frame (the held slot excepted).
// Prints `LAMPVIEW frame<n> slot<k>/face<f> EXACT|BOUND|CLEAR|FAIL`, `LAMPVIEW RED ...` (each must FAIL) and
// `LAMPVIEW GATE OK|FAILED`. The bar is in lampview.sh (stated before the first run, never widened).
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

namespace lmv
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

constexpr GLenum kFb = 0x8D40, kDepthAtt = 0x8D00, kComplete = 0x8CD5, kFrag = 0x8B30, kVert = 0x8B31;
constexpr GLenum kCompiled = 0x8B81, kLinked = 0x8B82, kArrayBuf = 0x8892, kElemBuf = 0x8893;
constexpr GLenum kStatic = 0x88E4, kTex2DArray = 0x8C1A, kCubeArray = 0x9009, kD16 = 0x81A5;

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
	void( WINAPI * uniformMatrix4fv )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
	void( WINAPI * uniform2f )( GLint, GLfloat, GLfloat ) = nullptr;
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * genBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferData )( GLenum, std::ptrdiff_t, const void *, GLenum ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * drawElementsBaseVertex )( GLenum, GLsizei, GLenum, const void *, GLint ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTextureLayer )( GLenum, GLenum, GLuint, GLint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * texImage3D )( GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void * ) = nullptr;
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
		&& load( f.uniformMatrix4fv, "glUniformMatrix4fv" ) && load( f.uniform2f, "glUniform2f" ) && load( f.uniform1i, "glUniform1i" )
		&& load( f.genBuffers, "glGenBuffers" ) && load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.genVertexArrays, "glGenVertexArrays" ) && load( f.bindVertexArray, "glBindVertexArray" )
		&& load( f.enableVertexAttribArray, "glEnableVertexAttribArray" ) && load( f.vertexAttribPointer, "glVertexAttribPointer" )
		&& load( f.drawElementsBaseVertex, "glDrawElementsBaseVertex" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTextureLayer, "glFramebufferTextureLayer" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" ) && load( f.texImage3D, "glTexImage3D" );
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
		std::printf( "LAMPVIEW compile failed (%s): %.1500s\n", tag, log.data() );
		return 0;
	}
	return s;
}

GLuint program( const std::string & vs, const std::string & fs )
{
	const GLuint a = shader( kVert, vs, "vert" ), b = shader( kFrag, fs, "frag" );
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
		std::printf( "LAMPVIEW link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}

// a 3x3 rotation about a unit axis, R[row][col]
void rot3( double ax, double ay, double az, double ang, double R[3][3] )
{
	const double l = std::sqrt( ax * ax + ay * ay + az * az );
	ax /= l;
	ay /= l;
	az /= l;
	const double c = std::cos( ang ), si = std::sin( ang ), t = 1.0 - c;
	const double M[3][3] = { { t * ax * ax + c, t * ax * ay - si * az, t * ax * az + si * ay },
		{ t * ax * ay + si * az, t * ay * ay + c, t * ay * az - si * ax }, { t * ax * az - si * ay, t * ay * az + si * ax, t * az * az + c } };
	for ( int r = 0; r < 3; r++ )
		for ( int k = 0; k < 3; k++ )
			R[r][k] = M[r][k];
}

struct Diff
{
	std::size_t covered = 0, diff = 0, past1 = 0, vkSet = 0, refSet = 0;
	int maxu = 0;
	bool ok() const { return covered > 0 && maxu <= 1 && diff * 2000 <= covered; }
	bool clearOk() const { return vkSet == 0 && refSet == 0; }
	void add( const Diff & o )
	{
		covered += o.covered;
		diff += o.diff;
		past1 += o.past1;
		vkSet += o.vkSet;
		refSet += o.refSet;
		maxu = std::max( maxu, o.maxu );
	}
};

Diff compare( const std::uint16_t * a, const std::uint16_t * b, std::size_t n )
{
	Diff v;
	for ( std::size_t i = 0; i < n; i++ ) {
		v.vkSet += a[i] != 0xFFFF;
		v.refSet += b[i] != 0xFFFF;
		if ( a[i] == 0xFFFF && b[i] == 0xFFFF )
			continue;
		v.covered++;
		const int d = std::abs( int( a[i] ) - int( b[i] ) );
		v.diff += d != 0;
		v.past1 += d > 1;
		v.maxu = std::max( v.maxu, d );
	}
	return v;
}
}  // namespace lmv

int main( int argc, char ** argv )
{
	using namespace lmv;
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 2 ) {
		std::printf( "usage: lampview <tree root> [face]\nLAMPVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[1];
	const int F = argc > 2 ? std::atoi( argv[2] ) : 256;
	const int kSlots = 4, kLayers = 6 * kSlots, W = 256, H = 256, S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwlampview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwlampview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwlampview", "lamp view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "LAMPVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
		std::printf( "LAMPVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "face %d x %d slots (%d layers), GL on %s\n", F, kSlots, kLayers, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	const GLuint prog = program( readFile( root + "/res/shaders/features/lights/cell_shadowdepth.vert" ),
		readFile( root + "/res/shaders/features/lights/cell_shadowdepth.frag" ) );
	if ( !prog ) {
		std::printf( "LAMPVIEW GATE FAILED (GL program)\n" );
		return 2;
	}
	const GLint lRel = f.getUniformLocation( prog, "shRelFromView" ), lClip = f.getUniformLocation( prog, "shClipFromRel" );
	const GLint lRn = f.getUniformLocation( prog, "shRadiusNear" ), lMv = f.getUniformLocation( prog, "modelViewMatrix" );
	const GLint lAt = f.getUniformLocation( prog, "shAlphaTest" );
	if ( lRel < 0 || lClip < 0 || lRn < 0 || lMv < 0 ) {
		std::printf( "LAMPVIEW GATE FAILED (a uniform of cell_shadowdepth is not active)\n" );
		return 2;
	}

	// ---- the pool: random triangle soups and a wavy plate (vec3 positions, u16 indices: celllamps' client arrays)
	std::mt19937 rng( 11 );
	std::uniform_real_distribution<float> u( -1.0f, 1.0f );
	std::vector<float> pos;
	std::vector<std::uint16_t> idx;
	struct Mesh
	{
		std::uint32_t firstIndex, count;
		std::int32_t baseVertex;
	};
	std::vector<Mesh> meshes;
	auto beginMesh = [&]() { meshes.push_back( { std::uint32_t( idx.size() ), 0, std::int32_t( pos.size() / 3 ) } ); };
	auto endMesh = [&]() { meshes.back().count = std::uint32_t( idx.size() ) - meshes.back().firstIndex; };
	for ( int m = 0; m < 3; m++ ) {
		beginMesh();
		for ( int t = 0; t < 60; t++ ) {
			const float cx = u( rng ), cy = u( rng ), cz = u( rng );
			for ( int k = 0; k < 3; k++ ) {
				pos.push_back( cx + 0.45f * u( rng ) );
				pos.push_back( cy + 0.45f * u( rng ) );
				pos.push_back( cz + 0.45f * u( rng ) );
				idx.push_back( std::uint16_t( t * 3 + k ) );
			}
		}
		endMesh();
	}
	{
		beginMesh();
		const int n = 10;
		for ( int y = 0; y <= n; y++ )
			for ( int x = 0; x <= n; x++ ) {
				const float px = -1.0f + 2.0f * float( x ) / n, py = -1.0f + 2.0f * float( y ) / n;
				pos.push_back( px );
				pos.push_back( py );
				pos.push_back( 0.25f * std::sin( 3.0f * px ) * std::cos( 2.0f * py ) );
			}
		for ( int y = 0; y < n; y++ )
			for ( int x = 0; x < n; x++ ) {
				const std::uint16_t a = std::uint16_t( y * ( n + 1 ) + x ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + n + 1 ),
					d = std::uint16_t( c + 1 );
				const std::uint16_t q[6] = { a, b, d, a, d, c };
				idx.insert( idx.end(), q, q + 6 );
			}
		endMesh();
	}
	const int kMeshes = int( meshes.size() );

	// ---- the view (celllamps' vt / sc / cam) and the lights
	double Rv[3][3];
	rot3( 0.4, 1.0, -0.3, 0.8, Rv );
	float rotA[9];  // rot[j * 3 + row] = the view rotation's (j, row)
	for ( int j = 0; j < 3; j++ )
		for ( int row = 0; row < 3; row++ )
			rotA[j * 3 + row] = float( Rv[j][row] );
	const float sc = 1.25f, cam[3] = { 120.0f, -340.0f, 95.0f };
	struct Light
	{
		float pos[3], radius, nearClip;
	};
	const Light lights[kSlots] = { { { 0, 0, 0 }, 300.0f, 5.0f }, { { 900, 200, -50 }, 150.0f, 3.0f }, { { -700, 500, 300 }, 512.0f, 10.0f },
		{ { 300, -900, 120 }, 220.0f, 4.0f } };
	// rel = Rel * viewPos + cam - light, Rel[row][j] = rot(j, row) / sc: a world (light-relative) point -> its view point
	auto viewOf = [&]( const double w[3], double out[3] ) {
		double d[3] = { w[0] - cam[0], w[1] - cam[1], w[2] - cam[2] };
		for ( int j = 0; j < 3; j++ ) {  // view = sc * Rel^T-inverse: Rel = A / sc with A[row][j] = rot(j, row), A orthonormal
			double s = 0.0;
			for ( int row = 0; row < 3; row++ )
				s += double( rotA[j * 3 + row] ) * d[row];
			out[j] = sc * s;
		}
	};

	// ---- casters: world centre, scale (world units), turn; model-view per frame (view space, GL column-major)
	struct Caster
	{
		int slot, mesh;
		double c[3], s, ax, ay, az, ang;
	};
	std::vector<Caster> cast;
	auto addCaster = [&]( int k, const double off[3], double s ) {
		Caster c;
		c.slot = k;
		c.mesh = int( rng() % std::uint32_t( kMeshes ) );
		for ( int i = 0; i < 3; i++ )
			c.c[i] = lights[k].pos[i] + off[i];
		c.s = s;
		c.ax = u( rng );
		c.ay = u( rng );
		c.az = u( rng ) + 1.2;
		c.ang = 3.14159 * u( rng );
		cast.push_back( c );
	};
	std::vector<int> slotFirst( kSlots ), slotCount( kSlots );
	for ( int k = 0; k < kSlots; k++ ) {
		const double r = lights[k].radius;
		slotFirst[k] = int( cast.size() );
		static const double ax6[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for ( int a = 0; a < 6; a++ ) {  // one on every face axis, half the radius out
			const double off[3] = { ax6[a][0] * 0.5 * r + 0.05 * r * u( rng ), ax6[a][1] * 0.5 * r + 0.05 * r * u( rng ),
				ax6[a][2] * 0.5 * r + 0.05 * r * u( rng ) };
			addCaster( k, off, 0.15 * r );
		}
		for ( int i = 0; i < 8; i++ ) {  // random ones
			double off[3] = { u( rng ), u( rng ), u( rng ) };
			const double l = std::sqrt( off[0] * off[0] + off[1] * off[1] + off[2] * off[2] ) + 1e-6, d = r * ( 0.25 + 0.6 * ( 0.5 + 0.5 * u( rng ) ) );
			for ( double & o : off )
				o = o / l * d;
			addCaster( k, off, 0.08 * r + 0.1 * r * ( 0.5 + 0.5 * u( rng ) ) );
		}
		{  // inside the near clip: every fragment discarded
			const double off[3] = { 0.3 * lights[k].nearClip, -0.2 * lights[k].nearClip, 0.1 * lights[k].nearClip };
			addCaster( k, off, 0.25 * lights[k].nearClip );
		}
		{  // straddling the light 1-2 units away (crosses faces, partly inside the near clip)
			const double off[3] = { 1.5, -1.2, 0.8 };
			addCaster( k, off, 3.0 * lights[k].nearClip );
		}
		{  // past the radius: clamped at 1, clipped by the far plane
			const double off[3] = { -0.7 * r, 0.6 * r, -0.65 * r };
			addCaster( k, off, 0.3 * r );
		}
		slotCount[k] = int( cast.size() ) - slotFirst[k];
	}
	const int kMover = int( cast.size() );  // slot 3's mover: drawn by a LOAD op into face 0 (+x) over the kept casters
	{
		const double off[3] = { 0.3 * lights[3].radius, 0.02 * lights[3].radius, -0.03 * lights[3].radius };
		addCaster( 3, off, 0.12 * lights[3].radius );
	}
	const int kCasters = int( cast.size() );
	auto mvOf = [&]( double move, std::vector<float> & mats ) {
		mats.assign( std::size_t( kCasters ) * 16, 0.0f );
		for ( int i = 0; i < kCasters; i++ ) {
			const Caster & c = cast[i];
			double R[3][3], w[3], v[3];
			rot3( c.ax, c.ay, c.az, c.ang + 0.4 * move, R );
			const double r = lights[c.slot].radius;
			for ( int a = 0; a < 3; a++ )
				w[a] = c.c[a] + move * 0.04 * r * ( a == 0 ? 1.0 : a == 1 ? -0.7 : 0.5 );
			viewOf( w, v );
			// view-space model: (Rv-aligned) turn and scale; columns of the 3x3 = view axes of the model's axes
			float * m = &mats[std::size_t( i ) * 16];
			for ( int col = 0; col < 3; col++ )
				for ( int row = 0; row < 3; row++ )
					m[col * 4 + row] = float( R[row][col] * c.s * sc );
			m[12] = float( v[0] );
			m[13] = float( v[1] );
			m[14] = float( v[2] );
			m[15] = 1.0f;
		}
	};
	std::vector<float> mats[2];
	mvOf( 0.0, mats[0] );
	mvOf( 1.0, mats[1] );

	// ---- commands and ops. Plain path: per slot and face, a clear op with the slot's casters; slot 2 faces 4 and 5 clear only;
	// slot 3 face 0 then a LOAD op with the mover. Frame 2 (held = 1): no op for slot 1.
	std::vector<std::uint32_t> cmds;
	auto cmdOf = [&]( int ci ) {
		const Mesh & m = meshes[cast[ci].mesh];
		const std::uint32_t c[5] = { m.count, 1, m.firstIndex, std::uint32_t( m.baseVertex ), std::uint32_t( ci ) };
		cmds.insert( cmds.end(), c, c + 5 );
	};
	struct OpDef
	{
		int slot, face;
		bool clear;
		std::uint32_t first, n;
	};
	std::vector<OpDef> defs;
	std::uint32_t slotCmd[kSlots];
	for ( int k = 0; k < kSlots; k++ ) {
		slotCmd[k] = std::uint32_t( cmds.size() / 5 );
		for ( int i = 0; i < slotCount[k]; i++ )
			cmdOf( slotFirst[k] + i );
	}
	const std::uint32_t moverCmd = std::uint32_t( cmds.size() / 5 );
	cmdOf( kMover );
	auto opsFor = [&]( int held ) {
		std::vector<OpDef> d;
		for ( int k = 0; k < kSlots; k++ ) {
			if ( k == held )
				continue;
			for ( int fc = 0; fc < 6; fc++ ) {
				const bool coneSkip = k == 2 && fc >= 4;
				d.push_back( { k, fc, true, slotCmd[k], coneSkip ? 0u : std::uint32_t( slotCount[k] ) } );
				if ( k == 3 && fc == 0 )
					d.push_back( { k, fc, false, moverCmd, 1u } );
			}
		}
		return d;
	};
	std::printf( "pool %zu vertices, %zu indices, %d casters (%zu commands), frame 1 %zu ops, frame 2 %zu ops\n", pos.size() / 3, idx.size(),
		kCasters, cmds.size() / 5, opsFor( -1 ).size(), opsFor( 1 ).size() );

	// ---- GL: the pool buffers and the VAO (location 0, the u16 elements)
	GLuint bPos = 0, bIdx = 0, vao = 0;
	f.genBuffers( 1, &bPos );
	f.genBuffers( 1, &bIdx );
	f.genVertexArrays( 1, &vao );
	f.bindVertexArray( vao );
	f.bindBuffer( kArrayBuf, bPos );
	f.bufferData( kArrayBuf, std::ptrdiff_t( pos.size() * 4 ), pos.data(), kStatic );
	f.enableVertexAttribArray( 0 );
	f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	f.bindBuffer( kElemBuf, bIdx );
	f.bufferData( kElemBuf, std::ptrdiff_t( idx.size() * 2 ), idx.data(), kStatic );
	f.bindVertexArray( 0 );

	// ---- targets: GL's own cube array (the reference) and the shared one (celllamps keys shadowShareKey(r))
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "LAMPVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	const std::string no = WwVkPresent::lampRefusal();
	if ( !no.empty() ) {
		std::printf( "LAMPVIEW GATE FAILED (lampRefusal: %s)\n", no.c_str() );
		return 2;
	}
	static unsigned keyHome, otherHome;
	const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &keyHome );
	GLuint refT = 0, shT = 0;
	glGenTextures( 1, &refT );
	glBindTexture( kCubeArray, refT );
	f.texImage3D( kCubeArray, 0, GLint( kD16 ), F, F, kLayers, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
	glTexParameteri( kCubeArray, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( kCubeArray, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( kCubeArray, 0x813D, 0 );  // GL_TEXTURE_MAX_LEVEL
	glBindTexture( kCubeArray, 0 );
	if ( !WwVkPresent::sharedTarget( key, 3, F, F, kLayers, kD16, shT, why ) ) {
		std::printf( "LAMPVIEW GATE FAILED (sharedTarget: %s)\n", why.c_str() );
		return 2;
	}
	GLuint fbo = 0;
	f.genFramebuffers( 1, &fbo );
	auto attach = [&]( GLuint tex, int layer ) {
		f.bindFramebuffer( kFb, fbo );
		f.framebufferTextureLayer( kFb, kDepthAtt, tex, 0, layer );
		glDrawBuffer( GL_NONE );
		glReadBuffer( GL_NONE );
		return f.checkFramebufferStatus( kFb ) == kComplete;
	};
	// junk depth in every layer but the held slot's (a skipped clear shows)
	auto junk = [&]( GLuint tex, int held ) {
		glDisable( GL_SCISSOR_TEST );
		glDepthMask( GL_TRUE );
		glClearDepth( 0.37 );
		bool ok = true;
		for ( int l = 0; l < kLayers; l++ ) {
			if ( l / 6 == held )
				continue;
			ok = ok && attach( tex, l );
			glViewport( 0, 0, F, F );
			glClear( GL_DEPTH_BUFFER_BIT );
		}
		glClearDepth( 1.0 );
		return ok;
	};
	auto readAll = [&]( GLuint tex, std::vector<std::uint16_t> & out ) {
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		out.assign( std::size_t( F ) * F * kLayers, 0 );
		for ( int l = 0; l < kLayers; l++ ) {
			attach( tex, l );
			glReadPixels( 0, 0, F, F, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, out.data() + std::size_t( l ) * F * F );
		}
	};
	auto relOf = [&]( int k, float rel[16] ) { WwVkPresent::lampRelFromView( rotA, sc, cam, lights[k].pos, rel ); };
	// GL: celllamps' plain pass state and loop (a clear op clears the layer; a LOAD op does not)
	auto runGl = [&]( const std::vector<OpDef> & d, const std::vector<float> & mv, int held, std::vector<std::uint16_t> & out ) {
		bool ok = junk( refT, held );
		f.useProgram( prog );
		if ( lAt >= 0 )
			f.uniform1i( lAt, 0 );
		f.bindVertexArray( vao );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( 0x8037 );  // GL_POLYGON_OFFSET_FILL
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glViewport( 0, 0, F, F );
		for ( const OpDef & o : d ) {
			ok = ok && attach( refT, 6 * o.slot + o.face );
			glViewport( 0, 0, F, F );
			float rel[16], clip[16];
			relOf( o.slot, rel );
			WwVkPresent::lampFaceClip( o.face, lights[o.slot].radius, clip );
			f.uniformMatrix4fv( lRel, 1, GL_FALSE, rel );
			f.uniform2f( lRn, lights[o.slot].radius, lights[o.slot].nearClip );
			if ( o.clear ) {
				glClearDepth( 1.0 );
				glClear( GL_DEPTH_BUFFER_BIT );
			}
			f.uniformMatrix4fv( lClip, 1, GL_FALSE, clip );
			for ( std::uint32_t c = o.first; c < o.first + o.n; c++ ) {
				const std::uint32_t * cm = &cmds[std::size_t( c ) * 5];
				f.uniformMatrix4fv( lMv, 1, GL_FALSE, &mv[std::size_t( cm[4] ) * 16] );
				f.drawElementsBaseVertex( GL_TRIANGLES, GLsizei( cm[0] ), GL_UNSIGNED_SHORT,
					reinterpret_cast<const void *>( std::size_t( cm[2] ) * 2 ), GLint( cm[3] ) );
			}
		}
		f.bindVertexArray( 0 );
		readAll( refT, out );
		return ok && glGetError() == GL_NO_ERROR;
	};
	enum Red { kNone, kFaceOrder, kSlotOffset, kRadius, kNoClear, kMoverCleared };
	auto runVk = [&]( Red red, const std::vector<OpDef> & d, const std::vector<float> & mv, int held, std::vector<std::uint16_t> & out ) {
		bool ok = junk( shT, held );
		WwVkPresent::LampRun run;
		run.tex = shT;
		run.key = key;
		run.face = F;
		run.layers = kLayers;
		run.pos = pos.data();
		run.posBytes = pos.size() * 4;
		run.idx = idx.data();
		run.idxBytes = idx.size() * 2;
		run.mats = mv.data();
		run.nMats = std::size_t( kCasters );
		run.cmds = cmds.data();
		run.nCmds = cmds.size() / 5;
		for ( const OpDef & o : d ) {
			WwVkPresent::LampOp op;
			op.layer = 6 * o.slot + o.face;
			if ( red == kSlotOffset )
				op.layer = ( op.layer + 6 ) % kLayers;
			op.clear = red == kNoClear ? false : red == kMoverCleared ? true : o.clear;
			relOf( o.slot, op.rel );
			const float radius = red == kRadius ? lights[o.slot].radius * 1.01f : lights[o.slot].radius;
			WwVkPresent::lampFaceClip( red == kFaceOrder ? ( o.face ^ 1 ) : o.face, lights[o.slot].radius, op.clip );
			op.rn[0] = radius;
			op.rn[1] = lights[o.slot].nearClip;
			op.first = o.first;
			op.n = o.n;
			run.ops.push_back( op );
		}
		std::string w;
		if ( !WwVkPresent::lampRun( run, w ) ) {
			std::printf( "LAMPVIEW lampRun refused: %s\n", w.c_str() );
			return false;
		}
		readAll( shT, out );
		return ok && glGetError() == GL_NO_ERROR;
	};
	// verdict per layer: a face drawn into (an op with casters) EXACT / BOUND; a clear-only face CLEAR; the held slot EXACT
	auto judge = [&]( const std::vector<OpDef> & d, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b, int fr,
					 bool print, Diff * sum, const std::vector<std::uint16_t> * prevA = nullptr,
					 const std::vector<std::uint16_t> * prevB = nullptr ) {
		bool ok = true;
		const std::size_t n = std::size_t( F ) * F;
		for ( int l = 0; l < kLayers; l++ ) {
			bool drawn = false, touched = false;
			for ( const OpDef & o : d )
				if ( 6 * o.slot + o.face == l ) {
					touched = true;
					drawn = drawn || o.n > 0;
				}
			Diff v = compare( a.data() + std::size_t( l ) * n, b.data() + std::size_t( l ) * n, n );
			if ( sum )
				sum->add( v );
			const char * word = "FAIL";
			bool lok = false;
			if ( !touched ) {  // held: each side keeps its own frame 1 bytes, EXACT
				if ( !prevA || !prevB || prevA->size() != a.size() || prevB->size() != b.size() ) {
					lok = false;
				} else {
					const Diff va = compare( a.data() + std::size_t( l ) * n, prevA->data() + std::size_t( l ) * n, n );
					const Diff vb = compare( b.data() + std::size_t( l ) * n, prevB->data() + std::size_t( l ) * n, n );
					v = va;
					lok = va.diff == 0 && va.covered > 0 && vb.diff == 0 && vb.covered > 0;
				}
				word = lok ? "EXACT" : "FAIL";
			} else if ( !drawn ) {
				lok = v.clearOk();
				word = lok ? "CLEAR" : "FAIL";
			} else {
				lok = v.ok();
				word = !lok ? "FAIL" : v.diff == 0 ? "EXACT" : "BOUND";
			}
			ok = ok && lok;
			if ( print )
				std::printf( "LAMPVIEW frame%d slot%d/face%d %s%s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; set vk %zu gl %zu)\n",
					fr, l / 6, l % 6, word, touched ? "" : " held", v.diff, v.covered, v.past1, v.maxu, v.vkSet, v.refSet );
		}
		return ok;
	};

	bool ok = true;
	std::vector<std::uint16_t> ref[2], vkO, vk1;
	const int helds[2] = { -1, 1 };
	for ( int fr = 0; fr < 2; fr++ ) {
		const std::vector<OpDef> d = opsFor( helds[fr] );
		if ( fr == 1 ) {  // an unrelated shared target between the frames: the framebuffer cache's key moves
			GLuint other = 0;
			const std::uint64_t g0 = sharedGen();
			if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &otherHome ), 2, 64, 64, 1, kD16, other, why ) ) {
				std::printf( "LAMPVIEW frame2 FAIL (the unrelated shared target: %s)\n", why.c_str() );
				ok = false;
			}
			std::printf( "LAMPVIEW sharedGen %llu -> %llu (an unrelated shared target)\n", (unsigned long long)g0,
				(unsigned long long)sharedGen() );
		}
		if ( !runGl( d, mats[fr], helds[fr], ref[fr] ) || !runVk( kNone, d, mats[fr], helds[fr], vkO ) ) {
			std::printf( "LAMPVIEW frame%d FAIL (a run failed)\n", fr + 1 );
			ok = false;
			continue;
		}
		ok = judge( d, vkO, ref[fr], fr + 1, true, nullptr, fr ? &vk1 : nullptr, fr ? &ref[0] : nullptr ) && ok;
		if ( fr == 0 )
			vk1 = vkO;
	}
	// the reds against frame 1's ops and references: each must FAIL
	const struct
	{
		const char * name;
		Red red;
	} reds[5] = { { "face order (each face with face ^ 1's clip)", kFaceOrder }, { "slot offset (layer + 6)", kSlotOffset },
		{ "radius (radius * 1.01)", kRadius }, { "clear skipped (no op clears)", kNoClear },
		{ "mover cleared (the LOAD op set to clear)", kMoverCleared } };
	const std::vector<OpDef> d1 = opsFor( -1 );
	for ( const auto & r : reds ) {
		Diff all;
		const bool ran = runVk( r.red, d1, mats[0], -1, vkO );
		const bool passes = ran && !ref[0].empty() && judge( d1, vkO, ref[0], 1, false, &all );
		std::printf( "LAMPVIEW RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n", passes ? "PASSES" : "FAIL",
			r.name, all.diff, all.covered, all.past1, all.maxu );
		ok = ok && !passes;
	}
	const WwVkPresent::LampStat st = WwVkPresent::takeLampStat();
	std::printf( "LAMPVIEW stat: %d runs on Vulkan, %d failed (%s), %d ops, %d draws, %.3f ms a run (max %.3f), %d pipelines built in %.1f ms\n",
		st.runs, st.fails, st.failWhy.c_str(), st.ops, st.draws, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
	f.bindFramebuffer( kFb, 0 );
	WwVkPresent::sharedTargetDrop( key );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &otherHome ) );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "LAMPVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
