// Lane VKFAR2 (2026-11-14, row P4b): the fog bloom's own pass on the present device (vkfogbloom.inc fogBloomRun: one
// render pass, the tone frame's depth sample and stencil read in fogbloom_view_ps) against GL's own cell_fogbloom program
// in cellfogbloom.cpp's order (the frame's depth blitted NEAREST into a one-sample texture, the R8 target beside the
// frame's depth / stencil: clear 0, draw 1 stencil off fbTranslucent 0, draw 2 stencil EQUAL 0x04 mask 0x04
// fbTranslucent 1), per sample, on the same shared 4x MSAA tone frame. fxview.cpp's helpers are reused (its main renamed).
// The frame: per job a depth and stencil drawn per sample from a hash seeded by the job's name (FNV-1a): 12 px blocks of
// one depth 20..300000 units, 15 % sky (depth 1), one pixel in 4 an edge whose samples take their own depths; stencil
// 0x04 on ~30 % of the samples, 0x03 on ~50 % (bits the pass must ignore).
// Sweep: 4 jobs (seed + fbParams window; 8 draws), the app's window among them.
// Bar (set before the first run, never widened), per job on the R8 levels of every sample:
//   lit = samples non-zero on either side; past1 = samples more than 1 level apart; one = samples exactly 1 level apart.
//   EXACT = no sample differs; BOUND = past1 <= 0.01 % of lit and one <= 0.5 % of lit. Anything else FAILS.
//   The gate also needs >= 1000 lit samples over the sweep.
// Reds (job 0, each must FAIL): red 1 no second draw, red 2 the first draw with fbTranslucent 1, red 4 fbProj.y doubled
// (a first red 4, fbProj.y negated, read EXACT: the air's uv comes from the pixel and posView only gives the distance).
// Last line `FOGBLOOMVIEW GATE OK` or `FOGBLOOMVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

namespace fbv
{
using namespace fv;
void( WINAPI * blitFramebuffer )( GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum ) = nullptr;
constexpr GLenum fReadFb = 0x8CA8, fDrawFb = 0x8CA9, fDepthStencil = 0x84F9, fU248 = 0x84FA;

// the frame's depth (mode 0) and stencil marks (modes 1, 2), per sample; gl_SampleID makes it run per sample
const char * kFrameFs = "#version 410 core\n"
	"uniform uint seed; uniform vec2 pz; uniform int mode; out vec4 o;\n"
	"uint hsh( uint x ) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }\n"
	"float u01( uint x ) { return float( hsh( x ) >> 8 ) * ( 1.0 / 16777216.0 ); }\n"
	"void main() {\n"
	"  uvec2 p = uvec2( gl_FragCoord.xy ); uint s = uint( gl_SampleID );\n"
	"  uint px = hsh( seed ^ ( p.y * 4099u + p.x ) );\n"
	"  float h = u01( seed ^ 0x9e3779b9u ^ ( ( p.y / 12u ) * 977u + p.x / 12u ) );\n"
	"  bool edge = ( px & 3u ) == 0u;\n"
	"  float hs = edge ? u01( px ^ ( ( s + 1u ) * 0x85ebca6bu ) ) : h;\n"
	"  o = vec4( 0.2, 0.3, 0.4, 1.0 );\n"
	"  if ( mode == 0 ) {\n"
	"    if ( hs > 0.85 ) discard;\n"
	"    float d = 20.0 * pow( 15000.0, hs / 0.85 );\n"
	"    gl_FragDepth = clamp( ( pz.y - pz.x * d ) / d * 0.5 + 0.5, 0.0, 0.99999 );\n"
	"  } else if ( u01( px ^ ( ( s + 7u ) * 0x27d4eb2fu ) ^ uint( mode ) * 0x165667b1u ) > ( mode == 1 ? 0.3 : 0.5 ) ) {\n"
	"    discard;\n"
	"  }\n"
	"}\n";

std::uint32_t fnv1a( const std::string & s )
{
	std::uint32_t h = 2166136261u;
	for ( unsigned char c : s ) {
		h ^= c;
		h *= 16777619u;
	}
	return h;
}

struct Verdict
{
	size_t lit = 0, one = 0, past1 = 0, diff = 0, edgePast = 0;
	int maxd = 0;
	bool ok() const { return lit > 0 && past1 * 10000 <= lit && one * 200 <= lit; }
	const char * word() const { return lit == 0 && diff == 0 ? "EXACT (all 0)" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

int level( std::uint16_t hv )
{
	return int( std::lround( h2f( hv ) * 255.0f ) );
}

Verdict judge( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b, size_t ws )
{
	Verdict v;
	if ( a.size() != b.size() || a.empty() )
		return v;
	for ( size_t s = 0; s < a.size() / 4; s++ ) {
		const int la = level( a[s * 4] ), lb = level( b[s * 4] );
		if ( la == 0 && lb == 0 )
			continue;
		v.lit++;
		const int d = std::abs( la - lb );
		v.diff += d != 0;
		v.one += d == 1;
		v.past1 += d > 1;
		v.maxd = std::max( v.maxd, d );
		(void)ws;
	}
	return v;
}
}  // namespace fbv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fbv;
	gTag = "FOGBLOOMVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: fogbloomview <prep dir>\nFOGBLOOMVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = std::string( argv[1] ) + "/fogbloom";
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FOGBLOOMVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfogbloomview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfogbloomview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfogbloomview", "fog bloom view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	if ( !loadAll() || !load( blitFramebuffer, "glBlitFramebuffer" ) )
		return fatal( "GL entry points" );
	std::printf( "FOGBLOOMVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pFrame = program( kTriVs, kFrameFs, "frame" );
	// GL draws cell_ao.vert's quad; the full-screen triangle covers the same samples (the program reads gl_FragCoord only)
	const GLuint pFog = program( kTriVs, readFile( dir + "/prog.frag.glsl" ), "cell_fogbloom" );
	if ( !pUnpack || !pFrame || !pFog )
		return fatal( "GL programs" );
	std::vector<int> jv;
	if ( !readJob( dir + "/job.txt", jv, 2 ) )
		return fatal( "job.txt" );
	const int W = jv[0], H = jv[1], WS = W * S;
	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint refR8 = msTex( W, H, fR8, S );
	const GLuint fbRef = fbo( fT2DMS, refR8, 0, fds );
	const GLuint unp = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0, 0 );
	const GLuint depth1 = tex2d( W, H, fD24s8, fDepthStencil, fU248, nullptr, false );
	GLuint fbDepth1 = 0;
	f.genFramebuffers( 1, &fbDepth1 );
	f.bindFramebuffer( fFb, fbDepth1 );
	f.framebufferTexture2D( fFb, fDsAtt, GL_TEXTURE_2D, depth1, 0 );
	glDrawBuffer( GL_NONE );
	glReadBuffer( GL_NONE );
	if ( !fbFrame || !fbRef || !fbUnp || f.checkFramebufferStatus( fFb ) != fComplete )
		return fatal( "GL targets" );

	// ---- the program's inputs: the air's uniforms + textures (the volume in a shared target, as the app's)
	const char * slot[4] = { "volTex", "volTile", "volAirTerr", "volAirRainMap" };
	const int slotType[4] = { 1, 0, 2, 0 };
	const UniMap uni0 = readUni( dir + "/uni.txt" );
	std::vector<TexIn> tex = readTex( dir );
	if ( tex.size() != 4 )
		return fatal( "tex.txt" );
	static char twinKeys[4], volKey;
	GLuint volShared = 0;
	for ( int k = 0; k < 4; k++ ) {
		TexIn & t = tex[size_t( k )];
		if ( t.name != slot[k] || t.type != slotType[k] )
			return fatal( "tex.txt order at " + t.name );
		GLuint into = 0;
		if ( k == 0 ) {
			if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &volKey ), 1, t.w, t.h, t.d, fRgba16f, volShared, why ) )
				return fatal( "the shared volume: " + why );
			into = volShared;
		}
		if ( !upload( dir, t, into ) )
			return fatal( "upload " + t.name );
	}
	const auto pzIt = uni0.find( "fbProjZ" );
	if ( pzIt == uni0.end() || pzIt->second.size() != 2 )
		return fatal( "fbProjZ" );
	const float pz[2] = { float( pzIt->second[0] ), float( pzIt->second[1] ) };

	auto readSamples = [&]( GLuint ms ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 15, fT2DMS, ms );
		u1i( pUnpack, "src", 15 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		return out;
	};
	// the frame's depth and stencil for one job (colour untouched)
	auto frame = [&]( std::uint32_t seed ) {
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		f.useProgram( pFrame );
		const GLint ls = f.getUniformLocation( pFrame, "seed" );
		f.uniform1uiv( ls, 1, &seed );
		f.uniform2fv( f.getUniformLocation( pFrame, "pz" ), 1, pz );
		u1i( pFrame, "mode", 0 );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glDepthMask( GL_FALSE );
		glDisable( GL_DEPTH_TEST );
		glEnable( GL_STENCIL_TEST );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		glStencilMask( 0x04 );
		glStencilFunc( GL_ALWAYS, 0x04, 0xFF );
		u1i( pFrame, "mode", 1 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glStencilMask( 0x03 );
		glStencilFunc( GL_ALWAYS, 0x03, 0xFF );
		u1i( pFrame, "mode", 2 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glStencilMask( 0xFF );
		glDepthFunc( GL_LESS );
	};
	auto bindJob = [&]( const UniMap & uni ) {
		f.useProgram( pFog );
		setUnis( pFog, uni );
		for ( int k = 0; k < 4; k++ ) {
			bindUnit( k, tex[size_t( k )].target, tex[size_t( k )].gl );
			u1i( pFog, slot[k], k );
		}
		bindUnit( 5, GL_TEXTURE_2D, depth1 );
		u1i( pFog, "fbDepth", 5 );
		u1i( pFog, "fbTranslucent", 0 );
	};
	// cellfogbloom.cpp's GL steps
	auto runGl = [&]( const UniMap & uni ) {
		f.bindFramebuffer( fReadFb, fbFrame );
		f.bindFramebuffer( fDrawFb, fbDepth1 );
		blitFramebuffer( 0, 0, W, H, 0, 0, W, H, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
		f.bindFramebuffer( fFb, fbRef );
		glViewport( 0, 0, W, H );
		drawState0();
		glStencilMask( 0 );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );
		bindJob( uni );
		f.bindFramebuffer( fFb, fbRef );
		glDisable( GL_STENCIL_TEST );
		u1i( pFog, "fbTranslucent", 0 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glEnable( GL_STENCIL_TEST );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		glStencilFunc( GL_EQUAL, 0x04, 0x04 );
		u1i( pFog, "fbTranslucent", 1 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glStencilMask( 0xFF );
		return readSamples( refR8 );
	};
	auto runVk = [&]( const UniMap & uni, int red, std::vector<std::uint16_t> & out ) -> bool {
		bindJob( uni );
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		WwVkPresent::FogBloomRun t;
		t.w = W;
		t.h = H;
		t.samples = S;
		glGetIntegerv( GL_VIEWPORT, t.vp );
		t.color = fc;
		static char key;
		t.key = reinterpret_cast<std::uintptr_t>( &key );
		for ( int k = 0; k < 4; k++ ) {
			if ( !WwVkPresent::fxTexOf( &ga, pFog, slot[k], slotType[k], t.tex[k], why ) ) {
				std::printf( "FOGBLOOMVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[k] );
		}
		WwVkPresent::fxPack( &ga, pFog, WwVkPresent::kT_fogbloom, int( std::size( WwVkPresent::kT_fogbloom ) ), t.loose );
		t.red = red;
		unsigned target = 0;
		if ( !WwVkPresent::fogBloomRun( t, target, why ) || !target ) {
			std::printf( "FOGBLOOMVIEW fogBloomRun refused: %s\n", why.c_str() );
			return false;
		}
		out = readSamples( target );
		return glGetError() == GL_NO_ERROR;
	};

	// ---- the sweep: seed per name, fbParams' window (k, b, sigma at f = 0, at f = 1)
	const struct
	{
		const char * name;
		float params[4];
	} jobs[4] = {
		{ "fogbloom/step", { 1.0f, 0.5f, 0.0f, 1e-9f } },
		{ "fogbloom/wide", { 3.0f, 0.5f, 0.0f, 4e-5f } },
		{ "fogbloom/mid", { 1.0f, 0.5f, 1e-5f, 3e-5f } },
		{ "fogbloom/app", { 1.0f, 0.5f, 3.912f / 139986.0f, 3.912f / 69993.0f } },
	};
	bool ok = true;
	size_t litAll = 0;
	UniMap uniRed;
	std::vector<std::uint16_t> refRed;
	for ( int ji = 0; ji < 4; ji++ ) {
		UniMap uni = uni0;
		uni["fbParams"] = { jobs[ji].params[0], jobs[ji].params[1], jobs[ji].params[2], jobs[ji].params[3] };
		const std::uint32_t seed = fnv1a( jobs[ji].name );
		frame( seed );
		const std::vector<std::uint16_t> ref = runGl( uni );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( std::string( "the GL reference of " ) + jobs[ji].name );
		std::vector<std::uint16_t> a;
		if ( !runVk( uni, 0, a ) ) {
			std::printf( "FOGBLOOMVIEW %s FAIL (the Vulkan run failed)\n", jobs[ji].name );
			ok = false;
			continue;
		}
		const Verdict v = judge( a, ref, size_t( WS ) );
		std::printf( "FOGBLOOMVIEW %s %s (seed %08x, window %.3g..%.3g k %.1f: %zu samples lit, %zu differ, %zu by 1 level, %zu past 1, max %d)\n",
			jobs[ji].name, v.word(), seed, double( jobs[ji].params[2] ), double( jobs[ji].params[3] ), double( jobs[ji].params[0] ), v.lit,
			v.diff, v.one, v.past1, v.maxd );
		litAll += v.lit;
		ok = ok && ( v.diff == 0 || v.ok() );
		if ( ji == 0 ) {
			uniRed = uni;
			refRed = ref;
		}
	}
	if ( litAll < 1000 ) {
		std::printf( "FOGBLOOMVIEW the sweep lit %zu samples (< 1000): the job cannot judge the pass\n", litAll );
		ok = false;
	}
	{
		const WwVkPresent::FxStat st = WwVkPresent::takeFogBloomStat();
		std::printf( "FOGBLOOMVIEW stat: %d runs, %d pipelines built in %.1f ms, %d input copies\n", st.runs, st.pipelines, st.buildMs, st.copies );
	}
	const struct
	{
		const char * name;
		int red;
	} reds[3] = { { "no second draw", 1 }, { "first draw with fbTranslucent 1", 2 }, { "fbProj.y doubled", 4 } };
	frame( fnv1a( jobs[0].name ) );
	for ( const auto & r : reds ) {
		std::vector<std::uint16_t> a;
		if ( !runVk( uniRed, r.red, a ) ) {
			std::printf( "FOGBLOOMVIEW red %s: the run failed (counts as a FAIL)\n", r.name );
			continue;
		}
		const Verdict v = judge( a, refRed, size_t( WS ) );
		std::printf( "FOGBLOOMVIEW red %s: %s (%zu lit, %zu differ, %zu past 1)\n", r.name, v.word(), v.lit, v.diff, v.past1 );
		if ( v.diff == 0 || v.ok() ) {
			std::printf( "FOGBLOOMVIEW red %s PASSED: the gate cannot see it\n", r.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFogBloomStat();
	std::printf( "FOGBLOOMVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f; includes the harness's "
		"hand-off wait, not a frame cost)\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKey ) );
	for ( char & k : twinKeys )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "FOGBLOOMVIEW GATE OK" : "FOGBLOOMVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
