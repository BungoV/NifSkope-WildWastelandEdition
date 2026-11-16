// Lane VKOBJ15 (2026-11-16, census row 146): the lightning strips' preview (boltRun, vkbolt.inc) on the present device against
// GL's own boltstrip program (the tree's res/shaders/boltstrip.vert / .frag, uniforms.glsl expanded) with drawPreview's
// state, per sample, into the same 4x MSAA RGBA16F + D24S8 frame contents. fxview.cpp's helpers are reused (its main renamed).
// Frame before the strips: a sky clear and a depth slab over the left half between two strips' depths (half the strips hidden
// there). The strips: 40 camera-facing ribbons (3 quads each) at distinct view depths, odd strips wound the other way,
// vertex colour and alpha off 0 / 1, uvs past [0, 1] (repeat), a rotated + moved modelViewMatrix and a real projection in the
// global block (b0) on both sides. The Vulkan side takes what drawPreview takes: fxPack over GL's own uniforms, boltTexOf over
// GL's units, boltStateOf over GL's state.
// Cases: palette  BaseMap a BC3 texture-cache twin (objTexMake, 3 levels, colour and alpha not transpose-symmetric, trilinear
//                 repeat), GreyscaleMap an RGBA8 palette (a copy twin), greyscaleColor + greyscaleAlpha on, paletteRow 0.37;
//        plain    BaseMap a white RGBA8 (drawPreview's #FFFFFFFF), no palette;
//        single   BaseMap a single-level BC3 twin (GL_LINEAR), no palette (the control: no mip selection);
//        cull     the palette case with GL_CULL_FACE on (GL_BACK, CCW): the odd strips go.
// Bar (set before the first run, never widened; farview's without the stencil):
//   covered = samples whose colour differs from the frame before the strips on either side (must be >= 1000);
//   flips = samples with any half past 2 ulp (an edge or a depth tie on one side only);
//   EXACT = no half differs; BOUND = flips <= 0.05 % of covered, halves past 1 ulp outside flips <= 0.1 % of the covered
//   halves. Anything else FAILS.
// Reds on the palette case (each must FAIL): red 1 no blend, red 2 the palette off, red 4 u and v swapped.
// Last line `BOLTVIEW GATE OK` or `BOLTVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include "features/vkframe/vkbolt.h"

#include <random>

namespace boltv
{
using namespace fv;
struct Va
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * compressedTextureSubImage2D )( GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void * ) = nullptr;
	void( WINAPI * textureParameteri )( GLuint, GLenum, GLint ) = nullptr;
};
Va va;

// the GL state calls boltStateOf makes, as drawPreview's Qt functions name them
struct StateGl
{
	static GLboolean glIsEnabled( GLenum c ) { return ::glIsEnabled( c ); }
	static void glGetIntegerv( GLenum p, GLint * v ) { ::glGetIntegerv( p, v ); }
	static void glGetBooleanv( GLenum p, GLboolean * v ) { ::glGetBooleanv( p, v ); }
};
StateGl sg;

const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";

struct Verdict
{
	size_t covered = 0, flips = 0, past1 = 0, diff = 0;
	int maxu = 0;
	bool ok() const { return covered >= 1000 && flips * 2000 <= covered && past1 * 1000 <= covered * 4; }
	const char * word() const { return covered < 1000 ? "FAIL" : diff == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

Verdict judge( const std::vector<std::uint16_t> & bg, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b )
{
	Verdict v;
	if ( a.size() != b.size() || a.size() != bg.size() || a.empty() )
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
		if ( !cov )
			continue;
		v.covered++;
		v.diff += size_t( df );
		if ( m > 2 )
			v.flips++;
		else
			v.past1 += size_t( p1 );
		v.maxu = std::max( v.maxu, m > 2 ? 0 : m );
	}
	return v;
}

std::uint16_t rgb565( int r, int g, int b )
{
	return std::uint16_t( ( ( r & 31 ) << 11 ) | ( ( g & 63 ) << 5 ) | ( b & 31 ) );
}

// a BC3 texture-cache twin, every block its own colour and alpha endpoints and indices, x and y weighted differently
bool bc3Twin( int levels, unsigned & tex, std::string & why )
{
	if ( !WwVkPresent::objTexMake( 0x83F3, 64, 64, levels, tex, why ) )
		return false;
	for ( int l = 0; l < levels; l++ ) {
		const int bw = 16 >> l;
		std::vector<std::uint8_t> blocks( std::size_t( bw ) * bw * 16, 0 );
		for ( int by = 0; by < bw; by++ )
			for ( int bx = 0; bx < bw; bx++ ) {
				std::uint8_t * b = &blocks[( std::size_t( by ) * bw + bx ) * 16];
				b[0] = std::uint8_t( 30 + ( bx * 37 + by * 11 + l * 5 ) % 200 );
				b[1] = std::uint8_t( 250 - ( bx * 13 + by * 29 ) % 180 );
				for ( int k = 2; k < 8; k++ )
					b[k] = std::uint8_t( bx * 7 + by * 3 + k * 29 + l * 17 );
				const std::uint16_t c0 = rgb565( bx * 5 + l * 3, by * 9 + bx, bx * by + 7 ), c1 = rgb565( by * 3, bx * 11 + 5, by * 5 + 3 );
				b[8] = std::uint8_t( c0 & 0xFF );
				b[9] = std::uint8_t( c0 >> 8 );
				b[10] = std::uint8_t( c1 & 0xFF );
				b[11] = std::uint8_t( c1 >> 8 );
				for ( int k = 12; k < 16; k++ )
					b[k] = std::uint8_t( bx * 17 + by * 5 + k * 61 + l * 3 );
			}
		const int lw = 64 >> l;
		va.compressedTextureSubImage2D( tex, l, 0, 0, lw, lw, 0x83F3, GLsizei( blocks.size() ), blocks.data() );
	}
	va.textureParameteri( tex, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
	va.textureParameteri( tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	va.textureParameteri( tex, GL_TEXTURE_WRAP_S, GL_REPEAT );
	va.textureParameteri( tex, GL_TEXTURE_WRAP_T, GL_REPEAT );
	if ( !WwVkPresent::objTexKey( tex ) ) {
		why = "the BC3 twin has no texture-cache key";
		return false;
	}
	return true;
}
}  // namespace boltv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace boltv;
	gTag = "BOLTVIEW";
	if ( argc < 3 ) {
		std::printf( "usage: boltview <out dir> <tree>\nBOLTVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[2];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "BOLTVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 320, H = 180, WS = W * S;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwboltview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwboltview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwboltview", "bolt view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
		|| !load( va.vertexAttribPointer, "glVertexAttribPointer" ) || !load( va.enableVertexAttribArray, "glEnableVertexAttribArray" )
		|| !load( va.compressedTextureSubImage2D, "glCompressedTextureSubImage2D" ) || !load( va.textureParameteri, "glTextureParameteri" ) )
		return fatal( "GL entry points" );
	std::printf( "BOLTVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	// the tree's program, uniforms.glsl in place of its #include (as the app's loader)
	std::string vsrc = readFile( root + "/res/shaders/boltstrip.vert" );
	const std::string inc = "#include \"uniforms.glsl\"";
	const size_t ip = vsrc.find( inc );
	if ( ip == std::string::npos )
		return fatal( "boltstrip.vert has no #include \"uniforms.glsl\"" );
	vsrc.replace( ip, inc.size(), readFile( root + "/res/shaders/uniforms.glsl" ) );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pSlab = program( kTriVs, kSlabFs, "slab" );
	const GLuint pBolt = program( vsrc, readFile( root + "/res/shaders/boltstrip.frag" ), "boltstrip" );
	if ( !pUnpack || !pSlab || !pBolt )
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

	// ---- the textures: two BC3 twins, the white, the palette (8 rows, linear, clamp)
	unsigned bc3Mip = 0, bc3One = 0;
	if ( !bc3Twin( 3, bc3Mip, why ) || !bc3Twin( 1, bc3One, why ) )
		return fatal( "BC3 twin: " + why );
	std::vector<std::uint8_t> white( 16 * 4, 255 ), pal( 64 * 8 * 4 );
	for ( int y = 0; y < 8; y++ )
		for ( int x = 0; x < 64; x++ ) {
			std::uint8_t * p = &pal[( std::size_t( y ) * 64 + x ) * 4];
			p[0] = std::uint8_t( x * 4 );
			p[1] = std::uint8_t( 255 - x * 3 + y * 7 );
			p[2] = std::uint8_t( y * 30 + x );
			p[3] = std::uint8_t( 40 + x * 3 );
		}
	const GLuint whiteTex = tex2d( 4, 4, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, white.data(), true );
	const GLuint palTex = tex2d( 64, 8, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, pal.data(), true );

	// ---- the global block: a real asymmetric projection at 96 (std140: two mat3 before it), the rest filled but unread
	const float n0 = 0.5f, f0 = 50.0f, l0 = -0.42f, r0 = 0.38f, b0 = -0.30f, t0 = 0.28f;
	float proj[16] = {};
	proj[0] = 2.0f * n0 / ( r0 - l0 );
	proj[5] = 2.0f * n0 / ( t0 - b0 );
	proj[8] = ( r0 + l0 ) / ( r0 - l0 );
	proj[9] = ( t0 + b0 ) / ( t0 - b0 );
	proj[10] = -( f0 + n0 ) / ( f0 - n0 );
	proj[11] = -1.0f;
	proj[14] = -2.0f * f0 * n0 / ( f0 - n0 );
	std::vector<std::uint8_t> global( WwVkPresent::kFxGlobalBytes, 0 );
	for ( size_t k = 0; k < 96; k += 4 ) {
		const float junk = 0.125f * float( k % 7 ) - 0.3f;
		std::memcpy( &global[k], &junk, 4 );
	}
	std::memcpy( &global[96], proj, 64 );
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );
	f.bindBuffer( fUbo, ubo );
	f.bufferData( fUbo, std::ptrdiff_t( global.size() ), global.data(), fStatic );
	const GLuint bi = f.getUniformBlockIndex( pBolt, "globalUniforms" );
	if ( bi == 0xFFFFFFFFu )
		return fatal( "boltstrip has no globalUniforms block" );
	f.uniformBlockBinding( pBolt, bi, 0 );

	// ---- the modelView (GL columns): view = R * model + tr
	const float ay = 0.3f, ax = -0.2f;
	const float Ry[3][3] = { { std::cos( ay ), 0.0f, std::sin( ay ) }, { 0.0f, 1.0f, 0.0f }, { -std::sin( ay ), 0.0f, std::cos( ay ) } };
	const float Rx[3][3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, std::cos( ax ), -std::sin( ax ) }, { 0.0f, std::sin( ax ), std::cos( ax ) } };
	float R[3][3] = {};
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			for ( int k = 0; k < 3; k++ )
				R[i][j] += Ry[i][k] * Rx[k][j];
	const float tr[3] = { 0.4f, -0.3f, -1.0f };
	float mv[16] = {};
	for ( int c = 0; c < 3; c++ )
		for ( int r = 0; r < 3; r++ )
			mv[c * 4 + r] = R[r][c];
	for ( int r = 0; r < 3; r++ )
		mv[12 + r] = tr[r];
	mv[15] = 1.0f;

	// ---- the strips: camera-facing ribbons at distinct view depths (3 quads each), model space = R^T ( view - tr )
	const int strips = 40, segs = 3;
	std::mt19937 rng( 20261116u );
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<float> pos, col, uvs, zs;
	const float focal = proj[5] * 0.5f * float( H );
	for ( int i = 0; i < strips; i++ ) {
		const float z = -( 3.0f + 9.0f * ( float( i ) + 0.2f + 0.6f * u01( rng ) ) / float( strips ) );
		const float d = -z;
		const float cx = ( l0 + ( r0 - l0 ) * ( 0.1f + 0.8f * u01( rng ) ) ) * d / n0, cy = ( b0 + ( t0 - b0 ) * ( 0.1f + 0.8f * u01( rng ) ) ) * d / n0;
		const float th = 6.2831853f * u01( rng ), dx = std::cos( th ), dy = std::sin( th );
		const float len = ( 60.0f + 60.0f * u01( rng ) ) * d / focal, hw = ( 4.0f + 8.0f * u01( rng ) ) * d / focal;
		const float u0 = -0.7f + 1.2f * u01( rng ), span = 0.8f + 1.7f * u01( rng ), v0 = -0.3f + 0.2f * u01( rng ), v1 = 1.1f + 0.3f * u01( rng );
		const float base[3] = { 0.2f + 1.0f * u01( rng ), 0.2f + 1.0f * u01( rng ), 0.2f + 1.0f * u01( rng ) };
		{
			const float cz = proj[10] * z + proj[14], cw = d;
			zs.push_back( ( cz / cw ) * 0.5f + 0.5f );
		}
		for ( int s = 0; s < segs; s++ ) {
			const float a0 = len * ( float( s ) / segs - 0.5f ), a1 = len * ( float( s + 1 ) / segs - 0.5f );
			// corners A B (segment start, -/+ width), C D (segment end)
			const float vx[4] = { cx + dx * a0 + dy * hw, cx + dx * a0 - dy * hw, cx + dx * a1 + dy * hw, cx + dx * a1 - dy * hw };
			const float vy[4] = { cy + dy * a0 - dx * hw, cy + dy * a0 + dx * hw, cy + dy * a1 - dx * hw, cy + dy * a1 + dx * hw };
			const float cu[4] = { u0 + span * s / segs, u0 + span * s / segs, u0 + span * ( s + 1 ) / segs, u0 + span * ( s + 1 ) / segs };
			const float cv[4] = { v0, v1, v0, v1 };
			float ca[4];
			for ( float & a : ca )
				a = 0.15f + 0.8f * u01( rng );
			const int tri[6] = { 0, 2, 1, 1, 2, 3 }, triCw[6] = { 0, 1, 2, 1, 3, 2 };
			for ( int k = 0; k < 6; k++ ) {
				const int c = ( i & 1 ) ? triCw[k] : tri[k];
				const float v[3] = { vx[c] - tr[0], vy[c] - tr[1], z - tr[2] };
				for ( int j = 0; j < 3; j++ )
					pos.push_back( R[0][j] * v[0] + R[1][j] * v[1] + R[2][j] * v[2] );
				for ( int j = 0; j < 3; j++ )
					col.push_back( base[j] * ( 0.6f + 0.4f * float( c ) / 3.0f ) );
				col.push_back( ca[c] );
				uvs.push_back( cu[c] );
				uvs.push_back( cv[c] );
			}
		}
	}
	const int nv = int( pos.size() / 3 );
	std::sort( zs.begin(), zs.end() );
	const size_t m = zs.size() / 2;
	const float zd = 0.5f * ( zs[m - 1] + zs[m] );
	std::printf( "BOLTVIEW job: %dx%d x%d, %d strips, %d vertices, slab depth %.6f (between %.6f and %.6f)\n", W, H, S, strips, nv, zd,
		zs[m - 1], zs[m] );

	GLuint vao = 0, vb[3] = {};
	va.genVertexArrays( 1, &vao );
	va.bindVertexArray( vao );
	f.genBuffers( 3, vb );
	const std::vector<float> * arr[3] = { &pos, &col, &uvs };
	const int comps[3] = { 3, 4, 2 };
	for ( int k = 0; k < 3; k++ ) {
		f.bindBuffer( 0x8892, vb[k] );  // GL_ARRAY_BUFFER
		f.bufferData( 0x8892, std::ptrdiff_t( arr[k]->size() * 4 ), arr[k]->data(), fStatic );
		va.vertexAttribPointer( GLuint( k ), comps[k], GL_FLOAT, GL_FALSE, 0, nullptr );
		va.enableVertexAttribArray( GLuint( k ) );
	}
	va.bindVertexArray( 0 );

	struct Case
	{
		const char * name;
		GLuint base, pal;
		bool cull;
	};
	const Case cases[4] = { { "palette", bc3Mip, palTex, false }, { "plain", whiteTex, 0, false }, { "single", bc3One, 0, false },
		{ "cull", bc3Mip, palTex, true } };
	// drawPreview's program state and GL state for a case (the scene's depth func LEQUAL)
	auto bindCase = [&]( const Case & c, GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		f.useProgram( pBolt );
		f.uniformMatrix4fv( f.getUniformLocation( pBolt, "modelViewMatrix" ), 1, GL_FALSE, mv );
		u1i( pBolt, "boltTexture", 0 );
		u1i( pBolt, "GreyscaleMap", 1 );
		bindUnit( 0, GL_TEXTURE_2D, c.base );
		bindUnit( 1, GL_TEXTURE_2D, c.pal );
		u1i( pBolt, "greyscaleColor", c.pal ? 1 : 0 );
		u1i( pBolt, "greyscaleAlpha", c.pal ? 1 : 0 );
		const float row = c.pal ? 0.37f : 1.0f;
		f.uniform1fv( f.getUniformLocation( pBolt, "paletteRow" ), 1, &row );
		f.bindBufferBase( fUbo, 0, ubo );
		drawState0();
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_FALSE );
		if ( c.cull ) {
			glEnable( GL_CULL_FACE );
			glCullFace( GL_BACK );
			glFrontFace( GL_CCW );
		}
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
	// the frame before the strips: sky, then the slab's depth over the left half
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
	std::vector<std::uint16_t> bg, ref[4];
	before( fbRef );
	bg = readSamples( refCol );
	for ( int c = 0; c < 4; c++ ) {
		before( fbRef );
		bindCase( cases[c], fbRef );
		va.bindVertexArray( vao );
		glDrawArrays( GL_TRIANGLES, 0, nv );
		va.bindVertexArray( 0 );
		drawState0();
		glDepthFunc( GL_LESS );
		ref[c] = readSamples( refCol );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( std::string( "the GL reference draw (" ) + cases[c].name + ")" );
	}
	{
		const Verdict a = judge( bg, ref[0], ref[0] ), b = judge( bg, ref[3], ref[3] );
		std::printf( "BOLTVIEW GL: palette covers %zu samples, cull %zu (the odd strips culled: %s)\n", a.covered, b.covered,
			b.covered < a.covered ? "yes" : "NO" );
		if ( b.covered >= a.covered )
			return fatal( "GL's cull case culls nothing" );
	}
	static char twinKeys[2];
	auto runVk = [&]( int ci, int red, std::vector<std::uint16_t> & a ) -> bool {
		const Case & c = cases[ci];
		before( fbFrame );
		bindCase( c, fbFrame );
		static WwVkPresent::BoltRun t;
		t = WwVkPresent::BoltRun();
		t.w = W;
		t.h = H;
		glGetIntegerv( GL_VIEWPORT, t.vp );
		t.color = fc;
		t.n = nv;
		t.pos = pos.data();
		t.col = col.data();
		t.uv = uvs.data();
		std::memcpy( t.global, global.data(), sizeof( t.global ) );
		WwVkPresent::fxPack( &ga, pBolt, WwVkPresent::kT_boltVs, int( std::size( WwVkPresent::kT_boltVs ) ), t.vsLoose );
		WwVkPresent::fxPack( &ga, pBolt, WwVkPresent::kT_boltPs, int( std::size( WwVkPresent::kT_boltPs ) ), t.psLoose );
		const char * smp[2] = { "boltTexture", "GreyscaleMap" };
		for ( int k = 0; k < 2; k++ ) {
			if ( !WwVkPresent::boltTexOf( &ga, pBolt, smp[k], t.tex[k], why ) ) {
				std::printf( "BOLTVIEW boltTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[k].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[k] );
		}
		if ( !WwVkPresent::boltStateOf( &sg, t, why ) ) {
			std::printf( "BOLTVIEW boltStateOf refused: %s\n", why.c_str() );
			return false;
		}
		t.red = red;
		f.bindFramebuffer( fFb, fbFrame );
		if ( !WwVkPresent::boltRun( t, why ) ) {
			std::printf( "BOLTVIEW boltRun refused: %s\n", why.c_str() );
			return false;
		}
		drawState0();
		glDepthFunc( GL_LESS );
		a = readSamples( fc );
		return glGetError() == GL_NO_ERROR;
	};
	bool ok = true;
	for ( int c = 0; c < 4; c++ ) {
		std::vector<std::uint16_t> a;
		if ( !runVk( c, 0, a ) ) {
			std::printf( "BOLTVIEW %s FAIL (the Vulkan run failed)\n", cases[c].name );
			ok = false;
			continue;
		}
		const Verdict v = judge( bg, a, ref[c] );
		std::printf( "BOLTVIEW %s %s (%zu samples covered, %zu halves differ, %zu edge flips, %zu halves past 1 ulp outside them, "
			"max %d ulp outside them)\n", cases[c].name, v.word(), v.covered, v.diff, v.flips, v.past1, v.maxu );
		ok = ok && v.ok();
	}
	{
		const WwVkPresent::FxStat st = WwVkPresent::takeBoltStat();
		std::printf( "BOLTVIEW stat: %d runs, %d pipelines built in %.1f ms, %d input copies, %d refused (%s)\n", st.runs, st.pipelines,
			st.buildMs, st.copies, st.fails, st.failWhy.c_str() );
	}
	const struct
	{
		const char * name;
		int red;
	} reds[3] = { { "blend (none)", 1 }, { "palette (off)", 2 }, { "uv (u and v swapped)", 4 } };
	for ( const auto & r : reds ) {
		std::vector<std::uint16_t> a;
		if ( !runVk( 0, r.red, a ) ) {
			std::printf( "BOLTVIEW red %s: the run failed (counts as a FAIL)\n", r.name );
			continue;
		}
		const Verdict v = judge( bg, a, ref[0] );
		std::printf( "BOLTVIEW red %s: %s (%zu covered, %zu halves differ, %zu flips, %zu past 1 ulp)\n", r.name, v.word(), v.covered,
			v.diff, v.flips, v.past1 );
		if ( v.ok() ) {
			std::printf( "BOLTVIEW red %s PASSED: the gate cannot see it\n", r.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeBoltStat();
	std::printf( "BOLTVIEW stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f)\n", st.runs, st.fails,
		st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	for ( char & k : twinKeys )
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "BOLTVIEW GATE OK" : "BOLTVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
