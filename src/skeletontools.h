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

#endif // SKELETONTOOLS_H
