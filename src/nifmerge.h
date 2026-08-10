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
	/*! Imported effect objects whose name was already taken and had to be
	 *  qualified with their attach node. A sequence addresses what it drives by
	 *  NAME, so two `LightningBolt_01`s in one file is one of them animating and
	 *  the other standing still. See uniquifyEffectNames. */
	int nodesRenamed = 0;
	//! NiControllerManagers folded into the one on the root. A NIF carries one;
	//! every merged ArtObject brings another. See animgraph.h.
	int managersFolded = 0;
	//! Sequences that already existed by name and were merged into rather than
	//! added — six ArtObjects each bring an "autoPlay".
	int sequencesFused = 0;
	//! The sequences the file ends up with, after the fold.
	QStringList sequenceNames;
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

/*! @name Weapon parts
 *
 *  A Loaded NIFs row can be marked as a WEAPON PART, the third row mark beside
 *  the skeleton skull and the face donor. Unlike those two the mark is a SET —
 *  a Fallout 4 gun is a base NIF plus separate OMOD part files, and all of them
 *  are weapon parts at once — and the workspace merge reads it to hang each
 *  marked donor off the target's "WEAPON" bone instead of its root.
 *
 *  The registry lives here rather than in the window, because the merge is the
 *  only thing that acts on it and this file has no GUI to drag along. The models
 *  are held weakly; a document that goes away takes its mark with it.
 *
 *  NOTHING HERE KNOWS WHAT A "VALID" GUN IS. Parts may be combined in any
 *  combination, cross-weapon included; the checks below are STRUCTURAL and
 *  purely informational — what the two files carry, never whether the pairing is
 *  one Bethesda shipped. There is deliberately no table of families or legal
 *  combinations, and no API here should grow into one.
 */
///@{
//! Mark or unmark \a model as a weapon part.
void nifSetWeaponMark( NifModel * model, bool marked );
bool nifIsWeaponMarked( const NifModel * model );
//! Marks still held by a live model; prunes the dead ones as it counts.
int nifWeaponMarkCount();
void nifClearWeaponMarks();

//! "WEAPON" when \a target carries a NiNode of that name at ANY depth, else "".
/*! The name a weapon part attaches to. Empty is the answer for a target with no
 *  weapon bone at all, and the caller falls back to an ordinary root merge. */
QString nifWeaponAttachNode( const NifModel * target );

//! What a weapon part's own contents say about merging it onto \a attachNode.
/*! Zero, one or two informational lines, computed BEFORE the splice (they read
 *  the target as it stands). Never refuses anything.
 *
 *   - REDUNDANCY: shape names the donor brings that \a target already carries.
 *     The merge de-duplicates NiNodes by name but not shapes, so a part whose
 *     geometry names are already present is either the same part twice or a
 *     second one of something the gun already has (two barrels).
 *   - SLOT: Fallout 4 parts declare where they belong in
 *     BSConnectPoint::Children as "C-<Slot>", and the file they attach to offers
 *     the matching "P-<Slot>" in BSConnectPoint::Parents. When that point sits
 *     ON the attach node (grips, magazines) the part self-assembles by mere
 *     parenting; when it sits out along the gun (scopes, muzzle devices) or is
 *     not offered at all, parenting leaves the part at the grip. This says so —
 *     unless the target offers no connect points whatsoever, which means it is a
 *     rig rather than a half-built gun and the part arriving is the first thing
 *     on the node.
 *     It does NOT place the part: the slot table is a separate effort, and the
 *     job here is to not lie about the parts that cannot be placed yet.
 */
QStringList nifWeaponPartNotes( const NifModel * target, const NifModel * donor,
                                const QString & label, const QString & attachNode );

//! The summary text of the last workspace merge, for scripting and the harness.
/*! The merge ends in a modal box, which a driver has to dismiss to let the run
 *  continue and therefore cannot read. The text is kept here so an assertion can
 *  be made about what the merge SAID, not only about what it did. */
void nifSetLastMergeSummary( const QString & text );
QString nifLastMergeSummary();
///@}

#endif // NIFMERGE_H
