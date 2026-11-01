// lane FIX1101A lodch r2 (LATE ITEM 23): the VANILLA far path's budget (src/cellfarvanilla.h wwFarVanChargeMB, read by
// cellfarlod.cpp's overBudget before every chunk) on SHOTS1101's c3 vanilla rows. Inputs, MEASURED:
//   terrain + skirt triangles per ring: run2c3/live_c3_g13.notes "far lod: source ring i level L type vanilla" lines
//   ring 0 objects: the same line (386,920 tris, 8 chunks)
//   rings 1-2 objects: the game's .bto in the same notes family ("far lod: census ring 1/2 ... the game's .bto T tris
//   from C chunks") at 36,627 / 27,420 tris a chunk, times the vanilla ring's chunks (72 at level 4, 20 at level 8)
//   arrays: 4 of the game's .bto chunks hold 106-147 B a triangle as CellMesh arrays (notes/fix1101a/lodch/r2/btostats.py)
// The walk is the code's: terrain of every ring first, then objects ring by ring, overBudget asked before each chunk.
// Green: every object chunk of rings 0-2 is built and the charge stays under 2048 MB. Red (-DVANBUDGET_RED: every
// triangle a document row, the code before r2) must stop at ring 1's first chunk, as the c3 notes measured
// ("objects NOT BUILT past chunk -32,0").
#include "cellfarvanilla.h"

#include <cstdio>

int main()
{
	const double budgetMB = 2048.0;
	const long long terrain[5] = { 15559, 151027, 32342, 15350, 68680 };
	const long long skirt[5] = { 9608, 28048, 61984, 88316, 35960 };
	const int chunks[3] = { 8, 72, 20 };
	const long long perChunk[3] = { 386920 / 8, 36627, 27420 };
	long long tris = 0, side = 0;
	auto add = [&]( long long t, bool toSide ) {
		tris += t;
#ifndef VANBUDGET_RED
		if ( toSide )
			side += t;
#else
		( void ) toSide;
#endif
	};
	for ( int i = 0; i < 5; i++ )
		add( terrain[i] + skirt[i], true );
	int built[3] = { 0, 0, 0 }, refusedRing = -1, refusedChunk = -1;
	for ( int r = 0; r < 3 && refusedRing < 0; r++ )
		for ( int c = 0; c < chunks[r]; c++ ) {
			if ( wwFarVanChargeMB( tris, side ) > budgetMB ) {
				refusedRing = r;
				refusedChunk = c;
				break;
			}
			add( perChunk[r], true );
			built[r]++;
		}
	const double mb = wwFarVanChargeMB( tris, side );
	std::printf( "objects built: ring 0 %d of %d, ring 1 %d of %d, ring 2 %d of %d; charge %.0f MB of %.0f; refused %s\n",
		built[0], chunks[0], built[1], chunks[1], built[2], chunks[2], mb, budgetMB,
		refusedRing < 0 ? "none" : built[1] == 0 ? "no chunk of ring 1 (the c3 notes: NOT BUILT past chunk -32,0)" : "elsewhere" );
	if ( refusedRing >= 0 )
		std::printf( "stopped in ring %d before its chunk %d (equal chunks here; the real ring 0 chunks differ in size)\n",
			refusedRing, refusedChunk );
	const double measuredArrays = 147.0;	// the largest of the 4 measured chunks, B a triangle
	const double charged = wwFarVanChargeMB( 1048576, 1048576 ) * 1024.0 * 1024.0 / 1048576.0;
	std::printf( "arrays charged %.0f B a triangle, measured at most %.0f (the charge must not be under the measure)\n",
		charged, measuredArrays );
	const bool ok = refusedRing < 0 && built[1] == chunks[1] && built[2] == chunks[2] && mb <= budgetMB
		&& charged >= measuredArrays;
	std::printf( ok ? "VANBUDGET PASS\n" : "VANBUDGET FAIL\n" );
	return ok ? 0 : 1;
}
