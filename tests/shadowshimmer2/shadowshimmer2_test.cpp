/* lane SHADOWSHIMMER2 (FIX1030B round 2): the unit of FARSWAYSHADOW, TERRPENSTABLE and CSMOFFBIAS (no GL, no Qt). It runs
 * gl/sunshadowfit.h (the code gl/sunshadow.cpp and the shader mirror) and reads the shipped sources (mirror rule).
 *
 *   G1 terrain + air stable   a still camera over 30 TAA frames (Halton jitter) and a 30-degree turn on the spot: the
 *                             terrain penumbra's rotation (far tier 256 u and near tier 64 u texels) of every still world
 *                             point, and the air tail's strata start of the ray through it, never change. Mirror: the
 *                             shader's lines carry the twin's lattice (4 per tier texel, 2048 a unit, wrap 4096).
 *   G2 Off contact            Cascade Quality Off (CSM1's fit inputs: map 2048, D 3000, splits 800 / 3000, blend 100,
 *                             light back 15000): the thin caster's contact gap B sin e cos e / texel <= 1 texel per
 *                             cascade at every sun 5..85 degrees, and the acne guard (margin >= slope x texel + half a
 *                             quantum) for the ground and a sun-facing wall. Mirror: sunshadow.cpp / the shader use them.
 *   G3 far sway               fo4_default.vert's sway block and each caster's (lookdev_csmdepth.vert, _at.vert) are the
 *                             same text; a CPU twin built from each file's own constants moves 4096 vertices x 8 times by
 *                             the same offset (0 difference); the C++ side hands both passes the one per-paint sample.
 *
 * Reds (each must FAIL): -DSS2_RED_TERR (the pixel's rotation / jitter), -DSS2_RED_OFFBIAS (CSM1's 6 / 12 / 0.275-1.0
 * units, no normal offset), -DSS2_RED_SWAY (the caster as main ships it: no sway).
 * Build: g++ -std=c++17 -I<tree>/src tests/shadowshimmer2/shadowshimmer2_test.cpp; run: <exe> <tree>. */

#include "gl/sunshadowfit.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace WwCsmFit;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_fail = 0;
static void check( bool ok, const char * what, const std::string & detail )
{
	std::printf( "%s %s  %s\n", ok ? "ok  " : "FAIL", what, detail.c_str() );
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

static std::string fmt( const char * f, double a, double b = 0, double c = 0, double d = 0 )
{
	char buf[256];
	std::snprintf( buf, sizeof( buf ), f, a, b, c, d );
	return buf;
}

static size_t countOf( const std::string & s, const std::string & what )
{
	size_t n = 0;
	for ( size_t p = s.find( what ); p != std::string::npos; p = s.find( what, p + 1 ) )
		n++;
	return n;
}

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

//! the pixel centre world point Q lands in with the TAA jitter (pixels); false off screen
static bool project( const Camera & k, const double Q[3], double jx, double jy, double & fx, double & fy )
{
	double d[3];
	for ( int j = 0; j < 3; j++ )
		d[j] = Q[j] - k.C[j];
	const double z = dot3( d, k.fwd );
	if ( z <= 5.0 )
		return false;
	const double px = kW * 0.5 + dot3( d, k.rc ) / z / kTanX * kW * 0.5 + jx;
	const double py = kH * 0.5 + dot3( d, k.uc ) / z / kTanY * kH * 0.5 + jy;
	if ( px < 0 || py < 0 || px >= kW || py >= kH )
		return false;
	fx = std::floor( px ) + 0.5;
	fy = std::floor( py ) + 0.5;
	return true;
}

static double halton( int i, int b )
{
	double f = 1, r = 0;
	for ( ; i > 0; i /= b ) {
		f /= b;
		r += f * ( i % b );
	}
	return r;
}

// world points on the ground, 60..9000 units ahead (the terrain tiers reach far)
static std::vector<std::array<double, 3>> groundPoints( const Camera & k, int n )
{
	std::vector<std::array<double, 3>> pts;
	unsigned s = 4242u;
	auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return double( s >> 8 ) / double( 1u << 24 ); };
	const double h = std::hypot( k.fwd[0], k.fwd[1] );
	while ( int( pts.size() ) < n ) {
		const double dist = 60.0 + 9000.0 * rnd() * rnd(), side = ( rnd() - 0.5 ) * 1.6 * dist;
		std::array<double, 3> q;
		for ( int j = 0; j < 2; j++ )
			q[j] = k.C[j] + k.fwd[j] * dist / h + k.rc[j] * side;
		q[2] = 0.0;
		pts.push_back( q );
	}
	return pts;
}

//! what the receiver gives world point Q this frame: the far / near tier rotations and the air's strata start
static bool samplesOf( const Camera & k, const double Q[3], double jx, double jy, double out[3] )
{
	double fx, fy;
	if ( !project( k, Q, jx, jy, fx, fy ) )
		return false;
#ifdef SS2_RED_TERR
	out[0] = 6.2831853 * ignPixel( fx, fy );
	out[1] = out[0];
	out[2] = ignPixel( fx, fy );
#else
	out[0] = rotTerrStable( Q[0], Q[1], 256.0 );
	out[1] = rotTerrStable( Q[0], Q[1], 64.0 );
	double d[3];
	for ( int j = 0; j < 3; j++ )
		d[j] = Q[j] - k.C[j];
	out[2] = jitAirStable( d );
#endif
	return true;
}

// ---- G1 ----
static void gateG1( const std::string & tree )
{
	const double C0[3] = { -51234.5, 12345.25, 180.0 };
	const Camera k0 = makeCamera( C0, 35.0, -8.0 );
	const auto pts = groundPoints( k0, 4000 );
	// (a) still, 30 TAA frames; (b) a turn on the spot 35 -> 65 degrees, 1 a frame
	for ( int part = 0; part < 2; part++ ) {
		std::vector<std::array<double, 3>> first( pts.size(), { -1.0, -1.0, -1.0 } );
		int changed[3] = { 0, 0, 0 }, seen = 0;
		for ( int f = 0; f <= 30; f++ ) {
			const Camera k = part ? makeCamera( C0, 35.0 + f, -8.0 ) : k0;
			const double jx = part ? 0.0 : halton( f + 1, 2 ) - 0.5, jy = part ? 0.0 : halton( f + 1, 3 ) - 0.5;
			for ( size_t p = 0; p < pts.size(); p++ ) {
				double s[3];
				if ( !samplesOf( k, pts[p].data(), jx, jy, s ) )
					continue;
				if ( first[p][0] < 0.0 ) {
					first[p] = { s[0], s[1], s[2] };
					seen++;
					continue;
				}
				for ( int c = 0; c < 3; c++ ) {
					if ( s[c] != first[p][c] ) {
						changed[c]++;
						first[p][c] = s[c];
					}
				}
			}
		}
		const std::string d = fmt( "far tier %.0f, near tier %.0f, air %.0f changes over %.0f points", changed[0], changed[1], changed[2],
			seen );
		check( changed[0] == 0 && changed[1] == 0 && changed[2] == 0 && seen > 1000,
			part ? "G1b turn 30 deg: terrain penumbra rotation / air strata start per world point" :
			"G1a still 30 TAA frames: terrain penumbra rotation / air strata start per world point", d );
	}
	// the mirror: the shader runs the twin's lattice
	const std::string g = readFile( tree + "/res/shaders/ww_sunshadow.glsl" );
	const bool mirror = kCells == 4 && kCellWrap == 4096.0 && kAirCells == 2048.0
		&& countOf( g, "mod( floor( wxy * ( 4.0 / max( texel, 1e-6 ) ) ), 4096.0 )" ) == 1
		&& countOf( g, "mod( floor( oct * 2048.0 ), 4096.0 )" ) == 1
		&& countOf( g, "ussIgn( gl_FragCoord.xy )" ) == 0
		&& countOf( g, "ussTerrTier( uv, 0, z, ussTerrPen.y, pen, wp.xy )" ) == 1
		&& countOf( g, "ussTerrTier( un, 1, z, ussTerrBias.w, pen, wp.xy )" ) == 1;
	check( mirror, "G1 mirror: ww_sunshadow.glsl's terrain / air hash is the twin's (no bare pixel hash left)", mirror ? "found" : "not found" );
	const std::string sun = readFile( tree + "/src/gl/sunshadow.cpp" );
	const bool up = sun.find( "( sr & 16 ) ? 1.0f : 0.0f" ) != std::string::npos;
	check( up, "G1 mirror: sunshadow.cpp uploads ussCsmStable.z = the red terr only", up ? "found" : "not found" );
}

// ---- G2 ----
struct Bias
{
	double slope, units, recv, normal;	// recv / normal in texels (recvWorld: world units instead, CSM1)
	double recvWorld;
};

static void gateG2( const std::string & tree )
{
#ifdef SS2_RED_OFFBIAS
	( void ) tree;
	const Bias b = { 6.0, 12.0, 0.0, 0.0, 1.0 };	// CSM1 spec 2.6 / 2.4 (the larger receiver offset, B)
#else
	const Bias b = { kOffSlope, kOffUnits, kOffRecvTexels, kOffNormalTexels, 0.0 };
	const std::string sun = readFile( tree + "/src/gl/sunshadow.cpp" );
	const std::string g = readFile( tree + "/res/shaders/ww_sunshadow.glsl" );
	const bool mirror = countOf( sun, "s.slopeBias = WwCsmFit::kOffSlope;" ) == 1 && countOf( sun, "s.unitBias = WwCsmFit::kOffUnits;" ) == 1
		&& countOf( sun, "WwCsmFit::kOffRecvTexels * texel[0]" ) == 1 && countOf( sun, "WwCsmFit::kOffNormalTexels" ) == 1
		&& countOf( g, "float zRef = q.z - ( ussCsmStable.w != 0.0 ? ussCsmBias[i] : offsetWorld ) * csmInvRange[i];" ) == 1
		&& countOf( g, "vec3 q = csmUvd( i, posView + csmNOff * ussCsmTexel[i] );" ) == 2
		&& countOf( g, "csmNOff = csmNormalOff( posView, dFdx( posView ), dFdy( posView ) );" ) == 2;
	check( mirror, "G2 mirror: Off ships kOff* (caster, receiver, normal offset) through sunshadow.cpp and the shader",
		mirror ? "found" : "not found" );
#endif
	// CSM1's Off fit inputs (sunshadow.cpp kDefaultMap / kDefaultDistance / kSplit / kBlend / kLightBack)
	const int map = 2048;
	const double D = 3000, split0 = 800, split1 = 3000, blend = 100, lightBack = 15000, camNear = 5.0;
	const double C0[3] = { 1234.5, 2345.25, 180.0 };
	const Camera k = makeCamera( C0, 35.0, -15.0 );
	double worst[3] = { 0, 0, 0 }, worstAt[3] = { 0, 0, 0 }, acneShort = 0, acneAt = 0;
	int acne = 0;
	for ( int e = 5; e <= 85; e += 5 ) {
		const double el = e * M_PI / 180.0, az = 130.0 * M_PI / 180.0;
		const double to[3] = { std::cos( el ) * std::sin( az ), std::cos( el ) * std::cos( az ), std::sin( el ) };
		double L[3] = { -to[0], -to[1], -to[2] }, R[3], U[3];
		const double Y[3] = { 0, 1, 0 };
		cross3( L, Y, R );
		norm3( R );
		cross3( R, L, U );
		norm3( U );
		const double far[3] = { split0, split1, D };
		const double se = std::sin( el ), ce = std::cos( el ), te = se / ce;
		for ( int i = 0; i < 3; i++ ) {
			In in;
			for ( int j = 0; j < 3; j++ ) {
				in.C[j] = k.C[j];
				in.fwd[j] = k.fwd[j];
				in.rc[j] = k.rc[j];
				in.uc[j] = k.uc[j];
				in.R[j] = R[j];
				in.U[j] = U[j];
				in.L[j] = L[j];
			}
			in.tanX = kTanX;
			in.tanY = kTanY;
			in.zn = i == 0 ? camNear : far[i - 1] - blend;
			in.zf = far[i] + blend;
			in.first = i == 0;
			in.map = map;
			in.lightBack = lightBack;
			Out o;
			fitStable( in, o );
			const double t = o.texel, wz = o.far - kNear, q = wz / 65535.0;
			const double recv = b.recv * t + b.recvWorld;
			auto caster = [&]( double s, bool clamped ) {
				const double unitW = b.units * wz / 65536.0, off = b.slope * s * t + unitW;
				return clamped ? std::min( off, unitW + b.slope * t * 4.0 ) : off;	// SHADOWPAR1's clamp at a slope of 4
			};
			const double gap = ( caster( te, false ) + recv ) * se * ce / t;	// a thin card facing the sun, unclamped (worst)
			if ( gap > worst[i] ) {
				worst[i] = gap;
				worstAt[i] = e;
			}
			for ( double s : { 1.0 / te, te } ) {
				const double margin = b.normal * t * s + std::min( caster( s, false ), caster( s, true ) ) + recv;
				const double need = s * t + 0.5 * q;
				if ( margin < need ) {
					acne++;
					if ( need - margin > acneShort ) {
						acneShort = need - margin;
						acneAt = e;
					}
				}
			}
		}
	}
	for ( int i = 0; i < 3; i++ )
		check( worst[i] <= 1.0, i == 0 ? "G2 Off cascade 0: contact gap <= 1 texel" : i == 1 ? "G2 Off cascade 1: contact gap <= 1 texel"
			: "G2 Off cascade 2: contact gap <= 1 texel", fmt( "worst %.3f texel at %.0f deg", worst[i], worstAt[i] ) );
	check( acne == 0, "G2 Off acne guard (ground + sun-facing wall, 5..85 deg, 3 cascades)",
		fmt( "%.0f short (worst %.3f units at %.0f deg)", acne, acneShort, acneAt ) );
}

// ---- G3 ----
//! the sway block of a vertex shader: from "vec3 farRel" to the end of its if, whitespace folded; empty when absent
static std::string swayBlock( const std::string & text )
{
	const size_t a = text.find( "vec3 farRel" );
	if ( a == std::string::npos )
		return std::string();
	const size_t b = text.find( "v.xyz += viewMatrix * ( w * ( farSwayAmp * vertexColor.a ) );", a );
	if ( b == std::string::npos )
		return std::string();
	const size_t c = text.find( '}', b );
	std::string s = text.substr( a, c + 1 - a ), o;
	bool sp = false;
	for ( char ch : s ) {
		if ( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ) {
			sp = true;
			continue;
		}
		if ( sp && !o.empty() )
			o += ' ';
		sp = false;
		o += ch;
	}
	return o;
}

//! the twin built from a block's own constants: the offset (world) of a vertex at farXY, clock t, amplitude, weight a
struct SwayLaw
{
	bool on = false;
	double k[6] = { 0, 0, 0, 0, 0, 0 };	// time x k0, time x k1, ph x k2, y scale k3, phase (k4, k5)
	void offset( double x, double y, double t, double amp, double a, double o[3] ) const
	{
		o[0] = o[1] = o[2] = 0.0;
		if ( !on || amp <= 0.0 )
			return;
		const double ph = x * k[4] + y * k[5];
		o[0] = std::sin( t * k[0] + ph ) * amp * a;
		o[1] = std::cos( t * k[1] + ph * k[2] ) * k[3] * amp * a;
	}
};

static SwayLaw lawOf( const std::string & block )
{
	SwayLaw L;
	std::smatch m;
	const std::regex ph( "dot\\( farXY, vec2\\( ([0-9.]+), ([0-9.]+) \\) \\)" );
	const std::regex w( "sin\\( farSwayTime \\* ([0-9.]+) \\+ ph \\), cos\\( farSwayTime \\* ([0-9.]+) \\+ ph \\* ([0-9.]+) \\) \\* ([0-9.]+), 0\\.0" );
	if ( std::regex_search( block, m, ph ) ) {
		L.k[4] = std::stod( m[1] );
		L.k[5] = std::stod( m[2] );
		if ( std::regex_search( block, m, w ) ) {
			L.k[0] = std::stod( m[1] );
			L.k[1] = std::stod( m[2] );
			L.k[2] = std::stod( m[3] );
			L.k[3] = std::stod( m[4] );
			L.on = true;
		}
	}
	return L;
}

static void gateG3( const std::string & tree )
{
	const std::string colour = swayBlock( readFile( tree + "/res/shaders/fo4_default.vert" ) );
	check( !colour.empty(), "G3 fo4_default.vert carries the far sway (the colour pass)", colour.empty() ? "absent" : "found" );
	const SwayLaw lc = lawOf( colour );
	for ( const char * f : { "lookdev_csmdepth.vert", "lookdev_csmdepth_at.vert" } ) {
		const std::string text = readFile( tree + "/res/shaders/" + f );
#ifdef SS2_RED_SWAY
		const std::string caster;	// main's casters: no sway
		( void ) text;
#else
		const std::string caster = swayBlock( text );
		const bool wired = text.find( "#include \"uniforms.glsl\"" ) != std::string::npos
			&& text.find( "layout ( location = 1 ) in vec4 vertexColor;" ) != std::string::npos
			&& text.find( "farXY = v.xy;" ) != std::string::npos && text.find( "vec2 farXY = vertexPosition.xy;" ) != std::string::npos
			&& text.find( "gl_Position = csmClipFromView * v;" ) != std::string::npos;
		check( wired, ( std::string( "G3 " ) + f + ": the colour pass's inputs (viewMatrix, vertex alpha, farXY as fo4_default.vert)" ).c_str(),
			wired ? "found" : "not found" );
#endif
		check( !caster.empty() && caster == colour, ( std::string( "G3 " ) + f + ": the sway block is fo4_default.vert's, text for text" ).c_str(),
			caster.empty() ? "absent" : caster == colour ? "same" : "differs" );
		// the twin: 4096 vertices x 8 clocks, colour vs caster
		const SwayLaw lk = lawOf( caster );
		double worst = 0.0;
		unsigned s = 99u;
		auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return double( s >> 8 ) / double( 1u << 24 ); };
		for ( int n = 0; n < 4096; n++ ) {
			const double x = ( rnd() - 0.5 ) * 200000.0, y = ( rnd() - 0.5 ) * 200000.0, a = rnd();
			for ( int c = 0; c < 8; c++ ) {
				const double t = c * 1.37 + 0.25;
				double oc[3], ok[3];
				lc.offset( x, y, t, 10.0, a, oc );
				lk.offset( x, y, t, 10.0, a, ok );
				worst = std::max( worst, std::hypot( oc[0] - ok[0], oc[1] - ok[1] ) );
			}
		}
		check( lc.on && worst == 0.0, ( std::string( "G3 " ) + f + ": twin colour vs caster offset (amp 10, 4096 vertices x 8 clocks)" ).c_str(),
			fmt( "worst %.4f units", worst ) );
	}
	// the C++ side: both passes take the one per-paint sample; the casters get its three uniforms and the tree flag
	const std::string sun = readFile( tree + "/src/gl/sunshadow.cpp" ), far = readFile( tree + "/src/cellfarlod.cpp" );
	const bool cpp = countOf( sun, "wwCellFarLodSway( scene )" ) == 1 && countOf( far, "wwCellFarLodSway( scene )" ) == 1
		&& countOf( sun, "p->uni1f( \"farSwayAmp\", farSway.amp );" ) == 1 && countOf( sun, "p->uni1f( \"farSwayTime\", farSway.time );" ) == 1
		&& countOf( sun, "p->uni1i( \"lodTreeAnim\", k.sway ? 1 : 0 );" ) == 1
		&& countOf( far, "if ( scene == memoScene && paint == memoPaint )" ) == 1;
	check( cpp, "G3 C++: the colour uniforms and the casters read one per-paint sample (cellfarlod.cpp wwCellFarLodSway)",
		cpp ? "found" : "not found" );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string tree = argc > 1 ? argv[1] : ".";
	gateG1( tree );
	gateG2( tree );
	gateG3( tree );
	std::printf( "SHADOWSHIMMER2 UNIT %s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail );
	return g_fail ? 1 : 0;
}
