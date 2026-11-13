// Lane VKLIGHT1 (2026-11-13, plan rows L1-L3): AO, contact lines and the SSGI cast through the shipped
// WwVkPresent::lightRun (vkpresent.cpp with WW_VKPRESENT_NO_QT, the app's own pipelines from the light_*.spv.h headers)
// against the GL references of tests/vklight/light_gl_ref.py, read back from GL's own textures (the copy back included).
// lightview <jobs dir> <red spv dir> <job> ...: each job's chain.txt must read EXACT or BOUND under lightmatch.cpp's rules;
// each red_<kernel>.txt (that kernel's pipeline swapped for <kernel>_red.spv) must FAIL. Last line LIGHTVIEW GATE OK / FAILED.
#define WW_VKPRESENT_NO_QT
#include "../../src/features/vkframe/vkpresent.cpp"

#include <GL/gl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace lv
{
HMODULE gOpenGl = nullptr;
void * glProc( const char * name )
{
	void * p = reinterpret_cast<void *>( wglGetProcAddress( name ) );
	const intptr_t i = reinterpret_cast<intptr_t>( p );
	if ( !p || i == 1 || i == 2 || i == 3 || i == -1 )
		p = reinterpret_cast<void *>( GetProcAddress( gOpenGl, name ) );
	return p;
}

LRESULT CALLBACK wndProc( HWND hw, UINT m, WPARAM wp, LPARAM lp )
{
	return DefWindowProcA( hw, m, wp, lp );
}

void pump()
{
	MSG m;
	while ( PeekMessageA( &m, nullptr, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &m );
		DispatchMessageA( &m );
	}
}

std::vector<std::uint8_t> readBytes( const std::string & path )
{
	std::ifstream f( path, std::ios::binary );
	return std::vector<std::uint8_t>( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
}

// lightmatch.cpp's formats as GL's: internal format, upload format / type, bytes a texel, half, byte
struct Fmt { const char * name; unsigned inner, fmt, type; int tb; bool half, byte; };
constexpr unsigned cRed = 0x1903, cRg = 0x8227, cRgba = 0x1908, cHalf = 0x140B, cFloat = 0x1406, cUbyte = 0x1401;
const Fmt kFmt[] = { { "rgba16f", 0x881A, cRgba, cHalf, 8, true, false }, { "rg16f", 0x822F, cRg, cHalf, 4, true, false },
	{ "r16f", 0x822D, cRed, cHalf, 2, true, false }, { "r32f", 0x822E, cRed, cFloat, 4, false, false },
	{ "rg32f", 0x8230, cRg, cFloat, 8, false, false }, { "rgba32f", 0x8814, cRgba, cFloat, 16, false, false },
	{ "r8", 0x8229, cRed, cUbyte, 1, false, true } };

int ulps16( std::uint16_t a, std::uint16_t b )
{
	auto ord = []( std::uint16_t x ) { return ( x & 0x8000 ) ? -int( x & 0x7FFF ) : int( x ); };
	auto nan = []( std::uint16_t x ) { return ( x & 0x7C00 ) == 0x7C00 && ( x & 0x03FF ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) == nan( b ) ? 0 : 1 << 20;
	return std::abs( ord( a ) - ord( b ) );
}

int ulps32( std::uint32_t a, std::uint32_t b )
{
	auto ord = []( std::uint32_t x ) { return ( x & 0x80000000u ) ? -std::int64_t( x & 0x7FFFFFFFu ) : std::int64_t( x ); };
	auto nan = []( std::uint32_t x ) { return ( x & 0x7F800000u ) == 0x7F800000u && ( x & 0x007FFFFFu ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) == nan( b ) ? 0 : 1 << 30;
	const std::int64_t d = ord( a ) - ord( b );
	return int( std::min<std::int64_t>( d < 0 ? -d : d, 1 << 30 ) );
}

int kernelOf( std::string spv )
{
	if ( spv.size() > 4 && spv.compare( spv.size() - 4, 4, ".spv" ) == 0 )
		spv.resize( spv.size() - 4 );
	if ( spv.size() > 4 && spv.compare( spv.size() - 4, 4, "_red" ) == 0 )
		spv.resize( spv.size() - 4 );
	static const std::map<std::string, int> k = { { "ao_gbuf", WwVkPresent::LightAoGbuf }, { "ao_mip", WwVkPresent::LightAoMip },
		{ "ao_raw", WwVkPresent::LightAoRaw }, { "ao_blur", WwVkPresent::LightAoBlur }, { "ao_blurf", WwVkPresent::LightAoBlurF },
		{ "contact", WwVkPresent::LightContactK }, { "ssgi", WwVkPresent::LightSsgiK } };
	const auto i = k.find( spv );
	return i == k.end() ? -1 : i->second;
}

struct Img { std::string name, file; const Fmt * f = nullptr; int w = 0, h = 0, lv = 1; GLuint tex = 0; };
struct Cmp { std::string img, file, rule; };

// one chain through lightRun; red = the kernel swapped for its red build. Returns 0 green, 1 out of bound, 2 broken
int runChain( const std::string & jobs, const std::string & job, const std::string & chainName, const std::string & spvDir )
{
	const std::string dir = jobs + "/" + job + "/", name = job + "/" + chainName;
	auto broken = [&]( const std::string & w ) {
		std::printf( "LIGHTVIEW %s BROKEN (%s)\n", name.c_str(), w.c_str() );
		return 2;
	};
	const std::vector<std::uint8_t> txt = readBytes( dir + chainName );
	if ( txt.empty() )
		return broken( "chain file" );
	std::vector<Img> img;
	std::map<std::string, int> at;
	WwVkPresent::LightRun t;
	std::vector<Cmp> cmps;
	std::string bufFile, redSpv;
	int redKernel = -1;
	std::istringstream in( std::string( txt.begin(), txt.end() ) );
	std::string line;
	auto idx = [&]( const std::string & n ) { return n == "-" || !at.count( n ) ? -1 : at[n]; };
	while ( std::getline( in, line ) ) {
		std::istringstream l( line );
		std::string k;
		l >> k;
		if ( k == "img" ) {
			Img x;
			std::string f;
			l >> x.name >> f >> x.w >> x.h >> x.file;
			for ( const Fmt & y : kFmt )
				if ( f == y.name )
					x.f = &y;
			if ( !( l >> x.lv ) || x.lv < 1 )
				x.lv = 1;
			if ( !x.f || x.w < 1 || x.h < 1 )
				return broken( "img line: " + line );
			at[x.name] = int( img.size() );
			img.push_back( x );
		} else if ( k == "pass" ) {
			std::string label, spv, pc, o0, o1, s[5];
			WwVkPresent::LightDispatch d;
			l >> label >> spv >> pc >> o0 >> o1;
			for ( std::string & x : s )
				l >> x;
			l >> d.gx >> d.gy;
			d.kernel = kernelOf( spv );
			if ( d.kernel < 0 )
				return broken( "no kernel for " + spv );
			if ( spv.find( "_red.spv" ) != std::string::npos ) {
				redKernel = d.kernel;
				redSpv = spv;
			}
			const std::vector<std::uint8_t> p = readBytes( dir + pc );
			if ( p.size() != 128 )
				return broken( pc + " is not 128 bytes" );
			std::memcpy( d.pc, p.data(), 128 );
			d.out0 = idx( o0 );
			d.out1 = idx( o1 );
			for ( int i = 0; i < 5; i++ )
				d.t[i] = idx( s[i] );
			t.d.push_back( d );
		} else if ( k == "buf" ) {
			l >> bufFile;
		} else if ( k == "cmp" ) {
			Cmp c;
			l >> c.img >> c.file >> c.rule;
			cmps.push_back( c );
		}
	}
	if ( t.d.empty() || cmps.empty() )
		return broken( "no pass or no compare" );
	const int k0 = t.d[0].kernel;
	t.pass = k0 <= WwVkPresent::LightAoBlurF ? WwVkPresent::LightAo : k0 == WwVkPresent::LightContactK ? WwVkPresent::LightContact
		: WwVkPresent::LightSsgi;
	if ( !bufFile.empty() )
		t.buf = readBytes( dir + bufFile );
	// GL's own textures: the inputs uploaded level by level (int32 W, H, then the values), every image a GL name
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	for ( Img & x : img ) {
		glGenTextures( 1, &x.tex );
		glBindTexture( GL_TEXTURE_2D, x.tex );
		glTexParameteri( GL_TEXTURE_2D, 0x813D, x.lv - 1 );  // GL_TEXTURE_MAX_LEVEL
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, x.lv > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		std::vector<std::uint8_t> f;
		if ( x.file != "-" ) {
			f = readBytes( jobs + "/" + x.file );
			if ( f.empty() )
				return broken( "input " + x.file );
		}
		size_t p = 0;
		for ( int m = 0; m < x.lv; m++ ) {
			const int mw = std::max( x.w >> m, 1 ), mh = std::max( x.h >> m, 1 );
			const size_t want = size_t( mw ) * size_t( mh ) * size_t( x.f->tb );
			const void * data = nullptr;
			if ( !f.empty() ) {
				if ( f.size() < p + 8 + want || reinterpret_cast<const std::int32_t *>( f.data() + p )[0] != mw
					|| reinterpret_cast<const std::int32_t *>( f.data() + p )[1] != mh )
					return broken( "input " + x.file + " level " + std::to_string( m ) + " size" );
				data = f.data() + p + 8;
				p += 8 + want;
			}
			glTexImage2D( GL_TEXTURE_2D, m, GLint( x.f->inner ), mw, mh, 0, x.f->fmt, x.f->type, data );
		}
		if ( !f.empty() && p != f.size() )
			return broken( "input " + x.file + " bytes left over" );
		WwVkPresent::LightImage li;
		li.tex = x.tex;
		li.glFmt = x.f->inner;
		li.w = x.w;
		li.h = x.h;
		li.levels = x.lv;
		li.in = x.file != "-";
		for ( const Cmp & c : cmps )
			li.out = li.out || c.img == x.name;
		t.img.push_back( li );
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
	if ( const GLenum e = glGetError() )
		return broken( "GL error " + std::to_string( e ) + " making the inputs" );
	// a red: that kernel's pipeline swapped for the red build for this run only
	Vk & v = vk();
	Light & L = light();
	VkPipeline keep = VK_NULL_HANDLE, red = VK_NULL_HANDLE;
	if ( redKernel >= 0 ) {
		if ( !L.made || !L.pipe[redKernel] )
			return broken( "a red before its green made the pipelines" );
		const std::vector<std::uint8_t> code = readBytes( spvDir + "/" + redSpv );
		if ( code.empty() || code.size() % 4 )
			return broken( "red spv " + redSpv );
		VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mci.codeSize = code.size();
		mci.pCode = reinterpret_cast<const std::uint32_t *>( code.data() );
		VkShaderModule mod = VK_NULL_HANDLE;
		if ( v.vkCreateShaderModule( v.dev, &mci, nullptr, &mod ) != VK_SUCCESS )
			return broken( "red shader module" );
		VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		cp.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		cp.stage.module = mod;
		cp.stage.pName = "main";
		cp.layout = L.layout;
		const VkResult r = v.vkCreateComputePipelines( v.dev, VK_NULL_HANDLE, 1, &cp, nullptr, &red );
		v.vkDestroyShaderModule( v.dev, mod, nullptr );
		if ( r != VK_SUCCESS )
			return broken( "red pipeline" );
		keep = L.pipe[redKernel];
		L.pipe[redKernel] = red;
	}
	std::string why;
	const bool ran = WwVkPresent::lightRun( t, why );
	if ( red ) {
		LightPassState & P = L.p[t.pass];
		if ( P.pending )
			v.vkWaitForFences( v.dev, 1, &P.fence, VK_TRUE, 5000000000ull );
		L.pipe[redKernel] = keep;
		v.vkDestroyPipeline( v.dev, red, nullptr );
	}
	auto drop = [&]() {
		for ( Img & x : img )
			glDeleteTextures( 1, &x.tex );
	};
	if ( !ran ) {
		drop();
		return broken( "lightRun: " + why );
	}
	// GL's textures read back and held against GL's reference (lightmatch.cpp's rules, unchanged)
	bool ok = true, exact = true;
	for ( const Cmp & c : cmps ) {
		const Img & x = img[size_t( at[c.img] )];
		const std::vector<std::uint8_t> g = readBytes( dir + c.file );
		const size_t bytes = size_t( x.w ) * size_t( x.h ) * size_t( x.f->tb );
		if ( g.size() != bytes + 8 ) {
			std::printf( "%s: GL file missing or wrong size\n", c.file.c_str() );
			ok = false;
			continue;
		}
		std::vector<std::uint8_t> rb( bytes );
		glBindTexture( GL_TEXTURE_2D, x.tex );
		glGetTexImage( GL_TEXTURE_2D, 0, x.f->fmt, x.f->type, rb.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		const bool h16 = x.f->half, b8 = x.f->byte;
		const size_t es = b8 ? 1 : h16 ? 2 : 4, n = bytes / es;
		const std::uint8_t * v8 = rb.data(), * g8 = g.data() + 8;
		const std::uint16_t * v16 = reinterpret_cast<const std::uint16_t *>( rb.data() );
		const std::uint16_t * g16 = reinterpret_cast<const std::uint16_t *>( g.data() + 8 );
		const std::uint32_t * v32 = reinterpret_cast<const std::uint32_t *>( rb.data() );
		const std::uint32_t * g32 = reinterpret_cast<const std::uint32_t *>( g.data() + 8 );
		const int tol = c.rule.compare( 0, 3, "ulp" ) == 0 ? std::max( std::atoi( c.rule.c_str() + 3 ), 0 ) : 1;
		const bool absRule = !h16 && c.rule.compare( 0, 3, "abs" ) == 0;
		const double absTol = absRule ? std::atof( c.rule.c_str() + 3 ) : 0.0;
		double maxAbs = 0.0;
		bool nanBad = false;
		size_t d = 0, big = 0;
		int mu = 0;
		for ( size_t k = 0; k < n; k++ ) {
			if ( absRule ) {
				float fa, fb;
				std::memcpy( &fa, &v32[k], 4 );
				std::memcpy( &fb, &g32[k], 4 );
				if ( ( fa != fa ) != ( fb != fb ) )
					nanBad = true;
				else if ( fa == fa )
					maxAbs = std::max( maxAbs, std::fabs( double( fa ) - double( fb ) ) );
			}
			const int u = b8 ? std::abs( int( v8[k] ) - int( g8[k] ) ) : h16 ? ulps16( v16[k], g16[k] ) : ulps32( v32[k], g32[k] );
			d += u != 0;
			big += u > tol;
			mu = std::max( mu, u );
		}
		const bool pass = mu <= tol || ( c.rule == "step" && d * 200 <= n && big * 1000 <= n ) || ( c.rule == "gather" && big * 10000 <= n )
			|| ( c.rule == "sector" && d * 100 <= n && big * 500 <= n ) || ( absRule && !nanBad && maxAbs <= absTol && d * 50 <= n );
		exact = exact && d == 0;
		ok = ok && pass;
		std::printf( "  %s %dx%d [%s]: %zu of %zu values differ, %zu by > tol, max %d ulp, max abs %.3g%s\n", c.img.c_str(), x.w, x.h,
			c.rule.c_str(), d, n, big, mu, maxAbs, pass ? "" : " OUT" );
	}
	drop();
	std::printf( "LIGHTVIEW %s %s\n", name.c_str(), !ok ? "FAIL" : exact ? "EXACT" : "BOUND" );
	return ok ? 0 : 1;
}
}  // namespace lv

int main( int argc, char ** argv )
{
	using namespace lv;
	if ( argc < 4 ) {
		std::printf( "usage: lightview <jobs dir> <red spv dir> <job> ...\nLIGHTVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string jobs = argv[1], spvDir = argv[2];
	const int W = 640, H = 360, S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwlightview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwlightview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwlightview", "light view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf ) {
		std::printf( "LIGHTVIEW GATE FAILED (no 4x MSAA pixel format)\n" );
		return 2;
	}
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
	std::printf( "GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) ) {
		std::printf( "LIGHTVIEW GATE FAILED (the view: %s)\n", why.c_str() );
		return 2;
	}
	std::printf( "present device: shaderFloat64 %d, storage formats %d\n", int( vk().float64 ), int( vk().storageExt ) );
	bool ok = true;
	int greens = 0, reds = 0;
	for ( int a = 3; a < argc; a++ ) {
		const std::string job = argv[a];
		pump();
		const int g = runChain( jobs, job, "chain.txt", spvDir );
		ok = ok && g == 0;
		greens++;
		WIN32_FIND_DATAA fd;
		HANDLE fh = FindFirstFileA( ( jobs + "/" + job + "/red_*.txt" ).c_str(), &fd );
		if ( fh == INVALID_HANDLE_VALUE )
			continue;
		do {
			const int r = runChain( jobs, job, fd.cFileName, spvDir );
			if ( r != 1 ) {
				std::printf( "RED %s/%s DID NOT FAIL\n", job.c_str(), fd.cFileName );
				ok = false;
			}
			reds++;
		} while ( FindNextFileA( fh, &fd ) );
		FindClose( fh );
	}
	for ( int p = 0; p < WwVkPresent::LightPasses; p++ ) {
		const WwVkPresent::LightStat st = WwVkPresent::takeLightStat( p );
		std::printf( "LIGHTVIEW stat pass %d: %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in "
			"%.1f ms\n", p, st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
	}
	std::printf( "greens %d, reds %d\n", greens, reds );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "LIGHTVIEW GATE %s\n", ok ? "OK" : "FAILED" );
	return ok ? 0 : 1;
}
