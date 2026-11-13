// Lane VKOBJ1 (2026-11-13, plan row G4, slice 1): the objects' opaque depth prepass on the present device (vkpresent.cpp
// objRun, built in here with WW_VKPRESENT_NO_QT: the shipped code) against GL's own prepass program (lookdev_csmdepth_q.vert
// + lookdev_csmdepth.frag) drawn the way cellgpucull.cpp draws it, into multisample depth targets, every SAMPLE compared.
// Usage: objview <tree root>
// Arms: D24 x4, D32F x4, D24 x1 (MSAA off). Each: GL clears the target, then the occluders: GL with glMultiDrawElementsIndirect
// (span 0 back faces culled, span 1 two-sided, glPolygonOffset( 1, 2 ), LEQUAL), Vulkan with one objRun from the same pool,
// matrices and commands into the shared target. Scene: open random triangles of both windings, wavy plates wound CCW and CW,
// a ground plate, casters through the near plane, a perspective projection, an odd target size, 2 frames (moved matrices:
// the pending fence and the buffers reused).
// Readback per sample: a GL pass texelFetch()es each sample and writes its float bits into an R32UI target of width w * S.
// Prints `OBJVIEW <arm> frame<n> EXACT|FAIL`, `OBJVIEW RED <name> ...` (each must FAIL) and `OBJVIEW GATE OK|FAILED`.
// BAR (stated before the first run, never widened): EXACT (0 covered samples differ; covered = either side != 1.0; an arm
// with 0 covered samples FAILS).
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

namespace obv
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

constexpr GLenum kFb = 0x8D40, kDepthAtt = 0x8D00, kColorAtt0 = 0x8CE0, kComplete = 0x8CD5, kFrag = 0x8B30, kVert = 0x8B31;
constexpr GLenum kCompiled = 0x8B81, kLinked = 0x8B82, kArrayBuf = 0x8892, kElemBuf = 0x8893, kIndirect = 0x8F3F;
constexpr GLenum kDynamic = 0x88E8, kTexMs = 0x9100, kPolyFill = 0x8037, kMultisample = 0x809D, kTex0 = 0x84C0;
constexpr GLenum kD24 = 0x81A6, kD32f = 0x8CAC, kR32ui = 0x8236, kRedInt = 0x8D94;

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
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
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
	void( WINAPI * framebufferTexture2D )( GLenum, GLenum, GLenum, GLuint, GLint ) = nullptr;
	GLenum( WINAPI * checkFramebufferStatus )( GLenum ) = nullptr;
	void( WINAPI * texImage2DMultisample )( GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean ) = nullptr;
	void( WINAPI * activeTexture )( GLenum ) = nullptr;
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
		&& load( f.uniformMatrix4fv, "glUniformMatrix4fv" ) && load( f.uniform1i, "glUniform1i" )
		&& load( f.genBuffers, "glGenBuffers" ) && load( f.bindBuffer, "glBindBuffer" ) && load( f.bufferData, "glBufferData" )
		&& load( f.bufferSubData, "glBufferSubData" ) && load( f.genVertexArrays, "glGenVertexArrays" )
		&& load( f.bindVertexArray, "glBindVertexArray" ) && load( f.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( f.vertexAttribPointer, "glVertexAttribPointer" ) && load( f.vertexAttribDivisor, "glVertexAttribDivisor" )
		&& load( f.multiDrawElementsIndirect, "glMultiDrawElementsIndirect" ) && load( f.genFramebuffers, "glGenFramebuffers" )
		&& load( f.bindFramebuffer, "glBindFramebuffer" ) && load( f.framebufferTexture2D, "glFramebufferTexture2D" )
		&& load( f.checkFramebufferStatus, "glCheckFramebufferStatus" ) && load( f.texImage2DMultisample, "glTexImage2DMultisample" )
		&& load( f.activeTexture, "glActiveTexture" );
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
		std::printf( "OBJVIEW compile failed (%s): %.1500s\n", tag, log.data() );
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
		std::printf( "OBJVIEW link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}

// rotation about an axis (column-major 4x4), scaled, with a translation
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
	std::size_t covered = 0, diff = 0;
	std::uint32_t maxBits = 0;
	bool ok() const { return covered > 0 && diff == 0; }
	const char * word() const { return ok() ? "EXACT" : "FAIL"; }
};

Diff compare( const std::vector<std::uint32_t> & a, const std::vector<std::uint32_t> & b )
{
	Diff v;
	if ( a.size() != b.size() )
		return v;
	constexpr std::uint32_t kOne = 0x3F800000u;
	for ( std::size_t i = 0; i < a.size(); i++ ) {
		if ( a[i] == kOne && b[i] == kOne )
			continue;
		v.covered++;
		if ( a[i] != b[i] ) {
			v.diff++;
			v.maxBits = std::max( v.maxBits, a[i] > b[i] ? a[i] - b[i] : b[i] - a[i] );
		}
	}
	return v;
}
}  // namespace obv

int main( int argc, char ** argv )
{
	using namespace obv;
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 2 ) {
		std::printf( "usage: objview <tree root>\nOBJVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[1];
	const int W = 256, H = 256, TW = 333, TH = 217;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwobjview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwobjview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwobjview", "object prepass harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "OBJVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
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
		std::printf( "OBJVIEW GATE FAILED (GL entry points)\n" );
		return 2;
	}
	std::printf( "target %d x %d, GL on %s\n", TW, TH, reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	const GLuint prog = program( readFile( root + "/res/shaders/features/near/lookdev_csmdepth_q.vert" ),
		readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth.frag" ) );
	const std::string rbVs = "#version 410 core\nvoid main() { vec2 p = vec2( ( gl_VertexID << 1 ) & 2, gl_VertexID & 2 ); "
							 "gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 ); }\n";
	const std::string rbFs = "uniform int S;\nout uvec4 o;\nvoid main() { ivec2 p = ivec2( gl_FragCoord.xy ); int x = p.x / S; "
							 "int s = p.x - x * S;\n#ifdef MS\nfloat z = texelFetch( d, ivec2( x, p.y ), s ).r;\n#else\n"
							 "float z = texelFetch( d, ivec2( x, p.y ), 0 ).r;\n#endif\no = uvec4( floatBitsToUint( z ) ); }\n";
	const GLuint rbMs = program( rbVs, "#version 410 core\n#define MS 1\nuniform sampler2DMS d;\n" + rbFs );
	const GLuint rb1 = program( rbVs, "#version 410 core\nuniform sampler2D d;\n" + rbFs );
	if ( !prog || !rbMs || !rb1 ) {
		std::printf( "OBJVIEW GATE FAILED (GL program)\n" );
		return 2;
	}
	const GLint lClip = f.getUniformLocation( prog, "csmClipFromView" );

	// ---- the pool: open random triangles (each its own winding), wavy plates both ways, a ground plate
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
	const int kGround = int( meshes.size() );
	beginMesh();
	{
		const float g[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		pos.insert( pos.end(), g, g + 12 );
		const std::uint16_t gi[6] = { 0, 1, 2, 2, 1, 3 };
		idx.insert( idx.end(), gi, gi + 6 );
	}
	endMesh();

	// ---- occluders (model-view per occluder): 48 culled + 1 ground culled, then 16 two-sided; 4 of the culled cross the near plane
	const int kCulled = 49, kTwo = 16, kN = kCulled + kTwo;
	std::vector<DrawCmd> cmds( kN );
	struct Occ
	{
		double ax, ay, az, ang, tx, ty, tz, s;
	};
	std::vector<Occ> occ( kN );
	for ( int i = 0; i < kN; i++ ) {
		const bool ground = i == kCulled - 1;
		const int m = ground ? kGround : int( rng() % std::uint32_t( kGround ) );
		cmds[i] = { meshes[m].count, 1, meshes[m].firstIndex, meshes[m].baseVertex, std::uint32_t( i ) };
		const double depth = i < 4 ? 1.0 + 0.5 * u( rng ) : 8.0 + 220.0 * ( 0.5 + 0.5 * u( rng ) );
		occ[i] = { u( rng ), u( rng ), u( rng ) + 1.5, 3.14159 * u( rng ), 0.55 * depth * u( rng ), 0.4 * depth * u( rng ), -depth,
			i < 4 ? 2.5 : 2.0 + 0.12 * depth };
		if ( ground )  // the ground the camera looks down on (seen from above: its CCW side)
			occ[i] = { 1.0, 0.0, 0.0, -1.2, 0.0, -12.0, -120.0, 160.0 };
	}
	std::vector<float> mats( size_t( kN ) * 16 );
	auto setMats = [&]( double move ) {
		for ( int i = 0; i < kN; i++ ) {
			const Occ & c = occ[i];
			double M[16];
			rotT( c.ax, c.ay, c.az, c.ang + 0.3 * move, c.tx + 1.5 * move, c.ty - 1.0 * move, c.tz + 0.7 * move, c.s, M );
			for ( int k = 0; k < 16; k++ )
				mats[size_t( i ) * 16 + k] = float( M[k] );
		}
	};
	// the camera's projection (cellgpucull.cpp's pm): perspective, near 0.75, far 4000, the target's aspect
	float clip[16] = {};
	{
		const double fy = 1.0 / std::tan( 0.5 * 1.15 ), aspect = double( TW ) / double( TH ), n = 0.75, fr = 4000.0;
		clip[0] = float( fy / aspect );
		clip[5] = float( fy );
		clip[10] = float( ( fr + n ) / ( n - fr ) );
		clip[11] = -1.0f;
		clip[14] = float( 2.0 * fr * n / ( n - fr ) );
	}

	// ---- GL buffers (the pool, matrices, commands) and the VAO as cellqueue.cpp binds it
	GLuint bPos = 0, bIdx = 0, bMat = 0, bCmd = 0, vao = 0, vaoEmpty = 0;
	f.genBuffers( 1, &bPos );
	f.genBuffers( 1, &bIdx );
	f.genBuffers( 1, &bMat );
	f.genBuffers( 1, &bCmd );
	f.genVertexArrays( 1, &vaoEmpty );
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
	f.bindVertexArray( 0 );
	std::printf( "pool %zu vertices, %zu indices, %d meshes, %d commands (%d culled, %d two-sided)\n", pos.size() / 3, idx.size(),
		int( meshes.size() ), kN, kCulled, kTwo );

	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, 4, glProc, why ) ) {
		std::printf( "OBJVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	const std::string no = WwVkPresent::objRefusal();
	if ( !no.empty() ) {
		std::printf( "OBJVIEW GATE FAILED (objRefusal: %s)\n", no.c_str() );
		return 2;
	}
	GLuint fbo = 0, rbFbo = 0, rbTex = 0;
	f.genFramebuffers( 1, &fbo );
	f.genFramebuffers( 1, &rbFbo );
	glGenTextures( 1, &rbTex );

	struct Arm
	{
		const char * name;
		GLenum fmt;
		int samples;
	};
	const Arm arms[3] = { { "d24x4", kD24, 4 }, { "d32fx4", kD32f, 4 }, { "d24x1", kD24, 1 } };
	// diagnosis only: OBJVIEW_BIAS="slope,unit" on both sides (the gate runs the app's 1, 2)
	float bSlope = 1.0f, bUnit = 2.0f;
	if ( const char * e = std::getenv( "OBJVIEW_BIAS" ) )
		if ( std::sscanf( e, "%f,%f", &bSlope, &bUnit ) == 2 )
			std::printf( "OBJVIEW diagnosis bias slope %g unit %g (not a gate run)\n", bSlope, bUnit );
	static unsigned keyHome[3];
	bool ok = true;
	for ( int a = 0; a < 3; a++ ) {
		const Arm & A = arms[a];
		const GLenum target = A.samples > 1 ? kTexMs : GL_TEXTURE_2D;
		GLuint refT = 0, shT = 0;
		glGenTextures( 1, &refT );
		glBindTexture( target, refT );
		if ( A.samples > 1 )
			f.texImage2DMultisample( kTexMs, A.samples, A.fmt, TW, TH, GL_TRUE );
		else {
			glTexImage2D( GL_TEXTURE_2D, 0, GLint( A.fmt ), TW, TH, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		}
		glBindTexture( target, 0 );
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( &keyHome[a] );
		if ( !WwVkPresent::sharedTarget( key, 0, TW, TH, 1, A.fmt, shT, why, 1, A.samples ) ) {
			std::printf( "OBJVIEW %s FAIL (sharedTarget: %s)\n", A.name, why.c_str() );
			ok = false;
			glDeleteTextures( 1, &refT );
			continue;
		}
		// the readback target: one R32UI texel per sample
		glBindTexture( GL_TEXTURE_2D, rbTex );
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( kR32ui ), TW * A.samples, TH, 0, kRedInt, GL_UNSIGNED_INT, nullptr );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glBindTexture( GL_TEXTURE_2D, 0 );

		auto attachClear = [&]( GLuint tex ) {
			f.bindFramebuffer( kFb, fbo );
			f.framebufferTexture2D( kFb, kDepthAtt, target, tex, 0 );
			glDrawBuffer( GL_NONE );
			glReadBuffer( GL_NONE );
			if ( f.checkFramebufferStatus( kFb ) != kComplete )
				return false;
			glDisable( GL_SCISSOR_TEST );
			glViewport( 0, 0, TW, TH );
			glDepthMask( GL_TRUE );
			glClearDepth( 1.0 );
			glClear( GL_DEPTH_BUFFER_BIT );
			return true;
		};
		auto readBack = [&]( GLuint tex, std::vector<std::uint32_t> & out ) {
			f.bindFramebuffer( kFb, rbFbo );
			f.framebufferTexture2D( kFb, kColorAtt0, GL_TEXTURE_2D, rbTex, 0 );
			const GLenum att = kColorAtt0;
			glDrawBuffer( att );
			glReadBuffer( att );
			if ( f.checkFramebufferStatus( kFb ) != kComplete )
				return false;
			const GLuint p = A.samples > 1 ? rbMs : rb1;
			f.useProgram( p );
			f.activeTexture( kTex0 );
			glBindTexture( target, tex );
			f.uniform1i( f.getUniformLocation( p, "d" ), 0 );
			f.uniform1i( f.getUniformLocation( p, "S" ), A.samples );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_CULL_FACE );
			glDisable( kPolyFill );
			glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			glViewport( 0, 0, TW * A.samples, TH );
			f.bindVertexArray( vaoEmpty );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			glBindTexture( target, 0 );
			out.assign( size_t( TW ) * A.samples * TH, 0 );
			glPixelStorei( GL_PACK_ALIGNMENT, 1 );
			glReadPixels( 0, 0, TW * A.samples, TH, kRedInt, GL_UNSIGNED_INT, out.data() );
			return true;
		};
		// cellgpucull.cpp's prepass state, then its two spans
		auto runGl = [&]( std::vector<std::uint32_t> & out ) {
			if ( !attachClear( refT ) )
				return false;
			f.useProgram( prog );
			f.uniformMatrix4fv( lClip, 1, GL_FALSE, clip );
			glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
			glDisable( GL_BLEND );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_TRUE );
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			glFrontFace( GL_CCW );
			glCullFace( GL_BACK );
			glEnable( kPolyFill );
			glPolygonOffset( bSlope, bUnit );
			glEnable( kMultisample );
			f.bindVertexArray( vao );
			f.bindBuffer( kIndirect, bCmd );
			glEnable( GL_CULL_FACE );
			f.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( kCulled ), GLsizei( sizeof( DrawCmd ) ) );
			glDisable( GL_CULL_FACE );
			f.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::size_t( kCulled ) * sizeof( DrawCmd ) ),
				GLsizei( kTwo ), GLsizei( sizeof( DrawCmd ) ) );
			glDisable( kPolyFill );
			f.bindVertexArray( 0 );
			return readBack( refT, out ) && glGetError() == GL_NO_ERROR;
		};
		enum Red { kNone, kNoDepth, kWinding, kBinding, kFloatUnit };
		auto runVk = [&]( Red red, std::vector<std::uint32_t> & out ) {
			if ( !attachClear( shT ) )
				return false;
			std::vector<DrawCmd> c = cmds;
			if ( red == kBinding )  // red: each draw reads the next occluder's matrix
				for ( DrawCmd & d : c )
					d.baseInstance = ( d.baseInstance + 1 ) % std::uint32_t( kN );
			WwVkPresent::ObjRun run;
			run.w = TW;
			run.h = TH;
			run.samples = A.samples;
			run.tex = shT;
			run.key = key;
			run.pos = bPos;
			run.idx = bIdx;
			run.posBytes = pos.size() * 4;
			run.idxBytes = idx.size() * 2;
			run.mats = mats.data();
			run.nMats = std::size_t( kN );
			run.cmds = reinterpret_cast<const std::uint32_t *>( c.data() );
			run.nCmds = c.size();
			run.split = std::size_t( kCulled );
			std::memcpy( run.clip, clip, sizeof( clip ) );
			run.unit = red == kFloatUnit ? 0.5f * bUnit : bUnit;  // red: Vulkan's own float unit (objRun doubles it for D32F)
			run.slope = bSlope;
			run.red = red == kNoDepth ? 1 : red == kWinding ? 2 : 0;
			std::string w;
			if ( !WwVkPresent::objRun( run, w ) ) {
				std::printf( "OBJVIEW objRun refused: %s\n", w.c_str() );
				return false;
			}
			return readBack( shT, out ) && glGetError() == GL_NO_ERROR;
		};
		auto upload = [&]( double move ) {
			setMats( move );
			f.bindBuffer( kArrayBuf, bMat );
			f.bufferSubData( kArrayBuf, 0, std::ptrdiff_t( mats.size() * 4 ), mats.data() );
		};

		std::vector<std::uint32_t> ref, got;
		for ( int fr = 0; fr < 2; fr++ ) {
			upload( fr ? 1.0 : 0.0 );
			if ( !runGl( ref ) || !runVk( kNone, got ) ) {
				std::printf( "OBJVIEW %s frame%d FAIL (a run failed)\n", A.name, fr );
				ok = false;
				continue;
			}
			const Diff v = compare( got, ref );
			std::printf( "OBJVIEW %s frame%d %s (%zu of %zu covered samples differ, max %u float bits)\n", A.name, fr, v.word(), v.diff,
				v.covered, v.maxBits );
			ok = ok && v.ok();
		}
		if ( A.fmt == kD32f && std::getenv( "OBJVIEW_RULE" ) ) {  // diagnosis: the unit term per side, in 2^-24 and in ulps of z
			std::vector<std::uint32_t> gl0, vk0;
			const float u = bUnit, s = bSlope;
			bUnit = 0.0f;
			bSlope = 0.0f;
			runGl( gl0 );
			runVk( kNone, vk0 );
			bUnit = u;
			bSlope = s;
			int shown = 0;
			for ( std::size_t i = 0; i < ref.size() && shown < 24; i += ref.size() / 997 + 1 ) {
				float z0, zg, zv, zv0;
				std::memcpy( &z0, &gl0[i], 4 );
				std::memcpy( &zv0, &vk0[i], 4 );
				std::memcpy( &zg, &ref[i], 4 );
				std::memcpy( &zv, &got[i], 4 );
				if ( z0 >= 1.0f )
					continue;
				int e = 0;
				std::frexp( z0, &e );
				const double ulp = std::ldexp( 1.0, e - 24 );
				std::printf( "OBJVIEW RULE z %.9g vk0-gl0 %g | gl %.4g x2^-24 %.4g ulps | vk %.4g x2^-24 %.4g ulps\n", z0, double( zv0 - z0 ),
					( zg - z0 ) / std::ldexp( 1.0, -24 ), ( zg - z0 ) / ulp, ( zv - zv0 ) / std::ldexp( 1.0, -24 ), ( zv - zv0 ) / ulp );
				shown++;
			}
		}
		if ( A.fmt == kD32f ) {  // red: the float unit left at Vulkan's 2^(e-24) must FAIL
			const bool ran = runVk( kFloatUnit, got );
			const Diff v = compare( got, ref );
			const bool redFails = !ran || !v.ok();
			std::printf( "OBJVIEW RED %s float unit (Vulkan's 2^(e-24), not GL's 2^(e-23)) (%zu of %zu covered samples differ; must FAIL)\n",
				redFails ? "FAIL" : "EXACT", v.diff, v.covered );
			ok = ok && redFails;
		}
		if ( a == 0 ) {  // the reds against frame 1's reference: each must FAIL
			const struct
			{
				const char * name;
				Red red;
			} reds[3] = { { "nodepth (compare ALWAYS)", kNoDepth }, { "cull (GL's CCW front taken as Vulkan's)", kWinding },
				{ "binding (each draw with the next occluder's matrix)", kBinding } };
			for ( const auto & r : reds ) {
				const bool ran = runVk( r.red, got );
				const Diff v = compare( got, ref );
				const bool redFails = !ran || !v.ok();
				std::printf( "OBJVIEW RED %s %s (%zu of %zu covered samples differ; must FAIL)\n", redFails ? "FAIL" : "EXACT", r.name, v.diff,
					v.covered );
				ok = ok && redFails;
			}
		}
		f.bindFramebuffer( kFb, 0 );
		WwVkPresent::sharedTargetDrop( key );
		glDeleteTextures( 1, &refT );
	}
	const WwVkPresent::ObjStat st = WwVkPresent::takeObjStat();
	std::printf( "OBJVIEW stat: %d runs on Vulkan, %d failed (%s), %d commands (%d two-sided), %.3f ms a run (max %.3f), %d pipelines "
				 "built in %.1f ms\n",
		st.runs, st.fails, st.failWhy.c_str(), st.cmds, st.twoSided, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "OBJVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
