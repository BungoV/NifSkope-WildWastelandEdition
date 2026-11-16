#ifndef WW_VKAIRINJECT_H
#define WW_VKAIRINJECT_H

/* Lane VKAIR5 (2026-11-16): the air inject's stage 0 (cellvolair.cpp vaPassInner, the first drawShape: each froxel column's
 * opaque ray distances (far, near) into the RG32F tile) on the present device. GL draws one full-screen quad (viewport
 * cols x rows) with cell_volair at volStage 0 reading volGbuf (the shared D24S8 depth, or the AO pass's RGBA32F g-buffer);
 * here: one colour-only render pass over the SHARED RG32F tile (LOAD / STORE, GENERAL), one draw of 6 vertices, GL's b31
 * as is. Only volGbuf is fed: every other declared image reads a zero view and every texel buffer a 16-byte zero buffer, so
 * the inject's volumes, cascades and atlases are never copied. The twins: airinject_view_vs + airinject_view_ps (the default
 * key) or airinject_view_rt_ps (the all-on RT GI key). Gate: tests/vkframe/airinjectview.sh. */

#include <cstdint>
#include <string>

#include "features/vkframe/vkwater.h"
#include "features/vkframe/vkairinject_tables.h"

namespace WwVkPresent
{
/*! the pass: GL's program (in use, its uniforms set, volGbuf on its unit), its pixel stage's loader defines (Program::wwDefs),
 * the RT GI variant or not, the tile's size and its shared target's key; red (route reds, the gate's): 1 fogDistScale doubled,
 * 2 volGbufZ flipped, 4 volGbufSize.x one short, 8 volDims.y one more (b31 fields only) */
struct AirInjectRun
{
	unsigned prog = 0;
	std::string wwDefs;
	bool rt = false;
	int cols = 0, rows = 0;
	std::uintptr_t tileKey = 0;
	int red = 0;
};

struct AirInjectStat
{
	int runs = 0, copies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! the pixel stage's loader defines each twin is built for (Program::wwDefs)
inline const char * airInjectKey( bool rt )
{
	return rt ? "WW_CELL_AIR_SKYVIEW+WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO"
		: "WW_CELL_AIR_SKYVIEW+WW_CELL_FOGLEAK+WW_FRAME_UBO";
}

//! the run on the present device, GL's state already read into s (vkairinject.inc)
bool airInjectRunState( const AirInjectRun & r, WaterRun & s, std::string & why );
//! "" when the device can take a run now
std::string airInjectRefusal();
void airInjectOnGl( const char * why );
AirInjectStat takeAirInjectStat();
//! the route's WaterRun (big: the loose blocks), in its own state; GL's thread only
WaterRun & airInjectScratch();

//! GL current, r.prog in use with its uniforms and volGbuf bound: GL's side read, then the run
template <class GL> bool airInjectRun( const AirInjectRun & r, GL & fn, std::string & why )
{
	WaterRun & s = airInjectScratch();
	s = WaterRun();
	if ( r.rt )
		twinStateOf( &fn, r.prog, s, kT_airinjectRtPs, int( sizeof( kT_airinjectRtPs ) / sizeof( kT_airinjectRtPs[0] ) ), kT_airinjectRtPsBytes );
	else
		twinStateOf( &fn, r.prog, s, kT_airinjectPs, int( sizeof( kT_airinjectPs ) / sizeof( kT_airinjectPs[0] ) ), kT_airinjectPsBytes );
	return airInjectRunState( r, s, why );
}
}  // namespace WwVkPresent

#endif
