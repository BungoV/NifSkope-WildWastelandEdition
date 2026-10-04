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

#include "aovolume.h"
#include "probebvh.h"	// the probe soup's tracer (fp-contract off for the whole file)

#include <QCryptographicHash>
#include <QElapsedTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <thread>

namespace aovol {

namespace {

const double kPi = 3.14159265358979323846;

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

template <typename T> void put( QByteArray & b, T v )
{
	b.append( reinterpret_cast<const char *>( &v ), int( sizeof( T ) ) );
}
template <typename T> T get( const QByteArray & b, int off )
{
	T v;
	std::memcpy( &v, b.constData() + off, sizeof( T ) );
	return v;
}

double fadeOf( const Volume & v, const double m[3] )
{
	double inner = 1e300;
	for ( int a = 0; a < 3; a++ )
		inner = std::min( inner, std::min( m[a] - double( v.lo[a] ), double( v.hi[a] ) - m[a] ) );
	return std::clamp( inner / double( v.fade ), 0.0, 1.0 );
}

//! the trilinear coefficients at m (g clamped to [0, dims - 1], i0 <= dims - 2)
void trilinear( const Volume & v, const double m[3], double K[9] )
{
	int i0[3];
	double f[3];
	for ( int a = 0; a < 3; a++ ) {
		const double cell = ( double( v.hi[a] ) - double( v.lo[a] ) ) / double( v.dims[a] );
		double g = ( m[a] - double( v.lo[a] ) ) / cell - 0.5;
		g = std::clamp( g, 0.0, double( v.dims[a] - 1 ) );
		i0[a] = std::min( int( std::floor( g ) ), v.dims[a] - 2 );
		f[a] = g - double( i0[a] );
	}
	for ( int j = 0; j < 9; j++ )
		K[j] = 0.0;
	for ( int dz = 0; dz < 2; dz++ )
		for ( int dy = 0; dy < 2; dy++ )
			for ( int dx = 0; dx < 2; dx++ ) {
				const double w = ( dx ? f[0] : 1.0 - f[0] ) * ( dy ? f[1] : 1.0 - f[1] ) * ( dz ? f[2] : 1.0 - f[2] );
				const size_t i = ( size_t( i0[2] + dz ) * size_t( v.dims[1] ) + size_t( i0[1] + dy ) ) * size_t( v.dims[0] )
					+ size_t( i0[0] + dx );
				const float * k = &v.k[i * 9];
				for ( int j = 0; j < 9; j++ )
					K[j] += w * double( k[j] );
			}
}

double farSh( const Volume & v, const double u[3] )
{
	double y[9], s = 0.0;
	sh9( u, y );
	for ( int j = 0; j < 9; j++ )
		s += y[j] * double( v.coef[j] );
	return s;
}

}	// namespace

void sh9( const double d[3], double y[9] )
{
	const double x = d[0], yy = d[1], z = d[2];
	y[0] = 0.282095;
	y[1] = 0.488603 * yy;
	y[2] = 0.488603 * z;
	y[3] = 0.488603 * x;
	y[4] = 1.092548 * x * yy;
	y[5] = 1.092548 * yy * z;
	y[6] = 0.315392 * ( 3.0 * z * z - 1.0 );
	y[7] = 1.092548 * x * z;
	y[8] = 0.546274 * ( x * x - yy * yy );
}

void fib( int i, int n, double d[3] )
{
	const double ii = double( i ) + 0.5;
	const double z = 1.0 - 2.0 * ii / double( n );
	const double r = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
	const double ph = ii * kPi * ( 3.0 - std::sqrt( 5.0 ) );
	d[0] = r * std::cos( ph );
	d[1] = r * std::sin( ph );
	d[2] = z;
}

void Volume::footprint( float flo[3], float fhi[3] ) const
{
	for ( int a = 0; a < 3; a++ ) {
		flo[a] = std::min( lo[a], c[a] - rcut );
		fhi[a] = std::max( hi[a], c[a] + rcut );
	}
}

bool bake( const std::vector<float> & tris, Volume * out, QString * why, int rays )
{
	QElapsedTimer clock;
	clock.start();
	Volume & V = *out;
	V = Volume();
	const size_t nt = tris.size() / 9;
	if ( !nt || rays <= 0 ) {
		if ( why )
			*why = QStringLiteral( "no triangles" );
		return false;
	}
	double blo[3] = { 1e300, 1e300, 1e300 }, bhi[3] = { -1e300, -1e300, -1e300 };
	for ( size_t i = 0; i < nt * 3; i++ )
		for ( int a = 0; a < 3; a++ ) {
			blo[a] = std::min( blo[a], double( tris[i * 3 + size_t( a )] ) );
			bhi[a] = std::max( bhi[a], double( tris[i * 3 + size_t( a )] ) );
		}
	double ext[3] = { bhi[0] - blo[0], bhi[1] - blo[1], bhi[2] - blo[2] };
	double med[3] = { ext[0], ext[1], ext[2] };
	std::sort( med, med + 3 );
	const double margin = kMarginK * med[1];
	double lo[3], hi[3], E[3];
	for ( int a = 0; a < 3; a++ ) {
		lo[a] = blo[a] - margin;
		hi[a] = bhi[a] + margin;
		E[a] = hi[a] - lo[a];
	}
	if ( !( E[0] > 0.0 && E[1] > 0.0 && E[2] > 0.0 ) ) {
		if ( why )
			*why = QStringLiteral( "a flat model" );
		return false;
	}
	const double s = std::cbrt( E[0] * E[1] * E[2] / double( kBudget ) );
	for ( int a = 0; a < 3; a++ ) {
		V.dims[a] = std::max( 8, int( std::nearbyint( E[a] / s ) ) );
		V.lo[a] = float( lo[a] );
		V.hi[a] = float( hi[a] );
		V.c[a] = float( 0.5 * ( blo[a] + bhi[a] ) );
	}
	V.fade = float( margin * kFadeK );
	V.rays = std::uint32_t( rays );
	V.tris = int( nt );
	const int dx = V.dims[0], dy = V.dims[1], dz = V.dims[2];
	const size_t nv = V.voxels();
	double cell[3];
	for ( int a = 0; a < 3; a++ )
		cell[a] = E[a] / double( V.dims[a] );

	probebvh::Bvh bvh;
	bvh.t = tris;
	bvh.build();
	std::vector<double> tn( nt * 3 );
	for ( size_t t = 0; t < nt; t++ ) {
		const float * p = &tris[t * 9];
		const double e1[3] = { double( p[3] ) - p[0], double( p[4] ) - p[1], double( p[5] ) - p[2] };
		const double e2[3] = { double( p[6] ) - p[0], double( p[7] ) - p[1], double( p[8] ) - p[2] };
		tn[t * 3] = e1[1] * e2[2] - e1[2] * e2[1];
		tn[t * 3 + 1] = e1[2] * e2[0] - e1[0] * e2[2];
		tn[t * 3 + 2] = e1[0] * e2[1] - e1[1] * e2[0];
	}
	std::vector<double> D( size_t( rays ) * 3 ), Y( size_t( rays ) * 9 );
	for ( int r = 0; r < rays; r++ ) {
		fib( r, rays, &D[size_t( r ) * 3] );
		sh9( &D[size_t( r ) * 3], &Y[size_t( r ) * 9] );
	}
	const double scale = ( 4.0 * kPi / double( rays ) ) / ( 2.0 * std::sqrt( kPi ) );
	std::vector<double> k( nv * 9, 0.0 );
	std::vector<char> valid( nv, 1 );
	parallelFor( nv, [&]( size_t i ) {
		const int x = int( i % size_t( dx ) ), y = int( ( i / size_t( dx ) ) % size_t( dy ) ), z = int( i / ( size_t( dx ) * size_t( dy ) ) );
		const double C[3] = { lo[0] + ( x + 0.5 ) * cell[0], lo[1] + ( y + 0.5 ) * cell[1], lo[2] + ( z + 0.5 ) * cell[2] };
		double acc[9] = {};
		int hits = 0, back = 0;
		for ( int r = 0; r < rays; r++ ) {
			const double * d = &D[size_t( r ) * 3];
			double th;
			int tri = -1;
			if ( !bvh.ray( C, d, 1e30, &th, &tri ) )
				continue;
			hits++;
			const double * n = &tn[size_t( tri ) * 3];
			if ( n[0] * d[0] + n[1] * d[1] + n[2] * d[2] > 0.0 )
				back++;
			const double * yv = &Y[size_t( r ) * 9];
			for ( int j = 0; j < 9; j++ )
				acc[j] += yv[j];
		}
		for ( int j = 0; j < 9; j++ )
			k[i * 9 + size_t( j )] = acc[j] * scale;
		valid[i] = !( hits > 0 && 2 * back > hits );
	} );
	for ( char c : valid )
		V.insideVoxels += c ? 0 : 1;
	if ( size_t( V.insideVoxels ) >= nv ) {
		if ( why )
			*why = QStringLiteral( "every voxel inside (a model wound inside out?)" );
		return false;
	}
	// voxels inside the solid have no outside: filled from their valid neighbours, ring by ring (the twin's roll)
	for ( ;; ) {
		std::vector<size_t> grow;
		std::vector<double> val;
		for ( size_t i = 0; i < nv; i++ ) {
			if ( valid[i] )
				continue;
			const int x = int( i % size_t( dx ) ), y = int( ( i / size_t( dx ) ) % size_t( dy ) ), z = int( i / ( size_t( dx ) * size_t( dy ) ) );
			double s9[9] = {};
			int cnt = 0;
			const int nb[6][3] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
			for ( const auto & o : nb ) {
				const int xx = x + o[0], yy = y + o[1], zz = z + o[2];
				if ( xx < 0 || yy < 0 || zz < 0 || xx >= dx || yy >= dy || zz >= dz )
					continue;
				const size_t j = ( size_t( zz ) * size_t( dy ) + size_t( yy ) ) * size_t( dx ) + size_t( xx );
				if ( !valid[j] )
					continue;
				cnt++;
				for ( int c = 0; c < 9; c++ )
					s9[c] += k[j * 9 + size_t( c )];
			}
			if ( !cnt )
				continue;
			grow.push_back( i );
			for ( int c = 0; c < 9; c++ )
				val.push_back( s9[c] / cnt );
		}
		if ( grow.empty() )
			break;
		for ( size_t g = 0; g < grow.size(); g++ ) {
			for ( int c = 0; c < 9; c++ )
				k[grow[g] * 9 + size_t( c )] = val[g * 9 + size_t( c )];
			valid[grow[g]] = 1;
		}
	}

	// the far field: the outer band's voxels, an equivalent sphere about c, rho^2(u) fitted as L2 SH
	{
		double A[9][10] = {};
		for ( size_t i = 0; i < nv; i++ ) {
			const int x = int( i % size_t( dx ) ), y = int( ( i / size_t( dx ) ) % size_t( dy ) ), z = int( i / ( size_t( dx ) * size_t( dy ) ) );
			const double C[3] = { lo[0] + ( x + 0.5 ) * cell[0], lo[1] + ( y + 0.5 ) * cell[1], lo[2] + ( z + 0.5 ) * cell[2] };
			double inner = 1e300;
			for ( int a = 0; a < 3; a++ )
				inner = std::min( inner, std::min( C[a] - lo[a], hi[a] - C[a] ) );
			if ( std::clamp( inner / double( V.fade ), 0.0, 1.0 ) >= 1.0 )
				continue;
			const double v[3] = { C[0] - double( V.c[0] ), C[1] - double( V.c[1] ), C[2] - double( V.c[2] ) };
			const double r = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
			const double u[3] = { v[0] / r, v[1] / r, v[2] / r };
			const double k0 = std::clamp( k[i * 9], 0.0, 0.5 );
			const double rho2 = r * r * ( 1.0 - ( 1.0 - 2.0 * k0 ) * ( 1.0 - 2.0 * k0 ) );
			double yv[9];
			sh9( u, yv );
			for ( int a = 0; a < 9; a++ ) {
				for ( int b = 0; b < 9; b++ )
					A[a][b] += yv[a] * yv[b];
				A[a][9] += yv[a] * rho2;
			}
		}
		// the normal equations, Gaussian elimination with partial pivoting
		for ( int c = 0; c < 9; c++ ) {
			int p = c;
			for ( int r = c + 1; r < 9; r++ )
				if ( std::fabs( A[r][c] ) > std::fabs( A[p][c] ) )
					p = r;
			if ( p != c )
				for ( int j = 0; j < 10; j++ )
					std::swap( A[p][j], A[c][j] );
			if ( std::fabs( A[c][c] ) < 1e-300 )
				continue;
			for ( int r = 0; r < 9; r++ ) {
				if ( r == c )
					continue;
				const double f = A[r][c] / A[c][c];
				for ( int j = c; j < 10; j++ )
					A[r][j] -= f * A[c][j];
			}
		}
		for ( int c = 0; c < 9; c++ )
			V.coef[c] = std::fabs( A[c][c] ) < 1e-300 ? 0.0f : float( A[c][9] / A[c][c] );
		double mx = -1e300;
		for ( int i = 0; i < 2048; i++ ) {
			double d[3], yv[9], s2 = 0.0;
			fib( i, 2048, d );
			sh9( d, yv );
			for ( int j = 0; j < 9; j++ )
				s2 += yv[j] * double( V.coef[j] );
			mx = std::max( mx, s2 );
		}
		V.rcut = float( std::sqrt( std::max( mx, 1.0 ) ) / std::sqrt( kFarCut ) );
	}

	// quantise: u8 share, s8 for the rest at one per-file scale; k = what a reader of the file gets
	double cmax = 1e-6;
	for ( size_t i = 0; i < nv; i++ )
		for ( int j = 1; j < 9; j++ )
			cmax = std::max( cmax, std::fabs( k[i * 9 + size_t( j )] ) );
	V.cmax = float( cmax );
	V.payload.resize( nv * 9 );
	V.k.resize( nv * 9 );
	const double deq = double( V.cmax ) / 127.0;
	for ( size_t i = 0; i < nv; i++ ) {
		const double q0 = std::clamp( std::nearbyint( k[i * 9] * 255.0 ), 0.0, 255.0 );
		V.payload[i * 9] = std::uint8_t( q0 );
		V.k[i * 9] = float( q0 / 255.0 );
		for ( int j = 1; j < 9; j++ ) {
			const double q = std::clamp( std::nearbyint( k[i * 9 + size_t( j )] / cmax * 127.0 ), -127.0, 127.0 );
			V.payload[i * 9 + size_t( j )] = std::uint8_t( std::int8_t( q ) );
			V.k[i * 9 + size_t( j )] = float( q * deq );
		}
	}
	V.msBake = clock.nsecsElapsed() / 1e6;
	return true;
}

QByteArray aoBytes( const Volume & v )
{
	QByteArray b;
	put<std::uint32_t>( b, kAoMagic );
	put<std::uint16_t>( b, 1 );
	put<std::uint16_t>( b, 2 );
	for ( int a = 0; a < 3; a++ )
		put<std::uint16_t>( b, std::uint16_t( v.dims[a] ) );
	put<std::uint16_t>( b, 9 );
	for ( float f : v.lo )
		put<float>( b, f );
	for ( float f : v.hi )
		put<float>( b, f );
	put<float>( b, v.fade );
	put<float>( b, v.cmax );
	put<std::uint32_t>( b, v.rays );
	for ( float f : v.c )
		put<float>( b, f );
	for ( float f : v.coef )
		put<float>( b, f );
	put<float>( b, v.rcut );
	put<std::uint64_t>( b, v.fp );
	put<std::uint32_t>( b, crc32( reinterpret_cast<const char *>( v.payload.data() ), v.payload.size() ) );
	b.append( QByteArray( kAoHead - b.size(), '\0' ) );
	b.append( reinterpret_cast<const char *>( v.payload.data() ), int( v.payload.size() ) );
	return b;
}

bool readAo( const QByteArray & b, std::uint64_t expectFp, Volume * out, QString * why )
{
	auto no = [&]( const char * w ) {
		if ( why )
			*why = QString::fromLatin1( w );
		return false;
	};
	if ( b.size() < kAoHead )
		return no( "short" );
	if ( get<std::uint32_t>( b, 0 ) != kAoMagic || get<std::uint16_t>( b, 4 ) != 1 )
		return no( "magic" );
	Volume V;
	for ( int a = 0; a < 3; a++ )
		V.dims[a] = get<std::uint16_t>( b, 8 + 2 * a );
	const int nb = get<std::uint16_t>( b, 14 );
	if ( nb != 9 || get<std::uint16_t>( b, 6 ) != 2 || V.dims[0] < 2 || V.dims[1] < 2 || V.dims[2] < 2 )
		return no( "encoding" );
	for ( int a = 0; a < 3; a++ ) {
		V.lo[a] = get<float>( b, 16 + 4 * a );
		V.hi[a] = get<float>( b, 28 + 4 * a );
		V.c[a] = get<float>( b, 52 + 4 * a );
	}
	V.fade = get<float>( b, 40 );
	V.cmax = get<float>( b, 44 );
	V.rays = get<std::uint32_t>( b, 48 );
	for ( int j = 0; j < 9; j++ )
		V.coef[j] = get<float>( b, 64 + 4 * j );
	V.rcut = get<float>( b, 100 );
	V.fp = get<std::uint64_t>( b, 104 );
	const std::uint32_t crc = get<std::uint32_t>( b, 112 );
	const size_t nv = V.voxels();
	if ( size_t( b.size() - kAoHead ) != nv * 9 || crc32( b.constData() + kAoHead, nv * 9 ) != crc )
		return no( "payload" );
	if ( expectFp && V.fp != expectFp )
		return no( "stale: the model changed since the bake" );
	V.payload.assign( reinterpret_cast<const std::uint8_t *>( b.constData() + kAoHead ),
		reinterpret_cast<const std::uint8_t *>( b.constData() + kAoHead ) + nv * 9 );
	V.k.resize( nv * 9 );
	const double deq = double( V.cmax ) / 127.0;
	for ( size_t i = 0; i < nv; i++ ) {
		V.k[i * 9] = float( double( V.payload[i * 9] ) / 255.0 );
		for ( int j = 1; j < 9; j++ )
			V.k[i * 9 + size_t( j )] = float( double( std::int8_t( V.payload[i * 9 + size_t( j )] ) ) * deq );
	}
	*out = std::move( V );
	if ( why )
		*why = QStringLiteral( "ok" );
	return true;
}

QByteArray indexBytes( QVector<IndexRec> recs )
{
	std::sort( recs.begin(), recs.end(), []( const IndexRec & a, const IndexRec & b ) { return a.hash < b.hash; } );
	QByteArray strings, b;
	QVector<QPair<std::uint32_t, std::uint16_t>> at;
	for ( const IndexRec & r : recs ) {
		const QByteArray p = r.path.toUtf8();
		at.append( { std::uint32_t( strings.size() ), std::uint16_t( p.size() ) } );
		strings += p;
		strings += '\0';
	}
	put<std::uint32_t>( b, kAoiMagic );
	put<std::uint16_t>( b, 1 );
	put<std::uint16_t>( b, 0 );
	put<std::uint32_t>( b, std::uint32_t( recs.size() ) );
	put<std::uint32_t>( b, std::uint32_t( 32 + 64 * recs.size() ) );
	b.append( QByteArray( 16, '\0' ) );
	for ( int i = 0; i < recs.size(); i++ ) {
		const IndexRec & r = recs[i];
		put<std::uint64_t>( b, r.hash );
		put<std::uint64_t>( b, r.fp );
		put<std::uint32_t>( b, at[i].first );
		put<std::uint16_t>( b, at[i].second );
		put<std::uint16_t>( b, 0 );
		for ( float f : r.lo )
			put<float>( b, f );
		for ( float f : r.hi )
			put<float>( b, f );
		put<std::uint32_t>( b, r.size );
		put<std::uint32_t>( b, r.crc );
		b.append( QByteArray( 8, '\0' ) );
	}
	return b + strings;
}

bool readIndex( const QByteArray & b, QHash<QString, IndexRec> * out, QString * why )
{
	out->clear();
	if ( b.size() < 32 || get<std::uint32_t>( b, 0 ) != kAoiMagic || get<std::uint16_t>( b, 4 ) != 1 ) {
		if ( why )
			*why = QStringLiteral( "magic" );
		return false;
	}
	const std::uint32_t n = get<std::uint32_t>( b, 8 ), soff = get<std::uint32_t>( b, 12 );
	if ( soff != 32 + 64 * n || std::uint32_t( b.size() ) < soff ) {
		if ( why )
			*why = QStringLiteral( "size" );
		return false;
	}
	for ( std::uint32_t i = 0; i < n; i++ ) {
		const int o = int( 32 + 64 * i );
		IndexRec r;
		r.hash = get<std::uint64_t>( b, o );
		r.fp = get<std::uint64_t>( b, o + 8 );
		const std::uint32_t off = get<std::uint32_t>( b, o + 16 );
		const std::uint16_t len = get<std::uint16_t>( b, o + 20 );
		for ( int a = 0; a < 3; a++ ) {
			r.lo[a] = get<float>( b, o + 24 + 4 * a );
			r.hi[a] = get<float>( b, o + 36 + 4 * a );
		}
		r.size = get<std::uint32_t>( b, o + 48 );
		r.crc = get<std::uint32_t>( b, o + 52 );
		if ( std::uint64_t( soff ) + off + len > std::uint64_t( b.size() ) )
			continue;
		const QByteArray p = b.mid( int( soff + off ), len );
		if ( fnv1a64( p ) != r.hash )
			continue;
		r.path = QString::fromUtf8( p );
		out->insert( r.path, r );
	}
	return true;
}

std::uint64_t fnv1a64( const QByteArray & b )
{
	std::uint64_t h = 0xcbf29ce484222325ull;
	for ( char c : b )
		h = ( h ^ std::uint8_t( c ) ) * 0x100000001b3ull;
	return h;
}

QString normPath( const QString & p )
{
	QString s = p;
	s.replace( '/', '\\' );
	s = s.toLower();
	while ( s.startsWith( '\\' ) )
		s.remove( 0, 1 );
	return s;
}

std::uint64_t fingerprint( const QByteArray & modelBytes )
{
	const QByteArray h = QCryptographicHash::hash( modelBytes, QCryptographicHash::Sha256 );
	std::uint64_t v = 0;
	std::memcpy( &v, h.constData(), 8 );
	return v;
}

std::uint32_t crc32( const char * p, size_t n )
{
	static std::uint32_t table[256];
	static const bool init = [] {
		for ( std::uint32_t i = 0; i < 256; i++ ) {
			std::uint32_t c = i;
			for ( int k = 0; k < 8; k++ )
				c = ( c & 1 ) ? 0xEDB88320u ^ ( c >> 1 ) : c >> 1;
			table[i] = c;
		}
		return true;
	}();
	( void ) init;
	std::uint32_t c = 0xFFFFFFFFu;
	for ( size_t i = 0; i < n; i++ )
		c = table[( c ^ std::uint8_t( p[i] ) ) & 0xFF] ^ ( c >> 8 );
	return c ^ 0xFFFFFFFFu;
}

QString aoFileName( const QString & normalisedPath )
{
	return QStringLiteral( "%1.ao" ).arg( qulonglong( fnv1a64( normalisedPath.toUtf8() ) ), 16, 16, QChar( '0' ) );
}

double lookup( const Volume & v, const double m[3], const double n[3] )
{
	double K[9], Yn[9];
	trilinear( v, m, K );
	sh9( n, Yn );
	static const double band[9] = { 1.0, 2.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 0.25, 0.25, 0.25, 0.25, 0.25 };
	double occ = 0.0;
	for ( int j = 0; j < 9; j++ )
		occ += band[j] * K[j] * 2.0 * std::sqrt( kPi ) * Yn[j];
	const double f = fadeOf( v, m );
	double far = 0.0;
	const double w[3] = { m[0] - double( v.c[0] ), m[1] - double( v.c[1] ), m[2] - double( v.c[2] ) };
	const double r = std::max( std::sqrt( w[0] * w[0] + w[1] * w[1] + w[2] * w[2] ), 1e-6 );
	if ( r < double( v.rcut ) ) {
		const double u[3] = { w[0] / r, w[1] / r, w[2] / r };
		const double s2 = std::min( std::max( farSh( v, u ), 0.0 ) / ( r * r ), 1.0 );
		far = s2 * std::max( -( n[0] * u[0] + n[1] * u[1] + n[2] * u[2] ), 0.0 );
	}
	return std::clamp( 1.0 - ( occ * f + far * ( 1.0 - f ) ), 0.0, 1.0 );
}

void octantBasis( const double R[9], OctantBasis * out )
{
	OctantBasis & B = *out;
	const int n = 4096;
	B.dm.resize( size_t( n ) * 3 );
	B.oc.resize( size_t( n ) );
	std::memset( B.Q, 0, sizeof B.Q );
	std::memset( B.count, 0, sizeof B.count );
	for ( int i = 0; i < n; i++ ) {
		double d[3], m[3], y[9];
		fib( i, n, d );
		for ( int a = 0; a < 3; a++ )	// R^T d
			m[a] = R[0 * 3 + a] * d[0] + R[1 * 3 + a] * d[1] + R[2 * 3 + a] * d[2];
		const int o = ( d[0] < 0.0 ? 1 : 0 ) | ( d[1] < 0.0 ? 2 : 0 ) | ( d[2] < 0.0 ? 4 : 0 );
		B.oc[size_t( i )] = std::uint8_t( o );
		B.count[o]++;
		for ( int a = 0; a < 3; a++ )
			B.dm[size_t( i ) * 3 + size_t( a )] = float( m[a] );
		sh9( m, y );
		for ( int j = 0; j < 9; j++ )
			B.Q[o][j] += y[j];
	}
	for ( int o = 0; o < 8; o++ )
		for ( int j = 0; j < 9; j++ )
			B.Q[o][j] /= double( std::max( B.count[o], 1 ) );
}

void octantOcclusion( const Volume & v, const OctantBasis & B, const double m[3], double occ[8] )
{
	double K[9];
	trilinear( v, m, K );
	const double f = fadeOf( v, m );
	double far[8] = {};
	const double w0[3] = { double( v.c[0] ) - m[0], double( v.c[1] ) - m[1], double( v.c[2] ) - m[2] };
	const double r = std::max( std::sqrt( w0[0] * w0[0] + w0[1] * w0[1] + w0[2] * w0[2] ), 1e-6 );
	if ( f < 1.0 && r < double( v.rcut ) ) {
		const double w[3] = { w0[0] / r, w0[1] / r, w0[2] / r };
		const double mw[3] = { -w[0], -w[1], -w[2] };
		const double rho2 = std::max( farSh( v, mw ), 0.0 );
		const double cosa = std::sqrt( std::max( 0.0, 1.0 - std::min( rho2 / ( r * r ), 1.0 ) ) );
		int in[8] = {};
		const size_t n = B.oc.size();
		for ( size_t i = 0; i < n; i++ ) {
			const float * d = &B.dm[i * 3];
			if ( w[0] * double( d[0] ) + w[1] * double( d[1] ) + w[2] * double( d[2] ) >= cosa )
				in[B.oc[i]]++;
		}
		for ( int o = 0; o < 8; o++ )
			far[o] = double( in[o] ) / double( std::max( B.count[o], 1 ) );
	}
	for ( int o = 0; o < 8; o++ ) {
		double s = 0.0;
		for ( int j = 0; j < 9; j++ )
			s += K[j] * 2.0 * std::sqrt( kPi ) * B.Q[o][j];
		occ[o] = std::clamp( s * f + far[o] * ( 1.0 - f ), 0.0, 1.0 );
	}
}

}	// namespace aovol
