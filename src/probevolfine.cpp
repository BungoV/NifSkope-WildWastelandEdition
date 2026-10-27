#include "probevolfine.h"

#include "probebvh.h"

#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <thread>

namespace {

//! n jobs over the machine's threads, or on this thread (a room small enough to run whole on one worker)
void fineFor( size_t n, bool par, const std::function<void( size_t )> & fn )
{
	if ( !par || n < 2 ) {
		for ( size_t k = 0; k < n; k++ )
			fn( k );
		return;
	}
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

struct Coarse
{
	const ProbeFineGrid & g;
	int X, Y, Z;
	explicit Coarse( const ProbeFineGrid & grid ) : g( grid ), X( grid.dims[0] ), Y( grid.dims[1] ), Z( grid.dims[2] ) {}
	bool in( int x, int y, int z ) const { return x >= 0 && y >= 0 && z >= 0 && x < X && y < Y && z < Z; }
	size_t idx( int x, int y, int z ) const { return ( size_t( z ) * size_t( Y ) + size_t( y ) ) * size_t( X ) + size_t( x ); }
	//! the cell holding a world point, -1 off the grid
	long long at( const double p[3] ) const
	{
		int c[3];
		for ( int k = 0; k < 3; k++ )
			c[k] = int( std::floor( ( p[k] - g.origin[k] ) / g.cell ) );
		return in( c[0], c[1], c[2] ) ? (long long)idx( c[0], c[1], c[2] ) : -1;
	}
	//! the segment p -> q crosses air cells only: no triangle touches them, so none can cut it
	bool segAir( const double p[3], const double q[3] ) const
	{
		const double C = g.cell;
		double d[3], L = 0;
		for ( int k = 0; k < 3; k++ ) {
			d[k] = q[k] - p[k];
			L += d[k] * d[k];
		}
		L = std::sqrt( L );
		if ( L <= 0 )
			return true;
		int c[3], step[3];
		double tMax[3], tDelta[3];
		for ( int k = 0; k < 3; k++ ) {
			d[k] /= L;
			const double u = ( p[k] - g.origin[k] ) / C;
			c[k] = int( std::floor( u ) );
			if ( d[k] > 1e-12 ) {
				step[k] = 1;
				tMax[k] = ( c[k] + 1 - u ) * C / d[k];
				tDelta[k] = C / d[k];
			} else if ( d[k] < -1e-12 ) {
				step[k] = -1;
				tMax[k] = ( u - c[k] ) * C / -d[k];
				tDelta[k] = C / -d[k];
			} else {
				step[k] = 0;
				tMax[k] = 1e300;
				tDelta[k] = 1e300;
			}
		}
		for ( ;; ) {
			if ( !in( c[0], c[1], c[2] ) || g.state[idx( c[0], c[1], c[2] )] )
				return false;
			int a = 0;
			if ( tMax[1] < tMax[a] )
				a = 1;
			if ( tMax[2] < tMax[a] )
				a = 2;
			if ( tMax[a] > L )
				return true;
			c[a] += step[a];
			tMax[a] += tDelta[a];
		}
	}
};

}	// namespace

std::vector<std::vector<ProbeVolumeShape>> probeVolumesFineFit( const probebvh::Bvh & bvh, const double bvhO[2],
	const ProbeFineGrid & g, int rooms, const std::vector<double> & yaw, const ProbeFineSpec & spec, ProbeFineStats * stats )
{
	ProbeFineStats S;
	std::vector<std::vector<ProbeVolumeShape>> out( size_t( std::max( 0, rooms ) ) );
	const Coarse cg( g );
	const int X = cg.X, Y = cg.Y, Z = cg.Z;
	const size_t N = size_t( X ) * size_t( Y ) * size_t( Z );
	const double C = g.cell;
	if ( rooms <= 0 || !N )
		return out;
	QElapsedTimer clock;
	clock.start();

	// 1. owners: every air cell its own label; solid and void cells the label of the air nearest them, up to the reach
	// (a face may stand in the walls' solid band on its room's side, never on the other side's)
	std::vector<qint32> owner( N, -2 );
	std::vector<quint8> layer( N, 255 );
	fineFor( size_t( Z ), true, [&]( size_t zs ) {
		for ( int y = 0; y < Y; y++ )
			for ( int x = 0; x < X; x++ ) {
				const size_t i = cg.idx( x, y, int( zs ) );
				if ( !g.state[i] && g.labels[i] >= 0 ) {
					owner[i] = g.labels[i];
					layer[i] = 0;
				}
			}
	} );
	static const int nb6[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for ( int d = 1; d <= spec.ownerReach; d++ ) {
		std::vector<std::vector<std::pair<size_t, qint32>>> got( size_t( Z ) );
		fineFor( size_t( Z ), true, [&]( size_t zs ) {
			const int z = int( zs );
			for ( int y = 0; y < Y; y++ )
				for ( int x = 0; x < X; x++ ) {
					const size_t i = cg.idx( x, y, z );
					if ( layer[i] != 255 )
						continue;
					for ( const int * n : nb6 ) {
						if ( !cg.in( x + n[0], y + n[1], z + n[2] ) )
							continue;
						const size_t j = cg.idx( x + n[0], y + n[1], z + n[2] );
						if ( layer[j] == d - 1 ) {
							got[zs].push_back( { i, owner[j] } );
							break;
						}
					}
				}
		} );
		for ( const auto & v : got )
			for ( const auto & a : v ) {
				owner[a.first] = a.second;
				layer[a.first] = quint8( d );
			}
	}
	std::vector<quint8>().swap( layer );

	// 2. each room's box in its footprint frame (u along the yaw, v across, z up)
	std::vector<std::array<double, 6>> bb( size_t( rooms ), { 1e300, -1e300, 1e300, -1e300, 1e300, -1e300 } );
	std::vector<double> cs( size_t( rooms ) ), sn( size_t( rooms ) );
	for ( int r = 0; r < rooms; r++ ) {
		const double t = r < int( yaw.size() ) ? yaw[size_t( r )] : 0.0;
		cs[size_t( r )] = std::cos( t );
		sn[size_t( r )] = std::sin( t );
	}
	for ( int z = 0; z < Z; z++ )
		for ( int y = 0; y < Y; y++ )
			for ( int x = 0; x < X; x++ ) {
				const size_t i = cg.idx( x, y, z );
				const qint32 lab = g.labels[i];
				if ( lab <= 0 || lab > rooms || g.state[i] )
					continue;
				std::array<double, 6> & b = bb[size_t( lab - 1 )];
				const double c = cs[size_t( lab - 1 )], s = sn[size_t( lab - 1 )];
				for ( int q = 0; q < 4; q++ ) {
					const double wx = g.origin[0] + ( x + ( q & 1 ) ) * C, wy = g.origin[1] + ( y + ( q >> 1 ) ) * C;
					const double u = c * wx + s * wy, v = -s * wx + c * wy;
					b[0] = std::min( b[0], u );
					b[1] = std::max( b[1], u );
					b[2] = std::min( b[2], v );
					b[3] = std::max( b[3], v );
				}
				b[4] = std::min( b[4], double( g.origin[2] ) + z * C );
				b[5] = std::max( b[5], double( g.origin[2] ) + ( z + 1 ) * C );
			}
	S.msOwner = clock.nsecsElapsed() / 1e6;

	std::atomic<qint64> nRays( 0 ), nFree( 0 ), nSnap( 0 ), nPts( 0 ), nRoom( 0 ), nCov( 0 ), nForeign( 0 );
	std::atomic<qint64> nsRays( 0 ), nsCover( 0 );
	auto fitRoom = [&]( int r, bool par ) {
		const std::array<double, 6> & b = bb[size_t( r )];
		if ( b[0] > b[1] )
			return;
		const int room = r + 1;
		const double mg = ( spec.ownerReach + 0.5 ) * C;
		const double lo[3] = { b[0] - mg, b[2] - mg, b[4] - mg }, hi[3] = { b[1] + mg, b[3] + mg, b[5] + mg };
		double h = spec.h;
		auto count = [&]( double hh ) {
			return qint64( std::ceil( ( hi[0] - lo[0] ) / hh ) ) * qint64( std::ceil( ( hi[1] - lo[1] ) / hh ) )
				* qint64( std::ceil( ( hi[2] - lo[2] ) / hh ) );
		};
		if ( count( h ) > spec.maxPoints )
			h *= std::cbrt( double( count( h ) ) / double( spec.maxPoints ) ) * 1.001;
		const int n[3] = { std::max( 1, int( std::ceil( ( hi[0] - lo[0] ) / h ) ) ), std::max( 1, int( std::ceil( ( hi[1] - lo[1] ) / h ) ) ),
			std::max( 1, int( std::ceil( ( hi[2] - lo[2] ) / h ) ) ) };
		const int nx = n[0], ny = n[1], nz = n[2];
		const size_t P = size_t( nx ) * size_t( ny ) * size_t( nz );
		const double c = cs[size_t( r )], s = sn[size_t( r )];
		const double ax[3][3] = { { c, s, 0 }, { -s, c, 0 }, { 0, 0, 1 } };
		auto world = [&]( double i, double j, double k, double w[3] ) {
			const double u = lo[0] + i * h, v = lo[1] + j * h;
			w[0] = c * u - s * v;
			w[1] = s * u + c * v;
			w[2] = lo[2] + k * h;
		};
		auto pidx = [&]( int i, int j, int k ) { return ( size_t( k ) * size_t( ny ) + size_t( j ) ) * size_t( nx ) + size_t( i ); };
		const size_t sj = size_t( nx ), sk = size_t( nx ) * size_t( ny );
		QElapsedTimer t0;
		t0.start();
		// per point: bit 0 the room owns its cell, bit 1 the room's own air (a seed), bit 2 not the room's (foreign)
		std::vector<quint8> fl( P, 0 ), cut( P, 0 );
		fineFor( size_t( nz ), par, [&]( size_t ks ) {
			const int k = int( ks );
			for ( int j = 0; j < ny; j++ )
				for ( int i = 0; i < nx; i++ ) {
					double w[3];
					world( i + 0.5, j + 0.5, k + 0.5, w );
					const long long gi = cg.at( w );
					quint8 f = 4;
					if ( gi >= 0 && owner[size_t( gi )] == room ) {
						f = 1;
						if ( !g.state[size_t( gi )] && g.labels[size_t( gi )] == room )
							f |= 2;
					}
					fl[pidx( i, j, k )] = f;
				}
		} );
		// segments to the +u +v +z neighbour: cut when a triangle lies on them (only where the room owns an end)
		fineFor( size_t( nz ), par, [&]( size_t ks ) {
			const int k = int( ks );
			qint64 nr = 0, nf = 0;
			for ( int j = 0; j < ny; j++ )
				for ( int i = 0; i < nx; i++ ) {
					const size_t a = pidx( i, j, k );
					double p[3];
					world( i + 0.5, j + 0.5, k + 0.5, p );
					quint8 m = 0;
					for ( int e = 0; e < 3; e++ ) {
						const int i2 = i + ( e == 0 ), j2 = j + ( e == 1 ), k2 = k + ( e == 2 );
						if ( i2 >= nx || j2 >= ny || k2 >= nz )
							continue;
						const size_t bq = pidx( i2, j2, k2 );
						if ( !( fl[a] & 1 ) && !( fl[bq] & 1 ) )
							continue;
						double q[3];
						world( i2 + 0.5, j2 + 0.5, k2 + 0.5, q );
						if ( cg.segAir( p, q ) ) {
							nf++;
							continue;
						}
						const double o[3] = { p[0] - bvhO[0], p[1] - bvhO[1], p[2] };
						double th = 0;
						nr++;
						if ( bvh.ray( o, ax[e], h, &th ) )
							m |= quint8( 1 << e );
					}
					cut[a] = m;
				}
			nRays += nr;
			nFree += nf;
		} );
		nsRays += t0.nsecsElapsed();
		t0.start();
		// floods: the room from its own air through uncut segments into what it owns; then foreign from everything
		// else into the owned points the room did not reach. What neither reaches is enclosed (furniture, walls): free.
		std::vector<quint8> cls( P, 0 );   // 1 room, 2 foreign, 0 free
		auto step = [&]( size_t a, int dir, size_t * bq ) -> bool {
			const int i = int( a % sj ), j = int( ( a / sj ) % size_t( ny ) ), k = int( a / sk );
			switch ( dir ) {
			case 0: if ( i + 1 >= nx ) return false; *bq = a + 1; return !( cut[a] & 1 );
			case 1: if ( i == 0 ) return false; *bq = a - 1; return !( cut[a - 1] & 1 );
			case 2: if ( j + 1 >= ny ) return false; *bq = a + sj; return !( cut[a] & 2 );
			case 3: if ( j == 0 ) return false; *bq = a - sj; return !( cut[a - sj] & 2 );
			case 4: if ( k + 1 >= nz ) return false; *bq = a + sk; return !( cut[a] & 4 );
			default: if ( k == 0 ) return false; *bq = a - sk; return !( cut[a - sk] & 4 );
			}
		};
		std::vector<size_t> qu;
		for ( size_t a = 0; a < P; a++ )
			if ( fl[a] & 2 ) {
				cls[a] = 1;
				qu.push_back( a );
			}
		for ( size_t hd = 0; hd < qu.size(); hd++ )
			for ( int dir = 0; dir < 6; dir++ ) {
				size_t bq;
				if ( step( qu[hd], dir, &bq ) && !cls[bq] && ( fl[bq] & 1 ) ) {
					cls[bq] = 1;
					qu.push_back( bq );
				}
			}
		qu.clear();
		for ( int k = 0; k < nz; k++ )
			for ( int j = 0; j < ny; j++ )
				for ( int i = 0; i < nx; i++ ) {
					const size_t a = pidx( i, j, k );
					const bool rim = i == 0 || j == 0 || k == 0 || i == nx - 1 || j == ny - 1 || k == nz - 1;
					if ( !cls[a] && ( ( fl[a] & 4 ) || rim ) ) {
						cls[a] = 2;
						qu.push_back( a );
					}
				}
		for ( size_t hd = 0; hd < qu.size(); hd++ )
			for ( int dir = 0; dir < 6; dir++ ) {
				size_t bq;
				if ( step( qu[hd], dir, &bq ) && !cls[bq] ) {
					cls[bq] = 2;
					qu.push_back( bq );
				}
			}
		std::vector<quint8>().swap( fl );
		qint64 roomPts = 0, foreign = 0;
		for ( size_t a = 0; a < P; a++ ) {
			roomPts += cls[a] == 1;
			foreign += cls[a] == 2;
		}
		nPts += qint64( P );
		nRoom += roomPts;
		nForeign += foreign;
		if ( !roomPts ) {
			nsCover += t0.nsecsElapsed();
			return;
		}
		// depth: steps to the nearest foreign point or the lattice's rim; the deepest uncovered room point seeds a box
		std::vector<quint16> dist( P );
		for ( size_t a = 0; a < P; a++ )
			dist[a] = cls[a] == 2 ? 0 : 65535;
		for ( int k = 0; k < nz; k++ )
			for ( int j = 0; j < ny; j++ )
				for ( int i = 0; i < nx; i++ ) {
					const size_t a = pidx( i, j, k );
					int d = dist[a];
					d = std::min( d, ( i ? dist[a - 1] : 0 ) + 1 );
					d = std::min( d, ( j ? dist[a - sj] : 0 ) + 1 );
					d = std::min( d, ( k ? dist[a - sk] : 0 ) + 1 );
					dist[a] = quint16( std::min( d, 65535 ) );
				}
		for ( int k = nz - 1; k >= 0; k-- )
			for ( int j = ny - 1; j >= 0; j-- )
				for ( int i = nx - 1; i >= 0; i-- ) {
					const size_t a = pidx( i, j, k );
					int d = dist[a];
					d = std::min( d, ( i + 1 < nx ? dist[a + 1] : 0 ) + 1 );
					d = std::min( d, ( j + 1 < ny ? dist[a + sj] : 0 ) + 1 );
					d = std::min( d, ( k + 1 < nz ? dist[a + sk] : 0 ) + 1 );
					dist[a] = quint16( std::min( d, 65535 ) );
				}
		std::vector<size_t> seeds;
		seeds.reserve( size_t( roomPts ) );
		for ( size_t a = 0; a < P; a++ )
			if ( cls[a] == 1 )
				seeds.push_back( a );
		std::stable_sort( seeds.begin(), seeds.end(), [&]( size_t x, size_t y ) { return dist[x] > dist[y]; } );
		std::vector<quint16>().swap( dist );
		// summed tables of foreign and room points: a layer's counts in O(1)
		const size_t tx = size_t( nx ) + 1, ty = size_t( ny ) + 1;
		auto tIdx = [&]( int i, int j, int k ) { return ( size_t( k ) * ty + size_t( j ) ) * tx + size_t( i ); };
		std::vector<qint32> tF( tx * ty * ( size_t( nz ) + 1 ), 0 ), tR( tF.size(), 0 );
		for ( int k = 1; k <= nz; k++ )
			for ( int j = 1; j <= ny; j++ )
				for ( int i = 1; i <= nx; i++ ) {
					const quint8 v = cls[pidx( i - 1, j - 1, k - 1 )];
					for ( int w = 0; w < 2; w++ ) {
						std::vector<qint32> & T = w ? tR : tF;
						T[tIdx( i, j, k )] = ( v == ( w ? 1 : 2 ) ) + T[tIdx( i - 1, j, k )] + T[tIdx( i, j - 1, k )] + T[tIdx( i, j, k - 1 )]
							- T[tIdx( i - 1, j - 1, k )] - T[tIdx( i - 1, j, k - 1 )] - T[tIdx( i, j - 1, k - 1 )] + T[tIdx( i - 1, j - 1, k - 1 )];
					}
				}
		auto sum = [&]( const std::vector<qint32> & T, const int a[3], const int z[3] ) {   // inclusive a .. z
			const int i0 = a[0], j0 = a[1], k0 = a[2], i1 = z[0] + 1, j1 = z[1] + 1, k1 = z[2] + 1;
			return T[tIdx( i1, j1, k1 )] - T[tIdx( i0, j1, k1 )] - T[tIdx( i1, j0, k1 )] - T[tIdx( i1, j1, k0 )]
				+ T[tIdx( i0, j0, k1 )] + T[tIdx( i0, j1, k0 )] + T[tIdx( i1, j0, k0 )] - T[tIdx( i0, j0, k0 )];
		};
		std::vector<quint8> cov( P, 0 );   // 0 open, 1 covered, 2 tried (no seed, still counted as a gain)
		std::vector<std::array<int, 6>> boxes;
		const double h3 = h * h * h;
		int attempts = 0;
		qint64 covered = 0;
		for ( size_t sd : seeds ) {
			if ( int( boxes.size() ) >= spec.maxBoxes || attempts >= spec.maxBoxes * 8 )
				break;
			if ( cov[sd] )
				continue;
			attempts++;
			int blo[3] = { int( sd % sj ), int( ( sd / sj ) % size_t( ny ) ), int( sd / sk ) };
			int bhi[3] = { blo[0], blo[1], blo[2] };
			for ( bool grew = true; grew; ) {
				grew = false;
				for ( int f = 0; f < 6; f++ ) {
					const int a = f / 2;
					int l0[3] = { blo[0], blo[1], blo[2] }, l1[3] = { bhi[0], bhi[1], bhi[2] };
					if ( f & 1 ) {
						if ( blo[a] == 0 )
							continue;
						l0[a] = l1[a] = blo[a] - 1;
					} else {
						if ( bhi[a] + 1 >= n[a] )
							continue;
						l0[a] = l1[a] = bhi[a] + 1;
					}
					if ( sum( tF, l0, l1 ) != 0 || sum( tR, l0, l1 ) == 0 )
						continue;
					( f & 1 ? blo : bhi )[a] = l0[a];
					grew = true;
				}
			}
			qint64 gain = 0;
			for ( int k = blo[2]; k <= bhi[2]; k++ )
				for ( int j = blo[1]; j <= bhi[1]; j++ )
					for ( int i = blo[0]; i <= bhi[0]; i++ ) {
						const size_t a = pidx( i, j, k );
						gain += cls[a] == 1 && cov[a] != 1;
					}
			const bool keep = double( gain ) * h3 >= spec.minGain;
			for ( int k = blo[2]; k <= bhi[2]; k++ )
				for ( int j = blo[1]; j <= bhi[1]; j++ )
					for ( int i = blo[0]; i <= bhi[0]; i++ ) {
						const size_t a = pidx( i, j, k );
						if ( cls[a] == 1 && cov[a] != 1 )
							cov[a] = keep ? 1 : 2;
					}
			if ( !keep )
				continue;
			covered += gain;
			boxes.push_back( { blo[0], blo[1], blo[2], bhi[0], bhi[1], bhi[2] } );
		}
		nCov += covered;
		nsCover += t0.nsecsElapsed();
		// faces onto the triangles: each face sits half a step past its outer points unless a ray from one of the
		// room's points meets a triangle sooner; then it stands at most `poke` past that triangle
		std::vector<ProbeVolumeShape> shapes( boxes.size() );
		fineFor( boxes.size(), par, [&]( size_t bi ) {
			const std::array<int, 6> & B = boxes[bi];
			double fo[6];
			qint64 ns = 0;
			for ( int f = 0; f < 6; f++ ) {
				const int a = f / 2, a1 = ( a + 1 ) % 3, a2 = ( a + 2 ) % 3;
				const double sg = ( f & 1 ) ? -1.0 : 1.0;
				const int L = ( f & 1 ) ? B[a] : B[3 + a];
				const double reach = std::max( 0.0, h * 0.5 - spec.poke );
				double lim = h * 0.5;
				const double d[3] = { sg * ax[a][0], sg * ax[a][1], sg * ax[a][2] };
				for ( int p1 = B[a1]; p1 <= B[3 + a1]; p1++ )
					for ( int p2 = B[a2]; p2 <= B[3 + a2]; p2++ ) {
						int ijk[3];
						ijk[a] = L;
						ijk[a1] = p1;
						ijk[a2] = p2;
						if ( cls[pidx( ijk[0], ijk[1], ijk[2] )] != 1 || reach <= 0 )
							continue;
						double w[3];
						world( ijk[0] + 0.5, ijk[1] + 0.5, ijk[2] + 0.5, w );
						const double o[3] = { w[0] - bvhO[0], w[1] - bvhO[1], w[2] };
						double th = 0;
						ns++;
						if ( bvh.ray( o, d, reach, &th ) )
							lim = std::min( lim, th + spec.poke );
					}
				fo[f] = lim;
			}
			nSnap += ns;
			double l0[3], l1[3];
			for ( int a = 0; a < 3; a++ ) {
				l0[a] = B[a] + 0.5 - fo[2 * a + 1] / h;
				l1[a] = B[3 + a] + 0.5 + fo[2 * a] / h;
			}
			ProbeVolumeShape & sh = shapes[bi];
			sh.type = ProbeShapeType::Box;
			double w[3];
			world( ( l0[0] + l1[0] ) * 0.5, ( l0[1] + l1[1] ) * 0.5, ( l0[2] + l1[2] ) * 0.5, w );
			for ( int a = 0; a < 3; a++ ) {
				sh.center[a] = float( w[a] );
				sh.size[a] = float( ( l1[a] - l0[a] ) * h );
			}
			sh.rotation[2] = float( std::atan2( s, c ) * 180.0 / 3.14159265358979323846 );
		} );
		out[size_t( r )] = std::move( shapes );
	};
	// big rooms one at a time over every thread; the rest a room per thread
	std::vector<int> small;
	for ( int r = 0; r < rooms; r++ ) {
		const std::array<double, 6> & b = bb[size_t( r )];
		if ( b[0] > b[1] )
			continue;
		const double vol = ( b[1] - b[0] + 8 * C ) * ( b[3] - b[2] + 8 * C ) * ( b[5] - b[4] + 8 * C ) / ( spec.h * spec.h * spec.h );
		if ( vol > 400000.0 )
			fitRoom( r, true );
		else
			small.push_back( r );
	}
	fineFor( small.size(), true, [&]( size_t k ) { fitRoom( small[k], false ); } );
	for ( const auto & v : out ) {
		S.boxes += int( v.size() );
		S.rooms += !v.empty();
	}
	S.rays = nRays;
	S.freeEdges = nFree;
	S.snapRays = nSnap;
	S.points = nPts;
	S.roomPts = nRoom;
	S.coveredPts = nCov;
	S.foreignPts = nForeign;
	S.msRays = double( nsRays ) / 1e6;
	S.msCover = double( nsCover ) / 1e6;
	if ( stats )
		*stats = S;
	return out;
}
