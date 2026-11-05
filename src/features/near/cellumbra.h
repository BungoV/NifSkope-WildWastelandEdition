#ifndef CELLUMBRA_H
#define CELLUMBRA_H

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

/*! Decoder and visibility query for a worldspace block's previs file (vis/<plugin>/<RVIS>.uvd, an Umbra 3.x tome):
 *  start cells, portal traversal with 64 x 64 coverage rasters, then per-object tests. A conservative
 *  reconstruction (it can only add visible objects, except possibly far away where the game walks coarser tiles).
 *  Float32 order matches umbra_query.py bit for bit: keep FMA contraction off in cellumbra.cpp. */

struct WwUmbraPortal
{
	std::uint32_t w0 = 0, w8 = 0, wc = 0;
	std::uint16_t z = 0, target = 0;
};

struct WwUmbraCell
{
	std::uint32_t portal = 0, portalCount = 0;
	std::vector<std::uint32_t> objects;	//!< the run-length list, expanded
};

struct WwUmbraKd
{
	std::uint32_t n = 0, nwords = 0, words = 0, lutWords = 0;	//!< words / splits are byte offsets in the tome
	std::uint32_t splits = 0, numSplits = 0, midOff = 0, botOff = 0;
	bool present = false;
};

struct WwUmbraTile
{
	bool present = false, leaf = false;
	std::uint32_t off = 0;
	float mn[3] = { 0, 0, 0 }, mx[3] = { 0, 0, 0 }, pe = 0.0f;
	std::uint32_t numCells = 0;	//!< the tile's count (+0x34); `cells` is empty when the cell nodes are absent
	std::uint32_t nodeBits = 0, nodeData = 0, cellNodes = 0, bsp = 0, numBsp = 0, planes = 0;
	WwUmbraKd tree;
	std::vector<WwUmbraCell> cells;
	std::vector<WwUmbraPortal> portals;
};

struct WwUmbraTome
{
	QByteArray d;
	bool ok = false;
	QString error;
	float mn[3] = { 0, 0, 0 }, mx[3] = { 0, 0, 0 };
	std::uint32_t numObjects = 0, numClusters = 0, numTiles = 0, numLeaf = 0, numGates = 0, numCells = 0;
	std::uint32_t gateVert = 0, gateIdx = 0;
	std::vector<WwUmbraTile> tiles;
	std::vector<float> bounds;	//!< 6 a object
	std::vector<float> dist;	//!< 8 a object, empty = absent
	std::vector<std::uint32_t> uids;
	WwUmbraKd tree;

	//! Decodes `bytes`; false (and `error`) when anything is out of range or not a 3.x tome.
	bool load( const QByteArray & bytes );
	//! The structure text, line for line the Python `dump`.
	std::string dump() const;
};

struct WwUmbraCamera
{
	float M[16] = {};	//!< world -> D3D clip (z in [0, w]), row-major: clip = M * (x, y, z, 1)
	float pos[3] = { 0, 0, 0 };	//!< the camera, world
};

struct WwUmbraRaster
{
	std::uint64_t row[64];	//!< bit x of row[y] = pixel (x, y)
};

struct WwUmbraResult
{
	bool ok = false;
	QString error;
	std::vector<std::pair<int, int>> starts;	//!< (tile, cell), sorted
	std::vector<std::uint64_t> keys;		//!< (tile << 32 | cell) of each reached cell, sorted
	std::vector<WwUmbraRaster> rasters;	//!< parallel to keys
	std::vector<std::uint32_t> visible;	//!< object indices, sorted
	std::vector<char> objVisible;		//!< per object
	int portals = 0, entered = 0, rastered = 0, whole = 0;
};

/*! The query. `gateOpen` empty = every gate open. `flipFace` flips one portal face (debug control only). */
void wwUmbraQuery( const WwUmbraTome & t, const WwUmbraCamera & cam, const std::vector<char> & gateOpen,
	WwUmbraResult & out, bool flipFace = false );
//! The query file the Python `query` command recomputes (M, pos, gates, start, cell hashes, visible).
std::string wwUmbraQueryText( const WwUmbraTome & t, const WwUmbraCamera & cam, const std::vector<char> & gateOpen,
	const WwUmbraResult & r );
//! FNV-1a 64 over the 64 rows, each a little-endian u64.
std::uint64_t wwUmbraRasterHash( const WwUmbraRaster & r );

#endif
