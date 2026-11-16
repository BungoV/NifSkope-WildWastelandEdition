/* tests/vkframe/camveldraw.cpp -- lane VKOBJ18 (2026-11-16): the gate for the TAA inputs' camera vectors on Vulkan
 * (taainputs.cpp wwTaaInputsPass step 2, game_taa_mv.prog = cell_ao.vert + game_taa_mv.frag; twins camvel_vs from
 * res/shaders/hlsl/core/cell_ao_vs.hlsl, camvel_ps from res/shaders/hlsl/post/game_taa_mv_ps.hlsl; route vkcamvel.inc).
 *
 * BAR, committed before this gate's first run (it is never widened; a seeding change that lets a red reach its branch is
 * allowed, with the reason written here):
 *  - GL is the reference: GL's own game_taa_mv program draws taainputs.cpp's quad (the same 4 vertices and 6 indices) into an
 *    RG16F vel attachment alone, reading the frame's depth by texelFetch, in the pass's state (no depth test, no blend, no cull,
 *    every channel written). Vulkan draws the same quad through camVelRun into the shared twins.
 *  - vel (RG16F): every texel within 2 half-steps of the GL value (the half step at |GL value|, 2^-24 below 2^-14), each of R
 *    and G; a texel past that FAILS. EXACT is not claimed: taa_mv.hlsl's header records GL's own rounding of p.xy / p.w at 1 - 2
 *    last float bits that no order of the product matched (8 forms tried); objveldraw.cpp holds vel to the same 2 half-steps.
 *    Printed: texels not bit-equal, max difference in half steps.
 *  - mask (RGBA8) and depth: EXACT against the inputs (0 differing texels): the pass writes neither.
 *  - TESTS NOTHING: GL vel non-zero (R or G) on < 50 % of texels, or a red that moves < 1 % of texels against the unred
 *    Vulkan run (printed per red).
 *  - Reds, each must FAIL: 1 the jitter dropped, 2 reproj written one row (16 bytes) late, 3 the depth slot reads the zero
 *    view. A red the route refuses is a FAIL of the gate (not a red that failed).
 *  - Also checked: GL's own quad into the shared targets equals GL's plain reference EXACT (what the app draws after a
 *    refusal), and a target of the wrong size refuses by name with the targets untouched.
 *
 * Seeding (written before the first run):
 *  - 256 x 192, the whole case once per depth format: D24S8 (0x88F0) and D32F (0x8CAC). Each format has its own shared vel /
 *    mask / depth (sharedTarget, as the app's g.vk) and plain GL inputs; every run starts from the inputs (glCopyImageSubData).
 *  - Depth: a smooth non-constant field d = 0.55 + 0.4 * (0.5 + 0.5 sin(x / 23) cos(y / 17)) with a block at 1.0 (the far
 *    plane, x 200..239, y 20..59) and a block at 0.2 (x 30..79, y 120..169); D24S8 stores round(d * (2^24 - 1)) << 8 with
 *    stencil 0x5A, D32F the float.
 *  - The camera: a non-symmetric frustum (near 0.5, far 20); the current view yawed 0.21 rad and moved (0.4, -0.15, 0.6) against
 *    the previous one; reproj = prevVP x inverse( curVP ) built in double (as taainputs.cpp), sent as GL's column-major floats.
 *  - The jitter: (2.6, -1.7) px in NDC (2 j / size). Larger than the app's sub-pixel jitter so red 1's change stands above the
 *    2 half-step bound; the shader treats it as any uniform.
 *  - vel input: a non-zero half pattern (so a texel the quad missed shows); mask: G and A a 0 / 255 pattern.
 * Usage: camveldraw <out dir> <tree root>. Last line `CAMVELDRAW GATE OK` or `CAMVELDRAW GATE FAILED`.
 */
#define main fxview_main
#include "fxview.cpp"
#undef main

#include <cmath>

namespace cvd
{
using namespace fv;
using WwVkPresent::CamVelRun;

constexpr int W = 256, H = 192;
constexpr GLenum kArr = 0x8892, kElem = 0x8893, kDepthAtt = 0x8D00, kDs = 0x84F9, kU248 = 0x84FA, kCmpMode = 0x884C,
	kD24s8 = 0x88F0, kD32f = 0x8CAC, kRg16f = 0x822F;

struct Ox
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
};
Ox ox;

bool loadMore()
{
	return load( ox.genVertexArrays, "glGenVertexArrays" ) && load( ox.bindVertexArray, "glBindVertexArray" )
		&& load( ox.vertexAttribPointer, "glVertexAttribPointer" ) && load( ox.enableVertexAttribArray, "glEnableVertexAttribArray" );
}

// 4 x 4 in double, column-major (m[c * 4 + r]), as GL's mat4
struct D4
{
	double m[16] = {};
};

D4 ident()
{
	D4 a;
	for ( int i = 0; i < 4; i++ )
		a.m[i * 5] = 1.0;
	return a;
}

D4 mul( const D4 & a, const D4 & b )
{
	D4 o;
	for ( int c = 0; c < 4; c++ )
		for ( int r = 0; r < 4; r++ ) {
			double s = 0.0;
			for ( int k = 0; k < 4; k++ )
				s += a.m[k * 4 + r] * b.m[c * 4 + k];
			o.m[c * 4 + r] = s;
		}
	return o;
}

bool inverse( const D4 & a, D4 & out )
{
	double t[4][8];
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ ) {
			t[r][c] = a.m[c * 4 + r];
			t[r][c + 4] = r == c ? 1.0 : 0.0;
		}
	for ( int c = 0; c < 4; c++ ) {
		int p = c;
		for ( int r = c + 1; r < 4; r++ )
			if ( std::fabs( t[r][c] ) > std::fabs( t[p][c] ) )
				p = r;
		if ( std::fabs( t[p][c] ) < 1e-300 )
			return false;
		for ( int k = 0; k < 8; k++ )
			std::swap( t[c][k], t[p][k] );
		const double d = t[c][c];
		for ( int k = 0; k < 8; k++ )
			t[c][k] /= d;
		for ( int r = 0; r < 4; r++ )
			if ( r != c ) {
				const double x = t[r][c];
				for ( int k = 0; k < 8; k++ )
					t[r][k] -= x * t[c][k];
			}
	}
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			out.m[c * 4 + r] = t[r][c + 4];
	return true;
}

D4 trans( double x, double y, double z )
{
	D4 a = ident();
	a.m[12] = x;
	a.m[13] = y;
	a.m[14] = z;
	return a;
}

D4 rotY( double r )
{
	D4 a = ident();
	a.m[0] = std::cos( r );
	a.m[2] = -std::sin( r );
	a.m[8] = std::sin( r );
	a.m[10] = std::cos( r );
	return a;
}

D4 frustum( double l, double r, double b, double t, double n, double f )
{
	D4 a;
	a.m[0] = 2 * n / ( r - l );
	a.m[5] = 2 * n / ( t - b );
	a.m[8] = ( r + l ) / ( r - l );
	a.m[9] = ( t + b ) / ( t - b );
	a.m[10] = -( f + n ) / ( f - n );
	a.m[11] = -1.0;
	a.m[14] = -2 * f * n / ( f - n );
	return a;
}

struct Frame
{
	std::vector<std::uint16_t> vel;
	std::vector<std::uint8_t> mask;
	std::vector<std::uint32_t> depth;
	bool operator==( const Frame & o ) const { return vel == o.vel && mask == o.mask && depth == o.depth; }
};

struct Set
{
	GLuint v = 0, m = 0, d = 0, fb = 0;
};

bool gD24 = true;
GLuint pMv = 0, quadVao = 0;

Frame readSet( const Set & s )
{
	Frame fr;
	fr.vel.resize( std::size_t( W ) * H * 2 );
	fr.mask.resize( std::size_t( W ) * H * 4 );
	fr.depth.resize( std::size_t( W ) * H );
	glBindTexture( GL_TEXTURE_2D, s.v );
	glGetTexImage( GL_TEXTURE_2D, 0, fRg, fHalf, fr.vel.data() );
	glBindTexture( GL_TEXTURE_2D, s.m );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, fr.mask.data() );
	glBindTexture( GL_TEXTURE_2D, s.d );
	if ( gD24 )
		glGetTexImage( GL_TEXTURE_2D, 0, kDs, kU248, fr.depth.data() );
	else
		glGetTexImage( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, fr.depth.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	return fr;
}

void copyTex( GLuint a, GLuint b )
{
	f.copyImageSubData( a, GL_TEXTURE_2D, 0, 0, 0, 0, b, GL_TEXTURE_2D, 0, 0, 0, 0, W, H, 1 );
}

void reset( const Set & in, const Set & s )
{
	copyTex( in.v, s.v );
	copyTex( in.m, s.m );
	copyTex( in.d, s.d );
}

// taainputs.cpp's camFbo: vel alone
GLuint makeFb( GLuint c0 )
{
	GLuint b = 0;
	f.genFramebuffers( 1, &b );
	f.bindFramebuffer( fFb, b );
	f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, c0, 0 );
	const GLenum one = fCol0;
	f.drawBuffers( 1, &one );
	const bool ok = f.checkFramebufferStatus( fFb ) == fComplete;
	f.bindFramebuffer( fFb, 0 );
	return ok ? b : 0;
}

GLuint depthTex( GLenum fmt, const void * data )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( GL_TEXTURE_2D, t );
	if ( fmt == kD24s8 )
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), W, H, 0, kDs, kU248, data );
	else
		glTexImage2D( GL_TEXTURE_2D, 0, GLint( fmt ), W, H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, data );
	texParams( GL_TEXTURE_2D, false, false, false );
	glTexParameteri( GL_TEXTURE_2D, kCmpMode, GL_NONE );
	return t;
}

// GL's step 2, as taainputs.cpp draws it
GLenum glQuad( const Set & s, const CamVelRun & c )
{
	f.bindFramebuffer( fFb, s.fb );
	glViewport( 0, 0, W, H );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable( GL_BLEND );
	glDisable( GL_CULL_FACE );
	glDisable( GL_STENCIL_TEST );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	f.useProgram( pMv );
	f.activeTexture( fTex0 + 3 );
	glBindTexture( GL_TEXTURE_2D, s.d );
	f.uniform1i( f.getUniformLocation( pMv, "depthTex" ), 3 );
	f.uniformMatrix4fv( f.getUniformLocation( pMv, "reproj" ), 1, GL_FALSE, c.reproj );
	f.uniform2fv( f.getUniformLocation( pMv, "jitterNdc" ), 1, c.jitterNdc );
	f.uniform2fv( f.getUniformLocation( pMv, "invSize" ), 1, c.invSize );
	ox.bindVertexArray( quadVao );
	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr );
	ox.bindVertexArray( 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	f.activeTexture( fTex0 );
	f.useProgram( 0 );
	f.bindFramebuffer( fFb, 0 );
	return glGetError();
}

float halfToF( std::uint16_t h )
{
	const int s = h >> 15, e = ( h >> 10 ) & 31, m = h & 1023;
	float v = e == 0 ? std::ldexp( float( m ), -24 ) : e == 31 ? INFINITY : std::ldexp( float( m + 1024 ), e - 25 );
	return s ? -v : v;
}

// the half step at |x|
double halfStep( double x )
{
	x = std::fabs( x );
	if ( x < std::ldexp( 1.0, -14 ) )
		return std::ldexp( 1.0, -24 );
	int e = 0;
	std::frexp( x, &e );  // x = f 2^e, f in [0.5, 1)
	return std::ldexp( 1.0, e - 11 );
}

struct Judge
{
	std::size_t velOver = 0, velDiff = 0, mask = 0, depth = 0;
	double maxSteps = 0.0;
	bool ok() const { return velOver == 0 && mask == 0 && depth == 0; }
};

Judge judge( const Frame & in, const Frame & g, const Frame & v )
{
	Judge j;
	for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ ) {
		for ( int c = 0; c < 2; c++ ) {
			const std::uint16_t a = g.vel[i * 2 + c], b = v.vel[i * 2 + c];
			if ( a == b )
				continue;
			j.velDiff++;
			const double ga = halfToF( a ), vb = halfToF( b );
			const double steps = std::isfinite( ga ) && std::isfinite( vb ) ? std::fabs( ga - vb ) / halfStep( ga ) : 1e9;
			j.maxSteps = std::max( j.maxSteps, steps );
			if ( !( steps <= 2.0 ) )
				j.velOver++;
		}
		j.mask += std::memcmp( &in.mask[i * 4], &v.mask[i * 4], 4 ) != 0;
		j.depth += in.depth[i] != v.depth[i];
	}
	return j;
}

void printJudge( const char * fmt, const char * what, const Judge & j, const char * verdict )
{
	std::printf( "CAMVELDRAW %s %s: vel channels not bit-equal %zu, over 2 half steps %zu (max %.2f steps); mask %zu, depth %zu "
				 "texels changed -> %s\n", fmt, what, j.velDiff, j.velOver, j.maxSteps, j.mask, j.depth, verdict );
}

std::size_t moved( const Frame & a, const Frame & b )
{
	std::size_t n = 0;
	for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ )
		n += a.vel[i * 2] != b.vel[i * 2] || a.vel[i * 2 + 1] != b.vel[i * 2 + 1];
	return n;
}

struct Result
{
	bool ok = true, nothing = false;
};

Result runFormat( int fi, const CamVelRun & cam )
{
	Result res;
	const GLenum fmt = fi == 0 ? kD24s8 : kD32f;
	const char * fn = fi == 0 ? "d24s8" : "d32f";
	gD24 = fmt == kD24s8;
	std::string why;
	auto fail = [&]( const std::string & w ) {
		std::printf( "CAMVELDRAW %s FAIL (%s)\n", fn, w.c_str() );
		res.ok = false;
		return res;
	};

	// the inputs
	std::vector<std::uint16_t> velIn( std::size_t( W ) * H * 2 );
	std::vector<std::uint8_t> maskIn( std::size_t( W ) * H * 4, 0 );
	std::vector<std::uint32_t> d24( std::size_t( W ) * H );
	std::vector<float> d32( std::size_t( W ) * H );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const std::size_t i = std::size_t( y ) * W + x;
			velIn[i * 2] = std::uint16_t( 0x2000 + ( ( x * 7 + y * 13 ) & 0x3FF ) );
			velIn[i * 2 + 1] = std::uint16_t( 0xA000 + ( ( x * 11 + y * 5 ) & 0x3FF ) );
			maskIn[i * 4 + 1] = ( ( x / 3 + y / 5 ) & 1 ) ? 255 : 0;
			maskIn[i * 4 + 3] = ( ( x / 7 + y / 2 ) & 1 ) ? 255 : 0;
			double d = 0.55 + 0.4 * ( 0.5 + 0.5 * std::sin( x / 23.0 ) * std::cos( y / 17.0 ) );
			if ( x >= 200 && x < 240 && y >= 20 && y < 60 )
				d = 1.0;
			if ( x >= 30 && x < 80 && y >= 120 && y < 170 )
				d = 0.2;
			d32[i] = float( d );
			d24[i] = ( std::uint32_t( std::lround( d * 16777215.0 ) ) << 8 ) | 0x5Au;
		}
	const void * dIn = gD24 ? static_cast<const void *>( d24.data() ) : static_cast<const void *>( d32.data() );
	Set in, gl, sh;
	in.v = tex2d( W, H, kRg16f, fRg, fHalf, velIn.data(), false );
	in.m = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, maskIn.data(), false );
	in.d = depthTex( fmt, dIn );
	gl.v = tex2d( W, H, kRg16f, fRg, fHalf, nullptr, false );
	gl.m = tex2d( W, H, fRgba8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false );
	gl.d = depthTex( fmt, nullptr );
	const std::uintptr_t keyBase = 0x43560000u + std::uintptr_t( fi ) * 16u;
	unsigned sv = 0, sm = 0, sd = 0;
	if ( !WwVkPresent::sharedTarget( keyBase + 0, 0, W, H, 1, kRg16f, sv, why )
		|| !WwVkPresent::sharedTarget( keyBase + 1, 0, W, H, 1, fRgba8, sm, why )
		|| !WwVkPresent::sharedTarget( keyBase + 2, 0, W, H, 1, fmt, sd, why ) )
		return fail( "sharedTarget: " + why );
	sh.v = sv;
	sh.m = sm;
	sh.d = sd;
	for ( GLuint t : { sh.v, sh.m, sh.d } ) {
		glBindTexture( GL_TEXTURE_2D, t );
		texParams( GL_TEXTURE_2D, false, false, false );
	}
	glBindTexture( GL_TEXTURE_2D, sh.d );
	glTexParameteri( GL_TEXTURE_2D, kCmpMode, GL_NONE );
	glBindTexture( GL_TEXTURE_2D, 0 );
	gl.fb = makeFb( gl.v );
	sh.fb = makeFb( sh.v );
	if ( !gl.fb || !sh.fb )
		return fail( "framebuffer incomplete" );
	if ( const GLenum e = glGetError() )
		return fail( "GL error 0x" + std::to_string( e ) + " making the inputs" );
	const Frame FIN = readSet( in );

	// GL's reference
	reset( in, gl );
	if ( const GLenum e = glQuad( gl, cam ) )
		return fail( "GL error " + std::to_string( e ) + " in the reference" );
	const Frame G = readSet( gl );
	std::size_t nz = 0, same = 0;
	for ( std::size_t i = 0; i < std::size_t( W ) * H; i++ ) {
		nz += ( G.vel[i * 2] & 0x7FFF ) != 0 || ( G.vel[i * 2 + 1] & 0x7FFF ) != 0;
		same += G.vel[i * 2] == FIN.vel[i * 2] && G.vel[i * 2 + 1] == FIN.vel[i * 2 + 1];
	}
	std::printf( "CAMVELDRAW %s gl vel non-zero on %zu of %d texels (%.1f %%), equal to the input pattern on %zu\n", fn, nz, W * H,
		100.0 * double( nz ) / double( W * H ), same );
	if ( nz * 2 < std::size_t( W ) * H )
		res.nothing = true;

	// Vulkan through the route
	CamVelRun base = cam;
	base.velKey = keyBase + 0;
	base.depthKey = keyBase + 2;
	base.w = W;
	base.h = H;
	reset( in, sh );
	if ( !WwVkPresent::camVelRun( base, why ) )
		return fail( "the route refused the fixture: " + why );
	const Frame V = readSet( sh );
	const Judge jm = judge( FIN, G, V );
	printJudge( fn, "main", jm, jm.ok() ? ( jm.velDiff == 0 ? "EXACT" : "BOUND" ) : "FAIL" );
	if ( !jm.ok() )
		res.ok = false;

	// the reds
	const char * redName[4] = { "", "no-jitter", "reproj-row-late", "zero-depth" };
	for ( int k = 1; k <= 3; k++ ) {
		CamVelRun rr = base;
		rr.red = k;
		reset( in, sh );
		std::string w;
		if ( !WwVkPresent::camVelRun( rr, w ) ) {
			std::printf( "CAMVELDRAW %s RED %d %s refused (%s): not a FAIL\n", fn, k, redName[k], w.c_str() );
			res.ok = false;
			continue;
		}
		const Frame R = readSet( sh );
		const Judge jr = judge( FIN, G, R );
		printJudge( fn, ( std::string( "RED " ) + std::to_string( k ) + " " + redName[k] ).c_str(), jr, jr.ok() ? "PASSED (bad)" : "FAIL" );
		const std::size_t mv = moved( V, R );
		std::printf( "CAMVELDRAW %s RED %d moved %zu texels against the unred Vulkan run\n", fn, k, mv );
		if ( jr.ok() )
			res.ok = false;
		if ( mv * 100 < std::size_t( W ) * H )
			res.nothing = true;
	}

	// a refusal: by name, the targets untouched
	{
		CamVelRun rr = base;
		rr.w = W / 2;
		reset( in, sh );
		const Frame pre = readSet( sh );
		std::string w;
		const bool ran = WwVkPresent::camVelRun( rr, w );
		const Frame post = readSet( sh );
		const bool named = !ran && w.find( "not the pass's size" ) != std::string::npos, eq = pre == post;
		std::printf( "CAMVELDRAW %s refuse size: %s (%s), targets %s -> %s\n", fn, ran ? "RAN" : "refused", w.c_str(),
			eq ? "byte-equal" : "CHANGED", named && eq ? "OK" : "FAIL" );
		if ( !( named && eq ) )
			res.ok = false;
	}
	// GL's own quad into the shared targets (what the app does after a refusal) equals the plain reference
	reset( in, sh );
	glQuad( sh, base );
	const Frame GS = readSet( sh );
	const bool gsSame = GS.vel == G.vel && GS.mask == FIN.mask && GS.depth == FIN.depth;
	std::printf( "CAMVELDRAW %s gl into the shared targets: %s\n", fn, gsSame ? "EXACT" : "FAIL" );
	if ( !gsSame )
		res.ok = false;
	const WwVkPresent::CamVelStat st = WwVkPresent::takeCamVelStat();
	std::printf( "CAMVELDRAW %s stat: runs %d gl %d fails %d pipelines %d cpu %.2f ms (max %.2f, build %.1f)%s%s\n", fn, st.runs, st.gl,
		st.fails, st.pipelines, st.cpuMs, st.maxMs, st.buildMs, st.failWhy.empty() ? "" : " last fail: ", st.failWhy.c_str() );
	return res;
}
}  // namespace cvd

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace cvd;
	gTag = "CAMVELDRAW";
	auto fatal = [&]( const std::string & why ) {
		std::printf( "CAMVELDRAW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	if ( argc < 3 )
		return fatal( "usage: camveldraw <out dir> <tree root>" );
	const std::string root = argv[2];
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwcamvel";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwcamvel", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwcamvel", "camvel harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0 };
	int pf = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &pf, &nf ) || !nf )
		return fatal( "no pixel format" );
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, pf, sizeof( got ), &got );
	SetPixelFormat( dc, pf, &got );
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
	std::printf( "CAMVELDRAW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	pMv = program( readFile( root + "/res/shaders/cell_ao.vert" ), readFile( root + "/res/shaders/features/post/game_taa_mv.frag" ),
		"game_taa_mv" );
	if ( !pMv )
		return fatal( "GL program" );
	// taainputs.cpp's quad
	static const float quad[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	ox.genVertexArrays( 1, &quadVao );
	ox.bindVertexArray( quadVao );
	GLuint b[2] = {};
	f.genBuffers( 2, b );
	f.bindBuffer( kArr, b[0] );
	f.bufferData( kArr, sizeof( quad ), quad, fStatic );
	ox.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, nullptr );
	ox.enableVertexAttribArray( 0 );
	f.bindBuffer( kElem, b[1] );
	f.bufferData( kElem, sizeof( idx ), idx, fStatic );
	ox.bindVertexArray( 0 );
	f.bindBuffer( kArr, 0 );
	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, 1, glProc, why ) )
		return fatal( "the view: " + why );
	const std::string refusal = WwVkPresent::camVelRefusal();
	if ( !refusal.empty() )
		return fatal( "camVelRefusal: " + refusal );

	// the camera: prevVP x inverse( curVP ), in double
	const D4 P = frustum( -0.42, 0.38, -0.30, 0.28, 0.5, 20.0 );
	const D4 viewPrev = ident();
	const D4 viewCur = mul( rotY( 0.21 ), trans( -0.4, 0.15, -0.6 ) );
	D4 inv;
	if ( !inverse( mul( P, viewCur ), inv ) )
		return fatal( "curVP has no inverse" );
	const D4 reproj = mul( mul( P, viewPrev ), inv );
	CamVelRun cam;
	for ( int i = 0; i < 16; i++ )
		cam.reproj[i] = float( reproj.m[i] );
	cam.jitterNdc[0] = float( 2.0 * 2.6 / W );
	cam.jitterNdc[1] = float( 2.0 * -1.7 / H );
	cam.invSize[0] = 1.0f / float( W );
	cam.invSize[1] = 1.0f / float( H );

	bool ok = true, nothing = false;
	for ( int fi = 0; fi < 2; fi++ ) {
		const Result r = runFormat( fi, cam );
		ok = ok && r.ok;
		nothing = nothing || r.nothing;
	}
	WwVkPresent::shutdown();
	if ( nothing )
		std::printf( "CAMVELDRAW TESTS NOTHING (GL vel non-zero under 50 %%, or a red moving under 1 %% of texels)\n" );
	std::printf( "CAMVELDRAW GATE %s\n", ok && !nothing ? "OK" : "FAILED" );
	return ok && !nothing ? 0 : 1;
}
