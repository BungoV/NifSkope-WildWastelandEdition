/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

// lane ROOMCLAMP1: room labels per voxel (proberooms.h)

#include "proberooms.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <thread>

namespace {

void roomsParallel( size_t n, const std::function<void( size_t )> & fn )
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

bool planeBox( const float n[3], const float v[3], const float h[3] )
{
	float vmin[3], vmax[3];
	for ( int q = 0; q < 3; q++ ) {
		if ( n[q] > 0.0f ) {
			vmin[q] = -h[q] - v[q];
			vmax[q] = h[q] - v[q];
		} else {
			vmin[q] = h[q] - v[q];
			vmax[q] = -h[q] - v[q];
		}
	}
	if ( n[0] * vmin[0] + n[1] * vmin[1] + n[2] * vmin[2] > 0.0f )
		return false;
	return n[0] * vmax[0] + n[1] * vmax[1] + n[2] * vmax[2] >= 0.0f;
}

//! Akenine-Moller separating-axis test, box at `c` with half size `h` (probeplace.cpp's twin)
bool triBoxRooms( const float c[3], const float h[3], const float * tri )
{
	float v[3][3];
	for ( int i = 0; i < 3; i++ )
		for ( int k = 0; k < 3; k++ )
			v[i][k] = tri[i * 3 + k] - c[k];
	for ( int q = 0; q < 3; q++ ) {
		const float mn = std::min( { v[0][q], v[1][q], v[2][q] } );
		const float mx = std::max( { v[0][q], v[1][q], v[2][q] } );
		if ( mn > h[q] || mx < -h[q] )
			return false;
	}
	float e[3][3];
	for ( int i = 0; i < 3; i++ )
		for ( int k = 0; k < 3; k++ )
			e[i][k] = v[( i + 1 ) % 3][k] - v[i][k];
	for ( int i = 0; i < 3; i++ ) {
		for ( int j = 0; j < 3; j++ ) {
			float a[3] = { 0, 0, 0 };
			const int j1 = ( j + 1 ) % 3, j2 = ( j + 2 ) % 3;
			a[j1] = -e[i][j2];
			a[j2] = e[i][j1];
			const float p0 = a[0] * v[0][0] + a[1] * v[0][1] + a[2] * v[0][2];
			const float p1 = a[0] * v[1][0] + a[1] * v[1][1] + a[2] * v[1][2];
			const float p2 = a[0] * v[2][0] + a[1] * v[2][1] + a[2] * v[2][2];
			const float r = h[0] * std::fabs( a[0] ) + h[1] * std::fabs( a[1] ) + h[2] * std::fabs( a[2] );
			if ( std::min( { p0, p1, p2 } ) > r || std::max( { p0, p1, p2 } ) < -r )
				return false;
		}
	}
	const float n[3] = { e[0][1] * e[1][2] - e[0][2] * e[1][1], e[0][2] * e[1][0] - e[0][0] * e[1][2],
		e[0][0] * e[1][1] - e[0][1] * e[1][0] };
	return planeBox( n, v[0], h );
}

constexpr float kInf = 1e30f;

//! the squared distance transform of one line (Felzenszwalb-Huttenlocher), sites = the finite entries of f
void dtLine( const float * f, int n, float * d, std::vector<int> & v, std::vector<float> & z )
{
	v.resize( size_t( n ) );
	z.resize( size_t( n ) + 1 );
	int k = -1;
	for ( int q = 0; q < n; q++ ) {
		if ( f[q] >= kInf )
			continue;
		if ( k < 0 ) {
			k = 0;
			v[0] = q;
			z[0] = -kInf;
			z[1] = kInf;
			continue;
		}
		float s;
		for ( ;; ) {
			const int p = v[size_t( k )];
			s = ( ( f[q] + float( q ) * q ) - ( f[p] + float( p ) * p ) ) / ( 2.0f * float( q - p ) );
			if ( s <= z[size_t( k )] && k > 0 ) {
				k--;
				continue;
			}
			break;
		}
		if ( s <= z[size_t( k )] ) {   // k == 0 and the new site wins everywhere
			v[0] = q;
			continue;
		}
		k++;
		v[size_t( k )] = q;
		z[size_t( k )] = s;
		z[size_t( k ) + 1] = kInf;
	}
	if ( k < 0 ) {
		for ( int q = 0; q < n; q++ )
			d[q] = kInf;
		return;
	}
	int j = 0;
	for ( int q = 0; q < n; q++ ) {
		while ( z[size_t( j ) + 1] < float( q ) )
			j++;
		const int p = v[size_t( j )];
		d[q] = float( q - p ) * float( q - p ) + f[p];
	}
}

}	// namespace

bool ProbeRooms::at( const double p[3], int out[2] ) const
{
	out[0] = out[1] = -1;
	int g[3];
	for ( int c = 0; c < 3; c++ ) {
		g[c] = int( std::floor( ( p[c] - origin[c] ) / cell ) );
		if ( g[c] < 0 || g[c] >= dims[c] )
			return false;
	}
	const size_t i = index( g[0], g[1], g[2] );
	out[0] = a[i];
	out[1] = b[i];
	return true;
}

bool ProbeRooms::surface( const double p[3], const double n[3], int out[2] ) const
{
	for ( double k : { 0.75, 1.75 } ) {
		const double q[3] = { p[0] + n[0] * k * cell, p[1] + n[1] * k * cell, p[2] + n[2] * k * cell };
		if ( at( q, out ) && out[0] >= 0 )
			return true;
	}
	// lane ROOMCLAMP1: both reads in a wall's cells (a floor beside a thin wall, the wall in the same cell column): of
	// the 8 cells round either read in the plane of the two axes along the surface (an inner corner's room is the
	// diagonal one), the air cell nearest its read (the sum of the faces crossed), the 0.75 read first on a tie (a
	// floor beside an outer wall: the 0.75 read's one air neighbour is the outdoors across the wall)
	int m = 0;
	for ( int c = 1; c < 3; c++ )
		if ( std::fabs( n[c] ) > std::fabs( n[m] ) )
			m = c;
	const int ta = m == 0 ? 1 : 0, tb = m == 2 ? 1 : 2;
	out[0] = out[1] = -1;
	double best = 3.0;
	for ( double k : { 0.75, 1.75 } ) {
		const double q[3] = { p[0] + n[0] * k * cell, p[1] + n[1] * k * cell, p[2] + n[2] * k * cell };
		int g[3];
		double f[3];
		bool in = true;
		for ( int c = 0; c < 3; c++ ) {
			const double u = ( q[c] - origin[c] ) / cell;
			g[c] = int( std::floor( u ) );
			f[c] = u - g[c];
			in = in && g[c] >= 0 && g[c] < dims[c];
		}
		if ( !in )
			continue;
		for ( int sa = -1; sa <= 1; sa++ ) {
			for ( int sb = -1; sb <= 1; sb++ ) {
				if ( sa == 0 && sb == 0 )
					continue;
				int h[3] = { g[0], g[1], g[2] };
				h[ta] += sa;
				h[tb] += sb;
				if ( h[ta] < 0 || h[ta] >= dims[ta] || h[tb] < 0 || h[tb] >= dims[tb] )
					continue;
				const size_t i = index( h[0], h[1], h[2] );
				const double d = ( sa > 0 ? 1.0 - f[ta] : sa < 0 ? f[ta] : 0.0 ) + ( sb > 0 ? 1.0 - f[tb] : sb < 0 ? f[tb] : 0.0 );
				if ( a[i] >= 0 && d < best ) {
					best = d;
					out[0] = a[i];
					out[1] = b[i];
				}
			}
		}
	}
	return out[0] >= 0;
}

bool probeRoomsBuild( const ProbeSoup & soup, const ProbeRoomSpec & spec, ProbeRooms * out )
{
	ProbeRooms & R = *out;
	R = ProbeRooms();
	QElapsedTimer clock;
	clock.start();
	const bool red26 = spec.red == QLatin1String( "conn26" );
	const bool redBoxes = spec.red == QLatin1String( "boxes" );
	const bool redGlass = spec.red == QLatin1String( "glasswall" );
	const bool redNoMask = spec.red == QLatin1String( "nomask" );   // lane ALPHATEST2: masks ignored (ALPHATEST1 rooms)
	const bool useMask = !redNoMask && !soup.amask.empty();
	auto masked = [&]( size_t k ) {
		return useMask && k < soup.amask.triOf.size() && soup.amask.triOf[k] >= 0;
	};
	double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 };
	for ( const std::vector<float> * t : { &soup.tris, &soup.glass } )
		for ( size_t i = 0; i + 2 < t->size(); i += 3 )
			for ( int c = 0; c < 3; c++ ) {
				lo[c] = std::min( lo[c], double( ( *t )[i + size_t( c )] ) );
				hi[c] = std::max( hi[c], double( ( *t )[i + size_t( c )] ) );
			}
	if ( lo[0] > hi[0] ) {
		R.error = QStringLiteral( "rooms: the soup is empty" );
		return false;
	}
	// the cell, raised until the padded grid fits
	float cell = std::max( 1.0f, spec.cell );
	int pad = 0;
	for ( ;; ) {
		pad = int( std::ceil( spec.pinch / cell ) ) + 2;
		double n = 1.0;
		for ( int c = 0; c < 3; c++ )
			n *= std::ceil( ( hi[c] - lo[c] ) / cell ) + 1 + 2 * pad;
		if ( n <= double( std::max( 1000, spec.maxCells ) ) )
			break;
		cell *= 1.25f;
	}
	R.cell = cell;
	for ( int c = 0; c < 3; c++ ) {
		R.origin[c] = float( lo[c] - double( pad ) * cell );
		R.dims[c] = int( std::ceil( ( hi[c] - lo[c] ) / cell ) ) + 1 + 2 * pad;
	}
	const int X = R.dims[0], Y = R.dims[1], Z = R.dims[2];
	const size_t N = size_t( X ) * size_t( Y ) * size_t( Z );

	// 1. solid and glass: every cell a triangle touches (one z slab per task, no two tasks write one cell)
	std::vector<quint8> st( N, 0 );   // 0 air, 1 solid, 2 glass
	const float h[3] = { cell * 0.5f, cell * 0.5f, cell * 0.5f };
	auto mark = [&]( const std::vector<float> & t, quint8 val ) {
		const size_t nt = t.size() / 9;
		std::vector<std::vector<int>> bySlab( static_cast<size_t>( Z ) );
		for ( size_t k = 0; k < nt; k++ ) {
			const float * p = &t[k * 9];
			const float zl = std::min( { p[2], p[5], p[8] } ), zh = std::max( { p[2], p[5], p[8] } );
			const int z0 = std::max( 0, int( std::floor( ( zl - R.origin[2] ) / cell - 0.001f ) ) );
			const int z1 = std::min( Z - 1, int( std::floor( ( zh - R.origin[2] ) / cell + 0.001f ) ) );
			for ( int z = z0; z <= z1; z++ )
				bySlab[size_t( z )].push_back( int( k ) );
		}
		roomsParallel( size_t( Z ), [&]( size_t zs ) {
			const int z = int( zs );
			for ( int k : bySlab[zs] ) {
				if ( val == 1 && masked( size_t( k ) ) )
					continue;   // lane ALPHATEST2: below
				const float * p = &t[size_t( k ) * 9];
				int g0[2], g1[2];
				for ( int c = 0; c < 2; c++ ) {
					const float l = std::min( { p[c], p[c + 3], p[c + 6] } ), u = std::max( { p[c], p[c + 3], p[c + 6] } );
					g0[c] = std::max( 0, int( std::floor( ( l - R.origin[c] ) / cell - 0.001f ) ) );
					g1[c] = std::min( R.dims[c] - 1, int( std::floor( ( u - R.origin[c] ) / cell + 0.001f ) ) );
				}
				for ( int y = g0[1]; y <= g1[1]; y++ )
					for ( int x = g0[0]; x <= g1[0]; x++ ) {
						quint8 & s = st[R.index( x, y, z )];
						if ( s == 1 || s == val )
							continue;
						const float c[3] = { R.origin[0] + ( x + 0.5f ) * cell, R.origin[1] + ( y + 0.5f ) * cell,
							R.origin[2] + ( z + 0.5f ) * cell };
						if ( triBoxRooms( c, h, p ) )
							s = val;
					}
			}
		} );
	};
	mark( soup.glass, 2 );
	mark( soup.tris, 1 );   // a cell both touch is solid
	/* lane ALPHATEST2: the masked triangles. Per cell, the samples that land in it and the holes among them; a
	 * cell with samples is solid when holes x 2 < samples, one without takes the triangle's overall share */
	if ( useMask ) {
		std::vector<quint32> nSam( N, 0 ), nHole( N, 0 );
		std::vector<quint8> touch( N, 0 );   // 1 touched, 2 touched by a triangle mostly solid overall
		const size_t nt = soup.tris.size() / 9;
		const double step = double( cell ) / 8.0;
		std::vector<std::vector<int>> bySlab( static_cast<size_t>( Z ) );
		std::vector<quint8> triSolid( nt, 0 );
		for ( size_t k = 0; k < nt; k++ ) {
			if ( !masked( k ) )
				continue;
			const float * p = &soup.tris[k * 9];
			const float zl = std::min( { p[2], p[5], p[8] } ), zh = std::max( { p[2], p[5], p[8] } );
			const int z0 = std::max( 0, int( std::floor( ( zl - R.origin[2] ) / cell - 0.001f ) ) );
			const int z1 = std::min( Z - 1, int( std::floor( ( zh - R.origin[2] ) / cell + 0.001f ) ) );
			for ( int z = z0; z <= z1; z++ )
				bySlab[size_t( z )].push_back( int( k ) );
			// the triangle's overall share: a 16 x 16 subdivision's centroids
			int nh = 0, ns = 0;
			for ( int i = 0; i < 16; i++ )
				for ( int j = 0; i + j < 16; j++ ) {
					ns++;
					nh += soup.amask.hole( int( k ), ( i + 1.0 / 3 ) / 16, ( j + 1.0 / 3 ) / 16 ) ? 1 : 0;
					if ( i + j < 15 ) {
						ns++;
						nh += soup.amask.hole( int( k ), ( i + 2.0 / 3 ) / 16, ( j + 2.0 / 3 ) / 16 ) ? 1 : 0;
					}
				}
			triSolid[k] = nh * 2 < ns ? 1 : 0;
		}
		roomsParallel( size_t( Z ), [&]( size_t zs ) {
			const int z = int( zs );
			for ( int k : bySlab[zs] ) {
				const float * p = &soup.tris[size_t( k ) * 9];
				// the touched cells (the SAT, as any triangle)
				int g0[2], g1[2];
				for ( int c = 0; c < 2; c++ ) {
					const float l = std::min( { p[c], p[c + 3], p[c + 6] } ), u = std::max( { p[c], p[c + 3], p[c + 6] } );
					g0[c] = std::max( 0, int( std::floor( ( l - R.origin[c] ) / cell - 0.001f ) ) );
					g1[c] = std::min( R.dims[c] - 1, int( std::floor( ( u - R.origin[c] ) / cell + 0.001f ) ) );
				}
				for ( int y = g0[1]; y <= g1[1]; y++ )
					for ( int x = g0[0]; x <= g1[0]; x++ ) {
						const float c[3] = { R.origin[0] + ( x + 0.5f ) * cell, R.origin[1] + ( y + 0.5f ) * cell,
							R.origin[2] + ( z + 0.5f ) * cell };
						if ( triBoxRooms( c, h, p ) ) {
							quint8 & tc = touch[R.index( x, y, z )];
							tc = std::max( tc, quint8( triSolid[size_t( k )] ? 2 : 1 ) );
						}
					}
				// the samples: M x M subtriangles, one jittered point in each (a hash of triangle and subtriangle)
				double e = 0;
				for ( int a = 0; a < 3; a++ ) {
					const int b = ( a + 1 ) % 3;
					e = std::max( e, std::sqrt( std::pow( double( p[a * 3] ) - p[b * 3], 2 ) + std::pow( double( p[a * 3 + 1] ) - p[b * 3 + 1], 2 )
						+ std::pow( double( p[a * 3 + 2] ) - p[b * 3 + 2], 2 ) ) );
				}
				const int M = std::clamp( int( std::ceil( e / step ) ), 1, 1024 );
				for ( int i = 0; i < M; i++ )
					for ( int j = 0; i + j < M; j++ )
						for ( int up = 0; up < ( i + j < M - 1 ? 2 : 1 ); up++ ) {
							quint32 hsh = quint32( k ) * 2654435761u ^ quint32( i * 40503 + j * 9973 + up * 7 );
							hsh ^= hsh >> 15; hsh *= 2246822519u; hsh ^= hsh >> 13; hsh *= 3266489917u; hsh ^= hsh >> 16;
							double ju = ( hsh & 0xFFFF ) / 65536.0, jv = ( hsh >> 16 ) / 65536.0;
							if ( ju + jv > 1.0 ) {
								ju = 1.0 - ju;
								jv = 1.0 - jv;
							}
							const double b1 = up ? ( i + 1 - ju ) / M : ( i + ju ) / M;
							const double b2 = up ? ( j + 1 - jv ) / M : ( j + jv ) / M;
							const double b0 = 1.0 - b1 - b2;
							double q[3];
							for ( int c = 0; c < 3; c++ )
								q[c] = b0 * p[c] + b1 * p[3 + c] + b2 * p[6 + c];
							const int cz = int( std::floor( ( q[2] - R.origin[2] ) / cell ) );
							if ( cz != z )
								continue;
							const int cx = std::clamp( int( std::floor( ( q[0] - R.origin[0] ) / cell ) ), 0, X - 1 );
							const int cy = std::clamp( int( std::floor( ( q[1] - R.origin[1] ) / cell ) ), 0, Y - 1 );
							const size_t ci = R.index( cx, cy, z );
							nSam[ci]++;
							nHole[ci] += soup.amask.hole( k, b1, b2 ) ? 1u : 0u;
						}
			}
		} );
		for ( size_t i = 0; i < N; i++ ) {
			if ( !touch[i] || st[i] == 1 )
				continue;
			const bool solid = nSam[i] ? nHole[i] * 2 < nSam[i] : touch[i] == 2;
			if ( solid )
				st[i] = 1;
			R.cellsMaskSolid += solid ? 1 : 0;
			R.cellsMaskOpen += solid ? 0 : 1;
		}
	}
	// the door boxes (an opening's frame): no core grows through one
	std::vector<quint8> door( N, 0 );
	for ( const ProbeSoup::Door & d : soup.doors ) {
		int g0[3], g1[3];
		for ( int c = 0; c < 3; c++ ) {
			g0[c] = std::max( 0, int( std::ceil( ( d.lo[c] - R.origin[c] ) / cell - 0.5f ) ) );
			g1[c] = std::min( R.dims[c] - 1, int( std::floor( ( d.hi[c] - R.origin[c] ) / cell - 0.5f ) ) );
		}
		for ( int z = g0[2]; z <= g1[2]; z++ )
			for ( int y = g0[1]; y <= g1[1]; y++ )
				for ( int x = g0[0]; x <= g1[0]; x++ )
					door[R.index( x, y, z )] = 1;
	}
	for ( size_t i = 0; i < N; i++ ) {
		R.cellsSolid += st[i] == 1;
		R.cellsGlass += st[i] == 2;
		R.cellsAir += st[i] == 0;
		R.cellsDoor += st[i] == 0 && door[i];
	}

	// 2. the air's squared distance (cells) to the nearest solid or glass cell; beyond the grid is air
	std::vector<float> d2( N );
	for ( size_t i = 0; i < N; i++ )
		d2[i] = st[i] ? 0.0f : kInf;
	for ( int axis = 0; axis < 3; axis++ ) {
		const int n = R.dims[axis];
		const int o1 = axis == 0 ? 1 : 0, o2 = axis == 2 ? 1 : 2;
		const size_t lines = size_t( R.dims[o1] ) * size_t( R.dims[o2] );
		roomsParallel( lines, [&]( size_t li ) {
			int g[3];
			g[o1] = int( li % size_t( R.dims[o1] ) );
			g[o2] = int( li / size_t( R.dims[o1] ) );
			std::vector<float> f( static_cast<size_t>( n ) ), dd( static_cast<size_t>( n ) ), zz;
			std::vector<int> vv;
			for ( int q = 0; q < n; q++ ) {
				g[axis] = q;
				f[size_t( q )] = d2[R.index( g[0], g[1], g[2] )];
			}
			dtLine( f.data(), n, dd.data(), vv, zz );
			for ( int q = 0; q < n; q++ ) {
				g[axis] = q;
				d2[R.index( g[0], g[1], g[2] )] = dd[size_t( q )];
			}
		} );
	}

	// neighbours: 6, or 26 (red)
	std::vector<std::array<int, 3>> nb;
	for ( int dz = -1; dz <= 1; dz++ )
		for ( int dy = -1; dy <= 1; dy++ )
			for ( int dx = -1; dx <= 1; dx++ ) {
				const int m = std::abs( dx ) + std::abs( dy ) + std::abs( dz );
				if ( m == 1 || ( red26 && m > 1 ) )
					nb.push_back( { dx, dy, dz } );
			}
	auto border = [&]( int x, int y, int z ) { return x == 0 || y == 0 || z == 0 || x == X - 1 || y == Y - 1 || z == Z - 1; };
	std::vector<qint32> lab( N, -1 );
	int next = 1;
	// flood the cells `in` admits from i with label l (l < 0: a new one, 0 when the flood meets the border)
	std::vector<size_t> stack, comp;
	auto flood = [&]( size_t i0, const std::function<bool( size_t )> & in ) -> int {
		comp.clear();
		stack.assign( 1, i0 );
		lab[i0] = -2;
		bool out = false;
		while ( !stack.empty() ) {
			const size_t i = stack.back();
			stack.pop_back();
			comp.push_back( i );
			const int x = int( i % size_t( X ) ), y = int( ( i / size_t( X ) ) % size_t( Y ) ), z = int( i / ( size_t( X ) * size_t( Y ) ) );
			out = out || border( x, y, z );
			for ( const auto & o : nb ) {
				const int xx = x + o[0], yy = y + o[1], zz = z + o[2];
				if ( xx < 0 || yy < 0 || zz < 0 || xx >= X || yy >= Y || zz >= Z )
					continue;
				const size_t j = R.index( xx, yy, zz );
				if ( lab[j] != -1 || !in( j ) )
					continue;
				lab[j] = -2;
				stack.push_back( j );
			}
		}
		const int l = out ? 0 : next++;
		for ( size_t i : comp )
			lab[i] = l;
		return l;
	};

	// 3. the cores (deeper than the pinch, outside every door box), flooded
	const float pin2 = ( spec.pinch / cell ) * ( spec.pinch / cell );
	auto core = [&]( size_t i ) { return st[i] == 0 && !door[i] && d2[i] > pin2; };
	for ( size_t i = 0; i < N; i++ ) {
		if ( lab[i] != -1 || !core( i ) )
			continue;
		const int l = flood( i, core );
		R.cores++;
		R.coresOutdoors += l == 0;
		R.cellsCore += qint64( comp.size() );
	}
	// 4. the rest of the air, deepest first: a cell takes the room of the first labelled neighbour that reaches it
	{
		float mx = 0.0f;
		for ( size_t i = 0; i < N; i++ )
			if ( st[i] == 0 && d2[i] < kInf )
				mx = std::max( mx, d2[i] );
		const int nk = int( std::min( mx, 65535.0f ) ) + 1;
		std::vector<std::vector<qint32>> bucket( static_cast<size_t>( nk ) );
		auto key = [&]( size_t j, int cur ) { return std::min( cur, int( std::min( d2[j], float( nk - 1 ) ) ) ); };
		auto claim = [&]( size_t i, int cur ) {
			const int x = int( i % size_t( X ) ), y = int( ( i / size_t( X ) ) % size_t( Y ) ), z = int( i / ( size_t( X ) * size_t( Y ) ) );
			for ( const auto & o : nb ) {
				const int xx = x + o[0], yy = y + o[1], zz = z + o[2];
				if ( xx < 0 || yy < 0 || zz < 0 || xx >= X || yy >= Y || zz >= Z )
					continue;
				const size_t j = R.index( xx, yy, zz );
				if ( lab[j] != -1 || st[j] != 0 )
					continue;
				lab[j] = lab[i];
				bucket[size_t( key( j, cur ) )].push_back( qint32( j ) );
			}
		};
		for ( size_t i = 0; i < N; i++ )
			if ( lab[i] >= 0 )
				claim( i, nk - 1 );
		for ( int k = nk - 1; k >= 0; k-- ) {
			std::vector<qint32> & bk = bucket[size_t( k )];
			for ( size_t r = 0; r < bk.size(); r++ )
				claim( size_t( bk[r] ), k );
			std::vector<qint32>().swap( bk );
		}
	}
	// air no core reached: pockets, each a room of its own (outdoors at the border)
	for ( size_t i = 0; i < N; i++ ) {
		if ( lab[i] != -1 || st[i] != 0 )
			continue;
		flood( i, [&]( size_t j ) { return st[j] == 0; } );
		R.pockets++;
		R.cellsLeft += qint64( comp.size() );
	}
	R.rooms = next - 1;
	// red "boxes": each room is its bounding box (a box over the outdoors or another room takes it)
	if ( redBoxes ) {
		std::vector<std::array<int, 6>> bx( size_t( next ), { { X, Y, Z, -1, -1, -1 } } );
		for ( int z = 0; z < Z; z++ )
			for ( int y = 0; y < Y; y++ )
				for ( int x = 0; x < X; x++ ) {
					const qint32 l = lab[R.index( x, y, z )];
					if ( l <= 0 )
						continue;
					auto & b = bx[size_t( l )];
					b = { std::min( b[0], x ), std::min( b[1], y ), std::min( b[2], z ), std::max( b[3], x ), std::max( b[4], y ),
						std::max( b[5], z ) };
				}
		std::vector<qint32> bl( N, -1 );
		for ( int l = 1; l < next; l++ ) {
			const auto & b = bx[size_t( l )];
			for ( int z = b[2]; z <= b[5]; z++ )
				for ( int y = b[1]; y <= b[4]; y++ )
					for ( int x = b[0]; x <= b[3]; x++ ) {
						qint32 & t = bl[R.index( x, y, z )];
						if ( t < 0 )
							t = l;
					}
		}
		for ( size_t i = 0; i < N; i++ )
			if ( st[i] == 0 && bl[i] > 0 )
				lab[i] = bl[i];
	}
	// labels as int16, folded past 4094 (the shader's packing)
	auto fold = [&]( qint32 l ) -> qint16 {
		if ( l > 4094 ) {
			R.folded++;
			return qint16( 1 + ( l - 1 ) % 4094 );
		}
		return qint16( l );
	};
	R.a.assign( N, -1 );
	R.b.assign( N, -1 );
	for ( size_t i = 0; i < N; i++ )
		if ( st[i] == 0 && lab[i] >= 0 )
			R.a[i] = fold( lab[i] );
	// 5. openings: an air cell beside air of another room names both
	for ( int z = 0; z < Z; z++ )
		for ( int y = 0; y < Y; y++ )
			for ( int x = 0; x < X; x++ ) {
				const size_t i = R.index( x, y, z );
				if ( R.a[i] < 0 || st[i] != 0 )
					continue;
				for ( int k = 0; k < 6; k++ ) {
					const int xx = x + ( k == 0 ) - ( k == 1 ), yy = y + ( k == 2 ) - ( k == 3 ), zz = z + ( k == 4 ) - ( k == 5 );
					if ( xx < 0 || yy < 0 || zz < 0 || xx >= X || yy >= Y || zz >= Z )
						continue;
					const size_t j = R.index( xx, yy, zz );
					if ( st[j] == 0 && R.a[j] >= 0 && R.a[j] != R.a[i] ) {
						R.b[i] = R.a[j];
						R.cellsOpening++;
						break;
					}
				}
			}
	// glass: the rooms on its two sides (the first air along each axis, through glass, three cells at most)
	if ( !redGlass )
		for ( int z = 0; z < Z; z++ )
			for ( int y = 0; y < Y; y++ )
				for ( int x = 0; x < X; x++ ) {
					const size_t i = R.index( x, y, z );
					if ( st[i] != 2 )
						continue;
					int seen[6], ns = 0;
					for ( int k = 0; k < 6; k++ ) {
						int xx = x, yy = y, zz = z;
						for ( int s = 0; s < 3; s++ ) {
							xx += ( k == 0 ) - ( k == 1 );
							yy += ( k == 2 ) - ( k == 3 );
							zz += ( k == 4 ) - ( k == 5 );
							if ( xx < 0 || yy < 0 || zz < 0 || xx >= X || yy >= Y || zz >= Z )
								break;
							const size_t j = R.index( xx, yy, zz );
							if ( st[j] == 1 )
								break;
							if ( st[j] == 0 ) {
								if ( R.a[j] >= 0 )
									seen[ns++] = R.a[j];
								break;
							}
						}
					}
					if ( !ns )
						continue;
					// the most named first, then a different one
					int best = seen[0], bestN = 0;
					for ( int p = 0; p < ns; p++ ) {
						const int c = int( std::count( seen, seen + ns, seen[p] ) );
						if ( c > bestN ) {
							best = seen[p];
							bestN = c;
						}
					}
					R.a[i] = qint16( best );
					for ( int p = 0; p < ns; p++ )
						if ( seen[p] != best ) {
							R.b[i] = qint16( seen[p] );
							R.glassBoth++;
							break;
						}
				}
	R.ms = clock.nsecsElapsed() / 1e6;
	R.ok = true;
	return true;
}

QString probeRoomsCensusText( const ProbeRooms & r )
{
	if ( !r.ok )
		return QStringLiteral( "rooms: none (%1)" ).arg( r.error );
	return QStringLiteral( "rooms: grid %1x%2x%3 cell %4; air %5, solid %6, glass %7, in door boxes %8; cores %9 "
		"(%10 outdoors, %11 cells), rooms %12, pockets %13 (%14 cells), openings %15, glass naming both sides %16%17; ms %18" )
		.arg( r.dims[0] ).arg( r.dims[1] ).arg( r.dims[2] ).arg( double( r.cell ), 0, 'f', 1 )
		.arg( r.cellsAir ).arg( r.cellsSolid ).arg( r.cellsGlass ).arg( r.cellsDoor )
		.arg( r.cores ).arg( r.coresOutdoors ).arg( r.cellsCore ).arg( r.rooms ).arg( r.pockets ).arg( r.cellsLeft )
		.arg( r.cellsOpening ).arg( r.glassBoth )
		.arg( ( r.folded ? QStringLiteral( ", %1 cells folded past 4094 rooms" ).arg( r.folded ) : QString() )
			+ ( r.cellsMaskOpen + r.cellsMaskSolid   // lane ALPHATEST2 (no masked triangle: the line as before)
				? QStringLiteral( "; alpha-tested cells open %1, solid %2" ).arg( r.cellsMaskOpen ).arg( r.cellsMaskSolid )
				: QString() ) )
		.arg( qRound( r.ms ) );
}

bool probeRoomsDump( const ProbeRooms & r, const QString & path, QString * err )
{
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) ) {
		*err = QStringLiteral( "cannot write %1" ).arg( path );
		return false;
	}
	f.write( reinterpret_cast<const char *>( r.origin ), 12 );
	f.write( reinterpret_cast<const char *>( &r.cell ), 4 );
	f.write( reinterpret_cast<const char *>( r.dims ), 12 );
	const qint32 n = r.rooms;
	f.write( reinterpret_cast<const char *>( &n ), 4 );
	std::vector<qint16> ab( r.a.size() * 2 );
	for ( size_t i = 0; i < r.a.size(); i++ ) {
		ab[i * 2] = r.a[i];
		ab[i * 2 + 1] = r.b[i];
	}
	f.write( reinterpret_cast<const char *>( ab.data() ), qint64( ab.size() * 2 ) );
	return true;
}
