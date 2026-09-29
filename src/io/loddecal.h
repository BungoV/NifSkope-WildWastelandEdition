/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODDECAL_H
#define LODDECAL_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include <vector>

/*! THE GROUND DECALS (lane TERRLIVE1, 2026-09-29): the roads and flat objects
 *  of the LOD terrain as PROJECTED decals instead of paint baked into the
 *  terrain texture. Two files beside the `.lodl`, written by EVERY bake
 *  whatever its terrain option (full / hybrid / dynamic):
 *
 *    <ws>.lodd   THE LIBRARY. One picture set per DISTINCT piece -- a piece is
 *                a model path plus its effective material swap, because the
 *                swap changes the picture. Each piece is scan-converted in its
 *                own frame (the placement's transform removed) by the same
 *                rasteriser that paints roads into the terrain sheets, so a
 *                decal and the baked paint follow one law.
 *    <ws>.lodg   THE PLACEMENTS. One 32-byte record a placed piece (position,
 *                rotation, scale, piece index), stored in DRAW ORDER, plus a
 *                per-cell index of record ids for a consumer that streams by
 *                cell.
 *
 *  Layout, load-bearing (changing any of it is a format break):
 *
 *   * little-endian, absolute 64-bit offsets, fixed 256-byte headers, fixed
 *     strides (80-byte piece entry, 32-byte placement record);
 *   * a picture is NORTH-UP (row 0 = the piece's local +Y edge), west first,
 *     its mip chain in order from mip 0, each mip BC3 blocks row-major
 *     (a mip of w x h texels is max(1,(w+3)/4) x max(1,(h+3)/4) blocks);
 *   * colour sheet = BC3 sRGB: RGB the piece's colour, A its coverage (the
 *     same A the baked paint lerps by and suppresses ground cover with);
 *     normal sheet = BC3 linear: RGB = the stamped normal in the piece's frame
 *     (x east, y north, z up) as n*0.5+0.5, A = the stamp's weight;
 *   * the cell table's row 0 is the SOUTH row (as the `.lodl`'s), reached only
 *     through LodgFile::cellRecords( cx, cy ), which takes world cells;
 *   * a placement's box is the piece's local bounds [x0,x1] x [y0,y1] x
 *     [z0 - boxPad, z1 + boxPad], transformed by the record; the decal is
 *     projected along the box's local -Z.
 *
 *  Each file carries CRC32s (the LODT container's CRC, lodvCrc32) over its
 *  header, tables and every picture, and the `.lodg` names the `.lodd` it
 *  was written against by that file's table CRC, so a mismatched pair is a
 *  named refusal. */

constexpr quint32 LODD_MAGIC = 0x44444F4CU;   // 'L','O','D','D'
constexpr quint32 LODG_MAGIC = 0x47444F4CU;   // 'L','O','D','G'
constexpr quint32 LODD_VERSION = 1;
constexpr quint32 LODG_VERSION = 1;
constexpr int LODD_HEADER_BYTES = 256;
constexpr int LODD_PIECE_BYTES = 80;
constexpr int LODG_RECORD_BYTES = 32;
constexpr quint32 LODD_DXGI_BC3_UNORM = 77;
constexpr quint32 LODD_DXGI_BC3_UNORM_SRGB = 78;

//! Draw class, which is also the draw order: roads, then opaque flat objects,
//! then decal / blended / tested flat objects -- the rasteriser's own order.
enum LoddClass : int
{
	LODD_CLASS_ROAD = 0,
	LODD_CLASS_FLAT = 1,
	LODD_CLASS_FLAT_OVER = 2
};

struct LoddPiece
{
	float x0 = 0, y0 = 0, x1 = 0, y1 = 0;   //!< the picture's rectangle, piece-local units
	float z0 = 0, z1 = 0;                   //!< the geometry's local Z range
	int width = 0, height = 0;              //!< mip 0 texels, multiples of 4
	int mips = 0;
	int cls = LODD_CLASS_ROAD;
	quint32 flags = 0;                      //!< bit 0: the normal sheet carries a stamp
	quint32 covered = 0;                    //!< mip-0 texels with coverage > 0
	QString name;                           //!< "<model path>|<mswp form, 8 hex>"
	// file layout (filled by the writer / read by the reader)
	quint64 colourOffset = 0, normalOffset = 0;
	quint32 colourBytes = 0, normalBytes = 0, colourCrc = 0, normalCrc = 0;
	// writer input: the encoded mip chains
	QByteArray colourBc3, normalBc3;
};

struct LodgRecord
{
	float pos[3] = { 0, 0, 0 };
	float quat[4] = { 0, 0, 0, 1 };         //!< x y z w, stored as snorm16
	float scale = 1.0f;
	int piece = 0;
	int cls = 0;
	quint32 flags = 0;
	quint32 refr = 0;                       //!< the placed reference (the SCOL for a part)
};

//! Bytes of one BC3 mip chain for a w x h picture with `mips` levels.
qint64 loddChainBytes( int w, int h, int mips );

/*! Write the library. `pieces` carry their encoded chains; offsets, sizes and
 *  CRCs are filled in. `tableCrcOut` receives the CRC the `.lodg` names. */
bool loddWrite( const QString & path, const QString & worldEdid, float unitsPerTexel, int maxSide,
	QVector<LoddPiece> & pieces, quint32 * tableCrcOut, QString * error );

/*! Write the placements. `records` must already be in draw order. The cell
 *  index lists every record whose WORLD box footprint touches a cell of the
 *  rectangle [cx0..cx1] x [cy0..cy1]; `boxes` holds each record's world XY
 *  bounds (x0 y0 x1 y1) for that. */
bool lodgWrite( const QString & path, const QString & worldEdid, int cx0, int cy0, int cx1, int cy1,
	float boxPad, int pieceCount, quint32 loddTableCrc, const QVector<LodgRecord> & records,
	const QVector<QVector<float>> & boxes, QString * error );

class LoddFile
{
public:
	bool open( const QString & path, QString * error );
	int pieceCount() const { return int( pcs.size() ); }
	const LoddPiece & piece( int i ) const { return pcs[i]; }
	float unitsPerTexel() const { return upt; }
	int maxSide() const { return side; }
	QString worldEdid() const { return ws; }
	quint32 tableCrc() const { return tcrc; }
	qint64 fileBytes() const { return bytes; }
	/*! Decode one mip of a piece's colour (normal = false) or normal sheet to
	 *  0xAARRGGBB, north-up. */
	bool picture( int i, bool normal, int mip, std::vector<quint32> & out, int * w, int * h,
		QString * error ) const;
	/*! Every picture's CRC against its bytes, every chain's size against its
	 *  dimensions. One line of findings in `report`. */
	bool verify( QString * report, QString * error ) const;
private:
	QString filePath, ws;
	float upt = 4.0f;
	int side = 0;
	quint32 tcrc = 0;
	qint64 bytes = 0;
	QVector<LoddPiece> pcs;
};

class LodgFile
{
public:
	bool open( const QString & path, QString * error );
	int recordCount() const { return int( recs.size() ); }
	const LodgRecord & record( int i ) const { return recs[i]; }
	int cellMinX() const { return minX; }
	int cellMinY() const { return minY; }
	int cellMaxX() const { return maxX; }
	int cellMaxY() const { return maxY; }
	float boxPad() const { return pad; }
	int pieceCount() const { return nPieces; }
	quint32 loddTableCrc() const { return loddCrc; }
	qint64 fileBytes() const { return bytes; }
	qint64 indexEntries() const { return qint64( index.size() ); }
	//! The record ids touching world cell (cx, cy), in draw order.
	QVector<quint32> cellRecords( int cx, int cy ) const;
	/*! Every record's piece against the piece count, every index entry against
	 *  the record count, the index's order. */
	bool verify( QString * report, QString * error ) const;
private:
	int minX = 0, minY = 0, maxX = -1, maxY = -1, nPieces = 0;
	float pad = 0.0f;
	quint32 loddCrc = 0;
	qint64 bytes = 0;
	QVector<LodgRecord> recs;
	std::vector<quint32> cellFirst, cellCount, index;
};

/*! Open `<dir>/<ws>.lodd` and `<ws>.lodg` (or the pair beside a given file),
 *  verify both and their pairing. One report paragraph; false on any finding. */
bool loddCheckPair( const QString & fileOrDir, QString * report, QString * error );

#endif
