/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef SKELETONTOOLS_H
#define SKELETONTOOLS_H

#include <QList>
#include <QString>
#include <QStringList>

class NifModel;

/*! \file skeletontools.h
 * \brief Skeleton Manager analysis, model-layer only.
 *
 * Deliberately independent of the dock so the CLI (`skeleton` in nifcli.cpp) and
 * the UI read the same numbers — SKELETON_AND_POSE_PLAN.md §A.8. Everything here
 * is read-only; phase 1 of §A.7 writes nothing, so nothing it does can corrupt a
 * file.
 */

//! One node in the skeleton tree, with how much of the skin it actually drives.
struct SkeletonBoneInfo
{
	int block = -1;             //!< block number of the node
	QString name;               //!< node Name (may be empty)
	int parent = -1;            //!< parent node's block, -1 for a root
	int depth = 0;              //!< hierarchy depth, for flat rendering

	bool inSkin = false;        //!< named in some skin's Bones array
	int shapes = 0;             //!< skinned shapes that list this bone
	int verts = 0;              //!< vertices weighted to it above the threshold
	double weight = 0.0;        //!< summed weight across those vertices

	//! A bone the skin declares but no vertex actually uses. These are what
	//! "Prune unused bones" (phase 2) would remove; harmless but they cost a
	//! bone slot, and FO4 has a hard per-partition limit.
	bool isUnusedBone() const { return inSkin && verts == 0; }
	//! Not referenced by any skin at all — a plain scene node (camera, attach
	//! point, effect marker). Shown greyed rather than hidden, because "is this
	//! node a bone?" is exactly the question the dock exists to answer.
	bool isNotABone() const { return !inSkin; }
};

//! Whole-file skeleton picture. Built by skeletonAnalyse().
struct SkeletonReport
{
	QList<SkeletonBoneInfo> bones;   //!< every node block, parents before children
	int rootBlock = -1;              //!< first root node found
	int skinnedShapes = 0;           //!< skinned shapes contributing weights

	//! A skin's Bones array entry that does not resolve to a node block. This is
	//! a real corruption class — the game looks the bone up by index and finds
	//! nothing.
	QStringList danglingSkinBones;
	//! Names carried by more than one node. Bone lookup is by name in every
	//! tool here (and in Outfit Studio), so duplicates are ambiguous.
	QStringList duplicateNames;

	int deformingCount() const;
	int unusedCount() const;
};

/*! Analyse a model's skeleton.
 *
 * Handles both skin backends: FO4-style `BSSkin::Instance` + `BSSkin::BoneData`
 * with per-vertex `Bone Weights`/`Bone Indices` on `BSTriShape`, and the classic
 * `NiSkinInstance` + `NiSkinData` with per-bone `Vertex Weights` lists. Weight
 * counts come from the vertex data, not from the bone list, because a bone
 * being *listed* says nothing about whether anything is bound to it.
 *
 * \param nif       model to inspect; may be null (returns an empty report)
 * \param threshold weights at or below this do not count as influence
 */
SkeletonReport skeletonAnalyse( const NifModel * nif, float threshold = 0.0001f );

// ---------------------------------------------------------------------------
// bone operations (src/skeletonops.cpp)
// ---------------------------------------------------------------------------

class Transform;

//! One place a bone is referenced from. Produced by skeletonBoneRefs().
struct SkeletonBoneRef
{
	QString what;      //!< short kind: "child of", "skin bone", "bone LOD", ...
	int block = -1;    //!< the referencing block
	QString detail;    //!< human-readable location, says when it is by NAME
};

/*! Every reference to a bone: parent Children link, skin `Bones` slots (whose
 * ORDER is the vertex bone-index mapping), bone-LOD names, controller-sequence
 * node names, connect-point parents, and its collision object.
 *
 * This is the foundation every write operation goes through — a rename or delete
 * that misses one of these corrupts the file.
 */
QList<SkeletonBoneRef> skeletonBoneRefs( const NifModel * nif, int boneBlock );

/*! True when the file carries a `bhkRagdollSystem`.
 *
 * That block is a compiled Havok packfile binding physics bodies to nodes.
 * `hknpdecode` can read it but there is no proven encoder for ragdoll bodies and
 * constraints, so any structural bone change leaves it stale — callers must warn.
 */
bool skeletonFileHasRagdoll( const NifModel * nif );

//! Mirror a bone name across the midline (`_L_`/`_R_`, `.L`/`.R`, `Left`/`Right`,
//! leading `L`/`R`). Returns it unchanged when the bone has no side.
QString skeletonFlipBoneName( const QString & name );

//! Names in the same family as this one — same stem, differing only in trailing
//! digits (LArm1/LArm2/LArm3). Empty when the name has no trailing digits.
QStringList skeletonSimilarNames( const NifModel * nif, const QString & name );

//! A bone's world transform, from the product of local transforms to the root.
Transform skeletonWorldTransform( const NifModel * nif, int block );

//! Rename a bone and every by-NAME reference to it (bone LOD, sequences, connect
//! points). By-link references need no change. Does NOT touch the ragdoll.
bool skeletonRenameBone( NifModel * nif, int block, const QString & newName, QString * error );

//! Blender's extrude: a new child NiNode offset along the parent's local +Y.
//! Returns the new block, or -1.
int skeletonAddChildBone( NifModel * nif, int parentBlock, const QString & name, float length );

//! Move a bone under a new parent. keepTransform preserves its world position by
//! recomputing the local transform; otherwise the local transform is kept as-is
//! (Blender's Keep Offset), which moves the bone.
bool skeletonReparent( NifModel * nif, int block, int newParent, bool keepTransform, QString * error );

//! Remove a bone, its children adopted by its parent at their current world
//! positions (Blender's dissolve). Refuses on the root.
bool skeletonDissolve( NifModel * nif, int block, QString * error );

/*! Mirror a bone (optionally its whole subtree) to the opposite side.
 *
 * The mirror is named flip(name), placed at the world transform reflected across
 * the YZ plane, and parented under the mirror of the source's parent when one
 * exists. Rotation is mirrored by conjugation (M R M) so the result stays a
 * PROPER rotation — negating a single column would give a left-handed basis and
 * animation would rotate the wrong way.
 *
 * updateExisting = false is Duplicate Mirrored (refuses if the target exists);
 * true is Symmetrize (overwrites it).
 */
int skeletonMirrorBone( NifModel * nif, int block, bool wholeSubtree, bool updateExisting,
						QString * error );

//! Remove a bone and everything under it.
bool skeletonDeleteSubtree( NifModel * nif, int block, QString * error );

#endif // SKELETONTOOLS_H
