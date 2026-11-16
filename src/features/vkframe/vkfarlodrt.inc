/* Lane VKFAR26 (2026-11-16, plan notes/vkfar25/plan_bc.md B): the far LOD rings' RT GI twins on the present device. The
 * frozen farlod_view_ps is fo4_cellcsm's base key only; these are fo4_cellcsm's readable source under the RT keys
 * (vkframe_spirv.sh fxr / fxdg): fxWork kind 21 the all-on key (+ RESTIR + RTDDGI + RTDDGI_CASCADES), kind 22 the Ray-traced
 * GI row with no RT preset (+ RTDDGI + RTDDGI_CASCADES). Their images reach t 127 (64 + n), so the vertex stage is
 * fargnd_view_vs (t at 200), not farlod_view_vs (t at 120); the pixel blocks b0 global, b1 shadow (psGlobalB0), b31 from
 * vkfarlodrt_tables.h. Texel buffers (cellRestir, cellDdgiAtlas, as cellLights / farStream) take the rings' per-run copy
 * (farlodrun's twinTexelIn: the one place a shared texel-buffer lookup replaces). Included after vkfarground.inc. Gate:
 * tests/vkframe/farlodrtdraw.sh. */
#include "features/vkframe/farlod_view_rt_ps.spv.h"  // vkframe_spirv.sh's fxr / fxdg lines (lane VKFAR25)
#include "features/vkframe/farlod_view_dg_ps.spv.h"

namespace
{
namespace farlod
{
// the pixel stages' images (t n -> 64 + n, samplers at + 64) from the reflect (X:/ClaudeOverflow/vkfar25/probe rt.txt /
// dg.txt, notes/vkfar26/tools/imgtab.py) and the vertex stage's farStream (t 0 -> 200)
const WaterImgName kFarlodRtImg[] = {
	{ 64, "cellLights" }, { 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 71, "cellRestir" }, { 72, "cellFarSlots" },
	{ 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 80, "cellSsgi" }, { 89, "atApTex" },
	{ 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" }, { 93, "volAirRainMap" }, { 94, "volAirNoise" },
	{ 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" }, { 98, "ussCloudMap" }, { 99, "waterInfoIndex" },
	{ 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 106, "cellAo" },
	{ 107, "cellSsr" }, { 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" },
	{ 112, "landShoreAtlas" }, { 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 200, "farStream" } };
// the DG key keeps cellShadow (70) and reads no cellRestir
const WaterImgName kFarlodDgImg[] = {
	{ 64, "cellLights" }, { 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 72, "cellFarSlots" },
	{ 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 80, "cellSsgi" }, { 89, "atApTex" },
	{ 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" }, { 93, "volAirRainMap" }, { 94, "volAirNoise" },
	{ 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" }, { 98, "ussCloudMap" }, { 99, "waterInfoIndex" },
	{ 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 106, "cellAo" },
	{ 107, "cellSsr" }, { 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" },
	{ 112, "landShoreAtlas" }, { 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 200, "farStream" } };
// the integer images (usampler2D): the water info and shore lookups (cellCluster is a texel buffer, by name)
const std::uint32_t kFarlodRtInts[] = { 99, 100, 111, 112 };
// the pixel blocks (RT 7424, DG 7392 bytes) end before 20224: the constants after them
constexpr VkDeviceSize kFarlodRtConstAt = 20224, kFarlodRtHostBytes = 20480;
static_assert( kWaterPsLooseAt + WwVkPresent::kT_farlodRtPsBytes <= kFarlodRtConstAt
		&& kWaterPsLooseAt + WwVkPresent::kT_farlodDgPsBytes <= kFarlodRtConstAt
		&& kFarlodRtConstAt + 16 * WwVkPresent::kWaterAttribs <= kFarlodRtHostBytes
		&& WwVkPresent::kT_farlodRtPsBytes <= WwVkPresent::kTwinPsMax && WwVkPresent::kT_farlodDgPsBytes <= WwVkPresent::kTwinPsMax,
	"the far LOD RT twins' host buffer layout" );
const TwinSpec kFarlodRtSpec { "far LOD RT", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ), vkframe_farlod_view_rt_ps_spv,
	sizeof( vkframe_farlod_view_rt_ps_spv ), kFarlodRtImg, sizeof( kFarlodRtImg ) / sizeof( kFarlodRtImg[0] ), kFarlodRtInts,
	sizeof( kFarlodRtInts ) / sizeof( kFarlodRtInts[0] ), WwVkPresent::kT_farlodRtPsBytes, kFarlodRtConstAt, kFarlodRtHostBytes, true, 21, true };
const TwinSpec kFarlodDgSpec { "far LOD DG", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ), vkframe_farlod_view_dg_ps_spv,
	sizeof( vkframe_farlod_view_dg_ps_spv ), kFarlodDgImg, sizeof( kFarlodDgImg ) / sizeof( kFarlodDgImg[0] ), kFarlodRtInts,
	sizeof( kFarlodRtInts ) / sizeof( kFarlodRtInts[0] ), WwVkPresent::kT_farlodDgPsBytes, kFarlodRtConstAt, kFarlodRtHostBytes, true, 22, true };

WaterDraw & farLodRtD()
{
	static WaterDraw D;
	return D;
}

WaterDraw & farLodDgD()
{
	static WaterDraw D;
	return D;
}

//! v 1: the RT (all-on) twin, 2: the DG twin
WaterDraw & farLodRtOf( int v )
{
	return v == 2 ? farLodDgD() : farLodRtD();
}

const TwinSpec & farLodRtSpecOf( int v )
{
	return v == 2 ? kFarlodDgSpec : kFarlodRtSpec;
}
}  // namespace farlod
}  // namespace

namespace WwVkPresent
{
std::string farLodRtDrawRefusal( int v )
{
	farlod::WaterDraw & D = farlod::farLodRtOf( v );
	return D.broken ? D.brokenWhy : farlodrun::twinDrawRefusal();
}

bool farLodRtRun( const WaterRun & r, int v, std::string & why )
{
	if ( v != 1 && v != 2 ) {
		why = "no far LOD RT twin " + std::to_string( v ) + " (1 RT, 2 DG)";
		return false;
	}
	return farlodrun::twinRun( farlod::farLodRtOf( v ), farlod::farLodRtSpecOf( v ), r, why );
}

FxStat takeFarLodRtDrawStat( int v )
{
	return farlodrun::takeTwinStat( farlod::farLodRtOf( v ) );
}
}  // namespace WwVkPresent
