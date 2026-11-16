#ifndef WW_VKREFLRELIGHT_H
#define WW_VKREFLRELIGHT_H

/* Lane VKFAR19 (2026-11-15): the reflection probes' relight (cellreflprobe.cpp, GL's cell_reflrelight program) on the present
 * device. GL draws one full-screen quad per todo tile into the probe atlas (level 0, viewport = the tile) reading the
 * probe's G-buffer cube layers (rpAlb / rpNd); here: one render pass over the SHARED atlas (B10G11R11, LOAD / STORE), one
 * draw a tile with its own b31 slice (rpLayer, rpTile, rpPos) and descriptor set, up to 32 tiles a crossing. The twins:
 * reflrelight_view_vs + reflrelight_view_ps (the base key) or reflrelight_view_rt_ps (the RT GI key). GL mips the atlas
 * after. Gate: tests/vkframe/reflrelightdraw.sh. */

#include <cstdint>
#include <string>
#include <vector>

#include "features/vkframe/vkwater.h"
#include "features/vkframe/vkreflrelight_tables.h"

namespace WwVkPresent
{
//! one todo tile: the probe's G-buffer layer, the tile's origin in the atlas and its size (pixels), the probe (world)
struct ReflRelightTile
{
	int layer = 0;
	float tile[3] = {};
	float pos[3] = {};
};

/*! the pass: GL's program (in use, its uniforms set, its blocks and samplers bound), its pixel stage's loader defines
 * (Program::wwDefs), the RT GI variant or not, the shared atlas / G-buffer cube arrays' keys, the tiles in GL's order */
struct ReflRelightRun
{
	unsigned prog = 0;
	std::string wwDefs;
	bool rt = false;
	std::uintptr_t atlasKey = 0, albKey = 0, ndKey = 0;
	std::vector<ReflRelightTile> tiles;
};

struct ReflRelightStat
{
	int runs = 0, tiles = 0, crossings = 0, copies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! the pixel stage's loader defines each twin is built for (Program::wwDefs)
inline const char * reflRelightKey( bool rt )
{
	return rt ? "WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO" : "WW_CELL_FOGLEAK+WW_FRAME_UBO";
}

//! the run on the present device, GL's state already read into s (vkreflrelight.inc)
bool reflRelightRunState( const ReflRelightRun & r, WaterRun & s, std::string & why );
//! "" when the device can take a run now
std::string reflRelightRefusal();
void reflRelightOnGl( const char * why );
ReflRelightStat takeReflRelightStat();

//! GL current, r.prog in use with its uniforms, blocks and samplers bound: GL's side read, then the run
template <class GL> bool reflRelightRun( const ReflRelightRun & r, GL & fn, std::string & why )
{
	static WaterRun s;  // big (the loose blocks): one, GL's thread only
	s = WaterRun();
	if ( r.rt )
		twinStateOf( &fn, r.prog, s, kT_reflrelightRtPs, int( sizeof( kT_reflrelightRtPs ) / sizeof( kT_reflrelightRtPs[0] ) ),
			kT_reflrelightRtPsBytes );
	else
		twinStateOf( &fn, r.prog, s, kT_reflrelightPs, int( sizeof( kT_reflrelightPs ) / sizeof( kT_reflrelightPs[0] ) ),
			kT_reflrelightPsBytes );
	return reflRelightRunState( r, s, why );
}
}  // namespace WwVkPresent

#endif
