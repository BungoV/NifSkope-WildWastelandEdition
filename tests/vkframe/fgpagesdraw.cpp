// Lane VKGROUND3 (2026-11-16): far ground mode 2, Show > Far ground: Live, cached pages (vkfarground.inc farGroundLiveRun:
// the fargnd_view_vs / fargndp_view_ps twins of fo4_default.vert + far-ground/fo4_cellcsmfgp.frag, one non-instanced command
// with firstIndex / baseVertex as CellFarGround::vkDraw passes them) against GL's own program (fgpagesdraw_prep.py), per
// sample, into the same 4x MSAA RGBA16F + D24S8 frame contents. A copy of fargrounddraw.cpp (VKFAR3's mode-1 gate: the
// scene, blocks, judge and bar unchanged) with mode 2's inputs. The pixel twins are the gate-only WW_GLSL_BASE_TERMS build
// (fgpagesdraw.sh, ruling 11-16: FO4MATCH1's terms are HLSL-only, GL here is the legacy .frag).
// Mode 2's data as cellfarground.cpp makes it: fgCells RGBA16UI 3 x 3 (r pool 0 none / 1 near / 2 far, g layer, b fade),
// fgPagesN / fgPagesF BC1 (0x83F0) 2D arrays (256 / 128 px, 3 layers, mips to 4 px, LINEAR_MIPMAP_LINEAR, CLAMP, anisotropy
// 8) of one smooth field a layer, its 2x2-box mip chain, each level BC1-encoded (never random blocks). fgGridMap puts the
// cell corner (5000, 7400) inside the fixture's world square: on screen a near cell, a far cell, a cell with no page (the
// sheet) and a near cell faded 140 / 255 over the sheet. fgCells is a shared target and the pools far twins, as on the view
// (lane VKGROUND5; GL on them vs GL on plain twins must be EXACT; the plain-twin run copies exactly three more images).
// GL probes (each must move >= 1 sample, seed 1): the near pool rewritten, the far pool rewritten, the no-page cell given a
// page, the faded cell at full fade (each part of the fixture is on screen and read).
// One seeded world with fargrounddraw (two programs, one world): where mode 1's sampler list had fgAtlas, fgDecal, fgGrid,
// fgLtex, mode 2's has fgCells, fgPagesF, fgPagesN; throwaway fills of mode 1's three filled specs draw the same rng there,
// so every later sampler and uniform equals the mode-1 gate's seed (fgp1 without it: seed 1 a flat grade, 137 distinct).
// Bar (farloddraw's, never widened): covered >= 1000; one side only <= 0.05 %; depth within 1 D24 unit; colour rel <= 1e-2 on
// >= 99.9 % of the samples covered on both sides; GL >= 1000 distinct colours over the ground. The draw is opaque (no blend:
// no 4x blend-merge reference needed).
// Reds (each must FAIL, never refuse; seed 1): 1 the ps loose block a vec4 off, 2 plain samplers, 4 cull flipped, 8 depth
// ALWAYS, 16 polygon offset dropped (vkwater.inc); gate-side: 64 the element offset passed in bytes, 128 fgCells' near and
// far cells swapped after GL's draw, 256 the near pool rewritten after GL's draw, 512 the far pool rewritten after GL's draw.
// The app's key WW_VKVIEW_FARGROUNDLIVE_RED lives in CellFarGround::vkDraw: no gate reaches it.
// Last line `FGPAGESDRAW GATE OK` or `FGPAGESDRAW GATE FAILED`.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN
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
	gTag = "FGPAGESDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: fargrounddraw <prep dir>\nFGPAGESDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FGPAGESDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfgpagesdraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfgpagesdraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfgpagesdraw", "far ground draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "FGPAGESDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	const GLuint pFg = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.frag.glsl" ), "fo4_cellcsmfgp" );
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

	// ---- the program's samplers: one unit each; the far ground's four as cellfarground.cpp makes them
	std::vector<Smp> smp;
	int iCells = -1, iPagesN = -1, iPagesF = -1;
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
			s.glType = ty;
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
			}
			s.unit = at + 1;
			smp.push_back( s );
		}
	}
	if ( iCells < 0 || iPagesN < 0 || iPagesF < 0 )
		return fatal( "the program has no fgCells / fgPagesN / fgPagesF (not the cached-pages far ground)" );
	{
		std::string miss;
		int nMiss = 0;
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_fargndpPs )
			if ( f.getUniformLocation( pFg, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + fd.name;
			}
		for ( const WwVkPresent::FxField & fd : WwVkPresent::kT_waterVs )
			if ( f.getUniformLocation( pFg, fd.name ) < 0 ) {
				nMiss++;
				miss += std::string( " vs:" ) + fd.name;
			}
		std::printf( "FGPAGESDRAW %d twin fields GL has no location for:%s\n", nMiss, miss.c_str() );
	}
	std::printf( "FGPAGESDRAW %zu samplers active in GL's program:", smp.size() );
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
	std::printf( "FGPAGESDRAW blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes\n", blockSize[0], blockSize[1], blockSize[2] );
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
	std::printf( "FGPAGESDRAW job: %dx%d x%d, %zu vertices (baseVertex %d), %u elements from %u, cells %d x %d, pages %d / %d px x %d, "
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
				glTexParameterf( tg, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
		}
		glBindTexture( tg, 0 );
	}
	if ( glGetError() != GL_NO_ERROR )
		return fatal( "the shared cell table / BC1 page pools" );
	std::printf( "FGPAGESDRAW shared %d inputs on the present device: fgCells (sharedTarget), fgPagesN, fgPagesF (far twins)\n", kSh );
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
		for ( Smp & s : smp ) {
			const int at = int( &s - smp.data() );
			if ( at == iCells )	// mode 1's fgAtlas, fgDecal, (fgGrid skipped), fgLtex drew the rng here: the same draws
				for ( Smp & m : mode1 )
					if ( !fill( m, rng ) )
						return fatal( "mode 1's throwaway " + m.name );
			if ( at == iCells || at == iPagesN || at == iPagesF )	// made above, contents below
				continue;
			if ( !fill( s, rng ) )
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
		g_profileSeed = seed;
		const int nu = seedUnis( pFg, rng );
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
				std::printf( "FGPAGESDRAW note: cellRow has no location\n" );
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
				std::printf( "FGPAGESDRAW seed %d volPackDims %g x %g x %g (volTex), volPack shares %s the half-texel clamp\n", seed,
					double( pd[0] ), double( pd[1] ), double( pd[2] ), ok ? "inside" : "OUTSIDE" );
				if ( !ok )
					return fatal( "volPack under one texel of the air volume (wwGridAt's clamp undefined)" );
			} else if ( l >= 0 )
				return fatal( "volPackDims read but no volTex sampler" );
		}
		std::printf( "FGPAGESDRAW seed %d: %d loose uniforms seeded%s, fgMode 3, cached pages\n", seed, nu, scissor ? ", a scissor" : "" );

		auto glDraw = [&]() {
			ex.drawInst( GL_TRIANGLES, GLsizei( count ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( idxOff ) ), 1, base, 0 );
		};
		before( fbRef );
		const std::vector<std::uint16_t> bgC = readColor( refCol );
		const std::vector<float> bgD = readDepth( refDs, false );
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
			std::printf( "FGPAGESDRAW seed %d GL shared vs plain inputs: %s (%zu colour halves, %zu depth samples differ)\n", seed,
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
			std::printf( "FGPAGESDRAW seed %d GL colour over the ground: r %.4g..%.4g g %.4g..%.4g b %.4g..%.4g a %.4g..%.4g, %zu distinct\n", seed,
				lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3], distinct.size() );
			if ( distinct.size() < 1000 ) {
				std::printf( "FGPAGESDRAW seed %d: under 1000 distinct colours over the ground: a near-constant layer is blind\n", seed );
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
				std::printf( "FGPAGESDRAW seed %d GL probe %s: %zu samples moved (%s)\n", seed, what, moved,
					seed != 1 ? "info" : ok ? "read" : "NOT READ: blind" );
				if ( seed == 1 )
					gateOk = gateOk && ok;
			};
			probe( "near pool rewritten", [&]() { putPages( iPagesN, pagesNRed ); }, [&]() { putPages( iPagesN, pagesN ); } );
			probe( "far pool rewritten", [&]() { putPages( iPagesF, pagesFRed ); }, [&]() { putPages( iPagesF, pagesF ); } );
			probe( "the no-page cell given a page", [&]() { putCells( cellsPg ); }, [&]() { putCells( cells ); } );
			probe( "the faded cell at full fade", [&]() { putCells( cellsFull ); }, [&]() { putCells( cells ); } );
		}

		int copiesShared = -1, copiesPlain = -1;
		for ( int pick : { 0, -1, 1, 2, 4, 8, 16, 64, 128, 256, 512 } ) {
			const bool viaPlain = pick < 0;	// seed 1: the same draw on the plain twins (copied a run): the copy count's other side
			const int red = viaPlain ? 0 : pick;
			if ( ( red || viaPlain ) && seed != 1 )
				continue;
			if ( viaPlain )
				swapPlain();
			if ( red == 128 )
				putCells( cellsRed );
			if ( red == 256 )
				putPages( iPagesN, pagesNRed );
			if ( red == 512 )
				putPages( iPagesF, pagesFRed );
			before( fbFrame );
			drawState( fbFrame, scissor );
			WwVkPresent::WaterRun r;
			WwVkPresent::farGroundLiveStateOf( &gw, pFg, r );
			r.w = W;
			r.h = H;
			r.color = fc;
			WwVkPresent::WaterCmd c;	// CellFarGround::vkDraw's command
			c.count = count;
			c.instanceCount = 1;
			c.firstIndex = std::uint32_t( red == 64 ? idxOff : idxOff / 2 );
			c.baseVertex = base;
			c.baseInstance = 0;
			r.cmds.assign( 1, c );
			r.key = reinterpret_cast<std::uintptr_t>( twinKeys );
			r.red = red & 31;
			const bool ran = WwVkPresent::farGroundLiveRun( r, why );
			stateEnd();
			if ( viaPlain )
				swapPlain();
			if ( red == 128 )
				putCells( cells );
			if ( red == 256 )
				putPages( iPagesN, pagesN );
			if ( red == 512 )
				putPages( iPagesF, pagesF );
			runs++;
			const int copies = take().copies;
			if ( !red )
				( viaPlain ? copiesPlain : copiesShared ) = copies;
			if ( !ran ) {
				std::printf( "FGPAGESDRAW seed %d %s: REFUSED (%s)\n", seed, viaPlain ? "plain inputs" : ( "red " + std::to_string( red ) ).c_str(),
					why.c_str() );
				gateOk = false;
				continue;
			}
			const std::vector<std::uint16_t> vkC = readColor( fc );
			const std::vector<float> vkD = readDepth( fds, true );
			const Verdict v = judge( bgC, bgD, vkC, vkD, glC, glD );
			const char * word = v.word();
			const bool fail = std::strcmp( word, "FAIL" ) == 0;
			std::printf( "FGPAGESDRAW seed %d %s %d: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
				"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g), %d copies%s\n", seed, viaPlain ? "plain inputs, red" : "red", red,
				word, v.covered, v.both, v.onlyOne, v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel, copies,
				red ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
			if ( red ? !fail : fail )
				gateOk = false;
			if ( !red && !viaPlain && masks.size() == 4 ) {	// diagnosis (print only): where red 0's parted samples sit
				size_t in[4] = {}, past = 0, rest = 0, cov = 0, covIn[4] = {};
				for ( size_t s = 0; s < glD.size() && s < masks[0].size(); s++ ) {
					bool p = false, cv = glD[s] != bgD[s];
					for ( int c = 0; c < 3; c++ ) {
						const float fa = h2f( vkC[s * 4 + size_t( c )] ), fb = h2f( glC[s * 4 + size_t( c )] );
						p = p || std::fabs( double( fa ) - double( fb ) ) / std::max( 1.0, std::fabs( double( fb ) ) ) > 1e-2;
					}
					bool any = false;
					for ( int k = 0; k < 4; k++ ) {
						in[k] += size_t( p && masks[size_t( k )][s] );
						covIn[k] += size_t( cv && masks[size_t( k )][s] );
						any = any || masks[size_t( k )][s];
					}
					past += p;
					rest += p && !any;
					cov += cv;
				}
				std::printf( "FGPAGESDRAW seed %d red 0 parted (past 1e-2) samples by probe mask (info): %zu in all;", seed, past );
				for ( int k = 0; k < 4; k++ )
					std::printf( " %s %zu (of %zu covered)", maskNames[k], in[k], covIn[k] );
				std::printf( "; in no mask %zu (GL covered %zu)\n", rest, cov );
			}
		}
		if ( seed == 1 ) {	// the shared inputs never copied: exactly kSh copies fewer a run than on the plain twins
			const bool fewer = copiesShared >= 0 && copiesPlain - copiesShared == kSh;
			std::printf( "FGPAGESDRAW seed 1 copies a run: %d on the shared inputs, %d on plain ones, %d fewer of %d shared (%s)\n",
				copiesShared, copiesPlain, copiesPlain - copiesShared, kSh,
				fewer ? "fgCells, fgPagesN, fgPagesF read in place" : "NOT the shared count fewer" );
			gateOk = gateOk && fewer;
		}
	}
	std::printf( "FGPAGESDRAW %d runs: %d on Vulkan, %.2f ms CPU each (max %.2f; the harness's hand-off wait included), %d fails, "
		"%d pipelines built in %.1f ms (%.2f ms CPU a run without them)\n", runs, tot.runs, tot.runs ? tot.cpuMs / tot.runs : 0.0, tot.maxMs,
		tot.fails, tot.pipelines, tot.buildMs, tot.runs ? ( tot.cpuMs - tot.buildMs ) / tot.runs : 0.0 );
	std::printf( gateOk ? "FGPAGESDRAW GATE OK\n" : "FGPAGESDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
