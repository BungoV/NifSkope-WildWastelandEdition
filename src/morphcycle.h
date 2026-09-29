/* GENERATED PREVIEW CLIPS for the Rigging Manager: the chargen BODY SHAPE cycle
   and the FACEBONES cycle, for a male or a female, built in memory and put in
   the SAME animations list as a loaded .hkx (HkxPlayback / WwHkxAnimHub), so
   the ordinary transport -- play, pause, loop, scrub, speed -- plays them.

   bungo, 2026-09-29, verbatim: "can you build this preview for both male and
   female, into Nifskope? add a button in the rigging manager to load these
   animations and preview them, for both males and females, the animations get
   loaded as the loaded ones, so you can play them as regular animations" --
   "So body shape, facebones cycle."

   NEITHER IS A GAME CLIP. skeleton.hkx has no *_skin bone and no skin_bone_*
   bone; those live only in skeleton.nif / skeleton_faceBones.nif. So no
   Fallout 4 .hkx can carry either clip, and every export path refuses them
   with HkxPlayback::generatedRefusal() (HkxClipEntry::generated is set).

   ---- BODY SHAPE CYCLE ------------------------------------------------------
   THIN -> MUSCULAR -> FAT -> THIN, 0 / 2 / 4 / 6 s, linear barycentric weights,
   181 frames at 30 fps -- bodyBuildCycleKeys() and the k formula of
   src/bodybuild.h, on the HumanRace RACE record's bone scale table read out of
   Fallout4.esm. Exact at the three corners (the RACE Y/Z equal Bethesda's
   HumanRaceBoneScales*.txt to double epsilon, lane GLTFEXPORT1); UNPROVEN
   between corners (the shape of k -- refuter in bodybuild.h). Each *_skin bone
   gets saved-local * diag(scale): the Body Build panel's own fold.

   ---- FACEBONES CYCLE -------------------------------------------------------
   Every live channel of every chargen face region goes 0 -> +1 -> -1 -> 0,
   one channel at a time, 24 frames per channel at 24 fps (0 at +0, +1 at +6,
   -1 at +18, 0 at +24; the last frame of one channel is the first of the
   next), 1 + 24*channels frames. Each channel's start is an annotation on
   track 0, named "<region>: <channel>", which the workspace shows as a marker.

   Data: CharacterAssets/HumanRaceFacialBoneRegions{Male,Female}.txt (JSON:
   regions, each BonesA[{Bone, Maxima, Minima{Position, Rotation (degrees),
   Scale}}]) and skeleton_faceBones.nif / skeleton_female_faceBones.nif, both
   read through the Game Manager. Region "bone_X" is node "skin_bone_X".

   Channels: pos x, y, z, rot x, y, z, and ONE scale slider that scales all
   three Scale components together. A channel that is zero on every bone of
   its region is skipped. Measured 2026-09-29: male 32 regions / 103 live
   channels / 60 region bones; female 32 / 104 / 60 (Eyelids - Top and
   Eyelids - Bottom carry none).

   The value (Fallout4.exe 1.10.155, BGSCharacterMorph::TransformMinMax::
   CalcWeightedTransform @ RVA 0x2a32f0): out = v > 0 ? v * Max : |v| * Min,
   Min stored signed. The pose (TESNPC::MorphFaceBone @ RVA 0x5c09a0):

       posed local = restLocal * T(d) * R * S,   S = diag(1 + s)

   d in the bone's OWN local frame (measured 2026-09-29 in Blender: L/R mirror
   error 6% against 63-74% for the parent-frame and transposed readings).
   R = rotation by -angle about the local axis, Rz(-z) * Rx(-x) * Ry(-y) --
   the engine's FromEulerAnglesZXY with the angles negated. UNPROVEN: the SIGN
   (and, for a mix of axes, the order; the cycle moves one axis at a time).
   Refuter: one in-game slider against this preview.

   The globals are composed down the SKELETON's chain (skeleton_faceBones.nif
   hangs the face bones under C_MasterBot / MasterMouth / MasterEyebrow /
   MasterNose / Ear / Eye; the head NIFs are flat under HEAD), so the tracks
   are every region bone AND every skeleton descendant of one, and the
   playback writes each NIF node the local that reproduces that global
   (HkxClipEntry::GenSkeletonSpace).

   Lane MORPHCYC1, 2026-09-29. */

#ifndef MORPHCYCLE_H
#define MORPHCYCLE_H

#include "hkxplayback.h"

#include <QString>
#include <QStringList>

//! What a generator measured, for the status line and the harness.
struct MorphCycleStats
{
	int regions = 0;			//!< face: regions in the file
	int liveRegions = 0;		//!< face: regions with at least one live channel
	int channels = 0;			//!< face: live channels; body: bones with a non-neutral scale
	int regionBones = 0;		//!< face: distinct region bones named by the file
	int regionBonesFound = 0;	//!< face: of those, found in the skeleton
	int tracks = 0;				//!< tracks in the clip
	QStringList missing;		//!< face: region bones the skeleton does not have
	QString source;				//!< where the data came from, in words
};

struct MorphCycleOptions
{
	/*! HARNESS ONLY: the deliberately WRONG reading, the whole morph in the
	 *  PARENT's frame: T(d) * R * S * restLocal instead of restLocal * T(d) * R * S
	 *  (rotation and scale pivot on the parent's origin). The facebones harness
	 *  plays it to prove its 1e-3 gate can fail. (Offsets alone in the parent
	 *  frame cannot be caught by a displacement LENGTH -- measured 2026-09-29:
	 *  0.40000 / 0.59336 / 0.67813, the same as the right reading.) */
	bool parentFrame = false;
	/*! HARNESS ONLY: the opposite rotation sign, to measure whether the five
	 *  cross-checks can see the sign at all (the sign is UNPROVEN). */
	bool flipRotationSign = false;
};

//! 0 male, 1 female
QString morphCycleGenderWord( int gender );

/*! The body shape cycle for one gender. On success `out` is a generated entry
 *  ready for WwHkxAnimHub::addGenerated and `sentence` says what it is; on
 *  false `sentence` is the refusal (no Fallout4.esm, no bone scale table). */
bool morphCycleBodyShape( int gender, HkxClipEntry & out, QString & sentence,
						  MorphCycleStats * stats = nullptr );

/*! The facebones cycle for one gender. On false `sentence` is the refusal (the
 *  regions file or the skeleton could not be read from the game's archives). */
bool morphCycleFaceBones( int gender, HkxClipEntry & out, QString & sentence,
						  MorphCycleStats * stats = nullptr,
						  const MorphCycleOptions & opts = MorphCycleOptions() );

//! The time (s) a named facebones channel ("Chin: pos Y") reaches value v
//! (+1 or -1, or 0 for its start); -1 when the clip has no such channel.
float morphCycleChannelTime( const HkxClipEntry & e, const QString & channel, int v );

/*! WW_MORPHCYC_TEST: lane MORPHCYC1's gates, run inside the real application.
 *  Defined in src/morphcyctest.cpp; one line in nifskope_ui.cpp calls it. */
void wwMorphCycleHarness( class NifSkope * skope );

#endif // MORPHCYCLE_H
