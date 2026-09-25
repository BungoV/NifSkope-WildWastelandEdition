/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nativeemit.h"
#include "lodbfile.h"
#include "lodifile.h"
#include "lodgen.h"
#include "esmdata.h"
#include "lodgenlayout.h"
#include "lodgenparallel.h"
#include "lodgenao.h"
#include "io/lodmfile.h"
#include <array>

#include <QDir>
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>
#include <functional>
#include <tuple>

namespace
{

QString foldPath( const QString & s )
{
	QString t = s.toLower();
	t.replace( QChar( '/' ), QChar( '\\' ) );
	return t;
}

struct Arrival
{
	NativePlacement p;
	double aoSum = 0.0, skySum = 0.0, groundSum = 0.0;
	int litVerts = 0;
	int litDim = 0;          //!< the ring the lighting came from; the finest wins
	/*! v5, lane NATIVE1c: PLACEMENT AO -- one ray cast straight up from just
	 *  above this placement's drawn top, against the assembled chunk and the
	 *  heightfield. Not the same quantity as `aoSum`: that is the mean over the
	 *  placement's OWN lit chunk-mesh vertices, which a card-drawn placement
	 *  never has (`litVerts == 0`). Same finest-ring-wins rule. */
	double paoSum = 0.0;
	int paoRays = 0;
	int paoDim = 0;
	/* WHICH CHUNK LIT IT (lane INCR1, 2026-09-17). An arrival is keyed
	 * `(refForm, scolPart)` across the whole region, so two chunks CAN
	 * both light one -- and then its sums are built from both, and the
	 * per-chunk cache cannot reproduce the addition order. Counted here
	 * so the incremental driver can refuse on it instead of writing a
	 * pair that is nearly right. */
	int litCx = 0, litCy = 0;
	bool litSeen = false;
	bool shared = false;
};

struct State
{
	bool active = false;
	const EsmWorld * world = nullptr;
	QString outDir;
	NativeModelLoader loader = nullptr;
	void * user = nullptr;
	QString meshReport;          //!< --native-mesh-report: one line per library mesh, or empty
	//! v3, the two exact ways back (`--native-no-ladder`, `--native-no-occluders`)
	bool ladder = true;
	bool occluders = true;
	/*! v4 (lane NATIVE1c). `libraryNear` builds each base's level 0 from its own
	 *  NEAR `MODL` and pushes its four `MNAM` LOD slots ONE RUNG DOWN the `rep`
	 *  array, so the library's full detail is the real mesh and Bethesda's
	 *  authored LOD mesh is a level below it rather than the top. It is bungo's
	 *  ruling of 2026-09-16 11:1x ("Also do the parked") on the finding of
	 *  docs 3.5.4: the v3 ladder was correct and barely selectable because
	 *  "full detail" in the file was already a 47.7-triangle LOD mesh with
	 *  nothing left to remove.
	 *
	 *  `--library mnam` is the EXACT way back and writes the v3 geometry
	 *  choice byte for byte (at the v4 header, which is a format change and not
	 *  a behaviour change). `ladderFoliage` and `silhouetteMin` are the other
	 *  two knobs; they travel to `LodoLibrary` unchanged. */
	bool libraryNear = false;   // bungo 2026-09-17: authored LODs only
	bool ladderFoliage = LODO_LADDER_FOLIAGE_DEFAULT;
	float silhouetteMin = LODO_SILHOUETTE_MIN_DEFAULT;
	/*! v5 (lane NATIVE1c), the placement-AO module. `--native-no-placement-ao`
	 *  is the exact way back: with it false not one byte of the `.lodi` moves
	 *  and the file stays at version 3 or 4 (CONSTITUTION 10). */
	bool placementAo = true;
	/*! v6 (2026-09-18): the vertex-AO stream. `--native-no-vertex-ao` is the
	 *  exact way back: the `.lodi` stays at version 5, byte for byte. */
	bool vertexAo = true;
	/*! v7 (2026-09-18): the group table AND the per-vertex sky stream, one
	 *  switch because they are one version word. `--lodi-v6` is the exact way
	 *  back: the `.lodi` stays at version 6, byte for byte, with a 256-byte
	 *  header block and nothing at 0x100. */
	bool lodiV7 = true;
	/*! v9 (lane HORIZON3, 2026-09-19; kept by lane HORIZONOUT when the rest of
	 *  that lane's work was dropped): mark the placements a player can scrap at
	 *  a workshop, `LODI_INST_SCRAPPABLE`. It is what excludes a workshop-owned
	 *  caster from the runtime far shadow map, which is the identity route's
	 *  own need and outlived the baked-horizon route it was written beside.
	 *  OFF by default, and off is the exact way back -- no bit, `.lodi`
	 *  version 7, byte for byte. */
	bool scrappable = false;
	/*! v7 grouping, the PROXIMITY JOIN (bungo's ruling 2026-09-19). `legacy`
	 *  is the exact way back to the shipped architecture/box rule. */
	bool identityJoinLegacy = false;
	float identityJoinGap = 64.0f;
	QHash<QPair<quint32, int>, int> byKey;        //!< (ref, part) -> arrival index
	std::vector<Arrival> arrivals;
	std::map<std::tuple<int, int, int, int>, int> byObject;   //!< (chunkX, chunkY, dim, objectIndex) -> arrival
	quint64 arrivalsSeen = 0;
	int sharedArrivals = 0;      //!< placements more than one chunk lit
	/* v4, the aggregate module (bungo 2026-09-11 08:3x). Unarmed by default;
	 * `aggArmed` false means not one byte of the .lodi moves. */
	bool aggArmed = false;
	LodgenAggOptions aggOpts;
	QHash<quint32, LodgenAggCard> aggCards;
	QVector<LodgenAggSet> aggSets;
	LodgenAggStats aggStats;
	/* THE CARD LINK (lane CARDLINK1, 2026-09-24). Filled by
	 * `lodgenNativeLinkCards` from the card arrays' `.lodm` files and the
	 * chunks' `C` lines; empty -- and `cardsLinked` false -- on every bake
	 * that did not call it, which writes what it always wrote: cardLayer
	 * 0xFFFF, cardCount 0, cardCorpusHash 0, no FORCE_CARD bit. */
	bool cardsLinked = false;
	quint64 cardHash = 0;                         //!< the proposed R19 hash, docs 3 `cardCorpusHash`
	int cardArrays = 0, cardLayers = 0;
	quint64 cardLines = 0, cardLinesLinked = 0;  //!< `C` lines read, and those that name an array layer
	QHash<quint32, quint16> cardLayerOf;          //!< base formId -> (set << 11) | layer
	QHash<quint32, float> cardRadiusOf;           //!< base formId -> the card's bound radius at scale 1
	std::set<std::tuple<int, int, int, int>> cardPlaced;   //!< (chunkX, chunkY, dim, objectIndex) standing on a card
};

State & st()
{
	static State s;
	return s;
}

/*! The v4/v5 knobs, held OUTSIDE `State` because `lodgenNativeBegin` resets
 *  `State` wholesale and both callers (the command line and the GUI manager)
 *  set their options while they are parsing, before the bake begins. Sticky:
 *  set once, every later `Begin` in this process starts from them. */
struct NativeLadderOptions
{
	bool libraryNear = false;   // bungo 2026-09-17: authored LODs only
	bool ladderFoliage = LODO_LADDER_FOLIAGE_DEFAULT;
	float silhouetteMin = LODO_SILHOUETTE_MIN_DEFAULT;
	bool placementAo = true;
	bool vertexAo = true;
	bool lodiV7 = true;
	bool scrappable = false;            //!< v9: mark workshop-scrappable placements; off = .lodi v7
	bool identityJoinLegacy = false;    //!< v7 grouping: the pre-2026-09-19 architecture/box rule
	float identityJoinGap = 64.0f;      //!< v7 grouping: the MESH-to-MESH gap two placements join at
};

NativeLadderOptions & ladderOpts()
{
	static NativeLadderOptions o;
	return o;
}

//! FNV-1a over a POD value, little-endian by construction on this target.
template <typename T> quint64 fnv( quint64 h, const T & v )
{
	return lodoFnv1a64( &v, sizeof( T ), h );
}
quint64 fnvStr( quint64 h, const QString & s )
{
	const QByteArray b = s.toUtf8();
	return lodoFnv1a64( b.constData(), size_t( b.size() ), h );
}

//! The material key the chunk builder buckets by (lodgen.cpp, `ObjBucket & bucket = buckets[...]`).
QString materialKey( const NativeSrcShape & s )
{
	return s.tex0.toLower() + QChar( '|' ) + s.tex1.toLower() + QChar( '|' ) + s.tex7.toLower() + QChar( '|' )
		+ s.matName.toLower() + QString( "|%1|%2" ).arg( double( s.smoothness ) ).arg( double( s.specMult ) )
		+ QString( "|%1|%2|%3|%4|%5" ).arg( double( s.emitColor[0] ) ).arg( double( s.emitColor[1] ) )
			.arg( double( s.emitColor[2] ) ).arg( double( s.emitMult ) ).arg( s.ownEmit ? 1 : 0 )
		+ ( s.hasAlpha ? QStringLiteral( "|at" ) : QString() );
}

/* ---- v3, the precomputed occluders (bungo 2026-09-11 08:2x, "2 sounds good":
 *      "a few boxes per cell for buildings and hills, baked from the meshes") ----
 *
 * The rule that shapes every constant below: A BOX THAT STICKS OUT OF ITS
 * OBJECT HIDES THINGS WRONGLY, WHICH IS WORSE THAN NO OCCLUDER. So the fit is
 * deliberately timid: only a WATERTIGHT mesh (ray parity means nothing on an
 * open one), only an object big enough to be worth rejecting behind, the
 * largest all-interior voxel box, then a whole voxel shaved off every side,
 * then a hundred points inside the result tested against the mesh itself. Any
 * of those refuses and the object simply contributes no occluder, which the
 * census counts by reason. */
constexpr int NATIVE_OCC_GRID = 16;             //!< voxels an axis over the mesh AABB
constexpr float NATIVE_OCC_MIN_DIAG = 256.0f;   //!< model units at scale 1: below this, not worth a row
constexpr float NATIVE_OCC_MIN_FILL = 0.02f;    //!< the box must be this fraction of the AABB's volume
constexpr int NATIVE_OCC_PROBES = 100;          //!< the writer's own inside-the-mesh check

//! Why a mesh yielded no occluder box. Every one of these is counted and printed.
enum NativeOccRefusal
{
	NATIVE_OCC_OK = 0,
	NATIVE_OCC_NOT_WATERTIGHT,
	NATIVE_OCC_TOO_SMALL,
	NATIVE_OCC_NO_INTERIOR,
	NATIVE_OCC_TOO_THIN,
	NATIVE_OCC_PROBE_FAILED
};

/*! Crossings of the ray (origin, +X) with one triangle, Moeller-Trumbore
 *  specialised to the X axis. Returns false when the ray misses or is parallel;
 *  `tOut` is the crossing's X coordinate. */
bool rayXTri( const float o[3], const float a[3], const float b[3], const float c[3], float * tOut )
{
	float e1[3], e2[3];
	for ( int k = 0; k < 3; k++ ) {
		e1[k] = b[k] - a[k];
		e2[k] = c[k] - a[k];
	}
	// d = (1,0,0); p = d x e2 = (0*e2[2]-0*e2[1], 0*e2[0]-1*e2[2], 1*e2[1]-0*e2[0])
	const float p[3] = { 0.0f, -e2[2], e2[1] };
	const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
	if ( std::fabs( det ) < 1.0e-12f )
		return false;
	const float inv = 1.0f / det;
	float tv[3];
	for ( int k = 0; k < 3; k++ )
		tv[k] = o[k] - a[k];
	const float u = ( tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2] ) * inv;
	if ( u < 0.0f || u > 1.0f )
		return false;
	const float q[3] = { tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0] };
	const float v = ( q[0] ) * inv;      // dot(d, q) with d = (1,0,0)
	if ( v < 0.0f || u + v > 1.0f )
		return false;
	const float t = ( e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2] ) * inv;
	if ( t <= 0.0f )
		return false;
	*tOut = o[0] + t;
	return true;
}

//! Is the point inside the (watertight) triangle soup? Ray parity along +X.
bool pointInSoup( const float pt[3], const std::vector<float> & pos, const std::vector<quint32> & tris )
{
	int crossings = 0;
	for ( size_t t = 0; t + 2 < tris.size(); t += 3 ) {
		float x = 0.0f;
		if ( rayXTri( pt, &pos[size_t( tris[t] ) * 3], &pos[size_t( tris[t + 1] ) * 3],
			&pos[size_t( tris[t + 2] ) * 3], &x ) )
			crossings++;
	}
	return ( crossings & 1 ) != 0;
}

/*! Fit the largest axis-aligned box that lies INSIDE the soup, in the soup's own
 *  space. Returns the refusal reason; on NATIVE_OCC_OK, `centre` and `half`
 *  carry the box. */
NativeOccRefusal fitOccluderBox( const std::vector<float> & pos, const std::vector<quint32> & tris,
	bool watertight, float centre[3], float half[3] )
{
	if ( !watertight )
		return NATIVE_OCC_NOT_WATERTIGHT;
	if ( pos.size() < 9 || tris.size() < 9 )
		return NATIVE_OCC_TOO_SMALL;
	float mn[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, mx[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
	for ( size_t v = 0; v + 2 < pos.size(); v += 3 )
		for ( int k = 0; k < 3; k++ ) {
			mn[k] = std::min( mn[k], pos[v + k] );
			mx[k] = std::max( mx[k], pos[v + k] );
		}
	float ext[3], d[3];
	double aabbVol = 1.0;
	for ( int k = 0; k < 3; k++ ) {
		ext[k] = mx[k] - mn[k];
		d[k] = ext[k] / float( NATIVE_OCC_GRID );
		aabbVol *= double( ext[k] );
		if ( !( ext[k] > 0.0f ) )
			return NATIVE_OCC_TOO_SMALL;
	}
	const float diag = std::sqrt( ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2] );
	if ( diag < NATIVE_OCC_MIN_DIAG )
		return NATIVE_OCC_TOO_SMALL;

	/* The interior grid: ONE ray a (y, z) row rather than one a voxel, so the
	 * cost is 256 rays a mesh and not 4,096. */
	const int G = NATIVE_OCC_GRID;
	std::vector<unsigned char> inside( size_t( G ) * G * G, 0 );
	std::vector<float> xs;
	for ( int iz = 0; iz < G; iz++ ) {
		for ( int iy = 0; iy < G; iy++ ) {
			const float o[3] = { mn[0] - std::max( 1.0f, ext[0] ),
				mn[1] + ( float( iy ) + 0.5f ) * d[1], mn[2] + ( float( iz ) + 0.5f ) * d[2] };
			xs.clear();
			for ( size_t t = 0; t + 2 < tris.size(); t += 3 ) {
				float x = 0.0f;
				if ( rayXTri( o, &pos[size_t( tris[t] ) * 3], &pos[size_t( tris[t + 1] ) * 3],
					&pos[size_t( tris[t + 2] ) * 3], &x ) )
					xs.push_back( x );
			}
			if ( xs.empty() )
				continue;
			std::sort( xs.begin(), xs.end() );
			for ( int ix = 0; ix < G; ix++ ) {
				const float cx = mn[0] + ( float( ix ) + 0.5f ) * d[0];
				const size_t before = size_t( std::lower_bound( xs.begin(), xs.end(), cx ) - xs.begin() );
				if ( before & 1 )
					inside[( size_t( iz ) * G + iy ) * G + ix] = 1;
			}
		}
	}
	// 3D prefix sums, so "is this whole sub-box interior" is four lookups a plane
	std::vector<int> pre( size_t( G + 1 ) * ( G + 1 ) * ( G + 1 ), 0 );
	auto P = [&]( int x, int y, int z ) -> int & { return pre[( size_t( z ) * ( G + 1 ) + y ) * ( G + 1 ) + x]; };
	for ( int z = 1; z <= G; z++ )
		for ( int y = 1; y <= G; y++ )
			for ( int x = 1; x <= G; x++ )
				P( x, y, z ) = inside[( size_t( z - 1 ) * G + ( y - 1 ) ) * G + ( x - 1 )]
					+ P( x - 1, y, z ) + P( x, y - 1, z ) + P( x, y, z - 1 )
					- P( x - 1, y - 1, z ) - P( x - 1, y, z - 1 ) - P( x, y - 1, z - 1 )
					+ P( x - 1, y - 1, z - 1 );
	auto boxSum = [&]( int x0, int x1, int y0, int y1, int z0, int z1 ) {
		return P( x1 + 1, y1 + 1, z1 + 1 ) - P( x0, y1 + 1, z1 + 1 ) - P( x1 + 1, y0, z1 + 1 ) - P( x1 + 1, y1 + 1, z0 )
			+ P( x0, y0, z1 + 1 ) + P( x0, y1 + 1, z0 ) + P( x1 + 1, y0, z0 ) - P( x0, y0, z0 );
	};
	int bx0 = -1, bx1 = -1, by0 = -1, by1 = -1, bz0 = -1, bz1 = -1;
	double bestVol = 0.0;
	for ( int x0 = 0; x0 < G; x0++ )
		for ( int x1 = x0; x1 < G; x1++ )
			for ( int y0 = 0; y0 < G; y0++ )
				for ( int y1 = y0; y1 < G; y1++ ) {
					const int area = ( x1 - x0 + 1 ) * ( y1 - y0 + 1 );
					int run = 0;
					for ( int z = 0; z < G; z++ ) {
						if ( boxSum( x0, x1, y0, y1, z, z ) == area ) {
							run++;
							const double vol = double( x1 - x0 + 1 ) * d[0] * double( y1 - y0 + 1 ) * d[1] * double( run ) * d[2];
							if ( vol > bestVol ) {
								bestVol = vol;
								bx0 = x0; bx1 = x1; by0 = y0; by1 = y1; bz0 = z - run + 1; bz1 = z;
							}
						} else {
							run = 0;
						}
					}
				}
	if ( bx0 < 0 )
		return NATIVE_OCC_NO_INTERIOR;

	/* Shave a WHOLE voxel off every side. The grid only says a voxel's CENTRE
	 * is interior, so the surface may be half a voxel away; a whole voxel is
	 * that plus the same again. */
	float lo[3], hi[3];
	const int i0[3] = { bx0, by0, bz0 }, i1[3] = { bx1, by1, bz1 };
	for ( int k = 0; k < 3; k++ ) {
		lo[k] = mn[k] + float( i0[k] ) * d[k] + d[k];
		hi[k] = mn[k] + float( i1[k] + 1 ) * d[k] - d[k];
		if ( !( hi[k] > lo[k] ) )
			return NATIVE_OCC_TOO_THIN;
		centre[k] = 0.5f * ( lo[k] + hi[k] );
		half[k] = 0.5f * ( hi[k] - lo[k] );
	}
	if ( 8.0 * double( half[0] ) * double( half[1] ) * double( half[2] ) < NATIVE_OCC_MIN_FILL * aabbVol )
		return NATIVE_OCC_TOO_THIN;

	/* THE WRITER'S OWN GATE, and it is the same test the harness runs: a hundred
	 * points inside the box must be inside the mesh. The sequence is a fixed
	 * additive-recurrence one, so it is the same hundred points every bake and
	 * two writes stay byte-identical. */
	for ( int i = 0; i < NATIVE_OCC_PROBES; i++ ) {
		const double a1 = 0.7548776662466927, a2 = 0.5698402909980532, a3 = 0.4331362575972949;
		const double fx = std::fmod( a1 * double( i + 1 ), 1.0 );
		const double fy = std::fmod( a2 * double( i + 1 ), 1.0 );
		const double fz = std::fmod( a3 * double( i + 1 ), 1.0 );
		const float pt[3] = { lo[0] + float( fx ) * ( hi[0] - lo[0] ),
			lo[1] + float( fy ) * ( hi[1] - lo[1] ), lo[2] + float( fz ) * ( hi[2] - lo[2] ) };
		if ( !pointInSoup( pt, pos, tris ) )
			return NATIVE_OCC_PROBE_FAILED;
	}
	return NATIVE_OCC_OK;
}

bool shapeEmits( const NativeSrcShape & s )
{
	return s.ownEmit && ( s.emitColor[0] > 0.0f || s.emitColor[1] > 0.0f || s.emitColor[2] > 0.0f );
}

/*! THE OBJECT CENSUS AND ITS HASH, in one place because `--native-verify`
 *  re-runs it against the plugin. The hash law, stated exactly (docs 5, 8):
 *  FNV-1a 64 over, in ascending cell order and then the persistent cell,
 *  (REFR formId, base formId, DATA position, DATA rotation, XSCL, a flag byte
 *  of initially-disabled | deleted) for EVERY reference the walk reads; then,
 *  over the LOD-bearing bases in ascending formId order, (base formId, the
 *  four MNAM slot paths case-folded); then, over the SCOL bases in ascending
 *  formId order, (SCOL formId, each part's base formId, each placement's
 *  position, rotation and scale). Nothing else -- the LOAD ORDER is a separate
 *  u64 beside it (EsmWorld::loadOrderHash). */
bool nativeObjectCensus( const EsmWorld & world, std::vector<quint32> * baseIdsOut,
	quint64 * objHashOut, quint64 * censusRefsOut, QString * error )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = QStringLiteral( "native: " ) + m;
		return false;
	};
	int minX, minY, maxX, maxY;
	world.cellBounds( minX, minY, maxX, maxY );
	QSet<quint32> baseSet;
	QSet<quint32> scolSeen;
	quint64 objHash = Q_UINT64_C( 0xCBF29CE484222325 );
	quint64 censusRefs = 0;
	auto takeRef = [&]( const EsmRefr & r ) {
		objHash = fnv( objHash, r.formID );
		objHash = fnv( objHash, r.base );
		objHash = lodoFnv1a64( r.pos, sizeof( r.pos ), objHash );
		objHash = lodoFnv1a64( r.rot, sizeof( r.rot ), objHash );
		objHash = fnv( objHash, r.scale );
		const quint8 fl = quint8( ( r.initiallyDisabled ? 1 : 0 ) | ( r.deleted ? 2 : 0 ) );
		objHash = fnv( objHash, fl );
		if ( r.initiallyDisabled || r.deleted || !r.base )
			return;
		censusRefs++;
		if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
			for ( const EsmScolPart & part : world.scolParts( r.base ) )
				if ( world.lodBase( part.base ).hasLod )
					baseSet.insert( part.base );
			scolSeen.insert( r.base );
		} else if ( world.lodBase( r.base ).hasLod ) {
			baseSet.insert( r.base );
		}
	};
	for ( int cy = minY; cy <= maxY; cy++ )
		for ( int cx = minX; cx <= maxX; cx++ )
			if ( world.hasCell( cx, cy ) )
				for ( const EsmRefr & r : world.refrs( cx, cy ) )
					takeRef( r );
	for ( const EsmRefr & r : world.persistentRefrsIn( float( minX ) * 4096.0f, float( minY ) * 4096.0f,
		float( maxX + 1 ) * 4096.0f, float( maxY + 1 ) * 4096.0f ) )
		takeRef( r );
	std::vector<quint32> baseIds( baseSet.begin(), baseSet.end() );
	std::sort( baseIds.begin(), baseIds.end() );
	if ( baseIds.size() > size_t( LODI_BASE_MAX ) + 1 )
		return fail( QString( "the worldspace census names %1 LOD-bearing bases; the u16 baseId holds 65,536 (the last is 0x%2)" )
			.arg( baseIds.size() ).arg( baseIds.back(), 8, 16, QChar( '0' ) ) );
	for ( quint32 b : baseIds ) {
		const EsmLodBase & lb = world.lodBase( b );
		objHash = fnv( objHash, b );
		for ( int k = 0; k < 4; k++ )
			objHash = fnvStr( objHash, foldPath( lb.models[k] ) );
	}
	std::vector<quint32> scols( scolSeen.begin(), scolSeen.end() );
	std::sort( scols.begin(), scols.end() );
	for ( quint32 sc : scols ) {
		objHash = fnv( objHash, sc );
		for ( const EsmScolPart & part : world.scolParts( sc ) ) {
			objHash = fnv( objHash, part.base );
			for ( const EsmScolPlacement & pl : part.placements ) {
				objHash = lodoFnv1a64( pl.pos, sizeof( pl.pos ), objHash );
				objHash = lodoFnv1a64( pl.rot, sizeof( pl.rot ), objHash );
				objHash = fnv( objHash, pl.scale );
			}
		}
	}
	if ( baseIdsOut )
		*baseIdsOut = baseIds;
	if ( objHashOut )
		*objHashOut = objHash;
	if ( censusRefsOut )
		*censusRefsOut = censusRefs;
	return true;
}

} // namespace

/* ---------------------------------------------------------------- arming */

void lodgenNativeBegin( const EsmWorld * world, const QString & outDir, NativeModelLoader loader, void * user,
	const QString & meshReportPath, bool buildLadder, bool buildOccluders )
{
	State & s = st();
	s = State();
	s.active = world != nullptr && loader != nullptr;
	s.world = world;
	s.outDir = outDir;
	s.loader = loader;
	s.user = user;
	s.meshReport = meshReportPath;
	s.ladder = buildLadder;
	s.occluders = buildOccluders;
	const NativeLadderOptions & o = ladderOpts();
	s.libraryNear = o.libraryNear;
	s.ladderFoliage = o.ladderFoliage;
	s.silhouetteMin = o.silhouetteMin;
	s.placementAo = o.placementAo;
	s.vertexAo = o.vertexAo;
	s.lodiV7 = o.lodiV7;
	s.scrappable = o.scrappable;
	s.identityJoinLegacy = o.identityJoinLegacy;
	s.identityJoinGap = o.identityJoinGap;
}

void lodgenNativeLadderOptions( bool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo )
{
	NativeLadderOptions & o = ladderOpts();
	o.libraryNear = libraryNear;
	o.ladderFoliage = ladderFoliage;
	o.silhouetteMin = silhouetteMin;
	o.placementAo = placementAo;
}

void lodgenNativeVertexAoOption( bool on )
{
	ladderOpts().vertexAo = on;
}

void lodgenNativeLodiV7Option( bool on )
{
	ladderOpts().lodiV7 = on;
}

/*! v9 (lane HORIZON3, kept by lane HORIZONOUT): the workshop-scrappable bit.
 *  OFF by default, and off is the exact way back -- no bit is written and the
 *  `.lodi` stays at version 7, byte for byte.
 *
 *  It ships OFF because it has not been flown. bungo's standing rule of
 *  2026-09-17 is that an owed ruling never ships as a default. */
void lodgenNativeScrappableOption( bool scrappable )
{
	ladderOpts().scrappable = scrappable;
}

void lodgenNativeIdentityJoinOption( bool legacy, float gapWorld )
{
	ladderOpts().identityJoinLegacy = legacy;
	if ( gapWorld > 0.0f )
		ladderOpts().identityJoinGap = gapWorld;
}

bool lodgenNativeActive()
{
	return st().active;
}

/* THE LIBRARY-REUSE OFFER (lane PERF1, step 5). The `--incremental` driver
 * offers; `lodgenNativeWrite` decides and CONSUMES, so one arming buys one
 * write and a second write in the same process rebuilds rather than inheriting
 * a stale yes. A function-local static, like `st()` and `ladderOpts()` above
 * it, because the decision reads it from a translation unit whose statics are
 * all spelled that way. No file, no format field, no default moved: a full bake
 * never arms it and behaves exactly as it did. */
static NativeReuseOffer & reuseOffer()
{
	static NativeReuseOffer o;
	return o;
}

void lodgenNativeOfferLibraryReuse( const NativeReuseOffer & offer )
{
	reuseOffer() = offer;
}

/* THE CARD LINK (lane CARDLINK1, 2026-09-24). The contract, in full, is docs
 * LODGEN_NATIVE_LODO_LODI.md 4.13; the code below is its only implementation.
 *
 *   1. The chunks' manifests are read: the header gives (dim, chunk X, chunk
 *      Y), the rows give index -> base, and a `C` line with TWELVE tokens is
 *      one the card-arrays pass linked (`... <array .lodm> <layer>`); ten
 *      tokens is a card that is in no array.
 *   2. The arrays are the DISTINCT array `.lodm` files those lines name -- not
 *      a directory listing, so a stale array left in the folder by an older
 *      bake is never linked. A set's index is its rank in ascending order of
 *      the lower-cased file name's UTF-8 bytes; at most 32 sets (the 5 high
 *      bits of `cardLayer`), at most 2048 layers a set (the low 11).
 *   3. cardCorpusHash = FNV-1a 64 from the offset basis over, for each set in
 *      that order, its five files in the order .lodm, colour, normal, mask,
 *      emissive: the lower-cased file name's UTF-8 bytes, the file size as a
 *      little-endian u64, then every byte of the file.
 *   4. Every layer's `id` is the base's formID in 8 hex digits; a base in two
 *      layers is refused, and so is a `C` line whose (array, layer) is not
 *      the layer its base's id names. */
bool lodgenNativeLinkCards( const QStringList & btoPaths, const QString & cardArrayBase, QString * error )
{
	State & s = st();
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = QStringLiteral( "native card link: " ) + m;
		return false;
	};
	if ( !s.active )
		return fail( QStringLiteral( "the emitter is not armed" ) );
	s.cardsLinked = false;
	s.cardHash = 0;
	s.cardArrays = s.cardLayers = 0;
	s.cardLines = s.cardLinesLinked = 0;
	s.cardLayerOf.clear();
	s.cardRadiusOf.clear();
	s.cardPlaced.clear();
	auto lastComponent = []( QString p ) {
		p.replace( QChar( 92 ), QChar( '/' ) );
		return p.mid( p.lastIndexOf( QChar( '/' ) ) + 1 );
	};
	const QFileInfo baseInfo( cardArrayBase );
	const QDir dir = baseInfo.absoluteDir();
	const QString stemLower = baseInfo.fileName().toLower() + QChar( '.' );

	// 1. the manifests
	struct CLine { int cx, cy, dim, idx; quint32 base; QString arrayLower; int layer; QString where; };
	std::vector<CLine> lines;
	QMap<QByteArray, QString> arrayNames;    // lower-case UTF-8 name -> the name as the line spells it
	for ( const QString & bto : btoPaths ) {
		const QString mpath = bto + QStringLiteral( ".manifest.txt" );
		QFile mf( mpath );
		if ( !mf.open( QIODevice::ReadOnly | QIODevice::Text ) )
			return fail( QString( "%1: no manifest beside the chunk" ).arg( mpath ) );
		const QString mname = QFileInfo( mpath ).fileName();
		int dim = -1, cx = 0, cy = 0;
		bool header = false;
		QHash<int, quint32> baseOfRow;
		std::vector<CLine> mine;
		while ( !mf.atEnd() ) {
			const QString line = QString::fromUtf8( mf.readLine() ).trimmed();
			if ( line.isEmpty() )
				continue;
			const QStringList t = line.split( QChar( ' ' ), Qt::SkipEmptyParts );
			if ( line.startsWith( QLatin1String( "# lodgen manifest" ) ) ) {
				for ( int i = 0; i + 1 < t.size(); i++ ) {
					if ( t[i] == QLatin1String( "dim" ) )
						dim = t[i + 1].toInt();
					if ( t[i] == QLatin1String( "chunk" ) && i + 2 < t.size() ) {
						cx = t[i + 1].toInt();
						cy = t[i + 2].toInt();
						header = true;
					}
				}
				continue;
			}
			if ( line.startsWith( QChar( '#' ) ) )
				continue;
			if ( t[0] == QLatin1String( "C" ) ) {
				s.cardLines++;
				if ( t.size() == 10 )
					continue;         // a card in no array: nothing to link
				if ( t.size() != 12 )
					return fail( QString( "%1: a C line with %2 tokens (10 or 12 expected): %3" )
						.arg( mname ).arg( t.size() ).arg( line ) );
				CLine c;
				c.idx = t[1].toInt();
				c.base = 0;
				const QString an = lastComponent( t[10] );
				c.arrayLower = an.toLower();
				arrayNames.insert( c.arrayLower.toUtf8(), an );
				bool ok = false;
				c.layer = t[11].toInt( &ok );
				if ( !ok || c.layer < 0 )
					return fail( QString( "%1: C line for row %2 names layer '%3'" ).arg( mname ).arg( t[1] ).arg( t[11] ) );
				c.where = mname;
				mine.push_back( c );
				continue;
			}
			bool ok = false;
			const int idx = t[0].toInt( &ok );
			if ( ok && t.size() >= 2 )
				baseOfRow.insert( idx, t[1].toUInt( nullptr, 16 ) );
		}
		if ( !header || dim < 0 )
			return fail( QString( "%1: no '# lodgen manifest' header naming dim and chunk" ).arg( mname ) );
		for ( CLine & c : mine ) {
			auto b = baseOfRow.constFind( c.idx );
			if ( b == baseOfRow.constEnd() )
				return fail( QString( "%1: C line for row %2, which the manifest has no row for" ).arg( mname ).arg( c.idx ) );
			c.base = b.value();
			c.cx = cx; c.cy = cy; c.dim = dim;
			lines.push_back( c );
		}
	}
	if ( arrayNames.isEmpty() )
		return true;          // no C line names an array: nothing linked, the bake writes no card

	// 2 + 3. the arrays, in the documented order, hashed
	if ( arrayNames.size() > 32 )
		return fail( QString( "%1 card arrays; cardLayer's 5 set bits hold 32" ).arg( arrayNames.size() ) );
	quint64 h = Q_UINT64_C( 0xCBF29CE484222325 );
	auto hashFile = [&]( const QString & name, QString * why ) -> bool {
		QFile f( dir.filePath( name ) );
		if ( !f.open( QIODevice::ReadOnly ) ) {
			*why = QString( "%1 is not on disk beside the array base %2" ).arg( name ).arg( dir.absolutePath() );
			return false;
		}
		const QByteArray bytes = f.readAll();
		const QByteArray lower = name.toLower().toUtf8();
		h = lodoFnv1a64( lower.constData(), size_t( lower.size() ), h );
		const quint64 n = quint64( bytes.size() );
		unsigned char le[8];
		for ( int i = 0; i < 8; i++ )
			le[i] = static_cast<unsigned char>( ( n >> ( 8 * i ) ) & 0xFF );
		h = lodoFnv1a64( le, 8, h );
		h = lodoFnv1a64( bytes.constData(), size_t( bytes.size() ), h );
		return true;
	};
	QHash<QString, int> setOf;     // lower-case array name -> set index
	int set = 0;
	for ( auto it = arrayNames.constBegin(); it != arrayNames.constEnd(); ++it, ++set ) {
		const QString name = it.value();
		if ( !name.toLower().startsWith( stemLower ) )
			return fail( QString( "a C line names %1, which is not an array of %2" ).arg( name ).arg( cardArrayBase ) );
		QFile lf( dir.filePath( name ) );
		if ( !lf.open( QIODevice::ReadOnly ) )
			return fail( QString( "%1 is not on disk beside the array base %2" ).arg( name ).arg( dir.absolutePath() ) );
		const LodmMaterial m = lodmParse( lf.readAll() );
		lf.close();
		if ( !m.ok || m.kind != QLatin1String( "cardArray" ) )
			return fail( QString( "%1 does not parse as a cardArray .lodm (kind '%2')" ).arg( name ).arg( m.kind ) );
		QString why;
		if ( !hashFile( name, &why ) )
			return fail( why );
		const QString sheets[4] = { m.color, m.normal, m.mask, m.emissive };
		static const char * const sheetRole[4] = { "colour", "normal", "mask", "emissive" };
		for ( int k = 0; k < 4; k++ ) {
			if ( sheets[k].isEmpty() )
				return fail( QString( "%1 names no %2 sheet" ).arg( name ).arg( QLatin1String( sheetRole[k] ) ) );
			if ( !hashFile( lastComponent( sheets[k] ), &why ) )
				return fail( why );
		}
		const QJsonArray layers = m.root.value( QStringLiteral( "array" ) ).toObject()
			.value( QStringLiteral( "layers" ) ).toArray();
		if ( layers.isEmpty() )
			return fail( QString( "%1 has no layers" ).arg( name ) );
		if ( layers.size() > int( LODO_LAYER_CAP ) )
			return fail( QString( "%1 has %2 layers; cardLayer's 11 layer bits hold %3" )
				.arg( name ).arg( layers.size() ).arg( LODO_LAYER_CAP ) );
		for ( int li = 0; li < layers.size(); li++ ) {
			const QJsonObject o = layers[li].toObject();
			const QString id = o.value( QStringLiteral( "id" ) ).toString();
			bool ok = false;
			const quint32 form = id.toUInt( &ok, 16 );
			if ( id.size() != 8 || !ok )
				return fail( QString( "%1 layer %2: id '%3' is not a formID in 8 hex digits" ).arg( name ).arg( li ).arg( id ) );
			const quint16 packed = quint16( ( set << 11 ) | li );
			if ( packed == LODO_NO_CARD )
				return fail( QString( "%1 layer %2 packs to 0xFFFF, the no-card value" ).arg( name ).arg( li ) );
			if ( s.cardLayerOf.contains( form ) )
				return fail( QString( "base 0x%1 has a layer in two places (0x%2 and 0x%3)" )
					.arg( form, 8, 16, QChar( '0' ) ).arg( s.cardLayerOf.value( form ), 4, 16, QChar( '0' ) )
					.arg( packed, 4, 16, QChar( '0' ) ) );
			s.cardLayerOf.insert( form, packed );
			const QJsonArray half = o.value( QStringLiteral( "half" ) ).toArray();
			const QJsonArray ctr = o.value( QStringLiteral( "center" ) ).toArray();
			const double hw = half.size() > 0 ? half[0].toDouble() : 0.0, hh = half.size() > 1 ? half[1].toDouble() : 0.0;
			double c2 = 0.0;
			for ( int k = 0; k < 3 && k < ctr.size(); k++ )
				c2 += ctr[k].toDouble() * ctr[k].toDouble();
			s.cardRadiusOf.insert( form, float( std::sqrt( c2 ) + std::sqrt( 2.0 * hw * hw + hh * hh ) ) );
		}
		s.cardLayers += int( layers.size() );
		setOf.insert( QString::fromUtf8( it.key() ), set );
	}

	// 4. every linked C line agrees with the layer its base's id names
	for ( const CLine & c : lines ) {
		const int cs = setOf.value( c.arrayLower, -1 );
		const quint16 want = quint16( ( cs << 11 ) | c.layer );
		auto got = s.cardLayerOf.constFind( c.base );
		if ( cs < 0 || got == s.cardLayerOf.constEnd() || got.value() != want )
			return fail( QString( "%1: row %2 (base 0x%3) stands on layer %4 of %5, but that array's layer "
				"%4 is not this base's card" ).arg( c.where ).arg( c.idx ).arg( c.base, 8, 16, QChar( '0' ) )
				.arg( c.layer ).arg( c.arrayLower ) );
		s.cardPlaced.insert( std::make_tuple( c.cx, c.cy, c.dim, c.idx ) );
		s.cardLinesLinked++;
	}
	s.cardArrays = int( arrayNames.size() );
	s.cardHash = h;
	s.cardsLinked = true;
	return true;
}

void lodgenNativeEnd()
{
	st() = State();
}

void lodgenNativeSetAggregate( const LodgenAggOptions & opts,
	const QHash<quint32, LodgenAggCard> & cards )
{
	State & s = st();
	s.aggArmed = true;
	s.aggOpts = opts;
	s.aggCards = cards;
	s.aggSets.clear();
	s.aggStats = LodgenAggStats();
}

const QVector<LodgenAggSet> & lodgenNativeAggregateSets()
{
	return st().aggSets;
}

const LodgenAggStats & lodgenNativeAggregateStats()
{
	return st().aggStats;
}

/* ------------------------------------------------------------- journals */

/*! One recorded call. `lighting` says which of the two it was; the placement
 *  and the six lighting arguments ride together because a journal holds both
 *  kinds IN ONE SEQUENCE -- the order between them is exactly what has to
 *  survive the fan-out. */
enum LodgenNativeEventKind { LNE_PLACEMENT = 0, LNE_LIGHTING = 1, LNE_PLACEMENT_AO = 2 };

struct LodgenNativeJournalEvent
{
	int kind = LNE_PLACEMENT;
	NativePlacement p;
	int chunkX = 0, chunkY = 0, dim = 0, objectIndex = 0;
	float ao = 0.0f, sky = 0.0f, ground = 0.0f;
};

struct LodgenNativeJournal
{
	std::vector<LodgenNativeJournalEvent> events;
};

namespace
{
thread_local LodgenNativeJournal * tlsJournal = nullptr;
}

LodgenNativeJournal * lodgenNativeJournalBegin()
{
	if ( !st().active )
		return nullptr;
	tlsJournal = new LodgenNativeJournal;
	return tlsJournal;
}

void lodgenNativeJournalEnd()
{
	tlsJournal = nullptr;
}

int lodgenNativeJournalSize( const LodgenNativeJournal * j )
{
	return j ? int( j->events.size() ) : 0;
}

void lodgenNativeJournalDestroy( LodgenNativeJournal * j )
{
	delete j;
}

void lodgenNativeAddPlacement( const NativePlacement & p );
void lodgenNativeLighting( int chunkX, int chunkY, int dim, int objectIndex, float ao, float sky, float ground );
void lodgenNativePlacementAo( int chunkX, int chunkY, int dim, int objectIndex, float ao );

/* ---- THE PER-CHUNK NATIVE CACHE (`.lodj`, lane INCR1, 2026-09-17) -------
 *
 * The format, and why every float is a hex bit pattern: the whole point of
 * the file is that the arrivals it rebuilds are BIT-IDENTICAL to the ones the
 * chunk made, and a decimal round trip is not. It is otherwise the tree's own
 * plain-text shape -- UTF-8, LF, `key<TAB>fields` -- so `lodj_read.py` is a
 * dozen lines and the gates never need a binary reader.
 *
 *   lodj<TAB>1<TAB><ws><TAB><dim><TAB><cx><TAB><cy>
 *   placements<TAB><n>
 *   p<TAB>base<TAB>ref<TAB>scolPart<TAB>pos*3<TAB>rot*9<TAB>scale<TAB>slot
 *     <TAB>isTree<TAB>mirrorU<TAB>treeHash<TAB>hasAlpha<TAB>emits
 *     <TAB>objectIndex<TAB>model
 *   lit<TAB><n>
 *   l<TAB>objectIndex<TAB>aoSum<TAB>skySum<TAB>groundSum<TAB>verts
 *   pao<TAB><n>
 *   a<TAB>objectIndex<TAB>paoSum<TAB>rays
 *   end<TAB><placements><TAB><lit><TAB><pao>
 *
 * The rows keep FIRST-APPEARANCE order and each row's sums are accumulated in
 * the order the events arrived, which is the order the accumulator itself
 * would have added them in. */

static int g_cacheLastPlacements = 0;

static QString f32hex( float v )
{
	quint32 b = 0;
	std::memcpy( &b, &v, 4 );
	return QString::number( b, 16 ).rightJustified( 8, QChar( '0' ) );
}

static float hexf32( const QString & s )
{
	const quint32 b = s.toUInt( nullptr, 16 );
	float v = 0.0f;
	std::memcpy( &v, &b, 4 );
	return v;
}

static QString f64hex( double v )
{
	quint64 b = 0;
	std::memcpy( &b, &v, 8 );
	return QString::number( b, 16 ).rightJustified( 16, QChar( '0' ) );
}

static double hexf64( const QString & s )
{
	const quint64 b = s.toULongLong( nullptr, 16 );
	double v = 0.0;
	std::memcpy( &v, &b, 8 );
	return v;
}

int lodgenNativeSharedArrivals()
{
	return st().sharedArrivals;
}

int lodgenNativeCacheLastPlacements()
{
	return g_cacheLastPlacements;
}

//! The bulk form of `lodgenNativeLighting`: the chunk's whole contribution to
//! one arrival at once, under the same finest-ring-wins rule.
static void lightingBulk( int cx, int cy, int dim, int objectIndex,
	double ao, double sky, double ground, int verts )
{
	State & s = st();
	auto it = s.byObject.find( std::make_tuple( cx, cy, dim, objectIndex ) );
	if ( it == s.byObject.end() )
		return;
	Arrival & a = s.arrivals[size_t( it->second )];
	if ( a.litVerts && a.litDim != dim ) {
		if ( dim > a.litDim )
			return;
		a.aoSum = a.skySum = a.groundSum = 0.0;
		a.litVerts = 0;
	}
	if ( !a.litSeen ) {
		a.litSeen = true;
		a.litCx = cx;
		a.litCy = cy;
	} else if ( ( a.litCx != cx || a.litCy != cy ) && !a.shared ) {
		a.shared = true;
		s.sharedArrivals++;
	}
	a.litDim = dim;
	a.aoSum += ao;
	a.skySum += sky;
	a.groundSum += ground;
	a.litVerts += verts;
}

static void placementAoBulk( int cx, int cy, int dim, int objectIndex, double ao, int rays )
{
	State & s = st();
	if ( !s.placementAo )
		return;
	auto it = s.byObject.find( std::make_tuple( cx, cy, dim, objectIndex ) );
	if ( it == s.byObject.end() )
		return;
	Arrival & a = s.arrivals[size_t( it->second )];
	if ( a.paoRays && a.paoDim != dim ) {
		if ( dim > a.paoDim )
			return;
		a.paoSum = 0.0;
		a.paoRays = 0;
	}
	a.paoDim = dim;
	a.paoSum += ao;
	a.paoRays += rays;
}

namespace
{
struct CacheRow
{
	int objectIndex = 0;
	double ao = 0.0, sky = 0.0, ground = 0.0;
	int n = 0;
};
}

bool lodgenNativeJournalWriteCache( const LodgenNativeJournal * j, const QString & path,
	const QString & wsEdid, int dim, int cx, int cy, QString * error )
{
	auto fail = [&]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !j )
		return fail( QStringLiteral( "no journal for chunk (%1,%2): the chunk pass was not asked to record one" ).arg( cx ).arg( cy ) );

	QVector<NativePlacement> ps;
	QVector<CacheRow> lit, pao;
	QHash<int, int> litAt, paoAt;
	for ( const LodgenNativeJournalEvent & e : j->events ) {
		if ( e.kind == LNE_PLACEMENT ) {
			if ( e.p.chunkX != cx || e.p.chunkY != cy || e.p.dim != dim )
				return fail( QStringLiteral( "chunk (%1,%2) dim %3 journal carries a placement from (%4,%5) dim %6" ).arg( cx ).arg( cy ).arg( dim )
					.arg( e.p.chunkX ).arg( e.p.chunkY ).arg( e.p.dim ) );
			ps.append( e.p );
			continue;
		}
		if ( e.chunkX != cx || e.chunkY != cy || e.dim != dim )
			return fail( QStringLiteral( "chunk (%1,%2) dim %3 journal carries lighting from (%4,%5) dim %6" ).arg( cx ).arg( cy ).arg( dim )
				.arg( e.chunkX ).arg( e.chunkY ).arg( e.dim ) );
		QVector<CacheRow> & rows = ( e.kind == LNE_LIGHTING ) ? lit : pao;
		QHash<int, int> & at = ( e.kind == LNE_LIGHTING ) ? litAt : paoAt;
		auto f = at.constFind( e.objectIndex );
		int idx;
		if ( f == at.constEnd() ) {
			idx = rows.size();
			CacheRow r;
			r.objectIndex = e.objectIndex;
			rows.append( r );
			at.insert( e.objectIndex, idx );
		} else {
			idx = f.value();
		}
		CacheRow & r = rows[idx];
		r.ao += e.ao;
		if ( e.kind == LNE_LIGHTING ) {
			r.sky += e.sky;
			r.ground += e.ground;
		}
		r.n++;
	}

	QDir().mkpath( QFileInfo( path ).absolutePath() );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QStringLiteral( "cannot write the chunk cache %1" ).arg( path ) );
	QByteArray o;
	const QChar T( '\t' );
	auto line = [&]( const QString & s ) { o += s.toUtf8(); o += '\n'; };
	line( QStringLiteral( "lodj" ) + T + QStringLiteral( "1" ) + T + wsEdid + T
		+ QString::number( dim ) + T + QString::number( cx ) + T + QString::number( cy ) );
	line( QStringLiteral( "placements" ) + T + QString::number( ps.size() ) );
	for ( const NativePlacement & p : ps ) {
		QString s = QStringLiteral( "p" );
		s += T + QString::number( p.baseForm, 16 );
		s += T + QString::number( p.refForm, 16 );
		s += T + QString::number( p.scolPart );
		for ( int k = 0; k < 3; k++ )
			s += T + f32hex( p.pos[k] );
		for ( int k = 0; k < 9; k++ )
			s += T + f32hex( p.rot[k] );
		s += T + f32hex( p.scale );
		s += T + QString::number( p.slot );
		s += T + QString::number( p.isTree ? 1 : 0 );
		s += T + QString::number( p.mirrorU ? 1 : 0 );
		s += T + QString::number( p.treeHash, 16 );
		s += T + QString::number( p.hasAlpha ? 1 : 0 );
		s += T + QString::number( p.emits ? 1 : 0 );
		s += T + QString::number( p.objectIndex );
		s += T + p.model;
		line( s );
	}
	line( QStringLiteral( "lit" ) + T + QString::number( lit.size() ) );
	for ( const CacheRow & r : lit )
		line( QStringLiteral( "l" ) + T + QString::number( r.objectIndex ) + T
			+ f64hex( r.ao ) + T + f64hex( r.sky ) + T + f64hex( r.ground ) + T
			+ QString::number( r.n ) );
	line( QStringLiteral( "pao" ) + T + QString::number( pao.size() ) );
	for ( const CacheRow & r : pao )
		line( QStringLiteral( "a" ) + T + QString::number( r.objectIndex ) + T
			+ f64hex( r.ao ) + T + QString::number( r.n ) );
	line( QStringLiteral( "end" ) + T + QString::number( ps.size() ) + T
		+ QString::number( lit.size() ) + T + QString::number( pao.size() ) );
	if ( f.write( o ) != o.size() || !f.flush() )
		return fail( QStringLiteral( "short write on the chunk cache %1" ).arg( path ) );
	f.close();
	lodgenNoteLayoutFile( path );
	g_cacheLastPlacements = ps.size();
	return true;
}

bool lodgenNativeReplayCache( const QString & path, QString * error )
{
	auto fail = [&]( const QString & m ) { if ( error ) *error = m; return false; };
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "the chunk cache %1 is not there" ).arg( path ) );
	const QStringList lines = QString::fromUtf8( f.readAll() )
		.split( QChar( '\n' ), Qt::SkipEmptyParts );
	f.close();
	if ( lines.isEmpty() || !lines.at( 0 ).startsWith( QLatin1String( "lodj\t1\t" ) ) )
		return fail( QStringLiteral( "%1 is not a version 1 chunk cache" ).arg( path ) );
	const QStringList h = lines.at( 0 ).split( QChar( '\t' ) );
	if ( h.size() < 6 )
		return fail( QStringLiteral( "%1 has a short header line" ).arg( path ) );
	const int dim = h.at( 3 ).toInt(), cx = h.at( 4 ).toInt(), cy = h.at( 5 ).toInt();
	int np = 0, nl = 0, na = 0, sawP = 0, sawL = 0, sawA = 0;
	QVector<NativePlacement> ps;
	for ( int i = 1; i < lines.size(); i++ ) {
		const QStringList fd = lines.at( i ).split( QChar( '\t' ) );
		const QString & k = fd.at( 0 );
		if ( k == QLatin1String( "placements" ) ) { np = fd.value( 1 ).toInt(); continue; }
		if ( k == QLatin1String( "lit" ) ) { nl = fd.value( 1 ).toInt(); continue; }
		if ( k == QLatin1String( "pao" ) ) { na = fd.value( 1 ).toInt(); continue; }
		if ( k == QLatin1String( "end" ) ) {
			if ( fd.value( 1 ).toInt() != sawP || fd.value( 2 ).toInt() != sawL
				|| fd.value( 3 ).toInt() != sawA )
				return fail( QStringLiteral( "%1 is truncated: its end line says %2/%3/%4 and it carries %5/%6/%7" ).arg( path ).arg( fd.value( 1 ) ).arg( fd.value( 2 ) )
					.arg( fd.value( 3 ) ).arg( sawP ).arg( sawL ).arg( sawA ) );
			continue;
		}
		if ( k == QLatin1String( "p" ) ) {
			if ( fd.size() < 25 )   /* p + 3 ids + 3 pos + 9 rot + scale + slot + 5 flags + index + model */
				return fail( QStringLiteral( "%1 line %2: a placement row of %3 fields" )
					.arg( path ).arg( i + 1 ).arg( fd.size() ) );
			NativePlacement p;
			int c = 1;
			p.baseForm = fd.at( c++ ).toUInt( nullptr, 16 );
			p.refForm = fd.at( c++ ).toUInt( nullptr, 16 );
			p.scolPart = fd.at( c++ ).toInt();
			for ( int k2 = 0; k2 < 3; k2++ )
				p.pos[k2] = hexf32( fd.at( c++ ) );
			for ( int k2 = 0; k2 < 9; k2++ )
				p.rot[k2] = hexf32( fd.at( c++ ) );
			p.scale = hexf32( fd.at( c++ ) );
			p.slot = fd.at( c++ ).toInt();
			p.isTree = fd.at( c++ ).toInt() != 0;
			p.mirrorU = fd.at( c++ ).toInt() != 0;
			p.treeHash = fd.at( c++ ).toUInt( nullptr, 16 );
			p.hasAlpha = fd.at( c++ ).toInt() != 0;
			p.emits = fd.at( c++ ).toInt() != 0;
			p.objectIndex = fd.at( c++ ).toInt();
			/* the model path is the LAST field and may hold anything a path may,
			 * so it is what is left of the line rather than one split field. */
			p.model = QStringList( fd.mid( c ) ).join( QChar( '\t' ) );
			p.chunkX = cx;
			p.chunkY = cy;
			p.dim = dim;
			ps.append( p );
			sawP++;
			continue;
		}
		if ( k == QLatin1String( "l" ) ) { sawL++; continue; }
		if ( k == QLatin1String( "a" ) ) { sawA++; continue; }
	}
	if ( sawP != np || sawL != nl || sawA != na )
		return fail( QStringLiteral( "%1 promises %2/%3/%4 rows and carries %5/%6/%7" )
			.arg( path ).arg( np ).arg( nl ).arg( na ).arg( sawP ).arg( sawL ).arg( sawA ) );

	/* THE REPLAY, in the file's own order: every placement first, exactly as
	 * the chunk emitted them, then the sums. A row lands on the arrival
	 * `byObject` names, so the order among rows cannot move a value. */
	for ( const NativePlacement & p : ps )
		lodgenNativeAddPlacement( p );
	for ( int i = 1; i < lines.size(); i++ ) {
		const QStringList fd = lines.at( i ).split( QChar( '\t' ) );
		if ( fd.at( 0 ) == QLatin1String( "l" ) && fd.size() >= 6 )
			lightingBulk( cx, cy, dim, fd.at( 1 ).toInt(), hexf64( fd.at( 2 ) ),
				hexf64( fd.at( 3 ) ), hexf64( fd.at( 4 ) ), fd.at( 5 ).toInt() );
		else if ( fd.at( 0 ) == QLatin1String( "a" ) && fd.size() >= 4 )
			placementAoBulk( cx, cy, dim, fd.at( 1 ).toInt(), hexf64( fd.at( 2 ) ),
				fd.at( 3 ).toInt() );
	}
	g_cacheLastPlacements = ps.size();
	return true;
}
void lodgenNativeJournalReplay( LodgenNativeJournal * j )
{
	if ( !j )
		return;
	/* The replay thread must not be recording, or the events would go
	 * straight back into a journal. */
	LodgenNativeJournal * saved = tlsJournal;
	tlsJournal = nullptr;
	for ( const LodgenNativeJournalEvent & e : j->events ) {
		if ( e.kind == LNE_LIGHTING )
			lodgenNativeLighting( e.chunkX, e.chunkY, e.dim, e.objectIndex, e.ao, e.sky, e.ground );
		else if ( e.kind == LNE_PLACEMENT_AO )
			lodgenNativePlacementAo( e.chunkX, e.chunkY, e.dim, e.objectIndex, e.ao );
		else
			lodgenNativeAddPlacement( e.p );
	}
	tlsJournal = saved;
}

void lodgenNativeAddPlacement( const NativePlacement & p )
{
	State & s = st();
	if ( !s.active )
		return;
	if ( tlsJournal ) {
		LodgenNativeJournalEvent e;
		e.kind = LNE_PLACEMENT;
		e.p = p;
		tlsJournal->events.push_back( e );
		return;
	}
	s.arrivalsSeen++;
	const QPair<quint32, int> key( p.refForm, p.scolPart );
	auto it = s.byKey.find( key );
	int idx;
	if ( it == s.byKey.end() ) {
		idx = int( s.arrivals.size() );
		Arrival a;
		a.p = p;
		s.arrivals.push_back( a );
		s.byKey.insert( key, idx );
	} else {
		idx = it.value();
	}
	s.byObject[std::make_tuple( p.chunkX, p.chunkY, p.dim, p.objectIndex )] = idx;
}

void lodgenNativeLighting( int chunkX, int chunkY, int dim, int objectIndex, float ao, float sky, float ground )
{
	State & s = st();
	if ( !s.active )
		return;
	if ( tlsJournal ) {
		LodgenNativeJournalEvent e;
		e.kind = LNE_LIGHTING;
		e.chunkX = chunkX; e.chunkY = chunkY; e.dim = dim; e.objectIndex = objectIndex;
		e.ao = ao; e.sky = sky; e.ground = ground;
		tlsJournal->events.push_back( e );
		return;
	}
	auto it = s.byObject.find( std::make_tuple( chunkX, chunkY, dim, objectIndex ) );
	if ( it == s.byObject.end() )
		return;
	Arrival & a = s.arrivals[size_t( it->second )];
	if ( a.litVerts && a.litDim != dim ) {
		if ( dim > a.litDim )
			return;                 // a coarser ring: the finer one already spoke
		a.aoSum = a.skySum = a.groundSum = 0.0;   // a finer ring arrived: start over
		a.litVerts = 0;
	}
	/* THE SHARING COUNT (lane INCR1): the first chunk to light this
	 * arrival puts its name on it; a second one is counted once. */
	if ( !a.litSeen ) {
		a.litSeen = true;
		a.litCx = chunkX;
		a.litCy = chunkY;
	} else if ( ( a.litCx != chunkX || a.litCy != chunkY ) && !a.shared ) {
		a.shared = true;
		s.sharedArrivals++;
	}
	a.litDim = dim;
	a.aoSum += ao;
	a.skySum += sky;
	a.groundSum += ground;
	a.litVerts++;
}

void lodgenNativePlacementAo( int chunkX, int chunkY, int dim, int objectIndex, float ao )
{
	State & s = st();
	if ( !s.active || !s.placementAo )
		return;
	if ( tlsJournal ) {
		LodgenNativeJournalEvent e;
		e.kind = LNE_PLACEMENT_AO;
		e.chunkX = chunkX; e.chunkY = chunkY; e.dim = dim; e.objectIndex = objectIndex;
		e.ao = ao;
		tlsJournal->events.push_back( e );
		return;
	}
	auto it = s.byObject.find( std::make_tuple( chunkX, chunkY, dim, objectIndex ) );
	if ( it == s.byObject.end() )
		return;
	Arrival & a = s.arrivals[size_t( it->second )];
	if ( a.paoRays && a.paoDim != dim ) {
		if ( dim > a.paoDim )
			return;                 // a coarser ring: the finer one already spoke
		a.paoSum = 0.0;             // a finer ring arrived: start over
		a.paoRays = 0;
	}
	a.paoDim = dim;
	a.paoSum += ao;
	a.paoRays++;
}

/* ---------------------------------------------------------------- write */

/* ---- THE LIBRARY BUILD'S OWN SPLIT (lane PERF1, 2026-09-17) -------------
 *
 * Seven wall-clock reads inside `lodgenNativeWrite`, one per stage, so the
 * `stage times:` line can say WHERE the seconds went instead of putting the
 * whole call in `meshes`. Written on the calling thread only -- this function
 * is called once, from the region driver, after the chunk queue has joined --
 * so no atomic and no lock. Read back by `lodgenNativeLibrarySplit()`.
 *
 * `armed` is false until a write completes, so a stock bake (which never calls
 * this function) appends nothing and keeps the four-figure line it always had.
 */
namespace
{
struct NativeStageMs
{
	qint64 census = 0, models = 0, ladder = 0, bases = 0, lodo = 0, instances = 0, lodi = 0;
	int modelWorkers = 0;               //!< distinct threads SEEN loading models
	int ladderWorkers = 0;              //!< distinct threads SEEN laddering meshes
	bool armed = false;
};
NativeStageMs g_nativeStage;

//! The model-loading fan-out's worker ceiling; see the fan-out for the numbers.
constexpr int NATIVE_MODEL_WORKER_CAP = 4;

/* THE WORKER COUNT IS MEASURED, NOT ASKED (lane PERF1). `lodgenThreadCount()`
 * says what was permitted; this says how many threads actually ran a model job,
 * because that is the only number that can make the parallel gate non-vacuous.
 * One insert per job -- thousands of them against a 60-second pass -- so the
 * lock is free, and `--threads 1` reports 1, which is what the gate wants to
 * see when it checks that the way back really is the way back. */
QMutex g_nativeWorkerMutex;
QSet<quintptr> g_nativeWorkerIds;

void nativeNoteWorker()
{
	const quintptr id = quintptr( QThread::currentThreadId() );
	QMutexLocker lock( &g_nativeWorkerMutex );
	g_nativeWorkerIds.insert( id );
}
}

QString lodgenNativeLibrarySplit()
{
	if ( !g_nativeStage.armed )
		return QString();
	auto s = []( qint64 ms ) { return QString::number( double( ms ) / 1000.0, 'f', 1 ); };
	return QStringLiteral( "library: census %1 s, models %2 s, ladder %3 s, bases %4 s, "
		"lodo write %5 s, instances %6 s, lodi write %7 s, model workers %8, ladder workers %9" )
		.arg( s( g_nativeStage.census ) ).arg( s( g_nativeStage.models ) )
		.arg( s( g_nativeStage.ladder ) ).arg( s( g_nativeStage.bases ) )
		.arg( s( g_nativeStage.lodo ) ).arg( s( g_nativeStage.instances ) )
		.arg( s( g_nativeStage.lodi ) ).arg( g_nativeStage.modelWorkers )
		.arg( g_nativeStage.ladderWorkers );
}

/*! v7 grouping: THE KNOBS, in one place, because the rule is a PROPOSAL and
 *  ruling on it must be a matter of changing four numbers. Read by the
 *  placement loop (which placements are eligible) and by the grouping block
 *  (how they merge). */
struct GroupKnobs
{
	/*! Which placements are allowed to merge at all. Bethesda's houses live
	 *  under an Architecture folder; a tree, a car and a fence do not, and a
	 *  rule that merged anything touching anything would weld a street into
	 *  one id. Matched as a path COMPONENT, not a prefix -- see the census
	 *  note at the placement loop for the measurement that forced that. */
	const char * archComponent = "architecture";
	/*! How far apart two boxes may be and still count as one object, in
	 *  WORLD units. 16 is a quarter of a 64-unit Bethesda grid step: wide
	 *  enough for a wall kit whose pieces are authored with a seam, narrow
	 *  enough that two houses across a street (thousands of units) cannot
	 *  reach each other. */
	float touchTolerance = 16.0f;
	/*! The DRAWN MESH's local AABB, placed and re-bounded axis-aligned in
	 *  world -- not the base's bound SPHERE. `LodoBase` carries only
	 *  `boundRadius`, and a sphere of that radius around a long wall's
	 *  centre reaches halfway across the street; `LodoMesh` carries
	 *  `aabbMin`/`aabbExtent`, which is the geometry that is actually
	 *  drawn. Set false to measure the sphere rule instead. */
	bool useBox = true;
	//! Spatial-hash cell, world units. 33k instances makes the O(n^2) pair walk impossible.
	float gridCell = 1024.0f;

	/*! The knobs are also readable from the environment, and that is a
	 *  MEASURING surface, not a feature: the rule is a proposal, so the table
	 *  of what each knob does to a chunk has to come from bakes of the SHIPPED
	 *  code rather than from a re-implementation of it that could be wrong in
	 *  its own way. Nothing set here changes a default -- an unset variable
	 *  leaves the value above exactly as written. */
	GroupKnobs()
	{
		const QByteArray c = qgetenv( "WW_LODI_GROUP_COMPONENT" );
		if ( !c.isEmpty() ) {
			static QByteArray held;
			held = c;
			archComponent = held.constData();
		}
		bool okv = false;
		const float t = qgetenv( "WW_LODI_GROUP_TOLERANCE" ).toFloat( &okv );
		if ( okv )
			touchTolerance = t;
		const QByteArray b = qgetenv( "WW_LODI_GROUP_SHAPE" );
		if ( b == "sphere" )
			useBox = false;
		else if ( b == "box" )
			useBox = true;
		const float g = qgetenv( "WW_LODI_GROUP_GRID" ).toFloat( &okv );
		if ( okv && g > 0.0f )
			gridCell = g;
	}
};
//! The one instance. Every knob turned here, nowhere else.
static const GroupKnobs KNOB;


bool lodgenNativeWrite( QString * report, QString * error )
{
	State & s = st();
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = QStringLiteral( "native: " ) + m;
		return false;
	};
	if ( !s.active || !s.world )
		return fail( QStringLiteral( "not armed" ) );
	const EsmWorld & world = *s.world;
	g_nativeStage = NativeStageMs();
	{
		QMutexLocker lock( &g_nativeWorkerMutex );
		g_nativeWorkerIds.clear();
	}
	QElapsedTimer stageTimer;
	stageTimer.start();

	/* 1. The FULL census: every LOD-bearing base a placement of this
	 *    worldspace reaches, and the object corpus hash over exactly what the
	 *    object walk reads (spec 8.6), in ascending cell order. */
	std::vector<quint32> baseIds;
	quint64 objHash = 0, censusRefs = 0;
	if ( !nativeObjectCensus( world, &baseIds, &objHash, &censusRefs, error ) )
		return false;
	int minX, minY, maxX, maxY;
	world.cellBounds( minX, minY, maxX, maxY );
	QHash<quint32, quint32> baseIndex;
	for ( size_t i = 0; i < baseIds.size(); i++ )
		baseIndex.insert( baseIds[i], quint32( i ) );
	g_nativeStage.census = stageTimer.restart();

	/* ---- THE LIBRARY IS NOT REBUILT FOR NOTHING (lane PERF1, step 5) -----
	 *
	 * The `.lodo` is a pure function of four things: the base census, the three
	 * corpus hashes the bake record stores (load order, plugin corpus, object
	 * corpus) and the switch digest. The `--incremental` driver already REFUSES
	 * the whole run when the switch digest moves (src/nifcli.cpp), so by the
	 * time a write is offered the digest is known equal; the three hashes are
	 * compared here against the hex the record wrote last time, and the previous
	 * file is opened with a FULL payload check before one byte of it is trusted.
	 * The `.lodi` is still assembled from replay plus the dirty chunks either
	 * way -- only the library build is skipped.
	 *
	 * Two refusals are worth naming here rather than in a document:
	 *
	 *   occluders. The per-model occluder box the library build computes lives
	 *   in NEITHER file (the `.lodi` keeps only the writer's per-cell selection,
	 *   in world space, quantised), so a reused library has no box to offer and
	 *   the `.lodi` would differ. Occluders default ON, so the ruled pipeline
	 *   REBUILDS and says so; reuse pays on an occluders-off bake until the one
	 *   `.lodo` v5 field asked for in the lane report lands.
	 *
	 *   models. A mesh file edited on disk with no plugin change moves
	 *   `modelCorpusHash`, and NOTHING in the three recorded hashes sees it.
	 *   Reuse is keyed on the plugins, not on the meshes. Said plainly because
	 *   it is the hole a reader would otherwise find the hard way.
	 *
	 * The offer is consumed by the write that follows it: one arming, one
	 * write, and a second `lodgenNativeWrite` in the same process rebuilds. */
	QDir().mkpath( s.outDir );
	const QString ws = world.worldspaceEdid();
	const QString lodoPath = s.outDir + QChar( '/' ) + ws + QStringLiteral( ".lodo" );
	const QString lodiPath = s.outDir + QChar( '/' ) + ws + QStringLiteral( ".lodi" );
	/* The pair's directory is composed by the caller through
	 * lodgenFo4csWorldDir() (lane LAYOUT1, 2026-09-16); the census clause reads
	 * the root back out of the paths that were actually opened. */
	lodgenNoteLayoutFile( lodoPath );
	lodgenNoteLayoutFile( lodiPath );
	LodoHeader lh;
	QString err;

	/* What the library build fills and the tail reads, hoisted so that a REUSED
	 * library can fill the same names from the file instead of from the work. */
	LodoLibrary lib;
	std::vector<LodoMeshStats> meshStats;      // staged meshes; EMPTY under reuse
	std::vector<QString> meshStatPath;
	QHash<quint32, quint16> baseRow;           // formId -> row in the WRITTEN table
	int modelsLoaded = 0, modelsFailed = 0;
	int basesWritten = 0, basesWithoutMesh = 0;
	int cardOnlyBases = 0;     //!< CARDLINK1: bases written with a card and no mesh
	QStringList failedModels;

	bool libraryReused = false;
	QString libraryWhy;
	{
		auto hex16 = []( quint64 v ) { return QString( "%1" ).arg( v, 16, 16, QChar( '0' ) ); };
		const NativeReuseOffer offer = reuseOffer();
		reuseOffer() = NativeReuseOffer();
		if ( !offer.armed )
			libraryWhy = QStringLiteral( "not offered: this is not an incremental bake" );
		else if ( s.occluders )
			libraryWhy = QStringLiteral( "occluders are on and the per-model box is in neither file" );
		else if ( offer.loadOrderHex != hex16( world.loadOrderHash() ) )
			libraryWhy = QStringLiteral( "the load order moved" );
		else if ( offer.pluginCorpusHex != hex16( world.vhgtCorpusHash() ) )
			libraryWhy = QStringLiteral( "the plugin corpus moved" );
		else if ( offer.objectCorpusHex != hex16( objHash ) )
			libraryWhy = QStringLiteral( "the object census moved" );
		else if ( !QFileInfo::exists( lodoPath ) )
			libraryWhy = QStringLiteral( "no previous .lodo beside the one this bake would write" );
		else if ( !lodoRead( lodoPath, &lh, &lib, true, &err ) )
			libraryWhy = QStringLiteral( "the previous .lodo did not read back: " ) + err;
		else if ( lh.loadOrderHash != world.loadOrderHash()
			|| lh.pluginCorpusHash != world.vhgtCorpusHash()
			|| lh.objectCorpusHash != objHash )
			libraryWhy = QStringLiteral( "the previous .lodo's own header disagrees with the record" );
		else if ( lh.cardCorpusHash != ( s.cardsLinked ? s.cardHash : 0 ) )
			libraryWhy = QStringLiteral( "the card arrays moved" );
		else
			libraryReused = true;
		if ( libraryReused ) {
			/* The base table is the read file's, so the row numbers the `.lodi`
			 * writes are the read file's row numbers, which is the whole point. */
			basesWritten = int( lib.bases.size() );
			for ( size_t i = 0; i < lib.bases.size(); i++ )
				baseRow.insert( lib.bases[i].formId, quint16( i ) );
		} else {
			lib = LodoLibrary();
			lh = LodoHeader();
			err.clear();
		}
	}
	/* The reuse read is not free (a payload-checked open of the whole library),
	 * so it is BOOKED -- in the `models` cell, where the model load it replaces
	 * would have been. Under reuse `ladder` reads 0.0 s because no mesh was
	 * staged: an honest zero, not a missing number. */
	if ( libraryReused )
		g_nativeStage.models = stageTimer.restart();

	/* 2. Every model the bases name, loaded once, in path order. Tree-ness is
	 *    the chunk builder's own test (lodgenIsTreeModel, plus a TREE record)
	 *    so one definition of "tree" decides the sway channel. */
	struct Model
	{
		QString path;
		std::vector<NativeSrcShape> shapes;
		bool loaded = false;
		bool tree = false;
		float maxDist = 0.0f;
		bool anyAlpha = false, anyEmit = false;
		quint16 meshId = LODO_NO_MESH;
		//! v3: the occluder box fitted inside this model, in MODEL space at scale 1
		bool occOk = false;
		float occCentre[3] = { 0.0f, 0.0f, 0.0f };
		float occHalf[3] = { 0.0f, 0.0f, 0.0f };
		int occRefusal = NATIVE_OCC_NOT_WATERTIGHT;
	};
	/* v4 (lane NATIVE1c): THE FOUR MODEL SLOTS OF A BASE, in one place so the
	 * enumeration below and the base row further down cannot disagree about
	 * what the library is built from.
	 *
	 *   --library near (default): [ near MODL, MNAM 0, MNAM 1, MNAM 2 ]
	 *   --library mnam:           [ MNAM 0, MNAM 1, MNAM 2, MNAM 3 ]
	 *
	 * Under `near` the MNAM slots move ONE RUNG DOWN and MNAM 3 -- the coarsest
	 * authored LOD, which the stock bake draws only in the far ring -- falls off
	 * the end. That is the trade stated: the rung that was coarsest is given up
	 * for a rung that is the real mesh, and the ladder now has the room docs
	 * 3.5.4 measured it as lacking. A base with no near model keeps its MNAM
	 * slots in place rather than losing one, so nothing REGRESSES against the
	 * mnam arm; that case is counted (`basesWithoutNear`). */
	auto slotModels = [&]( const EsmLodBase & lb, QString out[4] ) {
		const bool haveNear = s.libraryNear && !lb.model.isEmpty();
		if ( haveNear ) {
			out[0] = lb.model;
			for ( int k = 1; k < 4; k++ )
				out[k] = lb.models[k - 1];
		} else {
			for ( int k = 0; k < 4; k++ )
				out[k] = lb.models[k];
		}
		return haveNear;
	};

	QMap<QString, Model> models;      // key = folded path: the mesh table's sort order
	int basesWithoutNear = 0;
	if ( !libraryReused ) {
		for ( quint32 b : baseIds ) {
			const EsmLodBase & lb = world.lodBase( b );
			const bool treeRecord = std::memcmp( &lb.type, "TREE", 4 ) == 0;
			QString slot[4];
			if ( !slotModels( lb, slot ) && s.libraryNear )
				basesWithoutNear++;
			for ( int k = 0; k < 4; k++ ) {
				if ( slot[k].isEmpty() )
					continue;
				Model & m = models[foldPath( slot[k] )];
				if ( m.path.isEmpty() )
					m.path = slot[k];
				if ( treeRecord || lodgenIsTreeModel( slot[k] ) )
					m.tree = true;
			}
		}
		quint64 modelHash = Q_UINT64_C( 0xCBF29CE484222325 );
		QMap<QString, LodoMaterial> materialRows;          // key -> row (layer unassigned in v1)
		QMap<QString, QString> materialString;

		/* ---- THE OBJECT PASS, FANNED OUT (lane PERF1, 2026-09-17) -------------
		 *
		 * Measured first, on the exe at launch: of an 81.2 s nine-chunk FO4CS bake,
		 * the library build is 62.8 s, and `models` -- this loop, which is a NIF
		 * parse and a vertex walk per model -- is 24.2 s of it. Nothing here reads
		 * another model, so the parse and the per-model arithmetic go wide.
		 *
		 * WHAT IS RETIRED IN JOB ORDER, and why each has to be:
		 *   * `modelHash`, an FNV-1a CHAIN over the folded path and then every
		 *     shape's vertex/triangle counts and position bytes. A chain is not
		 *     associative; folding per-model digests instead would give a different
		 *     number and that number is `modelCorpusHash` in the .lodo header, which
		 *     the bake record reads back. So the chain stays exactly where it was:
		 *     on one thread, in QMap order.
		 *   * `materialRows` / `materialString`, which are FIRST-WINS. A worker
		 *     cannot know what won globally, so it records the first use WITHIN its
		 *     own model, in shape order, and the retire does the global test in map
		 *     order. Same winner, same row, same string, every time.
		 *   * `modelsLoaded` / `modelsFailed` / `failedModels`, counters and a list
		 *     whose first eight entries are reported by path.
		 *
		 * What the worker owns outright is its own `Model`: the loaded shapes, the
		 * radius, the alpha and emit flags and the sway channel. The shared caches
		 * it reads -- `models` itself (by pointer, never by QMap::operator[], which
		 * would insert), the loader's data root -- are fixed before the fan-out and
		 * not written by anyone during it. The loader's own model cache is per
		 * THREAD as of this lane (src/lodgen.cpp lodgenNativeLoadModel).
		 *
		 * The way back: `--threads 1`, where lodgenParallelFor is the plain loop. */
		struct ModelJob
		{
			const QString * key = nullptr;
			Model * m = nullptr;
			std::vector<QString> matKey;     // first use, in shape order, this model only
			std::vector<LodoMaterial> matRow;
			std::vector<QString> matStr;
		};
		std::vector<ModelJob> modelJobs;
		modelJobs.reserve( size_t( models.size() ) );
		for ( auto it = models.begin(); it != models.end(); ++it ) {
			ModelJob j;
			j.key = &it.key();
			j.m = &it.value();
			modelJobs.push_back( std::move( j ) );
		}

		/* THE CAP, AND WHY IT IS FOUR. This stage is a NIF parse per model, and a
		 * parse is millions of small allocations against one process heap; measured
		 * on the nine-chunk region (seconds, `--threads`): 24.2 at 1, 18.8 at 2,
		 * 19.1 at 4, 24.0 at 6, 27.1 at 8, 33.9 at 16. Past four workers the stage
		 * is slower than the serial loop it replaced, so four is where it stops
		 * asking. The ladder pass below takes every thread it is given, because the
		 * same models measured 36.3, 19.7, 12.6, 10.4, 9.2, 8.3 there.
		 *
		 * The number is measured on ONE machine (bungo's: 16 logical cores, 31 GB).
		 * It is a ceiling, never a floor -- `--threads 1` is still one -- and the
		 * mechanism behind the curve is not proven, only its shape; report section
		 * 12 carries the row that asks whether it should be a switch. */
		lodgenParallelFor( int( modelJobs.size() ), [&]( int i ) {
			ModelJob & j = modelJobs[size_t( i )];
			Model & m = *j.m;
			nativeNoteWorker();
			m.loaded = s.loader( s.user, m.path, &m.shapes ) && !m.shapes.empty();
			if ( !m.loaded )
				return;
			for ( NativeSrcShape & sh : m.shapes ) {
				const quint32 nv = quint32( sh.geom.pos.size() / 3 );
				for ( quint32 v = 0; v < nv; v++ ) {
					const float * p = &sh.geom.pos[v * 3];
					m.maxDist = std::max( m.maxDist, std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] ) );
				}
				m.anyAlpha = m.anyAlpha || sh.hasAlpha;
				m.anyEmit = m.anyEmit || shapeEmits( sh );
				const QString key = materialKey( sh );
				bool seenHere = false;
				for ( const QString & k : j.matKey )
					if ( k == key ) { seenHere = true; break; }
				if ( !seenHere ) {
					LodoMaterial row;
					std::memset( &row, 0, sizeof( row ) );
					row.arrayClass = 0;
					row.arraySet = 0;
					row.layer = LODO_NO_LAYER;          // the arrays pass (lane OBJM) assigns these
					row.family = LODO_FAMILY_LEGACY;
					row.alphaThreshold = sh.hasAlpha ? sh.alphaThreshold : 0;
					row.flags = quint8( ( shapeEmits( sh ) ? LODO_MAT_EMITS : 0 ) | ( m.tree ? LODO_MAT_TREE : 0 ) );
					row.emissiveScale = shapeEmits( sh ) ? sh.emitMult : 0.0f;
					j.matKey.push_back( key );
					j.matRow.push_back( row );
					// v1: the string is the SOURCE material, or the diffuse when the shape names none
					j.matStr.push_back( sh.matName.isEmpty() ? sh.tex0 : sh.matName );
				}
			}
			// sway, the chunk builder's law, over ALL the model's shapes in its own space
			float zMin = 3.4e38f, zMax = -3.4e38f, radius = 0.0f;
			for ( const NativeSrcShape & sh : m.shapes )
				for ( size_t v = 0; v + 2 < sh.geom.pos.size(); v += 3 ) {
					zMin = std::min( zMin, sh.geom.pos[v + 2] );
					zMax = std::max( zMax, sh.geom.pos[v + 2] );
					radius = std::max( radius, std::sqrt( sh.geom.pos[v] * sh.geom.pos[v] + sh.geom.pos[v + 1] * sh.geom.pos[v + 1] ) );
				}
			const float span = zMax - zMin;
			for ( NativeSrcShape & sh : m.shapes ) {
				const size_t nv = sh.geom.pos.size() / 3;
				sh.geom.sway.assign( nv, 0 );
				if ( !m.tree || span <= 1.0e-4f )
					continue;
				for ( size_t v = 0; v < nv; v++ ) {
					const float * p = &sh.geom.pos[v * 3];
					const float hF = std::clamp( ( p[2] - zMin ) / span, 0.0f, 1.0f );
					const float rF = radius > 1.0e-4f ? std::clamp( std::sqrt( p[0] * p[0] + p[1] * p[1] ) / radius, 0.0f, 1.0f ) : 0.0f;
					sh.geom.sway[v] = quint8( std::clamp( int( std::lround( hF * hF * ( 0.35f + 0.65f * rF ) * 255.0f ) ), 0, 255 ) );
				}
			}
		}, NATIVE_MODEL_WORKER_CAP );

		// retired in job order: the hash chain, the first-wins material table, the counters
		for ( const ModelJob & j : modelJobs ) {
			const Model & m = *j.m;
			if ( !m.loaded ) {
				modelsFailed++;
				if ( failedModels.size() < 8 )
					failedModels << m.path;
				continue;
			}
			modelsLoaded++;
			modelHash = fnvStr( modelHash, *j.key );
			for ( const NativeSrcShape & sh : m.shapes ) {
				const quint32 nv = quint32( sh.geom.pos.size() / 3 ), nt = quint32( sh.geom.tris.size() / 3 );
				modelHash = fnv( modelHash, nv );
				modelHash = fnv( modelHash, nt );
				modelHash = lodoFnv1a64( sh.geom.pos.data(), sh.geom.pos.size() * sizeof( float ), modelHash );
			}
			for ( size_t k = 0; k < j.matKey.size(); k++ ) {
				if ( materialRows.contains( j.matKey[k] ) )
					continue;
				materialRows.insert( j.matKey[k], j.matRow[k] );
				materialString.insert( j.matKey[k], j.matStr[k] );
			}
		}

		{
			QMutexLocker lock( &g_nativeWorkerMutex );
			g_nativeStage.modelWorkers = g_nativeWorkerIds.size();
			g_nativeWorkerIds.clear();          // the ladder pass is counted on its own
		}
		g_nativeStage.models = stageTimer.restart();

		/* 3. The library: materials sorted by (family, class, set, layer, string),
		 *    then meshes in path order. */
		// v2: every emitted mesh is in GPU cache order (bungo 2026-09-11 08:2x).
		lib.flags |= LODO_FLAG_CACHE_ORDER;
		// v3: the cluster ladder, unless the exact way back asked for one level
		if ( s.ladder )
			lib.flags |= LODO_FLAG_LADDER;
		// v4: the two ladder knobs travel to the library unchanged (lane NATIVE1c)
		lib.ladderFoliage = s.ladderFoliage;
		lib.silhouetteMin = s.silhouetteMin;
		lib.worldspaceEdid = world.worldspaceEdid();
		lib.loadOrderHash = world.loadOrderHash();
		lib.pluginCorpusHash = world.vhgtCorpusHash();
		lib.objectCorpusHash = objHash;
		lib.modelCorpusHash = modelHash;
		// CARDLINK1: the proposed R19 hash over the linked arrays, 0 when none were linked
	lib.cardCorpusHash = s.cardsLinked ? s.cardHash : 0;
		lib.addString( QString() );
		{
			std::vector<QString> keys;
			for ( auto it = materialRows.constBegin(); it != materialRows.constEnd(); ++it )
				keys.push_back( it.key() );
			std::sort( keys.begin(), keys.end(), [&]( const QString & a, const QString & b ) {
				const LodoMaterial & A = materialRows[a];
				const LodoMaterial & B = materialRows[b];
				return std::make_tuple( A.family, A.arrayClass, A.arraySet, A.layer, foldPath( materialString[a] ) )
					< std::make_tuple( B.family, B.arrayClass, B.arraySet, B.layer, foldPath( materialString[b] ) );
			} );
			if ( keys.size() > 0xFFFF )
				return fail( QString( "%1 distinct materials; the u16 materialId holds 65,535" ).arg( keys.size() ) );
			QHash<QString, quint16> materialId;
			for ( size_t i = 0; i < keys.size(); i++ ) {
				LodoMaterial row = materialRows[keys[i]];
				row.lodmStringOffset = lib.addString( materialString[keys[i]] );
				lib.materials.push_back( row );
				materialId.insert( keys[i], quint16( i ) );
			}
			/* ---- THE LADDER PASS, FANNED OUT (lane PERF1, 2026-09-17) --------
			 *
			 * Step 1 measured this at 36.4 s of a 62.8 s library build -- the single
			 * biggest stage of an FO4CS bake. It is per-mesh work: welding, cluster
			 * formation, the ladder's simplification and the occluder fit all read
			 * ONE model's shapes and the library's fixed head (flags, the two ladder
			 * knobs, the finished material table).
			 *
			 * `lodoAppendMesh` cannot be called from a worker, because it appends
			 * into the six shared tables and its ids are positions in them. So the
			 * worker calls `lodoStageMesh`, which runs the identical code against a
			 * PRIVATE library carrying that same fixed head, and the retire calls
			 * `lodoMergeStagedMesh`, which rebases the staged rows (cluster range,
			 * vertex base, parent range, mesh id, the model string) into `lib` in
			 * ascending job order. Mesh ids, append order and the string table are
			 * therefore what the serial loop produced, byte for byte.
			 *
			 * IN BATCHES, because a staged library is held until it is merged:
			 * staging all ~3,000 meshes at once would hold the whole .lodo twice for
			 * no gain, while a batch holds a few dozen megabytes. The batch is also
			 * where the shadow-caster refusal and the alpha flag are applied, in
			 * order, so the FIRST failing model in map order is the one that speaks
			 * -- as it was when the loop was serial.
			 *
			 * `proto` is a COPY taken before the fan-out. A worker must not read
			 * `lib` while the retire writes it, and the staged build needs only the
			 * head, never a row another mesh put there. */
			LodoLibrary proto;
			proto.flags = lib.flags;
			proto.ladderFoliage = lib.ladderFoliage;
			proto.silhouetteMin = lib.silhouetteMin;
			proto.materials = lib.materials;
			proto.worldspaceEdid = lib.worldspaceEdid;

			std::vector<Model *> ladderModels;
			for ( auto it = models.begin(); it != models.end(); ++it )
				if ( it.value().loaded )
					ladderModels.push_back( &it.value() );

			struct LadderJob
			{
				LodoLibrary staged;
				LodoMeshStats ms;
				QString err;
				bool ok = false;
			};
			const size_t batchCap = 256;
			std::vector<LadderJob> batch;
			for ( size_t base = 0; base < ladderModels.size(); base += batchCap ) {
				const size_t count = std::min( batchCap, ladderModels.size() - base );
				batch.clear();
				batch.resize( count );
				lodgenParallelFor( int( count ), [&]( int i ) {
					Model & m = *ladderModels[base + size_t( i )];
					LadderJob & j = batch[size_t( i )];
					nativeNoteWorker();
					std::vector<LodoSrcShape> src;
					src.reserve( m.shapes.size() );
					for ( NativeSrcShape & sh : m.shapes ) {
						sh.geom.materialId = materialId.value( materialKey( sh ) );
						src.push_back( sh.geom );
					}
					j.ok = lodoStageMesh( j.staged, proto, src, m.path, &j.err, &j.ms );
					if ( !j.ok )
						return;
					/* v3: THE OCCLUDER BOX for this model, in its own space at scale 1.
					 * `ms.boundarySource == 0` is the watertight test -- the same
					 * boundary-edge count the shadow-caster rule reads, so one
					 * measurement serves both -- and it is the gate that keeps ray
					 * parity meaningful. */
					if ( s.occluders ) {
						std::vector<float> soupPos;
						std::vector<quint32> soupTris;
						for ( const LodoSrcShape & sh : src ) {
							const quint32 vbase = quint32( soupPos.size() / 3 );
							soupPos.insert( soupPos.end(), sh.pos.begin(), sh.pos.end() );
							for ( quint32 t : sh.tris )
								soupTris.push_back( vbase + t );
						}
						m.occRefusal = fitOccluderBox( soupPos, soupTris, j.ms.boundarySource == 0,
							m.occCentre, m.occHalf );
						m.occOk = ( m.occRefusal == NATIVE_OCC_OK );
					}
				} );
				for ( size_t i = 0; i < count; i++ ) {
					Model & m = *ladderModels[base + i];
					LadderJob & j = batch[i];
					if ( !j.ok )
						return fail( j.err );
					QString mergeErr;
					if ( !lodoMergeStagedMesh( lib, j.staged, m.path, &m.meshId, &mergeErr ) )
						return fail( mergeErr );
					meshStats.push_back( j.ms );
					meshStatPath.push_back( m.path );
					/* THE SHADOW-CASTER REFUSAL (bungo 2026-09-11 08:3x). The emitter
					 * does not decimate, so the emitted silhouette must equal the
					 * source's exactly; anything else means a triangle was lost between
					 * the loader and the clusters, and a lost triangle is a hole a far
					 * shadow leaks through. */
					if ( j.ms.boundaryEmitted > j.ms.boundarySource )
						return fail( QString( "%1: the emitted mesh has %2 boundary edges against the source's %3 -- "
							"the emit opened the silhouette and this object is a shadow caster" )
							.arg( m.path ).arg( j.ms.boundaryEmitted ).arg( j.ms.boundarySource ) );
					LodoMesh & row = lib.meshes.back();
					if ( m.anyAlpha )
						row.flags |= LODO_MESH_ANY_ALPHA;
				}
			}
			batch.clear();
		}
		g_nativeStage.ladder = stageTimer.restart();
		for ( quint32 b : baseIds ) {
			const EsmLodBase & lb = world.lodBase( b );
			LodoBase row;
			std::memset( &row, 0, sizeof( row ) );
			row.formId = b;
			row.modelStringOffset = lib.addString( lb.model );
			row.cardLayer = s.cardLayerOf.value( b, LODO_NO_CARD );
			bool any = false, tree = std::memcmp( &lb.type, "TREE", 4 ) == 0, alpha = false;
			float radius = 0.0f;
			QString slot[4];
			slotModels( lb, slot );
			for ( int k = 0; k < 4; k++ ) {
				row.rep[k] = LODO_NO_MESH;
				if ( slot[k].isEmpty() )
					continue;
				const Model & m = models[foldPath( slot[k] )];
				if ( !m.loaded )
					continue;
				row.rep[k] = m.meshId;
				any = true;
				tree = tree || m.tree;
				alpha = alpha || m.anyAlpha;
				radius = std::max( radius, m.maxDist );
			}
			/* v4, THE FIRST HEADER WORD (plan 5 row 2, census 6.3 item 2): the
			 * base's FULL-DETAIL triangle count, over the DISTINCT meshes its slots
			 * name -- a base whose four slots all resolve to one mesh counts that
			 * mesh once, which is why this is not a sum over `k`. Level-0 clusters
			 * only: the ladder's coarse levels are not full detail. The reader
			 * RECOUNTS it from the rows rather than believing it. */
			row.fullTriangles = 0;
			{
				quint16 seen[4] = { LODO_NO_MESH, LODO_NO_MESH, LODO_NO_MESH, LODO_NO_MESH };
				int ns = 0;
				for ( int k = 0; k < 4; k++ ) {
					if ( row.rep[k] == LODO_NO_MESH )
						continue;
					bool dup = false;
					for ( int j = 0; j < ns; j++ )
						dup = dup || seen[j] == row.rep[k];
					if ( dup )
						continue;
					seen[ns++] = row.rep[k];
					const LodoMesh & bm = lib.meshes[row.rep[k]];
					for ( quint32 c = bm.clusterFirst; c < bm.clusterFirst + bm.clusterCount; c++ )
						if ( lib.clusterLods[c].level == 0 )
							row.fullTriangles += lib.clusters[c].triangleCount;
				}
			}
			/* THE CARD (lane CARDLINK1): the layer the link found for this base,
			 * or 0xFFFF. A base whose slots loaded nothing but which has a card is
			 * WRITTEN -- docs 4.4's "no mesh in any slot must have a cardLayer" --
			 * with the card's own bound radius; without a card it is left out as
			 * before. */
			if ( row.cardLayer != LODO_NO_CARD && !( radius > 0.0f ) ) {
				radius = s.cardRadiusOf.value( b, 0.0f );
				if ( !any && radius > 0.0f )
					cardOnlyBases++;
			}
			if ( ( !any && row.cardLayer == LODO_NO_CARD ) || !( radius > 0.0f ) ) {
				basesWithoutMesh++;         // no slot loaded: the stock bake draws nothing for it either
				continue;
			}
			row.flags = quint16( ( tree ? LODO_BASE_TREE : 0 ) | ( alpha ? LODO_BASE_ANY_ALPHA : 0 ) | ( any ? LODO_BASE_ANY_MESH : 0 ) );
			row.boundRadius = radius;
			baseRow.insert( b, quint16( lib.bases.size() ) );
			lib.bases.push_back( row );
			basesWritten++;
		}
	}
	if ( lib.bases.empty() )
		return fail( QStringLiteral( "no base of the census loaded a LOD model; nothing to write" ) );

	/* THE DRAW RANK (bungo 2026-09-11 08:1x, "5 sounds good" -- instances
	 * pre-sorted by mesh then material). One rank per DISTINCT (primary mesh,
	 * that mesh's first material) pair, ascending, so the rank orders
	 * instances by mesh and then by material exactly as he asked. The primary
	 * mesh is `rep[0]`, the finest slot the base fills, because that is what a
	 * near-field consumer binds; a base whose rep[0] is empty takes its first
	 * filled slot, and one with no mesh at all sorts last (0xFFFF, 0xFFFF).
	 * It is a pure function of baseId, which is what lets the `.lodi` reader
	 * check the sort without opening the `.lodo`. */
	std::vector<quint16> baseDrawKey( lib.bases.size(), 0 );
	{
		std::vector<std::pair<quint16, quint16>> pairs( lib.bases.size() );
		for ( size_t i = 0; i < lib.bases.size(); i++ ) {
			quint16 mid = LODO_NO_MESH;
			for ( int k = 0; k < 4 && mid == LODO_NO_MESH; k++ )
				mid = lib.bases[i].rep[k];
			quint16 mat = 0xFFFF;
			if ( mid != LODO_NO_MESH ) {
				const LodoMesh & m = lib.meshes[mid];
				if ( m.clusterCount )
					mat = lib.clusters[m.clusterFirst].materialId;
			}
			pairs[i] = std::make_pair( mid, mat );
		}
		std::vector<std::pair<quint16, quint16>> distinct = pairs;
		std::sort( distinct.begin(), distinct.end() );
		distinct.erase( std::unique( distinct.begin(), distinct.end() ), distinct.end() );
		if ( distinct.size() > 0xFFFF )
			return fail( QString( "%1 distinct (mesh, material) draw pairs; the u16 drawKey holds 65,535" )
				.arg( distinct.size() ) );
		for ( size_t i = 0; i < pairs.size(); i++ )
			baseDrawKey[i] = quint16( std::lower_bound( distinct.begin(), distinct.end(), pairs[i] ) - distinct.begin() );
	}
	g_nativeStage.bases = stageTimer.restart();
	/* Under reuse the previous file IS the new file: not rewritten, not
	 * touched, not even opened again -- `lh` is the header the reuse read
	 * gave back, so every census clause below reads the same numbers it
	 * would have read from a fresh write. */
	if ( !libraryReused && !lodoWrite( lodoPath, lib, &lh, &err ) )
		return fail( err );
	g_nativeStage.lodo = stageTimer.restart();

	/* 4. The instances: one per (ref, part), the finest ring's lighting. */
	LodiSrcSet set;
	set.worldspaceEdid = ws;
	set.pluginCorpusHash = lib.pluginCorpusHash;
	set.objectCorpusHash = lib.objectCorpusHash;
	set.lodoIdentity = lodoIdentityOf( lh.headerCrc32, lh.modelCorpusHash, lh.objectCorpusHash );
	set.loadOrderHash = lib.loadOrderHash;
	int droppedNoBase = 0, unlit = 0, noIdentity = 0, paoUnmeasured = 0;
	//! v9 census: how many placements the three-clause workshop rule marked.
	quint32 scrappablePlacements = 0;
	//! CARDLINK1 census: instances given FORCE_CARD, and which clause gave it
	quint32 forcedCard = 0, forcedEmptySlot = 0, forcedByLine = 0;
	std::vector<std::array<int, 3>> instChunk;   //!< v6: (chunkX, chunkY, dim) that lit each instance, parallel to set.instances
	std::vector<quint8> instArch;   //!< v7: 1 when the base model path is under the architecture folder
	quint32 archPlacements = 0;     //!< v7 census: how many placements the path prefix catches
	/*! v7 grouping, the PROXIMITY JOIN's own census: what it was given, what it
	 *  measured and what it cost. Declared here, beside `archPlacements`, because
	 *  the census line that prints them is written long after the block that
	 *  fills them has closed. */
	quint32 joinEligible = 0;
	quint64 joinSamples = 0, joinPairs = 0;
	qint64 joinMs = 0;
	set.placementAo = s.placementAo;
	QVector<LodgenAggTree> aggTrees;
	for ( const Arrival & a : s.arrivals ) {
		const NativePlacement & p = a.p;
		auto br = baseRow.find( p.baseForm );
		if ( br == baseRow.end() ) {
			droppedNoBase++;
			continue;
		}
		LodiSrcInstance r;
		for ( int k = 0; k < 3; k++ )
			r.pos[k] = p.pos[k];
		for ( int k = 0; k < 9; k++ )
			r.rot[k] = p.rot[k];
		r.scale = p.scale;
		r.baseId = br.value();
		r.refFormId = p.refForm;
		r.scolPart = qint16( p.scolPart );
		if ( a.litVerts ) {
			r.ao = quint8( std::clamp( int( std::lround( a.aoSum / a.litVerts * 255.0 ) ), 0, 255 ) );
			r.sky = quint8( std::clamp( int( std::lround( a.skySum / a.litVerts * 255.0 ) ), 0, 255 ) );
			r.ground = quint8( std::clamp( int( std::lround( a.groundSum / a.litVerts * 255.0 ) ), 0, 255 ) );
		} else {
			unlit++;
			r.ao = 255; r.sky = 255; r.ground = 0;
		}
		r.drawKey = baseDrawKey[br.value()];
		/* IDENTITY (bungo 2026-09-11 08:4x: the far-shadow pass keys on the
		 * colour id). The stock bake's own identity index for this placement,
		 * the manifest's `index` column, R + G*256 of the .bto vertex colour.
		 * -1 means the chunk builder never gave this placement one -- with
		 * --no-identity, say -- and the record then carries 0 and the census
		 * line COUNTS it, so a zero is never mistaken for "index 0". */
		if ( p.objectIndex < 0 || p.objectIndex > 0xFFFF ) {
			noIdentity++;
			r.identity = 0;
		} else {
			r.identity = quint16( p.objectIndex );
		}
		/* v5: the PLACEMENT AO byte and the MNAM slot this placement was drawn
		 * at. 0xFF is NOT AO 255 here, it is NOT MEASURED -- a placement whose
		 * chunk was baked with --no-ao, or which no probe reached, says so
		 * rather than claiming full daylight. */
		if ( s.placementAo ) {
			if ( a.paoRays )
				r.placementAo = quint8( std::clamp( int( std::lround( a.paoSum / a.paoRays * double( LODI_PLACEMENT_AO_MAX ) ) ),
					0, int( LODI_PLACEMENT_AO_MAX ) ) );
			else
				paoUnmeasured++;
		}
		r.mnamSlot = quint8( std::clamp( p.slot, 0, 3 ) );
		r.seed = quint8( p.treeHash & 0xFF );
		r.flags = quint16( ( p.mirrorU ? LODI_INST_MIRRORED : 0 ) | ( p.hasAlpha ? LODI_INST_ALPHA_TESTED : 0 )
			| ( p.emits ? LODI_INST_EMITS : 0 ) | ( p.scolPart >= 0 ? LODI_INST_SCOL_PART : 0 ) );
		/* FORCE_CARD (lane CARDLINK1, 2026-09-24): the base has a card layer AND
		 * this placement's own ring -- the MNAM slot of the arrival the record
		 * keeps -- either has no mesh, or its chunk's `C` line stood it on its
		 * card (`--impostors-from-level`). Never set without a card layer, so a
		 * bake that linked no cards writes the bit nowhere. */
		if ( lib.bases[r.baseId].cardLayer != LODO_NO_CARD ) {
			const bool emptySlot = lib.bases[r.baseId].rep[r.mnamSlot] == LODO_NO_MESH;
			const bool byLine = s.cardPlaced.count( std::make_tuple( p.chunkX, p.chunkY, p.dim, p.objectIndex ) ) > 0;
			if ( emptySlot || byLine ) {
				r.flags |= LODI_INST_FORCE_CARD;
				forcedCard++;
				forcedEmptySlot += emptySlot ? 1 : 0;
				forcedByLine += byLine ? 1 : 0;
			}
		}
		/* v9, THE SCRAPPABLE BIT (lane HORIZON3, 2026-09-19). The rule is read
		 * out of the plugin, not guessed from the model path: the base must be
		 * the target of a workshop SCRAP recipe, the placement must stand
		 * inside a workshop BUILD AREA, and the base must not refuse to be
		 * scrapped (`EsmScrapIndex`, src/esmdata.h). The index behind it is
		 * built once, on the first call here, so a bake that never asks never
		 * walks the plugin for it.
		 *
		 * THE POSITION IS THE PLACEMENT'S OWN, not the chunk's and not the
		 * base's: a build area is a box a few thousand units across and a
		 * settlement's edge runs through the middle of object rows. A
		 * per-chunk answer would mark whole streets. */
		if ( s.scrappable && s.world && s.world->scrappable( p.baseForm, p.pos ) ) {
			r.flags = quint16( r.flags | quint16( LODI_INST_SCRAPPABLE ) );
			scrappablePlacements++;
		}
		r.boundRadius = lib.bases[br.value()].boundRadius;   // at scale 1; lodiWrite applies the quantised scale
		/* v3: the occluder box of the mesh this placement actually DREW -- not
		 * of its finest slot -- because the box has to lie inside the geometry
		 * the file carries for it. The writer picks which boxes survive per
		 * cell; the emitter only offers. */
		if ( s.occluders ) {
			auto mi = models.constFind( foldPath( p.model ) );
			if ( mi != models.constEnd() && mi.value().occOk && mi.value().meshId != LODO_NO_MESH ) {
				r.hasOccluder = true;
				for ( int k = 0; k < 3; k++ ) {
					r.occCentre[k] = mi.value().occCentre[k];
					r.occHalf[k] = mi.value().occHalf[k];
				}
				r.occMeshId = mi.value().meshId;
			}
		}
		r.baseName = QString( "0x%1 (%2)" ).arg( p.baseForm, 8, 16, QChar( '0' ) ).arg( p.model );
		set.instances.push_back( r );
		instChunk.push_back( { p.chunkX, p.chunkY, p.dim } );
		/* v7: the path test, taken HERE because this is the only place both the
		 * placement and its base row are in hand.
		 *
		 * The string asked is the BASE ROW'S model -- `lb.model`, the near MODL,
		 * the one `lib.addString` already put in the .lodo -- and not `p.model`,
		 * the model this placement DREW. Two measurements forced that, both on
		 * chunk 4.4.-12's own bake and both before this line was written:
		 *
		 *   - the brief's literal "starts with architecture\" caught 0 of 2,449;
		 *   - a path-COMPONENT test on the DRAWN model caught 238 of 2,449,
		 *     because the drawn model of a far placement is the authored LOD and
		 *     Bethesda files those by NEIGHBOURHOOD, not by kind:
		 *     `LOD\Neighborhoods\Cambridge\Cambridge10_Bld01LOD.nif`. Exactly 1 of
		 *     the 314 LOD-rooted paths in this .lodo carries an `architecture`
		 *     component; the other 313 are houses filed under a place name.
		 *
		 * The base's SOURCE path does carry the kind -- `Architecture\Buildings\
		 * BldgBrick7Story3x5FreeComEntA.nif` -- and catches 1,877 of 2,449. It is
		 * also the string the .lodo SHIPS (`bases[].modelStringOffset`), so a
		 * refuter reading the file judges the identical bytes this rule judged
		 * rather than a paraphrase of them. Both separators, because a path from
		 * an ESM and a path from a loose file do not agree on which one Bethesda
		 * used. */
		{
			const QString bp = lib.stringAt( lib.bases[br.value()].modelStringOffset );
			bool arch = false;
			int from = 0;
			while ( from <= bp.size() ) {
				int sep = from;
				while ( sep < bp.size() && bp[sep] != QLatin1Char( '\\' ) && bp[sep] != QLatin1Char( '/' ) )
					sep++;
				if ( QStringView( bp ).mid( from, sep - from ).compare( QLatin1String( KNOB.archComponent ), Qt::CaseInsensitive ) == 0 ) {
					arch = true;
					break;
				}
				from = sep + 1;
			}
			instArch.push_back( arch ? 1 : 0 );
			if ( arch )
				archPlacements++;
		}
		/* v4: the aggregate's source list, gathered HERE because `srcIndex` is
		 * the index in `set.instances` and nowhere else knows it. The rotation
		 * is the DRAWN one and `mirrorU` the repetition breaker's own bit --
		 * bungo's *"with the same rotation and mirror the repetition breaking
		 * would give them"*. */
		if ( s.aggArmed && p.isTree ) {
			LodgenAggTree t;
			t.srcIndex = quint32( set.instances.size() - 1 );
			t.baseId = p.baseForm;
			for ( int k = 0; k < 3; k++ )
				t.pos[k] = p.pos[k];
			for ( int k = 0; k < 9; k++ )
				t.rot[k] = p.rot[k];
			t.scale = p.scale;
			t.mirrorU = p.mirrorU;
			aggTrees.append( t );
		}
	}
	/* ---- v6: THE VERTEX-AO STREAM (2026-09-18; bungo: "where is the vertex AO we
	 * had?" / "The AO on the objects was from the objects themselves, from objects
	 * amongst each other, and with the objects and terrain and with objects on
	 * nearby chunks too").
	 *
	 * The .BTO route casts scene occlusion at every chunk vertex; the v5 byte
	 * above is that cast's mean per placement, and a mean painted flat over a
	 * house is a black storey. The per-vertex value cannot live in the shared
	 * .lodo (it differs per copy), so it is cast HERE, per instance, over the
	 * library's own vertices in the library's own order, and streamed into the
	 * .lodi. The scene is the chunk pass's: the ESM heightfield of the chunk
	 * plus one skirt cell each way (128-unit samples), every instance whose
	 * origin lies inside that field -- the chunk's own and the apron from the
	 * neighbours -- and the same LodgenAoScene caster with the same 8 rays and
	 * the same reach (300 miniature units = 300 x dim world units). One scene a
	 * chunk, chunks in parallel. A card-drawn instance, a base with no mesh in
	 * its slot, or a chunk with no land record leaves its range EMPTY, which the
	 * census counts. */
	int vaoInstances = 0, vaoEmpty = 0, vaoChunks = 0, vaoNoLand = 0;
	quint64 vaoBytes = 0, vaoDark = 0;
	double vaoSum = 0.0;
	int vskyInstances = 0;
	quint64 vskyBytes = 0, vskyOpen = 0;
	double vskySum = 0.0;
	if ( s.vertexAo && s.placementAo && s.world ) {
		set.vertexAo = true;
		// v7: the sky stream rides the SAME loop, the same scene, the same reach
		set.vertexSky = s.lodiV7;
		// the library meshes decoded once: vertex range, positions, normals, level-0 triangles
		struct DecodedMesh {
			quint32 first = 0, count = 0;
			bool contiguous = true;
			std::vector<float> pos, nrm;      // 3 a vertex, mesh space at scale 1
			std::vector<quint32> tris;        // level-0 triangles, indices into the range
		};
		std::vector<DecodedMesh> dm( lib.meshes.size() );
		for ( size_t m = 0; m < lib.meshes.size(); m++ ) {
			const LodoMesh & mesh = lib.meshes[m];
			DecodedMesh & d = dm[m];
			quint32 lo = 0xFFFFFFFFu, hi = 0, sum = 0;
			for ( quint32 c = mesh.clusterFirst; c < mesh.clusterFirst + mesh.clusterCount && c < lib.clusters.size(); c++ ) {
				const LodoCluster & cl = lib.clusters[c];
				lo = std::min( lo, cl.vertexBase );
				hi = std::max( hi, cl.vertexBase + cl.vertexCount );
				sum += cl.vertexCount;
			}
			if ( hi <= lo || hi > lib.vertices.size() )
				continue;
			d.first = lo;
			d.count = hi - lo;
			d.contiguous = ( sum == d.count );
			if ( !d.contiguous )
				return fail( QString( "mesh %1 (%2): its clusters cover %3 vertices over a range of %4; the vertex-AO stream needs one contiguous range" )
					.arg( m ).arg( lib.stringAt( mesh.modelStringOffset ) ).arg( sum ).arg( d.count ) );
			d.pos.resize( size_t( d.count ) * 3 );
			d.nrm.resize( size_t( d.count ) * 3 );
			for ( quint32 v = 0; v < d.count; v++ ) {
				const LodoVertex & lv = lib.vertices[lo + v];
				for ( int k = 0; k < 3; k++ )
					d.pos[size_t( v ) * 3 + k] = lodoDequantU16( lv.pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] );
				lodoUnpackOct12( quint32( lv.nrm[0] ) | ( quint32( lv.nrm[1] ) << 8 ) | ( quint32( lv.nrm[2] ) << 16 ), &d.nrm[size_t( v ) * 3] );
			}
			for ( quint32 c = mesh.clusterFirst; c < mesh.clusterFirst + mesh.clusterCount && c < lib.clusters.size(); c++ ) {
				if ( c < lib.clusterLods.size() && lib.clusterLods[c].level != 0 )
					continue;
				const LodoCluster & cl = lib.clusters[c];
				const size_t li = size_t( c ) * LODO_LOCAL_INDEX_BYTES;
				for ( int t = 0; t < int( cl.triangleCount ) && li + size_t( t ) * 3 + 2 < lib.localIndices.size(); t++ ) {
					const quint8 a = lib.localIndices[li + size_t( t ) * 3], b = lib.localIndices[li + size_t( t ) * 3 + 1],
						cc = lib.localIndices[li + size_t( t ) * 3 + 2];
					if ( a == LODO_LOCAL_INDEX_NONE || b == LODO_LOCAL_INDEX_NONE || cc == LODO_LOCAL_INDEX_NONE )
						continue;
					d.tris.push_back( cl.vertexBase + a - lo );
					d.tris.push_back( cl.vertexBase + b - lo );
					d.tris.push_back( cl.vertexBase + cc - lo );
				}
			}
		}
		// which mesh each instance draws, and which chunk lit it
		const size_t ni = set.instances.size();
		std::vector<quint16> instMesh( ni, LODO_NO_MESH );
		for ( size_t i = 0; i < ni; i++ ) {
			const LodiSrcInstance & r = set.instances[i];
			if ( r.baseId < lib.bases.size() && r.mnamSlot < 4 )
				instMesh[i] = lib.bases[r.baseId].rep[r.mnamSlot];
		}
		std::vector<std::tuple<int, int, int>> chunkKeys;
		std::vector<std::vector<quint32>> chunkInst;
		{
			std::map<std::tuple<int, int, int>, size_t> at;
			for ( size_t i = 0; i < ni; i++ ) {
				const std::tuple<int, int, int> key( instChunk[i][0], instChunk[i][1], instChunk[i][2] );
				auto f = at.find( key );
				if ( f == at.end() ) {
					f = at.emplace( key, chunkKeys.size() ).first;
					chunkKeys.push_back( key );
					chunkInst.emplace_back();
				}
				chunkInst[f->second].push_back( quint32( i ) );
			}
		}
		std::vector<int> chunkNoLand( chunkKeys.size(), 0 );
		/* ONE reader, many workers (lane BAKE1, 2026-09-25): `s.world` is shared, and
		 * libfo76utils decompresses a record IN the reader on its first read
		 * (`ESMFile::uncompressRecord`: `zlibBuf.back()` resized, a new buffer
		 * emplaced, the record's flags and data pointer rewritten). Two workers
		 * reaching two never-read LAND records at once corrupt each other; on the
		 * whole Commonwealth a pool thread threw and the bake died in terminate().
		 * The read is serialized; the heights copy and the rays stay parallel, and
		 * the bytes cannot move. */
		QMutex landLock;
		lodgenParallelFor( int( chunkKeys.size() ), [&]( int ci ) {
			nativeNoteWorker();
			const int cx = std::get<0>( chunkKeys[size_t( ci )] ), cy = std::get<1>( chunkKeys[size_t( ci )] );
			const int dim = std::max( 1, std::get<2>( chunkKeys[size_t( ci )] ) );
			constexpr int SKIRT = 1;
			const int cells = dim + 2 * SKIRT;
			/* The scene is built in the CHUNK'S MINIATURE units (world x 1/dim), as
			 * the .BTO colour-B cast was (src/lodgen.cpp ~3940, invDim): the caster's
			 * 2-unit ray-start offset and its 300-unit reach are then the same
			 * fraction of the geometry they were there. In world units the offset
			 * was 2 u, and an overpass whose road-surface layer floats 3-5 u above
			 * its slab got the slab top darkened by its own second layer (bungo
			 * 2026-09-18, "why is this area so dark on the AO?"). */
			const float inv = 1.0f / float( dim );
			LodgenAoScene scene;
			scene.hn = cells * 32 + 1;
			scene.hSpacing = 128.0f * inv;
			scene.ox = float( cx - SKIRT ) * 4096.0f * inv;
			scene.oy = float( cy - SKIRT ) * 4096.0f * inv;
			scene.span = float( cells ) * 4096.0f * inv;
			scene.hgt.assign( size_t( scene.hn ) * size_t( scene.hn ), s.world->defaultLandHeight() * inv );
			int landCells = 0;
			{
				EsmLand land;
				for ( int ly = 0; ly < cells; ly++ )
					for ( int lx = 0; lx < cells; lx++ ) {
						bool got;
						{
							QMutexLocker lock( &landLock );
							got = s.world->land( cx - SKIRT + lx, cy - SKIRT + ly, land );
						}
						if ( got ) {
							landCells++;
							for ( int row = 0; row < 33; row++ )
								for ( int col = 0; col < 33; col++ )
									scene.hgt[size_t( ly * 32 + row ) * size_t( scene.hn ) + size_t( lx * 32 + col )] = land.heights[row][col] * inv;
						}
					}
			}
			if ( landCells == 0 ) {
				chunkNoLand[size_t( ci )] = 1;
				return;
			}
			// every instance whose origin lies in the field: the chunk's own and the apron
			const float x0 = scene.ox, x1 = scene.ox + scene.span, y0 = scene.oy, y1 = scene.oy + scene.span;  // miniature
			auto place = [&]( const LodiSrcInstance & r, quint16 mid, auto && perVertex, bool addTris ) {
				const DecodedMesh & d = dm[mid];
				std::vector<Vector3> wp( d.count );
				for ( quint32 v = 0; v < d.count; v++ ) {
					const float * lp = &d.pos[size_t( v ) * 3];
					const float sx = lp[0] * r.scale, sy = lp[1] * r.scale, sz = lp[2] * r.scale;
					wp[v] = Vector3( ( r.rot[0] * sx + r.rot[1] * sy + r.rot[2] * sz + r.pos[0] ) * inv,
						( r.rot[3] * sx + r.rot[4] * sy + r.rot[5] * sz + r.pos[1] ) * inv,
						( r.rot[6] * sx + r.rot[7] * sy + r.rot[8] * sz + r.pos[2] ) * inv );
				}
				if ( addTris )
					for ( size_t t = 0; t + 2 < d.tris.size(); t += 3 ) {
						const Vector3 & ta = wp[d.tris[t]], & tb = wp[d.tris[t + 1]], & tc = wp[d.tris[t + 2]];
						scene.addTriangle( ta, tb, tc );
					}
				perVertex( d, wp );
			};
			auto noop = []( const DecodedMesh &, const std::vector<Vector3> & ) {};
			for ( size_t i = 0; i < ni; i++ ) {
				const LodiSrcInstance & r = set.instances[i];
				if ( instMesh[i] == LODO_NO_MESH || instMesh[i] >= dm.size() || dm[instMesh[i]].count == 0 )
					continue;
				if ( r.pos[0] * inv < x0 || r.pos[0] * inv >= x1 || r.pos[1] * inv < y0 || r.pos[1] * inv >= y1 )
					continue;
				place( r, instMesh[i], noop, true );
			}
			const float maxT = 300.0f;  // miniature units = 300 x dim world units, as the .BTO cast
			for ( quint32 i : chunkInst[size_t( ci )] ) {
				LodiSrcInstance & r = set.instances[i];
				if ( instMesh[i] == LODO_NO_MESH || instMesh[i] >= dm.size() || dm[instMesh[i]].count == 0 )
					continue;
				const bool wantSky = set.vertexSky;
				place( r, instMesh[i], [&]( const DecodedMesh & d, const std::vector<Vector3> & wp ) {
					r.vertexAo.resize( d.count );
					if ( wantSky )
						r.vertexSky.resize( d.count );
					for ( quint32 v = 0; v < d.count; v++ ) {
						const float * ln = &d.nrm[size_t( v ) * 3];
						Vector3 wn( r.rot[0] * ln[0] + r.rot[1] * ln[1] + r.rot[2] * ln[2],
							r.rot[3] * ln[0] + r.rot[4] * ln[1] + r.rot[5] * ln[2],
							r.rot[6] * ln[0] + r.rot[7] * ln[1] + r.rot[8] * ln[2] );
						wn.normalize();
						const float ao = scene.ambientOcclusion( wp[v], wn, maxT );
						r.vertexAo[v] = quint8( std::lround( std::min( 1.0f, std::max( 0.0f, ao ) ) * 255.0f ) );
						if ( wantSky ) {
							/* v7 (bungo: "We need per vertex sky visbility too").
							 * The SAME caster, the SAME scene, the SAME reach in
							 * the SAME miniature units as the AO byte beside it --
							 * `skyVisibility` is normal-independent by design, so
							 * the normal above is not passed to it. */
							const float sk = scene.skyVisibility( wp[v], maxT );
							r.vertexSky[v] = quint8( std::lround( std::min( 1.0f, std::max( 0.0f, sk ) ) * 255.0f ) );
						}
					}
				}, false );
			}
		} );
		vaoChunks = int( chunkKeys.size() );
		for ( int x : chunkNoLand )
			vaoNoLand += x;
		for ( const LodiSrcInstance & r : set.instances ) {
			if ( r.vertexAo.empty() ) {
				vaoEmpty++;
				continue;
			}
			vaoInstances++;
			vaoBytes += r.vertexAo.size();
			for ( quint8 v : r.vertexAo ) {
				vaoSum += v;
				if ( v < 128 )
					vaoDark++;
			}
		}
		for ( const LodiSrcInstance & r : set.instances ) {
			if ( r.vertexSky.empty() )
				continue;
			vskyInstances++;
			vskyBytes += r.vertexSky.size();
			for ( quint8 v : r.vertexSky ) {
				vskySum += v;
				if ( v >= 128 )
					vskyOpen++;
			}
		}
	}
	/* ---- v7: THE GROUPING. bungo 2026-09-18: "The houses should be one object
	 * each though, for identity".
	 *
	 * THIS RULE IS A PROPOSAL. Every knob it turns is in `GroupKnobs` above
	 * this file's one emit function, so that ruling on it is a matter of
	 * changing four numbers rather than reading the loop. `identity` is NOT
	 * touched: it stays the stock bake's unique per-placement index, and the
	 * group is a SECOND word beside it (docs s4.9). */
	if ( s.lodiV7 ) {
		set.group = true;
		const size_t ni = set.instances.size();
		std::vector<quint32> uf( ni );
		for ( size_t i = 0; i < ni; i++ )
			uf[i] = quint32( i );
		std::function<quint32( quint32 )> find = [&]( quint32 a ) {
			while ( uf[a] != a ) { uf[a] = uf[uf[a]]; a = uf[a]; }
			return a;
		};
		auto join = [&]( quint32 a, quint32 b ) {
			a = find( a ); b = find( b );
			if ( a != b )
				uf[std::max( a, b )] = std::min( a, b );
		};
		/* (i) A SCOL part's group is its SCOL reference's group. The parts of
		 * one static collection ARE one object and always were -- the stock
		 * bake split them only so each could carry its own draw key. */
		{
			std::unordered_map<quint32, quint32> scolFirst;
			for ( size_t i = 0; i < ni; i++ ) {
				if ( set.instances[i].scolPart < 0 )
					continue;
				auto f = scolFirst.find( set.instances[i].refFormId );
				if ( f == scolFirst.end() )
					scolFirst.emplace( set.instances[i].refFormId, quint32( i ) );
				else
					join( f->second, quint32( i ) );
			}
		}
		/* (ii) THE JOIN. Two rules live here and the command line picks one.
		 *
		 * PROXIMITY (the default since bungo's ruling of 2026-09-19): every
		 * placement that is NOT a tree and HAS a drawn LOD mesh joins a connected
		 * component with every other one whose MESH comes within
		 * `--identity-join-gap` world units (64 by default). Trees and card-only
		 * placements stay singletons.
		 *
		 * LEGACY (`--identity-join legacy`): the shipped rule -- only a placement
		 * whose BASE model path carries an `architecture` component joins, and it
		 * joins when the two WORLD AXIS-ALIGNED boxes are within 16 u on every
		 * axis. It is kept because it is the exact way back and because it is the
		 * red control of the gate: it must reproduce 588 groups on chunk 4.4.-12.
		 *
		 * WHY THE MEASURE CHANGED AND NOT ONLY THE NUMBER (lane IDENTPROX,
		 * scratchpad/identprox_20260919/RECOMMENDATION.txt): the box rule at 16 u
		 * ALREADY welds 286 placements across 9 Creation Kit layers into one
		 * 18,121-unit identity, because the elevated highway deck's axis-aligned
		 * box hangs over four South Boston city blocks. No gap fixes that; only
		 * the measure does. With the mesh measure the only identity above the
		 * 6,110 u threshold anywhere in the chunk, at every gap from 16 to 128 u,
		 * is the highway itself. */
		const bool legacyJoin = s.identityJoinLegacy;
		struct Box { float lo[3], hi[3]; };
		std::vector<Box> box( ni );
		std::vector<quint32> arch;          //!< the placements THIS rule lets join
		std::vector<quint16> meshOf( ni, quint16( LODO_NO_MESH ) );
		arch.reserve( archPlacements );
		for ( size_t i = 0; i < ni; i++ ) {
			const LodiSrcInstance & r = set.instances[i];
			Box & b = box[i];
			for ( int k = 0; k < 3; k++ ) { b.lo[k] = r.pos[k]; b.hi[k] = r.pos[k]; }
			quint16 mid = LODO_NO_MESH;
			if ( r.baseId < lib.bases.size() && r.mnamSlot < 4 )
				mid = lib.bases[r.baseId].rep[r.mnamSlot];
			meshOf[i] = mid;
			const bool hasMesh = ( mid != LODO_NO_MESH && mid < lib.meshes.size() );
			/* A TREE NEVER JOINS. Not a taste: a tree's far shadow is cast from its
			 * own authored LOD mesh with alpha test, and an identity shared with the
			 * wall it grows against would exclude that wall from the tree's shadow.
			 * The flag is the library's own (`LODO_BASE_TREE`), which is what the
			 * card baker and the foliage refusal already read. */
			const bool isTree = ( r.baseId < lib.bases.size() )
				&& ( lib.bases[r.baseId].flags & LODO_BASE_TREE ) != 0;
			const bool eligible = legacyJoin
				? ( i < instArch.size() && instArch[i] != 0 )
				: ( !isTree && hasMesh );
			if ( !eligible )
				continue;
			if ( KNOB.useBox && mid != LODO_NO_MESH && mid < lib.meshes.size() ) {
				// the mesh's eight local corners, placed, then re-bounded axis-aligned
				const LodoMesh & me = lib.meshes[mid];
				bool first = true;
				for ( int c = 0; c < 8; c++ ) {
					float lp[3];
					for ( int k = 0; k < 3; k++ )
						lp[k] = ( me.aabbMin[k] + ( ( c >> k ) & 1 ? me.aabbExtent[k] : 0.0f ) ) * r.scale;
					const float wx = r.rot[0] * lp[0] + r.rot[1] * lp[1] + r.rot[2] * lp[2] + r.pos[0];
					const float wy = r.rot[3] * lp[0] + r.rot[4] * lp[1] + r.rot[5] * lp[2] + r.pos[1];
					const float wz = r.rot[6] * lp[0] + r.rot[7] * lp[1] + r.rot[8] * lp[2] + r.pos[2];
					const float w[3] = { wx, wy, wz };
					for ( int k = 0; k < 3; k++ ) {
						b.lo[k] = first ? w[k] : std::min( b.lo[k], w[k] );
						b.hi[k] = first ? w[k] : std::max( b.hi[k], w[k] );
					}
					first = false;
				}
			} else {
				// the way back: the base's bound sphere at the quantised scale
				const float rad = r.boundRadius * r.scale;
				for ( int k = 0; k < 3; k++ ) { b.lo[k] = r.pos[k] - rad; b.hi[k] = r.pos[k] + rad; }
			}
			arch.push_back( quint32( i ) );
		}
		if ( legacyJoin ) {
			const float T = KNOB.touchTolerance, G = KNOB.gridCell;
			std::unordered_map<quint64, std::vector<quint32>> grid;
			auto keyOf = []( int gx, int gy ) {
				return ( quint64( quint32( gx ) ) << 32 ) | quint32( gy );
			};
			for ( quint32 i : arch ) {
				const Box & b = box[i];
				const int gx0 = int( std::floor( ( b.lo[0] - T ) / G ) ), gx1 = int( std::floor( ( b.hi[0] + T ) / G ) );
				const int gy0 = int( std::floor( ( b.lo[1] - T ) / G ) ), gy1 = int( std::floor( ( b.hi[1] + T ) / G ) );
				for ( int gy = gy0; gy <= gy1; gy++ )
					for ( int gx = gx0; gx <= gx1; gx++ )
						grid[keyOf( gx, gy )].push_back( i );
			}
			for ( auto & cellIt : grid ) {
				std::vector<quint32> & v = cellIt.second;
				for ( size_t a = 0; a < v.size(); a++ )
					for ( size_t b2 = a + 1; b2 < v.size(); b2++ ) {
						const Box & A = box[v[a]];
						const Box & B = box[v[b2]];
						bool touch = true;
						for ( int k = 0; k < 3 && touch; k++ )
							if ( A.lo[k] - B.hi[k] > T || B.lo[k] - A.hi[k] > T )
								touch = false;
						if ( touch )
							join( v[a], v[b2] );
					}
			}
		} else {
			/* THE MESH-TO-MESH JOIN, in three steps and no approximation of the
			 * third one.
			 *
			 * THE MEASURE is the one lane IDENTPROX measured and named `mesh`: the
			 * minimum distance between two SAMPLE SETS, each the placed level-0 LOD
			 * vertices PLUS every level-0 triangle's three edge midpoints and its
			 * centroid. It is a point-set measure, so it NEVER reports less than the
			 * true surface-to-surface distance and reports at most that distance
			 * plus the two sample sets' covering radii -- and the covering radius of
			 * {vertices, edge midpoints, centroid} over a triangle is at most half
			 * its longest edge. It therefore joins CONSERVATIVELY: a pair it joins
			 * is genuinely within the gap, and the only error it can make is to
			 * leave a pair apart that a point-to-triangle measure would join. That
			 * is the same arithmetic the Python sweep ran, which is why the two
			 * counts are comparable at all (root MISTAKES: a refuter that shares the
			 * producer's code measures agreement, not correctness -- here the
			 * sharing is a DEFINITION and the two implementations are independent).
			 *
			 * THE COST is one pass over the samples, not a pair walk: every sample
			 * of every eligible placement goes into one uniform grid whose cell IS
			 * the gap, and each sample then looks at its own cell and the 26 around
			 * it. Two placements already in one set cost a `find` and nothing else,
			 * which is what keeps a 205-placement building cheap. */
			QElapsedTimer joinClock;
			joinClock.start();
			const float T = ( s.identityJoinGap > 0.0f ) ? s.identityJoinGap : 64.0f;
			/* One sample set per MESH, in LOCAL units, built on first use: a region
			 * draws a few hundred meshes and tens of thousands of placements. */
			std::unordered_map<quint32, std::vector<float>> meshPts;
			auto samplesOf = [&]( quint16 mid ) -> const std::vector<float> & {
				auto it = meshPts.find( quint32( mid ) );
				if ( it != meshPts.end() )
					return it->second;
				std::vector<float> out;
				const LodoMesh & me = lib.meshes[mid];
				const quint32 c1 = me.clusterFirst + me.clusterCount;
				for ( quint32 cc = me.clusterFirst; cc < c1 && cc < lib.clusters.size(); cc++ ) {
					if ( cc >= lib.clusterLods.size() || lib.clusterLods[cc].level != 0 )
						continue;
					const LodoCluster & cl = lib.clusters[cc];
					float lv[48][3];
					int nv = 0;
					for ( int n = 0; n < int( cl.vertexCount ) && n < 48; n++ ) {
						const size_t vi = size_t( cl.vertexBase ) + size_t( n );
						if ( vi >= lib.vertices.size() )
							break;
						const LodoVertex & vv = lib.vertices[vi];
						for ( int k = 0; k < 3; k++ )
							lv[n][k] = me.aabbMin[k] + float( vv.pos[k] ) / 65535.0f * me.aabbExtent[k];
						out.push_back( lv[n][0] ); out.push_back( lv[n][1] ); out.push_back( lv[n][2] );
						nv++;
					}
					const size_t li = size_t( cc ) * 48;
					for ( int t = 0; t < int( cl.triangleCount ); t++ ) {
						if ( li + size_t( t ) * 3 + 2 >= lib.localIndices.size() )
							break;
						const int ia = lib.localIndices[li + size_t( t ) * 3 + 0];
						const int ib = lib.localIndices[li + size_t( t ) * 3 + 1];
						const int ic = lib.localIndices[li + size_t( t ) * 3 + 2];
						if ( ia >= nv || ib >= nv || ic >= nv )
							continue;
						for ( int k = 0; k < 3; k++ ) out.push_back( 0.5f * ( lv[ia][k] + lv[ib][k] ) );
						for ( int k = 0; k < 3; k++ ) out.push_back( 0.5f * ( lv[ib][k] + lv[ic][k] ) );
						for ( int k = 0; k < 3; k++ ) out.push_back( 0.5f * ( lv[ic][k] + lv[ia][k] ) );
						for ( int k = 0; k < 3; k++ )
							out.push_back( ( lv[ia][k] + lv[ib][k] + lv[ic][k] ) / 3.0f );
					}
				}
				/* A mesh whose level-0 clusters this reader could not walk keeps ONE
				 * sample -- the centre of its own AABB -- rather than none, so it is
				 * still a placement that can join something instead of silently
				 * becoming a singleton for a reason no line of the census states. */
				if ( out.empty() )
					for ( int k = 0; k < 3; k++ )
						out.push_back( me.aabbMin[k] + me.aabbExtent[k] * 0.5f );
				return meshPts.emplace( quint32( mid ), std::move( out ) ).first->second;
			};
			std::vector<float> pts;
			std::vector<quint32> owner;
			for ( quint32 i : arch ) {
				if ( meshOf[i] == LODO_NO_MESH || meshOf[i] >= lib.meshes.size() )
					continue;
				const LodiSrcInstance & r = set.instances[i];
				const std::vector<float> & lp = samplesOf( meshOf[i] );
				pts.reserve( pts.size() + lp.size() );
				owner.reserve( owner.size() + lp.size() / 3 );
				for ( size_t p = 0; p + 2 < lp.size(); p += 3 ) {
					/* The SAME placement arithmetic the box above uses: scale on the
					 * local point, then the row-major rotation, then the position. */
					const float x = lp[p] * r.scale, y = lp[p + 1] * r.scale, z = lp[p + 2] * r.scale;
					pts.push_back( r.rot[0] * x + r.rot[1] * y + r.rot[2] * z + r.pos[0] );
					pts.push_back( r.rot[3] * x + r.rot[4] * y + r.rot[5] * z + r.pos[1] );
					pts.push_back( r.rot[6] * x + r.rot[7] * y + r.rot[8] * z + r.pos[2] );
					owner.push_back( i );
				}
			}
			joinEligible = quint32( arch.size() );
			joinSamples = quint64( owner.size() );
			/* The cell key packs three signed cell indices into 21 bits each. At a
			 * 64-unit cell that is +/- 67 million world units an axis, five hundred
			 * times the widest Bethesda worldspace, so two different cells cannot
			 * share a key and no join can come from an aliased bucket. */
			auto cellOf = [T]( float v ) { return int( std::floor( double( v ) / double( T ) ) ); };
			auto keyOf3 = []( int gx, int gy, int gz ) {
				return ( ( quint64( quint32( gx ) ) & 0x1FFFFFull ) << 42 )
					| ( ( quint64( quint32( gy ) ) & 0x1FFFFFull ) << 21 )
					| ( quint64( quint32( gz ) ) & 0x1FFFFFull );
			};
			std::unordered_map<quint64, std::vector<quint32>> pgrid;
			pgrid.reserve( owner.size() / 4 + 16 );
			for ( size_t p = 0; p < owner.size(); p++ )
				pgrid[keyOf3( cellOf( pts[p * 3] ), cellOf( pts[p * 3 + 1] ), cellOf( pts[p * 3 + 2] ) )]
					.push_back( quint32( p ) );
			const float T2 = T * T;
			for ( size_t p = 0; p < owner.size(); p++ ) {
				const int gx = cellOf( pts[p * 3] ), gy = cellOf( pts[p * 3 + 1] ), gz = cellOf( pts[p * 3 + 2] );
				for ( int dz = -1; dz <= 1; dz++ )
					for ( int dy = -1; dy <= 1; dy++ )
						for ( int dx = -1; dx <= 1; dx++ ) {
							auto cit = pgrid.find( keyOf3( gx + dx, gy + dy, gz + dz ) );
							if ( cit == pgrid.end() )
								continue;
							for ( quint32 q : cit->second ) {
								if ( owner[q] == owner[p] )
									continue;
								if ( find( owner[p] ) == find( owner[q] ) )
									continue;
								const float ex = pts[p * 3] - pts[size_t( q ) * 3];
								const float ey = pts[p * 3 + 1] - pts[size_t( q ) * 3 + 1];
								const float ez = pts[p * 3 + 2] - pts[size_t( q ) * 3 + 2];
								if ( ex * ex + ey * ey + ez * ez <= T2 ) {
									join( owner[p], owner[q] );
									joinPairs++;
								}
							}
						}
			}
			joinMs = joinClock.elapsed();
		}
		/* The key the writer sees. A placement that ended up alone gets the
		 * SENTINEL rather than its own index, so "alone" is a stated state and
		 * not a coincidence of numbering. */
		std::vector<quint32> members( ni, 0 );
		for ( size_t i = 0; i < ni; i++ )
			members[find( quint32( i ) )]++;
		for ( size_t i = 0; i < ni; i++ ) {
			const quint32 root = find( quint32( i ) );
			set.instances[i].groupKey = ( members[root] <= 1 ) ? LODI_GROUP_ALONE : root;
		}
	}
	// a region bake is PARTIAL when its chunks do not span the worldspace's cells
	{
		int w = 0, e = 0, so = 0, no = 0;
		bool first = true;
		for ( const LodiSrcInstance & r : set.instances ) {
			const int cx = lodiChunkOf( r.pos[0] ), cy = lodiChunkOf( r.pos[1] );
			if ( first ) { w = e = cx; so = no = cy; first = false; }
			w = std::min( w, cx ); e = std::max( e, cx ); so = std::min( so, cy ); no = std::max( no, cy );
		}
		const int wsW = minX >> 2, wsE = maxX >> 2, wsS = minY >> 2, wsN = maxY >> 2;
		if ( first || w > wsW || e < wsE || so > wsS || no < wsN )
			set.flags |= LODI_FLAG_PARTIAL;
	}
	/* ---- v4: THE AGGREGATE RING-3 IMPOSTORS, before the .lodi is written.
	 *
	 * One card set per forested cell, composited from the cell's own trees'
	 * cards at the rotation and mirror the repetition breaker gives them
	 * (bungo 08:3x). The rows go into the same LodiSrcSet, so the writer's one
	 * sort law and its own refusals cover them; the SHEETS go back to the
	 * caller, which owns the DDS writer and the frame dilation.
	 *
	 * Unarmed, not one line of this runs and the file is written at version 3. */
	if ( s.aggArmed && !aggTrees.isEmpty() ) {
		QString aggErr;
		if ( !lodgenAggregateBuild( aggTrees, s.aggCards, s.aggOpts, &s.aggSets, &s.aggStats, &aggErr ) )
			return fail( aggErr );
		for ( const LodgenAggSet & a : s.aggSets ) {
			LodiSrcAggregate row;
			for ( int k = 0; k < 3; k++ )
				row.centre[k] = a.centre[k];
			row.half[0] = a.half[0];
			row.half[1] = a.half[1];
			row.depthSpan = a.depthSpan;
			row.boundRadius = a.boundRadius;
			row.cellX = a.cellX;
			row.cellY = a.cellY;
			row.views = quint16( a.views );
			row.flags = quint16( LODI_AGG_HEIGHT | ( a.anyMirrored ? LODI_AGG_MIRRORED : 0 ) );
			row.covered = a.covered;
			set.aggregates.push_back( row );
		}
	}

	g_nativeStage.instances = stageTimer.restart();
	LodiHeader ih;
	LodiWriteStats stats;
	if ( !lodiWrite( lodiPath, set, &ih, &stats, &err ) )
		return fail( err );
	g_nativeStage.lodi = stageTimer.restart();
	{
		QMutexLocker lock( &g_nativeWorkerMutex );
		g_nativeStage.ladderWorkers = g_nativeWorkerIds.size();
	}
	g_nativeStage.armed = true;

	/* THE PER-MESH REPORT. The cache-order and silhouette numbers are per
	 * mesh and there are thousands of them, so they go to a file beside the
	 * pair and the census line carries only the summary.
	 * tests/spells/lodgen_native_fields.py is the gate that reads it. */
	double acmrB = 0.0, acmrA = 0.0, atvrB = 0.0, atvrA = 0.0;
	quint64 triW = 0, srcVerts = 0, emitVerts = 0;
	quint32 opened = 0, acmrWorse = 0;
	for ( const LodoMeshStats & ms : meshStats ) {
		acmrB += double( ms.acmrBefore ) * ms.triangles;
		acmrA += double( ms.acmrAfter ) * ms.triangles;
		atvrB += double( ms.atvrBefore ) * ms.triangles;
		atvrA += double( ms.atvrAfter ) * ms.triangles;
		triW += ms.triangles;
		srcVerts += ms.srcVertices;
		emitVerts += ms.emittedVertices;
		if ( ms.boundaryEmitted > ms.boundarySource )
			opened++;
		if ( ms.acmrAfter > ms.acmrBefore + 1.0e-6f )
			acmrWorse++;
	}

	/* ---- v3 aggregates: the ladder and the occluders, every word WRITTEN and
	 *      MOVING, and every refusal counted by its own name ---- */
	quint32 meshesWithLadder = 0, meshesNoLadder = 0, meshesNoLadderTiny = 0;
	quint32 groupsFormed = 0, gRefSmall = 0, gRefNoCut = 0, gRefFlat = 0, eExact = 0, eBounded = 0;
	quint32 gRefSil = 0;
	// v4 (lane NATIVE1c): the two new refusals, by their own names
	quint32 gRefFoliage = 0, lvRefSil = 0, meshesSilLimited = 0;
	float silWorstAll = 1.0f;
	quint64 weldedV = 0, uvConf = 0;
	quint32 coarserBoundaryGrew = 0;
	float errMaxAll = 0.0f;
	for ( const LodoMeshStats & ms : meshStats ) {
		if ( ms.levelCount > 1 ) {
			meshesWithLadder++;
		} else {
			meshesNoLadder++;
			if ( ms.triangles <= LODO_CLUSTER_MAX_TRIS )
				meshesNoLadderTiny++;
		}
		groupsFormed += ms.groupsFormed;
		gRefSmall += ms.groupsRefusedSmall;
		gRefNoCut += ms.groupsRefusedNoCut;
		gRefFlat += ms.groupsRefusedFlatErr;
		gRefSil += ms.groupsRefusedSilhouette;
		gRefFoliage += ms.groupsRefusedFoliage;
		lvRefSil += ms.levelsRefusedSilhouette;
		if ( ms.levelsRefusedSilhouette )
			meshesSilLimited++;
		silWorstAll = std::min( silWorstAll, ms.silhouetteWorst );
		eExact += ms.errorsExact;
		eBounded += ms.errorsBounded;
		weldedV += ms.weldedVertices;
		uvConf += ms.weldUvConflicts;
		errMaxAll = std::max( errMaxAll, ms.maxError );
		if ( ms.boundaryCoarsest > ms.boundaryEmitted )
			coarserBoundaryGrew++;
	}
	std::vector<quint64> lvClusters( size_t( lh.levelMax ) + 1, 0 ), lvTris( size_t( lh.levelMax ) + 1, 0 );
	std::vector<double> lvErr( size_t( lh.levelMax ) + 1, 0.0 );
	quint64 coneOpen = 0, roots = 0, rootSourceTris = 0;
	for ( size_t i = 0; i < lib.clusters.size(); i++ ) {
		const LodoClusterLod & cl = lib.clusterLods[i];
		if ( size_t( cl.level ) < lvClusters.size() ) {
			lvClusters[cl.level]++;
			lvTris[cl.level] += lib.clusters[i].triangleCount;
			lvErr[cl.level] += double( cl.geometricError );
		}
		if ( lib.clusters[i].flags & LODO_CLUSTER_CONE_OPEN )
			coneOpen++;
		if ( cl.parentFirst == LODO_NO_PARENT ) {
			roots++;
			rootSourceTris += cl.sourceTriangles;
		}
	}
	// the occluder refusals, by reason, over the models that carry a mesh
	quint32 occFit = 0, occNotWatertight = 0, occTooSmall = 0, occNoInterior = 0, occTooThin = 0, occProbeFailed = 0;
	for ( auto it = models.constBegin(); it != models.constEnd(); ++it ) {
		if ( !it.value().loaded )
			continue;
		switch ( it.value().occRefusal ) {
		case NATIVE_OCC_OK: occFit++; break;
		case NATIVE_OCC_TOO_SMALL: occTooSmall++; break;
		case NATIVE_OCC_NO_INTERIOR: occNoInterior++; break;
		case NATIVE_OCC_TOO_THIN: occTooThin++; break;
		case NATIVE_OCC_PROBE_FAILED: occProbeFailed++; break;
		default: occNotWatertight++; break;
		}
	}
	/* THE PER-SOURCE CASTER COUNTS (bungo 2026-09-11 14:4x, "the census counts
	 * casters per source"; FO4CS plan section 5 row 17).  Every LOD object is a
	 * shadow caster (NATIVE 3.4) and his ruling is that a placement has exactly
	 * ONE shadow representation at a time -- so these four are a PARTITION of the
	 * instance table, in this priority, and the line states the sum law and
	 * whether it holds rather than leaving a reader to add it up:
	 *   tree   base carries LODO_BASE_TREE (its far caster is the tree rep);
	 *   card   no mesh at all, but a card layer or the force-card flag;
	 *   mesh   a mesh rep and not a tree;
	 *   none   no representation of any kind -- a caster that casts nothing.
	 * `meshSlotCasters` is the same count at MESH granularity: each instance
	 * counted once per DISTINCT mesh its base names, which is exactly what the
	 * sidecar's `casterInstances` column sums to. */
	quint64 castTree = 0, castCard = 0, castMesh = 0, castNone = 0, castMeshSlots = 0;
	std::vector<quint64> meshCasters( lib.meshes.size(), 0 );
	for ( const LodiSrcInstance & r : set.instances ) {
		const LodoBase * bp = ( size_t( r.baseId ) < lib.bases.size() ) ? &lib.bases[r.baseId] : nullptr;
		quint16 distinct[4];
		int nd = 0;
		if ( bp ) {
			for ( int k = 0; k < 4; k++ ) {
				if ( bp->rep[k] == LODO_NO_MESH )
					continue;
				bool seen = false;
				for ( int j = 0; j < nd; j++ )
					seen = seen || distinct[j] == bp->rep[k];
				if ( !seen )
					distinct[nd++] = bp->rep[k];
			}
			for ( int j = 0; j < nd; j++ )
				if ( size_t( distinct[j] ) < meshCasters.size() )
					meshCasters[distinct[j]]++;
			castMeshSlots += quint64( nd );
		}
		const bool isTree = bp && ( bp->flags & LODO_BASE_TREE );
		const bool hasMesh = nd > 0;
		const bool hasCard = ( r.flags & LODI_INST_FORCE_CARD )
			|| ( bp && bp->cardLayer != LODO_NO_CARD );
		if ( isTree )
			castTree++;
		else if ( !hasMesh && hasCard )
			castCard++;
		else if ( hasMesh )
			castMesh++;
		else
			castNone++;
	}

	if ( !s.meshReport.isEmpty() ) {
		QFile mf( s.meshReport );
		if ( !mf.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			return fail( QString( "cannot write the mesh report %1" ).arg( s.meshReport ) );
		QByteArray outBytes;
		/* Report version 4.  Every new column is inserted BEFORE `model`,
		 * because the model path is the only token that may hold a space and
		 * must stay the line's remainder.  v3 added the thirteen ladder
		 * columns; v4 adds `casterInstances`, the per-source caster count at
		 * mesh granularity (bungo 2026-09-11 14:4x); v6 (2026-09-18) adds
		 * `selfAoMean` and `selfAoDark`, the cast self-occlusion the library
		 * vertices now carry (bungo: "we needed vertex AO bakes for the lod
		 * objects"). */
		outBytes += "# lodgen native mesh report 6 ws ";
		outBytes += world.worldspaceEdid().toUtf8();
		outBytes += " columns meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter "
			"boundarySrc boundaryEmitted levels clustersL0 clustersLadder maxError groupsFormed refSmall "
			"refNoCut refFlat refSilhouette errExact errBounded weldedVerts uvConflicts boundaryCoarsest "
			"casterInstances refFoliage refLevelSilhouette silhouetteWorst selfAoMean selfAoDark model\n";
		for ( size_t i = 0; i < meshStats.size(); i++ ) {
			const LodoMeshStats & ms = meshStats[i];
			outBytes += QString( "%1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14 %15 %16 %17 %18 %19 %20 %21 %22 %23 %24 %25 %26 %27 %28 %29 %30 %31\n" )
				.arg( i ).arg( ms.triangles ).arg( ms.srcVertices ).arg( ms.emittedVertices )
				.arg( double( ms.acmrBefore ), 0, 'f', 5 ).arg( double( ms.acmrAfter ), 0, 'f', 5 )
				.arg( double( ms.atvrBefore ), 0, 'f', 5 ).arg( double( ms.atvrAfter ), 0, 'f', 5 )
				.arg( ms.boundarySource ).arg( ms.boundaryEmitted )
				.arg( ms.levelCount ).arg( ms.clustersL0 ).arg( ms.clustersLadder )
				.arg( double( ms.maxError ), 0, 'f', 5 )
				.arg( ms.groupsFormed ).arg( ms.groupsRefusedSmall ).arg( ms.groupsRefusedNoCut )
				.arg( ms.groupsRefusedFlatErr ).arg( ms.groupsRefusedSilhouette )
				.arg( ms.errorsExact ).arg( ms.errorsBounded )
				.arg( ms.weldedVertices ).arg( ms.weldUvConflicts ).arg( ms.boundaryCoarsest )
				.arg( i < meshCasters.size() ? meshCasters[i] : quint64( 0 ) )
				.arg( ms.groupsRefusedFoliage ).arg( ms.levelsRefusedSilhouette )
				.arg( double( ms.silhouetteWorst ), 0, 'f', 5 )
				.arg( double( ms.selfAoMean ), 0, 'f', 4 ).arg( ms.selfAoDark )
				.arg( meshStatPath[i] ).toUtf8();
		}
		if ( mf.write( outBytes ) != outBytes.size() )
			return fail( QString( "short write to %1" ).arg( s.meshReport ) );
		mf.close();
	}

	if ( report ) {
		quint64 tris = 0;
		for ( const LodoCluster & c : lib.clusters )
			tris += c.triangleCount;
		QString line = QString( "native: %1.lodo %2 bytes (bases %3 of %4 in the census, %5 without a loadable model; "
			"models %6 loaded %7 failed; meshes %8 clusters %9 triangles %10 vertices %11 materials %12); "
			"%1.lodi %13 bytes (instances %14 from %15 arrivals over %16 census refs, %17 dropped for a base outside the table, "
			"%18 unlit, %26 without a stock identity; chunks %19 present of %20 dense, max %21 a chunk, "
			"max scale %22, max baseId %23%24); %25 B a placement; "
			"cacheOrder acmr %27 -> %28, overfetch %29 -> %30, verts %31 -> %32; "
			"silhouette %33 meshes, %34 opened, %35 read worse after the reorder" )
			.arg( ws ).arg( lh.fileBytes ).arg( basesWritten ).arg( baseIds.size() ).arg( basesWithoutMesh )
			.arg( modelsLoaded ).arg( modelsFailed ).arg( lh.meshCount ).arg( lh.clusterCount ).arg( tris ).arg( lh.vertexCount ).arg( lh.materialCount )
			.arg( ih.fileBytes ).arg( stats.instances ).arg( s.arrivalsSeen ).arg( censusRefs ).arg( droppedNoBase ).arg( unlit )
			.arg( stats.presentChunks ).arg( stats.chunks ).arg( stats.maxInstancesPerChunk ).arg( double( stats.maxScale ), 0, 'f', 4 ).arg( stats.maxBaseId )
			.arg( ( set.flags & LODI_FLAG_PARTIAL ) ? QStringLiteral( ", PARTIAL" ) : QString() )
			.arg( stats.instances ? double( lh.fileBytes + ih.fileBytes ) / double( stats.instances ) : 0.0, 0, 'f', 1 )
			.arg( noIdentity )
			.arg( triW ? acmrB / double( triW ) : 0.0, 0, 'f', 4 ).arg( triW ? acmrA / double( triW ) : 0.0, 0, 'f', 4 )
			.arg( triW ? atvrB / double( triW ) : 0.0, 0, 'f', 4 ).arg( triW ? atvrA / double( triW ) : 0.0, 0, 'f', 4 )
			.arg( srcVerts ).arg( emitVerts )
			.arg( meshStats.size() ).arg( opened ).arg( acmrWorse );
		if ( !failedModels.isEmpty() )
			line += QStringLiteral( "; failed models: " ) + failedModels.join( QStringLiteral( ", " ) );

		/* The v3 rows go on their OWN lines rather than into the sentence above:
		 * the census line is already at the edge of readable, and a reader
		 * greps one prefix a subject. Each row names its serving arm and counts
		 * its refusals, so a zero can never be mistaken for "nothing to say". */
		QString ladderLine = QString( "native-ladder: %1 (group %2, exact-error budget %3 tris); levels 0..%4; "
			"meshes %5 with a ladder, %6 without (%7 of those are <= %8 triangles); groups %9 formed, "
			"refused %10 too small, %11 no triangle removed, %12 error did not grow, %22 opened a silhouette; "
			"errors %13 measured against full detail, %14 chain-bounded; welded verts %15, uv conflicts %16; "
			"maxError %17; coneOpen %18 clusters; roots %19 covering %20 full-detail triangles; "
			"coarsest level opened a silhouette on %21 meshes" )
			.arg( s.ladder ? QStringLiteral( "ON" ) : QStringLiteral( "OFF (--native-no-ladder)" ) )
			.arg( LODO_LADDER_GROUP ).arg( LODO_ERROR_EXACT_TRIS ).arg( lh.levelMax )
			.arg( meshesWithLadder ).arg( meshesNoLadder ).arg( meshesNoLadderTiny ).arg( LODO_CLUSTER_MAX_TRIS )
			.arg( groupsFormed ).arg( gRefSmall ).arg( gRefNoCut ).arg( gRefFlat )
			.arg( eExact ).arg( eBounded ).arg( weldedV ).arg( uvConf )
			.arg( double( errMaxAll ), 0, 'f', 4 ).arg( coneOpen ).arg( roots ).arg( rootSourceTris )
			.arg( coarserBoundaryGrew ).arg( gRefSil );
		for ( size_t lv = 0; lv < lvClusters.size(); lv++ )
			ladderLine += QString( "; level %1 clusters %2 triangles %3 meanError %4" )
				.arg( lv ).arg( lvClusters[lv] ).arg( lvTris[lv] )
				.arg( lvClusters[lv] ? lvErr[lv] / double( lvClusters[lv] ) : 0.0, 0, 'f', 4 );
		/* v4 (lane NATIVE1c), on its own prefix so a region without a single
		 * tree reads a plain 0 and a region full of them does not: the foliage
		 * refusal, the silhouette-fraction gate, and where level 0 came from. */
		ladderLine += QString( "\n  native-ladder-refused: foliage %1 clusters (alpha-tested tree material; "
			"%2 = --native-ladder-foliage turns the refusal off); silhouette %3 levels on %4 meshes "
			"(floor %5 of level 0's horizon outline, %6 views x %7 px; worst kept fraction %8)" )
			.arg( gRefFoliage )
			.arg( s.ladderFoliage ? QStringLiteral( "ON, laddering them" ) : QStringLiteral( "OFF" ) )
			.arg( lvRefSil ).arg( meshesSilLimited )
			.arg( double( s.silhouetteMin ), 0, 'f', 2 )
			.arg( LODO_SILHOUETTE_VIEWS ).arg( LODO_SILHOUETTE_GRID )
			.arg( double( silWorstAll ), 0, 'f', 4 );
		ladderLine += QString( "\n  native-library: level 0 from %1; %2 bases had no near MODL and kept their MNAM slots in place; "
			"placement AO %3" )
			.arg( s.libraryNear ? QStringLiteral( "the base's near MODL (--library near)" )
				: QStringLiteral( "the base's MNAM LOD slots (--library mnam)" ) )
			.arg( basesWithoutNear )
			.arg( s.placementAo ? QString( "ON: %1 measured, %2 not measured (0x%3)" )
				.arg( int( stats.instances ) - paoUnmeasured ).arg( paoUnmeasured )
				.arg( LODI_PLACEMENT_AO_UNMEASURED, 2, 16, QChar( '0' ) )
				: QStringLiteral( "OFF (--native-no-placement-ao)" ) );
		ladderLine += QString( "; vertex AO %1" )
			.arg( s.vertexAo && s.placementAo ? QString( "ON: %1 instances streamed (%2 bytes, mean %3, %4 below 128), %5 empty, %6 chunk(s) cast, %7 without land" )
				.arg( vaoInstances ).arg( vaoBytes ).arg( vaoBytes ? vaoSum / double( vaoBytes ) : 0.0, 0, 'f', 1 ).arg( vaoDark )
				.arg( vaoEmpty ).arg( vaoChunks ).arg( vaoNoLand )
				: QStringLiteral( "OFF (--native-no-vertex-ao)" ) );
		/* v7 (2026-09-18, lane LODIV7). Both halves state their OFF value by
		 * the switch that turns them off, so a reader of the census never has
		 * to guess whether a zero means "measured none" or "never ran". */
		ladderLine += QString( "; vertex sky %1" )
			.arg( s.lodiV7 && s.vertexAo && s.placementAo
				? QString( "ON: %1 placements streamed (%2 bytes, mean %3, %4 at or above 128)" )
					.arg( vskyInstances ).arg( vskyBytes )
					.arg( vskyBytes ? vskySum / double( vskyBytes ) : 0.0, 0, 'f', 1 ).arg( vskyOpen )
				: QStringLiteral( "OFF (--lodi-v6)" ) );
		ladderLine += QString( "; groups %1" )
			.arg( s.lodiV7
				? QString( "%1 over %2 placements (%3 grouped, largest %4, %5 singleton), %6 placement(s) whose BASE model path has a `%7` component" )
					.arg( stats.groups ).arg( stats.instances ).arg( stats.groupedPlacements )
					.arg( stats.largestGroup ).arg( stats.singletonGroups ).arg( archPlacements )
					.arg( QLatin1String( KNOB.archComponent ) )
				: QStringLiteral( "OFF (--lodi-v6)" ) );
		/* v7 grouping, THE JOIN RULE, on its own words and with its cost, because
		 * the rule is bungo's ruling of 2026-09-19 and a bake that silently ran
		 * the other one would be indistinguishable from a bake that ran this one
		 * badly. The gate reads `groups` above against lane IDENTPROX's Python
		 * count (chunk 4.4.-12: 588 under `legacy`, 167 under the ruled rule) and
		 * the legacy switch is its red control. */
		ladderLine += QString( "\n  native-identity-join: %1" )
			.arg( !s.lodiV7
				? QStringLiteral( "OFF (--lodi-v6); no group table is written" )
				: ( s.identityJoinLegacy
					? QString( "LEGACY (--identity-join legacy): the pre-2026-09-19 rule -- an `%1` "
						"path component and a WORLD AXIS-ALIGNED BOX gap of %2 u. %3 eligible placement(s)." )
						.arg( QLatin1String( KNOB.archComponent ) )
						.arg( double( KNOB.touchTolerance ), 0, 'f', 1 ).arg( archPlacements )
					: QString( "PROXIMITY (the default; --identity-join legacy is the way back): every "
						"NON-TREE placement with a drawn LOD mesh, MESH-TO-MESH gap %1 u "
						"(--identity-join-gap). %2 eligible placement(s), %L3 mesh sample point(s) "
						"(level-0 vertices + edge midpoints + centroids, placed), %L4 sample pair(s) "
						"inside the gap, %5 ms. The measure never reports LESS than the true "
						"surface distance, so every join it made is real." )
						.arg( double( s.identityJoinGap ), 0, 'f', 1 ).arg( joinEligible )
						.arg( joinSamples ).arg( joinPairs ).arg( joinMs ) ) );
		/* v10 (lane BAKE2, 2026-09-25): placements above the old 7.99988 line,
		 * written with instance flag bit 7. 0 = the file is the v7/v9 file it
		 * always was; any other number = version 10. */
		ladderLine += QString( "\n  native-wide-scale: %1 of %2 placements above %3 (flag bit 7); max scale %4; "
			".lodi version %5" )
			.arg( stats.wideScaleInstances ).arg( stats.instances ).arg( double( LODI_SCALE_MAX ), 0, 'f', 5 )
			.arg( double( stats.maxScale ), 0, 'f', 4 ).arg( stats.version );
		/* v9 (lane HORIZON3, 2026-09-19), on its own prefix. THE GATE NUMBER IS
		 * 14: that is what the three-clause rule counted on the measured urban
		 * region's 33,123 placements (0.04%), read out of Fallout4.esm by
		 * `scratchpad/horizon3_20260919/scrap_rule.py` and re-derived from the
		 * written file, without calling any of this code, by
		 * `tests/spells/lodgen_scrappable.sh`. A bake whose number is not
		 * 14 on that region has changed the rule, whether or not anyone meant
		 * to. The index's own counts are printed beside it so a disagreement
		 * says WHICH clause moved instead of only that one did. */
		static const EsmScrapIndex kScrapNever;     //!< what an OFF bake reports: nothing
		const EsmScrapIndex & sx = ( s.scrappable && s.world ) ? s.world->scrapIndex() : kScrapNever;
		ladderLine += QString( "\n  native-scrappable: %1" )
			.arg( s.scrappable
				? QString( "ON: %1 of %2 placements (%3 percent); clause 1 %4 scrap recipes over %5 COBJ "
					"(%6 FormLists expanded) -> %7 bases; clause 2 %8 build areas from %9 primitives "
					"(%10 not a box, %11 not linked to a workshop, %12 workshop REFRs, %13 tilted); "
					"clause 3 %14 bases refuse to be scrapped, %15 of them in clause 1; "
					"scrappablePlacements %16" )
					.arg( scrappablePlacements ).arg( stats.instances )
					.arg( stats.instances ? 100.0 * double( scrappablePlacements ) / double( stats.instances ) : 0.0,
						0, 'f', 2 )
					.arg( sx.cobjScrapRecipes ).arg( sx.cobjRecords ).arg( sx.formListsExpanded )
					.arg( sx.scrapBases.size() )
					.arg( sx.buildAreas.size() ).arg( sx.primitivesSeen ).arg( sx.primitivesNotBox )
					.arg( sx.primitivesUnlinked ).arg( sx.workshopRefrs ).arg( sx.tiltedAreas )
					.arg( sx.unscrappable.size() ).arg( sx.clause1And3 )
					//! the word the gate greps for; 14 on the measured urban region
					.arg( scrappablePlacements )
				: QStringLiteral( "OFF (--scrappable, the default); no bit is written and the .lodi "
					"stays at version 7, byte for byte; scrappablePlacements 0" ) );
		/* THE CARD LINK (lane CARDLINK1, 2026-09-24), on its own prefix. OFF is
		 * every bake that did not call `lodgenNativeLinkCards`: cardLayer 0xFFFF,
		 * cardCount 0, cardCorpusHash 0 and no FORCE_CARD bit, byte for byte
		 * what the emitter wrote before the lane. */
		{
			quint32 cardBases = 0;
			for ( const LodoBase & b : lib.bases )
				if ( b.cardLayer != LODO_NO_CARD )
					cardBases++;
			int cardOutside = 0;
			for ( auto it = s.cardLayerOf.constBegin(); it != s.cardLayerOf.constEnd(); ++it )
				if ( !baseRow.contains( it.key() ) )
					cardOutside++;
			ladderLine += QString( "\n  native-cards: %1" )
				.arg( s.cardsLinked
					? QString( "LINKED: cardCount %1 of %2 bases over %3 array(s) of %4 layer(s), cardCorpusHash 0x%5; "
						"card-only bases %6; card sets whose base is not in the table %7; FORCE_CARD on %8 of %9 "
						"instances (%10 whose ring slot has no mesh, %11 on a card by a C line); C lines %12 read, "
						"%13 linked" )
						.arg( cardBases ).arg( lib.bases.size() ).arg( s.cardArrays ).arg( s.cardLayers )
						.arg( lib.cardCorpusHash, 16, 16, QChar( '0' ) )
						.arg( cardOnlyBases ).arg( cardOutside )
						.arg( forcedCard ).arg( stats.instances ).arg( forcedEmptySlot ).arg( forcedByLine )
						.arg( s.cardLines ).arg( s.cardLinesLinked )
					: QString( "OFF (no card arrays linked); cardCount %1, cardCorpusHash 0x%2, FORCE_CARD on %3 instances" )
						.arg( cardBases ).arg( lib.cardCorpusHash, 16, 16, QChar( '0' ) ).arg( forcedCard ) );
		}
		/* THE PER-SOURCE CASTER COUNTS, on their own prefix.  The four are a
		 * PARTITION of the instance table (bungo 2026-09-11 14:4x: "every
		 * placement has exactly ONE shadow representation at a time"), so the
		 * line states the sum law and whether it holds, rather than leaving a
		 * reader to add four numbers up and guess.  `none` is a fault word:
		 * an instance with no representation of any kind casts nothing. */
		QString castLine = QString( "native-casters: per source, of %1 instances: "
			"tree %2, card %3, mesh %4, none %5; sum %6 == instances %7 == %8; "
			"mesh-slot casters %9 over %10 meshes (each instance once per distinct mesh its base names); "
			"terrain march: runtime, FO4CS measures it" )
			.arg( stats.instances ).arg( castTree ).arg( castCard ).arg( castMesh ).arg( castNone )
			.arg( castTree + castCard + castMesh + castNone ).arg( stats.instances )
			.arg( ( castTree + castCard + castMesh + castNone ) == quint64( stats.instances )
				? QStringLiteral( "AGREE" ) : QStringLiteral( "DISAGREE" ) )
			.arg( castMeshSlots ).arg( lib.meshes.size() );
		QString occLine = QString( "native-occluders: %1; boxes %2 written from %3 offered, %4 dropped (cap %5 a cell); "
			"cells %6 of %7 populated have one (%8 percent), %9 with none; "
			"models fitted %10, refused %11 not watertight, %12 too small (< %13 u diagonal), "
			"%14 no interior voxel, %15 too thin (< %16 percent of the box), %17 failed the %18-point probe" )
			.arg( s.occluders ? QStringLiteral( "ON" ) : QStringLiteral( "OFF (--native-no-occluders)" ) )
			.arg( stats.occluders ).arg( stats.occluderCandidates ).arg( stats.occludersDropped ).arg( LODI_OCCLUDERS_PER_CELL )
			.arg( stats.cellsWithOccluder ).arg( stats.cellsPopulated )
			.arg( stats.cellsPopulated ? 100.0 * double( stats.cellsWithOccluder ) / double( stats.cellsPopulated ) : 0.0, 0, 'f', 1 )
			.arg( stats.cellsPopulated - stats.cellsWithOccluder )
			.arg( occFit ).arg( occNotWatertight ).arg( occTooSmall ).arg( double( NATIVE_OCC_MIN_DIAG ), 0, 'f', 0 )
			.arg( occNoInterior ).arg( occTooThin ).arg( double( NATIVE_OCC_MIN_FILL * 100.0f ), 0, 'f', 0 )
			.arg( occProbeFailed ).arg( NATIVE_OCC_PROBES );
		quint64 aggCovered = 0, aggTexels = 0;
		int hSpanMin = 0, hSpanMax = 0;
		double hSpanSum = 0.0;
		for ( int i = 0; i < s.aggSets.size(); i++ ) {
			const LodgenAggSet & a = s.aggSets[i];
			aggCovered += a.covered.size();
			aggTexels += quint64( a.views ) * a.frameW * a.frameH;
			const int hs = a.heightMax - a.heightMin;
			hSpanMin = i ? std::min( hSpanMin, hs ) : hs;
			hSpanMax = std::max( hSpanMax, hs );
			hSpanSum += hs;
		}
		QString aggLine = QString( "native-aggregate: %1; cells %2 seen with trees, %3 forested at >= %4, "
			"%5 aggregated, %6 refused because every one of their trees was; trees %7 photographed, "
			"refused %8 with no card set, %9 baked through a perspective camera, %10 whose sheet would not load; "
			"views %11 azimuths, tile %12 px; sheet texels %13; height channel span min %14 max %15 mean %16 of 255" )
			.arg( s.aggArmed ? QStringLiteral( "ON" ) : QStringLiteral( "OFF (--aggregate turns it on; off is byte-identical)" ) )
			.arg( s.aggStats.cellsSeen ).arg( s.aggStats.cellsForested ).arg( s.aggOpts.minTrees )
			.arg( s.aggStats.cellsAggregated ).arg( s.aggStats.cellsRefusedAllTreesLost )
			.arg( s.aggStats.treesPhotographed ).arg( s.aggStats.treesRefusedNoCard )
			.arg( s.aggStats.treesRefusedNotOrtho ).arg( s.aggStats.treesRefusedNoImage )
			.arg( s.aggOpts.views ).arg( s.aggOpts.tile ).arg( aggTexels )
			.arg( hSpanMin ).arg( hSpanMax )
			.arg( s.aggSets.isEmpty() ? 0.0 : hSpanSum / double( s.aggSets.size() ), 0, 'f', 1 );
		aggLine += QString( "; count identity photographed %1 == file covered %2 == %3" )
			.arg( s.aggStats.treesPhotographed ).arg( stats.coveredInstances )
			.arg( ( quint64( s.aggStats.treesPhotographed ) == aggCovered
				&& aggCovered == quint64( stats.coveredInstances ) )
				? QStringLiteral( "AGREE" ) : QStringLiteral( "DISAGREE" ) );
		aggLine += QString( "; .lodi version %1, rows %2, switchPx %3, band x%4" )
			.arg( stats.version ).arg( stats.aggregates )
			.arg( double( ih.aggSwitchPx ), 0, 'f', 1 ).arg( double( ih.aggBandRatio ), 0, 'f', 2 );
		for ( const QString & r : s.aggStats.refusals )
			aggLine += QStringLiteral( "\n  native-aggregate refused: " ) + r;
		/* `native-library-build:` -- its OWN census keyword, registered in
		 * g_censusKeywords[] (src/lodbfile.cpp) and in the gate's copy of the
		 * list (tests/spells/lodgen_bakerec_gate.py). It is NOT hung off the
		 * existing `native-library:`, which already carries where level 0 came
		 * from: every keyword in that list appears exactly once in a record,
		 * and a reader greps by keyword. So the record carries this without a
		 * new FIELD, and `ww-census-contract` reads it. It says REUSED or
		 * REBUILT and, when rebuilt, the ONE reason that decided it -- first
		 * refusal wins, in the order the chain tests them. */
		const QString libLine = libraryReused
			? QStringLiteral( "native-library-build: reused (previous .lodo kept; base census, load order, "
				"plugin corpus and object corpus unmoved, payload checked)" )
			: QStringLiteral( "native-library-build: rebuilt (" ) + libraryWhy + QChar( ')' );
		*report = line + QChar( '\n' ) + ladderLine + QChar( '\n' ) + occLine
			+ QChar( '\n' ) + castLine
			+ QChar( '\n' ) + aggLine
			+ QChar( '\n' ) + libLine;
	}
	return true;
}

/* ---------------------------------------------------------------- verify */

/*! NAME THE PLUGIN, not just the hash (lane BAKEREC1, 2026-09-17).
 *
 *  bungo, 2026-09-16: "each bake needs to know plugins used or what's
 *  different". A `loadOrderHash` or `pluginCorpusHash` mismatch says a bake is
 *  stale and says nothing about WHICH of thirty plugins did it -- the hash is a
 *  fold over the whole list and cannot be un-folded. The bake record beside the
 *  pair carries the list itself, so when one is there the refusal names the
 *  file, its index, and whether it was added, removed, reordered, resized or
 *  EDITED IN PLACE at the same size -- the last of which the load-order hash
 *  cannot see at all.
 *
 *  Without a record the message is exactly what it was, and says so. */
static QString lodbNameTheStalePlugin( const QString & lodoPath, const EsmWorld * world )
{
	if ( !world )
		return QString();
	const QString rec = lodbPathBeside( lodoPath );
	if ( rec.isEmpty() )
		return QStringLiteral( "\n  (no .lodb bake record beside this pair, so the plugin that moved "
			"cannot be named -- re-bake once with this build and it will be)" );
	QVector<LodbPlugin> recorded;
	QString rerr;
	if ( !lodbReadPlugins( rec, &recorded, &rerr ) )
		return QStringLiteral( "\n  (the bake record beside this pair could not be read: " ) + rerr + QChar( ')' );
	const QStringList moved = lodbDiffPlugins( recorded, world->pluginList() );
	if ( moved.isEmpty() )
		return QStringLiteral( "\n  (the bake record lists " ) + QString::number( recorded.size() )
			+ QStringLiteral( " plugin(s) and NONE of them moved -- so the staleness is in the "
				"records, not in the load order)" );
	QString s = QStringLiteral( "\n  the bake record names what moved:" );
	for ( const QString & m : moved )
		s += QStringLiteral( "\n    " ) + m;
	return s;
}

bool lodgenNativeVerify( const QString & lodoPath, const QString & lodiPath, QString * report, QString * error,
	const EsmWorld * world )
{
	auto fail = [&]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	LodoHeader lh;
	LodoLibrary lib;
	QString err;
	if ( !lodoRead( lodoPath, &lh, &lib, true, &err ) )
		return fail( err );
	LodiHeader ih;
	LodiTable tab;
	if ( !lodiRead( lodiPath, &ih, &tab, true, &err ) )
		return fail( err );
	QStringList out;
	for ( const QString & l : lodoDescribe( lh, &lib ) )
		out << QStringLiteral( "lodo " ) + l;
	for ( const QString & l : lodiDescribe( ih, &tab ) )
		out << QStringLiteral( "lodi " ) + l;
	if ( ih.worldspaceEdid != lh.worldspaceEdid )
		return fail( QString( "pairing: .lodi is for %1, .lodo for %2" ).arg( ih.worldspaceEdid, lh.worldspaceEdid ) );
	if ( ih.pluginCorpusHash != lh.pluginCorpusHash )
		return fail( QStringLiteral( "pairing: pluginCorpusHash differs between the two files" ) );
	if ( ih.objectCorpusHash != lh.objectCorpusHash )
		return fail( QStringLiteral( "pairing: objectCorpusHash differs between the two files" ) );
	if ( ih.loadOrderHash != lh.loadOrderHash )
		return fail( QString( "pairing: loadOrderHash 0x%1 in %2 but 0x%3 in %4" )
			.arg( ih.loadOrderHash, 16, 16, QChar( '0' ) ).arg( lodiPath )
			.arg( lh.loadOrderHash, 16, 16, QChar( '0' ) ).arg( lodoPath ) );
	const quint64 want = lodoIdentityOf( lh.headerCrc32, lh.modelCorpusHash, lh.objectCorpusHash );
	if ( !( ih.flags & LODI_FLAG_NOLIB ) && ih.lodoIdentity != want )
		return fail( QString( "pairing: lodoIdentity 0x%1 does not name this .lodo (0x%2)" )
			.arg( ih.lodoIdentity, 16, 16, QChar( '0' ) ).arg( want, 16, 16, QChar( '0' ) ) );

	/* THE STALENESS CHECK, when the caller handed us the plugin. Each of the
	 * three is recomputed and refused BY NAME, naming the file, so the reason a
	 * bake is rejected is never "something changed". */
	if ( world ) {
		quint64 objHash = 0, censusRefs = 0;
		if ( !nativeObjectCensus( *world, nullptr, &objHash, &censusRefs, error ) )
			return false;
		const quint64 plugHash = world->vhgtCorpusHash();
		const quint64 loadHash = world->loadOrderHash();
		/* An EDITED plugin whose edit reached no record this pair carries
		 * leaves every hash equal, and that is CORRECT -- the pair is not
		 * stale. The record can still see it, so the census says so rather
		 * than staying silent about a file that changed under the bake. */
		{
			const QString rec = lodbPathBeside( lodoPath );
			QVector<LodbPlugin> recorded;
			QString rerr;
			if ( !rec.isEmpty() && lodbReadPlugins( rec, &recorded, &rerr ) ) {
				const QStringList moved = lodbDiffPlugins( recorded, world->pluginList() );
				out << QString( "bake record %1 plugin(s), %2 moved since the bake" )
					.arg( recorded.size() ).arg( moved.size() );
				for ( const QString & m : moved )
					out << QStringLiteral( "bake record: " ) + m;
			} else {
				out << QStringLiteral( "bake record none beside this pair" );
			}
		}
		out << QString( "recomputed objectCorpusHash 0x%1" ).arg( objHash, 16, 16, QChar( '0' ) )
			<< QString( "recomputed pluginCorpusHash 0x%1" ).arg( plugHash, 16, 16, QChar( '0' ) )
			<< QString( "recomputed loadOrderHash 0x%1" ).arg( loadHash, 16, 16, QChar( '0' ) )
			<< QString( "recomputed over %1 census refs of %2" ).arg( censusRefs ).arg( world->pluginList() );
		if ( objHash != lh.objectCorpusHash )
			return fail( QString( "%1 is STALE: objectCorpusHash 0x%2 in the file, 0x%3 from %4 -- a placement, a "
				"base's MNAM or a SCOL part changed since the bake" )
				.arg( lodoPath ).arg( lh.objectCorpusHash, 16, 16, QChar( '0' ) ).arg( objHash, 16, 16, QChar( '0' ) )
				.arg( world->pluginList() )
				/* ...and NAME the plugin, as the other two staleness branches below do.
				 * This is the branch an edited REFR takes -- the common case, and
				 * the one `lodgen_bakerec.sh` leg (e) exercises -- and it was the
				 * one of the three that did not call it (lane INCR1, 2026-09-17). */
				+ lodbNameTheStalePlugin( lodoPath, world ) );
		if ( plugHash != lh.pluginCorpusHash )
			return fail( QString( "%1 is STALE: pluginCorpusHash 0x%2 in the file, 0x%3 from %4 -- the terrain changed" )
				.arg( lodoPath ).arg( lh.pluginCorpusHash, 16, 16, QChar( '0' ) ).arg( plugHash, 16, 16, QChar( '0' ) )
				.arg( world->pluginList() ) + lodbNameTheStalePlugin( lodoPath, world ) );
		if ( loadHash != lh.loadOrderHash )
			return fail( QString( "%1 is STALE: loadOrderHash 0x%2 in the file, 0x%3 from %4 -- a plugin was added, "
				"removed, reordered or resized since the bake" )
				.arg( lodoPath ).arg( lh.loadOrderHash, 16, 16, QChar( '0' ) ).arg( loadHash, 16, 16, QChar( '0' ) )
				.arg( world->pluginList() ) + lodbNameTheStalePlugin( lodoPath, world ) );
	}
	/* The drawKey law, checked against the library the record points at: the
	 * rank of the base's (primary mesh, first material) pair among every base
	 * in this .lodo. The .lodi reader alone can only see that the ranks are
	 * ordered; THIS is where the rank is checked to be the right number. */
	std::vector<quint16> wantKey( lib.bases.size(), 0 );
	{
		std::vector<std::pair<quint16, quint16>> pairs( lib.bases.size() );
		for ( size_t i = 0; i < lib.bases.size(); i++ ) {
			quint16 mid = LODO_NO_MESH;
			for ( int k = 0; k < 4 && mid == LODO_NO_MESH; k++ )
				mid = lib.bases[i].rep[k];
			quint16 mat = 0xFFFF;
			if ( mid != LODO_NO_MESH && mid < lib.meshes.size() ) {
				const LodoMesh & m = lib.meshes[mid];
				if ( m.clusterCount && m.clusterFirst < lib.clusters.size() )
					mat = lib.clusters[m.clusterFirst].materialId;
			}
			pairs[i] = std::make_pair( mid, mat );
		}
		std::vector<std::pair<quint16, quint16>> distinct = pairs;
		std::sort( distinct.begin(), distinct.end() );
		distinct.erase( std::unique( distinct.begin(), distinct.end() ), distinct.end() );
		for ( size_t i = 0; i < pairs.size(); i++ )
			wantKey[i] = quint16( std::lower_bound( distinct.begin(), distinct.end(), pairs[i] ) - distinct.begin() );
	}
	quint32 withGeometry = 0, noGeometryNoCard = 0;
	QSet<quint16> identitySeen;
	for ( size_t i = 0; i < tab.instances.size(); i++ ) {
		const LodiInstance & r = tab.instances[i];
		if ( r.baseId >= lh.baseCount )
			return fail( QString( "instance %1: baseId %2 past baseCount %3" ).arg( i ).arg( r.baseId ).arg( lh.baseCount ) );
		if ( r.drawKey != wantKey[r.baseId] )
			return fail( QString( "instance %1 (ref 0x%2): drawKey %3 but base %4's (mesh, material) rank is %5" )
				.arg( i ).arg( tab.cold[i].refFormId, 8, 16, QChar( '0' ) ).arg( r.drawKey ).arg( r.baseId ).arg( wantKey[r.baseId] ) );
		identitySeen.insert( tab.cold[i].identity );
		const LodoBase & b = lib.bases[r.baseId];
		bool geometry = false;
		for ( int k = 0; k < 4 && !geometry; k++ )
			if ( b.rep[k] != LODO_NO_MESH ) {
				const LodoMesh & m = lib.meshes[b.rep[k]];
				for ( quint32 c = m.clusterFirst; c < m.clusterFirst + m.clusterCount && !geometry; c++ )
					geometry = lib.clusters[c].triangleCount > 0;
			}
		if ( geometry )
			withGeometry++;
		else if ( b.cardLayer == LODO_NO_CARD )
			noGeometryNoCard++;
	}
	/* v3: THE LADDER'S WHOLE-MESH STATEMENT, which no single row can make. The
	 * reader has already refused a child that deviates more than its parent;
	 * here every mesh's ROOT clusters must account for its level-0 triangles
	 * exactly -- that is the partition at an infinite tolerance, and it is the
	 * same arithmetic the reference selector runs at every finite one. */
	quint32 ladderedMeshes = 0;
	quint64 rootTrisAll = 0, l0TrisAll = 0;
	for ( size_t mi = 0; mi < lib.meshes.size(); mi++ ) {
		const LodoMesh & m = lib.meshes[mi];
		quint64 rt = 0, lt = 0;
		for ( quint64 c = m.clusterFirst; c < quint64( m.clusterFirst ) + m.clusterCount; c++ ) {
			const LodoClusterLod & cl = lib.clusterLods[size_t( c )];
			if ( cl.level == 0 )
				lt += lib.clusters[size_t( c )].triangleCount;
			if ( cl.parentFirst == LODO_NO_PARENT )
				rt += cl.sourceTriangles;
		}
		if ( rt != lt )
			return fail( QString( "mesh %1 (%2): its root clusters account for %3 full-detail triangles but the mesh "
				"has %4 at level 0 -- the ladder is not a partition of its own surface" )
				.arg( mi ).arg( lib.stringAt( m.modelStringOffset ) ).arg( rt ).arg( lt ) );
		if ( m.levelCount > 1 )
			ladderedMeshes++;
		rootTrisAll += rt;
		l0TrisAll += lt;
	}
	quint32 occInCell = 0;
	for ( const LodiOccluderRange & r : tab.occluderRanges )
		if ( r.occluderCount )
			occInCell++;

	out << QString( "pair identity ok" ) << QString( "instancesWithGeometry %1" ).arg( withGeometry )
		<< QString( "instancesWithNeitherGeometryNorCard %1" ).arg( noGeometryNoCard )
		<< QString( "distinctStockIdentities %1" ).arg( identitySeen.size() )
		<< QString( "drawKeyRanksChecked %1" ).arg( tab.instances.size() )
		<< QString( "ladderMeshes %1 of %2" ).arg( ladderedMeshes ).arg( lib.meshes.size() )
		<< QString( "ladderRootTriangles %1" ).arg( rootTrisAll )
		<< QString( "ladderLevel0Triangles %1" ).arg( l0TrisAll )
		<< QString( "occluderBoxes %1" ).arg( tab.occluders.size() )
		<< QString( "occluderCells %1" ).arg( occInCell );
	if ( noGeometryNoCard )
		return fail( QString( "%1 instances have neither geometry nor a card" ).arg( noGeometryNoCard ) );
	if ( report )
		*report = out.join( QChar( '\n' ) );
	return true;
}
