/* Playback and bone MAPPING for Fallout 4 Havok animation clips (.hkx) loaded
   into the Animation workspace, on top of lane HKX1's reader (src/hkxanim.h).

   bungo's ruling, 2026-09-10, verbatim: "in animation workspace, add an option
   to load a hkx file with animation, then they get added to the animations
   list, and if there's rigged geometry with nodes / bone names that match, they
   play".

   HOW IT PLAYS, in one paragraph. A loaded clip becomes a name in
   `Scene::animGroups`, with its start and end in `Scene::animTags`, so it is an
   entry in the animations list beside the NIF's own sequences and the WHOLE
   existing transport -- play, pause, loop, scrub, speed, reverse, cycle -- drives
   it with no new controls. Selecting it goes through `Scene::setSequence`, which
   binds the clip's tracks to the open NIF's NiNodes by name. Per frame,
   `Node::transform()` calls `applyLocal()` immediately after the node's own
   controllers have run and before anything reads a world transform, and it
   writes the decoded transform into `Node::local` -- the same member a
   `NiTransformController` writes, at the same moment, so skinning, the node
   markers, the bounds and the picking update exactly as they do today.

   THE MAPPING is case-insensitive and partial by design: skeleton.hkx and
   skeleton.nif disagree in case on Head, Spine1, Spine2 and Weapon, and 17
   `Weapon*` bones of the animation skeleton have no node in the body NIF at all
   (measured by lane HKX1). Matched bones play; unmatched bones are named in
   words in the summary line; zero matches refuses in words and writes nothing.

   THE NAMES themselves are not in a clip. A clip stores track -> bone INDEX
   (`HkxAnimClip::trackToBone`) against a skeleton it only names, so the bone
   names come from an hkaSkeleton, and `resolveNames()` states in words which of
   its four fallback arms served (CONSTITUTION rule 10, modules and fallbacks).

   Lane HKX2, 2026-09-10. */

#ifndef HKXPLAYBACK_H
#define HKXPLAYBACK_H

#include "hkxanim.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class Node;
class Scene;

//! One list of bone names held against the open NIF's NiNodes.
struct HkxMapping
{
	//! bone names that found a node (INCLUDING the case-folded ones)
	QStringList matched;
	//! bone names with no node in this NIF -- named in the summary line
	QStringList unmatched;
	//! of `matched`, those that matched only after folding case
	QStringList caseFolded;
	//! tracks whose bone index no skeleton names (so they cannot be matched)
	QStringList unnamed;
	//! named nodes the scene offered
	int nodesInNif = 0;

	bool any() const { return !matched.isEmpty(); }
	//! One sentence: how many play, how many do not, and WHICH ones do not.
	QString summary( const QString & what ) const;
	//! THE SAME THING IN A FEW WORDS -- "78 of 95 bones":
	//! bungo, over a screenshot of the old dock's binding paragraph: "look at all this text clutter".
	QString summaryShort() const;
};

//! One clip loaded from disk, living in the animations list.
struct HkxClipEntry
{
	QString name;			//!< the list entry, unique within the scene
	QString path;			//!< the file it was read from
	QString skeletonSource;	//!< which fallback arm gave the bone names, in words
	HkxAnimClip clip;		//!< as lane HKX1's reader decoded it
	QStringList trackBone;	//!< per track, its bone name ("" = no skeleton names it)
	HkxMapping mapping;		//!< recomputed at every bind

	//! Node::id() -> track index, for the per-node hook. Rebuilt at bind.
	QHash<int, int> nodeTrack;
	//! Node::id() -> the local transform the node had BEFORE this clip posed it.
	//! Restored verbatim on unbind, which is gate (b).
	QHash<int, Transform> saved;
	//! Node::id() of the node the extracted root motion is applied to (-1 none)
	int rootNode = -1;
	QString rootNodeName;
	//! blendHint ADDITIVE: the transforms are deltas, composed on `saved`
	bool additive = false;
	bool bound = false;

	// a GENERATED preview clip (src/morphcycle.h):
	// Built in memory from the chargen tables, never read from a file, and NEVER a game clip: skeleton.hkx has
	// no *_skin and no skin_bone_* bones, so no Fallout 4 .hkx can carry what these clips move.
	enum GenMode { GenNone = 0, GenLocalScale = 1, GenSkeletonSpace = 2 };
	QString generated;					//!< "" = a real clip; else the preview's kind, in words
	int genMode = GenNone;
	QVector<int> genParentTrack;		//!< GenSkeletonSpace: per track, its parent's track or -1
	QVector<Transform> genParentRest;	//!< ...and when -1, that parent's rest global
	QVector<Transform> genRestInv;		//!< per track, the inverse of its own rest global
	bool isGenerated() const { return !generated.isEmpty(); }
};

//! Every .hkx clip loaded into one Scene, and the pose of the one that plays.
class HkxPlayback
{
public:
	explicit HkxPlayback( Scene * s ) : scene( s ) {}

	//! Read a .hkx (or an HKXPACK .xml) and add every clip in it to the list.
	QString load( const QString & path, QStringList * added = nullptr );

	//! Drop one clip. Restores every node it posed, byte for byte.
	bool unload( const QString & clipName );
	//! Drop every clip and every remembered skeleton.
	void unloadAll();

	QStringList names() const;
	int count() const { return clips.count(); }
	const HkxClipEntry * find( const QString & clipName ) const;
	bool has( const QString & clipName ) const { return indexOf( clipName ) >= 0; }
	//! Every hkaSkeleton this session has seen, newest last.
	const QVector<HkxSkeleton> & knownSkeletons() const { return skeletons; }

	//! Select the clip that plays, or none. Called from Scene::setSequence.
	bool setActive( const QString & clipName );
	const QString & activeName() const { return active; }

	//! Put every loaded clip's name and time range back into the scene's
	//! animations list, and re-bind the active one against the nodes that are
	//! there NOW. Called from Scene::make() -- a different NIF was opened.
	void onSceneRebuilt();
	//! The nodes are about to be deleted: forget every binding (Scene::clear).
	void onSceneCleared();

	//! Root motion is a separate switch, default off.
	bool rootMotionEnabled() const { return rootMotion; }
	void setRootMotion( bool on );

	//! the animation workspace edits the clip that plays Replace a loaded clip's decoded data with an edited
	//! one (same entry name).
	QString replaceClip( const QString & clipName, const HkxAnimClip & clip, const QStringList & trackNames );
	//! Hold one node out of the pose: while `nodeId` is held, applyLocal() leaves that node's local transform
	//! alone, so the viewport shows what the transform gizmo wrote to it (the pose the user is keying) instead
	//! of the clip's sample.
	void setHeldNode( int nodeId ) { heldNode = nodeId; }
	int heldNode_() const { return heldNode; }
	//! bungo's ruling 5 of 2026-09-12: the Animations list is edited the way Blender edits a list -- reorder,
	//! duplicate, rename, paste.
	bool setOrder( const QStringList & order );
	//! Copy a loaded clip under a unique name, right after it.
	QString duplicateClip( const QString & clipName, QString * newName = nullptr );
	//! Rename a loaded clip, its scene registration and its binding with it.
	QString renameClip( const QString & clipName, const QString & newName );
	//! Put a copy of a clip entry in at `atIndex` under a unique name (paste).
	QString insertClip( const HkxClipEntry & src, int atIndex, QString * newName = nullptr );
	//! Where a clip sits in the list, or -1.
	int indexOfClip( const QString & clipName ) const { return indexOf( clipName ); }
	//! The entry at a list position, or null.
	const HkxClipEntry * clipAt( int i ) const { return ( i >= 0 && i < clips.count() ) ? &clips.at( i ) : nullptr; }

	//! The active entry, or null (the workspace reads nodeTrack / saved).
	const HkxClipEntry * activeEntry() const
	{ return ( activeIndex >= 0 && activeIndex < clips.count() ) ? &clips.at( activeIndex ) : nullptr; }

	//! Cheap gate for the per-node hook: is anything being posed at all?
	bool posing() const { return activeIndex >= 0; }
	//! Per frame, from Node::transform(): pose this node if the clip names it.
	void applyLocal( Node * node ) const;

	//! The decoder's own answer for one track at one time, with no node anywhere near it. This is the
	//! instrument gate (a) reads: the harness compares it against what actually landed in `Node::local`.
	static bool sampleTrack( const HkxAnimClip & clip, int track, float time,
							 HkxTransform & out );

	//! Hold a list of bone names against the scene's named nodes.
	static HkxMapping mapNames( const Scene * scene, const QStringList & boneNames );

	/*! the one sentence every export path refuses a generated
	 *  preview clip with ("" for a real clip or null). One string, so the
	 *  workspace's Save, Save as, its edits and the glTF export cannot drift. */
	static QString generatedRefusal( const HkxClipEntry * e );

	//! The last sentence this playback produced -- what a tooltip or a log shows.
	const QString & summary() const { return lastSummary; }
	//! The same thing in a few words -- what the panel's LABEL shows.
	const QString & summaryShort() const
	{
		return lastShort.isEmpty() ? lastSummary : lastShort;
	}

private:
	int indexOf( const QString & clipName ) const;
	bool bind( HkxClipEntry & e );
	void restore( HkxClipEntry & e );
	void registerInScene( const HkxClipEntry & e );
	//! Put the scene's own animations list in our order.
	void syncSceneOrder();
	//! Fill `e.trackBone` from a skeleton; sets `e.skeletonSource` to the arm.
	void resolveNames( HkxClipEntry & e );
	bool namesFromSkeletons( HkxClipEntry & e );
	bool loadSkeletonBeside( const QString & clipPath );
	//! the local a generated clip puts on this node at time t
	bool generatedPose( const HkxClipEntry & e, const Node * node, int track, float t,
						Transform & out ) const;

	Scene * scene = nullptr;
	QVector<HkxClipEntry> clips;
	QVector<HkxSkeleton> skeletons;
	QString active;
	int activeIndex = -1;
	bool rootMotion = false;
	QString lastSummary;
	QString lastShort;		//!< the few-word form of lastSummary, "" when there is none
	int heldNode = -1;		//!< the node the gizmo is posing, left alone by applyLocal
};

//! WW_HKXANIM_TEST: lane HKX2's gates, run inside the real application. Defined in src/hkxplaybacktest.cpp;
//! called once from the UI setup so this lane's footprint in nifskope_ui.cpp is one line.
void wwHkxAnimHarness( class NifSkope * skope );

#endif
