/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENSE BLOCK *****/

#include "probebake.h"
#include "probebvh.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <thread>
#include <unordered_map>

namespace {

constexpr double kFourPi = 12.566370614359172;

/* ---- the `.tbk` v3 records, laid out exactly as the reader's (natural alignment,
 * little-endian; the SectorRecord's two bytes after pad0 are alignment padding). */
struct TbkHeader
{
	quint32 magic = 0x314B4254u;    // 'TBK1'
	quint32 version = 3u;
	quint32 recordKind = 1u;        // resolved albedo
	qint32 cellX = 0, cellY = 0;
	float surfelCellSize = 70.0f;
	quint32 surfelCount = 0, probeCount = 0, linkCount = 0;
	quint32 flags = 0;              // 1 = links, 2 = sky visibility
	quint32 reserved[6] = { 0, 0, 0, 0, 0, 0 };
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
	qint16 dir[2];      // octahedral snorm16, probe -> surfel
	quint16 weight;     // unorm16 of the probe's linkWeightScale
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

// The reader's own key: float division, then floor (its FloorDiv).
inline qint32 floorDiv( float v, float s )
{
	return qint32( std::floor( v / s ) );
}

struct Key
{
	qint32 x, y, z;
	bool operator==( const Key & o ) const { return x == o.x && y == o.y && z == o.z; }
	bool operator<( const Key & o ) const
	{
		return z != o.z ? z < o.z : ( y != o.y ? y < o.y : x < o.x );
	}
};
struct KeyHash
{
	size_t operator()( const Key & k ) const
	{
		quint64 h = 1469598103934665603ull;
		for ( qint32 v : { k.x, k.y, k.z } )
			for ( int i = 0; i < 4; i++ ) {
				h ^= ( quint32( v ) >> ( i * 8 ) ) & 0xFFu;
				h *= 1099511628211ull;
			}
		return size_t( h );
	}
};

inline Key keyFor( const float p[3], float s )
{
	return Key{ floorDiv( p[0], s ), floorDiv( p[1], s ), floorDiv( p[2], s ) };
}

//! The reader's octahedral codec (octahedral snorm16, lower hemisphere folded).
void packDir( const double d[3], qint16 out[2] )
{
	const float len = float( std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] ) );
	out[0] = out[1] = 0;
	if ( !std::isfinite( len ) || !( len > 1.0e-12f ) )
		return;
	const float nx = float( d[0] ) / len, ny = float( d[1] ) / len, nz = float( d[2] ) / len;
	const float l1 = std::fabs( nx ) + std::fabs( ny ) + std::fabs( nz );
	float px = nx / l1, py = ny / l1;
	if ( nz < 0.0f ) {
		const float ax = 1.0f - std::fabs( py ), ay = 1.0f - std::fabs( px );
		px = px >= 0.0f ? ax : -ax;
		py = py >= 0.0f ? ay : -ay;
	}
	out[0] = qint16( std::lround( std::clamp( px, -1.0f, 1.0f ) * 32767.0f ) );
	out[1] = qint16( std::lround( std::clamp( py, -1.0f, 1.0f ) * 32767.0f ) );
}

/*! One surfel cell's samples, split by the side they face: bin = the dominant axis of
 *  the normal (turned toward the probe that saw it) and its sign. A thin wall puts
 *  both of its faces in one 70-unit cell; averaging them cancels the normal and a
 *  probe indoors would gather the sunlit outside (a light leak). The cell keeps the
 *  side most rays saw, plus every face not opposed to it (a corner's floor and wall). */
struct Bin
{
	double pos[3] = { 0, 0, 0 }, nrm[3] = { 0, 0, 0 }, alb[3] = { 0, 0, 0 };
	quint32 n = 0;
	void add( const Bin & o )
	{
		for ( int k = 0; k < 3; k++ ) {
			pos[k] += o.pos[k];
			nrm[k] += o.nrm[k];
			alb[k] += o.alb[k];
		}
		n += o.n;
	}
};
struct SurfelBins
{
	Bin b[6];
	void add( const SurfelBins & o )
	{
		for ( int i = 0; i < 6; i++ )
			b[i].add( o.b[i] );
	}
};
typedef std::unordered_map<Key, SurfelBins, KeyHash> SurfelMap;

struct Final
{
	TbkSurfel rec;
	double n[3];    //!< the stored (quantized) normal, what the facing rule tests
};
typedef std::unordered_map<Key, Final, KeyHash> FinalMap;

inline void unpackDir( const qint16 in[2], double v[3] )
{
	double x = in[0] / 32767.0, y = in[1] / 32767.0;
	const double z = 1.0 - std::fabs( x ) - std::fabs( y );
	if ( z < 0 ) {
		const double ax = ( 1.0 - std::fabs( y ) ) * ( x >= 0 ? 1 : -1 ), ay = ( 1.0 - std::fabs( x ) ) * ( y >= 0 ? 1 : -1 );
		x = ax;
		y = ay;
	}
	const double l = std::sqrt( x * x + y * y + z * z );
	v[0] = x / l;
	v[1] = y / l;
	v[2] = z / l;
}

struct ProbeOut
{
	TbkProbe rec;
	std::vector<TbkLink> links;
	std::vector<Key> linkKeys;  // absolute surfel keys, same order as links
	int sx = 0, sy = 0;         // sector
	bool capped = false;
	double sky = 0, turned = 0, voidShare = 0;
};

struct Chunk
{
	int first = 0, count = 0;
	SurfelMap surfels;
	std::vector<ProbeOut> probes;
	qint64 hits = 0, misses = 0;
	qint64 spilled = 0;         // links sent to a second side instead of refused
	qint64 turned = 0;          // links refused by the facing rule
};

} // namespace

bool probeBake( const ProbeSoup & soup, const std::vector<ProbePoint> & probes, const ProbeBakeSpec & spec,
	const QString & outDir, ProbeBakeResult * out )
{
	ProbeBakeResult & R = *out;
	R = ProbeBakeResult();
	if ( soup.tris.empty() || probes.empty() ) {
		R.error = soup.tris.empty() ? QStringLiteral( "the soup is empty" ) : QStringLiteral( "no probes to bake" );
		return false;
	}
	const int N = qBound( 64, spec.rays, 1 << 16 );
	const float cellS = spec.surfelCell > 1.0f ? spec.surfelCell : 70.0f;
	const bool albKnown = soup.alb.size() * 3 == soup.tris.size();
	R.albedoKnown = albKnown ? 1 : 0;
	const bool redOct = spec.red == QLatin1String( "octant" );
	const bool redNrm = spec.red == QLatin1String( "normal" );

	// local origin (the probes' center, z 0), as the placer does: float precision
	// must not depend on how far from the world origin the block is
	double O[3] = { 0, 0, 0 };
	for ( const ProbePoint & p : probes ) {
		O[0] += p.pos[0];
		O[1] += p.pos[1];
	}
	O[0] = std::floor( O[0] / double( probes.size() ) );
	O[1] = std::floor( O[1] / double( probes.size() ) );

	QElapsedTimer tm;
	tm.start();
	probebvh::Bvh bvh;
	bvh.t.resize( soup.tris.size() );
	for ( size_t i = 0; i < soup.tris.size(); i += 3 ) {
		bvh.t[i + 0] = float( double( soup.tris[i + 0] ) - O[0] );
		bvh.t[i + 1] = float( double( soup.tris[i + 1] ) - O[1] );
		bvh.t[i + 2] = soup.tris[i + 2];
	}
	bvh.build();

	// the ray set: a Fibonacci sphere, 4 pi / N steradians each
	std::vector<double> dirs( size_t( N ) * 3 );
	const double golden = 3.14159265358979323846 * ( 3.0 - std::sqrt( 5.0 ) );
	for ( int i = 0; i < N; i++ ) {
		const double z = 1.0 - ( 2.0 * i + 1.0 ) / N;
		const double r = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
		const double ph = golden * i;
		dirs[size_t( i ) * 3 + 0] = r * std::cos( ph );
		dirs[size_t( i ) * 3 + 1] = r * std::sin( ph );
		dirs[size_t( i ) * 3 + 2] = z;
	}
	const double omega = kFourPi / N;
	auto octantOf = [redOct]( const double d[3] ) {
		int o = ( d[0] < 0 ? 1 : 0 ) | ( d[1] < 0 ? 2 : 0 ) | ( d[2] < 0 ? 4 : 0 );
		if ( redOct )
			o = ( d[0] < 0 ? 4 : 0 ) | ( d[1] < 0 ? 2 : 0 ) | ( d[2] < 0 ? 1 : 0 );
		return o;
	};

	// fixed chunks, so the sums are the same whatever the thread count
	const int nChunks = qMin( 64, int( probes.size() ) );
	std::vector<Chunk> chunks( static_cast<size_t>( nChunks ) );
	for ( int c = 0; c < nChunks; c++ ) {
		chunks[size_t( c )].first = int( qint64( probes.size() ) * c / nChunks );
		chunks[size_t( c )].count = int( qint64( probes.size() ) * ( c + 1 ) / nChunks ) - chunks[size_t( c )].first;
	}

	// the ray: nearest soup triangle within rayMax, t > 1e-3 (a probe inside its own surface carries no direction)
	auto cast = [&]( const double o[3], const double * d, double * t, int * tri ) {
		*tri = -1;
		return bvh.ray( o, d, spec.rayMax, t, tri ) && *tri >= 0;
	};
	auto hitPoint = [&]( const double o[3], const double * d, double t, float hw[3] ) {
		hw[0] = float( o[0] + d[0] * t + O[0] );
		hw[1] = float( o[1] + d[1] * t + O[1] );
		hw[2] = float( o[2] + d[2] * t );
	};

	// pass 1: the surfels, from every probe's hits
	auto surfelChunk = [&]( Chunk & ch ) {
		for ( int pi = ch.first; pi < ch.first + ch.count; pi++ ) {
			const ProbePoint & pp = probes[size_t( pi )];
			const double o[3] = { double( pp.pos[0] ) - O[0], double( pp.pos[1] ) - O[1], double( pp.pos[2] ) };
			for ( int i = 0; i < N; i++ ) {
				const double * d = &dirs[size_t( i ) * 3];
				double t = 0;
				int tri;
				if ( !cast( o, d, &t, &tri ) || !( t > 1.0e-3 ) )
					continue;
				float hw[3];
				hitPoint( o, d, t, hw );
				// the face's normal, turned toward the probe
				const float * p = &soup.tris[size_t( tri ) * 9];
				const double e1[3] = { double( p[3] ) - p[0], double( p[4] ) - p[1], double( p[5] ) - p[2] };
				const double e2[3] = { double( p[6] ) - p[0], double( p[7] ) - p[1], double( p[8] ) - p[2] };
				double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
				const double nl = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
				if ( !( nl > 0 ) )
					continue;
				for ( double & v : n )
					v /= nl;
				if ( !redNrm && n[0] * d[0] + n[1] * d[1] + n[2] * d[2] > 0 )
					for ( double & v : n )
						v = -v;
				int ax = 0;
				for ( int k = 1; k < 3; k++ )
					if ( std::fabs( n[k] ) > std::fabs( n[ax] ) )
						ax = k;
				Bin & s = ch.surfels[keyFor( hw, cellS )].b[ax * 2 + ( n[ax] < 0 ? 1 : 0 )];
				for ( int k = 0; k < 3; k++ ) {
					s.pos[k] += hw[k];
					s.nrm[k] += n[k];
					s.alb[k] += albKnown ? soup.alb[size_t( tri ) * 3 + size_t( k )] / 255.0 : 0.5;
				}
				s.n++;
			}
		}
	};

	FinalMap fin;
	std::unordered_map<Key, Key, KeyHash> alt;   // a two-sided cell -> the neighbour holding its second side

	// pass 2: the probe records, linking only surfels that face the probe
	auto probeChunk = [&]( Chunk & ch ) {
		struct Cell { double w = 0, dir[3] = { 0, 0, 0 }; };
		std::unordered_map<Key, Cell, KeyHash> cells;
		for ( int pi = ch.first; pi < ch.first + ch.count; pi++ ) {
			const ProbePoint & pp = probes[size_t( pi )];
			const double o[3] = { double( pp.pos[0] ) - O[0], double( pp.pos[1] ) - O[1], double( pp.pos[2] ) };
			cells.clear();
			double oSky[8] = {}, oSurf[8] = {}, oD[8] = {}, oD2[8] = {}, total = 0, voidW = 0;
			for ( int i = 0; i < N; i++ ) {
				const double * d = &dirs[size_t( i ) * 3];
				double t = 0;
				int tri;
				const int oc = octantOf( d );
				if ( !cast( o, d, &t, &tri ) ) {
					if ( spec.noSky )
						voidW += omega;   // an interior: out through an opening, into nothing
					else
						oSky[oc] += omega;
					total += omega;
					ch.misses++;
					continue;
				}
				ch.hits++;
				if ( !( t > 1.0e-3 ) )
					continue;
				float hw[3];
				hitPoint( o, d, t, hw );
				Cell & cl = cells[keyFor( hw, cellS )];
				cl.w += omega;
				for ( int a = 0; a < 3; a++ )
					cl.dir[a] += d[a] * omega;
				oSurf[oc] += omega;
				oD[oc] += t * omega;
				oD2[oc] += t * t * omega;
				total += omega;
			}
			// the record, as the reader's accumulator emits it
			ProbeOut po;
			TbkProbe & r = po.rec;
			std::memset( &r, 0, sizeof r );
			for ( int a = 0; a < 3; a++ )
				r.position[a] = pp.pos[a];
			r.coverage = float( total / kFourPi );
			double skyAll = 0;
			for ( int oc = 0; oc < 8; oc++ ) {
				const double m = oSky[oc] + oSurf[oc];
				r.skyVis[oc] = m > 0 ? float( oSky[oc] / m ) : 0.0f;
				skyAll += oSky[oc];
				if ( oSurf[oc] > 0 ) {
					r.octantDistance[oc] = float( oD[oc] / oSurf[oc] );
					r.octantDistanceRms[oc] = float( std::sqrt( std::max( oD2[oc] / oSurf[oc], 0.0 ) ) );
				}
			}
			po.sky = skyAll / kFourPi;
			r.placementClass = quint32( int( pp.cls ) );
			r.placementLevel = quint32( qMax( 0, pp.level ) );
			// the facing rule: a surfel turned away from the probe is the far side of a
			// thin wall; its weight is unlinked (the relight renormalizes over the rest)
			struct Cand { Key k; const Cell * c; qint16 dir[2]; };
			std::vector<Cand> ord;
			ord.reserve( cells.size() );
			double turned = 0;
			for ( const auto & e : cells ) {
				Cand cd{ e.first, &e.second, { 0, 0 } };
				packDir( e.second.dir, cd.dir );
				const auto f = fin.find( e.first );
				double v[3];
				unpackDir( cd.dir, v );
				if ( f == fin.end() || v[0] * f->second.n[0] + v[1] * f->second.n[1] + v[2] * f->second.n[2] > -1.0e-4 ) {
					const auto a = alt.find( e.first );
					if ( a != alt.end() ) {
						const double * an = fin.find( a->second )->second.n;
						if ( v[0] * an[0] + v[1] * an[1] + v[2] * an[2] < -1.0e-4 ) {
							cd.k = a->second;
							ord.push_back( cd );
							ch.spilled++;
							continue;
						}
					}
					turned += e.second.w;
					ch.turned++;
					continue;
				}
				ord.push_back( cd );
			}
			std::sort( ord.begin(), ord.end(), []( const Cand & a, const Cand & b ) {
				if ( a.c->w != b.c->w )
					return a.c->w > b.c->w;
				return a.k < b.k;
			} );
			const size_t keep = qMin( size_t( spec.maxLinks ), ord.size() );
			po.capped = ord.size() > keep;
			po.turned = turned / kFourPi;
			double dropped = turned + voidW, maxW = 0;
			po.voidShare = voidW / kFourPi;
			for ( size_t i = keep; i < ord.size(); i++ )
				dropped += ord[i].c->w;
			for ( size_t i = 0; i < keep; i++ )
				maxW = std::max( maxW, ord[i].c->w );
			r.unlinkedWeight = float( dropped / kFourPi );
			const float pf[3] = { pp.pos[0], pp.pos[1], pp.pos[2] };
			const Key pk = keyFor( pf, cellS );
			if ( maxW > 0 ) {
				r.linkWeightScale = float( maxW / kFourPi / 65535.0 );
				for ( size_t i = 0; i < keep; i++ ) {
					const Key & k = ord[i].k;
					const qint64 dx = qint64( k.x ) - pk.x, dy = qint64( k.y ) - pk.y, dz = qint64( k.z ) - pk.z;
					if ( dx < -32767 || dx > 32767 || dy < -32767 || dy > 32767 || dz < -32767 || dz > 32767 ) {
						r.unlinkedWeight += float( ord[i].c->w / kFourPi );
						continue;
					}
					const qint64 q = std::llround( ord[i].c->w / maxW * 65535.0 );
					if ( q <= 0 )
						continue;
					TbkLink l;
					l.cellDelta[0] = qint16( dx );
					l.cellDelta[1] = qint16( dy );
					l.cellDelta[2] = qint16( dz );
					l.dir[0] = ord[i].dir[0];
					l.dir[1] = ord[i].dir[1];
					l.weight = quint16( qMin<qint64>( q, 65535 ) );
					po.links.push_back( l );
					po.linkKeys.push_back( k );
				}
			}
			r.linkCount = quint32( po.links.size() );
			po.sx = floorDiv( pp.pos[0], spec.sector );
			po.sy = floorDiv( pp.pos[1], spec.sector );
			ch.probes.push_back( std::move( po ) );
		}
	};

	int nThreads = spec.threads > 0 ? spec.threads : int( std::thread::hardware_concurrency() );
	nThreads = qBound( 1, nThreads, nChunks );
	auto runAll = [&]( const std::function<void( Chunk & )> & fn ) {
		std::atomic<int> next( 0 );
		std::vector<std::thread> pool;
		for ( int t = 0; t < nThreads; t++ )
			pool.emplace_back( [&]() {
				for ( int c = next++; c < nChunks; c = next++ )
					fn( chunks[size_t( c )] );
			} );
		for ( std::thread & t : pool )
			t.join();
	};

	runAll( surfelChunk );
	// merge in chunk order (the same sums whatever ran where), then settle each cell's side
	SurfelMap all;
	for ( Chunk & ch : chunks ) {
		for ( const auto & e : ch.surfels )
			all[e.first].add( e.second );
		SurfelMap().swap( ch.surfels );
	}
	fin.reserve( all.size() );
	// one surfel record from a side's summed samples, its position kept inside `home`
	// (the reader keys a surfel by where it is, so a mean outside would land elsewhere)
	auto makeFinal = [&]( const Bin & a, const Key & home ) {
		Final f;
		TbkSurfel & sr = f.rec;
		std::memset( &sr, 0, sizeof sr );
		const qint32 kk[3] = { home.x, home.y, home.z };
		const double nl = std::sqrt( a.nrm[0] * a.nrm[0] + a.nrm[1] * a.nrm[1] + a.nrm[2] * a.nrm[2] );
		for ( int c = 0; c < 3; c++ ) {
			// a side housed next door has its mean ON the shared face: clamp a hair
			// inside (1/1000 of the cell), and take the centre if float32 still disagrees
			const float lo = float( kk[c] ) * cellS, hi = float( kk[c] + 1 ) * cellS, m = 0.001f * cellS;
			float p = std::clamp( float( a.pos[c] / a.n ), lo + m, hi - m );
			if ( floorDiv( p, cellS ) != kk[c] )
				p = ( float( kk[c] ) + 0.5f ) * cellS;
			sr.position[c] = p;
			const double nc = nl > 0 ? a.nrm[c] / nl : ( c == 2 ? 1.0 : 0.0 );
			sr.normal[c] = qint16( std::lround( std::clamp( nc, -1.0, 1.0 ) * 32767.0 ) );
			sr.albedo[c] = quint8( std::lround( std::clamp( a.alb[c] / a.n, 0.0, 1.0 ) * 255.0 ) );
		}
		sr.samples = a.n;
		const double ql = std::sqrt( double( sr.normal[0] ) * sr.normal[0] + double( sr.normal[1] ) * sr.normal[1]
			+ double( sr.normal[2] ) * sr.normal[2] );
		for ( int c = 0; c < 3; c++ )
			f.n[c] = ql > 0 ? sr.normal[c] / ql : 0.0;
		return f;
	};
	std::vector<std::pair<Key, Bin>> backs;   // the second sides, housed below in key order
	for ( const auto & e : all ) {
		const SurfelBins & sb = e.second;
		int w = 0;
		for ( int i = 1; i < 6; i++ )
			if ( sb.b[i].n > sb.b[w].n )
				w = i;
		if ( !sb.b[w].n )
			continue;
		Bin a, back;
		bool split = false;
		for ( int i = 0; i < 6; i++ ) {
			const Bin & bi = sb.b[i];
			if ( !bi.n )
				continue;
			if ( bi.nrm[0] * sb.b[w].nrm[0] + bi.nrm[1] * sb.b[w].nrm[1] + bi.nrm[2] * sb.b[w].nrm[2] < 0 ) {
				split = true;
				back.add( bi );
				continue;
			}
			a.add( bi );
		}
		R.twoSided += split ? 1 : 0;
		if ( split && spec.spill )
			backs.emplace_back( e.first, back );
		fin.emplace( e.first, makeFinal( a, e.first ) );
	}
	/* THE SECOND SIDE'S HOME: a neighbour on the side the back face looks into --
	 * the 26 around the cell, tried from the one most along the back normal (the
	 * face neighbour first) down to 45 degrees off it. Only a cell nothing was ever
	 * hit in (no surfel of its own) and that no earlier back side took; key order,
	 * so the claim does not depend on hash order. Otherwise the old refusal stands
	 * for that cell. */
	std::sort( backs.begin(), backs.end(), []( const std::pair<Key, Bin> & x, const std::pair<Key, Bin> & y ) {
		return x.first < y.first;
	} );
	for ( const auto & kb : backs ) {
		const Bin & bk = kb.second;
		const double bl = std::sqrt( bk.nrm[0] * bk.nrm[0] + bk.nrm[1] * bk.nrm[1] + bk.nrm[2] * bk.nrm[2] );
		if ( bl <= 0 )
			continue;
		std::pair<double, Key> try26[26];
		int nt = 0;
		for ( int dx = -1; dx <= 1; dx++ )
			for ( int dy = -1; dy <= 1; dy++ )
				for ( int dz = -1; dz <= 1; dz++ ) {
					if ( !dx && !dy && !dz )
						continue;
					const double c = ( dx * bk.nrm[0] + dy * bk.nrm[1] + dz * bk.nrm[2] )
						/ ( bl * std::sqrt( double( dx * dx + dy * dy + dz * dz ) ) );
					Key nb = kb.first;
					nb.x += dx; nb.y += dy; nb.z += dz;
					try26[nt++] = { c, nb };
				}
		std::sort( try26, try26 + nt, []( const std::pair<double, Key> & x, const std::pair<double, Key> & y ) {
			if ( x.first != y.first )
				return x.first > y.first;
			return x.second < y.second;
		} );
		Key nb = kb.first;
		bool found = false;
		for ( int i = 0; i < nt && try26[i].first >= 0.7071 - 1.0e-6; i++ )
			if ( !all.count( try26[i].second ) && !fin.count( try26[i].second ) ) {
				nb = try26[i].second;
				found = true;
				break;
			}
		if ( !found )
			continue;
		fin.emplace( nb, makeFinal( bk, nb ) );
		alt.emplace( kb.first, nb );
		R.spilled++;
	}
	SurfelMap().swap( all );
	runAll( probeChunk );
	for ( const Chunk & ch : chunks ) {
		R.hits += ch.hits;
		R.misses += ch.misses;
		R.linksTurned += ch.turned;
		R.linksSpilled += ch.spilled;
	}
	R.msRays = double( tm.nsecsElapsed() ) / 1e6;
	tm.restart();

	// group by sector: each file = its probes, their links, and every surfel they link
	std::map<std::pair<int, int>, std::vector<const ProbeOut *>> sectors;
	for ( const Chunk & ch : chunks )
		for ( const ProbeOut & po : ch.probes ) {
			sectors[{ po.sx, po.sy }].push_back( &po );
			R.probes++;
			R.skyMean += po.sky;
			R.unlinkedMean += po.rec.unlinkedWeight;
			R.turnedMean += po.turned;
			R.voidMean += po.voidShare;
			R.probesCapped += po.capped ? 1 : 0;
		}
	R.skyMean /= qMax( 1, R.probes );
	R.unlinkedMean /= qMax( 1, R.probes );
	R.turnedMean /= qMax( 1, R.probes );
	R.voidMean /= qMax( 1, R.probes );
	R.noSky = spec.noSky;
	R.rays = qint64( R.probes ) * N;

	if ( !QDir().mkpath( outDir ) ) {
		R.error = QStringLiteral( "cannot create %1" ).arg( outDir );
		return false;
	}
	for ( const auto & sec : sectors ) {
		std::vector<TbkProbe> prs;
		std::vector<TbkLink> lks;
		std::map<Key, int> used;
		for ( const ProbeOut * po : sec.second ) {
			TbkProbe r = po->rec;
			r.linkOffset = quint32( lks.size() );
			lks.insert( lks.end(), po->links.begin(), po->links.end() );
			for ( const Key & k : po->linkKeys )
				used[k] = 0;
			prs.push_back( r );
		}
		std::vector<TbkSurfel> sfs;
		for ( const auto & u : used ) {
			const auto it = fin.find( u.first );
			if ( it != fin.end() )
				sfs.push_back( it->second.rec );
		}
		TbkHeader h;
		h.cellX = sec.first.first;
		h.cellY = sec.first.second;
		h.surfelCellSize = cellS;
		h.surfelCount = quint32( sfs.size() );
		h.probeCount = quint32( prs.size() );
		h.linkCount = quint32( lks.size() );
		h.flags = ( lks.empty() ? 0u : 1u ) | ( prs.empty() ? 0u : 2u );
		char name[64];
		std::snprintf( name, sizeof name, "sector_%+05d_%+05d.tbk", h.cellX, h.cellY );
		const QString path = QDir( outDir ).filePath( QString::fromLatin1( name ) );
		QFile f( path );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			R.error = QStringLiteral( "cannot write %1: %2" ).arg( path, f.errorString() );
			return false;
		}
		f.write( reinterpret_cast<const char *>( &h ), sizeof h );
		f.write( reinterpret_cast<const char *>( sfs.data() ), qint64( sfs.size() * sizeof( TbkSurfel ) ) );
		f.write( reinterpret_cast<const char *>( prs.data() ), qint64( prs.size() * sizeof( TbkProbe ) ) );
		f.write( reinterpret_cast<const char *>( lks.data() ), qint64( lks.size() * sizeof( TbkLink ) ) );
		f.close();
		R.files.append( path );
		R.sectors++;
		R.surfels += int( sfs.size() );
		R.links += int( lks.size() );
	}
	R.msWrite = double( tm.nsecsElapsed() ) / 1e6;
	return true;
}

QString probeBakeCensusText( const ProbeBakeResult & r )
{
	QString s;
	QTextStream t( &s );
	const qint64 all = qMax<qint64>( 1, r.hits + r.misses );
	t << "bake: " << r.probes << " probes, " << ( r.probes ? r.rays / r.probes : 0 ) << " rays each, hits "
	  << QString::number( 100.0 * double( r.hits ) / double( all ), 'f', 1 ) << "%, sky mean "
	  << QString::number( r.skyMean, 'f', 3 );
	if ( r.noSky )
		t << " (interior: no sky; misses unlinked, mean " << QString::number( r.voidMean, 'f', 4 ) << " of the sphere)";
	t << "\n";
	t << "bake: surfels " << r.surfels << ", links " << r.links << ", probes over the link cap " << r.probesCapped
	  << ", unlinked mean " << QString::number( r.unlinkedMean, 'f', 4 ) << ", sectors " << r.sectors << "\n";
	t << "bake: two-sided cells " << r.twoSided << " (second side housed next door " << r.spilled
	  << ", links to it " << r.linksSpilled << "), links refused as turned away " << r.linksTurned
	  << " (mean " << QString::number( r.turnedMean, 'f', 4 ) << " of the sphere)"
	  << ", albedo " << ( r.albedoKnown ? "from the textures" : "GREY (the soup carried none)" ) << "\n";
	t << "bake time ms: rays " << qRound( r.msRays ) << ", write " << qRound( r.msWrite ) << "\n";
	return s;
}

int probeBakeCli( const QStringList & args )
{
	QString soupPath, outDir, rect;
	ProbePlaceSpec ps;
	ProbeBakeSpec bs;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--spacing" ) ) { ps.spacing = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--rays" ) ) { bs.rays = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--threads" ) ) { bs.threads = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { bs.red = nx; i++; }
		else if ( a == QLatin1String( "--no-sky" ) ) { bs.noSky = true; }
		else if ( a == QLatin1String( "--no-spill" ) ) { bs.spill = false; }
		else if ( a == QLatin1String( "--max-links" ) ) { bs.maxLinks = quint32( qBound( 8, nx.toInt(), 4096 ) ); i++; }
		else if ( a == QLatin1String( "--no-openings" ) ) { ps.apertures = false; }
		else if ( a == QLatin1String( "--no-rooms" ) ) { ps.coverage = false; }
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outDir.isEmpty() || rc.size() != 4 ) {
		std::fprintf( stderr, "usage: probebake --soup <file> --rect minX,minY,maxX,maxY --out <dir> "
			"[--rays n] [--threads n] [--spacing s] [--red octant|normal] [--no-sky] [--no-spill] [--max-links n] [--no-openings] [--no-rooms]\n" );
		return 2;
	}
	ps.minX = rc[0].toFloat();
	ps.minY = rc[1].toFloat();
	ps.maxX = rc[2].toFloat();
	ps.maxY = rc[3].toFloat();
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "probebake: %s\n", qPrintable( err ) );
		return 1;
	}
	ProbePlaceResult pr;
	if ( !probePlace( soup, ps, &pr ) ) {
		std::fprintf( stderr, "probebake: placement: %s\n", qPrintable( pr.error ) );
		return 1;
	}
	ProbeBakeResult br;
	if ( !probeBake( soup, pr.probes, bs, outDir, &br ) ) {
		std::fprintf( stderr, "probebake: %s\n", qPrintable( br.error ) );
		return 1;
	}
	std::fputs( qPrintable( probeCensusText( pr ) ), stdout );
	std::fputs( qPrintable( probeBakeCensusText( br ) ), stdout );
	return 0;
}
