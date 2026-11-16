// Lane VKSMALL3 (2026-11-16, census row 76): the next frame's shading rate images on the present device (vkvrs.inc vrsRun:
// the view's MS RGBA8 colour resolved on Vulkan, then the tone_view_vs triangle + vrs_rate_view_ps into two shared R8UI
// twins) against GL's own program (cell_vrs_rate.prog: res/shaders/cell_ao.vert + cell_vrs_rate.frag) drawn with
// cellvrs.cpp wwCellVrsBuild's steps (blit the view into an RGBA8 texture, NEAREST; a tw x th viewport, one quad) from the
// same 4x MSAA view. fxview.cpp's helpers are reused (its main renamed).
// Scene (per sample): a smooth band (4x4 tiles), a band ramping 3 levels a pixel across (2x4), a 2-pixel checker (1x2) and
// four discs whose edges hold samples of two colours (1x1), so the resolve and every branch of pick() count.
// Bar (set before the first run, never widened): EXACT, every R8UI texel of both twins byte-equal to GL's targets, on two
// scenes (seed 0, then seed 1 into the same twins) and a motion arm (vrsMvOn with a GL RG32F vector texture).
// Floors: GL's opaque image holds >= 3 rate values; GL's motion arm differs from its still arm on >= 4 tiles.
// Reds (each must FAIL, never refuse): 1 vrsMax 2 (no 4x), 2 the twins cleared, no draw.
// Refusal arm: a draw framebuffer that is not the view must refuse.
// Stat: 5 runs, 1 refusal. Last line `VRSVIEW GATE OK` or `VRSVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <array>
#include <set>

#include "../../src/features/vkframe/vkvrs.h"

namespace vrv
{
using namespace fv;

struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * blitFramebuffer )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
};
Ex ex;

bool loadMore()
{
	ex.genVertexArrays = reinterpret_cast<decltype( ex.genVertexArrays )>( glProc( "glGenVertexArrays" ) );
	ex.bindVertexArray = reinterpret_cast<decltype( ex.bindVertexArray )>( glProc( "glBindVertexArray" ) );
	ex.vertexAttribPointer = reinterpret_cast<decltype( ex.vertexAttribPointer )>( glProc( "glVertexAttribPointer" ) );
	ex.enableVertexAttribArray = reinterpret_cast<decltype( ex.enableVertexAttribArray )>( glProc( "glEnableVertexAttribArray" ) );
	ex.blitFramebuffer = reinterpret_cast<decltype( ex.blitFramebuffer )>( glProc( "glBlitFramebuffer" ) );
	return ex.genVertexArrays && ex.bindVertexArray && ex.vertexAttribPointer && ex.enableVertexAttribArray && ex.blitFramebuffer;
}

constexpr GLenum kArrayBuf = 0x8892, kElemBuf = 0x8893, kR8ui = 0x8232, kRedInt = 0x8D94;

const char * kSceneFs = "#version 410 core\n"
	"uniform vec2 sz; uniform int seed; out vec4 o;\n"
	"void main() {\n"
	"  vec2 p = gl_FragCoord.xy - 0.5 + gl_SamplePosition;\n"
	"  float s = float( seed );\n"
	"  vec3 c;\n"
	"  if ( p.x < 64.0 ) c = vec3( 0.2 + 0.003 * p.x + 0.02 * s, 0.3 + 0.002 * p.y, 0.5 );\n"
	"  else if ( p.x < 128.0 ) c = vec3( 0.1 + 0.012 * ( p.x - 64.0 ), 0.4 + 0.002 * p.y, 0.3 + 0.03 * s );\n"
	"  else c = vec3( mod( floor( p.x * 0.5 ), 2.0 ) * 0.6, 0.2 + 0.002 * p.y, 0.4 );\n"
	"  for ( int i = 0; i < 4; i++ ) {\n"
	"    float k = float( i + 5 * seed );\n"
	"    vec2 ctr = vec2( fract( 0.237 * k + 0.13 ) * sz.x, ( 0.15 + 0.7 * fract( 0.391 * k + 0.07 ) ) * sz.y );\n"
	"    if ( distance( p, ctr ) < 9.0 + 3.0 * float( i ) ) c = vec3( 0.9, 0.1 + 0.05 * float( i ), 0.2 );\n"
	"  }\n"
	"  o = vec4( c, 1.0 );\n"
	"}\n";
}  // namespace vrv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace vrv;
	gTag = "VRSVIEW";
	if ( argc < 3 ) {
		std::printf( "usage: vrsview <out dir> <tree>\nVRSVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string tree = argv[2];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "VRSVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 200, H = 120, TX = 16, TY = 16, TW = ( W + TX - 1 ) / TX, TH = ( H + TY - 1 ) / TY;
	const float kCut = 0.025f, kCutSorted = 0.0125f, kMotion = 0.25f;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwvrsview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwvrsview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwvrsview", "shading rate harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40,
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
	std::printf( "VRSVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	// the app's program: cell_ao.vert + cell_vrs_rate.frag (cell_vrs_rate.prog; no includes)
	const GLuint pRate = program( readFile( tree + "/res/shaders/cell_ao.vert" ), readFile( tree + "/res/shaders/features/near/cell_vrs_rate.frag" ),
		"cell_vrs_rate" );
	const GLuint pScene = program( kTriVs, kSceneFs, "scene" );
	if ( !pRate || !pScene )
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
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	const GLuint fbView = WwVkPresent::viewFbo();
	if ( !fbView )
		return fatal( "the view has no FBO" );
	const GLuint otherCol = msTex( W, H, fRgba8, S );
	const GLuint fbOther = fbo( fT2DMS, otherCol, 0, 0 );
	const GLuint srcTex = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	const GLuint fbSrc = fbo( GL_TEXTURE_2D, srcTex, 0, 0 );
	const GLuint rate0 = tex2d( TW, TH, kR8ui, kRedInt, GL_UNSIGNED_BYTE, nullptr, false );
	const GLuint rate1 = tex2d( TW, TH, kR8ui, kRedInt, GL_UNSIGNED_BYTE, nullptr, false );
	const GLuint fbRate = fbo( GL_TEXTURE_2D, rate0, rate1, 0 );
	// the TAA's vectors (RG32F, texture units): a left half moving 6 pixels a frame, the rest still
	std::vector<float> mvPx( size_t( W ) * H * 2, 0.0f );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W / 2; x++ )
			mvPx[( size_t( y ) * W + x ) * 2] = 6.0f / float( W );
	const GLuint mvTex = tex2d( W, H, fRg32f, fRg, GL_FLOAT, mvPx.data(), false );
	if ( !fbOther || !fbSrc || !fbRate || !mvTex )
		return fatal( "GL targets" );
	auto drawScene = [&]( GLuint fb, int seed ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );
		f.useProgram( pScene );
		f.uniform2fv( f.getUniformLocation( pScene, "sz" ), 1, std::array<float, 2> { float( W ), float( H ) }.data() );
		u1i( pScene, "seed", seed );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
	};
	auto readRates = [&]( GLuint t0, GLuint t1 ) {
		std::vector<std::uint8_t> px( size_t( TW ) * TH * 2, 0xEE );
		glBindTexture( GL_TEXTURE_2D, t0 );
		glGetTexImage( GL_TEXTURE_2D, 0, kRedInt, GL_UNSIGNED_BYTE, px.data() );
		glBindTexture( GL_TEXTURE_2D, t1 );
		glGetTexImage( GL_TEXTURE_2D, 0, kRedInt, GL_UNSIGNED_BYTE, px.data() + size_t( TW ) * TH );
		glBindTexture( GL_TEXTURE_2D, 0 );
		return px;
	};
	auto u1f = [&]( const char * n, float v ) { f.uniform1fv( f.getUniformLocation( pRate, n ), 1, &v ); };
	auto u2f = [&]( const char * n, float a, float b ) {
		f.uniform2fv( f.getUniformLocation( pRate, n ), 1, std::array<float, 2> { a, b }.data() );
	};
	// cellvrs.cpp wwCellVrsBuild on GL: step 1 the blit, step 2 the rate quad into both targets
	auto glRates = [&]( GLuint mv ) {
		f.bindFramebuffer( kReadFb, fbView );
		f.bindFramebuffer( kDrawFb, fbSrc );
		ex.blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		f.bindFramebuffer( fFb, fbRate );
		glViewport( 0, 0, TW, TH );
		drawState0();
		f.useProgram( pRate );
		bindUnit( 0, GL_TEXTURE_2D, srcTex );
		bindUnit( 1, GL_TEXTURE_2D, mv ? mv : srcTex );
		u1i( pRate, "vrsSrc", 0 );
		u1i( pRate, "vrsMv", 1 );
		u1i( pRate, "vrsMvOn", mv ? 1 : 0 );
		u2f( "vrsSize", float( W ), float( H ) );
		u2f( "vrsTile", float( TX ), float( TY ) );
		u1f( "vrsCut", kCut );
		u1f( "vrsCutSorted", kCutSorted );
		u1i( pRate, "vrsMax", 4 );
		u1f( "vrsMotion", kMotion );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		bindUnit( 0, GL_TEXTURE_2D, 0 );
		bindUnit( 1, GL_TEXTURE_2D, 0 );
		glViewport( 0, 0, W, H );
		return readRates( rate0, rate1 );
	};
	auto diffs = [&]( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b ) {
		long n = 0;
		for ( size_t i = 0; i < a.size(); i++ )
			n += a[i] != b[i];
		return n;
	};
	unsigned lastTwin[2] = {};
	auto vkRates = [&]( GLuint fb, GLuint mv, int red, std::vector<std::uint8_t> & out, std::string & w ) {
		WwVkPresent::VrsRun r;
		r.w = W;
		r.h = H;
		r.drawFb = fb;
		r.tw = TW;
		r.th = TH;
		r.texelW = TX;
		r.texelH = TY;
		r.mv = mv;
		r.cut = kCut;
		r.cutSorted = kCutSorted;
		r.motion = kMotion;
		r.maxRate = 4;
		r.red = red;
		unsigned t[2] = {};
		const bool ran = WwVkPresent::vrsRun( r, t, w );
		if ( ran ) {
			out = readRates( t[0], t[1] );
			lastTwin[0] = t[0];
			lastTwin[1] = t[1];
		}
		return ran;
	};
	const long all = long( TW ) * TH * 2;

	for ( int seed = 0; seed < 2; seed++ ) {
		drawScene( fbView, seed );
		const std::vector<std::uint8_t> gl = glRates( 0 );
		std::set<int> vals( gl.begin(), gl.begin() + TW * TH );
		const bool floorOk = vals.size() >= 3;
		std::printf( "VRSVIEW seed %d floor: GL's opaque image holds %d rate values (want >= 3): %s\n", seed, int( vals.size() ),
			floorOk ? "OK" : "FAIL" );
		gateOk = gateOk && floorOk;
		std::vector<std::uint8_t> vk;
		std::string w;
		const bool ran = vkRates( fbView, 0, 0, vk, w );
		const long d = ran ? diffs( vk, gl ) : -1;
		std::printf( "VRSVIEW seed %d green: %s%s, %ld of %ld texels differ: %s\n", seed, ran ? "ran" : "refused: ", ran ? "" : w.c_str(), d,
			all, ran && d == 0 ? "EXACT" : "FAIL" );
		gateOk = gateOk && ran && d == 0;
		if ( seed == 1 ) {
			const std::vector<std::uint8_t> glMv = glRates( mvTex );
			const long moved = diffs( glMv, gl );
			const bool mvFloor = moved >= 4;
			std::printf( "VRSVIEW motion floor: GL's motion arm differs from its still arm on %ld tiles (want >= 4): %s\n", moved,
				mvFloor ? "OK" : "FAIL" );
			gateOk = gateOk && mvFloor;
			std::vector<std::uint8_t> vm;
			std::string mw;
			const bool mran = vkRates( fbView, mvTex, 0, vm, mw );
			const long md = mran ? diffs( vm, glMv ) : -1;
			std::printf( "VRSVIEW motion green: %s%s, %ld of %ld texels differ: %s\n", mran ? "ran" : "refused: ", mran ? "" : mw.c_str(), md,
				all, mran && md == 0 ? "EXACT" : "FAIL" );
			gateOk = gateOk && mran && md == 0;
			for ( int red : { 1, 2 } ) {
				std::vector<std::uint8_t> rv;
				std::string rw;
				const bool rran = vkRates( fbView, 0, red, rv, rw );
				const long rd = rran ? diffs( rv, gl ) : -1;
				const bool failed = rran && rd > 0;
				std::printf( "VRSVIEW red %d (%s): %s%s, %ld texels differ, want FAIL: %s\n", red,
					red == 1 ? "vrsMax 2" : "the twins cleared, no draw", rran ? "ran" : "refused: ", rran ? "" : rw.c_str(), rd,
					failed ? "FAIL (OK)" : "TESTS NOTHING" );
				gateOk = gateOk && failed;
			}
		}
	}
	{
		drawScene( fbOther, 0 );
		std::vector<std::uint8_t> v;
		std::string w;
		const bool ran = vkRates( fbOther, 0, 0, v, w );
		std::printf( "VRSVIEW refusal arm (a draw framebuffer that is not the view): %s%s, want refused: %s\n", ran ? "ran" : "refused: ",
			ran ? "" : w.c_str(), ran ? "FAIL" : "OK" );
		gateOk = gateOk && !ran;
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeVrsStat();
	const bool statOk = st.runs == 5 && st.fails == 1 && lastTwin[0] != 0 && lastTwin[1] != 0;
	std::printf( "VRSVIEW stat: %d runs (want 5), %d refused (want 1), %d pipelines in %.1f ms, twins %u %u: %s\n", st.runs, st.fails,
		st.pipelines, st.buildMs, lastTwin[0], lastTwin[1], statOk ? "OK" : "FAIL" );
	gateOk = gateOk && statOk;
	std::printf( gateOk ? "VRSVIEW GATE OK\n" : "VRSVIEW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
