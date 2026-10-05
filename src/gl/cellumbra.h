#ifndef CELLUMBRA_H
#define CELLUMBRA_H

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

/*! Lane UMBRA1 (2026-10-05): THE PREVIS TOME'S VISIBILITY QUERY, IN THE VIEWER.
 *
 *  A worldspace block's previs file (vis/<plugin>/<RVIS>.uvd) is an Umbra 3.3.17 tome. Its layout and the game's
 *  query were read from the 1.10.155 runtime (Todd's treat); the spec and an independent Python reader live in the
 *  lane notes (notes/umbra1/re, notes/umbra1/umbra_query.py). Public pointers only: the middleware's published
 *  documentation describes tomes, cells, portals and gates in general terms.
 *
 *  THE TOME (every offset self-relative to its owner, 0 = absent): header +0x14/+0x20 AABB, +0x2c tree node
 *  count << 5, +0x30 tree bits, +0x3c tree splits, +0x40 objects, +0x44 object boxes (6 floats), +0x48 object
 *  distances (P xyz, near^2, Q xyz, far^2), +0x4c uid starts, +0x50 uids, +0x54 list widths, +0x58/+0x5c object
 *  lists, +0x68 gates, +0x70 gate boxes, +0x78 gate indices, +0x7c clusters, +0x88 cell starts, +0x8c leaf tiles,
 *  +0x90 tiles, +0xa0 tile offsets. A tile: box, KD tree (2 bits a node + rank table), leaf -> cell / BSP, cells
 *  (36 B: portals, objects, clusters, packed box), portals (16 B: face 31-29, outside 28, gate 27, hierarchy 26,
 *  target tile 25-0, +4 z, +6 target cell, +8 / +0xc packed min/max quanta of the tile box; a gate portal's +8 / +0xc
 *  give its gate list and its box).
 *
 *  THE QUERY: the camera's start cells (tome tree, tile tree, BSP), then the portal traversal: each portal not
 *  facing away (quantized camera), its box projected (D3D clip, z in [0, w]), rasterized into the 64 x 64 one-bit
 *  coverage of the cell it leads to, ANDed with the coverage it came through; then each reached cell's objects are
 *  tested: distance window, frustum planes, projected rectangle against the cell's coverage.
 *
 *  THIS IS A CONSERVATIVE RECONSTRUCTION, not the game's query op for op. Divergences (all can only ADD visible
 *  objects, never drop one the game shows, except where noted):
 *   - leaf tiles only (the game may walk coarser tiles far away; theirs are larger, so the game's set can be larger
 *     there -- the one place this may be SMALLER than the game's: nothing has measured it);
 *   - camera-point start (the game's +16 buffer zone is not modelled);
 *   - traversal to a fixpoint, no work budget (the game's order and budgets are not decoded);
 *   - all six planes always; an exact divide where the game uses an approximate reciprocal (a rect edge may move
 *     one pixel);
 *   - no depth form of the object test, no outside portals, no non-leaf targets;
 *   - every gate open (doors are not tracked);
 *   - each previs block is queried as its own tome (the game merges every loaded block into one collection; a
 *     camera outside a block's tome has no start cell there, and that block's refs fall back to the occluder path).
 *
 *  The code is written to the same float32 operation order as umbra_query.py so the two agree bit for bit (FMA
 *  contraction is switched off in cellumbra.cpp). */

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

/*! The query. `gateOpen` empty = every gate open. `flipFace` = the red control of the Python reader (one portal face
 *  flipped); the viewer never sets it. */
void wwUmbraQuery( const WwUmbraTome & t, const WwUmbraCamera & cam, const std::vector<char> & gateOpen,
	WwUmbraResult & out, bool flipFace = false );
//! The query file the Python `query` command recomputes (M, pos, gates, start, cell hashes, visible).
std::string wwUmbraQueryText( const WwUmbraTome & t, const WwUmbraCamera & cam, const std::vector<char> & gateOpen,
	const WwUmbraResult & r );
//! FNV-1a 64 over the 64 rows, each a little-endian u64.
std::uint64_t wwUmbraRasterHash( const WwUmbraRaster & r );

#endif
