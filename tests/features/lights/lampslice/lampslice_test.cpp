/* lane PERFFIX2 b (item 5d): gl/lampslice.h, the lamp store's per-frame face budget, run as lampShadowPass runs it:
 * a slot set changes (16 new lamps, omni 6 faces / wall 5), each frame the dirty slots go nearest first, a store that
 * does not fit waits (its light read unshadowed) and is tried again the next frame.
 * Green: no frame spends more than max(slice, one store); every lamp is stored within ceil(faces / slice) + 1 frames;
 * the order is nearest first; a waiting slot is never read as shadowed; slice 0 stores all in frame 1 (the old).
 * Red -DLAMPSLICE_RED_NOFLOOR (no first-store floor): with a budget under one lamp (4 faces) nothing is ever stored: FAIL. */
#include "features/lights/lampslice.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;

static void check( bool ok, const char * what, const std::string & detail = std::string() )
{
	std::printf( "%s %s%s%s\n", ok ? "ok" : "FAIL", what, detail.empty() ? "" : ": ", detail.c_str() );
	if ( !ok )
		fails++;
}

struct Run
{
	int frames = 0, worst = 0, maxFrame = 0;
	bool order = true, neverShadowedWaiting = true, all = false;
};

// 16 slots in the dirty list's order (nearest first); faces per lamp; the frame each gets stored
static Run simulate( int slice, const std::vector<int> & faces )
{
	Run R;
	std::vector<int> storedAt( faces.size(), 0 );
	std::vector<char> pend( faces.size(), 0 );
	for ( int frame = 1; frame <= 200; frame++ ) {
		WwLampSlice::Budget b;
		b.slice = slice;
		for ( size_t k = 0; k < faces.size(); k++ ) {
			if ( storedAt[k] )
				continue;
			if ( !b.spend( faces[k] ) ) {
				pend[k] = 1;
				continue;
			}
			storedAt[k] = frame;
			pend[k] = 0;
		}
		R.worst = std::max( R.worst, b.spent );
		// the light buffer this frame: a pending slot reads unshadowed; a stored one shadowed
		for ( size_t k = 0; k < faces.size(); k++ )
			if ( pend[k] && storedAt[k] )
				R.neverShadowedWaiting = false;
		if ( std::all_of( storedAt.begin(), storedAt.end(), []( int f ) { return f > 0; } ) ) {
			R.all = true;
			R.frames = frame;
			break;
		}
	}
	for ( size_t k = 1; k < faces.size(); k++ )
		if ( storedAt[k] && storedAt[k - 1] && storedAt[k] < storedAt[k - 1] )
			R.order = false;
	R.maxFrame = *std::max_element( storedAt.begin(), storedAt.end() );
	return R;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::vector<int> faces;
	for ( int k = 0; k < 16; k++ )
		faces.push_back( k % 5 == 2 ? 5 : 6 );	// a few wall lamps
	int total = 0;
	for ( int f : faces )
		total += f;
	for ( int slice : { 0, 4, 6, 12, 24 } ) {
		const Run R = simulate( slice, faces );
		const int bound = slice == 0 ? 1 : ( total + std::max( slice, 6 ) - 1 ) / std::max( slice, 6 ) + 1;
		char what[120], det[160];
		std::snprintf( what, sizeof what, "slice %d: every lamp stored, within the bound, nearest first, no frame over budget", slice );
		std::snprintf( det, sizeof det, "all %d frames %d (bound %d) worst frame %d faces order %d", R.all, R.frames, bound, R.worst,
			R.order );
		check( R.all && R.frames <= bound && R.order && R.neverShadowedWaiting && ( slice == 0 || R.worst <= std::max( slice, 6 ) ),
			what, det );
	}
	check( simulate( 0, faces ).frames == 1, "slice 0: every store in the first frame (the old)" );
	check( simulate( 12, faces ).worst <= 12 && simulate( 12, faces ).frames >= 8, "slice 12 (the default): <= 12 faces a frame over 8+ frames" );
	std::printf( "LAMPSLICE %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
