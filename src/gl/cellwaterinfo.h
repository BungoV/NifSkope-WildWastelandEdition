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
 *  CLI: nifskope -no-gui lodl <file> --water-info <dir> writes the tables and the census (no window). */

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
	std::vector<quint32> bodyForms;	// lane WATERUNDER1: each body's WATR form (0 = none); not in the dump or bytes()
	int uniformDry = 0, uniformWet = 0, sparseBody = 0, sparseShore = 0, sparseSurface = 0, sparseDepth = 0;
	double buildSeconds = 0.0;

	quint64 bytes() const;
};

//! The packed depth code (0..255) of a depth in world units.
uint32_t wwWaterInfoDepthCode( double depth );
//! Builds the tables from a .lodl (pure CPU). False with the reason when the file has no water bodies or shore.
bool wwWaterInfoBuild( const QString & lodl, WwWaterInfoTables & t, QString * error );
//! The census lines of a build (counts and bytes), as the CLI and the gate print them.
QString wwWaterInfoCensus( const WwWaterInfoTables & t );
//! index.u32, atlas.u32, bodies.f32, census.txt into `dir`.
bool wwWaterInfoDump( const WwWaterInfoTables & t, const QString & dir, QString * error );

//! The cell document's worldspace .lodl ("" = none, an interior). Starts the build on a worker thread.
void wwCellWaterInfoSet( const void * nif, const QString & lodl );
//! With fo4_water.prog bound: uploads the tables once they are built and binds them at units unit..unit+2 when
//! the program reads them. Returns the units it used (0 or 3).
int wwCellWaterInfoUniforms( Scene * scene, int unit );
//! One census line: what this document's water info is and what was uploaded (read back from the GL).
QString wwCellWaterInfoEcho( const void * nif );

//! Lane WATERUNDER1: one texel of the lookup on the CPU (the same word water_info.glsl reads).
struct WwWaterInfoHit
{
	quint32 body = 0, form = 0;
	float height = 0.0f, depth = 0.0f, shore = 8160.0f;
	int cls = -1;
};
//! The baked water at world (x, y) for this document; false when its tables are not built (or the info red is off).
bool wwCellWaterInfoAt( const void * nif, float x, float y, WwWaterInfoHit & hit );

#endif
