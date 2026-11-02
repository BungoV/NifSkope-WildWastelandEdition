// lane OBJFADE1: the game's per-class distance fade law (src/gl/cellobjfade.h).
// g++ -std=c++17 -I<tree>/src tests/objfade/objfade_test.cpp; red: -DOBJFADE_RED (today: only decal shapes fade).
#include "gl/cellobjfade.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace WwObjFade;

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

// the cell view's rule for a placed non-decal ref: the red is today's (no fade outside decal-flag shapes)
static bool culled( float dist, float r, std::uint8_t type, std::uint32_t ref, const Config & c )
{
#ifdef OBJFADE_RED
	(void) dist; (void) r; (void) type; (void) ref; (void) c;
	return false;
#else
	return hidden( dist, r, type, ref, false, c );
#endif
}

static std::uint32_t fourcc( const char * s )
{
	std::uint32_t v;
	std::memcpy( &v, s, 4 );
	return v;
}

static bool near1( float a, float b, float tol ) { return std::fabs( a - b ) <= tol; }

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// the class table
	check( typeOf( fourcc( "MISC" ), false ) == Items && typeOf( fourcc( "WEAP" ), false ) == Items
		&& typeOf( fourcc( "SLGM" ), false ) == Items && typeOf( fourcc( "AMMO" ), false ) == Items, "items by base type" );
	check( typeOf( fourcc( "STAT" ), false ) == Objects && typeOf( fourcc( "SCOL" ), false ) == Objects
		&& typeOf( fourcc( "LIGH" ), false ) == Objects && typeOf( fourcc( "LIGH" ), false, true ) == Items, "objects, a carried light is an item" );
	check( typeOf( fourcc( "NPC_" ), false ) == Actors && typeOf( fourcc( "STAT" ), true ) == Actors, "actors" );

	// the worked numbers of the exe read (FOV 70, G 5): switch-out distances
	const Mults low = presetMults( 0 ), ultra = presetMults( 3 );
	const float dItemLow = distanceAt( 0.3f, 10.0f, Items, low, 70.0f, false );
	const float dItemUltra = distanceAt( 0.3f, 10.0f, Items, ultra, 70.0f, false );
	const float dStatUltra = distanceAt( 0.3f, 100.0f, Objects, ultra, 70.0f, false );
	const float dLarge = distanceAt( 0.3f, 3000.0f, Objects, ultra, 70.0f, false );
	std::printf( "     item r10 Low %.0f Ultra %.0f, static r100 Ultra %.0f, r3000 %.0f\n", double( dItemLow ),
		double( dItemUltra ), double( dStatUltra ), double( dLarge ) );
	check( near1( dItemLow, 1457.0f, 2.0f ) && near1( dItemUltra, 9714.0f, 2.0f ), "item switch-out 1457 / 9714" );
	check( near1( dStatUltra, 30171.0f, 3.0f ), "static r 100 at Ultra 30171" );
	check( near1( dLarge, 22846.0f, 3.0f ), "a large ref (r 3000) 22846, no multiplier" );
	check( effectiveType( Objects, 3000.0f, true ) == Objects, "a large ref indoors stays an object" );
	check( presetMults( -1 ).objects == 4.5f && presetMults( -1 ).items == 2.5f && presetMults( -1 ).actors == 6.0f, "exe defaults 4.5/2.5/6" );

	// FLOOR: a c3-like 5x5 block (20480 u square), camera in the middle, small items: Low culls >= 50 %
	Config cLow;
	cLow.mults = low;
	int items = 0, gone = 0;
	for ( int y = 0; y < 128; y++ )
		for ( int x = 0; x < 128; x++ ) {
			const float px = ( float( x ) + 0.5f ) / 128.0f * 20480.0f - 10240.0f;
			const float py = ( float( y ) + 0.5f ) / 128.0f * 20480.0f - 10240.0f;
			const float r = 4.0f + float( ( x * 7 + y * 13 ) % 40 );	// 4..43 u: cans, ammo, guns, boxes
			items++;
			gone += culled( std::sqrt( px * px + py * py ), r, Items, std::uint32_t( y * 128 + x ), cLow ) ? 1 : 0;
		}
	std::printf( "     Low: %d of %d item runs culled (%.1f %%)\n", gone, items, 100.0 * gone / items );
	check( gone * 2 >= items, "FLOOR Low culls >= 50 % of the item runs" );

	// PICTURE: at Ultra nothing within 4.9k u is hidden, any class, any radius, FOV 60..100
	Config cUltra;
	cUltra.mults = ultra;
	bool same = true;
	for ( int fov = 60; fov <= 100; fov += 10 ) {
		cUltra.fovDeg = float( fov );
		for ( float r = 0.0f; r < 6000.0f; r += 7.0f )
			for ( std::uint8_t t : { std::uint8_t( Objects ), std::uint8_t( Items ), std::uint8_t( Actors ) } )
				for ( float d = 0.0f; d <= 4900.0f; d += 100.0f )
					same = same && !culled( d, r, t, 1u, cUltra );
	}
	check( same, "Ultra: every ref within 4.9k u drawn (FOV 60..100)" );

	// the band and rung 0 are options: off = the game rule
	Config band = cLow;
	band.band = true;
	const float dIn = distanceAt( 0.7f, 10.0f, Items, low, 70.0f, false );
	int kept = 0, n = 0;
	for ( std::uint32_t ref = 1; ref < 4000; ref++ ) {
		n++;
		kept += hidden( 0.5f * ( dIn + dItemLow ), 10.0f, Items, ref, false, band ) ? 0 : 1;
	}
	std::printf( "     band: switch-in %.0f, mid-band kept %d of %d\n", double( dIn ), kept, n );
	check( kept > n * 4 / 10 && kept < n * 6 / 10, "the dithered band keeps about half at its middle" );
	check( !hidden( dIn - 1.0f, 10.0f, Items, 7u, false, band ) && hidden( dItemLow + 1.0f, 10.0f, Items, 7u, false, band ),
		"the band: all in before the switch-in, none past the switch-out" );
	Config ang = cLow;
	ang.law = Angular;
	check( hidden( 10.0f / 0.001f, 10.0f, Items, 1u, false, ang ) && !hidden( 10.0f / 0.002f, 10.0f, Items, 1u, false, ang ),
		"rung 0: hidden below 0.0014 rad" );
	check( !hidden( 1.0e6f, 10.0f, None, 1u, false, cLow ), "a run with no class never fades" );

	std::printf( "%s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
