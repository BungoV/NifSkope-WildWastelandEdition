// Lane VKFAR2 (2026-11-13, row G3): the far cards (farCardsRun, vkfarcardsdraw.inc: impostor_instez_vs + impostor_cellinstez_ps,
// one instanced draw a card set) on the present device against GL's own impostor_instez.vert / impostor_cellinstez.frag
// (the FOGLEAK build the shipped SPIR-V twins) drawn with impostordraw.cpp's state, per sample, into the same 4x MSAA
// RGBA16F + D24S8 frame contents. The Vulkan side takes what farCardsOnVulkan takes: fcTexOf over the 26 slots, the card
// buffers and each set's sheets (BC3, mipped), fxPack's b31 (pass-wide once, the per-set fields per set, the vertex stage's).
// Inputs: the fog (volsky's job), an interior cell with a shadowed omni (a D16 cube array, compare LEQUAL) and a spot, the
// DALC ambient; 2 sets (300 + 200 cards, one with a mask sheet), the depth search, the depth offset, the sway, the stipple cut.
// Frame before the cards: a sky clear and a depth slab over the left half at the cards' median depth. After the cards: a
// full-screen draw where stencil == 1 sets (0, 0, 0, 1) (the stencil), then one at the cards' 40th-percentile depth with
// depth LESS (the depth the cards wrote).
// Bar (set before the first run, never widened):
//   covered = samples whose colour differs from the frame before the cards on either side (must be >= 1000);
//   flips = samples with any half past 2 ulp; EXACT = no half differs; BOUND = flips <= 0.05 % of covered, halves past 1 ulp
//   outside flips <= 0.4 % of covered, stencil differs on <= 0.05 % of covered, depth flips (the depth draw's sample past 2
//   ulp where the stencil read was not) <= 0.05 % of covered. Anything else FAILS.
// Reds (each must FAIL): red 1 no depth write, red 2 every set's cardBase 0 (the per-set field dropped), red 4 the shadow compare inverted.
// Last line `FARCARDS GATE OK` or `FARCARDS GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <random>

namespace farc
{
using namespace fv;
struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * compressedTexImage2D )( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void * ) = nullptr;
	void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
	void( WINAPI * drawElementsInstanced )( GLenum, GLsizei, GLenum, const void *, GLsizei ) = nullptr;
};
Ex ex;

const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";
const char * kRevealFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.0, 0.0, 0.0, 1.0 ); }\n";
const char * kDepthFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.1, 0.9, 0.1, 0.25 ); }\n";

struct Verdict
{
	size_t covered = 0, flips = 0, past1 = 0, diff = 0, stencil = 0, depth = 0;
	int maxu = 0;
	bool ok() const
	{
		return covered >= 1000 && flips * 2000 <= covered && past1 * 250 <= covered && stencil * 2000 <= covered && depth * 2000 <= covered;
	}
	const char * word() const { return covered < 1000 ? "FAIL" : diff == 0 && stencil == 0 && depth == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

size_t gWS = 1;
bool gDump = false;

int sampleMax( const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b, size_t s )
{
	int m = 0;
	for ( int c = 0; c < 4; c++ )
		m = std::max( m, std::abs( ordered( a[s * 4 + size_t( c )] ) - ordered( b[s * 4 + size_t( c )] ) ) );
	return m;
}

// a / ra / da: Vulkan after the cards, after the stencil draw, after the depth draw; b / rb / db GL's
Verdict judge( const std::vector<std::uint16_t> & bg, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b,
	const std::vector<std::uint16_t> & ra, const std::vector<std::uint16_t> & rb, const std::vector<std::uint16_t> & da,
	const std::vector<std::uint16_t> & db )
{
	Verdict v;
	const size_t n = a.size();
	if ( !n || b.size() != n || bg.size() != n || ra.size() != n || rb.size() != n || da.size() != n || db.size() != n )
		return v;
	for ( size_t s = 0; s < n / 4; s++ ) {
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
		const bool dFlip = sampleMax( ra, rb, s ) <= 2 && sampleMax( da, db, s ) > 2;
		v.depth += dFlip;
		v.diff += dFlip;
		if ( m > 2 ) {
			if ( gDump && v.flips < 60 )  // FARCARDS_DUMP=1: where the flips sit (pixel, sample), Vulkan / GL / before
				std::printf( "FARCARDS flip px %zu,%zu s%zu: vk %.5g %.5g %.5g gl %.5g %.5g %.5g bg %.5g st %d/%d\n", ( s % gWS ) / 4, s / gWS,
					s % 4, h2f( a[s * 4] ), h2f( a[s * 4 + 1] ), h2f( a[s * 4 + 2] ), h2f( b[s * 4] ), h2f( b[s * 4 + 1] ), h2f( b[s * 4 + 2] ),
					h2f( bg[s * 4] ), int( sa ), int( sb ) );
			v.flips++;
		} else
			v.past1 += size_t( p1 );
		v.maxu = std::max( v.maxu, m > 2 ? 0 : m );
	}
	if ( gDump ) {  // FARCARDS_DUMP=1: an 8x10 px block map of the flips (. none, 1..9 tenths of the block's samples, # all), top row = y 0
		const size_t W = gWS / 4, H = n / 4 / gWS;
		for ( size_t by = 0; by < H; by += 10 ) {
			std::string row = "FARCARDS map ";
			for ( size_t bx = 0; bx < W; bx += 8 ) {
				size_t f = 0, all = 0;
				for ( size_t y = by; y < std::min( by + 10, H ); y++ )
					for ( size_t x = bx; x < std::min( bx + 8, W ); x++ )
						for ( size_t k = 0; k < 4; k++ ) {
							const size_t s = y * gWS + x * 4 + k;
							all++;
							f += sampleMax( a, b, s ) > 2;
						}
				row += f == 0 ? '.' : f == all ? '#' : char( '0' + std::min<size_t>( 9, 1 + f * 9 / all ) );
			}
			std::printf( "%s\n", row.c_str() );
		}
	}
	return v;
}

bool isSampler( GLenum ty )
{
	return ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 && !( ty >= 0x8DC6 && ty <= 0x8DC8 ) )
		|| ( ty >= 0x900C && ty <= 0x900F ) || ( ty >= 0x9108 && ty <= 0x910D );
}

struct Sheet
{
	std::string name;
	GLuint gl = 0;
};
}  // namespace farc (windows.h defines far)

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using namespace fv;
	using namespace farc;
	gTag = "FARCARDS";
	if ( argc < 2 ) {
		std::printf( "usage: farcards <prep dir>\nFARCARDS GATE FAILED\n" );
		return 2;
	}
	const std::string dir = std::string( argv[1] ) + "/farcards";
	auto fatal = [&]( const std::string & why ) {
		std::printf( "FARCARDS GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwfarcards";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwfarcards", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwfarcards", "far cards harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	// the app's GL state (src/gl/gltex.cpp, scenelighting.cpp): the shadow cubes filter across faces as Vulkan's always do
	glEnable( 0x884F );  // GL_TEXTURE_CUBE_MAP_SEAMLESS
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	if ( !loadAll() || !load( ex.genVertexArrays, "glGenVertexArrays" ) || !load( ex.bindVertexArray, "glBindVertexArray" )
		|| !load( ex.vertexAttribPointer, "glVertexAttribPointer" ) || !load( ex.enableVertexAttribArray, "glEnableVertexAttribArray" )
		|| !load( ex.compressedTexImage2D, "glCompressedTexImage2D" ) || !load( ex.texBuffer, "glTexBuffer" )
		|| !load( ex.drawElementsInstanced, "glDrawElementsInstanced" ) )
		return fatal( "GL entry points" );
	std::printf( "FARCARDS GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pSlab = program( kTriVs, kSlabFs, "slab" ),
		pReveal = program( kTriVs, kRevealFs, "reveal" ), pDepth = program( kTriVs, kDepthFs, "depth" );
	const GLuint pCards = program( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.frag.glsl" ), "impostor_cellinstez" );
	if ( !pUnpack || !pSlab || !pReveal || !pDepth || !pCards )
		return fatal( "GL programs" );
	std::vector<int> jv;
	if ( !readJob( dir + "/job.txt", jv, 2 ) )
		return fatal( "job.txt" );
	const int W = jv[0], H = jv[1], WS = W * S;
	gWS = size_t( WS );
	gDump = std::getenv( "FARCARDS_DUMP" ) != nullptr;
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

	// ---- the fog's uniforms + textures (the volume in a shared target, as the app's), the global block
	const char * fogSlot[5] = { "atApTex", "volAirTerr", "volAirRainMap", "volTex", "volTile" };
	const int fogType[5] = { 1, 2, 0, 1, 0 };
	UniMap uni = readUni( dir + "/uni.txt" );
	std::vector<TexIn> tex = readTex( dir );
	if ( tex.size() != 5 )
		return fatal( "tex.txt" );
	static char volKey;
	GLuint volShared = 0;
	for ( int k = 0; k < 5; k++ ) {
		TexIn & t = tex[size_t( k )];
		if ( t.name != fogSlot[k] || t.type != fogType[k] )
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
	if ( global.size() != size_t( WwVkPresent::kFxGlobalBytes ) )
		return fatal( "global.bin size" );
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );
	f.bindBuffer( fUbo, ubo );
	f.bufferData( fUbo, std::ptrdiff_t( global.size() ), global.data(), fStatic );
	const GLuint bi = f.getUniformBlockIndex( pCards, "globalUniforms" );
	if ( bi != 0xFFFFFFFFu )
		f.uniformBlockBinding( pCards, bi, 0 );
	const auto pit = uni.find( "projectionMatrix" );
	if ( pit == uni.end() || pit->second.size() != 16 )
		return fatal( "projectionMatrix" );
	float proj[16];
	for ( int k = 0; k < 16; k++ )
		proj[k] = float( pit->second[size_t( k )] );

	// ---- the sheets: BC3, every level (prep's numpy encoder)
	std::vector<Sheet> sheets;
	{
		std::ifstream in( dir + "/sheets.txt" );
		std::string line;
		while ( std::getline( in, line ) ) {
			std::istringstream ls( line );
			Sheet s;
			int w = 0, h = 0, lv = 0;
			std::string file;
			if ( !( ls >> s.name >> w >> h >> lv >> file ) )
				continue;
			const std::string data = readFile( dir + "/" + file );
			glGenTextures( 1, &s.gl );
			glBindTexture( GL_TEXTURE_2D, s.gl );
			size_t at = 0;
			for ( int l = 0; l < lv; l++ ) {
				const int lw = std::max( w >> l, 1 ), lh = std::max( h >> l, 1 );
				const size_t bytes = size_t( std::max( 1, ( lw + 3 ) / 4 ) ) * size_t( std::max( 1, ( lh + 3 ) / 4 ) ) * 16;
				if ( at + bytes > data.size() )
					return fatal( "sheet " + s.name + " is short" );
				ex.compressedTexImage2D( GL_TEXTURE_2D, l, 0x83F3, lw, lh, 0, GLsizei( bytes ), data.data() + at );  // DXT5
				at += bytes;
			}
			if ( at != data.size() )
				return fatal( "sheet " + s.name + " has bytes past its levels" );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLint( fMipLinear ) );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint( fClamp ) );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint( fClamp ) );
			glTexParameteri( GL_TEXTURE_2D, fMaxLevel, lv - 1 );
			sheets.push_back( s );
		}
	}
	auto sheetOf = [&]( const std::string & n ) -> GLuint {
		for ( const Sheet & s : sheets )
			if ( s.name == n )
				return s.gl;
		return 0;
	};
	if ( sheets.size() != 6 || glGetError() != GL_NO_ERROR )
		return fatal( "the sheets (BC3 upload)" );

	// ---- the view: rotated + moved, so modelViewMatrix, cardEyeModel and cellRow carry real values
	const float ang = 0.3f, ca = std::cos( ang ), sa = std::sin( ang );
	const float R[3][3] = { { ca, 0.0f, sa }, { 0.0f, 1.0f, 0.0f }, { -sa, 0.0f, ca } };
	const float tr[3] = { 10.0f, -20.0f, 30.0f };
	float mv[16] = {}, eye[3], row[12];
	for ( int c = 0; c < 3; c++ )
		for ( int r = 0; r < 3; r++ )
			mv[c * 4 + r] = R[r][c];
	for ( int r = 0; r < 3; r++ )
		mv[12 + r] = tr[r];
	mv[15] = 1.0f;
	for ( int k = 0; k < 3; k++ )
		eye[k] = -( R[0][k] * tr[0] + R[1][k] * tr[1] + R[2][k] * tr[2] );
	for ( int j = 0; j < 3; j++ ) {
		for ( int k = 0; k < 3; k++ )
			row[j * 4 + k] = R[k][j];
		row[j * 4 + 3] = eye[j];
	}
	auto toModel = [&]( const float v[3], float out[3] ) {
		for ( int k = 0; k < 3; k++ )
			out[k] = R[0][k] * ( v[0] - tr[0] ) + R[1][k] * ( v[1] - tr[1] ) + R[2][k] * ( v[2] - tr[2] );
	};
	auto dirToModel = [&]( const float v[3], float out[3] ) {
		for ( int k = 0; k < 3; k++ )
			out[k] = R[0][k] * v[0] + R[1][k] * v[1] + R[2][k] * v[2];
	};
	const float focal = proj[5] * 0.5f * float( H );

	// ---- the cards: 2 sets, 6 texels a card; the frames, 4 texels a frame (16 a set, set 1 from texel 64)
	const int nSets = 2, setCount[2] = { 300, 200 }, setBase[2] = { 0, 300 }, frameBase[2] = { 0, 64 };
	const int nCards = setCount[0] + setCount[1];
	std::mt19937 rng( 20261113u );
	std::uniform_real_distribution<float> u01( 0.0f, 1.0f );
	std::vector<float> cardData( size_t( nCards ) * 24 ), frames( size_t( 128 ) * 4, 0.0f ), zs;
	const float kTwoPi = 6.2831853f;
	for ( int s = 0; s < nSets; s++ )
		for ( int f2 = 0; f2 < 16; f2++ ) {
			const float th = float( f2 ) * kTwoPi / 16.0f;
			float * fr = &frames[size_t( frameBase[s] + f2 * 4 ) * 4];
			const float rect[4] = { float( f2 % 4 ) * 0.25f, float( f2 / 4 ) * 0.25f, 0.25f, 0.25f };
			const float off = ( f2 % 2 ? 3.0f : -3.0f ) * ( s ? 0.5f : 1.0f );
			std::memcpy( fr, rect, sizeof( rect ) );
			const float t1[4] = { std::cos( th ), 0.0f, -std::sin( th ), off }, t2[4] = { 0.0f, 1.0f, 0.0f, -off }, t3[4] = { std::sin( th ), 0.0f, std::cos( th ), 0.0f };
			std::memcpy( fr + 4, t1, sizeof( t1 ) );
			std::memcpy( fr + 8, t2, sizeof( t2 ) );
			std::memcpy( fr + 12, t3, sizeof( t3 ) );
		}
	for ( int i = 0; i < nCards; i++ ) {
		const float d = 200.0f + 2800.0f * u01( rng );
		const float v[3] = { ( u01( rng ) * 2.0f - 1.0f ) * d / proj[0], ( u01( rng ) * 2.0f - 1.0f ) * d / proj[5], -d };
		float c[3];
		toModel( v, c );
		const float hx = ( 8.0f + 32.0f * u01( rng ) ) * d / focal, hy = hx * ( 0.8f + 0.5f * u01( rng ) );
		float fw[3] = { eye[0] - c[0], eye[1] - c[1], eye[2] - c[2] };
		const float fl = std::sqrt( fw[0] * fw[0] + fw[1] * fw[1] + fw[2] * fw[2] );
		for ( float & x : fw )
			x /= fl;
		float rt[3] = { fw[2], 0.0f, -fw[0] };  // cross( (0, 1, 0), fwd )
		const float rl = std::sqrt( rt[0] * rt[0] + rt[2] * rt[2] );
		rt[0] /= rl;
		rt[2] /= rl;
		const float up[3] = { fw[1] * rt[2] - fw[2] * rt[1], fw[2] * rt[0] - fw[0] * rt[2], fw[0] * rt[1] - fw[1] * rt[0] };
		const float a = std::atan2( fw[0], fw[2] );
		const int f0 = ( int( std::lround( a / ( kTwoPi / 16.0f ) ) ) % 16 + 16 ) % 16;
		const float fr0 = float( f0 ), fr1 = float( ( f0 + 1 ) % 16 ), fr2 = float( ( f0 + 15 ) % 16 );
		const float rec[24] = { c[0], c[1], c[2], 2.0f * hx, hx, hy, kTwoPi * u01( rng ), 1.0f, rt[0], rt[1], rt[2], 3.0f,
			up[0], up[1], up[2], fr0, fw[0], fw[1], fw[2], fr1, 0.5f, 0.3f, 0.2f, fr2 };
		std::memcpy( &cardData[size_t( i ) * 24], rec, sizeof( rec ) );
		float cz = 0.0f, cw = 0.0f;
		for ( int k = 0; k < 3; k++ ) {
			cz += proj[k * 4 + 2] * v[k];
			cw += proj[k * 4 + 3] * v[k];
		}
		zs.push_back( ( ( cz + proj[14] ) / ( cw + proj[15] ) ) * 0.5f + 0.5f );
	}
	std::sort( zs.begin(), zs.end() );
	auto between = [&]( size_t m ) { return 0.5f * ( zs[m - 1] + zs[m] ); };
	const float zd = between( zs.size() / 2 ), zd2 = between( zs.size() * 2 / 5 );
	// the cell's 2 lights (model space = the cell's world): a shadowed omni (cube slot 0) and an unshadowed spot
	std::vector<float> lights( size_t( 2 ) * 8 * 4, 0.0f );
	{
		const float p0v[3] = { 300.0f, 400.0f, -100.0f }, p1v[3] = { -400.0f, 300.0f, -500.0f };
		float p0[3], p1[3], dv[3] = { 0.0f, -0.2f, -1.0f }, d1[3];
		const float dl = std::sqrt( dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2] );
		for ( float & x : dv )
			x /= dl;
		toModel( p0v, p0 );
		toModel( p1v, p1 );
		dirToModel( dv, d1 );
		const float L0[20] = { p0[0], p0[1], p0[2], 5000.0f, 3.0f, 2.5f, 2.0f, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f };
		const float L1[20] = { p1[0], p1[1], p1[2], 4000.0f, 1.0f, 1.5f, 2.0f, 0.5f, d1[0], d1[1], d1[2], 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
			-1.0f, 0.0f, 0.0f, 0.0f };
		std::memcpy( &lights[0], L0, sizeof( L0 ) );
		std::memcpy( &lights[32], L1, sizeof( L1 ) );
	}
	std::printf( "FARCARDS job: %dx%d x%d, %d cards in %d sets, slab depth %.6f, depth draw %.6f, focal %.2f px\n", W, H, S, nCards, nSets,
		zd, zd2, focal );

	// the texel buffers (GL_TEXTURE_BUFFER over RGBA32F)
	auto tbo = [&]( const std::vector<float> & v, GLuint & buf ) {
		GLuint t = 0;
		f.genBuffers( 1, &buf );
		f.bindBuffer( 0x8C2A, buf );
		f.bufferData( 0x8C2A, std::ptrdiff_t( v.size() * 4 ), v.data(), fStatic );
		glGenTextures( 1, &t );
		glBindTexture( 0x8C2A, t );
		ex.texBuffer( 0x8C2A, fRgba32f, buf );
		glBindTexture( 0x8C2A, 0 );
		f.bindBuffer( 0x8C2A, 0 );
		return t;
	};
	GLuint bufData = 0, bufFrames = 0, bufLights = 0;
	const GLuint tData = tbo( cardData, bufData ), tFrames = tbo( frames, bufFrames ), tLights = tbo( lights, bufLights );
	// the shadow cube array: one cube, D16, 8 x 8 blocks at depth 1 (lit) or 0.02 (shadowed past ~100 units)
	GLuint tShadow = 0;
	{
		const int cs = 64;
		std::vector<float> dep( size_t( cs ) * cs * 6 );
		for ( int face = 0; face < 6; face++ )
			for ( int y = 0; y < cs; y++ )
				for ( int x = 0; x < cs; x++ ) {
					const unsigned h = ( unsigned( x / 8 ) * 73856093u ) ^ ( unsigned( y / 8 ) * 19349663u ) ^ ( unsigned( face ) * 83492791u );
					dep[( size_t( face ) * cs + y ) * cs + x] = ( ( h >> 3 ) & 3u ) == 0 ? 0.02f : 1.0f;
				}
		glGenTextures( 1, &tShadow );
		glBindTexture( 0x9009, tShadow );
		f.texImage3D( 0x9009, 0, 0x81A5, cs, cs, 6, 0, 0x1902, GL_FLOAT, dep.data() );
		glTexParameteri( 0x9009, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( 0x9009, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( 0x9009, GL_TEXTURE_WRAP_S, GLint( fClamp ) );
		glTexParameteri( 0x9009, GL_TEXTURE_WRAP_T, GLint( fClamp ) );
		glTexParameteri( 0x9009, fWrapR, GLint( fClamp ) );
		glTexParameteri( 0x9009, fMaxLevel, 0 );
		glTexParameteri( 0x9009, 0x884C, 0x884E );  // GL_TEXTURE_COMPARE_MODE = GL_COMPARE_REF_TO_TEXTURE
		glTexParameteri( 0x9009, 0x884D, GL_LEQUAL );
		glBindTexture( 0x9009, 0 );
	}
	if ( glGetError() != GL_NO_ERROR )
		return fatal( "the texel buffers / the shadow cubes" );

	// the quad: impostordraw.cpp's unit quad, uv, indices
	static const float kQuadPos[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const float kQuadUv[8] = { 0, 1, 1, 1, 0, 0, 1, 0 };
	static const std::uint16_t kQuadIdx[6] = { 0, 1, 2, 2, 1, 3 };
	GLuint vao = 0, vb[3] = {};
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	f.genBuffers( 3, vb );
	f.bindBuffer( 0x8892, vb[0] );
	f.bufferData( 0x8892, sizeof( kQuadPos ), kQuadPos, fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
	ex.enableVertexAttribArray( 0 );
	f.bindBuffer( 0x8892, vb[1] );
	f.bufferData( 0x8892, sizeof( kQuadUv ), kQuadUv, fStatic );
	ex.vertexAttribPointer( 7, 2, GL_FLOAT, GL_FALSE, 0, nullptr );
	ex.enableVertexAttribArray( 7 );
	f.bindBuffer( 0x8893, vb[2] );  // GL_ELEMENT_ARRAY_BUFFER, held by the VAO
	f.bufferData( 0x8893, sizeof( kQuadIdx ), kQuadIdx, fStatic );
	ex.bindVertexArray( 0 );
	f.bindBuffer( 0x8892, 0 );

	// ---- every sampler its own unit (from 20; unit 15 is the unpack's), the textures bound by name
	std::map<std::string, int> unitOf;
	{
		f.useProgram( pCards );
		GLint n = 0;
		f.getProgramiv( pCards, fActiveUnis, &n );
		int next = 20;
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( pCards, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			if ( !isSampler( ty ) )
				continue;
			std::string base( nm );
			if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
				base.resize( base.size() - 3 );
			std::vector<GLint> units( size_t( std::max( sz, 1 ) ) );
			for ( GLint & u : units )
				u = next++;
			f.uniform1iv( f.getUniformLocation( pCards, nm ), GLsizei( units.size() ), units.data() );
			unitOf[base] = units[0];
		}
		std::printf( "FARCARDS %zu samplers, units 20..%d\n", unitOf.size(), next - 1 );
	}
	auto bindNamed = [&]( const char * name, GLenum target, GLuint t ) {
		const auto it = unitOf.find( name );
		if ( it != unitOf.end() )
			bindUnit( it->second, target, t );
	};
	struct PerSet
	{
		const char * colour, * normal, * mask;
		int hasMask;
		float covBase, covFloor;
	};
	const PerSet per[2] = { { "colour0", "normal0", nullptr, 0, 0.0f, 0.0f }, { "colour1", "normal1", "mask1", 1, 160.0f / 255.0f, 0.3f } };
	// the pass-wide state impostordraw.cpp sets before its set loop
	auto bindPass = [&]() {
		f.useProgram( pCards );
		setUnis( pCards, uni );
		for ( int k = 0; k < 5; k++ )
			bindNamed( fogSlot[k], tex[size_t( k )].target, tex[size_t( k )].gl );
		bindNamed( "cardData", 0x8C2A, tData );
		bindNamed( "cardFrames", 0x8C2A, tFrames );
		bindNamed( "cellLights", 0x8C2A, tLights );
		bindNamed( "cellShadow", 0x9009, tShadow );
		f.useProgram( pCards );
		f.uniformMatrix4fv( f.getUniformLocation( pCards, "modelViewMatrix" ), 1, GL_FALSE, mv );
		f.uniform3fv( f.getUniformLocation( pCards, "cardEyeModel" ), 1, eye );
		f.uniform1i( f.getUniformLocation( pCards, "cardOrtho" ), 0 );
		f.uniform4fv( f.getUniformLocation( pCards, "cellRow" ), 3, row );
		if ( const char * e = std::getenv( "FARCARDS_DEBUG" ) )  // a probe: one sheet channel raw on both sides
			f.uniform1i( f.getUniformLocation( pCards, "debugChannel" ), std::atoi( e ) );
		if ( const char * e = std::getenv( "FARCARDS_SETI" ) ) {  // a probe: name=value[,name=value...], int uniforms on both sides
			std::string s( e );
			for ( size_t at = 0; at < s.size(); ) {
				size_t end = s.find( ',', at );
				if ( end == std::string::npos )
					end = s.size();
				const std::string kv = s.substr( at, end - at );
				const size_t eq = kv.find( '=' );
				if ( eq != std::string::npos )
					f.uniform1i( f.getUniformLocation( pCards, kv.substr( 0, eq ).c_str() ), std::atoi( kv.c_str() + eq + 1 ) );
				at = end + 1;
			}
		}
		f.bindBufferBase( fUbo, 0, ubo );
	};
	// one set's sheets and uniforms, as GL's loop sets them
	auto setUp = [&]( int s ) {
		const PerSet & p = per[s];
		f.useProgram( pCards );
		bindNamed( "ColourSheet", GL_TEXTURE_2D, sheetOf( p.colour ) );
		bindNamed( "NormalSheet", GL_TEXTURE_2D, sheetOf( p.normal ) );
		bindNamed( "MaskSheet", GL_TEXTURE_2D, p.mask ? sheetOf( p.mask ) : 0 );
		bindNamed( "EmissiveSheet", GL_TEXTURE_2D, 0 );
		f.uniform1i( f.getUniformLocation( pCards, "hasMaskSheet" ), p.hasMask );
		f.uniform1i( f.getUniformLocation( pCards, "hasEmissiveSheet" ), 0 );
		f.uniform1i( f.getUniformLocation( pCards, "familyPbr" ), 0 );
		f.uniform1fv( f.getUniformLocation( pCards, "cardCovBase" ), 1, &p.covBase );
		f.uniform1fv( f.getUniformLocation( pCards, "cardCovFloor" ), 1, &p.covFloor );
		f.uniform1i( f.getUniformLocation( pCards, "cardBase" ), setBase[s] );
		f.uniform1i( f.getUniformLocation( pCards, "frameTableBase" ), frameBase[s] );
		const float zero3[3] = {}, zero = 0.0f;
		f.uniform3fv( f.getUniformLocation( pCards, "cardLodTint" ), 1, zero3 );
		f.uniform1fv( f.getUniformLocation( pCards, "cardLodStripe" ), 1, &zero );
		return true;
	};
	// impostordraw.cpp's draw state: depth LEQUAL with write, no blend, no cull; the stencil the pass before left
	auto cardState = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		glDepthMask( GL_TRUE );
		glDisable( GL_CULL_FACE );
		glDisable( GL_BLEND );
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0x01 );
		glStencilFunc( GL_ALWAYS, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
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
	auto depthDraw = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glDepthMask( GL_FALSE );
		f.useProgram( pDepth );
		f.uniform1fv( f.getUniformLocation( pDepth, "zd" ), 1, &zd2 );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glDepthMask( GL_TRUE );
		drawState0();
	};
	// GL's reference: one instanced draw a set
	std::vector<std::uint16_t> bg, refA, refB, refC;
	{
		before( fbRef );
		bg = readSamples( refCol );
		before( fbRef );
		bindPass();
		cardState( fbRef );
		ex.bindVertexArray( vao );
		for ( int s = 0; s < nSets; s++ ) {
			setUp( s );
			ex.drawElementsInstanced( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr, setCount[s] );
		}
		ex.bindVertexArray( 0 );
		drawState0();
		glDepthFunc( GL_LESS );
		const GLenum e = glGetError();
		if ( e != GL_NO_ERROR )
			return fatal( "the GL reference draw (GL error " + std::to_string( e ) + ")" );
		refA = readSamples( refCol );
		reveal( fbRef );
		refB = readSamples( refCol );
		depthDraw( fbRef );
		refC = readSamples( refCol );
	}
	// farCardsOnVulkan's gather, against the same GL state
	static const char * const kName[26] = { "atApTex", "volAirTerr", "volAirRainMap", "volTex", "volTile", "volAirNoise", "cellCluster",
		"cellIsBloom", "cellIsLut", "cellLights", "cellShadow", "cellCtGbuf", "cellFarSlots", "cellFarRecs", "cellFarPlaced", "cellSsgi",
		nullptr, "ussCloudMap", "cardSceneDepth", nullptr, nullptr, nullptr, "cellGi", "cellGiSlots", "cellRooms", "cellAoDecal" };
	static const int kType[26] = { 1, 2, 0, 1, 0, 1, 5, 0, 1, 5, 3, 0, 5, 5, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0 };
	static const char * const kPer[] = { "hasMaskSheet", "hasEmissiveSheet", "familyPbr", "emissiveScale", "cardMipCap", "cardCovFloor",
		"cardCovBase", "cardBase", "frameTableBase", "cardLodTint", "cardLodStripe" };
	std::vector<WwVkPresent::FxField> perSet;
	for ( const WwVkPresent::FxField & x : WwVkPresent::kT_farcards )
		for ( const char * n : kPer )
			if ( std::strcmp( x.name, n ) == 0 )
				perSet.push_back( x );
	// cardBase / frameTableBase live only in the vertex stage's table (packed per set through vsLoose)
	for ( const char * n : kPer ) {
		int hits = 0;
		for ( const WwVkPresent::FxField & x : WwVkPresent::kT_farcards )
			hits += std::strcmp( x.name, n ) == 0;
		for ( const WwVkPresent::FxField & x : WwVkPresent::kT_farcardsVs )
			hits += std::strcmp( x.name, n ) == 0;
		if ( hits == 0 )
			return fatal( std::string( "the per-set field " ) + n + " is in neither b31 table" );
	}
	static const char * const kSheet[4] = { "ColourSheet", "NormalSheet", "MaskSheet", "EmissiveSheet" };
	auto runVk = [&]( int red, std::vector<std::uint16_t> & a, std::vector<std::uint16_t> & b, std::vector<std::uint16_t> & c ) -> bool {
		before( fbFrame );
		bindPass();
		cardState( fbFrame );
		static WwVkPresent::FarCardsRun t;
		t = WwVkPresent::FarCardsRun();
		t.w = W;
		t.h = H;
		glGetIntegerv( GL_VIEWPORT, t.vp );
		t.color = fc;
		std::memcpy( t.global, global.data(), sizeof( t.global ) );
		WwVkPresent::FarCardsStencil & sc = t.stencil;
		sc.on = glIsEnabled( GL_STENCIL_TEST );
		static const GLenum kFront[7] = { GL_STENCIL_FUNC, GL_STENCIL_REF, GL_STENCIL_VALUE_MASK, GL_STENCIL_WRITEMASK, GL_STENCIL_FAIL,
			GL_STENCIL_PASS_DEPTH_FAIL, GL_STENCIL_PASS_DEPTH_PASS };
		static const GLenum kBack[7] = { 0x8800, 0x8CA3, 0x8CA4, 0x8CA5, 0x8801, 0x8802, 0x8803 };
		GLint fs[7] = {}, bs[7] = {};
		for ( int i = 0; i < 7; i++ ) {
			glGetIntegerv( kFront[i], &fs[i] );
			glGetIntegerv( kBack[i], &bs[i] );
		}
		if ( !std::equal( fs, fs + 7, bs ) ) {
			std::printf( "FARCARDS GL's front and back stencil differ\n" );
			return false;
		}
		sc.func = fs[0];
		sc.ref = fs[1];
		sc.mask = fs[2] & 0xFF;
		sc.write = fs[3] & 0xFF;
		sc.sfail = fs[4];
		sc.zfail = fs[5];
		sc.zpass = fs[6];
		for ( int k = 0; k < 26; k++ )
			if ( kName[k] && !WwVkPresent::fcTexOf( &ga, pCards, kName[k], kType[k], t.tex[k], why ) ) {
				std::printf( "FARCARDS fcTexOf refused: %s\n", why.c_str() );
				return false;
			}
		if ( !WwVkPresent::fcTexOf( &ga, pCards, "cardData", 5, t.cardData, why ) || !WwVkPresent::fcTexOf( &ga, pCards, "cardFrames", 5, t.cardFrames, why ) ) {
			std::printf( "FARCARDS fcTexOf refused: %s\n", why.c_str() );
			return false;
		}
		static std::uint8_t base[WwVkPresent::kT_farcardsBytes];
		std::memset( base, 0, sizeof( base ) );
		WwVkPresent::fxPack( &ga, pCards, WwVkPresent::kT_farcards, int( std::size( WwVkPresent::kT_farcards ) ), base );
		for ( int s = 0; s < nSets; s++ ) {
			setUp( s );
			t.sets.emplace_back();
			WwVkPresent::FarCardsSet & cs = t.sets.back();
			cs.count = setCount[s];
			for ( int j = 0; j < 4; j++ )
				if ( !WwVkPresent::fcTexOf( &ga, pCards, kSheet[j], 0, cs.sheet[j], why ) ) {
					std::printf( "FARCARDS fcTexOf refused: %s\n", why.c_str() );
					return false;
				}
			std::memcpy( cs.loose, base, sizeof( cs.loose ) );
			WwVkPresent::fxPack( &ga, pCards, perSet.data(), int( perSet.size() ), cs.loose );
			std::memset( cs.vsLoose, 0, sizeof( cs.vsLoose ) );
			WwVkPresent::fxPack( &ga, pCards, WwVkPresent::kT_farcardsVs, int( std::size( WwVkPresent::kT_farcardsVs ) ), cs.vsLoose );
			if ( gDump ) {
				int vb[2] = {};
				std::memcpy( vb, cs.vsLoose, 8 );
				std::printf( "FARCARDS set %d: count %d, VS cardBase %d frameTableBase %d (red %d)\n", s, cs.count, vb[0], vb[1], red );
			}
		}
		t.red = red;
		f.bindFramebuffer( fFb, fbFrame );
		if ( !WwVkPresent::farCardsRun( t, why ) ) {
			std::printf( "FARCARDS farCardsRun refused: %s\n", why.c_str() );
			return false;
		}
		drawState0();
		glDepthFunc( GL_LESS );
		a = readSamples( fc );
		reveal( fbFrame );
		b = readSamples( fc );
		depthDraw( fbFrame );
		c = readSamples( fc );
		return glGetError() == GL_NO_ERROR;
	};
	bool ok = true;
	{
		std::vector<std::uint16_t> a, b, c;
		if ( !runVk( 0, a, b, c ) ) {
			std::printf( "FARCARDS cards FAIL (the Vulkan run failed)\n" );
			ok = false;
		} else {
			const Verdict v = judge( bg, a, refA, b, refB, c, refC );
			std::printf( "FARCARDS cards %s (%zu samples covered, %zu halves differ, %zu edge flips, %zu halves past 1 ulp outside them, "
				"max %d ulp outside them, stencil differs on %zu, depth on %zu)\n", v.word(), v.covered, v.diff, v.flips, v.past1, v.maxu,
				v.stencil, v.depth );
			ok = ok && v.ok();
			const WwVkPresent::FxStat st = WwVkPresent::takeFarCardsStat();
			std::printf( "FARCARDS stat: %d runs, %d pipelines built in %.1f ms, %d input copies\n", st.runs, st.pipelines, st.buildMs, st.copies );
			// a second run: the sheets' twins are kept (no copy), the cubes and the volumes copied again; the picture the same
			std::vector<std::uint16_t> a2, b2, c2;
			if ( !runVk( 0, a2, b2, c2 ) || a2 != a || b2 != b || c2 != c ) {
				std::printf( "FARCARDS second run differs from the first\n" );
				ok = false;
			} else {
				const WwVkPresent::FxStat s2 = WwVkPresent::takeFarCardsStat();
				std::printf( "FARCARDS second run: identical, %d input copies\n", s2.copies );
			}
		}
	}
	const struct
	{
		const char * name;
		int red;
	} reds[3] = { { "depth (no depth write)", 1 }, { "cardBase (every set from card 0)", 2 }, { "shadow (the compare inverted)", 4 } };
	for ( const auto & r : reds ) {
		std::vector<std::uint16_t> a, b, c;
		if ( !runVk( r.red, a, b, c ) ) {
			std::printf( "FARCARDS red %s: the run failed (counts as a FAIL)\n", r.name );
			continue;
		}
		const Verdict v = judge( bg, a, refA, b, refB, c, refC );
		std::printf( "FARCARDS red %s: %s (%zu covered, %zu flips, %zu past 1 ulp, stencil differs on %zu, depth on %zu)\n", r.name, v.word(),
			v.covered, v.flips, v.past1, v.stencil, v.depth );
		if ( v.ok() ) {
			std::printf( "FARCARDS red %s PASSED: the gate cannot see it\n", r.name );
			ok = false;
		}
	}
	const WwVkPresent::FxStat st = WwVkPresent::takeFarCardsStat();
	std::printf( "FARCARDS stat (reds): %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f; includes the harness's hand-off "
		"wait, not a frame cost)\n", st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKey ) );
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "FARCARDS GATE OK" : "FARCARDS GATE FAILED" );
	return ok ? 0 : 1;
}
