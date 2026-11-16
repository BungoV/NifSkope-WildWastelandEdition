// Lane VKSMALL5 (2026-11-16, census row 77): the soft-effect layer (cellfxres.cpp wwCellFxResDraw) step 1 (the layer's
// depth, cell_fxres_down) and step 3 (the composite, cell_fxres_comp, dual-source blended into the 4x MSAA tone frame
// with a stencil mark) on the present device (vkfxres.inc) against GL's own programs (cell_ao.vert + the tree's .frag
// files) drawn with cellfxres.cpp's state. Step 2 (the soft draws) stays GL on both sides: a synthetic MRT fill into the
// layer (c, t, cov, depth test LESS against the layer depth, holes) after the three clears (0, 1, 0), as the app does.
// fxview.cpp's helpers are reused (its main renamed).
// Frame 203 x 121, 4 samples (W, H not divisible by the scale). Arm A: scale 2, fxrMode 0, seed 0. Arm B: scale 4,
// fxrMode 1, seed 1. Each arm: seed the frame (colour, depth per sample; stencil 0x82 left half, 0x0F right half), GL's
// passes on GL-owned layer targets, read; seed again, Vulkan's passes on the route's shared targets, read.
// Bar (set before the first run, never widened):
//   z (R32F) and the layer depth (D32F) after step 1: EXACT, every texel bit-equal.
//   colour after step 3, per sample (unpacked to W*S x H RGBA16F): fxview Halves ok() (<= 2 ulp, <= 0.1 % past 1 ulp).
//   stencil after step 3, per sample: EXACT.
//   Added after run 2 (measured, not a wider tolerance): NV GL blends a fixed set of pixels per PIXEL, one value
//   C + T x mean(dst) into every sample, where the spec (and Vulkan) blends per sample (C + T x dst_s): runs 3-4 dumped
//   it, 1144 / 1424 pixels, every one of them with sample-varying dst, GL = C + T x mean(start) at all of them, all
//   other written pixels bit-equal. Those pixels (GL one value, start not) leave the colour compare and get their own:
//   Vulkan per sample at every one, and Vulkan's sample mean within 1 ulp of GL's value per channel.
// Floors: the start state read twice is EXACT (colour + stencil, before GL's and before Vulkan's passes); GL's composite
// changes >= 1000 colour samples and leaves >= 1000 (cov holes); it rewrites >= 1000 stencil samples and leaves >= 1000;
// arm A's edge probe (the comp source with outC = the edge flag) counts >= 100 edge pixels and >= 100 bilinear pixels.
// Reds (each must FAIL, never refuse; run in arm A): comp red 1 stencil write mask 0; comp red 2 blend dst ONE (no SRC1);
// down red 4 max instead of min (FMin patched to FMax in a copy of the SPIR-V: 0 patched words = TESTS NOTHING).
// Refusal arm: a composite whose colour is not the tone frame's must refuse.
// Stat: 10 runs (crossings: 2 arms x 2 + 3 reds x 2), 1 refused, 10 copies (fxrSrc into its twin, every crossing), the
// owner's targets gone after fxresTargetsDrop. Last line `FXRESVIEW GATE OK` or `FXRESVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <array>

#include "../../src/features/vkframe/vkfxres.h"

namespace fxrg
{
using namespace fv;

struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
	void( WINAPI * blendEquation )( GLenum ) = nullptr;
};
Ex ex;

bool loadMore()
{
	return load( ex.genVertexArrays, "glGenVertexArrays" ) && load( ex.bindVertexArray, "glBindVertexArray" )
		&& load( ex.vertexAttribPointer, "glVertexAttribPointer" ) && load( ex.enableVertexAttribArray, "glEnableVertexAttribArray" )
		&& load( ex.clearBufferfv, "glClearBufferfv" ) && load( ex.blendEquation, "glBlendEquation" );
}

constexpr GLenum gxDsMode = 0x90EA, gxDepthComp = 0x1902, gxStencilIdx = 0x1901, gxArrayBuf = 0x8892, gxElemBuf = 0x8893;
constexpr GLenum gxColor = 0x1800, gxFuncAdd = 0x8006, gxSrc1Color = 0x88F9, gxSrc1Alpha = 0x8589, gxDepthStencil = 0x84F9;
constexpr GLenum gxU248 = 0x84FA, gxDepth32f = 0x8CAC, gxDepthAtt = 0x8D00, gxDsAtt2d = 0x821A;

// the frame: colour and depth per sample (discs over a floor ramp and a sky band, underdraw.cpp's shape)
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
	"  gl_FragDepth = d;\n"
	"  o = vec4( 0.1 + 0.8 * fract( p.x * 0.0173 + 0.31 * float( seed ) ), 0.2 + 0.7 * fract( p.y * 0.0219 + d * 3.0 ),\n"
	"    0.05 + 0.9 * fract( ( p.x + p.y ) * 0.0091 ), 0.3 + 0.6 * fract( p.x * 0.031 - p.y * 0.017 ) );\n"
	"}\n";

// step 2's stand-in: the soft draws' sum into the layer (C, T with T.a != T.r, cov 1), holes, depth tested
const char * kFillFs = "#version 410 core\n"
	"uniform vec2 lsz; uniform int seed;\n"
	"layout ( location = 0 ) out vec4 oc; layout ( location = 1 ) out vec4 ot; layout ( location = 2 ) out vec4 ocov;\n"
	"void main() {\n"
	"  vec2 p = gl_FragCoord.xy;\n"
	"  if ( p.x < lsz.x * 0.18 || mod( floor( p.x / 5.0 ) + floor( p.y / 3.0 ), 4.0 ) == 0.0 ) discard;\n"
	"  float k = float( seed );\n"
	"  oc = vec4( 0.05 + 0.4 * fract( p.x * 0.071 + k * 0.2 ), 0.1 + 0.3 * fract( p.y * 0.043 ),\n"
	"    0.02 + 0.5 * fract( ( p.x - p.y ) * 0.029 ), 0.1 + 0.2 * fract( p.y * 0.11 ) );\n"
	"  ot = vec4( 0.15 + 0.6 * fract( p.y * 0.053 + p.x * 0.013 ), 0.9, 0.7, 0.35 + 0.5 * fract( p.x * 0.037 + k * 0.4 ) );\n"
	"  ocov = vec4( 1.0 );\n"
	"  gl_FragDepth = 0.95 + 0.045 * fract( p.x * 0.019 + p.y * 0.027 );\n"
	"}\n";

const char * kStencilFs = "#version 410 core\n"
	"uniform usampler2DMS src; uniform int S; out vec4 o;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); o = vec4( float( texelFetch( src, ivec2( q.x / S, q.y ), q.x % S ).r ) ); }\n";

struct Layer
{
	GLuint z = 0, depth = 0, c = 0, t = 0, cov = 0, zFb = 0, layerFb = 0;
	int w = 0, h = 0;
};

GLuint fb2d( GLuint c0, GLuint c1, GLuint c2, GLuint depth )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( fFb, b );
	const GLuint cs[3] = { c0, c1, c2 };
	int n = 0;
	for ( int i = 0; i < 3; i++ ) {
		if ( cs[i] ) {
			f.framebufferTexture2D( fFb, fCol0 + GLenum( i ), GL_TEXTURE_2D, cs[i], 0 );
			n = i + 1;
		}
	}
	if ( depth )
		f.framebufferTexture2D( fFb, gxDepthAtt, GL_TEXTURE_2D, depth, 0 );
	const GLenum bufs[3] = { fCol0, fCol0 + 1, fCol0 + 2 };
	f.drawBuffers( n, bufs );
	if ( f.checkFramebufferStatus( fFb ) != fComplete ) {
		std::printf( "%s layer framebuffer incomplete\n", gTag );
		return 0;
	}
	return b;
}

struct Out
{
	bool ran = false;
	std::string why;
	std::vector<float> z, depth, st;
	std::vector<std::uint16_t> col;
};

long bitDiffs( const std::vector<float> & a, const std::vector<float> & b )
{
	if ( a.size() != b.size() || a.empty() )
		return -1;
	long n = 0;
	for ( size_t i = 0; i < a.size(); i++ )
		n += std::memcmp( &a[i], &b[i], 4 ) != 0;
	return n;
}

long halfDiffs( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
{
	if ( a.size() != b.size() || a.empty() )
		return -1;
	long n = 0;
	for ( size_t i = 0; i < a.size(); i += 4 )
		n += std::memcmp( &a[i], &b[i], 8 ) != 0;
	return n;
}
}  // namespace fxrg

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fxrg;
	gTag = "FXRESVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: fxresview <tree>\nFXRESVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string tree = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FXRESVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 203, H = 121;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfxresview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfxresview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfxresview", "fxres harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
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
	std::printf( "FXRESVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	// the app's programs (cell_fxres_down.prog / cell_fxres_comp.prog: cell_ao.vert + the .frag, no includes)
	const std::string vs = readFile( tree + "/res/shaders/cell_ao.vert" );
	const std::string compFs = readFile( tree + "/res/shaders/features/particles/cell_fxres_comp.frag" );
	const GLuint pDown = program( vs, readFile( tree + "/res/shaders/features/particles/cell_fxres_down.frag" ), "cell_fxres_down" );
	const GLuint pComp = program( vs, compFs, "cell_fxres_comp" );
	// the edge probe: the composite's source with outC = the edge flag (1 edge, 0 bilinear), blend off, into a 1-sample target
	std::string probeFs = compFs;
	const size_t at = probeFs.find( "outC = c;" );
	if ( at == std::string::npos )
		return fatal( "edge probe: no `outC = c;` in cell_fxres_comp.frag" );
	probeFs.replace( at, 9, "outC = vec4( edge ? 1.0 : 0.0, 0.0, 0.0, 1.0 );" );
	const GLuint pProbe = program( vs, probeFs, "edge probe" );
	const GLuint pScene = program( kTriVs, kSceneFs, "scene" );
	const GLuint pFill = program( kTriVs, kFillFs, "fill" );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" );
	const GLuint pStencil = program( kTriVs, kStencilFs, "stencil unpack" );
	if ( !pDown || !pComp || !pProbe || !pScene || !pFill || !pUnpack || !pStencil )
		return fatal( "GL programs" );
	bool gateOk = true;

	// the app's quad (drawShape: 4 vertices, 6 indices) on attribute 0
	GLuint vao = 0, vbo = 0, ebo = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	static const float quadV[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t quadI[6] = { 0, 1, 2, 2, 1, 3 };
	f.genBuffers( 1, &vbo );
	f.bindBuffer( gxArrayBuf, vbo );
	f.bufferData( gxArrayBuf, sizeof( quadV ), quadV, fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	ex.enableVertexAttribArray( 0 );
	f.genBuffers( 1, &ebo );
	f.bindBuffer( gxElemBuf, ebo );
	f.bufferData( gxElemBuf, sizeof( quadI ), quadI, fStatic );

	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint otherCol = msTex( W, H, fRgba16f, S ), otherDs = msTex( W, H, fD24s8, S );
	const GLuint fbOther = fbo( fT2DMS, otherCol, 0, otherDs );
	// the opaque depth (Scene::fxDepthTexId): a GL-owned 1-sample D24S8, so the route copies it into its twin
	const GLuint fullTex = tex2d( W, H, fD24s8, gxDepthStencil, gxU248, nullptr, false );
	const GLuint fullCol = tex2d( W, H, fRgba16f, GL_RGBA, fHalf, nullptr, false );
	const GLuint fbFull = fb2d( fullCol, 0, 0, 0 );
	if ( fbFull ) {
		f.bindFramebuffer( fFb, fbFull );
		f.framebufferTexture2D( fFb, gxDsAtt2d, GL_TEXTURE_2D, fullTex, 0 );
	}
	const GLuint unC = tex2d( W * S, H, fRgba16f, GL_RGBA, fHalf, nullptr, false ), fbUnC = fbo( GL_TEXTURE_2D, unC, 0, 0 );
	const GLuint unS = tex2d( W * S, H, fR32f, fRed, GL_FLOAT, nullptr, false ), fbUnS = fbo( GL_TEXTURE_2D, unS, 0, 0 );
	const GLuint prb = tex2d( W, H, fRgba16f, GL_RGBA, fHalf, nullptr, false ), fbPrb = fbo( GL_TEXTURE_2D, prb, 0, 0 );
	if ( !fbFrame || !fbOther || !fbFull || !fbUnC || !fbUnS || !fbPrb )
		return fatal( "GL targets" );
	const float proj[2] = { -1.002002f, -10.01001f };  // n 5, f 5000: [2][2], [3][2]
	const float edgeK = 0.05f;

	auto drawScene = [&]( GLuint fb, int seed, int w, int h ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, w, h );
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
	auto seedFrame = [&]( int seed ) {
		drawScene( fbFrame, seed, W, H );
		glEnable( GL_SCISSOR_TEST );
		glStencilMask( 0xFF );
		glScissor( 0, 0, W / 2, H );
		glClearStencil( 0x82 );
		glClear( GL_STENCIL_BUFFER_BIT );
		glScissor( W / 2, 0, W - W / 2, H );
		glClearStencil( 0x0F );
		glClear( GL_STENCIL_BUFFER_BIT );
		glClearStencil( 0 );
		drawState0();
		drawScene( fbFull, seed, W, H );  // the opaque depth at 1 sample (pixel centres)
	};
	auto readR32 = [&]( GLuint t, int w, int h ) {
		std::vector<float> px( static_cast<size_t>( w ) * h, -7.0f );
		glBindTexture( GL_TEXTURE_2D, t );
		glGetTexImage( GL_TEXTURE_2D, 0, fRed, GL_FLOAT, px.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		return px;
	};
	auto readD32 = [&]( GLuint t, int w, int h ) {
		std::vector<float> px( static_cast<size_t>( w ) * h, -7.0f );
		glBindTexture( GL_TEXTURE_2D, t );
		glGetTexImage( GL_TEXTURE_2D, 0, gxDepthComp, GL_FLOAT, px.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		return px;
	};
	auto readCol = [&]() {
		f.bindFramebuffer( fFb, fbUnC );
		glViewport( 0, 0, W * S, H );
		drawState0();
		bindUnit( 0, fT2DMS, fc );
		f.useProgram( pUnpack );
		u1i( pUnpack, "src", 0 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		bindUnit( 0, fT2DMS, 0 );
		std::vector<std::uint16_t> px( static_cast<size_t>( W ) * S * H * 4, 0x7E00 );
		glReadPixels( 0, 0, W * S, H, GL_RGBA, fHalf, px.data() );
		return px;
	};
	auto readSt = [&]() {
		f.bindFramebuffer( fFb, fbUnS );
		glViewport( 0, 0, W * S, H );
		drawState0();
		bindUnit( 0, fT2DMS, fds );
		f.activeTexture( fTex0 );
		glBindTexture( fT2DMS, fds );
		glTexParameteri( fT2DMS, gxDsMode, gxStencilIdx );
		f.useProgram( pStencil );
		u1i( pStencil, "src", 0 );
		u1i( pStencil, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		bindUnit( 0, fT2DMS, 0 );
		std::vector<float> px( static_cast<size_t>( W ) * S * H, -7.0f );
		glReadPixels( 0, 0, W * S, H, fRed, GL_FLOAT, px.data() );
		return px;
	};
	// step 1 on GL (cellfxres.cpp's state)
	auto glDown = [&]( Layer & L, int scale ) {
		f.bindFramebuffer( fFb, L.zFb );
		glViewport( 0, 0, L.w, L.h );
		drawState0();
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		glDepthMask( GL_TRUE );
		bindUnit( 0, GL_TEXTURE_2D, fullTex );
		f.useProgram( pDown );
		u1i( pDown, "fxrSrc", 0 );
		u1i( pDown, "fxrScale", scale );
		const GLint ssz[2] = { W, H };
		f.uniform2iv( f.getUniformLocation( pDown, "fxrSrcSize" ), 1, ssz );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		bindUnit( 0, GL_TEXTURE_2D, 0 );
		glDepthFunc( GL_LESS );
		drawState0();
	};
	// the three clears, then step 2's stand-in (GL on both sides)
	auto clearFill = [&]( GLuint layerFb, int w, int h, int seed ) {
		f.bindFramebuffer( fFb, layerFb );
		glViewport( 0, 0, w, h );
		drawState0();
		const GLfloat zero[4] = { 0, 0, 0, 0 }, one[4] = { 1, 1, 1, 1 };
		ex.clearBufferfv( gxColor, 0, zero );
		ex.clearBufferfv( gxColor, 1, one );
		ex.clearBufferfv( gxColor, 2, zero );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glDepthMask( GL_TRUE );
		f.useProgram( pFill );
		f.uniform2fv( f.getUniformLocation( pFill, "lsz" ), 1, std::array<float, 2> { float( w ), float( h ) }.data() );
		u1i( pFill, "seed", seed );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
	};
	auto bindComp = [&]( GLuint p, const Layer & L, int scale, int mode ) {
		const GLuint tex[5] = { L.c, L.t, L.cov, L.z, fullTex };
		static const char * const names[5] = { "fxrC", "fxrT", "fxrCov", "fxrZ", "fxrDepth" };
		f.useProgram( p );
		for ( int i = 0; i < 5; i++ ) {
			bindUnit( i, GL_TEXTURE_2D, tex[i] );
			u1i( p, names[i], i );
		}
		u1i( p, "fxrScale", scale );
		const GLint lsz[2] = { L.w, L.h };
		f.uniform2iv( f.getUniformLocation( p, "fxrSize" ), 1, lsz );
		f.uniform2fv( f.getUniformLocation( p, "fxrProj" ), 1, proj );
		f.uniform1fv( f.getUniformLocation( p, "fxrEdge" ), 1, &edgeK );
		u1i( p, "fxrMode", mode );
	};
	auto unbind5 = [&]() {
		for ( int i = 4; i >= 0; i-- )
			bindUnit( i, GL_TEXTURE_2D, 0 );
	};
	// step 3 on GL (cellfxres.cpp's state)
	auto glComp = [&]( const Layer & L, int scale, int mode ) {
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_BLEND );
		ex.blendEquation( gxFuncAdd );
		f.blendFuncSeparate( GL_ONE, gxSrc1Color, GL_ONE, gxSrc1Alpha );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0x03 );
		glStencilFunc( GL_ALWAYS, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		bindComp( pComp, L, scale, mode );
		glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
		unbind5();
		glStencilMask( 0xFF );
		glStencilFunc( GL_ALWAYS, 0, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		f.blendFuncSeparate( GL_ONE, GL_ZERO, GL_ONE, GL_ZERO );
		drawState0();
	};
	auto glLayer = [&]( int w, int h ) {
		Layer L;
		L.w = w;
		L.h = h;
		L.z = tex2d( w, h, fR32f, fRed, GL_FLOAT, nullptr, false );
		L.depth = tex2d( w, h, gxDepth32f, gxDepthComp, GL_FLOAT, nullptr, false );
		L.c = tex2d( w, h, fRgba16f, GL_RGBA, fHalf, nullptr, false );
		L.t = tex2d( w, h, fRgba16f, GL_RGBA, fHalf, nullptr, false );
		L.cov = tex2d( w, h, fR8, fRed, GL_UNSIGNED_BYTE, nullptr, false );
		L.zFb = fb2d( L.z, 0, 0, L.depth );
		L.layerFb = fb2d( L.c, L.t, L.cov, L.depth );
		return L;
	};
	static int owner = 0;
	const void * own = &owner;
	auto vkRun = [&]( int scale, int mode, int seed, int downRed, int compRed, Out & o ) {
		const int w = ( W + scale - 1 ) / scale, h = ( H + scale - 1 ) / scale;
		WwVkPresent::FxresTargets T;
		o.ran = false;
		if ( !WwVkPresent::fxresTargets( own, w, h, T, o.why ) )
			return;
		if ( !WwVkPresent::fxresTargetsAre( own, T ) ) {
			o.why = "fxresTargetsAre false right after fxresTargets";
			return;
		}
		WwVkPresent::FxresDown d;
		d.owner = own;
		d.W = W;
		d.H = H;
		d.scale = scale;
		d.src = fullTex;
		d.srcFmt = fD24s8;
		d.red = downRed;
		if ( !WwVkPresent::fxresDownRun( d, o.why ) )
			return;
		o.z = readR32( T.z, w, h );
		o.depth = readD32( T.depth, w, h );
		const GLuint lf = fb2d( T.c, T.t, T.cov, T.depth );
		if ( !lf ) {
			o.why = "the shared layer framebuffer";
			return;
		}
		clearFill( lf, w, h, seed );
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		WwVkPresent::FxresComp cp;
		cp.owner = own;
		cp.color = fc;
		cp.W = W;
		cp.H = H;
		cp.scale = scale;
		cp.src = fullTex;
		cp.srcFmt = fD24s8;
		cp.proj[0] = proj[0];
		cp.proj[1] = proj[1];
		cp.edge = edgeK;
		cp.mode = mode;
		cp.vp[2] = W;
		cp.vp[3] = H;
		cp.red = compRed;
		if ( !WwVkPresent::fxresCompRun( cp, o.why ) )
			return;
		o.ran = true;
		o.col = readCol();
		o.st = readSt();
	};
	// a run against GL's: every compare of the bar; true = all pass
	auto judge = [&]( const char * tag, const Out & gl, const Out & vk, const std::vector<std::uint16_t> & start, bool print ) {
		const long dz = bitDiffs( vk.z, gl.z ), dd = bitDiffs( vk.depth, gl.depth ), ds = bitDiffs( vk.st, gl.st );
		// GL's per-pixel blend pixels (the header): out of the sample compare, into their own
		std::vector<std::uint16_t> vkCol = vk.col;
		long ppx = 0, ppxPerSample = 0;
		double ppxWorst = 0.0;
		if ( vk.ran && vkCol.size() == gl.col.size() && start.size() == gl.col.size() ) {
			auto same = [&]( const std::vector<std::uint16_t> & v, size_t p ) {
				for ( int s = 1; s < S; s++ )
					for ( int c = 0; c < 4; c++ )
						if ( v[p + size_t( s ) * 4 + c] != v[p + c] )
							return false;
				return true;
			};
			for ( size_t p = 0; p < gl.col.size(); p += size_t( S ) * 4 ) {
				if ( !same( gl.col, p ) || same( start, p ) || std::equal( gl.col.begin() + p, gl.col.begin() + p + S * 4, start.begin() + p ) )
					continue;
				ppx++;
				ppxPerSample += !same( vk.col, p );
				for ( int c = 0; c < 4; c++ ) {
					double m = 0.0;
					for ( int s = 0; s < S; s++ )
						m += h2f( vk.col[p + size_t( s ) * 4 + c] );
					m /= S;
					const std::uint16_t gh = gl.col[p + c];
					const double g = h2f( gh ), ulp = std::fabs( double( h2f( std::uint16_t( gh + 1 ) ) ) - g );
					ppxWorst = std::max( ppxWorst, std::fabs( m - g ) / ( ulp > 0 ? ulp : 1e-30 ) );
				}
				std::copy( gl.col.begin() + p, gl.col.begin() + p + S * 4, vkCol.begin() + p );
			}
		}
		const bool ppxOk = ppxPerSample == ppx && ppxWorst <= 1.0;
		const Halves hv = compareHalves( vkCol, gl.col );
		const bool ok = vk.ran && dz == 0 && dd == 0 && ds == 0 && hv.ok() && ppxOk;
		if ( print ) {
			std::printf( "FXRESVIEW %s GL per-pixel blend: %ld pixels (GL one value, start samples differ), Vulkan per sample at %ld (want all), "
						 "Vulkan's sample mean vs GL max %.3f ulp (want <= 1): %s\n",
				tag, ppx, ppxPerSample, ppxWorst, ppxOk ? "OK" : "FAIL" );
			std::printf( "FXRESVIEW %s z: %ld of %zu texels differ: %s\n", tag, dz, gl.z.size(), dz == 0 ? "EXACT" : "FAIL" );
			std::printf( "FXRESVIEW %s layer depth: %ld of %zu texels differ: %s\n", tag, dd, gl.depth.size(), dd == 0 ? "EXACT" : "FAIL" );
			std::printf( "FXRESVIEW %s colour: %zu of %zu halves differ, %zu past 1 ulp, max %d ulp: %s\n", tag, hv.diff, hv.total, hv.past1,
				hv.maxu, hv.word() );
			std::printf( "FXRESVIEW %s stencil: %ld of %zu samples differ: %s\n", tag, ds, gl.st.size(), ds == 0 ? "EXACT" : "FAIL" );
		}
		return ok;
	};

	struct Arm
	{
		const char * name;
		int scale, mode, seed;
	};
	const Arm arms[2] = { { "A (scale 2, mode 0)", 2, 0, 0 }, { "B (scale 4, mode 1)", 4, 1, 1 } };
	for ( int a = 0; a < 2; a++ ) {
		const Arm & A = arms[a];
		const int w = ( W + A.scale - 1 ) / A.scale, h = ( H + A.scale - 1 ) / A.scale;
		Layer L = glLayer( w, h );
		if ( !L.zFb || !L.layerFb )
			return fatal( "GL layer targets" );
		// GL's passes
		seedFrame( A.seed );
		const std::vector<std::uint16_t> col0 = readCol();
		const std::vector<float> st0 = readSt();
		Out gl;
		gl.ran = true;
		glDown( L, A.scale );
		gl.z = readR32( L.z, w, h );
		gl.depth = readD32( L.depth, w, h );
		clearFill( L.layerFb, w, h, A.seed );
		std::vector<std::uint16_t> colPre;
		if ( std::getenv( "FXRESVIEW_DUMP" ) )
			colPre = readCol();  // diagnostics: the frame right before GL's composite
		glComp( L, A.scale, A.mode );
		gl.col = readCol();
		gl.st = readSt();
		// floors
		const long changed = halfDiffs( gl.col, col0 ), stWritten = bitDiffs( gl.st, st0 );
		const long samples = long( W ) * S * H;
		const bool covOk = changed >= 1000 && samples - changed >= 1000 && stWritten >= 1000 && samples - stWritten >= 1000;
		std::printf( "FXRESVIEW arm %s floor: GL's composite changed %ld of %ld colour samples, rewrote %ld stencil samples (want >= 1000 "
					 "each way): %s\n",
			A.name, changed, samples, stWritten, covOk ? "OK" : "FAIL" );
		gateOk = gateOk && covOk;
		if ( a == 0 ) {
			f.bindFramebuffer( fFb, fbPrb );
			glViewport( 0, 0, W, H );
			drawState0();
			glClearColor( 0.5f, 0.5f, 0.5f, 0.5f );
			glClear( GL_COLOR_BUFFER_BIT );
			bindComp( pProbe, L, A.scale, 0 );
			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
			unbind5();
			std::vector<std::uint16_t> pr( static_cast<size_t>( W ) * H * 4, 0 );
			glReadPixels( 0, 0, W, H, GL_RGBA, fHalf, pr.data() );
			long edges = 0, smooth = 0;
			for ( size_t i = 0; i < pr.size(); i += 4 ) {
				edges += pr[i] == 0x3C00;
				smooth += pr[i] == 0x0000;
			}
			const bool eOk = edges >= 100 && smooth >= 100;
			std::printf( "FXRESVIEW arm %s floor: edge probe %ld edge pixels, %ld bilinear pixels (want >= 100 each): %s\n", A.name, edges,
				smooth, eOk ? "OK" : "FAIL" );
			gateOk = gateOk && eOk;
		}
		// Vulkan's passes from the same start
		seedFrame( A.seed );
		const bool startOk = halfDiffs( readCol(), col0 ) == 0 && bitDiffs( readSt(), st0 ) == 0;
		std::printf( "FXRESVIEW arm %s floor: the start state read twice: %s\n", A.name, startOk ? "EXACT" : "FAIL" );
		gateOk = gateOk && startOk;
		Out vk;
		vkRun( A.scale, A.mode, A.seed, 0, 0, vk );
		std::printf( "FXRESVIEW arm %s green: %s%s\n", A.name, vk.ran ? "ran" : "refused: ", vk.ran ? "" : vk.why.c_str() );
		if ( const char * dump = std::getenv( "FXRESVIEW_DUMP" ) ) {  // diagnostics: start, GL and Vulkan colour (W*S x H RGBA halves)
			const std::vector<std::uint16_t> * v[4] = { &col0, &gl.col, &vk.col, &colPre };
			static const char * const nm[4] = { "start", "gl", "vk", "pre" };
			for ( int k = 0; k < 4; k++ ) {
				const std::string p = std::string( dump ) + "/arm" + char( '0' + a ) + "_" + nm[k] + ".bin";
				if ( FILE * fp = std::fopen( p.c_str(), "wb" ) ) {
					const int hd[3] = { W, H, S };
					std::fwrite( hd, sizeof( hd ), 1, fp );
					std::fwrite( v[k]->data(), 2, v[k]->size(), fp );
					std::fclose( fp );
				}
			}
		}
		const bool g = judge( A.name, gl, vk, col0, true );
		std::printf( "FXRESVIEW arm %s green: %s\n", A.name, g ? "OK" : "FAIL" );
		gateOk = gateOk && g;
		if ( a == 0 ) {
			struct Red
			{
				int down, comp;
				const char * what;
			};
			const Red reds[3] = { { 0, 1, "comp red 1: stencil write mask 0" }, { 0, 2, "comp red 2: blend dst ONE (no SRC1)" },
				{ 4, 0, "down red 4: max instead of min" } };
			for ( const Red & r : reds ) {
				seedFrame( A.seed );
				Out rv;
				vkRun( A.scale, A.mode, A.seed, r.down, r.comp, rv );
				const bool pass = judge( r.what, gl, rv, col0,false );
				const int patched = r.down == 4 ? WwVkPresent::fxresRedPatches() : -1;
				const bool bites = rv.ran && !pass && ( r.down != 4 || patched > 0 );
				std::printf( "FXRESVIEW %s: %s%s, patched words %d, want FAIL: %s\n", r.what, rv.ran ? "ran" : "refused: ",
					rv.ran ? "" : rv.why.c_str(), patched, bites ? "FAIL (OK)" : "TESTS NOTHING" );
				if ( rv.ran )
					judge( r.what, gl, rv, col0,true );
				gateOk = gateOk && bites;
			}
		}
	}
	{
		drawScene( fbOther, 0, W, H );
		WwVkPresent::FxresComp cp;
		cp.owner = own;
		cp.color = otherCol;
		cp.W = W;
		cp.H = H;
		cp.scale = 4;
		cp.src = fullTex;
		cp.srcFmt = fD24s8;
		cp.proj[0] = proj[0];
		cp.proj[1] = proj[1];
		cp.edge = edgeK;
		cp.vp[2] = W;
		cp.vp[3] = H;
		std::string w;
		const bool ran = WwVkPresent::fxresCompRun( cp, w );
		std::printf( "FXRESVIEW refusal arm (a colour that is not the tone frame's): %s%s, want refused: %s\n", ran ? "ran" : "refused: ",
			ran ? "" : w.c_str(), ran ? "FAIL" : "OK" );
		gateOk = gateOk && !ran;
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFxresStat();
	WwVkPresent::FxresTargets T;
	WwVkPresent::fxresTargetsDrop( own );
	const bool gone = !WwVkPresent::fxresTargetsAre( own, T );
	const bool statOk = st.runs == 10 && st.fails == 1 && st.copies == 10 && gone;
	std::printf( "FXRESVIEW stat: %d runs (want 10), %d refused (want 1), %d copies (want 10), %d pipelines in %.1f ms, targets gone after "
				 "drop %s: %s\n",
		st.runs, st.fails, st.copies, st.pipelines, st.buildMs, gone ? "yes" : "no", statOk ? "OK" : "FAIL" );
	gateOk = gateOk && statOk;
	WwVkPresent::toneFrameDrop();
	std::printf( gateOk ? "FXRESVIEW GATE OK\n" : "FXRESVIEW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
