/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef NIFMERGE_H
#define NIFMERGE_H

#include <QString>
#include <QStringList>
#include <QByteArray>

class NifModel;

//! @file nifmerge.h Merge one NIF's contents into another.
/*!
 * Built for the load-screen workflow: bring several armour pieces (and a bare
 * `skeleton.nif`) into one file so the set can be posed as a unit.
 *
 * The important behaviour is **node de-duplication by name**. A naive branch
 * splice gives every merged piece its own private copy of the bones it is
 * skinned to; the result renders correctly but cannot be posed, because moving
 * "Chest" would mean moving five separate copies of it. Merging maps a donor
 * node onto the target's same-named node instead, and because the splice
 * rewrites links through a donor->target block map, every skin's Bones array is
 * re-pointed at the shared bones for free.
 *
 * Runs on the model layer only — no GUI, no GL — so both the CLI and a future
 * spell can call it.
 */

struct NifMergeResult
{
	int blocksAdded    = 0;   //!< blocks spliced in
	int shapesAdded    = 0;   //!< of those, renderable shapes
	int nodesReused    = 0;   //!< donor nodes matched to an existing node by name
	int nodesAdded     = 0;   //!< donor nodes that had no counterpart
	int reparented     = 0;   //!< imported blocks linked under an existing node
	/*! Existing nodes the donor's hierarchy adopted, whose local transform was
	 *  rewritten so their WORLD transform did not change. Armour and clothing
	 *  store their bones flat with world transforms; a skeleton stores the same
	 *  bones nested with local ones, so without this the mesh folds up. */
	int rebased        = 0;
	/*! Bone names the merge left on more than one node (excluding any the target
	 *  already had). A rig binds bones BY NAME — the pose library, mirroring, and
	 *  this merge's own de-duplication all do — so a repeated name makes one of
	 *  the two nodes unreachable and sends whatever addressed it to the other. It
	 *  should always be empty; it is reported because when it is not, the symptom
	 *  (a rig that poses into a heap) says nothing about the cause. */
	QStringList duplicateNames;
	/*! The node the donor asked to hang from — its "AttachT" NiStringsExtraData
	 *  entry of the form "NamedNode&<name>", or the caller's override. Empty when
	 *  the file says nothing, which is NOT the same as "the root": Fallout 4
	 *  effects often carry only engine hints like "MultiTechnique" and take their
	 *  attach node from the ARTO record in the ESP instead. */
	QString attachRequested;
	//! The node it was actually parented under; empty means the target's root.
	QString attachedTo;
	/*! Nodes that individual top-level branches named for themselves, via the
	 *  `NamedAttach<NodeName>` convention. One donor can populate several — the
	 *  X-01 Tesla arm carries a pauldron arc and a forearm arc — which is exactly
	 *  what AttachT cannot express, since it names one place for the whole file.
	 *  Non-empty means those branches ignored attachRequested and went where they
	 *  said they belong. */
	QStringList namedAttachments;
	/*! Node names the target already had that were imported again anyway, because
	 *  neither side uses them as a bone. Effect files are authored from a shared
	 *  template and reuse names like `LightningBolt_01`; fusing those would hang
	 *  one limb's effects off another's. Non-empty is normal and healthy — it is
	 *  reported because it is the one place the merge deliberately creates a
	 *  duplicate name. */
	QStringList privateNames;
	//! NamedAttach branches whose local transform was rebased onto the node they
	//! named, because those branches are authored in actor space.
	int namedRebased = 0;
	//! The donor carries an "AttachT" extra data, i.e. it is an ArtObject —
	//! true even when that data names no node, which is the case worth warning
	//! about, because such an effect silently lands at the origin.
	bool isEffect = false;
	QString error;            //!< set when the merge returns false
};

//! Merge every branch of \a donorPath into \a target.
/*!
 * \param target       Model to merge into; edited in place under one undo step.
 * \param donorPath    File to read. Must share the target's Bethesda version.
 * \param dedupeByName Match donor NiNodes to same-named target nodes (default).
 *                     Turn off only to keep a verbatim, independent copy.
 * \param result       Counts and, on failure, the reason.
 * \param attachTo     Node to hang the donor's branches from, overriding its own
 *                     "AttachT" request. Needed for the Fallout 4 effects that
 *                     carry no NamedNode entry because the ESP's ARTO record
 *                     holds their attach node. Empty = use what the file says,
 *                     or the target's root if it says nothing.
 * \return true on success.
 */
bool nifMergeFile( NifModel * target, const QString & donorPath,
                   bool dedupeByName, NifMergeResult & result,
                   const QString & attachTo = QString() );

//! Merge a NIF held in memory (e.g. bytes extracted from a game archive).
/*!
 * Same splice and de-duplication as nifMergeFile; the donor comes from a byte
 * buffer instead of a path so archive resources can be merged without unpacking
 * them to disk.
 * \param label Display name for the source (used in messages / the undo step).
 */
bool nifMergeData( NifModel * target, const QByteArray & data, const QString & label,
                   bool dedupeByName, NifMergeResult & result,
                   const QString & attachTo = QString() );

#endif // NIFMERGE_H
