// airzone_math_test -- lane AIRZONE1 (2026-10-29): the hand-placed volumes' law (src/airzonemath.h) and the shader's own
// text (res/shaders/cell_airzone.glsl, compiled here through glsl_shim_az.h). No Qt, no GPU.
//
//   g++ -O2 -std=c++17 -I src tests/airzone/airzone_math_test.cpp -o airzone_math_test -static
//   ./airzone_math_test           -> every check PASS, last line "AIRZONE_MATH PASS 7", exit 0
//   -DAIRZONE_RED_<NAME>          -> swaps a wrong rule into the header; check <name> must FAIL (exit 1)
//
// Checks and their reds (tests/airzone/run.sh builds all seven and asserts each):
//   sd        the signed distance: inside exactly where cell_givol.glsl volInside holds (the GI's     RED_SD     no inside
//             own shapes), and the depth at a box's centre is its smallest half size                           distance
//   edge      the weight: 0 on the surface and outside, 1/2 half an edge in, 1 an edge in; the       RED_EDGE   the edge
//             runtime red hardedge (azRed 1) makes the shader's weight 1 there                                 centred on
//                                                                                                              the surface
//   order     priority: a high zone inside a low one wins where it is whole, the low one outside it  RED_ORDER  low first
//   glsl      the shader's azFogAt / azPartAt / azNoise (this text, packed by az::pack) equal the    RED_PACK   R packed,
//             double reference over random rotated zones and points                                           not R^T
//   noise     value noise: the lattice value is the hash, in [0, 1], no crease at a lattice face    RED_NOISE  linear fade
//   identity  outside every zone (and with none) k = 1, add = 0, tint = 1, density = 1 BIT FOR BIT  RED_IDENTITY  the
//             in the shader, tau k + add len == tau, the opacity untouched; the reference too                 zones' mean
//   alpha     a lit sprite's opacity as optical depth: density 1 keeps a, 2 is 1 - (1 - a)^2, 0 is 0 RED_ALPHA  linear
// Every check is independent of the others' reds where the law allows (a red build may also fail glsl: the shader is
// fixed text and the red moved the reference).

#include "airzonemath.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace glsl {
#include "glsl_shim_az.h"
#include "../../res/shaders/cell_airzone.glsl"
}
#undef uniform

namespace {

int g_fail = 0, g_pass = 0;

void report( const char * name, bool ok, const char * fmt, double a = 0, double b = 0, double c = 0 )
{
	char msg[256];
	std::snprintf( msg, sizeof msg, fmt, a, b, c );
	std::printf( "%s %s: %s\n", ok ? "PASS" : "FAIL", name, msg );
	( ok ? g_pass : g_fail )++;
}

// cell_givol.glsl volInside, transcribed (the GI's mask test): the shapes' meaning both reads must agree on
bool volInside( const double l[3], int type, const double s[3] )
{
	double h[3];
	for ( int c = 0; c < 3; c++ )
		h[c] = std::max( 0.5 * s[c], 1e-3 );
	if ( type == 4 )
		return ( l[0] / h[0] ) * ( l[0] / h[0] ) + ( l[1] / h[1] ) * ( l[1] / h[1] ) + ( l[2] / h[2] ) * ( l[2] / h[2] ) <= 1.0;
	if ( type == 3 ) {
		const double r = std::min( h[0], h[1] ), hz = std::max( h[2] - r, 0.0 );
		const double dz = l[2] - std::clamp( l[2], -hz, hz );
		return l[0] * l[0] + l[1] * l[1] + dz * dz <= r * r;
	}
	if ( std::abs( l[0] ) > h[0] || std::abs( l[1] ) > h[1] || std::abs( l[2] ) > h[2] )
		return false;
	if ( type == 1 )
		return ( l[0] / h[0] ) * ( l[0] / h[0] ) + ( l[1] / h[1] ) * ( l[1] / h[1] ) <= 1.0;
	if ( type == 2 )
		return l[2] + h[2] <= 2.0 * h[2] * ( 0.5 - 0.5 * l[1] / h[1] );
	return true;
}

void upload( const std::vector<az::Volume> & v, const std::vector<int> & picked, double seconds, int red )
{
	std::vector<float> buf( size_t( az::kMax * az::kVec4 * 4 ) );
	glsl::azCount = az::pack( v, picked, seconds, buf.data() );
	glsl::azRed = red;
	for ( int i = 0; i < az::kMax * az::kVec4; i++ )
		glsl::azZ[i] = glsl::vec4( buf[size_t( i ) * 4], buf[size_t( i ) * 4 + 1], buf[size_t( i ) * 4 + 2], buf[size_t( i ) * 4 + 3] );
	glsl::azPartD = 1.0f;
}

az::Volume box( float cx, float cy, float cz, float sx, float sy, float sz, float edge, int prio )
{
	az::Volume v;
	v.shape.type = az::Box;
	v.shape.center[0] = cx;
	v.shape.center[1] = cy;
	v.shape.center[2] = cz;
	v.shape.size[0] = sx;
	v.shape.size[1] = sy;
	v.shape.size[2] = sz;
	v.edge = edge;
	v.priority = prio;
	return v;
}

// ---- sd
void checkSd()
{
	std::mt19937 rng( 7 );
	std::uniform_real_distribution<double> U( -1.0, 1.0 );
	int bad = 0, n = 0;
	for ( int type = 0; type < 5; type++ ) {
		const double s[3] = { 300.0, 200.0, 260.0 };
		double h[3];
		for ( int c = 0; c < 3; c++ )
			h[c] = std::max( 0.5 * s[c], 1e-3 );
		for ( int k = 0; k < 20000; k++ ) {
			const double l[3] = { U( rng ) * 200.0, U( rng ) * 140.0, U( rng ) * 170.0 };
			const double sd = az::refSd( l, type, h );
			if ( std::abs( sd ) < 1e-6 )
				continue;
			n++;
			bad += ( sd <= 0.0 ) != volInside( l, type, s ) ? 1 : 0;
		}
	}
	const double hb[3] = { 150.0, 100.0, 130.0 }, o[3] = { 0, 0, 0 };
	const double centre = az::refSd( o, az::Box, hb );
	report( "sd", bad == 0 && std::abs( centre + 100.0 ) < 1e-9, "%g of %g points disagree with volInside; box centre sd %g (want -100)",
		bad, n, centre );
}

// ---- edge
void checkEdge()
{
	int bad = 0;
	double worst = 0.0;
	const int types[4] = { az::Box, az::Sphere, az::Capsule, az::Cylinder };
	for ( int type : types ) {
		az::Volume v = box( 0, 0, 0, 200, 200, 600, 40, 0 );
		v.shape.type = type;
		if ( type == az::Capsule || type == az::Cylinder )
			v.shape.size[2] = 600;	// the x axis meets the round side
		if ( type == az::Sphere )
			v.shape.size[2] = 200;
		const double xs[5] = { 120.0, 100.0, 80.0, 60.0, 0.0 }, want[5] = { 0.0, 0.0, 0.5, 1.0, 1.0 };
		for ( int k = 0; k < 5; k++ ) {
			const double P[3] = { xs[k], 0.0, 0.0 };
			const double w = az::refWeight( v, P, 0 );
			worst = std::max( worst, std::abs( w - want[k] ) );
			bad += std::abs( w - want[k] ) > 1e-9 ? 1 : 0;
		}
	}
	// the shader's weight and the runtime red hardedge, on a fog box
	std::vector<az::Volume> vv = { box( 0, 0, 0, 200, 200, 200, 40, 0 ) };
	vv[0].fog.on = true;
	const double eye[3] = { 0, 0, 0 };
	upload( vv, az::pick( vv, eye ), 0.0, 0 );
	const float wGl = glsl::azWeight( 0, glsl::vec3( 80.0, 0.0, 0.0 ) );
	upload( vv, az::pick( vv, eye ), 0.0, az::kRedHardEdge );
	const float wRed = glsl::azWeight( 0, glsl::vec3( 80.0, 0.0, 0.0 ) );
	const bool glOk = std::abs( wGl - 0.5f ) < 1e-5f && wRed == 1.0f;
	report( "edge", bad == 0 && glOk, "%g weights off (worst %g); shader half an edge in %g (want 0.5), hardedge red %g (want 1)",
		bad, worst, wGl );
	if ( !glOk )
		std::printf( "  edge: shader hardedge weight %g\n", double( wRed ) );
}

// ---- order
void checkOrder()
{
	std::vector<az::Volume> v = { box( 0, 0, 0, 1000, 1000, 1000, 0, 0 ), box( 0, 0, 0, 200, 200, 200, 0, 5 ) };
	v[0].fog.on = v[1].fog.on = true;
	v[0].fog.scale = 2.0f;
	v[1].fog.scale = 4.0f;
	const double eye[3] = { 0, 0, 0 };
	const std::vector<int> pk = az::pick( v, eye );
	const double in[3] = { 0, 0, 0 }, ring[3] = { 300, 0, 0 }, out[3] = { 900, 0, 0 };
	const double kIn = az::refFog( v, pk, in, 0.0, 0 ).k, kRing = az::refFog( v, pk, ring, 0.0, 0 ).k, kOut = az::refFog( v, pk, out, 0.0, 0 ).k;
	report( "order", kIn == 4.0 && kRing == 2.0 && kOut == 1.0, "inside the high zone k %g (want 4), in the low one %g (want 2), outside %g (want 1)",
		kIn, kRing, kOut );
}

// ---- glsl
void checkGlsl()
{
	std::mt19937 rng( 11 );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	double worstK = 0.0, worstAdd = 0.0, worstTint = 0.0, worstPart = 0.0, worstNoise = 0.0;
	int points = 0, scenes = 0;
	for ( int sc = 0; sc < 40; sc++ ) {
		std::vector<az::Volume> v;
		const int nz = 1 + int( U( rng ) * 10.0 );
		for ( int i = 0; i < nz; i++ ) {
			az::Volume z;
			z.shape.type = int( U( rng ) * 5.0 ) % 5;
			for ( int c = 0; c < 3; c++ ) {
				z.shape.center[c] = float( ( U( rng ) - 0.5 ) * 1000.0 );
				z.shape.size[c] = float( 80.0 + U( rng ) * 700.0 );
				z.shape.rotation[c] = float( ( U( rng ) - 0.5 ) * 360.0 );
			}
			z.edge = U( rng ) < 0.2 ? 0.0f : float( 16.0 + U( rng ) * 100.0 );
			z.priority = int( U( rng ) * 5.0 ) - 2;
			z.fog.on = U( rng ) < 0.7;
			z.fog.scale = float( U( rng ) * 4.0 );
			z.fog.add = float( U( rng ) * 6.0 );
			z.fog.heightFalloff = U( rng ) < 0.5 ? 0.0f : float( 50.0 + U( rng ) * 400.0 );
			for ( int c = 0; c < 3; c++ ) {
				z.fog.colour[c] = float( U( rng ) * 2.0 );
				z.part.tint[c] = float( U( rng ) * 2.0 );
				z.fog.noiseDrift[c] = float( ( U( rng ) - 0.5 ) * 40.0 );
			}
			z.fog.colourStrength = float( U( rng ) );
			z.fog.noise = U( rng ) < 0.5 ? 0.0f : float( U( rng ) );
			z.fog.noiseScale = float( 64.0 + U( rng ) * 512.0 );
			z.part.on = U( rng ) < 0.6;
			z.part.density = float( U( rng ) * 3.0 );
			z.part.light = float( U( rng ) * 3.0 );
			z.part.tintStrength = float( U( rng ) );
			v.push_back( z );
		}
		const double eye[3] = { ( U( rng ) - 0.5 ) * 2000.0, ( U( rng ) - 0.5 ) * 2000.0, ( U( rng ) - 0.5 ) * 2000.0 };
		const double seconds = U( rng ) * 100.0;
		const std::vector<int> pk = az::pick( v, eye );
		upload( v, pk, seconds, 0 );
		scenes++;
		for ( int k = 0; k < 500; k++ ) {
			const float Pf[3] = { float( ( U( rng ) - 0.5 ) * 1800.0 ), float( ( U( rng ) - 0.5 ) * 1800.0 ), float( ( U( rng ) - 0.5 ) * 1800.0 ) };
			const double P[3] = { Pf[0], Pf[1], Pf[2] };
			bool nearSurface = false;	// float vs double flips a hard edge or a noise lattice face: skip points on one
			for ( int idx : pk ) {
				double l[3], h[3];
				az::toLocal( v[size_t( idx )].shape, P, l );
				az::halfSize( v[size_t( idx )].shape, h );
				nearSurface = nearSurface || std::abs( az::refSd( l, v[size_t( idx )].shape.type, h ) ) < 0.05;
			}
			if ( nearSurface )
				continue;
			points++;
			const glsl::vec3 Pg( Pf[0], Pf[1], Pf[2] );
			const glsl::AzFog g = glsl::azFogAt( Pg );
			const az::FogOut r = az::refFog( v, pk, P, seconds, 0 );
			worstK = std::max( worstK, std::abs( double( g.k ) - r.k ) / ( 1.0 + std::abs( r.k ) ) );
			worstAdd = std::max( worstAdd, std::abs( double( g.add ) - r.add ) / ( 1e-4 + std::abs( r.add ) ) );
			const double gt[3] = { g.tint.x, g.tint.y, g.tint.z };
			for ( int c = 0; c < 3; c++ )
				worstTint = std::max( worstTint, std::abs( gt[c] - r.tint[c] ) / ( 1.0 + std::abs( r.tint[c] ) ) );
			const glsl::AzPart gp = glsl::azPartAt( Pg );
			const az::PartOut rp = az::refPart( v, pk, P, 0 );
			worstPart = std::max( worstPart, std::abs( double( gp.density ) - rp.density ) / ( 1.0 + rp.density ) );
			worstPart = std::max( worstPart, std::abs( double( gp.light ) - rp.light ) / ( 1.0 + rp.light ) );
			const float q[3] = { Pf[0] * 0.013f, Pf[1] * 0.013f, Pf[2] * 0.013f };
			worstNoise = std::max( worstNoise, std::abs( double( glsl::azNoise( glsl::vec3( q[0], q[1], q[2] ) ) ) - az::refNoise( q ) ) );
		}
	}
	const bool ok = worstK < 2e-3 && worstAdd < 2e-3 && worstTint < 2e-3 && worstPart < 2e-3 && worstNoise < 1e-5 && points > 10000;
	report( "glsl", ok, "worst k %g, add %g, tint %g (relative)", worstK, worstAdd, worstTint );
	std::printf( "  glsl: %d scenes, %d points; particles worst %g, noise worst %g\n", scenes, points, worstPart, worstNoise );
}

// ---- noise
void checkNoise()
{
	std::mt19937 rng( 3 );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	double worstLattice = 0.0, lo = 1.0, hi = 0.0, worstCrease = 0.0;
	for ( int k = 0; k < 400; k++ ) {
		const int x = int( U( rng ) * 40.0 ) - 20, y = int( U( rng ) * 40.0 ) - 20, z = int( U( rng ) * 40.0 ) - 20;
		const float p[3] = { float( x ), float( y ), float( z ) };
		worstLattice = std::max( worstLattice, std::abs( az::refNoise( p ) - double( az::refHash( x, y, z ) ) ) );
		const float q[3] = { float( U( rng ) * 30.0 ), float( U( rng ) * 30.0 ), float( U( rng ) * 30.0 ) };
		const double n = az::refNoise( q );
		lo = std::min( lo, n );
		hi = std::max( hi, n );
		// the slope along x just below and just above the face x = an integer (the smooth fade's own slope there is under
		// 9 e x the corner spread: < 0.002 at e = 2e-4; a linear fade jumps by the corners' differences, ~0.1..1)
		const float e = 2e-4f;
		const float f0 = float( 1 + int( U( rng ) * 20.0 ) );
		const float a[3] = { f0 - 2.0f * e, q[1], q[2] }, b[3] = { f0 - e, q[1], q[2] }, c[3] = { f0 + e, q[1], q[2] }, d[3] = { f0 + 2.0f * e, q[1], q[2] };
		const double below = ( az::refNoise( b ) - az::refNoise( a ) ) / double( e ), above = ( az::refNoise( d ) - az::refNoise( c ) ) / double( e );
		worstCrease = std::max( worstCrease, std::abs( above - below ) );
	}
	report( "noise", worstLattice < 1e-12 && lo >= 0.0 && hi <= 1.0 && worstCrease < 0.01,
		"lattice off the hash by %g; slope jump at a face %g (bar 0.01); range low %g", worstLattice, worstCrease, lo );
}

// ---- identity
void checkIdentity()
{
	std::vector<az::Volume> v = { box( 0, 0, 0, 200, 200, 200, 30, 0 ), box( 400, 0, 0, 100, 300, 200, 0, 2 ) };
	v[0].fog.on = v[1].fog.on = v[0].part.on = v[1].part.on = true;
	v[0].fog.scale = 3.0f;
	v[0].fog.add = 5.0f;
	v[0].fog.colourStrength = 1.0f;
	v[0].fog.colour[0] = 0.2f;
	v[1].part.density = 4.0f;
	v[1].part.light = 0.25f;
	v[1].shape.rotation[2] = 33.0f;
	const double eye[3] = { 0, 0, 0 };
	int bad = 0;
	std::mt19937 rng( 5 );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	for ( int pass = 0; pass < 2; pass++ ) {
		if ( pass == 0 ) {
			glsl::azCount = 0;	// no zones at all
			glsl::azRed = 0;
		} else {
			upload( v, az::pick( v, eye ), 0.0, 0 );
		}
		for ( int k = 0; k < 2000; k++ ) {
			const glsl::vec3 P( 2000.0 + U( rng ) * 500.0, ( U( rng ) - 0.5 ) * 4000.0, ( U( rng ) - 0.5 ) * 4000.0 );	// outside both
			const glsl::AzFog f = glsl::azFogAt( P );
			const glsl::AzPart p = glsl::azPartAt( P );
			const float tau = float( U( rng ) * 3.0 ), len = float( U( rng ) * 300.0 );
			const float tau2 = tau * f.k + f.add * len;
			glsl::azPartD = 1.0f;
			const glsl::vec3 lit( U( rng ), U( rng ), U( rng ) );
			const glsl::vec3 lit2 = glsl::azPartLit( P, lit );
			const float a = float( U( rng ) );
			bad += ( f.k != 1.0f || f.add != 0.0f || f.tint.x != 1.0f || f.tint.y != 1.0f || f.tint.z != 1.0f ) ? 1 : 0;
			bad += ( p.density != 1.0f || p.light != 1.0f || p.tint.x != 1.0f || p.tint.y != 1.0f || p.tint.z != 1.0f ) ? 1 : 0;
			bad += std::memcmp( &tau2, &tau, sizeof tau ) != 0 ? 1 : 0;
			bad += ( lit2.x != lit.x || lit2.y != lit.y || lit2.z != lit.z ) ? 1 : 0;
			const float a2 = glsl::azPartAlpha( a );
			bad += std::memcmp( &a2, &a, sizeof a ) != 0 ? 1 : 0;
		}
	}
	const double farP[3] = { 3000, 0, 0 };
	const az::FogOut r = az::refFog( v, az::pick( v, eye ), farP, 0.0, 0 );
	const az::PartOut rp = az::refPart( v, az::pick( v, eye ), farP, 0 );
	const bool refOk = r.k == 1.0 && r.add == 0.0 && r.tint[0] == 1.0 && rp.density == 1.0 && rp.light == 1.0;
	report( "identity", bad == 0 && refOk, "%g shader reads not the identity bit for bit (of 4000 points x 5); reference outside k %g, density %g",
		bad, r.k, rp.density );
}

// ---- alpha
void checkAlpha()
{
	int bad = 0;
	for ( int k = 0; k <= 100; k++ ) {
		const double a = k / 100.0;
		bad += az::refPartAlpha( a, 1.0 ) != a ? 1 : 0;
		bad += std::abs( az::refPartAlpha( a, 2.0 ) - ( 1.0 - ( 1.0 - a ) * ( 1.0 - a ) ) ) > 1e-12 ? 1 : 0;
		bad += az::refPartAlpha( a, 0.0 ) != 0.0 ? 1 : 0;
		glsl::azPartD = 2.5f;
		bad += std::abs( double( glsl::azPartAlpha( float( a ) ) ) - az::refPartAlpha( double( float( a ) ), 2.5 ) ) > 1e-5 ? 1 : 0;
	}
	glsl::azPartD = 1.0f;
	report( "alpha", bad == 0, "%g of 404 opacities wrong", bad );
}

} // namespace

int main()
{
	checkSd();
	checkEdge();
	checkOrder();
	checkGlsl();
	checkNoise();
	checkIdentity();
	checkAlpha();
	if ( g_fail ) {
		std::printf( "AIRZONE_MATH FAIL %d of %d\n", g_fail, g_fail + g_pass );
		return 1;
	}
	std::printf( "AIRZONE_MATH PASS %d\n", g_pass );
	return 0;
}
