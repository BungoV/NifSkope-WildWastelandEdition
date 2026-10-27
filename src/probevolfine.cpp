#include "probevolfine.h"

#include "bakert.h"
#include "probebvh.h"

#include <QElapsedTimer>
#include <QStringList>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <thread>

namespace {

constexpr double kFinePi = 3.14159265358979323846;

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
		std::vector<std::vector<std::pair<size_t, qint32>>> got( static_cast<size_t>( Z ) );
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
	// the room nearest each cell by grid steps (through anything): air past the owned cells is grown into only by its
	// nearest room, so two rooms never both claim it
	std::vector<qint32> nearR( N, 0 );
	{
		std::vector<quint32> q;
		for ( size_t i = 0; i < N; i++ )
			if ( !g.state[i] && g.labels[i] > 0 ) {
				nearR[i] = g.labels[i];
				q.push_back( quint32( i ) );
			}
		for ( size_t hd = 0; hd < q.size(); hd++ ) {
			const size_t i = q[hd];
			const int x = int( i % size_t( X ) ), y = int( ( i / size_t( X ) ) % size_t( Y ) ), z = int( i / ( size_t( X ) * size_t( Y ) ) );
			for ( const int * n : nb6 ) {
				if ( !cg.in( x + n[0], y + n[1], z + n[2] ) )
					continue;
				const size_t j = cg.idx( x + n[0], y + n[1], z + n[2] );
				if ( !nearR[j] ) {
					nearR[j] = nearR[i];
					q.push_back( quint32( j ) );
				}
			}
		}
	}
	// the thresholds as boxes (R = Rz Ry Rx, local = R^T (p - c)), with world bounds
	struct TBox
	{
		double c[3], h[3], r[3][3], lo[3], hi[3];
		bool seg( const double a[3], const double b[3] ) const { return enter( a, b ) >= 0; }
		//! where segment a..b enters the box, as a fraction of it (0 when a is inside); -1 when it misses
		double enter( const double a[3], const double b[3] ) const
		{
			double la[3], lb[3];
			for ( int k = 0; k < 3; k++ ) {
				la[k] = r[0][k] * ( a[0] - c[0] ) + r[1][k] * ( a[1] - c[1] ) + r[2][k] * ( a[2] - c[2] );
				lb[k] = r[0][k] * ( b[0] - c[0] ) + r[1][k] * ( b[1] - c[1] ) + r[2][k] * ( b[2] - c[2] );
			}
			double t0 = 0, t1 = 1;
			for ( int k = 0; k < 3; k++ ) {
				const double d = lb[k] - la[k];
				if ( std::fabs( d ) < 1e-12 ) {
					if ( std::fabs( la[k] ) > h[k] )
						return -1;
					continue;
				}
				double u = ( -h[k] - la[k] ) / d, v = ( h[k] - la[k] ) / d;
				if ( u > v )
					std::swap( u, v );
				t0 = std::max( t0, u );
				t1 = std::min( t1, v );
				if ( t0 > t1 )
					return -1;
			}
			return t0;
		}
	};
	std::vector<TBox> tbox;
	if ( g.cuts )
		for ( const ProbeVolumeShape & s : *g.cuts ) {
			TBox t;
			const double a[3] = { s.rotation[0] * kFinePi / 180.0, s.rotation[1] * kFinePi / 180.0, s.rotation[2] * kFinePi / 180.0 };
			const double ca = std::cos( a[0] ), sa = std::sin( a[0] ), cb = std::cos( a[1] ), sb = std::sin( a[1] );
			const double cc = std::cos( a[2] ), sc = std::sin( a[2] );
			// Rz Ry Rx
			const double m[3][3] = { { cc * cb, cc * sb * sa - sc * ca, cc * sb * ca + sc * sa },
				{ sc * cb, sc * sb * sa + cc * ca, sc * sb * ca - cc * sa }, { -sb, cb * sa, cb * ca } };
			for ( int i = 0; i < 3; i++ ) {
				t.c[i] = s.center[i];
				t.h[i] = s.size[i] * 0.5;
				for ( int j = 0; j < 3; j++ )
					t.r[i][j] = m[i][j];
			}
			for ( int i = 0; i < 3; i++ ) {
				const double e = std::fabs( m[i][0] ) * t.h[0] + std::fabs( m[i][1] ) * t.h[1] + std::fabs( m[i][2] ) * t.h[2];
				t.lo[i] = t.c[i] - e;
				t.hi[i] = t.c[i] + e;
			}
			tbox.push_back( t );
		}
	// the indoor rule on real rays (volcheck's): few of the upper rays reach the sky, few rays leave the reach
	std::vector<std::array<double, 3>> sdirs( size_t( std::max( 1, spec.sphereK ) ) );
	int upper = 0;
	for ( size_t k = 0; k < sdirs.size(); k++ ) {
		const double K = double( sdirs.size() );
		const double z = 1.0 - ( 2.0 * double( k ) + 1.0 ) / K, rr = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
		const double ph = double( k ) * kFinePi * ( 3.0 - std::sqrt( 5.0 ) );
		sdirs[k] = { rr * std::cos( ph ), rr * std::sin( ph ), z };
		upper += z > 0;
	}
	const double skyLim = spec.skyMax * upper, openLim = spec.openMax * double( sdirs.size() );
	std::atomic<qint64> nEnc( 0 ), nGrown( 0 ), nLost( 0 ), nPruned( 0 );

	// 2. each room's box in its footprint frame (u along the yaw, v across, z up)
	std::vector<std::array<double, 6>> bb( size_t( rooms ), { 1e300, -1e300, 1e300, -1e300, 1e300, -1e300 } );
	std::vector<double> cs( static_cast<size_t>( rooms ) ), sn( static_cast<size_t>( rooms ) );
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
		const double mg = ( spec.ownerReach + 0.5 + spec.grow ) * C;
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
		// per point: bit 0 the room owns its cell, bit 1 the room's own air (a seed), bit 2 not the room's (foreign),
		// bit 3 nearest this room but not owned (grown into when the indoor rule holds)
		std::vector<quint8> fl( P, 0 ), cut( P, 0 );
		// the thresholds near this lattice
		std::vector<const TBox *> thrNear;
		{
			double wl[3] = { 1e300, 1e300, 1e300 }, wh[3] = { -1e300, -1e300, -1e300 };
			for ( int q = 0; q < 8; q++ ) {
				double w[3];
				world( ( q & 1 ) ? nx : 0, ( q & 2 ) ? ny : 0, ( q & 4 ) ? nz : 0, w );
				for ( int k = 0; k < 3; k++ ) {
					wl[k] = std::min( wl[k], w[k] );
					wh[k] = std::max( wh[k], w[k] );
				}
			}
			for ( const TBox & t : tbox ) {
				bool in = true;
				for ( int k = 0; k < 3; k++ )
					in = in && t.hi[k] >= wl[k] && t.lo[k] <= wh[k];
				if ( in )
					thrNear.push_back( &t );
			}
		}
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
					} else if ( gi >= 0 && owner[size_t( gi )] <= 0 && nearR[size_t( gi )] == room ) {
						f = 8;
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
						if ( !( fl[a] & 9 ) && !( fl[bq] & 9 ) )
							continue;
						double q[3];
						world( i2 + 0.5, j2 + 0.5, k2 + 0.5, q );
						bool thr = false;
						for ( const TBox * t : thrNear )
							if ( t->seg( p, q ) ) {
								thr = true;
								break;
							}
						if ( thr ) {
							m |= quint8( 1 << e );
							continue;
						}
						if ( !spec.exactEdges && cg.segAir( p, q ) ) {
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
		// volcheck's per-point rule on real rays, for every point the room may take: bit 0 indoors (few upper rays reach
		// the sky, few leave the reach), bit 1 solid (most rays meet a triangle and most of those its back: inside geometry)
		// Points are classed lazily: the room's own air first, then each wave of the flood's frontier (most of the
		// lattice is never reached, and the rays are the fit's cost)
		std::vector<quint8> pc( P, 0 ), known( P, 0 );
		auto classify = [&]( const std::vector<size_t> & ids ) {
			const size_t K = sdirs.size(), per = std::max<size_t>( 1, size_t( par ? 4000000 : 500000 ) / K ), chunk = 256;
			std::vector<bakert::XRay> rays;
			std::vector<bakert::XHit> hits;
			for ( size_t b = 0; b < ids.size(); b += per ) {
				const size_t e = std::min( ids.size(), b + per );
				rays.resize( ( e - b ) * K );
				hits.resize( rays.size() );
				const size_t nChunks = ( e - b + chunk - 1 ) / chunk;
				fineFor( nChunks, par, [&]( size_t ci ) {
					const size_t x1 = std::min( e, b + ( ci + 1 ) * chunk );
					for ( size_t x = b + ci * chunk; x < x1; x++ ) {
						const size_t a = ids[x];
						double w[3];
						world( double( a % sj ) + 0.5, double( ( a / sj ) % size_t( ny ) ) + 0.5, double( a / sk ) + 0.5, w );
						for ( size_t k = 0; k < K; k++ ) {
							bakert::XRay & y = rays[( x - b ) * K + k];
							y.o[0] = w[0] - bvhO[0];
							y.o[1] = w[1] - bvhO[1];
							y.o[2] = w[2];
							for ( int q = 0; q < 3; q++ )
								y.d[q] = sdirs[k][size_t( q )];
							y.tmax = 65536.0;
							y.pad = 0;
							if ( !g.scene ) {
								double t = 0;
								int tri = -1;
								hits[( x - b ) * K + k] = bvh.ray( y.o, y.d, y.tmax, &t, &tri ) ? bakert::XHit { t, tri } : bakert::XHit { 0.0, -1 };
							}
						}
					}
				} );
				if ( g.scene )
					g.scene->cast( rays.data(), rays.size(), hits.data(), false );
				fineFor( nChunks, par, [&]( size_t ci ) {
					const size_t x1 = std::min( e, b + ( ci + 1 ) * chunk );
					for ( size_t x = b + ci * chunk; x < x1; x++ ) {
						int sky = 0, open = 0, hit = 0, back = 0;
						for ( size_t k = 0; k < K; k++ ) {
							const bakert::XHit & z = hits[( x - b ) * K + k];
							if ( z.tri < 0 ) {
								open++;
								sky += sdirs[k][2] > 0;
								continue;
							}
							hit++;
							open += z.t > spec.openReach;
							back += g.backFace && g.backFace( z.tri, sdirs[k].data() );
						}
						// bits 4 and 8: near the solid or the outdoor limit by the margin (volcheck's own lattice, a few
						// units away, may read them the other way); the floods stop there
						const double m = spec.margin, mo = spec.marginOpen;
						pc[ids[x]] = quint8( ( sky <= skyLim && open <= openLim ? 1 : 0 ) | ( hit * 2 >= int( K ) && back * 2 >= hit ? 2 : 0 )
							| ( hit * 2 >= m * double( K ) && back * 2 >= m * hit ? 4 : 0 ) | ( sky > mo * skyLim || open > mo * openLim ? 8 : 0 )
							| ( spec.ownedOut > 0 && ( sky > spec.ownedOut * skyLim || open > spec.ownedOut * openLim ) ? 16 : 0 ) );
					}
				} );
			}
			nEnc += qint64( ids.size() );
		};
		{
			std::vector<size_t> ids;
			for ( size_t a = 0; a < P; a++ )
				if ( fl[a] & 2 ) {
					known[a] = 1;
					ids.push_back( a );
				}
			classify( ids );
		}
		std::vector<size_t> qu;
		if ( g.anchors ) {
			// one start, as volcheck's flood: the room's own air nearest the anchor that the anchor sees. The room's
			// grid air the flood never reaches (behind a wall the coarse cells leaked through) is not the room's.
			const float * an = g.anchors + size_t( r ) * 3;
			std::vector<std::pair<double, size_t>> cand;
			for ( size_t a = 0; a < P; a++ )
				if ( ( fl[a] & 2 ) && !( pc[a] & 2 ) ) {
					double w[3];
					world( double( a % sj ) + 0.5, double( ( a / sj ) % size_t( ny ) ) + 0.5, double( a / sk ) + 0.5, w );
					cand.push_back( { ( w[0] - an[0] ) * ( w[0] - an[0] ) + ( w[1] - an[1] ) * ( w[1] - an[1] ) + ( w[2] - an[2] ) * ( w[2] - an[2] ), a } );
				}
			const size_t m = std::min<size_t>( 12, cand.size() );
			std::partial_sort( cand.begin(), cand.begin() + std::ptrdiff_t( m ), cand.end() );
			size_t pick = m ? cand[0].second : P;
			for ( size_t c = 0; c < m; c++ ) {
				double w[3];
				const size_t a = cand[c].second;
				world( double( a % sj ) + 0.5, double( ( a / sj ) % size_t( ny ) ) + 0.5, double( a / sk ) + 0.5, w );
				double d[3] = { w[0] - an[0], w[1] - an[1], w[2] - an[2] };
				const double len = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
				if ( len > 1e-6 ) {
					for ( double & x : d )
						x /= len;
					const double o[3] = { an[0] - bvhO[0], an[1] - bvhO[1], double( an[2] ) };
					double th = 0;
					if ( bvh.ray( o, d, len, &th ) )
						continue;
				}
				pick = a;
				break;
			}
			if ( pick < P ) {
				cls[pick] = 1;
				qu.push_back( pick );
			}
		} else {
			for ( size_t a = 0; a < P; a++ )
				if ( ( fl[a] & 2 ) && !( pc[a] & 2 ) ) {
					cls[a] = 1;
					qu.push_back( a );
				}
		}
		const std::vector<size_t> starts = qu;   // the check flood after the fit starts here too
		// past the owned points the flood takes the points nearest the room that the indoor rule holds for; never a
		// solid one (volcheck's flood stops there too)
		std::vector<size_t> need;
		for ( size_t hd = 0; hd < qu.size(); ) {
			const size_t end = qu.size();
			need.clear();
			for ( size_t x = hd; x < end; x++ )
				for ( int dir = 0; dir < 6; dir++ ) {
					size_t bq;
					if ( step( qu[x], dir, &bq ) && !cls[bq] && !known[bq] && ( fl[bq] & 9 ) ) {
						known[bq] = 1;
						need.push_back( bq );
					}
				}
			classify( need );
			for ( ; hd < end; hd++ )
				for ( int dir = 0; dir < 6; dir++ ) {
					size_t bq;
					if ( !step( qu[hd], dir, &bq ) || cls[bq] || ( pc[bq] & 6 ) )
						continue;
					if ( ( fl[bq] & 1 ) && !( pc[bq] & 16 ) ) {
						cls[bq] = 1;
						qu.push_back( bq );
					} else if ( ( fl[bq] & 8 ) && pc[bq] == 1 ) {
						cls[bq] = 1;
						qu.push_back( bq );
						nGrown++;
					}
				}
		}
		qu.clear();
		for ( int k = 0; k < nz; k++ )
			for ( int j = 0; j < ny; j++ )
				for ( int i = 0; i < nx; i++ ) {
					const size_t a = pidx( i, j, k );
					const bool rim = i == 0 || j == 0 || k == 0 || i == nx - 1 || j == ny - 1 || k == nz - 1;
					if ( !cls[a] && ( ( fl[a] & 4 ) || ( ( fl[a] & 2 ) && !( pc[a] & 2 ) ) || rim || ( ( fl[a] & 8 ) && known[a] && !( pc[a] & 1 ) ) ) ) {
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
		bool dumpThis = qEnvironmentVariableIsSet( "WW_VOLFIT_DUMP" );
		if ( dumpThis ) {   // WW_VOLFIT_DUMPROOM = the room's label, or x,y,z its anchor (within 4 u)
			const QStringList dr = qEnvironmentVariable( "WW_VOLFIT_DUMPROOM" ).split( QLatin1Char( ',' ) );
			if ( dr.size() == 3 && g.anchors ) {
				const float * an = g.anchors + size_t( r ) * 3;
				double d2 = 0;
				for ( int k = 0; k < 3; k++ )
					d2 += ( dr[k].toDouble() - an[k] ) * ( dr[k].toDouble() - an[k] );
				dumpThis = d2 <= 16.0;
			} else {
				dumpThis = dr.size() == 1 && dr[0].toInt() == room;
			}
		}
		if ( dumpThis ) {
			// a gate's look at one room: every lattice point a box may take (room 1, free 0), its flags and indoor test
			if ( std::FILE * fo = std::fopen( qEnvironmentVariable( "WW_VOLFIT_DUMP" ).toLocal8Bit().constData(), "w" ) ) {
				std::fprintf( fo, "x\ty\tz\tcls\tfl\tpc\n" );
				for ( size_t a = 0; a < P; a++ ) {
					if ( cls[a] == 2 )
						continue;
					double w[3];
					world( double( a % sj ) + 0.5, double( ( a / sj ) % size_t( ny ) ) + 0.5, double( a / sk ) + 0.5, w );
					std::fprintf( fo, "%.1f\t%.1f\t%.1f\t%d\t%d\t%d\n", w[0], w[1], w[2], int( cls[a] ), int( fl[a] ), int( pc[a] ) );
				}
				std::fclose( fo );
			}
		}
		std::vector<quint8>().swap( fl );
		std::vector<quint8>().swap( known );
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
		std::vector<std::array<int, 6>> boxes;
		qint64 covered = 0;
		for ( int pass = 0;; pass++ ) {
			boxes.clear();
			covered = 0;
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
							T[tIdx( i, j, k )] = ( w ? v == 1 : ( v == 2 || ( v == 0 && !spec.freeD ) ) ) + T[tIdx( i - 1, j, k )] + T[tIdx( i, j - 1, k )] + T[tIdx( i, j, k - 1 )]
								- T[tIdx( i - 1, j - 1, k )] - T[tIdx( i - 1, j, k - 1 )] - T[tIdx( i, j - 1, k - 1 )] + T[tIdx( i - 1, j - 1, k - 1 )];
						}
					}
			auto sum = [&]( const std::vector<qint32> & T, const int a[3], const int z[3] ) {   // inclusive a .. z
				const int i0 = a[0], j0 = a[1], k0 = a[2], i1 = z[0] + 1, j1 = z[1] + 1, k1 = z[2] + 1;
				return T[tIdx( i1, j1, k1 )] - T[tIdx( i0, j1, k1 )] - T[tIdx( i1, j0, k1 )] - T[tIdx( i1, j1, k0 )]
					+ T[tIdx( i0, j0, k1 )] + T[tIdx( i0, j1, k0 )] + T[tIdx( i1, j0, k0 )] - T[tIdx( i0, j0, k0 )];
			};
			std::vector<quint8> cov( P, 0 );   // 0 open, 1 covered, 2 tried (no seed, still counted as a gain)
			const double h3 = h * h * h;
			int attempts = 0;
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
						// a layer the earlier boxes already hold adds only overlap (the volumes pass draws every shape)
						bool fresh = false;
						for ( int k = l0[2]; k <= l1[2] && !fresh; k++ )
							for ( int j = l0[1]; j <= l1[1] && !fresh; j++ )
								for ( int i = l0[0]; i <= l1[0]; i++ ) {
									const size_t q = pidx( i, j, k );
									if ( cls[q] == 1 && cov[q] != 1 ) {
										fresh = true;
										break;
									}
								}
						if ( !fresh )
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
			if ( pass >= spec.checkPasses )
				break;
			// the check: volcheck's flood over the fit. It passes the room's boxes and, past them, only room points
			// the indoor rule holds for; never a solid point. Room points it misses would read as pokes: foreign, refit.
			std::vector<quint8> inB( P, 0 ), vis( P, 0 );
			for ( const std::array<int, 6> & B : boxes )
				for ( int k = B[2]; k <= B[5]; k++ )
					for ( int j = B[1]; j <= B[4]; j++ )
						for ( int i = B[0]; i <= B[3]; i++ )
							inB[pidx( i, j, k )] = 1;
			std::vector<size_t> vq;
			for ( size_t a : starts )
				if ( cls[a] == 1 && !vis[a] ) {
					vis[a] = 1;
					vq.push_back( a );
				}
			for ( size_t hd = 0; hd < vq.size(); hd++ )
				for ( int dir = 0; dir < 6; dir++ ) {
					size_t bq;
					if ( !step( vq[hd], dir, &bq ) || vis[bq] || ( pc[bq] & 6 ) )
						continue;
					if ( inB[bq] || ( cls[bq] == 1 && ( pc[bq] & 9 ) == 1 ) ) {
						vis[bq] = 1;
						vq.push_back( bq );
					}
				}
			qint64 lost = 0;
			for ( size_t a = 0; a < P; a++ )
				if ( cls[a] == 1 && !vis[a] ) {
					cls[a] = 2;
					lost++;
				}
			nLost += lost;
			roomPts -= lost;
			if ( !lost )
				break;
			if ( !roomPts ) {
				boxes.clear();
				covered = 0;
				break;
			}
		}
		nCov += covered;
		nsCover += t0.nsecsElapsed();
		// faces onto the triangles: when every room point on a face meets a triangle (or a threshold) within a step,
		// the face stands at the nearest of them, at most `poke` past a triangle: the lattice stops up to a step short
		// of a wall and volcheck reads that sliver as a gap. Otherwise it sits half a step past its outer points
		// (open air or another box's points) unless a triangle is nearer.
		std::vector<ProbeVolumeShape> shapes( boxes.size() );
		std::vector<std::array<double, 6>> ext( boxes.size() );   // the snapped box in lattice units (one frame per room)
		fineFor( boxes.size(), par, [&]( size_t bi ) {
			const std::array<int, 6> & B = boxes[bi];
			double fo[6];
			qint64 ns = 0;
			for ( int f = 0; f < 6; f++ ) {
				const int a = f / 2, a1 = ( a + 1 ) % 3, a2 = ( a + 2 ) % 3;
				const double sg = ( f & 1 ) ? -1.0 : 1.0;
				const int L = ( f & 1 ) ? B[a] : B[3 + a];
				const double reach = h * spec.snapSteps;
				double nearT = 1e300;
				bool all = true;
				const qint64 ns0 = ns;
				const double d[3] = { sg * ax[a][0], sg * ax[a][1], sg * ax[a][2] };
				for ( int p1 = B[a1]; p1 <= B[3 + a1]; p1++ )
					for ( int p2 = B[a2]; p2 <= B[3 + a2]; p2++ ) {
						int ijk[3];
						ijk[a] = L;
						ijk[a1] = p1;
						ijk[a2] = p2;
						if ( cls[pidx( ijk[0], ijk[1], ijk[2] )] != 1 )
							continue;
						double w[3];
						world( ijk[0] + 0.5, ijk[1] + 0.5, ijk[2] + 0.5, w );
						// from a unit behind the point: a point on a floor's plane still meets the floor
						const double o[3] = { w[0] - d[0] - bvhO[0], w[1] - d[1] - bvhO[1], w[2] - d[2] };
						double th = 0, t = 1e300;
						ns++;
						int tri = -1;
						bool hitT = bvh.ray( o, d, reach + 1.0, &th, &tri );
						if ( hitT && th < 1.001 && g.backFace && g.backFace( tri, d ) ) {
							// the wall the point sits on, seen from behind: cast from the point itself
							const double o2[3] = { w[0] - bvhO[0], w[1] - bvhO[1], w[2] };
							hitT = bvh.ray( o2, d, reach, &th, &tri );
							th += 1.0;
						}
						// a ray along a jamb's plane grazes it: no wall in front of the face
						if ( hitT && g.facing && std::fabs( g.facing( tri, d ) ) < 0.05 )
							hitT = false;
						if ( hitT ) {   // past half a step: on the triangle (a thin wall has no room for `poke`)
							th = std::max( 0.0, th - 1.0 );
							t = th + spec.poke <= h * 0.5 ? th + spec.poke : std::max( h * 0.5, th );
						}
						const double e[3] = { w[0] + d[0] * reach, w[1] + d[1] * reach, w[2] + d[2] * reach };
						for ( const TBox * tb : thrNear ) {
							const double u = tb->enter( w, e );
							if ( u >= 0 )
								t = std::min( t, u * reach );
						}
						all = all && t < 1e300;
						nearT = std::min( nearT, t );
					}
				// a face with no room point (only points inside furniture or walls the box took) stays on its points
				// past half a step by a unit: the faces of two boxes over neighbouring points overlap, never leave a seam
				fo[f] = ns == ns0 ? 0.0 : all ? nearT : std::min( h * 0.5 + 1.0, nearT );
			}
			nSnap += ns;
			double l0[3], l1[3];
			for ( int a = 0; a < 3; a++ ) {
				l0[a] = B[a] + 0.5 - fo[2 * a + 1] / h;
				l1[a] = B[3 + a] + 0.5 + fo[2 * a] / h;
				ext[bi][a] = l0[a];
				ext[bi][3 + a] = l1[a];
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
		// the greedy pass lays slabs over points inside floors and walls; once the faces snap, the big boxes hold the air
		// beside them: drop (smallest first) every box that holds less than minGain of room points no other box holds
		std::vector<size_t> order( shapes.size() );
		for ( size_t i = 0; i < order.size(); i++ )
			order[i] = i;
		auto vol = [&]( size_t i ) { return ( ext[i][3] - ext[i][0] ) * ( ext[i][4] - ext[i][1] ) * ( ext[i][5] - ext[i][2] ); };
		std::stable_sort( order.begin(), order.end(), [&]( size_t x, size_t y ) { return vol( x ) < vol( y ); } );
		std::vector<char> gone( shapes.size(), 0 );
		const double gainL = spec.minGain / ( h * h * h );
		for ( size_t i : order ) {
			std::vector<size_t> ov;
			for ( size_t j = 0; j < shapes.size(); j++ )
				if ( j != i && !gone[j] && ext[j][0] < ext[i][3] && ext[j][3] > ext[i][0] && ext[j][1] < ext[i][4]
					&& ext[j][4] > ext[i][1] && ext[j][2] < ext[i][5] && ext[j][5] > ext[i][2] )
					ov.push_back( j );
			if ( ov.empty() )
				continue;
			// the room points (lattice centres) this box holds and no other kept box does
			int pl[3], ph[3];
			for ( int a = 0; a < 3; a++ ) {
				pl[a] = std::max( 0, int( std::ceil( ext[i][a] - 0.5 ) ) );
				ph[a] = std::min( n[a] - 1, int( std::floor( ext[i][3 + a] - 0.5 ) ) );
			}
			double left = 0;
			for ( int z = pl[2]; z <= ph[2] && left < gainL; z++ )
				for ( int y = pl[1]; y <= ph[1] && left < gainL; y++ )
					for ( int x = pl[0]; x <= ph[0] && left < gainL; x++ ) {
						if ( cls[pidx( x, y, z )] != 1 )
							continue;
						// a point on a floor's or wall's plane sits on the face the big box snapped to: within volcheck's
						// 4 units it is that box's
						const double m[3] = { x + 0.5, y + 0.5, z + 0.5 }, tl = 4.0 / h;
						bool in = false;
						for ( size_t j : ov )
							if ( m[0] > ext[j][0] - tl && m[0] < ext[j][3] + tl && m[1] > ext[j][1] - tl && m[1] < ext[j][4] + tl
								&& m[2] > ext[j][2] - tl && m[2] < ext[j][5] + tl ) {
								in = true;
								break;
							}
						if ( !in )
							left += 1.0;
					}
			if ( left < gainL )
				gone[i] = 1;
		}
		size_t k = 0;
		for ( size_t i = 0; i < shapes.size(); i++ )
			if ( !gone[i] )
				shapes[k++] = shapes[i];
		nPruned += qint64( shapes.size() - k );
		shapes.resize( k );
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
	S.encTests = nEnc;
	S.encGrown = nGrown;
	S.checkLost = nLost;
	S.pruned = nPruned;
	S.msRays = double( nsRays ) / 1e6;
	S.msCover = double( nsCover ) / 1e6;
	if ( stats )
		*stats = S;
	return out;
}
