// Lane PERFFIX4: the previs rule (src/features/near/cellprevisrule.h). g++ -std=c++17 -I<tree>/src tests/features/near/previsrule/previsrule_test.cpp
// Red: -DPREVISRULE_RED swaps in the old rules (interiors follow the row, every tome used) and must fail.
#include "features/near/cellprevisrule.h"

#include <cstdio>
#include <vector>

using namespace WwPrevisRule;

#if defined( PREVISRULE_RED_ANYEDIT )
// PREVISSTALE2's red (WW_CELL_UMBRA_RED=anyedit): every later add or override is stale
static Stale ruleStale( bool hasFields, std::uint32_t writer, const std::vector<RefSrc> & refs )
{
	return tomeStale( hasFields, writer, refs, false, true );
}
static bool rulePrevis( bool rowOn, bool pinnedOff, bool interior )
{
	return previsFor( rowOn, pinnedOff, interior, true );
}
#elif defined( PREVISRULE_RED )
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
		check( !s.stale && s.added == 40 && s.moved == 0, "PREVISSTALE2: a later plugin only ADDS refs: fresh, 40 added" );
	}
	// PREVISSTALE2: an override that edits a light radius only (placement equal to Fallout4.esm's: settled 0)
	{
		std::vector<RefSrc> r = vanilla;
		r[9] = { 0, 5, false, false, 0 };
		r.push_back( { 5, 5, false, false } );
		const Stale s = ruleStale( true, 0, r );
		check( !s.stale && s.overridden == 1 && s.moved == 0, "PREVISSTALE2: override with the same placement + an add: fresh" );
	}
	// PREVISSTALE2: placement equal from plugin 2 on, the writer is 3 (it saw plugin 2's version): fresh
	{
		std::vector<RefSrc> r = vanilla;
		r[11] = { 0, 6, false, false, 2 };
		const Stale s = ruleStale( true, 3, r );
		check( !s.stale && s.moved == 0, "PREVISSTALE2: placement settled before the writer, later edit elsewhere: fresh" );
	}
	// PREVISSTALE2: placement changed by plugin 5 after the writer 0: stale, named plugin 5
	{
		std::vector<RefSrc> r = vanilla;
		r[12] = { 0, 5, false, false, 5 };
		for ( int k = 0; k < 4; k++ )
			r.push_back( { 5, 5, false, false } );
		const Stale s = ruleStale( true, 0, r );
		check( s.stale && s.moved == 1 && s.firstLater == 5, "PREVISSTALE2: an override moves a ref: STALE, moved 1, plugin 5" );
	}
	// PREVISSTALE2: a later plugin disables a ref (enable state differs: settled = its plugin)
	{
		std::vector<RefSrc> r = vanilla;
		r[13] = { 0, 4, false, true, 4 };
		const Stale s = ruleStale( true, 0, r );
		check( s.stale && s.disabled == 1, "PREVISSTALE2: a later plugin disables a ref: STALE" );
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
