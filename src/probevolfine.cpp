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
#include <numeric>
#include <queue>
#include <thread>

namespace {

constexpr double kFinePi = 3.14159265358979323846;

//! volcheck's lattice jitter (src/probevolcheck.cpp), so a room square to the world classes the very points volcheck will
double fineJitter( int x, int y, int z, int axis )
{
	quint32 v = quint32( x ) * 73856093u ^ quint32( y ) * 19349663u ^ quint32( z ) * 83492791u ^ quint32( axis + 1 ) * 2654435761u;
	v ^= v >> 15;
	v *= 0x2c1b3c6du;
	v ^= v >> 12;
	v *= 0x297a2d39u;
	v ^= v >> 15;
	return double( v & 0xFFFF ) / 65535.0 - 0.5;
}

//! VOLFIT5: how many big rooms share the machine; a big room's own fan-outs take their share of the threads
thread_local int tlFineShare = 1;

//! n jobs over the machine's threads, or on this thread (a room small enough to run whole on one worker)
void fineFor( size_t n, bool par, const std::function<void( size_t )> & fn )
{
	if ( !par || n < 2 ) {
		for ( size_t k = 0; k < n; k++ )
			fn( k );
		return;
	}
	const int nThreads = std::max( 1, int( std::thread::hardware_concurrency() ) / std::max( 1, tlFineShare ) );
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

	std::atomic<qint64> nRays( 0 ), nFree( 0 ), nSnap( 0 ), nPts( 0 ), nRoom( 0 ), nCov( 0 ), nForeign( 0 ), nEdgeCast( 0 );
	std::atomic<qint64> nsRays( 0 ), nsCover( 0 ), nsClass( 0 ), nsGreedy( 0 ), nsMoves( 0 );
	// VOLFIT6: the edge stage's profile (thread-summed over rooms; each part is the room's own wall)
	std::atomic<qint64> nsEFl( 0 ), nsESeg( 0 ), nsEBuild( 0 ), nsECast( 0 ), nsEScat( 0 ), nsELock( 0 ), nsEGpu( 0 ), nsEPost( 0 );
	std::atomic<qint64> nECalls( 0 ), nERecast( 0 ), nsSmallMax( 0 ), nsCLock( 0 ), nsCGpu( 0 ), nsCPost( 0 );
	// VOLFIT3: per room the grid fit's score and the moved set's (u3 of uncovered room air + badW x foreign air held),
	// and its extra lattice boxes with what each cuts
	struct V3Room
	{
		bool on = false;
		int nA = 0, faces = 0, merged = 0;
		double scoreA = 0, scoreB = 0;
		std::vector<ProbeVolumeShape> base, extra;
		std::vector<double> gain;
	};
	std::vector<V3Room> v3r( static_cast<size_t>( rooms ) );
	auto fitRoom = [&]( int r, bool par ) {
		const std::array<double, 6> & b = bb[size_t( r )];
		if ( b[0] > b[1] )
			return;
		const int room = r + 1;
		const double mg = ( spec.ownerReach + 0.5 + spec.grow ) * C;
		double lo[3] = { b[0] - mg, b[2] - mg, b[4] - mg };
		const double hi[3] = { b[1] + mg, b[3] + mg, b[5] + mg };
		double h = spec.h;
		auto count = [&]( double hh ) {
			return qint64( std::ceil( ( hi[0] - lo[0] ) / hh ) ) * qint64( std::ceil( ( hi[1] - lo[1] ) / hh ) )
				* qint64( std::ceil( ( hi[2] - lo[2] ) / hh ) );
		};
		if ( count( h ) > spec.maxPoints )
			h *= std::cbrt( double( count( h ) ) / double( spec.maxPoints ) ) * 1.001;
		// a room square to the world takes volcheck's own points: world-aligned cells, each point jittered in its cell
		const bool jit = spec.jitter && !spec.legacy && sn[size_t( r )] == 0.0 && cs[size_t( r )] > 0.0 && h == spec.h;
		int g0[3] = { 0, 0, 0 };
		if ( jit )
			for ( int k = 0; k < 3; k++ ) {
				g0[k] = int( std::floor( lo[k] / h ) );
				lo[k] = g0[k] * h;
			}
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
		// a point in lattice units (its cell's index + where in the cell it sits) and in the world
		auto ptL = [&]( size_t a, double l[3] ) {
			const int ii[3] = { int( a % sj ), int( ( a / sj ) % size_t( ny ) ), int( a / sk ) };
			for ( int k = 0; k < 3; k++ )
				l[k] = ii[k] + 0.5 + ( jit ? 0.5 * fineJitter( g0[0] + ii[0], g0[1] + ii[1], g0[2] + ii[2], k ) : 0.0 );
		};
		auto ptW = [&]( size_t a, double w[3] ) {
			double l[3];
			ptL( a, l );
			world( l[0], l[1], l[2], w );
		};
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
					ptW( pidx( i, j, k ), w );
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
		nsEFl += t0.nsecsElapsed();
		const bakert::CastTimes ct0 = bakert::castTimes();
		QElapsedTimer te;
		// segments to the +u +v +z neighbour: cut when a triangle lies on them (only where the room owns an end).
		// VOLFIT5: with a scene, a slab of slices gathers its rays and casts them as one batch (any-hit on the whole-ray
		// tracer, whose answer is the CPU tree's); the cut bits come out the same
		const bool edgeCast = g.scene && spec.edgeGpu;
		const size_t slab = edgeCast ? std::max<size_t>( 1, size_t( par ? 4000000 / std::max( 1, tlFineShare ) : 500000 ) / ( 3 * sk ) )
			: size_t( nz );
		for ( size_t k0 = 0; k0 < size_t( nz ); k0 += slab ) {
			const size_t k1 = std::min( size_t( nz ), k0 + slab );
			std::vector<std::vector<bakert::XRay>> eRays( edgeCast ? k1 - k0 : 0 );
			std::vector<std::vector<quint32>> eAt( edgeCast ? k1 - k0 : 0 );   // per ray: point a * 4 + axis, within the slice
			te.start();
			fineFor( k1 - k0, par, [&]( size_t ks ) {
				const int k = int( k0 + ks );
				qint64 nr = 0, nf = 0;
				for ( int j = 0; j < ny; j++ )
					for ( int i = 0; i < nx; i++ ) {
						const size_t a = pidx( i, j, k );
						double p[3];
						ptW( a, p );
						quint8 m = 0;
						for ( int e = 0; e < 3; e++ ) {
							const int i2 = i + ( e == 0 ), j2 = j + ( e == 1 ), k2 = k + ( e == 2 );
							if ( i2 >= nx || j2 >= ny || k2 >= nz )
								continue;
							const size_t bq = pidx( i2, j2, k2 );
							if ( !( fl[a] & 9 ) && !( fl[bq] & 9 ) )
								continue;
							double q[3];
							ptW( bq, q );
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
							double dq[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
							const double lq = std::sqrt( dq[0] * dq[0] + dq[1] * dq[1] + dq[2] * dq[2] );
							for ( double & x : dq )
								x /= lq;
							if ( edgeCast ) {
								eRays[ks].push_back( { { o[0], o[1], o[2] }, lq, { dq[0], dq[1], dq[2] }, 0.0 } );
								eAt[ks].push_back( quint32( ( a - size_t( k ) * sk ) * 4 + size_t( e ) ) );
								continue;
							}
							if ( bvh.ray( o, dq, lq, &th ) )
								m |= quint8( 1 << e );
						}
						cut[a] = m;
					}
				nRays += nr;
				nFree += nf;
			} );
			nsESeg += te.nsecsElapsed();
			if ( edgeCast ) {
				te.start();
				std::vector<size_t> at( k1 - k0 + 1, 0 );
				for ( size_t s = 0; s < k1 - k0; s++ )
					at[s + 1] = at[s] + eRays[s].size();
				std::vector<bakert::XRay> rays( at.back() );
				std::vector<bakert::XHit> hits( rays.size() );
				for ( size_t s = 0; s < k1 - k0; s++ ) {
					std::copy( eRays[s].begin(), eRays[s].end(), rays.begin() + std::ptrdiff_t( at[s] ) );
					std::vector<bakert::XRay>().swap( eRays[s] );
				}
				nsEBuild += te.nsecsElapsed();
				te.start();
				if ( !rays.empty() )
					g.scene->cast( rays.data(), rays.size(), hits.data(), g.scene->whole() );
				nsECast += te.nsecsElapsed();
				te.start();
				nEdgeCast += qint64( rays.size() );
				fineFor( k1 - k0, par, [&]( size_t s ) {
					const size_t base = ( k0 + s ) * sk;
					for ( size_t x = 0; x < eAt[s].size(); x++ )
						if ( hits[at[s] + x].tri >= 0 )
							cut[base + eAt[s][x] / 4] |= quint8( 1 << ( eAt[s][x] & 3 ) );
				} );
				nsEScat += te.nsecsElapsed();
			}
		}
		{
			const bakert::CastTimes & ct1 = bakert::castTimes();
			nsELock += ct1.lockNs - ct0.lockNs;
			nsEGpu += ct1.gpuNs - ct0.gpuNs;
			nsEPost += ct1.postNs - ct0.postNs;
			nECalls += ct1.calls - ct0.calls;
			nERecast += ct1.recast - ct0.recast;
		}
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
			QElapsedTimer tc;
			tc.start();
			const bakert::CastTimes cc0 = bakert::castTimes();
			const size_t K = sdirs.size(), per = std::max<size_t>( 1, size_t( par ? 4000000 / std::max( 1, tlFineShare ) : 500000 ) / K ), chunk = 256;
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
						ptW( a, w );
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
						double minT = 1e30;
						for ( size_t k = 0; k < K; k++ ) {
							const bakert::XHit & z = hits[( x - b ) * K + k];
							if ( z.tri < 0 ) {
								open++;
								sky += sdirs[k][2] > 0;
								continue;
							}
							hit++;
							minT = std::min( minT, z.t );
							open += z.t > spec.openReach;
							back += g.backFace && g.backFace( z.tri, sdirs[k].data() );
						}
						// bits 4 and 8: near the solid or the outdoor limit by the margin (volcheck's own lattice, a few
						// units away, may read them the other way); the floods stop there
						const double m = spec.margin, mo = spec.marginOpen;
						pc[ids[x]] = quint8( ( sky <= skyLim && open <= openLim ? 1 : 0 ) | ( hit * 2 >= int( K ) && back * 2 >= hit ? 2 : 0 )
							| ( hit * 2 >= m * double( K ) && back * 2 >= m * hit ? 4 : 0 ) | ( sky > mo * skyLim || open > mo * openLim ? 8 : 0 )
							| ( spec.ownedOut > 0 && ( sky > spec.ownedOut * skyLim || open > spec.ownedOut * openLim ) ? 16 : 0 )
							| ( minT < spec.skin ? 32 : 0 ) );   // bit 32: within volcheck's skin of a triangle (never a real gap)
					}
				} );
			}
			nEnc += qint64( ids.size() );
			nsClass += tc.nsecsElapsed();
			const bakert::CastTimes & cc1 = bakert::castTimes();
			nsCLock += cc1.lockNs - cc0.lockNs;
			nsCGpu += cc1.gpuNs - cc0.gpuNs;
			nsCPost += cc1.postNs - cc0.postNs;
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
					ptW( a, w );
					cand.push_back( { ( w[0] - an[0] ) * ( w[0] - an[0] ) + ( w[1] - an[1] ) * ( w[1] - an[1] ) + ( w[2] - an[2] ) * ( w[2] - an[2] ), a } );
				}
			const size_t m = std::min<size_t>( 12, cand.size() );
			std::partial_sort( cand.begin(), cand.begin() + std::ptrdiff_t( m ), cand.end() );
			size_t pick = m ? cand[0].second : P;
			for ( size_t c = 0; c < m; c++ ) {
				double w[3];
				const size_t a = cand[c].second;
				ptW( a, w );
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
					} else if ( ( fl[bq] & 8 ) && ( pc[bq] & 31 ) == 1 ) {
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
					ptW( a, w );
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
		QElapsedTimer tg;
		tg.start();
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
		nsGreedy += tg.nsecsElapsed();
		if ( !spec.legacy && g.grid && size_t( r ) < g.grid->size() ) {
			t0.start();
			V3Room & V = v3r[size_t( r )];
			const double h3 = h * h * h, bw = spec.badW, bo = std::max( spec.badW, spec.badO );
			// what a shape holding the point costs: 1 room air (-1), 2 foreign air, 3 another room's grid shape (overlap)
			auto cost = [&]( quint8 k ) { return k == 1 ? -1.0 : k == 2 ? bw : k == 3 ? bo : 0.0; };
			const double yawDeg = std::atan2( s, c ) * 180.0 / kFinePi;
			// per point: 1 room air past the skin (a gap when no shape holds it), 2 foreign air (a poke when one does);
			// the room's own solid band is free, never air to cover (covering it reads as a poke)
			std::vector<quint8> kd( P, 0 );
			for ( size_t a = 0; a < P; a++ )
				kd[a] = cls[a] == 1 ? ( ( pc[a] & 34 ) ? 0 : 1 ) : ( cls[a] == 2 && !( pc[a] & 6 ) ) ? 2 : 0;
			using Ext = std::array<double, 6>;   // a box in lattice units: lo u v z, hi u v z
			auto forBox = [&]( const Ext & e, const auto & fn ) {
				int i0[3], i1[3];
				for ( int k = 0; k < 3; k++ ) {   // a point sits within a quarter step of its cell's middle
					i0[k] = std::max( 0, int( std::floor( e[k] - 0.75 ) ) );
					i1[k] = std::min( n[k] - 1, int( std::ceil( e[3 + k] - 0.25 ) ) );
					if ( i0[k] > i1[k] )
						return;
				}
				for ( int z = i0[2]; z <= i1[2]; z++ )
					for ( int y = i0[1]; y <= i1[1]; y++ )
						for ( int x = i0[0]; x <= i1[0]; x++ ) {
							const size_t a = pidx( x, y, z );
							double l[3];
							ptL( a, l );
							if ( l[0] >= e[0] && l[0] <= e[3] && l[1] >= e[1] && l[1] <= e[4] && l[2] >= e[2] && l[2] <= e[5] )
								fn( a, l );
						}
			};
			auto slabOf = [&]( const Ext & e, int aa, double p0, double p1 ) {
				Ext o = e;
				o[aa] = std::min( p0, p1 );
				o[3 + aa] = std::max( p0, p1 );
				return o;
			};
			auto volE = [&]( const Ext & e ) { return ( e[3] - e[0] ) * ( e[4] - e[1] ) * ( e[5] - e[2] ); };
			auto shapeExt = [&]( const ProbeVolumeShape & sh ) {   // a shape's bounding sphere in lattice units
				const double rad = 0.5 * std::sqrt( double( sh.size[0] ) * sh.size[0] + double( sh.size[1] ) * sh.size[1] + double( sh.size[2] ) * sh.size[2] ) / h;
				const double u = c * sh.center[0] + s * sh.center[1], v = -s * sh.center[0] + c * sh.center[1];
				const double m[3] = { ( u - lo[0] ) / h, ( v - lo[1] ) / h, ( sh.center[2] - lo[2] ) / h };
				return Ext { m[0] - rad, m[1] - rad, m[2] - rad, m[0] + rad, m[1] + rad, m[2] + rad };
			};
			// points (air or solid) no flood of this room reached inside another room's grid shape: a box there overlaps it
			// (VOLFIT4: priced apart, a cross-wall overlap must not ride on the room air a move gains beside it)
			for ( size_t q = 0; q < g.grid->size(); q++ )
				if ( q != size_t( r ) )
					for ( const ProbeVolumeShape & sh : ( *g.grid )[q] )
						forBox( shapeExt( sh ), [&]( size_t a, const double * ) {
							if ( kd[a] == 3 || cls[a] == 1 || ( kd[a] == 2 && bo == bw ) )
								return;
							double w[3];
							ptW( a, w );
							const float p[3] = { float( w[0] ), float( w[1] ), float( w[2] ) };
							if ( sh.contains( p ) )
								kd[a] = bo == bw ? 2 : 3;
						} );
			// the grid shapes: boxes square to the frame move; any other shape (a wedge, a cylinder, a turned box) stays
			const std::vector<ProbeVolumeShape> & gs = ( *g.grid )[size_t( r )];
			std::vector<Ext> bx;
			std::vector<ProbeVolumeShape> fixedS;
			for ( const ProbeVolumeShape & sh : gs ) {
				double d = std::fmod( double( sh.rotation[2] ) - yawDeg, 180.0 );
				if ( d < 0 )
					d += 180.0;
				const bool sq = d < 0.01 || d > 179.99, sw = std::fabs( d - 90.0 ) < 0.01;
				if ( sh.type != ProbeShapeType::Box || std::fabs( sh.rotation[0] ) > 0.01f || std::fabs( sh.rotation[1] ) > 0.01f || !( sq || sw ) ) {
					fixedS.push_back( sh );
					continue;
				}
				const double u = c * sh.center[0] + s * sh.center[1], v = -s * sh.center[0] + c * sh.center[1];
				const double m[3] = { ( u - lo[0] ) / h, ( v - lo[1] ) / h, ( sh.center[2] - lo[2] ) / h };
				const double hs[3] = { ( sw ? sh.size[1] : sh.size[0] ) * 0.5 / h, ( sw ? sh.size[0] : sh.size[1] ) * 0.5 / h, sh.size[2] * 0.5 / h };
				bx.push_back( { m[0] - hs[0], m[1] - hs[1], m[2] - hs[2], m[0] + hs[0], m[1] + hs[1], m[2] + hs[2] } );
			}
			V.nA = int( gs.size() );
			std::vector<quint8> cnt0( P, 0 ), cnt;   // shapes holding each point: the fixed ones, then all
			for ( const ProbeVolumeShape & sh : fixedS ) {
				forBox( shapeExt( sh ), [&]( size_t a, const double * ) {
					double w[3];
					ptW( a, w );
					const float p[3] = { float( w[0] ), float( w[1] ), float( w[2] ) };
					if ( sh.contains( p ) && cnt0[a] < 255 )
						cnt0[a]++;
				} );
			}
			auto addBox = [&]( const Ext & e, int d ) {
				forBox( e, [&]( size_t a, const double * ) { cnt[a] = quint8( std::clamp( int( cnt[a] ) + d, 0, 255 ) ); } );
			};
			auto recount = [&]() {
				cnt = cnt0;
				for ( const Ext & e : bx )
					addBox( e, 1 );
			};
			auto score = [&]() {
				double sc = 0;
				for ( size_t a = 0; a < P; a++ )
					sc += kd[a] == 1 ? ( cnt[a] ? 0.0 : 1.0 ) : cnt[a] ? cost( kd[a] ) : 0.0;
				return sc * h3;
			};
			recount();
			V.scoreA = score();
			// faces move by whole steps (onto the lattice's cell walls, between the points) while the score drops
			std::vector<quint8> moved( bx.size(), 0 );
			for ( int round = 0; round < spec.rounds; round++ ) {
				int now = 0;
				for ( size_t bi = 0; bi < bx.size(); bi++ )
					for ( int f = 0; f < 6; f++ ) {
						Ext & E = bx[bi];
						const int aa = f % 3;
						const bool hiF = f >= 3;
						const double cur = E[f];
						double best = -1e-9, bestPos = cur;
						for ( int pass = 0; pass < 2; pass++ ) {
							const bool grow = pass == 0, up = hiF == grow;
							double acc = 0, p = cur;
							for ( int st = 0; st < 4; st++ ) {
								const double np = up ? std::floor( p ) + 1.0 : std::ceil( p ) - 1.0;
								if ( grow && ( np < 0.0 || np > double( n[aa] ) ) )
									break;
								if ( !grow && ( hiF ? np < E[aa] + 0.5 : np > E[3 + aa] - 0.5 ) )
									break;
								forBox( slabOf( E, aa, p, np ), [&]( size_t a, const double * ) {
									if ( grow && !cnt[a] )
										acc += cost( kd[a] );
									else if ( !grow && cnt[a] == 1 )
										acc -= cost( kd[a] );
								} );
								p = np;
								if ( acc < best ) {
									best = acc;
									bestPos = p;
								}
							}
						}
						if ( bestPos != cur ) {
							addBox( slabOf( E, aa, cur, bestPos ), ( hiF ? bestPos > cur : bestPos < cur ) ? 1 : -1 );
							E[f] = bestPos;
							moved[bi] |= quint8( 1 << f );
							now++;
						}
					}
				V.faces += now;
				if ( !now )
					break;
			}
			// a face with a wall in front of every room point of its outer step stands at the wall (poke past it)
			auto snapFace = [&]( Ext & E, int f ) {
				const int aa = f % 3;
				const bool hiF = f >= 3;
				const double fp = E[f], sg = hiF ? 1.0 : -1.0;
				const double d[3] = { sg * ax[aa][0], sg * ax[aa][1], sg * ax[aa][2] };
				double lim = hiF ? 1e300 : -1e300, edge = hiF ? -1e300 : 1e300;
				bool all = true, any = false;
				qint64 ns = 0;
				forBox( slabOf( E, aa, fp, hiF ? fp - 1.0 : fp + 1.0 ), [&]( size_t a, const double * l ) {
					if ( cls[a] != 1 || !all )
						return;
					any = true;
					edge = hiF ? std::max( edge, l[aa] ) : std::min( edge, l[aa] );
					const double reach = std::fabs( fp - l[aa] ) * h;
					double w[3];
					ptW( a, w );
					const double o[3] = { w[0] - d[0] - bvhO[0], w[1] - d[1] - bvhO[1], w[2] - d[2] };
					double th = 0, t = 1e300;
					int tri = -1;
					ns++;
					bool hitT = bvh.ray( o, d, reach + 1.0, &th, &tri );
					if ( hitT && th < 1.001 && g.backFace && g.backFace( tri, d ) ) {
						const double o2[3] = { w[0] - bvhO[0], w[1] - bvhO[1], w[2] };
						hitT = bvh.ray( o2, d, reach, &th, &tri );
						th += 1.0;
					}
					if ( hitT && g.facing && std::fabs( g.facing( tri, d ) ) < 0.05 )
						hitT = false;
					if ( hitT )
						t = std::max( 0.0, th - 1.0 ) + spec.poke;
					if ( reach > 0 ) {
						const double e[3] = { w[0] + d[0] * reach, w[1] + d[1] * reach, w[2] + d[2] * reach };
						for ( const TBox * tb : thrNear ) {
							const double u = tb->enter( w, e );
							if ( u >= 0 )
								t = std::min( t, u * reach );
						}
					}
					if ( t >= reach ) {
						all = false;
						return;
					}
					lim = hiF ? std::min( lim, l[aa] + t / h ) : std::max( lim, l[aa] - t / h );
				} );
				nSnap += ns;
				if ( any && all )
					E[f] = hiF ? std::max( lim, edge + 1e-3 ) : std::min( lim, edge - 1e-3 );
			};
			for ( size_t bi = 0; bi < bx.size(); bi++ )
				for ( int f = 0; f < 6; f++ )
					if ( moved[bi] & ( 1 << f ) )
						snapFace( bx[bi], f );
			recount();
			// two boxes become one where their joint box takes no foreign air the score would pay for
			for ( bool again = true; again; ) {
				again = false;
				for ( size_t i = 0; i < bx.size() && !again; i++ )
					for ( size_t j = i + 1; j < bx.size() && !again; j++ ) {
						Ext U;
						for ( int k = 0; k < 3; k++ ) {
							U[k] = std::min( bx[i][k], bx[j][k] );
							U[3 + k] = std::max( bx[i][3 + k], bx[j][3 + k] );
						}
						if ( volE( U ) > 1.2 * ( volE( bx[i] ) + volE( bx[j] ) ) )
							continue;
						double dl = 0;
						forBox( U, [&]( size_t a, const double * ) {
							if ( !cnt[a] )
								dl += cost( kd[a] );
						} );
						if ( dl > 1e-9 )
							continue;
						addBox( bx[i], -1 );
						addBox( bx[j], -1 );
						addBox( U, 1 );
						bx[i] = U;
						bx.erase( bx.begin() + std::ptrdiff_t( j ) );
						V.merged++;
						again = true;
					}
			}
			// drop (smallest first) a box whose own share cuts less than minGain of the score
			{
				std::vector<size_t> ord( bx.size() );
				std::iota( ord.begin(), ord.end(), size_t( 0 ) );
				std::stable_sort( ord.begin(), ord.end(), [&]( size_t x, size_t y ) { return volE( bx[x] ) < volE( bx[y] ); } );
				std::vector<char> gone( bx.size(), 0 );
				for ( size_t i : ord ) {
					double val = 0;
					forBox( bx[i], [&]( size_t a, const double * ) {
						if ( cnt[a] == 1 )
							val -= cost( kd[a] );
					} );
					if ( val * h3 < spec.minGain ) {
						addBox( bx[i], -1 );
						gone[i] = 1;
						nPruned++;
					}
				}
				size_t k = 0;
				for ( size_t i = 0; i < bx.size(); i++ )
					if ( !gone[i] )
						bx[k++] = bx[i];
				bx.resize( k );
			}
			V.scoreB = score();
			// extra lattice boxes, greedy over the room air nothing holds yet (deepest first), never on a foreign point
			std::vector<Ext> xb;
			if ( spec.extras > 0 ) {
				std::vector<quint16> dist( P );
				for ( size_t a = 0; a < P; a++ )
					dist[a] = cls[a] == 2 ? 0 : 65535;
				for ( int k = 0; k < nz; k++ )
					for ( int j = 0; j < ny; j++ )
						for ( int i = 0; i < nx; i++ ) {
							const size_t a = pidx( i, j, k );
							int dd = dist[a];
							dd = std::min( dd, ( i ? dist[a - 1] : 0 ) + 1 );
							dd = std::min( dd, ( j ? dist[a - sj] : 0 ) + 1 );
							dd = std::min( dd, ( k ? dist[a - sk] : 0 ) + 1 );
							dist[a] = quint16( std::min( dd, 65535 ) );
						}
				for ( int k = nz - 1; k >= 0; k-- )
					for ( int j = ny - 1; j >= 0; j-- )
						for ( int i = nx - 1; i >= 0; i-- ) {
							const size_t a = pidx( i, j, k );
							int dd = dist[a];
							dd = std::min( dd, ( i + 1 < nx ? dist[a + 1] : 0 ) + 1 );
							dd = std::min( dd, ( j + 1 < ny ? dist[a + sj] : 0 ) + 1 );
							dd = std::min( dd, ( k + 1 < nz ? dist[a + sk] : 0 ) + 1 );
							dist[a] = quint16( std::min( dd, 65535 ) );
						}
				std::vector<size_t> seeds;
				for ( size_t a = 0; a < P; a++ )
					if ( kd[a] == 1 && !cnt[a] )
						seeds.push_back( a );
				std::stable_sort( seeds.begin(), seeds.end(), [&]( size_t x, size_t y ) { return dist[x] > dist[y]; } );
				std::vector<quint16>().swap( dist );
				const size_t tx = size_t( nx ) + 1, ty = size_t( ny ) + 1;
				auto tIdx = [&]( int i, int j, int k ) { return ( size_t( k ) * ty + size_t( j ) ) * tx + size_t( i ); };
				std::vector<qint32> tF( tx * ty * ( size_t( nz ) + 1 ), 0 ), tR( tF.size(), 0 );
				for ( int k = 1; k <= nz; k++ )
					for ( int j = 1; j <= ny; j++ )
						for ( int i = 1; i <= nx; i++ ) {
							const size_t a = pidx( i - 1, j - 1, k - 1 );
							const quint8 v = cls[a];
							for ( int w = 0; w < 2; w++ ) {
								std::vector<qint32> & T = w ? tR : tF;
								T[tIdx( i, j, k )] = ( w ? kd[a] == 1 && !cnt[a] : ( v == 2 || ( v == 0 && !spec.freeD ) ) ) + T[tIdx( i - 1, j, k )] + T[tIdx( i, j - 1, k )]
									+ T[tIdx( i, j, k - 1 )] - T[tIdx( i - 1, j - 1, k )] - T[tIdx( i - 1, j, k - 1 )] - T[tIdx( i, j - 1, k - 1 )] + T[tIdx( i - 1, j - 1, k - 1 )];
							}
						}
				auto sum = [&]( const std::vector<qint32> & T, const int a[3], const int z[3] ) {
					const int i0 = a[0], j0 = a[1], k0 = a[2], i1 = z[0] + 1, j1 = z[1] + 1, k1 = z[2] + 1;
					return T[tIdx( i1, j1, k1 )] - T[tIdx( i0, j1, k1 )] - T[tIdx( i1, j0, k1 )] - T[tIdx( i1, j1, k0 )]
						+ T[tIdx( i0, j0, k1 )] + T[tIdx( i0, j1, k0 )] + T[tIdx( i1, j0, k0 )] - T[tIdx( i0, j0, k0 )];
				};
				std::vector<quint8> cov( P, 0 );
				int attempts = 0;
				std::vector<std::pair<double, Ext>> got;
				for ( size_t sd : seeds ) {
					if ( int( got.size() ) >= spec.extras || attempts >= spec.extras * 8 )
						break;
					if ( cov[sd] )
						continue;
					attempts++;
					int blo[3] = { int( sd % sj ), int( ( sd / sj ) % size_t( ny ) ), int( sd / sk ) };
					int bhi[3] = { blo[0], blo[1], blo[2] };
					for ( bool grew = true; grew; ) {
						grew = false;
						for ( int f = 0; f < 6; f++ ) {
							const int aa = f / 2;
							int l0[3] = { blo[0], blo[1], blo[2] }, l1[3] = { bhi[0], bhi[1], bhi[2] };
							if ( f & 1 ) {
								if ( blo[aa] == 0 )
									continue;
								l0[aa] = l1[aa] = blo[aa] - 1;
							} else {
								if ( bhi[aa] + 1 >= n[aa] )
									continue;
								l0[aa] = l1[aa] = bhi[aa] + 1;
							}
							if ( sum( tF, l0, l1 ) != 0 || sum( tR, l0, l1 ) == 0 )
								continue;
							bool fresh = false;
							for ( int k = l0[2]; k <= l1[2] && !fresh; k++ )
								for ( int j = l0[1]; j <= l1[1] && !fresh; j++ )
									for ( int i = l0[0]; i <= l1[0]; i++ ) {
										const size_t q = pidx( i, j, k );
										if ( kd[q] == 1 && !cnt[q] && cov[q] != 1 ) {
											fresh = true;
											break;
										}
									}
							if ( !fresh )
								continue;
							( f & 1 ? blo : bhi )[aa] = l0[aa];
							grew = true;
						}
					}
					qint64 gain = 0;
					for ( int k = blo[2]; k <= bhi[2]; k++ )
						for ( int j = blo[1]; j <= bhi[1]; j++ )
							for ( int i = blo[0]; i <= bhi[0]; i++ ) {
								const size_t a = pidx( i, j, k );
								gain += kd[a] == 1 && !cnt[a] && cov[a] != 1;
							}
					const bool keep = double( gain ) * h3 >= spec.minGain;
					for ( int k = blo[2]; k <= bhi[2]; k++ )
						for ( int j = blo[1]; j <= bhi[1]; j++ )
							for ( int i = blo[0]; i <= bhi[0]; i++ ) {
								const size_t a = pidx( i, j, k );
								if ( kd[a] == 1 && !cnt[a] && cov[a] != 1 )
									cov[a] = keep ? 1 : 2;
							}
					if ( keep )
						got.push_back( { double( gain ) * h3, Ext { double( blo[0] ), double( blo[1] ), double( blo[2] ), bhi[0] + 1.0, bhi[1] + 1.0, bhi[2] + 1.0 } } );
				}
				std::stable_sort( got.begin(), got.end(), [&]( const std::pair<double, Ext> & x, const std::pair<double, Ext> & y ) { return x.first > y.first; } );
				for ( auto & x : got ) {
					for ( int f = 0; f < 6; f++ )
						snapFace( x.second, f );
					V.gain.push_back( x.first );
					xb.push_back( x.second );
				}
			}
			auto toShape = [&]( const Ext & e ) {
				ProbeVolumeShape sh;
				sh.type = ProbeShapeType::Box;
				double w[3];
				world( ( e[0] + e[3] ) * 0.5, ( e[1] + e[4] ) * 0.5, ( e[2] + e[5] ) * 0.5, w );
				for ( int k = 0; k < 3; k++ ) {
					sh.center[k] = float( w[k] );
					sh.size[k] = float( ( e[3 + k] - e[k] ) * h );
				}
				sh.rotation[2] = float( yawDeg );
				return sh;
			};
			V.base = fixedS;
			for ( const Ext & e : bx )
				V.base.push_back( toShape( e ) );
			for ( const Ext & e : xb )
				V.extra.push_back( toShape( e ) );
			V.on = true;
			nsCover += t0.nsecsElapsed();
			nsMoves += t0.nsecsElapsed();
			return;
		}
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
	// big rooms over every thread, side by side (each over its share of the threads); the rest a room per thread. A room
	// writes only its own slots and sums only counts, so running rooms together changes no byte (bigpar=0: one at a time)
	std::vector<int> big, small;
	QElapsedTimer tb;
	tb.start();
	for ( int r = 0; r < rooms; r++ ) {
		const std::array<double, 6> & b = bb[size_t( r )];
		if ( b[0] > b[1] )
			continue;
		const double vol = ( b[1] - b[0] + 8 * C ) * ( b[3] - b[2] + 8 * C ) * ( b[5] - b[4] + 8 * C ) / ( spec.h * spec.h * spec.h );
		( vol > 400000.0 ? big : small ).push_back( r );
	}
	if ( spec.bigPar && big.size() > 1 ) {
		const int share = int( std::min<size_t>( big.size(), size_t( std::max( 1u, std::thread::hardware_concurrency() ) ) ) );
		fineFor( big.size(), true, [&]( size_t k ) {
			tlFineShare = share;
			fitRoom( big[k], true );
			tlFineShare = 1;
		} );
	} else {
		for ( int r : big )
			fitRoom( r, true );
	}
	S.msBig = tb.nsecsElapsed() / 1e6;
	tb.start();
	fineFor( small.size(), true, [&]( size_t k ) {
		QElapsedTimer tr;
		tr.start();
		fitRoom( small[k], false );
		const qint64 ns = tr.nsecsElapsed();
		for ( qint64 m = nsSmallMax.load(); ns > m && !nsSmallMax.compare_exchange_weak( m, ns ); )
			;
	} );
	S.msSmall = tb.nsecsElapsed() / 1e6;
	if ( !spec.legacy && g.grid ) {
		// VOLFIT3: each room the best of its grid fit and its moved set within the grid fit's count; then the extra boxes
		// that cut the most score per shape, over all rooms, until the budget (budget x the grid fit's shapes)
		std::vector<int> cur( static_cast<size_t>( rooms ), -1 );
		auto nOf = [&]( const V3Room & V, int k ) { return k < 0 ? V.nA : int( V.base.size() ) + k; };
		auto scOf = [&]( const V3Room & V, int k ) {
			if ( k < 0 )
				return V.scoreA;
			double sc = V.scoreB;
			for ( int i = 0; i < k; i++ )
				sc -= V.gain[size_t( i )];
			return sc;
		};
		int total = 0, gridN = 0;
		for ( int r = 0; r < rooms; r++ ) {
			const V3Room & V = v3r[size_t( r )];
			if ( !V.on ) {
				const int ng = size_t( r ) < g.grid->size() ? int( ( *g.grid )[size_t( r )].size() ) : 0;
				total += ng;
				gridN += ng;
				continue;
			}
			gridN += V.nA;
			int best = -1;
			for ( int k = 0; k <= int( V.gain.size() ); k++ )
				if ( nOf( V, k ) > 0 && nOf( V, k ) <= V.nA && scOf( V, k ) < scOf( V, best ) - 1e-6 )
					best = k;
			cur[size_t( r )] = best;
			total += nOf( V, best );
		}
		const int budget = int( std::floor( spec.budget * gridN ) );
		struct Step
		{
			double per;
			int r, k, from;
			bool operator<( const Step & o ) const { return per < o.per; }
		};
		std::priority_queue<Step> pq;
		auto push = [&]( int r ) {
			const V3Room & V = v3r[size_t( r )];
			const int k0 = cur[size_t( r )];
			double bp = 0;
			int bk = -2;
			for ( int k = std::max( 0, k0 + 1 ); k <= int( V.gain.size() ); k++ ) {
				const int dn = nOf( V, k ) - nOf( V, k0 );
				const double ds = scOf( V, k0 ) - scOf( V, k );
				if ( dn > 0 && ds / dn > bp ) {
					bp = ds / dn;
					bk = k;
				}
			}
			if ( bk >= 0 && bp >= spec.minGain )
				pq.push( { bp, r, bk, k0 } );
		};
		for ( int r = 0; r < rooms; r++ )
			if ( v3r[size_t( r )].on )
				push( r );
		while ( !pq.empty() ) {
			const Step st = pq.top();
			pq.pop();
			if ( cur[size_t( st.r )] != st.from )
				continue;
			const V3Room & V = v3r[size_t( st.r )];
			const int dn = nOf( V, st.k ) - nOf( V, st.from );
			if ( total + dn > budget )
				continue;
			total += dn;
			cur[size_t( st.r )] = st.k;
			push( st.r );
		}
		for ( int r = 0; r < rooms; r++ ) {
			const V3Room & V = v3r[size_t( r )];
			S.gridShapes += V.on ? V.nA : 0;
			S.facesMoved += V.faces;
			S.merged += V.merged;
			if ( !V.on )
				continue;
			const int k = cur[size_t( r )];
			if ( k < 0 ) {
				S.pickGrid++;
				continue;
			}
			std::vector<ProbeVolumeShape> o = V.base;
			o.insert( o.end(), V.extra.begin(), V.extra.begin() + k );
			out[size_t( r )] = std::move( o );
			S.pickMoved++;
			S.extrasUsed += k;
		}
	}
	for ( const auto & v : out ) {
		S.boxes += int( v.size() );
		S.rooms += !v.empty();
	}
	S.rays = nRays;
	S.freeEdges = nFree;
	S.edgeCast = nEdgeCast;
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
	S.msClass = double( nsClass ) / 1e6;
	S.msGreedy = double( nsGreedy ) / 1e6;
	S.msMoves = double( nsMoves ) / 1e6;
	S.msWall = clock.nsecsElapsed() / 1e6;
	S.prof = QStringLiteral( " small %1 (n %2 longest %3) big n %4; edge ms fl %5 seg %6 build %7 cast %8 (lock %9 gpu %10 post %11, calls %12 recast %13)"
		" scatter %14; class lock %15 gpu %16 post %17" )
		.arg( qRound( S.msSmall ) ).arg( small.size() ).arg( qRound( double( nsSmallMax ) / 1e6 ) ).arg( big.size() )
		.arg( qRound( double( nsEFl ) / 1e6 ) ).arg( qRound( double( nsESeg ) / 1e6 ) ).arg( qRound( double( nsEBuild ) / 1e6 ) )
		.arg( qRound( double( nsECast ) / 1e6 ) ).arg( qRound( double( nsELock ) / 1e6 ) ).arg( qRound( double( nsEGpu ) / 1e6 ) )
		.arg( qRound( double( nsEPost ) / 1e6 ) ).arg( nECalls.load() ).arg( nERecast.load() ).arg( qRound( double( nsEScat ) / 1e6 ) )
		.arg( qRound( double( nsCLock ) / 1e6 ) ).arg( qRound( double( nsCGpu ) / 1e6 ) ).arg( qRound( double( nsCPost ) / 1e6 ) );
	if ( stats )
		*stats = S;
	return out;
}
