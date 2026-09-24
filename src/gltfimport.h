/* glTF 2.0 ANIMATION importer: a .gltf (JSON + .bin) or .glb carrying one
   animation, turned into lane HKX1's clip type (HkxAnimClip, src/hkxanim.h)
   so that the writer (src/hkxwrite.h) can put it back into a FO4 .hkx.

   Lane HKX5, 2026-09-10.  The mapping contract -- axes, units, bone names,
   resampling, root motion and what is lost -- is docs/GLTF_IMPORT.md; this
   file implements exactly that page and refuses, by a sentence naming the
   field and its value, everything it does not.

   THE INVERSE OF THE EXPORT.  src/gltfexport.cpp (lane HKX4) writes NIF-space
   TRS on every node and hangs the whole hierarchy under ONE synthetic root
   named "NifSkope_Y_up" whose rotation is -90 degrees about X (Z-up -> Y-up),
   with every translation multiplied by 0.9144/64 metres per unit.  So the
   import is: divide translations by that constant, and CONSUME the up-axis
   node -- which is arithmetically identical to composing +90 degrees about X
   into each scene root's own local transform, and the importer does exactly
   that for a glTF that has no such node (Blender's own export).  Both arms
   are named in the report; gate (c) holds them against each other.

   Deliberately Qt-Core-only (QJsonDocument plus data/niftypes.h), like
   src/hkxanim.h and src/gltfexport.h, so the standalone gate binary links it
   without the rest of NifSkope. */

#ifndef GLTFIMPORT_H
#define GLTFIMPORT_H

#include "hkxanim.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

//! How the importer turns a glTF animation into a clip.  Every field has the
//! value that inverts src/gltfexport.cpp exactly; changing one is the way back
//! (CONSTITUTION rule 7).
struct GltfImportOptions
{
	//! Bone names of the skeleton the clip is being authored FOR, in bone
	//! order (hkaSkeleton::bones).  Empty = the glTF's own node names become
	//! the bone names and the mapping is the identity, which is what a
	//! round trip through our own exporter wants.
	QStringList skeletonBoneNames;

	//! Frames per second of the produced clip.  FO4's third-person clips are
	//! 30; the Mixamo fixture is 60.
	float targetFps = 30.0f;

	//! When the glTF's key times are already a uniform grid, keep THAT rate
	//! instead of `targetFps` (nothing is resampled and the import is exact).
	bool preserveSourceRate = false;

	//! THE ROOT-MOTION FLAG.  false: the root node's travel stays on its own
	//! track and the clip has no extracted motion (it plays where it is
	//! authored).  true: the travel is lifted off the root track into
	//! HkxAnimClip::rootMotion and the root track is flattened to the value it
	//! holds at `rootMotionReference` (frame 0 by default), which is what
	//! hkaDefaultAnimatedReferenceFrame means in FO4.
	bool extractRootMotion = false;
	//! Which node the motion is lifted from.  Empty = the single scene root
	//! that has children once the up-axis node is consumed; ambiguity refuses.
	QString rootNodeName;
	//! Frame whose root transform is treated as "in place" (default 0).
	int rootMotionReferenceFrame = 0;

	//! Goes into hkaAnimationBinding::originalSkeletonName.
	QString originalSkeletonName = QStringLiteral( "Root" );
	//! Which glTF animation to import when the file carries several.
	int animationIndex = 0;

	//! Metres per NIF unit; the exporter's 0.9144/64 = 0.0142875 exactly.
	float unitScale = 0.9144f / 64.0f;
	/*! Take the unit scale from the FILE when it states one, rather than from
	 *  the field above. Lane GLTFEXPORT1, 2026-09-19, finding (9).
	 *
	 *  src/gltfexport.cpp has always written `asset.extras.metresPerUnit` (its
	 *  line 537) and this importer has never read it, so a file exported in
	 *  game units (metresPerUnit = 1) came back 69.99x too large -- silently,
	 *  because nothing in the file or the log mentioned units at all. With this
	 *  on, a round trip needs no flag: the file says what it is.
	 *
	 *  A file with no `extras.metresPerUnit` -- anything Blender or Maya wrote
	 *  -- falls back to `unitScale` above and the report says which arm served,
	 *  so the two cases are never confused. Set false to force `unitScale`
	 *  regardless of the file; that is the way back (CONSTITUTION 7), and at
	 *  its off value this reader behaves exactly as it did before this field
	 *  existed. */
	bool unitScaleFromFile = true;
	//! Name of the exporter's synthetic up-axis root.  A scene root of this
	//! name, OR any childed, mesh-less, translation-free scene root whose
	//! rotation is -90 degrees about X to 1e-5, is consumed as the axis
	//! conversion instead of becoming a bone.
	QString upAxisNodeName = QStringLiteral( "NifSkope_Y_up" );
	//! false = leave the glTF in its own Y-up frame (no conversion at all);
	//! the way back from the axis rule.
	bool convertUpAxis = true;

	//! A matched bone whose node carries no animation channel still gets a
	//! track, constant at the node's bind TRS.  FO4 clips carry every bone.
	bool includeStaticTracks = true;

	//! hkaDefaultAnimatedReferenceFrame::up / ::forward, in NIF space.
	Vector3 rootMotionUp = Vector3( 0.0f, 0.0f, 1.0f );
	Vector3 rootMotionForward = Vector3( 0.0f, 1.0f, 0.0f );
};

//! What the import did, in words and numbers -- every arm names itself
//! (CONSTITUTION rule 10) and nothing is dropped silently.
struct GltfImportReport
{
	QString error;                  //!< empty on success

	QString container;              //!< "gltf+bin", "gltf (embedded base64)", "glb"
	QString upAxisArm;              //!< which axis arm served, in words
	QString rateArm;                //!< resampled / source rate kept, with the numbers
	QString mappingArm;             //!< how bone names were found, in words
	QString unitArm;                //!< where the unit scale came from, in words
	float unitScaleUsed = 0.0f;     //!< metres per unit actually applied

	//! "gltfNodeName -> boneName" for every track written
	QStringList matched;
	//! of `matched`, those that needed case folding, and those that were partial
	QStringList caseFolded, partialMatched;
	//! animated glTF nodes that reached no bone -- named, never dropped silently
	QStringList unmatchedNodes;
	//! target bones that no glTF node drives
	QStringList unmatchedBones;
	//! nodes whose name matched more than one bone (refused into unmatchedNodes)
	QStringList ambiguous;

	int gltfNodes = 0;              //!< nodes in the file
	int animatedNodes = 0;          //!< nodes with at least one channel
	int channelsRead = 0;           //!< glTF animation channels consumed
	int staticTracks = 0;           //!< tracks filled from a bind TRS

	QStringList interpolations;     //!< the sampler interpolations seen, sorted
	float sourceFirstKey = 0.0f, sourceLastKey = 0.0f;
	int sourceKeys = 0;             //!< keys of the longest sampler
	bool sourceUniform = false;     //!< every sampler on one uniform grid
	float sourceFps = 0.0f;         //!< 1 / the uniform step, 0 when not uniform
	//! |clip duration - (last key - first key)|, the cost of landing the last
	//! frame on the target grid
	float durationDelta = 0.0f;

	bool rootMotionExtracted = false;
	QString rootMotionNode;
	float rootMotionMaxTranslation = 0.0f;  //!< max |sample| over every frame
	float rootMotionMaxYawDeg = 0.0f;

	//! One sentence for a log line or the report.
	QString summary() const;
};

/*! Read a .gltf or .glb from disk and produce one clip.
 *
 *  Returns false with a sentence in `report.error` and writes nothing to
 *  `clip` when it refuses.  A .gltf's buffers are resolved relative to the
 *  file; `data:` URIs and a .glb's BIN chunk are read in place.
 */
bool gltfImportRead( const QString & path, const GltfImportOptions & opt,
					 HkxAnimClip & clip, GltfImportReport & report );

/*! The same, from bytes already in hand.
 *
 *  `blob` is either the JSON of a .gltf or the whole of a .glb (the magic
 *  decides).  `externalBuffers` answers a buffer URI that is neither a data:
 *  URI nor the GLB chunk; an empty answer for a URI the file needs refuses by
 *  name.  `name` becomes HkxAnimClip::name when the animation has none.
 */
bool gltfImportParse( const QByteArray & blob, const QString & name,
					  const GltfImportOptions & opt, HkxAnimClip & clip,
					  GltfImportReport & report,
					  const QHash<QString, QByteArray> & externalBuffers = QHash<QString, QByteArray>() );

#endif // GLTFIMPORT_H
