#pragma once

/* Lane VKFAR2 (2026-11-14, the froxel item; plan rows L5, P4b, L8): the air's froxel volume on the present device.
 * The air pass (src/features/air/cellvolair.cpp) keeps two 3D volumes: the inject (each froxel's in-scatter and its
 * slice's transmittance, RGBA32F) and the integrated one lookdev_fog.glsl reads (volTex: the light and transmittance from
 * the eye to each froxel, RGBA16F; RGBA32F under the f32 red). While the view is on Vulkan both are shared targets
 * (sharedTarget type 1). GL still draws the inject (stages 0, 4, 1: its lights, clusters, lamp atlas, GI probes, history
 * and noise are GL's); the integrate (stages 2 / 3, the near grid and the far grid) runs here as compute
 * (res/shaders/hlsl/air/volair_integ_cs.hlsl): GL signals, the worker dispatches one column a thread per grid, GL waits.
 * The finished volume is then a Vulkan image that a Vulkan reader (the fog bloom P4b, the air L5, the precipitation L8)
 * finds by its key with no copy, with the numbers that read it (WwVolAirShare, cellvolair.h).
 * Red WW_VKVIEW_FOG_RED=1: GL draws its integrate (the volumes stay shared while WW_VKVIEW_AIR_RED is unset).
 * Telemetry: `vk fog:` lines (fogSay). Gate: tests/vkframe/fogview.sh. */

#include <cstdint>
#include <string>

namespace WwVkPresent
{
//! one grid of the packed volume: columns x rows, its layers base .. base + layers - 1
struct FogGrid
{
	int cols = 0, rows = 0, base = 0, layers = 0;
};
struct FogIntegRun
{
	std::uintptr_t inject = 0, integ = 0;  //!< the two volumes' sharedTarget keys
	unsigned injectTex = 0, integTex = 0;  //!< their GL names (sharedTargetIs)
	bool f32 = false;                      //!< the integrated volume is RGBA32F (else RGBA16F)
	FogGrid grid[2];                       //!< the near grid, then the far grid (layers 0: none)
	int red = 0;                           //!< gates only: volair_integ_cs.hlsl's red bits
};
/* GL current, the inject drawn: the integrate on Vulkan, one crossing for the grids given (the app: one a grid, the far
 * grid's only on its sum frames). false: why; nothing written (GL draws its integrate this frame) */
bool fogIntegRun( const FogIntegRun & t, std::string & why );
//! a frame whose integrate stayed on GL while the view was on Vulkan, and why
void fogOnGl( const char * why );
struct FogStat
{
	int runs = 0, gl = 0, fails = 0, dispatches = 0;  //!< since the last take
	double cpuMs = 0.0, maxMs = 0.0;                   //!< the drawing thread's fogIntegRun ms
	std::string glWhy, failWhy;
	int pipelines = 0;
	double buildMs = 0.0;
};
FogStat takeFogStat();
//! the `vk fog:` route line, one a 5 s window while the integrate is wanted on Vulkan
void fogSay( bool wanted, const char * glWhy );
}  // namespace WwVkPresent
