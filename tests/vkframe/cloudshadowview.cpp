// Lane VKCLOUD3 (2026-11-16): the cloud shadow map's build on the present device (vkcloudshadow.inc cloudShadowRun: one
// render pass over the shared RGBA16F n x n map, LOAD, the slice's rows scissored) against GL's own cloud_shadowmap program
// in cloudshadow.cpp's order (the map's fbo, viewport n x n, scissor ( 0, row0, n, rows ), one full-screen draw), on the
// same inputs: tests/vklight/frag_jobs.py's cloudshadow jobs (vol mode 1, band mode 3 no sheets, beer mode 2, sheets mode 3
// with 2 sheets + blend targets, mipmapped repeat as the app's). fxview.cpp's helpers are reused (its main renamed).
// Both maps start from the same seed colour, so the rows a slice does not write must keep it on both sides.
// Jobs: the 4 full builds + `sheets/slice` (rows 3n/16 .. 4n/16, the PERFFIX3 amortised back build's size).
// Bar (set before the first run, never widened), per job, RGBA16F halves:
//   in the slice: max <= 2 ulp and <= 0.1 % of the slice's halves past 1 ulp; GL's alpha non-zero on >= 1/20 of the
//   slice's texels (the job reaches the cloud); outside the slice: every half EXACT (the seed kept).
//   EXACT = nothing differs; BOUND = inside the bar; anything else FAILS.
// Reds (each must FAIL): red 1 the sheet arrays swapped (csSheetTex <-> csSheetTex2) on `sheets`, red 2 the render pass
// CLEARs instead of LOADs on `sheets/slice`, red 4 the b31 csS field shifted 4 bytes on `sheets`.
// Last line `CLOUDSHADOWVIEW GATE OK` or `CLOUDSHADOWVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

namespace csv
{
using namespace fv;
const char * kSlot[5] = { "cvShape", "cvDetail", "cvMap", "csSheetTex", "csSheetTex2" };
constexpr int kSheets = 8;
constexpr float kSeed[4] = { 0.25f, 0.5f, 0.75f, 1.0f };

struct Verdict
{
	size_t in = 0, diff = 0, past1 = 0, outDiff = 0, lit = 0, texels = 0;
	int maxu = 0;
	bool ok() const { return in > 0 && outDiff == 0 && maxu <= 2 && past1 * 1000 <= in && lit * 20 >= texels; }
	const char * word() const { return in == 0 ? "FAIL" : diff == 0 && outDiff == 0 && lit * 20 >= texels ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

Verdict judge( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & ref, int n, int row0, int rows )
{
	Verdict v;
	if ( a.size() != ref.size() || a.size() != size_t( n ) * n * 4 )
		return v;
	for ( int y = 0; y < n; y++ ) {
		const bool inside = y >= row0 && y < row0 + rows;
		for ( int x = 0; x < n; x++ ) {
			const size_t p = ( size_t( y ) * n + x ) * 4;
			if ( inside ) {
				v.texels++;
				v.lit += h2f( ref[p + 3] ) != 0.0f;
			}
			for ( int c = 0; c < 4; c++ ) {
				const int u = std::abs( ordered( a[p + c] ) - ordered( ref[p + c] ) );
				if ( !inside ) {
					v.outDiff += u != 0;
					continue;
				}
				v.in++;
				v.diff += u != 0;
				v.past1 += u > 1;
				v.maxu = std::max( v.maxu, u );
			}
		}
	}
	return v;
}
}  // namespace csv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace csv;
	gTag = "CLOUDSHADOWVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: cloudshadowview <prep dir>\nCLOUDSHADOWVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "CLOUDSHADOWVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwcloudshadowview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwcloudshadowview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwcloudshadowview", "cloud shadow view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	if ( !loadAll() )
		return fatal( "GL entry points" );
	std::printf( "CLOUDSHADOWVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	// GL draws cloudshadow.cpp's quad( r ); the full-screen triangle covers the same texels (the program reads gl_FragCoord)
	const GLuint pCs = program( kTriVs, readFile( dir + "/prog.frag.glsl" ), "cloud_shadowmap" );
	if ( !pCs )
		return fatal( "GL program" );
	std::vector<std::string> names;
	{
		std::istringstream in( readFile( dir + "/jobs.txt" ) );
		std::string s;
		while ( in >> s )
			names.push_back( s );
	}
	if ( names.size() != 4 )
		return fatal( "jobs.txt" );
	std::vector<int> jv;
	if ( !readJob( dir + "/" + names[0] + "/job.txt", jv, 2 ) || jv[0] != jv[1] )
		return fatal( "job.txt" );
	const int n = jv[0];
	std::string why;
	if ( !WwVkPresent::viewBegin( n, n, S, glProc, why ) )
		return fatal( "the view: " + why );
	// the maps: GL's own (the reference) and the shared one the Vulkan run writes (the app's g.tex as a sharedTarget)
	const GLuint glMap = tex2d( n, n, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, true );
	const GLuint fbGl = fbo( GL_TEXTURE_2D, glMap, 0, 0 );
	static char mapKey;
	GLuint vkMap = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &mapKey ), 0, n, n, 1, fRgba16f, vkMap, why ) )
		return fatal( "the shared map: " + why );
	const GLuint fbVk = fbo( GL_TEXTURE_2D, vkMap, 0, 0 );
	if ( !fbGl || !fbVk )
		return fatal( "GL targets" );
	auto seed = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, n, n );
		drawState0();
		glClearColor( kSeed[0], kSeed[1], kSeed[2], kSeed[3] );
		glClear( GL_COLOR_BUFFER_BIT );
	};
	auto readMap = [&]( GLuint fb ) {
		std::vector<std::uint16_t> out( size_t( n ) * n * 4 );
		f.bindFramebuffer( fFb, fb );
		glReadPixels( 0, 0, n, n, GL_RGBA, fHalf, out.data() );
		return out;
	};
	static char twinKeys[5][kSheets];

	struct Job
	{
		std::string name;
		UniMap uni;
		std::vector<TexIn> tex;
	};
	std::vector<Job> jobs;
	for ( const std::string & nm : names ) {
		Job j;
		j.name = nm;
		j.uni = readUni( dir + "/" + nm + "/uni.txt" );
		j.tex = readTex( dir + "/" + nm );
		if ( j.tex.size() != 3 + 2 * kSheets )
			return fatal( "tex.txt of " + nm );
		for ( TexIn & t : j.tex )
			if ( !upload( dir + "/" + nm, t, 0 ) )
				return fatal( "upload " + nm + " " + t.name );
		jobs.push_back( j );
	}
	auto bindJob = [&]( const Job & j ) {
		f.useProgram( pCs );
		setUnis( pCs, j.uni );
		for ( size_t k = 0; k < j.tex.size(); k++ ) {
			const TexIn & t = j.tex[k];
			bindUnit( int( k ), t.target, t.gl );
			u1i( pCs, t.name.c_str(), int( k ) );
		}
	};
	auto runGl = [&]( const Job & j, int row0, int rows ) {
		seed( fbGl );
		bindJob( j );
		f.bindFramebuffer( fFb, fbGl );
		glViewport( 0, 0, n, n );
		glEnable( GL_SCISSOR_TEST );
		glScissor( 0, row0, n, rows );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		return readMap( fbGl );
	};
	auto runVk = [&]( const Job & j, int row0, int rows, int red, std::vector<std::uint16_t> & out ) -> bool {
		seed( fbVk );
		bindJob( j );
		WwVkPresent::CloudShadowRun t;
		t.n = n;
		t.row0 = row0;
		t.rows = rows;
		t.key = reinterpret_cast<std::uintptr_t>( &mapKey );
		for ( int k = 0; k < 3; k++ ) {
			if ( !WwVkPresent::fxTexOf( &ga, pCs, kSlot[k], k < 2 ? 1 : 0, t.tex[k], why ) ) {
				std::printf( "CLOUDSHADOWVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[k][0] );
		}
		for ( int a = 0; a < 2; a++ )
			for ( int i = 0; i < kSheets; i++ ) {
				const std::string nm = std::string( kSlot[3 + a] ) + "[" + std::to_string( i ) + "]";
				WwVkPresent::FxTex & x = a ? t.sheet2[i] : t.sheet[i];
				if ( !WwVkPresent::fxTexOf( &ga, pCs, nm.c_str(), 0, x, why ) ) {
					std::printf( "CLOUDSHADOWVIEW fxTexOf refused: %s\n", why.c_str() );
					return false;
				}
				x.twin = reinterpret_cast<std::uintptr_t>( &twinKeys[3 + a][i] );
			}
		WwVkPresent::fxPack( &ga, pCs, WwVkPresent::kT_cloudshadow, int( std::size( WwVkPresent::kT_cloudshadow ) ), t.loose );
		t.red = red;
		if ( !WwVkPresent::cloudShadowRun( t, why ) ) {
			std::printf( "CLOUDSHADOWVIEW cloudShadowRun refused: %s\n", why.c_str() );
			return false;
		}
		out = readMap( fbVk );
		return glGetError() == GL_NO_ERROR;
	};

	const struct
	{
		int job, row0, rows;
		const char * name;
	} runs[5] = { { 0, 0, n, "" }, { 1, 0, n, "" }, { 2, 0, n, "" }, { 3, 0, n, "" }, { 3, 3 * n / 16, n / 16, "/slice" } };
	bool ok = true;
	std::vector<std::uint16_t> refFull, refSlice;
	for ( int ri = 0; ri < 5; ri++ ) {
		const auto & r = runs[ri];
		const Job & j = jobs[size_t( r.job )];
		const std::vector<std::uint16_t> ref = runGl( j, r.row0, r.rows );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the GL reference of " + j.name + r.name );
		if ( ri == 3 )
			refFull = ref;
		if ( ri == 4 )
			refSlice = ref;
		std::vector<std::uint16_t> a;
		if ( !runVk( j, r.row0, r.rows, 0, a ) ) {
			std::printf( "CLOUDSHADOWVIEW %s%s FAIL (the Vulkan run failed)\n", j.name.c_str(), r.name );
			ok = false;
			continue;
		}
		const Verdict v = judge( a, ref, n, r.row0, r.rows );
		std::printf( "CLOUDSHADOWVIEW %s%s %s (rows %d..%d of %d: %zu halves, %zu differ, %zu past 1 ulp, max %d ulp; %zu of %zu texels lit; "
			"%zu halves outside the slice differ)\n", j.name.c_str(), r.name, v.word(), r.row0, r.row0 + r.rows, n, v.in, v.diff, v.past1, v.maxu,
			v.lit, v.texels, v.outDiff );
		ok = ok && ( v.diff == 0 || v.ok() ) && v.outDiff == 0 && v.lit * 20 >= v.texels;
	}
	{
		const WwVkPresent::FxStat st = WwVkPresent::takeCloudShadowStat();
		std::printf( "CLOUDSHADOWVIEW stat: %d runs, %d pipelines built in %.1f ms, %d input copies\n", st.runs, st.pipelines, st.buildMs, st.copies );
	}
	const struct
	{
		const char * name;
		int red, run;
	} reds[3] = { { "sheet arrays swapped", 1, 3 }, { "LOAD turned CLEAR on a slice", 2, 4 }, { "b31 csS shifted 4 bytes", 4, 3 } };
	for ( const auto & rd : reds ) {
		const auto & r = runs[rd.run];
		const Job & j = jobs[size_t( r.job )];
		std::vector<std::uint16_t> a;
		if ( !runVk( j, r.row0, r.rows, rd.red, a ) ) {
			std::printf( "CLOUDSHADOWVIEW red %s: the run failed (counts as a FAIL)\n", rd.name );
			continue;
		}
		const Verdict v = judge( a, rd.run == 4 ? refSlice : refFull, n, r.row0, r.rows );
		std::printf( "CLOUDSHADOWVIEW red %s: %s (%zu differ, %zu past 1 ulp, max %d ulp, %zu outside differ)\n", rd.name, v.word(), v.diff,
			v.past1, v.maxu, v.outDiff );
		if ( v.ok() || ( v.diff == 0 && v.outDiff == 0 ) ) {
			std::printf( "CLOUDSHADOWVIEW red %s PASSED: the gate cannot see it\n", rd.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeCloudShadowStat();
	std::printf( "CLOUDSHADOWVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f)\n", st.runs, st.fails,
		st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &mapKey ) );
	for ( auto & row : twinKeys )
		for ( char & k : row )
			WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "CLOUDSHADOWVIEW GATE OK" : "CLOUDSHADOWVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
