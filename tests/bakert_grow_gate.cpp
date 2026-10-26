// bakert_grow_gate -- lane GIBAKE2 (2026-10-23): do the grown BLAS triangles (bakert::growTri) let the float
// hardware report every triangle the double re-test accepts?
//
//   g++ -O2 -std=c++17 -I src tests/bakert_grow_gate.cpp -o bakert_grow_gate     (red: add -DGROW_RED, del = 0)
//
// Random triangles at Concord-sized coordinates (|x| up to 40000, slivers included), rays aimed within a few float
// ulps of an edge from up to 30000 units away. The double test (probebvh::Bvh::tri on the true triangle, the CPU
// bake's answer) is the reference; a float watertight test (Woop, Benthin, Wald 2013, the scheme ray-tracing
// hardware follows) on the grown float triangle stands in for the hardware. Checks: (a) every double-accepted ray is
// float-accepted on the grown triangle, (b) each grown edge sits del out from the true one, in the true plane. Exit 0
// only when both hold; the red (-DGROW_RED) must fail (a). A plain float Moller test is counted beside it (not gated:
// it loses whole triangles to cancellation on long rays, which no hardware does). No GPU, no Qt.
#include <cmath>
#include <cstdio>
#include <random>
#include <utility>

#include "bakertwalk.h"

static bool triF( const float * p, const float o[3], const float d[3] )
{
	const float e1[3] = { p[3] - p[0], p[4] - p[1], p[5] - p[2] }, e2[3] = { p[6] - p[0], p[7] - p[1], p[8] - p[2] };
	const float pv[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
	const float det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
	if ( std::fabs( det ) < 1e-20f )
		return false;
	const float id = 1.0f / det;
	const float tv[3] = { o[0] - p[0], o[1] - p[1], o[2] - p[2] };
	const float u = ( tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2] ) * id;
	const float qv[3] = { tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0] };
	const float v = ( d[0] * qv[0] + d[1] * qv[1] + d[2] * qv[2] ) * id;
	const float t = ( e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2] ) * id;
	return u >= 0.0f && v >= 0.0f && u + v <= 1.0f && t > 0.0f;
}

// watertight ray/triangle in float: vertices relative to the float origin, axes permuted so z is the ray's major
// axis, sheared, then the three edge functions (no tie-break: an edge hit counts, both faces)
static bool triW( const float * p, const float o[3], const float d[3] )
{
	int kz = 0;
	for ( int k = 1; k < 3; k++ )
		if ( std::fabs( d[k] ) > std::fabs( d[kz] ) )
			kz = k;
	int kx = ( kz + 1 ) % 3, ky = ( kx + 1 ) % 3;
	if ( d[kz] < 0.0f )
		std::swap( kx, ky );
	const float Sx = d[kx] / d[kz], Sy = d[ky] / d[kz], Sz = 1.0f / d[kz];
	float A[3], B[3], C[3];
	for ( int k = 0; k < 3; k++ ) {
		A[k] = p[k] - o[k];
		B[k] = p[3 + k] - o[k];
		C[k] = p[6 + k] - o[k];
	}
	const float Ax = A[kx] - Sx * A[kz], Ay = A[ky] - Sy * A[kz];
	const float Bx = B[kx] - Sx * B[kz], By = B[ky] - Sy * B[kz];
	const float Cx = C[kx] - Sx * C[kz], Cy = C[ky] - Sy * C[kz];
	const float U = Cx * By - Cy * Bx, V = Ax * Cy - Ay * Cx, W = Bx * Ay - By * Ax;
	if ( ( U < 0.0f || V < 0.0f || W < 0.0f ) && ( U > 0.0f || V > 0.0f || W > 0.0f ) )
		return false;
	const float det = U + V + W;
	if ( det == 0.0f )
		return false;
	const float T = U * ( Sz * A[kz] ) + V * ( Sz * B[kz] ) + W * ( Sz * C[kz] );
	return det > 0.0f ? T > 0.0f : T < 0.0f;
}

int main()
{
	const double kGraze = 2e-3;   // ~0.1 degrees off the plane
	std::mt19937_64 rng( 20261023 );
	std::uniform_real_distribution<double> U( -1.0, 1.0 );
	const double mag = 40000.0, diag = 60000.0;
#ifdef GROW_RED
	const double del = 0.0;
#else
	const double del = 4e-6 * ( mag + diag + 1.0 );   // as src/bakert.cpp sets it
#endif
	long long rays = 0, accD = 0, missF = 0, missG = 0, missM = 0, edgeBad = 0, tris = 0, capped = 0;
	int cap = 0;
	for ( int it = 0; it < 20000; it++ ) {
		float t[9], g[9];
		const double c[3] = { U( rng ) * mag * 0.9, U( rng ) * mag * 0.9, U( rng ) * 4000.0 };
		const double s = it % 4 == 0 ? 0.5 : it % 4 == 1 ? 5.0 : 50.0;   // small to big triangles
		for ( int i = 0; i < 9; i++ )
			t[i] = float( c[i % 3] + U( rng ) * s * ( it % 7 == 0 && i >= 6 ? 0.01 : 1.0 ) );   // every 7th a sliver
		int before = cap;
		if ( !bakert::growTri( t, del, g, &cap ) )
			continue;
		tris++;
		const bool wasCapped = cap != before;
		capped += wasCapped ? 1 : 0;
		// (b) each grown edge del out from the true edge line, all grown vertices in the true plane
		{
			double p[3][3], q[3][3];
			for ( int i = 0; i < 9; i++ ) {
				p[i / 3][i % 3] = t[i];
				q[i / 3][i % 3] = g[i];
			}
			double e1[3], e2[3], n[3];
			for ( int k = 0; k < 3; k++ ) {
				e1[k] = p[1][k] - p[0][k];
				e2[k] = p[2][k] - p[0][k];
			}
			n[0] = e1[1] * e2[2] - e1[2] * e2[1];
			n[1] = e1[2] * e2[0] - e1[0] * e2[2];
			n[2] = e1[0] * e2[1] - e1[1] * e2[0];
			const double nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
			const double tol = 3.0 * 0.004;   // float ulps at 40000 (the grown vertex is rounded to float)
			for ( int j = 0; j < 3 && !wasCapped; j++ ) {
				const double * a = p[j], * b = p[( j + 1 ) % 3];
				double e[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, m[3];
				m[0] = ( e[1] * n[2] - e[2] * n[1] ) / nl;
				m[1] = ( e[2] * n[0] - e[0] * n[2] ) / nl;
				m[2] = ( e[0] * n[1] - e[1] * n[0] ) / nl;
				const double ml = std::sqrt( m[0] * m[0] + m[1] * m[1] + m[2] * m[2] );
				for ( int w : { j, ( j + 1 ) % 3 } ) {
					const double dist = ( ( q[w][0] - a[0] ) * m[0] + ( q[w][1] - a[1] ) * m[1] + ( q[w][2] - a[2] ) * m[2] ) / ml;
					const double off = ( ( q[w][0] - p[0][0] ) * n[0] + ( q[w][1] - p[0][1] ) * n[1] + ( q[w][2] - p[0][2] ) * n[2] ) / nl;
					if ( std::fabs( dist - del ) > tol || std::fabs( off ) > tol )
						edgeBad++;
				}
			}
		}
		// (a) rays aimed within a few ulps of each edge, from up to 30000 units away
		for ( int r = 0; r < 64; r++ ) {
			const int j = r % 3;
			const double f = 0.05 + 0.9 * ( U( rng ) * 0.5 + 0.5 );
			double target[3];
			for ( int k = 0; k < 3; k++ )
				target[k] = t[j * 3 + k] + f * ( double( t[( ( j + 1 ) % 3 ) * 3 + k] ) - t[j * 3 + k] ) + U( rng ) * 0.01;
			const double dist = 10.0 + ( U( rng ) * 0.5 + 0.5 ) * 30000.0;
			double d[3] = { U( rng ), U( rng ), U( rng ) };
			const double dl = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
			for ( double & x : d )
				x /= dl;
			const double o[3] = { target[0] - d[0] * dist, target[1] - d[1] * dist, target[2] - d[2] * dist };
			rays++;
			double tt, u, v;
			if ( !probebvh::Bvh::tri( t, o, d, tt, u, v ) || !( tt > 1e-4 ) )
				continue;
			accD++;
			const float of[3] = { float( o[0] ), float( o[1] ), float( o[2] ) }, df[3] = { float( d[0] ), float( d[1] ), float( d[2] ) };
			missM += triF( g, of, df ) ? 0 : 1;
			if ( !triW( g, of, df ) ) {
				// a grazing ray: the float ray crosses the plane ulp(o) / cos away from the double one, past any growth
				const double e1[3] = { t[3] - double( t[0] ), t[4] - double( t[1] ), t[5] - double( t[2] ) };
				const double e2[3] = { t[6] - double( t[0] ), t[7] - double( t[1] ), t[8] - double( t[2] ) };
				const double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
				const double c = std::fabs( n[0] * d[0] + n[1] * d[1] + n[2] * d[2] ) / std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
				if ( c < kGraze ) {
					missG++;
					continue;
				}
				missF++;
				if ( missF <= 5 )
					std::printf( "  miss: |o| %.0f, t %.1f, u %.3g v %.3g w %.3g, triangle size %.2f, |cos| %.2g\n",
						std::fmax( std::fabs( o[0] ), std::fmax( std::fabs( o[1] ), std::fabs( o[2] ) ) ), tt, u, v, 1.0 - u - v, s, c );
			}
		}
	}
	std::printf( "grow gate: del %.4f u, triangles %lld (capped corners on %lld), rays %lld, double-accepted %lld, "
		"float watertight on the grown triangle missed %lld (grazing, |cos| < %g: %lld; float Moller %lld, not gated), grown edges off by more than float rounding %lld\n",
		del, tris, capped, rays, accD, missF, kGraze, missG, missM, edgeBad );
	const bool ok = missF == 0 && edgeBad == 0 && accD > 100000;
	std::printf( "%s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
