/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodtfile.h"
#include "esmdata.h"

/* Only for LODTEX_MAGIC: this reader must be able to NAME a terrain
 * TEXTURE file when it is handed one, because `.lodt` meant THIS format
 * until 2026-09-09 and means that one now. */
#include "io/lodvfile.h"

#include "btdfile.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QVector>

#include <functional>
#include <limits>
#include <memory>
#include <algorithm>
#include <atomic>
#include <thread>
#include <cmath>
#include <cstring>
#include <vector>

/*  docs/LODGEN_BTD_FORMAT.md is the contract. This file must not become a
 *  second, divergent description of it -- comments here explain WHY a thing is
 *  done, and the document says WHAT the bytes are.
 */

namespace
{

/* LODL_MAGIC is in lodtfile.h -- src/io/lodvfile.cpp refuses it by name.
 * Version 2 appended the worldspace default water fields AFTER the ten
 * section offsets, so 0x00..0x97 is unchanged and every offset a version 1
 * reader uses still sits where it did; only the first section moved, and it is
 * addressed by an offset in the header. The writer emits 2; both are read. */
constexpr quint32 LODL_VERSION = 2;
constexpr quint32 LODL_VERSION_MIN = 1;
constexpr qsizetype LODL_HEADER_V1 = 0x98;
constexpr qsizetype LODL_HEADER_V2 = 0xA0;

constexpr quint32 SECT_COLOUR = 1u << 0;
constexpr quint32 SECT_GROUNDCOVER = 1u << 1;
constexpr quint32 SECT_AO = 1u << 2;
constexpr quint32 SECT_WATER = 1u << 3;

constexpr quint16 WATER_TYPE_DEFAULT = 0xFFFFU;
constexpr quint16 CELL_HAS_WATER = 1u << 0;
constexpr quint16 CELL_HAS_LAND = 1u << 1;

/*! World units -> the stored word. ONE encoding, used by every plane the
 *  writer emits and by the landless-cell fallback, because they were two
 *  expressions and they disagreed: the fallback used a bare 32767, which is
 *  height ZERO, where the rest of the file (and the shadow heightmap) use the
 *  worldspace's default land height. */
static inline quint16 lodtHeightWord( double h, double quantum )
{
	return quint16( qBound( 0.0, std::floor( h / quantum + 32767.0 + 0.5 ), 65535.0 ) );
}

//! Little-endian appenders. Everything in the file is LE regardless of host.
struct Buf
{
	QByteArray b;
	void u8( quint8 v ) { b.append( char( v ) ); }
	void u16( quint16 v ) { u8( v & 0xFF ); u8( ( v >> 8 ) & 0xFF ); }
	void u32( quint32 v ) { u16( v & 0xFFFF ); u16( ( v >> 16 ) & 0xFFFF ); }
	void u64( quint64 v ) { u32( quint32( v & 0xFFFFFFFFU ) ); u32( quint32( v >> 32 ) ); }
	void i32( qint32 v ) { u32( quint32( v ) ); }
	void f32( float v ) { quint32 t; std::memcpy( &t, &v, 4 ); u32( t ); }
	qsizetype size() const { return b.size(); }
};

//! Patch a u64 already written at a known offset, once its target is known.
void patch64( QByteArray & b, qsizetype at, quint64 v )
{
	for ( int i = 0; i < 8; i++ )
		b[at + i] = char( ( v >> ( i * 8 ) ) & 0xFF );
}

/*! Everything the writer needs from a landscape source.
 *
 *  Two exist: Fallout 4's LAND records, and a Fallout 76 .btd. The writer
 *  below knows about neither, which is the point of having specified the
 *  format before implementing it -- the .btd path turned out to need no
 *  change to any section but the height quantum.
 */
struct LodtSource
{
	QString edid;
	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	int spc = 32;                     //!< this source's native samples a cell edge
	float defaultLand = 0.0f;
	quint32 defaultWaterType = 0;
	float defaultWaterHeight = 0.0f;

	//! cell height range in world units; false = no landscape in this cell
	std::function<bool( int, int, float &, float & )> range;
	//! does this cell carry authored vertex colour?
	std::function<bool( int, int )> hasColour;
	//! quadrant q: out[0..4] = five strongest layer LTEX forms (0 = unused),
	//! out[5] = the base texture. Forms, not indices - the writer interns them.
	std::function<void( int, int, int, quint32 * )> quadForms;
	//! quadrant q: ground cover forms, up to 8; returns how many
	std::function<int( int, int, int, quint32 * )> quadGcvr;
	/*! One cell's four planes at the native rate, packed exactly as the file
	 *  stores them. `slotForms` is the cell's 24 quadrant slots (6 a quadrant,
	 *  in q order) already resolved to LTEX FORM IDs, 0 = unused -- a source
	 *  matches its own layers against those and never sees the file's table.
	 *  false = no landscape. */
	std::function<bool( int, int, const quint32 *, float,
		std::vector<quint16> &, std::vector<quint16> &,
		std::vector<quint16> &, std::vector<quint16> & )> planes;
	//! cell water height and type form; false = none
	std::function<bool( int, int, float &, quint32 & )> water;
	/*! The range of the samples a cell with NO landscape of its own still
	 *  inherits from a neighbour across the shared VHGT edge; false = it
	 *  inherits nothing. Optional -- a .btd has no shared edge. */
	std::function<bool( int, int, float &, float & )> edgeRange;
};

/* The AO plane from a coarse height grid: horizon-based sky occlusion. Eight
 * directions, march out to twelve coarse samples, keep the steepest upward
 * slope in each, sum a saturating occlusion. Row 0 is SOUTH (the grid's own
 * order, cell row 0 first), not the DDS heightmap's north-up.
 *
 * ONE function for the writer and for lodtRefreshAo, so a refreshed plane is
 * byte-identical to a written one; the grid it reads is the stored height
 * word, whichever side produced it. */
static QByteArray lodtComputeAo( const std::vector<quint16> & grid, int aw, int ah,
	int aoS, float quantum, const std::function<bool( int, int )> & progress = {} )
{
	const double aoSpacing = 4096.0 / double( aoS );
	static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
		{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f },
		{ -0.7071f, 0.7071f }, { -0.7071f, -0.7071f } };
	auto aoHeight = [&]( int x, int y ) {
		x = qBound( 0, x, aw - 1 );
		y = qBound( 0, y, ah - 1 );
		return ( double( grid[size_t( y ) * size_t( aw ) + size_t( x )] ) - 32767.0 )
			* double( quantum );
	};
	QByteArray out;
	out.resize( qsizetype( aw ) * ah );
	for ( int y = 0; y < ah; y++ ) {
		for ( int x = 0; x < aw; x++ ) {
			const double h0 = aoHeight( x, y );
			double occl = 0.0;
			for ( const auto & d : dirs ) {
				double maxSlope = 0.0;
				for ( int step = 1; step <= 12; step += ( step < 4 ? 1 : 3 ) ) {
					const double dh = aoHeight( x + int( d[0] * float( step ) ),
						y + int( d[1] * float( step ) ) ) - h0;
					if ( dh > 0.0 ) {
						const double dist = double( step ) * aoSpacing
							* ( ( d[0] != 0.0f && d[1] != 0.0f ) ? 1.41421 : 1.0 );
						maxSlope = qMax( maxSlope, dh / dist );
					}
				}
				occl += maxSlope / ( 1.0 + maxSlope );
			}
			const double vis = qBound( 0.0, 1.0 - occl / 8.0 * 1.6, 1.0 );
			out[qsizetype( y ) * aw + x] = char( quint8( vis * 255.0 + 0.5 ) );
		}
		if ( progress && !progress( y + 1, ah ) )
			return QByteArray();
	}
	return out;
}

} // namespace

static bool lodtWriteSource( const LodtSource & src, const QString & outDir,
	const LodtOptions & opts, QString * outPath, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};

	const QString edid = src.edid;
	if ( edid.isEmpty() )
		return fail( QStringLiteral( "worldspace has no editor ID to name the file with" ) );

	const int minX = src.minX, minY = src.minY, maxX = src.maxX, maxY = src.maxY;
	if ( minX > maxX || minY > maxY )
		return fail( QStringLiteral( "worldspace has no indexed cells" ) );
	const int cellsX = maxX - minX + 1;
	const int cellsY = maxY - minY + 1;
	/* The RATE COMES FROM THE SOURCE, not the options: FO4's LAND is 32 a cell
	 * and a .btd is 128. It is a header field precisely so both fit. */
	const int spc = src.spc;
	const float quantum = opts.heightQuantum > 0.0f ? opts.heightQuantum : 8.0f;

	/* Pass one: walk every cell for the per-cell table, the form-ID tables and
	 * the height range. The heights themselves are re-read per block later
	 * rather than held -- a 804-cell worldspace is 662M samples and will not
	 * fit in memory as floats. */
	QVector<quint32> watrForms;
	QHash<quint32, int> watrIndex;
	QVector<quint32> ltexForms;
	QHash<quint32, int> ltexIndex;
	QVector<quint32> gcvrForms;
	QHash<quint32, int> gcvrIndex;
	auto ltexSlot = [&]( quint32 form ) -> quint16 {
		if ( !form )
			return 0xFFFFU;
		auto it = ltexIndex.constFind( form );
		if ( it != ltexIndex.constEnd() )
			return quint16( it.value() );
		const int idx = ltexForms.size();
		ltexForms.append( form );
		ltexIndex.insert( form, idx );
		return quint16( idx );
	};
	/* Six slots a quadrant: five layers plus the base at slot 5. Five is what
	 * a uint16 of 3-bit alphas can address, and it covers 99.27% of the
	 * Commonwealth's quadrants -- the rest lose their WEAKEST layers, which are
	 * the least visible by definition. */
	const int quadsX = cellsX * 2, quadsY = cellsY * 2;
	std::vector<quint16> quadSlots( size_t( quadsX ) * quadsY * 6, 0xFFFFU );
	int colourCells = 0;
	std::vector<float> cellMinH( size_t( cellsX ) * cellsY, 0.0f );
	std::vector<float> cellMaxH( size_t( cellsX ) * cellsY, 0.0f );
	std::vector<float> cellWaterH( size_t( cellsX ) * cellsY, 0.0f );
	std::vector<quint16> cellWaterT( size_t( cellsX ) * cellsY, WATER_TYPE_DEFAULT );
	std::vector<quint16> cellFlags( size_t( cellsX ) * cellsY, 0 );

	/* Where the time goes, per phase, reported in the census line. Put in
	 * before any optimisation so the first cut is aimed at a number, not a
	 * hunch: the writer took 30 s on the Commonwealth and the split between
	 * LAND decoding, the AO march and zlib was a guess. */
	QElapsedTimer tm;
	tm.start();
	qint64 tPass1 = 0, tOver = 0, tAo = 0, tLev[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	qint64 nsZip = 0, nsIo = 0, decodes = 0, decodesAtLevel[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, nullPlanes = 0;
	int curLevel = 7;
	const int gw = cellsX * spc;
	const int gh = cellsY * spc;

	/* There is no global grid. An earlier draft built three of them - heights,
	 * alphas and colour - which is 75 MB a plane for the Commonwealth but
	 * 1.32 GB a plane for the 804-cell target, so it carried a guard that
	 * refused anything large. Instead a cell's three planes are decoded on
	 * demand and kept in a small cache. Blocks are visited in spatial order, so
	 * the cells a block needs are almost always already resident.
	 *
	 * The cost is re-reading cells across levels - a level-3 block spans 8x8
	 * cells - which is roughly 4x the LAND parses of the grid path, and buys an
	 * unbounded worldspace size. */
	struct CellPlanes
	{
		std::vector<quint16> h, a, c, g;
	};
	QHash<qint64, CellPlanes> cellCache;
	QList<qint64> cacheOrder;
	/* Sized to a memory budget, not a constant. Every pass over the world -
	 * the overview, then each pyramid level - decodes each cell once if the
	 * cache holds the world and once PER PASS if it does not. At 512 entries
	 * the Commonwealth was decoded five times over; at 1 GB it fits whole
	 * (36,864 cells x ~8 KB) and is decoded once. Appalachia at 128 a cell
	 * does not fit and streams as before - its decode is a tile-cache fetch,
	 * not a LAND parse, so that is the cheap case. Floor of 512 so a level-3
	 * block's 64 cells always fit. */
	const qint64 bytesPerCell = qint64( spc ) * spc * 2 * 4 + 256;
	const int CACHE_CELLS = int( qBound( qint64( 512 ), ( qint64( 1 ) << 30 ) / bytesPerCell,
		qint64( cellsX ) * qint64( cellsY ) ) );

	auto planesFor = [&]( int cx, int cy ) -> const CellPlanes * {
		const qint64 key = ( qint64( cx ) << 32 ) | qint64( quint32( cy ) );
		auto it = cellCache.constFind( key );
		if ( it != cellCache.constEnd() )
			return &it.value();

		quint32 slotForms[24];
		for ( int q = 0; q < 4; q++ ) {
			const size_t o = ( size_t( ( cy - minY ) * 2 + ( q >> 1 ) ) * quadsX
				+ size_t( ( cx - minX ) * 2 + ( q & 1 ) ) ) * 6;
			for ( int k = 0; k < 6; k++ ) {
				const quint16 idx = quadSlots[o + size_t( k )];
				slotForms[q * 6 + k] =
					idx == 0xFFFFU ? 0u : quint32( ltexForms.value( int( idx ), 0 ) );
			}
		}

		CellPlanes cp;
		if ( !src.planes( cx, cy, slotForms, quantum, cp.h, cp.a, cp.c, cp.g ) ) {
			nullPlanes++;
			return nullptr;
		}
		decodes++;
		decodesAtLevel[curLevel]++;

		if ( cacheOrder.size() >= CACHE_CELLS ) {
			cellCache.remove( cacheOrder.first() );
			cacheOrder.removeFirst();
		}
		cacheOrder.append( key );
		return &cellCache.insert( key, cp ).value();
	};

	//! plane 0 heights, 1 alphas, 2 colour, 3 ground cover; full-rate sample
	auto sampleAt = [&]( int plane, int gx, int gy ) -> quint16 {
		gx = qBound( 0, gx, gw - 1 );
		gy = qBound( 0, gy, gh - 1 );
		const CellPlanes * cp = planesFor( minX + gx / spc, minY + gy / spc );
		if ( !cp )
			return plane == 0 ? lodtHeightWord( src.defaultLand, quantum )
				: ( plane == 2 ? quint16( 0xFFFFU ) : quint16( 0 ) );
		const size_t k = size_t( gy % spc ) * size_t( spc ) + size_t( gx % spc );
		switch ( plane ) {
		case 0:  return cp->h[k];
		case 1:  return cp->a[k];
		case 2:  return cp->c.empty() ? 0xFFFFU : cp->c[k];
		default: return cp->g.empty() ? 0 : cp->g[k];
		}
	};


	/* The coarse overview: an uncompressed whole-world height grid that never
	 * needs a block read. It is the outermost clipmap ring and the horizon
	 * silhouette, and paying an inflate for either is the wrong shape. */
	const int ov = qMax( 0, opts.overviewSamples );
	const int aoS = qMax( 0, opts.aoSamples );

	std::vector<quint16> gcvrSlots;
	float worldMin = 3.4e38f, worldMax = -3.4e38f;
	int landCells = 0, waterCells = 0;
	for ( int cy = minY; cy <= maxY; cy++ ) {
		if ( opts.progress && !opts.progress( 0, cy - minY, cellsY, 0, 0, 0 ) )
			return fail( QStringLiteral( "cancelled" ) );
		for ( int cx = minX; cx <= maxX; cx++ ) {
			const size_t s = size_t( cy - minY ) * cellsX + size_t( cx - minX );
			float lo = 3.4e38f, hi = -3.4e38f;
			if ( src.range( cx, cy, lo, hi ) ) {
				landCells++;
				cellFlags[s] |= CELL_HAS_LAND;
			} else {
				/* No LAND record: the surface here is the worldspace's default
				 * land height -- what the shadow heightmap writes for exactly
				 * these texels -- except on the row and column this cell shares
				 * with a neighbour that does have terrain. Without those in the
				 * range, a renderer culling on it would cull the inherited row. */
				lo = hi = src.defaultLand;
				float elo = 0.0f, ehi = 0.0f;
				if ( src.edgeRange && src.edgeRange( cx, cy, elo, ehi ) ) {
					lo = qMin( lo, elo );
					hi = qMax( hi, ehi );
				}
			}
			if ( cellFlags[s] & CELL_HAS_LAND ) {
				if ( src.hasColour && src.hasColour( cx, cy ) )
					colourCells++;
				for ( int q = 0; q < 4; q++ ) {
					quint32 qf[6] = { 0, 0, 0, 0, 0, 0 };
					src.quadForms( cx, cy, q, qf );
					const size_t qi = ( size_t( ( cy - minY ) * 2 + ( q >> 1 ) ) * quadsX
						+ size_t( ( cx - minX ) * 2 + ( q & 1 ) ) ) * 6;
					for ( int k = 0; k < 6; k++ )
						quadSlots[qi + size_t( k )] = ltexSlot( qf[k] );

					if ( src.quadGcvr ) {
						quint32 gf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
						const int n = src.quadGcvr( cx, cy, q, gf );
						if ( n > 0 && gcvrSlots.empty() )
							gcvrSlots.assign( size_t( quadsX ) * quadsY * 8, 0xFFFFU );
						for ( int k = 0; k < n && k < 8; k++ ) {
							if ( !gf[k] )
								continue;
							auto it = gcvrIndex.constFind( gf[k] );
							int idx;
							if ( it == gcvrIndex.constEnd() ) {
								idx = gcvrForms.size();
								gcvrForms.append( gf[k] );
								gcvrIndex.insert( gf[k], idx );
							} else {
								idx = it.value();
							}
							const size_t gq = ( size_t( ( cy - minY ) * 2 + ( q >> 1 ) ) * quadsX
								+ size_t( ( cx - minX ) * 2 + ( q & 1 ) ) ) * 8;
							gcvrSlots[gq + size_t( k )] = quint16( idx );
						}
					}
				}
			}
			cellMinH[s] = lo;
			cellMaxH[s] = hi;
			worldMin = qMin( worldMin, lo );
			worldMax = qMax( worldMax, hi );
			/* Decode the planes NOW, while the source's memo still holds this
			 * cell and its seam neighbours from range() and quadForms(): the
			 * decode costs no parse. Only when the cache can keep the whole
			 * world -- otherwise it would be evicted before any pass read it. */
			if ( ( cellFlags[s] & CELL_HAS_LAND )
				&& qint64( CACHE_CELLS ) >= qint64( cellsX ) * qint64( cellsY ) )
				planesFor( cx, cy );

			float wh = 0.0f;
			quint32 wt = 0;
			if ( src.water && src.water( cx, cy, wh, wt ) ) {
				waterCells++;
				cellFlags[s] |= CELL_HAS_WATER;
				cellWaterH[s] = wh;
				/* The type is a form ID; the file stores an index into a table
				 * of them, which is the same idiom LTEX and GCVR use. The
				 * worldspace default is 0xFFFF rather than an entry, so a
				 * reader can tell "inherited" from "explicitly this". */
				if ( wt && wt != src.defaultWaterType ) {
					auto it = watrIndex.constFind( wt );
					int idx;
					if ( it == watrIndex.constEnd() ) {
						idx = watrForms.size();
						watrForms.append( wt );
						watrIndex.insert( wt, idx );
					} else {
						idx = it.value();
					}
					cellWaterT[s] = quint16( idx );
				}
			}
		}
	}
	if ( !landCells )
		return fail( QStringLiteral( "worldspace has no LAND records" ) );
	tPass1 = tm.restart();

	/* A coarse height grid at k samples a cell, filled cell by cell so each
	 * cell is decoded once. Used for the overview and for AO -- which at the
	 * default 8 a cell are the SAME grid. AO used to march through sampleAt
	 * instead: its reach of +-2 cell rows over a 192-cell-wide raster is a
	 * working set of ~960 cells, the cache held 512, and every texel row
	 * re-decoded its cells: 1,055,205 decodes and 61 of the writer's 75
	 * seconds for a 13 ms computation. */
	auto buildCoarse = [&]( int k ) {
		std::vector<quint16> g( size_t( cellsX ) * k * size_t( cellsY ) * k, 32767 );
		const int step = qMax( 1, spc / k );
		for ( int cy = minY; cy <= maxY; cy++ ) {
			for ( int cx = minX; cx <= maxX; cx++ ) {
				const size_t s = size_t( cy - minY ) * cellsX + size_t( cx - minX );
				if ( !( cellFlags[s] & CELL_HAS_LAND ) )
					continue;
				for ( int j = 0; j < k; j++ ) {
					for ( int i = 0; i < k; i++ ) {
						const size_t row = size_t( cy - minY ) * k + size_t( j );
						const size_t col = size_t( cx - minX ) * k + size_t( i );
						g[row * ( size_t( cellsX ) * k ) + col] =
							sampleAt( 0, ( cx - minX ) * spc + i * step,
								( cy - minY ) * spc + j * step );
					}
				}
			}
		}
		return g;
	};
	std::vector<quint16> overview;
	if ( ov > 0 )
		overview = buildCoarse( ov );
	std::vector<quint16> aoGridOwn;
	if ( aoS > 0 && aoS != ov )
		aoGridOwn = buildCoarse( aoS );
	const std::vector<quint16> & aoGrid = ( aoS > 0 && aoS == ov ) ? overview : aoGridOwn;

	// ---- assemble -------------------------------------------------------
	/* The written version. Two paths reach it: the caller's option, and
	 * WW_LODL_VERSION for a consumer that must be handed version 1 bytes
	 * without a rebuild. Anything else is a refusal rather than a silent
	 * downgrade -- a wrong version number is exactly the field that makes a
	 * reader misparse instead of refuse. */
	quint32 version = quint32( opts.headerVersion );
	/* The old spelling is REFUSED, not ignored. A run that still says
	 * WW_LODT_VERSION was written for the format this file used to be called,
	 * and silently writing version 2 bytes for it is the failure the rename
	 * was supposed to make impossible. */
	if ( qEnvironmentVariableIsSet( "WW_LODT_VERSION" ) )
		return fail( QStringLiteral( "WW_LODT_VERSION is retired: the landscape file "
			"is .lodl now (.lodt names the terrain texture sheets) -- set "
			"WW_LODL_VERSION instead" ) );
	if ( qEnvironmentVariableIsSet( "WW_LODL_VERSION" ) )
		version = quint32( qEnvironmentVariableIntValue( "WW_LODL_VERSION" ) );
	if ( version < LODL_VERSION_MIN || version > LODL_VERSION )
		return fail( QStringLiteral( "header version %1 is not one this writer knows (%2..%3)" )
			.arg( version ).arg( LODL_VERSION_MIN ).arg( LODL_VERSION ) );

	Buf h;
	h.u32( LODL_MAGIC );
	h.u32( version );
	h.i32( minX ); h.i32( minY ); h.i32( maxX ); h.i32( maxY );
	h.u32( quint32( spc ) );
	h.u32( quint32( opts.blockEdge ) );
	h.u32( quint32( opts.levelCount ) );
	h.f32( worldMin );
	h.f32( worldMax );
	h.f32( quantum );
	h.u32( quint32( ltexForms.size() ) );
	h.u32( quint32( watrForms.size() ) );
	h.u32( quint32( gcvrForms.size() ) );
	h.u32( quint32( aoS ) );
	h.u32( quint32( ov ) );
	h.u32( ( waterCells ? SECT_WATER : 0u )
		| ( colourCells ? SECT_COLOUR : 0u )
		| ( gcvrForms.isEmpty() ? 0u : SECT_GROUNDCOVER )
		| ( opts.aoSamples > 0 ? SECT_AO : 0u ) );
	const qsizetype offLtexAt = h.size();     h.u64( 0 );
	const qsizetype offWatrAt = h.size();     h.u64( 0 );
	const qsizetype offGcvrAt = h.size();     h.u64( 0 );
	const qsizetype offQuadAt = h.size();     h.u64( 0 );
	const qsizetype offCellAt = h.size();     h.u64( 0 );
	const qsizetype offOverAt = h.size();     h.u64( 0 );
	const qsizetype offAoAt   = h.size();     h.u64( 0 );
	const qsizetype offDirAt  = h.size();     h.u64( 0 );
	const qsizetype offDataAt = h.size();     h.u64( 0 );
	const qsizetype offSizeAt = h.size();     h.u64( 0 );
	if ( version >= 2 ) {
		/* The worldspace's own water. Without these two fields the per-cell
		 * "water type = 0xFFFF, meaning the worldspace default" is a promise
		 * the file cannot keep: the default's WATR form appears in no table,
		 * because the writer deliberately does not intern it (that is what
		 * makes 0xFFFF distinguishable from "explicitly this type"). The
		 * height is here too, so a consumer can tell an inheriting cell's
		 * plane from the file alone -- the per-cell water height is already
		 * resolved, so the two must agree and now visibly do. */
		h.f32( src.defaultWaterHeight );
		h.u32( src.defaultWaterType );
	}
	/* The header size is what every section offset is measured against, so it
	 * is checked here rather than asserted in a debug build nobody runs. */
	if ( h.size() != ( version >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1 ) )
		return fail( QStringLiteral( "header assembled to %1 bytes, not the %2 version %3 declares" )
			.arg( h.size() ).arg( version >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1 ).arg( version ) );

	/* Streamed to disk section by section. An earlier draft assembled the
	 * whole file in one QByteArray and wrote it at the end: 2.5 GB resident
	 * for Appalachia's 1.55 GB file, ~4 GB for the 804-cell target. Now only
	 * the header (152 bytes at version 1, 160 at version 2, patched with
	 * each section's offset as it is
	 * reached), the block directory (16 bytes a block) and the one block
	 * being compressed are in memory. The directory's space is reserved
	 * ahead of the payloads and filled by a seek at the end, so the bytes on
	 * disk are identical to what the in-memory draft produced -- checked by
	 * hash, not assumed. */
	const QString dir = outDir + QStringLiteral( "/Terrain" );
	QDir().mkpath( dir );
	const QString path = dir + QStringLiteral( "/" ) + edid + QStringLiteral( ".lodl" );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QStringLiteral( "could not open %1" ).arg( path ) );
	QByteArray hdr = h.b;
	quint64 pos = 0;
	bool ioFail = false;
	auto wr = [&]( const QByteArray & x ) {
		QElapsedTimer ti;
		ti.start();
		if ( f.write( x ) != x.size() )
			ioFail = true;
		nsIo += ti.nsecsElapsed();
		pos += quint64( x.size() );
	};
	wr( hdr );   // placeholder; rewritten with the real offsets at the end
	auto append = [&]( const Buf & x ) { wr( x.b ); };

	patch64( hdr, offLtexAt, pos );
	{
		Buf t;
		for ( quint32 f : ltexForms )
			t.u32( f );
		append( t );
	}

	patch64( hdr, offWatrAt, pos );
	{
		Buf t;
		for ( quint32 f : watrForms )
			t.u32( f );
		append( t );
	}

	/* GCVR: the form-ID table, then - contiguously, present only when the table
	 * is non-empty - uint16[8] slots a quadrant, the same shape as the LTEX
	 * slots. FO4 has no ground cover, so for a .esm source both are empty. */
	patch64( hdr, offGcvrAt, pos );
	{
		Buf t;
		for ( quint32 f : gcvrForms )
			t.u32( f );
		for ( size_t s = 0; s < gcvrSlots.size(); s++ )
			t.u16( gcvrSlots[s] );
		append( t );
	}

	/* The quadrant slot table is GLOBAL, not per block -- exactly as FO76 does
	 * it, and for a reason that only shows up once blocks span more than one
	 * cell: a level-3 block covers 8x8 cells, so 256 quadrants, and a
	 * four-entry table in its header could not describe them. Slot assignment
	 * is a property of the world, not of a LOD level. */
	patch64( hdr, offQuadAt, pos );
	{
		Buf t;
		for ( size_t s = 0; s < quadSlots.size(); s++ )
			t.u16( quadSlots[s] );
		append( t );
	}

	patch64( hdr, offCellAt, pos );
	{
		Buf t;
		for ( size_t s = 0; s < cellMinH.size(); s++ ) {
			t.f32( cellMinH[s] );
			t.f32( cellMaxH[s] );
			t.f32( cellWaterH[s] );
			t.u16( cellWaterT[s] );
			t.u16( cellFlags[s] );
		}
		append( t );
	}

	patch64( hdr, offOverAt, pos );
	{
		Buf t;
		for ( quint16 v : overview )
			t.u16( v );
		append( t );
	}
	/* ---- the block pyramid -------------------------------------------- */
	/* AO: a FLAT coarse plane, not blocks. The spec first called for blocked AO
	 * mirroring the terrain pyramid, but at 8 samples a cell the whole
	 * Commonwealth is 2.4 MB - small enough that a block directory, a
	 * compression pass and a second addressing scheme would all be overhead for
	 * nothing. Coarse by design: AO is smooth and has nothing sharp to lose.
	 */
	tOver = tm.restart();
	patch64( hdr, offAoAt, pos );
	if ( aoS > 0 ) {
		Buf t;
		t.b = lodtComputeAo( aoGrid, cellsX * aoS, cellsY * aoS, aoS, quantum );
		append( t );
	}
	tAo = tm.restart();

	const int be = qMax( 1, opts.blockEdge );
	const int levels = qMax( 1, opts.levelCount );
	auto blocksX = [&]( int L ) { return ( cellsX + ( 1 << L ) - 1 ) >> L; };
	auto blocksY = [&]( int L ) { return ( cellsY + ( 1 << L ) - 1 ) >> L; };
	// sample at level L, in level-L coordinates, clamped to the grid
	auto at = [&]( int plane, int L, int x, int y ) {
		const int step = 1 << L;
		return sampleAt( plane, x * step, y * step );
	};

	/* Directory and payload are built together, COARSEST level first, so the
	 * levels a streaming reader always needs are contiguous at the front. */
	int nBlocks = 0;
	for ( int L = 0; L < levels; L++ )
		nBlocks += blocksX( L ) * blocksY( L );
	patch64( hdr, offDirAt, pos );
	const quint64 dirStart = pos;
	{
		// reserve the directory; filled by a seek once every payload offset is known
		const QByteArray zeros( 1 << 20, char( 0 ) );
		qint64 left = qint64( nBlocks ) * 16;
		while ( left > 0 ) {
			const qint64 n = qMin( left, qint64( zeros.size() ) );
			wr( zeros.left( n ) );
			left -= n;
		}
	}
	patch64( hdr, offDataAt, pos );
	Buf dirBuf;
	int blocksDone = 0;
	bool cancelled = false;
	/* Blocks are compressed in parallel batches and written in order. zlib -9
	 * on the Commonwealth's 48,960 blocks is 1.7 s single-threaded; the
	 * batch is large enough that spawning a pool per batch is noise. The
	 * directory entry is made at write time, so offsets are exactly what a
	 * serial writer would have produced. */
	std::vector<QByteArray> rawBatch;
	rawBatch.reserve( 256 );
	auto flushBatch = [&]() {
		if ( rawBatch.empty() )
			return;
		QElapsedTimer tz;
		tz.start();
		std::vector<QByteArray> zs( rawBatch.size() );
		const int nThreads = qBound( 1, int( std::thread::hardware_concurrency() ), int( rawBatch.size() ) );
		std::atomic<size_t> next( 0 );
		std::vector<std::thread> pool;
		for ( int t = 0; t < nThreads; t++ ) {
			pool.emplace_back( [&]() {
				for ( size_t k; ( k = next.fetch_add( 1 ) ) < rawBatch.size(); ) {
					/* qCompress prefixes a 4-byte size; dropping it leaves a plain
					 * zlib stream, so no consumer needs Qt to inflate a block. */
					QByteArray z = qCompress( rawBatch[k], 9 );
					z.remove( 0, 4 );
					zs[k] = z;
				}
			} );
		}
		for ( auto & th : pool )
			th.join();
		nsZip += tz.nsecsElapsed();
		for ( size_t k = 0; k < zs.size(); k++ ) {
			dirBuf.u64( pos );                        // absolute: the payload goes out right now
			dirBuf.u32( quint32( zs[k].size() ) );
			dirBuf.u32( quint32( rawBatch[k].size() ) );
			wr( zs[k] );
		}
		rawBatch.clear();
	};
	const int coarsest = levels - 1;
	for ( int L = coarsest; L >= 0; L-- ) {
		curLevel = qMin( L, 6 );
		const int bx = blocksX( L ), by = blocksY( L );
		for ( int j = 0; j < by; j++ ) {
			for ( int i = 0; i < bx; i++ ) {
				Buf blk;
				const int ox = i * be, oy = j * be;
				/* Heights, then alphas, then colour -- each stored the same
				 * way, so one helper covers all three. */
				// NOT named "emit": that is a Qt macro and expands to nothing.
				auto emitPlane = [&]( int plane ) {
					if ( L == coarsest ) {
						// the only level stored in full; everything finer is a delta
						for ( int y = 0; y < be; y++ )
							for ( int x = 0; x < be; x++ )
								blk.u16( at( plane, L, ox + x, oy + y ) );
					} else {
						/* Only the samples the coarser parent lacks: per parent
						 * sample, the three at right, below and below-right.
						 * 3/4 of the grid, nothing stored twice in the pyramid. */
						for ( int y = 0; y < be; y += 2 ) {
							for ( int x = 0; x < be; x += 2 ) {
								blk.u16( at( plane, L, ox + x + 1, oy + y ) );
								blk.u16( at( plane, L, ox + x, oy + y + 1 ) );
								blk.u16( at( plane, L, ox + x + 1, oy + y + 1 ) );
							}
						}
					}
				};
				emitPlane( 0 );
				emitPlane( 1 );
				if ( colourCells )
					emitPlane( 2 );
				if ( !gcvrForms.isEmpty() )
					emitPlane( 3 );
				rawBatch.push_back( blk.b );
				if ( rawBatch.size() >= 256 )
					flushBatch();
				blocksDone++;
				if ( opts.progress && !opts.progress( 1, blocksDone, nBlocks, L, i, j ) ) {
					cancelled = true;
					break;
				}
			}
			if ( cancelled )
				break;
		}
		if ( cancelled )
			break;
		flushBatch();   // a level's blocks stay contiguous: the directory order is the file order
		tLev[qMin( L, 7 )] = tm.restart();
	}
	if ( cancelled ) {
		f.close();
		QFile::remove( path );
		return fail( QStringLiteral( "cancelled" ) );
	}

	patch64( hdr, offSizeAt, pos );
	if ( !f.seek( qint64( dirStart ) ) || f.write( dirBuf.b ) != dirBuf.b.size()
		|| !f.seek( 0 ) || f.write( hdr ) != hdr.size() )
		ioFail = true;
	f.close();
	if ( ioFail )
		return fail( QStringLiteral( "short write to %1" ).arg( path ) );

	if ( outPath )
		*outPath = path;
	if ( error )
		*error = QString( "v%19 cells %1x%2, land %3, water %4 "
			"(worldspace default height %20 type %21), WATR types %5, "
			"height %6..%7, overview %8^2\n"
			"  timing: pass one %9 ms (decodes included when the world fits), coarse+sections %10 ms, AO %11 ms, levels %12 ms "
			"(zlib %13 ms, io %14 ms), %15 cell decodes for %16 cells; per level %17, null-plane calls %18" )
			.arg( cellsX ).arg( cellsY ).arg( landCells ).arg( waterCells )
			.arg( watrForms.size() ).arg( double( worldMin ) ).arg( double( worldMax ) )
			.arg( cellsX * ov )
			.arg( tPass1 ).arg( tOver ).arg( tAo )
			.arg( [&] { QStringList l; for ( int i = levels - 1; i >= 0; i-- ) l << QString::number( tLev[qMin( i, 7 )] ); return l.join( '+' ); }() )
			.arg( nsZip / 1000000 ).arg( nsIo / 1000000 )
			.arg( decodes ).arg( qint64( cellsX ) * cellsY )
			.arg( [&] { QStringList l; for ( int i = 7; i >= 0; i-- ) l << QString::number( decodesAtLevel[i] ); return l.join( '/' ); }() )
			.arg( nullPlanes )
			.arg( version ).arg( double( src.defaultWaterHeight ) )
			.arg( src.defaultWaterType, 8, 16, QChar( '0' ) );
	return true;
}

// ============================ reader =====================================

namespace
{
//! Read a little-endian scalar out of a byte buffer.
template <typename T> T rd( const QByteArray & b, qsizetype at )
{
	T v = T( 0 );
	if ( at < 0 || at + qsizetype( sizeof( T ) ) > b.size() )
		return v;
	std::memcpy( &v, b.constData() + at, sizeof( T ) );
	return v;
}
}

/* ---- source 1: Fallout 4's LAND records ------------------------------- */

bool lodtWrite( const EsmWorld & world, const QString & outDir,
	const LodtOptions & opts, QString * outPath, QString * error )
{
	/* Pass one asks a cell for its range, its colour flag and its four
	 * quadrants' textures, and the pyramid then asks for its planes. Without a
	 * memo that is six LAND parses a cell; the record is delta-coded, so each
	 * one is a full decode. One entry is enough - every caller works a cell to
	 * completion before moving on. */
	struct Memo
	{
		qint64 key = std::numeric_limits<qint64>::min();
		bool ok = false;
		EsmLand land;
	};
	/* Four entries, not one: the seam rule reads a cell's south, west and
	 * south-west neighbours, and with a single entry each of those would
	 * evict the cell itself under a pointer still in use. Round-robin, so a
	 * pointer stays valid for the next three loads -- callers fetch the
	 * neighbours first and the cell LAST. */
	struct MemoSet
	{
		Memo e[4];
		int next = 0;
	};
	auto memo = std::make_shared<MemoSet>();
	auto get = [&world, memo]( int cx, int cy ) -> const EsmLand * {
		const qint64 key = ( qint64( cx ) << 32 ) | qint64( quint32( cy ) );
		for ( Memo & m : memo->e )
			if ( m.key == key )
				return m.ok ? &m.land : nullptr;
		Memo & m = memo->e[memo->next];
		memo->next = ( memo->next + 1 ) & 3;
		m.key = key;
		m.ok = world.land( cx, cy, m.land );
		return m.ok ? &m.land : nullptr;
	};

	/* The seam rule. VHGT stores 33 rows and columns; row 32 and column 32
	 * are the north and east neighbours' row 0 and column 0, so a cell's
	 * row 0 and column 0 are also held by the cell to its south, its west,
	 * and (the corner) its south-west. This file keeps 32 a cell, so each of
	 * those samples has ONE slot, and where the records disagree the slot
	 * takes the MAXIMUM over every cell holding it.
	 *
	 * Measured, not chosen, and then decided: 439 Commonwealth cells at the
	 * world's rim are flat -352 filler whose edges disagree with the real
	 * terrain next door. Against a full dump of the ESM over 2,322,432 edge
	 * texels the maximum reproduced the Commonwealth_fine shadow heightmap
	 * with 0 mismatches, where "the cell's own row 0" missed 8,675. The
	 * heightmap and this file will be read as ONE surface -- terrain from
	 * here, far shadows from there -- and a row they disagree on is a ridge
	 * that casts a shadow without being drawn. bungo's call, 2026-09-05:
	 * max in both. */
	struct Seam
	{
		bool s = false, w = false, sw = false;
		float sRow[33] = {};
		float wCol[33] = {};
		float swC = 0.0f;
	};
	auto seams = [get]( int cx, int cy, Seam & o ) {
		if ( const EsmLand * n = get( cx, cy - 1 ) ) {
			o.s = true;
			for ( int c = 0; c < 33; c++ )
				o.sRow[c] = n->heights[32][c];
		}
		if ( const EsmLand * n = get( cx - 1, cy ) ) {
			o.w = true;
			for ( int r = 0; r < 33; r++ )
				o.wCol[r] = n->heights[r][32];
		}
		if ( const EsmLand * n = get( cx - 1, cy - 1 ) ) {
			o.sw = true;
			o.swC = n->heights[32][32];
		}
	};

	LodtSource src;
	src.edid = world.worldspaceEdid();
	world.cellBounds( src.minX, src.minY, src.maxX, src.maxY );
	src.spc = opts.samplesPerCell > 0 ? opts.samplesPerCell : 32;
	src.defaultLand = world.defaultLandHeight();
	src.defaultWaterType = world.defaultWaterType();
	src.defaultWaterHeight = world.defaultWaterHeight();

	src.range = [get, seams]( int cx, int cy, float & lo, float & hi ) {
		if ( !get( cx, cy ) )
			return false;
		Seam sm;
		seams( cx, cy, sm );
		const EsmLand * l = get( cx, cy );   // after the neighbours: see MemoSet
		lo = 3.4e38f;
		hi = -3.4e38f;
		// 33, not 32: VHGT carries the row and column shared with the neighbour
		for ( int r = 0; r < 33; r++ )
			for ( int c = 0; c < 33; c++ ) {
				lo = qMin( lo, l->heights[r][c] );
				hi = qMax( hi, l->heights[r][c] );
			}
		/* The stored row 0 / column 0 may be RAISED by the seam rule above
		 * anything in this record; a per-cell max that missed that would let
		 * a renderer cull the raised row. Only the max can move: max(a, b)
		 * is never below a. */
		for ( int c = 0; c < 32; c++ )
			if ( sm.s )
				hi = qMax( hi, sm.sRow[c] );
		for ( int r = 0; r < 32; r++ )
			if ( sm.w )
				hi = qMax( hi, sm.wCol[r] );
		if ( sm.sw )
			hi = qMax( hi, sm.swC );
		return true;
	};
	src.hasColour = [get]( int cx, int cy ) {
		const EsmLand * l = get( cx, cy );
		return l && l->hasColors;
	};
	src.quadForms = [get]( int cx, int cy, int q, quint32 * out ) {
		const EsmLand * l = get( cx, cy );
		if ( !l )
			return;
		/* Five slots for what may be more layers, so rank by PEAK opacity and
		 * keep the strongest -- what is dropped is the least visible by
		 * construction. 89.5% of Commonwealth quadrants have no layers at all
		 * and 99.27% fit in five. */
		QVector<QPair<float, quint32>> rank;
		for ( const EsmLandLayer & ly : l->layers[q] ) {
			float peak = 0.0f;
			for ( int r = 0; r < 17; r++ )
				for ( int c = 0; c < 17; c++ )
					peak = qMax( peak, ly.opacity[r][c] );
			rank.append( qMakePair( peak, ly.ltex ) );
		}
		std::sort( rank.begin(), rank.end(),
			[]( const QPair<float, quint32> & a, const QPair<float, quint32> & bb ) {
				return a.first > bb.first;
			} );
		for ( int k = 0; k < 5 && k < rank.size(); k++ )
			out[k] = rank[k].second;
		out[5] = l->baseTex[q];
	};
	/*! What a landless cell inherits, for the per-cell range. The same three
	 *  neighbours the seam rule reads, and nothing else can reach it. */
	src.edgeRange = [get, seams]( int cx, int cy, float & lo, float & hi ) {
		if ( get( cx, cy ) )
			return false;                    // it has land: not this rule's business
		Seam sm;
		seams( cx, cy, sm );
		if ( !sm.s && !sm.w && !sm.sw )
			return false;
		lo = 3.4e38f;
		hi = -3.4e38f;
		auto take = [&lo, &hi]( float v ) { lo = qMin( lo, v ); hi = qMax( hi, v ); };
		for ( int c = 0; c < 32; c++ )
			if ( sm.s )
				take( sm.sRow[c] );
		for ( int r = 0; r < 32; r++ )
			if ( sm.w )
				take( sm.wCol[r] );
		if ( sm.sw )
			take( sm.swC );
		return true;
	};
	src.planes = [get, seams, defaultLand = world.defaultLandHeight()](
		int cx, int cy, const quint32 * slotForms, float quantum,
		std::vector<quint16> & h, std::vector<quint16> & a,
		std::vector<quint16> & c, std::vector<quint16> & g )
	{
		Seam sm;
		seams( cx, cy, sm );
		const EsmLand * l = get( cx, cy );   // after the neighbours: see MemoSet
		const int spc = 32;
		if ( !l ) {
			/* NO LAND RECORD, and the cell to the south or the west has one.
			 * VHGT's row 32 / column 32 IS this cell's row 0 / column 0, so
			 * that row is real terrain -- and it was being written as flat
			 * ZERO, because the plane was refused outright and the caller
			 * substituted a bare 32767 sentinel.
			 *
			 * MEASURED against the shadow heightmaps, which apply the seam
			 * maximum whether or not the sample's own cell exists and which
			 * reproduce Bethesda's Commonwealth_fine byte for byte:
			 * DLC03FarHarbor 62 texels differed (ONE landless cell ringed by
			 * eight that have land), NukaWorldAmphitheater 97 over four cells,
			 * DiamondCity 167,936 of 172,032 -- 164 landless cells whose whole
			 * interior read 0 against that worldspace's default land height of
			 * -2048. The Commonwealth is fully dense, has not one landless
			 * cell, and stayed byte-identical throughout: that is why nothing
			 * caught this.
			 *
			 * The two files are read as ONE surface -- terrain from here, far
			 * shadows from the heightmap -- so a sample they disagree on is the
			 * ridge-that-casts-a-shadow-without-being-drawn of 2026-09-05c,
			 * one row in from a hole in the landscape.
			 *
			 * The default height does NOT take part in the maximum on an
			 * inherited sample, exactly as it does not in the heightmap: Far
			 * Harbor's default is 0 and its inherited row is around -250, and
			 * a max against the default would have kept every one of those 62
			 * texels wrong. */
			if ( !sm.s && !sm.w && !sm.sw )
				return false;                // nothing inherited: the caller's default
			h.assign( size_t( spc ) * spc, 0 );
			a.assign( size_t( spc ) * spc, 0 );
			c.assign( size_t( spc ) * spc, 0xFFFFU );
			g.clear();
			for ( int r = 0; r < spc; r++ ) {
				for ( int cc = 0; cc < spc; cc++ ) {
					float hh = defaultLand;
					bool inherited = false;
					auto take = [&hh, &inherited]( float v ) {
						hh = inherited ? qMax( hh, v ) : v;
						inherited = true;
					};
					if ( r == 0 && sm.s )
						take( sm.sRow[cc] );
					if ( cc == 0 && sm.w )
						take( sm.wCol[r] );
					if ( r == 0 && cc == 0 && sm.sw )
						take( sm.swC );
					h[size_t( r ) * size_t( spc ) + size_t( cc )] =
						lodtHeightWord( hh, quantum );
				}
			}
			return true;
		}
		h.assign( size_t( spc ) * spc, 32767 );
		a.assign( size_t( spc ) * spc, 0 );
		c.assign( size_t( spc ) * spc, 0xFFFFU );
		/* FO4 has no ground cover HERE: measured, Fallout4.esm carries 0 GCVR
		 * records. Its ground cover is the LTEX -> GNAM -> GRAS chain, and
		 * since 2026-09-06 it lives in the ALPHA of the per-chunk
		 * <ws>.<dim>.<x>.<y>_data.DDS and in the data sheet of a .lodv tile
		 * (docs/LODGEN_TERRAIN_VT.md) -- not in this section, which stays
		 * Fallout 76's. */
		g.clear();
		for ( int r = 0; r < spc; r++ ) {
			for ( int cc = 0; cc < spc; cc++ ) {
				const size_t k = size_t( r ) * size_t( spc ) + size_t( cc );
				float hh = l->heights[r][cc];
				if ( r == 0 && sm.s )
					hh = qMax( hh, sm.sRow[cc] );
				if ( cc == 0 && sm.w )
					hh = qMax( hh, sm.wCol[r] );
				if ( r == 0 && cc == 0 && sm.sw )
					hh = qMax( hh, sm.swC );
				h[k] = lodtHeightWord( hh, quantum );

				const int q = ( r >= 16 ? 2 : 0 ) + ( cc >= 16 ? 1 : 0 );
				const int qr = r >= 16 ? r - 16 : r;
				const int qc = cc >= 16 ? cc - 16 : cc;
				quint16 word = 0;
				for ( int s = 0; s < 5; s++ ) {
					const quint32 form = slotForms[q * 6 + s];
					if ( !form )
						continue;
					float alpha = 0.0f;
					for ( const EsmLandLayer & ly : l->layers[q] )
						if ( ly.ltex == form ) {
							alpha = ly.opacity[qr][qc];
							break;
						}
					word |= quint16( quint16( qBound( 0.0f, alpha * 7.0f + 0.5f, 7.0f ) )
						<< ( s * 3 ) );
				}
				a[k] = word;
				if ( l->hasColors )
					c[k] = quint16( ( ( l->colors[r][cc][0] >> 3 ) << 11 )
						| ( ( l->colors[r][cc][1] >> 3 ) << 6 )
						| ( l->colors[r][cc][2] >> 3 ) );
			}
		}
		return true;
	};
	src.water = [&world]( int cx, int cy, float & wh, quint32 & wt ) {
		return world.cellWater( cx, cy, wh, &wt );
	};

	return lodtWriteSource( src, outDir, opts, outPath, error );
}

/* ---- source 2: a Fallout 76 .btd ------------------------------------- */

bool lodtWriteBtd( const QString & btdPath, const QString & outDir,
	const LodtOptions & opts, QString * outPath, QString * error, QString * notes,
	const EsmWorld * waterFrom )
{
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};

	std::shared_ptr<BTDFile> btd;
	try {
		btd = std::make_shared<BTDFile>( btdPath.toLocal8Bit().constData() );
	} catch ( const std::exception & e ) {
		return fail( QStringLiteral( "cannot read .btd: %1" )
			.arg( QString::fromUtf8( e.what() ) ) );
	}
	/* A .btd tile is 8x8 CELLS, and every pass over the landscape here walks
	 * cells row-major -- pass one, the overview, the AO plane and each level of
	 * the pyramid. So a cache that cannot hold a full row of tiles evicts the
	 * row it is about to need again, and each tile is re-inflated once per cell
	 * row it spans: eight times over.
	 *
	 * Measured on Appalachia at the stock 16 tiles: 825 CPU seconds without
	 * finishing pass one, memory flat, because essentially all of it was
	 * repeated zlib. Size the cache to the world instead. A tile is ~5 MiB, so
	 * the cap is what keeps a very wide worldspace from trading this for
	 * swapping. */
	const int tilesPerRow = ( btd->getCellMaxX() - btd->getCellMinX() + 8 ) / 8;
	btd->setTileCacheSize( size_t( qBound( 16, tilesPerRow + 2, 96 ) ) );

	const float hMin = btd->getMinHeight();
	const float hMax = btd->getMaxHeight();
	if ( !( hMax > hMin ) )
		return fail( QStringLiteral( ".btd reports an empty height range" ) );

	/* Their heights are normalised 0..65535 across the WHOLE world; ours are a
	 * fixed-point count of `quantum` world units biased by 32767. So the
	 * quantum is not a constant here -- it is the finest power of two that
	 * still lets the tallest point fit. For Appalachia that lands far finer
	 * than FO4's 8, which is why the quantum is a header field. */
	/* The binding constraint is the FIXED 32767 bias: the tallest point has to
	 * land inside an int16's worth of steps either side of it, so the finest
	 * legal quantum is exactly maxAbs/32767 and there is no reason to take a
	 * coarser one. (An earlier draft rounded up to a power of two for
	 * tidiness and paid 1.6x the quantisation error for it.) The source
	 * resolves (hMax-hMin)/65535, so on a range centred near zero this is the
	 * same step and only the half-step grid offset is lost. */
	const float maxAbs = qMax( qAbs( hMin ), qAbs( hMax ) );
	const float quantum = qMax( maxAbs / 32767.0f, 1.0f / 4096.0f );

	LodtOptions o = opts;
	o.heightQuantum = quantum;
	/* 128, from the .btd itself. A block must cover exactly one cell at the
	 * finest level or the pyramid's level maths stops lining up with cells. */
	o.samplesPerCell = 128;
	o.blockEdge = 128;

	struct Stats
	{
		qint64 alphaSamples = 0, alphaOverflow = 0, colourSamples = 0;
		double sumR = 0.0, sumG = 0.0, sumB = 0.0;
		QSet<quint16> distinct;
	};
	auto st = std::make_shared<Stats>();

	struct Memo
	{
		int cx = 0x7FFFFFFF, cy = 0x7FFFFFFF;
		std::vector<quint16> h, a, vclr;
		std::vector<unsigned char> gc;
		unsigned char tset[64] = { 0 };
	};
	auto memo = std::make_shared<Memo>();
	auto load = [btd, memo]( int cx, int cy ) -> Memo * {
		if ( memo->cx == cx && memo->cy == cy )
			return memo.get();
		if ( cx < btd->getCellMinX() || cx > btd->getCellMaxX()
			|| cy < btd->getCellMinY() || cy > btd->getCellMaxY() )
			return nullptr;
		memo->cx = cx;
		memo->cy = cy;
		memo->h.resize( size_t( 128 ) * 128 );
		memo->a.resize( size_t( 128 ) * 128 );
		memo->gc.resize( size_t( 128 ) * 128 );
		memo->vclr.resize( size_t( 32 ) * 32 );
		btd->getCellHeightMap( memo->h.data(), cx, cy, 0 );
		btd->getCellLandTexture( memo->a.data(), cx, cy, 0 );
		btd->getCellGroundCover( memo->gc.data(), cx, cy, 0 );
		// terrain colour only exists from LOD2 down: 32x32, upsampled 4x below
		btd->getCellTerrainColor( memo->vclr.data(), cx, cy, 2 );
		btd->getCellTextureSet( memo->tset, cx, cy );
		return memo.get();
	};

	// double, not float: at 38,000 units a float32 ulp is 0.004, which is a
	// visible fraction of a 1.17 quantum's half-step round-trip bound
	auto toWorld = [hMin, hMax]( quint16 v ) {
		return double( hMin ) + ( double( v ) / 65535.0 ) * ( double( hMax ) - double( hMin ) );
	};

	LodtSource src;
	src.edid = QFileInfo( btdPath ).completeBaseName();
	src.minX = btd->getCellMinX();
	src.minY = btd->getCellMinY();
	src.maxX = btd->getCellMaxX();
	src.maxY = btd->getCellMaxY();
	src.spc = 128;
	src.defaultLand = hMin;
	src.defaultWaterType = 0;

	src.range = [load, toWorld]( int cx, int cy, float & lo, float & hi ) {
		Memo * m = load( cx, cy );
		if ( !m )
			return false;
		quint16 a = 0xFFFFU, z = 0;
		for ( quint16 v : m->h ) {
			a = qMin( a, v );
			z = qMax( z, v );
		}
		lo = float( toWorld( a ) );
		hi = float( toWorld( z ) );
		return true;
	};
	src.hasColour = [load]( int cx, int cy ) {
		Memo * m = load( cx, cy );
		if ( !m )
			return false;
		for ( quint16 v : m->vclr )
			if ( v != 0xFFFFU )
				return true;
		return false;
	};
	src.quadForms = [btd, load]( int cx, int cy, int q, quint32 * out ) {
		Memo * m = load( cx, cy );
		if ( !m )
			return;
		/* getCellTextureSet lays a quadrant out as t[0] = base texture and
		 * t[1..5] = its five layers, which is exactly our six slots. Those ids
		 * index the .btd's own table; the writer wants form IDs. */
		const unsigned char * t = m->tset + ( q << 4 );
		for ( int k = 0; k < 5; k++ )
			out[k] = t[k + 1] == 0xFF ? 0u : btd->getLandTexture( t[k + 1] );
		out[5] = t[0] == 0xFF ? 0u : btd->getLandTexture( t[0] );
	};
	src.quadGcvr = [btd, load]( int cx, int cy, int q, quint32 * out ) {
		Memo * m = load( cx, cy );
		if ( !m )
			return 0;
		const unsigned char * g = m->tset + ( q << 4 ) + 8;
		int n = 0;
		for ( int k = 0; k < 8; k++ ) {
			out[k] = g[k] == 0xFF ? 0u : btd->getGroundCover( g[k] );
			if ( out[k] )
				n = k + 1;
		}
		return n;
	};
	src.planes = [load, toWorld, st]( int cx, int cy, const quint32 *, float quantum,
		std::vector<quint16> & h, std::vector<quint16> & a,
		std::vector<quint16> & c, std::vector<quint16> & g )
	{
		Memo * m = load( cx, cy );
		if ( !m )
			return false;
		h.resize( size_t( 128 ) * 128 );
		a.resize( size_t( 128 ) * 128 );
		c.assign( size_t( 128 ) * 128, 0xFFFFU );
		g.assign( size_t( 128 ) * 128, 0 );
		for ( size_t k = 0; k < size_t( 128 ) * 128; k++ ) {
			/* std::floor(v + 0.5), not a bare cast: casting truncates, which
			 * costs a WHOLE quantum of error instead of half. Invisible on the
			 * FO4 path, where height/8 is already integral, and measurable the
			 * moment a .btd is the source. */
			const double v = toWorld( m->h[k] ) / double( quantum ) + 32767.0;
			h[k] = quint16( qBound( 0.0, std::floor( v + 0.5 ), 65535.0 ) );

			/* The alpha word is copied VERBATIM: five 3-bit weights in a
			 * uint16, which is the storage both sides use.
			 *
			 * What is counted here is NOT an invariant, though an earlier draft
			 * treated it as one. The theory was that the five weights partition
			 * an implicit base's share and so cannot sum past 7. They can, and
			 * on Appalachia 72% of samples do: the layers are INDEPENDENT
			 * opacities composited in order over the base, exactly as FO4's own
			 * layers paint over one another, not a partition of unity.
			 *
			 * The number is kept because it says something real about a source
			 * -- but it is a statistic, not a gate. It read 0% on
			 * EXM1PittWorldspace only because that worldspace has no land
			 * textures at all, so every word was zero. A check that cannot fail
			 * on a degenerate input is not a check. */
			const quint16 w = m->a[k];
			int sum = 0;
			for ( int s = 0; s < 5; s++ )
				sum += ( w >> ( s * 3 ) ) & 7;
			st->alphaSamples++;
			if ( sum > 7 )
				st->alphaOverflow++;
			a[k] = w;

			g[k] = quint16( m->gc[k] );
		}
		for ( int y = 0; y < 128; y++ ) {
			for ( int x = 0; x < 128; x++ ) {
				const quint16 v = m->vclr[size_t( y >> 2 ) * 32 + size_t( x >> 2 )];
				/* A1R5G5B5 -- from libfo76utils' own codec for pixelFormatRGBA16
				 * (filebuf.cpp: rMask 0x7C00, gMask 0x03E0, bMask 0x001F, aMask
				 * 0x8000), NOT RGB565. A first draft assumed 565 and read the
				 * alpha bit plus four bits of red as "red"; the means it then
				 * reported were meaningless. Blue was right by coincidence. */
				const int r = ( v >> 10 ) & 0x1F;
				const int gg = ( v >> 5 ) & 0x1F;
				const int z = v & 0x1F;
				st->colourSamples++;
				st->sumR += r;
				st->sumG += gg;
				st->sumB += z;
				/* Means alone cannot tell a wrong channel order from a
				 * constant plane, so count distinct words too: one of them
				 * means the source has no colour here to get wrong. */
				if ( st->distinct.size() < 4096 )
					st->distinct.insert( v );
				// into our own 5-5-5 layout: R at 11, G at 6, B at 0, as the FO4 path packs VCLR
				c[size_t( y ) * 128 + size_t( x )] =
					quint16( ( r << 11 ) | ( gg << 6 ) | z );
			}
		}
		return true;
	};
	/* A .btd carries no water at all -- measured, not assumed. Fallout 76
	 * keeps it exactly where Fallout 4 does: XCLW / XCWT on the CELL and WATR
	 * records, in SeventySix.esm (53,185 XCLW, 879 XCWT, 47 WATR counted).
	 * So the plugin is the water source when one is given, through the same
	 * cellWater() the FO4 path uses; the terrain still comes from the .btd. */
	if ( waterFrom ) {
		src.defaultWaterType = waterFrom->defaultWaterType();
		src.defaultWaterHeight = waterFrom->defaultWaterHeight();
		src.water = [waterFrom]( int cx, int cy, float & wh, quint32 & wt ) {
			return waterFrom->cellWater( cx, cy, wh, &wt );
		};
	}

	if ( !lodtWriteSource( src, outDir, o, outPath, error ) )
		return false;

	if ( notes ) {
		const double ovf = st->alphaSamples
			? 100.0 * double( st->alphaOverflow ) / double( st->alphaSamples ) : 0.0;
		const double n = qMax( 1.0, double( st->colourSamples ) );
		*notes = QStringLiteral(
			"  quantum %1 world units, from declared height range %2..%3\n"
			"  alpha fields: %4% of %5 samples have five weights summing past 7 (statistic, not a gate)\n"
			"  colour (A1R5G5B5): mean R %6 G %7 B %8 of 31, %9 distinct words\n"
			"  water: %10" )
			.arg( quantum ).arg( hMin ).arg( hMax )
			.arg( ovf, 0, 'f', 3 ).arg( st->alphaSamples )
			.arg( st->sumR / n, 0, 'f', 1 ).arg( st->sumG / n, 0, 'f', 1 )
			.arg( st->sumB / n, 0, 'f', 1 )
			.arg( st->distinct.size() >= 4096
				? QStringLiteral( "4096+" ) : QString::number( st->distinct.size() ) )
			.arg( waterFrom
				? QStringLiteral( "from the plugin's XCLW/XCWT/WATR (worldspace %1)" ).arg( waterFrom->worldspaceEdid() )
				: QStringLiteral( "none, a .btd stores none -- pass the plugin as <file> with --worldspace to take it from XCLW/XCWT/WATR" ) );
	}
	return true;
}

bool LodtFile::open( const QString & path, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};
	if ( file.isOpen() )
		file.close();
	file.setFileName( path );
	if ( !file.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "cannot open %1" ).arg( path ) );
	/* The header first, for the block-data offset; then everything before
	 * that offset. The payloads stay on disk. */
	buf = file.read( LODL_HEADER_V1 );
	if ( buf.size() < LODL_HEADER_V1 )
		return fail( QStringLiteral( "too short to be a .lodl landscape file" ) );
	{
		/* NAME the other format rather than saying "bad magic". `.lodt` was
		 * this format's own extension until 2026-09-09 and now belongs to the
		 * terrain texture sheets, so being handed one is the expected mistake,
		 * not an exotic one. */
		const quint32 magic = rd<quint32>( buf, 0 );
		if ( magic == LODTEX_MAGIC )
			return fail( QStringLiteral( "refused: %1 is a terrain TEXTURE file "
				"(.lodt, magic LDTX), not a .lodl landscape file" )
				.arg( QFileInfo( path ).fileName() ) );
		if ( magic != LODL_MAGIC )
			return fail( QStringLiteral( "not a .lodl landscape file (bad magic)" ) );
	}
	ver = int( rd<quint32>( buf, 4 ) );
	{
		const qsizetype hdrBytes = ver >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1;
		const quint64 dataAt = rd<quint64>( buf, 0x88 );
		const quint64 total = rd<quint64>( buf, 0x90 );
		if ( dataAt < quint64( hdrBytes ) || dataAt > total || total != quint64( file.size() ) )
			return fail( QStringLiteral( "header offsets do not fit the file" ) );
		file.seek( 0 );
		buf = file.read( qint64( dataAt ) );
		if ( quint64( buf.size() ) != dataAt )
			return fail( QStringLiteral( "short read of the file prefix" ) );
	}
	if ( quint32( ver ) < LODL_VERSION_MIN || quint32( ver ) > LODL_VERSION )
		return fail( QStringLiteral( "unsupported version %1 (this reader knows %2..%3)" )
			.arg( ver ).arg( LODL_VERSION_MIN ).arg( LODL_VERSION ) );
	if ( ver >= 2 ) {
		defWaterH = rd<float>( buf, 0x98 );
		defWaterType = rd<quint32>( buf, 0x9C );
	}

	minX = rd<qint32>( buf, 0x08 ); minY = rd<qint32>( buf, 0x0C );
	maxX = rd<qint32>( buf, 0x10 ); maxY = rd<qint32>( buf, 0x14 );
	spc = int( rd<quint32>( buf, 0x18 ) );
	blkEdge = int( rd<quint32>( buf, 0x1C ) );
	levels = int( rd<quint32>( buf, 0x20 ) );
	hMin = rd<float>( buf, 0x24 );
	hMax = rd<float>( buf, 0x28 );
	quantum = rd<float>( buf, 0x2C );
	const int nLtex = int( rd<quint32>( buf, 0x30 ) );
	const int nWatr = int( rd<quint32>( buf, 0x34 ) );
	aoS = int( rd<quint32>( buf, 0x3C ) );
	ovS = int( rd<quint32>( buf, 0x40 ) );
	sect = rd<quint32>( buf, 0x44 );
	const int nGcvr = int( rd<quint32>( buf, 0x38 ) );
	const quint64 oLtex = rd<quint64>( buf, 0x48 );
	const quint64 oWatr = rd<quint64>( buf, 0x50 );
	const quint64 oGcvr = rd<quint64>( buf, 0x58 );
	oQuad = rd<quint64>( buf, 0x60 );
	oCell = rd<quint64>( buf, 0x68 );
	oOver = rd<quint64>( buf, 0x70 );
	oAo   = rd<quint64>( buf, 0x78 );
	oDir  = rd<quint64>( buf, 0x80 );
	oData = rd<quint64>( buf, 0x88 );
	const quint64 oSize = rd<quint64>( buf, 0x90 );

	/* Refuse rather than misread. Every one of these is a way a truncated or
	 * mislabelled file could otherwise decode into plausible nonsense. */
	if ( oSize != quint64( file.size() ) )
		return fail( QStringLiteral( "size field %1 but file is %2" )
			.arg( oSize ).arg( file.size() ) );
	if ( minX > maxX || minY > maxY )
		return fail( QStringLiteral( "empty cell bounds" ) );
	if ( spc <= 0 || blkEdge <= 0 || levels <= 0 )
		return fail( QStringLiteral( "degenerate rate/block/level count" ) );
	if ( !( quantum > 0.0f ) )
		return fail( QStringLiteral( "height quantum must be positive" ) );
	if ( oData < oDir || oData > quint64( file.size() ) )
		return fail( QStringLiteral( "block directory does not precede its data" ) );

	ltex.resize( nLtex );
	for ( int i = 0; i < nLtex; i++ )
		ltex[i] = rd<quint32>( buf, qsizetype( oLtex ) + i * 4 );
	watr.resize( nWatr );
	for ( int i = 0; i < nWatr; i++ )
		watr[i] = rd<quint32>( buf, qsizetype( oWatr ) + i * 4 );
	/* The GCVR section is the form-ID table IMMEDIATELY followed by the
	 * per-quadrant slots, with no offset of its own; the count is what says
	 * whether either is there, so a FO4-sourced file stops at zero. */
	gcvr.resize( nGcvr );
	for ( int i = 0; i < nGcvr; i++ )
		gcvr[i] = rd<quint32>( buf, qsizetype( oGcvr ) + i * 4 );

	nBlocks = int( ( oData - oDir ) / 16 );
	if ( error )
		error->clear();
	return true;
}

bool LodtFile::cell( int cx, int cy, float & lo, float & hi,
	float & waterH, quint16 & waterType, quint16 & flags ) const
{
	if ( cx < minX || cx > maxX || cy < minY || cy > maxY )
		return false;
	const qsizetype s = qsizetype( cy - minY ) * cellsX() + ( cx - minX );
	const qsizetype at = qsizetype( oCell ) + s * 16;
	lo = rd<float>( buf, at );
	hi = rd<float>( buf, at + 4 );
	waterH = rd<float>( buf, at + 8 );
	waterType = rd<quint16>( buf, at + 12 );
	flags = rd<quint16>( buf, at + 14 );
	return true;
}

void LodtFile::quadrantSlots( int cx, int cy, int quad, quint16 out[6] ) const
{
	for ( int i = 0; i < 6; i++ )
		out[i] = 0xFFFFU;
	if ( cx < minX || cx > maxX || cy < minY || cy > maxY || quad < 0 || quad > 3 )
		return;
	const int qx = ( cx - minX ) * 2 + ( quad & 1 );
	const int qy = ( cy - minY ) * 2 + ( quad >> 1 );
	const qsizetype at = qsizetype( oQuad )
		+ ( qsizetype( qy ) * ( cellsX() * 2 ) + qx ) * 6 * 2;
	for ( int i = 0; i < 6; i++ )
		out[i] = rd<quint16>( buf, at + i * 2 );
}

quint16 LodtFile::planeSample( int gx, int gy, int plane ) const
{
	const int coarsest = levels - 1;
	/* Which level actually STORES this sample: the coarsest one whose stride
	 * divides both coordinates. That is what "progressive" means -- a sample
	 * appears exactly once in the pyramid, at the coarsest level that can
	 * express it. */
	int L = 0;
	while ( L < coarsest && ( gx % ( 1 << ( L + 1 ) ) ) == 0
		&& ( gy % ( 1 << ( L + 1 ) ) ) == 0 )
		L++;

	const int lx = gx >> L, ly = gy >> L;
	const int bi = lx / blkEdge, bj = ly / blkEdge;
	const int wx = lx % blkEdge, wy = ly % blkEdge;

	int firstOfLevel = 0;
	for ( int j = coarsest; j > L; j-- )
		firstOfLevel += ( ( cellsX() + ( 1 << j ) - 1 ) >> j )
			* ( ( cellsY() + ( 1 << j ) - 1 ) >> j );
	const int bx = ( cellsX() + ( 1 << L ) - 1 ) >> L;
	const int idx = firstOfLevel + bj * bx + bi;
	if ( idx < 0 || idx >= nBlocks )
		return 32767;

	int slot = -1;
	for ( int i = 0; i < cache.size(); i++ ) {
		if ( cache[i].idx == idx ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		const qsizetype e = qsizetype( oDir ) + qsizetype( idx ) * 16;
		const quint64 off = rd<quint64>( buf, e );
		const quint32 csz = rd<quint32>( buf, e + 8 );
		const quint32 usz = rd<quint32>( buf, e + 12 );
		if ( off + csz > quint64( file.size() ) )
			return 32767;
		/* qUncompress wants the 4-byte size prefix qCompress writes; the file
		 * stores a plain zlib stream so any consumer can inflate it, so the
		 * prefix is put back here rather than baked into the format. */
		QByteArray z;
		z.resize( 4 );
		for ( int i = 0; i < 4; i++ )
			z[i] = char( ( usz >> ( ( 3 - i ) * 8 ) ) & 0xFF );
		if ( !file.seek( qint64( off ) ) )
			return 32767;
		const QByteArray comp = file.read( qsizetype( csz ) );
		if ( comp.size() != qsizetype( csz ) )
			return 32767;
		z.append( comp );
		QByteArray raw = qUncompress( z );
		if ( raw.size() != qsizetype( usz ) )
			return 32767;
		if ( cache.size() < qMax( 1, cacheCap ) ) {
			cache.append( CachedBlock() );
			slot = cache.size() - 1;
		} else {
			// least recently used
			slot = 0;
			for ( int i = 1; i < cache.size(); i++ )
				if ( cache[i].used < cache[slot].used )
					slot = i;
		}
		cache[slot].idx = idx;
		cache[slot].raw = std::move( raw );
	}
	cache[slot].used = ++cacheClock;
	const QByteArray & raw = cache[slot].raw;

	const int n = ( L == coarsest )
		? blkEdge * blkEdge
		: ( blkEdge / 2 ) * ( blkEdge / 2 ) * 3;
	int k;
	if ( L == coarsest ) {
		k = wy * blkEdge + wx;
	} else {
		const int px = wx / 2, py = wy / 2;
		const int sub = ( ( wx & 1 ) && !( wy & 1 ) ) ? 0
			: ( ( !( wx & 1 ) && ( wy & 1 ) ) ? 1 : 2 );
		k = ( py * ( blkEdge / 2 ) + px ) * 3 + sub;
	}
	return rd<quint16>( raw, qsizetype( plane * n + k ) * 2 );
}

float LodtFile::height( int gx, int gy ) const
{
	return ( float( planeSample( gx, gy, 0 ) ) - 32767.0f ) * quantum;
}

quint16 LodtFile::alphaWord( int gx, int gy ) const
{
	return planeSample( gx, gy, 1 );
}

/* Planes 2 and 3 are OPTIONAL and the section flags say which are present, so
 * their indices are not fixed: ground cover is plane 3 when colour is there and
 * plane 2 when it is not. A reader that hardcoded 3 would read a FO76 file
 * correctly and a hypothetical colourless ground-cover file wrongly. */
bool lodtRefreshAo( const QString & path, QString * error,
	std::function<bool( int, int )> progress )
{
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};
	LodtFile rf;
	QString rerr;
	if ( !rf.open( path, &rerr ) )
		return fail( rerr );
	if ( !rf.aoOffset() || rf.aoSamples() <= 0 )
		return fail( QStringLiteral( "this file carries no AO plane; regenerate it in full to add one" ) );
	const int aoS = rf.aoSamples(), spc = rf.samplesPerCell();
	const int aw = rf.cellsX() * aoS, ah = rf.cellsY() * aoS;
	const int step = qMax( 1, spc / aoS );
	/* The same coarse grid the writer builds: the stored height word at every
	 * aoS-th sample. Read back through the pyramid in raster order, which
	 * keeps the reader's one-block cache warm. */
	std::vector<quint16> grid( size_t( aw ) * size_t( ah ), 32767 );
	for ( int y = 0; y < ah; y++ ) {
		for ( int x = 0; x < aw; x++ )
			grid[size_t( y ) * aw + size_t( x )] = rf.heightWord( x * step, y * step );
		if ( progress && !progress( y + 1, ah * 2 ) )
			return fail( QStringLiteral( "cancelled" ) );
	}
	const QByteArray ao = lodtComputeAo( grid, aw, ah, aoS, rf.heightQuantum(),
		[&]( int done, int total ) { return !progress || progress( total + done, total * 2 ); } );
	if ( ao.isEmpty() )
		return fail( QStringLiteral( "cancelled" ) );
	if ( ao.size() != qsizetype( aw ) * ah )
		return fail( QStringLiteral( "AO plane size mismatch" ) );
	QFile f( path );
	if ( !f.open( QIODevice::ReadWrite ) )
		return fail( QStringLiteral( "cannot open %1 for writing" ).arg( path ) );
	if ( !f.seek( qint64( rf.aoOffset() ) ) || f.write( ao ) != ao.size() )
		return fail( QStringLiteral( "short write to %1" ).arg( path ) );
	f.close();
	if ( error )
		*error = QStringLiteral( "AO plane refreshed: %1x%2 at %3 a cell" ).arg( aw ).arg( ah ).arg( aoS );
	return true;
}

quint16 LodtFile::colourWord( int gx, int gy ) const
{
	if ( !( sect & SECT_COLOUR ) )
		return 0xFFFFU;
	return planeSample( gx, gy, 2 );
}

quint16 LodtFile::groundCover( int gx, int gy ) const
{
	if ( !( sect & SECT_GROUNDCOVER ) )
		return 0;
	return planeSample( gx, gy, ( sect & SECT_COLOUR ) ? 3 : 2 );
}

void LodtFile::setBlockCacheSize( int blocks )
{
	cacheCap = qMax( 1, blocks );
	while ( cache.size() > cacheCap ) {
		int lru = 0;
		for ( int i = 1; i < cache.size(); i++ )
			if ( cache[i].used < cache[lru].used )
				lru = i;
		cache.removeAt( lru );
	}
}

quint8 LodtFile::aoSample( int ax, int ay ) const
{
	// 255 = nothing occludes this: the value that changes no picture, so a
	// missing section is a no-op rather than a black world.
	if ( !( sect & SECT_AO ) || aoS <= 0 )
		return 255;
	const int aw = cellsX() * aoS, ah = cellsY() * aoS;
	if ( ax < 0 || ay < 0 || ax >= aw || ay >= ah )
		return 255;
	const qsizetype at = qsizetype( oAo ) + qsizetype( ay ) * aw + ax;
	if ( at < 0 || at >= buf.size() )
		return 255;
	return quint8( buf.constData()[at] );
}

quint16 LodtFile::overviewWord( int ox, int oy ) const
{
	if ( ovS <= 0 || !oOver )
		return 32767;
	const int ow = cellsX() * ovS, oh = cellsY() * ovS;
	if ( ox < 0 || oy < 0 || ox >= ow || oy >= oh )
		return 32767;
	const qsizetype at = qsizetype( oOver ) + ( qsizetype( oy ) * ow + ox ) * 2;
	if ( at < 0 || at + 1 >= buf.size() )
		return 32767;
	return rd<quint16>( buf, at );
}
