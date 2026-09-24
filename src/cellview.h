/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLVIEW_H
#define CELLVIEW_H

#include <QString>
#include <QStringList>
#include <QVector>

class NifModel;

/* ---------------------------------------------------------------------------
 * THE CELL VIEW -- one exterior cell, or an N x N block of them, opened the way
 * the Creation Kit's render window shows it.
 *
 * bungo, 2026-09-19, while asking for whole-BUILDING LODs: "I think a
 * prerequisite would be, to be able to view a whole cell like the CK editor".
 * He wants to SEE what a building is made of, in place, before any generator
 * decides what to do with it.  So this is a READ-ONLY viewer: it never writes a
 * plugin, never writes a bake, and has no save path.
 *
 * ===========================================================================
 * 1. WHAT IT IS BUILT OUT OF, AND WHY THERE IS NO NEW DRAW PATH
 * ===========================================================================
 *
 * The scene is a NifModel DOCUMENT, exactly as `.lodi` and `.lodt` already open
 * (src/lodinative.cpp, src/btdterrain.cpp): one NiNode root, then BSTriShapes
 * carrying world-space geometry, with the same BSLightingShaderProperty /
 * BSShaderTextureSet plumbing the chunk builder writes.  Nothing in
 * src/gl/ changes, no shader is added, and the viewport, the navigation, the
 * lighting and the screenshot hook are the ones the fork already has.
 *
 * That choice was made from a MEASUREMENT, not from taste
 * (scratchpad/cellview1_20260919/cell_census.py + tri_budget.py against
 * X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm, 2026-09-19):
 *
 *   block (Commonwealth)      refrs   drawn  distinct  unique tris  welded tris
 *   5,-11 1x1 (downtown)       1483    1410       375      444,296      741,660
 *   5,-11 3x3                  5325    5057      1163    1,443,135    4,006,174
 *   5,-11 5x5                 12091   11540      2090    2,565,540    9,839,158
 *   -20,7 1x1 (Sanctuary)       142     140       123      152,737      230,965
 *   -30,-30 1x1 (wilderness)     74      74        23       26,632       77,584
 *
 * VRAM at 32 bytes a vertex + 6 bytes a triangle: the 5x5 downtown block is
 * 96.6 MB if each distinct model is resident once, 365.3 MB welded per
 * placement.  A welded 5x5 is therefore expensive but not impossible, and every
 * smaller case is cheap -- a 3x3 downtown is 150 MB and Sanctuary is 8 MB.
 *
 * SO: the scene WELDS, like a `.BTO` does, and the refusal thresholds below are
 * what stop a mistake rather than a promise that 5x5 is comfortable.  This is a
 * DIVERGENCE from the brief's "instanced draws" and it is stated rather than
 * hidden: there is ONE LOAD per distinct model (the model cache below, and
 * `lodgenNativeLoadModel` behind it), but the DRAW is merged, because making it
 * a true instanced draw means editing the renderer, and on 2026-09-19 the
 * renderer files belong to another lane.  The way to the instanced version is
 * open and nothing here blocks it: every placement is already written down in
 * the pick table with its own transform.
 *
 * DIVERGENCE FROM THE CK, stated:
 *   - the CK picks by triangle; this picks by the placement's world AABB,
 *     nearest along the ray (src/cellpick.h).  A ref hidden entirely behind
 *     another ref's box can therefore be picked in front of it, and the panel
 *     says which ref it chose rather than implying it was the only candidate.
 *   - the CK draws LIGH, markers, navmesh, regions, roads and the havok layer.
 *     This draws none of those except LIGH-with-a-model, and the census names
 *     every record type it skipped, with counts, so nothing is silently absent.
 *   - the CK edits.  This does not.
 *
 * ===========================================================================
 * 2. WHAT IS DRAWN
 * ===========================================================================
 *
 * Every persistent and temporary REFR of the chosen cells whose base is one of
 *
 *   STAT  SCOL  MSTT  FURN  CONT  DOOR  ACTI  TREE  FLOR  LIGH
 *
 * and whose base names a MODL.  SCOL is expanded to its parts through
 * `EsmWorld::scolParts`, with the same transform composition the chunk builder
 * uses (src/lodgen.cpp ~3474: `R = fromEuler(-x,-y,-z)`, part position rotated
 * by the ref and scaled by the ref's scale).  A base with no model is counted
 * by record type and skipped; so is a deleted ref.
 *
 * Initially-disabled refs, and refs whose XESP enable parent puts them in the
 * opposite state, are HIDDEN by default and drawn tinted when asked for.
 * Markers (the base's model is one of the marker meshes) are hidden by default.
 *
 * Under it: the LAND heights of each cell as a 32x32 quad grid with the
 * landscape vertex colour, and a water plane at the cell's resolved water
 * height.  The base texture layers are NOT sampled -- that needs the splat
 * compositor the terrain bake owns, and a viewer that quietly showed untextured
 * ground while claiming otherwise would be worse than one that says so.  The
 * ground is therefore flat-shaded with VCLR, and the census line says it.
 *
 * ===========================================================================
 * 3. THE OVERLAYS
 * ===========================================================================
 *
 * One at a time, written into the vertex colour at the point the placement is
 * welded -- the same seam `WW_LODL_CHANNEL` uses for the native LOD scene, so
 * there is no second colouring path.  Deterministic: the colour of a key is a
 * hash of the key, so two runs and two machines agree, and the legend the
 * census prints names every bucket with its colour.
 *
 * ===========================================================================
 * 4. HOW IT IS OPENED
 * ===========================================================================
 *
 *   - a `.wwcell` file: one line, `plugins|worldspace|x,y|n[|overlay]`, which
 *     is also what File > Open Cell... writes.  It is a real file so that the
 *     view is reproducible, scriptable and re-openable, and so that opening one
 *     goes down the SAME suffix branch every other built document goes down.
 *   - `WW_CELL_OPEN=<plugins>|<world>|<x>,<y>|<n>` for the harness, which
 *     overrides the file's line when both are present, and says so in the notes.
 *
 * `WW_CELL_DUMP=<file>` writes one row per DRAWN placement (ref form, base
 * form, part, world position, model).  A gate cannot count placements by
 * looking at welded geometry -- the same reason `WW_LODI_DUMP` exists -- so the
 * builder writes down what it placed and the gate compares that against an
 * independent Python walk of the same plugin.
 * --------------------------------------------------------------------------- */

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
	bool showDisabled = false;  //!< draw initially-disabled / enable-parent-off refs, tinted
	bool showMarkers = false;
	bool terrain = true;
	bool water = true;
	bool grid = true;       //!< the cell grid overlay
	bool valid = false;

	//! The inclusive cell rectangle `n` asks for around (cx, cy).
	void rect( int & x0, int & y0, int & x1, int & y1 ) const;
};

//! Parse `plugins|world|x,y|n[|overlay]`. False (with `*error`) on a bad line.
bool cellSpecFromLine( const QString & line, CellSceneSpec & spec, QString * error );
//! Read a `.wwcell` file's first non-comment line.
bool cellSpecFromFile( const QString & path, CellSceneSpec & spec, QString * error );
/*! `WW_CELL_OPEN`, plus `WW_CELL_OVERLAY`, `WW_CELL_DISABLED`, `WW_CELL_MARKERS`,
 *  `WW_CELL_DATAROOT`, `WW_CELL_LODI`, `WW_CELL_NOTERRAIN`, `WW_CELL_NOWATER`,
 *  `WW_CELL_NOGRID`, each read on its own. False when `WW_CELL_OPEN` is unset:
 *  the others then still apply to a spec that came from a file. */
bool cellSpecFromEnv( CellSceneSpec & spec, QString * error );
//! Apply only the modifier variables to an existing spec (see above).
void cellApplyEnvModifiers( CellSceneSpec & spec );

/*! Build the document. `notes` receives what the scene MEASURED -- refs read,
 *  refs drawn, refs hidden, skipped counts per record type, distinct models,
 *  shapes, vertices, triangles, the ground and water facts, the overlay legend
 *  and the load time -- so a picture is never the only evidence. */
bool nifCreateCellScene( NifModel * nif, const CellSceneSpec & spec,
	QString * error, QString * notes = nullptr );

#endif // CELLVIEW_H
