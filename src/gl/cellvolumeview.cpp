// lane VOLFIT1: the Space stage's debug view (src/gl/cellvolumeview.h)

#include "cellvolumeview.h"

#include "gl/glscene.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct VvState
{
	QHash<const void *, WwCellVolumeGeometry> views;   // built once at publish
	QString lastDump;
};

VvState & vv()
{
	static VvState s;
	return s;
}

void rotOf( const float deg[3], double m[3][3] )
{
	const double ax = deg[0] * kPi / 180.0, ay = deg[1] * kPi / 180.0, az = deg[2] * kPi / 180.0;
	const double cx = std::cos( ax ), sx = std::sin( ax ), cy = std::cos( ay ), sy = std::sin( ay ), cz = std::cos( az ),
		sz = std::sin( az );
	m[0][0] = cz * cy; m[0][1] = cz * sy * sx - sz * cx; m[0][2] = cz * sy * cx + sz * sx;
	m[1][0] = sz * cy; m[1][1] = sz * sy * sx + cz * cx; m[1][2] = sz * sy * cx - cz * sx;
	m[2][0] = -sy;     m[2][1] = cy * sx;                m[2][2] = cy * cx;
}

struct Pen
{
	WwCellVolumeGeometry & g;
	double m[3][3];
	float c[3];
	float rgba[4];
	int dash = 0;   // 0 solid, 1 dashed, 2 dotted: rooms read apart without their color

	void world( const double l[3], float w[3] ) const
	{
		for ( int i = 0; i < 3; i++ )
			w[i] = float( c[i] + m[i][0] * l[0] + m[i][1] * l[1] + m[i][2] * l[2] );
	}
	void seg( const double a[3], const double b[3] )
	{
		float wa[3], wb[3];
		world( a, wa );
		world( b, wb );
		const double len = std::sqrt( double( wb[0] - wa[0] ) * ( wb[0] - wa[0] ) + double( wb[1] - wa[1] ) * ( wb[1] - wa[1] )
			+ double( wb[2] - wa[2] ) * ( wb[2] - wa[2] ) );
		const double step = dash == 1 ? 24.0 : 8.0;
		const int n = dash == 0 ? 1 : std::max( 1, int( len / step ) );
		for ( int k = 0; k < n; k++ ) {
			if ( dash != 0 && ( k % 2 ) )
				continue;
			const double t0 = double( k ) / n, t1 = double( k + 1 ) / n;
			for ( double t : { t0, t1 } ) {
				for ( int i = 0; i < 3; i++ )
					g.lines.push_back( float( wa[i] + ( wb[i] - wa[i] ) * t ) );
				g.lineColor.insert( g.lineColor.end(), rgba, rgba + 4 );
			}
		}
	}
	//! an ellipse in the plane of axes (p, q) about `o`, radii (rp, rq)
	void ring( const double o[3], int p, int q, double rp, double rq, int segs, double a0 = 0, double a1 = 2 * kPi )
	{
		double prev[3] = { o[0], o[1], o[2] };
		for ( int k = 0; k <= segs; k++ ) {
			const double a = a0 + ( a1 - a0 ) * k / segs;
			double pt[3] = { o[0], o[1], o[2] };
			pt[p] += rp * std::cos( a );
			pt[q] += rq * std::sin( a );
			if ( k )
				seg( prev, pt );
			std::copy( pt, pt + 3, prev );
		}
	}
};

void drawShape( Pen & pen, const ProbeVolumeShape & s )
{
	rotOf( s.rotation, pen.m );
	std::copy( s.center, s.center + 3, pen.c );
	const double h[3] = { s.size[0] * 0.5, s.size[1] * 0.5, s.size[2] * 0.5 };
	switch ( s.type ) {
	case ProbeShapeType::Box: {
		for ( int a = 0; a < 3; a++ )
			for ( int s1 = -1; s1 <= 1; s1 += 2 )
				for ( int s2 = -1; s2 <= 1; s2 += 2 ) {
					double p0[3], p1[3];
					p0[a] = -h[a];
					p1[a] = h[a];
					p0[( a + 1 ) % 3] = p1[( a + 1 ) % 3] = s1 * h[( a + 1 ) % 3];
					p0[( a + 2 ) % 3] = p1[( a + 2 ) % 3] = s2 * h[( a + 2 ) % 3];
					pen.seg( p0, p1 );
				}
		break;
	}
	case ProbeShapeType::Wedge: {
		// full height at -y, zero at +y
		const double b[4][3] = { { -h[0], -h[1], -h[2] }, { h[0], -h[1], -h[2] }, { h[0], h[1], -h[2] }, { -h[0], h[1], -h[2] } };
		const double t0[3] = { -h[0], -h[1], h[2] }, t1[3] = { h[0], -h[1], h[2] };
		for ( int k = 0; k < 4; k++ )
			pen.seg( b[k], b[( k + 1 ) % 4] );
		pen.seg( b[0], t0 );
		pen.seg( b[1], t1 );
		pen.seg( t0, t1 );
		pen.seg( t0, b[3] );
		pen.seg( t1, b[2] );
		break;
	}
	case ProbeShapeType::Cylinder: {
		for ( int e = -1; e <= 1; e += 2 ) {
			const double o[3] = { 0, 0, e * h[2] };
			pen.ring( o, 0, 1, h[0], h[1], 32 );
		}
		for ( int k = 0; k < 4; k++ ) {
			const double a = k * kPi / 2;
			const double p0[3] = { h[0] * std::cos( a ), h[1] * std::sin( a ), -h[2] }, p1[3] = { p0[0], p0[1], h[2] };
			pen.seg( p0, p1 );
		}
		break;
	}
	case ProbeShapeType::Capsule: {
		const double r = std::min( h[0], h[1] ), e = std::max( 0.0, h[2] - r );
		for ( int s1 = -1; s1 <= 1; s1 += 2 ) {
			const double o[3] = { 0, 0, s1 * e };
			pen.ring( o, 0, 1, r, r, 32 );
			pen.ring( o, 0, 2, r, r, 16, s1 > 0 ? 0 : kPi, s1 > 0 ? kPi : 2 * kPi );
			pen.ring( o, 1, 2, r, r, 16, s1 > 0 ? 0 : kPi, s1 > 0 ? kPi : 2 * kPi );
		}
		for ( int k = 0; k < 4; k++ ) {
			const double a = k * kPi / 2;
			const double p0[3] = { r * std::cos( a ), r * std::sin( a ), -e }, p1[3] = { p0[0], p0[1], e };
			pen.seg( p0, p1 );
		}
		break;
	}
	case ProbeShapeType::Sphere: {
		const double o[3] = { 0, 0, 0 };
		pen.ring( o, 0, 1, h[0], h[1], 32 );
		pen.ring( o, 0, 2, h[0], h[2], 32 );
		pen.ring( o, 1, 2, h[1], h[2], 32 );
		break;
	}
	}
}

} // namespace

void wwCellVolumeColor( const QString & cellKey, quint32 id, float rgb[3] )
{
	// a hue from the hash, full saturation, bright: never the purple of a missing material
	const QByteArray h = QCryptographicHash::hash( ( cellKey + QLatin1Char( ':' ) + QString::number( id ) ).toUtf8(),
		QCryptographicHash::Md5 );
	double hue = ( quint8( h[0] ) * 256 + quint8( h[1] ) ) / 65536.0 * 360.0;
	if ( hue > 260.0 && hue < 320.0 )
		hue -= 60.0;
	const double c = 0.85, x = c * ( 1 - std::fabs( std::fmod( hue / 60.0, 2.0 ) - 1 ) ), m = 0.15;
	double r = 0, g = 0, b = 0;
	if ( hue < 60 ) { r = c; g = x; }
	else if ( hue < 120 ) { r = x; g = c; }
	else if ( hue < 180 ) { g = c; b = x; }
	else if ( hue < 240 ) { g = x; b = c; }
	else if ( hue < 300 ) { r = x; b = c; }
	else { r = c; b = x; }
	rgb[0] = float( r + m );
	rgb[1] = float( g + m );
	rgb[2] = float( b + m );
}

WwCellVolumeGeometry wwCellVolumeGeometry( const ProbeVolumes & v )
{
	WwCellVolumeGeometry g;
	for ( const ProbeVolumeRoom & r : v.rooms ) {
		Pen pen { g, {}, {}, {}, int( r.id % 3 ) };
		wwCellVolumeColor( r.cellKey, r.id, pen.rgba );
		pen.rgba[3] = 1.0f;
		for ( const ProbeVolumeShape & s : r.shapes ) {
			drawShape( pen, s );
			g.shapes++;
		}
		g.rooms++;
	}
	for ( const ProbeVolumeThreshold & t : v.thresholds ) {
		float col[4] = { 0.95f, 0.95f, 0.95f, 0.35f };
		if ( t.opening == QLatin1String( "door" ) ) { col[0] = 1.0f; col[1] = 0.7f; col[2] = 0.1f; }
		else if ( t.opening == QLatin1String( "window" ) ) { col[0] = 0.2f; col[1] = 0.9f; col[2] = 1.0f; }
		else if ( t.opening == QLatin1String( "portal" ) ) { col[0] = 0.75f; col[1] = 0.45f; col[2] = 1.0f; }
		double m[3][3];
		rotOf( t.shape.rotation, m );
		const double hx = t.shape.size[0] * 0.5, hz = t.shape.size[2] * 0.5;
		float q[4][3];
		const double lc[4][2] = { { -hx, -hz }, { hx, -hz }, { hx, hz }, { -hx, hz } };
		for ( int k = 0; k < 4; k++ )
			for ( int i = 0; i < 3; i++ )
				q[k][i] = float( t.shape.center[i] + m[i][0] * lc[k][0] + m[i][2] * lc[k][1] );
		for ( int k : { 0, 1, 2, 0, 2, 3 } ) {
			g.tris.insert( g.tris.end(), q[k], q[k] + 3 );
			g.triColor.insert( g.triColor.end(), col, col + 4 );
		}
		// its outline (opaque) and its normal, 48 units long
		Pen pen { g, {}, {}, { col[0], col[1], col[2], 1.0f }, 0 };
		for ( int a = 0; a < 3; a++ )
			for ( int b = 0; b < 3; b++ )
				pen.m[a][b] = a == b ? 1.0 : 0.0;
		for ( int k = 0; k < 4; k++ ) {
			const double a[3] = { q[k][0], q[k][1], q[k][2] }, b[3] = { q[( k + 1 ) % 4][0], q[( k + 1 ) % 4][1], q[( k + 1 ) % 4][2] };
			pen.seg( a, b );
		}
		const double c0[3] = { t.shape.center[0], t.shape.center[1], t.shape.center[2] };
		const double c1[3] = { c0[0] + t.normal[0] * 48.0, c0[1] + t.normal[1] * 48.0, c0[2] + t.normal[2] * 48.0 };
		pen.seg( c0, c1 );
		g.quads++;
	}
	return g;
}

void wwCellVolumeViewPublish( const void * nif, const ProbeVolumes & v )
{
	vv().views.insert( nif, wwCellVolumeGeometry( v ) );
}

void wwCellVolumeViewDraw( Scene * scene, const Transform & viewTrans )
{
	static const bool on = qgetenv( "WW_CELL_VOLUMES_VIEW" ).trimmed() == "1";
	if ( !on || !scene || !scene->nifModel || scene->selecting )
		return;
	VvState & s = vv();
	auto it = s.views.constFind( scene->nifModel );
	if ( it == s.views.constEnd() )
		return;
	const WwCellVolumeGeometry & g = *it;
	std::vector<Vector3> lp, tp;
	std::vector<FloatVector4> lc, tc;
	for ( size_t i = 0; i + 2 < g.lines.size(); i += 3 )
		lp.push_back( Vector3( g.lines[i], g.lines[i + 1], g.lines[i + 2] ) );
	for ( size_t i = 0; i + 3 < g.lineColor.size(); i += 4 )
		lc.push_back( FloatVector4( g.lineColor[i], g.lineColor[i + 1], g.lineColor[i + 2], g.lineColor[i + 3] ) );
	for ( size_t i = 0; i + 2 < g.tris.size(); i += 3 )
		tp.push_back( Vector3( g.tris[i], g.tris[i + 1], g.tris[i + 2] ) );
	for ( size_t i = 0; i + 3 < g.triColor.size(); i += 4 )
		tc.push_back( FloatVector4( g.triColor[i], g.triColor[i + 1], g.triColor[i + 2], g.triColor[i + 3] ) );
	scene->loadModelViewMatrix( viewTrans );
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_CULL_FACE );
	constexpr size_t kChunk = 65532;   // a multiple of 6 and of 2
	if ( !tp.empty() ) {
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDepthMask( GL_FALSE );   // translucent: the room's lines behind a threshold still show
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( -1.0f, -1.0f );
		for ( size_t i = 0; i < tp.size(); i += kChunk )
			scene->drawTriangles( tp.data() + i, std::min( kChunk, tp.size() - i ), tc.data() + i, true );
		glDisable( GL_POLYGON_OFFSET_FILL );
		glDepthMask( GL_TRUE );
		glDisable( GL_BLEND );
	}
	for ( size_t i = 0; i < lp.size(); i += kChunk )
		scene->drawLines( lp.data() + i, std::min( kChunk, lp.size() - i ), lc.data() + i );
	glDepthFunc( GL_LESS );
	static const QString dump = QString::fromLocal8Bit( qgetenv( "WW_CELL_VOLUMES_VIEW_DUMP" ) );
	if ( !dump.isEmpty() ) {
		const QString o = QStringLiteral( "rooms=%1 shapes=%2 thresholds=%3 lines=%4 tris=%5\n" ).arg( g.rooms ).arg( g.shapes )
			.arg( g.quads ).arg( lp.size() / 2 ).arg( tp.size() / 3 );
		if ( o != s.lastDump ) {
			QFile f( dump );
			if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
				f.write( o.toUtf8() );
				s.lastDump = o;
			}
		}
	}
}
