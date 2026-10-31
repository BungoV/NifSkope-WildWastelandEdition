#ifndef WWFLYFIX_H
#define WWFLYFIX_H

/* lane FLYFIX1 (bungo 10-23 after it: "I can finally fly"): the free camera's two rules. lane REDRETIRE2 retired
 * FLYFIX1's env red from the shipping build; the old way lives only as red twins of tests/flyfix's red build
 * (src/wwredtwin.h, -DWW_REDTWIN): REDTWIN=FLYFIX=tick (no stall clamp) and REDTWIN=FLYFIX=refocus (the first move
 * after a refocus looks). Without WW_REDTWIN both twins are a constant false: NifSkope.exe holds the new rules only.
 * GLView (src/glview.cpp) calls these, and its in-app gate (the live verb "flycheck") checks both. */

#include "wwredtwin.h"

#include <algorithm>

//! The camera moves at most 0.1 s a tick, so a stalled frame or a spell away is never one long jump.
inline float wwFlyTickSeconds( float dT )
{
	return wwRedTwin( "FLYFIX", "tick" ) ? dT : std::min( dT, 0.1f );
}

/*! Focus, activation or the cursor leaving/entering sets `flyResync`; the next free-camera move then only re-centres
 *  (returns true and clears it), however far the cursor jumped. Otherwise false: the move looks. */
inline bool wwFlyResyncOnly( bool & flyResync )
{
	if ( !flyResync || wwRedTwin( "FLYFIX", "refocus" ) )
		return false;
	flyResync = false;
	return true;
}

#endif	// WWFLYFIX_H
