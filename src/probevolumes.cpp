/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

// lane VOLFIT1: the Space stage (probevolumes.h)

#include "probevolumes.h"

#include "proberooms.h"
#include "gl/cellvolumeview.h"
#include "probevolumesgi.h"
#include "probegi.h"
#include "probebvh.h"
#include "probevolfine.h"
#include "bakert.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kSkyRays = 32, kWallRays = 16;
constexpr int kStageVersion = 2;   // 2: lane VOLRAY1 (ray refine, z clip)

void volParallel( size_t n, const std::function<void( size_t )> & fn )
{
	using std::thread;
	const int nThreads = std::max( 1, int( thread::hardware_concurrency() ) );
	std::atomic<size_t> next( 0 );
	std::vector<std::thread> pool;
	for ( int i = 0; i < nThreads; i++ )
		pool.emplace_back( [&]() {
			for ( size_t k; ( k = next.fetch_add( 1 ) ) < n; )
				fn( k );
		} );
	for ( std::thread & t : pool )
		t.join();
}

//! R = Rz Ry Rx (degrees), columns = the shape's local axes in the world
void rotMatrix( const float deg[3], double m[3][3] )
{
	const double ax = deg[0] * kPi / 180.0, ay = deg[1] * kPi / 180.0, az = deg[2] * kPi / 180.0;
	const double cx = std::cos( ax ), sx = std::sin( ax ), cy = std::cos( ay ), sy = std::sin( ay ), cz = std::cos( az ),
		sz = std::sin( az );
	// Rz * Ry * Rx
	m[0][0] = cz * cy; m[0][1] = cz * sy * sx - sz * cx; m[0][2] = cz * sy * cx + sz * sx;
	m[1][0] = sz * cy; m[1][1] = sz * sy * sx + cz * cx; m[1][2] = sz * sy * cx - cz * sx;
	m[2][0] = -sy;     m[2][1] = cy * sx;                m[2][2] = cy * cx;
}

const char * shapeTypeName( ProbeShapeType t )
{
	switch ( t ) {
	case ProbeShapeType::Box: return "box";
	case ProbeShapeType::Cylinder: return "cylinder";
	case ProbeShapeType::Wedge: return "wedge";
	case ProbeShapeType::Capsule: return "capsule";
	case ProbeShapeType::Sphere: return "sphere";
	}
	return "box";
}

bool shapeTypeFrom( const QString & s, ProbeShapeType * t )
{
	static const char * names[5] = { "box", "cylinder", "wedge", "capsule", "sphere" };
	for ( int i = 0; i < 5; i++ )
		if ( s == QLatin1String( names[i] ) ) {
			*t = ProbeShapeType( i );
			return true;
		}
	return false;
}

//! the shape's world AABB (its size box's corners)
void shapeBounds( const ProbeVolumeShape & s, double lo[3], double hi[3] )
{
	double m[3][3];
	rotMatrix( s.rotation, m );
	for ( int c = 0; c < 3; c++ ) {
		double e = 0;
		for ( int k = 0; k < 3; k++ )
			e += std::fabs( m[c][k] ) * s.size[k] * 0.5;
		lo[c] = s.center[c] - e;
		hi[c] = s.center[c] + e;
	}
}

QString hexId( quint32 id )
{
	return QStringLiteral( "0x%1" ).arg( id, 4, 16, QLatin1Char( '0' ) ).toUpper().replace( QLatin1String( "0X" ), QLatin1String( "0x" ) );
}

QJsonArray vec3( const float v[3] )
{
	QJsonArray a;
	for ( int c = 0; c < 3; c++ )
		a.append( std::round( double( v[c] ) * 10.0 ) / 10.0 );
	return a;
}

double r3( double v )
{
	return std::round( v * 1000.0 ) / 1000.0;
}

QJsonObject shapeJson( const ProbeVolumeShape & s )
{
	QJsonObject o;
	o.insert( QStringLiteral( "type" ), QLatin1String( shapeTypeName( s.type ) ) );
	o.insert( QStringLiteral( "center" ), vec3( s.center ) );
	o.insert( QStringLiteral( "size" ), vec3( s.size ) );
	o.insert( QStringLiteral( "rotation" ), vec3( s.rotation ) );
	return o;
}

bool readVec3( const QJsonValue & v, float out[3] )
{
	const QJsonArray a = v.toArray();
	if ( a.size() != 3 )
		return false;
	for ( int c = 0; c < 3; c++ )
		out[c] = float( a[c].toDouble() );
	return true;
}

//! a .recx / .space shape; false with a warning in `log`
bool shapeFromJson( const QJsonObject & o, ProbeVolumeShape * s, QStringList * log )
{
	const QString t = o.value( QStringLiteral( "type" ) ).toString();
	if ( !shapeTypeFrom( t, &s->type ) ) {
		QString near;
		for ( const char * n : { "box", "cylinder", "wedge", "capsule", "sphere" } ) {
			const QString nn = QLatin1String( n );
			int same = 0;
			for ( int i = 0; i < std::min( nn.size(), t.size() ); i++ )
				same += nn[i] == t[i] ? 1 : 0;
			if ( same * 2 >= nn.size() )
				near = nn;
		}
		if ( log )
			*log << ( near.isEmpty() ? QStringLiteral( "shape type \"%1\" unknown: skipped" ).arg( t )
				: QStringLiteral( "shape type \"%1\" unknown (did you mean \"%2\"?): skipped" ).arg( t, near ) );
		return false;
	}
	if ( !readVec3( o.value( QStringLiteral( "center" ) ), s->center ) || !readVec3( o.value( QStringLiteral( "size" ) ), s->size ) ) {
		if ( log )
			*log << QStringLiteral( "shape without center/size: skipped" );
		return false;
	}
	if ( !readVec3( o.value( QStringLiteral( "rotation" ) ), s->rotation ) )
		s->rotation[0] = s->rotation[1] = s->rotation[2] = 0.0f;
	return true;
}

void sha( QCryptographicHash & h, const void * p, size_t n )
{
	h.addData( QByteArrayView( reinterpret_cast<const char *>( p ), qsizetype( n ) ) );
}

}	// namespace

// ---------------------------------------------------------------- shapes

bool ProbeVolumeShape::contains( const float p[3] ) const
{
	double m[3][3];
	rotMatrix( rotation, m );
	const double d[3] = { double( p[0] ) - center[0], double( p[1] ) - center[1], double( p[2] ) - center[2] };
	double l[3];
	for ( int k = 0; k < 3; k++ )
		l[k] = m[0][k] * d[0] + m[1][k] * d[1] + m[2][k] * d[2];   // R^T d
	const double h[3] = { size[0] * 0.5, size[1] * 0.5, size[2] * 0.5 };
	switch ( type ) {
	case ProbeShapeType::Box:
		return std::fabs( l[0] ) <= h[0] && std::fabs( l[1] ) <= h[1] && std::fabs( l[2] ) <= h[2];
	case ProbeShapeType::Cylinder:
		return std::fabs( l[2] ) <= h[2] && h[0] > 0 && h[1] > 0
			&& ( l[0] / h[0] ) * ( l[0] / h[0] ) + ( l[1] / h[1] ) * ( l[1] / h[1] ) <= 1.0;
	case ProbeShapeType::Wedge:
		// full height at -y, zero at +y
		return std::fabs( l[0] ) <= h[0] && std::fabs( l[1] ) <= h[1] && l[2] >= -h[2] && size[1] > 0
			&& l[2] + h[2] <= size[2] * ( 0.5 - l[1] / size[1] );
	case ProbeShapeType::Capsule: {
		const double r = std::min( h[0], h[1] );
		const double zc = std::clamp( l[2], -std::max( 0.0, h[2] - r ), std::max( 0.0, h[2] - r ) );
		const double dz = l[2] - zc;
		return l[0] * l[0] + l[1] * l[1] + dz * dz <= r * r;
	}
	case ProbeShapeType::Sphere:
		return h[0] > 0 && h[1] > 0 && h[2] > 0
			&& ( l[0] / h[0] ) * ( l[0] / h[0] ) + ( l[1] / h[1] ) * ( l[1] / h[1] ) + ( l[2] / h[2] ) * ( l[2] / h[2] ) <= 1.0;
	}
	return false;
}

double ProbeVolumeShape::volume() const
{
	const double a = size[0], b = size[1], c = size[2];
	switch ( type ) {
	case ProbeShapeType::Box: return a * b * c;
	case ProbeShapeType::Cylinder: return kPi * a * b * 0.25 * c;
	case ProbeShapeType::Wedge: return a * b * c * 0.5;
	case ProbeShapeType::Capsule: {
		const double r = std::min( a, b ) * 0.5;
		return kPi * r * r * std::max( 0.0, c - 2 * r ) + 4.0 / 3.0 * kPi * r * r * r;
	}
	case ProbeShapeType::Sphere: return kPi / 6.0 * a * b * c;
	}
	return 0;
}

int ProbeVolumes::gridAt( const float p[3] ) const
{
	int g[3];
	for ( int c = 0; c < 3; c++ ) {
		g[c] = int( std::floor( ( p[c] - origin[c] ) / cell ) );
		if ( g[c] < 0 || g[c] >= dims[c] )
			return -2;
	}
	return labels.empty() ? -2 : labels[index( g[0], g[1], g[2] )];
}

int ProbeVolumes::roomAt( const float p[3] ) const
{
	int best = 0;
	double bestV = 1e300;
	for ( size_t r = 0; r < rooms.size(); r++ ) {
		double v = 0;
		bool in = false;
		for ( const ProbeVolumeShape & s : rooms[r].shapes ) {
			v += s.volume();
			in = in || s.contains( p );
		}
		if ( in && v < bestV ) {
			bestV = v;
			best = int( r ) + 1;
		}
	}
	return best;
}

// ---------------------------------------------------------------- the build

namespace {

struct Grid
{
	int X = 0, Y = 0, Z = 0;
	size_t idx( int x, int y, int z ) const { return ( size_t( z ) * size_t( Y ) + size_t( y ) ) * size_t( X ) + size_t( x ); }
	void xyz( size_t i, int & x, int & y, int & z ) const
	{
		x = int( i % size_t( X ) );
		y = int( ( i / size_t( X ) ) % size_t( Y ) );
		z = int( i / ( size_t( X ) * size_t( Y ) ) );
	}
	bool in( int x, int y, int z ) const { return x >= 0 && y >= 0 && z >= 0 && x < X && y < Y && z < Z; }
};

const int kNb[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

//! Amanatides-Woo through the coarse occupancy from p along d, the start cell skipped; true = a solid cell within maxT
bool ddaHit( const std::vector<quint8> & occ, const Grid & G, const float O[3], float C, const double p[3], const double d[3],
	double maxT )
{
	int g[3], step[3];
	double tMax[3], tDelta[3];
	for ( int c = 0; c < 3; c++ ) {
		const double u = ( p[c] - O[c] ) / C;
		g[c] = int( std::floor( u ) );
		if ( d[c] > 1e-12 ) {
			step[c] = 1;
			tMax[c] = ( g[c] + 1 - u ) * C / d[c];
			tDelta[c] = C / d[c];
		} else if ( d[c] < -1e-12 ) {
			step[c] = -1;
			tMax[c] = ( u - g[c] ) * C / -d[c];
			tDelta[c] = C / -d[c];
		} else {
			step[c] = 0;
			tMax[c] = 1e300;
			tDelta[c] = 1e300;
		}
	}
	if ( !G.in( g[0], g[1], g[2] ) )
		return false;
	for ( ;; ) {
		int a = 0;
		if ( tMax[1] < tMax[a] )
			a = 1;
		if ( tMax[2] < tMax[a] )
			a = 2;
		if ( tMax[a] > maxT )
			return false;
		g[a] += step[a];
		tMax[a] += tDelta[a];
		if ( !G.in( g[0], g[1], g[2] ) )
			return false;   // left the grid: escaped
		if ( occ[G.idx( g[0], g[1], g[2] )] )
			return true;
	}
}

//! the minimal-area rectangle's yaw (radians, 0 .. pi/2) of a point set (monotone-chain hull + rotating calipers)
double footprintYaw( std::vector<std::array<double, 2>> pts, double * areaAt0, double * areaBest )
{
	std::sort( pts.begin(), pts.end() );
	pts.erase( std::unique( pts.begin(), pts.end() ), pts.end() );
	auto cross = []( const std::array<double, 2> & o, const std::array<double, 2> & a, const std::array<double, 2> & b ) {
		return ( a[0] - o[0] ) * ( b[1] - o[1] ) - ( a[1] - o[1] ) * ( b[0] - o[0] );
	};
	std::vector<std::array<double, 2>> h( pts.size() * 2 );
	size_t k = 0;
	for ( size_t i = 0; i < pts.size(); i++ ) {
		while ( k >= 2 && cross( h[k - 2], h[k - 1], pts[i] ) <= 0 )
			k--;
		h[k++] = pts[i];
	}
	for ( size_t i = pts.size() - 1, t = k + 1; i-- > 0; ) {
		while ( k >= t && cross( h[k - 2], h[k - 1], pts[i] ) <= 0 )
			k--;
		h[k++] = pts[i];
	}
	h.resize( k > 1 ? k - 1 : k );
	auto areaAt = [&]( double th ) {
		const double c = std::cos( th ), s = std::sin( th );
		double lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };
		for ( const auto & p : h ) {
			const double u = c * p[0] + s * p[1], v = -s * p[0] + c * p[1];
			lo[0] = std::min( lo[0], u );
			hi[0] = std::max( hi[0], u );
			lo[1] = std::min( lo[1], v );
			hi[1] = std::max( hi[1], v );
		}
		return ( hi[0] - lo[0] ) * ( hi[1] - lo[1] );
	};
	*areaAt0 = areaAt( 0.0 );
	double best = *areaAt0, bestTh = 0.0;
	for ( size_t i = 0; i < h.size(); i++ ) {   // an edge of the hull lies on a side of the minimal rectangle
		const auto & a = h[i];
		const auto & b = h[( i + 1 ) % h.size()];
		double th = std::atan2( b[1] - a[1], b[0] - a[0] );
		th = std::fmod( th, kPi / 2 );
		if ( th < 0 )
			th += kPi / 2;
		const double ar = areaAt( th );
		if ( ar < best - 1e-9 ) {
			best = ar;
			bestTh = th;
		}
	}
	*areaBest = best;
	return bestTh;
}

}	// namespace

bool probeVolumesBuild( const ProbeSoup & soup, const std::vector<ProbeVolumeMarker> & markers,
	const std::vector<ProbeVolumeAperture> & apertures, const ProbeVolumeSpec & specIn, ProbeVolumes * out )
{
	ProbeVolumes & V = *out;
	V = ProbeVolumes();
	QElapsedTimer total, clock;
	total.start();
	ProbeVolumeSpec spec = specIn;
	const QString & red = spec.red;
	if ( red == QLatin1String( "nosky" ) )
		spec.skyMax = 2.0f;
	if ( red == QLatin1String( "nowall" ) )
		spec.wallMin = -1.0f;
	if ( red == QLatin1String( "noopen" ) )
		spec.openMax = 2.0f;
	if ( red == QLatin1String( "noback" ) )
		spec.backMax = 2.0f;
	if ( red == QLatin1String( "nogrow" ) )
		spec.growSteps = 0;
	if ( red == QLatin1String( "nofold" ) )
		spec.minVolume = 0.0f;
	if ( red == QLatin1String( "nomerge" ) )
		spec.mergeShare = 2.0f;
	const bool useFloor = red != QLatin1String( "nofloor" );

	// the door boxes' narrow side (what the pinch must close: half a door)
	{
		std::vector<double> w;
		for ( const ProbeSoup::Door & d : soup.doors )
			w.push_back( std::min( d.hi[0] - d.lo[0], d.hi[1] - d.lo[1] ) > 40.0f
				? std::max( double( d.hi[0] - d.lo[0] ), double( d.hi[1] - d.lo[1] ) ) : 0.0 );
		w.erase( std::remove( w.begin(), w.end(), 0.0 ), w.end() );
		if ( !w.empty() ) {
			std::nth_element( w.begin(), w.begin() + long( w.size() / 2 ), w.end() );
			V.doorWidth = w[w.size() / 2];
		}
	}

	// 1. the ROOMCLAMP1 grid, its working grids kept
	clock.start();
	ProbeRoomSpec rs;
	rs.cell = spec.cell;
	rs.maxCells = spec.maxCells;
	rs.pinch = spec.pinch;
	rs.keep = true;
	for ( int c = 0; c < 3; c++ ) {
		rs.clipLo[c] = spec.clipLo[c];
		rs.clipHi[c] = spec.clipHi[c];
	}
	ProbeRooms R;
	if ( !probeRoomsBuild( soup, rs, &R ) ) {
		V.error = R.error;
		return false;
	}
	probeGiRoomsOffer( soup, rs, R );   // lane GAPPERF1: GI and Space share one grid when clip and cell agree
	V.roomsLine = probeRoomsCensusText( R );
	V.msRooms = clock.nsecsElapsed() / 1e6;
	const float C = R.cell;
	V.cell = C;
	for ( int c = 0; c < 3; c++ ) {
		V.origin[c] = R.origin[c];
		V.dims[c] = R.dims[c];
	}
	Grid G { R.dims[0], R.dims[1], R.dims[2] };
	const size_t N = size_t( G.X ) * size_t( G.Y ) * size_t( G.Z );
	const std::vector<quint8> & st = R.state;
	std::vector<qint32> lab = std::move( R.labels );
	const std::vector<float> & d2 = R.dist2;
	bool openFace[6];
	for ( int f = 0; f < 6; f++ )
		openFace[f] = !( R.clippedFaces & ( 1 << f ) );

	// 2. roofed and floored, per column
	clock.start();
	std::vector<quint8> flag( N, 0 );   // 1 roofed, 2 floored
	volParallel( size_t( G.X ) * size_t( G.Y ), [&]( size_t col ) {
		const int x = int( col % size_t( G.X ) ), y = int( col / size_t( G.X ) );
		bool seen = false;
		for ( int z = 0; z < G.Z; z++ ) {
			const size_t i = G.idx( x, y, z );
			if ( st[i] )
				seen = true;
			else if ( seen )
				flag[i] |= 2;
		}
		seen = false;
		for ( int z = G.Z - 1; z >= 0; z-- ) {
			const size_t i = G.idx( x, y, z );
			if ( st[i] )
				seen = true;
			else if ( seen )
				flag[i] |= 1;
		}
	} );
	auto askSky = [&]( size_t i ) {
		return st[i] == 0 && lab[i] == 0 && ( flag[i] & 1 ) && ( !useFloor || ( flag[i] & 2 ) );
	};
	// the coarse occupancy and the sky / wall shares per coarse cell (fine cells read their parent's)
	Grid G2 { ( G.X + 1 ) / 2, ( G.Y + 1 ) / 2, ( G.Z + 1 ) / 2 };
	const size_t N2 = size_t( G2.X ) * size_t( G2.Y ) * size_t( G2.Z );
	std::vector<quint8> occ( N2, 0 ), want( N2, 0 );
	for ( size_t i = 0; i < N; i++ ) {
		int x, y, z;
		G.xyz( i, x, y, z );
		const size_t j = G2.idx( x / 2, y / 2, z / 2 );
		if ( st[i] )
			occ[j] = 1;
		if ( askSky( i ) )
			want[j] = 1;
	}
	std::vector<double> skyDir( kSkyRays * 3 ), wallDir( kWallRays * 3 );
	for ( int k = 0; k < kSkyRays; k++ ) {   // cosine-weighted: a Fibonacci disk lifted onto the hemisphere
		const double r = std::sqrt( ( k + 0.5 ) / kSkyRays ), ph = k * kPi * ( 3.0 - std::sqrt( 5.0 ) );
		skyDir[k * 3] = r * std::cos( ph );
		skyDir[k * 3 + 1] = r * std::sin( ph );
		skyDir[k * 3 + 2] = std::sqrt( std::max( 0.0, 1.0 - r * r ) );
	}
	for ( int k = 0; k < kWallRays; k++ ) {
		const double a = ( k + 0.5 ) * 2.0 * kPi / kWallRays;
		wallDir[k * 3] = std::cos( a );
		wallDir[k * 3 + 1] = std::sin( a );
		wallDir[k * 3 + 2] = 0.0;
	}
	std::vector<quint8> sky2( N2, 255 ), wall2( N2, 0 );
	std::vector<size_t> wantList;
	for ( size_t j = 0; j < N2; j++ )
		if ( want[j] )
			wantList.push_back( j );
	V.skyCoarse = qint64( wantList.size() );
	const float C2 = C * 2.0f;
	volParallel( wantList.size(), [&]( size_t w ) {
		const size_t j = wantList[w];
		int x, y, z;
		G2.xyz( j, x, y, z );
		const double p[3] = { V.origin[0] + ( x + 0.5 ) * C2, V.origin[1] + ( y + 0.5 ) * C2, V.origin[2] + ( z + 0.5 ) * C2 };
		int esc = 0, hit = 0;
		for ( int k = 0; k < kSkyRays; k++ )
			esc += ddaHit( occ, G2, V.origin, C2, p, &skyDir[size_t( k ) * 3], 1e30 ) ? 0 : 1;
		for ( int k = 0; k < kWallRays; k++ )
			hit += ddaHit( occ, G2, V.origin, C2, p, &wallDir[size_t( k ) * 3], spec.wallReach ) ? 1 : 0;
		sky2[j] = quint8( std::lround( 255.0 * esc / kSkyRays ) );
		wall2[j] = quint8( std::lround( 255.0 * hit / kWallRays ) );
	} );
	auto skyOf = [&]( size_t i ) {
		int x, y, z;
		G.xyz( i, x, y, z );
		return sky2[G2.idx( x / 2, y / 2, z / 2 )] / 255.0f;
	};
	auto wallOf = [&]( size_t i ) {
		int x, y, z;
		G.xyz( i, x, y, z );
		return wall2[G2.idx( x / 2, y / 2, z / 2 )] / 255.0f;
	};
	for ( size_t i = 0; i < N; i++ )
		V.skyCells += askSky( i ) ? 1 : 0;
	V.msSky = clock.nsecsElapsed() / 1e6;

	// 3. candidates in the outdoor air, their components judged as a whole
	clock.start();
	qint32 nextLabel = 0;
	for ( size_t i = 0; i < N; i++ )
		nextLabel = std::max( nextLabel, lab[i] );
	nextLabel++;
	std::vector<quint8> cand( N, 0 );
	for ( size_t i = 0; i < N; i++ )
		if ( askSky( i ) && skyOf( i ) < spec.skyMax && wallOf( i ) >= spec.wallMin )
			cand[i] = 1;
	// back share: a solid neighbor's one-sided faces pointing away from the air (seen from behind)
	auto faceSeen = [&]( size_t solid, int dir ) -> int {   // 1 back, -1 front, 0 unknown
		if ( R.facing.empty() || st[solid] != 1 )
			return 0;
		const qint8 * f = &R.facing[solid * 3];
		const int dot = f[0] * kNb[dir][0] + f[1] * kNb[dir][1] + f[2] * kNb[dir][2];
		return dot >= 8 ? 1 : dot <= -8 ? -1 : 0;
	};
	struct RoomInfo
	{
		qint64 cells = 0;
		double sky = 0, wall = 0;
		qint64 open = 0, closed = 0, back = 0, known = 0;
		QString source;
	};
	std::map<qint32, RoomInfo> info;
	{
		std::vector<qint32> comp( N, -1 );
		std::vector<size_t> stack, members;
		for ( size_t i0 = 0; i0 < N; i0++ ) {
			if ( !cand[i0] || comp[i0] >= 0 )
				continue;
			const qint32 cid = qint32( i0 % 0x7fffffff );
			members.clear();
			stack.assign( 1, i0 );
			comp[i0] = cid;
			RoomInfo ri;
			std::map<qint32, qint64> touch;   // faces shared with each room already labelled
			while ( !stack.empty() ) {
				const size_t i = stack.back();
				stack.pop_back();
				members.push_back( i );
				ri.cells++;
				ri.sky += skyOf( i );
				ri.wall += wallOf( i );
				int x, y, z;
				G.xyz( i, x, y, z );
				for ( int k = 0; k < 6; k++ ) {
					const int xx = x + kNb[k][0], yy = y + kNb[k][1], zz = z + kNb[k][2];
					if ( !G.in( xx, yy, zz ) ) {
						( openFace[k] ? ri.open : ri.closed )++;
						continue;
					}
					const size_t j = G.idx( xx, yy, zz );
					if ( cand[j] ) {
						if ( comp[j] < 0 ) {
							comp[j] = cid;
							stack.push_back( j );
						}
						continue;
					}
					if ( st[j] ) {
						ri.closed++;
						const int s = faceSeen( j, k );
						ri.back += s > 0 ? 1 : 0;
						ri.known += s != 0 ? 1 : 0;
					} else if ( lab[j] == 0 )
						ri.open++;
					else {
						ri.closed++;   // another room: a threshold
						touch[lab[j]]++;
					}
				}
			}
			V.candidates++;
			V.candidateCells += ri.cells;
			{   /* lane VOLFIT1: mostly wrapped by rooms = part of them (a floor layer the watershed gave the outdoors by
				 * a doorway), not a room: each cell goes to the nearest room through the air (a layer running on
				 * through a doorway into the next room is that room's there) */
				qint64 touchN = 0;
				for ( const auto & t : touch )
					touchN += t.first > 0 ? t.second : 0;
				if ( touchN > 0 && double( touchN ) >= spec.mergeShare * double( std::max<qint64>( 1, ri.open + ri.closed ) ) ) {
					std::vector<size_t> q;
					std::vector<std::pair<size_t, qint32>> got;
					for ( size_t i : members ) {
						int x, y, z;
						G.xyz( i, x, y, z );
						for ( int k = 0; k < 6; k++ ) {
							const int xx = x + kNb[k][0], yy = y + kNb[k][1], zz = z + kNb[k][2];
							if ( !G.in( xx, yy, zz ) )
								continue;
							const size_t j = G.idx( xx, yy, zz );
							if ( !st[j] && comp[j] != cid && lab[j] > 0 ) {
								got.push_back( { i, lab[j] } );
								break;
							}
						}
					}
					std::unordered_map<size_t, qint32> to;
					for ( const auto & g : got ) {
						to[g.first] = g.second;
						q.push_back( g.first );
					}
					for ( size_t h = 0; h < q.size(); h++ ) {
						const size_t i = q[h];
						int x, y, z;
						G.xyz( i, x, y, z );
						for ( int k = 0; k < 6; k++ ) {
							const int xx = x + kNb[k][0], yy = y + kNb[k][1], zz = z + kNb[k][2];
							if ( !G.in( xx, yy, zz ) )
								continue;
							const size_t j = G.idx( xx, yy, zz );
							if ( comp[j] == cid && !to.count( j ) ) {
								to[j] = to[i];
								q.push_back( j );
							}
						}
					}
					for ( size_t i : members ) {
						const qint32 l = to.count( i ) ? to[i] : 0;
						lab[i] = l;
						if ( l > 0 && info.count( l ) ) {
							info[l].cells++;
							info[l].sky += skyOf( i );
							info[l].wall += wallOf( i );
						}
					}
					V.merged++;
					continue;
				}
			}
			const double vol = double( ri.cells ) * C * C * C;
			const double skyMean = ri.sky / double( ri.cells );
			const double openness = double( ri.open ) / double( std::max<qint64>( 1, ri.open + ri.closed ) );
			const double back = ri.known ? double( ri.back ) / double( ri.known ) : 0.0;
			if ( vol < spec.minVolume ) {
				V.rejectedSmall++;
				continue;
			}
			if ( skyMean >= spec.skyMax ) {
				V.rejectedSky++;
				continue;
			}
			if ( openness >= spec.openMax ) {
				V.rejectedOpen++;
				continue;
			}
			if ( back >= spec.backMax ) {
				V.rejectedBack++;
				continue;
			}
			const qint32 l = nextLabel++;
			for ( size_t i : members )
				lab[i] = l;
			ri.source = QStringLiteral( "roofed" );
			info[l] = ri;
			V.roofedRooms++;
		}
	}
	// grow each roofed room back to its wall line through roofed air (a roof no higher than the room's top + 2)
	if ( spec.growSteps > 0 && !info.empty() ) {
		std::map<qint32, int> topZ;
		for ( size_t i = 0; i < N; i++ )
			if ( lab[i] > 0 && info.count( lab[i] ) ) {
				int x, y, z;
				G.xyz( i, x, y, z );
				int & t = topZ[lab[i]];
				t = std::max( t, z );
			}
		std::vector<size_t> front;
		for ( size_t i = 0; i < N; i++ )
			if ( lab[i] > 0 && info.count( lab[i] ) )
				front.push_back( i );
		for ( int s = 0; s < spec.growSteps && !front.empty(); s++ ) {
			std::vector<std::pair<size_t, qint32>> take;
			for ( size_t i : front ) {
				int x, y, z;
				G.xyz( i, x, y, z );
				for ( int k = 0; k < 6; k++ ) {
					const int xx = x + kNb[k][0], yy = y + kNb[k][1], zz = z + kNb[k][2];
					if ( !G.in( xx, yy, zz ) )
						continue;
					const size_t j = G.idx( xx, yy, zz );
					if ( st[j] || lab[j] != 0 || !( flag[j] & 1 ) || ( useFloor && !( flag[j] & 2 ) ) )
						continue;
					int rz = zz + 1;
					while ( rz < G.Z && !st[G.idx( xx, yy, rz )] )
						rz++;
					if ( rz > topZ[lab[i]] + 2 )
						continue;
					take.push_back( { j, lab[i] } );
				}
			}
			front.clear();
			for ( const auto & t : take )
				if ( lab[t.first] == 0 ) {
					lab[t.first] = t.second;
					front.push_back( t.first );
					V.grownCells++;
				}
		}
	}
	// the enclosed rooms (ROOMCLAMP1's labels): their back share; and every room's neighbors for the fold
	std::map<qint32, std::map<qint32, qint64>> nbFaces;
	{
		std::map<qint32, RoomInfo> enc;
		for ( size_t i = 0; i < N; i++ ) {
			const qint32 l = lab[i];
			if ( l <= 0 || st[i] )
				continue;
			const bool roofed = info.count( l ) > 0;
			RoomInfo & ri = roofed ? info[l] : enc[l];
			if ( !roofed )
				ri.cells++;
			int x, y, z;
			G.xyz( i, x, y, z );
			for ( int k = 0; k < 6; k++ ) {
				const int xx = x + kNb[k][0], yy = y + kNb[k][1], zz = z + kNb[k][2];
				if ( !G.in( xx, yy, zz ) )
					continue;
				const size_t j = G.idx( xx, yy, zz );
				if ( st[j] ) {
					if ( !roofed ) {
						const int s = faceSeen( j, k );
						ri.back += s > 0 ? 1 : 0;
						ri.known += s != 0 ? 1 : 0;
					}
				} else if ( lab[j] != l && lab[j] >= 0 )
					nbFaces[l][lab[j]]++;
			}
		}
		for ( auto & e : enc ) {
			const double back = e.second.known ? double( e.second.back ) / double( e.second.known ) : 0.0;
			if ( back >= spec.backMax ) {   // inside a rock: no room, no air
				V.insideGeometry++;
				e.second.source.clear();
				continue;
			}
			e.second.source = nbFaces.count( e.first ) ? QStringLiteral( "enclosed" ) : QStringLiteral( "sealed" );
			V.enclosed++;
			info[e.first] = e.second;
		}
		for ( size_t i = 0; i < N; i++ )
			if ( lab[i] > 0 && !info.count( lab[i] ) )
				lab[i] = -1;
	}
	// fold the rooms under minVolume, smallest first, into the neighbor sharing most faces (sealed: dropped)
	{
		std::map<qint32, qint32> into;
		std::vector<std::pair<qint64, qint32>> order;
		for ( const auto & e : info )
			order.push_back( { e.second.cells, e.first } );
		std::sort( order.begin(), order.end() );
		auto root = [&]( qint32 l ) {
			while ( into.count( l ) )
				l = into[l];
			return l;
		};
		std::map<qint32, qint64> cellsNow;
		for ( const auto & e : info )
			cellsNow[e.first] = e.second.cells;
		for ( const auto & o : order ) {
			const qint32 l = o.second;
			if ( double( cellsNow[l] ) * C * C * C >= spec.minVolume )
				continue;
			qint32 best = -1;
			qint64 bestN = 0;
			std::map<qint32, qint64> agg;
			for ( const auto & nb : nbFaces[l] )
				agg[root( nb.first )] += nb.second;
			for ( const auto & a : agg )
				if ( a.first != l && a.second > bestN ) {
					bestN = a.second;
					best = a.first;
				}
			into[l] = best;   // -1: dropped
			if ( best > 0 ) {
				cellsNow[best] += cellsNow[l];
				for ( const auto & nb : nbFaces[l] )
					if ( root( nb.first ) != best )
						nbFaces[best][nb.first] += nb.second;
				V.folded++;
			} else if ( best < 0 )
				V.dropped++;
			else
				V.folded++;   // into the outside
		}
		for ( size_t i = 0; i < N; i++ )
			if ( lab[i] > 0 && into.count( lab[i] ) )
				lab[i] = root( lab[i] );
		for ( const auto & f : into )
			info.erase( f.first );
	}
	// compact: rooms 1 .. n in label order
	std::map<qint32, int> roomOf;
	for ( const auto & e : info ) {
		roomOf[e.first] = int( V.rooms.size() ) + 1;
		ProbeVolumeRoom r;
		r.source = e.second.source;
		r.cells = 0;
		r.sky = e.second.source == QLatin1String( "roofed" ) ? float( e.second.sky / double( std::max<qint64>( 1, e.second.cells ) ) ) : 0.0f;
		r.wall = e.second.source == QLatin1String( "roofed" ) ? float( e.second.wall / double( std::max<qint64>( 1, e.second.cells ) ) ) : 1.0f;
		r.openness = float( double( e.second.open ) / double( std::max<qint64>( 1, e.second.open + e.second.closed ) ) );
		r.back = e.second.known ? float( double( e.second.back ) / double( e.second.known ) ) : 0.0f;
		if ( r.source == QLatin1String( "roofed" ) )
			r.borderline = std::fabs( r.sky - specIn.skyMax ) < spec.borderBand || std::fabs( r.openness - specIn.openMax ) < spec.borderBand;
		V.rooms.push_back( r );
	}
	V.labels.assign( N, -1 );
	for ( size_t i = 0; i < N; i++ ) {
		if ( st[i] )
			continue;
		const qint32 l = lab[i];
		V.labels[i] = l == 0 ? 0 : l > 0 && roomOf.count( l ) ? roomOf[l] : -1;
	}
	std::vector<std::array<int, 6>> bbox( V.rooms.size(), { { G.X, G.Y, G.Z, -1, -1, -1 } } );
	std::vector<size_t> deepest( V.rooms.size(), 0 );
	std::vector<float> deepD( V.rooms.size(), -1.0f );
	for ( size_t i = 0; i < N; i++ ) {
		const int r = V.labels[i];
		if ( r <= 0 )
			continue;
		ProbeVolumeRoom & rm = V.rooms[size_t( r - 1 )];
		rm.cells++;
		int x, y, z;
		G.xyz( i, x, y, z );
		auto & b = bbox[size_t( r - 1 )];
		b = { std::min( b[0], x ), std::min( b[1], y ), std::min( b[2], z ), std::max( b[3], x ), std::max( b[4], y ), std::max( b[5], z ) };
		if ( d2[i] > deepD[size_t( r - 1 )] ) {
			deepD[size_t( r - 1 )] = d2[i];
			deepest[size_t( r - 1 )] = i;
		}
	}
	for ( size_t r = 0; r < V.rooms.size(); r++ ) {
		int x, y, z;
		G.xyz( deepest[r], x, y, z );
		V.rooms[r].anchor[0] = V.origin[0] + ( x + 0.5f ) * C;
		V.rooms[r].anchor[1] = V.origin[1] + ( y + 0.5f ) * C;
		V.rooms[r].anchor[2] = V.origin[2] + ( z + 0.5f ) * C;
		for ( int c = 0; c < 3; c++ ) {
			V.rooms[r].lo[c] = V.origin[c] + bbox[r][c] * C;
			V.rooms[r].hi[c] = V.origin[c] + ( bbox[r][c + 3] + 1 ) * C;
		}
	}
	V.msClassify = clock.nsecsElapsed() / 1e6;

	// 4. thresholds: the faces between two labels and the glass between them, clustered per pair
	clock.start();
	struct Face
	{
		int a, b;      // a < b
		size_t cell;   // the a-side cell (or the glass cell)
		int axis;
		int sign;      // +1: from a to b along +axis
		bool glass;
		float pos[3];
	};
	std::vector<Face> faces;
	for ( size_t i = 0; i < N; i++ ) {
		const int la = V.labels[i];
		if ( la < 0 || st[i] )
			continue;
		int x, y, z;
		G.xyz( i, x, y, z );
		for ( int axis = 0; axis < 3; axis++ ) {
			const int xx = x + ( axis == 0 ), yy = y + ( axis == 1 ), zz = z + ( axis == 2 );
			if ( !G.in( xx, yy, zz ) )
				continue;
			const size_t j = G.idx( xx, yy, zz );
			const int lb = V.labels[j];
			if ( lb < 0 || lb == la || st[j] )
				continue;
			Face f;
			f.a = std::min( la, lb );
			f.b = std::max( la, lb );
			f.cell = la < lb ? i : j;
			f.axis = axis;
			f.sign = la < lb ? 1 : -1;
			f.glass = false;
			f.pos[0] = V.origin[0] + ( x + 0.5f + 0.5f * ( axis == 0 ) ) * C;
			f.pos[1] = V.origin[1] + ( y + 0.5f + 0.5f * ( axis == 1 ) ) * C;
			f.pos[2] = V.origin[2] + ( z + 0.5f + 0.5f * ( axis == 2 ) ) * C;
			faces.push_back( f );
		}
	}
	if ( red != QLatin1String( "noglass" ) )
		for ( size_t i = 0; i < N; i++ ) {
			if ( st[i] != 2 )
				continue;
			int x, y, z;
			G.xyz( i, x, y, z );
			for ( int axis = 0; axis < 3; axis++ ) {
				int side[2] = { -9, -9 };
				for ( int s = 0; s < 2; s++ ) {
					int g[3] = { x, y, z };
					for ( int k = 0; k < 3; k++ ) {
						g[axis] += s ? 1 : -1;
						if ( !G.in( g[0], g[1], g[2] ) )
							break;
						const size_t j = G.idx( g[0], g[1], g[2] );
						if ( st[j] == 1 )
							break;
						if ( st[j] == 0 ) {
							side[s] = V.labels[j];
							break;
						}
					}
				}
				if ( side[0] < 0 || side[1] < 0 || side[0] == side[1] )
					continue;
				Face f;
				f.a = std::min( side[0], side[1] );
				f.b = std::max( side[0], side[1] );
				f.cell = i;
				f.axis = axis;
				f.sign = side[0] < side[1] ? 1 : -1;
				f.glass = true;
				f.pos[0] = V.origin[0] + ( x + 0.5f ) * C;
				f.pos[1] = V.origin[1] + ( y + 0.5f ) * C;
				f.pos[2] = V.origin[2] + ( z + 0.5f ) * C;
				faces.push_back( f );
			}
		}
	{
		// union-find over faces of one pair whose cells are 26-neighbors
		std::vector<int> parent( faces.size() );
		for ( size_t k = 0; k < faces.size(); k++ )
			parent[k] = int( k );
		std::function<int( int )> find = [&]( int k ) {
			while ( parent[size_t( k )] != k ) {
				parent[size_t( k )] = parent[size_t( parent[size_t( k )] )];
				k = parent[size_t( k )];
			}
			return k;
		};
		std::unordered_map<quint64, std::vector<int>> at;   // (pair, cell) -> faces
		auto key = [&]( const Face & f, size_t cell ) {
			return ( quint64( quint32( f.a ) * 1000003u + quint32( f.b ) ) << 34 ) ^ quint64( cell );
		};
		for ( size_t k = 0; k < faces.size(); k++ )
			at[key( faces[k], faces[k].cell )].push_back( int( k ) );
		for ( size_t k = 0; k < faces.size(); k++ ) {
			int x, y, z;
			G.xyz( faces[k].cell, x, y, z );
			for ( int dz = -1; dz <= 1; dz++ )
				for ( int dy = -1; dy <= 1; dy++ )
					for ( int dx = -1; dx <= 1; dx++ ) {
						if ( !G.in( x + dx, y + dy, z + dz ) )
							continue;
						auto it = at.find( key( faces[k], G.idx( x + dx, y + dy, z + dz ) ) );
						if ( it == at.end() )
							continue;
						for ( int o : it->second )
							if ( faces[size_t( o )].a == faces[k].a && faces[size_t( o )].b == faces[k].b ) {
								const int ra = find( int( k ) ), rb = find( o );
								if ( ra != rb )
									parent[size_t( std::max( ra, rb ) )] = std::min( ra, rb );
							}
					}
		}
		std::map<int, std::vector<int>> groups;
		for ( size_t k = 0; k < faces.size(); k++ )
			groups[find( int( k ) )].push_back( int( k ) );
		for ( const auto & g : groups ) {
			const Face & f0 = faces[size_t( g.second.front() )];
			ProbeVolumeThreshold t;
			// a room first; b = the other room or the outside (0)
			const bool flip = f0.a == 0;
			t.a = flip ? f0.b : f0.a;
			t.b = flip ? 0 : f0.b;
			double n[3] = { 0, 0, 0 }, c[3] = { 0, 0, 0 };
			bool glass = false;
			for ( int k : g.second ) {
				const Face & f = faces[size_t( k )];
				n[f.axis] += f.sign * ( flip ? -1 : 1 );
				for ( int q = 0; q < 3; q++ )
					c[q] += f.pos[q];
				glass = glass || f.glass;
			}
			const double cnt = double( g.second.size() );
			for ( int q = 0; q < 3; q++ )
				c[q] /= cnt;
			double nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
			if ( nl < 1e-9 ) {
				n[2] = 1.0;
				nl = 1.0;
			}
			for ( int q = 0; q < 3; q++ ) {
				t.normal[q] = float( n[q] / nl );
				t.shape.center[q] = float( c[q] );
			}
			// the plane's frame: u along the opening (level unless the opening is a floor's), v up it
			double u[3], v[3];
			const bool level = std::fabs( t.normal[2] ) > 0.7;
			if ( !level ) {
				const double h = std::sqrt( double( t.normal[0] ) * t.normal[0] + double( t.normal[1] ) * t.normal[1] );
				u[0] = t.normal[1] / h;
				u[1] = -t.normal[0] / h;
				u[2] = 0;
			} else {
				// a hatch: u = the principal axis of its faces in xy
				double sxx = 0, syy = 0, sxy = 0;
				for ( int k : g.second ) {
					const double dx = faces[size_t( k )].pos[0] - c[0], dy = faces[size_t( k )].pos[1] - c[1];
					sxx += dx * dx;
					syy += dy * dy;
					sxy += dx * dy;
				}
				const double th = 0.5 * std::atan2( 2 * sxy, sxx - syy );
				u[0] = std::cos( th );
				u[1] = std::sin( th );
				u[2] = 0;
			}
			v[0] = t.normal[1] * u[2] - t.normal[2] * u[1];
			v[1] = t.normal[2] * u[0] - t.normal[0] * u[2];
			v[2] = t.normal[0] * u[1] - t.normal[1] * u[0];
			double ulo = 1e300, uhi = -1e300, vlo = 1e300, vhi = -1e300;
			for ( int k : g.second ) {
				const float * p = faces[size_t( k )].pos;
				const double du = ( p[0] - c[0] ) * u[0] + ( p[1] - c[1] ) * u[1] + ( p[2] - c[2] ) * u[2];
				const double dv = ( p[0] - c[0] ) * v[0] + ( p[1] - c[1] ) * v[1] + ( p[2] - c[2] ) * v[2];
				ulo = std::min( ulo, du );
				uhi = std::max( uhi, du );
				vlo = std::min( vlo, dv );
				vhi = std::max( vhi, dv );
			}
			const double mu = ( ulo + uhi ) * 0.5, mv = ( vlo + vhi ) * 0.5;
			for ( int q = 0; q < 3; q++ )
				t.shape.center[q] = float( c[q] + u[q] * mu + v[q] * mv );
			t.shape.type = ProbeShapeType::Box;
			t.shape.size[0] = float( uhi - ulo + C );
			t.shape.size[1] = C;
			t.shape.size[2] = float( vhi - vlo + C );
			// rotation: local y = the normal, local x = u (Rz yaw; a floor's opening: Rx +-90 first)
			if ( !level ) {
				t.shape.rotation[2] = float( std::atan2( u[1], u[0] ) * 180.0 / kPi );
			} else {
				t.shape.rotation[0] = t.normal[2] > 0 ? 90.0f : -90.0f;
				t.shape.rotation[2] = float( std::atan2( u[1], u[0] ) * 180.0 / kPi );
			}
			t.area = float( cnt * C * C );
			t.opening = glass ? QStringLiteral( "window" ) : QStringLiteral( "opening" );
			if ( g.second.size() <= 2 )
				V.tinyThresholds++;
			V.thresholds.push_back( t );
		}
	}
	V.msThresholds = clock.nsecsElapsed() / 1e6;

	// 5. evidence: door boxes, PortalMarkers, the placer's apertures; RoomMarkers for rooms
	if ( red != QLatin1String( "noevidence" ) ) {
		for ( ProbeVolumeThreshold & t : V.thresholds ) {
			const float * c = t.shape.center;
			for ( const ProbeSoup::Door & d : soup.doors ) {
				bool in = true;
				for ( int q = 0; q < 3; q++ )
					in = in && c[q] >= d.lo[q] - C && c[q] <= d.hi[q] + C;
				if ( in ) {
					t.evidence << QStringLiteral( "door:%1" ).arg( hexId( d.ref ) );
					if ( t.opening == QLatin1String( "opening" ) )
						t.opening = QStringLiteral( "door" );
				}
			}
			for ( const ProbeVolumeMarker & m : markers ) {
				if ( m.type != 2 )
					continue;
				bool in = true;
				for ( int a = 0; a < 3; a++ ) {
					const double l = ( c[0] - m.c[0] ) * m.ax[a][0] + ( c[1] - m.c[1] ) * m.ax[a][1] + ( c[2] - m.c[2] ) * m.ax[a][2];
					in = in && std::fabs( l ) <= m.half[a] + std::max( C, 32.0f );
				}
				if ( in ) {
					t.evidence << QStringLiteral( "portal:%1" ).arg( hexId( m.ref ) );
					if ( t.opening == QLatin1String( "opening" ) )
						t.opening = QStringLiteral( "portal" );
				}
			}
			for ( const ProbeVolumeAperture & a : apertures ) {
				const double dx = a.pos[0] - c[0], dy = a.pos[1] - c[1], dz = a.pos[2] - c[2];
				const double reach = std::max( a.width, a.height ) * 0.5 + C;
				if ( dx * dx + dy * dy + dz * dz <= reach * reach ) {
					static const char * names[4] = { "none", "doorway", "window", "breach" };
					t.evidence << QStringLiteral( "aperture:%1" ).arg( QLatin1String( names[std::clamp( a.type, 0, 3 )] ) );
				}
			}
			t.evidence.removeDuplicates();
		}
		for ( const ProbeVolumeMarker & m : markers ) {
			if ( m.type != 1 )
				continue;
			const int r = V.gridAt( m.c );
			if ( r > 0 )
				V.rooms[size_t( r - 1 )].evidence << QStringLiteral( "roomMarker:%1" ).arg( hexId( m.ref ) );
		}
	}
	for ( ProbeVolumeRoom & r : V.rooms )
		r.evidence.prepend( r.source );

	// 6. shapes
	clock.start();
	const bool redOneBox = red == QLatin1String( "onebox" );
	const bool useWedge = red != QLatin1String( "nowedge" ) && !redOneBox;
	const bool useAlt = red != QLatin1String( "boxonly" ) && !redOneBox;
	std::vector<std::vector<size_t>> cellsOf( V.rooms.size() );
	for ( size_t i = 0; i < N; i++ )
		if ( V.labels[i] > 0 )
			cellsOf[size_t( V.labels[i] - 1 )].push_back( i );
	// the measure on the grid: coverage, overshoot into other air (per room cell), into solid (per covered cell)
	auto measure = [&]( int room, const std::vector<ProbeVolumeShape> & shapes, float * cov, float * oa, float * os ) {
		double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
		for ( const ProbeVolumeShape & s : shapes ) {
			double a[3], b[3];
			shapeBounds( s, a, b );
			for ( int c = 0; c < 3; c++ ) {
				lo[c] = std::min( lo[c], a[c] );
				hi[c] = std::max( hi[c], b[c] );
			}
		}
		int g0[3], g1[3];
		for ( int c = 0; c < 3; c++ ) {
			g0[c] = std::max( 0, int( std::floor( ( lo[c] - V.origin[c] ) / C ) ) );
			g1[c] = std::min( V.dims[c] - 1, int( std::floor( ( hi[c] - V.origin[c] ) / C ) ) );
		}
		std::atomic<qint64> inRoom( 0 ), inOther( 0 ), inSolid( 0 ), inAll( 0 );
		if ( !shapes.empty() && g0[0] <= g1[0] && g0[1] <= g1[1] && g0[2] <= g1[2] )
			volParallel( size_t( g1[2] - g0[2] + 1 ), [&]( size_t zs ) {
				const int z = g0[2] + int( zs );
				qint64 a = 0, b = 0, s = 0, all = 0;
				for ( int y = g0[1]; y <= g1[1]; y++ )
					for ( int x = g0[0]; x <= g1[0]; x++ ) {
						const float p[3] = { V.origin[0] + ( x + 0.5f ) * C, V.origin[1] + ( y + 0.5f ) * C, V.origin[2] + ( z + 0.5f ) * C };
						bool in = false;
						for ( const ProbeVolumeShape & sh : shapes )
							if ( sh.contains( p ) ) {
								in = true;
								break;
							}
						if ( !in )
							continue;
						all++;
						const size_t i = V.index( x, y, z );
						if ( st[i] )
							s++;
						else if ( V.labels[i] == room )
							a++;
						else if ( V.labels[i] >= 0 )
							b++;
					}
				inRoom += a;
				inOther += b;
				inSolid += s;
				inAll += all;
			} );
		const double n = double( std::max<qint64>( 1, V.rooms[size_t( room - 1 )].cells ) );
		*cov = float( double( inRoom ) / n );
		*oa = float( double( inOther ) / n );
		*os = float( double( inSolid ) / double( std::max<qint64>( 1, inAll ) ) );
	};
	// lane VOLRAY1: a cell any triangle touches is solid, so every grown face stops about a cell short of the wall.
	// Rays from inside each face, along its normal, move it onto the real triangles (src/probevolcheck.cpp judges it).
	const bool refineOn = red != QLatin1String( "norefine" ) && !soup.tris.empty();
	// lane VOLFIT2: the fine lattice fit replaces the per-face refine (red "nofine" keeps the VOLRAY1 refine)
	const bool fineOn = refineOn && red != QLatin1String( "nofine" );
	// lane VOLFIT3: the fine fit moves the refined grid shapes on volcheck's lattice (red "volfit2": VOLFIT2's fit)
	const bool legacyFine = red == QLatin1String( "volfit2" );
	const double rC = double( C );
	probebvh::Bvh rb;
	double rO[2] = { 0, 0 };
	if ( refineOn ) {
		double mn[2] = { 1e300, 1e300 }, mx[2] = { -1e300, -1e300 };
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 2; k++ ) {
				mn[k] = std::min( mn[k], double( soup.tris[i + size_t( k )] ) );
				mx[k] = std::max( mx[k], double( soup.tris[i + size_t( k )] ) );
			}
		for ( int k = 0; k < 2; k++ )
			rO[k] = std::floor( ( mn[k] + mx[k] ) * 0.5 );
		rb.t.reserve( soup.tris.size() + soup.glass.size() );
		for ( const std::vector<float> * src : { &soup.tris, &soup.glass } )
			for ( size_t i = 0; i < src->size(); i += 3 ) {
				rb.t.push_back( float( double( ( *src )[i + 0] ) - rO[0] ) );
				rb.t.push_back( float( double( ( *src )[i + 1] ) - rO[1] ) );
				rb.t.push_back( ( *src )[i + 2] );
			}
		rb.build();
		rb.mask = &soup.amask;
	}
	std::atomic<qint64> refineRays( 0 );
	auto cast1 = [&]( const double p[3], const double d[3], double tmax ) -> double {
		const double o[3] = { p[0] - rO[0], p[1] - rO[1], p[2] };
		double t = 0;
		refineRays++;
		return rb.ray( o, d, tmax, &t ) ? t : HUGE_VAL;
	};
	struct RSample
	{
		double o[3], d[3], d0;
	};
	// the grid at a point: 1 the room's own air, 2 other air (another room, outdoors), 3 solid or off the grid
	auto gridKind = [&]( const double p[3], int room ) -> int {
		int g[3];
		for ( int c = 0; c < 3; c++ ) {
			g[c] = int( std::floor( ( p[c] - V.origin[c] ) / C ) );
			if ( g[c] < 0 || g[c] >= V.dims[c] )
				return 3;
		}
		const size_t i = V.index( g[0], g[1], g[2] );
		return st[i] ? 3 : V.labels[i] == room ? 1 : V.labels[i] >= 0 ? 2 : 3;
	};
	// free run past each sample's face point: the first triangle, or 1 unit into a sibling shape the ray enters; a
	// sample whose face already touches a sibling is inside the room's union and has no say (NaN). The grid guards
	// both ways: a face point in the room's own air never shrinks (a lamp or a beam inside the room is not its wall),
	// a grow stops where the grid turns to other air (a doorway), a shrink stops at the room's own air.
	auto freeRuns = [&]( const std::vector<RSample> & smp, const std::vector<const ProbeVolumeShape *> & sib, int room ) {
		std::vector<double> run( smp.size() );
		volParallel( ( smp.size() + 255 ) / 256, [&]( size_t blk ) {
			for ( size_t i = blk * 256; i < std::min( smp.size(), blk * 256 + 256 ); i++ ) {
				const RSample & q = smp[i];
				float f1[3];
				for ( int k = 0; k < 3; k++ )
					f1[k] = float( q.o[k] + q.d[k] * ( q.d0 + 1.0 ) );
				bool inner = false;
				for ( const ProbeVolumeShape * o : sib )
					inner = inner || o->contains( f1 );
				if ( inner ) {
					run[i] = std::nan( "" );
					continue;
				}
				double fp[3];
				for ( int k = 0; k < 3; k++ )
					fp[k] = q.o[k] + q.d[k] * q.d0;
				double r;
				if ( gridKind( fp, room ) == 1 ) {
					// cast from inside the shape: a face point on or just past a wall would miss that wall
					const double t = cast1( q.o, q.d, q.d0 + 2.0 * rC );
					r = std::isfinite( t ) ? std::max( 0.0, t - q.d0 ) : HUGE_VAL;
					// a grow ends inside the first solid cell: a ray that slips through a seam in that wall must not
					// carry the face into the air beyond it
					for ( double s = 0.5 * rC; s <= 2.0 * rC && s < r; s += 0.5 * rC ) {
						double p[3];
						for ( int k = 0; k < 3; k++ )
							p[k] = fp[k] + q.d[k] * s;
						const int g = gridKind( p, room );
						if ( g == 2 ) {
							r = std::max( 0.0, s - 0.5 * rC );
							break;
						}
						if ( g == 3 ) {
							r = std::min( r, s + 0.5 * rC );
							break;
						}
					}
				} else {
					// no room air behind the face point within reach: solid the shape encloses (a pillar, a beam), not
					// a wall the face went through, so the sample has no say
					const double t = cast1( q.o, q.d, q.d0 + 2.0 * rC );
					r = std::nan( "" );
					for ( double s = 0.5 * rC; s <= 4.0 * rC; s += 0.5 * rC ) {
						double p[3];
						for ( int k = 0; k < 3; k++ )
							p[k] = fp[k] - q.d[k] * s;
						if ( gridKind( p, room ) == 1 ) {
							r = std::max( std::isfinite( t ) ? t - q.d0 : HUGE_VAL, -s );
							break;
						}
					}
				}
				for ( double s = 2.0; s <= 2.0 * rC && s < r; s += 2.0 ) {
					float p[3];
					for ( int k = 0; k < 3; k++ )
						p[k] = float( q.o[k] + q.d[k] * ( q.d0 + s ) );
					bool in = false;
					for ( const ProbeVolumeShape * o : sib )
						in = in || o->contains( p );
					if ( in ) {
						r = s + 1.0;
						break;
					}
				}
				run[i] = r;
			}
		} );
		return run;
	};
	// the move that keeps 99 % of the samples with a say short of a triangle; 0 when too few have a say or most see
	// nothing in reach
	auto quantMove = [&]( const std::vector<double> & run, bool keepNeg ) -> double {
		std::vector<double> v;
		for ( double r : run )
			if ( !std::isnan( r ) )
				v.push_back( r );
		// a floor or ceiling face point already past a triangle is a poke whatever the share that sees it
		const bool pokes = keepNeg && std::count_if( v.begin(), v.end(), []( double r ) { return r < 0.0; } ) >= 2;
		if ( !pokes && ( v.size() < 4 || v.size() * 10 < run.size() ) )
			return 0.0;
		// a minority of samples whose face point is already past a triangle (a corner in a sloped roof) is that
		// corner's business, not this face's: they would hold the whole face off its wall
		const size_t neg = size_t( std::count_if( v.begin(), v.end(), []( double r ) { return r < 0.0; } ) );
		if ( !keepNeg && neg > 0 && neg * 2 < v.size() )
			v.erase( std::remove_if( v.begin(), v.end(), []( double r ) { return r < 0.0; } ), v.end() );
		std::sort( v.begin(), v.end() );
		const double q = v[v.size() / 100];
		if ( !std::isfinite( q ) )
			return 0.0;
		return std::clamp( q - 0.5, -3.0 * rC, 2.0 * rC );
	};
	auto refineShape = [&]( ProbeVolumeShape & s, const std::vector<const ProbeVolumeShape *> & sib, int room ) {
		const double step = 8.0, inset = 2.0;
		// floor and ceiling first, then the walls; 3 = a wedge's slope, 4 = a cylinder's side
		std::vector<std::array<int, 2>> fl;
		if ( s.type == ProbeShapeType::Box )
			fl = { { 2, -1 }, { 2, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 1 } };
		else if ( s.type == ProbeShapeType::Wedge )
			fl = { { 2, -1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 3, 0 } };
		else if ( s.type == ProbeShapeType::Cylinder && s.size[0] == s.size[1] )
			fl = { { 2, -1 }, { 2, 1 }, { 4, 0 } };
		for ( int pass = 0; pass < 2; pass++ )
			for ( const std::array<int, 2> & f : fl ) {
				double m[3][3];
				rotMatrix( s.rotation, m );
				const double h[3] = { s.size[0] * 0.5, s.size[1] * 0.5, s.size[2] * 0.5 };
				auto toWorld = [&]( const double l[3], double w[3], bool dirOnly ) {
					for ( int k = 0; k < 3; k++ )
						w[k] = ( dirOnly ? 0.0 : double( s.center[k] ) ) + m[k][0] * l[0] + m[k][1] * l[1] + m[k][2] * l[2];
				};
				auto toLocal = [&]( const double w[3], double l[3] ) {
					const double d[3] = { w[0] - s.center[0], w[1] - s.center[1], w[2] - s.center[2] };
					for ( int k = 0; k < 3; k++ )
						l[k] = m[0][k] * d[0] + m[1][k] * d[1] + m[2][k] * d[2];
				};
				auto insideBy = [&]( const double l[3], double e ) {
					switch ( s.type ) {
					case ProbeShapeType::Wedge:
						return std::fabs( l[0] ) <= h[0] - e && std::fabs( l[1] ) <= h[1] - e && l[2] >= -h[2] + e && h[1] > 0
							&& l[2] <= -h[2] * l[1] / h[1] - e;
					case ProbeShapeType::Cylinder:
						return std::fabs( l[2] ) <= h[2] - e && std::hypot( l[0], l[1] ) <= h[0] - e;
					default:
						return std::fabs( l[0] ) <= h[0] - e && std::fabs( l[1] ) <= h[1] - e && std::fabs( l[2] ) <= h[2] - e;
					}
				};
				std::vector<RSample> smp;
				if ( f[0] < 3 ) {
					const int a = f[0], b = ( a + 1 ) % 3, c = ( a + 2 ) % 3;
					const double sg = f[1], d0 = std::min( 3.0 * rC, h[a] );
					const double eb = h[b] - inset, ec = h[c] - inset;
					if ( eb <= 0 || ec <= 0 || h[a] <= 1.0 )
						continue;
					const int nb = std::max( 1, int( std::ceil( 2 * eb / step ) ) ), nc = std::max( 1, int( std::ceil( 2 * ec / step ) ) );
					double la[3] = { 0, 0, 0 };
					la[a] = sg;
					// the face's rim is sampled too: a corner is where a sloped roof first meets a ceiling
					for ( int jc = 0; jc <= nc; jc++ )
						for ( int ib = 0; ib <= nb; ib++ ) {
							double l[3];
							l[b] = -eb + ib * 2 * eb / nb;
							l[c] = -ec + jc * 2 * ec / nc;
							// a wall's top and bottom band meet the ceiling and floor (a sloped roof runs into the
							// wall there): the floor and ceiling faces own that corner
							if ( a != 2 && h[2] > 3.0 * rC && std::fabs( l[2] ) > h[2] - 2.0 * rC )
								continue;
							l[a] = sg * ( h[a] - inset );
							if ( !insideBy( l, 0.5 ) )
								continue;
							double dd = d0;
							l[a] = sg * ( h[a] - dd );
							while ( !insideBy( l, 0.5 ) && dd > 4.0 ) {
								dd *= 0.5;
								l[a] = sg * ( h[a] - dd );
							}
							if ( !insideBy( l, 0.5 ) )
								continue;
							RSample q;
							toWorld( l, q.o, false );
							toWorld( la, q.d, true );
							q.d0 = dd;
							smp.push_back( q );
						}
					// a box's floor or ceiling owns its corners; a wedge's floor corner at the thin edge is the slope's
					const double mv = quantMove( freeRuns( smp, sib, room ), a == 2 && s.type == ProbeShapeType::Box );
					if ( mv == 0.0 )
						continue;
					const double hn = std::max( 1.0, h[a] + mv * 0.5 ), dh = hn - h[a];
					s.size[a] = float( 2 * hn );
					for ( int k = 0; k < 3; k++ )
						s.center[k] = float( s.center[k] + sg * m[k][a] * dh );
				} else if ( f[0] == 3 ) {
					// the slope: a line through the hits (the roof's own pitch), shifted under 99 % of them
					if ( h[0] <= inset || h[1] <= inset || h[2] <= 1.0 )
						continue;
					const double nl = std::sqrt( h[2] * h[2] / ( h[1] * h[1] ) + 1.0 );
					const double n[3] = { 0, h[2] / h[1] / nl, 1.0 / nl };
					const int nx = std::max( 1, int( std::ceil( 2 * ( h[0] - inset ) / step ) ) );
					const int ny = std::max( 1, int( std::ceil( 2 * ( h[1] - inset ) / ( step * n[2] ) ) ) );
					for ( int j = 0; j < ny; j++ )
						for ( int i = 0; i < nx; i++ ) {
							const double x = -( h[0] - inset ) + ( i + 0.5 ) * 2 * ( h[0] - inset ) / nx;
							const double y = -( h[1] - inset ) + ( j + 0.5 ) * 2 * ( h[1] - inset ) / ny;
							const double p[3] = { x, y, -h[2] * y / h[1] };
							double d0 = 3.0 * rC, l[3];
							for ( ;; ) {
								for ( int k = 0; k < 3; k++ )
									l[k] = p[k] - n[k] * d0;
								if ( insideBy( l, 0.5 ) || d0 < 2.0 )
									break;
								d0 *= 0.5;
							}
							if ( d0 < 2.0 )
								continue;
							RSample q;
							toWorld( l, q.o, false );
							toWorld( n, q.d, true );
							q.d0 = d0;
							smp.push_back( q );
						}
					const std::vector<double> run = freeRuns( smp, sib, room );
					std::vector<std::array<double, 2>> hp;   // hits in the wedge's (y, z)
					size_t say = 0;
					for ( size_t i = 0; i < smp.size(); i++ ) {
						if ( std::isnan( run[i] ) )
							continue;
						say++;
						if ( !std::isfinite( run[i] ) )
							continue;
						double w[3], l[3];
						for ( int k = 0; k < 3; k++ )
							w[k] = smp[i].o[k] + smp[i].d[k] * ( smp[i].d0 + run[i] );
						toLocal( w, l );
						hp.push_back( { l[1], l[2] } );
					}
					if ( hp.size() < 8 || hp.size() * 2 < say )
						continue;
					double sy = 0, sz = 0, syy = 0, syz = 0;
					for ( const auto & q : hp ) {
						sy += q[0];
						sz += q[1];
						syy += q[0] * q[0];
						syz += q[0] * q[1];
					}
					const double nh = double( hp.size() ), den = nh * syy - sy * sy;
					if ( den <= 1e-9 )
						continue;
					const double B = ( nh * syz - sy * sz ) / den;
					double A = ( sz - B * sy ) / nh;
					if ( B >= -1e-3 )
						continue;   // no pitch down toward the thin edge: leave the slope
					std::vector<double> res;
					for ( const auto & q : hp )
						res.push_back( q[1] - ( A + B * q[0] ) );
					std::sort( res.begin(), res.end() );
					A += res[res.size() / 100] - 0.5;
					// the thin edge stops at the wall it runs into (a sliver under the tolerance may pass it)
					double wall = HUGE_VAL;
					for ( int i = 0; i < 3; i++ ) {
						const double l2[3] = { ( i - 1 ) * h[0] * 0.5, -h[1] + std::min( h[1], rC ), -h[2] + 2.0 };
						const double ly[3] = { 0, 1, 0 };
						double o[3], d[3];
						toWorld( l2, o, false );
						toWorld( ly, d, true );
						const double t = cast1( o, d, 2.0 * h[1] + 4.0 * rC );
						if ( std::isfinite( t ) )
							wall = std::min( wall, l2[1] + t );
					}
					const double ht = std::clamp( A - B * h[1] + h[2], std::max( 4.0, 2 * h[2] - 4 * rC ), 2 * h[2] + 4 * rC );
					double ly = ( -h[2] - A ) / B + h[1];
					ly = std::min( ly, wall + h[1] + 2.0 );
					ly = std::clamp( ly, std::max( 8.0, 2 * h[1] - 4 * rC ), 2 * h[1] + 4 * rC );
					const double lc2[3] = { 0, -h[1] + ly * 0.5, -h[2] + ht * 0.5 };
					double c2[3];
					toWorld( lc2, c2, false );
					s.size[1] = float( ly );
					s.size[2] = float( ht );
					for ( int k = 0; k < 3; k++ )
						s.center[k] = float( c2[k] );
				} else {
					const double r0 = h[0], d0 = std::min( 3.0 * rC, r0 );
					if ( r0 <= 1.0 || h[2] <= inset )
						continue;
					const int na = std::max( 8, int( std::ceil( 2 * kPi * r0 / step ) ) );
					const int nz = std::max( 1, int( std::ceil( 2 * ( h[2] - inset ) / step ) ) );
					for ( int j = 0; j < nz; j++ )
						for ( int i = 0; i < na; i++ ) {
							const double ph = 2 * kPi * ( i + 0.5 ) / na;
							const double dl[3] = { std::cos( ph ), std::sin( ph ), 0.0 };
							const double l[3] = { ( r0 - d0 ) * dl[0], ( r0 - d0 ) * dl[1], -( h[2] - inset ) + ( j + 0.5 ) * 2 * ( h[2] - inset ) / nz };
							RSample q;
							toWorld( l, q.o, false );
							toWorld( dl, q.d, true );
							q.d0 = d0;
							smp.push_back( q );
						}
					const double mv = quantMove( freeRuns( smp, sib, room ), false );
					if ( mv == 0.0 )
						continue;
					s.size[0] = s.size[1] = float( 2 * std::max( 1.0, r0 + mv ) );
				}
			}
	};
	std::vector<double> roomYaw( V.rooms.size(), 0.0 );
	std::vector<char> keepGrid( V.rooms.size(), 0 );
	for ( size_t r = 0; r < V.rooms.size(); r++ ) {
		ProbeVolumeRoom & rm = V.rooms[r];
		const int room = int( r ) + 1;
		const std::vector<size_t> & cells = cellsOf[r];
		if ( cells.empty() )
			continue;
		// the footprint frame
		std::vector<std::array<double, 2>> pts;
		{
			std::vector<quint64> cols;
			for ( size_t i : cells ) {
				int x, y, z;
				G.xyz( i, x, y, z );
				cols.push_back( ( quint64( quint32( x ) ) << 32 ) | quint32( y ) );
			}
			std::sort( cols.begin(), cols.end() );
			cols.erase( std::unique( cols.begin(), cols.end() ), cols.end() );
			for ( quint64 k : cols ) {
				const double x = double( k >> 32 ), y = double( k & 0xffffffffu );
				for ( int q = 0; q < 4; q++ )
					pts.push_back( { V.origin[0] + ( x + ( q & 1 ) ) * C, V.origin[1] + ( y + ( q >> 1 ) ) * C } );
			}
		}
		double a0 = 0, aBest = 0;
		double th = footprintYaw( pts, &a0, &aBest );
		if ( a0 <= aBest * 1.03 || std::fabs( th ) < 2.0 * kPi / 180.0 || std::fabs( th - kPi / 2 ) < 2.0 * kPi / 180.0 )
			th = 0.0;   // the grid's own axes unless the turn saves over 3 % (rooms are mostly square to the world)
		roomYaw[r] = th;
		const double ct = std::cos( th ), sn = std::sin( th );
		// the local grid (u, v, z) at the room's cell size: 1 room, 2 other air, 3 solid / outside
		double ulo = 1e300, uhi = -1e300, vlo = 1e300, vhi = -1e300;
		for ( const auto & p : pts ) {
			const double u = ct * p[0] + sn * p[1], v = -sn * p[0] + ct * p[1];
			ulo = std::min( ulo, u );
			uhi = std::max( uhi, u );
			vlo = std::min( vlo, v );
			vhi = std::max( vhi, v );
		}
		int z0 = G.Z, z1 = -1;
		for ( size_t i : cells ) {
			int x, y, z;
			G.xyz( i, x, y, z );
			z0 = std::min( z0, z );
			z1 = std::max( z1, z );
		}
		Grid L { std::max( 1, int( std::lround( ( uhi - ulo ) / C ) ) ), std::max( 1, int( std::lround( ( vhi - vlo ) / C ) ) ), z1 - z0 + 1 };
		const size_t NL = size_t( L.X ) * size_t( L.Y ) * size_t( L.Z );
		std::vector<quint8> lc( NL, 3 );
		std::vector<float> ld( NL, 0.0f );
		auto worldOf = [&]( double i, double j, double k, double w[3] ) {   // local cell coordinates -> world
			const double u = ulo + i * C, v = vlo + j * C;
			w[0] = ct * u - sn * v;
			w[1] = sn * u + ct * v;
			w[2] = V.origin[2] + ( z0 + k ) * C;
		};
		qint64 roomN = 0;
		for ( int k = 0; k < L.Z; k++ )
			for ( int j = 0; j < L.Y; j++ )
				for ( int i = 0; i < L.X; i++ ) {
					double w[3];
					worldOf( i + 0.5, j + 0.5, k + 0.5, w );
					int g[3];
					for ( int c = 0; c < 3; c++ )
						g[c] = int( std::floor( ( w[c] - V.origin[c] ) / C ) );
					if ( !G.in( g[0], g[1], g[2] ) )
						continue;
					const size_t gi = G.idx( g[0], g[1], g[2] );
					const size_t li = L.idx( i, j, k );
					lc[li] = st[gi] ? 3 : V.labels[gi] == room ? 1 : V.labels[gi] >= 0 ? 2 : 3;
					ld[li] = d2[gi];
					roomN += lc[li] == 1;
				}
		if ( !roomN )
			continue;
		// summed-volume tables
		auto table = [&]( const std::function<bool( size_t )> & f ) {
			std::vector<qint32> s( size_t( L.X + 1 ) * size_t( L.Y + 1 ) * size_t( L.Z + 1 ), 0 );
			auto at = [&]( int x, int y, int z ) -> qint32 & { return s[( size_t( z ) * size_t( L.Y + 1 ) + size_t( y ) ) * size_t( L.X + 1 ) + size_t( x )]; };
			for ( int z = 1; z <= L.Z; z++ )
				for ( int y = 1; y <= L.Y; y++ )
					for ( int x = 1; x <= L.X; x++ )
						at( x, y, z ) = ( f( L.idx( x - 1, y - 1, z - 1 ) ) ? 1 : 0 ) + at( x - 1, y, z ) + at( x, y - 1, z ) + at( x, y, z - 1 )
							- at( x - 1, y - 1, z ) - at( x - 1, y, z - 1 ) - at( x, y - 1, z - 1 ) + at( x - 1, y - 1, z - 1 );
			return s;
		};
		auto sum = [&]( const std::vector<qint32> & s, const int lo[3], const int hi[3] ) -> qint64 {
			auto at = [&]( int x, int y, int z ) { return qint64( s[( size_t( z ) * size_t( L.Y + 1 ) + size_t( y ) ) * size_t( L.X + 1 ) + size_t( x )] ); };
			const int a[3] = { lo[0], lo[1], lo[2] }, b[3] = { hi[0] + 1, hi[1] + 1, hi[2] + 1 };
			return at( b[0], b[1], b[2] ) - at( a[0], b[1], b[2] ) - at( b[0], a[1], b[2] ) - at( b[0], b[1], a[2] ) + at( a[0], a[1], b[2] )
				+ at( a[0], b[1], a[2] ) + at( b[0], a[1], a[2] ) - at( a[0], a[1], a[2] );
		};
		const std::vector<qint32> tRoom = table( [&]( size_t i ) { return lc[i] == 1; } );
		const std::vector<qint32> tOther = table( [&]( size_t i ) { return lc[i] == 2; } );
		const std::vector<qint32> tSolid = table( [&]( size_t i ) { return lc[i] == 3; } );
		std::vector<quint8> covered( NL, 0 );
		auto boxShape = [&]( const int lo[3], const int hi[3] ) {
			ProbeVolumeShape s;
			s.type = ProbeShapeType::Box;
			double w[3];
			worldOf( ( lo[0] + hi[0] + 1 ) * 0.5, ( lo[1] + hi[1] + 1 ) * 0.5, ( lo[2] + hi[2] + 1 ) * 0.5, w );
			for ( int c = 0; c < 3; c++ ) {
				s.center[c] = float( w[c] );
				s.size[c] = float( ( hi[c] - lo[c] + 1 ) * C );
			}
			s.rotation[2] = float( th * 180.0 / kPi );
			return s;
		};
		// a wedge over [lo, hi] (local cells), high side along axis a at the low end (hiAtMin) or the high end
		auto wedgeIn = [&]( const int lo[3], const int hi[3], int a, bool hiAtMin, int i, int j, int k ) {
			const int n = hi[a] - lo[a] + 1;
			const int q = a == 0 ? i : j;
			const double t = hiAtMin ? ( q - lo[a] + 0.5 ) / n : ( hi[a] - q + 0.5 ) / n;
			return ( k - lo[2] + 0.5 ) <= ( 1.0 - t ) * ( hi[2] - lo[2] + 1 );
		};
		auto wedgeShape = [&]( const int lo[3], const int hi[3], int a, bool hiAtMin ) {
			ProbeVolumeShape s = boxShape( lo, hi );
			s.type = ProbeShapeType::Wedge;
			const double eu = ( hi[0] - lo[0] + 1 ) * C, ev = ( hi[1] - lo[1] + 1 ) * C;
			double phi;   // the turn in the room frame that points the wedge's -y at its high side
			if ( a == 1 ) {
				phi = hiAtMin ? 0.0 : 180.0;
				s.size[0] = float( eu );
				s.size[1] = float( ev );
			} else {
				phi = hiAtMin ? -90.0 : 90.0;
				s.size[0] = float( ev );
				s.size[1] = float( eu );
			}
			s.rotation[2] = float( th * 180.0 / kPi + phi );
			return s;
		};
		std::vector<ProbeVolumeShape> shapes;
		if ( redOneBox ) {
			const int lo[3] = { 0, 0, 0 }, hi[3] = { L.X - 1, L.Y - 1, L.Z - 1 };
			shapes.push_back( boxShape( lo, hi ) );
		} else {
			qint64 coveredN = 0;
			for ( int step = 0; step < spec.maxShapes && coveredN < spec.coverTarget * roomN; step++ ) {
				const std::vector<qint32> tU = table( [&]( size_t i ) { return lc[i] == 1 && !covered[i]; } );
				// the seed: the uncovered room cell deepest from the walls
				size_t seed = NL;
				float best = -1;
				for ( size_t i = 0; i < NL; i++ )
					if ( lc[i] == 1 && !covered[i] && ld[i] > best ) {
						best = ld[i];
						seed = i;
					}
				if ( seed == NL )
					break;
				int lo[3], hi[3];
				L.xyz( seed, lo[0], lo[1], lo[2] );
				for ( int c = 0; c < 3; c++ )
					hi[c] = lo[c];
				const int dimL[3] = { L.X, L.Y, L.Z };
				for ( ;; ) {
					int bestF = -1;
					double bestFill = -1;
					qint64 bestGain = -1;
					for ( int f = 0; f < 6; f++ ) {
						const int ax = f / 2;
						int sl[3] = { lo[0], lo[1], lo[2] }, sh[3] = { hi[0], hi[1], hi[2] };
						if ( f % 2 == 0 ) {
							if ( hi[ax] + 1 >= dimL[ax] )
								continue;
							sl[ax] = sh[ax] = hi[ax] + 1;
						} else {
							if ( lo[ax] - 1 < 0 )
								continue;
							sl[ax] = sh[ax] = lo[ax] - 1;
						}
						const qint64 vol = qint64( sh[0] - sl[0] + 1 ) * ( sh[1] - sl[1] + 1 ) * ( sh[2] - sl[2] + 1 );
						const qint64 m = sum( tRoom, sl, sh ), o = sum( tOther, sl, sh ), s = sum( tSolid, sl, sh );
						if ( m == 0 || double( o ) > spec.slabOther * double( m + o ) || s * 2 > vol )
							continue;
						const double fill = double( m ) / double( vol );
						const qint64 gain = sum( tU, sl, sh );
						if ( fill > bestFill + 1e-9 || ( std::fabs( fill - bestFill ) <= 1e-9 && gain > bestGain ) ) {
							bestFill = fill;
							bestGain = gain;
							bestF = f;
						}
					}
					if ( bestF < 0 )
						break;
					if ( bestF % 2 == 0 )
						hi[bestF / 2]++;
					else
						lo[bestF / 2]--;
				}
				qint64 gainBox = sum( tU, lo, hi );
				// wedges over the uncovered remainder compete (a pitched roof's halves)
				bool wedge = false;
				int wlo[3] = { 0, 0, 0 }, whi[3] = { 0, 0, 0 }, wa = 0;
				bool wmin = false;
				if ( useWedge && step > 0 ) {
					int rlo[3] = { L.X, L.Y, L.Z }, rhi[3] = { -1, -1, -1 };
					for ( size_t i = 0; i < NL; i++ )
						if ( lc[i] == 1 && !covered[i] ) {
							int x, y, z;
							L.xyz( i, x, y, z );
							rlo[0] = std::min( rlo[0], x ); rlo[1] = std::min( rlo[1], y ); rlo[2] = std::min( rlo[2], z );
							rhi[0] = std::max( rhi[0], x ); rhi[1] = std::max( rhi[1], y ); rhi[2] = std::max( rhi[2], z );
						}
					if ( rhi[2] - rlo[2] >= 2 ) {
						qint64 bestW = qint64( double( gainBox ) * 1.1 );
						for ( int a = 0; a < 2; a++ )
							for ( int part = 0; part < 3; part++ ) {
								int plo[3] = { rlo[0], rlo[1], rlo[2] }, phi[3] = { rhi[0], rhi[1], rhi[2] };
								const int mid = ( rlo[a] + rhi[a] ) / 2;
								if ( part == 1 )
									phi[a] = mid;
								if ( part == 2 )
									plo[a] = mid + 1;
								if ( phi[a] < plo[a] )
									continue;
								for ( int hm = 0; hm < 2; hm++ ) {
									qint64 m = 0, o = 0, s = 0, gain = 0, vol = 0;
									for ( int k = plo[2]; k <= phi[2]; k++ )
										for ( int j = plo[1]; j <= phi[1]; j++ )
											for ( int i = plo[0]; i <= phi[0]; i++ ) {
												if ( !wedgeIn( plo, phi, a, hm == 1, i, j, k ) )
													continue;
												const size_t li = L.idx( i, j, k );
												vol++;
												m += lc[li] == 1;
												o += lc[li] == 2;
												s += lc[li] == 3;
												gain += lc[li] == 1 && !covered[li];
											}
									if ( m == 0 || double( o ) > spec.slabOther * double( m + o ) || s * 2 > vol )
										continue;
									if ( gain > bestW ) {
										bestW = gain;
										wedge = true;
										for ( int c = 0; c < 3; c++ ) {
											wlo[c] = plo[c];
											whi[c] = phi[c];
										}
										wa = a;
										wmin = hm == 1;
									}
								}
							}
					}
				}
				const qint64 gain = wedge ? 0 : gainBox;
				if ( !wedge && ( gain <= 0 || ( step > 0 && double( gain ) < 0.01 * double( roomN ) ) ) )
					break;
				if ( wedge ) {
					qint64 g = 0;
					for ( int k = wlo[2]; k <= whi[2]; k++ )
						for ( int j = wlo[1]; j <= whi[1]; j++ )
							for ( int i = wlo[0]; i <= whi[0]; i++ )
								if ( wedgeIn( wlo, whi, wa, wmin, i, j, k ) ) {
									const size_t li = L.idx( i, j, k );
									g += lc[li] == 1 && !covered[li];
									covered[li] = 1;
								}
					if ( double( g ) < 0.01 * double( roomN ) )
						break;
					coveredN += g;
					shapes.push_back( wedgeShape( wlo, whi, wa, wmin ) );
				} else {
					for ( int k = lo[2]; k <= hi[2]; k++ )
						for ( int j = lo[1]; j <= hi[1]; j++ )
							for ( int i = lo[0]; i <= hi[0]; i++ )
								covered[L.idx( i, j, k )] = 1;
					coveredN += gain;
					shapes.push_back( boxShape( lo, hi ) );
				}
			}
		}
		float cov = 0, oa = 0, os = 0;
		measure( room, shapes, &cov, &oa, &os );
		// one primitive for the whole room competes: fewer shapes win, boxes win ties
		if ( useAlt && !shapes.empty() ) {
			std::vector<std::vector<ProbeVolumeShape>> alts;
			const int lo[3] = { 0, 0, 0 }, hi[3] = { L.X - 1, L.Y - 1, L.Z - 1 };
			// the room's own extent in the local grid
			int rlo[3] = { L.X, L.Y, L.Z }, rhi[3] = { -1, -1, -1 };
			double cu = 0, cv = 0;
			for ( size_t i = 0; i < NL; i++ )
				if ( lc[i] == 1 ) {
					int x, y, z;
					L.xyz( i, x, y, z );
					rlo[0] = std::min( rlo[0], x ); rlo[1] = std::min( rlo[1], y ); rlo[2] = std::min( rlo[2], z );
					rhi[0] = std::max( rhi[0], x ); rhi[1] = std::max( rhi[1], y ); rhi[2] = std::max( rhi[2], z );
					cu += x + 0.5;
					cv += y + 0.5;
				}
			Q_UNUSED( lo );
			Q_UNUSED( hi );
			cu /= double( roomN );
			cv /= double( roomN );
			ProbeVolumeShape bx = boxShape( rlo, rhi );
			// cylinder: about the footprint's centroid, the radius holding 97 % of the room's cells
			{
				std::vector<double> rr;
				for ( size_t i = 0; i < NL; i++ )
					if ( lc[i] == 1 ) {
						int x, y, z;
						L.xyz( i, x, y, z );
						rr.push_back( std::hypot( x + 0.5 - cu, y + 0.5 - cv ) );
					}
				std::nth_element( rr.begin(), rr.begin() + long( rr.size() * 97 / 100 ), rr.end() );
				const double rad = ( rr[rr.size() * 97 / 100] + 0.5 ) * C;
				ProbeVolumeShape cy = bx;
				cy.type = ProbeShapeType::Cylinder;
				double w[3];
				worldOf( cu, cv, ( rlo[2] + rhi[2] + 1 ) * 0.5, w );
				for ( int c = 0; c < 3; c++ )
					cy.center[c] = float( w[c] );
				cy.size[0] = cy.size[1] = float( 2 * rad );
				alts.push_back( { cy } );
			}
			{
				ProbeVolumeShape sp = bx;
				sp.type = ProbeShapeType::Sphere;
				alts.push_back( { sp } );
			}
			{
				ProbeVolumeShape cp = bx;
				cp.type = ProbeShapeType::Capsule;
				const double eu = bx.size[0], ev = bx.size[1], ez = bx.size[2];
				if ( eu >= ev && eu >= ez ) {
					cp.size[0] = cp.size[1] = float( std::max( ev, ez ) );
					cp.size[2] = float( eu );
					cp.rotation[1] = 90.0f;
				} else if ( ev >= eu && ev >= ez ) {
					cp.size[0] = cp.size[1] = float( std::max( eu, ez ) );
					cp.size[2] = float( ev );
					cp.rotation[1] = 90.0f;
					cp.rotation[2] += 90.0f;
				} else {
					cp.size[0] = cp.size[1] = float( std::max( eu, ev ) );
				}
				alts.push_back( { cp } );
			}
			for ( int a = 0; a < 2; a++ )
				for ( int hm = 0; hm < 2; hm++ )
					alts.push_back( { wedgeShape( rlo, rhi, a, hm == 1 ) } );
			// lane VOLFIT1: a gabled room: walls to the eaves (the top level still filling 90 % of the footprint),
			// the roof's two halves above, high at the ridge
			if ( useWedge ) {
				std::vector<quint8> foot( size_t( L.X ) * size_t( L.Y ), 0 );
				std::vector<qint64> level( size_t( L.Z ), 0 );
				for ( size_t i = 0; i < NL; i++ )
					if ( lc[i] == 1 ) {
						int x, y, z;
						L.xyz( i, x, y, z );
						foot[size_t( y ) * size_t( L.X ) + size_t( x )] = 1;
						level[size_t( z )]++;
					}
				qint64 F = 0;
				for ( quint8 f : foot )
					F += f;
				int eave = -1;
				for ( int z = rlo[2]; z <= rhi[2]; z++ )
					if ( double( level[size_t( z )] ) >= 0.9 * double( F ) )
						eave = z;
				if ( eave >= rlo[2] && rhi[2] - eave >= 3 )
					for ( int a = 0; a < 2; a++ ) {
						const int mid = ( rlo[a] + rhi[a] ) / 2;
						int blo[3] = { rlo[0], rlo[1], rlo[2] }, bhi[3] = { rhi[0], rhi[1], eave };
						int w1lo[3] = { rlo[0], rlo[1], eave + 1 }, w1hi[3] = { rhi[0], rhi[1], rhi[2] };
						int w2lo[3] = { rlo[0], rlo[1], eave + 1 }, w2hi[3] = { rhi[0], rhi[1], rhi[2] };
						w1hi[a] = mid;
						w2lo[a] = mid + 1;
						alts.push_back( { boxShape( blo, bhi ), wedgeShape( w1lo, w1hi, a, false ), wedgeShape( w2lo, w2hi, a, true ) } );
					}
			}
			int pick = -1;
			float pc = cov, po = oa, ps = os;
			for ( size_t k = 0; k < alts.size(); k++ ) {
				float c2 = 0, o2 = 0, s2 = 0;
				measure( room, alts[k], &c2, &o2, &s2 );
				const bool better = shapes.size() > 1
					? c2 >= std::min( cov, spec.coverTarget ) - 0.02f && o2 <= oa + 0.02f
					: c2 >= cov + 0.05f && o2 <= oa + 0.01f;
				// as many shapes as the boxes: only a closer fit (fewer win on the bar alone)
				const bool fewer = alts[k].size() < shapes.size() || ( alts[k].size() == shapes.size() && c2 - o2 > cov - oa + 0.005f );
				if ( better && fewer && ( pick < 0 || c2 - o2 > pc - po ) ) {
					pick = int( k );
					pc = c2;
					po = o2;
					ps = s2;
				}
			}
			if ( pick >= 0 ) {
				shapes = alts[size_t( pick )];
				cov = pc;
				oa = po;
				os = ps;
			}
		}
		// a room the grid fitted with slopes or rounds keeps them (the fine fit's boxes step along a slope): refined here
		bool curved = false;
		for ( const ProbeVolumeShape & s : shapes )
			curved = curved || s.type != ProbeShapeType::Box;
		keepGrid[r] = curved && legacyFine;
		// VOLFIT3 starts every room's fine fit from its refined grid shapes
		if ( refineOn && ( !fineOn || curved || !legacyFine ) && !shapes.empty() ) {
			const std::vector<ProbeVolumeShape> grid = shapes;
			for ( size_t k = 0; k < shapes.size(); k++ ) {
				std::vector<const ProbeVolumeShape *> sib;
				for ( size_t j = 0; j < shapes.size(); j++ )
					if ( j != k )
						sib.push_back( &shapes[j] );
				refineShape( shapes[k], sib, room );
			}
			// the rays may only move faces onto triangles: a fit that lost the room's own air or took other air on
			// the grid moved past what the grid already knew, and the grid fit stands
			float c2 = 0, o2 = 0, s2 = 0;
			measure( room, shapes, &c2, &o2, &s2 );
			if ( c2 >= cov - 0.03f && o2 <= oa + 0.01f ) {
				cov = c2;
				oa = o2;
				os = s2;
			} else {
				shapes = grid;
			}
		}
		rm.shapes = std::move( shapes );
		rm.coverage = cov;
		rm.overshootAir = oa;
		rm.overshootSolid = os;
	}
	// lane VOLRAY1: a threshold from the cell faces leans with the stepped boundary and is one cell thick. Square it to
	// its room, then fit it by rays to the opening: jamb to jamb, floor to lintel, through the wall.
	if ( refineOn )
		for ( ProbeVolumeThreshold & t : V.thresholds ) {
			if ( std::fabs( t.normal[2] ) > 0.7 || t.opening == QLatin1String( "window" ) || t.a < 1 || t.a > int( V.rooms.size() ) )
				continue;
			double psi = t.shape.rotation[2];
			const ProbeVolumeRoom & ra = V.rooms[size_t( t.a - 1 )];
			if ( !ra.shapes.empty() ) {
				const double rho = ra.shapes[0].rotation[2];
				const double k = std::round( ( psi - rho ) / 90.0 );
				if ( std::fabs( psi - ( rho + 90.0 * k ) ) <= 15.0 )
					psi = rho + 90.0 * k;
			}
			const double pr = psi * kPi / 180.0;
			const double ex[3] = { std::cos( pr ), std::sin( pr ), 0 }, ey[3] = { -std::sin( pr ), std::cos( pr ), 0 };
			const double ez[3] = { 0, 0, 1 }, nx[3] = { -ex[0], -ex[1], 0 }, nz[3] = { 0, 0, -1 };
			double c[3] = { t.shape.center[0], t.shape.center[1], t.shape.center[2] };
			const double hw0 = t.shape.size[0] * 0.5, hh0 = t.shape.size[2] * 0.5;
			auto at = [&]( double sx, double sy, double sz, double p[3] ) {
				for ( int k = 0; k < 3; k++ )
					p[k] = c[k] + ex[k] * sx + ey[k] * sy + ez[k] * sz;
			};
			double zlo = -hh0, zhi = hh0, xl = hw0, xr = hw0, ylo = -rC * 0.5, yhi = rC * 0.5;
			bool ok = false;
			for ( int round = 0; round < 2; round++ ) {
				// floor and lintel: the middle of three columns
				double dn[3], up[3];
				for ( int s = 0; s < 3; s++ ) {
					double p[3];
					at( ( xr - xl ) * 0.5 + ( s - 1 ) * ( xl + xr ) * 0.25, 0, ( zlo + zhi ) * 0.5, p );
					dn[s] = cast1( p, nz, hh0 * 2 + 4 * rC );
					up[s] = cast1( p, ez, hh0 * 2 + 4 * rC );
				}
				std::sort( dn, dn + 3 );
				std::sort( up, up + 3 );
				if ( !std::isfinite( dn[1] ) )
					break;
				const double zm = ( zlo + zhi ) * 0.5;
				zlo = zm - dn[1];
				zhi = std::isfinite( up[1] ) ? zm + up[1] : zhi;
				// jambs: the widest of three rows
				double l = 0, r = 0;
				for ( int s = 1; s <= 3; s++ ) {
					double p[3];
					at( ( xr - xl ) * 0.5, 0, zlo + ( zhi - zlo ) * s / 4.0, p );
					const double a = cast1( p, nx, hw0 * 2 + 4 * rC ), b = cast1( p, ex, hw0 * 2 + 4 * rC );
					const double xm = ( xr - xl ) * 0.5;
					l = std::max( l, std::isfinite( a ) ? a - xm : xl );
					r = std::max( r, std::isfinite( b ) ? b + xm : xr );
				}
				xl = l;
				xr = r;
				// the wall: the run of offsets across where a jamb or the lintel still stands close
				const double half = ( xl + xr ) * 0.5, xm = ( xr - xl ) * 0.5, zm2 = ( zlo + zhi ) * 0.5;
				auto wallAt = [&]( double y ) {
					double p[3];
					at( xm, y, zm2, p );
					return cast1( p, nx, half + 4.0 ) <= half + 4.0 || cast1( p, ex, half + 4.0 ) <= half + 4.0
						|| cast1( p, ez, zhi - zm2 + 4.0 ) <= zhi - zm2 + 4.0;
				};
				const int span = int( 4 * rC );
				int best = -100000;
				for ( int y = 0; y <= span && best == -100000; y++ ) {
					if ( wallAt( y ) )
						best = y;
					else if ( wallAt( -y ) )
						best = -y;
				}
				if ( best == -100000 )
					break;
				// a passage that stays narrow (a corridor, a low lintel) is not one wall: a threshold deeper than a cell
				// carves the rooms' own air and parts each room's flood from its shapes
				const int deep = int( rC );
				int y0 = best, y1 = best;
				while ( y1 < best + span && y1 - y0 < deep && wallAt( y1 + 1 ) )
					y1++;
				while ( y0 > best - span && y1 - y0 < deep && wallAt( y0 - 1 ) )
					y0--;
				ylo = y0 - 1.0;
				yhi = y1 + 1.0;
				// recentre on the passage, the next round measures from its middle
				for ( int k = 0; k < 3; k++ )
					c[k] += ey[k] * ( ylo + yhi ) * 0.5;
				yhi -= ( ylo + yhi ) * 0.5;
				ylo = -yhi;
				ok = true;
			}
			// a jamb or lintel ray that missed widens the opening into a room and cuts it in two (museum 0x001B: 7.6M u3
			// of its own shapes parted from its air): a jamb moves out at most a cell, else the grid's threshold stays
			if ( !ok || xl > hw0 + rC || xr > hw0 + rC || zhi - zlo > 2.0 * hh0 + 2.0 * rC )
				continue;
			double p[3];
			at( ( xr - xl ) * 0.5, 0, ( zlo + zhi ) * 0.5, p );
			for ( int k = 0; k < 3; k++ )
				t.shape.center[k] = float( p[k] );
			t.shape.size[0] = float( xl + xr + 2.0 );
			t.shape.size[1] = float( yhi - ylo );
			t.shape.size[2] = float( zhi - zlo + 2.0 );
			t.shape.rotation[0] = t.shape.rotation[1] = 0.0f;
			t.shape.rotation[2] = float( psi );
		}
	// lane VOLFIT2: the rooms refitted on a fine lattice; the fitted thresholds part the floods (the threshold fit reads
	// only the rooms' yaw, which the fine shapes keep)
	if ( fineOn ) {
		QElapsedTimer fc;
		fc.start();
		std::vector<ProbeVolumeShape> cuts;
		for ( const ProbeVolumeThreshold & t : V.thresholds )
			cuts.push_back( t.shape );
		ProbeFineGrid fg;
		fg.labels = V.labels.data();
		fg.state = st.data();
		for ( int c = 0; c < 3; c++ ) {
			fg.dims[c] = V.dims[c];
			fg.origin[c] = V.origin[c];
		}
		fg.cell = C;
		fg.cuts = &cuts;
		std::vector<float> anchors;
		for ( const ProbeVolumeRoom & rm : V.rooms )
			anchors.insert( anchors.end(), rm.anchor, rm.anchor + 3 );
		fg.anchors = anchors.data();
		// volcheck's per-point rays, on the GPU when the bake would take it (the answers are the CPU tree's)
		std::unique_ptr<bakert::Scene> scene;
		if ( bakert::pickPath() != bakert::Path::Cpu ) {
			scene = bakert::Scene::make( rb );
			if ( scene && scene->path() != bakert::Path::Cpu )
				fg.scene = scene.get();
		}
		const size_t nSoupTris = soup.tris.size() / 9;
		fg.backFace = [&]( int tri, const double * d ) -> bool {   // a one-sided face seen from behind (volcheck's)
			if ( tri < 0 || size_t( tri ) >= nSoupTris || soup.isTwoSided( size_t( tri ) ) )
				return false;
			const float * p = &rb.t[size_t( tri ) * 9];
			const double e1[3] = { double( p[3] ) - p[0], double( p[4] ) - p[1], double( p[5] ) - p[2] };
			const double e2[3] = { double( p[6] ) - p[0], double( p[7] ) - p[1], double( p[8] ) - p[2] };
			const double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
			return n[0] * d[0] + n[1] * d[1] + n[2] * d[2] > 0;
		};
		fg.facing = [&]( int tri, const double * d ) -> double {
			if ( tri < 0 || size_t( tri ) >= nSoupTris )
				return 1.0;
			const float * p = &rb.t[size_t( tri ) * 9];
			const double e1[3] = { double( p[3] ) - p[0], double( p[4] ) - p[1], double( p[5] ) - p[2] };
			const double e2[3] = { double( p[6] ) - p[0], double( p[7] ) - p[1], double( p[8] ) - p[2] };
			const double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
			const double l = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
			return l > 0 ? ( n[0] * d[0] + n[1] * d[1] + n[2] * d[2] ) / l : 1.0;
		};
		ProbeFineSpec spec;
		// a tuning pin for the gate runs: WW_VOLFIT_SPEC=h=16,boxes=64,gain=12288,grow=4,reach=3
		for ( const QString & kv : qEnvironmentVariable( "WW_VOLFIT_SPEC" ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) ) {
			const QString k = kv.section( QLatin1Char( '=' ), 0, 0 ).trimmed();
			const double x = kv.section( QLatin1Char( '=' ), 1 ).toDouble();
			if ( k == QLatin1String( "h" ) && x > 0 ) spec.h = x;
			else if ( k == QLatin1String( "boxes" ) && x > 0 ) spec.maxBoxes = int( x );
			else if ( k == QLatin1String( "gain" ) ) spec.minGain = x;
			else if ( k == QLatin1String( "grow" ) ) spec.grow = x;
			else if ( k == QLatin1String( "reach" ) ) spec.ownerReach = int( x );
			else if ( k == QLatin1String( "exact" ) ) spec.exactEdges = x != 0;
			else if ( k == QLatin1String( "free" ) ) spec.freeD = x != 0;
			else if ( k == QLatin1String( "check" ) ) spec.checkPasses = int( x );
			else if ( k == QLatin1String( "margin" ) && x > 0 ) spec.margin = x;
			else if ( k == QLatin1String( "marginopen" ) && x > 0 ) spec.marginOpen = x;
			else if ( k == QLatin1String( "ownedout" ) ) spec.ownedOut = x;
			else if ( k == QLatin1String( "snap" ) && x > 0 ) spec.snapSteps = x;
			else if ( k == QLatin1String( "badw" ) ) spec.badW = x;
			else if ( k == QLatin1String( "extras" ) ) spec.extras = int( x );
			else if ( k == QLatin1String( "budget" ) && x > 0 ) spec.budget = x;
			else if ( k == QLatin1String( "rounds" ) ) spec.rounds = int( x );
			else if ( k == QLatin1String( "jitter" ) ) spec.jitter = x != 0;
		}
		spec.legacy = legacyFine;
		std::vector<std::vector<ProbeVolumeShape>> gridShapes;
		if ( !legacyFine ) {
			for ( const ProbeVolumeRoom & rm : V.rooms )
				gridShapes.push_back( rm.shapes );
			fg.grid = &gridShapes;
		}
		ProbeFineStats fs;
		const std::vector<std::vector<ProbeVolumeShape>> fine = probeVolumesFineFit( rb, rO, fg, int( V.rooms.size() ), roomYaw, spec, &fs );
		for ( size_t r = 0; r < V.rooms.size() && r < fine.size(); r++ ) {
			if ( fine[r].empty() || keepGrid[r] )
				continue;
			ProbeVolumeRoom & rm = V.rooms[r];
			rm.shapes = fine[r];
			float cov = 0, oa = 0, os = 0;
			measure( int( r ) + 1, rm.shapes, &cov, &oa, &os );
			rm.coverage = cov;
			rm.overshootAir = oa;
			rm.overshootSolid = os;
		}
		V.fineBoxes = fs.boxes;
		V.fineRays = fs.rays + fs.snapRays + fs.encTests * spec.sphereK;
		V.fineGrown = fs.encGrown;
		V.finePruned = fs.pruned;
		V.fineLost = fs.checkLost;
		if ( !legacyFine )
			V.fineNote = QStringLiteral( "; volfit3 grid shapes %1 rooms grid %2 moved %3 extras %4 faces %5 merged %6" ).arg( fs.gridShapes )
				.arg( fs.pickGrid ).arg( fs.pickMoved ).arg( fs.extrasUsed ).arg( fs.facesMoved ).arg( fs.merged );
		V.msFine = fc.nsecsElapsed() / 1e6;
	}
	V.refineRays = refineRays;
	V.msFit = clock.nsecsElapsed() / 1e6;
	V.msTotal = total.nsecsElapsed() / 1e6;
	V.ok = true;
	return true;
}

// ---------------------------------------------------------------- cells, hand fixes, ids

void probeVolumesAssignCells( ProbeVolumes * v, bool interior, const QString & key )
{
	for ( ProbeVolumeRoom & r : v->rooms )
		r.cellKey = interior ? key
			: QStringLiteral( "%1_%2" ).arg( int( std::floor( r.anchor[0] / 4096.0 ) ) ).arg( int( std::floor( r.anchor[1] / 4096.0 ) ) );
	// ids per cell, in anchor order (deterministic); probeVolumesKeepIds may change them
	std::map<QString, std::vector<size_t>> per;
	for ( size_t r = 0; r < v->rooms.size(); r++ )
		per[v->rooms[r].cellKey].push_back( r );
	for ( auto & p : per ) {
		std::sort( p.second.begin(), p.second.end(), [&]( size_t a, size_t b ) {
			const float * x = v->rooms[a].anchor, * y = v->rooms[b].anchor;
			return std::make_tuple( x[2], x[1], x[0] ) < std::make_tuple( y[2], y[1], y[0] );
		} );
		quint32 id = 1;
		for ( size_t r : p.second )
			v->rooms[r].id = id++;
	}
}

QStringList probeVolumesApplyRecx( ProbeVolumes * v, const QString & recxPath, const QString & name )
{
	QStringList log;
	QFile f( recxPath );
	if ( !f.open( QIODevice::ReadOnly ) )
		return log;
	QJsonParseError pe;
	const QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &pe );
	if ( doc.isNull() ) {
		log << QStringLiteral( "recx %1: %2" ).arg( recxPath, pe.errorString() );
		return log;
	}
	for ( const QJsonValue & ev : doc.object().value( QStringLiteral( "entries" ) ).toArray() ) {
		const QJsonObject e = ev.toObject();
		if ( !name.isEmpty() && e.value( QStringLiteral( "name" ) ).toString() != name )
			continue;
		for ( const QJsonValue & rv : e.value( QStringLiteral( "rooms" ) ).toArray() ) {
			const QJsonObject ro = rv.toObject();
			const QJsonArray sh = ro.value( QStringLiteral( "shapes" ) ).toArray();
			float anchor[3];
			if ( sh.isEmpty() || !readVec3( ro.value( QStringLiteral( "anchor" ) ), anchor ) )
				continue;
			std::vector<ProbeVolumeShape> shapes;
			for ( const QJsonValue & s : sh ) {
				ProbeVolumeShape x;
				if ( shapeFromJson( s.toObject(), &x, &log ) )
					shapes.push_back( x );
			}
			if ( shapes.empty() )
				continue;
			const int r = v->gridAt( anchor );
			if ( r > 0 ) {
				ProbeVolumeRoom & rm = v->rooms[size_t( r - 1 )];
				rm.shapes = shapes;
				rm.handShapes = true;
				rm.source += QStringLiteral( "+recx" );
				log << QStringLiteral( "recx: room at %1,%2,%3 takes %4 hand shapes" ).arg( double( anchor[0] ) ).arg( double( anchor[1] ) )
					.arg( double( anchor[2] ) ).arg( shapes.size() );
			} else {
				ProbeVolumeRoom rm;
				rm.source = QStringLiteral( "recx" );
				for ( int c = 0; c < 3; c++ )
					rm.anchor[c] = anchor[c];
				rm.shapes = shapes;
				rm.handShapes = true;
				rm.evidence << QStringLiteral( "recx" );
				v->rooms.push_back( rm );
				log << QStringLiteral( "recx: a room the bake left outside, at %1,%2,%3 (%4 shapes)" ).arg( double( anchor[0] ) )
					.arg( double( anchor[1] ) ).arg( double( anchor[2] ) ).arg( shapes.size() );
			}
			v->handFixed++;
		}
	}
	return log;
}

QStringList probeVolumesKeepIds( ProbeVolumes * v, const QString & cellKey, const ProbeVolumes & prev )
{
	QStringList log;
	std::map<size_t, quint32> kept;
	quint32 maxId = 0;
	for ( const ProbeVolumeRoom & o : prev.rooms ) {
		maxId = std::max( maxId, o.id );
		const int r = v->gridAt( o.anchor );
		if ( r <= 0 || v->rooms[size_t( r - 1 )].cellKey != cellKey ) {
			log << QStringLiteral( "room %1 (anchor %2,%3,%4) no longer matches a room" ).arg( hexId( o.id ) )
				.arg( double( o.anchor[0] ) ).arg( double( o.anchor[1] ) ).arg( double( o.anchor[2] ) );
			continue;
		}
		if ( kept.count( size_t( r - 1 ) ) ) {
			log << QStringLiteral( "room %1 merged into %2" ).arg( hexId( o.id ), hexId( kept[size_t( r - 1 )] ) );
			continue;
		}
		kept[size_t( r - 1 )] = o.id;
	}
	std::vector<size_t> fresh;
	for ( size_t r = 0; r < v->rooms.size(); r++ ) {
		if ( v->rooms[r].cellKey != cellKey )
			continue;
		auto it = kept.find( r );
		if ( it != kept.end() ) {
			v->rooms[r].id = it->second;
			v->idsKept++;
		} else
			fresh.push_back( r );
	}
	for ( size_t r : fresh ) {   // in their anchor order (AssignCells' ids)
		Q_UNUSED( r );
	}
	std::sort( fresh.begin(), fresh.end(), [&]( size_t a, size_t b ) { return v->rooms[a].id < v->rooms[b].id; } );
	for ( size_t r : fresh ) {
		v->rooms[r].id = ++maxId;
		v->idsNew++;
	}
	return log;
}

QString probeVolumesCensusText( const ProbeVolumes & v )
{
	if ( !v.ok )
		return QStringLiteral( "space: none (%1)" ).arg( v.error );
	int shapes = 0, borderline = 0, wedges = 0, other = 0, hand = 0;
	double covMin = 1.0, covSum = 0, oaMax = 0;
	int fitted = 0;
	for ( const ProbeVolumeRoom & r : v.rooms ) {
		shapes += int( r.shapes.size() );
		borderline += r.borderline ? 1 : 0;
		hand += r.handShapes ? 1 : 0;
		for ( const ProbeVolumeShape & s : r.shapes ) {
			wedges += s.type == ProbeShapeType::Wedge;
			other += s.type != ProbeShapeType::Box && s.type != ProbeShapeType::Wedge;
		}
		if ( r.cells > 0 && !r.handShapes ) {
			covMin = std::min( covMin, double( r.coverage ) );
			covSum += r.coverage;
			oaMax = std::max( oaMax, double( r.overshootAir ) );
			fitted++;
		}
	}
	int doors = 0, windows = 0, portals = 0;
	for ( const ProbeVolumeThreshold & t : v.thresholds ) {
		doors += t.opening == QLatin1String( "door" );
		windows += t.opening == QLatin1String( "window" );
		portals += t.opening == QLatin1String( "portal" );
	}
	return QStringLiteral( "space: rooms %1 (enclosed %2, roofed %3 of %4 candidates: small %5 sky %6 open %7 back %8; inside geometry %9, "
		"folded %10, dropped %11, grown %12 cells, merged %35), borderline %13; shapes %14 (wedges %15, round %16, hand %17), coverage min %18 "
		"mean %19, overshoot air max %20; thresholds %21 (doors %22, windows %23, portals %24, tiny %25); sky cells %26 (coarse %27); "
		"door width %28; ms rooms %29 sky %30 classify %31 thresholds %32 fit %33 total %34" )
		.arg( v.rooms.size() ).arg( v.enclosed ).arg( v.roofedRooms ).arg( v.candidates ).arg( v.rejectedSmall ).arg( v.rejectedSky )
		.arg( v.rejectedOpen ).arg( v.rejectedBack ).arg( v.insideGeometry )
		.arg( v.folded ).arg( v.dropped ).arg( v.grownCells ).arg( borderline ).arg( shapes ).arg( wedges ).arg( other ).arg( hand )
		.arg( fitted ? covMin : 0.0, 0, 'f', 3 ).arg( fitted ? covSum / fitted : 0.0, 0, 'f', 3 ).arg( oaMax, 0, 'f', 3 )
		.arg( v.thresholds.size() ).arg( doors ).arg( windows ).arg( portals ).arg( v.tinyThresholds )
		.arg( v.skyCells ).arg( v.skyCoarse ).arg( v.doorWidth, 0, 'f', 0 )
		.arg( qRound( v.msRooms ) ).arg( qRound( v.msSky ) ).arg( qRound( v.msClassify ) ).arg( qRound( v.msThresholds ) )
		.arg( qRound( v.msFit ) ).arg( qRound( v.msTotal ) ).arg( v.merged )
		+ ( v.msFine > 0 ? QStringLiteral( "; fine boxes %1 pruned %5 lost %6 rays %2 grown %3 ms %4" ).arg( v.fineBoxes ).arg( v.fineRays ).arg( v.fineGrown ).arg( qRound( v.msFine ) ).arg( v.finePruned ).arg( v.fineLost ) + v.fineNote : QString() );
}

// ---------------------------------------------------------------- files

QByteArray probeVolumesJson( const ProbeVolumes & v, const QString & key, const QString & world, bool interior )
{
	QJsonObject root;
	root.insert( QStringLiteral( "format" ), QStringLiteral( "space" ) );
	root.insert( QStringLiteral( "version" ), kStageVersion );
	root.insert( QStringLiteral( "stage" ), QStringLiteral( "3 space" ) );
	root.insert( QStringLiteral( "cell" ), key );
	if ( !world.isEmpty() )
		root.insert( QStringLiteral( "worldspace" ), world );
	root.insert( QStringLiteral( "interior" ), interior );
	QJsonObject grid;
	grid.insert( QStringLiteral( "origin" ), vec3( v.origin ) );
	grid.insert( QStringLiteral( "cell" ), double( v.cell ) );
	grid.insert( QStringLiteral( "dims" ), QJsonArray { v.dims[0], v.dims[1], v.dims[2] } );
	root.insert( QStringLiteral( "grid" ), grid );
	auto nameOf = [&]( int r ) -> QString {
		if ( r <= 0 )
			return QStringLiteral( "outside" );
		const ProbeVolumeRoom & rm = v.rooms[size_t( r - 1 )];
		return rm.cellKey == key ? hexId( rm.id ) : rm.cellKey + QLatin1Char( ':' ) + hexId( rm.id );
	};
	QJsonArray rooms;
	for ( const ProbeVolumeRoom & r : v.rooms ) {
		if ( r.cellKey != key )
			continue;
		QJsonObject o;
		o.insert( QStringLiteral( "room" ), hexId( r.id ) );
		o.insert( QStringLiteral( "anchor" ), vec3( r.anchor ) );
		o.insert( QStringLiteral( "source" ), r.source );
		QJsonArray sh;
		for ( const ProbeVolumeShape & s : r.shapes )
			sh.append( shapeJson( s ) );
		o.insert( QStringLiteral( "shapes" ), sh );
		QJsonObject fit;
		fit.insert( QStringLiteral( "cells" ), double( r.cells ) );
		fit.insert( QStringLiteral( "volume" ), std::round( double( r.cells ) * v.cell * v.cell * v.cell ) );
		fit.insert( QStringLiteral( "coverage" ), r3( r.coverage ) );
		fit.insert( QStringLiteral( "overshootAir" ), r3( r.overshootAir ) );
		fit.insert( QStringLiteral( "overshootSolid" ), r3( r.overshootSolid ) );
		fit.insert( QStringLiteral( "hand" ), r.handShapes );
		o.insert( QStringLiteral( "fit" ), fit );
		QJsonObject cls;
		cls.insert( QStringLiteral( "sky" ), r3( r.sky ) );
		cls.insert( QStringLiteral( "wall" ), r3( r.wall ) );
		cls.insert( QStringLiteral( "openness" ), r3( r.openness ) );
		cls.insert( QStringLiteral( "back" ), r3( r.back ) );
		cls.insert( QStringLiteral( "borderline" ), r.borderline );
		o.insert( QStringLiteral( "classify" ), cls );
		o.insert( QStringLiteral( "bounds" ), QJsonArray { vec3( r.lo ), vec3( r.hi ) } );
		o.insert( QStringLiteral( "evidence" ), QJsonArray::fromStringList( r.evidence ) );
		rooms.append( o );
	}
	root.insert( QStringLiteral( "rooms" ), rooms );
	QJsonArray th;
	int n = 0;
	for ( const ProbeVolumeThreshold & t : v.thresholds ) {
		// the owner: room a's cell
		if ( t.a <= 0 || v.rooms[size_t( t.a - 1 )].cellKey != key )
			continue;
		QJsonObject o;
		o.insert( QStringLiteral( "threshold" ), QStringLiteral( "T%1" ).arg( ++n, 4, 10, QLatin1Char( '0' ) ) );
		o.insert( QStringLiteral( "between" ), QJsonArray { nameOf( t.a ), nameOf( t.b ) } );
		o.insert( QStringLiteral( "opening" ), t.opening );
		o.insert( QStringLiteral( "shape" ), shapeJson( t.shape ) );
		o.insert( QStringLiteral( "normal" ), vec3( t.normal ) );
		o.insert( QStringLiteral( "area" ), std::round( double( t.area ) ) );
		o.insert( QStringLiteral( "evidence" ), QJsonArray::fromStringList( t.evidence ) );
		th.append( o );
	}
	root.insert( QStringLiteral( "thresholds" ), th );
	return QJsonDocument( root ).toJson( QJsonDocument::Indented );
}

bool probeVolumesRead( const QString & path, ProbeVolumes * out, QString * error )
{
	*out = ProbeVolumes();
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		*error = QStringLiteral( "cannot read %1" ).arg( path );
		return false;
	}
	QJsonParseError pe;
	const QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &pe );
	const QJsonObject root = doc.object();
	if ( doc.isNull() || root.value( QStringLiteral( "format" ) ).toString() != QLatin1String( "space" ) ) {
		*error = QStringLiteral( "%1: not a .space file (%2)" ).arg( path, pe.errorString() );
		return false;
	}
	const QString key = root.value( QStringLiteral( "cell" ) ).toString();
	std::map<QString, int> byName;
	for ( const QJsonValue & rv : root.value( QStringLiteral( "rooms" ) ).toArray() ) {
		const QJsonObject o = rv.toObject();
		ProbeVolumeRoom r;
		r.id = o.value( QStringLiteral( "room" ) ).toString().toUInt( nullptr, 0 );
		r.cellKey = key;
		r.source = o.value( QStringLiteral( "source" ) ).toString();
		readVec3( o.value( QStringLiteral( "anchor" ) ), r.anchor );
		for ( const QJsonValue & s : o.value( QStringLiteral( "shapes" ) ).toArray() ) {
			ProbeVolumeShape x;
			if ( shapeFromJson( s.toObject(), &x, nullptr ) )
				r.shapes.push_back( x );
		}
		const QJsonObject fit = o.value( QStringLiteral( "fit" ) ).toObject();
		r.cells = qint64( fit.value( QStringLiteral( "cells" ) ).toDouble() );
		r.coverage = float( fit.value( QStringLiteral( "coverage" ) ).toDouble() );
		r.handShapes = fit.value( QStringLiteral( "hand" ) ).toBool();
		byName[o.value( QStringLiteral( "room" ) ).toString()] = int( out->rooms.size() ) + 1;
		out->rooms.push_back( r );
	}
	for ( const QJsonValue & tv : root.value( QStringLiteral( "thresholds" ) ).toArray() ) {
		const QJsonObject o = tv.toObject();
		ProbeVolumeThreshold t;
		const QJsonArray b = o.value( QStringLiteral( "between" ) ).toArray();
		t.a = b.size() > 0 && byName.count( b[0].toString() ) ? byName[b[0].toString()] : 0;
		t.b = b.size() > 1 && byName.count( b[1].toString() ) ? byName[b[1].toString()] : 0;
		t.opening = o.value( QStringLiteral( "opening" ) ).toString();
		shapeFromJson( o.value( QStringLiteral( "shape" ) ).toObject(), &t.shape, nullptr );
		readVec3( o.value( QStringLiteral( "normal" ) ), t.normal );
		t.area = float( o.value( QStringLiteral( "area" ) ).toDouble() );
		out->thresholds.push_back( t );
	}
	out->ok = true;
	return true;
}

ProbeVolumesInputs probeVolumesInputs( const ProbeSoup & soup, const std::vector<ProbeVolumeMarker> & markers,
	const std::vector<ProbeVolumeAperture> & apertures, const ProbeVolumeSpec & s, const QString & recxPath )
{
	ProbeVolumesInputs in;
	{
		QCryptographicHash h( QCryptographicHash::Sha1 );
		const QString id = QStringLiteral( "space v%1 cell %2 max %3 pinch %4 clip %5,%6,%7,%8 sky %9 wall %10 open %11 back %12 reach %13 "
			"minvol %14 grow %15 cover %16 slab %17 shapes %18 band %19 red %20 merge %21" )
			.arg( kStageVersion ).arg( double( s.cell ) ).arg( s.maxCells ).arg( double( s.pinch ) ).arg( double( s.clipLo[0] ) )
			.arg( double( s.clipLo[1] ) ).arg( double( s.clipHi[0] ) ).arg( double( s.clipHi[1] ) ).arg( double( s.skyMax ) )
			.arg( double( s.wallMin ) ).arg( double( s.openMax ) ).arg( double( s.backMax ) ).arg( double( s.wallReach ) )
			.arg( double( s.minVolume ) ).arg( s.growSteps ).arg( double( s.coverTarget ) ).arg( double( s.slabOther ) ).arg( s.maxShapes )
			.arg( double( s.borderBand ) ).arg( s.red ).arg( double( s.mergeShare ) );
		h.addData( id.toUtf8() );
		in.identity = h.result().toHex();
	}
	{
		QCryptographicHash h( QCryptographicHash::Sha1 );
		sha( h, soup.tris.data(), soup.tris.size() * 4 );
		sha( h, soup.glass.data(), soup.glass.size() * 4 );
		for ( const ProbeSoup::Door & d : soup.doors ) {
			sha( h, &d.ref, 4 );
			sha( h, d.lo, 12 );
			sha( h, d.hi, 12 );
		}
		sha( h, soup.twoSided.data(), soup.twoSided.size() );
		sha( h, soup.amask.triOf.data(), soup.amask.triOf.size() * sizeof( int ) );
		for ( const auto & t : soup.amask.tris ) {
			sha( h, &t.map, sizeof( int ) );
			sha( h, &t.thr, 1 );
			sha( h, t.uv, 24 );
			sha( h, t.as, 12 );
		}
		for ( const auto & m : soup.amask.maps ) {
			sha( h, &m.w, 4 );
			sha( h, &m.h, 4 );
			sha( h, m.a.data(), m.a.size() );
		}
		in.soup = s.red == QLatin1String( "fpsoup" ) ? QByteArray( "0" ) : h.result().toHex();
	}
	{
		QCryptographicHash h( QCryptographicHash::Sha1 );
		for ( const ProbeVolumeMarker & m : markers ) {
			sha( h, &m.type, 4 );
			sha( h, m.c, 12 );
			sha( h, m.ax, 36 );
			sha( h, m.half, 12 );
			sha( h, &m.ref, 4 );
			sha( h, &m.from, 4 );
			sha( h, &m.to, 4 );
		}
		in.markers = h.result().toHex();
	}
	{
		QCryptographicHash h( QCryptographicHash::Sha1 );
		for ( const ProbeVolumeAperture & a : apertures ) {
			sha( h, &a.type, 4 );
			sha( h, a.pos, 12 );
			sha( h, &a.width, 4 );
			sha( h, &a.height, 4 );
			sha( h, &a.doorRef, 4 );
		}
		in.apertures = h.result().toHex();
	}
	in.recx = "none";
	QFile f( recxPath );
	if ( !recxPath.isEmpty() && f.open( QIODevice::ReadOnly ) )
		in.recx = QCryptographicHash::hash( f.readAll(), QCryptographicHash::Sha1 ).toHex();
	return in;
}

QString probeVolumesCheck( const QString & dir, const QString & key, const ProbeVolumesInputs & in, const QString & red )
{
	if ( red == QLatin1String( "noskip" ) )
		return QStringLiteral( "forced (red noskip)" );
	QFile f( QDir( dir ).filePath( key + QStringLiteral( ".space.manifest" ) ) );
	if ( !f.exists() )
		return QStringLiteral( "never built" );
	if ( !f.open( QIODevice::ReadOnly ) )
		return QStringLiteral( "unreadable" );
	std::map<QByteArray, QByteArray> rec;
	std::vector<std::pair<QByteArray, QByteArray>> outs;
	for ( const QByteArray & line : f.readAll().split( '\n' ) ) {
		const QList<QByteArray> w = line.trimmed().split( ' ' );
		if ( w.size() == 2 && w[0] == "identity" )
			rec["identity"] = w[1];
		else if ( w.size() == 3 && w[0] == "input" )
			rec[w[1]] = w[2];
		else if ( w.size() == 3 && w[0] == "output" )
			outs.push_back( { w[1], w[2] } );
	}
	if ( rec["identity"] != in.identity )
		return QStringLiteral( "identity" );
	const std::pair<const char *, const QByteArray *> inputs[] = { { "soup", &in.soup }, { "markers", &in.markers },
		{ "apertures", &in.apertures }, { "recx", &in.recx } };
	for ( const auto & i : inputs )
		if ( rec[i.first] != *i.second )
			return QStringLiteral( "input %1" ).arg( QLatin1String( i.first ) );
	if ( outs.empty() )
		return QStringLiteral( "unreadable" );
	for ( const auto & o : outs ) {
		QFile of( QDir( dir ).filePath( QString::fromUtf8( o.first ) ) );
		if ( !of.open( QIODevice::ReadOnly ) || QCryptographicHash::hash( of.readAll(), QCryptographicHash::Sha1 ).toHex() != o.second )
			return QStringLiteral( "output %1" ).arg( QString::fromUtf8( o.first ) );
	}
	return QString();
}

bool probeVolumesWriteCell( const QString & dir, const QString & key, const QByteArray & json, const ProbeVolumesInputs & in,
	QString * error )
{
	QDir().mkpath( dir );
	const QString name = key + QStringLiteral( ".space" );
	QFile f( QDir( dir ).filePath( name ) );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) || f.write( json ) != json.size() ) {
		*error = QStringLiteral( "cannot write %1" ).arg( f.fileName() );
		return false;
	}
	f.close();
	QFile m( QDir( dir ).filePath( key + QStringLiteral( ".space.manifest" ) ) );
	if ( !m.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		*error = QStringLiteral( "cannot write %1" ).arg( m.fileName() );
		return false;
	}
	QByteArray t;
	t += "stage 3 space\n";
	t += "identity " + in.identity + "\n";
	t += "input soup " + in.soup + "\n";
	t += "input markers " + in.markers + "\n";
	t += "input apertures " + in.apertures + "\n";
	t += "input recx " + in.recx + "\n";
	t += "output " + name.toUtf8() + " " + QCryptographicHash::hash( json, QCryptographicHash::Sha1 ).toHex() + "\n";
	m.write( t );
	return true;
}

QString probeVolumesDir( const QString & root, const QString & world, bool interior, const QString & key )
{
	return interior ? QDir( root ).filePath( QStringLiteral( "Interiors/" ) + key ) : QDir( root ).filePath( world + QLatin1Char( '/' ) + key );
}

bool probeVolumesMarkersWrite( const QString & path, const std::vector<ProbeVolumeMarker> & ms )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return false;
	QTextStream s( &f );
	for ( const ProbeVolumeMarker & m : ms ) {
		s << m.type << ' ' << m.ref << ' ' << m.from << ' ' << m.to;
		for ( float v : m.c )
			s << ' ' << v;
		for ( const auto & a : m.ax )
			for ( float v : a )
				s << ' ' << v;
		for ( float v : m.half )
			s << ' ' << v;
		s << '\n';
	}
	return true;
}

bool probeVolumesMarkersRead( const QString & path, std::vector<ProbeVolumeMarker> * ms )
{
	ms->clear();
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return false;
	for ( const QByteArray & line : f.readAll().split( '\n' ) ) {
		const QList<QByteArray> w = line.simplified().split( ' ' );
		if ( w.size() != 22 )
			continue;
		ProbeVolumeMarker m;
		m.type = w[0].toInt();
		m.ref = w[1].toUInt();
		m.from = w[2].toUInt();
		m.to = w[3].toUInt();
		int k = 4;
		for ( float & v : m.c )
			v = w[k++].toFloat();
		for ( auto & a : m.ax )
			for ( float & v : a )
				v = w[k++].toFloat();
		for ( float & v : m.half )
			v = w[k++].toFloat();
		ms->push_back( m );
	}
	return true;
}

// ---------------------------------------------------------------- CLI

int probeVolumesCli( const QStringList & args )
{
	QString soupPath, outDir, key, world, markersPath, recxPath, rect, recxName, viewDump, giMap;
	bool interior = false, gridOut = false, aps = false;
	ProbeVolumeSpec spec;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--key" ) ) { key = nx; i++; }
		else if ( a == QLatin1String( "--world" ) ) { world = nx; i++; }
		else if ( a == QLatin1String( "--markers" ) ) { markersPath = nx; i++; }
		else if ( a == QLatin1String( "--recx" ) ) { recxPath = nx; i++; }
		else if ( a == QLatin1String( "--recx-name" ) ) { recxName = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--view-dump" ) ) { viewDump = nx; i++; }
		else if ( a == QLatin1String( "--gi-map" ) ) { giMap = nx; i++; }
		else if ( a == QLatin1String( "--skymax" ) ) { spec.skyMax = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--pinch" ) ) { spec.pinch = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--cell" ) ) { spec.cell = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--max-cells" ) ) { spec.maxCells = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { spec.red = nx; i++; }
		else if ( a == QLatin1String( "--interior" ) ) interior = true;
		else if ( a == QLatin1String( "--grid" ) ) gridOut = true;
		else if ( a == QLatin1String( "--apertures" ) ) aps = true;
	}
	QTextStream o( stdout );
	if ( soupPath.isEmpty() || outDir.isEmpty() || ( interior && key.isEmpty() ) || ( !interior && rect.isEmpty() && key.isEmpty() ) ) {
		o << "usage: volumes --soup S --out D (--interior --key EDID | --rect x0,y0,x1,y1 [--world W] | --key K) [--markers f] "
			"[--recx f [--recx-name N]] [--skymax s] [--red r] [--grid] [--apertures] [--view-dump f] [--gi-map f]\n";
		return 2;
	}
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		o << "volumes: " << err << "\n";
		return 1;
	}
	std::vector<ProbeVolumeMarker> markers;
	if ( !markersPath.isEmpty() && !probeVolumesMarkersRead( markersPath, &markers ) ) {
		o << "volumes: cannot read markers " << markersPath << "\n";
		return 1;
	}
	float rc[4] = { 0, 0, 0, 0 };
	const QStringList rv = rect.split( ',' );
	const bool hasRect = rv.size() == 4;
	if ( hasRect ) {
		for ( int k = 0; k < 4; k++ )
			rc[k] = rv[k].toFloat();
		if ( !interior ) {   // the block plus a margin: rooms on its edge are whole, kept only by their anchor's cell
			spec.clipLo[0] = rc[0] - 512.0f;
			spec.clipLo[1] = rc[1] - 512.0f;
			spec.clipHi[0] = rc[2] + 512.0f;
			spec.clipHi[1] = rc[3] + 512.0f;
		}
	}
	std::vector<ProbeVolumeAperture> apertures;
	if ( aps && hasRect ) {
		ProbePlaceSpec ps;
		ps.minX = rc[0];
		ps.minY = rc[1];
		ps.maxX = rc[2];
		ps.maxY = rc[3];
		ProbePlaceResult pr;
		if ( probePlace( soup, ps, &pr ) )
			for ( const ProbePoint & p : pr.probes )
				if ( p.cls == ProbeClass::Aperture ) {
					ProbeVolumeAperture a;
					a.type = int( p.kind );
					for ( int c = 0; c < 3; c++ )
						a.pos[c] = p.pos[c];
					a.width = p.width;
					a.height = p.height;
					a.doorRef = p.doorRef;
					apertures.push_back( a );
				}
	}
	// the cells this run writes
	ProbeVolumesStageSpec st;
	st.root = outDir;
	st.world = world;
	st.interior = interior;
	st.flat = world.isEmpty() && !interior;
	st.byAnchor = !interior && hasRect;
	st.spec = spec;
	st.recxFile = recxPath;
	st.recxName = recxName;
	st.grid = gridOut;
	if ( interior || !hasRect )
		st.keys << key;
	else
		for ( int y = int( std::floor( rc[1] / 4096.0 ) ); y <= int( std::floor( rc[3] / 4096.0 ) ); y++ )
			for ( int x = int( std::floor( rc[0] / 4096.0 ) ); x <= int( std::floor( rc[2] / 4096.0 ) ); x++ )
				st.keys << QStringLiteral( "%1_%2" ).arg( x ).arg( y );
	ProbeVolumes v;
	bool ok = false;
	o << probeVolumesStage( soup, markers, apertures, st, &v, &ok );
	if ( !viewDump.isEmpty() ) {   // the debug view's geometry, counted (the GL draw itself is a live check)
		const WwCellVolumeGeometry g = wwCellVolumeGeometry( v );
		QFile f( viewDump );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			f.write( QStringLiteral( "rooms=%1 shapes=%2 thresholds=%3 lines=%4 tris=%5\n" ).arg( g.rooms ).arg( g.shapes ).arg( g.quads )
				.arg( g.lines.size() / 6 ).arg( g.tris.size() / 9 ).toUtf8() );
	}
	if ( !giMap.isEmpty() ) {   // the rooms onto a GI room grid built from the same soup (the default room spec)
		ProbeRooms gr;
		if ( !probeRoomsBuild( soup, ProbeRoomSpec(), &gr ) ) {
			o << "volumes: GI room grid failed: " << gr.error << "\n";
			return 1;
		}
		const ProbeVolumesGi g = probeVolumesToGi( v, gr, 0.2f, spec.red );
		QFile f( giMap );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			o << "volumes: cannot write " << giMap << "\n";
			return 1;
		}
		QString t = QStringLiteral( "census\t%1\ngirooms\t%2\n" ).arg( g.census ).arg( gr.rooms );
		for ( size_t i = 0; i < g.rooms.size(); i++ )
			t += QStringLiteral( "room\t%1\t%2\t%3\t%4\t%5\t%6\n" ).arg( v.rooms[i].id ).arg( g.rooms[i].giRoom ).arg( g.rooms[i].votes )
				.arg( g.rooms[i].samples ).arg( g.rooms[i].outdoors ).arg( g.rooms[i].split.join( ' ' ) );
		for ( const ProbeVolumesGiThreshold & th : g.thresholds )
			t += QStringLiteral( "threshold\t%1\t%2\n" ).arg( th.rooms[0] ).arg( th.rooms[1] );
		t += QStringLiteral( "shapes\t%1\n" ).arg( g.shapes.size() );
		for ( const QString & l : g.log )
			t += QStringLiteral( "log\t%1\n" ).arg( l );
		// the Space rooms as the room source (WW_CELL_VOLUMES_GI=1), then mapped again onto the relabeled grid
		const QString sline = probeRoomsFromSpace( v, &gr, spec.red );
		const ProbeVolumesGi sg = probeVolumesToGi( v, gr, 0.2f );
		t += QStringLiteral( "spacegi\t%1\n" ).arg( sline );
		for ( size_t i = 0; i < sg.rooms.size(); i++ )
			t += QStringLiteral( "sroom\t%1\t%2\n" ).arg( v.rooms[i].id ).arg( sg.rooms[i].giRoom );
		for ( const QString & l : sg.log )
			t += QStringLiteral( "slog\t%1\n" ).arg( l );
		f.write( t.toUtf8() );
		o << g.census << "\n";
	}
	return ok ? 0 : 1;
}

// ---------------------------------------------------------------- the stage (CLI and cell view)

QString probeVolumesStage( const ProbeSoup & soup, const std::vector<ProbeVolumeMarker> & markers,
	const std::vector<ProbeVolumeAperture> & apertures, const ProbeVolumesStageSpec & st, ProbeVolumes * out, bool * ok )
{
	QString log;
	QTextStream o( &log );
	*ok = false;
	*out = ProbeVolumes();
	const ProbeVolumeSpec & spec = st.spec;
	const QString recxUse = spec.red == QLatin1String( "norecx" ) ? QString() : st.recxFile;
	const ProbeVolumesInputs in = probeVolumesInputs( soup, markers, apertures, spec, recxUse );
	auto dirOf = [&]( const QString & k ) {
		return st.flat ? QDir( st.root ).filePath( k ) : probeVolumesDir( st.root, st.world, st.interior, k );
	};
	QString why;
	for ( const QString & k : st.keys ) {
		why = probeVolumesCheck( dirOf( k ), k, in, spec.red );
		if ( !why.isEmpty() ) {
			why = k + QStringLiteral( ": " ) + why;
			break;
		}
	}
	if ( why.isEmpty() ) {   // clean: the files stand; read back for the view
		o << "space: skipped (clean)\n";
		out->ok = true;
		for ( const QString & k : st.keys ) {
			ProbeVolumes c;
			QString e;
			if ( !probeVolumesRead( QDir( dirOf( k ) ).filePath( k + QStringLiteral( ".space" ) ), &c, &e ) )
				continue;
			const int base = int( out->rooms.size() );
			for ( ProbeVolumeThreshold & t : c.thresholds ) {
				t.a = t.a > 0 ? t.a + base : 0;
				t.b = t.b > 0 ? t.b + base : 0;
				out->thresholds.push_back( t );
			}
			out->rooms.insert( out->rooms.end(), c.rooms.begin(), c.rooms.end() );
		}
		*ok = true;
		return log;
	}
	o << "space: rebuild (" << why << ")\n";
	ProbeVolumes & v = *out;
	if ( !probeVolumesBuild( soup, markers, apertures, spec, &v ) ) {
		o << "volumes: " << v.error << "\n";
		return log;
	}
	const QString one = st.keys.isEmpty() ? QString() : st.keys.front();
	probeVolumesAssignCells( &v, !st.byAnchor, one );
	if ( !recxUse.isEmpty() )   // one named record, else each cell's own record
		for ( const QString & k : st.recxName.isEmpty() ? st.keys : QStringList{ st.recxName } )
			for ( const QString & l : probeVolumesApplyRecx( &v, recxUse, k ) )
				o << "  " << l << "\n";
	for ( ProbeVolumeRoom & r : v.rooms )   // a room the recx added takes its anchor's cell too
		if ( r.cellKey.isEmpty() )
			r.cellKey = !st.byAnchor ? one
				: QStringLiteral( "%1_%2" ).arg( int( std::floor( r.anchor[0] / 4096.0 ) ) ).arg( int( std::floor( r.anchor[1] / 4096.0 ) ) );
	o << v.roomsLine << "\n";
	// every cell's ids first (a threshold names a neighbor cell's room by its kept id), then the files
	if ( spec.red != QLatin1String( "noids" ) )
		for ( const QString & k : st.keys ) {
			ProbeVolumes prev;
			QString e2;
			if ( probeVolumesRead( QDir( dirOf( k ) ).filePath( k + QStringLiteral( ".space" ) ), &prev, &e2 ) )
				for ( const QString & l : probeVolumesKeepIds( &v, k, prev ) )
					o << "  ids " << k << ": " << l << "\n";
		}
	for ( const QString & k : st.keys ) {
		const QString d = dirOf( k );
		QString err;
		if ( !probeVolumesWriteCell( d, k, probeVolumesJson( v, k, st.world, st.interior ), in, &err ) ) {
			o << "volumes: " << err << "\n";
			return log;
		}
		if ( st.grid ) {
			QFile g( QDir( d ).filePath( k + QStringLiteral( ".spacegrid" ) ) );
			if ( g.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
				g.write( reinterpret_cast<const char *>( v.origin ), 12 );
				g.write( reinterpret_cast<const char *>( &v.cell ), 4 );
				g.write( reinterpret_cast<const char *>( v.dims ), 12 );
				g.write( reinterpret_cast<const char *>( v.labels.data() ), qint64( v.labels.size() * 4 ) );
			}
		}
	}
	o << probeVolumesCensusText( v ) << "\n";
	*ok = true;
	return log;
}
