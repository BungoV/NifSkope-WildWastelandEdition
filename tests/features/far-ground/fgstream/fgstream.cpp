// lane VKFAR14 (row G3 leftover): offline gate of the live far ground on a STREAMED open (src/features/far-ground/cellfgcut.h,
// the rule cellfarlod.cpp and cellfarground.cpp both call). Build: g++ -std=c++17 -I<tree>/src fgstream.cpp -o fgstream.exe
//
// Bar (committed before the first run, never widened):
//  B1 streamed opens prepare: streamPrepares( stream 1, built 1, noTerrain 0, red ) is true with no red.
//  B2 no hole at any handover: ring 0 = the open's centre +- ( roam + 3 ) cells (CELLSTREAM1: sr = roam + 2, reach sr + 1);
//     the camera walks 0..roam cells from the centre in 8 directions; loaded = the camera's 5x5. Every ring-0 cell that is
//     not loaded must be drawn by the live ground (prepared and not cut): holes 0 at every step, opens |1 and |3, roam 2..4.
//  B3 pages: at roam 4 (the default) the drawn cells fit the 254 pages a grid byte can name (cached mode not refused).
//  B4 plain opens unchanged: stream 0 -> Cut.has equals the old literal test ( wx >= cx0 && wx <= cx1 && ... ) on every cell
//     of a 13x13 ring with a 3x3 and a 5x5 block; prepared iff built and not the noterrain red, whatever WW_CELL_FARGROUND_RED.
//  B5 the reds: stream 1 with red nostream prepares nothing (B1 and B2 FAIL); a kept cut (Cut.off false, the pre-VKFAR14
//     uncut rule's missing half, env FG_GATE_RED=keepcut) leaves the open block a hole once the camera leaves it (B2 FAILS).
// Last line: `FGSTREAM GATE OK` or `FGSTREAM GATE FAILED`.

#include "features/far-ground/cellfgcut.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using CellFarGround::Cut;

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s\n", what );
	}
}

int main()
{
	const char * red = std::getenv( "WW_CELL_FARGROUND_RED" );
	const char * gred = std::getenv( "FG_GATE_RED" );
	const bool keepCut = gred && std::strcmp( gred, "keepcut" ) == 0;
	std::printf( "fgstream: red WW_CELL_FARGROUND_RED=%s FG_GATE_RED=%s\n", red ? red : "-", gred ? gred : "-" );

	// B1
	const bool prep = CellFarGround::streamPrepares( true, true, false, red );
	std::printf( "B1 streamed open prepares: %d\n", int( prep ) );
	check( prep, "B1 a streamed open prepares the live ground" );

	// B2 / B3
	const int dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
	for ( int roam = 2; roam <= 4; roam++ )
		for ( int h = 0; h <= 1; h++ ) {
			const int ox = 10, oy = -2, sr = roam + 2, ring = sr + 1;
			Cut c;
			c.x0 = ox - h;
			c.y0 = oy - h;
			c.x1 = ox + h;
			c.y1 = oy + h;
			c.off = !keepCut;	// what cellfarlod.cpp hands prepare on a streamed open
			int drawn = 0;
			for ( int wy = oy - ring; wy <= oy + ring; wy++ )
				for ( int wx = ox - ring; wx <= ox + ring; wx++ )
					drawn += ( prep && !c.has( wx, wy ) ) ? 1 : 0;
			int worst = 0, steps = 0;
			for ( const auto & d : dirs )
				for ( int k = 0; k <= roam; k++ ) {
					const int ex = ox + d[0] * k, ey = oy + d[1] * k;
					int holes = 0;
					for ( int wy = oy - ring; wy <= oy + ring; wy++ )
						for ( int wx = ox - ring; wx <= ox + ring; wx++ ) {
							const bool loaded = wx >= ex - 2 && wx <= ex + 2 && wy >= ey - 2 && wy <= ey + 2;
							if ( !loaded && !( prep && !c.has( wx, wy ) ) )
								holes++;
						}
					steps++;
					if ( holes > worst )
						worst = holes;
				}
			std::printf( "B2 roam %d open |%d: ring %dx%d, drawn %d, %d steps, worst holes %d\n", roam, 2 * h + 1, 2 * ring + 1,
				2 * ring + 1, drawn, steps, worst );
			check( worst == 0, "B2 no ring-0 cell outside the loaded 5x5 left undrawn" );
			if ( roam == 4 ) {
				std::printf( "B3 roam 4 open |%d: %d drawn cells, 254 pages at most\n", 2 * h + 1, drawn );
				check( drawn > 0 && drawn <= 254, "B3 the default roam's ring 0 fits the pages" );
			}
		}

	// B4
	int same = 0, cells = 0;
	for ( int h = 1; h <= 2; h++ ) {
		Cut c;
		c.x0 = -h;
		c.y0 = -h;
		c.x1 = h;
		c.y1 = h;
		for ( int wy = -6; wy <= 6; wy++ )
			for ( int wx = -6; wx <= 6; wx++ ) {
				const bool old = wx >= c.x0 && wx <= c.x1 && wy >= c.y0 && wy <= c.y1;
				cells++;
				same += c.has( wx, wy ) == old ? 1 : 0;
			}
	}
	const bool plainA = CellFarGround::streamPrepares( false, true, false, red );
	const bool plainB = CellFarGround::streamPrepares( false, true, false, "nostream" );
	const bool plainC = !CellFarGround::streamPrepares( false, false, false, nullptr ) && !CellFarGround::streamPrepares( false, true, true, nullptr )
		&& !CellFarGround::streamPrepares( true, true, true, nullptr ) && !CellFarGround::streamPrepares( true, false, false, nullptr );
	std::printf( "B4 plain cut: %d of %d cells equal the old test; plain prepares %d / %d (red nostream), refusals kept %d\n", same, cells,
		int( plainA ), int( plainB ), int( plainC ) );
	check( same == cells, "B4 the plain cut equals the old test" );
	check( plainA && plainB && plainC, "B4 plain opens prepare as before" );

	std::printf( "fgstream: %d checks, %d failed\n", checks, fails );
	std::printf( fails ? "FGSTREAM GATE FAILED\n" : "FGSTREAM GATE OK\n" );
	return fails ? 1 : 0;
}
