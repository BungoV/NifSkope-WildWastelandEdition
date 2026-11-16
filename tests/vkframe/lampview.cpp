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
#include <algorithm>
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
	void( WINAPI * uniform1f )( GLint, GLfloat ) = nullptr;
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
	void( WINAPI * blitFramebuffer )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;  // VKLAMP4
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
		&& load( f.uniform1f, "glUniform1f" ) && load( f.genBuffers, "glGenBuffers" ) && load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.genVertexArrays, "glGenVertexArrays" ) && load( f.bindVertexArray, "glBindVertexArray" )
		&& load( f.enableVertexAttribArray, "glEnableVertexAttribArray" ) && load( f.vertexAttribPointer, "glVertexAttribPointer" )
		&& load( f.drawElementsBaseVertex, "glDrawElementsBaseVertex" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTextureLayer, "glFramebufferTextureLayer" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" ) && load( f.texImage3D, "glTexImage3D" )
		&& load( f.blitFramebuffer, "glBlitFramebuffer" );
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
	const GLint lBase = f.getUniformLocation( prog, "BaseMap" ), lUvS = f.getUniformLocation( prog, "uvScale" );
	const GLint lUvO = f.getUniformLocation( prog, "uvOffset" ), lAlpha = f.getUniformLocation( prog, "alpha" );
	const GLint lThr = f.getUniformLocation( prog, "alphaThreshold" );
	if ( lRel < 0 || lClip < 0 || lRn < 0 || lMv < 0 || lAt < 0 || lBase < 0 || lUvS < 0 || lUvO < 0 || lAlpha < 0 || lThr < 0 ) {
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
	// lane VKLAMP2: each mesh's vertex count and content generation (the pool's key data)
	std::vector<std::uint32_t> meshVerts;
	for ( int m = 0; m < kMeshes; m++ )
		meshVerts.push_back( std::uint32_t( ( m + 1 < kMeshes ? meshes[m + 1].baseVertex : std::int32_t( pos.size() / 3 ) ) - meshes[m].baseVertex ) );
	std::vector<std::uint64_t> meshGen( std::size_t( kMeshes ), 1 );
	// lane VKLAMP2: location 7 UVs from the positions (no rng draw: the cases before keep their fixture)
	std::vector<float> uvs;
	for ( std::size_t v = 0; v + 2 < pos.size(); v += 3 ) {
		uvs.push_back( 2.3f * pos[v] + 0.7f * pos[v + 2] );
		uvs.push_back( 1.9f * pos[v + 1] - 0.4f * pos[v + 2] );
	}

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
	GLuint bPos = 0, bIdx = 0, bUv = 0, vao = 0;
	f.genBuffers( 1, &bPos );
	f.genBuffers( 1, &bIdx );
	f.genBuffers( 1, &bUv );
	f.genVertexArrays( 1, &vao );
	f.bindVertexArray( vao );
	f.bindBuffer( kArrayBuf, bPos );
	f.bufferData( kArrayBuf, std::ptrdiff_t( pos.size() * 4 ), pos.data(), kStatic );
	f.enableVertexAttribArray( 0 );
	f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	f.bindBuffer( kArrayBuf, bUv );
	f.bufferData( kArrayBuf, std::ptrdiff_t( uvs.size() * 4 ), uvs.data(), kStatic );
	f.enableVertexAttribArray( 7 );
	f.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 8, nullptr );
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
	const std::vector<std::uint32_t> & lmvCmds = cmds;
	auto runVk = [&]( Red red, const std::vector<OpDef> & d, const std::vector<float> & mv, int held, std::vector<std::uint16_t> & out,
					 const std::vector<std::uint32_t> * cmdsIn = nullptr ) {
		const std::vector<std::uint32_t> & cmds = cmdsIn ? *cmdsIn : lmvCmds;
		bool ok = junk( shT, held );
		WwVkPresent::LampRun run;
		run.tex = shT;
		run.key = key;
		run.face = F;
		run.layers = kLayers;
		// lane VKLAMP2: the pool's API: one LampMesh a mesh (key = its index + 1, gen = meshGen), draws {mesh, matrix}
		std::vector<WwVkPresent::LampMesh> lm( meshes.size() );
		for ( std::size_t m = 0; m < meshes.size(); m++ ) {
			lm[m].key = m + 1;
			lm[m].gen = meshGen[m];
			lm[m].pos = pos.data() + std::size_t( meshes[m].baseVertex ) * 3;
			lm[m].verts = meshVerts[m];
			lm[m].idx = idx.data() + meshes[m].firstIndex;
			lm[m].indices = meshes[m].count;
		}
		std::vector<std::uint32_t> dr;
		for ( std::size_t c = 0; c < cmds.size() / 5; c++ ) {
			std::uint32_t me = 0;
			while ( me < meshes.size() && meshes[me].firstIndex != cmds[c * 5 + 2] )
				me++;
			dr.push_back( me );
			dr.push_back( cmds[c * 5 + 4] );
		}
		run.meshes = lm.data();
		run.nMeshes = lm.size();
		run.mats = mv.data();
		run.nMats = std::size_t( kCasters );
		run.draws = dr.data();
		run.nDraws = dr.size() / 2;
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
				std::printf( "LAMPVIEW %s%d slot%d/face%d %s%s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; set vk %zu gl %zu)\n",
					fr ? "frame" : "alpha", fr ? fr : 1, l / 6, l % 6, word, touched ? "" : " held", v.diff, v.covered, v.past1, v.maxu, v.vkSet, v.refSet );
		}
		return ok;
	};

	bool ok = true;
	// lane VKLAMP2: the pool's counts since the last take, against the bar (-1: not stated); the stat line sums them
	WwVkPresent::LampStat tot;
	auto poolTake = [&]( const char * what, int copies, int hits, int regen, int repacks ) {
		const WwVkPresent::LampStat s = WwVkPresent::takeLampStat();
		tot.runs += s.runs;
		tot.fails += s.fails;
		tot.ops += s.ops;
		tot.draws += s.draws;
		tot.cpuMs += s.cpuMs;
		tot.maxMs = std::max( tot.maxMs, s.maxMs );
		tot.pipelines = std::max( tot.pipelines, s.pipelines );
		tot.buildMs = std::max( tot.buildMs, s.buildMs );
		if ( tot.failWhy.empty() )
			tot.failWhy = s.failWhy;
		tot.copies += s.copies;
		tot.hits += s.hits;
		tot.regen += s.regen;
		tot.repacks += s.repacks;
		tot.redSkips += s.redSkips;
		tot.copyKb += s.copyKb;
		const bool good = ( copies < 0 || s.copies == copies ) && ( hits < 0 || s.hits == hits ) && ( regen < 0 || s.regen == regen )
			&& ( repacks < 0 || s.repacks == repacks );
		std::printf( "LAMPVIEW pool %s copies=%d hits=%d regen=%d repacks=%d redSkips=%d (%.1f KiB copied) %s\n", what, s.copies, s.hits, s.regen,
			s.repacks, s.redSkips, s.copyKb, good ? "OK" : "FAIL" );
		return good;
	};
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
		} else {
			ok = judge( d, vkO, ref[fr], fr + 1, true, nullptr, fr ? &vk1 : nullptr, fr ? &ref[0] : nullptr ) && ok;
			if ( fr == 0 )
				vk1 = vkO;
		}
		ok = poolTake( fr ? "frame2" : "frame1", fr ? 0 : 4, fr ? 4 : 0, -1, -1 ) && ok;
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
	ok = poolTake( "reds", 0, -1, -1, -1 ) && ok;

	// ---- lane VKLAMP2: GL's alpha-tested casters on a Vulkan-written layer (the app's route) against GL doing every face.
	// 4 of each slot's 8 random casters alpha-tested; a 64 x 64 checker BaseMap (8 px cells, alpha 0 / 1), threshold 0.5.
	std::vector<char> isAt( std::size_t( kCasters ), 0 );
	for ( int k = 0; k < kSlots; k++ )
		for ( int i = 1; i < 8; i += 2 )
			isAt[std::size_t( slotFirst[k] + 6 + i )] = 1;
	GLuint baseT = 0;
	{
		std::vector<std::uint8_t> px( 64 * 64 * 4 );
		for ( int y = 0; y < 64; y++ )
			for ( int x = 0; x < 64; x++ ) {
				std::uint8_t * p = &px[std::size_t( y * 64 + x ) * 4];
				p[0] = p[1] = p[2] = 200;
				p[3] = ( ( x / 8 + y / 8 ) & 1 ) ? 255 : 0;
			}
		glGenTextures( 1, &baseT );
		glBindTexture( GL_TEXTURE_2D, baseT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data() );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, 0x813D, 0 );  // GL_TEXTURE_MAX_LEVEL
	}
	// the Vulkan arm's commands: each slot's solid casters only (the app's build loop skips isAt), then the mover
	std::vector<std::uint32_t> solidCmds;
	std::uint32_t slotSolid[kSlots], slotSolidN[kSlots];
	for ( int k = 0; k < kSlots; k++ ) {
		slotSolid[k] = std::uint32_t( solidCmds.size() / 5 );
		for ( std::uint32_t c = slotCmd[k]; c < slotCmd[k] + std::uint32_t( slotCount[k] ); c++ )
			if ( !isAt[cmds[std::size_t( c ) * 5 + 4]] )
				solidCmds.insert( solidCmds.end(), &cmds[std::size_t( c ) * 5], &cmds[std::size_t( c ) * 5] + 5 );
		slotSolidN[k] = std::uint32_t( solidCmds.size() / 5 ) - slotSolid[k];
	}
	const std::uint32_t moverSolid = std::uint32_t( solidCmds.size() / 5 );
	solidCmds.insert( solidCmds.end(), &cmds[std::size_t( moverCmd ) * 5], &cmds[std::size_t( moverCmd ) * 5] + 5 );
	std::vector<OpDef> dSolid = d1;
	for ( OpDef & o : dSolid ) {
		const bool isMover = !o.clear;
		o.first = isMover ? moverSolid : slotSolid[o.slot];
		o.n = isMover ? 1u : o.n ? slotSolidN[o.slot] : 0u;
	}
	enum AlphaArm { kRefAll, kRefSolid, kAlphaOnly };
	enum AlphaRed { kANone, kAClear, kAAlways, kATestOff };
	// GL: celllamps' plain pass state; kAlphaOnly = the app's GL loop on the Vulkan route (no clear, isAt casters only)
	auto glCasters = [&]( GLuint tex, AlphaArm arm, AlphaRed red, const std::vector<OpDef> & d, const std::vector<float> & mv ) {
		bool ok = true;
		f.useProgram( prog );
		glBindTexture( GL_TEXTURE_2D, baseT );
		f.uniform1i( lBase, 0 );
		f.uniform2f( lUvS, 1.0f, 1.0f );
		f.uniform2f( lUvO, 0.0f, 0.0f );
		f.uniform1f( lAlpha, 1.0f );
		f.uniform1f( lThr, 0.5f );
		f.bindVertexArray( vao );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( 0x8037 );  // GL_POLYGON_OFFSET_FILL
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( red == kAAlways ? GL_ALWAYS : GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		for ( const OpDef & o : d ) {
			ok = ok && attach( tex, 6 * o.slot + o.face );
			glViewport( 0, 0, F, F );
			float rel[16], clip[16];
			relOf( o.slot, rel );
			WwVkPresent::lampFaceClip( o.face, lights[o.slot].radius, clip );
			f.uniformMatrix4fv( lRel, 1, GL_FALSE, rel );
			f.uniform2f( lRn, lights[o.slot].radius, lights[o.slot].nearClip );
			f.uniformMatrix4fv( lClip, 1, GL_FALSE, clip );
			if ( arm == kAlphaOnly ? ( red == kAClear && o.clear ) : o.clear ) {
				glClearDepth( 1.0 );
				glClear( GL_DEPTH_BUFFER_BIT );
			}
			for ( std::uint32_t c = o.first; c < o.first + o.n; c++ ) {
				const std::uint32_t * cm = &cmds[std::size_t( c ) * 5];
				const bool at = isAt[cm[4]] != 0;
				if ( ( arm == kRefSolid && at ) || ( arm == kAlphaOnly && !at ) )
					continue;
				f.uniform1i( lAt, at && red != kATestOff ? 1 : 0 );
				f.uniformMatrix4fv( lMv, 1, GL_FALSE, &mv[std::size_t( cm[4] ) * 16] );
				f.drawElementsBaseVertex( GL_TRIANGLES, GLsizei( cm[0] ), GL_UNSIGNED_SHORT,
					reinterpret_cast<const void *>( std::size_t( cm[2] ) * 2 ), GLint( cm[3] ) );
			}
		}
		f.uniform1i( lAt, 0 );
		f.bindVertexArray( 0 );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glDepthFunc( GL_LEQUAL );
		return ok && glGetError() == GL_NO_ERROR;
	};
	std::vector<std::uint16_t> refA, refS, vkA;
	bool aRan = junk( refT, -1 ) && glCasters( refT, kRefAll, kANone, d1, mats[0] );
	readAll( refT, refA );
	aRan = aRan && junk( refT, -1 ) && glCasters( refT, kRefSolid, kANone, d1, mats[0] );
	readAll( refT, refS );
	std::size_t alphaWon = 0;
	for ( std::size_t i = 0; i < refA.size(); i++ )
		alphaWon += refA[i] != refS[i];
	auto alphaArm = [&]( AlphaRed red ) {
		bool r = runVk( kNone, dSolid, mats[0], -1, vkA, &solidCmds ) && glCasters( shT, kAlphaOnly, red, d1, mats[0] );
		readAll( shT, vkA );
		return r && glGetError() == GL_NO_ERROR;
	};
	if ( !aRan || !alphaArm( kANone ) ) {
		std::printf( "LAMPVIEW alpha FAIL (a run failed)\n" );
		ok = false;
	} else {
		ok = judge( d1, vkA, refA, 0, true, nullptr ) && ok;
	}
	std::printf( "LAMPVIEW alpha casters win %zu texels over the solid-only picture (%s)\n", alphaWon, alphaWon ? "ok" : "FAIL: proves nothing" );
	ok = ok && alphaWon > 0;
	const struct
	{
		const char * name;
		AlphaRed red;
	} aReds[3] = { { "alpha clear (GL clears before its alpha draw)", kAClear }, { "alpha depth func (GL_ALWAYS, not LEQUAL)", kAAlways },
		{ "alpha test off (shAlphaTest 0 on the alpha draw)", kATestOff } };
	for ( const auto & r : aReds ) {
		Diff all;
		const bool ran = alphaArm( r.red );
		const bool passes = ran && !refA.empty() && judge( d1, vkA, refA, 0, false, &all );
		std::printf( "LAMPVIEW RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n", passes ? "PASSES" : "FAIL",
			r.name, all.diff, all.covered, all.past1, all.maxu );
		if ( r.red == kATestOff && all.diff == 0 ) {
			std::printf( "LAMPVIEW alpha FAIL (the alpha test discards nothing)\n" );
			ok = false;
		}
		ok = ok && !passes;
	}
	ok = poolTake( "alpha", 0, -1, -1, -1 ) && ok;

	// ---- lane VKLAMP2: the pool's new generation (frame 3) and its growth (frame 4), all slots, frame 2's matrices
	const std::vector<OpDef> dAll = opsFor( -1 );
	auto poolFrame = [&]( int fr ) {
		std::vector<std::uint16_t> refP;
		if ( !runGl( dAll, mats[1], -1, refP ) || !runVk( kNone, dAll, mats[1], -1, vkO ) ) {
			std::printf( "LAMPVIEW frame%d FAIL (a run failed)\n", fr );
			return false;
		}
		return judge( dAll, vkO, refP, fr, true, nullptr );
	};
	auto upload = [&]() {
		f.bindVertexArray( vao );
		f.bindBuffer( kArrayBuf, bPos );
		f.bufferData( kArrayBuf, std::ptrdiff_t( pos.size() * 4 ), pos.data(), kStatic );
		f.bindBuffer( kArrayBuf, bUv );
		f.bufferData( kArrayBuf, std::ptrdiff_t( uvs.size() * 4 ), uvs.data(), kStatic );
		f.bindBuffer( kElemBuf, bIdx );
		f.bufferData( kElemBuf, std::ptrdiff_t( idx.size() * 2 ), idx.data(), kStatic );
		f.bindVertexArray( 0 );
		f.bindBuffer( kArrayBuf, 0 );
	};
	{  // frame 3: the plate (mesh 3) re-specified in place: same sizes, a new amplitude, a new generation
		for ( std::uint32_t v = 0; v < meshVerts[3]; v++ ) {
			float * p = &pos[( std::size_t( meshes[3].baseVertex ) + v ) * 3];
			p[2] = 0.4f * std::sin( 2.0f * p[0] + 0.5f ) * std::cos( 3.0f * p[1] );
		}
		upload();
		meshGen[3]++;
		ok = poolFrame( 3 ) && ok;
		ok = poolTake( "frame3", 1, -1, 1, -1 ) && ok;
	}
	{  // frame 4: the mover takes a new 81 x 81 plate (6561 vertices: the positions pass the pool's 64 KiB floor)
		beginMesh();
		const int n = 80;
		for ( int y = 0; y <= n; y++ )
			for ( int x = 0; x <= n; x++ ) {
				const float px = -1.0f + 2.0f * float( x ) / n, py = -1.0f + 2.0f * float( y ) / n, pz = 0.2f * std::sin( 4.0f * px ) * std::cos( 3.0f * py );
				pos.push_back( px );
				pos.push_back( py );
				pos.push_back( pz );
				uvs.push_back( 2.3f * px + 0.7f * pz );
				uvs.push_back( 1.9f * py - 0.4f * pz );
			}
		for ( int y = 0; y < n; y++ )
			for ( int x = 0; x < n; x++ ) {
				const std::uint16_t a = std::uint16_t( y * ( n + 1 ) + x ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + n + 1 ),
					d = std::uint16_t( c + 1 );
				const std::uint16_t q[6] = { a, b, d, a, d, c };
				idx.insert( idx.end(), q, q + 6 );
			}
		endMesh();
		meshVerts.push_back( std::uint32_t( ( n + 1 ) * ( n + 1 ) ) );
		meshGen.push_back( 1 );
		upload();
		cast[std::size_t( kMover )].mesh = int( meshes.size() ) - 1;
		cmds.clear();
		for ( int k = 0; k < kSlots; k++ )
			for ( int i = 0; i < slotCount[k]; i++ )
				cmdOf( slotFirst[k] + i );
		cmdOf( kMover );
		ok = poolFrame( 4 ) && ok;
		ok = poolTake( "frame4", 5, -1, -1, 1 ) && ok;
	}

	// ---- lane VKLAMP4: the lamp tier's stored maps (celllamps.cpp lampShadowPass) on frame 4's fixture. Slot 0 = a full lamp
	// (store blocks 0 and 1), slot 3 = a wall lamp (back face 4, block 2); slot 3's mover (frame 4's plate) over the restored map
	const int kStoreLayers = 9, kBack = 4, kBase3 = 6 * 3;
	static unsigned storeHome;
	const std::uintptr_t storeKey = reinterpret_cast<std::uintptr_t>( &storeHome );
	GLuint refStore = 0, shStore = 0, fboR = 0, qVao = 0, qPos = 0, qIdx = 0;
	glGenTextures( 1, &refStore );
	glBindTexture( kTex2DArray, refStore );
	f.texImage3D( kTex2DArray, 0, GLint( kD16 ), F, F, kStoreLayers, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
	glTexParameteri( kTex2DArray, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( kTex2DArray, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( kTex2DArray, 0x813D, 0 );  // GL_TEXTURE_MAX_LEVEL
	glBindTexture( kTex2DArray, 0 );
	const bool sOk = WwVkPresent::sharedTarget( storeKey, 2, F, F, kStoreLayers, kD16, shStore, why );
	if ( !sOk ) {
		std::printf( "LAMPVIEW stored FAIL (sharedTarget, the store: %s)\n", why.c_str() );
		ok = false;
	}
	f.genFramebuffers( 1, &fboR );
	// the wall quad (lampWallQuad, 0.6 radius out on the back's axis) and the world -> view matrix GL's quad draw takes
	float quad[12], viewM[16] = {};
	WwVkPresent::lampWallQuad( lights[3].pos, lights[3].radius, kBack, 0.6f * lights[3].radius, quad );
	for ( int j = 0; j < 3; j++ ) {
		double t = 0.0;
		for ( int row = 0; row < 3; row++ ) {
			viewM[row * 4 + j] = sc * rotA[j * 3 + row];
			t += double( sc * rotA[j * 3 + row] ) * cam[row];
		}
		viewM[12 + j] = float( -t );
	}
	viewM[15] = 1.0f;
	{
		const std::uint16_t qi[6] = { 0, 1, 2, 0, 2, 3 };
		f.genBuffers( 1, &qPos );
		f.genBuffers( 1, &qIdx );
		f.genVertexArrays( 1, &qVao );
		f.bindVertexArray( qVao );
		f.bindBuffer( kArrayBuf, qPos );
		f.bufferData( kArrayBuf, std::ptrdiff_t( sizeof quad ), quad, kStatic );
		f.enableVertexAttribArray( 0 );
		f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
		f.bindBuffer( kElemBuf, qIdx );
		f.bufferData( kElemBuf, std::ptrdiff_t( sizeof qi ), qi, kStatic );
		f.bindVertexArray( 0 );
		f.bindBuffer( kArrayBuf, 0 );
	}
	// celllamps' steps in its order: kind 0 a face draw (into the store or a cube face), 1 a blit store -> cube, 2 the wall quad
	struct SStep
	{
		int kind;
		bool store;
		int slot, face, layer, vx, vy;
		WwVkPresent::LampRect cut;  // w 0: the whole layer; a blit: its destination rect
		bool clear;
		std::uint32_t first, n;
		int src;                    // a blit's store layer
		WwVkPresent::LampRect a;    // a blit's source rect
	};
	const int blk0[2] = { 0, 1 }, blk3 = 2;
	const std::uint32_t kStill = 10, kMov = 4;  // lane VKSHADOW2, the mixed case: slot 1's still casters, then its movers
	auto steps = [&]( bool store, bool mixed = false ) {
		std::vector<SStep> v;
		const WwVkPresent::LampRect full { 0, 0, F, F };
		int sides[4], layer[4], member[4];
		WwVkPresent::lampSides( kBack, sides, layer, member );
		WwVkPresent::LampRect room[4], wall[4];
		for ( int i = 0; i < 4; i++ )
			WwVkPresent::lampSideHalves( sides[i], kBack, F, room[i], wall[i] );
		const std::uint32_t n0 = std::uint32_t( slotCount[0] ), n3 = std::uint32_t( slotCount[3] );
		if ( store ) {
			for ( int fc = 0; fc < 6; fc++ )
				v.push_back( { 0, true, 0, fc, 3 * blk0[fc / 3] + fc % 3, 0, 0, {}, true, slotCmd[0], n0, 0, {} } );
			v.push_back( { 0, true, 3, kBack ^ 1, 3 * blk3, 0, 0, {}, true, slotCmd[3], n3, 0, {} } );
			for ( int i = 0; i < 4; i++ ) {
				const WwVkPresent::LampRect dst = WwVkPresent::lampPacked( room[i], member[i], F );
				v.push_back( { 0, true, 3, sides[i], 3 * blk3 + layer[i], dst.x - room[i].x, dst.y - room[i].y, dst, true, slotCmd[3], n3, 0, {} } );
			}
		}
		for ( int fc = 0; fc < 6; fc++ )
			v.push_back( { 1, false, 0, fc, fc, 0, 0, full, false, 0, 0, 3 * blk0[fc / 3] + fc % 3, full } );
		v.push_back( { 1, false, 3, kBack ^ 1, kBase3 + ( kBack ^ 1 ), 0, 0, full, false, 0, 0, 3 * blk3, full } );
		for ( int i = 0; i < 4; i++ )
			v.push_back( { 1, false, 3, sides[i], kBase3 + sides[i], 0, 0, room[i], false, 0, 0, 3 * blk3 + layer[i],
				WwVkPresent::lampPacked( room[i], member[i], F ) } );
		v.push_back( { 2, false, 3, kBack, kBase3 + kBack, 0, 0, {}, true, 0, 0, 0, {} } );
		for ( int i = 0; i < 4; i++ )
			v.push_back( { 2, false, 3, sides[i], kBase3 + sides[i], 0, 0, wall[i], true, 0, 0, 0, {} } );
		for ( int fc = 0; fc < 6; fc++ )  // the mover over the restored map (LOAD)
			v.push_back( { 0, false, 3, fc, kBase3 + fc, 0, 0, {}, false, moverCmd, mixed && fc == kBack ? 0u : 1u, 0, {} } );  // mixed: 4 off the cone
		if ( mixed ) {  // slot 1 drawn direct in run B: its clears, then its movers (LOAD); face 5 off the cone (empty lists)
			for ( int fc = 0; fc < 6; fc++ )
				v.push_back( { 0, false, 1, fc, 6 + fc, 0, 0, {}, true, slotCmd[1], fc == 5 ? 0u : kStill, 0, {} } );
			for ( int fc = 0; fc < 6; fc++ )
				v.push_back( { 0, false, 1, fc, 6 + fc, 0, 0, {}, false, slotCmd[1] + kStill, fc == 5 ? 0u : kMov, 0, {} } );
		}
		return v;
	};
	auto glState = [&]() {
		f.useProgram( prog );
		f.uniform1i( lAt, 0 );
		f.bindVertexArray( vao );
		glDisable( GL_BLEND );
		glDisable( GL_CULL_FACE );
		glDisable( 0x8037 );  // GL_POLYGON_OFFSET_FILL
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	};
	// one step as celllamps' GL code does it (drawFace: attach, viewport at the origin, scissor = cut, clear, draws; blit)
	// lane VKSHADOW2 at: 0 every caster untested; 1 the alpha-tested ones tested; 2 only those, no clear (aRed 1 clears, 2 drops
	// the origin); 3 the solid ones only
	auto glRun = [&]( const SStep & s, GLuint cube, GLuint store, const std::vector<float> & mv, int at = 0, int aRed = 0 ) {
		const WwVkPresent::LampRect c = s.cut.w > 0 ? s.cut : WwVkPresent::LampRect { 0, 0, F, F };
		if ( s.kind == 1 ) {
			f.bindFramebuffer( 0x8CA8, fboR );  // GL_READ_FRAMEBUFFER
			f.framebufferTextureLayer( 0x8CA8, kDepthAtt, store, 0, s.src );
			glReadBuffer( GL_NONE );
			f.bindFramebuffer( 0x8CA9, fbo );  // GL_DRAW_FRAMEBUFFER
			f.framebufferTextureLayer( 0x8CA9, kDepthAtt, cube, 0, s.layer );
			glDrawBuffer( GL_NONE );
			if ( f.checkFramebufferStatus( 0x8CA8 ) != kComplete || f.checkFramebufferStatus( 0x8CA9 ) != kComplete )
				return false;
			glDisable( GL_SCISSOR_TEST );
			f.blitFramebuffer( s.a.x, s.a.y, s.a.x + s.a.w, s.a.y + s.a.h, c.x, c.y, c.x + c.w, c.y + c.h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
			f.bindFramebuffer( kFb, fbo );
			return true;
		}
		if ( !attach( s.store ? store : cube, s.layer ) )
			return false;
		glViewport( at == 2 && aRed == 2 ? 0 : s.vx, at == 2 && aRed == 2 ? 0 : s.vy, F, F );
		glEnable( GL_SCISSOR_TEST );
		glScissor( c.x, c.y, c.w, c.h );
		float rel[16], clip[16];
		relOf( s.slot, rel );
		WwVkPresent::lampFaceClip( s.face, lights[s.slot].radius, clip );
		f.uniformMatrix4fv( lRel, 1, GL_FALSE, rel );
		f.uniform2f( lRn, lights[s.slot].radius, lights[s.slot].nearClip );
		f.uniformMatrix4fv( lClip, 1, GL_FALSE, clip );
		if ( at == 2 ? aRed == 1 : s.clear ) {
			glClearDepth( 1.0 );
			glClear( GL_DEPTH_BUFFER_BIT );
		}
		if ( s.kind == 2 ) {
			f.uniformMatrix4fv( lMv, 1, GL_FALSE, viewM );
			f.bindVertexArray( qVao );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
			f.bindVertexArray( vao );
		} else {
			if ( at ) {  // the alpha case's BaseMap and test uniforms (as glCasters)
				glBindTexture( GL_TEXTURE_2D, baseT );
				f.uniform1i( lBase, 0 );
				f.uniform2f( lUvS, 1.0f, 1.0f );
				f.uniform2f( lUvO, 0.0f, 0.0f );
				f.uniform1f( lAlpha, 1.0f );
				f.uniform1f( lThr, 0.5f );
			}
			for ( std::uint32_t k = s.first; k < s.first + s.n; k++ ) {
				const std::uint32_t * cm = &cmds[std::size_t( k ) * 5];
				const bool a = isAt[cm[4]] != 0;
				if ( ( at == 2 && !a ) || ( at == 3 && a ) )
					continue;
				if ( at )
					f.uniform1i( lAt, ( at == 1 || at == 2 ) && a ? 1 : 0 );
				f.uniformMatrix4fv( lMv, 1, GL_FALSE, &mv[std::size_t( cm[4] ) * 16] );
				f.drawElementsBaseVertex( GL_TRIANGLES, GLsizei( cm[0] ), GL_UNSIGNED_SHORT,
					reinterpret_cast<const void *>( std::size_t( cm[2] ) * 2 ), GLint( cm[3] ) );
			}
		}
		f.uniform1i( lAt, 0 );
		glDisable( GL_SCISSOR_TEST );
		return true;
	};
	auto junkS = [&]( GLuint tex ) {
		glDisable( GL_SCISSOR_TEST );
		glDepthMask( GL_TRUE );
		glClearDepth( 0.37 );
		bool r = true;
		for ( int l = 0; l < kStoreLayers; l++ ) {
			r = attach( tex, l ) && r;
			glViewport( 0, 0, F, F );
			glClear( GL_DEPTH_BUFFER_BIT );
		}
		glClearDepth( 1.0 );
		return r;
	};
	auto readStore = [&]( GLuint tex, std::vector<std::uint16_t> & out ) {
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		out.assign( std::size_t( F ) * F * kStoreLayers, 0 );
		for ( int l = 0; l < kStoreLayers; l++ ) {
			attach( tex, l );
			glReadPixels( 0, 0, F, F, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, out.data() + std::size_t( l ) * F * F );
		}
	};
	// the reference: GL does every step (movers false: the stored-only picture)
	auto refArm = [&]( bool store, bool movers, const std::vector<float> & mv, bool mixed = false, int at = 0 ) {
		bool r = junk( refT, -1 ) && ( !store || junkS( refStore ) );
		glState();
		for ( const SStep & s : steps( store, mixed ) )
			if ( movers || s.kind != 0 || s.store || s.clear )
				r = glRun( s, refT, refStore, mv, s.kind == 0 ? at : 0 ) && r;
		f.bindVertexArray( 0 );
		return r && glGetError() == GL_NO_ERROR;
	};
	enum SRed { kSNone, kSOrigin, kSWhole, kSMoverClear, kSDirectClear, kSOrder, kSConeDrawn, kSConeClear, kSAlphaClear, kSAlphaOrigin };
	auto lampGo = [&]( GLuint tex, std::uintptr_t k, int layers, const std::vector<WwVkPresent::LampOp> & ops, const std::vector<float> & mv,
					  const std::vector<std::uint32_t> * cl = nullptr ) {
		const std::vector<std::uint32_t> & cmds = cl ? *cl : lmvCmds;
		WwVkPresent::LampRun run;
		run.tex = tex;
		run.key = k;
		run.face = F;
		run.layers = layers;
		std::vector<WwVkPresent::LampMesh> lm( meshes.size() );
		for ( std::size_t m = 0; m < meshes.size(); m++ ) {
			lm[m].key = m + 1;
			lm[m].gen = meshGen[m];
			lm[m].pos = pos.data() + std::size_t( meshes[m].baseVertex ) * 3;
			lm[m].verts = meshVerts[m];
			lm[m].idx = idx.data() + meshes[m].firstIndex;
			lm[m].indices = meshes[m].count;
		}
		std::vector<std::uint32_t> dr;
		for ( std::size_t c = 0; c < cmds.size() / 5; c++ ) {
			std::uint32_t me = 0;
			while ( me < meshes.size() && meshes[me].firstIndex != cmds[c * 5 + 2] )
				me++;
			dr.push_back( me );
			dr.push_back( cmds[c * 5 + 4] );
		}
		run.meshes = lm.data();
		run.nMeshes = lm.size();
		run.mats = mv.data();
		run.nMats = std::size_t( kCasters );
		run.draws = dr.data();
		run.nDraws = dr.size() / 2;
		run.ops = ops;
		std::string w;
		if ( !WwVkPresent::lampRun( run, w ) ) {
			std::printf( "LAMPVIEW stored lampRun refused: %s\n", w.c_str() );
			return false;
		}
		return true;
	};
	// the app's route: run A (the store's still casters), GL's blits and wall quads, run B (the movers): no read between
	auto vkArm = [&]( bool store, SRed red, const std::vector<float> & mv, bool mixed = false, bool alpha = false ) {
		bool r = junk( shT, -1 ) && ( !store || junkS( shStore ) );
		const std::vector<SStep> v = steps( store, mixed );
		std::vector<WwVkPresent::LampOp> opsA, opsB;
		std::vector<std::uint32_t> solid;  // alpha: the device's commands (the alpha-tested casters left to GL)
		for ( const SStep & s : v ) {
			if ( s.kind != 0 )
				continue;
			WwVkPresent::LampOp op;
			op.layer = s.layer;
			op.clear = s.clear || ( red == kSMoverClear && !s.store ) || ( red == kSDirectClear && s.slot == 1 && !s.clear )
				|| ( red == kSConeClear && s.slot == 3 && !s.store && s.n == 0 );
			op.vx = red == kSOrigin ? 0 : s.vx;
			op.vy = red == kSOrigin ? 0 : s.vy;
			op.cut = s.cut;
			if ( red == kSWhole && s.store && s.cut.w > 0 && ( s.cut.x > 0 || s.cut.y > 0 ) )
				op.cut = WwVkPresent::LampRect {};  // the second member cleared (and drawn) over the whole layer
			relOf( s.slot, op.rel );
			WwVkPresent::lampFaceClip( s.face, lights[s.slot].radius, op.clip );
			op.rn[0] = lights[s.slot].radius;
			op.rn[1] = lights[s.slot].nearClip;
			op.first = s.first;
			op.n = s.n;
			if ( red == kSConeDrawn && s.slot == 1 && s.face == 5 )
				op.n = s.clear ? kStill : kMov;  // the cone face drawn
			if ( alpha ) {
				const std::uint32_t a0 = op.first, an = op.n;
				op.first = std::uint32_t( solid.size() / 5 );
				for ( std::uint32_t c = a0; c < a0 + an; c++ )
					if ( !isAt[cmds[std::size_t( c ) * 5 + 4]] )
						solid.insert( solid.end(), &cmds[std::size_t( c ) * 5], &cmds[std::size_t( c ) * 5] + 5 );
				op.n = std::uint32_t( solid.size() / 5 ) - op.first;
			}
			( s.store ? opsA : opsB ).push_back( op );
		}
		if ( red == kSOrder )  // run B's LOAD ops before its clears
			std::stable_partition( opsB.begin(), opsB.end(), []( const WwVkPresent::LampOp & o ) { return !o.clear; } );
		const std::vector<std::uint32_t> * cl = alpha ? &solid : nullptr;
		const int aRed = red == kSAlphaClear ? 1 : red == kSAlphaOrigin ? 2 : 0;
		if ( store )
			r = lampGo( shStore, storeKey, kStoreLayers, opsA, mv, cl ) && r;
		glState();
		for ( const SStep & s : v )  // alpha: GL's alpha-tested casters on the store steps, no clear, the step's origin and cut
			if ( alpha && s.kind == 0 && s.store )
				r = glRun( s, shT, shStore, mv, 2, aRed ) && r;
		for ( const SStep & s : v )
			if ( s.kind != 0 )
				r = glRun( s, shT, shStore, mv ) && r;
		f.bindVertexArray( 0 );
		r = lampGo( shT, key, kLayers, opsB, mv, cl ) && r;
		if ( alpha ) {  // and on the cube steps after run B
			glState();
			for ( const SStep & s : v )
				if ( s.kind == 0 && !s.store )
					r = glRun( s, shT, shStore, mv, 2 ) && r;
			f.bindVertexArray( 0 );
		}
		return r && glGetError() == GL_NO_ERROR;
	};
	// verdicts: slots 0 and 3's cube faces and every store layer (S2: each side's store EXACT against its own S1 bytes)
	auto judgeS = [&]( int fr, const std::vector<std::uint16_t> & vc, const std::vector<std::uint16_t> & rc, const std::vector<std::uint16_t> & vs,
						 const std::vector<std::uint16_t> & rs, const std::vector<std::uint16_t> * pvs, const std::vector<std::uint16_t> * prs,
						 bool print, Diff * sum, const char * tag = "stored", bool mixed = false ) {
		bool good = true;
		const std::size_t n = std::size_t( F ) * F;
		for ( int k : { 0, 1, 3 } )
			for ( int fc = 0; fc < 6; fc++ ) {
				if ( k == 1 && !mixed )
					continue;
				const std::size_t l = std::size_t( 6 * k + fc );
				const Diff v = compare( vc.data() + l * n, rc.data() + l * n, n );
				if ( sum )
					sum->add( v );
				const bool cl = mixed && k == 1 && fc == 5;  // off the cone: CLEAR
				const bool lok = cl ? v.clearOk() : v.ok();
				good = good && lok;
				if ( print )
					std::printf( "LAMPVIEW %s S%d slot%d/face%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; set vk %zu gl %zu)\n", tag, fr,
						k, fc, cl ? ( lok ? "CLEAR" : "FAIL" ) : !lok ? "FAIL" : v.diff ? "BOUND" : "EXACT", v.diff, v.covered, v.past1, v.maxu, v.vkSet, v.refSet );
			}
		for ( int l = 0; l < kStoreLayers; l++ ) {
			Diff v = compare( vs.data() + std::size_t( l ) * n, rs.data() + std::size_t( l ) * n, n );
			if ( sum )
				sum->add( v );
			bool lok = v.ok();
			if ( pvs && prs ) {
				const Diff va = compare( vs.data() + std::size_t( l ) * n, pvs->data() + std::size_t( l ) * n, n );
				const Diff vb = compare( rs.data() + std::size_t( l ) * n, prs->data() + std::size_t( l ) * n, n );
				lok = va.diff == 0 && va.covered > 0 && vb.diff == 0 && vb.covered > 0;
				v = va;
			}
			good = good && lok;
			if ( print )
				std::printf( "LAMPVIEW %s S%d store%d %s%s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; set vk %zu gl %zu)\n", tag, fr, l,
					!lok ? "FAIL" : v.diff ? "BOUND" : "EXACT", pvs ? " held" : "", v.diff, v.covered, v.past1, v.maxu, v.vkSet, v.refSet );
		}
		// each packed half covers texels on both sides
		int sides[4], layer[4], member[4];
		WwVkPresent::lampSides( kBack, sides, layer, member );
		for ( int i = 0; i < 4; i++ ) {
			WwVkPresent::LampRect room, wall;
			WwVkPresent::lampSideHalves( sides[i], kBack, F, room, wall );
			const WwVkPresent::LampRect p = WwVkPresent::lampPacked( room, member[i], F );
			const std::size_t base = std::size_t( 3 * blk3 + layer[i] ) * n;
			std::size_t cv = 0, cr = 0;
			for ( int y = p.y; y < p.y + p.h; y++ )
				for ( int x = p.x; x < p.x + p.w; x++ ) {
					cv += vs[base + std::size_t( y ) * F + x] != 0xFFFF;
					cr += rs[base + std::size_t( y ) * F + x] != 0xFFFF;
				}
			good = good && cv > 0 && cr > 0;
			if ( print )
				std::printf( "LAMPVIEW %s S%d half face%d (store%d member %d, %d,%d %dx%d) covers vk %zu gl %zu %s\n", tag, fr, sides[i], 3 * blk3 + layer[i],
					member[i], p.x, p.y, p.w, p.h, cv, cr, cv && cr ? "ok" : "FAIL: proves nothing" );
		}
		return good;
	};
	if ( sOk ) {
		std::vector<float> matsS2 = mats[0];  // S2: only the mover moves
		std::memcpy( &matsS2[std::size_t( kMover ) * 16], &mats[1][std::size_t( kMover ) * 16], 64 );
		const std::vector<float> * smv[2] = { &mats[0], &matsS2 };
		std::vector<std::uint16_t> rc[2], rs[2], vc[2], vs[2], noMov;
		for ( int fr = 0; fr < 2; fr++ ) {
			const bool store = fr == 0;
			bool r = refArm( store, false, *smv[fr] );
			readAll( refT, noMov );
			r = r && refArm( store, true, *smv[fr] );
			readAll( refT, rc[fr] );
			readStore( refStore, rs[fr] );
			r = r && vkArm( store, kSNone, *smv[fr] );
			readAll( shT, vc[fr] );
			readStore( shStore, vs[fr] );
			if ( !r ) {
				std::printf( "LAMPVIEW stored S%d FAIL (a run failed)\n", fr + 1 );
				ok = false;
				continue;
			}
			ok = judgeS( fr + 1, vc[fr], rc[fr], vs[fr], rs[fr], fr ? &vs[0] : nullptr, fr ? &rs[0] : nullptr, true, nullptr ) && ok;
			std::size_t won = 0;
			for ( std::size_t i = std::size_t( kBase3 ) * F * F; i < std::size_t( kBase3 + 6 ) * F * F; i++ )
				won += noMov[i] != rc[fr][i];
			std::printf( "LAMPVIEW stored S%d mover wins %zu texels over the stored-only picture (%s)\n", fr + 1, won, won ? "ok" : "FAIL: proves nothing" );
			ok = ok && won > 0;
		}
		const struct
		{
			const char * name;
			SRed red;
		} sReds[3] = { { "store origin (the shifted viewport origin dropped)", kSOrigin },
			{ "whole-layer clear (the second member cleared over the whole layer)", kSWhole }, { "stored mover cleared (run B's LOAD ops clear)", kSMoverClear } };
		for ( const auto & rd : sReds ) {
			Diff all;
			std::vector<std::uint16_t> c, s;
			const bool ran = vkArm( true, rd.red, mats[0] );
			readAll( shT, c );
			readStore( shStore, s );
			const bool passes = ran && !rc[0].empty() && judgeS( 1, c, rc[0], s, rs[0], nullptr, nullptr, false, &all );
			std::printf( "LAMPVIEW RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n", passes ? "PASSES" : "FAIL", rd.name,
				all.diff, all.covered, all.past1, all.maxu );
			ok = ok && !passes;
		}
		// lane VKSHADOW2: the mixed run B (slot 1 drawn direct beside slot 3's mover; cone faces), S1's store steps
		{
			std::vector<std::uint16_t> nm, rc, rs, vc, vs;
			bool r = refArm( true, false, mats[0], true );
			readAll( refT, nm );
			r = r && refArm( true, true, mats[0], true );
			readAll( refT, rc );
			readStore( refStore, rs );
			r = r && vkArm( true, kSNone, mats[0], true );
			readAll( shT, vc );
			readStore( shStore, vs );
			if ( !r ) {
				std::printf( "LAMPVIEW mixed S1 FAIL (a run failed)\n" );
				ok = false;
			} else {
				ok = judgeS( 1, vc, rc, vs, rs, nullptr, nullptr, true, nullptr, "mixed", true ) && ok;
			}
			std::size_t won = 0;
			for ( std::size_t i = std::size_t( 6 ) * F * F; i < std::size_t( 12 ) * F * F; i++ )
				won += nm[i] != rc[i];
			std::printf( "LAMPVIEW mixed S1 direct movers win %zu texels over the still-only picture (%s)\n", won, won ? "ok" : "FAIL: proves nothing" );
			ok = ok && won > 0;
			const struct
			{
				const char * name;
				SRed red;
			} mReds[4] = { { "direct mover cleared (slot 1's LOAD ops clear)", kSDirectClear }, { "run B order (its LOAD ops before its clears)", kSOrder },
				{ "cone face drawn (slot 1 face 5 gets its casters and movers)", kSConeDrawn },
				{ "cone LOAD cleared (slot 3 face 4's empty LOAD op clears)", kSConeClear } };
			for ( const auto & rd : mReds ) {
				Diff all;
				std::vector<std::uint16_t> c, s;
				const bool ran = vkArm( true, rd.red, mats[0], true );
				readAll( shT, c );
				readStore( shStore, s );
				const bool passes = ran && r && judgeS( 1, c, rc, s, rs, nullptr, nullptr, false, &all, "mixed", true );
				std::printf( "LAMPVIEW RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n", passes ? "PASSES" : "FAIL", rd.name,
					all.diff, all.covered, all.past1, all.maxu );
				ok = ok && !passes;
			}
		}
		// lane VKSHADOW2: GL's alpha-tested casters on the store's packed halves after run A (no clear), S1's steps
		{
			std::vector<std::uint16_t> rc, rs, ss, vc, vs;
			bool r = refArm( true, true, mats[0], false, 3 );
			readStore( refStore, ss );
			r = r && refArm( true, true, mats[0], false, 1 );
			readAll( refT, rc );
			readStore( refStore, rs );
			r = r && vkArm( true, kSNone, mats[0], false, true );
			readAll( shT, vc );
			readStore( shStore, vs );
			if ( !r ) {
				std::printf( "LAMPVIEW alpha S1 FAIL (a run failed)\n" );
				ok = false;
			} else {
				ok = judgeS( 1, vc, rc, vs, rs, nullptr, nullptr, true, nullptr, "alpha" ) && ok;
			}
			int sides[4], layer[4], member[4];
			WwVkPresent::lampSides( kBack, sides, layer, member );
			std::size_t won = 0;
			for ( int i = 0; i < 4; i++ ) {
				WwVkPresent::LampRect room, wall;
				WwVkPresent::lampSideHalves( sides[i], kBack, F, room, wall );
				const WwVkPresent::LampRect p = WwVkPresent::lampPacked( room, member[i], F );
				const std::size_t base = std::size_t( 3 * blk3 + layer[i] ) * F * F;
				for ( int y = p.y; y < p.y + p.h; y++ )
					for ( int x = p.x; x < p.x + p.w; x++ )
						won += rs[base + std::size_t( y ) * F + x] != ss[base + std::size_t( y ) * F + x];
			}
			std::printf( "LAMPVIEW alpha S1 casters win %zu texels in the packed halves over the solid-only store (%s)\n", won,
				won ? "ok" : "FAIL: proves nothing" );
			ok = ok && won > 0;
			const struct
			{
				const char * name;
				SRed red;
			} aReds[2] = { { "stored alpha clear (GL clears the cut before its alpha draw)", kSAlphaClear },
				{ "stored alpha origin (GL's alpha draw at the unshifted viewport origin)", kSAlphaOrigin } };
			for ( const auto & rd : aReds ) {
				Diff all;
				std::vector<std::uint16_t> c, s;
				const bool ran = vkArm( true, rd.red, mats[0], false, true );
				readAll( shT, c );
				readStore( shStore, s );
				const bool passes = ran && r && judgeS( 1, c, rc, s, rs, nullptr, nullptr, false, &all, "alpha" );
				std::printf( "LAMPVIEW RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n", passes ? "PASSES" : "FAIL", rd.name,
					all.diff, all.covered, all.past1, all.maxu );
				ok = ok && !passes;
			}
		}
		poolTake( "stored", -1, -1, -1, -1 );
	}
	WwVkPresent::sharedTargetDrop( storeKey );
	const WwVkPresent::LampStat & st = tot;
	std::printf( "LAMPVIEW stat: %d runs on Vulkan, %d failed (%s), %d ops, %d draws, %.3f ms a run (max %.3f), %d pipelines built in %.1f ms; "
				 "pool %d copies (%.1f KiB), %d hits, %d regen, %d repacks, %d red skips\n",
		st.runs, st.fails, st.failWhy.c_str(), st.ops, st.draws, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs, st.copies,
		st.copyKb, st.hits, st.regen, st.repacks, st.redSkips );
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
