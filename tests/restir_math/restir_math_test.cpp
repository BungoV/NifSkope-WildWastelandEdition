/* Lane RESTIRDI1 (2026-10-30): the unit test and the CPU reference gate of src/restirmath.h (ReSTIR DI for the cell
 * lights that cast shadows). tests/restir_math/run.sh builds it green and once per red (-DRESTIR_RED_<X>); each red
 * must fail its own check. The fixture is analytic (a floor, three boxes, seven lights, a 96 x 64 camera with an
 * off-centre projection), so the visibility is exact and the reference is brute force:
 *   R(p) = sum over the shadow lights i of p_hat_i(p) x Vsoft_i(p), Vsoft = the share of 256 stratified points on the
 *   light's sphere (restirmath.h spherePoint) that the segment from p + 0.5 n reaches.
 * Checks:
 *   mirror    src/rtddgi/restir_common.glsl (compiled here as C++) == the header's primitives on random inputs   red HASH
 *   viewpos   the pixel's world point from the stored linear depth == the primary ray's hit                     red VIEWPOS
 *   shape     p_hat is 0 behind a hemisphere light's plane and outside a box light's box                         red NOSHAPE
 *   vis       the shadowed estimate's image mean (64 seeds) is R's, not the unshadowed sum's                      red NOVIS
 *   pdf       unbiased: every pixel's 64-seed mean of p_hat W V within 4 standard errors of R                      red NOPDF
 *   temporal  after 16 static frames the ratio-applied sum U x ratio is within the RMSE bar of R                 red NOTEMPORAL
 *   clamp     a light switched on after 40 frames is picked at >= 35 % of its ideal share (green 53 %, red 0.2 %) within 4 frames       red NOCLAMP
 */
#include "restirmath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace glsl {
#include "restir_shim.h"
// ddgi_common.glsl's two curves (MIRROR rtddgimath.h lightRadial / spotCone), which restir_common.glsl calls
inline float ddgiRadial( double d, double r, double bias, double scale, double expo )
{
	return rtddgi::lightRadial( float( d ), float( r ), float( bias ), float( scale ), float( expo ) );
}
inline float ddgiSpot( double dl, double cosOuter, double cone ) { return rtddgi::spotCone( float( dl ), float( cosOuter ), float( cone ) ); }
#include "../../src/rtddgi/restir_common.glsl"
}

using restir::V3;

namespace {

int g_fail = 0, g_pass = 0;
void report( bool ok, const char * check, const char * fmt, double a, double b = 0, double c = 0 )
{
	std::printf( ok ? "ok %s: " : "FAIL %s: ", check );
	std::printf( fmt, a, b, c );
	std::printf( "\n" );
	( ok ? g_pass : g_fail )++;
}

// ------------------------------------------------------------------ the fixture
struct Box
{
	float lo[3], hi[3];
};
const Box kBoxes[3] = { { { -140, -80, 0 }, { -60, 40, 110 } }, { { 70, -20, 0 }, { 150, 80, 160 } }, { { -30, 120, 0 }, { 30, 130, 140 } } };

//! the nearest hit in (1e-4, tmax] of the floor (z = 0, x and y within 600) and the boxes; normal out
bool traceScene( V3 o, V3 d, float tmax, float * tOut, V3 * nOut )
{
	float best = tmax;
	bool hit = false;
	if ( std::fabs( d.z ) > 1e-9f ) {
		const float t = -o.z / d.z;
		const V3 p = o + d * t;
		if ( t > 1e-4f && t <= best && std::fabs( p.x ) <= 600 && std::fabs( p.y ) <= 600 ) {
			best = t;
			hit = true;
			*nOut = V3( 0, 0, o.z > 0 ? 1.0f : -1.0f );
		}
	}
	for ( const Box & b : kBoxes ) {
		float t0 = -1e30f, t1 = 1e30f;
		int axis = -1;
		float sgn = 0;
		bool miss = false;
		for ( int k = 0; k < 3; k++ ) {
			const float ok = k == 0 ? o.x : k == 1 ? o.y : o.z, dk = k == 0 ? d.x : k == 1 ? d.y : d.z;
			if ( std::fabs( dk ) < 1e-12f ) {
				if ( ok < b.lo[k] || ok > b.hi[k] )
					miss = true;
				continue;
			}
			float a = ( b.lo[k] - ok ) / dk, c = ( b.hi[k] - ok ) / dk;
			float s = -1;
			if ( a > c ) {
				std::swap( a, c );
				s = 1;
			}
			if ( a > t0 ) {
				t0 = a;
				axis = k;
				sgn = s;
			}
			t1 = std::min( t1, c );
		}
		if ( miss || t0 > t1 || axis < 0 )
			continue;
		if ( t0 > 1e-4f && t0 <= best ) {
			best = t0;
			hit = true;
			*nOut = V3( axis == 0 ? sgn : 0, axis == 1 ? sgn : 0, axis == 2 ? sgn : 0 );
		}
	}
	*tOut = best;
	return hit;
}

bool blockedSeg( V3 o, V3 d, float tmax )
{
	float t;
	V3 n;
	return tmax > 1e-3f && traceScene( o, d, tmax, &t, &n );
}

void packOmni( float * l, V3 p, float r, V3 c, float src, bool shadow )
{
	for ( int k = 0; k < 32; k++ )
		l[k] = 0.0f;
	l[0] = p.x;
	l[1] = p.y;
	l[2] = p.z;
	l[3] = r;
	l[4] = c.x;
	l[5] = c.y;
	l[6] = c.z;
	l[12] = 0.0f;   // bias
	l[13] = 1.0f;   // scale
	l[14] = 1.0f;   // exponent
	l[29] = src;
	l[30] = shadow ? 1.0f : 0.0f;
}

struct Fixture
{
	int W = 96, H = 64;
	restir::Cam cam;
	std::vector<float> gbuf;       // view normal, linear depth
	std::vector<V3> hitP;          // the primary ray's world hit (for viewpos)
	std::vector<float> lights;
	std::vector<int> cand;
	int nLights = 0;
	std::vector<double> R, U;      // the reference shadowed and unshadowed sums a pixel
};

void buildFixture( Fixture & F )
{
	const V3 eye( 0, -420, 260 ), at( 0, 20, 0 ), up( 0, 0, 1 );
	const V3 f = rtddgi::normalize( at - eye );
	const V3 r = rtddgi::normalize( rtddgi::cross( f, up ) );
	const V3 u = rtddgi::cross( r, f );
	const float rows[12] = { r.x, u.x, -f.x, eye.x, r.y, u.y, -f.y, eye.y, r.z, u.z, -f.z, eye.z };
	for ( int k = 0; k < 12; k++ )
		F.cam.rows[k] = rows[k];
	const float fy = 1.0f / std::tan( 0.5f * 55.0f * 3.14159265f / 180.0f );
	F.cam.proj[0] = fy * float( F.H ) / float( F.W );
	F.cam.proj[1] = fy;
	F.cam.proj[2] = 0.013f;    // off-centre (a TAA jitter's size), so the sign of the offset is tested
	F.cam.proj[3] = -0.008f;
	const size_t n = size_t( F.W ) * size_t( F.H );
	F.gbuf.assign( n * 4, 0.0f );
	F.hitP.assign( n, V3( 0, 0, 0 ) );
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ ) {
			const float nx = ( x + 0.5f ) / F.W * 2.0f - 1.0f, ny = ( y + 0.5f ) / F.H * 2.0f - 1.0f;
			const V3 dv( ( nx + F.cam.proj[2] ) / F.cam.proj[0], ( ny + F.cam.proj[3] ) / F.cam.proj[1], -1.0f );
			const V3 dw = F.cam.dirToWorld( dv );   // view z = -t: t is the linear depth
			float t;
			V3 nw;
			float * g = &F.gbuf[( size_t( y ) * F.W + x ) * 4];
			if ( !traceScene( eye, dw, 1e5f, &t, &nw ) ) {
				g[3] = 1e6f;
				continue;
			}
			const V3 nv( dot( nw, r ), dot( nw, u ), -dot( nw, f ) );
			g[0] = nv.x;
			g[1] = nv.y;
			g[2] = nv.z;
			g[3] = t;
			F.hitP[size_t( y ) * F.W + x] = eye + dw * t;
		}
	F.nLights = 7;
	F.lights.assign( size_t( F.nLights ) * 32, 0.0f );
	packOmni( &F.lights[0 * 32], V3( -20, -140, 60 ), 600, V3( 1.0f, 0.8f, 0.6f ), 8, true );
	packOmni( &F.lights[1 * 32], V3( 190, 20, 70 ), 550, V3( 0.5f, 0.6f, 1.0f ), 12, true );
	packOmni( &F.lights[2 * 32], V3( 0, 60, 90 ), 300, V3( 1.2f, 1.0f, 0.7f ), 4, true );
	packOmni( &F.lights[3 * 32], V3( -100, 150, 260 ), 600, V3( 1, 1, 1 ), 6, true );   // a spot
	{
		float * l = &F.lights[3 * 32];
		const V3 dd = rtddgi::normalize( V3( 0.2f, -0.3f, -0.93f ) );
		l[7] = 1.0f;
		l[8] = dd.x;
		l[9] = dd.y;
		l[10] = dd.z;
		l[11] = std::cos( 35.0f * 3.14159265f / 180.0f );
		l[15] = 1.0f;
	}
	packOmni( &F.lights[4 * 32], V3( 250, 150, 100 ), 450, V3( 0.8f, 1.0f, 0.8f ), 6, true );   // a hemisphere facing -x
	{
		float * l = &F.lights[4 * 32];
		l[8] = -1.0f;
		l[28] = 1.0f;
	}
	packOmni( &F.lights[5 * 32], V3( -250, 100, 120 ), 500, V3( 1.0f, 0.5f, 0.5f ), 6, true );   // a box light
	{
		float * l = &F.lights[5 * 32];
		const float rowsB[12] = { 1.0f / 200, 0, 0, 250.0f / 200, 0, 1.0f / 250, 0, -100.0f / 250, 0, 0, 1.0f / 120, -60.0f / 120 };
		for ( int k = 0; k < 12; k++ )
			l[16 + k] = rowsB[k];
		l[28] = 2.0f;
	}
	packOmni( &F.lights[6 * 32], V3( 0, -300, 150 ), 500, V3( 2, 2, 2 ), 6, false );   // no shadow flag: never a candidate
	F.cand = { 0, 1, 2, 3, 4, 5 };
	// the brute-force reference
	F.R.assign( n, 0.0 );
	F.U.assign( n, 0.0 );
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ ) {
			const restir::Pixel p = restir::pixelAt( F.gbuf.data(), F.W, F.H, F.cam, x, y );
			if ( !p.ok )
				continue;
			const size_t i = size_t( y ) * F.W + x;
			for ( int c : F.cand ) {
				const float * l = &F.lights[size_t( c ) * 32];
				const float ph = restir::target( l, p.P, p.N );
				if ( ph <= 0.0f )
					continue;
				int vis = 0;
				const int S = 16;
				for ( int a = 0; a < S; a++ )
					for ( int b = 0; b < S; b++ ) {
						const V3 X = restir::spherePoint( V3( l[0], l[1], l[2] ), l[29], p.P, ( a + 0.5f ) / S, ( b + 0.5f ) / S );
						const V3 o = p.P + p.N * 0.5f;
						const V3 dl = X - o;
						const float len = rtddgi::length( dl );
						vis += blockedSeg( o, dl * ( 1.0f / len ), len ) ? 0 : 1;
					}
				F.R[i] += double( ph ) * vis / double( S * S );
				F.U[i] += ph;
			}
		}
}

restir::FrameIn frameIn( const Fixture & F, uint32_t frame, const std::vector<float> * prev )
{
	restir::FrameIn in;
	in.W = F.W;
	in.H = F.H;
	in.gbuf = F.gbuf.data();
	in.prevGbuf = prev ? prev->data() : nullptr;
	in.cam = F.cam;
	in.prev = F.cam;
	in.lights = F.lights.data();
	in.nLights = F.nLights;
	in.cand = F.cand.data();
	in.nCand = int( F.cand.size() );
	in.frame = frame;
	return in;
}

auto visFn = []( V3 o, V3 d, float tmax ) { return blockedSeg( o, d, tmax ); };

// ------------------------------------------------------------------ checks
void checkMirror( const Fixture & F )
{
	int bad = 0, n = 0;
	uint32_t s = 12345u;
	auto next = [&]() { s = s * 1664525u + 1013904223u; return s; };
	auto fr = [&]() { return float( next() >> 8 ) / 16777216.0f; };
	for ( int k = 0; k < 20000; k++, n++ ) {
		const uint32_t x = next(), y = next();
		if ( restir::hash( x ) != glsl::restirHash( x ) || restir::rand01( x, y & 1023u ) != glsl::restirRand( x, y & 1023u )
			|| restir::pixelSeed( x, y, k & 3 ) != glsl::restirSeed( x, y, uint32_t( k & 3 ) ) )
			bad++;
	}
	auto close = []( double a, double b, double rel ) { return std::fabs( a - b ) <= rel * std::max( std::fabs( a ), std::fabs( b ) ) + 1e-7; };
	for ( int k = 0; k < 20000; k++, n++ ) {
		const float * l = &F.lights[size_t( k % F.nLights ) * 32];
		const V3 P( fr() * 800 - 400, fr() * 800 - 400, fr() * 300 );
		const V3 N = rtddgi::normalize( V3( fr() * 2 - 1, fr() * 2 - 1, fr() * 2 - 1 ) );
		glsl::vec4 v[8];
		for ( int q = 0; q < 8; q++ )
			v[q] = glsl::vec4( l[q * 4], l[q * 4 + 1], l[q * 4 + 2], l[q * 4 + 3] );
		const float a = restir::target( l, P, N );
		const float b = glsl::restirTarget( v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], glsl::vec3( P.x, P.y, P.z ), glsl::vec3( N.x, N.y, N.z ) );
		if ( !close( a, b, 1e-4 ) )
			bad++;
		const V3 c( l[0], l[1], l[2] );
		const float u1 = fr(), u2 = fr();
		const V3 sp = restir::spherePoint( c, l[29], P, u1, u2 );
		const glsl::vec3 sq = glsl::restirSpherePoint( glsl::vec3( c.x, c.y, c.z ), l[29], glsl::vec3( P.x, P.y, P.z ), u1, u2 );
		if ( std::fabs( sp.x - sq.x ) + std::fabs( sp.y - sq.y ) + std::fabs( sp.z - sq.z ) > 1e-3 )
			bad++;
		restir::Res r = restir::resMake( k % 5 - 1, fr() * 4, float( k % 9 ), 0 );
		glsl::RestirRes g = glsl::restirMake( r.y, r.wsum, r.M, r.W );
		const float w = fr() * 2, u = fr();
		r = restir::update( r, 7, w, 1.0f, u );
		g = glsl::restirUpdate( g, 7, w, 1.0, u );
		const int red = k & 3 ? 0 : RESTIR_BIT_NOPDF;   // the shader's macro (restir_common.glsl)
		const float ph = fr();
		r = restir::finalize( r, ph, r.M, red );
		g = glsl::restirFinalize( g, ph, g.M, red );
		if ( r.y != g.y || !close( r.wsum, g.wsum, 1e-6 ) || r.M != g.M || !close( r.W, g.W, 1e-5 ) )
			bad++;
		const float M1 = fr() * 400, M2 = 8;
		if ( restir::clampHistory( M1, M2, k & 1 ? restir::kRedNoClamp : 0 ) != glsl::restirClampM( M1, M2, k & 1 ? 8 : 0 ) )
			bad++;
		restir::Acc pa;
		pa.num = fr();
		pa.den = fr();
		pa.n = float( k % 20 );
		const bool valid = ( k % 3 ) != 0;
		const restir::Acc ca = restir::accumulate( pa, w, ph, valid, k & 4 ? restir::kRedNoTemporal : 0 );
		const glsl::vec4 ga = glsl::restirAccumulate( glsl::vec4( pa.num, pa.den, pa.n, 0.0 ), w, ph, valid, k & 4 ? RESTIR_BIT_NOTEMPORAL : 0 );
		if ( !close( ca.num, ga.x, 1e-5 ) || !close( ca.den, ga.y, 1e-5 ) || ca.n != ga.z )
			bad++;
		const float dA = fr() * 1000, dB = dA * ( 0.85f + 0.3f * fr() );
		const V3 nB = rtddgi::normalize( N + V3( fr() - 0.5f, fr() - 0.5f, fr() - 0.5f ) * 0.8f );
		if ( restir::similar( dA, N, dB, nB ) != glsl::restirSimilar( dA, glsl::vec3( N.x, N.y, N.z ), dB, glsl::vec3( nB.x, nB.y, nB.z ) ) )
			bad++;
		if ( !close( restir::shadowed( ph, w, k & 1, k & 2 ? 1 : 0 ), glsl::restirShadowed( ph, w, k & 1, k & 2 ? 1 : 0 ), 1e-6 )
			|| !close( restir::ratio( w, ph ), glsl::restirRatio( w, ph ), 1e-6 ) )
			bad++;
		const V3 vp = restir::viewPos( float( k % 96 ), float( k % 64 ), 96, 64, dA, F.cam.proj );
		const glsl::vec3 vq = glsl::restirViewPos( k % 96, k % 64, 96, 64, dA,
			glsl::vec4( F.cam.proj[0], F.cam.proj[1], F.cam.proj[2], F.cam.proj[3] ) );
		if ( !close( vp.x, vq.x, 1e-5 ) || !close( vp.y, vq.y, 1e-5 ) || !close( vp.z, vq.z, 1e-5 ) )
			bad++;
	}
	report( bad == 0, "mirror", "%.0f of %.0f GLSL/header comparisons differ", bad, n );
}

void checkViewPos( const Fixture & F )
{
	double worst = 0;
	int n = 0;
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ ) {
			const restir::Pixel p = restir::pixelAt( F.gbuf.data(), F.W, F.H, F.cam, x, y );
			if ( !p.ok )
				continue;
			const V3 e = p.P - F.hitP[size_t( y ) * F.W + x];
			worst = std::max( worst, double( rtddgi::length( e ) ) / p.D );
			n++;
		}
	report( n > 1000 && worst < 1e-4, "viewpos", "worst world error %.2e of the depth over %.0f pixels", worst, n );
}

void checkShape( const Fixture & F )
{
	int wrong = 0, behind = 0, inside = 0, outside = 0;
	const float * hemi = &F.lights[4 * 32];
	const float * box = &F.lights[5 * 32];
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ ) {
			const restir::Pixel p = restir::pixelAt( F.gbuf.data(), F.W, F.H, F.cam, x, y );
			if ( !p.ok )
				continue;
			if ( p.P.x > 252.0f ) {
				behind++;
				wrong += restir::target( hemi, p.P, p.N ) > 0.0f ? 1 : 0;
			}
			const bool in = std::fabs( p.P.x + 250 ) <= 199 && std::fabs( p.P.y - 100 ) <= 249 && std::fabs( p.P.z - 60 ) <= 119;
			const bool out = std::fabs( p.P.x + 250 ) > 201 || std::fabs( p.P.y - 100 ) > 251 || std::fabs( p.P.z - 60 ) > 121;
			inside += in && restir::target( box, p.P, p.N ) > 0.0f ? 1 : 0;
			if ( out ) {
				outside++;
				wrong += restir::target( box, p.P, p.N ) > 0.0f ? 1 : 0;
			}
		}
	report( wrong == 0 && behind > 50 && outside > 50 && inside > 50, "shape",
		"%.0f clipped pixels lit (behind the hemisphere %.0f, box lights inside %.0f)", wrong, behind, inside );
}

struct SeedStats
{
	std::vector<double> sum, sum2, sumU;
};

void runSeeds( const Fixture & F, SeedStats & S )
{
	const size_t n = size_t( F.W ) * F.H;
	S.sum.assign( n, 0.0 );
	S.sum2.assign( n, 0.0 );
	S.sumU.assign( n, 0.0 );
	for ( int seed = 0; seed < 64; seed++ ) {
		restir::Buffers B;
		for ( int f = 0; f < 4; f++ ) {
			restir::FrameIn in = frameIn( F, uint32_t( seed * 1000 + f ), f ? &F.gbuf : nullptr );
			restir::frame( in, B, visFn );
		}
		for ( size_t i = 0; i < n; i++ ) {
			S.sum[i] += B.num[i];
			S.sum2[i] += double( B.num[i] ) * B.num[i];
			S.sumU[i] += B.den[i];
		}
	}
}

void checkVis( const Fixture & F, const SeedStats & S )
{
	double est = 0, ref = 0, uns = 0;
	for ( size_t i = 0; i < S.sum.size(); i++ ) {
		est += S.sum[i] / 64.0;
		ref += F.R[i];
		uns += F.U[i];
	}
	const double rel = std::fabs( est - ref ) / ref;
	report( rel < 0.02 && uns > 1.05 * ref, "vis", "the shadowed estimate's mean is %.4f of the reference (unshadowed %.4f of it), relative error %.4f",
		est / ref, uns / ref, rel );
}

void checkPdf( const Fixture & F, const SeedStats & S )
{
	double maxR = 0;
	for ( double r : F.R )
		maxR = std::max( maxR, r );
	int n = 0, out = 0;
	double denEst = 0, uRef = 0;
	for ( size_t i = 0; i < S.sum.size(); i++ ) {
		if ( F.U[i] <= 0.0 )
			continue;
		n++;
		const double m = S.sum[i] / 64.0, v = std::max( S.sum2[i] / 64.0 - m * m, 0.0 );
		const double se = std::sqrt( v / 63.0 );
		if ( std::fabs( m - F.R[i] ) > 4.0 * se + 1e-3 * maxR )
			out++;
		denEst += S.sumU[i] / 64.0;
		uRef += F.U[i];
	}
	report( n > 1000 && out <= n / 50 && std::fabs( denEst / uRef - 1.0 ) < 0.02, "pdf",
		"%.0f of %.0f lit pixels outside 4 standard errors of the reference; the unshadowed estimate's mean %.4f of U", out, n, denEst / uRef );
}

double ratioRmse( const Fixture & F, int frames, double * unfiltered = nullptr )
{
	restir::Buffers B;
	for ( int f = 0; f < frames; f++ ) {
		restir::FrameIn in = frameIn( F, uint32_t( 777000 + f ), f ? &F.gbuf : nullptr );
		restir::frame( in, B, visFn );
	}
	double se = 0, mean = 0, se1 = 0;
	int n = 0;
	for ( size_t i = 0; i < F.R.size(); i++ ) {
		if ( F.U[i] <= 0.0 )
			continue;
		const double g = F.U[i] * B.rat[i];
		const double g1 = B.num[i];   // the pixel's own sample, unfiltered
		se += ( g - F.R[i] ) * ( g - F.R[i] );
		se1 += ( g1 - F.R[i] ) * ( g1 - F.R[i] );
		mean += F.R[i];
		n++;
	}
	mean /= std::max( n, 1 );
	if ( unfiltered )
		*unfiltered = std::sqrt( se1 / std::max( n, 1 ) ) / mean;
	return std::sqrt( se / std::max( n, 1 ) ) / mean;
}

void checkTemporal( const Fixture & F )
{
	double raw1 = 0, raw16 = 0;
	const double r1 = ratioRmse( F, 1, &raw1 );
	const double r16 = ratioRmse( F, 16, &raw16 );
	// the bar (measured 10-30): green 0.064, red NOTEMPORAL 0.103 (16 frames); the pixel's own sample shows the
	// history's share without the ratio's window
	const double bar = 0.08;
	report( r16 <= bar && raw16 < raw1, "temporal",
		"relative RMSE of U x ratio vs the reference: %.4f after 16 frames (bar %.2f), %.4f after 1", r16, bar, r1 );
	std::printf( "   (the pixel's own sample p_hat W V: relative RMSE %.4f after 1 frame, %.4f after 16)\n", raw1, raw16 );
}

void checkClamp( Fixture F )
{
	// light 7: bright, switched on at frame 40 (colour 0 before: p_hat 0, never kept)
	F.nLights = 8;
	F.lights.resize( 8 * 32 );
	packOmni( &F.lights[7 * 32], V3( 20, -60, 120 ), 500, V3( 0, 0, 0 ), 6, true );
	F.cand.push_back( 7 );
	restir::Buffers B;
	for ( int f = 0; f < 44; f++ ) {
		if ( f == 40 )
			for ( int c = 4; c < 7; c++ )
				F.lights[7 * 32 + c] = 8.0f;
		restir::FrameIn in = frameIn( F, uint32_t( 5000 + f ), f ? &F.gbuf : nullptr );
		restir::frame( in, B, visFn );
	}
	double got = 0, ideal = 0;
	int n = 0;
	for ( int y = 0; y < F.H; y++ )
		for ( int x = 0; x < F.W; x++ ) {
			const restir::Pixel p = restir::pixelAt( F.gbuf.data(), F.W, F.H, F.cam, x, y );
			if ( !p.ok )
				continue;
			const float p7 = restir::target( &F.lights[7 * 32], p.P, p.N );
			if ( p7 <= 0.0f )
				continue;
			double all = 0;
			for ( int c : F.cand )
				all += restir::target( &F.lights[size_t( c ) * 32], p.P, p.N );
			ideal += p7 / all;
			got += B.hist[size_t( y ) * F.W + x].y == 7 ? 1.0 : 0.0;
			n++;
		}
	report( n > 500 && got >= 0.35 * ideal, "clamp", "4 frames after the switch the new light is kept in %.0f pixels, ideal %.0f (of %.0f)", got, ideal, n );
}

} // namespace

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	Fixture F;
	buildFixture( F );
	checkMirror( F );
	checkViewPos( F );
	checkShape( F );
	SeedStats S;
	runSeeds( F, S );
	checkVis( F, S );
	checkPdf( F, S );
	checkTemporal( F );
	checkClamp( F );
	if ( g_fail ) {
		std::printf( "RESTIR_MATH FAILED %d of %d\n", g_fail, g_fail + g_pass );
		return 1;
	}
	std::printf( "RESTIR_MATH PASS %d\n", g_pass );
	return 0;
}
