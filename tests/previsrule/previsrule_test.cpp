// Lane PERFFIX4: the previs rule (src/gl/cellprevisrule.h). g++ -std=c++17 -I<tree>/src tests/previsrule/previsrule_test.cpp
// Red: -DPREVISRULE_RED swaps in the old rules (interiors follow the row, every tome used) and must fail.
#include "gl/cellprevisrule.h"

#include <cstdio>
#include <vector>

using namespace WwPrevisRule;

#ifdef PREVISRULE_RED
static Stale ruleStale( bool hasFields, std::uint32_t writer, const std::vector<RefSrc> & refs )
{
	return tomeStale( hasFields, writer, refs, true );	// the old code used every tome
}
static bool rulePrevis( bool rowOn, bool pinnedOff, bool interior )
{
	return previsFor( rowOn, pinnedOff, interior, false );	// interiors followed the row
}
#else
static Stale ruleStale( bool hasFields, std::uint32_t writer, const std::vector<RefSrc> & refs )
{
	return tomeStale( hasFields, writer, refs, false );
}
static bool rulePrevis( bool rowOn, bool pinnedOff, bool interior )
{
	return previsFor( rowOn, pinnedOff, interior, true );
}
#endif

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s\n", what );
	} else {
		std::printf( "ok   %s\n", what );
	}
}

int main()
{
	// a vanilla cell: Fallout4.esm (0) wrote the previs, every ref is Fallout4.esm's
	std::vector<RefSrc> vanilla( 3000 );
	{
		const Stale s = ruleStale( true, 0, vanilla );
		check( !s.stale && s.added == 0 && s.overridden == 0, "vanilla cell, only the writer's refs: tome fresh" );
	}
	// a patch that wrote its own previs (writer 3) over refs it overrode itself (src 3) and added (owner 3)
	{
		std::vector<RefSrc> r = vanilla;
		r[10].src = 3;
		r.push_back( { 3, 3, false, false } );
		const Stale s = ruleStale( true, 3, r );
		check( !s.stale, "refs touched only by the previs writer itself: fresh" );
	}
	// ELFX: index 5, copied XPRI/VISI written by Fallout4.esm's bake is in ELFX's CELL override, but the writer is
	// the plugin the TOME belongs to; model: writer 0 (Fallout4.esm), ELFX adds refs
	{
		std::vector<RefSrc> r = vanilla;
		for ( int k = 0; k < 40; k++ )
			r.push_back( { 5, 5, false, false } );
		const Stale s = ruleStale( true, 0, r );
		check( s.stale && s.added == 40 && s.firstLater == 5, "ELFX-style later plugin adds refs: STALE, 40 added, plugin 5" );
	}
	// a later plugin moves one ref (an override)
	{
		std::vector<RefSrc> r = vanilla;
		r[7].src = 2;
		const Stale s = ruleStale( true, 1, r );
		check( s.stale && s.overridden == 1 && s.added == 0, "one ref overridden after the writer: STALE (moved)" );
	}
	// a later plugin deletes and disables refs
	{
		std::vector<RefSrc> r = vanilla;
		r[1] = { 0, 4, true, false };
		r[2] = { 0, 4, false, true };
		const Stale s = ruleStale( true, 0, r );
		check( s.stale && s.deleted == 1 && s.disabled == 1, "refs deleted and disabled after the writer: STALE" );
	}
	// no previs fields on the winning CELL
	{
		const Stale s = ruleStale( false, 2, vanilla );
		check( s.stale, "winning CELL carries no XPRI/VISI: stale (no writer)" );
	}
	// a ref overridden by an EARLIER plugin than the writer does not count
	{
		std::vector<RefSrc> r = vanilla;
		r[3].src = 1;
		const Stale s = ruleStale( true, 2, r );
		check( !s.stale, "ref overridden before the writer: fresh" );
	}
	// the default per document
	check( rulePrevis( false, false, true ), "interior, row off (default): previs ON" );
	check( !rulePrevis( false, false, false ), "exterior, row off (default): previs off (Hi-Z only)" );
	check( rulePrevis( true, false, false ), "exterior, row on: previs on" );
	check( !rulePrevis( true, true, true ), "WW_CELL_PREVIS=0 pins previs off in an interior too" );
	// the tome per document
	check( tomeFor( true, true, false, false ), "fresh tome, previs on: used" );
	check( !tomeFor( true, true, true, false ), "stale tome: refused" );
	check( tomeFor( true, true, true, true ), "stale tome under RED=stale: used" );
	check( !tomeFor( false, true, false, false ), "previs off: no tome" );
	check( !tomeFor( true, false, false, false ), "WW_CELL_UMBRA=0: no tome" );
	std::printf( "PREVISRULE %s (%d checks, %d failures)\n", fails ? "FAIL" : "PASS", checks, fails );
	return fails ? 1 : 0;
}
