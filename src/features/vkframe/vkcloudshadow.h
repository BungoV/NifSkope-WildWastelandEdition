#pragma once
/* lane VKCLOUD3 (2026-11-16): the cloud shadow map's build (cloudshadow.cpp, cloud_shadowmap) on the present device,
 * behind WW_VKVIEW=1. The map (RGBA16F n x n) is a shared target the pass LOADs: a slice writes its rows only. */
#include "features/vkframe/vkpresent.h"

namespace WwVkPresent
{
// cloud_shadowmap_view_ps's b31 (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_cloudshadow[] = {
	{ "cvBands.baseKm", 0, 3, 1, 0, 0 }, { "cvBands.topKm", 16, 3, 1, 0, 0 }, { "cvBands.density", 32, 3, 1, 0, 0 },
	{ "cvBands.coverage", 48, 3, 1, 0, 0 }, { "cvBands.sheet", 60, 1, 1, 0, 0 }, { "cvBands.mapRect", 64, 4, 1, 0, 0 },
	{ "cvBands.windKm", 80, 3, 1, 0, 0 }, { "cvCal", 96, 1, 34, 0, 0 }, { "csE1", 640, 3, 1, 0, 0 },
	{ "csE2", 656, 3, 1, 0, 0 }, { "csS", 672, 3, 1, 0, 0 }, { "csFit", 688, 4, 1, 0, 0 }, { "csSize", 704, 2, 1, 0, 0 },
	{ "csMode", 712, 1, 1, 1, 0 }, { "csSteps", 716, 1, 1, 1, 0 }, { "csFs", 720, 3, 1, 0, 0 },
	{ "csSheetN", 732, 1, 1, 1, 0 }, { "csSheetP", 736, 4, 1, 0, 0 }, { "csSheetA", 752, 4, 8, 0, 0 },
	{ "csSheetB", 880, 4, 8, 0, 0 }, { "csSheetC", 1008, 4, 8, 0, 0 }, { "csUnitKm", 1136, 1, 1, 0, 0 },
};
inline constexpr int kT_cloudshadowBytes = 1152;
inline constexpr int kCloudShadowSheets = 8;
struct CloudShadowRun
{
	int n = 0, row0 = 0, rows = 0;  //!< the map's side; GL's scissor rows ( 0, row0, n, rows )
	std::uintptr_t key = 0;         //!< the map's sharedTarget key (RGBA16F n x n, made by the owner)
	FxTex tex[3];                   //!< cvShape, cvDetail (3D), cvMap (t64..t66)
	FxTex sheet[kCloudShadowSheets], sheet2[kCloudShadowSheets];  //!< csSheetTex[8] (t67), csSheetTex2[8] (t75)
	std::uint8_t loose[kT_cloudshadowBytes] = {};
	int red = 0;                    //!< gates only: 1 the sheet arrays swapped, 2 CLEAR instead of LOAD, 4 csS shifted 4 bytes
};
/* GL current, cloud_shadowmap's uniforms set and its units bound: the map's rows drawn on Vulkan. false: why; nothing
 * drawn (GL draws its quad). */
bool cloudShadowRun( const CloudShadowRun & t, std::string & why );
void cloudShadowOnGl( const char * why );
FxStat takeCloudShadowStat();
}  // namespace WwVkPresent
