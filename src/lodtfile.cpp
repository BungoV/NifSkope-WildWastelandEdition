/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodtfile.h"
#include "esmdata.h"
#include "lodgenlayout.h"

/* Only for LODTEX_MAGIC: this reader must be able to NAME a terrain
 * TEXTURE file when it is handed one, because `.lodt` meant THIS format
 * until 2026-09-09 and means that one now. */
#include "io/lodvfile.h"

#include "btdfile.hpp"

/* Only for the WATR `NAM0` linear velocities the water module falls back on.
 * EsmWorld exposes no WATR accessor and this lane does not own esmdata.*, so
 * the plugin is opened here, ON DEMAND, and only when the module is switched on
 * with a plugin to read (see scratchpad/water2_20260909/ESMDATA_CHANGE_NEEDED.md
 * for the accessor that would retire this). */
#include "esmfile.hpp"

#include <QDir>
#include <QFileInfo>
#include <QPair>
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
constexpr quint32 LODL_VERSION = 3;
constexpr quint32 LODL_VERSION_MIN = 1;
constexpr qsizetype LODL_HEADER_V1 = 0x98;
constexpr qsizetype LODL_HEADER_V2 = 0xA0;
/* Version 3 appends the water-body fields from 0xA0 and its SECTIONS after the
 * block data, so 0x00..0x9F is byte-for-byte what version 2 writes and every
 * version-2 offset still sits where it did. */
constexpr qsizetype LODL_HEADER_V3 = 0xF8;

/*! Header size by version -- a TABLE, not a ternary.
 *
 *  It used to be `ver >= 2 ? V2 : V1`, evaluated BEFORE the version check, so
 *  the moment a third version existed a version-3 file was measured against a
 *  160-byte floor by the very reader that was about to refuse it. Harmless
 *  while the refusal stands and a misparse the day someone relaxes it, which is
 *  why the spec called this out as a thing to fix WITH the bump rather than
 *  after it. An unknown version answers 0, and 0 makes every offset check fail
 *  closed. */
static inline qsizetype lodtHeaderBytes( int version )
{
	switch ( version ) {
	case 1:  return LODL_HEADER_V1;
	case 2:  return LODL_HEADER_V2;
	case 3:  return LODL_HEADER_V3;
	default: return 0;
	}
}

constexpr quint32 SECT_COLOUR = LODL_SECT_COLOUR;
constexpr quint32 SECT_GROUNDCOVER = LODL_SECT_GROUNDCOVER;
constexpr quint32 SECT_AO = LODL_SECT_AO;
constexpr quint32 SECT_WATER = LODL_SECT_WATER;
constexpr quint32 SECT_BODIES = LODL_SECT_BODIES;
constexpr quint32 SECT_FLOW = LODL_SECT_FLOW;
constexpr quint32 SECT_SHORE = LODL_SECT_SHORE;
constexpr quint32 SECT_STROKE = LODL_SECT_STROKE;

//! Two pi, spelled out: M_PI is not standard C++ and is absent under /std:c++.
constexpr double kTwoPi = 6.283185307179586476925286766559;

//! Bytes of one body-table record as THIS build writes it (spec §3.3).
constexpr int LODL_BODY_RECORD = 48;
//! World units per stored shore step.
constexpr float LODL_SHORE_QUANTUM = 32.0f;

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

//! The same for a u32 -- the section-flag word is patched, because whether the
//! water sections are really THERE is only known after they have been built.
void patch32( QByteArray & b, qsizetype at, quint32 v )
{
	for ( int i = 0; i < 4; i++ )
		b[at + i] = char( ( v >> ( i * 8 ) ) & 0xFF );
}

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
	/*! Lane FIX1: the range of a landless cell's FILLED heights (the
	 *  landFill option); false = not filled. Optional. */
	std::function<bool( int, int, float &, float & )> fillRange;
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

/* =========================================================================
 *  WATER BODIES  (version 3)
 *
 *  scratchpad/specs_20260909/spec_water.md is the contract; this is its rule D
 *  and its §4 algorithms, and the comments here say only what the code cannot.
 *
 *  Two things in it are NOT what that page says, and both are measured
 *  corrections rather than preferences (lane WATER2, 2026-09-10 --
 *  scratchpad/water2_20260909/bridge_exact.py, bridge_effect.py,
 *  bridge_variants.py):
 *
 *   1. THE SHORE TEST IS EXACT. Lane WATER1's Python compared DECIMATED point
 *      clouds (every `area/4000`-th texel), which can only ever OVERESTIMATE a
 *      distance: it found 218 of the 545 pairs that are actually within two
 *      texels, and its 590 bodies are that shortfall. Scanning the disc of
 *      radius `gap` around every texel is exact and costs one pass.
 *   2. A BRIDGE IN WHICH EXACTLY ONE SIDE INHERITS the worldspace type is
 *      accepted only when the INHERITING side is the SMALLER of the two.
 *      Without that clause the exact test hands the Commonwealth's ocean --
 *      21,587,443 texels -- to `ExtMarshScumWater`, because a painted marsh
 *      passes within two texels of it. Rule C's merge already states the
 *      direction ("an INHERITING component is merged INTO the painted one");
 *      this is the same direction applied to a gap instead of a touch.
 *
 *  The known-answer control below (`lodl --water-selftest`) then found the same
 *  missing direction in rule C's ADJACENT merge -- the synthetic sea TOUCHES a
 *  painted river reach at its own height -- so the clause is in both merges.
 *
 *  Measured on the Commonwealth: 805 components -> 793 rule-C bodies -> 346
 *  bodies, 528 bridge merges accepted, 13 refused, and thirteen of the fifteen
 *  per-form texel totals identical to the read-only census's to the texel.
 * ========================================================================= */

//! Everything the water pass reads. It never sees a plugin or a LAND record.
struct WaterInput
{
	int cellsX = 0, cellsY = 0, spc = 32, minX = 0, minY = 0;
	float quantum = 8.0f;
	const std::vector<quint16> * cellFlags = nullptr;
	const std::vector<float> * cellWaterH = nullptr;
	const std::vector<quint16> * cellWaterT = nullptr;
	const QVector<quint32> * watrForms = nullptr;
	quint32 defaultWaterType = 0;
	float defaultWaterHeight = 0.0f;
	//! Fills `cellsX * spc` stored height words for one global row, row 0 south.
	std::function<void( int gy, quint16 * row )> heightRow;
	//! WATR NAM0 linear velocity, X and Y. False = this arm is unavailable.
	std::function<bool( quint32 form, float &, float & )> velocity;
	LodtWaterOptions opt;
};

//! What the pass produces: the sections, already packed, and its own census.
struct WaterOut
{
	QByteArray bodyTable, nameBlob, strokeStore;
	//! Packed at a known base offset, so their directories are absolute.
	QByteArray idPlane, flowPlane, shorePlane;
	int bodyCount = 0;
	int bodySamples = 0, flowSamples = 0, shoreSamples = 0;
	QString census;      //!< the table --water-census prints
	QString summary;     //!< one line for the writer's own census
};

/*! One maximal run of wet texels with one (height, type) key, in one row. */
struct WaterRun
{
	qint32 x0 = 0, x1 = 0;       //!< inclusive
	float wh = 0.0f;             //!< the cell's resolved water height
	qint32 hq = 0;               //!< round(wh * 8) -- the height half of the key
	quint16 tq = 0;              //!< the water TYPE index, 0xFFFF = inherited
	qint32 comp = 0;             //!< component id, filled after the union
};

//! Union-find whose root is always the SMALLEST index in the set, so component
//! numbering is a property of the grid and not of the order of the unions.
struct WaterUf
{
	std::vector<qint32> p;
	void reset( size_t n )
	{
		p.resize( n );
		for ( size_t i = 0; i < n; i++ )
			p[i] = qint32( i );
	}
	qint32 find( qint32 a )
	{
		while ( p[size_t( a )] != a ) {
			p[size_t( a )] = p[size_t( p[size_t( a )] )];
			a = p[size_t( a )];
		}
		return a;
	}
	void join( qint32 a, qint32 b )
	{
		a = find( a );
		b = find( b );
		if ( a == b )
			return;
		if ( a < b )
			p[size_t( b )] = a;
		else
			p[size_t( a )] = b;
	}
};

//! A component of rule C, before any merge.
struct WaterComp
{
	qint64 area = 0;
	qint32 x0 = 0, x1 = 0, y0 = 0, y1 = 0;
	float wh = 0.0f;
	quint16 tq = 0;
	bool edge = false;
	qint32 body = 0;             //!< the rule-C body it ends up in
};

//! A rule-C body: components merged by adjacency; the bridge works on these.
struct WaterCBody
{
	qint64 area = 0;
	qint32 x0 = 0, x1 = 0, y0 = 0, y1 = 0;
	float wh = 0.0f;
	quint16 tq = 0;              //!< majority type by area, INCLUDING the default
	bool edge = false;
	int parts = 0;
	bool ambiguous = false;
	QHash<quint16, qint64> types;
	qint32 out = 0;              //!< the final body id
};

//! The finished thing: what one record of the body table describes.
struct WaterBody
{
	qint64 area = 0;
	qint32 x0 = 0, x1 = 0, y0 = 0, y1 = 0;
	float wh = 0.0f;
	quint16 tq = 0xFFFFU;        //!< the PAINTED majority; 0xFFFF = inherited
	quint32 form = 0;
	bool edge = false, ambiguous = false;
	int parts = 0;
	int cls = 2;                 //!< 0 sea, 1 river, 2 lake
	double axX = 1.0, axY = 0.0, aniso = 0.0, elong = 0.0;
	double bedR = 0.0, bedDrop = 0.0;
	double flowX = 0.0, flowY = 0.0;
	int flowSource = 0;
	int outlet = 0;
	double nam0X = 0.0, nam0Y = 0.0;
	bool hasNam0 = false;
	// accumulators
	double n = 0, sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
	double sb = 0, sbb = 0, sxb = 0, syb = 0;
	double tMin = 0, tMax = 0;
};

/*! The principal axis of a point set from its second moments, and the
 *  anisotropy `1 - l1/l0` the classification and the flow rule both gate on.
 *  Closed form rather than an eigen solver: a symmetric 2x2 is a quadratic. */
static void lodtPrincipalAxis( double n, double sx, double sy, double sxx,
	double syy, double sxy, double & ax, double & ay, double & aniso )
{
	ax = 1.0;
	ay = 0.0;
	aniso = 0.0;
	if ( n < 3.0 )
		return;
	const double mx = sx / n, my = sy / n;
	const double cxx = sxx / n - mx * mx;
	const double cyy = syy / n - my * my;
	const double cxy = sxy / n - mx * my;
	const double tr = cxx + cyy;
	const double det = cxx * cyy - cxy * cxy;
	const double d = qMax( tr * tr / 4.0 - det, 0.0 );
	const double l0 = tr / 2.0 + std::sqrt( d );
	const double l1 = tr / 2.0 - std::sqrt( d );
	double vx, vy;
	if ( std::fabs( cxy ) > 1e-12 ) {
		vx = l0 - cyy;
		vy = cxy;
	} else if ( cxx >= cyy ) {
		vx = 1.0;
		vy = 0.0;
	} else {
		vx = 0.0;
		vy = 1.0;
	}
	const double m = std::sqrt( vx * vx + vy * vy );
	if ( m > 0.0 ) {
		ax = vx / m;
		ay = vy / m;
	}
	if ( l0 > 0.0 )
		aniso = 1.0 - l1 / l0;
}

/*! ONE tiled, zlib-compressed plane, packed at a known file offset.
 *
 *  Three planes share this container so there is one implementation and one
 *  gate. A tile whose compressed size is 0 is UNIFORM and its uncompressed-size
 *  field holds the repeated sample instead -- 99.2% of the Commonwealth's wet
 *  area is one body, so most tiles cost sixteen bytes rather than an inflate. */
static QByteArray lodtPackPlane( int tilesX, int tilesY, int tileEdge,
	int bytesPerSample, quint64 base,
	const std::function<void( int tx, int ty, quint8 * out )> & fill,
	qint64 * uniformTiles )
{
	const qint64 nTiles = qint64( tilesX ) * tilesY;
	const qint64 hdr = 32;
	const qint64 dirBytes = nTiles * 16;
	Buf head;
	head.u32( quint32( tilesX ) );
	head.u32( quint32( tilesY ) );
	head.u32( quint32( tileEdge ) );
	head.u32( quint32( bytesPerSample ) );
	head.u64( base + quint64( hdr ) );
	head.u64( base + quint64( hdr + dirBytes ) );

	Buf dir, data;
	quint64 pos = base + quint64( hdr + dirBytes );
	const qsizetype tileBytes = qsizetype( tileEdge ) * tileEdge * bytesPerSample;
	std::vector<quint8> raw( size_t( tileBytes ), quint8( 0 ) );
	qint64 uniform = 0;
	for ( int ty = 0; ty < tilesY; ty++ ) {
		for ( int tx = 0; tx < tilesX; tx++ ) {
			fill( tx, ty, raw.data() );
			bool same = true;
			for ( qsizetype k = bytesPerSample; k < tileBytes && same; k += bytesPerSample )
				for ( int c = 0; c < bytesPerSample; c++ )
					if ( raw[size_t( k + c )] != raw[size_t( c )] ) {
						same = false;
						break;
					}
			if ( same ) {
				quint32 v = 0;
				for ( int c = 0; c < bytesPerSample; c++ )
					v |= quint32( raw[size_t( c )] ) << ( 8 * c );
				dir.u64( 0 );
				dir.u32( 0 );          // csize 0 = uniform
				dir.u32( v );          // and the repeated sample lives here
				uniform++;
				continue;
			}
			QByteArray z = qCompress( QByteArray( reinterpret_cast<const char *>( raw.data() ),
				int( tileBytes ) ), 9 );
			z.remove( 0, 4 );          // a plain zlib stream, as the blocks are
			dir.u64( pos );
			dir.u32( quint32( z.size() ) );
			dir.u32( quint32( tileBytes ) );
			data.b.append( z );
			pos += quint64( z.size() );
		}
	}
	if ( uniformTiles )
		*uniformTiles = uniform;
	QByteArray out = head.b;
	out.append( dir.b );
	out.append( data.b );
	return out;
}

/*! Build every water section. Returns false with a NAMED reason.
 *
 *  `baseOffset` is where the first section will land in the file, because the
 *  plane directories store absolute offsets exactly as the block directory
 *  does. */
static bool lodtBuildWater( const WaterInput & in, quint64 baseOffset,
	WaterOut & out, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	const int spc = in.spc, cellsX = in.cellsX, cellsY = in.cellsY;
	const qint64 gw = qint64( cellsX ) * spc;
	const qint64 gh = qint64( cellsY ) * spc;
	const qint64 samples = gw * gh;
	/* The classification needs the whole wet mask resident, which is 2 bytes a
	 * sample for the id grid and 2 for the shore sweep. That is 150 MB for the
	 * Commonwealth and 2.6 GB for an 804-cell Fallout 76 port, so the module
	 * REFUSES rather than thrashing -- and its fallback is itself: the file is
	 * written at version 2, which is what every consumer reads today. */
	if ( samples > ( qint64( 1 ) << 28 ) )
		return fail( QString( "water bodies need the whole %1 x %2 sample grid resident "
			"(%3 million samples, about %4 MB); this build refuses above 268 million. "
			"Write without --water-bodies, or raise the body sample rate" )
			.arg( gw ).arg( gh ).arg( samples / 1000000 ).arg( samples * 4 / ( 1 << 20 ) ) );
	if ( gw <= 0 || gh <= 0 )
		return fail( QStringLiteral( "the worldspace has no samples to classify" ) );

	const int gap = qBound( 0, in.opt.bridgeGap, 32 );
	const int near = qBound( 1, in.opt.nearTexels, 4096 );

	// ---- pass A: the wet mask, as runs -----------------------------------
	std::vector<WaterRun> runs;
	std::vector<qint64> rowAt( size_t( gh ) + 1, 0 );
	{
		std::vector<quint16> row( size_t( gw ), quint16( 0 ) );
		for ( qint64 gy = 0; gy < gh; gy++ ) {
			rowAt[size_t( gy )] = qint64( runs.size() );
			in.heightRow( int( gy ), row.data() );
			const int cy = int( gy / spc );
			const size_t cellRow = size_t( cy ) * size_t( cellsX );
			bool open = false;
			for ( qint64 gx = 0; gx < gw; gx++ ) {
				const int cx = int( gx / spc );
				const size_t s = cellRow + size_t( cx );
				const bool has = ( ( *in.cellFlags )[s] & CELL_HAS_WATER ) != 0;
				bool wet = false;
				float wh = 0.0f;
				quint16 tq = WATER_TYPE_DEFAULT;
				if ( has ) {
					wh = ( *in.cellWaterH )[s];
					tq = ( *in.cellWaterT )[s];
					const float h = ( float( row[size_t( gx )] ) - 32767.0f ) * in.quantum;
					wet = h < wh;
				}
				if ( !wet ) {
					open = false;
					continue;
				}
				const qint32 hq = qint32( std::floor( double( wh ) * 8.0 + 0.5 ) );
				if ( open && runs.back().hq == hq && runs.back().tq == tq
					&& runs.back().x1 == qint32( gx ) - 1 ) {
					runs.back().x1 = qint32( gx );
					continue;
				}
				WaterRun r;
				r.x0 = r.x1 = qint32( gx );
				r.wh = wh;
				r.hq = hq;
				r.tq = tq;
				runs.push_back( r );
				open = true;
			}
		}
		rowAt[size_t( gh )] = qint64( runs.size() );
	}
	if ( runs.empty() )
		return fail( QStringLiteral( "no wet texel anywhere: every cell's terrain is at or "
			"above its own water plane, so there is no body to classify" ) );

	// ---- components: 4-connected, equal (height, type) --------------------
	WaterUf uf;
	uf.reset( runs.size() );
	/* Pairs of runs that TOUCH but did not join, which is where rule C's
	 * "merge an inheriting component into the painted one it touches" comes
	 * from. Collected as run pairs and reduced to component pairs once the
	 * roots are known. */
	std::vector<std::pair<qint32, qint32>> touch;
	for ( qint64 y = 0; y < gh; y++ ) {
		const qint64 a0 = rowAt[size_t( y )], a1 = rowAt[size_t( y ) + 1];
		for ( qint64 i = a0 + 1; i < a1; i++ )
			if ( runs[size_t( i - 1 )].x1 + 1 == runs[size_t( i )].x0 )
				touch.emplace_back( qint32( i - 1 ), qint32( i ) );
		if ( y == 0 )
			continue;
		qint64 i = rowAt[size_t( y ) - 1], iEnd = a0;
		qint64 j = a0, jEnd = a1;
		while ( i < iEnd && j < jEnd ) {
			const WaterRun & A = runs[size_t( i )];
			const WaterRun & B = runs[size_t( j )];
			if ( A.x1 < B.x0 ) {
				i++;
			} else if ( B.x1 < A.x0 ) {
				j++;
			} else {
				if ( A.hq == B.hq && A.tq == B.tq )
					uf.join( qint32( i ), qint32( j ) );
				else
					touch.emplace_back( qint32( i ), qint32( j ) );
				if ( A.x1 < B.x1 )
					i++;
				else
					j++;
			}
		}
	}
	// number components by ascending root, which is ascending first run
	std::vector<qint32> compOf( runs.size(), 0 );
	std::vector<WaterComp> comps;
	{
		QHash<qint32, qint32> byRoot;
		for ( size_t i = 0; i < runs.size(); i++ ) {
			const qint32 r = uf.find( qint32( i ) );
			auto it = byRoot.constFind( r );
			qint32 c;
			if ( it == byRoot.constEnd() ) {
				c = qint32( comps.size() );
				byRoot.insert( r, c );
				comps.push_back( WaterComp() );
				comps.back().x0 = comps.back().x1 = runs[i].x0;
				comps.back().y0 = comps.back().y1 = 0;
				comps.back().wh = runs[i].wh;
				comps.back().tq = runs[i].tq;
				comps.back().y0 = 1 << 30;
				comps.back().y1 = -1;
				comps.back().x0 = 1 << 30;
				comps.back().x1 = -1;
			} else {
				c = it.value();
			}
			compOf[i] = c;
			runs[i].comp = c;
		}
		for ( qint64 y = 0; y < gh; y++ ) {
			for ( qint64 k = rowAt[size_t( y )]; k < rowAt[size_t( y ) + 1]; k++ ) {
				WaterComp & c = comps[size_t( compOf[size_t( k )] )];
				const WaterRun & r = runs[size_t( k )];
				c.area += r.x1 - r.x0 + 1;
				c.x0 = qMin( c.x0, r.x0 );
				c.x1 = qMax( c.x1, r.x1 );
				c.y0 = qMin( c.y0, qint32( y ) );
				c.y1 = qMax( c.y1, qint32( y ) );
				if ( r.x0 == 0 || r.x1 == qint32( gw ) - 1 || y == 0 || y == gh - 1 )
					c.edge = true;
			}
		}
	}
	const qint64 nComp = qint64( comps.size() );

	// component adjacency, from the run pairs
	std::vector<std::vector<qint32>> adj;
	adj.resize( size_t( nComp ) );
	{
		QSet<qint64> seen;
		for ( const auto & t : touch ) {
			const qint32 a = compOf[size_t( t.first )], b = compOf[size_t( t.second )];
			if ( a == b )
				continue;
			const qint64 key = ( qint64( qMin( a, b ) ) << 32 ) | quint32( qMax( a, b ) );
			if ( seen.contains( key ) )
				continue;
			seen.insert( key );
			adj[size_t( a )].push_back( b );
			adj[size_t( b )].push_back( a );
		}
	}

	// ---- rule C: an inheriting component joins the painted one it touches --
	WaterUf cuf;
	cuf.reset( size_t( nComp ) );
	int ambiguousMerges = 0, refusedMerges = 0;
	for ( qint64 c = 0; c < nComp; c++ ) {
		if ( comps[size_t( c )].tq != WATER_TYPE_DEFAULT )
			continue;
		qint32 best = -1;
		int candidates = 0;
		for ( qint32 nb : adj[size_t( c )] ) {
			const WaterComp & o = comps[size_t( nb )];
			if ( o.tq == WATER_TYPE_DEFAULT )
				continue;
			if ( std::fabs( double( o.wh ) - double( comps[size_t( c )].wh ) ) >= 0.01 )
				continue;
			candidates++;
			if ( best < 0 || o.area > comps[size_t( best )].area
				|| ( o.area == comps[size_t( best )].area && nb < best ) )
				best = nb;
		}
		if ( candidates == 0 )
			continue;
		/* THE DIRECTION, and the one clause that makes it safe. The rule reads
		 * "an INHERITING component is merged INTO the painted one it touches",
		 * so the inheriting side is the one absorbed -- and it may only be
		 * absorbed when it is the SMALLER of the two, or the merge renames the
		 * larger water after the smaller. Without this the synthetic control's
		 * 73,728-texel sea, which touches the tidal reach of a 512-texel
		 * painted river at exactly the sea's own height, comes out carrying the
		 * river's form. On the Commonwealth it costs one merge of thirteen and
		 * leaves NO body where the two readings of the form rule disagree,
		 * where without it there are two (measured:
		 * scratchpad/water2_20260909/merge_guard_variants.py). */
		if ( comps[size_t( c )].area >= comps[size_t( best )].area ) {
			refusedMerges++;
			continue;
		}
		if ( candidates > 1 )
			ambiguousMerges++;
		cuf.join( qint32( c ), best );
	}
	std::vector<WaterCBody> cb;
	{
		QHash<qint32, qint32> byRoot;
		for ( qint64 c = 0; c < nComp; c++ ) {
			const qint32 r = cuf.find( qint32( c ) );
			auto it = byRoot.constFind( r );
			qint32 b;
			if ( it == byRoot.constEnd() ) {
				b = qint32( cb.size() );
				byRoot.insert( r, b );
				cb.push_back( WaterCBody() );
				cb.back().x0 = cb.back().y0 = 1 << 30;
				cb.back().x1 = cb.back().y1 = -1;
				cb.back().wh = comps[size_t( c )].wh;
			} else {
				b = it.value();
			}
			WaterComp & cc = comps[size_t( c )];
			cc.body = b;
			WaterCBody & B = cb[size_t( b )];
			B.area += cc.area;
			B.x0 = qMin( B.x0, cc.x0 );
			B.x1 = qMax( B.x1, cc.x1 );
			B.y0 = qMin( B.y0, cc.y0 );
			B.y1 = qMax( B.y1, cc.y1 );
			B.edge = B.edge || cc.edge;
			B.parts++;
			B.types[cc.tq] += cc.area;
		}
		for ( WaterCBody & B : cb ) {
			qint64 bestArea = -1;
			quint16 bestT = WATER_TYPE_DEFAULT;
			for ( auto it = B.types.constBegin(); it != B.types.constEnd(); ++it )
				if ( it.value() > bestArea || ( it.value() == bestArea && it.key() < bestT ) ) {
					bestArea = it.value();
					bestT = it.key();
				}
			B.tq = bestT;
		}
	}
	const qint64 nCb = qint64( cb.size() );
	if ( nCb > 65534 )
		return fail( QString( "%1 water components before bridging; the body-ID plane is "
			"16-bit and cannot name more than 65534" ).arg( nCb ) );

	// ---- the rule-C label grid, which the exact shore test scans ----------
	std::vector<quint16> grid( size_t( samples ), 0 );
	for ( qint64 y = 0; y < gh; y++ ) {
		for ( qint64 k = rowAt[size_t( y )]; k < rowAt[size_t( y ) + 1]; k++ ) {
			const WaterRun & r = runs[size_t( k )];
			const qint32 b = comps[size_t( r.comp )].body + 1;
			quint16 * p = grid.data() + size_t( y * gw + r.x0 );
			for ( qint32 x = r.x0; x <= r.x1; x++ )
				*p++ = quint16( b );
		}
	}

	// ---- rule D: bridge components whose SHORES are within `gap` ----------
	WaterUf duf;
	duf.reset( size_t( nCb ) );
	qint64 accepted = 0, refused = 0;
	{
		QSet<qint64> pairSeen;
		for ( int dy = 0; dy <= gap; dy++ ) {
			for ( int dx = -gap; dx <= gap; dx++ ) {
				if ( dy == 0 && dx <= 0 )
					continue;
				if ( dy * dy + dx * dx > gap * gap )
					continue;
				const qint64 y0 = 0, y1 = gh - dy;
				const qint64 x0 = dx >= 0 ? 0 : -dx;
				const qint64 x1 = dx >= 0 ? gw - dx : gw;
				for ( qint64 y = y0; y < y1; y++ ) {
					const quint16 * a = grid.data() + size_t( y * gw );
					const quint16 * b = grid.data() + size_t( ( y + dy ) * gw + dx );
					for ( qint64 x = x0; x < x1; x++ ) {
						const quint16 u = a[x], v = b[x];
						if ( !u || !v || u == v )
							continue;
						const quint16 lo = qMin( u, v ), hi = qMax( u, v );
						const qint64 key = ( qint64( lo ) << 20 ) | hi;
						if ( pairSeen.contains( key ) )
							continue;
						pairSeen.insert( key );
						const WaterCBody & A = cb[size_t( lo ) - 1];
						const WaterCBody & B = cb[size_t( hi ) - 1];
						if ( std::fabs( double( A.wh ) - double( B.wh ) ) > 0.01 )
							continue;
						const bool inhA = A.tq == WATER_TYPE_DEFAULT;
						const bool inhB = B.tq == WATER_TYPE_DEFAULT;
						bool join = false;
						if ( A.tq == B.tq ) {
							join = true;
						} else if ( inhA && inhB ) {
							join = true;
						} else if ( inhA || inhB ) {
							/* The inheriting side is the one absorbed, and only
							 * when it is the smaller: an unpainted REACH joins
							 * its river, an ocean does not swallow a marsh. */
							const WaterCBody & small = inhA ? A : B;
							const WaterCBody & big = inhA ? B : A;
							join = small.area < big.area;
						}
						if ( join ) {
							duf.join( qint32( lo ) - 1, qint32( hi ) - 1 );
							accepted++;
						} else {
							refused++;
						}
					}
				}
			}
		}
	}

	// ---- final bodies, numbered by DESCENDING AREA (spec §4.1 step 6) -----
	std::vector<WaterBody> bodies;
	{
		QHash<qint32, qint32> byRoot;
		std::vector<std::vector<qint32>> group;
		for ( qint64 b = 0; b < nCb; b++ ) {
			const qint32 r = duf.find( qint32( b ) );
			auto it = byRoot.constFind( r );
			qint32 g;
			if ( it == byRoot.constEnd() ) {
				g = qint32( group.size() );
				byRoot.insert( r, g );
				group.push_back( std::vector<qint32>() );
			} else {
				g = it.value();
			}
			group[size_t( g )].push_back( qint32( b ) );
		}
		std::vector<qint32> order( group.size() );
		for ( size_t i = 0; i < group.size(); i++ )
			order[i] = qint32( i );
		std::vector<qint64> gArea( group.size(), 0 );
		for ( size_t i = 0; i < group.size(); i++ )
			for ( qint32 b : group[i] )
				gArea[i] += cb[size_t( b )].area;
		std::sort( order.begin(), order.end(), [&]( qint32 a, qint32 b ) {
			if ( gArea[size_t( a )] != gArea[size_t( b )] )
				return gArea[size_t( a )] > gArea[size_t( b )];
			return group[size_t( a )][0] < group[size_t( b )][0];
		} );
		bodies.resize( order.size() );
		for ( size_t k = 0; k < order.size(); k++ ) {
			WaterBody & W = bodies[k];
			W.x0 = W.y0 = 1 << 30;
			W.x1 = W.y1 = -1;
			QHash<quint16, qint64> types;
			for ( qint32 b : group[size_t( order[k] )] ) {
				WaterCBody & B = cb[size_t( b )];
				B.out = qint32( k ) + 1;
				W.area += B.area;
				W.x0 = qMin( W.x0, B.x0 );
				W.x1 = qMax( W.x1, B.x1 );
				W.y0 = qMin( W.y0, B.y0 );
				W.y1 = qMax( W.y1, B.y1 );
				W.edge = W.edge || B.edge;
				W.parts += B.parts;
				W.ambiguous = W.ambiguous || B.ambiguous;
				for ( auto it = B.types.constBegin(); it != B.types.constEnd(); ++it )
					types[it.key()] += it.value();
			}
			W.wh = cb[size_t( group[size_t( order[k] )][0] )].wh;
			/* The body's WATR form is the PAINTED type with the most area; the
			 * worldspace default only when nothing in it was painted. Measured
			 * on the Commonwealth: with the bridge clause above, there is not
			 * one body where this disagrees with "the majority type counting
			 * the default", so the two readings of the rule now coincide. */
			qint64 bestArea = -1;
			quint16 bestT = WATER_TYPE_DEFAULT;
			for ( auto it = types.constBegin(); it != types.constEnd(); ++it ) {
				if ( it.key() == WATER_TYPE_DEFAULT )
					continue;
				if ( it.value() > bestArea || ( it.value() == bestArea && it.key() < bestT ) ) {
					bestArea = it.value();
					bestT = it.key();
				}
			}
			W.tq = bestT;
			W.form = ( bestT == WATER_TYPE_DEFAULT )
				? in.defaultWaterType
				: quint32( in.watrForms->value( int( bestT ), 0 ) );
			if ( !W.form )
				W.form = in.defaultWaterType;
		}
	}
	const qint64 nBodies = qint64( bodies.size() );
	if ( nBodies > 65534 )
		return fail( QString( "%1 water bodies; the body-ID plane is 16-bit" ).arg( nBodies ) );

	// remap the grid from rule-C ids to final ids
	{
		std::vector<quint16> lut( size_t( nCb ) + 1, 0 );
		for ( qint64 b = 0; b < nCb; b++ )
			lut[size_t( b ) + 1] = quint16( cb[size_t( b )].out );
		for ( size_t i = 0; i < grid.size(); i++ )
			grid[i] = lut[grid[i]];
	}

	// ---- pass B: the moments the axis, the elongation and the bed need ----
	{
		std::vector<quint16> row( size_t( gw ), quint16( 0 ) );
		for ( qint64 gy = 0; gy < gh; gy++ ) {
			in.heightRow( int( gy ), row.data() );
			const quint16 * g = grid.data() + size_t( gy * gw );
			for ( qint64 gx = 0; gx < gw; gx++ ) {
				const quint16 b = g[gx];
				if ( !b )
					continue;
				WaterBody & W = bodies[size_t( b ) - 1];
				const double x = double( gx ), y = double( gy );
				const double bed = ( double( row[size_t( gx )] ) - 32767.0 ) * double( in.quantum );
				W.n += 1.0;
				W.sx += x;
				W.sy += y;
				W.sxx += x * x;
				W.syy += y * y;
				W.sxy += x * y;
				W.sb += bed;
				W.sbb += bed * bed;
				W.sxb += x * bed;
				W.syb += y * bed;
			}
		}
	}
	for ( WaterBody & W : bodies ) {
		lodtPrincipalAxis( W.n, W.sx, W.sy, W.sxx, W.syy, W.sxy, W.axX, W.axY, W.aniso );
		const double w = double( W.x1 - W.x0 + 1 ), h = double( W.y1 - W.y0 + 1 );
		W.elong = qMax( w, h ) * qMax( w, h ) / qMax( 1.0, double( W.area ) );
		/* r and the slope of `bed` against the position along the axis, from
		 * the raw moments: t = ax*x + ay*y is linear, so every sum it needs is
		 * one of the nine already counted and the pass never runs twice. */
		const double n = W.n;
		const double st = W.axX * W.sx + W.axY * W.sy;
		const double stt = W.axX * W.axX * W.sxx + 2.0 * W.axX * W.axY * W.sxy
			+ W.axY * W.axY * W.syy;
		const double stb = W.axX * W.sxb + W.axY * W.syb;
		const double varT = stt / n - ( st / n ) * ( st / n );
		const double varB = W.sbb / n - ( W.sb / n ) * ( W.sb / n );
		const double cov = stb / n - ( st / n ) * ( W.sb / n );
		W.bedR = ( varT > 1e-12 && varB > 1e-12 ) ? cov / std::sqrt( varT * varB ) : 0.0;
		const double slope = varT > 1e-12 ? cov / varT : 0.0;
		W.tMin = 1e30;
		W.tMax = -1e30;
		W.bedDrop = slope;      // scaled by the t range once that is measured
	}
	for ( qint64 y = 0; y < gh; y++ ) {
		for ( qint64 k = rowAt[size_t( y )]; k < rowAt[size_t( y ) + 1]; k++ ) {
			const WaterRun & r = runs[size_t( k )];
			const qint32 id = cb[size_t( comps[size_t( r.comp )].body )].out;
			WaterBody & W = bodies[size_t( id ) - 1];
			const double ta = W.axX * double( r.x0 ) + W.axY * double( y );
			const double tb = W.axX * double( r.x1 ) + W.axY * double( y );
			W.tMin = qMin( W.tMin, qMin( ta, tb ) );
			W.tMax = qMax( W.tMax, qMax( ta, tb ) );
		}
	}
	for ( WaterBody & W : bodies )
		W.bedDrop = W.bedDrop * ( W.tMax - W.tMin );

	// ---- the drainage relation: exact, bucketed at `near` -----------------
	/* Two texels more than one bucket apart are at least `near`+1 apart, so the
	 * 3x3 neighbourhood of buckets is the whole search. Only BOUNDARY texels
	 * can carry the minimum between two disjoint sets, which is what keeps the
	 * sea's 21.5 M texels out of the comparison. */
	struct NearHit { double d; double cx, cy; };
	QHash<qint64, NearHit> nearest;
	{
		struct BPt { qint32 x, y; quint16 b; };
		std::vector<BPt> pts;
		for ( qint64 y = 0; y < gh; y++ ) {
			const quint16 * g = grid.data() + size_t( y * gw );
			for ( qint64 x = 0; x < gw; x++ ) {
				const quint16 b = g[x];
				if ( !b || bodies[size_t( b ) - 1].area < 16 )
					continue;
				const bool edgeOfGrid = ( x == 0 || y == 0 || x == gw - 1 || y == gh - 1 );
				if ( edgeOfGrid
					|| g[x - 1] != b || g[x + 1] != b
					|| grid[size_t( ( y - 1 ) * gw + x )] != b
					|| grid[size_t( ( y + 1 ) * gw + x )] != b ) {
					BPt p;
					p.x = qint32( x );
					p.y = qint32( y );
					p.b = b;
					pts.push_back( p );
				}
			}
		}
		const int bx = int( ( gw + near - 1 ) / near );
		const int by = int( ( gh + near - 1 ) / near );
		std::vector<std::vector<qint32>> bucket;
		bucket.resize( size_t( bx ) * size_t( by ) );
		for ( size_t i = 0; i < pts.size(); i++ )
			bucket[size_t( pts[i].y / near ) * size_t( bx ) + size_t( pts[i].x / near )]
				.push_back( qint32( i ) );
		auto consider = [&]( const std::vector<qint32> & A, const std::vector<qint32> & B ) {
			for ( qint32 ia : A ) {
				const BPt & a = pts[size_t( ia )];
				for ( qint32 ib : B ) {
					const BPt & b = pts[size_t( ib )];
					if ( a.b == b.b )
						continue;
					const double dx = double( a.x - b.x ), dy = double( a.y - b.y );
					const double dd = dx * dx + dy * dy;
					if ( dd > double( near ) * double( near ) )
						continue;
					const quint16 lo = qMin( a.b, b.b ), hi = qMax( a.b, b.b );
					const qint64 key = ( qint64( lo ) << 20 ) | hi;
					const double d = std::sqrt( dd );
					auto it = nearest.find( key );
					if ( it == nearest.end() || d < it.value().d ) {
						NearHit h;
						h.d = d;
						h.cx = 0.5 * ( double( a.x ) + double( b.x ) );
						h.cy = 0.5 * ( double( a.y ) + double( b.y ) );
						nearest.insert( key, h );
					}
				}
			}
		};
		for ( int j = 0; j < by; j++ ) {
			for ( int i = 0; i < bx; i++ ) {
				const std::vector<qint32> & A = bucket[size_t( j ) * size_t( bx ) + size_t( i )];
				if ( A.empty() )
					continue;
				for ( int dj = 0; dj <= 1; dj++ ) {
					for ( int di = -1; di <= 1; di++ ) {
						if ( dj == 0 && di < 0 )
							continue;
						const int ni = i + di, nj = j + dj;
						if ( ni < 0 || ni >= bx || nj >= by )
							continue;
						consider( A, bucket[size_t( nj ) * size_t( bx ) + size_t( ni )] );
					}
				}
			}
		}
	}

	// ---- class and flow ---------------------------------------------------
	struct Lower { int body; double d, cx, cy; };
	std::vector<std::vector<Lower>> lower;
	lower.resize( size_t( nBodies ) );
	for ( auto it = nearest.constBegin(); it != nearest.constEnd(); ++it ) {
		const int a = int( it.key() >> 20 ), b = int( it.key() & 0xFFFFF );
		const WaterBody & A = bodies[size_t( a ) - 1];
		const WaterBody & B = bodies[size_t( b ) - 1];
		if ( double( B.wh ) < double( A.wh ) - 0.01 )
			lower[size_t( a ) - 1].push_back( { b, it.value().d, it.value().cx, it.value().cy } );
		if ( double( A.wh ) < double( B.wh ) - 0.01 )
			lower[size_t( b ) - 1].push_back( { a, it.value().d, it.value().cx, it.value().cy } );
	}
	for ( auto & v : lower )
		std::sort( v.begin(), v.end(), []( const Lower & a, const Lower & b ) {
			return a.d != b.d ? a.d < b.d : a.body < b.body;
		} );

	int noVelocity = 0, directionNoSpeed = 0;
	for ( qint64 i = 0; i < nBodies; i++ ) {
		WaterBody & W = bodies[size_t( i )];
		const std::vector<Lower> & lo = lower[size_t( i )];
		W.cls = W.edge ? 0 : ( ( W.elong >= 6.0 || !lo.empty() ) ? 1 : 2 );
		float vx = 0.0f, vy = 0.0f;
		W.hasNam0 = in.velocity && in.velocity( W.form, vx, vy );
		if ( !W.hasNam0 )
			noVelocity++;
		W.nam0X = double( vx );
		W.nam0Y = double( vy );
		const double speed = std::sqrt( W.nam0X * W.nam0X + W.nam0Y * W.nam0Y );
		const double mx = W.n > 0 ? W.sx / W.n : 0.0;
		const double my = W.n > 0 ? W.sy / W.n : 0.0;
		if ( W.cls == 0 ) {
			W.flowSource = 0;                       // the sea, unless a stroke says otherwise
		} else if ( !lo.empty() && W.aniso >= 0.5 ) {
			const double sgn = ( ( lo[0].cx - mx ) * W.axX + ( lo[0].cy - my ) * W.axY ) >= 0.0
				? 1.0 : -1.0;
			W.flowSource = 3;
			W.outlet = lo[0].body;
			W.flowX = W.axX * sgn * speed;
			W.flowY = W.axY * sgn * speed;
		} else if ( std::fabs( W.bedR ) >= 0.7 && std::fabs( W.bedDrop ) >= 64.0
			&& W.aniso >= 0.5 ) {
			const double sgn = W.bedR > 0.0 ? -1.0 : 1.0;   // downhill
			W.flowSource = 2;
			W.flowX = W.axX * sgn * speed;
			W.flowY = W.axY * sgn * speed;
		} else if ( W.aniso < 0.5 && lo.empty() ) {
			W.flowSource = 0;                       // a round lake with no outlet
		} else if ( speed > 0.0 ) {
			W.flowSource = 1;
			W.flowX = W.nam0X;
			W.flowY = W.nam0Y;
		} else {
			W.flowSource = 0;
		}
		if ( W.flowSource && W.flowX == 0.0 && W.flowY == 0.0 )
			directionNoSpeed++;
	}

	// ---- the sections -----------------------------------------------------
	const int bodyS = in.opt.bodySamples > 0 ? qMin( in.opt.bodySamples, spc ) : spc;
	const int flowS = in.opt.flowSamples > 0 ? qMin( in.opt.flowSamples, spc ) : spc;
	if ( spc % bodyS || spc % flowS )
		return fail( QString( "a plane rate must divide the file's own %1 samples a cell; "
			"%2 and %3 do not" ).arg( spc ).arg( bodyS ).arg( flowS ) );
	out.bodySamples = bodyS;
	out.flowSamples = flowS;
	out.shoreSamples = in.opt.shore ? bodyS : 0;

	Buf table;
	for ( qint64 i = 0; i < nBodies; i++ ) {
		const WaterBody & W = bodies[size_t( i )];
		quint8 flags = 0;
		if ( W.ambiguous )
			flags |= 1u << 3;
		if ( W.area < 4 )
			flags |= 1u << 4;
		table.u16( quint16( i + 1 ) );
		table.u8( quint8( W.cls ) );
		table.u8( flags );
		table.f32( W.wh );
		table.u32( W.form );
		table.u32( quint32( W.area ) );
		table.u16( quint16( qint16( in.minX + W.x0 / spc ) ) );
		table.u16( quint16( qint16( in.minY + W.y0 / spc ) ) );
		table.u16( quint16( qint16( in.minX + W.x1 / spc ) ) );
		table.u16( quint16( qint16( in.minY + W.y1 / spc ) ) );
		table.u16( 0 );                       // source: no stroke graph yet
		table.u16( quint16( W.outlet ) );
		table.f32( float( W.flowX ) );
		table.f32( float( W.flowY ) );
		table.u8( 0 ); table.u8( 0 ); table.u8( 0 ); table.u8( 0 );   // colour, A=0 = none
		table.u8( 0 );                        // confidence: nothing constrains it yet
		table.u8( quint8( W.flowSource ) );
		table.u16( 0 );                       // reserved
		table.u32( 0 );                       // name offset: unnamed
	}
	if ( table.size() != qsizetype( nBodies ) * LODL_BODY_RECORD )
		return fail( QString( "the body table assembled to %1 bytes, not %2 x %3" )
			.arg( table.size() ).arg( nBodies ).arg( LODL_BODY_RECORD ) );
	out.bodyTable = table.b;
	out.bodyCount = int( nBodies );

	/* The stroke store is written EMPTY and present: a count of zero. The
	 * planes above are derived from the .lodl plus this store, so a file that
	 * has one -- even an empty one -- is a file a panel can write into without
	 * a version bump, and a consumer can tell "nobody has marked anything" from
	 * "this file predates marking". */
	{
		Buf s;
		s.u32( 0 );
		out.strokeStore = s.b;
	}

	// the shore sweep, on the full grid, before any subsampling
	std::vector<quint16> shore;
	if ( in.opt.shore ) {
		/* A 3-4 chamfer, and the ONE thing that makes it per-body: a neighbour
		 * belonging to a different body counts as a shore at distance zero, so
		 * two bodies that touch each have their own shore along the seam. */
		const quint16 BIG = 60000;
		shore.assign( size_t( samples ), 0 );
		for ( size_t i = 0; i < grid.size(); i++ )
			shore[i] = grid[i] ? BIG : 0;
		auto step = [&]( qint64 y, qint64 x, qint64 ny, qint64 nx, int w ) {
			if ( ny < 0 || ny >= gh || nx < 0 || nx >= gw )
				return;
			const size_t at = size_t( y * gw + x ), nat = size_t( ny * gw + nx );
			if ( !grid[at] )
				return;
			const quint16 cand = ( grid[nat] == grid[at] )
				? quint16( qMin( int( shore[nat] ) + w, int( BIG ) ) )
				: quint16( w );
			if ( cand < shore[at] )
				shore[at] = cand;
		};
		for ( qint64 y = 0; y < gh; y++ )
			for ( qint64 x = 0; x < gw; x++ ) {
				step( y, x, y - 1, x, 3 );
				step( y, x, y - 1, x - 1, 4 );
				step( y, x, y - 1, x + 1, 4 );
				step( y, x, y, x - 1, 3 );
			}
		for ( qint64 y = gh - 1; y >= 0; y-- )
			for ( qint64 x = gw - 1; x >= 0; x-- ) {
				step( y, x, y + 1, x, 3 );
				step( y, x, y + 1, x - 1, 4 );
				step( y, x, y + 1, x + 1, 4 );
				step( y, x, y, x + 1, 3 );
			}
	}

	quint64 pos = baseOffset + quint64( out.bodyTable.size() ) + quint64( out.strokeStore.size() );
	qint64 uniformId = 0, uniformFlow = 0, uniformShore = 0;
	const int stepB = spc / bodyS;
	out.idPlane = lodtPackPlane( cellsX, cellsY, bodyS, 2, pos,
		[&]( int tx, int ty, quint8 * dst ) {
			for ( int j = 0; j < bodyS; j++ ) {
				const qint64 gy = qint64( ty ) * spc + qint64( j ) * stepB;
				for ( int i = 0; i < bodyS; i++ ) {
					const qint64 gx = qint64( tx ) * spc + qint64( i ) * stepB;
					const quint16 v = grid[size_t( gy * gw + gx )];
					dst[( j * bodyS + i ) * 2] = quint8( v & 0xFF );
					dst[( j * bodyS + i ) * 2 + 1] = quint8( v >> 8 );
				}
			}
		}, &uniformId );
	pos += quint64( out.idPlane.size() );

	const int stepF = spc / flowS;
	out.flowPlane = lodtPackPlane( cellsX, cellsY, flowS, 2, pos,
		[&]( int tx, int ty, quint8 * dst ) {
			for ( int j = 0; j < flowS; j++ ) {
				const qint64 gy = qint64( ty ) * spc + qint64( j ) * stepF;
				for ( int i = 0; i < flowS; i++ ) {
					const qint64 gx = qint64( tx ) * spc + qint64( i ) * stepF;
					const quint16 b = grid[size_t( gy * gw + gx )];
					quint16 word = 0;
					if ( b ) {
						const WaterBody & W = bodies[size_t( b ) - 1];
						const double m = std::sqrt( W.flowX * W.flowX + W.flowY * W.flowY );
						if ( m > 0.0 ) {
							double a = std::atan2( W.flowY, W.flowX );
							if ( a < 0.0 )
								a += kTwoPi;
							const int dir = int( a / kTwoPi * 256.0 + 0.5 ) & 0xFF;
							/* With no stroke the field is CONSTANT over the
							 * body and equal to its mean, so speed is exactly
							 * the mid step 8 and confidence 0 -- the value that
							 * tells a consumer to cross-fade to `meanFlow`. */
							word = quint16( dir | ( 8 << 8 ) );
						}
					}
					dst[( j * flowS + i ) * 2] = quint8( word & 0xFF );
					dst[( j * flowS + i ) * 2 + 1] = quint8( word >> 8 );
				}
			}
		}, &uniformFlow );
	pos += quint64( out.flowPlane.size() );

	if ( in.opt.shore ) {
		out.shorePlane = lodtPackPlane( cellsX, cellsY, bodyS, 1, pos,
			[&]( int tx, int ty, quint8 * dst ) {
				for ( int j = 0; j < bodyS; j++ ) {
					const qint64 gy = qint64( ty ) * spc + qint64( j ) * stepB;
					for ( int i = 0; i < bodyS; i++ ) {
						const qint64 gx = qint64( tx ) * spc + qint64( i ) * stepB;
						const size_t at = size_t( gy * gw + gx );
						/* chamfer units are thirds of a texel; a texel is
						 * 4096/spc world units and a step is 32 of them. */
						const double texels = double( shore[at] ) / 3.0;
						const double units = texels * ( 4096.0 / double( spc ) );
						const int v = grid[at]
							? int( qMin( 255.0, units / double( LODL_SHORE_QUANTUM ) + 0.5 ) )
							: 255;
						dst[j * bodyS + i] = quint8( v );
					}
				}
			}, &uniformShore );
	}

	// ---- the census -------------------------------------------------------
	{
		qint64 hist[7] = { 0, 0, 0, 0, 0, 0, 0 };
		const qint64 gate[7] = { 1, 4, 16, 64, 256, 1024, 65536 };
		int cls[3] = { 0, 0, 0 }, clsBig[3] = { 0, 0, 0 };
		int fsrc[5] = { 0, 0, 0, 0, 0 }, fsrcBig[5] = { 0, 0, 0, 0, 0 };
		int multi = 0, tiny = 0;
		QHash<quint32, QPair<int, qint64>> perForm;
		for ( const WaterBody & W : bodies ) {
			for ( int k = 0; k < 7; k++ )
				if ( W.area >= gate[k] )
					hist[k]++;
			cls[W.cls]++;
			fsrc[W.flowSource]++;
			if ( W.area >= 64 ) {
				clsBig[W.cls]++;
				fsrcBig[W.flowSource]++;
			}
			if ( W.parts > 1 )
				multi++;
			if ( W.area < 4 )
				tiny++;
			auto & e = perForm[W.form];
			e.first++;
			e.second += W.area;
		}
		QStringList L;
		L << QStringLiteral( "== INPUT ==" );
		L << QString( "  cells %1..%2 x %3..%4 = %5  spc %6  wet texels %7" )
			.arg( in.minX ).arg( in.minX + cellsX - 1 ).arg( in.minY )
			.arg( in.minY + cellsY - 1 ).arg( qint64( cellsX ) * cellsY ).arg( spc )
			.arg( [&] { qint64 t = 0; for ( const WaterBody & W : bodies ) t += W.area; return t; }() );
		L << QString( "  worldspace default water height %1 type %2" )
			.arg( double( in.defaultWaterHeight ), 0, 'f', 1 )
			.arg( in.defaultWaterType, 8, 16, QChar( '0' ) );
		L << QString( "  bridge gap %1 texels: %2 merges accepted, %3 refused" )
			.arg( gap ).arg( accepted ).arg( refused );
		L << QString( "  rule C: %1 components, %2 merged into a painted neighbour, "
				"%3 refused (the inheriting side was not the smaller), "
				"%4 had more than one candidate" )
			.arg( nComp ).arg( nComp - nCb ).arg( refusedMerges ).arg( ambiguousMerges );
		L << QString();
		L << QStringLiteral( "== BODIES ==" );
		L << QString( "  total %1" ).arg( nBodies );
		for ( int k = 0; k < 7; k++ )
			L << QString( "    area >= %1 texels : %2" )
				.arg( gate[k], 6 ).arg( hist[k], 4 );
		L << QString( "  class: sea %1  river %2  lake %3" ).arg( cls[0] ).arg( cls[1] ).arg( cls[2] );
		L << QString( "  class (area >= 64): sea %1  river %2  lake %3" )
			.arg( clsBig[0] ).arg( clsBig[1] ).arg( clsBig[2] );
		L << QString( "  bodies assembled from more than one component: %1" ).arg( multi );
		L << QString( "  TINY (< 4 texels): %1" ).arg( tiny );
		L << QString();
		L << QStringLiteral( "== PER WATR FORM ==" );
		{
			QList<quint32> forms = perForm.keys();
			std::sort( forms.begin(), forms.end(), [&]( quint32 a, quint32 b ) {
				return perForm[a].second > perForm[b].second;
			} );
			for ( quint32 f : forms )
				L << QString( "  %1 bodies %2  texels %3" )
					.arg( QString::number( f, 16 ).rightJustified( 8, QChar( '0' ) ), -28 )
					.arg( perForm[f].first, 4 ).arg( perForm[f].second, 9 );
		}
		L << QString();
		L << QStringLiteral( "== FLOW SOURCE ==" );
		static const char * const fname[5] = { "none", "form NAM0", "bed", "drain", "stroke" };
		{
			QStringList a, b;
			for ( int k = 0; k < 5; k++ ) {
				if ( fsrc[k] )
					a << QString( "%1 %2" ).arg( QLatin1String( fname[k] ) ).arg( fsrc[k] );
				if ( fsrcBig[k] )
					b << QString( "%1 %2" ).arg( QLatin1String( fname[k] ) ).arg( fsrcBig[k] );
			}
			L << QString( "  all bodies : %1" ).arg( a.join( QStringLiteral( "  " ) ) );
			L << QString( "  area >= 64 : %1" ).arg( b.join( QStringLiteral( "  " ) ) );
		}
		if ( noVelocity )
			L << QString( "  REFUSED ARM: %1 bodies have no WATR NAM0 to fall back on "
					"(%2), so their flow is `none` rather than a made-up number" )
				.arg( noVelocity )
				.arg( in.velocity ? QStringLiteral( "the form carries no NAM0" )
					: QStringLiteral( "no plugin was supplied to read velocities from" ) );
		if ( directionNoSpeed )
			L << QString( "  %1 bodies have a DIRECTION but no speed: the rule that set the "
					"direction carries none and NAM0 was unavailable" ).arg( directionNoSpeed );
		L << QString();
		L << QStringLiteral( "== THE 20 BIGGEST BODIES ==" );
		L << QStringLiteral( "  id   form     class      area   height parts  elong  aniso flow      cells" );
		{
			std::vector<qint64> ord( size_t( nBodies ), qint64( 0 ) );
			for ( qint64 i = 0; i < nBodies; i++ )
				ord[size_t( i )] = i;
			std::sort( ord.begin(), ord.end(), [&]( qint64 a, qint64 b ) {
				return bodies[size_t( a )].area > bodies[size_t( b )].area;
			} );
			static const char * const cname[3] = { "sea", "river", "lake" };
			for ( size_t k = 0; k < ord.size() && k < 20; k++ ) {
				const WaterBody & W = bodies[size_t( ord[k] )];
				L << QString( "  %1 %2 %3 %4 %5 %6 %7 %8 %9 (%10..%11, %12..%13)" )
					.arg( ord[k] + 1, -4 )
					.arg( QString::number( W.form, 16 ).rightJustified( 8, QChar( '0' ) ) )
					.arg( QLatin1String( cname[W.cls] ), -6 )
					.arg( W.area, 9 ).arg( double( W.wh ), 8, 'f', 1 ).arg( W.parts, 5 )
					.arg( W.elong, 6, 'f', 1 ).arg( W.aniso, 6, 'f', 2 )
					.arg( QLatin1String( fname[W.flowSource] ), -9 )
					.arg( in.minX + W.x0 / spc ).arg( in.minX + W.x1 / spc )
					.arg( in.minY + W.y0 / spc ).arg( in.minY + W.y1 / spc );
			}
		}
		out.census = L.join( QStringLiteral( "\n" ) );
		out.summary = QString( "water: %1 bodies (sea %2, river %3, lake %4), "
			"%5 bridge merges accepted / %6 refused, planes id %7 + flow %8 + shore %9 bytes, "
			"uniform tiles %10/%11/%12 of %13" )
			.arg( nBodies ).arg( cls[0] ).arg( cls[1] ).arg( cls[2] )
			.arg( accepted ).arg( refused )
			.arg( out.idPlane.size() ).arg( out.flowPlane.size() ).arg( out.shorePlane.size() )
			.arg( uniformId ).arg( uniformFlow ).arg( uniformShore )
			.arg( qint64( cellsX ) * cellsY );
	}
	if ( error )
		error->clear();
	return true;
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
	int filledCells = 0;   // lane FIX1: landless cells the landFill option filled
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
				if ( src.fillRange && src.fillRange( cx, cy, lo, hi ) )
					filledCells++;   // lane FIX1: the landless-cell fill
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
	/* The water module RAISES the version and nothing else does. A caller who
	 * did not ask for bodies gets the same bytes it got before this section
	 * existed, which is gate G1 and also the module's own fallback floor. */
	const bool wantWater = opts.water.enabled;
	if ( wantWater && version < 3 )
		version = 3;
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
	const qsizetype offSectAt = h.size();
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
	/* Version 3's water fields, from 0xA0. They are written as zeroes and
	 * patched at the end, because the sections they point at are appended AFTER
	 * the block data -- which is what keeps every version-2 offset at the byte
	 * it was. A section bit stays clear until its section really exists. */
	qsizetype offBodyAt = 0, offBodyCountAt = 0, offNameAt = 0, offNameLenAt = 0;
	qsizetype offIdRateAt = 0, offIdAt = 0, offFlowRateAt = 0, offFlowAt = 0;
	qsizetype offShoreRateAt = 0, offShoreAt = 0, offStrokeAt = 0, offStrokeLenAt = 0;
	if ( version >= 3 ) {
		offBodyAt = h.size();       h.u64( 0 );
		offBodyCountAt = h.size();  h.u32( 0 ); h.u32( quint32( LODL_BODY_RECORD ) );
		offNameAt = h.size();       h.u64( 0 );
		offNameLenAt = h.size();    h.u32( 0 );
		offIdRateAt = h.size();     h.u32( 0 );
		offIdAt = h.size();         h.u64( 0 );
		offFlowRateAt = h.size();   h.u32( 0 ); h.u32( 0 );   // rate, encoding 0
		offFlowAt = h.size();       h.u64( 0 );
		offShoreRateAt = h.size();  h.u32( 0 );
		h.u32( quint32( LODL_SHORE_QUANTUM ) );
		offShoreAt = h.size();      h.u64( 0 );
		offStrokeAt = h.size();     h.u64( 0 );
		offStrokeLenAt = h.size();  h.u32( 0 );
		h.u32( 0 );                                            // reserved
	}
	/* The header size is what every section offset is measured against, so it
	 * is checked here rather than asserted in a debug build nobody runs. */
	if ( h.size() != lodtHeaderBytes( int( version ) ) )
		return fail( QStringLiteral( "header assembled to %1 bytes, not the %2 version %3 declares" )
			.arg( h.size() ).arg( lodtHeaderBytes( int( version ) ) ).arg( version ) );

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
	/* ONE ROOT (lane LAYOUT1, 2026-09-16, bungo's 19:3x ruling): the landscape
	 * file used to go to `Terrain\<ws>.lodl`, an engine folder that no engine
	 * reader ever opens it from. It is `FO4CSLOD\<ws>\<ws>.lodl` now, composed
	 * by lodgenFo4csWorldDir() like every other FO4CS-target file. */
	const QString dir = lodgenFo4csWorldDir( outDir, edid );
	QDir().mkpath( dir );
	const QString path = dir + QStringLiteral( "/" ) + edid + QStringLiteral( ".lodl" );
	lodgenNoteLayoutFile( path );
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

	/* ---- version 3: the water sections, APPENDED ------------------------
	 *
	 * They go after the block data, which is why nothing a version-2 reader
	 * addresses moves by a byte. Every one is optional and its bit in the
	 * section word is only set once the bytes exist. */
	QString waterSummary;
	if ( version >= 3 && wantWater ) {
		WaterInput wi;
		wi.cellsX = cellsX;
		wi.cellsY = cellsY;
		wi.spc = spc;
		wi.minX = minX;
		wi.minY = minY;
		wi.quantum = quantum;
		wi.cellFlags = &cellFlags;
		wi.cellWaterH = &cellWaterH;
		wi.cellWaterT = &cellWaterT;
		wi.watrForms = &watrForms;
		wi.defaultWaterType = src.defaultWaterType;
		wi.defaultWaterHeight = src.defaultWaterHeight;
		wi.opt = opts.water;
		/* One decoded cell per cell per row, out of the same cache the pyramid
		 * used, so the two passes over the world cost cache hits and not LAND
		 * parses. */
		wi.heightRow = [&]( int gy, quint16 * row ) {
			const int cy = minY + gy / spc;
			const int ly = gy % spc;
			for ( int cx = minX; cx <= maxX; cx++ ) {
				quint16 * dst = row + qsizetype( cx - minX ) * spc;
				const CellPlanes * cp = planesFor( cx, cy );
				if ( !cp || cp->h.empty() ) {
					const quint16 dflt = lodtHeightWord( src.defaultLand, quantum );
					for ( int i = 0; i < spc; i++ )
						dst[i] = dflt;
				} else {
					std::memcpy( dst, cp->h.data() + size_t( ly ) * size_t( spc ),
						size_t( spc ) * 2 );
				}
			}
		};
		QHash<quint32, QPair<float, float>> nam0;
		if ( !opts.water.velocityPlugin.isEmpty() ) {
			QSet<quint32> want;
			for ( quint32 wf : watrForms )
				want.insert( wf );
			if ( src.defaultWaterType )
				want.insert( src.defaultWaterType );
			try {
				ESMFile esmv( opts.water.velocityPlugin.toLocal8Bit().constData() );
				for ( quint32 wf : want ) {
					const ESMFile::ESMRecord * r = esmv.findRecord( wf );
					if ( !r || !( *r == "WATR" ) )
						continue;
					ESMFile::ESMField fld( esmv, *r );
					while ( fld.next() ) {
						if ( fld == "NAM0" && fld.size() >= 12 ) {
							const float vx = fld.readFloat();
							const float vy = fld.readFloat();
							nam0.insert( wf, qMakePair( vx, vy ) );
							break;
						}
					}
				}
			} catch ( ... ) {
				nam0.clear();   // a plugin we cannot read is a MISSING arm, not a crash
			}
		}
		if ( !nam0.isEmpty() ) {
			wi.velocity = [nam0]( quint32 form, float & vx, float & vy ) {
				auto it = nam0.constFind( form );
				if ( it == nam0.constEnd() )
					return false;
				vx = it.value().first;
				vy = it.value().second;
				return true;
			};
		}
		WaterOut wo;
		QString werr;
		if ( !lodtBuildWater( wi, pos, wo, &werr ) ) {
			f.close();
			QFile::remove( path );
			return fail( QStringLiteral( "water bodies: %1" ).arg( werr ) );
		}
		quint32 sect = ( waterCells ? SECT_WATER : 0u )
			| ( colourCells ? SECT_COLOUR : 0u )
			| ( gcvrForms.isEmpty() ? 0u : SECT_GROUNDCOVER )
			| ( opts.aoSamples > 0 ? SECT_AO : 0u );

		patch64( hdr, offBodyAt, pos );
		patch32( hdr, offBodyCountAt, quint32( wo.bodyCount ) );
		wr( wo.bodyTable );
		patch64( hdr, offStrokeAt, pos );
		patch32( hdr, offStrokeLenAt, quint32( wo.strokeStore.size() ) );
		wr( wo.strokeStore );
		sect |= SECT_STROKE;
		patch32( hdr, offIdRateAt, quint32( wo.bodySamples ) );
		patch64( hdr, offIdAt, pos );
		wr( wo.idPlane );
		sect |= SECT_BODIES;
		patch32( hdr, offFlowRateAt, quint32( wo.flowSamples ) );
		patch64( hdr, offFlowAt, pos );
		wr( wo.flowPlane );
		sect |= SECT_FLOW;
		if ( !wo.shorePlane.isEmpty() ) {
			patch32( hdr, offShoreRateAt, quint32( wo.shoreSamples ) );
			patch64( hdr, offShoreAt, pos );
			wr( wo.shorePlane );
			sect |= SECT_SHORE;
		}
		// the name blob stays empty until something names a body
		patch64( hdr, offNameAt, 0 );
		patch32( hdr, offNameLenAt, 0 );
		patch32( hdr, offSectAt, sect );
		waterSummary = wo.summary;
		if ( !opts.water.reportPath.isEmpty() ) {
			QFile rf( opts.water.reportPath );
			if ( rf.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
				rf.write( ( wo.census + QStringLiteral( "\n" ) ).toUtf8() );
		}
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
	if ( error && src.fillRange )
		*error += QStringLiteral( "\n  landless-cell fill: %1 cells filled" ).arg( filledCells );
	if ( error && !waterSummary.isEmpty() )
		*error += QStringLiteral( "\n  " ) + waterSummary;
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
	/* Lane FIX1: the landless-cell fill's range (LodtOptions::landFill). Rows
	 * and columns 0..31, the samples this cell stores; an inherited one is
	 * the edge rule's and the caller merges edgeRange on top. */
	if ( opts.landFill ) {
		src.fillRange = [get, fill = opts.landFill]( int cx, int cy, float & lo, float & hi ) {
			if ( get( cx, cy ) )
				return false;
			float fh[33 * 33];
			if ( !fill( cx, cy, fh ) )
				return false;
			lo = 3.4e38f;
			hi = -3.4e38f;
			for ( int r = 0; r < 32; r++ )
				for ( int c = 0; c < 32; c++ ) {
					lo = qMin( lo, fh[r * 33 + c] );
					hi = qMax( hi, fh[r * 33 + c] );
				}
			return true;
		};
	}
	src.planes = [get, seams, defaultLand = world.defaultLandHeight(), fill = opts.landFill](
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
			/* Lane FIX1: a landless cell the landFill option has heights for
			 * (the game's own terrain LOD there) takes them wherever it does
			 * not inherit a real sample across an edge. */
			float fh[33 * 33];
			const bool filled = fill && fill( cx, cy, fh );
			if ( !sm.s && !sm.w && !sm.sw && !filled )
				return false;                // nothing inherited: the caller's default
			h.assign( size_t( spc ) * spc, 0 );
			a.assign( size_t( spc ) * spc, 0 );
			c.assign( size_t( spc ) * spc, 0xFFFFU );
			g.clear();
			for ( int r = 0; r < spc; r++ ) {
				for ( int cc = 0; cc < spc; cc++ ) {
					float hh = filled ? fh[r * 33 + cc] : defaultLand;
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
		/* A TABLE, and it runs before the version refusal on purpose: an
		 * unknown version answers 0, which makes the offset check below fail
		 * closed instead of measuring a version-3 file against version 2's
		 * 160-byte floor. That ternary was harmless only while no third
		 * version existed. */
		const qsizetype hdrBytes = lodtHeaderBytes( ver );
		/* And the version refusal comes FIRST now, not after the prefix read:
		 * an unknown version has no header size, so there is nothing honest to
		 * measure the offsets against. */
		if ( !hdrBytes || quint32( ver ) < LODL_VERSION_MIN || quint32( ver ) > LODL_VERSION )
			return fail( QStringLiteral( "unsupported version %1 (this reader knows %2..%3)" )
				.arg( ver ).arg( LODL_VERSION_MIN ).arg( LODL_VERSION ) );
		const quint64 dataAt = rd<quint64>( buf, 0x88 );
		const quint64 total = rd<quint64>( buf, 0x90 );
		if ( dataAt < quint64( hdrBytes ) || dataAt > total || total != quint64( file.size() ) )
			return fail( QStringLiteral( "header offsets do not fit the file" ) );
		file.seek( 0 );
		buf = file.read( qint64( dataAt ) );
		if ( quint64( buf.size() ) != dataAt )
			return fail( QStringLiteral( "short read of the file prefix" ) );
	}
	/* (The version refusal used to be HERE, after the prefix read. It moved up,
	 * beside the header-size table, because an unknown version cannot be given
	 * an honest floor to measure `blockDataOffset` against. Leaving a second
	 * copy behind would be dead code that reads like a second gate.) */
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

	/* ---- version 3: water bodies -----------------------------------------
	 *
	 * These sections sit AFTER the block data, so they are not in `buf`. Each
	 * is read on its own, and each way of being wrong REFUSES BY NAME: a bit
	 * set over an empty offset, a record stride this build cannot read, an id
	 * past the table, an id that is not its own index. A silent zero here would
	 * be a body that draws as "no water". */
	if ( ver >= 3 && ( sect & LODL_SECT_BODIES ) ) {
		const quint64 oBody = rd<quint64>( buf, 0xA0 );
		const int count = int( rd<quint32>( buf, 0xA8 ) );
		bodyStride = int( rd<quint32>( buf, 0xAC ) );
		bodyS = int( rd<quint32>( buf, 0xBC ) );
		const quint64 oId = rd<quint64>( buf, 0xC0 );
		if ( !oBody || count <= 0 )
			return fail( QStringLiteral( "section bodies is declared present but its "
				"offset or count is empty" ) );
		if ( !bodyS || !oId )
			return fail( QStringLiteral( "section bodies is declared present but the "
				"body-ID plane's rate or offset is empty" ) );
		if ( bodyStride < LODL_BODY_RECORD )
			return fail( QStringLiteral( "the body table's records are %1 bytes; this "
				"reader knows %2" ).arg( bodyStride ).arg( LODL_BODY_RECORD ) );
		const qint64 need = qint64( count ) * bodyStride;
		if ( qint64( oBody ) + need > file.size() )
			return fail( QStringLiteral( "the body table runs past the end of the file" ) );
		if ( !file.seek( qint64( oBody ) ) )
			return fail( QStringLiteral( "cannot seek to the body table" ) );
		const QByteArray tb = file.read( need );
		if ( tb.size() != need )
			return fail( QStringLiteral( "short read of the body table" ) );
		bodies.resize( count );
		for ( int i = 0; i < count; i++ ) {
			const qsizetype at = qsizetype( i ) * bodyStride;
			LodtWaterBody & b = bodies[i];
			b.id = rd<quint16>( tb, at );
			if ( int( b.id ) != i + 1 )
				return fail( QStringLiteral( "body table record %1 says it is body %2; "
					"record i is body i + 1" ).arg( i ).arg( b.id ) );
			b.cls = rd<quint8>( tb, at + 0x02 );
			b.flags = rd<quint8>( tb, at + 0x03 );
			b.waterHeight = rd<float>( tb, at + 0x04 );
			b.watrForm = rd<quint32>( tb, at + 0x08 );
			b.area = rd<quint32>( tb, at + 0x0C );
			b.x0 = rd<qint16>( tb, at + 0x10 );
			b.y0 = rd<qint16>( tb, at + 0x12 );
			b.x1 = rd<qint16>( tb, at + 0x14 );
			b.y1 = rd<qint16>( tb, at + 0x16 );
			b.source = rd<quint16>( tb, at + 0x18 );
			b.outlet = rd<quint16>( tb, at + 0x1A );
			b.flowX = rd<float>( tb, at + 0x1C );
			b.flowY = rd<float>( tb, at + 0x20 );
			for ( int k = 0; k < 4; k++ )
				b.colour[k] = rd<quint8>( tb, at + 0x24 + k );
			b.confidence = rd<quint8>( tb, at + 0x28 );
			b.flowSource = rd<quint8>( tb, at + 0x29 );
			b.nameOffset = rd<quint32>( tb, at + 0x2C );
		}
		const quint64 oName = rd<quint64>( buf, 0xB0 );
		const quint32 nameLen = rd<quint32>( buf, 0xB8 );
		if ( oName && nameLen ) {
			file.seek( qint64( oName ) );
			nameBlob = file.read( nameLen );
		}
		if ( !readPlaneStore( oId, 2, idStore, error ) )
			return false;
		/* An id past the table is REFUSED, not clamped. Every UNIFORM tile's
		 * value is in the directory, so this costs a directory scan and catches
		 * the whole of the sea; a compressed tile's ids are checked by the
		 * census sweep (lodtWaterCensus), which is what the harness runs,
		 * because inflating 36,864 tiles at open would make every File > Open
		 * pay for a verification. */
		for ( qint64 t = 0; t < qint64( idStore.tilesX ) * idStore.tilesY; t++ ) {
			const qsizetype e = qsizetype( t ) * 16;
			if ( rd<quint32>( idStore.dir, e + 8 ) )
				continue;                       // not uniform
			const quint32 v = rd<quint32>( idStore.dir, e + 12 );
			if ( v > quint32( count ) )
				return fail( QStringLiteral( "the body-ID plane names body %1 and the table "
					"holds %2" ).arg( v ).arg( count ) );
		}
	}
	if ( ver >= 3 && ( sect & LODL_SECT_FLOW ) ) {
		flowS = int( rd<quint32>( buf, 0xC8 ) );
		flowEnc = rd<quint32>( buf, 0xCC );
		const quint64 oFlow = rd<quint64>( buf, 0xD0 );
		if ( !flowS || !oFlow )
			return fail( QStringLiteral( "section flow is declared present but its rate "
				"or offset is empty" ) );
		if ( !readPlaneStore( oFlow, 2, flowStore, error ) )
			return false;
	}
	if ( ver >= 3 && ( sect & LODL_SECT_SHORE ) ) {
		shoreS = int( rd<quint32>( buf, 0xD8 ) );
		shoreQ = float( rd<quint32>( buf, 0xDC ) );
		const quint64 oShore = rd<quint64>( buf, 0xE0 );
		if ( !shoreS || !oShore )
			return fail( QStringLiteral( "section shore is declared present but its rate "
				"or offset is empty" ) );
		if ( !( shoreQ > 0.0f ) )
			return fail( QStringLiteral( "the shore quantum must be positive" ) );
		if ( !readPlaneStore( oShore, 1, shoreStore, error ) )
			return false;
	}
	if ( ver >= 3 && ( sect & LODL_SECT_STROKE ) ) {
		const quint64 oStroke = rd<quint64>( buf, 0xE8 );
		const quint32 len = rd<quint32>( buf, 0xF0 );
		if ( !oStroke || len < 4 )
			return fail( QStringLiteral( "section strokes is declared present but its "
				"offset or length is empty" ) );
		file.seek( qint64( oStroke ) );
		strokes = file.read( len );
		if ( quint32( strokes.size() ) != len )
			return fail( QStringLiteral( "short read of the stroke store" ) );
		nStrokes = int( rd<quint32>( strokes, 0 ) );
	}
	if ( ver >= 3 && ( sect & LODL_SECT_DYE ) ) {
		/* The dye plane's offset is the version-3 header's reserved word at
		 * 0xF4, 32 bits wide; the container it points at carries its own
		 * rate and sample size, which is why one word is enough. */
		dyeAt = quint64( rd<quint32>( buf, 0xF4 ) );
		if ( !dyeAt )
			return fail( QStringLiteral( "section dye is declared present but its offset "
				"is empty" ) );
		if ( !readPlaneStore( dyeAt, 4, dyeStore, error ) )
			return false;
		dyeS = dyeStore.tileEdge;
		if ( !dyeS )
			return fail( QStringLiteral( "the dye plane declares 0 samples a cell" ) );
	}

	if ( error )
		error->clear();
	return true;
}

/*! One plane container's fixed head and directory. The payloads stay on disk;
 *  a uniform tile has no payload at all. */
bool LodtFile::readPlaneStore( quint64 at, int bytesPerSample, PlaneStore & s,
	QString * error ) const
{
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};
	if ( !file.seek( qint64( at ) ) )
		return fail( QStringLiteral( "cannot seek to a plane store" ) );
	const QByteArray head = file.read( 32 );
	if ( head.size() != 32 )
		return fail( QStringLiteral( "short read of a plane store header" ) );
	s.tilesX = int( rd<quint32>( head, 0 ) );
	s.tilesY = int( rd<quint32>( head, 4 ) );
	s.tileEdge = int( rd<quint32>( head, 8 ) );
	s.bytesPerSample = int( rd<quint32>( head, 12 ) );
	s.dirOffset = rd<quint64>( head, 16 );
	s.dataOffset = rd<quint64>( head, 24 );
	if ( s.tilesX != cellsX() || s.tilesY != cellsY() )
		return fail( QStringLiteral( "a plane store tiles %1x%2 where the worldspace is "
			"%3x%4 cells" ).arg( s.tilesX ).arg( s.tilesY ).arg( cellsX() ).arg( cellsY() ) );
	if ( s.tileEdge <= 0 || s.bytesPerSample != bytesPerSample )
		return fail( QStringLiteral( "a plane store declares %1 bytes a sample where this "
			"plane is %2" ).arg( s.bytesPerSample ).arg( bytesPerSample ) );
	const qint64 dirBytes = qint64( s.tilesX ) * s.tilesY * 16;
	if ( qint64( s.dirOffset ) + dirBytes > file.size() || s.dataOffset < s.dirOffset + quint64( dirBytes ) )
		return fail( QStringLiteral( "a plane store's directory does not fit the file" ) );
	if ( !file.seek( qint64( s.dirOffset ) ) )
		return fail( QStringLiteral( "cannot seek to a plane store's directory" ) );
	s.dir = file.read( dirBytes );
	if ( s.dir.size() != dirBytes )
		return fail( QStringLiteral( "short read of a plane store's directory" ) );
	s.ok = true;
	return true;
}

quint32 LodtFile::planeSampleOf( const PlaneStore & s, int x, int y, quint32 absent ) const
{
	if ( !s.ok || s.tileEdge <= 0 )
		return absent;
	const int tx = x / s.tileEdge, ty = y / s.tileEdge;
	if ( x < 0 || y < 0 || tx >= s.tilesX || ty >= s.tilesY )
		return absent;
	const qsizetype e = ( qsizetype( ty ) * s.tilesX + tx ) * 16;
	const quint64 off = rd<quint64>( s.dir, e );
	const quint32 csz = rd<quint32>( s.dir, e + 8 );
	const quint32 usz = rd<quint32>( s.dir, e + 12 );
	const int lx = x % s.tileEdge, ly = y % s.tileEdge;
	const qsizetype k = ( qsizetype( ly ) * s.tileEdge + lx ) * s.bytesPerSample;
	if ( !csz )
		return usz;                       // UNIFORM: the sample itself lives here
	const quint64 key = ( quint64( quintptr( &s ) ) << 1 ) ^ ( quint64( ty ) << 20 ) ^ quint64( tx );
	QByteArray raw = tileCache.value( key );
	if ( raw.isEmpty() ) {
		if ( !file.seek( qint64( off ) ) )
			return absent;
		QByteArray z = file.read( csz );
		if ( quint32( z.size() ) != csz )
			return absent;
		QByteArray withLen;
		withLen.resize( 4 );
		withLen[0] = char( ( usz >> 24 ) & 0xFF );
		withLen[1] = char( ( usz >> 16 ) & 0xFF );
		withLen[2] = char( ( usz >> 8 ) & 0xFF );
		withLen[3] = char( usz & 0xFF );
		withLen.append( z );
		raw = qUncompress( withLen );
		if ( raw.isEmpty() )
			return absent;
		if ( tileOrder.size() >= 32 ) {
			tileCache.remove( tileOrder.first() );
			tileOrder.removeFirst();
		}
		tileCache.insert( key, raw );
		tileOrder.append( key );
	}
	if ( k + s.bytesPerSample > raw.size() )
		return absent;
	quint32 v = 0;
	for ( int c = 0; c < s.bytesPerSample; c++ )
		v |= quint32( quint8( raw.at( k + c ) ) ) << ( 8 * c );
	return v;
}

bool LodtFile::waterBody( int id, LodtWaterBody & out ) const
{
	if ( id < 1 || id > bodies.size() )
		return false;
	out = bodies[id - 1];
	return true;
}

QString LodtFile::bodyName( const LodtWaterBody & b ) const
{
	if ( !b.nameOffset || qsizetype( b.nameOffset ) >= nameBlob.size() )
		return QString();
	return QString::fromUtf8( nameBlob.constData() + b.nameOffset );
}

quint16 LodtFile::bodyIdAt( int bx, int by ) const
{
	/* 0 = no water here, which is also what an absent plane answers -- the same
	 * discipline as AO's 255: the neutral value is the one that draws nothing
	 * rather than the one that draws garbage. */
	return quint16( planeSampleOf( idStore, bx, by, 0 ) );
}

quint16 LodtFile::flowWordAt( int fx, int fy ) const
{
	return quint16( planeSampleOf( flowStore, fx, fy, 0 ) );
}

quint8 LodtFile::shoreAt( int sx, int sy ) const
{
	return quint8( planeSampleOf( shoreStore, sx, sy, 255 ) );
}

quint32 LodtFile::dyeWordAt( int dx, int dy ) const
{
	// 0 = no dye, which is also what an absent plane answers
	return planeSampleOf( dyeStore, dx, dy, 0 );
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

/* =========================================================================
 *  --water-census: the body table, read back OUT OF THE FILE
 *
 *  It never consults the writer's own tables. A census printed from what the
 *  writer meant to write would echo intent; this echoes the bytes, which is the
 *  only thing a consumer will ever see. The sweep over the body-ID plane is
 *  also where an id past the table is caught in a COMPRESSED tile -- open()
 *  only checks the uniform ones, because it must not make File > Open pay for
 *  a verification.
 * ========================================================================= */
bool lodtWaterCensus( const QString & path, QString * text, QString * error )
{
	LodtFile f;
	if ( !f.open( path, error ) )
		return false;
	auto fail = [error]( const QString & m ) {
		if ( error ) *error = m;
		return false;
	};
	if ( !( f.sectionFlags() & LODL_SECT_BODIES ) || f.bodyCount() <= 0 )
		return fail( QString( "%1 carries no water body table (version %2, section flags "
			"0x%3). Write it with --water-bodies" )
			.arg( QFileInfo( path ).fileName() ).arg( f.headerVersion() )
			.arg( f.sectionFlags(), 0, 16 ) );

	const int n = f.bodyCount();
	const int rate = f.bodyIdSamples();
	const qint64 pw = qint64( f.cellsX() ) * rate, ph = qint64( f.cellsY() ) * rate;
	std::vector<qint64> seen( size_t( n ) + 1, 0 );
	qint64 wetPlane = 0;
	for ( qint64 y = 0; y < ph; y++ ) {
		for ( qint64 x = 0; x < pw; x++ ) {
			const quint16 b = f.bodyIdAt( int( x ), int( y ) );
			if ( !b )
				continue;
			if ( int( b ) > n )
				return fail( QString( "the body-ID plane names body %1 at (%2, %3) and the "
					"table holds %4" ).arg( b ).arg( x ).arg( y ).arg( n ) );
			seen[b]++;
			wetPlane++;
		}
	}

	QStringList L;
	static const char * const cname[3] = { "sea", "river", "lake" };
	static const char * const fname[5] = { "none", "form NAM0", "bed", "drain", "stroke" };
	int cls[3] = { 0, 0, 0 }, fsrc[5] = { 0, 0, 0, 0, 0 };
	qint64 mismatched = 0, tiny = 0;
	QHash<quint32, QPair<int, qint64>> perForm;
	for ( int i = 1; i <= n; i++ ) {
		LodtWaterBody b;
		f.waterBody( i, b );
		if ( b.cls < 3 )
			cls[b.cls]++;
		if ( b.flowSource < 5 )
			fsrc[b.flowSource]++;
		if ( b.flags & ( 1u << 4 ) )
			tiny++;
		/* The table's area and the plane's own count are two measurements of
		 * the same thing, and a field that is written but never checked against
		 * the thing it describes is exactly the counter this tree has a rule
		 * about. They agree only when the plane is at the file's full rate. */
		if ( rate == f.samplesPerCell() && qint64( b.area ) != seen[i] )
			mismatched++;
		auto & e = perForm[b.watrForm];
		e.first++;
		e.second += b.area;
	}
	L << QString( "file %1  version %2  sections 0x%3" )
		.arg( QFileInfo( path ).fileName() ).arg( f.headerVersion() )
		.arg( f.sectionFlags(), 0, 16 );
	L << QString( "bodies %1  record %2 bytes  planes: id %3/cell, flow %4/cell, "
			"shore %5/cell (quantum %6 units)  strokes %7" )
		.arg( n ).arg( f.bodyRecordBytes() ).arg( f.bodyIdSamples() )
		.arg( f.flowPlaneSamples() ).arg( f.shorePlaneSamples() )
		.arg( double( f.shoreQuantum() ), 0, 'f', 0 ).arg( f.strokeCount() );
	L << QString( "body-ID plane %1 x %2, %3 texels name a body" ).arg( pw ).arg( ph ).arg( wetPlane );
	L << QString( "table area vs plane count: %1 of %2 bodies disagree%3" )
		.arg( mismatched ).arg( n )
		.arg( rate == f.samplesPerCell() ? QString()
			: QStringLiteral( " (not checked: the plane is coarser than the file)" ) );
	L << QString( "class: sea %1  river %2  lake %3" ).arg( cls[0] ).arg( cls[1] ).arg( cls[2] );
	{
		QStringList a;
		for ( int k = 0; k < 5; k++ )
			if ( fsrc[k] )
				a << QString( "%1 %2" ).arg( QLatin1String( fname[k] ) ).arg( fsrc[k] );
		L << QString( "flow source: %1" ).arg( a.join( QStringLiteral( "  " ) ) );
	}
	L << QString( "TINY (< 4 texels): %1" ).arg( tiny );
	L << QString();
	L << QStringLiteral( "per WATR form:" );
	{
		QList<quint32> forms = perForm.keys();
		std::sort( forms.begin(), forms.end(), [&]( quint32 a, quint32 b ) {
			return perForm[a].second > perForm[b].second;
		} );
		for ( quint32 fm : forms )
			L << QString( "  %1  bodies %2  texels %3" )
				.arg( QString::number( fm, 16 ).rightJustified( 8, QChar( '0' ) ) )
				.arg( perForm[fm].first, 4 ).arg( perForm[fm].second, 9 );
	}
	L << QString();
	L << QStringLiteral( "  id   form     class      area   height flow      outlet  flow x, y            cells" );
	{
		std::vector<int> ord( size_t( n ), 0 );
		for ( int i = 0; i < n; i++ )
			ord[size_t( i )] = i + 1;
		std::sort( ord.begin(), ord.end(), [&]( int a, int b ) {
			LodtWaterBody x, y;
			f.waterBody( a, x );
			f.waterBody( b, y );
			return x.area > y.area;
		} );
		for ( size_t k = 0; k < ord.size() && k < 40; k++ ) {
			LodtWaterBody b;
			f.waterBody( ord[k], b );
			L << QString( "  %1 %2 %3 %4 %5 %6 %7 %8 (%9..%10, %11..%12)" )
				.arg( b.id, -4 )
				.arg( QString::number( b.watrForm, 16 ).rightJustified( 8, QChar( '0' ) ) )
				.arg( QLatin1String( b.cls < 3 ? cname[b.cls] : "?" ), -6 )
				.arg( b.area, 9 ).arg( double( b.waterHeight ), 8, 'f', 1 )
				.arg( QLatin1String( b.flowSource < 5 ? fname[b.flowSource] : "?" ), -9 )
				.arg( b.outlet, 6 )
				.arg( QString( "%1, %2" ).arg( double( b.flowX ), 0, 'f', 3 )
					.arg( double( b.flowY ), 0, 'f', 3 ), -18 )
				.arg( b.x0 ).arg( b.x1 ).arg( b.y0 ).arg( b.y1 );
		}
	}
	if ( text )
		*text = L.join( QStringLiteral( "\n" ) );
	if ( error )
		error->clear();
	return true;
}

/* =========================================================================
 *  THE KNOWN-ANSWER CONTROL  (`lodl --water-selftest`)
 *
 *  ww-control-calibration step 1, and CONSTITUTION rule 4: run the metric on
 *  an input whose answer was written down before the code, and show the check
 *  FAILING on the other side of the floor.
 *
 *  The worldspace is lane WATER1's `scratchpad/water_20260909/control_synth.py`
 *  built here in C++ instead of numpy, so the CLASSIFIER under test is the one
 *  that writes real files -- not a second implementation of it:
 *
 *    24 x 24 cells at 32 samples an edge, dry ground +1000, default water 450.
 *    SEA     texel columns 0..95 cut to -100, inheriting -- reaches the edge.
 *    RIVER   a 16-texel channel at y 400..415, x 96..607, cut to -50, water
 *            type 1, its plane stepping +50 a cell from 450 to 1200.
 *    LAKE    cells 14..17 square, floor +800, plane 1000, type 2.
 *    PUDDLE  a 4x4 dip at (200, 600), floor 0, plane 450, type 3.
 *
 *  Expected, before the run: FOUR bodies, classed sea / river / lake / lake.
 *  The REFUTER is the measurement that earns keying bodies on water TYPE at
 *  all: with the type ignored, the tidal river reach sits at exactly the sea's
 *  height and touches it, and the two fuse.
 * ========================================================================= */
bool lodtWaterSelfTest( QString * text, QString * error )
{
	const int SPC = 32, CELLS = 24;
	const int N = CELLS * SPC;
	std::vector<float> terrain( size_t( N ) * N, 1000.0f );
	std::vector<float> wh( size_t( CELLS ) * CELLS, 450.0f );
	std::vector<quint16> wt( size_t( CELLS ) * CELLS, WATER_TYPE_DEFAULT );
	std::vector<quint16> fl( size_t( CELLS ) * CELLS, CELL_HAS_WATER | CELL_HAS_LAND );
	auto T = [&]( int y, int x ) -> float & { return terrain[size_t( y ) * N + x]; };
	for ( int y = 0; y < N; y++ )
		for ( int x = 0; x < 96; x++ )
			T( y, x ) = -100.0f;
	for ( int y = 400; y < 416; y++ )
		for ( int x = 96; x < 608; x++ )
			T( y, x ) = -50.0f;
	for ( int cx = 3; cx < 19; cx++ ) {
		wt[size_t( 12 ) * CELLS + cx] = 1;
		wh[size_t( 12 ) * CELLS + cx] = 450.0f + 50.0f * float( cx - 3 );
	}
	for ( int y = 14 * SPC; y < 18 * SPC; y++ )
		for ( int x = 14 * SPC; x < 18 * SPC; x++ )
			T( y, x ) = 800.0f;
	for ( int cy = 14; cy < 18; cy++ )
		for ( int cx = 14; cx < 18; cx++ ) {
			wh[size_t( cy ) * CELLS + cx] = 1000.0f;
			wt[size_t( cy ) * CELLS + cx] = 2;
		}
	for ( int y = 600; y < 604; y++ )
		for ( int x = 200; x < 204; x++ )
			T( y, x ) = 0.0f;
	wt[size_t( 600 / SPC ) * CELLS + ( 200 / SPC )] = 3;

	QVector<quint32> forms;
	forms << 0x11111111u << 0x22222222u << 0x33333333u << 0x44444444u;

	auto run = [&]( bool typeBlind, WaterOut & wo, QString * err ) {
		std::vector<quint16> wtUse = wt;
		if ( typeBlind )
			std::fill( wtUse.begin(), wtUse.end(), quint16( WATER_TYPE_DEFAULT ) );
		WaterInput in;
		in.cellsX = in.cellsY = CELLS;
		in.spc = SPC;
		in.minX = in.minY = 0;
		in.quantum = 8.0f;
		in.cellFlags = &fl;
		in.cellWaterH = &wh;
		in.cellWaterT = &wtUse;
		in.watrForms = &forms;
		in.defaultWaterType = 0x11111111u;
		in.defaultWaterHeight = 450.0f;
		in.opt.enabled = true;
		in.heightRow = [&]( int gy, quint16 * row ) {
			for ( int x = 0; x < N; x++ )
				row[x] = lodtHeightWord( double( terrain[size_t( gy ) * N + x] ), 8.0 );
		};
		return lodtBuildWater( in, 0, wo, err );
	};

	QStringList L;
	WaterOut real, blind;
	QString e1, e2;
	if ( !run( false, real, &e1 ) )
		{ if ( error ) *error = e1; return false; }
	if ( !run( true, blind, &e2 ) )
		{ if ( error ) *error = e2; return false; }

	/* The census text carries the counts; the assertions read the table it was
	 * built from, so the control tests the BYTES the writer would emit. */
	auto rec = []( const QByteArray & t, int i, int off ) {
		return quint8( t.at( i * LODL_BODY_RECORD + off ) );
	};
	auto area = []( const QByteArray & t, int i ) {
		quint32 v = 0;
		for ( int k = 0; k < 4; k++ )
			v |= quint32( quint8( t.at( i * LODL_BODY_RECORD + 0x0C + k ) ) ) << ( 8 * k );
		return v;
	};
	auto form = []( const QByteArray & t, int i ) {
		quint32 v = 0;
		for ( int k = 0; k < 4; k++ )
			v |= quint32( quint8( t.at( i * LODL_BODY_RECORD + 0x08 + k ) ) ) << ( 8 * k );
		return v;
	};
	int fails = 0;
	auto check = [&]( const QString & what, bool ok ) {
		L << QString( "  %1 %2" ).arg( ok ? QStringLiteral( "ok  " )
			: QStringLiteral( "FAIL" ) ).arg( what );
		if ( !ok )
			fails++;
	};
	/* THE EXPECTED ANSWER IS RULE D'S, NOT RULE A'S. The spec's gate G6 asked
	 * for "4 bodies, the river with 16 surfaces", which is what lane WATER1's
	 * read-only census measured under a rule that keyed bodies on water TYPE
	 * alone. Rule D keys on (height, type) and a body carries ONE plane by
	 * construction -- 803 of the Commonwealth's 804 do -- so a river that steps
	 * sixteen times IS sixteen bodies, and "a body with 16 surfaces" is not a
	 * thing rule D can produce. The geometry below is unchanged from
	 * scratchpad/water_20260909/control_synth.py; only the arithmetic that
	 * turns it into an expected answer is rule D's:
	 *
	 *   1 sea + 16 river steps + 1 lake + 1 puddle = 19 bodies.
	 *
	 * CLASS is asserted only for the sea, because that is the one this control
	 * exists to pin; the others depend on which body is nearest and lower, and
	 * a number that has to be measured before it can be written down is not a
	 * known answer. They are printed. */
	L << QStringLiteral( "EXPECTED  19 bodies: 1 sea, 16 river steps, 1 lake, 1 puddle" );
	L << QString( "MEASURED  %1 bodies" ).arg( real.bodyCount );
	static const char * const cname[3] = { "sea", "river", "lake" };
	QHash<quint32, int> countOf, areaOf, seaCls;
	for ( int i = 0; i < real.bodyCount; i++ ) {
		const quint32 fm = form( real.bodyTable, i );
		countOf[fm]++;
		areaOf[fm] += int( area( real.bodyTable, i ) );
		if ( i < 6 || real.bodyCount <= 20 )
			L << QString( "  body %1  form %2  class %3  area %4" )
				.arg( i + 1 ).arg( fm, 8, 16, QChar( '0' ) )
				.arg( QLatin1String( rec( real.bodyTable, i, 2 ) < 3
					? cname[rec( real.bodyTable, i, 2 )] : "?" ) )
				.arg( area( real.bodyTable, i ) );
		if ( fm == 0x11111111u )
			seaCls.insert( fm, rec( real.bodyTable, i, 2 ) );
	}
	check( QStringLiteral( "nineteen bodies" ), real.bodyCount == 19 );
	check( QStringLiteral( "the sea is ONE body, and it keeps the WORLDSPACE's own "
		"form -- the defect this control exists for gave it the river's" ),
		countOf.value( 0x11111111u, 0 ) == 1 );
	check( QStringLiteral( "the sea is class sea (it reaches the worldspace edge)" ),
		seaCls.value( 0x11111111u, -1 ) == 0 );
	check( QStringLiteral( "the sea is 73,728 texels -- 96 columns of 768, and not one "
		"texel of the river" ), areaOf.value( 0x11111111u, 0 ) == 73728 );
	check( QStringLiteral( "the stepped river is SIXTEEN bodies, one a plane" ),
		countOf.value( 0x22222222u, 0 ) == 16 );
	check( QStringLiteral( "the lake is one body of 16,384 texels" ),
		countOf.value( 0x33333333u, 0 ) == 1 && areaOf.value( 0x33333333u, 0 ) == 16384 );
	check( QStringLiteral( "the puddle is one body of 16 texels" ),
		countOf.value( 0x44444444u, 0 ) == 1 && areaOf.value( 0x44444444u, 0 ) == 16 );
	L << QString( "REFUTER   type-blind: %1 bodies (the sea and the river's tidal "
			"step sit at the same height and touch, so they fuse)" )
		.arg( blind.bodyCount );
	check( QStringLiteral( "the refuter FIRES: ignoring water type changes the answer" ),
		blind.bodyCount != real.bodyCount );
	L << QString( "control %1" ).arg( fails ? QStringLiteral( "FAIL" ) : QStringLiteral( "PASS" ) );
	if ( text )
		*text = L.join( QStringLiteral( "\n" ) );
	if ( error )
		error->clear();
	return fails == 0;
}
