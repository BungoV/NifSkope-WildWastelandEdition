// Lane VKWATER4 (2026-11-15, plan row G5): the ultra ocean's build (src/features/water/cellocean.cpp: spectrum, 14
// Stockham FFT stages, the six layers, 7 mips) through the shipped WwVkPresent::lightRun as pass LightOcean
// (res/shaders/hlsl/vklight/ocean_vk.hlsl, the light_ocean_*.spv.h pipelines) against GL's own programs
// (ocean_spectrum / ocean_fft / ocean_final (+ ocean_layers.geom) / ocean_mip .prog as the app links them, written by
// waterdraw_prep.py) drawn with cellocean.cpp's state: the same textures, formats, targets, layered attachments and
// base = max level = lv - 1 per mip draw. fxview.cpp's helpers are reused (its main renamed).
// Inputs: oceanspectrum.h (params + h0Tiles), winds 5 and 12 m/s (swell 0.344 m) x phases 0.137 and 0.618, the app's dk.
// Bars (set 11-15 before the first run, notes/vkwater4/README.md; never widened). fp32 ulp, half ulp on RGBA16F:
//   each stage in isolation (GL's own stage N-1 output in): spectrum <= 1 ulp, every FFT stage <= 1 ulp, final EXACT,
//   every mip level EXACT (the arrays read back from GL after lightRun's copy into their layers);
//   end to end (all 31 dispatches from h0 / q in one run): every value |vk - gl| <= 1e-5 x max |gl| of that array level.
// Floors (on GL's outputs; amended before the first run, the reason from oceanspectrum.h's band edges: the six tiles'
//   bands hold ~42 % of the modes, so a spectrum output is ~58 % zero by design): spectrum: on the texels with h0 != 0,
//   >= 90 % of values non-zero and >= 50 % differ from h0; FFT: pooled over a case's 14 stages, >= 50 % non-zero and
//   >= 50 % differ from the stage's input; final: B, C, D >= 50 % non-zero, C and D >= 50 % differ from x0 / x1 (B is
//   x0 copied by design); mips: B, C, D of each level pooled, >= 50 % non-zero, >= 50 % differ from the level above's
//   texel 2p. The end-to-end arrays are the stages' own (floored there).
// Reds (each must FAIL, never refuse): spectrum dk one tile on; FFT stage 4 axis swapped; final tile offset 0 (tile 0 in
//   every layer); mip level 2 from level 0 (two levels up); end to end with FFT stage 7 skipped.
// bash tests/vkframe/oceanview.sh <out dir>. Last line `OCEANVIEW GATE OK` or `OCEANVIEW GATE FAILED`.
#define main fxview_main
#include "fxview.cpp"
#undef main

#include "../../src/features/water/oceanspectrum.h"

#include <functional>

namespace ov
{
using namespace fv;
using WwVkPresent::LightDispatch;
using WwVkPresent::LightImage;
using WwVkPresent::LightRun;

struct Ex
{
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	void( WINAPI * vertexAttribPointer )( GLuint, GLint, GLenum, GLboolean, GLsizei, const void * ) = nullptr;
	void( WINAPI * enableVertexAttribArray )( GLuint ) = nullptr;
	void( WINAPI * framebufferTexture )( GLenum, GLenum, GLuint, GLint ) = nullptr;
};
Ex ex;

bool loadMore()
{
	ex.genVertexArrays = reinterpret_cast<decltype( ex.genVertexArrays )>( glProc( "glGenVertexArrays" ) );
	ex.bindVertexArray = reinterpret_cast<decltype( ex.bindVertexArray )>( glProc( "glBindVertexArray" ) );
	ex.vertexAttribPointer = reinterpret_cast<decltype( ex.vertexAttribPointer )>( glProc( "glVertexAttribPointer" ) );
	ex.enableVertexAttribArray = reinterpret_cast<decltype( ex.enableVertexAttribArray )>( glProc( "glEnableVertexAttribArray" ) );
	ex.framebufferTexture = reinterpret_cast<decltype( ex.framebufferTexture )>( glProc( "glFramebufferTexture" ) );
	return ex.genVertexArrays && ex.bindVertexArray && ex.vertexAttribPointer && ex.enableVertexAttribArray && ex.framebufferTexture;
}

constexpr GLenum kGeom = 0x8DD9, kArrayBuf = 0x8892, kElemBuf = 0x8893, kBase = 0x813C, kAniso = 0x84FE, kAnisoMax = 0x84FF;
constexpr int kN = WwOcean::kN, kT = WwOcean::kTiles, kW = kN * kT, kLv = 8;

GLuint program3( const std::string & vs, const std::string & gs, const std::string & fs, const char * tag )
{
	if ( gs.empty() || fs.empty() ) {
		std::printf( "%s no source (%s)\n", gTag, tag );
		return 0;
	}
	const GLuint a = shader( fVert, vs, tag ), g = shader( kGeom, gs, tag ), b = shader( fFrag, fs, tag );
	if ( !a || !g || !b )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, g );
	f.attachShader( p, b );
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, fLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "%s link failed (%s): %.1500s\n", gTag, tag, log.data() );
		return 0;
	}
	return p;
}

// cellocean.cpp texArr: 8 levels, trilinear, repeat, anisotropic
GLuint texArr( GLenum ifmt )
{
	GLuint t = 0;
	glGenTextures( 1, &t );
	glBindTexture( fT2DA, t );
	for ( int lv = 0; lv < kLv; lv++ )
		f.texImage3D( fT2DA, lv, GLint( ifmt ), kN >> lv, kN >> lv, kT, 0, GL_RGBA, GL_FLOAT, nullptr );
	glTexParameteri( fT2DA, GL_TEXTURE_MIN_FILTER, GLint( fMipLinear ) );
	glTexParameteri( fT2DA, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( fT2DA, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( fT2DA, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTexParameteri( fT2DA, kBase, 0 );
	glTexParameteri( fT2DA, fMaxLevel, kLv - 1 );
	GLfloat an = 1.0f;
	glGetFloatv( kAnisoMax, &an );
	glTexParameterf( fT2DA, kAniso, std::min( 16.0f, an ) );
	glBindTexture( fT2DA, 0 );
	return t;
}

// a texture read back: raw bits (a half widened) and the values
struct Got
{
	bool half = false;
	std::vector<std::uint32_t> bits;
	std::vector<float> v;
};

Got grab( GLenum target, GLuint t, int lv, bool half, size_t texels )
{
	Got g;
	g.half = half;
	const size_t n = texels * 4;
	g.bits.resize( n );
	g.v.resize( n );
	glBindTexture( target, t );
	if ( half ) {
		std::vector<std::uint16_t> h( n );
		glGetTexImage( target, lv, GL_RGBA, fHalf, h.data() );
		for ( size_t k = 0; k < n; k++ ) {
			g.bits[k] = h[k];
			g.v[k] = h2f( h[k] );
		}
	} else {
		glGetTexImage( target, lv, GL_RGBA, GL_FLOAT, g.v.data() );
		std::memcpy( g.bits.data(), g.v.data(), n * 4 );
	}
	glBindTexture( target, 0 );
	return g;
}

Got grab2d( GLuint t, int w, int h, bool half = false )
{
	return grab( GL_TEXTURE_2D, t, 0, half, size_t( w ) * size_t( h ) );
}

Got grabArr( GLuint t, int lv, bool half )
{
	return grab( fT2DA, t, lv, half, size_t( kN >> lv ) * size_t( kN >> lv ) * kT );
}

int ulpsOf( std::uint32_t a, std::uint32_t b, bool half )
{
	if ( half ) {
		auto nan = []( std::uint32_t x ) { return ( x & 0x7C00 ) == 0x7C00 && ( x & 0x03FF ); };
		if ( nan( a ) || nan( b ) )
			return nan( a ) == nan( b ) ? 0 : 1 << 20;
		return std::abs( ordered( std::uint16_t( a ) ) - ordered( std::uint16_t( b ) ) );
	}
	auto ord = []( std::uint32_t x ) { return ( x & 0x80000000u ) ? -std::int64_t( x & 0x7FFFFFFFu ) : std::int64_t( x ); };
	auto nan = []( std::uint32_t x ) { return ( x & 0x7F800000u ) == 0x7F800000u && ( x & 0x007FFFFFu ); };
	if ( nan( a ) || nan( b ) )
		return nan( a ) == nan( b ) ? 0 : 1 << 30;
	const std::int64_t d = ord( a ) - ord( b );
	return int( std::min<std::int64_t>( d < 0 ? -d : d, 1 << 30 ) );
}

struct Cmp
{
	size_t n = 0, d = 0, big = 0;
	int mu = 0;
	double maxAbs = 0.0, maxGl = 0.0;
	bool nanBad = false;
};

Cmp compare( const Got & vk, const Got & gl, int tol )
{
	Cmp c;
	if ( vk.bits.size() != gl.bits.size() || vk.half != gl.half ) {
		c.nanBad = true;
		c.mu = 1 << 30;
		return c;
	}
	c.n = gl.bits.size();
	for ( size_t k = 0; k < c.n; k++ ) {
		const int u = ulpsOf( vk.bits[k], gl.bits[k], gl.half );
		c.d += u != 0;
		c.big += u > tol;
		c.mu = std::max( c.mu, u );
		const float a = vk.v[k], b = gl.v[k];
		if ( ( a != a ) != ( b != b ) )
			c.nanBad = true;
		else if ( a == a ) {
			c.maxAbs = std::max( c.maxAbs, std::fabs( double( a ) - double( b ) ) );
			c.maxGl = std::max( c.maxGl, std::fabs( double( b ) ) );
		}
	}
	return c;
}

int gGreens = 0, gReds = 0, gGreenBad = 0, gRedBad = 0, gFloorBad = 0;

// info line: differing values per RGBA channel of a 4-channel grab
void chanInfo( const std::string & name, const char * out, const Got & vk, const Got & gl )
{
	size_t d[4] = {};
	for ( size_t k = 0; k < gl.bits.size() && k < vk.bits.size(); k++ )
		d[k & 3] += vk.bits[k] != gl.bits[k];
	std::printf( "    info %s %s differ per channel: x %zu y %zu z %zu w %zu\n", name.c_str(), out, d[0], d[1], d[2], d[3] );
}

// a stage's verdict: rel < 0 the ulp bar (tol), else |vk - gl| <= rel x max |gl|. Returns true when it passes.
bool verdict( const std::string & name, const std::vector<std::pair<std::string, Cmp>> & cs, int tol, double rel )
{
	bool pass = true, exact = true;
	for ( const auto & nc : cs ) {
		const Cmp & c = nc.second;
		const bool p = rel < 0.0 ? c.mu <= tol && !c.nanBad : !c.nanBad && c.maxAbs <= rel * c.maxGl;
		pass = pass && p;
		exact = exact && c.d == 0 && !c.nanBad;
		std::printf( "    %s %s: %zu of %zu values differ, %zu by > %d ulp, max %d ulp, max abs %.3g of max |gl| %.3g%s\n", name.c_str(),
			nc.first.c_str(), c.d, c.n, c.big, tol, c.mu, c.maxAbs, c.maxGl, p ? "" : " OUT" );
	}
	std::printf( "  %s: %s\n", name.c_str(), !pass ? "FAIL" : exact ? "EXACT" : "BOUND" );
	return pass;
}

void green( const std::string & name, bool ran, bool pass )
{
	gGreens++;
	if ( !ran || !pass ) {
		gGreenBad++;
		std::printf( "  GREEN %s %s\n", name.c_str(), ran ? "OUT OF BAR" : "DID NOT RUN" );
	}
}

void red( const std::string & name, bool ran, bool pass )
{
	gReds++;
	if ( !ran || pass ) {
		gRedBad++;
		std::printf( "  RED %s %s\n", name.c_str(), ran ? "DID NOT FAIL" : "DID NOT RUN (refused)" );
	}
}

void floorLine( const std::string & name, size_t n, size_t nz, size_t dif, double nzMin, bool difOn )
{
	const double a = n ? double( nz ) / double( n ) : 0.0, b = n ? double( dif ) / double( n ) : 0.0;
	const bool ok = n > 0 && a >= nzMin && ( !difOn || b >= 0.5 );
	gFloorBad += !ok;
	std::printf( "  floor %s: %zu values, %.1f %% non-zero (>= %.0f %%), %.1f %% differ from the input%s: %s\n", name.c_str(), n, 100.0 * a,
		100.0 * nzMin, 100.0 * b, difOn ? " (>= 50 %)" : " (info: a copy)", ok ? "OK" : "LOW" );
}

LightImage img( GLuint tex, GLenum fmt, int w, int h, bool in, bool out, int tiles = 0, int outLevel = 0 )
{
	LightImage x;
	x.tex = tex;
	x.glFmt = fmt;
	x.w = w;
	x.h = h;
	x.in = in;
	x.out = out;
	x.tiles = tiles;
	x.outLevel = outLevel;
	return x;
}

LightDispatch disp( int kernel, int out0, int out1, int t0, int t1 )
{
	LightDispatch d;
	d.kernel = kernel;
	d.out0 = out0;
	d.out1 = out1;
	d.t[0] = t0;
	d.t[1] = t1;
	return d;
}

void pcF( LightDispatch & d, int slot, float v )	// slot = c index * 4 + component
{
	std::memcpy( d.pc + slot * 4, &v, 4 );
}

void pcI( LightDispatch & d, int comp, int v )	// LpxJob.i
{
	std::memcpy( d.pc + 112 + comp * 4, &v, 4 );
}

bool run( const LightRun & t, const std::string & name )
{
	std::string why;
	const bool ok = WwVkPresent::lightRun( t, why );
	if ( !ok )
		std::printf( "  %s: lightRun refused: %s\n", name.c_str(), why.c_str() );
	return ok;
}

struct GlSide
{
	GLuint h0 = 0, q = 0, ping[2][2] = {}, fb[2] = {}, arrB = 0, arrC = 0, arrD = 0, fbArr = 0;
	GLuint F[15][2] = {};	// GL's spectrum (0) and each FFT stage's output (1..14)
	GLuint pSpec = 0, pFft = 0, pFin = 0, pMip = 0;
};

void bind2( GLuint t0, GLuint t1 )
{
	f.activeTexture( fTex0 + 0 );
	glBindTexture( fT2DA, 0 );
	glBindTexture( GL_TEXTURE_2D, t0 );
	f.activeTexture( fTex0 + 1 );
	glBindTexture( fT2DA, 0 );
	glBindTexture( GL_TEXTURE_2D, t1 );
}

void copy2d( GLuint from, GLuint to )
{
	f.copyImageSubData( from, GL_TEXTURE_2D, 0, 0, 0, 0, to, GL_TEXTURE_2D, 0, 0, 0, 0, kW, kN, 1 );
}

int uni( GLuint p, const char * n )
{
	const GLint l = f.getUniformLocation( p, n );
	if ( l < 0 ) {
		std::printf( "  GL uniform %s not active\n", n );
		gGreenBad++;
	}
	return l;
}

// cellocean.cpp's draws, its state and order; each stage's output kept (F) for the stages in isolation
void glChain( GlSide & g, float phase, const float dk[6], const std::function<void()> & quad )
{
	const GLenum bufs[3] = { fCol0, fCol0 + 1, fCol0 + 2 };
	f.useProgram( g.pSpec );
	f.bindFramebuffer( fFb, g.fb[0] );
	glViewport( 0, 0, kW, kN );
	bind2( g.h0, g.q );
	f.uniform1i( uni( g.pSpec, "oceanH0" ), 0 );
	f.uniform1i( uni( g.pSpec, "oceanQ" ), 1 );
	f.uniform1fv( uni( g.pSpec, "oceanPhase" ), 1, &phase );
	f.uniform1fv( uni( g.pSpec, "oceanDk" ), 6, dk );
	quad();
	copy2d( g.ping[0][0], g.F[0][0] );
	copy2d( g.ping[0][1], g.F[0][1] );
	int cur = 0, n = 0;
	f.useProgram( g.pFft );
	f.uniform1i( uni( g.pFft, "oceanIn0" ), 0 );
	f.uniform1i( uni( g.pFft, "oceanIn1" ), 1 );
	for ( int ax = 0; ax < 2; ax++ ) {
		f.uniform1i( uni( g.pFft, "oceanAxis" ), ax );
		for ( int st = 0; st < 7; st++ ) {
			f.bindFramebuffer( fFb, g.fb[1 - cur] );
			bind2( g.ping[cur][0], g.ping[cur][1] );
			f.uniform1i( uni( g.pFft, "oceanNs" ), 1 << st );
			quad();
			cur = 1 - cur;
			n++;
			copy2d( g.ping[cur][0], g.F[n][0] );
			copy2d( g.ping[cur][1], g.F[n][1] );
		}
	}
	f.useProgram( g.pFin );
	bind2( g.ping[cur][0], g.ping[cur][1] );
	f.uniform1i( uni( g.pFin, "oceanX0" ), 0 );
	f.uniform1i( uni( g.pFin, "oceanX1" ), 1 );
	f.bindFramebuffer( fFb, g.fbArr );
	ex.framebufferTexture( fFb, fCol0, g.arrB, 0 );
	ex.framebufferTexture( fFb, fCol0 + 1, g.arrC, 0 );
	ex.framebufferTexture( fFb, fCol0 + 2, g.arrD, 0 );
	f.drawBuffers( 3, bufs );
	glViewport( 0, 0, kN, kN );
	quad();
	f.useProgram( g.pMip );
	f.uniform1i( uni( g.pMip, "oceanB" ), 0 );
	f.uniform1i( uni( g.pMip, "oceanC" ), 1 );
	f.uniform1i( uni( g.pMip, "oceanD" ), 2 );
	const GLuint arr[3] = { g.arrB, g.arrC, g.arrD };
	for ( int k = 0; k < 3; k++ ) {
		f.activeTexture( fTex0 + k );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glBindTexture( fT2DA, arr[k] );
	}
	for ( int lv = 1; lv < kLv; lv++ ) {
		for ( int k = 0; k < 3; k++ ) {
			f.activeTexture( fTex0 + k );
			glTexParameteri( fT2DA, kBase, lv - 1 );
			glTexParameteri( fT2DA, fMaxLevel, lv - 1 );
		}
		glViewport( 0, 0, kN >> lv, kN >> lv );
		ex.framebufferTexture( fFb, fCol0, g.arrB, lv );
		ex.framebufferTexture( fFb, fCol0 + 1, g.arrC, lv );
		ex.framebufferTexture( fFb, fCol0 + 2, g.arrD, lv );
		quad();
	}
	for ( int k = 0; k < 3; k++ ) {
		f.activeTexture( fTex0 + k );
		glTexParameteri( fT2DA, kBase, 0 );
		glTexParameteri( fT2DA, fMaxLevel, kLv - 1 );
		glBindTexture( fT2DA, 0 );
	}
	f.activeTexture( fTex0 );
	f.useProgram( 0 );
	f.bindFramebuffer( fFb, 0 );
	glFinish();
}

// the array's level lv layers side by side into a strip (the stages' layout)
void arrToStrip( GLuint arr, int lv, GLuint strip )
{
	const int tw = kN >> lv;
	for ( int l = 0; l < kT; l++ )
		f.copyImageSubData( arr, fT2DA, lv, 0, 0, l, strip, GL_TEXTURE_2D, 0, l * tw, 0, 0, tw, tw, 1 );
}

// end to end: the 31 dispatches the app records (skip = the FFT stage left out, 1-based; 0 none)
LightRun e2e( GLuint h0, GLuint q, const GLuint outArr[3], float phase, const float dk[6], int skip )
{
	using namespace WwVkPresent;
	LightRun t;
	t.pass = LightOcean;
	t.img.push_back( img( h0, fRgba32f, kW, kN, true, false ) );
	t.img.push_back( img( q, fR32f, kW, kN, true, false ) );
	for ( int k = 0; k < 4; k++ )
		t.img.push_back( img( 0, fRgba32f, kW, kN, false, false ) );	// ping [0][0], [0][1], [1][0], [1][1]
	const GLenum fm[3] = { fRgba16f, fRgba32f, fRgba16f };
	for ( int lv = 0; lv < kLv; lv++ )
		for ( int k = 0; k < 3; k++ )
			t.img.push_back( img( outArr[k], fm[k], kW >> lv, kN >> lv, false, true, kT, lv ) );
	auto strip = [&]( int lv, int k ) { return 6 + lv * 3 + k; };
	LightDispatch s = disp( LightOceanSpecK, 2, 3, 0, 1 );
	pcF( s, 0, phase );
	for ( int k = 0; k < 6; k++ )
		pcF( s, 4 + k, dk[k] );
	t.d.push_back( s );
	int cur = 0, n = 0;
	for ( int ax = 0; ax < 2; ax++ )
		for ( int st = 0; st < 7; st++ ) {
			if ( ++n == skip )
				continue;
			LightDispatch d = disp( LightOceanFftK, 2 + 2 * ( 1 - cur ), 3 + 2 * ( 1 - cur ), 2 + 2 * cur, 3 + 2 * cur );
			pcI( d, 0, 1 << st );
			pcI( d, 1, ax );
			t.d.push_back( d );
			cur = 1 - cur;
		}
	LightDispatch fbc = disp( LightOceanFinBcK, strip( 0, 0 ), strip( 0, 1 ), 2 + 2 * cur, 3 + 2 * cur );
	pcI( fbc, 0, kN );
	t.d.push_back( fbc );
	LightDispatch fd = disp( LightOceanFinDK, strip( 0, 2 ), -1, 3 + 2 * cur, -1 );
	pcI( fd, 0, kN );
	t.d.push_back( fd );
	for ( int lv = 1; lv < kLv; lv++ ) {
		t.d.push_back( disp( LightOceanMipBcK, strip( lv, 0 ), strip( lv, 1 ), strip( lv - 1, 0 ), strip( lv - 1, 1 ) ) );
		t.d.push_back( disp( LightOceanMipDK, strip( lv, 2 ), -1, strip( lv - 1, 2 ), -1 ) );
	}
	return t;
}

struct Case
{
	double U, swell;
	float phase;
};

void runCase( GlSide & g, const Case & c, const std::function<void()> & quad )
{
	using namespace WwVkPresent;
	char nm[64];
	std::snprintf( nm, sizeof( nm ), "U%g ph%.3f", c.U, double( c.phase ) );
	const std::string cs = nm;
	std::printf( "OCEANVIEW case %s (wind %.1f m/s, swell %.3f m, phase %.3f)\n", nm, c.U, c.swell, double( c.phase ) );
	WwOcean::SetParams P[2];
	WwOcean::params( c.U, c.swell, P );
	std::vector<float> h0, q;
	WwOcean::h0Tiles( P, h0, q );
	glBindTexture( GL_TEXTURE_2D, g.h0 );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kW, kN, GL_RGBA, GL_FLOAT, h0.data() );
	glBindTexture( GL_TEXTURE_2D, g.q );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kW, kN, fRed, GL_FLOAT, q.data() );
	glBindTexture( GL_TEXTURE_2D, 0 );
	float dk[6], dkRed[6];
	for ( int t = 0; t < kT; t++ )
		dk[t] = float( 2.0 * WwOcean::kPi / WwOcean::set( t / 3 ).L[t % 3] );
	for ( int t = 0; t < kT; t++ )
		dkRed[t] = dk[( t + 1 ) % kT];
	glChain( g, c.phase, dk, quad );

	GLuint V[2] = { tex2d( kW, kN, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false ), tex2d( kW, kN, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false ) };
	const GLuint VA[3] = { texArr( fRgba16f ), texArr( fRgba32f ), texArr( fRgba16f ) }, EA[3] = { texArr( fRgba16f ), texArr( fRgba32f ),
		texArr( fRgba16f ) };
	const GLuint GA[3] = { g.arrB, g.arrC, g.arrD };
	const bool half[3] = { true, false, true };
	const char * an[3] = { "B", "C", "D" };
	const GLenum fm[3] = { fRgba16f, fRgba32f, fRgba16f };

	// 1 spectrum (and its red: dk one tile on)
	for ( int r = 0; r < 2; r++ ) {
		LightRun t;
		t.pass = LightOcean;
		t.img = { img( g.h0, fRgba32f, kW, kN, true, false ), img( g.q, fR32f, kW, kN, true, false ), img( V[0], fRgba32f, kW, kN, false, true ),
			img( V[1], fRgba32f, kW, kN, false, true ) };
		LightDispatch d = disp( LightOceanSpecK, 2, 3, 0, 1 );
		pcF( d, 0, c.phase );
		for ( int k = 0; k < 6; k++ )
			pcF( d, 4 + k, ( r ? dkRed : dk )[k] );
		t.d.push_back( d );
		const std::string name = cs + ( r ? " RED spectrum dk one tile on" : " spectrum" );
		const bool ran = run( t, name );
		bool pass = false;
		if ( ran ) {
			const Got v0 = grab2d( V[0], kW, kN ), v1 = grab2d( V[1], kW, kN ), g0 = grab2d( g.F[0][0], kW, kN ), g1 = grab2d( g.F[0][1], kW, kN );
			pass = verdict( name, { { "o0", compare( v0, g0, 1 ) }, { "o1", compare( v1, g1, 1 ) } }, 1, -1.0 );
			if ( !r ) {
				chanInfo( name, "o0", v0, g0 );
				chanInfo( name, "o1", v1, g1 );
				size_t n = 0, nz = 0, dif = 0;
				for ( size_t p = 0; p < size_t( kW ) * kN; p++ ) {
					if ( !h0[p * 4] && !h0[p * 4 + 1] && !h0[p * 4 + 2] && !h0[p * 4 + 3] )
						continue;
					for ( int k = 0; k < 4; k++ )
						for ( const Got * o : { &g0, &g1 } ) {
							n++;
							nz += o->v[p * 4 + k] != 0.0f;
							dif += o->v[p * 4 + k] != h0[p * 4 + k];
						}
				}
				floorLine( cs + " spectrum (texels with h0 != 0)", n, nz, dif, 0.9, true );
			}
		}
		( r ? red : green )( name, ran, pass );
	}

	// 2 each FFT stage on GL's own input (and the red: stage 4 with the axis swapped)
	size_t fn = 0, fnz = 0, fdif = 0;
	for ( int s = 1; s <= 14; s++ )
		for ( int r = 0; r < ( s == 4 ? 2 : 1 ); r++ ) {
			const int ax = ( s - 1 ) / 7, st = ( s - 1 ) % 7;
			LightRun t;
			t.pass = LightOcean;
			t.img = { img( g.F[s - 1][0], fRgba32f, kW, kN, true, false ), img( g.F[s - 1][1], fRgba32f, kW, kN, true, false ),
				img( V[0], fRgba32f, kW, kN, false, true ), img( V[1], fRgba32f, kW, kN, false, true ) };
			LightDispatch d = disp( LightOceanFftK, 2, 3, 0, 1 );
			pcI( d, 0, 1 << st );
			pcI( d, 1, r ? 1 - ax : ax );
			t.d.push_back( d );
			const std::string name = cs + ( r ? " RED fft stage 4 axis swapped" : " fft stage " + std::to_string( s ) );
			const bool ran = run( t, name );
			bool pass = false;
			if ( ran ) {
				const Got v0 = grab2d( V[0], kW, kN ), v1 = grab2d( V[1], kW, kN ), g0 = grab2d( g.F[s][0], kW, kN ), g1 = grab2d( g.F[s][1], kW, kN );
				pass = verdict( name, { { "o0", compare( v0, g0, 1 ) }, { "o1", compare( v1, g1, 1 ) } }, 1, -1.0 );
				if ( !r ) {
					const Got i0 = grab2d( g.F[s - 1][0], kW, kN ), i1 = grab2d( g.F[s - 1][1], kW, kN );
					for ( size_t k = 0; k < g0.v.size(); k++ ) {
						fn += 2;
						fnz += ( g0.v[k] != 0.0f ) + ( g1.v[k] != 0.0f );
						fdif += ( g0.v[k] != i0.v[k] ) + ( g1.v[k] != i1.v[k] );
					}
				}
			}
			( r ? red : green )( name, ran, pass );
		}
	floorLine( cs + " fft (14 stages pooled)", fn, fnz, fdif, 0.5, true );

	// 3 the layers on GL's last stage (and the red: tile offset 0)
	for ( int r = 0; r < 2; r++ ) {
		LightRun t;
		t.pass = LightOcean;
		t.img = { img( g.F[14][0], fRgba32f, kW, kN, true, false ), img( g.F[14][1], fRgba32f, kW, kN, true, false ) };
		for ( int k = 0; k < 3; k++ )
			t.img.push_back( img( VA[k], fm[k], kW, kN, false, true, kT, 0 ) );
		LightDispatch bc = disp( LightOceanFinBcK, 2, 3, 0, 1 ), dd = disp( LightOceanFinDK, 4, -1, 1, -1 );
		pcI( bc, 0, r ? 0 : kN );
		pcI( dd, 0, r ? 0 : kN );
		t.d = { bc, dd };
		const std::string name = cs + ( r ? " RED final tile offset 0" : " final" );
		const bool ran = run( t, name );
		bool pass = false;
		if ( ran ) {
			std::vector<std::pair<std::string, Cmp>> cmps;
			Got gl[3];
			for ( int k = 0; k < 3; k++ ) {
				gl[k] = grabArr( GA[k], 0, half[k] );
				cmps.push_back( { an[k], compare( grabArr( VA[k], 0, half[k] ), gl[k], 0 ) } );
			}
			pass = verdict( name, cmps, 0, -1.0 );
			if ( !r ) {
				const Got x0 = grab2d( g.F[14][0], kW, kN ), x1 = grab2d( g.F[14][1], kW, kN );
				for ( int k = 0; k < 3; k++ ) {
					size_t n = 0, nz = 0, dif = 0;
					const Got & in = k == 2 ? x1 : x0;
					for ( int l = 0; l < kT; l++ )
						for ( int y = 0; y < kN; y++ )
							for ( int x = 0; x < kN; x++ )
								for ( int e = 0; e < 4; e++ ) {
									const float o = gl[k].v[( ( size_t( l ) * kN + y ) * kN + x ) * 4 + e];
									n++;
									nz += o != 0.0f;
									dif += o != in.v[( size_t( y ) * kW + l * kN + x ) * 4 + e];
								}
					floorLine( cs + " final " + an[k], n, nz, dif, 0.5, k != 0 );
				}
			}
		}
		( r ? red : green )( name, ran, pass );
	}

	// 4 each mip level on GL's level above (and the red: level 2 from level 0)
	for ( int lv = 1; lv < kLv; lv++ )
		for ( int r = 0; r < ( lv == 2 ? 2 : 1 ); r++ ) {
			const int src = r ? lv - 2 : lv - 1, sw = kW >> src, sh = kN >> src;
			GLuint S[3];
			for ( int k = 0; k < 3; k++ ) {
				S[k] = tex2d( sw, sh, fm[k], GL_RGBA, GL_FLOAT, nullptr, false );
				arrToStrip( GA[k], src, S[k] );
			}
			LightRun t;
			t.pass = LightOcean;
			for ( int k = 0; k < 3; k++ )
				t.img.push_back( img( S[k], fm[k], sw, sh, true, false ) );
			for ( int k = 0; k < 3; k++ )
				t.img.push_back( img( VA[k], fm[k], kW >> lv, kN >> lv, false, true, kT, lv ) );
			t.d = { disp( LightOceanMipBcK, 3, 4, 0, 1 ), disp( LightOceanMipDK, 5, -1, 2, -1 ) };
			const std::string name = cs + ( r ? " RED mip 2 from level 0" : " mip " + std::to_string( lv ) );
			const bool ran = run( t, name );
			bool pass = false;
			if ( ran ) {
				std::vector<std::pair<std::string, Cmp>> cmps;
				size_t n = 0, nz = 0, dif = 0;
				const int w = kN >> lv, wu = kN >> ( lv - 1 );
				for ( int k = 0; k < 3; k++ ) {
					const Got gl = grabArr( GA[k], lv, half[k] );
					cmps.push_back( { an[k], compare( grabArr( VA[k], lv, half[k] ), gl, 0 ) } );
					if ( r )
						continue;
					const Got up = grabArr( GA[k], lv - 1, half[k] );
					for ( int l = 0; l < kT; l++ )
						for ( int y = 0; y < w; y++ )
							for ( int x = 0; x < w; x++ )
								for ( int e = 0; e < 4; e++ ) {
									const float o = gl.v[( ( size_t( l ) * w + y ) * w + x ) * 4 + e];
									n++;
									nz += o != 0.0f;
									dif += o != up.v[( ( size_t( l ) * wu + 2 * y ) * wu + 2 * x ) * 4 + e];
								}
				}
				pass = verdict( name, cmps, 0, -1.0 );
				if ( !r )
					floorLine( cs + " mip " + std::to_string( lv ) + " (B, C, D)", n, nz, dif, 0.5, true );
			}
			glDeleteTextures( 3, S );
			( r ? red : green )( name, ran, pass );
		}

	// 5 end to end, the 31 dispatches from h0 / q (and the red: FFT stage 7 skipped)
	for ( int r = 0; r < 2; r++ ) {
		const LightRun t = e2e( g.h0, g.q, EA, c.phase, dk, r ? 7 : 0 );
		const std::string name = cs + ( r ? " RED end to end, fft stage 7 skipped" : " end to end (" + std::to_string( t.d.size() ) + " dispatches)" );
		const bool ran = run( t, name );
		bool pass = false;
		if ( ran ) {
			std::vector<std::pair<std::string, Cmp>> cmps;
			for ( int lv = 0; lv < kLv; lv++ )
				for ( int k = 0; k < 3; k++ )
					cmps.push_back( { std::string( an[k] ) + " level " + std::to_string( lv ),
						compare( grabArr( EA[k], lv, half[k] ), grabArr( GA[k], lv, half[k] ), 0 ) } );
			pass = verdict( name, cmps, 0, 1e-5 );
		}
		( r ? red : green )( name, ran, pass );
	}
	glDeleteTextures( 2, V );
	glDeleteTextures( 3, VA );
	glDeleteTextures( 3, EA );
}
}  // namespace ov

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace ov;
	gTag = "OCEANVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: oceanview <prep dir>\nOCEANVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "OCEANVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	const int S = 4, W = 320, H = 180;
	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwoceanview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwoceanview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, W, H };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwoceanview", "ocean view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
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
	if ( !loadAll() || !loadMore() )
		return fatal( "GL entry points" );
	std::printf( "OCEANVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	GlSide g;
	const std::string vs = readFile( dir + "/ocean_spectrum/prog.vert.glsl" );
	g.pSpec = program( vs, readFile( dir + "/ocean_spectrum/prog.frag.glsl" ), "ocean_spectrum" );
	g.pFft = program( readFile( dir + "/ocean_fft/prog.vert.glsl" ), readFile( dir + "/ocean_fft/prog.frag.glsl" ), "ocean_fft" );
	g.pFin = program3( readFile( dir + "/ocean_final/prog.vert.glsl" ), readFile( dir + "/ocean_final/prog.geom.glsl" ),
		readFile( dir + "/ocean_final/prog.frag.glsl" ), "ocean_final" );
	g.pMip = program3( readFile( dir + "/ocean_mip/prog.vert.glsl" ), readFile( dir + "/ocean_mip/prog.geom.glsl" ),
		readFile( dir + "/ocean_mip/prog.frag.glsl" ), "ocean_mip" );
	if ( !g.pSpec || !g.pFft || !g.pFin || !g.pMip )
		return fatal( "GL programs" );

	// cellocean.cpp create(): the textures and targets
	g.h0 = tex2d( kW, kN, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	g.q = tex2d( kW, kN, fR32f, fRed, GL_FLOAT, nullptr, false );
	const GLenum bufs[3] = { fCol0, fCol0 + 1, fCol0 + 2 };
	f.genFramebuffers( 2, g.fb );
	f.genFramebuffers( 1, &g.fbArr );
	bool ok = true;
	for ( int i = 0; i < 2; i++ ) {
		g.ping[i][0] = tex2d( kW, kN, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
		g.ping[i][1] = tex2d( kW, kN, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
		f.bindFramebuffer( fFb, g.fb[i] );
		f.framebufferTexture2D( fFb, fCol0, GL_TEXTURE_2D, g.ping[i][0], 0 );
		f.framebufferTexture2D( fFb, fCol0 + 1, GL_TEXTURE_2D, g.ping[i][1], 0 );
		f.drawBuffers( 2, bufs );
		ok = ok && f.checkFramebufferStatus( fFb ) == fComplete;
	}
	for ( auto & F : g.F )
		for ( GLuint & t : F )
			t = tex2d( kW, kN, fRgba32f, GL_RGBA, GL_FLOAT, nullptr, false );
	g.arrB = texArr( fRgba16f );
	g.arrC = texArr( fRgba32f );
	g.arrD = texArr( fRgba16f );
	f.bindFramebuffer( fFb, g.fbArr );
	ex.framebufferTexture( fFb, fCol0, g.arrB, 0 );
	ex.framebufferTexture( fFb, fCol0 + 1, g.arrC, 0 );
	ex.framebufferTexture( fFb, fCol0 + 2, g.arrD, 0 );
	f.drawBuffers( 3, bufs );
	ok = ok && f.checkFramebufferStatus( fFb ) == fComplete;
	f.bindFramebuffer( fFb, 0 );
	if ( !ok )
		return fatal( "a GL target is incomplete" );

	// the app's quad (drawShape: 4 vertices, 6 indices) on attribute 0; cellocean.cpp's state (no depth, blend, cull, scissor)
	GLuint vao = 0, vbo = 0, ebo = 0;
	ex.genVertexArrays( 1, &vao );
	ex.bindVertexArray( vao );
	static const float quadV[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	f.genBuffers( 1, &vbo );
	f.bindBuffer( kArrayBuf, vbo );
	f.bufferData( kArrayBuf, sizeof( quadV ), quadV, fStatic );
	ex.vertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 12, nullptr );
	ex.enableVertexAttribArray( 0 );
	f.genBuffers( 1, &ebo );
	f.bindBuffer( kElemBuf, ebo );
	f.bufferData( kElemBuf, sizeof( idx ), idx, fStatic );
	const std::function<void()> quad = []() { glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr ); };
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_CULL_FACE );
	glDisable( GL_SCISSOR_TEST );
	glDepthMask( GL_FALSE );
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );

	std::string why;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	std::printf( "OCEANVIEW present device: storage formats %d\n", int( vk().storageExt ) );
	const Case cases[4] = { { 5.0, 0.344, 0.137f }, { 5.0, 0.344, 0.618f }, { 12.0, 0.344, 0.137f }, { 12.0, 0.344, 0.618f } };
	for ( const Case & c : cases ) {
		pump();
		runCase( g, c, quad );
	}
	const WwVkPresent::LightStat st = WwVkPresent::takeLightStat( WwVkPresent::LightOcean );
	std::printf( "OCEANVIEW stat pass ocean: %d runs, %d failed (%s), drawing thread %.3f ms a run (max %.3f), %d pipelines built in %.1f ms\n",
		st.runs, st.fails, st.failWhy.c_str(), st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs, st.pipelines, st.buildMs );
	const GLenum err = glGetError();
	std::printf( "OCEANVIEW GL error 0x%x\n", unsigned( err ) );
	// 4 cases x ( 1 + 14 + 1 + 7 + 1 ) greens, 4 x 5 reds
	const bool gate = gGreenBad == 0 && gRedBad == 0 && gFloorBad == 0 && gGreens == 96 && gReds == 20 && err == GL_NO_ERROR;
	std::printf( "greens %d (%d out), reds %d (%d did not fail), floors low %d\n", gGreens, gGreenBad, gReds, gRedBad, gFloorBad );
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "OCEANVIEW GATE %s\n", gate ? "OK" : "FAILED" );
	return gate ? 0 : 1;
}
