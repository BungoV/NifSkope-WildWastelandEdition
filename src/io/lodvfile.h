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


//! What one sheet of a tile carries.
enum LodvSheetRole
{
	LODV_ROLE_UNUSED = 0,
	LODV_ROLE_COLOR = 1,        //!< RGB albedo, the grass tint folded in
	LODV_ROLE_MSN = 2,          //!< model-space normal, 0.5 + 0.5 encoded
	LODV_ROLE_DATA = 3,         //!< R AO, G wetness, B shore proximity, A cover
	LODV_ROLE_HEIGHT = 4        //!< R16_UNORM, height/8 + 32767, as the shadow heightmap encodes it
};

//! DXGI formats this version writes and accepts.
enum LodvDxgiFormat
{
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
	LODV_TILE_COVER = 2         //!< the data sheet uses dxgiFormatCover, cover in alpha
};

struct LodvSheetDesc
{
	quint16 dxgiFormat = 0;         //!< the format when the tile's COVER bit is CLEAR
	quint16 dxgiFormatCover = 0;    //!< and when it is SET; equal except on role 3
	quint8 role = LODV_ROLE_UNUSED;
	quint8 colorSpace = 0;          //!< 0 linear, 1 sRGB
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
	LodvSheetDesc sheets[4];
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
//! One sheet's bytes at one mip, for a reader that unpacks a payload.
quint32 lodvSheetMipBytes( const LodvHeaderFields & h, int sheet, int mip, bool cover );

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
