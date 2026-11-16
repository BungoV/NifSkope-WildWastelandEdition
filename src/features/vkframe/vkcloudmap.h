#pragma once
/* lane VKCLOUD4 (2026-11-16): the cloud map's build (cloudmap.cpp buildMap, cloudmap.prog) on the present device, behind
 * WW_VKVIEW=1. The map (RGBA8 n x n) is a shared target the pass CLEARs to 0, then each layer is drawn with GL's union
 * blend (dst = src (1 - dst) + dst; the red "max": MAX) into its band's channels. */
#include "features/vkframe/vkpresent.h"

#include <vector>

namespace WwVkPresent
{
// cloudmap_view_vs's b31 (at 32) and cloudmap_view_ps's b31 (spirv-cross --reflect of the shipped SPIR-V; tools:
// notes/vkair1/tools/reflect_table.py, notes/vkcloud4/tools/twins.sh)
inline constexpr FxField kT_cloudmap_vs[] = {
	{ "cmHeight", 0, 1, 1, 0, 0 }, { "cmHalfExtent", 4, 1, 1, 0, 0 }, { "uvOffset", 8, 2, 1, 0, 0 },
	{ "cmShow", 16, 1, 1, 1, 0 }, { "cmQuad", 20, 1, 1, 1, 0 },
};
inline constexpr int kT_cloudmap_vsBytes = 32;
inline constexpr FxField kT_cloudmap_ps[] = {
	{ "texBlend", 0, 1, 1, 0, 0 }, { "alpha", 4, 1, 1, 0, 0 }, { "cmShow", 8, 1, 1, 1, 0 },
};
inline constexpr int kT_cloudmap_psBytes = 16;
inline constexpr int kCloudMapLayers = 32;          //!< FO4's cloud layers per weather
inline constexpr int kCloudMapVerts = 1 << 17;      //!< every layer's vertices together
inline constexpr int kCloudMapIndices = 3 << 17;
struct CloudMapLayer
{
	FxTex tex, tex2;                                //!< Tex (t64), Tex2 (t65)
	std::uint8_t vs[kT_cloudmap_vsBytes] = {}, ps[kT_cloudmap_psBytes] = {};
	int mask = 15;                                  //!< GL's colour mask: 1 R, 2 G, 4 B, 8 A
	int firstVertex = 0, firstIndex = 0, nIdx = 0;  //!< into CloudMapRun::verts / idx
};
struct CloudMapRun
{
	int n = 0;                     //!< the map's side
	std::uintptr_t key = 0;        //!< the map's sharedTarget key (RGBA8 n x n, made by the owner)
	bool max = false;              //!< the red WW_CLOUDMAP_RED=max: GL_MAX instead of the union
	int layers = 0;
	CloudMapLayer layer[kCloudMapLayers];
	std::vector<float> verts;      //!< 9 floats a vertex: position xyz, colour rgba, uv (drawShape's attributes 0, 1, 7)
	std::vector<std::uint16_t> idx;
	int red = 0;                   //!< gates only: 1 Tex / Tex2 swapped, 2 LOAD instead of CLEAR, 4 the vertex block's
	                               //!< uvOffset.y read 4 bytes early, 8 every layer's colour mask RGBA
};
//! one layer's mesh copied into the run (the caller then fills tex / tex2 / vs / ps / mask); false: past a budget
inline bool cloudMapAddMesh( CloudMapRun & t, const float * pos, const float * col, const float * uv, int nv,
	const std::uint16_t * idx, int nIdx )
{
	if ( t.layers >= kCloudMapLayers || nv <= 0 || nIdx <= 0 || int( t.verts.size() / 9 ) + nv > kCloudMapVerts
		|| int( t.idx.size() ) + nIdx > kCloudMapIndices )
		return false;
	CloudMapLayer & l = t.layer[t.layers];
	l = CloudMapLayer();
	l.firstVertex = int( t.verts.size() / 9 );
	l.firstIndex = int( t.idx.size() );
	l.nIdx = nIdx;
	for ( int k = 0; k < nv; k++ ) {
		const float v[9] = { pos[k * 3], pos[k * 3 + 1], pos[k * 3 + 2], col ? col[k * 4] : 1.0f, col ? col[k * 4 + 1] : 1.0f,
			col ? col[k * 4 + 2] : 1.0f, col ? col[k * 4 + 3] : 1.0f, uv ? uv[k * 2] : 0.0f, uv ? uv[k * 2 + 1] : 0.0f };
		t.verts.insert( t.verts.end(), v, v + 9 );
	}
	t.idx.insert( t.idx.end(), idx, idx + nIdx );
	t.layers++;
	return true;
}
/* GL current, cloudmap.prog's last layer state irrelevant: every layer drawn on Vulkan into the shared map. false: why;
 * nothing drawn (GL draws the layers). */
bool cloudMapRun( const CloudMapRun & t, std::string & why );
void cloudMapOnGl( const char * why );
FxStat takeCloudMapStat();
}  // namespace WwVkPresent
