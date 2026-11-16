/* Lane VKGROUND11 (2026-11-16, plan notes/vkground9/next.md step 2): the far ground's ring-0 terrain (mode 1, fo4_cellcsmfgl,
 * shader decals) under the RT GI keys on the present device: fxWork kind 23 the all-on key (+ RESTIR + RTDDGI + RTDDGI_CASCADES),
 * kind 24 the Ray-traced GI row with no RT preset (+ RTDDGI + RTDDGI_CASCADES), from vkframe_spirv.sh's fxr / fxdg lines.
 * Vertex stage fargnd_view_vs (t at 200); the pixel blocks b0 global, b1 shadow, b31 from vkfargroundrt_tables.h. One crossing
 * a shape (no queue: the caller flushes the base twin's queue first). Included after vkfarlodrt.inc. Gate:
 * tests/vkframe/fargndrtdraw.sh. Lane VKGROUND18: mode 2 (fo4_cellcsmfgp, cached pages) the same way, kinds 25 (RT) / 26 (DG). */
#define WW_VK_FARGNDRT 1
#define WW_VK_FARGNDPRT 1
#include "features/vkframe/fargnd_view_rt_ps.spv.h"  // vkframe_spirv.sh's fxr / fxdg lines (lane VKGROUND11)
#include "features/vkframe/fargnd_view_dg_ps.spv.h"
#include "features/vkframe/fargndp_view_rt_ps.spv.h"  // lane VKGROUND18: mode 2's
#include "features/vkframe/fargndp_view_dg_ps.spv.h"

namespace
{
namespace farlod
{
// every DECLARED image of the shipped headers (notes/vkfar3/tools/spvh_binds.py): the base twin's census (kFargndImg) plus
// cellRestir 71 (RT only), cellGiHalf 104, cellGiHalfKey 105, cellDdgiAtlas 108; the vertex stage's farStream (t 0 -> 200)
const WaterImgName kFargndRtImg[] = { { 64, "cellLights" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" },
	{ 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 71, "cellRestir" }, { 72, "cellFarSlots" }, { 73, "cellFarRecs" },
	{ 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 79, "cellAoDecal" }, { 80, "cellSsgi" }, { 81, "fgGrid" }, { 82, "fgLtex" },
	{ 83, "fgDecal" }, { 84, "fgAtlas" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 102, "waterFlowIndex" },
	{ 103, "waterFlowAtlas" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 106, "cellAo" }, { 107, "cellSsr" },
	{ 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" }, { 112, "landShoreAtlas" },
	{ 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 117, "BacklightMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 200, "farStream" } };
// the DG key reads no cellRestir (71)
const WaterImgName kFargndDgImg[] = { { 64, "cellLights" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" },
	{ 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 72, "cellFarSlots" }, { 73, "cellFarRecs" },
	{ 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 79, "cellAoDecal" }, { 80, "cellSsgi" }, { 81, "fgGrid" }, { 82, "fgLtex" },
	{ 83, "fgDecal" }, { 84, "fgAtlas" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 102, "waterFlowIndex" },
	{ 103, "waterFlowAtlas" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 106, "cellAo" }, { 107, "cellSsr" },
	{ 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" }, { 112, "landShoreAtlas" },
	{ 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 117, "BacklightMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 200, "farStream" } };
// the pixel blocks (RT 7504, DG 7472 bytes) end before 20480: the constants after them
constexpr VkDeviceSize kFargndRtConstAt = 20480, kFargndRtHostBytes = 20736;
static_assert( kWaterPsLooseAt + WwVkPresent::kT_fargndRtPsBytes <= kFargndRtConstAt
		&& kWaterPsLooseAt + WwVkPresent::kT_fargndDgPsBytes <= kFargndRtConstAt
		&& kFargndRtConstAt + 16 * WwVkPresent::kWaterAttribs <= kFargndRtHostBytes
		&& WwVkPresent::kT_fargndRtPsBytes <= WwVkPresent::kTwinPsMax && WwVkPresent::kT_fargndDgPsBytes <= WwVkPresent::kTwinPsMax,
	"the far ground RT twins' host buffer layout" );
// the integer images: the base twin's (fgGrid, the water info, flow and shore lookups)
const TwinSpec kFargndRtSpec { "far ground RT", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ),
	vkframe_fargnd_view_rt_ps_spv, sizeof( vkframe_fargnd_view_rt_ps_spv ), kFargndRtImg, sizeof( kFargndRtImg ) / sizeof( kFargndRtImg[0] ),
	kFargndInts, sizeof( kFargndInts ) / sizeof( kFargndInts[0] ), WwVkPresent::kT_fargndRtPsBytes, kFargndRtConstAt, kFargndRtHostBytes,
	true, 23, true };
const TwinSpec kFargndDgSpec { "far ground DG", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ),
	vkframe_fargnd_view_dg_ps_spv, sizeof( vkframe_fargnd_view_dg_ps_spv ), kFargndDgImg, sizeof( kFargndDgImg ) / sizeof( kFargndDgImg[0] ),
	kFargndInts, sizeof( kFargndInts ) / sizeof( kFargndInts[0] ), WwVkPresent::kT_fargndDgPsBytes, kFargndRtConstAt, kFargndRtHostBytes,
	true, 24, true };
// lane VKGROUND18: mode 2's RT / DG twins declare mode 2's base census (kFargndpImg) plus cellRestir 71 (RT only),
// cellGiHalf 104, cellGiHalfKey 105, cellDdgiAtlas 108 (spvh_binds of both headers)
const WaterImgName kFargndpRtImg[] = { { 64, "cellLights" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" },
	{ 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 71, "cellRestir" }, { 72, "cellFarSlots" }, { 73, "cellFarRecs" },
	{ 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 79, "cellAoDecal" }, { 80, "cellSsgi" }, { 81, "fgGrid" }, { 82, "fgLtex" },
	{ 86, "fgCells" }, { 87, "fgPagesN" }, { 88, "fgPagesF" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 102, "waterFlowIndex" },
	{ 103, "waterFlowAtlas" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 106, "cellAo" }, { 107, "cellSsr" },
	{ 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" }, { 112, "landShoreAtlas" },
	{ 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 117, "BacklightMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 200, "farStream" } };
const WaterImgName kFargndpDgImg[] = { { 64, "cellLights" }, { 65, "cellGi" }, { 66, "cellGiSlots" }, { 67, "cellRooms" },
	{ 68, "cellIsLut" }, { 69, "cellIsBloom" }, { 70, "cellShadow" }, { 72, "cellFarSlots" }, { 73, "cellFarRecs" },
	{ 74, "cellFarPlaced" }, { 77, "cellCtGbuf" }, { 79, "cellAoDecal" }, { 80, "cellSsgi" }, { 81, "fgGrid" }, { 82, "fgLtex" },
	{ 86, "fgCells" }, { 87, "fgPagesN" }, { 88, "fgPagesF" }, { 89, "atApTex" }, { 90, "volTex" }, { 91, "volTile" }, { 92, "volAirTerr" },
	{ 93, "volAirRainMap" }, { 94, "volAirNoise" }, { 95, "csmMap" }, { 96, "ussTerrMap" }, { 97, "ussContactMap" },
	{ 98, "ussCloudMap" }, { 99, "waterInfoIndex" }, { 100, "waterInfoAtlas" }, { 101, "waterInfoBodies" }, { 102, "waterFlowIndex" },
	{ 103, "waterFlowAtlas" }, { 104, "cellGiHalf" }, { 105, "cellGiHalfKey" }, { 106, "cellAo" }, { 107, "cellSsr" },
	{ 108, "cellDdgiAtlas" }, { 109, "cellCluster" }, { 110, "rainCover" }, { 111, "landShoreIndex" }, { 112, "landShoreAtlas" },
	{ 114, "BaseMap" }, { 115, "NormalMap" }, { 116, "GlowMap" }, { 117, "BacklightMap" }, { 118, "SpecularMap" },
	{ 119, "GreyscaleMap" }, { 120, "EnvironmentMap" }, { 121, "CubeMap" }, { 122, "RefractionSrc" }, { 123, "LandLodDiff" },
	{ 124, "LandLodOuter" }, { 125, "LandLodNoise" }, { 126, "LandLodDetail" }, { 127, "farSkyMap" }, { 200, "farStream" } };
static_assert( kWaterPsLooseAt + WwVkPresent::kT_fargndpRtPsBytes <= kFargndRtConstAt
		&& kWaterPsLooseAt + WwVkPresent::kT_fargndpDgPsBytes <= kFargndRtConstAt
		&& WwVkPresent::kT_fargndpRtPsBytes <= WwVkPresent::kTwinPsMax && WwVkPresent::kT_fargndpDgPsBytes <= WwVkPresent::kTwinPsMax,
	"far ground mode 2's RT twins in the RT host buffer layout" );
// mode 2's integer images (kFargndpInts: + the cell table fgCells)
const TwinSpec kFargndpRtSpec { "far ground pages RT", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ),
	vkframe_fargndp_view_rt_ps_spv, sizeof( vkframe_fargndp_view_rt_ps_spv ), kFargndpRtImg, sizeof( kFargndpRtImg ) / sizeof( kFargndpRtImg[0] ),
	kFargndpInts, sizeof( kFargndpInts ) / sizeof( kFargndpInts[0] ), WwVkPresent::kT_fargndpRtPsBytes, kFargndRtConstAt, kFargndRtHostBytes,
	true, 25, true };
const TwinSpec kFargndpDgSpec { "far ground pages DG", vkframe_fargnd_view_vs_spv, sizeof( vkframe_fargnd_view_vs_spv ),
	vkframe_fargndp_view_dg_ps_spv, sizeof( vkframe_fargndp_view_dg_ps_spv ), kFargndpDgImg, sizeof( kFargndpDgImg ) / sizeof( kFargndpDgImg[0] ),
	kFargndpInts, sizeof( kFargndpInts ) / sizeof( kFargndpInts[0] ), WwVkPresent::kT_fargndpDgPsBytes, kFargndRtConstAt, kFargndRtHostBytes,
	true, 26, true };

WaterDraw & farGroundRtD()
{
	static WaterDraw D;
	return D;
}

WaterDraw & farGroundDgD()
{
	static WaterDraw D;
	return D;
}

//! v 1: the RT (all-on) twin, 2: the DG twin
WaterDraw & farGroundRtOf( int v )
{
	return v == 2 ? farGroundDgD() : farGroundRtD();
}

WaterDraw & farGroundPRtD()
{
	static WaterDraw D;
	return D;
}

WaterDraw & farGroundPDgD()
{
	static WaterDraw D;
	return D;
}

//! lane VKGROUND18: mode 2's, v 1: the RT (all-on) twin, 2: the DG twin
WaterDraw & farGroundLiveRtOf( int v )
{
	return v == 2 ? farGroundPDgD() : farGroundPRtD();
}
}  // namespace farlod
}  // namespace

namespace WwVkPresent
{
std::string farGroundRtDrawRefusal( int v )
{
	farlod::WaterDraw & D = farlod::farGroundRtOf( v );
	return D.broken ? D.brokenWhy : farlodrun::twinDrawRefusal();
}

/* as the rings' farLodRtRun (lanes VKFAR27 / VKFAR31): while GL's program reads the half-rate screen GI (cellGiHalfOn 1),
 * the RT / DG twins read cellGiHalf / cellGiHalfKey in place, VKBUF2's shared images found by name (vkrtbuf.h rtImgOf): each
 * bound GL name must be the shared one, else the draw stays on GL by name. Empty: none refused (both modes, lane VKGROUND18) */
static std::string farGroundGiHalfRefusal( const WaterRun & r, const FxField * fs, int nf )
{
	for ( int i = 0; i < nf; i++ ) {
		if ( std::strcmp( fs[i].name, "cellGiHalfOn" ) != 0 )
			continue;
		std::int32_t on = 0;
		if ( fs[i].offset + 4 <= r.psBytes )
			std::memcpy( &on, r.psLoose + fs[i].offset, 4 );
		if ( on == 0 )
			continue;
		for ( const char * nm : { "cellGiHalf", "cellGiHalfKey" } ) {
			RtImg img;
			std::string iw;
			if ( !rtImgOf( nm, img, iw ) )
				return std::string( nm ) + ": " + iw;
			const WaterSlot * s = nullptr;
			for ( const WaterSlot & x : r.census.inputs )
				if ( x.name == nm )
					s = &x;
			if ( !s )
				return std::string( nm ) + ": GL's program has no such sampler";
			if ( s->tex != img.glTex )
				return std::string( nm ) + ": the bound texture is not the shared image of that name";
			if ( farSharedOf( s->tex ) != img.key )
				return std::string( nm ) + ": the shared image is not the bound name's";
		}
	}
	return std::string();
}

bool farGroundRtRun( const WaterRun & r, int v, std::string & why )
{
	if ( v != 1 && v != 2 ) {
		why = "no far ground RT twin " + std::to_string( v ) + " (1 RT, 2 DG)";
		return false;
	}
	const FxField * fs = v == 2 ? kT_fargndDgPs : kT_fargndRtPs;
	const int nf = v == 2 ? int( sizeof( kT_fargndDgPs ) / sizeof( kT_fargndDgPs[0] ) ) : int( sizeof( kT_fargndRtPs ) / sizeof( kT_fargndRtPs[0] ) );
	if ( const std::string w = farGroundGiHalfRefusal( r, fs, nf ); !w.empty() ) {
		FxStat & S = farlod::farGroundRtOf( v ).st;
		S.fails++;
		S.failWhy = why = "cellGiHalfOn 1: " + w;
		return false;
	}
	return farlodrun::twinRun( farlod::farGroundRtOf( v ), v == 2 ? farlod::kFargndDgSpec : farlod::kFargndRtSpec, r, why );
}

FxStat takeFarGroundRtDrawStat( int v )
{
	return farlodrun::takeTwinStat( farlod::farGroundRtOf( v ) );
}

// lane VKGROUND18: mode 2 (fo4_cellcsmfgp, cached pages) under the RT GI keys, one crossing a shape after mode 1's queue
std::string farGroundLiveRtDrawRefusal( int v )
{
	farlod::WaterDraw & D = farlod::farGroundLiveRtOf( v );
	return D.broken ? D.brokenWhy : farlodrun::twinDrawRefusal();
}

bool farGroundLiveRtRun( const WaterRun & r, int v, std::string & why )
{
	if ( v != 1 && v != 2 ) {
		why = "no far ground mode 2 RT twin " + std::to_string( v ) + " (1 RT, 2 DG)";
		return false;
	}
	if ( !farGroundFlush( why ) )	// mode 1's queued tiles draw first (GL's order)
		return false;
	const FxField * fs = v == 2 ? kT_fargndpDgPs : kT_fargndpRtPs;
	const int nf = v == 2 ? int( sizeof( kT_fargndpDgPs ) / sizeof( kT_fargndpDgPs[0] ) ) : int( sizeof( kT_fargndpRtPs ) / sizeof( kT_fargndpRtPs[0] ) );
	if ( const std::string w = farGroundGiHalfRefusal( r, fs, nf ); !w.empty() ) {
		FxStat & S = farlod::farGroundLiveRtOf( v ).st;
		S.fails++;
		S.failWhy = why = "cellGiHalfOn 1: " + w;
		return false;
	}
	return farlodrun::twinRun( farlod::farGroundLiveRtOf( v ), v == 2 ? farlod::kFargndpDgSpec : farlod::kFargndpRtSpec, r, why );
}

FxStat takeFarGroundLiveRtDrawStat( int v )
{
	return farlodrun::takeTwinStat( farlod::farGroundLiveRtOf( v ) );
}
}  // namespace WwVkPresent
