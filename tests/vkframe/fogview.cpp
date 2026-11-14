// Lane VKFAR2 (2026-11-14, the froxel item; plan rows L5, P4b, L8): the air's integrate on the present device (vkfog.inc
// fogIntegRun: volair_integ_cs.hlsl over the shared inject into the shared integrated volume, the shipped code built in
// here through fxview.cpp with WW_VKPRESENT_NO_QT) against GL's own integrate: the app's cell_volair program (written by
// fogview_prep.py under the app's two switch sets) replayed with cellvolair.cpp's exact draw loops: stage 3 (7 layers a
// draw into the MRT, the running (L, T) in the RGBA32F carry ping-pong), red oldint's stage 2 (every layer re-summed), the
// far grid's stage 2 at volLayerBase = the near slices. Every non-pass uniform is seeded by type from its own stream
// (seed ^ FNV-1a of its name; floats in [0.05, 1), ints and uints 0), every sampler on its own unit with nothing bound;
// then the pass's own (volStage, volLayer, volCount, volLayerBase, volInject, volCarry).
// The inject is the inject GL draws in the app (unchanged by this lane: its inputs are GL's): seeded data of its format
// (RGBA32F; rgb 0 / [0, 2) / [0, 40), a 0 / [0, 0.6) / [0.6, 1) / 1) uploaded into the SHARED inject both sides read.
// Arms (one volume each; both integrated volumes start at the sentinel 7 everywhere, so the grids' bounds are judged):
//   a1 rt     near 23x17x64 + far 19x13x32, RGBA16F, stage 3, two crossings (the app's: near, then far)
//   a2 rt     near 31x9x96 + far 40x22x24 (the far grid wider: the volume's W/H are its), RGBA16F, stage 3, one crossing
//   a3 plain  near 17x29x128, no far grid, RGBA16F, GL's stage 2 (red oldint)
//   a4 plain  near 23x17x64 + far 19x13x32, RGBA32F (the f32 red's volume), stage 3, two crossings
//   a5 rt     near 8x8x7 + far 9x5x5 (one group edge, one stage-3 draw), RGBA16F, stage 3, two crossings
// Bars (set before the first run, never widened): written = the grids' texels hold no sentinel on either side and every
// texel outside them still holds it, on both sides; RGBA16F: every half within 1 ulp of GL's and <= 0.1 % of halves
// differ at all; RGBA32F: |vk - gl| <= 1e-5 max(1, |gl|) on every value; no NaN / Inf on either side.
// Reds (each must FAIL the GL bar; a1, then a4): volair_integ_cs.hlsl's red bits 1 no transmittance, 2 T before L,
// 4 read one layer late, 8 layer 1's light dropped. Dumps for fogview_check.py (the numpy rebuild): <out>/<arm>/inject.bin,
// gl.bin, vk.bin, vk_red<r>.bin (float32, layer, row, column, RGBA) + meta.txt.
// Last line `FOGVIEW GATE OK` or `FOGVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>

namespace fg
{
using namespace fv;

struct Ex
{
	void( WINAPI * framebufferTextureLayer )( GLenum, GLenum, GLuint, GLint, GLint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
};
Ex ex;

constexpr GLenum kArrayBuf = 0x8892, kStaticDraw = 0x88E4;
constexpr int kInjectUnit = 1, kCarryUnit = 2, kFirstSeedUnit = 3;
constexpr float kSentinel = 7.0f;

std::uint32_t fnv( const std::string & s )
{
	std::uint32_t h = 2166136261u;
	for ( unsigned char c : s ) {
		h ^= c;
		h *= 16777619u;
	}
	return h;
}

// every non-block uniform seeded by type from its own stream; samplers on their own units (nothing bound: GL reads 0)
int seedUnis( GLuint p, unsigned seed, int & samplers )
{
	f.useProgram( p );
	GLint n = 0;
	f.getProgramiv( p, fActiveUnis, &n );
	int set = 0, unit = kFirstSeedUnit;
	samplers = 0;
	for ( GLint i = 0; i < n; i++ ) {
		char nm[256] = {};
		GLint sz = 0;
		GLenum ty = 0;
		f.getActiveUniform( p, GLuint( i ), 256, nullptr, &sz, &ty, nm );
		const GLuint idx = GLuint( i );
		GLint bi = -1;
		f.getActiveUniformsiv( p, 1, &idx, fBlockIndex, &bi );
		if ( bi >= 0 )
			continue;
		std::string base( nm );
		if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
			base.resize( base.size() - 3 );
		const GLint l = f.getUniformLocation( p, nm );
		if ( l < 0 )
			continue;
		std::mt19937 rng( seed ^ fnv( base ) );
		std::uniform_real_distribution<float> u01( 0.05f, 1.0f );
		int comp = 0;
		char kind = 0;
		switch ( ty ) {
		case 0x1406: kind = 'f'; comp = 1; break;
		case 0x8B50: kind = 'f'; comp = 2; break;
		case 0x8B51: kind = 'f'; comp = 3; break;
		case 0x8B52: kind = 'f'; comp = 4; break;
		case 0x1404: case 0x8B56: kind = 'i'; comp = 1; break;
		case 0x8B53: case 0x8B57: kind = 'i'; comp = 2; break;
		case 0x8B54: case 0x8B58: kind = 'i'; comp = 3; break;
		case 0x8B55: case 0x8B59: kind = 'i'; comp = 4; break;
		case 0x1405: kind = 'u'; comp = 1; break;
		case 0x8B5B: kind = '3'; comp = 9; break;
		case 0x8B5C: kind = '4'; comp = 16; break;
		case 0x8DC6: case 0x8DC7: case 0x8DC8: case 0x8B5A: continue;  // uvec2-4, mat2: 0 by default (none read by stages 2 / 3)
		default: {  // a sampler: its own unit
			std::vector<GLint> u( size_t( std::max( sz, 1 ) ) );
			for ( GLint & x : u )
				x = unit++;
			f.uniform1iv( l, GLsizei( u.size() ), u.data() );
			samplers += int( u.size() );
			continue;
		}
		}
		const size_t total = size_t( std::max( sz, 1 ) ) * size_t( comp );
		std::vector<float> fv( total );
		std::vector<GLint> iv( total, 0 );
		for ( float & x : fv )
			x = u01( rng );
		const GLsizei cnt = GLsizei( std::max( sz, 1 ) );
		switch ( kind ) {
		case 'f': ( comp == 1 ? f.uniform1fv : comp == 2 ? f.uniform2fv : comp == 3 ? f.uniform3fv : f.uniform4fv )( l, cnt, fv.data() ); break;
		case 'i': ( comp == 1 ? f.uniform1iv : comp == 2 ? f.uniform2iv : comp == 3 ? f.uniform3iv : f.uniform4iv )( l, cnt, iv.data() ); break;
		case 'u': f.uniform1uiv( l, cnt, reinterpret_cast<const GLuint *>( iv.data() ) ); break;
		case '3': f.uniformMatrix3fv( l, cnt, GL_FALSE, fv.data() ); break;
		default: f.uniformMatrix4fv( l, cnt, GL_FALSE, fv.data() ); break;
		}
		set++;
	}
	return set;
}

struct Arm
{
	const char * name;
	int set;  // 0 rt, 1 plain
	int cols, rows, slices, fcols, frows, fslices;
	bool f32, oldint, oneCall;
	unsigned seed;
};

std::vector<float> injectData( int W, int H, int D, unsigned seed )
{
	std::mt19937 rng( seed * 7919u + 13u );
	std::uniform_real_distribution<float> u( 0.0f, 1.0f );
	std::vector<float> v( size_t( W ) * H * D * 4 );
	for ( size_t i = 0; i < v.size(); i += 4 ) {
		const float pc = u( rng ), pa = u( rng );
		for ( int c = 0; c < 3; c++ )
			v[i + size_t( c )] = pc < 0.2f ? 0.0f : pc < 0.9f ? 2.0f * u( rng ) : 40.0f * u( rng );
		v[i + 3] = pa < 0.05f ? 0.0f : pa < 0.2f ? 0.6f * u( rng ) : pa < 0.8f ? 0.6f + 0.4f * u( rng ) : 1.0f;
	}
	return v;
}

void upload3d( GLuint t, int W, int H, int D, const float * data )
{
	glBindTexture( fT3D, t );
	f.texSubImage3D( fT3D, 0, 0, 0, 0, W, H, D, GL_RGBA, GL_FLOAT, data );
	glBindTexture( fT3D, 0 );
}

std::vector<float> read3d( GLuint t, int W, int H, int D, bool f32, std::vector<std::uint16_t> * halves )
{
	std::vector<float> out( size_t( W ) * H * D * 4 );
	glBindTexture( fT3D, t );
	if ( f32 ) {
		glGetTexImage( fT3D, 0, GL_RGBA, GL_FLOAT, out.data() );
	} else {
		std::vector<std::uint16_t> hv( out.size() );
		glGetTexImage( fT3D, 0, GL_RGBA, fHalf, hv.data() );
		for ( size_t i = 0; i < hv.size(); i++ )
			out[i] = h2f( hv[i] );
		if ( halves )
			*halves = hv;
	}
	glBindTexture( fT3D, 0 );
	return out;
}

void writeBin( const std::string & path, const std::vector<float> & v )
{
	if ( FILE * o = std::fopen( path.c_str(), "wb" ) ) {
		std::fwrite( v.data(), sizeof( float ), v.size(), o );
		std::fclose( o );
	}
}

bool inGrid( const Arm & a, int x, int y, int z )
{
	if ( z < a.slices )
		return x < a.cols && y < a.rows;
	return z < a.slices + a.fslices && x < a.fcols && y < a.frows;
}

struct Verdict
{
	size_t texels = 0, inside = 0, sentinelIn = 0, writtenOut = 0, values = 0, diff = 0, past1 = 0, bad = 0, nonFinite = 0;
	int maxu = 0;
	double maxRel = 0.0;
	bool ok = false;
};

int ulps( std::uint16_t a, std::uint16_t b )
{
	auto ord = []( std::uint16_t h ) { return ( h & 0x8000 ) ? -int( h & 0x7FFF ) : int( h ); };
	return std::abs( ord( a ) - ord( b ) );
}

Verdict judge( const Arm & a, int W, int H, int D, const std::vector<float> & gl, const std::vector<float> & vk,
	const std::vector<std::uint16_t> & hg, const std::vector<std::uint16_t> & hv )
{
	Verdict v;
	for ( int z = 0; z < D; z++ )
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t i = ( ( size_t( z ) * H + y ) * W + x ) * 4;
				const bool in = inGrid( a, x, y, z );
				v.texels++;
				v.inside += in ? 1 : 0;
				for ( int c = 0; c < 4; c++ ) {
					const float g = gl[i + size_t( c )], k = vk[i + size_t( c )];
					if ( !std::isfinite( g ) || !std::isfinite( k ) )
						v.nonFinite++;
				}
				const bool gS = gl[i] == kSentinel && gl[i + 1] == kSentinel && gl[i + 2] == kSentinel && gl[i + 3] == kSentinel;
				const bool kS = vk[i] == kSentinel && vk[i + 1] == kSentinel && vk[i + 2] == kSentinel && vk[i + 3] == kSentinel;
				if ( in && ( gS || kS ) )
					v.sentinelIn++;
				if ( !in && ( !gS || !kS ) )
					v.writtenOut++;
				for ( int c = 0; c < 4; c++ ) {
					v.values++;
					if ( a.f32 ) {
						const double g = gl[i + size_t( c )], k = vk[i + size_t( c )];
						const double rel = std::fabs( k - g ) / std::max( 1.0, std::fabs( g ) );
						v.maxRel = std::max( v.maxRel, rel );
						v.diff += k != g ? 1 : 0;
						v.bad += rel > 1e-5 ? 1 : 0;
					} else {
						const int u = ulps( hg[i + size_t( c )], hv[i + size_t( c )] );
						v.maxu = std::max( v.maxu, u );
						v.diff += u > 0 ? 1 : 0;
						v.past1 += u > 1 ? 1 : 0;
					}
				}
			}
	const bool numbers = a.f32 ? v.bad == 0 : ( v.maxu <= 1 && v.diff * 1000 <= v.values );
	v.ok = v.inside > 0 && v.sentinelIn == 0 && v.writtenOut == 0 && v.nonFinite == 0 && numbers;
	return v;
}

std::string say( const Arm & a, const Verdict & v )
{
	char b[512];
	if ( a.f32 )
		std::snprintf( b, sizeof( b ), "%zu texels, %zu in the grids: unwritten %zu, written outside %zu, non-finite %zu; values off %zu "
			"(past 1e-5 rel %zu), max rel %.3g", v.texels, v.inside, v.sentinelIn, v.writtenOut, v.nonFinite, v.diff, v.bad, v.maxRel );
	else
		std::snprintf( b, sizeof( b ), "%zu texels, %zu in the grids: unwritten %zu, written outside %zu, non-finite %zu; halves off %zu "
			"of %zu (%.4f %%), past 1 ulp %zu, max %d ulp", v.texels, v.inside, v.sentinelIn, v.writtenOut, v.nonFinite, v.diff, v.values,
			100.0 * double( v.diff ) / double( std::max<size_t>( v.values, 1 ) ), v.past1, v.maxu );
	return b;
}

GLuint gVbo = 0;

void quad()
{
	f.bindBuffer( kArrayBuf, gVbo );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	ex.enableVertexAttribArray( 0 );
	glDrawArrays( GL_TRIANGLES, 0, 6 );
}
}  // namespace fg

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace fg;
	gTag = "FOGVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: fogview <prep dir>\nFOGVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FOGVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, VW = 320, VH = 180;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfogview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfogview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, VW, VH };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfogview", "fog view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	if ( !loadAll() || !load( ex.framebufferTextureLayer, "glFramebufferTextureLayer" )
		|| !load( ex.vertexAttribPointer, "glVertexAttribPointer" ) || !load( ex.enableVertexAttribArray, "glEnableVertexAttribArray" ) )
		return fatal( "GL entry points" );
	std::printf( "FOGVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	std::string why;
	if ( !WwVkPresent::viewBegin( VW, VH, S, glProc, why ) )
		return fatal( "the view: " + why );
	GLuint prog[2] = {};
	const char * setName[2] = { "rt", "plain" };
	for ( int k = 0; k < 2; k++ ) {
		const std::string vs = readFile( dir + "/volair_" + setName[k] + ".vert.glsl" ), fs = readFile( dir + "/volair_" + setName[k] + ".frag.glsl" );
		prog[k] = program( vs, fs, setName[k] );
		if ( !prog[k] )
			return fatal( std::string( "the cell_volair program (" ) + setName[k] + ")" );
	}
	const float quadV[18] = { -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0 };
	f.genBuffers( 1, &gVbo );
	f.bindBuffer( kArrayBuf, gVbo );
	f.bufferData( kArrayBuf, sizeof( quadV ), quadV, kStaticDraw );
	GLuint fb = 0;
	f.genFramebuffers( 1, &fb );

	const Arm arms[5] = {
		{ "a1", 0, 23, 17, 64, 19, 13, 32, false, false, false, 1 },
		{ "a2", 0, 31, 9, 96, 40, 22, 24, false, false, true, 2 },
		{ "a3", 1, 17, 29, 128, 0, 0, 0, false, true, false, 3 },
		{ "a4", 1, 23, 17, 64, 19, 13, 32, true, false, false, 4 },
		{ "a5", 0, 8, 8, 7, 9, 5, 5, false, false, false, 5 },
	};
	static GLuint injKey = 0, integKey = 0;  // the shared targets' keys (their addresses), as the air pass keys g.inject / g.integ
	bool allOk = true;
	int redsFailed = 0, redsRun = 0;
	for ( const Arm & a : arms ) {
		const int W = std::max( a.cols, a.fcols ), H = std::max( a.rows, a.frows ), D = a.slices + a.fslices;
		const GLenum ifmt = a.f32 ? fRgba32f : fRgba16f;
		unsigned injTex = 0, vkTex = 0;
		if ( !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &injKey ), 1, W, H, D, fRgba32f, injTex, why )
			|| !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &integKey ), 1, W, H, D, ifmt, vkTex, why ) )
			return fatal( std::string( "shared volumes (" ) + a.name + "): " + why );
		GLuint glTex = 0, carry[2] = {};
		glGenTextures( 1, &glTex );
		glBindTexture( fT3D, glTex );
		f.texImage3D( fT3D, 0, GLint( ifmt ), W, H, D, 0, GL_RGBA, GL_FLOAT, nullptr );
		texParams( fT3D, false, false, false );
		glGenTextures( 2, carry );
		for ( GLuint c : carry ) {
			glBindTexture( GL_TEXTURE_2D, c );
			glTexImage2D( GL_TEXTURE_2D, 0, GLint( fRgba32f ), a.cols, a.rows, 0, GL_RGBA, GL_FLOAT, nullptr );
			texParams( GL_TEXTURE_2D, false, false, false );
		}
		glBindTexture( GL_TEXTURE_2D, 0 );
		const std::vector<float> inj = injectData( W, H, D, a.seed );
		const std::vector<float> sent( inj.size(), kSentinel );
		upload3d( injTex, W, H, D, inj.data() );
		upload3d( glTex, W, H, D, sent.data() );
		upload3d( vkTex, W, H, D, sent.data() );

		// ---- GL: cellvolair.cpp's integrate draws
		const GLuint p = prog[a.set];
		int samplers = 0;
		const int seeded = seedUnis( p, a.seed, samplers );
		auto ui = [&]( const char * n, int v ) {
			const GLint l = f.getUniformLocation( p, n );
			if ( l >= 0 )
				f.uniform1i( l, v );
		};
		ui( "volInject", kInjectUnit );
		ui( "volCarry", kCarryUnit );
		ui( "volLayerBase", 0 );
		bindUnit( kInjectUnit, fT3D, injTex );
		glDisable( GL_BLEND );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_CULL_FACE );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, a.cols, a.rows );
		int draws = 0;
		bool fbOk = true;
		if ( !a.oldint ) {
			ui( "volStage", 3 );
			for ( int k = 0, c = 0; k < a.slices; k += 7, c++ ) {
				const int n = std::min( 7, a.slices - k );
				GLenum bufs[8];
				for ( int i = 0; i < 7; i++ ) {
					ex.framebufferTextureLayer( fFb, fCol0 + GLenum( i ), i < n ? glTex : 0, 0, i < n ? k + i : 0 );
					bufs[i] = i < n ? fCol0 + GLenum( i ) : GLenum( GL_NONE );
				}
				f.framebufferTexture2D( fFb, fCol0 + 7, GL_TEXTURE_2D, carry[( c + 1 ) & 1], 0 );
				bufs[7] = fCol0 + 7;
				f.drawBuffers( 8, bufs );
				bindUnit( kCarryUnit, GL_TEXTURE_2D, carry[c & 1] );
				if ( k == 0 && f.checkFramebufferStatus( fFb ) != fComplete )
					fbOk = false;
				ui( "volLayer", k );
				ui( "volCount", n );
				quad();
				draws++;
			}
			bindUnit( kCarryUnit, GL_TEXTURE_2D, 0 );
			for ( int i = 1; i < 8; i++ )
				f.framebufferTexture2D( fFb, fCol0 + GLenum( i ), GL_TEXTURE_2D, 0, 0 );
			const GLenum first = fCol0;
			f.drawBuffers( 1, &first );
		} else {
			ui( "volStage", 2 );
			const GLenum first = fCol0;
			f.drawBuffers( 1, &first );
			for ( int k = 0; k < a.slices; k++ ) {
				ex.framebufferTextureLayer( fFb, fCol0, glTex, 0, k );
				if ( k == 0 && f.checkFramebufferStatus( fFb ) != fComplete )
					fbOk = false;
				ui( "volLayer", k );
				quad();
				draws++;
			}
		}
		if ( a.fslices ) {  // the far grid: stage 2 a layer at volLayerBase = the near slices
			ui( "volStage", 2 );
			ui( "volLayerBase", a.slices );
			glViewport( 0, 0, a.fcols, a.frows );
			const GLenum first = fCol0;
			f.drawBuffers( 1, &first );
			for ( int k = 0; k < a.fslices; k++ ) {
				ex.framebufferTextureLayer( fFb, fCol0, glTex, 0, a.slices + k );
				if ( k == 0 && f.checkFramebufferStatus( fFb ) != fComplete )
					fbOk = false;
				ui( "volLayer", k );
				quad();
				draws++;
			}
		}
		ex.framebufferTextureLayer( fFb, fCol0, 0, 0, 0 );
		f.bindFramebuffer( fFb, 0 );
		if ( !fbOk )
			return fatal( std::string( "GL's integrate framebuffer (" ) + a.name + ")" );

		// ---- Vulkan: fogIntegRun as the app calls it
		auto vkRun = [&]( int red, std::string & w ) {
			WwVkPresent::FogIntegRun t;
			t.inject = reinterpret_cast<std::uintptr_t>( &injKey );
			t.integ = reinterpret_cast<std::uintptr_t>( &integKey );
			t.injectTex = injTex;
			t.integTex = vkTex;
			t.f32 = a.f32;
			t.red = red;
			t.grid[0] = WwVkPresent::FogGrid { a.cols, a.rows, 0, a.slices };
			if ( a.fslices && a.oneCall )
				t.grid[1] = WwVkPresent::FogGrid { a.fcols, a.frows, a.slices, a.fslices };
			if ( !WwVkPresent::fogIntegRun( t, w ) )
				return false;
			if ( a.fslices && !a.oneCall ) {
				WwVkPresent::FogIntegRun u = t;
				u.grid[0] = WwVkPresent::FogGrid { a.fcols, a.frows, a.slices, a.fslices };
				if ( !WwVkPresent::fogIntegRun( u, w ) )
					return false;
			}
			return true;
		};
		std::string w;
		if ( !vkRun( 0, w ) )
			return fatal( std::string( "fogIntegRun refused (" ) + a.name + "): " + w );
		glFinish();
		std::vector<std::uint16_t> hg, hv;
		const std::vector<float> gl = read3d( glTex, W, H, D, a.f32, &hg ), vk = read3d( vkTex, W, H, D, a.f32, &hv );
		const Verdict v = judge( a, W, H, D, gl, vk, hg, hv );
		allOk = allOk && v.ok;
		std::printf( "FOGVIEW %s (%s, %dx%dx%d%s, %s, %s, %s): GL %d draws, %d uniforms seeded, %d samplers on own units; %s -> %s\n",
			a.name, setName[a.set], a.cols, a.rows, a.slices,
			a.fslices ? ( " + far " + std::to_string( a.fcols ) + "x" + std::to_string( a.frows ) + "x" + std::to_string( a.fslices ) ).c_str() : "",
			a.f32 ? "RGBA32F" : "RGBA16F", a.oldint ? "GL stage 2" : "GL stage 3", a.fslices ? ( a.oneCall ? "one crossing" : "two crossings" ) : "one crossing",
			draws, seeded, samplers, say( a, v ).c_str(), v.ok ? "ok" : "FAIL" );
		const std::string ad = dir + "/" + a.name;
		CreateDirectoryA( ad.c_str(), nullptr );
		writeBin( ad + "/inject.bin", inj );
		writeBin( ad + "/gl.bin", gl );
		writeBin( ad + "/vk.bin", vk );
		if ( FILE * m = std::fopen( ( ad + "/meta.txt" ).c_str(), "w" ) ) {
			std::fprintf( m, "tex %d %d %d\nnear %d %d %d\nfar %d %d %d\nf32 %d\nsentinel %g\nset %s\n", W, H, D, a.cols, a.rows, a.slices,
				a.fcols, a.frows, a.fslices, a.f32 ? 1 : 0, double( kSentinel ), setName[a.set] );
			std::fclose( m );
		}
		// ---- the reds: each must FAIL the GL bar
		if ( std::string( a.name ) == "a1" || std::string( a.name ) == "a4" ) {
			for ( int red : { 1, 2, 4, 8 } ) {
				upload3d( vkTex, W, H, D, sent.data() );
				std::string rw;
				if ( !vkRun( red, rw ) ) {
					std::printf( "FOGVIEW red %d (%s) REFUSED (%s): a red must fail, never refuse\n", red, a.name, rw.c_str() );
					allOk = false;
					continue;
				}
				glFinish();
				std::vector<std::uint16_t> hr;
				const std::vector<float> vr = read3d( vkTex, W, H, D, a.f32, &hr );
				const Verdict r = judge( a, W, H, D, gl, vr, hg, hr );
				redsRun++;
				redsFailed += r.ok ? 0 : 1;
				if ( r.ok )
					allOk = false;
				std::printf( "%s red %d must fail (%s): %s -> %s\n", r.ok ? "BAD" : "ok", red, a.name, say( a, r ).c_str(), r.ok ? "PASSED (gate defect)" : "failed" );
				if ( std::string( a.name ) == "a1" )
					writeBin( ad + "/vk_red" + std::to_string( red ) + ".bin", vr );
			}
		}
		glDeleteTextures( 1, &glTex );
		glDeleteTextures( 2, carry );
	}
	const WwVkPresent::FogStat st = WwVkPresent::takeFogStat();
	std::printf( "FOGVIEW vk fog: %d crossings, %d grid dispatches, %d refused (last: %s); %d pipelines built in %.1f ms; drawing "
		"thread %.3f ms a crossing, max %.3f\n", st.runs, st.dispatches, st.fails, st.failWhy.c_str(), st.pipelines, st.buildMs,
		st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	std::printf( "FOGVIEW reds: %d of %d failed\n", redsFailed, redsRun );
	allOk = allOk && redsRun == 8 && redsFailed == redsRun;
	std::printf( allOk ? "FOGVIEW GATE OK\n" : "FOGVIEW GATE FAILED\n" );
	return allOk ? 0 : 1;
}
