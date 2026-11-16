#pragma once

// Lane VKOBJ18 (2026-11-16): game_taa_mv's loose uniforms (b31 of camvel_ps at binding 31), from spirv-cross --reflect
// of the twin (notes/vkobj18/tools/reflect_camvel.sh, notes/vkair1/tools/reflect_table.py).
#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
// kT_camvelPs: 3 fields, 80 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_camvelPs[] = {
	{ "reproj", 0, 4, 1, 0, 4 }, { "jitterNdc", 64, 2, 1, 0, 0 }, { "invSize", 72, 2, 1, 0, 0 },
};
inline constexpr int kT_camvelPsBytes = 80;
}  // namespace WwVkPresent
