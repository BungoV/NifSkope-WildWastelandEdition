// lane CLOUDSHAPE1: the Cloud Shapes density's CPU side for the offline gate (tests/spells/cloudshape_check.py builds this
// with g++). No GL, no Qt: src/gl/cloudnoise.h (the exe's noise and body field) + src/gl/cloudvolmath.h (the twin).
//
//   cloudshape_dump stats
//       "shape <mean> <sd>" of the shape volume's bytes (kShpMean, kShpSd are these), "cell <mean> <sd> <min> <max>"
//   cloudshape_dump cell <out.bin>
//       the body field, kCellN^2 float32, row-major (y rows), as cloudvol.cpp uploads it
//   cloudshape_dump cal <T> <base0> <top0> <dens0> <sheet> <base1> <top1> <dens1> <nocal 0|1>
//       "type <b> topFrac domeK sheet", "cal <b> v0..v16" for the Low (type T) and Mid bands, as cloudvol.cpp builds them
//   cloudshape_dump grid <cal args...> <mapR> <mapG> <mapB> <Ox> <Oy> <Oz> <Ux> <Uy> <Uz> <Vx> <Vy> <Vz> <nx> <ny>
//       the cal lines, then per row j "g <j> d.." (shapeDensity at O + i U + j V, wind 0, footprint 0)
#include "gl/cloudnoise.h"
#include "gl/cloudvolmath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace V = WwCloudVolMath;

namespace
{

float arg( char ** argv, int i )
{
	return float( std::atof( argv[i] ) );
}

struct Sky
{
	V::ShapeType st[2];
	float base[2], top[2], dens[2], cal[2][V::kCalN];
};

Sky tables( const V::ShapeVolumes & vol, char ** argv, int i )
{
	Sky s;
	const float T = arg( argv, i ), sheet = arg( argv, i + 4 );
	const bool nocal = std::atoi( argv[i + 8] ) != 0;
	s.base[0] = arg( argv, i + 1 );
	s.top[0] = arg( argv, i + 2 );
	s.dens[0] = arg( argv, i + 3 );
	s.base[1] = arg( argv, i + 5 );
	s.top[1] = arg( argv, i + 6 );
	s.dens[1] = arg( argv, i + 7 );
	s.st[0] = V::shapeType( T );
	s.st[1] = V::shapeType( 1.0f / 3.0f );	// cloudvol.cpp bandsOf: the Mid band's rule
	for ( int b = 0; b < 2; b++ ) {
		const float sh = b == 0 ? std::max( sheet, s.st[0].sheet ) : s.st[1].sheet;
		V::shapeCalibrate( vol, s.st[b], b, s.base[b], s.top[b], s.dens[b], sh, nocal, s.cal[b] );
		std::printf( "type %d %.9g %.9g %.9g %.9g\n", b, double( s.st[b].topFrac ), double( s.st[b].domeK ),
			double( s.st[b].sheet ), double( s.st[b].anvil ) );
		std::printf( "cal %d", b );
		for ( float c : s.cal[b] )
			std::printf( " %.9g", double( c ) );
		std::printf( "\n" );
	}
	return s;
}

} // namespace

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		return 2;
	const std::vector<std::uint8_t> shape = WwCloudNoise::shapeVolume(), detail = WwCloudNoise::detailVolume();
	const std::vector<float> cell = WwCloudNoise::cellField();
	const V::ShapeVolumes vol{ shape.data(), WwCloudNoise::kShapeN, detail.data(), WwCloudNoise::kDetailN, cell.data(),
		WwCloudNoise::kCellN };
	if ( !std::strcmp( argv[1], "stats" ) ) {
		double m = 0.0, q = 0.0;
		for ( std::uint8_t v : shape ) {
			m += v / 255.0;
			q += ( v / 255.0 ) * ( v / 255.0 );
		}
		m /= double( shape.size() );
		std::printf( "shape %.6f %.6f\n", m, std::sqrt( std::max( q / double( shape.size() ) - m * m, 0.0 ) ) );
		double cm = 0.0, cq = 0.0, lo = 1.0, hi = 0.0;
		for ( float v : cell ) {
			cm += v;
			cq += double( v ) * v;
			lo = std::min( lo, double( v ) );
			hi = std::max( hi, double( v ) );
		}
		cm /= double( cell.size() );
		std::printf( "cell %.6f %.6f %.6f %.6f\n", cm, std::sqrt( cq / double( cell.size() ) - cm * cm ), lo, hi );
		return 0;
	}
	if ( !std::strcmp( argv[1], "cell" ) && argc > 2 ) {
		FILE * f = std::fopen( argv[2], "wb" );
		if ( !f )
			return 3;
		std::fwrite( cell.data(), sizeof( float ), cell.size(), f );
		std::fclose( f );
		std::printf( "wrote %d\n", int( cell.size() ) );
		return 0;
	}
	if ( !std::strcmp( argv[1], "cal" ) && argc >= 11 ) {
		tables( vol, argv, 2 );
		return 0;
	}
	if ( !std::strcmp( argv[1], "grid" ) && argc >= 25 ) {
		const Sky s = tables( vol, argv, 2 );
		const float map[4] = { arg( argv, 11 ), arg( argv, 12 ), arg( argv, 13 ), 0.0f };
		const float O[3] = { arg( argv, 14 ), arg( argv, 15 ), arg( argv, 16 ) };
		const float U[3] = { arg( argv, 17 ), arg( argv, 18 ), arg( argv, 19 ) };
		const float W[3] = { arg( argv, 20 ), arg( argv, 21 ), arg( argv, 22 ) };
		const int nx = std::atoi( argv[23] ), ny = std::atoi( argv[24] );
		for ( int j = 0; j < ny; j++ ) {
			std::printf( "g %d", j );
			for ( int i = 0; i < nx; i++ ) {
				const float x = O[0] + i * U[0] + j * W[0], y = O[1] + i * U[1] + j * W[1], z = O[2] + i * U[2] + j * W[2];
				std::printf( " %.6g", double( V::shapeDensity( vol, s.st, s.base, s.top, s.dens, s.cal, map, x, y, z ) ) );
			}
			std::printf( "\n" );
		}
		return 0;
	}
	return 2;
}
