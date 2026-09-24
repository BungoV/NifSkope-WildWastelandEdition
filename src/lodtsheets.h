/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODTSHEETS_H
#define LODTSHEETS_H

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

/*! The `.lodt` terrain-texture pyramid, seen from the VIEWER.
 *
 *  `src/io/lodvfile.h` writes and validates the container; it has no tile
 *  PAYLOAD reader, because until now nothing opened one. This file is that
 *  reader, and it is a reader only: it never writes a `.lodt`, never touches a
 *  bake output and never changes one byte of what the generator produced.
 *
 *  THE DECODE STEP, AND WHY IT EXISTS. The renderer takes textures by NAME and
 *  resolves the name through the resource stack (BSAs, then loose folders) --
 *  `TexCache::find` -> `NifModel::getResourceFile` -> `GameManager`. An
 *  absolute path is not a way in: `GameManager::get_full_path` lowercases the
 *  name, turns the separators round and prefixes `textures/`, so `E:/x/y.dds`
 *  arrives as `textures/e:/x/y.dds` and misses. A `.lodt` tile is therefore
 *  UNPACKED to a loose `.DDS` under a cache directory, and that directory is
 *  pushed onto the session's Fallout 4 folder list the same way
 *  `WW_LODGEN_RESOURCES` does it in `src/main.cpp` -- session only, never
 *  written to QSettings.
 *
 *  The cache directory is `WW_LODL_SHEET_CACHE` when set, otherwise a
 *  `nifskope_ww_lodl_sheets` folder under the system temporary directory. The
 *  files inside it are named after the container, the tile and the sheet role,
 *  so two levels and two worldspaces cannot collide.
 */

struct LodtSheetTile
{
	//! Texture names as the renderer will be given them, `Textures\...`-relative
	//! and resolvable against the cache directory. Empty when the tile is absent.
	QString colour;
	QString msn;
	//! The EMISSIVE sheet (role 6), empty when the container carries none.
	//! Unpacked for WW_LODL_CHANNEL=emissive only -- nothing else binds it.
	QString emissive;
};

/*! One LEVEL of one worldspace's pyramid, opened and ready to hand out tiles.
 *
 *  Cells and tiles: tile (tx, ty) covers cells x in
 *  `[west + tx*levelDim, west + (tx+1)*levelDim - 1]` and cells y in
 *  `[north - (ty+1)*levelDim + 1, north - ty*levelDim]` -- ty = 0 is the NORTH
 *  row, which is the container's own rule (docs/LODGEN_TERRAIN_VT.md 2.2) and
 *  the opposite of the `.lodl`'s row-0-SOUTH order.
 */
class LodtSheets
{
public:
	LodtSheets();
	~LodtSheets();

	/*! Find and open the pyramid for a `.lodl`.
	 *
	 *  Looked for in `WW_LODL_SHEETS` when that names a directory, otherwise
	 *  beside the `.lodl` itself: `<worldspace>.VT.<dim>.lodt`. When several
	 *  levels are present the FINEST (smallest `dim`) is taken, unless
	 *  `WW_LODL_SHEET_DIM` names one.
	 *
	 *  Returns false and leaves `*why` saying which of those steps came up
	 *  empty -- "no sheets" is a normal answer here, not a defect, and the
	 *  caller falls back to the inline-colour data view. */
	bool open( const QString & lodlPath, QString * why );

	/*! The same open, but for the level that carries a ROLE (lane HORIZON1).
	 *
	 *  The horizon sheets (role 7) are authored on ONE level of the pyramid --
	 *  the coarsest whose texel is no wider than the bake's `--vt-horizon-texel`
	 *  -- so opening "the finest level" and asking for role 7 would find nothing
	 *  and say "absent" about a container that has it. This picks the FINEST
	 *  level that actually carries the role, ignores `WW_LODL_SHEET_DIM` (the
	 *  horizon lives where the bake put it, not where a dial points), and
	 *  refuses by name when no level has it. */
	bool openForRole( const QString & lodlPath, int role, QString * why );

	bool isOpen() const;

	int levelDim() const;
	int tilesX() const;
	int tilesY() const;
	//! The padded cell rectangle this level covers, inclusive.
	int west() const;
	int south() const;
	int north() const;
	int east() const;
	int contentTexels() const;
	int borderTexels() const;
	int storedTexels() const;
	QString containerPath() const;
	QString cacheDir() const;

	//! The tile holding a cell, or false when the cell is outside this level.
	bool tileOfCell( int cellX, int cellY, int * tx, int * ty ) const;

	/*! Unpack tile (tx, ty)'s colour and `_msn` sheets to loose DDS (once per
	 *  session; a file already on disk is reused) and return their names.
	 *  False with `*why` when the tile is absent or the unpack failed. */
	bool tile( int tx, int ty, LodtSheetTile & out, QString * why );

	//! One `u` or `v` in [0,1] over a tile's CONTENT maps to this range of the
	//! STORED sheet: `uvBias + t * uvScale`, i.e. border/stored and content/stored.
	float uvBias() const;
	float uvScale() const;

	/*! The mask sheet's B -- the terrain's sky AO per texel, with the placed
	 *  objects' occlusion folded in when the bake ran --terrain-object-ao
	 *  (docs/LODGEN_TERRAIN_VT.md, role 5 RMAOS) -- for tile (tx, ty), decoded
	 *  from mip 0 into `storedTexels() x storedTexels()` bytes, row 0 the top
	 *  of the sheet as the DDS stores it. 255 = open sky. False with `*why`
	 *  when the container has no mask sheet, the tile is absent, or the
	 *  format is not BC1/BC3. Bytes only, nothing written to the cache.
	 *  (bungo 2026-09-18, "why is the terrain AO so low res?": the AO view
	 *  had read the 8-a-cell .lodl plane; the texture is the AO.) */
	bool maskAo( int tx, int ty, std::vector<quint8> & out, QString * why );

	/*! THE SAME SAMPLER, one channel of one sheet ROLE (lane CHANVIEW1, for
	 *  WW_LODL_CHANNEL). `maskAo` is exactly `sheetChannel( LODV_ROLE_MASK, tx,
	 *  ty, 2, ... )` and calls it, so the AO read and every other channel read
	 *  share one decoder and cannot drift apart (the brief's "terrain sheets are
	 *  read through the existing sampler; no second decoder").
	 *
	 *  `channel` is 0 = R, 1 = G, 2 = B, 3 = A. A BC1 tile has no alpha and the
	 *  call REFUSES with that in `*why` rather than inventing one: on the mask
	 *  sheet, A is the ground cover and only a COVER tile (BC3) carries it.
	 *  `role` is one of the `LODV_ROLE_*` values; a container without that role
	 *  refuses by name too.
	 *
	 *  Decoded from mip 0 into `storedTexels() x storedTexels()` bytes, row 0 the
	 *  top of the sheet as the DDS stores it. Bytes only, nothing written to the
	 *  cache. */
	/*! `occurrence` picks WHICH sheet of a repeated role (role 7, the horizon,
	 *  is the only one: four azimuth bins ride one RGBA sheet, so bin b is
	 *  occurrence `b / 4`, channel `b % 4`). 0 for every other role, where it
	 *  reads the one sheet that role has. Out of range refuses by name.
	 *
	 *  Role 7 is UNCOMPRESSED R8G8B8A8 and decodes here without a block
	 *  decoder -- deliberately the same function and not a second reader, so
	 *  "the sampler is one decoder" stays true across the new role. */
	bool sheetChannel( int role, int tx, int ty, int channel,
		std::vector<quint8> & out, QString * why, int occurrence = 0 );

	//! Does the container carry a sheet with this role at all? (`emissive` has to
	//! be able to say "absent" by name instead of drawing something else.)
	bool hasRole( int role ) const;

	//! How many sheets carry this role: 0 or 1 for every role but the horizon.
	int roleCount( int role ) const;

	//! What was measured, for the document's notes. Never a picture's only evidence.
	QStringList notes() const;

private:
	//! `open` and `openForRole` in one body; `wantRole` < 0 means "any level".
	bool openFiltered( const QString & lodlPath, int wantRole, QString * why );

	struct Impl;
	std::unique_ptr<Impl> d;
};

#endif // LODTSHEETS_H
