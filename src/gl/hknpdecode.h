/* Decoder for FO4 compiled collision: the "Binary Data" of a
   bhkPhysicsSystem / bhkRagdollSystem is a Havok 2014 binary packfile
   holding hknp (new physics) objects. Format notes and the Python
   reference implementation live in tools/hkparse.py and
   TO_BE_IMPLEMENTED.md ("Display compiled FO4 collision"). */

#ifndef HKNPDECODE_H
#define HKNPDECODE_H

#include "data/niftypes.h"

#include <QByteArray>
#include <QString>
#include <QVector>

//! One decoded hknp collision shape, in Havok units
//! (multiply by 69.99125 for game units)
struct HknpShape
{
	QString className;
	//! shape-local vertices (primitives get synthesized wireframe geometry)
	QVector<Vector3> verts;
	//! triangulated geometry (convex faces are fan-triangulated)
	QVector<Triangle> tris;
	//! convex only: the original face loops (indices into verts)
	QVector<QVector<int>> faces;
	//! convex only: face planes (nx, ny, nz, d)
	QVector<Vector4> planes;
	float convexRadius = 0.0f;
	bool isConvex = false;
	//! Havok material CRC (u32 at shape+0x18; same value as the legacy
	//! HavokMaterial enum). 0 = unknown.
	quint32 materialCRC = 0;

	//! primitive interpretation: 0 generic, 1 sphere, 2 capsule
	int primType = 0;
	Vector3 capA, capB;            //!< capsule end points / sphere center (capA)
	float primRadius = 0.0f;       //!< sphere / capsule radius (capsule: approx)

	//! index of the hknpBodyCinfo this shape belongs to (-1 = none). Each
	//! bhkNPCollisionObject names its body via its "Body ID" field, and the
	//! body is PLACED BY THAT NODE's world transform (the cinfo position /
	//! orientation is Elric's rest pose, not the scene placement - validated
	//! on vanilla stair helpers where the two differ).
	int bodyId = -1;

	//! instance transform from hknpDynamicCompoundShape. Verbatim NIF
	//! Matrix4 rows: v' = (v * scaleVec) . R + trans (row vectors)
	bool hasTransform = false;
	Vector3 rotRows[3] = { Vector3( 1, 0, 0 ), Vector3( 0, 1, 0 ), Vector3( 0, 0, 1 ) };
	Vector3 trans;
	Vector3 scaleVec = Vector3( 1, 1, 1 );

	//! a vertex with the instance transform applied (still Havok units)
	Vector3 transformed( const Vector3 & v ) const
	{
		if ( !hasTransform )
			return v;
		Vector3 s( v[0] * scaleVec[0], v[1] * scaleVec[1], v[2] * scaleVec[2] );
		return rotRows[0] * s[0] + rotRows[1] * s[1] + rotRows[2] * s[2] + trans;
	}
};

//! Per-body physics decoded from the packfile (indexed by body id).
//! Layouts validated against controlled Elric pairs + CK FileConvert XML
//! (PropCollision mass 10 -> inverseMass 0.1, layer 10, motionType 2);
//! body_props stride 0x50 established by signature scan on 6-body files
//! (PyNifly documents 0x110, which only ever fit single-body files).
struct HknpBodyPhys
{
	quint32 layer = 1;          //!< cinfo +0x14 packed Havok Filter (old local writer used +0x1C)
	quint8 filterFlags = 0;     //!< packed Havok Filter byte 1
	quint16 filterGroup = 0;    //!< packed Havok Filter upper 16 bits
	quint32 materialCRC = 0;    //!< body material ID resolved through hknpBSMaterialProperties
	bool hasMotion = false;     //!< cinfo +0x0C != 0x7fffffff (dynamic body)
	float friction = 0.5f;      //!< body_props +0x12 (truncated float16)
	float restitution = 0.4f;   //!< body_props +0x16 (truncated float16)
	/*! cinfo +0x30: the body's POSITION, not its centre of mass.
	 *
	 * Measured on the brahmin ragdoll: all 39 entries equal the bone origin
	 * obtained by accumulating hkaSkeleton's referencePose down the hierarchy, to
	 * every decimal place printed. Reading it as a centre-of-mass offset puts a
	 * limb a metre away from its own bone. It doubles as an independent check on
	 * the skeleton decode, since the two come from unrelated parts of the file
	 * and agree exactly.
	 */
	Vector3 position;
	/*! cinfo +0x40: the body's orientation, xyzw as Havok stores it.
	 *
	 * Sits directly after the position, which is the usual hknpBodyCinfo layout.
	 * Whether it really is a quaternion is checked rather than assumed -- see the
	 * unit-norm test in the simulate trace.
	 */
	Quat orientation = Quat( 1, 0, 0, 0 );

	/* Per-body dynamics. A ragdoll has one entry per bone in the root's
	 * dyn_motion (+0x20, stride 0x40) and dyn_inertia (+0x30, stride 0x70)
	 * arrays; a single-body physics system has one of each, which is why these
	 * lived on HknpSystem as scalars until ragdolls decoded. Always populated -
	 * seeded from the system values when the arrays are absent - so callers can
	 * read them without checking.
	 */
	float mass = 0.0f;                  //!< 1 / (dyn_inertia +0x04)
	float density = 0.0f;               //!< dyn_inertia +0x08
	/*! dyn_inertia +0x20/24/28: the diagonal of the INVERSE inertia tensor.
	 *
	 * Not the inertia, despite the slot's name in every earlier revision of this
	 * header. +0x04 beside it is plainly inverse mass (0.2, 0.05, 1.0 for 5, 20 and
	 * 1 kg), and the same reading makes the tensor physical: the brahmin pelvis at
	 * 5 kg reads 4.16 here, giving I = 0.24 and a radius near 0.22 m. Taken as
	 * inertia the same number implies a 0.9 m pelvis. The simulator confirmed it
	 * from the other end -- reciprocating it made every ragdoll explode.
	 */
	Vector3 invInertia;
	float gravityFactor = 1.0f;         //!< dyn_motion +0x08
	float maxLinVelocity = 104.375f;    //!< dyn_motion +0x10
	float maxAngVelocity = 31.57f;      //!< dyn_motion +0x14
	float linDamping = 0.1f;            //!< dyn_motion +0x18
	float angDamping = 0.05f;           //!< dyn_motion +0x1C
};

/*! One ragdoll joint: which two bodies it binds, and what kind it is.
 *
 * From hknpRagdollData's array at +0x50 - 0x18 bytes an entry, one per non-root
 * body: constraint pointer at +0x00 (a global fixup), child body at +0x08, parent
 * body at +0x0c, 8 bytes of padding.
 *
 * Verified on the brahmin ragdoll against the skeleton itself: all 38 bindings
 * name a bone and its nearest ancestor that HAS a body (bodiless intermediates
 * like LNeckHub are skipped, which a ragdoll must do). The kinds line up
 * anatomically too - the 8 hkpLimitedHinge entries are exactly the two knees,
 * two elbows, two wrists and two toes, with ball-and-socket everywhere else.
 *
 * The constraint object itself is a flat run of "atoms" starting at +0x20. Each
 * atom opens with a u16 type and carries NO size field, so walking the chain
 * needs a type-to-size table; see decodeConstraintAtoms in the .cpp for how that
 * table was established and checked.
 */

/*! One angular limit atom, in radians.
 *
 * Havok writes -100 as the lower bound of a cone limit to mean "no lower bound",
 * which is why min is not clamped to [-pi, pi] here.
 *
 * Two hinges in the vanilla assets (both in the human skeleton) store min and max
 * the wrong way round - exactly +5.00/-5.00 and +0.10/-0.10 degrees. That is what
 * the file says, so it is what this reports; it is not corrected silently.
 */
struct HknpAngLimit
{
	bool present = false;
	float min = 0.0f;
	float max = 0.0f;
	float tau = 0.0f;   //!< angularLimitsTauFactor: 0.8 on ragdolls, 1.0 on hinges
};

struct HknpConstraint
{
	int childBody = -1;
	int parentBody = -1;
	//! Havok class name, e.g. hkpRagdollConstraintData / hkpLimitedHingeConstraintData
	QString kind;
	//! reached through an hknpBreakableConstraintData wrapper: the joint can snap
	bool breakable = false;

	/*! The joint's frame in each body's space: a rotation basis and a pivot.
	 *
	 * Havok's transformA / transformB, the two hkTransforms of the constraint's
	 * SET_LOCAL_TRANSFORMS atom, four hkVector4 each - three basis rows then the
	 * pivot. A belongs to the CHILD body and B to the PARENT, matching the binding
	 * entry's own order (child at +0x08, parent at +0x0c).
	 *
	 * Which is which was settled by measurement, not by the names: a joint sits at
	 * the child bone's origin, so in the child's own space its pivot is (0,0,0) and
	 * in the parent's space it is the child's local translation - which hkaSkeleton's
	 * referencePose supplies independently. Across 755 joints, pivotA is (0,0,0) for
	 * 98.8% and pivotB equals the child's reference-pose position for 93.9%.
	 *
	 * That the offsets are right at all was checked separately: 1514 of 1514 hkp*
	 * frames are orthonormal with determinant +1, which bytes at a wrong offset are
	 * not. hknpBreakableConstraintData is a wrapper with its own layout; the decoder
	 * follows it to the real constraint rather than misreading it.
	 */
	bool hasFrames = false;
	Vector3 rotA[3], rotB[3];   //!< A = child side, B = parent side
	Vector3 pivotA, pivotB;

	/*! How far the joint may move. Which of these are filled depends on kind:
	 * a ragdoll constraint has twist + cone + plane, a limited hinge has hinge.
	 */
	HknpAngLimit twist;   //!< spin about the bone's own axis
	HknpAngLimit cone;    //!< how far the bone may swing away from its parent
	HknpAngLimit plane;   //!< swing limit in the perpendicular plane
	HknpAngLimit hinge;   //!< the single range of a limited hinge (knee, elbow)
	float friction = 0.0f;    //!< maxFrictionTorque, resisting rotation
	bool motorEnabled = false;   //!< a powered joint (keyframed / animation-driven)
};

/*! One bone of the ragdoll's own skeleton copy (hkaSkeleton).
 *
 * hkaSkeleton sits at the packfile root alongside hknpRagdollData, one per
 * ragdoll, and holds hkArrays at +0x18 (parentIndices, hkInt16), +0x28 (bones,
 * 16 bytes each) and +0x38 (referencePose, hkQsTransform at 48 bytes). The four
 * arrays after those are empty in every vanilla ragdoll.
 *
 * Bone index equals body index: all 757 constraint bindings across the 35 vanilla
 * ragdolls name the same parent as parentIndices does, and every ragdoll has
 * exactly one more bone than it has joints, rooted at index 0.
 *
 * NAMES ARE NOT HERE. Bethesda strips them - every hkaBone's name pointer is null
 * and carries no fixup - so the names must come from the NIF's own node list.
 */
struct HknpBone
{
	int parent = -1;                //!< index into HknpSystem::bones; -1 at the root
	bool lockTranslation = false;
	//! reference (rest) pose, local to the parent. Scale is (1,1,1) on all 792
	//! vanilla bones and the rotation is a unit quaternion on all 792, which is
	//! what confirmed the 48-byte hkQsTransform stride.
	Vector3 translation;
	Quat rotation;
};

//! A decoded bhkPhysicsSystem blob
struct HknpSystem
{
	QVector<HknpShape> shapes;
	//! per-body physics, indexed by body id (may be shorter than expected on
	//! malformed data - callers use value( id ) semantics)
	QVector<HknpBodyPhys> bodyPhys;
	//! dyn_motion / dyn_inertia arrays present (PSD +0x20 / +0x30): the
	//! system simulates dynamically (props); statics lack both arrays
	bool dynamic = false;
	//! The following are the FIRST body's values. Kept because callers outside the
	//! collision paths read them; anything per-body should use HknpBodyPhys.
	float mass = 0.0f;          //!< 1 / (dyn_inertia +0x04); 0 for statics
	float density = 0.0f;       //!< dyn_inertia +0x08 (mass / collision volume)
	//! dyn_inertia +0x20/24/28: INVERSE inertia diagonal, see HknpBodyPhys::invInertia
	Vector3 invInertia;
	//! dyn_motion values (engine defaults when the array is absent)
	float gravityFactor = 1.0f;         //!< dyn_motion +0x08
	float maxLinVelocity = 104.375f;    //!< dyn_motion +0x10
	float maxAngVelocity = 31.57f;      //!< dyn_motion +0x14
	float linDamping = 0.1f;            //!< dyn_motion +0x18
	float angDamping = 0.05f;           //!< dyn_motion +0x1C
	/*! Shape body ids were assigned POSITIONALLY (shape index = body id)
	 * because the packfile carried no decodable body array.
	 *
	 * True for bhkRagdollSystem, whose root is hknpRagdollData rather than
	 * hknpPhysicsSystemData. The mapping is sound but it is an inference, so a
	 * caller that knows how many collision objects reference the system should
	 * check that count against shapes.size() before relying on it.
	 */
	//! Ragdoll joints, empty for a plain physics system. See HknpConstraint.
	QVector<HknpConstraint> constraints;
	//! The ragdoll's own skeleton copy, indexed like the bodies. See HknpBone.
	QVector<HknpBone> bones;
	bool positionalBodies = false;
	//! class names present but not decoded
	QStringList unknownShapes;
	bool valid = false;
	QString error;
};

//! Decode the Binary Data of a bhkPhysicsSystem / bhkRagdollSystem block
HknpSystem hknpDecode( const QByteArray & data );

//! The decode result for this data, from a small internal cache
const HknpSystem & hknpDecodeCached( const QByteArray & data );

#endif // HKNPDECODE_H
