// Lane VKGROUND17 (2026-11-16): far ground mode 2 (fo4_cellcsmfgp, Show > Far ground: Live, cached pages) under the RT GI keys
// the all-on route links: RT = FOGLEAK + RESTIR + RTDDGI + RTDDGI_CASCADES + FRAME_UBO (twin 1, fxWork kind 25), DG = the same
// without RESTIR (twin 2, kind 26). GENERATED from fgpagesdraw.cpp (mode 2's fixture: fgCells, fgPagesN, fgPagesF, fgMode 3) by
// E:/Projects/Claude/notes/vkground9/tools/mk_fgprt.py with mk_fargndrt.py's RT parts (fargndrtdraw.cpp's header says what each
// arm judges; the bar, reds and arms are the same). One key a run: fgpagesrtdraw.exe <prep dir> rt|dg; fargndrtdraw.sh runs
// mode 1 and mode 2, both keys, and is OK only at 4 of 4. GL's side is the HLSL-built SPIR-V of the key; the legacy .frag of
// the same key gives the sampler types and the fill order (BATCH1118), mode 1's throwaway fills kept at fgCells' place.
// Every Vulkan call goes through one lambda: with WW_VK_FARGNDPRT (defined by vkfargroundrt.inc) farGroundLiveRtStateOf /
// farGroundLiveRtRun / takeFarGroundLiveRtDrawStat( v ), tables kT_fargndpRtPs / kT_fargndpDgPs; without it every arm is
// REFUSED "no twin" and the gate FAILS (step 1: the bar before the twin). Twin 0 (red key) = mode 2's base twin
// (farGroundLiveRun).
// Anisotropy (manager ruling 11-16, kLtexAnisoPix): mode 2 has no fgLtex; its anisotropic inputs are the page pools fgPagesN /
// fgPagesF (8 in the app). By the ruling's rule the pixel compare runs both at 1 on both sides, and the state arm (seed 1)
// sets the app's 8 on fgPagesN and checks every census input's sampling state; red: the twin's fgPagesN anisotropy 4.
// Seeding: the DG key's legacy program has no cellRestir, so its rng stream seeds volPack under one texel of the air volume
// (wwGridAt's clamp undefined); the gate raises each share to one texel and prints it (the app's shares are whole tiles).
// The copies check: fgCells, fgPagesN, fgPagesF read in place (3 fewer a run than on plain twins).
// Last line `FARGNDRTDRAW2 GATE OK` or `FARGNDRTDRAW2 GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

#include "features/vkframe/vkrtbuf.h"
#include <functional>

namespace fgd
{
using namespace fv;
using namespace fld;

// a cell is 4096 world units (cell_farground.glsl); the world square (4350..5650, 7150..8600) straddles (5000, 7400), the
// corner in front of the slab (the left half's occluder at view z -700 = world y 7700; fgp1 had it at 7900: the no-page
// cell sat behind the slab and its probe moved 0)
const int kCn = 3, kSideN = 256, kSideF = 128, kLayers = 3;
const float kOx = 5000.0f - 4096.0f, kOy = 7400.0f - 4096.0f, kSpacing = 128.0f, kTiling = 400.0f;
const int kDdgiDims[4] = { 9, 7, 8, 9 * 7 * 8 };
const size_t kProbes = size_t( 9 * 7 * 8 ), kAtlasTexels = 193 * kProbes;
const float kWx = 5000.0f, kWy = 7000.0f, kWz = 300.0f;	// world = (x + kWx, -z + kWy, y + kWz)
// manager ruling 11-16: NV GL vs NV VK 8x anisotropic filtering differs on partly covered primitives (VKGROUND15 proof); pixel compare at 1, state checked at 8
const float kLtexAnisoPix = 1.0f, kLtexAnisoApp = 8.0f;	// fgPagesN: the pixel compare's (both sides), the app's (cellfarground.cpp, the state arm)

size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	size_t n = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
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
		std::printf( "FARGNDRTDRAW2 SPIR-V: %s\n", !bin || !spec ? "no glShaderBinary / glSpecializeShader" : "no prog.vert.spv / prog.frag.spv" );
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
			std::printf( "FARGNDRTDRAW2 SPIR-V %s stage refused: %.1500s\n", s ? "pixel" : "vertex", log.data() );
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
		std::printf( "FARGNDRTDRAW2 SPIR-V link failed: %.1500s\n", log.data() );
		return 0;
	}
	return p;
}

int levelsOf( int side )
{
	int n = 0;
	for ( int s = side; s >= 4; s >>= 1 )
		n++;
	return n;
}

// fgCells: r pool (0 none, 1 near, 2 far), g layer, b fade; red: the near and far cells swapped
std::vector<std::uint16_t> cellsData( bool red )
{
	std::vector<std::uint16_t> c( size_t( kCn * kCn * 4 ), 0 );
	auto put = [&]( int x, int y, int pool, int layer, int fade ) {
		std::uint16_t * p = &c[size_t( ( y * kCn + x ) * 4 )];
		p[0] = std::uint16_t( pool ), p[1] = std::uint16_t( layer ), p[2] = std::uint16_t( fade ), p[3] = 0;
	};
	for ( int y = 0; y < kCn; y++ )
		for ( int x = 0; x < kCn; x++ )
			put( x, y, 2, 0, 255 );
	put( 0, 0, red ? 2 : 1, 1, 255 );	// near, layer 1
	put( 1, 0, red ? 1 : 2, 2, 255 );	// far, layer 2
	put( 0, 1, 0, 0, 255 );				// no page: the sheet
	put( 1, 1, 1, 2, 140 );				// near, layer 2, faded over the sheet
	return c;
}

// the no-page cell given the near pool's layer 0 (the probe that it is on screen)
std::vector<std::uint16_t> cellsPaged()
{
	std::vector<std::uint16_t> c = cellsData( false );
	c[size_t( ( 1 * kCn + 0 ) * 4 )] = 1;
	return c;
}

// the faded cell at full fade (the probe that it is on screen)
std::vector<std::uint16_t> cellsUnfaded()
{
	std::vector<std::uint16_t> c = cellsData( false );
	c[size_t( ( 1 * kCn + 1 ) * 4 + 2 )] = 255;
	return c;
}

// one 4 x 4 block of RGB (0..1) into BC1: the bounding box's corners as the 565 ends, each texel its nearest of 4 colours
void bc1Block( const float * px, std::uint8_t * out )
{
	float lo[3] = { 1.0f, 1.0f, 1.0f }, hi[3] = { 0.0f, 0.0f, 0.0f };
	for ( int t = 0; t < 16; t++ )
		for ( int c = 0; c < 3; c++ )
			lo[c] = std::min( lo[c], px[t * 3 + c] ), hi[c] = std::max( hi[c], px[t * 3 + c] );
	auto q565 = []( const float * v ) {
		auto q = []( float x, int m ) { return std::uint16_t( std::lround( std::clamp( x, 0.0f, 1.0f ) * float( m ) ) ); };
		return std::uint16_t( ( q( v[0], 31 ) << 11 ) | ( q( v[1], 63 ) << 5 ) | q( v[2], 31 ) );
	};
	auto un565 = []( std::uint16_t e, float * v ) {
		v[0] = float( e >> 11 ) / 31.0f, v[1] = float( ( e >> 5 ) & 63 ) / 63.0f, v[2] = float( e & 31 ) / 31.0f;
	};
	std::uint16_t e0 = q565( hi ), e1 = q565( lo );
	if ( e0 < e1 )
		std::swap( e0, e1 );
	float pal[4][3];
	un565( e0, pal[0] );
	un565( e1, pal[1] );
	for ( int c = 0; c < 3; c++ ) {
		pal[2][c] = ( 2.0f * pal[0][c] + pal[1][c] ) / 3.0f;
		pal[3][c] = ( pal[0][c] + 2.0f * pal[1][c] ) / 3.0f;
	}
	std::uint32_t bits = 0;
	for ( int t = 0; t < 16 && e0 != e1; t++ ) {
		int best = 0;
		float bd = 1e30f;
		for ( int k = 0; k < 4; k++ ) {
			float d = 0.0f;
			for ( int c = 0; c < 3; c++ )
				d += ( px[t * 3 + c] - pal[k][c] ) * ( px[t * 3 + c] - pal[k][c] );
			if ( d < bd )
				bd = d, best = k;
		}
		bits |= std::uint32_t( best ) << ( 2 * t );
	}
	out[0] = std::uint8_t( e0 & 0xFF ), out[1] = std::uint8_t( e0 >> 8 ), out[2] = std::uint8_t( e1 & 0xFF ), out[3] = std::uint8_t( e1 >> 8 );
	for ( int k = 0; k < 4; k++ )
		out[4 + k] = std::uint8_t( bits >> ( 8 * k ) );
}

// one pool: a smooth field a layer (phase ph, mean moved by dm), its 2x2-box mip chain down to 4 px, each level's layers in
// BC1 blocks
std::vector<std::vector<std::uint8_t>> pagesData( int side, float ph, float dm = 0.0f )
{
	std::vector<std::vector<std::uint8_t>> out;
	std::vector<float> lv( size_t( side ) * size_t( side ) * kLayers * 3 );
	const float k = 6.2831853f;
	for ( int z = 0; z < kLayers; z++ )
		for ( int j = 0; j < side; j++ )
			for ( int i = 0; i < side; i++ ) {
				const float u = ( float( i ) + 0.5f ) / float( side ), v = ( float( j ) + 0.5f ) / float( side );
				float * p = &lv[( ( size_t( z ) * side + size_t( j ) ) * side + size_t( i ) ) * 3];
				p[0] = 0.45f + dm + 0.35f * std::sin( k * ( 1.3f * u + 0.7f * v + ph + 0.31f * float( z ) ) );
				p[1] = 0.45f + dm + 0.35f * std::sin( k * ( 0.6f * u - 1.1f * v + 2.0f * ph + 0.17f * float( z ) ) );
				p[2] = 0.40f + dm + 0.30f * std::cos( k * ( 0.9f * u + 1.4f * v + 3.0f * ph + 0.23f * float( z ) ) );
			}
	for ( int s = side; s >= 4; s >>= 1 ) {
		const int nb = s / 4;
		std::vector<std::uint8_t> blocks( size_t( nb ) * size_t( nb ) * 8 * kLayers );
		for ( int z = 0; z < kLayers; z++ )
			for ( int by = 0; by < nb; by++ )
				for ( int bx = 0; bx < nb; bx++ ) {
					float px[48];
					for ( int t = 0; t < 16; t++ )
						for ( int c = 0; c < 3; c++ )
							px[t * 3 + c] = lv[( ( size_t( z ) * s + size_t( by * 4 + t / 4 ) ) * s + size_t( bx * 4 + t % 4 ) ) * 3 + size_t( c )];
					bc1Block( px, &blocks[( ( size_t( z ) * nb + size_t( by ) ) * nb + size_t( bx ) ) * 8] );
				}
		out.push_back( blocks );
		const int h = s / 2;
		std::vector<float> dn( size_t( h ) * size_t( h ) * kLayers * 3 );
		for ( int z = 0; z < kLayers; z++ )
			for ( int j = 0; j < h; j++ )
				for ( int i = 0; i < h; i++ )
					for ( int c = 0; c < 3; c++ ) {
						auto at = [&]( int x, int y ) { return lv[( ( size_t( z ) * s + size_t( y ) ) * s + size_t( x ) ) * 3 + size_t( c )]; };
						dn[( ( size_t( z ) * h + size_t( j ) ) * h + size_t( i ) ) * 3 + size_t( c )] =
							0.25f * ( at( 2 * i, 2 * j ) + at( 2 * i + 1, 2 * j ) + at( 2 * i, 2 * j + 1 ) + at( 2 * i + 1, 2 * j + 1 ) );
					}
		lv.swap( dn );
	}
	return out;
}
}  // namespace fgd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace fgd;
	gTag = "FARGNDRTDRAW2";
	if ( argc < 3 ) {
		std::printf( "usage: fgpagesrtdraw <prep dir> rt|dg\nFARGNDRTDRAW2 GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARGNDRTDRAW2 GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const std::string keyTag = argv[2];
	const int kv = keyTag == "rt" ? 1 : keyTag == "dg" ? 2 : 0;
	if ( !kv )
		return fatal( "the key is rt or dg, not " + keyTag );
	const char * tag = kv == 1 ? "RT" : "DG";
	const char * keyName = kv == 1 ? "HLSL-built SPIR-V fo4_cellcsmfgp@WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO"
		: "HLSL-built SPIR-V fo4_cellcsmfgp@WW_CELL_FOGLEAK+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO";
	std::printf( "FARGNDRTDRAW2 key %s (twin %d)\n", tag, kv );
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfgpagesrtdraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfgpagesrtdraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfgpagesrtdraw", "far ground draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "FARGNDRTDRAW2 GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	// BATCH1117: GL's side is the HLSL-built program (the HLSL is the reference); the legacy .frag only names the sampler
	// types SPIR-V does not carry and the fill order, and is run instead with FARGNDRTDRAW2_LEGACY=1
	const char * legacyEnv = std::getenv( "FARGNDRTDRAW2_LEGACY" );
	const bool legacy = legacyEnv && std::string( legacyEnv ) == "1";
	const GLuint pLegacy = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog_" + keyTag + ".frag.glsl" ), "fo4_cellcsmfgp legacy" );
	const GLuint pFg = legacy ? pLegacy : spvProgram( readFile( dir + "/prog.vert.spv" ), readFile( dir + "/prog_" + keyTag + ".frag.spv" ) );
	std::printf( "FARGNDRTDRAW2 GL program: %s (legacy fo4_cellcsmfgp.frag %s, info only)\n",
		legacy ? "the legacy fo4_cellcsmfgp.frag (FARGNDRTDRAW2_LEGACY=1)" : keyName, pLegacy ? "links" : "does not link" );
	if ( !pLegacy )
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
	if ( !pUnpack || !pDepth || !pSlab || !pFg )
		return fatal( "GL programs" );
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
	std::printf( "FARGNDRTDRAW2 shared: cellRestir %zu bytes (buffer %u), cellDdgiAtlas %zu bytes (buffer %u), cellGiHalf texture %u, "
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
			std::printf( "FARGNDRTDRAW2 shared %s: rtBufOf refused (%s)\n", name, w.c_str() );
			return false;
		}
		glFinish();
		std::vector<std::uint8_t> seen;
		if ( !WwVkPresent::farTexelBufferRead( rb.key, seen, w ) ) {
			std::printf( "FARGNDRTDRAW2 shared %s: farTexelBufferRead refused (%s)\n", name, w.c_str() );
			return false;
		}
		const size_t compared = std::min( seen.size(), want.size() ), bad = differ( seen, want );
		const bool ok = compared > 0 && bad == 0 && rb.glBuf == buf && rb.view != 0;
		std::printf( "FARGNDRTDRAW2 shared %s: %zu bytes compared, %zu differ, GL buffer %u (bound %u), view %s: %s\n", name, compared, bad,
			rb.glBuf, buf, rb.view ? "set" : "none", ok ? "OK" : "FAIL" );
		return ok;
	};

	// ---- the program's samplers: one unit each; the far ground's four as cellfarground.cpp makes them
	std::vector<Smp> smp;
	int iCells = -1, iPagesN = -1, iPagesF = -1;
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
			if ( s.name == "fgGrid" || s.name == "fgLtex" || s.name == "fgDecal" || s.name == "fgAtlas" )
				return fatal( "GL's program reads " + s.name + ": not mode 2's (cached pages) far ground" );
			if ( s.name == "fgCells" ) {
				s.unbound = false;	// made below (RGBA16UI, nearest, shared)
				s.fmt = 0x8D76;
				s.w = s.h = kCn;
				iCells = at;
			} else if ( s.name == "fgPagesN" || s.name == "fgPagesF" ) {
				s.fmt = 0x83F0;	// BC1 2D arrays as allocPool makes them (below)
				s.w = s.h = s.name == "fgPagesN" ? kSideN : kSideF;
				s.d = kLayers;
				s.levels = levelsOf( s.w );
				( s.name == "fgPagesN" ? iPagesN : iPagesF ) = at;
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
	if ( iCells < 0 || iPagesN < 0 || iPagesF < 0 )
		return fatal( "the program has no fgCells / fgPagesN / fgPagesF (not the cached-pages far ground)" );
	if ( !hasAtlas || hasRestir != ( kv == 1 ) )
		return fatal( std::string( tag ) + " program: cellDdgiAtlas " + ( hasAtlas ? "active" : "missing" ) + ", cellRestir "
			+ ( hasRestir ? "active" : "missing" ) + " (not the key's RT inputs)" );
	for ( const char * n : { "cellGiHalfOn", "cellGiHalfScale", "cellGiHalfRect", "cellDdgiOn", "cellProbe" } )
		if ( f.getUniformLocation( pFg, n ) < 0 )
			return fatal( std::string( n ) + " has no location in GL's program (an arm would test nothing)" );
	{
		std::string miss;
		int nMiss = 0;
#ifdef WW_VK_FARGNDPRT
		const WwVkPresent::FxField * fsT = kv == 1 ? WwVkPresent::kT_fargndpRtPs : WwVkPresent::kT_fargndpDgPs;
		const size_t nfsT = kv == 1 ? sizeof( WwVkPresent::kT_fargndpRtPs ) / sizeof( WwVkPresent::kT_fargndpRtPs[0] )
			: sizeof( WwVkPresent::kT_fargndpDgPs ) / sizeof( WwVkPresent::kT_fargndpDgPs[0] );
		for ( size_t i = 0; i < nfsT; i++ )
			if ( f.getUniformLocation( pFg, fsT[i].name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + fsT[i].name;
			}
#else
		std::printf( "FARGNDRTDRAW2 no twin table (WW_VK_FARGNDPRT undefined): the pixel fields are not checked\n" );
#endif
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterVs )
			if ( f.getUniformLocation( pFg, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " vs:" ) + fd.name;
			}
		std::printf( "FARGNDRTDRAW2 %d twin fields GL has no location for:%s\n", nMiss, miss.c_str() );
	}
	std::printf( "FARGNDRTDRAW2 %zu samplers active in GL's program:", smp.size() );
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
	for ( int k = 0; k < 3; k++ ) {
		const GLuint bi = f.getUniformBlockIndex( pFg, blockName[k] );
		if ( bi != 0xFFFFFFFFu ) {
			f.uniformBlockBinding( pFg, bi, GLuint( k ) );
			f.getActiveUniformBlockiv( pFg, bi, fBlockSize, &blockSize[k] );
		}
		std::vector<std::uint8_t> z( size_t( std::max( blockSize[k], 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[k] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
	}
	std::printf( "FARGNDRTDRAW2 blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes\n", blockSize[0], blockSize[1], blockSize[2] );
	struct ShMember
	{
		std::string name;
		GLint off = 0, type = 0, size = 1, stride = 0;
	};
	std::vector<ShMember> shMembers;
	{
		const GLuint sbi = f.getUniformBlockIndex( pFg, "wwShadowFrame" );
		GLint n = 0;
		f.getProgramiv( pFg, fActiveUnis, &n );
		for ( GLint i = 0; i < n && sbi != 0xFFFFFFFFu; i++ ) {
			const GLuint idx = GLuint( i );
			GLint bi = -1;
			f.getActiveUniformsiv( pFg, 1, &idx, fBlockIndex, &bi );
			if ( bi != GLint( sbi ) )
				continue;
			ShMember m;
			char nm[256] = {};
			GLenum ty = 0;
			f.getActiveUniform( pFg, idx, 256, nullptr, &m.size, &ty, nm );
			m.name = nm;
			m.type = GLint( ty );
			f.getActiveUniformsiv( pFg, 1, &idx, 0x8A3B, &m.off );     // GL_UNIFORM_OFFSET
			f.getActiveUniformsiv( pFg, 1, &idx, 0x8A3C, &m.stride );  // GL_UNIFORM_ARRAY_STRIDE
			shMembers.push_back( m );
		}
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
	std::printf( "FARGNDRTDRAW2 job: %dx%d x%d, %zu vertices (baseVertex %d), %u elements from %u, cells %d x %d, pages %d / %d px x %d, "
		"slab depth %.6f\n", W, H, S, vtx.size(), base, count, first, kCn, kCn, kSideN, kSideF, kLayers, zd );

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
			f.bindBufferBase( fUbo, GLuint( k ), ubo[k] );
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
	auto putCells = [&]( const std::vector<std::uint16_t> & c ) {
		glBindTexture( GL_TEXTURE_2D, smp[size_t( iCells )].tex );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kCn, kCn, 0x8D99 /* GL_RGBA_INTEGER */, GL_UNSIGNED_SHORT, c.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
	};
	using CompSub3 = void( WINAPI * )( GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void * );
	const auto compSub3 = reinterpret_cast<CompSub3>( wglGetProcAddress( "glCompressedTexSubImage3D" ) );
	if ( !compSub3 )
		return fatal( "glCompressedTexSubImage3D" );
	auto putPages = [&]( int ix, const std::vector<std::vector<std::uint8_t>> & lv ) {
		const Smp & s = smp[size_t( ix )];
		glBindTexture( 0x8C1A, s.tex );
		for ( int l = 0; l < s.levels && l < int( lv.size() ); l++ )
			compSub3( 0x8C1A, l, 0, 0, 0, s.w >> l, s.h >> l, s.d, 0x83F0, GLsizei( lv[size_t( l )].size() ), lv[size_t( l )].data() );
		glBindTexture( 0x8C1A, 0 );
	};
	static char twinKeys[192];
	bool gateOk = true;
	int runs = 0;
	/* lane VKGROUND5: all three of mode 2's inputs on the present device as cellfarground.cpp makes them while the view draws
	 * mode 2: fgCells by sharedTarget (slot 3), the pools as far twins (slots 4 / 5, allocPoolIn: BC1, mips to 4 px,
	 * LINEAR_MIPMAP_LINEAR, clamped, anisotropy 8). The twin reads them in place (no copy a run); plain GL twins of the same
	 * contents give the GL-vs-GL check and the copy count (the plain run copies exactly kSh more) */
	using CopyImg = void( WINAPI * )( GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei,
		GLsizei, GLsizei );
	const auto copyImg = reinterpret_cast<CopyImg>( wglGetProcAddress( "glCopyImageSubData" ) );
	if ( !copyImg )
		return fatal( "glCopyImageSubData" );
	constexpr int kSh = 3;
	static char sharedKeys[kSh];
	const int iShared[kSh] = { iCells, iPagesN, iPagesF };
	GLuint plain[kSh] = {};
	for ( int k = 0; k < kSh; k++ ) {
		Smp & s = smp[size_t( iShared[k] )];
		const unsigned tg = targetOf( s.kind );
		const bool pool = k > 0;
		const std::uintptr_t key = reinterpret_cast<std::uintptr_t>( sharedKeys + k );
		unsigned t = 0;
		if ( pool ? !WwVkPresent::farTwin( key, 2, s.w, s.h, s.d, s.fmt, t, why, s.levels )
			: !WwVkPresent::sharedTarget( key, s.kind == 2 ? 2 : 0, s.w, s.h, s.d, s.fmt, t, why, s.levels ) )
			return fatal( std::string( pool ? "farTwin " : "sharedTarget " ) + s.name + ": " + why );
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
			glTexParameteri( tg, GL_TEXTURE_MIN_FILTER, pool ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST );	// cells: tex2D's nearest
			glTexParameteri( tg, GL_TEXTURE_MAG_FILTER, pool ? GL_LINEAR : GL_NEAREST );
			glTexParameteri( tg, GL_TEXTURE_WRAP_S, 0x812F );
			glTexParameteri( tg, GL_TEXTURE_WRAP_T, 0x812F );
			glTexParameteri( tg, fMaxLevel, s.levels - 1 );
			if ( pool )
				glTexParameterf( tg, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoPix );	// manager ruling 11-16 (fgrt_consts: the reason)
		}
		glBindTexture( tg, 0 );
	}
	if ( glGetError() != GL_NO_ERROR )
		return fatal( "the shared cell table / BC1 page pools" );
	std::printf( "FARGNDRTDRAW2 shared %d inputs on the present device: fgCells (sharedTarget), fgPagesN, fgPagesF (far twins)\n", kSh );
	auto toPlain = [&]() {	// the shared contents into the plain twins, every level
		for ( int k = 0; k < kSh; k++ ) {
			const Smp & s = smp[size_t( iShared[k] )];
			const GLenum tg = targetOf( s.kind );
			for ( int l = 0; l < s.levels; l++ )
				copyImg( s.tex, tg, l, 0, 0, 0, plain[k], tg, l, 0, 0, 0, std::max( s.w >> l, 1 ), std::max( s.h >> l, 1 ), s.kind == 2 ? s.d : 1 );
		}
	};
	auto swapPlain = [&]() {
		for ( int k = 0; k < kSh; k++ )
			std::swap( smp[size_t( iShared[k] )].tex, plain[k] );
	};
	WwVkPresent::FxStat tot;
	auto take = [&]() {	// one run's stat into the total
		const WwVkPresent::FxStat st = WwVkPresent::takeFarGroundLiveDrawStat();
		tot.runs += st.runs, tot.fails += st.fails, tot.cpuMs += st.cpuMs, tot.maxMs = std::max( tot.maxMs, st.maxMs );
		tot.pipelines = st.pipelines, tot.buildMs = st.buildMs, tot.copies += st.copies;	// the build counts are running totals
		return st;
	};
	take();
	// mode 1's three filled far-ground samplers as fargrounddraw.cpp makes them (throwaways: only their rng draws matter)
	std::vector<Smp> mode1;
	for ( const auto & nt : { std::pair<const char *, GLenum>( "fgAtlas", 0x8B5E ), { "fgDecal", 0x8DC2 }, { "fgLtex", 0x8DC1 } } ) {
		Smp m;
		m.name = nt.first;
		m.glType = nt.second;
		if ( !smpOf( m, W, H ) )
			return fatal( std::string( "mode 1's throwaway " ) + nt.first );
		if ( m.name == "fgLtex" )
			m.fmt = 0x8058, m.w = m.h = 64, m.d = 4, m.levels = 7;
		else if ( m.name == "fgAtlas" )
			m.fmt = 0x8058, m.w = m.h = 64, m.levels = 7;
		mode1.push_back( m );
	}
	for ( int seed = 1; seed <= 2; seed++ ) {
		std::mt19937 rng( 20261114u + std::uint32_t( seed ) * 7919u
			+ std::uint32_t( getenv( "FLD_SEED" ) ? std::atoi( getenv( "FLD_SEED" ) ) : 0 ) );
		/* BATCH1118: fill in the LEGACY program's sampler order from this rng (mode 1's throwaways at fgCells' place), GL-only
		 * samplers after from a side rng: seedUnis and blocks then draw exactly the legacy run's values */
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
			std::printf( "FARGNDRTDRAW2 textures filled in the legacy program's order: %zu, from a side rng (GL's program only): %d\n",
				fillOrder.size() - size_t( sideFills ), sideFills );
		for ( const int fo : fillOrder ) {
			Smp & s = smp[size_t( fo >= 0 ? fo : -1 - fo )];
			const int at = int( &s - smp.data() );
			if ( at == iCells )	// mode 1's fgAtlas, fgDecal, (fgGrid skipped), fgLtex drew the rng here: the same draws
				for ( Smp & m : mode1 )
					if ( !fill( m, rng ) )
						return fatal( "mode 1's throwaway " + m.name );
			if ( at == iCells || at == iPagesN || at == iPagesF )	// made above, contents below
				continue;
			if ( !fill( s, fo >= 0 ? rng : rngSide ) )
				return fatal( "texture " + s.name );
		}
		std::mt19937 rngPg( 4242u + std::uint32_t( seed ) );	// a side rng: GL's sampler order seeds nothing here
		std::uniform_real_distribution<float> uPh( 0.0f, 1.0f );
		const float phN = uPh( rngPg ), phF = uPh( rngPg );
		const std::vector<std::uint16_t> cells = cellsData( false ), cellsRed = cellsData( true ), cellsPg = cellsPaged(),
			cellsFull = cellsUnfaded();
		const std::vector<std::vector<std::uint8_t>> pagesN = pagesData( kSideN, phN ), pagesNRed = pagesData( kSideN, phN + 0.37f ),
			pagesF = pagesData( kSideF, phF ), pagesFRed = pagesData( kSideF, phF + 0.37f, -0.3f );
		/* red 512's far pool also moves its mean: the far cells read the pool's coarse levels, where a phase shift alone
		 * averages away (vkground4: with the air fixture fixed it moved 21 samples past 1e-2; its earlier FAIL was the
		 * fixture's fog mismatch) */
		putCells( cells );
		putPages( iPagesN, pagesN );
		putPages( iPagesF, pagesF );
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
		const int nu = seedUnis( pLegacy, rng, pFg );	// the legacy program's order and types
		blocks( rng );
		const bool scissor = seed == 2;
		f.useProgram( pFg );
		setI( "farInst", 0 );
		setI( "fgMode", 3 );	// cellfarground.cpp uniforms(), ready 3
		set4( "fgGridMap", kOx, kOy, 1.0f / kSpacing, kTiling );
		{	// view space onto the grid's world square: world = (x + 5000, -z + 7000, y + 300)
			const GLint l = f.getUniformLocation( pFg, "cellRow" );
			const float rows[12] = { 1.0f, 0.0f, 0.0f, 5000.0f, 0.0f, 0.0f, -1.0f, 7000.0f, 0.0f, 1.0f, 0.0f, 300.0f };
			if ( l >= 0 )
				f.uniform4fv( l, 3, rows );
			else
				std::printf( "FARGNDRTDRAW2 note: cellRow has no location\n" );
		}
		{
			const GLint l = f.getUniformLocation( pFg, "fogDistScale" );
			const float v = 1.0f / 1500.0f;
			if ( l >= 0 )
				f.uniform1fv( l, 1, &v );
		}
		/* the cell grade neutral (saturation, brightness, contrast 1, no tint), as the game's default imagespace: seeded in
		 * [0.05, 1) its contrast and saturation flattened the ground to 0.48..0.68 and the far pool (red 512) moved under
		 * 1e-2 even with its mean moved by 0.3 */
		{
			const GLint l = f.getUniformLocation( pFg, "cellIsCine" );
			const float v[3] = { 1.0f, 1.0f, 1.0f };
			if ( l >= 0 )
				f.uniform3fv( l, 1, v );
		}
		set4( "cellIsTint", 0.0f, 1.0f, 1.0f, 1.0f );
		{	/* volPackDims.xyz = the packed air volume's size in texels, as cellvolair.cpp sets it (tw, th, texD). Seeded in
			 * [0.05, 1) it made wwGridAt's clamp( uv * share, 0.5 / dims, share - 0.5 / dims ) run with lo > hi, which GLSL
			 * and SPIR-V both leave undefined and NV's two compilers resolve differently: seed 1 (far grid on) parted 7966
			 * samples, max rel 0.057. The near-grid slice count (.w) keeps its seeded value. */
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
				std::printf( "FARGNDRTDRAW2 seed %d volPackDims %g x %g x %g (volTex), volPack shares %s the half-texel clamp\n", seed,
					double( pd[0] ), double( pd[1] ), double( pd[2] ), ok ? "inside" : "OUTSIDE" );
				if ( !ok && lp >= 0 ) {	// lane VKGROUND17: seeding, never the bar (the DG key's rng stream seeded a share under one texel)
					for ( int c = 0; c < 4; c++ ) {
						const float lo = 1.0f / pd[c % 2];
						pk[c] = pk[c] < lo ? lo : pk[c];
					}
					f.uniform4fv( lp, 1, pk );
					std::printf( "FARGNDRTDRAW2 seed %d volPack raised to one texel of the air volume: %g %g %g %g (seeding)\n", seed,
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
					std::printf( "FARGNDRTDRAW2 %s seed 1: %s has no GL location (its block bytes stay zero)\n", tag, z.n );
					continue;
				}
				float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				f.getUniformfv( pFg, l, v );
				v[z.c] = 0.0f;
				( z.comps == 2 ? f.uniform2fv : z.comps == 3 ? f.uniform3fv : f.uniform4fv )( l, 1, v );
				zeroed++;
			}
			std::printf( "FARGNDRTDRAW2 %s seed 1: %d of 4 loop-bound neighbours zeroed (cellCenter.y, cellRoomsDims.y, cellCtRect.y, "
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
			std::printf( "FARGNDRTDRAW2 %s seed 1: cellIsCine contrast 1 (location %d; saturation %g, z %g kept)\n", tag, l, double( c[0] ),
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
		std::printf( "FARGNDRTDRAW2 seed %d: %d loose uniforms seeded%s, fgMode 3, cached pages\n", seed, nu, scissor ? ", a scissor" : "" );

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
		std::printf( "FARGNDRTDRAW2 %s seed %d: cellRestir ratio over %d of %d pixels (the ground's depth), the rest 1e6; linear depth %.0f..%.0f\n", tag, seed,
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
			std::printf( "FARGNDRTDRAW2 seed %d GL shared vs plain inputs: %s (%zu colour halves, %zu depth samples differ)\n", seed,
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
			std::printf( "FARGNDRTDRAW2 seed %d GL colour over the ground: r %.4g..%.4g g %.4g..%.4g b %.4g..%.4g a %.4g..%.4g, %zu distinct\n", seed,
				lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3], distinct.size() );
			if ( distinct.size() < 1000 ) {
				std::printf( "FARGNDRTDRAW2 seed %d: under 1000 distinct colours over the ground: a near-constant layer is blind\n", seed );
				gateOk = false;
			}
		}
		std::vector<std::vector<char>> masks;	// diagnosis (print only): the samples each probe moved
		const char * maskNames[4] = { "near pool", "far pool", "no-page cell", "faded cell" };
		{	// GL probes: each part of the fixture is on screen and read (moved >= 1 sample each); judged on seed 1 only
			auto probe = [&]( const char * what, const std::function<void()> & on, const std::function<void()> & off ) {
				on();
				before( fbRef );
				drawState( fbRef, scissor );
				glDraw();
				stateEnd();
				off();
				const std::vector<std::uint16_t> pC = readColor( refCol );
				size_t moved = 0;
				std::vector<char> m( glC.size() / 4, 0 );
				for ( size_t s = 0; s * 4 + 3 < pC.size() && s * 4 + 3 < glC.size(); s++ ) {
					m[s] = pC[s * 4] != glC[s * 4] || pC[s * 4 + 1] != glC[s * 4 + 1] || pC[s * 4 + 2] != glC[s * 4 + 2];
					moved += size_t( m[s] );
				}
				masks.push_back( m );
				const bool ok = moved >= 1 && glGetError() == GL_NO_ERROR;
				std::printf( "FARGNDRTDRAW2 seed %d GL probe %s: %zu samples moved (%s)\n", seed, what, moved,
					seed != 1 ? "info" : ok ? "read" : "NOT READ: blind" );
				if ( seed == 1 )
					gateOk = gateOk && ok;
			};
			probe( "near pool rewritten", [&]() { putPages( iPagesN, pagesNRed ); }, [&]() { putPages( iPagesN, pagesN ); } );
			probe( "far pool rewritten", [&]() { putPages( iPagesF, pagesFRed ); }, [&]() { putPages( iPagesF, pagesF ); } );
			probe( "the no-page cell given a page", [&]() { putCells( cellsPg ); }, [&]() { putCells( cells ); } );
			probe( "the faded cell at full fade", [&]() { putCells( cellsFull ); }, [&]() { putCells( cells ); } );
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
			std::printf( "FARGNDRTDRAW2 %s seed %d GL probe %s 0: moved %zu of %zu covered samples (%.1f %%): %s\n", tag, seed, sw, moved,
				glCovered, glCovered ? 100.0 * double( moved ) / double( glCovered ) : 0.0, ok ? "OK" : "FAIL (the input does not reach the colour)" );
			if ( !ok )
				gateOk = false;
		}

		// every Vulkan call: twin 0 the base twin, 1 RT, 2 DG; without the RT twins (WW_VK_FARGNDPRT) each RT run is REFUSED "no twin"
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
				WwVkPresent::farGroundLiveStateOf( &gw, pFg, r );
				r.w = W, r.h = H, r.color = fc;
				r.cmds.assign( 1, c );
				r.key = reinterpret_cast<std::uintptr_t>( baseKey );
				ran = WwVkPresent::farGroundLiveRun( r, w );
				lastCopies = take().copies;
			} else {
#ifdef WW_VK_FARGNDPRT
				WwVkPresent::farGroundLiveRtStateOf( &gw, pFg, r, twin );
				r.w = W, r.h = H, r.color = fc;
				r.cmds.assign( 1, c );
				r.key = reinterpret_cast<std::uintptr_t>( twin == kv ? twinKey : redKey );
				r.red = red;
				if ( between )
					between( r );
				ran = WwVkPresent::farGroundLiveRtRun( r, twin, w );
				lastCopies = WwVkPresent::takeFarGroundLiveRtDrawStat( twin ).copies;
#else
				( void )red;
				( void )between;
				w = "no twin: the far ground RT / DG twins are not built (WW_VK_FARGNDPRT undefined)";
#endif
			}
			stateEnd();
			runs++;
			return ran;
		};

		// the Vulkan runs: red 0, then (seed 1) plain inputs and the reds
		enum { kMain, kPlain, kRed1, kRedRestir, kRedAtlas, kRedKey };
		const char * caseName[6] = { "red 0", "plain inputs (red 0 on plain fgCells / fgPagesN / fgPagesF)", "red 1 (ps block one vec4 off)",
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
				std::printf( "FARGNDRTDRAW2 %s seed %d %s: REFUSED (%s)%s\n", tag, seed, caseName[cs], w.c_str(), okRefuse ? " -> red refused, as it may" : "" );
				if ( !okRefuse )
					gateOk = false;
				continue;
			}
			const Verdict v = judge( bgC, bgD, readColor( fc ), readDepth( fds, true ), glC, glD );
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			const bool red = cs >= kRed1;
			std::printf( "FARGNDRTDRAW2 %s seed %d %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
				"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g), %d copies%s\n", tag, seed, caseName[cs], word, v.covered, v.both,
				v.onlyOne, v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, lastCopies,
				red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
			if ( red ? !fail : fail )
				gateOk = false;
		}
		if ( seed == 1 ) {	// fgCells, fgPagesN, fgPagesF read in place: exactly three copies fewer a run than on the plain twins
			const bool fewer = copiesShared >= 0 && copiesPlain - copiesShared == 3;
			std::printf( "FARGNDRTDRAW2 %s seed 1 copies a run: %d on the shared inputs, %d on plain ones (%s)\n", tag, copiesShared, copiesPlain,
				fewer ? "fgCells, fgPagesN, fgPagesF read in place" : "NOT three fewer" );
			gateOk = gateOk && fewer;
		}

		// lane VKGROUND16, the state arm (seed 1; manager ruling 11-16, fgrt_consts): fgPagesN at the app's anisotropy 8, every census
		// input's sampling state side by side: GL's own texture state on the input's unit vs the twin's VkSampler / view, derived from
		// the census slot farGroundLiveRtStateOf fills (the app's path) by waterSampler's rules (vkwater.inc) and the device's anisotropy.
		// Filter, wrap, compare, levels, LOD and anisotropy must match exactly; red: the twin's fgPagesN anisotropy changed must FAIL.
		if ( seed == 1 ) {
#ifdef WW_VK_FARGNDPRT
			glBindTexture( 0x8C1A, smp[size_t( iPagesN )].tex );
			glTexParameterf( 0x8C1A, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoApp );
			glBindTexture( 0x8C1A, 0 );
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun sr;
			WwVkPresent::farGroundLiveRtStateOf( &gw, pFg, sr, kv );
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
							std::printf( "FARGNDRTDRAW2 %s state %-16s kind %d unit %d tex %u: no sampler state to check (buffer, other or unbound)\n", tag,
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
					if ( s.name == "fgPagesN" )
						ltexGl = gAn, ltexVk = vAn;
					if ( print || !same )
						std::printf( "FARGNDRTDRAW2 %s state %-16s GL tex %u %s\nFARGNDRTDRAW2 %s state %-16s VK tex %u %s : %s\n", tag, s.name.c_str(),
							unsigned( name ), gl, tag, s.name.c_str(), s.tex, vv, same ? "same" : "DIFFERS" );
				}
				gw.glActiveTexture( GLenum( act ) );
				if ( print )
					std::printf( "FARGNDRTDRAW2 %s state: %d inputs checked, %d without sampler state, %d anisotropic, %d differ\n", tag, seen, skipped,
						nAniso, diff );
				return seen > 0 ? diff : -1;
			};
			int nAn = 0;
			float lg = 0.0f, lv = 0.0f;
			const int sDiff = stateTable( sr, true, nAn, lg, lv );
			const bool sOk = sDiff == 0 && lg == kLtexAnisoApp && lv == kLtexAnisoApp && nAn >= 1;
			std::printf( "FARGNDRTDRAW2 %s seed 1 state arm (fgPagesN anisotropy GL %g / twin %g, the app's %g): %s\n", tag, double( lg ), double( lv ),
				double( kLtexAnisoApp ), sOk ? "OK" : "FAIL" );
			gateOk = gateOk && sOk;
			{	// red: the twin's fgPagesN anisotropy other than GL's (the slot the twin's VkSampler is made from)
				WwVkPresent::WaterRun bad( sr );
				int hit = 0;
				for ( WwVkPresent::WaterSlot & s : bad.census.inputs )
					if ( s.name == "fgPagesN" )
						s.aniso = 4.0f, hit++;
				const int rDiff = hit == 1 ? stateTable( bad, false, nAn, lg, lv ) : -1;
				const bool rFails = rDiff > 0;
				std::printf( "FARGNDRTDRAW2 %s seed 1 state red aniso (twin's fgPagesN 4, GL 8): %d differ%s\n", tag, rDiff,
					rFails ? " -> red FAILS as it must" : " -> red PASSES: the state arm is blind" );
				gateOk = gateOk && rFails;
			}
			stateEnd();
			glBindTexture( 0x8C1A, smp[size_t( iPagesN )].tex );
			glTexParameterf( 0x8C1A, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, kLtexAnisoPix );
			glBindTexture( 0x8C1A, 0 );
#else
			std::printf( "FARGNDRTDRAW2 %s seed 1 state arm: FAIL (no twin: WW_VK_FARGNDPRT undefined)\n", tag );
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
			std::printf( "FARGNDRTDRAW2 %s all-on key: %zu texels on the ground, %zu decode to a unit normal (want >= 1000, >= 90 %%): %s\n", tag,
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
				std::printf( "FARGNDRTDRAW2 %s all-on GL probe %s: moved %zu of %zu covered samples (%.1f %%): %s\n", tag,
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
					std::printf( "FARGNDRTDRAW2 %s all-on %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
						"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g)", tag, name, word.c_str(), v.covered, v.both, v.onlyOne,
						v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel );
				} else
					std::printf( "FARGNDRTDRAW2 %s all-on %s: REFUSED (%s)", tag, name, w.c_str() );
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
			std::printf( "FARGNDRTDRAW2 %s all-on: clean %d of %d BOUND, reds failing %d of 5\n", tag, cleanBound, cleanRuns, redsFailing );
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
				std::printf( "FARGNDRTDRAW2 %s half rate (%dx%d): key %zu texels on the ground, %zu decode to a unit normal (want >= 250, >= 90 %%): %s\n",
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
					std::printf( "FARGNDRTDRAW2 %s half rate GL probe %s: moved %zu of %zu covered samples (%.1f %%): %s\n", tag, hProbe[pr], hm[pr],
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
#ifdef WW_VK_FARGNDPRT
					const WwVkPresent::FxField * fs = kv == 2 ? WwVkPresent::kT_fargndpDgPs : WwVkPresent::kT_fargndpRtPs;
					const size_t nf = kv == 2 ? sizeof( WwVkPresent::kT_fargndpDgPs ) / sizeof( WwVkPresent::kT_fargndpDgPs[0] )
						: sizeof( WwVkPresent::kT_fargndpRtPs ) / sizeof( WwVkPresent::kT_fargndpRtPs[0] );
					for ( size_t i = 0; i < nf; i++ )
						if ( std::strcmp( fs[i].name, "cellGiHalfScale" ) == 0 && fs[i].offset + 4 <= r.psBytes )
							std::memcpy( r.psLoose + fs[i].offset, &one, 4 );
#else
					( void )r;
#endif
				}, true );
				hRun( "half rate clean after red", nullptr, false );
				std::printf( "FARGNDRTDRAW2 %s half rate: clean %d of 2 BOUND, reds failing %d of 1\n", tag, hClean, hRed );
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
	std::printf( "FARGNDRTDRAW2 %s %d runs (the base twin: %d on Vulkan, %d fails)\n", tag, runs, tot.runs, tot.fails );
	std::printf( gateOk ? "FARGNDRTDRAW2 GATE OK\n" : "FARGNDRTDRAW2 GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
