#ifndef WW_VKAIRTEMPORAL_H
#define WW_VKAIRTEMPORAL_H

/* Lane VKAIR14 (2026-11-16): the air inject's stage 1 with a history, the |3 temporal reader (cellvolair.cpp vaPassInner:
 * 64 draws, draw k writes the inject's layer k at attachment 0 and stratum p's layer p x 64 + k of the other history at
 * attachment 1 + p, MRT 1 + strata) on the present device. One crossing: one render pass a slice over 2D views of the SHARED
 * inject (RGBA32F 3D) and the SHARED out history (RGBA32F 3D, strata x slices deep), GL's b31 with volLayer = k a slice, each
 * slice its own descriptor set and host slice. The twins are the air inject's (airinject_view_vs + _ps / _rt_ps, the same
 * cell_volair keys): no new SPIR-V. Inputs from GL's units: shared targets and the present device's texel buffers in place,
 * other images one copy a run, other texel buffers one copy a run; refused by name: the history in (volHist) not shared,
 * an input that is a target (feedback), cellDdgiAtlas / cellRestir bound as GL-owned buffers, volLampAtlas bound and not
 * shared. Gate: tests/vkframe/airtemporalview.sh. */

#include <cstdint>
#include <string>

#include "features/vkframe/vkairinject.h"

/* lane VKAIR20: the far grid's stage 1 rides the same route (AirTemporalRun::far, airFarOnGl, takeAirFarStat; gate: the far
 * arms of tests/vkframe/airtemporalview.cpp). windef.h's empty `far` macro would eat the field's name: undefined here, FAR
 * kept meaning nothing */
#define WW_VK_AIRFAR 1
/* lane VKAIR21: the near grid's stage 1 with temporal off (cellvolair.cpp vaPassInner's last else) rides it too, in a third
 * instance (AirTemporalRun::plain, airPlainOnGl, takeAirPlainStat; gate: the plain arms of airtemporalview.cpp) */
#define WW_VK_AIRPLAIN 1
#ifdef far
#undef far
#ifdef FAR
#undef FAR
#define FAR
#endif
#endif

namespace WwVkPresent
{
/*! the pass: GL's program (in use, its uniforms set, stage 1's inputs on their units), its pixel stage's loader defines, the
 * RT GI variant or not, the grid (cols x rows x slices), the strata (4 or 6), the shared inject's and out history's keys;
 * red (route reds, the gate's): 1 volLayer 0 on every slice, 2 the strata attachments one late ((p + 1) mod strata) */
struct AirTemporalRun
{
	unsigned prog = 0;
	std::string wwDefs;
	bool rt = false;
	int cols = 0, rows = 0, slices = 0, strata = 0;
	std::uintptr_t injectKey = 0, histOutKey = 0;
	int red = 0;
	/* lane VKAIR20: the far grid's stage 1 (cellvolair.cpp vaPassInner's far loop): no history (strata 0, histOutKey 0), one
	 * attachment; draw k in [k0, k1) of slices writes the inject's layer base + k with volLayer k. Reds: 1 as above, 4 the
	 * base dropped (layers k), 8 the run's bounds ignored (0..slices) */
	bool far = false;
	int base = 0, k0 = 0, k1 = 0;
	/* lane VKAIR21: the near grid's stage 1 with temporal off: the far shape (far set, base 0, k 0..slices) in its own instance,
	 * so the near and far grids keep their pipelines and stats apart; plain without far is refused */
	bool plain = false;
};

struct AirTemporalStat
{
	int runs = 0, draws = 0, copies = 0, bufCopies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! the pixel stage's loader defines each twin is built for: the air inject's (one cell_volair program)
inline const char * airTemporalKey( bool rt )
{
	return airInjectKey( rt );
}

//! the run on the present device, GL's state already read into s (vkairtemporal.inc)
bool airTemporalRunState( const AirTemporalRun & r, WaterRun & s, std::string & why );
//! "" when the device can take a run now
std::string airTemporalRefusal();
void airTemporalOnGl( const char * why );
AirTemporalStat takeAirTemporalStat();
//! the route's WaterRun, in its own state (the reader's or the far grid's); GL's thread only
WaterRun & airTemporalScratch( bool isFar = false, bool plain = false );
//! lane VKAIR20: the far grid's GL fallback (its why) and stat, apart from the reader's
void airFarOnGl( const char * why );
AirTemporalStat takeAirFarStat();
//! lane VKAIR21: the near grid's temporal-off stage 1, its GL fallback and stat
void airPlainOnGl( const char * why );
AirTemporalStat takeAirPlainStat();

//! GL current, r.prog in use with its uniforms and stage 1's inputs bound: GL's side read, then the run
template <class GL> bool airTemporalRun( const AirTemporalRun & r, GL & fn, std::string & why )
{
	WaterRun & s = airTemporalScratch( r.far, r.plain );
	s = WaterRun();
	if ( r.rt )
		twinStateOf( &fn, r.prog, s, kT_airinjectRtPs, int( sizeof( kT_airinjectRtPs ) / sizeof( kT_airinjectRtPs[0] ) ), kT_airinjectRtPsBytes );
	else
		twinStateOf( &fn, r.prog, s, kT_airinjectPs, int( sizeof( kT_airinjectPs ) / sizeof( kT_airinjectPs[0] ) ), kT_airinjectPsBytes );
	return airTemporalRunState( r, s, why );
}
}  // namespace WwVkPresent

#endif
