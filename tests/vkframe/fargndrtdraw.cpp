// Lane VKGROUND10 (2026-11-16): the far ring 0's live ground (fo4_cellcsmfgl, shader decals, mode 1) under the RT GI keys the
// all-on route links: RT = FOGLEAK + RESTIR + RTDDGI + RTDDGI_CASCADES + FRAME_UBO (twin 1), DG = the same without RESTIR (twin 2).
// GENERATED from fargrounddraw.cpp by E:/Projects/Claude/notes/vkground9/tools/mk_fargndrt.py (the ground fixture: fgGrid, fgLtex,
// fgDecal, fgAtlas, shared targets, cellRow, fgMode 1, decals) with farlodrtdraw.cpp's RT machinery. One key a run:
// fargndrtdraw.exe <prep dir> rt|dg (fargndrtdraw.sh runs both). GL's side is the HLSL-built SPIR-V of that key; the legacy
// .frag of the same key gives the sampler types and the fill order. Every Vulkan call goes through one lambda: with
// WW_VK_FARGNDRT (defined by vkfargroundrt.inc) farGroundRtStateOf / farGroundRtRun / takeFarGroundRtDrawStat( v ); without it
// every arm is REFUSED "no twin" and the gate FAILS (step 1: the bar before the twin).
//
// Space: cellRow maps view onto the grid's world square, world = (x + 5000, -z + 7000, y + 300) (the camera at world
// (5000, 7000, 300)). cellRestir = (ratio, linear view depth) per pixel, as the far LOD gate; the DDGI box in WORLD units,
// origin (4300, 7100, 100) step (175, 250, 25), 9 x 7 x 8 probes (one cascade = the same box); the gi-half key's xyz is the
// WORLD position of GL's own sample 0 (cell_gihalfat.hlsli compares against P in world, tol from distance( P, cellRow.w )),
// its w the octahedral code of GL's own shading normal (cellProbe 1, landSplat 0).
// Seeding: farlodrtdraw's switch profile (FLD_ON, cellClusterOn off, cellRestirOn / cellDdgiOn / cellGiSky / cellLightCount /
// cellExtDalc on), cellGiLoop8 8, cellGiAmb 1, cellDdgiCount 1, light 0 a shadow-casting omni over the ground (radius 5000).
// Seed 1 zeroes the vec4-earlier neighbours of the four int loop bounds (red 1 reads the block one vec4 off: a seeded bound
// hung the device on VKFAR29), read by offset from vkfargroundrt_tables.h for both keys: cellCenter.y (-> cellAmboCount),
// cellRoomsDims.y (-> cellGiFineN), cellCtRect.y (-> cellGiLoop8), cellClusterLog.x (-> cellDdgiCount).
// Seed 1 also sets the grade's contrast (cellIsCine.y) to 1 (VKGROUND16): its seeded 0.156 squeezed every lighting change ~6x,
// so the DDGI reds (atlas one texel off, DG key on the base twin) stayed under 1e-2 although Vulkan read the wrong data.
//
// The bar, per key (seeds 1, 2; reds on seed 1):
// - cellRestir (RT only) and cellDdgiAtlas are rtBufShare'd buffers GL binds by name; each seed reads the shared bytes back
//   through rtBufOf + farTexelBufferRead: 0 bytes differ, the GL buffer is the bound one.
// - GL probes before any Vulkan call: cellRestirOn 0 (RT), cellDdgiOn 0 each move >= 20 % of GL's covered samples;
//   >= 1000 distinct colours over the ground.
// - red 0 BOUND (fargrounddraw's judge) on both seeds; seed 1 the same draw on plain twins of fgGrid / fgLtex / fgAtlas: exactly
//   3 copies more a run than on the shared ones (the three read in place; the RT buffers never copied).
// - reds that must FAIL: red 1 (pixel block one vec4 off), restir (rewritten after GL drew, RT only), atlas (shifted one texel
//   after GL drew); red key (the other twin: RT key on the DG twin, DG key on the base twin) FAILS or is REFUSED.
// - fgLtex anisotropy (manager ruling 11-16, the reason at kLtexAnisoPix): every pixel compare runs it at 1 on both sides; the
//   state arm (seed 1) sets the app's 8 and checks every census input's sampling state side by side, GL's own texture state
//   on the unit vs the twin's sampler / view from the slot farGroundRtStateOf fills: filter, wrap, compare, levels, LOD,
//   anisotropy all equal, fgLtex 8 on both; red: the twin's fgLtex anisotropy 4 must differ.
// - all-on arm (seed 1): cellGiHalfOn 1 on the rtImgShare'd cellGiHalf / cellGiHalfKey (W x H); key >= 1000 texels on the
//   ground, >= 90 % decode to a unit normal; GL probes cellGiHalfOn 0 / cellGiHalf reseeded each move >= 20 %; clean BOUND,
//   then after EACH red a clean run BOUND (6 clean); reds: not shared (REFUSED naming cellGiHalf:), wrong image (FAIL or
//   REFUSED), stale gen (rtImgGen moves, not BOUND), key mismatch (borrowTarget( 16 ) on cellGiHalf's GL name: REFUSED "the
//   shared image is not the bound name's"), no sampler (REFUSED "GL's program has no such sampler"): 5 of 5 failing.
// - half-rate arm (seed 1): both images at rtImgSize( W, 2 ) x rtImgSize( H, 2 ), rect (0, 0, hw, hh), scale 0.5, key texel h =
//   pixel min( 2h, W - 1 ): key >= 250 texels, >= 90 % unit; GL probes cellGiHalfOn 0 / reseeded / cellGiHalfScale 1 each
//   >= 20 %; clean BOUND, red half rate ignored (cellGiHalfScale 1 in the twin's block) FAILS, clean after it BOUND.
// Last line `FARGNDRTDRAW GATE OK` or `FARGNDRTDRAW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

#include "features/vkframe/vkrtbuf.h"
#include <functional>

namespace fgd
{
using namespace fv;
using namespace fld;

const int kGn = 16, kBins = 4, kRec = 8, kBinOff = kRec * 5, kIdOff = kBinOff + kBins * kBins;
const float kOx = 4300.0f, kOy = 7000.0f, kSpacing = 128.0f, kBin = 512.0f, kTiling = 400.0f;
const int kDdgiDims[4] = { 9, 7, 8, 9 * 7 * 8 };
const size_t kProbes = size_t( 9 * 7 * 8 ), kAtlasTexels = 193 * kProbes;
const float kWx = 5000.0f, kWy = 7000.0f, kWz = 300.0f;	// world = (x + kWx, -z + kWy, y + kWz)
// manager ruling 11-16: NV GL vs NV VK 8x anisotropic filtering differs on partly covered primitives (VKGROUND15 proof); pixel compare at 1, state checked at 8
const float kLtexAnisoPix = 1.0f, kLtexAnisoApp = 8.0f;	// fgLtex: the pixel compare's (both sides), the app's (cellfarground.cpp, the state arm)

size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	size_t n = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
}

std::uint32_t pack4x8( float a, float b, float c, float d )
{
	auto q = []( float v ) { return std::uint32_t( std::lround( std::clamp( v, 0.0f, 1.0f ) * 255.0f ) ); };
	return q( a ) | ( q( b ) << 8 ) | ( q( c ) << 16 ) | ( q( d ) << 24 );
}

// the grid: per sample ids 0|1, ids 2|3 (65535 = the engine default's grey), weights unorm8 x 4, colour RGB8 + page 255
std::vector<std::uint32_t> gridData( std::mt19937 & rng, bool redo )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<std::uint32_t> g( size_t( kGn * kGn * 4 ) );
	for ( int k = 0; k < kGn * kGn; k++ ) {
		std::uint32_t id[4];
		for ( auto & i : id )
			i = ( rng() & 7u ) == 7u ? 65535u : rng() & 3u;
		float w[4];
		for ( auto & x : w )
			x = ( rng() & 3u ) == 0u ? 0.0f : u01( rng );
		if ( redo )	// red 128: the weights turned round
			std::swap( w[0], w[3] ), std::swap( w[1], w[2] );
		g[size_t( k * 4 )] = id[0] | ( id[1] << 16 );
		g[size_t( k * 4 + 1 )] = id[2] | ( id[3] << 16 );
		g[size_t( k * 4 + 2 )] = pack4x8( w[0], w[1], w[2], w[3] );
		g[size_t( k * 4 + 3 )] = pack4x8( 0.5f + 0.5f * u01( rng ), 0.5f + 0.5f * u01( rng ), 0.5f + 0.5f * u01( rng ), 1.0f );
	}
	return g;
}

// the decal buffer: kRec records of 5 texels (position + scale, rotation, box xy, box z + texel size, atlas rect), then
// (start, count) a bin, then the record ids 4 a texel; every bin lists every record (the walk's box test picks)
std::vector<float> decalData( std::mt19937 & rng, float shift, int texels )
{
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<float> d( size_t( texels * 4 ), 0.0f );
	for ( int r = 0; r < kRec; r++ ) {
		float * p = &d[size_t( r * 5 * 4 )];
		const float a = r % 3 == 0 ? 0.6f * float( r + 1 ) : 0.0f, sz = 0.8f + 0.6f * u01( rng );
		const float px = 4450.0f + 1100.0f * u01( rng ) + shift, py = 7200.0f + 1300.0f * u01( rng );
		const float rec[20] = { px, py, 300.0f, sz, 0.0f, 0.0f, std::sin( 0.5f * a ), std::cos( 0.5f * a ),
			-150.0f - 60.0f * u01( rng ), -110.0f, 150.0f, 110.0f + 50.0f * u01( rng ), -600.0f, 600.0f, 1.0f, 0.0f,
			0.05f + 0.2f * u01( rng ), 0.05f + 0.2f * u01( rng ), 0.6f + 0.3f * u01( rng ), 0.6f + 0.3f * u01( rng ) };
		std::copy( rec, rec + 20, p );
	}
	for ( int b = 0; b < kBins * kBins; b++ ) {
		d[size_t( ( kBinOff + b ) * 4 )] = float( b * kRec );
		d[size_t( ( kBinOff + b ) * 4 + 1 )] = float( kRec );
	}
	for ( int k = 0; k < kBins * kBins * kRec; k++ )
		d[size_t( kIdOff * 4 + k )] = float( ( k + k / kRec ) % kRec );	// each bin its own record order
	return d;
}

// BATCH1117: GL's program from the HLSL-built SPIR-V pair (fargrounddraw_prep.py), as the app's wwSpvAttach links it
// (glcontext.cpp): glShaderBinary 0x9551 + glSpecializeShader("main"); the blocks keep their decorated bindings.
GLuint spvProgram( const std::string & vsBin, const std::string & psBin )
{
	using ShaderBinary = void( WINAPI * )( GLsizei, const GLuint *, GLenum, const void *, GLsizei );
	using Specialize = void( WINAPI * )( GLuint, const char *, GLuint, const GLuint *, const GLuint * );
	auto bin = reinterpret_cast<ShaderBinary>( wglGetProcAddress( "glShaderBinary" ) );
	auto spec = reinterpret_cast<Specialize>( wglGetProcAddress( "glSpecializeShader" ) );
	if ( !bin || !spec || vsBin.size() < 20 || psBin.size() < 20 ) {
		std::printf( "FARGNDRTDRAW SPIR-V: %s\n", !bin || !spec ? "no glShaderBinary / glSpecializeShader" : "no prog.vert.spv / prog.frag.spv" );
		return 0;
	}
	const GLuint p = f.createProgram();
	const std::string * src[2] = { &vsBin, &psBin };
	for ( int s = 0; s < 2; s++ ) {
		const GLuint sh = f.createShader( s ? fFrag : fVert );
		bin( 1, &sh, 0x9551, src[s]->data(), GLsizei( src[s]->size() ) );
		spec( sh, "main", 0, nullptr, nullptr );
		GLint ok = 0;
		f.getShaderiv( sh, fCompiled, &ok );
		if ( !ok ) {
			std::vector<char> log( 65536 );
			f.getShaderInfoLog( sh, GLsizei( log.size() ), nullptr, log.data() );
			std::printf( "FARGNDRTDRAW SPIR-V %s stage refused: %.1500s\n", s ? "pixel" : "vertex", log.data() );
			return 0;
		}
		f.attachShader( p, sh );
	}
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, fLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "FARGNDRTDRAW SPIR-V link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}
}  // namespace fgd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace fgd;
	gTag = "FARGNDRTDRAW";
	if ( argc < 3 ) {
		std::printf( "usage: fargndrtdraw <prep dir> rt|dg\nFARGNDRTDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARGNDRTDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const std::string keyTag = argv[2];
	const int kv = keyTag == "rt" ? 1 : keyTag == "dg" ? 2 : 0;
	if ( !kv )
		return fatal( "the key is rt or dg, not " + keyTag );
	const char * tag = kv == 1 ? "RT" : "DG";
	const char * keyName = kv == 1 ? "HLSL-built SPIR-V fo4_cellcsmfgl@WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO"
		: "HLSL-built SPIR-V fo4_cellcsmfgl@WW_CELL_FOGLEAK+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO";
	std::printf( "FARGNDRTDRAW key %s (twin %d)\n", tag, kv );
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfargndrtdraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfargndrtdraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfargndrtdraw", "far ground RT draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "FARGNDRTDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	// BATCH1117: GL's side is the HLSL-built program (the HLSL is the reference); the legacy .frag only names the sampler
	// types SPIR-V does not carry (a shadow sampler reads as its plain type) and is run instead with FARGNDRTDRAW_LEGACY=1
	const char * legacyEnv = std::getenv( "FARGNDRTDRAW_LEGACY" );
	const bool legacy = legacyEnv && std::string( legacyEnv ) == "1";
	const GLuint pLegacy = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog_" + keyTag + ".frag.glsl" ), "fo4_cellcsmfgl legacy" );
	const GLuint pFg = legacy ? pLegacy : spvProgram( readFile( dir + "/prog.vert.spv" ), readFile( dir + "/prog_" + keyTag + ".frag.spv" ) );
	std::printf( "FARGNDRTDRAW GL program: %s (legacy fo4_cellcsmfgl.frag %s, info only)\n",
		legacy ? "the legacy fo4_cellcsmfgl.frag (FARGNDRTDRAW_LEGACY=1)" : keyName,
		pLegacy ? "links" : "does not link" );
	if ( !pUnpack || !pDepth || !pSlab || !pFg || !pLegacy )
		return fatal( "GL programs" );
	std::map<std::string, GLenum> legacyType;
	std::vector<std::string> legacyOrder;	// BATCH1118: the textures are filled in this order (the legacy run's rng stream)
	{
		GLint n = 0;
		f.getProgramiv( pLegacy, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pLegacy, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			legacyType[nm] = ty;
			legacyOrder.push_back( nm );
		}
	}
	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint refCol = msTex( W, H, fRgba16f, S ), refDs = msTex( W, H, fD24s8, S );
	const GLuint fbRef = fbo( fT2DMS, refCol, 0, refDs );
	const GLuint unp = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0, 0 );
	const GLuint unpD = tex2d( WS, H, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnpD = fbo( GL_TEXTURE_2D, unpD, 0, 0 );
	if ( !fbFrame || !fbRef || !fbUnp || !fbUnpD )
		return fatal( "GL targets" );
	// lane VKGROUND10: the RT inputs as the app's owners make them (vkrtbuf.h): cellRestir RG32F a pixel, cellDdgiAtlas RGBA32F
	// 193 texels a probe (rtBufShare, one owner a slot), the screen GI's two images at full size (rtImgShare)
	const size_t rsBytes = size_t( W ) * size_t( H ) * 8, atBytes = kAtlasTexels * 16;
	unsigned rsBuf = 0, rsTex = 0, atBuf = 0, atTex = 0;
	if ( !WwVkPresent::rtBufShare( WwVkPresent::kRtBufRestir, 0x8230 /* GL_RG32F */, rsBytes, rsBuf, rsTex, why ) )
		return fatal( "rtBufShare cellRestir: " + why );
	if ( !WwVkPresent::rtBufShare( WwVkPresent::kRtBufDdgiAtlas, 0x8814 /* GL_RGBA32F */, atBytes, atBuf, atTex, why ) )
		return fatal( "rtBufShare cellDdgiAtlas: " + why );
	WwVkPresent::rtBufOwner( WwVkPresent::kRtBufRestir, []( std::string & ) { return true; } );
	WwVkPresent::rtBufOwner( WwVkPresent::kRtBufDdgiAtlas, []( std::string & ) { return true; } );
	static char giOwnerKeys[2];
	auto giKeyOf = [&]( int slot ) { return reinterpret_cast<std::uintptr_t>( &giOwnerKeys[slot] ); };
	unsigned giTex[2] = { 0, 0 };
	for ( int slot = 0; slot < 2; slot++ ) {
		if ( !WwVkPresent::rtImgShare( slot, giKeyOf( slot ), W, H, 0, 0, giTex[slot], why ) )
			return fatal( std::string( "rtImgShare " ) + WwVkPresent::rtImgName( slot ) + ": " + why );
		WwVkPresent::rtImgOwner( slot, []( std::string & ) { return true; } );
	}
	using ClearTexImage = void( WINAPI * )( GLuint, GLint, GLenum, GLenum, const void * );
	const auto clearTex = reinterpret_cast<ClearTexImage>( wglGetProcAddress( "glClearTexImage" ) );
	if ( !clearTex )
		return fatal( "glClearTexImage" );
	std::printf( "FARGNDRTDRAW shared: cellRestir %zu bytes (buffer %u), cellDdgiAtlas %zu bytes (buffer %u), cellGiHalf texture %u, "
		"cellGiHalfKey texture %u (%dx%d RGBA32F)\n", rsBytes, rsBuf, atBytes, atBuf, giTex[0], giTex[1], W, H );
	auto bufWrite = [&]( GLuint buf, const void * data, size_t bytes ) {
		f.bindBuffer( 0x8C2A, buf );
		ex.bufferSubData( 0x8C2A, 0, std::ptrdiff_t( bytes ), data );
		f.bindBuffer( 0x8C2A, 0 );
	};
	// the shared bytes as the present device sees them (rtBufOf -> farTexelBufferRead) against what the gate wrote
	auto sharedCheck = [&]( const char * name, unsigned buf, const std::vector<std::uint8_t> & want ) {
		WwVkPresent::RtBuf rb;
		std::string w;
		if ( !WwVkPresent::rtBufOf( name, rb, w ) ) {
			std::printf( "FARGNDRTDRAW shared %s: rtBufOf refused (%s)\n", name, w.c_str() );
			return false;
		}
		glFinish();
		std::vector<std::uint8_t> seen;
		if ( !WwVkPresent::farTexelBufferRead( rb.key, seen, w ) ) {
			std::printf( "FARGNDRTDRAW shared %s: farTexelBufferRead refused (%s)\n", name, w.c_str() );
			return false;
		}
		const size_t compared = std::min( seen.size(), want.size() ), bad = differ( seen, want );
		const bool ok = compared > 0 && bad == 0 && rb.glBuf == buf && rb.view != 0;
		std::printf( "FARGNDRTDRAW shared %s: %zu bytes compared, %zu differ, GL buffer %u (bound %u), view %s: %s\n", name, compared, bad,
			rb.glBuf, buf, rb.view ? "set" : "none", ok ? "OK" : "FAIL" );
		return ok;
	};

	// ---- the program's samplers: one unit each; the far ground's four as cellfarground.cpp makes them
	std::vector<Smp> smp;
	int iGrid = -1, iLtex = -1, iDecal = -1, iAtlas = -1;
	bool hasRestir = false, hasAtlas = false;
	{
		GLint n = 0;
		f.getProgramiv( pFg, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pFg, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			Smp s;
			s.name = nm;
			s.glType = legacyType.count( s.name ) ? legacyType[s.name] : ty;
			if ( !smpOf( s, W, H ) ) {
				const bool sampler = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 );
				if ( sampler )
					return fatal( "sampler " + s.name + " of a GL type the gate does not make (0x" + std::to_string( ty ) + ")" );
				continue;
			}
			const int at = int( smp.size() );
			if ( s.name == "fgGrid" ) {
				s.unbound = false;	// made below (RGBA32UI, nearest)
				s.fmt = 0x8D70;
				s.w = s.h = kGn;
				iGrid = at;
			} else if ( s.name == "fgLtex" ) {
				s.fmt = 0x8058;	// RGBA8, mipped, REPEAT (fill), anisotropy 8 (below)
				s.w = s.h = 64;
				s.d = 4;
				s.levels = 7;
				iLtex = at;
			} else if ( s.name == "fgDecal" ) {
				iDecal = at;	// RGBA32F texel buffer (smpOf), its contents below
			} else if ( s.name == "fgAtlas" ) {
				s.fmt = 0x8058;	// mipped (cellfarground.cpp's atlas; seed 2 lifts fgBinMap.w's lod clamp off 0)
				s.w = s.h = 64;
				s.levels = 7;
				iAtlas = at;
			} else if ( s.name == "cellRestir" ) {	// the shared names (fill only zeroes them)
				s.fmt = 0x8230;
				s.bufBytes = int( rsBytes );
				s.tex = rsTex;
				s.buf = rsBuf;
				hasRestir = true;
			} else if ( s.name == "cellDdgiAtlas" ) {
				s.fmt = 0x8814;
				s.bufBytes = int( atBytes );
				s.tex = atTex;
				s.buf = atBuf;
				hasAtlas = true;
			}
			s.unit = at + 1;
			smp.push_back( s );
		}
	}
	if ( iGrid < 0 || iLtex < 0 || iDecal < 0 || iAtlas < 0 )
		return fatal( "the program has no fgGrid / fgLtex / fgDecal / fgAtlas (not the shader-decals far ground)" );
	if ( !hasAtlas || hasRestir != ( kv == 1 ) )
		return fatal( std::string( tag ) + " program: cellDdgiAtlas " + ( hasAtlas ? "active" : "missing" ) + ", cellRestir "
			+ ( hasRestir ? "active" : "missing" ) + " (not the key's RT inputs)" );
	for ( const char * n : { "cellGiHalfOn", "cellGiHalfScale", "cellGiHalfRect", "cellDdgiOn", "cellProbe" } )
		if ( f.getUniformLocation( pFg, n ) < 0 )
			return fatal( std::string( n ) + " has no location in GL's program (an arm would test nothing)" );
	{
		std::string miss;
		int nMiss = 0;
#ifdef WW_VK_FARGNDRT
		const WwVkPresent::FxField * fsT = kv == 1 ? WwVkPresent::kT_fargndRtPs : WwVkPresent::kT_fargndDgPs;
		const size_t nfsT = kv == 1 ? sizeof( WwVkPresent::kT_fargndRtPs ) / sizeof( WwVkPresent::kT_fargndRtPs[0] )
			: sizeof( WwVkPresent::kT_fargndDgPs ) / sizeof( WwVkPresent::kT_fargndDgPs[0] );
		for ( size_t i = 0; i < nfsT; i++ )
			if ( f.getUniformLocation( pFg, fsT[i].name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + fsT[i].name;
			}
#else
		std::printf( "FARGNDRTDRAW no twin table (WW_VK_FARGNDRT undefined): the pixel fields are not checked\n" );
#endif
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterVs )
			if ( f.getUniformLocation( pFg, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " vs:" ) + fd.name;
			}
		std::printf( "FARGNDRTDRAW %d twin fields GL has no location for:%s\n", nMiss, miss.c_str() );
	}
	std::printf( "FARGNDRTDRAW %zu samplers active in GL's program:", smp.size() );
	for ( const Smp & s : smp )
		std::printf( " %s%s", s.name.c_str(), s.unbound ? "(unbound)" : "" );
	std::printf( "\n" );

	// ---- the blocks: globalUniforms (the frame's camera), wwShadowFrame (seeded per member), skinningUniforms (zero)
	const float n0 = 10.0f, f0 = 100000.0f, fy = 1.0f / std::tan( 0.5f * 1.0471976f ), asp = float( W ) / float( H );
	float proj[16] = {};
	proj[0] = fy / asp;
	proj[5] = fy;
	proj[10] = ( f0 + n0 ) / ( n0 - f0 );
	proj[11] = -1.0f;
	proj[14] = 2.0f * f0 * n0 / ( n0 - f0 );
	GLuint ubo[3] = {};
	f.genBuffers( 3, ubo );
	const char * blockName[3] = { "globalUniforms", "wwShadowFrame", "skinningUniforms" };
	GLint blockSize[3] = { 336, 248, 12288 };
	GLint blockAt[3] = { 0, 1, 2 };  // BATCH1117: a SPIR-V program keeps its decorated binding (the app sets none), read here
	for ( int k = 0; k < 3; k++ ) {
		const GLuint bi = f.getUniformBlockIndex( pFg, blockName[k] );
		if ( bi != 0xFFFFFFFFu ) {
			if ( legacy )
				f.uniformBlockBinding( pFg, bi, GLuint( k ) );
			else
				f.getActiveUniformBlockiv( pFg, bi, 0x8A3F, &blockAt[k] );  // GL_UNIFORM_BLOCK_BINDING
			f.getActiveUniformBlockiv( pFg, bi, fBlockSize, &blockSize[k] );
		}
		for ( int j = 0; j < k; j++ )
			if ( blockAt[j] == blockAt[k] )
				return fatal( std::string( "blocks " ) + blockName[j] + " and " + blockName[k] + " share binding " + std::to_string( blockAt[k] ) );
		std::vector<std::uint8_t> z( size_t( std::max( blockSize[k], 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[k] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
	}
	std::printf( "FARGNDRTDRAW blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes; bindings %d %d %d\n", blockSize[0],
		blockSize[1], blockSize[2], blockAt[0], blockAt[1], blockAt[2] );
	struct ShMember
	{
		std::string name;
		GLint off = 0, type = 0, size = 1, stride = 0;
	};
	auto membersOf = [&]( GLuint q ) {
		std::vector<ShMember> out;
		const GLuint sbi = f.getUniformBlockIndex( q, "wwShadowFrame" );
		GLint n = 0;
		f.getProgramiv( q, fActiveUnis, &n );
		for ( GLint i = 0; i < n && sbi != 0xFFFFFFFFu; i++ ) {
			const GLuint idx = GLuint( i );
			GLint bi = -1;
			f.getActiveUniformsiv( q, 1, &idx, fBlockIndex, &bi );
			if ( bi != GLint( sbi ) )
				continue;
			ShMember m;
			char nm[256] = {};
			GLenum ty = 0;
			f.getActiveUniform( q, idx, 256, nullptr, &m.size, &ty, nm );
			m.name = nm;
			m.type = GLint( ty );
			f.getActiveUniformsiv( q, 1, &idx, 0x8A3B, &m.off );     // GL_UNIFORM_OFFSET
			f.getActiveUniformsiv( q, 1, &idx, 0x8A3C, &m.stride );  // GL_UNIFORM_ARRAY_STRIDE
			out.push_back( m );
		}
		return out;
	};
	// BATCH1117: seeded in the legacy program's member order (the same world on either program); the SPIR-V block must hold
	// the same members at the same offsets (a bool reads uint there)
	const std::vector<ShMember> shMembers = membersOf( pLegacy );
	{
		auto key = []( std::vector<ShMember> v ) {
			std::vector<std::string> k;
			for ( const ShMember & m : v ) {
				const GLint t = m.type == 0x8B56 || m.type == 0x1404 ? 0x1405 : m.type;  // bool / int / uint: one 4-byte word
				k.push_back( std::to_string( m.off ) + "/" + std::to_string( t ) + "/" + std::to_string( m.size ) + "/" + std::to_string( m.stride ) );
			}
			std::sort( k.begin(), k.end() );
			return k;
		};
		const std::vector<ShMember> spv = membersOf( pFg );
		const bool same = key( shMembers ) == key( spv );
		std::printf( "FARGNDRTDRAW wwShadowFrame members: legacy %zu, GL's program %zu, layout %s\n", shMembers.size(), spv.size(),
			same ? "the same" : "DIFFERS" );
		if ( !same && !shMembers.empty() && !spv.empty() )
			return fatal( "wwShadowFrame's layout differs between the legacy and the HLSL-built program" );
	}
	auto blocks = [&]( std::mt19937 & rng ) {
		std::uniform_real_distribution<float> u01( 0.05f, 1.0f );
		std::vector<std::uint8_t> g( size_t( blockSize[0] ), 0 );
		auto putF = [&]( size_t at, float x ) { std::memcpy( &g[at], &x, 4 ); };
		for ( int c = 0; c < 3; c++ ) {
			putF( size_t( 16 * c + 4 * c ), 1.0f );       // viewMatrix identity
			putF( size_t( 48 + 16 * c + 4 * c ), 1.0f );  // envMapRotation identity
		}
		for ( int k = 0; k < 16; k++ )
			putF( size_t( 96 + 4 * k ), proj[k] );
		const float L[3] = { 0.3f, 0.8f, 0.52f };
		for ( int k = 0; k < 3; k++ )
			putF( size_t( 160 + 4 * k ), L[k] );
		for ( size_t at = 176; at < 272; at += 4 )
			if ( at >= 208 || ( at % 16 ) != 12 )
				putF( at, u01( rng ) );
		for ( size_t at = 272; at < 288; at += 4 )
			putF( at, u01( rng ) );
		const GLint vpd[4] = { 0, 0, W, H };
		std::memcpy( &g[288], vpd, 16 );
		putF( 320, u01( rng ) );
		putF( 324, u01( rng ) );
		f.bindBuffer( fUbo, ubo[0] );
		ex.bufferSubData( fUbo, 0, std::ptrdiff_t( g.size() ), g.data() );
		std::vector<std::uint8_t> sh( size_t( blockSize[1] ), 0 );
		for ( const ShMember & m : shMembers ) {
			int comp = 1;
			bool isInt = false;
			switch ( m.type ) {
			case 0x1406: comp = 1; break;
			case 0x8B50: comp = 2; break;
			case 0x8B51: comp = 3; break;
			case 0x8B52: comp = 4; break;
			case 0x8B5C: comp = 16; break;
			case 0x1404: case 0x8B56: case 0x1405: comp = 1, isInt = true; break;
			case 0x8B53: case 0x8B57: case 0x8DC6: comp = 2, isInt = true; break;
			case 0x8B54: case 0x8B58: case 0x8DC7: comp = 3, isInt = true; break;
			case 0x8B55: case 0x8B59: case 0x8DC8: comp = 4, isInt = true; break;
			default: continue;
			}
			const bool off = m.name.find( "View" ) != std::string::npos || m.name.find( "Probe" ) != std::string::npos
				|| m.name.find( "Red" ) != std::string::npos || m.name.find( "red" ) != std::string::npos
				|| m.name.find( "Dbg" ) != std::string::npos || m.name.find( "Debug" ) != std::string::npos || extraZero( m.name );
			for ( GLint e = 0; e < std::max( m.size, 1 ); e++ )
				for ( int c = 0; c < comp; c++ ) {
					const size_t at = size_t( m.off ) + size_t( e ) * size_t( m.stride ) + size_t( 4 * c );
					if ( at + 4 > sh.size() )
						continue;
					if ( isInt ) {
						const GLint v = off ? 0 : GLint( rng() & 1u );
						std::memcpy( &sh[at], &v, 4 );
					} else {
						const float x = off ? 0.0f : u01( rng );
						std::memcpy( &sh[at], &x, 4 );
					}
				}
		}
		f.bindBuffer( fUbo, ubo[1] );
		ex.bufferSubData( fUbo, 0, std::ptrdiff_t( sh.size() ), sh.data() );
	};

	// ---- the terrain shape in view space (modelViewMatrix identity): a dummy vertex block first (baseVertex 10) and 900
	// leading degenerate elements (firstIndex 900, idxOff 1800 bytes); 1800 trailing ones keep red 64's shifted range inside
	struct Vtx
	{
		float p[3];
		std::uint8_t c[4];
		std::uint32_t n, t, b;
		float uv[2];
	};
	static_assert( sizeof( Vtx ) == 36, "the interleaved stride" );
	std::vector<Vtx> vtx( 10, Vtx {} );
	std::vector<std::uint16_t> idx( 900, 0 );
	const int GX = 40, GZ = 48, base = int( vtx.size() );
	auto height = []( float x, float z ) { return -110.0f + 22.0f * std::sin( x * 0.009f ) * std::cos( z * 0.011f ) + 6.0f * std::sin( x * 0.05f ); };
	for ( int j = 0; j < GZ; j++ )
		for ( int i = 0; i < GX; i++ ) {
			const float x = -650.0f + 1300.0f * float( i ) / float( GX - 1 ), z = -150.0f - 1450.0f * float( j ) / float( GZ - 1 );
			const float e = 0.5f, y = height( x, z ), dx = ( height( x + e, z ) - height( x - e, z ) ) / ( 2.0f * e ),
				dz = ( height( x, z + e ) - height( x, z - e ) ) / ( 2.0f * e );
			float nn[3] = { -dx, 1.0f, -dz };
			const float nl = std::sqrt( nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2] );
			const float tl = std::sqrt( 1.0f + dx * dx ), bl = std::sqrt( 1.0f + dz * dz );
			Vtx v {};
			v.p[0] = x, v.p[1] = y, v.p[2] = z;
			v.c[0] = std::uint8_t( 120 + 135 * i / GX ), v.c[1] = 180, v.c[2] = std::uint8_t( 120 + 135 * j / GZ ), v.c[3] = 255;
			v.n = pack1010102( nn[0] / nl, nn[1] / nl, nn[2] / nl, 1.0f );
			v.t = pack1010102( 1.0f / tl, dx / tl, 0.0f, 1.0f );
			v.b = pack1010102( 0.0f, -dz / bl, -1.0f / bl, 1.0f );
			v.uv[0] = x / 250.0f, v.uv[1] = z / 250.0f;
			vtx.push_back( v );
		}
	const std::uint32_t first = std::uint32_t( idx.size() );
	for ( int j = 0; j + 1 < GZ; j++ )
		for ( int i = 0; i + 1 < GX; i++ ) {
			const std::uint16_t a = std::uint16_t( j * GX + i ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + GX ), d = std::uint16_t( c + 1 );
			if ( j % 3 == 1 )
				idx.insert( idx.end(), { a, c, b, b, c, d } );  // clockwise seen from above: culled
			else
				idx.insert( idx.end(), { a, b, c, b, d, c } );
		}
	const std::uint32_t count = std::uint32_t( idx.size() ) - first;
	idx.insert( idx.end(), 1800, std::uint16_t( 0 ) );
	const long long idxOff = (long long) first * 2;	// what bsshape passes (WwBl::geo.idxOff, bytes)
	GLuint vao = 0, vb = 0, eb = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	f.genBuffers( 1, &vb );
	f.bindBuffer( 0x8892, vb );
	f.bufferData( 0x8892, std::ptrdiff_t( vtx.size() * sizeof( Vtx ) ), vtx.data(), fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 36, reinterpret_cast<const void *>( 0 ) );
	ex.vertexAttribPointer( 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 36, reinterpret_cast<const void *>( 12 ) );
	for ( int k = 0; k < 3; k++ )
		ex.vertexAttribPointer( GLuint( 2 + k ), 4, 0x8D9F, GL_TRUE, 36, reinterpret_cast<const void *>( std::uintptr_t( 16 + 4 * k ) ) );
	ex.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 36, reinterpret_cast<const void *>( 28 ) );
	for ( GLuint l : { 0u, 1u, 2u, 3u, 4u, 7u } )
		ex.enableVertexAttribArray( l );
	f.genBuffers( 1, &eb );
	f.bindBuffer( 0x8893, eb );
	f.bufferData( 0x8893, std::ptrdiff_t( idx.size() * 2 ), idx.data(), fStatic );
	ex.bindVertexArray( 0 );
	const float zm = -700.0f, zd = ( ( proj[10] * zm + proj[14] ) / -zm ) * 0.5f + 0.5f;
	std::printf( "FARGNDRTDRAW job: %dx%d x%d, %zu vertices (baseVertex %d), %u elements from %u, grid %d x %d, %d decals in %d x %d bins, "
		"slab depth %.6f\n", W, H, S, vtx.size(), base, count, first, kGn, kGn, kRec, kBins, kBins, zd );

	auto readColor = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 0, fT2DMS, msColor );
		u1i( pUnpack, "src", 0 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		bindUnit( 0, fT2DMS, 0 );
		return out;
	};
	auto readDepth = [&]( GLuint msDs, bool shared ) {
		std::vector<float> out( size_t( WS ) * H );
		glBindTexture( fT2DMS, msDs );
		glTexParameteri( fT2DMS, 0x90EA, GL_DEPTH_COMPONENT );
		glBindTexture( fT2DMS, 0 );
		f.bindFramebuffer( fFb, fbUnpD );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pDepth );
		bindUnit( 0, fT2DMS, msDs );
		u1i( pDepth, "src", 0 );
		u1i( pDepth, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, fRed, GL_FLOAT, out.data() );
		bindUnit( 0, fT2DMS, 0 );
		if ( shared ) {
			glBindTexture( fT2DMS, msDs );
			glTexParameteri( fT2DMS, 0x90EA, GL_STENCIL_INDEX );
			glBindTexture( fT2DMS, 0 );
		}
		return out;
	};
	auto before = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.3f, 0.45f, 0.7f, 0.5f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_SCISSOR_TEST );
		glScissor( 0, 0, W / 2, H );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		f.useProgram( pSlab );
		f.uniform1fv( f.getUniformLocation( pSlab, "zd" ), 1, &zd );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	// the terrain's state: opaque, depth LEQUAL written, back faces culled, a polygon offset
	auto drawState = [&]( GLuint fb, bool scissor ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );
		glEnable( 0x8037 );  // GL_POLYGON_OFFSET_FILL
		glPolygonOffset( 1.0f, 2.0f );
		if ( scissor ) {
			glEnable( GL_SCISSOR_TEST );
			glScissor( W / 5, H / 6, W / 2, H / 2 );
		}
		f.useProgram( pFg );
		for ( int k = 0; k < 3; k++ )
			f.bindBufferBase( fUbo, GLuint( blockAt[k] ), ubo[k] );
		for ( const Smp & s : smp ) {
			bindUnit( s.unit, targetOf( s.kind ), s.tex );
			f.uniform1i( f.getUniformLocation( pFg, s.name.c_str() ), s.unit );
		}
		ex.bindVertexArray( vao );
	};
	auto stateEnd = [&]() {
		ex.bindVertexArray( 0 );
		for ( const Smp & s : smp )
			bindUnit( s.unit, targetOf( s.kind ), 0 );
		glDisable( 0x8037 );
		glPolygonOffset( 0.0f, 0.0f );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	auto setI = [&]( const char * n, GLint v ) {
		const GLint l = f.getUniformLocation( pFg, n );
		if ( l >= 0 )
			f.uniform1iv( l, 1, &v );
	};
	auto set4 = [&]( const char * n, float a, float b, float c, float d ) {
		const GLint l = f.getUniformLocation( pFg, n );
		const float v[4] = { a, b, c, d };
		if ( l >= 0 )
			f.uniform4fv( l, 1, v );
	};
	auto putGrid = [&]( const std::vector<std::uint32_t> & g ) {
		glBindTexture( GL_TEXTURE_2D, smp[size_t( iGrid )].tex );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kGn, kGn, 0x8D99 /* GL_RGBA_INTEGER */, GL_UNSIGNED_INT, g.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
	};
	auto putDecals = [&]( const std::vector<float> & d ) {
		f.bindBuffer( 0x8C2A, smp[size_t( iDecal )].buf );
		ex.bufferSubData( 0x8C2A, 0, std::ptrdiff_t( d.size() * 4 ), d.data() );
	};
	const int decalTexels = smp[size_t( iDecal )].bufBytes / 16;
	if ( kIdOff + ( kBins * kBins * kRec + 3 ) / 4 > decalTexels )
		return fatal( "the decal buffer is too small for the fixture" );

	static char twinKeys[192];
	bool gateOk = true;
	int runs = 0;
	// fgGrid, fgLtex and fgAtlas on the present device as cellfarground.cpp makes them on the view (sharedTarget: the twin
	// reads them in place, no copy a run); plain GL twins of the same contents for the GL-vs-GL check and the copy count
	using CopyImg = void( WINAPI * )( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei,
		GLsizei, GLsizei );
	const auto copyImg = reinterpret_cast<CopyImg>( wglGetProcAddress( "glCopyImageSubData" ) );
	if ( !copyImg )
		return fatal( "glCopyImageSubData" );
	static char sharedKeys[3];
	const int iShared[3] = { iGrid, iLtex, iAtlas };
	GLuint plain[3] = {};
	for ( int k = 0; k < 3; k++ ) {
		Smp & s = smp[size_t( iShared[k] )];
		const unsigned tg = targetOf( s.kind );
		unsigned t = 0;
		if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( sharedKeys + k ), s.kind == 2 ? 2 : 0, s.w, s.h, s.d, s.fmt, t,
				why, s.levels ) )
			return fatal( "sharedTarget " + s.name + ": " + why );
		s.tex = t;
		glGenTextures( 1, &plain[k] );
		for ( const GLuint x : { GLuint( t ), plain[k] } ) {
			glBindTexture( tg, x );
			if ( x == plain[k] ) {
				if ( s.kind == 0 )
					ex.texStorage2D( tg, s.levels, s.fmt, s.w, s.h );
				else
					ex.texStorage3D( tg, s.levels, s.fmt, s.w, s.h, s.d );
			}
			const bool mip = s.levels > 1, lin = k != 0;	// the grid: nearest, clamped
			glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, mip ? GL_LINEAR_MIPMAP_LINEAR : lin ? GL_LINEAR : GL_NEAREST );
			glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, lin ? GL_LINEAR : GL_NEAREST );
			glTexParameteri( tg, GL_TEXTURE_WRAP_S, mip ? GL_REPEAT : 0x812F );
			glTexParameteri( tg, GL_TEXTURE_WRAP_T, mip ? GL_REPEAT : 0x812F );
			glTexParameteri( tg, fMaxLevel, s.levels - 1 );
			if ( k == 1 )
				glTexParameterf( tg, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoPix );	// manager ruling 11-16 (fgrt_consts: the reason)
		}
		glBindTexture( tg, 0 );
	}
	auto toPlain = [&]() {	// the shared contents into the plain twins, every level
		for ( int k = 0; k < 3; k++ ) {
			const Smp & s = smp[size_t( iShared[k] )];
			const GLenum tg = targetOf( s.kind );
			for ( int l = 0; l < s.levels; l++ )
				copyImg( s.tex, tg, l, 0, 0, 0, plain[k], tg, l, 0, 0, 0, std::max( s.w >> l, 1 ), std::max( s.h >> l, 1 ), s.kind == 2 ? s.d : 1 );
		}
	};
	auto swapPlain = [&]() {
		for ( int k = 0; k < 3; k++ )
			std::swap( smp[size_t( iShared[k] )].tex, plain[k] );
	};
	WwVkPresent::FxStat tot;
	auto take = [&]() {	// one run's stat into the total
		const WwVkPresent::FxStat st = WwVkPresent::takeFarGroundDrawStat();
		tot.runs += st.runs, tot.fails += st.fails, tot.cpuMs += st.cpuMs, tot.maxMs = std::max( tot.maxMs, st.maxMs );
		tot.pipelines = st.pipelines, tot.buildMs = st.buildMs, tot.copies += st.copies;	// the build counts are running totals
		return st;
	};
	take();
	for ( int seed = 1; seed <= 2; seed++ ) {
		std::mt19937 rng( 20261114u + std::uint32_t( seed ) * 7919u
			+ std::uint32_t( getenv( "FLD_SEED" ) ? std::atoi( getenv( "FLD_SEED" ) ) : 0 ) );
		/* BATCH1118: fill in the LEGACY program's sampler order from this rng, the HLSL-only samplers (farSkyMap) after
		 * from a side rng: seedUnis and blocks then draw exactly the legacy run's values. Walking GL's (HLSL) sampler order
		 * here shifted every later draw: seed 1 got cellIsAdapted 0.58 / contrast 0.24 (a flat grade, 552 distinct) */
		std::vector<int> fillOrder;
		std::vector<char> inOrder( smp.size(), 0 );
		for ( const std::string & nm : legacyOrder ) {
			int at = -1;
			for ( size_t k = 0; k < smp.size(); k++ )
				if ( smp[k].name == nm )
					at = int( k );
			if ( at >= 0 ) {
				fillOrder.push_back( at );
				inOrder[size_t( at )] = 1;
				continue;
			}
			Smp probe;
			probe.name = nm;
			probe.glType = legacyType[nm];
			if ( smpOf( probe, W, H ) )
				return fatal( "legacy sampler " + nm + " is not in GL's program: its fill would shift every later value" );
		}
		std::mt19937 rngSide( 9001u + std::uint32_t( seed ) );
		int sideFills = 0;
		for ( size_t k = 0; k < smp.size(); k++ )
			if ( !inOrder[k] ) {
				fillOrder.push_back( -1 - int( k ) );
				sideFills++;
			}
		if ( seed == 1 )
			std::printf( "FARGNDRTDRAW textures filled in the legacy program's order: %zu, from a side rng (GL's program only): %d\n",
				fillOrder.size() - size_t( sideFills ), sideFills );
		for ( const int fo : fillOrder ) {
			Smp & s = smp[size_t( fo >= 0 ? fo : -1 - fo )];
			if ( int( &s - smp.data() ) == iGrid ) {
				if ( !s.tex ) {
					glGenTextures( 1, &s.tex );
					glBindTexture( GL_TEXTURE_2D, s.tex );
					ex.texStorage2D( GL_TEXTURE_2D, 1, 0x8D70, kGn, kGn );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F );
					glBindTexture( GL_TEXTURE_2D, 0 );
				}
				continue;
			}
			if ( !fill( s, fo >= 0 ? rng : rngSide ) )
				return fatal( "texture " + s.name );
		}
		glBindTexture( 0x8C1A, smp[size_t( iLtex )].tex );
		glTexParameterf( 0x8C1A, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoPix );	// manager ruling 11-16 (fgrt_consts: the reason)
		glBindTexture( 0x8C1A, 0 );
		std::mt19937 rngFg( 4242u + std::uint32_t( seed ) );
		const std::vector<std::uint32_t> grid = gridData( rngFg, false );
		std::mt19937 rngRedo( 4242u + std::uint32_t( seed ) );
		const std::vector<std::uint32_t> gridRed = gridData( rngRedo, true );
		std::mt19937 rngDc( 777u + std::uint32_t( seed ) ), rngDcRed( 777u + std::uint32_t( seed ) );
		const std::vector<float> decals = decalData( rngDc, 0.0f, decalTexels ), decalsRed = decalData( rngDcRed, 90.0f, decalTexels );
		putGrid( grid );
		putDecals( decals );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the far ground's data upload" );
		{	// farlodrtdraw's switch profile: farloddraw's seed 1 / 2 sets without cellClusterOn, the RT GI switches on
			const std::string base = "cellOn,fogOn,volOn,cellFarOn,cellShadowOn,cellShadowFadeOn,ussOn,ussCsmOn,ussTerrOn,"
				"ussContactOn,ussCloudOn,cellAoOn,cellHasDir,landShoreOn,landWetOn,landSwashOn,waterInfoOn,"
				"cellRestirOn,cellDdgiOn,cellGiSky,cellLightCount,cellExtDalc";
			static std::string prof;	// _putenv keeps the pointer
			prof = "FLD_ON=" + base + ( seed == 1 ? ",cellSsgiOn,cellSsrOn,cellIsOn"
				: ",cellReflOn,cellReflCount,cellGoboOn,cellCtOn,cellFarPlacedOn,cellHasDalc,cellSpecOcc" );
			_putenv( prof.c_str() );
		}
		g_profileSeed = seed;
		const int nu = seedUnis( pLegacy, rng, pFg );	// the legacy program's order and types: the same world on either program
		blocks( rng );
		const bool scissor = seed == 2;
		f.useProgram( pFg );
		setI( "farInst", 0 );
		setI( "fgMode", 1 );
		setI( "fgDecalsOn", 1 );
		set4( "fgGridMap", kOx, kOy, 1.0f / kSpacing, kTiling );
		{
			const GLint l = f.getUniformLocation( pFg, "fgGridN" );
			const float v[2] = { float( kGn ), float( kGn ) };
			if ( l >= 0 )
				f.uniform2fv( l, 1, v );
		}
		set4( "fgBinMap", kOx, kOy, 1.0f / kBin, seed == 2 ? 3.0f : 0.0f );	// .w: the atlas lod clamp (cellfarground.cpp's maxLod)
		set4( "fgBinN", float( kBins ), float( kBins ), float( kBinOff ), float( kIdOff ) );
		{	// view space onto the grid's world square: world = (x + 5000, -z + 7000, y + 300)
			const GLint l = f.getUniformLocation( pFg, "cellRow" );
			const float rows[12] = { 1.0f, 0.0f, 0.0f, 5000.0f, 0.0f, 0.0f, -1.0f, 7000.0f, 0.0f, 1.0f, 0.0f, 300.0f };
			if ( l >= 0 )
				f.uniform4fv( l, 3, rows );
			else
				std::printf( "FARGNDRTDRAW note: cellRow has no location\n" );
		}
		{
			const GLint l = f.getUniformLocation( pFg, "fogDistScale" );
			const float v = 1.0f / 1500.0f;
			if ( l >= 0 )
				f.uniform1fv( l, 1, &v );
		}
		{	/* lane VKGROUND5 (VKGROUND4's fgpagesdraw fix): volPackDims.xyz = the packed air volume's size in texels, as
			 * cellvolair.cpp sets it. Seeded in [0.05, 1) it made wwGridAt's clamp( uv * share, 0.5 / dims, share - 0.5 / dims )
			 * run with lo > hi (undefined in GLSL and SPIR-V; NV's two compilers differ) on seed 1, whose far air grid is on */
			const GLint l = f.getUniformLocation( pFg, "volPackDims" ), lp = f.getUniformLocation( pFg, "volPack" );
			const Smp * vt = nullptr;
			for ( const Smp & s : smp )
				if ( s.name == "volTex" )
					vt = &s;
			if ( l >= 0 && vt ) {
				float pd[4] = {}, pk[4] = {};
				f.getUniformfv( pFg, l, pd );
				if ( lp >= 0 )
					f.getUniformfv( pFg, lp, pk );
				pd[0] = float( vt->w );
				pd[1] = float( vt->h );
				pd[2] = float( vt->d );
				f.uniform4fv( l, 1, pd );
				const bool ok = pk[0] >= 1.0f / pd[0] && pk[1] >= 1.0f / pd[1] && pk[2] >= 1.0f / pd[0] && pk[3] >= 1.0f / pd[1];
				std::printf( "FARGNDRTDRAW seed %d volPackDims %g x %g x %g (volTex), volPack shares %s the half-texel clamp\n", seed,
					double( pd[0] ), double( pd[1] ), double( pd[2] ), ok ? "inside" : "OUTSIDE" );
				if ( !ok && lp >= 0 ) {	// lane VKFLIGHT1 (VKGROUND17's rule): seeding, never the bar (the flight arm's rng stream seeded a share under one texel)
					for ( int c = 0; c < 4; c++ ) {
						const float lo = 1.0f / pd[c % 2];
						pk[c] = pk[c] < lo ? lo : pk[c];
					}
					f.uniform4fv( lp, 1, pk );
					std::printf( "FARGNDRTDRAW seed %d volPack raised to one texel of the air volume: %g %g %g %g (seeding)\n", seed,
						double( pk[0] ), double( pk[1] ), double( pk[2] ), double( pk[3] ) );
				} else if ( !ok )
					return fatal( "volPack under one texel of the air volume (wwGridAt's clamp undefined)" );
			} else if ( l >= 0 )
				return fatal( "volPackDims read but no volTex sampler" );
		}
		auto setF = [&]( const char * n, int comps, int cnt, const float * v ) {
			const GLint l = f.getUniformLocation( pFg, n );
			if ( l >= 0 )
				( comps == 1 ? f.uniform1fv : comps == 2 ? f.uniform2fv : comps == 3 ? f.uniform3fv : f.uniform4fv )( l, cnt, v );
		};
		{	// lane VKGROUND10: the RT GI seeding (header: Space, Seeding); the DDGI box in world units over the ground
			const float rsz[4] = { float( W ), float( H ), 0.0f, 0.0f };
			setF( "cellRestirSize", 4, 1, rsz );
			const float org[4] = { 4300.0f, 7100.0f, 100.0f, 300.0f }, stp[4] = { 175.0f, 250.0f, 25.0f, 0.0f };
			setF( "cellDdgiOrigin", 4, 1, org );
			setF( "cellDdgiStep", 4, 1, stp );
			setF( "cellDdgiCOrigin", 4, 1, org );
			setF( "cellDdgiCStep", 4, 1, stp );
			for ( const char * n : { "cellDdgiDims", "cellDdgiCDims" } )
				if ( f.getUniformLocation( pFg, n ) >= 0 )
					f.uniform4iv( f.getUniformLocation( pFg, n ), 1, kDdgiDims );
			setI( "cellDdgiCBase", 0 );
			setI( "cellDdgiCount", 1 );
			setI( "cellGiLoop8", 8 );
			setI( "cellGiSky", 1 );
			setI( "cellExtDalc", 1 );
			setI( "cellGiAmb", 1 );
			setI( "cellLightCount", 1 );
			setI( "cellClusterOn", 0 );
			setI( "cellRestirOn", 1 );
			setI( "cellDdgiOn", 1 );
			setI( "cellGiHalfOn", 0 );
			const float band[2] = { 6000.0f, 8000.0f };	// cellFarW: the placed light unfaded over the ground (seed 2's band faded it out)
			setF( "cellFarBand", 2, 1, band );
		}
		if ( seed == 1 ) {	// the loop bounds' vec4-earlier neighbours zero (header): red 1 must not hang the device
			const struct { const char * n; int comps, c; } nb[4] = { { "cellCenter", 3, 1 }, { "cellRoomsDims", 3, 1 },
				{ "cellCtRect", 4, 1 }, { "cellClusterLog", 2, 0 } };
			int zeroed = 0;
			for ( const auto & z : nb ) {
				const GLint l = f.getUniformLocation( pFg, z.n );
				if ( l < 0 ) {
					std::printf( "FARGNDRTDRAW %s seed 1: %s has no GL location (its block bytes stay zero)\n", tag, z.n );
					continue;
				}
				float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				f.getUniformfv( pFg, l, v );
				v[z.c] = 0.0f;
				( z.comps == 2 ? f.uniform2fv : z.comps == 3 ? f.uniform3fv : f.uniform4fv )( l, 1, v );
				zeroed++;
			}
			std::printf( "FARGNDRTDRAW %s seed 1: %d of 4 loop-bound neighbours zeroed (cellCenter.y, cellRoomsDims.y, cellCtRect.y, "
				"cellClusterLog.x)\n", tag, zeroed );
		}
		if ( seed == 1 ) {	// lane VKGROUND16: the grade's contrast 1 on seed 1 (header: Seeding; the reds are seed 1's)
			const GLint l = f.getUniformLocation( pFg, "cellIsCine" );
			float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			if ( l >= 0 ) {
				f.getUniformfv( pFg, l, c );
				c[1] = 1.0f;
				f.uniform3fv( l, 1, c );
			}
			std::printf( "FARGNDRTDRAW %s seed 1: cellIsCine contrast 1 (location %d; saturation %g, z %g kept)\n", tag, l, double( c[0] ),
				double( c[2] ) );
		}
		for ( const Smp & s : smp )	// light 0: a shadow-casting omni over the ground (world), radius 5000, flat radial
			if ( s.name == "cellLights" ) {
				const float L[32] = { kWx, kWy + 700.0f, kWz + 1500.0f, 5000.0f, 1.0f, 0.8f, 0.6f, -2.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, -1.0f, 1.0f, 0.0f, 0.0f };
				bufWrite( s.buf, L, sizeof( L ) );
			}
		// the atlas: irradiance seeded, distance pairs far, probe states usable / missing in turn
		std::vector<float> atlas( kAtlasTexels * 4 );
		{
			std::uniform_real_distribution<float> u( 0.05f, 1.0f );
			for ( float & x : atlas )
				x = u( rng );
			for ( size_t t = 64 * kProbes; t < 192 * kProbes; t++ ) {
				atlas[4 * t] = atlas[4 * t + 2] = 2000.0f;
				atlas[4 * t + 1] = atlas[4 * t + 3] = 4.0e6f;
			}
			for ( size_t t = 192 * kProbes; t < 193 * kProbes; t++ ) {
				atlas[4 * t] = atlas[4 * t + 1] = atlas[4 * t + 2] = 0.0f;
				atlas[4 * t + 3] = ( t - 192 * kProbes ) % 2 ? 1.0f : 2.0f;
			}
			bufWrite( atBuf, atlas.data(), atBytes );
		}
		// lane VKGROUND12: here, after the RT seeding (before it, the seeding overwrote the RT switches)
		/* FLD_SET=name=v[:v:v:v],...: one value set on the drawn program AFTER seeding (BATCH1118: a probe on the same
		 * world; FLD_ZERO skips a draw and shifts every later value). Ints by the uniform's GL type, else floats */
		if ( const char * fs = getenv( "FLD_SET" ) ) {
			std::string list( fs );
			size_t s = 0;
			while ( s < list.size() ) {
				size_t e = list.find( ',', s );
				if ( e == std::string::npos )
					e = list.size();
				const std::string item = list.substr( s, e - s );
				s = e + 1;
				const size_t eq = item.find( '=' );
				if ( eq == std::string::npos )
					continue;
				const std::string nm = item.substr( 0, eq );
				std::vector<float> v;
				for ( size_t a = eq + 1; a <= item.size(); ) {
					size_t b = item.find( ':', a );
					if ( b == std::string::npos )
						b = item.size();
					v.push_back( std::strtof( item.substr( a, b - a ).c_str(), nullptr ) );
					a = b + 1;
				}
				GLenum ty = 0;
				GLint n = 0;
				f.getProgramiv( pFg, fActiveUnis, &n );
				for ( GLint i = 0; i < n; i++ ) {
					char un[256] = {};
					GLint sz = 0;
					GLenum t = 0;
					f.getActiveUniform( pFg, GLuint( i ), 256, nullptr, &sz, &t, un );
					if ( nm == un || nm + "[0]" == un )
						ty = t;
				}
				const GLint l = f.getUniformLocation( pFg, nm.c_str() );
				const bool isInt = ty == 0x1404 || ty == 0x8B56;
				if ( l >= 0 && isInt ) {
					const GLint iv = GLint( v[0] );
					f.uniform1iv( l, 1, &iv );
				} else if ( l >= 0 )
					( v.size() == 1 ? f.uniform1fv : v.size() == 2 ? f.uniform2fv : v.size() == 3 ? f.uniform3fv : f.uniform4fv )( l, 1, v.data() );
				std::printf( "FARGNDRTDRAW seed %d FLD_SET %s (%zu values, type 0x%X, location %d)\n", seed, nm.c_str(), v.size(), ty, l );
			}
		}
		// FLD_GET=name,...: the value GL holds for each named loose uniform after seeding (both programs: the same world?)
		if ( const char * fg = getenv( "FLD_GET" ) ) {
			std::string list( fg );
			for ( size_t s = 0; s < list.size(); ) {
				size_t e = list.find( ',', s );
				if ( e == std::string::npos )
					e = list.size();
				const std::string nm = list.substr( s, e - s );
				s = e + 1;
				const GLint l = f.getUniformLocation( pFg, nm.c_str() );
				GLfloat v[16] = {};
				GLint iv[16] = {};
				if ( l >= 0 ) {
					gw.glGetUniformfv( pFg, l, v );
					gw.glGetUniformiv( pFg, l, iv );
				}
				std::printf( "FARGNDRTDRAW seed %d FLD_GET %s location %d: f %.6g %.6g %.6g %.6g i %d %d %d %d\n", seed, nm.c_str(), l,
					v[0], v[1], v[2], v[3], iv[0], iv[1], iv[2], iv[3] );
			}
		}
		std::printf( "FARGNDRTDRAW seed %d: %d loose uniforms seeded%s, fgMode 1, decals on\n", seed, nu, scissor ? ", a scissor" : "" );

		auto glDraw = [&]() {
			ex.drawInst( GL_TRIANGLES, GLsizei( count ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ), 1, base, 0 );
		};
		before( fbRef );
		const std::vector<std::uint16_t> bgC = readColor( refCol );
		const std::vector<float> bgD = readDepth( refDs, false );
		// cellRestir: GL's depth first (it does not read the ratio), then (ratio, linear view depth) where the ground covers sample 0
		before( fbRef );
		drawState( fbRef, scissor );
		glDraw();
		stateEnd();
		const std::vector<float> preD = readDepth( refDs, false );
		std::vector<float> restir( size_t( W ) * size_t( H ) * 2 );
		int onGround = 0;
		float dMin = 1.0e9f, dMax = 0.0f;
		{
			std::uniform_real_distribution<float> u( 0.05f, 1.0f );
			for ( int y = 0; y < H; y++ )
				for ( int x = 0; x < W; x++ ) {
					const size_t s0 = size_t( y ) * size_t( WS ) + size_t( x ) * size_t( S ), t = size_t( y ) * size_t( W ) + size_t( x );
					restir[2 * t] = u( rng );
					restir[2 * t + 1] = 1.0e6f;
					if ( preD[s0] != bgD[s0] ) {
						restir[2 * t + 1] = proj[14] / ( 2.0f * preD[s0] - 1.0f + proj[10] );
						onGround++;
						dMin = std::min( dMin, restir[2 * t + 1] );
						dMax = std::max( dMax, restir[2 * t + 1] );
					}
				}
			bufWrite( rsBuf, restir.data(), rsBytes );
		}
		std::printf( "FARGNDRTDRAW %s seed %d: cellRestir ratio over %d of %d pixels (the ground's depth), the rest 1e6; linear depth %.0f..%.0f\n", tag, seed,
			onGround, W * H, dMin, dMax );
		{
			std::vector<std::uint8_t> a( atBytes ), r2( rsBytes );
			std::memcpy( a.data(), atlas.data(), atBytes );
			std::memcpy( r2.data(), restir.data(), rsBytes );
			if ( !sharedCheck( "cellDdgiAtlas", atBuf, a ) || ( kv == 1 && !sharedCheck( "cellRestir", rsBuf, r2 ) ) )
				gateOk = false;
		}
		before( fbRef );
		drawState( fbRef, scissor );
		glDraw();
		stateEnd();
		const GLenum ge = glGetError();
		if ( ge != GL_NO_ERROR )
			return fatal( "the GL reference draw: GL error " + std::to_string( ge ) );
		const std::vector<std::uint16_t> glC = readColor( refCol );
		const std::vector<float> glD = readDepth( refDs, false );
		{	// GL on the shared inputs vs GL on plain twins of the same bytes: EXACT (the shared storage changes nothing GL reads)
			toPlain();
			swapPlain();
			before( fbRef );
			drawState( fbRef, scissor );
			glDraw();
			stateEnd();
			swapPlain();
			const std::vector<std::uint16_t> plC = readColor( refCol );
			const std::vector<float> plD = readDepth( refDs, false );
			size_t cDiff = 0, dDiff = 0;
			for ( size_t i = 0; i < plC.size() && i < glC.size(); i++ )
				cDiff += plC[i] != glC[i];
			for ( size_t i = 0; i < plD.size() && i < glD.size(); i++ )
				dDiff += plD[i] != glD[i];
			const bool exact = !cDiff && !dDiff && plC.size() == glC.size() && plD.size() == glD.size() && glGetError() == GL_NO_ERROR;
			std::printf( "FARGNDRTDRAW seed %d GL shared vs plain inputs: %s (%zu colour halves, %zu depth samples differ)\n", seed,
				exact ? "EXACT" : "DIFFER", cDiff, dDiff );
			gateOk = gateOk && exact;
		}
		{
			float lo[4] = { 1e30f, 1e30f, 1e30f, 1e30f }, hi[4] = { -1e30f, -1e30f, -1e30f, -1e30f };
			std::set<std::uint64_t> distinct;
			for ( size_t s = 0; s < glD.size(); s++ ) {
				if ( glD[s] == bgD[s] )
					continue;
				std::uint64_t key = 0;
				for ( int c = 0; c < 4; c++ ) {
					const float x = h2f( glC[s * 4 + size_t( c )] );
					lo[c] = std::min( lo[c], x ), hi[c] = std::max( hi[c], x );
					key = ( key << 16 ) | glC[s * 4 + size_t( c )];
				}
				distinct.insert( key );
			}
			std::printf( "FARGNDRTDRAW seed %d GL colour over the ground: r %.4g..%.4g g %.4g..%.4g b %.4g..%.4g a %.4g..%.4g, %zu distinct\n", seed,
				lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3], distinct.size() );
			if ( distinct.size() < 1000 ) {
				std::printf( "FARGNDRTDRAW seed %d: under 1000 distinct colours over the ground: a near-constant layer is blind\n", seed );
				gateOk = false;
			}
		}

		// ---- lane VKGROUND10: the RT arms (header). GL's covered samples, then the GL probes before any Vulkan call
		auto setIp = [&]( const char * n, GLint v ) {
			f.useProgram( pFg );
			setI( n, v );
		};
		auto setFp = [&]( const char * n, int comps, const float * v ) {
			f.useProgram( pFg );
			setF( n, comps, 1, v );
		};
		std::vector<char> cov( glD.size(), 0 );
		size_t glCovered = 0;
		for ( size_t s = 0; s < glD.size(); s++ ) {
			bool c = glD[s] != bgD[s];
			for ( int ch = 0; ch < 4; ch++ )
				c = c || glC[s * 4 + size_t( ch )] != bgC[s * 4 + size_t( ch )];
			cov[s] = c;
			glCovered += c;
		}
		auto movedIn = [&]( const std::vector<char> & mask, const std::vector<std::uint16_t> & ref, const std::vector<std::uint16_t> & pc ) {
			size_t m = 0;
			for ( size_t s = 0; s < mask.size(); s++ ) {
				if ( !mask[s] )
					continue;
				bool d = false;
				for ( int ch = 0; ch < 4; ch++ )
					d = d || pc[s * 4 + size_t( ch )] != ref[s * 4 + size_t( ch )];
				m += d;
			}
			return m;
		};
		auto glFrame = [&]() {
			before( fbRef );
			drawState( fbRef, scissor );
			glDraw();
			stateEnd();
			return readColor( refCol );
		};
		for ( const char * sw : { "cellRestirOn", "cellDdgiOn" } ) {
			if ( kv == 2 && std::strcmp( sw, "cellRestirOn" ) == 0 )
				continue;
			GLint was = 1;	// lane VKGROUND12: put back the value seeded (an FLD_SET probe's 0 too), not a hard 1
			if ( f.getUniformLocation( pFg, sw ) >= 0 )
				gw.glGetUniformiv( pFg, f.getUniformLocation( pFg, sw ), &was );
			setIp( sw, 0 );
			const size_t moved = movedIn( cov, glC, glFrame() );
			setIp( sw, was );
			const bool ok = glCovered > 0 && moved * 5 >= glCovered;
			std::printf( "FARGNDRTDRAW %s seed %d GL probe %s 0: moved %zu of %zu covered samples (%.1f %%): %s\n", tag, seed, sw, moved,
				glCovered, glCovered ? 100.0 * double( moved ) / double( glCovered ) : 0.0, ok ? "OK" : "FAIL (the input does not reach the colour)" );
			if ( !ok )
				gateOk = false;
		}

		// every Vulkan call: twin 0 the base twin, 1 RT, 2 DG; without the RT twins (WW_VK_FARGNDRT) each RT run is REFUSED "no twin"
		static char twinKey[192], redKey[192], baseKey[192];
		int lastCopies = -1;
		auto vkGo = [&]( int twin, int red, const std::function<void( WwVkPresent::WaterRun & )> & between, std::string & w ) {
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun r;
			WwVkPresent::WaterCmd c;	// CellFarGround::vkDraw's command
			c.count = count;
			c.instanceCount = 1;
			c.firstIndex = std::uint32_t( idxOff / 2 );
			c.baseVertex = base;
			c.baseInstance = 0;
			bool ran = false;
			lastCopies = -1;
			if ( twin == 0 ) {
				WwVkPresent::farGroundStateOf( &gw, pFg, r );
				r.w = W, r.h = H, r.color = fc;
				r.cmds.assign( 1, c );
				r.key = reinterpret_cast<std::uintptr_t>( baseKey );
				ran = WwVkPresent::farGroundRun( r, w );
				lastCopies = take().copies;
			} else {
#ifdef WW_VK_FARGNDRT
				WwVkPresent::farGroundRtStateOf( &gw, pFg, r, twin );
				r.w = W, r.h = H, r.color = fc;
				r.cmds.assign( 1, c );
				r.key = reinterpret_cast<std::uintptr_t>( twin == kv ? twinKey : redKey );
				r.red = red;
				if ( between )
					between( r );
				ran = WwVkPresent::farGroundRtRun( r, twin, w );
				lastCopies = WwVkPresent::takeFarGroundRtDrawStat( twin ).copies;
#else
				( void )red;
				( void )between;
				w = "no twin: the far ground RT / DG twins are not built (WW_VK_FARGNDRT undefined)";
#endif
			}
			stateEnd();
			runs++;
			return ran;
		};

		// the Vulkan runs: red 0, then (seed 1) plain inputs and the reds
		enum { kMain, kPlain, kRed1, kRedRestir, kRedAtlas, kRedKey };
		const char * caseName[6] = { "red 0", "plain inputs (red 0 on plain fgGrid / fgLtex / fgAtlas)", "red 1 (ps block one vec4 off)",
			"red restir (rewritten after GL drew)", "red atlas (shifted one texel after GL drew)", "red key (the other twin)" };
		int copiesShared = -1, copiesPlain = -1;
		for ( int cs = kMain; cs <= kRedKey; cs++ ) {
			if ( cs != kMain && seed != 1 )
				continue;
			if ( cs == kRedRestir && kv != 1 )
				continue;
			if ( cs == kPlain )
				swapPlain();
			if ( cs == kRedRestir ) {
				std::vector<float> bad( restir );
				for ( size_t t = 0; t < size_t( W ) * size_t( H ); t++ )
					bad[2 * t] = 1.05f - restir[2 * t];
				bufWrite( rsBuf, bad.data(), rsBytes );
			} else if ( cs == kRedAtlas ) {
				std::vector<float> bad( atlas );
				std::memmove( bad.data(), atlas.data() + 4, ( atlas.size() - 4 ) * 4 );
				bufWrite( atBuf, bad.data(), atBytes );
			}
			std::string w;
			const int twin = cs == kRedKey ? ( kv == 1 ? 2 : 0 ) : kv;	// red key: the RT program on the DG twin, the DG one on the base twin
			const bool ran = vkGo( twin, cs == kRed1 ? 1 : 0, nullptr, w );
			if ( cs == kPlain )
				swapPlain();
			if ( cs == kRedRestir )
				bufWrite( rsBuf, restir.data(), rsBytes );
			else if ( cs == kRedAtlas )
				bufWrite( atBuf, atlas.data(), atBytes );
			if ( ran && cs == kMain )
				copiesShared = lastCopies;
			if ( ran && cs == kPlain )
				copiesPlain = lastCopies;
			if ( !ran ) {
				const bool okRefuse = cs == kRedKey;
				std::printf( "FARGNDRTDRAW %s seed %d %s: REFUSED (%s)%s\n", tag, seed, caseName[cs], w.c_str(), okRefuse ? " -> red refused, as it may" : "" );
				if ( !okRefuse )
					gateOk = false;
				continue;
			}
			const Verdict v = judge( bgC, bgD, readColor( fc ), readDepth( fds, true ), glC, glD );
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			const bool red = cs >= kRed1;
			std::printf( "FARGNDRTDRAW %s seed %d %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
				"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g), %d copies%s\n", tag, seed, caseName[cs], word, v.covered, v.both,
				v.onlyOne, v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, lastCopies,
				red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
			if ( red ? !fail : fail )
				gateOk = false;
		}
		if ( seed == 1 ) {	// fgGrid, fgLtex, fgAtlas read in place: exactly three copies fewer a run than on the plain twins
			const bool fewer = copiesShared >= 0 && copiesPlain - copiesShared == 3;
			std::printf( "FARGNDRTDRAW %s seed 1 copies a run: %d on the shared inputs, %d on plain ones (%s)\n", tag, copiesShared, copiesPlain,
				fewer ? "fgGrid, fgLtex, fgAtlas read in place" : "NOT three fewer" );
			gateOk = gateOk && fewer;
		}

		// lane VKGROUND16, the state arm (seed 1; manager ruling 11-16, fgrt_consts): fgLtex at the app's anisotropy 8, every census
		// input's sampling state side by side: GL's own texture state on the input's unit vs the twin's VkSampler / view, derived from
		// the census slot farGroundRtStateOf fills (the app's path) by waterSampler's rules (vkwater.inc) and the device's anisotropy.
		// Filter, wrap, compare, levels, LOD and anisotropy must match exactly; red: the twin's fgLtex anisotropy changed must FAIL.
		if ( seed == 1 ) {
#ifdef WW_VK_FARGNDRT
			glBindTexture( 0x8C1A, smp[size_t( iLtex )].tex );
			glTexParameterf( 0x8C1A, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoApp );
			glBindTexture( 0x8C1A, 0 );
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun sr;
			WwVkPresent::farGroundRtStateOf( &gw, pFg, sr, kv );
			auto stateTable = [&]( const WwVkPresent::WaterRun & r, bool print, int & nAniso, float & ltexGl, float & ltexVk ) {
				static const unsigned kTg[5] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009 }, kBd[5] = { 0x8069, 0x806A, 0x8C1D, 0x8514, 0x900A };
				GLint act = 0;
				gw.glGetIntegerv( 0x84E0, &act );
				auto mag = []( unsigned m ) { return m == 0x2600 ? "NEAREST" : "LINEAR"; };
				auto minf = []( unsigned m ) { return m == 0x2600 || m == 0x2700 || m == 0x2702 ? "NEAREST" : "LINEAR"; };
				auto isMip = []( unsigned m ) { return m >= 0x2700 && m <= 0x2703; };
				auto mipm = [&]( unsigned m ) { return !isMip( m ) ? "none" : m == 0x2702 || m == 0x2703 ? "LINEAR" : "NEAREST"; };
				auto addr = []( unsigned w ) { return w == 0x2901 ? "REPEAT" : w == 0x8370 ? "MIRROR" : w == 0x812D ? "BORDER" : "EDGE"; };
				int diff = 0, seen = 0, skipped = 0;
				nAniso = 0;
				ltexGl = ltexVk = -1.0f;
				for ( const WwVkPresent::WaterSlot & s : r.census.inputs ) {
					if ( s.kind < 0 || s.kind > 4 || s.unit < 0 || !s.tex ) {
						skipped++;
						if ( print )
							std::printf( "FARGNDRTDRAW %s state %-16s kind %d unit %d tex %u: no sampler state to check (buffer, other or unbound)\n", tag,
								s.name.c_str(), s.kind, s.unit, s.tex );
						continue;
					}
					seen++;
					// GL: the texture on the input's unit, read here (not through the census)
					gw.glActiveTexture( 0x84C0u + unsigned( s.unit ) );
					const unsigned tg = kTg[s.kind], lt = s.kind == 3 ? 0x8515u : tg;
					GLint name = 0, sobj = 0, gMag = 0, gMin = 0, gW[3] = {}, gCm = 0, gCf = 0, gBase = 0, gMax = 0;
					float gAn = 1.0f, gBias = 0.0f, gMinL = 0.0f, gMaxL = 0.0f;
					gw.glGetIntegerv( kBd[s.kind], &name );
					gw.glGetIntegerv( 0x8919, &sobj );
					gw.glGetTexParameteriv( tg, 0x2800, &gMag );
					gw.glGetTexParameteriv( tg, 0x2801, &gMin );
					gw.glGetTexParameteriv( tg, 0x2802, &gW[0] );
					gw.glGetTexParameteriv( tg, 0x2803, &gW[1] );
					gw.glGetTexParameteriv( tg, 0x8072, &gW[2] );
					gw.glGetTexParameteriv( tg, 0x884C, &gCm );
					gw.glGetTexParameteriv( tg, 0x884D, &gCf );
					gw.glGetTexParameteriv( tg, 0x813C, &gBase );
					gw.glGetTexParameteriv( tg, 0x813D, &gMax );
					gw.glGetTexParameterfv( tg, 0x84FE, &gAn );
					gw.glGetTexParameterfv( tg, 0x8501, &gBias );
					gw.glGetTexParameterfv( tg, 0x813A, &gMinL );
					gw.glGetTexParameterfv( tg, 0x813B, &gMaxL );
					const bool gMip = isMip( unsigned( gMin ) );
					int gLv = 1;	// the levels a mipmapped sample can reach: level 0 .. max level, while a level exists
					while ( gMip && gLv <= gMax && gLv < 16 ) {
						GLint lw = 0;
						gw.glGetTexLevelParameteriv( lt, gLv, 0x1000, &lw );
						if ( lw < 1 )
							break;
						gLv++;
					}
					const float gLo = gMip ? std::max( gMinL, 0.0f ) : 0.0f, gHi = gMip ? std::min( gMaxL, float( gLv - 1 ) ) : 0.0f;
					// the twin: waterSampler's VkSamplerCreateInfo from the slot (plain only without a slot / under red 2) + the view
					const bool vMip = isMip( s.minF ), vAnOn = s.aniso > 1.0f && vk().anisotropy;
					const float vAn = vAnOn ? std::min( s.aniso, vk().maxAniso ) : 1.0f;
					const bool vCmp = s.cmpMode == 0x884E, gCmp = gCm == 0x884E;
					const float vHi = vMip ? float( s.levels - 1 ) : 0.0f;	// maxLod NONE: the view's last level
					char gl[256], vv[256];
					std::snprintf( gl, sizeof( gl ), "mag %s min %s mip %s addr %s %s %s cmp %d %x levels %d base %d lod %g..%g bias %g aniso %g smpobj %d",
						mag( unsigned( gMag ) ), minf( unsigned( gMin ) ), mipm( unsigned( gMin ) ), addr( unsigned( gW[0] ) ), addr( unsigned( gW[1] ) ),
						addr( unsigned( gW[2] ) ), int( gCmp ), gCmp ? unsigned( gCf ) - 0x0200u : 0u, gMip ? gLv : 1, gBase, double( gLo ), double( gHi ),
						double( gBias ), double( gAn ), int( sobj != 0 ) );
					std::snprintf( vv, sizeof( vv ), "mag %s min %s mip %s addr %s %s %s cmp %d %x levels %d base %d lod %g..%g bias %g aniso %g smpobj %d",
						mag( s.magF ), minf( s.minF ), mipm( s.minF ), addr( s.wrap[0] ), addr( s.wrap[1] ), addr( s.wrap[2] ), int( vCmp ),
						vCmp ? s.cmpFunc - 0x0200u : 0u, vMip ? s.levels : 1, s.baseLevel, 0.0, double( vHi ), 0.0, double( vAn ), 0 );
					const bool same = std::strcmp( gl, vv ) == 0 && unsigned( name ) == s.tex;
					if ( !same )
						diff++;
					if ( gAn > 1.0f || vAn > 1.0f )
						nAniso++;
					if ( s.name == "fgLtex" )
						ltexGl = gAn, ltexVk = vAn;
					if ( print || !same )
						std::printf( "FARGNDRTDRAW %s state %-16s GL tex %u %s\nFARGNDRTDRAW %s state %-16s VK tex %u %s : %s\n", tag, s.name.c_str(),
							unsigned( name ), gl, tag, s.name.c_str(), s.tex, vv, same ? "same" : "DIFFERS" );
				}
				gw.glActiveTexture( GLenum( act ) );
				if ( print )
					std::printf( "FARGNDRTDRAW %s state: %d inputs checked, %d without sampler state, %d anisotropic, %d differ\n", tag, seen, skipped,
						nAniso, diff );
				return seen > 0 ? diff : -1;
			};
			int nAn = 0;
			float lg = 0.0f, lv = 0.0f;
			const int sDiff = stateTable( sr, true, nAn, lg, lv );
			const bool sOk = sDiff == 0 && lg == kLtexAnisoApp && lv == kLtexAnisoApp && nAn >= 1;
			std::printf( "FARGNDRTDRAW %s seed 1 state arm (fgLtex anisotropy GL %g / twin %g, the app's %g): %s\n", tag, double( lg ), double( lv ),
				double( kLtexAnisoApp ), sOk ? "OK" : "FAIL" );
			gateOk = gateOk && sOk;
			{	// red: the twin's fgLtex anisotropy other than GL's (the slot the twin's VkSampler is made from)
				WwVkPresent::WaterRun bad( sr );
				int hit = 0;
				for ( WwVkPresent::WaterSlot & s : bad.census.inputs )
					if ( s.name == "fgLtex" )
						s.aniso = 4.0f, hit++;
				const int rDiff = hit == 1 ? stateTable( bad, false, nAn, lg, lv ) : -1;
				const bool rFails = rDiff > 0;
				std::printf( "FARGNDRTDRAW %s seed 1 state red aniso (twin's fgLtex 4, GL 8): %d differ%s\n", tag, rDiff,
					rFails ? " -> red FAILS as it must" : " -> red PASSES: the state arm is blind" );
				gateOk = gateOk && rFails;
			}
			stateEnd();
			glBindTexture( 0x8C1A, smp[size_t( iLtex )].tex );
			glTexParameterf( 0x8C1A, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoPix );
			glBindTexture( 0x8C1A, 0 );
#else
			std::printf( "FARGNDRTDRAW %s seed 1 state arm: FAIL (no twin: WW_VK_FARGNDRT undefined)\n", tag );
			gateOk = false;
#endif
		}

		// the all-on arm (seed 1): cellGiHalfOn 1 on the shared screen GI images
		if ( seed == 1 ) {
			Smp * sg = nullptr, * sk = nullptr;
			for ( Smp & s : smp ) {
				if ( s.name == "cellGiHalf" )
					sg = &s;
				else if ( s.name == "cellGiHalfKey" )
					sk = &s;
			}
			if ( !sg || !sk )
				return fatal( "cellGiHalf / cellGiHalfKey not active (the all-on arm would test nothing)" );
			const GLuint plainG = sg->tex, plainK = sk->tex;
			auto getI = [&]( const char * n ) {
				float x = 0.0f;
				const GLint l = f.getUniformLocation( pFg, n );
				if ( l >= 0 ) {
					f.useProgram( pFg );
					f.getUniformfv( pFg, l, &x );
				}
				return GLint( x );
			};
			const GLint passWas = getI( "cellPass" ), splatWas = getI( "landSplat" );
			setIp( "cellPass", 0 );
			setIp( "cellProbe", 0 );
			{
				const float rect[4] = { 0.0f, 0.0f, float( W ), float( H ) }, one = 1.0f;
				setFp( "cellGiHalfRect", 4, rect );
				setFp( "cellGiHalfScale", 1, &one );
			}
			// the key: GL's own shading normal (cellProbe 1) and depth at each pixel's sample 0
			setIp( "cellGiHalfOn", 0 );
			setIp( "cellProbe", 1 );
			setIp( "landSplat", 0 );
			const std::vector<std::uint16_t> nC = glFrame();
			const std::vector<float> nD = readDepth( refDs, false );
			setIp( "cellProbe", 0 );
			setIp( "landSplat", splatWas );
			size_t written = 0, unitN = 0;
			auto makeKey = [&]( int step, int tw, int th, std::vector<float> & keyT ) {	// one texel per `step` pixels, WORLD position
				keyT.assign( size_t( tw ) * size_t( th ) * 4u, 0.0f );
				written = 0, unitN = 0;
				for ( int hy = 0; hy < th; hy++ )
					for ( int hx = 0; hx < tw; hx++ ) {
						const int x = std::min( step * hx, W - 1 ), y = std::min( step * hy, H - 1 );
						const size_t s0 = size_t( y ) * size_t( WS ) + size_t( x ) * size_t( S ), t = 4 * ( size_t( hy ) * size_t( tw ) + size_t( hx ) );
						keyT[t + 3] = -1.0f;
						if ( nD[s0] == bgD[s0] )
							continue;
						written++;
						float n[3];
						for ( int c = 0; c < 3; c++ )
							n[c] = 2.0f * h2f( nC[s0 * 4 + size_t( c )] ) - 1.0f;
						if ( std::fabs( std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] ) - 1.0f ) > 0.03f )
							continue;	// not a normal: no surface there
						unitN++;
						const float D = proj[14] / ( 2.0f * nD[s0] - 1.0f + proj[10] );
						const float xn = ( float( x ) + 0.5f ) / float( W ) * 2.0f - 1.0f, yn = ( float( y ) + 0.5f ) / float( H ) * 2.0f - 1.0f;
						const float vx = xn * D / proj[0], vy = yn * D / proj[5], vz = -D;
						keyT[t] = vx + kWx;
						keyT[t + 1] = -vz + kWy;
						keyT[t + 2] = vy + kWz;
						const float a = std::fabs( n[0] ) + std::fabs( n[1] ) + std::fabs( n[2] );
						float ox = n[0] / a, oy = n[1] / a;
						if ( n[2] < 0.0f ) {
							const float tx = ( 1.0f - std::fabs( oy ) ) * ( ox >= 0.0f ? 1.0f : -1.0f ), ty = ( 1.0f - std::fabs( ox ) ) * ( oy >= 0.0f ? 1.0f : -1.0f );
							ox = tx, oy = ty;
						}
						keyT[t + 3] = std::floor( ( ox * 0.5f + 0.5f ) * 2047.0f + 0.5f ) * 2048.0f + std::floor( ( oy * 0.5f + 0.5f ) * 2047.0f + 0.5f );
					}
			};
			std::vector<float> keyT, giT( size_t( W ) * size_t( H ) * 4u ), giT2( giT.size() );
			makeKey( 1, W, H, keyT );
			{
				std::mt19937 giRng( 20261130u );	// a side rng: the other arms' seeding stays as it was
				std::uniform_real_distribution<float> gu( 0.05f, 1.0f );
				for ( float & g : giT )
					g = gu( giRng );
				for ( float & g : giT2 )
					g = gu( giRng );
			}
			const GLuint gPlain = tex2d( W, H, fRgba32f, GL_RGBA, GL_FLOAT, giT.data(), false ),
				gPlain2 = tex2d( W, H, fRgba32f, GL_RGBA, GL_FLOAT, giT2.data(), false ),
				kPlain = tex2d( W, H, fRgba32f, GL_RGBA, GL_FLOAT, keyT.data(), false );
			auto toShared = [&]( GLuint src, int slot ) {
				f.copyImageSubData( src, GL_TEXTURE_2D, 0, 0, 0, 0, giTex[slot], GL_TEXTURE_2D, 0, 0, 0, 0, W, H, 1 );
			};
			toShared( gPlain, 0 );
			toShared( kPlain, 1 );
			const bool keyOk = written >= 1000 && unitN * 10 >= written * 9;
			std::printf( "FARGNDRTDRAW %s all-on key: %zu texels on the ground, %zu decode to a unit normal (want >= 1000, >= 90 %%): %s\n", tag,
				written, unitN, keyOk ? "OK" : "FAIL" );
			if ( !keyOk )
				gateOk = false;
			sg->tex = giTex[0], sk->tex = giTex[1];
			setIp( "cellGiHalfOn", 1 );
			const std::vector<std::uint16_t> aC = glFrame();
			const std::vector<float> aD = readDepth( refDs, false );
			std::vector<char> aMask( aD.size(), 0 );
			size_t aCov = 0;
			for ( size_t s = 0; s < aD.size(); s++ ) {
				bool c = aD[s] != bgD[s];
				for ( int ch = 0; ch < 4; ch++ )
					c = c || aC[s * 4 + size_t( ch )] != bgC[s * 4 + size_t( ch )];
				aMask[s] = c;
				aCov += c;
			}
			setIp( "cellGiHalfOn", 0 );
			const size_t mOff = movedIn( aMask, aC, glFrame() );
			setIp( "cellGiHalfOn", 1 );
			toShared( gPlain2, 0 );
			const size_t mImg = movedIn( aMask, aC, glFrame() );
			toShared( gPlain, 0 );
			for ( int pr = 0; pr < 2; pr++ ) {
				const size_t m = pr == 0 ? mOff : mImg;
				const bool ok = aCov > 0 && m * 5 >= aCov;
				std::printf( "FARGNDRTDRAW %s all-on GL probe %s: moved %zu of %zu covered samples (%.1f %%): %s\n", tag,
					pr == 0 ? "cellGiHalfOn 0" : "cellGiHalf reseeded", m, aCov, aCov ? 100.0 * double( m ) / double( aCov ) : 0.0,
					ok ? "OK" : "FAIL (the screen GI does not reach the colour)" );
				if ( !ok )
					gateOk = false;
			}
			auto vkRun = [&]( const char * name, const std::function<void( WwVkPresent::WaterRun & )> & between,
					const std::vector<std::uint16_t> * refC = nullptr, const std::vector<float> * refD = nullptr ) {
				std::string w;
				const bool ran = vkGo( kv, 0, between, w );
				std::string word = "REFUSED";
				if ( ran ) {
					const Verdict v = judge( bgC, bgD, readColor( fc ), readDepth( fds, true ), refC ? *refC : aC, refD ? *refD : aD );
					word = v.word();
					std::printf( "FARGNDRTDRAW %s all-on %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
						"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g)", tag, name, word.c_str(), v.covered, v.both, v.onlyOne,
						v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel );
				} else
					std::printf( "FARGNDRTDRAW %s all-on %s: REFUSED (%s)", tag, name, w.c_str() );
				return std::make_pair( word, w );
			};
			int cleanRuns = 0, cleanBound = 0, redsFailing = 0;
			auto clean = [&]( const char * name ) {
				const auto res = vkRun( name, nullptr );
				const bool ok = res.first == "BOUND" || res.first == "EXACT";
				cleanRuns++;
				cleanBound += ok;
				std::printf( "%s\n", ok ? "" : " -> FAIL (must be BOUND)" );
				if ( !ok )
					gateOk = false;
			};
			auto red = [&]( bool ok ) {
				redsFailing += ok;
				std::printf( "%s\n", ok ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" );
				if ( !ok )
					gateOk = false;
			};
			clean( "clean" );
			sg->tex = gPlain;	// red not shared: the same bytes in a plain GL texture
			{
				const auto res = vkRun( "red not shared", nullptr );
				red( res.first == "REFUSED" && res.second.find( "cellGiHalf:" ) != std::string::npos );
			}
			sg->tex = giTex[0];
			clean( "clean after red not shared" );
			sg->tex = giTex[1], sk->tex = giTex[0];	// red wrong image: the two shared images swapped on the twin side
			{
				const auto res = vkRun( "red wrong image", nullptr );
				red( res.first == "FAIL" || ( res.first == "REFUSED" && res.second.find( "no twin" ) == std::string::npos ) );
			}
			sg->tex = giTex[0], sk->tex = giTex[1];
			clean( "clean after red wrong image" );
			{	// red stale gen: the run taken, then the owner remakes cellGiHalf (a sentinel in the new image)
				const std::uint64_t gen0 = WwVkPresent::rtImgGen();
				unsigned remade = 0;
				bool remadeOk = false;
				std::string rw;
				const auto res = vkRun( "red stale gen", [&]( WwVkPresent::WaterRun & ) {
					WwVkPresent::rtImgDrop( 0, giKeyOf( 0 ) );
					remadeOk = WwVkPresent::rtImgShare( 0, giKeyOf( 0 ), W, H, 0, 0, remade, rw );
					const float sentinel[4] = { 64.0f, 64.0f, 64.0f, 1.0f };
					if ( remadeOk )
						clearTex( remade, 0, GL_RGBA, GL_FLOAT, sentinel );
				} );
				const bool genMoved = WwVkPresent::rtImgGen() != gen0;
				std::printf( " [remade %s: GL texture %u -> %u, rtImgGen %s]", remadeOk ? "OK" : rw.c_str(), giTex[0], remade,
					genMoved ? "moved" : "did NOT move" );
				red( remadeOk && genMoved && res.first != "BOUND" && res.first != "EXACT" && res.second.find( "no twin" ) == std::string::npos );
				if ( remadeOk ) {
					giTex[0] = remade;
					sg->tex = remade;
					toShared( gPlain, 0 );
				}
			}
			clean( "clean after red stale gen" );
			{	// red key mismatch: a borrowed entry of a key that sorts first, on cellGiHalf's GL name (farSharedOf answers it)
				const Shared * own = sharedFind( giKeyOf( 0 ) );
				const std::uintptr_t lowKey = 16;
				std::string bw = "no shared entry of the owner's key";
				bool borrowed = false;
				if ( own && lowKey < giKeyOf( 0 ) ) {
					WwVkPresent::Borrow b;
					b.device = vk().dev, b.image = own->img, b.view = own->view, b.tex = own->tex, b.glFmt = 0x8814;
					b.w = own->w, b.h = own->h;
					borrowed = WwVkPresent::borrowTarget( lowKey, b, bw );
				}
				const std::uintptr_t answer = farSharedOf( giTex[0] );
				const auto res = vkRun( "red key mismatch", nullptr );
				std::printf( " [borrowed key %llu: %s; owner's key %llu; farSharedOf %llu]", static_cast<unsigned long long>( lowKey ),
					borrowed ? "OK" : bw.c_str(), static_cast<unsigned long long>( giKeyOf( 0 ) ), static_cast<unsigned long long>( answer ) );
				if ( borrowed )
					WwVkPresent::borrowDrop( lowKey );
				red( borrowed && answer == lowKey && res.first == "REFUSED"
					&& res.second.find( "cellGiHalf: the shared image is not the bound name's" ) != std::string::npos );
			}
			clean( "clean after red key mismatch" );
			{	// red no sampler: cellGiHalf out of the run's census
				const auto res = vkRun( "red no sampler", []( WwVkPresent::WaterRun & r ) {
					auto & in = r.census.inputs;
					in.erase( std::remove_if( in.begin(), in.end(), []( const WwVkPresent::WaterSlot & x ) { return x.name == "cellGiHalf"; } ),
						in.end() );
				} );
				red( res.first == "REFUSED" && res.second.find( "cellGiHalf: GL's program has no such sampler" ) != std::string::npos );
			}
			clean( "clean after red no sampler" );
			std::printf( "FARGNDRTDRAW %s all-on: clean %d of %d BOUND, reds failing %d of 5\n", tag, cleanBound, cleanRuns, redsFailing );
			{	// the half-rate arm (header)
				const int hw = WwVkPresent::rtImgSize( W, 2 ), hh = WwVkPresent::rtImgSize( H, 2 );
				unsigned half[2] = { 0, 0 };
				for ( int slot = 0; slot < 2; slot++ ) {
					std::string rw;
					WwVkPresent::rtImgDrop( slot, giKeyOf( slot ) );	// its GL name goes too
					if ( !WwVkPresent::rtImgShare( slot, giKeyOf( slot ), hw, hh, 0, 0, half[slot], rw ) )
						return fatal( std::string( "rtImgShare half " ) + WwVkPresent::rtImgName( slot ) + ": " + rw );
				}
				std::vector<float> keyH, giH( size_t( hw ) * size_t( hh ) * 4u ), giH2( giH.size() );
				makeKey( 2, hw, hh, keyH );
				{
					std::mt19937 hRng( 20261131u );	// its own side rng
					std::uniform_real_distribution<float> gu( 0.05f, 1.0f );
					for ( float & g : giH )
						g = gu( hRng );
					for ( float & g : giH2 )
						g = gu( hRng );
				}
				const GLuint hPlain = tex2d( hw, hh, fRgba32f, GL_RGBA, GL_FLOAT, giH.data(), false ),
					hPlain2 = tex2d( hw, hh, fRgba32f, GL_RGBA, GL_FLOAT, giH2.data(), false ),
					hKey = tex2d( hw, hh, fRgba32f, GL_RGBA, GL_FLOAT, keyH.data(), false );
				auto toHalf = [&]( GLuint src, int slot ) {
					f.copyImageSubData( src, GL_TEXTURE_2D, 0, 0, 0, 0, half[slot], GL_TEXTURE_2D, 0, 0, 0, 0, hw, hh, 1 );
				};
				toHalf( hPlain, 0 );
				toHalf( hKey, 1 );
				const bool hKeyOk = written >= 250 && unitN * 10 >= written * 9;
				std::printf( "FARGNDRTDRAW %s half rate (%dx%d): key %zu texels on the ground, %zu decode to a unit normal (want >= 250, >= 90 %%): %s\n",
					tag, hw, hh, written, unitN, hKeyOk ? "OK" : "FAIL" );
				if ( !hKeyOk )
					gateOk = false;
				sg->tex = half[0], sk->tex = half[1];
				const float hRect[4] = { 0.0f, 0.0f, float( hw ), float( hh ) }, hScale = 0.5f, one = 1.0f;
				setFp( "cellGiHalfRect", 4, hRect );
				setFp( "cellGiHalfScale", 1, &hScale );
				const std::vector<std::uint16_t> hC = glFrame();
				const std::vector<float> hD = readDepth( refDs, false );
				std::vector<char> hMask( hD.size(), 0 );
				size_t hCov = 0;
				for ( size_t s = 0; s < hD.size(); s++ ) {
					bool c = hD[s] != bgD[s];
					for ( int ch = 0; ch < 4; ch++ )
						c = c || hC[s * 4 + size_t( ch )] != bgC[s * 4 + size_t( ch )];
					hMask[s] = c;
					hCov += c;
				}
				size_t hm[3];
				setIp( "cellGiHalfOn", 0 );
				hm[0] = movedIn( hMask, hC, glFrame() );
				setIp( "cellGiHalfOn", 1 );
				toHalf( hPlain2, 0 );
				hm[1] = movedIn( hMask, hC, glFrame() );
				toHalf( hPlain, 0 );
				setFp( "cellGiHalfScale", 1, &one );
				hm[2] = movedIn( hMask, hC, glFrame() );
				setFp( "cellGiHalfScale", 1, &hScale );
				const char * hProbe[3] = { "cellGiHalfOn 0", "cellGiHalf reseeded", "cellGiHalfScale 1" };
				for ( int pr = 0; pr < 3; pr++ ) {
					const bool ok = hCov > 0 && hm[pr] * 5 >= hCov;
					std::printf( "FARGNDRTDRAW %s half rate GL probe %s: moved %zu of %zu covered samples (%.1f %%): %s\n", tag, hProbe[pr], hm[pr],
						hCov, hCov ? 100.0 * double( hm[pr] ) / double( hCov ) : 0.0, ok ? "OK" : "FAIL (the half-rate read does not reach the colour)" );
					if ( !ok )
						gateOk = false;
				}
				int hClean = 0, hRed = 0;
				auto hRun = [&]( const char * name, const std::function<void( WwVkPresent::WaterRun & )> & between, bool isRed ) {
					const auto res = vkRun( name, between, &hC, &hD );
					const bool bound = res.first == "BOUND" || res.first == "EXACT";
					const bool ok = isRed ? res.first == "FAIL" : bound;
					( isRed ? hRed : hClean ) += ok;
					std::printf( "%s\n", isRed ? ( ok ? " -> red FAILS as it must" : " -> red does not FAIL: the gate is blind" )
						: ( ok ? "" : " -> FAIL (must be BOUND)" ) );
					if ( !ok )
						gateOk = false;
				};
				hRun( "half rate clean", nullptr, false );
				hRun( "half rate red half rate ignored", [&]( WwVkPresent::WaterRun & r ) {
#ifdef WW_VK_FARGNDRT
					const WwVkPresent::FxField * fs = kv == 2 ? WwVkPresent::kT_fargndDgPs : WwVkPresent::kT_fargndRtPs;
					const size_t nf = kv == 2 ? sizeof( WwVkPresent::kT_fargndDgPs ) / sizeof( WwVkPresent::kT_fargndDgPs[0] )
						: sizeof( WwVkPresent::kT_fargndRtPs ) / sizeof( WwVkPresent::kT_fargndRtPs[0] );
					for ( size_t i = 0; i < nf; i++ )
						if ( std::strcmp( fs[i].name, "cellGiHalfScale" ) == 0 && fs[i].offset + 4 <= r.psBytes )
							std::memcpy( r.psLoose + fs[i].offset, &one, 4 );
#else
					( void )r;
#endif
				}, true );
				hRun( "half rate clean after red", nullptr, false );
				std::printf( "FARGNDRTDRAW %s half rate: clean %d of 2 BOUND, reds failing %d of 1\n", tag, hClean, hRed );
				for ( int slot = 0; slot < 2; slot++ ) {	// both images at W x H again
					std::string rw;
					WwVkPresent::rtImgDrop( slot, giKeyOf( slot ) );
					if ( !WwVkPresent::rtImgShare( slot, giKeyOf( slot ), W, H, 0, 0, giTex[slot], rw ) )
						return fatal( std::string( "rtImgShare full " ) + WwVkPresent::rtImgName( slot ) + ": " + rw );
				}
				setFp( "cellGiHalfScale", 1, &one );
				const GLuint hDel[3] = { hPlain, hPlain2, hKey };
				glDeleteTextures( 3, hDel );
			}
			sg->tex = plainG, sk->tex = plainK;
			setIp( "cellGiHalfOn", 0 );
			setIp( "cellPass", passWas );
			const GLuint del[3] = { gPlain, gPlain2, kPlain };
			glDeleteTextures( 3, del );
		}
	}
	std::printf( "FARGNDRTDRAW %s %d runs (the base twin: %d on Vulkan, %d fails)\n", tag, runs, tot.runs, tot.fails );
	std::printf( gateOk ? "FARGNDRTDRAW GATE OK\n" : "FARGNDRTDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
