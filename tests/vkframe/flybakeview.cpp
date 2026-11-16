// Lane VKCLOUD5 (2026-11-16): the fly-through box's bake (cloudfly.cpp runPass) on the present device (vkcloudflybake.inc
// flyBakeRun: one render pass a slice into a 3D R8 shared target through sliceView, fxWork kind 19) against GL's own
// cloud_flybake / cloud_flysdf programs drawn as runPass draws them (one glFramebufferTextureLayer a slice, cfLayer = k).
// fxview.cpp's helpers are reused (its main renamed). Job: tests/vkframe/flybake_prep.py (the Low preset, 64 x 64 x 16).
// GL reference chain: bake -> gv0, sdf x gv0 -> gv1, y gv1 -> gv2, z gv2 -> gv3.
// Vulkan arms on four shared targets S0..S3 (static char keys), each filled with the sentinel 200 by glClearTexImage before
// the run, read back TWICE with glGetTexImage (the reads must be equal; the first is judged):
//   A  bake [0, nz) into S0 vs gv0
//   B  sdf x from GL's gv0 (copied into S0) into S1 vs gv1;  C  sdf y from gv1 into S2 vs gv2;  D  sdf z from gv2 into S3 vs gv3
//   E  the whole chain on Vulkan: S0 -> S1 -> S2 -> S3, each vs gv0..gv3
//   F  the bake phased as the rolling build draws it: [0, 5) then [5, nz) into S0 vs gv0
//   G0 the route header's GL side (wwFlyBakeRoute wantVk false): GL_SAMPLES_PASSED == nxy * nxy * nz and S0 vs gv0
//   G  the route on Vulkan: GL_SAMPLES_PASSED == 0 around the whole route, onVk, S0 vs gv0
//   M  lane VKCLOUD7, the shape's mip chain: cfShapeMip reads cvShape at levels 0, 1, 2 (Load) and keeps the max, and generated
//      mips never exceed level 0, so the arms above cannot see a dropped level copy. M's cvShape = the job's generated chain
//      with level 0 cleared to 0 (levels 1 and 2 carry the bound); M2 levels 0 and 1 cleared (level 2 only). Each vs GL on the
//      same texture. Then cvShape's and cvDetail's twins vs GL at every level (cfProbe 0: the bake never reads cvDetail).
//      Job checks: fxTexOf gives cvShape and cvDetail >= 3 levels; M's GL bake 0 < occ < total; M and M2 unlike GL's bake on
//      an all-zero shape.
//   Q  lane VKCLOUD9, the full build in ONE crossing (flyBakeRunN: the four passes in one command buffer, a barrier between
//      passes, a descriptor set and b31 slot range a pass) in the app's ping-pong (cloudfly.cpp runPass: bake -> S0, x S0 -> S1,
//      y S1 -> S0, z S0 -> S2 = vol[back]: pass 2 overwrites what pass 0 wrote and pass 1 read). Q4 = the four single runs
//      (flyBakeRun, today's path), Q1 = the batch. After each: S1 vs gv1, S0 vs gv2, S2 vs gv3. Crossings and runs are read off
//      the bake's own stat (before / after, no take; the counter moves where fxCross returned true).
//      Bars: Q4 4 crossings and 4 runs (the counter counts); Q1 1 crossing and 4 runs; Q1's three volumes byte-EXACT vs Q4's.
//   V  lane VKCLOUD9, pass 0 reads its inputs in place: cvShape, cvDetail, cvMap copied (glCopyImageSubData, every level fxTexOf
//      gave) into harness shared targets with the same levels, filter and wrap. GL's bake on the shared names vs gv0 EXACT (the
//      same bytes); a warm Vulkan run, then plain (GL's own names bound) and shared (the shared names bound), copies read off the
//      bake's stat before / after each. Bars: plain copies > 0, shared copies == 0, plain and shared S0 vs gv0 EXACT.
//   I  lane VKCLOUD10, the inputs made as the app makes them (cloudinshare.h): ensureVol / ensureMap with want true (shared
//      targets) and false (GL's own names), the map drawn by a blit of the fixture's. Bars: every level EXACT vs the fixture's,
//      GL's bake on either set vs gv0 EXACT, Vulkan on either set vs gv0 EXACT, copies plain 3 / shared 0, inPlace none / all 3,
//      a remake after a drop (a present shutdown) EXACT with 0 copies.
// Bar (set before the first run, never widened): every arm EXACT (0 texels differ), read twice equal, no GL error.
// Job checks (else the job cannot judge): 0 < occupancy < total, gv0 holds only 0 / 255, gv1..gv3 only 0..15, each pass
// changes its input, some slice differs from slice 0 and at least one adjacent slice pair differs.
// Reds (each must FAIL = texels differ on a fresh sentinel, read twice equal; printed "N of M texels differ, U unwritten",
// moved = differ - unwritten; stuck shows only as unwritten on this job: slice 15, drawn last into slice 0, equals slice 0):
//   stuck    every slice drawn into slice 0 (FlyBakeRun.red 1)
//   offby1   slice k drawn into slice k + 1 (red 2)
//   glwrites the route with redGlWrites (GL draws after Vulkan): GL_SAMPLES_PASSED > 0
//   nomip    (lane VKCLOUD7) M's run with cvShape read from a twin of level 0 only, levels 1.. cleared to 0 (FxTex.shared):
//            must FAIL and equal GL's all-zero-shape bake; the same red on the job's own chain is INFO (not judged)
//   perpass  (lane VKCLOUD9) Q1 with red 16: flyBakeRunN crosses once a pass (today's submit): must FAIL the crossing bar
//            (4, want 1); its texels are INFO
//   oneset   (lane VKCLOUD9) Q1 with red 32: every pass records with pass 0's descriptor set and b31 slots: texels must differ
//   skip     (lane VKCLOUD9) V's shared run with red 8: the farSharedOf lookup skipped: copies must be > 0 (FAIL the bar)
//   never shared / stalenoise / stalemap / undrawnmap  (lane VKCLOUD10, arm I; the shared memory cleared to 255 / 1.0 before
//            each drop): copies must be > 0; a noise remake not filled, a map remake not attached to the map's FBO, a map remake
//            not drawn again: texels must differ from gv0
//   swap     the other pass's program (red 4: cloud_flysdf, b31 zero but cfLayer), run LAST
// Last line `FLYBAKEVIEW GATE OK` or `FLYBAKEVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include "features/clouds/cloudflybakeroute.h"

namespace fbv
{
using namespace fv;

struct Res
{
	size_t diff = 0, unwritten = 0, total = 0;
	bool twice = false;
	size_t moved() const { return diff - unwritten; }
};

std::uintptr_t keyOf( const char & c )
{
	return reinterpret_cast<std::uintptr_t>( &c );
}
}  // namespace fbv

int main( int argc, char ** argv )
{
	using namespace fbv;
	setvbuf( stdout, nullptr, _IONBF, 0 );
	gTag = "FLYBAKEVIEW";
	auto fatal = []( const std::string & m ) {
		std::printf( "FLYBAKEVIEW %s\nFLYBAKEVIEW GATE FAILED\n", m.c_str() );
		return 2;
	};
	if ( argc < 2 )
		return fatal( "usage: flybakeview <prep dir>" );
	const std::string dir = std::string( argv[1] ) + "/flybake";
	using ClearTex = void( WINAPI * )( GLuint, GLint, GLenum, GLenum, const void * );
	using FbLayer = void( WINAPI * )( GLenum, GLenum, GLuint, GLint, GLint );
	using GenQ = void( WINAPI * )( GLsizei, GLuint * );
	using BeginQ = void( WINAPI * )( GLenum, GLuint );
	using EndQ = void( WINAPI * )( GLenum );
	using GetQ = void( WINAPI * )( GLuint, GLenum, GLuint * );
	ClearTex clearTex = nullptr;
	FbLayer fbLayer = nullptr;
	GenQ genQ = nullptr;
	BeginQ beginQ = nullptr;
	EndQ endQ = nullptr;
	GetQ getQ = nullptr;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwflybakeview";
	RegisterClassA( &wc );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwflybakeview", "fly bake harness", WS_OVERLAPPEDWINDOW, 40, 40, 160, 90, nullptr, nullptr,
		wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	pump();
	if ( !loadAll() || !load( clearTex, "glClearTexImage" ) || !load( fbLayer, "glFramebufferTextureLayer" ) || !load( genQ, "glGenQueries" )
		|| !load( beginQ, "glBeginQuery" ) || !load( endQ, "glEndQuery" ) || !load( getQ, "glGetQueryObjectuiv" ) )
		return fatal( "GL entry points" );
	std::printf( "FLYBAKEVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	int nxy = 0, nz = 0;
	{
		std::ifstream jf( dir + "/job.txt" );
		jf >> nxy >> nz;
	}
	if ( nxy < 1 || nz < 6 )
		return fatal( "job.txt: no box size" );
	const GLuint pBake = program( kTriVs, readFile( dir + "/bake.frag.glsl" ), "cloud_flybake" );
	const GLuint pSdf = program( kTriVs, readFile( dir + "/sdf.frag.glsl" ), "cloud_flysdf" );
	if ( !pBake || !pSdf )
		return fatal( "GL programs" );
	const UniMap uni = readUni( dir + "/bake.uni" );
	const int setB = setUnis( pBake, uni ), setS = setUnis( pSdf, uni );
	std::vector<TexIn> tin = readTex( dir );
	if ( tin.size() != 3 )
		return fatal( "tex.txt: want cvShape, cvDetail, cvMap" );
	for ( TexIn & t : tin )
		if ( !upload( dir, t, 0 ) )
			return fatal( "texture " + t.name );
	std::printf( "FLYBAKEVIEW job: %d x %d x %d cells, %d bake + %d sdf uniforms set, inputs %s %s %s\n", nxy, nxy, nz, setB, setS, tin[0].name.c_str(),
		tin[1].name.c_str(), tin[2].name.c_str() );
	std::string why;
	if ( !WwVkPresent::viewBegin( 160, 90, 1, glProc, why ) )
		return fatal( "the view: " + why );
	const size_t total = static_cast<size_t>( nxy ) * static_cast<size_t>( nxy ) * static_cast<size_t>( nz );
	const size_t plane = static_cast<size_t>( nxy ) * static_cast<size_t>( nxy );

	// ---- GL as runPass draws: cloud_flybake's units t0..t2 (the reflected order), cloud_flysdf's source on unit 3
	GLuint shapeGl = tin[0].gl;          // lane VKCLOUD7: the cvShape bound (arms M, M2, red nomip swap it)
	std::uintptr_t shapeShared = 0;      // non-zero: Vulkan reads cvShape from this shared target (red nomip)
	int texLevels[3] = {};               // the levels fxTexOf gave each input
	auto bindBake = [&]() {
		f.useProgram( pBake );
		for ( int k = 0; k < 3; k++ ) {
			bindUnit( k, tin[static_cast<size_t>( k )].target, k == 0 ? shapeGl : tin[static_cast<size_t>( k )].gl );
			u1i( pBake, tin[static_cast<size_t>( k )].name.c_str(), k );
		}
	};
	auto vol3 = [&]() {
		GLuint t = 0;
		glGenTextures( 1, &t );
		glBindTexture( fT3D, t );
		f.texImage3D( fT3D, 0, GLint( fR8 ), nxy, nxy, nz, 0, fRed, GL_UNSIGNED_BYTE, nullptr );
		texParams( fT3D, false, false, false );
		glBindTexture( fT3D, 0 );
		return t;
	};
	GLuint fb = 0;
	f.genFramebuffers( 1, &fb );
	auto glSlice = [&]( GLuint prog, GLuint dst, int k ) {
		f.bindFramebuffer( fFb, fb );
		fbLayer( fFb, fCol0, dst, 0, k );
		const GLenum b0 = fCol0;
		f.drawBuffers( 1, &b0 );
		if ( f.checkFramebufferStatus( fFb ) != fComplete )
			return false;
		glViewport( 0, 0, nxy, nxy );
		u1i( prog, "cfLayer", k );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		return true;
	};
	auto glUse = [&]( int pass, GLuint src ) {
		drawState0();
		if ( pass == 0 ) {
			bindBake();
			return pBake;
		}
		bindUnit( 3, fT3D, src );
		u1i( pSdf, "cfSrc", 3 );
		u1i( pSdf, "cfAxis", pass - 1 );
		return pSdf;
	};
	auto glPass = [&]( int pass, GLuint src, GLuint dst, int from, int to ) {
		const GLuint prog = glUse( pass, src );
		bool ok = true;
		for ( int k = from; k < to && ok; k++ )
			ok = glSlice( prog, dst, k );
		f.bindFramebuffer( fFb, 0 );
		return ok && glGetError() == GL_NO_ERROR;
	};
	auto read = [&]( GLuint t ) {
		std::vector<std::uint8_t> a( total );
		glBindTexture( fT3D, t );
		glGetTexImage( fT3D, 0, fRed, GL_UNSIGNED_BYTE, a.data() );
		glBindTexture( fT3D, 0 );
		return a;
	};
	GLuint gv[4] = {};
	for ( GLuint & t : gv )
		t = vol3();
	if ( !glPass( 0, 0, gv[0], 0, nz ) || !glPass( 1, gv[0], gv[1], 0, nz ) || !glPass( 2, gv[1], gv[2], 0, nz ) || !glPass( 3, gv[2], gv[3], 0, nz ) )
		return fatal( "the GL reference chain did not draw" );
	std::vector<std::uint8_t> ref[4];
	for ( int k = 0; k < 4; k++ )
		ref[k] = read( gv[k] );

	// ---- the job must be able to judge the pass
	{
		size_t occ = 0, bad0 = 0, badS = 0, adj = 0;
		for ( std::uint8_t x : ref[0] ) {
			occ += x == 255;
			bad0 += x != 0 && x != 255;
		}
		for ( int k = 1; k < 4; k++ )
			for ( std::uint8_t x : ref[k] )
				badS += x > 15;
		auto sliceEq = [&]( int a, int b ) {
			return std::equal( ref[0].begin() + std::ptrdiff_t( plane * size_t( a ) ), ref[0].begin() + std::ptrdiff_t( plane * size_t( a + 1 ) ),
				ref[0].begin() + std::ptrdiff_t( plane * size_t( b ) ) );
		};
		for ( int z = 0; z + 1 < nz; z++ )
			adj += !sliceEq( z, z + 1 );
		// red stuck needs a slice unlike slice 0 (the band fills the middle slices: slice 0 and nz - 1 may both be clear)
		size_t notZero = 0;
		for ( int z = 1; z < nz; z++ )
			notZero += !sliceEq( 0, z );
		const bool ends = notZero > 0;
		const bool p1 = ref[1] != ref[0], p2 = ref[2] != ref[1], p3 = ref[3] != ref[2];
		std::printf( "FLYBAKEVIEW INFO occupancy %zu of %zu cells (0 < occ < total: %s); gv0 off 0/255: %zu; gv1..3 past 15: %zu; adjacent slice pairs "
			"that differ: %zu of %d; slices unlike slice 0: %zu of %d; passes change their input: x %d y %d z %d\n",
			occ, total, occ > 0 && occ < total ? "yes" : "NO", bad0, badS, adj, nz - 1, notZero, nz - 1, p1, p2, p3 );
		if ( !( occ > 0 && occ < total ) || bad0 || badS || !adj || !ends || !p1 || !p2 || !p3 )
			return fatal( "the job cannot judge the bake (see INFO)" );
	}

	// ---- the shared targets and the Vulkan runs
	static char S[4], twinKeys[3];
	GLuint st[4] = {};
	for ( int k = 0; k < 4; k++ ) {
		if ( !WwVkPresent::sharedTarget( keyOf( S[k] ), 1, nxy, nxy, nz, fR8, st[k], why, 1 ) )
			return fatal( "shared target S" + std::to_string( k ) + ": " + why );
		const Shared * s = sharedFind( keyOf( S[k] ) );
		if ( !s || !s->slices )
			return fatal( "shared target S" + std::to_string( k ) + " has no slices" );
		glBindTexture( fT3D, st[k] );
		texParams( fT3D, false, false, false );
	}
	glBindTexture( fT3D, 0 );
	auto sentinel = [&]( int k ) {
		const std::uint8_t s = 200;
		clearTex( st[k], 0, fRed, GL_UNSIGNED_BYTE, &s );
		glFinish();
	};
	auto copyIn = [&]( int g, int k ) {  // GL's output into S(k): a GL copy, never an upload
		f.copyImageSubData( gv[g], fT3D, 0, 0, 0, 0, st[k], fT3D, 0, 0, 0, 0, nxy, nxy, nz );
		glFinish();
	};
	// one run's job from GL's program state (lane VKCLOUD9: split from vkRun so the batch arm Q1 can queue four)
	auto vkJob = [&]( int pass, int srcK, int dstK, int from, int to, int red, WwVkPresent::FlyBakeRun & t ) -> bool {
		t = WwVkPresent::FlyBakeRun();
		t.pass = pass;
		t.from = from;
		t.to = to;
		t.dst = keyOf( S[dstK] );
		t.src = srcK >= 0 ? keyOf( S[srcK] ) : 0;
		t.red = red;
		if ( pass == 0 ) {
			bindBake();
			for ( int k = 0; k < 3; k++ ) {
				const TexIn & x = tin[static_cast<size_t>( k )];
				if ( !WwVkPresent::fxTexOf( &ga, pBake, x.name.c_str(), x.type, t.tex[k], why ) ) {
					std::printf( "FLYBAKEVIEW fxTexOf refused: %s\n", why.c_str() );
					return false;
				}
				t.tex[k].twin = keyOf( twinKeys[k] );
				texLevels[k] = t.tex[k].levels;
				if ( k == 0 && shapeShared )
					t.tex[0].shared = shapeShared;
			}
			WwVkPresent::fxPack( &ga, pBake, WwVkPresent::kT_flybake, int( std::size( WwVkPresent::kT_flybake ) ), t.loose );
		} else {
			u1i( pSdf, "cfAxis", pass - 1 );
			WwVkPresent::fxPack( &ga, pSdf, WwVkPresent::kT_flysdf, int( std::size( WwVkPresent::kT_flysdf ) ), t.loose );
		}
		return true;
	};
	auto vkRun = [&]( int pass, int srcK, int dstK, int from, int to, int red ) -> bool {
		WwVkPresent::FlyBakeRun t;
		if ( !vkJob( pass, srcK, dstK, from, to, red, t ) )
			return false;
		if ( !WwVkPresent::flyBakeRun( t, why ) ) {
			std::printf( "FLYBAKEVIEW flyBakeRun refused: %s\n", why.c_str() );
			return false;
		}
		return glGetError() == GL_NO_ERROR;
	};
	auto judgeVol = [&]( int k, const std::vector<std::uint8_t> & want ) {
		Res r;
		const std::vector<std::uint8_t> a = read( st[k] ), b = read( st[k] );
		r.twice = a == b;
		r.total = a.size();
		for ( size_t i = 0; i < a.size() && i < want.size(); i++ ) {
			r.diff += a[i] != want[i];
			r.unwritten += a[i] == 200;
		}
		r.diff += a.size() != want.size() ? 1 : 0;
		return r;
	};
	bool ok = true;
	auto arm = [&]( const std::string & name, const Res & r, bool ran ) {
		const GLenum e = glGetError();
		const bool exact = ran && r.diff == 0 && r.twice && r.total > 0 && e == GL_NO_ERROR;
		std::printf( "FLYBAKEVIEW %s: %zu of %zu texels differ, %zu unwritten, read twice %s, GL error 0x%04X: %s\n", name.c_str(), r.diff, r.total,
			r.unwritten, r.twice ? "equal" : "DIFFERS", unsigned( e ), exact ? "EXACT" : ran ? "FAIL" : "FAIL (the run was refused)" );
		ok = ok && exact;
	};
	auto redArm = [&]( const std::string & name, const Res & r, bool ran ) {
		// the sentinel is cleared right before each red and every green reads 0 unwritten: a texel left at 200 is the red's miss
		const bool bit = ran && r.twice && r.diff > 0;
		std::printf( "FLYBAKEVIEW red %s: %s (%zu of %zu texels differ, %zu unwritten, %zu moved%s)\n", name.c_str(), bit ? "FAIL" : "EXACT", r.diff,
			r.total, r.unwritten, r.moved(), ran ? "" : ", the run was refused" );
		if ( !bit ) {
			std::printf( "FLYBAKEVIEW red %s PASSED: the gate cannot see it\n", name.c_str() );
			ok = false;
		}
	};
	{  // A
		sentinel( 0 );
		const bool ran = vkRun( 0, -1, 0, 0, nz, 0 );
		arm( "A bake [0, " + std::to_string( nz ) + ") vs GL", judgeVol( 0, ref[0] ), ran );
	}
	const char * axis[3] = { "x", "y", "z" };
	for ( int p = 1; p <= 3; p++ ) {  // B, C, D: each sdf pass from GL's previous output
		copyIn( p - 1, p - 1 );
		sentinel( p );
		const bool ran = vkRun( p, p - 1, p, 0, nz, 0 );
		arm( std::string( 1, char( 'A' + p ) ) + " sdf " + axis[p - 1] + " from GL's gv" + std::to_string( p - 1 ) + " vs GL", judgeVol( p, ref[p] ), ran );
	}
	{  // E: the whole chain on Vulkan
		for ( int k = 0; k < 4; k++ )
			sentinel( k );
		bool ran = vkRun( 0, -1, 0, 0, nz, 0 );
		for ( int p = 1; p <= 3 && ran; p++ )
			ran = vkRun( p, p - 1, p, 0, nz, 0 );
		for ( int k = 0; k < 4; k++ )
			arm( "E chain S" + std::to_string( k ) + " vs GL gv" + std::to_string( k ), judgeVol( k, ref[k] ), ran );
	}
	{  // F: the rolling build's phases
		sentinel( 0 );
		const bool ran = vkRun( 0, -1, 0, 0, 5, 0 ) && vkRun( 0, -1, 0, 5, nz, 0 );
		arm( "F bake [0, 5) + [5, " + std::to_string( nz ) + ") vs GL", judgeVol( 0, ref[0] ), ran );
	}
	GLuint q = 0;
	genQ( 1, &q );
	// the route over the bake into S0; returns GL_SAMPLES_PASSED around the whole route call
	auto route = [&]( bool wantVk, bool redGlWrites, bool & onVk, bool & drew ) {
		sentinel( 0 );
		glUse( 0, 0 );
		beginQ( 0x8914, q );  // GL_SAMPLES_PASSED
		drew = wwFlyBakeRoute(
			wantVk, redGlWrites, [&]( int from, int to ) { return vkRun( 0, -1, 0, from, to, 0 ); },
			[&]( int k ) { return glSlice( pBake, st[0], k ); }, 0, nz, onVk );
		endQ( 0x8914 );
		f.bindFramebuffer( fFb, 0 );
		GLuint n = 0;
		getQ( q, 0x8866, &n );  // GL_QUERY_RESULT
		return n;
	};
	{  // G0: the route's GL side
		bool onVk = true, drew = false;
		const GLuint n = route( false, false, onVk, drew );
		std::printf( "FLYBAKEVIEW G0 the route on GL: onVk %d, GL_SAMPLES_PASSED %u (want %zu)\n", onVk ? 1 : 0, n, total );
		ok = ok && !onVk && drew && size_t( n ) == total;
		arm( "G0 the route on GL vs GL", judgeVol( 0, ref[0] ), drew && !onVk );
	}
	{  // G: the route on Vulkan, GL draws nothing
		bool onVk = false, drew = false;
		const GLuint n = route( true, false, onVk, drew );
		std::printf( "FLYBAKEVIEW G the route on Vulkan: onVk %d, GL_SAMPLES_PASSED %u (want 0)\n", onVk ? 1 : 0, n );
		ok = ok && onVk && drew && n == 0;
		arm( "G the route on Vulkan vs GL", judgeVol( 0, ref[0] ), drew && onVk );
	}
	// ---- M, M2 (lane VKCLOUD7): the shape's mip chain carries the bound
	const std::uint8_t zero8 = 0;
	int shapeLev = 1;
	while ( ( tin[0].w >> shapeLev ) > 0 )
		shapeLev++;
	auto shapeCut = [&]( int cut ) {  // the job's cvShape with its generated chain, levels [0, cut) cleared to 0
		TexIn t = tin[0];
		t.gl = 0;
		if ( !upload( dir, t, 0 ) )
			return GLuint( 0 );
		for ( int l = 0; l < cut; l++ )
			clearTex( t.gl, l, fRed, GL_UNSIGNED_BYTE, &zero8 );
		glFinish();
		return t.gl;
	};
	const GLuint shapeM = shapeCut( 1 ), shapeM2 = shapeCut( 2 ), shapeZ = shapeCut( shapeLev );
	if ( !shapeM || !shapeM2 || !shapeZ || glGetError() != GL_NO_ERROR )
		return fatal( "arm M's shape textures" );
	auto glBakeWith = [&]( GLuint shape ) {
		shapeGl = shape;
		GLuint t = vol3();
		const bool drew = glPass( 0, 0, t, 0, nz );
		std::vector<std::uint8_t> a = read( t );
		glDeleteTextures( 1, &t );
		shapeGl = tin[0].gl;
		return drew ? a : std::vector<std::uint8_t>();
	};
	const std::vector<std::uint8_t> refM = glBakeWith( shapeM ), refM2 = glBakeWith( shapeM2 ), refZ = glBakeWith( shapeZ );
	{
		auto occOf = [&]( const std::vector<std::uint8_t> & a ) {
			size_t n = 0;
			for ( std::uint8_t x : a )
				n += x == 255;
			return n;
		};
		const size_t oJ = occOf( ref[0] ), oM = occOf( refM ), oM2 = occOf( refM2 ), oZ = occOf( refZ );
		std::printf( "FLYBAKEVIEW INFO M: cvShape %d levels; occupancy job %zu, M (level 0 cleared) %zu, M2 (levels 0, 1 cleared) %zu, all-zero shape "
			"%zu of %zu; M unlike the all-zero bake: %s, M2 unlike it: %s, M2 unlike M: %s\n",
			shapeLev, oJ, oM, oM2, oZ, total, refM != refZ ? "yes" : "NO", refM2 != refZ ? "yes" : "NO", refM2 != refM ? "yes" : "no" );
		if ( refM.size() != total || refM2.size() != total || refZ.size() != total || !( oM > 0 && oM < total ) || refM == refZ || refM2 == refZ )
			return fatal( "arm M cannot judge the mip chain (see INFO)" );
	}
	auto vkWith = [&]( GLuint shape, const std::vector<std::uint8_t> & want, const std::string & name ) {
		shapeGl = shape;
		sentinel( 0 );
		const bool ran = vkRun( 0, -1, 0, 0, nz, 0 );
		shapeGl = tin[0].gl;
		arm( name, judgeVol( 0, want ), ran );
	};
	vkWith( shapeM, refM, "M bake, cvShape level 0 cleared (levels 1, 2 carry the bound) vs GL" );
	vkWith( shapeM2, refM2, "M2 bake, cvShape levels 0, 1 cleared (level 2 carries the bound) vs GL" );
	for ( int k = 0; k < 2; k++ ) {  // the twins after M2: every level as GL's
		const TexIn & x = tin[static_cast<size_t>( k )];
		const GLuint src = k == 0 ? shapeM2 : x.gl;
		GLuint tw = 0;
		if ( !WwVkPresent::sharedTarget( keyOf( twinKeys[k] ), 1, x.w, x.h, x.d, fR8, tw, why, texLevels[k] ) )
			return fatal( "the " + x.name + " twin: " + why );
		size_t d = 0, all = 0;
		for ( int l = 0; l < texLevels[k]; l++ ) {
			const size_t m = static_cast<size_t>( std::max( x.w >> l, 1 ) ) * static_cast<size_t>( std::max( x.h >> l, 1 ) )
				* static_cast<size_t>( std::max( x.d >> l, 1 ) );
			std::vector<std::uint8_t> a( m ), b( m );
			glBindTexture( fT3D, src );
			glGetTexImage( fT3D, l, fRed, GL_UNSIGNED_BYTE, a.data() );
			glBindTexture( fT3D, tw );
			glGetTexImage( fT3D, l, fRed, GL_UNSIGNED_BYTE, b.data() );
			for ( size_t i = 0; i < m; i++ )
				d += a[i] != b[i];
			all += m;
		}
		glBindTexture( fT3D, 0 );
		const bool same = d == 0 && all > 0 && texLevels[k] >= 3 && glGetError() == GL_NO_ERROR;
		std::printf( "FLYBAKEVIEW M %s twin: %d levels (want >= 3), %zu of %zu texels differ: %s\n", x.name.c_str(), texLevels[k], d, all,
			same ? "EXACT" : "FAIL" );
		ok = ok && same;
	}
	// ---- lane VKCLOUD9: the full build in one crossing, the app's ping-pong (S0 vol[0], S1 vol[1], S2 vol[back])
	struct QOut
	{
		bool ran = false;
		int crossings = 0, runs = 0;
		Res r[3];                           // S0 vs gv2, S1 vs gv1, S2 vs gv3
		std::vector<std::uint8_t> v[3];
	};
	auto qRun = [&]( bool batch, int red ) {
		static const int kSrc[4] = { -1, 0, 1, 0 }, kDst[4] = { 0, 1, 0, 2 }, kWant[3] = { 2, 1, 3 };
		QOut o;
		for ( int k = 0; k < 3; k++ )
			sentinel( k );
		const WwVkPresent::FxStat & S = flyBakeJob().st;
		const int c0 = S.crossings, r0 = S.runs;
		o.ran = true;
		if ( batch ) {
			WwVkPresent::FlyBakeRun t[4];
			for ( int p = 0; p < 4 && o.ran; p++ )
				o.ran = vkJob( p, kSrc[p], kDst[p], 0, nz, p == 0 ? red : 0, t[p] );
			if ( o.ran && !WwVkPresent::flyBakeRunN( t, 4, why ) ) {
				std::printf( "FLYBAKEVIEW flyBakeRunN refused: %s\n", why.c_str() );
				o.ran = false;
			}
			o.ran = o.ran && glGetError() == GL_NO_ERROR;
		} else {
			for ( int p = 0; p < 4 && o.ran; p++ )
				o.ran = vkRun( p, kSrc[p], kDst[p], 0, nz, 0 );
		}
		o.crossings = S.crossings - c0;
		o.runs = S.runs - r0;
		for ( int k = 0; k < 3; k++ ) {
			o.r[k] = judgeVol( k, ref[kWant[k]] );
			o.v[k] = read( st[k] );
		}
		return o;
	};
	{
		const char * vol[3] = { "S0 (y, vol[0]) vs GL gv2", "S1 (x, vol[1]) vs GL gv1", "S2 (z, vol[back]) vs GL gv3" };
		const QOut q4 = qRun( false, 0 );
		for ( int k = 0; k < 3; k++ )
			arm( std::string( "Q4 four crossings, " ) + vol[k], q4.r[k], q4.ran );
		const bool c4 = q4.ran && q4.crossings == 4 && q4.runs == 4;
		std::printf( "FLYBAKEVIEW Q4 counts: %d crossings, %d runs (want 4, 4): %s\n", q4.crossings, q4.runs, c4 ? "OK" : "FAIL" );
		const QOut q1 = qRun( true, 0 );
		for ( int k = 0; k < 3; k++ )
			arm( std::string( "Q1 one crossing, " ) + vol[k], q1.r[k], q1.ran );
		const bool c1 = q1.ran && q1.crossings == 1 && q1.runs == 4;
		std::printf( "FLYBAKEVIEW Q1 counts: %d crossings, %d runs (want 1, 4): %s\n", q1.crossings, q1.runs, c1 ? "OK" : "FAIL" );
		bool same = q4.ran && q1.ran;
		size_t d = 0;
		for ( int k = 0; k < 3; k++ ) {
			same = same && !q1.v[k].empty() && q1.v[k].size() == q4.v[k].size();
			for ( size_t i = 0; same && i < q1.v[k].size(); i++ )
				d += q1.v[k][i] != q4.v[k][i];
		}
		same = same && d == 0;
		std::printf( "FLYBAKEVIEW Q1 vs Q4 (the 4-crossing path), S0 S1 S2: %zu texels differ: %s\n", d, same ? "EXACT" : "FAIL" );
		ok = ok && c4 && c1 && same;
	}
	// ---- lane VKCLOUD9, V: pass 0 reads shared inputs in place (no copy); red skip (red 8) must copy
	static char inKeys[3];
	GLuint inSh[3] = {};
	for ( int k = 0; k < 3; k++ ) {
		const TexIn & x = tin[static_cast<size_t>( k )];
		GLenum ifmt = 0, pf = 0, ty = 0;
		int bpp = 0;
		if ( !fmtOf( x.fmt, ifmt, pf, ty, bpp ) || texLevels[k] < 1
			|| !WwVkPresent::sharedTarget( keyOf( inKeys[k] ), x.type, x.w, x.h, x.d, ifmt, inSh[k], why, texLevels[k] ) )
			return fatal( "arm V's shared " + x.name + ": " + why );
		glBindTexture( x.target, inSh[k] );
		texParams( x.target, x.filt == "linear", x.wrap == "repeat", x.mips != 0 );
		glBindTexture( x.target, 0 );
		for ( int l = 0; l < texLevels[k]; l++ )
			f.copyImageSubData( x.gl, x.target, l, 0, 0, 0, inSh[k], x.target, l, 0, 0, 0, std::max( x.w >> l, 1 ), std::max( x.h >> l, 1 ),
				std::max( x.d >> l, 1 ) );
	}
	glFinish();
	if ( glGetError() != GL_NO_ERROR )
		return fatal( "arm V's shared inputs: a GL error" );
	const GLuint plainGl[3] = { tin[0].gl, tin[1].gl, tin[2].gl };
	auto useIn = [&]( bool shared ) {
		for ( int k = 0; k < 3; k++ )
			tin[static_cast<size_t>( k )].gl = shared ? inSh[k] : plainGl[k];
		shapeGl = tin[0].gl;
	};
	auto vRun = [&]( bool shared, int red, int & copies ) {
		useIn( shared );
		sentinel( 0 );
		const int c0 = flyBakeJob().st.copies;
		const bool ran = vkRun( 0, -1, 0, 0, nz, red );
		copies = flyBakeJob().st.copies - c0;
		useIn( false );
		return ran;
	};
	{
		useIn( true );
		GLuint t = vol3();
		const bool drew = glPass( 0, 0, t, 0, nz );
		const std::vector<std::uint8_t> a = read( t );
		glDeleteTextures( 1, &t );
		useIn( false );
		size_t d = 0;
		for ( size_t i = 0; i < a.size() && i < ref[0].size(); i++ )
			d += a[i] != ref[0][i];
		const bool same = drew && a.size() == ref[0].size() && d == 0;
		std::printf( "FLYBAKEVIEW V GL's bake on the shared inputs vs gv0: %zu of %zu texels differ: %s\n", d, a.size(), same ? "EXACT" : "FAIL" );
		ok = ok && same;
		int cw = 0, cp = 0, cs = 0;
		const bool ranW = vRun( true, 0, cw );
		const bool ranP = vRun( false, 0, cp );
		arm( "V plain (GL's own input names) vs GL", judgeVol( 0, ref[0] ), ranP );
		const bool ranS = vRun( true, 0, cs );
		arm( "V shared (the inputs read in place) vs GL", judgeVol( 0, ref[0] ), ranS );
		const bool cOk = ranW && ranP && ranS && cp > 0 && cs == 0;
		std::printf( "FLYBAKEVIEW V copies a run: warm %d, plain %d, shared %d (want plain > 0, shared 0): %s\n", cw, cp, cs, cOk ? "OK" : "FAIL" );
		ok = ok && cOk;
		int cr = 0;
		const bool ranR = vRun( true, 8, cr );
		const bool bit = ranR && cr > 0;
		std::printf( "FLYBAKEVIEW red skip (the shared lookup skipped, red 8): %s (%d copies, want 0; INFO S0 %zu texels differ)\n", bit ? "FAIL" : "EXACT",
			cr, judgeVol( 0, ref[0] ).diff );
		if ( !bit ) {
			std::printf( "FLYBAKEVIEW red skip PASSED: the gate cannot see it\n" );
			ok = false;
		}
	}
	// ---- lane VKCLOUD10, I: the inputs made as the app makes them (cloudinshare.h ensureVol / ensureMap): want true = shared
	// targets read in place, want false = GL's own names. Bars: every level EXACT vs the fixture's; GL's bake on either set vs gv0
	// EXACT; Vulkan on either set vs gv0 EXACT; copies plain 3, shared 0; inPlace plain none, shared cvShape cvDetail cvMap; a remake
	// after a drop EXACT again. Reds (must FAIL; the shared memory cleared to 255 / 1.0 before each drop): never shared (copies),
	// a noise remake not filled, a map remake not attached to the map's FBO, a map remake not drawn again.
	{
		namespace IS = WwCloudInShare;
		using BlitFb = void( WINAPI * )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum );
		const BlitFb blit = reinterpret_cast<BlitFb>( glProc( "glBlitFramebuffer" ) );
		const unsigned all3 = WwVkPresent::kInShape | WwVkPresent::kInDetail | WwVkPresent::kInMap;
		GlN gn = glN();
		static char inKeysI[4];
		const std::uintptr_t kIn = keyOf( inKeysI[0] );
		const int nS = tin[0].w, nD = tin[1].w, nM = tin[2].w;
		const std::string bS = readFile( dir + "/" + tin[0].file ), bD = readFile( dir + "/" + tin[1].file );
		GLenum mFmt = 0, mPf = 0, mTy = 0;
		int mBpp = 0;
		if ( !blit || tin[0].name != "cvShape" || tin[1].name != "cvDetail" || tin[2].name != "cvMap" || tin[2].h != nM
			|| !fmtOf( tin[2].fmt, mFmt, mPf, mTy, mBpp ) || bS.size() != static_cast<size_t>( nS ) * nS * nS
			|| bD.size() != static_cast<size_t>( nD ) * nD * nD )
			return fatal( "arm I's fixture" );
		const GLuint own[3] = { tin[0].gl, tin[1].gl, tin[2].gl };
		GLuint rFb = 0, sFb = 0, pFb = 0, uFb = 0;
		for ( GLuint * b : { &rFb, &sFb, &pFb, &uFb } )
			f.genFramebuffers( 1, b );
		f.bindFramebuffer( fFb, rFb );
		f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, own[2], 0 );
		f.bindFramebuffer( fFb, 0 );
		struct InSet
		{
			WwCloudInShare::Vol s, d, m;
			int size = 0;
			GLuint fb = 0;
		};
		InSet pl, shd, un;
		pl.fb = pFb;
		shd.fb = sFb;
		un.fb = uFb;
		auto make = [&]( InSet & z, bool want, unsigned redS, unsigned redM, bool fill ) -> bool {
			const bool s = IS::ensureVol( &gn, z.s, kIn, reinterpret_cast<const std::uint8_t *>( bS.data() ), nS, want, "cvShape", redS );
			const bool d = IS::ensureVol( &gn, z.d, kIn + 1, reinterpret_cast<const std::uint8_t *>( bD.data() ), nD, want, "cvDetail", redS );
			bool remade = false;
			const bool m = IS::ensureMap( &gn, z.m, z.size, z.fb, kIn + 2, nM, mFmt, want, remade, redM );
			if ( remade && fill ) {
				f.bindFramebuffer( 0x8CA8, rFb );
				f.bindFramebuffer( 0x8CA9, z.fb );
				blit( 0, 0, nM, nM, 0, 0, nM, nM, GL_COLOR_BUFFER_BIT, GL_NEAREST );
			}
			f.bindFramebuffer( fFb, 0 );
			glFinish();
			return s && d && m && glGetError() == GL_NO_ERROR;
		};
		auto use = [&]( const InSet & z ) {
			tin[0].gl = z.s.tex;
			tin[1].gl = z.d.tex;
			tin[2].gl = z.m.tex;
			shapeGl = tin[0].gl;
		};
		auto sameLevels = [&]( const InSet & z, size_t & d, size_t & all ) {
			d = all = 0;
			for ( int i = 0; i < 3; i++ ) {
				const GLuint y0 = i == 0 ? z.s.tex : i == 1 ? z.d.tex : z.m.tex;
				const GLenum tg = i < 2 ? fT3D : GLenum( GL_TEXTURE_2D );
				const int n = i == 0 ? nS : i == 1 ? nD : nM, levels = i < 2 ? IS::chainLevels( n ) : 1;
				for ( int l = 0; l < levels; l++ ) {
					const int m = std::max( n >> l, 1 );
					const size_t bytes = i < 2 ? static_cast<size_t>( m ) * m * m : static_cast<size_t>( m ) * m * mBpp;
					std::vector<unsigned char> x( bytes ), y( bytes );
					for ( int side = 0; side < 2; side++ ) {
						glBindTexture( tg, side ? y0 : own[i] );
						if ( i < 2 )
							glGetTexImage( tg, l, fRed, GL_UNSIGNED_BYTE, side ? y.data() : x.data() );
						else
							glGetTexImage( tg, 0, mPf, mTy, side ? y.data() : x.data() );
					}
					for ( size_t j = 0; j < bytes; j++ )
						d += x[j] != y[j];
					all += bytes;
				}
			}
			return glGetError() == GL_NO_ERROR;
		};
		// lane VKCLOUD11: the map on for this arm only (the job's mapRect has w 0: cfCoverMax returned the bands' cover and cvMap was
		// never read, so VKCLOUD10's stalemap / undrawnmap ran blind). The rect is the box's own plus one cell each side; the arm's GL
		// reference refI is the bake on the plain set with it, and it must differ from gv0 (w 0) in >= 50 texels.
		if ( !uni.count( "cfOrgKm" ) || uni.at( "cfOrgKm" ).size() < 2 || !uni.count( "cfDim" ) || !uni.count( "cfCellKm" ) )
			return fatal( "arm I's box uniforms" );
		UniMap uI = uni;
		const std::vector<double> org = uni.at( "cfOrgKm" );
		const double cKm = uni.at( "cfCellKm" )[0], span = uni.at( "cfDim" )[0] * cKm + 2.0 * cKm;
		uI["cvBands.mapRect"] = std::vector<double> { org[0] - cKm, org[1] - cKm, 1.0 / span, 1.0 };
		setUnis( pBake, uI );
		std::vector<std::uint8_t> refI;
		auto glBake = [&]( const InSet & z ) {
			use( z );
			GLuint t = vol3();
			const bool drew = glPass( 0, 0, t, 0, nz );
			const std::vector<std::uint8_t> a = read( t );
			glDeleteTextures( 1, &t );
			size_t d = drew && a.size() == refI.size() ? 0 : 1;
			for ( size_t i = 0; i < a.size() && i < refI.size(); i++ )
				d += a[i] != refI[i];
			return d;
		};
		struct IOut
		{
			bool ran = false;
			int copies = 0;
			unsigned in = 0;
		};
		auto iRun = [&]( const InSet & z ) {
			use( z );
			sentinel( 0 );
			IOut o;
			const int c0 = flyBakeJob().st.copies;
			flyBakeJob().st.inPlace = 0;
			o.ran = vkRun( 0, -1, 0, 0, nz, 0 );
			o.copies = flyBakeJob().st.copies - c0;
			o.in = flyBakeJob().st.inPlace;
			return o;
		};
		bool made = make( pl, false, 0, 0, true ) && make( shd, true, 0, 0, true );
		made = made && shd.s.shared && shd.d.shared && shd.m.shared && !pl.s.shared && !pl.d.shared && !pl.m.shared;
		{
			GLuint t = vol3();  // first on the fixture's own textures (tin[] still holds own[]): the plain set must match it
			const bool drewO = glPass( 0, 0, t, 0, nz );
			const std::vector<std::uint8_t> oI = read( t );
			use( pl );
			const bool drew = drewO && glPass( 0, 0, t, 0, nz );
			refI = read( t );
			glDeleteTextures( 1, &t );
			size_t d = 0, occ = 0, dO = oI.size() == refI.size() ? 0 : 1;
			for ( size_t i = 0; i < refI.size() && i < oI.size(); i++ )
				dO += refI[i] != oI[i];
			for ( size_t i = 0; i < refI.size() && i < ref[0].size(); i++ ) {
				d += refI[i] != ref[0][i];
				occ += refI[i] == 255;
			}
			const bool mapOn = made && drew && refI.size() == total && d >= 50 && occ > 0 && occ < total && dO == 0;
			std::printf( "FLYBAKEVIEW I map on (mapRect %.3f %.3f %.6g 1, the arm's own GL reference): GL's bake vs gv0 (mapRect w 0, cvMap never "
				"read): %zu of %zu texels differ (want >= 50), occupancy %zu; on the plain set vs on the fixture's own textures %zu differ (want 0): "
				"%s\n", org[0] - cKm, org[1] - cKm, 1.0 / span, d, total, occ, dO, mapOn ? "OK" : "FAIL" );
			ok = ok && mapOn;
		}
		size_t dP = 0, aP = 0, dS = 0, aS = 0;
		const bool readOk = sameLevels( pl, dP, aP ) && sameLevels( shd, dS, aS );
		const size_t gp = glBake( pl ), gs = glBake( shd );
		const bool fixture = made && readOk && aP > 0 && dP == 0 && dS == 0 && gp == 0 && gs == 0;
		std::printf( "FLYBAKEVIEW I fixture: shared made %d; every level vs the fixture's: plain %zu of %zu bytes differ, shared %zu of %zu; GL's bake "
			"vs gv0 on the plain set %zu texels differ, on the shared set %zu: %s\n", int( made ), dP, aP, dS, aS, gp, gs, fixture ? "EXACT" : "FAIL" );
		ok = ok && fixture;
		iRun( pl );  // warm
		const IOut p = iRun( pl );
		arm( "I plain (GL's own names) vs GL", judgeVol( 0, refI ), p.ran );
		const IOut s = iRun( shd );
		arm( "I shared (read in place) vs GL", judgeVol( 0, refI ), s.ran );
		const bool cOk = p.ran && s.ran && p.copies == 3 && s.copies == 0;
		std::printf( "FLYBAKEVIEW I copies a run: plain %d (want 3), shared %d (want 0): %s\n", p.copies, s.copies, cOk ? "OK" : "FAIL" );
		const bool named = p.in == 0 && s.in == all3;
		std::printf( "FLYBAKEVIEW I read in place: plain %s, shared %s (want none / cvShape cvDetail cvMap): %s\n", IS::inPlaceNames( p.in ).c_str(),
			IS::inPlaceNames( s.in ).c_str(), named ? "OK" : "FAIL" );
		ok = ok && cOk && named;
		const unsigned char full4[4] = { 255, 255, 255, 255 };
		const float one4[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		auto spoil = [&]() {
			for ( int l = 0; l < IS::chainLevels( nS ); l++ )
				clearTex( shd.s.tex, l, fRed, GL_UNSIGNED_BYTE, full4 );
			for ( int l = 0; l < IS::chainLevels( nD ); l++ )
				clearTex( shd.d.tex, l, fRed, GL_UNSIGNED_BYTE, full4 );
			clearTex( shd.m.tex, 0, mPf, mTy, mTy == GL_FLOAT ? static_cast<const void *>( one4 ) : static_cast<const void *>( full4 ) );
			glFinish();
			for ( int i = 0; i < 3; i++ )
				WwVkPresent::sharedTargetDrop( kIn + i );
		};
		spoil();
		const bool stale = IS::mapStale( shd.m, kIn + 2, true );
		const bool remake = make( shd, true, 0, 0, true ) && shd.s.shared && shd.d.shared && shd.m.shared;
		const IOut g = iRun( shd );
		const Res rg = judgeVol( 0, refI );
		const bool green = stale && remake && g.ran && rg.diff == 0 && rg.twice && g.copies == 0 && g.in == all3;
		std::printf( "FLYBAKEVIEW I remake after a drop: map stale %d, %zu of %zu texels differ, %d copies (want 0), read in place %s: %s\n", int( stale ),
			rg.diff, rg.total, g.copies, IS::inPlaceNames( g.in ).c_str(), green ? "OK" : "FAIL" );
		ok = ok && green;
		auto restore = [&]() {
			for ( int i = 0; i < 3; i++ )
				WwVkPresent::sharedTargetDrop( kIn + i );
			return make( shd, true, 0, 0, true );
		};
		{
			const bool m = make( un, true, IS::kRedUnshared, IS::kRedUnshared, true );
			const IOut r = iRun( un );
			const bool bit = m && ( !r.ran || r.copies != 0 || r.in != all3 );
			std::printf( "FLYBAKEVIEW red never shared (cloudinshare red 2): %s (%d copies, want 0; read in place %s; INFO S0 %zu texels differ)\n",
				bit ? "FAIL" : "EXACT", r.copies, IS::inPlaceNames( r.in ).c_str(), judgeVol( 0, refI ).diff );
			if ( !bit ) {
				std::printf( "FLYBAKEVIEW red never shared PASSED: the gate cannot see it\n" );
				ok = false;
			}
			for ( GLuint * t : { &un.s.tex, &un.d.tex, &un.m.tex } )
				glDeleteTextures( 1, t );
		}
		{
			spoil();
			const bool m = make( shd, true, IS::kRedStale, 0, true );
			const IOut r = iRun( shd );
			redArm( "stalenoise (a noise remake not filled)", judgeVol( 0, refI ), r.ran && m );
			ok = restore() && ok;
		}
		{
			spoil();
			const bool m = make( shd, true, 0, IS::kRedStale, true );
			const IOut r = iRun( shd );
			redArm( "stalemap (a map remake not attached to the map's FBO)", judgeVol( 0, refI ), r.ran && m );
			ok = restore() && ok;
		}
		{
			spoil();
			const bool m = make( shd, true, 0, 0, false );
			const IOut r = iRun( shd );
			redArm( "undrawnmap (a map remake not drawn again)", judgeVol( 0, refI ), r.ran && m );
			ok = restore() && ok;
		}
		for ( int k = 0; k < 3; k++ )
			tin[static_cast<size_t>( k )].gl = own[k];
		shapeGl = tin[0].gl;
		setUnis( pBake, uni );
		for ( int i = 0; i < 4; i++ )
			WwVkPresent::sharedTargetDrop( kIn + i );
		for ( GLuint * t : { &pl.s.tex, &pl.d.tex, &pl.m.tex } )
			glDeleteTextures( 1, t );
	}
	for ( char & k : inKeys )
		WwVkPresent::sharedTargetDrop( keyOf( k ) );
	{
		const WwVkPresent::FxStat st0 = WwVkPresent::takeFlyBakeStat();
		std::printf( "FLYBAKEVIEW stat: %d runs, %d failed (%s), %d pipelines built in %.1f ms, %d input copies\n", st0.runs, st0.fails,
			st0.failWhy.c_str(), st0.pipelines, st0.buildMs, st0.copies );
	}
	// ---- reds
	{
		sentinel( 0 );
		const bool ran = vkRun( 0, -1, 0, 0, nz, 1 );
		redArm( "stuck (every slice into slice 0)", judgeVol( 0, ref[0] ), ran );
	}
	{
		sentinel( 0 );
		const bool ran = vkRun( 0, -1, 0, 0, nz, 2 );
		redArm( "offby1 (slice k into slice k + 1)", judgeVol( 0, ref[0] ), ran );
	}
	{
		bool onVk = false, drew = false;
		const GLuint n = route( true, true, onVk, drew );
		const Res r = judgeVol( 0, ref[0] );
		const bool bit = n > 0;
		std::printf( "FLYBAKEVIEW red glwrites (the route lets GL draw after Vulkan): %s (GL_SAMPLES_PASSED %u, onVk %d; target %zu of %zu texels differ)\n",
			bit ? "FAIL" : "EXACT", n, onVk ? 1 : 0, r.diff, r.total );
		if ( !bit ) {
			std::printf( "FLYBAKEVIEW red glwrites PASSED: the gate cannot see it\n" );
			ok = false;
		}
	}
	{  // lane VKCLOUD7: the mip copy dropped (a twin of level 0 only, levels 1.. cleared to 0), on M; on the job's chain as INFO
		static char nomipKey;
		auto nomip = [&]( GLuint shape, const std::vector<std::uint8_t> & want, Res & r ) {
			GLuint tw = 0;
			if ( !WwVkPresent::sharedTarget( keyOf( nomipKey ), 1, tin[0].w, tin[0].h, tin[0].d, fR8, tw, why, texLevels[0] ) )
				return false;
			for ( int l = 0; l < texLevels[0]; l++ )
				clearTex( tw, l, fRed, GL_UNSIGNED_BYTE, &zero8 );
			f.copyImageSubData( shape, fT3D, 0, 0, 0, 0, tw, fT3D, 0, 0, 0, 0, tin[0].w, tin[0].h, tin[0].d );
			glFinish();
			shapeGl = shape;
			shapeShared = keyOf( nomipKey );
			sentinel( 0 );
			const bool ran = vkRun( 0, -1, 0, 0, nz, 0 );
			shapeShared = 0;
			shapeGl = tin[0].gl;
			r = judgeVol( 0, want );
			return ran;
		};
		Res r;
		const bool ran = nomip( shapeM, refM, r );
		const Res rz = judgeVol( 0, refZ );
		redArm( "nomip (cvShape's twin holds level 0 only, levels 1.. 0; on M)", r, ran );
		const bool zeroEq = ran && rz.twice && rz.diff == 0;
		std::printf( "FLYBAKEVIEW red nomip equals GL's all-zero-shape bake: %s (%zu of %zu texels differ)\n", zeroEq ? "yes" : "NO", rz.diff, rz.total );
		ok = ok && zeroEq;
		Res rj;
		const bool ranJ = nomip( tin[0].gl, ref[0], rj );
		std::printf( "FLYBAKEVIEW INFO nomip on the job's own chain (not judged): %zu of %zu texels differ, %zu unwritten%s\n", rj.diff, rj.total,
			rj.unwritten, ranJ ? "" : ", the run was refused" );
		WwVkPresent::sharedTargetDrop( keyOf( nomipKey ) );
	}
	{  // lane VKCLOUD9: perpass (red 16) must fail the crossing bar; oneset (red 32) must move texels
		const QOut qp = qRun( true, 16 );
		const bool bit = qp.ran && qp.crossings != 1;
		std::printf( "FLYBAKEVIEW red perpass (a crossing a pass): %s (%d crossings, %d runs, want 1 crossing; INFO texels differ S0 %zu S1 %zu S2 %zu)\n",
			bit ? "FAIL" : "EXACT", qp.crossings, qp.runs, qp.r[0].diff, qp.r[1].diff, qp.r[2].diff );
		if ( !bit ) {
			std::printf( "FLYBAKEVIEW red perpass PASSED: the gate cannot see it\n" );
			ok = false;
		}
		const QOut qs = qRun( true, 32 );
		Res r;
		r.twice = true;
		for ( const Res & x : qs.r ) {
			r.diff += x.diff;
			r.unwritten += x.unwritten;
			r.total += x.total;
			r.twice = r.twice && x.twice;
		}
		redArm( "oneset (every pass on pass 0's descriptor set and b31 slots; S0 S1 S2)", r, qs.ran );
	}
	{  // last: a pipeline of the other program
		sentinel( 0 );
		const bool ran = vkRun( 0, -1, 0, 0, nz, 4 );
		redArm( "swap (cloud_flysdf's pipeline, b31 zero but cfLayer)", judgeVol( 0, ref[0] ), ran );
	}
	{
		const WwVkPresent::FxStat st1 = WwVkPresent::takeFlyBakeStat();
		std::printf( "FLYBAKEVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f; includes the harness's hand-off wait)\n",
			st1.runs, st1.fails, st1.failWhy.c_str(), st1.runs ? st1.cpuMs / st1.runs : 0.0, st1.maxMs );
	}
	for ( char & k : S )
		WwVkPresent::sharedTargetDrop( keyOf( k ) );
	for ( char & k : twinKeys )
		WwVkPresent::sharedTargetDrop( keyOf( k ) );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "FLYBAKEVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
