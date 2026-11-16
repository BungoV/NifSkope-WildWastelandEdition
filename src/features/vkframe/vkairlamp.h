#ifndef WW_VKAIRLAMP_H
#define WW_VKAIRLAMP_H

/* Lane VKAIR16 (2026-11-16): the air's stage 4, the lamp volume's walk (cellvolair.cpp vaPassInner, volStage 4: 64 draws, draw k
 * writes lvHist out layer k (RGBA16F) at attachment 0 and lvNow out layer k (RGBA32F) at attachment 1, MRT 2) on the present
 * device. One crossing: one render pass a slice over 2D views of the two SHARED outs, GL's b31 with volLayer = k a slice, each
 * slice its own descriptor set and host slice. The twins are the air inject's (airinject_view_vs + _ps / _rt_ps, the same
 * cell_volair keys): no new SPIR-V. Inputs from GL's units: shared targets and the present device's texel buffers in place,
 * other images one copy a run, other texel buffers (the light list) one copy a run; refused by name: an out that is an input
 * (feedback), volLampHist / volLampNow unbound or not shared while volLampHistOk (GL's unbound sampler reads (0,0,0,1), Vulkan's
 * zero view 0), volStbn not shared, cellDdgiAtlas / cellRestir bound as GL-owned buffers, volLampAtlas bound and not shared.
 * Gate: tests/vkframe/airlampview.sh. */

#include <cstdint>
#include <string>

#include "features/vkframe/vkairinject.h"

namespace WwVkPresent
{
/*! the pass: GL's program (in use, its uniforms set, stage 4's inputs on their units), its pixel stage's loader defines, the RT
 * GI variant or not, the grid (cols x rows x slices), the shared lvHist out's and lvNow out's keys;
 * red (route reds, the gate's): 1 volLayer 0 on every slice, 2 the out slices one late (k -> (k + 1) mod slices) */
struct AirLampRun
{
	unsigned prog = 0;
	std::string wwDefs;
	bool rt = false;
	int cols = 0, rows = 0, slices = 0;
	std::uintptr_t histOutKey = 0, nowOutKey = 0;
	int red = 0;
};

struct AirLampStat
{
	int runs = 0, draws = 0, copies = 0, bufCopies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! the pixel stage's loader defines each twin is built for: the air inject's (one cell_volair program)
inline const char * airLampKey( bool rt )
{
	return airInjectKey( rt );
}

//! the run on the present device, GL's state already read into s (vkairlamp.inc)
bool airLampRunState( const AirLampRun & r, WaterRun & s, std::string & why );
//! "" when the device can take a run now
std::string airLampRefusal();
void airLampOnGl( const char * why );
AirLampStat takeAirLampStat();
//! the route's WaterRun, in its own state; GL's thread only
WaterRun & airLampScratch();

//! GL current, r.prog in use with its uniforms and stage 4's inputs bound: GL's side read, then the run
template <class GL> bool airLampRun( const AirLampRun & r, GL & fn, std::string & why )
{
	WaterRun & s = airLampScratch();
	s = WaterRun();
	if ( r.rt )
		twinStateOf( &fn, r.prog, s, kT_airinjectRtPs, int( sizeof( kT_airinjectRtPs ) / sizeof( kT_airinjectRtPs[0] ) ), kT_airinjectRtPsBytes );
	else
		twinStateOf( &fn, r.prog, s, kT_airinjectPs, int( sizeof( kT_airinjectPs ) / sizeof( kT_airinjectPs[0] ) ), kT_airinjectPsBytes );
	return airLampRunState( r, s, why );
}
}  // namespace WwVkPresent

#endif
