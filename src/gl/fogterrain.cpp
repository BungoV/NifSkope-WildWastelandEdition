#include "gl/fogterrain.h"

#ifndef WW_FOGTERRAIN_GATE
#include "wwswitch.h"
#endif

#include <algorithm>
#include <cmath>
#include <utility>

void wwFogTerrainLowpass( const std::vector<float> & H, int N, double T, double x0, double y0, float noH, double sigma,
	WwFogTerrGrid & out )
{
	const int k = std::max( 1, int( std::lround( kFogTerrCell / T ) ) );	// input texels per output texel
	const int n = ( N + k - 1 ) / k;
	out.nx = out.ny = n;
	out.cell = T * k;
	out.x0 = x0;
	out.y0 = y0;
	out.H.assign( size_t( n ) * size_t( n ), 0.0f );
	if ( N <= 0 || H.size() != size_t( N ) * size_t( N ) )
		return;
	// 1. box average per output texel: value sum and valid count
	std::vector<double> v( size_t( n ) * size_t( n ), 0.0 ), c( size_t( n ) * size_t( n ), 0.0 );
	for ( int j = 0; j < N; j++ )
		for ( int i = 0; i < N; i++ ) {
			const float h = H[size_t( j ) * size_t( N ) + size_t( i )];
			if ( !( h > noH * 0.5f ) )
				continue;
			const size_t o = size_t( j / k ) * size_t( n ) + size_t( i / k );
			v[o] += double( h );
			c[o] += 1.0;
		}
	// 2. separable Gaussian, normalised by the valid weight (holes and the map's edge do not pull the ground down)
	const double s = sigma / out.cell;
	const int r = std::max( 1, int( std::ceil( 3.0 * s ) ) );
	std::vector<double> w( size_t( 2 * r + 1 ) );
	for ( int d = -r; d <= r; d++ )
		w[size_t( d + r )] = std::exp( -0.5 * double( d ) * double( d ) / ( s * s ) );
	std::vector<double> tv( v.size(), 0.0 ), tc( c.size(), 0.0 );
	for ( int j = 0; j < n; j++ )
		for ( int i = 0; i < n; i++ ) {
			double a = 0.0, b = 0.0;
			for ( int d = -r; d <= r; d++ ) {
				const int ii = i + d;
				if ( ii < 0 || ii >= n )
					continue;
				const size_t o = size_t( j ) * size_t( n ) + size_t( ii );
				a += w[size_t( d + r )] * v[o];
				b += w[size_t( d + r )] * c[o];
			}
			tv[size_t( j ) * size_t( n ) + size_t( i )] = a;
			tc[size_t( j ) * size_t( n ) + size_t( i )] = b;
		}
	double sum = 0.0, cnt = 0.0;
	std::vector<char> ok( v.size(), 0 );
	for ( int j = 0; j < n; j++ )
		for ( int i = 0; i < n; i++ ) {
			double a = 0.0, b = 0.0;
			for ( int d = -r; d <= r; d++ ) {
				const int jj = j + d;
				if ( jj < 0 || jj >= n )
					continue;
				const size_t o = size_t( jj ) * size_t( n ) + size_t( i );
				a += w[size_t( d + r )] * tv[o];
				b += w[size_t( d + r )] * tc[o];
			}
			const size_t o = size_t( j ) * size_t( n ) + size_t( i );
			if ( b > 1e-9 ) {
				out.H[o] = float( a / b );
				ok[o] = 1;
				sum += a / b;
				cnt += 1.0;
			}
		}
	const float mean = cnt > 0.0 ? float( sum / cnt ) : 0.0f;
	for ( size_t o = 0; o < ok.size(); o++ )
		if ( !ok[o] )
			out.H[o] = mean;
	out.hmin = out.H.empty() ? 0.0f : *std::min_element( out.H.begin(), out.H.end() );
	out.hmax = out.H.empty() ? 0.0f : *std::max_element( out.H.begin(), out.H.end() );
}

namespace
{
WwFogTerrGrid & grid()
{
	static WwFogTerrGrid g;
	return g;
}
}

void wwFogTerrainPublish( WwFogTerrGrid && g )
{
	static std::uint64_t gen = 0;
	g.gen = ++gen;
	grid() = std::move( g );
}

const WwFogTerrGrid & wwFogTerrainGrid()
{
	return grid();
}

#ifndef WW_FOGTERRAIN_GATE
bool wwFogTerrainWanted()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_FOG_TERRAIN_RED" ) == QLatin1StringView( "absolute" ) );
	return !red;
}
#endif
