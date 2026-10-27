// lane DUNESAND3: the ground legend's TX07 clause follows a live WW_TERRBLEND_RED set.
// Build: g++ -std=c++17 -I<tree>/src -I<qt6 include> -I<qt6 include>/QtCore groundlegendtest.cpp -lQt6Core
// Red: -DGROUNDLEGEND_RED keeps the open's legend (the base: built once at open, never rewritten); it must fail.
#include "cellgroundlegend.h"

#include <cstdio>

static int failed = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok" : "FAIL", what );
	if ( !ok )
		failed++;
}

static QString live( const QString & notes, bool nospec )
{
#ifdef GROUNDLEGEND_RED
	(void)nospec;
	return notes;
#else
	return cellGroundSpecClauseLive( notes, nospec );
#endif
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const QString head = QStringLiteral( "cell view: ok\n  ground: 9 LAND cells, 44 passes" );
	const QString tail = QStringLiteral( "; layers composited in the ATXT paint order\n  sky: on\n" );
	const QString open = head + cellGroundSpecClause( false, 38, 44, QStringLiteral( "passes" ) ) + tail;
	check( open.contains( QStringLiteral( "; 38 of 44 passes carry their TX07 _s map;" ) ), "open text is the pre-lane green clause" );

	const QString red = live( open, true );
	check( red == head + QStringLiteral( "; RED CONTROL WW_TERRBLEND_RED=nospec: no TX07 map on the ground "
		"(38 of 44 passes name one)" ) + tail, "live nospec set rewrites the clause to the red" );
	check( live( red, false ) == open, "live set back restores the open's green text byte for byte" );
	check( live( open, false ) == open, "an unchanged value leaves the notes alone" );
	check( live( red, true ) == red, "nospec twice is idempotent" );

	const QString mosaicRed = head + cellGroundSpecClause( true, 13, 16, QStringLiteral( "mosaic buckets" ) ) + tail;
	check( live( mosaicRed, false ) == head + QStringLiteral( "; 13 of 16 mosaic buckets carry their TX07 _s map" ) + tail,
		"a mosaic opened under nospec gets its green clause on a live set" );

	const QString none = QStringLiteral( "cell view: interior, no ground\n" );
	check( live( none, true ) == none, "notes without a ground clause are unchanged" );

	const QString two = open + open;
	check( live( two, true ) == red + red, "every clause in the notes is rewritten" );

	std::printf( "GROUNDLEGEND %s %d failed\n", failed ? "FAIL" : "PASS", failed );
	return failed ? 1 : 0;
}
