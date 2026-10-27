#ifndef GEOKEEPCLOCK_H
#define GEOKEEPCLOCK_H

#include <algorithm>
#include <cstdint>

/*! lane STREAMMEM1: which cached meshes a streaming paint may free. paint() hands back the geometry cache's paint
 *  counter as the second `keep` ago began, never later than 3 paints ago (a slow paint keeps what it drew);
 *  a mesh drawn before it is free to go. 0 = nothing yet (every 2 s at most, like the texture rule). */
struct GeoKeepClock
{
	std::uint32_t secEpoch[64] = {};   // the counter as each wall second began
	std::uint32_t paintEpoch[4] = {};
	std::uint32_t lastSec = 0, paints = 0, next = 0;

	//! one paint at wall second `sec` (from 1) whose cache counter reads `epoch`
	std::uint32_t paint( std::uint32_t sec, std::uint32_t epoch, std::uint32_t keep )
	{
		if ( sec != lastSec ) {
			for ( std::uint32_t t = std::max( lastSec + 1U, sec > 63U ? sec - 63U : 1U ); t <= sec; t++ )
				secEpoch[t & 63U] = epoch;
			lastSec = sec;
		}
		paintEpoch[paints++ & 3U] = epoch;
		if ( !keep || keep > 60U || paints < 4 || sec < next || sec <= keep )
			return 0;
		next = sec + 2U;
		return std::min( secEpoch[( sec - keep ) & 63U], paintEpoch[paints & 3U] );
	}
};

#endif
