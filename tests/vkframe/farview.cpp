// Lane VKFAR1 (2026-11-13, row G3): the far bulb dots (farDotsRun, vkfardots.inc: instanced quads, no geometry stage) on
// the present device against GL's own cell_fardots program drawn as GL_POINTS (celllights.cpp wwCellFarDotsDraw's state),
// per sample, into the same 4x MSAA RGBA16F + D24S8 frame contents. fxview.cpp's helpers are reused (its main renamed).
// Frame before the dots: a sky clear and a depth slab over the left half at the dots' median depth (half the dots hidden
// there). After the dots a full-screen draw where stencil == 1 sets alpha 1, so the stencil the dots wrote is read too.
// Bar (set before the first run, never widened):
//   covered = samples whose colour differs from the frame before the dots on either side (must be >= 1000);
//   flips = samples with any half past 2 ulp (a sprite edge or the g < 0.011 discard on one side only);
//   EXACT = no half differs; BOUND = flips <= 0.05 % of covered, halves past 1 ulp outside flips <= 0.1 % of the covered
//   halves, stencil differs on <= 0.05 % of covered samples. Anything else FAILS.
// Reds (each must FAIL): red 1 stencil write mask 0, red 2 no blend, red 4 quads twice the point size.
// Last line `FARVIEW GATE OK` or `FARVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>

namespace farv
{
using namespace fv;
struct Va
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
};
Va va;

const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";
const char * kRevealFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.0, 0.0, 0.0, 1.0 ); }\n";

struct Verdict
{
	size_t covered = 0, flips = 0, past1 = 0, diff = 0, stencil = 0;
	int maxu = 0;
	bool ok() const { return covered >= 1000 && flips * 2000 <= covered && past1 * 1000 <= covered * 4 && stencil * 2000 <= covered; }
	const char * word() const { return covered < 1000 ? "FAIL" : diff == 0 && stencil == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

size_t gWS = 1;
bool gDump = false;

Verdict judge( const std::vector<std::uint16_t> & bg, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b,
	const std::vector<std::uint16_t> & ra, const std::vector<std::uint16_t> & rb )
{
	Verdict v;
	if ( a.size() != b.size() || a.size() != bg.size() || ra.size() != a.size() || rb.size() != a.size() || a.empty() )
		return v;
	for ( size_t s = 0; s < a.size() / 4; s++ ) {
		bool cov = false;
		int m = 0, p1 = 0, df = 0;
		for ( int c = 0; c < 4; c++ ) {
			const size_t i = s * 4 + size_t( c );
			cov = cov || a[i] != bg[i] || b[i] != bg[i];
			const int d = std::abs( ordered( a[i] ) - ordered( b[i] ) );
			m = std::max( m, d );
			p1 += d > 1;
			df += d != 0;
		}
		const bool sa = ra[s * 4 + 3] == 0x3C00, sb = rb[s * 4 + 3] == 0x3C00;
		cov = cov || sa || sb;
		if ( !cov )
			continue;
		v.covered++;
		v.diff += size_t( df );
		v.stencil += sa != sb;
		if ( m > 2 ) {
			if ( gDump && v.flips < 60 )  // FARVIEW_DUMP=1: where the flips sit (pixel, sample), Vulkan / GL / before, stencil
				std::printf( "FARVIEW flip px %zu,%zu s%zu: vk %.5g %.5g %.5g gl %.5g %.5g %.5g bg %.5g st %d/%d\n", ( s % gWS ) / 4, s / gWS,
					s % 4, h2f( a[s * 4] ), h2f( a[s * 4 + 1] ), h2f( a[s * 4 + 2] ), h2f( b[s * 4] ), h2f( b[s * 4 + 1] ), h2f( b[s * 4 + 2] ),
					h2f( bg[s * 4] ), int( sa ), int( sb ) );
			v.flips++;
		} else
			v.past1 += size_t( p1 );
		v.maxu = std::max( v.maxu, m > 2 ? 0 : m );
	}
	return v;
}
}  // namespace farv (windows.h defines far)

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace farv;
	gTag = "FARVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: farview <prep dir>\nFARVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = std::string( argv[1] ) + "/fardots";
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfarview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfarview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfarview", "far view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	if ( !loadAll() || !load( va.genVertexArrays, "glGenVertexArrays" ) || !load( va.bindVertexArray, "glBindVertexArray" )
		|| !load( va.vertexAttribPointer, "glVertexAttribPointer" ) || !load( va.enableVertexAttribArray, "glEnableVertexAttribArray" ) )
		return fatal( "GL entry points" );
	std::printf( "FARVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pSlab = program( kTriVs, kSlabFs, "slab" ),
		pReveal = program( kTriVs, kRevealFs, "reveal" );
	const GLuint pDots = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.frag.glsl" ), "cell_fardots" );
	if ( !pUnpack || !pSlab || !pReveal || !pDots )
		return fatal( "GL programs" );
	std::vector<int> jv;
	if ( !readJob( dir + "/job.txt", jv, 2 ) )
		return fatal( "job.txt" );
	const int W = jv[0], H = jv[1], WS = W * S;
	gWS = size_t( WS );
	gDump = std::getenv( "FARVIEW_DUMP" ) != nullptr;
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

	// ---- the program's inputs: the fog's uniforms + textures (the volume in a shared target, as the app's), the global block
	const char * slot[5] = { "atApTex", "volAirTerr", "volAirRainMap", "volTex", "volTile" };
	const int slotType[5] = { 1, 2, 0, 1, 0 };
	UniMap uni = readUni( dir + "/uni.txt" );
	std::vector<TexIn> tex = readTex( dir );
	if ( tex.size() != 5 )
		return fatal( "tex.txt" );
	static char twinKeys[5], volKey;
	GLuint volShared = 0;
	for ( int k = 0; k < 5; k++ ) {
		TexIn & t = tex[size_t( k )];
		if ( t.name != slot[k] || t.type != slotType[k] )
			return fatal( "tex.txt order at " + t.name );
		GLuint into = 0;
		if ( k == 3 ) {
			if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &volKey ), 1, t.w, t.h, t.d, fRgba16f, volShared, why ) )
				return fatal( "the shared volume: " + why );
			into = volShared;
		}
		if ( !upload( dir, t, into ) )
			return fatal( "upload " + t.name );
	}
	const std::string global = readFile( dir + "/global.bin" );
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );
	f.bindBuffer( fUbo, ubo );
	f.bufferData( fUbo, std::ptrdiff_t( global.size() ), global.data(), fStatic );
	const GLuint bi = f.getUniformBlockIndex( pDots, "globalUniforms" );
	if ( bi != 0xFFFFFFFFu )
		f.uniformBlockBinding( pDots, bi, 0 );
	const auto pit = uni.find( "projectionMatrix" );
	if ( pit == uni.end() || pit->second.size() != 16 )
		return fatal( "projectionMatrix" );
	float proj[16];
	for ( int k = 0; k < 16; k++ )
		proj[k] = float( pit->second[size_t( k )] );

	// ---- the dots: view space in the frustum (some past its edges), 600..30000 units (the band 1024..5120 fades the near
	// ones; under 1024 a dot has size 0), a rotated + moved view so dotRow and dotCam carry real values
	const float ang = 0.3f, ca = std::cos( ang ), sa = std::sin( ang );
	const float R[3][3] = { { ca, 0.0f, sa }, { 0.0f, 1.0f, 0.0f }, { -sa, 0.0f, ca } };
	const float tr[3] = { 10.0f, -20.0f, 30.0f };
	float row[12], cam[3];
	for ( int j = 0; j < 3; j++ ) {
		for ( int k = 0; k < 3; k++ )
			row[j * 4 + k] = R[j][k];
		row[j * 4 + 3] = tr[j];
	}
	for ( int k = 0; k < 3; k++ )
		cam[k] = -( R[0][k] * tr[0] + R[1][k] * tr[1] + R[2][k] * tr[2] );
	const float band[2] = { 1024.0f, 5120.0f };
	const float focal = proj[5] * 0.5f * float( H );
	const int n = 4000;
	std::mt19937 rng( 20261113u );
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<float> pos( size_t( n ) * 3 ), val( size_t( n ) * 3 ), zs;
	for ( int i = 0; i < n; i++ ) {
		const float d = 600.0f * std::pow( 50.0f, u01( rng ) );
		const float v[3] = { ( u01( rng ) * 2.2f - 1.1f ) * d / proj[0], ( u01( rng ) * 2.2f - 1.1f ) * d / proj[5], -d };
		for ( int k = 0; k < 3; k++ )
			pos[size_t( i ) * 3 + k] = R[0][k] * ( v[0] - tr[0] ) + R[1][k] * ( v[1] - tr[1] ) + R[2][k] * ( v[2] - tr[2] );
		const float sigma = std::max( 0.6f, focal * 4.0f / d ), kk = d / focal;
		for ( int c = 0; c < 3; c++ )
			val[size_t( i ) * 3 + c] = ( 0.5f + 7.5f * u01( rng ) ) * 6.2831853f * sigma * sigma * kk * kk;
		if ( d > band[0] ) {
			float cz = 0.0f, cw = 0.0f;
			for ( int k = 0; k < 3; k++ ) {
				cz += proj[k * 4 + 2] * v[k];
				cw += proj[k * 4 + 3] * v[k];
			}
			cz += proj[14];
			cw += proj[15];
			zs.push_back( ( cz / cw ) * 0.5f + 0.5f );
		}
	}
	// the slab sits halfway between two neighbouring dot depths: a dot exactly AT the slab is a depth tie, where GL's
	// z / w * 0.5 + 0.5 and Vulkan's ((z + w) / 2) / w may land one D24 step apart (a fixture artefact, not the pass)
	std::sort( zs.begin(), zs.end() );
	const size_t m = zs.size() / 2;
	const float zd = zs.size() < 2 ? 0.5f : 0.5f * ( zs[m - 1] + zs[m] );
	std::printf( "FARVIEW job: %dx%d x%d, %d dots (%zu past the band's start), slab depth %.6f, focal %.2f px\n", W, H, S, n, zs.size(), zd, focal );

	GLuint vao = 0, vb[2] = {};
	va.genVertexArrays( 1, &vao );
	va.bindVertexArray( vao );
	f.genBuffers( 2, vb );
	for ( int k = 0; k < 2; k++ ) {
		f.bindBuffer( 0x8892, vb[k] );  // GL_ARRAY_BUFFER
		f.bufferData( 0x8892, std::ptrdiff_t( pos.size() * 4 ), k ? val.data() : pos.data(), fStatic );
		va.vertexAttribPointer( GLuint( k ), 3, GL_FLOAT, GL_FALSE, 0, nullptr );
		va.enableVertexAttribArray( GLuint( k ) );
	}
	va.bindVertexArray( 0 );

	auto bindJob = [&]() {
		f.useProgram( pDots );
		setUnis( pDots, uni );
		for ( int k = 0; k < 5; k++ ) {
			bindUnit( k, tex[size_t( k )].target, tex[size_t( k )].gl );
			u1i( pDots, slot[k], k );
		}
		f.useProgram( pDots );
		const GLint lr = f.getUniformLocation( pDots, "dotRow" );
		f.uniform4fv( lr, 3, row );
		f.uniformMatrix4fv( f.getUniformLocation( pDots, "dotProj" ), 1, GL_FALSE, proj );
		f.uniform3fv( f.getUniformLocation( pDots, "dotCam" ), 1, cam );
		f.uniform2fv( f.getUniformLocation( pDots, "dotBand" ), 1, band );
		f.uniform1fv( f.getUniformLocation( pDots, "dotFocal" ), 1, &focal );
		const float bulb = 4.0f;
		f.uniform1fv( f.getUniformLocation( pDots, "dotBulb" ), 1, &bulb );
		f.uniform1i( f.getUniformLocation( pDots, "dotRed" ), 0 );
		f.bindBufferBase( fUbo, 0, ubo );
	};
	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 15, fT2DMS, msColor );
		u1i( pUnpack, "src", 15 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		return out;
	};
	// the frame before the dots: sky, then the slab's depth over the left half
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
	auto reveal = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0 );
		glStencilFunc( GL_EQUAL, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		f.useProgram( pReveal );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
	};
	// celllights.cpp wwCellFarDotsDraw's state, GL's points
	std::vector<std::uint16_t> bg, refA, refB;
	{
		before( fbRef );
		bg = readSamples( refCol );
		before( fbRef );
		bindJob();
		f.bindFramebuffer( fFb, fbRef );
		glViewport( 0, 0, W, H );
		glEnable( 0x8642 );  // GL_PROGRAM_POINT_SIZE
		glEnable( 0x8861 );  // GL_POINT_SPRITE (a compatibility context: gl_PointCoord)
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		glDisable( GL_CULL_FACE );
		glEnable( GL_BLEND );
		glBlendFunc( GL_ONE, GL_ONE );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0x03 );
		glStencilFunc( GL_ALWAYS, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		va.bindVertexArray( vao );
		glDrawArrays( GL_POINTS, 0, n );
		va.bindVertexArray( 0 );
		glDisable( 0x8642 );
		glDisable( 0x8861 );
		drawState0();
		glDepthFunc( GL_LESS );
		refA = readSamples( refCol );
		reveal( fbRef );
		refB = readSamples( refCol );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the GL reference draw" );
	}
	if ( std::getenv( "FARVIEW_PROBE" ) ) {
		// each dot alone through GL's own vertex stage, a fragment stage writing (gl_PointCoord, gl_FragCoord.xy): the
		// driver's point centre is fragCoord - (pointCoord - 0.5) * size (x) and fragCoord - (0.5 - pointCoord) * size (y)
		const char * kProbeFs = "#version 410 core\nout vec4 o; void main() { o = vec4( gl_PointCoord, gl_FragCoord.xy ); }\n";
		std::string vsrc = readFile( dir + "/prog.vert.glsl" );
		const GLuint pProbe = program( vsrc, kProbeFs, "probe" );
		const GLuint pt = tex2d( W, H, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
		const GLuint fbP = fbo( GL_TEXTURE_2D, pt, 0, 0 );
		// a second program hands the vertex stage's gl_Position to the fragment, flat (a smooth output comes back through 1/w)
		std::string vclip = vsrc;
		const size_t mpos = vclip.find( "void main()" ), lastBrace = vclip.rfind( '}' );
		if ( mpos == std::string::npos || lastBrace == std::string::npos )
			return fatal( "probe: prog.vert.glsl main" );
		vclip.insert( lastBrace, "\twwClip = gl_Position;\n" );
		vclip.insert( mpos, "flat out vec4 wwClip;\n" );
		const GLuint pClip = program( vclip, "#version 410 core\nflat in vec4 wwClip; out vec4 o; void main() { o = wwClip; }\n", "probe clip" );
		if ( !pProbe || !pClip || !fbP )
			return fatal( "probe program / target" );
		const float bulb = 4.0f;
		for ( GLuint p : { pClip, pProbe } ) {
			f.useProgram( p );
			setUnis( p, uni );
			f.uniform4fv( f.getUniformLocation( p, "dotRow" ), 3, row );
			f.uniformMatrix4fv( f.getUniformLocation( p, "dotProj" ), 1, GL_FALSE, proj );
			f.uniform3fv( f.getUniformLocation( p, "dotCam" ), 1, cam );
			f.uniform2fv( f.getUniformLocation( p, "dotBand" ), 1, band );
			f.uniform1fv( f.getUniformLocation( p, "dotFocal" ), 1, &focal );
			f.uniform1fv( f.getUniformLocation( p, "dotBulb" ), 1, &bulb );
			f.uniform1i( f.getUniformLocation( p, "dotRed" ), 0 );
			const GLuint bp = f.getUniformBlockIndex( p, "globalUniforms" );
			if ( bp != 0xFFFFFFFFu )
				f.uniformBlockBinding( p, bp, 0 );
		}
		f.bindBufferBase( fUbo, 0, ubo );
		f.bindFramebuffer( fFb, fbP );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( 0x8642 );
		glEnable( 0x8861 );
		va.bindVertexArray( vao );
		std::vector<float> img( size_t( W ) * H * 4 );
		FILE * out = std::fopen( ( std::string( argv[1] ) + "/glcentre.txt" ).c_str(), "w" );
		int drawn = 0;
		for ( int i = 0; i < n; i++ ) {
			glClearColor( -1.0f, -1.0f, -1.0f, -1.0f );
			glClear( GL_COLOR_BUFFER_BIT );
			glDrawArrays( GL_POINTS, i, 1 );
			glReadPixels( 0, 0, W, H, GL_RGBA, GL_FLOAT, img.data() );
			// the covered box: its side is the point size, its first texel gives the centre
			int x0 = W, y0 = H, x1 = -1, y1 = -1;
			for ( int y = 0; y < H; y++ )
				for ( int x = 0; x < W; x++ )
					if ( img[( size_t( y ) * W + x ) * 4 + 2] >= 0.0f ) {
						x0 = std::min( x0, x ); x1 = std::max( x1, x );
						y0 = std::min( y0, y ); y1 = std::max( y1, y );
					}
			if ( x1 < 0 )
				continue;
			// an interior texel (not clipped by the frame edge) with its pointcoord, and the size from two texels apart
			const int xm = ( x0 + x1 ) / 2, ym = ( y0 + y1 ) / 2;
			const float * c0 = &img[( size_t( ym ) * W + xm ) * 4];
			float size = 0.0f;
			if ( xm + 1 <= x1 )
				size = 1.0f / ( img[( size_t( ym ) * W + xm + 1 ) * 4] - c0[0] );
			else if ( xm - 1 >= x0 )
				size = 1.0f / ( c0[0] - img[( size_t( ym ) * W + xm - 1 ) * 4] );
			const float sz = size > 0.0f ? std::round( size ) : 1.0f;
			const double cx = double( c0[2] ) - ( double( c0[0] ) - 0.5 ) * sz, cy = double( c0[3] ) - ( 0.5 - double( c0[1] ) ) * sz;
			const float pcx = c0[0], pcy = c0[1], fx = c0[2], fy = c0[3];
			// GL's clip position of this dot, exact bits
			f.useProgram( pClip );
			glClear( GL_COLOR_BUFFER_BIT );
			glDrawArrays( GL_POINTS, i, 1 );
			float clip[4] = {};
			glReadPixels( xm, ym, 1, 1, GL_RGBA, GL_FLOAT, clip );
			f.useProgram( pProbe );
			std::fprintf( out, "%d %.0f %.9f %.9f %.9g %.9g %.9g %.9g %a %a %a %a\n", i, sz, cx, cy, pcx, pcy, fx, fy, clip[0], clip[1],
				clip[2], clip[3] );
			drawn++;
		}
		std::fclose( out );
		va.bindVertexArray( 0 );
		glDisable( 0x8642 );
		glDisable( 0x8861 );
		drawState0();
		// the dots' inputs, for the candidate transforms (python, float32)
		FILE * in = std::fopen( ( std::string( argv[1] ) + "/probe_in.txt" ).c_str(), "w" );
		std::fprintf( in, "vp %d %d %d %d\nrow", 0, 0, W, H );
		for ( float x : row )
			std::fprintf( in, " %.9g", x );
		std::fprintf( in, "\nproj" );
		for ( float x : proj )
			std::fprintf( in, " %.9g", x );
		std::fprintf( in, "\n" );
		for ( int i = 0; i < n; i++ )
			std::fprintf( in, "pos %d %.9g %.9g %.9g\n", i, pos[size_t( i ) * 3], pos[size_t( i ) * 3 + 1], pos[size_t( i ) * 3 + 2] );
		std::fclose( in );
		std::printf( "FARVIEW probe: %d dots drawn alone, centres in glcentre.txt\n", drawn );
	}
	auto runVk = [&]( int red, std::vector<std::uint16_t> & a, std::vector<std::uint16_t> & b ) -> bool {
		before( fbFrame );
		bindJob();
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		WwVkPresent::FarDotsRun t;
		t.w = W;
		t.h = H;
		glGetIntegerv( GL_VIEWPORT, t.vp );
		t.color = fc;
		t.n = n;
		t.pos = pos.data();
		t.value = val.data();
		std::memcpy( t.dotRow, row, sizeof( row ) );
		std::memcpy( t.dotProj, proj, sizeof( proj ) );
		std::memcpy( t.dotCam, cam, sizeof( cam ) );
		std::memcpy( t.dotBand, band, sizeof( band ) );
		t.dotFocal = focal;
		t.dotBulb = 4.0f;
		for ( int k = 0; k < 5; k++ ) {
			if ( !WwVkPresent::fxTexOf( &ga, pDots, slot[k], slotType[k], t.tex[k], why ) ) {
				std::printf( "FARVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[k] );
		}
		WwVkPresent::fxPack( &ga, pDots, WwVkPresent::kT_fardots, int( std::size( WwVkPresent::kT_fardots ) ), t.loose );
		t.red = red;
		f.bindFramebuffer( fFb, fbFrame );
		if ( !WwVkPresent::farDotsRun( t, why ) ) {
			std::printf( "FARVIEW farDotsRun refused: %s\n", why.c_str() );
			return false;
		}
		a = readSamples( fc );
		reveal( fbFrame );
		b = readSamples( fc );
		return glGetError() == GL_NO_ERROR;
	};
	bool ok = true;
	{
		std::vector<std::uint16_t> a, b;
		if ( !runVk( 0, a, b ) ) {
			std::printf( "FARVIEW fardots FAIL (the Vulkan run failed)\n" );
			ok = false;
		} else {
			const Verdict v = judge( bg, a, refA, b, refB );
			std::printf( "FARVIEW fardots %s (%zu samples covered, %zu halves differ, %zu edge flips, %zu halves past 1 ulp outside them, "
				"max %d ulp outside them, stencil differs on %zu; volume %s)\n", v.word(), v.covered, v.diff, v.flips, v.past1, v.maxu,
				v.stencil, "shared" );
			ok = ok && v.ok();
			const WwVkPresent::FxStat st = WwVkPresent::takeFarDotsStat();
			std::printf( "FARVIEW stat: %d runs, %d pipelines built in %.1f ms, %d input copies\n", st.runs, st.pipelines, st.buildMs, st.copies );
		}
	}
	const struct
	{
		const char * name;
		int red;
	} reds[3] = { { "stencil (write mask 0)", 1 }, { "blend (none)", 2 }, { "size (quads twice the point size)", 4 } };
	for ( const auto & r : reds ) {
		std::vector<std::uint16_t> a, b;
		if ( !runVk( r.red, a, b ) ) {
			std::printf( "FARVIEW red %s: the run failed (counts as a FAIL)\n", r.name );
			continue;
		}
		const Verdict v = judge( bg, a, refA, b, refB );
		std::printf( "FARVIEW red %s: %s (%zu covered, %zu flips, stencil differs on %zu)\n", r.name, v.word(), v.covered, v.flips, v.stencil );
		if ( v.ok() ) {
			std::printf( "FARVIEW red %s PASSED: the gate cannot see it\n", r.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFarDotsStat();
	std::printf( "FARVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f; includes the harness's hand-off "
		"wait, not a frame cost)\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKey ) );
	for ( char & k : twinKeys )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "FARVIEW GATE OK" : "FARVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
