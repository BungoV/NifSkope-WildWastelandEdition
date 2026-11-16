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
// Bar (set before the first run, never widened): every arm EXACT (0 texels differ), read twice equal, no GL error.
// Job checks (else the job cannot judge): 0 < occupancy < total, gv0 holds only 0 / 255, gv1..gv3 only 0..15, each pass
// changes its input, some slice differs from slice 0 and at least one adjacent slice pair differs.
// Reds (each must FAIL = texels differ on a fresh sentinel, read twice equal; printed "N of M texels differ, U unwritten",
// moved = differ - unwritten; stuck shows only as unwritten on this job: slice 15, drawn last into slice 0, equals slice 0):
//   stuck    every slice drawn into slice 0 (FlyBakeRun.red 1)
//   offby1   slice k drawn into slice k + 1 (red 2)
//   glwrites the route with redGlWrites (GL draws after Vulkan): GL_SAMPLES_PASSED > 0
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
	auto bindBake = [&]() {
		f.useProgram( pBake );
		for ( int k = 0; k < 3; k++ ) {
			bindUnit( k, tin[static_cast<size_t>( k )].target, tin[static_cast<size_t>( k )].gl );
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
	auto vkRun = [&]( int pass, int srcK, int dstK, int from, int to, int red ) -> bool {
		WwVkPresent::FlyBakeRun t;
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
			}
			WwVkPresent::fxPack( &ga, pBake, WwVkPresent::kT_flybake, int( std::size( WwVkPresent::kT_flybake ) ), t.loose );
		} else {
			u1i( pSdf, "cfAxis", pass - 1 );
			WwVkPresent::fxPack( &ga, pSdf, WwVkPresent::kT_flysdf, int( std::size( WwVkPresent::kT_flysdf ) ), t.loose );
		}
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
