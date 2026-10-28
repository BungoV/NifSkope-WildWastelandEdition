// lane VOLFIT1: the Space stage's debug view (src/gl/cellvolumeview.h); lane VOLVIEW2: part of the PRTP Debug View

#include "cellvolumeview.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "gl/celllights.h"
#include "gl/cellprobeview.h"
#include "gl/glscene.h"

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kLabelReach = 2500.0;	// lane VOLVIEW2: room and threshold labels within this of the camera
constexpr double kProbeReach = 3000.0;	// probe rings and their links within this
constexpr double kRing = 22.0;	// the probe ring's radius: round the largest marker box (16)

struct VvDoc
{
	ProbeVolumes v;	// every room's group filled
	bool drivesGi = false;	// the GI's room labels are these rooms (WW_CELL_VOLUMES_GI=1): label k = rooms[k - 1]
	WwCellVolumeGeometry g;
	std::vector<WwCellVolumeLabel> fixedLabels;	// rooms and thresholds: world point, text, color
	std::vector<WwCellVolumeLabel> labels;	// the last frame's, culled to the camera
	WwCellProbeLinks links;
	const void * linkGi = nullptr;
	size_t linkProbes = 0;
	int linkRed = -1;
	int linkVer = -1;
	int dashRed = -1;	// lane VOLPLACE2: WW_CELL_VOLUMES_DASHRED the fixed lines were built with
	QString dumpKey;
	std::vector<Vector3> lp, tp;	// the GL arrays: the fixed lines first (fixedPoints), then the frame's
	std::vector<FloatVector4> lc, tc;
	size_t fixedPoints = 0;
};

struct VvState
{
	std::map<const void *, VvDoc> docs;
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

const float kOutdoors[3] = { 0.93f, 0.93f, 0.93f };	// the outdoors' color: white
const float kNoRoom[3] = { 0.45f, 0.45f, 0.45f };	// a probe whose read is solid: grey

QString roomName( const ProbeVolumes & v, int k )
{
	if ( k == 0 )
		return QStringLiteral( "outdoors" );
	if ( k > 0 && k <= int( v.rooms.size() ) )
		return probeVolumesRoomName( v.rooms[size_t( k - 1 )] );
	return QStringLiteral( "room?" );
}

//! the color of the space room k (1-based; 0 the outdoors): its group's
void spaceColor( const ProbeVolumes & v, int k, float rgb[3] )
{
	if ( k == 0 ) {
		std::copy( kOutdoors, kOutdoors + 3, rgb );
		return;
	}
	if ( k > 0 && k <= int( v.rooms.size() ) ) {
		const ProbeVolumeRoom & r = v.rooms[size_t( k - 1 )];
		wwCellVolumeColor( r.group.isEmpty() ? probeVolumesRoomName( r ) : r.group, rgb );
		return;
	}
	std::copy( kNoRoom, kNoRoom + 3, rgb );
}

//! the color of the GI's room label k: the space room's when the volumes drive the GI, else a hash of the label
void giColor( const VvDoc & d, int k, float rgb[3] )
{
	if ( k < 0 ) {
		std::copy( kNoRoom, kNoRoom + 3, rgb );
		return;
	}
	if ( k == 0 || ( d.drivesGi && k <= int( d.v.rooms.size() ) ) ) {
		spaceColor( d.v, k, rgb );
		return;
	}
	wwCellVolumeColor( QStringLiteral( "gi:%1" ).arg( k ), rgb );
}

QString giName( const VvDoc & d, int k )
{
	if ( k < 0 )
		return QStringLiteral( "none" );
	if ( k == 0 || ( d.drivesGi && k <= int( d.v.rooms.size() ) ) )
		return roomName( d.v, k );
	return QStringLiteral( "gi:%1" ).arg( k );
}

QString giGroup( const VvDoc & d, int k )
{
	if ( k > 0 && d.drivesGi && k <= int( d.v.rooms.size() ) )
		return d.v.rooms[size_t( k - 1 )].group;
	return giName( d, k );
}

void buildFixed( VvDoc & d )
{
	const ProbeVolumes & v = d.v;
	WwCellVolumeGeometry & g = d.g;
	g = wwCellVolumeGeometry( v );
	d.dashRed = wwEnv( "WW_CELL_VOLUMES_DASHRED" ).trimmed() == "1" ? 1 : 0;
	d.lp.clear();
	d.lc.clear();
	d.tp.clear();
	d.tc.clear();
	for ( size_t i = 0; i + 2 < g.lines.size(); i += 3 )
		d.lp.push_back( Vector3( g.lines[i], g.lines[i + 1], g.lines[i + 2] ) );
	for ( size_t i = 0; i + 3 < g.lineColor.size(); i += 4 )
		d.lc.push_back( FloatVector4( g.lineColor[i], g.lineColor[i + 1], g.lineColor[i + 2], g.lineColor[i + 3] ) );
	for ( size_t i = 0; i + 2 < g.tris.size(); i += 3 )
		d.tp.push_back( Vector3( g.tris[i], g.tris[i + 1], g.tris[i + 2] ) );
	for ( size_t i = 0; i + 3 < g.triColor.size(); i += 4 )
		d.tc.push_back( FloatVector4( g.triColor[i], g.triColor[i + 1], g.triColor[i + 2], g.triColor[i + 3] ) );
	d.fixedPoints = d.lp.size();
	d.fixedLabels.clear();
	for ( const ProbeVolumeRoom & r : v.rooms ) {
		if ( r.shapes.empty() )
			continue;
		const ProbeVolumeShape * big = &r.shapes.front();
		for ( const ProbeVolumeShape & s : r.shapes )
			if ( s.size[0] * s.size[1] * s.size[2] > big->size[0] * big->size[1] * big->size[2] )
				big = &s;
		WwCellVolumeLabel l;
		l.at = Vector3( big->center[0], big->center[1], big->center[2] );
		const QString n = probeVolumesRoomName( r );
		l.text = r.group.isEmpty() || r.group == n ? n : QStringLiteral( "%1  (%2)" ).arg( n, r.group );
		wwCellVolumeColor( r.group.isEmpty() ? n : r.group, l.rgb );
		d.fixedLabels.push_back( l );
	}
	for ( const ProbeVolumeThreshold & t : v.thresholds ) {
		WwCellVolumeLabel l;
		l.at = Vector3( t.shape.center[0], t.shape.center[1], t.shape.center[2] );
		l.text = QStringLiteral( "%1 <-> %2 %3" ).arg( roomName( v, t.a ), roomName( v, t.b ), t.opening );
		l.rgb[0] = l.rgb[1] = l.rgb[2] = 1.0f;
		l.isThreshold = true;
		d.fixedLabels.push_back( l );
	}
}

} // namespace

void wwCellVolumeColor( const QString & key, float rgb[3] )
{
	// a hue from the hash, full saturation, bright: never the purple of a missing material
	const QByteArray h = QCryptographicHash::hash( key.toUtf8(), QCryptographicHash::Md5 );
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
	/* lane VOLPLACE2 (bungo 10-28): rooms solid, thresholds dashed, so the two read apart at a glance.
	 * Red WW_CELL_VOLUMES_DASHRED=1: VOLVIEW2's style (a dash per room, thresholds solid) */
	const bool dashRed = wwEnv( "WW_CELL_VOLUMES_DASHRED" ).trimmed() == "1";
	for ( const ProbeVolumeRoom & r : v.rooms ) {
		Pen pen { g, {}, {}, {}, dashRed ? int( r.id % 3 ) : 0 };
		// lane VOLVIEW2: the group's color (one building, one color, across cells); the dash still tells its rooms apart
		wwCellVolumeColor( r.group.isEmpty() ? probeVolumesRoomName( r ) : r.group, pen.rgba );
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
		if ( t.shape.hull ) {   // lane VOLPLACE2: a threshold snapped to its frame: its mid-depth face and the frame's 12 edges
			static const int mid[4][2] = { { 0, 2 }, { 1, 3 }, { 5, 7 }, { 4, 6 } };
			for ( int k = 0; k < 4; k++ )
				for ( int i = 0; i < 3; i++ )
					q[k][i] = 0.5f * ( t.shape.corners[mid[k][0]][i] + t.shape.corners[mid[k][1]][i] );
			Pen pen { g, {}, {}, { col[0], col[1], col[2], 1.0f }, 0 };
			for ( int a = 0; a < 3; a++ )
				for ( int b = 0; b < 3; b++ )
					pen.m[a][b] = a == b ? 1.0 : 0.0;
			for ( int a = 0; a < 8; a++ )
				for ( int bit : { 1, 2, 4 } )
					if ( !( a & bit ) ) {
						double p0[3], p1[3];
						for ( int i = 0; i < 3; i++ ) {
							p0[i] = t.shape.corners[a][i];
							p1[i] = t.shape.corners[a | bit][i];
						}
						pen.seg( p0, p1 );
					}
		}
		for ( int k : { 0, 1, 2, 0, 2, 3 } ) {
			g.tris.insert( g.tris.end(), q[k], q[k] + 3 );
			g.triColor.insert( g.triColor.end(), col, col + 4 );
		}
		/* lane VOLVIEW2: a two-color edge, the outline twice: 3 units to room a's side in a's color, 3 units to b's
		 * side in b's (the outdoors white), so the threshold shows which two rooms it joins */
		for ( int side = 0; side < 2; side++ ) {
			float rgb[3];
			spaceColor( v, side == 0 ? t.a : t.b, rgb );
			Pen pen { g, {}, {}, { rgb[0], rgb[1], rgb[2], 1.0f }, dashRed ? 0 : 1 };
			for ( int a = 0; a < 3; a++ )
				for ( int b = 0; b < 3; b++ )
					pen.m[a][b] = a == b ? 1.0 : 0.0;
			const double o = side == 0 ? -3.0 : 3.0;
			for ( int k = 0; k < 4; k++ ) {
				const double a[3] = { q[k][0] + t.normal[0] * o, q[k][1] + t.normal[1] * o, q[k][2] + t.normal[2] * o };
				const int k1 = ( k + 1 ) % 4;
				const double b[3] = { q[k1][0] + t.normal[0] * o, q[k1][1] + t.normal[1] * o, q[k1][2] + t.normal[2] * o };
				pen.seg( a, b );
			}
			if ( side == 1 ) {	// its normal (a toward b), 48 units long, in b's color
				const double c0[3] = { t.shape.center[0], t.shape.center[1], t.shape.center[2] };
				const double c1[3] = { c0[0] + t.normal[0] * 48.0, c0[1] + t.normal[1] * 48.0, c0[2] + t.normal[2] * 48.0 };
				pen.seg( c0, c1 );
			}
		}
		g.quads++;
	}
	return g;
}

WwCellProbeLinks wwCellProbeLinksBuild( const WwCellGi & G, const std::vector<float> & probes, int red )
{
	WwCellProbeLinks L;
	const size_t n = probes.size() / 21;
	L.room0.assign( n, -1 );
	L.room1.assign( n, -1 );
	L.dropped.assign( n, 0 );
	L.start.assign( n + 1, 0 );
	if ( G.rooms.empty() || G.slotRooms.empty() || G.roomsCell <= 0 ) {
		L.why = QStringLiteral( "no rooms in the GI (the links are the rooms rule)" );
		return L;
	}
	if ( red & 8 ) {
		L.why = QStringLiteral( "red noclamp: the rooms rule is off" );
		return L;
	}
	const size_t nVox = size_t( G.dims[0] ) * size_t( G.dims[1] ) * size_t( G.dims[2] );
	if ( G.slotRooms.size() != nVox || G.rgba.size() < nVox * 12 * 4 ) {
		L.why = QStringLiteral( "the GI grid has no two-slot layout" );
		return L;
	}
	const double rc = G.roomsCell, vox = G.voxel;
	auto unpack = []( float f, int o[2] ) {
		const int k = int( f + 0.5f );
		o[0] = k / 4096 - 1;
		o[1] = k - ( k / 4096 ) * 4096 - 1;
	};
	// cellRoomAt (-1: solid or off the grid) and cellRoomSolid (off the grid: not solid)
	auto cellOf = [&]( const double q[3], size_t * idx ) {
		int c[3];
		for ( int a = 0; a < 3; a++ ) {
			c[a] = int( std::floor( ( q[a] - G.roomsOrigin[a] ) / rc ) );
			if ( c[a] < 0 || c[a] >= G.roomsDims[a] )
				return false;
		}
		*idx = size_t( c[0] ) + size_t( G.roomsDims[0] ) * ( size_t( c[1] ) + size_t( G.roomsDims[1] ) * size_t( c[2] ) );
		return true;
	};
	auto roomAt = [&]( const double q[3], int o[2] ) {
		size_t i;
		o[0] = o[1] = -1;
		if ( cellOf( q, &i ) )
			unpack( G.rooms[i], o );
	};
	auto solid = [&]( const double q[3] ) {
		size_t i;
		if ( !cellOf( q, &i ) )
			return false;
		int o[2];
		unpack( G.rooms[i], o );
		return o[0] < 0;
	};
	std::vector<std::vector<qint64>> keys( n );
	for ( size_t p = 0; p < n; p++ ) {
		const double P[3] = { probes[p * 21], probes[p * 21 + 1], probes[p * 21 + 2] };
		// a surface at the probe facing up (N = +z): the room read at P + N x 0.75 cell, else 1.75 (cellGiRoomSample)
		const double q1[3] = { P[0], P[1], P[2] + 0.75 * rc }, q2[3] = { P[0], P[1], P[2] + 1.75 * rc };
		int Lr[2];
		roomAt( q1, Lr );
		if ( Lr[0] < 0 )
			roomAt( q2, Lr );
		L.room0[p] = Lr[0];
		L.room1[p] = Lr[1];
		if ( Lr[0] < 0 )
			continue;	// both reads solid: the shader's neighbour search, not modelled here (no links)
		// the view point (cellGiSeeFrom): the first read that is not solid
		const double * from = !solid( q1 ) ? q1 : q2;
		double g[3];
		int i0[3];
		double f[3];
		for ( int a = 0; a < 3; a++ ) {
			g[a] = ( P[a] + ( a == 2 ? 0.5 * vox : 0.0 ) - G.origin[a] ) / vox - 0.5;
			i0[a] = int( std::floor( g[a] ) );
			f[a] = g[a] - i0[a];
		}
		for ( int k = 0; k < 8; k++ ) {
			const int o[3] = { k & 1, ( k >> 1 ) & 1, ( k >> 2 ) & 1 };
			int c[3];
			double w = 1.0;
			for ( int a = 0; a < 3; a++ ) {
				c[a] = std::clamp( i0[a] + o[a], 0, G.dims[a] - 1 );
				w *= o[a] ? f[a] : 1.0 - f[a];
			}
			const size_t vi = size_t( c[0] ) + size_t( G.dims[0] ) * ( size_t( c[1] ) + size_t( G.dims[1] ) * size_t( c[2] ) );
			int S[2];
			unpack( G.slotRooms[vi], S );
			const int base = ( S[0] >= 0 && ( S[0] == Lr[0] || S[0] == Lr[1] ) ) ? 0
				: ( S[1] >= 0 && ( S[1] == Lr[0] || S[1] == Lr[1] ) ) ? 1 : -1;
			if ( base < 0 || w <= 0.0 )
				continue;
			if ( ( red & 16 ) == 0 ) {	// cellGiSeen
				double d[3];
				for ( int a = 0; a < 3; a++ )
					d[a] = G.origin[a] + ( c[a] + 0.5 ) * vox - from[a];
				const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
				const int nt = std::clamp( int( std::ceil( len / ( 0.5 * rc ) ) ), 1, 12 );
				bool seen = true;
				for ( int j = 1; j <= nt && seen; j++ ) {
					const double t = double( j ) / double( nt + 1 );
					const double s[3] = { from[0] + d[0] * t, from[1] + d[1] * t, from[2] + d[2] * t };
					if ( solid( s ) )
						seen = false;
				}
				if ( !seen ) {
					L.dropped[p]++;
					continue;
				}
			}
			const bool two = ( red & 256 ) == 0 && Lr[0] >= 0 && Lr[1] >= 0 && Lr[0] != Lr[1] && base == 0 && S[1] >= 0
				&& ( S[1] == Lr[0] || S[1] == Lr[1] );
			for ( int q = 0; q < ( two ? 2 : 1 ); q++ ) {
				const int slot = q == 0 ? base : 1;
				// an empty slot holds no light: no weight (the +Z face's valid flag, the read for N = +z)
				if ( G.rgba[( ( size_t( slot ) * 6 + 4 ) * nVox + vi ) * 4 + 3] <= 0.0f )
					continue;
				keys[p].push_back( qint64( vi ) * 2 + slot );
			}
		}
		std::sort( keys[p].begin(), keys[p].end() );
		keys[p].erase( std::unique( keys[p].begin(), keys[p].end() ), keys[p].end() );
	}
	std::unordered_map<qint64, std::vector<int>> readers;
	for ( size_t p = 0; p < n; p++ )
		for ( qint64 k : keys[p] )
			readers[k].push_back( int( p ) );
	std::vector<int> nb;
	for ( size_t p = 0; p < n; p++ ) {
		nb.clear();
		for ( qint64 k : keys[p] )
			for ( int q : readers[k] )
				if ( q != int( p ) )
					nb.push_back( q );
		std::sort( nb.begin(), nb.end() );
		nb.erase( std::unique( nb.begin(), nb.end() ), nb.end() );
		for ( int q : nb ) {
			L.to.push_back( q );
			L.thr.push_back( L.room0[size_t( q )] != L.room0[p] ? 1 : 0 );
		}
		L.start[p + 1] = int( L.to.size() );
	}
	L.ok = true;
	return L;
}

void wwCellVolumeViewPublish( const void * nif, const ProbeVolumes & v, bool drivesGi )
{
	VvDoc & d = vv().docs[nif];
	d = VvDoc();
	d.v = v;
	bool fill = false;
	for ( const ProbeVolumeRoom & r : d.v.rooms )
		fill = fill || r.group.isEmpty();
	if ( fill )	// a .space file from before VOLVIEW2 (or the nogroup red): the groups from the thresholds read back
		probeVolumesGroups( &d.v );
	d.drivesGi = drivesGi;
	buildFixed( d );
}

bool wwCellVolumeViewOn( const void * nif )
{
	// the PRTP Debug View tick (the probe view's shown flag); WW_CELL_VOLUMES_VIEW=1 / 0 pins it on / off (harness)
	const QByteArray pin = wwEnv( "WW_CELL_VOLUMES_VIEW" ).trimmed();
	if ( pin == "1" )
		return true;
	if ( pin == "0" )
		return false;
	const WwCellProbeView * pv = wwCellProbeViewFor( nif );
	return pv && pv->probesShown;
}

std::vector<WwCellVolumeLabel> wwCellVolumeViewLabels( const void * nif )
{
	auto it = vv().docs.find( nif );
	if ( it == vv().docs.end() || !wwCellVolumeViewOn( nif ) )
		return {};
	return it->second.labels;
}

void wwCellVolumeViewDraw( Scene * scene, const Transform & viewTrans )
{
	if ( !scene || !scene->nifModel || scene->selecting )
		return;
	VvState & s = vv();
	auto it = s.docs.find( scene->nifModel );
	if ( it == s.docs.end() )
		return;
	VvDoc & d = it->second;
	if ( !wwCellVolumeViewOn( scene->nifModel ) ) {
		d.labels.clear();
		return;
	}
	if ( d.dashRed != ( wwEnv( "WW_CELL_VOLUMES_DASHRED" ).trimmed() == "1" ? 1 : 0 ) )
		buildFixed( d );   // the dash red set live: the rooms and thresholds again
	QElapsedTimer timer;
	timer.start();
	const WwCellVolumeGeometry & g = d.g;
	// the rooms and thresholds were built at publish; the probes' rings and links are appended past them each frame
	std::vector<Vector3> & lp = d.lp;
	std::vector<FloatVector4> & lc = d.lc;
	const std::vector<Vector3> & tp = d.tp;
	const std::vector<FloatVector4> & tc = d.tc;
	const size_t fixedLines = d.fixedPoints / 2;
	lp.resize( d.fixedPoints );
	lc.resize( d.fixedPoints );

	// lane VOLVIEW2: the probes (a ring in their room's color) and the probes each works with (the links)
	const WwCellProbeView * pv = wwCellProbeViewFor( scene->nifModel );
	const WwCellGi * G = wwCellGiFor( scene->nifModel );
	const int red = wwCellPassRed();
	const int ver = wwCellGiVersion( scene->nifModel );
	if ( pv && G && ( d.linkGi != G || d.linkVer != ver || d.linkProbes != pv->probes.size() || d.linkRed != red ) ) {
		d.links = wwCellProbeLinksBuild( *G, pv->probes, red );
		d.linkGi = G;
		d.linkProbes = pv->probes.size();
		d.linkRed = red;
		d.linkVer = ver;
	}
	size_t probeRings = 0, linkLines = 0;
	if ( pv && d.linkGi == G && G ) {
		const size_t n = pv->probes.size() / 21;
		std::vector<char> nearP( n, 0 );
		for ( size_t p = 0; p < n; p++ ) {
			const Vector3 c = viewTrans * Vector3( pv->probes[p * 21], pv->probes[p * 21 + 1], pv->probes[p * 21 + 2] );
			nearP[p] = c.length() < kProbeReach ? 1 : 0;
		}
		auto put = [&]( const double a[3], const double b[3], const float rgb[3] ) {
			lp.push_back( Vector3( float( a[0] ), float( a[1] ), float( a[2] ) ) );
			lp.push_back( Vector3( float( b[0] ), float( b[1] ), float( b[2] ) ) );
			lc.push_back( FloatVector4( rgb[0], rgb[1], rgb[2], 1.0f ) );
			lc.push_back( FloatVector4( rgb[0], rgb[1], rgb[2], 1.0f ) );
		};
		for ( size_t p = 0; p < n; p++ ) {
			if ( !nearP[p] )
				continue;
			const double P[3] = { pv->probes[p * 21], pv->probes[p * 21 + 1], pv->probes[p * 21 + 2] };
			float rgb[3];
			giColor( d, p < d.links.room0.size() ? d.links.room0[p] : -1, rgb );
			for ( int ax = 0; ax < 3; ax++ ) {	// three rings: the round probe
				const int a = ax == 2 ? 1 : 0, b = ax == 0 ? 1 : 2;
				for ( int k = 0; k < 12; k++ ) {
					double u[3] = { P[0], P[1], P[2] }, w[3] = { P[0], P[1], P[2] };
					u[a] += kRing * std::cos( k * kPi / 6 );
					u[b] += kRing * std::sin( k * kPi / 6 );
					w[a] += kRing * std::cos( ( k + 1 ) * kPi / 6 );
					w[b] += kRing * std::sin( ( k + 1 ) * kPi / 6 );
					put( u, w, rgb );
				}
			}
			probeRings++;
			if ( !d.links.ok )
				continue;
			for ( int e = d.links.start[p]; e < d.links.start[p + 1]; e++ ) {
				const int q = d.links.to[size_t( e )];
				if ( nearP[size_t( q )] && q < int( p ) )
					continue;	// drawn from q
				const double Q[3] = { pv->probes[size_t( q ) * 21], pv->probes[size_t( q ) * 21 + 1], pv->probes[size_t( q ) * 21 + 2] };
				if ( !d.links.thr[size_t( e )] ) {	// the same room: one solid line in its group's color
					put( P, Q, rgb );
				} else {	// through a threshold: dashed, each half in its own end's room color
					float rq[3];
					giColor( d, d.links.room0[size_t( q )], rq );
					for ( int k = 0; k < 8; k += 2 ) {
						const double t0 = k / 8.0, t1 = ( k + 1 ) / 8.0;
						const double a[3] = { P[0] + ( Q[0] - P[0] ) * t0, P[1] + ( Q[1] - P[1] ) * t0, P[2] + ( Q[2] - P[2] ) * t0 };
						const double b[3] = { P[0] + ( Q[0] - P[0] ) * t1, P[1] + ( Q[1] - P[1] ) * t1, P[2] + ( Q[2] - P[2] ) * t1 };
						put( a, b, k < 4 ? rgb : rq );
					}
				}
				linkLines++;
			}
		}
	}
	// the labels within reach (painted after the frame by GLView, src/glview.cpp)
	d.labels.clear();
	for ( const WwCellVolumeLabel & l : d.fixedLabels )
		if ( ( viewTrans * l.at ).length() < kLabelReach )
			d.labels.push_back( l );

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
	const qint64 ns = timer.nsecsElapsed();

	/* WW_CELL_VOLUMES_VIEW_DUMP=<file>: what is drawn (the first line as before VOLVIEW2), every room with its group,
	 * every threshold with its two rooms, and every probe's room and links (written again when the links change) */
	WW_SWITCH_STATIC( QString, dump, QString::fromLocal8Bit( wwEnv( "WW_CELL_VOLUMES_VIEW_DUMP" ) ) );
	if ( dump.isEmpty() )
		return;
	const QString key = QStringLiteral( "%1 %2 %3 %4 %5" ).arg( quintptr( d.linkGi ) ).arg( d.linkProbes ).arg( d.linkRed )
		.arg( d.links.to.size() ).arg( d.linkVer );
	if ( key == d.dumpKey )
		return;
	QFile f( dump );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return;
	QTextStream t( &f );
	t << QStringLiteral( "rooms=%1 shapes=%2 thresholds=%3 lines=%4 tris=%5\n" ).arg( g.rooms ).arg( g.shapes )
		.arg( g.quads ).arg( fixedLines ).arg( tp.size() / 3 );
	QHash<QString, int> groupRooms;
	for ( const ProbeVolumeRoom & r : d.v.rooms )
		groupRooms[r.group]++;
	t << "groups " << groupRooms.size() << " gi " << ( d.drivesGi ? "space" : "own" ) << "\n";
	for ( const ProbeVolumeRoom & r : d.v.rooms )
		t << "room " << probeVolumesRoomName( r ) << " group " << r.group << " source " << r.source << " shapes " << r.shapes.size()
		  << " cells " << r.cells << "\n";
	for ( size_t i = 0; i < d.v.thresholds.size(); i++ ) {
		const ProbeVolumeThreshold & th = d.v.thresholds[i];
		t << "threshold " << i << " " << roomName( d.v, th.a ) << " <-> " << roomName( d.v, th.b ) << " " << th.opening
		  << " at " << qRound( th.shape.center[0] ) << " " << qRound( th.shape.center[1] ) << " " << qRound( th.shape.center[2] ) << "\n";
	}
	if ( G )
		t << "gi dims " << G->dims[0] << " " << G->dims[1] << " " << G->dims[2] << " voxel " << G->voxel << " rooms dims "
		  << G->roomsDims[0] << " " << G->roomsDims[1] << " " << G->roomsDims[2] << " cell " << G->roomsCell << " red " << red << "\n";
	const WwCellProbeLinks & L = d.links;
	qint64 nRoom = 0, nThr = 0, nDrop = 0;
	if ( pv ) {
		const size_t n = pv->probes.size() / 21;
		for ( size_t p = 0; p < n && L.start.size() == n + 1; p++ ) {
			t << "probe " << p << " at " << QString::number( pv->probes[p * 21], 'f', 1 ) << " "
			  << QString::number( pv->probes[p * 21 + 1], 'f', 1 ) << " " << QString::number( pv->probes[p * 21 + 2], 'f', 1 )
			  << " room " << L.room0[p] << "," << L.room1[p] << " name " << giName( d, L.room0[p] ) << " group "
			  << giGroup( d, L.room0[p] ) << " dropped " << L.dropped[p] << " links";
			nDrop += L.dropped[p];
			for ( int e = L.start[p]; e < L.start[p + 1]; e++ ) {
				t << " " << L.to[size_t( e )] << ( L.thr[size_t( e )] ? ":t" : ":r" );
				( L.thr[size_t( e )] ? nThr : nRoom )++;
			}
			t << "\n";
		}
	}
	t << "links " << ( L.ok ? QStringLiteral( "ok" ) : QStringLiteral( "none (%1)" ).arg( L.why ) ) << " probes "
	  << ( pv ? pv->probes.size() / 21 : 0 ) << " room " << nRoom << " threshold " << nThr << " dropped " << nDrop
	  << " drawn rings " << probeRings << " links " << linkLines << " labels " << d.labels.size() << " draw ms "
	  << QString::number( ns / 1e6, 'f', 3 ) << "\n";
	d.dumpKey = key;
}
