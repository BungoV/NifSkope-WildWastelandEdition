// Lane VKSHADOW1 (2026-11-13, plan row G6): the sun cascades' queued casters on the present device (vkpresent.cpp csmRun,
// built in here with WW_VKPRESENT_NO_QT: the shipped code) against GL's own cascade program (lookdev_csmdepth_q.vert +
// lookdev_csmdepth.frag) drawn the way sunshadow.cpp draws a cascade, into D16 2D arrays, every texel compared.
// Usage: shadowview <tree root> [map]
// Both arms: two targets (live, static) of 3 layers, cleared to 1; GL draws a "ground" first (the casters GL keeps), then
// the queued draws: GL with glMultiDrawElementsIndirect per draw (viewport, scissor, glPolygonOffset(Clamp), cull), Vulkan
// with one csmRun from the same pool, matrices and commands into the shared targets. Jobs: open random triangles of both
// windings, slanted plates, three cascades (one at the full map, two at odd sizes), scissor strips, a two-sided span, the
// static target, a second frame with moved casters (the pending fence and the buffers reused).
// Prints `SHADOWVIEW <frame> <target>/<layer> EXACT|BOUND|FAIL`, `SHADOWVIEW RED <name> ...` (each must FAIL) and
// `SHADOWVIEW GATE OK|FAILED`.
// BOUND (stated before the first run, never widened): EXACT expected (same vertex products in the same order, the same
// D16 bias law on the same rasterizer); else max 1 D16 unit and <= 0.05 % of covered texels differ.
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

namespace shv
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
constexpr GLenum kCompiled = 0x8B81, kLinked = 0x8B82, kArrayBuf = 0x8892, kElemBuf = 0x8893, kIndirect = 0x8F3F;
constexpr GLenum kDynamic = 0x88E8, kTex2DArray = 0x8C1A, kD16 = 0x81A5, kPolyFill = 0x8037;

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
	void( WINAPI * genBuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindBuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * bufferData )( GLenum, std::ptrdiff_t, const void *, GLenum ) = nullptr;
	void( WINAPI * bufferSubData )( GLenum, std::ptrdiff_t, std::ptrdiff_t, const void * ) = nullptr;
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * vertexAttribDivisor )( GLuint, GLuint ) = nullptr;
	void( WINAPI * multiDrawElementsIndirect )( GLenum, GLenum, const void *, GLsizei, GLsizei ) = nullptr;
	void( WINAPI * genFramebuffers )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindFramebuffer )( GLenum, GLuint ) = nullptr;
	void( WINAPI * framebufferTextureLayer )( GLenum, GLenum, GLuint, GLint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * texImage3D )( GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void * ) = nullptr;
	void( WINAPI * polygonOffsetClamp )( GLfloat, GLfloat, GLfloat ) = nullptr;
};
Fns f;

template <class P> bool load( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	return p != nullptr;
}

bool loadAll()
{
	const bool ok = load( f.createShader, "glCreateShader" ) && load( f.shaderSource, "glShaderSource" )
		&& load( f.compileShader, "glCompileShader" ) && load( f.getShaderiv, "glGetShaderiv" )
		&& load( f.getShaderInfoLog, "glGetShaderInfoLog" ) && load( f.createProgram, "glCreateProgram" )
		&& load( f.attachShader, "glAttachShader" ) && load( f.linkProgram, "glLinkProgram" )
		&& load( f.getProgramiv, "glGetProgramiv" ) && load( f.getProgramInfoLog, "glGetProgramInfoLog" )
		&& load( f.useProgram, "glUseProgram" ) && load( f.getUniformLocation, "glGetUniformLocation" )
		&& load( f.uniformMatrix4fv, "glUniformMatrix4fv" ) && load( f.genBuffers, "glGenBuffers" )
		&& load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.bufferSubData, "glBufferSubData" ) && load( f.genVertexArrays, "glGenVertexArrays" )
		&& load( f.bindVertexArray, "glBindVertexArray" ) && load( f.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( f.vertexAttribPointer, "glVertexAttribPointer" ) && load( f.vertexAttribDivisor, "glVertexAttribDivisor" )
		&& load( f.multiDrawElementsIndirect, "glMultiDrawElementsIndirect" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTextureLayer, "glFramebufferTextureLayer" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" ) && load( f.texImage3D, "glTexImage3D" );
	if ( !load( f.polygonOffsetClamp, "glPolygonOffsetClamp" ) )
		load( f.polygonOffsetClamp, "glPolygonOffsetClampEXT" );
	return ok;
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
		std::printf( "SHADOWVIEW compile failed (%s): %.1500s\n", tag, log.data() );
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
		std::printf( "SHADOWVIEW link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}

// a 4x4 GL column-major product a * b
void mul4( const double * a, const double * b, double * o )
{
	for ( int c = 0; c < 4; c++ )
		for ( int r = 0; r < 4; r++ ) {
			double s = 0.0;
			for ( int k = 0; k < 4; k++ )
				s += a[k * 4 + r] * b[c * 4 + k];
			o[c * 4 + r] = s;
		}
}

// rotation about a unit axis (column-major 4x4) with a translation
void rotT( double ax, double ay, double az, double ang, double tx, double ty, double tz, double s, double * o )
{
	const double l = std::sqrt( ax * ax + ay * ay + az * az );
	ax /= l;
	ay /= l;
	az /= l;
	const double c = std::cos( ang ), si = std::sin( ang ), t = 1.0 - c;
	const double R[3][3] = { { t * ax * ax + c, t * ax * ay - si * az, t * ax * az + si * ay },
		{ t * ax * ay + si * az, t * ay * ay + c, t * ay * az - si * ax }, { t * ax * az - si * ay, t * ay * az + si * ax, t * az * az + c } };
	for ( int i = 0; i < 16; i++ )
		o[i] = 0.0;
	for ( int r = 0; r < 3; r++ )
		for ( int cc = 0; cc < 3; cc++ )
			o[cc * 4 + r] = R[r][cc] * s;
	o[12] = tx;
	o[13] = ty;
	o[14] = tz;
	o[15] = 1.0;
}

struct DrawCmd
{
	std::uint32_t count, instanceCount, firstIndex;
	std::int32_t baseVertex;
	std::uint32_t baseInstance;
};

struct Diff
{
	std::size_t covered = 0, diff = 0, past1 = 0;
	int maxu = 0;
	bool ok() const { return covered > 0 && maxu <= 1 && diff * 2000 <= covered; }
	const char * word() const { return covered == 0 ? "FAIL" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
	void add( const Diff & o )
	{
		covered += o.covered;
		diff += o.diff;
		past1 += o.past1;
		maxu = std::max( maxu, o.maxu );
	}
};

Diff compare( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
{
	Diff v;
	if ( a.size() != b.size() )
		return v;
	for ( std::size_t i = 0; i < a.size(); i++ ) {
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
}  // namespace shv

int main( int argc, char ** argv )
{
	using namespace shv;
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 2 ) {
		std::printf( "usage: shadowview <tree root> [map]\nSHADOWVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[1];
	const int map = argc > 2 ? std::atoi( argv[2] ) : 1024;
	const int W = 256, H = 256, S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwshadowview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwshadowview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwshadowview", "shadow view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "SHADOWVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
		std::printf( "SHADOWVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "map %d x 3 layers x 2 targets, GL on %s, glPolygonOffsetClamp %s\n", map,
		reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ), f.polygonOffsetClamp ? "yes" : "no" );
	const GLuint prog = program( readFile( root + "/res/shaders/features/near/lookdev_csmdepth_q.vert" ),
		readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth.frag" ) );
	if ( !prog ) {
		std::printf( "SHADOWVIEW GATE FAILED (GL program)\n" );
		return 2;
	}
	const GLint lClip = f.getUniformLocation( prog, "csmClipFromView" );

	// ---- the pool: open random triangles (each its own winding), slanted plates both ways, a ground plate
	std::mt19937 rng( 7 );
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
	for ( int m = 0; m < 3; m++ ) {  // soups
		beginMesh();
		for ( int t = 0; t < 70; t++ ) {
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
	for ( int m = 0; m < 2; m++ ) {  // a 12 x 12 plate with a wave, CCW (m 0) or CW (m 1) seen from +z
		beginMesh();
		const int n = 12;
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
				for ( int k = 0; k < 6; k++ )
					idx.push_back( m ? q[5 - k] : q[k] );
			}
		endMesh();
	}
	const int kGround = int( meshes.size() );  // the plate GL draws first (the casters GL keeps)
	beginMesh();
	{
		const float g[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		pos.insert( pos.end(), g, g + 12 );
		const std::uint16_t gi[6] = { 0, 1, 2, 2, 1, 3 };
		idx.insert( idx.end(), gi, gi + 6 );
	}
	endMesh();

	// ---- casters (model-view per caster), commands: 40 queued + 2 ground
	const int kQueued = 40, kCasters = kQueued + 2;
	std::vector<DrawCmd> cmds( kCasters );
	struct Caster
	{
		double ax, ay, az, ang, tx, ty, tz, s;
	};
	std::vector<Caster> cast( kCasters );
	for ( int i = 0; i < kQueued; i++ ) {
		const int m = int( rng() % std::uint32_t( kGround ) );
		cmds[i] = { meshes[m].count, 1, meshes[m].firstIndex, meshes[m].baseVertex, std::uint32_t( i ) };
		cast[i] = { u( rng ), u( rng ), u( rng ) + 1.5, 3.14159 * u( rng ), 90.0 * u( rng ), 90.0 * u( rng ), -150.0 + 80.0 * u( rng ),
			8.0 + 22.0 * ( 0.5 + 0.5 * u( rng ) ) };
	}
	for ( int i = kQueued; i < kCasters; i++ ) {
		cmds[i] = { meshes[kGround].count, 1, meshes[kGround].firstIndex, meshes[kGround].baseVertex, std::uint32_t( i ) };
		cast[i] = { 1.0, 0.2, 0.1, 1.1 + 0.4 * ( i - kQueued ), 10.0 * ( i - kQueued ), -20.0, -170.0, 70.0 };
	}
	std::vector<float> mats( size_t( kCasters ) * 16 );
	auto setMats = [&]( double move ) {
		for ( int i = 0; i < kCasters; i++ ) {
			const Caster & c = cast[i];
			double M[16];
			rotT( c.ax, c.ay, c.az, c.ang + 0.3 * move, c.tx + 7.0 * move, c.ty - 5.0 * move, c.tz + 3.0 * move, c.s, M );
			for ( int k = 0; k < 16; k++ )
				mats[size_t( i ) * 16 + k] = float( M[k] );
		}
	};

	// ---- the cascades: clipFromView = ortho * light-from-view, three extents, viewports at the full map and two odd sizes
	struct Casc
	{
		float clip[16];
		int vw, vh;
		float clamp;
	};
	Casc casc[3];
	const double ext[3] = { 45.0, 95.0, 190.0 }, cx[3] = { -10.0, 5.0, 0.0 }, cy[3] = { 8.0, -6.0, 0.0 };
	const int vws[3] = { map, map - 24, map - 7 }, vhs[3] = { map, map - 11, map - 25 };
	const float slopeBias = 6.0f, unitBias = 12.0f, kNearL = 0.0f, farL = 420.0f;
	{
		double L[16];
		rotT( 0.3, 1.0, 0.2, 0.9, 0.0, 0.0, 0.0, 1.0, L );  // view -> light rotation
		double Tz[16];
		rotT( 1, 0, 0, 0.0, 0.0, 0.0, -60.0, 1.0, Tz );  // the light's eye beyond the casters
		double LT[16];
		mul4( Tz, L, LT );
		for ( int i = 0; i < 3; i++ ) {
			double O[16] = {};
			O[0] = 1.0 / ext[i];
			O[5] = 1.0 / ext[i];
			O[10] = -2.0 / ( farL - kNearL );
			O[12] = -cx[i] / ext[i];
			O[13] = -cy[i] / ext[i];
			O[14] = -( farL + kNearL ) / ( farL - kNearL );
			O[15] = 1.0;
			double C[16];
			mul4( O, LT, C );
			for ( int k = 0; k < 16; k++ )
				casc[i].clip[k] = float( C[k] );
			casc[i].vw = vws[i];
			casc[i].vh = vhs[i];
			const double texel = 2.0 * ext[i] / double( vws[i] );
			casc[i].clamp = f.polygonOffsetClamp ? float( unitBias / 65536.0 + slopeBias * texel * 4.0 / std::max( double( farL - kNearL ), 1e-6 ) )
												 : 0.0f;
		}
	}

	// ---- the draw list (sunshadow.cpp's vkRec x queue): target, layer, cascade, scissor, cull, commands
	struct Rec
	{
		int target, layer, cascade;
		int sc[4];
		bool cull;
		std::uint32_t first, n;
	};
	std::vector<Rec> recs = {
		{ 0, 0, 0, { 0, 0, 0, 0 }, true, 0, 20 },
		{ 0, 0, 0, { 0, 0, 0, 0 }, false, 20, 10 },          // the two-sided span
		{ 0, 1, 1, { 0, 700, map - 24, map - 11 }, true, 0, 30 },  // a scissor strip (the cache's scroll strip)
		{ 1, 2, 2, { 0, 0, 0, 0 }, true, 0, 25 },            // the static target
		{ 0, 2, 2, { 0, 0, 0, 0 }, true, 0, 25 },            // ... duplicated into the live one (GL's static->live copy)
		{ 0, 2, 2, { 800, 0, map - 7, map - 25 }, false, 25, 5 },
		{ 0, 1, 1, { 0, 0, 0, 0 }, true, 30, 10 },
	};

	// ---- GL buffers (the pool, matrices, commands) and the VAO as cellqueue.cpp binds it
	GLuint bPos = 0, bIdx = 0, bMat = 0, bCmd = 0, vao = 0;
	f.genBuffers( 1, &bPos );
	f.genBuffers( 1, &bIdx );
	f.genBuffers( 1, &bMat );
	f.genBuffers( 1, &bCmd );
	f.genVertexArrays( 1, &vao );
	f.bindVertexArray( vao );
	f.bindBuffer( kArrayBuf, bPos );
	f.bufferData( kArrayBuf, std::ptrdiff_t( pos.size() * 4 ), pos.data(), kDynamic );
	f.enableVertexAttribArray( 0 );
	f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	f.bindBuffer( kArrayBuf, bMat );
	f.bufferData( kArrayBuf, std::ptrdiff_t( mats.size() * 4 ), nullptr, kDynamic );
	for ( int c = 0; c < 4; c++ ) {
		f.enableVertexAttribArray( GLuint( 8 + c ) );
		f.vertexAttribPointer( GLuint( 8 + c ), 4, GL_FLOAT, GL_FALSE, 64, reinterpret_cast<const void *>( std::size_t( c ) * 16 ) );
		f.vertexAttribDivisor( GLuint( 8 + c ), 1 );
	}
	f.bindBuffer( kElemBuf, bIdx );
	f.bufferData( kElemBuf, std::ptrdiff_t( idx.size() * 2 ), idx.data(), kDynamic );
	f.bindBuffer( kIndirect, bCmd );
	f.bufferData( kIndirect, std::ptrdiff_t( cmds.size() * sizeof( DrawCmd ) ), cmds.data(), kDynamic );
	std::printf( "pool %zu vertices, %zu indices, %d meshes, %d commands, %zu draws\n", pos.size() / 3, idx.size(), int( meshes.size() ),
		kCasters, recs.size() );

	// ---- targets: GL's own (the reference) and the shared ones (keys: their homes, as sunshadow.cpp keys &s.tex)
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "SHADOWVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	const std::string no = WwVkPresent::csmRefusal();
	if ( !no.empty() ) {
		std::printf( "SHADOWVIEW GATE FAILED (csmRefusal: %s)\n", no.c_str() );
		return 2;
	}
	GLuint refT[2] = {}, shT[2] = {};
	static unsigned keyHome[2];
	for ( int t = 0; t < 2; t++ ) {
		glGenTextures( 1, &refT[t] );
		glBindTexture( kTex2DArray, refT[t] );
		f.texImage3D( kTex2DArray, 0, GLint( kD16 ), map, map, 3, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
		glTexParameteri( kTex2DArray, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( kTex2DArray, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keyHome[t] ), 2, map, map, 3, kD16, shT[t], why ) ) {
			std::printf( "SHADOWVIEW GATE FAILED (sharedTarget %d: %s)\n", t, why.c_str() );
			return 2;
		}
	}
	glBindTexture( kTex2DArray, 0 );
	GLuint fbo = 0;
	f.genFramebuffers( 1, &fbo );
	auto attach = [&]( GLuint tex, int layer ) {
		f.bindFramebuffer( kFb, fbo );
		f.framebufferTextureLayer( kFb, kDepthAtt, tex, 0, layer );
		glDrawBuffer( GL_NONE );
		glReadBuffer( GL_NONE );
		return f.checkFramebufferStatus( kFb ) == kComplete;
	};
	auto clearAll = [&]( const GLuint * tex ) {
		glDisable( GL_SCISSOR_TEST );
		glDepthMask( GL_TRUE );
		glClearDepth( 1.0 );
		for ( int t = 0; t < 2; t++ )
			for ( int l = 0; l < 3; l++ ) {
				if ( !attach( tex[t], l ) )
					return false;
				glViewport( 0, 0, map, map );
				glClear( GL_DEPTH_BUFFER_BIT );
			}
		return true;
	};
	// sunshadow.cpp's pass state, then one GL draw (a Rec) with its viewport, scissor, bias and cull
	auto glState = [&]() {
		f.useProgram( prog );
		f.bindVertexArray( vao );
		f.bindBuffer( kIndirect, bCmd );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		glDisable( GL_BLEND );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glFrontFace( GL_CCW );
		glCullFace( GL_BACK );
		glEnable( kPolyFill );
		glPolygonOffset( slopeBias, unitBias );
	};
	auto glDraw = [&]( const GLuint * tex, const Rec & R ) {
		if ( !attach( tex[R.target], R.layer ) )
			return false;
		const Casc & c = casc[R.cascade];
		glViewport( 0, 0, c.vw, c.vh );
		const bool sc = R.sc[2] > R.sc[0];
		if ( sc ) {
			glEnable( GL_SCISSOR_TEST );
			glScissor( R.sc[0], R.sc[1], R.sc[2] - R.sc[0], R.sc[3] - R.sc[1] );
		}
		if ( f.polygonOffsetClamp )
			f.polygonOffsetClamp( slopeBias, unitBias, c.clamp );
		f.uniformMatrix4fv( lClip, 1, GL_FALSE, c.clip );
		R.cull ? glEnable( GL_CULL_FACE ) : glDisable( GL_CULL_FACE );
		f.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::size_t( R.first ) * sizeof( DrawCmd ) ),
			GLsizei( R.n ), GLsizei( sizeof( DrawCmd ) ) );
		if ( sc )
			glDisable( GL_SCISSOR_TEST );
		return true;
	};
	// the ground GL keeps: into every layer of both targets, before the queued draws (two-sided: the light may see its back)
	auto ground = [&]( const GLuint * tex ) {
		bool ok = true;
		for ( int t = 0; t < 2; t++ )
			for ( int l = 0; l < 3; l++ )
				ok = ok && glDraw( tex, Rec { t, l, l, { 0, 0, 0, 0 }, false, std::uint32_t( kQueued ), 2 } );
		return ok;
	};
	auto readAll = [&]( const GLuint * tex, std::vector<std::uint16_t> out[2][3] ) {
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		for ( int t = 0; t < 2; t++ )
			for ( int l = 0; l < 3; l++ ) {
				out[t][l].assign( size_t( map ) * map, 0 );
				attach( tex[t], l );
				glReadPixels( 0, 0, map, map, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, out[t][l].data() );
			}
	};
	auto upload = [&]( double move ) {
		setMats( move );
		f.bindBuffer( kArrayBuf, bMat );
		f.bufferSubData( kArrayBuf, 0, std::ptrdiff_t( mats.size() * 4 ), mats.data() );
	};
	auto runGl = [&]( std::vector<std::uint16_t> out[2][3] ) {
		bool ok = clearAll( refT );
		glState();
		ok = ok && ground( refT );
		for ( const Rec & R : recs )
			ok = ok && glDraw( refT, R );
		readAll( refT, out );
		return ok && glGetError() == GL_NO_ERROR;
	};
	enum Red { kNone, kSplit, kNoBias, kNoCull };
	auto runVk = [&]( Red red, std::vector<std::uint16_t> out[2][3] ) {
		bool ok = clearAll( shT );
		glState();
		ok = ok && ground( shT );
		WwVkPresent::CsmRun run;
		run.map = map;
		run.layers = 3;
		for ( int t = 0; t < 2; t++ ) {
			run.tex[t] = shT[t];
			run.key[t] = reinterpret_cast<std::uintptr_t>( &keyHome[t] );
		}
		run.pos = bPos;
		run.idx = bIdx;
		run.posBytes = pos.size() * 4;
		run.idxBytes = idx.size() * 2;
		run.mats = mats.data();
		run.nMats = std::size_t( kCasters );
		run.cmds = reinterpret_cast<const std::uint32_t *>( cmds.data() );
		run.nCmds = cmds.size();
		for ( const Rec & R : recs ) {
			WwVkPresent::CsmDraw d;
			const int ci = red == kSplit ? ( R.cascade + 1 ) % 3 : R.cascade;  // red: the next cascade's matrix
			const Casc & c = casc[R.cascade];
			d.target = R.target;
			d.layer = R.layer;
			d.vw = c.vw;
			d.vh = c.vh;
			std::memcpy( d.sc, R.sc, sizeof( d.sc ) );
			std::memcpy( d.clip, casc[ci].clip, sizeof( d.clip ) );
			d.unit = red == kNoBias ? 0.0f : unitBias;
			d.slope = red == kNoBias ? 0.0f : slopeBias;
			d.clamp = red == kNoBias ? 0.0f : c.clamp;
			d.cull = red == kNoCull ? false : R.cull;
			d.first = R.first;
			d.n = R.n;
			run.d.push_back( d );
		}
		std::string w;
		if ( !WwVkPresent::csmRun( run, w ) ) {
			std::printf( "SHADOWVIEW csmRun refused: %s\n", w.c_str() );
			return false;
		}
		readAll( shT, out );
		return ok && glGetError() == GL_NO_ERROR;
	};

	bool ok = true;
	std::vector<std::uint16_t> ref[2][2][3], vkO[2][3];
	const double moves[2] = { 0.0, 1.0 };
	for ( int fr = 0; fr < 2; fr++ ) {
		upload( moves[fr] );
		if ( !runGl( ref[fr] ) || !runVk( kNone, vkO ) ) {
			std::printf( "SHADOWVIEW frame%d FAIL (a run failed)\n", fr );
			ok = false;
			continue;
		}
		for ( int t = 0; t < 2; t++ )
			for ( int l = 0; l < 3; l++ ) {
				const Diff v = compare( vkO[t][l], ref[fr][t][l] );
				std::printf( "SHADOWVIEW frame%d %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", fr,
					t ? "static" : "live", l, v.word(), v.diff, v.covered, v.past1, v.maxu );
				ok = ok && v.ok();  // the static target's layers 0 and 1 hold only the ground (GL's own draws)
			}
	}
	// the reds against frame 1's references: each must FAIL (over all six layers)
	const struct
	{
		const char * name;
		Red red;
	} reds[3] = { { "split (each draw with the next cascade's clipFromView)", kSplit }, { "nobias (unit and slope 0)", kNoBias },
		{ "cull (back faces drawn on the one-sided spans)", kNoCull } };
	for ( const auto & r : reds ) {
		const bool ran = runVk( r.red, vkO );
		Diff all;
		for ( int t = 0; t < 2; t++ )
			for ( int l = 0; l < 3; l++ )
				all.add( compare( vkO[t][l], ref[1][t][l] ) );
		const bool redFails = !ran || !all.ok();
		std::printf( "SHADOWVIEW RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n",
			redFails ? "FAIL" : all.word(), r.name, all.diff, all.covered, all.past1, all.maxu );
		ok = ok && redFails;
	}
	const WwVkPresent::CsmStat st = WwVkPresent::takeCsmStat();
	std::printf( "SHADOWVIEW stat: %d runs on Vulkan, %d failed (%s), %d draws, %d commands, %.3f ms a run (max %.3f), %d pipelines "
		"built in %.1f ms\n", st.runs, st.fails, st.failWhy.c_str(), st.draws, st.cmds, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs,
		st.pipelines, st.buildMs );
	f.bindFramebuffer( kFb, 0 );
	for ( int t = 0; t < 2; t++ )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &keyHome[t] ) );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "SHADOWVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
