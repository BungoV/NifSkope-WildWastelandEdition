/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODTFILE_H
#define LODTFILE_H

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QList>
#include <QString>
#include <QVector>

#include <functional>

class EsmWorld;

/*! The landscape file's magic, first four bytes, little-endian.
 *
 *  It spells `LODT` on disk and it is NOT changing: the 2026-09-09 rename is an
 *  extension rename, and the gate on it is that the bytes of a `.lodl` are
 *  identical to the bytes the same worldspace wrote as a `.lodt` yesterday.
 *  What distinguishes the two formats is that the TEXTURE file has its own
 *  magic (LODTEX_MAGIC, `LDTX`, src/io/lodvfile.h) -- so each reader can NAME
 *  the other's file instead of misparsing it. */
constexpr quint32 LODL_MAGIC = 0x54444F4CU;   // 'L','O','D','T' little-endian

/*! The section-present bits of the header word at 0x44.
 *
 *  A READER CHECKS THE BIT, NOT THE OFFSET -- the rule
 *  docs/LODGEN_BTD_FORMAT.md has stated since version 1, and the reason the
 *  offsets of absent sections are allowed to hold anything at all. Bits 0..3
 *  are version 1's; bits 4..7 are version 3's water bodies, and each names one
 *  section that a version 1 or 2 file simply does not have. */
constexpr quint32 LODL_SECT_COLOUR      = 1u << 0;
constexpr quint32 LODL_SECT_GROUNDCOVER = 1u << 1;
constexpr quint32 LODL_SECT_AO          = 1u << 2;
constexpr quint32 LODL_SECT_WATER       = 1u << 3;
constexpr quint32 LODL_SECT_BODIES      = 1u << 4;   //!< body table + body-ID plane
constexpr quint32 LODL_SECT_FLOW        = 1u << 5;   //!< flow plane
constexpr quint32 LODL_SECT_SHORE       = 1u << 6;   //!< shore-distance plane
constexpr quint32 LODL_SECT_STROKE      = 1u << 7;   //!< stroke store
/*! The DYE plane (lane WATER4): where one body's water is carried into
 *  another's -- a river's tint past its mouth, a dye pin's plume.  It lives
 *  in the version-3 header's reserved word at 0xF4 as a 32-bit offset, so
 *  the version stays 3 and no existing offset moves; a reader checks this
 *  bit, never the word. */
constexpr quint32 LODL_SECT_DYE         = 1u << 8;   //!< dye plane

/*! One water BODY: a connected sheet of water with one plane height, one WATR
 *  form and one flow, addressed by the body-ID plane.
 *
 *  Sixteen WATR forms serve 590 candidate bodies in the Commonwealth --
 *  `ExtLakeWater` alone paints sixteen separate lakes with one colour and one
 *  velocity -- so "a different colour per body of water" cannot be expressed
 *  per FORM at all. That is what this record exists for. The layout is
 *  scratchpad/specs_20260909/spec_water.md §3.3 and it is 48 bytes; the file
 *  carries the stride, so a longer record can be added later without a version
 *  bump and a reader that knows fewer fields reads the prefix it knows. */
struct LodtWaterBody
{
	quint16 id = 0;              //!< == its index + 1; the reader refuses otherwise
	quint8 cls = 0;              //!< 0 sea, 1 river, 2 lake
	quint8 flags = 0;            //!< bit0 user-edited, 1 flow from a stroke,
	                             //!< 2 colour override, 3 ambiguous merge,
	                             //!< 4 TINY (< 4 texels), 5 class set by hand
	float waterHeight = 0.0f;    //!< world units; the body's one plane
	quint32 watrForm = 0;        //!< RESOLVED -- never 0, never 0xFFFF
	quint32 area = 0;            //!< texels of the body-ID plane
	qint16 x0 = 0, y0 = 0, x1 = 0, y1 = 0;   //!< cell bbox, inclusive
	quint16 source = 0;          //!< flows FROM this body, 0 = none
	quint16 outlet = 0;          //!< flows INTO this body, 0 = none
	float flowX = 0.0f, flowY = 0.0f;        //!< mean flow, world units/second
	quint8 colour[4] = { 0, 0, 0, 0 };       //!< RGBA override; A = 0 means none
	quint8 confidence = 0;       //!< 0..255
	quint8 flowSource = 0;       //!< 0 none, 1 form NAM0, 2 bed, 3 drain, 4 stroke
	quint32 nameOffset = 0;      //!< into the name blob, 0 = unnamed
};

/*! The water-bodies module's own switches (CONSTITUTION rule 10: every feature
 *  is a module with its own master switch and a fallback floor beneath it).
 *
 *  With `enabled` false the writer emits version 2 and the bytes are what they
 *  were; that is the zero-effort way back, and it is what `WW_LODL_VERSION=2`
 *  reaches without a rebuild. */
struct LodtWaterOptions
{
	bool enabled = false;        //!< --water-bodies
	int bridgeGap = 2;           //!< --water-bridge N, texels; 0 = no bridging
	int nearTexels = 64;         //!< the drainage proximity, texels
	int bodySamples = 0;         //!< body-ID plane rate; 0 = the file's own
	int flowSamples = 0;         //!< flow plane rate; 0 = the file's own
	bool shore = true;           //!< bake the shore-distance plane
	/*! The plugin the WATR `NAM0` linear velocities are read from -- vanilla's
	 *  ONLY flow signal, and the fallback floor under the automatic rules. Empty
	 *  means the arm is unavailable, which the census SAYS rather than silently
	 *  writing zeroes. */
	QString velocityPlugin;
	QString reportPath;          //!< --water-report <file>, empty = stdout only
};

/*! Writer for the `.lodl` whole-worldspace LANDSCAPE file.
 *
 *  THE NAME MOVED (bungo's ruling, 2026-09-09). This file used to be written
 *  as `.lodt`; `.lodt` now names the terrain TEXTURE sheets (src/io/lodvfile.h,
 *  magic LDTX), and the landscape file is `.lodl`. The C++ names in this header
 *  -- LodtFile, lodtWrite, LodtOptions -- were NOT renamed with it: they are
 *  internal, they appear in no file on disk and in no command, and renaming
 *  three hundred call sites is churn a reader gains nothing from. Read
 *  every `lodt` identifier here as "the .lodl landscape file". The magic bytes
 *  did not move either (LODL_MAGIC still spells LODT on disk), because the
 *  rename must leave the file byte-identical.
 *
 *  The format is specified in docs/LODGEN_BTD_FORMAT.md; that document is the
 *  contract, not this header. It replaces the per-chunk `.btr` terrain path
 *  entirely: one file per worldspace holding heights, LTEX blend alphas, water
 *  (height AND type), terrain colour, ground cover and a coarse AO channel,
 *  behind a LOD pyramid of zlib blocks.
 *
 *  Two properties are worth knowing before reading the implementation:
 *
 *   - the height quantum is a HEADER FIELD, not a constant. From Fallout 4 it
 *     is 8, which is VHGT's own storage quantum and therefore exactly
 *     lossless; a Fallout 76 source would use something finer;
 *   - the LOD pyramid is PROGRESSIVE and its coarse levels are SUBSAMPLES.
 *     Each finer level stores only the samples its coarser parent lacks, which
 *     is both what geomorphing needs (a coarse vertex must BE a fine vertex)
 *     and what a clipmap needs (levels must nest exactly).
 */
struct LodtOptions
{
	//! World units per stored height step. 8 = VHGT's quantum, lossless for FO4.
	float heightQuantum = 8.0f;
	//! Samples per cell edge. 32 from FO4 LAND; a header field, never assumed.
	int samplesPerCell = 32;
	//! Block edge in samples. One block per cell at the finest level.
	int blockEdge = 32;
	//! LOD levels, level 0 finest.
	int levelCount = 4;
	//! Coarse always-resident overview, samples per cell edge. 0 = none.
	int overviewSamples = 8;
	//! Baked AO, samples per cell edge. 0 = none.
	int aoSamples = 8;
	/*! Header version to WRITE. 2 adds the worldspace default water height and
	 *  WATR form after the section offsets, which is what makes a cell's
	 *  "water type 0xFFFF = the worldspace default" resolvable at all; 1 is the
	 *  exact bytes this writer produced before that, for a consumer that has
	 *  not learned version 2 yet. Both are accepted by the reader. The
	 *  environment variable WW_LODL_VERSION overrides it, so the fallback is
	 *  reachable without a rebuild (WW_LODT_VERSION is refused by name).
	 *
	 *  Version 3 appends the water-body sections. It is NOT the default: the
	 *  writer raises the version to 3 only when the water module is switched
	 *  on, so a file nobody asked new sections of is byte-identical to the one
	 *  this writer produced before the module existed. */
	int headerVersion = 2;

	//! Water bodies, flow and shore -- OFF by default; see LodtWaterOptions.
	LodtWaterOptions water;

	/*! Progress, for a GUI: phase 0 = pass one (done = cell rows, total =
	 *  cell rows), phase 1 = blocks (done = blocks emitted, total = blocks;
	 *  level and the block's i, j so a map can paint the cells it covers -
	 *  a level-L block at (i, j) is cells [i<<L, (i+1)<<L) x [j<<L, (j+1)<<L)).
	 *  Return false to cancel: the writer stops, removes the partial file and
	 *  fails with "cancelled". Called on the writer's thread. */
	std::function<bool( int phase, int done, int total, int level, int i, int j )> progress;
};

/*! Recompute ONLY the AO plane of an existing .lodl, in place.
 *
 *  AO is a flat uncompressed section of fixed size at a known offset, and it
 *  is derived from the file's own heights, so it can be rebuilt without
 *  touching anything else - which is what "re-bake the AO but keep the
 *  heightmap" means. The plane is computed by the same function the writer
 *  uses, from the same samples read back through the reader, so a refreshed
 *  file is byte-identical to a freshly written one. `progress( done, total )`
 *  counts AO rows; return false to cancel (the file is untouched until the
 *  single write at the end). */
bool lodtRefreshAo( const QString & path, QString * error,
	std::function<bool( int, int )> progress = {} );

//! Write <outDir>/FO4CSLOD/<EDID>/<EDID>.lodl for one worldspace (lane LAYOUT1,
//! 2026-09-16: the path is composed by lodgenFo4csWorldDir(), never spelled here).
bool lodtWrite( const EsmWorld & world, const QString & outDir,
	const LodtOptions & opts, QString * outPath, QString * error );

/*! Convert a Fallout 76 .btd to .lodl.
 *
 *  The format was specified with this conversion in mind, so most of it copies
 *  across untouched -- the alpha packing, the progressive 3/4-per-level pyramid
 *  and the block edge are the same scheme. Two things differ: heights are
 *  rescaled from their world-normalised uint16 onto our quantum, which is why
 *  the quantum is a header field rather than a constant, and there is no water,
 *  because a .btd carries none. `notes` receives what the conversion measured.
 */
bool lodtWriteBtd( const QString & btdPath, const QString & outDir,
	const LodtOptions & opts, QString * outPath, QString * error,
	QString * notes = nullptr, const EsmWorld * waterFrom = nullptr );

/*! The `--water-census` table, read back out of a written file.
 *
 *  It reads the FILE, never the writer's own in-memory tables: a census that
 *  reported what the writer meant to write rather than what a reader sees would
 *  be the "telemetry echoes intent" failure this tree has a rule against. False,
 *  with the reason named, when the file carries no body table. */
bool lodtWaterCensus( const QString & path, QString * text, QString * error );

/*! The water classifier's KNOWN-ANSWER control (`lodl <file> --water-selftest`).
 *
 *  A synthetic worldspace with a sea, a stepped river, a lake and a puddle whose
 *  answer was written down before the code, run through the same classifier that
 *  writes real files, with the type-blind REFUTER beside it so the check is seen
 *  to fail on the other side of the floor. False = the control did not hold. */
bool lodtWaterSelfTest( QString * text, QString * error );

/*! Reader. Nothing verifies a format writer except an independent reader --
 *  a file can be self-consistently wrong, which is exactly how a DDS header
 *  with the pixel-format block at the wrong offset passed its own parser until
 *  a shipped file was run through the same code.
 */
class LodtFile
{
public:
	bool open( const QString & path, QString * error );

	int cellMinX() const { return minX; }
	int cellMinY() const { return minY; }
	int cellMaxX() const { return maxX; }
	int cellMaxY() const { return maxY; }
	int cellsX() const { return maxX - minX + 1; }
	int cellsY() const { return maxY - minY + 1; }
	int samplesPerCell() const { return spc; }
	int blockEdge() const { return blkEdge; }
	int levelCount() const { return levels; }
	float heightQuantum() const { return quantum; }
	float minHeight() const { return hMin; }
	float maxHeight() const { return hMax; }
	int ltexCount() const { return int( ltex.size() ); }
	int watrCount() const { return int( watr.size() ); }
	int headerVersion() const { return ver; }
	/*! The worldspace's own water, from WRLD DNAM/NAM2 -- what a cell whose
	 *  water type reads 0xFFFF is inheriting. Version 1 files carry neither,
	 *  and report 0 and "no default water": the fields did not exist, so a
	 *  reader must not treat the zero as a form id. */
	bool hasDefaultWater() const { return ver >= 2 && defWaterType != 0; }
	float defaultWaterHeight() const { return defWaterH; }
	quint32 defaultWaterType() const { return defWaterType; }
	int gcvrCount() const { return int( gcvr.size() ); }
	int aoSamples() const { return aoS; }
	int overviewSamples() const { return ovS; }
	quint32 sectionFlags() const { return sect; }
	quint32 ltexForm( int i ) const { return ltex.value( i, 0 ); }
	quint32 watrForm( int i ) const { return watr.value( i, 0 ); }
	quint32 gcvrForm( int i ) const { return gcvr.value( i, 0 ); }

	/* ---- version 3: water bodies, flow and shore --------------------------
	 *
	 * Every one of these answers a NEUTRAL value on a file that carries no such
	 * section, for the same reason aoSample() answers 255: an absent plane and
	 * an all-zero plane look identical in a picture, so the neutral value is
	 * the one that changes nothing. `bodyCount() == 0` is how a consumer asks
	 * whether the section is there at all -- it already tested the flag bit. */

	//! How many bodies the table holds; 0 when the file carries none.
	int bodyCount() const { return int( bodies.size() ); }
	//! The stride the FILE declares, which may exceed this reader's 48.
	int bodyRecordBytes() const { return bodyStride; }
	/*! Body `id` (1-based, as the plane stores it). False outside 1..count. */
	bool waterBody( int id, LodtWaterBody & out ) const;
	//! A body's name from the blob, or an empty string.
	QString bodyName( const LodtWaterBody & b ) const;

	int bodyIdSamples() const { return bodyS; }
	int flowPlaneSamples() const { return flowS; }
	int shorePlaneSamples() const { return shoreS; }
	//! World units per stored shore step (32 as written).
	float shoreQuantum() const { return shoreQ; }
	//! Flow-word encoding id; 0 = dir8 / speed4 / confidence4.
	quint32 flowEncoding() const { return flowEnc; }
	/*! The dye plane (LODL_SECT_DYE): samples per cell edge, 0 when absent.
	 *  One uint32 a sample: bits 0..15 the SOURCE -- 1..32767 a body id, the
	 *  body whose water this is; 0x8000 | n the n-th dye pin of the stroke
	 *  store; 0 no dye -- and bits 16..23 the weight 0..255.  The plane is
	 *  written at the flow plane's rate by the marking tool (watermark.cpp);
	 *  the generator never writes one. */
	int dyePlaneSamples() const { return dyeS; }
	//! Where the dye plane store sits, 0 when absent (the word at 0xF4).
	quint64 dyePlaneOffset() const { return dyeAt; }

	//! Body id at a sample of the BODY plane's own grid; 0 = no water here.
	quint16 bodyIdAt( int bx, int by ) const;
	//! Flow word at a sample of the FLOW plane's own grid; 0 = still water.
	quint16 flowWordAt( int fx, int fy ) const;
	//! Shore steps at a sample of the SHORE plane's own grid; 255 = far/absent.
	quint8 shoreAt( int sx, int sy ) const;
	//! Dye word at a sample of the DYE plane's own grid; 0 = no dye here.
	quint32 dyeWordAt( int dx, int dy ) const;

	//! How many strokes the store holds; 0 when there is no store.
	int strokeCount() const { return nStrokes; }
	//! The raw stroke store, exactly as written (empty when absent).
	QByteArray strokeStore() const { return strokes; }

	//! Per-cell record. Returns false outside the worldspace.
	bool cell( int cx, int cy, float & lo, float & hi,
		float & waterH, quint16 & waterType, quint16 & flags ) const;

	//! Height at a full-rate global sample, walking the progressive pyramid.
	//! This is the operation the whole block layout exists to serve.
	float height( int gx, int gy ) const;

	//! The stored height word itself (height = (word - 32767) * quantum).
	quint16 heightWord( int gx, int gy ) const { return planeSample( gx, gy, 0 ); }

	//! Where the AO plane sits in the file; 0 when there is none.
	quint64 aoOffset() const { return ( sect & 4u ) ? oAo : 0; }

	/*! One texel of the baked AO plane, which is FLAT and uncompressed:
	 *  `cellsX * aoSamples` by `cellsY * aoSamples`, one uint8 each. 255 is
	 *  fully open sky. Returns 255 - the "nothing occludes this" value - when
	 *  the file carries no AO plane or the coordinates leave it, so a consumer
	 *  that forgot to check `aoSamples()` darkens nothing rather than
	 *  everything. */
	quint8 aoSample( int ax, int ay ) const;

	/*! One sample of the coarse overview: the uncompressed always-resident
	 *  height grid at `overviewSamples()` a cell, in the SAME quantised
	 *  encoding as the blocks (`height = (word - 32767) * quantum`), so a
	 *  reader can compare the two directly. 32767 (height zero) when there is
	 *  no overview section or the coordinates leave it. */
	quint16 overviewWord( int ox, int oy ) const;
	float overviewHeight( int ox, int oy ) const
	{ return ( float( overviewWord( ox, oy ) ) - 32767.0f ) * quantum; }

	/*! How many inflated blocks the reader keeps. One is right for a verifier
	 *  walking samples in order; a consumer that reads a SUBSAMPLED grid needs
	 *  more, because the progressive pyramid puts consecutive samples of such
	 *  a grid on DIFFERENT levels - a step of 4 alternates between the level
	 *  that stores every 4th sample and the one that stores every 8th, and a
	 *  one-block cache then misses on every single sample. Clamped to >= 1. */
	void setBlockCacheSize( int blocks );

	//! LTEX alpha word at a full-rate global sample; five 3-bit weights.
	quint16 alphaWord( int gx, int gy ) const;

	//! Terrain colour, 5-5-5 (R at 11, G at 6, B at 0); 0xFFFF when the file has none.
	quint16 colourWord( int gx, int gy ) const;

	//! Ground cover mask, one bit a slot; 0 when the file has none.
	quint16 groundCover( int gx, int gy ) const;

	//! The five LTEX slots of a quadrant, plus its base at index 5.
	void quadrantSlots( int cx, int cy, int quad, quint16 out[6] ) const;

	int blockCount() const { return nBlocks; }

private:
	/* Everything up to the block data - header, tables, per-cell records, the
	 * overview, the AO plane and the block directory - is read once into
	 * `buf`; that is ~20 MB for Appalachia. The block payloads are NOT: they
	 * are seek-read one at a time through `file`, because the payloads are
	 * the file (1.5 GB of Appalachia's 1.55) and a reader that loaded them
	 * whole was the 2.1 GB peak the streamed writer had been blamed for. */
	mutable QFile file;
	QByteArray buf;
	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	int spc = 32, blkEdge = 32, levels = 4, aoS = 0, ovS = 0;
	int ver = 1;
	float defWaterH = 0.0f;
	quint32 defWaterType = 0;
	float quantum = 8.0f, hMin = 0.0f, hMax = 0.0f;
	quint32 sect = 0;
	QVector<quint32> ltex, watr, gcvr;
	quint64 oQuad = 0, oCell = 0, oOver = 0, oAo = 0, oDir = 0, oData = 0;
	int nBlocks = 0;

	/* ---- version 3 ------------------------------------------------------
	 * The water sections live AFTER the block data, so they are not inside
	 * `buf` (which stops at oData) and are read into their own buffers at
	 * open: the body table and the name blob whole, each plane's directory
	 * whole, and the tile payloads on demand. A directory is 16 bytes a cell
	 * -- 590 KB for the Commonwealth -- and a uniform tile costs no read at
	 * all, which is the whole point of the container. */
	QVector<LodtWaterBody> bodies;
	QByteArray nameBlob;
	int bodyStride = 0;
	int bodyS = 0, flowS = 0, shoreS = 0;
	int dyeS = 0;
	quint64 dyeAt = 0;
	float shoreQ = 32.0f;
	quint32 flowEnc = 0;
	int nStrokes = 0;
	QByteArray strokes;

	struct PlaneStore
	{
		int tilesX = 0, tilesY = 0, tileEdge = 0, bytesPerSample = 0;
		quint64 dirOffset = 0, dataOffset = 0;
		QByteArray dir;              //!< tilesX*tilesY * { u64, u32, u32 }
		bool ok = false;
	};
	PlaneStore idStore, flowStore, shoreStore;
	PlaneStore dyeStore;
	//! Read a plane container's head and directory; refuses by name.
	bool readPlaneStore( quint64 at, int bytesPerSample, PlaneStore & s,
		QString * error ) const;
	//! One tile, inflated; `uniform` tiles never touch the file.
	quint32 planeSampleOf( const PlaneStore & s, int x, int y, quint32 absent ) const;
	mutable QHash<quint64, QByteArray> tileCache;
	mutable QList<quint64> tileOrder;

	//! plane 0 = heights, 1 = alphas, 2 = colour, 3 = ground cover
	quint16 planeSample( int gx, int gy, int plane ) const;

	/* Inflated blocks, kept between calls. Consecutive samples almost always
	 * land in the same block, and without this every sample paid a full 131 KB
	 * inflate: --verify-only on Appalachia took 4m48s for 18,496 samples.
	 * Least-recently-used, capacity 1 by default so a sample walk behaves
	 * exactly as it did when this was a single slot. Single-threaded by
	 * design; this is a verifier, not a renderer. */
	struct CachedBlock
	{
		int idx = -1;
		quint64 used = 0;
		QByteArray raw;
	};
	mutable QVector<CachedBlock> cache;
	mutable quint64 cacheClock = 0;
	int cacheCap = 1;
};

#endif // LODTFILE_H
