/* lane SHADOWSHIMMER3 (FIX1030C): the unit of FARSWAYLEGACY and AIRTAILSLIDE (no GL, no Qt). It runs gl/sunshadowfit.h
 * (the twin of the shader's air hash) and reads the shipped sources (mirror rule).
 *
 *   G1 one tree flag          the far trees' sway flag (lodTreeAnim) of the three passes that draw a shape -- the PBR
 *                             colour pass (Shape::setUniforms), the legacy colour pass (Renderer::setupProgramCE1, which
 *                             writes it again after setUniforms) and the sun caster (gl/sunshadow.cpp c.sway) -- read from
 *                             each source's own expression, over every combination of the shader property's flag, the
 *                             NIF Tree_Anim bit and a "FarLOD " name (TREECROWN1's rule): all three agree on every one.
 *   G2 air tail on a slide    the air tail's strata start of every still world point (ground and raised points 2000..40000
 *                             units out): a still camera over 30 TAA frames, a 30-degree turn, a 240-unit side slide, a
 *                             240-unit forward slide and a 120-unit crane: no change that is not a power-of-two level
 *                             crossing of the point's distance, and all changes <= 1 % of the point-frames. Float: the
 *                             shader's float reconstruction (the eye's world position + the view vector) flips <= 0.5 %.
 *                             Mirror: the shader's lines carry the twin's lattice (2048, 37 / 61, wrap 4096), the red
 *                             airslide upload is -1.
 *
 * Reds (each must FAIL): -DSS3_RED_TREEANIM (main's legacy line and caster test: the shader property's flag),
 * -DSS3_RED_AIRSLIDE (main's hash: SHADOWSHIMMER2's world direction, which a slide moves).
 * Build: g++ -std=c++17 -I<tree>/src tests/shadowshimmer3/shadowshimmer3_test.cpp; run: <exe> <tree>. */

#include "gl/sunshadowfit.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace WwCsmFit;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_fail = 0;
static void check( bool ok, const std::string & what, const std::string & detail )
{
	std::printf( "%s %s  %s\n", ok ? "ok  " : "FAIL", what.c_str(), detail.c_str() );
	if ( !ok )
		g_fail++;
}

static std::string readFile( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

static std::string fmt( const char * f, double a, double b = 0, double c = 0, double d = 0, double e = 0 )
{
	char buf[320];
	std::snprintf( buf, sizeof( buf ), f, a, b, c, d, e );
	return buf;
}

static size_t countOf( const std::string & s, const std::string & what )
{
	size_t n = 0;
	for ( size_t p = s.find( what ); p != std::string::npos; p = s.find( what, p + 1 ) )
		n++;
	return n;
}

//! the line of `text` holding `anchor` (empty when absent or not exactly once)
[[maybe_unused]] static std::string lineOf( const std::string & text, const std::string & anchor )
{
	if ( countOf( text, anchor ) != 1 )
		return std::string();
	const size_t p = text.find( anchor );
	const size_t a = text.rfind( '\n', p ), b = text.find( '\n', p );
	return text.substr( a == std::string::npos ? 0 : a + 1, ( b == std::string::npos ? text.size() : b ) - ( a == std::string::npos ? 0 : a + 1 ) );
}

// ---- G1 ----
struct Facts
{
	bool prop, nifBit, farName;
};

//! TREECROWN1's rule (glshape.cpp Shape::updateShader): the shape's flag
static bool shapeFlag( const Facts & f )
{
	return f.prop || ( f.nifBit && f.farName );
}

/*! what a pass's expression reads: the default arm of `( red ) ? A : B` (or the whole text without one). 1 = the SHAPE's
 *  flag, 0 = the shader property's, -1 = neither (unknown) */
static int sourceOf( const std::string & expr )
{
	std::string arm = expr;
	const size_t q = expr.find( "& 128 ) ?" );
	if ( q != std::string::npos ) {
		const size_t c = expr.find( " : ", q );
		if ( c == std::string::npos )
			return -1;
		arm = expr.substr( c + 3 );
	}
	const bool shape = arm.find( "mesh->isVertexAlphaAnimation" ) != std::string::npos || arm.find( "sh->wwTreeAnim()" ) != std::string::npos;
	const bool prop = arm.find( "bsprop->isVertexAlphaAnimation" ) != std::string::npos
		|| arm.find( "wwShaderLighting()->isVertexAlphaAnimation" ) != std::string::npos;
	if ( shape && !prop )
		return 1;
	if ( prop && !shape )
		return 0;
	// both named: the first one in the default arm is what it reads
	const size_t ps = std::min( arm.find( "mesh->isVertexAlphaAnimation" ), arm.find( "sh->wwTreeAnim()" ) );
	const size_t pp = std::min( arm.find( "bsprop->isVertexAlphaAnimation" ), arm.find( "wwShaderLighting()->isVertexAlphaAnimation" ) );
	return ps == std::string::npos && pp == std::string::npos ? -1 : ps < pp ? 1 : 0;
}

static void gateG1( const std::string & tree )
{
	const std::string ren = readFile( tree + "/src/gl/renderer.cpp" ), sun = readFile( tree + "/src/gl/sunshadow.cpp" );
	const std::string gs = readFile( tree + "/src/gl/glshape.cpp" ), gh = readFile( tree + "/src/gl/glshape.h" );
	// the PBR colour pass: Shape::setUniforms uploads the shape's own flag (TREECROWN1's rule in updateShader)
	const bool pbr = countOf( gs, "prog->uni1i_l( l, isVertexAlphaAnimation ? 1 : 0 );" ) == 1
		&& countOf( gs, "bssp->hasSF2( ShaderFlags::SLSF2_Tree_Anim )" ) == 1
		&& countOf( gs, "name.startsWith( QLatin1String( \"FarLOD \" ) ) )" ) == 1
		&& countOf( gs, "isVertexAlphaAnimation = true;" ) == 1;
	check( pbr, "G1 PBR colour pass: Shape::setUniforms uploads the shape's flag (TREECROWN1's FarLOD Tree_Anim rule)",
		pbr ? "found" : "not found" );
#ifdef SS3_RED_TREEANIM
	// main ad042dbb's two lines
	const std::string legacy = "	prog->uni1i( \"lodTreeAnim\", bsprop->isVertexAlphaAnimation ? 1 : 0 );";
	const std::string caster = "		if ( farSway.amp > 0.0f && c.skinned.empty() && sh->wwShaderLighting() && sh->wwShaderLighting()->isVertexAlphaAnimation ) {";
#else
	const std::string legacy = lineOf( ren, "prog->uni1i( \"lodTreeAnim\"" );
	const std::string caster = lineOf( sun, "& 128 ) ? sh->wwShaderLighting()->isVertexAlphaAnimation" );
	const bool acc = countOf( gh, "bool wwTreeAnim() const { return isVertexAlphaAnimation; }" ) == 1;
	check( acc, "G1 glshape.h: wwTreeAnim() returns the shape's flag", acc ? "found" : "not found" );
	const bool redParse = countOf( sun, "q == QLatin1String( \"treeanim\" ) ? 128" ) == 1;
	check( redParse, "G1 sunshadow.cpp: WW_SUN_SHADOW_STABLE_RED=treeanim is bit 128", redParse ? "found" : "not found" );
#endif
	const int sl = sourceOf( legacy ), sc = sourceOf( caster );
	check( sl >= 0 && sc >= 0, "G1 the legacy colour line and the caster test are found and read one flag",
		fmt( "legacy %.0f, caster %.0f (1 shape, 0 property)", sl, sc ) );
	int disagree = 0, n = 0;
	std::string first;
	for ( int m = 0; m < 8; m++ ) {
		const Facts f { ( m & 1 ) != 0, ( m & 2 ) != 0, ( m & 4 ) != 0 };
		const bool p = shapeFlag( f );
		const bool l = sl == 1 ? shapeFlag( f ) : f.prop;
		const bool k = sc == 1 ? shapeFlag( f ) : f.prop;
		n++;
		if ( !( p == l && p == k ) ) {
			if ( !disagree )
				first = fmt( " (first: property %.0f, NIF Tree_Anim %.0f, FarLOD name %.0f -> PBR %.0f, legacy %.0f", f.prop, f.nifBit,
					f.farName, p, l ) + fmt( ", caster %.0f)", k );
			disagree++;
		}
	}
	check( disagree == 0 && sl >= 0 && sc >= 0, "G1 PBR colour, legacy colour and sun caster sway the same shapes",
		fmt( "%.0f of %.0f cases disagree", disagree, n ) + first );
}

// ---- G2 ----
static void cross3( const double * a, const double * b, double * o )
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}
static void norm3( double * v )
{
	const double l = std::sqrt( dot3( v, v ) );
	for ( int i = 0; i < 3; i++ )
		v[i] /= l;
}

struct Camera
{
	double C[3], fwd[3], rc[3], uc[3];
};

static Camera makeCamera( const double C[3], double yawDeg, double pitchDeg )
{
	Camera k;
	const double y = yawDeg * M_PI / 180.0, p = pitchDeg * M_PI / 180.0;
	const double f[3] = { std::cos( p ) * std::sin( y ), std::cos( p ) * std::cos( y ), std::sin( p ) };
	const double Z[3] = { 0, 0, 1 };
	double r[3];
	cross3( f, Z, r );
	norm3( r );
	double u[3];
	cross3( r, f, u );
	for ( int j = 0; j < 3; j++ ) {
		k.C[j] = C[j];
		k.fwd[j] = f[j];
		k.rc[j] = r[j];
		k.uc[j] = u[j];
	}
	return k;
}

static const int kW = 1920, kH = 1080;
static const double kTanY = std::tan( 27.5 * M_PI / 180.0 ), kTanX = kTanY * 1920.0 / 1080.0;

static bool onScreen( const Camera & k, const double Q[3] )
{
	double d[3];
	for ( int j = 0; j < 3; j++ )
		d[j] = Q[j] - k.C[j];
	const double z = dot3( d, k.fwd );
	if ( z <= 5.0 )
		return false;
	const double px = dot3( d, k.rc ) / z / kTanX, py = dot3( d, k.uc ) / z / kTanY;
	return std::fabs( px ) < 1.0 && std::fabs( py ) < 1.0;
}

//! the strata start the shader gives the tail ending at Q this frame (fl: float reconstruction, the shader's)
static double jitOf( const Camera & k, const double Q[3], bool fl, double * level )
{
	double d[3];
	for ( int j = 0; j < 3; j++ )
		d[j] = Q[j] - k.C[j];
	const double dist = std::sqrt( dot3( d, d ) );
	if ( level )
		*level = airLevel( dist );
#ifdef SS3_RED_AIRSLIDE
	( void ) fl;
	return jitAirStable( d );
#else
	if ( !fl )
		return jitAirWorld( Q, dist );
	// the shader: w1 = ussWorldFromView * p1 in float (the eye's world translation + the rotated view vector)
	double w[3];
	for ( int j = 0; j < 3; j++ )
		w[j] = double( float( float( k.C[j] ) + float( d[j] ) ) );
	const float cell = std::exp2( float( airLevel( double( float( dist ) ) ) ) );
	const float cx = std::floor( float( w[0] ) / cell ), cy = std::floor( float( w[1] ) / cell ), cz = std::floor( float( w[2] ) / cell );
	return ign( glslMod( double( cx + cz * 37.0f ), kCellWrap ), glslMod( double( cy + cz * 61.0f ), kCellWrap ) );
#endif
}

static void gateG2( const std::string & tree )
{
	const double C0[3] = { -51234.5, 12345.25, 180.0 };
	const Camera k0 = makeCamera( C0, 35.0, -4.0 );
	std::vector<std::array<double, 3>> pts;
	unsigned s = 777u;
	auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return double( s >> 8 ) / double( 1u << 24 ); };
	const double h = std::hypot( k0.fwd[0], k0.fwd[1] );
	while ( pts.size() < 6000 ) {
		const double dist = 2000.0 + 38000.0 * rnd() * rnd(), side = ( rnd() - 0.5 ) * 1.6 * dist;
		std::array<double, 3> q;
		for ( int j = 0; j < 2; j++ )
			q[j] = k0.C[j] + k0.fwd[j] * dist / h + k0.rc[j] * side;
		q[2] = ( pts.size() % 3 == 0 ) ? rnd() * 1500.0 : 0.0;	// a third raised (walls, roofs, hills), the rest ground
		pts.push_back( q );
	}
	const char * names[5] = { "still 30 TAA frames", "turn 30 deg", "side slide 240 u", "forward slide 240 u", "crane 120 u" };
	for ( int part = 0; part < 5; part++ ) {
		std::vector<double> prev( pts.size(), -1.0 ), prevLevel( pts.size(), -1.0 ), prevF( pts.size(), -1.0 );
		long changed = 0, unexplained = 0, seen = 0, pairs = 0;
		[[maybe_unused]] long floatFlips = 0;
		for ( int f = 0; f <= 30; f++ ) {
			double C[3] = { C0[0], C0[1], C0[2] };
			double yaw = 35.0;
			if ( part == 1 )
				yaw += f;
			for ( int j = 0; j < 3; j++ ) {
				if ( part == 2 )
					C[j] += k0.rc[j] * 8.0 * f;
				if ( part == 3 )
					C[j] += k0.fwd[j] * 8.0 * f;
			}
			if ( part == 4 )
				C[2] += 4.0 * f;
			const Camera k = makeCamera( C, yaw, -4.0 );
			for ( size_t p = 0; p < pts.size(); p++ ) {
				if ( !onScreen( k, pts[p].data() ) )
					continue;
				double lv = 0.0;
				const double j = jitOf( k, pts[p].data(), false, &lv );
				const double jf = jitOf( k, pts[p].data(), true, nullptr );
				if ( prev[p] < 0.0 ) {
					seen++;
				} else {
					pairs++;
					if ( j != prev[p] ) {
						changed++;
#ifdef SS3_RED_AIRSLIDE
						unexplained++;	// the direction hash has no level: every change is the slide's
#else
						if ( lv == prevLevel[p] )
							unexplained++;
#endif
					}
#ifndef SS3_RED_AIRSLIDE
					// a float flip: the float hash moved where the exact one did not
					if ( j == prev[p] && jf != prevF[p] )
						floatFlips++;
#endif
				}
				prev[p] = j;
				prevLevel[p] = lv;
				prevF[p] = jf;
			}
		}
		/* the bar: no change but a level crossing, and all changes <= 1 % of the point-frames. (First bar: 2 % of the points;
		 * a 240 u forward slide crosses a power of two at ~ dd / (d ln 2) of them, 4.9 % here, all crossings.) */
		const double share = pairs > 0 ? double( changed ) / double( pairs ) : 1.0;
		check( unexplained == 0 && share <= 0.01 && seen > 1000, std::string( "G2 air tail strata start, " ) + names[part],
			fmt( "%.0f changes (%.0f not a level crossing) over %.0f point-frames (%.0f points), %.3f %%", double( changed ),
				double( unexplained ), double( pairs ), double( seen ), 100.0 * share ) );
#ifndef SS3_RED_AIRSLIDE
		const double fs = pairs > 0 ? double( floatFlips ) / double( pairs ) : 0.0;
		check( fs <= 0.005, std::string( "G2 float reconstruction flips, " ) + names[part],
			fmt( "%.0f of %.0f point-frames (%.3f %%)", double( floatFlips ), double( pairs ), 100.0 * fs ) );
#endif
	}
	// the lattice is as fine on screen as the direction's: cell / distance in [1, 2) / 2048 past 2048 units
	double lo = 1e9, hi = 0.0;
	for ( double dist = 2048.0; dist < 60000.0; dist *= 1.013 ) {
		const double a = std::exp2( airLevel( dist ) ) / dist * kAirCells;
		lo = std::min( lo, a );
		hi = std::max( hi, a );
	}
	check( lo >= 1.0 - 1e-9 && hi < 2.0, "G2 the cell is 1..2 x distance / 2048 (the direction hash's angular size)",
		fmt( "cell x 2048 / distance %.3f .. %.3f", lo, hi ) );
#ifndef SS3_RED_AIRSLIDE
	const std::string g = readFile( tree + "/res/shaders/ww_sunshadow.glsl" ), sun = readFile( tree + "/src/gl/sunshadow.cpp" );
	const bool mirror = kAirCells == 2048.0 && kAirZx == 37.0 && kAirZy == 61.0 && kCellWrap == 4096.0
		&& countOf( g, "float cell = exp2( max( ceil( log2( max( t1, 1.0 ) / 2048.0 ) ), 0.0 ) );" ) == 1
		&& countOf( g, "vec3 c = floor( w1 / cell );" ) == 1
		&& countOf( g, "key = mod( c.xy + c.z * vec2( 37.0, 61.0 ), 4096.0 );" ) == 1
		&& countOf( g, "vec3 w1 = ( ussWorldFromView * vec4( dirView * ( t1 / fogDistScale ), 1.0 ) ).xyz;" ) == 1
		&& countOf( g, "float jit = ussIgn( ussCsmStable.z > 0.5 ? gl_FragCoord.xy : key );" ) == 1
		&& countOf( g, "if ( ussCsmStable.z < -0.5 ) {" ) == 1
		&& countOf( g, "J *= wwAirTailShadow" ) == 0;	// the caller lives in lookdev_fog.glsl
	check( mirror, "G2 mirror: ww_sunshadow.glsl's air hash is the twin's (world end point, 2048, 37 / 61, wrap 4096)",
		mirror ? "found" : "not found" );
	const std::string fog = readFile( tree + "/res/shaders/lookdev_fog.glsl" );
	const bool caller = countOf( fog, "J *= wwAirTailShadow( posView / max( length( posView ), 1e-12 ), far, d, tau );" ) == 1;
	check( caller, "G2 lookdev_fog.glsl: t1 is the shaded point's distance (d), the direction its posView's", caller ? "found" : "not found" );
	const bool up = countOf( sun, "( ( sr & 16 ) ? 1.0f : 0.0f ) - ( ( sr & 80 ) == 64 ? 1.0f : 0.0f )" ) == 1
		&& countOf( sun, "q == QLatin1String( \"airslide\" ) ? 64" ) == 1;
	check( up, "G2 mirror: sunshadow.cpp uploads ussCsmStable.z -1 for the red airslide alone (terr wins)", up ? "found" : "not found" );
#else
	( void ) tree;
#endif
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string tree = argc > 1 ? argv[1] : ".";
	gateG1( tree );
	gateG2( tree );
	std::printf( "SHADOWSHIMMER3 UNIT %s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail );
	return g_fail ? 1 : 0;
}
