#pragma once

// Lane VKAO4 (2026-11-16): cell_givol's loose uniforms (b31 of volmask_view_vs at binding 287 and volmask_view_ps at 31),
// from spirv-cross --reflect of the twins (notes/vkao4/tools/reflect_volmask.sh, notes/vkair1/tools/reflect_table.py).
#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
// kT_volmaskVs: 2 fields, 68 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_volmaskVs[] = {
	{ "volClip", 0, 4, 4, 0, 0 }, { "volGrow", 64, 1, 1, 0, 0 },
};
inline constexpr int kT_volmaskVsBytes = 80;

// kT_volmaskPs: 4 fields, 68 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_volmaskPs[] = {
	{ "volFull", 0, 2, 1, 1, 0 }, { "volProj", 8, 2, 1, 0, 0 }, { "volRow", 16, 4, 3, 0, 0 },
	{ "volBias", 64, 1, 1, 0, 0 },
};
inline constexpr int kT_volmaskPsBytes = 80;
}  // namespace WwVkPresent
