#pragma once

// Lane VKAO3 (2026-11-16): cell_aodecal's loose uniforms (b31 of aodecal_view_vs at binding 287 and aodecal_view_ps at 31),
// from spirv-cross --reflect of the twins (notes/vkao3/tools/reflect_aodecal.sh, notes/vkair1/tools/reflect_table.py).
#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
// kT_aodecalVs: 1 fields, 64 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_aodecalVs[] = {
	{ "aoClip", 0, 4, 4, 0, 0 },
};
inline constexpr int kT_aodecalVsBytes = 64;

// kT_aodecalPs: 15 fields, 356 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_aodecalPs[] = {
	{ "aoMode", 0, 1, 1, 1, 0 }, { "aoSelf", 4, 1, 1, 0, 0 }, { "aoAdd", 8, 1, 1, 1, 0 }, { "aoProj", 16, 4, 1, 0, 0 },
	{ "aoSize", 32, 2, 1, 0, 0 }, { "aoSc", 40, 1, 1, 0, 0 }, { "aoM", 48, 4, 3, 0, 0 }, { "aoN", 96, 3, 3, 0, 0 },
	{ "aoLo", 144, 3, 1, 0, 0 }, { "aoHi", 160, 3, 1, 0, 0 }, { "aoDims", 176, 3, 1, 1, 0 },
	{ "aoFade", 188, 1, 1, 0, 0 }, { "aoC", 192, 3, 1, 0, 0 }, { "aoCoef", 208, 1, 9, 0, 0 },
	{ "aoRcut", 352, 1, 1, 0, 0 },
};
inline constexpr int kT_aodecalPsBytes = 368;
}  // namespace WwVkPresent
