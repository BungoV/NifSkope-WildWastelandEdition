/* Reader + decoder for Fallout 4 Havok animation files (.hkx): the
   hkaSplineCompressedAnimation clip, its hkaAnimationBinding (track -> bone),
   the hkaSkeleton(s) a file may carry (skeleton.hkx), and the extracted root
   motion (hkaDefaultAnimatedReferenceFrame).

   The format contract, every offset traced to the 1.10.155 exe's hkClass
   reflection and to the engine's own decoder disassembly, is
   docs/HKX_ANIMATION_FORMAT.md. This reader implements exactly what that page
   states and refuses, by name, everything it does not (lane HKX1, 2026-09-10).

   Two routes behind one call:
     route A  an HKXPACK unpack of the file (.xml)          hkxAnimLoadXml
     route B  the Havok 2014 binary packfile itself (.hkx)  hkxAnimLoadPackfile
   Both produce the same HkxAnimFile; tests/spells/hkxanim_gates.py holds them
   against each other and against the independent Python decoder
   (tests/spells/hkxanim_decode.py).

   Deliberately Qt-Core-only (plus data/niftypes.h for Vector3 / Quat), so a
   standalone test binary links without the rest of NifSkope. */

#ifndef HKXANIM_H
#define HKXANIM_H

#include "data/niftypes.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

//! One bone-local transform. rotation is NifSkope's Quat (w, x, y, z); the
//! file stores Havok's (x, y, z, w) and the reader converts at the boundary.
struct HkxTransform
{
	Vector3 translation;
	Quat rotation;
	Vector3 scale = Vector3( 1.0f, 1.0f, 1.0f );
};

//! An hkaAnnotationTrack::Annotation: a text key on one track.
struct HkxAnnotation
{
	float time = 0.0f;
	QString text;
};

//! One hkaDefaultAnimatedReferenceFrame sample: the root's displacement and
//! its yaw (radians) about the frame's `up` axis, one per frame.
struct HkxRootMotion
{
	Vector3 translation;
	float yaw = 0.0f;
};

//! An hkaSkeleton: names, parents (-1 at a root) and the bind pose, bone-local.
struct HkxSkeleton
{
	QString name;
	QStringList boneNames;
	QVector<int> parents;
	QVector<HkxTransform> referencePose;
	QVector<bool> lockTranslation;
};

//! A decoded clip: every transform track at every frame, plus what the
//! playback lane needs to map tracks onto a skeleton.
struct HkxAnimClip
{
	QString name;                   //!< the file stem
	QString originalSkeletonName;   //!< from the binding ("Root" for the player)
	QString blendHint;              //!< NORMAL / ADDITIVE / ADDITIVE_DEPRECATED
	QString rotationQuantization;   //!< the packing(s) seen: THREECOMP40, THREECOMP48
	int numFrames = 0;
	int numTracks = 0;              //!< numberOfTransformTracks
	int numFloatTracks = 0;         //!< carried, NOT decoded (4 FO4 clips have one)
	int numBlocks = 0;
	int maxFramesPerBlock = 0;
	float duration = 0.0f;          //!< seconds; == (numFrames - 1) * frameDuration
	float frameDuration = 0.0f;
	//! track i drives bone trackToBone[i] of the skeleton named above
	//! (hkaAnimationBinding::transformTrackToBoneIndices)
	QVector<int> trackToBone;
	//! true when the file gave no mapping and the identity map was used:
	//! either there is no hkaAnimationBinding at all, or its
	//! transformTrackToBoneIndices is EMPTY (third-party clips). Names the
	//! serving arm, so a consumer never reports a derived map as a stored one.
	bool trackToBoneIsIdentity = false;
	//! per track, its annotations (text keys), in file order
	QVector<QVector<HkxAnnotation>> annotations;
	//! frames[frame][track]
	QVector<QVector<HkxTransform>> frames;
	//! one per frame, or empty when the clip carries no extracted motion
	QVector<HkxRootMotion> rootMotion;
	Vector3 rootMotionUp = Vector3( 0.0f, 0.0f, 1.0f );
	Vector3 rootMotionForward = Vector3( 0.0f, 1.0f, 0.0f );

	//! DIAGNOSTICS, written by the decoder and tested by the gates.
	//! Worst |1 - |q|| over every spline-evaluated quaternion before
	//! normalisation (a spline between unit quaternions is not unit).
	float worstQuatLengthDeviation = 0.0f;
	//! Frames that lie on a block boundary and were decoded from BOTH blocks;
	//! the worst component difference between the two decodes (gate g).
	int blockOverlapFrames = 0;
	float blockOverlapWorst = 0.0f;
	//! per block, the byte where the transform-track walk ended and the byte
	//! where the block's float data begins -- equal on a well-formed clip
	QVector<int> walkEnd, blockEnd;
};

struct HkxAnimFile
{
	QString error;                  //!< empty on success; a sentence naming the field and value otherwise
	QString route;                  //!< "xml" or "packfile"
	QVector<HkxSkeleton> skeletons;
	QVector<HkxAnimClip> clips;
	bool ok() const { return error.isEmpty(); }
};

//! Load by extension: .xml -> route A, anything else -> route B.
HkxAnimFile hkxAnimLoad( const QString & path );
//! Route A: an HKXPACK unpack (`<hkpackfile>` with `hkaAnimationContainer`).
HkxAnimFile hkxAnimLoadXml( const QByteArray & xml, const QString & name );
//! Route B: the binary packfile.
HkxAnimFile hkxAnimLoadPackfile( const QByteArray & blob, const QString & name );

#endif
