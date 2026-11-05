// Lane FIX1101A (moon): dumps gl/moonlight.h for the Python twin (moon_check.py --dump <exe>).
// g++ -std=c++17 -O1 -I src tests/features/sky/moonlight1101a/moonlight_dump.cpp -o <exe>
#include "features/sky/moonlight.h"
#include <cstdio>

int main()
{
	const float cases[][13] = {
		// sun rgb, sun dir, moon rgb, moon dir, wN
		{ 0.3f, 0.28f, 0.22f, 0.6f, -0.5f, 0.2f, 0.0f, 0.0f, 0.0f, -0.6f, -0.5f, 0.3f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 0.9f, 0.1f, -0.1f, 0.02f, 0.017f, 0.013f, -0.3f, -0.5f, 0.8f, 1.0f },
		{ 0.01f, 0.008f, 0.006f, 0.9f, 0.1f, 0.05f, 0.02f, 0.017f, 0.013f, -0.3f, -0.5f, 0.8f, 0.4f },
		{ 0.0f, 0.0f, 0.0f, 0.9f, 0.1f, -0.2f, 0.0f, 0.0f, 0.0f, -0.6f, -0.5f, 0.1f, 0.7f },
		{ 0.0f, 0.0f, 0.0f, 0.9f, 0.1f, -0.2f, 0.0f, 0.0f, 0.0f, -0.6f, -0.5f, 0.1f, 0.2f },
	};
	for ( const auto & c : cases ) {
		float rgb[3], dir[3];
		wwmoon::combine( c, c + 3, c + 6, c + 9, c[12], rgb, dir );
		std::printf( "%.7f %.7f %.7f %.7f %.7f %.7f\n", rgb[0], rgb[1], rgb[2], dir[0], dir[1], dir[2] );
	}
	for ( float e : { 5.0f, 0.0f, -1.0f, -3.0f, -5.5f, -6.0f, -20.0f } )
		std::printf( "nw %.1f %.7f\n", e, wwmoon::nightWeight( e ) );
	for ( double e : { 0.0, 45.0, 90.0, 160.0, 180.0, 200.0, 300.0 } )
		std::printf( "af %.1f %.7f\n", e, wwmoon::alphaFromElongation( e ) );
	const float med[4] = { 3.88757e-05f, -9936.0f, 1.0f / 789958.6f, 29811.8f }, layer[4] = { 0.0f, 0.0f, 0.0f, 5.667f };
	const float layer2[4] = { 0.3f, 50.0f, 1.0f / 900.0f, 5.667f }, ground[2] = { 64.0f, 1.0f / 4195.8f };
	for ( float mu : { 0.55249f, 0.2f, 0.9f } )
		for ( float tSky : { 0.484f, 0.95f } )
			std::printf( "dg %.7g %.7g %.7g\n", wwmoon::discAirGain( 183.0f, mu, tSky, med, layer, ground ),
				wwmoon::discAirGain( 183.0f, mu, tSky, med, layer2, ground ), wwmoon::airBandT( 183.0f, mu, med, layer2 ) );
	return 0;
}
