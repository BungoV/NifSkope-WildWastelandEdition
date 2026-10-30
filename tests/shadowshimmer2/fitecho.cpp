/* lane SHADOWSHIMMER2 (CSM1GATEFIT): the code's fit (gl/sunshadowfit.h, what gl/sunshadow.cpp runs for Cascade Quality
 * Off) for a few cameras and suns, printed for csm1fit_selftest.py, which runs tests/spells/pbr_csm1_gates.py's own fit on
 * the same inputs. One line per (case, law, cascade):
 *   fit <case> <stable|old> <i> C fwd rc uc tanX tanY near map D L | zn zf texel n vw vh pb0 pb1 pb2 l b far */

#include "gl/sunshadowfit.h"

#include <cmath>
#include <cstdio>

using namespace WwCsmFit;

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

int main()
{
	const double PI = 3.14159265358979323846;
	struct Case { double C[3], yaw, pitch, elev, az; int map; double tanY; };
	const Case cases[] = {
		{ { 1234.5, 2345.25, 180.0 }, 35.0, -15.0, 40.0, 130.0, 2048, 0.52056705 },
		{ { -700.0, 3000.0, 2600.0 }, 10.0, -89.0, 62.0, 200.0, 512, 0.41421356 },
		{ { 0.0, -1200.0, 450.0 }, 200.0, -30.0, 18.0, 60.0, 2048, 0.70020754 },
		{ { 51234.75, -12000.5, 900.0 }, 300.0, 5.0, 75.0, 300.0, 1024, 0.52056705 },
	};
	const double split[2] = { 800.0, 3000.0 }, D = 3000.0, blend = 100.0, back = 15000.0, camNear = 7.5;
	int ci = 0;
	for ( const Case & k : cases ) {
		const double y = k.yaw * PI / 180.0, p = k.pitch * PI / 180.0;
		double f[3] = { std::cos( p ) * std::sin( y ), std::cos( p ) * std::cos( y ), std::sin( p ) };
		const double Z[3] = { 0, 0, 1 };
		double rc[3], uc[3];
		cross3( f, Z, rc );
		norm3( rc );
		cross3( rc, f, uc );
		const double e = k.elev * PI / 180.0, a = k.az * PI / 180.0;
		double L[3] = { -std::cos( e ) * std::sin( a ), -std::cos( e ) * std::cos( a ), -std::sin( e ) }, R[3], U[3];
		const double Y[3] = { 0, 1, 0 };
		cross3( L, Y, R );
		norm3( R );
		cross3( R, L, U );
		norm3( U );
		const double tanX = k.tanY * 1920.0 / 1080.0;
		const double far[3] = { split[0], split[1], D };
		for ( int law = 0; law < 2; law++ ) {
			for ( int i = 0; i < 3; i++ ) {
				In in;
				for ( int j = 0; j < 3; j++ ) {
					in.C[j] = k.C[j];
					in.fwd[j] = f[j];
					in.rc[j] = rc[j];
					in.uc[j] = uc[j];
					in.R[j] = R[j];
					in.U[j] = U[j];
					in.L[j] = L[j];
				}
				in.tanX = tanX;
				in.tanY = k.tanY;
				in.zn = i == 0 ? camNear : far[i - 1] - blend;
				in.zf = far[i] + blend;
				in.first = i == 0;
				in.map = k.map;
				in.lightBack = back;
				Out o;
				law ? fitOld( in, o ) : fitStable( in, o );
				std::printf( "fit %d %s %d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g "
					"%d %.17g %.17g %.17g %.17g | %.17g %.17g %.17g %d %d %d %.17g %.17g %.17g %.17g %.17g %.17g\n",
					ci, law ? "old" : "stable", i, k.C[0], k.C[1], k.C[2], f[0], f[1], f[2], rc[0], rc[1], rc[2], uc[0], uc[1], uc[2],
					tanX, k.tanY, camNear, k.map, D, L[0], L[1], L[2], in.zn, in.zf, o.texel, o.n, o.vw, o.vh, o.pb[0], o.pb[1],
					o.pb[2], o.l, o.b, o.far );
			}
		}
		ci++;
	}
	return 0;
}
