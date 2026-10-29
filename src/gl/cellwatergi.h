#pragma once
/* lane PRTP6 (bungo 10-25: water is dynamic, so water gets no PRTP). A cell water shape drawn the old way (Water row
 * off, the probe pass) through a cell program would read the probe GI; the guard zeroes the GI switches for its draw
 * and gives the program's next other draw back the values the pass set. Pure logic: tests/watergi. */

#include <algorithm>
#include <unordered_map>

struct WwWaterGiGuard
{
	static constexpr int kN = 5;	// cellGiOn, cellGiSky, cellSpecOcc, cellGiFineOn, cellDdgiOn (lane PRTOFF1) (cellGiHalfOn is set per draw)
	struct Entry
	{
		int saved[kN] = {};
		bool zeroed = false;
	};
	std::unordered_map<const void *, Entry> progs;
	enum Act { None, Zero, Restore };

	//! One draw of program p. Zero: save() the live values, then write zeros. Restore: write restore( p ).
	Act draw( const void * p, bool water ) const
	{
		const auto it = progs.find( p );
		const bool z = it != progs.end() && it->second.zeroed;
		if ( water )
			return z ? None : Zero;
		return z ? Restore : None;
	}
	void save( const void * p, const int * live )
	{
		Entry & e = progs[p];
		std::copy( live, live + kN, e.saved );
		e.zeroed = true;
	}
	const int * restore( const void * p )
	{
		Entry & e = progs[p];
		e.zeroed = false;
		return e.saved;
	}
	//! The pass wrote fresh values on p: nothing older is given back
	void passSet( const void * p )
	{
#ifndef WW_WATERGI_STALE_RED
		const auto it = progs.find( p );
		if ( it != progs.end() )
			it->second.zeroed = false;
#else
		(void)p;
#endif
	}
};
