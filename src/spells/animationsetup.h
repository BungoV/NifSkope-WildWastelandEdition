/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef SPELLS_ANIMATIONSETUP_H
#define SPELLS_ANIMATIONSETUP_H

#include "data/niftypes.h"

#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QVector>

class NifModel;

//! @file animationsetup.h Parameterised core of the animation rigging spells.
/*!
 * The "Setup Controllers" spell used to hold its whole implementation inside
 * cast(), behind a modal dialog. Everything it does is pure block-graph work —
 * create controller + interpolator + data, attach to the node, register a
 * ControlledBlock in a NiControllerSequence, add a NiDefaultAVObjectPalette
 * entry, create the manager/palette if missing — so it needs no GUI at all.
 *
 * The dialog is now one caller of setupControllers(); the headless CLI
 * (`NifSkope -no-gui anim-setup`) is another.
 */

namespace AnimSetup
{

enum CtlrKind
{
	KindFloat = 0, KindColor, KindBool, KindTransform
};

//! One controller type offered for a given block.
struct CtlrOption
{
	QString type;                //!< Controller block type
	QString label;               //!< Human-readable description
	CtlrKind kind = KindFloat;
	bool hasEffectVar = false;   //!< Takes an FO4 EffectShaderControlledVariable
	bool hasIntVar = false;      //!< Takes a numeric LightingShaderControlledVariable
};

//! What can be attached to this block, in a stable order.
/*! The returned order defines the indices used by Params::chosen. */
QVector<CtlrOption> controllerOptions( const NifModel * nif, const QModelIndex & iBlock );

//! Everything the dialog used to collect.
struct Params
{
	//! Indices into controllerOptions() for this block.
	QVector<int> chosen;
	//! false = standalone controller, always playing, no sequence.
	bool useSequence = true;
	//! Create sequenceName rather than looking it up.
	bool newSequence = false;
	//! Sequence to create or to add to. Empty with useSequence picks the first.
	QString sequenceName;
	//! FO4 EffectShaderControlledVariable (0-9); 6 = U Offset.
	int effectVar = 6;
	//! LightingShaderControlledVariable enum value.
	int intVar = 0;
};

//! Attach the chosen controllers to a block and wire them into the animation graph.
/*!
 * Runs as one undoable snapshot operation. Safe to call with no GUI.
 *
 * \param nif    The model to edit.
 * \param iBlock Target block: an NiAVObject, or a property owned by one.
 * \param p      What to create.
 * \param error  Set to a human-readable reason when the call returns false.
 * \return true when something was created.
 */
bool setupControllers( NifModel * nif, const QModelIndex & iBlock,
                       const Params & p, QString * error = nullptr );

//! Names of every NiControllerSequence in the file, in block order.
QStringList sequenceNames( const NifModel * nif );

// ---- poses ---------------------------------------------------------------
/*!
 * A pose is a `NiControllerSequence` carrying ONE key per bone at t=0 — the NIF
 * equivalent of a Blender Action holding a single frame. That choice means the
 * pose library reuses the sequence machinery wholesale: poses live in the file,
 * show up in the Timeline, and export like any other animation.
 */

//! Block numbers of every node some skinned shape is bound to.
QVector<int> poseBoneNodes( const NifModel * nif );

//! Capture the current bone transforms as a new one-key sequence.
bool savePose( NifModel * nif, const QString & name, QString * error = nullptr );

//! Write a pose sequence's t=0 transforms back onto the bone nodes.
/*!
 * \param blend 1.0 replaces the current transform with the pose; a smaller
 *              value interpolates from where each bone is now toward the pose
 *              (Blender's pose-strength slider). Values outside [0,1] extrapolate.
 */
bool applyPose( NifModel * nif, const QString & name, float blend = 1.0f,
                QString * error = nullptr );

//! A bone's transform channels as stored in a pose (or read from a node).
struct BoneTransform
{
	Vector3 translation;
	Quat    rotation;      //!< identity by default
	float   scale = 1.0f;
};

//! Read a pose sequence's per-bone transforms, keyed by node name.
/*! Empty when no such sequence exists. */
QHash<QString, BoneTransform> readPose( const NifModel * nif, const QString & name );

// ---- Outfit Studio pose XML (BodySlide) ----------------------------------
/*!
 * Outfit Studio stores poses as `<PoseData><Pose name><Bone name rotX/Y/Z
 * transX/Y/Z/></Pose></PoseData>`. Rotations are Euler RADIANS and everything
 * is a DELTA FROM REST — only posed bones are listed; absent bones stay at
 * rest. Bone names are the FO4 skeleton NiNode names, so the delta applies to
 * a matching node directly.
 *
 * These functions need each bone's rest transform to turn a delta into an
 * absolute local transform (import) or back (export). Callers pass a rest map
 * keyed by bone NAME — in pose mode that is the transform captured on mode
 * entry; standalone it is the file's current (bind) pose.
 */

//! Apply an Outfit Studio pose XML onto the file's bones (delta from rest).
/*! \param blend as applyPose. \param applied/missing counts (optional). */
bool applyOutfitStudioPose( NifModel * nif, const QString & path,
                            const QHash<QString, Transform> & restByName,
                            float blend, int * applied, int * missing, QString * error );

//! Write the current pose as an Outfit Studio pose XML (delta from rest).
/*! Only bones that actually moved from rest are written, matching OS. Rest is
 *  keyed by BLOCK (not name) so bones that happen to share a name can't be
 *  diffed against the wrong node. */
bool writeOutfitStudioPose( NifModel * nif, const QString & path, const QString & poseName,
                            const QHash<int, Transform> & restByBlock, QString * error );

// ---- Screen Archer Menu pose JSON ---------------------------------------
/*!
 * SAM (Screen Archer Menu) stores a pose as
 * `{"name","skeleton","version":2,"transforms":{ boneName:{yaw,pitch,roll,x,y,z,scale} }}`
 * with every value a JSON STRING and the angles in DEGREES.
 *
 * Unlike an Outfit Studio pose these are ABSOLUTE local transforms that REPLACE
 * the NiNode's own — no rest map is needed and nothing is composed. Rotation is
 * `Rx(yaw)·Ry(pitch)·Rz(roll)` (yaw about X, pitch about Y, roll about Z), which
 * in NIF row order is exactly `Matrix::fromEuler( yaw, pitch, roll )` in radians.
 *
 * Bone keys are exact, case-sensitive NiNode names; a pose routinely names bones
 * a given file does not have (armour pieces saved while unequipped), so missing
 * names are counted and skipped, never fatal.
 */
//! Apply a SAM pose JSON onto the file's bones (absolute local transforms).
/*! \param blend 1.0 replaces each bone's transform with the pose; less
 *         interpolates from where the bone is NOW toward the pose.
 *  \param applied/missing counts (optional).
 *  \param error failure message; on success, any non-fatal note (bones not in
 *         this skeleton, malformed entries, scale-0 "hidden" bones). */
bool applySamPose( NifModel * nif, const QString & path, float blend,
                   int * applied, int * missing, QString * error );

} // namespace AnimSetup

#endif // SPELLS_ANIMATIONSETUP_H
