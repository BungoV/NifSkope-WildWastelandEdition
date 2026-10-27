#ifndef CELLWATERINFO_H
#define CELLWATERINFO_H

#include <QString>

#include <cstdint>
#include <vector>

class Scene;

/*! Lane WATERDATA1 (2026-10-27): THE BAKED WATER PLANES FOR THE WATER SHADER.
 *
 *  The worldspace's .lodl (the far LOD bake, cellview.cpp cellFarLodl) carries per texel (128 u) a body id, the
 *  shore distance (32 u steps, 255 = 8160 u or more) and a surface delta, and a 48-byte record per body. This
 *  module packs them for fo4_water.prog (res/shaders/water_info.glsl, wwWaterInfo) as one sparse layout:
 *    - texel word: body id | shore steps << 16 | depth code << 24, depth = body height + surface delta - ground,
 *      code = sqrt(depth / 8192) * 255 (0 on dry texels);
 *    - an index of 16 x 16-texel tiles (RG32UI): slot + 1 (0 = uniform) and the uniform tile's word. A tile is
 *      uniform when its words agree and, if wet, its depths span <= 64 u (it stores the mean);
 *    - an atlas of the other tiles (R32UI, 64 tiles a row), and the body table (RGBA32F, 256 bodies a row:
 *      height, class, flags, area; body 0 = none, class -1).
 *  Nothing reads the lookup in the default program (its only reader is the WW_WATER_INFO_PROBE build, probes
 *  28..31), so the picture is unchanged. Later lanes (shore, depth, underwater) build their looks on it.
 *  Harness only:
 *    WW_CELL_WATER_INFO_RED=off      the lookup answers its neutral values (8160 u shore, depth 0, body 0, class -1)
 *    WW_CELL_WATER_INFO_PROBE=1      (launch) the probes' build of fo4_water: WW_CELL_WATER_PROBE=28 shore,
 *                                    29 depth, 30 body id, 31 class
 *  CLI: nifskope -no-gui lodl <file> --water-info <dir> writes the tables and the census (no window).
 *
 *  Lane WATERFLOWBAKE1 adds the FLOW tables (wwWaterFlow in water_info.glsl, probe 32 in the probe build): the
 *  .lodl flow plane decoded to a velocity per texel, the same sparse layout. WW_CELL_WATER_INFO_RED=off gives
 *  (0, 0) there too. Nothing in the default program reads it. */

struct WwWaterInfoTables
{
	static constexpr int kTileEdge = 16;
	static constexpr int kTilesPerRow = 64;
	static constexpr int kBodiesPerRow = 256;
	static constexpr double kDepthMax = 8192.0;
	static constexpr double kSpread = 64.0;

	QString path;
	int tilesX = 0, tilesY = 0;
	float originX = 0.0f, originY = 0.0f, texel = 0.0f, shoreQuantum = 32.0f;
	std::vector<uint32_t> index;	// tilesY rows of tilesX (slot + 1, uniform word)
	std::vector<uint32_t> atlas;	// atlasH rows of atlasW words
	int atlasW = 0, atlasH = 0, slotCount = 0;	// not "slots": a Qt keyword macro
	std::vector<float> bodies;	// bodyRows rows of kBodiesPerRow texels, 4 floats each
	int bodyCount = 0, bodyRows = 0;
	int uniformDry = 0, uniformWet = 0, sparseBody = 0, sparseShore = 0, sparseSurface = 0, sparseDepth = 0;
	double buildSeconds = 0.0;

	/* Lane WATERWETBAKE1: the shore plane's LAND side (a dry texel's distance to the nearest wet texel, 32 u
	 * steps, 255 = far, 0 on wet texels) for the terrain shaders (res/shaders/land_shore.glsl). The water words
	 * above take 255 on every dry texel, so they are the same with or without the land side in the file. */
	std::vector<uint32_t> landIndex;	// tilesY rows of tilesX: the uniform tile's byte, or (slot + 1) << 8
	std::vector<uint8_t> landAtlas;	// landAtlasH rows of atlasW bytes, 64 tiles a row
	int landAtlasH = 0, landSlots = 0, landUniformFar = 0, landUniformWet = 0, landUniformOther = 0;
	qint64 landNearTexels = 0;	// dry texels under 255

	/* Lane WATERFLOWBAKE1: the flow plane as a velocity, same tile layout at the flow plane's own rate. Texel
	 * word = vx | vy << 16, each a signed 16-bit count of 1/1024 world units a second (+-32 u/s); 0 = still or
	 * dry. A confidence-0 word (the body's constant) decodes to the body's exact mean flow; any other to
	 * direction x speed code x |mean| / 8. A tile is uniform when its words are equal (exact). */
	static constexpr double kFlowScale = 1024.0;
	int flowTilesX = 0, flowTilesY = 0;	// 0 = the file has no flow plane
	float flowTexel = 0.0f;
	std::vector<uint32_t> flowIndex;	// flowTilesY rows of flowTilesX (slot + 1, uniform word)
	std::vector<uint32_t> flowAtlas;	// flowAtlasH rows of flowAtlasW words
	int flowAtlasW = 0, flowAtlasH = 0, flowSlotCount = 0;
	int flowUniformStill = 0, flowUniformMoving = 0;
	qint64 flowMovingTexels = 0, flowSolvedTexels = 0;

	/* Lane WATERSKYVISFAR1: the .lodl sky plane (version 4) for the far water's sky reflection, laid out as the land
	 * side: index R32UI = the uniform tile's byte, or (slot + 1) << 8; atlas R8UI, 64 tiles a row, atlasW wide.
	 * Byte 0 = dry, else 1 + round(vis * 254). haveSky false = no sky plane in the file: the shader reads 1. */
	bool haveSky = false;
	std::vector<uint32_t> skyIndex;	// tilesY rows of tilesX
	std::vector<uint8_t> skyAtlas;	// skyAtlasH rows of atlasW bytes
	int skyAtlasH = 0, skySlots = 0, skyUniformOpen = 0, skyUniformDry = 0, skyUniformOther = 0;
	qint64 skyWetTexels = 0, skyBelowOpen = 0;

	quint64 bytes() const;	// the water tables
	quint64 landBytes() const;
	quint64 flowBytes() const;
	quint64 skyBytes() const;
};

//! A velocity (world units a second) as the flow table's texel word.
uint32_t wwWaterFlowPack( double vx, double vy );
//! The flow census line (counts and bytes).
QString wwWaterFlowCensus( const WwWaterInfoTables & t );

//! The packed depth code (0..255) of a depth in world units.
uint32_t wwWaterInfoDepthCode( double depth );
//! Builds the tables from a .lodl (pure CPU). False with the reason when the file has no water bodies or shore.
bool wwWaterInfoBuild( const QString & lodl, WwWaterInfoTables & t, QString * error );
//! The census lines of a build (counts and bytes), as the CLI and the gate print them.
QString wwWaterInfoCensus( const WwWaterInfoTables & t );
//! The land side's census line (lane WATERWETBAKE1).
QString wwLandShoreCensus( const WwWaterInfoTables & t );
//! The sky plane's census line (lane WATERSKYVISFAR1).
QString wwWaterSkyCensus( const WwWaterInfoTables & t );
//! index.u32, atlas.u32, bodies.f32, census.txt (+ WATERFLOWBAKE1 flowindex.u32, flowatlas.u32), and the
//! land side's land_index.u32, land_atlas.u8, land_census.txt (+ WATERSKYVISFAR1 sky_index.u32, sky_atlas.u8,
//! sky_census.txt when the file has a sky plane) into `dir`.
bool wwWaterInfoDump( const WwWaterInfoTables & t, const QString & dir, QString * error );

//! The cell document's worldspace .lodl ("" = none, an interior). Starts the build on a worker thread.
void wwCellWaterInfoSet( const void * nif, const QString & lodl );
//! With fo4_water.prog bound: uploads the tables once they are built and binds them at units unit..unit+2 when
//! the program reads them, and the flow tables at the next two units when it reads waterFlowIndex (lane
//! WATERFLOWBAKE1). Returns the units it used (0, 2, 3 or 5). Lane WATERSKYVISFAR1: the sky plane at
//! WwCellUnit::WaterSkyIndex/WaterSkyAtlas when the program reads waterSkyIndex (fo4_waterlook only); red
//! WW_CELL_WATER_SKYFAR_RED=one: the far water's sky visibility reads 1 (the bytes before the lane).
int wwCellWaterInfoUniforms( Scene * scene, int unit );
/*! Lane WATERWETBAKE1: with a cell program bound (fo4_cell*, pbrm_cell* family), binds the land side at
 *  WwCellUnit::LandShoreIndex/LandShoreAtlas when the program reads `landShoreIndex` (nothing reads it today:
 *  the default programs drop the uniforms, and the call returns at once). `terrain` = the drawn shape is land
 *  (near LAND or far terrain), uploaded as `landShoreTerrain`. Red WW_CELL_LAND_SHORE_RED=off: neutral (far). */
void wwCellLandShoreUniforms( Scene * scene, bool terrain );
//! One census line: what this document's water info is and what was uploaded (read back from the GL).
QString wwCellWaterInfoEcho( const void * nif );

#endif
