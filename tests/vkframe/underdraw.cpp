// Lane VKWATER4 (2026-11-14, plan row L7): the camera-underwater fog on the present device (vkunder.inc underRun: the
// tone_view_vs triangle, underwater_view_ps then underwater_view_ms_ps per sample, dual-source blended into the tone
// frame) against GL's own programs (cell_hdr.vert + cell_underwater.frag / cell_underwater_ms.frag as the app links them,
// written by waterdraw_prep.py), drawn with cellunderwater.cpp's state into the same 4x MSAA RGBA16F + D24S8 contents.
// fxview.cpp's helpers are reused (its main renamed). uwDepth is made by GL's cell_depthmax.frag from the frame's own
// depth (cellhdr.cpp depthMaxMake), as the app makes it.
// Scene (per sample, gl_SamplePosition): a floor whose depth ramps up the frame, a sky band (depth 1.0) and six discs
// nearer, each with its own colour, so edge pixels hold samples of two depths (the per-sample draw's work).
// Reference (coordinator ruling 11-14: the blend equation is per sample, NVIDIA GL's 4x tile mean is a driver quirk, not
// FO4's look): GL's two programs drawn again with blending off, fragAdd / fragMul moved to two RGBA32F targets (location
// 0 and 1, fragMul's alpha cleared to -1 so an unwritten sample shows), read per sample, then frame = frame * mul + add
// applied per sample by GL's own blend unit at 1x: every sample is one texel of a single-sample RGBA16F target, the
// captures go back as fragAdd / fragMul with the same blend and colour mask, one pass per draw. (A float CPU blend misses
// the blend unit by 1 ulp on ~8 % of halves even on constant outputs, so it stays an info line.) Reference check (bar
// written before the first run): the 1x blend of the captures (of the programs as linked, which GL's 4x frame ran)
// equals GL's 4x blended frame exactly on the pixels GL did not flatten (all four samples one value). The 1x constant probe against the 4x one is info only (ruling 11-14: its
// premise is wrong, GL flattens 2x2 tiles). GL's blended frame is printed against the reference (info).
// The per-sample capture is GL's program with fwidth( Pn.z ) moved before the discard (ruling 11-14: the fixed
// cell_underwater_ps.hlsl draws that order; GLSL's derivative after a discard is undefined, GL's GLSL keeps it).
// Bar (set before the first run, never widened): Vulkan against that reference per sample, every half within 2 ulp and
// <= 0.1 % of halves past 1 ulp (fxview's volsky bar); floors: the fog changes >= 50 % of the samples against the frame
// before it, and the scene holds >= 100 pixels whose samples differ in depth.
// Checks: GL's programs sample uwDepth and uwDepthMs only (the waterInfo / waterFlow images are not active in GL either);
// GL's gl_FragCoord in a per-sample draw is the pixel centre (the per-sample twin takes the centre too);
// arms A (WATR fog law), B (Underwater light row, class 1), C (the meniscus: the near plane crosses the surface).
// Each arm also runs Vulkan's per-pixel draw alone and tables which draw changed each sample (GL: from the reference's
// two passes): Vulkan must fog every sample by one draw at most ("both" 0); a sample the two APIs give to different draws
// prints e against tol in double (uwNearMax's tie; the twin spells GL's float form of it, read from GL's fp assembly).
// Info: GL's per-sample program as linked (the old order); arm C's waterline samples are the ones it moves.
// Arm A also draws GL's blend with constant
// outputs (info, the quirk's record): on this PC GL's 4x blend leaves bare * mul + add in some 2-pixel tiles and writes
// one value to all four samples (the pixel's mean blended), single-source or dual, with or without a written sample mask.
// Reds (each must FAIL, never refuse): 1 no per-sample draw, 2 blend without SRC1 (dst ONE), 8 uwRow one vec4 off (the whole b31 moved uwSamples
// into a loop bound and hung the GPU: device lost on the next submit), 4 the
// uwDepth copy skipped (on a scene whose depth differs from the last copied one), old order (arm C against GL's program as
// linked, fwidth after the discard). A last arm with uwSamples 0 must refuse.
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

// float to half, round to nearest even (the blend's store into RGBA16F)
std::uint16_t f2h( float x )
{
	std::uint32_t b;
	std::memcpy( &b, &x, 4 );
	const std::uint16_t s = std::uint16_t( ( b >> 16 ) & 0x8000 );
	b &= 0x7FFFFFFFu;
	if ( b >= 0x7F800000u )
		return s | 0x7C00 | ( b > 0x7F800000u ? 0x200 : 0 );
	if ( b >= 0x477FF000u )
		return s | 0x7C00;
	if ( b < 0x38800000u ) {
		float v;
		std::memcpy( &v, &b, 4 );
		return s | std::uint16_t( std::nearbyint( v * 16777216.0f ) );
	}
	return s | std::uint16_t( ( b + 0xFFFu + ( ( b >> 13 ) & 1u ) - 0x38000000u ) >> 13 );
}

// the 1x blend pass: one texel per sample, the captured outputs back as the two sources (unwritten samples discarded)
const char * kBlendFs = "#version 410 core\n"
	"uniform sampler2D addT, mulT; layout(location=0,index=0) out vec4 fa; layout(location=0,index=1) out vec4 fm;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ); vec4 m = texelFetch( mulT, q, 0 ); if ( m.a < 0.0 ) discard;\n"
	"  fa = texelFetch( addT, q, 0 ); fm = m; }\n";

// a probe copy of the per-sample program: fwidth( Pn.z ) taken before the discard, as the HLSL twin takes it (GLSL's
// derivative after a discard in non-uniform flow is undefined)
std::string fwidthFirst( std::string fs, bool & ok )
{
	const std::string n = "vec2 ndc = ( gl_FragCoord.xy - uwVp.xy ) / uwVp.zw * 2.0 - 1.0;", w = "fwidth( Pn.z )";
	ok = fs.find( n ) != std::string::npos && fs.find( w ) != std::string::npos;
	if ( ok ) {
		fs.replace( fs.find( w ), w.size(), "uwPnW" );
		fs.insert( fs.find( n ) + n.size(), " float uwPnW = fwidth( uwWorld( ndc, -1.0 ).z );" );
	}
	return fs;
}

// the capture programs: fragAdd / fragMul to locations 0 and 1 instead of one location's two indices
std::string captureFs( std::string fs, bool & ok )
{
	const std::string a = "layout ( location = 0, index = 0 ) out vec4 fragAdd;", m = "layout ( location = 0, index = 1 ) out vec4 fragMul;";
	const size_t pa = fs.find( a ), pm = fs.find( m );
	ok = pa != std::string::npos && pm != std::string::npos;
	if ( ok ) {
		fs.replace( fs.find( m ), m.size(), "layout ( location = 1 ) out vec4 fragMul;" );
		fs.replace( fs.find( a ), a.size(), "layout ( location = 0 ) out vec4 fragAdd;" );
	}
	return fs;
}

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
	bool capOk[2] = {};
	const GLuint pUwC = program( hdrVs, captureFs( readFile( dir + "/uw/prog.frag.glsl" ), capOk[0] ), "cell_underwater capture" );
	const GLuint pMsC = program( readFile( dir + "/uwms/prog.vert.glsl" ), captureFs( readFile( dir + "/uwms/prog.frag.glsl" ), capOk[1] ),
		"cell_underwater_ms capture" );
	if ( !capOk[0] || !capOk[1] || !pUwC || !pMsC )
		return fatal( "capture programs (fragAdd / fragMul declarations not found)" );
	bool fwOk = false;
	const GLuint pMsFw = program( readFile( dir + "/uwms/prog.vert.glsl" ),
		fwidthFirst( captureFs( readFile( dir + "/uwms/prog.frag.glsl" ), capOk[1] ), fwOk ), "cell_underwater_ms capture, fwidth first" );
	if ( !fwOk || !pMsFw )
		return fatal( "probe program (fwidth before the discard)" );
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
	const GLuint capAdd = msTex( W, H, fRgba32f, S ), capMul = msTex( W, H, fRgba32f, S ), fbCap = fbo( fT2DMS, capAdd, capMul, 0 );
	const GLuint bl1 = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false ), fbBl1 = fbo( GL_TEXTURE_2D, bl1, 0, 0 );
	const GLuint addT = tex2d( WS, H, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false ), mulT = tex2d( WS, H, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint pBlend = program( kTriVs, kBlendFs, "1x blend" );
	const GLuint unpF = tex2d( WS, H, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false ), fbUnpF = fbo( GL_TEXTURE_2D, unpF, 0, 0 );
	using ClearBufF = void( APIENTRY * )( GLenum, GLint, const GLfloat * );
	const auto clearBufferfv = reinterpret_cast<ClearBufF>( glProc( "glClearBufferfv" ) );
	if ( !fbFrame || !fbFrameC || !fbRef || !fbRefC || !fbMax || !fbUnp || !fbUnpD || !fbCap || !fbUnpF || !clearBufferfv || !fbBl1 || !pBlend )
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
	auto readSamplesF = [&]( GLuint msColor ) {
		std::vector<float> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnpF );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 15, fT2DMS, msColor );
		u1i( pUnpack, "src", 15 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, GL_FLOAT, out.data() );
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
		for ( GLuint p : { pUw, pMs, pUwC, pMsC, pMsFw } ) {
			f.useProgram( p );
			auto L =[&]( const char * n ) { return f.getUniformLocation( p, n ); };
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
	// GL's two programs with blending off: per draw, fragAdd / fragMul per sample (fragMul alpha -1 where not written)
	struct Cap { std::vector<float> add, mul; };
	auto glCapture = [&]( GLuint ds, GLuint prog ) {
		f.bindFramebuffer( fFb, fbCap );
		glViewport( 0, 0, W, H );
		drawState0();
		const float z[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, none[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
		clearBufferfv( GL_COLOR, 0, z );
		clearBufferfv( GL_COLOR, 1, none );
		f.activeTexture( fTex0 + 1 );
		glBindTexture( GL_TEXTURE_2D, maxTex );
		f.activeTexture( fTex0 + 2 );
		glBindTexture( fT2DMS, ds );
		ex.texParameteri( fT2DMS, kDsMode, kDepthComp );
		f.useProgram( prog );
		drawQuad();
		ex.texParameteri( fT2DMS, kDsMode, kStencilIdx );
		glBindTexture( fT2DMS, 0 );
		f.activeTexture( fTex0 + 1 );
		glBindTexture( GL_TEXTURE_2D, 0 );
		f.activeTexture( fTex0 );
		Cap c;
		c.add = readSamplesF( capAdd );
		c.mul = readSamplesF( capMul );
		return c;
	};
	// the blend equation per sample by GL's blend unit at 1x: the frame's samples as texels of bl1, then one pass of prog
	// (kBlendFs with the captures, or a constant probe) with cellunderwater.cpp's blend and colour mask
	auto blend1x = [&]( const std::vector<std::uint16_t> & fr, GLuint prog, const Cap * c ) {
		glBindTexture( GL_TEXTURE_2D, bl1 );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, WS, H, GL_RGBA, fHalf, fr.data() );
		if ( c ) {
			glBindTexture( GL_TEXTURE_2D, addT );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, WS, H, GL_RGBA, GL_FLOAT, c->add.data() );
			glBindTexture( GL_TEXTURE_2D, mulT );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, WS, H, GL_RGBA, GL_FLOAT, c->mul.data() );
		}
		glBindTexture( GL_TEXTURE_2D, 0 );
		f.bindFramebuffer( fFb, fbBl1 );
		glViewport( 0, 0, WS, H );
		drawState0();
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
		glEnable( GL_BLEND );
		f.blendFuncSeparate( GL_ONE, kSrc1Color, GL_ZERO, GL_ONE );
		f.useProgram( prog );
		if ( c ) {
			bindUnit( 13, GL_TEXTURE_2D, addT );
			bindUnit( 14, GL_TEXTURE_2D, mulT );
			u1i( prog, "addT", 13 );
			u1i( prog, "mulT", 14 );
		}
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		std::vector<std::uint16_t> out( fr.size() );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		bindUnit( 13, GL_TEXTURE_2D, 0 );
		bindUnit( 14, GL_TEXTURE_2D, 0 );
		return out;
	};
	// the same equation in float on the CPU (info only: it misses the blend unit by 1 ulp on some halves)
	auto cpuBlend = [&]( std::vector<std::uint16_t> fr, const Cap & c ) {
		for ( size_t i = 0; i < fr.size(); i += 4 )
			if ( c.mul[i + 3] >= 0.0f )
				for ( int k = 0; k < 3; k++ )
					fr[i + k] = f2h( h2f( fr[i + k] ) * c.mul[i + k] + c.add[i + k] );
		return fr;
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
		const std::vector<std::uint16_t> vk = readSamples( fc ), glb = readSamples( refCol );
		// the per-sample capture from the fwidth-first program (what the HLSL draws, ruling 11-14); GL's GLSL as linked kept
		const Cap c1 = glCapture( refDs, pUwC ), c2 = a.samples > 0 ? glCapture( refDs, pMsFw ) : Cap {};
		const std::vector<std::uint16_t> ref1 = blend1x( bare, pBlend, &c1 ), ref = a.samples > 0 ? blend1x( ref1, pBlend, &c2 ) : ref1;
		const Halves h = compareHalves( vk, ref );
		std::vector<char> glFlat( size_t( W ) * H );  // GL holds one RGB in all four samples of the pixel
		for ( size_t p = 0; p < glFlat.size(); p++ ) {
			bool same = true;
			for ( int k = 1; k < S; k++ )
				for ( int c = 0; c < 3; c++ )
					same = same && glb[( p * S + k ) * 4 + c] == glb[p * S * 4 + c];
			glFlat[p] = same;
		}
		bool refOk = true;
		if ( a.red == 0 ) {  // the reference's own check, then GL's blended frame against it (the tile mean, on record)
			// the 1x blend method is checked on the captures of the programs GL's 4x frame ran (as linked, old order)
			const Cap cl = a.samples > 0 ? glCapture( refDs, pMsC ) : Cap {};
			const std::vector<std::uint16_t> refl = a.samples > 0 ? blend1x( ref1, pBlend, &cl ) : ref1;
			size_t outside = 0, ne = 0;
			for ( size_t i = 0; i < refl.size(); i++ )
				if ( !glFlat[i / 4 / S] ) {
					outside++;
					ne += refl[i] != glb[i];
				}
			refOk = outside > 0 && ne == 0;
			std::printf( "UNDERDRAW   reference: 1x blend of the as-linked captures vs GL's 4x frame outside flattened pixels: %zu of %zu halves differ (want 0): %s\n",
				ne, outside, refOk ? "OK" : "FAIL" );
			const Halves cb = compareHalves( a.samples > 0 ? cpuBlend( cpuBlend( bare, c1 ), c2 ) : cpuBlend( bare, c1 ), ref );
			std::printf( "UNDERDRAW   float CPU blend vs the reference (info): halves %zu differ, %zu past 1 ulp, max %d ulp\n", cb.diff, cb.past1, cb.maxu );
			const Halves hl = compareHalves( vk, refl ), mv = compareHalves( refl, ref );
			std::printf( "UNDERDRAW   GL per-sample program as linked, fwidth after the discard (info): Vulkan vs it %s (halves %zu differ, %zu past 1 ulp, "
						 "max %d ulp); halves the old order moves from the reference %zu (max %d ulp)\n", hl.word(), hl.diff, hl.past1, hl.maxu, mv.diff, mv.maxu );
			if ( a.seed == 2 ) {  // red: the old order (GL's GLSL as linked) as the reference must FAIL on the meniscus arm
				const bool failed = ran && !hl.ok();
				std::printf( "UNDERDRAW red old order (fwidth after the discard, arm C): %s (halves %zu differ, %zu past 1 ulp, max %d ulp), want FAIL: %s\n",
					ran ? ( hl.ok() ? hl.word() : "FAIL" ) : "REFUSED", hl.diff, hl.past1, hl.maxu, failed ? "OK" : "NOT ARMED" );
				gateOk = gateOk && failed;
			}
			const Halves g = compareHalves( glb, ref );
			size_t flat = 0;  // pixels GL flattened where the reference does not hold one value
			for ( size_t p = 0; p < glFlat.size(); p++ ) {
				bool rSame = true;
				for ( int k = 1; k < S; k++ )
					for ( int c = 0; c < 3; c++ )
						rSame = rSame && ref[( p * S + k ) * 4 + c] == ref[p * S * 4 + c];
				flat += glFlat[p] && !rSame;
			}
			std::printf( "UNDERDRAW   GL blended frame vs the per-sample reference (info): %s (halves %zu differ, %zu past 1 ulp, max %d ulp); "
						 "pixels GL flattened to one value %zu\n", g.word(), g.diff, g.past1, g.maxu, flat );
		}
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
		// which draw fogged each sample, per API (0 neither, 1 per-pixel, 2 per-sample, 3 both): GL, which draw wrote the
		// sample in the capture; Vulkan, against its per-pixel draw alone (red 1). Vulkan "both" must be 0 (the twins agree
		// on uwNearMax); the table, with the samples past 1 ulp per cell, shows where GL and Vulkan part
		bool onceOk = true;
		if ( a.red == 0 ) {
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
				const int g = ( rgbNe( ref1, bare, i ) ? 1 : 0 ) | ( rgbNe( ref, ref1, i ) ? 2 : 0 );
				const int v = ( rgbNe( ppV, bare, i ) ? 1 : 0 ) | ( rgbNe( vk, ppV, i ) ? 2 : 0 );
				cross[g][v]++;
				if ( ( g == 1 && v == 2 ) || ( g == 2 && v == 1 ) ) {  // the two APIs split on uwNearMax: e against tol in double
					const size_t smp = i / 4, y = smp / WS, xs = smp % WS, px = xs / S;
					double dm = 0.0;
					for ( int k = 0; k < S; k++ )
						dm = std::max( dm, double( depthS[y * WS + px * S + k] ) );
					const double nx = ( px + 0.5 ) / W * 2.0 - 1.0, ny = ( y + 0.5 ) / H * 2.0 - 1.0;
					auto world = [&]( double z ) {
						const double vw = ( N - F ) / ( 2.0 * F * N ) * z + ( F + N ) / ( 2.0 * F * N );
						const double p[3] = { asp / fy * nx / vw, 1.0 / fy * ny / vw, -1.0 / vw };
						return std::array<double, 3> { -p[2] + 100.0, -p[0] + 200.0, p[1] + double( a.eyeZ ) };
					};
					const std::array<double, 3> pa = world( dm * 2.0 - 1.0 ), pb = world( double( depthS[smp] ) * 2.0 - 1.0 );
					const double e = std::sqrt( ( pa[0] - pb[0] ) * ( pa[0] - pb[0] ) + ( pa[1] - pb[1] ) * ( pa[1] - pb[1] ) + ( pa[2] - pb[2] ) * ( pa[2] - pb[2] ) );
					std::printf( "UNDERDRAW   split x %zu y %zu s %zu: gl draw %d, vk draw %d; e %.7f world units, tol %s %.7f\n", px, y, xs % S, g, v, e,
						a.light ? "(light row, not computed)" : "", a.light ? 0.0 : 1500.0 / ( 512.0 * 0.9f ) );
				}
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
			if ( !h.ok() && !a.light && cross[1][2] + cross[2][1] > 0 )  // still counted as failing; the bar is not widened
				std::printf( "UNDERDRAW   known: uwNearMax threshold ties, double agrees with neither side (%zu samples split, e / tol on the split lines)\n",
					cross[1][2] + cross[2][1] );
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
					auto offAt = [&]( const std::vector<std::uint16_t> & x, size_t j ) {  // off by more than 2 ulp at the value
						const float want = h2f( bare[j] ) * 0.65f + 0.01f;
						return std::fabs( h2f( x[j] ) - want ) > std::max( 0.002f * want, 1e-4f );
					};
					size_t off = 0, n = 0;
					for ( size_t j = 0; j < pp.size(); j += 4 ) {
						off += offAt( pp, j );
						n++;
					}
					std::printf( "UNDERDRAW   GL blend %s (info): %zu of %zu samples off bare * 0.65 + 0.01\n", v.n, off, n );
					if ( v.p == qNo ) {  // the reference's blend unit check: the same probe at 1x
						const GLuint p1x = program( kTriVs, fsNoMask, "probe 1x" );
						const std::vector<std::uint16_t> q = blend1x( bare, p1x, nullptr );
						size_t off1 = 0, kept = 0, ne = 0;
						for ( size_t j = 0; j < q.size(); j += 4 ) {
							off1 += offAt( q, j );
							if ( !offAt( pp, j ) ) {
								kept++;
								for ( int c = 0; c < 4; c++ )
									ne += q[j + c] != pp[j + c];
							}
						}
						// retired as a verdict (ruling 11-14): its premise, 4x keeps a sample's own blend outside the off ones, is
						// wrong: GL flattens whole 2x2 tiles. The reference check is the captures' 1x blend vs GL's 4x frame above.
						std::printf( "UNDERDRAW   reference (info, premise wrong: GL flattens 2x2 tiles): GL blend at 1x, same probe: %zu samples off, "
									 "%zu halves differ from 4x on the %zu samples 4x kept%s\n", off1, ne, kept, p1x ? "" : " (probe 1x not linked)" );
						size_t neFlat = 0, neRest = 0, flatPx = 0;  // where those halves sit: pixels 4x flattened (one RGB in all four samples)
						for ( size_t p = 0; p < size_t( W ) * H; p++ ) {
							bool same = true;
							for ( int k = 1; k < S; k++ )
								for ( int c = 0; c < 3; c++ )
									same = same && pp[( p * S + k ) * 4 + c] == pp[p * S * 4 + c];
							flatPx += same;
							for ( int k = 0; k < S; k++ ) {
								const size_t j = ( p * S + k ) * 4;
								if ( !offAt( pp, j ) )
									for ( int c = 0; c < 4; c++ )
										( same ? neFlat : neRest ) += q[j + c] != pp[j + c];
							}
						}
						std::printf( "UNDERDRAW   reference (info): of those halves, %zu in the %zu pixels 4x flattened, %zu elsewhere\n", neFlat, flatPx, neRest );
					}
				}
			}
			int shown = 0;
			for ( size_t i = 0; i < vk.size() && shown < 20 && !h.ok() && a.red == 0; i += 4 ) {
				int d = 0;
				for ( int c = 0; c < 3; c++ )
					d = std::max( d, std::abs( ordered( vk[i + c] ) - ordered( ref[i + c] ) ) );
				if ( d > 1 ) {
					const size_t smp = i / 4, y = smp / WS, xs = smp % WS;
					float dm = 0.0f;
					for ( int k = 0; k < S; k++ )
						dm = std::max( dm, depthS[y * WS + ( xs / S ) * S + k] );
					std::printf( "UNDERDRAW   x %zu y %zu s %zu %s depth %.7f pixel max %.7f: vk %.4f %.4f %.4f ref %.4f %.4f %.4f bare %.4f\n",
						xs / S, y, xs % S, mixMap[y * W + xs / S] ? "mixed" : "plain", double( depthS[smp] ), double( dm ), double( h2f( vk[i] ) ),
						double( h2f( vk[i + 1] ) ), double( h2f( vk[i + 2] ) ), double( h2f( ref[i] ) ), double( h2f( ref[i + 1] ) ),
						double( h2f( ref[i + 2] ) ), double( h2f( bare[i] ) ) );
					std::printf( "UNDERDRAW     gl capture r: per-pixel %s add %.5f mul %.5f, per-sample %s add %.5f mul %.5f; GL blended %.4f\n",
						c1.mul[i + 3] >= 0.0f ? "wrote" : "skipped", double( c1.add[i] ), double( c1.mul[i] ),
						c2.mul.empty() || c2.mul[i + 3] < 0.0f ? "skipped" : "wrote", c2.mul.empty() ? 0.0 : double( c2.add[i] ),
						c2.mul.empty() ? 0.0 : double( c2.mul[i] ), double( h2f( glb[i] ) ) );
					shown++;
				}
			}
		if ( a.red == 0 ) {
			const bool ok = ran && h.ok() && floorOk && onceOk && refOk;
			std::printf( "UNDERDRAW arm %s: %s against the per-sample reference (halves %zu differ, %zu past 1 ulp, max %d ulp of %zu); fogged %zu / "
						 "%zu samples, mixed-depth pixels %d (floors 50 %%, 100); Vulkan fogs each sample once: %s; reference checks %s%s%s: %s\n",
				a.name, ran ? h.word() : "REFUSED", h.diff, h.past1, h.maxu, h.total, fogged, total, mixed, onceOk ? "yes" : "NO", refOk ? "OK" : "FAIL",
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
