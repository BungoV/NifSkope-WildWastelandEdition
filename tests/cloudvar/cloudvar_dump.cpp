// lane CLOUD5: the Cloud Variety density's CPU side for the offline gate (tests/spells/cloudvar_check.py builds this with
// g++). No GL, no Qt: src/gl/cloudnoise.h (the exe's noise bytes) + src/gl/cloudvolmath.h (the twin).
//
//   cloudvar_dump cal  <base0> <dens0> <depthKm> <base1> <top1> <dens1> <curlKm> <nocal 0|1>
//       "calT <k> v0..v16" for the kVarCal type anchors (Low, T = varCalT(k)), "calM v0..v16" (Mid)
//   cloudvar_dump grid <base0> <dens0> <depthKm> <base1> <top1> <dens1> <curlKm> <type> <spread> <swing>
//                      <mapR> <mapG> <mapB> <Ox> <Oy> <Oz> <Ux> <Uy> <Uz> <Vx> <Vy> <Vz> <nx> <ny>
//       the cal lines, then per row j: "g <j> d.." (varietyDensity at O + i U + j V), "n <j> .." (weatherNoise),
//       "c <j> cx cy cz .." (curlRaw)
//   cloudvar_dump light
//       "depth <T> <km>" (typeDepthKm, depth 1.5, tower 7) and "light <tmax> <depth> <tower> <preset> <km>"
#include "gl/cloudnoise.h"
#include "gl/cloudvolmath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace V = WwCloudVolMath;

namespace
{

float arg( char ** argv, int i )
{
	return float( std::atof( argv[i] ) );
}

struct Tables
{
	float calT[V::kVarCal][V::kCalN];
	float calM[V::kCalN];
};

void tables( const V::VarVolumes & vol, const V::Variety & v, float base0, float dens0, float base1, float top1, float dens1,
	bool nocal, Tables & t )
{
	for ( int k = 0; k < V::kVarCal; k++ ) {
		const float T = V::varCalT( k );
		V::varCalibrate( vol, v, 0, T, base0, base0 + V::typeDepthKm( T, v.depthKm, v.towerKm ), dens0, nocal, t.calT[k] );
	}
	V::varCalibrate( vol, v, 1, 0.0f, base1, top1, dens1, nocal, t.calM );
	for ( int k = 0; k < V::kVarCal; k++ ) {
		std::printf( "calT %d", k );
		for ( float c : t.calT[k] )
			std::printf( " %.9g", double( c ) );
		std::printf( "\n" );
	}
	std::printf( "calM" );
	for ( float c : t.calM )
		std::printf( " %.9g", double( c ) );
	std::printf( "\n" );
}

} // namespace

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		return 2;
	const std::string mode = argv[1];
	if ( mode == "light" ) {
		for ( float T : { 0.0f, 0.1667f, 0.3333f, 0.5f, 0.6667f, 0.8f, 0.9f, 1.0f } )
			std::printf( "depth %g %.9g\n", double( T ), double( V::typeDepthKm( T, 1.5f, V::kVarTowerKm ) ) );
		for ( float tm : { 0.6f, 0.8f, 1.0f } )
			for ( float pre : { 1.0f, 1.2f } )
				std::printf( "light %g 1.5 %g %g %.9g\n", double( tm ), double( V::kVarTowerKm ), double( pre ),
					double( V::varLightKm( tm, 1.5f, V::kVarTowerKm, pre ) ) );
		return 0;
	}
	const auto shape = WwCloudNoise::shapeVolume();
	const auto detail = WwCloudNoise::detailVolume();
	const V::VarVolumes vol{ shape.data(), WwCloudNoise::kShapeN, detail.data(), WwCloudNoise::kDetailN };
	if ( mode == "cal" && argc >= 10 ) {
		V::Variety v;
		v.depthKm = arg( argv, 4 );
		v.curlKm = arg( argv, 8 );
		Tables t;
		tables( vol, v, arg( argv, 2 ), arg( argv, 3 ), arg( argv, 5 ), arg( argv, 6 ), arg( argv, 7 ), std::atoi( argv[9] ) != 0, t );
		return 0;
	}
	if ( mode == "grid" && argc >= 26 ) {
		V::Variety v;
		v.depthKm = arg( argv, 4 );
		v.curlKm = arg( argv, 8 );
		v.type = arg( argv, 9 );
		v.spread = arg( argv, 10 );
		v.swing = arg( argv, 11 );
		const float base0 = arg( argv, 2 ), dens0 = arg( argv, 3 ), base1 = arg( argv, 5 ), top1 = arg( argv, 6 ), dens1 = arg( argv, 7 );
		Tables t;
		tables( vol, v, base0, dens0, base1, top1, dens1, false, t );
		const float map[4] = { arg( argv, 12 ), arg( argv, 13 ), arg( argv, 14 ), 0.0f };
		const float O[3] = { arg( argv, 15 ), arg( argv, 16 ), arg( argv, 17 ) };
		const float U[3] = { arg( argv, 18 ), arg( argv, 19 ), arg( argv, 20 ) };
		const float W[3] = { arg( argv, 21 ), arg( argv, 22 ), arg( argv, 23 ) };
		const int nx = std::atoi( argv[24] ), ny = std::atoi( argv[25] );
		const float baseKm[2] = { base0, base1 };
		const float topKm[2] = { base0 + V::typeDepthKm( std::min( 1.0f, v.type + v.spread ), v.depthKm, v.towerKm ), top1 };
		const float dens[2] = { dens0, dens1 };
		for ( int j = 0; j < ny; j++ ) {
			std::string g = "g " + std::to_string( j ), n = "n " + std::to_string( j ), c = "c " + std::to_string( j );
			char buf[96];
			for ( int i = 0; i < nx; i++ ) {
				const float x = O[0] + float( i ) * U[0] + float( j ) * W[0], y = O[1] + float( i ) * U[1] + float( j ) * W[1];
				const float z = O[2] + float( i ) * U[2] + float( j ) * W[2];
				std::snprintf( buf, sizeof( buf ), " %.9g",
					double( V::varietyDensity( vol, v, baseKm, topKm, dens, t.calT, t.calM, map, x, y, z ) ) );
				g += buf;
				std::snprintf( buf, sizeof( buf ), " %.9g", double( V::weatherNoise( x, y ) ) );
				n += buf;
				float cu[3];
				V::curlRaw( x, y, z, cu );
				std::snprintf( buf, sizeof( buf ), " %.9g %.9g %.9g", double( cu[0] ), double( cu[1] ), double( cu[2] ) );
				c += buf;
			}
			std::printf( "%s\n%s\n%s\n", g.c_str(), n.c_str(), c.c_str() );
		}
		return 0;
	}
	return 2;
}
