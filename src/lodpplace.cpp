/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

/* The release build is -O3 -march=haswell, where GCC fuses a*b+c into one FMA by default. Patch
 * geometry must come out bit for bit what the independent twin (tests/spells/lodp_decode.py)
 * computes, and the 45-degree crease test sits on exact ties, so this unit does plain IEEE
 * arithmetic (lane LODP1, 2026-10-15: FMA noise of 1e-17 split 5 bevelled meshes). */
#pragma GCC optimize( "fp-contract=off" )

#include "lodpplace.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace {

struct V3
{
	double x = 0, y = 0, z = 0;
	V3() = default;
	V3( double a, double b, double c ) : x( a ), y( b ), z( c ) {}
	V3 operator+( const V3 & o ) const { return V3( x + o.x, y + o.y, z + o.z ); }
	V3 operator-( const V3 & o ) const { return V3( x - o.x, y - o.y, z - o.z ); }
	V3 operator*( double s ) const { return V3( x * s, y * s, z * s ); }
};
double dot( const V3 & a, const V3 & b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross( const V3 & a, const V3 & b )
{
	return V3( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x );
}
double len( const V3 & a ) { return std::sqrt( a.x * a.x + a.y * a.y + a.z * a.z ); }

bool isFoliage( const LodoLibrary & lib, const LodoCluster & c )
{
	if ( c.materialId >= lib.materials.size() )
		return false;
	const LodoMaterial & m = lib.materials[c.materialId];
	return ( m.flags & LODO_MAT_TREE ) && m.alphaThreshold != 0;
}

V3 vertexPos( const LodoMesh & m, const LodoVertex & v )
{
	return V3( double( m.aabbMin[0] ) + double( v.pos[0] ) / 65535.0 * double( m.aabbExtent[0] ),
		double( m.aabbMin[1] ) + double( v.pos[1] ) / 65535.0 * double( m.aabbExtent[1] ),
		double( m.aabbMin[2] ) + double( v.pos[2] ) / 65535.0 * double( m.aabbExtent[2] ) );
}

//! One cluster triangle of a mesh, decoded.
struct Tri
{
	quint32 slot;       //!< (cluster - clusterFirst) * 16 + t
	quint8 level;
	bool foliage;
	V3 p[3];
	V3 cr;              //!< cross product (2 x area x normal)
	double crLen;
};

std::vector<Tri> meshTris( const LodoLibrary & lib, const LodoMesh & m )
{
	std::vector<Tri> out;
	for ( quint32 k = 0; k < m.clusterCount; k++ ) {
		quint32 ci = m.clusterFirst + k;
		const LodoCluster & c = lib.clusters[ci];
		const quint8 * li = lib.localIndices.data() + size_t( ci ) * LODO_LOCAL_INDEX_BYTES;
		quint8 level = ci < lib.clusterLods.size() ? lib.clusterLods[ci].level : 0;
		bool fol = isFoliage( lib, c );
		for ( int t = 0; t < c.triangleCount; t++ ) {
			Tri r;
			r.slot = k * LODO_CLUSTER_MAX_TRIS + quint32( t );
			r.level = level;
			r.foliage = fol;
			for ( int e = 0; e < 3; e++ )
				r.p[e] = vertexPos( m, lib.vertices[c.vertexBase + li[3 * t + e]] );
			r.cr = cross( r.p[1] - r.p[0], r.p[2] - r.p[0] );
			r.crLen = len( r.cr );
			out.push_back( r );
		}
	}
	return out;
}

constexpr double DEGENERATE = 1e-6;

} // namespace

bool lodpMeshCharts( const LodoLibrary & lib, quint32 mesh, LodpMeshCharts * out, QString * error )
{
	const LodoMesh & m = lib.meshes[mesh];
	std::vector<Tri> tris = meshTris( lib, m );
	LodpMeshCharts r;
	r.triChart.assign( size_t( m.clusterCount ) * LODO_CLUSTER_MAX_TRIS, LODP_CHART_NONE );
	std::vector<int> chart( tris.size(), -1 );
	std::vector<V3> n( tris.size() );
	for ( size_t t = 0; t < tris.size(); t++ )
		if ( tris[t].crLen > DEGENERATE )
			n[t] = tris[t].cr * ( 1.0 / tris[t].crLen );
	auto chartable = [&]( size_t t ) { return tris[t].level == 0 && !tris[t].foliage && tris[t].crLen > DEGENERATE; };
	int nc = 0;
	std::vector<V3> sum;
	for ( size_t t = 0; t < tris.size(); t++ ) {
		if ( !chartable( t ) || chart[t] >= 0 )
			continue;
		const V3 n0 = n[t];
		for ( size_t u = t; u < tris.size(); u++ )
			if ( chartable( u ) && chart[u] < 0 && dot( n[u], n0 ) >= LODP_CREASE_COS - LODP_CREASE_TIE )
				chart[u] = nc;
		nc++;
	}
	if ( nc > int( LODP_CHART_MAX ) + 1 ) {
		if ( error )
			*error = QString( "mesh %1: %2 charts, more than the stream's %3" ).arg( mesh ).arg( nc ).arg( int( LODP_CHART_MAX ) + 1 );
		return false;
	}
	sum.assign( size_t( nc ), V3() );
	for ( size_t t = 0; t < tris.size(); t++ )
		if ( chart[t] >= 0 )
			sum[size_t( chart[t] )] = sum[size_t( chart[t] )] + tris[t].cr;
	r.normal.assign( size_t( nc ) * 3, 0.0 );
	for ( int c = 0; c < nc; c++ ) {
		double l = len( sum[size_t( c )] );
		V3 u = l > 0 ? sum[size_t( c )] * ( 1.0 / l ) : V3( 0, 0, 1 );
		r.normal[size_t( c ) * 3 + 0] = u.x;
		r.normal[size_t( c ) * 3 + 1] = u.y;
		r.normal[size_t( c ) * 3 + 2] = u.z;
	}
	for ( size_t t = 0; t < tris.size(); t++ ) {
		quint8 id = LODP_CHART_NONE;
		if ( tris[t].foliage ) {
			id = LODP_CHART_FOLIAGE;
			if ( tris[t].level == 0 && tris[t].crLen > DEGENERATE )
				r.foliage = true;
		} else if ( tris[t].crLen > DEGENERATE ) {
			if ( chart[t] >= 0 ) {
				id = quint8( chart[t] );
			} else if ( nc > 0 ) {
				int best = 0;
				double bd = -2.0;
				for ( int c = 0; c < nc; c++ ) {
					double d = n[t].x * r.normal[size_t( c ) * 3] + n[t].y * r.normal[size_t( c ) * 3 + 1] + n[t].z * r.normal[size_t( c ) * 3 + 2];
					if ( d > bd ) {
						bd = d;
						best = c;
					}
				}
				id = quint8( best );
			}
		}
		r.triChart[tris[t].slot] = id;
	}
	r.charts = nc;
	if ( out )
		*out = std::move( r );
	return true;
}

bool lodpChartStream( const LodoLibrary & lib, std::vector<quint8> * stream, std::vector<quint8> * meshCharts, QString * error )
{
	stream->assign( lib.clusters.size() * LODO_CLUSTER_MAX_TRIS, LODP_CHART_NONE );
	meshCharts->assign( lib.meshes.size(), 0 );
	for ( quint32 mi = 0; mi < lib.meshes.size(); mi++ ) {
		LodpMeshCharts mc;
		if ( !lodpMeshCharts( lib, mi, &mc, error ) )
			return false;
		const LodoMesh & m = lib.meshes[mi];
		std::memcpy( stream->data() + size_t( m.clusterFirst ) * LODO_CLUSTER_MAX_TRIS, mc.triChart.data(), mc.triChart.size() );
		( *meshCharts )[mi] = quint8( mc.charts );
	}
	return true;
}

/* ---- placement ---- */

namespace {

using Poly = std::vector<V3>;

//! Keep the side where dot(p - o, a) * sign >= bound * sign.
Poly clipPlane( const Poly & in, const V3 & o, const V3 & a, double bound, bool keepGreater )
{
	Poly out;
	const size_t n = in.size();
	if ( !n )
		return out;
	auto f = [&]( const V3 & p ) { double d = dot( p - o, a ) - bound; return keepGreater ? d : -d; };
	for ( size_t k = 0; k < n; k++ ) {
		const V3 & p = in[k];
		const V3 & q = in[( k + 1 ) % n];
		double fp = f( p ), fq = f( q );
		if ( fp >= 0 )
			out.push_back( p );
		if ( ( fp >= 0 ) != ( fq >= 0 ) ) {
			double t = fp / ( fp - fq );
			out.push_back( p + ( q - p ) * t );
		}
	}
	return out;
}

//! Area and area-weighted centroid of a planar convex polygon (fan from vertex 0).
double polyArea( const Poly & p, V3 * centroidTimesArea )
{
	double a = 0;
	V3 c;
	for ( size_t k = 1; k + 1 < p.size(); k++ ) {
		double t = 0.5 * len( cross( p[k] - p[0], p[k + 1] - p[0] ) );
		a += t;
		c = c + ( p[0] + p[k] + p[k + 1] ) * ( t / 3.0 );
	}
	if ( centroidTimesArea )
		*centroidTimesArea = c;
	return a;
}

//! Closest point on a segment.
V3 closestOnSeg( const V3 & p, const V3 & a, const V3 & b )
{
	V3 ab = b - a;
	double l2 = dot( ab, ab );
	double t = l2 > 0 ? std::clamp( dot( p - a, ab ) / l2, 0.0, 1.0 ) : 0.0;
	return a + ab * t;
}

//! Closest point on a planar convex polygon with unit normal `nrm`.
V3 closestOnPoly( const V3 & p, const Poly & poly, const V3 & nrm )
{
	V3 q = p - nrm * dot( p - poly[0], nrm );
	bool inside = true;
	const size_t n = poly.size();
	for ( size_t k = 0; k < n && inside; k++ ) {
		V3 e = poly[( k + 1 ) % n] - poly[k];
		if ( dot( cross( e, q - poly[k] ), nrm ) < -1e-12 )
			inside = false;
	}
	if ( inside )
		return q;
	V3 best = poly[0];
	double bd = std::numeric_limits<double>::max();
	for ( size_t k = 0; k < n; k++ ) {
		V3 c = closestOnSeg( p, poly[k], poly[( k + 1 ) % n] );
		double d = dot( c - p, c - p );
		if ( d < bd ) {
			bd = d;
			best = c;
		}
	}
	return best;
}

struct LocalPatch
{
	V3 pos, nrm;
	double area = 0;
	bool moved = false;
};

struct LocalChart
{
	quint8 id = 0;
	quint8 kind = LODP_CHART_KIND_SURFACE;
	quint16 nu = 1, nv = 1, nw = 1;
	float step = 1;
	float origin[3], axisU[3], axisN[3];
	std::vector<quint32> slotIds;         //!< local patch index or LODP_NONE
	std::vector<LocalPatch> patches;
	quint32 fallback = 0;
};

struct MeshFrame
{
	LodpMeshCharts mc;
	std::vector<Tri> tris;
	// per surface chart, the float frame as written
	std::vector<std::array<float, 3>> origin, axisU, axisN;
	std::vector<std::array<double, 6>> range;   // (uMin? unused) du/dv/dw max over the chart's vertices
	// foliage
	V3 folMin, folMax;
	double folArea = 0;
};

MeshFrame frameOf( const LodoLibrary & lib, quint32 mesh, QString * error, bool * ok )
{
	MeshFrame f;
	*ok = lodpMeshCharts( lib, mesh, &f.mc, error );
	if ( !*ok )
		return f;
	f.tris = meshTris( lib, lib.meshes[mesh] );
	const int nc = f.mc.charts;
	f.origin.resize( size_t( nc ) );
	f.axisU.resize( size_t( nc ) );
	f.axisN.resize( size_t( nc ) );
	f.range.resize( size_t( nc ) );
	for ( int c = 0; c < nc; c++ ) {
		V3 N( f.mc.normal[size_t( c ) * 3], f.mc.normal[size_t( c ) * 3 + 1], f.mc.normal[size_t( c ) * 3 + 2] );
		V3 up = std::fabs( N.z ) < 0.9 ? V3( 0, 0, 1 ) : V3( 1, 0, 0 );
		V3 U = cross( up, N );
		U = U * ( 1.0 / len( U ) );
		V3 V = cross( N, U );
		double lo[3] = { 1e300, 1e300, 1e300 };
		for ( const Tri & t : f.tris ) {
			if ( t.level != 0 || f.mc.triChart[t.slot] != quint8( c ) )
				continue;
			for ( const V3 & p : t.p ) {
				lo[0] = std::min( lo[0], dot( p, U ) );
				lo[1] = std::min( lo[1], dot( p, V ) );
				lo[2] = std::min( lo[2], dot( p, N ) );
			}
		}
		V3 o = U * lo[0] + V * lo[1] + N * lo[2];
		f.origin[size_t( c )] = { float( o.x ), float( o.y ), float( o.z ) };
		f.axisU[size_t( c )] = { float( U.x ), float( U.y ), float( U.z ) };
		f.axisN[size_t( c )] = { float( N.x ), float( N.y ), float( N.z ) };
	}
	f.folMin = V3( 1e300, 1e300, 1e300 );
	f.folMax = V3( -1e300, -1e300, -1e300 );
	for ( const Tri & t : f.tris ) {
		if ( t.level != 0 || !t.foliage || !( t.crLen > DEGENERATE ) )
			continue;
		f.folArea += 0.5 * t.crLen;
		for ( const V3 & p : t.p ) {
			f.folMin = V3( std::min( f.folMin.x, p.x ), std::min( f.folMin.y, p.y ), std::min( f.folMin.z, p.z ) );
			f.folMax = V3( std::max( f.folMax.x, p.x ), std::max( f.folMax.y, p.y ), std::max( f.folMax.z, p.z ) );
		}
	}
	return f;
}

V3 fv( const std::array<float, 3> & a ) { return V3( a[0], a[1], a[2] ); }

//! Slots and patches of every chart of one mesh at one step (mesh units).
std::vector<LocalChart> slotMesh( const MeshFrame & f, float step, int red )
{
	std::vector<LocalChart> out;
	for ( int c = 0; c < f.mc.charts; c++ ) {
		LocalChart lc;
		lc.id = quint8( c );
		lc.step = step;
		for ( int k = 0; k < 3; k++ ) {
			lc.origin[k] = f.origin[size_t( c )][size_t( k )];
			lc.axisU[k] = f.axisU[size_t( c )][size_t( k )];
			lc.axisN[k] = f.axisN[size_t( c )][size_t( k )];
		}
		const V3 O = fv( f.origin[size_t( c )] ), U = fv( f.axisU[size_t( c )] ), N = fv( f.axisN[size_t( c )] );
		const V3 V = cross( N, U );
		const V3 ax[3] = { U, V, N };
		const double s = double( step );
		std::vector<const Tri *> mine;
		for ( const Tri & t : f.tris )
			if ( t.level == 0 && f.mc.triChart[t.slot] == quint8( c ) )
				mine.push_back( &t );
		if ( red == 2 && mine.size() > 1 )
			mine.pop_back();
		int nmax[3] = { 0, 0, 0 };
		for ( const Tri * t : mine )
			for ( const V3 & p : t->p )
				for ( int a = 0; a < 3; a++ )
					nmax[a] = std::max( nmax[a], int( std::ceil( dot( p - O, ax[a] ) / s ) ) - 1 );
		lc.nu = quint16( nmax[0] + 1 );
		lc.nv = quint16( nmax[1] + 1 );
		lc.nw = quint16( nmax[2] + 1 );
		const int n[3] = { lc.nu, lc.nv, lc.nw };
		const size_t total = size_t( lc.nu ) * lc.nv * lc.nw;
		// accumulate pieces per slot
		std::unordered_map<quint32, std::vector<std::pair<Poly, V3>>> pieces;
		for ( const Tri * t : mine ) {
			int lo[3], hi[3];
			for ( int a = 0; a < 3; a++ ) {
				double mn = 1e300, mx = -1e300;
				for ( const V3 & p : t->p ) {
					double d = dot( p - O, ax[a] );
					mn = std::min( mn, d );
					mx = std::max( mx, d );
				}
				lo[a] = std::clamp( int( std::floor( mn / s ) ), 0, n[a] - 1 );
				hi[a] = std::clamp( int( std::floor( mx / s ) ), 0, n[a] - 1 );
			}
			const V3 tn = t->cr * ( 1.0 / t->crLen );
			Poly tri = { t->p[0], t->p[1], t->p[2] };
			for ( int i = lo[0]; i <= hi[0]; i++ ) {
				Poly pi = tri;
				if ( i > 0 )
					pi = clipPlane( pi, O, U, i * s, true );
				if ( i < n[0] - 1 )
					pi = clipPlane( pi, O, U, ( i + 1 ) * s, false );
				if ( pi.size() < 3 )
					continue;
				for ( int j = lo[1]; j <= hi[1]; j++ ) {
					Poly pj = pi;
					if ( j > 0 )
						pj = clipPlane( pj, O, V, j * s, true );
					if ( j < n[1] - 1 )
						pj = clipPlane( pj, O, V, ( j + 1 ) * s, false );
					if ( pj.size() < 3 )
						continue;
					for ( int k = lo[2]; k <= hi[2]; k++ ) {
						Poly pk = pj;
						if ( k > 0 )
							pk = clipPlane( pk, O, N, k * s, true );
						if ( k < n[2] - 1 )
							pk = clipPlane( pk, O, N, ( k + 1 ) * s, false );
						if ( pk.size() < 3 )
							continue;
						quint32 idx = quint32( ( size_t( k ) * lc.nv + size_t( j ) ) * lc.nu + size_t( i ) );
						pieces[idx].push_back( { std::move( pk ), tn } );
					}
				}
			}
		}
		lc.slotIds.assign( total, LODP_NONE );
		const double minArea = 1e-6 * s * s;
		double bestA = -1;
		for ( size_t idx = 0; idx < total; idx++ ) {
			auto it = pieces.find( quint32( idx ) );
			if ( it == pieces.end() )
				continue;
			LocalPatch lp;
			V3 cs, ns;
			for ( const auto & pc : it->second ) {
				V3 ca;
				double a = polyArea( pc.first, &ca );
				lp.area += a;
				cs = cs + ca;
				ns = ns + pc.second * a;
			}
			if ( !( lp.area > minArea ) )
				continue;
			V3 cen = cs * ( 1.0 / lp.area );
			// onto the nearest piece
			V3 best = cen;
			double bd = std::numeric_limits<double>::max();
			for ( const auto & pc : it->second ) {
				if ( polyArea( pc.first, nullptr ) <= 0 )
					continue;
				V3 q = closestOnPoly( cen, pc.first, pc.second );
				double d = dot( q - cen, q - cen );
				if ( d < bd ) {
					bd = d;
					best = q;
				}
			}
			lp.moved = std::sqrt( bd ) > 1e-4 * s;
			lp.pos = best;
			double nl = len( ns );
			lp.nrm = nl > 0 ? ns * ( 1.0 / nl ) : N;
			lc.slotIds[idx] = quint32( lc.patches.size() );
			if ( lp.area > bestA ) {
				bestA = lp.area;
				lc.fallback = quint32( lc.patches.size() );
			}
			lc.patches.push_back( lp );
		}
		if ( lc.patches.empty() ) {
			// every triangle a sliver below the threshold: one patch at the chart's first vertex keeps the chart drawable
			LocalPatch lp;
			lp.pos = mine.empty() ? O : mine[0]->p[0];
			lp.nrm = N;
			double a = 0;
			for ( const Tri * t : mine )
				a += 0.5 * t->crLen;
			lp.area = std::max( a, minArea );
			size_t idx = 0;
			if ( !mine.empty() ) {
				int ijk[3];
				for ( int q = 0; q < 3; q++ )
					ijk[q] = std::clamp( int( std::floor( dot( lp.pos - O, ax[q] ) / s ) ), 0, n[q] - 1 );
				idx = ( size_t( ijk[2] ) * lc.nv + size_t( ijk[1] ) ) * lc.nu + size_t( ijk[0] );
			}
			lc.slotIds[idx] = 0;
			lc.fallback = 0;
			lc.patches.push_back( lp );
		}
		out.push_back( std::move( lc ) );
	}
	if ( f.mc.foliage ) {
		LocalChart lc;
		lc.id = LODP_CHART_FOLIAGE;
		lc.kind = LODP_CHART_KIND_TREE;
		lc.step = step;
		V3 cen = ( f.folMin + f.folMax ) * 0.5;
		lc.origin[0] = float( f.folMin.x );
		lc.origin[1] = float( f.folMin.y );
		lc.origin[2] = float( f.folMin.z );
		lc.axisU[0] = 1;
		lc.axisU[1] = 0;
		lc.axisU[2] = 0;
		lc.axisN[0] = 0;
		lc.axisN[1] = 0;
		lc.axisN[2] = 1;
		LocalPatch lp;
		lp.pos = cen;
		lp.nrm = V3( 0, 0, 1 );
		lp.area = f.folArea;
		lc.slotIds.assign( 1, 0 );
		lc.patches.push_back( lp );
		out.push_back( std::move( lc ) );
	}
	return out;
}

struct CellBuild
{
	int cx = 0, cy = 0;
	bool terrain = false;
	std::vector<LodpPatch> terr;        //!< pos still in world units, packed later
	std::vector<double> terrZ, terrX, terrY;
	struct Inst
	{
		quint32 lodiIndex;
		quint32 mesh;
		const LodpPlacementIn * in;
		const std::vector<LocalChart> * charts;
	};
	std::vector<Inst> insts;
};

void mulR( const float r[9], const V3 & v, V3 * o )
{
	*o = V3( r[0] * v.x + r[1] * v.y + r[2] * v.z, r[3] * v.x + r[4] * v.y + r[5] * v.z, r[6] * v.x + r[7] * v.y + r[8] * v.z );
}

} // namespace

void lodpPatchWorld( const LodpSector & s, int cellIndex, const LodpPatch & p, double xyz[3] )
{
	int cx = s.sectorX * LODP_SECTOR_CELLS + cellIndex % LODP_SECTOR_CELLS;
	int cy = s.sectorY * LODP_SECTOR_CELLS + cellIndex / LODP_SECTOR_CELLS;
	xyz[0] = double( cx ) * LODP_CELL_UNITS + double( p.pos[0] ) * LODP_POS_STEP;
	xyz[1] = double( cy ) * LODP_CELL_UNITS + double( p.pos[1] ) * LODP_POS_STEP;
	xyz[2] = double( s.cells[size_t( cellIndex )].zBase ) + double( p.pos[2] ) * LODP_POS_STEP;
}

bool lodpPlace( const LodoLibrary & lib, const std::vector<LodpPlacementIn> & placements,
	const LodpTerrainIn * terrain, const LodpPlaceOptions & opt,
	std::map<std::pair<int, int>, LodpSector> * sectors, LodpPlaceStats * stats, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	LodpPlaceStats st;
	const int G = opt.terrainGrid;
	if ( !( opt.patchUnits > 0.0f ) )
		return fail( "patch size must be positive" );
	if ( G < 1 || G > 64 )
		return fail( "terrain grid must be 1..64" );

	// meshes, cut once
	std::unordered_map<quint32, MeshFrame> frames;
	std::map<std::pair<quint32, quint32>, std::vector<LocalChart>> slotted;   // (mesh, step bits)
	std::map<std::pair<int, int>, CellBuild> cells;
	for ( const LodpPlacementIn & in : placements ) {
		st.placements++;
		if ( in.mesh >= lib.meshes.size() )
			return fail( QString( "placement %1: mesh %2 is past the library's %3" ).arg( in.lodiIndex ).arg( in.mesh ).arg( lib.meshes.size() ) );
		if ( !( in.scale > 0.0f ) )
			return fail( QString( "placement %1: scale %2 is not positive" ).arg( in.lodiIndex ).arg( in.scale ) );
		auto fit = frames.find( in.mesh );
		if ( fit == frames.end() ) {
			bool ok = true;
			MeshFrame f = frameOf( lib, in.mesh, error, &ok );
			if ( !ok )
				return false;
			fit = frames.emplace( in.mesh, std::move( f ) ).first;
			st.meshesCut++;
		}
		float step = opt.patchUnits / in.scale;
		quint32 bits;
		std::memcpy( &bits, &step, 4 );
		auto key = std::make_pair( in.mesh, bits );
		auto sit = slotted.find( key );
		if ( sit == slotted.end() )
			sit = slotted.emplace( key, slotMesh( fit->second, step, opt.red ) ).first;
		if ( sit->second.empty() )
			continue;
		int cx = int( std::floor( double( in.pos[0] ) / LODP_CELL_UNITS ) );
		int cy = int( std::floor( double( in.pos[1] ) / LODP_CELL_UNITS ) );
		CellBuild & cb = cells[{ cx, cy }];
		cb.cx = cx;
		cb.cy = cy;
		cb.insts.push_back( { in.lodiIndex, in.mesh, &in, &sit->second } );
		st.placedObjects++;
	}
	st.meshScaleKeys = slotted.size();

	// terrain
	if ( terrain && terrain->samplesPerCell > 0 ) {
		const int spc = terrain->samplesPerCell;
		if ( spc % ( 2 * G ) != 0 )
			return fail( QString( "terrain: %1 samples a cell cannot centre a %2 x %2 grid on a sample (needs a multiple of %3)" )
				.arg( spc ).arg( G ).arg( 2 * G ) );
		const int stp = spc / G;
		const double d = double( LODP_CELL_UNITS ) / spc;
		const int lastX = ( terrain->maxX - terrain->minX + 1 ) * spc - 1;
		const int lastY = ( terrain->maxY - terrain->minY + 1 ) * spc - 1;
		for ( int cy = terrain->minY; cy <= terrain->maxY; cy++ ) {
			for ( int cx = terrain->minX; cx <= terrain->maxX; cx++ ) {
				bool land = false, water = false;
				float wh = 0;
				terrain->cell( cx, cy, &land, &water, &wh );
				if ( !land )
					continue;
				st.landCells++;
				CellBuild & cb = cells[{ cx, cy }];
				cb.cx = cx;
				cb.cy = cy;
				cb.terrain = true;
				const int bx = ( cx - terrain->minX ) * spc, by = ( cy - terrain->minY ) * spc;
				for ( int j = 0; j < G; j++ ) {
					for ( int i = 0; i < G; i++ ) {
						auto H = [&]( int gx, int gy ) { return double( terrain->height( std::min( gx, lastX ), std::min( gy, lastY ) ) ); };
						V3 cr;
						double area = 0;
						for ( int v = 0; v < stp; v++ ) {
							for ( int u = 0; u < stp; u++ ) {
								int gx = bx + i * stp + u, gy = by + j * stp + v;
								V3 p00( 0, 0, H( gx, gy ) ), p10( d, 0, H( gx + 1, gy ) ), p01( 0, d, H( gx, gy + 1 ) ), p11( d, d, H( gx + 1, gy + 1 ) );
								V3 c1 = cross( p10 - p00, p11 - p00 ), c2 = cross( p11 - p00, p01 - p00 );
								cr = cr + c1 + c2;
								area += 0.5 * ( len( c1 ) + len( c2 ) );
							}
						}
						int gx = bx + i * stp + stp / 2, gy = by + j * stp + stp / 2;
						double z = H( gx, gy );
						LodpPatch p;
						std::memset( &p, 0, sizeof p );
						double cl = len( cr );
						float nf[3] = { float( cr.x / cl ), float( cr.y / cl ), float( cr.z / cl ) };
						p.normal = lodpPackOct8( nf );
						p.area = lodpHalfFromFloat( float( area / LODP_AREA_UNIT ) );
						p.kind = LODP_KIND_TERRAIN;
						p.lodSource = quint32( j * G + i );
						if ( water && z < double( wh ) ) {
							p.flags |= LODP_PATCH_SUBMERGED;
							st.submerged++;
						}
						cb.terr.push_back( p );
						cb.terrX.push_back( double( cx ) * LODP_CELL_UNITS + ( i * stp + stp / 2 ) * d );
						cb.terrY.push_back( double( cy ) * LODP_CELL_UNITS + ( j * stp + stp / 2 ) * d );
						cb.terrZ.push_back( z );
					}
				}
			}
		}
	}

	// sectors
	std::map<std::pair<int, int>, LodpSector> out;
	// group cells by sector, in cell-index order
	std::map<std::pair<int, int>, std::vector<CellBuild *>> bySector;
	for ( auto & kv : cells )
		bySector[{ lodpSectorOf( kv.first.first ), lodpSectorOf( kv.first.second ) }].push_back( &kv.second );
	for ( auto & sv : bySector ) {
		LodpSector s;
		s.sectorX = sv.first.first;
		s.sectorY = sv.first.second;
		s.flags = opt.bound ? LODP_FLAG_BOUND : 0;
		s.worldspaceHash = opt.worldspaceHash;
		s.lodInputHash = opt.lodInputHash;
		s.lodoIdentity = opt.lodoIdentity;
		s.patchUnits = opt.patchUnits;
		s.terrainGrid = quint8( G );
		s.optionsHash = lodpOptionsHash( opt.patchUnits, G );
		s.cells.assign( LODP_SECTOR_CELL_COUNT, LodpCell() );
		std::memset( s.cells.data(), 0, s.cells.size() * sizeof( LodpCell ) );
		std::vector<CellBuild *> byIndex( LODP_SECTOR_CELL_COUNT, nullptr );
		for ( CellBuild * cb : sv.second )
			byIndex[size_t( lodpCellIndex( cb->cx, cb->cy, s.sectorX, s.sectorY ) )] = cb;
		struct InstRow { quint32 lodiIndex, firstChart, chartCount, mesh; };
		std::vector<InstRow> rows;
		std::vector<quint32> chartRow;      // chart -> its row in `rows`
		for ( int ci = 0; ci < LODP_SECTOR_CELL_COUNT; ci++ ) {
			LodpCell & cell = s.cells[size_t( ci )];
			cell.firstPatch = quint32( s.patches.size() );
			cell.firstChart = quint32( s.charts.size() );
			CellBuild * cb = byIndex[size_t( ci )];
			if ( !cb ) {
				continue;
			}
			// world positions first, so zBase can be chosen
			struct Pending { LodpPatch p; double x, y, z; };
			std::vector<Pending> pend;
			if ( cb->terrain ) {
				cell.flags |= LODP_CELL_TERRAIN;
				for ( size_t k = 0; k < cb->terr.size(); k++ )
					pend.push_back( { cb->terr[k], cb->terrX[k], cb->terrY[k], cb->terrZ[k] } );
				st.terrainPatches += cb->terr.size();
			}
			std::sort( cb->insts.begin(), cb->insts.end(), []( const CellBuild::Inst & a, const CellBuild::Inst & b ) { return a.lodiIndex < b.lodiIndex; } );
			for ( const CellBuild::Inst & ins : cb->insts ) {
				InstRow row { ins.lodiIndex, quint32( s.charts.size() ), quint32( ins.charts->size() ), ins.mesh };
				const LodpPlacementIn & in = *ins.in;
				const V3 T( in.pos[0], in.pos[1], in.pos[2] );
				for ( const LocalChart & lc : *ins.charts ) {
					quint32 chartIndex = quint32( s.charts.size() );
					LodpChart c;
					std::memset( &c, 0, sizeof c );
					c.firstSlot = quint32( s.slotIds.size() );
					c.nu = lc.nu;
					c.nv = lc.nv;
					c.nw = lc.nw;
					c.meshChart = lc.id;
					c.kind = lc.kind;
					c.step = lc.step;
					std::memcpy( c.origin, lc.origin, 12 );
					std::memcpy( c.axisU, lc.axisU, 12 );
					std::memcpy( c.axisN, lc.axisN, 12 );
					c.instance = quint32( rows.size() );   // the row in creation order; re-pointed after the sort
					quint32 base = quint32( s.patches.size() + pend.size() ) - cell.firstPatch;   // cell-local index of this chart's first patch
					for ( quint32 v : lc.slotIds )
						s.slotIds.push_back( v == LODP_NONE ? LODP_NONE : base + v );   // cell-local for now
					c.fallbackPatch = base + lc.fallback;
					s.charts.push_back( c );
					chartRow.push_back( quint32( rows.size() ) );
					st.charts++;
					if ( lc.kind == LODP_CHART_KIND_TREE )
						st.treeCharts++;
					st.slotIds += lc.slotIds.size();
					for ( const LocalPatch & lp : lc.patches ) {
						LodpPatch p;
						std::memset( &p, 0, sizeof p );
						V3 w, wn;
						mulR( in.rot, lp.pos * double( in.scale ), &w );
						w = w + T;
						if ( lc.kind == LODP_CHART_KIND_TREE ) {
							wn = V3( 0, 0, 1 );
						} else {
							mulR( in.rot, lp.nrm, &wn );
							double l = len( wn );
							wn = wn * ( 1.0 / l );
						}
						if ( opt.red == 1 && lc.kind != LODP_CHART_KIND_TREE )
							w = w + wn * 8.0;
						float nf[3] = { float( wn.x ), float( wn.y ), float( wn.z ) };
						p.normal = lodpPackOct8( nf );
						p.area = lodpHalfFromFloat( float( lp.area * double( in.scale ) * double( in.scale ) / LODP_AREA_UNIT ) );
						if ( !( lodpFloatFromHalf( p.area ) > 0.0f ) )
							p.area = 1;   // the smallest positive half: a sliver still stands for something
						p.kind = lc.kind == LODP_CHART_KIND_TREE ? LODP_KIND_TREE : LODP_KIND_OBJECT;
						p.lodSource = chartIndex;
						pend.push_back( { p, w.x, w.y, w.z } );
						if ( lc.kind == LODP_CHART_KIND_TREE )
							st.treePatches++;
						else
							st.objectPatches++;
						if ( lp.moved )
							st.centroidMoved++;
						double rx = std::max( { double( cb->cx ) * LODP_CELL_UNITS - w.x, w.x - double( cb->cx + 1 ) * LODP_CELL_UNITS,
							double( cb->cy ) * LODP_CELL_UNITS - w.y, w.y - double( cb->cy + 1 ) * LODP_CELL_UNITS, 0.0 } );
						st.maxReach = std::max( st.maxReach, rx );
					}
				}
				rows.push_back( row );
			}
			// zBase and the packing
			double zlo = 1e300, zhi = -1e300;
			for ( const Pending & q : pend ) {
				zlo = std::min( zlo, q.z );
				zhi = std::max( zhi, q.z );
			}
			cell.zBase = pend.empty() ? 0.0f : float( std::floor( ( zlo + zhi ) * 0.5 ) );
			for ( const Pending & q : pend ) {
				LodpPatch p = q.p;
				const double rel[3] = { q.x - double( cb->cx ) * LODP_CELL_UNITS, q.y - double( cb->cy ) * LODP_CELL_UNITS, q.z - double( cell.zBase ) };
				for ( int k = 0; k < 3; k++ ) {
					long long v = std::llround( rel[k] / LODP_POS_STEP );
					if ( v < -32768 || v > 32767 )
						return fail( QString( "cell (%1, %2): a patch lies %3 units from the cell's %4 origin, past the i16 position's +-16384" )
							.arg( cb->cx ).arg( cb->cy ).arg( rel[k] ).arg( "xyz"[k] ) );
					p.pos[k] = qint16( v );
				}
				s.patches.push_back( p );
			}
			cell.patchCount = quint32( s.patches.size() ) - cell.firstPatch;
			cell.chartCount = quint32( s.charts.size() ) - cell.firstChart;
			st.maxPatchesPerCell = std::max<quint64>( st.maxPatchesPerCell, cell.patchCount );
			if ( cell.patchCount > 800 )
				st.cellsOverCap++;
			// slots and fallbacks were cell-local: make them sector-local
			for ( quint32 ch = cell.firstChart; ch < cell.firstChart + cell.chartCount; ch++ ) {
				LodpChart & c = s.charts[ch];
				c.fallbackPatch += cell.firstPatch;
				size_t n = size_t( c.nu ) * c.nv * c.nw;
				for ( size_t k = 0; k < n; k++ )
					if ( s.slotIds[c.firstSlot + k] != LODP_NONE )
						s.slotIds[c.firstSlot + k] += cell.firstPatch;
			}
		}
		// the instance table, sorted by lodiIndex; charts re-pointed at their row
		std::vector<quint32> order( rows.size() );
		for ( size_t k = 0; k < order.size(); k++ )
			order[k] = quint32( k );
		std::sort( order.begin(), order.end(), [&]( quint32 a, quint32 b ) { return rows[a].lodiIndex < rows[b].lodiIndex; } );
		std::vector<quint32> rowAt( rows.size() );
		for ( size_t k = 0; k < order.size(); k++ ) {
			const InstRow & r = rows[order[k]];
			rowAt[order[k]] = quint32( k );
			LodpInstance ir;
			std::memset( &ir, 0, sizeof ir );
			ir.lodiIndex = r.lodiIndex;
			ir.firstChart = r.firstChart;
			ir.chartCount = quint16( r.chartCount );
			ir.lodoMesh = r.mesh;
			s.instances.push_back( ir );
		}
		for ( size_t ch = 0; ch < s.charts.size(); ch++ )
			s.charts[ch].instance = rowAt[chartRow[ch]];
		if ( s.patches.empty() )
			continue;
		st.sectors++;
		st.bytes += LODP_HEADER_BYTES + quint64( LODP_SECTOR_CELL_COUNT ) * sizeof( LodpCell ) + s.instances.size() * sizeof( LodpInstance )
			+ s.charts.size() * sizeof( LodpChart ) + s.slotIds.size() * 4 + s.patches.size() * sizeof( LodpPatch );
		out.emplace( sv.first, std::move( s ) );
	}
	if ( sectors )
		*sectors = std::move( out );
	if ( stats )
		*stats = st;
	return true;
}
