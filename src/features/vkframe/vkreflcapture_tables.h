#pragma once

// Lane VKFAR22 (2026-11-16): the reflection probe G-buffer capture's loose uniforms (reflcap_view_ps b31: cell_reflcap_ps on fxg's
// flags; reflcap_view_vs b287: cell_reflcap_vs on fxt's), from spirv-cross --reflect of the shipped SPIR-V
// (notes/vkfar22/tools/reflect_rc.sh, notes/vkair1/tools/reflect_table.py). A matrix's GL column k at + 16 k (fxPack).
#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
// kT_reflcapPs: 5 fields, 20 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_reflcapPs[] = {
	{ "rpHasBase", 0, 1, 1, 1, 0 }, { "rpHasVc", 4, 1, 1, 1, 0 }, { "rpAlphaTest", 8, 1, 1, 1, 0 },
	{ "alpha", 12, 1, 1, 0, 0 }, { "alphaThreshold", 16, 1, 1, 0, 0 },
};
inline constexpr int kT_reflcapPsBytes = 32;

// kT_reflcapVs: 5 fields, 208 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_reflcapVs[] = {
	{ "rpRelFromView", 0, 4, 1, 0, 4 }, { "rpClipFromRel", 64, 4, 1, 0, 4 }, { "modelViewMatrix", 128, 4, 1, 0, 4 },
	{ "uvScale", 192, 2, 1, 0, 0 }, { "uvOffset", 200, 2, 1, 0, 0 },
};
inline constexpr int kT_reflcapVsBytes = 208;
}  // namespace WwVkPresent
