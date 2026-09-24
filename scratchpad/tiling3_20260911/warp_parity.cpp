/* TILING3 -- does the C++ warp compute the same numbers as the prototype?
 *
 * The five lines of src/lodgen.cpp's lodgenWarpHash / lodgenLandWarpOffsets /
 * lodgenLandWarp, copied verbatim with Qt's integer typedefs spelled out, and
 * printed at a fixed set of world positions.  a4_warp.py / a5_tune.py print the
 * same list; warp_parity.py diffs them.  This is a UNIT CHECK OF A PURE
 * FUNCTION, not a build of the product: no Qt, no link, nothing in release/.
 */
#include <cstdio>
#include <cmath>
#include <cstdint>
typedef int32_t qint32;
typedef uint32_t quint32;

static float g_landWarpAmp     = 0.0f;
static float g_landWarpLattice = 1024.0f;
static int   g_landWarpOctaves = 1;

static inline quint32 lodgenWarpHash( qint32 i, qint32 j, quint32 k )
{
	quint32 h = quint32( i ) * 374761393u
		+ quint32( j ) * 668265263u
		+ k * 2246822519u;
	h ^= h >> 13;
	h *= 1274126177u;
	h ^= h >> 16;
	return h;
}

static void lodgenLandWarpOffsets( double wx, double wy, double lattice,
                                   double * ox, double * oy )
{
	const double gx = wx / lattice;
	const double gy = wy / lattice;
	const double fi = std::floor( gx );
	const double fj = std::floor( gy );
	const qint32 i = qint32( fi );
	const qint32 j = qint32( fj );
	const double fx = gx - fi;
	const double fy = gy - fj;
	const double sx = fx * fx * ( 3.0 - 2.0 * fx );
	const double sy = fy * fy * ( 3.0 - 2.0 * fy );
	double out[2] = { 0.0, 0.0 };
	for ( quint32 k = 0; k < 2; k++ ) {
		const double a = double( lodgenWarpHash( i,     j,     k ) ) / 4294967296.0;
		const double b = double( lodgenWarpHash( i + 1, j,     k ) ) / 4294967296.0;
		const double c = double( lodgenWarpHash( i,     j + 1, k ) ) / 4294967296.0;
		const double d = double( lodgenWarpHash( i + 1, j + 1, k ) ) / 4294967296.0;
		const double v = ( a * ( 1.0 - sx ) + b * sx ) * ( 1.0 - sy )
			+ ( c * ( 1.0 - sx ) + d * sx ) * sy;
		out[k] = v * 2.0 - 1.0;
	}
	*ox = out[0];
	*oy = out[1];
}

void lodgenLandWarp( float wx, float wy, float * wxOut, float * wyOut )
{
	if ( g_landWarpAmp <= 0.0f || g_landWarpOctaves < 1 ) {
		*wxOut = wx;
		*wyOut = wy;
		return;
	}
	double ox = 0.0, oy = 0.0;
	double a = double( g_landWarpAmp );
	double l = double( g_landWarpLattice );
	for ( int k = 0; k < g_landWarpOctaves; k++ ) {
		double dx = 0.0, dy = 0.0;
		lodgenLandWarpOffsets( double( wx ) + double( k ) * 9137.0,
		                       double( wy ) - double( k ) * 4271.0, l, &dx, &dy );
		ox += a * dx;
		oy += a * dy;
		a *= 0.5;
		l *= 0.5;
	}
	*wxOut = float( double( wx ) + ox );
	*wyOut = float( double( wy ) + oy );
}

int main()
{
	/* world positions spanning the worldspace, including negatives and the far
	 * edge where a float lattice index would have quantised */
	const double P[][2] = {
		{ 0.0, 0.0 }, { 1.0, -1.0 }, { 341.3333, 341.3333 },
		{ -81920.0, 98304.0 }, { -81920.0, 81920.0 }, { -147456.0, -81920.0 },
		{ -16384.0, -81920.0 }, { 114688.0, -81920.0 }, { -16384.0, 65536.0 },
		{ 98304.0, 65536.0 }, { 1999999.0, -1999999.0 }, { 12345.678, -98765.432 },
		{ -0.5, -0.5 }, { -1024.0, -1024.0 }, { 1023.9999, 1023.9999 }
	};
	/* OFF first: the warp must return the coordinate untouched, bit for bit */
	printf( "OFF\n" );
	for ( unsigned n = 0; n < sizeof( P ) / sizeof( P[0] ); n++ ) {
		float ox = -12345.0f, oy = -12345.0f;
		lodgenLandWarp( float( P[n][0] ), float( P[n][1] ), &ox, &oy );
		printf( "%.9g %.9g\n", double( ox ), double( oy ) );
	}
	const struct { float a, l; int o; } S[] = {
		{ 683.0f, 1024.0f, 1 }, { 683.0f, 1024.0f, 2 }, { 1365.0f, 2048.0f, 3 },
		{ 341.0f, 2048.0f, 1 }
	};
	for ( unsigned s = 0; s < sizeof( S ) / sizeof( S[0] ); s++ ) {
		g_landWarpAmp = S[s].a; g_landWarpLattice = S[s].l; g_landWarpOctaves = S[s].o;
		printf( "A=%g L=%g o%d\n", double( S[s].a ), double( S[s].l ), S[s].o );
		for ( unsigned n = 0; n < sizeof( P ) / sizeof( P[0] ); n++ ) {
			float ox = 0.0f, oy = 0.0f;
			lodgenLandWarp( float( P[n][0] ), float( P[n][1] ), &ox, &oy );
			printf( "%.9g %.9g\n", double( ox ), double( oy ) );
		}
	}
	return 0;
}

