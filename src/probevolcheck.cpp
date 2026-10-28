/*! lane VOLRAY1 (2026-10-27): `volcheck`, the independent ray check of the Space stage's shapes (probevolcheck.h).
 *  It shares no code with the fitter (src/probevolumes.cpp): its own .space reader, its own shape containment, its
 *  own lattice; the only common inputs are the soup's triangles and the .space files. */
#include "probevolcheck.h"

#include "bakert.h"
#include "probebvh.h"
#include "probeplace.h"
#include "probevolopen.h"

#include <QColor>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- shapes (the format's definition, written afresh)

struct VShape
{
	int type = 0;               // 0 box, 1 cylinder, 2 wedge, 3 capsule, 4 sphere
	double c[3] = { 0, 0, 0 }, h[3] = { 0, 0, 0 };
	double r[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };   // R = Rz Ry Rx; local = R^T (p - c)
	double lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };              // world bounds
	double vol = 0;

	void setRotation( const double deg[3] )
	{
		const double a[3] = { deg[0] * kPi / 180.0, deg[1] * kPi / 180.0, deg[2] * kPi / 180.0 };
		const double X[3][3] = { { 1, 0, 0 }, { 0, std::cos( a[0] ), -std::sin( a[0] ) }, { 0, std::sin( a[0] ), std::cos( a[0] ) } };
		const double Y[3][3] = { { std::cos( a[1] ), 0, std::sin( a[1] ) }, { 0, 1, 0 }, { -std::sin( a[1] ), 0, std::cos( a[1] ) } };
		const double Z[3][3] = { { std::cos( a[2] ), -std::sin( a[2] ), 0 }, { std::sin( a[2] ), std::cos( a[2] ), 0 }, { 0, 0, 1 } };
		double zy[3][3];
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ ) {
				zy[i][j] = 0;
				for ( int k = 0; k < 3; k++ )
					zy[i][j] += Z[i][k] * Y[k][j];
			}
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 3; j++ ) {
				r[i][j] = 0;
				for ( int k = 0; k < 3; k++ )
					r[i][j] += zy[i][k] * X[k][j];
			}
	}
	void finish()
	{
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = 1e300;
			hi[k] = -1e300;
		}
		for ( int n = 0; n < 8; n++ ) {
			const double l[3] = { ( n & 1 ) ? h[0] : -h[0], ( n & 2 ) ? h[1] : -h[1], ( n & 4 ) ? h[2] : -h[2] };
			for ( int i = 0; i < 3; i++ ) {
				const double w = c[i] + r[i][0] * l[0] + r[i][1] * l[1] + r[i][2] * l[2];
				lo[i] = std::min( lo[i], w );
				hi[i] = std::max( hi[i], w );
			}
		}
		const double a = 2 * h[0], b = 2 * h[1], z = 2 * h[2];
		switch ( type ) {
		case 0: vol = a * b * z; break;
		case 1: vol = kPi * h[0] * h[1] * z; break;
		case 2: vol = a * b * z * 0.5; break;
		case 3: {
			const double rr = std::min( h[0], h[1] );
			vol = kPi * rr * rr * std::max( 0.0, z - 2 * rr ) + 4.0 / 3.0 * kPi * rr * rr * rr;
			break;
		}
		default: vol = 4.0 / 3.0 * kPi * h[0] * h[1] * h[2]; break;
		}
	}
	void local( const double p[3], double l[3] ) const
	{
		const double d[3] = { p[0] - c[0], p[1] - c[1], p[2] - c[2] };
		for ( int k = 0; k < 3; k++ )
			l[k] = r[0][k] * d[0] + r[1][k] * d[1] + r[2][k] * d[2];
	}
	bool in( const double p[3] ) const
	{
		for ( int k = 0; k < 3; k++ )
			if ( p[k] < lo[k] || p[k] > hi[k] )
				return false;
		double l[3];
		local( p, l );
		switch ( type ) {
		case 0:
			return std::fabs( l[0] ) <= h[0] && std::fabs( l[1] ) <= h[1] && std::fabs( l[2] ) <= h[2];
		case 1:
			return std::fabs( l[2] ) <= h[2] && h[0] > 0 && h[1] > 0
				&& ( l[0] * l[0] ) / ( h[0] * h[0] ) + ( l[1] * l[1] ) / ( h[1] * h[1] ) <= 1.0;
		case 2:   // full height at -y, nothing at +y: the top falls linearly across y
			return h[1] > 0 && std::fabs( l[0] ) <= h[0] && std::fabs( l[1] ) <= h[1] && l[2] >= -h[2]
				&& ( l[2] + h[2] ) <= h[2] * ( 1.0 - l[1] / h[1] );
		case 3: {
			const double rr = std::min( h[0], h[1] ), half = std::max( 0.0, h[2] - rr );
			const double z = std::max( -half, std::min( half, l[2] ) ) - l[2];
			return l[0] * l[0] + l[1] * l[1] + z * z <= rr * rr;
		}
		default:
			return h[0] > 0 && h[1] > 0 && h[2] > 0
				&& ( l[0] * l[0] ) / ( h[0] * h[0] ) + ( l[1] * l[1] ) / ( h[1] * h[1] ) + ( l[2] * l[2] ) / ( h[2] * h[2] ) <= 1.0;
		}
	}
	//! does the segment a-b touch this shape's local box (a threshold is a box)
	bool segBox( const double a[3], const double b[3] ) const
	{
		double la[3], lb[3];
		local( a, la );
		local( b, lb );
		double t0 = 0, t1 = 1;
		for ( int k = 0; k < 3; k++ ) {
			const double d = lb[k] - la[k];
			if ( std::fabs( d ) < 1e-12 ) {
				if ( std::fabs( la[k] ) > h[k] )
					return false;
				continue;
			}
			double u = ( -h[k] - la[k] ) / d, v = ( h[k] - la[k] ) / d;
			if ( u > v )
				std::swap( u, v );
			t0 = std::max( t0, u );
			t1 = std::min( t1, v );
			if ( t0 > t1 )
				return false;
		}
		return true;
	}
};

bool readV3( const QJsonValue & v, double o[3] )
{
	const QJsonArray a = v.toArray();
	if ( a.size() != 3 )
		return false;
	for ( int k = 0; k < 3; k++ )
		o[k] = a[k].toDouble();
	return true;
}

bool shapeOf( const QJsonObject & o, VShape * s )
{
	static const char * names[5] = { "box", "cylinder", "wedge", "capsule", "sphere" };
	const QString t = o.value( QStringLiteral( "type" ) ).toString();
	s->type = -1;
	for ( int k = 0; k < 5; k++ )
		if ( t == QLatin1String( names[k] ) )
			s->type = k;
	double size[3], rot[3] = { 0, 0, 0 };
	if ( s->type < 0 || !readV3( o.value( QStringLiteral( "center" ) ), s->c ) || !readV3( o.value( QStringLiteral( "size" ) ), size ) )
		return false;
	readV3( o.value( QStringLiteral( "rotation" ) ), rot );
	for ( int k = 0; k < 3; k++ )
		s->h[k] = std::max( 0.0, size[k] * 0.5 );
	s->setRotation( rot );
	s->finish();
	return true;
}

struct VRoom
{
	QString cell, id, source;
	double anchor[3] = { 0, 0, 0 };
	std::vector<VShape> shapes;
	bool hand = false, check = true;
	double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
	double vol = 0;   // the sum of its shapes' volumes (roomAt's tie order: the smallest room wins)
	QString name() const { return cell + QLatin1Char( ':' ) + id; }
};

struct VThr
{
	int a = -1, b = -1;   // room index; -1 outside, -2 a name no file holds
	QString opening, cell;
	VShape box;
};

// ---------------------------------------------------------------- rays

struct Tracer
{
	const ProbeSoup * soup = nullptr;
	probebvh::Bvh bvh;
	double O[2] = { 0, 0 };
	size_t nSoup = 0;          // triangles of the soup; glass after them
	std::unique_ptr<bakert::Scene> scene;
	bool gpu = false;
	long long rays = 0;
	double ms = 0;

	void cast( const std::vector<bakert::XRay> & r, std::vector<bakert::XHit> & out, bool anyHit )
	{
		QElapsedTimer t;
		t.start();
		out.resize( r.size() );
		if ( gpu && scene ) {
			scene->cast( r.data(), r.size(), out.data(), anyHit );
		} else {
			std::atomic<size_t> next { 0 };
			const size_t chunk = 2048;
			std::vector<std::thread> pool;
			const int n = bakert::threads();
			for ( int w = 0; w < n; w++ )
				pool.emplace_back( [&]() {
					for ( ;; ) {
						const size_t b = next.fetch_add( chunk );
						if ( b >= r.size() )
							break;
						const size_t e = std::min( r.size(), b + chunk );
						for ( size_t i = b; i < e; i++ ) {
							double th = 0;
							int tri = -1;
							if ( bvh.ray( r[i].o, r[i].d, r[i].tmax, &th, &tri ) )
								out[i] = { th, tri };
							else
								out[i] = { 0.0, -1 };
						}
					}
				} );
			for ( std::thread & th : pool )
				th.join();
		}
		rays += qint64( r.size() );
		ms += double( t.nsecsElapsed() ) / 1e6;
	}
	//! the ray met the triangle's back (a one-sided face seen from behind); glass and two-sided faces have none
	bool back( int tri, const double d[3] ) const
	{
		if ( tri < 0 || size_t( tri ) >= nSoup || soup->isTwoSided( size_t( tri ) ) )
			return false;
		const float * p = &bvh.t[size_t( tri ) * 9];
		const double e1[3] = { double( p[3] ) - p[0], double( p[4] ) - p[1], double( p[5] ) - p[2] };
		const double e2[3] = { double( p[6] ) - p[0], double( p[7] ) - p[1], double( p[8] ) - p[2] };
		const double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
		return n[0] * d[0] + n[1] * d[1] + n[2] * d[2] > 0;
	}
	bakert::XRay ray( const double a[3], const double d[3], double tmax ) const
	{
		bakert::XRay x;
		x.o[0] = a[0] - O[0];
		x.o[1] = a[1] - O[1];
		x.o[2] = a[2];
		for ( int k = 0; k < 3; k++ )
			x.d[k] = d[k];
		x.tmax = tmax;
		x.pad = 0;
		return x;
	}
};

// ---------------------------------------------------------------- the lattice

constexpr int kOff = 1 << 20;

quint64 pack( int x, int y, int z )
{
	return quint64( x + kOff ) | ( quint64( y + kOff ) << 21 ) | ( quint64( z + kOff ) << 42 );
}
void unpack( quint64 k, int & x, int & y, int & z )
{
	x = int( k & 0x1FFFFF ) - kOff;
	y = int( ( k >> 21 ) & 0x1FFFFF ) - kOff;
	z = int( ( k >> 42 ) & 0x1FFFFF ) - kOff;
}
double jitter( int x, int y, int z, int axis )
{
	quint32 v = quint32( x ) * 73856093u ^ quint32( y ) * 19349663u ^ quint32( z ) * 83492791u ^ quint32( axis + 1 ) * 2654435761u;
	v ^= v >> 15;
	v *= 0x2c1b3c6du;
	v ^= v >> 12;
	v *= 0x297a2d39u;
	v ^= v >> 15;
	return double( v & 0xFFFF ) / 65535.0 - 0.5;   // -0.5 .. 0.5
}

struct Lat
{
	double h = 16;
	std::vector<quint64> key;
	std::vector<double> p;     // 3 per point
	std::vector<int> nb;       // 6 per point: +x -x +y -y +z -z (-1 none)
	int find( int x, int y, int z ) const
	{
		const quint64 k = pack( x, y, z );
		const auto it = std::lower_bound( key.begin(), key.end(), k );
		return it != key.end() && *it == k ? int( it - key.begin() ) : -1;
	}
	size_t size() const { return key.size(); }
	const double * at( int i ) const { return &p[size_t( i ) * 3]; }
};

// per point, from the rays
struct PointRays
{
	std::vector<quint8> blocked;   // bit a: the +a edge is cut by a triangle
	std::vector<quint8> sky, open, hits, back;
	std::vector<float> minT;
	std::vector<quint32> sig;      // a hash of every sphere answer (the GPU / CPU compare)
};

void castPoints( Tracer & T, const Lat & L, const std::vector<std::array<double, 3>> & dirs, double reach, PointRays * R,
	double * msEdges, double * msSphere, long long * nEdges, long long * nSphere, const ProbeVolOpenings * op )
{
	const size_t P = L.size(), K = dirs.size();
	R->blocked.assign( P, 0 );
	R->sky.assign( P, 0 );
	R->open.assign( P, 0 );
	R->hits.assign( P, 0 );
	R->back.assign( P, 0 );
	R->minT.assign( P, 1e30f );
	R->sig.assign( P, 0 );
	const size_t perChunk = std::max<size_t>( 1, size_t( 4000000 ) / ( K + 3 ) );
	std::vector<bakert::XRay> rays;
	std::vector<bakert::XHit> hits;
	std::vector<std::pair<int, int>> who;
	for ( size_t b = 0; b < P; b += perChunk ) {
		const size_t e = std::min( P, b + perChunk );
		// edges: is the straight segment to the +x/+y/+z neighbour free
		rays.clear();
		who.clear();
		for ( size_t i = b; i < e; i++ )
			for ( int a = 0; a < 3; a++ ) {
				const int j = L.nb[i * 6 + size_t( a ) * 2];
				if ( j < 0 )
					continue;
				const double * p = L.at( int( i ) );
				const double * q = L.at( j );
				double d[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
				const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
				for ( double & c : d )
					c /= len;
				rays.push_back( T.ray( p, d, len ) );
				who.push_back( { int( i ), a } );
			}
		const double ms0 = T.ms;
		T.cast( rays, hits, true );
		*msEdges += T.ms - ms0;
		*nEdges += qint64( rays.size() );
		for ( size_t r = 0; r < rays.size(); r++ )
			if ( hits[r].tri >= 0 )
				R->blocked[size_t( who[r].first )] |= quint8( 1 << who[r].second );
		// the sphere: K directions, nearest hits out to the world's end
		rays.clear();
		for ( size_t i = b; i < e; i++ )
			for ( size_t k = 0; k < K; k++ )
				rays.push_back( T.ray( L.at( int( i ) ), dirs[k].data(), 65536.0 ) );
		const double ms1 = T.ms;
		T.cast( rays, hits, false );
		*msSphere += T.ms - ms1;
		*nSphere += qint64( rays.size() );
		for ( size_t i = b; i < e; i++ ) {
			quint32 sig = 2166136261u;
			for ( size_t k = 0; k < K; k++ ) {
				const bakert::XHit & x = hits[( i - b ) * K + k];
				sig = ( sig ^ quint32( x.tri + 1 ) ) * 16777619u;
				if ( x.tri < 0 ) {
					if ( op && op->through( L.at( int( i ) ), dirs[k].data(), 1e300 ) )
						continue;   // sky through the room's own opening: light in, not outdoor air
					R->open[i]++;
					if ( dirs[k][2] > 0 )
						R->sky[i]++;
					continue;
				}
				sig = ( sig ^ quint32( std::lround( x.t * 64.0 ) ) ) * 16777619u;
				R->hits[i]++;
				if ( x.t > reach && !( op && op->through( L.at( int( i ) ), dirs[k].data(), x.t ) ) )
					R->open[i]++;
				R->minT[i] = std::min( R->minT[i], float( x.t ) );
				if ( T.back( x.tri, dirs[k].data() ) )
					R->back[i]++;
			}
			R->sig[i] = sig;
		}
	}
}

// ---------------------------------------------------------------- union-find

struct Dsu
{
	std::vector<int> p;
	explicit Dsu( size_t n ) : p( n ) { for ( size_t i = 0; i < n; i++ ) p[i] = int( i ); }
	int f( int x )
	{
		while ( p[size_t( x )] != x ) {
			p[size_t( x )] = p[size_t( p[size_t( x )] )];
			x = p[size_t( x )];
		}
		return x;
	}
	void u( int a, int b )
	{
		a = f( a );
		b = f( b );
		if ( a != b )
			p[size_t( std::max( a, b ) )] = std::min( a, b );
	}
};

struct RoomStat
{
	qint64 air = 0, leak = 0, gap = 0, gapReal = 0, blobs = 0, worstBlob = 0, solidReached = 0;
	qint64 gapOnce = 0, sharedReal = 0, sharedBlobs = 0;   // gapOnce: real gap points no earlier room counted
	qint64 poke = 0, pokeShallow = 0, overlap = 0, pokeOut = 0, spill = 0, sealed = 0, solidIn = 0;
	double pokeMax = 0;
	double blobAt[3] = { 0, 0, 0 }, pokeAt[3] = { 0, 0, 0 };
	bool seeded = false;
	QString seedNote;
};

struct Spot
{
	double severity = 0;
	QString kind, room;
	double at[3] = { 0, 0, 0 };
	QString text;
};

QString f1( double v ) { return QString::number( v, 'f', 1 ); }
QString f0( double v ) { return QString::number( v, 'f', 0 ); }
QString at3( const double p[3] ) { return QStringLiteral( "(%1, %2, %3)" ).arg( f0( p[0] ), f0( p[1] ), f0( p[2] ) ); }

}	// namespace

int probeVolCheckCli( const QStringList & args )
{
	QString soupPath, spacePath, outDir, raysWay = QStringLiteral( "auto" ), mutate, cellsOnly, pngPath, pointsPath;
	QString indoorRule = QStringLiteral( "openings" );   // lane HALLINDOOR1; "old" = VOLRAY1's rule (the red)
	double h = 16, margin = 128, tolPoke = 4, tolSkin = 8, tolGap = 4096, skyMax = 0.25, openMax = 0.25, reach = 1536;
	int K = 32;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--space" ) ) { spacePath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rays" ) ) { raysWay = nx; i++; }
		else if ( a == QLatin1String( "--mutate" ) ) { mutate = nx; i++; }
		else if ( a == QLatin1String( "--cells" ) ) { cellsOnly = nx; i++; }
		else if ( a == QLatin1String( "--png" ) ) { pngPath = nx; i++; }
		else if ( a == QLatin1String( "--points" ) ) { pointsPath = nx; i++; }
		else if ( a == QLatin1String( "--h" ) ) { h = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--margin" ) ) { margin = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--k" ) ) { K = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--tol-poke" ) ) { tolPoke = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--tol-skin" ) ) { tolSkin = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--tol-gap" ) ) { tolGap = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--skymax" ) ) { skyMax = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--openmax" ) ) { openMax = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--reach" ) ) { reach = nx.toDouble(); i++; }
		else if ( a == QLatin1String( "--indoor" ) ) { indoorRule = nx; i++; }
	}
	QTextStream o( stdout );
	if ( soupPath.isEmpty() || spacePath.isEmpty() || h <= 0 || K < 8 ) {
		o << "usage: volcheck --soup S --space <dir|file.space> [--out D] [--rays auto|cpu|gpu|both] [--h 16] [--margin 128] "
			"[--k 32] [--tol-poke 4] [--tol-skin 8] [--tol-gap 4096] [--cells k1,k2] [--mutate grow:N|shrink:N|shift:N|drop] "
			"[--png f] [--points f] [--reach 1536] [--indoor openings|old]\n";
		return 2;
	}
	QElapsedTimer total;
	total.start();
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		o << "volcheck: " << err << "\n";
		return 2;
	}

	// ---- the .space files: rooms and thresholds, names resolved across files
	QStringList files;
	if ( QFileInfo( spacePath ).isDir() ) {
		QDirIterator it( spacePath, QStringList { QStringLiteral( "*.space" ) }, QDir::Files, QDirIterator::Subdirectories );
		while ( it.hasNext() )
			files << it.next();
	} else {
		files << spacePath;
	}
	files.sort();
	std::vector<VRoom> rooms;
	std::vector<VThr> thrs;
	std::map<QString, int> byName;   // "cell:0xID" -> room
	struct PendingThr { VThr t; QString cell, na, nb; };
	std::vector<PendingThr> pend;
	const QStringList only = cellsOnly.isEmpty() ? QStringList() : cellsOnly.split( ',' );
	for ( const QString & fp : files ) {
		QFile f( fp );
		if ( !f.open( QIODevice::ReadOnly ) )
			continue;
		const QJsonObject root = QJsonDocument::fromJson( f.readAll() ).object();
		if ( root.value( QStringLiteral( "format" ) ).toString() != QLatin1String( "space" ) )
			continue;
		const QString cell = root.value( QStringLiteral( "cell" ) ).toString();
		for ( const QJsonValue & rv : root.value( QStringLiteral( "rooms" ) ).toArray() ) {
			const QJsonObject ro = rv.toObject();
			VRoom r;
			r.cell = cell;
			r.id = ro.value( QStringLiteral( "room" ) ).toString();
			r.source = ro.value( QStringLiteral( "source" ) ).toString();
			readV3( ro.value( QStringLiteral( "anchor" ) ), r.anchor );
			r.hand = ro.value( QStringLiteral( "fit" ) ).toObject().value( QStringLiteral( "hand" ) ).toBool();
			r.check = only.isEmpty() || only.contains( cell );
			for ( const QJsonValue & sv : ro.value( QStringLiteral( "shapes" ) ).toArray() ) {
				VShape s;
				if ( shapeOf( sv.toObject(), &s ) )
					r.shapes.push_back( s );
			}
			byName[r.name()] = int( rooms.size() );
			rooms.push_back( r );
		}
		for ( const QJsonValue & tv : root.value( QStringLiteral( "thresholds" ) ).toArray() ) {
			const QJsonObject to = tv.toObject();
			PendingThr p;
			p.cell = cell;
			p.t.cell = cell;
			p.t.opening = to.value( QStringLiteral( "opening" ) ).toString();
			if ( !shapeOf( to.value( QStringLiteral( "shape" ) ).toObject(), &p.t.box ) )
				continue;
			const QJsonArray bw = to.value( QStringLiteral( "between" ) ).toArray();
			p.na = bw.size() > 0 ? bw[0].toString() : QString();
			p.nb = bw.size() > 1 ? bw[1].toString() : QString();
			pend.push_back( p );
		}
	}
	auto resolve = [&]( const QString & cell, const QString & n ) -> int {
		if ( n == QLatin1String( "outside" ) )
			return -1;
		const QString full = n.contains( QLatin1Char( ':' ) ) ? n : cell + QLatin1Char( ':' ) + n;
		const auto it = byName.find( full );
		return it == byName.end() ? -2 : it->second;
	};
	for ( PendingThr & p : pend ) {
		p.t.a = resolve( p.cell, p.na );
		p.t.b = resolve( p.cell, p.nb );
		thrs.push_back( p.t );
	}
	if ( rooms.empty() ) {
		o << "volcheck: no rooms in " << spacePath << " (" << files.size() << " files)\n";
		return 2;
	}
	// the red: a deliberately broken fit must fail
	if ( !mutate.isEmpty() ) {
		const QStringList mv = mutate.split( ':' );
		const double n = mv.size() > 1 ? mv[1].toDouble() : 0;
		for ( VRoom & r : rooms ) {
			if ( mv[0] == QLatin1String( "drop" ) && r.shapes.size() > 1 ) {
				auto big = std::max_element( r.shapes.begin(), r.shapes.end(), []( const VShape & x, const VShape & y ) { return x.vol < y.vol; } );
				r.shapes.erase( big );
			}
			for ( VShape & s : r.shapes ) {
				if ( mv[0] == QLatin1String( "grow" ) )
					for ( double & v : s.h ) v += n;
				else if ( mv[0] == QLatin1String( "shrink" ) )
					for ( double & v : s.h ) v = std::max( 1.0, v - n );
				else if ( mv[0] == QLatin1String( "shift" ) )
					s.c[0] += n;
				s.finish();
			}
		}
	}
	for ( VRoom & r : rooms ) {
		r.vol = 0;
		for ( const VShape & s : r.shapes ) {
			r.vol += s.vol;
			for ( int k = 0; k < 3; k++ ) {
				r.lo[k] = std::min( r.lo[k], s.lo[k] );
				r.hi[k] = std::max( r.hi[k], s.hi[k] );
			}
		}
	}

	// ---- the tracer: the soup's triangles and its glass, local to the soup's middle (as the bake builds it)
	Tracer T;
	T.soup = &soup;
	T.nSoup = soup.tris.size() / 9;
	{
		double mn[2] = { 1e300, 1e300 }, mx[2] = { -1e300, -1e300 };
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 2; k++ ) {
				mn[k] = std::min( mn[k], double( soup.tris[i + size_t( k )] ) );
				mx[k] = std::max( mx[k], double( soup.tris[i + size_t( k )] ) );
			}
		if ( !soup.tris.empty() )
			for ( int k = 0; k < 2; k++ )
				T.O[k] = std::floor( ( mn[k] + mx[k] ) * 0.5 );
		T.bvh.t.reserve( soup.tris.size() + soup.glass.size() );
		for ( const std::vector<float> * src : { &soup.tris, &soup.glass } )
			for ( size_t i = 0; i < src->size(); i += 3 ) {
				T.bvh.t.push_back( float( double( ( *src )[i + 0] ) - T.O[0] ) );
				T.bvh.t.push_back( float( double( ( *src )[i + 1] ) - T.O[1] ) );
				T.bvh.t.push_back( ( *src )[i + 2] );
			}
		T.bvh.build();
		T.bvh.mask = &soup.amask;   // alpha-test holes let a ray through, as the bake's rays
	}

	// ---- the lattice over every checked room's bounds plus the margin
	Lat L;
	L.h = h;
	{
		std::vector<quint64> keys;
		for ( const VRoom & r : rooms ) {
			if ( !r.check || r.shapes.empty() )
				continue;
			int g0[3], g1[3];
			for ( int k = 0; k < 3; k++ ) {
				g0[k] = int( std::floor( ( r.lo[k] - margin ) / h ) );
				g1[k] = int( std::floor( ( r.hi[k] + margin ) / h ) );
			}
			const double n = double( g1[0] - g0[0] + 1 ) * double( g1[1] - g0[1] + 1 ) * double( g1[2] - g0[2] + 1 );
			if ( n > 6e7 ) {
				o << "volcheck: room " << r.name() << " spans " << f0( n ) << " lattice points at h " << h << ": raise --h\n";
				return 2;
			}
			for ( int z = g0[2]; z <= g1[2]; z++ )
				for ( int y = g0[1]; y <= g1[1]; y++ )
					for ( int x = g0[0]; x <= g1[0]; x++ )
						keys.push_back( pack( x, y, z ) );
			if ( keys.size() > size_t( 120000000 ) ) {
				std::sort( keys.begin(), keys.end() );
				keys.erase( std::unique( keys.begin(), keys.end() ), keys.end() );
			}
		}
		std::sort( keys.begin(), keys.end() );
		keys.erase( std::unique( keys.begin(), keys.end() ), keys.end() );
		L.key = std::move( keys );
	}
	const size_t P = L.size();
	if ( P == 0 || P > size_t( 60000000 ) ) {
		o << "volcheck: " << P << " lattice points at h " << h << " (none, or over 60 M): change --h or --cells\n";
		return 2;
	}
	L.p.resize( P * 3 );
	L.nb.assign( P * 6, -1 );
	for ( size_t i = 0; i < P; i++ ) {
		int x, y, z;
		unpack( L.key[i], x, y, z );
		const int g[3] = { x, y, z };
		for ( int k = 0; k < 3; k++ )
			L.p[i * 3 + size_t( k )] = ( g[k] + 0.5 + 0.5 * jitter( x, y, z, k ) ) * h;
		static const int dd[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for ( int n = 0; n < 6; n++ )
			L.nb[i * 6 + size_t( n )] = L.find( x + dd[n][0], y + dd[n][1], z + dd[n][2] );
	}

	// ---- rays: edges + the sphere, on the path asked for (both: GPU and CPU, every answer compared)
	std::vector<std::array<double, 3>> dirs( static_cast<size_t>( K ) );
	for ( int k = 0; k < K; k++ ) {
		const double z = 1.0 - ( 2.0 * k + 1.0 ) / K, rr = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
		const double ph = k * kPi * ( 3.0 - std::sqrt( 5.0 ) );
		dirs[size_t( k )] = { rr * std::cos( ph ), rr * std::sin( ph ), z };
	}
	int upper = 0;
	for ( const auto & d : dirs )
		upper += d[2] > 0 ? 1 : 0;
	const bool wantGpu = raysWay == QLatin1String( "gpu" ) || raysWay == QLatin1String( "both" );
	if ( wantGpu )
		qputenv( "WW_BAKE_RT", "1" );
	std::string why;
	const bool tryGpu = wantGpu || ( raysWay == QLatin1String( "auto" ) && bakert::pickPath( &why ) != bakert::Path::Cpu );
	QString pathLine = QStringLiteral( "cpu (%1 threads)" ).arg( bakert::threads() );
	if ( tryGpu ) {
		std::string log;
		T.scene = bakert::Scene::make( T.bvh, &log );
		T.gpu = T.scene && T.scene->path() != bakert::Path::Cpu;
		pathLine = T.gpu ? QStringLiteral( "%1 (%2)" ).arg( QLatin1String( bakert::pathName( T.scene->path() ) ),
			QString::fromStdString( bakert::deviceName() ) ) : QStringLiteral( "cpu (no GPU path: %1)" ).arg( QString::fromStdString( log ) );
	}
	// the indoor rule's openings: every window (glass: light passes, air does not) between a room and the outside;
	// doors and plain openings stay outdoor air
	ProbeVolOpenings openings;
	if ( indoorRule != QLatin1String( "old" ) ) {
		for ( const VThr & t : thrs )
			if ( ( t.a == -1 || t.b == -1 ) && t.opening == QLatin1String( "window" ) )
				openings.add( t.box.c, t.box.h, t.box.r );
		openings.build( reach );
	}
	PointRays R;
	double msE = 0, msS = 0;
	long long nE = 0, nS = 0;
	castPoints( T, L, dirs, reach, &R, &msE, &msS, &nE, &nS, &openings );
	QString compareLine;
	if ( raysWay == QLatin1String( "both" ) && T.gpu ) {
		const bool keep = T.gpu;
		T.gpu = false;
		PointRays C;
		double cE = 0, cS = 0;
		long long cnE = 0, cnS = 0;
		castPoints( T, L, dirs, reach, &C, &cE, &cS, &cnE, &cnS, &openings );
		qint64 unlikeE = 0, unlikeS = 0;
		for ( size_t i = 0; i < P; i++ ) {
			unlikeE += R.blocked[i] != C.blocked[i] ? 1 : 0;
			unlikeS += ( R.sig[i] != C.sig[i] || R.back[i] != C.back[i] ) ? 1 : 0;
		}
		compareLine = QStringLiteral( "volcheck: GPU vs CPU: rays %1 + %2 each way; GPU %3 ms + %4 ms, CPU %5 ms + %6 ms (edges + sphere); "
			"points whose edge answers differ %7, whose sphere answers differ %8 of %9" )
			.arg( nE ).arg( nS ).arg( f0( msE ) ).arg( f0( msS ) ).arg( f0( cE ) ).arg( f0( cS ) ).arg( unlikeE ).arg( unlikeS ).arg( P );
		R = std::move( C );   // the CPU walk is the reference the verdict reads
		T.gpu = keep;
	}

	// per point: solid (inside geometry: most rays meet a back face), enclosed (the fitter's indoor rule, by real rays)
	std::vector<quint8> solid( P, 0 ), enclosed( P, 0 );
	for ( size_t i = 0; i < P; i++ ) {
		solid[i] = R.hits[i] * 2 >= K && R.back[i] * 2 >= R.hits[i] ? 1 : 0;
		enclosed[i] = double( R.sky[i] ) <= skyMax * upper && double( R.open[i] ) <= openMax * K ? 1 : 0;
	}
	auto passable = [&]( size_t i, int n ) -> bool {   // the edge from i toward neighbour slot n, cut by no triangle
		const int j = L.nb[i * 6 + size_t( n )];
		if ( j < 0 )
			return false;
		return ( n & 1 ) ? !( R.blocked[size_t( j )] & ( 1 << ( n >> 1 ) ) ) : !( R.blocked[i] & ( 1 << ( n >> 1 ) ) );
	};
	// thresholds cut the edges they cross (a flood stops at an opening it names)
	std::vector<quint8> cut( P, 0 );   // bit a: the +a edge crosses a threshold
	for ( const VThr & t : thrs ) {
		int g0[3], g1[3];
		for ( int k = 0; k < 3; k++ ) {
			g0[k] = int( std::floor( t.box.lo[k] / h ) ) - 2;
			g1[k] = int( std::floor( t.box.hi[k] / h ) ) + 1;
		}
		for ( int z = g0[2]; z <= g1[2]; z++ )
			for ( int y = g0[1]; y <= g1[1]; y++ )
				for ( int x = g0[0]; x <= g1[0]; x++ ) {
					const int i = L.find( x, y, z );
					if ( i < 0 )
						continue;
					for ( int a = 0; a < 3; a++ ) {
						const int j = L.nb[size_t( i ) * 6 + size_t( a ) * 2];
						if ( j >= 0 && t.box.segBox( L.at( i ), L.at( j ) ) )
							cut[size_t( i )] |= quint8( 1 << a );
					}
				}
	}
	auto crosses = [&]( size_t i, int n ) -> bool {
		const int j = L.nb[i * 6 + size_t( n )];
		return ( n & 1 ) ? ( cut[size_t( j )] & ( 1 << ( n >> 1 ) ) ) : ( cut[i] & ( 1 << ( n >> 1 ) ) );
	};

	// ---- which rooms' shapes hold each point (CSR), and how many shapes (the volume pass's layers)
	const int NR = int( rooms.size() );
	std::vector<std::vector<int>> inRoom( static_cast<size_t>( NR ) );   // sorted point indexes
	std::vector<quint8> layers( P, 0 );
	for ( int ri = 0; ri < NR; ri++ ) {
		std::vector<int> & v = inRoom[size_t( ri )];
		for ( const VShape & s : rooms[size_t( ri )].shapes ) {
			int g0[3], g1[3];
			for ( int k = 0; k < 3; k++ ) {
				g0[k] = int( std::floor( s.lo[k] / h ) ) - 1;
				g1[k] = int( std::floor( s.hi[k] / h ) ) + 1;
			}
			for ( int z = g0[2]; z <= g1[2]; z++ )
				for ( int y = g0[1]; y <= g1[1]; y++ ) {
					const auto first = std::lower_bound( L.key.begin(), L.key.end(), pack( g0[0], y, z ) );
					const quint64 last = pack( g1[0], y, z );
					for ( auto it = first; it != L.key.end() && *it <= last; ++it ) {
						const int i = int( it - L.key.begin() );
						if ( s.in( L.at( i ) ) ) {
							v.push_back( i );
							if ( layers[size_t( i )] < 255 )
								layers[size_t( i )]++;
						}
					}
				}
		}
		std::sort( v.begin(), v.end() );
		v.erase( std::unique( v.begin(), v.end() ), v.end() );
	}
	std::vector<int> csrOff( P + 1, 0 ), csr;
	for ( int ri = 0; ri < NR; ri++ )
		for ( int i : inRoom[size_t( ri )] )
			csrOff[size_t( i ) + 1]++;
	for ( size_t i = 0; i < P; i++ )
		csrOff[i + 1] += csrOff[i];
	csr.resize( size_t( csrOff[P] ) );
	{
		std::vector<int> fill( csrOff.begin(), csrOff.end() - 1 );
		for ( int ri = 0; ri < NR; ri++ )
			for ( int i : inRoom[size_t( ri )] )
				csr[size_t( fill[size_t( i )]++ )] = ri;
	}
	auto holds = [&]( size_t i, int ri ) -> bool {
		for ( int k = csrOff[i]; k < csrOff[i + 1]; k++ )
			if ( csr[size_t( k )] == ri )
				return true;
		return false;
	};
	auto nRooms = [&]( size_t i ) -> int { return csrOff[i + 1] - csrOff[i]; };

	// ---- seeds: the lattice point nearest the anchor that sees it (one LOS ray each, nearest candidates first)
	std::vector<int> seed( size_t( NR ), -1 );
	{
		std::vector<bakert::XRay> rays;
		std::vector<bakert::XHit> hits;
		std::vector<std::pair<int, int>> who;
		for ( int ri = 0; ri < NR; ri++ ) {
			const VRoom & r = rooms[size_t( ri )];
			std::vector<std::pair<double, int>> cand;
			for ( int i : inRoom[size_t( ri )] ) {
				if ( solid[size_t( i )] )
					continue;
				const double * p = L.at( i );
				const double d2 = ( p[0] - r.anchor[0] ) * ( p[0] - r.anchor[0] ) + ( p[1] - r.anchor[1] ) * ( p[1] - r.anchor[1] )
					+ ( p[2] - r.anchor[2] ) * ( p[2] - r.anchor[2] );
				cand.push_back( { d2, i } );
			}
			const size_t m = std::min<size_t>( 12, cand.size() );
			std::partial_sort( cand.begin(), cand.begin() + qsizetype( m ), cand.end() );
			for ( size_t c = 0; c < m; c++ ) {
				const double * p = L.at( cand[c].second );
				double d[3] = { p[0] - r.anchor[0], p[1] - r.anchor[1], p[2] - r.anchor[2] };
				const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
				if ( len < 1e-6 ) {
					d[0] = 0; d[1] = 0; d[2] = 1;
				} else {
					for ( double & x : d ) x /= len;
				}
				rays.push_back( T.ray( r.anchor, d, std::max( len, 1e-3 ) ) );
				who.push_back( { ri, cand[c].second } );
			}
		}
		T.cast( rays, hits, true );
		for ( size_t k = 0; k < rays.size(); k++ )
			if ( hits[k].tri < 0 && seed[size_t( who[k].first )] < 0 )
				seed[size_t( who[k].first )] = who[k].second;
	}

	// ---- floods: each room from its seed, through open edges; stops at thresholds, outdoor air, other rooms, solid
	std::vector<RoomStat> st( static_cast<size_t>( NR ) );
	std::vector<std::vector<int>> reached( static_cast<size_t>( NR ) );   // sorted
	std::vector<std::vector<int>> gapPts( static_cast<size_t>( NR ) );
	std::vector<int> stamp( P, -1 );
	std::vector<qint32> reachCount( P, 0 );
	struct Pair { qint64 n = 0; double at[3] = { 0, 0, 0 }; };
	std::map<std::pair<int, int>, Pair> missing;
	for ( int ri = 0; ri < NR; ri++ ) {
		RoomStat & S = st[size_t( ri )];
		if ( seed[size_t( ri )] < 0 ) {
			S.seedNote = QStringLiteral( "no lattice point in its shapes sees the anchor" );
			continue;
		}
		S.seeded = true;
		std::vector<int> q { seed[size_t( ri )] };
		stamp[size_t( seed[size_t( ri )] )] = ri;
		std::vector<int> & got = reached[size_t( ri )];
		for ( size_t qi = 0; qi < q.size(); qi++ ) {
			const size_t u = size_t( q[qi] );
			got.push_back( int( u ) );
			const bool inI = holds( u, ri );
			if ( inI )
				( enclosed[u] ? S.air : S.leak )++;
			else
				gapPts[size_t( ri )].push_back( int( u ) ), S.gap++;
			for ( int n = 0; n < 6; n++ ) {
				if ( !passable( u, n ) || crosses( u, n ) )
					continue;
				const int v = L.nb[u * 6 + size_t( n )];
				if ( stamp[size_t( v )] == ri )
					continue;
				stamp[size_t( v )] = ri;
				if ( solid[size_t( v )] ) {
					S.solidReached++;
					continue;
				}
				const bool vIn = holds( size_t( v ), ri );
				if ( !vIn && nRooms( size_t( v ) ) > 0 ) {   // another room's air with no threshold between
					for ( int k = csrOff[size_t( v )]; k < csrOff[size_t( v ) + 1]; k++ ) {
						Pair & pr = missing[{ std::min( ri, csr[size_t( k )] ), std::max( ri, csr[size_t( k )] ) }];
						if ( pr.n++ == 0 )
							for ( int c = 0; c < 3; c++ )
								pr.at[c] = L.at( v )[c];
					}
					continue;
				}
				if ( !vIn && !enclosed[size_t( v )] )   // outdoor air: the room's open side
					continue;
				q.push_back( v );
			}
		}
		std::sort( got.begin(), got.end() );
		for ( int i : got )
			reachCount[size_t( i )]++;
	}

	// ---- a gate's look at one room's flood: WW_VOLCHECK_DUMP=<tsv> WW_VOLCHECK_DUMPROOM=<cell:0xID>
	// WW_VOLCHECK_DUMPBOX=x0,y0,z0,x1,y1,z1: every lattice point in the box, reached by that room, solid, enclosed, its shapes
	if ( qEnvironmentVariableIsSet( "WW_VOLCHECK_DUMP" ) ) {
		const QString want = qEnvironmentVariable( "WW_VOLCHECK_DUMPROOM" );
		const QStringList bx = qEnvironmentVariable( "WW_VOLCHECK_DUMPBOX" ).split( QLatin1Char( ',' ) );
		QFile df( qEnvironmentVariable( "WW_VOLCHECK_DUMP" ) );
		for ( int ri = 0; ri < NR && bx.size() == 6; ri++ ) {
			if ( rooms[size_t( ri )].name() != want || !df.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
				continue;
			QTextStream ds( &df );
			ds << "x\ty\tz\treached\tsolid\tenclosed\tin\trooms\tsky\thits\topen\n";
			const std::vector<int> & got = reached[size_t( ri )];
			for ( size_t i = 0; i < P; i++ ) {
				const double * p = L.at( i );
				bool in = true;
				for ( int k = 0; k < 3; k++ )
					in = in && p[k] >= bx[k].toDouble() && p[k] <= bx[3 + k].toDouble();
				if ( !in )
					continue;
				ds << f0( p[0] ) << '\t' << f0( p[1] ) << '\t' << f0( p[2] ) << '\t'
				   << int( std::binary_search( got.begin(), got.end(), int( i ) ) ) << '\t' << int( solid[i] ) << '\t'
				   << int( enclosed[i] ) << '\t' << int( holds( i, ri ) ) << '\t' << nRooms( i ) << '\t' << int( R.sky[i] ) << '\t'
				   << int( R.hits[i] ) << '\t' << int( R.open[i] ) << '\n';
			}
		}
	}

	// ---- the air's components (thresholds do not cut them: an opening is air)
	Dsu D( P );
	for ( size_t i = 0; i < P; i++ ) {
		if ( solid[i] )
			continue;
		for ( int n = 0; n < 6; n += 2 )
			if ( passable( i, n ) && !solid[size_t( L.nb[i * 6 + size_t( n )] )] )
				D.u( int( i ), L.nb[i * 6 + size_t( n )] );
	}
	std::vector<quint8> compOpen( P, 0 ), compReached( P, 0 );
	for ( size_t i = 0; i < P; i++ ) {
		if ( solid[i] )
			continue;
		const int c = D.f( int( i ) );
		if ( !enclosed[i] )
			compOpen[size_t( c )] = 1;
		if ( reachCount[i] > 0 )
			compReached[size_t( c )] = 1;
	}

	// ---- shape points the room's own flood never reached: sealed, solid, a spill through an opening or a wall poke
	struct PokeRay { int room, p, q; };
	std::vector<PokeRay> pk;
	std::vector<std::pair<PokeRay, double>> pokeDump;   // --points: every poke point, its room and depth
	for ( int ri = 0; ri < NR; ri++ ) {
		RoomStat & S = st[size_t( ri )];
		const std::vector<int> & mine = inRoom[size_t( ri )];
		const std::vector<int> & got = reached[size_t( ri )];
		// the nearest point of the room's reached air for every shape point (a BFS over the lattice, through walls)
		std::vector<int> src;
		std::vector<int> nearest;   // aligned with `mine`
		nearest.assign( mine.size(), -1 );
		std::map<int, size_t> pos;
		for ( size_t k = 0; k < mine.size(); k++ )
			pos[mine[k]] = k;
		std::vector<int> qq;
		for ( size_t k = 0; k < mine.size(); k++ )
			if ( std::binary_search( got.begin(), got.end(), mine[k] ) ) {
				nearest[k] = mine[k];
				qq.push_back( mine[k] );
			}
		for ( size_t qi = 0; qi < qq.size(); qi++ ) {
			const int u = qq[qi];
			const int from = nearest[pos[u]];
			for ( int n = 0; n < 6; n++ ) {
				const int v = L.nb[size_t( u ) * 6 + size_t( n )];
				if ( v < 0 )
					continue;
				const auto it = pos.find( v );
				if ( it == pos.end() || nearest[it->second] >= 0 )
					continue;
				nearest[it->second] = from;
				qq.push_back( v );
			}
		}
		for ( size_t k = 0; k < mine.size(); k++ ) {
			const int i = mine[k];
			if ( std::binary_search( got.begin(), got.end(), i ) )
				continue;
			if ( solid[size_t( i )] ) {   // inside geometry: no surface there reads the shape
				S.solidIn++;
				continue;
			}
			const int c = D.f( i );
			if ( !compOpen[size_t( c )] && !compReached[size_t( c )] ) {
				S.sealed++;
				continue;
			}
			pk.push_back( { ri, i, nearest[k] } );
		}
	}
	{
		std::vector<bakert::XRay> rays;
		std::vector<bakert::XHit> hits;
		std::vector<size_t> idx;
		for ( size_t k = 0; k < pk.size(); k++ ) {
			const double * p = L.at( pk[k].p );
			const double * q = pk[k].q >= 0 ? L.at( pk[k].q ) : rooms[size_t( pk[k].room )].anchor;
			double d[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
			const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
			if ( len < 1e-6 )
				continue;
			for ( double & x : d ) x /= len;
			rays.push_back( T.ray( p, d, len ) );
			idx.push_back( k );
		}
		T.cast( rays, hits, false );
		std::vector<std::vector<std::pair<double, int>>> pokes( static_cast<size_t>( NR ) );
		for ( size_t r = 0; r < rays.size(); r++ ) {
			const PokeRay & x = pk[idx[r]];
			RoomStat & S = st[size_t( x.room )];
			if ( hits[r].tri < 0 ) {
				S.spill++;
				continue;
			}
			const double depth = hits[r].t;
			if ( depth <= tolPoke ) {
				S.pokeShallow++;
				continue;
			}
			S.poke++;
			pokes[size_t( x.room )].push_back( { depth, x.p } );
			if ( !pointsPath.isEmpty() )
				pokeDump.push_back( { x, depth } );
			if ( nRooms( size_t( x.p ) ) > 1 )
				S.overlap++;
			if ( compOpen[size_t( D.f( x.p ) )] && reachCount[size_t( x.p )] == 0 )
				S.pokeOut++;
			if ( depth > S.pokeMax ) {
				S.pokeMax = depth;
				for ( int c = 0; c < 3; c++ )
					S.pokeAt[c] = L.at( x.p )[c];
			}
		}
	}

	// ---- gap blobs per room (real = more than tolSkin from every surface). A blob another room's flood also reaches
	// is air between rooms with no threshold (unclaimed): a missing room or threshold, not a face short of its wall.
	std::vector<quint8> counted( P, 0 );
	for ( int ri = 0; ri < NR; ri++ ) {
		RoomStat & S = st[size_t( ri )];
		std::vector<int> & g = gapPts[size_t( ri )];
		std::sort( g.begin(), g.end() );
		std::vector<quint8> done( g.size(), 0 );
		for ( size_t k = 0; k < g.size(); k++ ) {
			if ( done[k] )
				continue;
			std::vector<int> q { g[k] };
			done[k] = 1;
			qint64 real = 0;
			bool shared = false;
			double c[3] = { 0, 0, 0 };
			for ( size_t qi = 0; qi < q.size(); qi++ ) {
				const int u = q[qi];
				shared = shared || reachCount[size_t( u )] > 1;
				if ( R.minT[size_t( u )] >= tolSkin ) {
					real++;
					if ( !counted[size_t( u )] ) {
						counted[size_t( u )] = 1;
						S.gapOnce++;
					}
					for ( int a = 0; a < 3; a++ )
						c[a] += L.at( u )[a];
				}
				for ( int n = 0; n < 6; n++ ) {
					if ( !passable( size_t( u ), n ) )
						continue;
					const int v = L.nb[size_t( u ) * 6 + size_t( n )];
					const auto it = std::lower_bound( g.begin(), g.end(), v );
					if ( it == g.end() || *it != v || done[size_t( it - g.begin() )] )
						continue;
					done[size_t( it - g.begin() )] = 1;
					q.push_back( v );
				}
			}
			S.gapReal += real;
			if ( shared ) {
				S.sharedReal += real;
				if ( double( real ) * h * h * h > tolGap )
					S.sharedBlobs++;
				continue;
			}
			if ( double( real ) * h * h * h > tolGap )
				S.blobs++;
			if ( real > S.worstBlob ) {
				S.worstBlob = real;
				for ( int a = 0; a < 3; a++ )
					S.blobAt[a] = c[a] / double( real );
			}
		}
	}

	// ---- --points: the real gap points (with how many rooms' floods reach each) and the poke points, one per line
	if ( !pointsPath.isEmpty() ) {
		QFile pf( pointsPath );
		if ( pf.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			QTextStream ps( &pf );
			ps << "kind\tx\ty\tz\trooms\troom\tdepth\n";
			for ( size_t i = 0; i < P; i++ )
				if ( counted[i] ) {
					int first = -1;
					for ( int ri = 0; ri < NR && first < 0; ri++ )
						if ( std::binary_search( reached[size_t( ri )].begin(), reached[size_t( ri )].end(), int( i ) ) )
							first = ri;
					const double * p = L.at( int( i ) );
					ps << "gap\t" << f0( p[0] ) << "\t" << f0( p[1] ) << "\t" << f0( p[2] ) << "\t" << reachCount[i] << "\t"
						<< ( first >= 0 ? rooms[size_t( first )].name() : QString() ) << "\t" << f1( R.minT[i] ) << "\n";
				}
			for ( const auto & d : pokeDump ) {
				const double * p = L.at( d.first.p );
				ps << ( nRooms( size_t( d.first.p ) ) > 1 ? "overlap\t" : "poke\t" ) << f0( p[0] ) << "\t" << f0( p[1] ) << "\t" << f0( p[2] )
					<< "\t" << nRooms( size_t( d.first.p ) ) << "\t" << rooms[size_t( d.first.room )].name() << "\t" << f1( d.second ) << "\n";
			}
			for ( int ri = 0; ri < NR; ri++ )
				for ( int i : reached[size_t( ri )] )
					if ( holds( size_t( i ), ri ) && !enclosed[size_t( i )] ) {
						const double * p = L.at( i );
						ps << "leak\t" << f0( p[0] ) << "\t" << f0( p[1] ) << "\t" << f0( p[2] ) << "\t1\t" << rooms[size_t( ri )].name() << "\t0\n";
					}
		}
	}

	// ---- the report
	const double v3 = h * h * h;
	QDir().mkpath( outDir.isEmpty() ? QStringLiteral( "." ) : outDir );
	const QString od = outDir.isEmpty() ? QStringLiteral( "." ) : outDir;
	QFile tf( od + QStringLiteral( "/volcheck_rooms.tsv" ) );
	tf.open( QIODevice::WriteOnly | QIODevice::Truncate );
	QTextStream ts( &tf );
	ts << "cell\troom\tsource\tshapes\thand\tair_u3\tgap_u3\tgap_real_u3\tgap_pct\tblobs_over_tol\tworst_blob_u3\tworst_blob_at\t"
		"poke_u3\tpoke_max\tpoke_at\toverlap_u3\tpoke_outdoor_u3\tpoke_shallow_u3\tspill_u3\tleak_u3\tsealed_u3\tsolid_in_u3\tseed\n";
	struct Sum { qint64 air = 0, gap = 0, gapReal = 0, blobs = 0, poke = 0, overlap = 0, spill = 0, leak = 0, sealed = 0, solidIn = 0, pokeShallow = 0;
		qint64 gapOnce = 0, sharedReal = 0, sharedBlobs = 0;
		int rooms = 0, shapes = 0, maxShapes = 0; double pokeMax = 0; };
	std::map<QString, Sum> cells;
	Sum all;
	std::vector<Spot> spots;
	for ( int ri = 0; ri < NR; ri++ ) {
		const VRoom & r = rooms[size_t( ri )];
		if ( !r.check )
			continue;
		const RoomStat & S = st[size_t( ri )];
		const double roomAir = double( S.air + S.gap );
		ts << r.cell << '\t' << r.id << '\t' << r.source << '\t' << r.shapes.size() << '\t' << ( r.hand ? 1 : 0 ) << '\t'
			<< f0( S.air * v3 ) << '\t' << f0( S.gap * v3 ) << '\t' << f0( S.gapReal * v3 ) << '\t'
			<< ( roomAir > 0 ? QString::number( 100.0 * S.gapReal / roomAir, 'f', 2 ) : QStringLiteral( "-" ) ) << '\t'
			<< S.blobs << '\t' << f0( S.worstBlob * v3 ) << '\t' << ( S.worstBlob ? at3( S.blobAt ) : QString() ) << '\t'
			<< f0( S.poke * v3 ) << '\t' << f1( S.pokeMax ) << '\t' << ( S.poke ? at3( S.pokeAt ) : QString() ) << '\t'
			<< f0( S.overlap * v3 ) << '\t' << f0( S.pokeOut * v3 ) << '\t' << f0( S.pokeShallow * v3 ) << '\t' << f0( S.spill * v3 ) << '\t'
			<< f0( S.leak * v3 ) << '\t' << f0( S.sealed * v3 ) << '\t' << f0( S.solidIn * v3 ) << '\t'
			<< ( S.seeded ? QStringLiteral( "ok" ) : S.seedNote ) << '\n';
		for ( Sum * s : { &cells[r.cell], &all } ) {
			s->air += S.air; s->gap += S.gap; s->gapReal += S.gapReal; s->blobs += S.blobs; s->poke += S.poke;
			s->overlap += S.overlap; s->spill += S.spill; s->leak += S.leak; s->sealed += S.sealed; s->solidIn += S.solidIn;
			s->pokeShallow += S.pokeShallow;
			s->gapOnce += S.gapOnce; s->sharedReal += S.sharedReal; s->sharedBlobs += S.sharedBlobs;
			s->rooms++;
			s->shapes += int( r.shapes.size() );
			s->maxShapes = std::max( s->maxShapes, int( r.shapes.size() ) );
			s->pokeMax = std::max( s->pokeMax, S.pokeMax );
		}
		if ( S.blobs ) {
			Spot sp;
			sp.severity = S.worstBlob * v3;
			sp.kind = QStringLiteral( "gap" );
			sp.room = r.name();
			std::copy( S.blobAt, S.blobAt + 3, sp.at );
			sp.text = QStringLiteral( "worst blob %1 u3 (%2 blobs over tolerance, %3% of the room's air)" ).arg( f0( sp.severity ) )
				.arg( S.blobs ).arg( roomAir > 0 ? QString::number( 100.0 * S.gapReal / roomAir, 'f', 1 ) : QStringLiteral( "-" ) );
			spots.push_back( sp );
		}
		if ( S.poke ) {
			Spot sp;
			sp.severity = S.poke * v3 + S.pokeMax * 1e4;
			sp.kind = QStringLiteral( "poke" );
			sp.room = r.name();
			std::copy( S.pokeAt, S.pokeAt + 3, sp.at );
			sp.text = QStringLiteral( "%1 u3 past a wall, deepest %2 u (overlapping another room's shapes %3 u3)" ).arg( f0( S.poke * v3 ) )
				.arg( f1( S.pokeMax ) ).arg( f0( S.overlap * v3 ) );
			spots.push_back( sp );
		}
	}
	tf.close();
	for ( const auto & m : missing ) {
		const int a = m.first.first, b = m.first.second;
		if ( !rooms[size_t( a )].check && !rooms[size_t( b )].check )
			continue;
		bool named = false;
		for ( const VThr & t : thrs )
			if ( ( t.a == a && t.b == b ) || ( t.a == b && t.b == a ) )
				named = true;
		Spot sp;
		sp.severity = double( m.second.n ) * v3 * 0.001;
		sp.kind = named ? QStringLiteral( "around-threshold" ) : QStringLiteral( "no-threshold" );
		sp.room = rooms[size_t( a )].name() + QStringLiteral( " | " ) + rooms[size_t( b )].name();
		std::copy( m.second.at, m.second.at + 3, sp.at );
		sp.text = QStringLiteral( "%1 lattice steps pass between the two rooms' air %2" ).arg( m.second.n )
			.arg( named ? QStringLiteral( "around their threshold" ) : QStringLiteral( "with no threshold named between them" ) );
		spots.push_back( sp );
	}
	std::sort( spots.begin(), spots.end(), []( const Spot & x, const Spot & y ) { return x.severity > y.severity; } );
	QFile sf( od + QStringLiteral( "/volcheck_spots.tsv" ) );
	sf.open( QIODevice::WriteOnly | QIODevice::Truncate );
	QTextStream ss( &sf );
	ss << "kind\troom\tx\ty\tz\twhat\n";
	for ( const Spot & sp : spots )
		ss << sp.kind << '\t' << sp.room << '\t' << f0( sp.at[0] ) << '\t' << f0( sp.at[1] ) << '\t' << f0( sp.at[2] ) << '\t' << sp.text << '\n';
	sf.close();

	// the volume pass's cost: hulls (36 vertices a shape) and the mean layers of shapes over the covered air
	qint64 covered = 0, layerSum = 0;
	for ( size_t i = 0; i < P; i++ )
		if ( layers[i] ) {
			covered++;
			layerSum += layers[i];
		}
	int nShapes = 0, maxPerRoom = 0, over16 = 0;
	for ( const VRoom & r : rooms ) {
		nShapes += int( r.shapes.size() );
		maxPerRoom = std::max( maxPerRoom, int( r.shapes.size() ) );
		over16 += r.shapes.size() > 16 ? 1 : 0;
	}

	// a top-down picture: the worst thing in each column
	if ( !pngPath.isEmpty() ) {
		int x0 = 1 << 30, y0 = 1 << 30, x1 = -( 1 << 30 ), y1 = -( 1 << 30 );
		for ( size_t i = 0; i < P; i++ ) {
			int x, y, z;
			unpack( L.key[i], x, y, z );
			x0 = std::min( x0, x ); x1 = std::max( x1, x ); y0 = std::min( y0, y ); y1 = std::max( y1, y );
		}
		QImage img( x1 - x0 + 1, y1 - y0 + 1, QImage::Format_RGB32 );
		img.fill( QColor( 40, 40, 44 ) );
		std::vector<quint8> rank( size_t( img.width() ) * size_t( img.height() ), 0 );
		auto put = [&]( int i, int rk, QRgb c ) {
			int x, y, z;
			unpack( L.key[size_t( i )], x, y, z );
			const size_t k = size_t( y1 - y ) * size_t( img.width() ) + size_t( x - x0 );
			if ( rk > rank[k] ) {
				rank[k] = quint8( rk );
				img.setPixel( x - x0, y1 - y, c );
			}
		};
		for ( size_t i = 0; i < P; i++ )
			if ( solid[i] && layers[i] )
				put( int( i ), 1, qRgb( 110, 110, 110 ) );
		for ( int ri = 0; ri < NR; ri++ ) {
			const QColor hue = QColor::fromHsv( ( ri * 47 ) % 360, 120, 150 );
			for ( int i : reached[size_t( ri )] )
				if ( holds( size_t( i ), ri ) )
					put( i, enclosed[size_t( i )] ? 2 : 3, enclosed[size_t( i )] ? hue.rgb() : qRgb( 230, 220, 60 ) );
			for ( int i : gapPts[size_t( ri )] )
				if ( R.minT[size_t( i )] >= tolSkin )
					put( i, 4, qRgb( 255, 140, 0 ) );
		}
		for ( const PokeRay & x : pk ) {
			(void)x;
		}
		img.save( pngPath );
	}

	// ---- the census and the verdict
	const bool pass = all.blobs == 0 && all.sharedBlobs == 0 && all.poke == 0 && all.overlap == 0;
	o << "volcheck: soup " << ( soup.tris.size() / 9 ) << " triangles + " << ( soup.glass.size() / 9 ) << " glass; " << files.size()
		<< " .space files, rooms " << all.rooms << " checked of " << NR << ", thresholds " << thrs.size() << "; indoor rule "
		<< ( indoorRule == QLatin1String( "old" ) ? QStringLiteral( "old" ) : QStringLiteral( "openings %1" ).arg( openings.size() ) ) << "\n";
	o << "volcheck: lattice " << P << " points at h " << h << " (margin " << margin << "), rays " << T.rays << " on " << pathLine
		<< ": edges " << nE << " in " << f0( msE ) << " ms, sphere " << nS << " (K " << K << ") in " << f0( msS ) << " ms, total ray ms "
		<< f0( T.ms ) << "\n";
	if ( T.scene )
		o << "volcheck: " << QString::fromStdString( bakert::census( "volcheck", *T.scene ) ) << "\n";
	if ( !compareLine.isEmpty() )
		o << compareLine << "\n";
	for ( const auto & c : cells ) {
		const Sum & s = c.second;
		const double air = double( s.air + s.gapOnce );
		o << "volcheck: cell " << c.first << ": rooms " << s.rooms << ", shapes " << s.shapes << " (max " << s.maxShapes << " a room); air "
			<< f0( s.air * v3 ) << " u3; gaps real " << f0( s.gapOnce * v3 ) << " u3 ("
			<< ( air > 0 ? QString::number( 100.0 * s.gapOnce / air, 'f', 2 ) : QStringLiteral( "-" ) ) << "% of air + gaps, each point once), blobs over "
			<< f0( tolGap ) << " u3: " << s.blobs << " in one room, " << s.sharedBlobs << " unclaimed (" << f0( s.sharedReal * v3 )
			<< " u3 summed over rooms); wall pokes " << f0( s.poke * v3 ) << " u3 (deepest " << f1( s.pokeMax )
			<< " u), cross-wall overlaps " << f0( s.overlap * v3 ) << " u3; spills " << f0( s.spill * v3 ) << " u3, leaks "
			<< f0( s.leak * v3 ) << " u3, sealed " << f0( s.sealed * v3 ) << " u3, in solid " << f0( s.solidIn * v3 ) << " u3\n";
	}
	qint64 noThr = 0, aroundThr = 0;
	for ( const Spot & sp : spots ) {
		noThr += sp.kind == QLatin1String( "no-threshold" ) ? 1 : 0;
		aroundThr += sp.kind == QLatin1String( "around-threshold" ) ? 1 : 0;
	}
	o << "volcheck: room pairs whose air meets with no threshold named " << noThr << ", around a named threshold " << aroundThr << "\n";
	o << "volcheck: volume pass cost: shapes " << nShapes << " (" << ( nShapes * 36 ) << " hull vertices), max " << maxPerRoom
		<< " a room, rooms over 16: " << over16 << "; mean layers over the covered air "
		<< ( covered ? QString::number( double( layerSum ) / double( covered ), 'f', 2 ) : QStringLiteral( "-" ) ) << "\n";
	int shown = 0;
	for ( const Spot & sp : spots ) {
		if ( shown++ >= 12 )
			break;
		o << "volcheck: spot " << sp.kind << " " << sp.room << " at " << at3( sp.at ) << ": " << sp.text << "\n";
	}
	o << "volcheck: tolerances: gap blob > " << f0( tolGap ) << " u3 of air more than " << f1( tolSkin ) << " u from every surface; poke > "
		<< f1( tolPoke ) << " u past a surface" << ( mutate.isEmpty() ? QString() : QStringLiteral( "; MUTATED %1" ).arg( mutate ) ) << "\n";
	o << "volcheck: VERDICT " << ( pass ? "PASS" : "FAIL" ) << ": gap blobs " << all.blobs << ", unclaimed blobs " << all.sharedBlobs
		<< ", wall pokes " << f0( all.poke * v3 )
		<< " u3, cross-wall overlaps " << f0( all.overlap * v3 ) << " u3 (" << f0( double( total.elapsed() ) ) << " ms)\n";
	o.flush();
	return pass ? 0 : 1;
}
