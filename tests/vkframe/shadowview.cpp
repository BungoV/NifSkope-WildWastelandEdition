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
// Lane VKSHADOW4 (2026-11-15, item 3 part 1), lines `SHADOWVIEW mesh ...` after the old ones (those stay byte-equal): the
// per-draw casters (sunshadow.cpp drawShape 1872 whole, 1866 cullIdx ranges) as CsmMesh draws against GL's own per-draw
// program lookdev_csmdepth.vert (uniforms.glsl spliced in, farInst / lodTreeAnim 0, farSwayAmp 0: the solid non-sway
// casters part 1 moves) + lookdev_csmdepth.frag, the same BOUND. Frame 0: mesh draws only (no queue pool: nCmds 0); frame 1:
// the queue too, one mesh re-specified (same sizes, new positions and gen: a CPU-skinned caster), must count regen >= 1;
// frame 2: a 6561-vertex mesh (77 KiB, past the pool's 64 KiB floor) joins, must count repacks >= 1. Frame 1 must show the
// mesh casters winning > 0 texels against GL without them. Reds after frame 2 (each must FAIL past 1 unit): whole (the
// ranges drawn as the whole mesh; the fixture's excluded triangles are in view), split, nobias, cull on the mesh draws.
// Pool reds, each its own process (shadowview.sh): WW_VKCSM_POOL_RED=1 (key only) a frame1 layer FAILs with redSkips >= 1,
// =2 (grow without copying again) a frame2 layer FAILs with repacks >= 1. The 3-frame shape is what lets both reach their
// branch: the re-specify (frame 1) and the growth (frame 2) are in different frames, else the repack hides red 1.
// Lane VKSHADOW5 (2026-11-16, item 3 part 2 bar), lines `SHADOWVIEW ileave ...` after the mesh ones (those stay byte-equal):
// the ground quad (sunshadow.cpp drawShape 3: 4 world-space vertices, indices 0 1 2 2 1 3, model-view vt, culled, in every
// job's culled mesh draws) as one more pooled CsmMesh, and the run's draws in the app's order: per job its queue span, then
// its mesh draws (sunshadow.cpp emits queue, mesh, queue, mesh), so csmWork's mesh -> queue rebind inside one render pass
// runs. Two ground model-views: one the light sees from the front, one from below (GL culls it). The same BOUND as above.
// Two frames: the ground moves in frame 1 (same key and sizes, new gen: gxy / ghalf / gz moved). Must hold: every layer
// EXACT or BOUND, the ground wins > 0 texels in frame 0 against GL without it, requeues >= 1 each frame (CsmStat: a queue
// draw after a mesh draw in one render pass: the rebind ran), regen >= 1 in frame 1. Red (must FAIL past 1 unit): the
// ground drawn without cull (the from-below quad then lands). No red removes the rebind: a queue draw on the mesh pool's
// buffers reads out of range on the GPU (a hang risk on the desktop), so the requeues count is the proof that it ran.
// Lane VKSHADOW6 (2026-11-16, item 3 part 3 bar), lines `SHADOWVIEW remap ...` after the ileave ones (those stay byte-equal):
// the cache's Scroll (sunshadow.cpp: scratch <- old static layer i, static i = clear + remap(i) + GL casters + set 1, live
// i = static i copied whole + set 2) with the remap moved into the crossing (route B): kCascades shared scratch layers
// (sharedTarget type 2 on its own key, CsmRun::scratch), GL blits old static i into scratch layer i, the remap is a
// CsmDraw (remap, src, srcX / srcY / srcW / srcH, zScale / zBias) recorded into BOTH static i and live i (GL's
// static -> live copy now carries no remap), drawn LEQUAL: the remap is a hand-written depth (lookdev_csmremap.frag), and
// GL's ALWAYS onto a fresh clear followed by LEQUAL casters keeps min(remap, casters), so LEQUAL in any order is the same.
// GL reference: GL's own lookdev_csmremap.vert/.frag through a 1-layer scratch, GL_ALWAYS, in the app's order. The old
// windows' content is frame 0's live layers of the base case (each arm its own: the inputs must be byte-equal on both
// arms, printed); frame 1's matrices draw the new windows. Offsets and old sizes put the new viewport outside the old
// window on all four sides; zScale / zBias away from 1 and powers of two. The same BOUND as above (EXACT expected).
// Seeding change (VKSHADOW6 run g1, before any frame-1 verdict): fixed zScale / zBias (1.31, -0.27 and 1.13, -0.04) left
// "clamped to 0" and "clamped to 1" at 0 texels (the old depths sit in a narrow band), so cascades 0 and 2 now map their own
// old layer's 25th / 75th depth percentile to 0 / 1 (a quarter clamps each way); cascade 1 keeps 0.77, 0.29 (in range).
// Must hold: every layer EXACT or BOUND; remap branch counts each > 0 (outside, o == 1, clamped to 0, to 1, in range);
// the remap wins > 0 texels against GL without it; GL's casters beat the remap on > 0 texels (else the ALWAYS red
// proves nothing); the late GL fallback (refusal: GL remaps LEQUAL into static and live after its casters, then the
// records, from scratch layer i through the 1-layer scratch) EXACT or BOUND, no device. Reds (each must FAIL past 1
// unit): plain (no remap draws: the plain clear), wrong (scratch layer of the next cascade), static (the remap recorded
// into the static layers only: the live layers FAIL). Process red (shadowview.sh): WW_VKCSM_REMAP_RED=1 builds the remap
// pipeline GL_ALWAYS (it then overwrites GL's casters): a remap frame1 layer FAILs past 1 unit with remapRed >= 1.
// Lane VKSHADOW7 (2026-11-16, item 1 bar), lines `SHADOWVIEW alpha ...` after the remap ones: the alpha-tested per-draw
// casters on the cache's BC twins; the case, its BOUND and its reds: shadowview_alpha.inc.
// Lane VKSHADOW10 (2026-11-16, item 3 bar), lines `SHADOWVIEW sway ...` after the alpha ones: the swaying per-draw casters
// (lookdev_csmdepth.vert's far wind); the case, its BOUND and its reds: shadowview_sway.inc.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"
#include "../../src/features/shadows/shadowatsampler.h"  // lane VKSHADOW9 (R1): the app's alpha-test sampler

#include <GL/gl.h>
#include <algorithm>
#include <array>
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
	// lane VKSHADOW4: the per-draw program's uniforms and its globalUniforms block
	void( WINAPI * uniform1i )( GLint, GLint ) = nullptr;
	void( WINAPI * uniform1f )( GLint, GLfloat ) = nullptr;
	GLuint( WINAPI * getUniformBlockIndex )( GLuint, const char * ) = nullptr;
	void( WINAPI * uniformBlockBinding )( GLuint, GLuint, GLuint ) = nullptr;
	void( WINAPI * bindBufferBase )( GLenum, GLuint, GLuint ) = nullptr;
	void( WINAPI * blitFramebuffer )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
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
	load( f.uniform1i, "glUniform1i" );
	load( f.uniform1f, "glUniform1f" );
	load( f.getUniformBlockIndex, "glGetUniformBlockIndex" );
	load( f.uniformBlockBinding, "glUniformBlockBinding" );
	load( f.bindBufferBase, "glBindBufferBase" );
	load( f.blitFramebuffer, "glBlitFramebuffer" );
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

	// ---- lane VKSHADOW4 (item 3 part 1): the per-draw casters as pooled meshes (CsmMesh), see the header
	bool meshOk = true, ilOk = true;
	{
		std::string vsM = readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth.vert" );
		const std::string inc = "#include \"uniforms.glsl\"";
		const std::size_t incAt = vsM.find( inc );
		if ( incAt != std::string::npos )
			vsM.replace( incAt, inc.size(), readFile( root + "/res/shaders/uniforms.glsl" ) );
		const GLuint progM = incAt == std::string::npos ? 0
			: program( vsM, readFile( root + "/res/shaders/features/shadows/lookdev_csmdepth.frag" ) );
		if ( !progM || !f.uniform1i || !f.uniform1f || !f.getUniformBlockIndex || !f.uniformBlockBinding || !f.bindBufferBase ) {
			std::printf( "SHADOWVIEW mesh FAIL (the per-draw program or its entry points)\n" );
			meshOk = false;
		}
		constexpr GLenum kUbo = 0x8A11;
		const GLint mClip = meshOk ? f.getUniformLocation( progM, "csmClipFromView" ) : -1;
		const GLint mMv = meshOk ? f.getUniformLocation( progM, "modelViewMatrix" ) : -1;
		GLuint ubo = 0, vaoM = 0;
		if ( meshOk ) {
			f.useProgram( progM );
			f.uniform1i( f.getUniformLocation( progM, "farInst" ), 0 );
			f.uniform1i( f.getUniformLocation( progM, "lodTreeAnim" ), 0 );
			f.uniform1f( f.getUniformLocation( progM, "farSwayAmp" ), 0.0f );
			const std::vector<char> zeros( 4096, 0 );
			f.genBuffers( 1, &ubo );
			f.bindBuffer( kUbo, ubo );
			f.bufferData( kUbo, 4096, zeros.data(), kDynamic );
			const GLuint blk = f.getUniformBlockIndex( progM, "globalUniforms" );
			if ( blk != 0xFFFFFFFFu )
				f.uniformBlockBinding( progM, blk, 0 );
			f.bindBufferBase( kUbo, 0, ubo );
			f.genVertexArrays( 1, &vaoM );
		}

		// the meshes: a soup, plates CCW / CW, a plate re-specified in frame 1, a big plate first drawn in frame 2
		struct HMesh
		{
			std::vector<float> p;
			std::vector<std::uint16_t> i;
			std::uint64_t gen = 0;
			GLuint vb = 0, ib = 0;
		};
		std::vector<HMesh> hm( 5 );
		auto plate = [&]( HMesh & m, int n, bool cw, float amp, float ph ) {
			m.p.clear();
			m.i.clear();
			for ( int y = 0; y <= n; y++ )
				for ( int x = 0; x <= n; x++ ) {
					const float px = -1.0f + 2.0f * float( x ) / n, py = -1.0f + 2.0f * float( y ) / n;
					m.p.push_back( px );
					m.p.push_back( py );
					m.p.push_back( amp * std::sin( 3.0f * px + ph ) * std::cos( 2.0f * py ) );
				}
			for ( int y = 0; y < n; y++ )
				for ( int x = 0; x < n; x++ ) {
					const std::uint16_t a = std::uint16_t( y * ( n + 1 ) + x ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + n + 1 ),
						d = std::uint16_t( c + 1 );
					const std::uint16_t q[6] = { a, b, d, a, d, c };
					for ( int k = 0; k < 6; k++ )
						m.i.push_back( cw ? q[5 - k] : q[k] );
				}
		};
		for ( int t = 0; t < 70; t++ ) {
			const float qx = u( rng ), qy = u( rng ), qz = u( rng );
			for ( int k = 0; k < 3; k++ ) {
				hm[0].p.push_back( qx + 0.45f * u( rng ) );
				hm[0].p.push_back( qy + 0.45f * u( rng ) );
				hm[0].p.push_back( qz + 0.45f * u( rng ) );
				hm[0].i.push_back( std::uint16_t( t * 3 + k ) );
			}
		}
		plate( hm[1], 12, false, 0.25f, 0.0f );
		plate( hm[2], 12, true, 0.25f, 0.0f );
		plate( hm[3], 16, false, 0.2f, 0.0f );
		plate( hm[4], 80, false, 0.15f, 0.0f );
		auto upMesh = [&]( HMesh & m ) {
			if ( !m.vb ) {
				f.genBuffers( 1, &m.vb );
				f.genBuffers( 1, &m.ib );
			}
			f.bindBuffer( kArrayBuf, m.vb );
			f.bufferData( kArrayBuf, std::ptrdiff_t( m.p.size() * 4 ), m.p.data(), kDynamic );
			f.bindBuffer( kElemBuf, m.ib );
			f.bufferData( kElemBuf, std::ptrdiff_t( m.i.size() * 2 ), m.i.data(), kDynamic );
		};
		for ( std::size_t m = 0; m < hm.size() && meshOk; m++ ) {
			hm[m].gen = 1000 * ( m + 1 );
			f.bindVertexArray( vaoM );
			upMesh( hm[m] );
		}

		// the casters: 14 on meshes 0-3, 2 on the big mesh
		const int kMC = 16;
		std::vector<Caster> mc( kMC );
		std::vector<int> meshOfC( kMC );
		for ( int i = 0; i < kMC; i++ ) {
			meshOfC[i] = i < 14 ? i % 4 : 4;
			mc[i] = { u( rng ), u( rng ), u( rng ) + 1.5, 3.14159 * u( rng ), 90.0 * u( rng ), 90.0 * u( rng ), -150.0 + 80.0 * u( rng ),
				i < 14 ? 8.0 + 22.0 * ( 0.5 + 0.5 * u( rng ) ) : 40.0 };
		}
		std::vector<float> mm( std::size_t( kMC ) * 16 );
		auto setMM = [&]( double move ) {
			for ( int i = 0; i < kMC; i++ ) {
				const Caster & c = mc[i];
				double M[16];
				rotT( c.ax, c.ay, c.az, c.ang + 0.2 * move, c.tx - 4.0 * move, c.ty + 6.0 * move, c.tz + 2.0 * move, c.s, M );
				for ( int k = 0; k < 16; k++ )
					mm[std::size_t( i ) * 16 + k] = float( M[k] );
			}
		};

		// the draw list (sunshadow.cpp's per-draw loop per job): target, layer, cascade, scissor, cull, casters, ranges, from frame
		struct MRec
		{
			int target, layer, cascade;
			int sc[4];
			bool cull;
			std::vector<int> cs;
			bool ranges;
			int from;
		};
		const std::vector<MRec> mrecs = {
			{ 0, 0, 0, { 0, 0, 0, 0 }, true, { 0, 1, 2, 3, 4, 5 }, false, 0 },
			{ 0, 0, 0, { 0, 0, 0, 0 }, false, { 6, 7 }, false, 0 },  // two-sided
			{ 0, 1, 1, { 0, 700, map - 24, map - 11 }, true, { 0, 1, 2, 3, 8, 9 }, false, 0 },  // a scissor strip
			{ 0, 1, 1, { 0, 0, 0, 0 }, true, { 8, 9 }, true, 0 },  // cullIdx ranges (1866)
			{ 1, 2, 2, { 0, 0, 0, 0 }, true, { 10, 11, 12, 13 }, true, 0 },  // the static target, ranges
			{ 0, 2, 2, { 0, 0, 0, 0 }, true, { 10, 11, 12, 13 }, true, 0 },  // ... duplicated into the live one
			{ 0, 0, 0, { 0, 0, 0, 0 }, true, { 14, 15 }, false, 2 },  // the big mesh: the pool grows
			{ 1, 1, 1, { 0, 0, 0, 0 }, false, { 14 }, false, 2 },
		};
		// a rec's draws {caster, first index, index count}: the whole mesh, or two ranges (the first third, a fifth from the middle)
		auto drawsOf = [&]( const MRec & R, bool whole, int fr ) {
			std::vector<std::array<std::uint32_t, 3>> out;
			if ( fr < R.from )
				return out;
			for ( int c : R.cs ) {
				const std::uint32_t n = std::uint32_t( hm[std::size_t( meshOfC[c] )].i.size() ), tris = n / 3;
				if ( R.ranges && !whole ) {
					out.push_back( { std::uint32_t( c ), 0u, ( tris / 3 ) * 3 } );
					out.push_back( { std::uint32_t( c ), ( tris / 2 ) * 3, ( tris / 5 ) * 3 } );
				} else {
					out.push_back( { std::uint32_t( c ), 0u, n } );
				}
			}
			return out;
		};
		auto glMesh = [&]( const GLuint * tex, const MRec & R, int fr ) {
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
			f.uniformMatrix4fv( mClip, 1, GL_FALSE, c.clip );
			R.cull ? glEnable( GL_CULL_FACE ) : glDisable( GL_CULL_FACE );
			for ( const auto & d : drawsOf( R, false, fr ) ) {
				const HMesh & m = hm[std::size_t( meshOfC[d[0]] )];
				f.uniformMatrix4fv( mMv, 1, GL_FALSE, &mm[std::size_t( d[0] ) * 16] );
				f.bindBuffer( kArrayBuf, m.vb );
				f.enableVertexAttribArray( 0 );
				f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
				f.bindBuffer( kElemBuf, m.ib );
				glDrawElements( GL_TRIANGLES, GLsizei( d[2] ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::size_t( d[1] ) * 2 ) );
			}
			if ( sc )
				glDisable( GL_SCISSOR_TEST );
			return true;
		};
		auto runGlM = [&]( int fr, bool queue, bool meshes, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( refT );
			glState();
			ok2 = ok2 && ground( refT );
			for ( const Rec & R : recs )
				ok2 = ok2 && ( !queue || glDraw( refT, R ) );
			if ( meshes ) {
				f.useProgram( progM );
				f.bindVertexArray( vaoM );
				for ( const MRec & R : mrecs )
					ok2 = ok2 && glMesh( refT, R, fr );
			}
			readAll( refT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		enum MRed { kMNone, kMWhole, kMSplit, kMNoBias, kMNoCull };
		auto runVkM = [&]( int fr, bool queue, MRed red, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( shT );
			glState();
			ok2 = ok2 && ground( shT );
			WwVkPresent::CsmRun run;
			run.map = map;
			run.layers = 3;
			for ( int t = 0; t < 2; t++ ) {
				run.tex[t] = shT[t];
				run.key[t] = reinterpret_cast<std::uintptr_t>( &keyHome[t] );
			}
			// the matrices: the queue's casters, then the mesh casters (one buffer, as sunshadow.cpp appends them)
			std::vector<float> allM( mats );
			allM.insert( allM.end(), mm.begin(), mm.end() );
			run.mats = allM.data();
			run.nMats = allM.size() / 16;
			auto put = [&]( const Casc & c, int ci, int target, int layer, const int * scr, bool cull, std::uint32_t first,
						   std::uint32_t n, bool mesh ) {
				WwVkPresent::CsmDraw d;
				d.target = target;
				d.layer = layer;
				d.vw = c.vw;
				d.vh = c.vh;
				std::memcpy( d.sc, scr, sizeof( d.sc ) );
				std::memcpy( d.clip, casc[ci].clip, sizeof( d.clip ) );
				const bool nb = mesh && red == kMNoBias;
				d.unit = nb ? 0.0f : unitBias;
				d.slope = nb ? 0.0f : slopeBias;
				d.clamp = nb ? 0.0f : c.clamp;
				d.cull = mesh && red == kMNoCull ? false : cull;
				d.first = first;
				d.n = n;
				d.mesh = mesh;
				run.d.push_back( d );
			};
			if ( queue ) {
				run.pos = bPos;
				run.idx = bIdx;
				run.posBytes = pos.size() * 4;
				run.idxBytes = idx.size() * 2;
				run.cmds = reinterpret_cast<const std::uint32_t *>( cmds.data() );
				run.nCmds = cmds.size();
				for ( const Rec & R : recs )
					put( casc[R.cascade], R.cascade, R.target, R.layer, R.sc, R.cull, R.first, R.n, false );
			}
			std::vector<WwVkPresent::CsmMesh> cm( hm.size() );
			for ( std::size_t m = 0; m < hm.size(); m++ ) {
				cm[m].key = reinterpret_cast<std::uintptr_t>( &hm[m] );
				cm[m].gen = hm[m].gen;
				cm[m].pos = hm[m].p.data();
				cm[m].verts = std::uint32_t( hm[m].p.size() / 3 );
				cm[m].idx = hm[m].i.data();
				cm[m].indices = std::uint32_t( hm[m].i.size() );
			}
			std::vector<std::uint32_t> md;
			for ( const MRec & R : mrecs ) {
				const std::uint32_t first = std::uint32_t( md.size() / 4 );
				for ( const auto & d : drawsOf( R, red == kMWhole, fr ) ) {
					md.push_back( std::uint32_t( meshOfC[d[0]] ) );
					md.push_back( std::uint32_t( kCasters ) + d[0] );
					md.push_back( d[1] );
					md.push_back( d[2] );
				}
				const std::uint32_t n = std::uint32_t( md.size() / 4 ) - first;
				if ( n )
					put( casc[R.cascade], red == kMSplit ? ( R.cascade + 1 ) % 3 : R.cascade, R.target, R.layer, R.sc, R.cull, first, n,
						true );
			}
			run.meshes = cm.data();
			run.nMeshes = cm.size();
			run.mdraws = md.data();
			run.nMDraws = md.size() / 4;
			std::string w;
			if ( !WwVkPresent::csmRun( run, w ) ) {
				std::printf( "SHADOWVIEW mesh csmRun refused: %s\n", w.c_str() );
				return false;
			}
			readAll( shT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};

		std::vector<std::uint16_t> refM[3][2][3], vkM[2][3], bare[2][3];
		int regen1 = 0, repacks2 = 0, copies0 = 0;
		for ( int fr = 0; fr < 3 && meshOk; fr++ ) {
			const bool queue = fr > 0;
			if ( fr == 1 ) {  // a CPU-skinned caster: the same sizes, new positions, a new generation
				plate( hm[3], 16, false, 0.6f, 1.3f );
				hm[3].gen++;
				f.bindVertexArray( vaoM );
				upMesh( hm[3] );
			}
			upload( moves[fr > 0 ? 1 : 0] );
			setMM( double( fr ) * 0.5 );
			if ( !runGlM( fr, queue, true, refM[fr] ) || !runVkM( fr, queue, kMNone, vkM ) ) {
				std::printf( "SHADOWVIEW mesh frame%d FAIL (a run failed)\n", fr );
				meshOk = false;
				continue;
			}
			const WwVkPresent::CsmStat ps = WwVkPresent::takeCsmStat();
			std::printf( "SHADOWVIEW mesh pool frame%d copies=%d hits=%d regen=%d repacks=%d redSkips=%d mdraws=%d (%.1f KiB copied)\n", fr,
				ps.copies, ps.hits, ps.regen, ps.repacks, ps.redSkips, ps.mdraws, ps.copyKb );
			copies0 += fr == 0 ? ps.copies : 0;
			regen1 += fr == 1 ? ps.regen : 0;
			repacks2 += fr == 2 ? ps.repacks : 0;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ ) {
					const Diff v = compare( vkM[t][l], refM[fr][t][l] );
					std::printf( "SHADOWVIEW mesh frame%d %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", fr,
						t ? "static" : "live", l, v.word(), v.diff, v.covered, v.past1, v.maxu );
					meshOk = meshOk && v.ok();
				}
			if ( fr == 1 ) {  // the mesh casters must win texels, or the case proves nothing
				std::size_t won = 0;
				if ( runGlM( fr, queue, false, bare ) )
					for ( int t = 0; t < 2; t++ )
						for ( int l = 0; l < 3; l++ )
							for ( std::size_t i = 0; i < bare[t][l].size(); i++ )
								won += bare[t][l][i] != refM[1][t][l][i];
				std::printf( "SHADOWVIEW mesh won %zu texels against GL without the mesh casters (must be > 0)\n", won );
				meshOk = meshOk && won > 0;
			}
		}
		std::printf( "SHADOWVIEW mesh triggers: frame0 copies=%d (>= 4), frame1 regen=%d (>= 1), frame2 repacks=%d (>= 1)\n", copies0,
			regen1, repacks2 );
		meshOk = meshOk && copies0 >= 4 && regen1 >= 1 && repacks2 >= 1;
		// the reds on frame 2's content: each must FAIL past 1 unit (over all six layers)
		const struct
		{
			const char * name;
			MRed red;
		} mreds[4] = { { "whole (the cullIdx ranges drawn as the whole mesh)", kMWhole },
			{ "split (the mesh draws with the next cascade's clipFromView)", kMSplit }, { "nobias (the mesh draws' unit and slope 0)", kMNoBias },
			{ "cull (back faces drawn on the one-sided mesh draws)", kMNoCull } };
		for ( const auto & r : mreds ) {
			if ( !meshOk )
				break;
			const bool ran = runVkM( 2, true, r.red, vkM );
			Diff all;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ )
					all.add( compare( vkM[t][l], refM[2][t][l] ) );
			const bool redFails = !ran || all.past1 > 0;
			std::printf( "SHADOWVIEW mesh RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n",
				redFails ? "FAIL" : "TESTS NOTHING", r.name, all.diff, all.covered, all.past1, all.maxu );
			meshOk = meshOk && redFails;
		}
		const WwVkPresent::CsmStat ms = WwVkPresent::takeCsmStat();
		std::printf( "SHADOWVIEW mesh stat: %d runs on Vulkan, %d failed (%s), %d draws, %d mesh draws\n", ms.runs, ms.fails,
			ms.failWhy.c_str(), ms.draws, ms.mdraws );

		// ---- lane VKSHADOW5 (item 3 part 2 bar): the ground quad as a pooled mesh, the draws interleaved as the app emits them
		ilOk = meshOk;
		HMesh gq;  // sunshadow.cpp: { x0, y0, gz, x1, y0, gz, x0, y1, gz, x1, y1, gz }, indices { 0, 1, 2, 2, 1, 3 }
		auto quad = [&]( float gx, float gy, float half, float gz ) {
			gq.p = { gx - half, gy - half, gz, gx + half, gy - half, gz, gx - half, gy + half, gz, gx + half, gy + half, gz };
			gq.i = { 0, 1, 2, 2, 1, 3 };
			gq.gen = gq.gen ? gq.gen + 1 : 7000;
			f.bindVertexArray( vaoM );
			upMesh( gq );
		};
		// the two ground model-views: the front one first (counter-clockwise in cascade 0's clip space), the one from below second
		float gm[32];
		double gFront = 0.0, gBack = 0.0;
		{
			auto ndc = [&]( const double * M, double x, double y, double * o ) {
				double v[4], c[4];
				for ( int r = 0; r < 4; r++ )
					v[r] = M[r] * x + M[4 + r] * y + M[12 + r];
				for ( int r = 0; r < 4; r++ )
					c[r] = casc[0].clip[r] * v[0] + casc[0].clip[4 + r] * v[1] + casc[0].clip[8 + r] * v[2] + casc[0].clip[12 + r] * v[3];
				o[0] = c[0] / c[3];
				o[1] = c[1] / c[3];
			};
			auto area = [&]( const double * M ) {
				double a[2], b[2], c[2];
				ndc( M, -1.0, -1.0, a );
				ndc( M, 1.0, -1.0, b );
				ndc( M, -1.0, 1.0, c );
				return ( b[0] - a[0] ) * ( c[1] - a[1] ) - ( b[1] - a[1] ) * ( c[0] - a[0] );
			};
			double A[16], B[16];
			rotT( 1, 0, 0, 0.35, -6.0, 4.0, -135.0, 18.0, A );
			rotT( 1, 0, 0, 0.35 + 3.14159265358979, 9.0, -7.0, -135.0, 18.0, B );
			const bool swap = area( A ) < 0.0;
			const double * F = swap ? B : A, * K = swap ? A : B;
			gFront = area( F );
			gBack = area( K );
			for ( int k = 0; k < 16; k++ ) {
				gm[k] = float( F[k] );
				gm[16 + k] = float( K[k] );
			}
		}
		std::printf( "SHADOWVIEW ileave ground facing: front area %.4f (must be > 0), from below %.4f (must be < 0)\n", gFront, gBack );
		ilOk = ilOk && gFront > 0.0 && gBack < 0.0;
		auto glGround = [&]( const GLuint * tex, const MRec & R ) {
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
			f.uniformMatrix4fv( mClip, 1, GL_FALSE, c.clip );
			glEnable( GL_CULL_FACE );
			for ( int g = 0; g < 2; g++ ) {
				f.uniformMatrix4fv( mMv, 1, GL_FALSE, &gm[16 * g] );
				f.bindBuffer( kArrayBuf, gq.vb );
				f.enableVertexAttribArray( 0 );
				f.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
				f.bindBuffer( kElemBuf, gq.ib );
				glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
			}
			if ( sc )
				glDisable( GL_SCISSOR_TEST );
			return true;
		};
		const std::size_t nJ = std::max( recs.size(), mrecs.size() );
		// GL: per job its queue span, its mesh casters, then the ground in a culled job (sunshadow.cpp drawJob's order)
		auto runGlI = [&]( bool withGround, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( refT );
			glState();
			ok2 = ok2 && ground( refT );
			for ( std::size_t j = 0; j < nJ; j++ ) {
				if ( j < recs.size() ) {
					glState();
					ok2 = ok2 && glDraw( refT, recs[j] );
				}
				if ( j >= mrecs.size() )
					continue;
				f.useProgram( progM );
				f.bindVertexArray( vaoM );
				ok2 = ok2 && glMesh( refT, mrecs[j], 2 );
				if ( withGround && mrecs[j].cull )
					ok2 = ok2 && glGround( refT, mrecs[j] );
			}
			readAll( refT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		// Vulkan: one csmRun, the draws in the same job order (queue, then the job's mesh draws with the ground in the culled ones)
		auto runVkI = [&]( bool cullRed, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( shT );
			glState();
			ok2 = ok2 && ground( shT );
			WwVkPresent::CsmRun run;
			run.map = map;
			run.layers = 3;
			for ( int t = 0; t < 2; t++ ) {
				run.tex[t] = shT[t];
				run.key[t] = reinterpret_cast<std::uintptr_t>( &keyHome[t] );
			}
			std::vector<float> allM( mats );
			allM.insert( allM.end(), mm.begin(), mm.end() );
			allM.insert( allM.end(), gm, gm + 32 );
			run.mats = allM.data();
			run.nMats = allM.size() / 16;
			run.pos = bPos;
			run.idx = bIdx;
			run.posBytes = pos.size() * 4;
			run.idxBytes = idx.size() * 2;
			run.cmds = reinterpret_cast<const std::uint32_t *>( cmds.data() );
			run.nCmds = cmds.size();
			std::vector<WwVkPresent::CsmMesh> cm( hm.size() + 1 );
			for ( std::size_t m = 0; m < hm.size(); m++ ) {
				cm[m].key = reinterpret_cast<std::uintptr_t>( &hm[m] );
				cm[m].gen = hm[m].gen;
				cm[m].pos = hm[m].p.data();
				cm[m].verts = std::uint32_t( hm[m].p.size() / 3 );
				cm[m].idx = hm[m].i.data();
				cm[m].indices = std::uint32_t( hm[m].i.size() );
			}
			const std::uint32_t gMesh = std::uint32_t( hm.size() ), gMat = std::uint32_t( kCasters + kMC );
			cm[gMesh].key = reinterpret_cast<std::uintptr_t>( &gq );
			cm[gMesh].gen = gq.gen;
			cm[gMesh].pos = gq.p.data();
			cm[gMesh].verts = 4;
			cm[gMesh].idx = gq.i.data();
			cm[gMesh].indices = 6;
			auto put = [&]( int ci, int target, int layer, const int * scr, bool cull, std::uint32_t first, std::uint32_t n, bool mesh ) {
				WwVkPresent::CsmDraw d;
				d.target = target;
				d.layer = layer;
				d.vw = casc[ci].vw;
				d.vh = casc[ci].vh;
				std::memcpy( d.sc, scr, sizeof( d.sc ) );
				std::memcpy( d.clip, casc[ci].clip, sizeof( d.clip ) );
				d.unit = unitBias;
				d.slope = slopeBias;
				d.clamp = casc[ci].clamp;
				d.cull = cull;
				d.first = first;
				d.n = n;
				d.mesh = mesh;
				run.d.push_back( d );
			};
			std::vector<std::uint32_t> md;
			for ( std::size_t j = 0; j < nJ; j++ ) {
				if ( j < recs.size() ) {
					const Rec & R = recs[j];
					put( R.cascade, R.target, R.layer, R.sc, R.cull, R.first, R.n, false );
				}
				if ( j >= mrecs.size() )
					continue;
				const MRec & R = mrecs[j];
				std::uint32_t first = std::uint32_t( md.size() / 4 );
				for ( const auto & d : drawsOf( R, false, 2 ) ) {
					md.push_back( std::uint32_t( meshOfC[d[0]] ) );
					md.push_back( std::uint32_t( kCasters ) + d[0] );
					md.push_back( d[1] );
					md.push_back( d[2] );
				}
				if ( R.cull && !cullRed )
					for ( std::uint32_t g = 0; g < 2; g++ )
						md.insert( md.end(), { gMesh, gMat + g, 0u, 6u } );
				const std::uint32_t n = std::uint32_t( md.size() / 4 ) - first;
				if ( n )
					put( R.cascade, R.target, R.layer, R.sc, R.cull, first, n, true );
				if ( R.cull && cullRed ) {  // the red: the ground in a draw of its own with cull off
					first = std::uint32_t( md.size() / 4 );
					for ( std::uint32_t g = 0; g < 2; g++ )
						md.insert( md.end(), { gMesh, gMat + g, 0u, 6u } );
					put( R.cascade, R.target, R.layer, R.sc, false, first, 2, true );
				}
			}
			run.meshes = cm.data();
			run.nMeshes = cm.size();
			run.mdraws = md.data();
			run.nMDraws = md.size() / 4;
			std::string w;
			if ( !WwVkPresent::csmRun( run, w ) ) {
				std::printf( "SHADOWVIEW ileave csmRun refused: %s\n", w.c_str() );
				return false;
			}
			readAll( shT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		std::vector<std::uint16_t> refI[2][3], vkI[2][3], bareI[2][3];
		int requeues[2] = { 0, 0 }, regenI = 0;
		upload( moves[1] );
		setMM( 1.0 );
		for ( int fr = 0; fr < 2 && ilOk; fr++ ) {
			quad( fr ? 0.45f : 0.2f, fr ? 0.3f : -0.1f, fr ? 1.4f : 1.5f, fr ? 0.25f : 0.3f );
			if ( !runGlI( true, refI ) || !runVkI( false, vkI ) ) {
				std::printf( "SHADOWVIEW ileave frame%d FAIL (a run failed)\n", fr );
				ilOk = false;
				break;
			}
			const WwVkPresent::CsmStat ps = WwVkPresent::takeCsmStat();
			std::printf( "SHADOWVIEW ileave pool frame%d copies=%d hits=%d regen=%d repacks=%d mdraws=%d requeues=%d\n", fr, ps.copies,
				ps.hits, ps.regen, ps.repacks, ps.mdraws, ps.requeues );
			requeues[fr] = ps.requeues;
			regenI += fr == 1 ? ps.regen : 0;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ ) {
					const Diff v = compare( vkI[t][l], refI[t][l] );
					std::printf( "SHADOWVIEW ileave frame%d %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", fr,
						t ? "static" : "live", l, v.word(), v.diff, v.covered, v.past1, v.maxu );
					ilOk = ilOk && v.ok();
				}
			if ( fr == 0 ) {  // the ground must win texels, or the case proves nothing
				std::size_t won = 0;
				if ( runGlI( false, bareI ) )
					for ( int t = 0; t < 2; t++ )
						for ( int l = 0; l < 3; l++ )
							for ( std::size_t i = 0; i < bareI[t][l].size(); i++ )
								won += bareI[t][l][i] != refI[t][l][i];
				std::printf( "SHADOWVIEW ileave ground won %zu texels against GL without it (must be > 0)\n", won );
				ilOk = ilOk && won > 0;
			}
		}
		std::printf( "SHADOWVIEW ileave triggers: requeues frame0=%d frame1=%d (each >= 1), frame1 regen=%d (>= 1)\n", requeues[0],
			requeues[1], regenI );
		ilOk = ilOk && requeues[0] >= 1 && requeues[1] >= 1 && regenI >= 1;
		if ( ilOk ) {  // the red on frame 1's content: must FAIL past 1 unit (over all six layers)
			const bool ran = runVkI( true, vkI );
			Diff all;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ )
					all.add( compare( vkI[t][l], refI[t][l] ) );
			const bool redFails = !ran || all.past1 > 0;
			std::printf( "SHADOWVIEW ileave RED %s cull (the ground drawn without cull: the quad seen from below lands) (%zu of %zu "
				"covered texels differ, %zu past 1 unit, max %d; must FAIL)\n", redFails ? "FAIL" : "TESTS NOTHING", all.diff, all.covered,
				all.past1, all.maxu );
			ilOk = redFails;
			WwVkPresent::takeCsmStat();
		}
	}
	std::printf( "SHADOWVIEW mesh %s\n", meshOk ? "OK" : "FAILED" );
	std::printf( "SHADOWVIEW ileave %s\n", ilOk ? "OK" : "FAILED" );

	// ---- lane VKSHADOW6 (item 3 part 3 bar): the Scroll's remap in the crossing, see the header
	bool rmOk = true;
	{
		constexpr GLenum kReadFb = 0x8CA8, kDrawFb = 0x8CA9;
		const GLuint progR = program( readFile( root + "/res/shaders/features/shadows/lookdev_csmremap.vert" ),
			readFile( root + "/res/shaders/features/shadows/lookdev_csmremap.frag" ) );
		if ( !progR || !f.blitFramebuffer || !f.uniform1i || !f.uniform1f ) {
			std::printf( "SHADOWVIEW remap FAIL (the remap program or its entry points)\n" );
			rmOk = false;
		}
		// per cascade: the new window's offset into the old one, the old window's size, the depth map (zScale, zBias)
		const int rOff[3][2] = { { 37, -23 }, { -51, 19 }, { 12, 44 } };
		const int rSize[3][2] = { { casc[0].vw - 30, casc[0].vh - 17 }, { casc[1].vw + 9, casc[1].vh - 40 },
			{ casc[2].vw - 21, casc[2].vh + 6 } };
		float rZ[3][2] = { { 1.31f, -0.27f }, { 0.77f, 0.29f }, { 1.13f, -0.04f } };	// 0 and 2 reseeded below
		GLuint oldT = 0, scr1 = 0, fbo2 = 0, vaoR = 0, shS = 0;
		static unsigned keyScr;
		auto mkArr = [&]( GLuint & t, int layers ) {
			glGenTextures( 1, &t );
			glBindTexture( kTex2DArray, t );
			f.texImage3D( kTex2DArray, 0, GLint( kD16 ), map, map, layers, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr );
			glTexParameteri( kTex2DArray, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( kTex2DArray, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			glBindTexture( kTex2DArray, 0 );
		};
		if ( rmOk ) {
			mkArr( oldT, 3 );  // GL's old static layers (the reference's)
			mkArr( scr1, 1 );  // sunshadow.cpp's 1-layer GL scratch
			f.genFramebuffers( 1, &fbo2 );
			f.genVertexArrays( 1, &vaoR );
			if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &keyScr ), 2, map, map, 3, kD16, shS, why ) ) {
				std::printf( "SHADOWVIEW remap FAIL (the shared scratch: %s)\n", why.c_str() );
				rmOk = false;
			}
		}
		// one whole layer, depth bits (sunshadow.cpp copyFrom)
		auto blit = [&]( GLuint src, int sl, GLuint dst, int dl ) {
			f.bindFramebuffer( kReadFb, fbo2 );
			f.framebufferTextureLayer( kReadFb, kDepthAtt, src, 0, sl );
			glReadBuffer( GL_NONE );
			f.bindFramebuffer( kDrawFb, fbo );
			f.framebufferTextureLayer( kDrawFb, kDepthAtt, dst, 0, dl );
			glDrawBuffer( GL_NONE );
			glDisable( GL_SCISSOR_TEST );
			f.blitFramebuffer( 0, 0, map, map, 0, 0, map, map, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
			f.bindFramebuffer( kFb, fbo );
		};
		auto readL = [&]( GLuint tex, int l, std::vector<std::uint16_t> & o ) {
			o.assign( size_t( map ) * map, 0 );
			attach( tex, l );
			glReadPixels( 0, 0, map, map, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, o.data() );
		};
		// sunshadow.cpp remap( l ): the old layer from the 1-layer scratch, cull and bias off, the cascade's viewport; func
		// GL_ALWAYS (the app's Scroll on a fresh clear) or GL_LEQUAL (the refusal's late fallback over GL's casters)
		auto glRemap = [&]( const GLuint * tex, int t, int l, GLenum func ) {
			if ( !attach( tex[t], l ) )
				return false;
			f.useProgram( progR );
			f.uniform1i( f.getUniformLocation( progR, "srcMap" ), 0 );
			f.uniform1i( f.getUniformLocation( progR, "srcX" ), rOff[l][0] );
			f.uniform1i( f.getUniformLocation( progR, "srcY" ), rOff[l][1] );
			f.uniform1i( f.getUniformLocation( progR, "srcW" ), rSize[l][0] );
			f.uniform1i( f.getUniformLocation( progR, "srcH" ), rSize[l][1] );
			f.uniform1f( f.getUniformLocation( progR, "zScale" ), rZ[l][0] );
			f.uniform1f( f.getUniformLocation( progR, "zBias" ), rZ[l][1] );
			glBindTexture( kTex2DArray, scr1 );
			f.bindVertexArray( vaoR );
			glDisable( GL_CULL_FACE );
			glDisable( kPolyFill );
			glDepthFunc( func );
			glViewport( 0, 0, casc[l].vw, casc[l].vh );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			glBindTexture( kTex2DArray, 0 );
			glState();  // the casters' state again (LEQUAL, offset on, their program and VAO)
			return true;
		};
		auto gnd = [&]( const GLuint * tex, int l ) { return glDraw( tex, Rec { 1, l, l, { 0, 0, 0, 0 }, false, std::uint32_t( kQueued ), 2 } ); };
		// GL, the app's Scroll per layer: static = clear, scratch <- old, remap ALWAYS, GL's casters, set 1; live = static copied
		// whole, set 2. withRemap false: the plain clear; withGround false: no GL casters
		auto runGlR = [&]( bool withRemap, bool withGround, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( refT );
			glState();
			for ( int l = 0; l < 3; l++ ) {
				if ( withRemap ) {
					blit( oldT, l, scr1, 0 );
					ok2 = ok2 && glRemap( refT, 1, l, GL_ALWAYS );
				}
				if ( withGround )
					ok2 = ok2 && gnd( refT, l );
				for ( const Rec & R : recs )
					if ( R.target == 1 && R.layer == l )
						ok2 = ok2 && glDraw( refT, R );
				blit( refT[1], l, refT[0], l );
				for ( const Rec & R : recs )
					if ( R.target == 0 && R.layer == l )
						ok2 = ok2 && glDraw( refT, R );
			}
			readAll( refT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		enum RRed { rNone, rPlain, rWrong, rStatic };
		// the route: GL keeps its casters and the whole copy (no remap on GL); Vulkan draws the remaps (static and live) and the
		// records in one csmRun. lateGl: the refusal path instead (GL remaps LEQUAL after its casters, then the records)
		auto runVkR = [&]( RRed red, bool lateGl, std::vector<std::uint16_t> out[2][3] ) {
			bool ok2 = clearAll( shT );
			glState();
			for ( int l = 0; l < 3; l++ ) {
				ok2 = ok2 && gnd( shT, l );
				blit( shT[1], l, shT[0], l );
			}
			if ( lateGl ) {
				for ( int l = 0; l < 3; l++ ) {
					blit( shS, l, scr1, 0 );
					ok2 = ok2 && glRemap( shT, 1, l, GL_LEQUAL ) && glRemap( shT, 0, l, GL_LEQUAL );
				}
				for ( const Rec & R : recs )
					ok2 = ok2 && glDraw( shT, R );
				readAll( shT, out );
				return ok2 && glGetError() == GL_NO_ERROR;
			}
			WwVkPresent::CsmRun run;
			run.map = map;
			run.layers = 3;
			for ( int t = 0; t < 2; t++ ) {
				run.tex[t] = shT[t];
				run.key[t] = reinterpret_cast<std::uintptr_t>( &keyHome[t] );
			}
			run.scratch = shS;
			run.scratchKey = reinterpret_cast<std::uintptr_t>( &keyScr );
			run.pos = bPos;
			run.idx = bIdx;
			run.posBytes = pos.size() * 4;
			run.idxBytes = idx.size() * 2;
			run.mats = mats.data();
			run.nMats = std::size_t( kCasters );
			run.cmds = reinterpret_cast<const std::uint32_t *>( cmds.data() );
			run.nCmds = cmds.size();
			for ( int l = 0; l < 3; l++ )  // the app's order: per layer the remap first, static then live
				for ( int t = 1; t >= 0; t-- ) {
					if ( red == rPlain || ( red == rStatic && t == 0 ) )
						continue;
					WwVkPresent::CsmDraw d;
					d.target = t;
					d.layer = l;
					d.vw = casc[l].vw;
					d.vh = casc[l].vh;
					d.cull = false;
					d.remap = true;
					d.src = red == rWrong ? ( l + 1 ) % 3 : l;
					d.srcX = rOff[l][0];
					d.srcY = rOff[l][1];
					d.srcW = rSize[l][0];
					d.srcH = rSize[l][1];
					d.zScale = rZ[l][0];
					d.zBias = rZ[l][1];
					run.d.push_back( d );
				}
			for ( const Rec & R : recs ) {
				WwVkPresent::CsmDraw d;
				const Casc & c = casc[R.cascade];
				d.target = R.target;
				d.layer = R.layer;
				d.vw = c.vw;
				d.vh = c.vh;
				std::memcpy( d.sc, R.sc, sizeof( d.sc ) );
				std::memcpy( d.clip, c.clip, sizeof( d.clip ) );
				d.unit = unitBias;
				d.slope = slopeBias;
				d.clamp = c.clamp;
				d.cull = R.cull;
				d.first = R.first;
				d.n = R.n;
				run.d.push_back( d );
			}
			std::string w;
			if ( !WwVkPresent::csmRun( run, w ) ) {
				std::printf( "SHADOWVIEW remap csmRun refused: %s\n", w.c_str() );
				return false;
			}
			readAll( shT, out );
			return ok2 && glGetError() == GL_NO_ERROR;
		};
		std::vector<std::uint16_t> refR[2][3], vkR[2][3], cmpR[2][3], oldR[3], scrR[3];
		// the old windows: frame 0 of the base case on each arm, its live layers into the old layers (GL's) and the scratch (shared)
		upload( moves[0] );
		if ( rmOk && ( !runGl( refR ) || !runVk( kNone, vkR ) ) ) {
			std::printf( "SHADOWVIEW remap FAIL (the old windows' runs)\n" );
			rmOk = false;
		}
		WwVkPresent::takeCsmStat();
		std::size_t inDiff = 0, inCov = 0;
		for ( int l = 0; l < 3 && rmOk; l++ ) {
			blit( refT[0], l, oldT, l );
			blit( shT[0], l, shS, l );
			readL( oldT, l, oldR[l] );
			readL( shS, l, scrR[l] );
			for ( std::size_t i = 0; i < oldR[l].size(); i++ ) {
				inDiff += oldR[l][i] != scrR[l][i];
				inCov += oldR[l][i] != 0xFFFF;
			}
		}
		std::printf( "SHADOWVIEW remap inputs: the old layers on both arms %s (%zu of %zu covered texels differ)\n",
			inDiff == 0 && inCov > 0 ? "byte-equal" : "DIFFER", inDiff, inCov );
		rmOk = rmOk && inDiff == 0 && inCov > 0;
		// seeding: cascades 0 and 2 map their old layer's 25th / 75th covered depth percentile to 0 / 1
		for ( int l : { 0, 2 } ) {
			if ( !rmOk )
				break;
			std::vector<std::uint16_t> cov;
			for ( std::uint16_t o : oldR[l] )
				if ( o != 0xFFFF )
					cov.push_back( o );
			std::sort( cov.begin(), cov.end() );
			const double p25 = cov.empty() ? 0.0 : cov[cov.size() / 4] / 65535.0;
			const double p75 = cov.empty() ? 1.0 : cov[cov.size() * 3 / 4] / 65535.0;
			if ( p75 > p25 ) {
				rZ[l][0] = float( 1.0 / ( p75 - p25 ) );
				rZ[l][1] = float( -p25 / ( p75 - p25 ) );
			}
			std::printf( "SHADOWVIEW remap seed %d: zScale %.6g zBias %.6g (old depth p25 %.6f p75 %.6f)\n", l, rZ[l][0], rZ[l][1],
				p25, p75 );
		}
		// the remap's branches over the three new viewports (CPU, from GL's old layers): outside the old window (each side), o == 1,
		// o * zScale + zBias below 0, above 1, in range
		std::size_t bOut = 0, bOne = 0, bLo = 0, bHi = 0, bMid = 0, side[4] = {};
		for ( int l = 0; l < 3 && rmOk; l++ )
			for ( int y = 0; y < casc[l].vh; y++ )
				for ( int x = 0; x < casc[l].vw; x++ ) {
					const int px = x + rOff[l][0], py = y + rOff[l][1];
					if ( px < 0 || py < 0 || px >= rSize[l][0] || py >= rSize[l][1] ) {
						bOut++;
						side[0] += px < 0;
						side[1] += px >= rSize[l][0];
						side[2] += py < 0;
						side[3] += py >= rSize[l][1];
						continue;
					}
					const std::uint16_t o = oldR[l][size_t( py ) * map + size_t( px )];
					if ( o == 0xFFFF ) {
						bOne++;
						continue;
					}
					const double v = double( o ) / 65535.0 * rZ[l][0] + rZ[l][1];
					bLo += v < 0.0;
					bHi += v > 1.0;
					bMid += v >= 0.0 && v <= 1.0;
				}
		std::printf( "SHADOWVIEW remap branches: outside %zu (left %zu right %zu bottom %zu top %zu), o == 1 %zu, clamped to 0 %zu, "
			"clamped to 1 %zu, in range %zu (each > 0)\n", bOut, side[0], side[1], side[2], side[3], bOne, bLo, bHi, bMid );
		rmOk = rmOk && side[0] > 0 && side[1] > 0 && side[2] > 0 && side[3] > 0 && bOne > 0 && bLo > 0 && bHi > 0 && bMid > 0;
		// the new windows: frame 1's matrices
		upload( moves[1] );
		auto judge = [&]( const char * tag, std::vector<std::uint16_t> got[2][3] ) {
			bool all = true;
			for ( int t = 0; t < 2; t++ )
				for ( int l = 0; l < 3; l++ ) {
					const Diff v = compare( got[t][l], refR[t][l] );
					std::printf( "SHADOWVIEW remap %s %s/%d %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d)\n", tag,
						t ? "static" : "live", l, v.word(), v.diff, v.covered, v.past1, v.maxu );
					all = all && v.ok();
				}
			return all;
		};
		if ( rmOk && ( !runGlR( true, true, refR ) || !runVkR( rNone, false, vkR ) ) ) {
			std::printf( "SHADOWVIEW remap frame1 FAIL (a run failed)\n" );
			rmOk = false;
		}
		if ( rmOk ) {
			rmOk = judge( "frame1", vkR );
			const WwVkPresent::CsmStat rs = WwVkPresent::takeCsmStat();
			std::printf( "SHADOWVIEW remap stat: %d runs on Vulkan, %d failed (%s), %d draws, remaps=%d remapRed=%d\n", rs.runs, rs.fails,
				rs.failWhy.c_str(), rs.draws, rs.remaps, rs.remapRed );
			rmOk = rmOk && rs.remaps == 6;
			const bool lateRan = runVkR( rNone, true, vkR );
			rmOk = judge( "late", vkR ) && lateRan && rmOk;
			// the remap must win texels (against GL without it), GL's casters must beat it on some (against GL without them)
			std::size_t won = 0, beat = 0;
			if ( runGlR( false, true, cmpR ) )
				for ( int t = 0; t < 2; t++ )
					for ( int l = 0; l < 3; l++ )
						for ( std::size_t i = 0; i < cmpR[t][l].size(); i++ )
							won += cmpR[t][l][i] != refR[t][l][i];
			if ( runGlR( true, false, cmpR ) )
				for ( int t = 0; t < 2; t++ )
					for ( int l = 0; l < 3; l++ )
						for ( std::size_t i = 0; i < cmpR[t][l].size(); i++ )
							beat += cmpR[t][l][i] != refR[t][l][i];
			std::printf( "SHADOWVIEW remap won %zu texels against GL without it, GL's casters beat it on %zu (each > 0)\n", won, beat );
			rmOk = rmOk && won > 0 && beat > 0;
			const struct
			{
				const char * name;
				RRed red;
				bool liveOnly;
			} rr[3] = { { "plain (no remap draws: the plain clear)", rPlain, false },
				{ "wrong (the scratch layer of the next cascade)", rWrong, false },
				{ "static (the remap recorded into the static layers only: the live layers)", rStatic, true } };
			for ( const auto & r : rr ) {
				const bool ran = runVkR( r.red, false, vkR );
				Diff all;
				for ( int t = 0; t < 2; t++ )
					for ( int l = 0; l < 3; l++ )
						if ( !r.liveOnly || t == 0 )
							all.add( compare( vkR[t][l], refR[t][l] ) );
				const bool redFails = !ran || all.past1 > 0;
				std::printf( "SHADOWVIEW remap RED %s %s (%zu of %zu covered texels differ, %zu past 1 unit, max %d; must FAIL)\n",
					redFails ? "FAIL" : "TESTS NOTHING", r.name, all.diff, all.covered, all.past1, all.maxu );
				rmOk = rmOk && redFails;
			}
			WwVkPresent::takeCsmStat();
		}
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &keyScr ) );
	}
	std::printf( "SHADOWVIEW remap %s\n", rmOk ? "OK" : "FAILED" );
#include "shadowview_alpha.inc"
#include "shadowview_sway.inc"
	ok = ok && meshOk && ilOk && rmOk && atOk && swOk;
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
