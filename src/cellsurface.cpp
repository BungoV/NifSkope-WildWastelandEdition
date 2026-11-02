// lane ESPWRITE2 (2026-11-02): the surface pick's maths (src/cellsurface.h)

#include "cellsurface.h"

#include <QByteArray>

#include <algorithm>
#include <cmath>
#include <cstdlib>

QString cellSurfaceRed()
{
	return QString::fromLocal8Bit( qgetenv( "WW_CELLSURF_RED" ) ).trimmed().toLower();
}

bool cellSurfaceEyeDistance( float depth, double nearT, double farT, bool persp, double & zEye )
{
	if ( !( depth > 0.0f ) || !( depth < 1.0f - 1.0e-7f ) || !( farT > nearT ) || !( nearT > 0.0 || !persp ) )
		return false;
	const double d = double( depth );
	if ( !persp || cellSurfaceRed() == QLatin1String( "lin" ) ) {
		zEye = nearT + d * ( farT - nearT );
		return true;
	}
	/* glFrustum: ndc = ( (f+n) z_e + 2 f n ) / ( (f-n) * -z_e ) with z_e < 0; solved for -z_e */
	const double ndc = 2.0 * d - 1.0;
	const double den = ( farT + nearT ) - ndc * ( farT - nearT );
	if ( !( den > 0.0 ) )
		return false;
	zEye = 2.0 * farT * nearT / den;
	return true;
}

bool cellSurfaceRayParam( const float dir[3], const float forward[3], double zEye, double & t )
{
	const double c = double( dir[0] ) * forward[0] + double( dir[1] ) * forward[1] + double( dir[2] ) * forward[2];
	if ( !( c > 1.0e-6 ) )
		return false;
	t = zEye / c;
	return true;
}

static void sub3( const float a[3], const float b[3], double r[3] )
{
	for ( int i = 0; i < 3; i++ )
		r[i] = double( a[i] ) - double( b[i] );
}

static void cross3( const double a[3], const double b[3], double r[3] )
{
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

static double dot3( const double a[3], const double b[3] )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool cellSurfaceRayTri( const float o[3], const float d[3], const float a[3], const float b[3], const float c[3],
	float & t )
{
	double e1[3], e2[3], s[3], p[3], q[3];
	const double dd[3] = { d[0], d[1], d[2] };
	sub3( b, a, e1 );
	sub3( c, a, e2 );
	cross3( dd, e2, p );
	const double det = dot3( e1, p );
	if ( std::fabs( det ) < 1.0e-12 )
		return false;
	const double inv = 1.0 / det;
	sub3( o, a, s );
	const double u = dot3( s, p ) * inv;
	if ( u < 0.0 || u > 1.0 )
		return false;
	cross3( s, e1, q );
	const double v = dot3( dd, q ) * inv;
	if ( v < 0.0 || u + v > 1.0 )
		return false;
	const double tt = dot3( e2, q ) * inv;
	if ( !( tt > 1.0e-4 ) )
		return false;
	t = float( tt );
	return true;
}

bool cellSurfaceTriNormal( const float a[3], const float b[3], const float c[3], const float rayDir[3], float n[3] )
{
	double e1[3], e2[3], r[3];
	sub3( b, a, e1 );
	sub3( c, a, e2 );
	cross3( e1, e2, r );
	const double len = std::sqrt( dot3( r, r ) );
	if ( !( len > 1.0e-12 ) )
		return false;
	const double dd[3] = { rayDir[0], rayDir[1], rayDir[2] };
	const double s = ( dot3( r, dd ) > 0.0 ? -1.0 : 1.0 ) / len;
	for ( int i = 0; i < 3; i++ )
		n[i] = float( r[i] * s );
	return true;
}

float cellSurfaceTolerance( float t )
{
	return std::max( 2.0f, 0.01f * std::fabs( t ) );
}

namespace {
double g_near = 0.0, g_far = 0.0;
bool g_persp = true;
}

void cellSurfaceNotePlanes( double nearT, double farT, bool persp )
{
	g_near = nearT;
	g_far = farT;
	g_persp = persp;
}

void cellSurfacePlanes( double & nearT, double & farT, bool & persp )
{
	nearT = g_near;
	farT = g_far;
	persp = g_persp;
}

CellSurfaceStats & cellSurfaceStats()
{
	static CellSurfaceStats s;
	return s;
}

QString cellSurfaceStatsLine()
{
	const CellSurfaceStats & s = cellSurfaceStats();
	return QStringLiteral( "cell surface: %1 probes, %2 depth+triangle, %3 depth only, %4 whole-scene ray, %5 sky, "
		"%6 depth unreadable, %7 missed%8" )
		.arg( s.probes ).arg( s.refined ).arg( s.depthOnly ).arg( s.rayFallback ).arg( s.sky ).arg( s.unreadable )
		.arg( s.missed ).arg( cellSurfaceRed().isEmpty() ? QString() : QStringLiteral( " (RED %1)" ).arg( cellSurfaceRed() ) );
}
