/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef ESMDATA_H
#define ESMDATA_H

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

#include <memory>

class ESMFile;

/* The record layer LODGEN reads a worldspace through (docs/LODGEN_PLAN.md,
 * rung 0). It answers exactly three questions per cell: what is the ground
 * (LAND heights), what stands on it (REFRs with their transforms), and which
 * of those have LOD models (STAT MNAM / TREE). Everything else in the ESM is
 * out of scope on purpose.
 *
 * The container parser is the vendored fo76utils ESMFile; layouts follow
 * wbDefinitionsFO4.pas. One trap this layer owns so callers cannot fall into
 * it: a worldspace's PERSISTENT cell also reports grid (0,0) — it holds
 * thousands of REFRs and no LAND. It is distinguished by its parent group
 * (directly under the world-children group, not under a subblock), kept
 * separate, and its REFRs are served alongside every cell's.
 */

//! One placed reference, already resolved far enough for LOD generation.
struct EsmRefr
{
	quint32 formID = 0;
	quint32 base = 0;
	quint32 baseType = 0;       //!< record type fourcc ("STAT", "TREE", ...)
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 }; //!< radians, Z-Y-X euler as stored
	float scale = 1.0f;         //!< XSCL, 1.0 when absent
	bool initiallyDisabled = false;
	bool deleted = false;
};

//! A base object's LOD model set (STAT MNAM rows / TREE model).
struct EsmLodBase
{
	quint32 formID = 0;
	quint32 type = 0;
	QString models[4];          //!< per-level LOD model paths, empty = none
	QString model;              //!< the base's own near model (MODL): what the impostor bake photographs
	bool hasLod = false;
	// tree wind knobs (TREE CNAM / STAT DNAM), for the chunk manifest
	float trunkFlexibility = 0.0f;
	float branchFlexibility = 0.0f;
	float leafAmplitude = 0.0f;
	float leafFrequency = 0.0f;
};

//! One placement inside a static-collection part, in the SCOL's local space.
struct EsmScolPlacement
{
	float pos[3] = { 0, 0, 0 };
	float rot[3] = { 0, 0, 0 };  //!< radians, same euler convention as REFR
	float scale = 1.0f;
};

//! One part of a static collection: a source base and where its copies sit.
struct EsmScolPart
{
	quint32 base = 0;
	QVector<EsmScolPlacement> placements;
};

//! One additional splat layer on a cell quadrant: 17x17 opacities.
struct EsmLandLayer
{
	quint32 ltex = 0;
	float opacity[17][17];      //!< [row][col] over the quadrant, 0..1
};

/*! One GRAS record's ground-cover parameters. The payload is `DATA`, 32 bytes,
 *  in all 107 GRAS of the shipped corpus (there is no `DNAM`); offsets 3, 6..7
 *  and 29..31 are stale slots and are not read. The bake reads Density,
 *  Max Slope and MODL and nothing else — Min Slope is 0 in every record, so a
 *  lower gate would be dead code. */
struct EsmGrass
{
	quint32 form = 0;
	quint8 density = 0;         //!< off 0, 1..96 over the corpus
	quint8 minSlope = 0;        //!< off 1, 0 in all 107
	quint8 maxSlope = 0;        //!< off 2, degrees, 14..70
	quint8 flags = 0;           //!< off 28: 1 vertex lighting, 2 uniform scale, 4 fit to slope
	quint16 unitsFromWater = 0; //!< off 4 — read, not modelled (docs/LODGEN_TERRAIN_VT.md)
	quint32 waterType = 0;      //!< off 8
	float positionRange = 0.0f; //!< off 12
	float heightRange = 0.0f;   //!< off 16
	float colourRange = 0.0f;   //!< off 20 — a per-instance tint SPREAD, not a colour
	float wavePeriod = 0.0f;    //!< off 24
	QString model;              //!< MODL, the grass mesh the tint is read from
};

/*! The per-LTEX ground-cover constants of docs/LODGEN_TERRAIN_VT.md §2:
 *  D = the GNAM density sum, S = the density-weighted Max Slope, T = the
 *  density-weighted average grass colour. Cached per form exactly as
 *  ltexTextures() caches the texture pair, so a worldspace whose paint names
 *  no grass-bearing LTEX never opens a GRAS record. */
struct EsmLtexCover
{
	quint16 density = 0;        //!< D(L)
	quint16 tintDensity = 0;    //!< the tint-bearing part of D — T's denominator
	float maxSlope = 0.0f;      //!< S(L), degrees; 0 when D == 0
	float tint[3] = { 0.0f, 0.0f, 0.0f };   //!< T(L), 0..1 RGB
	bool hasTint = false;
	bool exists = false;        //!< the form resolved to an LTEX record at all
	int grasses = 0;            //!< GNAM links that resolved to a GRAS record
	int grassesWithoutTint = 0;
	int danglingGnam = 0;       //!< GNAM links whose target is not a GRAS
	bool resolved = false;
};

/*! Corpus-wide LTEX/GRAS totals, so the bake's census line carries its own
 *  denominators and a harness can cross-check it against an independent walk
 *  instead of against the reader that produced it. */
struct EsmCoverCensus
{
	int ltexTotal = 0;          //!< LTEX records in the loaded plugin set
	int grasTotal = 0;
	int gnamLinks = 0;          //!< GNAM subrecords over every LTEX
	int ltexWithGnam = 0;
	int grasDataMin = -1;       //!< smallest and largest GRAS DATA payload seen
	int grasDataMax = -1;
	int grasWithoutData = 0;
};

/*! Resolves a grass mesh path to its average diffuse colour, 0..1 RGB.
 *  The resolver lives in lodgen.cpp because it needs the asset reader and the
 *  DDS decoder, both internal there; the bake installs it with
 *  setGrassTintResolver() before the first ltexCover() call. Returning false
 *  means "no tint" — the grass still contributes its density to D. */
typedef bool ( *EsmGrassTintFn )( const QString & model, const QString & dataRoot,
	void * user, float * rgb );

//! One exterior cell's landscape: 33x33 heights in game units, world-placed.
struct EsmLand
{
	int cellX = 0, cellY = 0;
	bool valid = false;
	float heights[33][33];      //!< [row=y][col=x], SW origin, game units
	// splat data (docs/LODGEN_ESM_LAYOUTS.md): quadrants 0 BL, 1 BR, 2 TL, 3 TR
	quint32 baseTex[4] = { 0, 0, 0, 0 };    //!< BTXT LTEX per quadrant
	QVector<EsmLandLayer> layers[4];        //!< ATXT/VTXT layers, draw order
	bool hasColors = false;                 //!< VCLR present
	quint8 colors[33][33][3];               //!< [row][col] RGB, 255 = neutral
};

class EsmWorld
{
public:
	EsmWorld();
	~EsmWorld();

	//! Open an ESM (or comma-separated master list) and index one worldspace.
	bool load( const QString & esmPath, quint32 worldspaceFormID, QString * error );

	quint32 worldspace() const { return wsForm; }
	QString worldspaceEdid() const { return wsEdid; }

	float defaultLandHeight() const { return defLandH; }
	float defaultWaterHeight() const { return defWaterH; }

	//! Cell water: true when the cell has water, with its height resolved
	//! (XCLW when present and not the no-water sentinel, else the
	//! worldspace default).
	//!
	//! typeForm, when given, receives the cell's WATR form: XCWT when the cell
	//! overrides it, else the worldspace default from WRLD NAM2. Only 507 of
	//! the Commonwealth's 36,865 exterior cells override it -- but those are
	//! the Glowing Sea, the marshes and the rivers, so a consumer that cannot
	//! tell them from the harbour has nothing to go on.
	bool cellWater( int cx, int cy, float & height,
		quint32 * typeForm = nullptr ) const;

	//! The worldspace's default water type (WRLD NAM2), 0 when absent.
	quint32 defaultWaterType() const { return defWaterType; }

	//! Grid extent of indexed exterior cells (inclusive).
	void cellBounds( int & minX, int & minY, int & maxX, int & maxY ) const;

	/*! FO4CS's fingerprint of a heightmap bake's INPUT: FNV-1a 64 over the raw
	 *  VHGT payload of every LAND under this worldspace, in file order. The
	 *  Commonwealth's value is pinned inside that loader (0xD8337D022F637F22)
	 *  and a heightmap carrying any other value is refused, so the CLI checks
	 *  ours against it after every bake. */
	quint64 vhgtCorpusHash( int * landsHashed = nullptr ) const;
	static constexpr quint64 kCommonwealthVhgtCorpusHash = Q_UINT64_C( 0xD8337D022F637F22 );

	/*! FNV-1a 64 over the inputs the ground-cover plane and the far albedo
	 *  actually read: every LAND's raw BTXT/ATXT/VTXT payloads in ascending
	 *  cell order, then every referenced LTEX's TNAM and GNAM list, then every
	 *  referenced GRAS's DATA and MODL, both in ascending form-id order.
	 *  vhgtCorpusHash() pins HEIGHTS only and a plugin that adds a grass mod or
	 *  overrides an LTEX leaves it untouched — this is the hash that catches
	 *  that, and .lodv carries both. */
	quint64 paintCorpusHash() const;

	/*! The VHGT corpus hash taken in ascending cell order (y then x) instead of
	 *  file order. Measured only: vhgtCorpusHash() stays in FILE order because
	 *  its value is pinned inside FO4CS's shipped heightmap loader and inside
	 *  every heightmap DDS already written. See lane_terrain_report.md §4. */
	quint64 vhgtCorpusHashSorted( int * landsHashed = nullptr ) const;
	int cellCount() const { return cellIndex.size(); }
	bool hasCell( int cx, int cy ) const;

	//! LAND heights for one exterior cell. False when the cell or LAND is absent.
	bool land( int cx, int cy, EsmLand & out ) const;

	//! REFRs placed in one exterior cell (temporary + that cell's persistent).
	QVector<EsmRefr> refrs( int cx, int cy ) const;
	//! The worldspace's persistent-cell REFRs, grid-filtered by world position.
	QVector<EsmRefr> persistentRefrsIn( float minX, float minY, float maxX, float maxY ) const;

	//! LOD model info for a base object, cached. Never null.
	const EsmLodBase & lodBase( quint32 baseFormID ) const;

	//! A static collection's parts, cached; empty for non-SCOL bases.
	const QVector<EsmScolPart> & scolParts( quint32 formID ) const;

	//! An LTEX form's diffuse/normal texture paths (via TNAM -> TXST), cached.
	void ltexTextures( quint32 ltexForm, QString & diffuse, QString & normal ) const;

	//! One GRAS record, cached. False when the form is not a GRAS.
	bool grass( quint32 grasForm, EsmGrass & out ) const;

	//! An LTEX form's ground-cover constants (LTEX -> GNAM -> GRAS), cached.
	//! Never null; a form that is not an LTEX comes back with `exists` false.
	const EsmLtexCover & ltexCover( quint32 ltexForm, const QString & dataRoot ) const;

	//! Corpus-wide LTEX/GRAS totals for the bake's census line, computed once.
	const EsmCoverCensus & coverCensus() const;

	//! Install the grass-tint resolver (lodgen.cpp owns it; see EsmGrassTintFn).
	void setGrassTintResolver( EsmGrassTintFn fn, void * user ) const;

	//! How many GRAS records have actually been decoded — a census counter, so
	//! a per-texel plugin lookup fails a check instead of hiding in a parse.
	int grasReadCount() const { return grasReads; }

	//! All worldspaces in the file: formID -> EDID (static convenience).
	static QVector<QPair<quint32, QString>> listWorldspaces( const QString & esmPath, QString * error );

private:
	std::unique_ptr<ESMFile> esm;
	quint32 wsForm = 0;
	QString wsEdid;
	float defLandH = 0.0f;
	float defWaterH = 0.0f;
	quint32 defWaterType = 0;
	struct CellEntry
	{
		quint32 cellForm = 0;
		quint32 childGroup = 0;   //!< the cell's type-6 group (0 if none)
	};
	QHash<QPair<int, int>, CellEntry> cellIndex;
	quint32 persistentCellGroup = 0;
	mutable QHash<quint32, EsmLodBase> lodBaseCache;
	mutable QHash<quint32, QVector<EsmScolPart>> scolCache;
	mutable QHash<quint32, QPair<QString, QString>> ltexCache;
	mutable QHash<quint32, EsmLtexCover> ltexCoverCache;
	mutable QHash<quint32, EsmGrass> grasCache;
	mutable EsmCoverCensus census;
	mutable bool censusBuilt = false;
	mutable EsmGrassTintFn tintFn = nullptr;
	mutable void * tintUser = nullptr;
	mutable int grasReads = 0;
	mutable QVector<EsmRefr> persistentCache;
	mutable bool persistentCacheBuilt = false;

	void indexWorldspace();
	QVector<EsmRefr> refrsInGroup( quint32 groupID ) const;
	QVector<QPair<int, int>> cellsAscending() const;
	quint32 landFormOf( int cx, int cy ) const;
	const QVector<EsmRefr> & persistentRefrs() const;
};

#endif // ESMDATA_H
