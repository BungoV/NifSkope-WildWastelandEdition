// lane SKINSSS2: the half-resolution skin blur (src/gl/wwskinssshalf.h, the twin of cell_skinsss.frag's sssHalf path)
// against the full-resolution twin of SKINSSS1 (src/gl/wwskinsss.h) over a synthetic face / wall / depth-step scene.
// Bar written before the first run (notes/skinsss2/RESULTS.md): on skin, mean |half - full| <= 0.10 x mean
// |full - lit|; it missed (0.558) and is printed as a BAR line. Checked: non-skin pixels keep the frame exactly, no
// light crosses a depth step above the threshold, a flat field equals the full path. Pins of the shader and C++ text. usage: skinsshalf <tree root>; rc 0 = all pass.
// Built with -DWWSKINSSSHALF_RED (the full-resolution path) it must fail (tests/skinsshalf/skinsshalf.sh).
#include "gl/wwskinssshalf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace WwSkinSss;

static int fails = 0;
static void check( bool ok, const char * what, double v = 0.0 )
{
	std::printf( "%s %s %.6g\n", ok ? "ok  " : "FAIL", what, v );
	if ( !ok )
		fails++;
}

static std::string readAll( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

struct Scene
{
	int w = 0, h = 0;
	std::vector<V3> lit;
	std::vector<float> depth, skin;
};

static void blurImage( std::vector<Px> & img, const std::vector<float> & depth, int W, int H, const V3 * k, int R )
{
	std::vector<Px> bx( img.size() );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ )
			bx[size_t( y ) * size_t( W ) + size_t( x )] = blur( &img[size_t( y ) * size_t( W )], &depth[size_t( y ) * size_t( W )], W, x, k, R );
	std::vector<Px> col( static_cast<size_t>( H ) );
	std::vector<float> dcol( static_cast<size_t>( H ) );
	for ( int x = 0; x < W; x++ ) {
		for ( int y = 0; y < H; y++ ) {
			col[size_t( y )] = bx[size_t( y ) * size_t( W ) + size_t( x )];
			dcol[size_t( y )] = depth[size_t( y ) * size_t( W ) + size_t( x )];
		}
		for ( int y = 0; y < H; y++ )
			img[size_t( y ) * size_t( W ) + size_t( x )] = blur( col.data(), dcol.data(), H, y, k, R );
	}
}

// SKINSSS1's passes at full resolution (radius 6)
static std::vector<V3> fullRun( const Scene & s )
{
	const int R = kernelRadius( kWorkScale );
	V3 k[kMaxTaps];
	kernel( k, R );
	std::vector<Px> e( s.lit.size() );
	for ( size_t i = 0; i < e.size(); i++ )
		e[i] = extract( s.lit[i], s.skin[i] );
	blurImage( e, s.depth, s.w, s.h, k, R );
	std::vector<V3> o = s.lit;
	for ( size_t i = 0; i < e.size(); i++ )
		if ( e[i].a > 0.0f )
			o[i] = composite( s.lit[i], e[i] );
	return o;
}

// the half path as cellskinsss.cpp runs it: extract 2x2 (clamped, the gather's edge), blur x / y at half (radius 3, the
// half depth = each block's first texel), the depth-aware bilinear composite at full resolution
static std::vector<V3> halfRun( const Scene & s )
{
	const int R = kernelRadius( kHalfWorkScale );
	V3 k[kMaxTaps];
	kernel( k, R );
	const int W = s.w, H = s.h, hw = halfSize( W ), hh = halfSize( H );
	std::vector<Px> e( static_cast<size_t>( hw ) * static_cast<size_t>( hh ) );
	std::vector<float> hd( e.size() );
	for ( int qy = 0; qy < hh; qy++ )
		for ( int qx = 0; qx < hw; qx++ ) {
			V3 l[4];
			float t[4], d[4];
			for ( int n = 0; n < 4; n++ ) {
				const int x = std::min( 2 * qx + ( n & 1 ), W - 1 ), y = std::min( 2 * qy + ( n >> 1 ), H - 1 );
				const size_t i = size_t( y ) * size_t( W ) + size_t( x );
				l[n] = s.lit[i];
				t[n] = s.skin[i];
				d[n] = s.depth[i];
			}
			e[size_t( qy ) * size_t( hw ) + size_t( qx )] = extractHalf( l, t, d, 4 );
			hd[size_t( qy ) * size_t( hw ) + size_t( qx )] = d[0];
		}
	blurImage( e, hd, hw, hh, k, R );
	std::vector<V3> o = s.lit;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const size_t i = size_t( y ) * size_t( W ) + size_t( x );
			const Px u = upsample( e.data(), hw, hh, s.depth.data(), W, H, x, y, s.skin[i] );
			if ( u.a > 0.0f )
				o[i] = composite( s.lit[i], u );
		}
	return o;
}

// the twin's run (cellskinsss.cpp: half under WW_CELL_SKINSSS_HALF=1); the red build has only the full path
static std::vector<V3> run( const Scene & s )
{
	return halfWanted() ? halfRun( s ) : fullRun( s );
}

static float dif( const V3 & a, const V3 & b )
{
	return ( std::fabs( a.r - b.r ) + std::fabs( a.g - b.g ) + std::fabs( a.b - b.b ) ) / 3.0f;
}

// a face (skin, left 60 %, smooth shading with a hard shadow terminator), a hand in front of its lower part (skin, a
// depth step 0.01 nearer), a wall (not skin, right); odd width and height so the half grid's clamped edge is used
static Scene makeScene( int W, int H, bool greenBelow = false )
{
	Scene s;
	s.w = W;
	s.h = H;
	const size_t n = size_t( W ) * size_t( H );
	s.lit.resize( n );
	s.depth.resize( n );
	s.skin.resize( n );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const size_t i = size_t( y ) * size_t( W ) + size_t( x );
			const bool face = x < W * 6 / 10;
			const bool hand = face && y >= H * 6 / 10 && x >= W / 6 && x < W / 2;
			const float shade = 0.6f + 0.4f * std::sin( float( x + 2 * y ) * 0.21f );
			const bool shadow = x > W * 3 / 10;
			if ( hand ) {
				s.lit[i] = greenBelow ? V3{ 0.02f, 0.9f, 0.02f } : V3{ 0.70f * shade, 0.48f * shade, 0.40f * shade };
				s.depth[i] = 0.9400f + 0.00002f * float( x );
				s.skin[i] = 1.0f;
			} else if ( face ) {
				const float sh = shadow ? 0.12f : 1.0f;
				s.lit[i] = { 0.80f * shade * sh, 0.55f * shade * sh, 0.45f * shade * sh };
				s.depth[i] = 0.9500f + 0.00003f * float( x ) + 0.00002f * float( y );
				s.skin[i] = 1.0f;
			} else {
				s.lit[i] = { 0.30f, 0.30f, 0.32f + 0.1f * float( y % 3 ) };
				s.depth[i] = 0.9700f + 0.0001f * float( x );
				s.skin[i] = 0.0f;
			}
		}
	return s;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";

	// 1. the half path is the pass's choice, its kernel FO4CS's own work size
	check( halfWanted(), "the twin has the half-resolution path" );
	check( kernelRadius( kHalfWorkScale ) == 3, "half kernel radius 3 (FO4CS blurRadius 3 work pixels)", kernelRadius( kHalfWorkScale ) );
	check( halfSize( 1919 ) == 960 && halfSize( 1080 ) == 540, "half size rounds up", halfSize( 1919 ) );

	// 2. the picture against the full-resolution twin
	const Scene sc = makeScene( 81, 49 );
	const std::vector<V3> full = fullRun( sc ), got = run( sc );
	double eff = 0.0, err = 0.0, maxErr = 0.0, maxEff = 0.0;
	int nSkin = 0;
	bool nonSkinExact = true;
	for ( size_t i = 0; i < sc.lit.size(); i++ ) {
		if ( sc.skin[i] > 0.0f ) {
			eff += dif( full[i], sc.lit[i] );
			err += dif( got[i], full[i] );
			maxErr = std::max( maxErr, double( dif( got[i], full[i] ) ) );
			maxEff = std::max( maxEff, double( dif( full[i], sc.lit[i] ) ) );
			nSkin++;
		} else {
			nonSkinExact = nonSkinExact && got[i].r == sc.lit[i].r && got[i].g == sc.lit[i].g && got[i].b == sc.lit[i].b;
		}
	}
	eff /= std::max( nSkin, 1 );
	err /= std::max( nSkin, 1 );
	std::printf( "info skin %d mean effect %.6g mean |half-full| %.6g max effect %.6g max |half-full| %.6g\n", nSkin, eff, err, maxEff, maxErr );
	check( eff > 0.005, "the full twin moves skin (the effect is measurable)", eff );
	// the picture bar (written before the first run) is reported, not counted: it MISSED (0.558), so the half path ships
	// as an option, off (RESULTS.md); the row's default stays the full-resolution path
	std::printf( "BAR picture %s: mean |half - full| / mean effect %.4f (bar <= 0.10)\n", err <= 0.10 * eff ? "MET" : "MISSED",
		eff > 0 ? err / eff : 99.0 );
	check( halfWanted() && err > 0.0, "the half path is a different picture (not the full path re-run)", err );
	check( err <= 0.60 * eff, "half stays near full (guard: mean error <= 0.60 x effect, today 0.558)", eff > 0 ? err / eff : 99.0 );
	check( nonSkinExact, "non-skin pixels keep the frame exactly" );
	{
		// the hand's light changed: face pixels above the hand's top edge (a 0.01 step) must not move
		const Scene s2 = makeScene( 81, 49, true );
		const std::vector<V3> g2 = run( s2 );
		double moved = 0.0;
		const int yTop = 49 * 6 / 10;
		for ( int y = 0; y < yTop; y++ )
			for ( int x = 0; x < 81 * 6 / 10; x++ ) {
				const size_t i = size_t( y ) * 81u + size_t( x );
				moved = std::max( moved, double( dif( g2[i], got[i] ) ) );
			}
		check( moved == 0.0, "no light crosses the depth step (hand over face)", moved );
	}
	{
		// a flat skin field: the half path equals the full one (blur and upsample are identities)
		Scene f;
		f.w = 17;
		f.h = 15;
		f.lit.assign( 255, V3{ 0.6f, 0.4f, 0.3f } );
		f.depth.assign( 255, 0.5f );
		f.skin.assign( 255, 1.0f );
		const std::vector<V3> a = halfRun( f ), b = fullRun( f );
		double worst = 0.0;
		for ( size_t i = 0; i < 255; i++ )
			worst = std::max( worst, double( dif( a[i], b[i] ) ) );
		check( worst < 1e-5, "flat field: half = full", worst );
	}

	// 3. pins: the shader and C++ text carry the twin's law and the wiring
	{
		const std::string fs = readAll( root + "/res/shaders/cell_skinsss.frag" );
		const std::string cs = readAll( root + "/src/gl/cellskinsss.cpp" );
		const std::string tw = readAll( root + "/src/gl/wwskinssshalf.h" );
		check( fs.find( "vec4 w = tg * vec4( sssDepthWeight( dg.w, dg.x ), sssDepthWeight( dg.w, dg.y ), sssDepthWeight( dg.w, dg.z ), 1.0 );" )
			!= std::string::npos, "pin: shader extract weights the block at its first texel's depth" );
		check( fs.find( "return vec4( sum / ws, ws * 0.25 * clamp( sssParams.x, 0.0, 1.0 ) );" ) != std::string::npos, "pin: shader extract coverage" );
		check( fs.find( "sssHalfDepth = vec4( dc );" ) != std::string::npos, "pin: shader half depth on attachment 1" );
		check( fs.find( "vec3 k = sssKernel[abs( t )] * ( sssDepthWeight( dc, texelFetch( hdepthTex, q, 0 ).r ) * s.a );" ) != std::string::npos,
			"pin: shader half blur reads the half depth" );
		check( fs.find( "if ( all( greaterThanEqual( w, vec4( 0.98 ) ) ) ) {" ) != std::string::npos
			&& tw.find( "constexpr float kUpFast = 0.98f;" ) != std::string::npos, "pin: composite plain bilinear threshold = twin" );
		check( fs.find( "fragColor = vec4( o, a );" ) != std::string::npos, "pin: shader composite under the blend" );
		const std::string hf = readAll( root + "/res/shaders/cell_skinssshalf.frag" ), hp = readAll( root + "/res/shaders/cell_skinssshalf.prog" );
		check( hf.find( "#define WW_SSS_HALF 1\n#include \"cell_skinsss.frag\"" ) != std::string::npos
			&& hp.find( "shaders cell_ao.vert cell_skinssshalf.frag" ) != std::string::npos
			&& fs.find( "#ifdef WW_SSS_HALF\n\tsssHalfMain( p );\n\treturn;\n#endif" ) != std::string::npos,
			"pin: the half passes are their own program (cell_skinsss.prog keeps SKINSSS1's code)" );
		check( cs.find( "r->useProgram( half ? \"cell_skinssshalf.prog\" : \"cell_skinsss.prog\" )" ) != std::string::npos, "pin: program by option" );
		check( cs.find( "wwEnv( \"WW_CELL_SKINSSS_HALF\" ).trimmed() == \"1\"" ) != std::string::npos, "pin: half is a live option, off" );
		check( cs.find( "fn->glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );" ) != std::string::npos, "pin: composite blend" );
		check( cs.find( "target( pass == 0 ? g.hext : pass == 1 ? g.hbx : g.hby, pass == 0 ? g.hdep : 0 );" ) != std::string::npos,
			"pin: half targets + half depth attachment" );
		check( cs.find( "WwSkinSss::kernelRadius( half ? WwSkinSss::kHalfWorkScale : WwSkinSss::kWorkScale )" ) != std::string::npos,
			"pin: half kernel radius" );
	}

	std::printf( "%s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
