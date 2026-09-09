/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODTFILE_H
#define LODTFILE_H

#include <QByteArray>
#include <QFile>
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
	 *  reachable without a rebuild (WW_LODT_VERSION is refused by name). */
	int headerVersion = 2;

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

//! Write <outDir>/Terrain/<EDID>.lodl for one worldspace.
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
