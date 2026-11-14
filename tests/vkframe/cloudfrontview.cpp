// Lane VKFAR2 (2026-11-14, row P8): the clouds in front of the scene on the present device (vkcloudfront.inc
// cloudFrontRun: one draw of cloud_composite_view_ps, cvFront 2, into the tone frame's colour with its stencil) against
// GL's own cloud_composite program in cloudvol.cpp frontPass's mode-2 state (blend ONE / ONE_MINUS_SRC_ALPHA for colour,
// ZERO / ONE for alpha; stencil EQUAL 1 mask 0x03, write mask 0), per sample, on the same shared 4x MSAA tone frame.
// fxview.cpp's helpers are reused (its main renamed).
// Inputs, seeded per job name (FNV-1a): the frame's colour per sample (HDR, alpha 0..1) and stencil (low bits 1 on ~47 %,
// 2 on ~8 %, bit 0x04 on ~30 %: bits the pass must ignore); the history (RGBA16F, premultiplied light, 25 % clear texels)
// and its depth (RG32F, km; 1e4 where clear) in shared targets at 107x61 (not a multiple of the frame); the scene's
// window depth (R32F, frame size, NEAREST, 15-40 % sky) as GL's own texture, copied into its twin by the pass.
// Sweep: 4 drawn jobs (4 draws): plain, a projection offset with an inset viewport, near clouds over more sky, dense clouds
// with another offset; plus the flash job, which Vulkan must refuse (run 1 drew it against the bar below and FAILED:
// 1165 of 78882 changed halves off, 131 past 1 ulp, max 7; spelling NV GL's fma order moved 1 half, so the flash frames
// stay GL's: `length( e ) - 6360` km is ill-conditioned, skill ww-vk-image-pass-match).
// Bar (set before the first run, never widened), per job on every RGBA16F half of every sample:
//   changed = halves GL's pass changed in the frame; diff = halves where Vulkan and GL differ.
//   Drawn jobs: EXACT, or BOUND when every differing half is within 1 ulp and diff <= 0.5 % of changed.
//   (The flash's bar, run 1: diff <= 0.5 % of changed and past 1 ulp <= 0.1 %; it failed, and stays in judge().)
//   Each drawn job must change >= 1000 halves.
// Reds (job 0, the strict bar, each must FAIL): red 1 the stencil test off, red 2 no blend, red 4 cvFront 1 (the display
// composite in the linear frame).
// Last line `CLOUDFRONTVIEW GATE OK` or `CLOUDFRONTVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <cmath>

namespace cfv
{
using namespace fv;

// the frame's colour (mode 0) and stencil marks (modes 1..3), per sample; gl_SampleID makes it run per sample
const char * kFrameFs = "#version 410 core\n"
	"uniform uint seed; uniform int mode; out vec4 o;\n"
	"uint hsh( uint x ) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }\n"
	"float u01( uint x ) { return float( hsh( x ) >> 8 ) * ( 1.0 / 16777216.0 ); }\n"
	"void main() {\n"
	"  uvec2 p = uvec2( gl_FragCoord.xy ); uint s = uint( gl_SampleID );\n"
	"  uint ps = hsh( hsh( seed ^ ( p.y * 4099u + p.x ) ) ^ ( ( s + 1u ) * 0x85ebca6bu ) );\n"
	"  o = vec4( 2.0 * u01( ps ^ 1u ), 2.0 * u01( ps ^ 2u ), 2.0 * u01( ps ^ 3u ), u01( ps ^ 4u ) );\n"
	"  if ( mode == 1 && u01( ps ^ 0x27d4eb2fu ) > 0.55 ) discard;\n"
	"  if ( mode == 2 && u01( ps ^ 0x165667b1u ) > 0.15 ) discard;\n"
	"  if ( mode == 3 && u01( ps ^ 0x9e3779b9u ) > 0.30 ) discard;\n"
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

std::uint32_t hs( std::uint32_t x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float u01( std::uint32_t x )
{
	return float( hs( x ) >> 8 ) * ( 1.0f / 16777216.0f );
}

struct Verdict
{
	size_t total = 0, changed = 0, diff = 0, past1 = 0;
	int maxu = 0;
	size_t worst = 0;
	std::uint16_t worstA = 0, worstB = 0;
	bool ok( bool flash ) const
	{
		if ( diff == 0 )
			return true;
		if ( flash )
			return diff * 200 <= changed && past1 * 1000 <= changed;
		return maxu <= 1 && diff * 200 <= changed;
	}
	const char * word( bool flash ) const { return diff == 0 ? "EXACT" : ok( flash ) ? "BOUND" : "FAIL"; }
};

Verdict judge( const std::vector<std::uint16_t> & vk, const std::vector<std::uint16_t> & gl, const std::vector<std::uint16_t> & pre )
{
	Verdict v;
	if ( vk.size() != gl.size() || gl.size() != pre.size() || gl.empty() ) {
		v.diff = v.past1 = 1;
		v.maxu = 1 << 16;
		return v;
	}
	v.total = gl.size();
	for ( size_t i = 0; i < gl.size(); i++ ) {
		v.changed += gl[i] != pre[i];
		const bool nanA = ( vk[i] & 0x7C00 ) == 0x7C00 && ( vk[i] & 0x3FF ), nanB = ( gl[i] & 0x7C00 ) == 0x7C00 && ( gl[i] & 0x3FF );
		int d = std::abs( ordered( vk[i] ) - ordered( gl[i] ) );
		if ( nanA || nanB )
			d = nanA == nanB ? 0 : 1 << 16;
		v.diff += d != 0;
		v.past1 += d > 1;
		if ( d > v.maxu ) {
			v.maxu = d;
			v.worst = i;
			v.worstA = vk[i];
			v.worstB = gl[i];
		}
	}
	return v;
}
}  // namespace cfv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace cfv;
	gTag = "CLOUDFRONTVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: cloudfrontview <prep dir>\nCLOUDFRONTVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = std::string( argv[1] ) + "/cloudfront";
	auto fatal = [&]( const std::string & why ) {
		std::printf( "CLOUDFRONTVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 320, H = 180, WS = W * S, HW = 107, HH = 61;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwcloudfrontview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwcloudfrontview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwcloudfrontview", "clouds in front view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	std::printf( "CLOUDFRONTVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pFrame = program( kTriVs, kFrameFs, "frame" );
	// GL draws cell_ao.vert's quad; the full-screen triangle covers the same samples (the program reads gl_FragCoord only)
	const GLuint pCf = program( kTriVs, readFile( dir + "/prog.frag.glsl" ), "cloud_composite" );
	if ( !pUnpack || !pFrame || !pCf )
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
	if ( !fbFrame || !fbRef || !fbUnp )
		return fatal( "GL targets" );

	// ---- the history (shared, as the app's VKSHARE1 targets) and the scene depth (GL's own, copied into a twin)
	static char histKey, depKey, twinKeys[3];
	GLuint hist = 0, hdep = 0;
	if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &histKey ), 0, HW, HH, 1, fRgba16f, hist, why )
		|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &depKey ), 0, HW, HH, 1, fRg32f, hdep, why ) )
		return fatal( "the shared history: " + why );
	for ( GLuint t : { hist, hdep } ) {
		glBindTexture( GL_TEXTURE_2D, t );
		texParams( GL_TEXTURE_2D, true, false, false );
	}
	const GLuint scene = tex2d( W, H, fR32f, fRed, GL_FLOAT, nullptr, false );
	glBindTexture( GL_TEXTURE_2D, 0 );
	// the scene's projection: n 10, f 1e7 game units; 1 unit = 1.4287 cm
	const double n = 10.0, fz = 1e7;
	const float P22 = float( -( fz + n ) / ( fz - n ) ), P32 = float( -2.0 * fz * n / ( fz - n ) ), kmPerUnit = 1.4287e-5f;

	struct Job
	{
		const char * name;
		bool flash;
		float p20, p21;
		int inset;
		float kmLo, kmHi, sky;
	};
	const int kJobs = 5;
	const Job jobs[kJobs] = {
		{ "cloudfront/plain", false, 0.0f, 0.0f, 0, 0.3f, 12.0f, 0.15f },
		{ "cloudfront/offset", false, 0.0031f, -0.0017f, 12, 0.3f, 12.0f, 0.15f },
		{ "cloudfront/near", false, 0.0f, 0.0f, 0, 0.05f, 3.0f, 0.40f },
		{ "cloudfront/dense", false, -0.0023f, 0.0041f, 6, 0.1f, 20.0f, 0.05f },
		{ "cloudfront/flash", true, 0.0f, 0.0f, 0, 0.5f, 8.0f, 0.15f },
	};
	auto seedInputs = [&]( const Job & j, std::uint32_t seed ) {
		std::vector<float> h( size_t( HW ) * HH * 4 ), d( size_t( HW ) * HH * 2 ), z( size_t( W ) * H );
		for ( int i = 0; i < HW * HH; i++ ) {
			const std::uint32_t x = hs( seed ^ 0xA511E9B3u ^ std::uint32_t( i ) * 2654435761u );
			const bool clear = u01( x ) < 0.25f;
			const float T = clear ? 1.0f : u01( x ^ 1u );
			for ( int c = 0; c < 3; c++ )
				h[size_t( i ) * 4 + size_t( c )] = ( 1.0f - T ) * 3.0f * u01( x ^ std::uint32_t( 2 + c ) );
			h[size_t( i ) * 4 + 3] = T;
			const float km = clear ? 1e4f : j.kmLo * std::pow( j.kmHi / j.kmLo, u01( x ^ 5u ) );
			d[size_t( i ) * 2] = km;
			d[size_t( i ) * 2 + 1] = km * km;
		}
		for ( int i = 0; i < W * H; i++ ) {
			const std::uint32_t y = hs( seed ^ 0x5C0FFEE5u ^ std::uint32_t( i ) * 2246822519u );
			if ( u01( y ) < j.sky ) {
				z[size_t( i )] = 1.0f;
				continue;
			}
			const double D = 1e3 * std::pow( 2e3, double( u01( y ^ 1u ) ) );
			z[size_t( i )] = float( 0.5 * ( -double( P22 ) + double( P32 ) / D ) + 0.5 );
		}
		glBindTexture( GL_TEXTURE_2D, hist );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, HW, HH, GL_RGBA, GL_FLOAT, h.data() );
		glBindTexture( GL_TEXTURE_2D, hdep );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, HW, HH, fRg, GL_FLOAT, d.data() );
		glBindTexture( GL_TEXTURE_2D, scene );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, W, H, fRed, GL_FLOAT, z.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
	};
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
	// the frame's colour and stencil for one job into one target
	auto frame = [&]( GLuint fb, std::uint32_t seed ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glStencilMask( 0xFF );
		glClearStencil( 0 );
		glClear( GL_STENCIL_BUFFER_BIT );
		f.useProgram( pFrame );
		f.uniform1uiv( f.getUniformLocation( pFrame, "seed" ), 1, &seed );
		u1i( pFrame, "mode", 0 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		glEnable( GL_STENCIL_TEST );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		const GLuint refs[3] = { 1, 2, 4 }, masks[3] = { 0xFF, 0x03, 0x04 };
		for ( int m = 0; m < 3; m++ ) {
			glStencilMask( masks[m] );
			glStencilFunc( GL_ALWAYS, GLint( refs[m] ), 0xFF );
			u1i( pFrame, "mode", m + 1 );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
		}
		drawState0();
		glStencilMask( 0xFF );
	};
	auto u4 = [&]( const char * nm, float a, float b, float c, float d ) {
		const float v[4] = { a, b, c, d };
		f.uniform4fv( f.getUniformLocation( pCf, nm ), 1, v );
	};
	auto u3 = [&]( const char * nm, float a, float b, float c ) {
		const float v[3] = { a, b, c };
		f.uniform3fv( f.getUniformLocation( pCf, nm ), 1, v );
	};
	// frontPass's uniforms and units (cloudvol.cpp), the pass's viewport bound
	auto bindJob = [&]( const Job & j, const GLint * vp ) {
		f.useProgram( pCf );
		bindUnit( 0, GL_TEXTURE_2D, hist );
		bindUnit( 1, GL_TEXTURE_2D, hdep );
		bindUnit( 2, GL_TEXTURE_2D, scene );
		u1i( pCf, "cvHist", 0 );
		u1i( pCf, "cvHistDepth", 1 );
		u1i( pCf, "cvScene", 2 );
		u1i( pCf, "cvFront", 2 );
		u4( "cvSceneZ", P22, P32, kmPerUnit, 1.0f );
		u4( "cvProj", 0.8023f, 1.4281f, j.p20, j.p21 );
		u4( "cvRect", float( vp[0] ), float( vp[1] ), 1.0f / float( vp[2] ), 1.0f / float( vp[3] ) );
		const float ex = 1.3f;
		f.uniform1fv( f.getUniformLocation( pCf, "sceneExposure" ), 1, &ex );
		u1i( pCf, "viewTransform", 0 );
		u1i( pCf, "cvLtOn", j.flash ? 1 : 0 );
		u4( "cvLtSrc", 0.0f, 3.0f, 0.8f, 1.0f );
		u4( "cvLtPrm", 0.04f, 0.8f, 5.2f, 0.6f );
		u3( "cvLtCol", 40.0f, 40.0f, 50.0f );
		const float cam[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f };  // right, up, back (world z up, looking +y)
		f.uniformMatrix3fv( f.getUniformLocation( pCf, "cvCam" ), 1, GL_FALSE, cam );
		u3( "cvCamKm", 0.0f, 0.0f, 0.1f );
		glViewport( vp[0], vp[1], vp[2], vp[3] );
	};
	auto vpOf = [&]( const Job & j, GLint * vp ) {
		vp[0] = j.inset;
		vp[1] = j.inset / 2;
		vp[2] = W - 2 * j.inset;
		vp[3] = H - j.inset;
	};
	// cloudvol.cpp frontPass mode 2's GL draw
	auto runGl = [&]( const Job & j ) {
		GLint vp[4];
		vpOf( j, vp );
		f.bindFramebuffer( fFb, fbRef );
		drawState0();
		bindJob( j, vp );
		glEnable( GL_BLEND );
		f.blendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0 );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		glStencilFunc( GL_EQUAL, 1, 0x03 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glStencilMask( 0xFF );
		return readSamples( refCol );
	};
	auto runVk = [&]( const Job & j, int red, std::vector<std::uint16_t> & out ) -> bool {
		GLint vp[4];
		vpOf( j, vp );
		f.bindFramebuffer( fFb, fbFrame );
		drawState0();
		bindJob( j, vp );
		WwVkPresent::CloudFrontRun t;
		t.w = W;
		t.h = H;
		glGetIntegerv( 0x80A9, &t.samples );  // GL_SAMPLES (the app's frontOnVulkan reads the same)
		glGetIntegerv( GL_VIEWPORT, t.vp );
		t.color = fc;
		static const char * const kName[3] = { "cvHist", "cvHistDepth", "cvScene" };
		for ( int k = 0; k < 3; k++ ) {
			if ( !WwVkPresent::fxTexOf( &ga, pCf, kName[k], 0, t.tex[k], why ) ) {
				std::printf( "CLOUDFRONTVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[k] );
		}
		WwVkPresent::fxPack( &ga, pCf, WwVkPresent::kT_cloudfront, int( std::size( WwVkPresent::kT_cloudfront ) ), t.loose );
		t.red = red;
		if ( !WwVkPresent::cloudFrontRun( t, why ) ) {
			std::printf( "CLOUDFRONTVIEW cloudFrontRun refused: %s\n", why.c_str() );
			return false;
		}
		out = readSamples( fc );
		return glGetError() == GL_NO_ERROR;
	};

	bool ok = true;
	std::vector<std::uint16_t> refRed, preRed;
	for ( int ji = 0; ji < kJobs; ji++ ) {
		const Job & j = jobs[ji];
		const std::uint32_t seed = fnv1a( j.name );
		seedInputs( j, seed );
		frame( fbFrame, seed );
		frame( fbRef, seed );
		const std::vector<std::uint16_t> pre = readSamples( fc ), preRef = readSamples( refCol );
		if ( pre != preRef )
			return fatal( std::string( "the two frames of " ) + j.name + " differ before the pass" );
		const std::vector<std::uint16_t> ref = runGl( j );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( std::string( "the GL reference of " ) + j.name );
		std::vector<std::uint16_t> a;
		if ( j.flash ) {
			// the flash stays GL's (vkcloudfront.inc refuses cvLtOn 1); the Vulkan run must refuse and leave the frame as it was
			const bool drew = runVk( j, 0, a );
			const std::vector<std::uint16_t> after = readSamples( fc );
			const bool refused = !drew && why.find( "lightning flash" ) != std::string::npos && after == pre;
			std::printf( "CLOUDFRONTVIEW %s %s (Vulkan %s: %s; GL's own draw changed %zu halves)\n", j.name, refused ? "REFUSED as designed" : "FAIL",
				drew ? "drew" : "refused", why.c_str(), judge( ref, ref, pre ).changed );
			ok = ok && refused;
			continue;
		}
		if ( !runVk( j, 0, a ) ) {
			std::printf( "CLOUDFRONTVIEW %s FAIL (the Vulkan run failed)\n", j.name );
			ok = false;
			continue;
		}
		const Verdict v = judge( a, ref, pre );
		std::printf( "CLOUDFRONTVIEW %s %s (seed %08x, clouds %.2f..%.1f km, sky %.0f %%, flash %d: %zu of %zu halves changed by GL, %zu differ, "
			"%zu past 1 ulp, max %d ulp)\n", j.name, v.word( j.flash ), seed, double( j.kmLo ), double( j.kmHi ), double( j.sky * 100.0f ),
			j.flash ? 1 : 0, v.changed, v.total, v.diff, v.past1, v.maxu );
		if ( v.diff ) {
			const size_t px = v.worst / 4;
			std::printf( "CLOUDFRONTVIEW %s: worst at sample column %zu row %zu channel %zu: Vulkan %.6g, GL %.6g\n", j.name, px % size_t( WS ),
				px / size_t( WS ), v.worst % 4, double( h2f( v.worstA ) ), double( h2f( v.worstB ) ) );
		}
		if ( v.changed < 1000 ) {
			std::printf( "CLOUDFRONTVIEW %s changed %zu halves (< 1000): the job cannot judge the pass\n", j.name, v.changed );
			ok = false;
		}
		ok = ok && v.ok( j.flash );
		if ( ji == 0 ) {
			refRed = ref;
			preRed = pre;
		}
	}
	{
		const WwVkPresent::FxStat st = WwVkPresent::takeCloudFrontStat();
		std::printf( "CLOUDFRONTVIEW stat: %d runs, %d pipelines built in %.1f ms, %d input copies\n", st.runs, st.pipelines, st.buildMs, st.copies );
	}
	const struct
	{
		const char * name;
		int red;
	} reds[3] = { { "the stencil test off", 1 }, { "no blend", 2 }, { "cvFront 1 (the display composite)", 4 } };
	seedInputs( jobs[0], fnv1a( jobs[0].name ) );
	for ( const auto & r : reds ) {
		frame( fbFrame, fnv1a( jobs[0].name ) );
		std::vector<std::uint16_t> a;
		if ( !runVk( jobs[0], r.red, a ) ) {
			std::printf( "CLOUDFRONTVIEW red %s: the run failed (counts as a FAIL)\n", r.name );
			continue;
		}
		const Verdict v = judge( a, refRed, preRed );
		std::printf( "CLOUDFRONTVIEW red %s: %s (%zu differ, %zu past 1 ulp, max %d ulp)\n", r.name, v.word( false ), v.diff, v.past1, v.maxu );
		if ( v.ok( false ) ) {
			std::printf( "CLOUDFRONTVIEW red %s PASSED: the gate cannot see it\n", r.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeCloudFrontStat();
	std::printf( "CLOUDFRONTVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f; includes the harness's "
		"hand-off wait, not a frame cost)\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &histKey ) );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &depKey ) );
	for ( char & k : twinKeys )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "CLOUDFRONTVIEW GATE OK" : "CLOUDFRONTVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
