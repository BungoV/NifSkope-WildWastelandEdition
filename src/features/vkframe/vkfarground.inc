/* Lane VKFAR3 (2026-11-14, plan row G3): the far ground's ring-0 terrain on the present device. fo4_cellcsmfgl (Show > Far
 * ground: Live, shader decals, the default) through VKFAR2's twinRun: fargnd_view_vs (fo4_default's twin) + fargnd_view_ps
 * (fo4_cellcsmfgl's readable source, FOGLEAK + FRAME_UBO), one command a shape. Included after vkwater.inc (its TwinSpec,
 * WaterDraw, twinRun). Gate: tests/vkframe/fargrounddraw.sh. */
#include "features/vkframe/fargnd_view_vs.spv.h"  // vkframe_spirv.sh's fxt / fxg lines
#include "features/vkframe/fargnd_view_ps.spv.h"

namespace
{
// the pixel stage's images (t n -> 64 + n, samplers at + 64) and the vertex stage's farStream (t 0 -> 200), reflected names
const WaterImgName kFargndImg[] = { { 64, "cellLights" }, { 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" },
	{ 72, "cellFarSlots" }, { 73, "cellFarRecs" }, { 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 80, "cellSsgi" }, { 81, "fgGrid" },
	{ 82, "fgLtex" }, { 83, "fgDecal" }, { 84, "fgAtlas" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 106, "cellAo" },
	{ 107, "cellSsr" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 112, "BaseMap" }, { 113, "NormalMap" }, { 114, "GlowMap" },
	{ 116, "SpecularMap" }, { 117, "GreyscaleMap" }, { 118, "EnvironmentMap" }, { 119, "CubeMap" }, { 120, "RefractionSrc" },
	{ 121, "landShoreIndex" }, { 122, "landShoreAtlas" }, { 123, "LandLodDiff" }, { 124, "LandLodOuter" }, { 125, "LandLodNoise" },
	{ 126, "LandLodDetail" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" }, { 79, "cellAoDecal" },
	{ 102, "waterFlowIndex" }, { 103, "waterFlowAtlas" }, { 115, "BacklightMap" }, { 200, "farStream" } };
// the integer images (usampler2D): fgGrid (RGBA32UI), the water info, flow and shore lookups
const std::uint32_t kFargndInts[] = { 81, 99, 100, 102, 103, 121, 122 };
// the pixel block (kT_fargndPsBytes 7104) ends at 19904: the constants after it, as the rings'
constexpr VkDeviceSize kFargndConstAt = 19968, kFargndHostBytes = 20224;
static_assert( kWaterPsLooseAt + WwVkPresent::kT_fargndPsBytes <= kFargndConstAt
		&& kFargndConstAt + 16 * WwVkPresent::kWaterAttribs <= kFargndHostBytes && WwVkPresent::kT_fargndPsBytes <= WwVkPresent::kTwinPsMax,
	"the far ground's host buffer layout" );
// rings: GL's polygon offset as the depth bias and the zero integer / colour cube views; the pixel stage's blocks b0 global, b1 shadow
const TwinSpec kFargndSpec { "far ground", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ), vkframe_fargnd_view_ps_spv,
	sizeof( vkframe_fargnd_view_ps_spv ), kFargndImg, sizeof( kFargndImg ) / sizeof( kFargndImg[0] ), kFargndInts,
	sizeof( kFargndInts ) / sizeof( kFargndInts[0] ), WwVkPresent::kT_fargndPsBytes, kFargndConstAt, kFargndHostBytes, true, 9, true };

WaterDraw & farGroundD()
{
	static WaterDraw D;
	return D;
}
}  // namespace

namespace WwVkPresent
{
static WaterStat & farGroundGlS()
{
	static WaterStat s;
	return s;
}

void farGroundOnGl( const char * why )
{
	WaterStat & s = farGroundGlS();
	s.gl++;
	s.glWhy = why ? why : "";
}

WaterStat takeFarGroundGlStat()
{
	WaterStat s = farGroundGlS();
	farGroundGlS() = WaterStat();
	return s;
}

std::string farGroundDrawRefusal()
{
	return farGroundD().broken ? farGroundD().brokenWhy : twinDrawRefusal();
}

bool farGroundRun( const WaterRun & r, std::string & why )
{
	return twinRun( farGroundD(), kFargndSpec, r, why );
}

FxStat takeFarGroundDrawStat()
{
	return takeTwinStat( farGroundD() );
}
}  // namespace WwVkPresent

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
void farGroundSay( bool wanted, const char * glWhy )
{
	static QString said;
	static double at = 0.0;
	const QString head = QStringLiteral( "vk farground: " );
	if ( !wanted ) {
		const QString line = head + QStringLiteral( "on GL (%1)" ).arg( QString::fromUtf8( glWhy ? glWhy : "" ) );
		if ( line != said ) {
			said = line;
			qInfo().noquote() << line;
		}
		return;
	}
	const double now = nowMs();
	if ( now - at < 5000.0 && said.startsWith( head + QStringLiteral( "wanted" ) ) )
		return;
	at = now;
	const WaterStat st = takeFarGroundGlStat();
	const FxStat ds = takeFarGroundDrawStat();
	QString line = head + QStringLiteral( "wanted on Vulkan, %1 draws run (fo4_cellcsmfgl twins, ring-0 terrain, into the tone frame; "
		"%2 ms CPU each, max %3, %4 copies, %5 fails%6); on GL %7 draws" )
		.arg( ds.runs ).arg( ds.runs ? ds.cpuMs / ds.runs : 0.0, 0, 'f', 3 ).arg( ds.maxMs, 0, 'f', 3 ).arg( ds.copies ).arg( ds.fails )
		.arg( ds.failWhy.empty() ? QString() : QStringLiteral( ": " ) + QString::fromStdString( ds.failWhy ) ).arg( st.gl );
	if ( !st.glWhy.empty() )
		line += QStringLiteral( " (refused: %1)" ).arg( QString::fromStdString( st.glWhy ) );
	said = line;
	qInfo().noquote() << line;
}
}  // namespace WwVkPresent
#endif
