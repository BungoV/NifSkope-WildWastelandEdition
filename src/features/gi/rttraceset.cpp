// lane AIRPERF1 round 4: the ray-traced GI's traced set follows the streamed cells (rttraceset.h)
#include "features/gi/rttraceset.h"

#include "features/gi/probegi.h"      // ProbeGiSpec
#include "features/gi/probeplace.h"   // ProbeSoup
#include "features/gi/probesky.h"     // the kept soup the volume traces
#include "features/gi/rtddgicell.h"
#include "features/gi/rtfartrace.h"   // round 5: the far field's runs
#include "features/gi/rttracesetrule.h"
#include "gl/celllights.h"            // wwCellPrtOn
#include "wwswitch.h"

#include <QElapsedTimer>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

using Bytes = std::vector<unsigned char>;

struct RtTraceUnitSoup
{
	ProbeSoup soup;   //!< its amask.maps[i].a and glow.maps[i].rgb are empty: the bytes are maskBytes[i] / glowBytes[i]
	std::vector<std::shared_ptr<const Bytes>> maskBytes, glowBytes;
};

namespace
{

template <class V> void drop( V & v )
{
	V().swap( v );
}

//! the bitmaps the units share: a map 25 cells name is held once, and goes with its last unit
struct Pool
{
	std::mutex m;
	std::unordered_map<std::string, std::weak_ptr<const Bytes>> map;
};

Pool & pool()
{
	static Pool p;
	return p;
}

std::string mapKey( char tag, const std::string & name, int w, int h )
{
	std::string k;
	k.reserve( name.size() + 24 );
	k += tag;
	k += '|';
	for ( const char c : name )
		k += ( c >= 'A' && c <= 'Z' ) ? char( c - 'A' + 'a' ) : c;
	k += '|';
	k += std::to_string( w );
	k += 'x';
	k += std::to_string( h );
	return k;
}

std::shared_ptr<const Bytes> pooled( const std::string & key, Bytes & bytes )
{
	Pool & p = pool();
	std::lock_guard<std::mutex> lk( p.m );
	if ( p.map.size() > 4096 ) {
		for ( auto it = p.map.begin(); it != p.map.end(); )
			it = it->second.expired() ? p.map.erase( it ) : std::next( it );
	}
	std::weak_ptr<const Bytes> & slot = p.map[key];
	if ( std::shared_ptr<const Bytes> have = slot.lock() ) {
		if ( have->size() == bytes.size() ) {
			drop( bytes );
			return have;
		}
	}
	std::shared_ptr<const Bytes> mine = std::make_shared<const Bytes>( std::move( bytes ) );
	drop( bytes );
	slot = mine;
	return mine;
}

using Index = std::unordered_map<std::string, int>;

//! one soup appended to the merged one: triangle tables shifted, maps and models found again by name
void append( ProbeSoup & out, const ProbeSoup & s, const std::vector<std::shared_ptr<const Bytes>> * maskBytes,
	const std::vector<std::shared_ptr<const Bytes>> * glowBytes, bool withAlb, Index & maskIdx, Index & glowIdx, Index & modelIdx )
{
	const size_t t0 = out.tris.size() / 9, n = s.tris.size() / 9;
	if ( !n )
		return;
	out.tris.insert( out.tris.end(), s.tris.begin(), s.tris.begin() + std::ptrdiff_t( n * 9 ) );
	if ( withAlb ) {
		out.alb.resize( t0 * 3, 128 );   // a soup without albedo: mid grey (ProbeSoup::addTri's rule)
		if ( s.alb.size() == n * 3 )
			out.alb.insert( out.alb.end(), s.alb.begin(), s.alb.end() );
		else
			out.alb.resize( ( t0 + n ) * 3, 128 );
	}
	if ( !s.twoSided.empty() ) {
		out.twoSided.resize( t0, 0 );
		out.twoSided.insert( out.twoSided.end(), s.twoSided.begin(),
			s.twoSided.begin() + std::ptrdiff_t( std::min( s.twoSided.size(), n ) ) );
	}
	if ( !s.amask.tris.empty() ) {
		std::vector<int> mm( s.amask.maps.size(), -1 ), md( s.amask.models.size(), -1 );
		for ( size_t i = 0; i < s.amask.maps.size(); i++ ) {
			const probebvh::AlphaMask::Map & src = s.amask.maps[i];
			const std::string name = i < s.amask.mapNames.size() ? s.amask.mapNames[i] : std::string();
			const std::string key = name.empty() ? std::string() : mapKey( 'a', name, src.w, src.h );
			const auto f = key.empty() ? maskIdx.end() : maskIdx.find( key );
			if ( f != maskIdx.end() ) {
				mm[i] = f->second;
				continue;
			}
			probebvh::AlphaMask::Map mp;
			mp.w = src.w;
			mp.h = src.h;
			mp.a = ( maskBytes && i < maskBytes->size() && ( *maskBytes )[i] ) ? *( *maskBytes )[i] : src.a;
			mm[i] = int( out.amask.maps.size() );
			out.amask.maps.push_back( std::move( mp ) );
			out.amask.mapNames.push_back( name );
			if ( !key.empty() )
				maskIdx.emplace( key, mm[i] );
		}
		for ( size_t i = 0; i < s.amask.models.size(); i++ ) {
			const auto f = modelIdx.find( s.amask.models[i] );
			if ( f != modelIdx.end() ) {
				md[i] = f->second;
				continue;
			}
			md[i] = int( out.amask.models.size() );
			out.amask.models.push_back( s.amask.models[i] );
			modelIdx.emplace( s.amask.models[i], md[i] );
		}
		const int base = int( out.amask.tris.size() );
		for ( probebvh::AlphaMask::Tri t : s.amask.tris ) {
			if ( t.map >= 0 && size_t( t.map ) < mm.size() )
				t.map = mm[size_t( t.map )];
			t.model = ( t.model >= 0 && size_t( t.model ) < md.size() ) ? md[size_t( t.model )] : -1;
			out.amask.tris.push_back( t );
		}
		out.amask.triOf.resize( t0, -1 );
		for ( size_t j = 0; j < n; j++ ) {
			const int v = j < s.amask.triOf.size() ? s.amask.triOf[j] : -1;
			out.amask.triOf.push_back( v < 0 ? -1 : v + base );
		}
	}
	if ( !s.glow.tris.empty() ) {
		std::vector<int> mm( s.glow.maps.size(), -1 );
		for ( size_t i = 0; i < s.glow.maps.size(); i++ ) {
			const ProbeEmit::Map & src = s.glow.maps[i];
			const std::string name = i < s.glow.mapNames.size() ? s.glow.mapNames[i] : std::string();
			const std::string key = name.empty() ? std::string() : mapKey( 'g', name, src.w, src.h );
			const auto f = key.empty() ? glowIdx.end() : glowIdx.find( key );
			if ( f != glowIdx.end() ) {
				mm[i] = f->second;
				continue;
			}
			ProbeEmit::Map mp;
			mp.w = src.w;
			mp.h = src.h;
			mp.rgb = ( glowBytes && i < glowBytes->size() && ( *glowBytes )[i] ) ? *( *glowBytes )[i] : src.rgb;
			mm[i] = int( out.glow.maps.size() );
			out.glow.maps.push_back( std::move( mp ) );
			out.glow.mapNames.push_back( name );
			if ( !key.empty() )
				glowIdx.emplace( key, mm[i] );
		}
		const int emBase = int( out.glow.emitters.size() );
		for ( ProbeEmit::Emitter e : s.glow.emitters ) {
			if ( e.map >= 0 && size_t( e.map ) < mm.size() )
				e.map = mm[size_t( e.map )];
			out.glow.emitters.push_back( e );
		}
		const int base = int( out.glow.tris.size() );
		for ( ProbeEmit::Tri t : s.glow.tris ) {
			if ( t.emitter >= 0 )
				t.emitter += emBase;
			out.glow.tris.push_back( t );
		}
		out.glow.triOf.resize( t0, -1 );
		for ( size_t j = 0; j < n; j++ ) {
			const int v = j < s.glow.triOf.size() ? s.glow.triOf[j] : -1;
			out.glow.triOf.push_back( v < 0 ? -1 : v + base );
		}
	}
}

/*! round 5: a far shape's picked runs appended (rtfartrace.h): world triangles copied, an instanced row's mesh placed
 *  (a mirrored placement turns the winding), each triangle with its own colour, an alpha-tested map by its name */
void appendFar( ProbeSoup & out, const RtFarPick & p, bool withAlb, Index & maskIdx )
{
	const RtFarShape & f = *p.shape;
	const bool masked = f.mask && !f.maskName.empty() && f.uv.size() * 9 == f.tris.size() * 6;
	int map = -1;
	for ( const int ri : p.runs ) {
		if ( ri < 0 || size_t( ri ) >= f.runs.size() )
			continue;
		const RtFarShape::Run & r = f.runs[size_t( ri )];
		const size_t t0 = out.tris.size() / 9, n = size_t( std::max( r.count, 0 ) ), a = size_t( std::max( r.first, 0 ) );
		if ( !n || ( a + n ) * 9 > f.tris.size() )
			continue;
		const RtFarShape::Xf * x = ( r.xf >= 0 && size_t( r.xf ) < f.xf.size() ) ? &f.xf[size_t( r.xf )] : nullptr;
		const bool flip = x && x->flip;
		if ( !x ) {
			out.tris.insert( out.tris.end(), f.tris.begin() + std::ptrdiff_t( a * 9 ), f.tris.begin() + std::ptrdiff_t( ( a + n ) * 9 ) );
		} else {
			out.tris.resize( ( t0 + n ) * 9 );
			float * d = out.tris.data() + t0 * 9;
			const float * s = f.tris.data() + a * 9;
			for ( size_t j = 0; j < n; j++, d += 9, s += 9 ) {
				for ( int k = 0; k < 3; k++ ) {
					const float * v = s + ( flip && k ? 3 - k : k ) * 3;
					for ( int c = 0; c < 3; c++ )
						d[k * 3 + c] = x->m[c * 4] * v[0] + x->m[c * 4 + 1] * v[1] + x->m[c * 4 + 2] * v[2] + x->m[c * 4 + 3];
				}
			}
		}
		if ( withAlb ) {
			out.alb.resize( t0 * 3, 128 );
			if ( f.alb.size() >= ( a + n ) * 3 )
				out.alb.insert( out.alb.end(), f.alb.begin() + std::ptrdiff_t( a * 3 ), f.alb.begin() + std::ptrdiff_t( ( a + n ) * 3 ) );
			else
				out.alb.resize( ( t0 + n ) * 3, 128 );
		}
		if ( f.twoSided ) {
			out.twoSided.resize( t0, 0 );
			out.twoSided.resize( t0 + n, 1 );
		}
		if ( !masked )
			continue;
		if ( map < 0 ) {
			const std::string key = mapKey( 'a', f.maskName, f.maskW, f.maskH );
			const auto have = maskIdx.find( key );
			if ( have != maskIdx.end() ) {
				map = have->second;
			} else {
				probebvh::AlphaMask::Map mp;
				mp.w = f.maskW;
				mp.h = f.maskH;
				mp.a = *f.mask;
				map = int( out.amask.maps.size() );
				out.amask.maps.push_back( std::move( mp ) );
				out.amask.mapNames.push_back( f.maskName );
				maskIdx.emplace( key, map );
			}
		}
		out.amask.triOf.resize( t0, -1 );
		for ( size_t j = 0; j < n; j++ ) {
			probebvh::AlphaMask::Tri t;
			t.map = map;
			t.thr = static_cast<unsigned char>( std::clamp( f.thr, 1, 255 ) );
			const float * u = f.uv.data() + ( a + j ) * 6;
			for ( int k = 0; k < 3; k++ ) {
				const int kk = flip && k ? 3 - k : k;
				t.uv[k * 2 + 0] = ( x && x->mirrorU ) ? x->uMid - u[kk * 2 + 0] : u[kk * 2 + 0];
				t.uv[k * 2 + 1] = u[kk * 2 + 1];
			}
			out.amask.triOf.push_back( int( out.amask.tris.size() ) );
			out.amask.tris.push_back( t );
		}
	}
}

struct Merge
{
	std::vector<RtTraceUnit> units;
	std::vector<rttracesetrule::Unit> rule;
	std::vector<RtFarPick> farPicks;
	RtFarInfo farInfo;
	ProbeSoup out;                            //!< becomes the kept soup
	std::shared_ptr<const ProbeSoup> build;   //!< the builds' copy, made here so no frame pays for it
	std::atomic<bool> done { false };
	double ms = 0.0;
	qint64 nearTris = 0, farTris = 0;
	std::uint64_t key = 0;
	int farCells = 0, ring = 0, cx = 0, cy = 0, noSoup = 0;
};

void mergeAll( Merge & m )
{
	size_t total = 0;
	bool withAlb = false;
	for ( const RtTraceUnit & u : m.units )
		if ( u.soup ) {
			total += u.soup->soup.tris.size() / 9;
			withAlb = withAlb || !u.soup->soup.alb.empty();
		}
	m.nearTris = qint64( total );
	m.farTris = 0;
	for ( const RtFarPick & p : m.farPicks )
		for ( const int ri : p.runs )
			if ( p.shape && ri >= 0 && size_t( ri ) < p.shape->runs.size() )
				m.farTris += p.shape->runs[size_t( ri )].count;
	if ( m.farTris > 0 )
		withAlb = true;   // every far triangle has its colour
	total += size_t( m.farTris );
	m.out.tris.reserve( total * 9 );
	if ( withAlb )
		m.out.alb.reserve( total * 3 );
	Index maskIdx, glowIdx, modelIdx;
	for ( const RtTraceUnit & u : m.units )
		if ( u.soup )
			append( m.out, u.soup->soup, &u.soup->maskBytes, &u.soup->glowBytes, withAlb, maskIdx, glowIdx, modelIdx );
	const size_t nearGot = m.out.tris.size() / 9;
	for ( const RtFarPick & p : m.farPicks )
		if ( p.shape )
			appendFar( m.out, p, withAlb, maskIdx );
	const size_t got = m.out.tris.size() / 9;
	m.farTris = qint64( got - nearGot );
	if ( withAlb )
		m.out.alb.resize( got * 3, 128 );
	if ( !m.out.amask.triOf.empty() )
		m.out.amask.triOf.resize( got, -1 );
	if ( !m.out.glow.triOf.empty() )
		m.out.glow.triOf.resize( got, -1 );
}

struct Set
{
	std::vector<RtTraceUnit> units;            //!< the loaded set the stream gave last
	std::vector<rttracesetrule::Unit> rule;
	std::vector<RtFarPick> farPicks;
	RtFarInfo farInfo;
	int farCells = 0, ring = 0, cx = 0, cy = 0, noSoup = 0;
	std::uint64_t wantKey = 0, pubKey = 0;
	bool changed = false, saidEmpty = false, saidParked = false;
	std::shared_ptr<Merge> merge;
	std::shared_ptr<const ProbeSoup> build;    //!< the last publish's copy for the builds
	quint64 gen = 0;
	QElapsedTimer sincePublish, sinceChange;
	float reach = 0.0f;
	int idleFrames = 0;
};

std::map<const void *, Set> & sets()
{
	static std::map<const void *, Set> m;
	return m;
}

void say( const QString & line )
{
	std::fprintf( stderr, "rtddgi traced set: %s\n", qPrintable( line ) );
	std::fflush( stderr );
}

double mbOf( double bytes )
{
	return bytes / 1048576.0;
}

} // namespace

bool rtTraceSetWeldRed()
{
	return !wwEnvIsEmpty( "WW_CELL_RTSET_WELD_RED" );
}

bool rtTraceSetFollows()
{
	return !wwCellPrtOn() && rtddgiCellShaderDefine();
}

bool rtTraceSetRed( const char * word )
{
	return wwEnvStr( "WW_CELL_RTSET_RED" ).trimmed() == QLatin1String( word );
}

std::shared_ptr<const RtTraceUnitSoup> rtTraceSetIntern( ProbeSoup && soup )
{
	auto u = std::make_shared<RtTraceUnitSoup>();
	u->soup = std::move( soup );
	ProbeSoup & p = u->soup;
	// what the volume's trace never reads (RtDdgiLive::open: triangles, albedo, two-sided, masks, glow)
	drop( p.doors );
	drop( p.glass );
	drop( p.glassT );
	drop( p.decal );
	drop( p.decalA );
	drop( p.water );
	drop( p.waterKind );
	drop( p.waterTypes );
	drop( p.mat );
	drop( p.matTex );
	drop( p.matTexPtr );
	drop( p.cubeExtra );
	drop( p.cubeExtraMat );
	p.veil = {};
	p.doorGeom = {};
	p.tris.shrink_to_fit();
	p.alb.shrink_to_fit();
	p.twoSided.shrink_to_fit();
	p.amask.tris.shrink_to_fit();
	p.amask.triOf.shrink_to_fit();
	p.glow.tris.shrink_to_fit();
	p.glow.triOf.shrink_to_fit();
	u->maskBytes.resize( p.amask.maps.size() );
	for ( size_t i = 0; i < p.amask.maps.size(); i++ ) {
		probebvh::AlphaMask::Map & mp = p.amask.maps[i];
		if ( i < p.amask.mapNames.size() && !p.amask.mapNames[i].empty() )
			u->maskBytes[i] = pooled( mapKey( 'a', p.amask.mapNames[i], mp.w, mp.h ), mp.a );
		else
			u->maskBytes[i] = std::make_shared<const Bytes>( std::move( mp.a ) );
		drop( mp.a );
	}
	u->glowBytes.resize( p.glow.maps.size() );
	for ( size_t i = 0; i < p.glow.maps.size(); i++ ) {
		ProbeEmit::Map & mp = p.glow.maps[i];
		if ( i < p.glow.mapNames.size() && !p.glow.mapNames[i].empty() )
			u->glowBytes[i] = pooled( mapKey( 'g', p.glow.mapNames[i], mp.w, mp.h ), mp.rgb );
		else
			u->glowBytes[i] = std::make_shared<const Bytes>( std::move( mp.rgb ) );
		drop( mp.rgb );
	}
	return u;
}

qint64 rtTraceUnitTris( const RtTraceUnitSoup & u )
{
	return u.soup.triCount();
}

int rtTraceSetRing( const void * nif, int gridHalf )
{
	const auto it = sets().find( nif );
	return rttracesetrule::farRing( it == sets().end() ? 0.0 : double( it->second.reach ), gridHalf );
}

void rtTraceSetSync( const void * nif, std::vector<RtTraceUnit> units, int cx, int cy, int ex, int ey, int ring,
	std::vector<RtFarPick> farPicks, const RtFarInfo & farInfo, int unitsNoSoup )
{
	Q_UNUSED( ex );
	Q_UNUSED( ey );
	Set & s = sets()[nif];
	std::sort( units.begin(), units.end(), []( const RtTraceUnit & a, const RtTraceUnit & b ) {
		if ( a.lref != b.lref )
			return a.lref < b.lref;
		return a.y != b.y ? a.y < b.y : a.x < b.x;
	} );
	std::vector<rttracesetrule::Unit> rule;
	rule.reserve( units.size() );
	for ( const RtTraceUnit & u : units ) {
		rttracesetrule::Unit r;
		r.x = u.x;
		r.y = u.y;
		r.lref = u.lref;
		r.tris = u.soup ? std::uint64_t( rtTraceUnitTris( *u.soup ) ) : 0;
		rule.push_back( r );
	}
	const int farCells = farInfo.cells;
	const std::uint64_t farTris = std::uint64_t( farInfo.terrainTris + farInfo.objTris );
	const std::uint64_t key = rttracesetrule::setKey( rule, ring, std::uint64_t( farCells ), farTris, farInfo.key );
	s.cx = cx;
	s.cy = cy;
	s.noSoup = unitsNoSoup;
	if ( key == s.wantKey && !s.units.empty() )
		return;
	s.wantKey = key;
	s.units = std::move( units );
	s.rule = std::move( rule );
	s.farPicks = std::move( farPicks );
	s.farInfo = farInfo;
	s.farCells = farCells;
	s.ring = ring;
	s.changed = key != s.pubKey;
	s.saidEmpty = false;
	s.sinceChange.start();
}

void rtTraceSetTick( const void * nif, int ex, int ey )
{
	const auto it = sets().find( nif );
	if ( it == sets().end() )
		return;
	Set & s = it->second;
	if ( s.merge && s.merge->done.load() ) {
		std::shared_ptr<Merge> m = std::move( s.merge );
		s.merge.reset();
		s.pubKey = m->key;
		s.changed = s.wantKey != s.pubKey;
		if ( m->out.tris.empty() ) {
			if ( !s.saidEmpty )
				say( QStringLiteral( "nothing to trace yet: %1 loaded cells, %2 of them without a soup (they loaded before the "
					"Ray-traced GI row went on: the stream gathers their soup now, nearest first), %3 far cells" )
					.arg( int( m->rule.size() ) + m->noSoup ).arg( m->noSoup ).arg( m->farCells ) );
			s.saidEmpty = true;
		} else {
			int cells = 0, lrefs = 0;
			qint64 cellTris = 0, lrefTris = 0;
			QString list;
			for ( const rttracesetrule::Unit & u : m->rule ) {
				if ( u.lref ) {
					lrefs++;
					lrefTris += qint64( u.tris );
					continue;
				}
				cells++;
				cellTris += qint64( u.tris );
				list += QStringLiteral( " %1,%2" ).arg( u.x ).arg( u.y );
			}
			const bool in = rttracesetrule::cameraIn( m->rule, ex, ey );
			const double bytes = double( m->out.tris.size() ) * 4.0 + double( m->out.alb.size() ) + double( m->out.twoSided.size() );
			ProbeGiSpec ks;   // an exterior: lit by the sky as it is now (probesky.cpp)
			probeSkyKeepMove( nif, std::move( m->out ), ks, true );
			s.build = m->build;
			s.gen++;
			s.sincePublish.start();
			s.idleFrames = 0;
			say( QStringLiteral( "gen %1, %2 cells + %3 large refs + %4 far cells (ring %5), centre %6,%7, camera cell %8,%9 in set %10, "
				"%11 triangles (near %12, far %13), %14 MB kept + the same for the builds, merge %15 ms%16, cells:%17" )
				.arg( s.gen ).arg( cells ).arg( lrefs ).arg( m->farCells ).arg( m->ring ).arg( m->cx ).arg( m->cy ).arg( ex ).arg( ey )
				.arg( in ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ).arg( m->nearTris + m->farTris ).arg( m->nearTris )
				.arg( m->farTris ).arg( mbOf( bytes ), 0, 'f', 1 ).arg( m->ms, 0, 'f', 1 )
				.arg( m->noSoup ? QStringLiteral( ", %1 loaded cells without a soup (loaded before the row went on: being gathered)" ).arg( m->noSoup ) : QString() )
				.arg( list ) );
			// round 5: what each source brings (39 B a triangle kept: 9 floats and its colour)
			const RtFarInfo & fi = m->farInfo;
			const auto mb39 = []( qint64 tris ) { return mbOf( double( tris ) * 39.0 ); };
			say( QStringLiteral( "gen %1 by source: loaded cells %2 triangles %3 MB | large refs %4 triangles %5 MB | far terrain %6 "
				"triangles %7 MB | far objects %8 triangles %9 MB in %10 cells (budget %11 triangles: %12 triangles of %13 cells left "
				"out, nearest cells kept first) | far triangles on a stated tone (no map read) %14 | far cache %15 shapes %16 MB, "
				"%17 far shapes still to read" )
				.arg( s.gen ).arg( cellTris ).arg( mb39( cellTris ), 0, 'f', 1 ).arg( lrefTris ).arg( mb39( lrefTris ), 0, 'f', 1 )
				.arg( fi.terrainTris ).arg( mb39( fi.terrainTris ), 0, 'f', 1 ).arg( fi.objTris ).arg( mb39( fi.objTris ), 0, 'f', 1 )
				.arg( fi.objCells ).arg( qint64( rttracesetrule::kFarObjTriCap ) ).arg( fi.objDropped ).arg( fi.objCellsDropped )
				.arg( fi.flat ).arg( fi.cacheShapes ).arg( fi.cacheMb, 0, 'f', 1 ).arg( fi.pending ) );
		}
	}
	const bool parked = rtTraceSetRed( "parked" );
	if ( parked && s.changed && s.gen > 0 && !s.saidParked ) {
		s.saidParked = true;
		say( QStringLiteral( "RED WW_CELL_RTSET_RED=parked: the loaded set changed, gen %1 stays (the GI parked where it was built)" ).arg( s.gen ) );
	}
	if ( !parked )
		s.saidParked = false;
	if ( !rttracesetrule::publishNow( s.changed, bool( s.merge ), s.gen > 0, s.gen > 0 ? double( s.sincePublish.elapsed() ) : 0.0,
			rttracesetrule::kMinGapMs, parked ) )
		return;
	auto m = std::make_shared<Merge>();
	m->units = s.units;
	m->rule = s.rule;
	m->farPicks = s.farPicks;   // red nofar: the stream picked none (cellstream.cpp traceSync)
	m->farInfo = s.farInfo;
	m->key = s.wantKey;
	m->farCells = s.farCells;
	m->ring = s.ring;
	m->cx = s.cx;
	m->cy = s.cy;
	m->noSoup = s.noSoup;
	s.merge = m;
	std::thread( [m]() {
		QElapsedTimer t;
		t.start();
		mergeAll( *m );
		if ( !m->out.tris.empty() )
			m->build = std::make_shared<const ProbeSoup>( m->out );
		m->ms = double( t.nsecsElapsed() ) / 1e6;
		m->done.store( true );
	} ).detach();
}

void rtTraceSetForget( const void * nif )
{
	sets().erase( nif );   // a merge in flight ends on its own (it holds what it reads)
}

bool rtTraceSetActive( const void * nif )
{
	return sets().count( nif ) > 0;
}

bool rtTraceSetPending( const void * nif )
{
	const auto it = sets().find( nif );
	if ( it == sets().end() )
		return false;
	const Set & s = it->second;
	if ( s.merge )
		return true;
	// a changed set that waits for its gap; never longer than 10 s, so a stuck set cannot hold the volume's own rebuilds
	return s.changed && !rtTraceSetRed( "parked" ) && s.sinceChange.isValid() && s.sinceChange.elapsed() < 10000;
}

quint64 rtTraceSetGen( const void * nif )
{
	const auto it = sets().find( nif );
	return it == sets().end() ? 0 : it->second.gen;
}

std::shared_ptr<const ProbeSoup> rtTraceSetBuildSoup( const void * nif, const ProbeSoup * kept )
{
	const auto it = sets().find( nif );
	if ( it == sets().end() || !kept || !it->second.build || it->second.build->tris.size() != kept->tris.size() )
		return nullptr;
	return it->second.build;
}

void rtTraceSetFrame( const void * nif, float reachUnits, bool building )
{
	const auto it = sets().find( nif );
	if ( it == sets().end() )
		return;
	Set & s = it->second;
	s.reach = reachUnits;
	if ( building || s.merge ) {
		s.idleFrames = 0;
	} else if ( s.build && ++s.idleFrames > 120 ) {
		s.build.reset();   // every cascade is built: a later rebuild (a light moved) copies the kept soup, as before
		s.idleFrames = 0;
	}
}
