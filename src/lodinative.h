/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODINATIVE_H
#define LODINATIVE_H

#include <QModelIndex>
#include <QSet>
#include <QString>

class NifModel;

/* -------------------------------------------------------------------------
 * `.lodi` + `.lodo` — the FO4CS-native object LOD, opened as a built document.
 *
 * A `.lodi` stores no triangles either: it is a worldspace's PLACEMENT table,
 * and the geometry those placements point at lives in the `.lodo` LIBRARY
 * beside it (docs/LODGEN_NATIVE_LODO_LODI.md). So "opening" one means the same
 * thing it means for a `.btd` or a `.lodl` — choose a region, choose a detail
 * level and BUILD the scene here, out of the two files and nothing else.
 *
 * The readers are `src/lodifile.h` and `src/lodofile.h` and this file does not
 * contain a second one. Nothing here writes a `.lodi`, a `.lodo` or any other
 * bake output.
 *
 * The shape of the built scene is the `.BTO`'s, on purpose: one NiNode root,
 * then one BSTriShape per (base, material) bucket carrying every placement of
 * that base welded into it in WORLD space, with the same BSLightingShaderProperty
 * plumbing the chunk builder writes (src/lodgen.cpp ~4189). A per-placement node
 * tree would be 3,526 nodes on one region and would not answer any question a
 * bucket does not.
 *
 * WHERE THE INSTANCE CENSUS COMES FROM. A gate cannot count placements by
 * looking at merged geometry, and a screen coordinate is not carried between
 * builds (`nifskope-ww-render-shot`, the SKEL2 corollary). So the builder that
 * places them WRITES DOWN where it put them: `WW_LODI_DUMP=<file>` gets one row
 * per placed instance, and that file is what the harness compares against the
 * `.lodi` reader and the chunk manifest.
 * ------------------------------------------------------------------------- */

//! One `.lodi` view: an inclusive cell rectangle, a cluster-ladder level and
//! whether the occluder boxes are drawn.
struct LodiSceneSpec
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	//! false = the file's whole extent, filled in by the builder.
	bool haveRegion = false;
	/*! The cluster-ladder level to draw, 0 = full detail (`LodoClusterLod::level`).
	 *  A mesh with fewer levels than this draws its own coarsest. */
	int level = 0;
	//! Occluder boxes are drawn ONLY when this is set, and then as wire boxes.
	bool boxes = false;
	bool valid = false;

	/* ---- lane FARLOD1: the cell view's far rings. Every field is inert at its
	 *  default, so the `.lodi` views and the BAKEBLOCK soup build what they built. */
	//! placements whose cell is inside this inclusive rectangle are skipped (the loaded block / inner ring)
	bool haveHole = false;
	int hx0 = 0, hy0 = 0, hx1 = -1, hy1 = -1;
	//! 0..3 = draw only this AUTHORED slot (wins over WW_LODI_SLOT); -1 = as before
	int slot = -1;
	//! put in front of every bucket's shape name
	QString namePrefix;
	//! subtracted from every shape's Translation: the cell scene's own origin (it is origin-relative)
	float shift[3] = { 0.0f, 0.0f, 0.0f };
	//! > 0: a placement whose materials are all TREE materials is dropped past this many units
	//! from (dropX, dropY) -- the game's fTreeLoadDistance
	float dropTreesBeyond = 0.0f;
	float dropX = 0.0f, dropY = 0.0f;
	//! TREE materials carry the `.lodo` sway weight as vertex alpha and SLSF2 Tree_Anim
	bool sway = false;
	//! REFR form ids drawn by somebody else (the impostor cards); skipped here
	const QSet<quint32> * skipRefs = nullptr;
	//! > 0: when the placements come to more triangles than this, nothing is written to the document
	//! (LodiAppendCounts::overBudget; tris = what it would have been) -- the far field's memory budget
	qint64 maxTris = 0;
	/*! lane FARLOD2: the shapes keep their vertex and triangle arrays BESIDE the document (src/cellmesh.h)
	 *  instead of as rows -- the far rings' representation. Ignored under WW_CELL_SPEED_RED=slow. */
	bool sideMesh = false;
};

//! lane FARLOD1: what one append put in, counted at the push.
struct LodiAppendCounts
{
	qint64 placed = 0, holeSkipped = 0, treesDropped = 0, refsSkipped = 0;
	qint64 tris = 0, swayVerts = 0;
	bool overBudget = false;   //!< spec.maxTris refused the write
};

//! `WW_LODI_REGION="x0,y0,x1,y1"` (cells), `WW_LODI_LEVEL=n`, `WW_LODI_BOXES=1`.
//! False when none of them is set; each is read on its own.
bool lodiSpecFromEnv( LodiSceneSpec & spec );

/* -------------------------------------------------------------------------
 * WW_LODL_CHANNEL=<name> -- ONE baked channel drawn on its own.
 *
 * `WW_LODL_AO=1` showed one of the bake's channels (the AO) by writing it into
 * the vertex colour at the point the scene is built. Every other baked channel
 * is just as invisible in a normal capture, and bungo asked for them by name
 * (2026-09-18: "Beyond the AO, you will also now show me on the same chunk:
 * Leaf sway bake, identity bake, sky visibility bake" and, the same round,
 * "show me also ground contact blend, ground cover, and roughness / metallic").
 * This switch is that seam widened: the SAME point in the SAME builders, one
 * name at a time, and every name writes a note line carrying what was actually
 * put in the buffer.
 *
 * `ao` IS `WW_LODL_AO=1` -- the same code, the same bytes -- so the way back is
 * a name and not a second path. An unknown name refuses BY NAME and draws the
 * default scene.
 *
 * Objects (src/lodinative.cpp) carry `identity`..`ao` as vertex colour;
 * `mask-*`, `emissive` and `normal` are the terrain's sheets
 * (src/btdterrain.cpp, through the LodtSheets sampler and no second decoder);
 * `ground` colours both, so the placements' blend can be read against the
 * surface they blend into.
 * ------------------------------------------------------------------------- */
enum class LodlChannel
{
	None = 0,       //!< unset, or a name that is not known
	/*! v7: the GROUP, hashed (bungo 2026-09-18: "The houses should be one
	 *  object each though, for identity"). On a version-6 file there is no
	 *  group table and this falls back to the placement identity, which the
	 *  note line SAYS by name rather than drawing it silently. */
	Identity,
	Placement,      //!< the unique per-placement identity, hashed -- what `identity` drew before v7
	IdentityRaw,    //!< `placement-lowbyte`: the placed piece's id, low byte as grey (was `identityraw`)
	Sky,            //!< `.lodi` sky visibility, per placement
	Ground,         //!< `.lodi` ground-contact blend, per placement (+ the terrain)
	Seed,           //!< `.lodi` tree seed, hashed; 0 (not a tree) is black
	Sway,           //!< `.lodo` per-vertex wind-sway weight
	SelfAo,         //!< `.lodo` per-vertex self-AO, alone
	Ao,             //!< exactly WW_LODL_AO=1
	MaskR,          //!< mask sheet R: roughness
	MaskG,          //!< mask sheet G: metallic
	MaskB,          //!< mask sheet B: sky AO (what WW_LODL_AO reads on the terrain)
	MaskA,          //!< mask sheet A: ground cover (BC3 cover tiles only)
	Emissive,       //!< the emissive sheet (role 6), when the container has one
	Normal,         //!< the model-space normal sheet as RGB, unlit
	/*! v9: the WORKSHOP-SCRAPPABLE bit (`.lodi` 0x14 bit 6), per placement.
	 *  Magenta = the player can scrap it and it will not be there; grey = it
	 *  stays. The picture is the refuter for the rule in `EsmScrapIndex`: a
	 *  settlement whose whole street comes back magenta means the build-area
	 *  box test is wrong, and no census number says that as fast as one shot
	 *  does. On a file below version 9 nothing is magenta and the note line
	 *  SAYS the version rather than drawing an all-grey picture in silence. */
	Scrappable
};

/* THE SUN (`WW_SUN`) AND THE HORIZON CHANNELS WERE REMOVED 2026-09-19 by lane
 * HORIZONOUT. They existed to draw the baked far shadow the `.lodi` v8 stream
 * carried, and that route was dropped on bungo's word the same day: the far
 * shadow map keys on the `.lodi` GROUP id now, which is what the `identity`
 * channel draws. `release/NifSkope.before_horizonout.exe` still has both
 * channels and still bakes the stream they read. The two numbers that ended
 * the route are in docs/LODGEN_NATIVE_LODO_LODI.md's history paragraph. */

//! The channel `WW_LODL_CHANNEL` names. `None` when unset or unknown; `*given`
//! receives the raw string, so an unknown name can be refused BY NAME.
LodlChannel lodlChannelFromEnv( QString * given = nullptr, int * bin = nullptr );

//! The canonical name of a channel ("ao", "mask-r", ...); empty for `None`.
QString lodlChannelName( LodlChannel c );

//! Every name this switch accepts, in the order the docs list them.
QString lodlChannelNames();

/*! Build a Fallout 4 document whose geometry is the region's object LOD.
 *
 *  `notes`, when given, receives what the scene MEASURED — placements read,
 *  placements drawn, bases and materials used, the cluster level each mesh
 *  actually had — so a picture is never the only evidence.
 */
bool nifCreateLodiObjectScene( NifModel * nif, const QString & lodiPath,
	const LodiSceneSpec & spec, QString * error, QString * notes = nullptr );

/*! The same objects APPENDED to a document somebody else created, under
 *  `iRoot`. This is how one picture can carry the terrain from a `.lodl` and
 *  the objects from a `.lodi` at once; the caller owns `createNew` and
 *  `updateModel`, and must hold updates around the call as the terrain builder
 *  already does. */
bool nifAppendLodiObjects( NifModel * nif, const QModelIndex & iRoot,
	const QString & lodiPath, const LodiSceneSpec & spec,
	QString * error, QString * notes = nullptr, LodiAppendCounts * counts = nullptr );

#endif // LODINATIVE_H
