/* TILING4 -- does src/lodgen.cpp's hex tiling compute the same numbers as the
 * prototype the numbers were picked on?
 *
 * The C++ that landed is a transcription of h_cand.py's `tri_grid`, `hash01`
 * and `_blend`.  A transcription is a place to make a mistake -- a skew the
 * wrong way round, a vertex triple in the wrong order, a weight taken from the
 * mirrored triangle, a missing square root -- and none of those would crash:
 * they would quietly bake a DIFFERENT sampler from the one the fourteen sheets
 * were measured on, and every number in this lane's report would then describe
 * a bake the exe never produces.
 *
 * So the same pure function is evaluated on both sides at the same fifteen
 * world positions x five settings and diffed.  lodgenLandHexCell and
 * lodgenLandHexOffset are copied VERBATIM from src/lodgen.cpp, with Qt's
 * typedefs spelled out.  The texture is replaced by a closed-form stand-in --
 * s(u,v) = (255u, 255v, 255uv) -- so the blend's arithmetic is exercised with
 * no DDS, no Qt and no link: pure IEEE double on both sides.
 *
 * WHAT THIS PROBE CANNOT REACH.  In the product the three taps come back as
 * float32 from getPixelT and the accumulator is a FloatVector4, so the shipped
 * blend runs in float; here both sides run in double.  This proves the
 * GEOMETRY, THE HASH, THE VERTEX TRIPLE AND THE BLEND FORMULA agree; it does
 * not claim the product's last float bit.  That is what gate F2 is for.
 *
 *   (compile)  g++ -O2 -std=c++17 -static -o hex_parity.exe hex_parity.cpp
 *   (run)      hex_parity.exe > logs/hex_parity_cpp.txt
 */
#include <cstdio>
#include <cmath>
#include <cstdint>
typedef int32_t qint32;
typedef uint32_t quint32;

static float g_landHexSize = 0.0f;

/* ---- verbatim from src/lodgen.cpp -------------------------------------- */
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

static const double LODGEN_HEX_SKEW  = 0.57735026918962576;
static const double LODGEN_HEX_SCALE = 1.15470053837925152;

static void lodgenLandHexCell( double wx, double wy, double size,
                               qint32 * vi, qint32 * vj, double * w )
{
	const double px = wx / size;
	const double py = wy / size;
	const double sx = px - LODGEN_HEX_SKEW * py;
	const double sy = LODGEN_HEX_SCALE * py;
	const double bi = std::floor( sx );
	const double bj = std::floor( sy );
	const double tx = sx - bi;
	const double ty = sy - bj;
	const double tz = 1.0 - tx - ty;
	const bool up = tz > 0.0;
	const double o = up ? 0.0 : 1.0;
	w[0] = up ? tz : -tz;
	w[1] = up ? ty : 1.0 - ty;
	w[2] = up ? tx : 1.0 - tx;
	vi[0] = qint32( bi + o );        vj[0] = qint32( bj + o );
	vi[1] = qint32( bi + o );        vj[1] = qint32( bj + 1.0 - o );
	vi[2] = qint32( bi + 1.0 - o );  vj[2] = qint32( bj + o );
}

static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )
{
	return double( lodgenWarpHash( i, j, k ) ) / 4294967296.0;
}
/* ---- end verbatim ------------------------------------------------------ */

/* the stand-in for getPixelT: a closed form, so both languages agree exactly */
static void fakeTap( double u, double v, double * rgb )
{
	rgb[0] = 255.0 * u;
	rgb[1] = 255.0 * v;
	rgb[2] = 255.0 * u * v;
}

/* the shipped blend, with the stand-in in place of the texture */
static void hexSample( double wx, double wy, double tile, double * out )
{
	if ( g_landHexSize <= 0.0f ) {
		double u = std::fmod( wx / tile, 1.0 );
		double v = std::fmod( wy / tile, 1.0 );
		if ( u < 0.0 ) u += 1.0;
		if ( v < 0.0 ) v += 1.0;
		fakeTap( u, v, out );
		return;
	}
	qint32 vi[3], vj[3];
	double w[3];
	lodgenLandHexCell( wx, wy, double( g_landHexSize ), vi, vj, w );
	const double mean[3] = { 127.5, 127.5, 127.5 };
	double acc[3] = { 0.0, 0.0, 0.0 };
	double wsq = 0.0;
	for ( int k = 0; k < 3; k++ ) {
		const double ox = lodgenLandHexOffset( vi[k], vj[k], 0 ) * tile;
		const double oy = lodgenLandHexOffset( vi[k], vj[k], 1 ) * tile;
		double tu = std::fmod( ( wx + ox ) / tile, 1.0 );
		double tv = std::fmod( ( wy + oy ) / tile, 1.0 );
		if ( tu < 0.0 ) tu += 1.0;
		if ( tv < 0.0 ) tv += 1.0;
		double s[3];
		fakeTap( tu, tv, s );
		for ( int c = 0; c < 3; c++ )
			acc[c] += ( s[c] - mean[c] ) * w[k];
		wsq += w[k] * w[k];
	}
	if ( wsq > 1e-12 ) {
		const double inv = 1.0 / std::sqrt( wsq );
		for ( int c = 0; c < 3; c++ )
			acc[c] *= inv;
	}
	for ( int c = 0; c < 3; c++ )
		out[c] = mean[c] + acc[c];
}

int main()
{
	const double TILE = 341.3333;
	const double P[15][2] = {
		{ 0.0, 0.0 }, { 1.0, -1.0 }, { 341.3333, 341.3333 },
		{ -81920.0, 98304.0 }, { -81920.0, 81920.0 }, { -147456.0, -81920.0 },
		{ -16384.0, -81920.0 }, { 114688.0, -81920.0 }, { -16384.0, 65536.0 },
		{ 98304.0, 65536.0 }, { 1999999.0, -1999999.0 }, { 12345.678, -98765.432 },
		{ -0.5, -0.5 }, { -1024.0, -1024.0 }, { 1023.9999, 1023.9999 }
	};
	const float SIZES[4] = { 256.0f, 341.3333f, 512.0f, 682.6667f };

	/* OFF first: it must be the plain wrapped tap, which is what the shipped
	 * lodgenLandHexTap returns by an early `return` rather than by arithmetic */
	printf( "OFF\n" );
	for ( int n = 0; n < 15; n++ ) {
		double o[3];
		hexSample( P[n][0], P[n][1], TILE, o );
		printf( "%.9g %.9g %.9g\n", o[0], o[1], o[2] );
	}
	for ( int s = 0; s < 4; s++ ) {
		g_landHexSize = SIZES[s];
		printf( "SIZE=%.9g\n", double( SIZES[s] ) );
		for ( int n = 0; n < 15; n++ ) {
			/* the lattice cell and the weights, then the blended colour: a
			 * wrong vertex triple with right weights would otherwise hide */
			qint32 vi[3], vj[3];
			double w[3], o[3];
			lodgenLandHexCell( P[n][0], P[n][1], double( g_landHexSize ),
			                   vi, vj, w );
			hexSample( P[n][0], P[n][1], TILE, o );
			printf( "%d %d %d %d %d %d %.9g %.9g %.9g %.9g %.9g %.9g\n",
			        int( vi[0] ), int( vj[0] ), int( vi[1] ), int( vj[1] ),
			        int( vi[2] ), int( vj[2] ), w[0], w[1], w[2],
			        o[0], o[1], o[2] );
		}
	}
	return 0;
}
