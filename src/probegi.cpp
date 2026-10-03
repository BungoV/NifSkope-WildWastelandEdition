/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "probegi.h"

#include "gl/celllights.h"
#include "probebvh.h"
#include "probebake.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>
#include <unordered_map>

namespace {

constexpr double kFourPi = 12.566370614359172;
constexpr double kPi = 3.141592653589793;

// ---- the `.tbk` v3 records (src/probebake.cpp writes them; the layouts are FO4CS's reader's)
struct TbkHeader
{
	quint32 magic, version, recordKind;
	qint32 cellX, cellY;
	float surfelCellSize;
	quint32 surfelCount, probeCount, linkCount;
	quint32 flags;
	quint32 reserved[6];
};
static_assert( sizeof( TbkHeader ) == 64, "tbk header" );

struct TbkSurfel
{
	float position[3];
	qint16 normal[3];
	quint8 albedo[3];
	quint8 pad0;
	quint32 samples;
	quint32 pad1;
};
static_assert( sizeof( TbkSurfel ) == 32, "tbk surfel" );

struct TbkLink
{
	qint16 cellDelta[3];
	qint16 dir[2];
	quint16 weight;
};
static_assert( sizeof( TbkLink ) == 12, "tbk link" );

struct TbkProbe
{
	float position[3];
	quint32 linkOffset, linkCount;
	float linkWeightScale, coverage, unlinkedWeight;
	float skyVis[8], octantDistance[8], octantDistanceRms[8];
	quint32 placementClass, placementLevel, placementReserved[2];
};
static_assert( sizeof( TbkProbe ) == 144, "tbk probe" );

// lane BAKE4: the v4 tail's link record (src/probebake.cpp's TbkLinkExt)
struct TbkLinkExt
{
	quint8 side;      //!< 1 = the cell's back surfel
	quint8 tint[3];   //!< glass transmittance on the way, 255 = clear
	quint32 door;
};
static_assert( sizeof( TbkLinkExt ) == 8, "tbk link ext" );

// lane SKY1: the v4 tail's probe record (src/probebake.cpp's TbkProbeExt)
struct TbkProbeExt
{
	quint8 skyTint[8][3];   //!< per octant, the sky seen through glass (255 = clear)
	quint32 room[2];
};
static_assert( sizeof( TbkProbeExt ) == 32, "tbk probe ext" );

// lane BOUNCE2: the v4 tail's room box (src/probebake.cpp's TbkRoomBox), a room's air
struct TbkRoomBox
{
	quint32 room;
	float lo[3], hi[3];
	quint32 reserved;
};
static_assert( sizeof( TbkRoomBox ) == 32, "tbk room box" );

struct Tbk
{
	QString name;
	TbkHeader h;
	std::vector<TbkSurfel> surfels;
	std::vector<TbkProbe> probes;
	std::vector<TbkLink> links;
	std::vector<TbkSurfel> back;      //!< v4: the cells' second sides
	std::vector<TbkLinkExt> lext;     //!< v4: one per link
	std::vector<TbkProbeExt> pext;    //!< v4: one per probe (lane SKY1 reads its sky tint)
	std::vector<TbkRoomBox> boxes;    //!< v4: the rooms' air (lane BOUNCE2)
};

bool readTbk( const QString & path, Tbk & t, QString * err )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		*err = QStringLiteral( "%1: cannot open" ).arg( path );
		return false;
	}
	const QByteArray b = f.readAll();
	if ( b.size() < 64 ) {
		*err = QStringLiteral( "%1: shorter than a header" ).arg( path );
		return false;
	}
	std::memcpy( &t.h, b.constData(), 64 );
	// lane BAKE4: v4 = the v3 body + a tail (back surfels, link and probe extensions, room boxes)
	if ( t.h.magic != 0x314B4254u || ( t.h.version != 3u && t.h.version != 4u ) || t.h.recordKind != 1u ) {
		*err = QStringLiteral( "%1: not a v3 or v4 resolved-albedo .tbk" ).arg( path );
		return false;
	}
	const qint64 body = 64 + 32 * qint64( t.h.surfelCount ) + 144 * qint64( t.h.probeCount ) + 12 * qint64( t.h.linkCount );
	const bool v4 = t.h.version == 4u;
	const qint64 need = body
		+ ( v4 ? 32 * qint64( t.h.reserved[0] ) + 8 * qint64( t.h.linkCount ) + 32 * qint64( t.h.probeCount )
				+ 32 * qint64( t.h.reserved[1] )
			   : 0 );
	if ( b.size() != need ) {
		*err = QStringLiteral( "%1: %2 bytes, the counts say %3" ).arg( path ).arg( b.size() ).arg( need );
		return false;
	}
	const char * p = b.constData() + 64;
	t.surfels.resize( t.h.surfelCount );
	std::memcpy( t.surfels.data(), p, t.surfels.size() * 32 );
	p += t.surfels.size() * 32;
	t.probes.resize( t.h.probeCount );
	std::memcpy( t.probes.data(), p, t.probes.size() * 144 );
	p += t.probes.size() * 144;
	t.links.resize( t.h.linkCount );
	std::memcpy( t.links.data(), p, t.links.size() * 12 );
	p += t.links.size() * 12;
	t.back.clear();
	t.lext.clear();
	t.pext.clear();
	t.boxes.clear();
	if ( v4 ) {
		t.back.resize( t.h.reserved[0] );
		std::memcpy( t.back.data(), p, t.back.size() * 32 );
		p += t.back.size() * 32;
		t.lext.resize( t.h.linkCount );
		std::memcpy( t.lext.data(), p, t.lext.size() * 8 );
		p += t.lext.size() * 8;
		t.pext.resize( t.h.probeCount );
		std::memcpy( t.pext.data(), p, t.pext.size() * 32 );
		p += t.pext.size() * 32;
		t.boxes.resize( t.h.reserved[1] );
		std::memcpy( t.boxes.data(), p, t.boxes.size() * 32 );
	}
	t.name = QFileInfo( path ).fileName();
	return true;
}

// the reader's key: float division, then floor
inline qint32 floorDiv( float v, float s )
{
	return qint32( std::floor( v / s ) );
}

struct Key3
{
	qint32 x, y, z;
	bool operator==( const Key3 & o ) const { return x == o.x && y == o.y && z == o.z; }
};
struct Key3Hash
{
	size_t operator()( const Key3 & k ) const
	{
		return size_t( quint32( k.x ) * 73856093u ^ quint32( k.y ) * 19349663u ^ quint32( k.z ) * 83492791u );
	}
};

struct SurfKey
{
	quint32 v[6];
	bool operator==( const SurfKey & o ) const { return std::memcmp( v, o.v, sizeof v ) == 0; }
};
struct SurfKeyHash
{
	size_t operator()( const SurfKey & k ) const
	{
		size_t h = 1469598103u;
		for ( quint32 x : k.v )
			h = ( h ^ x ) * 16777619u;
		return h;
	}
};

void unpackOct( const qint16 d[2], double out[3] )
{
	double x = d[0] / 32767.0, y = d[1] / 32767.0;
	double z = 1.0 - std::fabs( x ) - std::fabs( y );
	if ( z < 0.0 ) {
		const double nx = ( 1.0 - std::fabs( y ) ) * ( x >= 0.0 ? 1.0 : -1.0 );
		const double ny = ( 1.0 - std::fabs( x ) ) * ( y >= 0.0 ? 1.0 : -1.0 );
		x = nx;
		y = ny;
	}
	const double n = std::sqrt( x * x + y * y + z * z );
	out[0] = x / n;
	out[1] = y / n;
	out[2] = z / n;
}

// PRTP2 section 1 (the shader's cellRadial)
double radial( double d, double r, const WwCellLight & l )
{
	const double x = std::min( std::max( d / std::max( r, 0.001 ), 0.0 ), 1.0 );
	const double xe = l.exponent > 0.0f ? std::pow( x, double( l.exponent ) ) : 1.0;
	const double k = 1.0 - std::min( std::max( l.scale * xe + l.bias, 0.0 ), 1.0 );
	return std::pow( k, 2.2 );
}

void parallelFor( size_t n, const std::function<void( size_t )> & fn )
{
	const int nThreads = std::max( 1, int( std::thread::hardware_concurrency() ) );
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

const double kAxes[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

}	// namespace

bool probeGiRelight( const ProbeSoup & soup, const QString & bakeDir, const WwCellLighting & lighting,
	const ProbeGiSpec & spec, ProbeGiResult * out )
{
	ProbeGiResult & R = *out;
	R = ProbeGiResult();
	QElapsedTimer clock;
	clock.start();
	const bool redNoShadow = spec.red == QLatin1String( "noshadow" );
	const bool redNoVis = spec.red == QLatin1String( "novis" );
	const bool redFlip = spec.red == QLatin1String( "flip" );
	// lane SKY1: the weather's sky and sun, outdoors; lane SKYINT1: and in an interior that shows the sky
	const bool skyOn = spec.sky.on && ( !lighting.interior || spec.interiorSky ) && spec.skyRed != QLatin1String( "off" );
	const bool redSkyNoVis = spec.skyRed == QLatin1String( "novis" );
	const bool redSkyNoTint = spec.skyRed == QLatin1String( "notint" );
	const bool redSunThrough = spec.skyRed == QLatin1String( "sunthrough" );
	const bool sunOn = skyOn && ( !lighting.interior || spec.interiorSun ) && spec.sky.sunTo[2] > 0.0f
		&& ( spec.sky.sun[0] > 0.0f || spec.sky.sun[1] > 0.0f || spec.sky.sun[2] > 0.0f );
	R.skyLit = skyOn;
	R.skyLabel = spec.sky.label;

	// ---- the bake's files, by name (the gate reads them in the same order)
	const QStringList names = QDir( bakeDir ).entryList( { QStringLiteral( "sector_*.tbk" ) }, QDir::Files, QDir::Name );
	std::vector<Tbk> tbks( size_t( names.size() ) );
	for ( int i = 0; i < names.size(); i++ )
		if ( !readTbk( QDir( bakeDir ).filePath( names[i] ), tbks[size_t( i )], &R.error ) )
			return false;
	if ( tbks.empty() ) {
		R.error = QStringLiteral( "no sector_*.tbk in %1" ).arg( bakeDir );
		return false;
	}
	R.files = int( tbks.size() );

	// ---- the soup's tracer, local to the soup's middle (as the bake builds it)
	double O[2] = { 0, 0 };
	{
		double mn[2] = { 1e300, 1e300 }, mx[2] = { -1e300, -1e300 };
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 2; k++ ) {
				mn[k] = std::min( mn[k], double( soup.tris[i + size_t( k )] ) );
				mx[k] = std::max( mx[k], double( soup.tris[i + size_t( k )] ) );
			}
		if ( !soup.tris.empty() )
			for ( int k = 0; k < 2; k++ )
				O[k] = std::floor( ( mn[k] + mx[k] ) * 0.5 );
	}
	probebvh::Bvh bvh;
	bvh.t.resize( soup.tris.size() );
	for ( size_t i = 0; i < soup.tris.size(); i += 3 ) {
		bvh.t[i + 0] = float( double( soup.tris[i + 0] ) - O[0] );
		bvh.t[i + 1] = float( double( soup.tris[i + 1] ) - O[1] );
		bvh.t[i + 2] = soup.tris[i + 2];
	}
	bvh.build();
	bvh.mask = &soup.amask;   // lane ALPHATEST1: light passes an alpha-test hole
	auto blocked = [&]( const double p[3], const double q[3], double clearEnd ) -> bool {
		double d[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
		const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
		const double tmax = len - clearEnd;
		if ( tmax <= 1e-3 )
			return false;
		for ( double & c : d )
			c /= len;
		const double o[3] = { p[0] - O[0], p[1] - O[1], p[2] };
		double tHit;
		return bvh.ray( o, d, tmax, &tHit );
	};

	// ---- 1. the surfels, one per position + normal across the files, lit
	std::unordered_map<SurfKey, int, SurfKeyHash> uniq;
	std::vector<std::vector<int>> fileSurfel( tbks.size() ), fileBack( tbks.size() );   // lane BAKE4: + v4 back surfels
	struct US { double p[3], n[3], a[3]; double B[3]; double S[3] = { 0, 0, 0 }; };   // S: the sun's part of B (lane SKY1)
	std::vector<US> us;
	for ( size_t f = 0; f < tbks.size(); f++ ) {
		for ( size_t si = 0; si < tbks[f].surfels.size() + tbks[f].back.size(); si++ ) {
			const bool isBack = si >= tbks[f].surfels.size();
			const TbkSurfel & s = isBack ? tbks[f].back[si - tbks[f].surfels.size()] : tbks[f].surfels[si];
			SurfKey k;
			std::memcpy( k.v, s.position, 12 );
			k.v[3] = quint32( quint16( s.normal[0] ) );
			k.v[4] = quint32( quint16( s.normal[1] ) );
			k.v[5] = quint32( quint16( s.normal[2] ) );
			auto it = uniq.find( k );
			int idx;
			if ( it == uniq.end() ) {
				idx = int( us.size() );
				uniq.emplace( k, idx );
				US u;
				double nl = 0.0;
				for ( int c = 0; c < 3; c++ ) {
					u.p[c] = s.position[c];
					u.n[c] = s.normal[c] / 32767.0;
					nl += u.n[c] * u.n[c];
					u.a[c] = s.albedo[c] / 255.0;
					u.B[c] = 0.0;
				}
				nl = std::sqrt( std::max( nl, 1e-12 ) );
				for ( double & c : u.n )
					c /= nl;
				us.push_back( u );
			} else {
				idx = it->second;
			}
			( isBack ? fileBack : fileSurfel )[f].push_back( idx );
		}
	}
	R.surfels = int( us.size() );
	if ( !tbks.empty() )
		R.surfelCell = tbks[0].h.surfelCellSize;	// lane PROBEVIEW1
	const QVector<WwCellLight> & lights = lighting.lights;
	std::atomic<qint64> rays( 0 ), shadowed( 0 );
	std::atomic<int> lit( 0 );
	std::atomic<qint64> sunRays( 0 ), sunBlocked( 0 );
	std::atomic<int> sunLit( 0 );
	parallelFor( us.size(), [&]( size_t i ) {
		US & u = us[i];
		const double o[3] = { u.p[0] + u.n[0] * 2.0, u.p[1] + u.n[1] * 2.0, u.p[2] + u.n[2] * 2.0 };
		double E[3] = { 0, 0, 0 };
		qint64 nr = 0, nb = 0;
		bool any = false;
		for ( const WwCellLight & l : lights ) {
			const double Lv[3] = { l.pos[0] - u.p[0], l.pos[1] - u.p[1], l.pos[2] - u.p[2] };
			const double d = std::sqrt( Lv[0] * Lv[0] + Lv[1] * Lv[1] + Lv[2] * Lv[2] );
			if ( d >= l.radius || !wwCellLightShapeIn( l, u.p[0], u.p[1], u.p[2] ) )	// lane HEMI1: the volume
				continue;
			const double inv = 1.0 / std::max( d, 0.001 );
			const double L[3] = { Lv[0] * inv, Lv[1] * inv, Lv[2] * inv };
			const double nl = u.n[0] * L[0] + u.n[1] * L[1] + u.n[2] * L[2];
			if ( nl <= 0.0 )
				continue;
			double a = radial( d, l.radius, l );
			if ( l.spot ) {
				const double dl = -( L[0] * l.dir[0] + L[1] * l.dir[1] + L[2] * l.dir[2] );
				const double base = std::min( std::max( 1.0 - ( 1.0 - dl ) / std::max( 1.0 - l.cosOuter, 1e-4 ), 0.0 ), 1.0 );
				a *= std::min( std::pow( base, std::max( double( l.cone ), 1e-3 ) ), 1.0 );
			}
			if ( a * nl <= 0.0 )
				continue;
			if ( !redNoShadow ) {
				const double q[3] = { l.pos[0], l.pos[1], l.pos[2] };
				nr++;
				if ( blocked( o, q, spec.fixtureClear ) ) {
					nb++;
					continue;
				}
			}
			any = true;
			for ( int c = 0; c < 3; c++ )
				E[c] += l.color[c] * a * nl;
		}
		if ( lighting.interior && lighting.hasDirectional ) {
			double dl = std::sqrt( double( lighting.dirTo[0] ) * lighting.dirTo[0] + double( lighting.dirTo[1] ) * lighting.dirTo[1]
				+ double( lighting.dirTo[2] ) * lighting.dirTo[2] );
			const double nl = ( u.n[0] * lighting.dirTo[0] + u.n[1] * lighting.dirTo[1] + u.n[2] * lighting.dirTo[2] ) / std::max( dl, 1e-9 );
			if ( nl > 0.0 )
				for ( int c = 0; c < 3; c++ )
					E[c] += lighting.dirColor[c] * nl;
		}
		// lane SKY1: the sun, behind one ray through the soup (to beyond anything loaded)
		if ( sunOn ) {
			const double nl = u.n[0] * spec.sky.sunTo[0] + u.n[1] * spec.sky.sunTo[1] + u.n[2] * spec.sky.sunTo[2];
			if ( nl > 0.0 ) {
				bool hit = false;
				if ( !redSunThrough ) {
					const double reach = 400000.0;
					const double q[3] = { o[0] + spec.sky.sunTo[0] * reach, o[1] + spec.sky.sunTo[1] * reach,
						o[2] + spec.sky.sunTo[2] * reach };
					sunRays++;
					hit = blocked( o, q, 0.0 );
					if ( hit )
						sunBlocked++;
				}
				if ( !hit ) {
					sunLit++;
					for ( int c = 0; c < 3; c++ ) {
						u.S[c] = u.a[c] * spec.sky.sun[c] * nl;
						E[c] += spec.sky.sun[c] * nl;
					}
				}
			}
		}
		for ( int c = 0; c < 3; c++ )
			u.B[c] = u.a[c] * E[c];
		rays += nr;
		shadowed += nb;
		if ( any )
			lit++;
	} );
	R.shadowRays = rays;
	R.shadowBlocked = shadowed;
	R.surfelsLit = lit;
	R.surfelsSun = sunLit;
	R.sunRays = sunRays;
	R.sunBlocked = sunBlocked;
	R.msLight = clock.nsecsElapsed() / 1e6;

	// ---- 2. every probe's ambient cube from its links
	struct UP { double p[3]; double E[6][3]; double sky[6]; double S[6][3]; double kUnl = 1.0; quint32 room[2] = { 0, 0xFFFFFFFFu }; };   // S: the sky's part of E (lane SKY1)
	std::vector<UP> up;
	struct LK { double omega, cosA[6], tint[3]; };   // lane BOUNCE2: a resolved link, for the later passes' gathers
	std::vector<LK> lks;
	double skyVisSum = 0.0;
	R.probeLinkStart.assign( 1, 0 );	// lane PROBEVIEW1
	for ( size_t f = 0; f < tbks.size(); f++ ) {
		const Tbk & t = tbks[f];
		const float cs = t.h.surfelCellSize;
		std::unordered_map<Key3, int, Key3Hash> keys, keysBack;   // lane BAKE4: the back side's own map
		for ( size_t i = 0; i < t.surfels.size(); i++ ) {
			const TbkSurfel & s = t.surfels[i];
			keys.emplace( Key3 { floorDiv( s.position[0], cs ), floorDiv( s.position[1], cs ), floorDiv( s.position[2], cs ) }, int( i ) );
		}
		for ( size_t i = 0; i < t.back.size(); i++ ) {
			const TbkSurfel & s = t.back[i];
			keysBack.emplace( Key3 { floorDiv( s.position[0], cs ), floorDiv( s.position[1], cs ), floorDiv( s.position[2], cs ) }, int( i ) );
		}
		for ( const TbkProbe & pr : t.probes ) {
			UP P;
			for ( int c = 0; c < 3; c++ )
				P.p[c] = pr.position[c];
			std::memset( P.E, 0, sizeof P.E );
			std::memset( P.S, 0, sizeof P.S );
			/* lane PROBEVIEW1: the open-sky share on each axis = the mean of the four octants on its side
			 * (octant = x<0 | (y<0)<<1 | (z<0)<<2, the bake's skyVis) */
			for ( int a = 0; a < 6; a++ ) {
				const int bit = 1 << ( a / 2 ), want = ( a & 1 ) ? bit : 0;
				double sum = 0.0;
				for ( int o = 0; o < 8; o++ )
					if ( ( o & bit ) == want )
						sum += double( pr.skyVis[o] );
				P.sky[a] = std::min( std::max( sum * 0.25, 0.0 ), 1.0 );
			}
			const Key3 pk { floorDiv( pr.position[0], cs ), floorDiv( pr.position[1], cs ), floorDiv( pr.position[2], cs ) };
			double linked = 0.0;
			for ( quint32 j = 0; j < pr.linkCount; j++ ) {
				const TbkLink & lk = t.links[size_t( pr.linkOffset + j )];
				R.links++;
				// lane BAKE4: a v4 link names its side and carries the glass tint on its way
				const size_t li = size_t( pr.linkOffset + j );
				const bool sideBack = li < t.lext.size() && t.lext[li].side;
				const auto & km = sideBack ? keysBack : keys;
				auto it = km.find( Key3 { pk.x + lk.cellDelta[0], pk.y + lk.cellDelta[1], pk.z + lk.cellDelta[2] } );
				if ( it == km.end() ) {
					R.linksUnresolved++;
					continue;
				}
				const int ui = ( sideBack ? fileBack : fileSurfel )[f][size_t( it->second )];
				const US & u = us[size_t( ui )];
				R.probeLinks.push_back( ui );
				double tint[3] = { 1, 1, 1 };
				if ( li < t.lext.size() )
					for ( int c = 0; c < 3; c++ )
						tint[c] = t.lext[li].tint[c] / 255.0;
				double dir[3];
				unpackOct( lk.dir, dir );
				if ( redFlip )
					for ( double & c : dir )
						c = -c;
				const double w = double( lk.weight ) * pr.linkWeightScale;
				linked += w;
				const double omega = w * kFourPi;
				LK rec;
				rec.omega = omega;
				for ( int c = 0; c < 3; c++ )
					rec.tint[c] = tint[c];
				for ( int a = 0; a < 6; a++ ) {
					const double cosA = std::max( kAxes[a][0] * dir[0] + kAxes[a][1] * dir[1] + kAxes[a][2] * dir[2], 0.0 );
					rec.cosA[a] = cosA;
					for ( int c = 0; c < 3; c++ )
						P.E[a][c] += u.B[c] * tint[c] * omega * cosA;
				}
				lks.push_back( rec );
			}
			// the unlinked share (void, dropped links) sees what the linked surfaces see on average
			if ( linked > 0.0 && pr.unlinkedWeight > 0.0f ) {
				const double k = ( linked + pr.unlinkedWeight ) / linked;
				P.kUnl = k;
				for ( auto & e : P.E )
					for ( double & c : e )
						c *= k;
			}
			{	// lane BOUNCE2: the probe's room, and an opening's second room
				const size_t pi = size_t( &pr - t.probes.data() );
				if ( pi < t.pext.size() ) {
					P.room[0] = t.pext[pi].room[0];
					P.room[1] = t.pext[pi].room[1];
				}
			}
			// lane SKY1: the sky this probe sees, beside what its surfaces send (src/probesky.h)
			if ( skyOn ) {
				const size_t pi = size_t( &pr - t.probes.data() );
				const TbkProbeExt * px = pi < t.pext.size() ? &t.pext[pi] : nullptr;
				probeSkyCube( pr.skyVis, px ? px->skyTint : nullptr, spec.sky, redSkyNoVis, redSkyNoTint, P.S );
				double vs = 0.0;
				bool tinted = false;
				for ( int o = 0; o < 8; o++ ) {
					vs += pr.skyVis[o];
					if ( px && pr.skyVis[o] > 0.0f )
						tinted = tinted || px->skyTint[o][0] != 255 || px->skyTint[o][1] != 255 || px->skyTint[o][2] != 255;
				}
				skyVisSum += vs / 8.0;
				R.probesSky += vs > 0.0 ? 1 : 0;
				R.probesTinted += tinted ? 1 : 0;
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						P.E[a][c] += P.S[a][c];
			}
			up.push_back( P );
			R.probeLinkStart.push_back( int( R.probeLinks.size() ) );
		}
	}
	R.probes = int( up.size() );
	R.skyVisMean = up.empty() ? 0.0 : skyVisSum / double( up.size() );
	R.msGather = clock.nsecsElapsed() / 1e6 - R.msLight;

	// ---- 3. the voxel grid, each voxel blending the probes it can see
	double mn[3] = { 1e300, 1e300, 1e300 }, mx[3] = { -1e300, -1e300, -1e300 };
	for ( const US & u : us )
		for ( int c = 0; c < 3; c++ ) {
			mn[c] = std::min( mn[c], u.p[c] );
			mx[c] = std::max( mx[c], u.p[c] );
		}
	if ( us.empty() || up.empty() ) {
		R.error = QStringLiteral( "the bake holds %1 surfels and %2 probes" ).arg( us.size() ).arg( up.size() );
		return false;
	}
	double v = spec.voxelMin;
	int dims[3];
	for ( ;; ) {
		qint64 total = 6;
		for ( int c = 0; c < 3; c++ ) {
			dims[c] = int( std::ceil( ( mx[c] - mn[c] ) / v ) ) + 4;
			total *= dims[c];
		}
		if ( total / 6 <= spec.maxVoxels && dims[2] * 6 <= 2048 && dims[0] <= 2048 && dims[1] <= 2048 )
			break;
		v *= 1.1;
	}
	R.voxel = float( v );
	for ( int c = 0; c < 3; c++ ) {
		R.dims[c] = dims[c];
		R.origin[c] = float( std::floor( mn[c] - 2.0 * v ) );
	}
	// the blend radius from the probes' spacing
	{
		std::vector<double> nn;
		const size_t step = std::max<size_t>( 1, up.size() / 4000 );
		for ( size_t i = 0; i < up.size(); i += step ) {
			double best = 1e300;
			for ( size_t j = 0; j < up.size(); j++ ) {
				if ( j == i )
					continue;
				double d2 = 0.0;
				for ( int c = 0; c < 3; c++ )
					d2 += ( up[i].p[c] - up[j].p[c] ) * ( up[i].p[c] - up[j].p[c] );
				best = std::min( best, d2 );
			}
			if ( best < 1e300 )
				nn.push_back( std::sqrt( best ) );
		}
		std::sort( nn.begin(), nn.end() );
		const double med = nn.empty() ? 256.0 : nn[nn.size() / 2];
		R.radius = float( std::max( med * spec.radiusScale, 2.0 * v ) );
	}
	const double rad = R.radius;
	std::unordered_map<Key3, std::vector<int>, Key3Hash> ph;
	for ( size_t i = 0; i < up.size(); i++ )
		ph[Key3 { floorDiv( float( up[i].p[0] ), float( rad ) ), floorDiv( float( up[i].p[1] ), float( rad ) ),
			floorDiv( float( up[i].p[2] ), float( rad ) ) }].push_back( int( i ) );

	/* ---- lane BOUNCE2: more than one bounce (probegi.h). What each surfel reads: the probes within the radius
	 * it can see from its own point and that stand in its room, else the closest such within twice the radius. */
	const double msBefore = clock.nsecsElapsed() / 1e6;
	const bool redRooms = spec.red == QLatin1String( "rooms" );
	const bool redGrow = spec.red == QLatin1String( "grow" );
	{
		std::vector<TbkRoomBox> boxes;
		for ( const Tbk & t : tbks )
			boxes.insert( boxes.end(), t.boxes.begin(), t.boxes.end() );
		R.surfelRoom.assign( us.size(), -1 );
		R.feedStart.assign( us.size() + 1, 0 );
		std::vector<std::vector<std::pair<int, float>>> feed( us.size() );
		std::atomic<qint64> fr( 0 ), fb( 0 ), fo( 0 );
		std::atomic<int> roomed( 0 ), closest( 0 );
		parallelFor( us.size(), [&]( size_t i ) {
			const US & u = us[i];
			// the room: the nearest room box to the surfel's air (17.5 units out), within one placement voxel (35)
			const double qa[3] = { u.p[0] + u.n[0] * 17.5, u.p[1] + u.n[1] * 17.5, u.p[2] + u.n[2] * 17.5 };
			double best = 35.0 * 35.0;
			qint64 room = -1;
			for ( const TbkRoomBox & b : boxes ) {
				double d2 = 0.0;
				for ( int c = 0; c < 3; c++ ) {
					const double e = std::max( std::max( double( b.lo[c] ) - qa[c], qa[c] - double( b.hi[c] ) ), 0.0 );
					d2 += e * e;
				}
				if ( d2 <= best ) {
					best = d2;
					room = qint64( b.room );
				}
			}
			R.surfelRoom[i] = int( room );
			if ( room >= 0 )
				roomed++;
			const double o[3] = { u.p[0] + u.n[0] * 2.0, u.p[1] + u.n[1] * 2.0, u.p[2] + u.n[2] * 2.0 };
			const Key3 ck { floorDiv( float( o[0] ), float( rad ) ), floorDiv( float( o[1] ), float( rad ) ), floorDiv( float( o[2] ), float( rad ) ) };
			std::vector<std::pair<double, int>> cand;
			for ( int dz = -2; dz <= 2; dz++ )
				for ( int dy = -2; dy <= 2; dy++ )
					for ( int dx = -2; dx <= 2; dx++ ) {
						auto it = ph.find( Key3 { ck.x + dx, ck.y + dy, ck.z + dz } );
						if ( it == ph.end() )
							continue;
						for ( int j : it->second ) {
							double d2 = 0.0;
							for ( int a = 0; a < 3; a++ )
								d2 += ( up[size_t( j )].p[a] - o[a] ) * ( up[size_t( j )].p[a] - o[a] );
							if ( d2 < 4.0 * rad * rad )
								cand.emplace_back( d2, j );
						}
					}
			std::sort( cand.begin(), cand.end() );
			qint64 nr = 0, nb = 0, no = 0;
			for ( const auto & c : cand ) {
				const bool inRadius = c.first < rad * rad;
				if ( !inRadius && !feed[i].empty() )
					break;
				const UP & P = up[size_t( c.second )];
				if ( !redRooms ) {
					if ( room >= 0 && quint32( room ) != P.room[0] && quint32( room ) != P.room[1] ) {
						no++;
						continue;
					}
					nr++;
					if ( blocked( o, P.p, 0.0 ) ) {
						nb++;
						continue;
					}
				}
				if ( inRadius ) {
					const double t = 1.0 - c.first / ( rad * rad );
					feed[i].emplace_back( c.second, float( t * t ) );
				} else {
					feed[i].emplace_back( c.second, 1.0f );
					closest++;
					break;
				}
			}
			fr += nr;
			fb += nb;
			fo += no;
		} );
		R.surfelsRoomed = roomed;
		R.fedClosest = closest;
		R.feedRays = fr;
		R.feedBlocked = fb;
		R.feedOtherRoom = fo;
		for ( size_t i = 0; i < us.size(); i++ ) {
			R.surfelsFed += feed[i].empty() ? 0 : 1;
			for ( const auto & e : feed[i] ) {
				R.feedProbe.push_back( e.first );
				R.feedWeight.push_back( e.second );
			}
			R.feedStart[i + 1] = int( R.feedProbe.size() );
		}
	}
	// the passes: pass 1 is the relight above; pass k re-lights every surfel with its probes' light of pass k - 1
	std::vector<double> Bk( us.size() * 3 );
	for ( size_t i = 0; i < us.size(); i++ )
		for ( int c = 0; c < 3; c++ )
			Bk[i * 3 + size_t( c )] = us[i].B[c];
	double sum1 = 0.0, max1 = 0.0;
	for ( double b : Bk ) {
		sum1 += b;
		max1 = std::max( max1, b );
	}
	R.passLog = { 0.0, sum1, max1 };
	const int cap = spec.passes > 0 ? spec.passes : std::max( 1, spec.maxPasses );
	R.passes = 1;
	R.settled = spec.passes > 0;
	for ( int pass = 2; pass <= cap; pass++ ) {
		std::vector<double> Bn( Bk.size() );
		parallelFor( us.size(), [&]( size_t i ) {
			const US & u = us[i];
			double E[3] = { 0, 0, 0 }, ws = 0.0;
			for ( int k = R.feedStart[i]; k < R.feedStart[i + 1]; k++ ) {
				const UP & P = up[size_t( R.feedProbe[size_t( k )] )];
				const double w = R.feedWeight[size_t( k )];
				ws += w;
				// the shader's sample: the facing axis of each pair, weighted by n^2
				for ( int a = 0; a < 3; a++ ) {
					const int ax = 2 * a + ( u.n[a] >= 0.0 ? 0 : 1 );
					for ( int c = 0; c < 3; c++ )
						E[c] += w * u.n[a] * u.n[a] * P.E[ax][c];
				}
			}
			for ( int c = 0; c < 3; c++ ) {
				const double a = redGrow ? 1.5 : u.a[c];
				Bn[i * 3 + size_t( c )] = u.B[c] + ( ws > 0.0 ? a * ( E[c] / ws ) / kPi : 0.0 );
			}
		} );
		double ch = 0.0, sum = 0.0, mx = 0.0;
		for ( size_t k = 0; k < Bn.size(); k++ ) {
			ch = std::max( ch, std::fabs( Bn[k] - Bk[k] ) );
			sum += Bn[k];
			mx = std::max( mx, Bn[k] );
		}
		Bk.swap( Bn );
		// every probe gathers its links again (the expression of step 2, in its order)
		size_t li = 0;
		for ( size_t j = 0; j < up.size(); j++ ) {
			UP & P = up[j];
			std::memset( P.E, 0, sizeof P.E );
			for ( int k = R.probeLinkStart[j]; k < R.probeLinkStart[j + 1]; k++, li++ ) {
				const LK & L = lks[li];
				const double * B = &Bk[size_t( R.probeLinks[size_t( k )] ) * 3];
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						P.E[a][c] += B[c] * L.tint[c] * L.omega * L.cosA[a];
			}
			if ( P.kUnl != 1.0 )
				for ( auto & e : P.E )
					for ( double & c : e )
						c *= P.kUnl;
			if ( skyOn )
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						P.E[a][c] += P.S[a][c];
		}
		R.passes = pass;
		R.passLog.insert( R.passLog.end(), { ch, sum, mx } );
		if ( spec.passes <= 0 && ch <= spec.settle * mx ) {
			R.settled = true;
			break;
		}
	}
	R.gain = sum1 > 0.0 ? R.passLog[R.passLog.size() - 2] / sum1 : 1.0;
	R.surfelBounce.assign( Bk.begin(), Bk.end() );
	R.msBounce = clock.nsecsElapsed() / 1e6 - msBefore;

	// the voxels a fragment on a surfel's surface samples (the shader's normal offset, then +-1)
	const size_t nVox = size_t( dims[0] ) * size_t( dims[1] ) * size_t( dims[2] );
	std::vector<quint8> near( nVox, 0 );
	// lane ROOMCLAMP1: the rooms (src/proberooms.h), rebuilt from the soup; red "noclamp": none (every surface,
	// probe and slot then has the room -1 and the blend is the one before the lane)
	R.roomsOn = spec.red != QLatin1String( "noclamp" ) && spec.red != QLatin1String( "prelane" )
		&& probeRoomsBuild( soup, spec.rooms, &R.rooms );   // "prelane": the gates' pin (WW_CELL_ROOMCLAMP_PIN=off)
	const ProbeRooms & RM = R.rooms;
	// every probe's rooms: its cell's, or in a solid cell the nearest air cell's within two cells
	std::vector<std::array<int, 2>> probeRoom( up.size(), { { -1, -1 } } );
	if ( R.roomsOn ) {
		std::vector<std::array<int, 3>> offs;
		for ( int dz = -2; dz <= 2; dz++ )
			for ( int dy = -2; dy <= 2; dy++ )
				for ( int dx = -2; dx <= 2; dx++ )
					offs.push_back( { dx, dy, dz } );
		std::stable_sort( offs.begin(), offs.end(), []( const std::array<int, 3> & a, const std::array<int, 3> & b ) {
			return a[0] * a[0] + a[1] * a[1] + a[2] * a[2] < b[0] * b[0] + b[1] * b[1] + b[2] * b[2];
		} );
		for ( size_t j = 0; j < up.size(); j++ ) {
			for ( const auto & o : offs ) {
				const double q[3] = { up[j].p[0] + o[0] * RM.cell, up[j].p[1] + o[1] * RM.cell, up[j].p[2] + o[2] * RM.cell };
				int r2[2];
				if ( RM.at( q, r2 ) && r2[0] >= 0 ) {
					probeRoom[j] = { r2[0], r2[1] };
					break;
				}
			}
			R.probesRoomless += probeRoom[j][0] < 0;
		}
		for ( const auto & pr : probeRoom )
			R.probeRooms.insert( R.probeRooms.end(), { pr[0], pr[1] } );
	}
	// lane ROOMCLAMP1: per near voxel, the rooms of the surfaces reading it (at most four), how many read it from
	// each, and each room's eye: its sample point (surface + half a voxel along its normal) nearest the voxel's
	// centre, where a fragment of that room reading the voxel stands
	struct VS { int lab[4] = { -1, -1, -1, -1 }; int n[4] = { 0, 0, 0, 0 }; float eye[4][3]; float eyeD[4] = { 1e30f, 1e30f, 1e30f, 1e30f }; int used = 0; };
	std::vector<VS> vs( nVox );
	for ( const US & u : us ) {
		int g[3];
		double s[3];
		for ( int c = 0; c < 3; c++ ) {
			s[c] = u.p[c] + u.n[c] * v * 0.5;
			g[c] = int( std::floor( ( s[c] - R.origin[c] ) / v ) );
		}
		int L[2] = { -1, -1 };
		if ( R.roomsOn )
			RM.surface( u.p, u.n, L );
		for ( int dz = -1; dz <= 1; dz++ )
			for ( int dy = -1; dy <= 1; dy++ )
				for ( int dx = -1; dx <= 1; dx++ ) {
					const int x = g[0] + dx, y = g[1] + dy, z = g[2] + dz;
					if ( x < 0 || y < 0 || z < 0 || x >= dims[0] || y >= dims[1] || z >= dims[2] )
						continue;
					const size_t i = ( size_t( z ) * size_t( dims[1] ) + size_t( y ) ) * size_t( dims[0] ) + size_t( x );
					near[i] = 1;
					const int gi[3] = { x, y, z };
					double d2 = 0.0;
					for ( int c = 0; c < 3; c++ ) {
						const double e = s[c] - ( R.origin[c] + ( gi[c] + 0.5 ) * v );
						d2 += e * e;
					}
					VS & V = vs[i];
					for ( int li = 0; li < ( L[1] >= 0 ? 2 : 1 ); li++ ) {
						int k = 0;
						while ( k < V.used && V.lab[k] != L[li] )
							k++;
						if ( k == V.used ) {
							if ( V.used == 4 )
								continue;
							V.lab[V.used++] = L[li];
						}
						V.n[k]++;
						if ( d2 < V.eyeD[k] ) {
							V.eyeD[k] = float( d2 );
							for ( int c = 0; c < 3; c++ )
								V.eye[k][c] = float( s[c] );
						}
					}
				}
	}
	// the two slots: the rooms read most (first seen first on a tie); a known room before the surfaces that found
	// none (-1: the shader's room blend never matches it, only the plain blend reads slot 0)
	std::vector<std::array<int, 2>> slotOf( nVox, { { -1, -1 } } );   // index into VS, -1 none
	for ( size_t i = 0; i < nVox; i++ ) {
		const VS & V = vs[i];
		int b0 = -1, b1 = -1;
		auto more = [&]( int k, int than ) {
			return than < 0 || ( V.lab[k] >= 0 ) > ( V.lab[than] >= 0 )
				|| ( ( V.lab[k] >= 0 ) == ( V.lab[than] >= 0 ) && V.n[k] > V.n[than] );
		};
		for ( int k = 0; k < V.used; k++ ) {
			if ( more( k, b0 ) ) {
				b1 = b0;
				b0 = k;
			} else if ( more( k, b1 ) )
				b1 = k;
		}
		if ( b1 >= 0 && V.lab[b1] < 0 )
			b1 = -1;
		slotOf[i] = { b0, b1 };
	}
	const bool redNoEye = spec.red == QLatin1String( "noeye" ) || spec.red == QLatin1String( "prelane" );
	std::atomic<int> nEye( 0 ), nFar( 0 ), nBare( 0 ), nTwo( 0 ), nTwoBare( 0 ), nElse( 0 );
	std::vector<size_t> todo;
	for ( size_t i = 0; i < nVox; i++ )
		if ( near[i] )
			todo.push_back( i );
	R.voxelsNear = int( todo.size() );
	R.grid.assign( nVox * 6 * 4, 0.0f );
	R.gridSky.assign( nVox * 6 * 4, 0.0f );	// lane PROBEVIEW1: the same voxels, the same weights
	if ( R.roomsOn ) {
		R.grid2.assign( nVox * 6 * 4, 0.0f );
		R.gridSky2.assign( nVox * 6 * 4, 0.0f );
		R.slotRooms.assign( nVox, probeRoomsPack( -1, -1 ) );
	}
	std::atomic<qint64> vr( 0 ), vb( 0 );
	std::atomic<int> valid( 0 );
	parallelFor( todo.size(), [&]( size_t k ) {
		const size_t i = todo[k];
		const int x = int( i % size_t( dims[0] ) ), y = int( ( i / size_t( dims[0] ) ) % size_t( dims[1] ) ),
			z = int( i / ( size_t( dims[0] ) * size_t( dims[1] ) ) );
		const double c[3] = { R.origin[0] + ( x + 0.5 ) * v, R.origin[1] + ( y + 0.5 ) * v, R.origin[2] + ( z + 0.5 ) * v };
		int cr[2] = { -1, -1 };
		if ( R.roomsOn )
			RM.at( c, cr );
		const VS & V = vs[i];
		qint64 nr = 0, nb = 0;
		int labs[2] = { -1, -1 };
		for ( int slot = 0; slot < 2; slot++ ) {
			const int si = slotOf[i][size_t( slot )];
			if ( si < 0 )
				continue;
			const int lab = V.lab[si];
			labs[slot] = lab;
			if ( slot == 1 )
				nTwo++;
			double acc[6][3] = {}, accSky[6] = {}, wsum = 0.0;
			// the probes of room `lab` (any: -1) within r of o that o sees, weighted (1 - d^2/r^2)^2
			auto gather = [&]( const double o[3], double r ) {
				const Key3 ck { floorDiv( float( o[0] ), float( rad ) ), floorDiv( float( o[1] ), float( rad ) ), floorDiv( float( o[2] ), float( rad ) ) };
				const int reach = int( std::ceil( r / rad ) );
				for ( int dz = -reach; dz <= reach; dz++ )
					for ( int dy = -reach; dy <= reach; dy++ )
						for ( int dx = -reach; dx <= reach; dx++ ) {
							auto it = ph.find( Key3 { ck.x + dx, ck.y + dy, ck.z + dz } );
							if ( it == ph.end() )
								continue;
							for ( int j : it->second ) {
								if ( lab >= 0 && probeRoom[size_t( j )][0] != lab && probeRoom[size_t( j )][1] != lab )
									continue;
								const UP & P = up[size_t( j )];
								double d2 = 0.0;
								for ( int a = 0; a < 3; a++ )
									d2 += ( P.p[a] - o[a] ) * ( P.p[a] - o[a] );
								if ( d2 >= r * r )
									continue;
								if ( !redNoVis ) {
									nr++;
									if ( blocked( o, P.p, 0.0 ) ) {
										nb++;
										continue;
									}
								}
								const double t = 1.0 - d2 / ( r * r );
								const double w = t * t;
								wsum += w;
								for ( int a = 0; a < 6; a++ ) {
									for ( int ch = 0; ch < 3; ch++ )
										acc[a][ch] += w * P.E[a][ch];
									accSky[a] += w * P.sky[a];
								}
							}
						}
			};
			const double e[3] = { V.eye[si][0], V.eye[si][1], V.eye[si][2] };
			// from the centre when it stands in the slot's room (or no room is known), else from the slot's eye
			const bool fromCentre = lab < 0 || cr[0] == lab || cr[1] == lab;
			if ( !fromCentre )
				nElse++;
			gather( fromCentre ? c : e, rad );
			// lane ROOMCLAMP1: blocked at the centre (inside a wall, or behind it): from the voxel's eye, then twice as far
			if ( wsum <= 0.0 && !redNoEye ) {
				if ( fromCentre ) {
					gather( e, rad );
					if ( wsum > 0.0 && slot == 0 )
						nEye++;
				}
				if ( wsum <= 0.0 ) {
					gather( e, 2.0 * rad );
					if ( wsum > 0.0 && slot == 0 )
						nFar++;
				}
			}
			if ( wsum <= 0.0 ) {
				if ( slot == 0 )
					nBare++;
				else
					nTwoBare++;
				continue;
			}
			if ( slot == 0 )
				valid++;
			std::vector<float> & G = slot == 0 ? R.grid : R.grid2;
			std::vector<float> & GS = slot == 0 ? R.gridSky : R.gridSky2;
			for ( int a = 0; a < 6; a++ ) {
				const size_t o = ( size_t( a ) * nVox + i ) * 4;
				for ( int ch = 0; ch < 3; ch++ )
					G[o + size_t( ch )] = float( acc[a][ch] / wsum );
				G[o + 3] = 1.0f;
				GS[o] = GS[o + 1] = GS[o + 2] = float( accSky[a] / wsum );
				GS[o + 3] = 1.0f;
			}
		}
		vr += nr;
		vb += nb;
		if ( R.roomsOn )
			R.slotRooms[i] = probeRoomsPack( labs[0], labs[1] );
	} );
	R.visRays = vr;
	R.visBlocked = vb;
	R.voxelsValid = valid;
	R.voxelsEye = nEye;
	R.voxelsFar = nFar;
	R.voxelsTwoRooms = nTwo;
	R.voxelsCentreElsewhere = nElse;
	// lane ROOMCLAMP1: a slot whose eye sees no probe either (a pocket: a beam's top under the ceiling) takes the
	// mean of the valid slots of its room among its neighbours, two rings at most
	for ( int ring = 0; ring < 2 && !redNoEye; ring++ ) {
		struct GW { size_t i; int slot; float val[6][4]; };
		std::vector<GW> grow;
		for ( size_t i : todo ) {
			for ( int slot = 0; slot < 2; slot++ ) {
				const int si = slotOf[i][size_t( slot )];
				if ( si < 0 )
					continue;
				const std::vector<float> & G0 = slot == 0 ? R.grid : R.grid2;
				if ( G0[i * 4 + 3] > 0.0f )
					continue;
				const int lab = vs[i].lab[si];
				const int x = int( i % size_t( dims[0] ) ), y = int( ( i / size_t( dims[0] ) ) % size_t( dims[1] ) ),
					z = int( i / ( size_t( dims[0] ) * size_t( dims[1] ) ) );
				double s[6][4] = {};
				int n = 0;
				for ( int dz = -1; dz <= 1; dz++ )
					for ( int dy = -1; dy <= 1; dy++ )
						for ( int dx = -1; dx <= 1; dx++ ) {
							const int xx = x + dx, yy = y + dy, zz = z + dz;
							if ( xx < 0 || yy < 0 || zz < 0 || xx >= dims[0] || yy >= dims[1] || zz >= dims[2] )
								continue;
							const size_t j = ( size_t( zz ) * size_t( dims[1] ) + size_t( yy ) ) * size_t( dims[0] ) + size_t( xx );
							for ( int sj = 0; sj < 2; sj++ ) {
								const int q = slotOf[j][size_t( sj )];
								if ( q < 0 || vs[j].lab[q] != lab )
									continue;
								const std::vector<float> & G = sj == 0 ? R.grid : R.grid2;
								const std::vector<float> & GS = sj == 0 ? R.gridSky : R.gridSky2;
								if ( G[j * 4 + 3] <= 0.0f )
									continue;
								n++;
								for ( int a = 0; a < 6; a++ ) {
									const size_t o = ( size_t( a ) * nVox + j ) * 4;
									for ( int ch = 0; ch < 3; ch++ )
										s[a][ch] += G[o + size_t( ch )];
									s[a][3] += GS[o];
								}
							}
						}
				if ( !n )
					continue;
				GW w;
				w.i = i;
				w.slot = slot;
				for ( int a = 0; a < 6; a++ )
					for ( int ch = 0; ch < 4; ch++ )
						w.val[a][ch] = float( s[a][ch] / n );
				grow.push_back( w );
			}
		}
		for ( const GW & w : grow ) {
			std::vector<float> & G = w.slot == 0 ? R.grid : R.grid2;
			std::vector<float> & GS = w.slot == 0 ? R.gridSky : R.gridSky2;
			for ( int a = 0; a < 6; a++ ) {
				const size_t o = ( size_t( a ) * nVox + w.i ) * 4;
				for ( int ch = 0; ch < 3; ch++ )
					G[o + size_t( ch )] = w.val[a][ch];
				G[o + 3] = 1.0f;
				GS[o] = GS[o + 1] = GS[o + 2] = w.val[a][3];
				GS[o + 3] = 1.0f;
			}
			if ( w.slot == 0 ) {
				R.voxelsGrown++;
				R.voxelsValid++;
				nBare--;
			} else
				nTwoBare--;
		}
	}
	R.voxelsBare = nBare;
	R.voxelsTwoBare = nTwoBare;
	R.msGrid = clock.nsecsElapsed() / 1e6 - R.msLight - R.msGather - R.msBounce;

	// the gate's copies
	R.surfelOut.reserve( us.size() * 12 );
	for ( const US & u : us )
		for ( const double * a : { u.p, u.n, u.a, u.B } )
			for ( int c = 0; c < 3; c++ )
				R.surfelOut.push_back( float( a[c] ) );
	R.probeCube.reserve( up.size() * 21 );
	for ( const UP & P : up ) {
		for ( int c = 0; c < 3; c++ )
			R.probeCube.push_back( float( P.p[c] ) );
		for ( int a = 0; a < 6; a++ )
			for ( int c = 0; c < 3; c++ )
				R.probeCube.push_back( float( P.E[a][c] ) );
		for ( int a = 0; a < 6; a++ )
			R.probeSky.push_back( float( P.sky[a] ) );
	}
	if ( skyOn ) {   // lane SKY1: the sky's and the sun's parts, for the gate
		R.probeSkyE.reserve( up.size() * 18 );
		for ( const UP & P : up )
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					R.probeSkyE.push_back( float( P.S[a][c] ) );
		R.surfelSun.reserve( us.size() * 3 );
		for ( const US & u : us )
			for ( int c = 0; c < 3; c++ )
				R.surfelSun.push_back( float( u.S[c] ) );
	}
	R.ok = true;
	return true;
}

// lane SKY1: the census line of a weather-lit exterior
static QString skyCensusText( const ProbeGiResult & r )
{
	return QStringLiteral( "gi sky: %1; %2 of %3 probes see sky (mean share %4), %5 through glass; sun reaches %6 of %7 surfels "
		"(%8 rays, %9 blocked)" )
		.arg( r.skyLabel ).arg( r.probesSky ).arg( r.probes ).arg( r.skyVisMean, 0, 'f', 3 ).arg( r.probesTinted )
		.arg( r.surfelsSun ).arg( r.surfels ).arg( r.sunRays ).arg( r.sunBlocked );
}

// lane BOUNCE2: the passes
static QString bounceCensusText( const ProbeGiResult & r )
{
	const size_t n = r.passLog.size();
	return QStringLiteral( "gi bounce: %1 passes (%2), last change %3 of the brightest %4, gain %5; %6 of %7 surfels read a "
		"probe (%8 the closest only), %9 in a known room; feed rays %10 (%11 blocked), %12 probes in another room refused; ms %13" )
		.arg( r.passes ).arg( r.settled ? QStringLiteral( "settled" ) : QStringLiteral( "NOT settled" ) )
		.arg( n >= 3 ? r.passLog[n - 3] : 0.0, 0, 'g', 4 ).arg( n >= 3 ? r.passLog[n - 1] : 0.0, 0, 'g', 4 )
		.arg( r.gain, 0, 'f', 4 ).arg( r.surfelsFed ).arg( r.surfels ).arg( r.fedClosest ).arg( r.surfelsRoomed )
		.arg( r.feedRays ).arg( r.feedBlocked ).arg( r.feedOtherRoom ).arg( r.msBounce, 0, 'f', 0 );
}

void probeGiRoomsInto( ProbeGiResult & r, WwCellGi & gi )
{
	if ( !r.roomsOn || gi.rgba.empty() )
		return;
	gi.rgba.insert( gi.rgba.end(), r.grid2.begin(), r.grid2.end() );
	gi.sky.insert( gi.sky.end(), r.gridSky2.begin(), r.gridSky2.end() );
	gi.slotRooms = std::move( r.slotRooms );
	gi.rooms.resize( r.rooms.a.size() );
	for ( size_t i = 0; i < r.rooms.a.size(); i++ )
		gi.rooms[i] = probeRoomsPack( r.rooms.a[i], r.rooms.b[i] );
	for ( int c = 0; c < 3; c++ ) {
		gi.roomsOrigin[c] = r.rooms.origin[c];
		gi.roomsDims[c] = r.rooms.dims[c];
	}
	gi.roomsCell = r.rooms.cell;
}

QString probeGiCensusText( const ProbeGiResult & r )
{
	if ( !r.ok )
		return QStringLiteral( "gi REFUSED: %1" ).arg( r.error );
	return QStringLiteral( "gi: %1 files, %2 surfels (%3 lit), %4 probes, %5 links (%6 unresolved); shadow rays %7 "
		"(%8 blocked); grid %9x%10x%11 voxel %12 radius %13, %14 near surfaces, %15 valid; visibility rays %16 "
		"(%17 blocked); ms light %18 gather %19 grid %20" )
		.arg( r.files ).arg( r.surfels ).arg( r.surfelsLit ).arg( r.probes ).arg( r.links ).arg( r.linksUnresolved )
		.arg( r.shadowRays ).arg( r.shadowBlocked ).arg( r.dims[0] ).arg( r.dims[1] ).arg( r.dims[2] )
		.arg( double( r.voxel ), 0, 'f', 1 ).arg( double( r.radius ), 0, 'f', 1 ).arg( r.voxelsNear ).arg( r.voxelsValid )
		.arg( r.visRays ).arg( r.visBlocked ).arg( r.msLight, 0, 'f', 0 ).arg( r.msGather, 0, 'f', 0 ).arg( r.msGrid, 0, 'f', 0 )
		+ QStringLiteral( "\n  gi grid: voxels blocked at their centre filled from their eye: within the radius %1, "
			"within twice it %2; from valid neighbours %3; left empty %4" ).arg( r.voxelsEye ).arg( r.voxelsFar )
			.arg( r.voxelsGrown ).arg( r.voxelsBare )   // lane ROOMCLAMP1
		+ QStringLiteral( "\n  " ) + ( r.roomsOn ? probeRoomsCensusText( r.rooms )
			+ QStringLiteral( "\n  gi rooms: voxels with a second room %1 (left empty %2), slots gathered from their eye "
				"(the centre in another room) %3, probes in no room %4" ).arg( r.voxelsTwoRooms ).arg( r.voxelsTwoBare )
				.arg( r.voxelsCentreElsewhere ).arg( r.probesRoomless )
			: QStringLiteral( "gi rooms: off (red noclamp: one value a voxel)" ) )
		+ ( r.skyLit ? QStringLiteral( "\n  " ) + skyCensusText( r ) : QString() )
		+ QStringLiteral( "\n  " ) + bounceCensusText( r );
}

/* The dump (little-endian):
 *   gi_surfels.bin  int32 n, then n x 12 float32: position, normal, albedo (linear), B (outgoing)
 *   gi_probes.bin   int32 n, then n x 21 float32: position, E on +X -X +Y -Y +Z -Z (rgb each);
 *                   the probes in file-name order, then file order
 *   gi_grid.bin     float32 origin[3], voxel, radius; int32 dims[3]; then the grid (probegi.h)
 *   gi_skygrid.bin  the same header, then the sky grid (lane PROBEVIEW1)
 *   gi_probesky.bin int32 n, then n x 6 float32: the sky share on +X -X +Y -Y +Z -Z
 *   gi_links.bin    int32 n, int32 start[n + 1], then int32 surfel index (gi_surfels.bin order) per resolved link
 *   gi_meta.txt     the spec */
bool probeGiDump( const ProbeGiResult & r, const ProbeGiSpec & spec, const QString & dir, QString * err )
{
	QDir().mkpath( dir );
	auto put = [&]( const QString & name, const QByteArray & head, const std::vector<float> & body ) {
		QFile f( QDir( dir ).filePath( name ) );
		if ( !f.open( QIODevice::WriteOnly ) ) {
			*err = QStringLiteral( "cannot write %1" ).arg( f.fileName() );
			return false;
		}
		f.write( head );
		f.write( reinterpret_cast<const char *>( body.data() ), qint64( body.size() * sizeof( float ) ) );
		return true;
	};
	auto i32 = []( qint32 x ) { return QByteArray( reinterpret_cast<const char *>( &x ), 4 ); };
	auto f32 = []( float x ) { return QByteArray( reinterpret_cast<const char *>( &x ), 4 ); };
	if ( !put( QStringLiteral( "gi_surfels.bin" ), i32( r.surfels ), r.surfelOut ) )
		return false;
	if ( !put( QStringLiteral( "gi_probes.bin" ), i32( r.probes ), r.probeCube ) )
		return false;
	const QByteArray gh = f32( r.origin[0] ) + f32( r.origin[1] ) + f32( r.origin[2] ) + f32( r.voxel ) + f32( r.radius )
		+ i32( r.dims[0] ) + i32( r.dims[1] ) + i32( r.dims[2] );
	if ( !put( QStringLiteral( "gi_grid.bin" ), gh, r.grid ) )
		return false;
	/* lane SKY1, a weather-lit exterior only:
	 *   gi_sky.bin   int32 n, then n x 18 float32: the sky's part of each probe's E (same order)
	 *   gi_sun.bin   int32 n, then n x 3 float32: the sun's part of each surfel's B (same order)
	 *   gi_sky.txt   the light the relight used: amb (six rows, a surface facing +X -X +Y -Y +Z -Z),
	 *                sunTo, sun, the label, the red */
	if ( r.skyLit ) {
		if ( !put( QStringLiteral( "gi_sky.bin" ), i32( r.probes ), r.probeSkyE ) )
			return false;
		if ( !put( QStringLiteral( "gi_sun.bin" ), i32( r.surfels ), r.surfelSun ) )
			return false;
		QFile s( QDir( dir ).filePath( QStringLiteral( "gi_sky.txt" ) ) );
		if ( s.open( QIODevice::WriteOnly ) ) {
			QString t;
			for ( int a = 0; a < 6; a++ )
				t += QStringLiteral( "amb %1 %2 %3 %4\n" ).arg( a ).arg( double( spec.sky.amb[a][0] ), 0, 'g', 9 )
					.arg( double( spec.sky.amb[a][1] ), 0, 'g', 9 ).arg( double( spec.sky.amb[a][2] ), 0, 'g', 9 );
			t += QStringLiteral( "sunTo %1 %2 %3\nsun %4 %5 %6\nlabel %7\nskyRed %8\n" )
				.arg( double( spec.sky.sunTo[0] ), 0, 'g', 9 ).arg( double( spec.sky.sunTo[1] ), 0, 'g', 9 )
				.arg( double( spec.sky.sunTo[2] ), 0, 'g', 9 ).arg( double( spec.sky.sun[0] ), 0, 'g', 9 )
				.arg( double( spec.sky.sun[1] ), 0, 'g', 9 ).arg( double( spec.sky.sun[2] ), 0, 'g', 9 )
				.arg( spec.sky.label, spec.skyRed.isEmpty() ? QStringLiteral( "-" ) : spec.skyRed );
			s.write( t.toUtf8() );
		}
	}
	// lane PROBEVIEW1: the sky grid (gi_grid.bin's header and layout), each probe's six sky shares, its links
	if ( !put( QStringLiteral( "gi_skygrid.bin" ), gh, r.gridSky ) )
		return false;
	if ( !put( QStringLiteral( "gi_probesky.bin" ), i32( r.probes ), r.probeSky ) )
		return false;
	{
		QFile f( QDir( dir ).filePath( QStringLiteral( "gi_links.bin" ) ) );
		if ( !f.open( QIODevice::WriteOnly ) ) {
			*err = QStringLiteral( "cannot write %1" ).arg( f.fileName() );
			return false;
		}
		f.write( i32( r.probes ) );
		f.write( reinterpret_cast<const char *>( r.probeLinkStart.data() ), qint64( r.probeLinkStart.size() * 4 ) );
		f.write( reinterpret_cast<const char *>( r.probeLinks.data() ), qint64( r.probeLinks.size() * 4 ) );
	}
	/* lane BOUNCE2: the passes
	 *   gi_bounce.bin  int32 n, int32 passes, then n x 3 float32: each surfel's B after the last pass
	 *   gi_feed.bin    int32 n, int32 start[n + 1], then per entry int32 probe (gi_probes.bin order) + float32
	 *                  weight, then int32 room per surfel (-1 = not known)
	 *   gi_passes.txt  per pass: "pass k change sumB maxB" (change = the largest change of a surfel's B) */
	{
		QFile f( QDir( dir ).filePath( QStringLiteral( "gi_bounce.bin" ) ) );
		if ( !f.open( QIODevice::WriteOnly ) ) {
			*err = QStringLiteral( "cannot write %1" ).arg( f.fileName() );
			return false;
		}
		f.write( i32( r.surfels ) + i32( r.passes ) );
		f.write( reinterpret_cast<const char *>( r.surfelBounce.data() ), qint64( r.surfelBounce.size() * sizeof( float ) ) );
		QFile g( QDir( dir ).filePath( QStringLiteral( "gi_feed.bin" ) ) );
		if ( !g.open( QIODevice::WriteOnly ) ) {
			*err = QStringLiteral( "cannot write %1" ).arg( g.fileName() );
			return false;
		}
		g.write( i32( r.surfels ) );
		g.write( reinterpret_cast<const char *>( r.feedStart.data() ), qint64( r.feedStart.size() * 4 ) );
		for ( size_t k = 0; k < r.feedProbe.size(); k++ )
			g.write( i32( r.feedProbe[k] ) + f32( r.feedWeight[k] ) );
		g.write( reinterpret_cast<const char *>( r.surfelRoom.data() ), qint64( r.surfelRoom.size() * 4 ) );
		QFile t( QDir( dir ).filePath( QStringLiteral( "gi_passes.txt" ) ) );
		if ( t.open( QIODevice::WriteOnly ) ) {
			QString s = QStringLiteral( "passes %1 settled %2 red %3\n" ).arg( r.passes ).arg( r.settled ? 1 : 0 )
				.arg( spec.red.isEmpty() ? QStringLiteral( "-" ) : spec.red );
			for ( size_t k = 0; k + 2 < r.passLog.size(); k += 3 )
				s += QStringLiteral( "pass %1 %2 %3 %4\n" ).arg( k / 3 + 1 ).arg( r.passLog[k], 0, 'g', 9 )
					.arg( r.passLog[k + 1], 0, 'g', 12 ).arg( r.passLog[k + 2], 0, 'g', 9 );
			t.write( s.toUtf8() );
		}
	}
	/* lane ROOMCLAMP1, with the rooms on:
	 *   gi_rooms.bin  the fine grid (proberooms.h probeRoomsDump)
	 *   gi_slots.bin  gi_grid.bin's header, then per voxel int32 slot 0 room, int32 slot 1 room (-1 none), then
	 *                 slot 1's grid (gi_grid.bin's layout), then slot 1's sky grid
	 *   gi_proberooms.bin  int32 n, then per probe int32 room, int32 second room */
	if ( r.roomsOn ) {
		if ( !probeRoomsDump( r.rooms, QDir( dir ).filePath( QStringLiteral( "gi_rooms.bin" ) ), err ) )
			return false;
		QByteArray labs;
		labs.reserve( int( r.slotRooms.size() * 8 ) );
		for ( float p : r.slotRooms ) {
			const int k = int( p + 0.5f );
			labs += i32( k / 4096 - 1 ) + i32( k % 4096 - 1 );
		}
		std::vector<float> both( r.grid2 );
		both.insert( both.end(), r.gridSky2.begin(), r.gridSky2.end() );
		if ( !put( QStringLiteral( "gi_slots.bin" ), gh + labs, both ) )
			return false;
		QFile pr( QDir( dir ).filePath( QStringLiteral( "gi_proberooms.bin" ) ) );
		if ( !pr.open( QIODevice::WriteOnly ) ) {
			*err = QStringLiteral( "cannot write %1" ).arg( pr.fileName() );
			return false;
		}
		pr.write( i32( qint32( r.probeRooms.size() / 2 ) ) );
		pr.write( reinterpret_cast<const char *>( r.probeRooms.data() ), qint64( r.probeRooms.size() * 4 ) );
	}
	QFile m( QDir( dir ).filePath( QStringLiteral( "gi_meta.txt" ) ) );
	if ( m.open( QIODevice::WriteOnly ) )
		m.write( QStringLiteral( "fixtureClear %1\nradiusScale %2\nred %3\n%4\n" ).arg( double( spec.fixtureClear ) )
			.arg( double( spec.radiusScale ) ).arg( spec.red.isEmpty() ? QStringLiteral( "-" ) : spec.red )
			.arg( probeGiCensusText( r ) ).toUtf8() );
	return true;
}

// lane ROOMCLAMP1: `probegi --soup <f> --rect minX,minY,maxX,maxY --out <dir> --light x,y,z,radius,r,g,b [--light ...]
// [--spacing s] [--rays n] [--passes n] [--red noclamp|...] [--rooms-red conn26|boxes|glasswall] [--pinch u]
// [--cell u]`: place, bake (<dir>/bake), relight as an interior lit by the lights given, dump into <dir>
int probeGiCli( const QStringList & args )
{
	QString soupPath, outDir, rect;
	ProbePlaceSpec ps;
	ProbeBakeSpec bs;
	ProbeGiSpec gs;
	WwCellLighting L;
	L.interior = true;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--spacing" ) ) { ps.spacing = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--rays" ) ) { bs.rays = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--passes" ) ) { gs.passes = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { gs.red = nx; i++; }
		else if ( a == QLatin1String( "--rooms-red" ) ) { gs.rooms.red = nx; i++; }
		else if ( a == QLatin1String( "--pinch" ) ) { gs.rooms.pinch = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--cell" ) ) { gs.rooms.cell = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--light" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 7 ) {
				WwCellLight l;
				for ( int c = 0; c < 3; c++ ) {
					l.pos[c] = v[c].toFloat();
					l.color[c] = v[4 + c].toFloat();
				}
				l.radius = v[3].toFloat();
				L.lights.push_back( l );
			}
			i++;
		}
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outDir.isEmpty() || rc.size() != 4 || L.lights.isEmpty() ) {
		std::fprintf( stderr, "usage: probegi --soup <file> --rect minX,minY,maxX,maxY --out <dir> --light x,y,z,radius,r,g,b "
			"[--light ...] [--spacing s] [--rays n] [--passes n] [--red noclamp|noeye|novis|...] "
			"[--rooms-red conn26|boxes|glasswall] [--pinch u] [--cell u]\n" );
		return 2;
	}
	ps.minX = rc[0].toFloat();
	ps.minY = rc[1].toFloat();
	ps.maxX = rc[2].toFloat();
	ps.maxY = rc[3].toFloat();
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "probegi: %s\n", qPrintable( err ) );
		return 1;
	}
	ProbePlaceResult pr;
	if ( !probePlace( soup, ps, &pr ) ) {
		std::fprintf( stderr, "probegi: placement: %s\n", qPrintable( pr.error ) );
		return 1;
	}
	const QString bakeDir = QDir( outDir ).filePath( QStringLiteral( "bake" ) );
	ProbeBakeResult br;
	if ( !probeBake( soup, pr.probes, bs, bakeDir, &br, &pr.roomBoxes ) ) {
		std::fprintf( stderr, "probegi: %s\n", qPrintable( br.error ) );
		return 1;
	}
	ProbeGiResult gr;
	if ( !probeGiRelight( soup, bakeDir, L, gs, &gr ) ) {
		std::fprintf( stderr, "probegi: %s\n", qPrintable( gr.error ) );
		return 1;
	}
	if ( !probeGiDump( gr, gs, outDir, &err ) ) {
		std::fprintf( stderr, "probegi: %s\n", qPrintable( err ) );
		return 1;
	}
	std::fputs( qPrintable( probeCensusText( pr ) ), stdout );
	std::fputs( qPrintable( probeBakeCensusText( br ) ), stdout );
	std::fputs( qPrintable( probeGiCensusText( gr ) + QStringLiteral( "\n" ) ), stdout );
	return 0;
}
