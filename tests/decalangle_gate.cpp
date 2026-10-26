// decalangle_gate -- lane DECALANGLE (2026-10-26): does the placed decal's angle rule match the game's, per pixel?
//
//   g++ -O2 -std=c++17 -I src tests/decalangle_gate.cpp -o decalangle_gate && ./decalangle_gate [res/shaders/fo4_default.frag]
//   red: add -DANGLE_RED (the draw takes the old per-vertex fade, WW_CELL_DECAL_RED=vertexangle): (a) must FAIL
//   red: add -DCULL_RED (a pre-cull without the length margin): (c) must FAIL
//
// The fixture: a stair of 6 steps (tread 30 deep, riser 20 high, 100 wide) climbing +Y, a decal box over it that
// projects straight down (D = +Z). The risers are SMOOTHED at their top edge (the corner normal is the mean of the
// tread's and the riser's) and hard at their foot, the way a smoothed stair mesh carries them, so a riser's corner
// normals run from dot 0.707 to 0 against D. Each riser triangle is sampled on a barycentric grid:
//   game  = the deferred decal pixel shader's term: face dot >= 0.3 -> 1, else saturate((dot(normalize(N), D) - 0.3)
//           / 0.25), N the interpolated normal (no normal map on the fixture)
//   ours  = what the cell view draws: each corner's vertexValue (src/celldecalangle.h) stored in an 8-bit vertex alpha,
//           interpolated beside the per-vertex unit normal, then fragTerm (the mirror of fo4_default.frag
//           wwDecalVertexA) on the interpolated value and normal length
// Checks: (a) |ours - game| <= 0.02 on every riser sample; (b) the old rule (vertexFade interpolated) covers the
// risers more than the game by >= 10% of the game's coverage (the streak bungo saw, as a number); (c) mayPass never
// rejects a triangle any of whose pixels the game draws (the fixture's risers + 200000 random corner normal sets);
// (d) the shader file holds the mirrored expression and the 4/255 drop. Exit 0 only when all hold. No GPU, no Qt.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "celldecalangle.h"

using namespace celldecalangle;

#ifdef CULL_RED   // red for (c): the naive cull (no corner passes 0.3) loses pixels the renormalised normal lifts over
#define mayPass( d, a, b, c ) ( ( d ) > kMin )
#endif

struct V { float x, y, z; };
static V add( V a, V b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static V mul( V a, float s ) { return { a.x * s, a.y * s, a.z * s }; }
static float dot( V a, V b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float len( V a ) { return std::sqrt( dot( a, a ) ); }
static V unit( V a ) { return mul( a, 1.0f / len( a ) ); }
static V cross( V a, V b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
static float q8( float v ) { return std::round( sat( v ) * 255.0f ) / 255.0f; }   // ByteColor4: 8 bits a channel

struct Tri { V p[3], n[3]; };

int main( int argc, char ** argv )
{
	const V D = { 0.0f, 0.0f, 1.0f };   // minus the projection axis: the box projects down
	std::vector<Tri> risers;
	const V up = { 0, 0, 1 }, front = { 0, -1, 0 }, corner = unit( add( up, front ) );
	for ( int s = 0; s < 6; s++ ) {
		const float y = 30.0f * s, z0 = 20.0f * s, z1 = z0 + 20.0f;
		const V a = { 0, y, z0 }, b = { 100, y, z0 }, c = { 100, y, z1 }, d = { 0, y, z1 };
		risers.push_back( { { a, b, c }, { front, front, corner } } );
		risers.push_back( { { a, c, d }, { front, corner, corner } } );
	}
	double cover[3] = { 0, 0, 0 };   // game, ours, old rule
	float worst = 0.0f;
	int samples = 0, culledLit = 0;
	const int G = 40;
	for ( const Tri & t : risers ) {
		V face = unit( cross( add( t.p[1], mul( t.p[0], -1 ) ), add( t.p[2], mul( t.p[0], -1 ) ) ) );
		if ( dot( face, add( add( t.n[0], t.n[1] ), t.n[2] ) ) < 0.0f )
			face = mul( face, -1.0f );
		const float gd = dot( face, D );
		float vv[3], vf[3], un[3][3], dMax = -1.0f;
		for ( int k = 0; k < 3; k++ ) {
			const V n = unit( t.n[k] );
			un[k][0] = n.x; un[k][1] = n.y; un[k][2] = n.z;
			vv[k] = q8( vertexValue( gd, dot( n, D ) ) );
			vf[k] = q8( vertexFade( gd, dot( n, D ) ) );
			dMax = std::max( dMax, dot( n, D ) );
		}
		const bool kept = gd >= kMin || mayPass( dMax, un[0], un[1], un[2] );
		for ( int i = 0; i <= G; i++ )
			for ( int j = 0; i + j <= G; j++ ) {
				const float w[3] = { 1.0f - float( i + j ) / G, float( i ) / G, float( j ) / G };
				V n = { 0, 0, 0 };
				float val = 0.0f, old = 0.0f;
				for ( int k = 0; k < 3; k++ ) {
					n = add( n, mul( unit( t.n[k] ), w[k] ) );
					val += vv[k] * w[k];
					old += vf[k] * w[k];
				}
				const float game = gameTerm( gd, dot( n, D ), len( n ) );
#ifdef ANGLE_RED
				const float ours = kept ? old : 0.0f;
#else
				const float ours = kept ? fragTerm( val, len( n ) ) : 0.0f;
#endif
				if ( !kept && game > 0.0f )
					culledLit++;
				worst = std::max( worst, std::fabs( ours - game ) );
				cover[0] += game;
				cover[1] += ours;
				cover[2] += old;
				samples++;
			}
	}
	// (c) random corner normal sets: a rejected triangle must have no pixel the game draws
	std::mt19937 rng( 26102026 );
	std::normal_distribution<float> nd( 0.0f, 1.0f );
	int randRejected = 0;
	for ( int r = 0; r < 200000; r++ ) {
		V n[3];
		float un[3][3], dMax = -1.0f;
		for ( int k = 0; k < 3; k++ ) {
			n[k] = unit( { nd( rng ), nd( rng ), nd( rng ) } );
			if ( r & 1 )   // half of them clustered near the 0.3 cone, where a bad bound shows
				n[k] = unit( add( mul( n[k], 0.35f ), { 0.0f, 0.95f, 0.3f } ) );
			un[k][0] = n[k].x; un[k][1] = n[k].y; un[k][2] = n[k].z;
			dMax = std::max( dMax, dot( n[k], D ) );
		}
		if ( mayPass( dMax, un[0], un[1], un[2] ) )
			continue;
		randRejected++;
		for ( int i = 0; i <= 24; i++ )
			for ( int j = 0; i + j <= 24; j++ ) {
				const float w[3] = { 1.0f - float( i + j ) / 24, float( i ) / 24, float( j ) / 24 };
				const V m = add( add( mul( n[0], w[0] ), mul( n[1], w[1] ) ), mul( n[2], w[2] ) );
				if ( len( m ) > 1.0e-4f && gameTerm( 0.0f, dot( m, D ), len( m ) ) > 0.0f )
					culledLit++;
			}
	}
	// (d) the shader holds the mirror
	const char * frag = argc > 1 ? argv[1] : "res/shaders/fo4_default.frag";
	std::ifstream f( frag );
	std::stringstream ss;
	ss << f.rdbuf();
	const std::string src = ss.str();
	const bool shaderOk = src.find( "( vertexA / max( length( btnMatrix[2] ), 1.0e-4 ) - 0.3 ) / 0.25" ) != std::string::npos
		&& src.find( "wwDecalAngle && a < 4.0 / 255.0" ) != std::string::npos;

	const bool a = worst <= 0.02f, b = cover[2] >= cover[0] * 1.10, c = culledLit == 0;
	std::printf( "riser samples %d: game coverage %.1f, drawn %.1f, old rule %.1f (old / game %.3f)\n", samples,
		cover[0], cover[1], cover[2], cover[2] / cover[0] );
	std::printf( "(a) drawn = game per pixel: worst |diff| %.4f  %s\n", worst, a ? "PASS" : "FAIL" );
	std::printf( "(b) the old rule streaks (>= 1.10 x game): %s\n", b ? "PASS" : "FAIL" );
	std::printf( "(c) pre-cull safe: %d lit pixels in rejected triangles (%d random rejected)  %s\n", culledLit,
		randRejected, c ? "PASS" : "FAIL" );
	std::printf( "(d) shader mirror in %s: %s\n", frag, shaderOk ? "PASS" : "FAIL" );
	const bool ok = a && b && c && shaderOk;
	std::printf( "decalangle %s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
