/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef ESMDATA_H
#define ESMDATA_H

// the fields the CELL VIEW needs and the LOD bake never did -- the layer a ref is on, its enable parent, and a
// base's editor id.
#define ESM_HAS_CELL_FIELDS 1

#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

#include "features/far-lod/largeref.h"	// lane GEOLOD1: WRLD RNAM rows

class ESMFile;

// The record layer LODGEN reads a worldspace through (docs/LODGEN_PLAN.md, rung 0).

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
	/* v10: XLYR, the Creation Kit LAYER this ref is on,
	 * and XESP, its enable parent. Both 0 when the ref carries neither. */
	quint32 layer = 0;
	quint32 enableParent = 0;
	bool enableParentOpposite = false;    //!< XESP flag bit 0: the parent's state, inverted
	// XMSP, the placement's MATERIAL SWAP (an MSWP form), 0 when the ref carries none. It overrides the base's
	// MODS.
	quint32 materialSwap = 0;
	// XEMI, the placement's emittance source (a LIGH or a REGN), 0 when none
	quint32 emittance = 0;
	/* XTEL, a door's teleport (its first field, the destination door), 0 when none. A load
	 * door: in game it never opens, it only plays a short animation and loads elsewhere. */
	quint32 teleport = 0;
	// what a placed LIGH overrides. XRDS is the radius, when `hasRadius` (else the base's DATA radius stands;
	// the plugin stores NEGATIVE values too -- 2,080 of the gate's 3,945 rows -- so no sentinel).
	bool hasRadius = false;
	float radius = 0.0f;
	int xligCount = 0;
	float xlig[6] = { 0, 0, 0, 0, 0, 0 };
	/* a light's box. `lightBox` = the XLKR target under KYWD 00115705 LightBoxLink
	 * (0 when none); a primitive ref carries XPRM: its type (1 Box) and half extents as stored. */
	quint32 lightBox = 0;
	bool hasPrim = false;
	quint32 primType = 0;
	float primHalf[3] = { 0, 0, 0 };
	/* the game's rooms and portals (wbDefinitionsFO4 REFR). A portal (PortalMarker) carries XPOD,
	 * its origin and destination ROOM refs (0 = outside); a room (RoomMarker) carries XRMR and its XLRM linked
	 * rooms, the rooms it always sees without a portal. */
	quint32 portalFrom = 0, portalTo = 0;
	bool roomBound = false;
	QVector<quint32> linkedRooms;
	/* XBSD, a placed cable (its base is a BNDS bendable spline). Slack, thickness, the half
	 * extents (the cable's ends sit at the ref's position minus / plus these, in the ref's own frame), and
	 * the detached-end byte (absent in a 20-byte XBSD = attached at both ends; any nonzero byte = detached). */
	bool hasSpline = false;
	float splineSlack = 0.0f;
	float splineThickness = 0.0f;
	float splineHalf[3] = { 0, 0, 0 };
	bool splineDetached = false;
};

/*! a BNDS (bendable spline) base, the look of a cable. DNAM: default tile count, slices round
 *  the tube, whether the tiles count per unit of length, the default colour, wind sensibility and wind
 *  flexibility; TNAM: a texture set (TXST TX00 / TX01), empty when none. */
struct EsmBendable
{
	bool exists = false;
	QString edid;
	float tiles = 1.0f;
	int slices = 3;
	bool tilesRelative = false;
	float color[3] = { 1, 1, 1 };
	float windSensibility = 0.0f;
	float windFlexibility = 0.0f;
	QString diffuse, normal;
};

//! one CELL's precombined / previs fields (wbDefinitionsFO4 CELL).
struct EsmCellPrevis
{
	bool exists = false;
	quint32 rvis = 0;
	quint16 visi = 0, pcmb = 0;
	bool hasVisi = false, hasPcmb = false;
	quint32 combinedMeshes = 0, combinedRefs = 0, previsRefs = 0;
	//! XCRI's refs (each pair's first word, mapped to the load order): the refs a precombined mesh replaced
	QVector<quint32> combinedRefIds;
	/*! lane PCMB1: XCRI's mesh keys (meshes\precombined\<cell form & 0xFFFFFF, 8 hex>_<key, 8 hex>_oc.nif) and, parallel
	 *  to combinedRefIds, the key of the mesh each ref went into (each pair's second word) */
	QVector<quint32> combinedMeshKeys, combinedRefKeys;
	//! the winning CELL record's plugin (load-order index) and whether it carries XPRI / VISI
	quint32 srcFile = 0;
	bool hasPrevisFields = false;
};

/* one REFR's plugins, for the previs staleness rule (src/features/near/cellprevisrule.h): the plugin that owns
 * its form id, the plugin of its winning version, and its record flags. */
struct EsmRefSource
{
	quint32 owner = 0, src = 0;
	bool deleted = false, disabled = false;
	quint32 form = 0;	//!< lane PCMB1: the REFR's form id (load order), to match XCRI's refs
	//! the plugin of the first version in the trailing run of versions whose placement
	//! (DATA, XSCL, deleted/disabled, XESP) equals the winner's (WwPrevisRule::RefSrc::settled)
	quint32 settled = 0xFFFFFFFFu;
};

// one LIGH record (wbDefinitionsFO4 LIGH DATA + FNAM).
struct EsmLight
{
	bool exists = false;
	QString edid;
	qint32 time = 0;
	quint32 radius = 0;
	quint8 color[3] = { 0, 0, 0 };  //!< R, G, B as stored
	quint32 flags = 0;
	float falloff = 1.0f;           //!< falloff exponent
	float fov = 90.0f;
	float nearClip = 10.0f;
	float constant = 0.0f, scalar = 0.0f, exponent = 0.0f;  //!< present from DATA's 11th member
	bool hasAttenuation = false;
	float fade = 1.0f;              //!< FNAM
	QString gobo;                   //!< NAM0
	quint32 godRays = 0;            //!< WGDR, the light's GDRY (0 = none: the light casts no shafts)
	quint32 lens = 0;               //!< LNAM, the light's LENS (0 = no lens flare)
	float godRayNear = 0.0f;        //!< DATA "God Rays - Near Clip" (offset 52)
	float flickerPeriod = 0.0f, flickerIntensity = 0.0f, flickerMovement = 0.0f;	//!< DATA Flicker Effect
	//! "omni", "spot", "hemi", "box", with "shadow " in front when it casts one
	QString typeName() const;
};

// an interior cell -- no LAND, no grid, its own lighting (XCLL, wbDefinitionsFO4 layout) and an optional
// lighting template (LTMP -> LGTM).
struct EsmInteriorCell
{
	bool valid = false;
	quint32 cellForm = 0;
	QString edid;
	quint32 lightingTemplate = 0;   //!< LTMP, 0 when absent
	quint32 imageSpace = 0;         //!< XCIM, the cell's IMGS, 0 when absent
	quint32 godRays = 0;            //!< XGDR, the cell's GDRY, 0 when absent
	QByteArray xcll;                //!< raw, empty when absent
	quint8 ambient[3] = { 0, 0, 0 };
	quint8 directional[3] = { 0, 0, 0 };
	quint8 fogNearColor[3] = { 0, 0, 0 };
	float fogNear = 0.0f, fogFar = 0.0f;
	quint32 inherits = 0;           //!< XCLL "Inherits" flags: which fields come from LTMP
	quint16 flags = 0;              //!< DATA: bit 7 Show Sky, 8 Use Sky Lighting, 11 Sunlight Shadows
	bool hasWater = false;
	float waterHeight = 0.0f;       //!< XCLW, valid when hasWater
};

//! one MSWP substitution row, BNAM -> SNAM (+ CNAM when present).
struct EsmMaterialSubst
{
	QString original;           //!< BNAM, as stored (relative to Materials\)
	QString replacement;        //!< SNAM, as stored
	bool hasColorRemap = false; //!< a CNAM followed this row
	float colorRemap = 0.0f;    //!< CNAM, the colour-remapping index
};

//! an MSWP record, the winning version by load order.
struct EsmMaterialSwap
{
	quint32 formID = 0;
	bool exists = false;        //!< the form is an MSWP record
	QString edid;
	QVector<EsmMaterialSubst> rows;
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
	quint32 materialSwap = 0;   //!< MODS, the base's default material swap (MSWP), 0 = none
	/* 2026-10-01: MODC, the model's color remapping index -- the paint row a
	 * Greyscale_To_PaletteColor material takes (the Concord museum pickup, MSTT
	 * PickUpTruck08, is 0.235). An MSWP row's own CNAM wins for its material. */
	bool hasColorRemap = false;
	float colorRemap = 0.0f;
	/* the record header's flag word (STAT bit 23 = Is
	 * Marker, 0x00800000) and whether the record carries destructible data
	 * (a DEST or DSTD subrecord). Read-only facts for the near-library bake. */
	quint32 recordFlags = 0;
	bool hasDestructible = false;
	/* an ACTI's WNAM, the water type (WATR) it
	 * carries. A placed ACTI with one is an authored water mesh (a river,
	 * a pool); 0 = none. */
	quint32 waterType = 0;
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

// THE ATXT LAYER INDEX IS NOW READ. cellsplat.cpp composites a quadrant's layers in this order.
#define WW_CELLSPLAT_LAYER_INDEX 1

//! One additional splat layer on a cell quadrant: 17x17 opacities.
struct EsmLandLayer
{
	quint32 ltex = 0;
	float opacity[17][17];      //!< [row][col] over the quadrant, 0..1
	int index = 0;              //!< ATXT layer index: the engine's paint order
};

//! One GRAS record's ground-cover parameters. The payload is `DATA`, 32 bytes, in all 107 GRAS of the shipped
//! corpus (there is no `DNAM`); offsets 3, 6..7 and 29..31 are stale slots and are not read.
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

//! The per-LTEX ground-cover constants of docs/LODGEN_TERRAIN_VT.md §2: D = the GNAM density sum, S = the
//! density-weighted Max Slope, T = the density-weighted average grass colour.
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

//! Everything a layer's TXST names, not only the two slots the colour bake used to need.
constexpr quint32 ESM_LTEX_ENGINE_DEFAULT = 0xFFFFFFFFu;

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

//! Resolves a grass mesh path to its average diffuse colour, 0..1 RGB.
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
	/*! VNML, the game's own vertex normals on the same 33x33 grid ([row][col], world x y z,
	 *  unit length after decode). The raw bytes are kept beside them so a gate can decode them on its own. */
	bool hasNormals = false;
	float normals[33][33][3];
	quint8 normalsRaw[33 * 33 * 3];
};

//! v9: ONE WORKSHOP BUILD AREA:
//! An XPRM Box primitive on a REFR that links, by KYWD `000B91E6 WorkshopLinkedPrimitive`, to a workshop
//! workbench REFR.
struct EsmScrapBox
{
	float centre[3] = { 0.0f, 0.0f, 0.0f };
	float half[3] = { 0.0f, 0.0f, 0.0f };
	float yaw = 0.0f;           //!< the primitive REFR's DATA rotation about Z, radians
	quint32 refr = 0;           //!< the REFR that carries the primitive
	quint32 workshop = 0;       //!< the workbench REFR it links to
};

//! v9: THE WORKSHOP-SCRAPPABLE INDEX, built once per plugin set on first use and cached.
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
	//! whose terrain LOD files the worldspace draws (the parent's under PNAM "Use LOD Data")
	QString lodWorldEdid() const { return lodEdid.isEmpty() ? wsEdid : lodEdid; }
	//! WRLD DATA (the flags byte; 0x40 "Fixed Dimensions") and WCTR (the fixed centre cell)
	unsigned worldFlags() const { return wsFlags; }
	bool hasFixedCentre() const { return wsHasCentre; }
	int fixedCentreX() const { return wsCentreX; }
	int fixedCentreY() const { return wsCentreY; }

	//! Exactly the string load() was given: one plugin path, or a comma-separated list in load order.
	QString pluginList() const { return srcPath; }

	/*! the base file name of the plugin that holds the WINNING
	 *  version of a record (ESMFile's srcFile, an index into pluginList()).
	 *  Empty when the form is not in the load order. */
	QString recordPlugin( quint32 formID ) const;

	//! FNV-1a 64 over the LOAD ORDER, in the order load() was given: for each plugin, its lower-cased BASE FILE
	//! NAME's UTF-8 bytes, then its byte size as a little-endian u64.
	quint64 loadOrderHash() const;

	float defaultLandHeight() const { return defLandH; }
	float defaultWaterHeight() const { return defWaterH; }

	//! Cell water: true when the cell has water, with its height resolved (XCLW when present and not the
	//! no-water sentinel, else the worldspace default).
	bool cellWater( int cx, int cy, float & height,
		quint32 * typeForm = nullptr ) const;
	//! cell (cx, cy)'s regions (XCLR, load-order forms) and its form in *cellForm; empty = none
	QVector<quint32> cellRegions( int cx, int cy, quint32 * cellForm = nullptr ) const;

	//! The worldspace's default water type (WRLD NAM2), 0 when absent.
	quint32 defaultWaterType() const { return defWaterType; }
	//! MODULES1 R6-2: the sea this worldspace lies in: its own NAM2, else the first one up its WNAM parents
	//! (Goodneighbor has none of its own and lies in the Commonwealth's); 0 when the chain has none.
	quint32 seaWaterType() const { return seaType; }
	//! the worldspace that names seaWaterType() (its EDID)
	QString seaWaterFrom() const { return seaFrom; }

	//! the WATR record `form` (src/features/water/esmwater.h), false when it is no WATR
	bool waterRecord( quint32 form, struct WwWaterRecord & out ) const;

	//! Grid extent of indexed exterior cells (inclusive).
	void cellBounds( int & minX, int & minY, int & maxX, int & maxY ) const;

	//! FO4CS's fingerprint of a heightmap bake's INPUT: FNV-1a 64 over the raw VHGT payload of every LAND under
	//! this worldspace, in file order.
	quint64 vhgtCorpusHash( int * landsHashed = nullptr ) const;
	static constexpr quint64 kCommonwealthVhgtCorpusHash = Q_UINT64_C( 0xD8337D022F637F22 );

	//! FNV-1a 64 over the inputs the ground-cover plane and the far albedo actually read: every LAND's raw
	//! BTXT/ATXT/VTXT payloads in ascending cell order, then every referenced LTEX's TNAM and GNAM list, then
	//! every referenced GRAS's DATA and MODL, both in ascending form-id order.
	quint64 paintCorpusHash() const;

	//! The VHGT corpus hash taken in ascending cell order (y then x) instead of file order.
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
	//! the CELL record `cellForm`'s previs / precombined fields (exists false: no such CELL)
	EsmCellPrevis cellPrevis( quint32 cellForm ) const;
	//! The cell record's form id at this grid position, or 0.
	quint32 cellForm( int cx, int cy ) const;
	//! The worldspace's persistent-cell REFRs, grid-filtered by world position.
	QVector<EsmRefr> persistentRefrsIn( float minX, float minY, float maxX, float maxY ) const;
	//! lane GEOLOD1: the WRLD's RNAM large-ref rows (load-order form ids), as read (red WW_CELL_LARGEREFS_RED=xy: X,Y order)
	const std::vector<LargeRef::Row> & largeRefRows() const { return largeRows; }
	//! lane GEOLOD1: one REFR of cell (cx, cy) or of the persistent cell by form id; false when neither holds it
	bool refrByForm( quint32 formID, int cx, int cy, EsmRefr & out, bool * persistent = nullptr ) const;
	//! lane CELLSPAWN1: a base form's OBND (object bounds, base-local units), false when it has none
	bool objectBounds( quint32 baseForm, float lo[3], float hi[3] ) const;
	//! the REFR `doorRef`'s XTEL arrival point (where the game puts the player who uses the
	//! OTHER side's door), false when that REFR or its XTEL is absent
	bool teleportArrival( quint32 doorRef, float pos[3], float rot[3] ) const;

	//! LOD model info for a base object, cached. Never null.
	const EsmLodBase & lodBase( quint32 baseFormID ) const;

	//! an MSWP record's substitution rows, cached. Never null; a form that is not an MSWP comes back with
	//! `exists` false.
	const EsmMaterialSwap & materialSwap( quint32 mswpForm ) const;

	//! a BNDS base's cable look, cached. Never null; `exists` false when the form is not a BNDS.
	const EsmBendable & bendable( quint32 bndsForm ) const;

	//! a placed REFR's XMSP and NAME, read from the record by its form id (the winning version). False when the
	//! form is not a REFR.
	bool refrMaterialSwap( quint32 refrForm, quint32 * xmsp, quint32 * base ) const;

	//! v9: is this placement WORKSHOP-SCRAPPABLE? The three-clause rule of `EsmScrapIndex`, on the base form
	//! and the placement's WORLD position.
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

	//! the parent EDID whose terrain LOD `wsEdid` draws ("Use LOD Data"), else empty; no load
	static QString lodWorldOf( const QString & esmPath, const QString & wsEdid, QString * error = nullptr );

	/*! every INTERIOR cell of a load order: formID -> EDID, in
	 *  file order. Walks the top-level CELL group only (blocks, subblocks). */
	static QVector<QPair<quint32, QString>> listInteriors( const QString & esmPath, QString * error );

	/*! open a load order for ONE interior cell instead of a
	 *  worldspace. `cell` is its EDID or its form id in hex. After this the
	 *  exterior queries answer nothing; interior and interiorRefrs do. */
	bool loadInterior( const QString & esmPath, const QString & cell, QString * error );
	const EsmInteriorCell & interior() const { return interiorCell; }
	//! The interior's REFRs, persistent and temporary, later plugins' groups joined.
	QVector<EsmRefr> interiorRefrs() const;

	//! one LIGH record, cached. Never null; `exists` false when not a LIGH.
	const EsmLight & light( quint32 formID ) const;
	/*! one LGTM (lighting template) record, raw: `data` = DATA (the XCLL layout through
	 *  the Light Fade distances), `dalc` = its DALC (6 axis colours, specular, scale). False when
	 *  the form is not an LGTM. */
	bool lightingTemplate( quint32 formID, QByteArray & data, QByteArray & dalc ) const;
	//! an LGTM's WGDR (its GDRY), 0 when absent or not an LGTM
	quint32 lightingTemplateGodRays( quint32 formID ) const;
	//! an IMGS record's EDID, HNAM (HDR, 9 floats), CNAM (cinematic, 3), TNAM (tint, 4) raw and its
	//! TX00 LUT path; false when not an IMGS
	bool imageSpace( quint32 formID, QString & edid, QByteArray & hnam, QByteArray & cnam, QByteArray & tnam,
		QString & lut ) const;

	//! the plugin itself, for readers kept in their own files (src/esmplaced.cpp)
	ESMFile * plugin() const { return esm.get(); }
	//! the open interior's child groups, load order (empty for a worldspace)
	const QVector<quint32> & interiorChildGroups() const { return interiorGroups; }
	//! an exterior cell's child groups (its own + later plugins'), empty when the cell is absent
	QVector<quint32> cellChildGroups( int cx, int cy ) const;
	//! the worldspace's persistent-cell groups (its own + later plugins')
	QVector<quint32> persistentChildGroups() const;
	//! every REFR in these groups (nested groups walked) once, with its owner and winning plugins
	QVector<EsmRefSource> refSources( const QVector<quint32> & groups ) const;

private:
	std::unique_ptr<ESMFile> esm;
	quint32 wsForm = 0;
	QString wsEdid;
	QString lodEdid;            //!< the parent's EDID when it lends its LOD, else empty
	unsigned wsFlags = 0;       //!< WRLD DATA
	bool wsHasCentre = false;   //!< WRLD WCTR read
	int wsCentreX = 0, wsCentreY = 0;
	QString srcPath;            //!< what load() was given, verbatim
	float defLandH = 0.0f;
	float defWaterH = 0.0f;
	quint32 defWaterType = 0;
	quint32 seaType = 0;
	QString seaFrom;
	/*! a child worldspace that takes its parent's land (WRLD
	 *  WNAM parent + PNAM bit 0 "Use Land Data": in Fallout4.esm only Goodneighbor) holds no LAND of its own; the game
	 *  draws the PARENT's LAND at the same cell grid. Cell grid -> the parent cell's type-6 group, indexed cells only. */
	quint32 landParent = 0;
	QHash<QPair<int, int>, quint32> parentLandGroups;
	quint32 landFormInGroup( quint32 group ) const;
	void indexParentLand();
	struct CellEntry
	{
		quint32 cellForm = 0;
		quint32 childGroup = 0;   //!< the cell's type-6 group (0 if none)
		//! type-6 groups a LATER plugin opened under this cell (its new REFRs);
		//! libfo76utils links them to no record, see indexWorldspace
		QVector<quint32> extraGroups;
		//! the cell record's EDID. FULL is a localised string
		//! index in Fallout4.esm and this reader has no string table, so the
		//! DISPLAY name is not available here -- see esmdata.cpp.
		QString edid;
	};
	QHash<QPair<int, int>, CellEntry> cellIndex;
	quint32 persistentCellGroup = 0;
	quint32 persistentCellForm = 0;
	QVector<quint32> persistentExtraGroups;   //!< later plugins' persistent-cell groups
	int extraWorldGroups = 0;                 //!< later plugins' world-children groups walked
	int extraCellGroups = 0;                  //!< later plugins' cell-children groups joined
	mutable QHash<quint32, EsmLodBase> lodBaseCache;
	mutable QHash<quint32, EsmMaterialSwap> mswpCache;
	mutable QHash<quint32, EsmBendable> bendableCache;
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
	EsmInteriorCell interiorCell;
	QVector<quint32> interiorGroups;        //!< its type-6 groups, load order
	mutable QHash<quint32, EsmLight> lightCache;
	std::vector<LargeRef::Row> largeRows;   //!< lane GEOLOD1
	mutable QHash<QPair<int, int>, QVector<EsmRefr>> refrCellCache;   //!< lane GEOLOD1: refrByForm's cells

	void indexWorldspace();
	QVector<EsmRefr> refrsInGroup( quint32 groupID ) const;
	QVector<QPair<int, int>> cellsAscending() const;
	quint32 landFormOf( int cx, int cy ) const;
	const QVector<EsmRefr> & persistentRefrs() const;
	void buildScrapIndex() const;
};

#endif // ESMDATA_H
