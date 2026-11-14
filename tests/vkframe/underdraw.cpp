// Lane VKWATER4 (2026-11-14, plan row L7): the camera-underwater fog on the present device (vkunder.inc underRun: the
// tone_view_vs triangle, underwater_view_ps then underwater_view_ms_ps per sample, dual-source blended into the tone
// frame) against GL's own programs (cell_hdr.vert + cell_underwater.frag / cell_underwater_ms.frag as the app links them,
// written by waterdraw_prep.py), drawn with cellunderwater.cpp's state into the same 4x MSAA RGBA16F + D24S8 contents.
// fxview.cpp's helpers are reused (its main renamed). uwDepth is made by GL's cell_depthmax.frag from the frame's own
// depth (cellhdr.cpp depthMaxMake), as the app makes it.
// Scene (per sample, gl_SamplePosition): a floor whose depth ramps up the frame, a sky band (depth 1.0) and six discs
// nearer, each with its own colour, so edge pixels hold samples of two depths (the per-sample draw's work).
// Bar (set before the first run, never widened): per sample, every half within 2 ulp and <= 0.1 % of halves past 1 ulp
// (fxview's volsky bar); floors: the fog changes >= 50 % of the samples against the frame before it, and the scene holds
// >= 100 pixels whose samples differ in depth.
// Checks: GL's programs sample uwDepth and uwDepthMs only (the waterInfo / waterFlow images are not active in GL either);
// GL's gl_FragCoord in a per-sample draw is the pixel centre (the per-sample twin takes the centre too);
// arms A (WATR fog law), B (Underwater light row, class 1), C (the meniscus: the near plane crosses the surface).
// Each arm also runs both APIs' per-pixel draw alone and tables which draw fogged each sample: Vulkan must fog every
// sample by one draw at most ("both" 0). Arm A also draws GL's blend with constant outputs (info): on this PC GL's
// 4x blend leaves bare * mul + add in some 2-pixel tiles and writes one value to all four samples (the pixel's mean
// blended), single-source or dual, with or without a written sample mask; arm A's misses sit in those tiles.
// Reds (each must FAIL, never refuse): 1 no per-sample draw, 2 blend without SRC1 (dst ONE), 8 uwRow one vec4 off (the whole b31 moved uwSamples
// into a loop bound and hung the GPU: device lost on the next submit), 4 the
// uwDepth copy skipped (on a scene whose depth differs from the last copied one). A last arm with uwSamples 0 must refuse.
// Stat: 10 runs (3 of them the tables' red 1), 1 refusal, 9 uwDepth copies. Last line `UNDERDRAW GATE OK` or `UNDERDRAW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <array>

namespace udr
{
using namespace fv;

struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * texParameteri )( GLenum, GLenum, GLint ) = nullptr;
};
Ex ex;

bool loadMore()
{
	ex.genVertexArrays = reinterpret_cast<decltype( ex.genVertexArrays )>( glProc( "glGenVertexArrays" ) );
	ex.bindVertexArray = reinterpret_cast<decltype( ex.bindVertexArray )>( glProc( "glBindVertexArray" ) );
	ex.vertexAttribPointer = reinterpret_cast<decltype( ex.vertexAttribPointer )>( glProc( "glVertexAttribPointer" ) );
	ex.enableVertexAttribArray = reinterpret_cast<decltype( ex.enableVertexAttribArray )>( glProc( "glEnableVertexAttribArray" ) );
	ex.texParameteri = &::glTexParameteri;
	return ex.genVertexArrays && ex.bindVertexArray && ex.vertexAttribPointer && ex.enableVertexAttribArray;
}

constexpr GLenum kDsMode = 0x90EA, kDepthComp = 0x1902, kStencilIdx = 0x1901, kSrc1Color = 0x88F9, kArrayBuf = 0x8892,
	kElemBuf = 0x8893;

const char * kSceneFs = "#version 410 core\n"
	"uniform vec2 sz; uniform int seed; out vec4 o;\n"
	"void main() {\n"
	"  vec2 p = gl_FragCoord.xy - 0.5 + gl_SamplePosition;\n"
	"  float d = 0.985 + 0.0145 * ( p.y / sz.y );\n"
	"  vec3 c = vec3( 0.4 + 0.3 * sin( p.x * 0.07 ), 0.5 + 0.25 * cos( p.y * 0.05 ), 0.6 + 0.2 * sin( ( p.x + p.y ) * 0.03 ) );\n"
	"  if ( p.y > sz.y * 0.82 ) { d = 1.0; c = vec3( 1.5, 1.8, 2.2 ); }\n"
	"  for ( int i = 0; i < 6; i++ ) {\n"
	"    float k = float( i + 7 * seed );\n"
	"    vec2 ctr = vec2( fract( 0.137 * k + 0.11 ) * sz.x, ( 0.12 + 0.62 * fract( 0.291 * k + 0.07 ) ) * sz.y );\n"
	"    float rad = ( 0.06 + 0.06 * fract( 0.53 * k + 0.2 ) ) * sz.x;\n"
	"    float di = 0.93 + 0.009 * float( i ) + 0.004 * ( p.x / sz.x );\n"
	"    if ( distance( p, ctr ) < rad && di < d ) { d = di; c = vec3( fract( 0.31 * k ), fract( 0.57 * k ), fract( 0.83 * k ) ) * 1.6 + 0.1; }\n"
	"  }\n"
	"  gl_FragDepth = d; o = vec4( c, 0.5 );\n"
	"}\n";
const char * kDepthFs = "#version 410 core\n"
	"uniform sampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = vec4( texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ).r ); }\n";

struct Arm
{
	const char * name;
	int seed = 0, light = 0, red = 0, samples = 4;
	float eyeZ = -200.0f;
};
}  // namespace udr

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace udr;
	gTag = "UNDERDRAW";
	if ( argc < 3 ) {
		std::printf( "usage: underdraw <prep dir> <tree>\nUNDERDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1], tree = argv[2];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "UNDERDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 200, H = 120, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwunderdraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwunderdraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwunderdraw", "underwater draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40,
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
	std::printf( "UNDERDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	// the app's programs: cell_hdr.vert + the frag (prepped), cell_depthmax (no includes)
	const std::string hdrVs = readFile( dir + "/uw/prog.vert.glsl" );
	const GLuint pUw = program( hdrVs, readFile( dir + "/uw/prog.frag.glsl" ), "cell_underwater" );
	const GLuint pMs = program( readFile( dir + "/uwms/prog.vert.glsl" ), readFile( dir + "/uwms/prog.frag.glsl" ), "cell_underwater_ms" );
	const GLuint pMax = program( hdrVs, readFile( tree + "/res/shaders/features/exposure/cell_depthmax.frag" ), "cell_depthmax" );
	const GLuint pScene = program( kTriVs, kSceneFs, "scene" ), pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	const GLuint pDepth = program( kTriVs, kDepthFs, "depth unpack" );
	if ( !pUw || !pMs || !pMax || !pScene || !pUnpack || !pDepth )
		return fatal( "GL programs" );
	bool gateOk = true;

	// check: GL's own programs sample uwDepth and uwDepthMs only
	{
		const char * dead[5] = { "waterInfoIndex", "waterInfoAtlas", "waterInfoBodies", "waterFlowIndex", "waterFlowAtlas" };
		for ( GLuint p : { pUw, pMs } ) {
			int live = 0, deadN = 0;
			for ( const char * n : { "uwDepth", "uwDepthMs" } )
				live += f.getUniformLocation( p, n ) >= 0;
			for ( const char * n : dead )
				deadN += f.getUniformLocation( p, n ) >= 0;
			const bool ok = live == 2 && deadN == 0;
			std::printf( "UNDERDRAW samplers %s: uwDepth + uwDepthMs active %d/2, waterInfo / waterFlow active %d/5: %s\n",
				p == pUw ? "cell_underwater" : "cell_underwater_ms", live, deadN, ok ? "OK" : "FAIL" );
			gateOk = gateOk && ok;
		}
	}

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
	auto drawQuad = [&]() { glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr ); };

	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds ), fbFrameC = fbo( fT2DMS, fc, 0, 0 );
	const GLuint refCol = msTex( W, H, fRgba16f, S ), refDs = msTex( W, H, fD24s8, S );
	const GLuint fbRef = fbo( fT2DMS, refCol, 0, refDs ), fbRefC = fbo( fT2DMS, refCol, 0, 0 );
	const GLuint maxTex = tex2d( W, H, fR32f, fRed, GL_FLOAT, nullptr, false );
	const GLuint fbMax = fbo( GL_TEXTURE_2D, maxTex, 0, 0 );
	const GLuint unp = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false ), fbUnp = fbo( GL_TEXTURE_2D, unp, 0, 0 );
	const GLuint unpD = tex2d( WS, H, fR32f, fRed, GL_FLOAT, nullptr, false ), fbUnpD = fbo( GL_TEXTURE_2D, unpD, 0, 0 );
	if ( !fbFrame || !fbFrameC || !fbRef || !fbRefC || !fbMax || !fbUnp || !fbUnpD )
		return fatal( "GL targets" );
	auto dsMode = [&]( GLuint t, GLenum mode ) {
		glBindTexture( fT2DMS, t );
		ex.texParameteri( fT2DMS, kDsMode, GLint( mode ) );
		glBindTexture( fT2DMS, 0 );
	};
	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 15, fT2DMS, msColor );
		u1i( pUnpack, "src", 15 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		bindUnit( 15, fT2DMS, 0 );
		return out;
	};
	std::vector<char> mixMap( size_t( W ) * H );
	std::vector<float> depthS( size_t( WS ) * H );
	auto mixedPixels = [&]( GLuint ds ) {
		dsMode( ds, kDepthComp );
		std::vector<float> d( size_t( WS ) * H );
		f.bindFramebuffer( fFb, fbUnpD );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pDepth );
		bindUnit( 15, fT2DMS, ds );
		u1i( pDepth, "src", 15 );
		u1i( pDepth, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, fRed, GL_FLOAT, d.data() );
		bindUnit( 15, fT2DMS, 0 );
		dsMode( ds, kStencilIdx );
		depthS = d;
		int n = 0;
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const float * s = &d[size_t( y ) * WS + size_t( x ) * S];
				bool mixed = false;
				for ( int k = 1; k < S; k++ )
					mixed = mixed || std::fabs( s[k] - s[0] ) > 1e-3f;  // two surfaces, not one surface's slope
				n += mixed;
				mixMap[size_t( y ) * W + size_t( x )] = mixed;
			}
		return n;
	};
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
	// cellhdr.cpp depthMaxMake: the farthest depth per pixel of the frame's own depth
	auto depthMax = [&]( GLuint ds ) {
		f.bindFramebuffer( fFb, fbMax );
		glViewport( 0, 0, W, H );
		drawState0();
		f.activeTexture( fTex0 + 3 );
		glBindTexture( fT2DMS, ds );
		ex.texParameteri( fT2DMS, kDsMode, kDepthComp );
		f.useProgram( pMax );
		u1i( pMax, "depthMs", 3 );
		u1i( pMax, "depthSamples", S );
		drawQuad();
		ex.texParameteri( fT2DMS, kDsMode, kStencilIdx );
		glBindTexture( fT2DMS, 0 );
		f.activeTexture( fTex0 );
	};
	// cellunderwater.cpp's uniforms, the same on both programs
	const double N = 5.0, F = 100000.0, fov = 60.0 * 3.14159265358979 / 180.0, fy = 1.0 / std::tan( fov / 2 ), asp = double( W ) / H;
	auto uniforms = [&]( const Arm & a ) {
		const float ey[3] = { 100.0f, 200.0f, a.eyeZ };
		float inv[16] = {};  // column-major inverse of GL's perspective
		inv[0] = float( asp / fy );
		inv[5] = float( 1.0 / fy );
		inv[11] = float( ( N - F ) / ( 2.0 * F * N ) );  // column 2, row 3
		inv[14] = -1.0f;                                  // column 3, row 2
		inv[15] = float( ( F + N ) / ( 2.0 * F * N ) );
		const float rows[12] = { 0, 0, -1, ey[0], -1, 0, 0, ey[1], 0, 1, 0, ey[2] };  // forward +x, up +z
		for ( GLuint p : { pUw, pMs } ) {
			f.useProgram( p );
			auto L = [&]( const char * n ) { return f.getUniformLocation( p, n ); };
			f.uniform1i( L( "uwDepth" ), 1 );
			f.uniform1i( L( "uwDepthMs" ), 2 );
			f.uniform1i( L( "uwSamples" ), a.samples );
			f.uniformMatrix4fv( L( "uwInvProj" ), 1, GL_FALSE, inv );
			f.uniform4fv( L( "uwRow" ), 3, rows );
			const float vp[4] = { 0.0f, 0.0f, float( W ), float( H ) };
			f.uniform4fv( L( "uwVp" ), 1, vp );
			f.uniform3fv( L( "uwEye" ), 1, ey );
			const float fog[4] = { 0.9f, 0.0f, 1500.0f, 0.0f };
			f.uniform4fv( L( "uwFog" ), 1, fog );
			const float col[3] = { 0.05f, 0.12f, 0.1f }, tint[3] = { 0.2f, 0.45f, 0.4f };
			f.uniform3fv( L( "uwColor" ), 1, col );
			f.uniform3fv( L( "uwTint" ), 1, tint );
			f.uniform1i( L( "uwRed" ), 0 );
			f.uniform1i( L( "uwLight" ), a.light );
			f.uniform1i( L( "uwLightRed" ), 0 );
			const float sun[4] = { 2.0f, 1.9f, 1.7f, 0.6f }, sky[3] = { 0.3f, 0.4f, 0.5f }, men[2] = { 1.5f, 0.6f };
			f.uniform4fv( L( "uwSun" ), 1, sun );
			f.uniform3fv( L( "uwSky" ), 1, sky );
			f.uniform1i( L( "uwClass" ), 1 );
			f.uniform2fv( L( "uwMenK" ), 1, men );
		}
	};
	// GL's draws with cellunderwater.cpp's state into a colour-only target
	auto glFog = [&]( GLuint fbC, GLuint ds, int samples ) {
		f.bindFramebuffer( fFb, fbC );
		glViewport( 0, 0, W, H );
		drawState0();
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
		glEnable( GL_BLEND );
		f.blendFuncSeparate( GL_ONE, kSrc1Color, GL_ZERO, GL_ONE );
		f.activeTexture( fTex0 + 1 );
		glBindTexture( GL_TEXTURE_2D, maxTex );
		f.activeTexture( fTex0 + 2 );
		glBindTexture( fT2DMS, ds );
		ex.texParameteri( fT2DMS, kDsMode, kDepthComp );
		f.useProgram( pUw );
		drawQuad();
		if ( samples > 0 ) {
			f.useProgram( pMs );
			drawQuad();
		}
		ex.texParameteri( fT2DMS, kDsMode, kStencilIdx );
		glBindTexture( fT2DMS, 0 );
		f.activeTexture( fTex0 + 1 );
		glBindTexture( GL_TEXTURE_2D, 0 );
		f.activeTexture( fTex0 );
		drawState0();
	};
	auto vkFog = [&]( const Arm & a, std::string & w ) {
		WwVkPresent::UnderRun r;
		r.w = W;
		r.h = H;
		r.color = fc;
		r.depth = maxTex;
		r.depthW = W;
		r.depthH = H;
		r.depthFmt = fR32f;
		r.depthMs = fds;
		r.samples = a.samples;
		r.vp[2] = W;
		r.vp[3] = H;
		WwVkPresent::underPack( &ga, pUw, r.loose[0] );
		WwVkPresent::underPack( &ga, pMs, r.loose[1] );
		r.red = a.red;
		f.bindFramebuffer( fFb, fbFrameC );
		return WwVkPresent::underRun( r, w );
	};

	// check: GL's gl_FragCoord in a per-sample draw (gl_SampleID read) is the pixel centre, so the per-sample twin takes
	// the centre too (Vulkan's FragCoord there is the sample's own position)
	{
		const GLuint p1 = program( kTriVs,
			"#version 410 core\nout vec4 o; void main() { o = vec4( fract( gl_FragCoord.xy ), float( gl_SampleID ), 0.0 ); }\n", "fragcoord" );
		f.bindFramebuffer( fFb, fbRefC );
		glViewport( 0, 0, W, H );
		drawState0();
		f.useProgram( p1 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		const std::vector<std::uint16_t> s = readSamples( refCol );
		bool ok = p1 != 0;
		for ( int k = 0; k < S; k++ )
			ok = ok && h2f( s[k * 4] ) == 0.5f && h2f( s[k * 4 + 1] ) == 0.5f && h2f( s[k * 4 + 2] ) == float( k );
		std::printf( "UNDERDRAW GL gl_FragCoord in a per-sample draw at the pixel centre, %d samples: %s\n", S, ok ? "OK" : "FAIL" );
		gateOk = gateOk && ok;
	}
	const Arm arms[7] = { { "A fog law", 0, 0, 0 }, { "B light row", 1, 1, 0 }, { "C meniscus", 2, 1, 0, 4, -1.0f },
		{ "red 1 (no per-sample draw)", 0, 0, 1 }, { "red 2 (blend without SRC1)", 0, 0, 2 }, { "red 8 (uwRow one vec4 off)", 0, 0, 8 },
		{ "red 4 (uwDepth copy skipped)", 3, 0, 4 } };
	for ( const Arm & a : arms ) {
		drawScene( fbFrame, a.seed );
		drawScene( fbRef, a.seed );
		const int mixed = mixedPixels( fds );
		const std::vector<std::uint16_t> bare = readSamples( refCol );
		depthMax( fds );
		uniforms( a );
		glFog( fbRefC, refDs, a.samples );
		std::string w;
		const bool ran = vkFog( a, w );
		const std::vector<std::uint16_t> vk = readSamples( fc ), ref = readSamples( refCol );
		const Halves h = compareHalves( vk, ref );
		size_t fogged = 0;  // samples whose RGB the fog changed
		for ( size_t i = 0; i < ref.size(); i += 4 )
			fogged += ref[i] != bare[i] || ref[i + 1] != bare[i + 1] || ref[i + 2] != bare[i + 2];
		const size_t total = ref.size() / 4;
		const bool floorOk = fogged * 2 >= total && mixed >= 100;
		if ( a.red == 0 && !h.ok() ) {  // where: mixed / plain pixels, per sample index, the worst few
			size_t past[2][4] = {};
			for ( size_t i = 0; i < vk.size(); i++ ) {
				const int d = std::abs( ordered( vk[i] ) - ordered( ref[i] ) );
				if ( d > 1 ) {
					const size_t smp = i / 4, y = smp / WS, xs = smp % WS;
					past[mixMap[y * W + xs / S] ? 1 : 0][xs % S]++;
				}
			}
			std::printf( "UNDERDRAW   past 1 ulp: plain pixels %zu %zu %zu %zu, mixed pixels %zu %zu %zu %zu (by sample)\n", past[0][0],
				past[0][1], past[0][2], past[0][3], past[1][0], past[1][1], past[1][2], past[1][3] );
			{
				std::vector<int> rows( H );
				for ( size_t i = 0; i < vk.size(); i++ )
					if ( std::abs( ordered( vk[i] ) - ordered( ref[i] ) ) > 1 )
						rows[( i / 4 ) / WS]++;
				std::printf( "UNDERDRAW   rows past 1 ulp (row:halves)" );
				for ( int y = 0; y < H; y++ )
					if ( rows[y] )
						std::printf( " %d:%d", y, rows[y] );
				std::printf( "\n" );
			}
		}
		// which draw fogged each sample, per API (0 neither, 1 per-pixel, 2 per-sample, 3 both), against the per-pixel
		// draw alone (GL: glFog without the second draw; Vulkan: red 1). Vulkan "both" must be 0 (the twins agree on
		// uwNearMax); the table, with the samples past 1 ulp per cell, shows where GL and Vulkan part
		bool onceOk = true;
		if ( a.red == 0 ) {
			drawScene( fbRef, a.seed );
			glFog( fbRefC, refDs, 0 );
			const std::vector<std::uint16_t> ppG = readSamples( refCol );
			drawScene( fbFrame, a.seed );
			Arm a1 = a;
			a1.red = 1;
			std::string w2;
			vkFog( a1, w2 );
			const std::vector<std::uint16_t> ppV = readSamples( fc );
			auto rgbNe = [&]( const std::vector<std::uint16_t> & x, const std::vector<std::uint16_t> & y, size_t i ) {
				return x[i] != y[i] || x[i + 1] != y[i + 1] || x[i + 2] != y[i + 2];
			};
			size_t cross[4][4] = {}, bad[4][4] = {};
			for ( size_t i = 0; i < vk.size(); i += 4 ) {
				const int g = ( rgbNe( ppG, bare, i ) ? 1 : 0 ) | ( rgbNe( ref, ppG, i ) ? 2 : 0 );
				const int v = ( rgbNe( ppV, bare, i ) ? 1 : 0 ) | ( rgbNe( vk, ppV, i ) ? 2 : 0 );
				cross[g][v]++;
				for ( int c = 0; c < 3; c++ )
					if ( std::abs( ordered( vk[i + c] ) - ordered( ref[i + c] ) ) > 1 ) {
						bad[g][v]++;
						break;
					}
			}
			for ( int g = 0; g < 4; g++ )
				std::printf( "UNDERDRAW   draws gl %d: vk 0 %zu (%zu past 1 ulp), vk 1 %zu (%zu), vk 2 %zu (%zu), vk 3 %zu (%zu)\n", g, cross[g][0],
					bad[g][0], cross[g][1], bad[g][1], cross[g][2], bad[g][2], cross[g][3], bad[g][3] );
			onceOk = cross[0][3] + cross[1][3] + cross[2][3] + cross[3][3] == 0;
		}
		if ( a.red == 0 && a.seed == 0 ) {  // GL's own blend, constant outputs (add 0.01, mul 0.65): does it keep bare * 0.65 + 0.01 per sample?
				static const char * fsMask = "#version 410 core\nlayout(location=0,index=0) out vec4 fa; layout(location=0,index=1) out vec4 fm;\n"
											 "void main() { gl_SampleMask[0] = 15; fa = vec4( 0.01 ); fm = vec4( 0.65 ); }\n";
				static const char * fsNoMask = "#version 410 core\nlayout(location=0,index=0) out vec4 fa; layout(location=0,index=1) out vec4 fm;\n"
											   "void main() { fa = vec4( 0.01 ); fm = vec4( 0.65 ); }\n";
				static const char * fsSingle = "#version 410 core\nout vec4 fa;\n"
											   "void main() { gl_SampleMask[0] = 15; fa = vec4( 0.01 ); }\n";
				const GLuint qMask = program( hdrVs, fsMask, "probe mask quad" ), qNo = program( hdrVs, fsNoMask, "probe nomask quad" );
				const GLuint tMask = program( kTriVs, fsMask, "probe mask tri" ), qSingle = program( hdrVs, fsSingle, "probe single" );
				struct V { const char * n; GLuint p; bool tri, single; } vs[4] = { { "dual mask quad", qMask, false, false },
					{ "dual nomask quad", qNo, false, false }, { "dual mask tri", tMask, true, false }, { "single mask quad", qSingle, false, true } };
				for ( const V & v : vs ) {
					drawScene( fbRef, a.seed );
					f.bindFramebuffer( fFb, fbRefC );
					glViewport( 0, 0, W, H );
					drawState0();
					glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
					glEnable( GL_BLEND );
					if ( v.single ) {
						reinterpret_cast<void( APIENTRY * )( GLfloat, GLfloat, GLfloat, GLfloat )>( glProc( "glBlendColor" ) )( 0.65f, 0.65f, 0.65f, 0.65f );
						f.blendFuncSeparate( GL_ONE, 0x8001 /* CONSTANT_COLOR */, GL_ZERO, GL_ONE );
					} else
						f.blendFuncSeparate( GL_ONE, kSrc1Color, GL_ZERO, GL_ONE );
					f.useProgram( v.p );
					if ( v.tri )
						glDrawArrays( GL_TRIANGLES, 0, 3 );
					else
						drawQuad();
					drawState0();
					const std::vector<std::uint16_t> pp = readSamples( refCol );
					size_t off = 0, n = 0;  // samples off by more than 2 ulp at the value
					for ( size_t j = 0; j < pp.size(); j += 4 ) {
						const float want = h2f( bare[j] ) * 0.65f + 0.01f, got = h2f( pp[j] );
						off += std::fabs( got - want ) > std::max( 0.002f * want, 1e-4f );
						n++;
					}
					std::printf( "UNDERDRAW   GL blend %s (info): %zu of %zu samples off bare * 0.65 + 0.01\n", v.n, off, n );
				}
			}
			int shown = 0;
			for ( size_t i = 0; i < vk.size() && shown < 6 && !h.ok() && a.red == 0; i += 4 ) {
				const int d = std::abs( ordered( vk[i] ) - ordered( ref[i] ) );
				if ( d > 16 ) {
					const size_t smp = i / 4, y = smp / WS, xs = smp % WS;
					float dm = 0.0f;
					for ( int k = 0; k < S; k++ )
						dm = std::max( dm, depthS[y * WS + ( xs / S ) * S + k] );
					std::printf( "UNDERDRAW   x %zu y %zu s %zu %s depth %.7f pixel max %.7f: vk %.4f %.4f %.4f gl %.4f %.4f %.4f bare %.4f\n",
						xs / S, y, xs % S, mixMap[y * W + xs / S] ? "mixed" : "plain", double( depthS[smp] ), double( dm ), double( h2f( vk[i] ) ),
						double( h2f( vk[i + 1] ) ), double( h2f( vk[i + 2] ) ), double( h2f( ref[i] ) ), double( h2f( ref[i + 1] ) ),
						double( h2f( ref[i + 2] ) ), double( h2f( bare[i] ) ) );
					shown++;
				}
			}
		if ( a.red == 0 ) {
			const bool ok = ran && h.ok() && floorOk && onceOk;
			std::printf( "UNDERDRAW arm %s: %s (halves %zu differ, %zu past 1 ulp, max %d ulp of %zu); fogged %zu / %zu samples, "
						 "mixed-depth pixels %d (floors 50 %%, 100); Vulkan fogs each sample once: %s%s%s: %s\n",
				a.name, ran ? h.word() : "REFUSED", h.diff, h.past1, h.maxu, h.total, fogged, total, mixed, onceOk ? "yes" : "NO",
				ran ? "" : " why: ", ran ? "" : w.c_str(), ok ? "OK" : "FAIL" );
			gateOk = gateOk && ok;
		} else {
			const bool failed = ran && !h.ok();
			std::printf( "UNDERDRAW %s: %s (halves %zu differ, %zu past 1 ulp, max %d ulp)%s%s, want FAIL: %s\n", a.name,
				ran ? ( h.ok() ? h.word() : "FAIL" ) : "REFUSED", h.diff, h.past1, h.maxu, ran ? "" : " why: ", ran ? "" : w.c_str(),
				failed ? "OK" : "NOT ARMED" );
			gateOk = gateOk && failed;
		}
	}
	// uwSamples 0 (GL's one draw from the farthest depth): refused with a reason, GL draws it
	{
		Arm a { "refusal", 0, 0, 0, 0 };
		uniforms( a );
		std::string w;
		const bool ran = vkFog( a, w );
		std::printf( "UNDERDRAW refusal arm (uwSamples 0): %s%s, want refused: %s\n", ran ? "ran" : "refused: ", ran ? "" : w.c_str(),
			ran ? "FAIL" : "OK" );
		gateOk = gateOk && !ran;
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeUnderStat();
	const bool statOk = st.runs == 10 && st.fails == 1 && st.copies == 9;
	std::printf( "UNDERDRAW stat: %d runs (want 10), %d refused (want 1), %d uwDepth copies (want 9), %d pipelines in %.1f ms: %s\n",
		st.runs, st.fails, st.copies, st.pipelines, st.buildMs, statOk ? "OK" : "FAIL" );
	gateOk = gateOk && statOk;
	std::printf( gateOk ? "UNDERDRAW GATE OK\n" : "UNDERDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
