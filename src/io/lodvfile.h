/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODVFILE_H
#define LODVFILE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

/*! `.lodt` v1 — one level of a terrain virtual texture: the terrain TEXTURE
 *  sheets, per level.
 *
 *  THE NAME MOVED (bungo's ruling, 2026-09-09). This container was written as
 *  `.lodv` until then, and `.lodt` belonged to the whole-worldspace LANDSCAPE
 *  file, which is `.lodl` now. `.lodt` is therefore REPURPOSED, not merely
 *  renamed, and that is why the magic changed with it: LODTEX_MAGIC is `LDTX`
 *  where the old container's was `LODV`, and the landscape file's is still
 *  `LODT`. Each of the three is refused BY NAME by the other reader, so a
 *  yesterday's file opened through today's route says what it actually is
 *  instead of misparsing or saying only "bad magic". The C++ names here --
 *  LodvWriter, lodvValidate, LODV_ROLE_* -- were NOT renamed with the format:
 *  they are internal and appear in no file on disk and in no command.
 *
 *  The format contract is docs/LODGEN_TERRAIN_VT.md; this header is the
 *  layout and nothing else. A container holds one LEVEL of the pyramid: a
 *  fixed grid of square tiles, each carrying the same set of sheets at the
 *  same size, indexed by a fixed-stride table so a consumer can seek to one
 *  tile without parsing anything.
 *
 *  Load-bearing, in the sense that changing any of it is a format break:
 *
 *   * little-endian throughout, ABSOLUTE 64-bit offsets, a fixed 24-byte
 *     table stride, and 64-bit table arithmetic. An int32 relative offset
 *     dies inside one worldspace: the Commonwealth's finest level is 1.19 GiB
 *     of payload before the height sheet is counted;
 *   * NORTH-UP row order, in the tile table and inside every payload. Two
 *     conventions are live in this codebase (.lodl is row-0-SOUTH) and the
 *     mismatch has already cost one consumer a Y mirror, so it is reader
 *     rule 19 — a refusal, not a hint;
 *   * a border that is a multiple of 4 AND still a multiple of 4 after
 *     mipCount-1 halvings. Break it and a BC block straddles the
 *     content/border line, which makes a tile's bytes depend on its
 *     neighbour's bake and kills incremental re-bake and byte identity both;
 *   * payloads in table-index order at 4,096-aligned offsets with ZERO pad
 *     bytes, and an absent entry's 24 bytes all zero. That is what makes two
 *     runs of the same bake byte-identical;
 *   * both corpus hashes, in the header and in the index. A stale pyramid
 *     against an edited plugin must be a NAMED refusal, exactly as a stale
 *     heightmap already is.
 *
 *  The magic is deliberately neither `DDS ` nor `LODT` (the landscape file's,
 *  which this extension used to name) nor `LODV` (this container's own, before
 *  the 2026-09-09 rename): a wrong-but-plausible parse is worse than a
 *  refusal. */

/*! The terrain texture container's magic, first four bytes, little-endian:
 *  `LDTX`. It is NEW as of 2026-09-09 and it is the whole reason the `.lodt`
 *  extension can be repurposed safely -- src/lodtfile.cpp refuses a file
 *  carrying it by name, and lodvValidate() refuses LODL_MAGIC by name. */
constexpr quint32 LODTEX_MAGIC = 0x5854444CU;   // 'L','D','T','X' little-endian
//! The retired `.lodv` magic, kept ONLY so a stale container is named, not guessed.
constexpr quint32 LODTEX_MAGIC_RETIRED_LODV = 0x56444F4CU;   // 'L','O','D','V'


/*! The container VERSION this build writes and the only one it accepts.
 *
 *  **2 since 2026-09-11** (bungo's ruling of 09:5x, verbatim: *"you can mirror
 *  how it's set up for the .lodm"* and *"we just add the coverage for whatever's
 *  missing in terrain textures that lod objects have in the texture
 *  department"*). Version 1 carried FOUR sheets whose third was role 3, `data`
 *  -- R sky AO, G flow wetness, B shore proximity, A ground cover. Version 2
 *  carries the OBJECT texture family instead: role 5 `mask` (RMAOS: R
 *  roughness, G metallic, B AO, A ground cover) and an optional role 6
 *  `emissive`, with shore proximity and wetness DROPPED (shore is a runtime
 *  subtraction from the `.lodl` water planes; wetness is a close-up effect and
 *  far wetness is a weather state).
 *
 *  **A v1 file is REFUSED, not converted.** No `.lodt` pyramid has ever been
 *  written to disk outside this tree -- the writer is opt-in behind `--vt` and
 *  bungo's installed `Data\Terrain` holds no `.lodt` (checked read-only,
 *  2026-09-11) -- so there is nothing in the world to convert, and a converter
 *  would be a second definition of channels that no longer mean the same thing.
 *  Role 3 keeps its number so a v1 file's refusal can NAME what it was. */
constexpr quint32 LODTEX_VERSION = 2;

//! What one sheet of a tile carries.
enum LodvSheetRole
{
	LODV_ROLE_UNUSED = 0,
	LODV_ROLE_COLOR = 1,        //!< RGB albedo, the grass tint folded in
	LODV_ROLE_MSN = 2,          //!< model-space normal, 0.5 + 0.5 encoded
	LODV_ROLE_DATA = 3,         //!< RETIRED with v1: R AO, G wetness, B shore, A cover. Refused in v2 BY NAME.
	LODV_ROLE_HEIGHT = 4,       //!< R16_UNORM, height/8 + 32767, as the shadow heightmap encodes it
	LODV_ROLE_MASK = 5,         //!< RMAOS: R roughness, G metallic, B sky AO, A ground cover
	LODV_ROLE_EMISSIVE = 6,     //!< RGB emissive colour, no alpha; ABSENT when no layer supplies one
	/*! The TERRAIN HORIZON (lane HORIZON1, 2026-09-18, docs/LODGEN_TERRAIN_VT.md
	 *  role 7). Four azimuth bins a sheet, one per channel in R,G,B,A order, each
	 *  byte the elevation of the highest thing the ground under that texel can
	 *  see in that azimuth: `degrees = byte / 255 * 90`. A receiver is lit when
	 *  the sun's elevation clears the two bins its azimuth falls between.
	 *
	 *  UNCOMPRESSED, R8G8B8A8_UNORM, and that is not laziness: BC1's endpoints
	 *  are RGB565, so a bin in R or B carries a 2.8-degree quantisation floor,
	 *  and 2.8 degrees of horizon error moves the shadow edge of a 1,000-unit
	 *  tower at a 10-degree sun by over 1,600 world units. The byte has to
	 *  survive intact, so the sheet is stored intact.
	 *
	 *  It is the ONE role a container may carry more than once: 16 bins is four
	 *  sheets. They are contiguous and last, and bin b lives in sheet
	 *  `b / 4`, channel `b % 4`. */
	LODV_ROLE_HORIZON = 7
};

//! Azimuth bins one horizon sheet carries -- R, G, B, A, in that order.
constexpr int LODV_HORIZON_BINS_PER_SHEET = 4;
//! Horizon sheets a container may carry, so at most 64 azimuth bins.
constexpr int LODV_HORIZON_MAX_SHEETS = 4;

/*! Sheets the header has room for. v1 held four at 0xA0..0xBF; v2 held six at
 *  0xA0..0xCF; since 2026-09-18 it holds TEN at 0xA0..0xEF and the reserved
 *  tail is 0xF0..0xFF.
 *
 *  Ten is not a round number, it is the largest set this version can produce:
 *  colour, msn, mask, height, emissive and four horizon sheets is nine, and the
 *  tenth slot is the one spare the 256-byte header can still pay for. The
 *  descriptor stride and the offsets of everything before 0xA0 did not move, so
 *  a container written before this line still reads -- its slots 6..9 are the
 *  zeroes the old writer left there, which is exactly what "past sheetCount"
 *  requires. */
constexpr int LODV_MAX_SHEETS = 10;

//! DXGI formats this version writes and accepts.
enum LodvDxgiFormat
{
	//! Uncompressed RGBA, four 8-bit channels. The HORIZON sheet only (role 7).
	LODV_DXGI_R8G8B8A8_UNORM = 28,
	LODV_DXGI_R16_UNORM = 56,
	LODV_DXGI_BC1_UNORM = 71,
	LODV_DXGI_BC1_UNORM_SRGB = 72,
	LODV_DXGI_BC3_UNORM = 77,
	LODV_DXGI_BC3_UNORM_SRGB = 78
};

enum LodvHeaderFlags
{
	LODV_FLAG_ROW_ORDER_NORTH_UP = 1,   //!< clear is a refusal; no other order is defined
	LODV_FLAG_FULL_MODE = 2             //!< the set's finest level is dim 1
};

enum LodvTileFlags
{
	LODV_TILE_PRESENT = 1,
	LODV_TILE_COVER = 2         //!< the MASK sheet uses dxgiFormatCover, cover in alpha
};

struct LodvSheetDesc
{
	quint16 dxgiFormat = 0;         //!< the format when the tile's COVER bit is CLEAR
	quint16 dxgiFormatCover = 0;    //!< and when it is SET; equal except on role 5 (mask)
	quint8 role = LODV_ROLE_UNUSED;
	quint8 colorSpace = 0;          //!< 0 linear, 1 sRGB
	/*! Descriptor byte 6 (lane VTNORMAL1, 2026-09-23; zero in every file
	 *  before it). How many of the tile's mips this sheet does NOT store, from
	 *  the top: 0 = all `mipCount` of them at `storedTexels`, 1 = a HALF-
	 *  resolution sheet whose first stored mip is `storedTexels / 2` and which
	 *  stores `mipCount - 1` of them. It is exactly the full sheet with mip 0
	 *  dropped, so the border stays a multiple of 4 by rule 12 and the colour
	 *  sheet (always 0) still sets the texel density. */
	quint8 mipSkip = 0;
};

//! Everything in the 256-byte header a writer decides. Offsets and sizes the
//! writer computes are not here.
struct LodvHeaderFields
{
	quint32 flags = LODV_FLAG_ROW_ORDER_NORTH_UP;
	quint64 vhgtCorpusHash = 0;
	quint64 paintCorpusHash = 0;
	QString worldspaceEdid;             //!< refused, never truncated, at 32 bytes
	qint16 south = 0, west = 0, north = 0, east = 0;             //!< padded, this level
	qint16 worldSouth = 0, worldWest = 0, worldNorth = 0, worldEast = 0;
	quint16 levelDim = 2;
	quint16 levelIndex = 0;
	quint16 levelCount = 1;
	quint16 tilesX = 0, tilesY = 0;
	quint16 contentTexels = 256;
	quint16 borderTexels = 8;
	quint16 storedTexels = 272;
	quint8 mipCount = 2;
	quint8 sheetCount = 4;
	quint8 anisoSupported = 8;
	quint8 compression = 0;             //!< 0 raw, 1 zlib (RFC 1950)
	float coverNormalisation = 96.0f;
	float tintStrength = 0.35f;
	quint16 levelDims[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	LodvSheetDesc sheets[LODV_MAX_SHEETS];
};

struct LodvTileEntry
{
	quint64 offset = 0;
	quint32 storedBytes = 0;
	quint32 rawBytes = 0;
	quint32 crc32 = 0;
	quint16 flags = 0;
	quint16 reserved = 0;
};

//! The raw payload size one tile holds, from the header fields and its COVER
//! bit. Sheets are concatenated sheet-major, mip-minor.
quint32 lodvTileRawBytes( const LodvHeaderFields & h, bool cover );
//! One sheet's bytes at one of ITS stored mips (0 = its first stored mip, which
//! is `storedTexels >> mipSkip` on a side); 0 past its last stored mip.
quint32 lodvSheetMipBytes( const LodvHeaderFields & h, int sheet, int mip, bool cover );
//! The side in texels of one sheet's stored mip `mip` (0 = its first stored one).
int lodvSheetSide( const LodvHeaderFields & h, int sheet, int mip );

//! CRC-32, zlib polynomial 0xEDB88320, the one both the header and the tiles use.
quint32 lodvCrc32( const unsigned char * p, qsizetype n, quint32 seed = 0 );

/*! The streaming writer. It never holds a level: buffering one to fill the
 *  table in place is 1.19 GiB at dim 2 before the height sheet. Instead the
 *  table's space is reserved, payloads are appended at increasing 4,096-aligned
 *  offsets in table-index order with only the 24-byte row kept in RAM, and the
 *  table, `fileBytes` and `indexCrc32` are patched at the end. */
class LodvWriter
{
public:
	LodvWriter();
	~LodvWriter();

	bool begin( const QString & path, const LodvHeaderFields & fields, QString * error );
	//! Append the next tile in table-index order. `raw` is the whole payload.
	bool addTile( const QByteArray & raw, bool cover, QString * error );
	//! Append an absent tile: 24 zero bytes, no payload.
	bool addAbsent( QString * error );
	bool finish( QString * error );

	int tilesWritten() const;
	int tilesPresent() const;
	quint64 fileBytes() const;

private:
	struct Impl;
	std::unique_ptr<Impl> d;
};

/*! Read and validate a container, by every rule of docs/LODGEN_TERRAIN_VT.md.
 *  On failure `error` NAMES the field that failed, because a validator that
 *  says only "invalid" cannot be mutation-tested. `payloadCheck` also
 *  recomputes every present tile's CRC, which an opening reader must not do —
 *  12,276 CRCs to open a file would cost the whole point of the index. */
bool lodvValidate( const QString & path, LodvHeaderFields * fields,
	std::vector<LodvTileEntry> * table, bool payloadCheck, QString * error );

//! Human-readable, one `key value` token per line, for the CLI and the harness.
QStringList lodvDescribe( const LodvHeaderFields & h, const std::vector<LodvTileEntry> & table );

#endif // LODVFILE_H
