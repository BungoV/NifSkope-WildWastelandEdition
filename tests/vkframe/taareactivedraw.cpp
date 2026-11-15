// Lane VKFAR7 (2026-11-15): the TAA mask's reactive pass (taainputs.cpp pass 4) on the present device (vktaareactive.inc
// taReactiveAdd / taReactiveFlush: the taareactive_view twins, fxWork's kind 11, N draws a crossing with per-draw host
// slices and sets) against GL's own taa_reactive.vert / .frag (read from the tree) in pass 4's state (drawBuffers NONE /
// ATT1, colour mask R G on ATT1, blend MAX ONE ONE, depth LEQUAL no writes, polygon offset (-1, -4), no cull), per pixel,
// on the same RGBA8 mask and D24S8 depth. fxview.cpp's helpers are reused (its main renamed). Each draw's BaseMap slot is
// read by the shipped waterCensusOf over GL's program, as the app's call site reads it.
// Fixture: 320 x 180; the mask seeded (R G 0..160, B A 0..255); the depth from two tilted opaque planes (depth LESS,
// writes on; stencil cleared 0x5A); 48 seeded draws cycling particle (vertex colour, some alpha 0: discards) / water
// (outRG 0 1) / textured (three base maps: RGBA8 64x64 mipmapped linear repeat, RGBA8 32x16 nearest clamp, BC3 64x64
// mipmapped; uvScale / uvOffset seeded) / glass (outRG 1 1) / colour only; one in four non-particles coplanar with a plane
// (only the polygon offset lets them pass LEQUAL), the rest at z -3..-14 (some behind the planes); draw 1 a big near
// particle quad (alpha 0.97); a perspective clip with a sub-pixel jitter.
// Bar (set before the first run, never widened):
//   covered = pixels whose R or G differs from the seed on either side; >= 3000.
//   B and A equal the seed on every pixel on both sides.
//   one-side coverage (R or G moved on one side only) <= 0.05 % of covered.
//   past1 = covered pixels where R or G differ by more than 1 level: <= 0.1 % of covered.
//   EXACT = no byte differs; BOUND = within the bar. Every queued draw accepted (0 refusals) and every flush ok.
//   The depth twin the flush read (taReactiveDepthTwin) equals the depth byte for byte.
//   Singles (maxPer 1: 48 crossings) and maxPer 5 (10 crossings) equal the one-crossing batch byte for byte.
//   Fixture checks (GL vs GL): GL twice EXACT; GL with the depth test off and GL with the polygon offset off each move
//   >= 200 pixels (the planes occlude, the coplanar draws exist).
// Reds (each must FAIL the bar against GL): red 1024 draw k binds draw k-1's set, red 2048 draw 1 of each crossing dropped.
// Last line `TAAREACTIVEDRAW GATE OK` or `TAAREACTIVEDRAW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>

namespace trd
{
using namespace fv;

//! GL's entry points under GL's own names (waterCensusOf calls fn->glX as on Qt's functions)
struct GlFn
{
	void( WINAPI * glGetProgramiv )( GLuint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetActiveUniform )( GLuint, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, char * ) = nullptr;
	void( WINAPI * glGetActiveUniformsiv )( GLuint, GLsizei, const GLuint *, GLenum, GLint * ) = nullptr;
	GLint( WINAPI * glGetUniformLocation )( GLuint, const char * ) = nullptr;
	void( WINAPI * glGetUniformiv )( GLuint, GLint, GLint * ) = nullptr;
	void( WINAPI * glActiveTexture )( GLenum ) = nullptr;
	void( WINAPI * glGetIntegerv )( GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexLevelParameteriv )( GLenum, GLint, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameteriv )( GLenum, GLenum, GLint * ) = nullptr;
	void( WINAPI * glGetTexParameterfv )( GLenum, GLenum, GLfloat * ) = nullptr;
	void( WINAPI * glGetAttachedShaders )( GLuint, GLsizei, GLsizei *, GLuint * ) = nullptr;
	void( WINAPI * glGetShaderiv )( GLuint, GLenum, GLint * ) = nullptr;
};
GlFn gw;

struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * disableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * blendEquation )( GLenum ) = nullptr;
	void( WINAPI * colorMaski )( GLuint, GLboolean, GLboolean, GLboolean, GLboolean ) = nullptr;
	void( WINAPI * compressedTexImage2D )( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void * ) = nullptr;
};
Ex ex;

template <class P> bool ldx( P & p, const char * n )
{
	p = reinterpret_cast<P>( glProc( n ) );
	if ( !p )
		std::printf( "TAAREACTIVEDRAW missing %s\n", n );
	return p != nullptr;
}

bool loadMore()
{
	bool ok = true;
#define TR_L( n ) ok = ldx( gw.n, #n ) && ok
	TR_L( glGetProgramiv ); TR_L( glGetActiveUniform ); TR_L( glGetActiveUniformsiv ); TR_L( glGetUniformLocation );
	TR_L( glGetUniformiv ); TR_L( glActiveTexture ); TR_L( glGetIntegerv ); TR_L( glGetTexLevelParameteriv );
	TR_L( glGetTexParameteriv ); TR_L( glGetTexParameterfv ); TR_L( glGetAttachedShaders ); TR_L( glGetShaderiv );
#undef TR_L
#define TR_X( m, n ) ok = ldx( ex.m, n ) && ok
	TR_X( genVertexArrays, "glGenVertexArrays" ); TR_X( bindVertexArray, "glBindVertexArray" );
	TR_X( vertexAttribPointer, "glVertexAttribPointer" ); TR_X( enableVertexAttribArray, "glEnableVertexAttribArray" );
	TR_X( disableVertexAttribArray, "glDisableVertexAttribArray" ); TR_X( blendEquation, "glBlendEquation" );
	TR_X( colorMaski, "glColorMaski" ); TR_X( compressedTexImage2D, "glCompressedTexImage2D" );
#undef TR_X
	return ok;
}

constexpr int W = 320, H = 180, kDraws = 48;
constexpr GLenum kArray = 0x8892, kElem = 0x8893, kMax = 0x8008, kBc3 = 0x83F3, kDepthStencil = 0x84F9, kU248 = 0x84FA;

struct Draw
{
	WwVkPresent::TaReactiveDraw d;
	GLuint tex = 0;
};

std::vector<Draw> gDraws;
GLuint gProg = 0, gVao = 0, gBuf[4] = {}, gFbo = 0, gMask = 0, gSeed = 0, gDepth = 0;
float gClip[16] = {};

GLint loc( const char * n )
{
	return f.getUniformLocation( gProg, n );
}

void glDraw( const Draw & x )
{
	const WwVkPresent::TaReactiveDraw & d = x.d;
	f.uniformMatrix4fv( loc( "clipJit" ), 1, GL_FALSE, d.clipJit );
	f.uniform2fv( loc( "uvScale" ), 1, d.uvScale );
	f.uniform2fv( loc( "uvOffset" ), 1, d.uvOffset );
	f.uniform1i( loc( "useTex" ), d.useTex );
	f.uniform1i( loc( "hasColor" ), d.hasColor );
	f.uniform1fv( loc( "matAlpha" ), 1, &d.matAlpha );
	f.uniform2fv( loc( "outRG" ), 1, d.outRG );
	bindUnit( 0, GL_TEXTURE_2D, x.tex );
	f.bindBuffer( kArray, gBuf[0] );
	f.bufferData( kArray, std::ptrdiff_t( d.pos.size() * 4 ), d.pos.data(), fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
	ex.enableVertexAttribArray( 0 );
	if ( d.col.empty() ) {
		ex.disableVertexAttribArray( 1 );
	} else {
		f.bindBuffer( kArray, gBuf[1] );
		f.bufferData( kArray, std::ptrdiff_t( d.col.size() * 4 ), d.col.data(), fStatic );
		ex.vertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, 0, nullptr );
		ex.enableVertexAttribArray( 1 );
	}
	if ( d.uv.empty() ) {
		ex.disableVertexAttribArray( 7 );
	} else {
		f.bindBuffer( kArray, gBuf[2] );
		f.bufferData( kArray, std::ptrdiff_t( d.uv.size() * 4 ), d.uv.data(), fStatic );
		ex.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 0, nullptr );
		ex.enableVertexAttribArray( 7 );
	}
	f.bindBuffer( kElem, gBuf[3] );
	f.bufferData( kElem, std::ptrdiff_t( d.idx.size() * 4 ), d.idx.data(), fStatic );
	glDrawElements( GL_TRIANGLES, GLsizei( d.idx.size() ), GL_UNSIGNED_INT, nullptr );
}

std::vector<std::uint8_t> readMask()
{
	std::vector<std::uint8_t> v( size_t( W ) * H * 4 );
	glBindTexture( GL_TEXTURE_2D, gMask );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, v.data() );
	return v;
}

std::vector<std::uint32_t> readDepth( GLuint t )
{
	std::vector<std::uint32_t> v( size_t( W ) * H, 0xDEADBEEFu );
	glBindTexture( GL_TEXTURE_2D, t );
	glGetTexImage( GL_TEXTURE_2D, 0, kDepthStencil, kU248, v.data() );
	return v;
}

void seedMask()
{
	f.copyImageSubData( gSeed, GL_TEXTURE_2D, 0, 0, 0, 0, gMask, GL_TEXTURE_2D, 0, 0, 0, 0, W, H, 1 );
}

// GL's pass 4 (depthTest / offset off: the fixture checks)
std::vector<std::uint8_t> glRun( bool depthTest, bool offset )
{
	seedMask();
	f.bindFramebuffer( fFb, gFbo );
	glViewport( 0, 0, W, H );
	drawState0();
	const GLenum two[2] = { GL_NONE, fCol0 + 1 };
	f.drawBuffers( 2, two );
	ex.colorMaski( 1, GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE );
	glEnable( GL_BLEND );
	ex.blendEquation( kMax );
	glBlendFunc( GL_ONE, GL_ONE );
	if ( depthTest ) {
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
	}
	if ( offset ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -4.0f );
	}
	f.useProgram( gProg );
	for ( const Draw & x : gDraws )
		glDraw( x );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_BLEND );
	ex.blendEquation( 0x8006 );  // FUNC_ADD
	ex.colorMaski( 1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	drawState0();
	return readMask();
}

struct VkOut
{
	std::vector<std::uint8_t> mask;
	bool ok = false;
	std::string why, refWhy;
	int refused = 0;
	WwVkPresent::TaReactiveStat st;
};

VkOut vkRun( int maxPer, int red )
{
	VkOut o;
	seedMask();
	WwVkPresent::takeTaReactiveStat();
	f.useProgram( gProg );
	f.uniform1i( loc( "BaseMap" ), 0 );
	for ( const Draw & x : gDraws ) {
		WwVkPresent::TaReactiveDraw d = x.d;
		if ( d.useTex ) {
			bindUnit( 0, GL_TEXTURE_2D, x.tex );
			WwVkPresent::WaterCensus c;
			WwVkPresent::waterCensusOf( &gw, gProg, c );
			bool found = false;
			for ( const WwVkPresent::WaterSlot & s : c.inputs )
				if ( s.name == "BaseMap" ) {
					d.base = s;
					found = true;
				}
			if ( !found || d.base.tex != x.tex ) {
				o.refused++;
				o.refWhy = "the census found no BaseMap on the draw's texture";
				continue;
			}
		}
		std::string w;
		if ( !WwVkPresent::taReactiveAdd( std::move( d ), w ) ) {
			o.refused++;
			o.refWhy = w;
		}
	}
	WwVkPresent::TaReactiveRun r;
	r.mask = gMask;
	r.depth = gDepth;
	r.dsFormat = fD24s8;
	r.w = W;
	r.h = H;
	r.maxPer = maxPer;
	r.red = red;
	o.ok = WwVkPresent::taReactiveFlush( r, o.why );
	o.st = WwVkPresent::takeTaReactiveStat();
	o.mask = readMask();
	return o;
}

struct Verdict
{
	size_t covered = 0, oneSide = 0, past1 = 0, one = 0, ba = 0, diff = 0;
	int maxd = 0;
	bool ok() const { return covered >= 3000 && ba == 0 && oneSide * 2000 <= covered && past1 * 1000 <= covered; }
	const char * word() const { return !ok() ? "FAIL" : diff == 0 ? "EXACT" : "BOUND"; }
};

Verdict judge( const std::vector<std::uint8_t> & ref, const std::vector<std::uint8_t> & got, const std::vector<std::uint8_t> & seed )
{
	Verdict v;
	for ( size_t p = 0; p < size_t( W ) * H; p++ ) {
		const std::uint8_t * a = &ref[p * 4], * b = &got[p * 4], * s = &seed[p * 4];
		if ( a[2] != s[2] || a[3] != s[3] || b[2] != s[2] || b[3] != s[3] )
			v.ba++;
		const bool ra = a[0] != s[0] || a[1] != s[1], rb = b[0] != s[0] || b[1] != s[1];
		if ( !ra && !rb )
			continue;
		v.covered++;
		v.oneSide += ra != rb;
		const int d = std::max( std::abs( int( a[0] ) - int( b[0] ) ), std::abs( int( a[1] ) - int( b[1] ) ) );
		v.diff += d != 0;
		v.one += d == 1;
		v.past1 += d > 1;
		v.maxd = std::max( v.maxd, d );
	}
	return v;
}

size_t moved( const std::vector<std::uint8_t> & a, const std::vector<std::uint8_t> & b )
{
	size_t n = 0;
	for ( size_t p = 0; p < a.size() / 4; p++ )
		n += std::memcmp( &a[p * 4], &b[p * 4], 4 ) != 0;
	return n;
}

// the scene ------------------------------------------------------------------------------------------------------
float zA( float x, float ) { return -8.0f + 0.3f * x; }   // plane A: x -10..0, y -6..6
float zB( float, float y ) { return -5.0f - 0.2f * y; }   // plane B: x -1..4, y -3..3

void quad( WwVkPresent::TaReactiveDraw & d, float cx, float cy, float hx, float hy, float ( *zf )( float, float ), float z0, float tz )
{
	const float xs[4] = { cx - hx, cx + hx, cx + hx, cx - hx }, ys[4] = { cy - hy, cy - hy, cy + hy, cy + hy };
	d.pos.clear();
	for ( int k = 0; k < 4; k++ ) {
		d.pos.push_back( xs[k] );
		d.pos.push_back( ys[k] );
		d.pos.push_back( zf ? zf( xs[k], ys[k] ) : z0 + tz * ( xs[k] - cx ) );
	}
	d.idx = { 0, 1, 2, 0, 2, 3 };
}

void buildScene( const GLuint texs[3] )
{
	std::mt19937 rng( 0x7AA3u );
	auto U = [&]( float a, float b ) { return std::uniform_real_distribution<float>( a, b )( rng ); };
	for ( int i = 0; i < kDraws; i++ ) {
		Draw x;
		WwVkPresent::TaReactiveDraw & d = x.d;
		std::memcpy( d.clipJit, gClip, sizeof( gClip ) );
		const int type = i == 1 ? 0 : i % 5;
		const bool coplanar = type != 0 && i % 4 == 2;
		if ( i == 1 ) {
			quad( d, -0.3f, 0.2f, 0.9f, 0.6f, nullptr, -2.6f, 0.0f );
		} else if ( coplanar && ( i / 4 ) % 2 == 0 ) {
			quad( d, U( -9.0f, -1.5f ), U( -4.5f, 4.5f ), U( 0.4f, 1.0f ), U( 0.4f, 1.2f ), &zA, 0.0f, 0.0f );
		} else if ( coplanar ) {
			quad( d, U( -0.4f, 3.2f ), U( -2.2f, 2.2f ), U( 0.3f, 0.7f ), U( 0.3f, 0.7f ), &zB, 0.0f, 0.0f );
		} else {
			const float z0 = U( -14.0f, -3.0f ), dd = -z0;
			quad( d, U( -0.9f, 0.9f ) * dd, U( -0.5f, 0.5f ) * dd, U( 0.04f, 0.2f ) * dd, U( 0.04f, 0.2f ) * dd, nullptr, z0,
				U( -0.3f, 0.3f ) );
		}
		auto colours = [&]( float a0, float a1 ) {
			d.col.clear();
			for ( int k = 0; k < 4; k++ ) {
				d.col.push_back( U( 0.0f, 1.0f ) );
				d.col.push_back( U( 0.0f, 1.0f ) );
				d.col.push_back( U( 0.0f, 1.0f ) );
				d.col.push_back( std::clamp( U( a0, a1 ), 0.0f, 1.0f ) );
			}
		};
		switch ( type ) {
		case 0:  // a particle system's sprites
			d.hasColor = 1;
			d.outRG[0] = 1.0f;
			colours( -0.3f, 1.0f );
			if ( i == 1 )
				for ( int k = 0; k < 4; k++ )
					d.col[size_t( k * 4 + 3 )] = 0.97f;
			break;
		case 1:  // water
			d.outRG[1] = 1.0f;
			break;
		case 2: {  // a textured see-through shape
			d.useTex = 1;
			x.tex = texs[( i / 5 ) % 3];
			d.uvScale[0] = U( 0.5f, 3.0f );
			d.uvScale[1] = U( 0.5f, 3.0f );
			d.uvOffset[0] = U( -1.0f, 1.0f );
			d.uvOffset[1] = U( -1.0f, 1.0f );
			const float k = U( 0.5f, 2.0f );
			d.uv = { 0.0f, 0.0f, k, 0.0f, k, k, 0.0f, k };
			d.hasColor = ( i / 5 ) % 2;
			if ( d.hasColor )
				colours( 0.3f, 1.0f );
			d.matAlpha = U( 0.4f, 1.0f );
			d.outRG[0] = 1.0f;
			d.outRG[1] = ( i / 5 ) % 3 == 0 ? 1.0f : 0.0f;
			break;
		}
		case 3:  // glass (lit see-through: composition too)
			d.outRG[0] = d.outRG[1] = 1.0f;
			d.matAlpha = U( 0.1f, 1.0f );
			break;
		default:  // colour only
			d.hasColor = 1;
			colours( 0.0f, 1.0f );
			d.matAlpha = U( 0.5f, 1.0f );
			d.outRG[0] = 1.0f;
			break;
		}
		gDraws.push_back( std::move( x ) );
	}
}
}  // namespace trd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace trd;
	gTag = "TAAREACTIVEDRAW";
	if ( argc < 2 ) {
		std::printf( "usage: taareactivedraw <tree root>\nTAAREACTIVEDRAW GATE FAILED\n" );
		return 2;
	}
	const std::string root = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "TAAREACTIVEDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwtaareactivedraw";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwtaareactivedraw", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwtaareactivedraw", "taa reactive harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf )
		return fatal( "no pixel format" );
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
	std::printf( "TAAREACTIVEDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const std::string sh = root + "/res/shaders/features/post/taa_reactive";
	gProg = program( readFile( sh + ".vert" ), readFile( sh + ".frag" ), "taa_reactive" );
	if ( !gProg )
		return fatal( "GL's taa_reactive program" );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, 1, glProc, why ) )
		return fatal( "the view: " + why );

	// ---- targets: the mask (and its seed), the depth
	std::mt19937 rng( 0x5EEDu );
	std::vector<std::uint8_t> seed( size_t( W ) * H * 4 );
	for ( size_t p = 0; p < size_t( W ) * H; p++ ) {
		seed[p * 4] = std::uint8_t( rng() % 161 );
		seed[p * 4 + 1] = std::uint8_t( rng() % 161 );
		seed[p * 4 + 2] = std::uint8_t( rng() & 255 );
		seed[p * 4 + 3] = std::uint8_t( rng() & 255 );
	}
	gSeed = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, seed.data(), false );
	gMask = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	gDepth = tex2d( W, H, fD24s8, kDepthStencil, kU248, nullptr, false );
	f.genFramebuffers( 1, &gFbo );
	f.bindFramebuffer( fFb, gFbo );
	f.framebufferTexture2D( fFb, fCol0 + 1, GL_TEXTURE_2D, gMask, 0 );
	f.framebufferTexture2D( fFb, fDsAtt, GL_TEXTURE_2D, gDepth, 0 );
	const GLenum two[2] = { GL_NONE, fCol0 + 1 };
	f.drawBuffers( 2, two );
	glReadBuffer( GL_NONE );
	if ( f.checkFramebufferStatus( fFb ) != fComplete )
		return fatal( "GL's mask framebuffer" );
	ex.genVertexArrays( 1, &gVao );
	ex.bindVertexArray( gVao );
	f.genBuffers( 4, gBuf );

	// ---- base maps
	GLuint texs[3] = {};
	{
		std::vector<std::uint8_t> a( 64 * 64 * 4 ), b( 32 * 16 * 4 );
		for ( auto & c : a )
			c = std::uint8_t( rng() & 255 );
		for ( auto & c : b )
			c = std::uint8_t( rng() & 255 );
		glGenTextures( 3, texs );
		glBindTexture( GL_TEXTURE_2D, texs[0] );
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fRgba8 ), 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, a.data() );
		texParams( GL_TEXTURE_2D, true, true, true );
		f.generateMipmap( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, texs[1] );
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fRgba8 ), 32, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, b.data() );
		texParams( GL_TEXTURE_2D, false, false, false );
		glBindTexture( GL_TEXTURE_2D, texs[2] );
		for ( int l = 0, s = 64; s >= 1; l++, s /= 2 ) {
			std::vector<std::uint8_t> blk( size_t( std::max( s / 4, 1 ) * std::max( s / 4, 1 ) * 16 ) );
			for ( auto & c : blk )
				c = std::uint8_t( rng() & 255 );
			ex.compressedTexImage2D( GL_TEXTURE_2D, l, kBc3, s, s, 0, GLsizei( blk.size() ), blk.data() );
		}
		texParams( GL_TEXTURE_2D, true, true, true );
		glBindTexture( GL_TEXTURE_2D, 0 );
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the base maps" );
	}

	// ---- the clip (perspective 60 degrees, near 0.5, far 100) with a sub-pixel jitter, the opaque planes' depth
	{
		const float fy = 1.0f / std::tan( 3.14159265f / 6.0f ), n = 0.5f, fa = 100.0f;
		gClip[0] = fy / ( float( W ) / float( H ) );
		gClip[5] = fy;
		gClip[10] = ( fa + n ) / ( n - fa );
		gClip[11] = -1.0f;
		gClip[14] = 2.0f * fa * n / ( n - fa );
		gClip[8] = 0.37f * 2.0f / float( W );
		gClip[9] = -0.21f * 2.0f / float( H );
	}
	{
		f.bindFramebuffer( fFb, gFbo );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glClearDepth( 1.0 );
		glClearStencil( 0x5A );
		glClear( GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		f.useProgram( gProg );
		Draw pa, pb;
		std::memcpy( pa.d.clipJit, gClip, sizeof( gClip ) );
		std::memcpy( pb.d.clipJit, gClip, sizeof( gClip ) );
		quad( pa.d, -5.0f, 0.0f, 5.0f, 6.0f, &zA, 0.0f, 0.0f );
		quad( pb.d, 1.5f, 0.0f, 2.5f, 3.0f, &zB, 0.0f, 0.0f );
		glDraw( pa );
		glDraw( pb );
		drawState0();
		if ( glGetError() != GL_NO_ERROR )
			return fatal( "the opaque planes" );
	}
	buildScene( texs );
	f.useProgram( gProg );
	f.uniform1i( loc( "BaseMap" ), 0 );

	bool pass = true;
	auto check = [&]( bool ok, const char * what, const std::string & detail ) {
		std::printf( "TAAREACTIVEDRAW %-34s %s%s\n", what, ok ? "OK" : "FAIL", detail.empty() ? "" : ( "  " + detail ).c_str() );
		pass = pass && ok;
	};

	// ---- GL: the reference and the fixture checks
	const std::vector<std::uint8_t> ref = glRun( true, true ), ref2 = glRun( true, true );
	const std::vector<std::uint8_t> noDepth = glRun( false, true ), noOffset = glRun( true, false );
	if ( glGetError() != GL_NO_ERROR )
		return fatal( "GL's pass" );
	const std::vector<std::uint32_t> depth0 = readDepth( gDepth );
	check( moved( ref, ref2 ) == 0, "fixture: GL twice equal", std::to_string( moved( ref, ref2 ) ) + " pixels differ" );
	check( moved( ref, noDepth ) >= 200, "fixture: the planes occlude", std::to_string( moved( ref, noDepth ) ) + " pixels move" );
	check( moved( ref, noOffset ) >= 200, "fixture: the offset matters", std::to_string( moved( ref, noOffset ) ) + " pixels move" );
	check( moved( seed, ref ) >= 3000, "fixture: GL covers", std::to_string( moved( seed, ref ) ) + " pixels" );

	// ---- Vulkan
	auto say = [&]( const char * what, const VkOut & o, bool wantOk ) {
		const Verdict v = judge( ref, o.mask, seed );
		char b[512];
		std::snprintf( b, sizeof( b ), "%s: covered %zu, one-side %zu, 1 level %zu, past 1 %zu (max %d), B/A moved %zu; flush %s%s; "
			"%d refused%s%s; %d draws in %d crossings, %d base map copies, %.3f ms CPU",
			v.word(), v.covered, v.oneSide, v.one, v.past1, v.maxd, v.ba, o.ok ? "ok" : "FAILED ", o.ok ? "" : o.why.c_str(), o.refused,
			o.refused ? ": " : "", o.refWhy.c_str(), o.st.draws, o.st.crossings, o.st.copies, o.st.cpuMs );
		const bool ran = o.ok && o.refused == 0;  // a red must run and FAIL the bar, never refuse
		check( ran && ( wantOk ? v.ok() : !v.ok() ), what, b );
		return v;
	};
	const VkOut batch = vkRun( 0, 0 );
	say( "vulkan batch vs GL", batch, true );
	check( batch.st.crossings == 1 && batch.st.draws == kDraws, "batch: one crossing",
		std::to_string( batch.st.crossings ) + " crossings, " + std::to_string( batch.st.draws ) + " draws" );
	const std::vector<std::uint32_t> depthTwin = readDepth( WwVkPresent::taReactiveDepthTwin() ), depth1 = readDepth( gDepth );
	size_t dd = 0;
	for ( size_t p = 0; p < depth0.size(); p++ )
		dd += depthTwin[p] != depth0[p] || depth1[p] != depth0[p];
	check( WwVkPresent::taReactiveDepthTwin() != 0 && dd == 0, "depth twin EXACT", std::to_string( dd ) + " texels differ" );
	const VkOut singles = vkRun( 1, 0 );
	say( "vulkan singles vs GL", singles, true );
	check( singles.st.crossings == kDraws && moved( singles.mask, batch.mask ) == 0, "singles vs batch EXACT",
		std::to_string( moved( singles.mask, batch.mask ) ) + " pixels differ, " + std::to_string( singles.st.crossings ) + " crossings" );
	const VkOut fives = vkRun( 5, 0 );
	say( "vulkan maxPer 5 vs GL", fives, true );
	check( fives.st.crossings == ( kDraws + 4 ) / 5 && moved( fives.mask, batch.mask ) == 0, "maxPer 5 vs batch EXACT",
		std::to_string( moved( fives.mask, batch.mask ) ) + " pixels differ, " + std::to_string( fives.st.crossings ) + " crossings" );

	// ---- reds (each must FAIL)
	say( "red 1024 (draw k binds set k-1)", vkRun( 0, 1024 ), false );
	say( "red 2048 (draw 1 dropped)", vkRun( 0, 2048 ), false );

	WwVkPresent::sharedTargetDrop( taKey( 0 ) );
	WwVkPresent::sharedTargetDrop( taKey( 1 ) );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	ReleaseDC( wnd, dc );
	DestroyWindow( wnd );
	std::printf( "TAAREACTIVEDRAW GATE %s\n", pass ? "OK" : "FAILED" );
	return pass ? 0 : 1;
}
