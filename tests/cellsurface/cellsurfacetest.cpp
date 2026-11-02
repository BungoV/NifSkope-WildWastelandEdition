// Lane ESPWRITE2: the surface pick's maths (src/cellsurface.h). Red WW_CELLSURF_RED=lin (depth read as linear) must fail.
// Build: g++ -std=gnu++2a -Isrc tests/cellsurface/cellsurfacetest.cpp src/cellsurface.cpp -lQt6Core (Qt include paths)
#include "cellsurface.h"

#include <cmath>
#include <cstdio>

static int checks = 0, failures = 0;

static void check( bool ok, const char * what, double got, double want )
{
	++checks;
	if ( !ok ) {
		++failures;
		std::printf( "FAIL %s: got %.6f want %.6f\n", what, got, want );
	}
}

//! glFrustum's window depth of an eye distance d (> 0) with planes n, f
static double depthOf( double d, double n, double f )
{
	// clip z = -(f+n)/(f-n) z_e - 2fn/(f-n), w = -z_e = d: ndc(n) = -1, ndc(f) = +1
	const double ndc = ( ( f + n ) * d - 2.0 * f * n ) / ( ( f - n ) * d );
	return 0.5 * ndc + 0.5;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// 1. eye distance from depth, over the planes the cell view uses (near 1..10, far 2e5..1e6)
	const double planes[][2] = { { 1.0, 200000.0 }, { 5.0, 500000.0 }, { 10.0, 1000000.0 } };
	const double dists[] = { 30.0, 150.0, 1024.0, 4096.0, 20000.0 };
	double worst = 0.0;
	for ( const auto & p : planes )
		for ( double d : dists ) {
			const float depth = float( depthOf( d, p[0], p[1] ) );
			double z = 0;
			const bool ok = cellSurfaceEyeDistance( depth, p[0], p[1], true, z );
			const double rel = ok ? std::fabs( z - d ) / d : 1.0;
			worst = std::max( worst, rel );
			check( ok && rel < 0.02, "eye distance from depth (2 % of the distance, float depth)", z, d );
		}
	std::printf( "eye distance: worst relative error %.6f over %d samples\n", worst, 15 );
	{
		double z = 0;
		check( !cellSurfaceEyeDistance( 1.0f, 1.0, 1000.0, true, z ), "the far plane reads as sky", z, 0 );
		check( !cellSurfaceEyeDistance( 0.0f, 1.0, 1000.0, true, z ), "depth 0 is refused", z, 0 );
		check( cellSurfaceEyeDistance( 0.25f, 10.0, 110.0, false, z ) && std::fabs( z - 35.0 ) < 1e-4, "ortho is linear", z, 35.0 );
	}
	// 2. ray parameter: 45 degrees off the forward axis
	{
		const float dir[3] = { float( std::sqrt( 0.5 ) ), 0.0f, -float( std::sqrt( 0.5 ) ) };
		const float fw[3] = { 0.0f, 0.0f, -1.0f };
		double t = 0;
		check( cellSurfaceRayParam( dir, fw, 100.0, t ) && std::fabs( t - 100.0 * std::sqrt( 2.0 ) ) < 1e-3, "ray parameter", t,
			100.0 * std::sqrt( 2.0 ) );
		const float back[3] = { 0.0f, 0.0f, 1.0f };
		check( !cellSurfaceRayParam( back, fw, 100.0, t ), "a ray away from the view is refused", t, 0 );
	}
	// 3. ray-triangle: the floor z = -965 under (99, 4343.2) from 200 units up
	{
		const float a[3] = { 0, 4200, -965 }, b[3] = { 400, 4200, -965 }, c[3] = { 0, 4600, -965 };
		const float o[3] = { 99.0f, 4343.2f, -765.0f }, d[3] = { 0, 0, -1 };
		float t = 0;
		check( cellSurfaceRayTri( o, d, a, b, c, t ) && std::fabs( t - 200.0f ) < 1e-3f, "ray hits the floor at 200", t, 200 );
		const float o2[3] = { 399.0f, 4599.0f, -765.0f };
		check( !cellSurfaceRayTri( o2, d, a, b, c, t ), "a ray outside the triangle misses", t, 0 );
		float n[3];
		check( cellSurfaceTriNormal( a, b, c, d, n ) && n[2] > 0.999f, "the normal faces the ray (up)", n[2], 1 );
		const float up[3] = { 0, 0, 1 };
		check( cellSurfaceTriNormal( a, b, c, up, n ) && n[2] < -0.999f, "seen from below it faces down", n[2], -1 );
	}
	check( cellSurfaceTolerance( 10.0f ) == 2.0f && std::fabs( cellSurfaceTolerance( 1000.0f ) - 10.0f ) < 1e-4f, "tolerance",
		cellSurfaceTolerance( 1000.0f ), 10 );
	std::printf( "cellsurface%s: %d checks, %d failures\n", cellSurfaceRed().isEmpty() ? "" : " (RED)", checks, failures );
	return failures ? 1 : 0;
}
