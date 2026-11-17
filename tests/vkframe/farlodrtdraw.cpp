// Lane VKFAR28 (2026-11-16, plan notes/vkfar26/next.md item 2 as changed by notes/vkfar27/next.md item 1): the far LOD rings
// under the RT GI keys on the present device (vkfarlodrt.inc farLodRtRun: fargnd_view_vs + farlod_view_rt_ps, twin 1 the
// all-on key RESTIR + RTDDGI + RTDDGI_CASCADES; farlod_view_dg_ps, twin 2 the DG key RTDDGI + RTDDGI_CASCADES) against GL's own
// fo4_cellcsm under the same keys (farlodrtdraw_prep.py), per sample, colour + depth, into the same 4x MSAA RGBA16F + D24S8
// frame contents. farloddraw.cpp's helpers, meshes, instances, state and judge are reused (FLD_NO_MAIN); GL's side is read by
// the shipped farLodRtStateOf.
// The RT inputs are the app's shared texel buffers (vkrtbuf.h): cellRestir (RG32F, W x H texels: ratio, linear depth) and
// cellDdgiAtlas (RGBA32F, 193 texels a probe) are made with rtBufShare and GL binds THOSE GL names, so the twin reads them in
// place through rtBufOf (no per-run copy). Each arm reads the shared bytes back through farTexelBufferRead( rb.key ) and
// requires them equal to what the gate wrote (bytes compared > 0) and rb.glBuf the bound buffer.
// Seeding (so both inputs reach the colour on both sides): cellRow = identity rotation, zero translation (world = view, the
// ReSTIR depth D = -z_view); one GL draw first gives each pixel's sample-0 depth, D = proj[14] / ( 2d - 1 + proj[10] ), and
// cellRestir holds (seeded ratio 0.05..1, D) where the rings cover the pixel, (ratio, 1e6) elsewhere; cellRestirSize (W, H, 0,
// 0). Light 0 is a shadow-casting omni (t4 = (-1, 1): no cube slot) at (0, 150, -700), radius 5000, cellLightCount 1,
// cellClusterOn off. One DDGI cascade (origin (-1200, -900, -2100), step 300, 9 x 7 x 8 probes) covers the rings, seeded
// irradiance, distance pairs far (2000 / 4e6: Chebyshev 1), probe states alternating 2 / 1; cellDdgiCount 1, cellGiLoop8 8,
// cellGiSky 1, cellExtDalc 1, cellGiAmb 1. cellGiHalfOn 0 in every judged arm but the all-on arm below.
// Switch profiles: farloddraw's seeds 1 and 2 without cellClusterOn, plus cellRestirOn, cellDdgiOn, cellGiSky, cellLightCount,
// cellExtDalc. Arms: key RT and key DG, each seed 1 and seed 2.
// Bar (set before the first run, never widened; farloddraw's colour bar, unchanged):
//   covered >= 1000, coverage mismatch <= 0.05 % of covered, depth within 1 D24 unit, colour |vk - gl| <= 1e-2 * max(1, |gl|)
//   per half on >= 99.9 % of the samples covered on both sides (red 0 of every arm); >= 1000 distinct GL colours over the rings;
//   the shared bytes read back equal (both slots, every arm); GL probes: cellRestirOn 0 (RT key) and cellDdgiOn 0 (both keys)
//   each move >= 20 % of GL's covered samples (an input that never reaches the colour makes its red blind).
// Reds (seed 1 of each key; each must FAIL by the same judge): red 1 the ps loose block one vec4 off (twinRun); red restir
// (RT key): after GL's draw the shared cellRestir is rewritten (ratio -> 1.05 - ratio, depth kept) by glBufferSubData into the
// shared GL buffer, Vulkan runs, then the bytes are put back (a twin reading a stale copy, or none, passes); red atlas: the
// shared cellDdgiAtlas shifted one texel the same way; red key: the DG twin on the RT program, the base twin (farLodRun) on
// the DG program (FAIL or REFUSE both count).
// Refusal check (seed 1 of the DG key, counted): cellGiHalfOn 1 must REFUSE with the "cellGiHalfOn 1" reason (the DG twin
// reads the screen GI too, but its route is not gated yet). Lane VKFAR30: on the RT key (twin 1, the all-on key) this refusal became a route, so its
// check is replaced by the all-on arm. Lane VKFAR31: the DG key's check is replaced the same way (the arm runs on both keys).
// All-on arm (seed 1 of the RT key): cellGiHalf / cellGiHalfKey are VKBUF2's shared images (vkrtbuf.h rtImgShare at W x H,
// step 1: cellGiHalfScale 1, cellGiHalfRect (0, 0, W, H); owners registered, always fresh) and GL binds THOSE names;
// cellGiHalfOn 1, cellPass 0, cellProbe 0. The key is the rings' own surface: a GL normals pass (cellProbe 1: under RTDDGI without
// WW_CELL_VIEWS the program writes its own shading normal cellWorldDir( normal ) * 0.5 + 0.5; wwDebugView is compiled out)
// and its depth give each pixel's sample-0 position (view = world, P from the depth at the pixel centre)
// and normal, packed as cellGiKeyNormal reads it (octahedral, 2048 x 2048); w = -1 where the rings miss. cellGiHalf holds
// seeded values from a side rng (every other arm's seeding unchanged). Both are copied into the shared images from plain
// textures (glCopyImageSubData).
// All-on bar: the clean run BOUND (the same judge and colour bar as red 0); >= 1000 key texels written and >= 90 % of them
// decode to a unit normal; GL probes cellGiHalfOn 0 and cellGiHalf reseeded each move >= 20 % of GL's covered samples.
// All-on reds (each must FAIL: judge FAIL or REFUSED; a clean run after each must be BOUND again): not shared (a plain GL
// texture with the same bytes bound on cellGiHalf: must REFUSE, naming "cellGiHalf:"), wrong image (the twin side binds
// cellGiHalfKey's shared image on cellGiHalf and cellGiHalf's on cellGiHalfKey), stale gen (the WaterRun taken, then the
// owner drops and remakes cellGiHalf: rtImgGen must move, the new image is cleared to a sentinel; the run must not be BOUND).
// Lane VKFAR31 (bar and reds set before the run that lifts the DG twin; the earlier bar unchanged):
// - The all-on arm runs on seed 1 of the DG key too (twin 2: the same seeding, key, probes, bar and reds; lines "DG all-on").
// - Two more all-on reds, each REFUSED with its own reason text: key mismatch (a borrowed shared entry, of a key that sorts
//   before the owner's, on cellGiHalf's GL name, so farSharedOf answers that key: "the shared image is not the bound name's")
//   and no sampler (cellGiHalf taken out of the run's census after GL's state is read: "GL's program has no such sampler").
//   A clean run after each must be BOUND. Reds failing: 5 of 5 per key.
// - Half-rate arm (after the reds, each key): both images shared again at rtImgSize( W, 2 ) x rtImgSize( H, 2 ),
//   cellGiHalfRect (0, 0, hw, hh), cellGiHalfScale 0.5, as ao.cpp sets them; key texel h is the pixel 2h of the same normals
//   pass (cell_gihalf_ps's writer); cellGiHalf from its own side rng. Its own GL reference. Bar: >= 250 key texels written and
//   >= 90 % of them decode to a unit normal; GL probes cellGiHalfOn 0, cellGiHalf reseeded and cellGiHalfScale 1 each move
//   >= 20 % of GL's covered samples; the clean run BOUND; red half rate ignored (the twin's cellGiHalfScale set to 1 in the
//   run's pixel block) must FAIL; a clean run after it BOUND. Then both images are shared again at W x H.
// Last line `FARLODRTDRAW GATE OK` or `FARLODRTDRAW GATE FAILED`.
// Seeding fix after run frt1 (bar and reds unchanged): the run refused every Vulkan arm with "no owner" on cellDdgiAtlas
// (rtBufOf answers only for a slot with an owner: the gate now registers one per slot, always fresh, as the app's owners
// do) and with "an image at b65 no census name maps" (a twin fix, its own commit); DG seed 1 drew 500 distinct colours
// (flat: the wrong-key red came out BOUND), so both keys now seed from the same rng seed per arm, not one per key.
// Lane VKFAR29 (bar and reds unchanged): (1) red 0 on seed 1 compared the legacy fo4_cellcsm.frag against twins carrying
// FO4MATCH1's HLSL-only terms (frt2: 9600 / 23708 samples past 1e-2); per the 11-16 ruling (HLSL is the reference) the .sh
// builds both RT twins again with the gate-only WW_GLSL_BASE_TERMS into <out>/inc, as fargrounddraw.sh does. (2) Seeding:
// red 1 reads every ps loose member from the vec4 before it, so the int loop bounds cellAmboCount, cellGiFineN, cellGiLoop8 and
// cellDdgiCount read the float bits of cellCenter.y, cellRoomsDims.y, cellCtRect.y and cellClusterLog.x (~1e9 iterations a
// pixel): the GPU hung, Vulkan covered nothing and the next submit returned DEVICE_LOST (frt2 / frt3). Seed 1 (the reds'
// seed) sets those four components to 0; cellCtOn, cellClusterOn and cellGiRooms are off on seed 1.
#define FLD_NO_MAIN
#include "farloddraw.cpp"
#undef FLD_NO_MAIN

#include "features/vkframe/vkrtbuf.h"

namespace frt
{
using namespace fv;
using namespace fld;

const int kDdgiDims[4] = { 9, 7, 8, 9 * 7 * 8 };
const size_t kProbes = size_t( 9 * 7 * 8 ), kAtlasTexels = 193 * kProbes;

struct KeyArm
{
	const char * tag;
	const char * frag;
	int v;  // farLodRtRun's twin: 1 RT, 2 DG
	GLuint p = 0;
	std::vector<Smp> smp;
};

size_t differ( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	size_t n = a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
	for ( size_t i = 0; i < std::min( a.size(), b.size() ); i++ )
		n += a[i] != b[i] ? 1 : 0;
	return n;
}
}  // namespace frt

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fld;
	using namespace frt;
	gTag = "FARLODRTDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: farlodrtdraw <prep dir>\nFARLODRTDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARLODRTDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 256, H = 160, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfarlodrtdraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfarlodrtdraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfarlodrtdraw", "far LOD RT draw harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "FARLODRTDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pDepth = program( kTriVs, kDepthFs, "depth" ),
		pSlab = program( kTriVs, kSlabFs, "slab" );
	KeyArm keys[2] = { { "RT", "prog_rt.frag.glsl", 1 }, { "DG", "prog_dg.frag.glsl", 2 } };
	for ( KeyArm & k : keys )
		k.p = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/" + k.frag ), k.tag );
	if ( !pUnpack || !pDepth || !pSlab || !keys[0].p || !keys[1].p )
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

	// ---- the RT inputs: the app's shared texel buffers (vkrtbuf.h), GL binds these names
	const size_t rsBytes = size_t( W ) * size_t( H ) * 8, atBytes = kAtlasTexels * 16;
	unsigned rsBuf = 0, rsTex = 0, atBuf = 0, atTex = 0;
	if ( !WwVkPresent::rtBufShare( WwVkPresent::kRtBufRestir, 0x8230 /* GL_RG32F */, rsBytes, rsBuf, rsTex, why ) )
		return fatal( "rtBufShare cellRestir: " + why );
	if ( !WwVkPresent::rtBufShare( WwVkPresent::kRtBufDdgiAtlas, 0x8814 /* GL_RGBA32F */, atBytes, atBuf, atTex, why ) )
		return fatal( "rtBufShare cellDdgiAtlas: " + why );
	std::printf( "FARLODRTDRAW shared: cellRestir %zu bytes (GL buffer %u, texture %u), cellDdgiAtlas %zu bytes (GL buffer %u, texture %u)\n",
		rsBytes, rsBuf, rsTex, atBytes, atBuf, atTex );
	// the owners (as rtddgicell.cpp / the ReSTIR step register them): the gate writes the buffers itself, always fresh
	WwVkPresent::rtBufOwner( WwVkPresent::kRtBufRestir, []( std::string & ) { return true; } );
	WwVkPresent::rtBufOwner( WwVkPresent::kRtBufDdgiAtlas, []( std::string & ) { return true; } );
	// lane VKFAR30: the screen GI's two images (vkrtbuf.h rtImg), as ao.cpp's giShare makes them, at full size (step 1)
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
	std::printf( "FARLODRTDRAW shared: cellGiHalf texture %u, cellGiHalfKey texture %u (%dx%d RGBA32F, rtImgGen %llu)\n", giTex[0], giTex[1],
		W, H, static_cast<unsigned long long>( WwVkPresent::rtImgGen() ) );

	// ---- each key's samplers: one unit each; cellRestir / cellDdgiAtlas are the shared names
	for ( KeyArm & k : keys ) {
		GLint n = 0;
		f.getProgramiv( k.p, fActiveUnis, &n );
		bool hasRestir = false, hasAtlas = false;
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( k.p, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			Smp s;
			s.name = nm;
			s.glType = ty;
			if ( !smpOf( s, W, H ) ) {
				const bool sampler = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 ) || ( ty >= 0x9008 && ty <= 0x9110 );
				if ( sampler )
					return fatal( std::string( k.tag ) + " sampler " + s.name + " of a GL type the gate does not make (" + std::to_string( ty ) + ")" );
				continue;
			}
			if ( s.name == "cellRestir" ) {
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
			s.unit = int( k.smp.size() ) + 1;
			k.smp.push_back( s );
		}
		if ( !hasAtlas || hasRestir != ( k.v == 1 ) )
			return fatal( std::string( k.tag ) + " program: cellDdgiAtlas " + ( hasAtlas ? "active" : "missing" ) + ", cellRestir "
				+ ( hasRestir ? "active" : "missing" ) + " (not the key's RT inputs)" );
		std::string miss;
		int nMiss = 0;
		const WwVkPresent::FxField * fs = k.v == 1 ? WwVkPresent::kT_farlodRtPs : WwVkPresent::kT_farlodDgPs;
		const int nfs = k.v == 1 ? int( sizeof( WwVkPresent::kT_farlodRtPs ) / sizeof( WwVkPresent::kT_farlodRtPs[0] ) )
			: int( sizeof( WwVkPresent::kT_farlodDgPs ) / sizeof( WwVkPresent::kT_farlodDgPs[0] ) );
		for ( int i = 0; i < nfs; i++ )
			if ( f.getUniformLocation( k.p, fs[i].name ) < 0 ) {
				nMiss++;
				miss += std::string( " " ) + fs[i].name;
			}
		std::printf( "FARLODRTDRAW %s: %d of %d twin fields GL has no location for:%s\n", k.tag, nMiss, nfs, miss.c_str() );
		std::printf( "FARLODRTDRAW %s: %zu samplers active in GL's program:", k.tag, k.smp.size() );
		for ( const Smp & s : k.smp )
			std::printf( " %s%s", s.name.c_str(), s.unbound ? "(unbound)" : "" );
		std::printf( "\n" );
		if ( f.getUniformLocation( k.p, "cellGiHalfOn" ) < 0 )
			return fatal( std::string( k.tag ) + " program has no cellGiHalfOn (the refusal check would test nothing)" );
	}

	// ---- the blocks (farloddraw's): globalUniforms (the frame's camera), wwShadowFrame (seeded by member type), skinningUniforms
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
	for ( int b = 0; b < 3; b++ ) {
		GLint sizes[2] = { -1, -1 };
		for ( int j = 0; j < 2; j++ ) {
			const GLuint bi = f.getUniformBlockIndex( keys[j].p, blockName[b] );
			if ( bi != 0xFFFFFFFFu ) {
				f.uniformBlockBinding( keys[j].p, bi, GLuint( b ) );
				f.getActiveUniformBlockiv( keys[j].p, bi, fBlockSize, &sizes[j] );
			}
		}
		if ( sizes[0] != sizes[1] )
			return fatal( std::string( "block " ) + blockName[b] + ": RT " + std::to_string( sizes[0] ) + " vs DG " + std::to_string( sizes[1] ) + " bytes" );
		if ( sizes[0] > 0 )
			blockSize[b] = sizes[0];
		std::vector<std::uint8_t> z( size_t( std::max( blockSize[b], 16 ) ), 0 );
		f.bindBuffer( fUbo, ubo[b] );
		f.bufferData( fUbo, std::ptrdiff_t( z.size() ), z.data(), fStatic );
	}
	std::printf( "FARLODRTDRAW blocks: globalUniforms %d, wwShadowFrame %d, skinningUniforms %d bytes\n", blockSize[0], blockSize[1], blockSize[2] );
	struct ShMember
	{
		std::string name;
		GLint off = 0, type = 0, size = 1, stride = 0;
	};
	std::vector<ShMember> shMembers;
	{
		const GLuint p0 = keys[0].p, sbi = f.getUniformBlockIndex( p0, "wwShadowFrame" );
		GLint n = 0;
		f.getProgramiv( p0, fActiveUnis, &n );
		for ( GLint i = 0; i < n && sbi != 0xFFFFFFFFu; i++ ) {
			const GLuint idx = GLuint( i );
			GLint bi = -1;
			f.getActiveUniformsiv( p0, 1, &idx, fBlockIndex, &bi );
			if ( bi != GLint( sbi ) )
				continue;
			ShMember m;
			char nm[256] = {};
			GLenum ty = 0;
			f.getActiveUniform( p0, idx, 256, nullptr, &m.size, &ty, nm );
			m.name = nm;
			m.type = GLint( ty );
			f.getActiveUniformsiv( p0, 1, &idx, 0x8A3B, &m.off );     // GL_UNIFORM_OFFSET
			f.getActiveUniformsiv( p0, 1, &idx, 0x8A3C, &m.stride );  // GL_UNIFORM_ARRAY_STRIDE
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

	// ---- the meshes and instances: farloddraw's (cellfarinst.cpp's interleaved layout, stride 36)
	struct Vtx
	{
		float p[3];
		std::uint8_t c[4];
		std::uint32_t n, t, b;
		float uv[2];
	};
	static_assert( sizeof( Vtx ) == 36, "cellfarinst.cpp's stride" );
	std::vector<Vtx> vtx;
	std::vector<std::uint16_t> idx( 6, 0 );
	auto grid = [&]( int GX, int GZ, float x0, float x1, float z0, float z1, auto height, bool cwRows ) {
		const int base = int( vtx.size() );
		for ( int j = 0; j < GZ; j++ )
			for ( int i = 0; i < GX; i++ ) {
				const float x = x0 + ( x1 - x0 ) * float( i ) / float( GX - 1 ), z = z0 + ( z1 - z0 ) * float( j ) / float( GZ - 1 );
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
				const std::uint16_t a = std::uint16_t( j * GX + i ), b = std::uint16_t( a + 1 ), c = std::uint16_t( a + GX ),
					d = std::uint16_t( c + 1 );
				if ( cwRows && j % 3 == 1 )
					idx.insert( idx.end(), { a, c, b, b, c, d } );
				else
					idx.insert( idx.end(), { a, b, c, b, d, c } );
			}
		WwVkPresent::WaterCmd c;
		c.count = std::uint32_t( idx.size() ) - first;
		c.firstIndex = first;
		c.baseVertex = base;
		return c;
	};
	auto strip = []( float x, float z ) { return 25.0f * std::sin( x * 0.011f ) * std::cos( z * 0.013f ); };
	auto hill = []( float x, float z ) { return 80.0f * std::exp( -( x * x + z * z ) / 9000.0f ) + 6.0f * std::sin( x * 0.05f ); };
	std::vector<WwVkPresent::WaterCmd> cmds;
	cmds.push_back( grid( 32, 40, -400.0f, 400.0f, 0.0f, -1500.0f, strip, true ) );
	cmds.push_back( grid( 16, 16, -160.0f, 160.0f, 160.0f, -160.0f, hill, false ) );
	cmds[0].instanceCount = 2, cmds[0].baseInstance = 0;
	cmds[1].instanceCount = 2, cmds[1].baseInstance = 2;
	const int nVtx = int( vtx.size() );
	struct Inst
	{
		float row[3][4];
		float extra[4];
	};
	auto inst = [&]( float yawDeg, float scale, float tx, float ty, float tz, float ex0, float ey, float ez, float ew ) {
		const float a = yawDeg * 0.01745329f, c = std::cos( a ) * scale, s = std::sin( a ) * scale;
		Inst k { { { c, 0.0f, s, tx }, { 0.0f, scale, 0.0f, ty }, { -s, 0.0f, c, tz } }, { ex0, ey, ez, ew } };
		return k;
	};
	const Inst kInst[4] = { inst( 0.0f, 1.0f, -430.0f, -130.0f, -250.0f, 0.0f, 0.9f, 0.8f, 0.95f ),
		inst( 0.0f, 1.0f, 430.0f, -110.0f, -250.0f, 2.0f, 0.7f, 1.0f, 2.85f ),
		inst( 0.0f, 1.0f, -250.0f, -70.0f, -700.0f, 0.0f, 1.0f, 1.0f, 1.0f ),
		inst( 30.0f, 1.5f, 300.0f, -90.0f, -1300.0f, 0.0f, 0.8f, 0.9f, 1.0f ) };
	const float kLadder[4][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 1.5f, 0.0f }, { -1.5f, 0.0f } };
	GLuint vao = 0, vb = 0, ib = 0, lb = 0, eb = 0;
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
	f.genBuffers( 1, &ib );
	f.bindBuffer( 0x8892, ib );
	f.bufferData( 0x8892, std::ptrdiff_t( sizeof( kInst ) ), kInst, fStatic );
	for ( int a = 0; a < 4; a++ ) {
		ex.vertexAttribPointer( GLuint( 10 + a ), 4, GL_FLOAT, GL_FALSE, 64, reinterpret_cast<const void *>( std::uintptr_t( 16 * a ) ) );
		ex.enableVertexAttribArray( GLuint( 10 + a ) );
		ex.vertexAttribDivisor( GLuint( 10 + a ), 1 );
	}
	f.genBuffers( 1, &lb );
	f.bindBuffer( 0x8892, lb );
	f.bufferData( 0x8892, std::ptrdiff_t( sizeof( kLadder ) ), kLadder, fStatic );
	ex.vertexAttribPointer( 9, 2, GL_FLOAT, GL_FALSE, 8, nullptr );
	ex.enableVertexAttribArray( 9 );
	ex.vertexAttribDivisor( 9, 1 );
	f.genBuffers( 1, &eb );
	f.bindBuffer( 0x8893, eb );
	f.bufferData( 0x8893, std::ptrdiff_t( idx.size() * 2 ), idx.data(), fStatic );
	ex.bindVertexArray( 0 );
	const float zm = -900.0f, zd = ( ( proj[10] * zm + proj[14] ) / -zm ) * 0.5f + 0.5f;
	std::printf( "FARLODRTDRAW job: %dx%d x%d, %d vertices, %zu elements, 2 commands x 2 instances, slab depth %.6f\n", W, H, S, nVtx,
		idx.size(), zd );

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
	auto drawState = [&]( GLuint fb, bool scissor, const KeyArm & k ) {
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
		f.useProgram( k.p );
		for ( int b = 0; b < 3; b++ )
			f.bindBufferBase( fUbo, GLuint( b ), ubo[b] );
		for ( const Smp & s : k.smp ) {
			bindUnit( s.unit, targetOf( s.kind ), s.tex );
			f.uniform1i( f.getUniformLocation( k.p, s.name.c_str() ), s.unit );
		}
		ex.bindVertexArray( vao );
	};
	auto stateEnd = [&]( const KeyArm & k ) {
		ex.bindVertexArray( 0 );
		for ( const Smp & s : k.smp )
			bindUnit( s.unit, targetOf( s.kind ), 0 );
		glDisable( 0x8037 );
		glPolygonOffset( 0.0f, 0.0f );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	auto glDraw = [&]() {
		for ( const WwVkPresent::WaterCmd & c : cmds )
			ex.drawInst( GL_TRIANGLES, GLsizei( c.count ), GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( std::uintptr_t( c.firstIndex ) * 2u ),
				GLsizei( c.instanceCount ), c.baseVertex, c.baseInstance );
	};
	auto loc = [&]( const KeyArm & k, const char * n ) { return f.getUniformLocation( k.p, n ); };
	auto setI = [&]( const KeyArm & k, const char * n, GLint v ) {
		f.useProgram( k.p );
		if ( loc( k, n ) >= 0 )
			f.uniform1iv( loc( k, n ), 1, &v );
	};
	auto setF = [&]( const KeyArm & k, const char * n, int comps, int count, const float * v ) {
		f.useProgram( k.p );
		const GLint l = loc( k, n );
		if ( l < 0 )
			return;
		( comps == 1 ? f.uniform1fv : comps == 3 ? f.uniform3fv : f.uniform4fv )( l, count, v );
	};
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
			std::printf( "FARLODRTDRAW shared %s: rtBufOf refused (%s)\n", name, w.c_str() );
			return false;
		}
		glFinish();
		std::vector<std::uint8_t> seen;
		if ( !WwVkPresent::farTexelBufferRead( rb.key, seen, w ) ) {
			std::printf( "FARLODRTDRAW shared %s: farTexelBufferRead refused (%s)\n", name, w.c_str() );
			return false;
		}
		const size_t compared = std::min( seen.size(), want.size() ), bad = differ( seen, want );
		const bool ok = compared > 0 && bad == 0 && rb.glBuf == buf && rb.view != 0;
		std::printf( "FARLODRTDRAW shared %s: %zu bytes compared, %zu differ, GL buffer %u (bound %u), view %s: %s\n", name, compared, bad,
			rb.glBuf, buf, rb.view ? "set" : "none", ok ? "OK" : "FAIL" );
		return ok;
	};

	static char twinKeys[2][208], redKeys[2][208];
	bool gateOk = true;
	int runs = 0;
	for ( int ki = 0; ki < 2; ki++ )
		for ( int seed = 1; seed <= 2; seed++ ) {
			KeyArm & k = keys[ki];
			std::mt19937 rng( 20261116u + std::uint32_t( seed ) * 7919u );	// the same seed for both keys (frt1: DG's own gave a flat seed 1)
			for ( Smp & s : k.smp )
				if ( !fill( s, rng ) )
					return fatal( std::string( k.tag ) + " texture " + s.name );
			// farloddraw's seed 1 / 2 profiles without cellClusterOn (lights from cellLights), the RT GI switches on
			const std::string base = "cellOn,fogOn,volOn,cellFarOn,cellShadowOn,cellShadowFadeOn,ussOn,ussCsmOn,ussTerrOn,"
				"ussContactOn,ussCloudOn,cellAoOn,cellHasDir,landShoreOn,landWetOn,landSwashOn,waterInfoOn,"
				"cellRestirOn,cellDdgiOn,cellGiSky,cellLightCount,cellExtDalc";
			const std::string prof = "FLD_ON=" + base + ( seed == 1 ? ",cellSsgiOn,cellSsrOn,cellIsOn"
				: ",cellReflOn,cellReflCount,cellGoboOn,cellCtOn,cellFarPlacedOn,cellHasDalc,cellSpecOcc" );
			_putenv( prof.c_str() );
			const int nu = seedUnis( k.p, rng );
			blocks( rng );
			const bool scissor = seed == 2;
			setI( k, "farInst", 1 );
			setI( k, "farStreamOn", 0 );
			{
				const float vc[4] = { seed == 2 ? 0.7f : 0.0f, seed == 2 ? 0.5f : 0.0f, seed == 2 ? 3.0f : 0.0f, 0.0f };
				setF( k, "farVcAo", 4, 1, vc );
				const float fds1 = 1.0f / 1500.0f;
				setF( k, "fogDistScale", 1, 1, &fds1 );
			}
			// the RT GI seeding (header: Seeding)
			{
				const float row[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
				setF( k, "cellRow", 4, 3, row );
				const float rsz[4] = { float( W ), float( H ), 0.0f, 0.0f };
				setF( k, "cellRestirSize", 4, 1, rsz );
				const float org[4] = { -1200.0f, -900.0f, -2100.0f, 300.0f }, stp[4] = { 300.0f, 300.0f, 300.0f, 0.0f };
				setF( k, "cellDdgiOrigin", 4, 1, org );
				setF( k, "cellDdgiStep", 4, 1, stp );
				setF( k, "cellDdgiCOrigin", 4, 1, org );
				setF( k, "cellDdgiCStep", 4, 1, stp );
				f.useProgram( k.p );
				if ( loc( k, "cellDdgiDims" ) >= 0 )
					f.uniform4iv( loc( k, "cellDdgiDims" ), 1, kDdgiDims );
				if ( loc( k, "cellDdgiCDims" ) >= 0 )
					f.uniform4iv( loc( k, "cellDdgiCDims" ), 1, kDdgiDims );
				setI( k, "cellDdgiCBase", 0 );
				setI( k, "cellDdgiCount", 1 );
				setI( k, "cellGiLoop8", 8 );
				setI( k, "cellGiSky", 1 );
				setI( k, "cellExtDalc", 1 );
				setI( k, "cellGiAmb", 1 );
				setI( k, "cellLightCount", 1 );
				setI( k, "cellClusterOn", 0 );
				setI( k, "cellRestirOn", 1 );
				setI( k, "cellDdgiOn", 1 );
				setI( k, "cellGiHalfOn", 0 );
			}
			if ( seed == 1 ) {	// lane VKFAR29 seeding (header): the loop bounds' vec4-earlier neighbours zero, red 1 must not hang
				const struct { const char * n; int comps, c; } nb[4] = { { "cellCenter", 3, 1 }, { "cellRoomsDims", 3, 1 },
					{ "cellCtRect", 4, 1 }, { "cellClusterLog", 2, 0 } };
				int zeroed = 0;
				for ( const auto & z : nb ) {
					const GLint l = loc( k, z.n );
					if ( l < 0 )
						continue;
					float v[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					f.useProgram( k.p );
					f.getUniformfv( k.p, l, v );
					v[z.c] = 0.0f;
					( z.comps == 2 ? f.uniform2fv : z.comps == 3 ? f.uniform3fv : f.uniform4fv )( l, 1, v );
					zeroed++;
				}
				std::printf( "FARLODRTDRAW %s seed 1: %d of 4 loop-bound neighbours zeroed (cellCenter.y, cellRoomsDims.y, cellCtRect.y, "
					"cellClusterLog.x)\n", k.tag, zeroed );
			}
			f.bindBuffer( 0x8892, ib );
			ex.bufferSubData( 0x8892, 0, std::ptrdiff_t( sizeof( kInst ) ), kInst );
			for ( const Smp & s : k.smp )	// light 0: a shadow-casting omni (t4 = (-1, 1): no cube slot), radius 5000, flat radial
				if ( s.name == "cellLights" ) {
					const float L[32] = { 0.0f, 150.0f, -700.0f, 5000.0f, 1.0f, 0.8f, 0.6f, -2.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
						-1.0f, 1.0f, 0.0f, 0.0f };
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
			std::printf( "FARLODRTDRAW %s seed %d: %d loose uniforms seeded%s\n", k.tag, seed, nu, scissor ? ", a scissor, the rows' vertex-colour AO" : "" );

			before( fbRef );
			const std::vector<std::uint16_t> bgC = readColor( refCol );
			const std::vector<float> bgD = readDepth( refDs, false );
			// the ratio: GL's depth first (it does not read the ratio), then (ratio, D) where the rings cover the pixel's sample 0
			before( fbRef );
			drawState( fbRef, scissor, k );
			glDraw();
			stateEnd( k );
			const std::vector<float> preD = readDepth( refDs, false );
			std::vector<float> restir( size_t( W ) * size_t( H ) * 2 );
			int onRings = 0;
			{
				std::uniform_real_distribution<float> u( 0.05f, 1.0f );
				for ( int y = 0; y < H; y++ )
					for ( int x = 0; x < W; x++ ) {
						const size_t s0 = size_t( y ) * size_t( WS ) + size_t( x ) * size_t( S ), t = size_t( y ) * size_t( W ) + size_t( x );
						restir[2 * t] = u( rng );
						restir[2 * t + 1] = 1.0e6f;
						if ( preD[s0] != bgD[s0] ) {
							restir[2 * t + 1] = proj[14] / ( 2.0f * preD[s0] - 1.0f + proj[10] );
							onRings++;
						}
					}
				bufWrite( rsBuf, restir.data(), rsBytes );
			}
			std::printf( "FARLODRTDRAW %s seed %d: cellRestir ratio over %d of %d pixels (the rings' depth), the rest 1e6\n", k.tag, seed,
				onRings, W * H );
			{
				std::vector<std::uint8_t> a( atBytes ), r2( rsBytes );
				std::memcpy( a.data(), atlas.data(), atBytes );
				std::memcpy( r2.data(), restir.data(), rsBytes );
				if ( !sharedCheck( "cellDdgiAtlas", atBuf, a ) || !sharedCheck( "cellRestir", rsBuf, r2 ) )
					gateOk = false;
			}

			before( fbRef );
			drawState( fbRef, scissor, k );
			glDraw();
			stateEnd( k );
			const GLenum ge = glGetError();
			if ( ge != GL_NO_ERROR )
				return fatal( "the GL reference draw: GL error " + std::to_string( ge ) );
			const std::vector<std::uint16_t> glC = readColor( refCol );
			const std::vector<float> glD = readDepth( refDs, false );
			size_t glCovered = 0;
			std::vector<char> cov( glD.size(), 0 );
			{	// the drawn colour must vary (a constant layer hides the inputs)
				std::set<std::uint64_t> distinct;
				for ( size_t s = 0; s < glD.size(); s++ ) {
					bool c = glD[s] != bgD[s];
					for ( int ch = 0; ch < 4; ch++ )
						c = c || glC[s * 4 + size_t( ch )] != bgC[s * 4 + size_t( ch )];
					cov[s] = c;
					glCovered += c;
					if ( glD[s] == bgD[s] )
						continue;
					std::uint64_t key = 0;
					for ( int ch = 0; ch < 4; ch++ )
						key = ( key << 16 ) | glC[s * 4 + size_t( ch )];
					distinct.insert( key );
				}
				std::printf( "FARLODRTDRAW %s seed %d GL: %zu samples covered, %zu distinct colours over the rings\n", k.tag, seed, glCovered,
					distinct.size() );
				if ( distinct.size() < 1000 ) {
					std::printf( "FARLODRTDRAW %s seed %d: under 1000 distinct colours over the rings: a near-constant layer is blind\n", k.tag, seed );
					gateOk = false;
				}
			}
			// GL probes: each RT input off must move >= 20 % of GL's covered samples
			for ( const char * sw : { "cellRestirOn", "cellDdgiOn" } ) {
				if ( k.v == 2 && std::strcmp( sw, "cellRestirOn" ) == 0 )
					continue;
				setI( k, sw, 0 );
				before( fbRef );
				drawState( fbRef, scissor, k );
				glDraw();
				stateEnd( k );
				setI( k, sw, 1 );
				const std::vector<std::uint16_t> pc = readColor( refCol );
				size_t moved = 0;
				for ( size_t s = 0; s < cov.size(); s++ ) {
					if ( !cov[s] )
						continue;
					bool m = false;
					for ( int ch = 0; ch < 4; ch++ )
						m = m || pc[s * 4 + size_t( ch )] != glC[s * 4 + size_t( ch )];
					moved += m;
				}
				const bool ok = glCovered > 0 && moved * 5 >= glCovered;
				std::printf( "FARLODRTDRAW %s seed %d GL probe %s 0: moved %zu of %zu covered samples (%.1f %%): %s\n", k.tag, seed, sw, moved,
					glCovered, glCovered ? 100.0 * double( moved ) / double( glCovered ) : 0.0, ok ? "OK" : "FAIL (the input does not reach the colour)" );
				if ( !ok )
					gateOk = false;
			}

			// the Vulkan runs: red 0, then (seed 1) the reds
			enum { kMain, kRed1, kRedRestir, kRedAtlas, kRedKey };
			const char * caseName[5] = { "red 0", "red 1 (ps block one vec4 off)", "red restir (rewritten after GL drew)",
				"red atlas (shifted one texel after GL drew)", "red key (wrong twin)" };
			for ( int cs = kMain; cs <= kRedKey; cs++ ) {
				if ( cs != kMain && seed != 1 )
					continue;
				if ( cs == kRedRestir && k.v != 1 )
					continue;
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
				before( fbFrame );
				drawState( fbFrame, scissor, k );
				WwVkPresent::WaterRun r;
				bool ran = false;
				if ( cs == kRedKey && k.v == 1 ) {  // the DG twin on the RT program
					WwVkPresent::farLodRtStateOf( &gw, k.p, r, 2 );
					r.w = W, r.h = H, r.color = fc, r.cmds = cmds, r.key = reinterpret_cast<std::uintptr_t>( redKeys[ki] );
					ran = WwVkPresent::farLodRtRun( r, 2, why );
				} else if ( cs == kRedKey ) {  // the base twin on the DG program
					WwVkPresent::farLodStateOf( &gw, k.p, r );
					r.w = W, r.h = H, r.color = fc, r.cmds = cmds, r.key = reinterpret_cast<std::uintptr_t>( redKeys[ki] );
					ran = WwVkPresent::farLodRun( r, why );
				} else {
					WwVkPresent::farLodRtStateOf( &gw, k.p, r, k.v );
					r.w = W, r.h = H, r.color = fc, r.cmds = cmds, r.key = reinterpret_cast<std::uintptr_t>( twinKeys[ki] );
					r.red = cs == kRed1 ? 1 : 0;
					ran = WwVkPresent::farLodRtRun( r, k.v, why );
				}
				stateEnd( k );
				runs++;
				std::vector<std::uint16_t> vkC;
				std::vector<float> vkD;
				if ( ran ) {
					vkC = readColor( fc );
					vkD = readDepth( fds, true );
				}
				if ( cs == kRedRestir )
					bufWrite( rsBuf, restir.data(), rsBytes );
				else if ( cs == kRedAtlas )
					bufWrite( atBuf, atlas.data(), atBytes );
				if ( !ran ) {
					const bool okRefuse = cs == kRedKey;
					std::printf( "FARLODRTDRAW %s seed %d %s: REFUSED (%s)%s\n", k.tag, seed, caseName[cs], why.c_str(),
						okRefuse ? " -> red refused, as it may" : "" );
					if ( !okRefuse )
						gateOk = false;
					continue;
				}
				const Verdict v = judge( bgC, bgD, vkC, vkD, glC, glD );
				const char * word = v.word();
				const bool fail = std::strcmp( word, "FAIL" ) == 0;
				std::printf( "FARLODRTDRAW %s seed %d %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
					"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g)%s\n", k.tag, seed, caseName[cs], word, v.covered, v.both,
					v.onlyOne, v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel,
					cs != kMain ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : "" );
				if ( cs != kMain ? !fail : fail )
					gateOk = false;
			}
			/* lane VKFLIGHT18 (frame time), the batch / cost arm (seed 1): kN draws of the twin, a crossing each (farLodRtRun, the
			 * app before) vs queued (farLodRtQueue + farGroundFlush, the app now): crossings, CPU ms a draw, Vulkan's GPU ms a timed
			 * crossing (fxWork timestamps, read at the next crossing). Bars: both match GL, queued EXACT vs a crossing each, queued
			 * crossings = ceil( kN / kTwinBatchMax ) for kN draws, 0 lost. Red: queued and read before the flush must FAIL (the
			 * queue draws nothing until its flush). Without WW_VK_FARLODRTQ (vkfarlodrt.inc) the arm FAILS. */
			if ( seed == 1 ) {
#ifdef WW_VK_FARLODRTQ
				const int kN = 64, kind = k.v == 2 ? 22 : 21;
				struct QImg
				{
					std::vector<std::uint16_t> c;
					std::vector<float> d;
					bool ran = true;
					double ms = 0.0, gpuMs = 0.0;
					int gpuN = 0, cross = 0, draws = 0, lost = 0;
					std::string why;
				};
				// mode 0 a crossing each, 1 queued + flushed, 2 (red) queued, read before the flush
				auto qRun = [&]( int mode ) {
					QImg o;
					before( fbFrame );
					( void )WwVkPresent::takeFarLodRtDrawStat( k.v );
					( void )WwVkPresent::takeFarLodRtBatchStat( k.v );
					int miss = 0;
					WwVkPresent::takeFxGpu( kind, o.gpuMs, o.gpuN, miss );
					const double t0 = nowMs();
					for ( int i = 0; i < kN; i++ ) {
						drawState( fbFrame, scissor, k );
						WwVkPresent::WaterRun r;
						WwVkPresent::farLodRtStateOf( &gw, k.p, r, k.v );
						r.w = W, r.h = H, r.color = fc, r.cmds = cmds, r.key = reinterpret_cast<std::uintptr_t>( twinKeys[ki] );
						o.ran = ( mode ? WwVkPresent::farLodRtQueue( r, k.v, o.why ) : WwVkPresent::farLodRtRun( r, k.v, o.why ) ) && o.ran;
						stateEnd( k );
					}
					if ( mode == 1 )
						o.ran = WwVkPresent::farGroundFlush( o.why ) && o.ran;
					o.ms = ( nowMs() - t0 ) / double( kN );
					if ( mode == 2 ) {
						o.c = readColor( fc );
						o.d = readDepth( fds, true );
						o.ran = WwVkPresent::farGroundFlush( o.why ) && o.ran;
					}
					runs++;
					const WwVkPresent::FxStat ds = WwVkPresent::takeFarLodRtDrawStat( k.v );
					const WwVkPresent::TwinBatchStat bs = WwVkPresent::takeFarLodRtBatchStat( k.v );
					o.cross = mode ? bs.crossings : ds.runs;
					o.draws = mode ? bs.draws : ds.runs;
					o.lost = bs.lost;
					if ( mode != 2 ) {
						o.c = readColor( fc );
						o.d = readDepth( fds, true );
					}
					WwVkPresent::takeFxGpu( kind, o.gpuMs, o.gpuN, miss );
					return o;
				};
				const int wantCross = ( kN + farlod::kTwinBatchMax - 1 ) / farlod::kTwinBatchMax;
				auto qArm = [&]( const char * what, const QImg & o, int mode ) {
					if ( !o.ran ) {
						std::printf( "FARLODRTDRAW %s batch %s: REFUSED (%s)\n", k.tag, what, o.why.c_str() );
						gateOk = false;
						return;
					}
					const Verdict v = judge( bgC, bgD, o.c, o.d, glC, glD );
					const char * word = v.word();
					const bool fail = std::strcmp( word, "FAIL" ) == 0;
					const bool crossOk = mode == 0 ? o.cross == kN : ( o.cross == wantCross && o.draws == kN && !o.lost );
					std::printf( "FARLODRTDRAW %s batch %s: %s covered %zu (both %zu, one side only %zu), colour %zu halves differ; %d crossings for "
						"%d draws, %d lost, %.4f ms CPU a draw, GPU %.4f ms a timed crossing (%d timed)%s\n", k.tag, what, word, v.covered, v.both,
						v.onlyOne, v.halvesDiff, o.cross, kN, o.lost, o.ms, o.gpuN ? o.gpuMs / o.gpuN : 0.0, o.gpuN,
						mode == 2 ? ( fail ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" ) : crossOk ? "" : " -> NOT the crossings wanted" );
					if ( mode == 2 ? !fail : ( fail || !crossOk ) )
						gateOk = false;
				};
				const QImg singles = qRun( 0 );
				qArm( "64 draws, a crossing each", singles, 0 );
				const QImg queued = qRun( 1 );
				qArm( "64 draws queued + flushed", queued, 1 );
				size_t cd = 0, dd = 0;
				for ( size_t i = 0; i < queued.c.size() && i < singles.c.size(); i++ )
					cd += queued.c[i] != singles.c[i];
				for ( size_t i = 0; i < queued.d.size() && i < singles.d.size(); i++ )
					dd += queued.d[i] != singles.d[i];
				const bool exact = !cd && !dd && !queued.c.empty() && queued.c.size() == singles.c.size() && queued.d.size() == singles.d.size();
				std::printf( "FARLODRTDRAW %s batch queued vs a crossing each: %s (%zu colour halves, %zu depth samples differ)\n", k.tag,
					exact ? "EXACT" : "DIFFER", cd, dd );
				gateOk = gateOk && exact;
				qArm( "red (queued, read before the flush)", qRun( 2 ), 2 );
				std::printf( "FARLODRTDRAW %s batch cost for %d draws: a crossing each %d crossings, %.4f ms CPU a draw; queued %d crossings, "
					"%.4f ms CPU a draw (%.1fx)\n", k.tag, kN, singles.cross, singles.ms, queued.cross, queued.ms,
					queued.ms > 0.0 ? singles.ms / queued.ms : 0.0 );
#else
				std::printf( "FARLODRTDRAW %s batch: FAIL (no far LOD RT queue: farLodRtQueue / farLodRtRunN are not built, WW_VK_FARLODRTQ undefined)\n",
					k.tag );
				gateOk = false;
#endif
			}
			// lane VKFAR30: the all-on arm (header): cellGiHalfOn 1 on VKBUF2's shared screen GI images (lane VKFAR31: both keys)
			if ( seed == 1 ) {
				Smp * sg = nullptr, * sk = nullptr;
				for ( Smp & s : k.smp ) {
					if ( s.name == "cellGiHalf" )
						sg = &s;
					else if ( s.name == "cellGiHalfKey" )
						sk = &s;
				}
				if ( !sg || !sk )
					return fatal( std::string( k.tag ) + " program: cellGiHalf / cellGiHalfKey not active (the all-on arm would test nothing)" );
				const GLuint plainG = sg->tex, plainK = sk->tex;	// the gate's own fills, bound again after the arm
				auto getI = [&]( const char * n ) {
					float x = 0.0f;
					if ( loc( k, n ) >= 0 ) {
						f.useProgram( k.p );
						f.getUniformfv( k.p, loc( k, n ), &x );
					}
					return GLint( x );
				};
				const GLint passWas = getI( "cellPass" ), splatWas = getI( "landSplat" );
				setI( k, "cellPass", 0 );
				setI( k, "cellProbe", 0 );
				{
					const float rect[4] = { 0.0f, 0.0f, float( W ), float( H ) }, one = 1.0f;
					setF( k, "cellGiHalfRect", 4, 1, rect );
					setF( k, "cellGiHalfScale", 1, 1, &one );
				}
				// the key: GL's own shading normal (cellProbe 1, header) and depth at each pixel's sample 0
				setI( k, "cellGiHalfOn", 0 );
				setI( k, "cellProbe", 1 );
				setI( k, "landSplat", 0 );
				before( fbRef );
				drawState( fbRef, scissor, k );
				glDraw();
				stateEnd( k );
				setI( k, "cellProbe", 0 );
				setI( k, "landSplat", splatWas );
				const std::vector<std::uint16_t> nC = readColor( refCol );
				const std::vector<float> nD = readDepth( refDs, false );
				// lane VKFAR31: the key at one texel per `step` pixels, texel h = the pixel step * h (cell_gihalf_ps's writer)
				size_t written = 0, unitN = 0;
				auto makeKey = [&]( int step, int tw, int th, std::vector<float> & keyT ) {
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
							keyT[t] = xn * D / proj[0];
							keyT[t + 1] = yn * D / proj[5];
							keyT[t + 2] = -D;
							const float a = std::fabs( n[0] ) + std::fabs( n[1] ) + std::fabs( n[2] );
							float ox = n[0] / a, oy = n[1] / a;
							if ( n[2] < 0.0f ) {
								const float tx = ( 1.0f - std::fabs( oy ) ) * ( ox >= 0.0f ? 1.0f : -1.0f ), ty = ( 1.0f - std::fabs( ox ) ) * ( oy >= 0.0f ? 1.0f : -1.0f );
								ox = tx, oy = ty;
							}
							keyT[t + 3] = std::floor( ( ox * 0.5f + 0.5f ) * 2047.0f + 0.5f ) * 2048.0f + std::floor( ( oy * 0.5f + 0.5f ) * 2047.0f + 0.5f );
						}
				};
				std::vector<float> keyT, giT( static_cast<size_t>( W ) * static_cast<size_t>( H ) * 4u ), giT2( giT.size() );
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
				std::printf( "FARLODRTDRAW %s all-on key: %zu texels on the rings, %zu decode to a unit normal (want >= 1000, >= 90 %%): %s\n", k.tag,
					written, unitN, keyOk ? "OK" : "FAIL" );
				if ( !keyOk )
					gateOk = false;
				// GL's reference on the shared names
				sg->tex = giTex[0], sk->tex = giTex[1];
				setI( k, "cellGiHalfOn", 1 );
				auto glFrame = [&]() {
					before( fbRef );
					drawState( fbRef, scissor, k );
					glDraw();
					stateEnd( k );
					return readColor( refCol );
				};
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
				auto movedOf = [&]( const std::vector<std::uint16_t> & pc ) {
					size_t m = 0;
					for ( size_t s = 0; s < aMask.size(); s++ ) {
						if ( !aMask[s] )
							continue;
						bool d = false;
						for ( int ch = 0; ch < 4; ch++ )
							d = d || pc[s * 4 + size_t( ch )] != aC[s * 4 + size_t( ch )];
						m += d;
					}
					return m;
				};
				setI( k, "cellGiHalfOn", 0 );
				const size_t mOff = movedOf( glFrame() );
				setI( k, "cellGiHalfOn", 1 );
				toShared( gPlain2, 0 );
				const size_t mImg = movedOf( glFrame() );
				toShared( gPlain, 0 );
				for ( int pr = 0; pr < 2; pr++ ) {
					const size_t m = pr == 0 ? mOff : mImg;
					const bool ok = aCov > 0 && m * 5 >= aCov;
					std::printf( "FARLODRTDRAW %s all-on GL probe %s: moved %zu of %zu covered samples (%.1f %%): %s\n", k.tag,
						pr == 0 ? "cellGiHalfOn 0" : "cellGiHalf reseeded", m, aCov, aCov ? 100.0 * double( m ) / double( aCov ) : 0.0,
						ok ? "OK" : "FAIL (the screen GI does not reach the colour)" );
					if ( !ok )
						gateOk = false;
				}
				// the Vulkan runs: BOUND / EXACT, FAIL, or REFUSED (with the reason)
				// lane VKFAR31: `between` gets the run (a red edits it); refC / refD: another GL reference (the half-rate arm)
				auto vkRun = [&]( const char * name, const std::function<void( WwVkPresent::WaterRun & )> & between,
						const std::vector<std::uint16_t> * refC = nullptr, const std::vector<float> * refD = nullptr ) {
					before( fbFrame );
					drawState( fbFrame, scissor, k );
					WwVkPresent::WaterRun r;
					WwVkPresent::farLodRtStateOf( &gw, k.p, r, k.v );
					r.w = W, r.h = H, r.color = fc, r.cmds = cmds, r.key = reinterpret_cast<std::uintptr_t>( twinKeys[ki] );
					if ( between )
						between( r );
					std::string w;
					const bool ran = WwVkPresent::farLodRtRun( r, k.v, w );
					stateEnd( k );
					runs++;
					std::string word = "REFUSED";
					if ( ran ) {
						const Verdict v = judge( bgC, bgD, readColor( fc ), readDepth( fds, true ), refC ? *refC : aC, refD ? *refD : aD );
						word = v.word();
						std::printf( "FARLODRTDRAW %s all-on %s: %s covered %zu (both %zu, one side only %zu), depth max %ld D24 units (%zu past 1), "
							"colour %zu halves differ, %zu samples past 1e-2 (max rel %.3g)", k.tag, name, word.c_str(), v.covered, v.both, v.onlyOne,
							v.maxDepthU, v.depthPast, v.halvesDiff, v.colPast, v.maxRel );
					} else
						std::printf( "FARLODRTDRAW %s all-on %s: REFUSED (%s)", k.tag, name, w.c_str() );
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
				auto red = [&]( const std::pair<std::string, std::string> & res, bool ok ) {
					redsFailing += ok;
					std::printf( "%s\n", ok ? " -> red FAILS as it must" : " -> red PASSES: the gate is blind" );
					if ( !ok )
						gateOk = false;
				};
				clean( "clean" );
				sg->tex = gPlain;	// red not shared: the same bytes in a plain GL texture
				{
					const auto res = vkRun( "red not shared", nullptr );
					red( res, res.first == "REFUSED" && res.second.find( "cellGiHalf:" ) != std::string::npos );
				}
				sg->tex = giTex[0];
				clean( "clean after red not shared" );
				sg->tex = giTex[1], sk->tex = giTex[0];	// red wrong image: the two shared images swapped on the twin side
				{
					const auto res = vkRun( "red wrong image", nullptr );
					red( res, res.first == "FAIL" || res.first == "REFUSED" );
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
					red( res, remadeOk && genMoved && res.first != "BOUND" && res.first != "EXACT" );
					if ( !remadeOk )
						return fatal( "rtImgShare cellGiHalf again: " + rw );
					giTex[0] = remade;
					sg->tex = remade;
					toShared( gPlain, 0 );
				}
				clean( "clean after red stale gen" );
				{	// lane VKFAR31 red key mismatch: a borrowed entry of a key that sorts first, on cellGiHalf's GL name (farSharedOf answers it)
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
					red( res, borrowed && answer == lowKey && res.first == "REFUSED"
						&& res.second.find( "cellGiHalf: the shared image is not the bound name's" ) != std::string::npos );
				}
				clean( "clean after red key mismatch" );
				{	// lane VKFAR31 red no sampler: cellGiHalf out of the run's census
					const auto res = vkRun( "red no sampler", []( WwVkPresent::WaterRun & r ) {
						auto & in = r.census.inputs;
						in.erase( std::remove_if( in.begin(), in.end(), []( const WwVkPresent::WaterSlot & x ) { return x.name == "cellGiHalf"; } ),
							in.end() );
					} );
					red( res, res.first == "REFUSED" && res.second.find( "cellGiHalf: GL's program has no such sampler" ) != std::string::npos );
				}
				clean( "clean after red no sampler" );
				std::printf( "FARLODRTDRAW %s all-on: clean %d of %d BOUND, reds failing %d of 5\n", k.tag, cleanBound, cleanRuns, redsFailing );
				{	// lane VKFAR31: the half-rate arm (header)
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
					std::printf( "FARLODRTDRAW %s half rate (%dx%d): key %zu texels on the rings, %zu decode to a unit normal (want >= 250, >= 90 %%): %s\n",
						k.tag, hw, hh, written, unitN, hKeyOk ? "OK" : "FAIL" );
					if ( !hKeyOk )
						gateOk = false;
					sg->tex = half[0], sk->tex = half[1];
					const float hRect[4] = { 0.0f, 0.0f, float( hw ), float( hh ) }, hScale = 0.5f, one = 1.0f;
					setF( k, "cellGiHalfRect", 4, 1, hRect );
					setF( k, "cellGiHalfScale", 1, 1, &hScale );
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
					auto hMoved = [&]( const std::vector<std::uint16_t> & pc ) {
						size_t m = 0;
						for ( size_t s = 0; s < hMask.size(); s++ ) {
							if ( !hMask[s] )
								continue;
							bool d = false;
							for ( int ch = 0; ch < 4; ch++ )
								d = d || pc[s * 4 + size_t( ch )] != hC[s * 4 + size_t( ch )];
							m += d;
						}
						return m;
					};
					size_t hm[3];
					setI( k, "cellGiHalfOn", 0 );
					hm[0] = hMoved( glFrame() );
					setI( k, "cellGiHalfOn", 1 );
					toHalf( hPlain2, 0 );
					hm[1] = hMoved( glFrame() );
					toHalf( hPlain, 0 );
					setF( k, "cellGiHalfScale", 1, 1, &one );
					hm[2] = hMoved( glFrame() );
					setF( k, "cellGiHalfScale", 1, 1, &hScale );
					const char * hProbe[3] = { "cellGiHalfOn 0", "cellGiHalf reseeded", "cellGiHalfScale 1" };
					for ( int pr = 0; pr < 3; pr++ ) {
						const bool ok = hCov > 0 && hm[pr] * 5 >= hCov;
						std::printf( "FARLODRTDRAW %s half rate GL probe %s: moved %zu of %zu covered samples (%.1f %%): %s\n", k.tag, hProbe[pr], hm[pr],
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
						const WwVkPresent::FxField * fs = k.v == 2 ? WwVkPresent::kT_farlodDgPs : WwVkPresent::kT_farlodRtPs;
						const size_t nf = k.v == 2 ? sizeof( WwVkPresent::kT_farlodDgPs ) / sizeof( WwVkPresent::kT_farlodDgPs[0] )
							: sizeof( WwVkPresent::kT_farlodRtPs ) / sizeof( WwVkPresent::kT_farlodRtPs[0] );
						for ( size_t i = 0; i < nf; i++ )
							if ( std::strcmp( fs[i].name, "cellGiHalfScale" ) == 0 && fs[i].offset + 4 <= r.psBytes )
								std::memcpy( r.psLoose + fs[i].offset, &one, 4 );
					}, true );
					hRun( "half rate clean after red", nullptr, false );
					std::printf( "FARLODRTDRAW %s half rate: clean %d of 2 BOUND, reds failing %d of 1\n", k.tag, hClean, hRed );
					// both images at W x H again (the next key's arm copies W x H into them)
					for ( int slot = 0; slot < 2; slot++ ) {
						std::string rw;
						WwVkPresent::rtImgDrop( slot, giKeyOf( slot ) );
						if ( !WwVkPresent::rtImgShare( slot, giKeyOf( slot ), W, H, 0, 0, giTex[slot], rw ) )
							return fatal( std::string( "rtImgShare full " ) + WwVkPresent::rtImgName( slot ) + ": " + rw );
					}
					setF( k, "cellGiHalfScale", 1, 1, &one );
					const GLuint hDel[3] = { hPlain, hPlain2, hKey };
					glDeleteTextures( 3, hDel );
				}
				sg->tex = plainG, sk->tex = plainK;
				setI( k, "cellGiHalfOn", 0 );
				setI( k, "cellPass", passWas );
				const GLuint del[3] = { gPlain, gPlain2, kPlain };
				glDeleteTextures( 3, del );
			}
		}
	for ( int v = 1; v <= 2; v++ ) {
		const WwVkPresent::FxStat st = WwVkPresent::takeFarLodRtDrawStat( v );
		std::printf( "FARLODRTDRAW twin %d (%s): %d on Vulkan, %.2f ms CPU each (max %.2f), %d fails (%s), %d copies\n", v, v == 1 ? "RT" : "DG",
			st.runs, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.fails, st.failWhy.c_str(), st.copies );
	}
	std::printf( "FARLODRTDRAW %d runs\n", runs );
	std::printf( gateOk ? "FARLODRTDRAW GATE OK\n" : "FARLODRTDRAW GATE FAILED\n" );
	return gateOk ? 0 : 1;
}
