// lane CLOUD1: prints the cloud map's CPU math (gl/cloudmath.h) on fixed cases, one "key values..." line each;
// notes/cloud1/unit/check.py gates them. Build: g++ -std=c++17 -I src tests/cloudmap/cloudmath_dump.cpp
#include "gl/cloudmath.h"

#include <cstdio>

using namespace WwCloudMath;

int main()
{
	// two 50 % layers in one band: union 0.75, the red "max" 0.5
	const int r2[2] = { Lower, Lower };
	const float c2[2] = { 0.5f, 0.5f };
	BandParams u[3], m[3];
	bandsOf( r2, c2, nullptr, 2, u, false );
	bandsOf( r2, c2, nullptr, 2, m, true );
	std::printf( "union2 %.6f\n", double( u[0].coverage ) );
	std::printf( "max2 %.6f\n", double( m[0].coverage ) );

	// bands by role: Fill + Upper + Wispy each land in their own band
	const int r3[3] = { Fill, Upper, Wispy };
	const float c3[3] = { 0.4f, 0.3f, 0.2f };
	BandParams b[3];
	bandsOf( r3, c3, nullptr, 3, b );
	std::printf( "bands %.6f %.6f %.6f sheet %.6f base %.1f %.1f %.1f\n", double( b[0].coverage ), double( b[1].coverage ),
		double( b[2].coverage ), double( b[0].sheet ), double( b[0].baseM ), double( b[1].baseM ), double( b[2].baseM ) );

	// the role rule by name
	std::printf( "roles %d %d %d %d %d %d\n", roleOfName( "sky/cloudsfill01.dds" ), roleOfName( "sky/cloudslower02.dds" ),
		roleOfName( "sky/cloudslowerlight01.dds" ), roleOfName( "sky/cloudsupper01.dds" ),
		roleOfName( "sky/cloudshorizon01.dds" ), roleOfName( "sky/x.dds" ) );

	// the transition fixture: a red tint at hue 350 -> one at hue 10, half way
	float a[3], c[3];
	rgbOfHsv( 350.0f, 0.8f, 1.0f, a );
	rgbOfHsv( 10.0f, 0.8f, 1.0f, c );
	BandParams pa, pc;
	pa.coverage = 0.6f;
	pc.coverage = 0.4f;
	for ( int k = 0; k < 3; k++ ) {
		pa.tint[k] = a[k];
		pc.tint[k] = c[k];
	}
	const BandParams h = lerpParams( pa, pc, 0.5f );
	float hh, hs, hv;
	hsv( h.tint, hh, hs, hv );
	float red[3];
	lerpTintColorRed( a, c, 0.5f, red );
	float rh, rs, rv;
	hsv( red, rh, rs, rv );
	std::printf( "transition cover %.6f hue %.3f redhue %.3f\n", double( h.coverage ), double( hh ), double( rh ) );

	// an empty band takes the other side's height
	BandParams e;
	BandParams f;
	f.coverage = 0.5f;
	f.baseM = 3000.0f;
	f.topM = 5000.0f;
	const BandParams g = lerpParams( e, f, 0.25f );
	std::printf( "emptyside base %.1f cover %.6f\n", double( g.baseM ), double( g.coverage ) );

	// the tint rule: a grey PNAM stays white, a saturated one tints
	const float grey[3] = { 200, 190, 180 }, orange[3] = { 255, 120, 40 };
	float tg[3], to[3];
	tintOfLayerColor( grey, tg );
	tintOfLayerColor( orange, to );
	std::printf( "tint grey %.4f %.4f %.4f orange %.4f %.4f %.4f\n", double( tg[0] ), double( tg[1] ), double( tg[2] ),
		double( to[0] ), double( to[1] ), double( to[2] ) );
	return 0;
}
