/* The CHARACTER glTF export: several NIFs, one skeleton, one clip, one file.
   Lane GLTFEXPORT1, 2026-09-19.

   src/gltfexportnif.cpp turns ONE NifModel into a GltfExportScene. That is the
   whole of today's export, and it is why bungo's Blender session went the way
   it did on 2026-09-19: a body NIF carries a FLAT list of its *_skin bones and
   no hierarchy, so a clip drove 13 of 95 tracks; the joint list held only the
   bones the skin weights, so Blender built several armatures and 130 loose
   empties; the head, hands, eyes and mouth were five more files; and the whole
   thing came out in metres when PyNifly wanted game units.

   Each of those was patched up afterwards by a script in
   E:/Projects/Claude/tools/gltf_units (gltf_merge_skin.py, gltf_unify_skin.py,
   gltf_to_game_units.py, bake_empties_to_armature.py). This file is those
   scripts moved INTO the exporter, driven by GltfExportOptions, so the
   workaround chain retires.

   It calls gltfExportNifScene() once per NIF and merges the results by NODE
   NAME. Nothing in gltfexportnif.cpp changes, so a legacy export still goes
   down the old path byte for byte -- gltfExportOptionsAreLegacy() is the
   switch, and the gate pins it. */

#ifndef GLTFEXPORTCHAR_H
#define GLTFEXPORTCHAR_H

#include "gltfexportnif.h"
#include "gltfexportopts.h"

class NifModel;

//! What the character export decided, on top of GltfExportReport. Every count
//! is written by gltfExportCharacter() and moves when its input moves --
//! CONSTITUTION 4, the three rules of 2026-09-04 21:33, rule 1.
struct GltfExportCharReport
{
	int skeletonNodes = 0;        //!< nodes the skeleton NIF contributed
	int partsMerged = 0;          //!< extra NIFs merged in (0 = the open file alone)
	int nodesMatchedByName = 0;   //!< a part's node that the skeleton already had
	int nodesAdded = 0;           //!< a part's node the skeleton did NOT have
	int helpersDropped = 0;       //!< option (6)
	int helpersKeptWeighted = 0;  //!< helper bones a skin weights, never dropped
	int helperTracksDropped = 0;  //!< clip channels that pointed at a dropped helper
	int jointsPerSkin = 0;        //!< option (3); 0 when each skin keeps its own
	int skinsUnified = 0;
	int inverseBindsDerived = 0;  //!< joints with no stored bind, derived from the world matrix
	int texturesCopied = 0;
	int texturesMissing = 0;
	int buildBonesScaled = 0;     //!< Part 2 bake
	int buildCycleChannels = 0;
	/*! `--skeleton auto` (the default since bungo's ruling of 2026-09-19 09:45)
	 *  looked and found nothing, so the file's own nodes were used. NOT an
	 *  error -- a static or a weapon has no CharacterAssets beside it -- but it
	 *  is reported on the MEASURED line and in the notes, because the point of
	 *  the default is that a character comes out rigged and a reader must be
	 *  able to see when one did not. */
	bool skeletonAutoMissed = false;
	QStringList unmatchedBones;   //!< a part's skin bone with no node anywhere -- NAMED, never dropped silently
	QStringList notes;
};

/*! The whole export. `clip` may be null.
 *
 * @param nif       the open model (the body, usually).
 * @param iRoot     a node to export, or invalid for every root.
 * @param clip      the clip to write, or null.
 * @param boneNames the clip's bone names in BONE-INDEX order (see
 *                  gltfExportNifClip); empty when the clip names its own.
 * @param opts      the one options struct. When gltfExportOptionsAreLegacy()
 *                  is true this forwards to gltfExportNifWrite() unchanged.
 */
bool gltfExportCharacter( const NifModel * nif, const QModelIndex & iRoot,
						  const HkxAnimClip * clip, const QStringList & boneNames,
						  const GltfExportOptions & opts, const QString & gltfPath,
						  GltfExportReport & report, GltfExportCharReport & charReport,
						  QString & error );

/*! Where option (2)'s "auto" looks for skeleton.nif, in order:
 *    1. the NIF's own folder,
 *    2. ../CharacterAssets and ./CharacterAssets beside it,
 *    3. <dataRoot>/Meshes/Actors/Character/CharacterAssets/skeleton.nif,
 *    4. the same under every folder the game manager serves.
 *  Returns the first that exists, or an empty string -- and an empty string is
 *  a REFUSAL with a sentence, never a silent fall back to "none".
 */
QString gltfExportFindSkeleton( const QString & nifPath, const QString & dataRoot,
								QStringList * looked = nullptr );

/*! Part 2's bake. Multiplies the exported bone SCALE of every `*_skin` node
 *  the RACE table names by the game's own combination of the three corners
 *  (src/bodybuild.h, measured from Fallout4.exe 1.10.155), and, when
 *  `opts.buildCycleClip` is set, appends the 6-second
 *  THIN -> MUSCULAR -> FAT -> THIN cycle as a glTF animation with SCALE
 *  channels only. A bone the table names and the file does not have is NAMED
 *  in `cr.unmatchedBones`, never dropped in silence. */
bool gltfExportApplyBodyBuild( GltfExportScene & scene, const QHash<QString, int> & byName,
							   const GltfExportOptions & opts,
							   GltfExportCharReport & cr, QString & error );

/*! The bone-name rule of option (6), counted. Pass the node names of a
 *  skeleton and get back how many are helpers -- the number the ROWS FOR BUNGO
 *  table quotes, produced by the same predicate the exporter uses so the
 *  table cannot drift from the code. */
int gltfExportCountHelpers( const QStringList & nodeNames, QStringList * helpers = nullptr );

#endif // GLTFEXPORTCHAR_H
