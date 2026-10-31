/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLVIEW_H
#define CELLVIEW_H

#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

class NifModel;

/* The Cell view: a read-only viewer for one exterior cell (or an N x N block) or an
 * interior cell, built as a NifModel document of welded world-space BSTriShapes so the
 * existing renderer draws it. Opened from a `.wwcell` file or WW_CELL_OPEN. */

//! The colour overlay. One at a time; `None` draws the models' own materials.
enum class CellOverlay
{
	None = 0,
	Layer,          //!< by XLYR layer
	Identity,       //!< by our `.lodi` GROUP id (needs a bake beside it)
	HasLod,         //!< by which MNAM slots the base fills
	RecordType,     //!< by the base's record type
	Precombined     //!< by whether the cell's combined-refs list names the ref
};

//! The canonical overlay name (`layer`, `identity`, ...); empty for `None`.
QString cellOverlayName( CellOverlay o );
//! Every accepted name, in the order the docs list them.
QString cellOverlayNames();
//! The overlay a name asks for; `None` for an empty string. `*known` is false
//! when the name is not one of them, so it can be refused BY NAME.
CellOverlay cellOverlayFromName( const QString & name, bool * known = nullptr );

//! What to build: a worldspace, a centre cell, an odd block size.
struct CellSceneSpec
{
	QString plugins;        //!< comma-separated load order, exactly as EsmWorld::load takes it
	QString world;          //!< worldspace EDID, e.g. "Commonwealth"
	QString dataRoot;       //!< where models are read from; empty = the resource stack's first entry
	int cx = 0, cy = 0;
	int n = 1;              //!< block size, odd: 1, 3, 5
	CellOverlay overlay = CellOverlay::None;
	QString lodiPath;       //!< a bake to read GROUP ids out of, for CellOverlay::Identity
	QStringList resources;  //!< lane OPENCELLFIX1: the load order's resource folders (a file's `resources|a;b` line), empty = unchanged
	bool showDisabled = false;  //!< draw initially-disabled / enable-parent-off refs, tinted
	bool showMarkers = false;
	bool terrain = true;
	bool water = true;
	bool grid = false;      //!< the cell grid overlay (off by default, as the "Cell borders" row ships)
	/* An INTERIOR cell instead of an exterior block: `plugins|interior|<EDID or hex form>[|overlay]`.
	 * No LAND, no grid, no worldspace water; `world` holds the word "interior". */
	bool interior = false;
	QString interiorCell;
	/* `probes` places the bake's probes over the whole loaded block; `probesShow` draws them. */
	bool probes = false;
	bool probesShow = true;
	bool probesHiddenByEnv = false;	//!< lane LIVESET1: WW_CELL_PROBES_HIDE turned probesShow off
	/* Place the probes, then bake them into `.tbk` sector files in `bakeDir` (empty =
	 * `<app>/prtp_bake/<world or cell>`). One-shot: the workspace clears it once noted. */
	bool probesBake = false;
	QString bakeDir;
	bool valid = false;

	//! The inclusive cell rectangle `n` asks for around (cx, cy).
	void rect( int & x0, int & y0, int & x1, int & y1 ) const;
};

//! Parse `plugins|world|x,y|n[|overlay]` or `plugins|interior|<cell>[|overlay]`.
//! False (with `*error`) on a bad line.
bool cellSpecFromLine( const QString & line, CellSceneSpec & spec, QString * error );
//! Read a `.wwcell` file's first non-comment line, and a `resources|a;b;c` line after it (lane OPENCELLFIX1).
bool cellSpecFromFile( const QString & path, CellSceneSpec & spec, QString * error );
/*! `WW_CELL_OPEN`, plus `WW_CELL_OVERLAY`, `WW_CELL_DISABLED`, `WW_CELL_MARKERS`,
 *  `WW_CELL_DATAROOT`, `WW_CELL_LODI`, `WW_CELL_NOTERRAIN`, `WW_CELL_NOWATER`,
 *  `WW_CELL_NOGRID`, each read on its own. False when `WW_CELL_OPEN` is unset:
 *  the others then still apply to a spec that came from a file. */
bool cellSpecFromEnv( CellSceneSpec & spec, QString * error );
//! Apply only the modifier variables to an existing spec (see above).
void cellApplyEnvModifiers( CellSceneSpec & spec );

//! One probe marker kind: its name in the PRTP band and its draw color.
struct CellProbeKind
{
	const char * name;
	float rgb[3];
};

//! The marker kinds in the PRTP band's order; the markers draw with the same table.
const CellProbeKind * cellProbeKinds( int * count );

/*! Build the document. `notes` receives what the scene measured (counts, skips, legend,
 *  load time) so a picture is never the only evidence. */
//! lane BAKEINCR1: sha1 of the far soup a bake of (cx, cy) with a (2 half + 1)^2 block appends; "none" without LOD
//! lane LANDSPEC5: with `w`, a "Use LOD Data" child's ring is its parent's (the bake's own)
class EsmWorld;
QString cellBakeFarDigest( const QString & world, const QString & dataRoot, int cx, int cy, int half,
	const EsmWorld * w = nullptr );

bool nifCreateCellScene( NifModel * nif, const CellSceneSpec & spec,
	QString * error, QString * notes = nullptr );

/*! lane GLTFCELL1: the cell export's view of the open. With a sink set, the build hands it every placement it
 *  loads -- the model as the cell view loaded it (material swap applied, sky cards and markers already out) and
 *  the placement's world transform -- and stops after the model loop: no weld, no ground, no GI. */
struct NativeSrcShape;
struct CellDecalRef;
struct CellExportPlaced
{
	quint32 ref = 0, base = 0;
	int part = -1;                  //!< SCOL part index, -1 for a plain reference
	QString model;                  //!< the model path; with a swap the key is "model|swap"
	QString key;                    //!< the load cache key: one per distinct model + swap
	quint32 swap = 0;
	quint32 paintSwap = 0;          //!< the placement's MSWP even when it substitutes nothing: its CNAMs pick palette rows
	float pos[3] = { 0, 0, 0 };     //!< world units
	float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   //!< row-major, world = R * (local * scale) + pos
	float scale = 1.0f;
	int cellX = 0, cellY = 0;
	const std::vector<NativeSrcShape> * shapes = nullptr;   //!< model space; valid during the call
};
class CellExportSink
{
public:
	virtual ~CellExportSink() = default;
	virtual void begin( const EsmWorld & world, const QString & dataRoot ) = 0;
	virtual void placed( const CellExportPlaced & p ) = 0;
	//! the placed decal references (TXST bases), once, after the last placement
	virtual void decals( const std::vector<CellDecalRef> & refs ) { (void)refs; }
};
void cellExportSetSink( CellExportSink * sink );

/*! lane LIVESET1: the far LOD and the probe markers follow a live set in place (the document's last sections).
 *  0 = nothing moved, 1 = rebuilt (`why` says what), 2 = a reopen is needed (`why` says which input). */
int cellLiveSections( NifModel * nif, QString * why );
//! lane LIVESET1: the PRTP band's Show probes in place; 1 done, 2 a reopen is needed
int cellLiveProbesShow( NifModel * nif, bool show, QString * why );
/*! lane LIVESET4: the menu rows Far LOD, Far LOD source, Actors outdoors and Weapon mod parts (and, lane FAR6, Game LOD trees sway) in place (their values
 *  against the build's); 0 nothing moved, 1 rebuilt, 2 a reopen is needed (`why` says why) */
int cellLiveRebuild( NifModel * nif, QString * why );

/*! The Particles row (WW/CellParticles, env WW_CELL_PARTICLES, default off): copy a placed
 *  model's particle systems into the cell when it opens. */
bool wwCellParticlesOn();
void wwCellParticlesSetOn( bool on );

struct ProbeSoup;
/*! lane LIGHTPHYS4: a change of the light rows rebakes the far light's layers (WW_CELL_FARLIGHT) under the new curve
 *  and republishes the far tables; probesky's tick calls it with its kept soup. true = it relit. */
bool cellFarLightRelit( const void * nif, const ProbeSoup & soup, const QString & giBakeDir );

#endif // CELLVIEW_H
