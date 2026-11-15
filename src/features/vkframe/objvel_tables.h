#pragma once

// Lane VKOBJ12 (2026-11-16): taa_objvel's loose uniforms (b31 of objvel_vs at binding 287 and objvel_ps at 31), from
// spirv-cross --reflect of the twins (notes/vkobj12/tools/reflect_objvel.sh, notes/vkair1/tools/reflect_table.py).
#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
// kT_objvelVs: 8 fields, 352 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_objvelVs[] = {
	{ "clipJit", 0, 4, 1, 0, 4 }, { "clipCur", 64, 4, 1, 0, 4 }, { "clipPrev", 128, 4, 1, 0, 4 },
	{ "modelViewMatrix", 192, 4, 1, 0, 4 }, { "velMode", 256, 1, 1, 1, 0 }, { "wwGrassWind", 272, 4, 1, 0, 0 },
	{ "wwGrassWind2", 288, 4, 1, 0, 0 }, { "normalMatrix", 304, 3, 1, 0, 3 },
};
inline constexpr int kT_objvelVsBytes = 352;

// kT_objvelPs: 15 fields, 104 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_objvelPs[] = {
	{ "uvScale", 0, 2, 1, 0, 0 }, { "uvOffset", 8, 2, 1, 0, 0 }, { "alphaTest", 16, 1, 1, 1, 0 },
	{ "alpha", 20, 1, 1, 0, 0 }, { "alphaThreshold", 24, 1, 1, 0, 0 }, { "wwGrassFade", 32, 3, 1, 0, 0 },
	{ "fxMode", 44, 1, 1, 1, 0 }, { "falloffParams", 48, 4, 1, 0, 0 }, { "fxOrtho", 64, 1, 1, 1, 0 },
	{ "fxDepthOrigin", 68, 2, 1, 1, 0 }, { "fxProj", 80, 2, 1, 0, 0 }, { "fxDistScale", 88, 1, 1, 0, 0 },
	{ "fxSoftDepth", 92, 1, 1, 0, 0 }, { "covMode", 96, 1, 1, 1, 0 }, { "covThreshold", 100, 1, 1, 0, 0 },
};
inline constexpr int kT_objvelPsBytes = 112;
}  // namespace WwVkPresent
