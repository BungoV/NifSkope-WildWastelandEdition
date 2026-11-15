// lane VKWATER4 (L7): the Vulkan twin of cell_underwater_ms.prog (water/cell_underwater_ms_ps.hlsl at b418ac4a): the
// per-sample draw over the samples the per-pixel twin left, each with its own depth.
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define WW_UW_SAMPLE 1
#include "underwater_view_ps.hlsl"
