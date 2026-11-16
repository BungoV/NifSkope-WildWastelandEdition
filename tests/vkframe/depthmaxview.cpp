// Lane VKSMALL1 (2026-11-16): the frame's farthest depth per pixel on the present device (vkdepthmax.inc depthMaxRun: the
// tone_view_vs triangle + depthmax_view_ps, the shared tone frame's depth read in place, into the route's shared R32F twin)
// against GL's own program (cell_hdr.vert as the app links it, written by waterdraw_prep.py, + cell_depthmax.frag) drawn
// with cellhdr.cpp depthMaxMake's state from the same 4x MSAA D24S8 depth. fxview.cpp's helpers are reused (its main renamed).
// Scene (underdraw.cpp's, per sample): a floor whose depth ramps up the frame, a sky band (depth 1.0) and six nearer discs,
// so edge pixels hold samples of two depths.
// Bar (set before the first run, never widened): EXACT, every R32F texel of the twin bit-equal to GL's target, on two
// scenes (seed 0, then seed 1 into the same twin). Floor: GL's own max over 4 samples differs from its sample 0 alone on
// >= 100 pixels (the reds can bite).
// Reds (each must FAIL, never refuse): 1 depthSamples 1 (sample 0 alone), 2 the twin cleared, no draw.
// Refusal arm: a multisample depth that is not the tone frame's must refuse.
// Stat: 4 runs, 1 refusal. Last line `DEPTHMAXVIEW GATE OK` or `DEPTHMAXVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <array>

#include "../../src/features/vkframe/vkdepthmax.h"

namespace dmv
{
using namespace fv;

struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
};
Ex ex;

bool loadMore()
{
	ex.genVertexArrays = reinterpret_cast<decltype( ex.genVertexArrays )>( glProc( "glGenVertexArrays" ) );
	ex.bindVertexArray = reinterpret_cast<decltype( ex.bindVertexArray )>( glProc( "glBindVertexArray" ) );
	ex.vertexAttribPointer = reinterpret_cast<decltype( ex.vertexAttribPointer )>( glProc( "glVertexAttribPointer" ) );
	ex.enableVertexAttribArray = reinterpret_cast<decltype( ex.enableVertexAttribArray )>( glProc( "glEnableVertexAttribArray" ) );
	return ex.genVertexArrays && ex.bindVertexArray && ex.vertexAttribPointer && ex.enableVertexAttribArray;
}

constexpr GLenum kDsMode = 0x90EA, kDepthComp = 0x1902, kStencilIdx = 0x1901, kArrayBuf = 0x8892, kElemBuf = 0x8893;

const char * kSceneFs = "#version 410 core\n"
	"uniform vec2 sz; uniform int seed; out vec4 o;\n"
	"void main() {\n"
	"  vec2 p = gl_FragCoord.xy - 0.5 + gl_SamplePosition;\n"
	"  float d = 0.985 + 0.0145 * ( p.y / sz.y );\n"
	"  if ( p.y > sz.y * 0.82 ) d = 1.0;\n"
	"  for ( int i = 0; i < 6; i++ ) {\n"
	"    float k = float( i + 7 * seed );\n"
	"    vec2 ctr = vec2( fract( 0.137 * k + 0.11 ) * sz.x, ( 0.12 + 0.62 * fract( 0.291 * k + 0.07 ) ) * sz.y );\n"
	"    float rad = ( 0.06 + 0.06 * fract( 0.53 * k + 0.2 ) ) * sz.x;\n"
	"    float di = 0.93 + 0.009 * float( i ) + 0.004 * ( p.x / sz.x );\n"
	"    if ( distance( p, ctr ) < rad && di < d ) d = di;\n"
	"  }\n"
	"  gl_FragDepth = d; o = vec4( d, 0.0, 0.0, 1.0 );\n"
	"}\n";
}  // namespace dmv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace dmv;
	gTag = "DEPTHMAXVIEW";
	if ( argc < 3 ) {
		std::printf( "usage: depthmaxview <prep dir> <tree>\nDEPTHMAXVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1], tree = argv[2];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "DEPTHMAXVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 200, H = 120;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwdepthmaxview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwdepthmaxview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwdepthmaxview", "depth max harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40,
		40, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf )
		return fatal( "no 4x MSAA pixel format" );
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
	if ( !loadAll() || !loadMore() )
		return fatal( "GL entry points" );
	std::printf( "DEPTHMAXVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	// the app's program: cell_hdr.vert (prepped) + cell_depthmax.frag (no includes)
	const GLuint pMax = program( readFile( dir + "/uw/prog.vert.glsl" ), readFile( tree + "/res/shaders/features/exposure/cell_depthmax.frag" ),
		"cell_depthmax" );
	const GLuint pScene = program( kTriVs, kSceneFs, "scene" );
	if ( !pMax || !pScene )
		return fatal( "GL programs" );
	bool gateOk = true;

	// the app's quad (drawShape: 4 vertices, 6 indices) on attribute 0
	GLuint vao = 0, vbo = 0, ebo = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	f.genBuffers( 1, &vbo );
	f.bindBuffer( kArrayBuf, vbo );
	f.bufferData( kArrayBuf, sizeof( quad ), quad, fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	ex.enableVertexAttribArray( 0 );
	f.genBuffers( 1, &ebo );
	f.bindBuffer( kElemBuf, ebo );
	f.bufferData( kElemBuf, sizeof( idx ), idx, fStatic );

	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint otherCol = msTex( W, H, fRgba16f, S ), otherDs = msTex( W, H, fD24s8, S );
	const GLuint fbOther = fbo( fT2DMS, otherCol, 0, otherDs );
	const GLuint maxTex = tex2d( W, H, fR32f, fRed, GL_FLOAT, nullptr, false );
	const GLuint fbMax = fbo( GL_TEXTURE_2D, maxTex, 0, 0 );
	if ( !fbFrame || !fbOther || !fbMax )
		return fatal( "GL targets" );
	auto drawScene = [&]( GLuint fb, int seed ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.5f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		f.useProgram( pScene );
		f.uniform2fv( f.getUniformLocation( pScene, "sz" ), 1, std::array<float, 2> { float( W ), float( H ) }.data() );
		u1i( pScene, "seed", seed );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glDepthFunc( GL_LESS );
		drawState0();
	};
	auto readR32 = [&]( GLuint t ) {
		std::vector<float> px( size_t( W ) * H, -7.0f );
		glBindTexture( GL_TEXTURE_2D, t );
		glGetTexImage( GL_TEXTURE_2D, 0, fRed, GL_FLOAT, px.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		return px;
	};
	// cellhdr.cpp depthMaxMake on GL: the farthest of `samples` samples per pixel into maxTex
	auto glMax = [&]( GLuint ds, int samples ) {
		f.bindFramebuffer( fFb, fbMax );
		glViewport( 0, 0, W, H );
		drawState0();
		f.activeTexture( fTex0 + 3 );
		glBindTexture( fT2DMS, ds );
		glTexParameteri( fT2DMS, kDsMode, kDepthComp );
		f.useProgram( pMax );
		u1i( pMax, "depthMs", 3 );
		u1i( pMax, "depthSamples", samples );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		glTexParameteri( fT2DMS, kDsMode, kStencilIdx );
		glBindTexture( fT2DMS, 0 );
		f.activeTexture( fTex0 );
		return readR32( maxTex );
	};
	auto diffs = [&]( const std::vector<float> & a, const std::vector<float> & b ) {
		long n = 0;
		for ( size_t i = 0; i < a.size(); i++ )
			n += std::memcmp( &a[i], &b[i], 4 ) != 0;
		return n;
	};
	unsigned lastTwin = 0;
	auto vkMax = [&]( GLuint ds, int red, std::vector<float> & out, std::string & w ) {
		WwVkPresent::DepthMaxRun r;
		r.w = W;
		r.h = H;
		r.ds = ds;
		r.samples = S;
		r.red = red;
		unsigned t = 0;
		const bool ran = WwVkPresent::depthMaxRun( r, t, w );
		if ( ran ) {
			out = readR32( t );
			lastTwin = t;
		}
		return ran;
	};

	for ( int seed = 0; seed < 2; seed++ ) {
		drawScene( fbFrame, seed );
		const std::vector<float> gl1 = glMax( fds, 1 ), gl = glMax( fds, S );
		const long floorN = diffs( gl, gl1 );
		const bool floorOk = floorN >= 100;
		std::printf( "DEPTHMAXVIEW seed %d floor: GL's max of %d samples differs from sample 0 alone on %ld pixels (want >= 100): %s\n",
			seed, S, floorN, floorOk ? "OK" : "FAIL" );
		gateOk = gateOk && floorOk;
		std::vector<float> vk;
		std::string w;
		const bool ran = vkMax( fds, 0, vk, w );
		const long d = ran ? diffs( vk, gl ) : -1;
		std::printf( "DEPTHMAXVIEW seed %d green: %s%s, %ld of %d texels differ: %s\n", seed, ran ? "ran" : "refused: ",
			ran ? "" : w.c_str(), d, W * H, ran && d == 0 ? "EXACT" : "FAIL" );
		gateOk = gateOk && ran && d == 0;
		if ( seed == 1 ) {
			for ( int red : { 1, 2 } ) {
				std::vector<float> rv;
				std::string rw;
				const bool rran = vkMax( fds, red, rv, rw );
				const long rd = rran ? diffs( rv, gl ) : -1;
				const bool failed = rran && rd > 0;
				std::printf( "DEPTHMAXVIEW red %d (%s): %s%s, %ld texels differ, want FAIL: %s\n", red,
					red == 1 ? "depthSamples 1" : "the twin cleared, no draw", rran ? "ran" : "refused: ", rran ? "" : rw.c_str(), rd,
					failed ? "FAIL (OK)" : "TESTS NOTHING" );
				gateOk = gateOk && failed;
			}
		}
	}
	{
		drawScene( fbOther, 0 );
		std::vector<float> v;
		std::string w;
		const bool ran = vkMax( otherDs, 0, v, w );
		std::printf( "DEPTHMAXVIEW refusal arm (a depth that is not the tone frame's): %s%s, want refused: %s\n", ran ? "ran" : "refused: ",
			ran ? "" : w.c_str(), ran ? "FAIL" : "OK" );
		gateOk = gateOk && !ran;
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeDepthMaxStat();
	const bool statOk = st.runs == 4 && st.fails == 1 && lastTwin != 0;
	std::printf( "DEPTHMAXVIEW stat: %d runs (want 4), %d refused (want 1), %d pipelines in %.1f ms, twin %u: %s\n", st.runs, st.fails,
		st.pipelines, st.buildMs, lastTwin, statOk ? "OK" : "FAIL" );
	gateOk = gateOk && statOk;
	WwVkPresent::toneFrameDrop();
	std::printf( gateOk ? "DEPTHMAXVIEW GATE OK\n" : "DEPTHMAXVIEW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
