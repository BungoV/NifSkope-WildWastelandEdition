/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef ESMDATA_H
#define ESMDATA_H

/* Lane CELLVIEW1 (2026-09-19): the fields the CELL VIEW needs and the
 * LOD bake never did -- the layer a ref is on, its enable parent, and a
 * base's editor id. A consumer compiled before this existed still
 * compiles: src/cellview.cpp tests this switch and says in its census
 * line which half it was built against. */
#define ESM_HAS_CELL_FIELDS 1

#include <QHash>
#include <QPair>
#include <QSet>
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
	/* v10 (lane CELLVIEW1): XLYR, the Creation Kit LAYER this ref is on,
	 * and XESP, its enable parent. Both 0 when the ref carries neither. */
	quint32 layer = 0;
	quint32 enableParent = 0;
	bool enableParentOpposite = false;    //!< XESP flag bit 0: the parent's state, inverted
};

//! A base object's LOD model set (STAT MNAM rows / TREE model).
struct EsmLodBase
{
	quint32 formID = 0;
	quint32 type = 0;
	QString models[4];          //!< per-level LOD model paths, empty = none
	QString model;              //!< the base's own near model (MODL): what the impostor bake photographs
	bool hasLod = false;
	QString edid;               //!< EDID, for the cell view's pick panel
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

/* THE ATXT LAYER INDEX IS NOW READ (lane CELLVIEW4).
 * cellsplat.cpp composites a quadrant's layers in this order. Without it
 * the order is the one the layers happen to sit in the record, which is
 * not the engine's paint order -- so the define is what lets the splat
 * builder tell a known order from a guess instead of quietly assuming. */
#define WW_CELLSPLAT_LAYER_INDEX 1

//! One additional splat layer on a cell quadrant: 17x17 opacities.
struct EsmLandLayer
{
	quint32 ltex = 0;
	float opacity[17][17];      //!< [row][col] over the quadrant, 0..1
	int index = 0;              //!< ATXT layer index: the engine's paint order
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

/*! Everything a layer's TXST names, not only the two slots the colour bake
 *  used to need.
 *
 *  The far-terrain mask sheet (docs/LODGEN_TERRAIN_VT.md §2.2) needs the
 *  MATERIAL as well as the maps: bungo's ruling of 2026-09-11 09:3x-09:5x is
 *  that a PBRM-backed layer gives its own roughness and metallic and a legacy
 *  one gives `1 - gloss`, and neither can be decided from TX00/TX01 alone.
 *  `specular` is TX07 -- the vanilla `_s` map, whose GREEN channel is the gloss
 *  this tree has always composed as `smoothness * _s.G` (the arrays pass in
 *  lodgen.cpp); `material` is MNAM, which is where a `.bgsm` and its same-name
 *  `.pbrm` sibling are looked for.
 *
 *  `diffuse` keeps its old behaviour EXACTLY, including the fallback that hands
 *  the material path through when a material-backed TXST carries no TX00, so
 *  every existing caller reads what it read before. */
struct EsmLtexTextureSet
{
	QString diffuse;            //!< TX00, or MNAM when the TXST is material-backed
	QString normal;             //!< TX01
	QString specular;           //!< TX07, the vanilla `_s` map
	QString material;           //!< MNAM, exactly as spelled
	bool exists = false;        //!< the form resolved to an LTEX naming a TXST
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

/*! v9 (lane HORIZON3, 2026-09-19): ONE WORKSHOP BUILD AREA.
 *
 *  An XPRM Box primitive on a REFR that links, by KYWD
 *  `000B91E6 WorkshopLinkedPrimitive`, to a workshop workbench REFR. That box
 *  IS the settlement's build area: what is inside it is what the player can
 *  walk up to and scrap.
 *
 *  The box is rotated by its own REFR's DATA rotation, and ONLY ABOUT Z. That
 *  is not a simplification of the record -- DATA carries all three angles and
 *  they are read -- it is what the measurement found: every one of the 111
 *  build areas in Fallout4.esm has X and Y within float noise of 0, because
 *  the Creation Kit's build-area tool only ever yaws them. The Python oracle
 *  this ports (`scratchpad/horizon3_20260919/scrap_rule.py`) tests the same
 *  way, so the two agree by construction rather than by luck, and a plugin
 *  that ever pitches one will be caught by `tiltedAreas` below rather than
 *  silently mis-tested. */
struct EsmScrapBox
{
	float centre[3] = { 0.0f, 0.0f, 0.0f };
	float half[3] = { 0.0f, 0.0f, 0.0f };
	float yaw = 0.0f;           //!< the primitive REFR's DATA rotation about Z, radians
	quint32 refr = 0;           //!< the REFR that carries the primitive
	quint32 workshop = 0;       //!< the workbench REFR it links to
};

/*! v9 (lane HORIZON3): THE WORKSHOP-SCRAPPABLE INDEX, built once per plugin
 *  set on first use and cached.
 *
 *  THE RULE, in three clauses, each read out of the plugin and none of them
 *  from memory:
 *
 *    1. the placement's BASE is the CNAM (created object) of a COBJ whose FNAM
 *       category array contains `00106D8F WorkshopRecipeFilterScrap`. 87 of
 *       those CNAM targets are FormLists rather than bases -- a scrap recipe
 *       may name a whole family at once -- so FLST members are expanded
 *       transitively; AND
 *    2. the placement's position lies inside at least one build area
 *       (`EsmScrapBox`); AND
 *    3. the placement's base does NOT carry `001CC46A UnscrappableObject`.
 *
 *  Clause 3 removes nothing on Fallout4.esm -- 149 bases carry the keyword and
 *  only 2 of them are in clause 1, and neither of those 2 is placed inside a
 *  build area. It is in the rule anyway, because a DLC or a mod that sets it
 *  means it, and a rule that only works on the base game is not a rule.
 *
 *  ON THE MEASURED URBAN REGION THIS IS 14 PLACEMENTS OF 33,123 (0.04%). That
 *  number is the gate: a bake whose `scrappablePlacements` is not 14 on that
 *  region has changed the rule, whether or not anyone meant to. */
struct EsmScrapIndex
{
	bool built = false;
	QSet<quint32> scrapBases;       //!< clause 1, FormLists expanded
	QSet<quint32> unscrappable;     //!< clause 3
	QVector<EsmScrapBox> buildAreas;    //!< clause 2
	// the census, all of it printed rather than assumed
	int cobjRecords = 0;            //!< COBJ records walked
	int cobjScrapRecipes = 0;       //!< ...of which carry the scrap filter keyword
	int formListsExpanded = 0;      //!< CNAM targets that were FLST, not bases
	int workshopRefrs = 0;          //!< REFRs whose base EDID names a workshop workbench
	int primitivesSeen = 0;         //!< REFRs carrying an XPRM at all
	int primitivesNotBox = 0;       //!< ...that are not type 1 (Box)
	int primitivesUnlinked = 0;     //!< ...with no WorkshopLinkedPrimitive link to a workshop
	int tiltedAreas = 0;            //!< build areas whose X or Y rotation is not ~0 (see EsmScrapBox)
	int clause1And3 = 0;            //!< bases in clause 1 that clause 3 then removed
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

	//! Exactly the string load() was given: one plugin path, or a comma-separated list in load order.
	QString pluginList() const { return srcPath; }

	/*! FNV-1a 64 over the LOAD ORDER, in the order load() was given: for each
	 *  plugin, its lower-cased BASE FILE NAME's UTF-8 bytes, then its byte
	 *  size as a little-endian u64. Nothing else -- not the path, not the
	 *  mtime, so moving a mod folder or touching a file does not fire it while
	 *  adding, removing, reordering or editing a plugin does. `.lodo`/`.lodi`
	 *  carry it at header 0xB8 / 0x90 (docs/LODGEN_NATIVE_LODO_LODI.md 8);
	 *  the object corpus hash beside it covers the RECORDS. */
	quint64 loadOrderHash() const;

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

	/*! The cell record's EDID for this grid position, or empty when the
	 *  worldspace has no cell there or the record carries none (lane
	 *  CELLWORK1). The DISPLAY name (FULL) is not available: see CellEntry. */
	QString cellEditorId( int cx, int cy ) const;
	//! The cell record's form id at this grid position, or 0.
	quint32 cellForm( int cx, int cy ) const;
	//! The worldspace's persistent-cell REFRs, grid-filtered by world position.
	QVector<EsmRefr> persistentRefrsIn( float minX, float minY, float maxX, float maxY ) const;

	//! LOD model info for a base object, cached. Never null.
	const EsmLodBase & lodBase( quint32 baseFormID ) const;

	/*! v9 (lane HORIZON3, 2026-09-19): is this placement WORKSHOP-SCRAPPABLE?
	 *  The three-clause rule of `EsmScrapIndex`, on the base form and the
	 *  placement's WORLD position. The index behind it is built once, on the
	 *  first call, by one walk of the whole plugin set; a bake that never asks
	 *  never pays for it. `areaRefr` receives the build area that answered.
	 */
	bool scrappable( quint32 baseForm, const float worldPos[3], quint32 * areaRefr = nullptr ) const;
	//! The index itself, for the census line. Built on first use.
	const EsmScrapIndex & scrapIndex() const;

	//! A static collection's parts, cached; empty for non-SCOL bases.
	const QVector<EsmScolPart> & scolParts( quint32 formID ) const;

	//! An LTEX form's diffuse/normal texture paths (via TNAM -> TXST), cached.
	void ltexTextures( quint32 ltexForm, QString & diffuse, QString & normal ) const;

	//! The WHOLE texture set the same TNAM -> TXST chain names, cached; the
	//! mask sheet needs TX07 and MNAM as well. Never null; a form that is not an
	//! LTEX naming a TXST comes back with `exists` false and every path empty.
	const EsmLtexTextureSet & ltexTextureSet( quint32 ltexForm ) const;

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
	QString srcPath;            //!< what load() was given, verbatim
	float defLandH = 0.0f;
	float defWaterH = 0.0f;
	quint32 defWaterType = 0;
	struct CellEntry
	{
		quint32 cellForm = 0;
		quint32 childGroup = 0;   //!< the cell's type-6 group (0 if none)
		//! lane CELLWORK1: the cell record's EDID. FULL is a localised string
		//! index in Fallout4.esm and this reader has no string table, so the
		//! DISPLAY name is not available here -- see esmdata.cpp.
		QString edid;
	};
	QHash<QPair<int, int>, CellEntry> cellIndex;
	quint32 persistentCellGroup = 0;
	mutable QHash<quint32, EsmLodBase> lodBaseCache;
	mutable QHash<quint32, QVector<EsmScolPart>> scolCache;
	mutable EsmScrapIndex scrapIdx;     //!< v9, built on first scrappable() call
	mutable QHash<quint32, EsmLtexTextureSet> ltexCache;
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
	void buildScrapIndex() const;
};

#endif // ESMDATA_H
