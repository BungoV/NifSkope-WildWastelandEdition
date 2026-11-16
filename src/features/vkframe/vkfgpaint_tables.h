#ifndef WW_VKFGPAINT_TABLES_H
#define WW_VKFGPAINT_TABLES_H

/* Lane VKGROUND6 (2026-11-16): the b31 tables of the far ground page paint twins (fgpage_view_ps: cell_fgpage, fgbc1_view_ps:
 * cell_fgbc1), from spirv-cross --reflect of the shipped SPIR-V (notes/vkground6/tools/spv3.sh). */

#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
inline constexpr FxField kT_fgpagePs[] = {
	{ "fgMode", 0, 1, 1, 1, 0 }, { "fgDecalsOn", 4, 1, 1, 1, 0 }, { "fgGridMap", 16, 4, 1, 0, 0 },
	{ "fgGridN", 32, 2, 1, 0, 0 }, { "fgBinMap", 48, 4, 1, 0, 0 }, { "fgBinN", 64, 4, 1, 0, 0 },
	{ "fgStampCell", 80, 2, 1, 0, 0 }, { "fgStampSide", 88, 1, 1, 0, 0 }, { "fgPageDecals", 92, 1, 1, 1, 0 },
};
inline constexpr int kT_fgpagePsBytes = 96;
inline constexpr FxField kT_fgbc1Ps[] = {
	{ "fgSrcLevel", 0, 1, 1, 1, 0 }, { "fgBc1Red", 4, 1, 1, 1, 0 },
};
inline constexpr int kT_fgbc1PsBytes = 16;
}  // namespace WwVkPresent

#endif
